# 第 24 章 — chapter_24

Init 含獨家 4-stage camera scan，預先掃過地圖四角；end 為 FD2 唯一的 text-scroll cinematic（文字向上 scroll + palette fade-out 收尾）。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_24_init @ 0x000338C4` | 166 B |
| End | `chapter_24_end @ 0x00024C1E` | 260 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[23]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[23]` |  |

## Init handler 階段

4-stage map preview camera tour：

1. `init_battle_state_for_chapter`
2. `display_dialog_scene(page=0)`
3. `load_chapter_portraits_and_dump_tmp(1)`
4. **4× pan + 400ms delay**：
   - `pan_cursor_and_window(0, 4)` + 400ms
   - `pan_cursor_and_window(0, 0x16)` + 400ms
   - `pan_cursor_and_window(0x1A, 0x18)` + 400ms
   - `pan_cursor_and_window(0x1A, 2)` + 400ms
5. `display_dialog_scene(page=1)` + `pan_cursor_to_char(0)`

四個 pan 座標即為 FDFIELD turn-event 援軍的四個 spawn 角落，init 階段預先讓玩家看到。

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 24 | 0, 1 |
| End | 24 | 2, 3 |

## char_id 初始化序列

無 `init_runtime_char_from_base_growth` 呼叫；本章不在 init 或 end 加入新角色。

## Cutscene events

無 `cutscene_event_trigger`；init 用 4× pan + 400ms delay 取代 cutscene event walk-animation。

## Post-action handler

`data_fd2_chapter_post_action_handler_table[23]` 指向 `check_battle_end_default_handler`，無自訂勝負條件：
- 全敵死 → win
- 索爾 (chars[0]) 死 → lose

## End handler events

`chapter_24_end @ 0x00024C1E` (260 B) — text-scroll cinematic 結尾（FD2 唯一）：

1. `display_dialog_scene(page=2)`
2. **Phase 1 文字向上捲動**：迴圈 `for line=2..9`：
   - `scroll_text_screen_up_by_lines(line)`
   - 內層 30 frames：`composite_battle_frame(1)` + `wait_n_bios_ticks(1)`
3. `display_dialog_scene(page=3)`
4. **Phase 2 文字向上捲動 + palette fade**：迴圈 `for line=9..14`：
   - `scroll_text_screen_up_by_lines(line)`
   - 內層 12 frames：`set_vga_palette_range(0, 0xFF, brightness_sub)` 漸暗 + `composite_battle_frame(0)` + `wait` + `brightness_sub++`
   - 結束時 brightness_sub = 12 × 5 = 60 → 全暗
5. `crt_memset(0xA0000, 0, 64000)` — 黑屏
6. `save_runtime_char_to_template` + `current_chapter_id += 1`

`scroll_text_screen_up_by_lines` 在全 FD2 中只在 ch24_end 呼叫一次。

## FDFIELD event script

FDFIELD entry idx **70** (= chapter_id × 3 + 1, chapter_id=23)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

4 / 16 active hooks (其餘 12 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 2 | 0 (enemy_turn_intro) | 0x36 | `0x0003535D` | 援軍 reinforcement (4 角落) |
| 4 | 0 (enemy_turn_intro) | 0x36 | `0x0003535D` | 援軍 reinforcement |
| 7 | 0 (enemy_turn_intro) | 0x36 | `0x0003535D` | 援軍 reinforcement |
| 10 | 0 (enemy_turn_intro) | 0x36 | `0x0003535D` | 援軍 reinforcement |
