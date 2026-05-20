# 第 18 章 — chapter_18

首章「擊殺指定 boss = win」設計：post_action 完全 bypass default，自行實作勝負 (char[0x34] boss 死 = 勝；chars[0,0x10,0x11] 任一死 = 敗)。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_18_init @ 0x000335DA` | 154 B |
| End | `chapter_18_end @ 0x00023CD5` | 356 B |
| Post-action | `chapter_18_post_action @ 0x000208CF` | bypass default; 自行實作勝負 |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[17]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[17]` |  |

## Init handler 階段

1. `init_battle_state_for_chapter`
2. `display_dialog_scene(page=0)`
3. `pan_cursor_and_window(0x10, 4)` + `cutscene_event_trigger(0x36)`
4. `display_dialog_scene(page=1)`
5. `pan_cursor_and_window(0x10, 4)` + `cutscene_event_trigger(0x37)`
6. `display_dialog_scene(page=2)` + `pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 18 | 0, 1, 2 |
| End | 18 | 7, 8, 9, 10 |

## char_id 初始化序列

無 init handler 內 char init。約拿 (char_id 0x15 = 21) 與蘭斯洛特 (char_id 7) 由 end handler 加入。

## Cutscene events

- Init：`0x36`, `0x37`
- End：`0x38`, `0x39`, `0x3A` (3 events 連鎖)

## Post-action handler

`chapter_18_post_action @ 0x208CF` (bypass default — 自行實作所有勝負邏輯)：

- if `chars[0]` OR `chars[0x10]` OR `chars[0x11]` 任一死亡 → game_event_flag = 1 (lose)
- if `char[0x34]` 死亡 → game_event_flag = 2 (win)

| char slot | 角色 | 條件 |
|---|---|---|
| char[0] | 索爾 | 死亡 = 敗 |
| char[0x10] | 約拿 (NPC) | 死亡 = 敗 |
| char[0x11] | 蘭斯洛特 (NPC) | 死亡 = 敗 |
| char[0x34] | 黑暗騎士 boss | 死亡 = 勝 |

註：post_action 用 char[0x10]/[0x11] 為 lose 條件，但 end handler 加入時用 char_id 21/7 (base table id)；這是 runtime slot vs base char_id 不同 indices。

## End handler events

`chapter_18_end @ 0x23CD5` (356 B)：

1. 從 scene tables (`chapter_18_end_scene_pos_x/y_table` + `facing_table`) 讀 5 entries
2. `save_runtime_char_to_template`
3. `setup_chars_and_camera_for_intro(0x10, 0x11, 0x19, 8, 1, 0x12, 4)`
4. `display_dialog_scene(page=7)` + `cutscene_event_trigger(0x38)`
5. `display_dialog_scene(page=8)` + `cutscene_event_trigger(0x39)`
6. `display_dialog_scene(page=9)` + `cutscene_event_trigger(0x3A)`
7. `display_dialog_scene(page=10)`
8. `init_runtime_char_from_base_growth(0x15=21)` (約拿) + `init_runtime_char_from_base_growth(7)` (蘭斯洛特)
9. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **52** (= chapter_id × 3 + 1, chapter_id=17)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**2/16 active hooks**：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 0 (enemy_turn_intro) | 0x2B | `0x00035091` | ai_setup |
| 8 | 0 (enemy_turn_intro) | 0x2A | `0x0003505F` | dialog_only (敵援軍出現) |
