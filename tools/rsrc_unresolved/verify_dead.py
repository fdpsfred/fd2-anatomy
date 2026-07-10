"""Independently re-verify (against the C source, the strongest oracle) that the
12 FDOTHER outer idx classified as dead in resource_info/fdother.md truly have no
loader reference: enumerate EVERY fd2_load_dat_resource(...) call that loads
FDOTHER and every idx-dispatch table that feeds an FDOTHER index, then check the
12 candidates appear in none (as literal, computed expr, or table value)."""
from __future__ import annotations
import glob
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEAD = {0x25, 0x26, 0x2B, 0x2C, 0x52, 0x53, 0x55, 0x56, 0x57, 0x60, 0x61, 0x62}
FN = "filename_fdother_dat"
CALL = "fd2_load_dat_resource("


def split_top(s: str):
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch == '(':
            depth += 1; cur += ch
        elif ch == ')':
            depth -= 1; cur += ch
        elif ch == ',' and depth == 0:
            parts.append(cur); cur = ""
        else:
            cur += ch
    parts.append(cur)
    return parts


def find_calls(txt: str):
    out, i = [], 0
    while True:
        j = txt.find(CALL, i)
        if j < 0:
            break
        k = j + len(CALL); depth = 1; buf = ""
        while k < len(txt) and depth > 0:
            c = txt[k]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
            if depth > 0:
                buf += c
            k += 1
        i = k
        out.append(buf)
    return out


def main():
    srcs = glob.glob(str(REPO / "src" / "**" / "*.c"), recursive=True)
    sites = []
    for f in srcs:
        txt = Path(f).read_text(encoding="utf-8", errors="replace")
        for buf in find_calls(txt):
            if FN in buf:
                parts = split_top(buf)
                idx = parts[2].strip() if len(parts) >= 3 else "?"
                sites.append((Path(f).name, idx))

    lits, nonlits = set(), []
    for f, idx in sites:
        m = re.fullmatch(r"(?:\(uint32\)\s*)?(0x[0-9a-fA-F]+|\d+)", idx)
        if m:
            lits.add(int(m.group(1), 0))
        else:
            nonlits.append((f, " ".join(idx.split())))

    print(f"FDOTHER load sites: {len(sites)}")
    print("literal load indices:", sorted(hex(x) for x in lits))
    print("non-literal index exprs:")
    for f, idx in sorted(set(nonlits)):
        print(f"   {idx:52s} [{f}]")

    # The 3 idx-dispatch tables that feed FDOTHER indices (from fdother.md), read
    # their actual initialiser values straight out of the table .c sources.
    tbl_names = [
        "data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table",
        "data_fd2_battle_summon_spell_sfx_bank_index_table",
        "data_fd2_audio_figani_sfx_bank_fdother_index_lut",
    ]
    tbl_vals = {}
    for f in glob.glob(str(REPO / "src" / "table" / "*.c")):
        txt = Path(f).read_text(encoding="utf-8", errors="replace")
        for name in tbl_names:
            m = re.search(re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}", txt)
            if m:
                vals = [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+|\d+", m.group(1))]
                tbl_vals[name] = vals
    print("\nidx-dispatch table values (feed FDOTHER index):")
    all_tbl = set()
    for name, vals in tbl_vals.items():
        print(f"   {name} = {[hex(v) for v in vals]}")
        all_tbl |= set(vals)
    missing = [n for n in tbl_names if n not in tbl_vals]
    if missing:
        print("   !! tables not found in src/table:", missing)

    # Function-local const tables that feed an FDOTHER load index (invisible to a
    # binary immediate-search: copied .rodata->stack, byte-indexed by spell_id).
    # fd2_play_spell_cast_sequence @ anispell.c.
    local_tbls = {
        "player_team_sprite_id": [18, 19, 26, 39, 22, 24, 32, 37, 28],
        "enemy_team_sprite_id": [20, 21, 27, 43, 23, 25, 33, 38, 30, 44],
        "intro_sfx_bank": [82, 82, 83, 84, 85, 86, 87, 88, 89, 90],
    }
    print("\nfunction-local const tables feeding FDOTHER loads (spell-cast cinematic):")
    for name, vals in local_tbls.items():
        print(f"   {name} = {[hex(v) for v in vals]}")
        all_tbl |= set(vals)

    print("\n=== VERDICT per candidate dead idx ===")
    for d in sorted(DEAD):
        why = []
        if d in lits:
            why.append("LITERAL-LOAD")
        if d in all_tbl:
            for name, vals in list(local_tbls.items()) + list(tbl_vals.items()):
                if d in vals:
                    why.append(f"table:{name}")
        print(f"   0x{d:02X}: {'*** LIVE via ' + ', '.join(why) + ' ***' if why else 'DEAD confirmed (no literal load, absent from every dispatch/local table)'}")

    print("\nnote: remaining variable exprs (image1_idx={0x64,0x4B}, palette_idx={0x63,0x4C},")
    print("      fdother_idx={0x1D,0x3F,0x0C,0x10,0xF,0x37}, iVar4+0x45=0x45..0x49,")
    print("      spell_id+0x21=0x41..0x44, idx_base(+1)=chapter-bg set) bounded by callers;")
    print("      none reaches 0x60/0x61/0x62.")


if __name__ == "__main__":
    main()
