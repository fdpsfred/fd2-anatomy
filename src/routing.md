# FD2 Emit Routing Table

653 個 fd2/crt function 到 source file 的完整路由。每個 .c 編譯為一個 .obj。

## Source of Truth

**`src/routing.json`** 是 function → target file 的唯一 source of truth（653 entries，key = address）。
由 `tools/emit/mkroute.py generate` 從 `emit_functions.json` + 內建 routing rules 生成。

查詢方式：`python -c "import json; d=json.load(open('src/routing.json')); print(d['<address>'])"`

## Routing 修正規則

**emit 過程中發現某 function 的 routing 不正確時，必須立即：**
1. 修正 `src/routing.json` 中該 address 的 `target` 欄位
2. 重新執行 `python tools/emit/mkroute.py` 驗證計數
3. 若已將 code 寫入錯誤的 .c 檔，立刻搬移到正確的檔案
4. 更新本文件 §一 的 fn 計數和 Decision Notes（如有新決策）

## 一、Folder 與 Source File 分類定義

### `input/` — 鍵盤輸入與等待

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `input.c` | BIOS keyboard buffer check/clear, BIOS tick read/wait, INT 16h key read, 所有 `fd2_wait_*` 複合等待迴圈（含 idle frame repaint、dialog blink、palette cycle、status panel repaint）, `fd2_wrapper_clear_keyboard_buffer` | 15 |

### `battle/` — 戰鬥系統

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `battle.c` | 傷害/治癒 pipeline (`apply_damage`, `apply_hp/mp_heal`, `calc_magic_damage`), 戰鬥數值計算 (`recalculate_combat_stats`, `execute_attack_damage_calculation`, `calculate_combat_hit_outcome`), 反擊/攻擊判定 (`check_can_counter_attack`, `check_can_default_attack_target`), tile attribute accessor (`read_tile_attribute_at_pos`), inventory slot accessor (`get_inventory_slot_item_id`), 戰鬥 utility (`face_char_toward_target`, `check_char_status_immunity`, `check_char_is_dead`, `flash_char_hit_sprite`, `compute_combat_bubble_screen_pos`, `find_equipped_item_by_kind`), RNG (`advance_rng_state`) | 22 |
| `btl_ai.c` | 敵方/NPC 回合派遣與 AI 行動執行 (`enemy_turn_action_dispatcher`, `enemy_turn_phase_team0`, `npc_turn_phase_team1`, `attack_action_dispatch`, `execute_ai_item_use/offensive_spell/physical_attack`) | 7 |
| `btl_aitg.c` | AI 目標/地形掃描、AoE tile 標記、移動規劃 (`ai_advance_*`, `ai_seek_*`, `ai_pass_turn_*`, `ai_walk_to_target_tile`, `mark_aoe_*`, `mark_char_occupant_*`, `set_tile_overlay_bit_80`, `scan_chars_*`, `compute_aoe_targets`, `collect_unmarked_*`, `tally_chars_*`, `find_tile_*`, `resolve_terrain_*`) | 14 |
| `btl_aisc.c` | AI 行動評分 (`ai_score_physical_attack`, `ai_score_item_use`, `score_item_candidate`, `ai_score_offensive_spell`, `score_spell_candidate`) | 5 |
| `btl_turn.c` | 回合迴圈 (`run_full_turn_cycle`, `fire_chapter_turn_events_for_phase`), 回合狀態管理 (`mark_char_acted_this_turn`, `check_all_player_acted_or_asleep`, `mark_char_as_dead`, `kill_runtime_chars_*`, `set_combat_aux_block_*`), 戰鬥結束判定 (`check_battle_end_*`), XP/升級 (`process_xp_and_level_up_for_char`, `roll_stat_gain_and_show_message`), 戰利品 (`collect_*_drops`, `process_battle_drop_entries`), 角色查找 (`find_char_at_cursor_pos`, `find_char_by_id_or_template`), 狀態 tick (`tick_status_effects_and_show_messages`), 計數 (`count_active_chars_*`), tile event (`check_tile_event_post_action`) | 19 |
| `btl_init.c` | 戰鬥/角色初始化與清理 (`init_runtime_char_for_battle`, `init_runtime_char_from_base_growth`, `init_battle_state_for_chapter`, `clear_all_chars_facing/acted_flag`, `convert_battle_tiles_to_24px`, `restore_all_chars_full_hp_mp`, `set_chapter_init_done_flag`, `set_battle_anim_phase_to_1`) | 9 |

### `table/` — 資料表存取

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `table.c` | 所有 `fd2_get_*_entry` table accessor (item/spell/enemy/char_base/char_growth/chapter_intro/spell_learning/class_promotion/attack_anim/job_allowed_items/movement_cost/cutscene_event) + orphan table accessor | 13 |

