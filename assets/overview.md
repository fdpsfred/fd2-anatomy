# 遊戲內容總覽

assets/ 收錄從 FD2.LE binary 與資源檔解析出的遊戲內容。內容分四類：

## 1. 通用內容索引

- `characters.md` — 32 個可加入角色名單與基礎屬性
- `items.md` — 215 個道具一覽與效果
- `spells.md` — 36 個法術屬性與每職業習得
- `enemies.md` — 68 個敵人 / 友軍 unit 屬性
- `jobs.md` — 27 個職業 ID + 魔抗 / 暴擊率

## 2. 章節 (`chapters/`)

每章一份檔案，含章名、加入角色、敵人配置、寶物、特殊機制與完整對話內容。
跨章機制 (Good/Bad ending fork、conditional recruit 矩陣、reward 鏈) 在
`chapters/_index.md`。

## 3. 文字 (`text/`)

從 FDTXT.DAT 解出的純文字內容：

- `global_text.md` — entry 0 全遊戲共用對話片段 (661 pages)
- `endgame_text.md` — entry 31..33 結局文字
- `glyph_table.md` — 1824 glyph_id ↔ 中文字 lookup table

## 4. 資料表 (`tables/`)

binary 內主要 `.object3` data table 的 struct layout 與資料來源說明：

- `character_base.md` — 32 角色出場屬性 (24 B/筆)
- `character_growth.md` — 68 升級屬性 (11 B/筆)
- `enemy_data.md` — 68 敵人 unit (10 B/筆)
- `item_effect.md` — 215 道具 (23 B/筆)
- `spell_effect.md` — 36 法術 (7 B/筆)
- `spell_learning.md` — 20 升級習得序列 (12 B/筆)
- `shop.md` — 28 商店組合 (28 B/筆)
- `job_crit.md` — 27 職業暴擊率 (1 B/筆)
- `job_magic_resist.md` — 27 職業魔法抗性 (4 B/筆)

## 與 program / resource 文件的關係

`assets/` 是「玩家視角」的遊戲內容；`program_info/` 是「程式視角」的執行邏輯；
`resource_info/` 是「檔案視角」的格式說明。
