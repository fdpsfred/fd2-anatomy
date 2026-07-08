# 第 29 章 — 無邊的黑暗之中

進入黃金城核心區域次元反應爐，悠妮以「天空之鑰」接管要塞中樞控制系統；途中須摧毀三條防衛中樞巨龍。最終揭露隱身於隊伍中的 char[0x14] 真身為空魔神（ASR-06）。

## 加入角色

無新加入角色。

## 敵人配置

本章 FDFIELD entry 85 共 74 個會生成的 spawn 記錄（另有 2 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 36 | 火龍 | LV30 | ×1 | aggressive_physical |
| 59 | 暗黑龍 | LV30 | ×1 | aggressive_physical |
| 37 | 雷龍 | LV30 | ×1 | aggressive_physical |
| 45 | 光束炮座 | LV20 | ×6 | aggressive_physical |
| 44 | 機甲射手 | LV24 | ×19 | default_attacker / aggressive_physical |
| 43 | 機甲兵 | LV25 | ×25 | default_attacker / aggressive_physical |
| 48 | 機甲隊長 | LV34 | ×5 | aggressive_physical / default_attacker |
| 46 | 機甲突擊兵 | LV23 | ×6 | aggressive_physical |
| 47 | 機甲守衛 | LV24 | ×5 | aggressive_physical |
| 46 | 機甲突擊兵 | LV25 | ×2 | default_attacker |
| 44 | 機甲射手 | LV25 | ×2 | default_attacker |
| 58 | 空魔神 | LV34 | ×1 | default_attacker |

三條巨龍（火龍 enemy_data 36 / 雷龍 37 / 暗黑龍 59）即要塞防衛中樞 boss，須以直接攻擊摧毀；實際勝利判定走 tile_event 旗標（見 §特殊機制）。

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：神聖之水 (0xC3)、神聖之水 (0xC3)、神聖之水 (0xC3)、神聖之水 (0xC3)、神聖之水 (0xC3)、神聖之水 (0xC3)

## 商店

無。

## 特殊機制

- **勝利條件（FD2 唯一）**：以 `tile_event_consumed_flags[0x12]`、`[0x13]`、`[0x14]` 全數觸發（= 解除三處防衛系統）判勝，而非擊敗特定敵人。這是全 30 章唯一用 tile_event 旗標而非 char 死活作勝利判定的章節（`fd2_chapter_29_post_action @ 0x00020B72`）。
- **失敗條件**：索爾 chars[0] 死 → 負；悠妮 chars[1] 死 → 顯示 page 9「不能輸給那傢伙‥索爾‥」對話後判負。
- **Init 從 page 7 開始**：跳過 page 0..6（保留給 FDFIELD turn-event / tile-step handler，含護送悠妮到控制中心石碑、巨龍對話等），延續 ch28 結尾劇情。
- **擊毀第一隻機甲隊長 → 寶箱平台援軍**：tile-step handler 改寫 turn-event hook，使下回合中央左右寶箱平台 spawn 援軍（詳見 FDFIELD event script 節）。
- **護送悠妮到控制中心石碑**：tile-step 觸發後再過 3 回合，`fd2_fire_chapter_turn_events_for_phase` 觸發三條巨龍 boss 戰。
- **char[0x14] 變身為空魔神**：end handler 把偽裝在隊伍中的 char[0x14] 之 bPortrait_id / bChar_id 都設為 0x7E，揭露真身為空魔神（ASR-06，敵資料表 entry 58 @ `0x7AD51`）。詳細 cinematic 步驟見 Handler 流程 § End handler events。
- **跨章天空之鑰兌換鏈**：悠妮以「天空之鑰」接管要塞中樞（page 7 對白「只要有這天空之鑰，我就可以接管要塞的中樞系統」）。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_29_init @ 0x00033DBA` | 130 B |
| End | `fd2_chapter_29_end @ 0x0002548C` | 451 B |
| Post-action | `fd2_chapter_29_post_action @ 0x00020B72` | bypass default; tile_event_consumed_flags-based win |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[28]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[28]` |  |

