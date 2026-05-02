# 第 16 章 — 冰原之戰

於冰原與劍聖蜜蒂會合，協同對抗來犯敵軍；達成嚴苛條件可說服蜜蒂正式入隊。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 18 (0x12) | 劍聖蜜蒂 | 章末 | 三條件 AND：索爾 HP_max ≥ 320 AND 18 回合內擊敗敵全部 AND 蜜蒂 8 個部下陣亡 ≤ 4 |

## 敵人配置

LV9 黑暗法師等冰原敵軍。蜜蒂以友軍 NPC (char[0x41]) 自走，下方 8 個部下 (chars[0x42..0x49]) 同樣為自走 NPC。具體配置寫在 FDFIELD.DAT entry 46。

## 寶物

待 FDFIELD.DAT entry 46 確認。

## 商店

無章內商店。

## 特殊機制

- **失敗條件**：索爾死亡，或蜜蒂 (char[0x41]) NPC 死亡。
- **蜜蒂招募**：FD2 全 30 章中最複雜的 end-handler 條件式招募，需同時達成三項：
  - 索爾 HP_max ≥ 320
  - 18 回合內擊敗全部敵人 (TURN 1..18)
  - 蜜蒂 8 個部下中陣亡數 ≤ 4
  三條件全達成才走 page 4 招募分支；否則走 page 2 + cutscene 0x31 + page 3 不招募分支。

## 對話

