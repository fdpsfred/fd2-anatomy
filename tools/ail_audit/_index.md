# tools/ail_audit/

支援「AIL function 分類復查 + `crt_*` 占位升級判定」工作流的 reusable script。
每支 script 從 Ghidra MCP 匯出的 dump（registry / call_graph / 各 JSON 中介
檔）讀資料，做機械處理 — 命名與分類判斷由 script 套用設計嚴格的 evidence
gate 規則，並對 `suspect` / `ambiguous` 案例輸出 manual review 列表給使用者
裁定。

工作流序列（每支 script 用前一個 step 的 JSON 輸出當輸入）：
`verify_public` → `bfs_reach` → `promote_crt` → `apply_renames` →
`orphan_review` → `build_inventory`。

所有 script 的輸入 dump、中介 JSON、`apply_*.java`、summary `.md` 一律寫到
`workspace/ail_audit/`（暫態 audit artifact 目錄，非 KB）。

## 從 Ghidra 匯出的 raw dump

3 份 dump 用 `mcp__ghidra__run_script_inline` 跑 inline Java 一次性匯出（每次
Ghidra 內 AIL 命名有更動需重跑），檔名固定供下面的 script 讀取：

| 檔名 | 內容 | 用途 |
|---|---|---|
| `ail_decomp_dump.json` | 所有 `AIL_*` 函式的 decompile + callees + callers + data_refs + body_size + instr_count | `verify_public.py` 自我證明驗證、AIL globals/strings 集合建立 |
| `crt_candidate_dump.json` | 上一步找出的 `crt_*` candidate 同樣資料 | `promote_crt.py` evidence 檢查 |
| `orphan_xrefs.json` | BFS 不可達 `AIL_internal_*` 的 xrefs to entry | `orphan_review.py` 依 xref 分 sub-case |

## Scripts

### `verify_public.py`

對 `AIL_*`（公開）函式自我證明驗證：解析 decompile 的 `fprintf(*, "<fmt>", ...)`
找 `<fmt>` 開頭等於函式名的 self-print；callee 含
`AIL_internal_log_print_timestamp_prefix @ 0x3794c` 視為 log helper signal。
Verdict 分布為
`confirmed_public_multi` / `public_wrapper` / `confirmed_public` /
`confirmed_public_trivial` / `suspect_public` / `defer_F`。

同步建立 AIL globals 集合（confirmed AIL 函式讀寫的 global address 聯集，扣
掉 `crt_globals_map.json` 已知 CRT global）與 AIL string xref（AIL-prefix
字串被哪些 AIL function 引用）。

```bash
python tools/ail_audit/verify_public.py
```

### `bfs_reach.py`

以前一步 confirmed_*/public_wrapper 為 seed，forward BFS 跑
`program_info/call_graph.json` edges，跳過指向 `align_nop_*` 的邊。每個
reached node 依當前名稱前綴 + `rebuild_info/crt_lookup_9.5a.json` 分類為
`ail_public_callee` / `ail_internal_confirmed` / `crt_real` / `crt_candidate_pure`
/ `excluded` / `boundary_leak`。

```bash
python tools/ail_audit/bfs_reach.py
```

`boundary_leak` 必須為 0；非 0 表示 BFS 漏到 game function，需先解。

### `promote_crt.py`

對前一步 `crt_candidate_pure` 跑 worklist fixed-point。每輪檢查每個 candidate
的 callers（剝除 `align_nop_*` / 自己）：
- 全部 ⊆ AIL set + disasm evidence pass → 升入 `promoted`，AIL set 加大，下輪重評
- 全部 ⊆ AIL set 但 evidence FAIL → `ambiguous`
- 有非 AIL caller → `non_ail_caller`
- 0 caller → `deferred_zero_indegree`

Disasm evidence gate（任一成立即過）：
- callee 含 `AIL_internal_log_print_timestamp_prefix @ 0x3794c`
- 引用 AIL globals 集合內的 global address
- 引用 AIL string xref 集合內的 string address

新名啟發式：`crt_<rest>` → `AIL_internal_<rest>`（保留語意）；無語意則 fall back 到
`AIL_internal_helper_<addr>`。

```bash
python tools/ail_audit/promote_crt.py
```

### `apply_renames.py`

讀 promotion JSON（`--source` 預設指向 `promote_crt.py` 的輸出，或指向
`orphan_review.py` 輸出做 plate-only 套用）產生 rename log + Ghidra inline
Java apply script。

`--commit` 模式才寫 Java；無 `--commit` 是 dry-run。Java 透過
`mcp__ghidra__run_script_inline` 執行套用 `setName` + `setComment`，
plate template 含日期 / promotion evidence / AIL caller list / in-degree
breakdown / 對應 audit JSON 路徑。

```bash
python tools/ail_audit/apply_renames.py            # dry-run
python tools/ail_audit/apply_renames.py --commit   # 產生 Ghidra apply Java
# 然後用 mcp__ghidra__run_script_inline 跑生成的 Java
```

Naming conflict check：rename 前對 `workspace/function_review/registry.json`
比對新名沒撞已存在 symbol。

### `orphan_review.py`

對 BFS-unreached AIL function 依 `orphan_xrefs.json` 的 xref pattern 分
`vtable_indirect`（DATA xref，function-pointer 註冊點）/ `cluster_member`
（CALL xref 全來自其他 BFS-unreached AIL_internal_*）/ `tail_call_target`
（UNCONDITIONAL_JUMP）/ `dead_code_stub`（無 xref）/ `non_ail_caller`，產生
分類 JSON + summary + plate-comment apply Java。

```bash
python tools/ail_audit/orphan_review.py
# 然後用 mcp__ghidra__run_script_inline 跑生成的 plate-apply Java
```

### `build_inventory.py`

把前面所有 step 結果整合成 canonical AIL function inventory JSON。每筆含
addr / name / category（`public` / `internal_reached` / `promoted_internal` /
`orphan_internal`）/ sub_class / evidence_ref（指回對應中介 JSON 的 `#<addr>`）。

```bash
python tools/ail_audit/build_inventory.py
```

## 一個典型新 audit session 的流程

```bash
# 0. sanity check
mcp ghidra list_instances  # 確認 FD2.LE 還在

# 1. 重新 dump function list + call graph
# (透過 Claude Code MCP 把 list_functions_enhanced + get_full_call_graph 結果
#  覆蓋到 workspace/function_review/raw/)
python tools/function_review/build_registry.py
python tools/function_review/build_call_graph.py

# 2. 用 inline Java 重新 dump AIL function decompile + data refs
# (見對應 raw dump 用途註解)

# 3. 跑 audit 流程
python tools/ail_audit/verify_public.py
python tools/ail_audit/bfs_reach.py
python tools/ail_audit/promote_crt.py
python tools/ail_audit/apply_renames.py --commit  # 產生 apply Java
# 跑生成的 apply Java
python tools/ail_audit/orphan_review.py
# 跑生成的 plate-apply Java
python tools/ail_audit/build_inventory.py
```

每 step 結束建議停下來看 summary `.md` 與 JSON 內 ambiguous / suspect 列表，
人工裁定後再進下一步。
