"""Validate the latest Ghidra function dump used as source-of-truth.

This script does NOT call MCP itself. The dump is produced by Claude Code
running `mcp__ghidra__list_functions_enhanced(limit=10000)` and writing the
JSON result to `workspace/function_audit/ghidra_dump_<utc>.json`. This
script validates the most recent dump file before `build_worklist.py`
consumes it.

Exits non-zero if no dump is present or its structure is unexpected, so
the per-group startup checklist fails fast and the dump must be retaken.
"""

from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "function_audit"


def find_latest_dump() -> Path | None:
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    dumps = sorted(WORKSPACE.glob("ghidra_dump_*.json"))
    return dumps[-1] if dumps else None


def _unwrap(text: str):
    """Ghidra MCP returns {"result": "...json string..."} or raw JSON."""
    blob = json.loads(text)
    if isinstance(blob, dict) and "result" in blob and isinstance(blob["result"], str):
        return json.loads(blob["result"])
    return blob


def main() -> int:
    dump = find_latest_dump()
    if dump is None:
        print("ERROR: no ghidra_dump_*.json found under workspace/function_audit/")
        print("       Have Claude Code save the latest MCP list_functions_enhanced "
              "output to workspace/function_audit/ghidra_dump_<utc>.json before "
              "running build_worklist.")
        return 2

    text = dump.read_text(encoding="utf-8")
    try:
        payload = _unwrap(text)
    except Exception as e:
        print(f"ERROR: cannot parse {dump.name}: {e}")
        return 3

    if not isinstance(payload, dict) or "functions" not in payload:
        print(f"ERROR: {dump.name} missing top-level 'functions' array")
        return 3

    fns = payload["functions"]
    if not isinstance(fns, list) or not fns:
        print(f"ERROR: {dump.name} has empty functions array")
        return 3

    sample = fns[0]
    required = {"address", "name"}
    missing = required - set(sample.keys())
    if missing:
        print(f"ERROR: {dump.name} entry missing required fields: {missing}")
        return 3

    mtime = datetime.fromtimestamp(dump.stat().st_mtime, tz=timezone.utc)
    print(f"OK  latest dump: {dump.name}")
    print(f"    functions:   {len(fns)}")
    print(f"    mtime (UTC): {mtime.strftime('%Y-%m-%d %H:%M:%S')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
