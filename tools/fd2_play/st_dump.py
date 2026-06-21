#!/usr/bin/env python3
"""st_dump.py -- decode an FD2 replay STnn.BIN capture into readable fields.

The capture format (tests/play/capture.c): 16 little-endian int32 key globals,
then party_count x 0x50-byte runtime_char records. This tool prints the header
and a per-unit table (offsets from src/include/types.h runtime_char).

Usage:
  python tools/fd2_play/st_dump.py <path/to/STnn.BIN> [--raw IDX]
"""
import argparse
import struct
import sys
from pathlib import Path

HDR_WORDS = 16
HDR_NAMES = [
    "chapter_id", "event_flag", "turn", "party_count", "gold",
    "cur_wx", "cur_wy", "cur_sx", "cur_sy", "view_ox", "view_oy",
    "rng_seed", "ai_phys", "ai_spell", "ai_item", "reserved",
]
REC = 0x50


def parse(buf):
    hdr = list(struct.unpack_from("<16i", buf, 0))
    n = hdr[3]
    chars = []
    base = HDR_WORDS * 4
    for i in range(n):
        off = base + i * REC
        if off + REC > len(buf):
            break
        chars.append(buf[off:off + REC])
    return hdr, chars


def u8(rec, o):
    return rec[o]


def u16(rec, o):
    return struct.unpack_from("<H", rec, o)[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--raw", type=int, default=-1,
                    help="hex-dump the raw 0x50 bytes of unit IDX")
    args = ap.parse_args()

    buf = Path(args.path).read_bytes()
    hdr, chars = parse(buf)

    print("== header ==")
    for name, val in zip(HDR_NAMES, hdr):
        extra = ""
        if name == "rng_seed":
            extra = "  (0x%04X)" % (val & 0xFFFF)
        print("  %-11s %d%s" % (name, val, extra))

    print("== units (%d) ==" % len(chars))
    print("  idx  x  y team port  cid job lvl   hp/  max   mp/  max"
          "    ap   dp  hit evade  slp dead")
    for i, rec in enumerate(chars):
        dead = rec[5] & 1
        print("  %3d %2d %2d  %d   0x%02X 0x%02X %3d %3d  %4d/%4d %4d/%4d"
              "  %4d %4d %4d %4d   %d   %d" % (
                  i, u8(rec, 0), u8(rec, 1), u8(rec, 6), u8(rec, 7),
                  u8(rec, 8), u8(rec, 0x20), u8(rec, 0x21),
                  u16(rec, 0x40), u16(rec, 0x42), u16(rec, 0x44), u16(rec, 0x46),
                  u16(rec, 0x48), u16(rec, 0x4A), u16(rec, 0x4C), u16(rec, 0x4E),
                  u8(rec, 0x26), dead))

    if 0 <= args.raw < len(chars):
        rec = chars[args.raw]
        print("== raw unit %d ==" % args.raw)
        for r in range(0, REC, 16):
            row = " ".join("%02X" % b for b in rec[r:r + 16])
            print("  +%02X  %s" % (r, row))


if __name__ == "__main__":
    main()
