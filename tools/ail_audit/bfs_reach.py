"""Phase C — BFS reachability + classification of all nodes reached from
confirmed AIL_* public functions.

Seeds: every Phase B record whose verdict in {confirmed_public,
confirmed_public_multi, public_wrapper, confirmed_public_trivial}.

For each reached node, classify by current Ghidra name prefix and CRT lookup:

    ail_public_callee       — name starts with AIL_ but not AIL_internal_
                              (i.e. a public-API recursion or seed itself)
    ail_internal_confirmed  — name starts with AIL_internal_
    crt_real                — name in CRT lookup or canonical CRT public symbol
                              (do NOT rename)
    crt_candidate_pure      — name starts with crt_, NOT in CRT lookup
    crt_candidate_shared    — same as above but reached from non-AIL caller too
                              (we mark this only via additional caller analysis;
                              for Phase C we use a simplified rule and let
                              Phase D's caller-set check refine)
    excluded                — align_nop_* (alignment filler — BFS edges to it
                              are skipped per plan; should not appear)
    boundary_leak           — game-named function (anomaly)

BFS traversal rule: an edge from N -> M is skipped iff M's name starts with
align_nop_; align_nop_ functions are not enqueued.

Inputs:
  program_info/call_graph.json
  rebuild_info/crt_lookup_9.5a.json
  workspace/ail_audit/phase_b_public_verify.json

Output:
  workspace/ail_audit/phase_c_reachable.json
"""
from __future__ import annotations

import json
from collections import deque
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CALL_GRAPH = REPO / "program_info" / "call_graph.json"
CRT_LOOKUP = REPO / "rebuild_info" / "crt_lookup_9.5a.json"
PHASE_B = REPO / "workspace" / "ail_audit" / "phase_b_public_verify.json"
OUT = REPO / "workspace" / "ail_audit" / "phase_c_reachable.json"

PUBLIC_CRT_SYMBOLS = {
    "malloc", "free", "_nmalloc", "_nfree",
    "fread", "fwrite", "fopen", "fclose", "fseek", "fgets", "fputs",
    "getc", "putc", "vfprintf", "fprintf", "sprintf",
    "open", "close", "read", "write", "lseek", "_tell", "_filelength",
    "memcpy", "memmove", "memset", "_memset_bulk", "_memset_inner",
    "strcpy", "strncpy", "strncmp", "strnicmp", "strlen",
    "tolower", "toupper",
    "strtod", "sin", "cos",
    "time", "asctime", "mktime",
    "exit", "getenv",
    "outp",
    "delay",
    "__filbuf", "__get_doserrno_ptr", "__get_errno_ptr",
}


def categorize(name: str, addr: str, crt_lookup_addrs: set, crt_lookup_names: set) -> str:
    if name.startswith("align_nop_"):
        return "excluded"
    if name.startswith("AIL_internal_"):
        return "ail_internal_confirmed"
    if name.startswith("AIL_"):
        return "ail_public_callee"
    if addr in crt_lookup_addrs or name in crt_lookup_names:
        return "crt_real"
    if name in PUBLIC_CRT_SYMBOLS:
        return "crt_real"
    if name.startswith("crt_"):
        return "crt_candidate_pure"
    return "boundary_leak"


def main():
    cg = json.loads(CALL_GRAPH.read_text(encoding="utf-8"))
    crt = json.loads(CRT_LOOKUP.read_text(encoding="utf-8"))
    pb = json.loads(PHASE_B.read_text(encoding="utf-8"))

    nodes_by_addr = {n["address"]: n for n in cg["nodes"]}
    edges_out = {}  # addr -> list of (to_addr)
    for e in cg["edges"]:
        edges_out.setdefault(e["from"], []).append(e["to"])

    crt_lookup_addrs = set(crt["by_address"].keys())
    crt_lookup_names = set(crt["by_name"].keys())

    confirmed_verdicts = {
        "confirmed_public", "confirmed_public_multi",
        "public_wrapper", "confirmed_public_trivial",
    }
    seeds = [r["address"] for r in pb["records"] if r["verdict"] in confirmed_verdicts]

    visited = {}     # addr -> {name, category, depth, first_pred}
    queue = deque()
    for s in seeds:
        node = nodes_by_addr.get(s)
        if not node:
            continue
        cat = categorize(node["name"], s, crt_lookup_addrs, crt_lookup_names)
        visited[s] = {"name": node["name"], "category": cat,
                      "depth": 0, "first_pred": None,
                      "is_seed": True}
        queue.append((s, 0))

    while queue:
        cur, depth = queue.popleft()
        for nxt in edges_out.get(cur, []):
            node = nodes_by_addr.get(nxt)
            if not node:
                continue
            if node["name"].startswith("align_nop_"):
                continue
            if nxt in visited:
                continue
            cat = categorize(node["name"], nxt, crt_lookup_addrs, crt_lookup_names)
            visited[nxt] = {"name": node["name"], "category": cat,
                            "depth": depth + 1, "first_pred": cur,
                            "is_seed": False}
            queue.append((nxt, depth + 1))

    bucket_counts = {}
    for v in visited.values():
        bucket_counts[v["category"]] = bucket_counts.get(v["category"], 0) + 1

    boundary_leaks = [
        {"address": a, **v} for a, v in visited.items()
        if v["category"] == "boundary_leak"
    ]

    crt_candidates = sorted(
        a for a, v in visited.items()
        if v["category"] == "crt_candidate_pure"
    )

    OUT.write_text(json.dumps({
        "_meta": {
            "seeds_count": len(seeds),
            "reached_total": len(visited),
            "bucket_counts": bucket_counts,
            "boundary_leaks_count": len(boundary_leaks),
            "crt_candidates_count": len(crt_candidates),
        },
        "boundary_leaks": boundary_leaks,
        "crt_candidates": crt_candidates,
        "reachable": {
            a: visited[a] for a in sorted(visited.keys())
        },
    }, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"seeds:         {len(seeds)}")
    print(f"reached total: {len(visited)}")
    for cat, cnt in sorted(bucket_counts.items()):
        print(f"  {cat:30s} {cnt}")
    print(f"\nboundary_leaks: {len(boundary_leaks)}")
    print(f"crt_candidates: {len(crt_candidates)}")
    print(f"\nwrote: {OUT.relative_to(REPO)}")


if __name__ == "__main__":
    main()
