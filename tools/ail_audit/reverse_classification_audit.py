"""Reverse misclassification audit: find crt_* functions whose callers are all
AIL_* and whose disasm shows AIL evidence — these are AIL helpers misnamed as
crt_* during prior placeholder naming.

This complements xref_source_audit.py (which checks AIL functions for non-AIL
xrefs). Together they cover both directions of misclassification.

Reads:
  workspace/function_review/registry.json

Writes:
  workspace/ail_audit/reverse_classification_report.json
  workspace/ail_audit/reverse_classification_report.md

Flag rules (apply to every function whose name starts with `crt_` or
`align_nop_*` is excluded):
  ail_only_callers — all live callers (excluding align_nop_*) are AIL_*-named
  game_or_crt_callers — has at least one non-AIL caller, NOT a flag
"""
from __future__ import annotations

import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
REG = REPO / "workspace" / "function_review" / "registry.json"
CRT_LOOKUP = REPO / "rebuild_info" / "crt_lookup_9.5a.json"
OUT_JSON = REPO / "workspace" / "ail_audit" / "reverse_classification_report.json"
OUT_MD = REPO / "workspace" / "ail_audit" / "reverse_classification_report.md"


def main():
    registry = json.loads(REG.read_text(encoding="utf-8"))
    crt = json.loads(CRT_LOOKUP.read_text(encoding="utf-8"))
    crt_lookup_addrs = set(crt["by_address"].keys())

    reg_by_addr = {r["address"]: r for r in registry}

    flagged = []
    skipped_no_callers = []
    skipped_in_lookup = []

    for r in registry:
        name = r["current_name"]
        if not name.startswith("crt_"):
            continue
        addr = r["address"]
        # Skip CRT functions that are formally verified in lookup table
        if addr in crt_lookup_addrs:
            skipped_in_lookup.append({"addr": addr, "name": name})
            continue
        # Get live callers (excluding align_nop_* and self)
        callers = r.get("callers", [])
        live_callers = [
            c for c in callers
            if c != addr
            and not (reg_by_addr.get(c, {}).get("current_name", "").startswith("align_nop_"))
        ]
        if not live_callers:
            skipped_no_callers.append({"addr": addr, "name": name})
            continue
        # Check if all callers are AIL_*
        caller_classes = {}
        for c in live_callers:
            caller_name = reg_by_addr.get(c, {}).get("current_name", "?")
            if caller_name.startswith("AIL_"):
                caller_classes.setdefault("ail", []).append(caller_name)
            elif caller_name.startswith("crt_"):
                caller_classes.setdefault("crt", []).append(caller_name)
            else:
                caller_classes.setdefault("other", []).append(caller_name)
        if "other" in caller_classes:
            continue  # has game caller, not AIL-only
        if "crt" in caller_classes and "ail" not in caller_classes:
            continue  # all callers are crt — pure CRT, not flagged
        if "ail" in caller_classes:
            flagged.append({
                "addr": addr,
                "name": name,
                "ail_callers": caller_classes.get("ail", []),
                "crt_callers": caller_classes.get("crt", []),
                "live_caller_count": len(live_callers),
            })

    OUT_JSON.write_text(json.dumps({
        "_meta": {
            "crt_total": sum(1 for r in registry if r["current_name"].startswith("crt_")),
            "skipped_in_lookup": len(skipped_in_lookup),
            "skipped_no_callers": len(skipped_no_callers),
            "flagged": len(flagged),
        },
        "flagged": flagged,
        "skipped_no_callers": skipped_no_callers,
    }, indent=2, ensure_ascii=False), encoding="utf-8")

    md = ["# crt_* functions with all-AIL callers — reverse-classification audit\n"]
    md.append(f"檢查所有 `crt_*` (非 lookup-confirmed) 函式的 caller 集合，flag 那些")
    md.append(f"caller 全部來自 `AIL_*` namespace 的（可能是 AIL helper 被誤命名為 crt_*）。\n")
    md.append("## 統計\n")
    md.append(f"- crt_* 總數: {sum(1 for r in registry if r['current_name'].startswith('crt_'))}")
    md.append(f"- formally-verified (in crt_lookup_9.5a.json): {len(skipped_in_lookup)}")
    md.append(f"- 0 in-degree (orphan placeholder): {len(skipped_no_callers)}")
    md.append(f"- **flagged (all-AIL callers, 候選 reclassify)**: {len(flagged)}")
    md.append("")

    if flagged:
        md.append(f"\n## flagged crt_* 函式（{len(flagged)} 個）— 需 disasm 驗證\n")
        md.append("| addr | crt name | AIL callers | crt callers (cluster) |")
        md.append("|---|---|---|---|")
        for f in flagged:
            ail_summary = ", ".join(f["ail_callers"][:3]) + ("…" if len(f["ail_callers"]) > 3 else "")
            crt_summary = ", ".join(f["crt_callers"][:3]) + ("…" if len(f["crt_callers"]) > 3 else "")
            md.append(f"| `0x{f['addr']}` | `{f['name']}` | {ail_summary or '(none)'} | {crt_summary or '(none)'} |")

    OUT_MD.write_text("\n".join(md) + "\n", encoding="utf-8")

    print(f"crt_* total: {sum(1 for r in registry if r['current_name'].startswith('crt_'))}")
    print(f"in lookup (skipped): {len(skipped_in_lookup)}")
    print(f"orphan crt_* (zero in-degree, skipped): {len(skipped_no_callers)}")
    print(f"flagged for reclassification review: {len(flagged)}")
    print(f"\nwrote: {OUT_MD.relative_to(REPO)}")


if __name__ == "__main__":
    main()
