# 第 27 章 — 命運的交會點

抵達古代人遺跡轉送站；悠妮被機甲衝擊喚醒記憶。GOOD/BAD ENDING 分歧點：有天空之鑰則大家一同上黃金城；無則悠妮獨自傳送，遊戲到此結束。

## 加入角色

兩條 ending 路徑皆無加入。

## 敵人配置

FDFIELD.DAT[27]：機甲衛兵 + ASR-07 控制單元。

## 寶物

待解：寶物清單需從 FDFIELD.DAT tile_event 解析。

## 商店

連戰最後章節（本章戰前為最後可買賣處，ch28 之後進入黃金城內部不再有商店機會）。

## 特殊機制

- **Init 條件式對話**：若隊伍持有「天空之鑰」(item 100)，init 階段 page 0 之後額外播放 page 3 (希爾法說明傳送平台需要天空之鑰)。
- **Init 3× spell cast cinematic**：`cast_screen_wide_spell_with_fade` 在三個不同位置觸發小範圍光效（劇情中三方人物使用魔法/技能）。
- **GOOD/BAD ENDING 分歧**：end handler 用 `any_char_has_item(100)` 判定：
  - **GOOD**：有天空之鑰 → 大家一同上黃金城 → 進入 ch28+
  - **BAD**：無天空之鑰 → 悠妮獨自傳送離去 (`animate_warp_teleport_char(1, ...)`) → `play_game_ending_cinematic` + 無限迴圈，遊戲結束無法繼續
- **跨章天空之鑰兌換鏈**（ch21 → ch23 → ch27 → ch28+）：
  - ch21 結束時可由 6 件物品集齊兌換得到天空之鑰
  - ch23 持鑰 → 卡里斯加入
  - **ch27 持鑰 → GOOD ENDING (進 ch28+)**；無鑰 → 強制 BAD ENDING
- **勝負條件**：default + chars[1] (悠妮) 死 = 負。
- **石碑階梯 tile-step**：到達石碑下方階梯時，FDFIELD tile-step handler 會 rewrite turn-event hook table，使下回合敵 turn 觸發援軍 spawn 事件 (event 0x3F / 0x41)。

## 對話

對話文字 24 pages 來自 FDTXT.DAT entry 27。Init 引用 page 0/[3 conditional]/4/5/6/7，End 依 ending 分歧引用 GOOD: 8, 9, 10, 0xB, 0xC 或 BAD: 8, 0xD, 0xE, 0xF, 0x10。Page 1/2/11-23 由 FDFIELD turn-event / tile-step handler 引用。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這裡就是遺跡了嗎？果然是
[PAGE_BREAK]
　個很詭異的地方！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『此地不僅詭異，還有大批敵
[PAGE_BREAK]
　人呢！準備迎敵吧！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『嗶！緊集狀況，請求支援！
[PAGE_BREAK]
　』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『‥警告，任務中止‥ASR一07
[PAGE_BREAK]
　出現異常現象，原因無法解
[PAGE_BREAK]
　析‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『中央的平台似乎就是傳送的
[PAGE_BREAK]
　地點，就看「天空之鑰」能
[PAGE_BREAK]
　否發生作用了！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x0024]
『嗶！敵人及目標出現，準備
[PAGE_BREAK]
　進行第6號戰鬥指令。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0025]
『命令確認。目標位置標定，
[PAGE_BREAK]
　系統作用功率全開。2秒後
[PAGE_BREAK]
　動作開始。』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『呀一一一一一一』
[END]
```

### Page 6

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！可惡‥竟敢對她下手
[PAGE_BREAK]
　！我饒不了你們！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0024]
『制御及管理中樞ASR一07，完
[PAGE_BREAK]
　成記憶庫連接。進行控制程
[PAGE_BREAK]
　式更新及損壞資料排除。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『啊！我的頭！‥我的頭好像
[PAGE_BREAK]
　要炸開來了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_CHAR=0x0024]
『故障排除完成，初步檢查對
[PAGE_BREAK]
　應﹕各系統完好無損。ASR一
[PAGE_BREAK]
　07，繼續執行你的任務。完
[PAGE_BREAK]
　畢。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『我‥我‥為什麼‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！悠妮！妳還好吧！‥
[PAGE_BREAK]
　‥咦，妳‥妳為什麼哭了？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，別管我，‥不管怎樣
[PAGE_BREAK]
　，先打倒這些傢伙，別的以
[PAGE_BREAK]
　後再說‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0024]
『ASR一07，你有敵對反應！你
[PAGE_BREAK]
　是我們的中樞之一，不該對
[PAGE_BREAK]
　我們有敵對意志！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0025]
『緊急狀況，ASR一07出現預期
[PAGE_BREAK]
　外現象，快通知最高控制中
[PAGE_BREAK]
　樞‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『敵人就在眼前了，趕快發動
[PAGE_BREAK]
　攻擊啊！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『對啊！我在發什麼呆，大家
[PAGE_BREAK]
　上！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『一切的謎底就在眼前了！』
[END]
```

