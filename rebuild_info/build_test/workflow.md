# 建置 / 連結 / 實機 playtest 工作流程

從 `src/`（emit 出來的 C source）實際 compile + link 出可在 DOS 跑的 FD2.EXE、再放進完整遊戲環境
對照原版 playtest 的整套流程與定位手段。本檔記錄**方法論與實測得到的結論**；各 script 的逐項用法
在對應的 `tools/*/_index.md`（見文末交叉引用），連結設定的完整內容與證據鏈在 `../link/wlink_settings.md`。

## 兩個建置目標

`src/` 同一份 source 要支援兩種連結：

| 目標 | 內容 | 入口 | 用途 |
|---|---|---|---|
| TEST build | `src/` + `tests/` | `tests/testmain.c` 的 `main` | 單元測試（table / audio / battle / save / ...） |
| FD2.EXE | 只有 `src/` + vendor lib | `src/life/main.c` 的 `main` | 真實遊戲執行檔，放進 `fd2_game_files/` 實機跑 |

### 雙 main 處置

遊戲入口 function 命名為 `main`（CRT cmain386 進入點契約要求；這是唯一豁免 `fd2_` 前綴的 game
function）。但 `tests/testmain.c` 也有一個 `main`。兩者衝突，處置方式：

- **TEST build**：對 `src/life/main.c` 加 `-Dmain=fd2_main`，把遊戲入口改名避開 testmain 的 main。
- **FD2.EXE build**：`build_fd2.py` 編 `life/main.c` **不帶** `-Dmain`，讓遊戲入口保持 `main`，
  和 CRT cstart 的 `__CMain → main` 契約對上。

## src-only FD2.EXE 建置與 undefined 神諭

`build_fd2.py` 自編全部 `src/*.c` + link，產出 `FD2.EXE`（零 `tests/` 依賴、不經 `build_test.py`），
補齊後即最終建置。

**通則：只要資料 / 函式還沒補齊，連結就會回報 undefined symbol —— 這份 undefined 清單就是「FD2.EXE
還缺哪些東西在 `src/`」的權威 worklist**（以 linker 符號引用為準，比 name-pattern grep 可靠）。
`analyze_undefined.py` 把 undefined 分類（vendor libc/math、`data_fd2_*`、`fd2_*`、`crt_*`/`AIL_*`、
plain-named）並帶「誰引用」脈絡。

連結設定的完整內容（`system dos4g` 不自動 pull Watcom CRT、須顯式 `library` 列出
CLIB3S / MATH387S / EMU387、Miles AIL lib 掛載、`fd2common.lib` 不參與連結、Layer-2 不下 FAR_DATA /
object layout directive）與 directive↔binary 證據鏈全在 `../link/wlink_settings.md`；工具操作與 build
輸出位置見 `tools/fd2_build/_index.md`。

## 重編 FD2.EXE 給實機測試

```bash
python tools/fd2_build/build_fd2.py              # 自編 src + link -> FD2.EXE
python tools/fd2_build/analyze_undefined.py      # （若有 undefined）分類
# 把產出的 FD2.EXE 複製到 fd2_game_files/FD2.EXE，使用者啟動 DOSBox 實機跑
```

`build_fd2.py` 編 `life/main.c` 不帶 `-Dmain`（FD2.EXE 用真正的 `main` 入口），完全自包含、零 `tests/`
依賴、不碰 `tests/OUT`。build 輸出位置見 `tools/fd2_build/_index.md`。

## host WDISASM 反組譯比對（找 codegen / layout bug 的主力，免 DOSBox）

定位「rebuild 與原版行為分歧」的主力手段：用 **`WATCOM_9.5a\BINNT\WDISASM.EXE` 直接在 Windows 跑**，
反組譯 `tests/OUT/obj/*.obj`，對照 Ghidra MCP `disassemble_function` 的原版反組譯，逐指令比對。
這就是「rebuild vs 原版」的差異定位法，不需要 DOSBox。

操作注意：

- 先把 `.obj` 複製到**不含 `-` 的暫存目錄**再跑，並用相對檔名 —— repo 路徑含 `-`（`fd2-anatomy`）會
  被 WDISASM 當 option 解析。
- 對照時鎖定可疑的 codegen / 號性 / 指令數差異。

實際戰績（見 `playtest_bugs.md`）：

- **音效全靜音**：wdis 看到 sample offset 被留在 EBX 跨 `AIL_init_sample`，原版 spill 到 stack。
- **開場 scene 跳底**：wdis 看到 cursor 座標比較編成無號 JAE/JB，原版有號 JGE/JLE。
- **商店腳步聲被切**：wdis 比對 pose 縮放熱迴圈的除法指令數（`>>7` 2 指令 vs `/128` 6 指令）。

