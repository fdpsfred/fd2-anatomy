# 第 2 章 — 羅德鎮

抵達馬拉大陸沿岸的第一座城鎮羅德鎮，遭遇預告要洗劫全鎮的強盜團。保住全部六名村民可獲隱藏獎勵；戰後少女希莉亞主動要求同行，於章末加入隊伍。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 8 | 希莉亞 | End handler 末段 | 章末 cutscene 後固定加入 |

## 敵人配置

本章 FDFIELD entry 4 共 23 個會生成的 spawn 記錄（另有 17 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 28 | 盜賊 | LV2 | ×10 | default_attacker |
| 28 | 盜賊 | LV3 | ×6 | default_attacker |

友軍 NPC（team 1，戰場自走）——章末獎勵關聯對象（chars[5..10]，6 名**全數**死亡才敗；失去 1-5 名不敗、僅喪失獎勵；6 名全數存活則章末獲力量藥水獎勵，見 §特殊機制）：

| enemy_data | 單位 | 等級 | 數量 |
|---|---|---|---|
| 66 | 女村民 | LV3 | ×3 |
| 65 | 男村民 | LV3 | ×3 |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：草藥 (0xC0)、旅行裝 (0x81)、綠寶石 (0xC9)、回復劑 (0xC1)、光之杖 (0x3D)、光之斧 (0x28)
- 空寶箱（0 金錢誘餌）×1

（tile_pickup 另有 1 筆 kind≥2 = 劇情事件觸發點（tile[12]），非寶物，見 §FDFIELD event script。）

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：草藥 (0xC0)
- 金錢：1000

## 商店

story 章，intro 主選單提供武器店 / 道具店，另有以隱藏熱鍵（Ctrl+F2）開啟的神秘商店。品項（chapter_intro_metadata entry 1）：

- **武器店**：短劍 (0x00)、闊劍 (0x01)、手斧 (0x20)、皮甲 (0x84)、法師袍 (0xA5)
- **道具店**：草藥 (0xC0)
- **神秘商店（Ctrl+F2）**：回復劑 (0xC1)、魔法水 (0xCE)

## 特殊機制

- **失敗條件**：索爾死亡，或 6 名村民 NPC (chars[5..10]) **全數**死亡。判定由 `fd2_chapter_02_post_action @ 0x000206C5` 在標準 default（所有敵死＝勝、索爾死＝負）之上，追加「chars[5..10] 全 6 名死亡 → 敗」（迴圈一遇活村民即 return，失去 1-5 名不強制敗）。
- **隱藏 reward**：6 名村民全活 → end handler 給予 item 0xC6 = 力量藥水 (AP+9)。攻略本「為了保住所有村民，最好幫亞雷斯買長戟」即此機制。
- **援軍**：第 3 回合結束觸發 FDFIELD turn-event hook（phase 1、event_code 0x06、handler `0x00034422` = reinforcement_spawner），生成第二波強盜（對話 page 4 提到敵人「兵分兩路」，推測即此波援軍，惟兩來源未明載此頁由該 hook 觸發）。
- **章末加入**：希莉亞 (char 8) 由 end handler `fd2_init_runtime_char_from_base_growth(8)` 加入。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_02_init @ 0x00032D18` | 402 B |
| End | `fd2_chapter_02_end @ 0x00022F37` | 443 B |
| Post-action | `fd2_chapter_02_post_action @ 0x000206C5` | 自訂 — 額外 lose if chars[5..10] 全 6 名死 |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[1]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[1]` |  |

### Init handler

`fd2_init_battle_state_for_chapter` 進入正式戰鬥模式後，依序：

