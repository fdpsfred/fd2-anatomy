# 第 16 章 — 冰原之戰

隊伍在冰原與劍聖蜜蒂會合，蜜蒂帶著八名部下以友軍 NPC 自走，協同對抗來犯的冰原敵軍。達成三項嚴苛條件即可說服蜜蒂於章末正式入隊，是全 30 章中條件最複雜的招募。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 18 (0x12) | 劍聖蜜蒂 | 章末 | 三條件 AND：索爾 HP_max ≥ 320 AND 18 回合內擊敗敵全部 AND 蜜蒂 8 個部下陣亡 ≤ 4 |

## 敵人配置

本章 FDFIELD entry 46 共 60 個會生成的 spawn 記錄。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 35 | 獸人隊長 | LV21 | ×4 | aggressive_physical |
| 34 | 獸人 | LV21 | ×10 | aggressive_physical / targeted_approach |
| 13 | 狂戰士 | LV5 | ×18 | aggressive_physical / targeted_approach |
| 20 | 黑暗射手 | LV9 | ×9 | aggressive_physical / targeted_approach |
| 23 | 黑暗法師 | LV9 | ×6 | targeted_approach / aggressive_physical |
| 26 | 黑暗僧侶 | LV9 | ×2 | aggressive_physical |
| 30 | 影之忍者 | LV3 | ×2 | item_pickup |

友軍 NPC（team 1，戰場自走）：

| enemy_data | 單位 | 等級 | 數量 |
|---|---|---|---|
| char 0x12 | 蜜蒂 | LV1 | ×1 |
| 0 | 士兵 | LV24 | ×8 |

其中 8 名士兵為蜜蒂的部下（runtime slots chars[0x42..0x49]），與蜜蒂 (char[0x41]) 一同以友軍 NPC 自走協戰。

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：藍寶石 (0xCB)、暗殺服 (0x8C)、領悟之書 (0x5B)、耐力藥水 (0xC7)、力量藥水 (0xC6)
- 金錢：10000、18000

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：生命之實 (0x5E)、回復劑 (0xC1)、再生藥 (0xC2)、解毒劑 (0xC4)、藍寶石 (0xCB)、水晶粒 (0xCF)
- 金錢：10000、20000

## 商店

無章內商店。

## 特殊機制

- **失敗條件**：索爾死亡，或蜜蒂 (char[0x41] NPC) 戰死。蜜蒂在本章以友軍 NPC 自走，`fd2_chapter_16_post_action` 在 default 判定（敵全滅＝勝、索爾死＝負）之上額外檢查 `char[0x41]` 是否死亡，蜜蒂戰死即設 game_event_flag=1 判負。
- **蜜蒂招募**：FD2 全 30 章中最複雜的 end-handler 條件式招募，`fd2_chapter_16_end` 需同時達成三項：
  - 索爾 HP_max ≥ 320（`chars[0].wHP_max > 0x13F`）
  - 18 回合內擊敗全部敵人（`data_fd2_battle_turn_counter < 0x13`，即 TURN 1..18）
  - 蜜蒂 8 個部下（chars[0x42..0x49]）陣亡數 ≤ 4
  三條件全達成才走 page 4 招募分支，載入蜜蒂（char_id 0x12）；否則走 page 2 + cutscene 0x31 + page 3 不招募分支。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_16_init @ 0x000335A0` | 10 B (stub-tier) |
| End | `fd2_chapter_16_end @ 0x00023A0A` | 341 B |
| Post-action | `fd2_chapter_16_post_action @ 0x0002084A` | default + 額外 lose if char[0x41] dead |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[15]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[15]` |  |

### Init handler

最簡，三步：

1. `fd2_init_battle_state_for_chapter`
2. `fd2_display_dialog_scene(page=0)`
3. `fd2_pan_cursor_to_char(0)`

無 cutscene、無 portrait load、無 char init。

### Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 16 | 0 |
| End (recruit success) | 16 | 4 |
| End (recruit fail) | 16 | 2, 3 |

### char_id 初始化序列

無 init handler 內 char init。蜜蒂 (char_id 0x12 = 18) 由 end handler 條件式加入。

### Cutscene events

- Init：無
- End：`0x31`（僅在招募失敗分支觸發）

### Post-action handler

`fd2_chapter_16_post_action @ 0x2084A`：

- default 判定（敵全死＝勝、索爾死＝負）
- 額外 lose：if `char[0x41]` 死亡 → game_event_flag = 1

`char[0x41]` = 蜜蒂 NPC slot — 蜜蒂在本章以友軍 NPC 自走，戰死即敗。

### End handler events

