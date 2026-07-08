# 第 28 章 — chapter_28

Init 全清隊 20 chars + revive HP>0；3× 重複觸發同一 cutscene event 0x55；30 章中最小 end (40 B)。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_28_init @ 0x00033C9D` | 285 B |
| End | `fd2_chapter_28_end @ 0x00025464` | 40 B (最小 end) |
| Post-action | `fd2_chapter_22_27_28_post_action_shared @ 0x00020A87` | default + lose if char[1] dead (與 ch22/27 共用) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[27]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[27]` |  |

## Init handler 階段

與 ch23 init 結構類似（清隊 + spell visual）但範圍更大 (20 chars vs 16 chars)：

1. `fd2_init_battle_state_for_chapter`
2. **大規模清隊**：`for i in [0, 0x14): fd2_mark_char_as_dead(i)` (20 chars 全標 dead)
3. `fd2_pan_cursor_and_window(0x1D, 0xF)` + `fd2_cast_screen_wide_spell_with_fade(cursor+6, +5, 10, 8)`
4. **revive 過濾**：`for i in [0, 0x14): if chars[i].wHP_current != 0: chars[i].bFlags = 0`
   - 與 ch23 不同：ch28 此處 **不設 sprite facing**
5. `fd2_composite_battle_frame` + `fd2_set_vga_palette_range_with_add(0, 0xFF, 0)` palette restore
6. 500ms wait
7. **3× `fd2_cutscene_event_trigger(0x55)`** — 同 event 觸發三次 (3 個並行 group 各執行一次同 walk script)
8. `fd2_clear_all_chars_facing` + `fd2_display_dialog_scene(page=0)`
9. `fd2_cinematic_chapter_portrait_dump_with_white_flash(0, 0x10, 6)` + `fd2_cinematic_chapter_portrait_dump_with_white_flash(7, 0x10, 7)` — pan + load_chapter_portraits + palette flash + composite_battle_frame cinematic transition helper
10. `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 28 | 0 |
| End | 28 | 7 |

## char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫；本章不在 init 或 end 加入新角色。

## Cutscene events

- Init: `0x55` (1 unique event，呼叫 3 次)

## Post-action handler

`fd2_chapter_22_27_28_post_action_shared @ 0x00020A87` (與 ch22/27 共用)：
- 標準 default 判定（全敵死 = 勝、索爾死 = 負）
- **額外 lose 條件**：if chars[1] dead → `game_event_flag = 1`

本章 chars[1] = 悠妮（編成畫面 pin 表：chapter_id > 0x19 → char 9）。

## End handler events

`fd2_chapter_28_end @ 0x00025464` (40 B) — 30 章中最小 end：

1. `fd2_display_dialog_scene(page=7)`
2. `fd2_save_runtime_char_to_template`
3. `current_chapter_id += 1`

無 cutscene、無加入。

## FDFIELD event script

FDFIELD entry idx **82** (= chapter_id × 3 + 1, chapter_id=27)，entry size 1691 bytes。
header `+0..+2` = shap_id_byte / party_count=20 / char_count=60；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

3 / 16 active hooks (其餘 13 為 sentinel 在初始狀態)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0 (enemy_turn_intro) | 0x42 | `0x000359C8` | sentinel-like |
| 0xFF | 0 (enemy_turn_intro) | 0x44 | `0x00035A48` | sentinel-like |
| 0xFF | 0 (enemy_turn_intro) | 0x46 | `0x00035B05` | sentinel-like |

`turn=0xFF` 不會等於回合計數，初始 hook table 不會 fire。實際觸發機制：tile-step-event handler 在某些劇本 tile 被踩到時，會動態 rewrite 該 chapter turn-event-hook table 的 turn byte (0xFF → current_save_metadata_block / +1)，把原本 sentinel 的 entry 啟動為下一回合 fire 的 event。

對應「越過藍色平台左側火焰則己方結束時敵援軍」屬此類 tile-step → turn-event 連鎖。
