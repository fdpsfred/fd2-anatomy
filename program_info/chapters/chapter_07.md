# 第 7 章 — chapter_07

含 double-conditional recruit (凱麗) — 需 tile event 觸發 + char[43] 存活雙重條件。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_07_init @ 0x00033169` | 176 B |
| End | `chapter_07_end @ 0x000232E8` | 222 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `per_chapter_player_turn_bgm[6]` |  |
| BGM (enemy turn) | `per_chapter_enemy_turn_bgm[6]` |  |

## Init handler 階段

`init_battle_state_for_chapter` 後依序：

- `display_dialog_scene(page=0)`
- `chapter_init_phase_flag = 1`; `load_chapter_portraits_and_dump_tmp(race_id=1)`; `chapter_init_phase_flag = 0`
- `pan_cursor_and_window(8, 1)` + `cutscene_event_trigger(0x1C)`
- `pan_cursor_and_window(8, 0)` + `cutscene_event_trigger(0x1D)`
- `display_dialog_scene(page=1)` + `pan_cursor_to_char(0)`

`chapter_init_phase_flag` 在 portrait load 期間短暫升起（可能影響 dialog rendering）。

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 7 | 0, 1 |
| End (tile event 0x11 觸發 AND char[43] 活) | 7 | 4 |
| End (其他狀況) | 7 | 5 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。

End handler 條件式 `init_runtime_char_from_base_growth(0xC)` → 凱麗加入（雙條件滿足時）。

## Cutscene events

- Init: `0x1C, 0x1D` (2 events，連續觸發兩 walk animation)

## Post-action handler

`per_chapter_post_action_handler[6]` 指向 `check_battle_end_default_handler`，
無自訂勝負條件。

## End handler events

`chapter_07_end @ 0x000232E8` (222 B) — double-conditional recruit：

1. 從 `chapter_07_end_scene_pos_x_table` / `pos_y_table` / `facing_table`
   (@ 0x520E1/0x520EA/0x520F3) 讀 4 chars 位置
2. `save_runtime_char_to_template`
3. **Conditional 1**：`tile_event_consumed_flags[0x11] == 1` (某 tile event 已觸發)
   - 若是 → **Conditional 2**：`check_char_is_dead(0x2B)` (char 43)
     - 若 char[43] 活著 → `setup_chars_and_camera_for_intro(...)` + `display_dialog_scene(page=4)` +
       `init_runtime_char_from_base_growth(0xC)` — char 12 = 凱麗加入
     - 若 char[43] 已死 → `display_dialog_scene(page=5)` (skip recruit)
   - 若 `tile_event_consumed_flags[0x11] != 1` → `display_dialog_scene(page=5)`
4. `current_chapter_id += 1`

雙重條件：必須觸發過特定 tile event AND 保住 char[43] 才會加入凱麗。

## FDFIELD event script

FDFIELD entry idx **19** (= chapter_id × 3 + 1, chapter_id=6)，entry size 1171 bytes。
header `+0..+2` = shap_id_byte / party_count=9 / char_count=40；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

1 / 16 active hooks (其餘 15 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 10 | 0 (enemy_turn_intro) | 0x19 | `0x00034924` | first_time_gated; ch7_first_time |
