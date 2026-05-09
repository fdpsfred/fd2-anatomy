"""Phase E — apply Phase D promotions: rename + plate comment.

Reads:
  workspace/ail_audit/phase_d_promotion.json  (default; --source overrides)
  workspace/function_review/registry.json     (for in-degree breakdown + name conflict check)

Outputs:
  workspace/ail_audit/phase_e_rename_log.json — per-action plan, status, plate text
  workspace/ail_audit/phase_e_apply.java       — Ghidra inline script body that renames + sets plate

Usage:
  python tools/ail_audit/apply_renames.py            # dry-run (default), no Java written
  python tools/ail_audit/apply_renames.py --commit   # writes Java apply script for Ghidra MCP

The python side never calls Ghidra MCP itself; the apply Java is invoked separately
via mcp__ghidra__run_script_inline. This keeps the toolchain testable offline.

Phase F orphan reclassifications can re-use this script via:
  python tools/ail_audit/apply_renames.py --source workspace/ail_audit/phase_f_orphans.json --commit
(provided the source JSON has the same `promoted: {addr: {proposed_name, ail_callers, evidence}}` shape.)
"""
from __future__ import annotations

import argparse
import json
from datetime import date
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "ail_audit"
FN_REVIEW = REPO / "workspace" / "function_review"

REG = FN_REVIEW / "registry.json"

DEFAULT_SOURCE = WORK / "phase_d_promotion.json"
OUT_LOG = WORK / "phase_e_rename_log.json"
OUT_JAVA = WORK / "phase_e_apply.java"


PLATE_TEMPLATE = """[AIL internal — promoted by AIL audit {date}]
Original Ghidra name: {old_name}
Promotion evidence:
  - all callers in AIL set: {ail_callers}
{evidence_lines}
Caller in-degree: {in_degree} ({breakdown})
Audit JSON: workspace/ail_audit/phase_d_promotion.json#{addr}"""


def render_plate(addr: str, old_name: str, info: dict, registry: list) -> str:
    callers_list = info["ail_callers"]
    if isinstance(callers_list, dict):
        callers_list = list(callers_list.values())
    in_degree = len(callers_list)
    breakdown = ", ".join(callers_list[:5]) + ("…" if len(callers_list) > 5 else "")
    ev = info.get("evidence", {})
    ev_lines = []
    if ev.get("log_helper_called"):
        ev_lines.append("  - calls AIL_internal_log_print_timestamp_prefix @ 0x3794c")
    if ev.get("ail_globals_referenced"):
        gs = ev["ail_globals_referenced"]
        ev_lines.append(
            f"  - references AIL globals (×{len(gs)}): "
            + ", ".join(f"0x{g}" for g in gs[:5])
            + ("…" if len(gs) > 5 else "")
        )
    if ev.get("ail_strings_referenced"):
        ss = ev["ail_strings_referenced"]
        ev_lines.append(
            f"  - references AIL strings (×{len(ss)}): "
            + ", ".join(f"0x{s}" for s in ss[:5])
            + ("…" if len(ss) > 5 else "")
        )
    return PLATE_TEMPLATE.format(
        date=date.today().isoformat(),
        old_name=old_name,
        ail_callers=", ".join(callers_list),
        evidence_lines="\n".join(ev_lines),
        in_degree=in_degree,
        breakdown=breakdown,
        addr=addr,
    )


def java_escape(s: str) -> str:
    out = []
    for ch in s:
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif ch == '\t':
            out.append('\\t')
        elif ord(ch) < 0x20:
            out.append(f'\\u{ord(ch):04x}')
        else:
            out.append(ch)
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    ap.add_argument("--commit", action="store_true")
    args = ap.parse_args()

    source = args.source
    promotions = json.loads(source.read_text(encoding="utf-8"))["promoted"]
    registry = json.loads(REG.read_text(encoding="utf-8"))
    name_to_addr = {r["current_name"]: r["address"] for r in registry}
    addr_to_name = {r["address"]: r["current_name"] for r in registry}

    actions = []
    for addr, info in sorted(promotions.items()):
        new_name = info["proposed_name"]
        old_name = info.get("name") or addr_to_name.get(addr)
        # Naming conflict check
        if new_name in name_to_addr and name_to_addr[new_name] != addr:
            actions.append({
                "addr": addr,
                "old_name": old_name,
                "new_name": new_name,
                "status": "skipped_name_conflict",
                "conflict_with_addr": name_to_addr[new_name],
            })
            continue
        plate = render_plate(addr, old_name, info, registry)
        actions.append({
            "addr": addr,
            "old_name": old_name,
            "new_name": new_name,
            "status": "planned",
            "plate": plate,
        })

    OUT_LOG.write_text(json.dumps({
        "_meta": {
            "source": str(source.relative_to(REPO)),
            "commit": args.commit,
            "action_count": len(actions),
            "planned_count": sum(1 for a in actions if a["status"] == "planned"),
            "skipped_count": sum(1 for a in actions if a["status"].startswith("skipped")),
        },
        "actions": actions,
    }, indent=2, ensure_ascii=False), encoding="utf-8")

    if args.commit:
        java_lines = []
        java_lines.append("import ghidra.program.model.listing.Function;")
        java_lines.append("import ghidra.program.model.listing.FunctionManager;")
        java_lines.append("import ghidra.program.model.address.Address;")
        java_lines.append("import ghidra.program.model.symbol.SourceType;")
        java_lines.append("")
        java_lines.append("FunctionManager fm = currentProgram.getFunctionManager();")
        java_lines.append("int applied = 0;")
        java_lines.append("int errors = 0;")
        for a in actions:
            if a["status"] != "planned":
                continue
            addr = a["addr"]
            new_name = java_escape(a["new_name"])
            plate = java_escape(a["plate"])
            java_lines.append("")
            java_lines.append("try {")
            java_lines.append(
                f'  Address a = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress("{addr}");'
            )
            java_lines.append(f"  Function f = fm.getFunctionAt(a);")
            java_lines.append("  if (f == null) {")
            java_lines.append(f'    println("MISSING addr {addr}"); errors++;')
            java_lines.append("  } else {")
            java_lines.append(f'    f.setName("{new_name}", SourceType.USER_DEFINED);')
            java_lines.append(f'    f.setComment("{plate}");')
            java_lines.append(f'    println("renamed " + a.toString() + " -> {new_name}");')
            java_lines.append("    applied++;")
            java_lines.append("  }")
            java_lines.append("} catch (Exception ex) {")
            java_lines.append(f'  println("ERROR at {addr}: " + ex.getMessage());')
            java_lines.append("  errors++;")
            java_lines.append("}")
        java_lines.append("")
        java_lines.append('println("=== applied: " + applied + " ; errors: " + errors + " ===");')
        OUT_JAVA.write_text("\n".join(java_lines) + "\n", encoding="utf-8")
        print(f"wrote: {OUT_JAVA.relative_to(REPO)} ({sum(1 for a in actions if a['status']=='planned')} apply blocks)")

    print(f"actions:           {len(actions)}")
    print(f"  planned:         {sum(1 for a in actions if a['status']=='planned')}")
    print(f"  skipped:         {sum(1 for a in actions if a['status'].startswith('skipped'))}")
    print(f"\nwrote: {OUT_LOG.relative_to(REPO)}")
    if not args.commit:
        print("(dry-run; pass --commit to also write the Ghidra apply Java)")


if __name__ == "__main__":
    main()
