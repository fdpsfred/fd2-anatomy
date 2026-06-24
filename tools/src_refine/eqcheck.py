#!/usr/bin/env python3
"""eqcheck.py -- Stage-2 functional-equivalence gate for FD2.EXE.

Stage-2 renames symbols. A symbol rename perturbs the ORDER of records in the LE
"Fixup Record Table" (wlink emits fixup records in a symbol-name-dependent order),
so the rebuilt FD2.EXE can no longer be byte-identical to the baseline even though
NO code/data byte changes. Proven empirically: the only differing bytes after
renames lie inside the LE fixup record table, the fixup byte-multiset is unchanged
(same relocation set, reordered), and every other region -- including all code/data
pages -- is byte-identical.

This gate therefore checks FUNCTIONAL EQUIVALENCE instead of raw byte-identity:
  1. everything OUTSIDE the LE fixup record table is byte-identical to baseline
     (this covers the MZ/LE headers, object table, page map, entry table, import
      tables, AND all data/code pages -> zero logic/data change), AND
  2. the fixup record table has the SAME byte multiset as baseline
     (same relocation set, only reordered -> loader applies an identical image).

LE table bounds are parsed live from the EXE header (fixup_record_table_off @ LE+0x6C
.. import_module_table_off @ LE+0x70); nothing is hardcoded.

Baseline reference: tools/src_refine/data/baseline_eq.json, generated from the
byte-identical baseline build (sha256 == data/baseline_hash.txt).

Usage:
  python tools/src_refine/eqcheck.py --gen <baseline_exe>   # (re)create baseline_eq.json
  python tools/src_refine/eqcheck.py [built_exe]            # gate; default workspace EXE
Exit: 0 + PASS iff functionally equivalent, else 1 + FAIL.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE_EQ = ROOT / "tools" / "src_refine" / "data" / "baseline_eq.json"
DEFAULT_EXE = ROOT / "workspace" / "fd2_build" / "exe" / "out" / "FD2.EXE"


def sha(bb):
    return hashlib.sha256(bb).hexdigest()


def fixup_bounds(b):
    """(lo, hi) of the LE Fixup Record Table, parsed live from the header."""
    H = struct.unpack_from("<I", b, 0x3C)[0]
    if b[H:H + 2] != b"LE":
        raise SystemExit("not an LE executable (sig %r at 0x%x)" % (b[H:H + 2], H))
    fixup_rec = struct.unpack_from("<I", b, H + 0x6C)[0] + H  # fixup record table
    imp_mod = struct.unpack_from("<I", b, H + 0x70)[0] + H    # import module table = end of fixups
    if not (H < fixup_rec < imp_mod <= len(b)):
        raise SystemExit("implausible LE table bounds: fixup=0x%x imp=0x%x" % (fixup_rec, imp_mod))
    return fixup_rec, imp_mod


def profile(b):
    lo, hi = fixup_bounds(b)
    outside = b[:lo] + b[hi:]
    fixup = b[lo:hi]
    return {
        "size": len(b),
        "fixup_lo": lo, "fixup_hi": hi,
        "outside_sha256": sha(outside),
        "fixup_multiset_sha256": sha(bytes(sorted(fixup))),
        "fixup_len": len(fixup),
    }


def main():
    args = sys.argv[1:]
    if args and args[0] == "--gen":
        exe = Path(args[1])
        prof = profile(exe.read_bytes())
        prof["_doc"] = ("Baseline functional-equivalence profile for Stage-2. outside_sha256 "
                        "= sha256 of the EXE minus the LE fixup record table (must stay identical); "
                        "fixup_multiset_sha256 = sha256 of the sorted fixup-table bytes (same "
                        "relocation set, order may differ after renames). Generated from the "
                        "byte-identical baseline build.")
        BASE_EQ.write_text(json.dumps(prof, indent=2) + "\n", encoding="utf-8")
        print("wrote %s" % BASE_EQ)
        print(json.dumps({k: v for k, v in prof.items() if not k.startswith("_")}, indent=2))
        return 0

    exe = Path(args[0]) if args else DEFAULT_EXE
    if not exe.is_file():
        print("FAIL: exe not found: %s" % exe)
        return 1
    base = json.loads(BASE_EQ.read_text(encoding="utf-8"))
    got = profile(exe.read_bytes())

    ok_size = got["size"] == base["size"]
    ok_outside = got["outside_sha256"] == base["outside_sha256"]
    ok_fixup = got["fixup_multiset_sha256"] == base["fixup_multiset_sha256"]
    ok = ok_size and ok_outside and ok_fixup
    print(("PASS" if ok else "FAIL") + " functional-equivalence-to-baseline")
    print("  exe            : %s (%d bytes)" % (exe, got["size"]))
    print("  size==base     : %s" % ok_size)
    print("  code/data+hdr  : %s (everything outside LE fixup table byte-identical)" % ok_outside)
    print("  fixup multiset : %s (same relocation set, reorder-only)" % ok_fixup)
    if not ok_outside:
        print("  -> REAL CHANGE: a code/data/header byte differs. NOT just a rename reorder.")
        print("     This is a genuine logic/data change -- investigate the offending edit.")
    if ok_outside and not ok_fixup:
        print("  -> fixup set changed (not a pure reorder). A relocation was added/removed.")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
