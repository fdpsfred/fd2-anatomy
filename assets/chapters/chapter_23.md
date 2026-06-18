# 第 23 章 — 向天空之旅

由傳送魔法降落到沙漠綠洲廢墟；遭遇武聖卡里斯與劍聖羅德曼，擊敗古代機兵後，飛行岩起飛載眾人前往天空。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 22 (0x16) | 武聖卡里斯 | end handler | 隊伍持有「天空之鑰」(item 100) |
| 19 (0x13) | 劍聖羅德曼 | end handler | 蜜蒂 (template id 0x12) 不在隊伍 且 戰鬥於 15 回合內結束 |

## 敵人配置

FDFIELD.DAT[23]：古代機兵守衛，含機甲隊長 boss (char[0x12])。後段戰場由 end handler 中段 `fd2_load_dat_resource("FDFIELD.DAT", 0x45)` 載入新地圖。

## 寶物

待解：寶物清單需從 FDFIELD.DAT tile_event 解析。**「天空之鑰」(item 100)** 為跨章兌換鏈關鍵物：
- ch21 結束時可由 6 件物品集齊兌換
- 持有此物 → ch23 卡里斯加入
- 持有此物 → ch27 GOOD ENDING fork (進 ch28+)

## 商店

待解：詳細 enemy/item 配置需從 FDFIELD.DAT entry 解析。

## 特殊機制

- **開場 cinematic**：傳送魔法降落配 screen-wide spell visual + 16-char 全清隊 → revive HP>0 過濾。HP=0 的角色不上場。
- **三重 conditional join**（end handler）：
  - 天空之鑰 → 卡里斯加入；無 → 不加入
  - 蜜蒂在 template → mark char[0x11] dead
  - 蜜蒂未在 + < 15 回合 → 羅德曼加入；蜜蒂未在 + ≥ 15 回合 → mark char[0x11] dead
- **跨章天空之鑰兌換鏈**：本章為「持鑰」第一個分支點 (卡里斯)，下個分支點在 ch27 (GOOD/BAD ending fork)。
- **兩段戰鬥**：end handler 中段 reload `FDFIELD.DAT[0x45]` + `FDSHAP.DAT[0x2E/0x2F]` 進入第二戰場 cinematic，但 chapter_id 在第二戰場結束才 +1。
- **勝負條件**：擊毀機甲隊長 (char[0x12]) = 勝；索爾、希爾法、卡里斯、羅德曼 (chars[0,1,0x10,0x11]) 任一死 = 負。
- **援軍 turn**：第 13、15、18、22 回合敵方 enemy turn intro 時觸發援軍 reinforcement。

## 對話

