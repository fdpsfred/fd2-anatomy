#ifndef PROTOS_H
#define PROTOS_H

#include "types.h"

/*
 * FD2 game function prototypes.
 * Populated incrementally during emission.
 * Convention: ~95% are __cdecl (Watcom -3s default).
 */

/* ---- replay/capture harness (TEST build only, -DFD2_REPLAY) ----
 * Defined in tests/play/{replay,capture}.c; compiled into FD2RP.EXE only.
 * In a production build (no -DFD2_REPLAY) these decls and every call site
 * vanish, so FD2.EXE is byte-identical. */
#ifdef FD2_REPLAY
void fd2_replay_init(void);   /* load input script + pin rng warm-up */
void fd2_replay_pump(void);   /* inject next scripted key / run captures */
#endif

/* ---- lifecycle ---- */
/* main() (the game C entry point, src/life/main.c @ 0x25BF4) is called by the
 * Watcom CRT startup; it is intentionally NOT prototyped here (declaring `main`
 * in a shared header is non-standard). SPECIAL CASE: it is the one game function
 * exempt from the project's `fd2_` prefix convention -- the C entry must be
 * literally `main` (CRT contract). Ghidra/src had named it fd2_main per that
 * convention; restored to `main` so the src-only FD2.EXE links. The TEST build
 * compiles src/life/main.c with -Dmain=fd2_main (tests/genbuild.py) to avoid a
 * duplicate-main with tests/testmain.c's runner main(). */

/* ---- table_accessor ---- */
uint8 *fd2_get_item_effect_entry(int item_id);
uint8 *fd2_get_spell_effect_entry(int spell_id);
uint8 *fd2_get_enemy_data_entry(int idx);
uint8 *fd2_get_char_base_entry(uint32 idx);
uint8 *fd2_get_char_growth_entry(int idx);
uint8 *fd2_get_chapter_intro_metadata_entry(int chapter_id);
uint8 *fd2_get_spell_learning_entry(int idx);
uint8 *fd2_get_class_promotion_data_entry(int class_id);
uint8 *fd2_get_attack_anim_pattern_for_weapon(int weapon_type);
uint8 *fd2_get_job_allowed_items_table_entry(int job_id);
uint8 *fd2_get_movement_cost_table_for_job(int job_id);
uint8 *fd2_get_cutscene_event_script(int event_id);
uint8 *fd2_get_orphan_packed3_table_entry(int idx);

/* ---- battle core ---- */
uint32 fd2_advance_rng_state(void);
void fd2_deduct_caster_mp(uint32 caster_idx, uint32 spell_id);
int fd2_apply_hp_heal_and_award_xp(uint32 target_idx, uint32 base_heal);
int fd2_apply_heal_spell_to_target(uint32 target_idx, uint32 spell_id);
int fd2_apply_damage_and_award_xp(uint32 target_idx, uint32 base_damage);
int fd2_apply_mp_heal_and_award_xp(uint32 target_idx, uint32 base_heal);
void fd2_compute_combat_bubble_screen_pos(uint32 out_xy_ptr, uint32 char_idx);
void fd2_calculate_combat_hit_outcome(uint32 attacker_idx, uint32 defender_idx, uint32 *outcome_out_ptr);
void fd2_flash_char_hit_sprite(uint32 workspace_buf, uint32 char_unit_id);
void fd2_render_mini_char_status_panel(uint32 buf, uint32 stride, uint32 char_idx);
int fd2_calc_magic_damage(uint32 target_idx, uint32 spell_id);
void fd2_recompute_runtime_char_total_stats(uint32 slot_idx);
void fd2_recalculate_combat_stats(uint32 char_idx);
int fd2_check_can_counter_attack(uint32 attacker_idx, uint32 defender_idx);
int fd2_check_can_default_attack_target(uint32 char_idx, uint32 tile_x, uint32 tile_y);
int fd2_execute_attack_damage_calculation(int attacker_idx, int defender_idx);
void fd2_read_tile_attribute_at_pos(uint32 x, uint32 y, uint32 buf_ptr);
void fd2_face_char_toward_target(uint32 actor_idx, uint32 target_idx);
int fd2_check_char_status_immunity(uint32 char_idx);
char fd2_resolve_terrain_for_aoe_targets(int n_chars, uint8 *target_idx_array);
int fd2_find_char_at_cursor_pos(void);
int fd2_find_char_by_id_or_template(uint32 char_id);
int fd2_game_main_loop(void);
int fd2_field_command_menu_loop(void);
void fd2_open_settings_dialog_with_slide(int32 *menu_options, int32 *menu_state);
int fd2_settings_menu_input_step(int32 *menu_options, int32 *menu_state);
void fd2_close_settings_dialog_with_slide(int32 *menu_options, int32 *menu_state);
int fd2_field_menu_status_save_load_quit_dispatch(void);
int fd2_text_dialog_typewriter_loop(void);
void fd2_animate_dialog_page_advance_collapse(void);
void fd2_game_options_menu_loop(void);
void fd2_count_active_menu_items_until_zero(int32 *menu_def);
int fd2_player_action_menu_loop(uint32 char_idx);
int fd2_player_inline_action_menu_dispatch(int char_idx,
    int32 *pSlot_disable_arr, int have_moved);
int fd2_spell_selection_menu_main(uint32 caster_idx);
int fd2_spell_select_input_loop(uint32 caster_idx);
int fd2_item_command_menu_dispatch(uint32 char_idx);
void fd2_handle_tile_event_interaction(uint32 char_idx);
void fd2_open_char_status_screen(uint32 char_idx);
void fd2_open_tactical_overview_zoom(void);
void fd2_mark_char_acted_this_turn(uint32 char_idx);
void fd2_check_all_player_acted_or_asleep(void);
void fd2_check_tile_event_post_action(uint32 x, uint32 y, uint32 arg);
void fd2_mark_char_as_dead(uint32 char_idx);
void fd2_kill_runtime_chars_from_index_to_end(uint32 start_char_idx);
void fd2_set_chapter_init_done_flag(void);
void fd2_set_battle_anim_phase_to_1(void);
void fd2_set_combat_aux_block_byte_d_low4_for_char_range(uint32 start_idx, uint32 end_idx, uint32 new_val);
void fd2_check_battle_end_condition(void);
void fd2_check_battle_end_default_handler(uint32 event_arg);
int fd2_collect_dead_char_drops(uint32 out_buffer);
uint32 fd2_find_equipped_item_by_kind(uint32 char_idx, uint32 kind);
uint8 fd2_get_inventory_slot_item_id(uint32 char_idx, uint32 slot);
int fd2_check_job_can_equip_item(uint32 char_idx, uint32 item_id);
void fd2_equip_item_in_slot(uint32 char_idx, uint32 slot_idx);
void fd2_give_item_to_first_player_char(uint32 item_id);
int fd2_inventory_selection_modal_dispatch(uint32 char_idx, uint32 mode);
int fd2_inventory_grid_input_step(uint32 char_idx, uint32 gate_flag);
void fd2_handle_tile_event_interaction(uint32 char_idx);
void fd2_run_full_turn_cycle(void);

