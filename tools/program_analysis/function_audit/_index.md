# tools/program_analysis/function_audit/

全 function 結構性審視工作流的 script。對每個 function 親自讀
disasm + decomp + plate + caller/callee，判斷現有命名 / plate /
signature / body 邊界是否正確；發現問題立刻修並三同步。9-group
hard-stop 切分（G1→G9，lib→core），每組之間等使用者拍板。

所有 script 只做機械處理 — verdict / 命名 / 分類判斷必須由人親自讀
disasm 與 decompiled source 後決定。

## 9-Group 切分

| Group | 範圍 | 命名鎖定 |
|---|---|---|
| G1 | `binary_artifact_*` | 自由 |
| G2 | crt_lookup-frozen + `L$N_*` | locked (lookup `name`) |
| G3 | Watcom RTL `_*` / `__*` / `crt_*` 非 lookup | mostly locked |
| G4 | `AIL_*` | locked (AIL baseline) |
| G5 | fd2_* engine (battle/input/animation/graphics/audio) | 自由 |
| G6 | fd2_* data/persistence (resource/save_load/field_map/table_accessor) | 自由 |
| G7 | fd2_* UI (ui_menu/text_dialog) | 自由 |
| G8 | fd2_* event dispatch + chapter handlers | 自由 |
| G9 | fd2_* glue / main loop / entry / 未分系統 | 自由 |

## 起手流程（每組）

1. Claude Code 跑 `mcp__ghidra__list_functions_enhanced(limit=10000)`，
   寫到 `workspace/function_audit/ghidra_dump_<utc>.json`
2. `python dump_ghidra_functions.py` 驗證 dump 結構
3. `python build_worklist.py` 比對前次 + 9-group 分類；diff 警告必須處理
4. `python progress.py [--group G{N}]` 看起手進度

## 逐 fn 工作流

每個 function 執行下述 6 步（不跳步、不批次）：

1. **Fetch**（6 MCP call）：`get_function_signature` →
   `disassemble_function` → `get_assembly_context(end, before=0, after=32)` →
   `decompile_function` → `get_plate_comment` →
   `get_function_callers` + `get_function_callees` +
   `get_function_xrefs(entry_only=False)`
2. **6 類問題判定**：body 邊界 / cc / shared prologue / jump-into-middle /
   plate 一致性 / name 正確性
3. **Action**（單 function MCP command）：`rename_function_by_address` /
   `set_plate_comment` / `set_function_prototype` /
   `delete_function`+`create_function` / `create_label`
4. **Verify-after-action**：`list_bookmarks(Bad Instruction)` 該 fn 範圍 0；
   body 邊界改動重抓 `disassemble_function`；prototype 改動跑 `force_decompile`
5. **三同步**：plate self-ref / KB grep & Edit（透過 `sync_check.py <old>`） /
   crt_lookup `current_name` 同步
6. `record_verdict.py` append + 印進度行

## Scripts

### `dump_ghidra_functions.py`
驗證 `workspace/function_audit/ghidra_dump_*.json` 最新 dump 的結構與
entry 數。Dump 本身由 Claude Code 透過 MCP 取得後寫入（這 script 只
validate）。

### `build_worklist.py`
讀最新 dump + `rebuild_info/crt/lookup_9.5a.json` + `program_info/**/*.md`，
產出 `workspace/function_audit/worklist.json`，每 row 含
`{addr, current_name, group, locked, lookup_name, lookup_mismatch,
is_thunk, status, problem_classes, verdict, notes}`。Ghidra-only entries
status=`pending`；worklist-only entries status=`deleted_in_ghidra`；既有
verdict / status / notes / problem_classes 全保留。

### `record_verdict.py`
每 fn 完工後 append 一筆 verdict 進
`workspace/function_audit/verdicts.jsonl`，並同步更新 `worklist.json`
對應 row。

合法 status：`clean` / `fixed` / `deferred`。`problem_classes` 數字 1–6
對應上述六類問題。

```bash
python tools/program_analysis/function_audit/record_verdict.py 0x12247 \
  '{"status":"fixed","problem_classes":[2,5],"actions":["set_function_prototype","set_plate_comment"],"notes":"cc was stdcall but body uses watcall"}'
```

### `progress.py`
讀 `worklist.json` + `verdicts.jsonl`，輸出 stdout 表格 +
`workspace/function_audit/progress.md`。`--group G{N}` 過濾單組。
worklist row 少於 Ghidra fn 數會 fail-fast。

### `sync_check.py`
列出 repo 內所有對 `<old_name>` 仍在引用的 file:line。Rename 後立刻
跑，驅動三同步。預設 ripgrep；缺則 fallback Python grep。Skip
`workspace/` / `legacy/`。

```bash
python tools/program_analysis/function_audit/sync_check.py <old_name> [<new_name>]
```

## Hard-stop handoff（每組收尾）

寫 `workspace/function_audit/group_{N}/proposal.md`（≤ 6 行 head + 僅
needs-action 條目）。Clean 不寫進去只計數。完工後等使用者拍板才進下一組。
