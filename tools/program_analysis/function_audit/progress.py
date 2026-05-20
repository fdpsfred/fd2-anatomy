"""Print and persist progress across the function-audit worklist.

Reads worklist.json + verdicts.jsonl and writes
`workspace/function_audit/progress.md` as a markdown dashboard, while also
emitting the same data to stdout for quick checks during a session.

Usage:

    python tools/program_analysis/function_audit/progress.py            # full dashboard
    python tools/program_analysis/function_audit/progress.py --group G1 # focus one group

Fails fast (returns 2) if worklist row count < Ghidra function count
recorded in the worklist (catch-all for stale builds).
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "function_audit"
WORKLIST = WORKSPACE / "worklist.json"
VERDICTS = WORKSPACE / "verdicts.jsonl"
OUT_MD = WORKSPACE / "progress.md"

# Manual section sentinel — content after this marker is preserved across
# `progress.py` runs.  Lets us record "next session todo" without having
# the markdown auto-regen wipe it out.
MANUAL_MARKER = "<!-- BEGIN MANUAL SECTION -->"

GROUP_ORDER = ["G1", "G2", "G3", "G4", "G5", "G6", "G7", "G8", "G9"]
PROBLEM_CLASS_LABELS = {
    1: "body_boundary",
    2: "calling_convention",
    3: "shared_prologue",
    4: "jump_into_middle",
    5: "plate_mismatch",
    6: "name_mismatch",
}


def render_table(rows: list[dict]) -> str:
    lines = []
    lines.append("| group | label | total | pending | clean | fixed | deferred |")
    lines.append("|---|---|---:|---:|---:|---:|---:|")
    for g in GROUP_ORDER:
        live = [r for r in rows
                if r["group"] == g and r["status"] != "deleted_in_ghidra"]
        if not live:
            continue
        label = live[0]["group_label"]
        stat = Counter(r["status"] for r in live)
        lines.append(
            f"| {g} | {label} | {len(live)} | "
            f"{stat.get('pending', 0)} | {stat.get('clean', 0)} | "
            f"{stat.get('fixed', 0)} | {stat.get('deferred', 0)} |"
        )
    return "\n".join(lines)


def render_problem_classes(rows: list[dict]) -> str:
    cls_count: Counter[int] = Counter()
    for r in rows:
        if r["status"] in {"fixed", "deferred"}:
            for c in r.get("problem_classes", []):
                cls_count[c] += 1
    if not cls_count:
        return "_(none yet)_"
    lines = ["| class | label | count |", "|---|---|---:|"]
    for c in sorted(PROBLEM_CLASS_LABELS):
        lines.append(f"| {c} | {PROBLEM_CLASS_LABELS[c]} | {cls_count.get(c, 0)} |")
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--group", help="Focus a single group (G1..G9)")
    args = parser.parse_args(argv[1:])

    if not WORKLIST.exists():
        print(f"ERROR: {WORKLIST} not found; run build_worklist.py first",
              file=sys.stderr)
        return 4
    blob = json.loads(WORKLIST.read_text(encoding="utf-8"))
    rows = blob["rows"]

    live_rows = [r for r in rows if r["status"] != "deleted_in_ghidra"]
    ghidra_n = blob.get("function_count_in_ghidra", 0)
    if len(live_rows) < ghidra_n:
        print(
            f"ERROR: worklist live row count ({len(live_rows)}) < "
            f"Ghidra dump count ({ghidra_n}). Re-run build_worklist.py.",
            file=sys.stderr,
        )
        return 2

    if args.group:
        rows_filter = [r for r in live_rows if r["group"] == args.group]
        if not rows_filter:
            print(f"no rows in group {args.group}", file=sys.stderr)
            return 4
        live_rows = rows_filter

    # stdout summary
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    print(f"Function audit progress  {ts}")
    print(f"  Ghidra fn count:    {ghidra_n}")
    print(f"  worklist live rows: {len(live_rows)}")
    print()
    print(render_table(rows))
    print()
    n_done = sum(1 for r in live_rows if r["status"] != "pending")
    print(f"Overall: {n_done}/{len(live_rows)}  "
          f"({n_done / max(1, len(live_rows)) * 100:.1f}%)")
    print()
    print("Problem class distribution (fixed + deferred):")
    print(render_problem_classes(live_rows))

    # write markdown
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    md = [
        "# Function audit progress",
        "",
        f"_regenerated {ts} by `tools/program_analysis/function_audit/progress.py`_",
        "",
        f"- Ghidra fn count: **{ghidra_n}**",
        f"- worklist live rows: **{len(live_rows)}**",
        f"- overall completed: **{n_done}/{len(live_rows)}** "
        f"({n_done / max(1, len(live_rows)) * 100:.1f}%)",
        "",
        "## Per-group status",
        "",
        render_table(rows),
        "",
        "## Problem class distribution (fixed + deferred)",
        "",
        render_problem_classes(live_rows),
        "",
    ]

    # Top pending per group (next-up hint)
    md.append("## Next pending per group")
    md.append("")
    for g in GROUP_ORDER:
        gp = [r for r in rows
              if r["group"] == g and r["status"] == "pending"]
        if not gp:
            continue
        md.append(f"### {g} {gp[0]['group_label']}  (pending {len(gp)})")
        for r in gp[:5]:
            md.append(f"- `{r['addr']}`  `{r['current_name']}`"
                      f"{' [LOCKED]' if r.get('locked') else ''}")
        if len(gp) > 5:
            md.append(f"- _… {len(gp) - 5} more_")
        md.append("")

    # Preserve any manual content after the sentinel marker
    manual_tail = ""
    if OUT_MD.exists():
        prev = OUT_MD.read_text(encoding="utf-8")
        idx = prev.find(MANUAL_MARKER)
        if idx >= 0:
            manual_tail = prev[idx:]
    auto_section = "\n".join(md)
    if manual_tail:
        OUT_MD.write_text(auto_section + manual_tail, encoding="utf-8")
    else:
        OUT_MD.write_text(auto_section, encoding="utf-8")
    print(f"\nwritten: {OUT_MD.relative_to(REPO)}")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
