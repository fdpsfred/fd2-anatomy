#ifndef PROTOS_H
#define PROTOS_H

#include "types.h"

/*
 * FD2 game function prototypes.
 * Populated incrementally during emission.
 * Convention: ~95% are __cdecl (Watcom -3s default).
 */

/* ---- lifecycle ---- */
void fd2_main(void);

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
int fd2_find_char_at_cursor_pos(void);
int fd2_find_char_by_id_or_template(uint32 char_id);
void fd2_mark_char_acted_this_turn(uint32 char_idx);
void fd2_check_all_player_acted_or_asleep(void);
void fd2_check_tile_event_post_action(uint32 x, uint32 y, uint32 arg);
void fd2_mark_char_as_dead(uint32 char_idx);
void fd2_set_chapter_init_done_flag(void);
void fd2_set_battle_anim_phase_to_1(void);
void fd2_set_combat_aux_block_byte_d_low4_for_char_range(uint32 start_idx, uint32 end_idx, uint32 new_val);
void fd2_check_battle_end_condition(void);
void fd2_check_battle_end_default_handler(uint32 event_arg);
int fd2_collect_dead_char_drops(uint32 out_buffer);
/* forward decl — defined in later phases */
uint32 fd2_find_equipped_item_by_kind(uint32 char_idx, uint32 kind);
uint8 fd2_get_inventory_slot_item_id(uint32 char_idx, uint32 slot);
void fd2_delay_ticks(uint32 ticks);
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
void fd2_cast_spell_11_stage_a(int, int, uint8 *);
void fd2_cast_spell_12_stage_b(int, int, uint8 *);
void fd2_cast_spell_13_stage_c(uint32, uint32, uint8 *);
void fd2_cast_spell_14_dispatch_aa8(int, int, uint8 *);
void fd2_cast_spell_15_dispatch_aa8(int, int, uint8 *);
void fd2_cast_spell_16_dispatch_cda(int, int, uint8 *);
void fd2_spell_handler_id_26_via_status_d1b_effect_25(int, int, int);
void fd2_spell_handler_id_27_via_status_d1b_effect_26(int, int, int);
void fd2_cast_spell_17_complex(uint32, uint32, uint32);
/* spell worker forward decls */
void fd2_execute_offensive_targeted_spell(int, int, int, int);
void fd2_execute_offensive_full_screen_flash_spell(int, int, int, int);
void fd2_cast_earthquake_spell_with_screen_shake(int, int, int, uint8 *);
void fd2_dispatch_variant_b_cast(int, int, int, int);
void fd2_apply_status_effect_with_anim(int, int, int, int, int);
void fd2_cast_status_spell_via_d1b(int, int, int, int, int);
void fd2_cast_ap_boost_spell(int, int, uint8 *);
void fd2_cast_dp_boost_spell(int, int, uint32);
void fd2_cast_speed_boost_spell(uint32, uint32, uint32);
void fd2_animate_spell_impact_per_target(uint32, uint32, uint32, uint32);
void fd2_animate_status_effect_overlay_flicker(uint32, uint32, uint32, uint32);
void fd2_animate_spell_full_screen_flash(uint32, uint32, uint32, uint32);
void fd2_animate_spell_overlay_blink(uint32, uint32, uint32, uint32);
void fd2_show_damage_number(uint32 val, uint32 type, uint32 target);
void fd2_show_miss_indicator(uint32 target);
void fd2_show_status_effect_overlay(uint32 target, uint32 spell_id);
void fd2_animate_spell_projectile_paths(void);
void fd2_remove_inventory_slot_at(uint32 char_idx, uint32 slot);
void fd2_apply_item_stat_modifier_with_anim(uint32, uint32, uint32, uint32, uint32, uint32, uint32);
void fd2_apply_attack_spell_damage(uint32, uint32, uint32, uint32);
void fd2_apply_use_effect_dispatch(uint32, uint32, uint32, uint32);
void fd2_load_status_effect_sfx(void);
void fd2_play_and_free_status_effect_sfx(void);
int fd2_collect_pending_death_drops(uint32 out_buffer);
void fd2_play_death_animation_and_mark_dead(void);
void fd2_process_battle_drop_entries(uint32, uint32, uint32);
void fd2_cast_group_hp_heal_spell(uint32, uint32, uint32, uint32);
void fd2_cast_status_cure_spell(uint32, uint32, uint32, uint32, uint32);
void fd2_cast_status_inflict_spell(uint32, uint32, uint32, uint32, uint32);
void fd2_play_sfx_with_handle(uint32, int, int);
void fd2_play_rising_pre_cast_effect(int, int, int);
void fd2_play_variant_b_slide_pre_effect(int, int);
void fd2_animate_warp_teleport_char(uint32, uint32, uint32, uint32, uint32);

