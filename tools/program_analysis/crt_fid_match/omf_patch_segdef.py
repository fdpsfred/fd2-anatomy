"""Patch Watcom Easy OMF-386 quirk: 16-bit-typed OMF records (0x98 SEGDEF /
0x90 PUBDEF / 0xA0 LEDATA / 0xA2 LIDATA / 0x9C FIXUPP) that nonetheless carry
32-bit length / offset fields. wlib / wlink accept this extension; Ghidra's
stock OmfLoader does not — it parses the records assuming the standard 16-bit
field widths, mis-aligns the rest of the record, and either fails (`Invalid
block name`, EOF) or registers ghost symbols that collide downstream
(`Duplicate key OmfSymbol`).

Fix: change each affected record's type byte to its 32-bit counterpart
(0x99 / 0x91 / 0xA1 / 0xA3 / 0x9D). Length / offset bytes already in 32-bit
form are then read correctly. ACBP / fixup-target / segment-USE attributes
are untouched, so 16-bit segments stay 16-bit semantically.

Detection: a single .obj is "Watcom-quirky" when any 0x98 SEGDEF in it has
content[3] == 0 (zero where standard 2-byte-length encoding requires
name_idx >= 1). Within such a file, the patcher then upgrades every 16/32
record-type pair en bloc, since Watcom emits the whole record set in the
matching width.

Usage:
    python tools/program_analysis/crt_fid_match/omf_patch_segdef.py --in-dir <dir> --out-dir <dir>
    python tools/program_analysis/crt_fid_match/omf_patch_segdef.py --in-dir <dir> --in-place
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

REC_THEADR = 0x80
REC_LHEADR = 0x82
REC_MODEND_16 = 0x8A
REC_MODEND_32 = 0x8B
REC_PUBDEF = 0x90
REC_PUBDEF32 = 0x91
REC_LINNUM = 0x94
REC_LINNUM32 = 0x95
REC_FIXUPP = 0x9C
REC_FIXUPP32 = 0x9D
REC_SEGDEF = 0x98
REC_SEGDEF32 = 0x99
REC_LEDATA = 0xA0
REC_LEDATA32 = 0xA1
REC_LIDATA = 0xA2
REC_LIDATA32 = 0xA3

# Pairs of (16-bit type → 32-bit type) that we patch when a file is detected
# as Watcom Easy OMF-386 quirky. Excludes record types whose 16-bit form would
# be valid in the same file (we only patch quirky .obj wholesale).
PATCH_PAIRS: dict[int, int] = {
    REC_SEGDEF:    REC_SEGDEF32,
    REC_PUBDEF:    REC_PUBDEF32,
    REC_LEDATA:    REC_LEDATA32,
    REC_LIDATA:    REC_LIDATA32,
    REC_FIXUPP:    REC_FIXUPP32,
    REC_LINNUM:    REC_LINNUM32,
    REC_MODEND_16: REC_MODEND_32,
}


def is_quirky(data: bytes) -> bool:
    """Detect Watcom Easy OMF-386 quirk: a 0x98 SEGDEF with content[3] == 0
    (which under standard 2-byte-length encoding would be the segment's
    name_idx — required to be >= 1 by the OMF spec)."""
    off = 0
    while off + 3 <= len(data):
        rt = data[off]
        rl = data[off + 1] | (data[off + 2] << 8)
        if off + 3 + rl > len(data):
            return False
        if rt == REC_SEGDEF and rl >= 9:
            if data[off + 3 + 3] == 0:
                return True
        off += 3 + rl
        if rt in (REC_MODEND_16, REC_MODEND_32):
            break
    return False


def patch_obj(data: bytes) -> tuple[bytes, dict[int, int]]:
    """Walk OMF records; if file is Watcom-quirky, upgrade all 16-bit-type
    records to their 32-bit counterparts and recompute checksum.

    Returns (patched_bytes, counts_per_record_type).
    """
    if not is_quirky(data):
        return data, {}

    out = bytearray(data)
    off = 0
    counts: dict[int, int] = {}
    while off + 3 <= len(out):
        rt = out[off]
        rl = out[off + 1] | (out[off + 2] << 8)
        rec_total = 3 + rl
        if off + rec_total > len(out):
            break

        if rt in PATCH_PAIRS:
            # MODEND-16 → MODEND-32 only safe when no start address (S-bit
            # clear); otherwise the offset field width differs (2 vs 4 bytes)
            # and a naive type swap would corrupt the record.
            if rt == REC_MODEND_16:
                flags = out[off + 3] if rec_total >= 5 else 0
                has_start = (flags & 0x40) != 0
                if has_start:
                    off += rec_total
                    if rt in (REC_MODEND_16, REC_MODEND_32):
                        break
                    continue

            out[off] = PATCH_PAIRS[rt]
            # OMF checksum: sum of all bytes in the record (incl. type, length,
            # content, checksum) ≡ 0 mod 256.
            checksum_off = off + rec_total - 1
            running = 0
            for i in range(off, checksum_off):
                running = (running + out[i]) & 0xFF
            out[checksum_off] = (256 - running) & 0xFF
            counts[rt] = counts.get(rt, 0) + 1

        off += rec_total
        if rt in (REC_MODEND_16, REC_MODEND_32):
            break
    return bytes(out), counts


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in-dir", required=True, type=Path)
    ap.add_argument("--out-dir", type=Path,
                    help="Destination dir; if omitted with --in-place, edits in place")
    ap.add_argument("--in-place", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if not args.in_dir.is_dir():
        print(f"ERROR: in-dir not found: {args.in_dir}", file=sys.stderr)
        return 2
    if not args.in_place and args.out_dir is None:
        print("ERROR: must supply --out-dir or --in-place", file=sys.stderr)
        return 2

    if args.out_dir is not None:
        args.out_dir.mkdir(parents=True, exist_ok=True)

    total = 0
    patched_files = 0
    grand_counts: dict[int, int] = {}
    for src in sorted(args.in_dir.glob("*.obj")):
        data = src.read_bytes()
        new_data, counts = patch_obj(data)
        total += 1
        if counts:
            patched_files += 1
            for rt, n in counts.items():
                grand_counts[rt] = grand_counts.get(rt, 0) + n
            if not args.quiet:
                desc = "  ".join(f"{k:#04x}:{v}" for k, v in counts.items())
                print(f"  patched {src.name}: {desc}")

        if args.in_place:
            if counts:
                src.write_bytes(new_data)
        else:
            dst = args.out_dir / src.name
            if counts:
                dst.write_bytes(new_data)
            else:
                shutil.copyfile(src, dst)

    print(f"\nTotal .obj: {total}  patched-files: {patched_files}")
    if grand_counts:
        names = {0x98: "SEGDEF", 0x90: "PUBDEF", 0xA0: "LEDATA",
                 0xA2: "LIDATA", 0x9C: "FIXUPP", 0x94: "LINNUM"}
        for rt, n in sorted(grand_counts.items()):
            print(f"  {names.get(rt, hex(rt))}: {n} records upgraded to 32-bit type")
    return 0


if __name__ == "__main__":
    sys.exit(main())