- `fd2_pan_cursor_and_window(0xD, 0xB)` 移到場景
- `fd2_cutscene_event_trigger(9)` + `fd2_display_dialog_scene(page=0)` 第一段對話
- `fd2_cutscene_event_trigger(0xA)` + `fd2_display_dialog_scene(page=1)`
- `fd2_load_chapter_portraits_and_dump_tmp(race_id=1)` 載肖像 set 1
- `fd2_cutscene_event_trigger(0xB)` + `fd2_display_dialog_scene(page=2)`
- `fd2_pan_cursor_and_window(6, 0xC)` 鏡頭移
- `chapter_init_phase_flag = 1` (transient) + `fd2_load_chapter_portraits_and_dump_tmp(race_id=2)` 換肖像
- `fd2_cutscene_event_trigger(0xC)` + `fd2_display_dialog_scene(page=3)`
- `fd2_pan_cursor_to_char(0)` 鏡頭聚焦索爾，戰鬥開始

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 2 | 0, 1, 2, 3 |
| End (villager 全活) | 2 | 6, 8, 9, 10 |
| End (有 villager 死) | 2 | 7, 8, 9, 10 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫 — 第 2 章不在 init 加入新隊員。

End handler 末段 `fd2_init_runtime_char_from_base_growth(8)` → 希莉亞加入。

### Cutscene events

該章用到的 `fd2_cutscene_event_trigger` 呼叫：

