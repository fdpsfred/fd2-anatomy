# tools/code_emit/ — FD2 function emit + review pipeline

把 Ghidra 內 decompiled function 產出為 functionally-equivalent C、寫 unit test、
經 build gate 驗證、由 reviewer 獨立復驗、per-function commit 的整套工具與 workflow。

> **目前階段：Phase 2 手動 merge cascade**（function emit 已用 4-way 並行 branch 完成）。
> 新 session 先讀 `src/handoff.md` §1 的精確斷點與 §3 merge 方法論;**本檔是 emit/review
> workflow 的操作參考**（供 Phase 2.6 review 與任何補 emit 使用）。

## 元件

| 檔案 | 用途 |
|---|---|
| `emit_review.wf.js` | **Workflow 雙模式編排**。序列(一次一個 function):`review`(已 emit 復驗)/ `emit`(從零產出)→ reviewer → 迭代(≤ MAX_ROUNDS)→ bookkeeper per-function commit。內含 budget guard、try/catch(token/usage limit 優雅停)、reviewer 主動查 Ghidra 事實、output-token 統計。 |
| `coland.wf.js` | **coordinated landing Workflow**。emit 序列 / review 並行雙模式;orchestrator 擁有 spy 刪除 + data-land + build gate + commit,sub-agent 只做單函式三源 emit / 唯讀 review;供需同時碰共享 testglob spy + 多套件的 coordinated landing(blit / pathfind / composite)。 |
| `build_test.py` | **build gate**(single source of truth)。clean → 啟動 DOSBox-X 跑生成的 `workspace/emit_drive/run.conf`(依各 checkout 的 `REPO_ROOT` 重寫 C:/E: mount,**worktree-safe**;`tests/dosbox.conf` 僅為模板;遊戲檔來源 `$FD2_GAME_DIR`→local→主 repo fallback)(compile src+tests / link / run)→ 前景輪詢結束訊號 → 解析 `BUILD.OUT`/`TEST.OUT` → 回傳 JSON。三種結束訊號(無固定等待):①`DONE.TXT` 出現(正常完成);②**DOSBox process 退出**(`proc.poll()`,涵蓋正常完成與「會交回 batch 的 crash」如 DOS/4GW GP fault,~2s 即偵測);③**heartbeat 停滯**(`tests/OUT/HB.TXT` 每個 test open/write/close 一次;run 階段若停滯 `--hang-stall` 秒〔預設 20s〕且 proc 仍存活 → 判定 hang,真無窮迴圈的唯一偵測)。**DOSBox-X fault logging**:`gen_run_conf()` 注入 `[log] logfile=…/dosbox.log`、Popen 另導 stdout/stderr 到 `dosbox_stdio.log`,run 後掃兩檔的 protected-mode fault(`illegal descriptor`/GP/invalid opcode…)放進結果 `dosbox_fault`——cinematic 測試驅動 real composite 讀 garbage sprite → wild access → DOSBox-X 彈「illegal descriptor」modal 卡住被判 hang,此欄揭露「hang 其實是 fault」。回傳 `{gate_pass, build_ok, done, failure_mode(completed/crash/hang/aborted/timeout), hung_test, crash_dump, dosbox_fault, errors, warnings, tests_passed, tests_failed}`。**merge gate 看 `error_count==0 && warning_count==0`**(run 階段 hang/fail 忽略)。**`--only <substr>`**:暫時把 `tests/testmain.c` 的 GENBUILD calls 區塊濾成只呼叫名稱/路徑含該 substr 的 suite runner,讓該 suite 跑到完成而不被「執行順序在前的 suite 卡死」擋住(Phase 3 逐 suite 修復用);仍編譯全部 src+tests,只改 main() 呼叫哪些 runner,跑完所有路徑都會還原 testmain.c,**不碰 src/**。JSON 多回 `only_runners`。若硬中斷殘留 filtered 狀態,`python tests/genbuild.py --apply` 可從 marker 重生完整清單。 |
| `next_batch.py` | **scout 下一批 work-list**。從 `src/routing.json` 取 `done & !reviewed`(review 模式)或 `!done`(emit 模式),輸出 Workflow `args.functions`。`--stats` 看覆蓋率。 |

測試架構：`tests/` 下每個測試檔對應一個 src 子檔（`tests/<domain>/<stem>.c`）。落點查詢用 `tests/where.py`；`build.bat` 的 src/test 編譯區、`test.lnk`、`testmain.c` runner 清單全部由 `tests/genbuild.py` 從 `src/` 與 `tests/` 自動產生——新建任何 src 或測試 .c 檔後跑一次 `python tests/genbuild.py --apply` 即可接上 build，嚴禁手改這三個檔。

## 狀態 source of truth

`src/routing.json`:`address → {name, target, phase, done, asm, reviewed}`。
- `done` = 已 emit C;`reviewed` = 已經 workflow 復驗通過。
- **進度 = `reviewed` 欄**;per-function commit 是斷點。任何中斷後重跑零成本續做。

進度查詢:`python tools/code_emit/next_batch.py --stats`

## 新 session 如何繼續(session 被刪/換機都適用)

狀態全在磁碟 + git,不依賴任何對話 context。步驟:

1. 確認可用工具(Ghidra MCP 已開 FD2.LE / DOSBox-X 在 PATH / Watcom 9.5a)，per `CLAUDE.md`。
2. `python tools/code_emit/next_batch.py --stats` 看還剩多少 review / emit。
3. `git status`;若有上次中斷殘留的未 commit 改動 → `git checkout -- src tests` 清半成品。
4. scout 下一批:`python tools/code_emit/next_batch.py --mode review --limit 12`(review 全部做完再 `--mode emit`)。
5. 對 Ghidra live 對齊:`search_functions` 比對 routing.json，drift 拋警告(audit source-of-truth)。
6. 跑:`Workflow({scriptPath: "tools/code_emit/emit_review.wf.js", args: <上一步 next_batch 的 JSON>})`。
7. 跑完一批 hard-stop,報告 + 等使用者確認下一批。

### 硬性注意(踩過的坑)

- **Workflow `args` 經 tool-call 會被當 string** → script 已 `JSON.parse` 容錯;傳 work-list 照常傳即可。
- **emitter/reviewer 必須前景跑 `build_test.py`，嚴禁 `run_in_background`** — subagent 一交出最終訊息就結束、收不到背景通知、不閉環(且會留 dosbox 孤兒)。
- build gate 結束偵測無固定等待:`DONE.TXT` 出現 / DOSBox process 退出 / heartbeat(`HB.TXT`)停滯三訊號擇一(見上表);不存在 stale-cache / DPMI-OOM 問題(舊文件誤判,見 `src/handoff.md` §6)。**測試的 heartbeat 機制**:`testharn.h` 的 `TEST_BEGIN` 呼叫 `test_heartbeat()`(定義在 `testglob.c`),每個 test 用 fopen/fprintf/**fclose** 重寫 `E:\OUT\HB.TXT`;close 才會讓 DOSBox 把寫入 commit 到 host 檔(光 `fflush` 不會,DOSBox local-drive 會快取重導向 stdout 到 file close),所以 host 端輪詢看得到即時進度、卡住時 `HB.TXT` 凍在 hang 的 test 名。
- **路徑佈局**:compile cwd=`C:\`(=src),`.obj`→`tests/OUT/obj\`,`TEST.EXE`→`tests/OUT`,run 段 `cd \out` 使 TEST.EXE cwd=`tests/OUT`,**src/ 乾淨**。`build.bat`/`test.lnk` 由 `genbuild.py` 全產生,勿手改。
- **真實檔案測試 gate**:build_test.py 啟動前把 8 個遊戲檔從 `fd2_game_files/` stage 到 `tests/OUT`(=cwd,缺/size 不符才複製,不掛載)。讀檔 function 的 test 必須讀 staged 真檔、斷言真實解析值;禁捏造假檔(`write_fake_*`)、禁 remove() staged 真檔。reviewer checklist 7b 強制。見 memory `feedback_real_file_tests_mandatory`。
- 每批 ≤ 12(checkpoint 粒度);全程 Opus。
- **Ghidra 斷線＝純 event-driven schema 偵測,無心跳**:emitter/reviewer 任一 Ghidra MCP 失敗/逾時先快速重試一次,仍失敗才設結構化 `ghidra_unreachable=true`+`ghidra_error_detail` → `runAgent` fast-stop(`result.stopped=='ghidra_disconnect'`)→ 完成通知喚醒 → `connect_instance('FD2')` 探測:恢復則 relaunch、wedged 則 PushNotification 請使用者重啟。**多來源並發操作同一 Ghidra instance 無妨**(不靠 grep/字串/reviewed 停滯/liveness 判斷)。詳見 memory `feedback_ghidra_disconnect_handling`。

## 完整 fd2.exe 的位置

本 workflow 只處理 **function**(`routing.json` 650 個)。完整可執行檔還需:
A. 本 workflow(function emit+review,650)→ B. Phase 8 data emit(~1300 data items,真實 byte → C const;建議沿用同架構 per-item workflow)→ C. wlink 整合(src `.obj` + Watcom CLIB3S + AIL lib → LE)→ D. Layer 1 驗證(DOSBox scripted playtest 對比原版)。
規格見 `rebuild_info/emission/`(pipeline_spec / calling_convention / pool_routing)、進度與嚴格規則見 `src/handoff.md`。
