# 整體架構

FD2.LE 是 Watcom C/C++ 9.5a 編譯的 DOS 32-bit Linear Executable（見
`rebuild_info/crt/fid_match.md`），搭配 DOS/4GW Protected Mode Extender 在 386+
環境執行。畫面用 VGA mode 13h (320×200×256 色)，音訊用 Miles Sound System
(AIL) 走連結進來的 driver。

## 驗證對象

- src：`life/main.c`（`main`、`fd2_main_menu_dispatcher`、`fd2_load_save_and_init_engine`
  以及 lifecycle 全域資料的 owner 檔）。
- 主要 Ghidra 對象（位址即時核對）：
  - `_cstart_ @ 0x3C964` — Watcom 9.5a CRT 進入點（LE binary EIP）
  - `__CMain @ 0x45D4B` — CRT main bridge
  - `main @ 0x25BF4` — 遊戲進入點
  - `fd2_load_chapter_battle_data @ 0x1088D` — 每章 battle 資料載入
  - `fd2_init_runtime_char_for_battle @ 0x10C50` — runtime_char 佈局來源
  - `fd2_check_battle_end_condition @ 0x205BE` — 勝敗旗標判定
  - 資料表 accessor（`fd2_get_*_entry`，逐一清單見 `table.md`）
- 相關資源檔：`FD2.SAV`（存檔）、`FDOTHER.DAT` / `FDTXT.DAT` / `FDFIELD.DAT` /
  `FDSHAP.DAT` / `FDICON.B24`（章節、UI、字模、portrait 資源）。

## 模組與記憶體佈局

執行時的 segment 由 LE loader 配置，主要分三大 object（範圍即時取自 Ghidra segment 表）：

```
0x00010000  .object1  0x00010000 - 0x0004EBD8   code：遊戲邏輯 + Watcom CRT + Miles AIL
                                                 （三類在 binary 中互相交錯，不分區擺放）
                       進入點 _cstart_ @ 0x0003C964
0x00050000  .object2  0x00050000 - 0x000556AF   靜態資料 + BSS runtime state
                                                 低位址：字串表 / jump table / lookup 資料
                                                 高位址：runtime 變數（cursor、
                                                 runtime_char 陣列指標 @ 0x53A45、
                                                 party_member_count @ 0x53BEB 等）
0x00060000  .object3  0x00060000 - 0x000634D1   initialization image（遊戲資料表）
  0x000602AD data_fd2_battle_item_effect_table[215]        (0x17 B/entry)
  0x000619FD data_fd2_battle_spell_effect_table[36]        (7 B/entry)
  0x00061AF9 data_fd2_battle_enemy_data_table[68]          (0xA B/entry)
  0x00061DA1 data_fd2_battle_character_base_table[32]      (0x18 B/entry)
  0x000620A1 data_fd2_battle_character_growth_table[68]    (0xB B/entry)
  0x0006238D data_fd2_chapter_intro_metadata_table[26]     (0x1F B/entry)
  0x000626B3 data_fd2_battle_spell_learning_table[20]      (0xC B/entry)
```

`.object1` 內 FD2 自寫遊戲邏輯、Watcom C runtime、Miles Sound System library
這三類函式 **互相交錯擺放**（Watcom linker 沒有依模組分區）。判別任一函式屬於
哪一類必須看：函式名稱前綴（`crt_*` / `AIL_*` / 已命名 game function）、callee
模式、字串引用，不能依 address range（見
`rebuild_info/equivalence/pool_classification.md`）。LE segment 的完整佈局與 wlink
設定見 `rebuild_info/link/le_layout.md`。

## 啟動與主迴圈

life 模組（`main.c`）負責整個 process 生命週期：從 CRT 交棒進 `main`，做完一次性
初始化後跑外層主選單／內層章節迴圈，最後關閉 AIL 與還原文字模式退出。

