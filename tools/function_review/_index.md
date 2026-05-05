# tools/function_review/

支援「全 function 命名審視 + call graph 建立」工作流的 script。輸入是 Ghidra MCP
匯出的純機械事實（function 列表、call graph、AIL 字串引用），輸出是
`workspace/function_review/` 內的 registry / edges / progress / dot snapshot。

所有 script 只做機械處理 — 命名與分類判斷必須由人親自讀 decompiled source 決定，
不允許在 script 內做。

## 原始 Ghidra dumps（放 `workspace/function_review/raw/`）

每次 session 在 Claude Code 內透過 MCP 工具重新抓，存成下列固定檔名供 script 讀：

| 檔名 | 來源 MCP 工具 | 內容 |
|---|---|---|
| `list_functions_enhanced.txt` | `list_functions_enhanced(limit=10000)` | 所有 1000 functions {address, name, isThunk, isExternal} |
| `call_graph_adjacency.txt` | `get_full_call_graph(format="adjacency", limit=5000)` | `caller: callee1, callee2, ...` 一列一個 caller，僅含至少 1 個 callee 的 function |
| `ail_strings.txt` | `list_strings(filter="AIL_", limit=300)` | `addr: "string content"` 一列一條 AIL_ 字串 |
| `ail_string_xrefs.json` | `get_bulk_xrefs(addresses=...)` 帶上述所有 AIL 字串 addr | 字串 addr → 引用 site 列表 |

`get_full_call_graph` 用 `edges` format 會漏邊；用 `adjacency` 才完整。

## Scripts

### `build_registry.py`

讀 `workspace/function_review/raw/` 的 4 個 dump，輸出：

- `workspace/function_review/registry.json` — 一筆一個 function，含 address /
  current_name / is_renamed / is_thunk / callers / callees / ail_string_refs /
  category_hint / review_status / confirmed_name / final_category /
  tentative_system / notes
- `workspace/function_review/edges.json` — call graph 邊資料
- `workspace/function_review/meta.json` — 各項計數（function 數、邊數、AIL 字串數等）

`category_hint` 是純機械訊號（function 內含 AIL_ 字串 → ail；名字以
`crt_` 開頭 → crt；其餘 → game），**只用來排 review 順序，絕不能當作命名根據**。
親自 review 後才寫 `final_category` 與 `confirmed_name`。

```bash
python tools/function_review/build_registry.py
```

### `progress_report.py`

讀 `registry.json` + `meta.json`，輸出 `workspace/function_review/progress.md`
進度面板：review status × category 計數 + 自動名剩餘量。每次 session 起手與
收尾都跑一次，作為斷點記錄。

```bash
python tools/function_review/progress_report.py
```

### `dump_dot.py`

讀 `registry.json` + `edges.json`，輸出
`workspace/function_review/call_graph_<utc>.dot` Graphviz 暫態圖檔，用 fillcolor
標示 `final_category`（沒 review 完前用 `category_hint`）：ail = 淺藍 / crt =
淺黃 / game = 淺灰。各階段內檢視中間進度用。

```bash
python tools/function_review/dump_dot.py
```

### `build_call_graph.py`

從 `registry.json` + `edges.json` 凍結 program_info call graph 三檔
(`call_graph.json` / `call_graph.dot` / `call_graph.md`)。內含 Watcom v2 公開
C-RTL 符號清單 `PUBLIC_CRT_SYMBOLS`（無 `crt_` 前綴但屬 CRT 的公開名，例如
`malloc` / `fread` / `sin`），用來把這些 node 歸為 `crt` category。

```bash
python tools/function_review/build_call_graph.py
```

### `fixup_orphans.py` + `fixup_orphans.java`

把 .object1 內所有 mis-classified byte[] 與 orphan instruction stream 全部建為
Function entity（暫名 `vendor_<pool>_<addr>`）。`fixup_orphans.py` 是 dry-run
planner（讀 candidate JSON 列出將執行的動作）。`fixup_orphans.java` 是 Ghidra
worker：clear data → disassemble → walk range 找 prologue 並 createFunction →
plate comment 標 `AUTO-CREATED Phase F`。

候選 JSON schema：`{"addr","len","kind","pool","first_hex"}`。kind 分三類：
`byte_array_misclassified` / `byte_array_with_jmp_xref` / `orphan_instructions`
（外加 `raw_undefined` 但實際 skip）。pool: `ail` / `crt` / `game`。

工作流程：

```bash
# 1. 跑 .object1 uncovered-range 統計 inline script 產生 phase_f_candidates.json
# 2. python tools/function_review/fixup_orphans.py  → dry-run summary
# 3. 透過 mcp__ghidra__run_script_inline 執行 fixup_orphans.java 內容
# 4. 若有 transient Bad Instruction bookmark：跑 cleanup pass（stale 移除 + recovery 重建）
# 5. python tools/function_review/build_registry.py 重建 registry
```

執行後候選必須全部處理完，function_count、Bad Instruction、find_code_gaps、
.object1 uncovered inst bytes 都要回到目標值。

### `bulk_rename.java`

Phase G 的機械批次 rename + 結構性 wrong-split fix。從 line-based TSV 計畫
（`workspace/function_review/phase_g_rename_plan.tsv`，每行
`section\taddr\tname[\tplate_or_role]`）讀條目，依 section 對 Function 套
新名稱、寫 plate comment；structural_fix 條目額外做 `fm.removeFunction` +
`Function.setBody()` 把 fragment 函數合併進前一個 named function。

支援 section：
- `align_nop` — 套 `align_nop_<addr>` 名稱
- `int_stub` — 套 `crt_dpmi_int_<NN>` 名稱（NN 從 addr 推算）
- `int3_stub` / `dispatcher_inner` / `dispatcher_outer` — 固定名稱
- `structural_fix` — 刪 vendor + 擴 prev fn body
- 其他 section 走通用 rename 路徑

執行：把 `bulk_rename.java` 內容貼入 `mcp__ghidra__run_script_inline` 的
`code` 參數（注意 Ghidra 內 inner class 在 script wrapper 下會 fail，內容
若需要展開請完全 inline）。內含 pre-flight name-conflict check 與 transaction
保護；失敗自動 rollback、記入 `phase_g_bulk_log.json`。

## 一個典型 session 的流程

```bash
# 起手：刷新進度看上次斷點
python tools/function_review/progress_report.py

# 進行人工 review（讀 decompiled source，rename + plate comment 透過 Ghidra MCP）
# 把 review_status / confirmed_name / final_category / notes 寫回 registry.json

# 階段尾：刷新進度與 dot snapshot；清 Bad Instruction bookmark；save_program
python tools/function_review/progress_report.py
python tools/function_review/dump_dot.py
```

如果 Ghidra 裡有大量 rename 改變，先重抓 raw dumps（覆寫 `raw/` 內檔案），
再跑 `build_registry.py` 重建。`build_registry.py` 會自動保留既有 registry 的
review 狀態欄位（review_status / confirmed_name / final_category /
tentative_system / notes），只刷新機械欄位（current_name / callers / callees /
ail_string_refs / category_hint）。
