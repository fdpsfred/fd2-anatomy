# 第 26 章 — chapter_26

End handler 用 `tile_event_consumed_flags[0xC]` 動態決定 dialog page (5 條路線對應 5 個寶箱選擇)；對應「畫面最上方 5 個寶箱選 1」的最強武器分支。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_26_init @ 0x00033AAE` | 67 B |
| End | `fd2_chapter_26_end @ 0x00024E80` | 466 B |
| Post-action | `fd2_chapter_26_post_action @ 0x00020B3C` | default + lose if char[1] OR char[2] dead |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[25]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[25]` |  |

## Init handler 階段

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(9, 0x27)` + `fd2_cutscene_event_trigger(0x4C)`
3. `fd2_display_dialog_scene(page=0)` + `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 26 | 0 |
| End (dynamic page #1) | 26 | tile_event_consumed_flags[0xC] + 5 → 5..9 |
| End (固定) | 26 | 7 |
| End (dynamic page #2) | 26 | tile_event_consumed_flags[0xC] + 8 → 8..12 |
| End (固定) | 26 | 10, 11 |

## char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫；本章不在 init 或 end 直接加入新角色。
機器人渥德加入由 FDFIELD tile-step / dialog event 處理 (見 entry 26 dialog page 4)。

## Cutscene events

- Init: `0x4C`
- End: `0x4D, 0x4E, 0x4F, 0x50` (4 events)

## Post-action handler

`fd2_chapter_26_post_action @ 0x00020B3C`：
- 標準 default 判定（全敵死 = 勝、索爾死 = 負）
- **額外 lose 條件**：if chars[1] (亞奇梅吉) OR chars[2] (悠妮) 死 → `game_event_flag = 1`

註：runtime char index 隨章節而變，由編成畫面 per-chapter pin 決定：chars[1] 在 ch22/23
是希爾法、ch27/28 是悠妮；ch26 先 pin 悠妮(9) 再 pin 亞奇梅吉(0x1D)，最終
chars[1]=亞奇梅吉、chars[2]=悠妮。

## End handler events

`fd2_chapter_26_end @ 0x00024E80` (466 B) — `tile_event_consumed_flags[0xC]` 動態 dialog page selection：

1. 從 `chapter_26_end_scene_pos_x/y/facing_table` 讀位置
2. **Reposition NPCs**：迴圈 chars[0x10..party_member_count]，若 `bPortrait_id == 0x1F` → 設 bPos = (0x10, 6)
3. `fd2_setup_chars_and_camera_for_intro(0xF, 0, 0, 0, 0, 9, 5)`
4. **Dynamic page #1**：`page = tile_event_consumed_flags[0xC] + 5` → `fd2_display_dialog_scene(page = 5..9)`
5. `fd2_cutscene_event_trigger(0x4D)`
6. `fd2_display_dialog_scene(page=7)` (固定)
7. `fd2_cutscene_event_trigger(0x4E)`
8. **Dynamic page #2**：`page = tile_event_consumed_flags[0xC] + 8` → `fd2_display_dialog_scene(page = 8..12)`
9. `fd2_cutscene_event_trigger(0x4F)`
10. `fd2_display_dialog_scene(page=10)` + `fd2_cutscene_event_trigger(0x50)`
11. `fd2_display_dialog_scene(page=11)`
12. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

flag 值與 dialog page 對應：

| `tile_event_consumed_flags[0xC]` | Dynamic #1 (page) | Dynamic #2 (page) |
|---|---|---|
| 0 | 5 | 8 |
| 1 | 6 | 9 |
| 2 | 7 | 10 |
| 3 | 8 | 11 |
| 4 | 9 | 12 |

對應「5 個寶箱選 1」的 5 條對話路線。

## FDFIELD event script

FDFIELD entry idx **76** (= chapter_id × 3 + 1, chapter_id=25)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

9 / 16 active hooks (其餘 7 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 2 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
| 4 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
| 6 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
| 8 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
| 10 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
| 12 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
| 15 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
| 16 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
| 17 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | 援軍 reinforcement |
