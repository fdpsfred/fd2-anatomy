"""Validate the latest Ghidra data-item dump used as source-of-truth.

This script does NOT call MCP itself. The dump is produced by Claude Code
running `mcp__ghidra__list_data_items(limit=10000)` (paginated if the live
program reports >10000 items) and writing the result to
`workspace/data_audit/ghidra_data_dump_<utc>.json`.

Schema expected (a JSON dict):

    {
      "items": [
        {"addr": "00010000", "name": "BYTE_ARRAY_00010000",
         "type": "byte[16]", "size": 16, "segment": ".object1"},
        ...
      ]
    }

Per-row schema is normalised by the dump producer (Claude Code), which
parses the textual `list_data_items` output of the form
`<name> @ <addr> [<type>] (<size> bytes)` and joins each item with the
segment from `list_segments`. This script validates the most recent dump
file before `build_worklist.py` consumes it.

Exits non-zero if no dump is present or its structure is unexpected so
the per-group startup checklist fails fast and the dump must be retaken.
"""

from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "data_audit"


def find_latest_dump() -> Path | None:
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    dumps = sorted(WORKSPACE.glob("ghidra_data_dump_*.json"))
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
        print("ERROR: no ghidra_data_dump_*.json found under workspace/data_audit/")
        print("       Have Claude Code save the latest MCP list_data_items "
              "output to workspace/data_audit/ghidra_data_dump_<utc>.json "
              "before running build_worklist.")
        return 2

    text = dump.read_text(encoding="utf-8")
    try:
        payload = _unwrap(text)
    except Exception as e:
        print(f"ERROR: cannot parse {dump.name}: {e}")
        return 3

    if not isinstance(payload, dict) or "items" not in payload:
        print(f"ERROR: {dump.name} missing top-level 'items' array")
        return 3

    items = payload["items"]
    if not isinstance(items, list) or not items:
        print(f"ERROR: {dump.name} has empty items array")
        return 3

    sample = items[0]
    required = {"addr", "name", "type", "size", "segment"}
    missing = required - set(sample.keys())
    if missing:
        print(f"ERROR: {dump.name} entry missing required fields: {missing}")
        return 3

    # Segment distribution sanity
    seg_counts: dict[str, int] = {}
    for it in items:
        seg_counts[it["segment"]] = seg_counts.get(it["segment"], 0) + 1

    mtime = datetime.fromtimestamp(dump.stat().st_mtime, tz=timezone.utc)
    print(f"OK  latest dump: {dump.name}")
    print(f"    items:       {len(items)}")
    print(f"    mtime (UTC): {mtime.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"    segments:")
    for seg in sorted(seg_counts):
        print(f"      {seg:12s}  {seg_counts[seg]:4d}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
