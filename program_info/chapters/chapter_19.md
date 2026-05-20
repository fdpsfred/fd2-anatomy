# 第 19 章 — chapter_19

共用 init `chapter_19_20_21_init_shared` (10 B，三章共用)；post_action 用 `save_metadata_block > 6` gate 巴拿羅西亞 (char[0x40]) 的 lose 條件。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_19_20_21_init_shared @ 0x00033674` | 10 B (與 ch20/21 共用) |
| End | `chapter_19_end @ 0x00023E39` | 59 B (trivial) |
| Post-action | `chapter_19_post_action @ 0x00020926` | default + gated lose |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[18]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[18]` |  |

## Init handler 階段

Shared minimal init (10 B)：

1. `init_battle_state_for_chapter`
2. `display_dialog_scene(page=0)` (從 ch19 的 FDTXT entry)
3. `pan_cursor_to_char(0)`

ch19/20/21 三章共用同一 init handler — 三章間沒有 init 時序差異，所有差異都在 post_action handler 與 FDFIELD.DAT entry。共用 init 對 30 章來說獨此 1 例。

## Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 19 | 0 |
| End | 19 | 3 |

## char_id 初始化序列

無 init handler 內 char init。巴拿羅西亞由 FDFIELD turn 6 reinforcement spawn 加入。

## Cutscene events

無 init / end cutscene。

## Post-action handler

`chapter_19_post_action @ 0x20926`：

- default 判定 (敵全死=勝、索爾死=負)
- **gated lose**：if `save_metadata_block > 6` AND `char[0x40]` 死亡 → game_event_flag = 1

`save_metadata_block > 6` 對應「巴拿羅西亞已加入後」(時序 gate)；在巴拿羅西亞尚未出現之前，char[0x40] 戰死不算敗。

## End handler events

`chapter_19_end @ 0x23E39` (59 B) — trivial：

1. `save_runtime_char_to_template`
2. `display_dialog_scene(page=3)`
3. `current_chapter_id += 1`

無 cutscene、無加入。

## FDFIELD event script

FDFIELD entry idx **55** (= chapter_id × 3 + 1, chapter_id=18)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

**3/16 active hooks**：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 4 | 2 (new_player_turn_intro) | 0x2C | `0x000350A4` | ai_setup |
| 6 | 1 (end_of_player_turn) | 0x2E | `0x000350CC` | reinforcement_spawner (巴拿羅西亞登場) |
| 10 | 2 (new_player_turn_intro) | 0x2D | `0x000350B9` | ai_setup |
