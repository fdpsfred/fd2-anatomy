# 第 1 章 — 初試身手

30 章中唯一含獨家 prologue 的章節（FD2 世界觀開場 cutscene）。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 0 | 索爾 (Lan) | 章首 | 主角，每章必在 |
| 4 | 亞雷斯 | 章首 | 無條件 |
| 9 | 悠妮 | 章首預初始化但暫未上場 | ch2+ template 用 |
| 30 (0x1E) | 蓋亞 | 章首 cutscene | 暫時出場 NPC |
| 1 | 哈諾 | 第 3 回合 reinforcement | 第 3 回合 FDFIELD turn-event hook 觸發加入；若哈諾還未出現便已消滅完敵人，哈諾不會加入 |

## 敵人配置

- LV2 盜賊 × 7 (HP28, AP24, DP4, DX2, MV4)
- LV2 盜賊 × 4
- LV3 海盜頭目 (HP72, AP34, DP11, DX6, MV4)
- LV2 盜賊 × 4
- LV2 士兵 × 4 (HP36, AP20, DP6, DX2, MV4) — 友方海防隊

敵人配置寫在 FDFIELD.DAT entry 1（chapter_id × 3 + 0 的 tile_map 與 +1 的
char_spawn_records），不在 init handler。

## 寶物

- 5000 元
- 3000 元
- 藥草
- 1000 元

由 FDFIELD tile_event_id 觸發 pickup，章首 `party_total_gold = 0` 重置。

## 特殊機制

- **獨家 prologue**：3-phase init handler（其他章大多只有 Phase 2/3）。FDTXT 對話
  跨 entry 33 (prologue) → entry 32 (intro) → entry 1 (start)。
- **哈瓦特暴走**：哈諾死後，哈瓦特的 protective AI 因失去 ai_target 自然 fall
  through 為 default attacker。屬 implicit consequence，非 turn-triggered AI flip。
- **勝負條件**：default — 全敵死=勝、索爾死=負。

## 對話

對話文字 12 pages 來自 FDTXT.DAT entry 1。Init Phase 3 引用 page 0/1/2，
End handler 引用 page 9。其他 pages 由 FDFIELD turn-event handler 內部
`display_dialog_scene` 引用。