對話文字 18 pages 來自 FDTXT.DAT entry 23。Init 引用 page 0/1/2/3/4，End handler 依 conditional 引用 page 8 或 9，10 或 0xC 或 0xD 或 0xA+0xB，及 0xE/0xF/0x10/0x11。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『呼！難過死了‥咦！這裡是
[PAGE_BREAK]
　‥敵人的根據地嗎？為何看
[PAGE_BREAK]
　起來一點都不像？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『在傳送魔法啟動後，我感覺
[PAGE_BREAK]
　到有極強的魔力從中干擾，
[PAGE_BREAK]
　傳送魔法的作用因此發生了
[PARAGRAPH]
　錯亂‥希爾法，我說得沒錯
[PAGE_BREAK]
　吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『是的，傳送過程受到這種干
[PAGE_BREAK]
　擾，我們沒有全部嵌到岩壁
[PAGE_BREAK]
　裏去就該慶幸不已了。不過
[PARAGRAPH]
　這裡應該離原來的目的地不
[PAGE_BREAK]
　遠，這個我大概可以確定‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可惡，原來又是敵人搞的鬼
[PAGE_BREAK]
　！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0016]
『咦？什麼時候多了好大一票
[PAGE_BREAK]
　人出來？他們是怎麼來到這
[PAGE_BREAK]
　裡的，我們居然毫無知覺？
[PARAGRAPH]
　』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0013]
『什麼？是敵人嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『看起來不像，還是先過去打
[PAGE_BREAK]
　個照面再說！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哎呀，有人比我們先來了呢
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001B]
『敵人這麼快就來了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『不像，對方只有兩個人而已
[PAGE_BREAK]
　‥好像想和我們談話的樣子
[PAGE_BREAK]
　。』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0016]
『不知各位為何跋涉千里，來
[PAGE_BREAK]
　到這沙漠之中的廢墟？是和
[PAGE_BREAK]
　我們一樣為了研究遺跡而來
[PARAGRAPH]
　的嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『啊！你不是‥卡里斯叔叔嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『你說什麼？你怎麼知道我叫
[PAGE_BREAK]
　卡里斯？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『卡里斯叔叔，我是索爾啊！
[PAGE_BREAK]
　小時候你經常帶我出去玩，
[PAGE_BREAK]
　還教我武術的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『你這麼一說，我也想起來了
[PAGE_BREAK]
　！他的確是曾經教過我們武
[PAGE_BREAK]
　術的卡里斯叔叔！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『你是‥索爾王子嗎？那你就
[PAGE_BREAK]
　是巴拉多的兒子亞雷斯了！
[PAGE_BREAK]
　我記得你們從小就在一起的
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『答對啦！卡里斯叔叔，你果
[PAGE_BREAK]
　然還記得我們！真是好久不
[PAGE_BREAK]
　見了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『聽父王說你到海外來探險，
[PAGE_BREAK]
　我們都神往不已呢！真沒想
[PAGE_BREAK]
　到會在這裡遇到您。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『時間過的真快，你們都長大
[PAGE_BREAK]
　啦！雷特王子‥‥，陛下這
[PAGE_BREAK]
　些日子還好嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『父王母后都很安健，不過老
[PAGE_BREAK]
　嫌生活平淡了些，大概是等
[PAGE_BREAK]
　著要聽卡里斯叔叔您的冒險
[PARAGRAPH]
　故事吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『哪！又遇見了一個熟人，這
[PAGE_BREAK]
　可不是羅德曼嗎？你一個好
[PAGE_BREAK]
　好的邊防將軍不做，又跑來
[PARAGRAPH]
　這裡考古啦？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『果然是約拿你這把老骨頭，
[PAGE_BREAK]
　世界上就你最清楚我的習性
[PAGE_BREAK]
　。你怎麼也捨得跑到這荒山
[PARAGRAPH]
　野外來了？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我倒忘了問你，這裡到底是
[PAGE_BREAK]
　哪裡？我們是被魔法傳送來
[PAGE_BREAK]
　的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『喲，這趟路可真遠了！這裡
[PAGE_BREAK]
　是大陸西北方的沙漠中央的
[PAGE_BREAK]
　一個綠洲，我一直對這綠洲
[PARAGRAPH]
　中的廢墟感興趣，所以和這
[PAGE_BREAK]
　朋友一起來調查。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『不瞞各位，最近有一位學者
[PAGE_BREAK]
　幫我解讀了一篇上古經典的
[PAGE_BREAK]
　記載，我正在追查其中一些
[PARAGRAPH]
　古物的下落，所以碰巧來到
[PAGE_BREAK]
　這裡‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『嗶，發現入侵者！指令﹕保
[PAGE_BREAK]
　護飛行岩，交戰並殲滅入侵
[PAGE_BREAK]
　者。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x006F]
『嗶，指令確認！開始動作！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『嘿，那是什麼怪東西？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『那是典籍中所記載的古代機
[PAGE_BREAK]
　兵，他們負責保護所有古代
[PAGE_BREAK]
　人所留下來的建築和設施，
[PARAGRAPH]
　會將入侵者殺戮殆盡‥‥現
[PAGE_BREAK]
　在沒時間說明了，準備應付
[PAGE_BREAK]
　這些傢伙吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『說到打架，我精神就來了！
[PAGE_BREAK]
　我們上！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_CHAR=0x0012]
『嗶，指令系統失效‥任務中
[PAGE_BREAK]
　止‥指令傳輸關閉‥‥』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『我怎能‥在這種時候‥倒下
[PAGE_BREAK]
　‥‥』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『可惡‥我還沒‥這廢墟的秘
[PAGE_BREAK]
　密‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0016]
『不愧是古代人建造的機兵，
[PAGE_BREAK]
　應付起來真是棘手的很‥幸
[PAGE_BREAK]
　好大家都能獨當一面，才能
[PAGE_BREAK]
　順利消滅這些敵人。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『危機已過，現在我們可以仔
[PAGE_BREAK]
　細研究這個遺跡了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『卡里斯叔叔，這遺跡到底有
[PAGE_BREAK]
　什麼秘密，你還沒告訴我們
[PAGE_BREAK]
　呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『是這樣嗎？其實我也是從古
[PAGE_BREAK]
　籍上看來的。據說眼前這個
[PAGE_BREAK]
　小丘是古代人所造的飛行用
[PARAGRAPH]
　具，經由一支傳送法杖的力
[PAGE_BREAK]
　量，可以把擁有「天空之鑰
[PAGE_BREAK]
　」的人帶到黃金的城堡上去
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『天空之鑰？那不就在我們手
[PAGE_BREAK]
　上嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『「傳送之杖」也有啊！我們
[PAGE_BREAK]
　就是被它傳送到這裡來的。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『真的？那真是太巧了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我一直認為，最近大陸上的
[PAGE_BREAK]
　天地異變和神秘敵人都和這
[PAGE_BREAK]
　「黃金城」有關。在古代傳
[PARAGRAPH]
　說中，黃金城總和戰爭、災
[PAGE_BREAK]
　厄等脫不了關係，所以我們
[PAGE_BREAK]
　一直都在追查這其中的關聯
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『這確是有加以查明的必要。
[PAGE_BREAK]
　索爾殿下，既然這兩件東西
[PAGE_BREAK]
　都已到手，可以讓我和你們
[PARAGRAPH]
　一起去看看嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『當然可以！有卡里斯叔叔同
[PAGE_BREAK]
　行，什麼對手都不足為懼了
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『是呀！』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_ID=0x0016]
『不愧是古代人建造的機兵，
[PAGE_BREAK]
　應付起來真是棘手的很‥幸
[PAGE_BREAK]
　好大家都能獨當一面，才能
[PARAGRAPH]
　順利消滅這些敵人。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『危機已過，現在我們可以仔
[PAGE_BREAK]
　細研究這個遺跡了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『卡里斯叔叔，這遺跡到底有
[PAGE_BREAK]
　什麼秘密，你還沒告訴我們
[PAGE_BREAK]
　呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『是這樣嗎？其實我也是從古
[PAGE_BREAK]
　籍上看來的，據說此地可能
[PAGE_BREAK]
　藏有「天空之鑰」。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我聽過這東西，據說它是通
[PAGE_BREAK]
　往黃金城堡的唯一方法。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『我們並未遇料到會有敵人出
[PAGE_BREAK]
　現，這回算是相當的驚險‥
[PAGE_BREAK]
　不過，我想我還是先回城休
[PARAGRAPH]
　養一下好了，將軍殿下，這
[PAGE_BREAK]
　裡就先交給你了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『卡里斯叔叔，你要多保重喔
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『索爾殿下，您和亞雷斯也要
[PAGE_BREAK]
　多小心！不過以你們的表現
[PAGE_BREAK]
　看來，我想我是可以放心了
[PARAGRAPH]
　。我們後會有期！』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『羅德曼，那你打算怎麼辦呢
[PAGE_BREAK]
　？我很希望能有像你這樣的
[PAGE_BREAK]
　武士加入，這對我們的戰力
[PAGE_BREAK]
　有很大的提昇。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『我‥唉，我想‥我還是不太
[PAGE_BREAK]
　適合。老友，我也要回城了
[PAGE_BREAK]
　，希望你們此行順利，都能
[PARAGRAPH]
　平安歸來。‥‥』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0012]
『唉，都是因為我的關係‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『前輩，這怎麼說？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『這是一件很久以前的往事了
[PAGE_BREAK]
　‥‥那時羅德曼將軍還在禁
[PAGE_BREAK]
　衛軍統領任內，而我才剛成
[PARAGRAPH]
　為禁衛軍騎士不久‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『啊！我想起來了！那時前輩
[PAGE_BREAK]
　妳向將軍他‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『是的，那時還年輕的我被感
[PAGE_BREAK]
　情沖昏了頭，在大庭廣眾面
[PAGE_BREAK]
　前向將軍表明愛意。但是將
[PARAGRAPH]
　軍的夫人妮莉公主是陛下最
[PAGE_BREAK]
　小的妹妹，我這麼做讓他非
[PAGE_BREAK]
　常為難，結果將軍向陛下請
[PARAGRAPH]
　調邊疆，就只為了避開我‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『原來是這樣‥‥。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『那時的我傷心欲絕，雖把全
[PAGE_BREAK]
　部心思用在劍術上，仍然忘
[PAGE_BREAK]
　不了這件事，因此才又離開
[PARAGRAPH]
　騎士團到深山中修練‥這麼
[PAGE_BREAK]
　多年以後，我已經不再釋懷
[PAGE_BREAK]
　，但將軍還這麼在乎這件事
[PARAGRAPH]
　，你們就可以知道當年我讓
[PAGE_BREAK]
　他多麼困擾了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒關係！密蒂前輩，將軍的
[PAGE_BREAK]
　份就算是由妳代替了，今後
[PAGE_BREAK]
　妳不但是為了大家而戰，也
[PARAGRAPH]
　是為了將軍而戰，過去的遺
[PAGE_BREAK]
　憾和感傷，就讓它在戰陣中
[PAGE_BREAK]
　煙消雲散！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『‥索爾，謝謝你。我會記住
[PAGE_BREAK]
　這句話的。』
[END]
```

### Page 12

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『羅德曼，那你打算怎麼辦呢
[PAGE_BREAK]
　？我很希望能有像你這樣的
[PAGE_BREAK]
　武士加入，這對我們的戰力
[PAGE_BREAK]
　有很大的提昇。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『老友，我很希望能與你們同行
[PAGE_BREAK]
　，可是我是受陛下的任命，
[PAGE_BREAK]
　在此擔任邊防將軍，
[PARAGRAPH]
　除非有陛下的諭命，否則擅
[PAGE_BREAK]
　離職守視同抗命。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『是這樣嗎？‥真是遺憾，我本
[PAGE_BREAK]
　來以為可以再看到你昔日的
[PAGE_BREAK]
　英姿呢‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『這也是件沒辦法的事‥老友，
[PAGE_BREAK]
　保重！我得回城了。願你們
[PAGE_BREAK]
　此行順利，平安歸來。』
[END]
```

