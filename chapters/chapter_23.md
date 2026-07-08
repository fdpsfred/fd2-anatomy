# 第 23 章 — 向天空之旅

由傳送魔法失準降落到大陸西北沙漠綠洲的廢墟，眾人巧遇正在考古的武聖卡里斯與劍聖羅德曼；擊敗守護遺跡的古代機兵後，以傳送法杖啟動飛行岩，載眾人升空飛向天空（傳說中的黃金城是 page 8 提到的目的地線索，惟飛行岩實際去向連眾人亦不確定）。這是 30 章中規模最大的一章 (init 548 B、end 960 B 皆居各章之冠)，且是唯一在章內 mid-handler 重載 FDFIELD 進入第二戰場的章節。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 22 (0x16) | 武聖卡里斯 | end handler | 隊伍持有「天空之鑰」(item 100) |
| 19 (0x13) | 劍聖羅德曼 | end handler | 蜜蒂 (template id 0x12) 不在隊伍 且 戰鬥於 15 回合內結束 |

## 敵人配置

本章 FDFIELD entry 67 共 70 個會生成的 spawn 記錄。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 48 | 機甲隊長 | LV22 | ×1 | aggressive_physical |
| 47 | 機甲守衛 | LV17 | ×2 | aggressive_physical |
| 43 | 機甲兵 | LV17 | ×19 | aggressive_physical |
| 44 | 機甲射手 | LV17 | ×2 | aggressive_physical |
| 44 | 機甲射手 | LV19 | ×6 | default_attacker |
| 46 | 機甲突擊兵 | LV19 | ×6 | default_attacker |
| 44 | 機甲射手 | LV21 | ×6 | default_attacker |
| 46 | 機甲突擊兵 | LV21 | ×6 | default_attacker |
| 44 | 機甲射手 | LV23 | ×6 | default_attacker |
| 46 | 機甲突擊兵 | LV23 | ×6 | default_attacker |
| 44 | 機甲射手 | LV25 | ×8 | default_attacker |

機甲隊長 (enemy_data 48) 為 boss，對應 runtime char 0x12（勝負判定見 §Post-action handler）。end handler 中段 `fd2_load_dat_resource("FDFIELD.DAT", 0x45)` 載入第二戰場新地圖（詳 §特殊機制）。

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：破魔槍 (0x1C)、光之斧 (0x28)、封魔弓 (0x32)、黑暗杖 (0x3B)、裂刃爪 (0x45)、黑暗之衣 (0x8D)、黑暗之衣 (0x8D)、黑暗鱗甲 (0x98)、黑暗鱗甲 (0x98)、重鎧甲 (0x9F)、天之袍 (0xAB)、黑暗之袍 (0xAA)、龍鱗甲 (0x9B)

天空之鑰 (item 0x64/100) 為跨章兌換鏈關鍵物；其取得與跨章影響見本章 §特殊機制及 `chapters/_index.md` 天空之鑰段。

## 商店

無章內商店（battle 章，不走 intro 商店選單）。

## 特殊機制

- **開場 cinematic (init handler)**：30 章中規模最大的開場。init 先 `fd2_mark_char_as_dead` 逐一清掉全隊 16 名角色 (i in [0, 0x10))，再由 `fd2_cast_screen_wide_spell_with_fade` 播放螢幕級傳送魔法特效，接著以 revive 過濾只復活原本 `wHP_current != 0` 的角色 (清 bFlags、面朝北)；HP=0 的角色不上場。
- **三重 conditional join (end handler)**：
  - `fd2_any_char_has_item(100)` 持有天空之鑰 → `fd2_init_runtime_char_from_base_growth(0x16)` 卡里斯加入；無 → 卡里斯離隊不加入 (cutscene 0x47)。
  - `fd2_find_template_char_by_id(0x12)` 蜜蒂仍在隊伍 → `fd2_mark_char_as_dead(0x11)` 標記羅德曼欄位陣亡 (羅德曼不加入)。
  - 蜜蒂不在 且 `data_fd2_battle_turn_counter < 0xF` (15 回合內取勝) → `fd2_init_runtime_char_from_base_growth(0x13)` 羅德曼加入；蜜蒂不在 但 ≥ 15 回合 → `fd2_mark_char_as_dead(0x11)` (羅德曼因逾時不加入)。
