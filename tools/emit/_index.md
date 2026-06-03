# tools/emit/ — FD2 function emit + review pipeline

把 Ghidra 內 decompiled function 產出為 functionally-equivalent C、寫 unit test、
經 build gate 驗證、由 reviewer 獨立復驗、per-function commit 的整套工具與 workflow。

## 元件

| 檔案 | 用途 |
|---|---|
| `emit_review.wf.js` | **Workflow 雙模式編排**。序列(一次一個 function):`review`(已 emit 復驗)/ `emit`(從零產出)→ reviewer → 迭代(≤ MAX_ROUNDS)→ bookkeeper per-function commit。內含 budget guard、try/catch(token/usage limit 優雅停)、reviewer 主動查 Ghidra 事實、output-token 統計。 |
| `build_test.py` | **build gate**(single source of truth)。clean → 啟動 DOSBox-X 跑 `tests/dosbox.conf`(compile src+tests / link / run)→ 前景輪詢結束訊號 → 解析 `BUILD.OUT`/`TEST.OUT` → 回傳 JSON。三種結束訊號(無固定等待):①`DONE.TXT` 出現(正常完成);②**DOSBox process 退出**(`proc.poll()`,涵蓋正常完成與「會交回 batch 的 crash」如 DOS/4GW GP fault,~2s 即偵測);③**heartbeat 停滯**(`tests/OUT/HB.TXT` 每個 test open/write/close 一次;run 階段若停滯 `--hang-stall` 秒〔預設 20s〕且 proc 仍存活 → 判定 hang,真無窮迴圈的唯一偵測)。回傳 `{gate_pass, build_ok, done, failure_mode(completed/crash/hang/aborted/timeout), hung_test, crash_dump, errors, warnings, tests_passed, tests_failed}`。 |
| `next_batch.py` | **scout 下一批 work-list**。從 `src/routing.json` 取 `done & !reviewed`(review 模式)或 `!done`(emit 模式),輸出 Workflow `args.functions`。`--stats` 看覆蓋率。 |
| `mkroute.py` | routing.json 生成/管理(從 emit_functions.json + 規則)。`generate`/`status`/`mark`/`pending`/`validate`/`resplit`。大 subsystem 依子功能切成多個 ≤~1000 行 .c 的規則在 `_subsplit()`（見 `tools/file_split/`），`resplit` 把切分套到既有 routing.json。 |
| `count_cats.py` / `dump_emit_functions.java` | 既有分類計數 / Ghidra dump 工具。 |

測試架構：`tests/` 下每個測試檔對應一個 src 子檔（`tests/<domain>/<stem>.c`）。落點查詢用 `tests/where.py`；`build.bat` 的 src/test 編譯區、`test.lnk`、`testmain.c` runner 清單全部由 `tests/genbuild.py` 從 `src/` 與 `tests/` 自動產生——新建任何 src 或測試 .c 檔後跑一次 `python tests/genbuild.py --apply` 即可接上 build，嚴禁手改這三個檔。一次性的大規模切分／搬移工具在 `tools/test_split/`（細節見 `tests/_index.md`）。

## 狀態 source of truth

`src/routing.json`:`address → {name, target, phase, done, asm, reviewed}`。
- `done` = 已 emit C;`reviewed` = 已經 workflow 復驗通過。
- **進度 = `reviewed` 欄**;per-function commit 是斷點。任何中斷後重跑零成本續做。

進度查詢:`python tools/emit/next_batch.py --stats`

## 新 session 如何繼續(session 被刪/換機都適用)

狀態全在磁碟 + git,不依賴任何對話 context。步驟:

1. 確認可用工具(Ghidra MCP 已開 FD2.LE / DOSBox-X 在 PATH / Watcom 9.5a)，per `CLAUDE.md`。
2. `python tools/emit/next_batch.py --stats` 看還剩多少 review / emit。
3. `git status`;若有上次中斷殘留的未 commit 改動 → `git checkout -- src tests` 清半成品。
4. scout 下一批:`python tools/emit/next_batch.py --mode review --limit 12`(review 全部做完再 `--mode emit`)。
5. 對 Ghidra live 對齊:`search_functions` 比對 routing.json，drift 拋警告(audit source-of-truth)。
6. 跑:`Workflow({scriptPath: "tools/emit/emit_review.wf.js", args: <上一步 next_batch 的 JSON>})`。
7. 跑完一批 hard-stop,報告 + 等使用者確認下一批。

### 硬性注意(踩過的坑)

- **Workflow `args` 經 tool-call 會被當 string** → script 已 `JSON.parse` 容錯;傳 work-list 照常傳即可。
- **emitter/reviewer 必須前景跑 `build_test.py`，嚴禁 `run_in_background`** — subagent 一交出最終訊息就結束、收不到背景通知、不閉環(且會留 dosbox 孤兒)。
- build gate 結束偵測無固定等待:`DONE.TXT` 出現 / DOSBox process 退出 / heartbeat(`HB.TXT`)停滯三訊號擇一(見上表);不存在 stale-cache / DPMI-OOM 問題(舊文件誤判,見 `src/handoff.md` §8)。**測試的 heartbeat 機制**:`testharn.h` 的 `TEST_BEGIN` 呼叫 `test_heartbeat()`(定義在 `testglob.c`),每個 test 用 fopen/fprintf/**fclose** 重寫 `E:\OUT\HB.TXT`;close 才會讓 DOSBox 把寫入 commit 到 host 檔(光 `fflush` 不會,DOSBox local-drive 會快取重導向 stdout 到 file close),所以 host 端輪詢看得到即時進度、卡住時 `HB.TXT` 凍在 hang 的 test 名。DOSBox crash/hang 行為實證見 `tools/hangprobe/`。
- **路徑佈局**:compile cwd=`C:\`(=src),`.obj`→`tests/OUT/obj\`,`TEST.EXE`→`tests/OUT`,run 段 `cd \out` 使 TEST.EXE cwd=`tests/OUT`,**src/ 乾淨**。`build.bat`/`test.lnk` 由 `genbuild.py` 全產生,勿手改。
- **真實檔案測試 gate**:build_test.py 啟動前把 7 個遊戲檔從 `fd2_game_files/` stage 到 `tests/OUT`(=cwd,缺/size 不符才複製,不掛載)。讀檔 function 的 test 必須讀 staged 真檔、斷言真實解析值;禁捏造假檔(`write_fake_*`)、禁 remove() staged 真檔。reviewer checklist 7b 強制。見 memory `feedback_real_file_tests_mandatory`。
- 每批 ≤ 12(checkpoint 粒度);全程 Opus。
- **Ghidra 斷線＝純 event-driven schema 偵測,無心跳**:emitter/reviewer 任一 Ghidra MCP 失敗/逾時先快速重試一次,仍失敗才設結構化 `ghidra_unreachable=true`+`ghidra_error_detail` → `runAgent` fast-stop(`result.stopped=='ghidra_disconnect'`)→ 完成通知喚醒 → `connect_instance('FD2')` 探測:恢復則 relaunch、wedged 則 PushNotification 請使用者重啟。**多來源並發操作同一 Ghidra instance 無妨**(不靠 grep/字串/reviewed 停滯/liveness 判斷)。詳見 `src/handoff.md` §5。

## 完整 fd2.exe 的位置

本 workflow 只處理 **function**(`routing.json` 653 個)。完整可執行檔還需:
A. 本 workflow(function emit+review,653)→ B. Phase 8 data emit(~1300 data items,真實 byte → C const;建議沿用同架構 per-item workflow)→ C. wlink 整合(src `.obj` + Watcom CLIB3S + AIL lib → LE)→ D. Layer 1 驗證(DOSBox scripted playtest 對比原版)。
規格見 `rebuild_info/emission/`(pipeline_spec / calling_convention / pool_routing)、進度與嚴格規則見 `src/handoff.md`。