/* ---- spell handlers ---- */
void fd2_spell_handler_id_0_via_targeted_blink(int, int, uint8 *);
void fd2_spell_handler_id_1_via_targeted_blink(int, int, uint8 *);
void fd2_spell_handler_id_2_via_targeted_blink(int, int, uint8 *);
void fd2_spell_handler_id_3_via_targeted_blink(int, int, uint8 *);
void fd2_spell_handler_id_4_via_full_screen_flash(int, int, uint8 *);
void fd2_spell_handler_id_5_via_full_screen_flash(int, int, uint8 *);
void fd2_spell_handler_id_6_via_full_screen_flash(int, int, uint8 *);
void fd2_spell_handler_id_7_via_full_screen_flash(int, int, uint8 *);
void fd2_spell_handler_id_8_via_targeted_blink(int, int, uint8 *);
void fd2_cast_spell_0a_basic(int, int, uint8 *);
void fd2_cast_spell_0b_with_prefx(int, int, uint8 *);
void fd2_cast_spell_0c_with_prefx(int, int, uint8 *);
void fd2_cast_spell_0d_variant_b(int, int, uint8 *);
void fd2_cast_spell_0e_variant_b(int, int, uint8 *);
void fd2_cast_spell_0f_variant_b(uint32, uint32, uint8 *);
void fd2_cast_spell_10_variant_b(uint32, uint32, uint8 *);
void fd2_cast_spell_11_ap_boost(int, int, uint8 *);
void fd2_cast_spell_12_dp_boost(int, int, uint8 *);
void fd2_cast_spell_13_speed_boost(uint32, uint32, uint8 *);
void fd2_cast_spell_14_dispatch(int, int, uint8 *);
void fd2_cast_spell_15_dispatch(int, int, uint8 *);
void fd2_cast_spell_16_dispatch(int, int, uint8 *);
void fd2_cast_spell_1a_dispatch(int, int, int);
void fd2_cast_spell_1b_dispatch(int, int, int);
void fd2_cast_spell_17_teleport(uint32, uint32, uint32);
/* spell worker forward decls */
void fd2_execute_offensive_targeted_spell(int, int, int, int);
void fd2_execute_offensive_targeted_spell_variant_b(int, int, int, int);
void fd2_execute_offensive_full_screen_flash_spell(int, int, int, int);
void fd2_execute_offensive_single_target_spell_id_9(int, int, uint8 *);
void fd2_cast_earthquake_spell_with_screen_shake(uint32, uint32, uint32, uint8 *);
void fd2_execute_variant_b_heal_cast(int, int, int, int);
void fd2_apply_status_effect_with_anim(int, int, int, int, int);
void fd2_cast_status_inflict_spell_wrapper(int, int, int, int, int);
void fd2_cast_ap_boost_spell(int, int, uint8 *);
void fd2_cast_dp_boost_spell(int, int, uint32);
void fd2_cast_speed_boost_spell(uint32, uint32, uint32);
void fd2_execute_reactivate_spell_id_25(int, int, uint8 *);
void fd2_animate_spell_impact_per_target(uint32, uint32, uint32, uint32);
void fd2_animate_status_effect_overlay_flicker(uint32, uint32, uint32, uint32);
void fd2_animate_spell_full_screen_flash(uint32, uint32, uint32, uint32);
void fd2_animate_palette_flash_pulse_white(void);
void fd2_cast_screen_wide_spell_with_fade(uint32 epicenter_tile_x, uint32 epicenter_tile_y, uint32 starting_radius, int radius_increment);
void fd2_animate_spell_overlay_blink(uint32, uint32, uint32, uint32);
void fd2_show_damage_number(uint32 val, uint32 type, uint32 target);
void fd2_show_miss_indicator(uint32 target);
void fd2_show_status_effect_overlay(uint32 target, uint32 spell_id);
void fd2_animate_spell_projectile_paths(void);
void fd2_composite_then_animate_projectiles(void);
void fd2_remove_inventory_slot_at(uint32 char_idx, uint32 slot);
void fd2_apply_item_stat_modifier_with_anim(uint32, uint32, uint32, uint32, uint32, uint32, uint32);
void fd2_apply_attack_spell_damage(uint32, uint32, uint32, uint32);
void fd2_apply_use_effect_dispatch(uint32, uint32, uint32, uint32);
void fd2_load_status_effect_sfx(void);
void fd2_stop_and_free_status_effect_sfx(void);
int fd2_collect_pending_death_drops(uint32 out_buffer);
void fd2_play_death_animation_and_mark_dead(void);
void fd2_kill_runtime_chars_from_index_to_end(uint32 start_char_idx);
void fd2_process_battle_drop_entries(uint32, uint32, uint32);
void fd2_cast_group_hp_heal_spell(uint32, uint32, uint32, uint32);
void fd2_cast_status_cure_spell(uint32, uint32, uint32, uint32, uint32);
void fd2_cast_status_inflict_spell(uint32, uint32, uint32, uint32, uint32);
void fd2_play_sfx_with_handle(uint32, int, int);
void fd2_play_rising_pre_cast_effect(int, int, int);
void fd2_play_variant_b_slide_pre_effect(int, int);
void fd2_execute_aoe_spell_with_caster_portrait_radial_scatter(int, int, int, int, int, int, uint32 *);
void fd2_scatter_sprite_around_origin_with_random_offset(int scatter_range_max, int sprite_array_index, uint32 sprite_x_array_addr, uint32 sprite_y_array_addr, uint32 sprite_type_array_addr, int origin_x, int origin_y);
void fd2_blit_palette_remap_with_sprite_mask(uint8 *dst, uint16 *sprite_mask, uint32 stride, uint32 remap_table);
void fd2_animate_warp_teleport_char(uint32, uint32, uint32, uint32, uint32);
void fd2_animate_warp_portal_open_at(uint32, uint32, uint32);
int  fd2_animate_warp_out_collapse(int, int, void *, uint32, uint32, int);
void fd2_animate_warp_in_expand(uint32, uint32, uint32, uint32, uint32, uint32 *, int);
void fd2_cast_screen_wide_spell_with_fade(uint32, uint32, uint32, int);
void fd2_execute_special_attack_skill(uint32 caster_idx, uint32 spell_id, int n_targets, uint8 *target_idx_buf);
void fd2_execute_summon_spell_cast(uint32 caster_idx, uint32 spell_id, uint32 n_targets, int target_id_array);
uint32 fd2_load_figani_sfx_bank(uint32 figani_data);
void fd2_animate_bg_zoom_transition_in(uint32 char_unit_id, uint32 char_figani, uint32 clear_buf, uint32 workspace, uint32 terrain_bg);
void fd2_animate_bg_zoom_transition_out(uint32 char_unit_id, uint32 char_figani, uint32 name_banner_sprite, uint32 clear_buf, uint32 workspace, uint32 terrain_bg);
void fd2_play_class_promotion_cinematic(uint32 promoting_char_idx, uint32 target_class_id);
void fd2_cycle_sprite_anim_with_bg_frames(uint32 sprite_atlas, uint32 workspace, uint32 iter_count);
void fd2_play_char_intro_zoom_anim(uint32 char_unit_id, uint32 mode_flag, uint32 char_sprite, uint32 char_sprite2, uint32 workspace, uint32 bg_sprite, uint32 name_banner_sprite);
void fd2_restore_portrait_cache_from_tmp(void);
void fd2_cinematic_warp_char_to_tile(uint32 char_id, uint32 tile_x, uint32 tile_y);

/* ---- cursor + pan ---- */
void fd2_cursor_move_up(void);
void fd2_cursor_move_down(void);
void fd2_cursor_move_right(void);
void fd2_cursor_move_left(void);
void fd2_pan_cursor_to_tile_animated(int target_x, int target_y);
void fd2_pan_cursor_to_char(uint32 char_idx);
void fd2_pan_cursor_and_window(uint32 target_ox, uint32 target_oy);
void fd2_composite_battle_frame(int skip_palette_cycle);
void fd2_animate_party_addition_with_appear_effect(uint32 chapter_id);
void fd2_composite_then_animate_projectiles(void);
void fd2_composite_battle_frame_zero(void);
int fd2_render_summon_aura_sprite_ring(int caster_unit_id, int sprite_handle, int dst_buf_base, int row_stride, char state_code);

