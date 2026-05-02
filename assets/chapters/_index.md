# assets/chapters/

30 章劇情 / 招募 / 敵人 / 機制 + 完整對話。每章一份 `chapter_NN.md`。

## 30 章對照總表

| 章 | 章名 | 加入角色 (binary) | 攻略「加入」 | 特殊機制 |
|---|---|---|---|---|
| 01 | 初試身手   | 0/9/4/30 (索爾/悠妮預/亞雷斯/蓋亞) | 戰士哈諾 (FDFIELD turn-3) | 30 章唯一 prologue (3-phase) |
| 02 | 羅德鎮     | end +8 (希莉亞)        | 弓兵希莉亞                | 6 villagers 全活 → item 0xC6 (力量藥水) reward |
| 03 | 往塞拉村途中 | end +2 (鐵諾, char[6] alive 條件) | 劍士鐵諾  | conditional recruit |
| 04 | 塞拉村前   | (無)                   | (無)                      | trivial end |
| 05 | 塞拉村     | end +10 (瑪琳)         | 僧侶瑪琳                  | — |
| 06 | 普里茲港   | end +13 (貝克威)       | 弓兵貝克威                | minimal init |
| 07 | 往王城的途中 | end +12 (凱麗, dual-conditional) | 武者凱麗 | dual-conditional recruit (tile_event[0x11] AND char[0x2B] alive) |
| 08 | 王城前的戰鬥 | end +5 (洛娜)         | 騎士洛娜                  | end framebuffer fade-to-black |
| 09 | 騎士的抉擇 | end revive char[11]    | (無)                      | init 11 chars 全面朝 north |
| 10 | 洞窟中的激戰 | end +11 +6 (索菲亞 + 萊汀) | 騎士萊汀、僧侶索菲亞 | init 兩 NPC 起始 sleep, end 大型轉場 |
| 11 | 幻之森林   | end +14 (珊)           | 魔法師珊                  | — |
| 12 | 北山道     | end +17 (米亞斯多德)   | 龍劍士米亞斯多德          | cutscene 在 dialog 之前先觸發 |
| 13 | 哈斯米爾之戰 | end +3 (哈瓦特)       | 戰士哈瓦諾 (攻略筆誤 = 哈瓦特) | init 最小 17B, post_action 最複雜 189B |
| 14 | 平原的會戰 | (無)                   | (無)                      | — |
| 15 | 拉卡湖的激戰 | end +15 (賽可邦勒)   | 武者賽可邦勒              | 首章 conditional dialog: 隊上有凱麗 → page 0/1/2; 無 → 3/4/5 |
| 16 | 冰原之戰   | end +18 (蜜蒂, 三條件) | 蜜蒂 (HP320+18 回合內+部下死≤4) | FD2 最複雜 end conditional |
| 17 | 血與冰之刃 | end +16 (凱拉斯)       | 龍劍士凱拉斯              | conditional dialog: 有蜜蒂 → page 5; 無 → page 7 + 蜜蒂告別 cutscene |
| 18 | 遙遠的彼岸 | end +21 +7 (約拿+蘭斯洛特) | 聖者約拿、聖騎士蘭斯洛特 | 首章 boss-kill = win (黑暗騎士 = char[0x34]) |
| 19 | 黑暗中的狙擊 | (無)                  | 龍劍士巴拿羅西亞 (FDFIELD) | **3 章共用 init** (ch19/20/21) |
| 20 | 死亡般的沈寂 | end +25 +28 (謝多+達可賽 conditional) | 忍者謝多 + 達可塞 (15 回合內) | 共用 init; Stage C 沼澤怪物排除 win override |
| 21 | 亞述森林   | end +24 +23 (希爾法+羅蘭) | 祭司希爾法、神射手羅蘭   | 🔑 6-item collection → item 0x64 天空之鑰 |
| 22 | 遠古呼喚   | (無)                   | 龍騎士莎拉 (FDFIELD)      | 白屏 fade-to-black 結尾 (FD2 唯一) |
| 23 | 向天空之旅 | end +22 (卡里斯, 天空之鑰) + 19 (羅德曼, 蜜蒂未加入+15 回合) | 卡里斯 + 羅德曼 | 最大 init+end; 3 conditional joins; 中段 reload 進第二戰場 |
| 24 | 在天空的彼方 | (無)                  | (無)                      | init 4-stage camera scan; end text-scroll cinematic (FD2 唯一) |
| 25 | 火焰的審判 | end +26 +29 (聖寇拉斯+亞奇梅吉) | 大法師亞奇梅吉、龍劍士聖寇拉斯 | init 地震 cutscene + FDOTHER[0x58] sfx; end 亞奇梅吉 init 在 save 後 |
| 26 | 未知的迴廊 | (無)                   | 機器人渥德 (FDFIELD)      | dynamic dialog page (5 寶箱選 1 切 5 路線) |
| 27 | 命運的交會點 | (無)                  | (無)                      | 🔑 **GOOD/BAD ENDING FORK** (`any_char_has_item(0x64)` = 天空之鑰) |
| 28 | 探索者     | (無)                   | (無)                      | 全清隊 20 chars + revive HP>0; 3× 重複 cutscene 0x55 |
| 29 | 無邊的黑暗之中 | (無)                | (無)                      | 唯一 tile_event win; end 9 連震 + 3 白光 + 64+64 palette + char[0x14] 變身 |
| 30 | 傳說的終章－結局 | (無)              | (無)                      | 🏆 **GOOD ENDING** + staff roll (load chapter 31 epilogue map + `play_game_ending_cinematic`) |

