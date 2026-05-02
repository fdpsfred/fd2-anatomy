# 第 11 章 — chapter_11

Init handler 含 2 個 cutscene + dialog 序列；FDFIELD 完全靜態（無 active turn-event hooks）。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_11_init @ 0x00033367` | 142 B |
| End | `chapter_11_end @ 0x00023790` | 69 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `per_chapter_player_turn_bgm[10]` |  |
| BGM (enemy turn) | `per_chapter_enemy_turn_bgm[10]` |  |

## Init handler 階段

1. `init_battle_state_for_chapter`
2. `display_dialog_scene(page=0)`
3. `pan_cursor_and_window(10, 7)` + `load_chapter_portraits_and_dump_tmp(1)`
4. `cutscene_event_trigger(0x26)` + `display_dialog_scene(page=1)`
5. `cutscene_event_trigger(0x27)` + `display_dialog_scene(page=2)`
6. `pan_cursor_to_char(0)` + `clear_all_chars_facing`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 11 | 0, 1, 2 |
| End | 11 | 3 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。

End handler 中：
- `init_runtime_char_from_base_growth(0xE = 14)` — 珊

## Cutscene events

`0x26, 0x27` (init)。End 無 cutscene event。

## Post-action handler

`per_chapter_post_action_handler[10]` 指向 `check_battle_end_default_handler` — 全敵死 = win，索爾 (char_id 0) 死 = lose。

攻略「珊會跟著貝克威走」屬 NPC follow AI behavior（NPC class 0xB heal/follow logic），不在 post_action handler。

## End handler events

`chapter_11_end @ 0x23790`：

1. `display_dialog_scene(page=3)`
2. `save_runtime_char_to_template`
3. `init_runtime_char_from_base_growth(0xE = 14)` — 珊加入
4. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **31** (= chapter_id × 3 + 1, chapter_id = 10)，entry size 1171 bytes。
header `+0..+2` = shap_id_byte / party_count = 13 / char_count = 40；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**No active turn-event hooks** — 全 16 個 slot 為 sentinel `(turn = 0xFF, event_code = 0xFF, phase = 0)`。本章 turn-based events 完全靜態，由 init / post_action / FDFIELD char spawn records 處理。
