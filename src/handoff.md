# FD2 Emit Pipeline — Handoff 文件

## 必讀文件（依順序）

1. **`~/.claude/plans/wiggly-skipping-ripple.md`** — 整體 emit pipeline 計畫（目錄結構 + per-function workflow + build pipeline + 驗證標準）
2. `CLAUDE.md` + `index.md` — 專案規範與知識庫結構
3. `~/.claude/projects/C--Users-fdpsf-Documents-fd2-anatomy/memory/MEMORY.md` — 全部 memory entries，特別是：
   - `feedback_asm_review_every_fn.md` — 每個 fn 必須 asm review
   - `feedback_strict_one_at_a_time.md` — 一次只處理一個 item
   - `project_src_83_filenames.md` — 8.3 檔名 + C89 限制
   - `feedback_no_workaround_root_cause_only.md` — 禁止 workaround
4. `src/handoff.md` — 本文件（進度 + 已知問題 + 嚴格規則）
5. **`src/routing.json`** — 653 函式唯一 source of truth（address → {name, target, phase, done, asm}）
6. `src/routing.md` — folder/file 類別描述 + decision notes（human reference）
7. `rebuild_info/emission/_index.md` — emit pipeline 規格總覽（Layer 2 目標）
8. `rebuild_info/emission/pipeline_spec.md` — fall-through pattern、data emit 規則
9. `rebuild_info/emission/calling_convention.md` — Watcom cc ABI 判斷規則
10. `rebuild_info/emission/pool_routing.md` — 四 pool 分類與 emit_action 路由
11. `rebuild_info/link/wlink_settings.md` — wlink 連結設定
12. `rebuild_info/ail/calling_convention.md` — AIL EBX-clobber convention
13. `program_info/overview.md` — 12 系統架構、runtime_char struct、.object3 tables
14. **`tests/_index.md`** — test 架構說明：如何執行、如何新增 test case、檔案結構

## 工作進度

| Phase   | 內容                                          | 狀態 | 函式   | tests   |
| ------- | --------------------------------------------- | ---- | ------ | ------- |
| Phase 0 | 基礎設施                                      | ✅   | —     | compile + link verified |
| Phase 1 | Table / Input / Battle Core / Spell / Palette | ✅   | 89/89  | 74 PASS |
| Phase 2 | Battle AI / Animation Tick / Lifecycle        | 🔶   | 97/107 | 177 PASS |
| Phase 3 | Graphics Blit + Render + CRT                  | ⬜   | 0/89   | —      |
| Phase 4 | Animation Play / Dialog / Spell Workers       | ⬜   | 0/92   | —      |
| Phase 5 | Menu / Save-Load / Misc Util                  | ⬜   | 1/108  | —      |
| Phase 6 | Chapter Init / End / Post-Action              | ⬜   | 0/78   | —      |
| Phase 7 | Chapter Event Handlers                        | ⬜   | 0/90   | —      |
| Phase 8 | Data Emit (global tables / strings / BSS)     | ⬜   | —     | —      |
| Phase 9 | Integration + Verify                          | ⬜   | —     | —      |

653 函式清單 + routing + 進度追蹤統一在 `src/routing.json`。管理工具：`python tools/emit/mkroute.py <status|pending|mark|validate|generate>`。
Pipeline: `tools/emit/dump_emit_functions.java` (Ghidra) → `workspace/emit/emit_functions.json` → `tools/emit/mkroute.py generate` → `src/routing.json`。

### Phase 1 完成成果

**Source files 建立/擴充：**

| 檔案 | 函式數 | 說明 |
|---|---|---|
| `src/table/table.c` | 12 | Table accessor functions |
| `src/battle/battle.c` | 17 | RNG、damage、heal、XP、stat calc、tile attr、combat outcome |
| `src/spell/spell.c` | 25 | Spell handler thunks (dispatch table callees) |
| `src/spell/spellwk.c` | 5 | Complex spell workers + use-effect dispatcher |
| `src/input/input.c` | 17 | Keyboard polling、BIOS tick、input wait loops、scancode remap |
| `src/gfx/palette.c` | 7 | VGA DAC palette manipulation、palette cycle animation |
| `src/ui_menu/cursor.c` | 5 | Cursor movement + pan |
| `src/ui_menu/status.c` | 1 | Item stat preview calculator |

