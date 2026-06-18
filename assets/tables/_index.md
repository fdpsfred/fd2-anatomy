# assets/tables/

binary 內主要 data table 的 struct layout 與資料來源。實際數值內容在
`assets/{characters,items,spells,enemies,jobs}.md`；本資料夾的檔案專注於
**struct 定義** + **位址資訊**。

## 檔案 (依資料表類型分類)

### 角色 / 敵人系

- `character_base.md` — `.object3 @ 0x61DA1`，32 entries × 24 B。RA / CL / LV /
  HP / MP / MV / MG[4] / IT[6] / AP / DP / DX 出場屬性。
- `character_growth.md` — `.object3 @ 0x620A1`，68 entries × 11 B。每職業
  AP/DP/DX/HP/MP min/max growth + spell_learning_idx。
- `enemy_data.md` — `.object3 @ 0x61AF9`，68 entries × 10 B。RA / CL / HP / MP /
  AP / DP / DX / MV / EX 敵人 unit。

### 道具 / 法術系

- `item_effect.md` — `.object3 @ 0x602AC`，215 entries × 23 B。TY / AP / HT /
  DP / EV / S1 / S2 / R1 / R2 / K1..6 / MM / trailing。
- `spell_effect.md` — `.object3 @ 0x619FD`，36 entries × 7 B。DA / HT / DS /
  RN / MP / WH。
- `spell_learning.md` — `.object3 @ 0x626B3`，20 entries × 12 B (= 6 ×
  spell_learn_pair)。配對 character_growth.spell_learning_idx 使用。
- `shop.md` — `.object3 @ 0x62390`，28 entries × 28 B。weapons[12] / items[8] /
  mystery[8] item ID slots。

### 職業屬性

- `job_crit.md` — `.object2 @ 0x5239B`，27 bytes。每 byte = job_id 對應的
  暴擊率 %。
- `job_magic_resist.md` — `.object2 @ 0x51F96`，27 dword。`抗性 = (10 - 數值) / 10`。

### 文字

- `glyph_id_to_character.csv` — glyph id ↔ 字元對照表（欄位 `glyph_id_hex,character`）。
  FDOTHER.DAT[4] 的 1824 個 16×16 glyph 各對應一個字元，FDTXT 對話文字 id 解碼用。
  由 `tools/glyph/` 渲染 + ET3 STDFONT pixel-match 產生。
