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
| `code_emit/` | FD2 function emit + review pipeline：`emit_review.wf.js` 編排、`build_test.py` build gate、routing scout |
| `data_emit/` | 真實 global data 從 FD2.LE 落地 `src/` 的 emit pipeline + `verify_real.py` byte-equality gate |
| `src_refine/` | `src/` 逐 symbol 解析 + refine（只改名稱/註解）+ 同步 Ghidra：worklist/partition/scout + `src_refine.wf.js` Stage 1 workflow + `hash_check` byte-identical gate + `merge_shards` 產 src_info/src_issues |
| `fd2_build/` | src-only FD2.EXE 正式建置（`build_fd2` 自編 src + link，零 `tests/` 依賴；`analyze_undefined` 把 undefined 分類成「`src/` 還缺什麼」worklist）|
| `snd_kbd_diag/` | 實機 playtest 診斷（wlink map / lib dump / SFX 重現 / runtime audio 狀態）|
| `stkdiag/` | `__CHK` 堆疊探測診斷：產生取代 CLIB3S(stk) 的 STKDIAG.OBJ，Stack Overflow 時先印觸發函數位址與 ESP |
| `fd2_play/` | 決定論 playthrough 整合測試工具：`build_replay.py` 編 FD2RP.EXE（src + `-DFD2_REPLAY`）、`run_play`/`run_all` 跑 scenario、`compare`/`expect` 比對 framebuffer+state golden、`gen_scenario`/`sweep_chapters`/`st_dump`/`fb2png` 輔助（詳見 `fd2_play/_index.md`）|
| `kb_overhaul/` | KB 翻新用產生器：`gen_ch_encounters` / `gen_ch_section3` / `gen_ch_shops` 由 byte-verified FDFIELD 表產各章 §敵人·寶物·商店、`rebase_row_addr` 跨版本資料表位址 rebase |
| `growth_table/` | 由 FD2.LE 角色基礎／成長／轉職表推導每級 HP/MP/AP/DP/DX，`gen_growth`＋`build_page` 產自足互動網頁「角色屬性數值比較」（三分頁：屬性排名／成長曲線比較／角色明細），發佈到 GitHub Pages（`docs/`）；詳 `growth_table/_index.md` |
| `tests_rename/` | `tests/` 舊 symbol 名批次同步：`sync_rename.py` 依 `rename_old2new.json` 改名、`verify.py` 驗證 |
| `rsrc_unresolved/` | 資源檔格式 ground-truth 分析：`analyze.py` 對真遊戲檔驗證 FDOTHER nested/dead 內容分類、ANI 檔頭與 per-frame 欄位、FDSHAP tile-attribute 值域、TAI round-trip（結論已整合進 `resource_info/`）|

## 資料儲放慣例

| 位置 | 用途 |
|---|---|
| `tools/{tool}/data/` | 無法靠 script 重產的 primary input（外部 snapshot、人工 authored verdict）|
| `workspace/{tool}/` | script 一切 intermediate / output / audit state 的目的地（scratch）|

KB（`rebuild_info` / `program_info` / `resource_info` / `assets`）不引用 `workspace/` path。要 fresh
資料時直接重跑 script（如 `build_call_graph.py`），不留 KB snapshot；少數混多來源的結果
（如 `build_final_lookup.py`）寫 workspace 後人工複製進 KB。
