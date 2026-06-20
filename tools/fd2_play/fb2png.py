#!/usr/bin/env python3
"""fb2png.py -- render FD2 replay framebuffer dumps to viewable PNGs.

Each checkpoint dump is FBnn.BIN (64000 mode-13h palette indices, 320x200) plus
PALnn.BIN (256 DAC entries x RGB, 6-bit). This pairs them into PNGnn.png so the
screen at each checkpoint can actually be seen (for authoring/verifying scenarios
and eyeballing regressions). Self-contained PNG writer (stdlib zlib only; no PIL).

Usage:
  python tools/fd2_play/fb2png.py --scenario boot                 # run dir = boot
  python tools/fd2_play/fb2png.py --scenario boot --out boot_a
  python tools/fd2_play/fb2png.py --dir <some_dir>                 # explicit dir
Renders every FBnn.BIN that has a matching PALnn.BIN in the dir.
"""
import argparse
import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUN_BASE = ROOT / "workspace" / "fd2_play" / "run"
W, H = 320, 200


def _chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def write_png(path, rgb, w, h):
    raw = bytearray()
    for y in range(h):
        raw.append(0)                       # filter type 0 (none)
        raw += rgb[y * w * 3:(y + 1) * w * 3]
    png = b"\x89PNG\r\n\x1a\n"
    png += _chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += _chunk(b"IEND", b"")
    Path(path).write_bytes(png)


def render(fb_path, pal_path, out_path):
    fb = fb_path.read_bytes()
    pal = pal_path.read_bytes()
    # 6-bit DAC -> 8-bit
    lut = bytes(min(255, (v * 255 + 31) // 63) for v in pal[:768])
    rgb = bytearray(W * H * 3)
    for i in range(min(len(fb), W * H)):
        c = fb[i] * 3
        o = i * 3
        rgb[o] = lut[c]
        rgb[o + 1] = lut[c + 1]
        rgb[o + 2] = lut[c + 2]
    write_png(out_path, rgb, W, H)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--dir", default="")
    args = ap.parse_args()

    if args.dir:
        d = Path(args.dir)
    elif args.scenario:
        d = RUN_BASE / (args.out or args.scenario)
    else:
        raise SystemExit("need --scenario or --dir")
    if not d.is_dir():
        raise SystemExit("dir not found: %s" % d)

    made = []
    for fb in sorted(d.glob("FB*.BIN")):
        m = re.match(r"FB(\d\d)\.BIN", fb.name, re.I)
        if not m:
            continue
        pal = d / ("PAL%s.BIN" % m.group(1))
        if not pal.is_file():
            print("  skip %s (no palette)" % fb.name)
            continue
        out = d / ("PNG%s.png" % m.group(1))
        render(fb, pal, out)
        made.append(out.name)
    print("rendered %d PNG(s) in %s: %s" % (len(made), d, ", ".join(made)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
