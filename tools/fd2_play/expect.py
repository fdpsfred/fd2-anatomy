#!/usr/bin/env python3
"""expect.py -- combat damage oracle for FD2 replay scenarios.

Predicts the exact physical-attack damage a scenario must produce, then checks
it against the captured result. The prediction is a faithful port of the
binary's player-attack damage path:

  fd2_execute_combat_hit_cinematic @ 0x29... : one RNG draw for the 3%
      double-strike bonus, then per strike ->
  fd2_calculate_combat_hit_outcome  @ 0x29F72 : the physical formula
      (terrain AP/DP modifiers, weapon-class branch, hit roll, crit roll
      that halves DP, jitter roll), and the cinematic applies oc.damage to
      the defender's hp_current.

driven by the SAME LFSR as the game (fd2_advance_rng_state @ 0x4E893:
seed = ROL16(seed + 0x9014, 3)) starting from the scenario's fixed seed.

Sources, per project rules:
  - The LFSR, the formula control flow, and the runtime_char field offsets are
    verified from Ghidra / src (not copied from KB prose).
  - All expected-value DATA (item weapon class, job crit rate, terrain
    modifier tables) is read LIVE from the original binary fd2_game_files/FD2.LE
    via a parsed LE virtual->file mapping -- nothing is hardcoded from the KB.
  - The runtime_char inputs and the start seed come from the scenario's
    pre-attack STnn.BIN; the actual damage from the post-attack STnn.BIN.

Scenario JSON carries an "oracle" block:
  "oracle": { "type": "physical_attack", "attacker_idx": N, "defender_idx": M,
              "before_cap": A, "after_cap": B }

Usage:
  python tools/fd2_play/expect.py --scenario combat_attack [--out <dir>]
Exit: 0 if predicted defender hp delta == actual, 1 otherwise.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCEN_DIR = ROOT / "tests" / "play" / "scenarios"
RUN_BASE = ROOT / "workspace" / "fd2_play" / "run"
GOLDEN_DIR = ROOT / "tests" / "play" / "golden"
LE_FILE = ROOT / "fd2_game_files" / "FD2.LE"

# --- FD2.LE table virtual addresses + runtime_char field offsets ---------
# These are FD2.LE layout facts taken from Ghidra / src/include (globals.h,
# types.h) and verified against the binary below. Only ADDRESSES live here;
# every VALUE is read live from FD2.LE / the captures.
ADDR_ITEM_TABLE = 0x602AC   # item_effect[215], 23 bytes/entry (globals.h)
ADDR_CRIT_TABLE = 0x5239B   # job_crit_rate_table[28], 1 byte/entry
ADDR_TILE_AP_MOD = 0x51A12  # tile_attr_mv_modifier_table[6], int32 (AP %)
ADDR_TILE_DP_MOD = 0x51A2A  # tile_attr_def_modifier_table[6], int32 (DP %)
ITEM_STRIDE = 23
# item_effect field offsets (types.h). battle.c reads these as weapon_entry[9]
# / weapon_entry[10] because fd2_get_item_effect_entry returns &entry.type
# (table_base + id*23 + 1), so entry[9] == struct offset +10 etc.
ITEM_SPECIAL_TYPE = 10      # 02=poison 03=double 04=crit (weapon_class)
ITEM_SPECIAL_CHANCE = 11

# runtime_char field offsets (types.h)
OFF_TEAM = 0x06
OFF_PORTRAIT = 0x07
OFF_CHAR_ID = 0x08
OFF_INV = 0x0A          # 8 slots x (flag, item_id)
OFF_ARCHETYPE = 0x1F
OFF_JOB = 0x20
OFF_HP = 0x40
OFF_HP_MAX = 0x42
OFF_AP = 0x48
OFF_DP = 0x4A
OFF_HIT = 0x4C          # dx_current = attacker hit stat
OFF_EVADE = 0x4E        # stat4_current = defender evade stat

ST_HDR_WORDS = 16
ST_SEED_WORD = 11       # header[11] = rng_seed
REC = 0x50


# --- LFSR (fd2_advance_rng_state) + C truncating division ----------------
def lfsr(seed):
    s = (seed + 0x9014) & 0xFFFF
    return ((s << 3) | (s >> 13)) & 0xFFFF


def c_div(a, b):
    """Integer division truncating toward zero (C semantics)."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


