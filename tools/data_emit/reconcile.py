#!/usr/bin/env python3
"""reconcile.py -- cross-reference the Ghidra data-symbol dump against the
current src/ (real defs) and tests/testglob.c (fake defs) to produce the
authoritative data-migration worklist.

Input  : workspace/data_emit/ghidra_data_symbols.tsv  (from run_script_inline)
Output : workspace/data_emit/worklist.tsv             (per-symbol status)
         + a printed summary.

Status per Ghidra symbol:
  real_in_src     -- already defined (file-scope) in some src/*.c   -> done
  fake_in_testglob-- defined (file-scope) only in tests/testglob.c  -> MIGRATE
  undefined       -- referenced/labelled but no C definition yet    -> CREATE
  sublabel        -- Ghidra len=-1 mid-array label (handled by parent)

All file reads are UTF-8. Definition detection = file-scope line (column 0),
optional `const`, a C type token, the symbol name, and a `[` or `=` (so extern
decls in headers and indented in-function uses are excluded).
"""
import io, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TSV  = os.path.join(ROOT, "workspace", "data_emit", "ghidra_data_symbols.tsv")
OUT  = os.path.join(ROOT, "workspace", "data_emit", "worklist.tsv")

# file-scope definition: starts at col 0 with optional const + a type word,
# contains the data_fd2_ name, and an array '[' or initializer/'=' on the line.
DEF_RE = re.compile(
    r"^(?:const\s+)?[A-Za-z_][A-Za-z0-9_ ]*?\*?\s*"   # type (maybe trailing *)
    r"(data_fd2_[A-Za-z0-9_]+)\s*(\[|=)"               # name then [ or =
)

def scan_defs(path):
    """Return {name: (basename, has_brace_initializer)} for file-scope defs."""
    out = {}
    with io.open(path, encoding="utf-8") as f:
        text = f.read()
    base = os.path.basename(path)
    for line in text.splitlines():
        m = DEF_RE.match(line)
        if m:
            name = m.group(1)
            brace = "{" in line  # initialized table vs scalar/bss
            out[name] = (base, brace)
    return out

def walk_src_c():
    for dp, _, fns in os.walk(os.path.join(ROOT, "src")):
        for fn in fns:
            if fn.endswith(".c"):
                yield os.path.join(dp, fn)

def main():
    src_defs = {}
    for p in walk_src_c():
        for k, v in scan_defs(p).items():
            src_defs.setdefault(k, v)
    tg = scan_defs(os.path.join(ROOT, "tests", "testglob.c"))

    rows = []
    with io.open(TSV, encoding="utf-8") as f:
        header = f.readline()
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 8:
                continue
            addr, seg, name, dt, ln, kind, xr, prim = parts[:8]
            if name in src_defs:
                status = "real_in_src"; home = src_defs[name][0]
            elif name in tg:
                status = "fake_in_testglob"; home = ""
            elif kind == "sublabel":
                status = "sublabel"; home = ""
            else:
                status = "undefined"; home = ""
            rows.append((addr, seg, name, dt, ln, kind, xr, status, home))

    with io.open(OUT, "w", encoding="utf-8") as f:
        f.write("addr\tsegment\tname\tdatatype\tlen\tkind\txrefs\tstatus\tsrc_home\n")
        for r in rows:
            f.write("\t".join(r) + "\n")

    # summary
    from collections import Counter
    by_status = Counter(r[7] for r in rows)
    by_seg_status = Counter((r[1], r[7]) for r in rows)
    migrate = [r for r in rows if r[7] == "fake_in_testglob"]
    mig_obj3 = [r for r in migrate if r[1] == ".object3"]
    mig_obj2 = [r for r in migrate if r[1] == ".object2"]
    mig_bytes_obj3 = sum(int(r[4]) for r in mig_obj3 if r[4].lstrip("-").isdigit() and int(r[4]) > 0)
    mig_ptr = [r for r in migrate if r[5] == "ptr_table"]

    print("=== reconcile: %d Ghidra data_fd2_ symbols ===" % len(rows))
    for k in ("real_in_src", "fake_in_testglob", "undefined", "sublabel"):
        print("  %-18s %d" % (k, by_status.get(k, 0)))
    print("--- by segment x status ---")
    for (seg, stt), c in sorted(by_seg_status.items()):
        print("  %-10s %-18s %d" % (seg, stt, c))
    print("--- MIGRATE worklist (fake_in_testglob = %d) ---" % len(migrate))
    print("  .object3 (FAR_DATA byte tables): %d symbols, %d bytes to extract" % (len(mig_obj3), mig_bytes_obj3))
    print("  .object2 (DGROUP const/state)  : %d symbols" % len(mig_obj2))
    print("  of which ptr_table             : %d (emit as symbol-name lists)" % len(mig_ptr))
    # cross-check: testglob defs not seen in Ghidra dump (naming drift)
    ghidra_names = set(r[2] for r in rows)
    orphan_tg = [n for n in tg if n not in ghidra_names]
    print("--- cross-check ---")
    print("  testglob file-scope data defs : %d" % len(tg))
    print("  src file-scope data defs      : %d" % len(src_defs))
    print("  testglob defs NOT in Ghidra dump (drift!): %d %s" %
          (len(orphan_tg), orphan_tg[:8]))
    print("WROTE %s" % OUT)

if __name__ == "__main__":
    main()
