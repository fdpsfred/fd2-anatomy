/*
 * testglob.c — Fake global definitions for all test suites
 */
#include "types.h"
#include "globals.h"
#include <stdlib.h>
#include <stdio.h>

/* Per-test heartbeat for build_test.py's hang detector. Each test writes its
 * name (with a monotonically increasing seq so the content always changes) to
 * E:\OUT\HB.TXT via fopen/fprintf/FCLOSE — the close is what forces DOSBox to
 * commit the write to the host file, so the host-side poller sees it live
 * (an in-program fflush alone does NOT propagate under DOSBox local-drive
 * caching). If a test hangs, HB.TXT freezes on its name -> the poller can both
 * detect the stall and report exactly which test hung. */
void test_heartbeat(const char *name)
{
    static unsigned long hb_seq = 0;
    FILE *f = fopen("E:\\OUT\\HB.TXT", "w");
    if (f) {
        fprintf(f, "%lu %s\n", ++hb_seq, name);
        fclose(f);
    }
}

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
uint8  data_fd2_orphan_table_60181[99 * 3];
uint8  data_fd2_job_allowed_items_table[27 * 7];
void  *data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21[21];
void  *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];
uint32 data_fd2_battle_pending_xp_credit = 0;
uint8  data_fd2_battle_last_hit_or_miss_flag = 1;
uint16 data_fd2_shared_rng_seed = 0;
uint32 data_fd2_battle_job_magic_resist_table[27];
uint8  data_fd2_battle_job_crit_rate_table[27];
uint32 data_fd2_battle_turn_counter = 0;
uint32 data_fd2_runtime_battle_state_ptr = 0;
uint32 data_fd2_battle_fast_mode_walk_overlay_ptr = 0;
uint32 data_fd2_menu_dialog_state_handle = 0;
uint32 data_fd2_tile_anim_table_base = 0;
uint32 data_fd2_chinese_font_sheet = 0;
uint8  data_fd2_ui_terrain_hud_user_enabled = 0;
uint32 data_fd2_ui_terrain_hud_panel_offset_51a0c = 0;
uint8  data_fd2_audio_sfx_driver_available_flag = 0;
uint8  data_fd2_audio_sfx_enabled_flag = 0;
uint32 data_fd2_audio_sfx_sample_handle_0 = 0;
/* Distinct non-zero default so the AIL_stop_sample spy can tell which real SFX
 * player called it: fd2_play_sfx_sample_from_bank drives this slot-1 handle,
 * fd2_play_sfx_with_handle drives slot-0 (handle_0). Callers that fire both
 * players (summon ticks, etc.) don't set the handles, so this sentinel keeps the
 * two routable. Tests that care set both handles explicitly. */
uint32 data_fd2_audio_sfx_sample_handle_1 = 0x5EE80000;
uint32 data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 0;
char   data_fd2_string_ui_render_decimal_format_template[6] = "%0.5d";
char   data_fd2_string_resource_filename_fdtxt_dat[] = "FDTXT.DAT";
char   data_fd2_string_resource_filename_fdother_dat[] = "FDOTHER.DAT";
char   data_fd2_string_resource_filename_fdfield_dat_51a59[] = "FDFIELD.DAT";
char   data_fd2_string_resource_filename_fdshap_dat_51a65[] = "FDSHAP.DAT";
char   data_fd2_string_resource_filename_dato_dat_51a70[] = "DATO.DAT";
char   data_fd2_string_save_load_oom_msg_load_pbuf_50004[] = " Out of Memory !!!\n";
char   data_fd2_string_save_load_oom_msg_tile_event_50023[] = " Out of Memory !!!\n";
char   data_fd2_string_save_load_oom_msg_runtime_char_50037[] = " Out of Memory !!!\n";
char   data_fd2_string_field_map_oom_msg_chapter_runtime_50064[] = " Out of Memory !!!\n";
char   data_fd2_string_field_map_fdicon_not_found_err_50086[] = "\n\n File not found 'FDICON.B24!! \n\n";
uint32 data_fd2_shared_party_total_gold = 0;
uint32 data_fd2_shared_menu_party_roster_buffer_ptr = 0;
uint32 data_fd2_shared_menu_party_member_count = 0;
uint32 data_fd2_battle_anim_phase = 0;
uint32 data_fd2_battle_ai_post_action_consequence_idx = 0;
uint32 data_fd2_battle_player_action_result_code = 0;
uint32 data_fd2_chapter_current_chapter_id = 1;
uint32 data_fd2_chapter_cutscene_event_state = 0;
uint32 data_fd2_graphics_static_bg_buffer_ptr = 0;
uint8  data_fd2_graphics_text_scroll_pending_line_count = 0;
uint32 data_fd2_graphics_animated_bg_buffer_ptr = 0;
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
uint32 chapter_portrait_load_buffer = 0;
uint32 battle_scene_snapshot = 0;
uint32 current_chapter_text = 0;
uint32 portrait_sprite_cache = 0;
uint32 data_fd2_resource_portrait_cache_count = 0;
uint32 data_fd2_resource_portrait_cache_total_size = 0;
uint32 data_fd2_resource_portrait_cache_alloc_offset = 0;
uint32 data_fd2_resource_portrait_cache_buffer_used = 0;
uint8  data_fd2_resource_portrait_cache_id_list_base[40] = {0};
uint32 data_fd2_battle_current_active_char_idx = 0;
uint8  data_fd2_ui_click_debounce_skip_count = 0;
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
uint8  data_fd2_chapter_init_phase_flag = 0;
uint16 data_fd2_input_idle_current_bios_tick_word = 0;
uint16 data_fd2_input_idle_last_rendered_tick_word = 0;
uint8  data_fd2_input_last_key_pressed = 0;
uint8  data_fd2_input_key_input_mode = 0;
uint32 data_ail_alloc_fnptr = 0;
uint32 data_ail_free_fnptr = 0;
uint32 data_fd2_engine_wait_one_bios_tick_last_seen = 0;
uint32 data_fd2_engine_wait_n_bios_ticks_last_seen = 0;
void fd2_delay_ticks(uint32 t) { }
void fd2_execute_offensive_targeted_spell(int a, int b, int c, int d) { }
void fd2_execute_offensive_full_screen_flash_spell(int a, int b, int c, int d) { }
/* fd2_dispatch_variant_b_cast: now emitted for real in src/spell/spellcin.c;
 * its former no-op stub here was removed. */
void fd2_cast_ap_boost_spell(int a, int b, uint8 *c) { }
void fd2_cast_dp_boost_spell(int a, int b, uint32 c) { }
void fd2_cast_speed_boost_spell(uint32 a, uint32 b, uint32 c) { }
int g_play_sfx_with_handle_calls = 0;
int g_dlg_blink_calls = 0;
/* Per-invocation counter for the real fd2_play_sfx_sample_from_bank
 * (src/audio/audio.c). Defined here ahead of the AIL_stop_sample spy that
 * increments it; see the relocation note near the removed stub below for the
 * handle-based routing rationale. */
int g_play_sfx_sample_from_bank_calls = 0;
/* SFX-id capture log: fd2_animate_spell_impact_per_target's per-spell SFX
 * dispatch chain is the highest-risk control flow in that function, so its
 * test pins the exact sequence of fired SFX ids (arg b) per frame. Additive:
 * existing tests only read g_play_sfx_with_handle_calls. */
int    g_sfx_last_id = 0;
int    g_sfx_id_count = 0;
int    g_sfx_id_log[64];
/* Additive arg-capture for fd2_play_and_free_status_effect_sfx's test: it must
 * forward the bank handle (arg a) and the trailing flag (arg c). Existing tests
 * only read g_sfx_last_id / g_play_sfx_with_handle_calls, so adding these is
 * non-breaking. */
uint32 g_sfx_last_arg_a = 0;
int    g_sfx_last_arg_c = 0;
/* Additive: captures the AIL sample handle (slot) the real player programmed, so
 * fd2_play_sfx_sample_from_bank's direct test can assert it drives slot 1
 * (data_fd2_audio_sfx_sample_handle_1) rather than slot 0. Non-breaking: existing
 * tests don't read it. */
