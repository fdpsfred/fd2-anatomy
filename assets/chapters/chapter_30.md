# 第 30 章 — 傳說的終章－結局

擊敗復生的四魔神（地、水、風、火）與最終 boss 空魔神 (ASR-06)；悠妮選擇與要塞一同消失於超空間，索爾等人被傳送回馬拉大陸，遊戲進入 GOOD ENDING / staff roll。

## 加入角色

最終章不加入新角色。

## 敵人配置

空魔神 (chars[0x14], char_id 0x7E, data_fd2_battle_enemy_data_table entry 58 @ `0x7AD51`)、水魔神 (id 0x7B)、地魔神 (id 0x7A)、風魔神 (id 0x7C)、火魔神 (id 0x7D)、機甲 — FDFIELD.DAT[30]。
init 階段以 7× cinematic warp 將魔神群傳送進場（4 個上方 + 3 個下方）。

## 寶物

最終章無寶箱。

## 商店

無。

## 特殊機制

- **Init 7× cinematic warp 進場**：`cinematic_warp_char_to_tile` 把 7 個魔神 / boss 從不同方向傳送到戰場（4 個上方 group：char 0x15/0x16/0x17/0x18；3 個下方 group：char 0x18/0x19/0x1A），中間穿插 `animate_palette_flash_pulse_white` 全螢幕白光閃爍。
- **GOOD ENDING 路徑**：擊殺空魔神 (chars[0x14]) → post_action 設 `game_event_flag = 2` → end handler 執行：
  - cast spell visual + palette fade
  - 推進 `current_chapter_id` 到 31 (out-of-range)
  - `load_chapter_battle_data(31)` 載入 epilogue map (FDFIELD.DAT entry 30, 0-indexed)
  - 64-step palette fade-in + 40 frame composite
  - 引用 epilogue dialog page 0/1（屬「chapter 31」FDTXT entry，包含悠妮的真相說明與道別）
  - `play_game_ending_cinematic` 觸發 staff roll
  - infinite loop 結束於此
- **Speedrun 支援**：post_action 對 chars[0x14] 死亡判定不依賴回合數，任何 turn 殺空魔神都直接觸發 win 路徑（攻略「以下為在第一回合便殺掉空魔神的方式」對應此設計）。
- **勝負條件**：
  - 勝：空魔神 (chars[0x14]) 死
  - 負：索爾 (chars[0]) 死
  - 負：悠妮 (chars[1]) 死 → 顯示 page 7 「‥我不能就這樣倒下‥控制系統‥索爾‥‥」
- **跨章天空之鑰兌換鏈終點**：本章 (GOOD ENDING) 為 ch21 → ch23 → ch27 → ch28-30 兌換鏈的最終結果。

## 對話

