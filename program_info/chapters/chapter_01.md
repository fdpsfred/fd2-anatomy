# 第 1 章 — chapter_01

30 章中唯一含獨家 prologue（分 Phase A–D 四段）的 init handler。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_01_init @ 0x0003231B` | 最大 init handler |
| End | `fd2_chapter_01_end @ 0x00022EF6` | 65 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default — 無自訂勝負) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[0] @ 0x51E63` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[0] @ 0x51E81` |  |

## Init handler（Phase A–D 四段）

依 `current_chapter_id` 三次寫入（0x20 / 0x1F / 0）分三個地圖階段；src 進一步把 map 0x20 段以
state=1 過場 cutscene 0x64 為界拆成 Phase A、Phase B。對應關係：舊三段分法的 Phase 1 = A + B。

### Phase A — Prologue 地圖 1（`current_chapter_id = 0x20`）

`fd2_init_battle_state_for_chapter` 進入 prologue 模式：

- `fd2_pan_cursor_and_window(3, 0x22)`
- `fd2_cutscene_event_trigger(0x63=99)`
- `walk_step_up × 0xF` 然後 `fd2_display_dialog_scene(page=0)`
- `walk_step_up × 0xD` 然後 `fd2_display_dialog_scene(page=1)`
- `fd2_set_bgm_track_with_fade(-1, 0)` 停 BGM
- `cutscene_event_state=1`；`fd2_cutscene_event_trigger(0x64=100)` 過場

### Phase B — 仍在 map 0x20（不重設 chapter_id、不重跑 battle-state init）

- `fd2_pan_cursor_and_window(0, 0x2B)`
- `fd2_set_bgm_track_with_fade(0xB, 0)` 切 BGM track 11
- `fd2_play_palette_fade_in`
- 4 段 cutscene + dialog：`0x65→page2`、`0x66→page3`、`0x67→page4`、`0x68→page5`
- `cutscene_event_state=1`；`fd2_cutscene_event_trigger(0x69)` 過場
- Phase A+B 共用 6 個 prologue dialog pages（FDTXT entry 33 pages 0..5）

### Phase C — Prologue 地圖 2（`current_chapter_id = 0x1F`）

`fd2_init_battle_state_for_chapter` 切到第二段 prologue 地圖，FDTXT 切到 entry 32：

- `fd2_pan_cursor_and_window(5, 0x2A)`
- `fd2_load_chapter_portraits_and_dump_tmp(1)` 載肖像 set 1
- cutscene events `0x5A..0x61` 配 dialog pages `0..9`
- 中段 `fd2_load_chapter_portraits_and_dump_tmp(3)` 換肖像、`fd2_pan_cursor_and_window(4, 0x29)`
- 中段 `fd2_mark_char_as_dead(2)` 移除 cutscene 角色
- 末段 `fd2_load_chapter_portraits_and_dump_tmp(5)` 換肖像 set
- `fd2_set_bgm_track_with_fade(-1, 0)` 停 BGM
- `cutscene_event_state=1`；`fd2_cutscene_event_trigger(0x62)` 結束 intro

### Phase D — 第 1 章正式戰鬥（`current_chapter_id = 0`）

先初始化 4 個 runtime char，**再**呼叫 `fd2_init_battle_state_for_chapter`（進入正式戰鬥模式、
FDTXT 切到 entry 1）：

- `fd2_init_runtime_char_from_base_growth(0)` — 索爾
- `fd2_init_runtime_char_from_base_growth(9)` — 悠妮（預初始化，稍後 `fd2_mark_char_as_dead(9)` 標未上場）
- `fd2_init_runtime_char_from_base_growth(4)` — 亞雷斯
- `fd2_init_runtime_char_from_base_growth(0x1E=30)` — 蓋亞
- `fd2_init_battle_state_for_chapter`
- `fd2_pan_cursor_and_window(4, 0xC)`
- `fd2_cutscene_event_trigger(0)` + `fd2_display_dialog_scene(page=0)`（entry 1）
- `fd2_animate_party_addition_with_appear_effect(1)` + `fd2_cutscene_event_trigger(1)`
- `fd2_animate_party_addition_with_appear_effect(2)` + `fd2_cutscene_event_trigger(2)`
- `fd2_display_dialog_scene(page=1)`
- `fd2_cutscene_event_trigger(5)` + `fd2_mark_char_as_dead(9)` — 悠妮退場
- `fd2_composite_battle_frame(0)`；`fd2_display_dialog_scene(page=2)`
- `fd2_clear_all_chars_facing`；`fd2_pan_cursor_to_char(0)` 鏡頭聚焦索爾
- `party_total_gold = 0`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Phase A+B (prologue) | 33 | 0, 1, 2, 3, 4, 5 |
| Phase C (prologue 地圖 2) | 32 | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 |
| Phase D (正式戰鬥) | 1 | 0, 1, 2 |
| End | 1 | 9 |

## char_id 初始化序列

`fd2_init_runtime_char_from_base_growth` 呼叫順序：
- char_id 0 → 索爾 (主角)
- char_id 9 → 悠妮 (預初始化即標 dead，ch2+ template 用)
- char_id 4 → 亞雷斯
- char_id 0x1E (30) → 蓋亞 (cutscene NPC，攻略未直接提)

哈諾 (char_id 1) 由 FDFIELD turn-event handler 0x00 在 turn 3 觸發加入，
不在本 init handler 內。

## Cutscene events

該章用到的 `fd2_cutscene_event_trigger` 呼叫：
`0x00, 0x01, 0x02, 0x05, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62,
 0x63 (=99), 0x64 (=100), 0x65, 0x66, 0x67, 0x68, 0x69`

- `0x00..0x05` = 普通章節 walk-animation (多章共用)
- `0x5A..0x69` = ch1 prologue/intro 專用 walk-animation
- `0x63=99` 與 `0x64=100` 在 binary 顯示為十進位

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id] @ 0x627D8` 的 walk-animation
script (`[n_groups][group: walk_count|step_count|{char_idx,dir}*N]+`)。