uint32 g_sfx_last_handle = 0;
/* fd2_play_sfx_with_handle is now a real emitted function (src/audio/audio.c);
 * its former counting stub here was removed. The SFX spy seam relocates one
 * level down into the AIL_* sample stubs the real player drives, mirroring the
 * fd2_blit_rectangle -> fd2_composite_battle_tile_map relocation done earlier.
 *
 * The real player runs its three audio gates, then unconditionally calls
 * AIL_stop_sample once per invocation (both the stop-only sfx_id==-1 path and
 * the normal play path) -> that is the host-observable per-call counter
 * (g_play_sfx_with_handle_calls / g_dlg_blink_calls). On the normal path it
 * then calls AIL_set_sample_address(handle, base+off(id), end(id)-off(id)):
 * caller tests stage a sfx bank (audiofix_make_bank) whose per-id offsets are
 * laid out so that length == sfx_id and start == base + off(id). The spy
 * therefore records the recovered sfx id in g_sfx_last_id / g_sfx_id_log (the
 * captured length) and the resolved sample start in g_sfx_last_arg_a. The
 * loop_count argument is recovered from AIL_set_sample_loop_count
 * (g_sfx_last_arg_c). See audiofix.h for the exact (triangular) offset layout.
 *
 * Gate note: a caller test that wants the SFX to actually fire must enable the
 * two driver flags + clear the cinematic flag; audiofix_enable_sfx() does this. */
int g_ail_stop_sample_calls = 0;
int g_ail_init_sample_calls = 0;
int g_ail_set_sample_addr_calls = 0;
int g_ail_start_sample_calls = 0;
void AIL_stop_sample(uint32 sample)
{
    g_ail_stop_sample_calls++;
    /* Route the per-invocation counter to the right player by the handle the
     * real function passed: slot 1 (handle_1) is fd2_play_sfx_sample_from_bank;
     * everything else (slot 0 / handle_0) is fd2_play_sfx_with_handle, which the
     * dialog typewriter and most callers drive. */
    if (sample == data_fd2_audio_sfx_sample_handle_1) {
        g_play_sfx_sample_from_bank_calls++;
    } else {
        g_play_sfx_with_handle_calls++;
        g_dlg_blink_calls++;
    }
}
void AIL_init_sample(uint32 sample) { (void)sample; g_ail_init_sample_calls++; }
void AIL_set_sample_address(uint32 sample, uint32 start, uint32 len)
{
    g_ail_set_sample_addr_calls++;
    g_sfx_last_handle = sample;   /* slot/handle programmed     */
    g_sfx_last_arg_a = start;     /* bank base (entry off==0)  */
    g_sfx_last_id = (int)len;     /* sfx id    (entry end==id) */
    if (g_sfx_id_count < 64) {
        g_sfx_id_log[g_sfx_id_count] = (int)len;
    }
    g_sfx_id_count++;
}
void AIL_set_sample_loop_count(uint32 sample, int count)
{
    (void)sample;
    g_sfx_last_arg_c = count;
}
void AIL_start_sample(uint32 sample) { (void)sample; g_ail_start_sample_calls++; }
/* Per-spell animation parameter tables (data segment @ 0x51F33/0x51F54/0x51F75).
 * Defined here with the real binary bytes until the data segment is emitted, so
 * anim tests assert on true frame counts / sprite offsets / SFX ids. */
uint8 data_fd2_animation_spell_sprite_offset_table[33] = {
    0x31,0x31,0x31,0x31,0x40,0x40,0x40,0x40,0x4c,0x57,0x31,0x31,0x31,0x39,0x39,
    0x39,0x39,0xb7,0x7e,0x93,0xcc,0xd9,0xaa,0x31,0x31,0xbf,0x8a,0x9e,0x31,0x31,
    0x00,0x00,0x40
};
uint8 data_fd2_animation_spell_frame_count_table[33] = {
    8,8,8,8,10,10,10,10,11,27,8,8,8,7,7,7,7,8,12,11,13,13,13,8,8,13,9,12,8,8,
    0,0,10
};
uint8 data_fd2_animation_spell_sfx_frame_table[33] = {
    6,6,6,6,9,9,9,9,10,14,0,0,0,12,12,12,12,6,7,8,4,4,3,0,0,5,3,2,0,0,0,0,9
};
/* Earthquake screen-shake params (data segment @ 0x52096). int[9] laid out as
 * 3 X-offsets, 3 Y-offsets, 3 scales. Real binary values until data emitted. */
int32 data_fd2_animation_earthquake_screen_shake_params_table[9] = {
    128, 0, -128, 128, 0, 128, 131, 128, 125
};
/* Resource portrait sheet base pointer (data segment @ 0x53AD1). Tests point it
 * at a zeroed scratch buffer. */
uint32 data_fd2_resource_portrait_sheet_ptr = 0;
/* fd2_play_rising_pre_cast_effect and fd2_play_variant_b_slide_pre_effect are
 * now real emitted functions (src/spell/spellcin.c); their former stubs were
 * removed. Both are pure VGA/VRAM cinematic workers whose behavior is deferred
 * to Phase 9 (see tests/spell/spellcin.c header). Their callees
 * fd2_render_circle_anim_row and fd2_render_filled_circle_band_anim (graphics,
 * not yet emitted) are stubbed here so spellcin.obj links. */
void fd2_render_circle_anim_row(int cx, int cy, int r, int scale_num,
                                int start_row, int end_row,
                                uint8 *palette_remap_src) {
    (void)cx; (void)cy; (void)r; (void)scale_num;
    (void)start_row; (void)end_row; (void)palette_remap_src;
}
void fd2_render_filled_circle_band_anim(uint32 param_1, uint32 param_2,
                                        uint32 param_3, int cx, int cy,
                                        int radius) {
    (void)param_1; (void)param_2; (void)param_3;
    (void)cx; (void)cy; (void)radius;
}
/* The four warp sub-animations (fd2_animate_warp_teleport_char,
 * fd2_animate_warp_portal_open_at, fd2_animate_warp_out_collapse,
 * fd2_animate_warp_in_expand) now live in src/spell/spellcin.c; their former
 * stubs were removed. */
uint16 data_fd2_animation_palette_cycle_last_tick = 0;
uint8  data_fd2_animation_palette_cycle_frame_idx = 0;
uint8  data_fd2_animation_palette_cycle_rgb_table[93] = {0};
/* fd2_check_char_is_dead stub. Default 0 (alive) keeps historical behavior for
 * every existing test. fd2_save_runtime_char_to_template uses this to decide the
 * char-0 (索爾) dead-skip special case, so its test pins the return via
 * g_check_char_is_dead_return. */
int g_check_char_is_dead_return = 0;
int fd2_check_char_is_dead(uint32 c) { (void)c; return g_check_char_is_dead_return; }
/* fd2_scan_chars_within_manhattan_range: now in btl_ai.c */
uint32 data_fd2_ui_anim_sprite_sheet_ptr = 0;
void  *data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[5] = {0};
uint32 data_fd2_dialog_portrait_blink_frame_idx = 0;
uint32 data_fd2_dialog_portrait_blink_subtick_counter = 0;
uint32 data_fd2_dialog_last_action_value_param = 0;
uint32 data_fd2_dialog_active_portrait_blit_offset = 0;
void  *data_fd2_dialog_area_backup_buffer = 0;
uint32 data_fd2_dialog_current_speaker_char_ptr = 0;
uint32 data_fd2_large_game_state_buffer_ptr = 0;
uint32 data_fd2_dialog_blink_phase_oscillator = 0;
uint32 data_fd2_dialog_blink_phase_oscillator_tick_latch = 0;
uint32 data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 0;
uint32 data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;
uint32 data_fd2_graphics_chapter_ambient_palette_anim_tick_latch = 0;
uint8  data_fd2_graphics_char_sprite_shake_jitter_bit = 0;
int32  data_fd2_graphics_char_sprite_paint_jitter_tick_latch = 0;
/* fd2_composite_battle_frame (rndscene.c) pipeline-callee stubs with arg
 * capture, so the compositor test can assert workspace address, pixel
 * constants, and call ordering forwarded to each stage. */
int    g_tile_map_calls = 0;
uint32 g_tile_map_last_dst = 0;
uint32 g_tile_map_last_stride = 0;
uint32 g_tile_map_last_w = 0;
uint32 g_tile_map_last_h = 0;
uint32 g_tile_map_last_ox = 0;
uint32 g_tile_map_last_oy = 0;
int    g_composite_call_count = 0;
/* opt-in ordered dst log (default off; used by the full-screen-flash test to
 * confirm the two real spell-effect composites route into the back-buffer then
 * a distinct malloc'd buffer, followed by the finalizer composite). */
