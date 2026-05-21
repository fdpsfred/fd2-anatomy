# 第 22 章 — chapter_22

End handler 為 FD2 全 30 章中唯一以「全螢幕白屏 → palette fade → 黑屏」收尾的章節 (`memset(0xA0000, 0xFF, 64000)` 後再 fade-to-black)。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_22_init @ 0x0003367E` | 34 B |
| End | `fd2_chapter_22_end @ 0x000244B6` | 354 B |
| Post-action | `fd2_chapter_22_27_28_post_action_shared @ 0x00020A87` | 與 ch27/28 共用 |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[21]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[21]` |  |

## Init handler 階段

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(0x10, 0x1C)`
3. `fd2_cutscene_event_trigger(0x43)` + `fd2_clear_all_chars_facing`
4. `fd2_display_dialog_scene(page=0)` + `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 22 | 0 |
| End | 22 | 4, 5, 6 |

## char_id 初始化序列

無 init handler 內 char init。莎拉由 FDFIELD turn-event 加入 (從牢房脫困)，end handler 不加入新角色。

## Cutscene events

- Init：`0x43`
- End：`0x41`, `0x42`

## Post-action handler

`fd2_chapter_22_27_28_post_action_shared @ 0x20A87` (與 ch27/28 共用)：
- default
- 額外 lose：if `char[1]` 死亡

`char[1]` = 希爾法 (跨 ch22/27/28 共用同一 NPC slot)。

## End handler events

`fd2_chapter_22_end @ 0x244B6` (354 B) — 白屏 fade-to-black 結尾：

1. 從 scene tables (含 facing_table) 讀位置
2. `fd2_setup_chars_and_camera_for_intro(0xF, 0x48, 0x16, 0x19, 2, 0x10, 0x12)`
3. `fd2_display_dialog_scene(page=4)` + `fd2_cutscene_event_trigger(0x41)`
4. `fd2_display_dialog_scene(page=5)` + `fd2_pan_cursor_and_window(0x10, 0x10)` + `fd2_cutscene_event_trigger(0x42)`
5. `fd2_display_dialog_scene(page=6)` + `fd2_pan_cursor_and_window(0x10, 0xE)`
6. `fd2_cast_screen_wide_spell_with_fade(cursor_y+3, ..., 10, 8)` — 大範圍 spell visual
7. 500ms wait
8. `memset(0xA0000, 0xFF, 64000)` — **白屏 (整 framebuffer = 0xFF)**
9. `fd2_play_palette_fade_to_black` — palette 漸暗
10. `memset(0xA0000, 0, 64000)` — 黑屏
11. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

「連戰四場後才有商店」屬 `fd2_chapter_transition_menu @ 0x2CAD7` 處理 (與 end handler 解耦)。

## FDFIELD event script

FDFIELD entry idx **64** (= chapter_id × 3 + 1, chapter_id=21)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**3/16 active hooks**：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 0 (enemy_turn_intro) | 0x31 | `0x000351E9` | 三角魔鬼 spawn (t3/t7 phase 0 reinforcement) |
| 5 | 2 (new_player_turn_intro) | 0x32 | `0x00035261` | reinforcement_spawner |
| 7 | 0 (enemy_turn_intro) | 0x31 | `0x000351E9` | 三角魔鬼 spawn |
