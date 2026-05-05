"""Promote call graph from workspace/ snapshots into program_info/ artefacts.

Reads `workspace/function_review/registry.json` + `edges.json` and emits the
finalised program_info call-graph trio:

    program_info/call_graph.json   (nodes + edges, machine-readable)
    program_info/call_graph.dot    (Graphviz, fillcolor by category)
    program_info/call_graph.md     (one-page reader's guide)

Category derivation (deterministic; no manual classification needed):

    ail   - name starts with AIL_                               (Miles AIL pool)
    crt   - name starts with crt_, or matches a curated set of  (Watcom CRT)
            Watcom v2 public C-RTL symbol names that were left
            unprefixed because Ghidra promoted the original
            public symbol (malloc, fread, sin, ...).
    game  - everything else                                     (game logic)

`system` is set to "audio" for ail nodes; for non-ail it is taken from
registry.tentative_system if populated, otherwise null. CRT nodes are not
a game system and are always null.
"""

from __future__ import annotations

import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "function_review"
OUT = REPO / "program_info"

REGISTRY = WORK / "registry.json"
EDGES = WORK / "edges.json"

JSON_OUT = OUT / "call_graph.json"
DOT_OUT = OUT / "call_graph.dot"
MD_OUT = OUT / "call_graph.md"

# Watcom v2 public C-RTL symbols that are present in FD2.LE without a
# `crt_` prefix because Ghidra recovered the original public symbol name.
# Treated as crt category for graph colouring.
PUBLIC_CRT_SYMBOLS = {
    # heap
    "malloc", "free", "_nmalloc", "_nfree",
    # stdio
    "fread", "fwrite", "fopen", "fclose", "fseek", "fgets", "fputs",
    "getc", "putc", "vfprintf", "fprintf", "sprintf",
    # POSIX-ish low level I/O
    "open", "close", "read", "write", "lseek", "_tell", "_filelength",
    # memory / string
    "memcpy", "memmove", "memset", "_memset_bulk", "_memset_inner",
    "strcpy", "strncpy", "strncmp", "strnicmp", "strlen",
    # ctype
    "tolower", "toupper",
    # numeric / math
    "strtod", "sin", "cos",
    # time
    "time", "asctime", "mktime",
    # process / env
    "exit", "getenv",
    # x86 port I/O
    "outp",
    # misc CRT support
    "delay",
    # Watcom internal accessors
    "__filbuf", "__get_doserrno_ptr", "__get_errno_ptr",
}

CATEGORY_FILL = {
    "ail":  "#cce4ff",
    "crt":  "#fff5cc",
    "game": "#e8e8e8",
}


def categorise(name: str) -> str:
    if name.startswith("AIL_"):
        return "ail"
    if (name.startswith("crt_") or name.startswith("align_nop_")
            or name in PUBLIC_CRT_SYMBOLS):
        return "crt"
    return "game"


def system_for(name: str, category: str, tentative: str | None) -> str | None:
    if category == "ail":
        return "audio"
    if category == "crt":
        return None
    return tentative


