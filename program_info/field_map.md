# field_map

章節生命週期 (chapter lifecycle) + 地圖/場景切換。每章一份 init / end handler，
共 60 個（部分共用）。每章獨有的 handler 細節見 `program_info/chapters/`。

## Chapter jump tables (4 張)

全部 pointer[30]，用 `current_chapter_id @ 0x53C03` 索引：

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x00051D71` | `chapter_init_jump_table` | 進入章節時呼叫 |
| `0x00051DE9` | `chapter_end_jump_table` | 結束章節時呼叫 |
| `0x00051B19` | `data_fd2_chapter_post_action_handler_table` (= `chapter_misc_jump_table`) | 5 個 turn-cycle 點觸發 |
| `0x00051B91` | `data_fd2_battle_ai_post_action_consequence_table` | 由 `tile_event_consumed_idx` 與 FDFIELD turn-event hook 索引 (詳 `chapter_event_dispatch.md`) |

## Per-chapter byte arrays

| 位址 | 名稱 | 每章 1 byte，值範圍 |
|---|---|---|
| `0x00051E63` | `data_fd2_audio_per_chapter_player_turn_bgm_track[30]` | BGM track id (0x03/0x04/0x08/0x13) |
| `0x00051E81` | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[30]`  | BGM track id (0x01/0x04/0x06/0x08/0x0C) |

`fd2_run_full_turn_cycle` 把兩者 cast 為 BGM track id 餵給 `fd2_set_bgm_track_with_fade`。

## Chapter init handlers (30 個，3 個共用)

| ID | 位址 | 名稱 | 大小 |
|---|---|---|---|
| 1 | `0x0003231B` | `fd2_chapter_01_init` | 含獨家 prologue (3-phase) |
| 2 | `0x00032D18` | `fd2_chapter_02_init` | 402 B |
| 3 | `0x00032E8C` | `fd2_chapter_03_init` | 324 B |
| 4 | `0x00032FB2` | `fd2_chapter_04_init` | 181 B |
| 5 | `0x00033049` | `fd2_chapter_05_init` | 258 B |
| 6 | `0x0003314B` | `fd2_chapter_06_init` | 79 B |
| 7 | `0x00033169` | `fd2_chapter_07_init` | 176 B |
| 8 | `0x00033219` | `fd2_chapter_08_init` | 100 B |
| 9 | `0x0003327D` | `fd2_chapter_09_init` | 174 B |
| 10 | `0x0003332B` | `fd2_chapter_10_init` | 90 B |
| 11 | `0x00033367` | `fd2_chapter_11_init` | 142 B |
| 12 | `0x000333F5` | `fd2_chapter_12_init` | 118 B |
| 13 | `0x0003346B` | `fd2_chapter_13_init` | 17 B |
| 14 | `0x0003347C` | `fd2_chapter_14_init` | 29 B |
| 15 | `0x000334D9` | `fd2_chapter_15_init` | 199 B |
| 16 | `0x000335A0` | `fd2_chapter_16_init` | 10 B (stub) |
| 17 | `0x000335AA` | `fd2_chapter_17_init` | 48 B |
| 18 | `0x000335DA` | `fd2_chapter_18_init` | 154 B |
| 19-21 | `0x00033674` | `fd2_chapter_19_20_21_init_shared` | 10 B (三章共用) |
| 22 | `0x0003367E` | `fd2_chapter_22_init` | 34 B |
| 23 | `0x000336A0` | `fd2_chapter_23_init` | 548 B (最大) |
| 24 | `0x000338C4` | `fd2_chapter_24_init` | 166 B |
| 25 | `0x0003396A` | `fd2_chapter_25_init` | 324 B |
| 26 | `0x00033AAE` | `fd2_chapter_26_init` | 67 B |
| 27 | `0x00033AF1` | `fd2_chapter_27_init` | 428 B |
| 28 | `0x00033C9D` | `fd2_chapter_28_init` | 285 B |
| 29 | `0x00033DBA` | `fd2_chapter_29_init` | 130 B |
| 30 | `0x00033E3C` | `fd2_chapter_30_init` | 316 B |

## Chapter end handlers (30 個)

