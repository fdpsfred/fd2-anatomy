# src_refine 交接文件（新 session 接續用）

對 `src/` 內每個 symbol（function + global）逐一解析用途/邏輯，refine 名稱、參數名、註解，並同步 Ghidra。
**只改名稱與註解，遊戲邏輯不動**。**Stage 2 rename 已全部完成**；本檔現在描述的是**收尾階段**。
完整工具說明見 `tools/src_refine/_index.md`（本檔不重複，只給接續所需）。

## 0. 目前狀態 + 下一個 session 要做的事（最重要）

- **Stage 2 rename 全部完成**：worklist **185/185 done、0 pending、0 anomaly**（`tools/src_refine/stage2_reconcile.py` 讀 live Ghidra fresh-dump 驗證）。最終 `build_fd2.py` + `eqcheck.py` = **PASS[RELOC]**（功能等價、零邏輯改動、0 undefined）。src 全 commit、Ghidra 全 `setName` + `save_program`。符號 rename 總數 75、param rename 279，皆已套用。
- **rename 階段不需再動**；下面「收尾」才是剩餘工作。

### 下一個 session 依序做（這兩項，照順序）

1. **23 個 logic issue**（`tools/src_refine/data/src_issues.json`，ID `ISS-0001..0023`）
   - 逐一深入分析、研究、解決（per CLAUDE.md：backlog 結束前要深入解決）。
   - 真的當下無法解決、且我確認無法處理，才寫進 `open_issues.md`。
   - 每個 issue 記錄了 address / 現象 / 相關 function；用 Ghidra MCP（decompile / disassemble / xref）親自查證，**嚴禁未驗證下結論**（per `feedback_no_unverified_claim`）。

2. **遺失 plate 重建**（前次 session 一次 Ghidra wedge 後 kill 重啟，遺失 wedge 前約 37 分鐘內 in-memory 的 plate；src/shard 全在 git 安全，只有 Ghidra plate 要重建）
   - 對每個 shard `ghidra_plate_action in (updated, created)`（**created 223 + updated 116 = 339 shard**）：驗 Ghidra 現有 plate 非空/與 src 註解相符，缺的就**依現行 src 註解**重套（data plate 走 `run_script_inline` 的 `cu.setComment(PLATE_COMMENT,…)`，見 memory `project_ghidra_data_plate_mechanism`）。
   - 一併更正所有 **Stage-2 改過名** 的 function/global plate（依現行 src 註解，含 param 名 drift）。已知至少：n=164（plate 仍寫「SFX trigger frame index」應為 SFX id）、n=168/169（pose 表 plate 仍帶舊 misnomer 措辭，名稱已 paired-swap 正確）。
   - shard 清單：`tools/src_refine/data/shards/rp*/*.json`（`ghidra_plate_action` 欄）。

### 其餘收尾項（先列著，等使用者決定何時做 — 勿自行開工）

- **次級引用一次性同步（使用者 2026-06-25 已批准，但時機待定）**：把 `tools/code_emit/data/routing.json`、`emit_issues.json`、相關 KB 文件裡**所有被改符號**（75 function/global + param）的引用，用 ledger 已知 old→new 映射**一次性**同步並驗證。注意：name-sweep 只抓「完整符號名」；**無前綴簡寫 prose** 抓不到（本 session 已手動補過 n=173 一處，類似情況需另查）。
- **零星 KB / 命名修正**：
  - n=173：`resource_info/fdfield.md` line 15 'portrait_load_buffer / portrait sprite list' vocabulary 應改成 char spawn-position table（ledger n=173 `CLOSEOUT-KB`）。
  - n=181：函數 `fd2_maybe_load_speed_mode_overlay` / `fd2_maybe_free_speed_mode_overlay` 名仍含 `_overlay`，但其 plate 已改成 SFX-bank 語意（plate 與名輕微不一致）；是否把函數改名（_overlay→_sfx_bank）由使用者定奪（非原 worklist 範圍）。
- **清理**：`git worktree remove` rp1-4（在 `C:/Users/fdpsf/Documents/fd2-wt/rp1..rp4`，branch refine-p1..4）；刪 `workspace/src_refine/` 暫存。

## 1. 必讀文件（依序）

1. `CLAUDE.md`、`index.md`（專案規範、工具）
2. 本檔
3. **進度真相（ledger）**：`tools/src_refine/data/stage2_progress.json`（`completed[]` 每筆 rename + `_gate_change`/`_gate_change2`/`_user_decision`/`CLOSEOUT-*` 註記）
4. **收尾資料**：`tools/src_refine/data/src_issues.json`（23 logic issue）；`tools/src_refine/data/shards/rp*/*.json`（plate 重建來源，`ghidra_plate_action` 欄）；`src_info.json` / `src_info_by_name.json`（address↔name 雙向查）
5. `tools/src_refine/_index.md`（全部 script 用法 + 流程）
6. memory：`project_src_refine_rename_eqcheck`（eqcheck 兩級 gate / COMDEF 重定位）、`project_ghidra_data_plate_mechanism`（plate 重建機制）、`feedback_no_unverified_claim`、`feedback_modification_sync_mandatory`、`feedback_no_surface_verify`、`feedback_ghidra_disconnect_handling`

