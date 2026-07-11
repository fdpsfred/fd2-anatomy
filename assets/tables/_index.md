# assets/tables/

binary 內主要 data table 的橋接層：把 **binary 位址** ↔ **`src/include/types.h` 的
struct 欄名** ↔ **攻略本縮寫** 對齊起來。本資料夾只放 **struct 定義、位址、邊界、
存取端語意**；每張表的 readable 數值內容（角色 / 敵人 / 道具 / 法術 / 職業）唯一正典在
`assets/{characters,enemies,items,spells,jobs}.md`，本資料夾一律單向引用之。

## 位址空間換算

每張表的表頭位址一律是 **Ghidra VA**（Ghidra code browser 顯示、`list_globals` 回報的
位址，本專案的位址正典），同時就是**執行期 linear 虛擬位址**：LE loader 把三個 object
載到 linear `0x10000` / `0x50000` / `0x60000`，正是 Ghidra 的 object 基底，故 runtime VA
就等於 Ghidra VA（delta 0）。其餘三個檔案位址空間由固定的 per-segment delta 換算，不必逐檔重列：

| 位址空間 | `.object2` 小表（位於 `0x5xxxx`） | `.object3` 大表（位於 `0x6xxxx`） |
|---|---|---|
| Ghidra VA（正典 = 執行期 VA） | 表頭值 | 表頭值 |
| 獨立 `FD2.LE` 檔案 offset | Ghidra VA − `0x2AB8` | Ghidra VA − `0xEAB8` |
| `fd2_game_files/FD2.EXE` 檔案 offset | Ghidra VA − `0x200` | Ghidra VA − `0xC200` |
| 另一發行版 FD2.EXE 檔案 offset（跨版本，未複核） | Ghidra VA + `0x25014` | Ghidra VA + `0x19014` |

各欄意義：**獨立 `FD2.LE`** 是 Ghidra 載入的 LE 模組檔（data pages 起於 file `0xE548`）；
**`fd2_game_files/FD2.EXE`** 是出貨的 MZ-bind 版，FD2.LE 模組嵌在 file `0x28B8`，故其
檔案 offset = 獨立 FD2.LE offset ＋ `0x28B8`（即 `−0x200` / `−0xC200` 欄）。最後一欄指向
**另一個 FD2 發行版**的 FD2.EXE 檔案 offset，未在本專案獨立複核，僅供跨版本定位（詳
`rebuild_info/link/le_layout.md`）。

換算範例（`.object3`）：item_effect Ghidra VA `0x602AC` → 獨立 FD2.LE `0x517F4`
→ `fd2_game_files/FD2.EXE` `0x540AC`（跨版本另一發行版 `0x792C0`）。換算範例（`.object2`）：
job_crit Ghidra VA `0x5239B` → 獨立 FD2.LE `0x4F8E3` → `fd2_game_files/FD2.EXE` `0x5219B`
（跨版本 `0x773AF`）。

一張表屬 `.object2` 還是 `.object3` 由它落在哪個 segment 決定（看 Ghidra VA 的高位）：
魔抗 / 暴擊 / 章節分類等小表在 `0x5xxxx`（`.object2`）；道具 / 敵人 / 角色 / 法術 / 章節
intro 等大表在 `0x6xxxx`（`.object3`）。

## 攻略縮寫 ↔ types.h 正式欄名

各表沿用漢堂攻略本的兩字母欄碼，權威欄名以 `src/include/types.h` 的 struct 定義為準。
對照如下（同一縮寫在不同表可能指不同欄，故依表分組）：

| 表 | 縮寫 → types.h 欄名 |
|---|---|
| character_base | RA→race_id, CL→class_id, LV→level, HP→hp, MP→mp, MV→mv, MG→initial_spells[4], IT→initial_items[6], AP→ap, DP→dp, DX→dx |
| character_growth | 直接用 types.h 欄名 ap_min / ap_max / dp_min / dp_max / dx_min / dx_max / hp_min / hp_max / mp_min / mp_max / spell_learning_idx |
| enemy_data | RA→race_id, CL→class_id, HP→hp, MP→mp, AP→ap, DP→dp, DX→dx, MV→mv, EX→exp_reward |
| item_effect | TY→type, AP→ap, HT→ht, DP→dp, EV→ev, S1→special_type, S2→special_chance, R1→range_min, R2→range_max, K1→use_effect, K2→use_param_lo, K3→use_param_hi, K4→cast_range_flags, K5→target_side, K6→area, MM→price |
| spell_effect | DA→damage, HT→hit_rate, DS→cast_range_flags, RN→area, MP→mp_cost, WH→target_side |

