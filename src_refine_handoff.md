# src_refine 交接文件（新 session 接續用）

對 `src/` 內每個 symbol（function + global）逐一解析用途/邏輯，refine 名稱、參數名、註解，並同步 Ghidra。
**只改名稱與註解，遊戲邏輯不動**；硬性驗收＝功能等價：Stage 1/§4（無 rename）是整檔 byte-identical；Stage 2 rename 後改用 `eqcheck.py`（code/data+header 逐 byte 相同 + LE fixup table 同 multiset），原因見 §0。
完整工具說明見 `tools/src_refine/_index.md`（本檔不重複，只給接續所需）。

## 0. 目前狀態（最重要）

- **§4 全部完成**：octopus merge rp1..rp4 回 main（commit `ddd51113`，零衝突）；`merge_shards.py` 產出 `data/src_info.json` / `src_info_by_name.json` / `src_issues.json`（1016 shard）；最終 merged build gate **byte-identical to baseline**（`ab5f110a…`）。Stage 1 機制（§3）已是歷史。
- **4 個 uncertain 已複審定奪**：3 改判 rename（`crt_equivalent_lx_module_loader`、`fd2_get_orphan_packed3_table_entry`、`data_fd2_animation_spell_sfx_id_table`）、1 keep（`fd2_chapter_event_handler_40__unref_dyn_turn_event`）。符號 rename 總數 72→**75**。
- **正在執行 §5（Stage 2 rename），進度 source of truth ＝ `tools/src_refine/data/stage2_progress.json`（ledger）**。worklist ＝ `workspace/src_refine/stage2_worklist.json`（185 work items；不在則用 `stage2_worklist.py` 重生）。新 session：讀 ledger 看 `completed[]`、`stage2_item.py <n>` 看下一個未做 item 詳情、續做。
- **關鍵：Stage 2 rename 無法整檔 byte-identical**（rename 擾動 LE fixup record 順序，已 root-cause、功能等價）。**驗收 gate 改用 `tools/src_refine/eqcheck.py`（非 hash_check.py）**，參考 `data/baseline_eq.json`。詳見 memory `project_src_refine_rename_eqcheck`。
- baseline hash：`ab5f110af02608462a1da464732c098ef4dc8e17e6dc43252b535ed1cbf3bd9b`（`data/baseline_hash.txt`，仍是 Stage-1/§4 的 byte-identical 基準）。

### 待解項（收尾前必清，per CLAUDE.md）

- **[DEFERRED] 遺失 plate 重建**：執行中曾發生一次 Ghidra MCP wedge（hang，非乾淨斷線），4 workflow 卡 `running` 不前進、不 fast-stop；kill+重啟 Ghidra 恢復。Ghidra 最後存檔在 wedge 前約 37 分鐘，期間 refiner 建立/更新的 plate（in-memory）隨 kill 遺失。src/shard 全在 git **安全**；遺失的只有 Ghidra plate，**可重建**。**收尾必做**：對每個 `ghidra_plate_action in (updated,created)` 的 shard，驗 Ghidra 現有 plate 非空/相符，缺的就依 src 註解重套。防線已加：refiner 改 plate 後立即 `save_program` 落地（src_refine.wf.js 步驟 D3），之後不會再大量遺失。
- **23 個 logic issue**：在 `src_issues.json`（ISS-0001..0023）；收尾前深入處理，真不行才寫 `open_issues.md`。
- **Stage 2 rename 進行中**：ledger `stage2_progress.json` 追蹤；每 ~50 個跑 `eqcheck.py`；prefix-collision 風險用裸 old_name grep 防（見 memory）；每改一個 commit。

## 1. 必讀文件（依序）

