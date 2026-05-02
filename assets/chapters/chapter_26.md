# 第 26 章 — 未知的迴廊

通過古代人禁地通道，遭遇大批機甲兵；途中悠妮喚醒一具廢棄機甲兵渥德為己方作戰，並在通道盡頭從 5 個寶箱中選 1 件最強武器。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| (機甲兵) | 渥德 | 通道內 tile event | 悠妮輸入啟動碼 (FDFIELD event)，見 entry 26 dialog page 4 |

## 敵人配置

LV28 龍騎士 ×8/×3/×3/×3 + LV29 龍人戰士 ×8 + LV24 龍人法師 ×5 — FDFIELD.DAT[26]。
援軍 9 階段密集 spawn：第 2、4、6、8、10、12、15、16、17 回合 enemy turn intro。

## 寶物

**畫面最上方 5 個寶箱**：均為最強武器，但只能選 1 個。`tile_event_consumed_flags[0xC]` 紀錄玩家拿了哪一個 (0-4)，影響 end handler 的 dialog page。

其他寶物待解：寶物清單需從 FDFIELD.DAT tile_event 解析。

## 商店

待解：詳細 enemy/item 配置需從 FDFIELD.DAT entry 解析。

## 特殊機制

- **寶箱五選一動態 dialog**：end handler 用 `tile_event_consumed_flags[0xC]` (值 0-4) 動態決定兩段 dialog page (5..9 與 8..12，分別對應 5 個寶箱選擇的劇情解說)。
- **悠妮喚醒機甲兵渥德**：通道內 tile event，悠妮輸入啟動碼 `01E0C244-FE2C5-1932`，渥德 (`01279943渥德`) 加入隊伍替己方作戰。
- **勝負條件**：default + 悠妮 (chars[1]) 或 亞奇梅吉 (chars[2]) 死 = 負。
- **援軍密集 turn**：第 2、4、6、8、10、12、15、16、17 回合敵 turn intro 觸發 reinforcement。

## 對話