## 檔案清單

### 角色 / 敵人系
- `character_base.md` — `data_fd2_battle_character_base_table` `.object3 @ 0x61DA1`，
  32 entries × 24 B。出場 race / class / level / HP / MP / MV / 初始法術 / 初始裝備 / AP / DP / DX。
- `character_growth.md` — `data_fd2_battle_character_growth_table` `.object3 @ 0x620A1`，
  68 entries × 11 B。每職業 AP / DP / DX / HP / MP 的 min 與 exclusive 上界 + spell_learning_idx；
  含升級 roll 組語+C 證據（每級最大 = `_max − 1`）與「升級最大值修改版」exe 的 +1/級 off-by-one。
- `enemy_data.md` — `data_fd2_battle_enemy_data_table` `.object3 @ 0x61AF9`，
  68 entries × 10 B。敵 / 友軍 NPC 單位的每等級係數 + 移動力 + 擊殺 XP 係數。

### 道具 / 法術系
- `item_effect.md` — `data_fd2_battle_item_effect_table` `.object3 @ 0x602AC`，
  215 entries × 23 B。武器 / 防具 / 消耗品的加值、特效、使用效果、價格。
- `spell_effect.md` — `data_fd2_battle_spell_effect_table` `.object3 @ 0x619FD`，
  36 entries × 7 B。法術傷害 / 命中 / 施放距離 / 範圍 / MP / 對象。
- `spell_learning.md` — `data_fd2_battle_spell_learning_table` `.object3 @ 0x626B3`，
  20 entries × 12 B。配對 character_growth.spell_learning_idx 使用的升級習得表。

### 職業屬性
- `job_crit.md` — `data_fd2_battle_job_crit_rate_table` `.object2 @ 0x5239B`，byte[28]，
  服務 class_id 0x01..0x1C（`table[job_id − 1]`，與魔抗表平行）。每 byte = job 的暴擊率 %。
- `job_magic_resist.md` — `data_fd2_battle_job_magic_resist_table` `.object2 @ 0x51F96`，
  dword[28]，服務 class_id 0x01..0x1C（`table[job_id − 1]`）。每 job 的魔法傷害縮放係數。
- `job_allowed_items.md` — `data_fd2_battle_job_allowed_items_table` `.object3 @ 0x6188A`，
  byte[203]（29 × 7）。每職業可裝備的 item type 白名單。
- `movement_cost.md` — `data_fd2_battle_movement_cost_table` `.object3 @ 0x61646`，
  byte[580]（29 × 20）。每職業對 20 種 tile 屬性的移動花費。
- `class_promotion.md` — `data_fd2_battle_class_promotion_data_table` `.object3 @ 0x615FE`，
  72 bytes（36 × 2；Ghidra 為 scalar `byte` label，未套 array 型別）。轉職後 job_id + 升職移動力加成。

### 章節資料
- `chapter_intro_metadata.md` — `data_fd2_chapter_intro_metadata_table` `.object3 @ 0x6238D`，
  26 entries × 31 B。story 章節 intro 畫面外觀變體碼 / hotkey + 內嵌的三家商店物品清單。
- `chapter_category.md` — `data_fd2_chapter_per_chapter_category_table` `.object2 @ 0x526B9`，
  byte[30]。以 chapter_id 索引的 story / battle 分派旗標。

### 文字
- `glyph_id_to_character.csv` — glyph id ↔ 字元對照表（欄位 `glyph_id_hex,character`）。
  FDOTHER.DAT[4] 的 1824 個 16×16 glyph 各對應一個字元，FDTXT 對話文字解碼用。
  由 `tools/glyph/` 渲染 + ET3 STDFONT pixel-match 產生。

商店資料沒有獨立的表：每章的三家商店物品清單直接內嵌在 `chapter_intro_metadata` 的
31-byte entry 內（+3 武器 / +15 道具 / +23 神秘），詳 `chapter_intro_metadata.md`。
