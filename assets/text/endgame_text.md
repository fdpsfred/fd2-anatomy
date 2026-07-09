# 結局與序章文字 (FDTXT.DAT entries 31–33)

FDTXT.DAT 的 entry 由 `fd2_load_chapter_battle_data(chapter_id)` 載入，取的是
`FDTXT.DAT[chapter_id+1]`（src/rsrc/rsrc.c）。本檔收錄三個高編號 entry；其中只有
entry 31 是結局內容，entry 32/33 其實是第 1 章的序章對白。

- **entry 31 — 結局 epilogue**（46 pages）。第 30 章通關後的結局過場
  （`fd2_play_final_chapter_30_ending`，src/anim/aniend.c）以
  `fd2_load_chapter_battle_data(0x1E)` 載入（引數 30 → 載入 30+1 = entry 31）；同一過場
  逐一角色動態索引此 entry：各角色的結局文字頁 = `char_id + 0xC`（範圍 0xC..0x2B），最後一名
  角色改用 page 0x2D（aniend.c:928/930）。page 10（0xA）「姓名﹕」與 page 11（0xB）「職業﹕」
  是角色卡的固定欄位標籤（分別配 all_game_text 的 `char_id+1` 姓名、`job_id+0x96` 職業）；
  page 0x2C 是開場/收尾的旁白框（aniend.c:790 一次性繪出）。
- **entry 32 — 第 1 章序章對話（第二幕）**（11 pages），並非結局內容。由
  `fd2_chapter_01_init`（src/field/chinit.c）在 prologue 地圖 chapter_id=0x1F 的
  Phase C 播出 page 0..9。
- **entry 33 — 第 1 章序章對話（第一幕）**（6 pages），並非結局內容。由
  `fd2_chapter_01_init` 在 prologue 地圖 chapter_id=0x20 的 Phase A/B 播出
  page 0..5。

三者實際播放順序為 entry 33 → entry 32 → entry 1（第 1 章主戰鬥文字）；序章劇情
逐頁對照見 `chapters/chapter_01.md`。

## Notation

- `[OPCODE]` / `[OPCODE=0xNNNN]` — dialog VM control code (詳見 `resource_info/fdtxt.md`)
- `{ascii char}` — 直接渲染的英數字模（atlas glyph_id 0x00–0x23：0x00–0x09 為 '0'–'9'、0x0A–0x23 為 'A'–'Z'；atlas 非 ASCII 對齊，無小寫與標點，'A' 是 glyph 0x0A 非 0x41）
- 中文字 — glyph_id 已從 `assets/text/glyph_table.md` 替換為實際字符
- `〈NNNN〉` — 該 glyph_id 無 lookup entry 時的 fallback

## Entry 31 — epilogue dialogue (chapter_id 30)

range [0x1a7ea, 0x1c24e), size 6756 bytes, 46 pages.

## Pages

### Page 0  (offset 0x5c, span 1064 bytes)

```text
[PORTRAIT_RIGHT_BY_ID=0x0003]
『這是馬拉大陸的土地！我們
[PAGE_BREAK]
　終於又回到家園了。哈，多
[PAGE_BREAK]
　踩幾下這堅實的土地！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0005]
『啊～涼爽的微風，和喣的陽
[PAGE_BREAK]
　光！還是這個世界比較美好
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『很高興我們解決了一切的事
[PAGE_BREAK]
　端，並且保住了這個世界。
[PAGE_BREAK]
　大家都做得很好！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『我會叫父王辦個盛大的慶功
[PAGE_BREAK]
　宴，我們大家好好慶祝慶祝
[PAGE_BREAK]
　！這可是連王國禁衛軍都做
[PARAGRAPH]
　不到的偉大成就哦！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『亞雷斯，到時候我們痛快的
[PAGE_BREAK]
　喝一杯！你會陪我的吧？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0004]
『這個‥我負責在旅途中侍衛
[PAGE_BREAK]
　索爾王子，如今事情已經圓
[PAGE_BREAK]
　滿解決，為了避免陛下和皇
[PARAGRAPH]
　后擔憂，我想還是早點回羅
[PAGE_BREAK]
　特帝亞比較好‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『這樣嗎‥可是，可是‥‥‥
[PAGE_BREAK]
　留下來玩個幾天不行嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『洛娜，既然你捨不得亞雷斯
[PAGE_BREAK]
　可以和他一起回羅特帝亞啊
[PAGE_BREAK]
　過幾天我也要以正式身份前
[PARAGRAPH]
　往羅特帝亞拜訪，那時候妳
[PAGE_BREAK]
　再和我一起去不就好了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『嗯，這樣的話，那‥‥‥‥
[PAGE_BREAK]
　亞雷斯，那我們到時候見了
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0004]
『嘿，好‥好啊！唉！這下頭
[PAGE_BREAK]
　大了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『看到這些年輕人的樣子，真
[PAGE_BREAK]
　讓我想起自己年輕的時候。
[PAGE_BREAK]
　呵呵呵‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『是啊！打贏了這麼一場仗，
[PAGE_BREAK]
　輕鬆輕鬆也是應該的‥咦？
[PAGE_BREAK]
　索爾呢？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『怪了，剛剛還在這裡的‥‥
[PAGE_BREAK]
　啊，在那邊！』
[END]
```

