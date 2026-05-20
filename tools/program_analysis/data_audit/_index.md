# tools/program_analysis/data_audit/

全 data item 結構性 audit 工作流的 script。對每個 data item 親自讀
bytes + caller decomp + plate + struct relation，判斷現有命名 / type /
boundary / plate 是否正確；發現問題立刻修並三同步。12-group hard-stop
切分 (D1→D12，trivial→hardest)，每組之間等使用者拍板。

所有 script 只做機械處理 — verdict / 命名 / 分類判斷必須由人親自讀
bytes + caller decompile 後決定。

## 12-Group 切分

| Group | 範圍 |
|---|---|
| D1 | 全 padding：`data_align_<addr>` |
| D2 | LE file structures：`IMG_*` |
| D3 | vendor strings：AIL / CRT 相關 ASCII string |
| D4 | fd2 .object3 已 named struct array (game data tables) |
| D5 | fd2 .object3 runtime state |
| D6 | fd2 .object2 lookup tables |
| D7 | fd2 .object2 runtime state (auto-named `DAT_*` / `BYTE_ARRAY_*`) |
| D8 | fd2 .object1 inline data |
| D9 | AIL .object1 dispatch tables + lock list + switch table |
| D10 | AIL .object2 globals subrange |
| D11 | orphan / pointer-indirect 跨 segment |
| D12 | 殘留 deferred / 真孤兒 / edge case |

## Bucket (xref topology, secondary)

| Bucket | 定義 |
|---|---|
| B0 | LE structure (`IMG_*`) |
| B1 | vendor-only xref (`ail` / `crt` pool 端 caller) |
| B2 | 唯一 fd2 caller |
| B3 | ≥2 fd2 caller scattered offsets |
| B4 | adjacent-byte cluster (merge 候選) |
| B5 | pointer-indirect (LE FIXUP 反查找 owner) |
| B6 | true orphan (0 direct + 0 indirect xref) |

## 起手流程（每組）

1. Claude Code 跑 `mcp__ghidra__list_data_items(limit=10000)` 分頁，寫到
   `workspace/data_audit/ghidra_data_dump_<utc>.json`
2. `python dump_data_inventory.py` 驗證 dump 結構
3. `python build_worklist.py` bucket + group + diff
4. `python progress.py [--group D{N}]` 看起手進度

## 逐 item 工作流

每個 data item 執行下述 6 步（不跳步、不批次）：

1. **Fetch**（4 MCP call）：`get_xrefs_to` → `read_memory(addr, size)` →
   `inspect_memory_content` → 每 caller
   `get_plate_comment + decompile_function`（限 5）
2. **5 類問題判定**：name / type / boundary / caller association /
   plate accuracy
3. **Action**（單 item MCP）：`rename_data` / `apply_data_type` /
   `create_struct` / `set_decompiler_comment` (data plate fallback) /
   boundary clear+re-apply
4. **Verify-after-action**：`list_bookmarks(Bad Instruction)` 該 addr 0；
   type 改 → 重抓 `inspect_memory_content`；name 改 → `sync_check.py`
5. **三同步**：plate self-ref / KB grep & Edit / scripts
6. `record_verdict.py` append + 印進度行

## 命名規範

格式：`data_<class>_<purpose>[_<size_hint>]`。

| Class | 範例 |
|---|---|
| `data_fd2_<subsystem>_*` | `data_fd2_battle_item_effect_table` |
| `data_fd2_shared_*` | `data_fd2_shared_rng_seed` |
| `data_ail_*` | `data_ail_mixer_dispatch_table_a` |
| `data_crt_*` | `data_crt_math387s_const_pool` |
| `data_align_*` | `data_align_3c962` |
| `data_le_*` | `data_le_header` |
| `data_string_<class>_*` | `data_string_ail_dig_init_error` |
| `data_orphan_*` | `data_orphan_4b1a3_unknown_blob_1024b` |

## Plate 撰寫

`set_plate_comment` 對 data 不可用（只 function 支援）。Data 改用
`mcp__ghidra__set_decompiler_comment(addr, ...)` 寫 PRE_COMMENT。Plate
文字一律英文撰寫；遊戲內容（角色 / 法術 / 物品名）保留繁體中文原文。

## Scripts