**Headers：**
- `src/include/types.h` — runtime_char(80B)、item_effect(23B)、spell_effect(7B)、enemy_data(10B)、character_base(24B)、character_growth(11B)
- `src/include/consts.h` — BIOS addresses、item/spell/job counts、struct field constants
- `src/include/globals.h` — 所有 extern global 宣告（~70 個）
- `src/include/protos.h` — 所有函式原型（emitted + forward decl）

**Test 覆蓋：**
- 74 test cases across 4 suites (testtbl / testbtl / testui / testspel)
- 未測試的函式主要為：4 個 interactive repaint wait loops（需完整 display pipeline）、24 個 spell handler thunks（thin wrappers 呼叫 stubbed inner functions）
- 這些函式的語意驗證需要 Phase 8 的 integration test（DOSBox-X scripted playtest）

**Ghidra 修正：**
- 7 個 global label 前綴修正（`battle_tile_map` → `data_fd2_battle_tile_map_ptr` 等）
- 多處 plate comment 修正（mode 6 語意、palette cycle table size、EAX tracking bug 紀錄）
- 2 個 DECOMPILER FRAGMENT / co-dead chain 確認跳過

### Phase 2 進行中

**已完成 97/107：**
- 8 個 DECOMPILER FRAGMENT / shared epilogue 跳過（`fd2_noop_stub_*` × 7 + `fd2_set_runtime_char_evade`）
- `fd2_check_char_status_immunity` / `fd2_face_char_toward_target` / `fd2_debug_print_ans_and_length` — battle core helpers
- `fd2_set_word_global_52758` / `_5275c` — AIL alloc/free fnptr swap
- 6 個 `fd2_dpmi_*` — DPMI INT 31h memory management wrappers
- `fd2_tick_tile_event_animations` / `fd2_tick_chapter_palette_animation` — 動畫 tick
- `fd2_find_char_at_cursor_pos` / `fd2_find_char_by_id_or_template` — char 查詢
- `fd2_walk_step_down/left/up/right` + `fd2_walk_path_animation_loop` — 4 方向 walk + path dispatcher
- `fd2_mark_char_acted_this_turn` / `fd2_check_all_player_acted_or_asleep` — turn state
- `fd2_check_tile_event_post_action` — 步入 tile 事件觸發
- `fd2_mark_char_as_dead` / `fd2_set_chapter_init_done_flag` / `fd2_set_battle_anim_phase_to_1` / `fd2_set_combat_aux_block_byte_d_low4_for_char_range` — state setters
- `fd2_check_battle_end_default_handler` / `fd2_check_battle_end_condition` — 勝敗判定
- `fd2_collect_dead_char_drops` / `fd2_collect_pending_death_drops` — 死亡掉落收集
- `fd2_ani_decoder_set_target_buffer` — ANI decoder buffer 設定
- `fd2_tally_chars_with_zero_at_field` / `fd2_find_tile_with_attribute_match` — AI utility
- `fd2_mark_aoe_plus_pattern_at` / `fd2_set_tile_overlay_bit_80` — AoE tile overlay
- `fd2_mark_char_occupant_tiles_for_team` / `fd2_collect_unmarked_tile_positions` — tile 佔位
- `fd2_scan_chars_within_manhattan_range` / `fd2_scan_chars_along_line_with_team_filter` — 範圍/直線掃描
- `fd2_tick_tutorial_progress_with_sfx` — 步行 SFX + 計數器
- `fd2_pathfind_count_unique_directions` — pathfind tiebreak helper
- 7 個 `fd2_slide_panel_*` — 面板 slide-in 動畫
- `fd2_npc_turn_phase_team1` / `fd2_enemy_turn_phase_team0` — NPC/enemy AI turn loops
- `fd2_score_item_candidate` — AI item 評分
- `fd2_tick_sprite_animation_step` — sprite 動畫 frame pacer
- `fd2_enemy_turn_action_dispatcher` — master AI turn handler, 12 behavior classes
- `fd2_ai_advance_to_nearest_team_target` — find nearest team char and walk toward
- `fd2_ai_pass_turn_with_heal` — pass turn with 20% HP regen if eligible
- `fd2_ai_seek_optimal_position` — pathfind-based AI movement to best cell
- `fd2_ai_score_physical_attack` — score physical attack across reachable tiles
- `fd2_compute_aoe_targets` — AoE target collection with tile overlay
- `fd2_ai_walk_to_target_tile` — two-stage pathfind + walk toward target
- `fd2_attack_action_dispatch` — score phys/spell/item + execute best
- `fd2_execute_ai_item_use` — AI item use execution with animation
- `fd2_execute_ai_offensive_spell` — AI offensive spell cast + drop handling
- `fd2_execute_ai_physical_attack` — AI physical attack with retaliation
- `fd2_ai_score_item_use` — score inventory items across reachable tiles
- `fd2_score_spell_candidate` — per-candidate spell scorer with kill/heal/status dispatch
- `fd2_tick_status_effects_and_show_messages` — end-of-turn poison + timer countdown
- `fd2_init_battle_state_for_chapter` — chapter battle state initialization
- `fd2_set_bgm_track_with_fade` — BGM track manager with AIL fade
- `fd2_tick_summon_spell_minor_animation_state` — summon minor anim #9
- `fd2_main_menu_continue_dispatcher` — NEW/CONTINUE/fallback dispatcher
- `fd2_tick_summon_anim_variant_d_3slot` — summon variant D, 3-slot #7
- `fd2_tick_summon_anim_variant_e_16slot` — summon variant E, 16-slot #8
- `fd2_tick_summon_anim_variant_a_6slot` — summon variant A, 6-slot w/ RNG jitter #4
- `fd2_tick_summon_anim_variant_b_6slot` — summon variant B, 6-slot 2-bucket SFX #5
- `fd2_tick_summon_spell_animation_state` — generic single-target summon anim #2（plate bug 修正）
- `fd2_tick_summon_spell_main_animation_state` — 12-slot main summon anim #3
- `fd2_tick_summon_spell_setup_pre_animation_8slot` — 8-slot pre-anim orbit #0
- `fd2_ani_decoder_decode_frame_bytes` + 10 chunk handlers — 完整 ANI decoder（static cursor 替代 ESI register protocol）
- `fd2_ai_score_offensive_spell` — score castable spells across reachable tiles

