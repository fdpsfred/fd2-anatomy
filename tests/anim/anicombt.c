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

void run_anim_anicombt_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anicombt\n");
    RUN_TEST(test_overlay_cull_and_arithmetic);
    RUN_TEST(test_overlay_palette3_offset);
    RUN_TEST(test_overlay_cull_top_edge);
    printf("\n");
}
