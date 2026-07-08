# 第 24 章 — 在天空的彼方

飛行岩穿越大陸西北部「惡魔之山」一帶天空，遭龍人族空中部隊攔截；擊退守軍後飛行岩高度急速下降、失控降落火焰之谷。

## 加入角色

無新加入角色。

## 敵人配置

本章 FDFIELD entry 70 共 60 個會生成的 spawn 記錄（另有 10 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 41 | 惡魔 | LV11 | ×15 | default_attacker |
| 41 | 惡魔 | LV10 | ×1 | default_attacker |
| 38 | 龍人戰士 | LV23 | ×32 | default_attacker |
| 18 | 龍騎士 | LV21 | ×4 | default_attacker |
| 39 | 龍人法師 | LV22 | ×4 | default_attacker |
| 42 | 大惡魔 | LV12 | ×4 | default_attacker |

援軍於第 2、4、7、10 回合敵方 turn intro 從地圖四個角落出現（波次時序見 §FDFIELD event script；四個角落座標即 init 開場鏡頭巡場的四點，詳 §特殊機制）。

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：風精之羽 (0x60)

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：流水劍 (0x08)、龍之槍 (0x1B)、生命之實 (0x5E)、魔力水晶 (0x5F)、風精之羽 (0x60)、力量鎖甲 (0x99)、地獄鎧甲 (0xA0)、再生藥 (0xC2)、神聖之水 (0xC3)、力量藥水 (0xC6)、耐力藥水 (0xC7)、速度藥水 (0xC8)、水晶粒 (0xCF)
- 金錢：1000、2000、3000

## 商店

無章內商店。

## 特殊機制

- **Init 四段鏡頭巡場**：開場後鏡頭依序 pan 過 (0,4)/(0,0x16)/(0x1A,0x18)/(0x1A,2) 四個地圖角落，每停 400ms（`fd2_pan_cursor_and_window`）。這四個座標即為 FDFIELD turn-event 援軍的四個 spawn 角落，init 先讓玩家看到將在何處遭遇援軍。
- **結尾文字捲動 cinematic**：本章是 FD2 唯一在結尾使用文字向上捲動 + palette fade-out 的章節，`fd2_scroll_text_screen_up_by_lines` 在全遊戲中只被 `fd2_chapter_24_end` 呼叫一次，視覺上呈現飛行岩高度急速下降。
- **勝負條件**：走 default handler（`fd2_check_battle_end_default_handler`），全敵死 = 勝；索爾（chars[0]）死 = 負。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_24_init @ 0x000338C4` | 166 B |
| End | `fd2_chapter_24_end @ 0x00024C1E` | 260 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[23]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[23]` |  |

### Init handler

4-stage map preview camera tour：

1. `fd2_init_battle_state_for_chapter`
2. `fd2_display_dialog_scene(page=0)`
3. `fd2_load_chapter_portraits_and_dump_tmp(1)`
4. **4× pan + 400ms delay**：
   - `fd2_pan_cursor_and_window(0, 4)` + 400ms
   - `fd2_pan_cursor_and_window(0, 0x16)` + 400ms
   - `fd2_pan_cursor_and_window(0x1A, 0x18)` + 400ms
   - `fd2_pan_cursor_and_window(0x1A, 2)` + 400ms
5. `fd2_display_dialog_scene(page=1)` + `fd2_pan_cursor_to_char(0)`

四個 pan 座標即為 FDFIELD turn-event 援軍的四個 spawn 角落，init 階段預先讓玩家看到。

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 24 | 0, 1 |
| End | 24 | 2, 3 |

### char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫；本章不在 init 或 end 加入新角色。

### Cutscene events

無 `fd2_cutscene_event_trigger`；init 用 4× pan + 400ms delay 取代 cutscene event walk-animation。

### Post-action handler

`data_fd2_chapter_post_action_handler_table[23]` 指向 `fd2_check_battle_end_default_handler`，無自訂勝負條件：

- 全敵死 → win
- 索爾 (chars[0]) 死 → lose

### End handler events

`fd2_chapter_24_end @ 0x00024C1E` (260 B) — text-scroll cinematic 結尾（FD2 唯一）：

1. `fd2_display_dialog_scene(page=2)`
2. **Phase 1 文字向上捲動**：迴圈 `for line=2..9`：
   - `fd2_scroll_text_screen_up_by_lines(line)`
   - 內層 30 frames：`fd2_composite_battle_frame(1)` + `fd2_wait_n_bios_ticks(1)`
3. `fd2_display_dialog_scene(page=3)`
4. **Phase 2 文字向上捲動 + palette fade**：迴圈 `for line=9..14`：
   - `fd2_scroll_text_screen_up_by_lines(line)`
   - 內層 12 frames：`fd2_set_vga_palette_range(0, 0xFF, brightness_sub)` 漸暗 + `fd2_composite_battle_frame(0)` + `wait` + `brightness_sub++`
   - 結束時 brightness_sub = 12 × 5 = 60 → 全暗
