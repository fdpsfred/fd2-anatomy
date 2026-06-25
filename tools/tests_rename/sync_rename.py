#!/usr/bin/env python3
"""sync_rename.py -- sync tests/ symbol references to the post-src_refine new names.

src_refine Stage-2 + closeout renamed ~78 symbols in src/ + Ghidra + emit configs
+ KB docs, but tests/ was generated pre-rename and still references the dead old
names -> tests/ no longer compiles against src/include/. This applies the
authoritative old->new map (READ-ONLY from tools/src_refine/data/rename_old2new.json
'symbols') to every tests/**/*.{c,h} via a SINGLE-PASS atomic word-boundary
replacement, so the n=168/169 pose-table name SWAP resolves correctly (a
sequential two-pass would cancel it back). See tools/src_refine/data/rename_explain.md.

This tool only ever READS tools/src_refine/ (the frozen map); it never writes there.
Dry-run by default (reports only). --apply writes files (UTF-8, EOL preserved).

Usage:
    python tools/tests_rename/sync_rename.py            # dry-run report
    python tools/tests_rename/sync_rename.py --apply     # write changes
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAP_FILE = ROOT / "tools" / "src_refine" / "data" / "rename_old2new.json"  # read-only
TESTS = ROOT / "tests"
OUT_DIR = ROOT / "workspace" / "tests_rename"

# The two pose-table names form a reverse pair (n=168/169 true swap). After a
# correct single-pass replacement BOTH names still exist in the file (just
# swapped), so a residual re-scan will "find" them -- that is a known false
# positive, not a missed rename. Reported separately, never counted as failure.
POSE_PAIR = {
    "data_fd2_chapter_intro_portrait_pose_x_column_table",
    "data_fd2_chapter_intro_portrait_pose_y_row_table",
}


def load_symbols():
    return json.load(open(MAP_FILE, encoding="utf-8"))["symbols"]


def build_pattern(m):
    # longest old-name first so a shorter name that is a prefix of a longer one
    # can never shadow it; word boundaries stop partial-token damage
    # (e.g. ..._text must not bite ..._text_ptr).
    olds = sorted(m, key=len, reverse=True)
    body = "|".join(re.escape(o) for o in olds)
    return re.compile(r"(?<![A-Za-z0-9_])(" + body + r")(?![A-Za-z0-9_])")


def iter_files():
    for p in sorted(TESTS.rglob("*.c")) + sorted(TESTS.rglob("*.h")):
        yield p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true",
                    help="write the replacements (default: dry-run report only)")
    args = ap.parse_args()

    m = load_symbols()
    pat = build_pattern(m)

    per_old = {}                 # old name -> total occurrences across tests/
    per_file = {}                # rel path -> count
    file_olds = {}               # rel path -> {old: count}
    total_subs = 0

    for fp in iter_files():
        with open(fp, encoding="utf-8", newline="") as fh:   # newline="" keeps EOL
            txt = fh.read()
        local = {}

        def repl(x, _local=local):
            old = x.group(1)
            per_old[old] = per_old.get(old, 0) + 1
            _local[old] = _local.get(old, 0) + 1
            return m[old]

        new, n = pat.subn(repl, txt)
        if n:
            rel = fp.relative_to(ROOT).as_posix()
            per_file[rel] = n
            file_olds[rel] = dict(local)
            total_subs += n
            if args.apply:
                with open(fp, "w", encoding="utf-8", newline="") as fh:
                    fh.write(new)

    report = {
        "mode": "apply" if args.apply else "dry-run",
        "map_symbols_total": len(m),
        "old_names_present": sorted(per_old),
        "old_names_present_count": len(per_old),
        "old_names_absent": sorted(set(m) - set(per_old)),
        "total_substitutions": total_subs,
        "files_changed": len(per_file),
        "per_old": dict(sorted(per_old.items(), key=lambda kv: -kv[1])),
        "per_file": dict(sorted(per_file.items())),
        "file_olds": file_olds,
    }
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    rpt_path = OUT_DIR / ("apply_report.json" if args.apply else "dryrun_report.json")
    rpt_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    print("== sync_rename (%s) ==" % report["mode"])
    print("map symbols           : %d" % report["map_symbols_total"])
    print("old names present     : %d / %d" % (report["old_names_present_count"], len(m)))
    print("total substitutions   : %d" % total_subs)
    print("files changed         : %d" % len(per_file))
    pose_present = [o for o in per_old if o in POSE_PAIR]
    print("pose-pair present     : %s" % (pose_present or "none"))
    print("\n-- per old name (count) --")
    for old, c in report["per_old"].items():
        print("  %4d  %s -> %s" % (c, old, m[old]))
    print("\n-- per file (count) --")
    for rel, c in report["per_file"].items():
        print("  %4d  %s" % (c, rel))
    print("\nreport written: %s" % rpt_path.relative_to(ROOT).as_posix())
    return 0


if __name__ == "__main__":
    sys.exit(main())
