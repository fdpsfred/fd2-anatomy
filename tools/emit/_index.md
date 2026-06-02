# tools/emit/ — FD2 function emit + review pipeline

把 Ghidra 內 decompiled function 產出為 functionally-equivalent C、寫 unit test、
經 build gate 驗證、由 reviewer 獨立復驗、per-function commit 的整套工具與 workflow。

## 元件

| 檔案 | 用途 |
|---|---|
| `emit_review.wf.js` | **Workflow 雙模式編排**。序列(一次一個 function):`review`(已 emit 復驗)/ `emit`(從零產出)→ reviewer → 迭代(≤ MAX_ROUNDS)→ bookkeeper per-function commit。內含 budget guard、try/catch(token/usage limit 優雅停)、reviewer 主動查 Ghidra 事實、output-token 統計。 |
| `build_test.py` | **build gate**(single source of truth)。clean → 啟動 DOSBox-X 跑 `tests/dosbox.conf`(compile src+tests / link / run)→ **前景輪詢 `tests/OUT/DONE.TXT`** → 解析 `BUILD.OUT`/`TEST.OUT` → 回傳 JSON。`{gate_pass, build_ok, errors, warnings, tests_passed, tests_failed}`。 |
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
- build gate 唯一正確完成訊號 = `DONE.TXT` 出現;不存在 stale-cache / DPMI-OOM 問題(舊文件誤判,見 `src/handoff.md` §8)。
- 每批 ≤ 12(checkpoint 粒度);全程 Opus。

## 完整 fd2.exe 的位置

本 workflow 只處理 **function**(`routing.json` 653 個)。完整可執行檔還需:
A. 本 workflow(function emit+review,653)→ B. Phase 8 data emit(~1300 data items,真實 byte → C const;建議沿用同架構 per-item workflow)→ C. wlink 整合(src `.obj` + Watcom CLIB3S + AIL lib → LE)→ D. Layer 1 驗證(DOSBox scripted playtest 對比原版)。
規格見 `rebuild_info/emission/`(pipeline_spec / calling_convention / pool_routing)、進度與嚴格規則見 `src/handoff.md`。