/* ---- graphics / palette ---- */
void fd2_set_vga_palette_range(uint32 start_idx, uint32 end_idx, uint32 brightness_subtract);
void fd2_set_vga_palette_range_with_add(uint32 start_idx, uint32 end_idx, uint32 brightness_add);
void fd2_animate_palette_flash_pulse_white(void);
void fd2_set_full_vga_palette_to_color(uint32 r, uint32 g, uint32 b);
void fd2_palette_overbright_settle_step_loop(uint32 start_intensity, uint32 step_delay_ms);
void fd2_interpolate_palette_range_toward_color(uint32 start_idx, uint32 end_idx, uint32 blend, uint32 target_r, uint32 target_g, uint32 target_b);
void fd2_apply_palette_remap_run(uint32 remap_table, uint32 byte_count, uint8 *buf);
void fd2_fill_palette_blink_pattern_6byte(int input_index, uint32 output_buffer_addr);
void fd2_render_circle_anim_row(int cx, int cy, int r, int scale_num, int start_row, int end_row, uint8 *palette_remap_src);
void fd2_render_filled_circle_band_anim(uint32 col_center, uint32 bottom_row, uint32 radius_factor, int top_row, int row_loop_end, int palette_remap_src);
void fd2_tick_chapter_palette_animation(void);
void fd2_update_palette_cycle_anim(void);
void fd2_animate_palette_flash_pulse_white(void);

/* ---- ui_menu / status ---- */
void fd2_open_shop_dialog_panel(uint32 item_count, uint8 *item_id_array, uint32 sell_mode_flag);
int  fd2_shop_menu_input_loop(uint32 item_count, uint8 *item_id_array, uint32 sell_mode_flag);
uint32 fd2_pick_stat_compare_color(int current_stat, int preview_stat);
void fd2_run_buy_item_menu(uint32 shop_item_count, uint8 *shop_item_id_array);
void fd2_run_sell_item_menu(void);
void fd2_run_equip_member_menu(void);
void fd2_run_give_item_menu(void);
int  fd2_party_roster_class_select_loop(uint32 candidate_count, uint32 candidate_array_ptr, uint32 item_id);
int  fd2_party_roster_single_select_loop(void);
void fd2_animate_shop_transaction_feedback(void);
void fd2_animate_scroll_up_in_shop_dialog(void);
void fd2_animate_scroll_down_in_shop_dialog(void);
void fd2_equip_unequip_inventory_menu(uint32 char_idx);
void fd2_compute_equipped_stats_with_item_preview(uint32 char_idx, uint32 item_id, uint32 stats_out_ptr);
void fd2_open_party_status_overview_screen(void);
void fd2_render_party_status_overview_content(uint32 dst_surface, uint32 stride);
int fd2_count_active_chars_for_team_filter(uint32 team);
uint32 fd2_check_party_has_char_id(uint32 char_id);

/* ---- field cutscene ---- */
void fd2_cutscene_event_trigger(uint32 event_id);
void fd2_setup_chars_and_camera_for_intro(uint32 pX_byte_array,
                                          uint32 pY_byte_array,
                                          uint32 sprite_facing_fixed_or_array,
                                          int char_start, int char_end,
                                          uint32 extra_char_idx,
                                          int extra_x, int extra_y,
                                          int extra_facing,
                                          int origin_x, int origin_y);

/* ---- chapter post-action handlers ---- */
void fd2_chapter_02_post_action(uint32 event_arg);
void fd2_chapter_10_post_action(uint32 event_arg);
void fd2_chapter_12_post_action(uint32 event_arg);
void fd2_chapter_13_post_action(uint32 event_arg);
void fd2_chapter_15_post_action(uint32 event_arg);
void fd2_chapter_16_post_action(uint32 event_arg);
void fd2_chapter_17_post_action(uint32 event_arg);
void fd2_chapter_18_post_action(uint32 event_arg);
void fd2_chapter_19_post_action(uint32 event_arg);
void fd2_chapter_20_post_action(uint32 event_arg);
void fd2_chapter_21_post_action(uint32 event_arg);
void fd2_chapter_22_27_28_post_action_shared(uint32 event_arg);
void fd2_chapter_23_post_action(uint32 event_arg);
void fd2_chapter_25_post_action(uint32 event_arg);
void fd2_chapter_26_post_action(uint32 event_arg);
void fd2_chapter_29_post_action(uint32 event_arg);
void fd2_chapter_30_post_action(uint32 event_arg);

/* ---- chapter init handlers ---- */
void fd2_chapter_01_init(void);
void fd2_chapter_02_init(void);
void fd2_chapter_03_init(void);
void fd2_chapter_04_init(void);
void fd2_chapter_05_init(void);
void fd2_chapter_06_init(void);
void fd2_chapter_07_init(void);
void fd2_chapter_08_init(void);
void fd2_chapter_09_init(void);
void fd2_chapter_10_init(void);
void fd2_chapter_11_init(void);
void fd2_chapter_12_init(void);
void fd2_chapter_13_init(void);
void fd2_chapter_14_init(void);
void fd2_chapter_15_init(void);
void fd2_chapter_16_init(void);
void fd2_chapter_17_init(void);
void fd2_chapter_18_init(void);
void fd2_chapter_19_20_21_init_shared(void);
void fd2_chapter_22_init(void);
void fd2_chapter_23_init(void);
void fd2_chapter_24_init(void);
void fd2_chapter_25_init(void);
void fd2_chapter_26_init(void);
void fd2_chapter_27_init(void);
void fd2_chapter_28_init(void);
void fd2_chapter_29_init(void);
void fd2_chapter_30_init(void);

/* ---- chapter end handlers ---- */
void fd2_chapter_20_end(void);
void fd2_chapter_21_end(void);
void fd2_chapter_22_end(void);
void fd2_chapter_23_end(void);
void fd2_chapter_24_end(void);
void fd2_chapter_25_end(void);
void fd2_chapter_26_end(void);
void fd2_chapter_27_end(void);
void fd2_chapter_28_end(void);
void fd2_chapter_29_end(void);
void fd2_chapter_30_end(void);

/* ---- chapter turn-event handlers (dispatch table 0x51B91) ---- */
void fd2_chapter_event_handler_00__ch1_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_01__ch1_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_02__ch1_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_03__ch1_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_04__unref_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_06__ch2_reinforcement(uint32 event_arg);
void fd2_chapter_event_handler_07__ch13_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_08__ch13_first_time(uint32 stepping_char_id);
void fd2_chapter_event_handler_09__ch3_char_cond(uint32 event_arg);
void fd2_chapter_event_handler_0a__ch14_first_time(uint32 event_arg);
void fd2_chapter_event_handler_0b__ch4_dialog(uint32 event_arg);
void fd2_chapter_event_handler_0c__unref_first_time(uint32 event_arg);
void fd2_chapter_event_handler_0d__ch15_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_0e__ch5_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_0f__ch5_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_10__ch5_dialog(uint32 event_arg);
void fd2_chapter_event_handler_11__ch5_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_12__ch15_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_13__unref_char_cond(uint32 event_arg);
void fd2_chapter_event_handler_14__ch6_dialog(uint32 event_arg);
void fd2_chapter_event_handler_15__ch6_char_cond(uint32 event_arg);
void fd2_chapter_event_handler_16__ch6_char_cond(uint32 event_arg);
void fd2_chapter_event_handler_17__unref_turn_gated(uint32 event_arg);
void fd2_chapter_event_handler_18__unref_dialog(uint32 event_arg);
void fd2_show_chapter_intro_text_dialog_mode_3(void);
void fd2_chapter_event_handler_19__ch7_first_time(uint32 event_arg);
void fd2_chapter_event_handler_1a__ch7_char_cond(uint32 stepping_char_id);
void fd2_chapter_event_handler_1b__ch8_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_1c__ch8_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_1d__unref_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_1e__unref_major_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_1f__ch9_reinforcement(uint32 event_arg);
void fd2_chapter_event_handler_20__ch10_dialog(uint32 event_arg);
void fd2_chapter_event_handler_21__ch10_dialog_with_state(uint32 event_arg);
void fd2_show_chapter_dialog_with_portrait_set_1(void);
void fd2_chapter_event_handler_05__ch13_thunk(uint32 event_arg);
void fd2_chapter_event_handler_22__unref_dialog(uint32 event_arg);
void fd2_chapter_event_handler_23__ch12_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_24__ch12_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_25__unref_major_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_26__ch15_dialog(uint32 event_arg);
void fd2_chapter_event_handler_27__unref_drop(uint32 stepping_char_id);
void fd2_chapter_event_handler_28__ch17_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_29__unref_drop(uint32 stepping_char_id);
void fd2_chapter_event_handler_2a__ch18_dialog(uint32 event_arg);
void fd2_chapter_event_handler_2b__ch18_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_2c__ch19_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_2d__ch19_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_2e__ch19_reinforcement(uint32 event_arg);

