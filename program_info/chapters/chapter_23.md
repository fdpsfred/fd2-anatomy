# 第 23 章 — chapter_23

30 章中最大 init (548 B) 與最大 end (960 B)；end 含 3 個 conditional join (天空之鑰 → 卡里斯；蜜蒂未在 + < 15 回合 → 羅德曼；蜜蒂在或錯過 → mark char[0x11] dead)，並在中段 `fd2_load_dat_resource("FDFIELD.DAT", 0x45)` reload 進入第二戰場。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_23_init @ 0x000336A0` | 548 B (最大 init) |
| End | `fd2_chapter_23_end @ 0x00024754` | 960 B (最大 end) |
| Post-action | `fd2_chapter_23_post_action @ 0x00020AAF` | bypass default |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[22]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[22]` |  |

## Init handler 階段

開場 cinematic 規模最大：含 screen-wide spell visual + 16-char 全清隊 → revive HP>0 過濾。

1. `fd2_init_battle_state_for_chapter`
2. **大規模清隊**：`for i in [0, 0x10): fd2_mark_char_as_dead(i)` (16 chars 全標 dead)
3. `fd2_pan_cursor_and_window(0xE, 0x20)`
4. `fd2_cast_screen_wide_spell_with_fade(cursor_x+6, cursor_y+5, 10, 8)` — screen-wide cinematic spell visual
5. **revive 過濾**：`for i in [0, 0x10): if chars[i].wHP_current != 0: chars[i].bFlags = 0; chars[i].pSprite_state[1] = 2` (face north) — 只復活原本 HP>0 的角色
6. `fd2_composite_battle_frame` + `fd2_set_vga_palette_range_with_add(0, 0xFF, 0)` 重繪
7. `fd2_display_dialog_scene(page=0)`
8. `fd2_pan_cursor_and_window(0xE, 0x1D)` + `fd2_cutscene_event_trigger(0x44)`
9. `fd2_display_dialog_scene(page=1)` + `fd2_cutscene_event_trigger(0x45)`
10. `fd2_display_dialog_scene(page=2)` + `fd2_cutscene_event_trigger(0x46)`
11. `fd2_display_dialog_scene(page=3)`
12. `fd2_pan_cursor_and_window(0xE, 0xD)` + `fd2_load_chapter_portraits_and_dump_tmp(1)`
13. **Palette transition**：`fd2_set_vga_palette_range_with_add(0, 0xFF, 0xFF)` (white-out) → composite → palette restore
14. `chars[0x10].pSprite_state[1] = 2`、`chars[0x11].pSprite_state[1] = 2` (face north)
15. `fd2_display_dialog_scene(page=4)` + `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 23 | 0, 1, 2, 3, 4 |
| End Phase 1 (有天空之鑰) | 23 | 8 |
| End Phase 1 (無天空之鑰) | 23 | 9 |
| End Phase 1 (蜜蒂在 template) | 23 | 0xA, 0xB |
| End Phase 1 (< 15 回合 + 蜜蒂未在) | 23 | 0xD |
| End Phase 1 (≥ 15 回合 + 蜜蒂未在) | 23 | 0xC |
| End Phase 2 (進第二戰場) | 23 | 0xE, 0xF, 0x10, 0x11 |

## char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 中 conditional 加入：
- char_id 22 (0x16) → 武聖卡里斯 — 若 `fd2_any_char_has_item(100)` (天空之鑰)
- char_id 19 (0x13) → 劍聖羅德曼 — 若蜜蒂 (template id 0x12) 未在 template 且 `save_metadata_block < 0xF` (< 15 回合)

End handler conditional `fd2_mark_char_as_dead(0x11)`：若蜜蒂在 template，或蜜蒂未在但 ≥ 15 回合。

## Cutscene events

- Init: `0x44, 0x45, 0x46`
- End: `0x47` (若無天空之鑰) 或 `0x48` (若蜜蒂在或錯過 15 回合)；`0x49` × 3 (進入第二戰場)

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id]` 的 walk-animation script。

## Post-action handler

`fd2_chapter_23_post_action @ 0x00020AAF` — bypass default：

