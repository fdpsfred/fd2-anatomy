"""FD2 rle_blit_sprite decoder — decompresses RLE-encoded image data used by
FDOTHER, FDSHAP, DATO, FIGANI, BG, FDICON archives.

Decode format (per ground_truth/systems/graphics.md):
    Each opcode byte: high 2 bits = mode, low 6 bits = count.
        0b00xxxxxx  literal copy: copy `count` source bytes
        0b01xxxxxx  stretched literal: 1 src byte -> 2 dst pixels, `count` times
        0b10xxxxxx  RLE run: next 1 src byte repeated `count` times
        0b11xxxxxx  skip: emit `count` transparent pixels (filled with 0)

Outputs raw 8bpp indexed pixel bytes; palette decoding is the caller's
responsibility (FDOTHER[0x65] = 256-entry VGA R6G6B6 palette).

Usage:
    from rle_decoder import rle_decode, write_pgm_indexed, palette_to_color_ppm
    pixels = rle_decode(blob, max_pixels=320*200)
    write_pgm_indexed(pixels, 320, 200, Path("out.pgm"))

CLI:
    python tools/decoders/rle_decoder.py --self-test
    python tools/decoders/rle_decoder.py --decode-fdother 0x36 --width 320 --height 200 \
        --out out.pgm
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# tools/decoders/rle_decoder.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[4]
FDOTHER_PATH = REPO_ROOT / "FDOTHER.DAT"


def rle_decode(src: bytes, max_pixels: int) -> bytes:
    """Decode rle_blit_sprite payload to raw 8bpp pixels (length up to max_pixels).

    Stops at end of source or when max_pixels reached. Truncated data simply
    yields fewer pixels.
    """
    out = bytearray()
    i = 0
    while i < len(src) and len(out) < max_pixels:
        op = src[i]
        i += 1
        mode = (op >> 6) & 0x3
        count = op & 0x3F
        if count == 0:
            count = 64  # observed convention: 0 -> full 64-pixel run
        if mode == 0:
            for _ in range(count):
                if i >= len(src) or len(out) >= max_pixels:
                    break
                out.append(src[i])
                i += 1
        elif mode == 1:
            for _ in range(count):
                if i >= len(src) or len(out) + 2 > max_pixels:
                    break
                b = src[i]
                i += 1
                out.append(b)
                out.append(b)
        elif mode == 2:
            if i >= len(src):
                break
            b = src[i]
            i += 1
            for _ in range(count):
                if len(out) >= max_pixels:
                    break
                out.append(b)
        elif mode == 3:
            for _ in range(count):
                if len(out) >= max_pixels:
                    break
                out.append(0)
    return bytes(out)


def write_pgm_indexed(pixels: bytes, width: int, height: int, out_path: Path) -> None:
    """Write raw indexed pixels as a PGM P5 (8-bit grayscale; palette undecoded)."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    n = width * height
    if len(pixels) < n:
        pixels = pixels + bytes(n - len(pixels))
    with open(out_path, "wb") as f:
        f.write(f"P5\n{width} {height}\n255\n".encode("ascii"))
        f.write(pixels[:n])


def palette_to_color_ppm(palette_bytes: bytes, out_path: Path) -> None:
    """Render 256-entry VGA palette (R6G6B6 DAC) as a 16x16 colour PPM swatch."""
    cell = 16
    grid = 16
    w = grid * cell
    h = grid * cell
    pixels = bytearray(w * h * 3)
    for i in range(256):
        if i * 3 + 2 >= len(palette_bytes):
            r = g = b = 0
        else:
            r = palette_bytes[i * 3]
            g = palette_bytes[i * 3 + 1]
            b = palette_bytes[i * 3 + 2]
            r = (r << 2) | (r >> 4)
            g = (g << 2) | (g >> 4)
            b = (b << 2) | (b >> 4)
        col = i % grid
        row = i // grid
        for dy in range(cell):
            for dx in range(cell):
                py = row * cell + dy
                px = col * cell + dx
                offset = (py * w + px) * 3
                pixels[offset] = r
                pixels[offset + 1] = g
                pixels[offset + 2] = b
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(f"P6\n{w} {h}\n255\n".encode("ascii"))
        f.write(bytes(pixels))


def _read_dat_entry(path: Path, idx: int) -> bytes:
    """Local copy of LLLLLL DAT extractor so this script is self-contained."""
    raw = path.read_bytes()
    first_off = struct.unpack_from("<I", raw, 6)[0]
    n = (first_off - 6) // 4
    if idx >= n - 1:
        raise ValueError(f"idx {idx} out of range (n={n})")
    start = struct.unpack_from("<I", raw, 6 + idx * 4)[0]
    end = struct.unpack_from("<I", raw, 6 + (idx + 1) * 4)[0]
    return raw[start:end]


def _self_test() -> int:
    """Quick sanity test on FDOTHER[0x65] palette + a simple round trip."""
    # synthetic round trip: 64 zeros via skip op
    skipped = rle_decode(bytes([0xC0 | 64 & 0x3F]), max_pixels=128)
    assert all(b == 0 for b in skipped), "skip mode failed"
    # literal copy of one byte
    lit = rle_decode(bytes([0x01, 0x42]), max_pixels=128)
    assert lit == b"\x42", f"literal mode failed: {lit!r}"
    # RLE run of 5 0xAB bytes
    run = rle_decode(bytes([0x80 | 5, 0xAB]), max_pixels=128)
    assert run == b"\xAB" * 5, f"rle mode failed: {run!r}"
    # stretched literal: 3 src bytes -> 6 dst
    stretch = rle_decode(bytes([0x40 | 3, 0x10, 0x20, 0x30]), max_pixels=128)
    assert stretch == b"\x10\x10\x20\x20\x30\x30", f"stretch mode failed: {stretch!r}"
    print("rle_decode self-test: PASS (skip/literal/rle/stretch)")
    if FDOTHER_PATH.exists():
        pal = _read_dat_entry(FDOTHER_PATH, 0x65)
        print(f"FDOTHER[0x65] palette: {len(pal)} bytes "
              f"({'PASS' if len(pal) == 768 else 'unexpected'})")
    else:
        print(f"FDOTHER.DAT not at {FDOTHER_PATH}; skipping live palette read")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--self-test", action="store_true",
                   help="run synthetic round-trip checks for the 4 RLE modes")
    g.add_argument("--decode-fdother", type=lambda s: int(s, 0),
                   metavar="IDX",
                   help="decode FDOTHER[IDX] as RLE pixels and write a PGM")
    p.add_argument("--width", type=int, default=320)
    p.add_argument("--height", type=int, default=200)
    p.add_argument("--out", type=Path, default=Path("rle_decoded.pgm"))
    args = p.parse_args()

    if args.self_test:
        return _self_test()
    blob = _read_dat_entry(FDOTHER_PATH, args.decode_fdother)
    pixels = rle_decode(blob, max_pixels=args.width * args.height)
    write_pgm_indexed(pixels, args.width, args.height, args.out)
    print(f"wrote {args.out} ({args.width}x{args.height}, "
          f"{len(pixels)} decoded pixels)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
