"""Build FD2.LE call graph from raw Ghidra MCP dumps.

Reads 2 raw dumps from `workspace/call_graph/raw/` and writes the 3 output
artifacts to `workspace/call_graph/` (`call_graph.json` / `.dot` / `.md`).
These are regenerated on demand; not published to KB.

Each function node carries `(category, emit_action)` derived from its current
Ghidra name via `categorise()` / `emit_action_for()`:

- 4 pool: ail / crt / fd2 / binary_artifact
- 3 emit_action: link_vendor_lib / emit_fd2_source / skip_artifact

CRT classification consults `rebuild_info/crt/lookup_9.5a.json` (Watcom real
symbols) + the hardcoded `PUBLIC_CRT_SYMBOLS` set in this file.

Raw dumps (refresh from Ghidra MCP before each re-run):

| File | MCP call |
|---|---|
| `list_functions_enhanced.txt` | `mcp__ghidra__list_functions_enhanced(limit=10000)` |
| `call_graph_adjacency.txt`    | `mcp__ghidra__get_full_call_graph(format="adjacency", limit=5000)` |

Both dumps are stored verbatim (Ghidra MCP wraps results as `{"result": ...}`;
this script unwraps automatically).

Usage:
    python tools/program_analysis/build_call_graph.py

If `categorise()` encounters an unclassifiable name it raises — that's the
last-line check that every function maps to exactly one pool. New Watcom RTL
public symbols without a `crt_` prefix should be added to PUBLIC_CRT_SYMBOLS.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RAW = REPO / "workspace" / "call_graph" / "raw"
OUT = REPO / "workspace" / "call_graph"

LIST_FN_FILE = RAW / "list_functions_enhanced.txt"
CALL_GRAPH_FILE = RAW / "call_graph_adjacency.txt"

JSON_OUT = OUT / "call_graph.json"
DOT_OUT = OUT / "call_graph.dot"
MD_OUT = OUT / "call_graph.md"

# Watcom 9.5a public C-RTL symbols that are present in FD2.LE without a
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
    "strtod", "sin", "cos", "log", "log2",
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
    # crt audit byte-match additions (Watcom CLIB / EMU387 / GRAPH public symbols)
    "__exit", "_fpreset", "__EINVAL", "__set_EDOM", "__setEFGfmt",
    "_Not_Enough_Memory",
}

CATEGORY_FILL = {
    "ail":             "#cce4ff",
    "crt":             "#fff5cc",
    "fd2":             "#e8e8e8",
    "binary_artifact": "#f5e8e8",
}


def _unwrap_mcp_result(text: str) -> str:
    """Ghidra MCP saves results as {"result": "...escaped..."}."""
    blob = json.loads(text)
    return blob["result"] if isinstance(blob, dict) and "result" in blob else text


def load_functions() -> list[dict]:
    text = LIST_FN_FILE.read_text(encoding="utf-8")
    inner = _unwrap_mcp_result(text)
    payload = json.loads(inner)
    return payload["functions"]


def load_name_edges() -> list[tuple[str, str]]:
    """Parse Ghidra's adjacency-format call graph: 'caller: callee1, callee2, ...'."""
    raw = CALL_GRAPH_FILE.read_text(encoding="utf-8")
    inner = _unwrap_mcp_result(raw)
    edges = []
    for entry in inner.split("\n"):
        entry = entry.strip()
        if not entry or ":" not in entry:
            continue
        caller, callee_blob = entry.split(":", 1)
        caller = caller.strip()
        for callee in callee_blob.split(","):
            callee = callee.strip()
            if callee:
                edges.append((caller, callee))
    return edges


def derive_function_address_from_name(name: str) -> str | None:
    """Recover address from auto names like FUN_0003617e or thunk_FUN_0003dccd."""
    m = re.search(r"FUN_([0-9a-fA-F]{8})", name)
    return m.group(1).lower() if m else None


def _load_lookup_names() -> set[str]:
    """Pull every `name` field out of crt/lookup_9.5a.json so all CRT symbols
    introduced by the audit (byte_match etc.) are categorised as crt without
    needing manual entries in PUBLIC_CRT_SYMBOLS."""
    lk_path = REPO / "rebuild_info" / "crt" / "lookup_9.5a.json"
    if not lk_path.exists():
        return set()
    blob = json.loads(lk_path.read_text(encoding="utf-8"))
    return set(e["name"] for e in blob.get("by_address", {}).values())


_LOOKUP_NAMES = _load_lookup_names()


