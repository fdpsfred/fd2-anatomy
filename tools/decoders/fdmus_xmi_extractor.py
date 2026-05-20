"""FDMUS.DAT XMI extractor + IFF chunk inspector — extracts the Miles XMIDI
sequences stored in each FDMUS entry as IFF FORM/XDIR chunks.

FDMUS layout (verified by hex peek):
    idx 1, 3, 4, 6, 8, 10..19  XMI sequences (size 562..12784 bytes)
    idx 0, 2, 5, 7, 9          3-byte placeholders (' \\r\\n')

Per-chapter BGM tables (from FD2.LE @ 0x51E63 + 0x51E81):
    player_turn[30]  -> see PLAYER_BGM constant below
    enemy_turn[30]   -> see ENEMY_BGM constant below

CLI:
    python tools/decoders/fdmus_xmi_extractor.py --list
    python tools/decoders/fdmus_xmi_extractor.py --extract --out-dir ./out
    python tools/decoders/fdmus_xmi_extractor.py --bgm-table     # show per-chapter mapping
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

# tools/decoders/fdmus_xmi_extractor.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[2]
FDMUS_PATH = REPO_ROOT / "fd2_game_files" / "FDMUS.DAT"
DEFAULT_OUT_DIR = REPO_ROOT / "workspace" / "decoders" / "fdmus"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dat_header_parser import parse_dat_header, read_dat_entry  # noqa: E402

PLAYER_BGM = [0x13, 0x13, 0x13, 0x13, 0x03, 0x13, 0x13, 0x13, 0x03, 0x04,
              0x13, 0x13, 0x13, 0x13, 0x03, 0x13, 0x04, 0x13, 0x13, 0x03,
              0x13, 0x03, 0x04, 0x13, 0x03, 0x13, 0x04, 0x13, 0x13, 0x08]
ENEMY_BGM = [0x0C, 0x0C, 0x01, 0x0C, 0x06, 0x0C, 0x0C, 0x01, 0x06, 0x04,
             0x0C, 0x01, 0x0C, 0x01, 0x06, 0x0C, 0x08, 0x0C, 0x0C, 0x06,
             0x0C, 0x06, 0x08, 0x0C, 0x06, 0x01, 0x08, 0x01, 0x01, 0x08]


def parse_iff_chunks(data: bytes, depth: int = 0) -> list:
    out = []
    i = 0
    while i + 8 <= len(data):
        chunk_id = data[i:i + 4]
        size = struct.unpack_from(">I", data, i + 4)[0]
        payload_start = i + 8
        chunk = {
            "depth": depth,
            "offset": i,
            "id": chunk_id.decode("ascii", errors="replace"),
            "size": size,
            "payload_first_16": data[payload_start:payload_start + 16].hex().upper(),
        }
        out.append(chunk)
        if chunk_id in (b"FORM", b"LIST", b"CAT "):
            sub_id = data[payload_start:payload_start + 4]
            chunk["sub_id"] = sub_id.decode("ascii", errors="replace")
            sub_payload = data[payload_start + 4:payload_start + 4 + size - 4]
            out.extend(parse_iff_chunks(sub_payload, depth + 1))
        i = payload_start + size
        if size % 2:
            i += 1
    return out


def cmd_list() -> int:
    hdr = parse_dat_header(FDMUS_PATH)
    print(f"FDMUS.DAT: {hdr['file_size']} bytes, {hdr['entry_count']} entries")
    print(f"{'idx':>4} {'size':>8} kind         first8")
    for e in hdr["entries"]:
        size = e["size"]
        if size == 3:
            kind = "placeholder"
        elif e["first16_hex"].startswith("464F524D"):  # "FORM"
            kind = "iff_xmidi"
        else:
            kind = "unknown"
        print(f"{e['idx']:>4} {e['size']:>8} {kind:<12} {e['first16_hex'][:16]}")
    return 0


def cmd_extract(out_dir: Path) -> int:
    out_dir.mkdir(parents=True, exist_ok=True)
    hdr = parse_dat_header(FDMUS_PATH)
    summary = {"fdmus_entries": []}
    for e in hdr["entries"]:
        idx = e["idx"]
        entry = read_dat_entry(FDMUS_PATH, idx)
        info = {
            "idx": idx,
            "size": len(entry),
            "first16_hex": entry[:16].hex().upper(),
            "in_player_turn_bgm": idx in PLAYER_BGM,
            "in_enemy_turn_bgm": idx in ENEMY_BGM,
        }
        if len(entry) == 3:
            info["classification"] = "placeholder_3byte"
        elif entry[:4] == b"FORM":
            info["classification"] = "iff_xmidi"
            chunks = parse_iff_chunks(entry)
            info["iff_chunks"] = chunks[:30]
            xmi_path = out_dir / f"fdmus_idx_{idx:02d}.xmi"
            xmi_path.write_bytes(entry)
            info["artifact_xmi"] = str(xmi_path)
        else:
            info["classification"] = "unknown_format"
        summary["fdmus_entries"].append(info)
        print(f"FDMUS[{idx:>2d}]  size={len(entry):>5d}  "
              f"{info['classification']:<18}  "
              f"player={'Y' if info['in_player_turn_bgm'] else '.'}  "
              f"enemy={'Y' if info['in_enemy_turn_bgm'] else '.'}")
    out_json = out_dir / "fdmus_summary.json"
    out_json.write_text(json.dumps(summary, indent=2, ensure_ascii=False),
                        encoding="utf-8")
    print(f"\nwrote {out_json}")
    return 0


def cmd_bgm_table() -> int:
    print(f"{'ch':>3}  player_turn  enemy_turn")
    for i in range(30):
        print(f"{i+1:>3}  0x{PLAYER_BGM[i]:02X}         0x{ENEMY_BGM[i]:02X}")
    used = set(PLAYER_BGM) | set(ENEMY_BGM) | {0x10, 0x11, 0x12}
    print(f"\nReferenced FDMUS idx (BGM tables + main_menu + special): "
          f"{sorted(used)}")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--list", action="store_true",
                   help="list FDMUS entries by classification")
    g.add_argument("--extract", action="store_true",
                   help="extract every IFF/XMI entry to --out-dir")
    g.add_argument("--bgm-table", action="store_true",
                   help="print per-chapter BGM idx table")
    p.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR,
                   help="directory to write XMI artifacts + summary JSON. "
                   f"default: workspace/decoders/fdmus")
    args = p.parse_args()
    if args.list:
        return cmd_list()
    if args.extract:
        return cmd_extract(args.out_dir)
    if args.bgm_table:
        return cmd_bgm_table()
    return 0


if __name__ == "__main__":
    sys.exit(main())