int    g_tile_map_log_on = 0;
int    g_tile_map_log_count = 0;
uint32 g_tile_map_log_dst[16];
/* Test-controllable loop-break seam for the idle loops (the real
 * fd2_wait_input_with_dialog_repaint and the menu loops that idle through it).
 * The tile-map composite runs exactly once at the top of every idle-loop body
 * (and once per fd2_composite_battle_frame pass), and is the only such callee
 * still backed by a recording stub, so it is the single harness-driveable seam
 * for running an idle loop BODY exactly once: when g_repaint_flip_buffer_after
 * != 0, the g_repaint_settings_calls counter reaching that threshold flips the
 * BIOS keyboard buffer from empty->nonempty (tail 0x41C := head 0x41A + 2) so
 * the next loop-top fd2_check_keyboard_buffer_nonempty() returns nonzero and
 * the loop exits. Default 0 keeps the historical no-op behavior for all other
 * tests. (The former seam lived in the fd2_render_terrain_info_hud_panel stub;
 * that function is now a real emitted routine in src/gfx/rndstat.c.) */
int g_repaint_settings_calls = 0;
int g_repaint_flip_buffer_after = 0;
void fd2_composite_battle_tile_map(uint32 d, uint32 s, uint32 w, uint32 h, uint32 ox, uint32 oy) {
    /* The tile-map blit is the first stage of every fd2_composite_battle_frame
     * pass and runs exactly once per composite (unconditional, both skip-cycle
     * paths). It is the host-observable proxy that counts composite frames for
     * caller tests (cursor.c, spelleff.c, btl_ai.c, ...) that only care "a frame
     * composited". fd2_blit_rectangle is now a real emitted function
     * (src/gfx/blitspr.c) and no longer available as that proxy. */
    g_composite_call_count++;
    g_tile_map_calls++;
    g_tile_map_last_dst = d; g_tile_map_last_stride = s;
    g_tile_map_last_w = w; g_tile_map_last_h = h;
    g_tile_map_last_ox = ox; g_tile_map_last_oy = oy;
    if (g_tile_map_log_on && g_tile_map_log_count < 16) {
        g_tile_map_log_dst[g_tile_map_log_count] = d;
        g_tile_map_log_count++;
    }

    g_repaint_settings_calls++;
    if (g_repaint_flip_buffer_after != 0 &&
        g_repaint_settings_calls >= g_repaint_flip_buffer_after) {
        *(volatile uint16 *)0x41CuL =
            (uint16)(*(volatile uint16 *)0x41AuL + 2);
    }
}
/* fd2_paint_cursor_overlay_pattern, fd2_composite_all_chars_overlay,
 * fd2_paint_char_sprite_at_world_pos, fd2_paint_chars_shadow_overlay and
 * fd2_blit_animated_tile_at_pos are now real emitted functions
 * (src/gfx/rndscene.c / src/gfx/blittile.c); their former no-op/recording
 * stubs here were removed. The real fd2_composite_all_chars_overlay loops over
 * alive party slots calling the real fd2_paint_char_sprite_at_world_pos and
 * finishes with one unconditional real fd2_paint_chars_shadow_overlay. The
 * per-char paint's own blit reaches the recording fd2_tile_blit_24x24_passthrough
 * / fd2_tile_blit_24x24_dimmed_grayscale stubs below; the shadow overlay's
 * tile-redraw goes through the real fd2_blit_animated_tile_at_pos, whose own
 * blit reaches that same passthrough recording stub (rndscene shadow tests set
 * up a renderable tile-map so every requested tile resolves to a blit, then
 * recover (x, y) from the recorded dst offset). */

/* Recording stub for fd2_tile_blit_24x24_passthrough (the RLE row blitter, real
 * body not yet emitted). fd2_blit_24x24_at_window_relative_pos (real, emitted in
 * src/gfx/blittile.c) is the only caller; recording (src, dst, stride) at this
 * level lets the blittile.c + cursor-overlay tests verify the real window-clip /
 * dst-offset / sprite-source arithmetic without touching pixels. */
int    g_blitpass_calls = 0;
uint32 g_blitpass_src[64];
uint32 g_blitpass_dst[64];
uint32 g_blitpass_stride[64];
void fd2_tile_blit_24x24_passthrough(uint32 src, uint32 dst, uint32 stride) {
    if (g_blitpass_calls < 64) {
        g_blitpass_src[g_blitpass_calls] = src;
        g_blitpass_dst[g_blitpass_calls] = dst;
        g_blitpass_stride[g_blitpass_calls] = stride;
    }
    g_blitpass_calls++;
}
/* Recording stub for fd2_tile_blit_24x24_dimmed_grayscale (the greyed/dimmed
 * 24x24 blitter, real body not yet emitted). The real
 * fd2_paint_char_sprite_at_world_pos calls this instead of the passthrough
 * blitter when the unit's flags bit7 (already-acted) is set. Records into the
 * shared g_blitpass_* arrays (so dst/src arithmetic checks are uniform) and
 * bumps a separate dimmed counter so tests can distinguish which blitter ran. */
int    g_blitdim_calls = 0;
void fd2_tile_blit_24x24_dimmed_grayscale(uint32 src, uint32 dst, uint32 stride) {
    if (g_blitpass_calls < 64) {
        g_blitpass_src[g_blitpass_calls] = src;
        g_blitpass_dst[g_blitpass_calls] = dst;
        g_blitpass_stride[g_blitpass_calls] = stride;
    }
    g_blitpass_calls++;
    g_blitdim_calls++;
}
/* fd2_tile_blit_24x24_with_remap_table (real body not yet emitted): records into
 * the shared g_blitpass_* arrays plus a separate remap counter so tests can tell
 * the remap branch from the plain passthrough branch. */
int    g_blitremap_calls = 0;
uint32 g_blitremap_table[64];
void fd2_tile_blit_24x24_with_remap_table(uint32 src, uint32 dst, uint32 stride,
                                          uint32 remap_table) {
    if (g_blitpass_calls < 64) {
        g_blitpass_src[g_blitpass_calls] = src;
        g_blitpass_dst[g_blitpass_calls] = dst;
        g_blitpass_stride[g_blitpass_calls] = stride;
        g_blitremap_table[g_blitpass_calls] = remap_table;
    }
    g_blitpass_calls++;
    g_blitremap_calls++;
}
/* fd2_tile_blit_24x24_solid_color (the solid-colour silhouette blitter, real
 * body not yet emitted). The real fd2_animate_status_effect_overlay_flicker is
 * the caller; recording (src, dst, stride) into the shared g_blitpass_* arrays
 * plus the colour arg into a separate log lets the status-overlay test verify
 * the per-char dst-offset / sprite-source / colour-index arithmetic without
 * touching pixels. */
int    g_blitsolid_calls = 0;
uint32 g_blitsolid_color[64];
void fd2_tile_blit_24x24_solid_color(uint32 src, uint32 dst, uint32 color_or_stride,
                                     uint32 unused) {
    if (g_blitpass_calls < 64) {
        g_blitpass_src[g_blitpass_calls] = src;
        g_blitpass_dst[g_blitpass_calls] = dst;
        g_blitpass_stride[g_blitpass_calls] = color_or_stride;
        g_blitsolid_color[g_blitpass_calls] = unused;
    }
    g_blitpass_calls++;
    g_blitsolid_calls++;
}
/* fd2_render_terrain_info_hud_panel is now a real emitted function
 * (src/gfx/rndstat.c). Its former recording/loop-break stub here was removed;
 * the idle-loop break seam (g_repaint_settings_calls / g_repaint_flip_buffer_after)
 * moved up into the fd2_composite_battle_tile_map stub, which also runs once per
 * idle-loop body. */
