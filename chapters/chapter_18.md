# 第 18 章 — 遙遠的彼岸

吊橋上與聖者約拿、聖騎士蘭斯洛特會合，聯手擊潰「死亡骷髏」傭兵團的黑暗騎士。本章是 FD2 首個以擊殺指定 boss 為勝利條件的章節。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 21 (0x15) | 聖者約拿 | 章末 | 無條件 |
| 7 (0x07) | 聖騎士蘭斯洛特 | 章末 | 無條件 |

## 敵人配置

本章 FDFIELD entry 52 共 59 個會生成的 spawn 記錄（另有 11 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 15 | 突擊騎兵 | LV21 | ×18 | default_attacker / aggressive_physical |
| 10 | 鎧甲武士 | LV22 | ×12 | default_attacker / aggressive_physical |
| 21 | 狙擊手 | LV8 | ×10 | default_attacker / aggressive_physical |
| 24 | 巫師 | LV8 | ×6 | default_attacker / aggressive_physical |
| 40 | 魔鬼 | LV7 | ×8 | default_attacker |
| 17 | 黑暗騎士 | LV10 | ×1 | aggressive_physical |
| 27 | 大祭師 | LV8 | ×2 | default_attacker |

單隻的 LV10 黑暗騎士 (17) 即死亡骷髏傭兵團 boss（runtime char[0x34]），擊殺即勝（見 §特殊機制）。第 8 回合敵援軍從地圖後方登場，與橋上部隊左右夾擊。

友軍 NPC（team 1，戰場自走）：

| enemy_data | 單位 | 等級 | 數量 |
|---|---|---|---|
| char 0x15 | 約拿 | LV5 | ×1 |
| char 0x07 | 蘭斯洛特 | LV4 | ×1 |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：暗殺服 (0x8C)、殭屍爪 (0x42)、鑽石 (0xCC)、退麻藥 (0xC5)、再生藥 (0xC2)、水晶弓 (0x62)
- 金錢：10000

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：再生藥 (0xC2)、神聖之水 (0xC3)、水晶粒 (0xCF)
- 金錢：5000

## 商店

無章內商店。

## 特殊機制

- **勝利條件**：擊殺黑暗騎士 boss（runtime slot char[0x34]）。FD2 首個「擊殺指定 boss = win」設計。
- **失敗條件**：索爾（char[0]）、約拿（char[0x10]）、蘭斯洛特（char[0x11]）任一死亡即敗。
- 勝負完全由 `fd2_chapter_18_post_action` 自行實作，bypass default post-action handler（不走通用的全滅判定）。
- **第 8 回合敵援軍**：FDFIELD turn-event hook 於 turn 8 enemy_turn_intro（event 0x2A）觸發援軍出現對白，敵軍從後方與橋上部隊左右夾擊。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_18_init @ 0x000335DA` | 154 B |
| End | `fd2_chapter_18_end @ 0x00023CD5` | 356 B |
| Post-action | `fd2_chapter_18_post_action @ 0x000208CF` | bypass default；自行實作勝負 |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[17]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[17]` |  |

### Init handler

