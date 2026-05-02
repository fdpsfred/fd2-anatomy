# 第 11 章 — 幻之森林

精靈族藏寶之地的迷霧森林結界失效，獸人趁機搶寶；舊識魔法師珊出現並加入隊伍。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 14 (0xE) | 珊 | 章末 | End handler 加入 |

## 敵人配置

由 FDFIELD.DAT entry 31 的 char_spawn_records 決定（chapter_id × 3 + 1, chapter_id = 10）。詳見 `resource_info/fdfield.md`。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。

## 特殊機制

- **珊跟隨貝克威 AI**：攻略「珊會跟著貝克威走」屬 NPC follow AI behavior（NPC class 0xB heal/follow logic）。
- **勝負條件**：default — 全敵死 = 勝、索爾死 = 負。

## 對話

對話文字 4 pages 來自 FDTXT.DAT entry 11。Init 引用 page 0/1/2，End handler 引用 page 3。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x000D]
『索菲亞，妳確定約拿是往
[PAGE_BREAK]
　這裡走的嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『他說他會一路往北，經由
[PAGE_BREAK]
　冰海的冰原到黑森林去，
[PAGE_BREAK]
　走這段行程的話，我想這條
[PARAGRAPH]
　路應該不會錯‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『不知怎的，
[PAGE_BREAK]
　我總覺得氣氛不大對‥
[PAGE_BREAK]
　會不會是走錯路了？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『嗯‥這附近的景色看起來
[PAGE_BREAK]
　是頗為陌生。早知道不該抄
[PAGE_BREAK]
　近路的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『小心！附近有人！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『吼嗚！
[PAGE_BREAK]
　‥我看到前面有人類！
[PAGE_BREAK]
　怎麼辦？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『呼‥知道這裡有寶物的應
[PAGE_BREAK]
　該只有我們啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0013]
『吼‥管他的，
[PAGE_BREAK]
　妨礙我們的都得死！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0015]
『對！先宰了他們再說！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『麻煩了，這種獸人低智商
[PAGE_BREAK]
　又不講理，我們準備應戰吧
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『不過，他們提到的寶物是
[PAGE_BREAK]
　什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『不知道。此地應該離艾爾
[PAGE_BREAK]
　夫森林不遠，可是這裡的景
[PAGE_BREAK]
　色我卻從未看過‥‥』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x000E]
『咦？你不是貝克威嗎？
[PAGE_BREAK]
　你怎麼會跑到這裡來了？』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x000D]
『妳是以前住在我家隔壁的
[PAGE_BREAK]
　珊嘛！好久不見啦，聽說妳
[PAGE_BREAK]
　後來跟著大法師穆勒學法術
[PARAGRAPH]
　去了‥對了，妳在這裡幹什
[PAGE_BREAK]
　麼？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『這裡就是我們精靈族歷代
[PAGE_BREAK]
　長老的藏寶之處，以前叫做
[PAGE_BREAK]
　「迷霧森林」的地方‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『對，我想起來了‥這裡一
[PAGE_BREAK]
　向有大霧籠罩，我們從小就
[PAGE_BREAK]
　被告誡不準接近‥‥這又怎
[PARAGRAPH]
　麼了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『最近天地異變，千年來保
[PAGE_BREAK]
　護此地的結界不知怎的失效
[PAGE_BREAK]
　了，這些獸人得知此地有寶
[PARAGRAPH]
　物，就伺機前來搶奪。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『有這種事！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『吼‥‥原來這些人都是妳
[PAGE_BREAK]
　帶來的！都給我一起殺了
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『貝克威，和我一起保護這
[PAGE_BREAK]
　些寶物吧！至少不要讓它們
[PAGE_BREAK]
　落入獸人的手中‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『可是我們還要趕路‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒關係，趁此機會活動活
[PAGE_BREAK]
　動筋骨，也沒什麼不好啊！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『謝了，索爾！珊，妳要多
[PAGE_BREAK]
　小心點！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『不要緊，我躲在你後面就
[PAGE_BREAK]
　可以了，我知道你會保護我
[PAGE_BREAK]
　的！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『說不過妳‥‥好，
[PAGE_BREAK]
　大家上吧！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x000E]
『有賴各位的幫助，終於把
[PAGE_BREAK]
　這些獸人驅離此地‥‥
[PAGE_BREAK]
　我代表精靈族謝謝各位。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『小事一樁！
[PAGE_BREAK]
　對了，貝克威先生，
[PAGE_BREAK]
　你難得遇見一個童年舊友，
[PARAGRAPH]
　不留下來和她‥‥
[PAGE_BREAK]
　好好聚聚嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『約拿囑咐的事情非常緊急
[PAGE_BREAK]
　雖然這愛爾夫森林是我的家
[PAGE_BREAK]
　鄉，我想我沒時間在此停留
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『非常緊急的事？
[PAGE_BREAK]
　快告訴我！
[PAGE_BREAK]
　一定很有趣吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『珊！這是很正經的事，
[PAGE_BREAK]
　可不是小孩時代的玩意‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『什麼！你以為我在開玩笑！
[PAGE_BREAK]
　好，我就和你一起去一趟，
[PAGE_BREAK]
　看看到底是什麼大事！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『別亂來！
[PAGE_BREAK]
　這趟旅程相當危險，
[PAGE_BREAK]
　妳還是乖乖待在這裡‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『既然旅程危險，我就不會擔
[PAGE_BREAK]
　心你嗎？至少我也是大法師
[PAGE_BREAK]
　穆勒的高徒耶！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『唉！說不過妳‥‥反正妳在
[PAGE_BREAK]
　家鄉八成也一樣不安分，
[PAGE_BREAK]
　就跟著我去好啦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『太棒了，謝謝你！
[PAGE_BREAK]
　我叫做珊，
[PAGE_BREAK]
　請大家以後多多指教！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『歡迎加入！那麼，
[PAGE_BREAK]
　現在請妳當嚮導帶我們走過
[PAGE_BREAK]
　這片森林吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000E]
『沒問題！大家跟我來吧！』
[END]
```
