# 第 15 章 — 拉卡湖的激戰

於拉卡湖支援豹人族迎戰獸人首領薩卡的部隊；揭露「魔眼寶石」與獸人族陰謀的關連。劇情依「隊伍是否含凱麗 (ch12 加入的劍士)」切兩線。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 15 (0xF) | 賽可邦勒 | 章末 | End handler 加入 |

## 敵人配置

由 FDFIELD.DAT entry 43 的 char_spawn_records 決定（chapter_id × 3 + 1, chapter_id = 14）。詳見 `resource_info/fdfield.md`。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。獸人運送的「光之眼」/「魔眼寶石」可於對話中得知（page 11）。

## 特殊機制

- **Conditional dialog**：Init 與 End handler 都依「隊伍是否含凱麗 (char_id 0xC，ch12 加入)」切兩組對話：
  - 含凱麗 → Init pages 0/1/2、End page 12（凱麗認出師兄賽可邦勒的劇情線）
  - 不含凱麗 → Init pages 3/4/5、End page 13（賽可邦勒不認識來者的劇情線）
- **失敗條件**：索爾死亡 / 賽可邦勒 (char[0x40]) 死亡，由 `chapter_15_post_action` 判定。
- **豹人 NPC AI**：攻略「前三回合豹人會往左退，第四回合開始攻擊敵軍」屬 NPC AI behavior（FDFIELD turn-event 或 NPC behavior class 切換）。

## 對話

對話文字 14 pages 來自 FDTXT.DAT entry 15。Init 與 End 依凱麗在隊與否各引用兩組互斥的 page；其餘 pages 由 FDFIELD turn-event handler 引用。

### Page 0 (Init 含凱麗)

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『我看到獸人軍了，他們就位
[PAGE_BREAK]
　在北方，更遠的地方好像是
[PAGE_BREAK]
　豹人族的部隊。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『談到豹人，我就想起我的師
[PAGE_BREAK]
　兄賽可邦勒‥‥咦？
[PAGE_BREAK]
　那不就是我師兄嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『賽可邦勒師兄！是你嗎？
[PAGE_BREAK]
　我是凱麗啊！』
[END]
```

### Page 1 (Init 含凱麗)

```text
[PORTRAIT_LEFT_BY_ID=0x000F]
『誰在叫我？』
[END]
```

### Page 2 (Init 含凱麗)

```text
[PORTRAIT_RIGHT_BY_ID=0x000F]
『咦！是凱麗啊！真是湊巧，
[PAGE_BREAK]
　怎麼會在這時候遇上妳‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『我聽說獸人大軍攻打豹人族
[PAGE_BREAK]
　就帶朋友們來幫忙，也沒預
[PAGE_BREAK]
　想到會在這裡遇到師兄你‥
[PARAGRAPH]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000F]
『既然如此，就先謝謝妳了！
[PAGE_BREAK]
　還有，這批獸人是由其首領
[PAGE_BREAK]
　薩卡所帶領，你們要特別小
[PARAGRAPH]
　心！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『知道了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『索爾，亞雷斯，大家幫我這
[PAGE_BREAK]
　個忙好嗎？我一個人是打不
[PAGE_BREAK]
　過這批獸人大軍的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『放心吧，我們本來就是要來
[PAGE_BREAK]
　幫豹人的，索爾，你說是不
[PAGE_BREAK]
　是？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『是啊！來這裡不就是要打架
[PAGE_BREAK]
　的嗎？眼前就有一大堆欠揍
[PAGE_BREAK]
　的傢伙，還囉唆什麼？我們
[PARAGRAPH]
　上吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『謝謝大家！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『哼，又有妨礙者了嗎？
[PAGE_BREAK]
　通通都去死吧！
[PAGE_BREAK]
　給我殺的一個都不剩！』
[END]
```

### Page 3 (Init 不含凱麗)

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『我看到獸人軍了，他們就位
[PAGE_BREAK]
　在北方，更遠的地方好像是
[PAGE_BREAK]
　豹人族的部隊。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『看來豹人情勢不妙，我們趕
[PAGE_BREAK]
　快去助他們一臂之力！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『喂！豹人的老兄們別怕，
[PAGE_BREAK]
　我們來幫忙了！』
[END]
```

### Page 4 (Init 不含凱麗)

