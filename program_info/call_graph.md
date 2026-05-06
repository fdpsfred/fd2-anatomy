# Call graph

FD2.LE 全程式 1699 個 function 的呼叫關係，由 Ghidra 直接匯出 + 經過完整命名審視
後以 `tools/function_review/build_call_graph.py` 凍結而成。

## 三檔用途

| 檔案 | 用途 |
|---|---|
| `call_graph.json` | machine-readable，給 script / IDE 讀。schema 見下節 |
| `call_graph.dot` | Graphviz 全圖，渲染 / 過濾子圖時當原始素材 |
| `call_graph.md` | 本檔，閱讀規範與基本統計 |

## JSON schema

頂層：`{function_count, edge_count, category_counts, nodes[], edges[]}`

`nodes[i]`：
- `address` — 8-hex 小寫 function start (例：`"00010010"`)
- `name` — Ghidra 內當前 function 名
- `category` — `"ail"` / `"crt"` / `"game"`
- `is_thunk` — Ghidra 內被標記為 thunk
- `system` — 屬於哪個遊戲 system；`"audio"` 對應全部 AIL_*，`crt` 一律 null，
  `game` 視 registry 內 tentative_system 而定（多數 null，待後續逐步補齊）

`edges[i]`：`{from, to}` 兩個 8-hex 小寫位址。

## DOT 視覺規範

- node 形狀：thunk 用 ellipse，其餘 box
- node fillcolor：ail = 淺藍 `#cce4ff` / crt = 淺黃 `#fff5cc` / game = 淺灰 `#e8e8e8`
- edge：箭頭 0.5x、灰 `#555555`

## 統計（凍結時）

| 指標 | 數量 |
|---|---|
| function 總數 | 1699 |
| edge 總數 | 4370 |
| ail 節點 | 289 |
| crt 節點 | 776 |
| game 節點 | 634 |

## 渲染子圖範例

全圖 1699 個節點直接 render 不易讀，建議過濾感興趣的子集。例：

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