| ID | 位址 | 名稱 | 大小 |
|---|---|---|---|
| 1 | `0x00022EF6` | `fd2_chapter_01_end` | 65 B |
| 2 | `0x00022F37` | `fd2_chapter_02_end` | 443 B |
| 3 | `0x000230F2` | `fd2_chapter_03_end` | 214 B |
| 4 | `0x000231BC` | `fd2_chapter_04_end` | 61 B |
| 5 | `0x000231F9` | `fd2_chapter_05_end` | 157 B |
| 6 | `0x00023296` | `fd2_chapter_06_end` | 82 B |
| 7 | `0x000232E8` | `fd2_chapter_07_end` | 222 B |
| 8 | `0x000234BB` | `fd2_chapter_08_end` | 140 B |
| 9 | `0x000235BC` | `fd2_chapter_09_end` | 61 B |
| 10 | `0x000235F9` | `fd2_chapter_10_end` | 407 B |
| 11 | `0x00023790` | `fd2_chapter_11_end` | 69 B |
| 12 | `0x000237D5` | `fd2_chapter_12_end` | 214 B |
| 13 | `0x0002389F` | `fd2_chapter_13_end` | 61 B |
| 14 | `0x000238DC` | `fd2_chapter_14_end` | 225 B |
| 15 | `0x000239BD` | `fd2_chapter_15_end` | 77 B |
| 16 | `0x00023A0A` | `fd2_chapter_16_end` | 341 B |
| 17 | `0x00023B5F` | `fd2_chapter_17_end` | 374 B |
| 18 | `0x00023CD5` | `fd2_chapter_18_end` | 356 B |
| 19 | `0x00023E39` | `fd2_chapter_19_end` | 59 B |
| 20 | `0x00023E74` | `fd2_chapter_20_end` | 646 B |
| 21 | `0x000240FA` | `fd2_chapter_21_end` | 572 B |
| 22 | `0x000244B6` | `fd2_chapter_22_end` | 354 B |
| 23 | `0x00024754` | `fd2_chapter_23_end` | 960 B (最大) |
| 24 | `0x00024C1E` | `fd2_chapter_24_end` | 260 B |
| 25 | `0x00024DF2` | `fd2_chapter_25_end` | 142 B |
| 26 | `0x00024E80` | `fd2_chapter_26_end` | 466 B |
| 27 | `0x000250CC` | `fd2_chapter_27_end` | 920 B |
| 28 | `0x00025464` | `fd2_chapter_28_end` | 40 B |
| 29 | `0x0002548C` | `fd2_chapter_29_end` | 451 B |
| 30 | `0x00025757` | `fd2_chapter_30_end` | 544 B |