### `dump_data_inventory.py`
驗證 `workspace/data_audit/ghidra_data_dump_*.json` 最新 dump 的結構與
entry 數。Dump 本身由 Claude Code 透過 MCP 取得後寫入（這 script 只
validate）。

### `build_worklist.py`
讀最新 dump + LE fixup index + `program_info/**/*.md` + 既有 worklist
（保 verdict），產出 `workspace/data_audit/worklist.json`，每 row 含
`{addr, current_name, segment, bucket, group, subsystem, locked, status,
problem_classes, verdict, notes}` + diff 警告。

Bucket 機械分桶：

- B0: name 前綴 `IMG_*`
- B1: 所有 caller name 前綴 `AIL_*` / `crt_*` / `L$*` / `IF@*` / PUBLIC_CRT_SYMBOLS
- B2-B3: fd2 caller 1 / ≥2
- B4: 與相鄰 named data 物理連續 (addr diff ≤ 4)
- B5: 0 direct xref + 出現在 LE fixup `target_addr_to_sources`
- B6: 0 direct + 0 indirect

### `record_verdict.py`
每 item 完工後 append 一筆 verdict 進
`workspace/data_audit/verdicts.jsonl`，並同步更新 `worklist.json` 對應
row。

### `progress.py`
讀 `worklist.json` + `verdicts.jsonl`，輸出 stdout 表格 +
`workspace/data_audit/progress.md`。`--group D{N}` 過濾單組。worklist
row 少於 Ghidra data count 會 fail-fast。

### `sync_check.py`
列出 repo 內所有對 `<old_name>` 仍在引用的 file:line。Rename 後立刻
跑，驅動三同步。沿用 `function_audit/sync_check.py` 同套 grep pattern。
Skip `workspace/` / `legacy/`。

### `parse_le_fixup.py`
Parse FD2.LE LE FIXUP table，產出 `workspace/data_audit/le_fixups.json`
含 forward (`source → target`) 與 reverse
(`target_addr_to_sources`) index。

對 source-list (0x20) / imported-ref (trg_type≠0) / additive (0x04) 等
FD2.LE 未使用變體 raise `NotImplementedError`（fail loudly）。內建 3 個
self-test（dispatch_a / dispatch_b / tzname_ptr_array fixup count）。

### `build_data_xrefs.py`
兩個模式：

- `--prep-batches [N]`：從最新 `ghidra_data_dump_*.json` 切成 N-sized
  batches（預設 100），每 batch 一行 comma-separated addresses 印到
  stdout。Claude Code 用這些行餵
  `mcp__ghidra__get_bulk_xrefs(addresses=<line>)`，把每 batch 結果寫到
  `workspace/data_audit/raw_xrefs_batch_<NN>.json`（1-indexed，順序與
  印出對應）。
- （無 flag）：合併所有 `raw_xrefs_batch_*.json`，讀
  `workspace/call_graph/raw/list_functions_enhanced.txt` 做 instruction
  位址 bisect → containing function entry resolution，輸出
  `workspace/data_audit/ghidra_data_xrefs_<utc>.json`（schema：
  `{"xrefs": {data_addr: [{source_addr, source_fn, type}]}}`）供
  `build_xref_graph.py` 消費。

Script 本身不呼叫 MCP（同 `dump_data_inventory.py` validator-only 模式）。

### `build_xref_graph.py`
讀最新 `ghidra_data_xrefs_*.json` + `le_fixups.json` + 最新
`ghidra_dump_*.json` (function list)，產出
`workspace/data_audit/xref_graph.json`：bipartite graph
`{data_to_fn, fn_to_data, data_to_data_via_fixup}`；對每個 data item
列完整 reachability。

### `build_data_inventory.py`
讀最新 data dump + `worklist.json` verdict metadata，產出
`workspace/data_audit/data_inventory.md`：per-segment 完整 data 主索引，
每行 `addr / name / type / size / pool / subsystem / emit_action /
data_kind`（8 種 emit_action 機械 derive）。即時重生，不留 KB snapshot。

## Hard-stop handoff（每組收尾）

寫 `workspace/data_audit/group_{N}/proposal.md`（≤ 6 行 head + 僅
needs-action 條目）。Clean 不寫進去只計數。完工後等使用者拍板才進下一組。
