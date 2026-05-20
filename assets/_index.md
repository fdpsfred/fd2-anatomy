# assets/

從程式和資源檔解析出的遊戲內容。

## 頂層檔案

- `overview.md` — assets/ 整體導覽
- `characters.md` — 32 角色名單 (id ↔ 中文名 + base/growth 屬性 + 法術習得)
- `items.md` — 215 道具完整清單 (0x00..0xD6)
- `spells.md` — 36 法術 (攻擊 / 劍技 / 恢復 / 輔助 / 召喚)
- `enemies.md` — 68 敵人 / 友軍 unit (`data_fd2_battle_enemy_data_table`)
- `jobs.md` — 27 職業 ID + 魔抗 / 暴擊率 + 轉職物品

## chapters/ 子資料夾

每章劇情、招募角色、敵人配置、寶物、特殊機制與完整對話內容。詳
`chapters/_index.md` (含 30 章對照總表 + Good/Bad ending fork + conditional
recruit 矩陣 + reward 表)。

## text/ 子資料夾

從 FDTXT.DAT 解出的純文字內容。

- `global_text.md` — entry 0 全遊戲共用對話 (661 pages)
- `endgame_text.md` — entry 31..33 結局文字 (epilogue + endgame extras)
- `glyph_table.md` — 1824 glyph_id ↔ 中文字 lookup table

## tables/ 子資料夾

binary `.object3` 主要 data table 的 struct layout 說明：

- `character_base.md` — 32 角色出場屬性 (24 B/筆)
- `character_growth.md` — 68 升級屬性 (11 B/筆)
- `enemy_data.md` — 68 敵人 unit (10 B/筆)
- `item_effect.md` — 215 道具 (23 B/筆)
- `spell_effect.md` — 36 法術 (7 B/筆)
- `spell_learning.md` — 20 升級習得序列 (12 B/筆)
- `chapter_intro_metadata.md` — 26 章 intro metadata (category + hotkey + 武 / 道 / 神秘三店 item IDs；31 B/筆)
- `job_crit.md` — 27 職業暴擊率 (1 B/筆)
- `job_magic_resist.md` — 27 職業魔法抗性 (4 B/筆)