`build_fd2.py --map` 是 layout 面的對應工具：build FD2.EXE 時在 link directive 加 `option map`，看 BSS /
COMDEF symbol 的實際擺放順序（炙焰刀 array 反序、union REGS 相鄰就是靠它驗證的）。

## DOSBox-X fault logging（hang 其實是 fault）

`build_test.py` 的 `gen_run_conf()` 會注入 `[log] logfile=.../dosbox.log`，並把 DOSBox stdout/stderr
導到 `tests/OUT/dosbox_stdio.log`；run 後掃這兩個檔的 protected-mode fault（`illegal descriptor` /
GP / invalid opcode ...）放進結果 JSON 的 `dosbox_fault` 欄。用途：cinematic 測試驅動 real composite
讀到 garbage sprite → wild access → DOSBox-X 彈「illegal descriptor」modal → 卡住被判 hang；
`dosbox_fault` 揭露「hang 其實是 fault」。`-silent` 不會抑制 `[log]` 檔。

## 結束偵測（無固定等待）

三個訊號擇一：`DONE.TXT` 出現（正常完成）／ DOSBox process 退出（涵蓋正常完成與會交回 batch 的 crash，
如 DOS/4GW GP fault）／ heartbeat 停滯（`tests/OUT/HB.TXT` 每個 test 重寫，停滯超過門檻且 process
存活 → hang，`hung_test` 指出卡住的 test）。heartbeat 用 fopen/fprintf/**fclose** 寫 HB.TXT —— close
才讓 DOSBox 把重導向的 stdout commit 到 host（`fflush` 不夠）。無 stale-cache / DPMI-OOM 問題，不要加
copy→rename / sleep / 兩段式 session 等 workaround。

## 真實檔案測試（讀檔 function 鐵則）

`build_test.py` 啟動前把 11 個遊戲檔（FDICON.B24 / FDFIELD / FDSHAP / FDOTHER / FDTXT / FDMUS /
DATO.DAT / FD2.SAV / FIGANI.DAT / BG.DAT / TAI.DAT）從 `fd2_game_files/` stage 到 `tests/OUT`（= cwd，缺或 size 不符才複製）。會
`fopen` 真檔的 function，測試必須讀這些 staged 真檔並對真實解析值斷言 —— 禁止捏造假檔 stand-in、
禁止 `remove()` staged 真檔。reviewer checklist 強制此 gate。

## 診斷工具

實機 playtest debug 期間累積的診斷腳本（操作細節見對應的 `tools/*/_index.md`）：

| 工具 | 用途 |
|---|---|
| `build_fd2.py --map`（`tools/fd2_build/`） | build FD2.EXE 時加 `option map`，看 BSS / COMDEF symbol 實際擺放（layout bug 驗證主力，見 B 類 bug） |
| `lib_probe.py`（`tools/snd_kbd_diag/`） | dump `fd2common.lib` / `ailv3.lib` 的 module + symbol（host 版 WLIB / WDISASM 最快） |
| `sfxdiag.c` + `run_sfxdiag.py`（`tools/snd_kbd_diag/`） | 用 FD2 編譯參數重現 AIL init（`install_DIG_INI`）+ 依序播多個 FDOTHER SFX（可聽測試、AIL 回歸測試） |
| `run_fd2_audbg.py`（`tools/snd_kbd_diag/`） | 把臨時 instrument 的 FD2.EXE headless 跑、讀 main 寫的 audio 狀態 log、還原原檔（取 runtime 數值） |
| `gen_stkdiag.py`（`tools/stkdiag/`） | 重現 stack-probe 誤判、對齊原版 `__CHK` 分佈（見 G 類 bug） |

## 交叉引用

- 實機解過的 bug 與根因類別：`playtest_bugs.md`
- wlink 連結設定與證據鏈：`../link/wlink_settings.md`
- LE binary layout / 入口流程：`../link/le_layout.md`
- AIL clobber pragma / handle 型別：`../ail/calling_convention.md`
- 通用 Watcom / DOS / DOSBox toolchain 陷阱（C89 / LFN / rename / batch / DOS4GW / Ghidra label 重複）：`toolchain_quirks.md`
- AIL audio driver 專屬陷阱（BLASTER IRQ / AIL_DEBUG / wlib OMF record）：`../ail/build_quirks.md`
- 等價鐵則（資料相鄰性 / Layer-2 時序 / E-3b 硬編位址）：`../equivalence/rules.md`
- src-only 神諭與最終建置操作：`tools/fd2_build/_index.md`
- 診斷工具操作細節：`tools/snd_kbd_diag/_index.md`、`tools/stkdiag/_index.md`
- build_test gate 操作：`tools/code_emit/_index.md`