### Page 1  (offset 0x484, span 102 bytes)

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，怎麼了？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮‥悠妮在那裡，在天空
[PAGE_BREAK]
　之中‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『有嗎？我什麼都沒看到‥』
[END]
```

### Page 2  (offset 0x4ea, span 24 bytes)

```text
『那是‥那是黃金城！』
[END]
```

### Page 3  (offset 0x502, span 68 bytes)

```text
『這就是幾百年來，從未有人
[PAGE_BREAK]
　見到過其廬山真面目的黃金
[PAGE_BREAK]
　城嗎？』
[END]
```

### Page 4  (offset 0x546, span 72 bytes)

```text
『‥‥有著神與天空的威嚴，
[PAGE_BREAK]
　卻只能帶來死亡和毀滅的傳
[PAGE_BREAK]
　說之城‥‥』
[END]
```

### Page 5  (offset 0x58e, span 50 bytes)

```text
『‥‥索爾‥‥永遠再見了，
[PAGE_BREAK]
　你要好好保重‥‥』
[END]
```

### Page 6  (offset 0x5c0, span 28 bytes)

```text
『是悠妮的聲音！悠妮‥‥』
[END]
```

### Page 7  (offset 0x5dc, span 36 bytes)

```text
『那是‥‥悠妮最後的道別吧
[PAGE_BREAK]
　？』
[END]
```

### Page 8  (offset 0x600, span 96 bytes)

```text
『‥‥謝謝妳，悠妮！在往後
[PAGE_BREAK]
　的歲月中，我仍會永遠記得
[PAGE_BREAK]
　妳‥‥和這段非凡的冒險旅
[PARAGRAPH]
　程‥‥』
[END]
```

### Page 9  (offset 0x660, span 50 bytes)

```text
『是的，一切就到此結束了。
[PAGE_BREAK]
　黃金城的傳說‥‥』
[END]
```

### Page 10  (offset 0x692, span 8 bytes)

```text
姓名﹕
[END]
```

### Page 11  (offset 0x69a, span 8 bytes)

```text
職業﹕
[END]
```

### Page 12  (offset 0x6a2, span 168 bytes)

```text
在回程的路上，索爾默默的下了決心。為了
[PAGE_BREAK]
悠妮的遺願，為了希莉亞的衷情，他要繼任
[PAGE_BREAK]
羅特帝亞的王位，成為一個偉大的帝王。這
[PAGE_BREAK]
是屬於他的宿命，不管自己的心中是多麼空
[PAGE_BREAK]
虛‥‥
[END]
```

### Page 13  (offset 0x74a, span 144 bytes)

```text
和索爾一夥人度過無數艱險的哈諾，現在已
[PAGE_BREAK]
被其父哈瓦那承認為一個真正的戰士了。據
[PAGE_BREAK]
說父子倆正在建造船舶，準備一起前往某個
[PAGE_BREAK]
傳說中的島嶼進行冒險。
[END]
```

### Page 14  (offset 0x7da, span 120 bytes)

```text
在索爾一行的協助下，鐵諾終於化解了和王
[PAGE_BREAK]
國軍的恩怨。心願已了之後，他決定離開這
[PAGE_BREAK]
個傷心地繼續浪跡天涯。直到再有一天‥‥
[END]
```

### Page 15  (offset 0x852, span 148 bytes)

```text
老而彌堅的哈瓦那是個閒不住的冒險者，聽
[PAGE_BREAK]
說他最近找到一張祖傳的地圖，雀躍不已的
[PAGE_BREAK]
準備要出海去尋寶。當然了，他的兒子哈諾
[PAGE_BREAK]
會是他最得力的助手和伙伴。
[END]
```

### Page 16  (offset 0x8e6, span 156 bytes)

```text
完成了保護王子的使命之後，亞雷斯光榮的
[PAGE_BREAK]
和索爾一起回國，隨即繼任禁衛騎士團長一
[PAGE_BREAK]
職。他預期洛娜必會很快就追到這裡來，而
[PAGE_BREAK]
且索爾會願意當這個現成的介紹人‥‥
[END]
```

### Page 17  (offset 0x982, span 156 bytes)

```text
在索爾等人回國之後，身為騎士團代表的洛
[PAGE_BREAK]
娜也喜孜孜的準備行李，等著和希莉亞一起
[PAGE_BREAK]
前往羅特帝亞訪問。她身上帶著國王的親筆
[PAGE_BREAK]
介紹信，這回亞雷斯是劫數難逃了‥‥
[END]
```

### Page 18  (offset 0xa1e, span 156 bytes)

```text
萊汀仍任王國禁衛軍隊長一職。瑪爾的死解
[PAGE_BREAK]
開了他心裏的死結，而莎拉的英姿則讓他找
[PAGE_BREAK]
到了生命的另一個目標‥這是讓眾人深深祝
[PAGE_BREAK]
福的一對，雖然他們至今仍不肯承認。
[END]
```

### Page 19  (offset 0xaba, span 144 bytes)

```text
這位沈默的騎士幾乎已被視為約拿的伙伴，
[PAGE_BREAK]
所以當約拿在一個多霧的早晨飄然離去時，
[PAGE_BREAK]
蘭斯洛特也隨著離開了。他的過去與一切，
[PAGE_BREAK]
似乎比約拿還要神秘‥‥
[END]
```

### Page 20  (offset 0xb4a, span 156 bytes)

```text
希莉亞雖然獲得了父王的應允，但索爾才剛
[PAGE_BREAK]
受到悠妮離去的打擊，因此這回她出使羅特
[PAGE_BREAK]
帝亞，將不會提起婚事。她相信，只要耐心
[PAGE_BREAK]
的等，索爾總有一天會明白她的心意‥
[END]
```

### Page 21  (offset 0xbe6, span 118 bytes)

```text
在漂浮在另一個時空的黃金城裏，悠妮在休
[PAGE_BREAK]
眠艙裡沈睡著‥等待著她的是另一段長久的
[PAGE_BREAK]
沈眠，連她也不知道自己何時會再醒來。
[END]
```

### Page 22  (offset 0xc5c, span 156 bytes)

```text
自小嬌生慣養的瑪琳在這一番冒險之後，也
[PAGE_BREAK]
終於有了些成長，現在她已是家中的乖女兒
[PAGE_BREAK]
，不再恃嬌惹事，除了繼續僧侶神術的修行
[PAGE_BREAK]
之外，她也已開始學著做些女紅家事。
[END]
```

### Page 23  (offset 0xcf8, span 156 bytes)

```text
身為大祭司的索菲亞在事件結束之後，還是
[PAGE_BREAK]
回到了她所崇信的神的身邊。在亞克斯王國
[PAGE_BREAK]
王城的神殿之中，光之神卡爾汀的祭壇之前
[PAGE_BREAK]
，始終迴盪著她沈靜輕柔的祈禱聲‥‥
[END]
```

### Page 24  (offset 0xd94, span 156 bytes)

```text
在師兄賽可邦勒回到豹人族之後，就只剩凱
[PAGE_BREAK]
麗陪在武神卡里斯身邊。她最近正因要與師
[PAGE_BREAK]
父前往羅特帝亞而興奮不已，但若她知道師
[PAGE_BREAK]
父此行的目的，可能就高興不起來了‥
[END]
```

### Page 25  (offset 0xe30, span 152 bytes)

```text
在事件之後，年輕的貝克威變得更加成熟了
[PAGE_BREAK]
。他不願蜇居於故居的精靈族森林中，想要
[PAGE_BREAK]
出去看看廣大的世界，因此事件結束後他就
[PAGE_BREAK]
離開了，只有珊知道他的去處‥‥
[END]
```

### Page 26  (offset 0xec8, span 140 bytes)

```text
有感於在戰陣中自己在魔法技能上的不足，
[PAGE_BREAK]
珊決定努力修習魔法。據說她和貝克威相約
[PAGE_BREAK]
，當貝克威從外面歷練回來時，她就要成為
[PAGE_BREAK]
一名真正的大法師。
[END]
```

### Page 27  (offset 0xf54, span 142 bytes)

```text
告別了恩師卡里斯之後，賽可邦德回到了豹
[PAGE_BREAK]
人族中，他現在不但是豹人族自衛隊的總教
[PAGE_BREAK]
練，負責訓練年輕的豹人武士，也已經結婚
[PAGE_BREAK]
生子，生活幸福美滿。
[END]
```

### Page 28  (offset 0xfe2, span 158 bytes)

```text
在與眾伙伴們打倒所有魔王後，凱拉斯終於
[PAGE_BREAK]
報了昔日死在黑暗軍手下的部下們的血仇。
[PAGE_BREAK]
在聖寇拉斯遷都平地之後，凱拉斯一直追隨
[PAGE_BREAK]
其後，現在他擔任新都城守備軍的隊長。
[END]
```

### Page 29  (offset 0x1080, span 138 bytes)

```text
憑著在激戰中所獲得的經驗與能力，米亞斯
[PAGE_BREAK]
多德得以正式成為龍人族衛隊的一個隊長，
[PAGE_BREAK]
他雖不時惦念過去自由的生活，但卻也能安
[PAGE_BREAK]
於自己的新職位。
[END]
```

### Page 30  (offset 0x110a, span 150 bytes)

```text
告別了眾位伙伴之後，密蒂回到了她的隱居
[PAGE_BREAK]
之所，繼續劍術和心靈的修練。那個年輕人
[PAGE_BREAK]
又在她沈靜的心湖裏掀起了一陣波瀾‥但她
[PAGE_BREAK]
相信，絕不會再有第三次了‥‥
[END]
```

### Page 31  (offset 0x11a0, span 152 bytes)

```text
羅德曼在事件後便悄悄回到領地，以免擅離
[PAGE_BREAK]
職守參與冒險之事被國王得知。但在希莉亞
[PAGE_BREAK]
的多嘴之下，羅德曼仍被邀請往王城一行，
[PAGE_BREAK]
痛快的和眾人舉行了一場慶功宴。
[END]
```

### Page 32  (offset 0x1238, span 128 bytes)

```text
在宴會結束之後，莎拉迅即向眾人道別，然
[PAGE_BREAK]
後乘上飛龍離去。她說她是要去尋找散落在
[PAGE_BREAK]
大陸上的族人們，重新復興龍騎士一族。然
[PAGE_BREAK]
而‥‥
[END]
```

### Page 33  (offset 0x12b8, span 156 bytes)

```text
這位始終充滿了神秘的賢者在亞克斯王國作
[PAGE_BREAK]
了幾天客之後，便向他的老友大法師希爾法
[PAGE_BREAK]
道別，又開始了另一次新的旅程。當然，這
[PAGE_BREAK]
次還是沒有人知道他們要到哪裡去‥‥
[END]
```

### Page 34  (offset 0x1354, span 140 bytes)

```text
武神卡里斯正準備結束多年來在馬拉大陸的
[PAGE_BREAK]
旅程，返回故居羅特帝亞。據說他此行除了
[PAGE_BREAK]
回故鄉定居外還另有目的，至於究竟是什麼
[PAGE_BREAK]
目的就不得而知了。
[END]
```

### Page 35  (offset 0x13e0, span 152 bytes)

```text
身為精靈族的一員，羅蘭也隨著其他的族人
[PAGE_BREAK]
們回到了森林中，她最近成為大法師希爾法
[PAGE_BREAK]
的助手，大概是基於昔日的經驗，想在幫忙
[PAGE_BREAK]
之餘順便學一些戰鬥用的魔法吧！
[END]
```

### Page 36  (offset 0x1478, span 146 bytes)

```text
希爾法在事件結束後又開始了他的新課題，
[PAGE_BREAK]
整天待在位於哈斯米爾的的新研究所裏，據
[PAGE_BREAK]
說他正致力於時空魔法的研究，希望能再把
[PAGE_BREAK]
黃金城帶回到這世界上來。
[END]
```

### Page 37  (offset 0x150a, span 140 bytes)

```text
在戰鬥結束後，謝多又回到了黑森林，過著
[PAGE_BREAK]
隱居的清閒生活。由於萊汀不時委託北上的
[PAGE_BREAK]
商隊為他送去各種生活用品，謝多晚年的日
[PAGE_BREAK]
子過得愉快而愜意。
[END]
```

### Page 38  (offset 0x1596, span 144 bytes)

```text
為了興旺龍人一族，身為龍人族之王的聖寇
[PAGE_BREAK]
拉斯決定將首都由高山遷到平地，並在亞克
[PAGE_BREAK]
斯王國的協助下建立新的城市，相信龍人族
[PAGE_BREAK]
必定會很快的昌盛起來。
[END]
```

### Page 39  (offset 0x1626, span 154 bytes)

```text
由於他優越的武術和人品，禁衛軍隊長萊汀
[PAGE_BREAK]
特地留巴拿羅西亞在亞克斯王國住下，平時
[PAGE_BREAK]
他是龍人族在亞克斯王國的使節，也兼任禁
[PAGE_BREAK]
衛軍的武術教練，和萊汀相處甚歡。
[END]
```

### Page 40  (offset 0x16c0, span 156 bytes)

```text
由於到亞齊梅吉的賞識，達克塞回到惡魔族
[PAGE_BREAK]
王國之後便被任命為禁衛軍隊長，把守惡魔
[PAGE_BREAK]
族的邊界。由於他對人類的瞭解與好感，在
[PAGE_BREAK]
與人類文明接觸之初解決了不少紛爭。
[END]
```

### Page 41  (offset 0x175c, span 126 bytes)

```text
戰爭結束後，惡魔族之王也恢復了正常。亞
[PAGE_BREAK]
奇梅吉回到了宰相的位子，在他的一生中，
[PAGE_BREAK]
始終致力於與其他人類國家的和平相處與友
[PAGE_BREAK]
誼。
[END]
```

### Page 42  (offset 0x17da, span 130 bytes)

```text
失去了悠妮的意念控制，蓋亞的機能已經完
[PAGE_BREAK]
全休止。雖然如此，索爾仍執意要把它運回
[PAGE_BREAK]
羅特帝亞，因為這是悠妮留給他的唯一的紀
[PAGE_BREAK]
念品‥‥
[END]
```

### Page 43  (offset 0x185c, span 146 bytes)

```text
渥德在悠妮和黃金城離開之後，失去了控制
[PAGE_BREAK]
聯繫的中樞，竟然就此不再動彈。但萊汀仍
[PAGE_BREAK]
將它好好的保存在王宮禁衛隊的寶庫中，希
[PAGE_BREAK]
望有天能再得到它的效力。
[END]
```

### Page 44  (offset 0x18ee, span 262 bytes)

```text
　　在亞克斯王國宮廷中
[PAGE_BREAK]
　盛大的慶祝宴會結束之後，
[PAGE_BREAK]
　　離別的時刻終於來到了。
[PAGE_BREAK]
昔日的戰友們自此將互道珍重，
[PAGE_BREAK]
　　　　各奔前程。
[PAGE_BREAK]
　　　在往日的回憶中，
[PAGE_BREAK]
　　　在未來的歲月裏，
[PAGE_BREAK]
　或許很難有再相見的機會，
[PAGE_BREAK]
　但這段冒險的回憶仍會長存，
[PAGE_BREAK]
　　在每個人的心裡‥‥‥
[END]
```

### Page 45  (offset 0x19f4, span 112 bytes)

```text
但她相信，三千年也好，三萬年也好，她一
[PAGE_BREAK]
定會再那個叫做索爾的年輕人在夢中相聚，
[PAGE_BREAK]
她相信，這樣就已經足夠了‥‥‥
[END]
```



---

## Entry 32 — 第 1 章序章對話（第二幕，chapter_id 0x1F）

range [0x1c24e, 0x1caea), size 2204 bytes, 11 pages。第 1 章序章第二幕：
`fd2_chapter_01_init` 在 prologue 地圖 chapter_id=0x1F 的 Phase C 播出 page 0..9
（搭配 cutscene 0x5A..0x62），內容為索爾與亞雷斯練劍時發現昏倒的悠妮與機器人蓋亞、
決意護送她前往馬拉大陸；page 10 在 init handler 內無 callsite（未使用）。

### Pages

### Page 0  (offset 0x16, span 356 bytes)

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『亞雷斯，趕快再來比一場！
[PAGE_BREAK]
　昨天輸給你，我越想越不甘
[PAGE_BREAK]
　心！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0001]
『贏了就是贏了，何況你
[PAGE_BREAK]
　已經打贏我十幾場，讓我一
[PAGE_BREAK]
　場有什麼關係！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『這可不一樣！我老爸說過，
[PAGE_BREAK]
　劍技要不斷磨練才會進步‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0001]
『拜託，今天休息吧！風和日
[PAGE_BREAK]
　麗的日子，躺在草地上曬曬
[PAGE_BREAK]
　太陽，不是也很愜意？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『好吧，既然如此，我回去找
[PAGE_BREAK]
　侍衛們練習也可以！』
[END]
```