### `spell/` — 法術系統

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `spell.c` | 24 個 spell handler dispatch wrapper (`fd2_spell_handler_id_*`, `fd2_cast_spell_*_dispatch_*`) — 從 spell dispatch table 呼叫，delegate 到 spelleff.c 的 worker | 24 |
| `spelleff.c` | 法術/道具效果套用: 道具使用派遣 (`apply_use_effect_dispatch`, `apply_item_stat_modifier_with_anim`, `apply_attack_spell_damage`, `apply_status_effect_with_anim`), 攻擊/治療/增益/狀態 targeted cast (`execute_offensive_*`, `cast_group_hp_heal_*`, `cast_ap/dp/speed_boost_*`, `cast_status_cure/inflict_*`, `execute_status_clear_*`, `cast_spell_17_complex`) | 17 |
| `spellsel.c` | 法術選擇 UI (`spell_selection_menu_main`, `spell_select_input_loop`, `draw_spell_selection_list`, `build_usable_spell_list`, `play_spell_palette_flash_with_sfx`, `grant_spell_to_char`) | 6 |
| `spellcin.c` | 動畫型/cinematic 法術 (`cast_earthquake_*`, `cast_screen_wide_*`, `execute_aoe_spell_with_caster_portrait_*`, `execute_special_attack_skill`, `execute_summon_spell_cast`, `animate_warp_*`, `scatter_sprite_*`, `play_rising_pre_cast_effect`, `play_variant_b_slide_pre_effect`, `dispatch_variant_b_cast`) | 13 |

### `ui_menu/` — 選單系統

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `cursor.c` | 4 方向 cursor 移動 (`cursor_move_up/down/left/right`), camera pan (`pan_cursor_to_tile_animated`, `pan_cursor_to_char`, `pan_cursor_and_window`) | 7 |
| `menu.c` | 主指令迴圈 (`game_main_loop`, `field_command_menu_loop`, `player_action_menu_loop`, `player_inline_action_menu_dispatch`) | 4 |
| `menucfg.c` | 遊戲設定/選項選單 (`game_options_menu_loop`, `open/close_settings_dialog_*`, `settings_menu_input_step`, `repaint_settings_dialog_borders`, `count_active_menu_items_until_zero`, `maybe_load/free_speed_mode_overlay`) | 8 |
| `menufld.c` | 田野互動派遣 (`handle_tile_event_interaction`, `field_menu_status_save_load_quit_dispatch`, `open_tactical_overview_zoom`) | 3 |
| `status.c` | 角色狀態畫面 (`open_char_status_screen`, `open/close_status_screen_*`), 隊伍總覽 (`open_party_status_overview_screen`), 裝備管理 (`equip_unequip_inventory_menu`, `equip_item_in_slot`, `check_job_can_equip_item`), 道具管理 (`inventory_selection_modal_dispatch`, `inventory_grid_input_step`, `item_command_menu_dispatch`, `add/remove/find_inventory_*`, `give_item_to_first_player_char`, `count_usable_inventory_slots`), 裝備預覽 (`compute_equipped_stats_with_item_preview`), 狀態選單 (`run_status_screen_member_menu`) | 17 |
| `shop.c` | 商店買賣 (`shop_menu_input_loop`, `open_shop_dialog_panel`, `run_buy/sell_item_menu`, `run_equip/give_item_menu`, `pick_stat_compare_color`) | 7 |
| `chintro.c` | 章節 intro 選單與隊伍編成選擇 (`chapter_intro_menu_input_loop`, `run_chapter_intro_menu_main/typeB/typeC`, `party_roster_single/class_select_loop`) | 6 |
| `promote.c` | 轉職/招募/復活 (`promote_members/member_select_loop`, `run_class_promotion_menu_main`, `execute_class_promotion_with_dialog`, `build_promotion_candidates_*`, `run_recruitment_or_branch_screen`, `build_dead_chars_list_for_revive`, `run_revive_menu_main`) | 8 |