## 2. 開工先確認工具（per CLAUDE.md）

Ghidra MCP 已開 FD2.LE；DOSBox-X 在 PATH（silent mode 自動化）；Watcom 9.5a 在 `C:\Users\fdpsf\Documents\WATCOM_9.5a\BIN`（DOSBox-X 內跑）。

## 3. 驗收 gate（Stage 2 已完成，但任何 plate/註解外的改動仍要驗）

- **`tools/src_refine/eqcheck.py`，兩級擇一 PASS**：**STRICT**（LE Fixup Record Table 外逐 byte 相同 + fixup 同 multiset）或 **RELOC**（STRICT 失敗 fallback：抹零候選+基準兩邊「每個 fixup site 值 + Fixup Record Table」後殘餘相同 ⇒ 純 COMDEF 重定位+重排、零邏輯改動）。
- RELOC 級因 n=175 加入：改未初始化 global 名會讓 wlink 依名稱重排 BSS、平移數 bytes，內嵌位址跟著變但行為不變（已逐 byte 證明良性）。參考檔 `data/baseline_eq.json`（含 `outside_sha256` / `fixup_multiset_sha256` / `residual_sha256`；baseline 從 commit `f83ec3c9` 重建 == `baseline_hash.txt` `ab5f110a…`）。
- 收尾若**只動 plate / 註解**（不改 src 識別字）：不影響 build，eqcheck 仍會 PASS；plate 純存在 Ghidra .gpr，改完 `save_program`。
- 詳見 memory `project_src_refine_rename_eqcheck`、ledger `_gate_change2`。

## 4. GLOBAL rename 機制（若收尾中發現需補改名才會用到）

- MCP `rename_global_variable`／`rename_data` 都**拒絕 `data_fd2_` 名**（強制 `g_` 前綴，與 `feedback_game_data_symbol_naming` 衝突）。故 global 改名一律用 `run_script_inline` 的 `setName(addr,newName,USER_DEFINED)`（一 call 一符號）。function／param 用 MCP `rename_function_by_address`／`rename_variables`。
- src 改名 per-item：裸 old_name grep（找 decl + def + 所有 reader + **prefix-collision**：old_name 是更長 symbol 前綴時 replace_all 會誤傷，改 surgical）+ 查 new-name 是否已存在 → replace_all 各檔（語意改名連 comment prose）→ `setName` → ledger + per-item commit。
- 註：`run_script_inline` 每次會印 Ghidra build-cache 編譯錯誤 spam（無害、重啟才清）。

## 5. 歷史參考（已完成，不需重做）

- **§4 merge**：octopus merge rp1..rp4 回 main（`ddd51113`，零衝突）；`merge_shards.py` 產出 `src_info.json` / `src_info_by_name.json` / `src_issues.json`（1016 shard）；merged build byte-identical to baseline。
- **Stage 1**：4 worktree（`fd2-wt/rp1..rp4`，branch `refine-p1..4`）平行跑 `src_refine.wf.js`，serial 逐 symbol、per-symbol commit；session-only auto-resume cron 已刪。教訓收進 memory `project_workflow_running_detection`、`feedback_ghidra_disconnect_handling`。
- **Stage 2 rename**：在 main 手動 per-item（嚴禁批次）逐一套用 75 符號名 + 279 param 名；gate 從 hash_check→eqcheck（STRICT）→再加 RELOC 級。完整每筆見 ledger `completed[]`。
- 工具：`stage2_worklist.json`（185 items）+ `stage2_item.py <n>`（看單筆）+ `stage2_reconcile.py`（live 比對）。

## 6. 硬性規則

- **絕不動遊戲邏輯**；改完非純註解的東西要 eqcheck PASS（STRICT 或 RELOC）。
- 程式碼內文字 ASCII（中文角色/道具/法術專名例外）；Ghidra 名與 src 名 byte-identical。
- 註解同步＝一致即可、src 為準。
- **嚴禁 Write 整檔覆寫 src**（會抹掉同檔其他符號）；用 Edit。durability：每完成一個 item 就 commit。
- 任何修改當下三同步 plate / KB / script（`feedback_modification_sync_mandatory`）；audit/verify 逐一不抽樣（`feedback_no_surface_verify`、`feedback_no_sampling_no_batch_verify`）。

## 7. 關鍵路徑

- 工具 + 用法：`tools/src_refine/_index.md`
- 耐久資料（tracked）：`tools/src_refine/data/`（baseline_hash.txt、baseline_eq.json、stage2_progress.json、shards/rpN/、src_info*.json、src_issues.json、eqcheck.py、stage2_reconcile.py）
- 可重生 scratch（gitignored）：`workspace/src_refine/`（ghidra dump tsv、worklist.json、stage2_item.py、stage2_worklist.py、pending.json）
- 4 worktree：`C:/Users/fdpsf/Documents/fd2-wt/rp1..rp4`（branch refine-p1..4，收尾時 remove）
