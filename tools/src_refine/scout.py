#!/usr/bin/env python3
"""scout.py -- emit the next refine batch (args.json) for one partition.

Resume checkpoint = the per-symbol shard files. This always returns the NEXT
not-yet-refined symbols for a partition, so re-running after ANY interruption picks up
exactly where it stopped (mirrors code_emit/next_batch.py).

A symbol is "done" iff its shard file <shards-dir>/<addr8>.json exists. Shards are
committed per-symbol inside each worktree, so point --shards-dir at the worktree's
shards/<partition> dir when scouting a resume.

Input  : tools/src_refine/data/partitions/<partition>.json (TRACKED) + shard files
Output : workspace/src_refine/args/<partition>.args.json   (scratch; fed to src_refine.wf.js)
Usage  : python tools/src_refine/scout.py --partition rp1 --root <worktree-abs> \
                 [--shards-dir <abs>] [--limit 50] [--stats]
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PART_DIR = ROOT / "workspace" / "src_refine" / "partitions"
ARGS_DIR = ROOT / "workspace" / "src_refine" / "args"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--partition", required=True, help="rp1 / rp2 / ...")
    ap.add_argument("--root", default="", help="absolute checkout path (args.root for the workflow)")
    ap.add_argument("--shards-dir", default="",
                    help="dir holding <addr>.json shards (default: <root>/tools/src_refine/data/shards/<partition>)")
    ap.add_argument("--limit", type=int, default=50, help="max symbols in this batch (hard-stop granularity)")
    ap.add_argument("--stats", action="store_true")
    a = ap.parse_args()

    part = json.loads((PART_DIR / (a.partition + ".json")).read_text(encoding="utf-8"))

    if a.shards_dir:
        shards = Path(a.shards_dir)
    elif a.root:
        shards = Path(a.root) / "tools" / "src_refine" / "data" / "shards" / a.partition
    else:
        shards = ROOT / "tools" / "src_refine" / "data" / "shards" / a.partition
    done = {p.stem for p in shards.glob("*.json")} if shards.is_dir() else set()

    def slim(s):
        # the refiner re-derives every detail from Ghidra; pass only what the
        # workflow needs to identify+route the symbol (keeps full-partition args small).
        o = {"address": s["address"], "name": s["name"], "kind": s["kind"]}
        if s["kind"] == "global":
            if s.get("datatype"):
                o["datatype"] = s["datatype"]
            if s.get("segment"):
                o["segment"] = s["segment"]
        else:
            o["is_thunk"] = bool(s.get("is_thunk"))
        return o

    total = part["n_syms"]
    files_out, n_in_batch, remaining = [], 0, 0
    for f in part["files"]:
        undone = [s for s in f["symbols"] if s["address"] not in done]
        remaining += len(undone)
        if a.stats or n_in_batch >= a.limit:
            continue
        take = undone[: max(0, a.limit - n_in_batch)]
        if take:
            files_out.append({"home": f["home"], "symbols": [slim(s) for s in take]})
            n_in_batch += len(take)

    if a.stats:
        print(json.dumps({"partition": a.partition, "total": total,
                          "done": len(done), "remaining": remaining}, indent=2))
        return

    out = {"root": a.root, "label": "refine-" + a.partition, "partition": a.partition,
           "maxRounds": 1, "files": files_out}
    ARGS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = ARGS_DIR / (a.partition + ".args.json")
    out_path.write_text(json.dumps(out, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"partition": a.partition, "total": total, "done": len(done),
                      "remaining": remaining, "in_batch": n_in_batch,
                      "args_file": str(out_path)}, indent=2))


if __name__ == "__main__":
    main()
