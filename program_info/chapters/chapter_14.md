# 第 14 章 — chapter_14

Init handler 僅 29 B（30 章中第二小）；FDFIELD 完全靜態（無 active turn-event hooks）。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_14_init @ 0x0003347C` | 29 B (第二小) |
| End | `chapter_14_end @ 0x000238DC` | 225 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `per_chapter_player_turn_bgm[13]` |  |
| BGM (enemy turn) | `per_chapter_enemy_turn_bgm[13]` |  |

## Init handler 階段

極簡：

1. `init_battle_state_for_chapter`
2. `pan_cursor_and_window(0x14, 0x14)`
3. `display_dialog_scene(page=0)`
4. `pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 14 | 0 |
| End | 14 | 2, 3 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。
End handler 也無——ch14 結算無新加入。

## Cutscene events

無 init cutscene；`0x2F` (end)。

## Post-action handler

`per_chapter_post_action_handler[13]` 指向 `check_battle_end_default_handler` — 全敵死 = win，索爾 (char_id 0) 死 = lose。

攻略「當己方通過地圖中央一帶，則敵軍便會前來攻擊」屬 FDFIELD position-trigger event，不在 post_action handler。

## End handler events

`chapter_14_end @ 0x238DC`：

1. 從 scene tables (`chapter_14_end_scene_pos_x/y/facing_table`) 讀 4 chars 位置
2. `load_chapter_portraits_and_dump_tmp(1)`
3. `setup_chars_and_camera_for_intro(0xF, 0, 0, 0, 0, 0xC, 10)`
4. `display_dialog_scene(page=2)`
5. `cutscene_event_trigger(0x2F)`
6. `display_dialog_scene(page=3)`
7. `save_runtime_char_to_template`
8. `current_chapter_id += 1`

無 `init_runtime_char_from_base_growth`。

## FDFIELD event script

FDFIELD entry idx **40** (= chapter_id × 3 + 1, chapter_id = 13)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count = 16 / char_count = 70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**No active turn-event hooks** — 全 16 個 slot 為 sentinel `(turn = 0xFF, event_code = 0xFF, phase = 0)`。本章 turn-based events 完全靜態，由 init / post_action / FDFIELD char spawn records 處理。
