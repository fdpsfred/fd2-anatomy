# 第 26 章 — 未知的迴廊

通過古代人禁地通道，遭遇大批機甲兵；途中悠妮喚醒一具廢棄機甲兵渥德為己方作戰，並在通道盡頭從 5 個寶箱中選 1 件最強武器。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| (機甲兵) | 渥德 | 通道內 tile event | 悠妮輸入啟動碼 (FDFIELD event)，見對話 page 4 |

## 敵人配置

本章 FDFIELD entry 76 共 69 個會生成的 spawn 記錄（另有 1 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 48 | 機甲隊長 | LV28 | ×1 | aggressive_physical |
| 47 | 機甲守衛 | LV22 | ×2 | aggressive_physical |
| 16 | 地獄騎士 | LV19 | ×2 | defensive_kiter |
| 39 | 龍人法師 | LV24 | ×5 | aggressive_physical |
| 18 | 龍騎士 | LV28 | ×17 | defensive_kiter / default_attacker |
| 43 | 機甲兵 | LV21 | ×18 | aggressive_physical / hardcoded_attack |
| 38 | 龍人戰士 | LV29 | ×8 | default_attacker |
| 44 | 機甲射手 | LV21 | ×1 | hardcoded_attack |
| 46 | 機甲突擊兵 | LV21 | ×2 | hardcoded_attack |
| 42 | 大惡魔 | LV14 | ×6 | default_attacker |
| 41 | 惡魔 | LV14 | ×6 | default_attacker |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：水晶粒 (0xCF)、神聖之水 (0xC3)、力量藥水 (0xC6)、速度藥水 (0xC8)、魔力水晶 (0x5F)
- 金錢：50000、50000

畫面最上方 5 個最強武器寶箱是 tile_pickup 表的 5 筆 kind≥2 事件 tile（tile[0..4]，皆 consequence 0x3A）：五者只能取其一，由 consequence 0x3A handler 處理（見 §FDFIELD event script）。（注意：end handler 的對話分支由 `tile_event_consumed_flags[0xC]` = 渥德招募旗標驅動，與寶箱選擇無關，見 §特殊機制。）

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：生命之實 (0x5E)、神聖之水 (0xC3)
- 金錢：50000

## 商店

story 章，intro 主選單提供武器店 / 道具店，另有以隱藏熱鍵（Ctrl+F6）開啟的神秘商店。品項（chapter_intro_metadata entry 25）：

- **武器店**：龍神劍 (0x0A)、巨神戟 (0x1F)、大地之鎚 (0x2A)、火神弓 (0x68)、龍之杖 (0x3C)、鬥神指環 (0x69)、雷神服 (0x8F)、惡魔鱗甲 (0x9A)、大地鎧甲 (0xA2)、天之袍 (0xAB)、武神鬥服 (0xB1)
- **道具店**：神聖之水 (0xC3)、水晶粒 (0xCF)
- **神秘商店（Ctrl+F6）**：神聖之水 (0xC3)、水晶粒 (0xCF)

## 特殊機制

- **渥德招募 + 動態 dialog**：`tile_event_consumed_flags[0xC]` 是 binary 渥德招募旗標（0=未招募、1=已招募）——由 tile-step handler `fd2_chapter_event_handler_3d__ch26_pickup @ 0x356B7`（consequence 0x3D）在玩家攜帶 key item 0xD0 踩上 pickup tile 時，消耗 0xD0 + 播 FDOTHER.DAT[0x2D] cinematic + `fd2_init_runtime_char_from_base_growth(0x1F)` spawn 渥德後設為 1（未帶 0xD0 則顯示 page 2 不消耗、可重試）。`fd2_chapter_26_end` 據此決定兩段 dialog page：Dynamic #1 = flag[0xC]+5 → page 5（未招募渥德）或 6（已招募）；Dynamic #2 = flag[0xC]+8 → page 8 或 9。（此旗標為 binary 0/1，非「5 寶箱 0-4 選擇器」；5 寶箱另由 consequence 0x3A 處理。）
- **悠妮喚醒機甲兵渥德**：通道內 tile event，悠妮輸入啟動碼 `01E0C244-FE2C5-1932`，機甲兵渥德 (`01279943渥德`) 加入隊伍替己方作戰。此加入由 FDFIELD tile-step / dialog event 處理，非 init/end handler 直接載入（見對話 page 4）。
- **勝負條件**：標準 default（全敵死 = 勝、索爾死 = 負）外，`fd2_chapter_26_post_action` 另判 chars[1]（亞齊梅吉）或 chars[2]（悠妮）死即負。runtime index 由編成畫面 per-chapter pin 決定（見 Post-action handler）。
- **援軍密集 turn**：第 2、4、6、8、10、12、15、16、17 回合敵方 turn intro 各觸發一次過場 cinematic（event_code 0x39、handler `0x000354DD`：載入該回合 portrait set + pan (9,0) + 400ms，不生成單位），共 9 個 turn-event hook（見 FDFIELD event script）；援軍單位由 turn-gated FDFIELD spawn records 生成。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_26_init @ 0x00033AAE` | 67 B |
| End | `fd2_chapter_26_end @ 0x00024E80` | 466 B |
| Post-action | `fd2_chapter_26_post_action @ 0x00020B3C` | default + lose if char[1] OR char[2] dead |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[25]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[25]` |  |

