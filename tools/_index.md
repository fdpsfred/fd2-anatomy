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
- `lowconf_signature/` — LOW-confidence (caller signal 不確定，auto
  param-count classifier 跳過的 277 個 function) 後續清理：`inventory.py`
  重建逐筆 LOW set + per-function signal、`plan_apply.py` 規則化 plan
  generator (參考用，per-function disasm 驗證才是預設工作流)
