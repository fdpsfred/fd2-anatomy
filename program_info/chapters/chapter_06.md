# 第 6 章 — chapter_06

30 章中最簡 init 之一 (79 B) — 無 cutscene、無 portrait load、僅 1 頁 dialog。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_06_init @ 0x0003314B` | 79 B |
| End | `chapter_06_end @ 0x00023296` | 82 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[5]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[5]` |  |

## Init handler 階段

極簡：

- `init_battle_state_for_chapter`
- `display_dialog_scene(page=0)`
- `pan_cursor_to_char(0)`

無 cutscene、無 portrait load、無 `pan_cursor_and_window` — 純戰鬥準備章。

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 6 | 0 |
| End | 6 | 6 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。

End handler 開頭 `init_runtime_char_from_base_growth(0xD)` → 貝克威加入。

## Cutscene events

- Init: 無
- End: `0x1B` (1 event)

## Post-action handler

`data_fd2_chapter_post_action_handler_table[5]` 指向 `check_battle_end_default_handler`，
無自訂勝負條件。

## End handler events

`chapter_06_end @ 0x00023296` (82 B)：

1. `init_runtime_char_from_base_growth(0xD)` — char 13 = 貝克威加入
2. `load_chapter_portraits_and_dump_tmp(race_id=3)` 載入加入時的肖像
3. `pan_cursor_and_window(5, 0xE)` + `cutscene_event_trigger(0x1B)`
4. `display_dialog_scene(page=6)`
5. `save_runtime_char_to_template` + `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **16** (= chapter_id × 3 + 1, chapter_id=5)，entry size 1171 bytes。
header `+0..+2` = shap_id_byte / party_count=8 / char_count=40；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

3 / 16 active hooks (其餘 13 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 5 | 2 (new_player_turn_intro) | 0x14 | `0x000347B1` | dialog_only; ch6_dialog |
| 10 | 2 (new_player_turn_intro) | 0x15 | `0x000347D9` | char_conditional; ch6_char_cond |
| 15 | 2 (new_player_turn_intro) | 0x16 | `0x00034819` | char_conditional; ch6_char_cond |
