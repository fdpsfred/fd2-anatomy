# 第 10 章 — chapter_10

Init handler 把 `runtime_char_array[0x32]` 與 `runtime_char_array[0x33]` 的 `bStatus_sleep_flag` 都設為 100 — 索菲亞與卡納恩三世起始即進入睡眠狀態，作為 cutscene 與失敗判定的鎖定機制。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_10_init @ 0x0003332B` | 90 B |
| End | `chapter_10_end @ 0x000235F9` | 407 B |
| Post-action | `chapter_10_post_action @ 0x00020707` | (custom) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[9]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[9]` |  |

## Init handler 階段

1. `init_battle_state_for_chapter`
2. `pan_cursor_and_window(10, 0)`
3. `runtime_char_array[0x32].bStatus_sleep_flag = 100` — 索菲亞起始睡眠
4. `runtime_char_array[0x33].bStatus_sleep_flag = 100` — 卡納恩三世起始睡眠
5. `display_dialog_scene(page=0)`
6. `pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 10 | 0 |
| End | 10 | 4, 5 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。

End handler 中：
- `init_runtime_char_from_base_growth(0xB = 11)` — 索菲亞
- `init_runtime_char_from_base_growth(6)` — 萊汀

## Cutscene events

`0x25` (end)。Init 無 cutscene event。

## Post-action handler

`chapter_10_post_action @ 0x20707`：

- default 判定（全敵死 = win，索爾死 = lose）
- 額外 lose 條件：char[0x32] 死亡 OR char[0x33] 死亡

對應「失敗條件：索爾死亡，索菲亞死亡，卡納恩三世死亡」——chars[0x32] = 索菲亞、chars[0x33] = 卡納恩三世（即起始睡眠的兩人）。

「第五回合己方結束時援軍出現」由 FDFIELD turn-event 處理（見下表）。

## End handler events

`chapter_10_end @ 0x235F9`（大型轉場 + 多 char 復活）：

1. 從 `data_fd2_chapter_ch10_end_scene_char_pos_x_table_chars_0_6` 與 `data_fd2_chapter_ch10_end_scene_char_pos_y_table` 讀位置
2. `play_palette_fade_to_black` + `clear_all_chars_acted_flag`
3. Reposition `chars[0..0xA]` (11 chars)：每個 char 設 bPos_x/y from table、sprite_state[1] = 2 (face north)
4. Special chars revival/repositioning：
   - `chars[0x32]`: bPos_x = 0xF, bPos_y = 0x23, `bStatus_sleep_flag = 0`（解除索菲亞睡眠）
   - `chars[0x33]`: bPos_x = 0xE, bPos_y = 0x23, `bStatus_sleep_flag = 0`（解除卡納恩三世睡眠）
   - `chars[0x34]`: bPos_x = 0x10, bPos_y = 0x23, `bFlags = 0`（revive）
   - `chars[5].bFlags = 0`（revive char[5]）
5. Reset battle camera：`battle_window_origin_x/y = 9, 0x22`；`cursor_world/screen_x/y = 9/0x22/0/0`
6. `composite_battle_frame` + `play_palette_fade_in` + 200ms wait
7. `display_dialog_scene(page=4)` + `cutscene_event_trigger(0x25)` + `display_dialog_scene(page=5)`
8. `save_runtime_char_to_template`
9. `init_runtime_char_from_base_growth(0xB = 11)` — 索菲亞
10. `init_runtime_char_from_base_growth(6)` — 萊汀
11. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **28** (= chapter_id × 3 + 1, chapter_id = 9)，entry size 1691 bytes。
header `+0..+2` = shap_id_byte / party_count = 11 / char_count = 60；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

2 / 16 active hooks（其餘 14 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 5 | 1 (end_of_player_turn) | 0x20 | `0x00034BE2` | dialog_only |
| 20 | 0 (enemy_turn_intro) | 0x21 | `0x00034C1E` | dialog_with_state |
