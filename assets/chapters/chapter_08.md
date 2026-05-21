# 第 8 章 — 王城前的戰鬥

抵達王城前遭駐軍誤判為亂黨圍攻，希莉亞亮出公主身份；騎士洛娜拒抗王命投靠；女神之淚首飾首次登場。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 5 | 洛娜 | End handler 末段 | 章末固定加入 |

## 敵人配置

寫在 FDFIELD.DAT entry 22。城門為主戰場，配合 turn 2-7 每回合 2 名敵騎兵的
reinforcement event。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。

## 商店

無特別記載（攻略本未提此章神秘商店）。

## 特殊機制

- **失敗條件**：索爾死亡（default handler）。
- **Turn-based reinforcement**：城門守軍由 FDFIELD event `0x1B` 在 turn 2-7 的 enemy_turn_intro
  各 fire 一次，總共 6 波（每波 2 名敵騎兵）。
- **章末加入**：洛娜 (char 5) 由 end handler `fd2_init_runtime_char_from_base_growth(5)` 加入。
- **Fade-to-black 轉場**：end handler 末段執行 `fd2_set_vga_palette_range(0, 0xFF, 0x40)` palette darken
  + `memset(0xA0000, 0, 64000)` framebuffer clear，作章末黑屏淡出效果。
- **跨章劇情**：希莉亞=亞克斯王國公主身份揭曉；女神之淚首飾為信物道具（後續章節有相關劇情）。

## 對話

對話文字 5 pages 來自 FDTXT.DAT entry 8。Init 引用 page 0/1，End 引用 page 3/4。

### Page 0

```text
[PORTRAIT_LEFT_BY_ID=0x000D]
『看！城堡就在眼前了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『可是，我們一路上居然都沒
[PAGE_BREAK]
　遇到敵軍，莫非‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『你說對啦，他們都在這裡等
[PAGE_BREAK]
　我們呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『大膽的盜匪，不管你們有多
[PAGE_BREAK]
　厲害，這裡就是你們的葬身
[PAGE_BREAK]
　之處了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『等等！大家聽我說，不要再
[PAGE_BREAK]
　打了！我們不是敵人‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『你們以為這種話我會相信？
[PAGE_BREAK]
　你們當我是白癡嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『無禮者！好吧，你瞧瞧這是
[PAGE_BREAK]
　什麼？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『咦！這是‥這是歷代王后所
[PAGE_BREAK]
　擁有的「女神之淚」首飾！
[PAGE_BREAK]
　這首飾應該是在公主的手中
[PARAGRAPH]
　，難道‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『沒錯，我就是亞克斯王國的
[PAGE_BREAK]
　公主希莉亞，我以公主之名
[PAGE_BREAK]
　命令你們立刻收起武器！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『可‥可是‥殿下您們攻擊我
[PAGE_BREAK]
　軍的探查隊與士兵，此事證
[PAGE_BREAK]
　據確鑿，為何殿下您會‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『我們和他們間發生了點誤會
[PAGE_BREAK]
　這些士兵極為蠻橫，不肯聽
[PAGE_BREAK]
　我說明，只好先把他們打發
[PARAGRAPH]
　掉啦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0005]
『隊長，這的確是公主殿下沒
[PAGE_BREAK]
　錯！我們先迎接公主進城，
[PAGE_BREAK]
　餘事以後再說‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『這‥這怎麼行！陛下親自吩
[PAGE_BREAK]
　咐，要我把這些亂黨消滅，
[PAGE_BREAK]
　我若沒有做到，就是違抗王
[PARAGRAPH]
　命！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0005]
『可是‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『不用多說了！誰敢違抗王命
[PAGE_BREAK]
　，一律殺無赦！弟兄們上啊
[PAGE_BREAK]
　！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0005]
『公主，妳放心！我身為史卡
[PAGE_BREAK]
　迪家的人，一定會捨命保護
[PAGE_BREAK]
　妳的！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『洛娜，妳竟敢違逆我的命令
[PAGE_BREAK]
　！妳也準備死吧！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0053]
『公主殿下‥‥屬下也是遵命
[PAGE_BREAK]
　行事，冒犯之處，請您原諒
[PAGE_BREAK]
　‥‥啊‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_RIGHT_BY_ID=0x0008]
『唉！事情為什麼會變成這樣
[PAGE_BREAK]
　‥‥我得去見父王一面，
[PAGE_BREAK]
　把事情弄個清楚才行。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0005]
『公主殿下，現在宮中情勢混
[PAGE_BREAK]
　亂，請容屬下隨行，以保護
[PAGE_BREAK]
　殿下的安全。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『剛才妳說妳是史卡迪家的人
[PAGE_BREAK]
　‥‥可是指騎士世家的史卡
[PAGE_BREAK]
　迪家族？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0005]
『是的！我史卡迪家的族人世
[PAGE_BREAK]
　代都擔負保護王族的使命，
[PAGE_BREAK]
　如今雖然我家族只剩下我一
[PARAGRAPH]
　個人，屬下仍會盡忠職守，
[PAGE_BREAK]
　誓以性命保護殿下的安全。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『我明白了，妳就跟我來吧‥
[PAGE_BREAK]
　‥索爾，亞雷斯，以後我還
[PAGE_BREAK]
　要借重你們的力量，你們也
[PARAGRAPH]
　和我一起進城吧！』
[END]
```

### Page 4

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『希莉亞‥‥是王國公主？
[PAGE_BREAK]
　那悠妮的身份又是怎麼
[PAGE_BREAK]
　一回事？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『很簡單，是我們搞錯啦。
[PAGE_BREAK]
　現在該怎麼辦？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『事到如今，也只有跟著希莉
[PAGE_BREAK]
　亞進去看看了。走吧！』
[END]
```
