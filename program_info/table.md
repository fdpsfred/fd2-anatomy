# 資料表模組 (table)

`src/table/` 收錄整個遊戲編譯進 FD2.LE 的唯讀資料表（外加少數 BSS 暫存陣列），以及一組
存取這些表的 leaf accessor 函數。這些表在 binary 內就是 `assets/tables/*.md` 與 `assets/*.md`
以「欄位值」層級所描述內容的原始位元組；本檔負責「模組層」索引：哪張表放在哪個 `.c`、位在哪個
Ghidra 位址、由誰消費，欄位語意細節一律引到對應的 assets 文件。

## 驗證對象

- 主要 src：`table/table.c`（13 個 leaf accessor）、`table/orphan.c`（orphan 表）、
  `table/btltab3.c`（10 張核心玩法資料表）、`table/chtab3.c`（章節 intro/cutscene 表）；
  其餘 10 個 `*tab.c` 為純資料檔（見下方 14 檔總覽）。
- Ghidra 對象（位址即時核）：leaf accessor `fd2_get_orphan_packed3_table_entry` @0x4DB84、
  `fd2_get_class_promotion_data_entry` @0x4E48D … `fd2_get_cutscene_event_script` @0x4E7F8；
  核心表 `data_fd2_battle_item_effect_table` @0x602AC、`data_fd2_battle_spell_effect_table` @0x619FD、
  `data_fd2_battle_enemy_data_table` @0x61AF9、`data_fd2_battle_character_base_table` @0x61DA1、
  `data_fd2_battle_character_growth_table` @0x620A1、`data_fd2_battle_spell_learning_table` @0x626B3、
  `data_fd2_chapter_intro_metadata_table` @0x6238D。
- 相關資源：無外部資源檔；所有表均編譯進 FD2.LE 的唯讀資料段。欄位值細節見
  `assets/tables/*.md` 與 `assets/{items,spells,enemies,characters,jobs}.md`。

## src/table 14 檔總覽

| `.c` 檔 | 內容性質 | 代表資料表 @ Ghidra 位址 | assets/tables 對應 | 主要消費子系統 |
|---|---|---|---|---|
| `table.c` | 13 個 leaf accessor 函數（無資料） | accessor @0x4DB84、0x4E48D–0x4E7F8 | — | 各子系統（透過 accessor） |
| `orphan.c` | orphan 唯讀表 | `data_fd2_orphan_packed3_table` @0x60181（byte[299]） | — | 無（dead accessor） |
| `btltab.c` | 戰鬥數值常數／修正表／召喚動畫 | `data_fd2_battle_job_magic_resist_table` @0x51F96（dword[28]） | `job_magic_resist.md`、`job_crit.md` | `battle.md` |
| `btltab2.c` | 召喚／受擊動畫參數、浮動傷害佇列 | `data_fd2_battle_summon_main_anim_12slot_y_offset_table` @0x52460（int[12]） | — | `battle.md`、`anim.md` |
| `btltab3.c` | 10 張核心玩法資料表（+ 少量 BSS 暫存） | 見下方「核心玩法資料表與 accessor」 | `item_effect.md`、`spell_effect.md`、`enemy_data.md`、`character_base.md`、`character_growth.md`、`spell_learning.md` | `battle.md`、`spell.md`、`field.md`、`town_menu.md`、`ui_menu.md` |
| `chtab.c` | 章末演出座標（ch03–12）、片尾配樂觸發幀 | `data_fd2_chapter_ending_music_trigger_frames` @0x5204E（int[15]） | — | `field.md`、`anim.md` |
| `chtab2.c` | 章末演出座標（ch12–18）、片尾 credit、intro 立繪座標 | `data_fd2_chapter_combat_cinematic_mode_per_chapter` @0x52363（byte[30]） | — | `field.md`、`anim.md` |
| `chtab3.c` | 章節 intro metadata、cutscene 腳本指標表 | `data_fd2_chapter_intro_metadata_table` @0x6238D（26×31 bytes） | `chapter_intro_metadata.md` | `field.md`、`town_menu.md` |
| `gfxtab.c` | 圓形／散射動畫浮點常數、tile 動畫 palette | `data_fd2_graphics_tile_anim_palette_phase_lookup` @0x51A97（byte[20]） | — | `gfx.md` |
| `anitab.c` | 法術／召喚動畫參數、palette 循環表 | `data_fd2_animation_palette_cycle_rgb_table` @0x60003（byte[93]） | — | `anim.md` |
| `audtab.c` | 章節 BGM 音軌、SFX bank、腳步聲節奏 | `data_fd2_audio_per_chapter_player_turn_bgm_track` @0x51E63（byte[30]） | — | `audio.md` |
| `dlgtab.c` | 對話框模板、商店對話文字 id 表 | `data_fd2_dialog_shop_buy_for_dialog_text_id_table` @0x526FA（short[6]） | — | `dialog.md` |
| `strtab.c` | OOM 訊息、資源檔名、格式模板等字串常數 | `data_fd2_string_resource_filename_fdtxt_dat` @0x51A43（string） | — | `save.md`、`rsrc.md`、`ui_menu.md`、`field.md` |
| `uitab.c` | 選單模板、復活／轉職費用、intro 對話角落偏移 | `data_fd2_ui_field_command_menu_options_template` @0x51E9F（int[4]） | — | `ui_menu.md`、`town_menu.md` |