## 跨章節隱藏機制鏈

### Good / Bad Ending Fork

```
ch21_end (6-item collection: 黃金徽章 0xD1 + 5 顆眼 0xD2..0xD6)
  ↓ 全 6 件收齊
  → give_item(0x64 = 天空之鑰)
  ↓
ch23_end: any_char_has_item(0x64=天空之鑰)
  → init_char(0x16=22=卡里斯) [武聖加入 GOOD PATH]
  ↓
ch27_end: any_char_has_item(0x64=天空之鑰)
  ├─ 有 → 進 ch28+ (continue good path)
  └─ 無 → animate_warp_teleport_char(悠妮) + play_game_ending_cinematic + INFINITE LOOP (BAD ENDING)
  ↓
ch28-29 戰鬥序列
  ↓
ch30_end: 殺空魔神 (char[0x14]) → game_event_flag = 2
  → load_chapter_battle_data(31) epilogue map + play_game_ending_cinematic + INFINITE LOOP (🏆 GOOD ENDING)
```

### Conditional Recruit 矩陣

| 章 | 角色 (char_id) | 條件 |
|---|---|---|
| ch3 | char 2 = 鐵諾 | char[6] alive |
| ch7 | char 12 = 凱麗 | tile_event[0x11] == 1 AND char[0x2B] alive |
| ch16 | char 18 = 蜜蒂 | chars[0].HP_max > 319 + save_metadata < 19 + chars[0x42..0x49] dead ≤ 4 |
| ch20 | char 28 = 達可賽 | save_metadata < 16 (15 回合內) |
| ch23 | char 22 = 卡里斯 | any_char_has_item(0x64 = 天空之鑰) |
| ch23 | char 19 = 羅德曼 | find_template_char_by_id(0x12 = 蜜蒂) == 0 (蜜蒂不在) AND save_metadata < 15 |

### 隱藏 reward / 兌換機制

| 章 | 條件 | 獎勵 |
|---|---|---|
| ch2 | 6 villagers 全活 (chars[5..10] 無死亡) | item 0xC6 = 力量藥水 (AP+9) |
| ch21 | 持有 item 0xD1..0xD6 全 6 件 (黃金徽章 + 5 顆眼) | item 0x64 = 天空之鑰 |

## 檔案

`chapter_01.md` ... `chapter_30.md` — 各章劇情 / 角色 / 敵人 / 對話完整內容。
