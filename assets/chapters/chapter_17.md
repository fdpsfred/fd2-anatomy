# 第 17 章 — 血與冰之刃

冰島決戰冰魔神，並結識龍人族劍士凱拉斯加入隊伍；本章劇情依蜜蒂是否已入隊而分歧。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 16 (0x10) | 龍劍士凱拉斯 | 章末 | 無條件 |

註：若上一章未招募蜜蒂，本章蜜蒂仍會以友軍 NPC (char[0x34]) 出戰，但戰鬥結束後告別離開。

## 敵人配置

LV9 黑暗法師×7 (HP351, MP180, AP228, DP195, 炎龍術) 等。配置寫在 FDFIELD.DAT entry 49。

## 寶物

待 FDFIELD.DAT entry 49 確認。

## 商店

無章內商店。

## 特殊機制

- **蜜蒂條件式出戰**：init handler 檢查隊伍是否已含蜜蒂 (char_id 0x12)，若無則載入她的肖像，使她以 NPC (char[0x34]) 形式出戰。
- **失敗條件**：索爾死亡。若蜜蒂未在上一章招募成功，則 NPC 蜜蒂 (char[0x34]) 死亡也算敗。
- **章末告別**：若蜜蒂未入隊，end handler 走 page 7 + cutscene 0x32/0x33 (告別) 路徑；若蜜蒂已入隊，直接走 page 5 (無告別)。兩路徑合流後共播 page 6 + cutscene 0x35 + page 8，並讓凱拉斯加入。

## 對話