**Source files（18 個 .c + 4 個 .h）：**

| 檔案 | 函式數 | 說明 |
|---|---|---|
| `src/table/table.c` | 12 | Table accessor functions |
| `src/battle/battle.c` | 17 | RNG、damage、heal、XP、stat calc、tile attr |
| `src/battle/btl_turn.c` | 15 | Char lookup、turn state、battle end、drops、status tick、battle init |
| `src/battle/btl_ai.c` | 24 | AI 全體系 + spell candidate scorer |
| `src/spell/spell.c` | 25 | Spell handler thunks |
| `src/spell/spellwk.c` | 5 | Complex spell workers |
| `src/input/input.c` | 17 | Keyboard、BIOS tick、input wait loops |
| `src/gfx/palette.c` | 8 | VGA palette + chapter palette tick |
| `src/anim/anim.c` | 22 | Walk + slide panel + summon spell variants (a/b/d/e/minor/generic/main/8slot) |
| `src/anim/anidec.c` | 12 | ANI decoder: set_target + dispatcher + 10 chunk handlers (static cursor 替代 ESI) |
| `src/audio/audio.c` | 1 | BGM track manager with fade |
| `src/life/main.c` | 1 | Main menu continue dispatcher (fd2_main 未 emit) |
| `src/ui_menu/cursor.c` | 5 | Cursor movement + pan |
| `src/ui_menu/status.c` | 1 | Item stat preview |
| `src/util/misc.c` | 3 | Debug print、AIL fnptr swap |
| `src/util/dpmi.c` | 6 | DPMI INT 31h wrappers |
| `src/util/pathfnd.c` | 1 | Pathfind tiebreak helper (8 個 pathfind 函式未 emit) |

