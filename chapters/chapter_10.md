# 第 10 章 — 洞窟中的激戰

潛入瀑布後的洞窟救出被擄的卡納恩三世與索菲亞，並揭露黃金徽章其實是飛天戰車的鑰匙。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 6 | 萊汀 | 章末 | End handler 加入 |
| 11 (0xB) | 索菲亞 | 章末 | End handler 加入 |

攻略提及加入「騎士萊汀、僧侶索菲亞」，對應 char 6 與 char 11，兩人都在 End handler 透過 `fd2_init_runtime_char_from_base_growth` 加入。

## 敵人配置

本章 FDFIELD entry 28 共 50 個會生成的 spawn 記錄（另有 10 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 54 | 地魔神 | LV5 | ×1 | aggressive_physical |
| 12 | 黑暗戰士 | LV4 | ×16 | pass_turn / aggressive_physical |
| 20 | 黑暗射手 | LV4 | ×8 | aggressive_physical |
| 23 | 黑暗法師 | LV4 | ×8 | aggressive_physical |
| 26 | 黑暗僧侶 | LV4 | ×6 | aggressive_physical |

友軍 NPC（team 1，戰場自走）：

| enemy_data | 單位 | 等級 | 數量 |
|---|---|---|---|
| 67 | 村民類 NPC（enemy_data[67]，class_id 0x1B 村民） | LV5 | ×1 |
| char 0x0B | 索菲亞 | LV16 | ×1 |
| 1 | 王國正規軍 | LV14 | ×8 |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：黑暗劍 (0x04)、再生藥 (0xC2)、草藥 (0xC0)、紅寶石 (0xCA)、解毒劑 (0xC4)、回復劑 (0xC1)
- 金錢：10000、10000、10000

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：槤枷 (0x37)、生命之實 (0x5E)、回復劑 (0xC1)、再生藥 (0xC2)、魔法水 (0xCE)
- 金錢：10000

索菲亞（NPC，char 0x0B）身上持有關鍵道具「黃金徽章」，由 FDFIELD char_spawn_record 的 inventory_slots 設置。

## 特殊機制

- **兩 NPC 起始睡眠鎖定**：`fd2_chapter_10_init` 把 `runtime_char_array[0x32]`（卡納恩三世）與 `runtime_char_array[0x33]`（索菲亞）的 `bStatus_sleep_flag` 設為 100，兩人於戰鬥開場即處於睡眠（不可動）狀態，作為 cutscene 與失敗判定的鎖定機制，需保護到援軍抵達；`fd2_chapter_10_end` 才把兩人的 sleep flag 清 0 解除。
- **失敗條件**：由 `fd2_chapter_10_post_action` 判定，索爾死亡、`chars[0x32]`（卡納恩三世）死亡、或 `chars[0x33]`（索菲亞）死亡，任一發生即敗。
- **援軍出場**：「第五回合己方結束時援軍出現」對應 FDFIELD entry 28 的 turn = 5 / phase = 1（end_of_player_turn）dialog_only turn-event hook（handler `0x00034BE2`）。
- **End cutscene 大型轉場**：`fd2_chapter_10_end` 讓多名角色復活與重新定位，`chars[0..0xA]`（11 名）從 end-scene 位置表讀座標並面朝北，`chars[0x32]`／`chars[0x33]`／`chars[0x34]`／`chars[5]` 復活並重新放置（詳見 Handler 流程的 End handler events）。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_10_init @ 0x0003332B` | 90 B |
| End | `fd2_chapter_10_end @ 0x000235F9` | 407 B |
| Post-action | `fd2_chapter_10_post_action @ 0x00020707` | (custom) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[9]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[9]` |  |

### Init handler

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(10, 0)`
3. `runtime_char_array[0x32].bStatus_sleep_flag = 100` — 卡納恩三世起始睡眠
4. `runtime_char_array[0x33].bStatus_sleep_flag = 100` — 索菲亞起始睡眠
5. `fd2_display_dialog_scene(page=0)`
6. `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 10 | 0 |
| End | 10 | 4, 5 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 中：

- `fd2_init_runtime_char_from_base_growth(0xB = 11)` — 索菲亞
- `fd2_init_runtime_char_from_base_growth(6)` — 萊汀

### Cutscene events

`0x25`（end）。Init 無 cutscene event。

### Post-action handler

