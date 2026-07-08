# 第 1 章 — 初試身手

索爾、亞雷斯、悠妮一行人在前往馬拉大陸途中於海島小憩，遭遇橫行沿海的海盜襲擊；島上的哈瓦特父子（哈瓦特、哈諾）與亞克斯王國海防隊相助擊退海盜，戰後哈諾加入隊伍。本章是 30 章中唯一含獨家 prologue（FD2 世界觀開場 cutscene）的章節。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 0 | 索爾 (Lan) | 章首 | 主角，每章必在 |
| 4 | 亞雷斯 | 章首 | 無條件 |
| 9 | 悠妮 | 章首預初始化但暫未上場 | ch2+ template 用 |
| 30 (0x1E) | 蓋亞 | 章首 cutscene | 暫時出場 NPC |
| 1 | 哈諾 | 第 3 回合 reinforcement | 第 3 回合 FDFIELD turn-event hook 觸發加入；若哈諾還未出現便已消滅完敵人，哈諾不會加入 |

## 敵人配置

本章 FDFIELD entry 1 共 30 個會生成的 spawn 記錄。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 28 | 盜賊 | LV2 | ×16 | default_attacker |
| 29 | 盜賊頭目 | LV3 | ×1 | default_attacker |
| 35 | 獸人隊長 | LV1 | ×3 | default_attacker |
| 8 | 士兵 | LV1 | ×4 | default_attacker |

友軍 NPC（team 1，戰場自走）——亞克斯王國海防隊士兵：

| enemy_data | 單位 | 等級 | 數量 |
|---|---|---|---|
| 0 | 士兵 | LV2 | ×4 |

哈瓦特（char 0x03）、哈諾（char 0x01）為 team 2 玩家班底 record，不列於上表；哈諾在第 3 回合援軍波次登場，戰後加入隊伍（見 §特殊機制 / §FDFIELD event script）。

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：草藥 (0xC0)、光之杖 (0x3D)、光之斧 (0x28)
- 金錢：3000、5000
- 空寶箱（0 金錢誘餌）×1

（tile_pickup 另有 1 筆 kind≥2 = 劇情事件觸發點（tile[12]），非寶物，見 §FDFIELD event script。）

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：草藥 (0xC0)
- 金錢：1000

## 商店

story 章，intro 主選單提供武器店 / 道具店，另有以隱藏熱鍵（Shift+F1）開啟的神秘商店。品項（chapter_intro_metadata entry 0）：

- **武器店**：布衣 (0x80)、旅行裝 (0x81)、皮甲 (0x84)、法師袍 (0xA5)
- **道具店**：草藥 (0xC0)
- **神秘商店（Shift+F1）**：闊劍 (0x01)、長戟 (0x16)、釘頭鎚 (0x35)、草藥 (0xC0)、回復劑 (0xC1)、皮甲 (0x84)

## 特殊機制

- **獨家 prologue**：本章 init handler 分 Phase A–D 四段（多數章節僅有正式戰鬥段）。
  prologue cutscene 的 FDTXT 對白跨 entry 33（Phase A+B）→ entry 32（Phase C）→
  entry 1（Phase D 正式戰鬥）。

- **哈瓦特暴走**：哈諾（char_id 1）死後，哈瓦特的 AI 因失去 ai_target 自然 fall-through
  為 default attacker。屬 implicit consequence，非 turn-triggered AI flip。
  - FDFIELD entry 1 唯二的 `team=2 player_class` record 是 record[8] @+0x153
    (char_id 0x03 哈瓦特, lv3, race=7) 與 record[9] @+0x16D
    (char_id 0x01 哈諾, lv1, race=3, pickup_kind=2 param=4 = 第 3 回合 reinforcement
    觸發)；其餘為 enemy_class 與 team=1 NPC（友方海防隊士兵 ×4 @ record[19..22]）。
  - **三 byte AI override**（char_spawn_record `+0x11/+0x12/+0x13` =
    `ai_class_flags / ai_aux / ai_target_pos`，由 `fd2_init_runtime_char_for_battle @ 0x10C50`
    拷到 runtime_char `+0x34/+0x35/+0x36` = `pCombat_aux_block[0xD/E/F]`）對哈瓦特與哈諾
    皆為 `(0, 0, 0)`，無顯式 protective AI 設定。
  - **protective 行為的實際來源**是哈瓦特 record[8] 的 `+0x02 ai_target_id = 0x01`
    (= 哈諾 char_id 1)：default attacker AI 透過 ai_target_id 偏好接近哈諾的敵人，形成護子
    效果。哈諾死後 ai_target_id 指向已 dead 的 runtime_char，fall-through 為純 default
    attacker = 「暴走」。
  - 索爾 / 亞雷斯 / 悠妮 / 蓋亞 不在 FDFIELD records 內：這 4 名主角由
    `fd2_chapter_01_init` 直接 register，不走 FDFIELD char_spawn 機制。