### Init handler

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(9, 0x38)` + `fd2_cutscene_event_trigger(0x56)`
3. `fd2_display_dialog_scene(page=7)`（從 page 7 開始，非 page 0 — 延續 ch28 結尾劇情）
4. `fd2_cinematic_chapter_portrait_dump_with_white_flash(9, 0x13, 8)` — char placement helper
5. `fd2_display_dialog_scene(page=8)` + `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 29 | 7, 8 |
| Post-action (chars[1] 死) | 29 | 9 |
| End | 29 | 10, 0xB, 0xC, 0xD, 0xE, 0xF (6 pages) |

Page 0..6 由 FDFIELD turn-event / tile-step handler 引用（含護送悠妮到控制中心石碑、三條巨龍對話等）。

### char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫；本章不加入新角色。

End handler 執行 char 變身：`chars[0x14].bPortrait_id = 0x7E` + `chars[0x14].bChar_id = 0x7E` → 變身為**空魔神**（`data_fd2_battle_enemy_data_table` entry 58 @ `0x7AD51`）。

### Cutscene events

- Init：`0x56`
- End：無 `fd2_cutscene_event_trigger`（純 cinematic helpers）

### Post-action handler

`fd2_chapter_29_post_action @ 0x00020B72` — bypass default：

- if `tile_event_consumed_flags[0x12]` AND `[0x13]` AND `[0x14]` 全部已觸發 → `game_event_flag = 2`（win — 解除防衛系統）
- if chars[0] dead → `game_event_flag = 1`（lose）
- if chars[1] dead → `fd2_display_dialog_scene(page=9)` + `game_event_flag = 1`

FD2 唯一用 `tile_event_consumed_flags` 而非 char 死活作勝利判定的章節。「解除防衛系統」= 觸發三個 tile_event（`0x12` / `0x13` / `0x14`）。

### End handler events

`fd2_chapter_29_end @ 0x0002548C` (451 B) — 大 cinematic + char 變身 + 9 連震 + 3 白光：

1. `fd2_display_dialog_scene(page=10)`
2. `fd2_kill_runtime_chars_from_index_to_end(0x14)` — char[0x14] 操作 helper
3. **char[0x14] 變身為空魔神**：
   - `chars[0x14].bPortrait_id = 0x7E`
   - `chars[0x14].bChar_id = 0x7E`
   - char_id 0x7E 屬 enemy class（≥ 0x44）→ 敵資料表 index = 0x7E - 0x44 = 58 → 位址 = 表基底 `0x7AB0D` + 58 × 10 = `0x7AD51`（空魔神）
4. `fd2_display_dialog_scene(page=0xB)`
5. `fd2_load_chapter_portraits_and_dump_tmp(9)` + `pan` + `fd2_pan_cursor_to_tile_animated(0xF, 10)`
6. `fd2_animate_warp_teleport_char(party_member_count - 1, 0xF, 0xA, 0xF, 0xA)` — 傳送最後 party char (warp 動畫)
7. `fd2_display_dialog_scene(page=0xC)`
8. **第 1 輪 3 連震**：`fd2_animate_screen_shake(0x14)` × 3（各 600ms）
9. `fd2_display_dialog_scene(page=0xD)`
10. **第 2 輪 3 連震**：`fd2_animate_screen_shake(0x14)` × 3（各 200ms）
11. `fd2_display_dialog_scene(page=0xE)`
12. **第 3 輪 climax**：`fd2_animate_screen_shake(0x14)` + 200ms + `fd2_animate_screen_shake(0x14)` + 100ms + `fd2_animate_screen_shake(0x28)`（3 倍長度）+ 200ms
13. **3 道白光閃**：`fd2_animate_palette_flash_pulse_white` × 3（各 300ms）
14. `fd2_display_dialog_scene(page=0xF)`
15. **64-step palette fade-out**：`for iVar5 in [0, 0x40): fd2_set_vga_palette_range_with_add(0, 0xFF, iVar5)` + 4ms（整螢幕逐步 white-out）
16. `memset(0xA0000, 0, 64000)` — 黑屏 + 800ms
17. **64-step palette fade-in (reversed)**：`for iVar5 from 0x3E down to 0` + 4ms
18. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **85**（= chapter_id × 3 + 1，chapter_id = 28），entry size 2107 bytes。header 內 party_member_count = 20、char_spawn_count = 76。header layout 見 resource_info/fdfield.md。

