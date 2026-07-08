# 第 7 章 — 往王城的途中

隊伍前往王城途中，再度被王國守備軍誤認為匪徒而遭圍攻。戰場上另有一名少女凱麗遭王國軍追捕，若在戰鬥中觸發指定 tile event 並保住她的性命，章末即可招募她入隊。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 12 (0x0C) | 凱麗 | End handler | 須觸發 tile event 0x11 **且** char[43] (0x2B) 戰鬥中存活 |

凱麗職業為武者（job 0x08）。戰場上她以 NPC runtime slot char[43] (0x2B) 出場，滿足雙重條件後才由 end handler 以 char_id 0x0C 正式加入永久隊伍。

## 敵人配置

寫在 FDFIELD.DAT entry 19。具體配置請參照該檔案。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。

## 商店

神秘商店 (Alt+F6) — 聖者之戒/心眼之書/白金勳章/飛龍卵，hack table 觸發。

## 特殊機制

- **失敗條件**：主角索爾陣亡即戰敗，由 `fd2_check_battle_end_default_handler` 判定（本章無自訂勝負條件）。
- **雙重條件招募凱麗**（char_id 0x0C，武者 job 0x08）：end handler `fd2_chapter_07_end` 依序檢查兩條件——(a) `tile_event_consumed_flags[0x11] == 1`（戰鬥中觸發指定 tile event），(b) `fd2_check_char_is_dead(0x2B)` 判定 char[43]（凱麗本人）仍存活；兩者皆成立才 `fd2_display_dialog_scene(page=4)` 並呼叫 `fd2_init_runtime_char_from_base_growth(0xC)` 讓凱麗入隊，任一不成立則顯示 page 5 跳過招募。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_07_init @ 0x00033169` | 176 B |
| End | `fd2_chapter_07_end @ 0x000232E8` | 222 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[6]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[6]` |  |

### Init handler

`fd2_init_battle_state_for_chapter` 後依序：

- `fd2_display_dialog_scene(page=0)`
- `chapter_init_phase_flag = 1`; `fd2_load_chapter_portraits_and_dump_tmp(race_id=1)`; `chapter_init_phase_flag = 0`
- `fd2_pan_cursor_and_window(8, 1)` + `fd2_cutscene_event_trigger(0x1C)`
- `fd2_pan_cursor_and_window(8, 0)` + `fd2_cutscene_event_trigger(0x1D)`
- `fd2_display_dialog_scene(page=1)` + `fd2_pan_cursor_to_char(0)`

`chapter_init_phase_flag` 在 portrait load 期間短暫升起（可能影響 dialog rendering）。

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 7 | 0, 1 |
| End (tile event 0x11 觸發 AND char[43] 活) | 7 | 4 |
| End (其他狀況) | 7 | 5 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 條件式 `fd2_init_runtime_char_from_base_growth(0xC)` → 凱麗加入（雙條件滿足時）。

### Cutscene events

- Init: `0x1C, 0x1D`（2 events，連續觸發兩段 walk animation）

### Post-action handler

`data_fd2_chapter_post_action_handler_table[6]` 指向 `fd2_check_battle_end_default_handler`，無自訂勝負條件。

### End handler events

`fd2_chapter_07_end @ 0x000232E8`（222 B）— double-conditional recruit：

1. 從 `data_fd2_chapter_ch07_end_scene_char_pos_x_table` / `pos_y_table` / `facing_table`
   (@ 0x520E1/0x520EA/0x520F3) 讀 4 chars 位置
2. `fd2_save_runtime_char_to_template`
3. **Conditional 1**：`tile_event_consumed_flags[0x11] == 1`（某 tile event 已觸發）
   - 若是 → **Conditional 2**：`fd2_check_char_is_dead(0x2B)`（char 43）
     - 若 char[43] 活著 → `fd2_setup_chars_and_camera_for_intro(...)` + `fd2_display_dialog_scene(page=4)` +
       `fd2_init_runtime_char_from_base_growth(0xC)` — char 0x0C = 凱麗加入
     - 若 char[43] 已死 → `fd2_display_dialog_scene(page=5)`（skip recruit）
   - 若 `tile_event_consumed_flags[0x11] != 1` → `fd2_display_dialog_scene(page=5)`
4. `current_chapter_id += 1`

雙重條件：必須觸發過特定 tile event 並保住 char[43] 才會加入凱麗。

## FDFIELD event script

- entry idx **19**（= chapter_id × 3 + 1，chapter_id=6），entry size 1171 bytes
- party_member_count = 9，char_spawn_count = 40
- header layout 見 `resource_info/fdfield.md`

