#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_growth.py -- FD2 character stat-growth table generator.

Reads the four game data tables dumped from FD2.LE (workspace/growth_table/
raw_tables.json) and computes, for every playable character, the per-level
stat table across its whole growth path (base job -> LV40, then each possible
promotion target LV1..40; Gaia/Ward: base job -> LV99).

Two variants per stat curve:
  - "min": every level-up rolls the minimum gain (the deterministic floor,
     identical to the game's spawn/recruit formula).
  - "max": every level-up rolls the maximum gain (the theoretical ceiling).

Verified game mechanics (all from src/ + Ghidra, see accompanying KB):
  * Join level      = character_base[char_id] + 2  (recruit path
                      fd2_init_runtime_char_from_base_growth uses base[+2]).
  * Spawn stats     = HP/MP = base + growth_min*(LV-1);
                      AP/DP/DX = base + growth_min*LV.
  * Level-up gain   = fd2_roll_stat_gain_and_show_message: gain in
                      [min_byte, max_byte-1]; range 0 (max==min) => fixed min.
  * Growth entry    = growth_table[portrait_id]; base char portrait==char_id;
                      promotion target portrait = char_id+0x20 / +0x32 / 0x34.
  * Level cap       = 99 for portrait 0x1E/0x1F (Gaia/Ward), else 40 (0x28).
  * Promotion       = level resets to 1, exp cleared, HP/MP restored; ONE roll
                      of the new growth applied immediately; then LV1..40.
                      Eligible: portrait_id in 0..0x11 and != 7 (Lancelot).
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
RAW = os.path.join(ROOT, "workspace", "growth_table", "raw_tables.json")
OUT = os.path.join(ROOT, "workspace", "growth_table", "growth_data.json")

MAX_LEVEL_NORMAL = 40   # 0x28
MAX_LEVEL_HERO = 99     # Gaia (portrait 0x1E) / Ward (portrait 0x1F)

# Mechanically-reachable but game-CRASHING promotions -> excluded from the tables.
# char_id -> set of buggy target portrait ids.
#   Kairas (16, base class 龍劍士) is flagged promotion-eligible (portrait 0x10 < 0x12,
#   != 7), joins at LV2, reaches the LV20 promote threshold, and his only target 0x30
#   (聖戰士) has real growth data + a real 1627-byte portrait -- yet EMPIRICALLY (playtest-
#   confirmed) actually promoting Kairas to 聖戰士 crashes the game. Static analysis of the
#   promotion path did NOT find the fault (portrait/growth/promo/cinematic indices are all
#   in-bounds), so the crash is subtler than a missing resource; root cause under study.
#   Kept out of the usable growth tables and surfaced as a documented crash bug instead.
KNOWN_CRASH_PROMOTIONS = {16: {0x30}}

CHAR_NAMES = [
    "索爾", "哈諾", "鐵諾", "哈瓦特",
    "亞雷斯", "洛娜", "萊汀", "蘭斯洛特",
    "希莉亞", "悠妮", "瑪琳", "索菲亞",
    "凱麗", "貝克威", "珊", "塞可邦勒",
    "凱拉斯", "米亞斯多德", "蜜蒂",
    "羅德曼", "莎拉", "約拿", "卡里斯",
    "羅蘭", "希爾法", "謝多", "聖寇拉斯",
    "巴拿羅西亞", "達克塞", "亞齊梅吉",
    "蓋亞", "渥德",
]

JOB_NAMES = {
    0x00: "龍", 0x01: "劍士", 0x02: "戰士", 0x03: "騎士",
    0x04: "弓兵", 0x05: "法師", 0x06: "僧侶", 0x07: "盜賊",
    0x08: "武者", 0x09: "劍聖", 0x0A: "聖戰士", 0x0B: "聖騎士",
    0x0C: "狙擊手", 0x0D: "大法師", 0x0E: "祭師", 0x0F: "龍劍士",
    0x10: "鬥士", 0x11: "英雄", 0x12: "魔戰士", 0x13: "龍騎士",
    0x14: "神射手", 0x15: "召喚師", 0x16: "聖者", 0x17: "忍者",
    0x18: "武聖", 0x19: "機兵", 0x1A: "？？？", 0x1C: "？？？",
}

ITEM_NAMES = {
    0x58: "聖者之戒", 0x59: "勇者徽章",
    0x5A: "精靈契印", 0x5B: "領悟之書",
    0x5C: "心眼之書", 0x5D: "白金徽章",
    0xCD: "飛龍卵",
}

RACE_NAMES = {1: "人類", 2: "精靈", 3: "豹人", 4: "龍人", 5: "魔族",
              6: "機械", 7: "魔神", 8: "獸人", 9: "魔物", 0x0A: "龍"}

STATS = ["hp", "mp", "ap", "dp", "dx"]


def hx(s):
    return bytes.fromhex(s)


def u16(b, i):
    return b[i] | (b[i + 1] << 8)


def parse_base(raw):
    b = hx(raw["hex"])
    st = raw["stride"]
    out = []
    for i in range(raw["count"]):
        e = b[i * st:(i + 1) * st]
        out.append({
            "race": e[0], "cls": e[1], "level": e[2],
            "hp": u16(e, 3), "mp": u16(e, 5), "mv": e[7],
            "ap": u16(e, 0x12), "dp": u16(e, 0x14), "dx": u16(e, 0x16),
        })
    return out


def parse_growth(raw):
    b = hx(raw["hex"])
    st = raw["stride"]
    out = []
    for i in range(raw["count"]):
        e = b[i * st:(i + 1) * st]
        # order in entry: AP, DP, DX, HP, MP (each min,max), then spell idx
        out.append({
            "ap": (e[0], e[1]), "dp": (e[2], e[3]), "dx": (e[4], e[5]),
            "hp": (e[6], e[7]), "mp": (e[8], e[9]), "spell_idx": e[10],
        })
    return out


def parse_promotion(raw):
    b = hx(raw["hex"])
    st = raw["stride"]
    base = raw["base_class_id"]
    out = {}
    for i in range(raw["count"]):
        e = b[i * st:(i + 1) * st]
        out[base + i] = {"job": e[0], "mv_bonus": e[1]}
    return out


def parse_keyitem(raw):
    return list(hx(raw["hex"]))


def gains(pair):
    """(min_gain, max_gain) actually applied per level for a (min,max) byte pair."""
    mn, mx = pair
    gmin = mn
    gmax = mx - 1 if mx > mn else mn   # range 0 => fixed min
    return gmin, gmax


def base_segment(base, growth, join_level, cap):
    """First-job per-level table from join_level..cap. Returns list of level dicts."""
    g = {s: gains(growth[s]) for s in STATS}
    rows = []
    for L in range(join_level, cap + 1):
        mn, mx = {}, {}
        for s in STATS:
            gmin, gmax = g[s]
            b = base[s]
            if s in ("hp", "mp"):
                spawn = b + gmin * (join_level - 1)
                mn[s] = b + gmin * (L - 1)
                mx[s] = spawn + gmax * (L - join_level)
            else:  # ap/dp/dx use * level
                spawn = b + gmin * join_level
                mn[s] = b + gmin * L
                mx[s] = spawn + gmax * (L - join_level)
        rows.append({"lv": L, "min": mn, "max": mx})
    return rows


def promo_segment(first_final_min, first_final_max, new_growth, cap=MAX_LEVEL_NORMAL):
    """Second-job table LV1..cap, carrying over first-job final stats then
    applying N rolls of the new growth (the promotion roll counts as level 1)."""
    g = {s: gains(new_growth[s]) for s in STATS}
    rows = []
    for N in range(1, cap + 1):
        mn, mx = {}, {}
        for s in STATS:
            gmin, gmax = g[s]
            mn[s] = first_final_min[s] + gmin * N
            mx[s] = first_final_max[s] + gmax * N
        rows.append({"lv": N, "min": mn, "max": mx})
    return rows


def growth_str(growth):
    """Human-readable growth spec: each stat 'min~maxgain'."""
    out = {}
    for s in STATS:
        gmin, gmax = gains(growth[s])
        out[s] = [gmin, gmax]
    out["spell_idx"] = growth["spell_idx"]
    return out


def main():
    raw = json.load(open(RAW, encoding="utf-8"))
    base = parse_base(raw["character_base_table"])
    growth = parse_growth(raw["character_growth_table"])
    promo = parse_promotion(raw["class_promotion_table"])
    keyitem = parse_keyitem(raw["portrait_class_change_key_item_table"])

    chars = []
    report = []
    for cid in range(32):
        b = base[cid]
        name = CHAR_NAMES[cid]
        portrait = cid   # base char portrait == char_id
        cap = MAX_LEVEL_HERO if portrait in (0x1E, 0x1F) else MAX_LEVEL_NORMAL
        join = b["level"]
        base_stats = {s: b[s] for s in STATS}
        first_growth = growth[cid]
        first_seg = base_segment(base_stats, first_growth, join, cap)
        first_final_min = first_seg[-1]["min"] if cap == MAX_LEVEL_NORMAL else None
        first_final_max = first_seg[-1]["max"] if cap == MAX_LEVEL_NORMAL else None
        # For promotion carry-over we always branch from LV40 of the base job.
        if cap == MAX_LEVEL_NORMAL:
            lv40 = next(r for r in first_seg if r["lv"] == 40)
            first_final_min = lv40["min"]
            first_final_max = lv40["max"]

        eligible = (portrait <= 0x11 and portrait != 7)
        crash_set = KNOWN_CRASH_PROMOTIONS.get(cid, set())
        targets = []
        buggy = []
        if eligible:
            for label, tgt, req in [("default", portrait + 0x20, None)] + \
                    ([("alt", portrait + 0x32, keyitem[portrait])] if keyitem[portrait] != 0xFF else []) + \
                    ([("summoner", 0x34, 0x5A)] if portrait == 9 else []):
                if tgt in crash_set:
                    buggy.append((label, tgt, req))
                else:
                    targets.append((label, tgt, req))
        # "promotable" = has at least one usable (non-crashing) promotion.
        promotable = len(targets) > 0

        segments = [{
            "kind": "base",
            "job_id": b["cls"], "job_name": JOB_NAMES.get(b["cls"], "?"),
            "from_level": join, "to_level": cap,
            "growth": growth_str(first_growth),
            "rows": first_seg,
        }]

        promo_summ = []
        for label, tgt, req_item in targets:
            pj = promo[tgt]["job"]
            mvb = promo[tgt]["mv_bonus"]
            ng = growth[tgt]
            seg = promo_segment(first_final_min, first_final_max, ng)
            segments.append({
                "kind": "promotion",
                "label": label,
                "target_portrait": tgt,
                "job_id": pj, "job_name": JOB_NAMES.get(pj, "?"),
                "required_item": req_item,
                "required_item_name": ITEM_NAMES.get(req_item) if req_item else None,
                "mv_bonus": mvb,
                "from_level": 1, "to_level": MAX_LEVEL_NORMAL,
                "growth": growth_str(ng),
                "rows": seg,
            })
            promo_summ.append("%s=%s(0x%02X,item=%s,mv+%d)" % (
                label, JOB_NAMES.get(pj, "?"), tgt,
                ITEM_NAMES.get(req_item, "-") if req_item else "-", mvb))

        buggy_promotions = [{
            "label": label, "target_portrait": tgt,
            "job_id": promo[tgt]["job"], "job_name": JOB_NAMES.get(promo[tgt]["job"], "?"),
            "note": "mechanically reachable but promoting crashes the game (excluded)",
        } for (label, tgt, req) in buggy]

        chars.append({
            "char_id": cid, "name": name,
            "race": b["race"], "race_name": RACE_NAMES.get(b["race"], "?"),
            "base_job_id": b["cls"], "base_job_name": JOB_NAMES.get(b["cls"], "?"),
            "join_level": join, "max_level": cap,
            "base_stats": base_stats,
            "mv": b["mv"],
            "promotable": promotable,
            "buggy_promotions": buggy_promotions,
            "segments": segments,
        })
        report.append("[%2d] %-6s race=%s job=%s joinLV=%2d cap=%d  promos: %s" % (
            cid, name, RACE_NAMES.get(b["race"], "?"), JOB_NAMES.get(b["cls"], "?"),
            join, cap, " | ".join(promo_summ) if promo_summ else "(none)"))

    result = {
        "meta": {
            "source": "FD2.LE via Ghidra MCP; formulas verified from src/",
            "stats": STATS,
            "max_level_normal": MAX_LEVEL_NORMAL,
            "max_level_hero": MAX_LEVEL_HERO,
            "note_min": "min variant = every level-up rolls minimum gain (= spawn/recruit floor)",
            "note_max": "max variant = every level-up rolls maximum gain (byte max-1)",
            "note_promo": "promotion carries base-job LV40 stats + one roll of the new growth, then LV1..40",
        },
        "job_names": {str(k): v for k, v in JOB_NAMES.items()},
        "item_names": {str(k): v for k, v in ITEM_NAMES.items()},
        "characters": chars,
    }
    json.dump(result, open(OUT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)

    # ---- compact model for the web page (params only; JS recomputes levels) ----
    compact_chars = []
    for c in chars:
        base_mv = c["mv"]
        segs = []
        for s in c["segments"]:
            mv_bonus = s.get("mv_bonus", 0)
            segs.append({
                "kind": s["kind"],
                "label": s.get("label"),
                "job_name": s["job_name"],
                "item_name": s.get("required_item_name"),
                "mv_bonus": mv_bonus,
                "mv": base_mv + mv_bonus,   # MV is fixed per job (base +0x07, + promotion bonus)
                "from": s["from_level"], "to": s["to_level"],
                "growth": {k: s["growth"][k] for k in STATS},
                "spell_idx": s["growth"]["spell_idx"],
            })
        compact_chars.append({
            "id": c["char_id"], "name": c["name"], "race": c["race_name"],
            "base_job": c["base_job_name"], "join": c["join_level"], "cap": c["max_level"],
            "base": c["base_stats"], "base_mv": base_mv, "promotable": c["promotable"],
            "buggy": [{"job_name": b["job_name"], "note": b["note"]} for b in c["buggy_promotions"]],
            "segments": segs,
        })
    compact = {"stats": STATS, "chars": compact_chars,
               "max_level_normal": MAX_LEVEL_NORMAL, "max_level_hero": MAX_LEVEL_HERO}
    json.dump(compact, open(os.path.join(os.path.dirname(OUT), "growth_compact.json"),
              "w", encoding="utf-8"), ensure_ascii=False, indent=1)

    # ---- verification report ----
    print("=== Character growth summary ===")
    for line in report:
        print(line)
    print("\n=== Boundary growth entries (raw min,max bytes) ===")
    for idx in (0x30, 0x42, 0x43):
        g = growth[idx]
        print("entry 0x%02X: AP%s DP%s DX%s HP%s MP%s spell=0x%02X" % (
            idx, g["ap"], g["dp"], g["dx"], g["hp"], g["mp"], g["spell_idx"]))
    print("\n=== key_item table (portrait -> item) ===")
    for p in range(18):
        ki = keyitem[p]
        print("  portrait 0x%02X (%s): 0x%02X %s" % (
            p, CHAR_NAMES[p], ki, ITEM_NAMES.get(ki, "(none)" if ki == 0xFF else "?")))
    print("\nWrote %s" % OUT)
    print("chars=%d" % len(chars))


if __name__ == "__main__":
    main()
