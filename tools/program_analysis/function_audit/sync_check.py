"""Scan the repo for stale references to an old function name.

After Claude Code renames a function in Ghidra, this script lists every
file:line that still mentions the old name across the KB / lookup /
tools. The output drives the immediate three-way sync (plate / KB /
script) per `[[feedback_modification_sync_mandatory]]`.

Usage:

    python tools/program_analysis/function_audit/sync_check.py <old_name> [<new_name>]

Searches under:
- program_info/**/*.md
- rebuild_info/**/*.{md,json}
- resource_info/**/*.md
- assets/**/*.md
- tools/**/*.{py,java,md}
- open_issues.md

Explicitly skipped (frozen historical state — must NOT be touched):
- workspace/**          (scratch / frozen — includes the audit's own worklist/verdicts)
- legacy/**             (frozen)
- This script lives in tools/program_analysis/function_audit/ but is also skipped to
  avoid self-matching its own example identifiers.

Output: file:line content, suitable for piping or grepping.
Exit code 0 always (informational); caller decides what to edit.
"""

from __future__ import annotations

import argparse
import io
import re
import subprocess
import sys
from pathlib import Path

# Force UTF-8 stdout so KB lines with ✓ / 中文 don't crash on Windows cp950.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
else:
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parents[3]

SEARCH_GLOBS = [
    "program_info/**/*.md",
    "rebuild_info/**/*.md",
    "rebuild_info/**/*.json",
    "resource_info/**/*.md",
    "assets/**/*.md",
    "tools/**/*.py",
    "tools/**/*.java",
    "tools/**/*.md",
    "open_issues.md",
    "index.md",
    "CLAUDE.md",
]

SKIP_PATTERNS = [
    "workspace",
    "legacy",
]


def is_skipped(rel_path: str) -> bool:
    norm = rel_path.replace("\\", "/")
    return any(s in norm for s in SKIP_PATTERNS)


def search_with_rg(pattern: str) -> list[tuple[str, int, str]]:
    """Use ripgrep if available; fall back to Python search."""
    try:
        result = subprocess.run(
            ["rg", "--no-heading", "--line-number", "--with-filename",
             "--fixed-strings", pattern,
             "--type-add=md:*.md", "--type-add=java:*.java",
             "-tmd", "-tjson", "-tpy", "-tjava",
             "program_info", "rebuild_info", "resource_info", "assets", "tools",
             "open_issues.md", "index.md", "CLAUDE.md"],
            cwd=REPO, capture_output=True, text=True, encoding="utf-8",
            errors="replace",
        )
        hits = []
        for line in result.stdout.splitlines():
            # format: path:lineno:content
            parts = line.split(":", 2)
            if len(parts) < 3:
                continue
            path, lineno, content = parts
            if is_skipped(path):
                continue
            try:
                hits.append((path, int(lineno), content))
            except ValueError:
                continue
        return hits
    except FileNotFoundError:
        return _python_grep_fallback(pattern)


def _python_grep_fallback(pattern: str) -> list[tuple[str, int, str]]:
    hits = []
    for glob in SEARCH_GLOBS:
        for p in REPO.glob(glob):
            rel = str(p.relative_to(REPO))
            if is_skipped(rel):
                continue
            try:
                text = p.read_text(encoding="utf-8", errors="replace")
            except (OSError, UnicodeDecodeError):
                continue
            for i, line in enumerate(text.splitlines(), 1):
                if pattern in line:
                    hits.append((rel, i, line))
    return hits


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("old_name", help="The old function name to find stale references for")
    parser.add_argument("new_name", nargs="?", default=None,
                        help="Optional new name; if given, lines already matching new_name are excluded")
    args = parser.parse_args(argv[1:])

    pattern = args.old_name
    # Word-boundary safety: match the exact symbol, not substring.
    # ripgrep --fixed-strings doesn't do word boundary; we filter in Python.
    word_re = re.compile(rf"(?<![A-Za-z0-9_$]){re.escape(pattern)}(?![A-Za-z0-9_$])")

    raw_hits = search_with_rg(pattern)
    hits = [(p, ln, c) for p, ln, c in raw_hits if word_re.search(c)]

    if args.new_name:
        hits = [(p, ln, c) for p, ln, c in hits if args.new_name not in c]

    if not hits:
        print(f"(no stale references to `{pattern}` found)")
        return 0

    print(f"Found {len(hits)} reference(s) to `{pattern}`:\n")
    for path, lineno, content in hits:
        print(f"{path}:{lineno}: {content.strip()}")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