(prologue 階段使用的 FDTXT entry 32 / 33 內容歸屬 ch1 prologue / intro，
詳見對應章節的 endgame 區段。本章對話內容下列以 entry 1 為主。)

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『累死了，
[PAGE_BREAK]
　大家休息一下吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0004]
『聽說再越過這片海洋
[PAGE_BREAK]
　就到馬拉大陸了，
[PAGE_BREAK]
　我們先在此休息一會兒，
[PARAGRAPH]
　等海水漲潮適合上岸的時候
[PAGE_BREAK]
　船夫就會來接我們。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好極了。悠妮，
[PAGE_BREAK]
妳‥‥嗯，坐了這麼久的船，
[PAGE_BREAK]
有點累吧？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『嗯，還好。
[PAGE_BREAK]
　海風吹起來真舒服啊‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001E]
『‥‥！！！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x000A]
『瞧！
[PAGE_BREAK]
　竟有呆鳥在這小島上休息，
[PAGE_BREAK]
　真是天上掉下來的肥肉。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0009]
『就是說嘛！
[PAGE_BREAK]
　俺去通報老大支援，
[PAGE_BREAK]
　你們趕快把這些小伙子擺
[PAGE_BREAK]
　平。』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『亞雷斯，那群傢伙
[PAGE_BREAK]
　在那裡鬼叫些什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『嗯‥‥嘿，好像是船夫
[PAGE_BREAK]
　提到過的海盜耶！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『海盜？
[PAGE_BREAK]
　是要來搶劫我們的嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『豈止搶劫而已，聽說
[PAGE_BREAK]
　這批海盜橫行馬拉大陸沿
[PAGE_BREAK]
　海，殺人越貨無所不為。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『什麼！要打架我奉陪，
[PAGE_BREAK]
　要搶劫嘛門都沒有。
[PARAGRAPH]
　亞雷斯，好久沒活動活動
[PAGE_BREAK]
　筋骨了，你沒問題吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『豈止沒問題而已，
[PAGE_BREAK]
　我手癢難熬，
[PAGE_BREAK]
　都快受不了啦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『喂！小子們，
[PAGE_BREAK]
　乖乖的把身上的錢財和
[PAGE_BREAK]
　那個漂亮小妞交出來，
[PARAGRAPH]
　我們就在老大面前說說好話
[PAGE_BREAK]
　保你們一命！不然‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『什麼是漂亮小妞？
[PAGE_BREAK]
　是指我嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『不是啦！那是‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可惡，
[PAGE_BREAK]
　你們這些亂說話的海盜，
[PAGE_BREAK]
　我要把你們全宰了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000A]
『啊呀，
[PAGE_BREAK]
　看來他們想抵抗呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『那就殺無赦！
[PAGE_BREAK]
　上啊！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0003]
『真是的，吵的要命，
[PAGE_BREAK]
　到底在搞什麼‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『老爸，
[PAGE_BREAK]
　有人在島上打架呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『什麼？待我看看‥
[PAGE_BREAK]
　啊哈，可不是海盜
[PAGE_BREAK]
　在打劫旅客嗎？
[PARAGRAPH]
　居然在我們門前搶人，
[PAGE_BREAK]
　膽子不小啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『就是說啊！
[PAGE_BREAK]
　老爸，您說這該怎麼辦？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『小子啊，
[PAGE_BREAK]
　這可是絕佳的歷練機會，
[PAGE_BREAK]
　我們就幫他們一個忙吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『老頭子，
[PAGE_BREAK]
　你們是來幹什麼的？
[PAGE_BREAK]
　若不想受傷就別過來
[PAGE_BREAK]
　湊熱鬧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『別緊張，小伙子，
[PAGE_BREAK]
　我們是來幫你們打退海盜
[PAGE_BREAK]
　的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『什麼？就這一點海盜，
[PAGE_BREAK]
　還用不著你們來幫忙！
[PAGE_BREAK]
　回去回去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『哎，索爾，人家是好意
[PAGE_BREAK]
　要幫忙，你也說幾句
[PAGE_BREAK]
　好聽一點的吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這樣嗎？好吧，那就‥喂，
[PAGE_BREAK]
　老頭子，如果你要幫忙的話
[PAGE_BREAK]
　就先說聲多謝了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『這些年輕人真有精神啊！
[PAGE_BREAK]
　小子，我們上！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『好的，老爸！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x000F]
『咦，
[PAGE_BREAK]
　弟兄們好像陷入苦戰呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『那還等什麼！我們上吧！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x0061]
『小子們，你們在搞些什麼！
[PAGE_BREAK]
　連這些小鬼都收拾不了，
[PAGE_BREAK]
　還要勞動老大我出馬，
[PAGE_BREAK]
　不怕給別人笑話嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，這難看又吵人的傢伙
[PAGE_BREAK]
　又是來幹什麼的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『大概是海盜首領吧！
[PAGE_BREAK]
　總算有個像樣的對手上場了
[PAGE_BREAK]
　這傢伙就交給我來處理啦！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那怎麼可以！
[PAGE_BREAK]
　對付這種麻煩的傢伙，
[PAGE_BREAK]
　當然是非我莫屬了！
[PAGE_BREAK]
　讓我來！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，你真是不夠朋友！
[PAGE_BREAK]
　就為了在喜歡的女孩子
[PAGE_BREAK]
　面前逞能，竟然‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『你‥你胡說些什麼！
[PAGE_BREAK]
　再亂說的話，我可就‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0061]
『喂！你們兩個到底在吵些
[PAGE_BREAK]
　什麼！要吵的話，等進了
[PAGE_BREAK]
　地獄後再吵也不遲！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『你煩什麼煩！
[PAGE_BREAK]
　滾一邊去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『這傢伙討厭得很，索爾，
[PAGE_BREAK]
　我們不吵了，
[PAGE_BREAK]
　先合力宰了他再說！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好，就這麼辦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0061]
『可惡啊！這兩個小子全不把
[PAGE_BREAK]
　我放在眼裡！給我殺！
[PAGE_BREAK]
　一個都別放過！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0044]
『咦！又有海盜在此逞兇了，
[PAGE_BREAK]
　現在正是我們海防隊建功
[PAGE_BREAK]
　的良機！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『今天可真是熱鬧啊！
[PAGE_BREAK]
　你們又是來幹什麼的？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0044]
『我們是亞克斯王國的海岸巡
[PAGE_BREAK]
　防隊，消滅肆虐沿海的海盜
[PAGE_BREAK]
　本是我們的職責，這些海盜
[PARAGRAPH]
　就交給我們來處理，請各位
[PAGE_BREAK]
　放心！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『鬼扯，我們打的正高興，
[PAGE_BREAK]
　才不需要你們來攪局！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾啊，
[PAGE_BREAK]
　我們就要踏進人家的地盤，
[PAGE_BREAK]
　至少對人家客氣點吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0044]
『沒關係！
[PAGE_BREAK]
　我們這就來幫忙了！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_ID=0x0001]
『老爸，我‥我不行啦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『小子，你先回屋子裡休息一
[PAGE_BREAK]
　下吧！這裡有老爸擋著！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『老爸，那我就先回去了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『你們這些該死的海盜，
[PAGE_BREAK]
　竟敢砍傷我兒子，
[PAGE_BREAK]
　我和你們拼了！』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0061]
『可恨啊！今日先碰上這群棘
[PAGE_BREAK]
　手的小子在先，又遇上王國
[PAGE_BREAK]
　海防隊，真是天亡我也！啊
[PAGE_BREAK]
　‥‥』
[END]
```

### Page 9

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『多謝老爹和您公子的幫忙，
[PAGE_BREAK]
　我們才能順利打敗海盜。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『那裡！我和小子住在這島上
[PAGE_BREAK]
　除了偶爾出去遊歷外，平常
[PAGE_BREAK]
　也閒來無事，幫你們打打海
[PARAGRAPH]
　盜，不算什麼啦！
[PAGE_BREAK]
　對了，說到這個，老頭子我
[PAGE_BREAK]
　倒有個請求。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『老爹您說說看。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『我瞧我這小子和你們滿合得
[PAGE_BREAK]
　來的，希望你們能帶他同行
[PAGE_BREAK]
　，讓他出去見識見識，歷練
[PARAGRAPH]
　一番。我從小教了他不少武
[PAGE_BREAK]
　術，當你們有麻煩的時候，
[PAGE_BREAK]
　相信他也可以幫上一點忙。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這個好像有點‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『哎！老爹您這個請求當然是
[PAGE_BREAK]
　沒問題的啦！悠妮妳說是不
[PAGE_BREAK]
　是？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『多一個同伴，
[PAGE_BREAK]
　路上也比較熱鬧啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『真是太謝謝各位了。
[PAGE_BREAK]
　小子啊，來向大家打個招呼
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『我‥我叫哈諾，
[PAGE_BREAK]
　以後請‥請大家多多指教
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『以後你就是我們的伙伴了。
[PAGE_BREAK]
　平時我們要好好相處，
[PAGE_BREAK]
　遇到危難時要同心協力，
[PAGE_BREAK]
　好嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『好‥好的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『漲潮的時間也差不多到了，
[PAGE_BREAK]
　你們可以準備上路了。
[PAGE_BREAK]
　我這小子就拜託你們啦！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『老爹您放心吧！
[PAGE_BREAK]
　我們走囉！』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『奈野啊捏？』
[END]
```

### Page 11

```text
[PORTRAIT_LEFT_BY_ID=0x0001]
『老爸！老爸！
[END]
```
