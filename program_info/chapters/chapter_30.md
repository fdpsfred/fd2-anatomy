# 第 30 章 — chapter_30

GOOD ENDING + staff roll：擊殺空魔神 (chars[0x14]) 後，end handler 推進 chapter_id 到 31，載入 epilogue map (FDFIELD entry for chapter 31)，引用 epilogue dialog page 0/1，呼叫 `play_game_ending_cinematic` 進入 staff roll，最終 infinite loop。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_30_init @ 0x00033E3C` | 316 B |
| End | `chapter_30_end @ 0x00025757` | 544 B |
| Post-action | `chapter_30_post_action @ 0x00020BF5` | bypass default; final boss check |
| BGM (player turn) | `per_chapter_player_turn_bgm[29]` |  |
| BGM (enemy turn) | `per_chapter_enemy_turn_bgm[29]` |  |

## Init handler 階段

最終章開場 cinematic：7× `cinematic_warp_char_to_tile` (魔神群傳送進場) + palette flash：

1. `init_battle_state_for_chapter`
2. `cutscene_event_trigger(0x57)`
3. `pan_cursor_and_window(0x10, 0x13)` + `display_dialog_scene(page=0)`
4. `pan_cursor_and_window(0x10, 1)`
5. **4× `cinematic_warp_char_to_tile`** (上方 group)：
   - char 0x15 → tile (0x15, 5)
   - char 0x16 → tile (0x17, 5)
   - char 0x17 → tile (0x14, 5)
   - char 0x18 → tile (0x18, 5)
6. `display_dialog_scene(page=1)`
7. `animate_palette_flash_pulse_white` — 全螢幕白光閃爍
8. `display_dialog_scene(page=2)`
9. `pan_cursor_and_window(0x10, 0xE)`
10. **3× `cinematic_warp_char_to_tile`** (下方 group)：
    - char 0x18 → tile (0x16, 0x12)
    - char 0x19 → tile (0x15, 0x12)
    - char 0x1A → tile (0x17, 0x12)
11. `pan_cursor_to_char(0)`

7 個 chars 用 cinematic warp 進場，對應魔神 / boss group 從不同方向「傳送」到戰場（空魔神、水魔神、地魔神、風魔神、火魔神、機甲）。

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 30 | 0, 1, 2 |
| Post-action (chars[1] 死) | 30 | 7 |
| End (chapter 30 text) | 30 | 9, 10 |
| End (epilogue text from chapter 31 entry) | 31 | 0, 1 |

## char_id 初始化序列

無 `init_runtime_char_from_base_growth` 呼叫；最終章不加入新角色。

## Cutscene events

- Init: `0x57`
- End: `0x58, 0x59`

## Post-action handler

`chapter_30_post_action @ 0x00020BF5` — bypass default：

- if chars[0x14] (空魔神) dead → `game_event_flag = 2` (win)
- if chars[0] (索爾) dead → `game_event_flag = 1` (lose)
- if chars[1] (悠妮) dead → `display_dialog_scene(page=7)` + `game_event_flag = 1`

任何 turn 殺 chars[0x14] 都直接 win — 支持 speedrun 「第 1 回合便殺掉空魔神」。

## End handler events

`chapter_30_end @ 0x00025757` (544 B) — GOOD ENDING + staff roll：

1. 從 `chapter_30_end_scene_pos_x/y/facing_table` 讀位置 (5 entries each)
2. `setup_chars_and_camera_for_intro(0x13, 0, 0, 0, 0, 0x10, 0x12)`
3. `display_dialog_scene(page=9)` + `cutscene_event_trigger(0x58)`
4. `display_dialog_scene(page=10)`
5. `pan_cursor_and_window(0x10, 0x12)` + `pan_cursor_to_tile_animated(0x16, 0x17)`
6. `cast_screen_wide_spell_with_fade(cursor, cursor_y+1, 10, 8)` — final boss death spell visual
7. `restore_all_chars_full_hp_mp` (×2 重複)
8. `current_chapter_id += 1` (= 31，out-of-range chapter id)
9. `load_chapter_battle_data(current_chapter_id)` — 載入 chapter 31 資料 (epilogue map)
10. `battle_window_origin = (0xB, 5)`，cursor reset
11. `composite_battle_frame(1)` + **64-step palette fade-in** (`for i=0x3E down to 0` + 4ms)
12. **40 frames composite + wait** — 過場動畫
13. `display_dialog_scene(page=0)` (epilogue page 0，從新載入的 chapter 31 dialog entry)
14. `pan_cursor_and_window(0xB, 0xC)` + `cutscene_event_trigger(0x59)`
15. `display_dialog_scene(page=1)` (epilogue page 1)
16. `play_game_ending_cinematic @ 0x2BCE5` — GOOD ENDING / staff roll
17. **infinite loop** — 結束於此

「chapter 31」是 epilogue scene 而非實際章節：`chapter_init/end_jump_table[30..]` 越界但 `load_chapter_battle_data(31)` 仍能載入 FDFIELD.DAT entry 30 (0-indexed) 作為 ending cinematic 的背景地圖。

`play_final_chapter_30_ending @ 0x2C405` 為另一 named function，處理 final chapter 內部 cinematic chain 的動態 page 機制 (`char.identity+1 / bJob_id+0x96 / 0x2d fallback`)。

## FDFIELD event script

FDFIELD entry idx **88** (= chapter_id × 3 + 1, chapter_id=29)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=20 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

1 / 16 active hooks (其餘 15 為 sentinel 在初始狀態)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0 (enemy_turn_intro) | 0x52 | `0x00035F92` | sentinel-like (final boss 終章 event reference) |

`turn=0xFF` 不會等於回合計數，初始 hook table 不會 fire。實際觸發機制：tile-step-event handler 在某些劇本 tile 被踩到時，會動態 rewrite 該 chapter turn-event-hook table 的 turn byte (0xFF → current_save_metadata_block / +1)，把原本 sentinel 的 entry 啟動為下一回合 fire 的 event。對應「空魔神傳送援軍」屬此類。
