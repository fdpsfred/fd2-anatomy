"""Generic FD2 DAT archive header parser — reads the LLLLLL container format
shared by FDFIELD/FDTXT/FDSHAP/FDOTHER/DATO/FDMUS/FIGANI/BG/TAI/ANI/TITLE.

DAT layout (verified across all 11 archives):

    +0x00     6 bytes      signature = b"LLLLLL"  (0x4C * 6)
    +0x06     N x u32 LE   offset[]               (last is sentinel = file size)
    +0x06+N*4 payload bytes                        (concatenated entries)

Reading entry idx i:
    start = offset[i]
    end   = offset[i+1]
    size  = end - start

entry_count = N - 1.

Usage:
    from dat_header_parser import read_dat_entry, parse_dat_header
    blob = read_dat_entry(Path("FDFIELD.DAT"), idx=1)
    header = parse_dat_header(Path("FDOTHER.DAT"))

CLI:
    python tools/decoders/dat_header_parser.py --list                # all DATs
    python tools/decoders/dat_header_parser.py --file FDOTHER.DAT    # one file
    python tools/decoders/dat_header_parser.py --file FDFIELD.DAT --entries
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# tools/decoders/dat_header_parser.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[4]

SIGNATURE_LEN = 6
EXPECTED_SIG = b"LLLLLL"

DAT_FILES = [
    "FDTXT.DAT",
    "FDOTHER.DAT",
    "FDFIELD.DAT",
    "FDSHAP.DAT",
    "DATO.DAT",
    "FDMUS.DAT",
    "FIGANI.DAT",
    "BG.DAT",
    "TAI.DAT",
    "TITLE.DAT",
    "ANI.DAT",
]


def parse_dat_header(path: Path) -> dict:
    """Parse signature + offset table; return descriptor with per-entry layout."""
    raw = path.read_bytes()
    file_size = len(raw)
    if file_size < SIGNATURE_LEN + 4:
        raise ValueError(f"{path} too small ({file_size} bytes)")
    signature = raw[:SIGNATURE_LEN]
    if signature != EXPECTED_SIG:
        raise ValueError(
            f"{path} unexpected signature {signature!r} (expected {EXPECTED_SIG!r})"
        )
    first_offset = struct.unpack_from("<I", raw, SIGNATURE_LEN)[0]
    if (first_offset - SIGNATURE_LEN) % 4 != 0:
        raise ValueError(
            f"{path} first offset 0x{first_offset:X} - 6 not multiple of 4"
        )
    if first_offset >= file_size:
        raise ValueError(
            f"{path} first offset 0x{first_offset:X} >= file size 0x{file_size:X}"
        )
    n_u32 = (first_offset - SIGNATURE_LEN) // 4
    offsets = list(struct.unpack_from(f"<{n_u32}I", raw, SIGNATURE_LEN))
    for i in range(1, n_u32):
        if offsets[i] < offsets[i - 1]:
            raise ValueError(
                f"{path} non-monotonic offset at idx {i}: "
                f"0x{offsets[i]:X} < 0x{offsets[i-1]:X}"
            )
    if offsets[-1] > file_size:
        raise ValueError(
            f"{path} sentinel 0x{offsets[-1]:X} exceeds file size 0x{file_size:X}"
        )
    entry_count = n_u32 - 1
    entries = []
    for i in range(entry_count):
        start = offsets[i]
        end = offsets[i + 1]
        size = end - start
        preview = raw[start:start + min(size, 16)]
        entries.append({
            "idx": i,
            "start": start,
            "end": end,
            "size": size,
            "first16_hex": preview.hex().upper(),
        })
    sizes = [e["size"] for e in entries]
    return {
        "file": path.name,
        "file_size": file_size,
        "header_size": SIGNATURE_LEN + n_u32 * 4,
        "n_offsets_u32": n_u32,
        "entry_count": entry_count,
        "trailing_bytes": file_size - offsets[-1],
        "size_min": min(sizes) if sizes else 0,
        "size_max": max(sizes) if sizes else 0,
        "size_total": sum(sizes),
        "entries": entries,
    }


def read_dat_entry(path: Path, idx: int) -> bytes:
    """Extract entry idx as raw bytes (no copying of full file metadata)."""
    raw = path.read_bytes()
    if raw[:SIGNATURE_LEN] != EXPECTED_SIG:
        raise ValueError(f"{path}: signature mismatch")
    first_off = struct.unpack_from("<I", raw, SIGNATURE_LEN)[0]
    n = (first_off - SIGNATURE_LEN) // 4
    if idx >= n - 1:
        raise ValueError(f"idx {idx} out of range (entry_count={n - 1})")
    start = struct.unpack_from("<I", raw, SIGNATURE_LEN + idx * 4)[0]
    end = struct.unpack_from("<I", raw, SIGNATURE_LEN + (idx + 1) * 4)[0]
    return raw[start:end]


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--list", action="store_true",
                   help="parse every known DAT under the repo root and print a summary")
    g.add_argument("--file", type=str,
                   help="parse one specific DAT file (relative to repo root or absolute)")
    p.add_argument("--entries", action="store_true",
                   help="dump per-entry start/size/first16 for the chosen file")
    p.add_argument("--idx", type=int,
                   help="with --file, hex-dump the first 64 bytes of entry idx")
    args = p.parse_args()

    if args.list:
        print(f"# Scanning {len(DAT_FILES)} DAT files under {REPO_ROOT}")
        print(f"{'file':14s} {'size':>10s} {'header':>8s} {'N(u32)':>8s} "
              f"{'entries':>8s} {'min':>7s} {'max':>8s}")
        for fname in DAT_FILES:
            path = REPO_ROOT / fname
            if not path.exists():
                print(f"{fname:14s} (missing)")
                continue
            try:
                hdr = parse_dat_header(path)
            except ValueError as e:
                print(f"{fname:14s} ERROR: {e}")
                continue
            print(f"{fname:14s} {hdr['file_size']:>10d} "
                  f"{hdr['header_size']:>8d} {hdr['n_offsets_u32']:>8d} "
                  f"{hdr['entry_count']:>8d} {hdr['size_min']:>7d} "
                  f"{hdr['size_max']:>8d}")
        return 0

    target = Path(args.file)
    if not target.is_absolute():
        target = REPO_ROOT / target
    hdr = parse_dat_header(target)
    print(f"file        : {hdr['file']}")
    print(f"file_size   : {hdr['file_size']} (0x{hdr['file_size']:X})")
    print(f"header_size : {hdr['header_size']}")
    print(f"n_offsets   : {hdr['n_offsets_u32']}")
    print(f"entry_count : {hdr['entry_count']}")
    print(f"trailing    : {hdr['trailing_bytes']}")
    print(f"size_min/max: {hdr['size_min']} / {hdr['size_max']}")
    if args.entries:
        print()
        print(f"{'idx':>4} {'start':>10} {'end':>10} {'size':>8} first16")
        for e in hdr["entries"]:
            print(f"{e['idx']:>4} {e['start']:>10} {e['end']:>10} "
                  f"{e['size']:>8} {e['first16_hex']}")
    if args.idx is not None:
        blob = read_dat_entry(target, args.idx)
        print()
        print(f"entry {args.idx}: size {len(blob)}")
        for off in range(0, min(len(blob), 64), 16):
            chunk = blob[off:off + 16]
            hex_part = " ".join(f"{b:02X}" for b in chunk)
            asc_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
            print(f"  {off:04X}: {hex_part:<48s}  {asc_part}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
