#!/usr/bin/env python3
"""verify.py -- cross-checks for the tests/ rename sync (READ-ONLY).

Two independent checks, neither of which trusts the rename map blindly:

  --targets  (run BEFORE apply): for every old name actually present in tests/,
             confirm its NEW name appears at least once under src/ (so the rename
             target is a real identifier tests can resolve against src/include/).
             A new name missing from src/ would mean tests still won't compile.

  --residual (run AFTER apply): re-scan tests/ with the same old-name pattern.
             Expect ZERO residual old names, EXCEPT the pose pair which legitimately
             still appears after the n=168/169 swap (both names persist, swapped) --
             those are reported separately and are not a failure.

  --orphans  (run AFTER apply): catch renames the snapshot map does NOT cover. Scan
             tests/ for every fd2_/data_fd2_/crt_equivalent_ token and report those
             that do NOT appear anywhere under src/. A token absent from src/ is
             either a stale name the map missed (-> investigate via live Ghidra) or
             a test-local symbol. This is a stronger completeness check than
             --residual, which only sees names already in the map.

Reads tools/src_refine/data/rename_old2new.json read-only; never writes anywhere
except the workspace report. Exit 0 = check passed, 1 = failed.
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAP_FILE = ROOT / "tools" / "src_refine" / "data" / "rename_old2new.json"  # read-only
SRC = ROOT / "src"
TESTS = ROOT / "tests"
OUT_DIR = ROOT / "workspace" / "tests_rename"

POSE_PAIR = {
    "data_fd2_chapter_intro_portrait_pose_x_column_table",
    "data_fd2_chapter_intro_portrait_pose_y_row_table",
}


def load_symbols():
    return json.load(open(MAP_FILE, encoding="utf-8"))["symbols"]


def word_pat(names):
    body = "|".join(re.escape(o) for o in sorted(names, key=len, reverse=True))
    return re.compile(r"(?<![A-Za-z0-9_])(" + body + r")(?![A-Za-z0-9_])")


def read(p):
    with open(p, encoding="utf-8", newline="") as fh:
        return fh.read()


def present_old_names(m):
    pat = word_pat(m)
    present = set()
    for p in list(TESTS.rglob("*.c")) + list(TESTS.rglob("*.h")):
        present |= set(pat.findall(read(p)))
    return present


def src_text_blob():
    parts = []
    for p in list(SRC.rglob("*.c")) + list(SRC.rglob("*.h")):
        parts.append(read(p))
    return "\n".join(parts)


def src_word_set():
    return set(re.findall(r"[A-Za-z_]\w*", src_text_blob()))


def check_targets(m):
    present = present_old_names(m)
    blob = src_text_blob()
    # word-boundary presence of each needed NEW name in src/
    missing = []
    for old in sorted(present):
        new = m[old]
        if not re.search(r"(?<![A-Za-z0-9_])" + re.escape(new) + r"(?![A-Za-z0-9_])", blob):
            missing.append((old, new))
    print("== verify --targets ==")
    print("old names present in tests/ : %d" % len(present))
    print("new names missing from src/ : %d" % len(missing))
    for old, new in missing:
        print("  MISSING  %s -> %s" % (old, new))
    ok = not missing
    print("RESULT: %s" % ("PASS" if ok else "FAIL"))
    return ok


def check_residual(m):
    pat = word_pat(m)
    residual = {}     # rel -> {old: count}
    pose_hits = {}    # rel -> {old: count}
    for p in list(TESTS.rglob("*.c")) + list(TESTS.rglob("*.h")):
        txt = read(p)
        hits = {}
        for mo in pat.finditer(txt):
            old = mo.group(1)
            hits[old] = hits.get(old, 0) + 1
        if not hits:
            continue
        rel = p.relative_to(ROOT).as_posix()
        non_pose = {k: v for k, v in hits.items() if k not in POSE_PAIR}
        pose = {k: v for k, v in hits.items() if k in POSE_PAIR}
        if non_pose:
            residual[rel] = non_pose
        if pose:
            pose_hits[rel] = pose
    print("== verify --residual ==")
    print("files with TRUE residual old names : %d" % len(residual))
    for rel, hits in sorted(residual.items()):
        for old, c in hits.items():
            print("  RESIDUAL %4d  %s  in %s" % (c, old, rel))
    print("files with pose-pair (expected false positive after swap): %d"
          % len(pose_hits))
    for rel, hits in sorted(pose_hits.items()):
        for old, c in hits.items():
            print("  pose     %4d  %s  in %s" % (c, old, rel))
    ok = not residual
    print("RESULT: %s" % ("PASS" if ok else "FAIL"))
    return ok


def check_orphans(m):
    src_words = src_word_set()
    tok = re.compile(
        r"(?<![A-Za-z0-9_])((?:fd2_|data_fd2_|crt_equivalent_)\w+)(?![A-Za-z0-9_])")
    orphans = {}     # token -> {rel: count}
    for p in list(TESTS.rglob("*.c")) + list(TESTS.rglob("*.h")):
        txt = read(p)
        rel = p.relative_to(ROOT).as_posix()
        for mo in tok.finditer(txt):
            t = mo.group(1)
            if t not in src_words:
                orphans.setdefault(t, {})
                orphans[t][rel] = orphans[t].get(rel, 0) + 1
    print("== verify --orphans ==")
    print("distinct test tokens (fd2_/data_fd2_/crt_equivalent_) absent from src/: %d"
          % len(orphans))
    for t, files in sorted(orphans.items()):
        total = sum(files.values())
        print("  ORPHAN %4d  %s  (in %d file(s))" % (total, t, len(files)))
    ok = not orphans
    print("RESULT: %s%s" % ("PASS" if ok else "REVIEW",
                            "" if ok else "  (classify each: stale rename vs test-local)"))
    return ok, orphans


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--targets", action="store_true")
    ap.add_argument("--residual", action="store_true")
    ap.add_argument("--orphans", action="store_true")
    args = ap.parse_args()
    if not (args.targets or args.residual or args.orphans):
        ap.error("pass --targets and/or --residual and/or --orphans")
    m = load_symbols()
    ok = True
    if args.targets:
        ok &= check_targets(m)
    if args.residual:
        ok &= check_residual(m)
    if args.orphans:
        ok &= check_orphans(m)[0]
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