位址上分兩群：核心玩法資料表（accessor 所指的表）集中在 0x60181–0x627D8；各子系統的繪圖／動畫／
音效／UI／對話／字串表則落在 0x519xx–0x526xx（少數浮點與 palette 常數在 0x60003 起）。分類靠命名前綴
與消費端，不靠位址範圍。

## 核心玩法資料表與 accessor（table.c）

`table.c` 提供 13 個 `__cdecl` leaf accessor，每個吃一個 index 引數、回傳 `uint8 *`、無副作用。
它們把「表基底 + idx × stride」（或指標表 deref）集中成單一函數，讓 battle / spell / field /
town_menu / ui_menu 各子系統以統一入口取表。並非每張表都有專屬 accessor：`job_magic_resist`
（@0x51F96）、`job_crit_rate`、tile 修正表等由各呼叫點直接以 index 存取。

### 索引型 accessor（回傳指向第 idx 個 entry 的指標）

| accessor @位址 | 目標表 @位址 | entry 數 × stride | index 語意 |
|---|---|---|---|
| `fd2_get_class_promotion_data_entry` @0x4E48D | `data_fd2_battle_class_promotion_data_table` @0x615FE | 36 × 2 B | `class_id - 0x20`（class_id >= 0x20） |
| `fd2_get_spell_learning_entry` @0x4E4A2 | `data_fd2_battle_spell_learning_table` @0x626B3 | 20 × 12 B | 學法索引（來自 growth +0x0A） |
| `fd2_get_chapter_intro_metadata_entry` @0x4E4B9 | `data_fd2_chapter_intro_metadata_table` @0x6238D | 26 × 31 B | `chapter_id - 1`（1-based 章號） |
| `fd2_get_char_growth_entry` @0x4E4D1 | `data_fd2_battle_character_growth_table` @0x620A1 | 68 × 11 B | 立繪／角色 id（0..67） |
| `fd2_get_char_base_entry` @0x4E4E8 | `data_fd2_battle_character_base_table` @0x61DA1 | 32 × 24 B | `char_id`（0..0x1F） |
| `fd2_get_enemy_data_entry` @0x4E4FF | `data_fd2_battle_enemy_data_table` @0x61AF9 | 68 × 10 B | 敵人相對 idx =（class/portrait id − 0x44），0..0x43 |
| `fd2_get_spell_effect_entry` @0x4E516 | `data_fd2_battle_spell_effect_table` @0x619FD | 36 × 7 B | `spell_id`（0..0x23） |
| `fd2_get_job_allowed_items_table_entry` @0x4E53E | `data_fd2_battle_job_allowed_items_table` @0x6188A | 29 實體列 × 7 B（job 0..0x1A 可定址） | `job_id`（0..0x1A） |
| `fd2_get_movement_cost_table_for_job` @0x4E555 | `data_fd2_battle_movement_cost_table` @0x61646 | 29 實體列 × 20 B（job 0..0x1A 可定址） | `job_id`，或移動類別（0x13 飛行、0x10 特殊單位） |
| `fd2_get_item_effect_entry` @0x4E56C | `data_fd2_battle_item_effect_table` @0x602AC | 215 × 23 B | `item_id`（0..0xD6），回傳 `&entry.type`（略過前置 byte） |