`fd2_chapter_10_post_action @ 0x20707`：

- default 判定（全敵死 = win，索爾死 = lose）
- 額外 lose 條件：`chars[0x32]` 死亡 OR `chars[0x33]` 死亡

對應「失敗條件：索爾死亡、卡納恩三世死亡、索菲亞死亡」，其中 `chars[0x32]` = 卡納恩三世、`chars[0x33]` = 索菲亞（即起始睡眠的兩人）。

「第五回合己方結束時援軍出現」由 FDFIELD turn-event 處理（見下方 FDFIELD event script）。

### End handler events

`fd2_chapter_10_end @ 0x235F9`（大型轉場 + 多 char 復活）：

1. 從 `data_fd2_chapter_ch10_end_scene_char_pos_x_table_chars_0_6` 與 `data_fd2_chapter_ch10_end_scene_char_pos_y_table` 讀位置
2. `fd2_play_palette_fade_to_black` + `fd2_clear_all_chars_acted_flag`
3. Reposition `chars[0..0xA]`（11 chars）：每個 char 設 bPos_x/y from table、sprite_state[1] = 2（face north）
4. Special chars revival/repositioning：
   - `chars[0x32]`：bPos_x = 0xF、bPos_y = 0x23、`bStatus_sleep_flag = 0`（解除卡納恩三世睡眠）
   - `chars[0x33]`：bPos_x = 0xE、bPos_y = 0x23、`bStatus_sleep_flag = 0`（解除索菲亞睡眠）
   - `chars[0x34]`：bPos_x = 0x10、bPos_y = 0x23、`bFlags = 0`（revive）
   - `chars[5].bFlags = 0`（revive char[5]）
5. Reset battle camera：`battle_window_origin_x/y = 9, 0x22`；`cursor_world/screen_x/y = 9/0x22/0/0`
6. `fd2_composite_battle_frame` + `fd2_play_palette_fade_in` + 200ms wait
7. `fd2_display_dialog_scene(page=4)` + `fd2_cutscene_event_trigger(0x25)` + `fd2_display_dialog_scene(page=5)`
8. `fd2_save_runtime_char_to_template`
9. `fd2_init_runtime_char_from_base_growth(0xB = 11)` — 索菲亞
10. `fd2_init_runtime_char_from_base_growth(6)` — 萊汀
11. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **28**（= chapter_id × 3 + 1，chapter_id = 9），entry size 1691 bytes；party_member_count = 11、char_spawn_count = 60。header layout 見 `resource_info/fdfield.md`。

2 / 16 turn-event hooks active（其餘 14 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 5 | 1 (end_of_player_turn) | 0x20 | `0x00034BE2` | dialog_only |
| 20 | 0 (enemy_turn_intro) | 0x21 | `0x00034C1E` | dialog_with_state |

## 對話