### `gfx/` — 圖形繪製

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `blittile.c` | 24×24 tile blit 7 variant (`tile_blit_24x24_*`) + tile-層級 blit (`blit_animated_tile_at_pos`, `blit_24x24_at_window_relative_pos`, `blit_24x24_tile_to_battle_grid_position`, `blit_scaled_chapter_pose`, `blit_scaled_tile_map_view`) | 12 |
| `blitspr.c` | sprite/RLE/glyph/dialog/screen-block blit: indexed sprite (`blit_indexed_sprite*`, `blit_sheet_sprite_*`, `alloc_and_blit_indexed_sprite_chunk`), RLE (`rle_blit_*`), dialog sprite (`dialog_sprite_blit_*`, `decode_dialog_pixel_byte`), glyph (`blit_glyph_2bpp_with_outline`), raw/stride/scaled sprite (`blit_sprite_raw_*`, `blit_sprite_with_stride_*`, `blit_sprite_scaled_with_skip`), palette remap (`blit_palette_remap_with_sprite_mask`), screen block save/restore (`save/restore_screen_block_*`, `save/restore_block_loop`), scroll (`scroll_buffer_block_with_wrap`), 矩形/fill/money/per-row (`blit_rectangle`, `fill_screen_rect_with_byte`, `blit_money_digit_sprite`, `blit_buffer_with_per_row_offset`) | 26 |
| `rndscene.c` | 戰鬥/野戰場景合成 + 戰鬥畫面/特效視覺 (`composite_battle_*`, `composite_all_chars_overlay`, `composite_chars_with_spell_effect_overlay`, `composite_then_animate_projectiles`, `paint_char_sprite_at_world_*`, `paint_chars_shadow_overlay`, `paint_cursor_overlay_pattern`, `paint_threat_overlay_for_team`, `render_combat_*`, `render_combatant_hp_bar_*`, `render_phase_banner_frame`, `render_circle_anim_row`, `render_filled_circle_band_anim`, `render_summon_aura_sprite_ring`) | 18 |
| `rndstat.c` | 狀態/角色數值面板 (`render_status_screen_static_layout`, `render_full_char_stat_panel`, `render_inventory_item_grid`, `render_horizontal_bar_segments`, `render_hp_or_mp_bar_proportional`, `render_number_red_when_full`, `render_decimal_number_to_buffer`, `render_mini_char_status_panel`, `render_terrain_info_hud_panel`, `render_signed_modifier_with_icon`, `render_party_status_overview_content`, `render_chapter_status_panel_segments`, `paint_status_panel_layer_left/right`, `paint_portrait_to_dialog_area`) | 15 |
| `rndmenu.c` | 選單格狀渲染 (`render_shop_item_grid`, `render_party_roster_grid`, `render_party_roster_with_item_stat_preview`, `render_save_slot_grid`, `render_promote_members/candidates_grid`, `render_recruitment_select_screen`, `render_chapter_intro_overlay/dialog_panels`, `render_battle_scene_with_portrait_grid_layout`) | 10 |
| `palette.c` | VGA palette 直接操作 (`set_vga_palette_range`, `set_vga_palette_range_with_add`, `set_full_vga_palette_to_color`), fade in/out (`play_palette_fade_in`, `play_palette_fade_to_black`), over-bright pulse (`palette_overbright_settle_step_loop`), interpolation (`interpolate_palette_range_toward_color`), palette cycle (`update_palette_cycle_anim`, `tick_chapter_palette_animation`), palette remap (`apply_palette_remap_run`), blink pattern (`fill_palette_blink_pattern_6byte`) | 11 |

### `anim/` — 動畫系統

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `aniwalk.c` | 角色移動動畫 (`walk_step_down/left/up/right`, `walk_path_animation_loop`), UI 面板滑入/滑出 (`slide_panel_*` 7 fn, `play_status_screen_outro_step`), tile event 動畫 (`tick_tile_event_animations`) | 14 |
| `anicombt.c` | 戰鬥擊中/傷害/法術效果疊加動畫 (`animate_combat_hit_with_hp_drain`, `animate_attack_hit_sequence`, `animate_combat_speech_bubbles`, `show_damage_number`, `show_miss_indicator`, `animate_spell_impact_per_target`, `animate_spell_full_screen_flash`, `animate_spell_overlay_blink`, `animate_spell_projectile_paths`, `animate_status_effect_overlay_flicker`, `play_death_animation_and_mark_dead`, `animate_phase_banner_slide_in/out`) | 13 |
| `anisummn.c` | 召喚法術動畫 tick 狀態機 (`tick_summon_spell_*`, `tick_summon_anim_variant_*`, `tick_sprite_animation_step`) | 10 |
| `anicine.c` | 戰鬥/figani cinematics (`play_full_combat_cinematic`, `execute_combat_hit_cinematic`, `play_char_intro_zoom_anim`, `play_figani_char_intro_animation`, `play_figani_animation_loop`, `step_figani_pose_animation`, `animate_spell_hit_cinematic`, `display_cinematic_image_with_fade`) | 8 |
| `anispell.c` | 施法 cinematic 序列 (`play_spell_cast_cinematic`, `play_spell_cast_sequence`, `cycle_sprite_anim_with_bg_frames`, `animate_bg_zoom_transition_in/out`, `play_ani_file_animation_sequence`) | 6 |
| `aniend.c` | 結局/章節過場 (`play_ending_and_record_clear`, `play_game_ending_cinematic`, `play_final_chapter_30_ending`, `play_chapter_clear_fanfare`, `play_chapter_intro_sprite_slideshow`) | 5 |
| `aniui.c` | UI/商店/金錢/教學動畫 (`animate_money_increment/decrement`, `animate_shop_transaction_feedback`, `animate_scroll_up/down_in_shop_dialog`, `animate_party_addition_with_appear_effect`, `animate_tutorial_dialog_intro_or_outro`, `tick_tutorial_progress_with_sfx`, `animate_screen_shake`, `animate_palette_flash_pulse_white`, `cinematic_warp_char_to_tile`) | 11 |
| `anidec.c` | ANI.DAT frame decoder: palette chunk (`chunk_palette_fill_byte`, `chunk_palette_load_literal`, `chunk_palette_load_rle`, `chunk_palette_load_run_pairs`), row chunk (`chunk_row_fill_byte`, `chunk_row_copy_literal`, `chunk_row_decode_rle`), sparse chunk (`chunk_sparse_set_byte`, `chunk_sparse_set_run_byte`, `chunk_sparse_copy_literal`), target setup (`set_target_buffer`), frame dispatch (`decode_frame_bytes`) | 12 |