### Page 1  (offset 0x17a, span 28 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0001]
『咦，那是什麼東西？』
[END]
```

### Page 2  (offset 0x196, span 18 bytes)

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『怎麼了？』
[END]
```

### Page 3  (offset 0x1a8, span 74 bytes)

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『好像是人昏倒在地呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『是啊，旁邊那個大傢伙又是
[PAGE_BREAK]
　什麼？』
[END]
```

### Page 4  (offset 0x1f2, span 74 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0003]
『不要再接近！否則我將照規
[PAGE_BREAK]
　定採取防衛行動！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『搞什麼嘛！』
[END]
```

### Page 5  (offset 0x23c, span 64 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『嗯，好痛‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『是女孩子呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『我去看看！』
[END]
```

### Page 6  (offset 0x27c, span 338 bytes)

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『小‥小姐，你沒事吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『你‥你是誰？
[PAGE_BREAK]
　這裡是哪裡？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『這裡是羅特帝亞，我叫索爾
[PAGE_BREAK]
　請問小姐你的芳名？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『我叫‥我叫‥悠妮吧。
[PAGE_BREAK]
　我怎麼會來到這裡的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『悠妮？好名字。悠妮小姐，
[PAGE_BREAK]
　妳怎麼會記不得怎麼來到這
[PAGE_BREAK]
　裡的？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『‥‥真的想不起來了，只記
[PAGE_BREAK]
　得‥只記得我好像是從很遠
[PAGE_BREAK]
　很遠的地方來的。蓋亞？』
[END]
```

