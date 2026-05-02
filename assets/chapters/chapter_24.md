# 第 24 章 — 在天空的彼方

飛行岩穿越大陸西北部「惡魔之山」一帶天空，遭龍人族空中部隊攔截；戰勝後高度急速下降，飛行岩失控降落火焰之谷。

## 加入角色

無新加入角色。

## 敵人配置

LV23 龍人戰士 ×12 + LV21 龍騎士 ×4 + 多 wave 援軍 — FDFIELD.DAT[24]。
援軍於第 2、4、7、10 回合敵方 turn intro 從地圖四個角落出現。

## 寶物

待解：寶物清單需從 FDFIELD.DAT tile_event 解析。

## 商店

待解：詳細 enemy/item 配置需從 FDFIELD.DAT entry 解析。

## 特殊機制

- **Init 4-stage camera scan**：開場後鏡頭依序掃過 (0,4)/(0,0x16)/(0x1A,0x18)/(0x1A,2) 四個地圖角落，每停 400ms。這四個座標即為 FDFIELD 援軍的四個 spawn 點，預示玩家將會在這些位置遇到援軍。
- **End text-scroll cinematic**：FD2 唯一在結尾使用文字向上捲動 + palette fade-out 動畫的章節（30 章中只有此章用 `scroll_text_screen_up_by_lines`）。隱喻飛行岩高度急速下降的視覺效果。
- **勝負條件**：default — 全敵死 = 勝；索爾死 = 負。

## 對話

對話文字 4 pages 來自 FDTXT.DAT entry 24。Init 引用 page 0/1，End 引用 page 2/3。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『啊，無聊得要命‥到底還要
[PAGE_BREAK]
　飛多久啊！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『真神奇，我們一天內就飛過
[PAGE_BREAK]
　了大陸西北部‥從方向看來
[PAGE_BREAK]
　，現在應該是在前往邊界的
[PARAGRAPH]
　山脈吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『你是說「惡魔之山」嗎？那
[PAGE_BREAK]
　一帶全是未熄滅的火山啊！
[PAGE_BREAK]
　聽說那裡也是惡魔族的根據
[PARAGRAPH]
　地‥難道惡魔族也和這件事
[PAGE_BREAK]
　有關？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『那倒不奇怪，惡魔族本來就
[PAGE_BREAK]
　棲息在黑暗和地獄般的炎熱
[PAGE_BREAK]
　中，和其他種族向不往來，
[PARAGRAPH]
　誰曉得它們暗地裡會搞出些
[PAGE_BREAK]
　什麼！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『瞧，有人來迎接我們了！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『那‥那是什麼？飛行的大岩
[PAGE_BREAK]
　石，上面有人？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『笨！那是魔法驅動的飛行裝
[PAGE_BREAK]
　置，那些人是乘坐在上面的
[PAGE_BREAK]
　乘客！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0012]
『他們想幹什麼？想突破這裡
[PAGE_BREAK]
　的空域前往火焰之谷嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0013]
『陛下吩咐過我們，絕不可讓
[PAGE_BREAK]
　任何人從空中或地下經過此
[PAGE_BREAK]
　地的！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『所以現在我們要宰了那些傢
[PAGE_BREAK]
　伙！上吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『哇！對方來意不善呢！好像
[PAGE_BREAK]
　想要動手了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『要在這種地方打嗎？好像對
[PAGE_BREAK]
　我們很不利吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0014]
『我們的伙伴中還有幾個會飛
[PAGE_BREAK]
　的，勉強可以牽制敵方的攻
[PAGE_BREAK]
　勢，你們只要在地上確實消
[PARAGRAPH]
　滅敵人就可以了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『對方不過是會飛而已，有什
[PAGE_BREAK]
　麼好怕的！我們先打了再說
[PAGE_BREAK]
　！』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『敵軍似乎是負責鎮守這一帶
[PAGE_BREAK]
　的空域，以防止他人侵入‥
[PAGE_BREAK]
　看來在惡魔之山一定有什麼
[PAGE_BREAK]
　事發生，大家要有心理準備
[PAGE_BREAK]
　，準備面對非常的狀況‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『約拿老頭，我們的高度好像
[PAGE_BREAK]
　開始下降了耶！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0014]
『的確，而且越來越快了‥‥
[PAGE_BREAK]
　』
[END]
```

### Page 3

```text
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哇啊啊啊！索爾，我好怕！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『希莉亞，抓緊我！悠妮，妳
[PAGE_BREAK]
　也過來！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥好！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『‥‥‥！啊，亞雷斯，
[PAGE_BREAK]
　我頭好昏，‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『不要緊吧？來，抓緊我！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『謝謝‥，亞雷斯，你真好‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『約拿老頭啊，我就說嘛，現
[PAGE_BREAK]
　在的女孩子要比以前大膽多
[PAGE_BREAK]
　了，連現在這種狀況都可以
[PARAGRAPH]
　拿來作調情的機會，看來我
[PAGE_BREAK]
　們都落伍嘍！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『希爾法，如果你不想被她們
[PAGE_BREAK]
　一腳踹下這飛行岩的話，我
[PAGE_BREAK]
　建議你在我們著陸前最好保
[PARAGRAPH]
　持安靜‥‥』
[END]
```