### `dialog/` — 對話系統

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `dialog.c` | 對話場景 (`display_dialog_scene`, `text_dialog_typewriter_loop`, `assemble_dialog_frame_layered`), portrait (`portrait_blink_animation_step`, `play_dialog_open_animation`, `show_portrait_dialog_with_input`), cinematic text (`cinematic_scroll_text_up_for_special_scenes`, `scroll_text_screen_up_by_lines`), dialog 管理 (`cleanup_dialog_sprite_buffer`, `close_dialog_panels_then_slide_in_at`, `close_intro_dialog_with_slide_out`, `backup/restore_dialog_area_to/from_buffer`) | 14 |

### `audio/` — 音訊系統

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `audio.c` | BGM 控制 (`set_bgm_track_with_fade`), SFX 播放 (`play_sfx_with_handle`, `play_sfx_sample_from_bank`), SFX 載入/釋放 (`load_status_effect_sfx`, `play_and_free_status_effect_sfx`, `load_figani_sfx_bank`) | 6 |

### `life/` — 遊戲生命週期

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `main.c` | 程式進入點 (`fd2_main`), 主選單 (`main_menu_continue_dispatcher`), 讀檔初始化 (`load_save_and_init_engine`) | 3 |

### `save/` — 存檔系統

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `save.c` | 存檔寫入 (`save_current_state_to_slot`, `save_runtime_char_to_template`), 讀檔 (`load_state_from_selected_slot`), 存檔 UI (`save_slot_selector_ui`), 加密/校驗 (`save_compute_checksum`, `save_crypt_buffer`), 戰場混淆 (`obfuscate_battle_tile_map`) | 7 |

### `rsrc/` — 資源載入

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `rsrc.c` | DAT 資源 (`load_dat_resource`), 章節資源 (`load_chapter_background_layers`, `load_chapter_battle_data`, `load_chapter_portraits_and_dump_tmp`, `load_chapter_portrait`, `load_chapter_party_roster`), portrait 快取 (`load_portrait_to_cache`, `restore_portrait_cache_from_tmp`), cinematic (`load_and_fade_in_cinematic_image`) | 9 |

### `field/` — 章節系統

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `chinit.c` | 28 個 `fd2_chapter_NN_init` handler (含 shared variants) | 28 |
| `chend1.c` | `fd2_chapter_NN_end` handler，章 1–19 | 19 |
| `chend2.c` | `fd2_chapter_NN_end` handler，章 20–30 | 11 |
| `chpost.c` | 17 個 `fd2_chapter_NN_post_action` handler (含 shared variants) | 17 |
| `chevt1.c` | `fd2_chapter_event_handler_NN_*`，handler index 00–2E + 早期 chapter event helper (`show_chapter_intro_text_dialog_mode_3`, `show_chapter_dialog_with_portrait_set_1`) | 49 |
| `chevt2.c` | `fd2_chapter_event_handler_NN_*`，handler index 2F–59 + cinematic helper (`wrap/cinematic_chapter_portrait_dump_with_white_flash`) | 45 |
| `chtrans.c` | 章節轉場 (`chapter_transition_menu`, `chapter_transition_with_intro`), 章節開場佈置 (`setup_chars_and_camera_for_intro`), cutscene 腳本解釋器 (`cutscene_event_trigger`) | 4 |

