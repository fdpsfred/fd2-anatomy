# ui_menu

## Per-frame 主迴圈：`fd2_game_main_loop @ 0x117E7`

從 `main` 的內迴圈每 frame 呼叫，分派鍵盤掃描碼：

| Scancode | 動作 | 處理函式 |
|---|---|---|
| 0x01 / 0x2C / 0x4C (Esc) | cancel / 切換到下一友軍 | inline + `fd2_clear_keyboard_buffer` |
| 0x39 / 0x1C (Space/Enter) | 行動 | `fd2_find_char_at_cursor_pos` → 三路分派 |
| 0x48 (↑) | cursor up | `fd2_cursor_move_up` |
| 0x50 (↓) | cursor down | `fd2_cursor_move_down` |
| 0x4B (←) | cursor left | `fd2_cursor_move_left` |
| 0x4D (→) | cursor right | `fd2_cursor_move_right` |
| 0x3B / 0x49 (F1) | 主選單 | `fd2_open_tactical_overview_zoom` |
| 0x3C / 0x47 (F2) | 游標目標 status query | `fd2_open_char_status_screen` |

### Space/Enter 三路分派

`fd2_find_char_at_cursor_pos` 回傳 char_idx 或 -1：

- **-1** (空 tile)：開 `fd2_field_command_menu_loop` (Save/EndTurn/Suspend modal)
- **bTeam == 2 + 未動作** (己方角色未行動)：`fd2_player_action_menu_loop` — 移動 + 動作環
- **其他** (己方已動或敵方/NPC)：`fd2_open_char_status_screen` — 唯讀狀態檢視

## 三層 cursor 座標

| 位址 | 名稱 | 範圍 |
|---|---|---|
| `0x53AB1` | `cursor_world_x` | 0..map_width_tiles-1 |
| `0x53AB5` | `cursor_world_y` | 0..map_height_tiles-1 |
| `0x53AB9` | `cursor_screen_x` | 視窗內相對位置 |
| `0x53ABD` | `cursor_screen_y` | 同上 |
| `0x53AA9` | `battle_window_origin_x` | 視窗左上角 world 座標 |
| `0x53AAD` | `battle_window_origin_y` | 同上 |
| `0x53AC1` | `map_width_tiles` | 上限 |
| `0x53AC5` | `map_height_tiles` | 上限 |

`cursor_move_up/down/left/right` 維護三層座標的同步：靠近邊緣時自動 scroll
window origin，否則只動 screen cursor。每次都 trigger `fd2_composite_battle_frame`
（除非動畫進行中）。

## Player 行動 UI：`fd2_player_action_menu_loop @ 0x18890`

250 行的核心 UI orchestrator：

1. 讀角色 movement-cost 表 (按 bJob_id，可被 portrait_id 0x1C 或地形 trigger
   override 為 0x10 或 0x13)
2. `FUN_0004E040` 算出可達 tile (marked in battle_tile_map)
3. `FUN_000146D1` 高亮可達範圍
4. `FUN_00018B84` 開動作環選單
5. `FUN_000115B6(4, 0, NULL)` 等玩家選 destination tile
6. 若取消：redraw 並回；若 unreachable：回 0
7. `FUN_0004E1A6` 路徑搜索 (src→dst)
8. `FUN_00013488` 走路動畫
9. `FUN_00018D8C` 開行動子選單 (Attack/Item/Wait/Cancel — 已動則限制動作集)
10. `FUN_00013A44` 完成位置 finalize

`player_action_result_code @ 0x53C53` 把選擇結果傳回 `fd2_game_main_loop`。

## Field command menu (空 tile + Space)

`fd2_field_command_menu_loop @ 0x16F55` 開出 4 選項 modal：

| 選項 | 動作 |
|---|---|
| 0 | Save / Load / NewGame → `fd2_field_menu_status_save_load_quit_dispatch` |
| 1 | End my turn → 全 team 2 角色 walk to cursor + game_event |
| 2 | `fd2_game_options_menu_loop @ 0x1728C` — game options/preferences sub-menu |
| 3 | Suspend → 確認 + save+exit |

讀 `current_menu_cursor_idx @ 0x53C57` 來判斷選擇 (多個 modal 共用)。

## Status screen：`fd2_open_char_status_screen @ 0x17AED`

modal UI 含 spell list overlay。3-buffer slide-in 動畫：

1. `FUN_00017E0B` 填 stat 顯示資料
2. `FUN_00016C57(0)` 等 ACK
3. `fd2_build_usable_spell_list` count spells
4. 若有 spell：7-frame slide-in (`paint_status_panel_layer_left/right` 階段
   + `FUN_0001839B` slide step) → `fd2_draw_spell_selection_list` 唯讀顯示 → 7-frame
   slide-out
5. 12-frame outro 透過 `fd2_play_status_screen_outro_step`

## 輸入等待

`fd2_wait_for_input_with_idle @ 0x11AA8` 是主要 input poll：

- `fd2_check_keyboard_buffer_nonempty` 檢查鍵盤輸入；無輸入時：
  - `fd2_update_palette_cycle_anim` 更新水/熔岩 palette
  - 若 BIOS tick 變化：`fd2_composite_battle_frame` (cursor 閃爍)
- 有輸入時：呼叫 `int386(0x16, ...)` → 標準化掃描碼 (0xE0 / 0x52 → 0x1C；0x53 → 0x01)
- 回傳 scancode

## 鏡頭

`fd2_pan_cursor_to_char` / `fd2_pan_cursor_to_tile_animated @ 0x12CEA`：鏡頭動畫平移。

## SFX 觸發

`fd2_play_sfx_with_handle @ 0x25A96` 是通用 AIL SFX 播放器，UI 各處呼叫
(cursor 移動 sfx 0、確認 sfx 7、取消等)。3 個 gate flag 詳見 `audio.md` §SFX 觸發。
