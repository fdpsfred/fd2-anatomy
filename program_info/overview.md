# 整體架構

FD2.LE 是 Watcom C/C++ 9.5a 編譯的 DOS 32-bit Linear Executable（見
`rebuild_info/crt/fid_match.md`），搭配 DOS/4GW Protected Mode Extender 在 386+
環境執行。畫面用 VGA mode 13h (320×200×256 色)，音訊用 Miles Sound System
(AIL) 走連結進來的 driver。

## 模組與記憶體佈局

執行時的 segment 由 LE loader 配置，主要分三大 object：

```
0x00010000  .object1 (code, ~252 KB)
  0x00010000-0x0004EBD8   game logic + Watcom CRT + Miles AIL library
                          (在 binary 中 interleaved — 不是分區擺放)
  0x0003C964              entry point (crt_entry_start)
0x00050000  .object2 (靜態資料 + runtime state)
  0x00050000-0x00053900   string tables, jump tables, lookup data
  0x00053A00-0x000543FF   runtime variables (cursor pos, char array 等)
0x00060000  .object3 (initialization image, 遊戲資料表)
  0x000602AC data_fd2_battle_item_effect_table[215]
  0x000619FD data_fd2_battle_spell_effect_table[36]
  0x00061AF9 data_fd2_battle_enemy_data_table[68]
  0x00061DA1 data_fd2_battle_character_base_table[32]
  0x000620A1 data_fd2_battle_character_growth_table[68]
  0x0006238D data_fd2_chapter_intro_metadata_table[26]
  0x000626B3 data_fd2_battle_spell_learning_table[20]
```

`.object1` 內 FD2 自寫遊戲邏輯、Watcom C runtime、Miles Sound System library
這三類函式 **互相交錯擺放**（Watcom linker 沒有依模組分區）。判別任一函式屬於
哪一類必須看：函式名稱前綴（`crt_*` / `AIL_*` / 已命名 game function）、callee
模式、字串引用，不能依 address range。

## 執行流程

```
crt_entry_start (0x3C964)
 └─ crt_main_trampoline (0x45D4B)  ← Watcom CRT startup
     └─ fd2_main (0x25BF4)
         ├─ AIL_startup() — audio init
         ├─ load .DAT resources (FDTXT/FDOTHER/FDFIELD/FDSHAP/DATO/FDMUS/...)
         ├─ malloc 大型 buffer (game state 152 KB 等)
         └─ outer loop:
             ├─ draw_main_menu
             ├─ main_menu_continue_dispatcher (NEW GAME / CONTINUE)
             ├─ if entered game:
             │   ├─ chapter_init_jump_table[chapter_id]() — 30 章 init
             │   └─ inner per-chapter loop:
             │       ├─ game_main_loop()  — per-frame input dispatcher
             │       │   ├─ fd2_wait_for_input_with_idle (cursor blink + palette cycle)
             │       │   ├─ scancode dispatch (cursor moves / actions / menus)
             │       │   ├─ fd2_player_action_menu_loop (玩家選行動)
             │       │   ├─ fd2_field_command_menu_loop (Save/EndTurn/Suspend modal)
             │       │   ├─ fd2_open_char_status_screen (角色狀態)
             │       │   ├─ fd2_enemy_turn_phase_team0 (敵方 AI phase)
             │       │   └─ fd2_npc_turn_phase_team1 (友方 NPC AI phase)
             │       ├─ if (game_event_flag == 1): fd2_play_chapter_clear_fanfare
             │       └─ if (game_event_flag == 2):
             │           ├─ chapter_end_jump_table[chapter_id]()
             │           ├─ fd2_chapter_transition_menu (save/continue prompt)
             │           └─ chapter_init_jump_table[next_chapter]()
             └─ if game_over: break outer loop
```

關鍵：FD2 沒有獨立的 `battle_loop()` function。戰鬥就是 `fd2_main` 的內迴圈反覆呼叫
`fd2_game_main_loop`，每 frame 處理一個輸入或繼續動畫。chapter init 把地圖、敵人配置好之後，
`fd2_game_main_loop` 自己跑，直到 `game_event_flag` 變成 1（主角索爾死）或 2（敵全滅）。

## 12 個 systems 概觀

