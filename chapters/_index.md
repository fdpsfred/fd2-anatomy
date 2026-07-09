# chapters/ — 30 章唯一文件

每章一份 `chapter_NN.md`，統一模板：劇情概要 → 加入角色 → 敵人配置 → 寶物 → 商店（部分章） →
特殊機制 → Handler 流程（Function 位址 / Init / Dialog page / char_id / Cutscene / Post-action /
End） → FDFIELD event script → 對話（FDTXT transcript）。

本 `_index.md` 是跨章節事實的唯一正典：handler 總表、共用 handler 組、跨章隱藏機制鏈（天空之鑰、
條件招募、隱藏獎勵、結局分歧），以及章號 ↔ FDFIELD entry ↔ FDTXT entry 對照。

## 30 章 handler 總表

`chapter_id` 為 0-based（ch1 = 0）。post_action 欄的 `char[N]` 是 `runtime_char_array` 的 slot
index（非 char_id），實際對應角色隨章而異，詳見各章檔與下方「跨章隱藏機制鏈」。

| 章 | init handler | end handler | post_action |
|---|---|---|---|
| 1  | `fd2_chapter_01_init @ 0x3231B`（最大，含 prologue） | `fd2_chapter_01_end @ 0x22EF6` | default |
| 2  | `fd2_chapter_02_init @ 0x32D18` | `fd2_chapter_02_end @ 0x22F37` | `fd2_chapter_02_post_action @ 0x206C5`（chars[5..10] survival） |
| 3  | `fd2_chapter_03_init @ 0x32E8C` | `fd2_chapter_03_end @ 0x230F2` | default |
| 4  | `fd2_chapter_04_init @ 0x32FB2` | `fd2_chapter_04_end @ 0x231BC` | default |
| 5  | `fd2_chapter_05_init @ 0x33049` | `fd2_chapter_05_end @ 0x231F9` | default |
| 6  | `fd2_chapter_06_init @ 0x3314B` | `fd2_chapter_06_end @ 0x23296` | default |
| 7  | `fd2_chapter_07_init @ 0x33169` | `fd2_chapter_07_end @ 0x232E8` | default |
| 8  | `fd2_chapter_08_init @ 0x33219` | `fd2_chapter_08_end @ 0x234BB` | default |
| 9  | `fd2_chapter_09_init @ 0x3327D` | `fd2_chapter_09_end @ 0x235BC` | default |
| 10 | `fd2_chapter_10_init @ 0x3332B` | `fd2_chapter_10_end @ 0x235F9` | `fd2_chapter_10_post_action @ 0x20707`（chars[0x32]/[0x33]） |
| 11 | `fd2_chapter_11_init @ 0x33367` | `fd2_chapter_11_end @ 0x23790` | default |
| 12 | `fd2_chapter_12_init @ 0x333F5` | `fd2_chapter_12_end @ 0x237D5` | `fd2_chapter_12_post_action`（char[0xE]） |
| 13 | `fd2_chapter_13_init @ 0x3346B`（最小 17 B） | `fd2_chapter_13_end @ 0x2389F` | `fd2_chapter_13_post_action @ 0x20765`（189 B，最複雜） |
| 14 | `fd2_chapter_14_init @ 0x3347C` | `fd2_chapter_14_end @ 0x238DC` | default |
| 15 | `fd2_chapter_15_init @ 0x334D9` | `fd2_chapter_15_end @ 0x239BD` | `fd2_chapter_15_post_action @ 0x20822`（char[0x40]） |
| 16 | `fd2_chapter_16_init @ 0x335A0` | `fd2_chapter_16_end @ 0x23A0A` | `fd2_chapter_16_post_action`（char[0x41]） |
| 17 | `fd2_chapter_17_init @ 0x335AA` | `fd2_chapter_17_end @ 0x23B5F` | `fd2_chapter_17_post_action`（gated lose） |
| 18 | `fd2_chapter_18_init @ 0x335DA` | `fd2_chapter_18_end @ 0x23CD5` | `fd2_chapter_18_post_action`（char[0x34] boss-kill = win） |
| 19 | `fd2_chapter_19_20_21_init_shared @ 0x33674`（三章共用） | `fd2_chapter_19_end @ 0x23E39` | `fd2_chapter_19_post_action @ 0x20926`（gated lose char[0x40]） |
| 20 | （共用 init） | `fd2_chapter_20_end @ 0x23E74`（646 B） | `fd2_chapter_20_post_action @ 0x20957`（多陣營：殲滅敵組 = 勝、char[0]/[0x34] 或 NPC 組全滅 = 敗） |
| 21 | （共用 init） | `fd2_chapter_21_end @ 0x240FA`（572 B） | `fd2_chapter_21_post_action @ 0x20A51`（char[0x10]/[0x11]） |
| 22 | `fd2_chapter_22_init @ 0x3367E` | `fd2_chapter_22_end @ 0x244B6`（白屏 fade-to-black） | `fd2_chapter_22_27_28_post_action_shared @ 0x20A87`（char[1]） |
| 23 | `fd2_chapter_23_init @ 0x336A0`（548 B，最大） | `fd2_chapter_23_end @ 0x24754`（960 B，最大；Phase 2 mid-handler reload FDFIELD.DAT 0x45 切第二戰場） | `fd2_chapter_23_post_action @ 0x20AAF`（保護 0/1/0x10/0x11 + 殺 boss[0x12] = 勝） |
| 24 | `fd2_chapter_24_init @ 0x338C4` | `fd2_chapter_24_end @ 0x24C1E`（text-scroll cinematic） | default |
| 25 | `fd2_chapter_25_init @ 0x3396A` | `fd2_chapter_25_end @ 0x24DF2` | `fd2_chapter_25_post_action @ 0x20B14`（char[0x10]） |
| 26 | `fd2_chapter_26_init @ 0x33AAE` | `fd2_chapter_26_end @ 0x24E80` | `fd2_chapter_26_post_action @ 0x20B3C`（char[1]/[2]） |
| 27 | `fd2_chapter_27_init @ 0x33AF1` | `fd2_chapter_27_end @ 0x250CC`（920 B，GOOD/BAD fork） | `fd2_chapter_22_27_28_post_action_shared @ 0x20A87`（char[1]） |
| 28 | `fd2_chapter_28_init @ 0x33C9D` | `fd2_chapter_28_end @ 0x25464` | `fd2_chapter_22_27_28_post_action_shared @ 0x20A87`（char[1]） |
| 29 | `fd2_chapter_29_init @ 0x33DBA` | `fd2_chapter_29_end @ 0x2548C` | `fd2_chapter_29_post_action`（tile_event win） |
| 30 | `fd2_chapter_30_init @ 0x33E3C` | `fd2_chapter_30_end @ 0x25757`（544 B，GOOD ENDING） | `fd2_chapter_30_post_action`（char[0x14] = win） |

