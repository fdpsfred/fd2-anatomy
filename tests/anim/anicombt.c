/*
 * unit tests for src/anim/anicombt.c
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* runtime-char array backing (testglob.c) */
extern runtime_char g_test_rc_array[8];

/* solid-colour blit recording (testglob.c): shared g_blitpass_* plus the
 * per-call colour log and a dedicated call counter. */
extern int    g_blitpass_calls;
extern uint32 g_blitpass_src[64];
extern uint32 g_blitpass_dst[64];
extern uint32 g_blitpass_stride[64];
extern int    g_blitsolid_calls;
extern uint32 g_blitsolid_color[64];

/* SFX-play recording (testglob.c) */
extern int    g_play_sfx_with_handle_calls;
extern int    g_sfx_last_id;
extern int    g_sfx_id_count;
extern int    g_sfx_id_log[64];

/* decoded-pixel sprite blit recording (testglob.c) */
extern uint32 g_blitdec_dst, g_blitdec_sprite, g_blitdec_stride;
extern int    g_blitdec_calls;

/* per-spell animation parameter tables (testglob.c, real binary bytes) */
extern uint8  data_fd2_animation_spell_sprite_offset_table[33];
extern uint8  data_fd2_animation_spell_frame_count_table[33];
extern uint8  data_fd2_animation_spell_sfx_frame_table[33];

/* Battle back-buffer backing. The real flicker body memmoves 0x25680 bytes
 * out of data_fd2_large_game_state_buffer_ptr and the real fd2_blit_rectangle
 * reads from +0x8088, so the backing must span the whole 0x25680 snapshot. */
#define LGS_SPAN 0x26000u
static uint8 g_lgs[LGS_SPAN];

/* Portrait sprite cache: a dword absolute-offset table. table[i] == i*0x100 so
 * the recorded src pointer (cache + table[frame_idx]) uniquely identifies the
 * resolved frame index. */
static uint8 g_portrait_cache[256 * 4];

#define WIN_OX  0x10u
#define WIN_OY  0x20u
#define WIN_MX  0x0Du   /* x window: [OX-1, OX+MX]      = [0x0F, 0x1D] */
#define WIN_MY  0x08u   /* y window: [OY-1, OY+MY+1]    = [0x1F, 0x29] */

