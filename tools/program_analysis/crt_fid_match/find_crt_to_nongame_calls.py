"""Audit: which CRT functions in crt_lookup_9.5a.json call non-CRT functions?

Reads the post-rename Ghidra call graph (adjacency format) and the verified
CRT lookup, then for each lookup entry inspects its callees and reports any
that are NOT also in the lookup. This catches:

- CRT helpers missed by FidDb (small unmatched stubs called only by CRT) —
  expected, low concern
- Real game/AIL calls from CRT (would indicate a hook or wrong classification)
- Misnamed / mis-classified entries (a lookup entry that's actually game code)

Output: workspace/crt_fid_match/crt_external_callees.json
"""
import json
import re
from collections import defaultdict
from pathlib import Path

LOOKUP = json.loads(Path("rebuild_info/crt/lookup_9.5a.json").read_text(encoding="utf-8"))
_RAW_ADJ = Path("workspace/crt_fid_match/call_graph_adjacency_post_crt_rename.txt").read_text(encoding="utf-8")


def _unwrap_mcp_result(text: str) -> str:
    """Ghidra MCP saves results as {"result": "...escaped..."}; tolerate raw too."""
    try:
        blob = json.loads(text)
        if isinstance(blob, dict) and "result" in blob:
            return blob["result"]
    except json.JSONDecodeError:
        pass
    return text


ADJ = _unwrap_mcp_result(_RAW_ADJ)

# Build sets of CRT names (primary + aliases) and their addresses.
crt_names = set()
crt_addrs = set()
for addr, e in LOOKUP["by_address"].items():
    crt_addrs.add(addr.lower())
    crt_names.add(e["name"])
    for al in e.get("aliases", []):
        crt_names.add(al)
# 0x4d8ea was demoted to noop_stub_4d8ea_zero — not in lookup, but still in
# binary. Plus 0x4d340 sibling JMP thunk. Track for completeness.

# Parse adjacency: each line "caller: callee1, callee2, ..."
adjacency: dict[str, list[str]] = {}
for line in ADJ.splitlines():
    line = line.strip()
    if not line or ":" not in line:
        continue
    caller, callee_str = line.split(":", 1)
    callees = [c.strip() for c in callee_str.split(",") if c.strip()]
    adjacency[caller.strip()] = callees

# For each CRT name, list its non-CRT callees.
# A callee is "non-CRT" if its NAME is NOT in crt_names. We classify these
# further using name patterns.
# Bare Watcom C-RTL public symbols (no `__` / `crt_` prefix). Mirror of
# `PUBLIC_CRT_SYMBOLS` in tools/program_analysis/build_call_graph.py, kept
# inline (not imported) so classification rules don't silently shift when
# the call-graph builder is touched.
KNOWN_CRT_BARE_NAMES = {
    # heap
    "malloc", "free",
    # stdio
    "fread", "fwrite", "fopen", "fclose", "fseek", "fgets", "fputs",
    "getc", "putc", "vfprintf", "fprintf", "sprintf",
    # POSIX-ish low level I/O
    "open", "close", "read", "write", "lseek",
    # memory / string
    "memcpy", "memmove", "memset",
    "strcpy", "strncpy", "strncmp", "strnicmp", "strlen",
    # ctype
    "tolower", "toupper",
    # numeric / math
    "strtod", "sin", "cos", "log", "log2",
    # time
    "time", "asctime", "mktime",
    # process / env
    "exit", "getenv",
    # x86 port I/O
    "outp",
    # misc CRT support
    "delay",
    # historical bare names retained from prior Ghidra runs (may have been
    # renamed since but kept here so older adjacency dumps still classify)
    "abort", "atexit", "raise", "signal", "longjmp", "setjmp",
    "system", "getpid", "kbhit", "calloc", "realloc", "_exit",
}


def classify(callee_name: str, hook_targets: set[str]) -> str:
    """Return one of: crt_helper_unmatched, watcom_internal_unmatched,
    crt_alt_entry_unmatched, noop_stub_unmatched, ail, game, addr_only,
    hook_user_main."""
    nm = callee_name
    if nm in hook_targets:
        return "hook_user_main"
    # Watcom-style internal names start with __ or _ followed by lowercase
    if nm.startswith("__"):
        return "watcom_internal_unmatched"
    if nm.startswith("_") and re.match(r"^_[a-z]", nm):
        return "watcom_internal_unmatched"
    if nm.startswith("crt_"):
        return "crt_helper_unmatched"
    # `L_<fn>_alt_<offset>` mid-entry labels are CRT alt-entries that don't
    # carry an independent lookup entry; the parent function's CRT pool
    # assignment applies. `L$N_*` anonymous statics are normally in lookup
    # so seldom reach here, but treat them the same.
    if nm.startswith("L_") or nm.startswith("L$"):
        return "crt_alt_entry_unmatched"
    if nm.startswith("IF@"):
        return "crt_helper_unmatched"
    if nm.startswith("noop_stub_"):
        return "noop_stub_unmatched"
    if nm in KNOWN_CRT_BARE_NAMES:
        return "watcom_internal_unmatched"
    if nm.startswith("AIL_") or nm.startswith("ail_"):
        return "ail"
    if re.match(r"^FUN_[0-9a-f]+$", nm) or re.match(r"^[a-f0-9]{8}$", nm):
        return "addr_only"
    # Otherwise heuristic
    return "game"


# Hooks where CRT is *expected* to call into game/user code (Watcom contract).
HOOK_TARGETS = {
    "fd2_main",  # __CMain calls user main (here renamed to fd2_main in FD2)
}


report = {}
for caller_name, callees in adjacency.items():
    if caller_name not in crt_names:
        continue  # not a CRT function
    non_crt = []
    for c in callees:
        if c in crt_names:
            continue
        non_crt.append({"callee": c, "class": classify(c, HOOK_TARGETS)})
    if non_crt:
        report[caller_name] = non_crt

# Summarize by class
by_class = defaultdict(list)
for caller, items in report.items():
    for it in items:
        by_class[it["class"]].append((caller, it["callee"]))

# Output
out = {
    "lookup_size": len(LOOKUP["by_address"]),
    "crt_callers_with_non_crt_callees": len(report),
    "total_non_crt_call_edges": sum(len(v) for v in report.values()),
    "by_class": {cls: sorted(set(items)) for cls, items in by_class.items()},
    "by_caller": {k: report[k] for k in sorted(report.keys())},
}
out_path = Path("workspace/crt_fid_match/crt_external_callees.json")
out_path.write_text(json.dumps(out, indent=2, ensure_ascii=False), encoding="utf-8")

# Console summary
print(f"Lookup CRT entries: {out['lookup_size']}")
print(f"CRT functions with at least one non-CRT callee: {out['crt_callers_with_non_crt_callees']}")
print(f"Total non-CRT call edges: {out['total_non_crt_call_edges']}")
print()
print("Breakdown by callee class:")
for cls in sorted(by_class.keys()):
    items = by_class[cls]
    print(f"  {cls}: {len(items)} edges")
    # Unique callees per class
    callees = sorted({c for _, c in items})
    print(f"    unique callees ({len(callees)}): {callees[:15]}{'...' if len(callees) > 15 else ''}")
print()
print("CRT callers with the most non-CRT callees:")
ranked = sorted(report.items(), key=lambda kv: -len(kv[1]))[:10]
for caller, items in ranked:
    classes = ", ".join(f"{c['callee']}({c['class']})" for c in items[:5])
    extra = f" ... +{len(items)-5} more" if len(items) > 5 else ""
    print(f"  {caller} ({len(items)}): {classes}{extra}")
