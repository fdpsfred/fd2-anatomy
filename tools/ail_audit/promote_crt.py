"""Phase D — Recursive crt_* candidate promotion via worklist fixed-point.

For each crt_candidate (Phase C output) decide:

  promoted   — all live callers ⊆ AIL set AND disasm evidence passes
  ambiguous  — all callers ⊆ AIL set BUT no evidence (manual review)
  non_ail_caller — some caller is outside AIL set (legitimately shared CRT)
  deferred_zero  — no callers at all (orphan; defer to Phase F)

PURE_AIL initial set:
  - all 103 confirmed AIL_* (Phase B confirmed_*)
  - all 176 AIL_internal_*

Evidence gate (any one suffices):
  E1: callee includes AIL_internal_log_print_timestamp_prefix @ 0x3794c
  E2: data ref into ail_globals_map.json globals (non-string)
  E3: data ref into ail_string_xrefs.json strings

Edges to align_nop_* are stripped from caller list (alignment fall-through xrefs
are not real callers per plan).

Inputs:
  workspace/function_review/registry.json
  workspace/ail_audit/phase_b_public_verify.json
  workspace/ail_audit/phase_c_reachable.json
  workspace/ail_audit/crt_candidate_dump.json
  workspace/ail_audit/ail_globals_map.json
  workspace/ail_audit/ail_string_xrefs.json

Output:
  workspace/ail_audit/phase_d_promotion.json
  workspace/ail_audit/phase_d_summary.md
"""
from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "ail_audit"
FN_REVIEW = REPO / "workspace" / "function_review"

REG = FN_REVIEW / "registry.json"
PHASE_B = WORK / "phase_b_public_verify.json"
PHASE_C = WORK / "phase_c_reachable.json"
CRT_DUMP = WORK / "crt_candidate_dump.json"
AIL_GLOBALS = WORK / "ail_globals_map.json"
AIL_STRINGS = WORK / "ail_string_xrefs.json"

OUT_JSON = WORK / "phase_d_promotion.json"
OUT_MD = WORK / "phase_d_summary.md"

LOG_HELPER_ADDR = "0003794c"


def derive_proposed_name(addr: str, fn: dict, ail_globals: set, ail_strings: dict,
                         current_name: str) -> str:
    """Derive AIL_internal_<...> proposed name. Preference:

    1. If current name is `crt_<rest>`, propose `AIL_internal_<rest>` — keeps
       the existing semantic descriptor (alloc_and_commit, etc.).
    2. Else if function references a clearly-named AIL string, derive from it.
    3. Else fall back to addr-stamped `AIL_internal_helper_<addr>`.
    """
    if current_name.startswith("crt_"):
        rest = current_name[len("crt_"):]
        return f"AIL_internal_{rest}"
    string_refs = []
    for ref in fn.get("data_refs", []):
        tgt = ref["to"].lower()
        if tgt in ail_strings:
            string_refs.append(ail_strings[tgt]["content"])
    if string_refs:
        first = string_refs[0]
        m = re.match(r"AIL_([a-zA-Z0-9_]+)", first)
        if m:
            return f"AIL_internal_{m.group(1)}_helper_{addr}"
    return f"AIL_internal_helper_{addr}"


def check_evidence(addr: str, fn: dict, ail_globals: set, ail_strings: set) -> dict:
    """Return evidence record. Empty dict -> no evidence."""
    callees_addrs = {c["addr"].lower() for c in fn.get("callees", [])}
    e1 = LOG_HELPER_ADDR in callees_addrs
    e2_globals = []
    e3_strings = []
    for ref in fn.get("data_refs", []):
        tgt = ref["to"].lower()
        if not re.fullmatch(r"[0-9a-f]{8}", tgt):
            continue
        if tgt in ail_globals:
            e2_globals.append(tgt)
        if tgt in ail_strings:
            e3_strings.append(tgt)
    passes = e1 or e2_globals or e3_strings
    return {
        "passes": bool(passes),
        "log_helper_called": e1,
        "ail_globals_referenced": sorted(set(e2_globals)),
        "ail_strings_referenced": sorted(set(e3_strings)),
    }