**Headers：**
- `src/include/types.h` — runtime_char(80B) + 5 struct 定義
- `src/include/consts.h` — BIOS addresses、counts、struct offsets
- `src/include/globals.h` — ~130 extern global 宣告
- `src/include/protos.h` — ~200 函式原型（emitted + forward decl + stubs）

**Test 覆蓋：**
- 177 test cases across 5 suites (testtbl / testbtl / testui / testspel / testanim)
- DOSBox-X compile + link + run 全自動（`dosbox-x -silent -conf tests/dosbox.conf`）
- 程式碼增長後 DPMI 記憶體不足問題需用二段式（compile session + debug session）解決

**剩餘 10 個 Phase 2 函式：**
- `life/main.c` (1) — `fd2_main`：遊戲 main()，依賴 AIL init/shutdown + BIOS 呼叫 + 完整 game loop
- `anim/anim.c` (1) — `fd2_tick_summon_anim_variant_c_5slot_radial`：FPU sin/cos radial positioning
- `util/pathfnd.c` (8) — floodfill + recursive pathfind，全部 register protocol (DL/DH/CL/EBX/EDI)，需整組 emit + tile map test infrastructure

**Phase 2 剩餘 10 函式分類：**

1. **fd2_main** (1) — 遊戲 main()，約 300 行，依賴 AIL init/shutdown + 8 個 load_dat_resource + int86 BIOS 呼叫 + 完整 game loop。建議最後 emit
2. **variant_c sin/cos** (1) — 0x26E39，需要 FPU sin/cos radial positioning，較複雜
3. **Pathfind functions** (8) — 全部用 register protocol。已完成 assembly 分析：
   - **Register mapping**: DL=x, DH=y, CL=remaining_steps, EBX=tile_overlay_ptr, EDI=work_stack_ptr, ESI=caller_context(job cost table), EBP=row_stride(width*4)
   - **Flood fill group** (3 fns): init 設 globals → recursive 呼叫 4 方向 neighbor_step → step 做 double-indirection cost lookup (terrain_attr→cost_entry→job_cost)
   - **Pathfind group** (5 fns): init 設 globals + dest → recursive_with_direction 呼叫 4 方向 step_with_tiebreak → check_destination / record_destination
   - **C translation 策略**: 用 C function parameters (x, y, remaining, tile_ptr) 替代 registers；EDI work stack 用 C recursion 的 local vars 替代（flood fill）或寫入 step_stack global（pathfind，因 check_destination_save_path 需讀取）
   - **Tile map test**: 需建立 minimal 3×3 tile map（7-byte header + 9×4 byte tiles，overlay 在 tile_ptr+0, flags 在 tile_ptr-1, terrain attr 在 tile_ptr-3）
   - **Stub 替換**: 現有 AI tests 依賴 g_pathfind_return stub，替換後需設 proper tile map 或改用 tile-map-aware test setup
   - **已完成**: 所有 pathfind globals 加到 globals.h + testglob.c（驗證 177 tests 不受影響）

**已解決的架構決策：**
- ANI decoder register protocol → 已採用 file-scope static `g_ani_cursor` 替代 ESI，成功 emit 全部 11 handler + dispatcher
- Summon spell plate comment is_enemy 條件 → assembly 驗證確認 decompiler 正確，plate 有誤（已修正）

### Phase 8 — Data Emit（所有 function emit 完成後執行）

全部 653 個 function emit 完畢後，開始處理 ~1300 個 global data items。依 `pipeline_spec.md` Data emit 路由分三類：

