# 第 19 章 — 黑暗中的狙擊

進入黑森林後遭「死亡骷髏」殘部突襲，龍劍士巴拿羅西亞奉聖寇拉斯王之命前來支援。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| — | 龍劍士巴拿羅西亞 | 章中 (FDFIELD turn event) | 必須在巴拿羅西亞出現 (約第 6 回合 reinforcement) 之後再消滅完敵人才會加入；若先擊敗全部敵人，巴拿羅西亞不會加入。 |

## 敵人配置

死亡骷髏殘部。具體配置寫在 FDFIELD.DAT entry 55。

## 寶物

待 FDFIELD.DAT entry 55 確認。

## 商店

無章內商店。

## 特殊機制

- **失敗條件**：索爾死亡；巴拿羅西亞 (char[0x40]) 在加入後死亡 (gated by save_metadata_block > 6，即巴拿羅西亞已出現後)。在巴拿羅西亞尚未出現之前，char[0x40] 戰死不算敗。
- **共用 init handler**：本章 init 與第 20、21 章共用 (chapter_19_20_21_init_shared)，三章開場流程完全相同。
- **第 4 / 6 / 10 回合事件**：FDFIELD turn-event hooks 控制 AI 行為與第 6 回合的巴拿羅西亞 reinforcement spawn。

## 對話

對話文字 4 pages 來自 FDTXT.DAT entry 19。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『真令人不舒服的森林！這裡
[PAGE_BREAK]
　是什麼地方？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這是大陸北方山脈之麓的黑
[PAGE_BREAK]
　森林，由於山脈和濃密的森
[PAGE_BREAK]
　林阻擋陽光，這一帶看起來
[PARAGRAPH]
　總是陰森森的，一般旅人都
[PAGE_BREAK]
　不敢行經此處。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『過了此地，也就快到死之沼
[PAGE_BREAK]
　澤了，聽說曾有人在這一帶
[PAGE_BREAK]
　遭怪物攻擊‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『我有不好的預感，大家要多
[PAGE_BREAK]
　小心！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『哎呀！老爹你說對啦，前面
[PAGE_BREAK]
　果然有人來了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0051]
『約拿老頭，上次在你手裡折
[PAGE_BREAK]
　損了不少弟兄，今天這筆帳
[PAGE_BREAK]
　要一併討回來！準備受死吧
[PARAGRAPH]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『可惡，又是陰魂不散的死亡
[PAGE_BREAK]
　骷髏！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『不知死活的傢伙，你們的罪
[PAGE_BREAK]
　業就到此為止了！我今天絕
[PAGE_BREAK]
　不放過你們！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x001B]
『咦！這是惡名昭彰的「死亡
[PAGE_BREAK]
　骷髏」傭兵團啊！他們竟然
[PAGE_BREAK]
　在這裡出現，難道果真是有
[PARAGRAPH]
　人在暗中‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『嘿！那可不是巴拿羅西亞閣
[PAGE_BREAK]
　下嗎？真巧啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001B]
『啊！你是凱拉斯嘛！好一陣
[PAGE_BREAK]
　子沒碰面了，想不到會在這
[PAGE_BREAK]
　裡遇見你‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『巴拿羅西亞閣下，現在可能
[PAGE_BREAK]
　不是敘舊的好時候，先讓我
[PAGE_BREAK]
　們解決了這幫想謀害約拿先
[PARAGRAPH]
　生的惡徒再說。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001B]
『約拿先生也在這裡？好極了
[PAGE_BREAK]
　，這場仗也算上我一份！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『‥為什麼‥為什麼這些小鬼
[PAGE_BREAK]
　竟然如此厲害‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x001B]
『這場仗打的真是漂亮！對於
[PAGE_BREAK]
　各位的表現，我個人真是非
[PAGE_BREAK]
　常佩服。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『巴拿羅西亞閣下，陛下不是
[PAGE_BREAK]
　和您在一起的嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001B]
『啊，我早該想到的，原來你
[PAGE_BREAK]
　是聖寇拉斯的部下‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『是的，我們在行經死亡沼澤
[PAGE_BREAK]
　的途中發現「死亡骷髏」在
[PAGE_BREAK]
　此出沒，陛下擔心他們的目
[PARAGRAPH]
　標可能是你們，所以命令我
[PAGE_BREAK]
　先來查看一番。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『原來如此‥‥那他往哪裡去
[PAGE_BREAK]
　了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001B]
『陛下說他已經發現了一些蛛
[PAGE_BREAK]
　絲馬跡，要趕往西方高塔一
[PAGE_BREAK]
　趟，我想現在應該還在路上
[PARAGRAPH]
　吧。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『西方高塔‥‥那不就是「聖
[PAGE_BREAK]
　靈之塔」嗎？那裡是我們精
[PAGE_BREAK]
　靈族的發源聖地，想不到敵
[PARAGRAPH]
　人也看上了那裡，不知道他
[PAGE_BREAK]
　們有何意圖？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『總之，我們要儘快和聖寇拉
[PAGE_BREAK]
　斯會合，看來他已發現了重
[PAGE_BREAK]
　要的線索，往後的幾天可能
[PARAGRAPH]
　得趕一下路，大家忍耐一點
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這有什麼問題！倒是約拿先
[PAGE_BREAK]
　生您年事已大，可不要太勉
[PAGE_BREAK]
　強喔！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『呵呵‥索爾，謝謝你的關心
[PAGE_BREAK]
　，我會注意的。我們這就上
[PAGE_BREAK]
　路吧！』
[END]
```
