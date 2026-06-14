#!/usr/bin/env python3
"""scout.py -- emit the data_emit.wf.js `args` JSON for one partition, skipping
symbols already finalized (data_routing.reviewed == true). Mirrors
tools/emit/next_batch.py for the data pipeline.

The main agent runs this once per worktree at 4-way launch time; the printed
JSON is passed straight to Workflow({scriptPath:"tools/data_emit/data_emit.wf.js",
args:<JSON>}). Because the workflow script has no filesystem access, the full
per-symbol routing entry must be inlined here.

Usage:
  python tools/data_emit/scout.py <partition_manifest> <worktree_abs_root> [label]
  python tools/data_emit/scout.py --stats          # coverage across all symbols
"""
import io, os, sys, json

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROUTING = os.path.join(ROOT, "src", "data_routing.json")
FIELDS = ("name", "addr", "segment", "len", "datatype", "kind",
          "emit_class", "needs_bytes", "writers", "note")


def load_routing():
    return json.load(io.open(ROUTING, encoding="utf-8"))


def stats():
    r = load_routing()
    done = sum(1 for e in r.values() if e.get("reviewed"))
    nb = sum(1 for e in r.values() if e.get("needs_bytes"))
    nb_done = sum(1 for e in r.values() if e.get("needs_bytes") and e.get("reviewed"))
    print("data_routing: %d symbols | reviewed %d / %d | needs_bytes %d (reviewed %d)" %
          (len(r), done, len(r), nb, nb_done))
    from collections import Counter
    pend = Counter(e["home"] for e in r.values() if not e.get("reviewed"))
    if pend:
        print("--- pending by home (%d files) ---" % len(pend))
        for h, c in pend.most_common():
            print("  %3d  %s" % (c, h))


def scout(manifest, root, label):
    r = load_routing()
    man = json.load(io.open(manifest, encoding="utf-8"))
    files = []
    for f in man["files"]:
        syms = []
        for nm in f["symbols"]:
            e = r.get(nm)
            if not e or e.get("reviewed"):
                continue
            syms.append({k: (nm if k == "name" else e[k]) for k in FIELDS})
        if syms:
            files.append({"home": f["home"], "symbols": syms})
    args = {"root": root, "label": label or man.get("label", "data"),
            "maxRounds": 10, "files": files}
    print(json.dumps(args, ensure_ascii=False))


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "--stats":
        stats(); return
    if len(sys.argv) < 3:
        print("usage: scout.py <partition_manifest> <worktree_abs_root> [label]", file=sys.stderr)
        sys.exit(2)
    manifest, root = sys.argv[1], sys.argv[2]
    label = sys.argv[3] if len(sys.argv) > 3 else None
    scout(manifest, root, label)


if __name__ == "__main__":
    main()
