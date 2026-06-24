#!/usr/bin/env python3
"""merge_shards.py -- combine per-symbol refine shards into the deliverables.

Reads every shard file (tools/src_refine/data/shards/rp*/<addr>.json) the refine
workflow committed, and emits the queryable deliverables:
  - src_info.json         : address-keyed full per-symbol analysis record (primary)
  - src_info_by_name.json : name (current AND final) -> address (so both name and
                            address queries work; rename keeps both keys resolvable)
  - src_issues.json       : ISS-#### logic issues flagged during refine (deferred work)
Cross-reference pass: each global record gets reader_fns / writer_fns derived from
every function's reads_globals / writes_globals.

Usage: python tools/src_refine/merge_shards.py
"""
import glob
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "tools" / "src_refine" / "data"
SHARDS = DATA / "shards"


def norm_home(h):
    """Canonicalize the agent-written 'home' to a repo-relative src/... path.

    Refiners ran in 4 worktrees and wrote 'home' inconsistently (bare
    'src/crt/crt.c', 'ROOT/src/crt/crt.c', or the absolute worktree path
    'C:/.../fd2-wt/rp4/src/crt/crt.c'). Every src file lives at src/<dir>/<file>.c
    with exactly one '/src/' segment, so slice from the last '/src/'.
    """
    if not h:
        return h
    h = h.replace("\\", "/")
    i = h.rfind("/src/")
    if i >= 0:
        return h[i + 1:]          # ".../src/crt/crt.c" -> "src/crt/crt.c"
    if h.startswith("src/"):
        return h                  # already canonical
    if h.startswith("ROOT/"):
        return h[len("ROOT/"):]   # "ROOT/foo" -> "foo" (defensive; no /src/ case)
    return h


def main():
    recs = {}
    for p in sorted(glob.glob(str(SHARDS / "rp*" / "*.json"))):
        r = json.loads(Path(p).read_text(encoding="utf-8"))
        # the shard FILENAME is the canonical 8-hex address (from scout/args); the
        # agent-written "address" field may vary (some add a 0x prefix), so key off
        # the filename and normalize the record's address to match.
        a = Path(p).stem.lower().replace("0x", "").zfill(8)
        if a in recs:
            raise SystemExit("duplicate shard address %s (%s vs %s)" % (a, p, recs[a].get("_shard")))
        r["address"] = a
        r["home"] = norm_home(r.get("home"))
        r["_shard"] = p.replace("\\", "/").split("/tools/")[-1]
        recs[a] = r

    # cross-ref: function reads/writes -> global reader_fns/writer_fns
    readers, writers = {}, {}
    for r in recs.values():
        if r.get("kind") == "function":
            fn = r.get("name_final") or r["name_current"]
            for g in (r.get("reads_globals") or []):
                readers.setdefault(g, set()).add(fn)
            for g in (r.get("writes_globals") or []):
                writers.setdefault(g, set()).add(fn)
    for r in recs.values():
        if r.get("kind") == "global":
            nm = r.get("name_final") or r["name_current"]
            r["reader_fns"] = sorted(readers.get(nm, set()))
            r["writer_fns"] = sorted(writers.get(nm, set()))

    # name -> address index (current + final both resolvable)
    name_idx = {}
    for a, r in recs.items():
        for key in {r["name_current"], r.get("name_final") or r["name_current"]}:
            name_idx[key] = a

    # issues -> ISS-#### (sequential; merge is single-pass, no concurrency)
    issues, i = {}, 0
    for a in sorted(recs):
        r = recs[a]
        for iss in (r.get("issues") or []):
            i += 1
            iid = "ISS-%04d" % i
            issues[iid] = {"id": iid, "symbol_address": a,
                           "symbol_name": r.get("name_final") or r["name_current"],
                           "kind": r.get("kind"), "home": r.get("home"),
                           "category": iss.get("category"), "severity": iss.get("severity"),
                           "title": iss.get("title"), "description": iss.get("description"),
                           "evidence": iss.get("evidence"), "status": "open"}
            r.setdefault("issue_ids", []).append(iid)

    (DATA / "src_info.json").write_text(json.dumps(recs, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (DATA / "src_info_by_name.json").write_text(json.dumps(name_idx, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (DATA / "src_issues.json").write_text(json.dumps(issues, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    nfn = sum(1 for r in recs.values() if r.get("kind") == "function")
    ngl = sum(1 for r in recs.values() if r.get("kind") == "global")
    nren = sum(1 for r in recs.values() if r.get("name_verdict") == "rename")
    nunc = sum(1 for r in recs.values() if r.get("name_verdict") == "uncertain")
    nparamren = sum(1 for r in recs.values() for p in (r.get("params") or []) if p.get("verdict") == "rename")
    nfn_with_paramren = sum(1 for r in recs.values()
                            if any(p.get("verdict") == "rename" for p in (r.get("params") or [])))
    print(json.dumps({"shards": len(recs), "functions": nfn, "globals": ngl,
                      "symbol_rename_proposed": nren, "uncertain": nunc,
                      "param_rename_proposed": nparamren, "fns_with_param_rename": nfn_with_paramren,
                      "issues": len(issues), "by_name_keys": len(name_idx)}, indent=2))


if __name__ == "__main__":
    main()