```text
[PORTRAIT_LEFT_BY_ID=0x000F]
『是誰來了？
[PAGE_BREAK]
　是敵方援軍嗎？』
[END]
```

### Page 5 (Init 不含凱麗)

```text
[PORTRAIT_LEFT_BY_ID=0x000F]
『咦！是一群年輕小伙子‥‥
[PAGE_BREAK]
　此地戰況險惡，你們來這裡
[PAGE_BREAK]
　幹什麼？不想受傷就快走吧
[PARAGRAPH]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『老兄你心地不錯啊！放心，
[PAGE_BREAK]
　我們是來幫你們的，這些雜
[PAGE_BREAK]
　碎交給我們就沒錯啦！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000F]
『這樣嗎？看你們信心十足的
[PAGE_BREAK]
　樣子‥好吧，我在此先謝謝
[PAGE_BREAK]
　你們！此外，這批獸人是由
[PARAGRAPH]
　其首領薩卡所帶領，你們要
[PAGE_BREAK]
　特別小心！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『知道啦！各位，咱們上吧！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『哼，又有妨礙者了嗎？
[PAGE_BREAK]
　通通都去死吧！
[PAGE_BREAK]
　給我殺的一個都不剩！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x000F]
『沒退路了，
[PAGE_BREAK]
　大家跟他們拼了！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『師父，
[PAGE_BREAK]
　我對不起您‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『就這幾個人還解決不了，
[PAGE_BREAK]
　第二隊再上！』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『‥啊‥主人，救救我‥
[PAGE_BREAK]
　你答應要給我‥
[PAGE_BREAK]
　給我力量和‥
[PARAGRAPH]
　永恆的‥生命的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『糟啦，這一下打得太重了些
[PAGE_BREAK]
　等一下沒人可以問話了，
[PAGE_BREAK]
　可惜可惜。』
[END]
```

### Page 10

```text
[PORTRAIT_LEFT_BY_CHAR=0x004A]
『哎呀，我們好像來遲啦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x004B]
『首領命令我們攜帶這批貴重
[PAGE_BREAK]
　寶物，為了避免引起注意所
[PAGE_BREAK]
　以繞道而行，遲到也是應該
[PARAGRAPH]
　的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x004C]
『不知道現在戰況如何？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x001E]
『喂！你們不是負責運送寶物
[PAGE_BREAK]
　的嗎？來這裡幹什麼？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x004A]
『首領吩咐我們帶著東西，
[PAGE_BREAK]
　到這裡和他們會合‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x001E]
『笨蛋！你沒看到戰況不妙嗎
[PAGE_BREAK]
　趕快離開這裡，把東西帶到
[PAGE_BREAK]
　艾斯島去！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x004A]
『‥知‥知道了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x004B]
『趕快逃啊！』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0011]
『咦？這‥這不是「光之眼」
[PAGE_BREAK]
　寶石嗎？為何獸人族也在收
[PAGE_BREAK]
　集這些東西‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『傳說中的「魔眼寶石」嗎？
[PAGE_BREAK]
　我聽老師說過，聽說它們有
[PAGE_BREAK]
　強大的魔力，但這和獸人族
[PARAGRAPH]
　有什麼關係？‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『妳也知道嗎？據說它們的價
[PAGE_BREAK]
　值並不只是魔力而已，而是
[PAGE_BREAK]
　和一件古代的秘密有很大的
[PARAGRAPH]
　關係‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『你們在談些什麼？
[PAGE_BREAK]
　好像很有趣的樣子！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『不聊了，
[PAGE_BREAK]
　先把仗打完再說！』
[END]
```

