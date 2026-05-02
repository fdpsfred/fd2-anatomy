# 第 12 章 — 北山道

於通往北森林的山道遭遇被獸人追殺的米亞斯多德；得知獸人即將進攻精靈城鎮哈斯米爾。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 17 (0x11) | 米亞斯多德 | 章末 | End handler 加入 |

## 敵人配置

由 FDFIELD.DAT entry 34 的 char_spawn_records 決定（chapter_id × 3 + 1, chapter_id = 11）。詳見 `resource_info/fdfield.md`。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。

## 特殊機制

- **失敗條件**：索爾死亡 / 米亞斯多德 (char[0xE]) 死亡，由 `chapter_12_post_action` 判定。
- **第一回合敵方援軍出現**：對應 FDFIELD turn = 1、phase = 0 的 cinematic_no_dialog event hook。
- **米亞斯多德 NPC follow**：攻略「米亞斯多德跟隨貝克威/珊」屬 NPC AI follow 行為。
- **Init cutscene 順序倒置**：cutscene events 0x28、0x29 在 dialog page 0 之前觸發。

## 對話

對話文字 5 pages 來自 FDTXT.DAT entry 12。Init 引用 page 0，End handler 引用 page 3 與 page 4。Page 1/2 由 FDFIELD turn-event handler 引用。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x000E]
『過了這山道，就到我們位在
[PAGE_BREAK]
　北森林的城鎮了。你們可以
[PAGE_BREAK]
　先在那裡休息一下‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『瞧，前面有人呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『咦？又是一大群獸人，
[PAGE_BREAK]
　還有‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0011]
『哎呀，有救了有救了！
[PAGE_BREAK]
　救命啊！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『你為什麼會被獸人追殺呢？
[PAGE_BREAK]
　我好想知道，
[PAGE_BREAK]
　趕快告訴我！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0011]
『如果你能打敗這些獸人救我
[PAGE_BREAK]
　一命，我不但告訴你為什麼
[PAGE_BREAK]
　還可以額外告訴你一個大秘
[PARAGRAPH]
　密！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『呀！聽起來很有趣，
[PAGE_BREAK]
　我答應‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『喂！
[PAGE_BREAK]
　不要給大夥兒找麻煩‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『沒關係，反正我們急著要過
[PAGE_BREAK]
　這山道，順便擺平這些獸人
[PAGE_BREAK]
　也沒什麼不好。
[PARAGRAPH]
　索爾，我說的沒錯吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『亞雷斯，你越來越瞭解我了
[PAGE_BREAK]
　不過何必這麼囉唆一一咱們說
[PAGE_BREAK]
　上就上！殺啊！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『這小子真是‥‥』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x000F]
『可惡‥
[PAGE_BREAK]
　但是還沒有結束‥‥』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0011]
『‥‥我不甘心‥‥
[PAGE_BREAK]
　寶石的秘密‥‥
[PAGE_BREAK]
　‥‥啊‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0011]
『我叫做米亞斯多德。
[PAGE_BREAK]
　感謝各位的幫忙，
[PAGE_BREAK]
　我才能撿回一命。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『米亞‥好囉唆的名字，
[PAGE_BREAK]
　你不是說，救了你你就會告
[PAGE_BREAK]
　訴我們什麼大秘密嗎？
[PARAGRAPH]
　快說吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0011]
『嘿，其實這個秘密你們不
[PAGE_BREAK]
　一定會有興趣，不過我還是
[PAGE_BREAK]
　說出來好了。
[PARAGRAPH]
　昨天夜裡我偷聽到獸人部隊
[PAGE_BREAK]
　的談話，牠們要進攻精靈城
[PAGE_BREAK]
　鎮哈斯米爾‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_RIGHT_BY_ID=0x000D]
『什麼！那不是我們的故鄉
[PAGE_BREAK]
　嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『糟了，我們要趕快回去警
[PAGE_BREAK]
　告鎮民們才行！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『哈斯米爾不是位在北森林
[PAGE_BREAK]
　的精靈都城嗎？這可奇怪了
[PAGE_BREAK]
　獸人族什麼時候有膽量攻打
[PARAGRAPH]
　精靈族了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0011]
『但我確實看到相當龐大的
[PAGE_BREAK]
　獸人部隊集結，牠們總該不
[PAGE_BREAK]
　是在大露營吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『這不重要，
[PAGE_BREAK]
　我們趕快前往哈斯米爾，
[PAGE_BREAK]
　幫精靈族打退獸人大軍。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『不好意思，
[PAGE_BREAK]
　這回又要麻煩你們了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『既然是朋友的事，我們怎
[PAGE_BREAK]
　能袖手旁觀呢！我們快上路
[PAGE_BREAK]
　吧，要不然就真的來不及了
[PARAGRAPH]
　！』
[END]
```