## Post-action handler

`data_fd2_chapter_post_action_handler_table[0]` 指向 `fd2_check_battle_end_default_handler`，
無自訂勝負條件：所有 team-0 死 → win，索爾 (char_id 0) 死 → lose。

攻略提到的 reinforcement events 由 FDFIELD event script (turn-event hooks) 處理，
**不是** post_action_handler。

## End handler events

`fd2_chapter_01_end @ 0x22EF6` — 最簡單的 end handler 之一：

1. `fd2_display_dialog_scene(page=9)` — 第 1 章結局對話 (entry 1 page 9)
2. `fd2_save_runtime_char_to_template` — 把 runtime party 狀態存回 template
3. `current_chapter_id = 1` — 推進到第 2 章

無 cutscene、無資源 reload、無加入新角色。

## FDFIELD event script

FDFIELD entry idx **1** (= chapter_id × 3 + 1, chapter_id=0)，entry size 937 bytes。
header `+0..+2` = shap_id_byte / party_count=4 / char_count=30；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

4 / 16 active hooks (其餘 12 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 1 (end_of_player_turn) | 0x00 | `0x000341DB` | 哈諾加入 (`fd2_init_runtime_char_from_base_growth(1)` + cutscene 7/8 + dialog 0xb,3) |
| 4 | 0 (enemy_turn_intro) | 0x01 | `0x000342B5` | party slot 4 reveal (cutscene 3 + dialog 4) — 哈瓦特 reveal |
| 5 | 0 (enemy_turn_intro) | 0x02 | `0x0003431D` | party slot 5 reveal (cutscene 4 + dialog 5) — 援軍出場 |
| 6 | 1 (end_of_player_turn) | 0x03 | `0x00034377` | portrait race=6 swap + cutscene 6 + dialog 6 — 場景過場 |

哈瓦特暴走機制：哈諾 (char_id 1) 死後，哈瓦特 protective AI
(`pCombat_aux_block[0xD/E/F]`，即 runtime_char +0x34/+0x35/+0x36，由
`fd2_init_runtime_char_for_battle` 從 char_spawn_record +0x11/+0x12/+0x13
（ai_class_flags / ai_aux / ai_target_pos）複製) 失去 ai_target dependency，自然
fall-through 為 default attacker。屬 implicit consequence，非 turn-triggered AI flip。