# --- minimal LE reader: parse FD2.LE header, map vaddr -> file offset -----
class LEImage:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        d = self.data
        base = 0
        if d[:2] == b"MZ":                       # DOS stub -> e_lfanew @ 0x3C
            base = struct.unpack_from("<I", d, 0x3C)[0]
        if d[base:base + 2] not in (b"LE", b"LX"):
            raise SystemExit("not an LE/LX image: %s" % path)
        self.pagesize = struct.unpack_from("<I", d, base + 0x28)[0]
        self.datapage = struct.unpack_from("<I", d, base + 0x80)[0]
        objtab = base + struct.unpack_from("<I", d, base + 0x40)[0]
        objcnt = struct.unpack_from("<I", d, base + 0x44)[0]
        self.objs = []
        for i in range(objcnt):
            o = objtab + i * 24
            size, vbase, flags, pagemap, mapsize, _ = \
                struct.unpack_from("<IIIIII", d, o)
            self.objs.append((vbase, size, pagemap))

    def file_off(self, vaddr):
        for vbase, size, pagemap in self.objs:
            if vbase <= vaddr < vbase + size:
                page_in_obj = (vaddr - vbase) // self.pagesize
                global_page = pagemap + page_in_obj          # 1-based
                return (self.datapage + (global_page - 1) * self.pagesize
                        + (vaddr - vbase) % self.pagesize)
        raise SystemExit("vaddr 0x%X not in any LE object" % vaddr)

    def read(self, vaddr, length):
        off = self.file_off(vaddr)
        return self.data[off:off + length]

    def u8(self, vaddr):
        return self.read(vaddr, 1)[0]

    def i32(self, vaddr):
        return struct.unpack("<i", self.read(vaddr, 4))[0]


# --- ST capture parsing --------------------------------------------------
def load_st(path):
    buf = Path(path).read_bytes()
    hdr = list(struct.unpack_from("<16i", buf, 0))
    n = hdr[3]
    chars = []
    base = ST_HDR_WORDS * 4
    for i in range(n):
        off = base + i * REC
        chars.append(buf[off:off + REC])
    return hdr, chars


def u16(rec, o):
    return struct.unpack_from("<H", rec, o)[0]


def equipped_weapon_id(rec):
    """First equipped (flag bit 0x40) physical (id < 0x80) inventory slot id,
    matching fd2_find_equipped_item_by_kind(idx, kind=0). -1 if none."""
    for slot in range(8):
        flag = rec[OFF_INV + slot * 2]
        item_id = rec[OFF_INV + slot * 2 + 1]
        if (flag & 0x40) and item_id < 0x80:
            return item_id
    return -1


# --- combat formula port -------------------------------------------------
def terrain_mod_is_zero(stat, mod_table):
    """True if every terrain modifier in mod_table rounds to 0 for this stat
    (i.e. terrain cannot change the effective stat). The game computes
    eff = stat + c_div(mod * stat, 100)."""
    return all(c_div(m * stat, 100) == 0 for m in mod_table)


def calc_hit_outcome(seed, atk, dfn, le, ap_mod, dp_mod):
    """Port of fd2_calculate_combat_hit_outcome. Returns (seed, damage, crit,
    miss). Reads weapon class + crit rate live from FD2.LE."""
    atk_ap = u16(atk, OFF_AP)
    def_dp = u16(dfn, OFF_DP)
    atk_hit = u16(atk, OFF_HIT)
    def_evade = u16(dfn, OFF_EVADE)
    job_idx = atk[OFF_JOB] - 1

    weapon_id = equipped_weapon_id(atk)
    if weapon_id < 0:
        raise SystemExit("attacker has no equipped physical weapon; cannot "
                         "predict a physical attack")
    item_base = ADDR_ITEM_TABLE + weapon_id * ITEM_STRIDE
    weapon_class = le.u8(item_base + ITEM_SPECIAL_TYPE)
    wpn_chance = le.u8(item_base + ITEM_SPECIAL_CHANCE)
    crit_pct = le.u8(ADDR_CRIT_TABLE + job_idx)

    atk_ap = atk_ap + c_div(ap_mod * atk_ap, 100)
    def_dp = def_dp + c_div(dp_mod * def_dp, 100)

    crit_total = crit_pct
    if weapon_class == 4:
        crit_total += wpn_chance
    elif weapon_class == 2:
        seed = lfsr(seed)                       # poison proc roll
        if seed % 100 < wpn_chance:
            seed = lfsr(seed)                   # poison value
    # weapon_class == 3 sets a double_hit flag (no RNG); class 0/1 nothing.

    miss, crit, damage = 1, 0, 0
    seed = lfsr(seed)                           # HIT roll
    if seed % 100 < atk_hit - def_evade:
        miss = 0
        seed = lfsr(seed)                       # CRIT roll
        if seed % 100 < crit_total:
            def_dp = c_div(def_dp, 2)
            crit = 1
        damage = c_div((atk_ap - def_dp) * 9, 10)
        if damage < 0:
            damage = 0
        jitter_range = c_div(damage, 9)
        if jitter_range != 0:
            seed = lfsr(seed)                   # JITTER roll
            damage += seed % jitter_range
    return seed, damage, crit, miss


