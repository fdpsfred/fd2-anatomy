# 第 21 章 — 亞述森林

亞述森林精靈族與「死亡骷髏」首領瑪爾決戰；若全程持有 6 件指定物品，將觸發隱藏的「天空之鑰」兌換儀式。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 24 (0x18) | 祭司希爾法 | 章末 | 無條件 |
| 23 (0x17) | 神射手羅蘭 | 章末 | 無條件 |

## 敵人配置

「死亡骷髏」首領瑪爾 + 大量傭兵。第 2、4、6、8 回合己方結束時，地圖四個角落各 spawn 一隻魔鬼 (FDFIELD turn-event 0x2F 重複觸發 4 次)。配置寫在 FDFIELD.DAT entry 61。

## 寶物

待 FDFIELD.DAT entry 61 確認。

## 商店

無章內商店。

## 特殊機制 — 6 件物品收集鏈與「天空之鑰」

End handler 對隊伍進行雙重迴圈，計算「持有 item 0xD1..0xD6 任一物品」的 char 數。若 6 件全齊：

1. 移除全部 6 件 item
2. 透過 give_item_to_first_player_char(100) 發放 item 100 = **天空之鑰**
3. 走 dialog page 7/8/9 + cutscene 0x3F/0x40 + 特殊 cinematic + dialog page 10

天空之鑰 (item 100) 由 ch21 自動兌換，是 FD2 隱藏機制的核心。對後續章節影響：

- **第 23 章**：若持有天空之鑰，武聖卡里斯加入 (chapter_23_end 內 ny_char_has_item(100) 觸發)。
- **第 27 章**：init 條件 dialog page 3、end handler GOOD/BAD path 分支均依 ny_char_has_item(100) 判定；若 BAD path，悠妮會獨自回到黃金城。

未收齊 6 件則走標準 dialog page 6，希爾法與羅蘭仍會無條件加入但不發給天空之鑰。

## 特殊機制 — 其他

- **失敗條件**：索爾、希爾法 (char[0x10])、羅蘭 (char[0x11]) 任一死亡。
- **共用 init handler**：本章 init 與第 19、20 章共用 (chapter_19_20_21_init_shared)。

## 對話

