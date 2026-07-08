# rebuild_info/

`src/` 是已可用 Watcom C/C++ 9.5a 編譯、通過遊玩測試的 FD2.LE 重建成品，是本專案的 ground
truth。本資料夾是維護這份重建碼的 lasting reference：把 FD2.LE 對映回 C source 需要的
toolchain、vendor lib、連結環境與 ABI 細節，維護 `src/` 時要守住的等價鐵則，以及實際把它
建出來、跑起來、抓 bug 的經驗。`program_info/` 回答「FD2 現在做什麼」，本資料夾回答「怎麼把
它重建成等價執行檔、以及重建時哪裡會踩雷」。

等價目標是 Layer 2（functionally-exact），不追求 Layer 3（byte-exact），詳見
`equivalence/rules.md`。

## 從哪裡開始

- 想從零重現建置＋測試環境：`quickstart.md`
- 想把某個 `src/` 模組對回 Ghidra 符號與對應 program_info 文件：`src_map.md`
- 想理解 eqcheck / FD2_REPLAY / playthrough golden / 原版差分四種驗證手段：`verification.md`

## 子資料夾

| Sub-folder | 內容 |
|---|---|
| `equivalence/` | 維護 `src/` 的等價鐵則與結構分類：三層等價 invariant、時序熱迴圈與 math intrinsic 例外、fall-through 六模式、絕對位址改 symbol、BSS scalar-as-array、四 pool 分類法、Watcom 32-bit ABI（三種 calling convention / `__CHK` / dispatch 簽名 / shared epilogue fragment）|
| `link/` | FD2.LE 的 LE binary 靜態 layout（3 object / DGROUP 內部排列 / fixup section 統計 / 入口流程）、從 binary 反推的 `wlink` 命令與旗標政策、Watcom Easy OMF-386 格式 quirks、DOS/4GW bind stub |
| `crt/` | Watcom 9.5a CRT 的命名約定、符號 inventory、`crt_equivalent_*` wrapper、Function ID lookup table 與版本判定 |
| `ail/` | Miles AIL audio library 在 FD2.LE 內的 inventory、抽 `.obj` 邊界、ABI 契約（clobber pragma）、OMF emit 規約、public API 語意 |
| `build_test/` | 把 `src/` 實際 compile + link 出 FD2.EXE、放進完整遊戲環境對照原版實機 playtest 的工作流程與解過的 bug |

各 sub-folder 內含自己的 `_index.md`。

## 頂層檔案

| 檔案 | 內容 |
|---|---|
| `quickstart.md` | 從零重現建置＋測試環境的端到端單頁：Watcom 掛載、vendor lib 重生、`build_fd2.py` 建置、golden playtest 驗證，各步指向對應 `tools/*/_index.md` |
| `src_map.md` | `src/` 15 個模組 ↔ Ghidra 命名體系（`fd2_` / `data_fd2_` / `crt_` / `AIL_`）↔ 對應 program_info 文件的三欄導航總覽，含 8.3 檔名與符號命名規則 |
| `verification.md` | eqcheck（rename 功能等價 gate）、FD2_REPLAY（gated replay 建置）、playthrough golden、原版 runtime 差分四種驗證手段的 KB 級說明，各段指向對應工具 |

## 實機 playtest 根因

`build_test/playtest_bugs.md` 是「Layer-2 功能等價 emit 在真實遊戲執行時悄悄偏離原版」這類 bug 的
正典，按七個根因類別 A–G 整理：A 跨 vendor 呼叫的暫存器 clobber、B Watcom BSS/COMDEF tentative
scalar 相鄰與順序、C 資料型別號性、D 熱迴圈 codegen 時序、E 硬編絕對位址、F math intrinsic 呼叫
形式、G stack-probe 分佈。每類含症狀、一句根因、教訓與排障經驗，機制細節指向對應正典。

## 對照組資料

CRT 版本判定用的 4 個版本 fidb（9.5a 正本 + 9.5/9.5b/9.5c 對照組）與 FidQuery 結果屬 pipeline
intermediate，不入 git，重生方式見 `tools/program_analysis/crt_fid_match/_index.md`。