```
LE loader (EIP = 0x3C964)
 └─ _cstart_ @ 0x3C964          Watcom 9.5a CRT startup（vendor CSTART3S）
      · 偵測 DOS extender：INT 21h AX=FF00h / DX=78h → Rational DOS/4G，variant_id = 1
        （另辨 Phar Lap 386|DOS variant_id=0x22、Intel CodeBuilder variant_id=9）
      · 存 PSP / cmdline / 環境區塊、REP STOSD 清 BSS
      ├─ __InitRtns @ 0x45D9A   C++ 靜態建構鏈（XI ctor list）
      └─ __CMain @ 0x45D4B      → __CommonInit（atexit / stdio / math init）
           └─ main @ 0x25BF4    （life/main.c）
                ── 一次性初始化 ──
                ├─ AIL_startup + install MDI/DIG driver + 配置 sequence / sample handle
                ├─ 8 次 fd2_load_dat_resource：FDOTHER 的 SFX bank(0x1F)/游標(1)/對話框(2)/
                │   tile 動畫(3)/中文字模(4)/UI 動畫(5)/portrait(6) + FDTXT 全文字(0)
                ├─ 3 次 malloc：tile-event consumed flags(0x20)、
                │   大型 game state buffer(0x25680)、menu 隊伍 roster(0xA00)
                ├─ INT 10h 設 VGA mode 13h
                └─ RNG 暖機（見下方「亂數暖機」）
                ── 外層迴圈 do { … } while (menu_result == 0) ──
                ├─ fd2_set_bgm_track_with_fade(0x12)   主選單 BGM
                ├─ fd2_main_menu_dispatcher()          標題／存檔選單
                │    · NEW GAME  → 章 0 init handler + 章 BGM
                │    · CONTINUE  → 讀 FD2.SAV slot（fd2_save_slot_selector_ui）
                │    · 其他       → fd2_load_save_and_init_engine（LOAD GAME）
                └─ 若 menu_result == 0（進入遊戲）:
                     └─ 內層迴圈 do { … } while (game_loop_result == 0):
                          ├─ fd2_game_main_loop()      per-frame 輸入 / AI / 動畫 dispatcher
                          ├─ end_code == 1（fd2_check_battle_end_condition 判主角索爾陣亡＝落敗）:
                          │    播 game-over 過場 sprite（fd2_play_game_over_sequence）→ 回主選單
                          └─ end_code == 2（敵全滅＝過關）:
                               ├─ data_fd2_chapter_end_handler_table[chapter_id]()   章結束 handler
                               ├─ fd2_chapter_transition_menu()                       存檔／續戰提示
                               └─ 未退出 → data_fd2_chapter_init_handler_table[chapter_id]() + 章 BGM
   離開外層迴圈 → AIL_shutdown → INT 10h 還原 text mode 3
```

`data_fd2_chapter_event_or_battle_end_code`（@ 0x53ECC）是主迴圈與戰鬥／章節系統之間
的信號：`fd2_check_battle_end_condition` 預設判「敵全滅」寫 2、「主角索爾死」寫 1、
其餘 0；各章 post-action handler（`field.md`）會依劇情覆寫勝敗（見「勝負判定覆寫」
的 win-overrides-loss 機制）。值 1 是落敗（回主選單），值 2 是過關（跑章結束 handler
後轉場到下一章）。

關鍵：FD2 沒有獨立的 `battle_loop()` function。戰鬥就是 `main` 的內迴圈反覆呼叫
`fd2_game_main_loop`，每 frame 處理一個輸入或繼續動畫。chapter init 把地圖、敵人配置好
之後，`fd2_game_main_loop` 自己跑，直到 `data_fd2_chapter_event_or_battle_end_code`
變成 1 或 2。

### 亂數暖機

`main` 啟動尾段跑一段 RNG 暖機：以 `rand() % 0x100` 決定迭代次數，重複呼叫
`fd2_advance_rng_state()` 打散初始亂數序列。同一段也把 BIOS tick 計數（word @ 0x46C）
latch 進 `data_fd2_graphics_chapter_ambient_palette_anim_tick_latch`，用來給章節環境
palette 動畫定速。RNG 演算法本身與 seeding 細節見 `battle.md`。

## 遊戲子系統概觀

以 src 模組為單位，各子系統的主要 function 與進入點對照（細節見對應 KB 檔）：

| 子系統（KB 檔）| 主要 function | 進入點 |
|---|---|---|
| lifecycle (`overview.md`) | `main`、`_cstart_`、AIL_startup/shutdown、`fd2_load_save_and_init_engine` | `_cstart_` @ 0x3C964 |
| `rsrc.md` | `fd2_load_dat_resource` | 各子系統自行呼叫 |
| `save.md` | FD2.SAV 存取 helper、`fd2_field_menu_status_save_load_quit_dispatch` | `fd2_field_command_menu_loop` |
| `field.md` | 各章 init/end handler、`fd2_chapter_transition_menu`、FDFIELD event 派遣 | jump table |
| `battle.md` | `fd2_attack_action_dispatch`（AI 三路 score）、12-class 敵 AI dispatcher | `fd2_enemy_turn_phase_team0` |
| `spell.md` | 法術 dispatch / cinematic / effect 三層 | `fd2_attack_action_dispatch` |
| `pathfind.md` | `fd2_init_movement_range_floodfill`、`fd2_pathfind_to_destination` | 選單／AI 走位 |
| `ui_menu.md` | `fd2_game_main_loop`、cursor 移動、`fd2_player_action_menu_loop`、`fd2_field_command_menu_loop` | scancode dispatch |
| `town_menu.md` | 商店 / 轉職 / status / chintro 選單樹 | 章節交接 |
| `dialog.md` | `fd2_display_dialog_scene`、`fd2_blit_glyph_1bpp_with_outline`、portrait cache | `fd2_display_dialog_scene` |
| `anim.md` | `fd2_play_figani_animation_loop`、slide 動畫 | spell / cinematic 執行 |
| `gfx.md` | `fd2_rle_blit_sprite`、`fd2_blit_rectangle`、`fd2_composite_battle_tile_map` | `fd2_composite_battle_frame` |
| `audio.md` | AIL wrapper + `fd2_play_sfx_with_handle` | per-event |
| `input.md` | `fd2_wait_for_input_with_idle`、BIOS 鍵盤直讀 | `fd2_game_main_loop` |
| `table.md` | `fd2_get_*_entry` accessor 群 | 各子系統查表 |
| `util.md` | `fd2_delay_ms`、DPMI INT 31h wrapper、AIL alloc/free 指標、party-roster helper | 各子系統呼叫 |

