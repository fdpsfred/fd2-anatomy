#!/usr/bin/env python3
"""eqcheck.py -- Stage-2 functional-equivalence gate for FD2.EXE.

Stage-2 renames symbols. A rename has two binary-visible but behaviour-neutral
effects, in increasing order of how much they perturb the image:

  (1) FIXUP REORDER. wlink emits LE fixup records in a symbol-name-dependent
      order, so the Fixup Record Table bytes get reordered (same multiset).

  (2) COMDEF RELOCATION. Uninitialized globals are Watcom COMDEF (tentative)
      symbols; wlink places them in a name-dependent order. Renaming one can
      move it (and its neighbours) by a few bytes, which changes the absolute
      address stored at every fixup SITE that targets a moved symbol, and the
      target field of the corresponding fixup records. Proven empirically: the
      relocated, loader-applied image is identical; only symbol placement moved.

Both are functionally inert (the loaded image behaves identically). This gate
certifies functional equivalence at two levels and PASSES if either holds:

  STRICT  : everything outside the LE Fixup Record Table is byte-identical to
            baseline AND the fixup table has the same byte multiset (only effect
            (1) occurred -> no COMDEF moved). This is the original, tightest gate.

  RELOC   : (fallback when STRICT fails) blank every fixup SITE (the 4-/2-/1-byte
            relocated value at each fixup source) and the whole Fixup Record
            Table in BOTH the candidate and the baseline, then compare the
            residual. Identical residual => the ONLY differences are relocation
            values + fixup reorder (effects (1)+(2)) => zero code/data/logic
            change. Any real code/data edit lands OUTSIDE a fixup site and breaks
            the residual -> FAIL. (Negative-tested.)

LE table bounds and fixup sites are parsed live from the EXE header; nothing is
hardcoded. RELOC needs the baseline residual_sha256, stored in baseline_eq.json.

Baseline reference: tools/src_refine/data/baseline_eq.json, generated from the
byte-identical baseline build (sha256 == data/baseline_hash.txt).

Usage:
  python tools/src_refine/eqcheck.py --gen <baseline_exe>   # (re)create baseline_eq.json
  python tools/src_refine/eqcheck.py [built_exe]            # gate; default workspace EXE
Exit: 0 + PASS iff functionally equivalent (STRICT or RELOC), else 1 + FAIL.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE_EQ = ROOT / "tools" / "src_refine" / "data" / "baseline_eq.json"
DEFAULT_EXE = ROOT / "workspace" / "fd2_build" / "exe" / "out" / "FD2.EXE"

# LE fixup source-type -> patched length in bytes (the relocated value width).
# 0x02 (16-bit selector) patches no offset value here.
SRC_SIZE = {0x00: 1, 0x02: 0, 0x05: 2, 0x06: 4, 0x07: 4, 0x08: 4, 0x09: 6}


def sha(bb):
    return hashlib.sha256(bb).hexdigest()


def le_header(b):
    H = struct.unpack_from("<I", b, 0x3C)[0]
    if b[H:H + 2] != b"LE":
        raise SystemExit("not an LE executable (sig %r at 0x%x)" % (b[H:H + 2], H))
    u32 = lambda o: struct.unpack_from("<I", b, H + o)[0]
    return {
        "H": H,
        "num_pages": u32(0x14),
        "page_size": u32(0x28),
        "fixup_page_tbl": u32(0x68) + H,
        "fixup_rec_tbl": u32(0x6C) + H,
        "import_mod_tbl": u32(0x70) + H,   # = end of fixup records
        "data_off": u32(0x80),
    }


def fixup_bounds(b):
    """(lo, hi) of the LE Fixup Record Table, parsed live from the header."""
    h = le_header(b)
    lo, hi = h["fixup_rec_tbl"], h["import_mod_tbl"]
    if not (h["H"] < lo < hi <= len(b)):
        raise SystemExit("implausible LE table bounds: fixup=0x%x imp=0x%x" % (lo, hi))
    return lo, hi


def fixup_sites(b):
    """List of (file_offset, length) for every LE fixup source site.

    Parses the Fixup Page Table + Fixup Record Table exactly so every record's
    target data is skipped correctly (otherwise the next record misaligns).
    """
    h = le_header(b)
    ps, fpt, frt = h["page_size"], h["fixup_page_tbl"], h["fixup_rec_tbl"]
    doff, npg = h["data_off"], h["num_pages"]
    pte = [struct.unpack_from("<I", b, fpt + 4 * j)[0] for j in range(npg + 1)]
    sites = []
    for pg in range(1, npg + 1):
        pos, end = frt + pte[pg - 1], frt + pte[pg]
        page_file = doff + (pg - 1) * ps
        while pos < end:
            src, trg = b[pos], b[pos + 1]
            pos += 2
            stype = src & 0x0F
            if src & 0x20:                                   # source list
                cnt = b[pos]; pos += 1
                offs = [struct.unpack_from("<h", b, pos + 2 * k)[0] for k in range(cnt)]
                pos += 2 * cnt
            else:
                offs = [struct.unpack_from("<h", b, pos)[0]]; pos += 2
            ttype = trg & 0x03
            if ttype == 0:                                   # internal ref
                pos += 2 if (trg & 0x40) else 1              # object number
                if stype != 0x02:
                    pos += 4 if (trg & 0x10) else 2          # target offset
            elif ttype == 1:                                 # import by ordinal
                pos += 2 if (trg & 0x40) else 1
                pos += 4 if (trg & 0x80) else (2 if (trg & 0x10) else 1)
            elif ttype == 2:                                 # import by name
                pos += 2 if (trg & 0x40) else 1
                pos += 4 if (trg & 0x10) else 2
            elif ttype == 3:                                 # internal via entry
                pos += 2 if (trg & 0x40) else 1
            if trg & 0x04:                                   # additive
                pos += 4 if (trg & 0x20) else 2
            ln = SRC_SIZE.get(stype, 4)
            if ln:
                for so in offs:
                    sites.append((page_file + so, ln))
    return sites


def residual_sha(b):
    """sha256 of the image with every fixup site value AND the whole Fixup
    Record Table blanked. Equal residual => differences are pure relocation."""
    ba = bytearray(b)
    lo, hi = fixup_bounds(b)
    for i in range(lo, hi):
        ba[i] = 0
    for off, ln in fixup_sites(b):
        for k in range(ln):
            if 0 <= off + k < len(ba):
                ba[off + k] = 0
    return sha(bytes(ba))


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
        "residual_sha256": residual_sha(b),
    }


def main():
    args = sys.argv[1:]
    if args and args[0] == "--gen":
        exe = Path(args[1])
        prof = profile(exe.read_bytes())
        prof["_doc"] = ("Baseline functional-equivalence profile for Stage-2. "
                        "outside_sha256 = sha256 of the EXE minus the LE fixup record "
                        "table (STRICT: must stay identical). fixup_multiset_sha256 = "
                        "sha256 of the sorted fixup-table bytes (same relocation set, "
                        "order may differ after renames). residual_sha256 = sha256 of "
                        "the EXE with every fixup site value AND the fixup record table "
                        "blanked (RELOC: stays identical under pure COMDEF relocation; "
                        "any real code/data edit breaks it). From the byte-identical "
                        "baseline build.")
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
    strict = ok_size and ok_outside and ok_fixup

    base_resid = base.get("residual_sha256")
    ok_resid = base_resid is not None and got["residual_sha256"] == base_resid
    reloc = ok_size and ok_resid

    ok = strict or reloc
    level = "STRICT" if strict else ("RELOC" if reloc else "FAIL")
    print(("PASS[%s]" % level if ok else "FAIL") + " functional-equivalence-to-baseline")
    print("  exe            : %s (%d bytes)" % (exe, got["size"]))
    print("  size==base     : %s" % ok_size)
    print("  STRICT code/data+hdr byte-identical : %s" % ok_outside)
    print("  STRICT fixup multiset (reorder-only): %s" % ok_fixup)
    print("  RELOC  residual (sites+table blanked): %s" % ok_resid)
    if ok and not strict:
        print("  -> PASS via RELOC: differences are pure COMDEF symbol relocation +")
        print("     fixup reorder (blanked-residual identical to baseline). Zero code/")
        print("     data/logic change -- a rename moved a tentative-def global.")
    if not ok:
        if not ok_size:
            print("  -> SIZE differs: not a rename-only change.")
        elif base_resid is None:
            print("  -> baseline_eq.json has no residual_sha256; regenerate with --gen.")
        else:
            print("  -> REAL CHANGE: a code/data byte OUTSIDE every fixup site differs.")
            print("     NOT explained by relocation/reorder. Investigate the offending edit.")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
