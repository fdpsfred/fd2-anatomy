# 第 20 章 — 死亡般的沈寂

死之沼澤遭遇隱形怪物與「死亡骷髏」夾擊，救援精靈族並招募忍者謝多；速戰可額外收下惡魔族戰士達可塞。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 25 (0x19) | 忍者謝多 | 章末 | 無條件 |
| 28 (0x1C) | 魔族達可塞 | 章末 | 15 回合內結束戰鬥 (save_metadata_block < 16，即 TURN 1..15) |

## 敵人配置

沼澤怪物 + 「死亡骷髏」傭兵 + 多群敵兵；精靈族 8 名 (chars[0x35..0x3D]) 為友軍 NPC 自走。配置寫在 FDFIELD.DAT entry 58。

## 寶物

待 FDFIELD.DAT entry 58 確認。

## 商店

無章內商店。

## 特殊機制

- **勝利條件**：擊敗沼澤怪物之外的全部敵人 (post_action Stage C 兩組敵 chars[0x24..0x33] + chars[0x3D..0x53] 全死)。
- **失敗條件**：
  - 索爾 (char[0]) 死亡
  - 忍者謝多 (char[0x34]) 死亡
  - 精靈族 8 名 (chars[0x35..0x3D]) 全滅，會額外播 page 10「糟糕，精靈們被全滅了！」
- **達可塞限時招募**：15 回合內結束戰鬥才觸發 cutscene 0x3C/0x3D/0x3E + dialog page 0xE/0xF/0x10 並讓達可塞加入。
- **共用 init handler**：本章 init 與第 19、21 章共用 (chapter_19_20_21_init_shared)。

## 對話

對話文字 17 pages 來自 FDTXT.DAT entry 20。Pages 12/13/14/15/16 為章末選擇性互動 (達可塞登場、悠妮溝通) 對話。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『咦！一陣子沒來這裡，沼澤
[PAGE_BREAK]
　的面積好像又變大了‥這裡
[PAGE_BREAK]
　本來應該還是森林的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『沼澤會吞食森林？這種事我
[PAGE_BREAK]
　可沒聽過。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000B]
『這個死之沼澤是全大陸邪氣
[PAGE_BREAK]
　最重的地方，什麼事情都有
[PAGE_BREAK]
　可能發生，大家要特別小心
[PARAGRAPH]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『安靜！我聽到前面有打鬥聲
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0019]
『可惡，看來傳聞果然是真的
[PAGE_BREAK]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0036]
『我還是不能相信，這世界上
[PAGE_BREAK]
　居然真有看不見的怪物！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0038]
『你才剛被咬了一口而已，接
[PAGE_BREAK]
　受事實吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0063]
『嘿，認命吧！誰教你們誤入
[PAGE_BREAK]
　這片沼澤！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0064]
『首領有命，凡是進入這片沼
[PAGE_BREAK]
　澤的都得死！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0038]
『謝多，我們現在該怎麼辦？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0019]
『‥‥跟他們拼了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0063]
『那你們就受死吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『從他們的話聽來，好像是遇
[PAGE_BREAK]
　到了看不見的隱形怪物呢！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『有這種事？這在我們度過此
[PAGE_BREAK]
　地時可是一大威脅，嗯‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『不如我們現在就出去和他們
[PAGE_BREAK]
　攜手作戰，強行越過這沼澤
[PAGE_BREAK]
　，同時也摸清楚這隱形怪物
[PARAGRAPH]
　的底細。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『我贊成！坐視無辜旅人被敵
[PAGE_BREAK]
　方攻擊，這可不是我們的一
[PAGE_BREAK]
　貫作風！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『既然如此，我們就上陣吧！
[PAGE_BREAK]
　不過對手中有看不見的傢伙
[PAGE_BREAK]
　，大家要謹慎些！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0034]
『‥‥我不甘心‥我不該輸給
[PAGE_BREAK]
　這些傢伙的‥‥』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0035]
『哇‥‥啊‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0036]
『聖靈之塔‥‥絕不能讓敵人
[PAGE_BREAK]
　侵入‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x0037]
『保‥護‥‥聖靈之塔‥‥』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_CHAR=0x0038]
『哇‥‥啊‥‥』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_CHAR=0x0039]
『聖靈之塔‥‥絕不能讓敵人
[PAGE_BREAK]
　侵入‥‥』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_CHAR=0x003A]
『哇‥‥啊‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_CHAR=0x003B]
『保‥護‥‥聖靈之塔‥‥』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_CHAR=0x003C]
『啊‥‥』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『糟糕，精靈們被全滅了！』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這雖是一場苦戰，但感謝神
[PAGE_BREAK]
　庇佑我們獲得最後的勝利。
[PAGE_BREAK]
　對了，你們是怎麼碰上這件
[PARAGRAPH]
　怪事的？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0019]
『我們在行經此地時，忽然就
[PAGE_BREAK]
　遭到這些傢伙和隱形的沼澤
[PAGE_BREAK]
　怪物攻擊，幸好各位趕來援
[PARAGRAPH]
　助，不然我們大概已經不明
[PAGE_BREAK]
　不白的死在這裡了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001B]
『為何那些傢伙要封鎖死之沼
[PAGE_BREAK]
　澤？想要阻止我們前往北方
[PAGE_BREAK]
　嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0048]
『可能和來路不明的黑暗軍進
[PAGE_BREAK]
　攻「聖靈之塔」一事有關‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『什麼？真有此事？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0048]
『是的，我們這一支精靈族住
[PAGE_BREAK]
　在亞述森林，世代負責守護
[PAGE_BREAK]
　聖地。最近集結的黑暗軍似
[PARAGRAPH]
　想進攻聖靈之塔，長老深恐
[PAGE_BREAK]
　我們的兵力抵敵不住，所以
[PAGE_BREAK]
　派遣我們向哈斯米爾求救，
[PARAGRAPH]
　沒想到敵方已在此布下陷阱
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『聖靈之塔究竟有什麼秘密，
[PAGE_BREAK]
　值得那些傢伙拼命進攻？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0048]
『我們也不知道，因為那裡已
[PAGE_BREAK]
　有數百年沒人進去過了‥‥
[PAGE_BREAK]
　不過長老深信黑暗軍一旦攻
[PARAGRAPH]
　佔聖靈之塔，後果一定不堪
[PAGE_BREAK]
　設想，所以嚴令我們務必死
[PAGE_BREAK]
　守。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『原來如此。哈斯米爾也遭到
[PAGE_BREAK]
　敵方攻擊而死傷慘重，救兵
[PAGE_BREAK]
　可以不必去求了，我們這就
[PARAGRAPH]
　去亞述森林增援。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0048]
『謝謝你們！這樣我們就可以
[PAGE_BREAK]
　放心的回去覆命了！』
[END]
```

### Page 12

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『嗯，看來你似乎是個忍者，
[PAGE_BREAK]
　這個職業現在已經不多見了
[PAGE_BREAK]
　‥‥你往後有何打算？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0019]
『我在這森林中隱居多年，這
[PAGE_BREAK]
　些年來的和平生活，讓我以
[PAGE_BREAK]
　為這一身技藝已經沒有用了
[PARAGRAPH]
　，現在我發現這種想法好像
[PAGE_BREAK]
　是錯了‥如果你們不介意的
[PAGE_BREAK]
　話，我願意加入你們，與黑
[PARAGRAPH]
　暗軍周旋到底！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『要與黑暗軍對陣，人手再多
[PAGE_BREAK]
　也不會嫌夠的。歡迎加入！
[PAGE_BREAK]
　』
[END]
```