### 共用 handler 組

- **ch19/20/21 init**：共用 `fd2_chapter_19_20_21_init_shared @ 0x33674`（10 B stub）；三章開場流程完全相同，差異全在 post-action handler 與各自的 FDFIELD entry。
- **ch22/27/28 post_action**：共用 `fd2_chapter_22_27_28_post_action_shared @ 0x20A87`（default + 保護 slot 1）。三章共用的是 slot-1 檢查結構，**不是**同一角色：ch22 的 chars[1] 是希爾法、ch27/28 的 chars[1] 是悠妮（見下方招募矩陣）。
- **default post_action**：ch1、ch3..ch9、ch11、ch14、ch24 走 `fd2_check_battle_end_default_handler @ 0x205B4`（全敵死 = 勝、索爾 char_id 0 死 = 負）。共 11 章（dispatch table @ 0x51B19 entry 0/2/3/4/5/6/7/8/10/13/23）。

## 30 章內容總表

| 章 | 章名 | binary 加入角色 (char_id) | 特殊機制亮點 |
|---|---|---|---|
| 01 | 初試身手 | 0/9/4/30（索爾/悠妮預/亞雷斯/蓋亞）、1（哈諾 turn-3） | 30 章唯一 prologue（Phase A–D 四段）；哈瓦特暴走 |
| 02 | 羅德鎮 | end +8（希莉亞） | 6 村民全活 → item 0xC6 力量藥水 |
| 03 | 往塞拉村途中 | end +2（鐵諾，char[6] 存活條件） | 條件式招募 |
| 04 | 塞拉村前 | （無） | 過場章 |
| 05 | 塞拉村 | end +10（瑪琳） | — |
| 06 | 普里茲港 | end +13（貝克威） | 極簡 init |
| 07 | 往王城的途中 | end +12（凱麗，雙條件） | 凱麗 = char 0xC，武者，tile_event[0x11] AND char[0x2B] 存活 |
| 08 | 王城前的戰鬥 | end +5（洛娜） | 每回合騎兵援軍 6 波；章末黑屏 fade |
| 09 | 騎士的抉擇 | end revive char[11] | 萊汀敗 → 援軍 state machine（flag[0x10]） |
| 10 | 洞窟中的激戰 | end +11 +6（索菲亞 + 萊汀） | 兩 NPC 起始麻痹：char[0x32] 卡納恩三世、char[0x33] 索菲亞 |
| 11 | 幻之森林 | end +14（珊） | — |
| 12 | 北山道 | end +17（米亞斯多德） | cutscene 先於 dialog；保護 char[0xE] |
| 13 | 哈斯米爾之戰 | end +3（哈瓦特） | init 17 B / post_action 189 B 工作量倒置 |
| 14 | 平原的會戰 | （無） | 位置觸發援軍（tile-step） |
| 15 | 拉卡湖的激戰 | end +15（塞可邦勒） | 首章 conditional dialog（含凱麗切兩線） |
| 16 | 冰原之戰 | end +18（蜜蒂，三條件） | HP_max>319 AND 18 回合內 AND 部下死 ≤ 4 |
| 17 | 血與冰之刃 | end +16（凱拉斯） | conditional dialog（是否有蜜蒂） |
| 18 | 遙遠的彼岸 | end +21 +7（約拿 + 蘭斯洛特） | boss-kill = win（黑暗騎士 char[0x34]） |
| 19 | 黑暗中的狙擊 | FDFIELD +0x40（巴拿羅西亞 turn-6） | 三章共用 init（ch19/20/21） |
| 20 | 死亡般的沈寂 | end +25 +28（謝多 + 達克塞條件） | 達克塞：15 回合內結束（`data_fd2_battle_turn_counter < 16`） |
| 21 | 亞述森林 | end +24 +23（希爾法 + 羅蘭） | 🔑 6 件收集 → item 100 天空之鑰 |
| 22 | 遠古呼喚 | FDFIELD（莎拉） | 白屏 fade-to-black 收尾（30 章唯一） |
| 23 | 向天空之旅 | end +22（卡里斯，天空之鑰） +19（羅德曼條件） | 最大 init+end；章內 mid-handler reload 進第二戰場 |
| 24 | 在天空的彼方 | （無） | init 4-stage camera；end text-scroll cinematic（30 章唯一） |
| 25 | 火焰的審判 | end +26 +29（聖寇拉斯 + 亞齊梅吉） | init 4 連震 cutscene；亞齊梅吉在 save 後 init（不進 template） |
| 26 | 未知的迴廊 | FDFIELD（渥德） | 5 寶箱選 1 切 5 路線 dynamic dialog |
| 27 | 命運的交會點 | （無） | 🔑 GOOD/BAD ENDING FORK（`fd2_any_char_has_item(100)`） |
| 28 | 探索者 | （無） | 全清隊 + revive；3× cutscene 0x55 |
| 29 | 無邊的黑暗之中 | （無） | 30 章唯一 tile_event win；char[0x14] 變身 |
| 30 | 傳說的終章－結局 | （無） | 🏆 GOOD ENDING + staff roll（epilogue map + `fd2_play_game_ending_cinematic`） |