對話文字共 6 pages，來自 FDTXT.DAT entry 10（= chapter_id + 1，chapter_id = 9）。Init handler 引用 page 0（開場的洞窟對話與敵首挑釁），End handler 依序引用 page 4 與 page 5（救出國王後的收場對話）。戰鬥中的 page 1（第五回合援軍抵達）、page 2、page 3（敵首戰敗、吐露黃金徽章秘密）由 FDFIELD turn-event 與敵首戰敗事件觸發引用。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哇塞，好大的洞窟！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『若沒有人提供消息的話
[PAGE_BREAK]
　還真想不到瀑布後面會有
[PAGE_BREAK]
　這麼一個地方呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『公主殿下，屬下已將事
[PAGE_BREAK]
　情查清楚了，禁衛軍統領
[PAGE_BREAK]
　葛雷和敵方勾結，他想謀
[PARAGRAPH]
　害陛下然後自立為王。
[PAGE_BREAK]
　事實上，陛下在那一晚遇
[PAGE_BREAK]
　襲時就已經被擄走了，之
[PARAGRAPH]
　後葛雷就偽稱陛下在寢宮
[PAGE_BREAK]
　中養傷，然後假借陛下的
[PAGE_BREAK]
　名義發號施令。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『他還想藉著我們惹上王
[PAGE_BREAK]
　國軍的機會，利用你們把
[PAGE_BREAK]
　我除掉，以後他就可以
[PARAGRAPH]
　高枕無憂了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『都怪屬下愚昧。還好葛
[PAGE_BREAK]
　雷的一名手下曾替他傳遞
[PAGE_BREAK]
　信件到此地，我們才能找
[PARAGRAPH]
　到這個所在，相信陛下可
[PAGE_BREAK]
　能被監禁在此地。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那還等什麼？我們快把
[PAGE_BREAK]
　那個作怪的傢伙找出來痛
[PAGE_BREAK]
　打一頓，這件事就可以做
[PARAGRAPH]
　個了結了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『不知死活的傢伙們，
[PAGE_BREAK]
　你們真是天真的可以！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『你就是幕後主使者嗎？
[PAGE_BREAK]
　快把陛下交還給我們，不
[PAGE_BREAK]
　然王國禁衛軍是不會放過
[PARAGRAPH]
　你的！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『你是說卡納恩三世那個
[PAGE_BREAK]
　老頭嗎？別擔心，等我把
[PAGE_BREAK]
　你們送上西天，我會讓他
[PARAGRAPH]
　和你們在天上相見的。
[PAGE_BREAK]
　哈哈哈哈！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『哼，難看的傢伙，先打
[PAGE_BREAK]
　上一場再來說大話吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『敵人數量不少，
[PAGE_BREAK]
　大家小心應戰！
[PAGE_BREAK]
　援軍很快就會到了，
[PARAGRAPH]
　所以大家量力而為，
[PAGE_BREAK]
　不要太勉強！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『儘管放馬過來吧！今天
[PAGE_BREAK]
　你們一個都別想活著出去
[PAGE_BREAK]
　！哇哈哈哈！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0045]
『萊汀大人！屬下來遲了
[PAGE_BREAK]
　真是罪該萬死‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『沒關係！陛下確是被監
[PAGE_BREAK]
　禁在此，憑著王國禁衛軍的
[PAGE_BREAK]
　名譽，大家要努力作戰，
[PARAGRAPH]
　將陛下平安救回！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0045]
『遵命！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『我不得不承認，你們比我預
[PAGE_BREAK]
　想的還難纏，不過我沒有耐
[PAGE_BREAK]
　性再和你們玩下去了，
[PARAGRAPH]
　來人，殺了卡納恩三世。』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『‥怎‥怎麼可能‥我居然被
[PAGE_BREAK]
　你們這些無名小卒給打敗了
[PAGE_BREAK]
　‥主人‥救救我‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『少討饒了！快告訴我們，你
[PAGE_BREAK]
　綁架國王﹑惹起這一切事端
[PAGE_BREAK]
　究竟有何目的？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『‥為了‥為了得到王家的至
[PAGE_BREAK]
　寶「黃金徽章」‥那是‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『怎樣！快說啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『‥飛天的‥戰車‥鑰匙‥
[PAGE_BREAK]
　啊‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_RIGHT_BY_ID=0x0008]
『父王，您沒事吧！
[PAGE_BREAK]
　我好擔心‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『乖女兒放心，我平安無事。
[PAGE_BREAK]
　這可要多謝妳的朋友們，
[PARAGRAPH]
　不然這回我真的是難逃劫數
[PAGE_BREAK]
　了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『國王陛下，令我們納悶的是
[PAGE_BREAK]
　敵方為何要對您下手？
[PAGE_BREAK]
　您可知道其中的原因？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『簡單來說，他們是為了王家
[PAGE_BREAK]
　至寶「黃金徽章」。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『黃金徽章？那是什麼？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0033]
『這個就讓我來回答吧。
[PAGE_BREAK]
　黃金徽章是亞克斯王家的祖
[PAGE_BREAK]
　傳之寶，據說它有著莫大的
[PARAGRAPH]
　神秘力量，所以幾代以來都
[PAGE_BREAK]
　一直小心的收藏著，但是它
[PAGE_BREAK]
　究竟有什麼功用，也一直都
[PARAGRAPH]
　沒有人知道。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『他們把我綁到這裡來，
[PAGE_BREAK]
　就是要逼我交出黃金徽章。
[PAGE_BREAK]
　後來我被拷問得受不了了，
[PARAGRAPH]
　終於告訴他們黃金徽章的所
[PAGE_BREAK]
　在，還連累了執掌神殿的索
[PAGE_BREAK]
　菲亞‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『原來如此。約拿果然料得沒
[PAGE_BREAK]
　錯‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0033]
『賢者約拿？你認識他嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『我和他算是舊識了，這回就
[PAGE_BREAK]
　是他要我到王城和他碰面，
[PAGE_BREAK]
　才會遇上這些事情的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0033]
『前幾天他也曾經來找我，他
[PAGE_BREAK]
　說幾天後會有一個朋友來王
[PAGE_BREAK]
　城找他，希望我能把黃金徽
[PARAGRAPH]
　章借給他，看來他說的就是
[PAGE_BREAK]
　你了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『妳果然見過約拿！那麼‥
[PAGE_BREAK]
　黃金徽章的事，
[PAGE_BREAK]
　妳能夠答應嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0033]
『這要看陛下的決定‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『沒關係，我早已聽說過賢者
[PAGE_BREAK]
　約拿的大名，他要借用黃金
[PAGE_BREAK]
　徽章，一定是有重要的用途
[PARAGRAPH]
　索菲亞，等我們回城後，
[PAGE_BREAK]
　便把黃金之徽交給他。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0033]
『陛下，我有一個請求，
[PAGE_BREAK]
　希望您也能一併答應‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『什麼請求？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0033]
『據約拿所言，最近有邪惡之
[PAGE_BREAK]
　徒正在進行一樁陰謀，他自
[PAGE_BREAK]
　己先動身去調查此事，但希
[PAGE_BREAK]
　望我也能前去幫忙，貢獻我
[PARAGRAPH]
　自己對神的虔誠與神聖的力
[PAGE_BREAK]
　量，來阻止邪徒的惡行。
[PARAGRAPH]
　所以我可能不能再擔任神殿
[PAGE_BREAK]
　司祭一職，請陛下您答應‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『這樣嗎？好吧，我也不
[PAGE_BREAK]
　能勉強妳‥‥妳就去吧！
[PAGE_BREAK]
　要好好保重！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0033]
『感激不盡！願聖靈與陛
[PAGE_BREAK]
　下同在！』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0008]
『父王，我也有一個請求‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『希莉亞，難道妳也‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『父王，這些天來，我都和這
[PAGE_BREAK]
　些朋友們一起冒險，覺得自
[PAGE_BREAK]
　己長大了不少。如今邪徒的
[PARAGRAPH]
　陰謀已經威脅到我們亞克斯
[PAGE_BREAK]
　王國，我身為王國第一公主
[PAGE_BREAK]
　跟著他們去冒險，多少也可
[PARAGRAPH]
　以盡一點責任‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『‥希莉亞，妳瘦了不少，
[PAGE_BREAK]
　這些天來吃了不少苦吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『不會啊，大家都對我很好，
[PAGE_BREAK]
　和敵人戰鬥時，索爾和亞雷
[PAGE_BREAK]
　斯也總是擋在我前面，
[PARAGRAPH]
　不讓我受到傷害。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『這樣嗎？索爾，亞雷斯，
[PAGE_BREAK]
　多謝你們照顧我的女兒，
[PAGE_BREAK]
　如果時間充裕的話，
[PARAGRAPH]
　希望能有機會好好謝謝你們
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『多謝陛下，我們不敢當。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『是呀！想當初我們就沒料到
[PAGE_BREAK]
　希莉亞會是公主‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『哈哈哈！希莉亞，妳的保密
[PAGE_BREAK]
　功夫還真到家。我答應妳的
[PAGE_BREAK]
　請求，不過為了妳的安全起
[PARAGRAPH]
　見，由禁衛隊長萊汀與妳隨
[PAGE_BREAK]
　行，妳沒有異議吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『沒有，沒有！謝謝父王！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『屬下必以生命保護公主，
[PAGE_BREAK]
　請陛下放心。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『好！其他的事，等我們回到
[PAGE_BREAK]
　城中再說，我們趕快離開這
[PAGE_BREAK]
　個陰森的洞窟。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『屬下這就安排禁衛軍前來迎
[PAGE_BREAK]
　接。諸位，我們走吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『看來事情越鬧越大了。
[PAGE_BREAK]
　索爾，這場熱鬧我們還要繼
[PAGE_BREAK]
　續湊下去嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『不管怎樣，先找到賢者約拿
[PAGE_BREAK]
　治好悠妮的失憶症再說。
[PAGE_BREAK]
　亞雷斯，你會幫我吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『那還用說！我們走吧！』
[END]
```
