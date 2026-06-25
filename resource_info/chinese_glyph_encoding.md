# 中文字元編碼 / 字型 (FDOTHER.DAT[4])

`chinese_font_sheet @ 0x53A75` 是由 FDOTHER.DAT[4] 載入的 1bpp 16×16 字模 atlas，
共 1824 glyphs × 32 bytes = 58,368 bytes。FDTXT.DAT bytecode 用 u16 glyph_id
直接索引這張 atlas。

## 編碼性質

**Direct atlas index**，不是 Big5 / GB 等標準中文編碼。每個 u16 glyph_id 是 atlas
內 fixed-size sprite 的索引。這意味遊戲執行不需要任何中文編碼解碼器，純粹靠位置
查找。

觀察到的 glyph_id 範圍：`0x0000..0x071F` (1824 distinct atlas slots)。

## ASCII 範圍

ASCII (`0x20..0x7E`) 在 atlas 中對應 ASCII 字模，可直接用 ASCII 碼作為 glyph_id
渲染英文字母 / 數字 / 符號。

`NUMBER` opcode (FDTXT bytecode `0xFFFA`) 內部從 `0x5014C` 讀 sprintf 結果再
`digit_buf[i] - 0x30` 換成 0..9，證明 atlas 索引 0..9 對應數字 '0'..'9' 字模
(數字字模在低索引處，**不**從 ASCII 0x30 起算)。

## 字模渲染

由 `fd2_blit_glyph_1bpp_with_outline @ 0x4EA2A` 渲染。命名「2bpp_with_outline」指
output buffer 是 2bpp (fill + outline 兩 channel)，**input glyph 本身是 1bpp**
(58368 ÷ 1824 ÷ 32 = 1.0)。

兩階段：
1. 可選背景色填 16×16 矩形
2. scan 16 rows × 16 bits；每 set bit 寫 `fill_color`，並在右下角加
   `outline_color` 像素 (產生 1-pixel outline 立體效果)

## glyph_id ↔ 中文字 lookup

完整 1824 glyph 的中文字對應已建立。lookup table 來自倚天 (ET3) STDFONT.15 的
pixel-level match + 人工校正：

| 統計 | 值 |
|---|---|
| entry_count | 1824 (= 1824 FDOTHER[4] glyphs) |
| filled (有中文字) | 1824 |
| unfilled | 0 |
| blank (字模本身全空) | 0 |
| multi_char (一格 2+ 字，例 'Lv'/'HP') | 44 |
| user_edited (校正 ET3 自動 pre-fill) | 100 |
| distinct characters | 1824 |

## ET3 STDFONT.15 layout

ET3 STDFONT.15 的真實 layout 與標準 Big5 lead/tail 線性 formula 不同：
- idx 0 = `一` (Big5 0xA440)，**不是** `、` (0xA140) — 不含 symbols block 0xA1xx-0xA3xx
- Lead 0xC6 只有 63 entries (tail 0x40..0x7E)；0xA1..0xFE 是 HKSCS 擴充 (cp950 解但 ET3 沒)
- Lead 0xF9 完整 116 entries (tail 0x40..0x7E + 0xA1..0xD5)
- 41 個倚天 (ETEN) 擴充字在 idx 13053..13093 = cp950 0xF9D6..0xF9FE (標準 Big5 無此 codepoint，須用 cp950 解碼；例 0xF9D8 = 裏 = glyph_id 0x02FD)

## 完整 1824 字 lookup 表

存放在 `assets/text/glyph_table.md` (純資料，1824 行 markdown 表)。

## 工具

- 從 FDOTHER[4] 抽 1824 個 1bpp 16×16 glyphs：`tools/glyph/render_glyph_atlas.py`
- ET3 字型像素比對 + 產生 glyph_id ↔ Big5 候選：`tools/glyph/et3_pixel_match.py`
- ET3 字型檔本身：`tools/glyph/ET3_fonts/{ASCFONT.15, STDFONT.15}` (解 glyph 必備)
