"""Watcom OMF library archive extractor — thin wrapper around `wlib -q -x`.

Earlier this script was a from-scratch OMF library parser. That worked but was
over-engineering: Watcom's own `wlib` ships with the toolchain, handles every
Easy OMF-386 quirk (most importantly the 4-byte length field on type 0x98
SEGDEF that pure-OMF parsers mis-read), and was verified byte-identical to the
Python parser across 379/379 modules. Replaced with a wlib invocation.

Usage:
    python tools/program_analysis/crt_fid_match/omf_lib_extract.py --lib path/to/CLIB3R.LIB \
        --out workspace/crt_fid_match/extracted/9.5c/CLIB3R

Requires `wlib.exe` (or `wlib`) on PATH. Open Watcom's bin* directory must be
in PATH; the FD2 reverse-engineering setup already adds it.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--lib", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if not args.lib.is_file():
        print(f"ERROR: lib not found: {args.lib}", file=sys.stderr)
        return 2
    if not shutil.which("wlib") and not shutil.which("wlib.exe"):
        print("ERROR: wlib not on PATH (install Open Watcom or add binnt/binnt64)", file=sys.stderr)
        return 2

    args.out.mkdir(parents=True, exist_ok=True)

    # wlib extracts modules into the CWD using the module name (lowercased) as
    # filename. Run it in a fresh temp dir to keep the .obj harvest isolated.
    with tempfile.TemporaryDirectory() as tmp_str:
        tmp = Path(tmp_str)
        # Copy the .lib so wlib doesn't write files next to the original.
        local_lib = tmp / args.lib.name
        shutil.copyfile(args.lib, local_lib)

        # `-q` quiet, `-x` extract all modules (no module-name args = all).
        result = subprocess.run(
            ["wlib", "-q", "-x", local_lib.name],
            cwd=tmp,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(f"ERROR: wlib failed (exit {result.returncode})", file=sys.stderr)
            print(result.stdout, file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            return 3

        local_lib.unlink()  # keep the harvest, drop the lib copy
        objs = sorted(tmp.glob("*.obj"))
        for obj in objs:
            dst = args.out / obj.name
            shutil.move(str(obj), dst)
            if not args.quiet:
                print(f"  {obj.stem:24s}  {dst.stat().st_size:7d}  {dst.name}")

    print(f"Extracted {len(objs)} modules from {args.lib.name} -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