### Page 7  (offset 0x3ce, span 62 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0003]
『是的，主人。有何吩咐？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『我們‥是從哪裡來的？』
[END]
```

### Page 8  (offset 0x40c, span 456 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0003]
『據我的慣性導航系統顯示，
[PAGE_BREAK]
　是在那個方位。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『啊！我知道了，你們是從馬
[PAGE_BREAK]
　拉大陸來的，我聽老爸說過
[PAGE_BREAK]
　，那的確是個很遙遠的地方
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『悠妮小姐，妳是從那個馬‥
[PAGE_BREAK]
　馬拉大陸來的嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『我記不得了，只記得‥好可
[PAGE_BREAK]
　怕的惡夢‥好可怕‥不！不
[PAGE_BREAK]
　要！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『悠‥悠妮小姐！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『我好怕‥帶我回家‥帶我回
[PAGE_BREAK]
　家，好嗎？求求你‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『‥好！好！別怕，有我在‥
[PAGE_BREAK]
　放心，我一定帶妳回家！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『哎喲喂！這小子該不會想要
[PAGE_BREAK]
　‥‥』
[END]
```

### Page 9  (offset 0x5d4, span 532 bytes)

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『喂！你真的要去？你瘋了！
[PAGE_BREAK]
　聽說那裡是與羅特帝亞完全
[PAGE_BREAK]
　不一樣的地方，為了一個女
[PARAGRAPH]
　孩子去冒這種險，未免‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『我一定要去！亞雷斯，
[PAGE_BREAK]
　你知不知道怎麼去？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0001]
『這？好‥好吧，我回去問問
[PAGE_BREAK]
　老爹，我想他應該會知道‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『好極了，我們整理一些輕便
[PAGE_BREAK]
　行李，我再向父王稟告一下
[PAGE_BREAK]
　，就可以出發了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『我也去好了！誰叫我們是
[PAGE_BREAK]
　從小打架打到大的朋友？
[PAGE_BREAK]
　總不能就這樣讓你一個人到
[PARAGRAPH]
　馬拉大陸去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『亞雷斯，謝謝你！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『不用謝了，你先帶悠妮小姐
[PAGE_BREAK]
　回去休養吧！陛下那邊和借
[PAGE_BREAK]
　船的事我來辦就好了。』
[END]
```

### Page 10  (offset 0x7e8, span 180 bytes)

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『悠妮小姐，妳就先和我回去
[PAGE_BREAK]
　吧！幾天後我們就啟程回馬
[PAGE_BREAK]
　拉大陸，妳放心，我一定平
[PARAGRAPH]
　安的送妳回家。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『謝‥謝謝你。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0000]
『嘿，我們這就走吧！還有不
[PAGE_BREAK]
　少事要忙呢！』
[END]
```