1. **`emit_c_const`（fd2 const）** — .object2/.object3 中有實際 byte content 的資料表和字串：
   - 資料表：`item_effect_table[215]`, `spell_effect_table[36]`, `enemy_data_table[68]`, `character_base_table[32]`, `character_growth_table[68]`, dispatch handler pointer tables, animation/spell lookup tables, tile attribute modifier tables, job resist/crit tables, chapter intro metadata, spell learning table, movement cost table, footstep cadence table 等
   - 字串常數：8 個 `.DAT` 檔名, OOM 訊息, debug format strings
   - 作法：用 Ghidra MCP `read_memory` 讀取真實 byte 值，emit 為 C initialized array
2. **`emit_c_const_zero`（fd2 BSS-style state）** — 啟動時為 0 的 game state globals：
   - cursor 座標, viewport origin, battle state flags, audio handles, portrait cache, timing state 等
   - 作法：emit 為 `uint32 data_fd2_xxx;`（不加 `= 0`，C 標準保證 zero-init）
3. **`link_vendor`（crt/ail data）** — CRT 和 AIL 的內部 data，由 vendor lib 帶入：
   - 作法：只保留 `extern` 宣告，wlink 解析

工具需求：寫 Ghidra script 批量 dump 所有 data items 的位址/大小/byte content → 產生 .c source files。

### Compile + Test Gate 規則

**每處理完 1 個 function 就必須跑一次 compile + test gate：**

1. `dosbox-x -silent -conf tests/dosbox.conf` — 全部 src .c + test .c 編譯 0 errors 0 warnings
2. `tests/OUT/TEST.OUT` — 所有 test PASS，0 failed
3. 新 emit 的 function 必須有對應的 test case（加到 `tests/test*.c`）
4. 新 .c 檔已加入 `tests/dosbox.conf` 的 compile 區塊 + `tests/test.lnk` 的 file 列表
5. 新 stub / fake global 已加入 `tests/testglob.c`

gate 失敗時必須立即修正，不能繼續 emit 下一個 function。phase 結束時也必須通過 gate。

## 過程中遇到的問題和解法

### 1. Watcom 9.5a 不支援長檔名 (LFN)

- **問題**: `phase0_verify.c` → "Unable to open"
- **解法**: 所有 source / header / 目錄名必須 ≤ 8.3 DOS 格式
- **範式**: headers 不加 `fd2` 前綴（`types.h` 非 `fd2type.h`），目錄用縮寫（`anim` 非 `animation`）

### 2. Watcom 9.5a 是 C89 compiler

- **問題**: `uint8 *result = fn();` 在 statement 後面 → compile error
- **解法**: 所有變數宣告必須在 block 開頭，statement 之前

### 3. DOS 命令列長度限制 (127 chars)

- **問題**: wlink 命令列超長 → "directive error near 'f'"
- **解法**: 使用 .lnk directive file (`wlink @test.lnk`)

### 4. Ghidra decompiler 系統性 EAX tracking bug

- **問題**: `fd2_advance_rng_state()` 呼叫後，decompiler 把 EAX return value 誤標為 call 前的其他變數
- **影響**: Phase 1 中至少 10+ 處確認此 bug（battle core、palette cycle、dialog blink 等）
- **解法**: **每個函式必須取 assembly 交叉驗證**，特別是含 `fd2_advance_rng_state()` 或任何 CALL 後使用 EAX 的 pattern
- **嚴格規則**: 一次只處理一個 function，逐一 decompile + disassemble + 對照驗證

### 5. Ghidra decompiler void return 誤標

- **問題**: `fd2_advance_rng_state` 被標為 void 但 callers 使用 EAX return value
- **解法**: 從 callers 的 assembly 確認隱含的 return value，正確宣告 return type

### 6. Test globals 衝突（已解決）

- **問題**: 多個 test file 定義相同 extern global → linker redefinition error
- **解法**: 所有 fake globals 和 stubs 集中到 `tests/testglob.c`，各 test*.c 只含 test functions。詳見 `tests/_index.md`

### 7. Forward declaration type mismatch

- **問題**: `fd2_pan_cursor_to_tile_animated` 在不同 section 有 `int` vs `uint32` 矛盾宣告
- **解法**: 以 assembly 確認原始簽章，統一所有宣告

