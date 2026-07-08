# field

章節生命週期（chapter lifecycle）與 FDFIELD 事件派遣。每章有一組 init / end
handler（部分共用），戰鬥中的劇情事件則由 FDFIELD 資料驅動的函式 dispatch 觸發。
各章劇情、招募、結局分歧等內容面細節見 `assets/chapters/`。

## 驗證對象

- src：`field/chinit.c`、`field/chend1.c`、`field/chend2.c`、`field/chpost.c`、
  `field/chevt1.c`、`field/chevt2.c`、`field/chtrans.c`；派遣點在 `battle/btl_turn.c`
  （`fd2_run_full_turn_cycle` / `fd2_fire_chapter_turn_events_for_phase` /
  `fd2_check_tile_event_post_action`）。
- Ghidra 關鍵對象：
  - `fd2_chapter_01_init @ 0x3231B` ~ `fd2_chapter_30_init`（init handler）
  - `fd2_chapter_01_end @ 0x22EF6` ~ `fd2_chapter_30_end @ 0x25757`（end handler）
  - `fd2_run_full_turn_cycle @ 0x1A30B`（6-phase 回合迴圈）
  - `fd2_fire_chapter_turn_events_for_phase @ 0x1A813`（turn-event dispatcher）
  - `fd2_check_tile_event_post_action @ 0x13A44`（tile-step latcher）
  - `fd2_init_runtime_char_for_battle @ 0x10C50`（由 char_spawn_record 建 runtime_char）
  - `data_fd2_chapter_post_action_handler_table @ 0x51B19`（30-entry 勝負判定表）
  - `data_fd2_battle_ai_post_action_consequence_table @ 0x51B91`（90-entry 事件 handler 表）
  - `data_fd2_battle_turn_counter @ 0x53BEF`、`data_fd2_chapter_event_or_battle_end_code @ 0x53ECC`
- 相關資源檔：`FDFIELD.DAT`（章節戰場/事件資料，layout 見 `resource_info/fdfield.md`）、
  `FDTXT.DAT`（章節對話）。

## Chapter jump tables（4 張）

全部 pointer[30]，用 `data_fd2_chapter_current_chapter_id @ 0x53C03` 索引：

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x51D71` | `chapter_init_jump_table` | 進入章節時呼叫對應 init handler |
| `0x51DE9` | `chapter_end_jump_table` | 結束章節時呼叫對應 end handler |
| `0x51B19` | `data_fd2_chapter_post_action_handler_table` | 每回合 turn-cycle 結束時的勝負判定（見下） |
| `0x51B91` | `data_fd2_battle_ai_post_action_consequence_table` | 90-entry 事件 handler 表，由 FDFIELD turn-event / tile-step hook 與 AI post-action consequence idx 索引（見「FDFIELD 事件派遣機制」）|

前兩張表的 dispatch 序列（`CALL [id*4 + 0x51DE9]`、`CALL [id*4 + 0x51D71]`）位於
end handler 收尾與 main loop 的章節切換路徑。

## Per-chapter BGM track 表

| 位址 | 名稱 | 每章 1 byte |
|---|---|---|
| `0x51E63` | `data_fd2_audio_per_chapter_player_turn_bgm_track[30]` | 玩家回合 BGM track id |
| `0x51E81` | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[30]` | 敵方回合 BGM track id |

`fd2_run_full_turn_cycle` 以 `current_chapter_id` 取這兩個 byte 當 track id 餵給
`fd2_set_bgm_track_with_fade`。track id 語意與 BGM 派遣機制見 `audio.md`。

## Chapter init handlers（30 個，19/20/21 三章共用）

各章 init handler 的內部流程與角色編成細節屬 per-chapter 內容，正典在
`assets/chapters/`；下表僅列位址與大小作模組級索引。

| ID | 位址 | 名稱 | 大小 |
|---|---|---|---|
| 1 | `0x3231B` | `fd2_chapter_01_init` | 1625 B（含獨家 prologue） |
| 2 | `0x32D18` | `fd2_chapter_02_init` | 402 B |
| 3 | `0x32E8C` | `fd2_chapter_03_init` | 324 B |
| 4 | `0x32FB2` | `fd2_chapter_04_init` | 181 B |
| 5 | `0x33049` | `fd2_chapter_05_init` | 258 B |
| 6 | `0x3314B` | `fd2_chapter_06_init` | 79 B |
| 7 | `0x33169` | `fd2_chapter_07_init` | 176 B |
| 8 | `0x33219` | `fd2_chapter_08_init` | 100 B |
| 9 | `0x3327D` | `fd2_chapter_09_init` | 174 B |
| 10 | `0x3332B` | `fd2_chapter_10_init` | 90 B |
| 11 | `0x33367` | `fd2_chapter_11_init` | 142 B |
| 12 | `0x333F5` | `fd2_chapter_12_init` | 118 B |
| 13 | `0x3346B` | `fd2_chapter_13_init` | 17 B |
| 14 | `0x3347C` | `fd2_chapter_14_init` | 29 B |
| 15 | `0x334D9` | `fd2_chapter_15_init` | 199 B |
| 16 | `0x335A0` | `fd2_chapter_16_init` | 10 B（stub） |
| 17 | `0x335AA` | `fd2_chapter_17_init` | 48 B |
| 18 | `0x335DA` | `fd2_chapter_18_init` | 154 B |
| 19-21 | `0x33674` | `fd2_chapter_19_20_21_init_shared` | 10 B（三章共用） |
| 22 | `0x3367E` | `fd2_chapter_22_init` | 34 B |
| 23 | `0x336A0` | `fd2_chapter_23_init` | 548 B（init 最大） |
| 24 | `0x338C4` | `fd2_chapter_24_init` | 166 B |
| 25 | `0x3396A` | `fd2_chapter_25_init` | 324 B |
| 26 | `0x33AAE` | `fd2_chapter_26_init` | 67 B |
| 27 | `0x33AF1` | `fd2_chapter_27_init` | 428 B |
| 28 | `0x33C9D` | `fd2_chapter_28_init` | 285 B |
| 29 | `0x33DBA` | `fd2_chapter_29_init` | 130 B |
| 30 | `0x33E3C` | `fd2_chapter_30_init` | 316 B |