- Init: `0x09, 0x0A, 0x0B, 0x0C` (4 events)
- End: `0x0E, 0x0F, 0x10` (3 events)

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id]` 的 walk-animation script。

### Post-action handler

`fd2_chapter_02_post_action @ 0x000206C5`：

- 標準 default 判定（所有敵死＝勝、索爾死＝負）
- **額外 lose 條件**：if chars[5..10] (6 個 villager NPC) **全數**死亡 → `game_event_flag = 1`（迴圈一遇活村民即 return，不覆寫 default flag）

對應失敗條件「索爾死亡，或 6 名村民全數死亡」——chars[5..10] = 6 個村民 NPC（男×3 + 女×3）。

### End handler events

`fd2_chapter_02_end @ 0x00022F37` (443 B) — villager survival branching：

1. 檢查 chars[5..10] (6 villagers)：迴圈設 `bVar = 1` if 任一死亡
2. **Conditional**：
   - 若全活 (`bVar == 0`) → `fd2_display_dialog_scene(page=6)` + `fd2_give_item_to_first_player_char(0xC6)` (item 198 = 力量藥水, AP+9)
   - 若有人死 (`bVar == 1`) → `fd2_display_dialog_scene(page=7)` (alternate ending，無 reward)
3. `fd2_pan_cursor_and_window(0xE, 2)` + `fd2_load_chapter_portraits_and_dump_tmp(4)`
4. `fd2_cutscene_event_trigger(0xE)` + `fd2_display_dialog_scene(page=8)`
5. `fd2_cutscene_event_trigger(0xF)` + `fd2_display_dialog_scene(page=9)`
6. `fd2_pan_cursor_and_window(0xE, 1)` + `fd2_cutscene_event_trigger(0x10)` + `fd2_display_dialog_scene(page=10)`
7. `fd2_init_runtime_char_from_base_growth(8)` — char 8 = 希莉亞加入
8. `fd2_save_runtime_char_to_template` + `current_chapter_id = 2`

## FDFIELD event script

FDFIELD tile_event entry idx **4** (= chapter_id × 3 + 1, chapter_id = 1)，entry size 1171 bytes；party_member_count = 5、char_spawn_count = 40。header layout 見 `resource_info/fdfield.md`。

1 / 16 active turn-event hooks（其餘 15 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 1 (end_of_player_turn) | 0x06 | `0x00034422` | reinforcement_spawner; ch2_reinforcement |

## 對話

對話文字 17 pages 來自 FDTXT.DAT entry 2。Init 引用 page 0/1/2/3；End handler 依 villager 存活分支引用 page 6（全活）或 page 7（有人死），再接 page 8/9/10。

### Page 0

```text
[PORTRAIT_LEFT_BY_ID=0x0004]
『我們已經抵達羅德鎮了。
[PAGE_BREAK]
　聽說這裡是沿岸最繁榮的城
[PAGE_BREAK]
　鎮，我們可以在這裡歇一下
[PARAGRAPH]
　，順便到酒店裡打聽一下消
[PAGE_BREAK]
　息‥』
[END]
```

### Page 1

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『亞雷斯，我看你的情報有誤
[PAGE_BREAK]
　這裡連半個人都沒有，
[PAGE_BREAK]
　倒像是座鬼城！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『好可怕‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『這可怪了，上次我和老爹來
[PAGE_BREAK]
　這裡買酒的時候，鎮上還熱
[PAGE_BREAK]
　鬧的很啊！怎麼會‥』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0005]
『啊！強盜來了！快逃啊！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『等等！我們不是強盜！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0005]
『你們不是強盜，
[PAGE_BREAK]
　那你們是來幹什麼的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『我們不過是路過此地，
[PAGE_BREAK]
　想在鎮上休息一下而已！
[PAGE_BREAK]
　請把事情說個清楚，
[PARAGRAPH]
　說不定我們可以助各位一臂
[PAGE_BREAK]
　之力！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0005]
『唉！你們這些小毛頭能做什
[PAGE_BREAK]
　麼？窮兇極惡的強盜團預告
[PAGE_BREAK]
　要在此時洗劫本鎮，鎮上的
[PARAGRAPH]
　人都逃光了，我們是回來多
[PAGE_BREAK]
　帶走一些財物的，馬上也要
[PAGE_BREAK]
　逃離此地。你們也快逃吧，
[PARAGRAPH]
　要是碰上了那群強盜就糟啦
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001E]
『‥‥！！！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『呀呼！
[PAGE_BREAK]
　我們照預告準時抵達！
[PAGE_BREAK]
　咦？
[PARAGRAPH]
　居然還有不怕死的沒走？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0005]
『啊！強盜來了！
[PAGE_BREAK]
　完啦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『我們不是說過，我們來的時
[PAGE_BREAK]
　候不准有人留在鎮上嗎？
[PAGE_BREAK]
　你們是活的不耐煩了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0005]
『‥啊，強盜大人，
[PAGE_BREAK]
　請放我們一馬，
[PAGE_BREAK]
　以後不敢了‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『這些強盜真猖狂啊！索爾，
[PAGE_BREAK]
　我們來教訓他們一頓！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『什麼教訓！不用給這些傢伙
[PAGE_BREAK]
　悔改的機會，把他們全宰了
[PAGE_BREAK]
　再說！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『我贊成！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『哎呀！那群小伙子說要把
[PAGE_BREAK]
　我們全宰了呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000C]
『好可怕呀！我好怕！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『既然如此，我們就陪他們玩
[PAGE_BREAK]
　個兩手！上！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x0015]
『咦！弟兄們在幹什麼？
[PAGE_BREAK]
　他們早該把事情辦好了吧
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0016]
『好像和別人打的正起勁呢！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0017]
『我看到有漏網之魚向這裡逃
[PAGE_BREAK]
　來呢！我們先打發了他們再
[PAGE_BREAK]
　說！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『救命啊！這裡又有強盜出現
[PAGE_BREAK]
　了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『可惡，沒想到敵人居然兵分
[PAGE_BREAK]
　兩路‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『不管怎樣，得想辦法援救那
[PAGE_BREAK]
　些無辜的居民！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『怎麼辦呢？索爾‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『喂！你們，往東南邊逃吧！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0007]
『好的！大伙兒往東南邊逃吧
[PAGE_BREAK]
　！』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『奈野啊捏？』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_CHAR=0x0006]
『謝謝各位的幫助，
[PAGE_BREAK]
　這是我們的一點小意思，
[PAGE_BREAK]
　請收下吧。』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_CHAR=0x0006]
『謝謝各位的幫助，
[PAGE_BREAK]
　讓我們逃過一劫。』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0008]
『好棒！好棒！打的真漂亮！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『咦？哪裡蹦出來的野丫頭？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『說話客氣點，
[PAGE_BREAK]
　我可不是野丫頭。
[PAGE_BREAK]
　剛才你們在打的時候，
[PARAGRAPH]
　我一直都躲在屋子裡看，
[PAGE_BREAK]
　真是看了一場好戲。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『小姐，妳的膽量可真大。
[PAGE_BREAK]
　有什麼我們能效勞的嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『也沒什麼啦，只是想問問你
[PAGE_BREAK]
　們，你們這麼好的身手，
[PAGE_BREAK]
　應該不只是來這裡打強盜的
[PARAGRAPH]
　吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『是這樣的，我們想醫治這個
[PAGE_BREAK]
　女孩子的失憶症，
[PAGE_BREAK]
　並且送她回家。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『這是怎麼一回事呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『簡單的來說，我們在森林撿
[PAGE_BREAK]
　到了悠妮這女孩‥‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『撿？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『不對不對，是找到了悠妮，
[PAGE_BREAK]
　雖然失去了記憶，但是我們
[PAGE_BREAK]
　相信她是這馬拉大陸的人，
[PARAGRAPH]
　甚至是某王國的公主也說不
[PAGE_BREAK]
　定，因此設法渡海送她回來
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『奇怪了，這大陸上就只有一
[PAGE_BREAK]
　個亞克斯王國，而這個王國
[PAGE_BREAK]
　的公主就‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『就怎樣？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『沒‥沒什麼啦。這樣吧，
[PAGE_BREAK]
　我知道哪裡有人能醫治失憶
[PAGE_BREAK]
　症，我可以帶你們去，不過
[PARAGRAPH]
　‥要附帶一個條件喔。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『先說來聽聽，
[PAGE_BREAK]
　我們會盡量想辦法的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『其實也很簡單，
[PAGE_BREAK]
　就是要帶我一起去。』
[END]
```

