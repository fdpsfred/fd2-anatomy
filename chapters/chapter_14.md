# 第 14 章 — 平原的會戰

於蘭迪平原襲擊獸人駐囤部隊；審問俘虜後得知獸人首領薩卡正率另一隊在拉卡湖攻打豹人族，於是決定趕往支援。

## 加入角色

無加入。

## 敵人配置

本章 FDFIELD entry 40 共 52 個會生成的 spawn 記錄（另有 18 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 35 | 獸人隊長 | LV17 | ×8 | hard_skip |
| 34 | 獸人 | LV17 | ×25 | hard_skip |
| 20 | 黑暗射手 | LV7 | ×8 | hard_skip |
| 23 | 黑暗法師 | LV7 | ×5 | hard_skip |
| 26 | 黑暗僧侶 | LV7 | ×5 | hard_skip |
| 34 | 獸人 | LV5 | ×1 | hard_skip |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：閃電鎚 (0x26)

敵人掉落（擊殺帶有掉落的敵人可得）：

- 金錢：2000、5000、6000、8000

## 商店

story 章，intro 主選單提供武器店 / 道具店，另有以隱藏熱鍵（Ctrl+F4）開啟的神秘商店。品項（chapter_intro_metadata entry 13）：

- **武器店**：巨魔劍 (0x06)、惡魔之矛 (0x19)、血之斧 (0x25)、精靈弓 (0x30)、忍者裝 (0x89)、合金鎖甲 (0x94)、聖者袍 (0xA8)、鬥士服 (0xAE)
- **道具店**：回復劑 (0xC1)、再生藥 (0xC2)、解毒劑 (0xC4)、退麻藥 (0xC5)
- **神秘商店（Ctrl+F4）**：神聖之水 (0xC3)、水晶粒 (0xCF)

## 特殊機制

- **失敗條件**：索爾（char_id 0）死亡即敗。ch14 用 default post-action handler `fd2_check_battle_end_default_handler`，判定全敵死 = 勝、索爾死 = 敗。
- **位置觸發援軍**：攻略「當己方通過地圖中央一帶，則敵軍便會前來攻擊」是 FDFIELD tile-step / position-trigger 事件，由 char 踏上帶 tile event 的格子時走 `fd2_check_tile_event_post_action` 流程觸發，不在 post-action handler 內。
- **turn-event 完全靜態**：本章 FDFIELD 的 16 個 turn-event hook slot 全為 sentinel，無任何回合觸發事件；戰場動態全部由 init / post-action / FDFIELD char_spawn 與 tile-step 記錄處理。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_14_init @ 0x0003347C` | 29 B（30 章中第二小） |
| End | `fd2_chapter_14_end @ 0x000238DC` | 225 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[13]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[13]` |  |

### Init handler

極簡，僅四步：

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(0x14, 0x14)`
3. `fd2_display_dialog_scene(page=0)`
4. `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 14 | 0 |
| End | 14 | 2, 3 |

Page 1 由 FDFIELD event handler 引用（獸人陣營被驚醒時的位置觸發對白）。

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫；End handler 亦無——ch14 結算無新加入。

### Cutscene events

無 init cutscene；end 觸發 `0x2F`。

### Post-action handler

`data_fd2_chapter_post_action_handler_table[13]` 指向 `fd2_check_battle_end_default_handler`：全敵死 = win，索爾（char_id 0）死 = lose。攻略「當己方通過地圖中央一帶，則敵軍便會前來攻擊」屬 FDFIELD position-trigger event，不在 post-action handler 內。

### End handler events

`fd2_chapter_14_end @ 0x238DC`：

1. 從 scene tables（`data_fd2_chapter_ch14_end_scene_char_pos_x_table` / `_pos_y_table` / `_facing_table`，各 @0x52153/0x52163/0x52173）讀 4 chars 位置
2. `fd2_load_chapter_portraits_and_dump_tmp(1)`
3. `fd2_setup_chars_and_camera_for_intro(0xF, 0, 0, 0, 0, 0xC, 10)`
4. `fd2_display_dialog_scene(page=2)`
5. `fd2_cutscene_event_trigger(0x2F)`
6. `fd2_display_dialog_scene(page=3)`
7. `fd2_save_runtime_char_to_template`
8. `current_chapter_id += 1`

無 `fd2_init_runtime_char_from_base_growth`。

## FDFIELD event script

FDFIELD entry idx **40**（= chapter_id × 3 + 1, chapter_id = 13），entry size 1951 bytes，party_member_count = 16，char_spawn_count = 70。header layout 見 `resource_info/fdfield.md`。