- **跨章天空之鑰兌換鏈**：本章為「持鑰」的第一個分支點 (卡里斯加入)，下一個分支點在 ch27 (GOOD/BAD ending fork，進 ch28+)。天空之鑰 (item 100) 於 ch21 由 6 件物品集齊兌換取得。
- **章內兩段戰鬥 (mid-handler FDFIELD reload)**：end handler Phase 1 判定加入並 `fd2_save_runtime_char_to_template` 後，`current_chapter_id` 由 22 遞增為 23；Phase 2 隨即以 `fd2_load_dat_resource("FDFIELD.DAT", 0x45)` (= ch24 tile_map) 與 `FDSHAP.DAT` 0x2E/0x2F 載入第二戰場，播放飛行岩起飛 cinematic。第二戰場沿用 Phase 1 已遞增的 chapter_id 23，handler 內不再二次遞增。ch23 是 30 章中唯一在章內 mid-handler reload FDFIELD 的章節 (詳 `resource_info/fdfield.md`)。
- **勝負條件**：`fd2_chapter_23_post_action` bypass default — chars[0] 索爾 / chars[1] 希爾法 / chars[0x10] 卡里斯 / chars[0x11] 羅德曼 任一死 = 負；chars[0x12] 機甲隊長死 = 勝。
- **援軍 reinforcement**：第 13、15、18、22 回合的 enemy_turn_intro (phase 0) 觸發 turn-event 0x34 (handler `0x000352E2`) 載入援軍。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_23_init @ 0x000336A0` | 548 B (最大 init) |
| End | `fd2_chapter_23_end @ 0x00024754` | 960 B (最大 end) |
| Post-action | `fd2_chapter_23_post_action @ 0x00020AAF` | bypass default |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[22]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[22]` |  |

### Init handler

開場 cinematic 規模最大：含 screen-wide spell visual + 16-char 全清隊 → revive HP>0 過濾。

1. `fd2_init_battle_state_for_chapter`
2. **大規模清隊**：`for i in [0, 0x10): fd2_mark_char_as_dead(i)` (16 chars 全標 dead)
3. `fd2_pan_cursor_and_window(0xE, 0x20)`
4. `fd2_cast_screen_wide_spell_with_fade(cursor_x+6, cursor_y+5, 10, 8)` — screen-wide cinematic spell visual
5. **revive 過濾**：`for i in [0, 0x10): if chars[i].wHP_current != 0: chars[i].bFlags = 0; chars[i].pSprite_state[1] = 2` (face north) — 只復活原本 HP>0 的角色
6. `fd2_composite_battle_frame` + `fd2_set_vga_palette_range_with_add(0, 0xFF, 0)` 重繪
7. `fd2_display_dialog_scene(page=0)`
8. `fd2_pan_cursor_and_window(0xE, 0x1D)` + `fd2_cutscene_event_trigger(0x44)`
9. `fd2_display_dialog_scene(page=1)` + `fd2_cutscene_event_trigger(0x45)`
10. `fd2_display_dialog_scene(page=2)` + `fd2_cutscene_event_trigger(0x46)`
11. `fd2_display_dialog_scene(page=3)`
12. `fd2_pan_cursor_and_window(0xE, 0xD)` + `fd2_load_chapter_portraits_and_dump_tmp(1)`
13. **Palette transition**：`fd2_set_vga_palette_range_with_add(0, 0xFF, 0xFF)` (white-out) → composite → palette restore
14. `chars[0x10].pSprite_state[1] = 2`、`chars[0x11].pSprite_state[1] = 2` (face north)
15. `fd2_display_dialog_scene(page=4)` + `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 23 | 0, 1, 2, 3, 4 |
| End Phase 1 (有天空之鑰) | 23 | 8 |
| End Phase 1 (無天空之鑰) | 23 | 9 |
| End Phase 1 (蜜蒂在 template) | 23 | 0xA, 0xB |
| End Phase 1 (< 15 回合 + 蜜蒂未在) | 23 | 0xD |
| End Phase 1 (≥ 15 回合 + 蜜蒂未在) | 23 | 0xC |
| End Phase 2 (進第二戰場) | 23 | 0xE, 0xF, 0x10, 0x11 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 中 conditional 加入：
- char_id 22 (0x16) → 武聖卡里斯 — 若 `fd2_any_char_has_item(100)` (天空之鑰)
- char_id 19 (0x13) → 劍聖羅德曼 — 若蜜蒂 (template id 0x12) 未在 template 且 `data_fd2_battle_turn_counter < 0xF` (< 15 回合)

End handler conditional `fd2_mark_char_as_dead(0x11)`：若蜜蒂在 template，或蜜蒂未在但 ≥ 15 回合。

### Cutscene events

- Init: `0x44, 0x45, 0x46`
- End: `0x47` (若無天空之鑰) 或 `0x48` (若蜜蒂在或錯過 15 回合)；`0x49` × 3 (進入第二戰場)

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id]` 的 walk-animation script。

