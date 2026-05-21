# 第 15 章 — chapter_15

第 15 章是 30 章中首例 conditional dialog branch — Init 與 End handler 都依「隊伍是否含凱麗 (char_id 0xC)」切兩組對話 page。Post-action 含「賽可邦勒死亡」自訂 lose 條件。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_15_init @ 0x000334D9` | 199 B (首章帶 conditional dialog branch) |
| End | `fd2_chapter_15_end @ 0x000239BD` | 77 B |
| Post-action | `fd2_chapter_15_post_action @ 0x00020822` | (custom) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[14]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[14]` |  |

## Init handler 階段

dialog page 索引依 `fd2_check_party_has_char_id(0xC)`（凱麗）決定：

```c
uVar3 = fd2_check_party_has_char_id(0xC);  // 隊上是否有 char_id 12 (凱麗)?
bVar1 = (byte)uVar3 ^ 1;        // 反轉 (有 → 0、無 → 1)
page_idx = (ushort)bVar1 * 3;   // (有 → 0、無 → 3)
fd2_display_dialog_scene(page_idx);             // page 0 OR 3
fd2_pan_cursor_and_window(0x18, 0x11);
fd2_display_dialog_scene(page_idx + 1);         // page 1 OR 4
fd2_cutscene_event_trigger(0x30);
fd2_display_dialog_scene(page_idx + 2);         // page 2 OR 5
fd2_pan_cursor_to_char(0);
```

- 隊伍含凱麗 → dialog pages 0, 1, 2
- 隊伍不含凱麗 → dialog pages 3, 4, 5

`fd2_check_party_has_char_id` 檢查 `runtime_char[+0x8] = char_id_init`。

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init (含凱麗) | 15 | 0, 1, 2 |
| Init (不含凱麗) | 15 | 3, 4, 5 |
| End (含凱麗) | 15 | 12 |
| End (不含凱麗) | 15 | 13 |

## char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 中：
- `fd2_init_runtime_char_from_base_growth(0xF = 15)` — 賽可邦勒

## Cutscene events

`0x30` (init)。End 無 cutscene event。

## Post-action handler

`fd2_chapter_15_post_action @ 0x20822`：

- default 判定（全敵死 = win，索爾死 = lose）
- 額外 lose 條件：char[0x40 = 64] 死亡

對應「失敗條件：索爾死亡，賽可邦勒死亡」——char[0x40] = 賽可邦勒。

「前三回合豹人會往左退，第四回合開始攻擊敵軍」屬 NPC AI behavior（FDFIELD turn-event 或 NPC behavior class 切換）。

## End handler events

`fd2_chapter_15_end @ 0x239BD`（第二處 conditional dialog）：

```c
uVar1 = fd2_check_party_has_char_id(0xC);                 // 隊上有凱麗?
page_idx = ((byte)uVar1 ^ 1) + 0xC;                   // 0xC if 凱麗 present, 0xD if not
fd2_display_dialog_scene(page=page_idx);
fd2_save_runtime_char_to_template;
fd2_init_runtime_char_from_base_growth(0xF = 15);         // 賽可邦勒加入
current_chapter_id += 1;
```

- 隊伍含凱麗 → dialog page 12 (0xC)
- 隊伍不含凱麗 → dialog page 13 (0xD)

整章 dialog 設計都依「隊伍是否含凱麗 (char 12)」分兩線。

## FDFIELD event script

FDFIELD entry idx **43** (= chapter_id × 3 + 1, chapter_id = 14)，entry size 2211 bytes。
header `+0..+2` = shap_id_byte / party_count = 16 / char_count = 80；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

3 / 16 active hooks（其餘 13 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 4 | 1 (end_of_player_turn) | 0x0D | `0x00034E90` | dialog_with_state |
| 9 | 0 (enemy_turn_intro) | 0x12 | `0x00034F02` | dialog_with_state |
| 7 | 0 (enemy_turn_intro) | 0x26 | `0x00034F42` | dialog_only |
