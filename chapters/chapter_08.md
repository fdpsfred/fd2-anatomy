# 第 8 章 — 王城前的戰鬥

抵達王城前遭駐軍誤判為亂黨圍攻，希莉亞出示「女神之淚」表明公主身份，駐軍隊長仍奉王命下令進攻；隊長麾下騎士洛娜拒抗王命、倒戈投靠隊伍；「女神之淚」首飾於此章首次登場。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 5 | 洛娜 | End handler 末段 | 章末固定加入 |

## 敵人配置

本章 FDFIELD entry 22 共 39 個會生成的 spawn 記錄（另有 21 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。城門為主戰場，敵騎兵援軍於 turn 2-7 每回合投入 2 名。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 15 | 突擊騎兵 | LV15 | ×1 | aggressive_physical |
| 10 | 鎧甲武士 | LV8 | ×5 | aggressive_physical |
| 22 | 魔法師 | LV11 | ×6 | aggressive_physical |
| 25 | 僧侶 | LV11 | ×4 | aggressive_physical |
| 19 | 弓箭手 | LV13 | ×2 | aggressive_physical |
| 14 | 騎兵 | LV12 | ×20 | default_attacker |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：耐力藥水 (0xC7)、再生藥 (0xC2)、風精之羽 (0x60)
- 金錢：3000、2500

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：巨劍 (0x03)、回復劑 (0xC1)、紅寶石 (0xCA)、魔法水 (0xCE)
- 金錢：2200、2800、3000

## 商店

無特別記載（攻略本未提此章神秘商店）。

## 特殊機制

- **失敗條件**：主角索爾陣亡即戰敗，由 `fd2_check_battle_end_default_handler`（default
  post-action handler）判定；本章無自訂勝負條件。
- **每回合騎兵援軍**：城門守軍由 FDFIELD turn-event hook（event_code `0x1B`，handler
  `0x000349D9`）在 turn 2-7 的 enemy_turn_intro 各觸發一次，共 6 波、每波 2 名敵騎兵。
  此援軍鏈全由 FDFIELD turn-event hooks 驅動，不經 post-action handler。
- **章末加入洛娜**：End handler 末段呼叫 `fd2_init_runtime_char_from_base_growth(5)`，
  將洛娜（char_id 5）加入隊伍。
- **Fade-to-black 轉場**：End handler 收尾以 `fd2_set_vga_palette_range(0, 0xFF, 0x40)`
  壓暗調色盤，再 `memset(0xA0000, 0, 64000)` 清空整個 framebuffer，構成章末黑屏淡出。
- **跨章劇情**：希莉亞的亞克斯王國公主身份於本章揭曉；「女神之淚」首飾作為信物道具首次
  登場，後續章節有相關劇情。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_08_init @ 0x00033219` | 100 B |
| End | `fd2_chapter_08_end @ 0x000234BB` | 140 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[7]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[7]` |  |

### Init handler

`fd2_init_battle_state_for_chapter` 後依序執行，呈對稱結構（兩段 pan + cutscene + dialog）：

- `fd2_pan_cursor_and_window(7, 0x20)` + `fd2_cutscene_event_trigger(0x1F)` + `fd2_display_dialog_scene(page=0)`
- `fd2_pan_cursor_and_window(7, 0x17)` + `fd2_cutscene_event_trigger(0x20)` + `fd2_display_dialog_scene(page=1)`
- `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 8 | 0, 1 |
| End | 8 | 3, 4 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 末段 `fd2_init_runtime_char_from_base_growth(5)` → 洛娜加入。

### Cutscene events

- Init：`0x1F, 0x20`（2 events）
- End：`0x21, 0x22`（2 events）

### Post-action handler

`data_fd2_chapter_post_action_handler_table[7]` 指向 `fd2_check_battle_end_default_handler`，
無自訂勝負條件。每回合敵騎兵援軍 6 波由 FDFIELD turn-event hooks 處理，**不是**
post-action handler。

### End handler events

`fd2_chapter_08_end @ 0x000234BB`（140 B）：

1. 從 `data_fd2_chapter_ch08_end_scene_char_pos_x_table` / `pos_y_table`（@ 0x520FC/0x52106，各 4 entries）讀 4 chars 位置
2. `fd2_setup_chars_and_camera_for_intro(...)`
3. `fd2_display_dialog_scene(page=3)`
4. `fd2_cutscene_event_trigger(0x21)`
5. `fd2_display_dialog_scene(page=4)`
6. `cutscene_event_state = 1` + `fd2_cutscene_event_trigger(0x22)` + `cutscene_event_state = 0`
7. `fd2_set_vga_palette_range(0, 0xFF, 0x40)` — palette darken
8. `memset(0xA0000, 0, 64000)` — 清空整個 framebuffer（黑屏 fade）
9. `fd2_init_runtime_char_from_base_growth(5)` — char 5 = 洛娜加入
10. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

末段 framebuffer clear 是 fade-to-black 轉場效果。

## FDFIELD event script

FDFIELD entry idx **22**（= chapter_id × 3 + 1，chapter_id = 7），entry size 1691 bytes，
party_member_count = 10，char_spawn_count = 60。header layout 見 `resource_info/fdfield.md`。

turn-event hooks 共 7/16 active（其餘 9 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 2 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement（turn 2-7 phase 0 各 fire 一次 = 每回合敵騎兵援軍 6 波×2 名） |
| 3 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 4 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 5 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 6 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 7 | 0 (enemy_turn_intro) | 0x1B | `0x000349D9` | ch8 reinforcement |
| 15 | 0 (enemy_turn_intro) | 0x1C | `0x00034A0E` | ai_setup; ch8_ai_ctrl |

## 對話

對話文字共 5 pages，來自 FDTXT.DAT entry 8。Init handler 播 page 0（王城守軍誤判圍攻、
希莉亞出示「女神之淚」表明公主身份、隊長仍下令進攻）與 page 1（洛娜宣示效忠公主、隊長
斥其違命）；End handler 播 page 3（洛娜自陳史卡迪家世代護王、隨希莉亞進城）與 page 4
（索爾一行議定跟進王城）。page 2（隊長陣亡遺言）不由 init 或 end handler 直接引用。

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