## Chapter end handlers（30 個）

`chend1.c` 收錄 ch1–19（`fd2_chapter_01_end` ~ `fd2_chapter_19_end`），
`chend2.c` 收錄 ch20–30（`fd2_chapter_20_end @ 0x23E74` ~ `fd2_chapter_30_end`），
邊界章 ch19 是 chend1 最後一章、ch20 是 chend2 第一章。各章招募/獎勵/分歧細節在
`assets/chapters/`。

| ID | 位址 | 名稱 | 大小 |
|---|---|---|---|
| 1 | `0x22EF6` | `fd2_chapter_01_end` | 65 B |
| 2 | `0x22F37` | `fd2_chapter_02_end` | 443 B |
| 3 | `0x230F2` | `fd2_chapter_03_end` | 214 B |
| 4 | `0x231BC` | `fd2_chapter_04_end` | 61 B |
| 5 | `0x231F9` | `fd2_chapter_05_end` | 157 B |
| 6 | `0x23296` | `fd2_chapter_06_end` | 82 B |
| 7 | `0x232E8` | `fd2_chapter_07_end` | 222 B |
| 8 | `0x234BB` | `fd2_chapter_08_end` | 140 B |
| 9 | `0x235BC` | `fd2_chapter_09_end` | 61 B |
| 10 | `0x235F9` | `fd2_chapter_10_end` | 407 B |
| 11 | `0x23790` | `fd2_chapter_11_end` | 69 B |
| 12 | `0x237D5` | `fd2_chapter_12_end` | 214 B |
| 13 | `0x2389F` | `fd2_chapter_13_end` | 61 B |
| 14 | `0x238DC` | `fd2_chapter_14_end` | 225 B |
| 15 | `0x239BD` | `fd2_chapter_15_end` | 77 B |
| 16 | `0x23A0A` | `fd2_chapter_16_end` | 341 B |
| 17 | `0x23B5F` | `fd2_chapter_17_end` | 374 B |
| 18 | `0x23CD5` | `fd2_chapter_18_end` | 356 B |
| 19 | `0x23E39` | `fd2_chapter_19_end` | 59 B |
| 20 | `0x23E74` | `fd2_chapter_20_end` | 646 B |
| 21 | `0x240FA` | `fd2_chapter_21_end` | 572 B |
| 22 | `0x244B6` | `fd2_chapter_22_end` | 354 B |
| 23 | `0x24754` | `fd2_chapter_23_end` | 960 B（end 最大） |
| 24 | `0x24C1E` | `fd2_chapter_24_end` | 260 B |
| 25 | `0x24DF2` | `fd2_chapter_25_end` | 142 B |
| 26 | `0x24E80` | `fd2_chapter_26_end` | 466 B |
| 27 | `0x250CC` | `fd2_chapter_27_end` | 920 B |
| 28 | `0x25464` | `fd2_chapter_28_end` | 40 B |
| 29 | `0x2548C` | `fd2_chapter_29_end` | 451 B |
| 30 | `0x25757` | `fd2_chapter_30_end` | 544 B |