void fd2_render_recruitment_party_screen(void) { }
uint32 data_fd2_ui_recruitment_screen_repaint_tick_latch = 0;
uint32 data_fd2_ui_slide_composed_target_buf_ptr = 0;
uint32 data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
uint32 data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
uint32 data_fd2_ui_menu_cursor_idx = 0;
/* field command menu templates — real FD2.LE values @ 0x51E9F / 0x53EF2 */
int32  data_fd2_ui_field_command_menu_options_template[4] = { 7, 5, 6, 4 };
int32  data_fd2_ui_field_command_menu_state_template[4] = { 0, 0, 0, 0 };
/* save/load/quit sub-menu templates — real FD2.LE values @ 0x51EF5 / 0x53F22 */
int32  data_fd2_ui_save_load_newgame_menu_template[4] = { 12, 13, 14, 15 };
int32  data_fd2_ui_save_load_menu_state_template[4] = { 0, 0, 0, 0 };
/* player action menu state template — real FD2.LE value @ 0x53F12 (all zero) */
int32  data_fd2_ui_player_action_menu_state_template[4] = { 0, 0, 0, 0 };
/* inline action menu template — real FD2.LE value @ 0x51ED5 (Attack/Spell/Item/Wait slot ids) */
int32  data_fd2_ui_inline_action_menu_template[4] = { 0, 1, 2, 3 };
/* item command menu templates — real FD2.LE values @ 0x51F05 / 0x53F32
 * (Use/Give/Sort/Drop slot ids; state all zero) */
int32  data_fd2_ui_item_command_menu_template[4] = { 8, 9, 10, 11 };
int32  data_fd2_ui_item_command_menu_state_template[4] = { 0, 0, 0, 0 };
/* status-effect overlay flicker colour template — real FD2.LE values @ 0x51F15
 * (32 bytes; mostly 0xC0 with a few status-specific colours). The real
 * fd2_animate_status_effect_overlay_flicker copies the first 30 bytes into a
 * stack scratch and indexes it by status_kind. */
uint8  data_fd2_animation_status_overlay_flicker_color_template[32] = {
    0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,
    0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,
    0xc0,0x92,0x48,0xd8,0xc0,0xc0,0x23,0xc0,
    0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x31,0x31
};
/* game options menu templates — real FD2.LE values @ 0x51EAF / 0x53F02 */
int32  data_fd2_ui_game_options_menu_slots_template[4] = { 0x12, 0x14, 0x16, 0x18 };
int32  data_fd2_ui_game_options_menu_state_template[4] = { 0, 0, 0, 0 };
/* dialog page-advance collapse template — real FD2.LE value @ 0x51EE5
 * (two corner sprite-index selectors, replicated to 16 bytes) */
int32  data_fd2_dialog_advance_collapse_template[4] = { 0x10, 0x11, 0x10, 0x11 };
uint32 data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[4] = {0};
uint32 data_fd2_chapter_intro_dialog_anim_frame_idx = 0;
uint32 data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
void fd2_render_chapter_dialog_borders(void) { }
void fd2_render_chapter_intro_dialog_panels(uint32 c, uint32 m) { }
/* capture wiring for fd2_blit_indexed_sprite_with_alloc tests; also drives the
 * real fd2_open_settings_dialog_with_slide corner-sprite blit. Records the last
 * (dst, sprite, stride) and counts total calls so the dialog-open / settings
 * loop tests can observe that the render ran. */
uint32 g_blitsetup_dst, g_blitsetup_sprite, g_blitsetup_stride;
int    g_blitsetup_calls = 0;
/* per-call log (page-advance collapse test verifies all 8 corner blits) */
uint32 g_blitsetup_dst_log[32];
uint32 g_blitsetup_sprite_log[32];
void fd2_blit_sprite_with_stride_setup(uint32 d, uint32 s, uint32 st)
{
    if (g_blitsetup_calls < 32) {
        g_blitsetup_dst_log[g_blitsetup_calls] = d;
        g_blitsetup_sprite_log[g_blitsetup_calls] = s;
    }
    g_blitsetup_dst = d;
    g_blitsetup_sprite = s;
    g_blitsetup_stride = st;
    g_blitsetup_calls++;
}
/* fd2_restore_dialog_area_from_buffer now has a real body in
 * src/dialog/dialog.c (inverse of fd2_backup_dialog_area_to_buffer). */
uint32 g_saveblk_out, g_saveblk_w, g_saveblk_h, g_saveblk_dst,
       g_saveblk_src, g_saveblk_stride;
int    g_saveblk_calls = 0;
void fd2_save_screen_block_to_buffer(uint32 out_buf, uint32 width, uint32 height,
                                     uint32 dst, uint32 src_ptr, uint32 stride)
{
    g_saveblk_out = out_buf;
    g_saveblk_w = width;
    g_saveblk_h = height;
    g_saveblk_dst = dst;
    g_saveblk_src = src_ptr;
    g_saveblk_stride = stride;
    g_saveblk_calls++;
}
/* fd2_assemble_dialog_frame_layered is now emitted for real in
 * src/dialog/dialog.c. Its callers' tests (fd2_play_dialog_open_animation,
 * and the dedicated frame-layout test) drive the real function and observe
 * its blit calls through the fd2_blit_sprite_raw_with_header log below. */
/* capture wiring for fd2_alloc_and_blit_indexed_sprite_chunk tests */
uint32 g_blitdec_dst, g_blitdec_sprite, g_blitdec_stride;
int    g_blitdec_calls = 0;
/* opt-in full per-call log (default off; used by the full-screen-flash test to
 * capture both real spell-effect overlay invocations' resolved fx-sprite addr
 * and dst, since the last-call vars above keep only the final call). */
int    g_blitdec_log_on = 0;
int    g_blitdec_log_count = 0;
uint32 g_blitdec_log_dst[16];
uint32 g_blitdec_log_sprite[16];
void fd2_blit_sprite_with_decoded_pixels(uint32 d, uint32 s, uint32 st)
{
    g_blitdec_dst = d;
    g_blitdec_sprite = s;
    g_blitdec_stride = st;
    if (g_blitdec_log_on && g_blitdec_log_count < 16) {
        g_blitdec_log_dst[g_blitdec_log_count] = d;
        g_blitdec_log_sprite[g_blitdec_log_count] = s;
        g_blitdec_log_count++;
    }
    g_blitdec_calls++;
}
/* capture wiring for fd2_blit_sheet_sprite_at_offset tests */
uint32 g_blitraw_dst, g_blitraw_sprite, g_blitraw_stride;
/* full call log (used by dialog frame-layout tests): records every raw blit */
int    g_blitraw_log_on = 0;
int    g_blitraw_count = 0;
uint32 g_blitraw_log_dst[512];
uint32 g_blitraw_log_sprite[512];
uint32 fd2_blit_sprite_raw_with_header(uint32 d, uint32 s, uint32 st)
{
    g_blitraw_dst = d;
    g_blitraw_sprite = s;
    g_blitraw_stride = st;
    if (g_blitraw_log_on && g_blitraw_count < 512) {
        g_blitraw_log_dst[g_blitraw_count] = d;
        g_blitraw_log_sprite[g_blitraw_count] = s;
        g_blitraw_count++;
    }
    return 0;
}
void fd2_render_recruitment_select_screen(uint32 a, uint32 b, uint32 c, uint32 d) { }
/* fd2_animate_spell_impact_per_target is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
/* fd2_animate_status_effect_overlay_flicker is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
/* fd2_animate_spell_full_screen_flash is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
void fd2_animate_spell_overlay_blink(uint32 a, uint32 b, uint32 c, uint32 d) { }
void fd2_show_damage_number(uint32 v, uint32 t, uint32 tg) { }
void fd2_show_miss_indicator(uint32 t) { }
void fd2_show_status_effect_overlay(uint32 t, uint32 s) { }
void fd2_animate_spell_projectile_paths(void) { }
/* fd2_composite_then_animate_projectiles: shared spell-finale + epilogue helper
 * @ 0x21190, routed to gfx/rndscene.c (not yet emitted). Stub here so callers
 * link; remove when rndscene.c lands the real definition. */
void fd2_composite_then_animate_projectiles(void) { }
/* fd2_remove_inventory_slot_at: now emitted for real in src/ui_menu/status.c.
 * Its old spy global g_remove_inventory_calls is gone; spell/spelleff.c now
 * observes the real slot-consume by checking slot[7].flag == 0x80. */
/* fd2_load_status_effect_sfx + fd2_play_and_free_status_effect_sfx: now emitted
 * for real in src/audio/audio.c. test_load_status_effect_sfx_real drives the
 * loader against staged real FDOTHER.DAT; test_play_and_free_status_effect_sfx
 * drives the player+free (observed via the g_sfx_* capture spy). */
