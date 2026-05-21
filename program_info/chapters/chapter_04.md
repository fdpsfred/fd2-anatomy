# 第 4 章 — chapter_04

End handler 為極小 (61 B) 的 trivial 結算 — 純對話 + 推進。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_04_init @ 0x00032FB2` | 181 B |
| End | `fd2_chapter_04_end @ 0x000231BC` | 61 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[3]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[3]` |  |

## Init handler 階段

`fd2_init_battle_state_for_chapter` 後依序：

- `fd2_pan_cursor_and_window(4, 0xB)`
- `fd2_cutscene_event_trigger(0x14)` + `fd2_display_dialog_scene(page=0)`
- `fd2_load_chapter_portraits_and_dump_tmp(race_id=1)` + `fd2_pan_cursor_and_window(4, 0)`
- `fd2_display_dialog_scene(page=1)` + `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 4 | 0, 1 |
| End | 4 | 4 |

## char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫（init/end 皆無）。

## Cutscene events

- Init: `0x14` (1 event)

## Post-action handler

`data_fd2_chapter_post_action_handler_table[3]` 指向 `fd2_check_battle_end_default_handler`，
無自訂勝負條件。

## End handler events

`fd2_chapter_04_end @ 0x000231BC` (61 B) — trivial：

1. `fd2_display_dialog_scene(page=4)`
2. `fd2_save_runtime_char_to_template`
3. `current_chapter_id += 1`

無 cutscene、無 reward、無加入。

## FDFIELD event script

FDFIELD entry idx **10** (= chapter_id × 3 + 1, chapter_id=3)，entry size 1171 bytes。
header `+0..+2` = shap_id_byte / party_count=7 / char_count=40；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

1 / 16 active hooks (其餘 15 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 4 | 1 (end_of_player_turn) | 0x0B | `0x00034565` | dialog_only; ch4_dialog |
