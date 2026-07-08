# 第 28 章 — 探索者

由轉送站進入黃金城（第一空中要塞）內部，遭遇要塞防衛機甲部隊；由悠妮帶路前往黃金城核心區域，奪回中樞系統控制權以解除要塞大部份防衛部隊的戰鬥狀態。

## 加入角色

無新加入角色。

## 敵人配置

本章 FDFIELD entry 82 共 44 個會生成的 spawn 記錄（另有 16 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 48 | 機甲隊長 | LV33 | ×2 | aggressive_physical |
| 47 | 機甲守衛 | LV23 | ×2 | targeted_approach |
| 43 | 機甲兵 | LV23 | ×18 | aggressive_physical / default_attacker |
| 45 | 光束炮座 | LV18 | ×4 | aggressive_physical / default_attacker |
| 44 | 機甲射手 | LV23 | ×8 | default_attacker / aggressive_physical |
| 46 | 機甲突擊兵 | LV23 | ×10 | default_attacker / aggressive_physical |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：衝擊手臂 (0x4B)、神聖之水 (0xC3)、神聖之水 (0xC3)、神聖之水 (0xC3)、水晶粒 (0xCF)、神聖之水 (0xC3)、神聖之水 (0xC3)、神聖之水 (0xC3)

## 商店

無章內商店（battle 章，不走 intro 商店選單）。

## 特殊機制

- **Init 全清隊 20 chars + revive HP>0**：與 ch23 同設計但範圍更大（20 chars vs 16 chars）。init 先 `for i in [0, 0x14): fd2_mark_char_as_dead(i)` 把 20 個 slot 全標 dead，再 `for i in [0, 0x14): if chars[i].wHP_current != 0: chars[i].bFlags = 0` 復活實際在隊角色，HP=0 者不上場。與 ch23 不同：ch28 此處不設 sprite facing。
- **3× 重複 cutscene event 0x55**：init 對同一 `fd2_cutscene_event_trigger(0x55)` 呼叫三次，3 個並行 group 各執行一次同一 walk-animation script，模擬多支隊伍同時出場的 cinematic。
- **最小 end handler（40 B）**：30 章中最小的 end，僅 `fd2_display_dialog_scene(page=7)` + `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`，無 cutscene、無加入。
- **勝負條件**：標準 default（全敵死 = 勝、索爾死 = 負）＋ 額外 lose 條件 chars[1]（悠妮）死亡；判定在共用 post-action handler `fd2_chapter_22_27_28_post_action_shared`。
- **藍色平台左側火焰 tile-step 援軍**：踩到藍色平台左側火焰 tile 時，FDFIELD tile-step-event handler 動態 rewrite 該章 turn-event hook table 的 turn byte（0xFF → 下一回合），把原本 sentinel 的 entry 啟動，使下回合敵方 turn 觸發援軍 spawn。屬 tile-step → turn-event 連鎖。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_28_init @ 0x00033C9D` | 285 B |
| End | `fd2_chapter_28_end @ 0x00025464` | 40 B (最小 end) |
| Post-action | `fd2_chapter_22_27_28_post_action_shared @ 0x00020A87` | default + lose if char[1] dead (與 ch22/27 共用) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[27]` | |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[27]` | |

### Init handler

與 ch23 init 結構類似（清隊 + spell visual）但範圍更大（20 chars vs 16 chars）：

1. `fd2_init_battle_state_for_chapter`
2. **大規模清隊**：`for i in [0, 0x14): fd2_mark_char_as_dead(i)`（20 chars 全標 dead）
3. `fd2_pan_cursor_and_window(0x1D, 0xF)` + `fd2_cast_screen_wide_spell_with_fade(cursor+6, +5, 10, 8)`
4. **revive 過濾**：`for i in [0, 0x14): if chars[i].wHP_current != 0: chars[i].bFlags = 0`（與 ch23 不同：ch28 此處不設 sprite facing）
5. `fd2_composite_battle_frame` + `fd2_set_vga_palette_range_with_add(0, 0xFF, 0)` palette restore
6. 500ms wait
7. **3× `fd2_cutscene_event_trigger(0x55)`**：同一 event 觸發三次（3 個並行 group 各執行一次同 walk script）
8. `fd2_clear_all_chars_facing` + `fd2_display_dialog_scene(page=0)`
9. `fd2_cinematic_chapter_portrait_dump_with_white_flash(0, 0x10, 6)` + `fd2_cinematic_chapter_portrait_dump_with_white_flash(7, 0x10, 7)`：pan + load_chapter_portraits + palette flash + composite_battle_frame 過場 helper
10. `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 28 | 0 |
| End | 28 | 7 |

### char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫；本章不在 init 或 end 加入新角色。

### Cutscene events

- Init：`0x55`（1 unique event，呼叫 3 次）

### Post-action handler

`fd2_chapter_22_27_28_post_action_shared @ 0x00020A87`（與 ch22/27 共用）：

- 標準 default 判定（全敵死 = 勝、索爾死 = 負）
- **額外 lose 條件**：if chars[1] dead → `game_event_flag = 1`

本章 chars[1] = 悠妮（編成畫面 pin 表：chapter_id > 0x19 → char 9）。與 ch22/ch27 共用的是 post-action handler 的 slot-1 檢查結構，非同一角色。