### 8. DOSBox-X 編譯/執行的完成偵測與成敗判定（唯一正確方式）

- DOSBox-X 的 host filesystem mount **不會** cache 來源檔——不存在 stale-cache 問題。
- compile + link + run 需要時間（全量數分鐘）。**唯一正確的完成訊號是 `tests/OUT/DONE.TXT` 出現**：啟動 `dosbox-x -silent -conf tests/dosbox.conf` 後輪詢 `DONE.TXT` 直到出現（timeout 給足，如 300s），期間**不可讀 output、不可判定成敗**。
- 成敗一律解析 `BUILD.OUT`（`Error!` → 編譯/連結失敗；`Warning!` → 警告）與 `TEST.OUT`（末尾 `Results: N passed, M failed`）。
- **禁用** copy→delete→rename / `Start-Sleep` / 兩段式 session 等 workaround——它們是對「提早讀取」症狀的誤判補丁。
- 此 gate 由 `tools/emit/build_test.py` 實作（clean → launch → poll DONE.TXT → parse → JSON）。

### 9. Palette cycle animation table 是 sliding window

- **問題**: plate comment 描述 "16 frames × 16 colors × 3 bytes = 768 bytes"，實際只有 93 bytes
- **真相**: 每幀偏移 3 bytes（16 frame × 3B stride），每幀讀取 48 bytes（16 color × 3B RGB），形成 sliding window
- **解法**: 修正 plate comment + 全域變數宣告為 `byte[93]`

### 10. DOSBox-X 單一 session compile+link+run 正常（無 DPMI OOM）

- 同一 DOSBox-X session 內 compile + link + run **不會** DPMI OOM。
- 先前觀察到的「TEST.OUT 空白 / DONE.TXT 未現」= 批次**尚未跑完**，繼續輪詢即可（見 §8），不是 OOM，也不需 `tests/dosbox_dbg.conf` 兩段式。

### 11. Stub → real function 轉換時的 linker 衝突

- **問題**: 先 emit 的函式用 stub（如 `fd2_wait_for_input_v2`），後來 emit 真實版本時 linker 報 redefinition
- **解法**: emit 真實函式後必須同步移除 testglob.c 中對應的 stub，並更新 test 改用 BIOS keyboard buffer 預載取代 stub sequence

### 12. 未初始化 function pointer table 導致 TEST.EXE crash

- **問題**: `data_fd2_chapter_post_action_handler_table[30]` 在 testglob.c 中宣告為未初始化的 function pointer 陣列（C zero-init = 全部 NULL），當 emit 的函式呼叫 `handler_table[chapter_id](0)` 時觸發 NULL function pointer call → DOS4GW crash（"Packed file is corrupt" + "RET from illegal descriptor type 12"）
- **症狀**: BUILD.OUT 顯示 compile + link 全部成功，但 TEST.EXE 執行時立即 crash，TEST.OUT 為 0 bytes，DONE.TXT 不產出
- **解法**: testglob.c 中所有 function pointer table 必須初始化指向 no-op handler：
  ```c
  static void g_noop_handler(uint32 x) { (void)x; }
  void (*table[30])(uint32) = { g_noop_handler, g_noop_handler, ... };
  ```
- **教訓**: 新 emit 的函式如果呼叫 function pointer table（dispatch tables），必須確認 testglob.c 裡的 table 有初始化。未來 emit 前要檢查所有被參照的 dispatch table

### 13. （已併入 §8）編譯/執行等待

- 完成偵測與輪詢規則統一見 §8。

## 本 session 犯的錯誤（絕不再犯）