`fd2_chapter_16_end @ 0x23A0A` (341 B) — 蜜蒂三條件招募：

1. 從 `chapter_16_end_scene_pos_x/y_table` 讀位置
2. `fd2_setup_chars_and_camera_for_intro(0xF, 0x41, 0x1C, 0x1E, 2, 0x16, 0x19)` 配置 chars
3. **Count dead 部下**：迴圈 `chars[0x42..0x49]`（蜜蒂的 8 個部下），每死一個 `local_14++`
4. 若 `local_14 > 4` → `local_10 = 1`（招募失敗 flag）
5. `fd2_save_runtime_char_to_template`
6. **三條件 AND 招募**：
   - `data_fd2_battle_turn_counter < 0x13`（即 18 回合內，TURN 1..18）**AND**
   - `local_10 != 1`（部下死 ≤ 4）**AND**
   - `chars[0].wHP_max > 0x13F`（索爾 HP_max ≥ 320）
   → `fd2_display_dialog_scene(page=4)` + `fd2_init_runtime_char_from_base_growth(0x12=18)`（蜜蒂加入）
7. **Else**（任一條件未達）：
   → `fd2_display_dialog_scene(page=2)` + `fd2_cutscene_event_trigger(0x31)` + `fd2_display_dialog_scene(page=3)`（no recruit）
8. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD tile_event entry idx **46**（= chapter_id × 3 + 1，chapter_id=15），entry size 1691 bytes。header layout 見 `resource_info/fdfield.md`。

- party_member_count = 16
- char_spawn_count = 60

**turn-event hooks：0/16 active** — 全 16 個 slot 為 sentinel `(turn=0xFF, event_code=0xFF, phase=0)`。本章 turn-based events 完全靜態，由 init / post_action / FDFIELD char spawn records 處理，無動態 turn 觸發。

## 對話

