"""Read the registry and write a markdown progress dashboard.

Counts pending / in_progress / done / skipped by category_hint and by
final_category, plus how many auto-named functions remain. Output is
workspace/function_review/progress.md, regenerated each call. The file is
the source of truth for "where did we leave off" between review sessions.
"""

from __future__ import annotations

import json
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
REGISTRY = REPO / "workspace" / "function_review" / "registry.json"
META = REPO / "workspace" / "function_review" / "meta.json"
OUT = REPO / "workspace" / "function_review" / "progress.md"


def main():
    registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
    meta = json.loads(META.read_text(encoding="utf-8"))

    by_status = Counter(r["review_status"] for r in registry)
    by_hint = Counter(r["category_hint"] for r in registry)
    by_final = Counter(r["final_category"] for r in registry)

    auto_remaining = sum(1 for r in registry if not r["is_renamed"]
                         and r["review_status"] != "done")
    pending_by_hint = Counter(r["category_hint"] for r in registry
                              if r["review_status"] == "pending")
    done_by_hint = Counter(r["category_hint"] for r in registry
                           if r["review_status"] == "done")

    lines = []
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    lines.append(f"# Function review progress")
    lines.append("")
    lines.append(f"_regenerated {ts} by `tools/function_review/progress_report.py`_")
    lines.append("")

    lines.append("## Totals")
    lines.append("")
    lines.append(f"- functions: {len(registry)}")
    lines.append(f"- edges: {meta['edge_count']}")
    lines.append(f"- AIL strings (with xref / total): "
                 f"{meta['ail_strings_with_xref']} / {meta['ail_string_count']}")
    lines.append("")

    lines.append("## Review status")
    lines.append("")
    lines.append("| status | count |")
    lines.append("|---|---|")
    for s in ("pending", "in_progress", "done", "skipped"):
        lines.append(f"| {s} | {by_status.get(s, 0)} |")
    lines.append("")

    lines.append("## Category hint distribution (mechanical, pre-review)")
    lines.append("")
    lines.append("| hint | total | pending | done |")
    lines.append("|---|---|---|---|")
    for h in ("ail", "crt", "game"):
        lines.append(f"| {h} | {by_hint.get(h, 0)} | "
                     f"{pending_by_hint.get(h, 0)} | "
                     f"{done_by_hint.get(h, 0)} |")
    lines.append("")

    lines.append("## Final category (post-review confirmed)")
    lines.append("")
    lines.append("| final | count |")
    lines.append("|---|---|")
    for fc in ("ail", "crt", "game", None):
        label = fc if fc else "(unset)"
        lines.append(f"| {label} | {by_final.get(fc, 0)} |")
    lines.append("")

    lines.append(f"## Auto-named (FUN_*) remaining: **{auto_remaining}**")
    lines.append("")

    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(REPO)}")
    print(f"  pending: {by_status.get('pending', 0)}, "
          f"in_progress: {by_status.get('in_progress', 0)}, "
          f"done: {by_status.get('done', 0)}, "
          f"skipped: {by_status.get('skipped', 0)}")
    print(f"  auto-named remaining: {auto_remaining}")


if __name__ == "__main__":
    main()
