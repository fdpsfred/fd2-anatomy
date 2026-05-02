# 第 13 章 — chapter_13

Init handler 僅 17 B（30 章中最小），戰鬥邏輯全部位於 189 B 的 `chapter_13_post_action` — 第 13 章是唯一明顯「init / post_action 工作量倒置」的章節。Post-action 含「精靈族全滅」與「哈瓦特出場後死亡」兩條件。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_13_init @ 0x0003346B` | 17 B (最小 init) |
| End | `chapter_13_end @ 0x0002389F` | 61 B |
| Post-action | `chapter_13_post_action @ 0x00020765` | 189 B (最複雜 non-default) |
| BGM (player turn) | `per_chapter_player_turn_bgm[12]` |  |
| BGM (enemy turn) | `per_chapter_enemy_turn_bgm[12]` |  |

## Init handler 階段

極簡：

1. `init_battle_state_for_chapter`
2. `display_dialog_scene(page=0)`
3. `pan_cursor_to_char(0)`

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 13 | 0 |
| Post-action | 13 | 2, 10 |
| End | 13 | 9 |

## char_id 初始化序列

Init handler 內無 `init_runtime_char_from_base_growth` 呼叫。

End handler 中：
- `init_runtime_char_from_base_growth(3)` — 哈瓦特

End handler **無條件**呼叫 `init_char(3)`，不檢查 ch13_post_action 是否觸發過 char[0x3B]（哈瓦特出場 trigger）。「有沒有出現都會加入」是因為加入邏輯獨立於戰場出場邏輯。

## Cutscene events

無 init / end cutscene event。

## Post-action handler

`chapter_13_post_action @ 0x20765`（最複雜 non-default handler）：

1. default 判定（敵全死 = 勝、索爾死 = 負）
2. 條件 1：if `chars[0xF..0x1A]`（12 個 NPC = 精靈族）全部死亡 → `display_dialog_scene(page=10)` + `game_event_flag = 1` (lose)
3. 條件 2：if `save_metadata_block > 5`（回合計數 > 5）AND `char[0x3B]` 死亡 → `display_dialog_scene(page=2)` + `game_event_flag = 1` (lose)

對應：
- 「失敗條件：索爾死亡，精靈族全滅」→ chars[0xF..0x1A] 12 個 = 精靈族
- 「第四回合己方結束時，哈瓦特在下方出現前來幫忙」→ 條件 2 中 char[0x3B] = 哈瓦特，`save_metadata_block > 5` = 第 6 回合起；若哈瓦特出現後（>5 回合）戰死則 page 2 dialog + lose

## End handler events

`chapter_13_end @ 0x2389F`：

1. `display_dialog_scene(page=9)`
2. `save_runtime_char_to_template`
3. `init_runtime_char_from_base_growth(3)` — 哈瓦特加入
4. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **37** (= chapter_id × 3 + 1, chapter_id = 12)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count = 15 / char_count = 70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

2 / 16 active hooks（其餘 14 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 4 | 1 (end_of_player_turn) | 0x05 | `0x00034D68` | thunk → FUN_00034BE7 (load_chapter_portraits race=1 + dialog page 1) |
| 9 | 0 (enemy_turn_intro) | 0x07 | `0x00034D72` | dialog_with_state |
