#!/usr/bin/env python3
"""rename_global.py -- mechanically apply ONE already-decided global rename
across src/ + tests/ C source, using whole-word boundaries so substrings (e.g.
a local `orig_<name>`) are never corrupted.

This is the apply step only: the caller decides old->new per global after
individual Ghidra inspection (address / type / xrefs / subsystem). It does NOT
derive names. Pair it with a matching Ghidra rename so symbol names stay
byte-identical on both sides.

Usage:
  python tools/data_emit/rename_global.py <old> <new>            # dry-run
  python tools/data_emit/rename_global.py <old> <new> --apply

Whole-word = r"\\b<old>\\b". Reports per-file hit counts; after --apply verifies
the old whole-word is gone (0). UTF-8 reads/writes, preserves newlines.
"""
import io, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DIRS = [os.path.join(ROOT, "src"), os.path.join(ROOT, "tests")]
EXTS = (".c", ".h")


def iter_files():
    for base in DIRS:
        for dp, _, fns in os.walk(base):
            if os.sep + "OUT" in dp or "__pycache__" in dp:
                continue
            for fn in fns:
                if fn.endswith(EXTS):
                    yield os.path.join(dp, fn)


def main():
    args = [a for a in sys.argv[1:] if a != "--apply"]
    apply = "--apply" in sys.argv
    if len(args) != 2:
        print("usage: rename_global.py <old> <new> [--apply]")
        return 2
    old, new = args
    if old == new or not re.fullmatch(r"[A-Za-z_]\w*", old) or not re.fullmatch(r"[A-Za-z_]\w*", new):
        print("bad identifiers"); return 2
    pat = re.compile(r"\b" + re.escape(old) + r"\b")

    total, touched = 0, []
    for path in iter_files():
        txt = io.open(path, encoding="utf-8").read()
        n = len(pat.findall(txt))
        if not n:
            continue
        total += n
        touched.append((os.path.relpath(path, ROOT).replace("\\", "/"), n))
        if apply:
            io.open(path, "w", encoding="utf-8", newline="").write(pat.sub(new, txt))

    for rel, n in sorted(touched):
        print("  %3d  %s" % (n, rel))
    print("%s: %d occurrences in %d files  (%s)" %
          (old, total, len(touched), "APPLIED -> " + new if apply else "dry-run"))

    if apply:
        left = sum(len(pat.findall(io.open(p, encoding="utf-8").read())) for p in iter_files())
        print("verify: whole-word '%s' remaining = %d (must be 0)" % (old, left))
        return 1 if left else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
