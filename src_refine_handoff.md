# src_refine 交接文件（新 session 接續用）

對 `src/` 內每個 symbol（function + global）逐一解析用途/邏輯，refine 名稱、參數名、註解，並同步 Ghidra。
**只改名稱與註解，遊戲邏輯不動**；硬性驗收＝重建的 production FD2.EXE 與 baseline byte-identical。
完整工具說明見 `tools/src_refine/_index.md`（本檔不重複，只給接續所需）。

## 0. 目前狀態（最重要）

- **Stage 1（逐 symbol refine，4 worktree 平行）進行中，未完成**。已 commit：
  - rp1 100/253、rp2 82/253、rp3 86/255、rp4 84/255 = **352 / 1016**。剩 ~664。
  - 撞過一次 usage limit（per-symbol commit 全 durable，零損失）。目前 4 個 workflow 都**停著**（已手動停）。
- 累計（僅這 352 個，partial）：符號改名 ~19、**參數改名 ~98**、logic issue ~10。最終數字待 Stage 1 全完成後 `merge_shards.py` 統計。
- baseline hash：`ab5f110af02608462a1da464732c098ef4dc8e17e6dc43252b535ed1cbf3bd9b`（在 `tools/src_refine/data/baseline_hash.txt`）。
- **尚未做**：Stage 2（套用 symbol + 參數 rename）、merge 回 main、merge_shards、最終 build gate、收尾 reconcile、處理 src_issues。

## 1. 必讀文件（依序）

1. `CLAUDE.md`、`index.md`（專案規範、工具）
2. 本檔
3. `tools/src_refine/_index.md`（全部 script 用法 + 流程 + 硬性注意 + auto-resume 機制）
4. memory：`feedback_no_python_derive_for_mass_rename`、`feedback_strict_one_at_a_time`、`feedback_modification_sync_mandatory`、`feedback_asm_review_every_fn`、`feedback_ghidra_disconnect_handling`
5. `workspace/src_refine/coverage_reconcile_start.md`（起手 scope 對齊報告）

## 2. 開工先確認工具（per CLAUDE.md）

Ghidra MCP 已開 FD2.LE；DOSBox-X 在 `C:\DOSBox-X\dosbox-x.exe`；Watcom 9.5a 在 `C:\Users\fdpsf\Documents\WATCOM_9.5a\BIN`。

## 3. 第一步：重建 auto-resume timer cron（必做）

cron 是 **session-only**（不跨 session），新 session 一定要重建，否則撞 limit 後不會自動續跑。
先 `CronList` 看有沒有 prompt 開頭是 `[src_refine auto-resume timer]` 的 job；沒有就 `CronCreate({cron:"37 * * * *", recurring:true, durable:true, prompt:<下方逐字>})`：

```
[src_refine auto-resume timer] Hourly timer: just a reminder to CHECK src_refine Stage 1 and resume it only if it stalled on a usage limit. The user pre-authorized this auto-resume, so calling the Workflow tool here is approved. Be terse; do ONLY the steps below, then END the turn (never wait for workflows):

1. If you lack context, read C:/Users/fdpsf/Documents/fd2-anatomy/tools/src_refine/_index.md and repo-root src_refine_handoff.md.
2. committed per partition = `find C:/Users/fdpsf/Documents/fd2-wt/<P>/tools/src_refine/data/shards/<P> -name '*.json' | wc -l`. Totals: rp1=253, rp2=253, rp3=255, rp4=255.
3. Call TaskList. A partition <P> is RUNNING iff a Workflow/local_workflow task with label 'refine-<P>' is in_progress.
4. Decide PER partition:
   - RUNNING  -> NO-OP (skip; never relaunch a running partition).
   - committed==total -> done (skip).
   - committed<total AND not running (= stalled, usually the usage limit) -> organize state then relaunch: in wt=C:/Users/fdpsf/Documents/fd2-wt/<P>, if `git -C "$wt" status --porcelain` is non-empty run `git -C "$wt" add -A && git -C "$wt" commit -m "src-refine: shard backfill (auto-resume cleanup)" -m "Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"`; then Workflow({scriptPath:"tools/src_refine/src_refine.wf.js", args:{root:"$wt", partition:"<P>", label:"refine-<P>"}}).
5. If ALL four committed==total -> Stage 1 COMPLETE: CronList then CronDelete the job whose prompt starts '[src_refine auto-resume timer]'; PushNotification "src_refine Stage 1 complete (1016/1016); Stage 2 (merge + symbol/param rename) can begin per tools/src_refine/_index.md"; do NOT start Stage 2 yourself. End.
6. If you are still usage-limited and cannot act at all, do nothing -- the next hourly fire retries automatically.
```

**唯一 driver 原則**：同時只能有一個 session 驅動（cron 跨 session 不可見，兩個 driver 會搶開同一 partition 而毀檔）。所以舊 session 要先關掉（其 session-only cron 與 workflow 隨之消失），再讓這個新 session 重建 cron 當唯一 driver。重建後可等 cron 下次 :37 自動啟動，或照 §4「手動立即啟動」即時開跑。

cron 只是 timer：撞 limit 時 session 不死，cron 每小時 fire，limit reset 後那次 fire 就會重啟停掉的 workflow（limit 期間的 fire 因 LLM 無 capacity 自動 no-op，下次再試）。

## 4. 續跑 Stage 1（讓 cron 自動跑，或手動立即啟動）

**自動**：cron 下一次 :37 fire 會重啟所有「停著且未完成」的 partition。

