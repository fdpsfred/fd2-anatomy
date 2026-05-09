"""Phase G — Build canonical AIL ecosystem function inventory.

Reads the post-Phase-E registry + Phase B/C/D/F outputs to emit a flat list
of every AIL function with its final category and an evidence pointer back
into the audit JSONs.

Output:
  workspace/ail_audit/ail_function_inventory.json
"""
from __future__ import annotations

import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "ail_audit"
FN_REVIEW = REPO / "workspace" / "function_review"

REG = FN_REVIEW / "registry.json"
PHASE_B = WORK / "phase_b_public_verify.json"
PHASE_C = WORK / "phase_c_reachable.json"
PHASE_D = WORK / "phase_d_promotion.json"
PHASE_F = WORK / "phase_f_orphans.json"

OUT = WORK / "ail_function_inventory.json"


def main():
    registry = json.loads(REG.read_text(encoding="utf-8"))
    phase_b = json.loads(PHASE_B.read_text(encoding="utf-8"))
    phase_c = json.loads(PHASE_C.read_text(encoding="utf-8"))
    phase_d = json.loads(PHASE_D.read_text(encoding="utf-8"))
    phase_f = json.loads(PHASE_F.read_text(encoding="utf-8"))

    pb_by_addr = {r["address"]: r for r in phase_b["records"]}
    promoted_addrs = set(phase_d["promoted"].keys())
    orphan_by_addr = phase_f["orphans"]
    reached = set(phase_c["reachable"].keys())

    inventory = []
    for r in registry:
        name = r["current_name"]
        if not (name.startswith("AIL_") or name.startswith("AIL_internal_")):
            continue
        addr = r["address"]

        # Determine final category + evidence pointer
        if not name.startswith("AIL_internal_"):
            # Public
            verdict = pb_by_addr.get(addr, {}).get("verdict")
            category = "public"
            evidence_ref = f"phase_b_public_verify.json#{addr}"
            sub_class = verdict
        elif addr in promoted_addrs:
            category = "promoted_internal"
            evidence_ref = f"phase_d_promotion.json#{addr}"
            sub_class = "phase_d_promotion"
        elif addr in orphan_by_addr:
            category = "orphan_internal"
            evidence_ref = f"phase_f_orphans.json#{addr}"
            sub_class = orphan_by_addr[addr]["sub_case"]
        elif addr in reached:
            category = "internal_reached"
            evidence_ref = f"phase_c_reachable.json#{addr}"
            sub_class = "ail_internal_confirmed"
        else:
            # Orphan but somehow not in phase_f json (shouldn't happen)
            category = "internal_unclassified"
            evidence_ref = None
            sub_class = None

        inventory.append({
            "addr": addr,
            "name": name,
            "category": category,
            "sub_class": sub_class,
            "evidence_ref": evidence_ref,
            "is_thunk": r.get("is_thunk", False),
            "callers_count": r.get("callers_count", 0),
            "callees_count": r.get("callees_count", 0),
        })

    inventory.sort(key=lambda x: x["addr"])

    cat_counts = {}
    for e in inventory:
        cat_counts[e["category"]] = cat_counts.get(e["category"], 0) + 1

    OUT.write_text(json.dumps({
        "_meta": {
            "ail_total": len(inventory),
            "category_counts": cat_counts,
            "audit_date": "2026-05-09",
        },
        "functions": inventory,
    }, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"AIL ecosystem total: {len(inventory)}")
    for cat, cnt in sorted(cat_counts.items()):
        print(f"  {cat:25s} {cnt}")
    print(f"\nwrote: {OUT.relative_to(REPO)}")


if __name__ == "__main__":
    main()
