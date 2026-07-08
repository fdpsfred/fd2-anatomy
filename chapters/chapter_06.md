# 第 6 章 — 普里茲港

抵達普里茲港尋找賢者約拿，卻因艾迪．沙林斯的通緝身份與王國軍爆發衝突；章末由貝克威帶路赴亞克斯王城。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 13 (0x0D) | 貝克威 | End handler 開頭 | 章末固定加入 |

## 敵人配置

本章 FDFIELD entry 16 共 40 個會生成的 spawn 記錄。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 9 | 精英戰士 | LV9 | ×1 | aggressive_physical |
| 8 | 士兵 | LV8 | ×8 | aggressive_physical |
| 22 | 魔法師 | LV7 | ×4 | aggressive_physical |
| 25 | 僧侶 | LV7 | ×4 | aggressive_physical |
| 19 | 弓箭手 | LV8 | ×4 | aggressive_physical |
| 50 | 萊汀 | LV10 | ×1 | default_attacker |
| 15 | 突擊騎兵 | LV10 | ×12 | default_attacker |
| 8 | 士兵 | LV5 | ×1 | default_attacker |

友軍 NPC（team 1，戰場自走）：

| enemy_data | 單位 | 等級 | 數量 |
|---|---|---|---|
| 2 | 傭兵 | LV7 | ×4 |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：魔法水 (0xCE)、紅寶石 (0xCA)

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：長劍 (0x02)、回復劑 (0xC1)、紅寶石 (0xCA)
- 金錢：2000

## 商店

story 章，intro 主選單提供武器店 / 道具店，另有以隱藏熱鍵（Alt+F6）開啟的神秘商店。品項（chapter_intro_metadata entry 5）：

- **武器店**：闊劍 (0x01)、長劍 (0x02)、騎槍 (0x15)、長戟 (0x16)、迴旋斧 (0x21)、戰斧 (0x22)、長弓 (0x2D)、釘頭鎚 (0x35)、巨鎚 (0x36)、硬皮甲 (0x85)、夜行裝 (0x86)、鎖子甲 (0x91)
- **道具店**：草藥 (0xC0)、回復劑 (0xC1)
- **神秘商店（Alt+F6）**：聖者之戒 (0x58)、心眼之書 (0x5C)、白金徽章 (0x5D)、飛龍卵 (0xCD)

## 特殊機制

- **失敗條件**：索爾死亡，走 default post-action handler `fd2_check_battle_end_default_handler`（無自訂勝負條件）。
- **章末加入**：貝克威 (char 13) 由 end handler 第一步 `fd2_init_runtime_char_from_base_growth(0xD)` 固定加入。
- **極簡 init**：30 章中最小級的 init handler (79 B)，僅顯示 1 頁對話，無 cutscene、無 portrait load、無 `fd2_pan_cursor_and_window`，是純戰鬥準備章。
- **回合觸發事件**：戰鬥中掛有 3 個 turn-event hook，分別在第 5、10、15 回合的玩家回合開場觸發（第 5 回合為純對白，第 10/15 回合為 char_conditional）。詳見 FDFIELD event script 節的 hook 表。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_06_init @ 0x0003314B` | 79 B |
| End | `fd2_chapter_06_end @ 0x00023296` | 82 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[5]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[5]` |  |

### Init handler

極簡三步，純戰鬥準備：

- `fd2_init_battle_state_for_chapter`
- `fd2_display_dialog_scene(page=0)`
- `fd2_pan_cursor_to_char(0)`

無 cutscene、無 portrait load、無 `fd2_pan_cursor_and_window`。

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 6 | 0 |
| End | 6 | 6 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 開頭 `fd2_init_runtime_char_from_base_growth(0xD)` → 貝克威加入。

### Cutscene events

- Init：無
- End：`0x1B` (1 event)

### Post-action handler

`data_fd2_chapter_post_action_handler_table[5]` 指向 `fd2_check_battle_end_default_handler`，無自訂勝負條件。

### End handler events

`fd2_chapter_06_end @ 0x00023296` (82 B)：

