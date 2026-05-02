# 第 9 章 — 騎士的抉擇

希莉亞與禁衛軍隊長萊汀正面交鋒；戰後揭露葛雷大臣陰謀，國王被擄。

## 加入角色

無 init 加入。End handler 復活 char[11]（ch9 init 已預初始化但 marked dead 的角色）。

## 敵人配置

由 FDFIELD.DAT entry 25 的 char_spawn_records 決定（chapter_id × 3 + 1, chapter_id = 8）。詳見 `resource_info/fdfield.md`。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。

## 特殊機制

- **Init 強制面朝 north**：迴圈把前 11 個 char (`runtime_char_array[0..0xA]`) 的 sprite facing 全部設為 2 (north)，作為章首 cutscene 排隊視覺效果。
- **萊汀被打敗援軍出場**：攻略「萊汀被打敗時敵方騎兵援軍立即出現在上方」屬 FDFIELD event (boss-death-triggered reinforcement)。
- **勝負條件**：default — 全敵死 = 勝、索爾死 = 負。

## 對話

對話文字 5 pages 來自 FDTXT.DAT entry 9。Init 引用 page 0/1，End handler 引用 page 4。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『希莉亞，看來妳父王的大軍
[PAGE_BREAK]
　已經在等我們了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『放心，交給我就是了。』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『殿下，以您王國公主的身份
[PAGE_BREAK]
　怎可和這些盜匪為伍？
[PAGE_BREAK]
　陛下已下令逮捕這幫盜賊，
[PARAGRAPH]
　讓屬下迎接殿下回宮，這些
[PAGE_BREAK]
　盜匪交給禁衛軍處理就可以
[PAGE_BREAK]
　了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『萊汀，我一直以為你是個聰
[PAGE_BREAK]
　明人，難道連你也不相信我
[PAGE_BREAK]
　嗎？趕快把這些禁衛軍撤走
[PARAGRAPH]
　我要去見父王，把事情說個
[PAGE_BREAK]
　清楚。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『公主殿下，陛下有令在先，
[PAGE_BREAK]
　把這幫匪徒通通抓起來再說
[PAGE_BREAK]
　，如果您還是要護著他們的
[PARAGRAPH]
　話，那屬下就得罪了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『萊汀，你敢對我動手！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『公主殿下，
[PAGE_BREAK]
　請原諒屬下的無禮。
[PARAGRAPH]
　來人，把這群盜匪都給我抓
[PAGE_BREAK]
　起來！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『唔‥公主殿下，您為何‥
[PAGE_BREAK]
　為何要帶這些人來加害
[PAGE_BREAK]
　陛下‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『萊汀，我怎麼會想要加害
[PAGE_BREAK]
　父王！宮中到底發生了
[PAGE_BREAK]
　什麼事？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『‥前幾天有刺客夜襲陛下的
[PAGE_BREAK]
　寢宮，想要抓走陛下，幸好
[PAGE_BREAK]
　打鬥聲驚動了侍衛，
[PARAGRAPH]
　衛兵衝進來救駕，陛下才倖
[PAGE_BREAK]
　免於難‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『有這種事？！說下去！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『刺客臨走前還揚言，
[PAGE_BREAK]
　幾天之內一定會再來‥‥
[PAGE_BREAK]
　陛下遭到這種驚嚇，
[PARAGRAPH]
　幾天來一直在床上休養‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這件事又為何會扯到我們
[PAGE_BREAK]
　身上？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『為了搜查可疑份子，我們在
[PAGE_BREAK]
　王國各地派出調查隊，結果
[PAGE_BREAK]
　又發現了不少怪事，例如普
[PARAGRAPH]
　里茲港的失蹤事件，而你們
[PAGE_BREAK]
　又惹上了調查隊，結果‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『大臣們一致認為我們就是刺
[PAGE_BREAK]
　客？這太荒謬了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『葛雷大人相當支持這種說法
[PAGE_BREAK]
　所以命令我們禁衛軍派兵攻
[PAGE_BREAK]
　擊你們‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『不是父王下令的嗎？怎麼又
[PAGE_BREAK]
　變成葛雷了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『陛下多日來一直臥病在床，
[PAGE_BREAK]
　軍令方面都是經由葛雷大人
[PAGE_BREAK]
　發佈‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0022]
『禁衛軍隊長萊汀，想不到你
[PAGE_BREAK]
　竟敢和盜匪勾結，我們奉葛
[PAGE_BREAK]
　雷大人之命將你逮捕！
[PARAGRAPH]
　趕快束手就擒吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『可惡，我明白了，葛雷這個
[PAGE_BREAK]
　該死的叛徒‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『萊汀，照這樣看來，父王處
[PAGE_BREAK]
　境危險，我們先設法殺進去
[PAGE_BREAK]
　再說！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『殿下，屬下被奸人所騙，
[PAGE_BREAK]
　有負保護陛下的使命，
[PAGE_BREAK]
　請讓屬下和你們一起去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『你受傷這麼重，還是先‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『喂！希莉亞，現在情況緊急
[PAGE_BREAK]
　我看這傢伙蠻能打的，叫瑪
[PAGE_BREAK]
　琳用法術給他治一下傷就可
[PARAGRAPH]
　以上陣了，應該可以多少幫
[PAGE_BREAK]
　上點忙吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『索爾，你說話也客氣點‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『公主殿下，請給屬下一個將
[PAGE_BREAK]
　功贖罪的機會！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這不就好了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『好吧，容我再借重一次大家
[PAGE_BREAK]
　的力量！我們上！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x004C]
『不得了了！葛雷大人被殺了
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x000B]
『把話講清楚一點！倒底是怎
[PAGE_BREAK]
　麼一回事？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x004C]
『我‥我是葛雷大人房外的衛
[PAGE_BREAK]
　兵，剛才忽然聽到房內傳出
[PAGE_BREAK]
　一陣慘叫聲，我衝進去一看
[PARAGRAPH]
　葛雷大人已經倒在地上不省
[PAGE_BREAK]
　人事了‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『有這種事！公主殿下，看來
[PAGE_BREAK]
　王城內情勢險惡，我先進去
[PAGE_BREAK]
　探查一番，再回來向殿下報
[PARAGRAPH]
　告！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『小心點！』
[END]
```
