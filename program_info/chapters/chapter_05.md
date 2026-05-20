# 第 5 章 — chapter_05

End handler 用 scene_pos tables 排版 cutscene + `init_runtime_char_from_base_growth(10)` 加入瑪琳。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_05_init @ 0x00033049` | 258 B |
| End | `chapter_05_end @ 0x000231F9` | 157 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[4]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[4]` |  |

## Init handler 階段

`init_battle_state_for_chapter` 後依序：

- `display_dialog_scene(page=0)`
- `pan_cursor_and_window(3, 3)` + `load_chapter_portraits_and_dump_tmp(race_id=1)`
- `cutscene_event_trigger(0x16)` + `display_dialog_scene(page=1)`
- `pan_cursor_and_window(8, 0xE)` + `cutscene_event_trigger(0x15)`
- `display_dialog_scene(page=2)` + `pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 5 | 0, 1, 2 |
| End | 5 | 9 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。

End handler 末段 `init_runtime_char_from_base_growth(10)` → 瑪琳加入。

## Cutscene events

- Init: `0x15, 0x16` (2 events，注意非按字典序：先 0x16，再 0x15)

## Post-action handler

`data_fd2_chapter_post_action_handler_table[4]` 指向 `check_battle_end_default_handler`，
無自訂勝負條件。

## End handler events

`chapter_05_end @ 0x000231F9` (157 B)：

1. 從 `data_fd2_chapter_ch05_end_scene_char_pos_x_table` / `pos_y_table` / `facing_table`
   (各 4 entries @ 0x520D2/0x520D9/0x520E0) 讀 4 entries → recruit_block × 3
2. `setup_chars_and_camera_for_intro(...)` 設置 camera + chars positions
3. `display_dialog_scene(page=9)`
4. `init_runtime_char_from_base_growth(10)` — char 10 = 瑪琳加入
5. `save_runtime_char_to_template` + `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **13** (= chapter_id × 3 + 1, chapter_id=4)，entry size 1431 bytes。
header `+0..+2` = shap_id_byte / party_count=7 / char_count=50；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

4 / 16 active hooks (其餘 12 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 0 (enemy_turn_intro) | 0x0E | `0x000345EA` | dialog_with_state; ch5_dialog_with_state |
| 4 | 1 (end_of_player_turn) | 0x0F | `0x0003462E` | dialog_with_state; ch5_dialog_with_state |
| 7 | 0 (enemy_turn_intro) | 0x10 | `0x00034696` | dialog_only; ch5_dialog |
| 8 | 0 (enemy_turn_intro) | 0x11 | `0x000346C8` | dialog_with_state; ch5_dialog_with_state |
