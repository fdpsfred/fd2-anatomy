# src_refine 交接文件（新 session 接續用）

對 `src/` 內每個 symbol 逐一 refine 名稱/參數名/註解並同步 Ghidra（**只改名稱與註解，遊戲邏輯不動**）。
**Stage 2 rename + 23 個 closeout logic issue 皆已完成**；Phase 2（plate 重建）已驗證為 moot 並關閉（0 plate 遺失，詳見 0a）；零星項 n=181/164/173 已完成（詳見 0b）；剩 item-1 次級引用同步 + 環境清理。
完整工具說明見 `tools/src_refine/_index.md`（本檔不重複，只給接續所需）。

## 0. 剩餘工作（phase-tagged，最重要）

> Phase 2 已關閉（0a）；零星項 n=181/164/173 已 2026-06-25 完成（0b）。以下仍「勿自行開工，等指示」。

1. **[CLOSEOUT] 次級引用一次性同步**（使用者已批准、時機待定）：用 ledger old→new 映射一次性同步**所有被改符號**的引用並驗證。實際待同步處（item 2/3/4 audit 查出）：
   - emit：`tools/code_emit/data/routing.json`、`tools/data_emit/data/data_routing.json`、`emit_issues.json`
   - KB：`resource_info/overview.md`（line 54 `portrait_load_buffer`=n=173 舊名、line 55 `battle_scene_snapshot`=n=174 舊名）、`resource_info/fdfield.md`（line 125 `portrait_cache_alloc_offset`=n=182 舊名）等
   - **tests/**：~15 檔（`fieldfix.h`、`chevt*.c`、`rsrc.c`…）引用已死舊 global `data_fd2_chapter_portrait_load_buffer`（n=173 漏同步到 tests → 現編不過）；同類 ISS-0018 gap（`tests/ui_menu/menucfg.c`）已於 item 2 順手修
   - 注意：name-sweep 只抓完整符號名，無前綴簡寫 prose（上述 KB 例）抓不到須逐一查；歷史 shards + 衍生 `src_info*.json` 不動。

2. **[CLOSEOUT] 環境清理**：`git worktree remove` rp1-4（`C:/Users/fdpsf/Documents/fd2-wt/rp1..rp4`，branch refine-p1..4）、刪 `workspace/src_refine/`；未追蹤的 `tools/src_refine/plate_rebuild_scope.py`（Phase2 prep，已 moot）建議刪。

## 0a. Phase 2 已關閉（moot — 已驗證 0 plate 遺失）

驗證結論（數字皆 Ghidra 即時掃描，非推測）：

- 401 個 plate-rebuild 目標中 **379 個 PLATE 完好**（113 function 全在 + 266 data），僅 **22 個（全 data）PLATE 為空**。
- 該 22 個的完整文件內容**全在 PRE_COMMENT**（逐一檢查 + 對 shard 交叉驗證內容一致），0 內容遺失；src/ 亦在 git 安全。
- 全程式 data 註解普查（1062 defined data）：PRE-only **646**、PLATE-only 87、both 217、無 112。PRE_COMMENT 才是本專案 data 文件的主流位置；那 22 個只是 646 的一小角，與 wedge **無關**，屬「按 `project_ghidra_data_plate_mechanism` 慣例應遷 PLATE 但既有未遷」的狀態。
- 使用者 2026-06-25 決定：**保留 PRE 文件現狀**（與全程式慣例一致），不做 PRE→PLATE 欄位遷移。`plate_rebuild_scope.py` / `plate_targets.txt` 隨之失去用途（見 0.2 清理）。

## 0b. 已完成（摘要；逐筆細節見 `src_issues.json` 的 `resolution` 欄 / ledger）

- **Stage 2 rename**：185/185（75 symbol + 279 param），Ghidra 全 `setName`+`save_program`。
- **23 個 logic issue 全 resolved**（`tools/src_refine/data/src_issues.json`，每筆 `status=resolved` + `resolution`）：
  - 大宗改動：protos.h 系統性去重 **47 函數**（ISS-5/7/9/10，移 60 行，工具 `dedup_protos.py`）；Ghidra `byte[856] orphan blob` 拆出 2 個 **LIVE** 表 movement_cost[580]/job_allowed_items[203]（ISS-22，曾被誤標 dead/skip_unreachable）；`status_sleep_flag→status_paralysis_flag` 跨 9 檔 + struct 欄位 + plate（ISS-1，0x26=麻痹非睡眠）。
  - 文件 reconcile：pose 表 x/y 名稱(8/17，Stage2 已對、清 stale plate/PRE/註解)、SFX bank plate(18，從 walk-animation 改正)、pipeline_spec **E-7e skip→emit**(21，orphan co-dead chain 在原版內、emit 才等價)、handler table 256→90(4)。
  - 忠實還原 binary、驗證不改：12/13/14/16（OOB read / LX close 潛在 bug / 10-entry table / 30-byte table，逐一 disasm/byte 確認 by-design）、2/3（舊 plate 誤解 Watcom phantom caller / tail-merged epilogue）。
  - Ghidra 標註：6（param origin_x→sprite_atlas）、23（type spell_entry→spell_effect）。
- **const（ISS-19/20）+ baseline 推進**：5 個 `data_fd2_ui_*_menu_state_template`（0x53EF2/F02/F12/F22/F32）經 write-xref 證明真唯讀，已改 const；使用者批准把 eqcheck baseline 推進到 const build → `baseline_eq.json`/`baseline_hash.txt` 現 = **const build `2f2e12da`**（原版等價 build 為 `ab5f110a`/commit f83ec3c9）、eqcheck 重回 **PASS[STRICT]**。理由：memory `feedback_semantic_correctness_over_eqcheck` + `baseline_eq.json` 的 `_advance_note`。
- **Final review 結論**：整個 closeout 僅 const family 一處屬「為等價犧牲語意」、已糾正；忠實還原 binary 的案例不算犧牲（重現原版是目標）。
- **零星 closeout（2026-06-25）n=181/164/173 done**：n=181 function `_overlay→_sfx_bank` 改名（src 4 檔 + tests 2 檔 + Ghidra 2 fn/6 plate/label + KB fdother.md + ledger；eqcheck PASS[STRICT]；順手修 `tests/ui_menu/menucfg.c` 的 ISS-0018 global gap）；n=164 修 0x51F75 plate（舊名→`_sfx_id_table`、清掉錯誤的 PRE「SFX trigger frame index」層）；n=173 修 `fdfield.md` idx-2 用詞（portrait→char_spawn_pos，4 處）。commit `3c71ecfd`/`5ea889f9`。audit 另查出 overview.md + ~15 tests/ 檔 + fdfield.md:125 的舊名殘留 → 歸 item-1（見 0.1）。
- 現況：0 error bookmark、build 0 undefined、eqcheck PASS[STRICT]、working tree clean（除上述未追蹤 prep script）。

## 1. 必讀文件（依序）

1. `CLAUDE.md`、`index.md`（專案規範、工具）
2. 本檔
3. **逐筆收尾結論**：`tools/src_refine/data/src_issues.json`（23 issue 每筆 `resolution`）；**進度 ledger** `tools/src_refine/data/stage2_progress.json`
4. **符號/位址查詢與 ledger 來源**（closeout 次級引用同步用）：`tools/src_refine/data/shards/rp*/*.json`、`src_info.json` / `src_info_by_name.json`（address↔name 雙向查）、75-符號 old→new ledger
5. `tools/src_refine/_index.md`（全部 script 用法 + 流程）
6. memory：`project_src_refine_rename_eqcheck`（eqcheck gate / baseline 推進）、`feedback_semantic_correctness_over_eqcheck`（語意正確凌駕 eqcheck 等價）、`project_ghidra_data_plate_mechanism`（plate 機制）、`feedback_no_unverified_claim`、`feedback_modification_sync_mandatory`、`feedback_no_surface_verify`、`feedback_ghidra_disconnect_handling`

## 2. 開工先確認工具（per CLAUDE.md）

Ghidra MCP 已開 FD2.LE；DOSBox-X 在 PATH（silent mode）；Watcom 9.5a 在 `C:\Users\fdpsf\Documents\WATCOM_9.5a\BIN`（DOSBox-X 內跑）。`build_fd2.py` 在 `tools/fd2_build/`。
註：`run_script_inline` 是 **Java GhidraScript**——不可用 JS 語法（`import X as Y`、`for(x of arr)`），要 `for(T x : arr)` / 完整限定名；每次會印先前壞 script 的 build-cache spam（無害）。

## 3. 驗收 gate

- **`tools/src_refine/eqcheck.py`，兩級擇一 PASS**：**STRICT**（LE Fixup Record Table 外逐 byte 相同 + fixup 同 multiset）或 **RELOC**（fallback：抹零兩邊「每 fixup site 值 + Fixup Record Table」後殘餘相同 ⇒ 純 COMDEF 重定位+重排、零邏輯改動）。
- **baseline 已於 closeout 推進**：`data/baseline_eq.json`（含 outside/fixup_multiset/residual sha256）現 = const build `2f2e12da`（原版等價 build `ab5f110a`/commit f83ec3c9）；推進理由＝ISS-0019/0020 把 5 個真唯讀 template 改 const（純 data 重定位、code 不變），rebuild 此後刻意與原版只差這組 const-template 重定位。見 `baseline_eq.json:_advance_note`。識別字/改名改動現應過 STRICT（COMDEF 移動才走 RELOC）。
- 只動 plate / 註解（不改 src 識別字）：不影響 build，eqcheck 仍 PASS；plate 純存 Ghidra .gpr，改完 `save_program`。

## 4. GLOBAL rename 機制（若收尾中發現需補改名）

- MCP `rename_global_variable`／`rename_data` 拒絕 `data_fd2_` 名（強制 `g_`，與 `feedback_game_data_symbol_naming` 衝突）→ global 改名一律用 `run_script_inline` 的 `setName(addr,newName,USER_DEFINED)`。function／param 用 MCP `rename_function_by_address`／`rename_variables`。
- src 改名 per-item：裸 old_name grep（找 decl+def+所有 reader + **prefix-collision**：old_name 是更長 symbol 前綴時 replace_all 會誤傷 → surgical）+ 查 new-name 是否已存在 → replace_all 各檔 → `setName` → ledger + per-item commit。

## 5. 歷史參考（已完成，不需重做）

- **Stage 1**：4 worktree（`fd2-wt/rp1..rp4`）平行跑 `src_refine.wf.js`，serial per-symbol commit。教訓進 memory `project_workflow_running_detection`、`feedback_ghidra_disconnect_handling`。
- **merge**：octopus merge rp1..rp4 回 main（`ddd51113`）；`merge_shards.py` 產 `src_info*.json` / `src_issues.json`（1016 shard）。
- **Stage 2 rename**：main 手動 per-item 套 75 符號 + 279 param；gate hash_check→eqcheck(STRICT)→加 RELOC 級。
- **Closeout（本批）**：23 issue 全 resolved + const/baseline 推進；逐筆 commit `0d3ce8a0`..`b6a9b9cb`，細節見 `src_issues.json`。

## 6. 硬性規則

- **絕不動遊戲邏輯**；改完非純註解的東西要 eqcheck PASS（STRICT 或 RELOC）。
- **語意正確可凌駕 eqcheck 等價**（`feedback_semantic_correctness_over_eqcheck`）：經完整分析確認的 const 等語意修正，即使破 byte 等價也保留；逐項過程**不擅自 diverge binary**，留到多 item 工作末了的 final review 一併處理 + 決定 baseline 是否推進。
- 程式碼內文字 ASCII（中文角色/道具/法術專名例外）；Ghidra struct/type 名對齊 src，欄位名沿 Hungarian 慣例。
- **嚴禁 Write 整檔覆寫 src**（會抹掉同檔其他符號）；用 Edit。durability：每完成一個 item 就 commit。
- 任何修改當下三同步 plate / KB / script（`feedback_modification_sync_mandatory`）；audit/verify 逐一不抽樣（`feedback_no_surface_verify`、`feedback_no_sampling_no_batch_verify`）。

## 7. 關鍵路徑

- 工具 + 用法：`tools/src_refine/_index.md`；closeout 新增 `dedup_protos.py`（protos 去重）；`plate_rebuild_scope.py`（Phase2 plate 目標收集，Phase 2 已 moot，見 0.2 建議刪）。
- 耐久資料（tracked）：`tools/src_refine/data/`（baseline_hash.txt、baseline_eq.json、stage2_progress.json、shards/rpN/、src_info*.json、src_issues.json、eqcheck.py、stage2_reconcile.py）。
- 可重生 scratch（gitignored）：`workspace/src_refine/`（plate_targets.txt、ghidra dump、worklist 等）。
- 4 worktree：`C:/Users/fdpsf/Documents/fd2-wt/rp1..rp4`（branch refine-p1..4，收尾時 remove）。