/* ---- cursor + pan ---- */
void fd2_cursor_move_up(void);
void fd2_cursor_move_down(void);
void fd2_cursor_move_right(void);
void fd2_cursor_move_left(void);
void fd2_pan_cursor_to_tile_animated(int target_x, int target_y);
void fd2_pan_cursor_to_char(uint32 char_idx);
void fd2_pan_cursor_and_window(uint32 target_ox, uint32 target_oy);
void fd2_composite_battle_frame(int ctx);

/* ---- graphics / palette ---- */
void fd2_set_vga_palette_range(uint32 start_idx, uint32 end_idx, uint32 brightness_subtract);
void fd2_set_vga_palette_range_with_add(uint32 start_idx, uint32 end_idx, uint32 brightness_add);
void fd2_set_full_vga_palette_to_color(uint32 r, uint32 g, uint32 b);
void fd2_palette_overbright_settle_step_loop(uint32 start_intensity, uint32 step_delay_ms);
void fd2_interpolate_palette_range_toward_color(uint32 start_idx, uint32 end_idx, uint32 blend, uint32 target_r, uint32 target_g, uint32 target_b);
void fd2_apply_palette_remap_run(uint32 remap_table, uint32 byte_count, uint8 *buf);
void fd2_tick_chapter_palette_animation(void);
void fd2_update_palette_cycle_anim(void);

/* ---- ui_menu / status ---- */
void fd2_compute_equipped_stats_with_item_preview(uint32 char_idx, uint32 item_id, uint32 stats_out_ptr);