### Page 13

```text
[PORTRAIT_LEFT_BY_ID=0x0015]
『遺憾的是，現在沒時間歡迎
[PAGE_BREAK]
　新人了，事態緊急，我們得
[PAGE_BREAK]
　立刻趕往亞述森林。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒問題！我們上路囉！』
[END]
```

### Page 14

```text
[PORTRAIT_LEFT_BY_ID=0x001C]
『嘎呼吱！希哩呼喀嚕‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001B]
『這不是下等魔族嗎？怎麼會
[PAGE_BREAK]
　在此出現？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『看起來好像沒有敵意的樣子
[PAGE_BREAK]
　，只可惜我不懂魔族的語言
[PAGE_BREAK]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『你說什麼？我聽不清楚，稍
[PAGE_BREAK]
　等一下。』
[END]
```

### Page 15

```text
[PORTRAIT_LEFT_BY_ID=0x001C]
『嘎呼吱！希哩呼喀嚕‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『嗯，嗯‥‥我明白了，我會
[PAGE_BREAK]
　向他們替你求情的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，妳懂得它們的話？』
[END]
```

### Page 16

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『是的，我聽得懂牠在說些什
[PAGE_BREAK]
　麼，不過我不知道為何我聽
[PAGE_BREAK]
　得懂‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『沒關係，悠妮小姐，牠和你
[PAGE_BREAK]
　說了些什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『他說他叫達克塞，是下等的
[PAGE_BREAK]
　魔族戰士，因為對首領的命
[PAGE_BREAK]
　令產生了懷疑，而被處以監
[PARAGRAPH]
　禁的刑罰，原先是由「死亡
[PAGE_BREAK]
　骷髏」所看管，後來我們打
[PAGE_BREAK]
　倒了負責防守死之沼澤的敵
[PARAGRAPH]
　軍，他才乘亂逃了出來。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『惡魔族也牽涉其中嗎？這比
[PAGE_BREAK]
　我想像的還麻煩‥‥那麼，
[PAGE_BREAK]
　這位達克塞先生對我們有何
[PARAGRAPH]
　指教呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『他說魔族已遭人以某種方式
[PAGE_BREAK]
　操縱，他希望能加入我們一
[PAGE_BREAK]
　行，拯救惡魔族免於毀滅的
[PARAGRAPH]
　命運。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001B]
『惡魔族能夠信任嗎？何況又
[PAGE_BREAK]
　是下等的惡魔‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒關係！只要有心對抗黑暗
[PAGE_BREAK]
　軍者，都可以加入我們一行
[PAGE_BREAK]
　！我們要先能相信別人，才
[PARAGRAPH]
　能得到別人的信任。不是嗎
[PAGE_BREAK]
　？』
[END]
```
