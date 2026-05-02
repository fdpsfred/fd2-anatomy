# program_info/chapters/

每章 init/end handler 函數流程。30 章對照總表 (含 init/end 大小、post_action
種類、共用 handler 組)：

| ID | init handler | size | end handler | size | post_action |
|---|---|---|---|---|---|
| 1  | `chapter_01_init @ 0x0003231B` | 含 prologue | `chapter_01_end @ 0x00022EF6` | 65 | default |
| 2  | `chapter_02_init @ 0x00032D18` | 402 | `chapter_02_end @ 0x00022F37` | 443 | char[5..10] survival |
| 3  | `chapter_03_init @ 0x00032E8C` | 324 | `chapter_03_end @ 0x000230F2` | 214 | default |
| 4  | `chapter_04_init @ 0x00032FB2` | 181 | `chapter_04_end @ 0x000231BC` | 61  | default |
| 5  | `chapter_05_init @ 0x00033049` | 258 | `chapter_05_end @ 0x000231F9` | 157 | default |
| 6  | `chapter_06_init @ 0x0003314B` | 79  | `chapter_06_end @ 0x00023296` | 82  | default |
| 7  | `chapter_07_init @ 0x00033169` | 176 | `chapter_07_end @ 0x000232E8` | 222 | default |
| 8  | `chapter_08_init @ 0x00033219` | 100 | `chapter_08_end @ 0x000234BB` | 140 | default |
| 9  | `chapter_09_init @ 0x0003327D` | 174 | `chapter_09_end @ 0x000235BC` | 61  | default |
| 10 | `chapter_10_init @ 0x0003332B` | 90  | `chapter_10_end @ 0x000235F9` | 407 | char[0x32, 0x33] |
| 11 | `chapter_11_init @ 0x00033367` | 142 | `chapter_11_end @ 0x00023790` | 69  | default |
| 12 | `chapter_12_init @ 0x000333F5` | 118 | `chapter_12_end @ 0x000237D5` | 214 | char[0xE] |
| 13 | `chapter_13_init @ 0x0003346B` | 17  | `chapter_13_end @ 0x0002389F` | 61  | 189B 最複雜 |
| 14 | `chapter_14_init @ 0x0003347C` | 29  | `chapter_14_end @ 0x000238DC` | 225 | default |
| 15 | `chapter_15_init @ 0x000334D9` | 199 | `chapter_15_end @ 0x000239BD` | 77  | char[0x40] |
| 16 | `chapter_16_init @ 0x000335A0` | 10  | `chapter_16_end @ 0x00023A0A` | 341 | char[0x41] |
| 17 | `chapter_17_init @ 0x000335AA` | 48  | `chapter_17_end @ 0x00023B5F` | 374 | gated lose |
| 18 | `chapter_18_init @ 0x000335DA` | 154 | `chapter_18_end @ 0x00023CD5` | 356 | bypass default |
| 19-21 | `chapter_19_20_21_init_shared @ 0x00033674` | 10 (三章共用) | — | — | — |
| 19 | (shared) | — | `chapter_19_end @ 0x00023E39` | 59  | gated by save_metadata |
| 20 | (shared) | — | `chapter_20_end @ 0x00023E74` | 646 | 250B 3-stage |
| 21 | (shared) | — | `chapter_21_end @ 0x000240FA` | 572 | char[0x10, 0x11] |
| 22 | `chapter_22_init @ 0x0003367E` | 34  | `chapter_22_end @ 0x000244B6` | 354 | char[1] (shared with ch27/28) |
| 23 | `chapter_23_init @ 0x000336A0` | 548 | `chapter_23_end @ 0x00024754` | 960 | bypass default |
| 24 | `chapter_24_init @ 0x000338C4` | 166 | `chapter_24_end @ 0x00024C1E` | 260 | default |
| 25 | `chapter_25_init @ 0x0003396A` | 324 | `chapter_25_end @ 0x00024DF2` | 142 | char[0x10] |
| 26 | `chapter_26_init @ 0x00033AAE` | 67  | `chapter_26_end @ 0x00024E80` | 466 | char[1, 2] |
| 27 | `chapter_27_init @ 0x00033AF1` | 428 | `chapter_27_end @ 0x000250CC` | 920 | char[1] (shared) |
| 28 | `chapter_28_init @ 0x00033C9D` | 285 | `chapter_28_end @ 0x00025464` | 40  | char[1] (shared) |
| 29 | `chapter_29_init @ 0x00033DBA` | 130 | `chapter_29_end @ 0x0002548C` | 451 | bypass; tile_event win |
| 30 | `chapter_30_init @ 0x00033E3C` | 316 | `chapter_30_end @ 0x00025757` | 544 | char[0x14] = win |

## 共用 handler 組

- **ch19/ch20/ch21**：共用 `chapter_19_20_21_init_shared @ 0x00033674` (10B stub)
- **ch22/ch27/ch28 post_action**：共用 `chapter_22_27_28_post_action_shared @ 0x00020A87`
  (檢查 char[1] 死亡)
- **ch1, ch3..ch9, ch11, ch14, ch24 post_action**：均為 `check_battle_end_default_handler @ 0x000205B4`

## 檔案

`chapter_01.md` ... `chapter_30.md` — 各章 handler 流程完整描述。
