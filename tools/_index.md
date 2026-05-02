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
