# 第 5 章 — 塞拉村

塞拉村被盜賊團卡特那洗劫，戰勝後僧侶瑪琳加入隊伍，並指引前往普里茲港找賢者約拿。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 10 | 瑪琳 | End handler 末段 | 章末固定加入 |

## 敵人配置

- LV8 卡特那 (HP240, AP90)
- LV7 盜賊頭目 × 3 (HP168)

敵人配置寫在 FDFIELD.DAT entry 13。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。

## 商店

神秘商店 (Shift+F4) — 力量/耐力/速度藥水 + 生命之實，hack table 觸發。

## 特殊機制

- **失敗條件**：索爾死亡（default handler）。
- **章末加入**：瑪琳 (char 10) 由 end handler `init_runtime_char_from_base_growth(10)` 加入。
- **跨章劇情**：祭司告知「失憶症神術不能治」並指引去普里茲港找賢者約拿，串聯下一章。

## 對話

對話文字 12 pages 來自 FDTXT.DAT entry 5。Init 引用 page 0/1/2，End 引用 page 9。

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