### Page 12 (End 含凱麗)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『感謝各位的協助，我們才能
[PAGE_BREAK]
　夠抵擋獸人軍的攻勢，我們
[PAGE_BREAK]
　代表豹人族向各位致上十二
[PARAGRAPH]
　萬分的謝意。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『師兄，不用客氣了啦，我這
[PAGE_BREAK]
　些朋友們都是正義的熱血好
[PAGE_BREAK]
　漢，他們一路上已經不知道
[PARAGRAPH]
　幫了多少人呢。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『話雖然這麼說，我還是要謝
[PAGE_BREAK]
　謝他們。對了，凱麗，妳可
[PAGE_BREAK]
　有看到獸人的軍隊在其他地
[PARAGRAPH]
　方出現？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000C]
『有啊！他們先是攻打精靈族
[PAGE_BREAK]
　的都城哈斯米爾，然後又和
[PAGE_BREAK]
　我們在蘭迪平原打了一仗，
[PARAGRAPH]
　最近獸人都看得都厭了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『有這種事？看來陰謀之說果
[PAGE_BREAK]
　然是真的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『什麼意思？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『最近我得到消息，說有人在
[PAGE_BREAK]
　背後操縱黑森林一帶的獸人
[PAGE_BREAK]
　族和狂戰士，準備要對精靈
[PARAGRAPH]
　族和豹人族進行滅族的屠戮
[PAGE_BREAK]
　行動，我起初還半信半疑，
[PAGE_BREAK]
　兼程趕回家鄉看看，沒想到
[PARAGRAPH]
　就遇上了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『我們精靈族也差點慘遭屠戮
[PAGE_BREAK]
　到底是誰在背後操縱這一切
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『這我也不清楚，不過據我那
[PAGE_BREAK]
　朋友所言，有更大群的獸人
[PAGE_BREAK]
　和黑暗戰士在北方的冰島一
[PARAGRAPH]
　帶集結，她已經先前去調查
[PAGE_BREAK]
　了，我預計在解決這裡的事
[PAGE_BREAK]
　情之後，也會儘快趕去和她
[PARAGRAPH]
　會合。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『嗯，約拿似乎也是往這個方
[PAGE_BREAK]
　向而去，看來這些事情似乎
[PAGE_BREAK]
　有所關連‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『既然順路的話，那我們就一
[PAGE_BREAK]
　起去吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『就這麼辦吧，我倒想看看到
[PAGE_BREAK]
　底是誰在搞鬼！走吧！』
[END]
```

### Page 13 (End 不含凱麗)

```text
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『感謝各位的協助，我們才能
[PAGE_BREAK]
　夠抵擋獸人軍的攻勢，我們
[PAGE_BREAK]
　代表豹人族向各位致上十二
[PARAGRAPH]
　萬分的謝意。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『小事一樁！正好也可讓我們
[PAGE_BREAK]
　活動活動筋骨。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『話雖然這麼說，我還是要謝
[PAGE_BREAK]
　謝你們。對了，各位可有看
[PAGE_BREAK]
　到獸人的軍隊在其他地方出
[PARAGRAPH]
　現？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『有啊！他們先是攻打我們精
[PAGE_BREAK]
　靈族族的都城哈斯米爾，然
[PAGE_BREAK]
　後又和我們在蘭迪平原打了
[PARAGRAPH]
　一仗，最近獸人真是囂張得
[PAGE_BREAK]
　不得了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『有這種事？看來陰謀之說果
[PAGE_BREAK]
　然是真的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『什麼意思？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『最近我得到消息，說有人在
[PAGE_BREAK]
　背後操縱黑森林一帶的獸人
[PAGE_BREAK]
　族和狂戰士，準備要對精靈
[PARAGRAPH]
　族和豹人族進行滅族的屠戮
[PAGE_BREAK]
　行動，我起初還半信半疑，
[PAGE_BREAK]
　兼程趕回家鄉看看，沒想到
[PARAGRAPH]
　就遇上了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『我們精靈族也差點慘遭屠戮
[PAGE_BREAK]
　到底是誰在背後操縱這一切
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0040]
『這我也不清楚，不過據我那
[PAGE_BREAK]
　朋友所言，有更大群的獸人
[PAGE_BREAK]
　和黑暗戰士在北方的冰島一
[PARAGRAPH]
　帶集結，她已經先前去調查
[PAGE_BREAK]
　了，我預計在解決這裡的事
[PAGE_BREAK]
　情之後，也會儘快趕去和她
[PARAGRAPH]
　會合。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『嗯，約拿似乎也是往這個方
[PAGE_BREAK]
　向而去，看來這些事情似乎
[PAGE_BREAK]
　有所關連‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『既然順路的話，那我們就一
[PAGE_BREAK]
　起去吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『就這麼辦吧，我倒想看看到
[PAGE_BREAK]
　底是誰在搞鬼！走吧！』
[END]
```