turn-event hook 表（1 / 16 active，其餘 15 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 10 | 0 (enemy_turn_intro) | 0x19 | `0x00034924` | first_time_gated; ch7_first_time |

## 對話

對話文字共 6 pages 來自 FDTXT.DAT entry 7。Init handler 引用 page 0、1；End handler 依雙重招募條件分支，成功引用 page 4、失敗（條件未滿足）引用 page 5。其餘 page 2、3 為戰鬥過程中觸發的過場對白。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x000D]
『這條路是通往王城的要道，
[PAGE_BREAK]
　雖然走這條路較快，但遇到
[PAGE_BREAK]
　王國守備軍的機會也較高‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『你說的沒錯，我已經看到了
[PAGE_BREAK]
　。』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0053]
『前面那批人一定就是在普里
[PAGE_BREAK]
　茲港逞兇的匪徒，通通給我
[PAGE_BREAK]
　拿下！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『等等，大家都是自己人，
[PAGE_BREAK]
　這是一場誤會‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0053]
『少玩弄詭計想拖延時間了。
[PAGE_BREAK]
　乖乖的受死吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『唉，說不通，
[PAGE_BREAK]
　只好先打再說了。
[PAGE_BREAK]
　索爾，我們上！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『啊？我沒問題，
[PAGE_BREAK]
　妳自己可要小心點，
[PAGE_BREAK]
　不要太勉強！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『索爾，謝謝你！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x000C]
『哎呀！一大群男人追殺一個
[PAGE_BREAK]
　小姑娘，不覺得丟臉嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0022]
『妳的一拳讓我的部下得療養
[PAGE_BREAK]
　三個月，無故毆打王國駐軍
[PAGE_BREAK]
　可是重罪，乖乖的跟我回去
[PARAGRAPH]
　說個明白吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000C]
『誰教他對我亂瞄！你們王國
[PAGE_BREAK]
　軍都是這個樣子的嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0022]
『哼‥‥不和妳吵了，
[PAGE_BREAK]
　給我抓起來！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『咦！那邊有王國軍在圍攻一
[PAGE_BREAK]
　個小女孩，這是怎麼一回
[PAGE_BREAK]
　事？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『喂，我們自己都顧不了了，
[PAGE_BREAK]
　不要再去湊熱鬧‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『什麼！你能眼睜睜的看著一
[PAGE_BREAK]
　群王國軍欺負一個小女孩？
[PAGE_BREAK]
　我自己去收拾他們好了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『好啦好啦，
[PAGE_BREAK]
　我陪你去就是了，
[PAGE_BREAK]
　反正不差這幾個‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『好吧，那就上吧！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0021]
『可惡‥國王陛下，我不能再
[PAGE_BREAK]
　護衛您了‥請原諒屬下的
[PAGE_BREAK]
　無能‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『你弄錯了，我們不是來加害
[PAGE_BREAK]
　國王的！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0021]
『是嗎？‥那麼‥前些日子到
[PAGE_BREAK]
　底是誰‥‥啊‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x000C]
『多謝你們的幫忙！連王國的
[PAGE_BREAK]
　精銳部隊也不是你們對手，
[PAGE_BREAK]
　你們到底是什麼來歷？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這個說起來很麻煩，此地又
[PAGE_BREAK]
　很危險，我看妳還是回家睡
[PAGE_BREAK]
　覺好了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000C]
『什麼，你竟敢小看我！若是
[PAGE_BREAK]
　聽到我老師的名字，包準你
[PAGE_BREAK]
　嚇一大跳‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『凱麗小姐，妳的身手的確是
[PAGE_BREAK]
　很不錯，可以請問妳的老師
[PAGE_BREAK]
　是哪一位嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000C]
『這個‥還是不能告訴你！
[PAGE_BREAK]
　不過，如果你們願意讓我加
[PAGE_BREAK]
　入的話，我可以考慮透露一
[PARAGRAPH]
　點。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『歡迎！我們就要和王國軍
[PAGE_BREAK]
　大戰，正愁人手不夠，有妳
[PAGE_BREAK]
　加入真是再好不過了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x000C]
『以後我們就是伙伴了，
[PAGE_BREAK]
　請大家多多指教！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『此地不宜久留，
[PAGE_BREAK]
　凱麗小姐，
[PAGE_BREAK]
　我們這就上路吧！』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0004]
『看來我們又被人家誤會了
[PAGE_BREAK]
　，索爾，現在我們該怎麼辦
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『那還用說，當然是殺進王
[PAGE_BREAK]
　城去弄個明白！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000A]
『嗯，夠氣魄！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『我們快上路吧！這件事不
[PAGE_BREAK]
　弄個清楚，更麻煩的事還在
[PAGE_BREAK]
　後頭呢！』
[END]
```