### Init handler

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(9, 0x27)` + `fd2_cutscene_event_trigger(0x4C)`
3. `fd2_display_dialog_scene(page=0)` + `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 26 | 0 |
| End (dynamic page #1) | 26 | flag[0xC] + 5 → page 5（無渥德）/ 6（有渥德） |
| End (固定) | 26 | 7 |
| End (dynamic page #2) | 26 | flag[0xC] + 8 → page 8 / 9 |
| End (固定) | 26 | 10, 11 |

### char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫；本章不在 init 或 end 直接加入新角色。
機甲兵渥德加入由 FDFIELD tile-step / dialog event 處理（見對話 page 4）。

### Cutscene events

- Init: `0x4C`
- End: `0x4D, 0x4E, 0x4F, 0x50` (4 events)

### Post-action handler

`fd2_chapter_26_post_action @ 0x00020B3C`：
- 標準 default 判定（全敵死 = 勝、索爾死 = 負）
- **額外 lose 條件**：if chars[1] (亞齊梅吉) OR chars[2] (悠妮) 死 → `game_event_flag = 1`

runtime char index 隨章節而變，由編成畫面 per-chapter pin 決定：ch26 先 pin 悠妮 (9) 再 pin 亞齊梅吉 (0x1D)，最終 chars[1] = 亞齊梅吉、chars[2] = 悠妮。此 slot-1/2 檢查結構與 ch22/23（chars[1] = 希爾法）、ch27/28（chars[1] = 悠妮）共用，但各章 pin 的角色不同。

### End handler events

`fd2_chapter_26_end @ 0x00024E80` (466 B) — `tile_event_consumed_flags[0xC]` 動態 dialog page selection：

1. 從 `data_fd2_chapter_ch26_end_scene_char_pos_x_table` / `_char_pos_y_table` / `_char_facing_table`（各 byte[16] @ 0x522D6/0x522E6/0x522F6）讀位置
2. **Reposition NPCs**：迴圈 chars[0x10..party_member_count]，若 `bPortrait_id == 0x1F` → 設 bPos = (0x10, 6)
3. `fd2_setup_chars_and_camera_for_intro(...,0, 0xF, 0, 0, 0, 0, 9, 5)`（3 表指標後 tail 為 0,0xF,0,0,0,0,9,5）
4. **Dynamic page #1**：`page = flag[0xC] + 5` → `fd2_display_dialog_scene(page = 5 無渥德 / 6 有渥德)`
5. `fd2_cutscene_event_trigger(0x4D)`
6. `fd2_display_dialog_scene(page=7)` (固定)
7. `fd2_cutscene_event_trigger(0x4E)`
8. **Dynamic page #2**：`page = flag[0xC] + 8` → `fd2_display_dialog_scene(page = 8 / 9)`
9. `fd2_cutscene_event_trigger(0x4F)`
10. `fd2_display_dialog_scene(page=10)` + `fd2_cutscene_event_trigger(0x50)`
11. `fd2_display_dialog_scene(page=11)`
12. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

flag 值與 dialog page 對應：

| `tile_event_consumed_flags[0xC]` | Dynamic #1 (page) | Dynamic #2 (page) |
|---|---|---|
| 0 | 5 | 8 |
| 1 | 6 | 9 |
| 2 | 7 | 10 |
| 3 | 8 | 11 |
| 4 | 9 | 12 |

對應「5 個寶箱選 1」的 5 條對話路線。

## FDFIELD event script

FDFIELD entry idx **76**（= chapter_id × 3 + 1，chapter_id = 25），entry size 1951 bytes；party_member_count = 16、char_spawn_count = 70。header layout 見 resource_info/fdfield.md。

9 / 16 active turn-event hooks（其餘 7 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 2 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |
| 4 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |
| 6 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |
| 8 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |
| 10 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |
| 12 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |
| 15 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |
| 16 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |
| 17 | 0 (enemy_turn_intro) | 0x39 | `0x000354DD` | reinforcement-wave cinematic (portrait + pan (9,0) + 400ms；不生成單位) |

## 對話

對話文字 12 pages 來自 FDTXT.DAT entry 26。Init handler 引用 page 0；End handler 依 `tile_event_consumed_flags[0xC]`（渥德招募旗標 0/1）動態引用 page 5或6（Dynamic #1）與 page 8或9（Dynamic #2），另加固定 page 7、10、11。Page 1-4 由 FDFIELD tile-step / dialog event 引用（page 4 = 攜 key item 0xD0 踩 pickup tile 招募渥德，handler 0x356B7）。roster 與機制描述用正名「亞齊梅吉」，對話 transcript 內維持「亞奇梅吉」係遊戲內部變體，不改。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『嘿，這是什麼怪地方？‥這
[PAGE_BREAK]
　種景色以前從沒看過！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001D]
『這條通道據說是古代人所建
[PAGE_BREAK]
　造的，歷代的惡魔族之王都
[PAGE_BREAK]
　禁止族人進入此地，似乎踏
[PARAGRAPH]
　進這通道就會招來可怕的災
[PAGE_BREAK]
　禍‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001A]
『我感到災禍好像已經在眼前
[PAGE_BREAK]
　了，那些看起來像玩偶的東
[PAGE_BREAK]
　西是什麼？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0074]
『入侵者發現！指令﹕防衛及
[PAGE_BREAK]
　消除威脅的存在，方法﹕接
[PAGE_BREAK]
　戰並消滅。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x006F]
『命令確認！目標分析完畢！
[PAGE_BREAK]
　啟動！執行接戰指令！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001A]
『那是什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『簡單的說，那是古代人建造
[PAGE_BREAK]
　的機器守衛，用來防守他們
[PAGE_BREAK]
　的遺跡和據點的‥沒時間解
[PARAGRAPH]
　釋了，我們準備動手吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『小心！它們要衝過來了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『敵人的數量比上次更多了，
[PAGE_BREAK]
　大家小心應戰！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『‥嚴重受損，指揮控制不能
[PAGE_BREAK]
　‥系統關閉‥‥』
[END]
```

### Page 2

```text
『那是什麼奇怪的東西？
[PAGE_BREAK]
　頭部還開著？』
[END]
```

### Page 3

```text
『這機兵的頭部怎麼開著？這
[PAGE_BREAK]
　個金屬盒子‥好像滿適合的
[PAGE_BREAK]
　，應該是這樣放進去‥‥‥
[PAGE_BREAK]
　咦！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x001F]
『系統啟始，請指示操作碼及
[PAGE_BREAK]
　行動模式。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『這‥好像是01E0C244一FE2C5
[PAGE_BREAK]
　1932。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『確認。主人，01279943渥德
[PAGE_BREAK]
　，回復到正常操作狀態，請
[PAGE_BREAK]
　指示。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『我為什麼記得這些東西？‥
[PAGE_BREAK]
　好吧，幫我們打倒那些機兵
[PAGE_BREAK]
　！能理解嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『理解，指令內容確認，系統
[PAGE_BREAK]
　資料重設‥待命狀態完成。
[PAGE_BREAK]
　完畢。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『目標清除後即解除一級戰鬥
[PAGE_BREAK]
　狀態，並確認所有我方目標
[PAGE_BREAK]
　。等待下一次戰鬥指示碼。
[PARAGRAPH]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『了解。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，妳在那邊做什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒‥沒什麼，我說服了這個
[PAGE_BREAK]
　機甲兵幫我們作戰。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『哇塞，悠妮妳真厲害！』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x001D]
『好極了，我們迅速擊潰了敵
[PAGE_BREAK]
　方機甲兵，現在可以馬上前
[PAGE_BREAK]
　往遺跡，不過我們還是對敵
[PARAGRAPH]
　人的底細一無所知‥‥悠妮
[PAGE_BREAK]
　小姐，妳的護衛也是古代人
[PAGE_BREAK]
　所造的機甲兵吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『是的，她初次和我們相遇時
[PAGE_BREAK]
　就帶著蓋亞。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001D]
『是這樣嗎？‥悠妮小姐，我
[PAGE_BREAK]
　看妳一定知道些什麼，有關
[PAGE_BREAK]
　古代人和這批機甲兵的秘密
[PARAGRAPH]
　‥‥是吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『古代人？機甲兵？‥‥那是
[PAGE_BREAK]
　什麼東西？我不知道！』
[END]
```

### Page 6

```text
[PORTRAIT_RIGHT_BY_ID=0x001D]
『好極了，我們迅速擊潰了敵
[PAGE_BREAK]
　方機甲兵，現在可以馬上前
[PAGE_BREAK]
　往遺跡‥嘿，隊伍裏怎麼有
[PAGE_BREAK]
　個敵方機甲兵？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那是悠妮幫我們拉進伙的，
[PAGE_BREAK]
　她和機甲兵一直很有緣。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『是啊，她初次和我們相遇
[PAGE_BREAK]
　時就帶著蓋亞。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001D]
『是這樣嗎？‥悠妮小姐，我
[PAGE_BREAK]
　看妳好像知道機甲兵的啟動
[PAGE_BREAK]
　碼，是吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『啊？啟動碼‥那是什麼東西
[PAGE_BREAK]
　？我不知道！』
[END]
```

### Page 7

```text
[PORTRAIT_RIGHT_BY_ID=0x001D]
『是嗎？悠妮小姐，我看妳一
[PAGE_BREAK]
　定知道！妳到底是從哪裡來
[PAGE_BREAK]
　的？妳絕不是一般人類！為
[PARAGRAPH]
　了我們大家，把話說個清楚
[PAGE_BREAK]
　吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥不‥不要再說了！‥我什
[PAGE_BREAK]
　麼都不知道‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『亞奇梅吉，夠了！你要是把
[PAGE_BREAK]
　她弄哭了，我絕不饒你！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥算啦，不要逼問她了，反
[PAGE_BREAK]
　正上了黃金城，相信一切的
[PAGE_BREAK]
　謎題自然都會解開的！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好，那就一鼓作氣衝過去，
[PAGE_BREAK]
　我們這就走吧！悠妮，跟著
[PAGE_BREAK]
　我！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『‥‥‥』
[END]
```

### Page 9

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『亞奇梅吉，夠了！你要是把
[PAGE_BREAK]
　她弄哭了，我絕不饒你！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥算啦，不要逼問她了，問
[PAGE_BREAK]
　這個機甲兵也是一樣‥‥喂
[PAGE_BREAK]
　，那個機甲兵。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『01279943渥德，命令確認。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『你叫渥德嗎？請回答我的問
[PAGE_BREAK]
　題。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『了解，記憶庫連接，資料載
[PAGE_BREAK]
　入待命。請指示。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『你們的種族來自何方？領導
[PAGE_BREAK]
　者又是誰？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『記憶庫搜尋，資料#73324﹕
[PAGE_BREAK]
　01279943渥德隸屬第六機甲
[PAGE_BREAK]
　戰鬥團第三戰鬥中隊，全師
[PARAGRAPH]
　團由第一空中要塞建造，在
[PAGE_BREAK]
　執行第七號戰鬥防衛命令時
[PAGE_BREAK]
　，經由轉送站送到地表。我
[PARAGRAPH]
　們的領導者是全能的創造者
[PAGE_BREAK]
　，他可以任意創造機械和生
[PAGE_BREAK]
　命，是這個大地的主宰者，
[PARAGRAPH]
　我們並不知道領導者的名字
[PAGE_BREAK]
　，那不在我們的記憶庫領域
[PAGE_BREAK]
　之內。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥從未聽過這種事‥好吧，
[PAGE_BREAK]
　那你們為何要攻擊這個大陸
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『資料庫搜尋，資料#2074213
[PAGE_BREAK]
　﹕第六機甲戰鬥團奉命執行
[PAGE_BREAK]
　第七號戰鬥防衛命令，主要
[PARAGRAPH]
　目標﹕絕滅X一07地區的敵方
[PAGE_BREAK]
　生命體，防止任何敵方單位
[PAGE_BREAK]
　進入轉送站；但就最後的記
[PARAGRAPH]
　錄，本次作戰已告中止，而
[PAGE_BREAK]
　01279943渥德在遭受敵方攻
[PAGE_BREAK]
　擊之後，誤動安全裝置導致
[PARAGRAPH]
　能源單位彈出，系統中斷至
[PAGE_BREAK]
　今，並未再接到過任何更新
[PAGE_BREAK]
　的作戰命令。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『這傢伙的話還真是難懂‥約
[PAGE_BREAK]
　拿老頭，你聽懂了多少？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我只知道這位渥德老兄好像
[PAGE_BREAK]
　並未參與這次的攻擊，不過
[PAGE_BREAK]
　一切麻煩的根源似乎都來自
[PARAGRAPH]
　所謂的「第一空中要塞」，
[PAGE_BREAK]
　它也可能就是「黃金的城堡
[PAGE_BREAK]
　」，由古代人所建造的‥飛
[PARAGRAPH]
　行堡壘，兼具建造機甲兵的
[PAGE_BREAK]
　能力‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001A]
『古代人不是早在數萬年前的
[PAGE_BREAK]
　最終戰爭中就死光了嗎？為
[PAGE_BREAK]
　什麼這個要塞還能發動攻擊
[PARAGRAPH]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這個「黃金城」會不會是由
[PAGE_BREAK]
　類似的機甲兵所控制，由於
[PAGE_BREAK]
　沒有收到所謂的中止作戰指
[PARAGRAPH]
　令，所以仍在執行以前的作
[PAGE_BREAK]
　戰行動？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『很有可能，古代人建造這種
[PAGE_BREAK]
　飛行要塞，主要目的應該就
[PAGE_BREAK]
　是空中制壓和對地攻擊，但
[PARAGRAPH]
　最終戰爭早在數萬年前就已
[PAGE_BREAK]
　經結束了，為何到現在才又
[PAGE_BREAK]
　開始行動？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『看來只有上黃金城一趟，這
[PAGE_BREAK]
　一切的謎題才能夠解開！渥
[PAGE_BREAK]
　德，你所說的轉送站在哪裡
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『就位在這地下通道的出口處
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好，那就一鼓作氣衝過去，
[PAGE_BREAK]
　我們這就走吧！悠妮，跟著
[PAGE_BREAK]
　我！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥‥‥』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，怎麼了？是不是‥我
[PAGE_BREAK]
　的口氣太粗魯了？對不起！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『不是的，索爾，我好迷惑‥
[PAGE_BREAK]
　‥我到底是誰？這一切‥到
[PAGE_BREAK]
　底和我有什麼關連？我真的
[PARAGRAPH]
　不知道‥我好怕我會給你帶
[PAGE_BREAK]
　來災禍，也許一開始你就不
[PAGE_BREAK]
　該救我的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，我永遠都會相信妳的
[PAGE_BREAK]
　！只要有妳和我在一起，我
[PAGE_BREAK]
　什麼也不怕！我們不是都來
[PARAGRAPH]
　到這裡了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『可是‥可是‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『剛才的事就當它沒發生過，
[PAGE_BREAK]
　來，我們一起走過去！』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『其實亞奇梅吉說的也沒錯‥
[PAGE_BREAK]
　這樣子好嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這個年輕人得到神的庇佑，
[PAGE_BREAK]
　我想我們應該尊重他的決定
[PAGE_BREAK]
　‥不多說了，我們走吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001D]
『也只能這樣了！』
[END]
```