def predict_attack(seed, atk, dfn, le):
    """Port of the cinematic wrapper: 3% double-strike roll, then per strike
    run the outcome formula and drain the defender's hp_current. Returns
    (total_damage_applied, detail dict)."""
    ap_mod_table = [le.i32(ADDR_TILE_AP_MOD + i * 4) for i in range(6)]
    dp_mod_table = [le.i32(ADDR_TILE_DP_MOD + i * 4) for i in range(6)]
    atk_ap = u16(atk, OFF_AP)
    def_dp = u16(dfn, OFF_DP)

    # Terrain needs the runtime tile map (not captured). It is only safe to
    # ignore when every modifier rounds to 0 for these stats; assert that.
    ap_terrain_safe = terrain_mod_is_zero(atk_ap, ap_mod_table)
    dp_terrain_safe = terrain_mod_is_zero(def_dp, dp_mod_table)

    seed = lfsr(seed)                           # double-strike bonus roll
    hit_count = 2 if seed % 100 < 3 else 1

    hp = u16(dfn, OFF_HP)
    start_hp = hp
    strikes = []
    for _ in range(hit_count):
        seed, dmg, crit, miss = calc_hit_outcome(seed, atk, dfn, le, 0, 0)
        applied = min(dmg, hp)
        hp -= applied
        strikes.append({"damage": dmg, "crit": bool(crit), "miss": bool(miss)})
    return start_hp - hp, {
        "hit_count": hit_count,
        "strikes": strikes,
        "ap_terrain_safe": ap_terrain_safe,
        "dp_terrain_safe": dp_terrain_safe,
        "ap_mod_table": ap_mod_table,
        "dp_mod_table": dp_mod_table,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", required=True)
    ap.add_argument("--out", default="", help="run dir name (default = scenario)")
    ap.add_argument("--golden", action="store_true",
                    help="read captures from the golden dir instead of the run dir")
    args = ap.parse_args()

    scen = json.loads((SCEN_DIR / (args.scenario + ".json")).read_text("utf-8"))
    oc = scen.get("oracle")
    if not oc or oc.get("type") != "physical_attack":
        raise SystemExit("scenario %s has no physical_attack oracle" % args.scenario)

    src = (GOLDEN_DIR / args.scenario) if args.golden \
        else (RUN_BASE / (args.out or args.scenario))
    before_hdr, before = load_st(src / ("ST%02d.BIN" % oc["before_cap"]))
    after_hdr, after = load_st(src / ("ST%02d.BIN" % oc["after_cap"]))

    a, d = oc["attacker_idx"], oc["defender_idx"]
    seed = before_hdr[ST_SEED_WORD] & 0xFFFF
    le = LEImage(LE_FILE)

    # Attacker + defender stat inputs come from the BEFORE (pre-damage)
    # capture; the actual result is read from the AFTER capture below.
    predicted, detail = predict_attack(seed, before[a], before[d], le)

    hp_before = u16(before[d], OFF_HP)
    hp_after = u16(after[d], OFF_HP)
    actual = hp_before - hp_after

    ok = (predicted == actual)
    if not (detail["ap_terrain_safe"] and detail["dp_terrain_safe"]):
        ok = False
        detail["terrain_warning"] = (
            "terrain modifier may be non-zero for these stats; oracle needs "
            "the tile map to predict exactly")

    result = {
        "scenario": args.scenario,
        "oracle": "physical_attack",
        "attacker_idx": a,
        "defender_idx": d,
        "seed": "0x%04X" % seed,
        "predicted_damage": predicted,
        "actual_damage": actual,
        "defender_hp_before": hp_before,
        "defender_hp_after": hp_after,
        "pass": ok,
        "detail": detail,
    }
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