## Chapter handler 共用 helpers

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x205DA` | `fd2_init_battle_state_for_chapter` | 章節進入時設定 active FDTXT entry、`data_fd2_battle_turn_counter = 1` |
| `0x135DD` | `fd2_pan_cursor_and_window` | cursor + window origin 移動到指定座標 |
| `0x112A5` | `fd2_init_runtime_char_from_base_growth` | 由 character_base / character_growth 建一個 player-class runtime_char |
| `0x10B4E` | `fd2_load_chapter_portraits_and_dump_tmp` | 依 race_id 篩 char_spawn_records，載 portrait set 並 dump 到 FD2.TMP |
| `0x1366A` | `fd2_cutscene_event_trigger` | 觸發 milestone cutscene event（見 `dialog.md`） |
| `0x32975` | `fd2_mark_char_as_dead` | 把指定 runtime_char 標 dead |
| `0x2CAD7` | `fd2_chapter_transition_menu` | 章間 modal（save / continue，見 `town_menu.md`） |
| `0x10C50` | `fd2_init_runtime_char_for_battle` | 由 char_spawn_record 建 runtime_char（含 enemy class） |
| `0x205BE` | `fd2_check_battle_end_condition` | 預設勝負判定（敵全滅 →2、主角死 →1） |
| `0x205B4` | `fd2_check_battle_end_default_handler` | 11 章 post-action 直接指向的預設 stub（緊接 `fd2_check_battle_end_condition`） |

## Init handler 階段結構

init handler 依 `current_chapter_id` 的寫入值切段：prologue 段把它設成 prologue map
id（0x20 / 0x1F），正式章節段才設成真正的 chapter id（0..0x1D）。整體流程大致為：

- **Prologue**（`current_chapter_id = 0x20 / 0x1F`）：清畫面 + palette、
  `fd2_cutscene_event_trigger(...)` 配 cutscene cue、`fd2_load_chapter_portraits_and_dump_tmp`
  載該章 portrait set、`fd2_display_dialog_scene` 播開場對話。
- **正式進入章節**（`current_chapter_id = 0..0x1D`）：先依序
  `fd2_init_runtime_char_from_base_growth(char_id)` 建立出戰隊伍角色，**之後才**呼叫
  `fd2_init_battle_state_for_chapter`（切正式戰鬥模式、設 turn=1），再
  `fd2_pan_cursor_and_window(...)` 移到地圖起點並播開場 cutscene。

只有 `fd2_chapter_01_init` 有完整 prologue（遊戲開場序章）。chinit.c 把它的
prologue 記為 Phase A–D 四段（A/B 在 map 0x20、C 在 map 0x1F、D 為第 1 章正式戰鬥），
逐段對話/cutscene 明細見 `assets/chapters/chapter_01.md`。其餘章節大多只有正式進入段
或極簡 stub。

## 回合結束 post-action：`data_fd2_chapter_post_action_handler_table`（`0x51B19`）

30-entry 函式指標表，由 `current_chapter_id` 索引，簽名 `void fn(uint32)`（dispatch 端
push 一個 cdecl 引數 active char_idx 再清掉，handler body 不讀）。從
`fd2_run_full_turn_cycle` 等 turn-cycle 點呼叫，讓每章自訂回合結束後的勝負判定。

### stub 與自訂

- **11 章用預設 stub** 指向 `fd2_check_battle_end_default_handler @ 0x205B4`：
  ch1, 3, 4, 5, 6, 7, 8, 9, 11, 14, 24。
- **其餘 19 章有自訂 handler**（定義於 `chpost.c`）：ch2, 10, 12, 13, 15, 16, 17, 18,
  19, 20, 21, 23, 25, 26, 29, 30，加上 ch22 / 27 / 28 共用同一個
  `fd2_chapter_22_27_28_post_action_shared @ 0x20A87`。

### 自訂 post-action 五大模式

| Pattern | 描述 | 章節 |
|---|---|---|
| **Simple extra-lose** | 跑完 default 判定後，加「若 slot X 死 → code=1（失敗）」 | ch12（slot 0xE）、15（0x40）、16（0x41）、25（0x10）；OR 兩格：ch10（0x32/0x33）、21（0x10/0x11）、26（1/2）；shared：ch22/27/28（slot 1） |
| **Group survival** | 整個 NPC group 全死才 → code=1 | ch2（slots 5..10，6 名村民）、13（0xF..0x1A，12 名精靈）、20（0x35..0x3C，8 名精靈） |
| **Turn-gated** | lose 條件加上 `data_fd2_battle_turn_counter` 門檻 | ch13（`>5` 且 slot 0x3B 死 → code=1 + page 2）、19（`>6` 且 slot 0x40 死 → code=1） |
| **Bypass default** | 不呼叫 `fd2_check_battle_end_condition`，自行寫完整勝負 | ch18, 23, 29, 30 |
| **Win condition（擊殺目標）** | 「若 boss slot X 死 → code=2（勝利）」，多在 bypass-default 章 | ch18（slot 0x34）、23（0x12）、30（0x14=空魔神） |

複合條件範例：ch17「隊伍已無 char_id 0x12（蜜蒂）且 slot 0x34 死 → code=1 + page 2」；
ch20 是最大的自訂 handler，三階段疊加（NPC group 0x35..0x3C 全滅 → code=1 + page 10；
slot 0 或 0x34 死 → code=1；敵方兩段聯集 0x24..0x33 與 0x3D..0x52 全滅 → code=2）。

### 勝負覆寫順序

bypass-default 章以「後寫覆蓋前寫」決定最終結果，方向分兩類：

- **win-overrides-loss**（先寫 lose、後寫 win）：ch18 / ch23 先判保護對象死（code=1），
  最後才判 boss 死（code=2）；因此在盟友倒下前擊殺 boss 仍算過關。
- **loss-overrides-win**（先寫 win、後寫 lose）：ch29 / ch30 先判 win，再判主角/必護對象死；
  ch30 先判空魔神死（code=2），再判索爾死（code=1）、悠妮死（code=1 + page 7），
  故主角或悠妮陣亡會覆蓋掉擊殺 boss 的勝利。

### char_idx 語意

post-action 內的 `char_idx` 是 `runtime_char_array` 的 index（runtime 隊伍 slot），**不是**
固定 char_id；同一 index 在不同章指不同角色。逐章 char_idx → 角色對照屬 per-chapter
內容，正典在 `assets/chapters/`，此處僅列關鍵對應：

| char_idx | 章節 | 對應角色 |
|---|---|---|
| 0 | 全 30 章 | 索爾（主角；任章死即失敗） |
| 1 | ch22 | 希爾法 |
| 1 | ch26 | 亞齊梅吉 |
| 1 | ch27, 28, 29, 30 | 悠妮 |
| 2 | ch26 | 悠妮 |
| 5..10 | ch2 | 6 名村民 |
| 0xE | ch12 | 米亞斯多德 |
| 0xF..0x1A | ch13 | 精靈族 12 人 |
| 0x10 | ch18, 21, 23, 25 | 變動：約拿 / 羅蘭 / 卡里斯 / 聖寇拉斯 |
| 0x11 | ch18, 21, 23 | 蘭斯洛特 / 希爾法 / 羅德曼 |
| 0x12 | ch23 | 機甲隊長（擊殺 = 勝利） |
| 0x14 | ch30 | 空魔神（final boss，擊殺 = 勝利） |
| 0x32, 0x33 | ch10 | 索菲亞 + 卡納恩三世（護送對象） |
| 0x34 | ch17, 18, 20 | 變動（蜜蒂 / 黑暗騎士 / 謝多） |
| 0x35..0x3C | ch20 | 精靈 group |
| 0x40 | ch15, 19 | 塞可邦勒 / 巴拿羅西亞 |
| 0x41 | ch16 | 蜜蒂 |
| 0x42..0x49 | ch16 | 蜜蒂 8 名部下 |

`ch22/27/28` 之所以共用一個 handler，是因為判負結構相同（default 檢查 + 保護 slot 1），
不是因為 slot 1 是同一角色。

## `data_fd2_battle_turn_counter`：精確 turn counter

`data_fd2_battle_turn_counter @ 0x53BEF` 等於螢幕顯示的「目前進行中 player turn 編號」
（從 1 起算）。`fd2_init_battle_state_for_chapter` 在章節進入時設為 1；每完整回合循環在
「進入下一個 player turn」前，於 `fd2_run_full_turn_cycle` 內 `+= 1`。turn 1 是初始那輪
（畫面顯示 "TURN 1"），經 player → NPC → enemy 一輪後，下一輪 player turn 開場時才 +1，
所以 post-action 與 end handler 讀到的值就是**當前** player turn 編號。

| 攻略本表述 | binary 條件 |
|---|---|
| 「N 回合內」 | `turn_counter <= N` 等價 `< N+1` |
| 「第 N 回合起」 | `turn_counter >= N` 等價 `> N-1` |

回合門檻用於招募/勝負判定的章節（門檻在 post-action 或 init/end handler 內）：

| 章 | binary 條件 | 攻略本對應 |
|---|---|---|
| ch13 | `> 5` 且 slot 0x3B（哈瓦特）死 → 失敗 + page 2 | 第 6+ 回合後哈瓦特死 |
| ch16 | `< 19` → 蜜蒂招募 gate | 「18 回合內全滅」 |
| ch19 | `> 6` 且 slot 0x40（巴拿羅西亞）死 → 失敗 | 第 7+ 回合後巴拿羅西亞死 |
| ch20 | `< 16` → 達克塞招募 gate | 「15 回合內」 |
| ch23 | `< 15` → 羅德曼招募 gate | 「15 回合內」（binary 實為 turn 1..14） |

## `data_fd2_chapter_event_or_battle_end_code` 三態

`data_fd2_chapter_event_or_battle_end_code @ 0x53ECC`（4-byte，zero-init，首次存取恆為寫入）：

- **0** = 戰鬥進行中（default）
- **1** = 失敗 → game over：主角或必護對象死；`main` 觸發 over 畫面後重置 0
- **2** = 勝利 → 過關：敵全滅或擊殺指定 boss；`main` 觸發 chapter_end + transition + 下一章 init

由 `fd2_check_battle_end_condition` 與各章 post-action handler 寫入；
`fd2_run_full_turn_cycle` 多處以 `if (code == 0)` 在每階段切換前檢查是否提前結束。

## `tile_event_consumed_flags` 用途

`data_fd2_field_map_tile_event_consumed_flags_ptr @ 0x53AD5` 指向章節事件的 once-only /
狀態旗標陣列。post-action 中唯一直接讀它的是 `fd2_chapter_29_post_action`：三個祭壇 tile
旗標 `flags[0x12] / [0x13] / [0x14]` 全 set → code=2（勝利），對應「啟動 3 個控制台」。
end handler 也用到（如 ch7_end 以 `flags[0x11]==1` 條件招募凱麗、ch26_end 以 `flags[0xC]`
決定寶箱路線 dialog page）。它同時是 FDFIELD 動態 turn-event 的 first-time gate（見下）。

## FDFIELD 事件派遣機制（canonical）

FD2 的章節事件**沒有自訂 bytecode interpreter**，而是「資料驅動的函式 dispatch」：
FDFIELD 每章 entry（entry index = `chapter_id*3+1`）內的 hook table 記錄
`(turn, event_code, phase)` 或 `(consequence_idx, event_type)` 元組，索引到一張 90-entry
函式指標表，指向已編譯的 cinematic C 函式。FDFIELD entry 的完整 131-byte header layout 見
`resource_info/fdfield.md`；此處記錄程式端如何讀取與派遣。

### 關鍵函數

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x1A30B` | `fd2_run_full_turn_cycle` | 6-phase 回合迴圈，內部呼叫下述兩個 dispatcher |
| `0x1A813` | `fd2_fire_chapter_turn_events_for_phase` | turn-event dispatcher |
| `0x13A44` | `fd2_check_tile_event_post_action` | tile-step latcher |
| `0x10C50` | `fd2_init_runtime_char_for_battle` | 由 char_spawn_record 建 runtime_char |