### Page 8

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『就是這個了，轉送站‥好久
[PAGE_BREAK]
　沒看到這種東西了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮？妳還好吧？』
[END]
```

### Page 9

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，所有過去的事，我都
[PAGE_BREAK]
　想起來了。請原諒我騙了你
[PAGE_BREAK]
　，可是那時我真的不知道‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，不管是為了什麼事，
[PAGE_BREAK]
　我都不會怪妳。我只是想送
[PAGE_BREAK]
　妳回家‥就這樣而已，相信
[PARAGRAPH]
　我吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『好，那麼‥我的家就在這上
[PAGE_BREAK]
　面，你願意‥送我回家嗎？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『當然！這不是我們的約定嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『索爾，你要小心！這女孩變
[PAGE_BREAK]
　得很奇怪，說不定她原本是
[PAGE_BREAK]
　敵方的人‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『約拿先生，希爾法大師，你
[PAGE_BREAK]
　們所謂的敵人就在這轉送站
[PAGE_BREAK]
　之後，被你們稱為「黃金城
[PARAGRAPH]
　」的第一空中要塞上‥‥如
[PAGE_BREAK]
　果你們想打倒他，就跟著我
[PAGE_BREAK]
　來，不然的話，就我和索爾
[PARAGRAPH]
　上去也可以‥‥你們意下如
[PAGE_BREAK]
　何呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥‥悠妮小姐，妳的記憶全
[PAGE_BREAK]
　部甦醒了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『是的。我在受到剛才那個機
[PAGE_BREAK]
　兵的傳導衝擊之後，受損的
[PAGE_BREAK]
　記憶部份強制還原了，它們
[PARAGRAPH]
　認為我會回到它們那一方，
[PAGE_BREAK]
　可惜的是，我這段日子以來
[PAGE_BREAK]
　的記憶並沒有因此消失‥‥
[PARAGRAPH]
　我已經不是以前的我了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『為什麼？‥我知道妳並不是
[PAGE_BREAK]
　這個世界的人，但為何妳會
[PAGE_BREAK]
　為了我們‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥自私一點來說，我並不是
[PAGE_BREAK]
　為了你們，我是為了一個一
[PAGE_BREAK]
　直保護我、照顧我的人‥‥
[PARAGRAPH]
　像我這樣的人是很悲哀的，
[PAGE_BREAK]
　我們擁有人的一切，卻不能
[PAGE_BREAK]
　做任何平凡人所能做的事情
[PARAGRAPH]
　，所以，我很珍惜這次的經
[PAGE_BREAK]
　歷‥‥我不容許那個無視這
[PAGE_BREAK]
　一切的傢伙傷害他和他所愛
[PARAGRAPH]
　的這個世界。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥我明白了。悠妮小姐，我
[PAGE_BREAK]
　很高興妳自始至終都會是我
[PAGE_BREAK]
　們的伙伴‥要對付那個傢伙
[PARAGRAPH]
　，光妳和索爾是不行的，就
[PAGE_BREAK]
　讓我們大家一起來吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『約拿先生，謝謝你‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，你們在說些什麼啊！
[PAGE_BREAK]
　我怎麼聽不太懂‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥索爾，等到上了黃金之城
[PAGE_BREAK]
　，我會讓你知道所有你想知
[PAGE_BREAK]
　道的，在此之前就不要再問
[PARAGRAPH]
　起這些事情了‥答應我好嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『當‥當然！我也只是隨口問
[PAGE_BREAK]
　問而已，就當我沒說好啦！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『好啦！大家可以整裝待發了
[PAGE_BREAK]
　，準備迎向最後的敵人！悠
[PAGE_BREAK]
　妮小姐，請妳啟動轉送裝置
[PARAGRAPH]
　吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『好的，請大家在這平台上站
[PAGE_BREAK]
　好，索爾，請把「天空之鑰
[PAGE_BREAK]
　」給我。』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『是這個吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『謝謝。大家站好了‥』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥01327一一344621073。識別
[PAGE_BREAK]
　碼辨識完成‥第一空中要塞
[PAGE_BREAK]
　，請解除出入區防護障壁，
[PARAGRAPH]
　預定五秒後啟動轉送程序。
[PAGE_BREAK]
　』
[END]
```

