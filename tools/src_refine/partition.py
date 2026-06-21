#!/usr/bin/env python3
"""partition.py -- split the refine worklist into N file-disjoint, weight-balanced partitions.

Partition unit = .c home file (a file is never split across partitions, so two worktrees
never edit the same file's comments). Greedy longest-processing-time balancing on symbol
count keeps the worktrees roughly equal in work.

Input  : workspace/src_refine/worklist.json (from build_worklist.py)
Output : tools/src_refine/data/partitions/rp1.json .. rpN.json  (TRACKED -- worktrees inherit
         them; workspace/ is gitignored so partitions can't live there)
Usage  : python tools/src_refine/partition.py [--n 4]
"""
import argparse
import json
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKLIST = ROOT / "workspace" / "src_refine" / "worklist.json"
OUT_DIR = ROOT / "workspace" / "src_refine" / "partitions"  # regenerable (deterministic LPT)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=4)
    a = ap.parse_args()

    wl = json.loads(WORKLIST.read_text(encoding="utf-8"))
    by_home = defaultdict(list)
    for s in wl["symbols"]:
        if not s.get("home"):
            raise SystemExit("symbol without home: %s" % s.get("name"))
        by_home[s["home"]].append(s)

    # greedy LPT: heaviest files first, each into the currently-lightest partition
    groups = [[] for _ in range(a.n)]
    load = [0] * a.n
    for home in sorted(by_home, key=lambda h: -len(by_home[h])):
        i = load.index(min(load))
        groups[i].append(home)
        load[i] += len(by_home[home])

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    summary = []
    for i, homes in enumerate(groups):
        label = "rp%d" % (i + 1)
        files = [{"home": h, "symbols": by_home[h]} for h in sorted(homes)]
        nsym = sum(len(f["symbols"]) for f in files)
        obj = {"label": label, "n_files": len(files), "n_syms": nsym, "files": files}
        (OUT_DIR / (label + ".json")).write_text(
            json.dumps(obj, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        summary.append({"label": label, "files": len(files), "symbols": nsym})

    print(json.dumps({"n": a.n, "total_symbols": len(wl["symbols"]),
                      "total_files": len(by_home), "partitions": summary}, indent=2))


if __name__ == "__main__":
    main()