- 若 chars[0]、chars[1]、chars[0x10]、chars[0x11] 任一死 → `game_event_flag = 1` (lose)
- 若 chars[0x12] (機甲隊長) 死 → `game_event_flag = 2` (win)

char[0]=索爾、char[1]=希爾法、char[0x10]=卡里斯、char[0x11]=羅德曼、char[0x12]=機甲隊長 boss。

## End handler events

`fd2_chapter_23_end @ 0x00024754` (960 B) — 含 3 重 conditional + scene reload 進入第二戰場：

### Phase 1 — 加入判定

1. 從 `chapter_23_end_scene_pos_x/y/facing_table` 讀位置
2. `fd2_setup_chars_and_camera_for_intro(0x10, 0x11, 0x15, 0x15, 2, 0xE, 0xE)` 配置
3. **Conditional 1**：`fd2_any_char_has_item(100)` (天空之鑰)
   - 有 → `fd2_display_dialog_scene(page=8)` + `fd2_init_runtime_char_from_base_growth(0x16)` (卡里斯加入)
   - 無 → `fd2_display_dialog_scene(page=9)` + `fd2_cutscene_event_trigger(0x47)`
4. **Conditional 2**：`fd2_find_template_char_by_id(0x12)` (蜜蒂在 template?)
   - 蜜蒂在 → `fd2_display_dialog_scene(page=0xA)` + `fd2_cutscene_event_trigger(0x48)` + `fd2_mark_char_as_dead(0x11)` + `fd2_display_dialog_scene(page=0xB)`
   - 蜜蒂未在 → 子條件 `save_metadata_block < 0xF` (< 15 回合)
     - < 15 回合 → `fd2_display_dialog_scene(page=0xD)` + `fd2_init_runtime_char_from_base_growth(0x13)` (羅德曼加入)
     - ≥ 15 回合 → `fd2_display_dialog_scene(page=0xC)` + `fd2_cutscene_event_trigger(0x48)` + `fd2_mark_char_as_dead(0x11)`
5. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

### Phase 2 — 進入第二戰場 cinematic

6. `fd2_display_dialog_scene(page=0xE)` + 400ms + `fd2_play_rising_pre_cast_effect(1, 0xF, 10)` + `fd2_animate_screen_shake(0x1E)` (第 1 次)
7. `fd2_display_dialog_scene(page=0xF)` + 400ms + `fd2_play_rising_pre_cast_effect` + `fd2_animate_screen_shake(0x1E)` (第 2 次)
8. `fd2_display_dialog_scene(page=0x10)` + 400ms + `fd2_play_rising_pre_cast_effect(1, 0x1E, 0x10)` (第 3 次，更大範圍)
9. **64-step palette fade-out** (`for i in [0..0x40) step 2`)
10. **Reload battle scene**：
    - `fd2_load_dat_resource("FDFIELD.DAT", 0x45)` → `battle_tile_map` (新地圖)
    - `fd2_load_dat_resource("FDSHAP.DAT", 0x2E)` → `data_fd2_battle_scene_tile_gfx_ptr`
    - `fd2_load_dat_resource("FDSHAP.DAT", 0x2F)` → `tile_attribute_flags_buffer`
    - `fd2_battle_reset_tile_transient_state(battle_tile_map)`
    - `fd2_load_chapter_background_layers`
11. `pan` + palette restore + `fd2_cutscene_event_trigger(0x49)` + `pan` + `fd2_cutscene_event_trigger(0x49)` × 2
12. `fd2_display_dialog_scene(page=0x11)`

第二戰場使用同 `current_chapter_id`，不增 chapter；end 結束時才推進。

## FDFIELD event script

FDFIELD entry idx **67** (= chapter_id × 3 + 1, chapter_id=22)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

4 / 16 active hooks (其餘 12 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 13 | 0 (enemy_turn_intro) | 0x34 | `0x000352E2` | 援軍 reinforcement (含 mid-handler chapter reload context) |
| 15 | 0 (enemy_turn_intro) | 0x34 | `0x000352E2` | 援軍 reinforcement |
| 18 | 0 (enemy_turn_intro) | 0x34 | `0x000352E2` | 援軍 reinforcement |
| 22 | 0 (enemy_turn_intro) | 0x34 | `0x000352E2` | 援軍 reinforcement |
