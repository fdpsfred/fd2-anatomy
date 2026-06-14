#!/usr/bin/env python3
"""home_map.py -- propose a home src file for every data symbol to migrate, so
Phase 1 can group the data-emit fan-out by target file.

Home rule (principled):
  * has writer function(s): home = the routing target .c of the writer (the
    function that initializes / owns the global). Single writer-file -> that
    file; multiple -> the most-written file, others listed in `note`.
  * no writer (read-only const table): `const-data` -- a subsystem data file
    keyed off the data_fd2_<subsystem>_ name; Phase 1 picks the 8.3 filename.

Inputs (all in workspace/, regenerate upstream first):
  workspace/data_emit/worklist.tsv          (reconcile: status / segment / kind / len)
  workspace/data_emit/data_xref_owners.tsv  (Ghidra: write_fns / read_fn_count)
  src/routing.json                          (function name -> target .c)

Output: workspace/data_emit/home_map.tsv (fake_in_testglob rows + proposed home)
        + a printed distribution summary.
"""
import io, os, json, re
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WL = os.path.join(ROOT, "workspace", "data_emit", "worklist.tsv")
OWN = os.path.join(ROOT, "workspace", "data_emit", "data_xref_owners.tsv")
ROUTING = os.path.join(ROOT, "src", "routing.json")
OUT = os.path.join(ROOT, "workspace", "data_emit", "home_map.tsv")


def load_routing_targets():
    r = json.load(io.open(ROUTING, encoding="utf-8"))
    items = r.items() if isinstance(r, dict) else [(f.get("addr"), f) for f in r]
    return {v.get("name"): v.get("target") for k, v in items
            if isinstance(v, dict) and v.get("name")}


def main():
    targets = load_routing_targets()
    owners = {}
    with io.open(OWN, encoding="utf-8") as f:
        f.readline()
        for ln in f:
            p = ln.rstrip("\n").split("\t")
            if len(p) >= 5:
                addr, name, wf, rc, tot = p[:5]
                owners[name] = [x for x in wf.split(";") if x]

    rows = []
    with io.open(WL, encoding="utf-8") as f:
        f.readline()
        for ln in f:
            p = ln.rstrip("\n").split("\t")
            if len(p) >= 8 and p[7] == "fake_in_testglob":
                rows.append(p)  # addr seg name dt len kind xr status ...

    out, dist, kindcnt = [], Counter(), Counter()
    for p in rows:
        addr, seg, name, dt, ln, kind = p[0], p[1], p[2], p[3], p[4], p[5]
        wfns = owners.get(name, [])
        wfiles = Counter(targets.get(fn) for fn in wfns if targets.get(fn))
        if wfiles:
            home = wfiles.most_common(1)[0][0]
            note = "" if len(wfiles) == 1 else "multi-writer:" + ",".join(sorted(wfiles))
        else:
            sub = name.split("_")[2] if name.count("_") >= 2 else "misc"
            home = "const-data:%s" % sub
            note = "no-writer (read-only const table)"
        out.append((name, seg, kind, ln, home, ";".join(wfns[:4]), note))
        dist[home] += 1
        kindcnt[(kind, "const" if not wfns else "state")] += 1

    with io.open(OUT, "w", encoding="utf-8") as f:
        f.write("name\tsegment\tkind\tlen\thome\twriter_fns\tnote\n")
        for r in out:
            f.write("\t".join(str(x) for x in r) + "\n")

    print("=== home_map: %d fake_in_testglob data symbols ===" % len(out))
    print("--- kind x owner-class ---")
    for k, c in sorted(kindcnt.items()):
        print("  %-12s %-6s %d" % (k[0], k[1], c))
    print("--- proposed home distribution (top 25) ---")
    for h, c in dist.most_common(25):
        print("  %3d  %s" % (c, h))
    nconst = sum(c for h, c in dist.items() if h.startswith("const-data"))
    nstate = len(out) - nconst
    print("--- summary: %d state (writer-homed) / %d const (data-file) ---" % (nstate, nconst))
    print("WROTE %s" % OUT)


if __name__ == "__main__":
    main()