### Post-action handler

`fd2_chapter_23_post_action @ 0x00020AAF` — bypass default：

- 若 chars[0]、chars[1]、chars[0x10]、chars[0x11] 任一死 → `game_event_flag = 1` (lose)
- 若 chars[0x12] (機甲隊長) 死 → `game_event_flag = 2` (win)

char[0]=索爾、char[1]=希爾法、char[0x10]=卡里斯、char[0x11]=羅德曼、char[0x12]=機甲隊長 boss。

### End handler events

`fd2_chapter_23_end @ 0x00024754` (960 B) — 含 3 重 conditional + scene reload 進入第二戰場：

#### Phase 1 — 加入判定

1. 從 `data_fd2_chapter_ch23_end_scene_char_pos_x/y_table` 與 `..._facing_table` 讀 5 char 位置
2. `fd2_setup_chars_and_camera_for_intro(pos_x, pos_y, facing, 0, 0x10, 0x11, 0x15, 0x15, 2, 0xE, 0xE)` 配置
3. **Conditional 1**：`fd2_any_char_has_item(100)` (天空之鑰)
   - 有 → `fd2_display_dialog_scene(page=8)` + `fd2_init_runtime_char_from_base_growth(0x16)` (卡里斯加入)
   - 無 → `fd2_display_dialog_scene(page=9)` + `fd2_cutscene_event_trigger(0x47)`
4. **Conditional 2**：`fd2_find_template_char_by_id(0x12)` (蜜蒂在 template?)
   - 蜜蒂在 → `fd2_display_dialog_scene(page=0xA)` + `fd2_cutscene_event_trigger(0x48)` + `fd2_mark_char_as_dead(0x11)` + `fd2_display_dialog_scene(page=0xB)`
   - 蜜蒂未在 → 子條件 `data_fd2_battle_turn_counter < 0xF` (< 15 回合)
     - < 15 回合 → `fd2_display_dialog_scene(page=0xD)` + `fd2_init_runtime_char_from_base_growth(0x13)` (羅德曼加入)
     - ≥ 15 回合 → `fd2_display_dialog_scene(page=0xC)` + `fd2_cutscene_event_trigger(0x48)` + `fd2_mark_char_as_dead(0x11)`
5. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1` (22 → 23)

#### Phase 2 — 進入第二戰場 cinematic

6. `fd2_display_dialog_scene(page=0xE)` + 400ms + `fd2_play_rising_pre_cast_effect(1, 0xF, 10)` + `fd2_animate_screen_shake(0x1E)` (第 1 次)
7. `fd2_display_dialog_scene(page=0xF)` + 400ms + `fd2_play_rising_pre_cast_effect(1, 0xF, 10)` + `fd2_animate_screen_shake(0x1E)` (第 2 次)
8. `fd2_display_dialog_scene(page=0x10)` + 400ms + `fd2_play_rising_pre_cast_effect(1, 0x1E, 0x10)` (第 3 次，更大範圍)
9. **64-step palette 漸變**：`for v in [0, 0x40) step 2: fd2_set_vga_palette_range_with_add(0, 0xFF, v)` (brightness 遞增白化)
10. **Reload battle scene**：
    - `fd2_load_dat_resource("FDFIELD.DAT", 0x45)` → `battle_tile_map` (= ch24 tile_map，新地圖)
    - `fd2_load_dat_resource("FDSHAP.DAT", 0x2E)` → `data_fd2_battle_scene_tile_gfx_ptr`
    - `fd2_load_dat_resource("FDSHAP.DAT", 0x2F)` → `tile_attribute_flags_buffer`
    - `fd2_battle_reset_tile_transient_state(battle_tile_map)`
    - `fd2_load_chapter_background_layers`
11. `pan(0xE, 0x1D)` + palette restore + `fd2_cutscene_event_trigger(0x49)` + `pan(0xE, 0xE)` + `fd2_cutscene_event_trigger(0x49)` × 2
12. `fd2_display_dialog_scene(page=0x11)`

第二戰場沿用 Phase 1 已遞增的 `current_chapter_id` (23)，handler 內不再二次遞增。

## FDFIELD event script

FDFIELD entry idx **67** (= chapter_id × 3 + 1，chapter_id = 22)，entry size 1951 bytes，party_member_count = 16，char_spawn_count = 70。header layout 見 `resource_info/fdfield.md`。

4 / 16 turn-event hooks active (其餘 12 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 13 | 0 (enemy_turn_intro) | 0x34 | `0x000352E2` | 援軍 reinforcement (含 mid-handler chapter reload context) |
| 15 | 0 (enemy_turn_intro) | 0x34 | `0x000352E2` | 援軍 reinforcement |
| 18 | 0 (enemy_turn_intro) | 0x34 | `0x000352E2` | 援軍 reinforcement |
| 22 | 0 (enemy_turn_intro) | 0x34 | `0x000352E2` | 援軍 reinforcement |

## 對話

對話文字 18 pages 來自 FDTXT.DAT entry 23。Init handler 依序引用 page 0-4 (傳送失準降落、發現對方陣營、認出卡里斯與羅德曼、古代機兵登場宣戰)。End handler 依戰況分支引用不同對白：page 8 / 9 對應天空之鑰有無 (卡里斯加入或離隊)；羅德曼的去留有三種結局 — page 0xA+0xB (蜜蒂仍在隊伍，羅德曼婉拒並帶出蜜蒂的往事)、page 0xC (逾 15 回合，羅德曼因職守婉拒)、page 0xD (15 回合內取勝，羅德曼加入)；page 0xE-0x11 是飛行岩起飛前往天空的第二戰場 cinematic。page 5/6/7 為戰鬥中機甲隊長 (char 0x12) 被擊毀、卡里斯 (char 0x10)、羅德曼 (char 0x11) 陣亡的台詞。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『呼！難過死了‥咦！這裡是
[PAGE_BREAK]
　‥敵人的根據地嗎？為何看
[PAGE_BREAK]
　起來一點都不像？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『在傳送魔法啟動後，我感覺
[PAGE_BREAK]
　到有極強的魔力從中干擾，
[PAGE_BREAK]
　傳送魔法的作用因此發生了
[PARAGRAPH]
　錯亂‥希爾法，我說得沒錯
[PAGE_BREAK]
　吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『是的，傳送過程受到這種干
[PAGE_BREAK]
　擾，我們沒有全部嵌到岩壁
[PAGE_BREAK]
　裏去就該慶幸不已了。不過
[PARAGRAPH]
　這裡應該離原來的目的地不
[PAGE_BREAK]
　遠，這個我大概可以確定‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『可惡，原來又是敵人搞的鬼
[PAGE_BREAK]
　！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0016]
『咦？什麼時候多了好大一票
[PAGE_BREAK]
　人出來？他們是怎麼來到這
[PAGE_BREAK]
　裡的，我們居然毫無知覺？
[PARAGRAPH]
　』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0013]
『什麼？是敵人嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『看起來不像，還是先過去打
[PAGE_BREAK]
　個照面再說！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哎呀，有人比我們先來了呢
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001B]
『敵人這麼快就來了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『不像，對方只有兩個人而已
[PAGE_BREAK]
　‥好像想和我們談話的樣子
[PAGE_BREAK]
　。』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0016]
『不知各位為何跋涉千里，來
[PAGE_BREAK]
　到這沙漠之中的廢墟？是和
[PAGE_BREAK]
　我們一樣為了研究遺跡而來
[PARAGRAPH]
　的嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『啊！你不是‥卡里斯叔叔嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『你說什麼？你怎麼知道我叫
[PAGE_BREAK]
　卡里斯？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『卡里斯叔叔，我是索爾啊！
[PAGE_BREAK]
　小時候你經常帶我出去玩，
[PAGE_BREAK]
　還教我武術的‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『你這麼一說，我也想起來了
[PAGE_BREAK]
　！他的確是曾經教過我們武
[PAGE_BREAK]
　術的卡里斯叔叔！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『你是‥索爾王子嗎？那你就
[PAGE_BREAK]
　是巴拉多的兒子亞雷斯了！
[PAGE_BREAK]
　我記得你們從小就在一起的
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『答對啦！卡里斯叔叔，你果
[PAGE_BREAK]
　然還記得我們！真是好久不
[PAGE_BREAK]
　見了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『聽父王說你到海外來探險，
[PAGE_BREAK]
　我們都神往不已呢！真沒想
[PAGE_BREAK]
　到會在這裡遇到您。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『時間過的真快，你們都長大
[PAGE_BREAK]
　啦！雷特王子‥‥，陛下這
[PAGE_BREAK]
　些日子還好嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『父王母后都很安健，不過老
[PAGE_BREAK]
　嫌生活平淡了些，大概是等
[PAGE_BREAK]
　著要聽卡里斯叔叔您的冒險
[PARAGRAPH]
　故事吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『哪！又遇見了一個熟人，這
[PAGE_BREAK]
　可不是羅德曼嗎？你一個好
[PAGE_BREAK]
　好的邊防將軍不做，又跑來
[PARAGRAPH]
　這裡考古啦？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『果然是約拿你這把老骨頭，
[PAGE_BREAK]
　世界上就你最清楚我的習性
[PAGE_BREAK]
　。你怎麼也捨得跑到這荒山
[PARAGRAPH]
　野外來了？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我倒忘了問你，這裡到底是
[PAGE_BREAK]
　哪裡？我們是被魔法傳送來
[PAGE_BREAK]
　的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『喲，這趟路可真遠了！這裡
[PAGE_BREAK]
　是大陸西北方的沙漠中央的
[PAGE_BREAK]
　一個綠洲，我一直對這綠洲
[PARAGRAPH]
　中的廢墟感興趣，所以和這
[PAGE_BREAK]
　朋友一起來調查。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『不瞞各位，最近有一位學者
[PAGE_BREAK]
　幫我解讀了一篇上古經典的
[PAGE_BREAK]
　記載，我正在追查其中一些
[PARAGRAPH]
　古物的下落，所以碰巧來到
[PAGE_BREAK]
　這裡‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『嗶，發現入侵者！指令﹕保
[PAGE_BREAK]
　護飛行岩，交戰並殲滅入侵
[PAGE_BREAK]
　者。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x006F]
『嗶，指令確認！開始動作！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『嘿，那是什麼怪東西？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『那是典籍中所記載的古代機
[PAGE_BREAK]
　兵，他們負責保護所有古代
[PAGE_BREAK]
　人所留下來的建築和設施，
[PARAGRAPH]
　會將入侵者殺戮殆盡‥‥現
[PAGE_BREAK]
　在沒時間說明了，準備應付
[PAGE_BREAK]
　這些傢伙吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『說到打架，我精神就來了！
[PAGE_BREAK]
　我們上！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_CHAR=0x0012]
『嗶，指令系統失效‥任務中
[PAGE_BREAK]
　止‥指令傳輸關閉‥‥』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『我怎能‥在這種時候‥倒下
[PAGE_BREAK]
　‥‥』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_CHAR=0x0011]
『可惡‥我還沒‥這廢墟的秘
[PAGE_BREAK]
　密‥‥』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_ID=0x0016]
『不愧是古代人建造的機兵，
[PAGE_BREAK]
　應付起來真是棘手的很‥幸
[PAGE_BREAK]
　好大家都能獨當一面，才能
[PAGE_BREAK]
　順利消滅這些敵人。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『危機已過，現在我們可以仔
[PAGE_BREAK]
　細研究這個遺跡了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『卡里斯叔叔，這遺跡到底有
[PAGE_BREAK]
　什麼秘密，你還沒告訴我們
[PAGE_BREAK]
　呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『是這樣嗎？其實我也是從古
[PAGE_BREAK]
　籍上看來的。據說眼前這個
[PAGE_BREAK]
　小丘是古代人所造的飛行用
[PARAGRAPH]
　具，經由一支傳送法杖的力
[PAGE_BREAK]
　量，可以把擁有「天空之鑰
[PAGE_BREAK]
　」的人帶到黃金的城堡上去
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『天空之鑰？那不就在我們手
[PAGE_BREAK]
　上嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『「傳送之杖」也有啊！我們
[PAGE_BREAK]
　就是被它傳送到這裡來的。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『真的？那真是太巧了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我一直認為，最近大陸上的
[PAGE_BREAK]
　天地異變和神秘敵人都和這
[PAGE_BREAK]
　「黃金城」有關。在古代傳
[PARAGRAPH]
　說中，黃金城總和戰爭、災
[PAGE_BREAK]
　厄等脫不了關係，所以我們
[PAGE_BREAK]
　一直都在追查這其中的關聯
[PARAGRAPH]
　。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『這確是有加以查明的必要。
[PAGE_BREAK]
　索爾殿下，既然這兩件東西
[PAGE_BREAK]
　都已到手，可以讓我和你們
[PARAGRAPH]
　一起去看看嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『當然可以！有卡里斯叔叔同
[PAGE_BREAK]
　行，什麼對手都不足為懼了
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『是呀！』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_ID=0x0016]
『不愧是古代人建造的機兵，
[PAGE_BREAK]
　應付起來真是棘手的很‥幸
[PAGE_BREAK]
　好大家都能獨當一面，才能
[PARAGRAPH]
　順利消滅這些敵人。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『危機已過，現在我們可以仔
[PAGE_BREAK]
　細研究這個遺跡了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『卡里斯叔叔，這遺跡到底有
[PAGE_BREAK]
　什麼秘密，你還沒告訴我們
[PAGE_BREAK]
　呢！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『是這樣嗎？其實我也是從古
[PAGE_BREAK]
　籍上看來的，據說此地可能
[PAGE_BREAK]
　藏有「天空之鑰」。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『我聽過這東西，據說它是通
[PAGE_BREAK]
　往黃金城堡的唯一方法。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『我們並未遇料到會有敵人出
[PAGE_BREAK]
　現，這回算是相當的驚險‥
[PAGE_BREAK]
　不過，我想我還是先回城休
[PARAGRAPH]
　養一下好了，將軍殿下，這
[PAGE_BREAK]
　裡就先交給你了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『卡里斯叔叔，你要多保重喔
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0016]
『索爾殿下，您和亞雷斯也要
[PAGE_BREAK]
　多小心！不過以你們的表現
[PAGE_BREAK]
　看來，我想我是可以放心了
[PARAGRAPH]
　。我們後會有期！』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『羅德曼，那你打算怎麼辦呢
[PAGE_BREAK]
　？我很希望能有像你這樣的
[PAGE_BREAK]
　武士加入，這對我們的戰力
[PAGE_BREAK]
　有很大的提昇。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『我‥唉，我想‥我還是不太
[PAGE_BREAK]
　適合。老友，我也要回城了
[PAGE_BREAK]
　，希望你們此行順利，都能
[PARAGRAPH]
　平安歸來。‥‥』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0012]
『唉，都是因為我的關係‥‥
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『前輩，這怎麼說？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『這是一件很久以前的往事了
[PAGE_BREAK]
　‥‥那時羅德曼將軍還在禁
[PAGE_BREAK]
　衛軍統領任內，而我才剛成
[PARAGRAPH]
　為禁衛軍騎士不久‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『啊！我想起來了！那時前輩
[PAGE_BREAK]
　妳向將軍他‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『是的，那時還年輕的我被感
[PAGE_BREAK]
　情沖昏了頭，在大庭廣眾面
[PAGE_BREAK]
　前向將軍表明愛意。但是將
[PARAGRAPH]
　軍的夫人妮莉公主是陛下最
[PAGE_BREAK]
　小的妹妹，我這麼做讓他非
[PAGE_BREAK]
　常為難，結果將軍向陛下請
[PARAGRAPH]
　調邊疆，就只為了避開我‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0006]
『原來是這樣‥‥。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『那時的我傷心欲絕，雖把全
[PAGE_BREAK]
　部心思用在劍術上，仍然忘
[PAGE_BREAK]
　不了這件事，因此才又離開
[PARAGRAPH]
　騎士團到深山中修練‥這麼
[PAGE_BREAK]
　多年以後，我已經不再釋懷
[PAGE_BREAK]
　，但將軍還這麼在乎這件事
[PARAGRAPH]
　，你們就可以知道當年我讓
[PAGE_BREAK]
　他多麼困擾了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒關係！密蒂前輩，將軍的
[PAGE_BREAK]
　份就算是由妳代替了，今後
[PAGE_BREAK]
　妳不但是為了大家而戰，也
[PARAGRAPH]
　是為了將軍而戰，過去的遺
[PAGE_BREAK]
　憾和感傷，就讓它在戰陣中
[PAGE_BREAK]
　煙消雲散！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『‥索爾，謝謝你。我會記住
[PAGE_BREAK]
　這句話的。』
[END]
```