void fd2_play_chapter_21_hidden_stage_unlock_cinematic(void);
void fd2_play_game_ending_cinematic(void);
int fd2_any_char_has_item(int item_id);
int fd2_find_template_char_by_id(uint32 char_id);
char fd2_require_char_id_in_active_party(uint32 max_chars, uint32 req_char_id);
void fd2_animate_screen_shake(uint32 frame_count);

/* ---- chapter event handlers (dispatch table @ 0x51B91) ---- */
void fd2_chapter_event_handler_2f__ch21_turn_gated(uint32 event_arg);
void fd2_chapter_event_handler_30__ch21_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_31__ch22_turn_gated(uint32 event_arg);
void fd2_chapter_event_handler_32__ch22_reinforcement(uint32 event_arg);
void fd2_chapter_event_handler_33__unref_drop(uint32 stepping_char_id);
void fd2_chapter_event_handler_34__ch23_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_35__unref_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_36__ch24_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_37__ch25_first_time(uint32 event_arg);
void fd2_chapter_event_handler_38__ch25_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_39__ch26_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_3a__unref_pickup(uint32 stepping_char_id);
void fd2_chapter_event_handler_3b__ch26_ai_ctrl(uint32 stepping_char_id);
void fd2_chapter_event_handler_3c__ch26_ai_ctrl(uint32 stepping_char_id);
void fd2_chapter_event_handler_3d__ch26_pickup(uint32 stepping_char_id);
void fd2_chapter_event_handler_3e__ch27_dyn_turn_event(uint32 event_arg);
void fd2_chapter_event_handler_3f__ch27_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_40__unref_dyn_turn_event(uint32 event_arg);
void fd2_chapter_event_handler_41__shared_dyn_turn_event(uint32 event_arg);
void fd2_chapter_event_handler_42__ch28_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_43__unref_dyn_turn_event(uint32 event_arg);
void fd2_chapter_event_handler_44__ch28_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_45__ch28_dyn_turn_event(uint32 stepping_char_id);
void fd2_chapter_event_handler_46__ch28_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_47__unref_dyn_turn_event(uint32 event_arg);
void fd2_chapter_event_handler_48__unref_portrait_cinematic_pair(uint32 event_arg);
void fd2_chapter_event_handler_49__unref_sentinel(uint32 event_arg);
void fd2_chapter_event_handler_4a__ch29_dyn_turn_event(uint32 event_arg);
void fd2_chapter_event_handler_4b__ch29_major_cinematic(uint32 stepping_char_id);
void fd2_chapter_event_handler_4c__ch29_major_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_4d__unref_sentinel(uint32 event_arg);
void fd2_chapter_event_handler_4e__unref_sentinel(uint32 event_arg);
void fd2_chapter_event_handler_4f__ch29_dyn_turn_event(uint32 event_arg);
void fd2_chapter_event_handler_50__ch30_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_51__unref_dyn_turn_event(uint32 event_arg);
void fd2_chapter_event_handler_52__ch30_major_cinematic(uint32 event_arg);
void fd2_chapter_event_handler_53__unref_dialog_with_state(uint32 event_arg);
void fd2_chapter_event_handler_54__ch27_ai_ctrl(uint32 event_arg);
void fd2_chapter_event_handler_55__unref_sentinel(uint32 event_arg);
void fd2_chapter_event_handler_56__unref_sentinel(uint32 event_arg);
void fd2_chapter_event_handler_57__unref_sentinel(uint32 event_arg);
void fd2_chapter_event_handler_58__unref_sentinel(uint32 event_arg);
void fd2_chapter_event_handler_59__unref_sentinel(uint32 event_arg);
void fd2_cinematic_chapter_portrait_dump_with_white_flash(uint32 target_tile_x, uint32 target_tile_y, uint32 portrait_set_id);
void fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash(uint32 target_tile_x, uint32 target_tile_y, uint32 chapter_id);
void fd2_cinematic_warp_char_to_tile(uint32 char_id, uint32 tile_x, uint32 tile_y);

/* ---- animation ---- */
void fd2_play_ani_file_animation_sequence(uint32 anim_idx, uint32 per_frame_delay,
                                          uint32 skip_on_key_flag);
