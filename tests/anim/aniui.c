/*
 * unit tests for src/anim/aniui.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

#define USE_ITEM_ID 10

extern runtime_char g_test_rc_array[8];
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;
extern uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
extern int g_ending_menu_return;
extern int g_slot_selector_return;
extern int g_chapter_transition_return;
extern int g_play_sfx_with_handle_calls;
extern int g_play_sfx_sample_from_bank_calls;
extern int g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int g_blit_indexed_sprite_last_x;
extern int g_blit_indexed_sprite_last_y;
extern int g_composite_call_count;
extern int g_attack_dispatch_return;
extern int g_attack_dispatch_calls;
extern int g_seek_optimal_return;
extern int g_advance_nearest_return;
extern int g_walk_return;
extern int g_score_physical_return;
extern int g_pass_turn_calls;
extern int g_execute_spell_calls;
extern int g_execute_physical_calls;
extern int g_pathfind_return;
extern int g_pathfind_walk_return;
extern int g_pathfind_write_dst;
extern int g_pathfind_dst_x;
extern int g_pathfind_dst_y;
extern int g_pathfind_seq_enable;
extern int g_pathfind_seq[4];
extern int g_pathfind_seq_idx;
extern int g_pathfind_seq_steps;
extern uint8 g_pathfind_step_bytes[8];
extern int g_pathfind_md0_dst_x;
extern int g_pathfind_md0_dst_y;
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;

/* screen-shake observability (testglob.c):
 *   - the per-frame delay thunk records call count + last tick value
 *   - the tile-map composite stub counts every composite pass (the function's
 *     own mode-9 snapshot + the one inside the real fd2_composite_battle_frame)
 *   - g_composite_call_count mirrors that count */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;
extern int    g_tile_map_calls;
extern int    g_composite_call_count;


/* Backing for data_fd2_large_game_state_buffer_ptr. The real fd2_blit_rectangle
 * (and the real fd2_composite_battle_frame finalizer) read the visible region
 * from workspace = base + 0x8088, up to row 0xC0 at stride 0x1C8 starting at
 * +0x8088 (+0x1C8 for odd iterations): max read offset = 0x8088 + 0xC0*0x1C8 +
 * 0x138 < 0x1E000. The VGA-side write to 0xA0504 lands in emulated video memory
 * and is harmless (write-only). */
#define SHAKE_LGS_SPAN 0x1E000u
static uint8 g_shake_lgs[SHAKE_LGS_SPAN];

/* Make fd2_composite_battle_frame(0) host-safe: HUD gated off (early-return),
 * cursor phase 0 (no overlay blit), empty party (no per-char paint), palette
 * cycle throttled to early-return. */
static void shake_setup(void)
{
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    g_tile_map_calls = 0;
    g_composite_call_count = 0;

    memset(g_shake_lgs, 0, sizeof(g_shake_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_shake_lgs;

    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x20;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;

    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;
    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_party_member_count = 0;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;
}


static void test_tick_tutorial_sfx_counter(void)
{
    uint8 save_ctr;

    save_ctr = data_fd2_audio_walk_step_sfx_cadence_counter;
    data_fd2_audio_walk_step_sfx_cadence_counter = 5;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].job_id = 1;

    fd2_tick_tutorial_progress_with_sfx(0);
    ASSERT_EQ(data_fd2_audio_walk_step_sfx_cadence_counter, 6);

    data_fd2_audio_walk_step_sfx_cadence_counter = save_ctr;
}


/*
 * fd2_animate_screen_shake — loop count + per-frame structure.
 *
 * The function snapshots the scene once (a mode-9 tile-map composite) + one
 * finalizer composite, then loops num_frames times, each frame blitting the
 * visible region to the mode13h primary with a 0x14-tick delay. The composite
 * count is frame-count-independent (always 2: the direct snapshot + the
 * finalizer's single tile-map pass); the delay count and per-frame blit count
 * equal num_frames.
 *
 * Verified here (host-safe observables): the loop iterates exactly num_frames
 * times (delay-thunk call count), the per-frame delay constant (0x14), and the
 * fixed two-composite preamble. The per-frame 1-row vertical-jitter source
 * alternation (i & 1) * 0x1C8 only manifests as a write to the mode13h primary
 * at the absolute VGA address 0xA0504 (the real fd2_blit_rectangle target);
 * VGA memory is write-only here (read-back is unreliable, no host buffer can
 * back the fixed 0xA0504), so the resulting pixel jitter is deferred to Phase 9
 * integration (screen-buffer comparison), consistent with the other VGA-output
 * side-effects (palette-DAC writes, the composite finalizer's own primary blit).
 */
static void test_screen_shake_loop_and_jitter(void)
{
    /* odd frame count */
    shake_setup();
    fd2_animate_screen_shake(3);
    ASSERT_EQ(g_delay375b2_calls, 3);
    ASSERT_EQ(g_delay375b2_last_ticks, 0x14u);
    ASSERT_EQ(g_tile_map_calls, 2);
    ASSERT_EQ(g_composite_call_count, 2);

    /* even frame count */
    shake_setup();
    fd2_animate_screen_shake(4);
    ASSERT_EQ(g_delay375b2_calls, 4);
    ASSERT_EQ(g_delay375b2_last_ticks, 0x14u);
    ASSERT_EQ(g_tile_map_calls, 2);
    ASSERT_EQ(g_composite_call_count, 2);

    /* single frame: one delay, one blit */
    shake_setup();
    fd2_animate_screen_shake(1);
    ASSERT_EQ(g_delay375b2_calls, 1);
    ASSERT_EQ(g_delay375b2_last_ticks, 0x14u);
    ASSERT_EQ(g_tile_map_calls, 2);

    /* zero frames: snapshot + finalizer composite still run, but no blit/delay.
     * Signed loop guard (int32)i < (int32)num_frames keeps num_frames==0 (and
     * any high-bit value) from looping. */
    shake_setup();
    fd2_animate_screen_shake(0);
    ASSERT_EQ(g_delay375b2_calls, 0);
    ASSERT_EQ(g_tile_map_calls, 2);
    ASSERT_EQ(g_composite_call_count, 2);
}


void run_anim_aniui_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/aniui\n");
    RUN_TEST(test_tick_tutorial_sfx_counter);
    RUN_TEST(test_screen_shake_loop_and_jitter);
    printf("\n");
}
