# 第 5 章 — 塞拉村

塞拉村遭盜賊團首領卡特那洗劫，僧侶瑪琳雇請索爾一行擊退盜賊。戰後瑪琳加入隊伍，並指引眾人前往南部大港普里茲尋找賢者約拿，為悠妮的失憶症尋求解法。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 10 | 瑪琳 | End handler 末段 | 章末固定加入 |

## 敵人配置

本章 FDFIELD entry 13 共 45 個會生成的 spawn 記錄（另有 5 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 28 | 盜賊 | LV7 | ×20 | aggressive_physical / default_attacker |
| 49 | 卡特那 | LV8 | ×1 | aggressive_physical |
| 29 | 盜賊頭目 | LV7 | ×3 | aggressive_physical / default_attacker |
| 22 | 魔法師 | LV6 | ×4 | default_attacker / aggressive_physical |
| 25 | 僧侶 | LV7 | ×2 | aggressive_physical / default_attacker |
| 34 | 獸人 | LV7 | ×4 | default_attacker |

卡特那（enemy_data 49，LV8）為盜賊團首領，是本章 boss。

友軍 NPC（team 1，戰場自走）——瑪琳的護衛，屬王國正規軍援軍，於戰鬥中途各回合波次抵達：

| enemy_data | 單位 | 等級 | 數量 |
|---|---|---|---|
| 0 | 士兵 | LV7 | ×4 |
| 1 | 王國正規軍 | LV7 | ×6 |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：回復劑 (0xC1)、長劍 (0x02)、力量藥水 (0xC6)、淬毒刀 (0x0E)
- 金錢：10000

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：回復劑 (0xC1)、解毒劑 (0xC4)、綠寶石 (0xC9)、紅寶石 (0xCA)
- 金錢：1000、2000、5000

## 商店

story 章，intro 主選單提供武器店 / 道具店，另有以隱藏熱鍵（Ctrl+F5）開啟的神秘商店。品項（chapter_intro_metadata entry 4）：

- **武器店**：闊劍 (0x01)、長劍 (0x02)、刺矛 (0x14)、騎槍 (0x15)、迴旋斧 (0x21)、短弓 (0x2C)、長弓 (0x2D)、鎖子甲 (0x91)、夜行裝 (0x86)、僧侶袍 (0xA6)
- **道具店**：草藥 (0xC0)、回復劑 (0xC1)、解毒劑 (0xC4)
- **神秘商店（Ctrl+F5）**：再生藥 (0xC2)、魔法水 (0xCE)

## 特殊機制

- **失敗條件**：索爾死亡；post-action 走 `fd2_check_battle_end_default_handler`，無自訂勝負條件。
- **章末加入**：瑪琳 (char 10) 由 end handler `fd2_init_runtime_char_from_base_growth(10)` 於對白後加入。
- **跨章劇情串聯**：戰後瑪琳說明失憶症屬心病、神術無從醫治，改而指引前往普里茲港找賢者約拿，串聯下一章。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_05_init @ 0x00033049` | 258 B |
| End | `fd2_chapter_05_end @ 0x000231F9` | 157 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[4]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[4]` |  |

### Init handler

`fd2_init_battle_state_for_chapter` 後依序：

