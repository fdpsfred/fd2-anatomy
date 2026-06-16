#!/usr/bin/env python3
"""mkpart.py -- partition the Phase 1 data worklist into N file-disjoint groups
for parallel worktree emit (mirrors tools/emit/mkpart.py for functions).

Each parallel worktree owns a disjoint set of home .c files, so concurrent
emit never has two worktrees writing the same source file. Within a worktree the
data_emit workflow still processes symbols one at a time (same-file writes are
serial); partitioning only spreads *distinct files* across worktrees.

Steps:
  1. Read src/data_routing.json (mk_routing.py output).
  2. Re-split each over-budget `table/<stem>.c` bucket (the const-table homes)
     into numbered 8.3 subfiles <stem>.c, <stem>2.c, ... by address order, so no
     single serial chain dominates and no file blows the line budget. Owner .c
     files (life/main.c etc., which already hold emitted functions) are NEVER
     split -- their data defs all land in that one file.
  3. Greedy-LPT bin-pack every resulting file-unit into N partitions, weighting
     needs_bytes symbols x2 (byte extract + verify_real) over zero-bss x1.
  4. Write finalized homes back to src/data_routing.json and emit
     workspace/data_emit/partitions/part_{1..N}.json manifests.

Idempotent: table homes are normalized to their base stem (numeric suffix
stripped) before re-splitting, so re-running yields the same layout.

Usage:  python tools/data_emit/mkpart.py [N]      (default N=4)
"""
import io, os, re, sys, json
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROUTING = os.path.join(ROOT, "src", "data_routing.json")
PARTDIR = os.path.join(ROOT, "workspace", "data_emit", "partitions")

CAP_SYMS = 18       # max symbols per table subfile (serial-chain length)
CAP_BYTES = 12000   # max raw bytes per table subfile (~line budget)


def base_stem(home):
    """table/btltab2.c -> table/btltab.c ; owner files unchanged."""
    if not home.startswith("table/"):
        return home
    m = re.match(r"(table/[a-z]+?)(\d*)\.c$", home)
    return m.group(1) + ".c" if m else home


def numbered(base, i):
    """i=0 -> table/btltab.c ; i=1 -> table/btltab2.c ; ..."""
    if i == 0:
        return base
    stem = base[:-2]  # strip '.c'
    return "%s%d.c" % (stem, i + 1)


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 4
    routing = json.load(io.open(ROUTING, encoding="utf-8"))

    # group symbols by base home
    by_home = defaultdict(list)
    for name, e in routing.items():
        by_home[base_stem(e["home"])].append(name)

    # build file-units (split table buckets; owner files stay whole)
    units = []  # {home, symbols[], weight}
    for home, names in by_home.items():
        names.sort(key=lambda nm: int(str(routing[nm]["addr"]), 16))
        if not home.startswith("table/"):
            for nm in names:
                routing[nm]["home"] = home
            w = sum(2 if routing[nm]["needs_bytes"] else 1 for nm in names)
            units.append({"home": home, "symbols": names, "weight": w})
            continue
        # split table bucket by CAP_SYMS / CAP_BYTES
        cur, cur_bytes, idx = [], 0, 0
        def flush(cur, idx):
            sub = numbered(home, idx)
            for nm in cur:
                routing[nm]["home"] = sub
            w = sum(2 if routing[nm]["needs_bytes"] else 1 for nm in cur)
            units.append({"home": sub, "symbols": list(cur), "weight": w})
        for nm in names:
            ln = routing[nm]["len"]
            ln = ln if isinstance(ln, int) and ln > 0 else 8
            if cur and (len(cur) >= CAP_SYMS or cur_bytes + ln > CAP_BYTES):
                flush(cur, idx); idx += 1; cur, cur_bytes = [], 0
            cur.append(nm); cur_bytes += ln
        if cur:
            flush(cur, idx)

    # greedy LPT bin-pack into n partitions
    units.sort(key=lambda u: -u["weight"])
    parts = [{"label": "data-p%d" % (i + 1), "files": [], "weight": 0,
              "n_syms": 0} for i in range(n)]
    for u in units:
        p = min(parts, key=lambda p: p["weight"])
        p["files"].append({"home": u["home"], "symbols": u["symbols"]})
        p["weight"] += u["weight"]
        p["n_syms"] += len(u["symbols"])

    # persist finalized homes + manifests
    json.dump(routing, io.open(ROUTING, "w", encoding="utf-8"),
              indent=2, ensure_ascii=False)
    io.open(ROUTING, "a", encoding="utf-8").write("\n")
    if not os.path.isdir(PARTDIR):
        os.makedirs(PARTDIR)
    for i, p in enumerate(parts):
        fn = os.path.join(PARTDIR, "part_%d.json" % (i + 1))
        json.dump(p, io.open(fn, "w", encoding="utf-8"), indent=2, ensure_ascii=False)

    print("=== mkpart: %d symbols -> %d file-units -> %d partitions ===" %
          (len(routing), len(units), n))
    for p in parts:
        nb = sum(1 for f in p["files"] for nm in f["symbols"] if routing[nm]["needs_bytes"])
        print("  %-8s  %2d files  %3d syms (%3d needs_bytes)  weight %d" %
              (p["label"], len(p["files"]), p["n_syms"], nb, p["weight"]))
        for f in sorted(p["files"], key=lambda f: -len(f["symbols"])):
            print("        %3d  %s" % (len(f["symbols"]), f["home"]))


if __name__ == "__main__":
    main()