對話文字 9 pages 來自 FDTXT.DAT entry 17。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0012]
『我所料果然不差，瞧，一大
[PAGE_BREAK]
　堆敵人集結在這小小的冰島
[PAGE_BREAK]
　上，一定有什麼圖謀‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『讓我看看‥哎呀！有獸人
[PAGE_BREAK]
　黑暗戰士和沒見過的傢伙呢
[PAGE_BREAK]
　聲勢似乎頗為浩大的樣子！
[PARAGRAPH]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『密蒂前輩，我們‥我們現在
[PAGE_BREAK]
　該怎麼辦呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那還用說！把他們全宰了，
[PAGE_BREAK]
　事情就可以告一段落了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『這樣嗎？我也同意索爾的看
[PAGE_BREAK]
　法‥總勝於站在這裡發呆吧
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『是啊！我們千里迢迢的趕來
[PAGE_BREAK]
　這裡，不就是要把事情弄清
[PAGE_BREAK]
　楚嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『小心！敵人已經注意到我們
[PAGE_BREAK]
　了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『劍聖密蒂嗎？妳總算來了，
[PAGE_BREAK]
　我知道這幾天來妳一直在調
[PAGE_BREAK]
　查此地的事‥唔，看來妳還
[PARAGRAPH]
　帶了不少陪葬的傢伙，這一
[PAGE_BREAK]
　來這冰島上可有得熱鬧了‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『密蒂前輩，這奇怪的傢伙就
[PAGE_BREAK]
　是事件的元兇首惡嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『我想應該是。別小看他這奇
[PAGE_BREAK]
　形怪狀的模樣，我感覺得到
[PAGE_BREAK]
　他身上有強大的魔力‥‥總
[PARAGRAPH]
　之，這是個棘手的強敵，索
[PAGE_BREAK]
　爾你要特別小心！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，我才不怕這傢伙呢！
[PAGE_BREAK]
　大家上啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『真沒格調的傢伙，本來還想
[PAGE_BREAK]
　和你們聊聊的，只好早點送
[PAGE_BREAK]
　你們下地獄了‥給我殺個片
[PARAGRAPH]
　甲不留！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0044]
『密蒂閣下，我們沒有來遲吧
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『各位來的正好！敵人兵力強
[PAGE_BREAK]
　大，請各位盡力作戰，我們
[PAGE_BREAK]
　務必取得勝利！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0044]
『是！我們一定不會讓閣下您
[PAGE_BREAK]
　失望的！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0012]
『啊‥黑暗的力量果真強大，
[PAGE_BREAK]
　我的修練還是不夠‥剩下的
[PAGE_BREAK]
　只好拜託你們了‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『為什麼‥為什麼這些下賤的
[PAGE_BREAK]
　人類居然能打倒我？‥‥
[PAGE_BREAK]
　主人，救救我‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_RIGHT_BY_ID=0x0011]
『咦！這可不是‥「冰之眼」
[PAGE_BREAK]
　寶石？原來是在冰魔神的
[PAGE_BREAK]
　身上‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『你也知道「魔眼寶石」的事
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『嗯，自從我知道某個古老的
[PAGE_BREAK]
　傳說以來，就一直在追尋
[PAGE_BREAK]
　其中的秘密。但為什麼這
[PARAGRAPH]
　些傢伙會擁有其中之一？
[PAGE_BREAK]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『我們也正在調查這一點，目
[PAGE_BREAK]
　前唯一可以肯定的是，這
[PAGE_BREAK]
　絕對不會是什麼好事‥嗯
[PARAGRAPH]
　這件事還是等打完仗再來
[PAGE_BREAK]
　談好了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『也好，反正這場仗已是勝利
[PAGE_BREAK]
　在望了！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x0012]
『原來我們打倒的傢伙叫做冰
[PAGE_BREAK]
　魔神，看來和你們上次救出
[PAGE_BREAK]
　國王時遇到的地魔神是一夥
[PARAGRAPH]
　的。他們真正的目的究竟為
[PAGE_BREAK]
　何，現在仍然是一個謎，看
[PAGE_BREAK]
　來我們還有好長一段路要走
[PARAGRAPH]
　‥‥各位可願意和我繼續一
[PAGE_BREAK]
　起追索這批邪惡之徒？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可是‥我們原先是要找到賢
[PAGE_BREAK]
　者約拿，請他醫治悠妮小姐
[PAGE_BREAK]
　的失憶症的‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『約拿？他就是這次調查行動
[PAGE_BREAK]
　的發起人啊！他以各種方法
[PAGE_BREAK]
　召集了馬拉大陸各地的勇士
[PARAGRAPH]
　能人，一起向這個未知的陰
[PAGE_BREAK]
　謀和其計畫者挑戰，難道你
[PAGE_BREAK]
　們不知道此事？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『約拿確是有請我前往北方的
[PAGE_BREAK]
　黑森林與他會合，但他並沒
[PAGE_BREAK]
　有說明原因‥‥現在我總算
[PARAGRAPH]
　明白了！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0010]
『小伙子們！是你們一行打敗
[PAGE_BREAK]
　冰魔神的嗎？』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_ID=0x0012]
『原來我們打倒的傢伙叫做冰
[PAGE_BREAK]
　魔神，看來和你們上次救出
[PAGE_BREAK]
　國王時遇到的地魔神是一夥
[PARAGRAPH]
　的。他們真正的目的究竟為
[PAGE_BREAK]
　何，現在仍然是一個謎‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『密蒂閣下，那您往後有什麼
[PAGE_BREAK]
　打算？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『繼續追查敵人的底細。此行
[PAGE_BREAK]
　有多麼危險，你們現在應該
[PAGE_BREAK]
　很清楚了，如果愛惜性命的
[PARAGRAPH]
　話，就到此為止比較好。
[PAGE_BREAK]
　賽可邦德，他們就交給你了
[PAGE_BREAK]
　時間緊迫，我得先走一步
[PARAGRAPH]
　‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這還需要懷疑嗎？對了，你
[PAGE_BREAK]
　是從哪裡跑出來的？和冰魔
[PAGE_BREAK]
　神是一夥的嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0010]
『別誤會，我是龍人族的劍士
[PAGE_BREAK]
　凱拉斯，奉命前來冰島調查
[PAGE_BREAK]
　時不小心中了這些雜碎的奸
[PARAGRAPH]
　計，一直被關在地下的冰穴
[PAGE_BREAK]
　中；不久前我才設法掙脫繩
[PAGE_BREAK]
　結跑出來。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『冰魔神和其部下確實已被我
[PAGE_BREAK]
　們打倒了，我想你可以放心
[PAGE_BREAK]
　了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0010]
『這樣嗎？能夠打倒冰魔神，
[PAGE_BREAK]
　真是一件了不起的成就‥‥
[PAGE_BREAK]
　不過你們為何要與他們作對
[PARAGRAPH]
　呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這哪需要理由！那些傢伙在
[PAGE_BREAK]
　大陸各地胡作非為，我們的
[PAGE_BREAK]
　目的雖然是尋找賢者約拿，
[PARAGRAPH]
　但是看到他們的種種惡行，
[PAGE_BREAK]
　怎麼可以置之不理？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『我們一路上與敵人交戰，不
[PAGE_BREAK]
　但因此交了不少朋友，戰技
[PAGE_BREAK]
　也大為提昇，而約拿恰巧也
[PARAGRAPH]
　在調查此事，我們的方向正
[PAGE_BREAK]
　好與他不謀而合。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0010]
『原來如此‥我族之王聖寇拉
[PAGE_BREAK]
　斯也受到約拿之邀出面調查
[PAGE_BREAK]
　此事，因此他與我分頭前往
[PARAGRAPH]
　冰島和西方的沙漠，想查明
[PAGE_BREAK]
　敵人的底細，結果我沒料到
[PAGE_BREAK]
　敵人大軍聚集於此，一下子
[PARAGRAPH]
　就被抓了‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『聖寇拉斯陛下竟然也親自出
[PAGE_BREAK]
　馬了，看來事態一定非同小
[PAGE_BREAK]
　可。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0010]
『是啊！只是我有負陛下使命
[PAGE_BREAK]
　被關了這麼多天，約定的日
[PAGE_BREAK]
　期早過了，也不知道接下來
[PARAGRAPH]
　該怎麼辦‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『那你乾脆加入我們一行好了
[PAGE_BREAK]
　反正陛下遲早也會和賢者約
[PAGE_BREAK]
　拿會合，和我們一起去找約
[PARAGRAPH]
　拿，不是也一樣可以找到陛
[PAGE_BREAK]
　下嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0010]
『你說的有理，不過不知道這
[PAGE_BREAK]
　些朋友們‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『龍人戰士是尊貴的武士，既
[PAGE_BREAK]
　然目的一致，我們當然歡迎
[PAGE_BREAK]
　你加入！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『是啊！被抓並不可恥，這筆
[PAGE_BREAK]
　帳以後再和敵人清算就好了
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0010]
『多謝各位！劍士凱拉斯，以
[PAGE_BREAK]
　後請多多指教。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好極了，又多了一個新伙伴
[PAGE_BREAK]
　不過，我可不想繼續待在這
[PAGE_BREAK]
　個光禿禿的冰島上，索菲亞
[PARAGRAPH]
　我們下一步該怎麼走？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『應該是再往北經過山道，前
[PAGE_BREAK]
　往史威特平原‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『那是條滿偏僻的山道，知道
[PAGE_BREAK]
　的人不多，想必往後的旅程
[PAGE_BREAK]
　應該會比較平靜了。
[PARAGRAPH]
　哈哈哈！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『但願如此。我們這就上路吧
[PAGE_BREAK]
　！』
[END]
```
