# 從零重現建置＋測試環境

把 `src/` 建成可在 DOS 執行的 `FD2.EXE`、再用 golden playthrough 驗證它與原版行為一致，端到端一頁。
每一步只說「做什麼」並指向對應的 `tools/*/_index.md`，實跑細節不在此複製。

## 前置條件

| 項目 | 說明 |
|---|---|
| Watcom C/C++ 9.5a | 安裝在 `C:\Users\fdpsf\Documents\WATCOM_9.5a`（`BIN` 在 PATH）。編譯器與原 binary 同版，是忠實重建的前提（見 `crt/fid_match.md`）。 |
| DOSBox-X | 執行檔在 PATH。建置 / 測試腳本一律以 `-silent` 全自動跑；wcc386 / wlink / wlib 都在 DOSBox-X 內執行。 |
| 遊戲檔 | `fd2_game_files/` 內的完整炎龍騎士團合輯版檔案（FDMUS / FDOTHER / FDFIELD / … + 原版 `~FD2.EXE`），實機 playtest 與原版差分都需要。 |
| Ghidra | 開著 FD2.LE 的 code browser，供建置 worklist 與符號 / 位址即時查證（唯讀）。 |

vendor lib 與 header 的落點（`libs/ailv3/`）由建置腳本掛成 DOSBox 磁碟；各腳本自行 mount Watcom 樹與
vendor 目錄，不需手動掛載。

## Step 1 — Watcom 掛載與 toolchain 陷阱

建置腳本在 DOSBox-X 內把 Watcom 9.5a 樹掛成磁碟後呼叫 wcc386 / wlink。重建任何 FD2 object 共通的
低階 Watcom / DOS / DOSBox-X 陷阱（C89、8.3 檔名、batch 行長截斷、DOS4GW 啟動、Ghidra label 重複）
見 `build_test/toolchain_quirks.md`；AIL 專屬的見 `ail/build_quirks.md`。

## Step 2 — 重生 vendor lib（ailv3.lib + fd2common.lib + ailv3.h）

Miles AIL audio library 從 FD2.LE 抽出、重建成 `ailv3.lib` + `ailv3.h`，放進 `libs/ailv3/` 供遊戲
建置連結。抽取同時產生副產品 `fd2common.lib`，但**不放進 `libs/ailv3/`、遊戲 FD2.EXE build 也不連
結它**——它只供 AIL self-test，其 8 個 bare-name symbol 已由 `src/util/dpmi.c` + `src/crt/crt.c`
提供。完整 pipeline（Ghidra dump -> OMF emit -> `wlib` 打包 -> header 生成 -> DOSBox 內 build/run
驗證）與跑法見 `tools/ail_extract/_index.md`。

`ailv3.h` 內每個 public AIL 宣告帶 clobber `#pragma aux ... "*" modify [eax ebx ecx edx]`，由
`src/include/protos.h` `#include`；缺這個 pragma 會使 SFX 全靜音（根因見 playtest_bugs A 類）。

## Step 3 — 建置 FD2.EXE

`build_fd2.py` 自掃 `src/*.c` 全部編譯 + 連結，零 `tests/` 依賴，輸出 src-only `FD2.EXE`；退出碼 0 表示
EXE 產出且 0 undefined。若有 undefined，`analyze_undefined.py` 把它分類成「`src/` 還缺什麼」的權威
worklist。跑法、`fd2.lnk` 連結設定、obj 命名規則見 `tools/fd2_build/_index.md`；`fd2.lnk` 每條
directive 與旗標政策（顯式列 CLIB3S / MATH387S / EMU387、`-s` 與 `__NO_MATH_OPS`）的唯一正典見
`link/wlink_settings.md`。

## Step 4 — golden playtest 驗證

`build_replay.py` 以 `build_fd2.py` 為範本、加 `-DFD2_REPLAY` 並連進 `tests/play/` guest harness，產出
決定論 replay 版 `FD2RP.EXE`（生產版 hook 全在 `#ifdef FD2_REPLAY` 內、byte-identical）。`run_all.py`
跑所有「有 golden」的 scenario，framebuffer 走 Hamming distance、state 走 byte-equality 逐一比對，帶
`oracle` 的 scenario 再跑 `expect.py` 用同一 LFSR + 公式核對戰鬥傷害數值。跑法、scenario 定義與各腳本
見 `tools/fd2_play/_index.md`。

四種驗證手段（此處的 golden playthrough，加上 eqcheck 功能等價 gate、FD2_REPLAY replay 建置、原版
runtime 差分）的 KB 級說明見 `verification.md`。

## 正典速查

| 要找 | 去 |
|---|---|
| `fd2.lnk` / wcc386 旗標 / `-s` / `__NO_MATH_OPS` | `link/wlink_settings.md` |
| LE layout / object 分布 / FD2.EXE offset 對照 | `link/le_layout.md` |
| 等價鐵則 / Layer 1/2/3 / fall-through 模式 | `equivalence/rules.md` |
| 四 pool 分類 / binary_artifact NOP | `equivalence/pool_classification.md` |
| Watcom 三種 calling convention / `__CHK` | `equivalence/watcom_abi.md` |
| 實機 playtest 解過的 bug（七類根因）| `build_test/playtest_bugs.md` |
| `src/` 模組 ↔ Ghidra 符號 ↔ program_info | `src_map.md` |
