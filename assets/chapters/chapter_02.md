# 第 2 章 — 羅德鎮

進入馬拉大陸第一座城鎮，遇上洗劫鎮民的強盜團；保住所有村民可獲隱藏 reward。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 8 | 希莉亞 | End handler 末段 | 章末 cutscene 後固定加入 |

## 敵人配置

- LV2 盜賊 × 10 (HP28, AP24, DP4, DX2, MV4)
- LV2 盜賊 × 6
- 友方 LV3 男村民 × 3 + LV3 女村民 × 3 = 6 NPC

敵人配置寫在 FDFIELD.DAT entry 4 (chapter_id × 3 + 1)，不在 init handler。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup（位置寫在 FDFIELD.DAT[4] tile event 區段）。

## 商店

武器 / 道具 / 神秘商店（神秘商店為 hack table 觸發）。

## 特殊機制

- **失敗條件**：索爾死亡，或 6 個村民 (chars[5..10]) 任一死亡。
- **隱藏 reward**：6 個村民全活 → end handler 給予 item 0xC6 = 力量藥水 (AP+9)。
  攻略本「為了保住所有村民，最好幫亞雷斯買長戟」即此機制。
- **章末加入**：希莉亞 (char 8) 由 end handler `fd2_init_runtime_char_from_base_growth(8)` 加入。

## 對話

對話文字 17 pages 來自 FDTXT.DAT entry 2。Init 引用 page 0/1/2/3，End handler 依
villager 存活分支引用 page 6 或 7，再接 page 8/9/10。

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