- **勝負條件**：default — 全敵死 = 勝、索爾（char_id 0）死 = 負（詳 Post-action handler）。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_01_init @ 0x0003231B` | 最大 init handler |
| End | `fd2_chapter_01_end @ 0x00022EF6` | 65 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default — 無自訂勝負) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[0] @ 0x51E63` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[0] @ 0x51E81` |  |

### Init handler

init handler 依 `current_chapter_id` 三次寫入（0x20 / 0x1F / 0）切三個地圖階段；map 0x20 段
再以 `cutscene_event_state=1` 過場 cutscene 0x64 為界拆成 Phase A、Phase B，故共四段 Phase A–D。

#### Phase A — Prologue 地圖 1（`current_chapter_id = 0x20`）

`fd2_init_battle_state_for_chapter` 進入 prologue 模式：

- `fd2_pan_cursor_and_window(3, 0x22)`
- `fd2_cutscene_event_trigger(0x63=99)`
- `walk_step_up × 0xF` 然後 `fd2_display_dialog_scene(page=0)`
- `walk_step_up × 0xD` 然後 `fd2_display_dialog_scene(page=1)`
- `fd2_set_bgm_track_with_fade(-1, 0)` 停 BGM
- `cutscene_event_state=1`；`fd2_cutscene_event_trigger(0x64=100)` 過場

#### Phase B — 仍在 map 0x20（不重設 chapter_id、不重跑 battle-state init）

- `fd2_pan_cursor_and_window(0, 0x2B)`
- `fd2_set_bgm_track_with_fade(0xB, 0)` 切 BGM track 11
- `fd2_play_palette_fade_in`
- 4 段 cutscene + dialog：`0x65→page2`、`0x66→page3`、`0x67→page4`、`0x68→page5`
- `cutscene_event_state=1`；`fd2_cutscene_event_trigger(0x69)` 過場
- Phase A+B 共用 6 個 prologue dialog pages（FDTXT entry 33 pages 0..5）

#### Phase C — Prologue 地圖 2（`current_chapter_id = 0x1F`）

`fd2_init_battle_state_for_chapter` 切到第二段 prologue 地圖，FDTXT 切到 entry 32：

- `fd2_pan_cursor_and_window(5, 0x2A)`
- `fd2_load_chapter_portraits_and_dump_tmp(1)` 載肖像 set 1
- cutscene events `0x5A..0x61` 配 dialog pages `0..9`
- 中段 `fd2_load_chapter_portraits_and_dump_tmp(3)` 換肖像、`fd2_pan_cursor_and_window(4, 0x29)`
- 中段 `fd2_mark_char_as_dead(2)` 移除 cutscene 角色
- 末段 `fd2_load_chapter_portraits_and_dump_tmp(5)` 換肖像 set
- `fd2_set_bgm_track_with_fade(-1, 0)` 停 BGM
- `cutscene_event_state=1`；`fd2_cutscene_event_trigger(0x62)` 結束 intro

#### Phase D — 第 1 章正式戰鬥（`current_chapter_id = 0`）

先初始化 4 個 runtime char，**再**呼叫 `fd2_init_battle_state_for_chapter`（進入正式戰鬥模式、
FDTXT 切到 entry 1）：

- `fd2_init_runtime_char_from_base_growth(0)` — 索爾
- `fd2_init_runtime_char_from_base_growth(9)` — 悠妮（預初始化，稍後 `fd2_mark_char_as_dead(9)` 標未上場）
- `fd2_init_runtime_char_from_base_growth(4)` — 亞雷斯
- `fd2_init_runtime_char_from_base_growth(0x1E=30)` — 蓋亞
- `fd2_init_battle_state_for_chapter`
- `fd2_pan_cursor_and_window(4, 0xC)`
- `fd2_cutscene_event_trigger(0)` + `fd2_display_dialog_scene(page=0)`（entry 1）
- `fd2_animate_party_addition_with_appear_effect(1)` + `fd2_cutscene_event_trigger(1)`
- `fd2_animate_party_addition_with_appear_effect(2)` + `fd2_cutscene_event_trigger(2)`
- `fd2_display_dialog_scene(page=1)`
- `fd2_cutscene_event_trigger(5)` + `fd2_mark_char_as_dead(9)` — 悠妮退場
- `fd2_composite_battle_frame(0)`；`fd2_display_dialog_scene(page=2)`
- `fd2_clear_all_chars_facing`；`fd2_pan_cursor_to_char(0)` 鏡頭聚焦索爾
- `party_total_gold = 0`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Phase A+B (prologue) | 33 | 0, 1, 2, 3, 4, 5 |
| Phase C (prologue 地圖 2) | 32 | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 |
| Phase D (正式戰鬥) | 1 | 0, 1, 2 |
| End | 1 | 9 |

### char_id 初始化序列

`fd2_init_runtime_char_from_base_growth` 呼叫順序：
- char_id 0 → 索爾 (主角)
- char_id 9 → 悠妮 (預初始化即標 dead，ch2+ template 用)
- char_id 4 → 亞雷斯
- char_id 0x1E (30) → 蓋亞 (cutscene NPC，攻略未直接提)

哈諾 (char_id 1) 由 FDFIELD turn-event handler 0x00 在 turn 3 觸發加入，
不在本 init handler 內。

### Cutscene events

該章用到的 `fd2_cutscene_event_trigger` 呼叫：
`0x00, 0x01, 0x02, 0x05, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62,
 0x63 (=99), 0x64 (=100), 0x65, 0x66, 0x67, 0x68, 0x69`

- `0x00..0x05` = 普通章節 walk-animation (多章共用)
- `0x5A..0x69` = ch1 prologue/intro 專用 walk-animation
- `0x63=99` 與 `0x64=100` 在 binary 顯示為十進位

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id] @ 0x627D8` 的 walk-animation
script (`[n_groups][group: walk_count|step_count|{char_idx,dir}*N]+`)。

