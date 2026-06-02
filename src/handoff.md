# FD2 Emit Pipeline — Handoff

把 Ghidra 內 FD2.LE 的 decompiled function 產出為 functionally-equivalent（Layer 2）的 C
source、寫 unit test、經 build gate + 獨立 reviewer 三源復驗、per-function commit。
**已工具化為 `tools/emit/` 的 emit-review workflow；讀完本檔即可用 §5 的 /loop 指令全自動接續。**

---

## 0. 新 session 快速啟動

### 必讀文件（依序）

1. 本檔 `src/handoff.md` — 現況、工作方式、checkpoint、鐵則（讀完即可開工）
2. `tools/emit/_index.md` — workflow 操作指南（元件、跑批步驟、踩過的坑）
3. `~/.claude/plans/wiggly-skipping-ripple.md` — 整體計畫與規格（目錄結構 / header 來源 / AIL / Watcom build flags / Phase 8-9 規劃 / 驗證標準）
4. 需要時：`rebuild_info/emission/`（pipeline_spec / calling_convention / pool_routing）、`rebuild_info/link/wlink_settings.md`。
   （`CLAUDE.md` / `index.md` / `MEMORY.md` 由 session 自動載入，含 `project_emit_review_workflow` memory。）

### 開工步驟

1. 確認工具：Ghidra MCP 已開 FD2.LE、DOSBox-X 在 PATH、Watcom 9.5a（per `CLAUDE.md`）。不可用就停下問使用者。
2. 看現況（單一事實來源，不靠任何對話記憶）：`python tools/emit/next_batch.py --stats`
   → `{total, emitted, reviewed, await_review, await_emit}`。`reviewed` 欄就是進度。
3. 全自動接續：使用者貼 §5 的 /loop 指令 → 自動推進直到 `await_review` 與 `await_emit` 皆為 0。

---

## 1. 待解 / 注意（處理到才碰，不必預先動）

- **完整可執行 fd2.exe ≠ 本 workflow 產物**。本 workflow 只做 **function**（653 個）。完整 exe 還需 §4 的 B/C/D。
- **src .c 已依子功能切分為 ≤~1000 行的子檔**；routing target 即子檔（切分規則在 `tools/emit/mkroute.py` 的 `_subsplit()`，檔案↔function 對照見 `src/routing.md`，分析/搬移工具見 `tools/file_split/`）。emit 新 function 照 routing target 落到對應子檔。測試鏡像 src 子檔（`tests/<domain>/<stem>.c`；落點查 `tests/where.py`）；新測試子檔跑 `python tests/genbuild.py --apply` 自動接上 build（per §7）。
- `src/emit_issues.json`：累積「需實際編譯才能確認的等價性疑慮」（FPU rounding / word width / table-copy / fragment 等價轉移到 parent…）。**留待 Phase 8（全 function 完成後）統一用 Watcom 9.5a 編譯 + disasm 比對解決**，不在 function review 階段處理。key 一律用 routing.json 同款 8-hex（如 `00010b43`），utf-8。
---

## 2. 工作方式：emit-review workflow

進度單一事實來源 = `src/routing.json` 的 `reviewed` 欄（+ `done`）。每個 function 的終態 =
reviewer approved + build gate green + per-function commit。

每批流程：

1. **scout**：`python tools/emit/next_batch.py --mode review --limit 12`（`review` 全做完再 `--mode emit`）。輸出即 Workflow 的 `args.functions`。
2. **對齊**：`search_functions` 比對 routing↔Ghidra，drift 拋警告（audit source-of-truth）。
3. **清半成品**：`git checkout -- src tests`（清上次中斷殘留）。
4. **跑**：`Workflow({scriptPath:"tools/emit/emit_review.wf.js", args:<next_batch JSON>})`。
   - 序列一次一個 function；`review` 模式直接 reviewer，`emit` 模式先 emitter；reviewer 獨立三源復驗 → 迭代（≤10 round）→ approved → bookkeeper per-function commit（code+test+KB + `routing.reviewed=true` + emit_issues）。