---

## Entry 33 — 第 1 章序章對話（第一幕，chapter_id 0x20）

range [0x1caea, 0x1d6b6), size 3020 bytes, 6 pages。第 1 章序章第一幕：
`fd2_chapter_01_init` 在 prologue 地圖 chapter_id=0x20 的 Phase A 播出 page 0/1
（王座廳，國王告知索爾將繼承羅特帝亞王位）、Phase B 播出 page 2..5（索爾向亞雷斯
吐露繼位煩惱、相約後山練劍）；6 pages 全數用完，播放順序在 entry 32 之前。

### Pages

### Page 0  (offset 0xc, span 272 bytes)

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『兒臣索爾，晉見父王陛下。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0000]
『你來啦！起來起來，這裡沒
[PAGE_BREAK]
　有別人，不用多禮。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『多謝父王。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0000]
『到這裡來，讓父王好好看看
[PAGE_BREAK]
　你。這一陣子國務繁忙，我
[PAGE_BREAK]
　們父子兩個也很久沒聚聚了
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0001]
『是啊！我們一家人，難得這
[PAGE_BREAK]
　樣在一起。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『是。』
[END]
```

### Page 1  (offset 0x11c, span 1332 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0000]
『索爾，時間過得真快，你已
[PAGE_BREAK]
　經長得這麼高了。大臣和將
[PAGE_BREAK]
　領們對你的武藝和能力也讚
[PARAGRAPH]
　譽有加，我想，也該是決定
[PAGE_BREAK]
　王位繼承人的時候了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『是的，父王，想必是由皇弟
[PAGE_BREAK]
　迪恩接任王位，兒臣不勝欣
[PAGE_BREAK]
　喜。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0000]
『錯了，索爾，朕希望能由你
[PAGE_BREAK]
　來接任羅特帝亞的王位。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『啊？兒‥兒臣我來接任王位
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0000]
『是的，索爾。你對朕的決定
[PAGE_BREAK]
　有什麼疑問嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『兒臣不敢。但‥‥‥但是，
[PAGE_BREAK]
　父王，兒臣並未繼承羅特帝
[PAGE_BREAK]
　亞王家的血脈，照理來說，
[PARAGRAPH]
　兒臣以為應該由皇弟迪恩
[PAGE_BREAK]
　來繼承王位‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0000]
『索爾，這是一個屬於強者的
[PAGE_BREAK]
　時代。只有強者才能掌握軍
[PAGE_BREAK]
　隊、統治國家，維持世界的
[PARAGRAPH]
　和平和人民的福祉。羅特帝
[PAGE_BREAK]
　亞需要一個強有力的統治者
[PAGE_BREAK]
　我相信你能接替我繼承王位
[PARAGRAPH]
　——而且也只有你才能繼承
[PAGE_BREAK]
　王位。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可是，皇弟他‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0000]
『迪恩他和你不一樣。迪恩是
[PAGE_BREAK]
　個聰明的孩子，可以成為賢
[PAGE_BREAK]
　明的君主，但是他長得太文
[PARAGRAPH]
　弱了，無法穿著重甲、統率
[PAGE_BREAK]
　大軍在戰場上奔馳。我還記
[PAGE_BREAK]
　得當我還是個孩子時所發生
[PARAGRAPH]
　的事情，所以‥為了羅特帝
[PAGE_BREAK]
　亞王族，為了羅特帝亞王國
[PAGE_BREAK]
　的人民，我希望你能成為王
[PARAGRAPH]
　位繼承人。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0001]
『是的。索爾，這些年來，我
[PAGE_BREAK]
　們把你當成自己親生的孩子
[PAGE_BREAK]
　撫養長大，這件事也是經過
[PARAGRAPH]
　慎重考慮之後才決定的，希
[PAGE_BREAK]
　望你能夠答應。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『兒臣‥兒臣‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0000]
『我就知道你會一時無法接受
[PAGE_BREAK]
　這樣吧，三天之後，你再給
[PAGE_BREAK]
　我考慮的結果，當然了，我
[PARAGRAPH]
　希望能夠聽到肯定的答覆。
[PAGE_BREAK]
　你先回去休息吧。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『‥是的。兒臣告退。』
[END]
```

