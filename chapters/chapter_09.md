# 第 9 章 — 騎士的抉擇

希莉亞與禁衛軍隊長萊汀正面交鋒；戰後揭露葛雷大臣陰謀，國王被擄。

## 加入角色

無 init 加入。End handler 復活 char[11]（ch9 init 已預初始化但 marked dead 的角色）。

## 敵人配置

本章 FDFIELD entry 25 共 37 個會生成的 spawn 記錄（另有 23 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 50 | 萊汀 | LV18 | ×1 | aggressive_physical |
| 15 | 突擊騎兵 | LV14 | ×2 | aggressive_physical |
| 10 | 鎧甲武士 | LV8 | ×6 | aggressive_physical |
| 22 | 魔法師 | LV11 | ×6 | aggressive_physical |
| 25 | 僧侶 | LV11 | ×4 | aggressive_physical |
| 19 | 弓箭手 | LV12 | ×4 | aggressive_physical |
| 15 | 突擊騎兵 | LV16 | ×1 | default_attacker |
| 14 | 騎兵 | LV14 | ×4 | default_attacker |
| 10 | 鎧甲武士 | LV10 | ×6 | default_attacker |
| 19 | 弓箭手 | LV14 | ×2 | default_attacker |
| 8 | 士兵 | LV5 | ×1 | default_attacker |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：生命之實 (0x5E)、魔力水晶 (0x5F)、回復劑 (0xC1)、解毒劑 (0xC4)、精靈披風 (0x82)
- 金錢：5000

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：回復劑 (0xC1)、速度藥水 (0xC8)、紅寶石 (0xCA)、魔法水 (0xCE)
- 金錢：1000

## 商店

story 章，intro 主選單提供武器店 / 道具店，另有以隱藏熱鍵（Alt+F9）開啟的神秘商店。品項（chapter_intro_metadata entry 8）：

- **武器店**：巨劍 (0x03)、戰鎚 (0x23)、黑暗弓 (0x2E)、巨鎚 (0x36)、鐵爪 (0x3F)、魔法皮甲 (0x87)、鱗甲 (0x92)、祭師袍 (0xA7)
- **道具店**：草藥 (0xC0)、回復劑 (0xC1)、解毒劑 (0xC4)
- **神秘商店（Alt+F9）**：生命之實 (0x5E)、魔力水晶 (0x5F)

## 特殊機制

- **Init 強制面朝 north**：`fd2_chapter_09_init` 進入對話前，迴圈把前 11 個 char（`runtime_char_array[0..0xA]`）的 sprite facing 全設為 2 (north)，作為章首 cutscene 排隊的視覺效果。
- **萊汀被打敗援軍出場（reinforcement state machine）**：攻略「萊汀被打敗時敵方騎兵援軍立即出現在上方」由 FDFIELD turn-event hook 觸發，落到 `fd2_chapter_event_handler_1f__ch9_reinforcement @ 0x34B5D`。此 handler 每次被觸發時，讀當前 `tile_event_consumed_flags[0x10]` 當 race_id 餵給 `fd2_load_chapter_portraits_and_dump_tmp` 載入對應波次援軍，接著把該 flag 遞增 1，形成一個從 race_id = 0 起算、逐波推進的援軍 state machine。起始值 `tile_event_consumed_flags[0x10] = 0` 由 `fd2_init_battle_state_for_chapter @ 0x205DA` 內的 `memset(tile_event_consumed_flags, 0, 0x20)` 統一清 0（全章共用），`fd2_chapter_09_init` 本身不對此 flag 設值。
- **勝負條件**：default — 全敵死 = 勝、索爾（char_id 0）死 = 負。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_09_init @ 0x0003327D` | 174 B |
| End | `fd2_chapter_09_end @ 0x000235BC` | 61 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[8]` | |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[8]` | |

### Init handler

1. `fd2_init_battle_state_for_chapter`
2. 迴圈：`runtime_char_array[0..0xA].pSprite_state[1] = 2` — 前 11 個 char 全部面朝 north
3. `fd2_pan_cursor_and_window(6, 0)`
4. `fd2_display_dialog_scene(page=0)` + `fd2_cutscene_event_trigger(0x23)`
5. `fd2_display_dialog_scene(page=1)`
6. `fd2_pan_cursor_to_char(0)` + `fd2_clear_all_chars_facing`

### Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 9 | 0, 1 |
| End | 9 | 4 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。End handler 透過 `runtime_char_array[0xB].bFlags = 0` 復活 char[11]（非新加入；ch9 init 已預初始化但 marked dead）。

### Cutscene events

`0x23` (init)、`0x24` (end)。每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id] @ 0x627D8` 的 walk-animation script。

