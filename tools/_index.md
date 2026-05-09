# tools/

可重複利用的 Python script。每個 script self-contained — 自帶路徑常數、不
import shared lib、不依賴 `legacy/`、不讀寫 `catalog/*.json`。

完整 script 用法 + 驗證指令見 `README.md`。

## 子資料夾

- `decoders/` — LLLLLL DAT archive parser + 各資源檔解碼器 (FDTXT / FDFIELD /
  FDSHAP / FDOTHER / DATO / FDMUS / FDICON / BG / TAI 等)
- `glyph/` — 中文字 glyph atlas 渲染 + ET3 STDFONT pixel-match lookup
- `glyph/ET3_fonts/` — ET3 字型檔 (ASCFONT.15, STDFONT.15)，glyph 比對工具的
  唯一外部依賴
- `calling_convention_audit/` — 把 FD2.LE 全 1000 個 function 的 cc 從 Ghidra
  自動推斷錯誤狀態校正成 ABI 正確的 pipeline (Python orchestrator + Ghidra
  Java workers)，跨 session 可恢復；尾端含 param-name cleanup 與 param-count
  apply 兩個 one-shot pass
- `function_review/` — 全 function 命名審視 + call graph 建立工作流的支援
  scripts。從 Ghidra MCP dump 建 per-function review registry / 進度面板 / DOT
  call graph snapshot + Phase E call_graph 凍結。經 1004 function 完整 review
  工作流跑過；命名與分類判斷由人親自做，scripts 只做機械處理
- `crt_fid_match/` — Ghidra Function ID 比對 pipeline 識別 FD2.LE 內
  Watcom CRT 函式。Python 端做 lib 拆解 / OMF 修補 / 跨版本 dedup /
  結果比對；Ghidra 端 Java scripts 在 `ghidra_scripts/` 下做 import /
  analyze / populate / query。完整說明見 `rebuild_info/crt_fid_match.md`
- `crt_callee_match/` — 比對 FidDB 已識別 CRT function 內呼叫到的「未識別 callee」
  與 Watcom CRT lib symbol。OMF parser（含 Watcom Easy OMF-386 quirks 處理）+
  size + caller-source-obj heuristic + byte-level FIXUPP-aware 比對。用於補抓
  CRT splitter 切錯的尾段碎片與 FidDB 漏抓的 small helper
- `jump_table_audit/` — 全 binary indirect-JMP / orphan code / fragmented body
  audit。Phase 2 跑 `compare_lookup_sizes.py` 對 lookup body_size vs lib `.obj`
  size 全面 diff（regression check）；Phase 1+3 用 Ghidra MCP run_script_inline
  一次性掃 + 修。對應 `rebuild_info/crt_fid_match.md` §12.2.1