### Page 2  (offset 0x650, span 240 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『哈！找你找了半天，居然是
[PAGE_BREAK]
　躲在這裡打瞌睡，真是一點
[PAGE_BREAK]
　都不像你的作風。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『怎麼啦？你今天好像有點不
[PAGE_BREAK]
　太對勁？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『‥‥唉！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『嘿！待會兒是不是要打雷了
[PAGE_BREAK]
　你居然在嘆氣！我看我們得
[PAGE_BREAK]
　好好談一談了！』
[END]
```

### Page 3  (offset 0x740, span 176 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『索爾，到底是發生什麼事了
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『‥其實也沒什麼啦。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『少裝啦！這世界上能讓你嘆
[PAGE_BREAK]
　氣的事絕對不會超過十件。
[PAGE_BREAK]
　說來聽聽吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0003]
『‥父王要我繼承王位。』
[END]
```

### Page 4  (offset 0x7f0, span 14 bytes)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『嗄？』
[END]
```

### Page 5  (offset 0x7fe, span 974 bytes)

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『沒聽清楚嗎？父王要我繼承
[PAGE_BREAK]
　王位。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『嗄？‥這‥這有什麼好嘆氣
[PAGE_BREAK]
　的？羅特帝亞的准王位繼承
[PAGE_BREAK]
　人‥‥這要好好慶祝一下才
[PARAGRAPH]
　是啊！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『笨蛋！枉費我們從小一起長
[PAGE_BREAK]
　大，你竟然一點都不瞭解我
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『啊？這怎麼說呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『我怎麼會喜歡國王一職！整
[PAGE_BREAK]
　天待在王城裏哪裡也不能去
[PAGE_BREAK]
　還得管理繁瑣的政務、應付
[PARAGRAPH]
　囉唆的大臣和官員‥‥那有
[PAGE_BREAK]
　什麼樂趣！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『以你的個性來說‥這樣講也
[PAGE_BREAK]
　沒錯。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『我從小想做的事，就是到外
[PAGE_BREAK]
　地去做幾次轟轟烈烈的冒險
[PAGE_BREAK]
　絕對不是枯坐在王位上終老
[PARAGRAPH]
　一生！我只是父王的養子，
[PAGE_BREAK]
　和羅特帝亞王家沒有血緣關
[PAGE_BREAK]
　係，所以我一直以為會由皇
[PARAGRAPH]
　弟迪恩繼承王位，我就可以
[PAGE_BREAK]
　落得輕鬆，那知‥唉！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『這就沒辦法啦。你已經被迫
[PAGE_BREAK]
　答應了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『父王要我三天後給他答覆，
[PAGE_BREAK]
　但是我能說不嗎？這和答應
[PAGE_BREAK]
　了有什麼兩樣！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『說的也是‥只可惜這件事我
[PAGE_BREAK]
　幫不上什麼忙。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0003]
『唉！如果你想幫忙的話，現
[PAGE_BREAK]
　在就陪我到後山去練劍，痛
[PAGE_BREAK]
　快的打上一場之後，也許我
[PARAGRAPH]
　會覺得好過一點。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0004]
『這是小事一樁！我們現在就
[PAGE_BREAK]
　走吧！』
[END]
```

