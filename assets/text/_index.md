# assets/text/

從 FDTXT.DAT 解出的純文字內容。

## 檔案

- `global_text.md` — entry 0 全遊戲共用對話庫 (661 pages)。`SUB_DIALOG_A` /
  `SUB_DIALOG_B` opcode 從 chapter dialogs 遞迴到此；`fd2_play_final_chapter_30_ending`
  用 `char.identity+1` (角色名) 與 `char.bJob_id+0x96` (職業名) 動態 page 索引
  拿系統文字。
- `endgame_text.md` — entry 31..33 結局文字。entry 31 是 first epilogue dialogue
  (46 pages)；entry 32 (11 pages) 與 entry 33 (6 pages) 推測為 staff roll 與
  final ending screen。
- `glyph_table.md` — 1824 glyph_id ↔ 中文字 lookup table (FDOTHER.DAT[4]
  字模 atlas 的索引對應)。

## 編碼說明

對話文字非 Big5 / GB 編碼。每個 u16 是 `chinese_font_sheet @ 0x53A75` 內 16×16
1bpp 字模 sprite 的索引。詳 `resource_info/fdtxt.md` 與
`resource_info/chinese_glyph_encoding.md`。