對話文字 5 pages 來自 FDTXT.DAT entry 16。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『唔，好冷！住慣了羅特帝亞
[PAGE_BREAK]
　的溫暖氣候，還真有點不習
[PAGE_BREAK]
　慣。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『不知道我的朋友是否已經抵
[PAGE_BREAK]
　達此地，照理說應該會有點
[PAGE_BREAK]
　徵兆的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『嘿，在這種地方特別容易看
[PAGE_BREAK]
　到遠處‥‥咦，那可不是有
[PAGE_BREAK]
　人在打架嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『啊，那不就是‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『賽可邦德？你來的正好，我
[PAGE_BREAK]
　遇上了一點小麻煩‥‥這些
[PAGE_BREAK]
　是你新交的朋友嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『是的，有一些志同道合的戰
[PAGE_BREAK]
　友，他們一路趕過來幫忙的
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『好極了，那麼這一回我可以
[PAGE_BREAK]
　少費點工夫，叫你的朋友們
[PAGE_BREAK]
　快點，趕快解決這場仗，我
[PARAGRAPH]
　們還有很多事要辦。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『賽可邦德，這位女士就是你
[PAGE_BREAK]
　所說的朋友嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『是的，她就是劍聖密蒂，多
[PAGE_BREAK]
　年來一直在山中隱居，鍛鍊
[PAGE_BREAK]
　更高強的劍技。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『是她啊！我記得多年前她在
[PAGE_BREAK]
　皇宮禁衛團中是我的前輩，
[PAGE_BREAK]
　後來好像因為醉心於劍術，
[PARAGRAPH]
　就辭去騎士團的職務，那時
[PAGE_BREAK]
　大家都很惋惜呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『像她這樣的厲害人物，為何
[PAGE_BREAK]
　會突然在此出現？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『她似乎是受友人所託，前來
[PAGE_BREAK]
　調查一些事情。我和她在路
[PAGE_BREAK]
　上結識，她得知我要回去確
[PARAGRAPH]
　認獸人的侵攻行動，就託我
[PAGE_BREAK]
　回來通知她結果。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『這樣說起來，莫非她和約拿
[PAGE_BREAK]
　也有什麼關連‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『算了吧！這女人一副大姐樣
[PAGE_BREAK]
　剛見面就對人發號施令，看
[PAGE_BREAK]
　了就不順眼！我才不管她是
[PARAGRAPH]
　什麼劍聖呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，那我們就表現給她看
[PAGE_BREAK]
　啊！讓她知道我們羅特帝亞
[PAGE_BREAK]
　的劍士可也不是泛泛之輩！
[PARAGRAPH]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『說的對！今天要大幹一場！
[PAGE_BREAK]
　上啊！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0041]
『啊‥‥我太天真了，過於倚
[PAGE_BREAK]
　賴劍技，沒有考慮到‥單打
[PAGE_BREAK]
　獨鬥還是不行的‥‥』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0012]
『嗯，一場不錯的仗，但是耗
[PAGE_BREAK]
　時太久了‥‥賽可邦德，有
[PAGE_BREAK]
　關獸人的情況我已經明白了
[PARAGRAPH]
　，等一下我馬上要趕到艾斯
[PAGE_BREAK]
　島去，如果你的朋友也想湊
[PAGE_BREAK]
　湊熱鬧的話，就由你帶他們
[PARAGRAPH]
　去吧，我應該告訴過你艾斯
[PAGE_BREAK]
　島的位置。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『是的，我確實記得。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『艾斯島情勢非比尋常，叫他
[PAGE_BREAK]
　們量力而為，不要把小命送
[PAGE_BREAK]
　掉了。我走了。』
[END]
```

### Page 3

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，這女人真是夠囂張了！
[PAGE_BREAK]
　我‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『唉，索爾，我們技不如人，
[PAGE_BREAK]
　你還想怎麼樣？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『我們還是先趕到艾斯島上觀
[PAGE_BREAK]
　察一下情勢，或許還有我們
[PAGE_BREAK]
　可以幫忙的地方‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『也只好這樣了！索爾，別無
[PAGE_BREAK]
　精打采的，我們走吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可惡，我才不肯認輸呢！
[PAGE_BREAK]
　等著瞧！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x0012]
『各位，漂亮的一仗！真是把
[PAGE_BREAK]
　團隊合作的精神發揮到了極
[PAGE_BREAK]
　致，我個人非常佩服。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『閣下的劍技也極為高超，
[PAGE_BREAK]
　真是讓我們開了眼界。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『謝謝。那麼，我想各位會到
[PAGE_BREAK]
　這裡來，相信都和我有著相
[PAGE_BREAK]
　同的目的，現在已查明敵方
[PARAGRAPH]
　部隊正集結在北方的艾斯島
[PAGE_BREAK]
　上，接下來就由我帶領各位
[PAGE_BREAK]
　前去，將敵人一舉殲滅。我
[PARAGRAPH]
　想各位都沒有意見吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『我有意見！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索‥索爾？你怎麼了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『你有什麼意見，就說出來聽
[PAGE_BREAK]
　聽看吧。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『既然是我們戰技高超，我們
[PAGE_BREAK]
　為什麼要由妳來帶領？應該
[PAGE_BREAK]
　是妳加入我們，然後嚮導我
[PARAGRAPH]
　們到艾斯島去才對，不是嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『索爾大哥，密蒂閣下也算是
[PAGE_BREAK]
　劍士中的前輩，你也多少敬
[PAGE_BREAK]
　老尊賢一下嘛‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，我才不服！會打架的才
[PAGE_BREAK]
　算高手，我從小就這麼認為
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『咈咈‥小子，你年紀這麼輕
[PAGE_BREAK]
　就有如此的膽識和武藝，我
[PAGE_BREAK]
　很欣賞‥‥也好，我一個人
[PARAGRAPH]
　過了這麼多年，也好久沒有
[PAGE_BREAK]
　嘗過被人帶領的滋味了，我
[PAGE_BREAK]
　就加入你們吧！劍聖密蒂，
[PARAGRAPH]
　以後請各位多多指教。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『我不敢相信，一向自視極高
[PAGE_BREAK]
　美貌而又驕傲的劍聖密蒂，
[PAGE_BREAK]
　居然就這樣折服在索爾的氣
[PARAGRAPH]
　魄之下了‥‥』
[PARAGRAPH]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『這就是王者的霸氣啊！不愧
[PAGE_BREAK]
　是索爾，光這一點我就比不
[PAGE_BREAK]
　上他了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『那麼，密蒂閣下，這就請妳
[PAGE_BREAK]
　帶領我們到艾斯島去吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0012]
『我既然已經加入你們，稱呼
[PAGE_BREAK]
　我時閣下兩個字就可以不必
[PAGE_BREAK]
　了，對吧，索爾？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，我才‥我才不在乎這些
[PAGE_BREAK]
　事呢！我們快上路吧！』
[END]
```
