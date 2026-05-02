# 第 28 章 — 探索者

由轉送站進入黃金城（第一空中要塞）內部，遭遇要塞防衛部隊；由悠妮帶路前往黃金城核心區域奪回中樞系統控制權。

## 加入角色

無新加入角色。

## 敵人配置

FDFIELD.DAT[28]：要塞防衛機甲部隊（多 wave）。

## 寶物

待解：寶物清單需從 FDFIELD.DAT tile_event 解析。

## 商店

無（黃金城內部）。

## 特殊機制

- **Init 全清隊 20 chars + revive HP>0**：與 ch23 同設計但範圍更大（20 chars vs 16 chars）。HP=0 的角色不上場，但 ch28 不設 sprite facing。
- **3× 重複 cutscene 0x55**：3 個並行 group 各執行一次同 walk-animation script，模擬多隊伍同時出場 cinematic。
- **最小 end handler**（40 B）：僅 `display_dialog_scene(page=7)` + save + chapter_id +1。
- **勝負條件**：default + chars[1] (悠妮) 死 = 負。
- **藍色平台左側火焰 tile-step**：到達該 tile 時，FDFIELD tile-step handler rewrite turn-event hook table，使下回合敵 turn 觸發援軍 spawn。

## 對話

對話文字 8 pages 來自 FDTXT.DAT entry 28。Init 引用 page 0，End 引用 page 7。Page 1-6 由 FDFIELD turn-event / tile-step handler 引用。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這��就是黃金城嗎？好奇怪
[PAGE_BREAK]
　的建築，這哪像什麼城堡！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『說是一座金屬作成的要塞還
[PAGE_BREAK]
　差不多，從外面的雲海看來
[PAGE_BREAK]
　，我們好像是在很高的地方
[PARAGRAPH]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『這裡是用來傳送機甲兵團到
[PAGE_BREAK]
　地上用的轉送站，由此就可
[PAGE_BREAK]
　以進入黃金城內部。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『看來古代人必定擁有極高度
[PAGE_BREAK]
　的文明‥‥這種冶金和構築
[PAGE_BREAK]
　的技術，絕對不是現有的文
[PARAGRAPH]
　明或魔法所能做到的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『如果有時間的話，真希望能
[PAGE_BREAK]
　好好的研究一下此地。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『可惜我們沒什麼時間了，我
[PAGE_BREAK]
　怕那傢伙會啟動最終兵器「
[PAGE_BREAK]
　黑暗之焰」，無論如何我們
[PARAGRAPH]
　要趕快找到他才行‥‥』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0014]
『注意！發現並確認敵單位！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0015]
『一級警告！敵單位與ASR一07
[PAGE_BREAK]
　從轉送站進入C一01地區，本
[PAGE_BREAK]
　地區防衛部隊立刻進入防衛
[PARAGRAPH]
　體勢，座標0177一03一42。重
[PAGE_BREAK]
　複一次，敵單位已進入要塞
[PAGE_BREAK]
　，立刻採取防衛體勢。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0014]
『本區防衛隊立刻開始防衛行
[PAGE_BREAK]
　動，交戰並殲滅敵目標。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『有麻煩了，我們趕快解決這
[PAGE_BREAK]
　些防衛隊，在這裡作戰無險
[PAGE_BREAK]
　可守。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這些機甲衛兵還不都是一些
[PAGE_BREAK]
　老貨色，相信很快就可以解
[PAGE_BREAK]
　決掉。我們上吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我們身處敵境，大家要特別
[PAGE_BREAK]
　小心些！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『‥嚴重受損，指揮控制不能
[PAGE_BREAK]
　‥系統關閉‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『C一02地區，第一防衛隊抵達
[PAGE_BREAK]
　。轉送辨識及接戰資料。』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『C一02地區，第二防衛隊抵達
[PAGE_BREAK]
　。轉送辨識及接戰資料。』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『C一02地區，第三防衛隊抵達
[PAGE_BREAK]
　。轉送辨識及接戰資料。』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『轉送完成。目標及命令確認
[PAGE_BREAK]
　，防衛行動開始。』
[END]
```

### Page 7

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『大家都沒事吧？我們趕快離
[PAGE_BREAK]
　開這裡，要不然敵方防衛隊
[PAGE_BREAK]
　會不斷向這裡湧來。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『接下來要往哪裡呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『到黃金城的核心區域，我必
[PAGE_BREAK]
　須奪回中樞系統的控制權，
[PAGE_BREAK]
　這樣我就可以解除要塞中大
[PARAGRAPH]
　部份防衛部隊的戰鬥狀態。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『‥聽不懂。就照妳說的吧！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『嘻，聽不懂是正常的！我來
[PAGE_BREAK]
　帶路，往這邊走！』
[END]
```
