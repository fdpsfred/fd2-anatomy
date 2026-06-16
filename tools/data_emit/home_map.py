#!/usr/bin/env python3
"""home_map.py -- propose a home src file AND the emit class for every data
symbol to migrate, so Phase 1 knows (a) which file to emit each into and (b)
whether real bytes must be extracted.

Emit class is decided by the AUTHORITATIVE signal -- the bytes at the symbol's
address (non-zero => has a real initializer) -- cross-checked with whether any
function writes it (Ghidra write-xref):
  * const     : no writer + non-zero bytes  -> `const T[] = {real bytes}`   (read-only rodata)
  * init-data : has writer + non-zero bytes -> `T name = {real bytes};`     (initialized mutable)
  * zero-bss  : all-zero bytes (writer or not) -> `T name;`                 (zero-init; written at
                runtime, sometimes only indirectly via memcpy/pointer so no direct write-xref)
`needs_bytes` = const|init-data (must pass the verify_real byte gate in Phase 1).

Home: the writer's routing-target .c when there is one (the owner that
initializes the global); else a per-subsystem data file keyed off the
data_fd2_<subsystem>_ name (Phase 1 picks the 8.3 filename).

Inputs (regenerate upstream first; the byte / xref dumps are Ghidra-side):
  workspace/data_emit/worklist.tsv           (reconcile: status / segment / kind / len)
  workspace/data_emit/data_xref_owners.tsv   (Ghidra: write_fns / read_fn_count)
  workspace/data_emit/fake_bytes_nonzero.tsv (Ghidra: 1=non-zero / 0=all-zero / -1=unreadable)
  src/routing.json                           (function name -> target .c)

Output: workspace/data_emit/home_map.tsv + a printed distribution summary.
"""
import io, os, json
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WL = os.path.join(ROOT, "workspace", "data_emit", "worklist.tsv")
OWN = os.path.join(ROOT, "workspace", "data_emit", "data_xref_owners.tsv")
NZ = os.path.join(ROOT, "workspace", "data_emit", "fake_bytes_nonzero.tsv")
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
                owners[p[1]] = [x for x in p[2].split(";") if x]
    nonzero = {}
    with io.open(NZ, encoding="utf-8") as f:
        f.readline()
        for ln in f:
            p = ln.rstrip("\n").split("\t")
            if len(p) >= 2:
                nonzero[p[0]] = p[1]

    rows = []
    with io.open(WL, encoding="utf-8") as f:
        f.readline()
        for ln in f:
            p = ln.rstrip("\n").split("\t")
            if len(p) >= 8 and p[7] == "fake_in_testglob":
                rows.append(p)

    out, dist, clscnt = [], Counter(), Counter()
    for p in rows:
        seg, name, ln, kind = p[1], p[2], p[4], p[5]
        wfns = owners.get(name, [])
        nz = nonzero.get(name, "?")
        if nz == "1":
            cls = "init-data" if wfns else "const"
        elif nz == "0":
            cls = "zero-bss"
        else:
            cls = "unknown"
        needs_bytes = "Y" if cls in ("const", "init-data") else "N"
        wfiles = Counter(targets.get(fn) for fn in wfns if targets.get(fn))
        if wfiles:
            home = wfiles.most_common(1)[0][0]
            note = "" if len(wfiles) == 1 else "multi-writer:" + ",".join(sorted(wfiles))
        else:
            sub = name.split("_")[2] if name.count("_") >= 2 else "misc"
            home = "const-data:%s" % sub
            note = "indirect-write bss" if cls == "zero-bss" else "read-only const table"
        out.append((name, seg, kind, ln, cls, needs_bytes, home, ";".join(wfns[:4]), note))
        dist[home] += 1
        clscnt[cls] += 1

    with io.open(OUT, "w", encoding="utf-8") as f:
        f.write("name\tsegment\tkind\tlen\temit_class\tneeds_bytes\thome\twriter_fns\tnote\n")
        for r in out:
            f.write("\t".join(str(x) for x in r) + "\n")

    nb = clscnt["const"] + clscnt["init-data"]
    print("=== home_map: %d fake_in_testglob data symbols ===" % len(out))
    print("--- emit class (by bytes x writer) ---")
    for c in ("const", "init-data", "zero-bss", "unknown"):
        if clscnt.get(c):
            print("  %-10s %d" % (c, clscnt[c]))
    print("  -> needs_bytes (verify_real gate): %d ; zero-init: %d" % (nb, clscnt["zero-bss"]))
    print("--- proposed home distribution (top 20) ---")
    for h, c in dist.most_common(20):
        print("  %3d  %s" % (c, h))
    print("WROTE %s" % OUT)


if __name__ == "__main__":
    main()
