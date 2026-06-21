#!/usr/bin/env python3
"""hash_check.py -- gate a freshly built FD2.EXE against the src_refine baseline.

The whole refine task changes only names + comments, so the production FD2.EXE MUST
stay byte-identical to the baseline (commit f83ec3c). This is the strongest "no logic
changed" check. build_fd2.py does not print a hash, so this is the bolt-on comparator
(it does NOT touch build_fd2.py).

Worktree-safe: ROOT is derived from this script's own location, so running the copy
inside a worktree compares that worktree's build against the tracked baseline.

Usage : python tools/src_refine/hash_check.py [path-to-FD2.EXE]
Default exe: <root>/workspace/fd2_build/exe/out/FD2.EXE
Exit  : 0 + PASS iff sha256 == baseline_hash.txt, else 1 + FAIL.
"""
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "tools" / "src_refine" / "data" / "baseline_hash.txt"
DEFAULT_EXE = ROOT / "workspace" / "fd2_build" / "exe" / "out" / "FD2.EXE"


def sha256(path):
    h = hashlib.sha256()
    h.update(Path(path).read_bytes())
    return h.hexdigest()


def main():
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_EXE
    base = BASELINE.read_text(encoding="utf-8").split()[0].strip()
    if not exe.is_file():
        print("FAIL: exe not found: %s" % exe)
        return 1
    got = sha256(exe)
    ok = (got == base)
    print(("PASS" if ok else "FAIL") + " byte-identical-to-baseline")
    print("  exe : %s (%d bytes)" % (exe, exe.stat().st_size))
    print("  got : %s" % got)
    print("  base: %s" % base)
    if not ok:
        print("  -> production binary CHANGED. A name/comment edit touched code, or a")
        print("     rename broke compilation/layout. Bisect the batch's commits.")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