### Page 12

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『羅德曼，那你打算怎麼辦呢
[PAGE_BREAK]
　？我很希望能有像你這樣的
[PAGE_BREAK]
　武士加入，這對我們的戰力
[PAGE_BREAK]
　有很大的提昇。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『老友，我很希望能與你們同行
[PAGE_BREAK]
　，可是我是受陛下的任命，
[PAGE_BREAK]
　在此擔任邊防將軍，
[PARAGRAPH]
　除非有陛下的諭命，否則擅
[PAGE_BREAK]
　離職守視同抗命。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0012]
『是這樣嗎？‥真是遺憾，我本
[PAGE_BREAK]
　來以為可以再看到你昔日的
[PAGE_BREAK]
　英姿呢‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『這也是件沒辦法的事‥老友，
[PAGE_BREAK]
　保重！我得回城了。願你們
[PAGE_BREAK]
　此行順利，平安歸來。』
[END]
```

### Page 13

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『羅德曼，那你打算怎麼辦呢
[PAGE_BREAK]
　？我很希望能有像你這樣的
[PAGE_BREAK]
　武士加入，這對我們的戰力
[PARAGRAPH]
　有很大的提昇。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『是嗎？難得聽你這樣誇讚我
[PAGE_BREAK]
　，如果我這把老骨頭還能派
[PAGE_BREAK]
　得上用場，那當然是樂意之
[PARAGRAPH]
　至。哈哈哈！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『太好啦！我知道羅德曼將軍
[PAGE_BREAK]
　的戰技一直都是沒話說的！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0013]
『咦，妳不是‥妳不是希莉亞
[PAGE_BREAK]
　公主嗎？為何殿下‥會在這
[PAGE_BREAK]
　裡？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『將軍你都可以蹺班來考古了
[PAGE_BREAK]
　，我怎麼可以待在宮裏發呆
[PAGE_BREAK]
　呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『好啦，別大驚小怪了，為了
[PAGE_BREAK]
　解決這件事情，王國的強者
[PAGE_BREAK]
　勇士幾乎都到齊了，我們先
[PARAGRAPH]
　想想接下來要怎麼辦吧！』
[END]
```

