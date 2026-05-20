# 第 8 章 — chapter_08

End handler 含 fade-to-black 轉場 (palette darken + framebuffer clear)；FDFIELD 含 turn 2-7 騎兵 reinforcement chain。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_08_init @ 0x00033219` | 100 B |
| End | `chapter_08_end @ 0x000234BB` | 140 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[7]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[7]` |  |

## Init handler 階段

`init_battle_state_for_chapter` 後依序，對稱結構（兩段 pan + cutscene + dialog）：

- `pan_cursor_and_window(7, 0x20)` + `cutscene_event_trigger(0x1F)` + `display_dialog_scene(page=0)`
- `pan_cursor_and_window(7, 0x17)` + `cutscene_event_trigger(0x20)` + `display_dialog_scene(page=1)`
- `pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 8 | 0, 1 |
| End | 8 | 3, 4 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。

End handler 末段 `init_runtime_char_from_base_growth(5)` → 洛娜加入。

## Cutscene events

- Init: `0x1F, 0x20` (2 events)
- End: `0x21, 0x22` (2 events)

## Post-action handler

`data_fd2_chapter_post_action_handler_table[7]` 指向 `check_battle_end_default_handler`，
無自訂勝負條件。每回合敵騎兵援軍 6 組由 FDFIELD turn-event hooks 處理，**不是**
post_action_handler。

## End handler events

`chapter_08_end @ 0x000234BB` (140 B)：

1. 從 `data_fd2_chapter_ch08_end_scene_char_pos_x_table` / `pos_y_table` (@ 0x520FC/0x52106, 各 4 entries) 讀 4 chars 位置
2. `setup_chars_and_camera_for_intro(...)`
3. `display_dialog_scene(page=3)`
4. `cutscene_event_trigger(0x21)`
5. `display_dialog_scene(page=4)`
6. `cutscene_event_state = 1` + `cutscene_event_trigger(0x22)` + `cutscene_event_state = 0`
7. `set_vga_palette_range(0, 0xFF, 0x40)` — palette darken
8. `crt_memset(0xA0000, 0, 64000)` — 清空整個 framebuffer (黑屏 fade)
9. `init_runtime_char_from_base_growth(5)` — char 5 = 洛娜加入
10. `save_runtime_char_to_template` + `current_chapter_id += 1`

末段 framebuffer clear 是 fade-to-black 轉場效果。

## FDFIELD event script

FDFIELD entry idx **22** (= chapter_id × 3 + 1, chapter_id=7)，entry size 1691 bytes。
header `+0..+2` = shap_id_byte / party_count=10 / char_count=60；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

7 / 16 active hooks (其餘 9 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 2 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement (turn 2-7 phase 0 各 fire 一次 = 每回合敵騎兵援軍 6 組×2 名) |
| 3 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 4 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 5 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 6 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 7 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 15 | 0 (enemy_turn_intro) | 0x1C | `0x00034A0E` | ai_setup; ch8_ai_ctrl |