1. `fd2_init_battle_state_for_chapter`
2. `fd2_display_dialog_scene(page=0)`
3. `fd2_pan_cursor_and_window(0x10, 4)` + `fd2_cutscene_event_trigger(0x36)`
4. `fd2_display_dialog_scene(page=1)`
5. `fd2_pan_cursor_and_window(0x10, 4)` + `fd2_cutscene_event_trigger(0x37)`
6. `fd2_display_dialog_scene(page=2)` + `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 18 | 0, 1, 2 |
| End | 18 | 7, 8, 9, 10 |

### char_id 初始化序列

無 init handler 內 char init。約拿 (char_id 0x15 = 21) 與蘭斯洛特 (char_id 7) 由 end handler 加入。

### Cutscene events

- Init：`0x36`, `0x37`
- End：`0x38`, `0x39`, `0x3A` (3 events 連鎖)

### Post-action handler

`fd2_chapter_18_post_action @ 0x208CF` (bypass default — 自行實作所有勝負邏輯)：

- if `chars[0]` OR `chars[0x10]` OR `chars[0x11]` 任一死亡 → game_event_flag = 1 (lose)
- if `char[0x34]` 死亡 → game_event_flag = 2 (win)

| char slot | 角色 | 條件 |
|---|---|---|
| char[0] | 索爾 | 死亡 = 敗 |
| char[0x10] | 約拿 (NPC) | 死亡 = 敗 |
| char[0x11] | 蘭斯洛特 (NPC) | 死亡 = 敗 |
| char[0x34] | 黑暗騎士 boss | 死亡 = 勝 |

註：post_action 以 char[0x10]/[0x11] 為 lose 條件，但 end handler 加入時用 char_id 21/7 (base table id)；這是 runtime slot 與 base char_id 兩套 indices。

### End handler events

`fd2_chapter_18_end @ 0x23CD5` (356 B)：

1. 從 scene tables (`chapter_18_end_scene_pos_x/y_table` + `facing_table`) 讀 5 entries
2. `fd2_save_runtime_char_to_template`
3. `fd2_setup_chars_and_camera_for_intro(0x10, 0x11, 0x19, 8, 1, 0x12, 4)`
4. `fd2_display_dialog_scene(page=7)` + `fd2_cutscene_event_trigger(0x38)`
5. `fd2_display_dialog_scene(page=8)` + `fd2_cutscene_event_trigger(0x39)`
6. `fd2_display_dialog_scene(page=9)` + `fd2_cutscene_event_trigger(0x3A)`
7. `fd2_display_dialog_scene(page=10)`
8. `fd2_init_runtime_char_from_base_growth(0x15=21)` (約拿) + `fd2_init_runtime_char_from_base_growth(7)` (蘭斯洛特)
9. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD tile_event entry idx **52**（= chapter_id × 3 + 1，chapter_id = 17），entry size 1951 bytes。party_member_count = 16、char_spawn_count = 70。header layout 見 `resource_info/fdfield.md`。

**turn-event hooks（2/16 active）**：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 0 (enemy_turn_intro) | 0x2B | `0x00035091` | ai_setup |
| 8 | 0 (enemy_turn_intro) | 0x2A | `0x0003505F` | dialog_only (敵援軍出現) |

## 對話

對話文字 11 pages 來自 FDTXT.DAT entry 18。Init handler 播 page 0/1/2（吊橋會合、約拿現身、死亡骷髏傭兵團來襲）；戰鬥中的 event 對白為 page 3（約拿倒下）、page 4（蘭斯洛特倒下）、page 5（骷髏隊長陣亡）、page 6（第 8 回合援軍夾擊）；End handler 播 page 7/8/9/10（約拿為悠妮診斷失憶症、索爾一行決定繼續同行）。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『好長的吊橋！在這山岳絕頂
[PAGE_BREAK]
　構築這麼長的吊橋，
[PAGE_BREAK]
　一定很不容易吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這吊橋通往哪裡？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『過了這道吊橋，地形便急轉
[PAGE_BREAK]
　直下，通往史威特平原和
[PAGE_BREAK]
　黑暗沼澤‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『咦！有人比我們先到此地了
[PAGE_BREAK]
　呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『老爹，吊橋上有一個高大的
[PAGE_BREAK]
　騎士和一位老者，是吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『老者？莫非是‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『是他的背影沒錯！約拿先生
[PAGE_BREAK]
　！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0015]
『哎呀！可不是索菲亞和貝克
[PAGE_BREAK]
　威嗎？還帶了一票小朋友呢
[PAGE_BREAK]
　你們什麼時候來的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『剛剛才到的，約拿先生，
[PAGE_BREAK]
　真高興能遇見您‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0015]
『呵呵呵！聽說你們一路上戰
[PAGE_BREAK]
　績卓著，想必法術戰技也都
[PAGE_BREAK]
　鍛鍊得不錯了，這對我們日
[PARAGRAPH]
　後與黑暗勢力的決戰大有助
[PAGE_BREAK]
　益‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『小心，約拿先生，您的背後
[PAGE_BREAK]
　有人！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x006C]
『嘿嘿，由他們的話聽來，這
[PAGE_BREAK]
　老頭果然是約拿沒錯！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0055]
『大家聽清楚了，宰了他就可
[PAGE_BREAK]
　以獲得豐厚的賞金！大家努
[PAGE_BREAK]
　力作戰，回去就可以快活度
[PARAGRAPH]
　日了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x004E]
『那還等什麼？
[PAGE_BREAK]
　大夥兒上吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『糟啦，由他們鎧甲上的紋飾
[PAGE_BREAK]
　看來，我們好像有大麻煩了
[PAGE_BREAK]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『怎麼回事？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『如果我沒記錯的話，這是一
[PAGE_BREAK]
　支叫做「死亡骷髏」的黑暗
[PAGE_BREAK]
　傭兵團，這些惡名昭彰的傢
[PARAGRAPH]
　伙專接暗殺和劫掠的生意，
[PAGE_BREAK]
　為了賺血腥錢無所不為！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『我想起來了，以前我們王國
[PAGE_BREAK]
　騎士團曾對他們發動過一次
[PAGE_BREAK]
　清剿，自那之後就沒再聽過
[PARAGRAPH]
　他們的消息了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『他們在此出現，難道是為了
[PAGE_BREAK]
　約拿先生而來？‥‥糟了，
[PAGE_BREAK]
　約拿先生有危險！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『敵人人數不少，我們趕快過
[PAGE_BREAK]
　去支援！走吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0007]
『約拿先生，出現在橋彼端的
[PAGE_BREAK]
　似乎是「死亡骷髏」傭兵團
[PAGE_BREAK]
　這批傭兵應該是衝著我們來
[PARAGRAPH]
　的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0015]
『是麼？敵人終於下殺手了‥
[PAGE_BREAK]
　也好，我好久沒活動這把老
[PAGE_BREAK]
　骨頭啦，還可以順便看看小
[PARAGRAPH]
　子們的表現，蘭斯洛特，你
[PAGE_BREAK]
　應該沒問題吧？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0007]
『請您放心，不管情勢如何，
[PAGE_BREAK]
　我都誓以生命來保護您的安
[PAGE_BREAK]
　全。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0015]
『不用這麼客氣，這又不是我
[PAGE_BREAK]
　們第一次聯手‥‥敵人來了
[PAGE_BREAK]
　我們小心迎戰吧！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『唔，我畢竟是上了年紀，
[PAGE_BREAK]
　玩不起這種運動了‥‥
[PAGE_BREAK]
　啊‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『約拿先生，
[PAGE_BREAK]
　您自己要保重‥‥
[PAGE_BREAK]
　我不行了‥‥』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x0055]
『可惡‥我的同伴們會為我報
[PAGE_BREAK]
　仇的‥‥等著瞧吧‥‥』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_CHAR=0x0035]
『好耐命的老頭！約拿和那個
[PAGE_BREAK]
　騎士居然都還活著？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x003D]
『他們好像找來了幫手，弟兄
[PAGE_BREAK]
　們好像死傷慘重！首領要我
[PAGE_BREAK]
　們來增援果然是對的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0035]
『既然如此，我們就從這裡突
[PAGE_BREAK]
　擊他們的後方，和橋上的弟
[PAGE_BREAK]
　兄左右夾擊，讓他們插翅也
[PARAGRAPH]
　難飛！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x003D]
『好，就這麼辦！
[PAGE_BREAK]
　我們上吧！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_ID=0x0015]
『多謝大家的幫助！瑪琳，索
[PAGE_BREAK]
　菲亞，貝克威，很高興你們
[PAGE_BREAK]
　能趕來此地與我會合；還有
[PARAGRAPH]
　不少我認識的朋友們，我也
[PAGE_BREAK]
　在此向你們問好。至於幾位
[PAGE_BREAK]
　初次見面的‥這幾位年輕的
[PARAGRAPH]
　勇士們怎麼稱呼呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『約拿先生，我叫索爾，這兩
[PAGE_BREAK]
　位是我的好友亞雷斯和悠妮
[PAGE_BREAK]
　小姐，我們來自東方的羅特
[PARAGRAPH]
　帝亞。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0015]
『啊！羅特帝亞嗎？那可真是
[PAGE_BREAK]
　遠道而來了。不知道你們為
[PAGE_BREAK]
　何大老遠前來此地？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『老實說，是想請約拿先生您
[PAGE_BREAK]
　醫治悠妮小姐的失憶症‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0015]
『失憶症？這可難倒我了，如
[PAGE_BREAK]
　果是魔法所造成的，我或許
[PAGE_BREAK]
　還能幫得上忙；但若是頭部
[PARAGRAPH]
　受傷或是疾病引起的，那就
[PAGE_BREAK]
　是僧侶分內的工作了‥‥‥
[PAGE_BREAK]
　不過既然你們都大老遠跑來
[PARAGRAPH]
　了，我還是幫她看看吧，悠
[PAGE_BREAK]
　妮小姐，請妳過來這邊‥』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0015]
『嗯‥‥有趣，有趣！這種事
[PAGE_BREAK]
　我還是第一次見到‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『約拿先生，怎麼樣？』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_ID=0x0015]
『壞消息是，我無法醫治她的
[PAGE_BREAK]
　失憶症，這有特別的原因；
[PAGE_BREAK]
　好消息則是，或許我能告訴
[PARAGRAPH]
　你她來自何處。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這樣也行，我們本來就是要
[PAGE_BREAK]
　送她回家的。約拿先生，能
[PAGE_BREAK]
　告訴我們地點在哪裡嗎？』
[END]
```