**手動立即啟動**（不想等 cron）：對每個未完成 partition `<P>`（rp1..rp4）：
```
# 先清掉未 commit 的 shard backfill（若有）
git -C C:/Users/fdpsf/Documents/fd2-wt/<P> status --porcelain   # 非空才需要
git -C C:/Users/fdpsf/Documents/fd2-wt/<P> add -A && git -C ... commit -m "src-refine: shard backfill (resume cleanup)"
```
然後啟動（args 極小，workflow 內部 bootstrap 會 scout 出未完成 symbol）：
```
Workflow({scriptPath:"tools/src_refine/src_refine.wf.js",
          args:{root:"C:/Users/fdpsf/Documents/fd2-wt/<P>", partition:"<P>", label:"refine-<P>"}})
```
進度查詢：`find C:/Users/fdpsf/Documents/fd2-wt/<P>/tools/src_refine/data/shards/<P> -name '*.json' | wc -l`
vs 總數 rp1=253 rp2=253 rp3=255 rp4=255。
**嚴禁同一 partition 開兩個 workflow**（會競寫同檔）；先 TaskList 確認沒在跑。

每個 partition 跑完後在該 worktree 跑 build gate（須 byte-identical）：
```
python C:/Users/fdpsf/Documents/fd2-wt/<P>/tools/fd2_build/build_fd2.py --timeout 360
python C:/Users/fdpsf/Documents/fd2-wt/<P>/tools/src_refine/hash_check.py   # 須 PASS
```

## 5. Stage 1 全完成後 → merge + merge_shards

1. 4 partition 都 committed==total 且各自 build gate PASS。
2. 把 4 個 branch merge 回 main（comment 改動 file-disjoint，shards 在各自 rpN 子夾，merge 無衝突）：
   `git checkout main; git merge refine-p1 refine-p2 refine-p3 refine-p4`（或逐一 merge；衝突極不該發生，有就停下查）。
3. `python tools/src_refine/merge_shards.py` → 產出 `tools/src_refine/data/src_info.json`（address 主鍵）、
   `src_info_by_name.json`（name→addr，可雙向查）、`src_issues.json`（ISS-####）。會印 symbol/param rename 與 issue 總數。
4. main 跑最終 build gate：`python tools/fd2_build/build_fd2.py` + `python tools/src_refine/hash_check.py` 須 ==baseline。

## 6. Stage 2：序列套用所有 rename（在 main，手動 per-item，嚴禁批次）

清單來源＝`src_info.json`：`name_verdict=="rename"`（符號名）+ 任何 `params[].verdict=="rename"`（參數名）。
**逐一處理，每個 per-item 親自檢視 + single-MCP 套用**（per `feedback_no_python_derive_for_mass_rename`、`feedback_strict_one_at_a_time`；每 50 個 hard-stop）：

- **符號名 rename**：`get_xrefs_to` 親自確認所有 call site → 改 def 名 + protos.h/globals.h 宣告 + 所有 call site（Grep 找）→ Ghidra `rename_function`/`rename_global_variable`（與 src byte-identical）→ 同步 routing.json/worklist 的 name 欄 → per-rename commit。
- **參數名 rename**：改該 function 的 def 簽章 + body 內該參數所有使用處 + protos.h 該 prototype 的參數名 → Ghidra `set_function_prototype`（參數名同步）→ per-rename commit。
- 每 50 個 rename 跑一次 `build_fd2.py` + `hash_check.py`（須 ==baseline，0 undefined）。編譯失敗＝call site 漏改，git log 在這批內二分補。
- 注意位址殘留 vs 領域 ID：`name_reason`/`reason` 已記判斷；可疑時用 `fd2-knowledge` skill 或 assets/ KB 復查，**嚴禁 regex 機械剝除**。

## 7. 最終驗證 + 收尾

1. 全 scope 處理完，main 跑 `build_fd2.py` + `hash_check.py` → **production FD2.EXE 必須 ==baseline**（byte-identical 證明零邏輯改動）。
2. 收尾 live Ghidra reconcile：重新全量 dump（run_script_inline 寫 ghidra_functions/symbols.tsv）→ 驗每個 scope symbol 的 `name.final == live 當前名`、無未處理、無新出現未納入 scope symbol。
3. `src_info.json` 每個 scope symbol `processed==true`。
4. 處理 `src_issues.json`（per CLAUDE.md：backlog 結束前要深入解決，真不行才寫 `open_issues.md`）。
5. 清理：CronDelete auto-resume cron；`git worktree remove` rp1-4（merge 後）；刪 workspace/src_refine/_parse_wf.py 暫存。

## 8. 硬性規則

- 只改名稱與註解，**絕不動遊戲邏輯**；byte-identical build 是總驗收。
- Stage 1 refiner 只**記錄** rename（符號名 + 參數名），**絕不在 Stage 1 套用**；rename 一律 Stage 2 序列。
- 程式碼內文字 ASCII（中文角色/道具/法術專名例外）；Ghidra 名與 src 名 byte-identical。
- 註解同步＝一致即可、src 為準（非 byte-identical）。
- per-symbol / per-rename commit（durability）；Edit 改註解、嚴禁 Write 整檔覆寫。
- 撞 limit：workflow 連續 3 個 agent null 會 fast-stop（stopped=usage_limit_suspected）；靠 cron 自動續跑。

## 9. 關鍵路徑

- 工具 + 用法：`tools/src_refine/_index.md`
- 耐久資料（tracked）：`tools/src_refine/data/`（baseline_hash.txt、shards/rpN/、merge 後的 src_info*.json / src_issues.json）
- 可重生 scratch（gitignored）：`workspace/src_refine/`（ghidra dump、worklist.json、coverage 報告、partition manifest、args）
- 4 worktree：`C:/Users/fdpsf/Documents/fd2-wt/rp1..rp4`（branch refine-p1..4）
- workflow：`tools/src_refine/src_refine.wf.js`；scout：`scout.py`；build gate：`hash_check.py`；merge：`merge_shards.py`；limit 等待：`limit_wait.py`