### `util/` — 工具函式

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `dpmi.c` | DPMI DOS 記憶體操作 (`dpmi_alloc/free_dos_memory`, `dpmi_lock/unlock_region`, `dpmi_lock/unlock_size`) | 6 |
| `pathfnd.c` | 移動範圍洪水填充 (`init_movement_range_floodfill`, `flood_fill_movement_range_recursive`, `flood_fill_neighbor_step`), A* 路徑搜索 (`pathfind_to_destination`, `pathfind_recursive_with_direction`, `pathfind_neighbor_step_with_tiebreak`, `pathfind_record_destination_xy`, `pathfind_count_unique_directions`, `pathfind_check_destination_save_path`) | 9 |
| `noop.c` | fall-through Pattern A 候選的 noop stub。`noop_stub_b43`/`c49`/`1011`/`1452`/`13994`/`15983` 已逐一確認為 DECOMPILER FRAGMENT（shared epilogue／return-tail），已改 `<fragment:inline-epilogue>` skip、不落在 noop.c。其餘 `noop_stub_4e915` 仍待各自 review 時逐一確認是否為 fragment（確認後比照前列改 skip） | 0 |
| `misc.c` | Debug (`debug_print_ans_and_length`), 原子交換 (`set_word_global_52758/5275c`), 隊伍查詢 (`any_char_has_item`, `check_party_has_char_id`, `require_char_id_in_active_party`, `count_selected_chars`, `reorder_party_by_selection`, `pin_required_char_to_party_slot1`, `find_template_char_by_id`), delay (`delay_400ms_via_idle_thunk`) | 11 |

### `crt/` — CRT 等價函式

| File | 包含的 function 類別 | fn |
|------|---------------------|-----|
| `crt.c` | 13 個 `crt_equivalent_*`: LX loader (`lx_chunk_read`, `lx_header_reader`, `lx_module_loader`), startup (`entry_start`, `dos_main_bootstrap`), exit (`exit_chain_stub`), FPU (`fpe_default_handler`, `softfp_tan_worker`), EFLAGS (`get_eflags`, `get_eflags_thunk`), math (`matherr_default_thunk`, `matherr_default_return_zero`), padding (`linker_padding`) | 13 |

---

## 二、Per-function Routing（653 entries）

完整 function → target 對照見 **`src/routing.json`**（JSON key = address hex）。

查詢範例：
```
python -c "import json; d=json.load(open('src/routing.json')); e=d['000115b6']; print(e['name'], '→', e['target'])"
# fd2_wait_for_action_target_input → input/input.c
```

### Pattern-based routing（Phase 6-7）

| Name pattern | Target |
|---|---|
| `fd2_chapter_NN_init*` | field/chinit.c |
| `fd2_chapter_NN_end` (NN 1–19 / 20–30) | field/chend1.c / chend2.c |
| `fd2_chapter_NN_post_action*` | field/chpost.c |
| `fd2_chapter_transition_*` | field/chtrans.c |
| `fd2_chapter_intro_menu_input_loop` | ui_menu/chintro.c |
| `fd2_chapter_event_handler_NN_*` (idx 00–2E / 2F–59) | field/chevt1.c / chevt2.c |

### Relocated helpers（地址在 event handler 區段的 Phase 4 函式）

| Function | Address | Target | 搬移原因 |
|----------|---------|--------|---------|
| `fd2_show_chapter_intro_text_dialog_mode_3` | `00034906` | field/chevt1.c | class-3 shared tail (JMP) |
| `fd2_show_chapter_dialog_with_portrait_set_1` | `00034be7` | field/chevt1.c | handler_05 JMP + handler_20 fall-through + 4 alt entry |
| `fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash` | `00035318` | field/chevt2.c | handler_3f tail-JMP target |
| `fd2_cinematic_chapter_portrait_dump_with_white_flash` | `00035822` | field/chevt2.c | caller 全是 event handler |

---

## 三、Decision Notes（非顯而易見的 routing 決策）

1. **`fd2_apply_use_effect_dispatch` → spell/spelleff.c**
   Plate 標 System=battle，但實際功能是 item/spell USE effect 頂層分派器，所有 case 都 delegate 到 spell worker functions。放入 spell effect worker 檔讓 caller → callee 在同一 .c 內。

2. **`fd2_flash_char_hit_sprite` → battle/battle.c**
   雖名含 "sprite" 暗示 gfx，但 plate 標 System=battle，功能是 per-frame 戰鬥受擊閃爍 blit，依 team + chapter 決定螢幕位置。屬戰鬥 helper。

3. **`fd2_compute_equipped_stats_with_item_preview` → ui_menu/status.c**
   Plate 標 System=ui_menu。唯一 caller 是 render_party_roster_with_item_stat_preview。純計算無副作用，屬裝備 preview UI。

4. **`fd2_read_tile_attribute_at_pos` → battle/battle.c**
   Wide xref accessor（battle core + AI + field 都用）。放 battle.c 作為 tile data 存取基礎設施。

5. **`fd2_game_main_loop` → ui_menu/menu.c**
   Plate 描述：per-frame game event handler，處理 keyboard dispatch + cursor + action menu + status screen。Plan 原本就把它歸在 menu_core。