### Post-action handler

`data_fd2_chapter_post_action_handler_table[8]` 指向 `fd2_check_battle_end_default_handler` — 全敵死 = win，索爾（char_id 0）死 = lose。攻略「萊汀被打敗時敵方騎兵援軍立即出現」由 FDFIELD event 處理，不在 post-action handler。

### End handler events

`fd2_chapter_09_end @ 0x235BC`：

1. `runtime_char_array[0xB].bFlags = 0` — 復活 char[11]（清死亡 flag）
2. `fd2_pan_cursor_and_window(6, 1)` + `fd2_load_chapter_portraits_and_dump_tmp(4)`
3. `fd2_cutscene_event_trigger(0x24)`
4. `fd2_display_dialog_scene(page=4)`
5. `fd2_save_runtime_char_to_template`
6. `current_chapter_id += 1`

無 `fd2_init_runtime_char_from_base_growth` — char[11] 是 revive 而非新加入。

## FDFIELD event script

FDFIELD entry idx **25**（= chapter_id × 3 + 1, chapter_id = 8），entry size 1691 bytes，party_member_count = 11，char_spawn_count = 60。header layout 見 `resource_info/fdfield.md`。

turn-event hook table：2 / 16 hook entries active（其餘 14 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0 (enemy_turn_intro) | 0x1F | `0x00034B5D` | turn=0xFF dormant entry — 不會在初始 hook table 狀態下 fire |
| 0xFF | 0 (enemy_turn_intro) | 0x1F | `0x00034B5D` | 同上 dormant entry |

兩筆 hook 皆指向 `fd2_chapter_event_handler_1f__ch9_reinforcement @ 0x34B5D`（援軍 state machine，見 §特殊機制）。`fd2_fire_chapter_turn_events_for_phase` 比對 `turn == data_fd2_battle_turn_counter`，0xFF 永遠不會等於回合計數，故初始狀態下不 fire。觸發機制：兩筆 turn-event hook 初始 turn=0xFF（dormant），`fd2_chapter_event_handler_1f__ch9_reinforcement` 實際經 shared `consequence_table[0x1F]` 觸發（ch9 的 tile-step-event 表 16 筆全為 sentinel，無 ch9 專屬 tile-step handler 改寫 turn byte；arm 者為 kill-drop handler `fd2_chapter_event_handler_1e__ch9_laiting_defeat @ 0x34A7A`——萊汀陣亡時經 kill-drop 路徑改寫這兩筆 hook 的 turn byte（+3/+6），使 0x1F 下一回合 fire。故 ch9 也有執行期改寫 turn byte 的機制，只是經 kill-drop 路徑而非 tile-step，並非僅 ch27/28/29 才有）。

## 對話

