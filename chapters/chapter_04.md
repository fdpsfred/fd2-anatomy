# 第 4 章 — 塞拉村前

抄近路前往塞拉村途中遭強盜伏擊，混戰中有半獸人路人加入攪局。戰後一行人懷疑塞拉村可能出事，決定趕往查看。

## 加入角色

無新加入角色。

## 敵人配置

本章 FDFIELD entry 10 共 21 個會生成的 spawn 記錄（另有 19 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 28 | 盜賊 | LV5 | ×12 | default_attacker |
| 29 | 盜賊頭目 | LV6 | ×1 | default_attacker |
| 25 | 僧侶 | LV3 | ×2 | default_attacker |
| 22 | 魔法師 | LV3 | ×2 | default_attacker |
| 34 | 獸人 | LV6 | ×4 | default_attacker |

僧侶（enemy_data 25）帶治療術，可為盜賊方補血。

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：綠寶石 (0xC9)、草藥 (0xC0)、草藥 (0xC0)、闊劍 (0x01)、風精之羽 (0x60)、力量藥水 (0xC6)

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：回復劑 (0xC1)、綠寶石 (0xC9)、紅寶石 (0xCA)、魔法水 (0xCE)

## 商店

神秘商店 (Alt+F3) — 含風精之羽 $20000，hack table 觸發。

## 特殊機制

- **失敗條件**：主角索爾死亡即敗，由 default post-action handler `fd2_check_battle_end_default_handler` 判定，本章無自訂勝負條件。
- **過場章**：End handler `fd2_chapter_04_end`（61 B）純對話 + 推進章節，不加入新角色、不發結算 reward。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_04_init @ 0x00032FB2` | 181 B |
| End | `fd2_chapter_04_end @ 0x000231BC` | 61 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[3]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[3]` |  |

### Init handler

`fd2_init_battle_state_for_chapter` 後依序：

- `fd2_pan_cursor_and_window(4, 0xB)`
- `fd2_cutscene_event_trigger(0x14)` + `fd2_display_dialog_scene(page=0)`
- `fd2_load_chapter_portraits_and_dump_tmp(race_id=1)` + `fd2_pan_cursor_and_window(4, 0)`
- `fd2_display_dialog_scene(page=1)` + `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 4 | 0, 1 |
| End | 4 | 4 |

### char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫（init/end 皆無）。

### Cutscene events

- Init: `0x14` (1 event)

### Post-action handler

`data_fd2_chapter_post_action_handler_table[3]` 指向 `fd2_check_battle_end_default_handler`，無自訂勝負條件。

### End handler events

`fd2_chapter_04_end @ 0x000231BC` (61 B) — trivial：

1. `fd2_display_dialog_scene(page=4)`
2. `fd2_save_runtime_char_to_template`
3. `current_chapter_id += 1`

無 cutscene、無 reward、無加入。

## FDFIELD event script

FDFIELD entry idx **10**（= chapter_id × 3 + 1，chapter_id = 3），entry size 1171 bytes；party_member_count = 7、char_spawn_count = 40。header layout 見 `resource_info/fdfield.md`。

16 個 turn-event hook 中 1 個 active（其餘 15 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 4 | 1 (end_of_player_turn) | 0x0B | `0x00034565` | dialog_only; ch4_dialog |

## 對話

對話文字 7 pages 來自 FDTXT.DAT entry 4。Init handler 引用 page 0、1，End handler 引用 page 4。

### Page 0

```text
[PORTRAIT_LEFT_BY_ID=0x0008]
『快到賽拉村啦！抄這條近路
[PAGE_BREAK]
　很快就到了。怎樣？
[PAGE_BREAK]
　還是帶著我比較好吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『老是走這種小道，
[PAGE_BREAK]
　搞不好會碰上強盜喔！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『誰說的，這條路隱密的很，
[PAGE_BREAK]
　只有我才知道‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『看，前面好像有人呢！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0061]
『你們想到賽拉村嗎？
[PAGE_BREAK]
　你們是來幹什麼的？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『干你屁事！不要礙路！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0061]
『首領說得沒錯，果然有人抄
[PAGE_BREAK]
　小路來偷襲了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『什麼偷襲？把話講清楚！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0061]
『少裝了！死吧！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0018]
『嗷嗚！‥是誰在這裡大吵
[PAGE_BREAK]
　大鬧？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0019]
『吼‥好像是人類在打架。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x001A]
『呼‥好像很有趣，我們去
[PAGE_BREAK]
　打人少的那一邊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x001B]
『贊成！贊成！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0066]
『好可怕！不玩了！不玩了
[PAGE_BREAK]
　！』
[END]
```

### Page 4

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『奇怪了，在這大陸上到處
[PAGE_BREAK]
　都遇得上強盜！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『不對，
[PAGE_BREAK]
　賽拉村可能出事了！
[PAGE_BREAK]
　我們趕快趕過去看看！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『打了一場大戰，不休息一
[PAGE_BREAK]
　下嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『放心，途中有個小村落可
[PAGE_BREAK]
　以稍事休息。走吧！』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這‥‥這是什麼碗糕！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0061]
『哇‥啊‥‥！』
[END]
```