執行期的 FDFIELD entry 由 `data_fd2_tile_event_data_table_ptr @ 0x53A55` 指向。

### 90-entry consequence handler table（`0x51B91`）

`data_fd2_battle_ai_post_action_consequence_table @ 0x51B91`：90 entries（idx 0x00..0x59），
4-byte LE 函式指標，指向 `.text 0x34000-0x36100` 範圍的 `fd2_chapter_event_handler_*`
（`chevt1.c` / `chevt2.c`）。**雙重用途**：FDFIELD chapter event hook 與 tile-step
consequence 共用同一張表。turn-event 派遣時引數傳 0；tile-step consequence 派遣時傳
recipient char_idx。

### Turn-event hook（entry `+0x03..+0x32`）

16 × 3 bytes：`turn`（觸發回合）、`event_code`（索引 0x51B91 表）、`phase`
（0=enemy-turn 開場、1=end-of-player-turn、2=new-player-turn 開場）。
`fd2_fire_chapter_turn_events_for_phase(phase)` 掃 16 個 entry，凡
`turn == data_fd2_battle_turn_counter` 且 `phase` 相符者，呼叫
`consequence_table[event_code](0)`。

sentinel slot `(turn=0xFF, event_code=0xFF, phase=0)`：turn=0xFF 永不等於 turn counter
（最多 ~30），故不會 fire；多數 slot 是 sentinel。少數章節（ch27..30）有
`(turn=0xFF, event_code≠0xFF)` 形態——turn=0xFF 暫不匹配，但 event_code 有效，屬「動態啟動
候選」，由 tile-step handler 改寫 turn byte 後啟動（見下）。

