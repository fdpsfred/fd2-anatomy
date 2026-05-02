"""BG.DAT + TAI.DAT decoder — extracts the 320x100 RLE background image
from BG entry 0 and inspects TAI/BG entry sizes.

BG.DAT entry 0 layout:
    +0x00  u16 LE  width   (= 320)
    +0x02  u16 LE  height  (= 100)
    +0x04  RLE-encoded 320x100 background image

CLI:
    python tools/decoders/bg_tai_decoder.py --list
    python tools/decoders/bg_tai_decoder.py --decode-bg 0 --out-dir ./out
    python tools/decoders/bg_tai_decoder.py --tai-summary
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# tools/decoders/bg_tai_decoder.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[4]
BG_PATH = REPO_ROOT / "BG.DAT"
TAI_PATH = REPO_ROOT / "TAI.DAT"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dat_header_parser import parse_dat_header, read_dat_entry  # noqa: E402
from rle_decoder import rle_decode, write_pgm_indexed  # noqa: E402


def cmd_list() -> int:
    for path in (BG_PATH, TAI_PATH):
        hdr = parse_dat_header(path)
        print(f"{path.name}: {hdr['file_size']} bytes, "
              f"{hdr['entry_count']} entries")
        print(f"{'idx':>4} {'size':>8} first16")
        for e in hdr["entries"]:
            print(f"{e['idx']:>4} {e['size']:>8} {e['first16_hex']}")
        print()
    return 0


def cmd_decode_bg(idx: int, out_dir: Path | None) -> int:
    bg = read_dat_entry(BG_PATH, idx)
    if len(bg) < 4:
        print(f"BG.DAT[{idx}] only {len(bg)} bytes - too small to decode")
        return 0
    width, height = struct.unpack_from("<2H", bg, 0)
    print(f"BG.DAT[{idx}]: {len(bg)} bytes; header dim {width}x{height}; "
          f"first8 {bg[:8].hex().upper()}")
    rle_data = bg[4:]
    decoded = rle_decode(rle_data, max_pixels=width * height)
    print(f"  decoded: {len(decoded)} pixels (target {width * height})")
    if out_dir is not None:
        out_dir.mkdir(parents=True, exist_ok=True)
        out_pgm = out_dir / f"bg_idx_{idx}_{width}x{height}.pgm"
        write_pgm_indexed(decoded, width, height, out_pgm)
        print(f"  wrote {out_pgm}")
    return 0


def cmd_tai_summary() -> int:
    hdr = parse_dat_header(TAI_PATH)
    print(f"TAI.DAT: {hdr['file_size']} bytes, {hdr['entry_count']} entries")
    sizes_by_count: dict[int, int] = {}
    for e in hdr["entries"]:
        sizes_by_count[e["size"]] = sizes_by_count.get(e["size"], 0) + 1
    print(f"\nSize histogram:")
    for sz, cnt in sorted(sizes_by_count.items()):
        print(f"  size={sz:>6d} : {cnt} entries")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--list", action="store_true",
                   help="list both BG.DAT and TAI.DAT entries")
    g.add_argument("--decode-bg", type=int, metavar="IDX",
                   help="decode BG[IDX] as RLE-encoded image")
    g.add_argument("--tai-summary", action="store_true",
                   help="size histogram for TAI.DAT")
    p.add_argument("--out-dir", type=Path,
                   help="directory to write PGM artifacts (with --decode-bg)")
    args = p.parse_args()
    if args.list:
        return cmd_list()
    if args.decode_bg is not None:
        return cmd_decode_bg(args.decode_bg, args.out_dir)
    if args.tai_summary:
        return cmd_tai_summary()
    return 0


if __name__ == "__main__":
    sys.exit(main())