6. **`fd2_load_save_and_init_engine` → life/main.c**
   主要 LOAD GAME 路徑，caller 是 main_menu_continue_dispatcher。屬 lifecycle entry point。

7. **`fd2_slide_panel_*` (7 fn) → anim/aniwalk.c**
   Plate 確認：row-copy helper for UI panel slide animations。被 status screen / dialog 的 slide-in/out 動畫呼叫。歸類為 animation helper（anim 子檔切分後落在 aniwalk.c）。

8. **`fd2_set_word_global_52758/5275c` → util/misc.c**
   0 callers，CRT-style atomic swap primitives。保留在 misc utilities。

9. **`fd2_delay_400ms_via_idle_thunk` → util/misc.c**
   Simple delay wrapper (PUSH 400, CALL __delay_thunk)。被 cinematic functions 呼叫。

10. **`fd2_obfuscate_battle_tile_map` → save/save.c**
    與 save_compute_checksum / save_crypt_buffer 同屬 save 資料處理 pipeline。

11. **`fd2_play_palette_fade_in/to_black` → gfx/palette.c**
    雖名含 "play" 暗示 anim，但功能是直接操作 VGA palette 暫存器的 fade loop。屬 palette 基礎設施。

12. **`fd2_noop_stub_b43` (0x10b43) → `<fragment:inline-epilogue>` (skip)**
    DECOMPILER FRAGMENT：純 caller-frame unwind（ADD ESP 0x4+0x8 / POP EBP/EDI/ESI/EBX / RET），
    被 `fd2_load_chapter_battle_data` (fall-through) + `fd2_play_rising_pre_cast_effect` / `fd2_play_variant_b_slide_pre_effect`
    (tail-JMP, +0x00) + 26 個 +0x03 tail-JMP 共用。依 `rebuild_info/emission/calling_convention.md`
    §「Decompiler fragments」epilogue cluster + `pipeline_spec.md` 模式A rule A-1，**不**獨立 emit 為 C function；
    epilogue 由 compiler 在各 parent 重新生成。routing.json 標 `skip:true`，不進 emit/review queue。

13. **`fd2_noop_stub_c49` (0x10c49) → `<fragment:inline-epilogue>` (skip)**
    DECOMPILER FRAGMENT：純 caller-frame unwind（ADD ESP 0x4 / POP EDI/ESI/EBX / RET，locals=0x4 + 3 saved regs），
    被 `fd2_convert_battle_tiles_to_24px` (JMP @0x13a3f) / `fd2_equip_unequip_inventory_menu` (JMP @0x1c13d) /
    `fd2_open_party_status_overview_screen` (JMP @0x1b418) 三個 tail-JMP（各為 parent 最後一條指令）共用。
    Ghidra `get_xrefs_to` 把這三個 JMP 標為 `UNCONDITIONAL_CALL` 是 display quirk，opcode 實為 JMP。
    依 `rebuild_info/emission/calling_convention.md` §「Decompiler fragments」epilogue cluster 0x10c49
    + `pipeline_spec.md` 模式A rule A-1，**不**獨立 emit 為 C function（原 `util/noop.c` 路由為誤判，已更正）；
    epilogue 由 compiler 在各 parent 重新生成。routing.json 標 `skip:true`，不進 emit/review queue。

14. **`fd2_noop_stub_1011` (0x11011) → `<fragment:inline-epilogue>` (skip)**
    DECOMPILER FRAGMENT：純 caller-frame unwind（ADD ESP 0x34 / POP EBP/EDI/ESI/EBX / RET，locals=0x34 + 4 saved regs），
    僅被唯一 parent `fd2_ai_walk_to_target_tile` (JMP @0x14eeb) 共用；該 JMP 為 parent 最後一條指令，
    其前一條 `MOV EAX,EBP` (@0x14ee9) 先把回傳值載入 EAX 再跳入 epilogue。
    Ghidra `get_xrefs_to` 把此 JMP 標為 `UNCONDITIONAL_CALL` 是 display quirk，opcode 實為 JMP。
    parent prologue `PUSH EBX/ESI/EDI/EBP` + `SUB ESP,0x34` (@0x14b82) 為此 epilogue 的精確逆操作。
    依 `rebuild_info/emission/calling_convention.md` §「Decompiler fragments」epilogue cluster 0x11011
    + `pipeline_spec.md` 模式A rule A-1，**不**獨立 emit 為 C function（原 `util/noop.c` 路由為誤判，已更正）；
    epilogue 由 compiler 在 parent 重新生成（`return ebp_value;`）。routing.json 標 `skip:true`，不進 emit/review queue。