/* ---- animation ---- */
void fd2_tick_tile_event_animations(void);
void fd2_walk_step_down(uint32 char_idx);
void fd2_walk_step_left(uint32 char_idx);
void fd2_walk_step_up(uint32 char_idx);
void fd2_walk_step_right(uint32 char_idx);
void fd2_walk_path_animation_loop(uint32 char_idx, uint32 path_buf, uint32 step_count);
void fd2_tick_tutorial_progress_with_sfx(uint32 char_idx);
void fd2_slide_panel_step_left_main(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_step_right_main(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_step_top_small(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_step_bottom_main(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_step_bottom_small(uint32 src_buffer, uint32 frame_idx);
void fd2_slide_panel_up_partial_step(uint32 y_offset, uint32 dst_workspace, uint32 src_buffer);
void fd2_slide_panel_down_step(uint32 y_offset, uint32 dst_workspace, uint32 src_buffer);
void fd2_tick_sprite_animation_step(uint8 *p_frame_idx, uint8 *p_tick, int x, int y, uint32 atlas);
void fd2_blit_indexed_sprite(uint32 atlas, uint32 frame_idx, int x, int y, int mode);
void fd2_ani_decoder_set_target_buffer(uint16 width, uint32 dst_buf, uint32 src_buf);
int fd2_tally_chars_with_zero_at_field(int len, uint32 char_idx_arr, int field_offset, int weight);
int fd2_find_tile_with_attribute_match(uint32 target_tag, uint32 out_pos);
int fd2_collect_unmarked_tile_positions(uint32 out_buf);
uint8 fd2_pathfind_count_unique_directions(void);
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
int fd2_execute_ai_physical_attack(uint32 char_idx, uint32 team);
uint32 fd2_animate_combat_speech_bubbles(uint32 ci, uint32 ti);
void fd2_render_combatant_hp_bar_proportional(uint32 d, uint32 s, uint32 ci, uint32 st);
int fd2_animate_combat_hit_with_hp_drain(uint32 a, uint32 d, uint32 st);
void fd2_render_combat_combatant_panels(uint32 st, uint32 a, uint32 d);
void fd2_play_full_combat_cinematic(uint32 a, uint32 d);
void fd2_process_xp_and_level_up_for_char(uint32 ci);
int fd2_count_usable_inventory_slots(uint32 ci);
int fd2_build_usable_spell_list(uint32 ci, uint32 buf);
int fd2_score_spell_candidate(uint32 si, uint32 nt, uint32 tb);
void fd2_execute_ai_item_use(uint32 char_idx, uint32 ctx);
void fd2_play_figani_char_intro_animation(uint32 char_idx);
void fd2_apply_use_effect_dispatch(uint32 ci, uint32 sl, uint32 n, uint32 buf);
void fd2_clear_all_chars_facing(void);
void fd2_add_item_to_inventory(uint32 char_idx, uint32 item_id);
void fd2_play_sfx_sample_from_bank(uint32 bank_ptr, uint32 sfx_id, uint32 p);
void fd2_paint_char_sprite_at_world_with_mode(uint32 ws, uint32 stride, uint32 ci, uint32 mode, uint32 color);
void fd2_paint_threat_overlay_for_team(uint32 ctx);
int fd2_pathfind_to_destination(uint32 ct, uint32 sx, uint32 sy, uint32 ms,
    uint32 db, uint32 f1, uint32 f2, uint32 md, uint32 tm, uint32 af);
void fd2_obfuscate_battle_tile_map(uint32 tile_map);
void fd2_init_movement_range_floodfill(uint32 ct, uint32 x, uint32 y,
    uint32 rng, uint32 tm, uint32 af);
int fd2_compute_aoe_targets(uint32 cx, uint32 cy, uint32 buf,
    uint32 range, uint32 aoe_r, uint32 team_f);

/* ---- battle turn / status ---- */
void fd2_tick_status_effects_and_show_messages(uint32 team);

/* ---- summon spell animation ---- */
int fd2_tick_summon_spell_minor_animation_state(uint32 sh, uint32 sa, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_d_3slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_e_16slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_a_6slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_anim_variant_b_6slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_spell_animation_state(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_spell_main_animation_state(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);
int fd2_tick_summon_spell_setup_pre_animation_8slot(uint32 ci, uint32 sh, uint32 oy, uint32 rs, uint32 sc);

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
void AIL_set_sequence_volume(uint32 seq, int target, int ramp_ms);
void AIL_stop_sequence(uint32 seq);
int  AIL_init_sequence(uint32 seq, uint32 xmi_data, int seq_idx);
void AIL_start_sequence(uint32 seq);
void AIL_set_sequence_loop_count(uint32 seq, uint32 count);
uint32 fd2_load_dat_resource(uint32 fname, uint32 buf, uint32 idx);

/* ---- chapter / battle init ---- */
void fd2_load_chapter_battle_data(uint32 chapter_id);
void fd2_play_palette_fade_in(void);
void fd2_play_palette_fade_to_black(void);
void fd2_init_battle_state_for_chapter(void);
void fd2_save_runtime_char_to_template(void);

/* ---- lifecycle / main menu ---- */
int fd2_play_ending_and_record_clear(void);
int fd2_main_menu_continue_dispatcher(void);
void fd2_save_crypt_buffer(uint32 buf, uint32 size);
int fd2_save_slot_selector_ui(uint32 buf, uint32 mode);
void fd2_close_intro_dialog_with_slide_out(void);
int fd2_chapter_transition_menu(void);
void fd2_load_save_and_init_engine(void);

/* ---- dialog / UI screens ---- */
void fd2_display_dialog_scene(uint32 txt, uint32 id, uint32 pb, uint32 w, uint32 p5, uint32 p6, uint32 p7, uint32 p8, uint32 p9);
void fd2_load_chapter_portrait(uint32 portrait_id);
void fd2_close_status_screen_with_slide_out(void);

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
void fd2_blit_sheet_sprite_at_offset(uint32 dst, uint32 stride, uint32 sheet_ptr, uint32 idx);
void fd2_paint_portrait_to_dialog_area(uint32 frame);
void fd2_composite_battle_tile_map(uint32 dst, uint32 stride, uint32 w, uint32 h, uint32 ox, uint32 oy);
void fd2_paint_cursor_overlay_pattern(void);
void fd2_composite_all_chars_overlay(void);
void fd2_render_terrain_info_hud_panel(uint32 buf, uint32 stride);
void fd2_blit_rectangle(uint32 dst, uint32 dstride, uint32 src, uint32 sstride, uint32 w, uint32 h);
void fd2_repaint_settings_dialog_borders(uint32 state, uint32 arr);
int fd2_wait_input_with_dialog_repaint(uint32 menu_state, uint32 pSlot_disable_arr);
void fd2_wait_input_with_status_panel_repaint(uint32 char_idx);
void fd2_render_recruitment_party_screen(void);
void fd2_render_chapter_dialog_borders(void);
void fd2_render_chapter_intro_dialog_panels(uint32 corner_offs_ptr, uint32 mode);
void fd2_blit_sprite_with_stride_setup(uint32 dst, uint32 sprite, uint32 stride);
int fd2_wait_input_with_chapter_dialog_blink(uint32 mode);
int fd2_wait_input_with_recruitment_repaint(uint32 p1, uint32 p2, uint32 p3, uint32 p4);
void fd2_render_recruitment_select_screen(uint32 a, uint32 b, uint32 c, uint32 d);
int fd2_check_char_is_dead(uint32 char_idx);
int fd2_scan_chars_within_manhattan_range(uint32 x, uint32 y, uint32 range, uint32 flag, int mode);

/* ---- util / misc ---- */
void fd2_debug_print_ans_and_length(int value);
uint32 fd2_set_word_global_52758(uint32 new_val);
uint32 fd2_set_word_global_5275c(uint32 new_val);

/* ---- util / dpmi ---- */
int fd2_dpmi_alloc_dos_memory(uint32 paragraphs, uint32 *out_linear, uint32 *out_segment, uint32 *out_selector);
void fd2_dpmi_free_dos_memory(uint32 selector);
int fd2_dpmi_lock_region(uint32 page_start, uint32 page_end);
int fd2_dpmi_unlock_region(uint32 page_start, uint32 page_end);
int fd2_dpmi_lock_size(uint32 base, uint32 size);
int fd2_dpmi_unlock_size(uint32 base, uint32 size);

#endif /* PROTOS_H */