### Page 14

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『‥我試了一下，這小丘上的
[PAGE_BREAK]
　法陣好像對這法杖上的魔力
[PAGE_BREAK]
　有反應，所以我想實驗看看
[PARAGRAPH]
　，看能不能讓它飛起來‥。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『快試，快試！我等不及要看
[PAGE_BREAK]
　了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『好吧，那我現在就開始‥米
[PAGE_BREAK]
　洛耶爾．希里卡恩．耶．哈
[PAGE_BREAK]
　洛納！』
[END]
```

### Page 15

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『地震了！』
[END]
```

### Page 16

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哇啊啊，好可怕！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『可能要起飛了，大家站穩！
[PAGE_BREAK]
　』
[END]
```

### Page 17

```text
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哇！飛了，飛了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001B]
『真是神奇！古代人的智慧和
[PAGE_BREAK]
　力量，果然不是我們所能想
[PAGE_BREAK]
　像的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這飛行岩開始高速飛行了‥
[PAGE_BREAK]
　希爾法，你知道它是要飛到
[PAGE_BREAK]
　哪裡去嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『嗄？這‥我也太不清楚，我
[PAGE_BREAK]
　只有使用這法杖啟動它而已
[PAGE_BREAK]
　，現在‥現在它是在飛它自
[PARAGRAPH]
　己的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『那‥那怎麼辦？我們總不能
[PAGE_BREAK]
　就這樣一直飛下去吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『別緊張‥‥這飛天丘總會飛
[PAGE_BREAK]
　到一個目的地的，這是在很
[PAGE_BREAK]
　久以前就已經決定好了的‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『悠妮小姐，妳知道和這飛行
[PAGE_BREAK]
　岩有關的事嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥好像有點印象‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『既然悠妮這麼說，我們就靜
[PAGE_BREAK]
　觀其變吧！看看這玩意到底
[PAGE_BREAK]
　會把我們帶到哪裡去！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『呀呼！這樣飛真是過癮呢！
[PAGE_BREAK]
　』
[END]
```