/* fd2_collect_pending_death_drops: now in btl_turn.c */
/* fd2_display_dialog_scene: now emitted in src/dialog/dialog.c */
/* fd2_load_chapter_portrait: now emitted for real in src/rsrc/rsrc.c and
 * linked for real; its blit-offset branch + DATO.DAT load are driven by the
 * test_lcp_* cases in tests/rsrc/rsrc.c (observed via the g_dlg_blit_mirrored
 * capture spy + an independent DATO.DAT parse). */
/* fd2_close_status_screen_with_slide_out: now emitted in src/ui_menu/status.c
 * and linked for real; the test_close_status_screen_slide_out_runs_full_
 * teardown case in tests/ui_menu/status.c drives the real function (observed
 * via g_composite_call_count for the trailing recomposite). */
/* fd2_load_chapter_battle_data: now in rsrc/rsrc.c */
/* fd2_load_chapter_portraits_and_dump_tmp: now in rsrc/rsrc.c */
/* fd2_init_runtime_char_for_battle is now emitted in src/battle/btl_init.c
 * and linked for real; its caller test in tests/rsrc/rsrc.c drives the real
 * function and observes data_fd2_battle_party_member_count. */
void fd2_play_palette_fade_to_black(void) { }
int g_ending_menu_return = 0;
int fd2_play_ending_and_record_clear(void) { return g_ending_menu_return; }
int g_slot_selector_return = -1;
int fd2_save_slot_selector_ui(uint32 b, uint32 m) { (void)b; (void)m; return g_slot_selector_return; }
void fd2_close_intro_dialog_with_slide_out(void) { }
int g_chapter_transition_return = 0;
int fd2_chapter_transition_menu(void) { return g_chapter_transition_return; }

/* ---- fd2_load_save_and_init_engine leaf helper fakes ----
 * (the real fd2_load_save_and_init_engine now lives in src/life/main.c) */
/* fd2_save_compute_checksum: now emitted in src/save/save.c (the loader
 * checksum test now stores a real computed checksum in the FD2.SAV tail). */
/* fd2_save_crypt_buffer: now emitted in src/save/save.c (the FD2.SAV fixture
 * encrypts its image so the loader's real decrypt recovers the plaintext). */
/* fd2_load_chapter_background_layers: now in rsrc/rsrc.c */
/* fd2_load_portrait_to_cache: now emitted in src/rsrc/rsrc.c */
/* fd2_alloc_and_blit_indexed_sprite_chunk: now emitted in src/gfx/blitspr.c.
 * It calls fd2_save_screen_block_to_buffer exactly once per invocation, so the
 * save-block call counter (g_saveblk_calls) is an exact proxy for the
 * alloc/blit-chunk call count in any test that drives it in isolation. */
/* fd2_render_decimal_number_to_buffer: now emitted in src/gfx/rndstat.c and
 * linked for real. Its caller tests (panel / inventory / redfull in
 * tests/gfx/rndstat.c) drive the real renderer end-to-end through the real
 * fd2_blit_indexed_sprite_at_xy -> fd2_rle_blit_sprite spy, recovering each
 * rendered digit/overflow glyph from the g_rle_blit_log_* per-call log against
 * a fake sheet (table[i]=i). No argument-recording spy remains. */
/* fd2_render_hp_or_mp_bar_proportional: now emitted in src/gfx/rndstat.c. The
 * panel tests drive the real function, which computes the proportional segment
 * count and forwards to the real fd2_render_horizontal_bar_segments ->
 * fd2_blit_sheet_sprite_at_offset pipeline, so the bars are observed end-to-end
 * through the g_blitraw_* sprite log (no bar-specific spy needed). */
/* fd2_render_number_red_when_full: now emitted in src/gfx/rndstat.c. It is a
 * thin wrapper that forwards into the real fd2_render_decimal_number_to_buffer
 * with a red/white color chosen by current==max; the panel/redfull tests
 * observe its rendered digit glyphs through the g_rle_blit_log_* log. */
int    g_delay375b2_calls = 0;
uint32 g_delay375b2_last_ticks = 0;
void __delay_thunk_375b2(uint32 ticks) { g_delay375b2_calls++; g_delay375b2_last_ticks = ticks; }

/* Link-time stub for the still-unemitted callee of the orphan/unreachable
 * fd2_execute_aoe_spell_with_caster_portrait_radial_scatter (src/spell/spellcin.c).
 * That AoE cinematic has no caller and no test drives it (its per-frame work is
 * unconditional VRAM blits to the hardcoded mode-13h literal 0xA0504, deferred to
 * Phase 9 like the rest of spellcin.c). This stub only needs to resolve the
 * symbol; its real body is owned by its routing target
 * (fd2_blit_palette_remap_with_sprite_mask -> gfx/blitspr.c) and will replace
 * this when emitted. (The sibling scatter callee
 * fd2_scatter_sprite_around_origin_with_random_offset is now the real emitted
 * function in src/spell/spellcin.c; its former stub here was removed.) */
void fd2_blit_palette_remap_with_sprite_mask(
    uint8 *dst, uint16 *sprite_mask, uint32 stride, uint32 remap_table) {
    (void)dst; (void)sprite_mask; (void)stride; (void)remap_table;
}

/* fd2_composite_chars_with_spell_effect_overlay is now a real emitted function
 * (src/gfx/rndscene.c); its former recording stub here was removed. The real
 * overlay first composites a tile map (observable via the
 * fd2_composite_battle_tile_map dst log) and then, per targeted char, draws the
 * effect sprite through the recording fd2_blit_sprite_with_decoded_pixels stub
 * (whose opt-in log captures the resolved fx-sprite addr per call). Its own
 * behavior is covered by tests/gfx/rndscene.c; the caller
 * fd2_animate_spell_full_screen_flash observes those two seams. */
int g_ail_vol_calls = 0;
int g_ail_last_vol = 0;
int g_ail_last_ramp = 0;
void AIL_set_sequence_volume(uint32 s, int t, int r) { g_ail_vol_calls++; g_ail_last_vol = t; g_ail_last_ramp = r; (void)s; }
void AIL_stop_sequence(uint32 s) { (void)s; }
int  AIL_init_sequence(uint32 s, uint32 d, int i) { (void)s; (void)d; (void)i; return 0; }
void AIL_start_sequence(uint32 s) { (void)s; }
void AIL_set_sequence_loop_count(uint32 s, uint32 c) { (void)s; (void)c; }
/* fd2_load_dat_resource: now emitted in src/rsrc/rsrc.c. Its caller tests
 * drive the real loader against the staged real DAT files (copied into the
 * test cwd by build_test.py) and cross-check its output against an independent
 * parse via tests/include/realfile.h. */
int    g_rle_blit_calls = 0;
uint32 g_rle_blit_last_sprite = 0;
int32  g_rle_blit_last_x = 0;
int32  g_rle_blit_last_y = 0;
uint32 g_rle_blit_last_buf = 0;
int32  g_rle_blit_last_stride = 0;
uint32 g_rle_blit_last_palette = 0;
int32  g_rle_blit_y_log[4];
uint8  g_rle_blit_sprite_first_byte_log[4];
/* opt-in full per-call log (default off; used by the decimal-number digit
 * renderer test to recover each digit's resolved sprite addr + dst, and by the
 * HUD tests to pin the backdrop blit (now no longer the LAST rle blit, since the
 * real signed-modifier renderer appends sign-icon + digit blits after it)). */
int    g_rle_blit_log_on = 0;
uint32 g_rle_blit_log_sprite[64];
uint32 g_rle_blit_log_dst[64];
int32  g_rle_blit_log_stride[64];
uint32 g_rle_blit_log_palette[64];
void fd2_rle_blit_sprite(uint32 rle_stream, int32 dst_x, int32 dst_y,
                         uint32 dst_buf, int32 stride, uint32 palette_op) {
    g_rle_blit_last_sprite = rle_stream;
    g_rle_blit_last_x = dst_x;
    g_rle_blit_last_y = dst_y;
    g_rle_blit_last_buf = dst_buf;
    g_rle_blit_last_stride = stride;
    g_rle_blit_last_palette = palette_op;
    if (g_rle_blit_calls < 4) {
        g_rle_blit_y_log[g_rle_blit_calls] = dst_y;
        /* first payload byte of the loaded sprite, used by the rsrc tests to
         * verify which DAT index the real loader fetched (each fixture seeds
         * payload[idx][0] = idx). */
        g_rle_blit_sprite_first_byte_log[g_rle_blit_calls] =
            (rle_stream != 0) ? *(uint8 *)rle_stream : 0;
    }
    if (g_rle_blit_log_on && g_rle_blit_calls < 64) {
        g_rle_blit_log_sprite[g_rle_blit_calls] = rle_stream;
        g_rle_blit_log_dst[g_rle_blit_calls] = dst_buf;
        g_rle_blit_log_stride[g_rle_blit_calls] = stride;
        g_rle_blit_log_palette[g_rle_blit_calls] = palette_op;
    }
    g_rle_blit_calls++;
}
/* fd2_render_signed_modifier_with_icon: now emitted in src/gfx/rndstat.c. The
 * rndstat HUD tests drive the real function (through its only caller
 * fd2_render_terrain_info_hud_panel), observing the sign-icon blit and the
 * 2-digit magnitude glyphs end-to-end via the g_rle_blit_log_* per-call log
 * against the fake sheet (table[i]=i). The former (dst,stride,modifier)
 * recording stub was removed. */
