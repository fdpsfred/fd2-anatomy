# 建置 / 連結 / 實機 playtest 工作流程

從 `src/`（emit 出來的 C source）實際 compile + link 出可在 DOS 跑的 FD2.EXE、再放進完整遊戲環境
對照原版 playtest 的整套流程與定位手段。本文件記錄**方法論與實測得到的結論**；各 script 的逐項用法
在對應的 `tools/*/_index.md`（見文末交叉引用）。

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
- **FD2.EXE build**：`link_oracle.py` 不帶 `-Dmain` 重新編 `lifemain.obj`，讓遊戲入口保持 `main`，
  和 CRT cstart 的 `__CMain → main` 契約對上。

## src-only fd2.lnk 神諭

在資料 / 函式還沒補齊的階段，src-only 連結會 link 不過，而它回報的 **undefined symbol 就是「FD2.EXE
還缺哪些東西在 `src/`」的權威 worklist** —— 以 linker 的符號引用為準，比 name-pattern grep 可靠。補齊後
同一套工具就是最終建置。`mklnk.py` 產 `tests/fd2.lnk`、`link_oracle.py` 在 DOSBox 跑 `wlink`、
`analyze_undefined.py` 把 undefined 分類（vendor libc/math、`data_fd2_*`、`fd2_*`、`crt_*`/`AIL_*`、
plain-named）並帶「誰引用」脈絡。操作見 `tools/fd2_build/_index.md`。

## 實際連結設定（已驗證 0 undefined）

`fd2.lnk` 的實際內容（由 `mklnk.py` 產生，Layer-2、讓 linker 自由擺放，不下 FAR_DATA / object layout
directive）：

```
system dos4g
name E:\out\FD2.EXE
file E:\out\obj\lifemain.obj      # 含 main，擺第一 -> 決定模組內部名
file E:\out\obj\<...>.obj         # 其餘全部 src .obj
library E:\out\ailv3.lib          # Miles AIL（stage 進 E:\out）
library E:\out\fd2common.lib      # FD2 自寫的 AIL-support helper
library D:\LIB386\DOS\CLIB3S.LIB  # Watcom 9.5a C runtime（stack-call，配 -3s）
library D:\LIB386\MATH387S.LIB    # 387 數學
library D:\LIB386\DOS\EMU387.LIB  # 387 emulator
```

### 關鍵實測：`system dos4g` 不自動 pull Watcom CRT

`link/wlink_settings.md` 早期從 binary 反推時推測「Watcom CRT 由 `system dos4g` 自動 pull、不用顯式
列」。**實際連結 src-only FD2.EXE 證明這個推測錯了**：當遊戲只透過 `crt_*` wrapper、不直接呼叫 libc
時，`.obj` 內的 default-library 記錄在沒有顯式 libpath 的情況下不會被解析，clib3s / math387s / emu387
的符號全部 undefined。Miles AIL lib 又會引用真 libc（`strcpy` / `memset` / `sprintf` / `malloc` ...）
與 math387s/emu387 的 `__8087` / `__hook387` 等。所以 `fd2.lnk` 必須**顯式 `library` 列出
CLIB3S / MATH387S / EMU387**（全路徑指向 DOSBox 掛載的 Watcom 樹 D:）才能連結到 0 undefined。
Layer-2 src-only 建置只需這三個 CRT lib 即達 0 undefined；原版 binary 分析提到的 GRAPH.LIB 在此建置
不需要（emit 出來的 source 沒有未解析的 GRAPH 符號）。詳見 `../link/wlink_settings.md`。

### AIL lib 落點

`ailv3.lib` / `fd2common.lib` 由 `link_oracle.py` 每次 stage 進 `E:\out`（`build_test.py` 會清
`tests/OUT`，所以每跑一次 oracle 前都要 re-stage）。CRT lib 直接從掛載的 Watcom 樹（D:）解析。

## 重編 FD2.EXE 給實機測試

```bash
python tools/emit/build_test.py                 # 編 src .obj 到 tests/OUT/obj
python tools/fd2_build/mklnk.py --apply          # 產 tests/fd2.lnk
python tools/fd2_build/link_oracle.py            # 在 DOSBox 跑 wlink，確認 0 undefined
python tools/fd2_build/analyze_undefined.py      # （若有 undefined）分類
# 把 tests/OUT/FD2.EXE 複製到 fd2_game_files/FD2.EXE，使用者啟動 DOSBox 實機跑
```

`link_oracle.py` 會不帶 `-Dmain` 重編 lifemain（給 FD2.EXE 用真正的 `main` 入口）。

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

`genmap.py` 是 layout 面的對應工具：重連 FD2.EXE + 產 wlink map，看 BSS / COMDEF symbol 的實際擺放
順序（炙焰刀 array 反序、union REGS 相鄰就是靠它驗證的）。

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

`build_test.py` 啟動前把 8 個遊戲檔（FDICON.B24 / FDFIELD / FDSHAP / FDOTHER / FDTXT / FDMUS /
DATO.DAT / FD2.SAV）從 `fd2_game_files/` stage 到 `tests/OUT`（= cwd，缺或 size 不符才複製）。會
`fopen` 真檔的 function，測試必須讀這些 staged 真檔並對真實解析值斷言 —— 禁止捏造假檔 stand-in、
禁止 `remove()` staged 真檔。reviewer checklist 強制此 gate。

## 診斷工具（`tools/snd_kbd_diag/`）

實機 playtest debug 期間累積的診斷腳本（操作細節見 `tools/snd_kbd_diag/_index.md`）：

| 工具 | 用途 |
|---|---|
| `genmap.py` | 重連 FD2.EXE + 產 wlink map，看 BSS / COMDEF symbol 實際擺放（layout bug 驗證主力，見 B 類 bug） |
| `lib_probe.py` | dump `fd2common.lib` / `ailv3.lib` 的 module + symbol（host 版 WLIB / WDISASM 最快） |
| `sfxdiag.c` + `run_sfxdiag.py` | 用 FD2 編譯參數重現 AIL init（`install_DIG_INI`）+ 依序播多個 FDOTHER SFX（可聽測試、AIL 回歸測試） |
| `run_fd2_audbg.py` | 把臨時 instrument 的 FD2.EXE headless 跑、讀 main 寫的 audio 狀態 log、還原原檔（取 runtime 數值） |

## 交叉引用

- 實機解過的 bug 與根因類別：`playtest_bugs.md`
- wlink 連結設定與證據鏈：`../link/wlink_settings.md`
- LE binary layout / 入口流程：`../link/le_layout.md`
- AIL clobber pragma / handle 型別：`../ail/calling_convention.md`
- 通用 Watcom / DOS / DOSBox toolchain 陷阱（C89 / LFN / rename / batch / DOS4GW / Ghidra label 重複）：`toolchain_quirks.md`
- AIL audio driver 專屬陷阱（BLASTER IRQ / AIL_DEBUG / wlib OMF record）：`../ail/build_quirks.md`
- emit pipeline 規則（E-8 array / Layer-2 時序 / 硬編位址）：`../emission/pipeline_spec.md`
- src-only 神諭與最終建置操作：`tools/fd2_build/_index.md`
- 診斷工具操作細節：`tools/snd_kbd_diag/_index.md`
- build_test gate 操作：`tools/emit/_index.md`