譯名說明：char 15 名表正名「塞可邦勒」（對白變體「賽可邦勒」）、char 28「達克塞」、char 29 名表正名
「亞齊梅吉」（對白變體「亞奇梅吉」）；各章 roster/機制欄採名表正名，對白 transcript 維持原文變體。
完整異名對照與正名依據見 `assets/names.md`。

## 跨章節隱藏機制鏈

### Good / Bad Ending 分歧

```
ch21_end：收齊 6 件（黃金徽章 0xD1 + 5 顆眼 0xD2..0xD6）
  → fd2_give_item_to_first_player_char(100 = 天空之鑰 = 0x64)
  ↓
ch23_end：fd2_any_char_has_item(100)
  → 持有 → fd2_init_runtime_char_from_base_growth(0x16 = 22 = 卡里斯) 加入（GOOD PATH）
  ↓
ch27_end：fd2_any_char_has_item(100)
  ├─ 持有 → 續進 ch28 以後（GOOD PATH）
  └─ 未持有 → fd2_animate_warp_teleport_char(悠妮) + fd2_play_game_ending_cinematic + 無限迴圈（BAD ENDING）
  ↓
ch28–29 戰鬥序列
  ↓
ch30 戰鬥勝利：fd2_chapter_30_post_action @ 0x20BF5 殺空魔神（char[0x14]） → game_event_flag = 2
  → ch30_end @ 0x25757：fd2_load_chapter_battle_data(0x1E) 載入 epilogue map + fd2_play_game_ending_cinematic + 無限迴圈（🏆 GOOD ENDING）
```