/* fd2_scroll_text_screen_up_by_lines: now emitted in src/dialog/dialog.c. Its
 * own dual-mode + cylinder-scroll behavior is covered by the dialog tests; the
 * rsrc caller test (chapter 0x17) drives the real tail call against a bounded
 * static_bg buffer. The former (calls, last_arg) recording stub was removed. */
void fd2_play_palette_fade_in(void) { }
void fd2_play_death_animation_and_mark_dead(void) { }

/* Turn-cycle display/dispatch callees driven by fd2_run_full_turn_cycle.
 * fd2_fire_chapter_turn_events_for_phase is now emitted for real in
 * src/battle/btl_turn.c; its former counting stub was removed and the
 * caller tests drive the real dispatcher via an in-memory tile-event
 * table + spy handlers (see tests/battle/btl_turn.c).
 * fd2_maybe_load_speed_mode_overlay / fd2_maybe_free_speed_mode_overlay:
 * now emitted in src/ui_menu/menucfg.c. */
int g_phase_banner_slide_in_calls = 0;
int g_phase_banner_slide_out_calls = 0;
void fd2_animate_phase_banner_slide_in(uint32 banner_sprite_id) {
    g_phase_banner_slide_in_calls++;
    (void)banner_sprite_id;
}
void fd2_animate_phase_banner_slide_out(uint32 banner_sprite_id) {
    g_phase_banner_slide_out_calls++;
    (void)banner_sprite_id;
}
/* fd2_process_battle_drop_entries: now emitted for real in
 * src/battle/btl_turn.c; its former noop stub here was removed. The
 * battle/btl_turn.c drop tests drive the real function (control-flow gates +
 * type-2 chapter-event dispatch); the type-0/1 display sequences are deferred
 * to Phase 9 integration. */
void fd2_cast_group_hp_heal_spell(uint32 a, uint32 b, uint32 c, uint32 d) { }
int g_cast_status_cure_calls = 0;
void fd2_cast_status_cure_spell(uint32 a, uint32 b, uint32 c, uint32 d, uint32 e) { g_cast_status_cure_calls++; (void)a; (void)b; (void)c; (void)d; (void)e; }
void fd2_cast_status_inflict_spell(uint32 a, uint32 b, uint32 c, uint32 d, uint32 e) { }
int g_cast_status_via_d1b_calls = 0;
void fd2_cast_status_spell_via_d1b(int a, int b, int c, int d, int e) { g_cast_status_via_d1b_calls++; (void)a; (void)b; (void)c; (void)d; (void)e; }
/* fd2_render_mini_char_status_panel @ 0x18c6d: now emitted for real in
 * src/gfx/rndstat.c and linked. Its callers' tests (fd2_flash_char_hit_sprite
 * in tests/battle/battle2.c) drive the real painter via the shared mini-panel
 * fixture (tests/include/minipfix.h) and observe the forwarded buf/char through
 * the real background blit + sleep-indicator digit, so no stub/spy is kept. */
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
/* fd2_run_full_turn_cycle: now emitted in src/battle/btl_turn.c */
/* fd2_enemy_turn_action_dispatcher: now in btl_ai.c */
/* fd2_ai_score_offensive_spell: now in btl_ai.c */
/* fd2_build_usable_spell_list: now real in src/spell/spellsel.c. Tests drive it
 * via the queried unit's spells_known_bitmap (+0x1A) so the real enumerator
 * produces the desired (count, ascending ids). */
/* fd2_score_spell_candidate: now in btl_ai.c */
double data_fd2_battle_ai_enemy_spell_score_multiplier_15 = 1.5;
/* AoE radial-scatter geometry constants (fd2_scatter_sprite_around_origin_with_random_offset).
 * The deg->rad factor is the binary's exact stored literal 0.0174532 (a 7-digit
 * approximation of pi/180, byte pattern af99d76c40df913f @ 0x501F8), NOT full-precision
 * pi/180. The Y skew is -8.0 @ 0x50200. */
double data_fd2_graphics_radian_per_degree_const = 0.0174532;
double data_fd2_graphics_scatter_y_offset_neg8 = -8.0;
/* fd2_ai_score_item_use: now in btl_ai.c */
/* fd2_count_usable_inventory_slots: now REAL in src/ui_menu/status.c */
/* inline action submenu dispatch seams (fd2_player_inline_action_menu_dispatch).
 * The real spell/item submenus are heavy UI/graphics orchestrators not yet
 * emitted; these stubs let the inline action dispatcher be driven to each
 * selection branch deterministically. (The field tile-event handler is now the
 * real fd2_handle_tile_event_interaction in src/ui_menu/menufld.c.) */
int g_inline_spell_menu_return = 1;
int g_inline_spell_menu_calls = 0;
/* On a committed cast the real submenu accrues spell XP into pending_xp_credit;
 * the inline dispatcher then scales it by the AP divisor. Model that here so the
 * scaling can be observed: when committing (return != -1) write this amount. */
int g_inline_spell_menu_pending = 0;
int fd2_spell_selection_menu_main(uint32 caster_idx)
{
    (void)caster_idx;
    g_inline_spell_menu_calls++;
    if (g_inline_spell_menu_return != -1) {
        data_fd2_battle_pending_xp_credit = (uint32)g_inline_spell_menu_pending;
    }
    return g_inline_spell_menu_return;
}
/* fd2_equip_unequip_inventory_menu is now emitted for real in
 * src/ui_menu/status.c and linked; its former no-op stub was removed. The
 * Sort/Equip modal is a heavy inventory-equip UI loop with no in-process input
 * seam (its end-to-end behavior is deferred to Phase 9 — see the deferral note
 * in tests/ui_menu/status.c), so no test drives it directly; the real body now
 * satisfies the link. Its equip-decision callee fd2_check_job_can_equip_item
 * (0x1C1C3) is now emitted for real in src/ui_menu/status.c; its former no-op
 * stub was removed. */
/* fd2_item_command_menu_dispatch is now emitted for real in
 * src/ui_menu/status.c and linked; its former counting stub (and the
 * g_inline_item_menu_return / g_inline_item_menu_calls seams that drove it)
 * were removed. The inline-action dispatcher's Item-branch behavioral coverage
 * (case-2 commit / cancel) is a heavy-UI input-loop path — the real item
 * command menu opens its own settings dialog and blocks on a keyboard read
 * whose buffer the caller's close already cleared — so it is deferred to
 * Phase 9 integration, the same deferral the file applies to the other
 * non-isolable heavy-UI submenus. */
/* fd2_handle_tile_event_interaction is now emitted for real in
 * src/ui_menu/menufld.c and linked; its former counting stub was removed. The
 * inline-action dispatcher's Wait-branch tests now drive the real handler, which
 * gate-returns on a non-event cursor tile (see tests/ui_menu/menu.c iam_setup's
 * zeroed tile-map / attr buffers). Its own behavioral coverage lives in
 * tests/ui_menu/menufld.c. */
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
/* fd2_composite_battle_frame is now a real emitted function (src/gfx/rndscene.c).
 * g_composite_call_count (defined above with the pipeline stubs) remains the
 * observable that existing caller tests (cursor.c, spelleff.c, btl_ai.c, ...)
 * use to count "a composite frame ran"; the real compositor calls
 * fd2_composite_battle_tile_map exactly once per frame, so the tile-map stub
 * bumps it. */

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
uint32 data_fd2_graphics_bg_anim_flip_flag = 0;
uint8  data_fd2_graphics_tile_anim_palette_phase_lookup[20] = {0};
/* fd2_add_item_to_inventory is now emitted for real in src/ui_menu/status.c
 * (and covered there by the test_add_item_* cases). The battle-drop suite that
 * once used the g_add_item_* spy now asserts on the real inventory state. */
