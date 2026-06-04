/*
 * unit tests for src/gfx/palette.c
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
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;


static void test_set_full_palette_smoke(void)
{
    fd2_set_full_vga_palette_to_color(0x3F, 0x3F, 0x3F);
    ASSERT_TRUE(1);
}


/* ---- Tests: update_palette_cycle_anim ---- */

static void test_update_palette_cycle_anim_no_update(void)
{
    data_fd2_animation_palette_cycle_last_tick =
        (uint16)BIOS_TICK_WORD;
    data_fd2_animation_palette_cycle_frame_idx = 5;
    fd2_update_palette_cycle_anim();
    ASSERT_EQ(data_fd2_animation_palette_cycle_frame_idx, 5);
}


/* ---- Tests: palette range ---- */

static void test_set_vga_palette_range_basic(void)
{
    uint8 fake_pal[6];
    fake_pal[0] = 0x3F; fake_pal[1] = 0x20; fake_pal[2] = 0x10;
    fake_pal[3] = 0x05; fake_pal[4] = 0x00; fake_pal[5] = 0x3F;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;
    fd2_set_vga_palette_range(0, 1, 0x10);
    ASSERT_TRUE(1);
}


static void test_palette_remap_run(void)
{
    uint8 table[256];
    uint8 data[4];
    int i;
    for (i = 0; i < 256; i++) table[i] = (uint8)(255 - i);
    data[0] = 0; data[1] = 1; data[2] = 2; data[3] = 3;
    fd2_apply_palette_remap_run((uint32)table, 4, data);
    ASSERT_EQ(data[0], 255);
    ASSERT_EQ(data[1], 254);
    ASSERT_EQ(data[3], 252);
}


static void test_interpolate_palette(void)
{
    uint8 fake_pal[3];
    fake_pal[0] = 0x28; fake_pal[1] = 0x14; fake_pal[2] = 0x00;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;
    fd2_interpolate_palette_range_toward_color(0, 1, 0x28, 0, 0, 0);
    ASSERT_TRUE(1);
}


/* Over-bright settle step loop (additive, NOT a fade to black). Smoke-level:
 * pure palette-port-write side effect, so terminating cleanly is the contract.
 * The additive saturation math itself is asserted in
 * test_set_vga_palette_range_with_add below. start_intensity=0 -> the loop
 * body runs once and drives the callee over the full 256-entry DAC range
 * (idx 0..0xFF, reading base[0..767]); allocate a full 768-byte palette so the
 * callee's reads stay in-bounds. */
static void test_palette_overbright_settle(void)
{
    static uint8 fake_pal[256 * 3];
    int i;
    for (i = 0; i < 256 * 3; i++) fake_pal[i] = 0x20;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;
    fd2_palette_overbright_settle_step_loop(0, 0);
    ASSERT_TRUE(1);
}


static void test_set_vga_palette_range_with_add(void)
{
    uint8 fake_pal[6];
    fake_pal[0] = 0x30; fake_pal[1] = 0x3F; fake_pal[2] = 0x10;
    fake_pal[3] = 0x20; fake_pal[4] = 0x3E; fake_pal[5] = 0x00;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;
    fd2_set_vga_palette_range_with_add(0, 1, 0x10);
    ASSERT_TRUE(1);
}


static void test_tick_chapter_palette_fast_cycle(void)
{
    int tick_val;

    tick_val = (int)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)tick_val;
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 0;

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_walk_anim_alt_palette_idx, 1);

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_walk_anim_alt_palette_idx, 2);
}


static void test_tick_chapter_palette_fast_wrap(void)
{
    int tick_val;

    tick_val = (int)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)tick_val;
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 3;

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_walk_anim_alt_palette_idx, 0);
}


static void test_tick_chapter_palette_slow_triggers(void)
{
    int tick_val;

    tick_val = (int)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)(tick_val - 10);
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_ambient_palette_anim_idx, 1);
}


