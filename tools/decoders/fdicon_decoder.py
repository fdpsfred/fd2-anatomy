"""FDICON.B24 decoder — decodes the 24x24 RLE icons stored in FDICON.B24.

FDICON.B24 layout:
    +0x00  u16 LE  width   = 24
    +0x02  u16 LE  height  = 24
    +0x04  u16 LE  count   = 1680 (active entries)
    +0x06  u32 LE x (count + 1)  offset[]   (last = sentinel = file size)
    +0x06 + (count+1)*4           payload start
    payload: `count` x RLE-encoded 24x24 icons

CLI:
    python tools/decoders/fdicon_decoder.py --info
    python tools/decoders/fdicon_decoder.py --samples 0,1,2,3,4,840,1679 --out-dir ./out
    python tools/decoders/fdicon_decoder.py --decode 100 --out-dir ./out
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# tools/decoders/fdicon_decoder.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[4]
FDICON_PATH = REPO_ROOT / "FDICON.B24"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rle_decoder import rle_decode  # noqa: E402


def parse_header(raw: bytes) -> tuple[int, int, int, list[int]]:
    width, height, count = struct.unpack_from("<HHH", raw, 0)
    n_offsets = count + 1
    offsets = list(struct.unpack_from(f"<{n_offsets}I", raw, 6))
    return width, height, count, offsets


def cmd_info() -> int:
    raw = FDICON_PATH.read_bytes()
    w, h, count, offsets = parse_header(raw)
    print(f"FDICON.B24: {len(raw)} bytes; width={w} height={h} count={count}")
    print(f"  offset[0]    = 0x{offsets[0]:X} (payload start)")
    print(f"  offset[1]    = 0x{offsets[1]:X}")
    print(f"  offset[{count}] = 0x{offsets[count]:X} (sentinel; "
          f"matches file size = {offsets[count] == len(raw)})")
    monotonic = all(offsets[i + 1] >= offsets[i] for i in range(count))
    print(f"  monotonic    = {monotonic}")
    sizes = [offsets[i + 1] - offsets[i] for i in range(count)]
    print(f"  per-icon size: min={min(sizes)} max={max(sizes)} "
          f"avg={sum(sizes) // count}")
    return 0


def cmd_decode(idx_list: list[int], out_dir: Path | None) -> int:
    raw = FDICON_PATH.read_bytes()
    w, h, count, offsets = parse_header(raw)
    if out_dir is not None:
        out_dir.mkdir(parents=True, exist_ok=True)

    # Side-by-side atlas of all samples
    if out_dir is not None:
        atlas_w = w * len(idx_list)
        atlas = bytearray(atlas_w * h)

    for i, idx in enumerate(idx_list):
        if idx >= count:
            print(f"  idx {idx}: out of range (count={count})")
            continue
        start = offsets[idx]
        end = offsets[idx + 1]
        compressed = raw[start:end]
        decoded = rle_decode(compressed, max_pixels=w * h)
        if len(decoded) < w * h:
            decoded = decoded + bytes(w * h - len(decoded))
        decoded = decoded[:w * h]
        hist = [0] * 256
        for b in decoded:
            hist[b] += 1
        nonzero = sum(1 for c in hist if c > 0)
        print(f"  icon idx={idx:>4d}  size={end-start:>5d}  "
              f"decoded={len(decoded)}/{w*h}  unique_colors={nonzero}")
        if out_dir is not None:
            for y in range(h):
                for x in range(w):
                    atlas[y * atlas_w + i * w + x] = decoded[y * w + x]
    if out_dir is not None and idx_list:
        out_path = out_dir / f"fdicon_atlas_{len(idx_list)}icons.pgm"
        with open(out_path, "wb") as f:
            f.write(f"P5\n{atlas_w} {h}\n255\n".encode("ascii"))
            f.write(bytes(atlas))
        print(f"\nwrote {out_path}")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--info", action="store_true",
                   help="show file header + per-icon size stats")
    g.add_argument("--decode", type=int, metavar="IDX",
                   help="decode one icon by idx")
    g.add_argument("--samples", type=str,
                   help="decode comma-separated idx list, e.g. '0,1,2,840,1679'")
    p.add_argument("--out-dir", type=Path,
                   help="directory to write atlas PGM")
    args = p.parse_args()
    if args.info:
        return cmd_info()
    if args.decode is not None:
        return cmd_decode([args.decode], args.out_dir)
    indices = [int(s, 0) for s in args.samples.split(",")]
    return cmd_decode(indices, args.out_dir)


if __name__ == "__main__":
    sys.exit(main())
