# 第 2 章 — chapter_02

含 villager-survival 條件式 end handler 與 hidden item reward。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_02_init @ 0x00032D18` | 402 B |
| End | `fd2_chapter_02_end @ 0x00022F37` | 443 B |
| Post-action | `fd2_chapter_02_post_action @ 0x000206C5` | 自訂 — 額外 lose if 任一 chars[5..10] 死 |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[1]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[1]` |  |

## Init handler 階段

`fd2_init_battle_state_for_chapter` 進入正式戰鬥模式後，依序：

- `fd2_pan_cursor_and_window(0xD, 0xB)` 移到場景
- `fd2_cutscene_event_trigger(9)` + `fd2_display_dialog_scene(page=0)` 第一段對話
- `fd2_cutscene_event_trigger(0xA)` + `fd2_display_dialog_scene(page=1)`
- `fd2_load_chapter_portraits_and_dump_tmp(race_id=1)` 載肖像 set 1
- `fd2_cutscene_event_trigger(0xB)` + `fd2_display_dialog_scene(page=2)`
- `fd2_pan_cursor_and_window(6, 0xC)` 鏡頭移
- `chapter_init_phase_flag = 1` (transient) + `fd2_load_chapter_portraits_and_dump_tmp(race_id=2)` 換肖像
- `fd2_cutscene_event_trigger(0xC)` + `fd2_display_dialog_scene(page=3)`
- `fd2_pan_cursor_to_char(0)` 鏡頭聚焦索爾，戰鬥開始

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 2 | 0, 1, 2, 3 |
| End (villager 全活) | 2 | 6, 8, 9, 10 |
| End (有 villager 死) | 2 | 7, 8, 9, 10 |

## char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫 — 第 2 章不在 init 加入新隊員。

End handler 末段 `fd2_init_runtime_char_from_base_growth(8)` → 希莉亞加入。

## Cutscene events

該章用到的 `fd2_cutscene_event_trigger` 呼叫：

- Init: `0x09, 0x0A, 0x0B, 0x0C` (4 events)
- End: `0x0E, 0x0F, 0x10` (3 events)

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id]` 的 walk-animation script。

## Post-action handler

`fd2_chapter_02_post_action @ 0x000206C5`：

- 標準 default 判定（所有敵死=勝、索爾死=負）
- **額外 lose 條件**：if 任一 chars[5..10] (6 個 villager NPC) 死亡 → `game_event_flag = 1`

對應失敗條件「索爾死亡，村民全滅」——chars[5..10] = 6 個村民 NPC（男×3 + 女×3）。

## End handler events

`fd2_chapter_02_end @ 0x00022F37` (443 B) — villager survival branching：

1. 檢查 chars[5..10] (6 villagers)：迴圈設 `bVar = 1` if 任一死亡
2. **Conditional**：
   - 若全活 (`bVar == 0`) → `fd2_display_dialog_scene(page=6)` + `fd2_give_item_to_first_player_char(0xC6)` (item 198 = 力量藥水, AP+9)
   - 若有人死 (`bVar == 1`) → `fd2_display_dialog_scene(page=7)` (alternate ending，無 reward)
3. `fd2_pan_cursor_and_window(0xE, 2)` + `fd2_load_chapter_portraits_and_dump_tmp(4)`
4. `fd2_cutscene_event_trigger(0xE)` + `fd2_display_dialog_scene(page=8)`
5. `fd2_cutscene_event_trigger(0xF)` + `fd2_display_dialog_scene(page=9)`
6. `fd2_pan_cursor_and_window(0xE, 1)` + `fd2_cutscene_event_trigger(0x10)` + `fd2_display_dialog_scene(page=10)`
7. `fd2_init_runtime_char_from_base_growth(8)` — char 8 = 希莉亞加入
8. `fd2_save_runtime_char_to_template` + `current_chapter_id = 2`

## FDFIELD event script

FDFIELD entry idx **4** (= chapter_id × 3 + 1, chapter_id=1)，entry size 1171 bytes。
header `+0..+2` = shap_id_byte / party_count=5 / char_count=40；`+3..+50` = 16×3
turn-event hooks；`+51..` = char_spawn_records。

1 / 16 active hooks (其餘 15 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 1 (end_of_player_turn) | 0x06 | `0x00034422` | reinforcement_spawner; ch2_reinforcement |