5. **驗證**：build gate 綠（除既存 warning）、`reviewed` 數增加、commit 乾淨、無 dosbox 孤兒。

中斷（token/usage limit）後重跑零成本續：`reviewed` 欄 + per-function commit = 斷點；workflow 內 try/catch 在 function 邊界優雅停。**禁忌與細節見 `tools/emit/_index.md`。**

---

## 3. Checkpoint 規則（每 5 個 batch，model 自我檢查）

每完成 **5 個 batch**（以 `workspace/emit/active_wf.json` 的 `batches_completed` 跨過 5 的倍數為觸發點；
1 batch = 一次 `Workflow` 跑完並通過驗證）做一次 checkpoint，model **自我檢查**下列五項：

1. build gate 綠（0 error；warning 僅既存且未增加）
2. `reviewed` / `emitted` 數增量與本段處理量一致（本段 = 5 batch ≈ 60 function）
3. `git log` 的 per-function commit 連續乾淨（一 function 一 commit，scope 只含該 function 的檔）
4. 無累積的 needs_user / interrupted
5. routing ↔ Ghidra 無 drift

**符合預期 → 印一行 checkpoint 摘要後「自動繼續、不要等使用者」。**
**只有遇到 §3.1 的「無法處理的未預期狀況」才停下等使用者確認。**

### 3.1 必須停下等使用者的情況

- build gate 紅燈（error），emitter 迭代到 MAX_ROUNDS(10) 仍修不好
- reviewer ↔ emitter 達 MAX_ROUNDS 仍 needs_user（無法 approve）
- routing ↔ Ghidra drift（function 不存在 / name 改變 / count 對不上）
- Ghidra 修改產生 error bookmark（`list_bookmarks(category="Bad Instruction")`）無法修復
- 出現現有 SOP / fall-through pattern 未涵蓋、需使用者決策的新狀況
- 同一現象連續多個 function 重複失敗（系統性問題）

### 3.2 不算「停」、自行處理續跑的情況

- **token / usage limit interrupt** → workflow return `stopped:"interrupt"`，已 commit 進度保留、重跑零成本；恢復走 §5 event-driven（完成通知或手動 /loop），目前無心跳自動恢復
- 單一 function reviewer block → emitter fix（正常迭代，不是異常）
---

## 4. 完整 fd2.exe 路線

**A.（本 workflow）**function emit+review 653 → **B. Phase 8 data emit**（~1300 data items 的真實 byte → C const / zero-init / vendor-link；建議沿用同架構 per-item workflow）→ **C. wlink 整合**（`src/*.obj` + Watcom CLIB3S + AIL lib → LE executable）→ **D. Layer 1 驗證**（DOSBox scripted playtest 對比原版 pixel / FD2.SAV / BGM）。

規格與細節：`rebuild_info/emission/`（pipeline_spec / calling_convention / pool_routing）。

---

## 5. /loop 全自動接續（event-driven 驅動模型）

**驅動**：loop 由 workflow 完成通知（`<task-notification>`）驅動 —— 一批完成→通知喚醒→驗證+下一批，鏈自我延續。**預設純 event-driven、不設 ScheduleWakeup 心跳**；僅在需要 usage-limit 自動恢復時才加 1h 心跳（上限 3600s）。in-flight 批次的 task_id 記於 `workspace/emit/active_wf.json`（scratch，loop 執行時才存在；內含自我描述的恢復說明）。唯一事實來源仍是 `routing.json` 的 reviewed 欄 + per-function commit，漏接通知零成本重來。

**任何喚醒（完成通知 / 手動 /loop）一律照下列判定**：

