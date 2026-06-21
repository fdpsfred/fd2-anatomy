#!/usr/bin/env python3
"""build_worklist.py -- assemble the src/ refine scope and reconcile it against live Ghidra.

Scope (every symbol currently emitted into src/):
  - functions: routing.json entries with done==True and not skip and target endswith '.c'
  - globals  : worklist.tsv rows with status=='real_in_src'

Reconcile (source of truth = live Ghidra dump, per audit-source-of-truth rule). The
live dump is produced separately by a Ghidra run_script_inline pass into:
  workspace/src_refine/ghidra_functions.tsv   (addr<TAB>name<TAB>thunk|real)
  workspace/src_refine/ghidra_symbols.tsv     (addr<TAB>name<TAB>SymbolType)
Each scope symbol is checked against that dump (exists? name matches?). When the live
Ghidra name differs from the registry name, the LIVE name wins (current_name := live).
Coverage gaps (Ghidra fd2_/data_fd2_ symbols that look in-scope but the registry misses)
are reported for manual review, not silently added.

Inputs  : tools/code_emit/data/routing.json, workspace/data_emit/worklist.tsv,
          workspace/call_graph/call_graph.json, the two ghidra_*.tsv dumps, src/ tree.
Outputs : workspace/src_refine/worklist.json, workspace/src_refine/coverage_reconcile_start.md
Usage   : python tools/src_refine/build_worklist.py
"""
import csv
import glob
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ROUTING = ROOT / "tools" / "code_emit" / "data" / "routing.json"
WORKLIST_TSV = ROOT / "workspace" / "data_emit" / "worklist.tsv"
CALLGRAPH = ROOT / "workspace" / "call_graph" / "call_graph.json"
WS = ROOT / "workspace" / "src_refine"          # regenerable intermediates (gitignored)
GH_FUNCS = WS / "ghidra_functions.tsv"
GH_SYMS = WS / "ghidra_symbols.tsv"
SRC = ROOT / "src"

OUT_JSON = WS / "worklist.json"                 # regenerable from registries + Ghidra dump
OUT_MD = WS / "coverage_reconcile_start.md"

FN_PREFIXES = ("fd2_", "crt_equivalent_", "crt_equiv_")
GLOBAL_PREFIXES = ("data_fd2_", "data_crt_")


def norm(addr):
    """Normalize a hex address to 8-lowercase-hex (Ghidra/routing canonical form)."""
    a = addr.strip().lower()
    if "::" in a:            # .image::00000000 -> keep marker, not a src/ address
        return a
    return a.zfill(8)