### Page 10

```text
[PORTRAIT_LEFT_BY_ID=0x0015]
『現在還不行，因為這涉及我
[PAGE_BREAK]
　正在調查的古代秘密，我自
[PAGE_BREAK]
　己也無法確定‥‥不過，也
[PARAGRAPH]
　許在這次的事情告一段落之
[PAGE_BREAK]
　後，我就可以給你們一個確
[PAGE_BREAK]
　切的答案。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那就是說，我們又得‥‥
[PAGE_BREAK]
　唉！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，別嘆氣啦！我們不如
[PAGE_BREAK]
　就跟著大家完成這場痛快的
[PAGE_BREAK]
　大冒險，這種經驗以後也很
[PARAGRAPH]
　難再有啦！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『是啊！索爾，
[PAGE_BREAK]
　你要是不在了，以後的戰鬥
[PAGE_BREAK]
　就沒人領導了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可是，我只是想趕快回復悠
[PAGE_BREAK]
　妮的記憶‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒關係，索爾，我一直有種
[PAGE_BREAK]
　奇異的預感，我覺得‥我覺
[PAGE_BREAK]
　得好像只要再這樣走下去，
[PARAGRAPH]
　事情一定會有結果的，所以
[PAGE_BREAK]
　我想‥我希望能繼續這段旅
[PAGE_BREAK]
　程‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『真‥真的嗎？好極了，既然
[PAGE_BREAK]
　妳這麼認為，我就沒有意見
[PAGE_BREAK]
　了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『萬歲！
[PAGE_BREAK]
　索爾他們會留下來了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『索爾，我們以後要靠你出力
[PAGE_BREAK]
　的地方還很多，歡迎你繼續
[PAGE_BREAK]
　和我們一起奮鬥。』
[PARAGRAPH]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『謝謝大家！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0015]
『呵呵呵，這裡可不是個敘舊
[PAGE_BREAK]
　的好地方，前面的路上有個
[PAGE_BREAK]
　小鎮，我們還是先離開這個
[PARAGRAPH]
　危險的山崖，去那裡好好休
[PAGE_BREAK]
　息一下吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『我同意！大家上路吧！』
[END]
```
