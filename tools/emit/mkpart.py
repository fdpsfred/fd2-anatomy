#!/usr/bin/env python3
"""mkpart.py - split the not-yet-emitted FD2 functions into N file-disjoint,
count-balanced partitions for parallel emit across git worktrees.

Source of truth = src/routing.json (read live, never hardcoded). "Remaining" =
entries with done != true and a real subfile target (target not starting with
'<', i.e. excluding any <fragment:...> placeholder). Each src subfile (the
routing `target`) is assigned WHOLE to exactly one partition, so two partitions
never touch the same src/<stem>.c or tests/<stem>.c main file.

Balancing: greedy LPT (longest-processing-time) bin-packing -- sort subfiles by
descending function count, assign each to the currently-smallest bin. Fully
deterministic (no randomness): ties break by subfile name / bin index.

Outputs tools/emit/partitions/branch_1.json .. branch_N.json, each:
  {"branch": i, "count": K, "subfiles": [...sorted...], "addrs": [...sorted 8-hex...]}
next_batch.py --partition <file> filters emit scouting to target in `subfiles`.

Usage:
  python tools/emit/mkpart.py [--branches 4] [--out tools/emit/partitions]
Prints a per-branch summary and self-verifies (disjoint + complete + balanced).
"""
import argparse
import collections
import json
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
ROUTING = REPO_ROOT / "src" / "routing.json"


def remaining_by_subfile(routing):
    """Group not-done, real-target entries by their src subfile (routing target).
    Returns OrderedDict {target: [addr,...]} with addrs sorted ascending."""
    groups = collections.defaultdict(list)
    for addr, v in routing.items():
        if v.get("done"):
            continue
        target = str(v.get("target", ""))
        if target.startswith("<"):      # <fragment:...> placeholder -> not real work
            continue
        groups[target].append(addr)
    return {t: sorted(addrs) for t, addrs in groups.items()}


def pack(groups, n):
    """Greedy LPT: assign each whole subfile to the smallest bin. Deterministic."""
    bins = [{"subfiles": [], "addrs": [], "count": 0} for _ in range(n)]
    # largest subfile first; tie-break by name for reproducibility
    for target in sorted(groups, key=lambda t: (-len(groups[t]), t)):
        # smallest current count; tie-break by lowest bin index
        bi = min(range(n), key=lambda i: (bins[i]["count"], i))
        b = bins[bi]
        b["subfiles"].append(target)
        b["addrs"].extend(groups[target])
        b["count"] += len(groups[target])
    for b in bins:
        b["subfiles"].sort()
        b["addrs"].sort()
    return bins


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--branches", type=int, default=4)
    ap.add_argument("--out", default=str(Path(__file__).resolve().parent / "partitions"))
    a = ap.parse_args()

    routing = json.loads(ROUTING.read_text(encoding="utf-8"))
    groups = remaining_by_subfile(routing)
    total = sum(len(v) for v in groups.values())
    bins = pack(groups, a.branches)

    # --- self-verification (fail fast; these MUST hold) ---
    all_addrs = [a2 for b in bins for a2 in b["addrs"]]
    all_subs = [s for b in bins for s in b["subfiles"]]
    assert len(all_addrs) == total, f"addr count {len(all_addrs)} != remaining {total}"
    assert len(set(all_addrs)) == len(all_addrs), "duplicate addr across bins"
    assert len(set(all_subs)) == len(all_subs), "subfile assigned to >1 bin"
    assert set(all_subs) == set(groups), "subfile coverage mismatch"

    outdir = Path(a.out)
    outdir.mkdir(parents=True, exist_ok=True)
    for i, b in enumerate(bins, 1):
        rec = {"branch": i, "count": b["count"],
               "subfiles": b["subfiles"], "addrs": b["addrs"]}
        (outdir / f"branch_{i}.json").write_text(
            json.dumps(rec, indent=2, ensure_ascii=False), encoding="utf-8")

    counts = [b["count"] for b in bins]
    print(f"remaining={total}  branches={a.branches}  "
          f"counts={counts}  spread={max(counts)-min(counts)}")
    for i, b in enumerate(bins, 1):
        print(f"\nbranch_{i}: {b['count']} fns across {len(b['subfiles'])} subfiles")
        for t in b["subfiles"]:
            print(f"  {len([x for x in b['addrs'] if x in set(groups[t])]):3d}  {t}")
    print(f"\nwrote {a.branches} manifests to {outdir}")


if __name__ == "__main__":
    main()
