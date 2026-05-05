"""Render the call graph as a Graphviz DOT file.

Reads workspace/function_review/registry.json + edges.json and emits a
workspace/function_review/call_graph_<utc>.dot snapshot. Nodes are coloured
by final_category (or category_hint if not yet reviewed):

    ail  → light blue,    crt  → light yellow,   game → light grey

Phase E rebuilds with final names + categories and promotes the result to
program_info/call_graph.dot.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "function_review"
REGISTRY = WORK / "registry.json"
EDGES = WORK / "edges.json"


CATEGORY_FILL = {
    "ail":  "#cce4ff",
    "crt":  "#fff5cc",
    "game": "#e8e8e8",
}


def main(out_path: Path | None = None):
    registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
    edges = json.loads(EDGES.read_text(encoding="utf-8"))

    if out_path is None:
        ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        out_path = WORK / f"call_graph_{ts}.dot"

    addr_to_row = {r["address"]: r for r in registry}

    lines = ["digraph fd2_call_graph {",
             '  rankdir=LR;',
             '  node [shape=box, fontname="Consolas", fontsize=9, style=filled];',
             '  edge [arrowsize=0.5, color="#555555"];']

    for r in registry:
        cat = r["final_category"] or r["category_hint"]
        fill = CATEGORY_FILL.get(cat, "#ffffff")
        label = r["current_name"].replace('"', '\\"')
        lines.append(f'  "{r["address"]}" [label="{label}", fillcolor="{fill}"];')

    for e in edges:
        if e["from"] in addr_to_row and e["to"] in addr_to_row:
            lines.append(f'  "{e["from"]}" -> "{e["to"]}";')

    lines.append("}")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {out_path.relative_to(REPO)}")
    print(f"  nodes: {len(registry)}, edges: {len(edges)}")


if __name__ == "__main__":
    main()
