"""FDFIELD.DAT layout inspector — proves the byte-level layout of every
tile_event_data_table entry (chapter_id*3 + 1) by counting active hooks,
pickups, and char_spawn records.

Per-entry layout (33 entries: 30 main chapters + 3 endgame):
    +0x00     u8       shap_id
    +0x01     u8       party_member_count
    +0x02     u8       char_spawn_count (N)
    +0x03..+0x32  16 x 3 bytes = 48 B  turn_event_hooks[16]
    +0x33..+0x52  16 x 2 bytes = 32 B  tile_step_event_hooks[16]
    +0x53..+0x82  16 x 3 bytes = 48 B  tile_pickup_table[16]
    +0x83..       N x 0x1A bytes      char_spawn_records[N]

Math: 3 + 48 + 32 + 48 = 131 = 0x83.

CLI:
    python tools/decoders/fdfield_layout_inspector.py --all
    python tools/decoders/fdfield_layout_inspector.py --chapter 5
    python tools/decoders/fdfield_layout_inspector.py --json --out report.json
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# tools/decoders/fdfield_layout_inspector.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[4]
FDFIELD_PATH = REPO_ROOT / "FDFIELD.DAT"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dat_header_parser import read_dat_entry  # noqa: E402

HEADER_SIZE = 0x83
RECORD_SIZE = 0x1A


def parse_tile_event_entry(entry: bytes, ch_id: int) -> dict:
    shap_id = entry[0]
    party_member_count = entry[1]
    char_spawn_count = entry[2]

    turn_hooks = []
    for i in range(16):
        off = 0x03 + i * 3
        turn = entry[off]
        event_code = entry[off + 1]
        phase = entry[off + 2]
        turn_hooks.append({
            "hook_idx": i, "turn": turn, "event_code": event_code,
            "phase": phase,
            "is_sentinel": (turn == 0xFF and event_code == 0xFF and phase == 0),
        })

    tile_step_hooks = []
    for i in range(16):
        off = 0x33 + i * 2
        consequence_idx = entry[off]
        event_type = entry[off + 1]
        tile_step_hooks.append({
            "hook_idx": i,
            "consequence_idx": consequence_idx,
            "event_type": event_type,
            "is_sentinel": (consequence_idx == 0xFF and event_type == 0xFF),
        })

    pickup_table = []
    for i in range(16):
        off = 0x53 + i * 3
        a, b, c = entry[off], entry[off + 1], entry[off + 2]
        pickup_table.append({
            "slot": i, "byte_a": a, "byte_b": b, "byte_c": c,
            "is_sentinel": (a == 0xFF and b == 0xFF and c == 0xFF),
        })

    expected_size = HEADER_SIZE + char_spawn_count * RECORD_SIZE
    actual_size = len(entry)
    size_match = (actual_size == expected_size)
    size_match_with_reserved = (
        not size_match and actual_size == expected_size + RECORD_SIZE
    )

    return {
        "ch_id": ch_id,
        "shap_id": shap_id,
        "party_member_count": party_member_count,
        "char_spawn_count": char_spawn_count,
        "expected_size": expected_size,
        "actual_size": actual_size,
        "size_match_exact": size_match,
        "size_match_with_1_reserved_record": size_match_with_reserved,
        "turn_event_hooks_active_count":
            sum(1 for h in turn_hooks if not h["is_sentinel"]),
        "tile_step_event_hooks_active_count":
            sum(1 for h in tile_step_hooks if not h["is_sentinel"]),
        "pickup_table_active_count":
            sum(1 for p in pickup_table if not p["is_sentinel"]),
    }


def chapter_label(ch_id: int) -> str:
    return f"ch{ch_id+1}" if ch_id < 30 else f"endgame_ch{ch_id}"


def cmd_all(want_json: bool, out_path: Path | None) -> int:
    parsed_entries = []
    for ch_id in range(33):
        tile_event_idx = ch_id * 3 + 1
        try:
            entry = read_dat_entry(FDFIELD_PATH, tile_event_idx)
            info = parse_tile_event_entry(entry, ch_id)
            info["fdfield_idx"] = tile_event_idx
            info["chapter_label"] = chapter_label(ch_id)
            parsed_entries.append(info)
        except Exception as e:
            parsed_entries.append({"ch_id": ch_id, "error": str(e)})

    total_size_match = sum(1 for e in parsed_entries
                           if e.get("size_match_exact"))
    total_size_match_reserved = sum(1 for e in parsed_entries
                                    if e.get("size_match_with_1_reserved_record"))
    total_size_mismatch = sum(
        1 for e in parsed_entries
        if not e.get("size_match_exact")
        and not e.get("size_match_with_1_reserved_record")
        and "error" not in e
    )
    total_turn_hooks = sum(e.get("turn_event_hooks_active_count", 0)
                           for e in parsed_entries)
    total_step_hooks = sum(e.get("tile_step_event_hooks_active_count", 0)
                           for e in parsed_entries)
    total_pickups = sum(e.get("pickup_table_active_count", 0)
                        for e in parsed_entries)
    total_chars = sum(e.get("char_spawn_count", 0)
                      for e in parsed_entries)

    print(f"FDFIELD entry layout proof (33 chapters):")
    print(f"  Size 0x83 + N*0x1A exact match     : {total_size_match}/33")
    print(f"  Size match with +1 reserved record : {total_size_match_reserved}/33")
    print(f"  Size mismatch                      : {total_size_mismatch}/33")
    print(f"  Total active turn-event hooks  : {total_turn_hooks}")
    print(f"  Total active tile-step hooks   : {total_step_hooks}")
    print(f"  Total active pickup entries    : {total_pickups}")
    print(f"  Total char_spawn_records       : {total_chars}")
    print()
    for e in parsed_entries:
        if "error" in e:
            print(f"  {e.get('ch_id')}: ERROR {e['error']}")
            continue
        match_str = ("exact" if e["size_match_exact"]
                     else ("+1reserved" if e["size_match_with_1_reserved_record"]
                           else "MISMATCH"))
        print(f"  {e['chapter_label']:14s} shap_id=0x{e['shap_id']:02X} "
              f"chars={e['char_spawn_count']:>3d} "
              f"size={e['actual_size']:>5d} (expected {e['expected_size']:>5d}, "
              f"{match_str})  turn_hooks={e['turn_event_hooks_active_count']:>2d} "
              f"step_hooks={e['tile_step_event_hooks_active_count']:>2d} "
              f"pickup={e['pickup_table_active_count']:>2d}")

    if want_json:
        report = {
            "header_size": HEADER_SIZE,
            "header_size_math": "3 + 16*3 + 16*2 + 16*3 = 131 = 0x83",
            "record_size": RECORD_SIZE,
            "stats": {
                "size_match_exact": total_size_match,
                "size_match_with_reserved": total_size_match_reserved,
                "size_mismatch": total_size_mismatch,
                "total_active_turn_event_hooks": total_turn_hooks,
                "total_active_tile_step_event_hooks": total_step_hooks,
                "total_active_pickup_entries": total_pickups,
                "total_char_spawn_records": total_chars,
            },
            "entries": parsed_entries,
        }
        if out_path:
            out_path.parent.mkdir(parents=True, exist_ok=True)
            out_path.write_text(json.dumps(report, indent=2, ensure_ascii=False),
                                encoding="utf-8")
            print(f"\nwrote {out_path}")
        else:
            print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


def cmd_chapter(chapter_n: int) -> int:
    ch_id = chapter_n - 1
    tile_event_idx = ch_id * 3 + 1
    entry = read_dat_entry(FDFIELD_PATH, tile_event_idx)
    info = parse_tile_event_entry(entry, ch_id)
    info["fdfield_idx"] = tile_event_idx
    info["chapter_label"] = chapter_label(ch_id)
    print(json.dumps(info, indent=2))
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--all", action="store_true",
                   help="inspect all 33 chapters")
    g.add_argument("--chapter", type=int,
                   help="inspect one chapter (1..30) and print JSON")
    p.add_argument("--json", action="store_true",
                   help="emit JSON report (with --all)")
    p.add_argument("--out", type=Path,
                   help="JSON output path (with --json)")
    args = p.parse_args()
    if args.all:
        return cmd_all(args.json, args.out)
    return cmd_chapter(args.chapter)


if __name__ == "__main__":
    sys.exit(main())
