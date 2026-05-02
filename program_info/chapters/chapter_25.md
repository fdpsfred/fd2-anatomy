# 第 25 章 — chapter_25

唯一在 init 階段呼叫 `load_dat_resource` 載入 FDOTHER[0x58] 地震音效的章節，開場 4 連震 cutscene；end 在 `save_runtime_char_to_template` 之後再 init 亞奇梅吉，導致該角色不被 save template 包含。

## Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `chapter_25_init @ 0x0003396A` | 324 B |
| End | `chapter_25_end @ 0x00024DF2` | 142 B |
| Post-action | `chapter_25_post_action @ 0x00020B14` | default + lose if char[0x10] dead |
| BGM (player turn) | `per_chapter_player_turn_bgm[24]` |  |
| BGM (enemy turn) | `per_chapter_enemy_turn_bgm[24]` |  |

## Init handler 階段

開場 4 連震 cutscene + 152 KB game state buffer 清空：

1. `init_battle_state_for_chapter`
2. `sfx_play_handle_table = NULL` 清空
3. `sfx_play_handle_table = load_dat_resource("FDOTHER.DAT", idx=0x58)` — 載入地震 sfx wave
4. `pan_cursor_and_window(5, 0)`
5. `display_dialog_scene(page=1)` (從 page 1 起，非 page 0)
6. `crt_memset(large_game_state_buffer, 0, 0x25680)` — 清 152 KB game state buffer
7. **4× 連續地震**：`play_sfx_with_handle(sfx_play_handle_table)` + `animate_screen_shake(strength)` + 600ms 等待
   - 第 1、2、3 次：strength = 0x14
   - 第 4 次：strength = 0x3C (3 倍長度 climax)
8. `display_dialog_scene(page=2)`
9. `pan_cursor_to_char(0)` + `play_and_free_status_effect_sfx`

Page 0 在此 init 路徑未被引用，可能保留給 alternate dialog beat。

## Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 25 | 1, 2 |
| End | 25 | 6, 7 |

## char_id 初始化序列

End handler 內：
- char_id 26 (0x1A) → 龍劍士聖寇拉斯 (在 `save_runtime_char_to_template` 之前 init)
- char_id 29 (0x1D) → 大法師亞奇梅吉 (在 `save_runtime_char_to_template` 之後 init — 不被 saved template 包含)

## Cutscene events

- Init: 無 `cutscene_event_trigger`（用 `play_sfx_with_handle + animate_screen_shake` × 4 自製 cutscene）
- End: `0x4B`

## Post-action handler

`chapter_25_post_action @ 0x00020B14`：
- 標準 default 判定（全敵死 = 勝、索爾死 = 負）
- **額外 lose 條件**：if chars[0x10] (聖寇拉斯) 死 → `game_event_flag = 1`

## End handler events

`chapter_25_end @ 0x00024DF2` (142 B) — 2 chars 加入 (其中 1 在 save 後)：

1. `display_dialog_scene(page=6)`
2. `pan_cursor_and_window(4, 0x10)` + `load_chapter_portraits_and_dump_tmp(2)`
3. `cutscene_event_trigger(0x4B)`
4. `display_dialog_scene(page=7)`
5. `init_runtime_char_from_base_growth(0x1A)` — 聖寇拉斯加入 (char 26)
6. `save_runtime_char_to_template`  ← save 在此
7. `init_runtime_char_from_base_growth(0x1D)` — 亞奇梅吉加入 (char 29)，**在 save 之後，不被 template 保留**
8. `current_chapter_id += 1`

亞奇梅吉每次進入 ch26 時可能由 ch26 init 重新初始化，或屬有意設計的「runtime-only」加入。

## DAT resources loaded (init)

| File | idx | 用途 |
|---|---|---|
| FDOTHER.DAT | 0x58 | 地震 sfx wave (chapter 25 開場 4 連震) |

## FDFIELD event script

FDFIELD entry idx **73** (= chapter_id × 3 + 1, chapter_id=24)，entry size 1951 bytes。
header `+0..+2` = shap_id_byte / party_count=16 / char_count=70；`+3..+50` = 16×3 turn-event hooks；`+51..` = char_spawn_records。

1 / 16 active hooks (其餘 15 為 sentinel)：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 6 | 1 (end_of_player_turn) | 0x38 | `0x00035487` | dialog_with_state (ch25 對話事件) |
