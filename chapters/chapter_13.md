# 第 13 章 — 哈斯米爾之戰

精靈都城哈斯米爾遭獸人大軍襲擊；玩家保護倖存的精靈戰士並擊退獸人，戰後老戰士哈瓦特加入隊伍。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 3 | 哈瓦特 | 章末 | End handler 加入；無條件加入（不檢查戰場出場 trigger） |

## 敵人配置

由 FDFIELD.DAT entry 37 的 char_spawn_records 決定（chapter_id × 3 + 1, chapter_id = 12）。詳見 `resource_info/fdfield.md`。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。哈斯米爾鎮中含「火焰之眼」寶石（依 page 0 / page 11 對話線索）。

## 商店

無。

## 特殊機制

- **失敗條件**：索爾死亡 ／ 精靈族全滅（chars[0xF..0x1A] 12 個 NPC 全死）／ 哈瓦特出場後戰死，皆由 `fd2_chapter_13_post_action` 判定。
- **哈瓦特 conditional 援軍**：第 4 回合己方回合結束時，哈瓦特在戰場下方登場前來幫忙（FDFIELD turn = 4、phase = 1 的 turn-event hook 觸發，載入 race=1 records 並播 dialog page 1）。唯有「哈瓦特出現後戰死」才會觸發 lose，因 post-action 條件 2 帶 `data_fd2_battle_turn_counter > 5` 閘（第 6 回合起才檢查其死亡）。哈瓦特在戰場出場與否不影響 End handler 加入。
- **哈瓦特名稱正名**：roster 正名為哈瓦特，對應 End handler `fd2_init_runtime_char_from_base_growth(3)` 的 char_id 3。對白（page 1 / 2 / 9）以暱稱「哈瓦那老爹」稱呼；攻略本作「哈瓦諾」係筆誤。三者為同一人；對白 transcript 維持原文「哈瓦那」不改。
- **Init／Post-action 工作量倒置**：init 僅 17 B，post_action 達 189 B；戰鬥判定邏輯全部寫在 turn-cycle 的 post-action 處理，而非 init。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_13_init @ 0x0003346B` | 17 B（最小 init） |
| End | `fd2_chapter_13_end @ 0x0002389F` | 61 B |
| Post-action | `fd2_chapter_13_post_action @ 0x00020765` | 189 B（最複雜 non-default） |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[12]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[12]` |  |

### Init handler

極簡三步：