/* fd2_inventory_selection_modal_dispatch and fd2_inventory_grid_input_step are
 * both now emitted for real in src/ui_menu/status.c. The grid-input step is
 * covered directly by the test_grid_input_* cases in tests/ui_menu/status.c,
 * which inject scancodes through the BIOS keyboard buffer and exercise the real
 * fd2_wait_for_input_dialog_with_blink. The modal dispatcher's input-loop tests
 * are deferred to Phase 9 integration: its fd2_open_status_screen_with_slide_in
 * clears the keyboard buffer before the loop, so the real wait can only be
 * released by async keyboard input (see tests/ui_menu/status.c). Both functions'
 * previous recording / sequence fakes were removed. */
/* fd2_play_sfx_sample_from_bank is now a real emitted function
 * (src/audio/audio.c); its former counting stub here was removed (mirrors the
 * fd2_play_sfx_with_handle relocation above). It is structurally identical to
 * fd2_play_sfx_with_handle except it drives sample slot 1
 * (data_fd2_audio_sfx_sample_handle_1) instead of slot 0. Both real players run
 * their gates then unconditionally call AIL_stop_sample(<their handle>) once per
 * invocation, so the AIL_stop_sample spy discriminates the two by the handle
 * value it receives: handle_1 -> g_play_sfx_sample_from_bank_calls, otherwise
 * (handle_0) -> g_play_sfx_with_handle_calls / g_dlg_blink_calls. Callers that
 * count both (e.g. summon variant b/d/e ticks) leave the handle globals at their
 * distinct testglob defaults, so the seam routes each caller's calls to the
 * right counter. (Counter defined near the top of this file, ahead of the AIL
 * spy that increments it.) */
void fd2_paint_char_sprite_at_world_with_mode(uint32 w, uint32 s, uint32 c, uint32 m, uint32 co) { }
/* Pathfind stub. Behavior is selected by the `md` (mode) arg:
 *   md==2  -> "find optimal reachable cell" call (fd2_ai_seek_optimal_position).
 *            When g_pathfind_write_dst!=0 it writes the discovered destination
 *            (g_pathfind_dst_x, g_pathfind_dst_y) into the db output buffer, and
 *            returns g_pathfind_return (the step/0xFF code).
 *   md==0/1 -> "route toward a specific target" call (inside
 *            fd2_ai_walk_to_target_tile). Returns g_pathfind_walk_return.
 * This separation lets a seek-position test pin the seek's pathfind result and
 * reported destination independently of the walk routine's own return value,
 * which is required to lock in the EAX-tracking semantics of did_move.
 *
 * Sequenced mode (g_pathfind_seq_enable != 0, default OFF so every existing test
 * keeps the single-value behavior above): fd2_ai_walk_to_target_tile issues THREE
 * sequential pathfinds in a fixed order -- Stage A (md==0), Stage B (md==1), and
 * the final route (md==0). A walk test that must drive distinct outcomes per call
 * (e.g. Stage A unreachable 0xFF -> Stage B succeeds -> final route) scripts the
 * per-call return codes in g_pathfind_seq[0..3] indexed by call order, and injects
 * a deterministic Stage B step-byte path (g_pathfind_step_bytes, length
 * g_pathfind_seq_steps) into the md==1 db buffer so the routine's step-decode +
 * furthest-walkable scan runs over known data.
 *
 * Always (both modes): the destination (f1,f2) of the LAST md==0 call is recorded
 * in g_pathfind_md0_dst_x/y, letting a test observe which tile the routine finally
 * routed to (the chosen "best adjacent tile"). */