def build():
    registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
    edges = json.loads(EDGES.read_text(encoding="utf-8"))

    nodes = []
    counts = {"ail": 0, "crt": 0, "game": 0}
    for r in registry:
        cat = categorise(r["current_name"])
        counts[cat] += 1
        nodes.append({
            "address": r["address"],
            "name": r["current_name"],
            "category": cat,
            "is_thunk": bool(r.get("is_thunk")),
            "system": system_for(r["current_name"], cat, r.get("tentative_system")),
        })

    json_payload = {
        "function_count": len(nodes),
        "edge_count": len(edges),
        "category_counts": counts,
        "nodes": nodes,
        "edges": [{"from": e["from"], "to": e["to"]} for e in edges],
    }
    JSON_OUT.write_text(
        json.dumps(json_payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    addr_to_node = {n["address"]: n for n in nodes}

    dot = ["digraph fd2_call_graph {",
           '  rankdir=LR;',
           '  node [shape=box, fontname="Consolas", fontsize=9, style=filled];',
           '  edge [arrowsize=0.5, color="#555555"];']
    for n in nodes:
        fill = CATEGORY_FILL[n["category"]]
        label = n["name"].replace('"', '\\"')
        shape = "ellipse" if n["is_thunk"] else "box"
        dot.append(
            f'  "{n["address"]}" [label="{label}", fillcolor="{fill}", '
            f'shape={shape}];'
        )
    for e in edges:
        if e["from"] in addr_to_node and e["to"] in addr_to_node:
            dot.append(f'  "{e["from"]}" -> "{e["to"]}";')
    dot.append("}")
    DOT_OUT.write_text("\n".join(dot) + "\n", encoding="utf-8")

    md = f"""# Call graph

FD2.LE 全程式 {len(nodes)} 個 function 的呼叫關係，由 Ghidra 直接匯出 + 經過完整命名審視
後以 `tools/function_review/build_call_graph.py` 凍結而成。

## 三檔用途

| 檔案 | 用途 |
|---|---|
| `call_graph.json` | machine-readable，給 script / IDE 讀。schema 見下節 |
| `call_graph.dot` | Graphviz 全圖，渲染 / 過濾子圖時當原始素材 |
| `call_graph.md` | 本檔，閱讀規範與基本統計 |

## JSON schema

頂層：`{{function_count, edge_count, category_counts, nodes[], edges[]}}`

`nodes[i]`：
- `address` — 8-hex 小寫 function start (例：`"00010010"`)
- `name` — Ghidra 內當前 function 名
- `category` — `"ail"` / `"crt"` / `"game"`
- `is_thunk` — Ghidra 內被標記為 thunk
- `system` — 屬於哪個遊戲 system；`"audio"` 對應全部 AIL_*，`crt` 一律 null，
  `game` 視 registry 內 tentative_system 而定（多數 null，待後續逐步補齊）

`edges[i]`：`{{from, to}}` 兩個 8-hex 小寫位址。

## DOT 視覺規範

- node 形狀：thunk 用 ellipse，其餘 box
- node fillcolor：ail = 淺藍 `#cce4ff` / crt = 淺黃 `#fff5cc` / game = 淺灰 `#e8e8e8`
- edge：箭頭 0.5x、灰 `#555555`

## 統計（凍結時）

| 指標 | 數量 |
|---|---|
| function 總數 | {len(nodes)} |
| edge 總數 | {len(edges)} |
| ail 節點 | {counts["ail"]} |
| crt 節點 | {counts["crt"]} |
| game 節點 | {counts["game"]} |

## 渲染子圖範例

全圖 {len(nodes)} 個節點直接 render 不易讀，建議過濾感興趣的子集。例：

```bash
# 全圖（需大張紙）
dot -Tsvg program_info/call_graph.dot -o full.svg

# 只看 AIL pool（用 jq 過濾 JSON 後另寫一份 dot）
jq '.nodes[] | select(.category=="ail") | .address' program_info/call_graph.json
```

或寫個 Python script 從 `call_graph.json` 抓某 root 函數的 reachable 子圖再
render，因為 categories 已經貼好，dot 內的 fillcolor 會自然繼承。

## 重新生成

修改後重新 build：

```bash
# 1) 在 Ghidra 內把改動 save 起來
# 2) 重抓 raw dumps（list_functions_enhanced + call_graph_adjacency）→ workspace/function_review/raw/
# 3) 重建 registry / edges
python tools/function_review/build_registry.py
# 4) 重建 program_info 三檔
python tools/function_review/build_call_graph.py
```

CRT 公開符號清單（`PUBLIC_CRT_SYMBOLS`）寫在 `build_call_graph.py` 內，
若日後又解出沒有 `crt_` 前綴但屬於 Watcom RTL 的公開符號，要加進那個 set。
"""
    MD_OUT.write_text(md, encoding="utf-8")

    print(f"function_count: {len(nodes)}")
    print(f"edge_count:     {len(edges)}")
    print(f"by category:    ail={counts['ail']} crt={counts['crt']} "
          f"game={counts['game']}")
    print(f"\nwrote:")
    for p in (JSON_OUT, DOT_OUT, MD_OUT):
        print(f"  {p.relative_to(REPO)}")


if __name__ == "__main__":
    build()
