# 第 21 章 — chapter_21

共用 `chapter_19_20_21_init_shared`；end handler 含 6-item 收集鏈：若隊伍持有 item 0xD1..0xD6 全 6 件，則自動兌換成 item 100「天空之鑰」(FD2 隱藏機制起點)。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_19_20_21_init_shared @ 0x00033674` | 10 B (與 ch19/20 共用) |
| End | `fd2_chapter_21_end @ 0x000240FA` | 572 B |
| Post-action | `fd2_chapter_21_post_action @ 0x00020A51` | default + 額外 lose if char[0x10] OR char[0x11] dead |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[20]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[20]` |  |

## Init handler 階段

Shared minimal init (同 ch19/20)：
1. `fd2_init_battle_state_for_chapter`
2. `fd2_display_dialog_scene(page=0)`
3. `fd2_pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 21 | 0 |
| End (未收齊 6 件) | 21 | 5, 6 |
| End (收齊 6 件) | 21 | 5, 7, 8, 9, 10 |

## char_id 初始化序列

無 init handler 內 char init。希爾法 (char_id 0x18 = 24) 與羅蘭 (char_id 0x17 = 23) 由 end handler 無條件加入。

## Cutscene events

- Init：無
- End：`0x3F`, `0x40` (僅在 6-item 收齊路徑)

## Post-action handler

`fd2_chapter_21_post_action @ 0x20A51`：
- default
- 額外 lose：if `char[0x10]` OR `char[0x11]` 死亡 → game_event_flag = 1

`char[0x10]` = 希爾法 (NPC)，`char[0x11]` = 羅蘭 (NPC)。

## End handler events

`fd2_chapter_21_end @ 0x240FA` (572 B) — **6-item collection 換天空之鑰**：

1. 從 scene tables (6 entries inc facing) 讀位置
2. `fd2_setup_chars_and_camera_for_intro(0x18, 0x19, 0x17, 0xE, 1, 0xE, 0xA)` 配 6 chars
3. `fd2_display_dialog_scene(page=5)`
4. **6 件物品 collection check** — 雙重迴圈：
   ```c
   iVar5 = 0;
   for item_id in [0xD1..0xD6]:
     for char_idx in [0..0x10]:
       if fd2_find_inventory_slot_with_item(char, item_id) != -1:
         iVar5++;
   ```
5. **if `iVar5 == 6`** (全 6 件物品都被某個 char 持有)：
   - 內層迴圈移除全部 6 件 item (`fd2_remove_inventory_slot_at`)
   - `fd2_give_item_to_first_player_char(100)` — **發放 item 100 = 天空之鑰**
   - `fd2_display_dialog_scene(page=7)` + `fd2_cutscene_event_trigger(0x3F)` + `fd2_display_dialog_scene(page=8)` + `fd2_cutscene_event_trigger(0x40)` + `fd2_display_dialog_scene(page=9)` + `fd2_play_chapter_21_hidden_stage_unlock_cinematic(...)` (特殊 cinematic)
   - 後續 dialog 設 typewriter_mode=0x4A、page=10 (與 std page 6 不同的 dialog 渲染)
6. **Else** (沒收齊 6 件)：`fd2_display_dialog_scene(page=6)` (標準分支)
7. (兩路徑合流) `fd2_display_dialog_scene` 用條件決定的 page (10 若收齊 / 6 若未收齊)
8. `fd2_init_runtime_char_from_base_growth(0x18=24)` (希爾法) + `fd2_init_runtime_char_from_base_growth(0x17=23)` (羅蘭)
9. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

跨章機制 — 天空之鑰 (item 100) 兌換鏈起點：
- ch23：若持有天空之鑰，武聖卡里斯加入 (chapter_23_end 內 `fd2_any_char_has_item(100)` 觸發)
- ch27_init：條件 dialog page 3 — `fd2_any_char_has_item(100)`
- ch27_end：GOOD/BAD path — 持有則進 ch28+，未持有則悠妮獨自回黃金城

## FDFIELD event script

FDFIELD entry idx **61** (= chapter_id × 3 + 1, chapter_id=20)，entry size 2211 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=80；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**5/16 active hooks**：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 2 | 0 (enemy_turn_intro) | 0x2F | `0x00035112` | 4 角魔鬼 spawn (t2/4/6/8 phase 0 重複 reinforcement) |
| 3 | 0 (enemy_turn_intro) | 0x30 | `0x000351C6` | ai_setup |
| 4 | 0 (enemy_turn_intro) | 0x2F | `0x00035112` | 4 角魔鬼 spawn |
| 6 | 0 (enemy_turn_intro) | 0x2F | `0x00035112` | 4 角魔鬼 spawn |
| 8 | 0 (enemy_turn_intro) | 0x2F | `0x00035112` | 4 角魔鬼 spawn |