## Chapter handler 共用 helpers

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x205DA` | `fd2_init_battle_state_for_chapter` | 章節進入時設定 active FDTXT entry / 初始 turn |
| `0x135DD` | `fd2_pan_cursor_and_window` | cursor + window origin 移動到指定座標 |
| `0x112A5` | `fd2_init_runtime_char_from_base_growth` | 從 character_base / character_growth 初始化一個 runtime_char (player class) |
| `0x10B4E` | `fd2_load_chapter_portraits_and_dump_tmp` | 依 race_id 篩選 char_spawn_records，載入 portrait set 並 dump 到 FD2.TMP |
| `0x1366A` | `fd2_cutscene_event_trigger` | 觸發 milestone event (歸 text_dialog) |
| `0x32975` | `fd2_mark_char_as_dead` | 把指定 runtime_char 標 dead bit |
| `0x2CAD7` | `fd2_chapter_transition_menu` | 章間 modal (save/continue) |
| `0x10C50` | `fd2_init_runtime_char_for_battle` | 從 char_spawn_record 創建 runtime_char (含 enemy class) |
| `0x205B4` | `fd2_check_battle_end_default_handler` | 11 章共用的 post-action 預設 (`fd2_check_battle_end_condition` inlined) |

## Chapter handler 三 phase 結構

每章 init handler 大致呼叫順序：

**Phase 1 — Prologue** (`current_chapter_id = 0x20`)
- 清畫面、palette
- `fd2_cutscene_event_trigger(99)` — 開場音樂
- 淡入淡出迴圈
- `fd2_display_dialog_scene` 顯示 prologue 對話 pages 0..N
- 每頁配 `fd2_cutscene_event_trigger(0x5A..0x69)` event cue

**Phase 2 — 章節本體** (`current_chapter_id = 0x1F`)
- 場景切換 palette
- `fd2_load_chapter_portraits_and_dump_tmp(race_id)` 載入該章 portrait set
- 顯示章節 intro 對話

**Phase 3 — 正式進入章節** (`current_chapter_id = 0..0x1D`)
- `fd2_init_runtime_char_from_base_growth(char_id)` 加入隊伍角色
- `fd2_init_battle_state_for_chapter` 進入正式戰鬥模式
- `fd2_pan_cursor_and_window(...)` 移到地圖開始位置
- 開場 cutscene 與 dialog
- 戰鬥開始

只有 chapter_01_init 同時有 Phase 1+2+3，其他章大多只有 Phase 2+3 或更簡。

## data_fd2_chapter_post_action_handler_table (`0x51B19`)

30-entry 函式指標表，由 `current_chapter_id` 索引。從 5 個 turn-cycle 點觸發
（`fd2_game_main_loop` / `fd2_tick_status_effects_and_show_messages` / `fd2_npc_turn_phase_team1` /
`fd2_enemy_turn_phase_team0`），每章可自訂回合結束後的勝負判定邏輯。

### Stub vs 非 stub 分類

- **11 章用 stub** 指向 `fd2_check_battle_end_default_handler @ 0x205B4`：
  ch1, 3, 4, 5, 6, 7, 8, 9, 11, 14, 24
- **17 個 distinct 非 stub** + ch22/27/28 共用 = 19 章有自訂邏輯：
  ch2, 10, 12, 13, 15, 16, 17, 18, 19, 20, 21, 22 (≡27, 28), 23, 25, 26, 29, 30

### 自訂 post_action 五大模式

| Pattern | 描述 | 章節 |
|---|---|---|
| **Simple extra-lose** | 在 default 判定後加 `if char[X] dead → lose` | ch10 (chars[0x32,0x33])、12 (char[0xE])、15 (char[0x40])、16 (char[0x41])、21 (chars[0x10,0x11])、22/27/28 (char[1])、25 (char[0x10])、26 (chars[1,2]) |
| **Group survival** | 整個 NPC group 全死 → lose | ch2 (chars[5..10] 6 villagers)、13 (chars[0xF..0x1A] 12 elves)、20 (chars[0x35..0x3D] 8 elves) |
| **Gated by save_metadata_block** | 條件依「回合計數」生效 | ch13 (>5)、19 (>6)、20 (<16)、23 (<15) |
| **Bypass default** | 不呼叫 `fd2_check_battle_end_condition`，自行實作完整邏輯 | ch18, 23, 29, 30 |
| **Win condition (boss kill)** | `if char[X] dead → game_event_flag = 2`，多在 bypass-default 章 | ch18 (char[0x34]=黑暗騎士)、23 (char[0x12]=機甲隊長)、30 (char[0x14]=空魔神) |

### char_idx 在 post_action 中的意義

`char_idx` 是 `runtime_char_array` 的 index — **不是固定 char_id**，而是 runtime
隊伍 slot。同一 index 在不同章節指不同 NPC：

| char_idx | 章節 | 對應 NPC |
|---|---|---|
| 0 | 全 30 章 | 索爾 (主角；任章死即 lose) |
| 1 | ch22 | 希爾法 |
| 1 | ch26, 27, 28, 29, 30 | 悠妮 |
| 2 | ch26 | 亞奇梅吉 |
| 5..10 | ch2 | 6 villagers |
| 0xE | ch12 | 米亞斯多德 |
| 0xF..0x1A | ch13 | 精靈族 12 人 |
| 0x10 | ch18, 21, 23, 25 | 變動：約拿 / 羅蘭 / 卡里斯 / 聖寇拉斯 |
| 0x11 | ch18, 21, 23 | 蘭斯洛特 / 希爾法 / 羅德曼 |
| 0x12 | ch23 | 機甲隊長 (boss kill = win) |
| 0x14 | ch30 | 空魔神 (final boss kill = win) |
| 0x32, 0x33 | ch10 | 索菲亞 + 卡納恩三世 (init 起始 sleep) |
| 0x34 | ch17, 18, 20 | 變動 (蜜蒂 / 黑暗騎士 / 謝多) |
| 0x35..0x3D | ch20 | 精靈 group |
| 0x40 | ch15, 19 | 賽可邦勒 / 巴拿羅西亞 |
| 0x41 | ch16 | 蜜蒂 |
| 0x42..0x49 | ch16 | 蜜蒂 8 部下 |

要逐章對應 char_idx → char_id，需根據 `fd2_chapter_NN_init` 內
`fd2_init_runtime_char_from_base_growth` 呼叫順序追蹤。詳細對照見
`program_info/chapters/`。

## save_metadata_block：精確 turn counter

`save_metadata_block` 等於螢幕顯示的「目前進行中 player turn 編號」
（從 1 起算）。

```
chapter init 時：save_metadata_block = 1   (第 1 回合進入)
each full turn cycle (player → NPC → enemy → next player intro):
  在「next player intro」開始前：save_metadata_block += 1