1. `fd2_init_battle_state_for_chapter`
2. `fd2_display_dialog_scene(page=0)`
3. `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 13 | 0 |
| Post-action | 13 | 2, 10 |
| End | 13 | 9 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 中呼叫 `fd2_init_runtime_char_from_base_growth(3)` — 哈瓦特。End handler **無條件**呼叫，不檢查 post-action 是否觸發過 char[0x3B]（哈瓦特戰場出場 trigger）；「有沒有出現都會加入」是因為加入邏輯獨立於戰場出場邏輯。

### Cutscene events

無 init／end cutscene event。

### Post-action handler

`fd2_chapter_13_post_action @ 0x20765`（最複雜 non-default handler）：

1. default 判定（敵全死 = 勝、索爾死 = 負）
2. 條件 1：if `chars[0xF..0x1A]`（12 個 NPC = 精靈族）全部死亡 → `fd2_display_dialog_scene(page=10)` + `game_event_flag = 1`（lose）
3. 條件 2：if `data_fd2_battle_turn_counter > 5`（回合計數 > 5）AND `char[0x3B]` 死亡 → `fd2_display_dialog_scene(page=2)` + `game_event_flag = 1`（lose）

對應：chars[0xF..0x1A] 12 個 = 精靈族；條件 2 的 char[0x3B] = 哈瓦特，`data_fd2_battle_turn_counter > 5` = 第 6 回合起，若哈瓦特出場後戰死則播 page 2 dialog 並判負。

### End handler events

`fd2_chapter_13_end @ 0x2389F`：

1. `fd2_display_dialog_scene(page=9)`
2. `fd2_save_runtime_char_to_template`
3. `fd2_init_runtime_char_from_base_growth(3)` — 哈瓦特加入
4. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **37**（= chapter_id × 3 + 1, chapter_id = 12），entry size 1951 bytes，party_member_count = 15，char_spawn_count = 70。header layout 見 `resource_info/fdfield.md`。

2 / 16 active turn-event hooks（其餘 14 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 4 | 1 (end_of_player_turn) | 0x05 | `0x00034D68` | thunk → `fd2_show_chapter_dialog_with_portrait_set_1` @0x34BE7（load_chapter_portraits race=1 + dialog page 1） |
| 9 | 0 (enemy_turn_intro) | 0x07 | `0x00034D72` | dialog_with_state |

## 對話

對話文字共 12 pages，取自 FDTXT.DAT entry 13。Init handler 引用 page 0；`fd2_chapter_13_post_action` 引用 page 2（哈瓦特陣亡）與 page 10（精靈族全滅）；End handler 引用 page 9；page 1 由第 4 回合的 FDFIELD turn-event handler 引用。對白內哈瓦特以暱稱「哈瓦那老爹」出現，係遊戲內部變體，transcript 維持原文不改。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x000D]
『糟了！
[PAGE_BREAK]
　我們果然來遲了一步！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『不要緊，村中的居民好像
[PAGE_BREAK]
　都已經先撤走了，只剩下一
[PAGE_BREAK]
　些戰士還在抵擋獸人的攻勢
[PARAGRAPH]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『好極了，寶箱好像都還安
[PAGE_BREAK]
　然無恙，不知道
[PAGE_BREAK]
　「火焰之眼」是否‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『你說什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『‥沒事！沒事！
[PAGE_BREAK]
　我們趕快去幫忙吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒錯！把那些獸人殺個落
[PAGE_BREAK]
　花流水！我們上吧！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0003]
『我的天啊！
[PAGE_BREAK]
　這是怎麼一回事？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『老‥老爹！
[PAGE_BREAK]
　您怎麼來了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『怎麼？我不能來嗎？
[PAGE_BREAK]
　我只是來看個老朋友而已‥
[PAGE_BREAK]
　怎麼會變成這樣子？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『獸人大軍來攻，
[PAGE_BREAK]
　村內的居民好像都逃光了，
[PAGE_BREAK]
　我們正在迎戰敵軍‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『獸人？
[PAGE_BREAK]
　這些傢伙是活的不耐煩了？
[PAGE_BREAK]
　我先來給牠們幾斧頭再說！
[PARAGRAPH]
　小子啊，你也要好好幹！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『是的，老爹！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哈瓦那老爹，歡迎加入！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0003]
『‥啊，我年紀畢竟是老了，
[PAGE_BREAK]
　不能太逞強‥‥哈諾，
[PAGE_BREAK]
　以後你要自己照顧自己了，
[PARAGRAPH]
　老爹‥不行了‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x000F]
『啊‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『你們消滅不了精靈族的‥‥
[PAGE_BREAK]
　啊‥‥』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_CHAR=0x0013]
『哇啊‥‥』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_CHAR=0x0015]
『啊‥‥』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_CHAR=0x0017]
『哇啊‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0067]
『吼嗚！宰了這些傢伙‥‥』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_ID=0x0048]
『你們不是貝克威和珊嗎？
[PAGE_BREAK]
　還有好久不見的哈瓦那老爹
[PAGE_BREAK]
　幸好你們帶了幫手來，要不
[PARAGRAPH]
　然我們真會抵擋不住了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0048]
『不過，你們又怎麼會知道
[PAGE_BREAK]
　獸人大軍前來攻擊呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『湊巧啦！
[PAGE_BREAK]
　還好這些朋友願意幫忙。
[PAGE_BREAK]
　城中的居民呢？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0048]
『我們事先收到警告，
[PAGE_BREAK]
　居民都連夜撤到安全的地方
[PAGE_BREAK]
　去了。
[PARAGRAPH]
　不過聽說獸人大軍還在離哈
[PAGE_BREAK]
　斯米爾不遠的蘭迪平原駐囤
[PAGE_BREAK]
　眼前情勢仍然很險惡‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『對了，獸人大軍為何要攻打
[PAGE_BREAK]
　哈斯米爾？我到現在還是不
[PAGE_BREAK]
　明白。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0048]
『這一點我們也不清楚，不過
[PAGE_BREAK]
　牠們在開戰前曾大聲宣言，
[PAGE_BREAK]
　要把精靈族殺戮殆盡，為何
[PARAGRAPH]
　如此我們並不清楚‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『還不簡單！獸人大軍現在不
[PAGE_BREAK]
　是駐囤在那個什麼平原嗎？
[PAGE_BREAK]
　我們去把他們殺個人仰馬翻
[PARAGRAPH]
　，再抓幾個來問問不就好了
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『典型的索爾作風‥‥不過
[PAGE_BREAK]
　也不失為一個好方法。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『是啊，讓那些獸人繼續待在
[PAGE_BREAK]
　那裡太危險了，先把牠們在
[PAGE_BREAK]
　野外殲滅，免得牠們又來進
[PARAGRAPH]
　攻。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『正好老爹也來了，
[PAGE_BREAK]
　我們的戰力大增，要對付
[PAGE_BREAK]
　那些獸人相信不是難事。
[PARAGRAPH]
　老爹，你會和我們一起去吧
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『當然！就這幾隻獸人，
[PAGE_BREAK]
　那能讓我過癮呢！再說，
[PAGE_BREAK]
　我也想看看這些日子以來，
[PARAGRAPH]
　小子長進了多少。
[PAGE_BREAK]
　哈哈哈！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『既然如此，那我們稍事休息
[PAGE_BREAK]
　之後就出發吧！』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『糟糕，精靈們全被消滅了
[PAGE_BREAK]
　！』
[END]
```

### Page 11

```text
[PORTRAIT_LEFT_BY_ID=0x0027]
『年輕人，你們年紀輕輕就有
[PAGE_BREAK]
　這種武藝，真是了不起！
[PARAGRAPH]
　這件重要的寶物絕不能讓它
[PAGE_BREAK]
　落入獸人的手中，
[PAGE_BREAK]
　就先拜託你保管了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好的！這是‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0027]
『你會發現它的功用的，
[PAGE_BREAK]
　代替我好好的使用它吧！』
[END]
```
