# 第 12 章 — chapter_12

Init handler 的 cutscene 在 dialog 之前先觸發（0x28、0x29），dialog 在最後 — 與其他章節的順序不同。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_12_init @ 0x000333F5` | 118 B |
| End | `chapter_12_end @ 0x000237D5` | 214 B |
| Post-action | `chapter_12_post_action @ 0x0002073D` | (custom) |
| BGM (player turn) | `per_chapter_player_turn_bgm[11]` |  |
| BGM (enemy turn) | `per_chapter_enemy_turn_bgm[11]` |  |

## Init handler 階段

1. `init_battle_state_for_chapter`
2. `pan_cursor_and_window(4, 4)`
3. `chapter_init_phase_flag = 1`；`load_chapter_portraits_and_dump_tmp(1)`；flag = 0
4. `cutscene_event_trigger(0x28)`
5. `pan_cursor_and_window(0xB, 0x28)` + `cutscene_event_trigger(0x29)`
6. `clear_all_chars_facing` + `display_dialog_scene(page=0)`
7. `pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 12 | 0 |
| End | 12 | 3, 4 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。

End handler 中：
- `init_runtime_char_from_base_growth(0x11 = 17)` — 米亞斯多德

## Cutscene events

`0x28, 0x29` (init), `0x2D` (end)。

## Post-action handler

`chapter_12_post_action @ 0x2073D`：

- default 判定（全敵死 = win，索爾死 = lose）
- 額外 lose 條件：char[0xE = 14] 死亡

對應「失敗條件：索爾死亡，米亞斯多德死亡」——char[0xE] = 米亞斯多德。

「第一回合己方結束時敵方第一波援軍出現」由 FDFIELD turn-event 處理（見下表）。

## End handler events

`chapter_12_end @ 0x237D5`：

1. 從 scene tables (`chapter_12_end_scene_pos_x/y/facing_table`) 讀 4 chars 位置
2. `setup_chars_and_camera_for_intro(0xD, 0xE, 10, 2, 0, 4, 0)` — 配 6 chars 進場
3. `display_dialog_scene(page=3)`
4. `cutscene_event_trigger(0x2D)`
5. `display_dialog_scene(page=4)`
6. `save_runtime_char_to_template`
7. `init_runtime_char_from_base_growth(0x11 = 17)` — 米亞斯多德加入
8. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **34** (= chapter_id × 3 + 1, chapter_id = 11)，entry size 1691 bytes。
header `+0..+2` = shap_id_byte / party_count = 14 / char_count = 60；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

2 / 16 active hooks（其餘 14 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 1 | 0 (enemy_turn_intro) | 0x23 | `0x00034C76` | cinematic_no_dialog |
| 5 | 1 (end_of_player_turn) | 0x24 | `0x00034CB3` | ai_setup |