static void setup_overlay(uint32 palette_idx)
{
    int i;
    uint32 *table;

    g_blitpass_calls = 0;
    g_blitsolid_calls = 0;
    g_play_sfx_with_handle_calls = 0;

    table = (uint32 *)g_portrait_cache;
    for (i = 0; i < 256; i++) {
        table[i] = (uint32)i * 0x100u;
    }
    portrait_sprite_cache = (uint32)g_portrait_cache;

    memset(g_lgs, 0, sizeof(g_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgs;

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;
    data_fd2_graphics_chapter_ambient_palette_anim_idx = palette_idx;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
}

/*
 * One in-window char and one out-of-window char. Verifies the window-cull
 * predicate (only the in-window char blits), the dst screen-position
 * arithmetic, the frame-source arithmetic, and the colour argument.
 * palette_idx = 1 (not 3) takes the frame_idx = frame_off + palette branch.
 */
static void test_overlay_cull_and_arithmetic(void)
{
    uint8 idx_array[2];
    uint32 exp_dst;
    uint32 frame_idx;
    uint32 exp_src;

    setup_overlay(1);

    /* char 0: inside the window (pos within [OX-1..OX+MX] x [OY-1..OY+MY+1]) */
    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;
    g_test_rc_array[0].sprite_state[0] = 4;   /* frame_off = 4*0xC = 0x30 */

    /* char 1: pos_x past the right edge (OX+MX = 0x1D) -> culled */
    g_test_rc_array[1].pos_x = 0x1E;
    g_test_rc_array[1].pos_y = 0x24;
    g_test_rc_array[1].sprite_state[0] = 7;

    idx_array[0] = 0;
    idx_array[1] = 1;

    fd2_animate_status_effect_overlay_flicker(0xDEAD, 17, 2, (uint32)idx_array);

    /* exactly one status sprite drawn (char 1 culled) */
    ASSERT_EQ(g_blitsolid_calls, 1);

    /* SFX 1 played once at entry */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);

    /* dst = lgs + (pos_y-OY)*0x2AC0 + (pos_x-OX)*0x18 + 0x75D8 */
    exp_dst = (uint32)g_lgs
            + (0x24u - WIN_OY) * 0x2ac0u
            + (0x15u - WIN_OX) * 0x18u
            + 0x75d8u;
    ASSERT_EQ(g_blitpass_dst[0], exp_dst);

    /* frame_idx = sprite_state[0]*0xC + palette(=1); src = cache + table[frame_idx] */
    frame_idx = 4u * 0xcu + 1u;                 /* 0x31 */
    exp_src = (uint32)g_portrait_cache + frame_idx * 0x100u;
    ASSERT_EQ(g_blitpass_src[0], exp_src);

    /* stride arg is the fixed 0x1C8 */
    ASSERT_EQ(g_blitpass_stride[0], 0x1c8u);

    /* colour arg = template[status_kind]; status_kind 17 -> template[17] = 0x92 */
    ASSERT_EQ(g_blitsolid_color[0], 0x92u);
}

/*
 * palette_idx == 3 forces frame_idx = frame_off + 2 (clash-avoidance branch),
 * independent of the palette value. Confirms the special-case offset.
 */
static void test_overlay_palette3_offset(void)
{
    uint8 idx_array[1];
    uint32 frame_idx;
    uint32 exp_src;

    setup_overlay(3);

    g_test_rc_array[0].pos_x = WIN_OX;          /* on the left window edge */
    g_test_rc_array[0].pos_y = WIN_OY;          /* on the top window edge  */
    g_test_rc_array[0].sprite_state[0] = 2;     /* frame_off = 2*0xC = 0x18 */
    idx_array[0] = 0;

    fd2_animate_status_effect_overlay_flicker(0, 0, 1, (uint32)idx_array);

    ASSERT_EQ(g_blitsolid_calls, 1);

    /* palette==3 -> frame_idx = frame_off + 2 = 0x18 + 2 = 0x1A */
    frame_idx = 2u * 0xcu + 2u;
    exp_src = (uint32)g_portrait_cache + frame_idx * 0x100u;
    ASSERT_EQ(g_blitpass_src[0], exp_src);

    /* dst at the window origin: offsets collapse to the +0x75D8 base */
    ASSERT_EQ(g_blitpass_dst[0], (uint32)g_lgs + 0x75d8u);

    /* status_kind 0 -> template[0] = 0xC0 */
    ASSERT_EQ(g_blitsolid_color[0], 0xc0u);
}

/*
 * Lower-edge culling: a char one row above the top window edge (pos_y = OY-2,
 * below the OY-1 lower bound) is rejected, leaving zero sprites drawn while the
 * snapshot/flicker plumbing still runs to completion.
 */
static void test_overlay_cull_top_edge(void)
{
    uint8 idx_array[1];

    setup_overlay(0);

    g_test_rc_array[0].pos_x = WIN_OX;
    g_test_rc_array[0].pos_y = (uint8)(WIN_OY - 2);   /* below OY-1 -> culled */
    g_test_rc_array[0].sprite_state[0] = 1;
    idx_array[0] = 0;

    fd2_animate_status_effect_overlay_flicker(0, 0, 1, (uint32)idx_array);

    ASSERT_EQ(g_blitsolid_calls, 0);
    /* entry SFX still fired even though nothing was drawn */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
}

/* ================================================================
 * fd2_animate_spell_impact_per_target tests
 * ================================================================ */

/* Scratch sheet for data_fd2_resource_portrait_sheet_ptr. The function reads a
 * dword at [6 + (sprite_off+frame)*4] and adds the sheet base to it to form the
 * frame sprite pointer. Sized for the worst case (spell 9: sprite_off 0x57 + 26
 * frames -> byte index 6 + (0x57+26)*4 = 458). */
static uint8 g_impact_sheet[2048];

static void setup_impact(void)
{
    g_play_sfx_with_handle_calls = 0;
    g_sfx_id_count = 0;
    g_sfx_last_id = 0;
    memset(g_sfx_id_log, 0, sizeof(g_sfx_id_log));
    g_blitdec_calls = 0;
    g_blitdec_dst = 0;
    g_blitdec_sprite = 0;
    g_blitdec_stride = 0;

    memset(g_lgs, 0, sizeof(g_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgs;

    memset(g_impact_sheet, 0, sizeof(g_impact_sheet));
    data_fd2_resource_portrait_sheet_ptr = (uint32)g_impact_sheet;

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
}

/*
 * Drives the per-spell SFX dispatch chain with target_count = 0 (no blits),
 * so only the per-frame SFX logic runs. Asserts the exact fired-id sequence
 * against the real tables for every special-cased spell plus a table-only and
 * a silent spell. This is the highest-risk control flow in the function.
 */
static void test_impact_sfx_dispatch_sequences(void)
{
    /* spell 0x16: frame_count 13; sfx_tbl[0x16]=3; special frame 7 -> 3.
       expected fires: frame0=3, frame7=3 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0xDEAD, 0x16, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 2);
    ASSERT_EQ(g_sfx_id_log[0], 3);
    ASSERT_EQ(g_sfx_id_log[1], 3);

    /* spell 0x19: frame_count 13; sfx_tbl[0x19]=5; special frames 3,6 -> 5.
       expected: frame0=5, frame3=5, frame6=5 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x19, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 3);
    ASSERT_EQ(g_sfx_id_log[0], 5);
    ASSERT_EQ(g_sfx_id_log[1], 5);
    ASSERT_EQ(g_sfx_id_log[2], 5);

    /* spell 0x12: frame_count 12; sfx_tbl[0x12]=7; special frame 4 -> 7.
       expected: frame0=7, frame4=7 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x12, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 2);
    ASSERT_EQ(g_sfx_id_log[0], 7);
    ASSERT_EQ(g_sfx_id_log[1], 7);

    /* spell 0x13: frame_count 13; sfx_tbl[0x13]=8; special frames 3,6 -> 8.
       expected: frame0=8, frame3=8, frame6=8 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x13, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 3);
    ASSERT_EQ(g_sfx_id_log[0], 8);
    ASSERT_EQ(g_sfx_id_log[1], 8);
    ASSERT_EQ(g_sfx_id_log[2], 8);

    /* spell 0x08: frame_count 11; sfx_tbl[8]=0x0A; special frames 3,6 -> 0x0A.
       expected: frame0=10, frame3=10, frame6=10 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x08, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 3);
    ASSERT_EQ(g_sfx_id_log[0], 10);
    ASSERT_EQ(g_sfx_id_log[1], 10);
    ASSERT_EQ(g_sfx_id_log[2], 10);

    /* spell 0x09: frame_count 27; sfx_tbl[9]=0x0E; special frames 0xF,0x13 -> 0xF.
       expected: frame0=14, frame15=15, frame19=15 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x09, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 3);
    ASSERT_EQ(g_sfx_id_log[0], 14);
    ASSERT_EQ(g_sfx_id_log[1], 15);
    ASSERT_EQ(g_sfx_id_log[2], 15);

    /* spell 0x00: table-only sfx (sfx_tbl[0]=6), not special; one fire on frame0 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x00, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 1);
    ASSERT_EQ(g_sfx_id_log[0], 6);

    /* spell 0x0A: sfx_tbl[0x0A]=0 and not special -> zero fires over 8 frames */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x0A, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 0);
}

/*
 * Window-cull predicate, per-frame blit count, dst/frame-sprite arithmetic.
 * Uses spell 0x0A (8 frames, no SFX) with one in-window and one out-of-window
 * target so each frame blits exactly once (the in-window char).
 */
static void test_impact_cull_and_arithmetic(void)
{
    uint8 idx_array[2];
    uint32 *sheet;
    uint32 sprite_off;
    uint32 last_frame;
    uint32 exp_src;
    uint32 exp_dst;

    setup_impact();

    /* seed the sheet dword table: entry[i] = i*0x10 so the resolved frame
       pointer (sheet + table[6+(off+frame)*4]) is frame-distinguishable */
    sheet = (uint32 *)(g_impact_sheet + 6);
    {
        int i;
        for (i = 0; i < 500; i++) {
            sheet[i] = (uint32)i * 0x10u;
        }
    }

    /* char 0 inside the window */
    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;
    /* char 1 past the right edge (OX+MX = 0x1D) -> culled */
    g_test_rc_array[1].pos_x = 0x1E;
    g_test_rc_array[1].pos_y = 0x24;

    idx_array[0] = 0;
    idx_array[1] = 1;

    /* spell 0x0A: 8 frames, no SFX, sprite_off table[0x0A] = 0x31 */
    sprite_off = data_fd2_animation_spell_sprite_offset_table[0x0A];
    fd2_animate_spell_impact_per_target(0, 0x0A, 2, (uint32)idx_array);

    /* one blit per frame (in-window char only); 8 frames */
    ASSERT_EQ(g_blitdec_calls, 8);
    ASSERT_EQ(data_fd2_animation_spell_frame_count_table[0x0A], 8);

    /* last recorded blit is the final frame (frame 7) of the in-window char.
       frame_sprite_addr = sheet_base + table[6 + (sprite_off+frame)*4],
       and table[k] == k*0x10 where k = sprite_off + frame */
    last_frame = 7u;
    exp_src = (uint32)g_impact_sheet
            + (sprite_off + last_frame) * 0x10u;
    ASSERT_EQ(g_blitdec_sprite, exp_src);

    /* dst = lgs + (pos_y-OY)*0x2AC0 + (pos_x-OX)*0x18 + 0x75D8 */
    exp_dst = (uint32)g_lgs
            + (0x24u - WIN_OY) * 0x2ac0u
            + (0x15u - WIN_OX) * 0x18u
            + 0x75d8u;
    ASSERT_EQ(g_blitdec_dst, exp_dst);

    /* stride is the fixed 0x1C8 */
    ASSERT_EQ(g_blitdec_stride, 0x1c8u);

    /* spell 0x0A fires no SFX */
    ASSERT_EQ(g_sfx_id_count, 0);
}

/*
 * Zero-frame guard: a spell whose frame_count table entry is 0 (index 30/31)
 * runs no frames at all -> no blits, no SFX, no waits.
 */
static void test_impact_zero_frames(void)
{
    uint8 idx_array[1];

    setup_impact();

    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;
    idx_array[0] = 0;

    /* spell 30 (0x1E): frame_count table[30] = 0 */
    ASSERT_EQ(data_fd2_animation_spell_frame_count_table[30], 0);
    fd2_animate_spell_impact_per_target(0, 30, 1, (uint32)idx_array);

    ASSERT_EQ(g_blitdec_calls, 0);
    ASSERT_EQ(g_sfx_id_count, 0);
}

/* ================================================================
 * fd2_animate_spell_full_screen_flash tests
 * ================================================================ */

/* recording stub state for the (not-yet-emitted) spell-effect overlay
 * compositor, plus the delay-thunk and composite-frame counters (testglob.c) */
extern int    g_spellfx_overlay_calls;
extern uint32 g_spellfx_overlay_dst[8];
extern uint32 g_spellfx_overlay_ntgt[8];
extern uint32 g_spellfx_overlay_arr[8];
extern int    g_spellfx_overlay_fx[8];
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;
extern int    g_composite_call_count;

static void setup_fullflash(void)
{
    g_spellfx_overlay_calls = 0;
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    g_composite_call_count = 0;

    /* the real fd2_composite_battle_frame(0) finalizer + the real
     * fd2_blit_rectangle strobe both read +0x8088 out of this buffer */
    memset(g_lgs, 0, sizeof(g_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgs;

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;
}

/*
 * Drives the whole flash pipeline and checks the two-composite + strobe
 * structure. The two spell-effect composites are recorded by the harness
 * stub; the strobe loop count is observed through the delay-thunk counter;
 * the closing composite_battle_frame(0) is observed through the tile-map
 * composite counter (the only stage in that finalizer still backed by a
 * recording stub). Verifies variant ordering (0x4A then 0x4B), the dst
 * buffer routing (live back-buffer then a distinct malloc'd buffer), the
 * forwarded target args, and that the loop runs exactly four iterations
 * (8 delays) — guarding the test-first counted loop against off-by-one.
 */
static void test_fullflash_two_composites_and_strobe(void)
{
    uint8 idx_array[3];

    setup_fullflash();

    idx_array[0] = 2;
    idx_array[1] = 5;
    idx_array[2] = 1;

    /* param_1 and spell_id are body-unused; pass sentinels */
    fd2_animate_spell_full_screen_flash(0xDEAD, 0xBEEF, 3, (uint32)idx_array);

    /* exactly two spell-effect composites: variant A then variant B */
    ASSERT_EQ(g_spellfx_overlay_calls, 2);
    ASSERT_EQ(g_spellfx_overlay_fx[0], 0x4a);
    ASSERT_EQ(g_spellfx_overlay_fx[1], 0x4b);

    /* variant A renders into the live back-buffer */
    ASSERT_EQ(g_spellfx_overlay_dst[0], (uint32)g_lgs);
    /* variant B renders into a freshly malloc'd buffer (non-null, distinct) */
    ASSERT_TRUE(g_spellfx_overlay_dst[1] != 0);
    ASSERT_TRUE(g_spellfx_overlay_dst[1] != (uint32)g_lgs);

    /* target_count and char_idx_array forwarded unchanged to both composites */
    ASSERT_EQ(g_spellfx_overlay_ntgt[0], 3u);
    ASSERT_EQ(g_spellfx_overlay_ntgt[1], 3u);
    ASSERT_EQ(g_spellfx_overlay_arr[0], (uint32)idx_array);
    ASSERT_EQ(g_spellfx_overlay_arr[1], (uint32)idx_array);

    /* strobe = 4 iterations x 2 delays = 8 delays, each of 0x5A ticks */
    ASSERT_EQ(g_delay375b2_calls, 8);
    ASSERT_EQ(g_delay375b2_last_ticks, 0x5au);

    /* closing fd2_composite_battle_frame(0) ran exactly once (its tile-map
     * stage bumps g_composite_call_count; the overlay stubs do not) */
    ASSERT_EQ(g_composite_call_count, 1);
}

void run_anim_anicombt_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anicombt\n");
    RUN_TEST(test_overlay_cull_and_arithmetic);
    RUN_TEST(test_overlay_palette3_offset);
    RUN_TEST(test_overlay_cull_top_edge);
    RUN_TEST(test_impact_sfx_dispatch_sequences);
    RUN_TEST(test_impact_cull_and_arithmetic);
    RUN_TEST(test_impact_zero_frames);
    RUN_TEST(test_fullflash_two_composites_and_strobe);
    printf("\n");
}