### Page 12

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『唔哇啊啊啊！‥‥』
[END]
```

### Page 13

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，所有過去的事，我都
[PAGE_BREAK]
　想起來了。請原諒我騙了你
[PAGE_BREAK]
　，可是那時我真的不知道‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，不管是為了什麼事，
[PAGE_BREAK]
　我都不會怪妳，我只是想送
[PAGE_BREAK]
　妳回家‥相信我吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥索爾，謝謝你‥一路從羅
[PAGE_BREAK]
　特帝亞到這裡，你一直都那
[PAGE_BREAK]
　麼照顧我、保護我‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這是我該做的，你知道‥我
[PAGE_BREAK]
　一直對妳‥對妳‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『我明白。索爾，但現在是我
[PAGE_BREAK]
　們分離的時刻了，如果我現
[PAGE_BREAK]
　在不走，就會太遲了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『為什麼？不是約好要我送你
[PAGE_BREAK]
　回家的嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒有天空之鑰，只有我能直
[PAGE_BREAK]
　接從此地前往黃金城。索爾
[PAGE_BREAK]
　，請忘了我吧，我並不是一
[PARAGRAPH]
　般的人類，既不能也沒有資
[PAGE_BREAK]
　格接受你的感情，這是命運
[PAGE_BREAK]
　的安排吧‥‥』
[END]
```

### Page 14

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『系統權限強制插入，A1型分
[PAGE_BREAK]
　解傳送啟動待命，座標1一1一
[PAGE_BREAK]
　72‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！不要做傻事！』
[END]
```

### Page 15

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，不要過來！為了你，
[PAGE_BREAK]
　也為了這個世界‥‥讓我為
[PAGE_BREAK]
　你做這最後的一件事吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『悠妮小姐！有關這一切事件
[PAGE_BREAK]
　的來龍去脈，我們還需要向
[PAGE_BREAK]
　妳請教‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒有必要了，我會帶走這代
[PAGE_BREAK]
　表災厄的黃金之城，就像很
[PAGE_BREAK]
　久很久以前一般，讓它沈睡
[PARAGRAPH]
　在一個誰也找不到的地方‥
[PAGE_BREAK]
　時間的洪流會把它的痕跡從
[PAGE_BREAK]
　人們的記憶中抹去，我想，
[PARAGRAPH]
　這會是最好的結果吧‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這樣嗎？‥也許妳的決定是
[PAGE_BREAK]
　對的，但沒有其他方法了嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒有，我必須自己去面對最
[PAGE_BREAK]
　後的一戰‥各位，再見了，
[PAGE_BREAK]
　和大家這段日子的相處，讓
[PARAGRAPH]
　我永難忘懷‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！不要！不要走！讓我
[PAGE_BREAK]
　幫妳‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，永別了，我永遠永遠
[PAGE_BREAK]
　也不會忘記你的，希望你以
[PAGE_BREAK]
　後幸福快樂‥‥』
[END]
```

### Page 16

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！悠妮！‥‥』
[END]
```

### Page 17

```text
『看！是‥是黃金城！』
[END]
```

### Page 18

```text
『啊！又‥又消失了！』
[END]
```

### Page 19

```text
『這次‥是永遠的消失了吧！
[PAGE_BREAK]
　』
[END]
```

### Page 20

```text
『是的！就像她所承諾的，到
[PAGE_BREAK]
　一個沒有人找得到的地方去
[PAGE_BREAK]
　了‥』
[END]
```

### Page 21

```text
『黃金城傳說‥看來永遠都會
[PAGE_BREAK]
　是個解不開的謎了‥‥』
[END]
```

### Page 22

```text
『索爾‥‥』
[END]
```

### Page 23

```text
『‥再見了，悠妮！雖然妳的
[PAGE_BREAK]
　離去只留下了更多的謎題，
[PAGE_BREAK]
　但在未來的歲月中，我仍會
[PAGE_BREAK]
　永遠記得妳‥和這段非凡的
[PAGE_BREAK]
　冒險‥‥』
[END]
```
