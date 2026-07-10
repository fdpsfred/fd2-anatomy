# ui_menu — 戰場 UI 與選單分派

戰場側的每 frame 輸入分派、三層游標、玩家行動選單 orchestrator、與空格叫出的
field-command modal。城鎮側選單（商店/轉職/status/章節交接）另見 town_menu.md。

## 驗證對象

- 對應 src：`src/ui_menu/menu.c`、`src/ui_menu/cursor.c`、`src/ui_menu/menufld.c`、
  `src/ui_menu/menucfg.c`
- 主要 Ghidra 對象（name@addr，均即時核對）：
  - `fd2_game_main_loop @ 0x117E7`、`fd2_wait_for_input_with_idle @ 0x11AA8`
  - `fd2_player_action_menu_loop @ 0x18890`、`fd2_player_inline_action_menu_dispatch @ 0x18D8C`
  - `fd2_field_command_menu_loop @ 0x16F55`、
    `fd2_field_menu_status_save_load_quit_dispatch @ 0x19DF7`、
    `fd2_game_options_menu_loop @ 0x1728C`、`fd2_open_tactical_overview_zoom @ 0x2000A`
  - `fd2_cursor_move_up @ 0x11B48`（含 down/left/right）、`fd2_pan_cursor_to_tile_animated @ 0x12CEA`
  - `fd2_mark_char_occupant_tiles_for_team @ 0x146D1`、
    `fd2_wait_input_with_status_panel_repaint @ 0x18B84`、
    `fd2_wait_for_action_target_input @ 0x115B6`、`fd2_walk_path_animation_loop @ 0x13488`、
    `fd2_check_tile_event_post_action @ 0x13A44`
  - `fd2_init_movement_range_floodfill @ 0x4E040`、`fd2_pathfind_to_destination @ 0x4E1A6`
    （演算法正典見 pathfind.md）
- 相關資源檔：FDOTHER.DAT（SFX bank、intro/狀態面板 RLE）、FDTXT.DAT（對話頁）

## Per-frame 主迴圈：`fd2_game_main_loop @ 0x117E7`

從 `main` 內迴圈每 frame 呼叫一次（唯一呼叫者 main），同時涵蓋地圖探索與戰場輸入。
先以 `fd2_wait_for_input_with_idle` 取一個鍵盤掃描碼再分派。回傳 int 供 main 判斷
（0 = 續迴圈、非 0 = 離開內迴圈）；只有空格上的 field-command 路徑會回非 0。

| Scancode | 動作 | 處理 |
|---|---|---|
| 0x01 / 0x2C / 0x4C | 切到下一個未行動 team-2 友軍（游標 pan 過去）+ 清鍵盤緩衝 | inline + `fd2_clear_keyboard_buffer` |
| 0x39 / 0x1C (Space/Enter) | 行動 / 確認 | `fd2_find_char_at_cursor_pos` -> 三路分派 |
| 0x22 | 保留 no-op（章節專用預留） | — |
| 0x3B / 0x49 (F1) | 戰術俯瞰縮放 | `fd2_open_tactical_overview_zoom @ 0x2000A` |
| 0x3C / 0x47 (F2) | 游標目標唯讀狀態檢視 | `fd2_open_char_status_screen`（見 town_menu.md）|
| 0x48 (↑) | cursor up | sfx 0 + `fd2_cursor_move_up @ 0x11B48` |
| 0x50 (↓) | cursor down | sfx 0 + `fd2_cursor_move_down` |
| 0x4B (←) | cursor left | sfx 0 + `fd2_cursor_move_left` |
| 0x4D (→) | cursor right | sfx 0 + `fd2_cursor_move_right` |

### Space/Enter 三路分派

`fd2_find_char_at_cursor_pos` 回傳游標下的 char_idx 或 -1。先歸零
`data_fd2_battle_pending_xp_credit`，並過濾掉 portrait_id == 0x79 與 archetype_flag == 10
的特殊標記格（這兩種直接不處理）：

- **-1（空 tile）**：loop `fd2_field_command_menu_loop`（Save/EndTurn/Options/Suspend modal）。
- **team == 2 且未行動且未麻痺**（flags bit7 == 0、status_paralysis_flag == 0）：播確認 sfx 7，
  loop `fd2_player_action_menu_loop` — 移動 + 動作環。