對話文字 5 pages 來自 FDTXT.DAT entry 16。Init handler 播 page 0（隊伍抵達冰原、與劍聖蜜蒂會合的序幕）。End handler 依招募是否成功分歧：三條件全達成走 page 4（蜜蒂折服於索爾的氣魄、正式入隊），否則走 page 2（蜜蒂獨自趕往艾斯島）接 page 3（不招募收場）。page 1 為蜜蒂 NPC (char[0x41]) 戰死時的台詞。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『唔，好冷！住慣了羅特帝亞
[PAGE_BREAK]
　的溫暖氣候，還真有點不習
[PAGE_BREAK]
　慣。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『不知道我的朋友是否已經抵
[PAGE_BREAK]
　達此地，照理說應該會有點
[PAGE_BREAK]
　徵兆的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『嘿，在這種地方特別容易看
[PAGE_BREAK]
　到遠處‥‥咦，那可不是有
[PAGE_BREAK]
　人在打架嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『啊，那不就是‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『賽可邦德？你來的正好，我
[PAGE_BREAK]
　遇上了一點小麻煩‥‥這些
[PAGE_BREAK]
　是你新交的朋友嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『是的，有一些志同道合的戰
[PAGE_BREAK]
　友，他們一路趕過來幫忙的
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『好極了，那麼這一回我可以
[PAGE_BREAK]
　少費點工夫，叫你的朋友們
[PAGE_BREAK]
　快點，趕快解決這場仗，我
[PARAGRAPH]
　們還有很多事要辦。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『賽可邦德，這位女士就是你
[PAGE_BREAK]
　所說的朋友嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『是的，她就是劍聖密蒂，多
[PAGE_BREAK]
　年來一直在山中隱居，鍛鍊
[PAGE_BREAK]
　更高強的劍技。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『是她啊！我記得多年前她在
[PAGE_BREAK]
　皇宮禁衛團中是我的前輩，
[PAGE_BREAK]
　後來好像因為醉心於劍術，
[PARAGRAPH]
　就辭去騎士團的職務，那時
[PAGE_BREAK]
　大家都很惋惜呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『像她這樣的厲害人物，為何
[PAGE_BREAK]
　會突然在此出現？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『她似乎是受友人所託，前來
[PAGE_BREAK]
　調查一些事情。我和她在路
[PAGE_BREAK]
　上結識，她得知我要回去確
[PARAGRAPH]
　認獸人的侵攻行動，就託我
[PAGE_BREAK]
　回來通知她結果。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『這樣說起來，莫非她和約拿
[PAGE_BREAK]
　也有什麼關連‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『算了吧！這女人一副大姐樣
[PAGE_BREAK]
　剛見面就對人發號施令，看
[PAGE_BREAK]
　了就不順眼！我才不管她是
[PARAGRAPH]
　什麼劍聖呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，那我們就表現給她看
[PAGE_BREAK]
　啊！讓她知道我們羅特帝亞
[PAGE_BREAK]
　的劍士可也不是泛泛之輩！
[PARAGRAPH]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『說的對！今天要大幹一場！
[PAGE_BREAK]
　上啊！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0041]
『啊‥‥我太天真了，過於倚
[PAGE_BREAK]
　賴劍技，沒有考慮到‥單打
[PAGE_BREAK]
　獨鬥還是不行的‥‥』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0012]
『嗯，一場不錯的仗，但是耗
[PAGE_BREAK]
　時太久了‥‥賽可邦德，有
[PAGE_BREAK]
　關獸人的情況我已經明白了
[PARAGRAPH]
　，等一下我馬上要趕到艾斯
[PAGE_BREAK]
　島去，如果你的朋友也想湊
[PAGE_BREAK]
　湊熱鬧的話，就由你帶他們
[PARAGRAPH]
　去吧，我應該告訴過你艾斯
[PAGE_BREAK]
　島的位置。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『是的，我確實記得。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『艾斯島情勢非比尋常，叫他
[PAGE_BREAK]
　們量力而為，不要把小命送
[PAGE_BREAK]
　掉了。我走了。』
[END]
```

### Page 3

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，這女人真是夠囂張了！
[PAGE_BREAK]
　我‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『唉，索爾，我們技不如人，
[PAGE_BREAK]
　你還想怎麼樣？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『我們還是先趕到艾斯島上觀
[PAGE_BREAK]
　察一下情勢，或許還有我們
[PAGE_BREAK]
　可以幫忙的地方‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『也只好這樣了！索爾，別無
[PAGE_BREAK]
　精打采的，我們走吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可惡，我才不肯認輸呢！
[PAGE_BREAK]
　等著瞧！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x0012]
『各位，漂亮的一仗！真是把
[PAGE_BREAK]
　團隊合作的精神發揮到了極
[PAGE_BREAK]
　致，我個人非常佩服。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『閣下的劍技也極為高超，
[PAGE_BREAK]
　真是讓我們開了眼界。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『謝謝。那麼，我想各位會到
[PAGE_BREAK]
　這裡來，相信都和我有著相
[PAGE_BREAK]
　同的目的，現在已查明敵方
[PARAGRAPH]
　部隊正集結在北方的艾斯島
[PAGE_BREAK]
　上，接下來就由我帶領各位
[PAGE_BREAK]
　前去，將敵人一舉殲滅。我
[PARAGRAPH]
　想各位都沒有意見吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『我有意見！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索‥索爾？你怎麼了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『你有什麼意見，就說出來聽
[PAGE_BREAK]
　聽看吧。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『既然是我們戰技高超，我們
[PAGE_BREAK]
　為什麼要由妳來帶領？應該
[PAGE_BREAK]
　是妳加入我們，然後嚮導我
[PARAGRAPH]
　們到艾斯島去才對，不是嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『索爾大哥，密蒂閣下也算是
[PAGE_BREAK]
　劍士中的前輩，你也多少敬
[PAGE_BREAK]
　老尊賢一下嘛‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，我才不服！會打架的才
[PAGE_BREAK]
　算高手，我從小就這麼認為
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『咈咈‥小子，你年紀這麼輕
[PAGE_BREAK]
　就有如此的膽識和武藝，我
[PAGE_BREAK]
　很欣賞‥‥也好，我一個人
[PARAGRAPH]
　過了這麼多年，也好久沒有
[PAGE_BREAK]
　嘗過被人帶領的滋味了，我
[PAGE_BREAK]
　就加入你們吧！劍聖密蒂，
[PARAGRAPH]
　以後請各位多多指教。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『我不敢相信，一向自視極高
[PAGE_BREAK]
　美貌而又驕傲的劍聖密蒂，
[PAGE_BREAK]
　居然就這樣折服在索爾的氣
[PARAGRAPH]
　魄之下了‥‥』
[PARAGRAPH]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『這就是王者的霸氣啊！不愧
[PAGE_BREAK]
　是索爾，光這一點我就比不
[PAGE_BREAK]
　上他了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『那麼，密蒂閣下，這就請妳
[PAGE_BREAK]
　帶領我們到艾斯島去吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『我既然已經加入你們，稱呼
[PAGE_BREAK]
　我時閣下兩個字就可以不必
[PAGE_BREAK]
　了，對吧，索爾？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，我才‥我才不在乎這些
[PAGE_BREAK]
　事呢！我們快上路吧！』
[END]
```