1. `CLAUDE.md`、`index.md`（專案規範、工具）
2. 本檔
3. **進度 source of truth（ledger）**：`tools/src_refine/data/stage2_progress.json`（`completed[]` 列每個已做 item + `_gate_change` 註記；續做時先讀它看做到哪）
4. **Stage 2 work-list 與工具**：`workspace/src_refine/stage2_worklist.json`（185 work items，缺則 `python workspace/src_refine/stage2_worklist.py` 重生）；`python workspace/src_refine/stage2_item.py <n>`（看單一 item 詳情/reason）；`tools/src_refine/eqcheck.py`（功能等價 gate）+ `tools/src_refine/data/baseline_eq.json`（gate 參考）
5. `tools/src_refine/_index.md`（全部 script 用法 + 流程 + 硬性注意）
6. memory：`project_src_refine_rename_eqcheck`（**Stage 2 必讀**：byte-identical 不可達成 / eqcheck gate / prefix-collision 防線）、`feedback_no_python_derive_for_mass_rename`、`feedback_strict_one_at_a_time`、`feedback_modification_sync_mandatory`、`feedback_asm_review_every_fn`、`feedback_ghidra_disconnect_handling`

## 2. 開工先確認工具（per CLAUDE.md）

Ghidra MCP 已開 FD2.LE；DOSBox-X 在 PATH（silent mode 自動化）；Watcom 9.5a 在 `C:\Users\fdpsf\Documents\WATCOM_9.5a\BIN`（DOSBox-X 內跑）。

## 3. Stage 1 執行機制（已完成，歷史參考）

Stage 1 用 4 worktree（`fd2-wt/rp1..rp4`，branch `refine-p1..4`）平行跑 `src_refine.wf.js`，serial 逐 symbol、一 refiner agent 一 symbol、per-symbol commit（撞 limit/斷線零損失）。撞 usage-limit 與 Ghidra wedge 都靠一個 session-only auto-resume cron 自動續跑/復原。**這套機制已完成、cron 已刪，不需再執行**。關鍵教訓已收進 memory：
- `TaskList` 查不到 `local_workflow` 背景任務，要用 `TaskOutput({block:false})` 看 `<status>running</status>` 偵測 RUNNING（task ID 持久化在 `workspace/src_refine/running_tasks.json`）——見 `project_workflow_running_detection`。
- hang 型 Ghidra wedge：workflow 卡 `running` 零進展且不 fast-stop；偵測＝最後 commit 停滯（>40 分）+ ping `get_current_program_info` timeout → TaskStop + kill ghidra javaw + 移除 project lock + `ghidraRun.bat` + 等 port 8089 + `open_program('/FD2.LE')`——見 `feedback_ghidra_disconnect_handling`。

若未來要重跑 refine，照 git 歷史的 cron prompt 與 `tools/src_refine/_index.md`。

## 4. merge + merge_shards（已完成；歷史參考）

1. 確認 4 partition 都 committed==total（已達成）。
2. merge 4 branch 回 main（comment 改動 file-disjoint、shards 在各自 rpN 子夾，理論無衝突）：
   `git checkout main; git merge refine-p1 refine-p2 refine-p3 refine-p4`（或逐一 merge；衝突極不該發生，有就停下查）。
3. `python tools/src_refine/merge_shards.py` → 產出 `tools/src_refine/data/src_info.json`（address 主鍵）、
   `src_info_by_name.json`（name→addr，可雙向查）、`src_issues.json`（ISS-####）。會印 symbol/param rename 與 issue 總數（應對上 §0：72 / 279 / 23）。
4. main 跑**最終 merged build gate**：`python tools/fd2_build/build_fd2.py` + `python tools/src_refine/hash_check.py` 須 ==baseline。此為全 1016 註解的 byte-identical 總驗收（rp1 已單獨 PASS）。

## 5. Stage 2：序列套用所有 rename（在 main，手動 per-item，嚴禁批次）— **進行中**

清單＝`stage2_worklist.json`（185 work items：**75 符號名** rename + 123 function 的 **279 參數名** rename，13 個兩者皆需）。進度＝ledger；下一個未做 item 用 `stage2_item.py <n>` 看詳情。4 個 uncertain 已複審（§0）。使用者指示**一路跑到完成**（不每 50 hard-stop；每 ~50 跑 eqcheck gate，只在 build 失敗/判斷不明時停）。
**逐一處理，每個 per-item 親自檢視 + single-MCP 套用**（per `feedback_no_python_derive_for_mass_rename`、`feedback_strict_one_at_a_time`）：

