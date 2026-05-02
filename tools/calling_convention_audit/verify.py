"""Phase 5 — confirm all recommendations applied; spot-check decompile output.

Pipeline phase: this is step 5 of 5 in the calling-convention audit. See
`_index.md` in this directory for the full pipeline.

I/O (workdir defaults to `<repo>/workspace/calling_convention_audit/`):
    reads:  recommendations.json, audit_after.json (re-run of dump after Phase 4)
    writes: verify_report.json
            progress.json   (phase -> done if all checks pass)

Usage:
    python tools/calling_convention_audit/verify.py [--workdir DIR]

The audit_after.json is produced by re-running ghidra_dump.java pointing
WORKDIR_OUT to audit_after.json (see ghidra_dump.java `OUT_PATH` constant).
The orchestrator (Claude / human operator) does that step; this script does
the diff + reporting.

Pass criteria:
    1. For every recommendation, audit_after must show that function's
       current_cc == recommended_cc (zero mismatches).
    2. Pinned addresses (0x36cd7, 0x4b502) must have their target cc.
    3. Renamed functions (recommendations.json suggested_name) must show the
       new name in audit_after.
"""

from __future__ import annotations

import argparse
import collections
import datetime
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WORKDIR = REPO_ROOT / "workspace" / "calling_convention_audit"

PINNED = {"0x00036cd7": "__stdcall", "0x0004b502": "__fastcall"}
EXPECTED_RENAME = ("0x00017ee8", "wrapper_clear_keyboard_buffer")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", type=Path, default=DEFAULT_WORKDIR,
                    help=f"workdir (default: {DEFAULT_WORKDIR})")
    args = ap.parse_args()
    workdir = args.workdir

    rec_path = workdir / "recommendations.json"
    audit_after_path = workdir / "audit_after.json"
    verify_path = workdir / "verify_report.json"
    progress_path = workdir / "progress.json"

    if not rec_path.exists():
        sys.exit(f"{rec_path} missing")
    if not audit_after_path.exists():
        sys.exit(f"missing {audit_after_path} — re-run ghidra_dump.java with OUT_PATH set to audit_after.json first")

    rec_doc = json.loads(rec_path.read_text(encoding="utf-8"))
    audit_after = json.loads(audit_after_path.read_text(encoding="utf-8"))
    rec_by_addr = {r["addr"]: r for r in rec_doc["recommendations"]}
    after_by_addr = {f["addr"]: f for f in audit_after["functions"]}

    mismatches = []
    pinned_ok = {}
    name_check = {}

    for addr, rec in rec_by_addr.items():
        after = after_by_addr.get(addr)
        if after is None:
            mismatches.append({"addr": addr, "issue": "function disappeared from audit_after"})
            continue
        expect_cc = rec["recommended_cc"]
        actual_cc = after["current_cc"]
        if expect_cc != actual_cc:
            mismatches.append({
                "addr": addr,
                "name": after["name"],
                "expected_cc": expect_cc,
                "actual_cc": actual_cc,
                "reason": rec["reason"],
            })

        if rec["suggested_name"]:
            name_check[addr] = {
                "expected_name": rec["suggested_name"],
                "actual_name": after["name"],
                "ok": after["name"] == rec["suggested_name"],
            }

        if addr in PINNED:
            pinned_ok[addr] = {
                "expected_cc": PINNED[addr],
                "actual_cc": actual_cc,
                "ok": actual_cc == PINNED[addr],
            }

    cc_dist_after = collections.Counter(f["current_cc"] for f in audit_after["functions"])

    spot_addrs = []
    for cc in ("__cdecl", "__fastcall", "__stdcall"):
        chosen = [f for f in audit_after["functions"] if f["current_cc"] == cc][:3]
        spot_addrs.extend([f["addr"] for f in chosen])
    spot_addrs = list(dict.fromkeys(spot_addrs))[:10]

    expected_rename_ok = (after_by_addr.get(EXPECTED_RENAME[0], {}).get("name") == EXPECTED_RENAME[1])

    report = {
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "total": len(rec_by_addr),
        "mismatches_count": len(mismatches),
        "mismatches": mismatches[:200],
        "pinned": pinned_ok,
        "rename_check": name_check,
        "expected_rename_check": {
            "addr": EXPECTED_RENAME[0],
            "expected": EXPECTED_RENAME[1],
            "ok": expected_rename_ok,
        },
        "cc_dist_after": dict(cc_dist_after),
        "cc_dist_recommended": rec_doc["recommended_cc_dist"],
        "spot_check_addrs": spot_addrs,
    }
    verify_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"VERIFY: total={len(rec_by_addr)} mismatches={len(mismatches)}")
    print(f"  cc_dist_after        = {dict(cc_dist_after)}")
    print(f"  cc_dist_recommended  = {rec_doc['recommended_cc_dist']}")
    for addr, info in pinned_ok.items():
        print(f"  pinned {addr}: expect={info['expected_cc']} actual={info['actual_cc']} ok={info['ok']}")
    print(f"  expected rename ok = {expected_rename_ok}")
    print(f"  spot_check_addrs   = {spot_addrs}")

    progress = json.loads(progress_path.read_text(encoding="utf-8"))
    if len(mismatches) == 0 and all(p["ok"] for p in pinned_ok.values()) and expected_rename_ok:
        progress["phase"] = "done"
        progress["updated_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
        progress_path.write_text(json.dumps(progress, indent=2, ensure_ascii=False), encoding="utf-8")
        print("ALL CHECKS PASSED -> phase=done")
    else:
        print("VERIFICATION ISSUES — see verify_report.json")
        sys.exit(1)


if __name__ == "__main__":
    main()