turn-event hook table 有 3 / 16 條 active（其餘 13 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0 (enemy_turn_intro) | 0x4A | `0x00035C32` | 動態啟動候選（空魔神 cinematic） |
| 0xFF | 2 (new_player_turn_intro) | 0x4C | `0x00035D60` | 動態啟動候選 |
| 0xFF | 0 (enemy_turn_intro) | 0x4F | `0x00035EE6` | 動態啟動候選 |

`turn=0xFF` 在初始狀態不會等於回合計數，hook table 不會直接 fire。實際觸發機制：tile-step-event handler 在特定劇本 tile 被踩到時，動態改寫該章 turn-event hook table 的 turn byte（0xFF → 當前回合 +1），把原本 sentinel 的 entry 啟動為下一回合 fire 的 event。「擊毀第一隻機甲隊長 → 中央左右寶箱平台立即出現援軍」與「護送悠妮到上方控制中心石碑前休息後再過三回合打三頭龍」都屬此類 tile-step → turn-event 連鎖。

## 對話

對話文字 16 pages（page 0..0xF）來自 FDTXT.DAT entry 29（= chapter_id + 1）。Init 引用 page 7、8；post-action「悠妮死亡」引用 page 9；End 引用 page 10、0xB、0xC、0xD、0xE、0xF。page 0..6 由 FDFIELD turn-event / tile-step handler 引用（護送悠妮到控制中心石碑、三條巨龍對話等）。

### Page 0

```text
『‥‥，這是什麼？
[PAGE_BREAK]
　還是要悠妮來才行！』
[END]
```

### Page 1

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『就是這裡了！系統識別碼
[PAGE_BREAK]
　‥0370425188‥
[PAGE_BREAK]
　啊，這是‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，怎樣？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『中樞撤銷最高級系統防護
[PAGE_BREAK]
　需要一些時間，我想還要再
[PAGE_BREAK]
　等一會兒！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒關係！倒是妳自己可要
[PAGE_BREAK]
　多小心些！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『好的！』
[END]
```

### Page 2

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『奇怪，終端竟然抗拒我的
[PAGE_BREAK]
　命令，有人對防衛系統動了
[PAGE_BREAK]
　手腳！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，怎樣了？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『糟糕，系統進入了最終防
[PAGE_BREAK]
　衛狀態！這‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0068]
『警告！中樞部有敵人侵入
[PAGE_BREAK]
　！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x0068]
『最終防衛狀態啟動！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x0068]
『對外系統連結閉鎖！自我
[PAGE_BREAK]
　防護裝置啟動！』
[END]
```

### Page 6

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，小心！那三條龍是
[PAGE_BREAK]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『事到如今，也只能這樣做
[PAGE_BREAK]
　了‥大家聽著，這三條巨龍
[PAGE_BREAK]
　形狀的東西是黃金城的防衛
[PARAGRAPH]
　中樞，由於敵人的干擾，我
[PAGE_BREAK]
　們現在已經無法控制防衛中
[PAGE_BREAK]
　樞，必須集合大家的力量來
[PARAGRAPH]
　把它們徹底摧毀！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『怎麼摧毀？用武器砍爛嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『是的，直接攻擊它們就行
[PAGE_BREAK]
　了！不過它們都有防衛裝置
[PAGE_BREAK]
　保護，防衛裝置的威力極為
[PARAGRAPH]
　強大，千萬不可大意！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『知道了！大家上吧！』
[END]
```

### Page 7

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『好累，拐了半天路，頭都快
[PAGE_BREAK]
　暈了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒辦法，為了少打一些無謂
[PAGE_BREAK]
　的仗以保留大家的實力，盡
[PAGE_BREAK]
　量避開其它的守衛隊是必須
[PARAGRAPH]
　的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『唔！這地方蘊含著極強大的
[PAGE_BREAK]
　能量，其威力之強是我前所
[PAGE_BREAK]
　未見的‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『是的，這裡就是黃金城的核
[PAGE_BREAK]
　心，次元反應爐。這反應爐
[PAGE_BREAK]
　不但產生整個要塞所需的動
[PARAGRAPH]
　力，以某種方法將它的反應
[PAGE_BREAK]
　逆轉，也可以讓它成為恐怖
[PAGE_BREAK]
　的最終兵器‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『那我們現在該怎麼辦？破壞
[PAGE_BREAK]
　它嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『不！並不需要破壞它，在這
[PAGE_BREAK]
　核心區的北方，就是要塞的
[PAGE_BREAK]
　中樞控制系統，只要有這天
[PARAGRAPH]
　空之鑰，我就可以接管要塞
[PAGE_BREAK]
　的中樞系統，經由我的終端
[PAGE_BREAK]
　連接內部網路來控制整個要
[PARAGRAPH]
　塞。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，想不到妳有這種能力
[PAGE_BREAK]
　，我好佩服妳！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『是嗎？想要有這種能力，可
[PAGE_BREAK]
　是要付出可怕的代價的‥不
[PAGE_BREAK]
　多說了，我們這就過去吧。
[PARAGRAPH]
　』
[END]
```