- **符號名 rename**：`get_xrefs_to` 親自確認 call site → **裸 old_name（非 `\b`）grep 找所有出現處並查 prefix-collision** → 改 def + protos.h/globals.h 宣告 + 所有 call site（Edit replace_all；old_name 若是更長 symbol 前綴則改用 surgical edit，見 memory）→ Ghidra `rename_function_by_address` → 同步 ledger → per-rename commit。
- **參數名 rename**：src 多數已用 proposed 名（只需 Ghidra `rename_variables` 同步）；src 若用別名則 surgical 改 def 簽章 + body + protos.h decl。swap 撞名靠 rename_variables 的最終態（中間態 error 可忽略，事後 `get_function_by_address` 驗）。
- 每 ~50 個跑 `build_fd2.py` + **`eqcheck.py`**（功能等價、0 undefined；**非 hash_check**——byte-identical 對 rename 不可達成）。outside-fixup 不符＝真 code/data 改動（誤傷），bisect 補。
- 位址殘留 vs 領域 ID：`name_reason` 已記判斷；可疑時用 `fd2-knowledge` skill 或 assets/ KB 復查，**嚴禁 regex 機械剝除**。
- 路徑名 stale：git checkout 過 src 後，harness file-state 失效，編輯前要重新 Read 該檔。

## 6. 最終驗證 + 收尾

1. 全 scope 處理完，main 跑 `build_fd2.py` + **`eqcheck.py`** → **功能等價必須 PASS**（code/data+header 逐 byte 相同 + fixup table 同 multiset；證明零邏輯改動。byte-identical 對 rename 不可達成，見 §0）。
2. 收尾 live Ghidra reconcile：重新全量 dump（run_script_inline 寫 ghidra_functions/symbols.tsv）→ 驗每個 scope symbol 的 `name.final == live 當前名`、無未處理、無新出現未納入 scope symbol。
3. `src_info.json` 每個 scope symbol `processed==true`。
4. 處理 §0 待解項：遺失 plate 重建 + `src_issues.json` 23 issue（per CLAUDE.md：backlog 結束前深入解決，真不行才寫 `open_issues.md`）。
5. 清理：`git worktree remove` rp1-4（merge 後）；刪 `workspace/src_refine/` 暫存（含 running_tasks.json、_parse_wf.py）。（auto-resume cron 已刪。）

## 7. 硬性規則

- 只改名稱與註解，**絕不動遊戲邏輯**；總驗收＝Stage 1/§4 byte-identical、Stage 2 用 eqcheck 功能等價。
- 程式碼內文字 ASCII（中文角色/道具/法術專名例外）；Ghidra 名與 src 名 byte-identical。
- 註解同步＝一致即可、src 為準（非 byte-identical）。
- per-rename commit（durability）；Edit 改檔、**嚴禁 Write 整檔覆寫 src**（會抹掉同檔其他符號）。
- Stage 2 rename：嚴禁 Python derive + bulk apply / inline Java batch；一律 per-item single-MCP（per memory）。

## 8. 關鍵路徑

- 工具 + 用法：`tools/src_refine/_index.md`
- 耐久資料（tracked）：`tools/src_refine/data/`（baseline_hash.txt、shards/rpN/、merge 後的 src_info*.json / src_issues.json）
- 可重生 scratch（gitignored）：`workspace/src_refine/`（ghidra dump、worklist.json、coverage 報告、partition manifest、args、running_tasks.json）
- 4 worktree：`C:/Users/fdpsf/Documents/fd2-wt/rp1..rp4`（branch refine-p1..4）
- Stage 2 gate：`eqcheck.py`（+ `data/baseline_eq.json`）；ledger：`data/stage2_progress.json`；work-list：`workspace/src_refine/stage2_worklist.json` + `stage2_item.py`
- Stage 1 歷史：workflow `src_refine.wf.js`；scout `scout.py`；§4 byte-identical gate `hash_check.py`；merge `merge_shards.py`
