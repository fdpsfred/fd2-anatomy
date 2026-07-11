#!/usr/bin/env python3
"""extract_state.py -- read FD2 state globals out of a DOSBox-X save-state of
the UNMODIFIED original ~FD2.EXE, in the same 16-int32 layout as the replay
harness capture.c ST dump, for the original-vs-rebuild state differential.

How it works: the original runs under DOS4GW (paged, relocated). We locate the
DGROUP data segment inside the save-state 'Memory' component by a read-only
CONST string signature -> that yields DGROUP's physical base for THIS run (a
runtime value, derived per-run, never hardcoded). Each scalar global is then
read at base + its link-time vaddr.

The symbol vaddrs below are link-time constants of FD2.LE taken from Ghidra
(mcp ghidra list_globals); the capture.c header order is the contract. To
regenerate a vaddr: list_globals name_substring=<symbol tail>.
"""
import argparse
import struct
import zipfile
from pathlib import Path

# DGROUP CONST anchor: read-only bytes at link vaddr 0x50004, so they appear
# verbatim in the running original's RAM; their physical offset fixes DGROUP.
DG_ANCHOR = b" Out of Memory !!!\n\x00rb\x00FD2.SAV"
DG_ANCHOR_VADDR = 0x50004
DG_LO, DG_HI = 0x50000, 0x60000          # DGROUP vaddr span; >=0x60000 = FAR_DATA

# capture.c ST header order: (name, vaddr, size, signed). vaddrs from Ghidra.
HDR = [
    ("chapter_current_chapter_id",      0x53c03, 4, False),
    ("chapter_event_or_battle_end_code",0x53ecc, 4, False),
    ("battle_turn_counter",             0x53bef, 4, False),
    ("battle_party_member_count",       0x53beb, 4, False),
    ("shared_party_total_gold",         0x53bf3, 4, True),
    ("battle_cursor_world_x",           0x53ab1, 4, False),
    ("battle_cursor_world_y",           0x53ab5, 4, False),
    ("battle_cursor_screen_x",          0x53ab9, 4, True),
    ("battle_cursor_screen_y",          0x53abd, 4, True),
    ("battle_view_window_origin_x",     0x53aa9, 4, False),
    ("battle_view_window_origin_y",     0x53aad, 4, False),
    ("shared_rng_seed",                 0x627b8, 2, False),   # FAR_DATA segment
    ("battle_ai_best_physical_score",   0x53c4f, 4, False),
    ("battle_ai_best_spell_score",      0x53c23, 4, False),
    ("battle_ai_best_item_score",       0x53c33, 4, False),
    ("reserved",                        None,    0, False),
]


def find_dgroup_base(mem):
    i = mem.find(DG_ANCHOR)
    if i < 0:
        raise SystemExit("DGROUP CONST anchor not found -- original not loaded?")
    if mem.find(DG_ANCHOR, i + 1) >= 0:
        raise SystemExit("DGROUP CONST anchor not unique -- ambiguous base")
    return i - DG_ANCHOR_VADDR            # phys = base + vaddr


def read_global(mem, base, vaddr, size, signed):
    off = base + vaddr
    raw = mem[off:off + size]
    if len(raw) < size:
        return None
    return int.from_bytes(raw, "little", signed=signed)


def extract(sav_path):
    mem = zipfile.ZipFile(sav_path).read("Memory")
    dbase = find_dgroup_base(mem)
    vals = []
    for name, vaddr, size, signed in HDR:
        if vaddr is None:
            vals.append(("reserved", 0))
        elif not (DG_LO <= vaddr < DG_HI):
            vals.append((name, None))     # FAR_DATA etc. -- base not located yet
        else:
            vals.append((name, read_global(mem, dbase, vaddr, size, signed)))
    return dbase, vals


def load_golden_hdr(scenario, idx):
    p = Path(__file__).resolve().parents[2] / "tests/play/golden" / scenario / ("ST%02d.BIN" % idx)
    if not p.is_file():
        return None
    b = p.read_bytes()[:16 * 4]
    return list(struct.unpack("<16i", b))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sav", required=True, help="path to DOSBox-X save-state .sav")
    ap.add_argument("--golden", default="", help="scenario name to diff against")
    ap.add_argument("--idx", type=int, default=0, help="golden ST index")
    args = ap.parse_args()

    dbase, vals = extract(args.sav)
    gold = load_golden_hdr(args.golden, args.idx) if args.golden else None
    print("DGROUP phys base: 0x%08x" % dbase)
    print("%-34s %12s %12s %s" % ("global", "original", "rebuild", "match"))
    for i, (name, v) in enumerate(vals):
        g = gold[i] if gold else None
        vs = "n/a" if v is None else str(v)
        gs = "" if g is None else str(g)
        m = "" if g is None or v is None else ("OK" if v == g else "DIFF")
        print("%-34s %12s %12s %s" % (name, vs, gs, m))


if __name__ == "__main__":
    main()