15. **`fd2_noop_stub_1452` (0x11452) → `<fragment:inline-epilogue>` (skip)**
    DECOMPILER FRAGMENT：純 caller-frame unwind（ADD ESP 0x20 / POP EBP/EDI/ESI/EBX / RET，locals=0x20 + 4 saved regs；
    emulate_function ESP 0x100000→0x100034 = +0x20 locals +0x10 four POPs +0x4 RET）。+0x00 全 epilogue 被 3 個
    parent 共用（各有 `PUSH EBX/ESI/EDI/EBP` + `SUB ESP,0x20` prologue）：`fd2_animate_spell_projectile_paths`
    (JZ @0x1df7f + JMP @0x1e0d6) / `fd2_assemble_dialog_frame_layered` (JGE @0x16b39) / `fd2_render_inventory_item_grid`
    (JGE @0x186e4)；+0x03 alt-entry (0x11455，跳過 ADD ESP，無 local frame) 被 `fd2_save_runtime_char_to_template`
    (JGE @0x1151f，prologue 僅 `PUSH EBX/ESI/EDI/EBP`、無 SUB ESP) 共用。依 `rebuild_info/emission/calling_convention.md`
    §「Decompiler fragments」epilogue cluster 0x11452 + `pipeline_spec.md` rule A-1，**不**獨立 emit 為 C function
    （原 `util/noop.c` 路由為誤判，已更正）；epilogue 由 compiler 在各 parent 重新生成。routing.json 標 `skip:true`，不進 emit/review queue。

16. **`fd2_noop_stub_13994` (0x13994) → `<fragment:inline-epilogue>` (skip)**
    DECOMPILER FRAGMENT：純 caller-frame unwind（ADD ESP 0x5C / POP EBP/EDI/ESI/EBX / RET，locals=0x5C + 4 saved regs；
    call_count=0、param_count=0、cyclomatic=1），僅被唯一 parent `fd2_play_ending_and_record_clear` (JMP @0x1ff74) 共用；
    該 JMP 為 parent 最後一條指令，其前一條 `MOV EAX,EBP` (@0x1ff72) 先把回傳值載入 EAX 再跳入 epilogue。
    site bytes @0x1ff72 = `89 e8 e9 1b 3a ff ff`：opcode `0xE9` 為 near JMP、非 `0xE8` CALL（Ghidra `get_xrefs_to` 標
    `UNCONDITIONAL_CALL` 是 display quirk，opcode 實為 JMP）。parent prologue `PUSH EBX/ESI/EDI/EBP` + `SUB ESP,0x5c`
    (@0x1f89e) 為此 epilogue 的精確逆操作。依 `rebuild_info/emission/calling_convention.md` §「Decompiler fragments」
    epilogue cluster 0x13994 + `pipeline_spec.md` 模式A rule A-1，**不**獨立 emit 為 C function（原 `util/noop.c` 路由為誤判，已更正）；
    epilogue 由 compiler 在 parent 重新生成（`return ebp_value;`）。routing.json 標 `skip:true`，不進 emit/review queue。

17. **`fd2_score_item_candidate_tail_15983` (0x15983) → `<fragment:inline-epilogue>` (skip)**
    SHARED RETURN/EPILOGUE fragment（**非** no-op；帶回傳值）。Disasm 兩條：`MOV EAX,EDI`（把回傳值載入 EAX）
    + `JMP 0x22bbe`（落入共用 epilogue `ADD ESP,4 / POP EBP/EDI/ESI/EBX / RET`，此 epilogue 同時也是
    `fd2_composite_battle_frame_zero @0x22bb7` 的 tail）。被兩個 prologue 相同
    （`PUSH framesize; CALL __CHK; PUSH EBX/ESI/EDI/EBP; SUB ESP,0x4`）的 parent 共用：(1) `fd2_score_item_candidate`
    @0x15880（主要；3 條 early-exit conditional jump：JGE @0x158e5 / JNZ @0x15936 / JGE @0x15959，此處 EDI=total_score，
    即 `return total_score;`）；(2) `fd2_alloc_and_blit_indexed_sprite_chunk` @0x15f0e（tail-JMP @0x15f7f，為 parent 最後一條指令；
    Ghidra `get_xrefs_to` 標 `UNCONDITIONAL_CALL` 是 display quirk、opcode 實為 JMP/0xE9；此處 EDI=malloc 出的 buffer pointer，
    來自 `MOV EDI,EAX` @0x15f51）。依 `rebuild_info/emission/calling_convention.md` §「Decompiler fragments」
    + `pipeline_spec.md` 模式A rule A-1，**不**獨立 emit 為 C function（原 `util/noop.c` 路由 + `fd2_noop_stub_15983` 命名為誤判，已更正）；
    `MOV EAX,EDI` 回傳值載入 + epilogue 由 compiler 在各 parent 的 `return` 重新生成。routing.json 標 `skip:true`，不進 emit/review queue。