另有跨系統的章節事件派遣機制（FDFIELD 章節 event hook → jump table → 編譯好的
cinematic C 函式），同一張表也被 AI post-action consequence 共用，詳 `field.md`。

函式的四 pool 分類（`ail` / `crt` / `fd2` / `binary_artifact`）、binary_artifact pool
的 alignment NOP 詳見 `rebuild_info/equivalence/pool_classification.md`；Watcom CRT 真符號
inventory 見 `rebuild_info/crt/lookup_9.5a.json` 與 `matched_function_sources.md`，
`crt_equivalent_*` / `fd2_*` CRT-style primitive 見 `rebuild_info/crt/symbol_inventory.md`。

## 共享資料結構

### `runtime_char` struct (80 bytes = 0x50)

每個玩家／敵人／NPC 的執行時狀態。指標 `data_fd2_battle_runtime_char_array_ptr`
（@ 0x00053A45）指向 malloc 出來的 runtime_char 陣列；元素個數在
`data_fd2_battle_party_member_count`（@ 0x00053BEB）。

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
                      [0x14]   bMove_range 移動力 (= runtime_char +0x3B)
                               player = character_base[7]；enemy = enemy_data[8]（皆原值，不乘等級）；
                               升職時加 promotion 表 byte[1]（顯示 all_game_text page 0x254
                               「移動力增加N點」）。所有讀取端都當「移動距離預算」餵給
                               fd2_init_movement_range_floodfill / fd2_pathfind_to_destination（見 pathfind.md）。
+0x3C bExp_carry      EX 經驗餘額（升級用的 XP carry，每滿 100 進位一級；
                      init: player = 0、NPC/enemy = 0xFF）。
                      注意：「本回合已行動」旗標不在此，而是 bFlags(+0x05) bit7。
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

`runtime_char` 詳細欄位語意與如何被各個 scorer / 動畫 / damage 函式使用，見
`battle.md`。

### 遊戲資料表 schema 索引

`.object3` 內建 7 張以上的唯讀資料表（位址見「模組與記憶體佈局」）；欄位 schema 一律
以 `assets/tables/` 為正典：

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

資料表模組級索引（哪張表在哪個 src 檔、誰在用、accessor 清單）見 `table.md`。

## 關鍵遊戲機制

1. **Enemy AI 12 種 behavior class** (`pCombat_aux_block[0xD] & 0x0F`) — 每個
   敵人的「個性」，從 class 0「只會打」到 class 11「智者 spell-then-physical」
   到 class 8「完全不動」。詳 `battle.md`。
2. **AI kill-shot 加權**：物理 score = 0x12 (18)、道具 score = 0x12、**法術
   score = 0x18 (24)**。敵法師會優先用魔法秒殺玩家殘血。詳 `battle.md`。
3. **20% HP_max idle heal** (`fd2_ai_pass_turn_with_heal`)：AI 待機自動恢復 20%
   最大 HP。FD2 重型敵人「拖不死」的程式根源。詳 `battle.md`。
4. **Two-pass enemy phase**：smart caster 第一輪先動（佔 AoE 位置與秒殺），
   melee 第二輪。AI 戰術設計，不是 bug。詳 `battle.md`。
5. **Mirror dialog blit** (`fd2_dialog_sprite_blit_mirrored`)：友軍對話用右→左
   pixel order blit，產生「兩人面對面」視覺效果。詳 `dialog.md`。
6. **20-bit pitch state** (AIL mixer)：`ail_mix_pitch_int/_low/_int_plus_one`
   是 16.16 fixed-point pitch increment；雙倍 stereo / 16-bit 時 left-shift 一次。
   詳 `audio.md`。
7. **Save data obfuscation** (`fd2_save_crypt_buffer`)：XOR-style involution
   (同一 function 加密與解密)；`fd2_save_compute_checksum` 是 4-byte 快速 checksum
   (非 CRC32，作為完整性檢查 / cheat deterrent，不是 cryptographic 安全)。詳 `save.md`。
```