### Tile-step-event hook（entry `+0x33..+0x52`）

16 × 2 bytes：`consequence_idx`（索引 0x51B91 表；0xFF=無效）、`event_type`（觸發 context
過濾：0=走入該 tile、1=在該 tile 行動）。char 移動/行動到有 terrain class 的 tile 時，
`fd2_check_tile_event_post_action(world_x, world_y, expected_event_type)` 讀 tile attribute，
以 `(terrain_class-1)` 為索引取該 tile-step hook；當 `consequence_idx != 0xFF` 且
`event_type` 與 expected 相符，就把值 latch 進 `data_fd2_battle_ai_post_action_consequence_idx`，
由接下來的 AI phase loop iteration 派遣 `consequence_table[consequence_idx]()`。

### 回合各 phase 觸發時點（per `fd2_run_full_turn_cycle`）

| 內部段 | 動作 | fire | turn_counter |
|---|---|---|---|
| player heal pass | — | — | N |
| end-of-player-turn | — | `fire(phase=1)` | N |
| NPC team turn | — | — | N |
| enemy banner + intro | — | `fire(phase=0)` | N |
| enemy team turn | — | — | N |
| 下一 player turn reveal | `turn_counter += 1` | `fire(phase=2)` | N+1 |

turn 1 沒有 phase 1/0 fire（沒有「上一回合 end」），但有 phase 2 fire
（`fd2_init_battle_state_for_chapter` 設 turn=1 後隨即進入 turn 1）。

### 動態 turn-event 啟動（ch27..30）

ch27..30 的「turn=0xFF 但 event_code≠0xFF」turn-event entries 是動態啟動候選，由該章
tile-step handler（category `state_machine_mutator`）改寫其 turn byte 觸發。以 ch27 為例，
`fd2_chapter_event_handler_3e__ch27_dyn_turn_event @ 0x35898` 在 tile 首次被踏（
`tile_event_consumed_flags[0x11] == 0`）時，把 `turn_counter + 1` 寫入
`tile_event_data_table[+3]`（turn-event hook 0 的 turn byte），使該 sentinel hook 變成
「下一個 player turn 才 fire」的 live hook，並設 `flags[0x11] = 1` 消費該 slot（once-only）。
被啟動的 hook 於下一回合 `fire(phase=...)` 觸發其 event_code handler，接續 cinematic 鏈；
ch28/29/30 以同型 state-machine-mutator handler 控制變身序列、final boss 分階段增援與過場
（各 handler 的 dialog page / flag 寫入詳見其 Ghidra plate comment）。

### 90-entry handler 對照表

命名規則：`fd2_chapter_event_handler_NN__chC_<purpose>` / `__shared_<purpose>` /
`__unref_<purpose>` / `__sentinel`。「refs」= 該 handler 被 FDFIELD hook 引用的次數。