### Page 8

```text
[PORTRAIT_LEFT_BY_CHAR=0x0036]
『嗶，目標發現並確認！執行
[PAGE_BREAK]
　作戰指令！間接座標傳輸完
[PAGE_BREAK]
　成，防衛用障壁啟動！各單
[PARAGRAPH]
　位，確實殲滅所有敵目標！
[PAGE_BREAK]
　不需顧慮ASR一07！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『哼，有埋伏！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『地形差了一點是真的，但要
[PAGE_BREAK]
　戰勝應該還不是難事。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『這裡現在還完全在敵方的控
[PAGE_BREAK]
　制之下，我們千萬不能掉以
[PAGE_BREAK]
　輕心。大家設法護送我到前
[PARAGRAPH]
　面的控制盤去就可以了，只
[PAGE_BREAK]
　要讓我有機會把「天空之鑰
[PAGE_BREAK]
　」安裝好，我就能讓這些防
[PARAGRAPH]
　衛部隊全部變成廢鐵。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『沒問題，包在我身上！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『我們趕快動手吧，可別讓敵
[PAGE_BREAK]
　人搶了先機！』
[END]
```

### Page 9

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0001]
『‥不‥我不能輸給那
[PAGE_BREAK]
　傢伙‥索爾‥索‥‥』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『成了！』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『好極了，防衛中樞已經完全
[PAGE_BREAK]
　停止，雖然我們失去了指揮
[PAGE_BREAK]
　其他防衛部隊的能力，但往
[PARAGRAPH]
　後敵人也無法再利用防衛部
[PAGE_BREAK]
　隊來阻止我們。多謝大家的
[PAGE_BREAK]
　幫忙，現在只要再接管剩下
[PARAGRAPH]
　的系統，便可以防止對方使
[PAGE_BREAK]
　用最終兵器。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『好極了，那我們接下來要怎
[PAGE_BREAK]
　麼做？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『找出最後的敵人，我相信他
[PAGE_BREAK]
　是這一切事件的背後主使者
[PAGE_BREAK]
　，他是‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『是指我嗎？』
[END]
```

### Page 12

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『是‥是你！真的是你！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『ASR一07，妳的系統真的受損
[PAGE_BREAK]
　這麼重嗎？居然認敵為友，
[PAGE_BREAK]
　實在令我無法理解‥不過，
[PARAGRAPH]
　還是很感激妳幫我送回這單
[PAGE_BREAK]
　元計算晶片，有了它，一直
[PAGE_BREAK]
　處在中止狀況的「黑暗之焰
[PARAGRAPH]
　」計畫就可以繼續下去了‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『為何你始終不肯放棄這個瘋
[PAGE_BREAK]
　狂的計畫？！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『這些地上的螻蟻，有什麼留
[PAGE_BREAK]
　存的必要！我們當初接到的
[PAGE_BREAK]
　命令不就是毀滅牠們嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『但我們最後接到了中止命令
[PAGE_BREAK]
　！「黑暗之焰」作戰已經中
[PAGE_BREAK]
　止了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『是中止，不是取消！那些傢
[PAGE_BREAK]
　伙命令我們進行最終計畫，
[PAGE_BREAK]
　把我們留在這個該死的星球
[PARAGRAPH]
　上，然後自己就逃到外太空
[PAGE_BREAK]
　去了！想想我們已經等了多
[PAGE_BREAK]
　久！已經多久了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『ASR一06，看來你的系統果然
[PAGE_BREAK]
　故障了！在上一次的意外中
[PAGE_BREAK]
　‥你一直沒有試著去自我修
[PARAGRAPH]
　復嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『沒有必要！我的使命就是執
[PAGE_BREAK]
　行作戰計畫。現在我終於可
[PAGE_BREAK]
　以開始履行我的職務了‥‥
[PARAGRAPH]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『不可能的，我已成功的和最
[PAGE_BREAK]
　高命令系統連接，現在整個
[PAGE_BREAK]
　要塞的中樞都在我的控制之
[PARAGRAPH]
　下！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『別傻了，妳忘了我身為監視
[PAGE_BREAK]
　暨戰鬥系統，在使用武器上
[PAGE_BREAK]
　有最高的優先權限嗎？即使
[PARAGRAPH]
　妳掌握著整個系統中樞，攻
[PAGE_BREAK]
　擊電腦仍然會接受我的命令
[PAGE_BREAK]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『你‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『黑暗之焰系統三十秒後啟動
[PAGE_BREAK]
　，目標馬拉大陸！哇哈哈哈
[PAGE_BREAK]
　，我等這一刻好久了！』
[END]
```