### Page 13

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『羅德曼，那你打算怎麼辦呢
[PAGE_BREAK]
　？我很希望能有像你這樣的
[PAGE_BREAK]
　武士加入，這對我們的戰力
[PARAGRAPH]
　有很大的提昇。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『是嗎？難得聽你這樣誇讚我
[PAGE_BREAK]
　，如果我這把老骨頭還能派
[PAGE_BREAK]
　得上用場，那當然是樂意之
[PARAGRAPH]
　至。哈哈哈！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『太好啦！我知道羅德曼將軍
[PAGE_BREAK]
　的戰技一直都是沒話說的！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『咦，妳不是‥妳不是希莉亞
[PAGE_BREAK]
　公主嗎？為何殿下‥會在這
[PAGE_BREAK]
　裡？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『將軍你都可以蹺班來考古了
[PAGE_BREAK]
　，我怎麼可以待在宮裏發呆
[PAGE_BREAK]
　呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『好啦，別大驚小怪了，為了
[PAGE_BREAK]
　解決這件事情，王國的強者
[PAGE_BREAK]
　勇士幾乎都到齊了，我們先
[PARAGRAPH]
　想想接下來要怎麼辦吧！』
[END]
```

### Page 14

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『‥我試了一下，這小丘上的
[PAGE_BREAK]
　法陣好像對這法杖上的魔力
[PAGE_BREAK]
　有反應，所以我想實驗看看
[PARAGRAPH]
　，看能不能讓它飛起來‥。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『快試，快試！我等不及要看
[PAGE_BREAK]
　了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『好吧，那我現在就開始‥米
[PAGE_BREAK]
　洛耶爾．希里卡恩．耶．哈
[PAGE_BREAK]
　洛納！』
[END]
```

