# 第 27 章 — chapter_27

GOOD/BAD ENDING 分歧點：end handler 用 `fd2_any_char_has_item(100)` (天空之鑰) 決定進 ch28+ 或直接 game ending cinematic + infinite loop。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_27_init @ 0x00033AF1` | 428 B (第二大 init) |
| End | `fd2_chapter_27_end @ 0x000250CC` | 920 B |
| Post-action | `fd2_chapter_22_27_28_post_action_shared @ 0x00020A87` | default + lose if char[1] dead (與 ch22/28 共用) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[26]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[26]` |  |

## Init handler 階段

含 item-conditional dialog branching + 3 次小範圍 spell cast cinematic：

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(9, 0x31)` + `fd2_cutscene_event_trigger(0x4C)`
3. `fd2_display_dialog_scene(page=0)`
4. **Conditional**：`fd2_any_char_has_item(100)` (天空之鑰)
   - 找到 → `fd2_display_dialog_scene(page=3)` 額外對話
   - 沒找到 → 跳過
5. `fd2_display_dialog_scene(page=4)`
6. `fd2_pan_cursor_and_window(9, 0x31)` + `fd2_cast_screen_wide_spell_with_fade(cursor_x, cursor_y+3, 2, 2)`
7. palette restore + `fd2_display_dialog_scene(page=5)`
8. `fd2_cast_screen_wide_spell_with_fade(cursor_x, cursor_y, 2, 2)` + palette + `fd2_cutscene_event_trigger(0x51)` + `fd2_display_dialog_scene(page=6)`
9. `fd2_cast_screen_wide_spell_with_fade(cursor_x+2, cursor_y, 2, 2)` + palette + `fd2_display_dialog_scene(page=7)`
10. `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init (固定) | 27 | 0, 4, 5, 6, 7 |
| Init (有天空之鑰) | 27 | 3 (插在 page 0 與 page 4 之間) |
| End (固定) | 27 | 8 |
| End GOOD (有天空之鑰) | 27 | 9, 10, 0xB, 0xC |
| End BAD (無天空之鑰) | 27 | 0xD, 0xE, 0xF, 0x10 |

## char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫；本章不加入新角色，兩條 ending 路徑皆無加入。

## Cutscene events

- Init: `0x4C, 0x51`
- End GOOD: `0x52, 0x53, 0x54`
- End BAD: `0x52` (×2), `0x54`

## Post-action handler

`fd2_chapter_22_27_28_post_action_shared @ 0x00020A87` (與 ch22/28 共用)：
- 標準 default 判定（全敵死 = 勝、索爾死 = 負）
- **額外 lose 條件**：if chars[1] dead → `game_event_flag = 1`

本章 chars[1] = 悠妮。

## End handler events

`fd2_chapter_27_end @ 0x000250CC` (920 B) — GOOD/BAD ENDING fork：

1. 從 `chapter_27_end_scene_pos_x/y_table` 讀位置 (含 1-byte vestigial facing)
2. **Revive all**：`chars[0..0xF].bFlags = 0` (16 chars 復活)
3. `fd2_setup_chars_and_camera_for_intro(0xF, 0, 9, 8)`
4. `fd2_display_dialog_scene(page=8)` + `fd2_cutscene_event_trigger(0x52)`
5. **GOOD/BAD FORK**：`fd2_any_char_has_item(100)` (天空之鑰)

### GOOD PATH (有天空之鑰) — 進 ch28+
- `fd2_display_dialog_scene(page=9)` + `fd2_cutscene_event_trigger(0x53)`
- `fd2_display_dialog_scene(page=10)` + `pan` + `fd2_cutscene_event_trigger(0x54)`
- `fd2_display_dialog_scene(page=0xB)`
- **6× `fd2_palette_fade_to_black_step_loop`**：(0x50,5) / (0x50,4) / (0x50,3) / (0x50,2) / (0x50,2) / (0x50,2) + 不同等待時間
- `fd2_display_dialog_scene(page=0xC)`
- `fd2_cast_screen_wide_spell_with_fade` (大範圍)
- 500ms wait + `memset(0xA0000, 0xFF, 64000)` (白屏) + `fd2_play_palette_fade_to_black` + `memset(0xA0000, 0, 64000)` (黑屏)
- `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`
- `fd2_restore_all_chars_full_hp_mp`
- `return` ← 正常 return 進入 chapter 28

### BAD PATH (無天空之鑰) — Bad ending
- `fd2_display_dialog_scene(page=0xD)` + `fd2_cutscene_event_trigger(0x54)`
- `fd2_display_dialog_scene(page=0xE)` + `fd2_cutscene_event_trigger(0x52)`
- `fd2_display_dialog_scene(page=0xF)`
- `fd2_animate_status_effect_overlay_flicker(0, 0x13, 1)` — chars[1] 悠妮 status effect 閃爍
- `fd2_animate_warp_teleport_char(1, 0xFF, 0xFF, ...)` — char[1] 悠妮 teleport away
- `fd2_display_dialog_scene(page=0x10)`
- `fd2_restore_all_chars_full_hp_mp`
- `fd2_play_game_ending_cinematic`
- **infinite loop** — 程式鎖死於此

## FDFIELD event script

FDFIELD entry idx **79** (= chapter_id × 3 + 1, chapter_id=26)，entry size 2211 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=80；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

2 / 16 active hooks (其餘 14 為 sentinel 在初始狀態)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0 (enemy_turn_intro) | 0x3F | `0x000358C7` | sentinel-like (final boss event reference) |
| 0xFF | 0 (enemy_turn_intro) | 0x41 | `0x0003599B` | sentinel-like |

`turn=0xFF` 不會等於回合計數，初始 hook table 不會 fire。實際觸發機制：tile-step-event handler 在某些劇本 tile 被踩到時，會動態 rewrite 該 chapter turn-event-hook table 的 turn byte (0xFF → current_save_metadata_block / +1)，把原本 sentinel 的 entry 啟動為下一回合 fire 的 event。形成 cinematic chain：tile-step → handler 寫入 turn = N 或 N+1 → 該回合 turn-event 自動 fire → 連鎖播放劇情。

對應「若到達石碑下方階梯則下回合敵援軍」屬此類 tile-step → turn-event 連鎖。