def categorise(name: str) -> str:
    if name.startswith("AIL_"):
        return "ail"
    if name.startswith("binary_artifact_"):
        return "binary_artifact"
    if name.startswith("fd2_"):
        return "fd2"
    if (name.startswith("crt_") or name.startswith("L$")
            or name.startswith("L_")
            or name.startswith("__")
            or name.startswith("IF@")
            or name in PUBLIC_CRT_SYMBOLS
            or name in _LOOKUP_NAMES):
        return "crt"
    raise ValueError(f"unclassifiable name: {name}")


def emit_action_for(name: str, category: str) -> str:
    if category == "ail":
        return "link_vendor_lib"
    if category == "binary_artifact":
        return "skip_artifact"
    if category == "fd2":
        return "emit_fd2_source"
    if category == "crt":
        if name in _LOOKUP_NAMES or name in PUBLIC_CRT_SYMBOLS:
            return "link_vendor_lib"
        # crt_equivalent_* and bare crt_* helpers that are FD2-emitted
        return "emit_fd2_source"
    raise ValueError(f"unknown category: {category}")


def system_for(category: str) -> str | None:
    if category == "ail":
        return "audio"
    return None


def resolve_edges(name_edges: list[tuple[str, str]],
                  functions: list[dict]) -> tuple[list[dict], int]:
    """Resolve (caller_name, callee_name) pairs to addresses, returning
    (resolved_edges, unresolved_count)."""
    name_to_addr: dict[str, str] = {}
    addr_to_name: dict[str, str] = {}
    for f in functions:
        addr = f["address"].lower()
        name_to_addr[f["name"]] = addr
        addr_to_name[addr] = f["name"]

    edges = []
    unresolved = 0
    for caller_name, callee_name in name_edges:
        caller_addr = name_to_addr.get(caller_name)
        callee_addr = name_to_addr.get(callee_name)
        if caller_addr is None:
            recovered = derive_function_address_from_name(caller_name)
            if recovered and recovered in addr_to_name:
                caller_addr = recovered
        if callee_addr is None:
            recovered = derive_function_address_from_name(callee_name)
            if recovered and recovered in addr_to_name:
                callee_addr = recovered
        if caller_addr is None or callee_addr is None:
            unresolved += 1
            continue
        edges.append({"from": caller_addr, "to": callee_addr})

    edges.sort(key=lambda e: (e["from"], e["to"]))
    return edges, unresolved


def build_nodes(functions: list[dict]) -> tuple[list[dict], dict, dict]:
    nodes = []
    counts = {"ail": 0, "crt": 0, "fd2": 0, "binary_artifact": 0}
    emit_counts = {"link_vendor_lib": 0, "emit_fd2_source": 0, "skip_artifact": 0}
    for f in functions:
        addr = f["address"].lower()
        name = f["name"]
        cat = categorise(name)
        action = emit_action_for(name, cat)
        counts[cat] += 1
        emit_counts[action] += 1
        nodes.append({
            "address": addr,
            "name": name,
            "category": cat,
            "emit_action": action,
            "is_thunk": bool(f.get("isThunk")),
            "system": system_for(cat),
        })
    nodes.sort(key=lambda n: n["address"])
    return nodes, counts, emit_counts


