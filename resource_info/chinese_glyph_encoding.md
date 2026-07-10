# 中文字元編碼 / 字型 (FDOTHER.DAT[4])

`data_fd2_chinese_font_sheet @ 0x53A75` 是由 FDOTHER.DAT[4] 載入的 1bpp 16×16 字模圖集，
共 1824 glyphs × 32 bytes = 58,368 bytes。FDTXT.DAT bytecode 用 u16 glyph_id
直接索引這張圖集。

## 編碼性質

**直接圖集索引**，不是 Big5 / GB 等標準中文編碼。每個 u16 glyph_id 是圖集
內 fixed-size sprite 的索引。這意味遊戲執行不需要任何中文編碼解碼器，純粹靠位置
查找。

觀察到的 glyph_id 範圍：`0x0000..0x071F` (1824 個字模格)。

> 版本差異:95 初版與 98 合輯版的字模圖集 是同一組 1824 字的重新編號(751 個字換了
> glyph_id),見 `version_diff.md`(含逐字對照全表)。

## 英數字區塊

圖集 **不是** ASCII 對齊。英數字模集中在低索引：glyph_id `0x00..0x09` = 數字
'0'..'9'、`0x0A..0x23` = 大寫 'A'..'Z'，`0x24` 起即為中文。因此不能拿 ASCII 碼直接
當 glyph_id（'A' 是 glyph 0x0A 而非 0x41；無小寫、無標點區塊）。

`NUMBER` opcode (FDTXT bytecode `0xFFFA`) 把數值交給 `sprintf @ 0x377D9`（格式字串
`"%d" @ 0x5014C`），再逐位 `digit_buf[i] - 0x30` 換成圖集索引 0..9，對應數字
'0'..'9' 字模（數字在低索引，**不**從 ASCII 0x30 起算）。

## 字模渲染

由 `fd2_blit_glyph_1bpp_with_outline @ 0x4EA2A` 渲染。函式名「1bpp」指 **input glyph**
是 1bpp (58368 ÷ 1824 ÷ 32 = 1.0)；輸出寫進 VGA mode-13h 8bpp framebuffer（每像素一個
palette-index byte，非 2bpp channel buffer）。

兩階段：
1. 可選背景色填 16×16 矩形
2. scan 16 rows × 16 bits；每 set bit 寫 `fill_color`，並在其正下方 (`+pitch`) 與
   左下 (`+pitch-1`) 各寫一個 `outline_color` 像素 (左下 L 形 drop-shadow)

## glyph_id ↔ 中文字 lookup

完整 1824 glyph 的中文字對應已建立。lookup table 來自倚天 (ET3) STDFONT.15 的
pixel-level match + 人工校正：

| 統計 | 值 |
|---|---|
| entry_count | 1824 (= 1824 FDOTHER[4] glyphs) |
| filled (有中文字) | 1824 |
| unfilled | 0 |
| blank (字模本身全空) | 0 |
| multi_char (一格 2+ 字；未解字模的 placeholder / 雜訊字串) | 43 |
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
