# 第 7 章 — 往王城的途中

赴王城途中又被王國軍當作匪徒圍攻；雙重條件滿足可招募凱麗。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 12 | 凱麗 | End handler | 須觸發 tile event 0x11 **且** char[43] 戰鬥中存活 |

## 敵人配置

寫在 FDFIELD.DAT entry 19。具體配置請參照該檔案。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。

## 商店

神秘商店 (Alt+F6) — 聖者之戒/心眼之書/白金勳章/飛龍卵，hack table 觸發。

## 特殊機制

- **失敗條件**：索爾死亡（default handler）。
- **雙重條件招募**：凱麗 (char 12) 必須同時滿足
  (a) 戰鬥中觸發 `tile_event_consumed_flags[0x11]` 對應的 tile event
  (b) char[43] (凱麗本人) 存活到結算
  才會在 end handler 加入；否則跳過 recruit 顯示 page 5。

## 對話

對話文字 6 pages 來自 FDTXT.DAT entry 7。Init 引用 page 0/1，End handler 依條件分支
引用 page 4 或 page 5。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x000D]
『這條路是通往王城的要道，
[PAGE_BREAK]
　雖然走這條路較快，但遇到
[PAGE_BREAK]
　王國守備軍的機會也較高‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『你說的沒錯，我已經看到了
[PAGE_BREAK]
　。』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0053]
『前面那批人一定就是在普里
[PAGE_BREAK]
　茲港逞兇的匪徒，通通給我
[PAGE_BREAK]
　拿下！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『等等，大家都是自己人，
[PAGE_BREAK]
　這是一場誤會‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『少玩弄詭計想拖延時間了。
[PAGE_BREAK]
　乖乖的受死吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『唉，說不通，
[PAGE_BREAK]
　只好先打再說了。
[PAGE_BREAK]
　索爾，我們上！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『啊？我沒問題，
[PAGE_BREAK]
　妳自己可要小心點，
[PAGE_BREAK]
　不要太勉強！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『索爾，謝謝你！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x000C]
『哎呀！一大群男人追殺一個
[PAGE_BREAK]
　小姑娘，不覺得丟臉嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0022]
『妳的一拳讓我的部下得療養
[PAGE_BREAK]
　三個月，無故毆打王國駐軍
[PAGE_BREAK]
　可是重罪，乖乖的跟我回去
[PARAGRAPH]
　說個明白吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000C]
『誰教他對我亂瞄！你們王國
[PAGE_BREAK]
　軍都是這個樣子的嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0022]
『哼‥‥不和妳吵了，
[PAGE_BREAK]
　給我抓起來！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『咦！那邊有王國軍在圍攻一
[PAGE_BREAK]
　個小女孩，這是怎麼一回
[PAGE_BREAK]
　事？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『喂，我們自己都顧不了了，
[PAGE_BREAK]
　不要再去湊熱鬧‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『什麼！你能眼睜睜的看著一
[PAGE_BREAK]
　群王國軍欺負一個小女孩？
[PAGE_BREAK]
　我自己去收拾他們好了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『好啦好啦，
[PAGE_BREAK]
　我陪你去就是了，
[PAGE_BREAK]
　反正不差這幾個‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『好吧，那就上吧！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0021]
『可惡‥國王陛下，我不能再
[PAGE_BREAK]
　護衛您了‥請原諒屬下的
[PAGE_BREAK]
　無能‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『你弄錯了，我們不是來加害
[PAGE_BREAK]
　國王的！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0021]
『是嗎？‥那麼‥前些日子到
[PAGE_BREAK]
　底是誰‥‥啊‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x000C]
『多謝你們的幫忙！連王國的
[PAGE_BREAK]
　精銳部隊也不是你們對手，
[PAGE_BREAK]
　你們到底是什麼來歷？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這個說起來很麻煩，此地又
[PAGE_BREAK]
　很危險，我看妳還是回家睡
[PAGE_BREAK]
　覺好了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000C]
『什麼，你竟敢小看我！若是
[PAGE_BREAK]
　聽到我老師的名字，包準你
[PAGE_BREAK]
　嚇一大跳‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『凱麗小姐，妳的身手的確是
[PAGE_BREAK]
　很不錯，可以請問妳的老師
[PAGE_BREAK]
　是哪一位嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000C]
『這個‥還是不能告訴你！
[PAGE_BREAK]
　不過，如果你們願意讓我加
[PAGE_BREAK]
　入的話，我可以考慮透露一
[PARAGRAPH]
　點。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『歡迎！我們就要和王國軍
[PAGE_BREAK]
　大戰，正愁人手不夠，有妳
[PAGE_BREAK]
　加入真是再好不過了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000C]
『以後我們就是伙伴了，
[PAGE_BREAK]
　請大家多多指教！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『此地不宜久留，
[PAGE_BREAK]
　凱麗小姐，
[PAGE_BREAK]
　我們這就上路吧！』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『看來我們又被人家誤會了
[PAGE_BREAK]
　，索爾，現在我們該怎麼辦
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那還用說，當然是殺進王
[PAGE_BREAK]
　城去弄個明白！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『嗯，夠氣魄！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『我們快上路吧！這件事不
[PAGE_BREAK]
　弄個清楚，更麻煩的事還在
[PAGE_BREAK]
　後頭呢！』
[END]
```
