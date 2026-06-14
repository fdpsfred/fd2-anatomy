#!/usr/bin/env python3
"""mk_routing.py -- build src/data_routing.json: the per-symbol progress +
home source-of-truth for Phase 1 data landing (mirrors src/routing.json for
functions).

Joins home_map.tsv (emit_class / needs_bytes / proposed home / writers) with
worklist.tsv (addr / datatype) by symbol name, resolves each `const-data:<sub>`
bucket to a concrete 8.3 source file under src/table/, and emits one JSON entry
per symbol:

  "<name>": {
    addr, segment, len, datatype, kind, emit_class, needs_bytes(bool),
    home (path relative to src/), writers[], note,
    emitted(false), reviewed(false), commit(null)
  }

`home` placement only affects which .c the symbol's definition lands in; because
the linker resolves the symbol regardless of file, placement is Layer-2 neutral.
It exists so partitioning (mkpart.py) can keep each parallel worktree's set of
home files disjoint. Per-symbol correctness (true type / bytes / zero-init) is
established by the workflow's caller analysis + verify_real, not by this file.

Inputs (regenerate upstream first):
  workspace/data_emit/home_map.tsv   (home_map.py)
  workspace/data_emit/worklist.tsv   (reconcile.py: addr / datatype / status)

Output: src/data_routing.json  +  a printed home/file distribution summary.
"""
import io, os, json
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HM = os.path.join(ROOT, "workspace", "data_emit", "home_map.tsv")
WL = os.path.join(ROOT, "workspace", "data_emit", "worklist.tsv")
OUT = os.path.join(ROOT, "src", "data_routing.json")

# const-data:<subsystem> bucket -> concrete 8.3 table file under src/table/.
# (mkpart.py may further split an over-budget file into <stem>2.c etc.)
SUBSYS_FILE = {
    "battle": "table/btltab.c",
    "chapter": "table/chtab.c",
    "animation": "table/anitab.c",
    "ui": "table/uitab.c",
    "string": "table/strtab.c",
    "dialog": "table/dlgtab.c",
    "graphics": "table/gfxtab.c",
    "audio": "table/audtab.c",
    "orphan": "table/orphan.c",
}


def resolve_home(home):
    if home.startswith("const-data:"):
        sub = home.split(":", 1)[1]
        return SUBSYS_FILE.get(sub, "table/misctab.c")
    return home  # already an owner .c (e.g. life/main.c)


def main():
    # worklist: addr / datatype by name
    wl = {}
    with io.open(WL, encoding="utf-8") as f:
        f.readline()
        for ln in f:
            p = ln.rstrip("\n").split("\t")
            if len(p) >= 9:
                wl[p[2]] = {"addr": p[0], "datatype": p[3]}

    routing, dist, cls = {}, Counter(), Counter()
    miss = []
    with io.open(HM, encoding="utf-8") as f:
        f.readline()
        for ln in f:
            p = ln.rstrip("\n").split("\t")
            if len(p) < 9:
                continue
            name, seg, kind, length, emit_class, needs_bytes, home, writers, note = p[:9]
            w = wl.get(name)
            if not w:
                miss.append(name)
                continue
            final_home = resolve_home(home)
            routing[name] = {
                "addr": w["addr"],
                "segment": seg,
                "len": int(length) if length.lstrip("-").isdigit() else length,
                "datatype": w["datatype"],
                "kind": kind,
                "emit_class": emit_class,
                "needs_bytes": needs_bytes == "Y",
                "home": final_home,
                "writers": [x for x in writers.split(";") if x],
                "note": note,
                "emitted": False,
                "reviewed": False,
                "commit": None,
            }
            dist[final_home] += 1
            cls[emit_class] += 1

    with io.open(OUT, "w", encoding="utf-8") as f:
        json.dump(routing, f, indent=2, ensure_ascii=False)
        f.write("\n")

    print("=== mk_routing: %d symbols -> %s ===" % (len(routing), os.path.relpath(OUT, ROOT)))
    if miss:
        print("  !! %d symbols in home_map but missing from worklist: %s" %
              (len(miss), ", ".join(miss[:8])))
    print("--- emit class ---")
    for c in ("const", "init-data", "zero-bss", "unknown"):
        if cls.get(c):
            print("  %-10s %d" % (c, cls[c]))
    print("--- home file distribution (%d files) ---" % len(dist))
    for h, c in dist.most_common():
        print("  %3d  %s" % (c, h))


if __name__ == "__main__":
    main()