對話文字 12 pages 來自 FDTXT.DAT entry 26。Init 引用 page 0；End handler 依 `tile_event_consumed_flags[0xC]` 動態引用 5..9 與 8..12，及固定 page 7、10、11。Page 1-4 由 FDFIELD tile-step / dialog event 引用。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『嘿，這是什麼怪地方？‥這
[PAGE_BREAK]
　種景色以前從沒看過！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001D]
『這條通道據說是古代人所建
[PAGE_BREAK]
　造的，歷代的惡魔族之王都
[PAGE_BREAK]
　禁止族人進入此地，似乎踏
[PARAGRAPH]
　進這通道就會招來可怕的災
[PAGE_BREAK]
　禍‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001A]
『我感到災禍好像已經在眼前
[PAGE_BREAK]
　了，那些看起來像玩偶的東
[PAGE_BREAK]
　西是什麼？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0074]
『入侵者發現！指令﹕防衛及
[PAGE_BREAK]
　消除威脅的存在，方法﹕接
[PAGE_BREAK]
　戰並消滅。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x006F]
『命令確認！目標分析完畢！
[PAGE_BREAK]
　啟動！執行接戰指令！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001A]
『那是什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『簡單的說，那是古代人建造
[PAGE_BREAK]
　的機器守衛，用來防守他們
[PAGE_BREAK]
　的遺跡和據點的‥沒時間解
[PARAGRAPH]
　釋了，我們準備動手吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『小心！它們要衝過來了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『敵人的數量比上次更多了，
[PAGE_BREAK]
　大家小心應戰！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『‥嚴重受損，指揮控制不能
[PAGE_BREAK]
　‥系統關閉‥‥』
[END]
```

### Page 2

```text
『那是什麼奇怪的東西？
[PAGE_BREAK]
　頭部還開著？』
[END]
```

### Page 3

```text
『這機兵的頭部怎麼開著？這
[PAGE_BREAK]
　個金屬盒子‥好像滿適合的
[PAGE_BREAK]
　，應該是這樣放進去‥‥‥
[PAGE_BREAK]
　咦！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x001F]
『系統啟始，請指示操作碼及
[PAGE_BREAK]
　行動模式。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『這‥好像是01E0C244一FE2C5
[PAGE_BREAK]
　1932。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『確認。主人，01279943渥德
[PAGE_BREAK]
　，回復到正常操作狀態，請
[PAGE_BREAK]
　指示。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『我為什麼記得這些東西？‥
[PAGE_BREAK]
　好吧，幫我們打倒那些機兵
[PAGE_BREAK]
　！能理解嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『理解，指令內容確認，系統
[PAGE_BREAK]
　資料重設‥待命狀態完成。
[PAGE_BREAK]
　完畢。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『目標清除後即解除一級戰鬥
[PAGE_BREAK]
　狀態，並確認所有我方目標
[PAGE_BREAK]
　。等待下一次戰鬥指示碼。
[PARAGRAPH]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『了解。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，妳在那邊做什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒‥沒什麼，我說服了這個
[PAGE_BREAK]
　機甲兵幫我們作戰。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『哇塞，悠妮妳真厲害！』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x001D]
『好極了，我們迅速擊潰了敵
[PAGE_BREAK]
　方機甲兵，現在可以馬上前
[PAGE_BREAK]
　往遺跡，不過我們還是對敵
[PARAGRAPH]
　人的底細一無所知‥‥悠妮
[PAGE_BREAK]
　小姐，妳的護衛也是古代人
[PAGE_BREAK]
　所造的機甲兵吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『是的，她初次和我們相遇時
[PAGE_BREAK]
　就帶著蓋亞。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001D]
『是這樣嗎？‥悠妮小姐，我
[PAGE_BREAK]
　看妳一定知道些什麼，有關
[PAGE_BREAK]
　古代人和這批機甲兵的秘密
[PARAGRAPH]
　‥‥是吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『古代人？機甲兵？‥‥那是
[PAGE_BREAK]
　什麼東西？我不知道！』
[END]
```

### Page 6

```text
[PORTRAIT_RIGHT_BY_ID=0x001D]
『好極了，我們迅速擊潰了敵
[PAGE_BREAK]
　方機甲兵，現在可以馬上前
[PAGE_BREAK]
　往遺跡‥嘿，隊伍��怎麼有
[PAGE_BREAK]
　個敵方機甲兵？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那是悠妮幫我們拉進伙的，
[PAGE_BREAK]
　她和機甲兵一直很有緣。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『是啊，她初次和我們相遇
[PAGE_BREAK]
　時就帶著蓋亞。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001D]
『是這樣嗎？‥悠妮小姐，我
[PAGE_BREAK]
　看妳好像知道機甲兵的啟動
[PAGE_BREAK]
　碼，是吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『啊？啟動碼‥那是什麼東西
[PAGE_BREAK]
　？我不知道！』
[END]
```

### Page 7

```text
[PORTRAIT_RIGHT_BY_ID=0x001D]
『是嗎？悠妮小姐，我看妳一
[PAGE_BREAK]
　定知道！妳到底是從哪裡來
[PAGE_BREAK]
　的？妳絕不是一般人類！為
[PARAGRAPH]
　了我們大家，把話說個清楚
[PAGE_BREAK]
　吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥不‥不要再說了！‥我什
[PAGE_BREAK]
　麼都不知道‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『亞奇梅吉，夠了！你要是把
[PAGE_BREAK]
　她弄哭了，我絕不饒你！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥算啦，不要逼問她了，反
[PAGE_BREAK]
　正上了黃金城，相信一切的
[PAGE_BREAK]
　謎題自然都會解開的！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好，那就一鼓作氣衝過去，
[PAGE_BREAK]
　我們這就走吧！悠妮，跟著
[PAGE_BREAK]
　我！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『‥‥‥』
[END]
```

### Page 9

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『亞奇梅吉，夠了！你要是把
[PAGE_BREAK]
　她弄哭了，我絕不饒你！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥算啦，不要逼問她了，問
[PAGE_BREAK]
　這個機甲兵也是一樣‥‥喂
[PAGE_BREAK]
　，那個機甲兵。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『01279943渥德，命令確認。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『你叫渥德嗎？請回答我的問
[PAGE_BREAK]
　題。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『了解，記憶庫連接，資料載
[PAGE_BREAK]
　入待命。請指示。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『你們的種族來自何方？領導
[PAGE_BREAK]
　者又是誰？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『記憶庫搜尋，資料#73324﹕
[PAGE_BREAK]
　01279943渥德隸屬第六機甲
[PAGE_BREAK]
　戰鬥團第三戰鬥中隊，全師
[PARAGRAPH]
　團由第一空中要塞建造，在
[PAGE_BREAK]
　執行第七號戰鬥防衛命令時
[PAGE_BREAK]
　，經由轉送站送到地表。我
[PARAGRAPH]
　們的領導者是全能的創造者
[PAGE_BREAK]
　，他可以任意創造機械和生
[PAGE_BREAK]
　命，是這個大地的主宰者，
[PARAGRAPH]
　我們並不知道領導者的名字
[PAGE_BREAK]
　，那不在我們的記憶庫領域
[PAGE_BREAK]
　之內。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥從未聽過這種事‥好吧，
[PAGE_BREAK]
　那你們為何要攻擊這個大陸
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『資料庫搜尋，資料#2074213
[PAGE_BREAK]
　﹕第六機甲戰鬥團奉命執行
[PAGE_BREAK]
　第七號戰鬥防衛命令，主要
[PARAGRAPH]
　目標﹕絕滅X一07地區的敵方
[PAGE_BREAK]
　生命體，防止任何敵方單位
[PAGE_BREAK]
　進入轉送站；但就最後的記
[PARAGRAPH]
　錄，本次作戰已告中止，而
[PAGE_BREAK]
　01279943渥德在遭受敵方攻
[PAGE_BREAK]
　擊之後，誤動安全裝置導致
[PARAGRAPH]
　能源單位彈出，系統中斷至
[PAGE_BREAK]
　今，並未再接到過任何更新
[PAGE_BREAK]
　的作戰命令。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『這傢伙的話還真是難懂‥約
[PAGE_BREAK]
　拿老頭，你聽懂了多少？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我只知道這位渥德老兄好像
[PAGE_BREAK]
　並未參與這次的攻擊，不過
[PAGE_BREAK]
　一切麻煩的根源似乎都來自
[PARAGRAPH]
　所謂的「第一空中要塞」，
[PAGE_BREAK]
　它也可能就是「黃金的城堡
[PAGE_BREAK]
　」，由古代人所建造的‥飛
[PARAGRAPH]
　行堡壘，兼具建造機甲兵的
[PAGE_BREAK]
　能力‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001A]
『古代人不是早在數萬年前的
[PAGE_BREAK]
　最終戰爭中就死光了嗎？為
[PAGE_BREAK]
　什麼這個要塞還能發動攻擊
[PARAGRAPH]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這個「黃金城」會不會是由
[PAGE_BREAK]
　類似的機甲兵所控制，由於
[PAGE_BREAK]
　沒有收到所謂的中止作戰指
[PARAGRAPH]
　令，所以仍在執行以前的作
[PAGE_BREAK]
　戰行動？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『很有可能，古代人建造這種
[PAGE_BREAK]
　飛行要塞，主要目的應該就
[PAGE_BREAK]
　是空中制壓和對地攻擊，但
[PARAGRAPH]
　最終戰爭早在數萬年前就已
[PAGE_BREAK]
　經結束了，為何到現在才又
[PAGE_BREAK]
　開始行動？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『看來只有上黃金城一趟，這
[PAGE_BREAK]
　一切的謎題才能夠解開！渥
[PAGE_BREAK]
　德，你所說的轉送站在哪裡
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001F]
『就位在這地下通道的出口處
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好，那就一鼓作氣衝過去，
[PAGE_BREAK]
　我們這就走吧！悠妮，跟著
[PAGE_BREAK]
　我！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥‥‥』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，怎麼了？是不是‥我
[PAGE_BREAK]
　的口氣太粗魯了？對不起！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『不是的，索爾，我好迷惑‥
[PAGE_BREAK]
　‥我到底是誰？這一切‥到
[PAGE_BREAK]
　底和我有什麼關連？我真的
[PARAGRAPH]
　不知道‥我好怕我會給你帶
[PAGE_BREAK]
　來災禍，也許一開始你就不
[PAGE_BREAK]
　該救我的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，我永遠都會相信妳的
[PAGE_BREAK]
　！只要有妳和我在一起，我
[PAGE_BREAK]
　什麼也不怕！我們不是都來
[PARAGRAPH]
　到這裡了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『可是‥可是‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『剛才的事就當它沒發生過，
[PAGE_BREAK]
　來，我們一起走過去！』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『其實亞奇梅吉說的也沒錯‥
[PAGE_BREAK]
　這樣子好嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這個年輕人得到神的庇佑，
[PAGE_BREAK]
　我想我們應該尊重他的決定
[PAGE_BREAK]
　‥不多說了，我們走吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001D]
『也只能這樣了！』
[END]
```
