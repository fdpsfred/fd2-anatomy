# assets/text/

從 FDTXT.DAT 解出的純文字內容。

## 檔案

- `global_text.md` — entry 0 全遊戲共用文字庫（661 pages）：角色 / 職業 / 種族 / 道具 /
  法術名表、章號與場景副標、各章勝敗條件、商店與升級系統提示。各 page 的語意分區與其 src
  消費端見該檔的「Page 語意分區」表。
- `endgame_text.md` — FDTXT entries 31–33。entry 31 為結局 epilogue（46 pages）；
  entry 32（11 pages）與 entry 33（6 pages）其實是第 1 章 prologue 對話，播放順序
  entry 33 → entry 32 → entry 1，由 `fd2_chapter_01_init` 的三個 phase 載入。
- `glyph_table.md` — 1824 筆 glyph_id ↔ 中文字 lookup table（FDOTHER.DAT[4] 字模 atlas
  的索引對應）。

## 編碼說明

對話文字不是 Big5 / GB 編碼；每個 u16 是一個中文字模 sprite 的 glyph_id。編碼規則與字模
atlas 佈局詳 `resource_info/fdtxt.md` 與 `resource_info/chinese_glyph_encoding.md`。