| idx | addr | 名稱 | category | chapters | refs |
|---|---|---|---|---|---|
| 0x00 | `0x341DB` | `fd2_chapter_event_handler_00__ch1_dialog_with_state` | dialog_with_state | ch1 | 1 |
| 0x01 | `0x342B5` | `fd2_chapter_event_handler_01__ch1_dialog_with_state` | dialog_with_state | ch1 | 1 |
| 0x02 | `0x3431D` | `fd2_chapter_event_handler_02__ch1_dialog_with_state` | dialog_with_state | ch1 | 1 |
| 0x03 | `0x34377` | `fd2_chapter_event_handler_03__ch1_dialog_with_state` | dialog_with_state | ch1 | 1 |
| 0x04 | `0x343E2` | `fd2_chapter_event_handler_04__unref_dialog_with_state` | dialog_with_state | - | 0 |
| 0x05 | `0x34D68` | `fd2_chapter_event_handler_05__ch13_thunk` | thunk | ch13 | 1 |
| 0x06 | `0x34422` | `fd2_chapter_event_handler_06__ch2_reinforcement` | reinforcement_spawner | ch2 | 1 |
| 0x07 | `0x34D72` | `fd2_chapter_event_handler_07__ch13_dialog_with_state` | dialog_with_state | ch13 | 1 |
| 0x08 | `0x34DCD` | `fd2_chapter_event_handler_08__ch13_first_time` | first_time_gated | ch13 | 1 |
| 0x09 | `0x344C2` | `fd2_chapter_event_handler_09__ch3_char_cond` | char_conditional | ch3 | 1 |
| 0x0A | `0x34E3B` | `fd2_chapter_event_handler_0a__ch14_first_time` | first_time_gated | ch14 | 1 |
| 0x0B | `0x34565` | `fd2_chapter_event_handler_0b__ch4_dialog` | dialog_only | ch4 | 1 |
| 0x0C | `0x34594` | `fd2_chapter_event_handler_0c__unref_first_time` | first_time_gated | - | 0 |
| 0x0D | `0x34E90` | `fd2_chapter_event_handler_0d__ch15_dialog_with_state` | dialog_with_state | ch15 | 1 |
| 0x0E | `0x345EA` | `fd2_chapter_event_handler_0e__ch5_dialog_with_state` | dialog_with_state | ch5 | 1 |
| 0x0F | `0x3462E` | `fd2_chapter_event_handler_0f__ch5_dialog_with_state` | dialog_with_state | ch5 | 1 |
| 0x10 | `0x34696` | `fd2_chapter_event_handler_10__ch5_dialog` | dialog_only | ch5 | 1 |
| 0x11 | `0x346C8` | `fd2_chapter_event_handler_11__ch5_dialog_with_state` | dialog_with_state | ch5 | 1 |
| 0x12 | `0x34F02` | `fd2_chapter_event_handler_12__ch15_dialog_with_state` | dialog_with_state | ch15 | 1 |
| 0x13 | `0x34716` | `fd2_chapter_event_handler_13__unref_char_cond` | char_conditional | - | 0 |
| 0x14 | `0x347B1` | `fd2_chapter_event_handler_14__ch6_dialog` | dialog_only | ch6 | 1 |
| 0x15 | `0x347D9` | `fd2_chapter_event_handler_15__ch6_char_cond` | char_conditional | ch6 | 1 |
| 0x16 | `0x34819` | `fd2_chapter_event_handler_16__ch6_char_cond` | char_conditional | ch6 | 1 |
| 0x17 | `0x34844` | `fd2_chapter_event_handler_17__unref_turn_gated` | turn_conditional | - | 0 |
| 0x18 | `0x348FC` | `fd2_chapter_event_handler_18__unref_dialog` | dialog_only | - | 0 |
| 0x19 | `0x34924` | `fd2_chapter_event_handler_19__ch7_first_time` | first_time_gated | ch7 | 1 |
| 0x1A | `0x3499B` | `fd2_chapter_event_handler_1a__ch7_char_cond` | char_conditional | ch7 | 1 |
| 0x1B | `0x349D9` | `fd2_chapter_event_handler_1b__ch8_cinematic` | cinematic_no_dialog | ch8 | 6 |
| 0x1C | `0x34A0E` | `fd2_chapter_event_handler_1c__ch8_ai_ctrl` | ai_setup | ch8 | 1 |
| 0x1D | `0x34A3C` | `fd2_chapter_event_handler_1d__unref_dialog_with_state` | dialog_with_state | - | 0 |
| 0x1E | `0x34A7A` | `fd2_chapter_event_handler_1e__unref_major_cinematic` | major_endgame_cinematic | - | 0 |
| 0x1F | `0x34B5D` | `fd2_chapter_event_handler_1f__ch9_reinforcement` | reinforcement_spawner | ch9 | 2 |
| 0x20 | `0x34BE2` | `fd2_chapter_event_handler_20__ch10_dialog` | dialog_only | ch10 | 1 |
| 0x21 | `0x34C1E` | `fd2_chapter_event_handler_21__ch10_dialog_with_state` | dialog_with_state | ch10 | 1 |
| 0x22 | `0x34C6C` | `fd2_chapter_event_handler_22__unref_dialog` | dialog_only | - | 0 |
| 0x23 | `0x34C76` | `fd2_chapter_event_handler_23__ch12_cinematic` | cinematic_no_dialog | ch12 | 1 |
| 0x24 | `0x34CB3` | `fd2_chapter_event_handler_24__ch12_ai_ctrl` | ai_setup | ch12 | 1 |
| 0x25 | `0x34CCC` | `fd2_chapter_event_handler_25__unref_major_cinematic` | major_endgame_cinematic | - | 0 |
| 0x26 | `0x34F42` | `fd2_chapter_event_handler_26__ch15_dialog` | dialog_only | ch15 | 1 |
| 0x27 | `0x34F74` | `fd2_chapter_event_handler_27__unref_drop` | drop_dialog | - | 0 |
| 0x28 | `0x34FCB` | `fd2_chapter_event_handler_28__ch17_dialog_with_state` | dialog_with_state | ch17 | 1 |
| 0x29 | `0x34FF0` | `fd2_chapter_event_handler_29__unref_drop` | drop_dialog | - | 0 |
| 0x2A | `0x3505F` | `fd2_chapter_event_handler_2a__ch18_dialog` | dialog_only | ch18 | 1 |
| 0x2B | `0x35091` | `fd2_chapter_event_handler_2b__ch18_ai_ctrl` | ai_setup | ch18 | 1 |
| 0x2C | `0x350A4` | `fd2_chapter_event_handler_2c__ch19_ai_ctrl` | ai_setup | ch19 | 1 |
| 0x2D | `0x350B9` | `fd2_chapter_event_handler_2d__ch19_ai_ctrl` | ai_setup | ch19 | 1 |
| 0x2E | `0x350CC` | `fd2_chapter_event_handler_2e__ch19_reinforcement` | reinforcement_spawner | ch19 | 1 |
| 0x2F | `0x35112` | `fd2_chapter_event_handler_2f__ch21_turn_gated` | turn_conditional | ch21 | 4 |
| 0x30 | `0x351C6` | `fd2_chapter_event_handler_30__ch21_ai_ctrl` | ai_setup | ch21 | 1 |
| 0x31 | `0x351E9` | `fd2_chapter_event_handler_31__ch22_turn_gated` | turn_conditional | ch22 | 2 |
| 0x32 | `0x35261` | `fd2_chapter_event_handler_32__ch22_reinforcement` | reinforcement_spawner | ch22 | 1 |
| 0x33 | `0x3529A` | `fd2_chapter_event_handler_33__unref_drop` | drop_dialog | - | 0 |
| 0x34 | `0x352E2` | `fd2_chapter_event_handler_34__ch23_ai_ctrl` | ai_setup | ch23 | 4 |
| 0x35 | `0x35321` | `fd2_chapter_event_handler_35__unref_dialog_with_state` | dialog_with_state | - | 0 |
| 0x36 | `0x3535D` | `fd2_chapter_event_handler_36__ch24_cinematic` | cinematic_no_dialog | ch24 | 4 |
| 0x37 | `0x353DA` | `fd2_chapter_event_handler_37__ch25_first_time` | first_time_gated | ch25 | 1 |
| 0x38 | `0x35487` | `fd2_chapter_event_handler_38__ch25_dialog_with_state` | dialog_with_state | ch25 | 1 |
| 0x39 | `0x354DD` | `fd2_chapter_event_handler_39__ch26_cinematic` | cinematic_no_dialog | ch26 | 9 |
| 0x3A | `0x354FE` | `fd2_chapter_event_handler_3a__unref_pickup` | item_pickup | - | 0 |
| 0x3B | `0x35641` | `fd2_chapter_event_handler_3b__ch26_ai_ctrl` | ai_setup | ch26 | 1 |
| 0x3C | `0x35675` | `fd2_chapter_event_handler_3c__ch26_ai_ctrl` | ai_setup | ch26 | 1 |
| 0x3D | `0x356B7` | `fd2_chapter_event_handler_3d__ch26_pickup` | item_pickup | ch26 | 1 |
| 0x3E | `0x35898` | `fd2_chapter_event_handler_3e__ch27_dyn_turn_event` | state_machine_mutator | ch27 | 1 |
| 0x3F | `0x358C7` | `fd2_chapter_event_handler_3f__ch27_cinematic` | ai_setup | ch27 | 1 |
| 0x40 | `0x358EA` | `fd2_chapter_event_handler_40__unref_dyn_turn_event` | state_machine_mutator | - | 0 |
| 0x41 | `0x3599B` | `fd2_chapter_event_handler_41__shared_dyn_turn_event` | state_machine_mutator | ch27, ch28 | 2 |
| 0x42 | `0x359C8` | `fd2_chapter_event_handler_42__ch28_dialog_with_state` | dialog_with_state | ch28 | 1 |
| 0x43 | `0x35A2F` | `fd2_chapter_event_handler_43__unref_dyn_turn_event` | state_machine_mutator | - | 0 |
| 0x44 | `0x35A48` | `fd2_chapter_event_handler_44__ch28_dialog_with_state` | dialog_with_state | ch28 | 1 |
| 0x45 | `0x35AB8` | `fd2_chapter_event_handler_45__ch28_dyn_turn_event` | state_machine_mutator | ch28 | 1 |
| 0x46 | `0x35B05` | `fd2_chapter_event_handler_46__ch28_dialog_with_state` | dialog_with_state | ch28 | 1 |
| 0x47 | `0x35B6B` | `fd2_chapter_event_handler_47__unref_dyn_turn_event` | state_machine_mutator | - | 0 |
| 0x48 | `0x35BF2` | `fd2_chapter_event_handler_48__unref_portrait_cinematic_pair` | ai_setup | - | 0 |
| 0x49 | `0x35C23` | `fd2_chapter_event_handler_49__unref_sentinel` | sentinel | - | 0 |
| 0x4A | `0x35C32` | `fd2_chapter_event_handler_4a__ch29_dyn_turn_event` | state_machine_mutator | ch29 | 1 |
| 0x4B | `0x35C79` | `fd2_chapter_event_handler_4b__ch29_major_cinematic` | major_endgame_cinematic | ch29 | 1 |
| 0x4C | `0x35D60` | `fd2_chapter_event_handler_4c__ch29_major_cinematic` | major_endgame_cinematic | ch29 | 1 |
| 0x4D | `0x35EBE` | `fd2_chapter_event_handler_4d__unref_sentinel` | sentinel | - | 0 |
| 0x4E | `0x35ED2` | `fd2_chapter_event_handler_4e__unref_sentinel` | sentinel | - | 0 |
| 0x4F | `0x35EE6` | `fd2_chapter_event_handler_4f__ch29_dyn_turn_event` | state_machine_mutator | ch29 | 1 |
| 0x50 | `0x35F5A` | `fd2_chapter_event_handler_50__ch30_ai_ctrl` | ai_setup | ch30 | 1 |
| 0x51 | `0x35F6F` | `fd2_chapter_event_handler_51__unref_dyn_turn_event` | state_machine_mutator | - | 0 |
| 0x52 | `0x35F92` | `fd2_chapter_event_handler_52__ch30_major_cinematic` | major_endgame_cinematic | ch30 | 1 |
| 0x53 | `0x36088` | `fd2_chapter_event_handler_53__unref_dialog_with_state` | dialog_with_state | - | 0 |
| 0x54 | `0x360C0` | `fd2_chapter_event_handler_54__ch27_ai_ctrl` | ai_setup | ch27 | 1 |
| 0x55 | `0x360D8` | `fd2_chapter_event_handler_55__unref_sentinel` | sentinel | - | 0 |
| 0x56 | `0x360E3` | `fd2_chapter_event_handler_56__unref_sentinel` | sentinel | - | 0 |
| 0x57 | `0x360EA` | `fd2_chapter_event_handler_57__unref_sentinel` | sentinel | - | 0 |
| 0x58 | `0x360F1` | `fd2_chapter_event_handler_58__unref_sentinel` | sentinel | - | 0 |
| 0x59 | `0x360F8` | `fd2_chapter_event_handler_59__unref_sentinel` | sentinel | - | 0 |

