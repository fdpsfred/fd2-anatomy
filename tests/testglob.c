/*
 * testglob.c — Fake global definitions for all test suites
 */
#include "types.h"
#include "globals.h"

runtime_char  g_test_rc_array[8];
runtime_char *data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
item_effect       data_fd2_battle_item_effect_table[215];
spell_effect      data_fd2_battle_spell_effect_table[36];
enemy_data        data_fd2_battle_enemy_data_table[68];
character_base    data_fd2_battle_character_base_table[32];
character_growth  data_fd2_battle_character_growth_table[68];
uint8  data_fd2_chapter_intro_metadata_table[26 * 31];
uint8  data_fd2_spell_learning_table[20 * 12];
uint8  data_fd2_class_promotion_data_table[20 * 2];
uint8  data_fd2_movement_cost_table[27 * 20];
uint8  data_fd2_job_allowed_items_table[27 * 7];
void  *data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21[21];
void  *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];
uint32 data_fd2_battle_pending_xp_credit = 0;
uint8  data_fd2_battle_last_hit_or_miss_flag = 1;
uint16 data_fd2_shared_rng_seed = 0;
uint32 data_fd2_battle_job_magic_resist_table[27];
uint8  data_fd2_battle_job_crit_rate_table[27];
uint32 data_fd2_save_load_save_slot_id = 0;
uint32 data_fd2_runtime_battle_state_ptr = 0;
uint32 data_fd2_menu_dialog_state_handle = 0;
uint32 data_fd2_tile_anim_table_base = 0;
uint32 data_fd2_chinese_font_sheet = 0;
uint8  data_fd2_ui_terrain_hud_user_enabled = 0;
uint8  data_fd2_audio_sfx_driver_available_flag = 0;
uint8  data_fd2_audio_sfx_enabled_flag = 0;
char   data_fd2_string_resource_filename_fdtxt_dat[] = "FDTXT.DAT";
char   data_fd2_string_resource_filename_fdother_dat[] = "FDOTHER.DAT";
uint32 data_fd2_shared_party_total_gold = 0;
uint32 data_fd2_shared_menu_party_roster_buffer_ptr = 0;
uint32 data_fd2_shared_menu_party_member_count = 0;
uint32 data_fd2_battle_anim_phase = 0;
uint32 data_fd2_battle_ai_post_action_consequence_idx = 0;
uint32 data_fd2_chapter_current_chapter_id = 1;
uint32 data_fd2_chapter_event_or_battle_end_code = 0;
uint32 data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
uint32 data_fd2_battle_teleport_dest_world_x = 0;
uint32 data_fd2_battle_teleport_dest_world_y = 0;
uint32 data_fd2_audio_status_effect_sfx_handle_ptr = 0;
uint32 data_fd2_battle_tile_attr_mv_modifier_table[32];
uint32 data_fd2_battle_tile_attr_def_modifier_table[32];
uint32 data_fd2_vga_palette_data_ptr = 0;
uint32 data_fd2_all_game_text_ptr = 0;
uint32 data_fd2_battle_tile_map_ptr = 0;
uint32 data_fd2_battle_party_member_count = 4;
uint32 data_fd2_tile_attribute_flags_buffer_ptr = 0;
uint32 data_fd2_tile_event_data_table_ptr = 0;
uint8  data_fd2_audio_bgm_last_set_track_id = 0xFF;
uint8  data_fd2_audio_bgm_enabled_flag = 1;
uint8  data_fd2_audio_per_chapter_player_turn_bgm_track[30] = {0};
uint8  data_fd2_audio_per_chapter_enemy_turn_bgm_track[30] = {0};
uint8  data_fd2_audio_bgm_driver_available_flag = 1;
uint32 data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
char   data_fd2_string_fdmus_dat[] = "FDMUS.DAT";
uint32 data_fd2_audio_bgm_sequence_handle = 0;
uint32 data_fd2_resource_last_loaded_resource_size = 0;
uint32 data_fd2_audio_fdother_sfx_bank_buf_ptr = 0;
uint32 data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
uint32 data_fd2_battle_view_window_origin_x = 0;
uint32 data_fd2_battle_view_window_origin_y = 0;
uint32 data_fd2_battle_cursor_world_x = 5;
uint32 data_fd2_battle_cursor_world_y = 5;
uint32 data_fd2_battle_cursor_screen_x = 5;
uint32 data_fd2_battle_cursor_screen_y = 5;
uint32 data_fd2_battle_map_width_tiles = 20;
uint32 data_fd2_battle_map_height_tiles = 15;
uint16 data_fd2_input_idle_current_bios_tick_word = 0;
uint16 data_fd2_input_idle_last_rendered_tick_word = 0;
uint8  data_fd2_input_last_key_pressed = 0;
uint8  data_fd2_input_key_input_mode = 0;
uint32 data_ail_alloc_fnptr = 0;
uint32 data_ail_free_fnptr = 0;
uint32 data_fd2_engine_wait_one_bios_tick_last_seen = 0;
uint32 data_fd2_engine_wait_n_bios_ticks_last_seen = 0;
int g_find_equipped_return = 0;
uint32 fd2_find_equipped_item_by_kind(uint32 c, uint32 k) {
    return (uint32)g_find_equipped_return;
}
void fd2_delay_ticks(uint32 t) { }
void fd2_execute_offensive_targeted_spell(int a, int b, int c, int d) { }
void fd2_execute_offensive_full_screen_flash_spell(int a, int b, int c, int d) { }
void fd2_cast_earthquake_spell_with_screen_shake(int a, int b, int c, uint8 *d) { }
void fd2_dispatch_variant_b_cast(int a, int b, int c, int d) { }
void fd2_cast_ap_boost_spell(int a, int b, uint8 *c) { }
void fd2_cast_dp_boost_spell(int a, int b, uint32 c) { }
void fd2_cast_speed_boost_spell(uint32 a, uint32 b, uint32 c) { }
int g_play_sfx_with_handle_calls = 0;
void fd2_play_sfx_with_handle(uint32 a, int b, int c) { g_play_sfx_with_handle_calls++; (void)a; (void)b; (void)c; }
void fd2_play_rising_pre_cast_effect(int a, int b, int c) { }
void fd2_play_variant_b_slide_pre_effect(int a, int b) { }
void fd2_animate_warp_teleport_char(uint32 a, uint32 b, uint32 c, uint32 d, uint32 e) { }
uint16 data_fd2_animation_palette_cycle_last_tick = 0;
uint8  data_fd2_animation_palette_cycle_frame_idx = 0;
uint8  data_fd2_animation_palette_cycle_rgb_table[93] = {0};
int fd2_check_char_is_dead(uint32 c) { return 0; }
/* fd2_scan_chars_within_manhattan_range: now in btl_ai.c */
uint32 data_fd2_ui_anim_sprite_sheet_ptr = 0;
uint32 data_fd2_dialog_last_action_value_param = 0;
uint32 data_fd2_dialog_active_portrait_blit_offset = 0;
uint32 data_fd2_dialog_current_speaker_char_ptr = 0;
uint32 data_fd2_large_game_state_buffer_ptr = 0;
uint32 data_fd2_dialog_blink_phase_oscillator = 0;
uint32 data_fd2_dialog_blink_phase_oscillator_tick_latch = 0;
void fd2_blit_sheet_sprite_at_offset(uint32 d, uint32 s, uint32 p, uint32 i) { }
uint32 data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 0;
uint32 data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;
uint32 data_fd2_graphics_chapter_ambient_palette_anim_tick_latch = 0;
void fd2_composite_battle_tile_map(uint32 d, uint32 s, uint32 w, uint32 h, uint32 ox, uint32 oy) { }
void fd2_paint_cursor_overlay_pattern(void) { }
void fd2_composite_all_chars_overlay(void) { }
void fd2_render_terrain_info_hud_panel(uint32 b, uint32 s) { }
void fd2_blit_rectangle(uint32 d, uint32 ds, uint32 s, uint32 ss, uint32 w, uint32 h) { }
void fd2_repaint_settings_dialog_borders(uint32 s, uint32 a) { }
void fd2_render_recruitment_party_screen(void) { }
uint32 data_fd2_ui_recruitment_screen_repaint_tick_latch = 0;
uint32 data_fd2_ui_slide_composed_target_buf_ptr = 0;
uint32 data_fd2_ui_menu_cursor_idx = 0;
uint32 data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[4] = {0};
uint32 data_fd2_chapter_intro_dialog_anim_frame_idx = 0;
uint32 data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
void fd2_render_chapter_dialog_borders(void) { }
void fd2_render_chapter_intro_dialog_panels(uint32 c, uint32 m) { }
void fd2_blit_sprite_with_stride_setup(uint32 d, uint32 s, uint32 st) { }
void fd2_render_recruitment_select_screen(uint32 a, uint32 b, uint32 c, uint32 d) { }
void fd2_animate_spell_impact_per_target(uint32 a, uint32 b, uint32 c, uint32 d) { }
void fd2_animate_status_effect_overlay_flicker(uint32 a, uint32 b, uint32 c, uint32 d) { }
void fd2_animate_spell_full_screen_flash(uint32 a, uint32 b, uint32 c, uint32 d) { }
void fd2_animate_spell_overlay_blink(uint32 a, uint32 b, uint32 c, uint32 d) { }
void fd2_show_damage_number(uint32 v, uint32 t, uint32 tg) { }
void fd2_show_miss_indicator(uint32 t) { }
void fd2_show_status_effect_overlay(uint32 t, uint32 s) { }
void fd2_animate_spell_projectile_paths(void) { }
void fd2_remove_inventory_slot_at(uint32 c, uint32 s) { }
void fd2_load_status_effect_sfx(void) { }
void fd2_play_and_free_status_effect_sfx(void) { }
/* fd2_collect_pending_death_drops: now in btl_turn.c */
void fd2_display_dialog_scene(uint32 a, uint32 b, uint32 c, uint32 d, uint32 e, uint32 f, uint32 g, uint32 h, uint32 ii) { }
void fd2_load_chapter_portrait(uint32 p) { }
void fd2_close_status_screen_with_slide_out(void) { }
void fd2_load_chapter_battle_data(uint32 c) { (void)c; }
void fd2_play_palette_fade_to_black(void) { }
int g_ending_menu_return = 0;
int fd2_play_ending_and_record_clear(void) { return g_ending_menu_return; }
void fd2_save_crypt_buffer(uint32 b, uint32 s) { (void)b; (void)s; }
int g_slot_selector_return = -1;
int fd2_save_slot_selector_ui(uint32 b, uint32 m) { (void)b; (void)m; return g_slot_selector_return; }
void fd2_close_intro_dialog_with_slide_out(void) { }
int g_chapter_transition_return = 0;
int fd2_chapter_transition_menu(void) { return g_chapter_transition_return; }
void fd2_load_save_and_init_engine(void) { }
int g_ail_vol_calls = 0;
int g_ail_last_vol = 0;
int g_ail_last_ramp = 0;
void AIL_set_sequence_volume(uint32 s, int t, int r) { g_ail_vol_calls++; g_ail_last_vol = t; g_ail_last_ramp = r; (void)s; }
void AIL_stop_sequence(uint32 s) { (void)s; }
int  AIL_init_sequence(uint32 s, uint32 d, int i) { (void)s; (void)d; (void)i; return 0; }
void AIL_start_sequence(uint32 s) { (void)s; }
void AIL_set_sequence_loop_count(uint32 s, uint32 c) { (void)s; (void)c; }
uint32 fd2_load_dat_resource(uint32 f, uint32 b, uint32 i) { (void)f; (void)i; return b; }
void fd2_play_palette_fade_in(void) { }
void fd2_play_death_animation_and_mark_dead(void) { }
void fd2_process_battle_drop_entries(uint32 a, uint32 b, uint32 c) { }
void fd2_cast_group_hp_heal_spell(uint32 a, uint32 b, uint32 c, uint32 d) { }
void fd2_cast_status_cure_spell(uint32 a, uint32 b, uint32 c, uint32 d, uint32 e) { }
void fd2_cast_status_inflict_spell(uint32 a, uint32 b, uint32 c, uint32 d, uint32 e) { }
void fd2_cast_status_spell_via_d1b(int a, int b, int c, int d, int e) { }
void fd2_paint_portrait_to_dialog_area(uint32 f) { }
void fd2_render_mini_char_status_panel(uint32 b, uint32 s, uint32 c) { }
uint8 data_fd2_audio_footstep_sfx_per_job_cadence_class_table[29] = {0};
uint8 data_fd2_audio_walk_step_sfx_cadence_counter = 0;
uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter = 0;
uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle = 0;
uint32 data_fd2_audio_summon_spell_sfx_bank_buf_ptr = 0;
int32  data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[15] = {0};
uint8  data_fd2_battle_summon_spell_8slot_visibility_table[7] = {0};
uint32 data_fd2_battle_summon_spell_8slot_y_offset_table[7] = {0};
int32  data_fd2_battle_summon_spell_8slot_row_multiplier_table[7] = {0};
int32  data_fd2_battle_summon_main_anim_12slot_frame_counter_array[12] = {0};
int32  data_fd2_battle_summon_main_anim_12slot_color_idx_array[12] = {0};
uint8  data_fd2_battle_summon_main_anim_color_rotation_counter = 0;
uint8  data_fd2_battle_summon_main_anim_terminate_flag = 0;
uint8  data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
int32  data_fd2_battle_summon_main_anim_12slot_y_offset_table[12] = {0};
uint8  data_fd2_battle_summon_main_anim_12color_v_offset_table[12] = {0};
uint8  data_fd2_battle_summon_main_anim_12color_sprite_offset_table[12] = {0};
uint8  data_fd2_battle_summon_spell_anim_phase_byte = 0;
uint8  data_fd2_battle_summon_spell_anim_aux_state_byte_unread = 0;
uint8  data_fd2_battle_summon_spell_sprite_anim_tick_counter = 0;
int32  data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[6] = {0};
int32  data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[6] = {0};
uint8  data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[6] = {0};
uint8  data_fd2_battle_summon_anim_variant_a_color_rotation_counter = 0;
uint8  data_fd2_battle_summon_anim_variant_a_terminate_flag = 0;
int32  data_fd2_battle_summon_anim_variant_a_10color_y_offset_table[10] = {0};
int32  data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[6] = {0};
int32  data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[6] = {0};
uint8  data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[6] = {0};
uint8  data_fd2_battle_summon_anim_variant_b_color_rotation_counter = 0;
uint8  data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
int32  data_fd2_battle_summon_anim_variant_b_10color_y_offset_table[10] = {0};
int32  data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[4] = {0};
int32  data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[4] = {0};
uint8  data_fd2_battle_summon_anim_variant_d_color_rotation_counter = 0;
uint8  data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
uint8  data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 0;
int32  data_fd2_animation_summon_variant_d_3slot_color_row_offsets[10] = {0};
uint8  data_fd2_animation_summon_variant_e_16slot_sprite_base_table[16] = {0};
int32  data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[16] = {0};
/* fd2_tick_tutorial_progress_with_sfx: now in anim.c */
void fd2_run_full_turn_cycle(void) { }
/* fd2_enemy_turn_action_dispatcher: now in btl_ai.c */
/* fd2_ai_score_offensive_spell: now in btl_ai.c */
int g_build_spell_list_return = 0;
int fd2_build_usable_spell_list(uint32 ci, uint32 buf) { return g_build_spell_list_return; }
/* fd2_score_spell_candidate: now in btl_ai.c */
double data_fd2_battle_ai_enemy_spell_score_multiplier_15 = 1.5;
/* fd2_ai_score_item_use: now in btl_ai.c */
int fd2_count_usable_inventory_slots(uint32 ci) { return 0; }
void (*data_fd2_battle_ai_post_action_consequence_table[90])(uint32);
void (*data_fd2_battle_spell_handler_table[28])(uint32, uint32, uint8 *);
static void g_noop_post_action_handler(uint32 x) { (void)x; }
static void g_noop_void_handler(void) { }
void (*data_fd2_chapter_init_handler_table[30])(void) = {
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler
};
void (*data_fd2_chapter_end_handler_table[30])(void) = {
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler,
    g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler, g_noop_void_handler
};
void (*data_fd2_chapter_post_action_handler_table[30])(uint32) = {
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler,
    g_noop_post_action_handler, g_noop_post_action_handler
};
uint32 data_fd2_battle_ai_best_spell_score = 0;
uint32 data_fd2_battle_ai_best_item_score = 0;
uint32 data_fd2_battle_ai_best_spell_target_x = 0;
uint32 data_fd2_battle_ai_best_spell_target_y = 0;
uint32 data_fd2_battle_ai_best_spell_id = 0;
uint32 data_fd2_battle_ai_best_item_target_x = 0;
uint32 data_fd2_battle_ai_best_item_target_y = 0;
uint32 data_fd2_battle_ai_best_item_slot = 0;
uint32 data_fd2_battle_ai_best_physical_target_x = 0;
uint32 data_fd2_battle_ai_best_physical_target_y = 0;
uint32 data_fd2_battle_ai_best_physical_target_idx = 0;
uint32 data_fd2_battle_ai_best_physical_score = 0;
int g_blit_indexed_sprite_calls = 0;
uint32 g_blit_indexed_sprite_last_frame = 0;
int g_blit_indexed_sprite_last_x = 0;
int g_blit_indexed_sprite_last_y = 0;
void fd2_blit_indexed_sprite(uint32 a, uint32 f, int x, int y, int m) {
    g_blit_indexed_sprite_calls++;
    g_blit_indexed_sprite_last_frame = f;
    g_blit_indexed_sprite_last_x = x;
    g_blit_indexed_sprite_last_y = y;
    (void)a; (void)m;
}
uint8  data_fd2_chapter_chapter_init_done_flag = 0;
uint8  data_fd2_ui_play_active_flag = 0;
uint8  data_fd2_ui_game_speed_flag = 0;
uint32 data_fd2_battle_view_window_max_x = 13;
uint32 data_fd2_battle_view_window_max_y = 8;
uint32 data_fd2_battle_compose_left_edge_clip_offset = 0;
uint32 data_fd2_battle_compose_parallax_scroll_y_rows = 0;
uint32 data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;
uint32 data_fd2_battle_walk_anim_x_scroll_offset = 0;
uint32 data_fd2_battle_walk_anim_y_scroll_rows = 0;
uint32 data_fd2_battle_pathfind_tile_cost_table_ptr = 0;
uint32 data_fd2_battle_pathfind_battle_tile_map_ptr = 0;
uint8  data_fd2_battle_pathfind_map_width = 0;
uint8  data_fd2_battle_pathfind_map_height = 0;
uint32 data_fd2_battle_pathfind_caller_context = 0;
uint8  data_fd2_battle_pathfind_floodfill_seed_x = 0;
uint8  data_fd2_battle_pathfind_floodfill_seed_y = 0;
uint8  data_fd2_battle_pathfind_floodfill_max_steps = 0;
uint8  data_fd2_battle_pathfind_dst_x = 0;
uint8  data_fd2_battle_pathfind_dst_y = 0;
uint32 data_fd2_battle_pathfind_path_output_buffer_ptr = 0;
uint8  data_fd2_battle_pathfind_current_depth = 0;
uint8  data_fd2_battle_pathfind_best_path_length = 0;
uint8  data_fd2_battle_pathfind_step_stack[256] = {0};
void  *data_fd2_animation_ani_decoder_frame_dispatch_table[10] = {0};
uint16 data_fd2_animation_ani_decoder_target_width = 0;
uint32 data_fd2_animation_ani_decoder_dst_buf = 0;
uint32 data_fd2_animation_ani_decoder_src_buf = 0;
int g_composite_call_count = 0;
void fd2_composite_battle_frame(int ctx) { g_composite_call_count++; }

