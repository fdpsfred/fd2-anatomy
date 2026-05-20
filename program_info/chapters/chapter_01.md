# 第 1 章 — chapter_01

30 章中唯一含獨家 prologue（3-phase）的 init handler。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_01_init @ 0x0003231B` | 最大 init handler |
| End | `chapter_01_end @ 0x00022EF6` | 65 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default — 無自訂勝負) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[0] @ 0x51E63` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[0] @ 0x51E81` |  |

## Init handler 三階段

### Phase 1 — Prologue (`current_chapter_id = 0x20`)

`init_battle_state_for_chapter` 進入 prologue 模式，開場 cutscene：

- `pan_cursor_and_window(3, 0x22)` 移到 prologue 場景
- `cutscene_event_trigger(99)` 開場音樂 cue (event 0x63)
- `walk_step_up(...) × 0xF` 然後 `display_dialog_scene(page=0)` — 開場走位
- `walk_step_up × 0xD` 然後 `display_dialog_scene(page=1)`
- `set_bgm_track_with_fade(-1, 0)` 停 BGM；`cutscene_event_trigger(100)` (event 0x64)
- `set_bgm_track_with_fade(0xB, 0)` 切 BGM track 11；`play_palette_fade_in`
- 連續 5 段 cutscene + dialog: `0x65→page2`, `0x66→page3`, `0x67→page4`, `0x68→page5`, `0x69`
- 共 6 個 prologue dialog pages (FDTXT entry 33 pages 0..5) + cutscene events 0x65..0x69

### Phase 2 — Chapter 1 Intro (`current_chapter_id = 0x1F`)

`init_battle_state_for_chapter` 切到 intro 模式，FDTXT 切到 entry 32：

- `pan_cursor_and_window(5, 0x2A)` 移到章節舞台
- `load_chapter_portraits_and_dump_tmp(race_id=1)` 載肖像 set 1
- 10 段 cutscene + dialog 序列：events `0x5A..0x62` + pages `0..9`
- 中段 `load_chapter_portraits_and_dump_tmp(race_id=3)` 換肖像 set
- 中段 `mark_char_as_dead(char_idx=2)` 移除某 char (cutscene 中某角色離場)
- 末段 `load_chapter_portraits_and_dump_tmp(race_id=5)` 換肖像 set 3
- 末尾 `set_bgm_track_with_fade(-1, 0)` + `cutscene_event_trigger(0x62)` 結束 intro

### Phase 3 — Set Chapter (`current_chapter_id = 0`)

`init_battle_state_for_chapter` 進入正式戰鬥模式，FDTXT 切到 entry 1：

- `init_runtime_char_from_base_growth(0)` — 索爾
- `init_runtime_char_from_base_growth(9)` — 悠妮 (預初始化，下一步 `mark_char_as_dead(9)` 標未上場)
- `init_runtime_char_from_base_growth(4)` — 亞雷斯
- `init_runtime_char_from_base_growth(0x1E=30)` — 蓋亞
- `pan_cursor_and_window(4, 0xC)` 移到地圖開始位置
- `cutscene_event_trigger(0)` + `display_dialog_scene(page=0)` (entry 1)
- `animate_party_addition_with_appear_effect(slot=1)` + `cutscene_event_trigger(1)`
- `animate_party_addition_with_appear_effect(slot=2)` + `cutscene_event_trigger(2)`
- `display_dialog_scene(page=1)` 對話
- `cutscene_event_trigger(5)` + `mark_char_as_dead(9)` — 悠妮退場
- `display_dialog_scene(page=2)` 結束開場
- `pan_cursor_to_char(0)` 鏡頭聚焦索爾，戰鬥開始
- `party_total_gold = 0` 重置初始金錢

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init Phase 1 (prologue) | 33 | 0, 1, 2, 3, 4, 5 |
| Init Phase 2 (intro) | 32 | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 |
| Init Phase 3 (start) | 1 | 0, 1, 2 |
| End | 1 | 9 |

## char_id 初始化序列

`init_runtime_char_from_base_growth` 呼叫順序：
- char_id 0 → 索爾 (主角)
- char_id 9 → 悠妮 (預初始化即標 dead，ch2+ template 用)
- char_id 4 → 亞雷斯
- char_id 0x1E (30) → 蓋亞 (cutscene NPC，攻略未直接提)

哈諾 (char_id 1) 由 FDFIELD turn-event handler 0x00 在 turn 3 觸發加入，
不在本 init handler 內。

## Cutscene events

該章用到的 `cutscene_event_trigger` 呼叫：
`0x00, 0x01, 0x02, 0x05, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62,
 0x63 (=99), 0x64 (=100), 0x65, 0x66, 0x67, 0x68, 0x69`

- `0x00..0x05` = 普通章節 walk-animation (多章共用)
- `0x5A..0x69` = ch1 prologue/intro 專用 walk-animation
- `0x63=99` 與 `0x64=100` 在 binary 顯示為十進位

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id] @ 0x627D8` 的 walk-animation
script (`[n_groups][group: walk_count|step_count|{char_idx,dir}*N]+`)。

## Post-action handler

`data_fd2_chapter_post_action_handler_table[0]` 指向 `check_battle_end_default_handler`，
無自訂勝負條件：所有 team-0 死 → win，索爾 (char_id 0) 死 → lose。

攻略提到的 reinforcement events 由 FDFIELD event script (turn-event hooks) 處理，
**不是** post_action_handler。

## End handler events

`chapter_01_end @ 0x22EF6` — 最簡單的 end handler 之一：

1. `display_dialog_scene(page=9)` — 第 1 章結局對話 (entry 1 page 9)
2. `save_runtime_char_to_template` — 把 runtime party 狀態存回 template
3. `current_chapter_id = 1` — 推進到第 2 章

無 cutscene、無資源 reload、無加入新角色。

## FDFIELD event script

FDFIELD entry idx **1** (= chapter_id × 3 + 1, chapter_id=0)，entry size 937 bytes。
header `+0..+2` = shap_id_byte / party_count=4 / char_count=30；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

4 / 16 active hooks (其餘 12 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 1 (end_of_player_turn) | 0x00 | `0x000341DB` | 哈諾加入 (`init_runtime_char_from_base_growth(1)` + cutscene 7/8 + dialog 0xb,3) |
| 4 | 0 (enemy_turn_intro) | 0x01 | `0x000342B5` | party slot 4 reveal (cutscene 3 + dialog 4) — 哈瓦特 reveal |
| 5 | 0 (enemy_turn_intro) | 0x02 | `0x0003431D` | party slot 5 reveal (cutscene 4 + dialog 5) — 援軍出場 |
| 6 | 1 (end_of_player_turn) | 0x03 | `0x00034377` | portrait race=6 swap + cutscene 6 + dialog 6 — 場景過場 |

哈瓦特暴走機制：哈諾 (char_id 1) 死後，哈瓦特 protective AI
(`pCombat_aux_block[0xD/E/F]` 由 `init_runtime_char_for_battle` 從 char_spawn_record
+0x94/+0x95/+0x96 複製) 失去 ai_target dependency，自然 fall-through 為 default
attacker。屬 implicit consequence，非 turn-triggered AI flip。