void fd2_tick_tile_event_animations(void);
void fd2_walk_step_down(uint32 char_idx);
void fd2_walk_step_left(uint32 char_idx);
void fd2_walk_step_up(uint32 char_idx);
void fd2_walk_step_right(uint32 char_idx);
void fd2_walk_path_animation_loop(uint32 char_idx, uint32 path_buf, uint32 step_count);
void fd2_tick_walk_step_footstep_sfx(uint32 walking_char_idx);
void fd2_animate_screen_shake(uint32 num_frames);
void fd2_animate_money_increment(uint32 delta);
void fd2_animate_money_decrement(uint32 delta);
void fd2_animate_chapter_intro_dialog_wings(uint32 open_or_close);
void fd2_animate_party_addition_with_appear_effect(uint32 target_race_id);
void fd2_slide_panel_step_left_main(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_step_right_main(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_step_top_small(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_step_bottom_main(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_step_bottom_small(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_up_partial_step(uint32 y_offset, uint32 dst_workspace, uint32 src_buffer);
void fd2_slide_panel_down_step(uint32 y_offset, uint32 dst_workspace, uint32 src_buffer);
void fd2_open_status_screen_with_slide_in(uint32 char_idx);
void fd2_paint_status_panel_layer_left(uint32 x_offset, uint32 dst_workspace, uint32 src_buffer);
void fd2_paint_status_panel_layer_right(uint32 y_offset, uint32 dst_workspace, uint32 src_buffer);
void fd2_render_status_screen_slide_frame(uint32 frame_idx, uint32 workspace, uint32 src_buffer, int snapshot_b);
void fd2_render_status_screen_static_layout(uint32 char_idx, uint32 overlay_buffer);
void fd2_render_full_char_stat_panel(uint32 char_idx, uint32 overlay_buffer);
void fd2_render_inventory_item_grid(uint32 char_idx, int highlight_slot, uint32 dst_buf);
void fd2_draw_spell_selection_list(uint32 char_idx, uint32 spell_idx, uint32 overlay_buffer);
void fd2_display_cinematic_image_with_fade(uint32 image1_idx, uint32 palette_idx,
                                           uint32 src_buf, int32 row_idx);
void fd2_tick_sprite_animation_step(uint8 *p_frame_idx, uint8 *p_tick, int dst_buf, int dst_stride, uint32 sheet_ptr);
void fd2_blit_indexed_sprite(uint32 sheet_ptr, uint32 sprite_idx, uint32 dst_buf,
                             int32 dst_stride, uint32 palette_op);
void fd2_ani_decoder_set_target_buffer(uint16 width, uint32 dst_buf, uint32 src_buf);
int fd2_tally_chars_with_zero_at_field(int len, uint32 char_idx_arr, int field_offset, int weight);
int fd2_find_tile_with_attribute_match(uint32 target_tag, uint32 out_pos);
int fd2_collect_unmarked_tile_positions(uint32 out_buf);
uint8 fd2_pathfind_count_unique_directions(void);
void fd2_flood_fill_movement_range_recursive(uint8 x, uint8 y, uint8 cost, uint8 *btm_ptr);
/* returns non-zero (binary: carry clear) iff the neighbour improved and the
 * caller should recurse; on non-zero, *new_cost_out = residual cost - tile cost */
int fd2_flood_fill_neighbor_step(uint8 remaining_cost, uint8 *btm_attr_ptr, uint8 *new_cost_out);
void fd2_pathfind_recursive_with_direction(uint8 x, uint8 y, uint8 cost, uint8 *btm_ptr);
/* x/y = neighbour coords (seen by the destination helpers as DL/DH); returns
 * non-zero (binary: carry clear) iff the neighbour improved and the caller should
 * recurse, with *new_cost_out = the reduced residual to recurse with. */
int fd2_pathfind_neighbor_step_with_tiebreak(uint8 x, uint8 y, uint8 remaining_cost,
    uint8 *btm_attr_ptr, uint8 *new_cost_out);
/* fd2_pathfind_record_destination_xy @ 0x4E3B3, fd2_pathfind_check_destination_save_path
 * @ 0x4E401: mode-2 / mode-0&1 destination helpers called by
 * fd2_pathfind_neighbor_step_with_tiebreak (both real-emitted in src/util/pathfnd.c).
 * x/y = neighbour coords (binary DL/DH). */
void fd2_pathfind_record_destination_xy(uint8 x, uint8 y, uint8 *btm_attr_ptr);
void fd2_pathfind_check_destination_save_path(uint8 x, uint8 y);
void fd2_mark_char_occupant_tiles_for_team(uint32 exclude_idx, uint32 team_selector);
void fd2_set_tile_overlay_bit_80(uint32 x, uint32 y);
void fd2_mark_aoe_plus_pattern_at(uint32 x, uint32 y);
void fd2_enemy_turn_action_dispatcher(uint32 char_idx, uint32 team);
void fd2_ai_score_offensive_spell(uint32 char_idx, uint32 mode);
void fd2_ai_score_item_use(uint32 char_idx, uint32 mode);
int fd2_score_item_candidate(uint32 item_id, uint32 n_targets, uint32 target_array_ptr);
void fd2_npc_turn_phase_team1(void);
void fd2_enemy_turn_phase_team0(void);
int fd2_scan_chars_along_line_with_team_filter(
    uint32 target_x, uint32 target_y, uint32 out_buf,
    uint32 start_x, uint32 start_y, uint32 step_count,
    uint32 team_filter);
int fd2_attack_action_dispatch(uint32 char_idx, uint32 team);
int fd2_ai_seek_optimal_position(uint32 char_idx, uint32 team);
int fd2_ai_advance_to_nearest_team_target(uint32 char_idx, uint32 team);
int fd2_ai_pass_turn_with_heal(uint32 char_idx);
int fd2_ai_walk_to_target_tile(uint32 tx, uint32 ty, uint32 ci, uint32 team);
int fd2_ai_score_physical_attack(uint32 char_idx, uint32 team);
int fd2_execute_ai_offensive_spell(uint32 char_idx, uint32 team);
void fd2_play_spell_cast_sequence(uint32 ci, uint32 si, uint32 nt, uint32 tb);
void fd2_play_spell_palette_flash_with_sfx(int spell_id);
int fd2_execute_ai_physical_attack(uint32 char_idx, uint32 team);
uint32 fd2_animate_combat_speech_bubbles(uint32 ci, uint32 ti);
void fd2_render_combatant_hp_bar_proportional(uint32 d, uint32 s, uint32 ci, uint32 st);
void fd2_render_combat_hp_bar_segments(uint32 dst_addr, uint32 stride, uint32 filled_count);
int fd2_animate_combat_hit_with_hp_drain(uint32 a, uint32 d, uint32 st);
void fd2_animate_attack_hit_sequence(uint32 attacker_idx, uint32 defender_idx);
void fd2_render_combat_combatant_panels(uint32 xy_array_ptr, uint32 defender_idx, uint32 attacker_idx);
void fd2_play_full_combat_cinematic(uint32 attacker_idx, uint32 defender_idx);
int fd2_execute_combat_hit_cinematic(uint32 attacker_idx, uint32 defender_idx,
    uint32 figani_anim, uint32 silhouette, uint32 workbuf, uint32 dst,
    uint32 banner, uint32 sfx_bank);
void fd2_animate_bg_zoom_transition_in(uint32 char_unit_id, uint32 char_figani,
    uint32 clear_buf, uint32 workspace, uint32 terrain_bg);
void fd2_animate_bg_zoom_transition_out(uint32 char_unit_id, uint32 char_figani,
    uint32 name_banner_sprite, uint32 clear_buf, uint32 workspace, uint32 terrain_bg);
void fd2_process_xp_and_level_up_for_char(uint32 char_idx);
int fd2_roll_stat_gain_and_show_message(short *stat_ptr, uint8 *growth_pair, uint32 dialog_text_id, int row_idx);
void fd2_grant_spell_to_char(uint32 char_idx, uint32 spell_id);
int fd2_count_usable_inventory_slots(uint32 char_idx);
int fd2_build_usable_spell_list(uint32 char_idx, uint32 out_buf);
void fd2_grant_spell_to_char(uint32 char_idx, uint32 spell_id);
int fd2_score_spell_candidate(uint32 si, uint32 nt, uint32 tb);
void fd2_execute_ai_item_use(uint32 char_idx, uint32 ctx);
void fd2_play_figani_char_intro_animation(uint32 char_idx);
void fd2_play_char_intro_zoom_anim(uint32 char_unit_id, uint32 mode_flag,
                                   uint32 char_sprite, uint32 char_sprite2,
                                   uint32 workspace, uint32 bg_sprite,
                                   uint32 name_banner_sprite);
void fd2_play_figani_animation_loop(uint32 caster_idx, uint32 spell_id,
                                    uint8 *caster_figani, uint8 *target_figani,
                                    uint32 workspace, uint32 dst_buf,
                                    uint32 bg_layer_a, uint32 bg_layer_b);
void fd2_step_figani_pose_animation(uint32 figani_data, uint32 palette_op,
                                    uint32 dst_buf, uint32 dst_stride);
void fd2_animate_spell_hit_cinematic(uint32 attacker_idx, uint32 dispatch_sprite_atlas,
                                     uint32 spell_sprite_atlas, int caster_sprite_atlas,
                                     uint32 base_workspace_offset, uint32 bg_workbuf,
                                     int hit_effect_sprite, int spell_type_idx);
uint32 fd2_load_figani_sfx_bank(uint32 figani_data);
void fd2_apply_use_effect_dispatch(uint32 ci, uint32 sl, uint32 n, uint32 buf);
void fd2_clear_all_chars_facing(void);
void fd2_clear_all_chars_acted_flag(void);
int fd2_add_item_to_inventory(uint32 char_idx, uint32 item_id);
void fd2_play_sfx_sample_from_bank(uint32 bank_ptr, uint32 sfx_id, uint32 loop_count);
void fd2_paint_char_sprite_at_world_with_mode(uint32 ws, uint32 stride, uint32 ci, uint32 mode, uint32 color);
void fd2_paint_threat_overlay_for_team(uint32 team_selector);
uint8 fd2_pathfind_to_destination(uint32 ct, uint32 sx, uint32 sy, uint32 ms,
    uint32 db, uint32 f1, uint32 f2, uint32 md, uint32 tm, uint32 af);
void fd2_obfuscate_battle_tile_map(uint32 tile_map);
void fd2_init_movement_range_floodfill(uint32 ct, uint32 x, uint32 y,
    uint32 rng, uint32 tm, uint32 af);
int fd2_compute_aoe_targets(uint32 cx, uint32 cy, uint32 buf,
    uint32 range, uint32 aoe_r, uint32 team_f);

/* ---- battle turn / status ---- */
void fd2_tick_status_effects_and_show_messages(uint32 team);
void fd2_fire_chapter_turn_events_for_phase(uint32 phase);
void fd2_maybe_load_speed_mode_overlay(void);
void fd2_maybe_free_speed_mode_overlay(void);
void fd2_animate_phase_banner_slide_in(uint32 banner_sprite_id);
void fd2_animate_phase_banner_slide_out(uint32 banner_sprite_id);
void fd2_render_phase_banner_frame(uint32 x_offset, uint32 banner_sprite_id);
void fd2_scroll_buffer_block_with_wrap(uint32 wrap_param, void *dst_buf, void *src_buf);

/* ---- summon spell animation ---- */
int fd2_tick_summon_spell_minor_animation_state(uint32 sh, uint32 sa, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_d_3slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_e_16slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_a_6slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_b_6slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_spell_animation_state(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_spell_main_animation_state(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_spell_setup_pre_animation_8slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_c_5slot_radial(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);

/* ---- ANI decoder ---- */
void fd2_ani_decoder_decode_frame_bytes(uint16 count, uint32 src);
void fd2_ani_decoder_chunk_palette_fill_byte(void);
void fd2_ani_decoder_chunk_palette_load_literal(void);
void fd2_ani_decoder_chunk_palette_load_rle(void);
void fd2_ani_decoder_chunk_palette_load_run_pairs(void);
void fd2_ani_decoder_chunk_row_fill_byte(void);
void fd2_ani_decoder_chunk_row_copy_literal(void);
void fd2_ani_decoder_chunk_row_decode_rle(void);
void fd2_ani_decoder_chunk_sparse_set_byte(void);
void fd2_ani_decoder_chunk_sparse_set_run_byte(void);
void fd2_ani_decoder_chunk_sparse_copy_literal(void);

/* ---- audio / AIL ---- */
void fd2_set_bgm_track_with_fade(uint32 track_id, uint32 loop_count);
/* AIL (Miles) vendor function prototypes + per-function clobber pragmas come
 * from the rebuilt vendor header ailv3.h (generated by
 * tools/ail_extract/gen_ailv3_h.py from the FD2.LE disasm). It types the
 * handles as void* (Ghidra-confirmed: AIL dereferences them as struct
 * pointers) and carries the modify [eax ebx ecx edx] clobber set this -3s
 * build needs -- without it the compiler keeps the SFX sample offset in a
 * register AIL trashes, producing a garbage sample address and silent SFX. */
#include "ailv3.h"
void fd2_play_chapter_clear_fanfare(void);
uint32 fd2_load_dat_resource(uint32 fname, uint32 buf, uint32 idx);
void fd2_rle_blit_sprite(uint32 rle_stream, int32 dst_x, int32 dst_y,
                         uint32 dst_buf, int32 stride, uint32 palette_op);
void fd2_scroll_text_screen_up_by_lines(uint32 lines);
uint32 fd2_save_compute_checksum(uint32 buf, uint32 size);
int  fd2_load_portrait_to_cache(uint32 portrait_id, uint32 fp);
void fd2_restore_portrait_cache_from_tmp(void);

/* ---- chapter / battle init ---- */
void fd2_load_chapter_battle_data(uint32 chapter_id);
void fd2_load_chapter_portraits_and_dump_tmp(uint32 target_race_id);
void fd2_cinematic_chapter_portrait_dump_with_white_flash(uint32 target_tile_x, uint32 target_tile_y, uint32 portrait_set_id);
void fd2_restore_portrait_cache_from_tmp(void);
int  fd2_load_chapter_shop_item_ids(uint8 *out_buf);
void fd2_init_runtime_char_for_battle(uint32 char_field_idx, uint32 fdicon_fp);
void fd2_init_runtime_char_from_base_growth(uint32 char_id);
void fd2_load_chapter_background_layers(void);
void fd2_play_palette_fade_in(void);
void fd2_play_palette_fade_to_black(void);
void fd2_play_ani_file_animation_sequence(uint32 anim_idx, uint32 per_frame_delay,
                                          uint32 skip_on_key_flag);
void fd2_load_and_fade_in_cinematic_image(uint32 anim_idx, uint32 per_frame_delay,
                                          uint32 palette_idx);
void fd2_render_chapter_status_panel_segments(uint32 panel_sheet, uint32 active_idx,
                                              uint32 menu_options);
void fd2_init_battle_state_for_chapter(void);
void fd2_save_runtime_char_to_template(void);
void fd2_restore_all_chars_full_hp_mp(void);
void fd2_chapter_01_end(void);
void fd2_chapter_02_end(void);
void fd2_chapter_03_end(void);
void fd2_chapter_04_end(void);
void fd2_chapter_05_end(void);
void fd2_chapter_06_end(void);
void fd2_chapter_07_end(void);
void fd2_chapter_08_end(void);
void fd2_chapter_09_end(void);
void fd2_chapter_10_end(void);
void fd2_chapter_11_end(void);
void fd2_chapter_12_end(void);
void fd2_chapter_13_end(void);
void fd2_chapter_14_end(void);
void fd2_chapter_15_end(void);
void fd2_chapter_16_end(void);
void fd2_chapter_17_end(void);
void fd2_chapter_18_end(void);
void fd2_chapter_19_end(void);

/* ---- lifecycle / main menu ---- */
void fd2_play_chapter_clear_fanfare(void);
void fd2_play_chapter_21_hidden_stage_unlock_cinematic(void);
int fd2_title_attract_and_main_menu(void);
void fd2_play_game_ending_cinematic(void);
void fd2_play_final_chapter_30_ending(void);
void fd2_show_portrait_dialog_with_input(uint32 dialog_text_id, uint32 portrait_id);
int fd2_main_menu_dispatcher(void);
void fd2_save_crypt_buffer(uint32 buf, uint32 size);
int fd2_save_slot_selector_ui(uint32 buf, uint32 mode);
void fd2_render_save_slot_grid(uint32 highlight_slot, uint32 surface_offset, uint8 *sav_decrypted_buf);
void fd2_close_intro_dialog_with_slide_out(void);
int fd2_chapter_transition_menu(void);
int fd2_chapter_transition_with_intro(void);
void fd2_render_chapter_intro_overlay(void);
void fd2_load_save_and_init_engine(void);

/* ---- dialog / UI screens ---- */
uint32 fd2_display_dialog_scene(uint32 text_base, uint32 page_idx, uint32 render_pos, uint32 render_pitch, uint32 glyph_fill_color, uint32 glyph_outline_color, uint32 glyph_bg_color, uint32 glyph_height, uint32 blink_flag);
void fd2_blit_glyph_2bpp_with_outline(uint32 font_data, uint32 glyph_idx, uint32 dst_buf, uint32 pitch, uint32 fill_color, uint32 outline_color, uint16 bg_color);
uint32 fd2_play_dialog_open_animation(uint32 pos_x, uint32 pos_y, uint32 dst_origin);
void fd2_assemble_dialog_frame_layered(uint32 dst, uint32 pitch, uint32 col_offset, int row_offset, int n_cols, int n_rows);
void fd2_scroll_portrait_dialog_text_up_one_line(void);
void fd2_dialog_sprite_blit_normal(uint32 dst, uint32 sprite_hdr, uint32 stride);
void fd2_dialog_sprite_blit_mirrored(uint32 dst, uint32 sprite_hdr, uint32 stride);
void fd2_close_dialog_panels_then_slide_out_to_cursor(uint32 anim_handle, uint32 slot_offset);
void fd2_portrait_blink_animation_step(void);
void fd2_dialog_open_speaker_portrait(uint32 portrait_id);
void fd2_close_status_screen_with_slide_out(void);
void fd2_show_portrait_dialog_with_input(uint32 portrait_id, uint32 text_idx);
int fd2_party_roster_single_select_loop(void);
void fd2_run_status_screen_member_menu(void);

/* ---- input / timing ---- */
int fd2_check_keyboard_buffer_nonempty(void);
void fd2_clear_keyboard_buffer(void);
uint16 fd2_read_bios_midnight_tick(void);
void fd2_wait_one_bios_tick(void);
void fd2_wait_n_bios_ticks(uint32 n_ticks);
int fd2_wait_for_input_with_idle(void);
int fd2_wait_for_input_dialog_with_blink(uint32 animated_cursor_mode);
void fd2_wait_ticks_or_keypress_with_palette(uint32 max_ticks);
int fd2_wait_for_input_v2(void);
int fd2_wait_for_action_target_input(int mode, uint32 n_options, uint8 *pTarget_array);
/* forward decl — gfx stubs */
void fd2_blit_sprite_raw_with_header(uint32 dst, uint32 sprite_hdr, uint32 stride);
void fd2_blit_sheet_sprite_at_offset(uint32 dst, uint32 dst_pitch, uint32 sheet, uint32 sprite_idx);
void fd2_blit_indexed_sprite_rle(uint32 dst, uint32 dst_pitch, uint32 sheet, uint32 sprite_idx);
void fd2_fill_screen_rect_with_byte(uint32 x, uint32 y, uint32 color, uint32 size);
void fd2_paint_portrait_to_dialog_area(uint32 frame);
void fd2_render_horizontal_bar_segments(uint32 dst_offset, uint32 dst_pitch, uint32 filled_count, uint32 sprite_base);
void fd2_render_chapter_status_panel_segments(uint32 sheet, uint32 active_idx, uint32 segment_count);
void fd2_composite_battle_tile_map(uint32 dst_buf, uint32 dst_stride, uint32 n_cols, uint32 n_rows, uint32 win_origin_x, uint32 win_origin_y);
void fd2_composite_chars_with_spell_effect_overlay(uint32 dst_buf, uint32 n_targets, uint32 target_array, int fx_sprite_idx);
void fd2_paint_cursor_overlay_pattern(void);
void fd2_blit_24x24_at_window_relative_pos(uint32 world_x, uint32 world_y, uint32 sprite_idx);
void fd2_blit_24x24_tile_to_battle_grid_position(uint32 atlas_base, uint32 tile_index, uint32 dst_buffer, uint32 row_stride, uint32 dst_x, uint32 dst_y);
void fd2_tile_blit_24x24_passthrough(uint32 src, uint32 dst, uint32 stride);
void *fd2_convert_battle_tiles_to_24px(void);
void fd2_blit_scaled_tile_map_view(uint32 src_cx, uint32 src_cy, uint32 scale, uint32 tile_data_table);
void fd2_open_tactical_overview_zoom(void);
void fd2_tile_blit_24x24_dimmed_grayscale(uint32 src, uint32 dst, uint32 stride);
void fd2_tile_blit_24x24_with_remap_table(uint32 src, uint32 dst, uint32 stride, uint32 remap_table);
void fd2_rle_blit_with_palette_remap(uint16 *rle_stream, int32 dst_x, int32 dst_y,
                                     int32 dst_buf, int32 stride, int32 palette_remap);
void fd2_tile_blit_24x24_solid_color(uint32 src, uint32 dst, uint32 color_or_stride, uint32 unused_color);
void fd2_tile_blit_24x24_with_tint_offset(uint32 rle_stream, uint32 dst_buf, uint32 stride, uint32 color_base, uint32 tint_offset);
void fd2_tile_blit_24x24_remap(uint32 rle_stream, uint32 dst_buf, uint32 stride, uint32 palette_remap);
void fd2_tile_blit_24x24_with_dialog_bg_fill(uint32 rle_stream, uint32 dst_buf, uint32 stride);
void fd2_composite_all_chars_overlay(void);
void fd2_paint_char_sprite_at_world_pos(uint32 char_idx);
void fd2_redraw_terrain_tiles_under_chars(void);
void fd2_blit_animated_tile_at_pos(uint32 buf, int32 tile_x, int32 tile_y);
void fd2_blit_scaled_tile_map_view(uint32 src_cx_fp, uint32 src_cy_fp, uint32 scale, uint32 tile_data_table);
void fd2_blit_scaled_chapter_pose(uint32 src_cx, uint32 src_cy, uint32 src_bitmap, int32 scale_fp_step);
void fd2_blit_24x24_tile_to_battle_grid_position(uint32 atlas_base, uint32 tile_index, uint32 dst_buffer, uint32 dst_row_stride, uint32 dst_x, uint32 dst_y);
void fd2_render_terrain_info_hud_panel(uint32 buf, uint32 stride);
void fd2_blit_rectangle(uint32 dst, uint32 dstride, uint32 src, uint32 sstride, uint32 w, uint32 h);
void fd2_render_circle_anim_row(int cx, int cy, int r, int scale_num, int start_row, int end_row, uint8 *palette_remap_src);
void fd2_render_filled_circle_band_anim(uint32 col_center, uint32 bottom_row, uint32 radius_factor, int top_row, int row_loop_end, int palette_remap_src);
void fd2_blit_money_digit_sprite(uint32 dst_buf, uint32 dst_stride, uint32 sprite_idx);
uint32 fd2_alloc_and_blit_indexed_sprite_chunk(uint32 sheet_base, uint32 dst, uint32 surface_pitch, uint32 col_offset, uint32 row_idx, uint32 sprite_idx);
void fd2_blit_money_digit_sprite(uint32 dst_buf, uint32 dst_stride, uint32 sprite_idx);
void fd2_render_decimal_number_to_buffer(uint32 dst, uint32 stride, uint32 value, uint32 x, uint32 digits);
void fd2_render_hp_or_mp_bar_proportional(uint32 dst_off, uint32 pitch, uint32 sprite_base, uint32 current, uint32 max);
void fd2_render_number_red_when_full(uint32 dst_off, uint32 pitch, uint32 current, uint32 max, uint32 digits);
void fd2_render_signed_modifier_with_icon(uint32 dst, uint32 stride, int32 modifier);
void fd2_cleanup_dialog_sprite_buffer(uint32 saved_block, uint32 dst, uint32 stride);
void fd2_restore_screen_block_from_buffer(uint32 saved_block, uint32 dst, uint32 stride);
void fd2_restore_block_loop(uint32 dst, uint32 src, uint32 stride);
void fd2_save_block_loop(uint32 src, uint32 dst, uint32 stride);
void fd2_repaint_settings_dialog_borders(uint32 menu_options, uint32 menu_state);
int fd2_wait_input_with_dialog_repaint(uint32 menu_state, uint32 pSlot_disable_arr);
void fd2_wait_input_with_status_panel_repaint(uint32 char_idx);
void fd2_render_recruitment_party_screen(void);
void fd2_render_chapter_dialog_borders(void);
void fd2_render_chapter_intro_dialog_panels(uint32 corner_offs_ptr, uint32 mode);
void fd2_render_shop_item_grid(uint32 item_count, uint8 *item_id_array, uint32 highlight_slot, int32 surface_offset, int32 sell_mode_flag);
void fd2_render_party_roster_grid(uint32 highlight_idx, uint32 surface_offset);
void fd2_render_party_roster_with_item_stat_preview(uint32 candidate_count, uint32 candidate_array_ptr, uint32 item_id, int32 highlight_idx, int32 surface_offset);
void fd2_render_promote_members_grid(uint32 candidate_count, uint32 surface_offset, uint32 highlight_idx, uint8 *candidate_idx_list);
void fd2_render_promote_candidates_grid(uint32 candidate_count, uint32 surface_offset, uint32 highlight_idx, uint8 *candidate_idx_list, uint8 *promotion_target_list);
void fd2_blit_sprite_with_stride_setup(uint32 dst, uint32 sprite_hdr, uint32 stride);
void fd2_blit_sprite_with_stride_loop(uint32 dst_buf, uint32 sprite_hdr);
void fd2_blit_sprite_scaled_with_skip(uint32 sprite_hdr, uint32 dst_buf, uint32 stride, uint32 scale_num, uint32 scale_den);
void fd2_blit_buffer_with_per_row_offset(uint32 src_buf, uint32 *dst_buf, uint32 offset_idx);
void fd2_backup_dialog_area_to_buffer(void);
void fd2_restore_dialog_area_from_buffer(void);
void fd2_blit_sprite_with_decoded_pixels(uint32 dst, uint32 sprite_hdr, uint32 stride);
uint16 fd2_decode_dialog_pixel_byte(uint16 state, uint8 **stream);
void fd2_save_screen_block_to_buffer(uint32 out_buf, uint32 width, uint32 height,
                                     uint32 src_base, uint32 src_offset, uint32 stride);
void *fd2_blit_indexed_sprite_with_alloc(uint32 sprite_hdr, uint32 dst,
                                         uint32 dst_pitch, uint32 sheet_base,
                                         uint32 sprite_idx);
int fd2_wait_input_with_chapter_dialog_blink(uint32 mode);
int fd2_chapter_intro_menu_input_loop(void);
int fd2_party_roster_single_select_loop(void);
int fd2_party_roster_class_select_loop(uint32 candidate_count, uint32 candidate_array_ptr, uint32 item_id);
void fd2_animate_scroll_up_in_shop_dialog(void);
void fd2_animate_scroll_down_in_shop_dialog(void);
uint32 fd2_run_chapter_intro_menu_main(uint32 pose_bitmap);
uint32 fd2_run_chapter_intro_menu_typeB(uint32 snapshot_buf);
uint32 fd2_run_chapter_intro_menu_typeC(uint32 pose_bitmap);
void fd2_run_status_screen_member_menu(void);
void fd2_run_revive_menu_main(void);
void fd2_run_class_promotion_menu_main(void);
void fd2_save_current_state_to_slot(uint32 prompt_flag);
void fd2_load_state_from_selected_slot(void);
void fd2_animate_chapter_intro_dialog_wings(uint32 open_or_close);
int fd2_load_chapter_shop_item_ids(uint8 *out_buf);
void fd2_run_sell_item_menu(void);
void fd2_run_equip_member_menu(void);
void fd2_run_give_item_menu(void);
void fd2_blit_scaled_chapter_pose(uint32 src_cx, uint32 src_cy,
                                  uint32 src_bitmap, int32 scale_fp_step);
int fd2_wait_input_with_recruitment_repaint(uint32 p1, uint32 p2, uint32 p3, uint32 p4);
int fd2_count_selected_chars(uint32 sel_state);
void fd2_reorder_party_by_selection(uint32 sel_state);
void fd2_pin_required_char_to_party_slot1(uint32 char_id);
void fd2_render_recruitment_select_screen(uint32 a, uint32 b, uint32 c, uint32 d);
void fd2_render_battle_scene_with_portrait_grid_layout(uint32 tile_atlas_base, uint32 src_framebuffer, uint32 chapter_id, uint8 *player_char_id_array, int32 enemy_count, uint8 *enemy_id_array, int32 reserved_char_pos);
int fd2_check_char_is_dead(uint32 char_idx);
int fd2_scan_chars_within_manhattan_range(uint32 x, uint32 y, uint32 range, uint32 flag, int mode);

/* ---- ui_menu / revive + promote ---- */
void fd2_run_revive_menu_main(void);
void fd2_run_class_promotion_menu_main(void);
int  fd2_run_recruitment_or_branch_screen(void);
int  fd2_count_selected_chars(uint32 selection_state);
void fd2_reorder_party_by_selection(uint32 selection_state);
char fd2_require_char_id_in_active_party(uint32 active_party_cap, uint32 char_id);
void fd2_pin_required_char_to_party_slot1(uint32 char_id);
uint8 fd2_build_promotion_candidates_with_targets(uint8 *out_chars, uint8 *out_targets);
int fd2_find_inventory_slot_with_item(uint32 char_idx, uint32 item_id);
void fd2_execute_class_promotion_with_dialog(uint32 char_idx);
int fd2_build_dead_chars_list_for_revive(uint8 *out_list_buf);
int fd2_revive_member_select_loop(uint32 candidate_count, uint8 *candidate_idx_list);
int fd2_promote_member_select_loop(int char_count, void *char_list_ptr, void *target_class_list_ptr);
void fd2_animate_scroll_down_in_shop_dialog(void);
void fd2_animate_scroll_up_in_shop_dialog(void);
void fd2_animate_money_decrement(uint32 amount);
void fd2_animate_shop_transaction_feedback(void);

/* ---- util / misc ---- */
void fd2_debug_print_ans_and_length(int value);
uint32 fd2_ail_set_alloc_fnptr(uint32 new_fnptr);
uint32 fd2_ail_install_free_fnptr(uint32 new_free_fnptr);
void fd2_noop_stub_4e915(void);
void fd2_delay_400ms(void);

/* ---- crt thunks ---- */
void fd2_delay_ms(uint32 ticks);

/* ---- crt_equivalent (FD2-specific CRT helpers; src/crt/crt.c) ---- */
int crt_equivalent_lx_chunk_read(int file_handle, int offset,
                                       uint8 mode, void *dest, uint32 length);
int crt_equivalent_lx_header_reader(char *path_or_base, uint8 mode_byte);
void *crt_equivalent_lx_module_loader(char *path, int flags,
                                            void *caller_buf);
void crt_equivalent_atexit_default_stub(void);

/* crt_equivalent_get_eflags @ 0x3ed58 — the 4-byte Watcom `_disable`
 * primitive the thunk @ 0x37f86 JMPs into (PUSHFD; POP EAX; CLI; RET).
 * Returns the prior EFLAGS in EAX and disables interrupts (CLI). Real
 * out-of-line function owning its own PUBDEF; the raw asm body is spliced
 * in from the same in-line #pragma aux helper as the thunk in
 * src/crt/crt.c. */
unsigned long crt_equivalent_get_eflags(void);

/* crt_equivalent_get_eflags_thunk @ 0x37f86 — Watcom `_disable` primitive
 * reached by the two AIL ISRs via near CALL. Returns the prior EFLAGS in
 * EAX and disables interrupts (CLI). Real out-of-line function; the raw
 * asm body is spliced in from an in-line #pragma aux helper in
 * src/crt/crt.c. */
unsigned long crt_equivalent_get_eflags_thunk(void);

/* crt_equivalent_fpe_default_handler @ 0x3d26e — SIGFPE / FPU-exception
 * default no-op handler (1-byte RET). Seeds the FPE dispatch slot @ 0x5283c;
 * invoked indirectly by __FPE_exception_ / __int7 when signal(SIGFPE, ...) was
 * never set. Address-taken (referenced as DATA from the slot), so it is a real
 * callable function. __cdecl void(int fpe_code); ignores the code and returns. */
void crt_equivalent_fpe_default_handler(int fpe_code);

/* crt_equivalent_matherr_default_thunk @ 0x4d340 — default value of the
 * user-matherr-handler slot @ 0x539A8. Read+CALLed by _matherr (PUSH exc;
 * CALL [0x539A8]; ADD ESP,4) before any diagnostic output; returns 0 ("not
 * handled") so _matherr runs its default path. Address-taken (the slot's
 * default contents, written by _set_matherr), so it is a real callable
 * function. Real out-of-line body tail-JMPs to the separate emit target
 * crt_equivalent_matherr_default_return_zero @ 0x4d8ea via an in-line
 * #pragma aux helper in src/crt/crt.c. __cdecl int(void *exc); ignores exc
 * and returns 0. */
int crt_equivalent_matherr_default_thunk(void *exc);

/* ---- util / dpmi ---- */
int fd2_dpmi_alloc_dos_memory(uint32 paragraphs, uint32 *out_linear, uint32 *out_segment, uint32 *out_selector);
void fd2_dpmi_free_dos_memory(uint32 linear_unused, uint32 segment_unused, uint32 selector);
int fd2_dpmi_lock_region(uint32 region_start, uint32 region_end);
int fd2_dpmi_unlock_region(uint32 page_start, uint32 page_end);
int fd2_dpmi_lock_size(uint32 base, uint32 size);
int fd2_dpmi_unlock_size(uint32 base, uint32 size);

#endif /* PROTOS_H */
