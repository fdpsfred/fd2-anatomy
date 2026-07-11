#!/usr/bin/env python3
"""One-off: rebase the per-row address columns in assets/characters.md and
assets/enemies.md from the cross-version FD2.EXE space (Ghidra VA + 0x19014)
back to the canonical Ghidra VA space (see assets/tables/_index.md).

Every address token in these two files sits uniformly at Ghidra VA + 0x19014
(verified: read_memory at 0x7Bxxx/0x7Axxx fails; values match at 0x620A1 /
0x61AF9 base). The transform is a pure -0x19014 subtraction on the address
tokens only:
  - characters.md growth rows: bare 5-hex "7Bxxx"
  - enemies.md enemy rows + char_id mapping section: "7Axxx" (bare or 0x-prefixed)

Table VALUES are decimal or 2-hex (RA/CL, spell idx) and never match 7A/7B + 3 hex,
so the regex touches addresses only. Prints every (old -> new) change for audit.
"""
import re
import sys
from pathlib import Path

DELTA = 0x19014
ROOT = str(Path(__file__).resolve().parents[2])

TARGETS = [
    (ROOT + "/assets/characters.md", r"7B[0-9A-F]{3}"),
    (ROOT + "/assets/enemies.md",    r"7A[0-9A-F]{3}"),
]


def rebase(path, pattern):
    with open(path, encoding="utf-8", newline="") as f:
        text = f.read()
    changes = []

    def sub(m):
        old = m.group(0)
        new = format(int(old, 16) - DELTA, "X")
        changes.append((old, new))
        return new

    out = re.sub(pattern, sub, text)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(out)
    return changes


def main():
    for path, pattern in TARGETS:
        changes = rebase(path, pattern)
        print(f"=== {path}: {len(changes)} address tokens rebased -0x{DELTA:X} ===")
        for old, new in changes:
            assert int(old, 16) - DELTA == int(new, 16)
        # print distinct old->new pairs
        for old, new in changes:
            print(f"  {old} -> {new}")


if __name__ == "__main__":
    main()
