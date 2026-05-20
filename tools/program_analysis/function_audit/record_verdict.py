"""Append a per-function audit verdict to verdicts.jsonl and sync worklist.

Each verdict marks one function as clean / fixed / deferred during the
function-audit structural review. The script is a pure sink: it never
derives a verdict from data, only transcribes the verdict the reviewer
(Claude Code, after personally inspecting disasm/decomp/plate/xrefs)
hands in.

Usage:

    python tools/program_analysis/function_audit/record_verdict.py <addr> <verdict_json>

Where `<verdict_json>` is a single-line JSON object like:

    {"status":"clean","problem_classes":[],"actions":[],"notes":""}
    {"status":"fixed","problem_classes":[2,5],
     "actions":["set_function_prototype","set_plate_comment"],
     "notes":"cc was stdcall but body is watcall; plate cleared phase tags"}
    {"status":"deferred","problem_classes":[3],"actions":[],
     "notes":"shared prologue with 0xABCDE; need user decision on split"}

After append, the script:
1. Echoes a progress line `[G{N} {label}] n/N_group  total m/M  | <addr> <name> → <status>` to stdout
2. Updates the matching row in worklist.json (status / problem_classes / verdict / notes)
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
WORKSPACE = REPO / "workspace" / "function_audit"
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

    # Append to verdicts.jsonl (idempotent: every record_verdict call yields one line;
    # later re-verdict on same addr overwrites status in worklist but jsonl keeps history)
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    log_entry = {
        "ts": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "addr": addr,
        "name": target_row["current_name"],
        "group": target_row["group"],
        "group_label": target_row["group_label"],
        "status": status,
        "problem_classes": verdict.get("problem_classes", []),
        "actions": verdict.get("actions", []),
        "notes": verdict.get("notes", ""),
    }
    with VERDICTS.open("a", encoding="utf-8") as f:
        f.write(json.dumps(log_entry, ensure_ascii=False) + "\n")

    # Update worklist row
    target_row["status"] = status
    target_row["problem_classes"] = verdict.get("problem_classes", [])
    target_row["verdict"] = {
        "actions": verdict.get("actions", []),
        "notes": verdict.get("notes", ""),
        "ts": log_entry["ts"],
    }
    if verdict.get("notes"):
        target_row["notes"] = verdict["notes"]

    WORKLIST.write_text(json.dumps(blob, indent=2, ensure_ascii=False), encoding="utf-8")

    # Compute progress: n in this group (non-pending), total non-pending
    group = target_row["group"]
    group_label = target_row["group_label"]
    group_rows = [r for r in blob["rows"]
                  if r["group"] == group and r["status"] != "deleted_in_ghidra"]
    n_done_group = sum(1 for r in group_rows if r["status"] != "pending")
    n_total_group = len(group_rows)
    all_live = [r for r in blob["rows"] if r["status"] != "deleted_in_ghidra"]
    m_done_total = sum(1 for r in all_live if r["status"] != "pending")
    m_total = len(all_live)

    actions = verdict.get("actions", [])
    suffix = f" ・{','.join(actions)}" if actions and status == "fixed" else ""
    print(f"[{group} {group_label}] {n_done_group}/{n_total_group}  "
          f"total {m_done_total}/{m_total}  | "
          f"{addr} {target_row['current_name']} → {status}{suffix}")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
