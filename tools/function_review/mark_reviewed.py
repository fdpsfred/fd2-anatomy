"""Apply human review verdicts to the registry.

Reads a verdicts JSON (path passed as argv[1] or
workspace/function_review/inbox.json by default) and merges the verdicts
into workspace/function_review/registry.json. Each verdict has:

    {"address": "000379ee",
     "confirmed_name": "AIL_startup",
     "final_category": "ail",
     "notes": "name verified by debug string \"AIL_startup()\\n\" @ 0x5042e",
     "tentative_system": "audio"}    # optional

If `confirmed_name` differs from the row's current `current_name`, this
script does NOT rename in Ghidra — that must be done via the
rename_function_by_address MCP tool. This script only records the human
verdict in the registry, marks review_status=done, and reports any
mismatches so the operator can confirm Ghidra is in sync.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
REGISTRY = REPO / "workspace" / "function_review" / "registry.json"
DEFAULT_INBOX = REPO / "workspace" / "function_review" / "inbox.json"


def main(inbox_path: str | None):
    inbox = Path(inbox_path) if inbox_path else DEFAULT_INBOX
    if not inbox.exists():
        sys.exit(f"verdicts file not found: {inbox}")

    verdicts = json.loads(inbox.read_text(encoding="utf-8"))
    if not isinstance(verdicts, list):
        sys.exit("verdicts file must be a JSON list")

    registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
    by_addr = {r["address"]: r for r in registry}

    applied, name_diffs, missing = 0, [], []

    for v in verdicts:
        addr = v["address"].lower()
        row = by_addr.get(addr)
        if row is None:
            missing.append(addr)
            continue
        if "confirmed_name" in v and v["confirmed_name"] != row["current_name"]:
            name_diffs.append((addr, row["current_name"], v["confirmed_name"]))
        for k in ("confirmed_name", "final_category", "tentative_system", "notes"):
            if k in v:
                row[k] = v[k]
        row["review_status"] = "done"
        applied += 1

    REGISTRY.write_text(
        json.dumps(registry, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    print(f"applied: {applied}")
    if missing:
        print(f"missing addresses ({len(missing)}): {missing}")
    if name_diffs:
        print(f"name diffs ({len(name_diffs)}) — confirm Ghidra rename matches:")
        for addr, cur, want in name_diffs:
            print(f"  {addr}  current={cur}  verdict={want}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else None)