### 結構性發現

1. **大部分 handler 走 `data_fd2_current_chapter_text_ptr`**：呼
   `fd2_display_dialog_scene(data_fd2_current_chapter_text_ptr, page_id, ...)`，FDTXT 入口
   idx 於 chapter init 時設定；同一 handler 若被 N 章共用會產生 N 組 page→scene 對映。
2. **idx 0x3A 例外用 `all_game_text`**：唯一的 pickup handler，呼
   `fd2_display_dialog_scene(all_game_text, page=0x1E0)`（背包滿）與 page 0x1A6（拾取成功）。
3. **`unref_*` handler 為 cut content**：未被任何章 FDFIELD turn-event / tile-step hook
   引用（refs=0），binary 內僅出現兩次（LE reloc fixup + 0x51B91 表項）。其 cut-feature
   分布：

   | category | 含義 |
   |---|---|
   | sentinel | 7-byte 空 stub（`__CHK` + `RET`）；預留 table slot。idx 0x49 設 `flags[0x12]=1` 是唯一含寫入的 sentinel |
   | state_machine_mutator | 含 `flags[0x10]++` + turn-event 動態啟動邏輯；cut 章節的 dyn-turn-event 觸發 |
   | dialog_with_state | 純 dialog page + state mutation；cut dialog branch |
   | drop_dialog | 背包滿 / 拾取成功 dialog；cut item drop |
   | major_endgame_cinematic | 完整 cinematic（char spawn / dialog / portrait load）；cut endgame variant |
   | dialog_only | 純 dialog page；cut 場景 |
   | first_time_gated / char_conditional / turn_conditional / item_pickup / ai_setup | 各 1 個對應 cut 事件 |

   cut content 集中在 endgame（idx ≥ 0x4D 連續 5 個 sentinel 0x55..0x59 + 散布的
   0x49/0x4D/0x4E），顯示 dispatch 表預留了更多 endgame variant slot，發行版只用了部分。
   idx 0x05（`ch13_thunk`）是 `fd2_chapter_event_handler_07__ch13_dialog_with_state` 的 thunk。

## 跨章節隱藏機制鏈

conditional recruit、reward、Good/Bad ending fork 等跨章節劇情機制的完整鏈路（如
ch21_end 收齊 6 件 0xD1..0xD6 → 給天空之鑰、ch23/27_end 以是否持天空之鑰決定 good/bad
path、ch30_end 擊殺空魔神 → epilogue good ending）正典在 `assets/chapters/_index.md`。