### 指標表 deref accessor（回傳第 idx 格所存的指標）

| accessor @位址 | 指標表 @位址 | 格數 × stride | index 語意 |
|---|---|---|---|
| `fd2_get_attack_anim_pattern_for_weapon` @0x4E52D | `data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21` @0x61955 | 21 × 4 B | `weapon_type`（回傳該格所存的攻擊動畫腳本指標） |
| `fd2_get_cutscene_event_script` @0x4E7F8 | `data_fd2_chapter_cutscene_event_script_ptr_table_106` @0x627D8 | 106 × 4 B | 走路動畫 cutscene `event_id`（0..105） |

### orphan accessor（dead）

`fd2_get_orphan_packed3_table_entry` @0x4DB84 回傳
`data_fd2_orphan_packed3_table`（@0x60181，byte[299]，99 筆 3-byte 記錄 + 2-byte 尾）內第 idx 筆
3-byte 記錄。此 accessor 靜態 xref 為 0（只可能經章節腳本的間接／函數指標呼叫觸及）。資料表本身
定義在 `orphan.c`，accessor 函數則與其餘 leaf accessor 一樣定義在 `table.c`。

## 幾張表的語意重點（值細節見 assets）

- **class_promotion**（0x615FE，36×2）：`byte[0]` = 轉職後 job_id；`byte[1]` = 升職移動力加成，
  非零時把該值加進 runtime_char 移動力欄並顯示「移動力增加N點！」（text page 0x254），**不是**學法術
  id。轉職流程見 `town_menu.md`。
- **enemy_data**（0x61AF9，68×10）：`+8` = MV 移動力（戰鬥 spawn 時原值複製到 runtime_char +0x3B，
  不乘等級）、`+9` = EX 每等級擊殺經驗係數；HP/MP/AP/DP/DX 五欄皆為「每等級係數」，出場數值 = 係數 ×
  level。runtime_char 佈局見 `overview.md`，數值細節見 `assets/tables/enemy_data.md`、`assets/enemies.md`。
- **character_growth**（0x620A1，68×11）：每 stat 佔 2 byte，`byte[0]` = 每級最小成長、`byte[1]` =
  exclusive 上界（實際最大成長 = byte−1；`byte == min` 時成長固定為 min）；`+0x0A` = 學法索引
  （0xFF = 無）。公式與特例見 `assets/tables/character_growth.md`。
- **chapter_intro_metadata**（0x6238D，26×31）：`+0 bCategory` 是 intro 畫面外觀變體碼（值域 0/1/2，
  決定 intro panel 背景資源與立繪座標列），**不是** story/battle 章節旗標；story/battle 分派由
  `data_fd2_chapter_per_chapter_category_table`（chtab3.c）決定。細節見
  `assets/tables/chapter_intro_metadata.md`。
- **movement_cost**（0x61646，29×20）：每列是某 job 的 per-tile-type 移動成本；回傳指標交給 flood-fill／
  尋路常式當成本表。移動範圍與尋路機制見 `pathfind.md`。

## 消費鏈範例

`fd2_get_item_effect_entry`（item_effect 表）的呼叫端橫跨戰鬥傷害與命中結算（`fd2_execute_attack_damage_calculation`、
`fd2_calculate_combat_hit_outcome`、`fd2_check_can_counter_attack`）、AI 評分（`fd2_ai_score_physical_attack`、
`fd2_ai_score_item_use`）、以及城鎮商店與背包／狀態（`fd2_run_buy_item_menu`、`fd2_check_job_can_equip_item`、
`fd2_recompute_runtime_char_total_stats`）。`fd2_get_movement_cost_table_for_job` 則由玩家行動選單與 AI 走位／
施法選點（`fd2_player_action_menu_loop`、`fd2_ai_seek_optimal_position`、`fd2_ai_score_physical_attack`、
`fd2_ai_score_offensive_spell` 等）取用，全部再餵給 flood-fill／pathfind。
