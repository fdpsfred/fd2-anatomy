# 第 16 章 — chapter_16

蜜蒂三條件招募 (FD2 最複雜的 end conditional)：HP320 AND 18 回合內擊敗 AND 部下死 ≤ 4。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_16_init @ 0x000335A0` | 10 B (stub-tier) |
| End | `fd2_chapter_16_end @ 0x00023A0A` | 341 B |
| Post-action | `fd2_chapter_16_post_action @ 0x0002084A` | default + 額外 lose if char[0x41] dead |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[15]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[15]` |  |

## Init handler 階段

最簡：
1. `fd2_init_battle_state_for_chapter`
2. `fd2_display_dialog_scene(page=0)`
3. `fd2_pan_cursor_to_char(0)`

無 cutscene、無 portrait load、無 char init。

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 16 | 0 |
| End (recruit success) | 16 | 4 |
| End (recruit fail) | 16 | 2, 3 |

## char_id 初始化序列

無 init handler 內 char init。蜜蒂 (char_id 0x12 = 18) 由 end handler 條件式加入。

## Cutscene events

- Init：無
- End：`0x31` (僅在招募失敗分支觸發)

## Post-action handler

`fd2_chapter_16_post_action @ 0x2084A`：
- default 判定 (敵全死=勝、索爾死=負)
- 額外 lose：if `char[0x41]` 死亡 → game_event_flag = 1

`char[0x41]` = 蜜蒂 NPC slot — 蜜蒂在本章以友軍 NPC 自走，戰死即敗。

## End handler events

`fd2_chapter_16_end @ 0x23A0A` (341 B) — 蜜蒂三條件招募：

1. 從 `chapter_16_end_scene_pos_x/y_table` 讀位置
2. `fd2_setup_chars_and_camera_for_intro(0xF, 0x41, 0x1C, 0x1E, 2, 0x16, 0x19)` 配置 chars
3. **Count dead 部下**：迴圈 `chars[0x42..0x49]` (蜜蒂的 8 個部下)，每死一個 `local_14++`
4. 若 `local_14 > 4` → `local_10 = 1` (招募失敗 flag)
5. `fd2_save_runtime_char_to_template`
6. **三條件 AND 招募**：
   - `save_metadata_block < 0x13` (即 18 回合內，TURN 1..18) **AND**
   - `local_10 != 1` (部下死 ≤ 4) **AND**
   - `chars[0].wHP_max > 0x13F` (索爾 HP_max ≥ 320)
   → `fd2_display_dialog_scene(page=4)` + `fd2_init_runtime_char_from_base_growth(0x12=18)` (蜜蒂加入)
7. **Else** (任一條件未達):
   → `fd2_display_dialog_scene(page=2)` + `fd2_cutscene_event_trigger(0x31)` + `fd2_display_dialog_scene(page=3)` (no recruit)
8. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **46** (= chapter_id × 3 + 1, chapter_id=15)，entry size 1691 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=60；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**0/16 active hooks** — 全 16 個 slot 為 sentinel `(turn=0xFF, event_code=0xFF, phase=0)`。本章 turn-based events 完全靜態（由 init / post_action / FDFIELD char spawn records 處理）。