### Page 13

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥好，既然如此，我只有用
[PAGE_BREAK]
　最後一著了！‥‥』
[END]
```

### Page 14

```text
[PORTRAIT_LEFT_BY_ID=0x007E]
『妳想幹什麼？妳阻止不了我
[PAGE_BREAK]
　的‥‥不，不，妳不能這樣
[PAGE_BREAK]
　做！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『你啟動了黑暗之焰，我就讓
[PAGE_BREAK]
　反應爐逆轉！等到能量積蓄
[PAGE_BREAK]
　突破亞空間障壁的負荷量，
[PARAGRAPH]
　引發空間扭曲，我們就一起
[PAGE_BREAK]
　和這黃金城在超空間中永恆
[PAGE_BREAK]
　的漂流吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『‥妳這樣做，這些人也難逃
[PAGE_BREAK]
　一劫！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『若讓你啟動了「黑暗之焰」
[PAGE_BREAK]
　，會有更多無辜的人因此喪
[PAGE_BREAK]
　命！為了保護這個大家所珍
[PARAGRAPH]
　惜愛護的世界，相信‥相信
[PAGE_BREAK]
　大家不會怪我的！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『悠妮，做得好！無論如何，
[PAGE_BREAK]
　我們都不能和邪惡之徒妥協
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『我們這些老頭子早就都已經
[PAGE_BREAK]
　活得膩了，只可惜了這些前
[PAGE_BREAK]
　途大好的年輕人們‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『我們的傳奇會留在羅特帝亞
[PAGE_BREAK]
　和馬拉大陸的歷史上吧！身
[PAGE_BREAK]
　為一個武士，這就是最高的
[PARAGRAPH]
　的回報了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『對不起，如果‥如果今天我
[PAGE_BREAK]
　沒有把大家帶到這黃金城上
[PAGE_BREAK]
　來，或許就不會這樣了‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，我自始至終都相信妳
[PAGE_BREAK]
　的抉擇，今天會有這種結果
[PAGE_BREAK]
　全是這傢伙的錯，不管怎樣
[PARAGRAPH]
　，我要先劈他兩劍再說！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007E]
『笨蛋，來送死吧！』
[END]
```

### Page 15

```text
[PORTRAIT_LEFT_BY_ID=0x007E]
『反、反應爐超過負荷了！‥
[PAGE_BREAK]
　』
[END]
```