def main():
    registry = json.loads(REG.read_text(encoding="utf-8"))
    phase_b = json.loads(PHASE_B.read_text(encoding="utf-8"))
    phase_c = json.loads(PHASE_C.read_text(encoding="utf-8"))
    crt_dump = json.loads(CRT_DUMP.read_text(encoding="utf-8"))
    globals_map = json.loads(AIL_GLOBALS.read_text(encoding="utf-8"))
    strings_map = json.loads(AIL_STRINGS.read_text(encoding="utf-8"))

    # Build lookup maps
    reg_by_addr = {r["address"]: r for r in registry}
    name_to_addr = {r["current_name"]: r["address"] for r in registry}
    by_addr_dump = crt_dump["by_addr"]

    confirmed_verdicts = {
        "confirmed_public", "confirmed_public_multi",
        "public_wrapper", "confirmed_public_trivial",
    }

    # PURE_AIL initial: confirmed AIL_* + all AIL_internal_*
    pure_ail = set()
    for r in phase_b["records"]:
        if r["verdict"] in confirmed_verdicts:
            pure_ail.add(r["address"])
    for r in registry:
        if r["current_name"].startswith("AIL_internal_"):
            pure_ail.add(r["address"])

    crt_candidates = list(phase_c["crt_candidates"])

    ail_globals_set = set(globals_map["globals"].keys())
    ail_strings_set = set(strings_map["by_string_addr"].keys())
    # Strings dict for naming heuristic
    ail_strings_dict = strings_map["by_string_addr"]

    promoted = {}
    ambiguous = {}
    non_ail_caller = {}
    deferred_zero = {}

    iter_count = 0
    while True:
        iter_count += 1
        changed = False
        for N in crt_candidates:
            if N in promoted or N in ambiguous or N in non_ail_caller or N in deferred_zero:
                continue
            reg_entry = reg_by_addr.get(N)
            fn_dump = by_addr_dump.get(N)
            if reg_entry is None or fn_dump is None:
                ambiguous[N] = {"reason": "missing data"}
                continue
            callers_all = set(reg_entry.get("callers", []))
            # Strip align_nop_* and self
            callers_live = {
                c for c in callers_all
                if c != N
                and not (reg_by_addr.get(c)
                         and reg_by_addr[c]["current_name"].startswith("align_nop_"))
            }
            if not callers_live:
                deferred_zero[N] = {
                    "name": reg_entry["current_name"],
                    "reason": "zero in-degree (no real callers)",
                }
                continue

            non_ail = callers_live - pure_ail
            if non_ail:
                # Has a caller outside AIL — record but allow re-evaluation if
                # those callers themselves get promoted later
                non_ail_resolved_names = {
                    c: reg_by_addr[c]["current_name"] if c in reg_by_addr else c
                    for c in non_ail
                }
                # If any non_ail caller is itself a crt_candidate not yet
                # decided, defer to next round
                pending_candidates = non_ail & set(crt_candidates)
                if pending_candidates and not all(
                    c in promoted or c in ambiguous or c in non_ail_caller or c in deferred_zero
                    for c in pending_candidates
                ):
                    # Wait
                    continue
                non_ail_caller[N] = {
                    "name": reg_entry["current_name"],
                    "non_ail_callers": non_ail_resolved_names,
                    "ail_callers": [
                        reg_by_addr[c]["current_name"] if c in reg_by_addr else c
                        for c in (callers_live - non_ail)
                    ],
                }
                continue

            # All callers ⊆ AIL — check evidence
            ev = check_evidence(N, fn_dump, ail_globals_set, ail_strings_set)
            if ev["passes"]:
                proposed = derive_proposed_name(N, fn_dump, ail_globals_set,
                                                ail_strings_dict,
                                                reg_entry["current_name"])
                promoted[N] = {
                    "name": reg_entry["current_name"],
                    "proposed_name": proposed,
                    "ail_callers": [
                        reg_by_addr[c]["current_name"] if c in reg_by_addr else c
                        for c in callers_live
                    ],
                    "evidence": ev,
                }
                pure_ail.add(N)
                changed = True
            else:
                ambiguous[N] = {
                    "name": reg_entry["current_name"],
                    "ail_callers": [
                        reg_by_addr[c]["current_name"] if c in reg_by_addr else c
                        for c in callers_live
                    ],
                    "evidence_failed": ev,
                    "reason": "all callers ⊆ AIL but no AIL-flavored disasm evidence",
                }
        if not changed:
            break

    # Emit JSON
    OUT_JSON.write_text(json.dumps({
        "_meta": {
            "candidate_count": len(crt_candidates),
            "promoted_count": len(promoted),
            "ambiguous_count": len(ambiguous),
            "non_ail_caller_count": len(non_ail_caller),
            "deferred_zero_count": len(deferred_zero),
            "fixed_point_iterations": iter_count,
        },
        "promoted": promoted,
        "ambiguous": ambiguous,
        "non_ail_caller": non_ail_caller,
        "deferred_zero_indegree": deferred_zero,
    }, indent=2, ensure_ascii=False), encoding="utf-8")

    # Emit MD
    md = ["# Phase D — crt_* recursive 升級判定 summary\n"]
    md.append(f"- candidate count: {len(crt_candidates)}")
    md.append(f"- fixed-point iterations: {iter_count}")
    md.append(f"- **promoted**: {len(promoted)}")
    md.append(f"- **ambiguous (manual review)**: {len(ambiguous)}")
    md.append(f"- **non_ail_caller (keep crt_*)**: {len(non_ail_caller)}")
    md.append(f"- **deferred_zero_indegree**: {len(deferred_zero)}")

    if promoted:
        md.append("\n## Promoted (使用者批准 name 後進 Phase E)\n")
        md.append("| addr | old name | proposed name | AIL callers | evidence |")
        md.append("|---|---|---|---|---|")
        for a, p in sorted(promoted.items()):
            ev = p["evidence"]
            ev_parts = []
            if ev["log_helper_called"]:
                ev_parts.append("log_helper")
            if ev["ail_globals_referenced"]:
                ev_parts.append(f"globals×{len(ev['ail_globals_referenced'])}")
            if ev["ail_strings_referenced"]:
                ev_parts.append(f"strings×{len(ev['ail_strings_referenced'])}")
            md.append(f"| `0x{a}` | `{p['name']}` | `{p['proposed_name']}` | "
                      f"{', '.join(p['ail_callers'])} | {', '.join(ev_parts)} |")

    if ambiguous:
        md.append("\n## Ambiguous (caller=AIL 但無 disasm evidence — 需使用者裁定)\n")
        md.append("| addr | name | AIL callers | reason |")
        md.append("|---|---|---|---|")
        for a, p in sorted(ambiguous.items()):
            md.append(f"| `0x{a}` | `{p.get('name','?')}` | "
                      f"{', '.join(p.get('ail_callers', []))} | {p.get('reason','?')} |")

    if non_ail_caller:
        md.append("\n## Non-AIL caller (shared CRT helper — keep crt_*)\n")
        md.append("| addr | name | non-AIL callers | AIL callers |")
        md.append("|---|---|---|---|")
        for a, p in sorted(non_ail_caller.items()):
            non_names = list(p["non_ail_callers"].values())
            md.append(f"| `0x{a}` | `{p['name']}` | "
                      f"{', '.join(non_names)} | {', '.join(p['ail_callers'])} |")

    if deferred_zero:
        md.append("\n## Deferred (zero in-degree — orphan, defer to Phase F)\n")
        md.append("| addr | name | reason |")
        md.append("|---|---|---|")
        for a, p in sorted(deferred_zero.items()):
            md.append(f"| `0x{a}` | `{p.get('name','?')}` | {p.get('reason','?')} |")

    OUT_MD.write_text("\n".join(md) + "\n", encoding="utf-8")

    # Console summary
    print(f"candidates:        {len(crt_candidates)}")
    print(f"promoted:          {len(promoted)}")
    print(f"ambiguous:         {len(ambiguous)}")
    print(f"non_ail_caller:    {len(non_ail_caller)}")
    print(f"deferred_zero:     {len(deferred_zero)}")
    print(f"iterations:        {iter_count}")
    print(f"\nwrote:\n  {OUT_JSON.relative_to(REPO)}\n  {OUT_MD.relative_to(REPO)}")


if __name__ == "__main__":
    main()