def write_json(nodes: list[dict], edges: list[dict],
               counts: dict, emit_counts: dict) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    payload = {
        "function_count": len(nodes),
        "edge_count": len(edges),
        "category_counts": counts,
        "emit_action_counts": emit_counts,
        "nodes": nodes,
        "edges": edges,
    }
    JSON_OUT.write_text(
        json.dumps(payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def write_dot(nodes: list[dict], edges: list[dict]) -> None:
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


MD_TEMPLATE = """# Call graph

FD2.LE 全程式 function 呼叫關係，由 Ghidra 直接匯出 + 經過完整命名審視
後以 `tools/program_analysis/build_call_graph.py` 寫到 `workspace/call_graph/`。
要的時候即時重生，不留 KB snapshot。

## 三檔用途

| 檔案 | 用途 |
|---|---|
| `call_graph.json` | machine-readable，給 script / IDE 讀。schema 見下節 |
| `call_graph.dot` | Graphviz 全圖，渲染 / 過濾子圖時當原始素材 |
| `call_graph.md` | 本檔，閱讀規範與基本統計 |

## JSON schema

頂層：`{function_count, edge_count, category_counts, emit_action_counts, nodes[], edges[]}`

`nodes[i]`：
- `address` — 8-hex 小寫 function start (例：`"00010010"`)
- `name` — Ghidra 內當前 function 名
- `category` — `"ail"` / `"crt"` / `"fd2"` / `"binary_artifact"`（四 pool 分類）
- `emit_action` — `"link_vendor_lib"` / `"emit_fd2_source"` / `"skip_artifact"`
- `is_thunk` — Ghidra 內被標記為 thunk
- `system` — 屬於哪個遊戲 system；`"audio"` 對應全部 AIL_*，其餘一律 null

`edges[i]`：`{from, to}` 兩個 8-hex 小寫位址。

## 四 pool + 兩維度分類

| pool (category) | 命名前綴 | 涵蓋 |
|---|---|---|
| `ail` | `AIL_*` | Miles AIL static lib (audio mixer) |
| `crt` | `crt_equivalent_*` / `crt_*` / `L$*` / lookup name / PUBLIC_CRT_SYMBOLS | Watcom CRT |
| `fd2` | `fd2_*` | FD2 工程師自寫 game / glue / dispatch / dead code |
| `binary_artifact` | `binary_artifact_*` | Watcom compiler/linker emit 的 align_nop 等 padding |

| emit_action | 涵蓋 |
|---|---|
| `link_vendor_lib` | wlink 從 vendor lib 解析 — ail 全部 + crt 內 lookup name + PUBLIC_CRT_SYMBOLS |
| `emit_fd2_source` | FD2 source 端 emit C function — 全部 fd2_* + crt_equivalent_* |
| `skip_artifact` | Watcom 9.5a 重 compile 自動生成 — binary_artifact 全部 |

## DOT 視覺規範

- node 形狀：thunk 用 ellipse，其餘 box
- node fillcolor：ail = 淺藍 `#cce4ff` / crt = 淺黃 `#fff5cc` / fd2 = 淺灰 `#e8e8e8`
  / binary_artifact = 淺粉 `#f5e8e8`
- edge：箭頭 0.5x、灰 `#555555`

## 當前統計

pool 節點數 / emit_action 節點數 / edge 總數請直接讀 `call_graph.json`
頂層 `function_count` / `edge_count` / `category_counts` /
`emit_action_counts` 欄位（jq 一行可拉）；本檔不固定數字以免 stale。

## 渲染子圖範例

全圖節點數多直接 render 不易讀，建議過濾感興趣的子集。例：

```bash
# 全圖（需大張紙）
dot -Tsvg workspace/call_graph/call_graph.dot -o full.svg

# 只看 AIL pool（用 jq 過濾 JSON 後另寫一份 dot）
jq '.nodes[] | select(.category=="ail") | .address' workspace/call_graph/call_graph.json

# 只看 emit_fd2_source 的 reachable subgraph
jq '.nodes[] | select(.emit_action=="emit_fd2_source") | .address' workspace/call_graph/call_graph.json
```

或寫個 Python script 從 `call_graph.json` 抓某 root 函數的 reachable 子圖再
render，因為 categories 已經貼好，dot 內的 fillcolor 會自然繼承。

## 重新生成

```bash
# 1) 在 Ghidra 內把改動 save 起來
# 2) 重抓 raw dumps 到 workspace/call_graph/raw/:
#      mcp__ghidra__list_functions_enhanced(limit=10000) → list_functions_enhanced.txt
#      mcp__ghidra__get_full_call_graph(format="adjacency", limit=5000) → call_graph_adjacency.txt
# 3) 重建 workspace/call_graph/ 三檔
python tools/program_analysis/build_call_graph.py
```

CRT 公開符號清單（`PUBLIC_CRT_SYMBOLS`）寫在 `build_call_graph.py` 內，
若日後又解出沒有 `crt_` 前綴但屬於 Watcom RTL 的公開符號，要加進那個 set。
"""


def write_md() -> None:
    MD_OUT.write_text(MD_TEMPLATE, encoding="utf-8")


def build():
    functions = load_functions()
    name_edges = load_name_edges()
    edges, unresolved = resolve_edges(name_edges, functions)
    nodes, counts, emit_counts = build_nodes(functions)

    write_json(nodes, edges, counts, emit_counts)
    write_dot(nodes, edges)
    write_md()

    print(f"function_count: {len(nodes)}")
    print(f"edge_count:     {len(edges)}")
    if unresolved:
        print(f"unresolved:     {unresolved}")
    print(f"by category:    ail={counts['ail']} crt={counts['crt']} "
          f"fd2={counts['fd2']} binary_artifact={counts['binary_artifact']}")
    print(f"by emit_action: link_vendor_lib={emit_counts['link_vendor_lib']} "
          f"emit_fd2_source={emit_counts['emit_fd2_source']} "
          f"skip_artifact={emit_counts['skip_artifact']}")
    print(f"\nwrote:")
    for p in (JSON_OUT, DOT_OUT, MD_OUT):
        print(f"  {p.relative_to(REPO)}")


if __name__ == "__main__":
    build()
