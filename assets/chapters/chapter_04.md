# 第 4 章 — 塞拉村前

抄近路前往塞拉村途中遭強盜伏擊，並有半獸人路人加入混戰。

## 加入角色

無新加入角色。

## 敵人配置

- LV6 盜賊頭目
- LV5 盜賊 × 12
- LV3 僧侶 × 2 (有治療術)

敵人配置寫在 FDFIELD.DAT entry 10。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup（攻略本提及共 10 件）。

## 商店

神秘商店 (Alt+F3) — 含風精之羽 $20000，hack table 觸發。

## 特殊機制

- **失敗條件**：索爾死亡（default handler）。
- **End handler 極簡**：61 B，純對話 + 推進章節，不加入新角色、不給 reward。

## 對話

對話文字 7 pages 來自 FDTXT.DAT entry 4。Init 引用 page 0/1，End 引用 page 4。

### Page 0

```text
[PORTRAIT_LEFT_BY_ID=0x0008]
『快到賽拉村啦！抄這條近路
[PAGE_BREAK]
　很快就到了。怎樣？
[PAGE_BREAK]
　還是帶著我比較好吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『老是走這種小道，
[PAGE_BREAK]
　搞不好會碰上強盜喔！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『誰說的，這條路隱密的很，
[PAGE_BREAK]
　只有我才知道‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『看，前面好像有人呢！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0061]
『你們想到賽拉村嗎？
[PAGE_BREAK]
　你們是來幹什麼的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『干你屁事！不要礙路！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0061]
『首領說得沒錯，果然有人抄
[PAGE_BREAK]
　小路來偷襲了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『什麼偷襲？把話講清楚！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0061]
『少裝了！死吧！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0018]
『嗷嗚！‥是誰在這裡大吵
[PAGE_BREAK]
　大鬧？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0019]
『吼‥好像是人類在打架。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x001A]
『呼‥好像很有趣，我們去
[PAGE_BREAK]
　打人少的那一邊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x001B]
『贊成！贊成！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0066]
『好可怕！不玩了！不玩了
[PAGE_BREAK]
　！』
[END]
```

### Page 4

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『奇怪了，在這大陸上到處
[PAGE_BREAK]
　都遇得上強盜！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『不對，
[PAGE_BREAK]
　賽拉村可能出事了！
[PAGE_BREAK]
　我們趕快趕過去看看！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『打了一場大戰，不休息一
[PAGE_BREAK]
　下嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『放心，途中有個小村落可
[PAGE_BREAK]
　以稍事休息。走吧！』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這‥‥這是什麼碗糕！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0061]
『哇‥啊‥‥！』
[END]
```