對話文字 11 pages 來自 FDTXT.DAT entry 30。Init 引用 page 0/1/2，Post-action（悠妮死）引用 page 7，End 引用 page 9/10（chapter 30 text）後切到 chapter 31 epilogue text 的 page 0/1。Page 3-6/8 由 FDFIELD turn-event handler 引用（魔神領命對白、空魔神終章對白）。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『唔‥我們還活著嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『好強烈的震動！不過我沒有
[PAGE_BREAK]
　感覺到什麼異狀‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『嘿，這是怎麼回事？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『哼，我們的命真大‥看來次
[PAGE_BREAK]
　元反應爐並未完全失效，雖
[PAGE_BREAK]
　然超荷的磁界能量爆發造成
[PARAGRAPH]
　了部份的空間扭曲，不過這
[PAGE_BREAK]
　應該是暫時性的，影響並不
[PAGE_BREAK]
　很大。哼哼哼‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『ASR一06，我們才剛撿回一條
[PAGE_BREAK]
　命難道你還‥？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『ASR一07，我偉大的計畫差點
[PAGE_BREAK]
　毀在你手��，這次我不會再
[PAGE_BREAK]
　重蹈覆轍了，我要先把你和
[PARAGRAPH]
　這幫螻蟻徹底消滅掉，再重
[PAGE_BREAK]
　新開始執行計畫‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『欠揍的傢伙，到現在你居然
[PAGE_BREAK]
　還不死心，你真的把我給搞
[PAGE_BREAK]
　火了‥‥今天非宰了你不可
[PARAGRAPH]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『來吧！伙伴們，我們一起消
[PAGE_BREAK]
　滅這個萬惡的原凶！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『咈咈咈‥‥‥
[PAGE_BREAK]
　一群不知死活的傢伙。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『都出來吧！』
[END]
```

### Page 1

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『咦！這些傢伙不是早就被我
[PAGE_BREAK]
　們擺平了？怎麼又‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『ASR一07，你不在的這段時間
[PAGE_BREAK]
　我致力於發展生化合成人，
[PAGE_BREAK]
　這四個魔神就是我的得意傑
[PARAGRAPH]
　作！它們不過我製造出來的
[PAGE_BREAK]
　生命體，所以損失了隨時可
[PAGE_BREAK]
　以再造‥這一批是我最新的
[PARAGRAPH]
　改良版本，絕對不會再輸給
[PAGE_BREAK]
　你們！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥哼，等一下！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『你還有什麼遺言嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『身為這個要塞的兩大中樞，
[PAGE_BREAK]
　我們本來應該協力共事的，
[PAGE_BREAK]
　很遺憾我們今天必須傾力對
[PARAGRAPH]
　決‥‥所以我也不需再隱藏
[PAGE_BREAK]
　了，就讓你看看第一空中要
[PAGE_BREAK]
　塞控制暨管理中樞真正的力
[PARAGRAPH]
　量吧！』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『咦，上一場仗中身上受的傷
[PAGE_BREAK]
　都痊癒了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『嗯，我覺得身上重新充滿了
[PAGE_BREAK]
　力量！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『哼哼‥想不到你還有這種能
[PAGE_BREAK]
　力，這樣不過能讓這些螻蟻
[PAGE_BREAK]
　撐久一點而已‥勝利仍將會
[PARAGRAPH]
　屬於我！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『少囉唆了，要打架就過來啊
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『大地的代言者，讓這些愚者
[PAGE_BREAK]
　知道創造者的威嚴！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007A]
『地魔神領命！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x007E]
『水與冰的女王，將這些傢伙
[PAGE_BREAK]
　冰封在永恆的死之牢獄中！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007B]
『水魔神領命！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x007E]
『狂風的吟唱者，用你的利刃
[PAGE_BREAK]
　將他們撕裂成血肉的飛霧！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007C]
『風魔神領命！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x007E]
『烈焰的主宰，燃起你的地獄
[PAGE_BREAK]
　之火，把他們燒得連灰都不
[PAGE_BREAK]
　剩！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007D]
『火魔神領命！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x007E]
『遺憾得很，不識抬舉的你們
[PAGE_BREAK]
　竟然選擇接受終末的天罰‥
[PAGE_BREAK]
　膽敢違抗神的意志的螻蟻們
[PAGE_BREAK]
　迎接你們的最後一刻吧！』
[END]
```