### Page 9

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『這‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『小姐，這可不是好玩的，
[PAGE_BREAK]
　妳一個年輕女孩子跟著我們
[PAGE_BREAK]
　，恐怕會倒大楣喔。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『小看我的本領，
[PAGE_BREAK]
　你才準備倒大楣呢。
[PARAGRAPH]
　怎樣？
[PAGE_BREAK]
　接不接受我的條件啊？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『索爾大哥，現在強盜也不知
[PAGE_BREAK]
　道何時會再來，把一個女孩
[PAGE_BREAK]
　子就這樣丟在這裡太危險了
[PARAGRAPH]
　，我看還是先帶她離開的好
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『是啊，索爾，悠妮的事必須
[PAGE_BREAK]
　盡快解決，了不起我們多為
[PAGE_BREAK]
　她操份心就是了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好吧！不過小姐妳給我聽好
[PAGE_BREAK]
　，妳是負責帶路的，別給我
[PAGE_BREAK]
　們添麻煩。知道嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『沒問題！走吧走吧，
[PAGE_BREAK]
　我來帶路！往這邊！』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『真是沒辦法，唉！』
[END]
```

### Page 11

```text
[PORTRAIT_LEFT_BY_CHAR=0x0005]
『啊‥‥！』
[END]
```

### Page 12

```text
[PORTRAIT_LEFT_BY_CHAR=0x0006]
『哇‥啊‥‥！』
[END]
```

### Page 13

```text
[PORTRAIT_LEFT_BY_CHAR=0x0007]
『啊‥‥！』
[END]
```

### Page 14

```text
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『哇‥啊‥‥！』
[END]
```

### Page 15

```text
[PORTRAIT_LEFT_BY_CHAR=0x0009]
『啊‥‥！』
[END]
```

### Page 16

```text
[PORTRAIT_LEFT_BY_CHAR=0x000A]
『哇‥啊‥‥！』
[END]
```
