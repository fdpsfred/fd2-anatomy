# 第 9 章 — chapter_09

Init handler 在進入對話前，迴圈把 `runtime_char_array[0..0xA]` 11 個 char 全部設為面朝 north (sprite_state[1] = 2) — cutscene 排隊用視覺效果。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_09_init @ 0x0003327D` | 174 B |
| End | `chapter_09_end @ 0x000235BC` | 61 B |
| Post-action | `check_battle_end_default_handler @ 0x000205B4` | (default) |
| BGM (player turn) | `per_chapter_player_turn_bgm[8]` |  |
| BGM (enemy turn) | `per_chapter_enemy_turn_bgm[8]` |  |

## Init handler 階段

1. `init_battle_state_for_chapter`
2. 迴圈：`runtime_char_array[0..0xA].pSprite_state[1] = 2` — 前 11 個 char 全部面朝 north
3. `pan_cursor_and_window(6, 0)`
4. `display_dialog_scene(page=0)` + `cutscene_event_trigger(0x23)`
5. `display_dialog_scene(page=1)`
6. `pan_cursor_to_char(0)` + `clear_all_chars_facing`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 9 | 0, 1 |
| End | 9 | 4 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。
End handler 透過 `runtime_char_array[0xB].bFlags = 0` 復活 char[11]（非新加入；ch9 init 已預初始化但 marked dead）。

## Cutscene events

`0x23` (init), `0x24` (end)。

每 event 對應 `cutscene_event_script_table[event_id] @ 0x627D8` 的 walk-animation script。

## Post-action handler

`per_chapter_post_action_handler[8]` 指向 `check_battle_end_default_handler` — 全敵死 = win，索爾 (char_id 0) 死 = lose。

攻略「萊汀被打敗時敵方騎兵援軍立即出現」由 FDFIELD event 處理，不在 post_action handler。

## End handler events

`chapter_09_end @ 0x235BC`：

1. `runtime_char_array[0xB].bFlags = 0` — 復活 char[11]（清死亡 flag）
2. `pan_cursor_and_window(6, 1)` + `load_chapter_portraits_and_dump_tmp(4)`
3. `cutscene_event_trigger(0x24)`
4. `display_dialog_scene(page=4)`
5. `save_runtime_char_to_template`
6. `current_chapter_id += 1`

無 `init_runtime_char_from_base_growth`——char[11] 是 revive 而非新加入。

## FDFIELD event script

FDFIELD entry idx **25** (= chapter_id × 3 + 1, chapter_id = 8)，entry size 1691 bytes。
header `+0..+2` = shap_id_byte / party_count = 11 / char_count = 60；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

2 / 16 hook entries（其餘 14 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0 (enemy_turn_intro) | 0x1F | `0x00034B5D` | turn=0xFF dormant entry — 不會在初始 hook table 狀態下 fire |
| 0xFF | 0 (enemy_turn_intro) | 0x1F | `0x00034B5D` | 同上 dormant entry |

`fire_chapter_turn_events_for_phase` 比對 `turn == save_metadata_block` — 0xFF 永遠不會等於回合計數，故這些 entries 不會在初始狀態下 fire。觸發機制：tile-step-event handlers 在某些劇本 tile 被踩到時，動態 rewrite 本 chapter 的 turn-event-hook table 的 turn byte (0xFF → current_save_metadata_block 或 +1)，把原本 dormant 的 entry 啟動成下一回合 fire 的 event。