| System | 主要 functions | 進入點 |
|---|---|---|
| **lifecycle** | fd2_main, AIL_startup/shutdown, fd2_load_save_and_init_engine | crt_main_entry |
| **resource** | fd2_load_dat_resource | 各 system 自己呼叫 |
| **save_load** | save/load FD2.SAV (8 helpers), fd2_field_menu_status_save_load_quit_dispatch | fd2_field_command_menu_loop |
| **field_map** | 60 個 chapter init/end handlers, fd2_chapter_transition_menu | jump tables |
| **battle** | fd2_attack_action_dispatch (AI 三路 score), 12-class enemy AI dispatcher | fd2_enemy_turn_phase_team0 |
| **ui_menu** | fd2_game_main_loop, cursor moves, fd2_player_action_menu_loop, fd2_field_command_menu_loop | scancode dispatch |
| **text_dialog** | fd2_display_dialog_scene, fd2_blit_glyph_2bpp_with_outline, portrait cache | fd2_display_dialog_scene |
| **animation** | fd2_play_figani_animation_loop, slide animations | spell/cinematic 執行 |
| **graphics** | fd2_rle_blit_sprite, fd2_blit_rectangle, fd2_composite_battle_tile_map | fd2_composite_battle_frame |
| **audio** | AIL wrappers + fd2_play_sfx_with_handle | per-event |
| **input** | fd2_wait_for_input_with_idle, BIOS keyboard direct access | fd2_game_main_loop |
| **table_accessor** | 5 個 get_*_entry helpers | 各 system 查表 |

另有跨系統的 **chapter_event_dispatch**：FDFIELD 章節 event hook table → jump
table @ 0x51B91 → 編譯好的 cinematic C 函數的 dispatch 機制（同一張表也被
AI post-action consequence 共用）。

四 pool 分類 (`ail` / `crt` / `fd2` / `binary_artifact`)、entry chain
(`crt_equivalent_entry_start` → `crt_equivalent_dos_main_bootstrap` → `__CMain`
→ `fd2_main`)、結局 cinematic、binary_artifact pool 的 alignment NOP 詳見
`rebuild_info/emission/pool_routing.md`；Watcom CRT 真符號 inventory 見
`rebuild_info/crt/lookup_9.5a.json` 與 `matched_function_sources.md`，
15 個 `crt_equivalent_*` / 10 個 `fd2_*` CRT-style primitive 見
`rebuild_info/crt/symbol_inventory.md`。

## 共享資料結構

### `runtime_char` struct (80 bytes = 0x50)

每個玩家/敵人/NPC 的執行時狀態。陣列 `runtime_char_array` 在 `0x00053A45`，
總長度由 `party_member_count` 在 `0x00053BEB`。

```
+0x00 bPos_x          位置 X
+0x01 bPos_y          位置 Y
+0x02 pSprite_state[3]  [0]=sprite_cache_idx [1]=facing(0=down) [2]=walk_anim_phase
+0x05 bFlags          bit 0x01 = dead；bit 0x04 = cannot act；bit 0x80 = acted-this-turn
+0x06 bTeam           0 = enemy, 1 = NPC ally, 2 = player
+0x07 bPortrait_id    portrait sprite 索引
+0x08 bChar_id         char_id (0..0x43 player / 0x44+ enemy)；
                      AI scoring / fd2_find_char_by_id_or_template 等以此判定身份。
                      值 0 觸發 flanking +50% 判斷。
+0x09 bReserved_padding_09  reserved padding。只有兩個 init 函式
                      (fd2_init_runtime_char_for_battle @ 0x10c50、
                      fd2_init_runtime_char_from_base_growth @ 0x112a5) 寫 0；
                      AI / combat / save / death / XP / item-use / cutscene
                      paths 無任何讀取點。save/load 透過 0x50-byte memcpy
                      整段保留但無語意讀取。
+0x0A pInventory_slots[8]   8 × (bSlot_flag, bItem_id)
                      slot_flag bit 0x40 = equipped, bit 0x80 = empty
+0x1A pSpells_known_bitmap[5]   40 spells × 1 bit
+0x1F bArchetype_flag  fd2_game_main_loop 檢查值 == 10 特殊化（boss/NPC 種類）
+0x20 bJob_id         職業 ID，決定移動/抗性
+0x21 pStatus_flags_block[5]
                      [0] level (init = base[2])
                      [1] AP buff flag (fd2_recalculate_combat_stats AP × 1.15, 截斷)
                      [2] DP buff flag (fd2_recalculate_combat_stats DP × 1.15, 截斷)
                      [3] DX buff flag (+0xF)
                      [4] 狀態 A (毒？AI fd2_score_spell_candidate spell 0x14 檢查)
+0x26 bStatus_sleep_flag  spell 0x15 解；scorer +6 if non-zero
+0x27 pCombat_aux_block[21]
                      [0]      bSilence_flag
                      [1..9]   reserved padding (9 bytes)。AI / combat /
                               status / item / cutscene 等 path 無讀寫；
                               兩個 init 函式不寫；save/load 走 memcpy
                               整段保留但無語意讀取
                      [0xA]    bPickup_kind (init from char_spawn_record +0x16)
                      [0xB-C]  wPickup_param (ushort)
                      [0xD]    bAi_class_and_flags
                               low nibble = AI behavior class (0..11)
                               bit 0x01 = heal-boost (×2 score)
                               bit 0x40 = tie-break modifier
                               bit 0x80 = 高價值/脆弱目標 (×3 score)
                      [0xE]    bAi_aux_byte (movement param)
                      [0xF]    bAi_target_pos
                      [0x10-0x11] wAP_total (ushort)
                      [0x12-0x13] wDP_total (ushort)
                      [0x14]   bMagic_resist (= base[7])
+0x3C bMovement_order  0 = 已動 / 0xFF = 未動 (init: player=0, NPC/enemy=0xFF)
+0x3D pAi_target_and_DX_block[3]
                      [0] bAi_target_id
                      [1-2] wDX_total (ushort)
+0x40 wHP_current
+0x42 wHP_max
+0x44 wMP_current
+0x46 wMP_max
+0x48 wAP             AP after equipment + status
+0x4A wDP             DP after equipment + status
+0x4C wDX_current     DX_total = base + status[3]*0xF + sum item.short@+3
+0x4E wStat4_current  4th derived stat (init = DX_total + sum item.short@+7)
```