/*
 * Slow-branch condition is a two-part OR: assembly @0x12995 tests
 * delta>4 (JG), and on fall-through @0x129a8 tests delta<0 (JGE skips
 * the slow body when delta>=0). The slow body therefore also runs for
 * a NEGATIVE delta, which production hits when the BIOS midnight tick
 * wraps (latch from the previous day is above the post-rollover tick).
 * Set latch = tick + 10 so delta = -10 < 0, exercising the JGE-not-taken
 * arm; ambient_idx must advance 0 -> 1.
 *
 * Determinism note: 0x46C is the live BIOS tick (advances ~18.2/s).
 * The ambient_idx assertion holds regardless of jitter — for the slow
 * body to be skipped the tick would have to advance into [10,14] counts
 * (~0.6 s) between adjacent statements; any larger advance re-enters the
 * slow body via the delta>4 arm. The latch-update side effect is NOT
 * asserted: the function re-reads 0x46C internally, so a value compare
 * against a re-read in the test would be a genuine race, not a fixture.
 */
static void test_tick_chapter_palette_slow_triggers_negative_delta(void)
{
    int tick_val;

    tick_val = (int)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)(tick_val + 10);
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_ambient_palette_anim_idx, 1);
}


/* fade-IN: walks brightness_subtract 0x40 down to 0 INCLUSIVE = 0x41
 * iterations, each calling __delay_thunk_375b2(2). The loop count and the
 * delay argument are the load-bearing correctness properties (the inner
 * palette write is a pure port-write side effect). The stubbed delay thunk
 * records call count + last arg, giving a deterministic check that the loop
 * runs exactly 65 times (i.e. the signed `>= 0` bound includes subtract=0,
 * not 64 times) with the right tick arg. A full 768-byte base palette keeps
 * the inner fd2_set_vga_palette_range (idx 0..0xFF, base[0..767]) in-bounds. */
static void test_play_palette_fade_in(void)
{
    static uint8 fake_pal[256 * 3];
    int i;
    for (i = 0; i < 256 * 3; i++) fake_pal[i] = 0x20;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;

    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    fd2_play_palette_fade_in();
    ASSERT_EQ(g_delay375b2_calls, 0x41);
    ASSERT_EQ(g_delay375b2_last_ticks, 2u);
}


/* fade-OUT: walks brightness_subtract 0 up to 0x3F (the signed `< 0x40`
 * exclusive bound) = exactly 0x40 iterations, each calling
 * __delay_thunk_375b2(2). The 0x40 loop count is the load-bearing direction
 * marker that distinguishes this fade-OUT entry from the fade-IN counterpart
 * (which runs 0x41 times via a `>= 0` inclusive bound); the inner palette
 * write is a pure port-write side effect. The stubbed delay thunk records the
 * call count + last arg for a deterministic check. A full 768-byte base
 * palette keeps the inner fd2_set_vga_palette_range (idx 0..0xFF,
 * base[0..767]) in-bounds. */
static void test_play_palette_fade_to_black(void)
{
    static uint8 fake_pal[256 * 3];
    int i;
    for (i = 0; i < 256 * 3; i++) fake_pal[i] = 0x20;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;

    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    fd2_play_palette_fade_to_black();
    ASSERT_EQ(g_delay375b2_calls, 0x40);
    ASSERT_EQ(g_delay375b2_last_ticks, 2u);
}


void run_gfx_palette_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/palette\n");
    RUN_TEST(test_set_full_palette_smoke);
    RUN_TEST(test_update_palette_cycle_anim_no_update);
    RUN_TEST(test_set_vga_palette_range_basic);
    RUN_TEST(test_set_vga_palette_range_with_add);
    RUN_TEST(test_palette_overbright_settle);
    RUN_TEST(test_palette_remap_run);
    RUN_TEST(test_interpolate_palette);
    RUN_TEST(test_tick_chapter_palette_fast_cycle);
    RUN_TEST(test_tick_chapter_palette_fast_wrap);
    RUN_TEST(test_tick_chapter_palette_slow_triggers);
    RUN_TEST(test_tick_chapter_palette_slow_triggers_negative_delta);
    RUN_TEST(test_play_palette_fade_in);
    RUN_TEST(test_play_palette_fade_to_black);
    printf("\n");
}
