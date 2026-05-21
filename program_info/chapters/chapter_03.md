# 第 3 章 — chapter_03

含 char survival 條件式 recruit (鐵諾)；end handler 用 scene_pos tables 排版 cutscene。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_03_init @ 0x00032E8C` | 324 B |
| End | `fd2_chapter_03_end @ 0x000230F2` | 214 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default — 無自訂勝負) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[2]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[2]` |  |

## Init handler 階段

`fd2_init_battle_state_for_chapter` 進入正式戰鬥模式後，依序：

- `fd2_pan_cursor_and_window(3, 0x11)` 場景
- `fd2_display_dialog_scene(page=0)`
- `fd2_cutscene_event_trigger(0x12)` + `fd2_load_chapter_portraits_and_dump_tmp(race_id=1)`
- `fd2_pan_cursor_and_window(3, 6)` + `fd2_cutscene_event_trigger(0x11)`
- `fd2_display_dialog_scene(page=1)` + `fd2_cutscene_event_trigger(0x13)`
- `fd2_display_dialog_scene(page=2)` + `fd2_pan_cursor_and_window(3, 0x11)`
- `fd2_display_dialog_scene(page=3)` + `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 3 | 0, 1, 2, 3 |
| End (char[6] 活) | 3 | 7 |
| End (char[6] 死) | 3 | 6 |

## char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 條件式 `fd2_init_runtime_char_from_base_growth(2)` → 鐵諾加入（僅當 char[6] 存活）。

## Cutscene events

- Init: `0x11, 0x12, 0x13` (3 events)

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id]` 的 walk-animation script。

## Post-action handler

`data_fd2_chapter_post_action_handler_table[2]` 指向 `fd2_check_battle_end_default_handler`，
無自訂勝負條件：所有 team-0 死 → win，索爾 (char_id 0) 死 → lose。

## End handler events

`fd2_chapter_03_end @ 0x000230F2` (214 B)：

1. 從 `data_fd2_chapter_ch03_end_scene_char_pos_x_table` / `pos_y_table` / `facing_table` (各 8 entries @ 0x520BD/0x520C4/0x520CB) 讀 4 entries → local recruit_block × 3
2. `fd2_save_runtime_char_to_template`
3. **Conditional**：`fd2_check_char_is_dead(6)`
   - 若 char[6] 活著 → `fd2_setup_chars_and_camera_for_intro(...)` + `fd2_display_dialog_scene(page=7)` +
     `fd2_init_runtime_char_from_base_growth(2)` — char 2 = 鐵諾加入
   - 若 char[6] 已死 → `fd2_display_dialog_scene(page=6)` (skip recruit)
4. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **7** (= chapter_id × 3 + 1, chapter_id=2)，entry size 1171 bytes。
header `+0..+2` = shap_id_byte / party_count=6 / char_count=40；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

1 / 16 active hooks (其餘 15 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 2 (new_player_turn_intro) | 0x09 | `0x000344C2` | char_conditional; ch3_char_cond |
