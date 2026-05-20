# 第 17 章 — chapter_17

Conditional init handler：依隊上是否有蜜蒂 (char_id 0x12) 而分支載入肖像，並影響 end handler 走 page 5 (有蜜蒂) 或 page 7 (無蜜蒂，含告別 cutscene)。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_17_init @ 0x000335AA` | 48 B |
| End | `chapter_17_end @ 0x00023B5F` | 374 B |
| Post-action | `chapter_17_post_action @ 0x00020872` | default + 條件 lose |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[16]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[16]` |  |

## Init handler 階段

Conditional：

```c
init_battle_state_for_chapter
iVar3 = check_party_has_char_id(0x12)   // 蜜蒂?
if (iVar3 == 0) {
    load_chapter_portraits_and_dump_tmp(1)   // 隊伍無蜜蒂 → 載她的肖像 (NPC 出戰用)
}
display_dialog_scene(page=0)
pan_cursor_to_char(0)
```

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 17 | 0 |
| End (no Mediti) | 17 | 7 (告別) → 6 → 8 |
| End (Mediti present) | 17 | 5 → 6 → 8 |

## char_id 初始化序列

無 init handler 內 char init。凱拉斯 (char_id 0x10 = 16) 由 end handler 加入。

## Cutscene events

- Init：無
- End：`0x32`, `0x33` (僅在 no-Mediti 分支), `0x35` (always)

## Post-action handler

`chapter_17_post_action @ 0x20872`：
- default
- 額外 lose：if `check_party_has_char_id(0x12) == 0` (蜜蒂未在隊伍) **AND** `char[0x34]` 死亡 → display_dialog_scene(page=2) + lose

`char[0x34]` = 蜜蒂 NPC slot — 蜜蒂上一章未招募時以 NPC 形式出戰，此 NPC 戰死即敗。

## End handler events

`chapter_17_end @ 0x23B5F` (374 B)：

1. 從 scene tables 讀位置
2. `save_runtime_char_to_template`
3. **Conditional**：`check_party_has_char_id(0x12)`
   - 若 **無蜜蒂** → `setup_chars_and_camera_for_intro(0xF, 0x34, 0x17, 0x17, 2, 0x11, 0x11)` + `display_dialog_scene(page=7)` + `cutscene_event_trigger(0x32)` + pan + `load_chapter_portraits_and_dump_tmp(3)` + `cutscene_event_trigger(0x33)` (蜜蒂告別)
   - 若 **有蜜蒂** → `display_dialog_scene(page=5)` + pan + `load_chapter_portraits_and_dump_tmp(3)`
4. (兩路徑合流) `display_dialog_scene(page=6)` + `cutscene_event_trigger(0x35)` + `display_dialog_scene(page=8)`
5. `init_runtime_char_from_base_growth(0x10=16)` — 凱拉斯加入
6. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **49** (= chapter_id × 3 + 1, chapter_id=16)，entry size 1691 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=60；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**1/16 active hooks**：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 4 | 1 (end_of_player_turn) | 0x28 | `0x00034FCB` | dialog_with_state |
