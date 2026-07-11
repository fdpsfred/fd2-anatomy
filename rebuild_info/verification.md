# 驗證手段

重建 `src/` 的正確性靠四種互補的驗證手段：兩種在 binary 層確認重建碼與 baseline 的關係，兩種在
runtime 層確認遊戲行為與原版一致。本檔是這四種手段的 KB 級說明；各段指向對應工具實跑。

## eqcheck — rename 功能等價 gate

**用途**：`src_refine` Stage 2 對 symbol 改名時，證明「只改了名字、沒改到 code / data / 邏輯」。
一般 `src/` 改動用 byte-identical gate（`hash_check.py`，重建 EXE 的 sha256 必等於 baseline）；但
symbol rename 會合法擾動 binary，byte-identical 會誤判，改用 eqcheck。

**機制**：rename 有兩個 binary 可見但行為中性的效果——(1) **fixup 重排**：wlink 依 symbol 名排序
emit LE fixup record，改名讓 Fixup Record Table 的 byte 重排（multiset 不變）；(2) **COMDEF 重定位**：
未初始化全域是 Watcom COMDEF tentative，wlink 依名排序擺放，改一個名可能讓它與鄰居位移幾個 byte，
連帶改變每個指向被移動 symbol 的 fixup site 值與對應 record 的 target 欄。兩者對載入後的映像都無影響。

**判定**：eqcheck 兩級通過即算功能等價——STRICT：Fixup Record Table 以外全部 byte-identical 且該表
byte multiset 相同（只發生效果 1，無 COMDEF 移動）；RELOC（STRICT 失敗時的 fallback）：把 candidate
與 baseline 的每個 fixup site 值與整個 Fixup Record Table 抹零後比殘差，殘差相同代表差異只有重定位值
＋fixup 重排（效果 1＋2）、零 code/data/邏輯改動。任何真的改到 code/data 都落在 fixup site 之外、會
破壞殘差而 FAIL。LE 表界與 fixup site 從 EXE header 即時 parse，不硬編。

**實跑**：`tools/src_refine/_index.md`（`eqcheck.py`；baseline 為 `data/baseline_eq.json`）。

## FD2_REPLAY — gated replay 建置

**用途**：把生產遊戲碼原封不動地放進一個可決定論驅動、可在檢查點擷取狀態的測試載體，供
playthrough golden 與傷害 oracle 使用。

**機制**：`build_replay.py` 以 `build_fd2.py` 為範本、ABI 旗標完全一致，唯一差別是加 `-DFD2_REPLAY`
並把 `tests/play/` 的 guest harness（`replay.c` 輸入注入 + `capture.c` 檢查點 dump）
一起編譯連結，輸出 `FD2RP.EXE`。所有 src/ 端 hook 都包在 `#ifdef FD2_REPLAY` 內（`life/main.c`、
`input/input.c`、`anim/aniend.c`、`save/save.c`），
生產版不定義此宏、hook 全不編入。

**判定**：生產 `FD2.EXE` 與未加 harness 的建置 byte-identical（已 hash 驗證），確保 replay 建置測到的
遊戲 codegen 與出貨版完全相同。

**實跑**：`tools/fd2_play/_index.md`（`build_replay.py`）、guest 端與 gated hook 清單見 `tests/play/_index.md`。

## playthrough golden — 決定論 playthrough 比對

**用途**：整合回歸。用腳本驅動 `FD2RP.EXE` 跑完一段遊玩流程，在邏輯檢查點擷取畫面與狀態，對凍結的
golden 逐一比對。

**機制**：`run_play.py` 把遊戲環境 + `SCRIPT.TXT` + 起始 `FD2.SAV` staging 好後於 DOSBox-X silent 啟動，
`replay.c` 把 scancode 注入 BIOS 鍵盤環驅動既有輸入路徑，`capture.c` 在檢查點 dump framebuffer
（`FBnn`）+ DAC 調色盤（`PALnn`）+ 關鍵全域狀態（`STnn`：16 個 int32 header + 每單位 0x50 byte
runtime_char）。決定論靠虛擬時鐘（replay 下 `BIOS_TICK_*` 改成每讀遞增的計數器）與「選單/對話確認鍵
看 ASCII」等機制。

**判定**：`compare.py` framebuffer 走 Hamming distance、state 走 byte-equality，比 `tests/play/golden/`；
帶 `oracle` 區塊的 scenario（如 `combat_attack`）再跑 `expect.py`，用與遊戲同一條 LFSR + 傷害公式從
擷取的攻防 runtime_char 預算傷害並斷言 hp 減少量。golden 保證擷取 byte 穩定、oracle 保證數值符合公式，
兩者皆過才 PASS。`run_all.py` 跑整個套件。

**實跑**：`tools/fd2_play/_index.md`。

## 原版 runtime 差分 — 以原版為正解回填期望值

**用途**：補足 golden「只是回歸 oracle（重建版自己跟自己比）」的缺口——用未修改的原版 `~FD2.EXE`
執行結果當正解，證明重建版忠實複刻原版行為。

**機制**：以 autotype 驅動原版在 DOSBox-X 內跑到同一檢查點，取 DOSBox-X 全記憶體 save-state 快照。
原版在 DOS/4GW 下分頁、重定位，無法用 link-time vaddr 直接定址；靠 DGROUP 開頭的一段唯讀 CONST
字串簽章在快照記憶體中定位 DGROUP 的本次物理基底（per-run 導出的 runtime 值，絕不硬編），再於
基底 + 各 scalar 全域的 link-time vaddr 讀值，輸出與 `capture.c` 的 `STnn` 完全相同的 16-int32 layout。

**判定**：原版擷取的狀態欄位對重建版 golden 逐欄比對；相符即證明該段流程的重建與原版行為一致（如
`ch01_intro` 的開場狀態 14 欄全符）。symbol vaddr 從 Ghidra `list_globals` 即時取，`capture.c` 的
header 欄位順序是契約。

**實跑**：`tools/fd2_diff/`（`extract_state.py`）。

## 四者如何互補

- `hash_check`（byte-identical）與 `eqcheck`（功能等價）在 binary 層把關：前者證明沒改到碼，後者容許
  rename 這種行為中性擾動。
- FD2_REPLAY 是讓生產碼可被決定論驅動 + 擷取的載體，本身不判對錯，但保證被測 codegen == 出貨版。
- playthrough golden 是主力回歸網，原版 runtime 差分則把 golden 的期望值背書到「原版正解」，兩者一起
  才能同時抓「重建版退化」與「golden 本身就抄錯」。