- **其他**（己方已動或敵方/NPC）：`fd2_open_char_status_screen` 唯讀狀態檢視（見 town_menu.md）。

行動結束後：composite frame、pending_xp_credit 夾到上限 99、`fd2_process_xp_and_level_up_for_char`、
呼叫該章 post-action handler、`fd2_check_all_player_acted_or_incapacitated`、必要時派
ai_post_action_consequence，最後清鍵盤緩衝。

## 三層 cursor 座標（cursor.c）

| 位址 | 符號 | 範圍 |
|---|---|---|
| 0x53AB1 | `data_fd2_battle_cursor_world_x` | 0..map_width_tiles-1 |
| 0x53AB5 | `data_fd2_battle_cursor_world_y` | 0..map_height_tiles-1 |
| 0x53AB9 | `data_fd2_battle_cursor_screen_x` | 視窗內相對位置（有號）|
| 0x53ABD | `data_fd2_battle_cursor_screen_y` | 同上（有號）|
| 0x53AA9 | `data_fd2_battle_view_window_origin_x` | 視窗左上角 world 座標 |
| 0x53AAD | `data_fd2_battle_view_window_origin_y` | 同上 |
| 0x53AC1 | `data_fd2_battle_map_width_tiles` | 上限 |
| 0x53AC5 | `data_fd2_battle_map_height_tiles` | 上限 |

`fd2_cursor_move_up/down/left/right`（cursor.c，起 0x11B48）維護三層座標同步：靠近視窗邊緣時
自動 scroll window origin，否則只動 screen cursor。composite 重繪的 gating 是：只有在「inner-step
分支且無動畫進行中（`battle_anim_phase @ 0x51A83 == 0`）」時才略過重繪，交給主迴圈下一 tick 自動重畫
游標；其餘情形（scroll 分支、動畫進行中、或頂／左邊界 world 座標已達 0）都一律立即 trigger
`fd2_composite_battle_frame`。

`cursor_screen_x/y` 是**有號 int**（move handler 以 JGE/JLE 有號分支和視窗邊緣比較）。當游標
或行走中的角色捲到地圖頂／左邊緣（origin 已到 0、walk-step 的 scroll 分支被 `origin != 0`
守衛擋掉而改走 inner 分支持續遞減）時，screen 座標會變負值。若誤宣告成無號，邊緣比較會把
負值座標當成極大值、取錯 scroll/inner 分支——這正是第一章開場走位動畫「對話框跳出時畫面捲到
地圖底部」的成因。

## Player 行動 UI：`fd2_player_action_menu_loop @ 0x18890`

戰場玩家回合的核心 UI orchestrator（menu.c，唯一呼叫者 fd2_game_main_loop）。己方未行動角色
收到 Space/Enter 後進入，讓玩家選移動目標格與行動：

1. 複製 4-int menu_state 樣板（`data_fd2_ui_player_action_menu_state_template`）；
   `data_fd2_battle_player_action_result_code @ 0x53C53` 歸零；
   `data_fd2_battle_ai_post_action_consequence_idx` 設 0xFF。
2. 移動預算 = 角色 runtime +0x3B（combat_aux_block[0x14] 移動力）。移動成本表按 bJob_id 選，遇
   `fd2_check_char_status_immunity != 0`（飛行）改用 job 0x13、遇 portrait_id == 0x1C 改用 0x10；
   `fd2_get_movement_cost_table_for_job` 取表，並 malloc 出路徑緩衝（per-job cost table 見 pathfind.md）。
3. `fd2_init_movement_range_floodfill`（0x4E040）以移動預算 flood-fill 出可達 tile 並寫進
   battle_tile_map（演算法見 pathfind.md）。
4. `fd2_mark_char_occupant_tiles_for_team(char_idx, 1)`（0x146D1）把其他角色（team != 0）佔據的
   格子標為不可通行，避免走位穿過友軍/敵人。
5. 存下目前 cursor world 座標當起點（saved origin）。
6. `fd2_wait_input_with_status_panel_repaint`（0x18B84）等待鍵盤輸入，過程持續重繪目標的戰場迷你
   狀態面板（HP/MP，依 cursor 螢幕位置決定貼左或右側）。
7. `fd2_wait_for_action_target_input(4, 0, NULL)`（0x115B6）等玩家把游標移到目的地格並確認；回
   -1 = 取消 -> pan 回起點、free、return 1（讓外層重問）。