- `fd2_display_dialog_scene(page=0)`
- `fd2_pan_cursor_and_window(3, 3)` + `fd2_load_chapter_portraits_and_dump_tmp(race_id=1)`
- `fd2_cutscene_event_trigger(0x16)` + `fd2_display_dialog_scene(page=1)`
- `fd2_pan_cursor_and_window(8, 0xE)` + `fd2_cutscene_event_trigger(0x15)`
- `fd2_display_dialog_scene(page=2)` + `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 5 | 0, 1, 2 |
| End | 5 | 9 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 末段 `fd2_init_runtime_char_from_base_growth(10)` → 瑪琳加入。

### Cutscene events

- Init: `0x15, 0x16` (2 events，注意非按字典序：先 0x16，再 0x15)

### Post-action handler

`data_fd2_chapter_post_action_handler_table[4]` 指向 `fd2_check_battle_end_default_handler`，無自訂勝負條件。

### End handler events

`fd2_chapter_05_end @ 0x000231F9` (157 B)：

1. 從 `data_fd2_chapter_ch05_end_scene_char_pos_x_table` / `pos_y_table` / `facing_table`
   (各 4 entries @ 0x520D2/0x520D9/0x520E0) 讀 4 entries → recruit_block × 3
2. `fd2_setup_chars_and_camera_for_intro(...)` 設置 camera + chars positions
3. `fd2_display_dialog_scene(page=9)`
4. `fd2_init_runtime_char_from_base_growth(10)` — char 10 = 瑪琳加入
5. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **13** (= chapter_id × 3 + 1，chapter_id = 4)，entry size 1431 bytes。
party_member_count = 7、char_spawn_count = 50。header layout 見 `resource_info/fdfield.md`。

4 / 16 active turn-event hooks (其餘 12 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 0 (enemy_turn_intro) | 0x0E | `0x000345EA` | dialog_with_state; ch5_dialog_with_state |
| 4 | 1 (end_of_player_turn) | 0x0F | `0x0003462E` | dialog_with_state; ch5_dialog_with_state |
| 7 | 0 (enemy_turn_intro) | 0x10 | `0x00034696` | dialog_only; ch5_dialog |
| 8 | 0 (enemy_turn_intro) | 0x11 | `0x000346C8` | dialog_with_state; ch5_dialog_with_state |

## 對話

對話文字共 12 pages，來自 FDTXT.DAT entry 5。Init handler 引用 page 0/1/2，End handler 引用 page 9；其餘 pages 多由戰鬥中的 turn-event 對白 handler（見 FDFIELD event script 節的 ch5_dialog / ch5_dialog_with_state）觸發，部分（如首領倒下的對白）可能由戰鬥事件而非固定 turn hook 觸發，精確對應未逐頁核實。

### Page 0

```text
[PORTRAIT_LEFT_BY_ID=0x0075]
『喔呵呵呵！
[PAGE_BREAK]
　擺平了這些士兵，
[PAGE_BREAK]
　塞拉村就是我的啦！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0044]
『別作夢了！我們的援軍再過
[PAGE_BREAK]
　一會兒就到了，等著瞧吧！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0075]
『那就要看你們能不能活到那
[PAGE_BREAK]
　個時候了！』
[END]
```

### Page 1

```text
[PORTRAIT_RIGHT_BY_ID=0x000A]
『可惡的強盜，連這個神聖的
[PAGE_BREAK]
　村莊都敢侵犯，我不會放過
[PAGE_BREAK]
　你們的！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0075]
『小女孩來湊什麼熱鬧，
[PAGE_BREAK]
　若不想受傷就快回家去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『可惡，妳敢輕視我！
[PAGE_BREAK]
　我的護衛們很快就到了，
[PAGE_BREAK]
　那時後就‥‥』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哎呀呀呀！果然出事了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『好大群的盜賊啊！這下子有
[PAGE_BREAK]
　得打了‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0075]
『喔呵呵呵！這些就是妳的護
[PAGE_BREAK]
　衛嗎？怎麼看起來都是些小
[PAGE_BREAK]
　毛頭？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000A]
『這個‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『誰是小毛頭！老女人，
[PAGE_BREAK]
　妳說話最好小心點！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0075]
『不知死活的小鬼，讓你吃些
[PAGE_BREAK]
　苦頭，你才會知道大姐我的
[PAGE_BREAK]
　厲害。通通都給我殺了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000A]
『喂！你們來幫個忙吧！打退
[PAGE_BREAK]
　這個盜賊團，我可以付你們
[PAGE_BREAK]
　不低的酬金‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『要打架嗎？我興緻可高得很
[PAGE_BREAK]
　，亞雷斯，我們上！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『敵人數量很多，大家小心了
[PAGE_BREAK]
　！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0075]
『就幾個小鬼還打發不掉嗎？
[PAGE_BREAK]
　好，第二隊，第三隊上！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x0045]
『無法無天的盜賊，竟然侵犯
[PAGE_BREAK]
　受神所庇護的神聖之村，
[PAGE_BREAK]
　難道不怕遭天罰嗎？
[PARAGRAPH]
　我們王國正規軍這就來替天
[PAGE_BREAK]
　行道！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0075]
『你們來的正好，還來得及幫
[PAGE_BREAK]
　你們的伙伴收屍。喔呵呵呵
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0045]
『可惡，大家上啊！和這些傢
[PAGE_BREAK]
　伙拼了！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_CHAR=0x0030]
『嗚吼～好吵啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0031]
『嗚吼～又發生什麼事了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0032]
『嗚吼～又有人類在打架了！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0033]
『有趣有趣！我們再去打人少
[PAGE_BREAK]
　的那一邊！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0066]
『吼‥等等，那好像是我們上
[PAGE_BREAK]
　次遇到的那一批人耶！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_ID=0x0066]
『好可怕，不要和他們打！
[PAGE_BREAK]
　快逃快逃！』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0075]
『‥啊‥我居然就這樣被一群
[PAGE_BREAK]
　小鬼打敗了，
[PAGE_BREAK]
　我不甘心‥‥』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_ID=0x000A]
『多謝大家幫我們打敗盜賊，
[PAGE_BREAK]
　我代表村中的祭司們向各位
[PAGE_BREAK]
　致謝。
[PARAGRAPH]
　除了我答應的酬勞外，各位
[PAGE_BREAK]
　是否還需要任何醫療與協助
[PAGE_BREAK]
　？相信祭司們會很樂意幫忙
[PARAGRAPH]
　的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『酬勞可以免啦，幫我們醫好
[PAGE_BREAK]
　這位小姐的失憶症就可以了
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000A]
『失憶症？這個‥這種病，
[PAGE_BREAK]
　他們恐怕是幫不上忙喔！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『為什麼？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000A]
『我曾跟著村中的祭司學過一
[PAGE_BREAK]
　陣子神術，據我所知，神術
[PAGE_BREAK]
　能醫治的都是肢骨肉體上的
[PARAGRAPH]
　疾病，失憶症算是一種心病
[PAGE_BREAK]
　，神術並不會對心靈記憶等
[PAGE_BREAK]
　產生影響，所以也無從治起
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『嗄？那豈不是‥白忙一場了
[PAGE_BREAK]
　？希‥希莉亞！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『這‥這不是我的錯啊！我本
[PAGE_BREAK]
　來也以為祭司們醫術高超，
[PAGE_BREAK]
　一定能治好悠妮的失憶症的
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，現在怪希莉亞
[PAGE_BREAK]
　也沒用，還是想想下一步該
[PAGE_BREAK]
　怎麼做吧。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，我也不好意思再麻煩
[PAGE_BREAK]
　你們了，不如‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000A]
『別急嘛！我還知道有另一個
[PAGE_BREAK]
　方法可以醫治悠妮小姐的失
[PAGE_BREAK]
　憶症，想不想試試看啊？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『有附帶條件吧？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000A]
『別說這麼難聽嘛！是這樣的
[PAGE_BREAK]
　啦，有個叫約拿的賢者住在
[PAGE_BREAK]
　南部的大港普里茲，他見多
[PARAGRAPH]
　識廣，應該知道怎麼醫治悠
[PAGE_BREAK]
　妮小姐的失憶症吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『所以妳就‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000A]
『我正好也要到普里茲港去找
[PAGE_BREAK]
　約拿，這一帶最近都不怎麼
[PAGE_BREAK]
　安寧，所以我想和你們結伴
[PARAGRAPH]
　而行，如此就可以互相照顧
[PAGE_BREAK]
　啦。你們放心，我自己有帶
[PAGE_BREAK]
　護衛隨行，不會太麻煩你們
[PARAGRAPH]
　的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『這樣妥當嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『既然是要治好悠妮的失憶症
[PAGE_BREAK]
　，我想什麼辦法都值得一試
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『說的也是。那我們就一起去
[PAGE_BREAK]
　找約拿吧！還有，希莉亞，
[PAGE_BREAK]
　妳今後打算怎麼辦呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『我還是跟著你們吧，多少也
[PAGE_BREAK]
　可以幫上一些忙。而且和你
[PAGE_BREAK]
　們同行，總是會遇到不少有
[PARAGRAPH]
　趣的事呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這個瘋丫頭真是‥不管了，
[PAGE_BREAK]
　我們上路吧！』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這‥‥這是什麼碗糕！』
[END]
```

### Page 11

```text
[PORTRAIT_LEFT_BY_ID=0x0060]
『糟啦！首領被打倒了！快逃
[PAGE_BREAK]
　啊！』
[END]
```
