# tools/

可重複利用的 Python script 與 Workflow。每個 script self-contained — 自帶路徑常數、不
import shared lib、不依賴 `legacy/`。CLI 用法看 `python <script> --help`。

## 子資料夾

| Subfolder | 內容 |
|---|---|
| `decoders/` | LLLLLL DAT archive parser + 各資源檔解碼器（FDTXT / FDFIELD / FDSHAP / DATO / FDMUS / BG / FDICON / RLE）|
| `glyph/` | 中文字 glyph atlas 渲染 + ET3 STDFONT pixel-match，產 glyph id ↔ Big5 對照 |
| `program_analysis/` | FD2.LE 結構性分析：CRT / 函式 / 資料 / jump-table audit pipeline、call graph builder、真遊戲檔 ground-truth dump |
| `ail_extract/` | 從 FD2.LE 抽 Miles AIL 重建為 `ailv3.lib` + `ailv3.h` + `fd2common.lib`，DOSBox-X 內 build / run 驗證 |
| `emit/` | FD2 function emit + review pipeline：`emit_review.wf.js` 編排、`build_test.py` build gate、routing scout |
| `data_emit/` | 真實 global data 從 FD2.LE 落地 `src/` 的 emit pipeline + `verify_real.py` byte-equality gate |
| `fd2_build/` | src-only FD2.EXE 連結（`mklnk` + `link_oracle` + `analyze_undefined`；undefined 即「`src/` 還缺什麼」神諭）|
| `snd_kbd_diag/` | 實機 playtest 診斷（wlink map / lib dump / SFX 重現 / runtime audio 狀態）|

## 資料儲放慣例

| 位置 | 用途 |
|---|---|
| `tools/{tool}/data/` | 無法靠 script 重產的 primary input（外部 snapshot、人工 authored verdict）|
| `workspace/{tool}/` | script 一切 intermediate / output / audit state 的目的地（scratch）|

KB（`rebuild_info` / `program_info` / `resource_info` / `assets`）不引用 `workspace/` path。要 fresh
資料時直接重跑 script（如 `build_call_graph.py`），不留 KB snapshot；少數混多來源的結果
（如 `build_final_lookup.py`）寫 workspace 後人工複製進 KB。
