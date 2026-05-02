# tools/glyph/

中文字 glyph atlas 渲染與 ET3 STDFONT pixel-match lookup 工具。

## 檔案

- `render_glyph_atlas.py` — 從 FDOTHER.DAT[4] 抽 1824 個 1bpp 16×16 glyphs，
  輸出為 PNG atlas + index CSV。
- `et3_pixel_match.py` — 對渲染出的 glyph atlas 與 `ET3_fonts/STDFONT.15`
  做像素級 pixel-match (Hamming distance)，產生 glyph_id ↔ Big5 候選字 CSV。

## 子資料夾

- `ET3_fonts/` — ET3 (倚天) 字型檔，glyph 比對工具的依賴：
  - `ASCFONT.15` — ASCII 字模
  - `STDFONT.15` — 標準中文字模 (Big5 + 倚天擴充)

完整 CLI 用法與驗證範例見 `tools/README.md`。

## 使用流程

1. 跑 `render_glyph_atlas.py` 從 FDOTHER.DAT[4] 產生 1824 個 PNG glyph
2. 跑 `et3_pixel_match.py` 比對 ET3 STDFONT.15 字型，得到自動 lookup
3. 人工 review (對 Hamming != 0 的 glyph 校正)，產生最終 `glyph_table.json`
4. 將 lookup 結果以 markdown 格式置入 `assets/text/glyph_table.md`
