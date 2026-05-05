"""Phase F dry-run planner for orphan / mis-classified-data fixup.

Reads `workspace/function_review/phase_f_candidates.json` and prints what the
Java worker (`fixup_orphans.java`) WOULD do, without touching Ghidra. Use this
to sanity-check the candidate list and prologue-detection heuristic before
running the real worker.

Usage:

    python tools/function_review/fixup_orphans.py

The actual fixup is performed by `fixup_orphans.java`, executed via Ghidra's
`run_script_inline` MCP tool.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CANDIDATES = REPO / "workspace" / "function_review" / "phase_f_candidates.json"


# Same prologue detection as Java worker — keep in sync.
def looks_like_prologue(buf: bytes) -> bool:
    if len(buf) < 2:
        return False
    b0 = buf[0]
    b1 = buf[1] if len(buf) > 1 else -1
    b2 = buf[2] if len(buf) > 2 else -1
    b3 = buf[3] if len(buf) > 3 else -1
    b4 = buf[4] if len(buf) > 4 else -1

    rules = [
        (b0 == 0x56 and b1 == 0x57 and b2 == 0x55, "PUSH ESI;EDI;EBP"),
        (b0 == 0x57 and b1 == 0x55, "PUSH EDI;EBP"),
        (b0 == 0x53 and b1 == 0x55 and b2 == 0x89 and b3 == 0xE5, "PUSH EBX;EBP;MOV EBP,ESP"),
        (b0 == 0x55 and b1 == 0x89 and b2 == 0xE5, "PUSH EBP;MOV EBP,ESP"),
        (b0 == 0x53 and b1 == 0x56 and b2 == 0x55 and b3 == 0x89 and b4 == 0xE5, "PUSH EBX;ESI;EBP;MOV EBP,ESP"),
        (b0 == 0x57 and b1 == 0x56 and b2 == 0x52 and b3 == 0x51 and b4 == 0x53, "soft-FP register save"),
        (b0 == 0x56 and b1 == 0x57 and b2 == 0x52 and b3 == 0x53, "PUSH ESI;EDI;EDX;EBX"),
        (b0 == 0x8D and b1 == 0x80 and b2 == 0 and b3 == 0 and b4 == 0, "6-byte align NOP"),
        (b0 == 0x8D and b1 == 0x40 and b2 == 0, "3-byte align NOP"),
        (b0 == 0x8D and b1 == 0x44 and b2 == 0x20 and b3 == 0, "4-byte align NOP"),
        (b0 == 0x8B and b1 == 0x44 and b2 == 0x24, "MOV EAX,[ESP+disp]"),
        (b0 == 0x8B and b1 == 0x06, "MOV EAX,[ESI]"),
        (b0 == 0x8B and b1 == 0x42, "MOV EAX,[EDX+disp]"),
        (b0 == 0x8B and b1 == 0xC0, "MOV EAX,EAX"),
        (b0 == 0x68, "PUSH imm32"),
        (b0 == 0x6A and b1 == 0x01, "PUSH 1"),
        (b0 == 0xC7 and b1 == 0x05, "MOV [imm32], imm32"),
        (b0 == 0x3B and b1 == 0x35, "CMP ESI, [imm32]"),
        (b0 == 0x80 and b1 == 0x3D, "CMP byte ptr [imm32]"),
        (b0 == 0x1E and b1 == 0x06, "PUSH DS;ES (DOS)"),
        (b0 == 0x60, "PUSHA"),
        (b0 == 0xB0 and b1 == 0x03 and b2 == 0x55, "MOV AL,3;PUSH EBP"),
    ]
    for ok, _ in rules:
        if ok:
            return True
    if b0 == 0x90:
        for i in range(1, min(8, len(buf))):
            if buf[i] in (0x55, 0x56, 0x57, 0x53):
                return True
    if b0 == 0 and b1 == 0:
        for i in range(2, min(6, len(buf))):
            if buf[i] in (0x55, 0x56, 0x57):
                return True
    return False


def main():
    candidates = json.loads(CANDIDATES.read_text(encoding="utf-8"))
    print(f"Loaded {len(candidates)} candidates from {CANDIDATES.relative_to(REPO)}\n")

    by_kind: dict[str, list] = {}
    by_pool: dict[str, list] = {}
    plan_function_count = 0
    no_prologue_starts = []

    for c in candidates:
        by_kind.setdefault(c["kind"], []).append(c)
        by_pool.setdefault(c["pool"], []).append(c)

        first_bytes = bytes(int(x, 16) for x in c["first_hex"].split())
        if c["kind"] == "raw_undefined":
            continue  # 1-byte fragments — handled by neighbor
        starts_with_prologue = looks_like_prologue(first_bytes)
        if starts_with_prologue:
            plan_function_count += 1  # at least one function from this candidate
        else:
            no_prologue_starts.append(c)

    print("== By kind ==")
    for k, items in sorted(by_kind.items()):
        total_bytes = sum(c["len"] for c in items)
        print(f"  {k:30s}  count={len(items):4d}  bytes={total_bytes:6d}")

    print("\n== By pool ==")
    for p, items in sorted(by_pool.items()):
        total_bytes = sum(c["len"] for c in items)
        print(f"  {p:10s}  count={len(items):4d}  bytes={total_bytes:6d}")

    print(f"\n== Plan summary ==")
    print(f"  Candidates that start with recognised prologue: "
          f"{len(candidates) - len(no_prologue_starts) - sum(1 for c in candidates if c['kind']=='raw_undefined')}")
    print(f"  Estimated minimum new functions (>= 1 per candidate): {plan_function_count}")
    print(f"  Candidates without recognised prologue at start: {len(no_prologue_starts)}")
    if no_prologue_starts:
        print(f"  → these require Java worker to walk the range looking for "
              f"prologue offsets (or treat the start byte as the entry).")
        for c in no_prologue_starts[:10]:
            print(f"     {c['addr']}  len={c['len']:5d}  kind={c['kind']:30s}  hex={c['first_hex'][:48]}...")
        if len(no_prologue_starts) > 10:
            print(f"     ... {len(no_prologue_starts) - 10} more")

    print(f"\n== Game-pool candidates (highest-priority for review) ==")
    for c in by_pool.get("game", []):
        print(f"  {c['addr']}  len={c['len']:5d}  kind={c['kind']:30s}")

    print(f"\nReady to run Java worker (`fixup_orphans.java`) via `run_script_inline`.")


if __name__ == "__main__":
    main()