### Post-action handler

`data_fd2_chapter_post_action_handler_table[0]` 指向 `fd2_check_battle_end_default_handler`，
無自訂勝負條件：所有 team-0 死 → win，索爾 (char_id 0) 死 → lose。

攻略提到的 reinforcement events 由 FDFIELD event script (turn-event hooks) 處理，
**不是** post_action_handler。

### End handler events

`fd2_chapter_01_end @ 0x22EF6` — 最簡單的 end handler 之一：

1. `fd2_display_dialog_scene(page=9)` — 第 1 章結局對話 (entry 1 page 9)
2. `fd2_save_runtime_char_to_template` — 把 runtime party 狀態存回 template
3. `current_chapter_id = 1` — 推進到第 2 章

無 cutscene、無資源 reload、無加入新角色。

## FDFIELD event script

FDFIELD entry idx **1**（= chapter_id × 3 + 1，chapter_id = 0），entry size 937 bytes；
`party_member_count` = 4，`char_spawn_count` = 30（loader 迭代上限；file 實含 31 records，
末筆 race_id=0xFF 為 reserved，永不 load）。header layout 見 `resource_info/fdfield.md`。

4 / 16 active turn-event hooks（其餘 12 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 1 (end_of_player_turn) | 0x00 | `0x000341DB` | 哈諾加入 (`fd2_init_runtime_char_from_base_growth(1)` + cutscene 7/8 + dialog 0xB, 3) |
| 4 | 0 (enemy_turn_intro) | 0x01 | `0x000342B5` | party slot 4 reveal (cutscene 3 + dialog 4) — 哈瓦特 reveal |
| 5 | 0 (enemy_turn_intro) | 0x02 | `0x0003431D` | party slot 5 reveal (cutscene 4 + dialog 5) — 援軍出場 |
| 6 | 1 (end_of_player_turn) | 0x03 | `0x00034377` | portrait race=6 swap + cutscene 6 + dialog 6 — 場景過場 |

## 對話