```

| 攻略本表述 | binary 條件 |
|---|---|
| 「N 回合內」 | `save_metadata_block <= N` 等價 `< N+1` |
| 「第 N 回合起」 | `save_metadata_block >= N` 等價 `> N-1` |

5 章 conditional gate：

| 章 | binary 條件 | 攻略本對應 |
|---|---|---|
| ch13 | `> 5` AND char[0x3B] dead → lose + page 2 | 第 6+ 回合後若哈瓦特死 |
| ch16 | `< 19` → 蜜蒂招募 gate | 「18 回合內擊敗敵全滅」 |
| ch19 | `> 6` AND char[0x40] dead → lose | 第 7+ 回合後若巴拿羅西亞死 |
| ch20 | `< 16` → 達可塞招募 gate | 「15 回合內」 |
| ch23 | `< 15` → 羅德曼招募 gate | 攻略「15 回合內」(實際 binary 為 turn 1..14) |

關鍵：turn 1 是初始進入 player turn 那一輪 (screen 顯示 "TURN 1")；經過完整
player→NPC→enemy 循環後，下一輪 player turn 開場時才 += 1。所以
`fd2_chapter_NN_post_action` 與 `fd2_chapter_NN_end` 期間檢查 `save_metadata_block` 等於
**當前** player turn 編號。

## game_event_flag 三態

`game_event_flag @ 0x53ECC`：

- **0** = battle ongoing (default)
- **1** = char[0] (索爾) dead → game over (`fd2_main` 觸發 fanfare/over screen 然後 reset 0)
- **2** = all team-0 (敵全滅) → chapter cleared (`fd2_main` 觸發 chapter_end + transition + next_init)

寫入由 `fd2_check_battle_end_condition @ 0x205BE` 在每次 turn cycle 結束時計算寫入；
guard 在 `fd2_run_full_turn_cycle` 多處用 `if (game_event_flag == 0)` 在每階段切換前
檢查是否提前結束。

## tile_event_consumed_flags 用途

post_action 中**唯一**的 `tile_event_consumed_flags` 用法在 `ch29_post_action`：
若 `flags[0x12, 0x13, 0x14]` 全 set → `game_event_flag = 2` (win)。對應「解除
防衛系統」=「啟動 3 個控制台 tile event」。

end handler 中也用到：
- `ch7_end`: `flags[0x11] == 1` → 條件招募 char 12
- `ch26_end`: `flags[0xC]` 動態決定 dialog page (5 個寶箱對應 5 路線)

## 跨章節隱藏機制鏈

詳細的 conditional recruit、reward、Good/Bad ending fork 見
`assets/chapters/_index.md`。簡列：

- ch21_end (6-item collection 0xD1..0xD6) → `fd2_give_item_to_first_player_char(100=天空之鑰)`
- ch23_end: `fd2_any_char_has_item(100)` → 卡里斯 (char 22) 加入 (good path)
- ch27_end: `fd2_any_char_has_item(100)` 否則 game over (bad ending)
- ch30_end: 殺空魔神 → `game_event_flag = 2` → load chapter 31 epilogue map +
  `fd2_play_game_ending_cinematic` (good ending)