### End handler events

`fd2_chapter_28_end @ 0x00025464`（40 B）— 30 章中最小 end：

1. `fd2_display_dialog_scene(page=7)`
2. `fd2_save_runtime_char_to_template`
3. `current_chapter_id += 1`

無 cutscene、無加入。

## FDFIELD event script

FDFIELD entry idx **82**（= chapter_id × 3 + 1，chapter_id=27），entry size 1691 bytes；party_member_count = 20、char_spawn_count = 60。header layout 見 `resource_info/fdfield.md`。

3 / 16 turn-event hooks 為動態啟動候選（其餘 13 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0 (enemy_turn_intro) | 0x42 | `0x000359C8` | sentinel-like，待 tile-step 啟動 |
| 0xFF | 0 (enemy_turn_intro) | 0x44 | `0x00035A48` | sentinel-like，待 tile-step 啟動 |
| 0xFF | 0 (enemy_turn_intro) | 0x46 | `0x00035B05` | sentinel-like，待 tile-step 啟動 |

`turn=0xFF` 不會等於回合計數，初始 hook table 不會 fire。實際觸發機制：tile-step-event handler 在某些劇本 tile 被踩到時，會動態 rewrite 該章 turn-event-hook table 的 turn byte（0xFF → data_fd2_battle_turn_counter / +1），把原本 sentinel 的 entry 啟動為下一回合 fire 的 event。「越過藍色平台左側火焰則己方結束時敵援軍」即屬此類 tile-step → turn-event 連鎖。

## 對話

對話文字 8 pages（page 0-7）來自 FDTXT.DAT entry 28。Init handler 引用 page 0（黃金城入口對白），End handler 引用 page 7（奪回中樞系統後撤離對白）；page 1-6 為戰場防衛部隊廣播與系統回報，由 FDFIELD turn-event / tile-step handler 於對應 turn 引用。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這裏就是黃金城嗎？好奇怪
[PAGE_BREAK]
　的建築，這哪像什麼城堡！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『說是一座金屬作成的要塞還
[PAGE_BREAK]
　差不多，從外面的雲海看來
[PAGE_BREAK]
　，我們好像是在很高的地方
[PARAGRAPH]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『這裡是用來傳送機甲兵團到
[PAGE_BREAK]
　地上用的轉送站，由此就可
[PAGE_BREAK]
　以進入黃金城內部。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『看來古代人必定擁有極高度
[PAGE_BREAK]
　的文明‥‥這種冶金和構築
[PAGE_BREAK]
　的技術，絕對不是現有的文
[PARAGRAPH]
　明或魔法所能做到的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『如果有時間的話，真希望能
[PAGE_BREAK]
　好好的研究一下此地。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『可惜我們沒什麼時間了，我
[PAGE_BREAK]
　怕那傢伙會啟動最終兵器「
[PAGE_BREAK]
　黑暗之焰」，無論如何我們
[PARAGRAPH]
　要趕快找到他才行‥‥』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0014]
『注意！發現並確認敵單位！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0015]
『一級警告！敵單位與ASR一07
[PAGE_BREAK]
　從轉送站進入C一01地區，本
[PAGE_BREAK]
　地區防衛部隊立刻進入防衛
[PARAGRAPH]
　體勢，座標0177一03一42。重
[PAGE_BREAK]
　複一次，敵單位已進入要塞
[PAGE_BREAK]
　，立刻採取防衛體勢。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0014]
『本區防衛隊立刻開始防衛行
[PAGE_BREAK]
　動，交戰並殲滅敵目標。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『有麻煩了，我們趕快解決這
[PAGE_BREAK]
　些防衛隊，在這裡作戰無險
[PAGE_BREAK]
　可守。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這些機甲衛兵還不都是一些
[PAGE_BREAK]
　老貨色，相信很快就可以解
[PAGE_BREAK]
　決掉。我們上吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我們身處敵境，大家要特別
[PAGE_BREAK]
　小心些！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『‥嚴重受損，指揮控制不能
[PAGE_BREAK]
　‥系統關閉‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『C一02地區，第一防衛隊抵達
[PAGE_BREAK]
　。轉送辨識及接戰資料。』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『C一02地區，第二防衛隊抵達
[PAGE_BREAK]
　。轉送辨識及接戰資料。』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『C一02地區，第三防衛隊抵達
[PAGE_BREAK]
　。轉送辨識及接戰資料。』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『轉送完成。目標及命令確認
[PAGE_BREAK]
　，防衛行動開始。』
[END]
```

### Page 7

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『大家都沒事吧？我們趕快離
[PAGE_BREAK]
　開這裡，要不然敵方防衛隊
[PAGE_BREAK]
　會不斷向這裡湧來。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『接下來要往哪裡呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『到黃金城的核心區域，我必
[PAGE_BREAK]
　須奪回中樞系統的控制權，
[PAGE_BREAK]
　這樣我就可以解除要塞中大
[PARAGRAPH]
　部份防衛部隊的戰鬥狀態。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『‥聽不懂。就照妳說的吧！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『嘻，聽不懂是正常的！我來
[PAGE_BREAK]
　帶路，往這邊走！』
[END]
```