18. **大 subsystem 依子功能切成多個 ≤~1000 行的 .c（routing 規劃準則）**
    為控制每個 .c 的行數（model 讀寫效率），10 個投影會超過 ~1000 行的 base target 依內聚子功能切成 8.3 子檔：
    `anim/anim.c`→7（aniwalk/anicombt/anisummn/anicine/anispell/aniend/aniui）、
    `gfx/render.c`→3（rndscene/rndstat/rndmenu）、`gfx/blit.c`→2（blittile/blitspr）、
    `spell/spellwk.c`→3（spelleff/spellsel/spellcin）、`battle/btl_ai.c`→3（btl_ai/btl_aitg/btl_aisc）、
    `battle/btl_turn.c`→2（btl_turn/btl_init）、`ui_menu/shop.c`→3（shop/chintro/promote）、
    `ui_menu/menu.c`→3（menu/menucfg/menufld）、`field/chevt.c`→2（chevt1/chevt2 依 handler index）、
    `field/chend.c`→2（chend1/chend2 依章號）。切分規則編在 `tools/emit/mkroute.py` 的 `_subsplit()`；
    每個 function 的大小預估來自 Ghidra decompiled code 行數（見 `tools/file_split/`）。

19. **`fd2_set_tile_overlay_bit_80` → battle/btl_aitg.c（更正先前 btl_turn 誤判）**
    名稱含 "set_tile_overlay" 曾使 routing 規則歸入 btl_turn，但其唯一 caller 是
    `fd2_mark_aoe_plus_pattern_at`（AI AoE 標記），實為 AI targeting primitive，故歸 btl_aitg.c。

## 四、File 統計摘要（from routing.json）

大 subsystem 已依 §三-18 切成子檔；下表為 routing.json 現況（43 個 .c + 1 個 fragment skip 群）。

| Target | Phase 分布 | 總數 |
|--------|-----------|------|
| input/input.c | P1 | 15 |
| battle/battle.c | P1+P2+P5 | 22 |
| battle/btl_ai.c | P2 | 7 |
| battle/btl_aitg.c | P2+P5 | 14 |
| battle/btl_aisc.c | P2 | 5 |
| battle/btl_turn.c | P2+P5 | 19 |
| battle/btl_init.c | P2+P5 | 9 |
| table/table.c | P1 | 13 |
| spell/spell.c | P1 | 24 |
| spell/spelleff.c | P1+P4 | 17 |
| spell/spellsel.c | P4+P5 | 6 |
| spell/spellcin.c | P4 | 13 |
| ui_menu/cursor.c | P1 | 7 |
| ui_menu/menu.c | P5 | 4 |
| ui_menu/menucfg.c | P5 | 8 |
| ui_menu/menufld.c | P5 | 3 |
| ui_menu/status.c | P1+P5 | 17 |
| ui_menu/shop.c | P5 | 7 |
| ui_menu/chintro.c | P5+P6 | 6 |
| ui_menu/promote.c | P5 | 8 |
| gfx/blittile.c | P3 | 12 |
| gfx/blitspr.c | P3+P5 | 26 |
| gfx/rndscene.c | P3 | 18 |
| gfx/rndstat.c | P3 | 15 |
| gfx/rndmenu.c | P3 | 10 |
| gfx/palette.c | P1+P2+P3+P4 | 11 |
| anim/aniwalk.c | P2+P4 | 14 |
| anim/anicombt.c | P4 | 13 |
| anim/anisummn.c | P2 | 10 |
| anim/anicine.c | P4 | 8 |
| anim/anispell.c | P4 | 6 |
| anim/aniend.c | P4 | 5 |
| anim/aniui.c | P2+P4 | 11 |
| anim/anidec.c | P2 | 12 |
| dialog/dialog.c | P4+P5 | 14 |
| audio/audio.c | P2+P4+P5 | 6 |
| life/main.c | P2+P5 | 3 |
| save/save.c | P5 | 7 |
| rsrc/rsrc.c | P5 | 9 |
| field/chinit.c | P6 | 28 |
| field/chend1.c | P6 | 19 |
| field/chend2.c | P6 | 11 |
| field/chpost.c | P6 | 17 |
| field/chevt1.c | P4+P7 | 49 |
| field/chevt2.c | P4+P7 | 45 |
| field/chtrans.c | P4+P5+P6 | 4 |
| util/dpmi.c | P2 | 6 |
| util/pathfnd.c | P2 | 9 |
| util/noop.c | P2 | 1 |
| &lt;fragment:inline-epilogue&gt; | P2 | 6 |
| util/misc.c | P2+P4+P5 | 11 |
| crt/crt.c | P3 | 13 |
| **Total** | | **653** |