/* --- AI dispatcher stubs + tracking --- */
int g_attack_dispatch_return = 0;
int g_attack_dispatch_calls = 0;
int g_seek_optimal_return = 0;
int g_advance_nearest_return = 0;
int g_walk_return = 0;
int g_score_physical_return = 0;
int g_pass_turn_calls = 0;
int g_execute_spell_calls = 0;
int g_execute_physical_calls = 0;
/* fd2_attack_action_dispatch: now in btl_ai.c */
/* fd2_ai_seek_optimal_position: now in btl_ai.c */
/* fd2_ai_advance_to_nearest_team_target: now in btl_ai.c */
/* fd2_ai_pass_turn_with_heal: now in btl_ai.c */
/* fd2_ai_walk_to_target_tile: now in btl_ai.c */
/* fd2_ai_score_physical_attack: now in btl_ai.c */
/* fd2_execute_ai_offensive_spell: now in btl_ai.c */
void fd2_play_spell_cast_sequence(uint32 ci, uint32 si, uint32 nt, uint32 tb) { }
/* fd2_execute_ai_physical_attack: now in btl_ai.c */
uint32 fd2_animate_combat_speech_bubbles(uint32 ci, uint32 ti) { return 0; }
void fd2_render_combatant_hp_bar_proportional(uint32 d, uint32 s, uint32 ci, uint32 st) { }
int fd2_animate_combat_hit_with_hp_drain(uint32 a, uint32 d, uint32 st) { return 0; }
void fd2_render_combat_combatant_panels(uint32 st, uint32 a, uint32 d) { }
void fd2_play_full_combat_cinematic(uint32 a, uint32 d) { }
void fd2_process_xp_and_level_up_for_char(uint32 ci) { }
/* fd2_execute_ai_item_use: now in btl_ai.c */
void fd2_play_figani_char_intro_animation(uint32 c) { }
/* fd2_apply_use_effect_dispatch: already in spellwk.c */
uint32 data_fd2_battle_tile_map_anim_frame_counter = 0;
void fd2_add_item_to_inventory(uint32 c, uint32 i) { }
int g_play_sfx_sample_from_bank_calls = 0;
void fd2_play_sfx_sample_from_bank(uint32 b, uint32 s, uint32 p) { g_play_sfx_sample_from_bank_calls++; (void)b; (void)s; (void)p; }
void fd2_clear_all_chars_facing(void) { }
void fd2_paint_char_sprite_at_world_with_mode(uint32 w, uint32 s, uint32 c, uint32 m, uint32 co) { }
void fd2_paint_threat_overlay_for_team(uint32 ctx) { }
/* Pathfind stub. Two modes are distinguished by the `md` (mode) arg:
 *   md==2  -> "find optimal reachable cell" call (fd2_ai_seek_optimal_position).
 *            When g_pathfind_write_dst!=0 it writes the discovered destination
 *            (g_pathfind_dst_x, g_pathfind_dst_y) into the db output buffer, and
 *            returns g_pathfind_return (the step/0xFF code).
 *   md==0/1 -> "route toward a specific target" call (inside
 *            fd2_ai_walk_to_target_tile). Returns g_pathfind_walk_return.
 * This separation lets a seek-position test pin the seek's pathfind result and
 * reported destination independently of the walk routine's own return value,
 * which is required to lock in the EAX-tracking semantics of did_move. */
int g_pathfind_return = 0;
int g_pathfind_walk_return = 0;
int g_pathfind_write_dst = 0;
int g_pathfind_dst_x = 0;
int g_pathfind_dst_y = 0;
int fd2_pathfind_to_destination(uint32 ct, uint32 sx, uint32 sy, uint32 ms,
    uint32 db, uint32 f1, uint32 f2, uint32 md, uint32 tm, uint32 af) {
    (void)ct; (void)sx; (void)sy; (void)ms;
    (void)f1; (void)f2; (void)tm; (void)af;
    if (md == 2) {
        if (g_pathfind_write_dst != 0 && db != 0) {
            ((uint8 *)db)[0] = (uint8)g_pathfind_dst_x;
            ((uint8 *)db)[1] = (uint8)g_pathfind_dst_y;
        }
        return g_pathfind_return;
    }
    return g_pathfind_walk_return;
}
void fd2_obfuscate_battle_tile_map(uint32 tm) { }
void fd2_init_movement_range_floodfill(uint32 ct, uint32 x, uint32 y,
    uint32 rng, uint32 tm, uint32 af) { }
/* fd2_compute_aoe_targets: now in btl_ai.c */
/* fd2_pan_cursor_to_char: already in cursor.c */