def load_ghidra_dump(path, with_thunk=False):
    """addr -> name (primary). Skips .image:: header pseudo-addresses."""
    d, extra = {}, {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        parts = line.split("\t")
        a, name = norm(parts[0]), parts[1]
        if "::" in a:
            continue
        d[a] = name
        if with_thunk and len(parts) > 2:
            extra[a] = parts[2]
    return (d, extra) if with_thunk else d


def main():
    # ---- live Ghidra dumps -------------------------------------------------
    gf, gf_thunk = load_ghidra_dump(GH_FUNCS, with_thunk=True)
    gs = load_ghidra_dump(GH_SYMS)

    # ---- registries --------------------------------------------------------
    routing = json.loads(ROUTING.read_text(encoding="utf-8"))
    cg_nodes = json.loads(CALLGRAPH.read_text(encoding="utf-8")).get("nodes", [])
    cg = {norm(n["address"]): n for n in cg_nodes}

    wl_rows = list(csv.DictReader(WORKLIST_TSV.read_text(encoding="utf-8").splitlines(),
                                  delimiter="\t"))

    # ---- src/ basename -> path-relative-to-src (clean 1:1, verified no collision) ----
    base2rel = {}
    for p in glob.glob(str(SRC / "**" / "*.c"), recursive=True):
        rel = Path(p).resolve().relative_to(SRC).as_posix()
        base2rel.setdefault(os.path.basename(p), []).append(rel)

    drift = {"fn_name_mismatch": [], "fn_missing_in_ghidra": [],
             "fn_vendor_in_scope": [], "fn_cg_missing": [],
             "global_name_mismatch": [], "global_missing_in_ghidra": [],
             "global_home_unresolved": []}
    symbols = []

    # ---- function scope ----------------------------------------------------
    fn_scope_addrs = set()
    for addr, v in routing.items():
        if not (v.get("done") and not v.get("skip") and str(v.get("target", "")).endswith(".c")):
            continue
        a = norm(addr)
        fn_scope_addrs.add(a)
        reg_name = v["name"]
        gname = gf.get(a)
        cur = reg_name
        if gname is None:
            drift["fn_missing_in_ghidra"].append({"addr": a, "name": reg_name})
        elif gname != reg_name:
            drift["fn_name_mismatch"].append({"addr": a, "routing": reg_name, "ghidra": gname})
            cur = gname            # live Ghidra wins
        node = cg.get(a)
        if node is None:
            drift["fn_cg_missing"].append(a)
            category, emit_action = None, None
        else:
            category, emit_action = node.get("category"), node.get("emit_action")
            if emit_action == "link_vendor_lib":
                drift["fn_vendor_in_scope"].append({"addr": a, "name": cur})
        symbols.append({
            "address": a, "name": cur, "routing_name": reg_name, "ghidra_name": gname,
            "kind": "function", "home": v["target"],
            "is_thunk": gf_thunk.get(a) == "thunk",
            "category": category, "emit_action": emit_action,
        })

    # ---- global scope ------------------------------------------------------
    glob_scope_addrs = set()
    for r in wl_rows:
        if r.get("status") != "real_in_src":
            continue
        a = norm(r["addr"])
        reg_name = r["name"]
        gname = gs.get(a)
        cur = reg_name
        if gname is None:
            # No named primary symbol at this address in live Ghidra. This is an
            # array element absorbed into an adjacent real array (e.g. the 3 stale
            # *_bg_layer_{0,1,2}_buf_ptr scalars now live as one [3] array at the
            # base addr). It is not a standalone refine target -> drop from scope,
            # keep in the drift report for review.
            drift["global_missing_in_ghidra"].append({"addr": a, "name": reg_name})
            continue
        glob_scope_addrs.add(a)
        if gname != reg_name:
            drift["global_name_mismatch"].append({"addr": a, "worklist": reg_name, "ghidra": gname})
            cur = gname
        base = r["src_home"]
        rels = base2rel.get(base)
        if not rels:
            drift["global_home_unresolved"].append({"addr": a, "name": reg_name, "src_home": base})
            home = None
        else:
            home = rels[0]
        symbols.append({
            "address": a, "name": cur, "worklist_name": reg_name, "ghidra_name": gname,
            "kind": "global", "home": home,
            "tsv_kind": r.get("kind"), "datatype": r.get("datatype"),
            "len": r.get("len"), "xrefs": r.get("xrefs"), "segment": r.get("segment"),
        })

    # ---- coverage gaps (report only) --------------------------------------
    routing_all = {norm(a) for a in routing}
    fn_gaps = []
    for a, name in gf.items():
        if not name.startswith(FN_PREFIXES):
            continue
        if a in fn_scope_addrs:
            continue
        node = cg.get(a)
        ea = node.get("emit_action") if node else None
        if a in routing_all:
            rv = routing[a] if a in routing else routing.get(a.lstrip("0"))
            reason = "in routing but " + ("skip" if rv and rv.get("skip") else
                                          ("target!=.c (%s)" % (rv.get("target") if rv else "?")))
        else:
            reason = "NOT in routing (emit_action=%s)" % ea
        # only emit_fd2_source not-in-routing is a true gap; vendor/artifact/skip are expected
        true_gap = (a not in routing_all) and (ea == "emit_fd2_source")
        fn_gaps.append({"addr": a, "name": name, "reason": reason, "true_gap": true_gap})

    wl_all_addrs = {norm(r["addr"]) for r in wl_rows}
    glob_gaps = []
    for a, name in gs.items():
        if not name.startswith(GLOBAL_PREFIXES):
            continue
        if name.startswith("data_le_"):     # LE module header structures, never src/
            continue
        if a in glob_scope_addrs:
            continue
        in_wl = a in wl_all_addrs
        glob_gaps.append({"addr": a, "name": name,
                          "reason": "in worklist (status!=real_in_src)" if in_wl else "NOT in worklist",
                          "true_gap": not in_wl})

    homes = sorted({s["home"] for s in symbols if s["home"]})
    out = {
        "counts": {"functions": len(fn_scope_addrs), "globals": len(glob_scope_addrs),
                   "total": len(symbols), "files": len(homes)},
        "drift_counts": {k: len(v) for k, v in drift.items()},
        "coverage_gap_counts": {
            "functions_total": len(fn_gaps),
            "functions_true_gap": sum(1 for g in fn_gaps if g["true_gap"]),
            "globals_total": len(glob_gaps),
            "globals_true_gap": sum(1 for g in glob_gaps if g["true_gap"]),
        },
        "drift": drift,
        "coverage_gaps": {"functions": fn_gaps, "globals": glob_gaps},
        "symbols": symbols,
    }
    OUT_JSON.write_text(json.dumps(out, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    # ---- human-readable reconcile report ----------------------------------
    md = []
    md.append("# src_refine 起手 coverage reconcile (live Ghidra)\n")
    md.append("## Scope")
    md.append("- functions (routing done & !skip & target=.c): **%d**" % len(fn_scope_addrs))
    md.append("- globals (worklist real_in_src): **%d**" % len(glob_scope_addrs))
    md.append("- total symbols: **%d** across **%d** .c files\n" % (len(symbols), len(homes)))
    md.append("## Drift vs live Ghidra (live name wins on mismatch)")
    for k, v in drift.items():
        md.append("- %s: **%d**" % (k, len(v)))
        for item in v[:20]:
            md.append("  - %s" % json.dumps(item, ensure_ascii=False))
        if len(v) > 20:
            md.append("  - ... (%d more)" % (len(v) - 20))
    md.append("\n## Coverage gaps (review; not auto-added)")
    md.append("### functions (Ghidra fd2_/crt_equivalent_ not in function scope): %d (true gaps: %d)"
              % (len(fn_gaps), sum(1 for g in fn_gaps if g["true_gap"])))
    for g in [x for x in fn_gaps if x["true_gap"]]:
        md.append("  - TRUE GAP %s %s -- %s" % (g["addr"], g["name"], g["reason"]))
    md.append("### globals (Ghidra data_fd2_/data_crt_ not in global scope): %d (true gaps: %d)"
              % (len(glob_gaps), sum(1 for g in glob_gaps if g["true_gap"])))
    for g in [x for x in glob_gaps if x["true_gap"]]:
        md.append("  - TRUE GAP %s %s -- %s" % (g["addr"], g["name"], g["reason"]))
    OUT_MD.write_text("\n".join(md) + "\n", encoding="utf-8")

    print(json.dumps({"counts": out["counts"], "drift_counts": out["drift_counts"],
                      "coverage_gap_counts": out["coverage_gap_counts"]}, indent=2))


if __name__ == "__main__":
    main()
