# 第 20 章 — chapter_20

共用 `chapter_19_20_21_init_shared`；post_action 為 250 B 三段式 (FD2 最大 non-default post_action)；end handler 含達可塞 (char_id 0x1C = 28) 限 15 回合內 conditional 招募。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_19_20_21_init_shared @ 0x00033674` | 10 B (與 ch19/21 共用) |
| End | `fd2_chapter_20_end @ 0x00023E74` | 646 B |
| Post-action | `fd2_chapter_20_post_action @ 0x00020957` | 250 B (FD2 最大 non-default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[19]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[19]` |  |

## Init handler 階段

Shared minimal init (同 ch19)：
1. `fd2_init_battle_state_for_chapter`
2. `fd2_display_dialog_scene(page=0)`
3. `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 20 | 0 |
| Post-action (精靈全滅) | 20 | 10 |
| End (always) | 20 | 0xB, 0xC, 0xD |
| End (達可塞分支) | 20 | 0xE, 0xF, 0x10 |

## char_id 初始化序列

無 init handler 內 char init。謝多 (char_id 0x19 = 25) 與達可塞 (char_id 0x1C = 28) 由 end handler 加入；達可塞為條件式。

## Cutscene events

- Init / Post-action：無 cutscene (post_action 內僅 trigger dialog page 10)
- End (always)：`0x3B`
- End (達可塞分支)：`0x3C`, `0x3D`, `0x3E`

## Post-action handler

`fd2_chapter_20_post_action @ 0x20957` (250 B) — 三段式邏輯：

1. **default 判定先跑**：敵全死=勝、索爾死=負
2. **Stage A — 精靈 group**：若 `chars[0x35..0x3D]` (8 個 NPC = 精靈) 全死 → `fd2_display_dialog_scene(page=10)` + `game_event_flag = 1` (lose)
3. **Stage B — 主角組**：if `char[0]` OR `char[0x34]` 死亡 → `game_event_flag = 1` (lose)
4. **Stage C — 兩 group win 判定**：if `chars[0x24..0x33]` + `chars[0x3D..0x53]` 兩組敵 chars 全部死亡 → `game_event_flag = 2` (win)。Stage C 為 ch20 真正的勝利條件 (對應「沼澤怪物之外的敵人全滅」)。

| char slot | 角色 |
|---|---|
| char[0] | 索爾 |
| char[0x34] | 忍者謝多 (NPC) |
| chars[0x35..0x3D] | 精靈族 8 名 (NPC) |
| chars[0x24..0x33] | 沼澤怪物 / 死亡骷髏 group A |
| chars[0x3D..0x53] | group B |

## End handler events

`fd2_chapter_20_end @ 0x23E74` (646 B)：

1. 從 `chapter_20_end_scene1_pos_x/y_table` (chars 0..0xF) + `chapter_20_end_scene2_pos_x/y_table` (chars 0x34..0x3C) 讀位置
2. `fd2_play_palette_fade_to_black` + `fd2_clear_all_chars_acted_flag`
3. **Reposition 25 chars**：
   - chars[0..0xF] (16 chars)：從 scene1 設 bPos + sprite facing = 1 (west)
   - chars[0x34..0x3C] (9 chars)：從 scene2 設 bPos + sprite facing = 3 (east)
4. Reset battle camera (battle_window_origin = 0x1A/0x1F, cursor reset)
5. composite + fade + 200ms
6. `fd2_display_dialog_scene(page=0xB)` + `fd2_cutscene_event_trigger(0x3B)` + `fd2_display_dialog_scene(page=0xC)`
7. `fd2_init_runtime_char_from_base_growth(0x19=25)` (謝多 — always)
8. `fd2_save_runtime_char_to_template`
9. **Conditional 達可塞招募**：if `save_metadata_block < 0x10` (16 回合內，TURN 1..15)：
   - `fd2_load_chapter_portraits_and_dump_tmp(1)` + `fd2_cutscene_event_trigger(0x3C)` + `fd2_display_dialog_scene(page=0xE)`
   - `fd2_cutscene_event_trigger(0x3D)` + `fd2_display_dialog_scene(page=0xF)`
   - `fd2_cutscene_event_trigger(0x3E)` + `fd2_display_dialog_scene(page=0x10)`
   - `fd2_init_runtime_char_from_base_growth(0x1C=28)` (達可塞)
10. `fd2_display_dialog_scene(page=0xD)` (always)
11. `current_chapter_id += 1`

`save_metadata_block` 是螢幕「TURN N」顯示值 (1 起算)；`< 16` = TURN 1..15 = 「15 回合內結束」。

## FDFIELD event script

FDFIELD entry idx **58** (= chapter_id × 3 + 1, chapter_id=19)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**0/16 active hooks** — 全 16 個 slot 為 sentinel `(turn=0xFF, event_code=0xFF, phase=0)`。本章 turn-based events 完全靜態 (由 init / post_action / FDFIELD char spawn records 處理)。
