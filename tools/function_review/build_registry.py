"""Build the per-function review registry from raw Ghidra MCP dumps.

Reads dumps from workspace/function_review/raw/ (list_functions_enhanced
JSON-in-JSON, full call_graph edges as plain text, AIL string list, and bulk
xref result for AIL strings) and emits workspace/function_review/registry.json
plus workspace/function_review/edges.json.

The registry has one row per function with mechanical signals only — no
naming or categorisation judgement. It exists so the per-function review
sessions in Phase B/C/D can iterate a worklist; the actual category and
final name are written back during review.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RAW = REPO / "workspace" / "function_review" / "raw"
OUT = REPO / "workspace" / "function_review"

LIST_FN_FILE = RAW / "list_functions_enhanced.txt"
CALL_GRAPH_FILE = RAW / "call_graph_adjacency.txt"
AIL_STRINGS_FILE = RAW / "ail_strings.txt"
AIL_XREFS_FILE = RAW / "ail_string_xrefs.json"

REGISTRY_OUT = OUT / "registry.json"
EDGES_OUT = OUT / "edges.json"
META_OUT = OUT / "meta.json"


def _unwrap_mcp_result(text: str) -> str:
    """Ghidra MCP saves results as {"result": "...escaped..."}."""
    blob = json.loads(text)
    return blob["result"] if isinstance(blob, dict) and "result" in blob else text


def load_functions() -> list[dict]:
    text = LIST_FN_FILE.read_text(encoding="utf-8")
    inner = _unwrap_mcp_result(text)
    payload = json.loads(inner)
    return payload["functions"]


def load_edges_by_name() -> list[tuple[str, str]]:
    """Parse Ghidra's adjacency-format call graph: 'caller: callee1, callee2, ...'.

    json.loads decodes the JSON-escaped \\n into real newlines, so split on \\n.
    """
    raw = CALL_GRAPH_FILE.read_text(encoding="utf-8")
    inner = _unwrap_mcp_result(raw)
    edges = []
    for entry in inner.split("\n"):
        entry = entry.strip()
        if not entry or ":" not in entry:
            continue
        caller, callee_blob = entry.split(":", 1)
        caller = caller.strip()
        for callee in callee_blob.split(","):
            callee = callee.strip()
            if callee:
                edges.append((caller, callee))
    return edges


def load_ail_strings() -> dict[str, str]:
    """Map string address (8-hex lowercase, no 0x) -> string content (without quotes)."""
    out = {}
    pat = re.compile(r"^([0-9a-fA-F]{8}):\s*\"(.*)\"$")
    for line in AIL_STRINGS_FILE.read_text(encoding="utf-8").splitlines():
        m = pat.match(line.strip())
        if m:
            addr = m.group(1).lower()
            out[addr] = m.group(2)
    return out


def load_ail_xrefs() -> dict[str, list[str]]:
    """Map string address -> list of code addresses referencing it."""
    raw = AIL_XREFS_FILE.read_text(encoding="utf-8")
    blob = json.loads(raw)
    out = {}
    for sa, refs in blob.items():
        out[sa.lower()] = [r["from"].lower() for r in refs if r.get("type") == "DATA"]
    return out


def derive_function_address_from_name(name: str) -> str | None:
    """Recover address from auto names like FUN_0003617e or thunk_FUN_0003dccd."""
    m = re.search(r"FUN_([0-9a-fA-F]{8})", name)
    return m.group(1).lower() if m else None


REVIEW_STATE_KEYS = ("review_status", "confirmed_name", "final_category",
                     "tentative_system", "notes")


def load_existing_review_state() -> dict[str, dict]:
    """If a registry already exists, return {address: {state_keys: values}}.

    Used to preserve human review work across re-runs that refresh raw dumps.
    """
    if not REGISTRY_OUT.exists():
        return {}
    rows = json.loads(REGISTRY_OUT.read_text(encoding="utf-8"))
    out = {}
    for r in rows:
        out[r["address"]] = {k: r.get(k) for k in REVIEW_STATE_KEYS}
    return out


def build():
    functions = load_functions()
    name_edges = load_edges_by_name()
    ail_strings = load_ail_strings()
    ail_xrefs = load_ail_xrefs()
    prior_state = load_existing_review_state()

    # Build name -> address map for resolving call graph edge endpoints
    name_to_addr: dict[str, str] = {}
    addr_to_name: dict[str, str] = {}
    addr_sorted: list[tuple[int, str]] = []  # (int_addr, addr_hex)

    for f in functions:
        addr = f["address"].lower()
        name = f["name"]
        name_to_addr[name] = addr
        addr_to_name[addr] = name
        addr_sorted.append((int(addr, 16), addr))
    addr_sorted.sort()

    def find_containing_function(code_addr_hex: str) -> str | None:
        """Largest function start address <= code_addr."""
        target = int(code_addr_hex, 16)
        lo, hi = 0, len(addr_sorted) - 1
        best = None
        while lo <= hi:
            mid = (lo + hi) // 2
            if addr_sorted[mid][0] <= target:
                best = addr_sorted[mid][1]
                lo = mid + 1
            else:
                hi = mid - 1
        return best

    # Build call graph edges (resolved to addresses)
    edges = []
    callees_by_addr: dict[str, set[str]] = {}
    callers_by_addr: dict[str, set[str]] = {}
    unresolved_edges = 0

    for caller_name, callee_name in name_edges:
        caller_addr = name_to_addr.get(caller_name)
        callee_addr = name_to_addr.get(callee_name)
        if caller_addr is None:
            # Try recover from FUN_xxx pattern (in case name had leading whitespace etc.)
            recovered = derive_function_address_from_name(caller_name)
            if recovered and recovered in addr_to_name:
                caller_addr = recovered
        if callee_addr is None:
            recovered = derive_function_address_from_name(callee_name)
            if recovered and recovered in addr_to_name:
                callee_addr = recovered
        if caller_addr is None or callee_addr is None:
            unresolved_edges += 1
            continue
        edges.append({"from": caller_addr, "to": callee_addr,
                      "from_name": caller_name, "to_name": callee_name})
        callees_by_addr.setdefault(caller_addr, set()).add(callee_addr)
        callers_by_addr.setdefault(callee_addr, set()).add(caller_addr)

    # Stable order: Ghidra's adjacency dump iteration is non-deterministic across
    # runs. Sorting here keeps program_info regenerations diff-clean.
    edges.sort(key=lambda e: (e["from"], e["to"]))

    # Map each AIL string xref code-address -> containing function
    fn_to_ail_strings: dict[str, list[dict]] = {}
    for s_addr, content in ail_strings.items():
        for code_addr in ail_xrefs.get(s_addr, []):
            fn_addr = find_containing_function(code_addr)
            if fn_addr:
                fn_to_ail_strings.setdefault(fn_addr, []).append({
                    "string_addr": s_addr,
                    "string_content": content,
                    "ref_site": code_addr,
                })

    # Build registry rows
    registry = []
    fun_pat = re.compile(r"^FUN_[0-9a-fA-F]{8}$")
    thunk_fun_pat = re.compile(r"^thunk_FUN_[0-9a-fA-F]{8}$")

    counts = {
        "total": 0, "renamed": 0, "auto": 0, "thunk": 0,
        "ail": 0, "crt_prefix": 0,
        "hint_ail": 0, "hint_crt": 0, "hint_game": 0,
    }

    for f in functions:
        addr = f["address"].lower()
        name = f["name"]
        is_thunk = bool(f.get("isThunk"))
        is_external = bool(f.get("isExternal"))

        is_auto = bool(fun_pat.match(name) or thunk_fun_pat.match(name))
        is_renamed = not is_auto

        callees = sorted(callees_by_addr.get(addr, set()))
        callers = sorted(callers_by_addr.get(addr, set()))

        ail_refs = fn_to_ail_strings.get(addr, [])

        # Mechanical category hint (NEVER used for naming, only worklist sort)
        if name.startswith("AIL_") or ail_refs:
            hint = "ail"
        elif name.startswith("crt_"):
            hint = "crt"
        else:
            hint = "game"

        counts["total"] += 1
        if is_thunk:
            counts["thunk"] += 1
        if is_renamed:
            counts["renamed"] += 1
        else:
            counts["auto"] += 1
        if name.startswith("AIL_"):
            counts["ail"] += 1
        if name.startswith("crt_"):
            counts["crt_prefix"] += 1
        counts[f"hint_{hint}"] += 1

        prior = prior_state.get(addr, {})
        registry.append({
            "address": addr,
            "current_name": name,
            "is_renamed": is_renamed,
            "is_thunk": is_thunk,
            "is_external": is_external,
            "callers": callers,
            "callees": callees,
            "callers_count": len(callers),
            "callees_count": len(callees),
            "ail_string_refs": ail_refs,
            "category_hint": hint,
            "review_status": prior.get("review_status") or "pending",
            "confirmed_name": prior.get("confirmed_name"),
            "final_category": prior.get("final_category"),
            "tentative_system": prior.get("tentative_system"),
            "notes": prior.get("notes"),
        })

    REGISTRY_OUT.write_text(
        json.dumps(registry, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    EDGES_OUT.write_text(
        json.dumps(edges, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    META_OUT.write_text(
        json.dumps({
            "function_count": len(functions),
            "edge_count": len(edges),
            "unresolved_edges": unresolved_edges,
            "ail_string_count": len(ail_strings),
            "ail_strings_with_xref": sum(1 for v in ail_xrefs.values() if v),
            "counts": counts,
        }, indent=2),
        encoding="utf-8",
    )

    # Console summary
    print(f"functions:       {len(functions)}")
    print(f"renamed:         {counts['renamed']}")
    print(f"auto (FUN_):     {counts['auto']}")
    print(f"thunks (Ghidra): {counts['thunk']}")
    print(f"AIL_ named:      {counts['ail']}")
    print(f"crt_ named:      {counts['crt_prefix']}")
    print(f"hint=ail:        {counts['hint_ail']}")
    print(f"hint=crt:        {counts['hint_crt']}")
    print(f"hint=game:       {counts['hint_game']}")
    print(f"edges total:     {len(edges)}")
    print(f"unresolved edges:{unresolved_edges}")
    print(f"AIL strings:     {len(ail_strings)}  with xref: "
          f"{sum(1 for v in ail_xrefs.values() if v)}")
    print(f"functions w/ AIL string ref: {len(fn_to_ail_strings)}")
    print(f"\nwrote:\n  {REGISTRY_OUT.relative_to(REPO)}\n  "
          f"{EDGES_OUT.relative_to(REPO)}\n  {META_OUT.relative_to(REPO)}")


if __name__ == "__main__":
    build()