int g_pathfind_return = 0;
int g_pathfind_walk_return = 0;
int g_pathfind_write_dst = 0;
int g_pathfind_dst_x = 0;
int g_pathfind_dst_y = 0;
int g_pathfind_seq_enable = 0;
int g_pathfind_seq[4] = { 0, 0, 0, 0 };
int g_pathfind_seq_idx = 0;
int g_pathfind_seq_steps = 0;
uint8 g_pathfind_step_bytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int g_pathfind_md0_dst_x = -1;
int g_pathfind_md0_dst_y = -1;
int fd2_pathfind_to_destination(uint32 ct, uint32 sx, uint32 sy, uint32 ms,
    uint32 db, uint32 f1, uint32 f2, uint32 md, uint32 tm, uint32 af) {
    (void)ct; (void)sx; (void)sy; (void)ms; (void)tm; (void)af;
    if (md == 0) {
        g_pathfind_md0_dst_x = (int)f1;
        g_pathfind_md0_dst_y = (int)f2;
    }
    if (g_pathfind_seq_enable != 0) {
        int idx;
        int rc;
        int k;
        idx = g_pathfind_seq_idx;
        if (idx > 3) idx = 3;
        rc = g_pathfind_seq[idx];
        if (md == 1 && db != 0) {
            for (k = 0; k < g_pathfind_seq_steps && k < 8; k++) {
                ((uint8 *)db)[k] = g_pathfind_step_bytes[k];
            }
        }
        g_pathfind_seq_idx++;
        return rc;
    }
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

/* --- menu.c dispatch-target stubs + tracking --- */
/* fd2_field_command_menu_loop is now emitted for real in ui_menu/menu.c.
 * Its menu-subsystem callees are stubbed below so its tests can drive each
 * dispatch branch by setting the input/cursor/dialog-result seams. */

/* fd2_settings_menu_input_step is now a real emitted function
 * (src/ui_menu/menucfg.c); its former stub and the g_settings_* seam variables
 * were removed. Tests that drive a menu loop (fd2_game_options_menu_loop,
 * fd2_field_command_menu_loop) now stage real scancodes into the BIOS keyboard
 * ring via tests/include/menufix.h, so the real input-step path (which reads
 * the key through the real fd2_wait_input_with_dialog_repaint) runs end to end.
 * fd2_open_settings_dialog_with_slide / fd2_close_settings_dialog_with_slide
 * are likewise real; tests observe g_blitsetup_calls (16 corner blits each) to
 * confirm the dialog opened/closed. */
/* fd2_field_menu_status_save_load_quit_dispatch is now emitted for real in
 * src/ui_menu/menufld.c; its former one-shot stub and the
 * g_save_load_quit_dispatch_* seam variables were removed. The
 * fd2_field_command_menu_loop cursor-0 path now drives the real dispatch end to
 * end: a staged BIOS-keyboard Esc cancels its settings sub-menu (the real
 * input-step path), so it returns 0 and the loop propagates that verbatim
 * (the EAX-passthrough). FD2.SAV is staged into the test cwd by build_test.py,
 * so the dispatch's real fopen("FD2.SAV","rb") probe reads it. */
/* fd2_open_party_status_overview_screen (the cursor-0 Status arm of the
 * save/load/quit dispatch) is now emitted for real in src/ui_menu/status.c
 * (with a host smoke test in tests/ui_menu/status.c); its former no-op
 * recording stub here was removed. The menufld.c dispatch tests cover only
 * the read-only Esc-cancel and menu-state gating in the setup phase and never
 * reach the cursor-0 Status path, so the real link-in is inert for them. */
/* fd2_text_dialog_typewriter_loop is now emitted for real in src/dialog/dialog.c
 * (driven by the test_typewriter_* cases in tests/dialog/dialog.c, which preload
 * the BIOS keyboard buffer so its INT 16h dispatch returns at once). Its former
 * recording stub (g_typewriter_loop_return / _calls) was removed. Caller paths
 * that reach it through the real blocking busy-wait (menufld.c's non-gate paths)
 * are deferred to Phase 9 integration, where real keyboard input releases the
 * loop — the same deferral the file already applies to its
 * fd2_wait_for_input_dialog_with_blink gold/item paths. */
/* fd2_animate_dialog_page_advance_collapse is now emitted for real in
 * src/dialog/dialog.c; its former recording stub here was removed. Caller
 * tests (menufld.c) drive the real function with the battle-tile-map gate ON
 * (so its scene-prime runs one fd2_composite_battle_tile_map) and observe its
 * effect through the recording g_composite_call_count proxy. */
/* fd2_game_options_menu_loop is now emitted for real in ui_menu/menucfg.c. */
/* fd2_player_action_menu_loop is now emitted for real in ui_menu/menu.c; its
 * former one-shot stub and the g_player_action_menu_loop_* seam variables were
 * removed. The fd2_game_main_loop player-action path now drives the real
 * function (behavioral coverage deferred to Phase 9 integration). */
/* fd2_player_inline_action_menu_dispatch is now emitted for real in
 * ui_menu/menu.c; its former one-shot stub and the g_inline_dispatch_* seam
 * variables were removed. Its spell/item submenu callees are stubbed above
 * (g_inline_spell_menu_* / g_inline_item_menu_*); its tile-event callee is now
 * the real fd2_handle_tile_event_interaction (src/ui_menu/menufld.c). */
/* fd2_open_char_status_screen: now emitted in src/ui_menu/status.c and linked
 * for real (was a recording stub here). It is pure VGA/sfx orchestration and is
 * never reached by a host test — fd2_game_main_loop (its sole in-tree caller)
 * is not exercised — so its behavioral coverage is deferred to Phase 9. */
int g_open_tactical_overview_zoom_calls = 0;
void fd2_open_tactical_overview_zoom(void) {
    g_open_tactical_overview_zoom_calls++;
}
/* Recording stub for fd2_restore_screen_block_from_buffer (the screen-block
 * restore blitter, not yet emitted). fd2_cleanup_dialog_sprite_buffer must
 * forward its (saved_block, dst, stride) args to this in order, then free
 * saved_block. The stub captures the args so the cleanup test can assert the
 * forwarding without touching real VGA memory. */
int    g_restore_block_calls = 0;
uint32 g_restore_block_last_buf = 0;
uint32 g_restore_block_last_dst = 0;
uint32 g_restore_block_last_stride = 0;
void fd2_restore_screen_block_from_buffer(uint32 saved_block, uint32 dst, uint32 stride) {
    g_restore_block_calls++;
    g_restore_block_last_buf = saved_block;
    g_restore_block_last_dst = dst;
    g_restore_block_last_stride = stride;
}

/* ---- fd2_display_dialog_scene (dialog VM) support ----
 * Globals it reads/writes (not yet defined elsewhere) and display-side-effect
 * callees stubbed to noop, with a recording stub for the glyph blitter and a
 * call counter for the blink-animation step so the VM's render-position
 * arithmetic and opcode dispatch can be asserted without touching VGA / sfx.
 * fd2_check_keyboard_buffer_nonempty and fd2_wait_for_input_dialog_with_blink
 * are the REAL linked functions; the dialog-VM tests use only TEXT / -3 / -6 /
 * -1 opcodes so the busy-wait (page-break) and portrait/file-load paths are
 * never reached. With the BIOS keyboard buffer left empty (head==tail), the
 * real keyboard poll returns 0 so blink_flag stays set and the blink stub runs. */
uint32 data_fd2_dialog_last_action_sprite_id_param = 0;
uint32 data_fd2_dialog_drop_swap_text_id_param = 0;
uint8 *data_fd2_portrait_sprite_buffer = (uint8 *)0;

int    g_dlg_glyph_calls = 0;
uint32 g_dlg_glyph_last_idx = 0;
uint32 g_dlg_glyph_last_pos = 0;

void fd2_blit_glyph_2bpp_with_outline(uint32 font_sheet, uint32 glyph_idx,
                                      uint32 render_pos, uint32 render_pitch,
                                      uint32 p5, uint32 p6, uint16 p7) {
    (void)font_sheet; (void)render_pitch; (void)p5; (void)p6; (void)p7;
    g_dlg_glyph_calls++;
    g_dlg_glyph_last_idx = glyph_idx;
    g_dlg_glyph_last_pos = render_pos;
}
/* fd2_play_dialog_open_animation: now emitted in src/dialog/dialog.c and
 * linked for real; its 5-stage frame assembly is driven by the
 * test_open_anim_* cases in tests/dialog/dialog.c.
 * fd2_cinematic_scroll_text_up_for_special_scenes: now emitted in
 * src/dialog/dialog.c and linked for real (was a no-op stub here). */
int    g_dlg_blit_normal_calls = 0;
int    g_dlg_blit_mirrored_calls = 0;
uint32 g_dlg_blit_last_dst = 0;
uint32 g_dlg_blit_last_sprite = 0;
uint32 g_dlg_blit_last_stride = 0;
void fd2_dialog_sprite_blit_normal(uint32 dst, uint32 sprite, uint32 stride) {
    g_dlg_blit_normal_calls++;
    g_dlg_blit_last_dst = dst;
    g_dlg_blit_last_sprite = sprite;
    g_dlg_blit_last_stride = stride;
}
void fd2_dialog_sprite_blit_mirrored(uint32 dst, uint32 sprite, uint32 stride) {
    g_dlg_blit_mirrored_calls++;
    g_dlg_blit_last_dst = dst;
    g_dlg_blit_last_sprite = sprite;
    g_dlg_blit_last_stride = stride;
}
/* fd2_render_full_char_stat_panel @ 0x17fc0: now emitted for real in
 * src/gfx/rndstat.c and driven by tests/gfx/rndstat.c (the numeric/bar
 * render primitives it dispatches to are the recording spies defined
 * above; the icon/team blits reach the real fd2_blit_sheet_sprite_at_offset
 * -> g_blitraw log; the three text-label fd2_display_dialog_scene calls run
 * for real against a minimal immediate-END text program). */
/* fd2_close_dialog_panels_then_slide_in_at: now emitted in
 * src/dialog/dialog.c and linked for real; its teardown + slide-out
 * interpolation is driven by the test_close_* cases in
 * tests/dialog/dialog.c (observed via the restore/save/blit-setup stubs). */

/* ---- fd2_open_char_status_screen / fd2_open_status_screen_with_slide_in
 * (status.c) support ----
 * The status-screen modals are pure VGA/sfx orchestration: every callee below
 * only blits/animates, and the functions themselves memmove to/from physical
 * VRAM (0xA0000). They are therefore deferred to Phase 9 integration and are
 * not driven by a host unit test; these noop stubs only satisfy the linker for
 * the not-yet-emitted display callees they reference. */
/* fd2_render_status_screen_static_layout: now emitted for real in
 * src/gfx/rndstat.c; deferred to Phase 9 integration for its dedicated test
 * (see src/emit_issues.json @00017eef). */
/* fd2_render_inventory_item_grid: now emitted for real in src/gfx/rndstat.c
 * (with host unit tests in tests/gfx/rndstat.c); stub removed. */
/* fd2_paint_status_panel_layer_left / _right: both now emitted for real in
 * src/gfx/rndstat.c (with host unit tests); stubs removed. */
/* fd2_play_status_screen_outro_step: now emitted for real in src/anim/aniwalk.c
 * (with host unit tests in tests/anim/aniwalk2.c driving the real panel
 * painters over in-memory buffers); stub removed. */
void fd2_draw_spell_selection_list(uint32 char_idx, uint32 spell_idx,
                                   uint32 overlay_buffer) {
    (void)char_idx; (void)spell_idx; (void)overlay_buffer;
}

/* fd2_render_party_status_overview_content: now emitted for real in
 * src/gfx/rndstat.c (with host unit tests in tests/gfx/rndstat.c driving the
 * real sprite-sheet / decimal / dialog renderers over in-memory fixtures);
 * recording stub removed. The army-overview orchestrator test in
 * tests/ui_menu/status.c now stands up a minimal sprite-sheet + immediate-END
 * text fixture so the real content renderer runs safely. */

/* fd2_count_active_chars_for_team_filter: now emitted for real in
 * src/battle/btl_turn.c (it scans g_test_rc_array via
 * data_fd2_battle_runtime_char_array_ptr and the fd2_check_char_is_dead stub);
 * recording fake removed. The content-renderer tests in tests/gfx/rndstat.c
 * now seed g_test_rc_array with a known per-team alive distribution so the
 * real counter feeds the per-team decimal renders. */

/* Fake for the remaining unemitted party-query callee of the content renderer
 * (fd2_check_party_has_char_id -> src/util/misc.c). Tests set the return value
 * directly. */
uint32 g_has_char_fake = 0;
uint32 g_has_char_last_arg = 0;
int    g_has_char_calls = 0;
uint32 fd2_check_party_has_char_id(uint32 char_id) {
    g_has_char_calls++;
    g_has_char_last_arg = char_id;
    return g_has_char_fake;
}