1. `active_wf.json` 有 task_id → `TaskOutput(task_id, block=false)`：`running` → 報告狀態後結束（不啟動新批、不 arm 心跳）；`completed`/查無 → 往下。
2. `next_batch.py --stats`：`await_review==0 && await_emit==0` → 全完成 → 刪 `active_wf.json` + PushNotification 通知使用者 + 結束 loop。
3. 否則跑下一批（= §2 每批流程）：先驗證上批（build gate 綠除既存 warning、reviewed 增、commit 乾淨、無 dosbox 孤兒）→ scout（review 做完改 `--mode emit`）→ `search_functions` 對齊 routing↔Ghidra（drift 拋警告）→ `git checkout -- src tests` → `Workflow(emit_review.wf.js, args)` → 新 task_id 覆寫 `active_wf.json`。
4. 每完成 5 個 batch（`batches_completed` 跨 5 倍數）做 §3 checkpoint；符合預期印一行摘要後自動續，只有 §3.1 才停。

**貼給新 session（zero-context 亦可接續）**：

> /loop 全自動接續 FD2 emit-review workflow 直到 routing.json 全部 reviewed + emitted。依 handoff §5 的 event-driven 驅動模型與喚醒判定執行；先讀 src/handoff.md + tools/emit/_index.md、確認工具（Ghidra MCP 已開 FD2.LE、DOSBox-X 在 PATH、Watcom 9.5a）可用，不可用就停下問我。全程 Opus、嚴守 §6 鐵則。

---

## 6. 嚴格規則（workflow 已結構性落實；人工介入時亦遵守）

- 一次一個 function（workflow 序列保證）；三源不省略（plate / disasm / decomp），即使極簡 thunk
- 語意完全保留；Ghidra 系統性 EAX-tracking bug（CALL 後 EAX return value 常被誤標）必對 assembly 核對
- cc / param 從 caller 推，少報比多報危險（少報 → 讀 stack 垃圾 → crash）
- 符號名與 Ghidra byte-identical；C89（變數宣告在 block 開頭）；檔名 8.3（Watcom 9.5a 無 LFN）
- 絕不半成品（改名 / static / 空殼 / `_impl`）；絕不為遷就 test 而扭曲 emit code
- emitter / reviewer **前景**跑 `build_test.py`，嚴禁 `run_in_background`（subagent 背景跑不閉環 + 留 dosbox 孤兒）
- 風險導向 test 覆蓋：數值 / 複雜分支 / RNG / EAX-bug 風險 / 狀態轉移強制測；純 blit/display 副作用延 Phase 8 integration
- Ghidra plate / name / data symbol 與 assembly 事實不符 → 當場 `set_plate_comment` / `rename` + 同步 KB / globals.h / testglob.c

---

## 7. build / test 知識

- build gate：`python tools/emit/build_test.py`（前景跑，內部輪詢 `tests/OUT/DONE.TXT`，回 JSON `{gate_pass, build_ok, errors, warnings, tests_passed, tests_failed}`）。
- **唯一完成訊號 = `DONE.TXT` 出現；無 stale-cache / DPMI-OOM 問題。** 不要加 copy→rename / sleep / 兩段式 session 等 workaround。
- C89：變數宣告在 block 開頭。8.3：檔名/目錄 ≤ 8.3。
- `testglob.c`：fake global / stub 集中；function pointer table 必須初始化指向 noop（否則 NULL call → DOS4GW crash）；emit 真實 function 後移除對應 stub（避免 linker redefinition）。
- 測試鏡像 src 子檔（`tests/<domain>/<stem>.c`，每檔 ≤1000 行）。新建任何 src 或測試 `.c` 檔後，跑一次 `python tests/genbuild.py --apply`，它掃 `src/` 與 `tests/` 自動產生 `build.bat`（src+test 編譯區）/ `test.lnk` / `testmain.c`，**嚴禁手改這三個檔**。編譯指令在 `build.bat`（由 `dosbox.conf` autoexec 呼叫，避開 autoexec 行數上限）。