### Page 7

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥我不能就這樣倒下‥控制
[PAGE_BREAK]
　系統‥索爾‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x007E]
『這怎麼可能‥我是至高無上
[PAGE_BREAK]
　的創造者和支配者‥竟然會
[PAGE_BREAK]
　被這些卑微的螻蟻給打倒‥
[PARAGRAPH]
　‥不‥‥』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_ID=0x0009]
『總算結束了‥‥這個很久以
[PAGE_BREAK]
　前就鑄成的錯誤，終於就此
[PAGE_BREAK]
　消失在時間的洪流之中了‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『‥我們終於打贏了這場仗！
[PAGE_BREAK]
　悠妮，真是太好了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『很高興我們終於解決了這件
[PAGE_BREAK]
　事情‥‥不過，身為一個學
[PAGE_BREAK]
　者，我總想知道這一切的來
[PARAGRAPH]
　龍去脈‥悠妮小姐，妳能給
[PAGE_BREAK]
　我們一個說明嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『‥‥‥我就簡短的說明吧。
[PAGE_BREAK]
　這個空中要塞，以及我和空
[PAGE_BREAK]
　魔神，都是古代人所製造的
[PARAGRAPH]
　惡魔族也是古代人發展出來
[PAGE_BREAK]
　的戰鬥用生物。‥‥那時古
[PAGE_BREAK]
　代人正在互相爭戰，為了獲
[PARAGRAPH]
　取勝利，雙方製造出無數的
[PAGE_BREAK]
　恐怖武器，這個要塞就是其
[PAGE_BREAK]
　中一個‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『自古流傳的「審判之日」戰
[PAGE_BREAK]
　爭嗎？據說那場戰爭毀滅了
[PAGE_BREAK]
　天空與大地，直到數萬年後
[PARAGRAPH]
　萬物才再次復興‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『是的。雙方使用最終武器互
[PAGE_BREAK]
　相攻擊，最後只有少數古代
[PAGE_BREAK]
　人的領袖乘著飛行器離開了
[PARAGRAPH]
　這個星球，其他的都死在這
[PAGE_BREAK]
　場戰爭中‥‥這個要塞是為
[PAGE_BREAK]
　了對地壓制攻擊而建造的，
[PARAGRAPH]
　我和空魔神是負責控制這要
[PAGE_BREAK]
　塞的中樞，我控制和維持要
[PAGE_BREAK]
　塞的運行，空魔神一一一一不，
[PARAGRAPH]
　ASR一06則負責對地面的監視
[PAGE_BREAK]
　和武器的使用，換言之，這
[PAGE_BREAK]
　個要塞是由我和空魔神所一
[PARAGRAPH]
　起指揮的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『真令人難以想像‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『我們都以人的肉體接受過特
[PAGE_BREAK]
　殊的再造和強化手術，能夠
[PAGE_BREAK]
　直接以意志和系統連接並傳
[PARAGRAPH]
　達各項命令，而且能以休眠
[PAGE_BREAK]
　的方式來防止肉體上的衰老
[PAGE_BREAK]
　所以，在理論上我們可以永
[PARAGRAPH]
　生不死，永遠擔任空中要塞
[PAGE_BREAK]
　的控制工作。』
[END]
```

### Page 10

```text
[PORTRAIT_LEFT_BY_ID=0x0009]
『就在殘餘的古代人要撤離此
[PAGE_BREAK]
　地的時候，我們收到攻擊指
[PAGE_BREAK]
　令，命令我們用最終兵器「
[PARAGRAPH]
　黑暗之焰」轟炸地表。可是
[PAGE_BREAK]
　就在執行命令的前一刻，卻
[PAGE_BREAK]
　又收到模糊的訊號，命令我
[PARAGRAPH]
　們中止攻擊。之後我們再也
[PAGE_BREAK]
　沒有收到過任何訊號，我們
[PAGE_BREAK]
　就這樣等待著永遠也不會再
[PARAGRAPH]
　有的戰鬥命令，和這座孤伶
[PAGE_BREAK]
　伶的要塞在天空中飄盪了三
[PAGE_BREAK]
　萬年的時光。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『三‥三萬年！妳不會覺得很
[PAGE_BREAK]
　寂寞嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『不。我們都處在休眠狀態，
[PAGE_BREAK]
　有狀況通訊系統會自動叫醒
[PAGE_BREAK]
　我們，所以三萬年的時間並
[PARAGRAPH]
　不算久。我們一直沈睡到不
[PAGE_BREAK]
　久之前‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『原本處在沈睡狀況下的要塞
[PAGE_BREAK]
　為何會再次甦醒？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『要塞發生了一次意外，安全
[PAGE_BREAK]
　系統誤報要塞遭受攻擊‥‥
[PAGE_BREAK]
　ASR一06認為那是來自地面上
[PARAGRAPH]
　的攻擊，堅持要繼續以前的
[PAGE_BREAK]
　攻擊命令，我和他發生很大
[PAGE_BREAK]
　的爭執。由於我醒來後曾對
[PARAGRAPH]
　地表做過觀察，發現陸地上
[PAGE_BREAK]
　已再次出現生命和文明，所
[PAGE_BREAK]
　以就把攻擊電腦所不可或缺
[PARAGRAPH]
　的單元計算晶片拿出來，從
[PAGE_BREAK]
　要塞上丟了下去。三萬年前
[PAGE_BREAK]
　的戰爭已經夠了，我不想看
[PARAGRAPH]
　到再一次的慘劇‥‥‥‥‥
[PAGE_BREAK]
　結果ASR一06發現了我的意圖
[PAGE_BREAK]
　在爭奪晶片的途中，我失足
[PARAGRAPH]
　跌了下去‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『結果‥妳就掉到羅特帝亞來
[PAGE_BREAK]
　了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『‥剛醒來的時候，我的系統
[PAGE_BREAK]
　受到嚴重衝擊，部份記憶無
[PAGE_BREAK]
　法存取，所以我忘記了以往
[PARAGRAPH]
　的一切。那時無心的說出想
[PAGE_BREAK]
　回家的話，索爾你們才和我
[PAGE_BREAK]
　一路來到這裡，‥這是命運
[PARAGRAPH]
　之神的安排吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『真是令人難以置信‥‥‥‥
[PAGE_BREAK]
　我想，這一切都是神的旨意
[PAGE_BREAK]
　現在我們已打倒了最後的敵
[PARAGRAPH]
　人，悠妮小姐，往後‥‥‥
[PAGE_BREAK]
　妳打算怎麼辦呢？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『我會用空間轉送裝置送你們
[PAGE_BREAK]
　回馬拉大陸，然後我會把要
[PAGE_BREAK]
　塞帶到超空間中，讓它永遠
[PARAGRAPH]
　永遠的從這個世界上消失‥
[PAGE_BREAK]
　一切就讓它就此結束吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，妳不和我們一起走嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『不。我本來就是這個要塞的
[PAGE_BREAK]
　一部份，而為了保證這種事
[PAGE_BREAK]
　不會再發生，我也必須待在
[PARAGRAPH]
　這個要塞上‥我想這是和大
[PAGE_BREAK]
　家說再見的時刻了，這些日
[PAGE_BREAK]
　子來的冒險與種種回憶‥‥
[PARAGRAPH]
　我會永遠記在心裡‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！
[PAGE_BREAK]
　妳真的要留在這兒？‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『索爾，我真的很感激你，你
[PAGE_BREAK]
　已經實現了諾言，將我送回
[PAGE_BREAK]
　家了。你對我來說是很特別
[PARAGRAPH]
　的，在記憶甦醒的剎那，我
[PAGE_BREAK]
　為了自己將選擇哪一方而迷
[PAGE_BREAK]
　亂，然而‥是你讓我做了最
[PARAGRAPH]
　後的決定。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『‥不能為了我‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『索爾，我瞭解你的心情，但
[PAGE_BREAK]
　是‥我並不是一般的女子，
[PAGE_BREAK]
　也不屬於這個新的世界‥‥
[PARAGRAPH]
　就讓我留在這裡吧，這也是
[PAGE_BREAK]
　我的職責‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『索爾，你要諒解悠妮的立場
[PAGE_BREAK]
　而且，在將來的歲月中，你
[PAGE_BREAK]
　也一定會再遇到適合你的女
[PARAGRAPH]
　孩子，更何況你身為羅特帝
[PAGE_BREAK]
　亞的王子，有美好的將來和
[PAGE_BREAK]
　前程，別辜負了悠妮的心意
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『‥‥‥‥我明白了。
[PAGE_BREAK]
　悠妮，妳要多保重‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『謝謝‥‥‥我會永遠永遠記
[PAGE_BREAK]
　得你的，索爾‥在我的生命
[PAGE_BREAK]
　中‥‥』
[END]
```

### Epilogue (chapter 31 entry, page 0)

End handler 推進 `current_chapter_id` 到 31 後 `load_chapter_battle_data(31)` 載入 chapter 31 dialog entry，引用其 page 0 與 page 1（內容為 staff roll 前的最終道別場景，未在 FDTXT entry 30 範圍內）。
