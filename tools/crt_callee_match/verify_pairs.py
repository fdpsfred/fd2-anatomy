"""Byte-level FD2 ↔ Watcom CRT lib comparison with FIXUPP-aware wildcards.

For each (FD2 address range, lib .obj function) pair, compares bytes with the
lib's FIXUPP positions wildcarded — pass means non-reference bytes are
identical.

Inputs (JSON files):
  --pairs       List of pairs to verify. Each item:
                  {label, kind, fd2_start (hex), fd2_len, lib_obj, lib_func,
                   notes (optional)}
                kind is "C" (whole-function) or "A" (parent + tail fragment),
                free-form text used for grouping in the report.
  --lib-bytes   One or more JSON outputs from `extract_obj_bytes.py`.
                Multiple files are merged (later files override earlier).
  --fd2-bytes   JSON with FD2 byte ranges. Format: {addr_hex8: {len, hex}}.

Use `extract_obj_bytes.py` to produce lib-bytes JSON; produce fd2-bytes JSON
via a Ghidra script that reads `Memory.getBytes()` for each address range.
"""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path


def compare_with_mask(fd2_hex: str, lib_hex: str, fixups: list) -> tuple[int, int, list[str]]:
    """Return (mismatches_count, total_bytes, mismatch_samples)."""
    fd2_b = bytes.fromhex(fd2_hex)
    lib_b = bytes.fromhex(lib_hex)
    if len(fd2_b) != len(lib_b):
        return -1, max(len(fd2_b), len(lib_b)), [f"LEN MISMATCH fd2={len(fd2_b)} lib={len(lib_b)}"]
    mask = bytearray(b"\x00" * len(lib_b))
    for pos, w in fixups:
        for k in range(pos, min(pos + w, len(mask))):
            mask[k] = 1
    mismatches: list[str] = []
    nm = 0
    for i in range(len(lib_b)):
        if mask[i]:
            continue
        if fd2_b[i] != lib_b[i]:
            nm += 1
            if len(mismatches) < 5:
                mismatches.append(f"  pos {i}: fd2={fd2_b[i]:02x} lib={lib_b[i]:02x}")
    return nm, len(lib_b), mismatches


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pairs", type=Path, required=True)
    ap.add_argument("--lib-bytes", type=Path, action="append", required=True,
                    help="JSON from extract_obj_bytes.py; pass multiple times to merge.")
    ap.add_argument("--fd2-bytes", type=Path, required=True)
    args = ap.parse_args()

    lib: dict = {}
    for fp in args.lib_bytes:
        lib.update(json.loads(fp.read_text()))
    fd2 = json.loads(args.fd2_bytes.read_text())
    pairs = json.loads(args.pairs.read_text())

    print(f"{'Label':32s} {'Kind':4s} {'Size':>5s}  {'Match':>6s}  Note")
    print("-" * 100)
    for p in pairs:
        label = p["label"]
        kind = p.get("kind", "")
        fd2_start = int(p["fd2_start"], 16) if isinstance(p["fd2_start"], str) else p["fd2_start"]
        fd2_len = p["fd2_len"]
        lib_obj = p["lib_obj"]
        lib_func = p["lib_func"]
        notes = p.get("notes", "")

        if lib_obj not in lib:
            print(f"{label:32s} {kind:4s} {fd2_len:>5d}  ??  lib_obj missing: {lib_obj}")
            continue
        funcs = lib[lib_obj]["by_func"]
        if lib_func not in funcs:
            print(f"{label:32s} {kind:4s} {fd2_len:>5d}  ??  lib_func missing: {lib_func} in {lib_obj}")
            continue
        lib_data = funcs[lib_func]
        if lib_data["size"] != fd2_len:
            print(f"{label:32s} {kind:4s} {fd2_len:>5d}  SIZE  lib size={lib_data['size']} != fd2 expected {fd2_len}")
            continue

        addr_key = f"{fd2_start:08x}"
        if addr_key not in fd2:
            print(f"{label:32s} {kind:4s} {fd2_len:>5d}  ??  fd2 bytes missing for {addr_key}")
            continue
        fd2_hex = fd2[addr_key]["hex"]
        if len(fd2_hex) != 2 * fd2_len:
            print(f"{label:32s} {kind:4s} {fd2_len:>5d}  HEX  fd2 hex len {len(fd2_hex)//2} != expected {fd2_len}")
            continue

        nm, total, samples = compare_with_mask(fd2_hex, lib_data["hex"], lib_data["fixups"])
        verdict = "PASS" if nm == 0 else f"FAIL({nm})"
        print(f"{label:32s} {kind:4s} {fd2_len:>5d}  {verdict:>6s}  {notes}")
        for s in samples:
            print(s)
    return 0


if __name__ == "__main__":
    sys.exit(main())