turn-event hook 表：16 個 slot 全為 sentinel `(turn = 0xFF, event_code = 0xFF, phase = 0)`，無 active hook。本章 turn-based events 完全靜態，由 init / post-action / FDFIELD char_spawn records 處理。

## 對話

對話文字 4 pages 來自 FDTXT.DAT entry 14。Init handler 引用 page 0；End handler 依序引用 page 2 與 page 3；page 1 由 FDFIELD event handler（獸人被驚醒的位置觸發對白）引用。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x000C]
『看到了！
[PAGE_BREAK]
　那裡有一大群獸人，
[PAGE_BREAK]
　好像還在休息呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好極了，
[PAGE_BREAK]
　我們去叫醒牠們！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『別太猴急，小子！
[PAGE_BREAK]
　我們先小心接近，
[PAGE_BREAK]
　再伺機而動！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『好，我們上吧！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0020]
『吼嗚！
[PAGE_BREAK]
　注意，有人接近了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『通通給我醒來！開打了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『咦？對手呢？
[PAGE_BREAK]
　怎麼都是一堆小鬼頭‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0012]
『管他的，知道我們大軍在此
[PAGE_BREAK]
　集結而還敢接近的，
[PAGE_BREAK]
　一定不是一般的小鬼，
[PARAGRAPH]
　先打了再說！』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『你們來這裡幹什麼？
[PAGE_BREAK]
　想要命的話就趕快說！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0066]
『‥我‥我不知道啊，
[PAGE_BREAK]
　首領薩卡大人‥
[PAGE_BREAK]
　四天前命令我們在此集結，
[PARAGRAPH]
　準備‥準備攻打精靈族的都
[PAGE_BREAK]
　城哈斯米爾，
[PAGE_BREAK]
　有一隊伙伴已經先去了，
[PARAGRAPH]
　到現在還沒回來，結果‥
[PAGE_BREAK]
　結果你們就先來了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『笨蛋，那一隊早就被我們宰
[PAGE_BREAK]
　光了！那麼，你們的首領薩
[PAGE_BREAK]
　卡為何要攻打哈斯米爾？
[PARAGRAPH]
　趕快說清楚！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0066]
『‥這‥這‥我也不太清楚，
[PAGE_BREAK]
　只記得首領吩咐隊長的時候
[PAGE_BREAK]
　有提到要殺光精靈族以及務
[PARAGRAPH]
　必要搶到寶物，其它的我就
[PAGE_BREAK]
　沒聽清楚了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0011]
『什麼寶物？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0066]
『‥哎，我怎麼可能知道‥‥
[PAGE_BREAK]
　求你們饒了我吧，我保證不
[PAGE_BREAK]
　會回去向首領通風報信‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好吧，
[PAGE_BREAK]
　再回答我們一個問題，
[PAGE_BREAK]
　我們就放你走。
[PARAGRAPH]
　你們的首領薩卡現在
[PAGE_BREAK]
　在哪裡？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0066]
『首領啊‥他應該是率領另一
[PAGE_BREAK]
　隊到拉卡湖去攻打豹人族了
[PAGE_BREAK]
　如果沒錯的話，他應該還在
[PARAGRAPH]
　那一帶才對。
[PAGE_BREAK]
　‥‥我知道的都告訴你們了
[PAGE_BREAK]
　可以放我走了吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好，你可以走了！
[PAGE_BREAK]
　快滾吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0066]
『謝‥謝謝！』
[END]
```

### Page 3

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，你怎麼可以未經大家
[PAGE_BREAK]
　的同意就放他走呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『不要緊，我們需要的都已經
[PAGE_BREAK]
　知道了，不過情況好像更加
[PAGE_BREAK]
　麻煩了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『獸人族是吃了什麼熊心豹子
[PAGE_BREAK]
　膽，竟敢同時攻打精靈族和
[PAGE_BREAK]
　豹人族！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『我看啊，一定是有人在背後
[PAGE_BREAK]
　指使，不然獸人族那來這麼
[PAGE_BREAK]
　大的膽量。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000E]
『豹人族的戰鬥力在這大陸的
[PAGE_BREAK]
　幾個種族中雖然也是數一數
[PAGE_BREAK]
　二的，但這種規模的攻擊他
[PARAGRAPH]
　們也未必擋得住，我們還是
[PAGE_BREAK]
　先去拉卡湖看看吧，
[PAGE_BREAK]
　必要時還可以支援他們。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『拉卡湖離此地不遠，途中好
[PAGE_BREAK]
　像有個小村落，我們可以先
[PAGE_BREAK]
　在那裡休息一下。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那我們還等什麼？走吧！』
[END]
```