1. **pathfnd.c `_impl` 死碼** — 為了避免 linker 衝突把函式改名為 static `_impl`，這是 workaround。正確做法：要嘛完整 emit + 建好 test infrastructure，要嘛就不寫。emit 出來的 function 絕不能是半成品。
2. **ANI decoder 11 handler 批次處理** — 只對 1 個做了完整三源驗證就直接 batch emit 其餘 10 個。嚴重違反「逐一處理」和「三源驗證不得省略」。
3. **5 個 summon variant 跳過 disasm** — variant_d/e/b/main/8slot 只讀 plate+decomp 就 emit，沒有用 assembly 逐行驗證。
4. **emit_issues.json 未更新** — fd2_score_spell_candidate 的 FPU rounding 問題沒寫入。
5. **為了遷就 test code 而影響 emit code** — pathfnd.c 的 `_impl` 就是這個錯誤的典型。test infrastructure 是服務 emit code 的，不能反過來讓 emit code 去遷就 test 的限制。

**保證：**
- emit 出來的 function 絕對不能是半成品（改名、static 化、incomplete 實作）
- 嚴格禁止為了遷就 test code 而影響真正重要的 code emit
- 三源驗證每一步都不省略，即使函式「看起來很簡單」
- 逐一處理不打折扣，不拿「結構相同」當批次處理的藉口

## 嚴格規則

1. **語意保留**: 修改 decompiled C code 時必須完全維持原本的語意和邏輯不變
2. **三源交叉驗證**: 每個 function 必須嚴格按以下步驟執行，不得省略任何一步：
   **Step A** — `get_plate_comment(address)` 讀取 plate comment
   **Step B** — `disassemble_function(address)` 讀取 assembly
   **Step C** — `decompile_function(address)` 讀取 decompiled source
   **Step D** — 讀取 emitted C code（對應 .c 檔中該 function 的實作）
   **Step E** — 綜合交叉比對以下項目：
   - assembly 控制流 / 運算是否與 emitted C 邏輯一致（特別注意 Ghidra EAX tracking bug）
   - plate comment 描述的行為/caller/callee 是否與 assembly 和 emitted C 一致
   - decompiled source 是否有 bug（如 EAX tracking bug），emitted C 是否已修正
   - routing target 是否與函式實際語意相符
     **Step F** — 發現任何不一致立即修正：
   - emitted C 邏輯錯誤 → 修正 .c 檔
   - plate comment 錯誤 → `set_plate_comment` 修正
   - routing 錯誤 → 修正 routing.json + 搬移 code
   - KB 描述不符 → 修正 KB
   - 無法當下確認的問題 → 記入 emit_issues.json
     **嚴禁**：跳過 Step A/B/C 任何一步、合併多個 function 的 MCP call、用「結構相同」為由省略讀取
3. **逐一處理**: 一次只處理一個 function，逐一完整檢查，嚴禁抽驗，嚴禁批次
4. **進度顯示**: 處理每個 function 時顯示「n/N」（當前第幾個 / 此 phase 的 function 總數）
5. **KB 同步**: 發現真實狀況與 KB 描述不符合時立刻更新 KB
6. **Routing 查詢與修正**: emit 每個 function 前先查 `src/routing.json`（JSON key = address）確認目標 .c 檔；發現 routing 錯誤時立即修正 routing.json 的 `target` 欄位，並將已寫入錯誤 .c 檔的 code 搬移到正確檔案
7. **Symbol name 必須與 Ghidra 完全一致**: emitted C 中所有 function name、global variable name、label name 必須與 Ghidra 內的名稱 byte-identical。不得自行縮寫、省略或改寫。發現不一致時立即用 `list_globals` / `search_functions` 查 Ghidra 真名並修正 .c / .h 檔
8. **命名前綴不合理立即修正**: 發現 Ghidra 內 global / function 的命名前綴不符合專案慣例（如 fd2 game-side global 缺少 `data_fd2_` 前綴），必須同時修正 Ghidra label（`rename_data` / `rename_label`）和 emitted C code / globals.h / testglob.c，確保三者保持一致
9. **待驗證問題追蹤**: 三源驗證中發現需要後續編譯才能確認或無法當下確定的問題（FPU rounding、word width 差異、結構性差異等），記錄到 `src/emit_issues.json`（key=address, value={name, issues}）。所有 emit phase 完成後統一用 Watcom 9.5a 編譯 + disasm 比對解決。**寫入 JSON 後必須 Read 回來檢查有無亂碼/encoding 錯誤，發現立即修正**