本章正式戰鬥的對話文字 12 pages 來自 FDTXT.DAT entry 1。Init Phase D 引用 page 0/1/2，
End handler 引用 page 9，其餘 pages 由 FDFIELD turn-event handler 內部
`fd2_display_dialog_scene` 引用。prologue 階段（Phase A+B / Phase C）另引用 FDTXT entry 33
與 entry 32 的對白，屬 ch1 prologue / intro 專用內容。下列 transcript 以 entry 1（pages 0–11）為主。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『累死了，
[PAGE_BREAK]
　大家休息一下吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0004]
『聽說再越過這片海洋
[PAGE_BREAK]
　就到馬拉大陸了，
[PAGE_BREAK]
　我們先在此休息一會兒，
[PARAGRAPH]
　等海水漲潮適合上岸的時候
[PAGE_BREAK]
　船夫就會來接我們。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好極了。悠妮，
[PAGE_BREAK]
妳‥‥嗯，坐了這麼久的船，
[PAGE_BREAK]
有點累吧？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0009]
『嗯，還好。
[PAGE_BREAK]
　海風吹起來真舒服啊‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001E]
『‥‥！！！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x000A]
『瞧！
[PAGE_BREAK]
　竟有呆鳥在這小島上休息，
[PAGE_BREAK]
　真是天上掉下來的肥肉。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0009]
『就是說嘛！
[PAGE_BREAK]
　俺去通報老大支援，
[PAGE_BREAK]
　你們趕快把這些小伙子擺
[PAGE_BREAK]
　平。』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『亞雷斯，那群傢伙
[PAGE_BREAK]
　在那裡鬼叫些什麼？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『嗯‥‥嘿，好像是船夫
[PAGE_BREAK]
　提到過的海盜耶！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『海盜？
[PAGE_BREAK]
　是要來搶劫我們的嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『豈止搶劫而已，聽說
[PAGE_BREAK]
　這批海盜橫行馬拉大陸沿
[PAGE_BREAK]
　海，殺人越貨無所不為。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『什麼！要打架我奉陪，
[PAGE_BREAK]
　要搶劫嘛門都沒有。
[PARAGRAPH]
　亞雷斯，好久沒活動活動
[PAGE_BREAK]
　筋骨了，你沒問題吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『豈止沒問題而已，
[PAGE_BREAK]
　我手癢難熬，
[PAGE_BREAK]
　都快受不了啦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『喂！小子們，
[PAGE_BREAK]
　乖乖的把身上的錢財和
[PAGE_BREAK]
　那個漂亮小妞交出來，
[PARAGRAPH]
　我們就在老大面前說說好話
[PAGE_BREAK]
　保你們一命！不然‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『什麼是漂亮小妞？
[PAGE_BREAK]
　是指我嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『不是啦！那是‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可惡，
[PAGE_BREAK]
　你們這些亂說話的海盜，
[PAGE_BREAK]
　我要把你們全宰了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000A]
『啊呀，
[PAGE_BREAK]
　看來他們想抵抗呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『那就殺無赦！
[PAGE_BREAK]
　上啊！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0003]
『真是的，吵的要命，
[PAGE_BREAK]
　到底在搞什麼‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『老爸，
[PAGE_BREAK]
　有人在島上打架呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『什麼？待我看看‥
[PAGE_BREAK]
　啊哈，可不是海盜
[PAGE_BREAK]
　在打劫旅客嗎？
[PARAGRAPH]
　居然在我們門前搶人，
[PAGE_BREAK]
　膽子不小啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『就是說啊！
[PAGE_BREAK]
　老爸，您說這該怎麼辦？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『小子啊，
[PAGE_BREAK]
　這可是絕佳的歷練機會，
[PAGE_BREAK]
　我們就幫他們一個忙吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『老頭子，
[PAGE_BREAK]
　你們是來幹什麼的？
[PAGE_BREAK]
　若不想受傷就別過來
[PAGE_BREAK]
　湊熱鬧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『別緊張，小伙子，
[PAGE_BREAK]
　我們是來幫你們打退海盜
[PAGE_BREAK]
　的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『什麼？就這一點海盜，
[PAGE_BREAK]
　還用不著你們來幫忙！
[PAGE_BREAK]
　回去回去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『哎，索爾，人家是好意
[PAGE_BREAK]
　要幫忙，你也說幾句
[PAGE_BREAK]
　好聽一點的吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這樣嗎？好吧，那就‥喂，
[PAGE_BREAK]
　老頭子，如果你要幫忙的話
[PAGE_BREAK]
　就先說聲多謝了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『這些年輕人真有精神啊！
[PAGE_BREAK]
　小子，我們上！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『好的，老爸！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x000F]
『咦，
[PAGE_BREAK]
　弟兄們好像陷入苦戰呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『那還等什麼！我們上吧！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x0061]
『小子們，你們在搞些什麼！
[PAGE_BREAK]
　連這些小鬼都收拾不了，
[PAGE_BREAK]
　還要勞動老大我出馬，
[PAGE_BREAK]
　不怕給別人笑話嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼，這難看又吵人的傢伙
[PAGE_BREAK]
　又是來幹什麼的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『大概是海盜首領吧！
[PAGE_BREAK]
　總算有個像樣的對手上場了
[PAGE_BREAK]
　這傢伙就交給我來處理啦！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那怎麼可以！
[PAGE_BREAK]
　對付這種麻煩的傢伙，
[PAGE_BREAK]
　當然是非我莫屬了！
[PAGE_BREAK]
　讓我來！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，你真是不夠朋友！
[PAGE_BREAK]
　就為了在喜歡的女孩子
[PAGE_BREAK]
　面前逞能，竟然‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『你‥你胡說些什麼！
[PAGE_BREAK]
　再亂說的話，我可就‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0061]
『喂！你們兩個到底在吵些
[PAGE_BREAK]
　什麼！要吵的話，等進了
[PAGE_BREAK]
　地獄後再吵也不遲！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『你煩什麼煩！
[PAGE_BREAK]
　滾一邊去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『這傢伙討厭得很，索爾，
[PAGE_BREAK]
　我們不吵了，
[PAGE_BREAK]
　先合力宰了他再說！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好，就這麼辦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0061]
『可惡啊！這兩個小子全不把
[PAGE_BREAK]
　我放在眼裡！給我殺！
[PAGE_BREAK]
　一個都別放過！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0044]
『咦！又有海盜在此逞兇了，
[PAGE_BREAK]
　現在正是我們海防隊建功
[PAGE_BREAK]
　的良機！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『今天可真是熱鬧啊！
[PAGE_BREAK]
　你們又是來幹什麼的？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0044]
『我們是亞克斯王國的海岸巡
[PAGE_BREAK]
　防隊，消滅肆虐沿海的海盜
[PAGE_BREAK]
　本是我們的職責，這些海盜
[PARAGRAPH]
　就交給我們來處理，請各位
[PAGE_BREAK]
　放心！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『鬼扯，我們打的正高興，
[PAGE_BREAK]
　才不需要你們來攪局！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾啊，
[PAGE_BREAK]
　我們就要踏進人家的地盤，
[PAGE_BREAK]
　至少對人家客氣點吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0044]
『沒關係！
[PAGE_BREAK]
　我們這就來幫忙了！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_ID=0x0001]
『老爸，我‥我不行啦！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『小子，你先回屋子裡休息一
[PAGE_BREAK]
　下吧！這裡有老爸擋著！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『老爸，那我就先回去了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『你們這些該死的海盜，
[PAGE_BREAK]
　竟敢砍傷我兒子，
[PAGE_BREAK]
　我和你們拼了！』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0061]
『可恨啊！今日先碰上這群棘
[PAGE_BREAK]
　手的小子在先，又遇上王國
[PAGE_BREAK]
　海防隊，真是天亡我也！啊
[PAGE_BREAK]
　‥‥』
[END]
```

### Page 9

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『多謝老爹和您公子的幫忙，
[PAGE_BREAK]
　我們才能順利打敗海盜。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『那裡！我和小子住在這島上
[PAGE_BREAK]
　除了偶爾出去遊歷外，平常
[PAGE_BREAK]
　也閒來無事，幫你們打打海
[PARAGRAPH]
　盜，不算什麼啦！
[PAGE_BREAK]
　對了，說到這個，老頭子我
[PAGE_BREAK]
　倒有個請求。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『老爹您說說看。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『我瞧我這小子和你們滿合得
[PAGE_BREAK]
　來的，希望你們能帶他同行
[PAGE_BREAK]
　，讓他出去見識見識，歷練
[PARAGRAPH]
　一番。我從小教了他不少武
[PAGE_BREAK]
　術，當你們有麻煩的時候，
[PAGE_BREAK]
　相信他也可以幫上一點忙。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這個好像有點‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『哎！老爹您這個請求當然是
[PAGE_BREAK]
　沒問題的啦！悠妮妳說是不
[PAGE_BREAK]
　是？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『多一個同伴，
[PAGE_BREAK]
　路上也比較熱鬧啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『真是太謝謝各位了。
[PAGE_BREAK]
　小子啊，來向大家打個招呼
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『我‥我叫哈諾，
[PAGE_BREAK]
　以後請‥請大家多多指教
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『以後你就是我們的伙伴了。
[PAGE_BREAK]
　平時我們要好好相處，
[PAGE_BREAK]
　遇到危難時要同心協力，
[PAGE_BREAK]
　好嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『好‥好的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0003]
『漲潮的時間也差不多到了，
[PAGE_BREAK]
　你們可以準備上路了。
[PAGE_BREAK]
　我這小子就拜託你們啦！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『老爹您放心吧！
[PAGE_BREAK]
　我們走囉！』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『奈野啊捏？』
[END]
```

### Page 11

```text
[PORTRAIT_LEFT_BY_ID=0x0001]
『老爸！老爸！
[END]
```