### Page 15

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『地震了！』
[END]
```

### Page 16

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哇啊啊，好可怕！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『可能要起飛了，大家站穩！
[PAGE_BREAK]
　』
[END]
```

### Page 17

```text
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哇！飛了，飛了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001B]
『真是神奇！古代人的智慧和
[PAGE_BREAK]
　力量，果然不是我們所能想
[PAGE_BREAK]
　像的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這飛行岩開始高速飛行了‥
[PAGE_BREAK]
　希爾法，你知道它是要飛到
[PAGE_BREAK]
　哪裡去嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『嗄？這‥我也太不清楚，我
[PAGE_BREAK]
　只有使用這法杖啟動它而已
[PAGE_BREAK]
　，現在‥現在它是在飛它自
[PARAGRAPH]
　己的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『那‥那怎麼辦？我們總不能
[PAGE_BREAK]
　就這樣一直飛下去吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『別緊張‥‥這飛天丘總會飛
[PAGE_BREAK]
　到一個目的地的，這是在很
[PAGE_BREAK]
　久以前就已經決定好了的‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『悠妮小姐，妳知道和這飛行
[PAGE_BREAK]
　岩有關的事嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥好像有點印象‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『既然悠妮這麼說，我們就靜
[PAGE_BREAK]
　觀其變吧！看看這玩意到底
[PAGE_BREAK]
　會把我們帶到哪裡去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『呀呼！這樣飛真是過癮呢！
[PAGE_BREAK]
　』
[END]
```