5. `memset(0xA0000, 0, 64000)` — 黑屏
6. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

`fd2_scroll_text_screen_up_by_lines` 在全 FD2 中只在 ch24_end 呼叫一次。

## FDFIELD event script

FDFIELD entry idx **70**（= chapter_id × 3 + 1，chapter_id = 23），entry size 1951 bytes。party_member_count = 16，char_spawn_count = 70。header layout 見 `resource_info/fdfield.md`。

4 / 16 active turn-event hooks（其餘 12 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 2 | 0 (enemy_turn_intro) | 0x36 | `0x0003535D` | 援軍 reinforcement (4 角落) |
| 4 | 0 (enemy_turn_intro) | 0x36 | `0x0003535D` | 援軍 reinforcement |
| 7 | 0 (enemy_turn_intro) | 0x36 | `0x0003535D` | 援軍 reinforcement |
| 10 | 0 (enemy_turn_intro) | 0x36 | `0x0003535D` | 援軍 reinforcement |

## 對話

對話文字 4 pages 來自 FDTXT.DAT entry 24。Init handler 引用 page 0、1；End handler 引用 page 2、3。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『啊，無聊得要命‥到底還要
[PAGE_BREAK]
　飛多久啊！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『真神奇，我們一天內就飛過
[PAGE_BREAK]
　了大陸西北部‥從方向看來
[PAGE_BREAK]
　，現在應該是在前往邊界的
[PARAGRAPH]
　山脈吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『你是說「惡魔之山」嗎？那
[PAGE_BREAK]
　一帶全是未熄滅的火山啊！
[PAGE_BREAK]
　聽說那裡也是惡魔族的根據
[PARAGRAPH]
　地‥難道惡魔族也和這件事
[PAGE_BREAK]
　有關？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『那倒不奇怪，惡魔族本來就
[PAGE_BREAK]
　棲息在黑暗和地獄般的炎熱
[PAGE_BREAK]
　中，和其他種族向不往來，
[PARAGRAPH]
　誰曉得它們暗地裡會搞出些
[PAGE_BREAK]
　什麼！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『瞧，有人來迎接我們了！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『那‥那是什麼？飛行的大岩
[PAGE_BREAK]
　石，上面有人？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『笨！那是魔法驅動的飛行裝
[PAGE_BREAK]
　置，那些人是乘坐在上面的
[PAGE_BREAK]
　乘客！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0012]
『他們想幹什麼？想突破這裡
[PAGE_BREAK]
　的空域前往火焰之谷嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0013]
『陛下吩咐過我們，絕不可讓
[PAGE_BREAK]
　任何人從空中或地下經過此
[PAGE_BREAK]
　地的！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『所以現在我們要宰了那些傢
[PAGE_BREAK]
　伙！上吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『哇！對方來意不善呢！好像
[PAGE_BREAK]
　想要動手了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『要在這種地方打嗎？好像對
[PAGE_BREAK]
　我們很不利吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0014]
『我們的伙伴中還有幾個會飛
[PAGE_BREAK]
　的，勉強可以牽制敵方的攻
[PAGE_BREAK]
　勢，你們只要在地上確實消
[PARAGRAPH]
　滅敵人就可以了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『對方不過是會飛而已，有什
[PAGE_BREAK]
　麼好怕的！我們先打了再說
[PAGE_BREAK]
　！』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『敵軍似乎是負責鎮守這一帶
[PAGE_BREAK]
　的空域，以防止他人侵入‥
[PAGE_BREAK]
　看來在惡魔之山一定有什麼
[PAGE_BREAK]
　事發生，大家要有心理準備
[PAGE_BREAK]
　，準備面對非常的狀況‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『約拿老頭，我們的高度好像
[PAGE_BREAK]
　開始下降了耶！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0014]
『的確，而且越來越快了‥‥
[PAGE_BREAK]
　』
[END]
```

### Page 3

```text
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哇啊啊啊！索爾，我好怕！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『希莉亞，抓緊我！悠妮，妳
[PAGE_BREAK]
　也過來！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥好！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『‥‥‥！啊，亞雷斯，
[PAGE_BREAK]
　我頭好昏，‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『不要緊吧？來，抓緊我！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0005]
『謝謝‥，亞雷斯，你真好‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『約拿老頭啊，我就說嘛，現
[PAGE_BREAK]
　在的女孩子要比以前大膽多
[PAGE_BREAK]
　了，連現在這種狀況都可以
[PARAGRAPH]
　拿來作調情的機會，看來我
[PAGE_BREAK]
　們都落伍嘍！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『希爾法，如果你不想被她們
[PAGE_BREAK]
　一腳踹下這飛行岩的話，我
[PAGE_BREAK]
　建議你在我們著陸前最好保
[PARAGRAPH]
　持安靜‥‥』
[END]
```