對話文字共 5 pages（page 0–4），來自 FDTXT.DAT entry 9（entry = chapter_id + 1）。Init handler 引用 page 0、1，End handler 引用 page 4。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『希莉亞，看來妳父王的大軍
[PAGE_BREAK]
　已經在等我們了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『放心，交給我就是了。』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『殿下，以您王國公主的身份
[PAGE_BREAK]
　怎可和這些盜匪為伍？
[PAGE_BREAK]
　陛下已下令逮捕這幫盜賊，
[PARAGRAPH]
　讓屬下迎接殿下回宮，這些
[PAGE_BREAK]
　盜匪交給禁衛軍處理就可以
[PAGE_BREAK]
　了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『萊汀，我一直以為你是個聰
[PAGE_BREAK]
　明人，難道連你也不相信我
[PAGE_BREAK]
　嗎？趕快把這些禁衛軍撤走
[PARAGRAPH]
　我要去見父王，把事情說個
[PAGE_BREAK]
　清楚。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『公主殿下，陛下有令在先，
[PAGE_BREAK]
　把這幫匪徒通通抓起來再說
[PAGE_BREAK]
　，如果您還是要護著他們的
[PARAGRAPH]
　話，那屬下就得罪了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『萊汀，你敢對我動手！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『公主殿下，
[PAGE_BREAK]
　請原諒屬下的無禮。
[PARAGRAPH]
　來人，把這群盜匪都給我抓
[PAGE_BREAK]
　起來！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『唔‥公主殿下，您為何‥
[PAGE_BREAK]
　為何要帶這些人來加害
[PAGE_BREAK]
　陛下‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『萊汀，我怎麼會想要加害
[PAGE_BREAK]
　父王！宮中到底發生了
[PAGE_BREAK]
　什麼事？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『‥前幾天有刺客夜襲陛下的
[PAGE_BREAK]
　寢宮，想要抓走陛下，幸好
[PAGE_BREAK]
　打鬥聲驚動了侍衛，
[PARAGRAPH]
　衛兵衝進來救駕，陛下才倖
[PAGE_BREAK]
　免於難‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『有這種事？！說下去！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『刺客臨走前還揚言，
[PAGE_BREAK]
　幾天之內一定會再來‥‥
[PAGE_BREAK]
　陛下遭到這種驚嚇，
[PARAGRAPH]
　幾天來一直在床上休養‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這件事又為何會扯到我們
[PAGE_BREAK]
　身上？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『為了搜查可疑份子，我們在
[PAGE_BREAK]
　王國各地派出調查隊，結果
[PAGE_BREAK]
　又發現了不少怪事，例如普
[PARAGRAPH]
　里茲港的失蹤事件，而你們
[PAGE_BREAK]
　又惹上了調查隊，結果‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『大臣們一致認為我們就是刺
[PAGE_BREAK]
　客？這太荒謬了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『葛雷大人相當支持這種說法
[PAGE_BREAK]
　所以命令我們禁衛軍派兵攻
[PAGE_BREAK]
　擊你們‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『不是父王下令的嗎？怎麼又
[PAGE_BREAK]
　變成葛雷了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『陛下多日來一直臥病在床，
[PAGE_BREAK]
　軍令方面都是經由葛雷大人
[PAGE_BREAK]
　發佈‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0022]
『禁衛軍隊長萊汀，想不到你
[PAGE_BREAK]
　竟敢和盜匪勾結，我們奉葛
[PAGE_BREAK]
　雷大人之命將你逮捕！
[PARAGRAPH]
　趕快束手就擒吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『可惡，我明白了，葛雷這個
[PAGE_BREAK]
　該死的叛徒‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『萊汀，照這樣看來，父王處
[PAGE_BREAK]
　境危險，我們先設法殺進去
[PAGE_BREAK]
　再說！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『殿下，屬下被奸人所騙，
[PAGE_BREAK]
　有負保護陛下的使命，
[PAGE_BREAK]
　請讓屬下和你們一起去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『你受傷這麼重，還是先‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『喂！希莉亞，現在情況緊急
[PAGE_BREAK]
　我看這傢伙蠻能打的，叫瑪
[PAGE_BREAK]
　琳用法術給他治一下傷就可
[PARAGRAPH]
　以上陣了，應該可以多少幫
[PAGE_BREAK]
　上點忙吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『索爾，你說話也客氣點‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『公主殿下，請給屬下一個將
[PAGE_BREAK]
　功贖罪的機會！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這不就好了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『好吧，容我再借重一次大家
[PAGE_BREAK]
　的力量！我們上！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x004C]
『不得了了！葛雷大人被殺了
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_CHAR=0x000B]
『把話講清楚一點！倒底是怎
[PAGE_BREAK]
　麼一回事？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x004C]
『我‥我是葛雷大人房外的衛
[PAGE_BREAK]
　兵，剛才忽然聽到房內傳出
[PAGE_BREAK]
　一陣慘叫聲，我衝進去一看
[PARAGRAPH]
　葛雷大人已經倒在地上不省
[PAGE_BREAK]
　人事了‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x000B]
『有這種事！公主殿下，看來
[PAGE_BREAK]
　王城內情勢險惡，我先進去
[PAGE_BREAK]
　探查一番，再回來向殿下報
[PARAGRAPH]
　告！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『小心點！』
[END]
```