### 條件招募矩陣

runtime 回合計數為 `data_fd2_battle_turn_counter`（螢幕「TURN N」顯示值，1 起算）。

| 章 | 角色 (char_id) | 條件 |
|---|---|---|
| ch3  | char 2 = 鐵諾 | char[6] 存活 |
| ch7  | char 12 (0xC) = 凱麗（武者） | tile_event[0x11] == 1 AND char[0x2B] 存活 |
| ch16 | char 18 = 蜜蒂 | chars[0].HP_max > 319 AND `data_fd2_battle_turn_counter < 0x13`（18 回合內）AND chars[0x42..0x49] 死 ≤ 4 |
| ch20 | char 28 = 達克塞 | `data_fd2_battle_turn_counter < 16`（15 回合內結束） |
| ch23 | char 22 = 卡里斯 | `fd2_any_char_has_item(100 = 天空之鑰)` |
| ch23 | char 19 = 羅德曼 | `fd2_find_template_char_by_id(0x12 = 蜜蒂) == 0`（蜜蒂不在）AND `data_fd2_battle_turn_counter < 15` |

### 隱藏獎勵 / 兌換

| 章 | 條件 | 獎勵 |
|---|---|---|
| ch2  | 6 名村民全活（chars[5..10] 無死亡） | item 0xC6 = 力量藥水（AP+9） |
| ch21 | 持有 item 0xD1..0xD6 全 6 件（黃金徽章 + 5 顆眼） | item 100 (0x64) = 天空之鑰 |

## 章號 ↔ FDFIELD entry ↔ FDTXT entry 對照

- **FDFIELD**（`chapter_id = 章號 − 1`）：`tile_map = chapter_id×3`、`tile_event = chapter_id×3 + 1`、
  `char_spawn_pos = chapter_id×3 + 2`。tile_event entry 內含 char_spawn_records、turn/tile-step/pickup
  hook，layout 正典見 `resource_info/fdfield.md`。
- **FDTXT**：對白 entry = 章號（`chapter_id + 1`），編碼與 dump 見 `resource_info/fdtxt.md`。
- prologue 例外：ch1 的 prologue 另用 FDTXT entry 33（Phase A+B）與 entry 32（Phase C），詳 `chapter_01.md`。

| 章 | FDFIELD tile_map | FDFIELD tile_event | FDFIELD spawn_pos | FDTXT entry |
|---|---|---|---|---|
| 1  | 0  | 1  | 2  | 1  |
| 2  | 3  | 4  | 5  | 2  |
| 3  | 6  | 7  | 8  | 3  |
| 4  | 9  | 10 | 11 | 4  |
| 5  | 12 | 13 | 14 | 5  |
| 6  | 15 | 16 | 17 | 6  |
| 7  | 18 | 19 | 20 | 7  |
| 8  | 21 | 22 | 23 | 8  |
| 9  | 24 | 25 | 26 | 9  |
| 10 | 27 | 28 | 29 | 10 |
| 11 | 30 | 31 | 32 | 11 |
| 12 | 33 | 34 | 35 | 12 |
| 13 | 36 | 37 | 38 | 13 |
| 14 | 39 | 40 | 41 | 14 |
| 15 | 42 | 43 | 44 | 15 |
| 16 | 45 | 46 | 47 | 16 |
| 17 | 48 | 49 | 50 | 17 |
| 18 | 51 | 52 | 53 | 18 |
| 19 | 54 | 55 | 56 | 19 |
| 20 | 57 | 58 | 59 | 20 |
| 21 | 60 | 61 | 62 | 21 |
| 22 | 63 | 64 | 65 | 22 |
| 23 | 66 | 67 | 68 | 23 |
| 24 | 69 | 70 | 71 | 24 |
| 25 | 72 | 73 | 74 | 25 |
| 26 | 75 | 76 | 77 | 26 |
| 27 | 78 | 79 | 80 | 27 |
| 28 | 81 | 82 | 83 | 28 |
| 29 | 84 | 85 | 86 | 29 |
| 30 | 87 | 88 | 89 | 30 |

endgame cinematic 另有 FDFIELD idx 90..98（epilogue map 等），詳 `resource_info/fdfield.md`。