1. `fd2_init_runtime_char_from_base_growth(0xD)` — char 13 = 貝克威加入
2. `fd2_load_chapter_portraits_and_dump_tmp(race_id=3)` 載入加入時的肖像
3. `fd2_pan_cursor_and_window(5, 0xE)` + `fd2_cutscene_event_trigger(0x1B)`
4. `fd2_display_dialog_scene(page=6)`
5. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **16** (= chapter_id × 3 + 1，chapter_id = 5)，entry size 1171 bytes。
header：`party_member_count` = 8、`char_spawn_count` = 40。header layout 見 `resource_info/fdfield.md`。

3 / 16 個 turn-event hook active (其餘 13 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 5  | 2 (new_player_turn_intro) | 0x14 | `0x000347B1` | dialog_only；ch6_dialog |
| 10 | 2 (new_player_turn_intro) | 0x15 | `0x000347D9` | char_conditional；ch6_char_cond |
| 15 | 2 (new_player_turn_intro) | 0x16 | `0x00034819` | char_conditional；ch6_char_cond |

## 對話

對話文字 8 pages 來自 FDTXT.DAT entry 6。Init handler 引用 page 0（普里茲港與王國軍衝突的開戰對白），End handler 引用 page 6（貝克威說明賢者約拿的去向，並邀隊伍同赴亞克斯王城）。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x000A]
『奇怪了，約拿好像不在城裡
[PAGE_BREAK]
　，只好先來這裡找找看了
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『前面好像有士兵在巡邏，大
[PAGE_BREAK]
　家要小心點！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000D]
『喂！你們站住！這裡不是禁
[PAGE_BREAK]
　止進入了嗎？你們來這裡
[PAGE_BREAK]
　幹什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『真抱歉，我們是來找人的‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000D]
『找什麼找，這裡什麼人都沒
[PAGE_BREAK]
　有！回去回去！‥‥咦？
[PARAGRAPH]
　等等，你們通通給我站住！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『又怎麼了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000D]
『你是艾迪．沙林斯吧！
[PAGE_BREAK]
　自從上次你們一夥搶劫了
[PAGE_BREAK]
　維克勒城之後，
[PARAGRAPH]
　我們一直在找你，想不到你
[PAGE_BREAK]
　居然跑去當別人的護衛了！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0046]
『‥‥‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000D]
『所以啊，我看你還是乖乖
[PAGE_BREAK]
　束手就擒，跟我們回去，
[PAGE_BREAK]
　這樣對你比較好。嗯？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『等一下！護衛是爸爸派給
[PAGE_BREAK]
　我的，你們要抓走他，叫我
[PAGE_BREAK]
　回去怎麼交代？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000D]
『那可不干我們的事！再無
[PAGE_BREAK]
　理取鬧的話，就連妳也一起
[PAGE_BREAK]
　逮捕！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『你敢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『那邊在吵什麼吵？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000D]
『隊長，我在這群人中發現
[PAGE_BREAK]
　一個通緝犯，但他的主人拒
[PARAGRAPH]
　絕讓我們逮捕他。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『那就全部抓起來！
[PAGE_BREAK]
　拒捕者格殺勿論！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『看來我們又有麻煩了，
[PAGE_BREAK]
　索爾，怎麼辦？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這些士兵也太蠻橫了！
[PAGE_BREAK]
　亞雷斯，我們上！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『這下子想不打都不行嘍！
[PAGE_BREAK]
　』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『看你們的身手不像是普通人
[PAGE_BREAK]
　，你們到底是誰？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『剛剛說過了，我們只是普通
[PAGE_BREAK]
　了旅行者罷了』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『不可能！你們絕對不是普通
[PAGE_BREAK]
　的旅行者！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『信不信由你了！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『可恨！這些人還真強！
[PAGE_BREAK]
　，弟兄們再撐一會兒萊汀
[PAGE_BREAK]
　大人就來了，
[PARAGRAPH]
　再撐一會兒！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『看他們的樣子似乎還有援軍
[PAGE_BREAK]
　！大家，先把小隊長打倒吧
[PAGE_BREAK]
　！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0076]
『小隊長，這是怎麼一回事？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『萊‥萊汀大人！
[PAGE_BREAK]
　這一群匪徒公然拒捕，
[PAGE_BREAK]
　他們戰技高超，
[PARAGRAPH]
　我們正嘗試要拿下他‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0076]
『原來不過是些匪徒而已，
[PAGE_BREAK]
　通通給我抓起來！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『你‥你們不是一般匪徒，
[PAGE_BREAK]
　你們‥到底有何目的？‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這一切只是誤會而已，我
[PAGE_BREAK]
　們一一』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0021]
『隊‥隊長！這‥這是怎麼
[PAGE_BREAK]
　回事？！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0008]
『史恩！快去通報邊境守衛
[PAGE_BREAK]
　隊，叫他們追捕這批匪徒，
[PAGE_BREAK]
　我‥我已經盡力了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x0021]
『隊‥隊長！你放心，這件
[PAGE_BREAK]
　事我一定辦到！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x000D]
『天啊！你們居然攻擊王國派
[PAGE_BREAK]
　來的調查隊，還把小隊長給
[PAGE_BREAK]
　殺了，難道你們不怕被全王
[PARAGRAPH]
　國軍通緝嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『這只是一場誤會，可是他們
[PAGE_BREAK]
　不肯聽我們解釋‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000D]
『唉，現在說什麼都沒用啦！
[PAGE_BREAK]
　你們到底是來幹什麼的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『我們來此找賢者約拿‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000D]
『你們是來找約拿的？約拿兩
[PAGE_BREAK]
　天前就已經離開此地了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『為什麼？聽說他近年來很少
[PAGE_BREAK]
　離開普里茲港的啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000D]
『是沒錯。不過這個港城最近
[PAGE_BREAK]
　夜裡發生不少樁失蹤事件，
[PAGE_BREAK]
　狀況嚴重到連王國都派守備
[PARAGRAPH]
　隊來調查了，想必你們還不
[PAGE_BREAK]
　知道吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『怪不得碼頭一帶到處都是士
[PAGE_BREAK]
　兵。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000D]
『我叫做貝克威，和約拿算是
[PAGE_BREAK]
　滿熟的朋友。前兩天他來找
[PAGE_BREAK]
　我，說他發現一件大陰謀，
[PARAGRAPH]
　和連日來的失蹤事件有關，
[PAGE_BREAK]
　必須立刻前去調查。他委託
[PAGE_BREAK]
　我一些事之後就離開了‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『真倒楣，那這趟不就又白跑
[PAGE_BREAK]
　了嗎？還因此和王國軍結了
[PAGE_BREAK]
　怨‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『你是在指我嗎？
[PAGE_BREAK]
　這又不全是我的錯！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『妳還敢說，要不是因為妳爸
[PAGE_BREAK]
　爸派給妳的護衛‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000D]
『好啦好啦！吵架解決不了事
[PAGE_BREAK]
　情，這樣吧，約拿約了我兩
[PAGE_BREAK]
　天後到亞克斯王國的王城去
[PARAGRAPH]
　和他碰面，既然你們有事找
[PAGE_BREAK]
　他，就和我一起去好了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『嗄？到王城去？那我們不是
[PAGE_BREAK]
　自投羅網嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『這個你們倒可以不用擔心，
[PAGE_BREAK]
　我保證一定不會有事的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『希莉亞，妳為什麼敢做這種
[PAGE_BREAK]
　保證？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『這是秘密，不告訴你！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好，既然如此，我們就準備
[PAGE_BREAK]
　上路吧！貝克威先生，您怎
[PAGE_BREAK]
　樣呢？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000D]
『先和我回城裏一趟，等我收
[PAGE_BREAK]
　拾一下東西，待會兒就可以
[PAGE_BREAK]
　啟程了。走吧！』
[END]
```

### Page 7

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『奈野啊捏？』
[END]
```