`runtime_char` 詳細欄位語意與如何被各個 scorer/動畫/damage 函式使用，見
`battle.md`。

### .object3 遊戲資料表

| 表 | 結構 size | 數量 | 詳細 schema |
|---|---|---|---|
| `data_fd2_battle_item_effect_table` | 23 B | 215 | `assets/tables/item_effect.md` |
| `data_fd2_battle_spell_effect_table` | 7 B | 36 | `assets/tables/spell_effect.md` |
| `data_fd2_battle_enemy_data_table` | 10 B | 68 | `assets/tables/enemy_data.md` |
| `data_fd2_battle_character_base_table` | 24 B | 32 | `assets/tables/character_base.md` |
| `data_fd2_battle_character_growth_table` | 11 B | 68 | `assets/tables/character_growth.md` |
| `data_fd2_chapter_intro_metadata_table` | 31 B | 26 | `assets/tables/chapter_intro_metadata.md` |
| `data_fd2_battle_spell_learning_table` | 12 B | 20 | `assets/tables/spell_learning.md` |
| `data_fd2_battle_job_magic_resist_table` | dword × 27 | 27 | `assets/tables/job_magic_resist.md` |
| `data_fd2_battle_job_crit_rate_table` | byte × 27 | 27 | `assets/tables/job_crit.md` |

## 關鍵遊戲機制

1. **Enemy AI 12 種 behavior class** (`pCombat_aux_block[0xD] & 0x0F`) — 每個
   敵人的「個性」，從 class 0「只會打」到 class 11「智者 spell-then-physical」
   到 class 8「完全不動」。詳 `battle.md`。
2. **AI kill-shot 加權**：物理 score = 0x12 (18)、道具 score = 0x12、**法術
   score = 0x18 (24)**。敵法師會優先用魔法秒殺玩家殘血。
3. **20% HP_max idle heal** (`fd2_ai_pass_turn_with_heal`)：AI 待機自動恢復 20%
   最大 HP。FD2 重型敵人「拖不死」的程式根源。
4. **Two-pass enemy phase**：smart caster 第一輪先動（佔 AoE 位置與秒殺），
   melee 第二輪。AI 戰術設計，不是 bug。
5. **Mirror dialog blit** (`fd2_dialog_sprite_blit_mirrored`)：友軍對話用右→左
   pixel order blit，產生「兩人面對面」視覺效果。
6. **20-bit pitch state** (AIL mixer)：`ail_mix_pitch_int/_low/_int_plus_one`
   是 16.16 fixed-point pitch increment；雙倍 stereo / 16-bit 時 left-shift 一次。
7. **Save data obfuscation** (`fd2_save_crypt_buffer`)：XOR-style involution
   (同一 function 加密與解密)；`fd2_save_compute_checksum` 是 4-byte 快速 checksum
   (非 CRC32，作為完整性檢查 / cheat deterrent，不是 cryptographic 安全)。