8. `fd2_pathfind_to_destination`（0x4E1A6）從起點對目的地做路徑搜索，回傳步數（尋路演算法見
   pathfind.md）。
9. 步數為 0（原地不動）-> 直接開行動子選單；否則 `fd2_walk_path_animation_loop`（0x13488）播走路
   動畫後開子選單，並對 portrait_id 不在 {0x12,0x13,0x22} 的角色 enable「已移動」動作集限制。
10. `fd2_player_inline_action_menu_dispatch`（0x18D8C）開 Attack/Spell/Item/Wait 行動子選單，loop
    到 commit（非 0）或全取消（-1）。
11. commit 後 `fd2_check_tile_event_post_action(pos_x, pos_y, 1)`（0x13A44）檢查落點 tile 是否觸發
    scripted post-action consequence，`fd2_mark_char_acted_this_turn` 對 runtime +0x05 bit7 標記
    本回合已行動，return `data_fd2_battle_player_action_result_code` 回 fd2_game_main_loop。

`fd2_player_inline_action_menu_dispatch` 依 have_moved 旗標調整可選動作集：Attack 無武器或射程內無
目標時 disable、Item 無可用道具時 disable、Spell 無可用法術或被沉默（combat_aux_block[0] != 0）時
disable；Wait 在尚未移動時回復 20% HP 並跑 tile-event 互動。法師移動後不能施法的機制正典見 spell.md。

## Field command menu（空 tile + Space）：`fd2_field_command_menu_loop @ 0x16F55`

玩家在無角色的空格按 Space/Enter 時彈出的 modal（menu.c）。複製 options 樣板（0x51E9F）與 state
樣板（0x53EF2）、`data_fd2_ui_menu_cursor_idx @ 0x53C57` 歸零，開選單並 loop 輸入到非 0；取消(-1)
return 1。依 cursor idx 分派：

| idx | 選項 | 動作 |
|---|---|---|
| 0 | Save / Load / New Game | `fd2_field_menu_status_save_load_quit_dispatch @ 0x19DF7` |
| 1 | End my turn | 「結束回合?」確認後，把每個未行動的 team-2 角色 walk 到游標格並派其 post-action consequence，`fd2_run_full_turn_cycle`，return 1 |
| 2 | Options | `fd2_game_options_menu_loop @ 0x1728C`（偏好設定子選單），return 0 |
| 3 | Suspend | 「Suspend the game?」確認後 `fd2_run_full_turn_cycle`，return 1 |

子提示取消時顯示「Aborted」對話（text 0x19C）並 return 1。多個 modal 共用
`data_fd2_ui_menu_cursor_idx` 判選擇。

## 城鎮選單與狀態畫面

城鎮選單樹（商店/轉職/status/章節交接）及角色狀態畫面 `fd2_open_char_status_screen @ 0x17AED`
（戰場 F2 與 Space/Enter「其他」分支叫出的唯讀檢視、含 spell list overlay 與 3-buffer slide-in
動畫）見 town_menu.md。

## 輸入等待：`fd2_wait_for_input_with_idle @ 0x11AA8`

主 input poll（fd2_game_main_loop 每 frame 呼叫一次）：

- `fd2_check_keyboard_buffer_nonempty` 檢查鍵盤；無輸入時 idle：`fd2_update_palette_cycle_anim`
  推進水/火把 palette；BIOS midnight tick（0:046C，18.2Hz word）變化時 `fd2_composite_battle_frame(0)`
  重繪一格——游標閃爍動畫即來自此。
- 有輸入時 `int386(0x16, ...)` 讀 INT 16h scancode，標準化特例：0xE0 或 0x52 -> 0x1C（Enter）、
  0x53 -> 0x01（Esc）。
- 回傳 scancode（存於 key_input_mode @ 0x53A8E）。

## 鏡頭

`fd2_pan_cursor_to_char` / `fd2_pan_cursor_to_tile_animated @ 0x12CEA`：鏡頭動畫平移到指定角色/格子。

## SFX 觸發

`fd2_play_sfx_with_handle @ 0x25A96` 是通用 AIL SFX 播放器，UI 各處呼叫（游標移動 sfx 0、
確認 sfx 7、取消等）。3 個 gate flag 詳見 audio.md §SFX 觸發。
