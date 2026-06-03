#!/usr/bin/env python3
"""Compute the current Claude Code session's context window usage."""
import json
import os
import sys
from pathlib import Path


def find_transcript(session_id: str) -> Path | None:
    base = Path.home() / ".claude" / "projects"
    if not base.is_dir():
        return None
    for p in base.rglob(f"{session_id}.jsonl"):
        if p.is_file():
            return p
    return None


def last_assistant_usage(path: Path) -> dict | None:
    last = None
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            try:
                e = json.loads(line)
            except json.JSONDecodeError:
                continue
            if e.get("type") != "assistant":
                continue
            if e.get("isSidechain"):
                continue
            usage = (e.get("message") or {}).get("usage")
            if not usage:
                continue
            last = usage
    return last


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({"error": "missing session_id argument"}))
        return

    sid = sys.argv[1]
    max_ctx = int(os.environ.get("MAX", "200000"))

    path = find_transcript(sid)
    if path is None:
        print(json.dumps({"error": "transcript not found", "session_id": sid}))
        return

    usage = last_assistant_usage(path)
    if usage is None:
        print(json.dumps({"error": "no assistant usage yet", "transcript": str(path)}))
        return

    inp = usage.get("input_tokens") or 0
    cr = usage.get("cache_read_input_tokens") or 0
    cc = usage.get("cache_creation_input_tokens") or 0
    total = inp + cr + cc

    print(json.dumps({
        "transcript": str(path),
        "input_tokens": inp,
        "cache_read": cr,
        "cache_creation": cc,
        "total_context_tokens": total,
        "max_context": max_ctx,
        "used_percentage": round(total * 100 / max_ctx, 1),
    }))


if __name__ == "__main__":
    main()
