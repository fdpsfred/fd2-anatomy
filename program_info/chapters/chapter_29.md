# 第 29 章 — chapter_29

唯一用 `tile_event_consumed_flags` 而非 char 死活作勝利判定的章節；end handler 含 9 連震 + 3 道白光 + 64+64 step palette fade + char[0x14] 變身為空魔神 (portrait_id 0x7E)。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_29_init @ 0x00033DBA` | 130 B |
| End | `chapter_29_end @ 0x0002548C` | 451 B |
| Post-action | `chapter_29_post_action @ 0x00020B72` | bypass default; tile_event_consumed_flags-based win |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[28]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[28]` |  |

## Init handler 階段

1. `init_battle_state_for_chapter`
2. `pan_cursor_and_window(9, 0x38)` + `cutscene_event_trigger(0x56)`
3. `display_dialog_scene(page=7)` (從 page 7 開始，非 page 0 — 延續 ch28 結尾劇情)
4. `FUN_00035822(9, 0x13, 8)` — char placement helper
5. `display_dialog_scene(page=8)` + `pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 29 | 7, 8 |
| Post-action (chars[1] 死) | 29 | 9 |
| End | 29 | 10, 0xB, 0xC, 0xD, 0xE, 0xF (6 pages) |

Page 0..6 由 FDFIELD turn-event / tile-step handler 引用（含護送悠妮到控制中心石碑、3 條巨龍對話等）。

## char_id 初始化序列

無 `init_runtime_char_from_base_growth` 呼叫；本章不加入新角色。

End handler 執行 char 變身：`chars[0x14].bPortrait_id = 0x7E` + `chars[0x14].bChar_id = 0x7E` → 變身為**空魔神**（data_fd2_battle_enemy_data_table entry 58 @ 0x7AD51）。

## Cutscene events

- Init: `0x56`
- End: 無 `cutscene_event_trigger`（純 cinematic helpers）

## Post-action handler

`chapter_29_post_action @ 0x00020B72` — bypass default：

- if `tile_event_consumed_flags[0x12]` AND `[0x13]` AND `[0x14]` 全部已觸發 → `game_event_flag = 2` (win — 解除防衛系統)
- if chars[0] dead → `game_event_flag = 1` (lose)
- if chars[1] dead → `display_dialog_scene(page=9)` + `game_event_flag = 1`

FD2 唯一用 `tile_event_consumed_flags` 而非 char 死活作勝利判定的章節。對應「解除防衛系統」= 觸發 3 個 tile_event (`0x12/0x13/0x14`)。

## End handler events

`chapter_29_end @ 0x0002548C` (451 B) — 大 cinematic + char 變身 + 9 連震 + 3 白光：

1. `display_dialog_scene(page=10)`
2. `FUN_00035bba(0x14)` — char[0x14] 操作 helper
3. **char[0x14] 變身為空魔神**：
   - `chars[0x14].bPortrait_id = 0x7E`
   - `chars[0x14].bChar_id = 0x7E`
   - char_id 0x7E 屬 enemy class (>= 0x44) → data_fd2_battle_enemy_data_table entry (0x7E - 0x44) / 10 = entry 58 → 位址 0x7AB0D + 58 × 10 = `0x7AD51` (空魔神)
4. `display_dialog_scene(page=0xB)`
5. `load_chapter_portraits_and_dump_tmp(9)` + `pan` + `pan_cursor_to_tile_animated(0xF, 10)`
6. `animate_warp_teleport_char(party_member_count - 1, 0xF, 0xA, 0xF, 0xA)` — 傳送最後 party char (warp 動畫)
7. `display_dialog_scene(page=0xC)`
8. **第 1 輪 3 連震**：`animate_screen_shake(0x14)` × 3 (各 600ms)
9. `display_dialog_scene(page=0xD)`
10. **第 2 輪 3 連震**：`animate_screen_shake(0x14)` × 3 (各 200ms)
11. `display_dialog_scene(page=0xE)`
12. **第 3 輪 climax**：`animate_screen_shake(0x14)` + 200ms + `animate_screen_shake(0x14)` + 100ms + `animate_screen_shake(0x28)` (3 倍長度) + 200ms
13. **3 道白光閃**：`animate_palette_flash_pulse_white` × 3 (各 300ms)
14. `display_dialog_scene(page=0xF)`
15. **64-step palette fade-out**：`for iVar5 in [0, 0x40): set_vga_palette_range_with_add(0, 0xFF, iVar5)` + 4ms (整螢幕逐步 white-out)
16. `crt_memset(0xA0000, 0, 64000)` — 黑屏 + 800ms
17. **64-step palette fade-in (reversed)**：`for iVar5 from 0x3E down to 0` + 4ms
18. `save_runtime_char_to_template` + `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **85** (= chapter_id × 3 + 1, chapter_id=28)，entry size 2107 bytes。
header `+0..+2` = shap_id_byte / party_count=20 / char_count=76；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

3 / 16 active hooks (其餘 13 為 sentinel 在初始狀態)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0 (enemy_turn_intro) | 0x4A | `0x00035C32` | sentinel-like (空魔神 cinematic 候選) |
| 0xFF | 2 (new_player_turn_intro) | 0x4C | `0x00035D60` | sentinel-like (phase=2) |
| 0xFF | 0 (enemy_turn_intro) | 0x4F | `0x00035EE6` | sentinel-like |

`turn=0xFF` 不會等於回合計數，初始 hook table 不會 fire。實際觸發機制：tile-step-event handler 在某些劇本 tile 被踩到時，會動態 rewrite 該 chapter turn-event-hook table 的 turn byte (0xFF → current_save_metadata_block / +1)，把原本 sentinel 的 entry 啟動為下一回合 fire 的 event。

對應「擊毀第一隻機甲隊長時，中央左右寶箱平台立即出現敵方援軍」+「護送悠妮到上方控制中心石碑前休息後再過三回合打三頭龍」屬此類 tile-step → turn-event 連鎖。