對話文字 11 pages 來自 FDTXT.DAT entry 21。Pages 7/8/9/10 僅在 6 件物品收齊路徑播放。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『呼！一路都沒休息的趕來，
[PAGE_BREAK]
　真是累死人了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『希望我們還來得及。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『這裡好像是平日的聚會場所
[PAGE_BREAK]
　，不過為何充滿了血腥味‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『啊！前面好像是敵方的大軍
[PAGE_BREAK]
　，精靈族人被圍在中間！‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『不要做白日夢了！我寧可葬
[PAGE_BREAK]
　身於此，也不願作你們的俘
[PAGE_BREAK]
　虜！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0078]
『老頭子，識時務者為俊傑，
[PAGE_BREAK]
　我的雇主很需要像你這樣法
[PAGE_BREAK]
　力高強又見識廣博的大法師
[PARAGRAPH]
　，如果你願意投降，我保證
[PAGE_BREAK]
　他不會虧待你的。呵呵呵‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『少囉唆了！精靈族可不是貪
[PAGE_BREAK]
　生怕死的種族，來一決勝負
[PAGE_BREAK]
　吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0017]
『有我在此，你們休想碰長老
[PAGE_BREAK]
　一根汗毛！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0078]
『既然你們不肯跟我走，我就
[PAGE_BREAK]
　只好把你們的屍體帶回去了
[PAGE_BREAK]
　‥給我殺！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『可惡！又是「死亡骷髏」！
[PAGE_BREAK]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『看來聖靈之塔已經落入敵手
[PAGE_BREAK]
　，剩下的精靈族人正在做最
[PAGE_BREAK]
　後的頑抗‥沒辦法，我們先
[PARAGRAPH]
　保住他們的性命，再作其他
[PAGE_BREAK]
　的打算！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0078]
『又有其他的傻瓜來了嗎？好
[PAGE_BREAK]
　極了，我正嫌這些精靈太少
[PAGE_BREAK]
　，殺起來不過癮呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『瑪‥瑪爾！原來是妳！沒想
[PAGE_BREAK]
　到妳還活著‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0078]
『哦，是禁衛軍的隊長大人啊
[PAGE_BREAK]
　！遺憾的很，「死亡骷髏」
[PAGE_BREAK]
　當年是被你們剿滅了，但我
[PARAGRAPH]
　花了好幾年的時間又把它重
[PAGE_BREAK]
　建起來，現在的「死亡骷髏
[PAGE_BREAK]
　」已非昔日所能比了‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『早知如此，當時就不該好心
[PAGE_BREAK]
　放妳逃走的‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0078]
『是嗎？那時沒有追殺到底是
[PAGE_BREAK]
　你自己的疏忽，也好，我們
[PAGE_BREAK]
　順便清算一下當年的帳‥‥
[PARAGRAPH]
　你們都別想活著離開這裡！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這種為錢賣命的惡棍，何必
[PAGE_BREAK]
　和他們囉唆！我們上！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『唔‥我還不能倒下‥還沒‥
[PAGE_BREAK]
　奪回‥聖靈之塔‥‥』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『‥神啊‥請保佑我們精靈族
[PAGE_BREAK]
　‥戰勝黑暗之力‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0078]
『終於來了嗎！
[PAGE_BREAK]
　宰了這些傢伙！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x001A]
『‥哼，小伙子們還真有一手
[PAGE_BREAK]
　啊‥‥敗在你們手��，我沒
[PAGE_BREAK]
　什麼話好說了‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『瑪爾！‥如果那時妳改過向
[PAGE_BREAK]
　善，就不會有今天的結果！
[PAGE_BREAK]
　‥為什麼‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x001A]
『‥萊汀，我對你說過，我的
[PAGE_BREAK]
　身上流著罪犯的血液，我們
[PAGE_BREAK]
　是注定不能並肩作戰的‥你
[PARAGRAPH]
　以後可以放心了‥‥唔‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『瑪爾‥‥』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x0018]
『我本來以為這回死定了，還
[PAGE_BREAK]
　好你們前來支援，我們才能
[PAGE_BREAK]
　戰勝這個殺手集團，真是非
[PARAGRAPH]
　常感激。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『幸好我們還趕得及，想必這
[PAGE_BREAK]
　是神的旨意。聖靈之塔的情
[PAGE_BREAK]
　形如何？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『三天前就已經被敵人攻入了
[PAGE_BREAK]
　，為了減少無謂的死傷，我
[PAGE_BREAK]
　就命令所有守軍撤退，所以
[PARAGRAPH]
　那裡的情形現在我也不很清
[PAGE_BREAK]
　楚。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我明白了。此外，我先前請
[PAGE_BREAK]
　你進行的調查可有結果？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『我在王城的大圖書館中花了
[PAGE_BREAK]
　三個月研究古代典籍，還在
[PAGE_BREAK]
　各地的遺跡中進行挖掘，結
[PARAGRAPH]
　果只得到了這支法杖和一點
[PAGE_BREAK]
　結論。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『嗯，真是支很奇異的法杖‥
[PAGE_BREAK]
　‥結論怎麼說呢？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『根據古典籍的記載，有關你
[PAGE_BREAK]
　所說的「天空之鑰」，事實
[PAGE_BREAK]
　上是由一個黃金徽章和五顆
[PARAGRAPH]
　魔法寶石所構成的，這六樣
[PAGE_BREAK]
　東西必須經由這法杖的魔力
[PAGE_BREAK]
　才能結合在一起，從而發出
[PARAGRAPH]
　及強大的力量，據說這力量
[PAGE_BREAK]
　可以克服時間和空間的限制
[PAGE_BREAK]
　，將使用者移動到遙遠的未
[PARAGRAPH]
　知地方去。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『很有趣的說法！你的結論呢
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『那有什麼結論！這六件東西
[PAGE_BREAK]
　，早在八百年前就不知道都
[PAGE_BREAK]
　失落到哪裡去了，而當我開
[PARAGRAPH]
　始試圖要尋找它們的蹤跡時
[PAGE_BREAK]
　，黑暗軍就對亞述森林發動
[PAGE_BREAK]
　了攻擊，我只好先中斷了這
[PARAGRAPH]
　段研究。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我一直相信天空之鑰和這一
[PAGE_BREAK]
　連串事端有關，既然你暫時
[PAGE_BREAK]
　無法完成研究，這個揣測也
[PARAGRAPH]
　就只好作罷，我們得換一個
[PAGE_BREAK]
　新的調查方向‥‥』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0018]
『為了避免敵人在塔中玩出什
[PAGE_BREAK]
　麼花樣，我們還是先消滅掉
[PAGE_BREAK]
　塔中的敵人再說，以免夜長
[PARAGRAPH]
　夢多。羅蘭，為了增強戰力
[PAGE_BREAK]
　起見，妳也和我一起去吧！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0017]
『是的，長老！善後工作交給
[PAGE_BREAK]
　其他人就可以了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『好，那我們即刻前往巨塔！
[PAGE_BREAK]
　事關緊要，大家稍事休息就
[PAGE_BREAK]
　馬上出發！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒問題！
[PAGE_BREAK]
　我們這就上路吧！』
[END]
```

### Page 7

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『等等！希爾法大師，你所說
[PAGE_BREAK]
　的徽章和寶石，是不是這些
[PAGE_BREAK]
　？‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0018]
『啊！蘊含光﹑闇、火焰﹑冰
[PAGE_BREAK]
　和星星之力的五顆寶石‥‥
[PAGE_BREAK]
　就是這個沒錯！你們怎麼找
[PARAGRAPH]
　到這些東西的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『路上逐一得來的，敵人似乎
[PAGE_BREAK]
　也對它們感興趣，所以其中
[PAGE_BREAK]
　幾個費了好大勁才到手。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『好極了，我現在就試試這支
[PAGE_BREAK]
　法杖管不管用。大家讓開一
[PAGE_BREAK]
　些，這可能會有些危險‥‥
[PARAGRAPH]
　』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_ID=0x0018]
『未知的遠古神祇啊，履行契
[PAGE_BREAK]
　約的時刻到了，請給予這法
[PAGE_BREAK]
　杖以失落的力量，讓被遺忘
[PARAGRAPH]
　的道路再次開啟‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『馬那德拉、亞布利加﹑艾米
[PAGE_BREAK]
　西德拉、伊多卡那瓦！‥‥
[PAGE_BREAK]
　』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『瞧，完成了！相信這就是
[PAGE_BREAK]
　你一直在找的「天空之鑰
[PAGE_BREAK]
　」！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『真令人不敢相信，我花了十
[PAGE_BREAK]
　幾年尋找的東西，竟然就這
[PAGE_BREAK]
　樣到手了，這是命運的安排
[PARAGRAPH]
　吧‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『約拿先生，這個「天空之鑰
[PAGE_BREAK]
　」有什麼特別的用途？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我只知道它與古代人的傳說
[PAGE_BREAK]
　有關，據說它可以打開往天
[PAGE_BREAK]
　空的道路，至於是用怎樣的
[PARAGRAPH]
　方式，就沒有人知道了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哎，好失望，我還以為這下
[PAGE_BREAK]
　子可以飛上天空了呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『索爾，別急，我相信我們真
[PAGE_BREAK]
　正的敵人必定與這天空之鑰
[PAGE_BREAK]
　有關，我們遲早可以解開這
[PARAGRAPH]
　個秘密‥以及悠妮小姐的來
[PAGE_BREAK]
　歷之謎。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『真的嗎？‥那我就放心了。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『雖然如此，為了避免敵人在
[PAGE_BREAK]
　塔中玩出什麼花樣，我們還
[PAGE_BREAK]
　是馬上動身前往巨塔，其他
[PARAGRAPH]
　的事以後再說！羅蘭，為了
[PAGE_BREAK]
　增強戰力起見，妳也和我一
[PAGE_BREAK]
　起去吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0017]
『是的，長老！善後工作交給
[PAGE_BREAK]
　其他人就可以了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好！我們這就出發囉！』
[END]
```
