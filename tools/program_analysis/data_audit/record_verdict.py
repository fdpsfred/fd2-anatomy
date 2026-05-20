"""Append a per-data-item audit verdict to verdicts.jsonl and sync worklist.

Each verdict marks one data item as clean / fixed / deferred during the
data-audit structural review. The script is a pure sink: it never derives a
verdict from data, only transcribes the verdict the reviewer (Claude
Code, after personally inspecting bytes/inspect/caller-decomp) hands in.

Usage:

    python tools/program_analysis/data_audit/record_verdict.py <addr> <verdict_json>

Where `<verdict_json>` is a single-line JSON object:

    {"status":"clean", "data_kind":"padding", "actions":[],
     "notes":"align_fill_3c962 already correct"}

    {"status":"fixed", "data_kind":"array",
     "actions":["rename_data","apply_data_type","set_decompiler_comment"],
     "struct_type":"item_entry", "parent_struct":"data_fd2_battle_item_effect_table",
     "access_kinds":["read"], "caller_pool":"fd2", "subsystem":"battle",
     "problem_classes":[1,2,5],
     "notes":"item_effect_table renamed to data_fd2_battle_item_effect_table"}

    {"status":"deferred", "data_kind":"unknown",
     "actions":[], "problem_classes":[4],
     "notes":"large blob @0x36a02 needs LE FIXUP reverse lookup; pending parse_le_fixup"}

After append, the script:
1. Echoes a progress line
   `[D{N} {label}] n/N_group  total m/M  | <addr> <old>→<new> → <status>`
   to stdout
2. Updates the matching row in worklist.json (status / problem_classes /
   verdict / notes / data_kind / struct_type / access_kinds / caller_pool /
   subsystem if provided)
3. Returns 0 on success, non-zero if the addr is not in the worklist
"""

from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "data_audit"
WORKLIST = WORKSPACE / "worklist.json"
VERDICTS = WORKSPACE / "verdicts.jsonl"

VALID_STATUS = {"clean", "fixed", "deferred"}


def normalise_addr(s: str) -> str:
    s = s.strip().lower()
    if s.startswith("0x"):
        s = s[2:]
    return s.zfill(8)


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("usage: record_verdict.py <addr> <verdict_json>", file=sys.stderr)
        return 2

    addr = normalise_addr(argv[1])
    try:
        verdict = json.loads(argv[2])
    except json.JSONDecodeError as e:
        print(f"ERROR: verdict_json invalid: {e}", file=sys.stderr)
        return 3

    status = verdict.get("status")
    if status not in VALID_STATUS:
        print(f"ERROR: status must be one of {VALID_STATUS}, got {status!r}",
              file=sys.stderr)
        return 3

    if not WORKLIST.exists():
        print(f"ERROR: {WORKLIST} not found; run build_worklist.py first",
              file=sys.stderr)
        return 4
    blob = json.loads(WORKLIST.read_text(encoding="utf-8"))

    target_row = None
    for row in blob["rows"]:
        if row["addr"] == addr:
            target_row = row
            break
    if target_row is None:
        print(f"ERROR: addr {addr} not found in worklist", file=sys.stderr)
        return 4

    WORKSPACE.mkdir(parents=True, exist_ok=True)
    log_entry = {
        "ts": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "addr": addr,
        "old_name": target_row.get("previous_name") or target_row["current_name"],
        "new_name": verdict.get("new_name", target_row["current_name"]),
        "segment": target_row["segment"],
        "bucket": target_row["bucket"],
        "group": target_row["group"],
        "group_label": target_row["group_label"],
        "status": status,
        "data_kind": verdict.get("data_kind"),
        "struct_type": verdict.get("struct_type"),
        "parent_struct": verdict.get("parent_struct"),
        "parent_function": verdict.get("parent_function"),
        "access_kinds": verdict.get("access_kinds", []),
        "caller_pool": verdict.get("caller_pool",
                                   target_row.get("caller_pool")),
        "subsystem": verdict.get("subsystem", target_row.get("subsystem")),
        "problem_classes": verdict.get("problem_classes", []),
        "actions": verdict.get("actions", []),
        "indirection_chase_method": verdict.get("indirection_chase_method"),
        "notes": verdict.get("notes", ""),
    }
    with VERDICTS.open("a", encoding="utf-8") as f:
        f.write(json.dumps(log_entry, ensure_ascii=False) + "\n")

    target_row["status"] = status
    target_row["problem_classes"] = verdict.get("problem_classes", [])
    target_row["verdict"] = {
        "data_kind": verdict.get("data_kind"),
        "struct_type": verdict.get("struct_type"),
        "parent_struct": verdict.get("parent_struct"),
        "parent_function": verdict.get("parent_function"),
        "access_kinds": verdict.get("access_kinds", []),
        "actions": verdict.get("actions", []),
        "indirection_chase_method": verdict.get("indirection_chase_method"),
        "notes": verdict.get("notes", ""),
        "ts": log_entry["ts"],
    }
    if "new_name" in verdict and verdict["new_name"] != target_row["current_name"]:
        target_row["previous_name"] = target_row["current_name"]
        target_row["current_name"] = verdict["new_name"]
    if verdict.get("caller_pool"):
        target_row["caller_pool"] = verdict["caller_pool"]
    if verdict.get("subsystem"):
        target_row["subsystem"] = verdict["subsystem"]
    if verdict.get("notes"):
        target_row["notes"] = verdict["notes"]

    WORKLIST.write_text(json.dumps(blob, indent=2, ensure_ascii=False), encoding="utf-8")

    group = target_row["group"]
    group_label = target_row["group_label"]
    group_rows = [r for r in blob["rows"]
                  if r["group"] == group and r["status"] != "deleted_in_ghidra"]
    n_done_group = sum(1 for r in group_rows if r["status"] != "pending")
    n_total_group = len(group_rows)
    all_live = [r for r in blob["rows"] if r["status"] != "deleted_in_ghidra"]
    m_done_total = sum(1 for r in all_live if r["status"] != "pending")
    m_total = len(all_live)

    old_disp = log_entry["old_name"]
    new_disp = log_entry["new_name"]
    rename_arrow = f"{old_disp}→{new_disp}" if old_disp != new_disp else old_disp
    actions = verdict.get("actions", [])
    suffix = f" ・修:{','.join(actions)}" if actions and status == "fixed" else ""
    print(f"[{group} {group_label}] {n_done_group}/{n_total_group}  "
          f"total {m_done_total}/{m_total}  | "
          f"{addr} {rename_arrow} → {status}{suffix}")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
