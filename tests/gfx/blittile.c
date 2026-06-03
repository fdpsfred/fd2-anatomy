/*
 * unit tests for src/gfx/blittile.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* recording stub for fd2_tile_blit_24x24_passthrough (testglob.c) */
extern int    g_blitpass_calls;
extern uint32 g_blitpass_src[64];
extern uint32 g_blitpass_dst[64];
extern uint32 g_blitpass_stride[64];

/* Sprite atlas backing data_fd2_runtime_battle_state_ptr: a 4-byte absolute
 * offset table at +6 (index*4). table[i] == i*0x10 so the recorded src pointer
 * uniquely identifies the sprite index used. */
static uint8 g_atlas[6 + 64 * 4];
/* Real backing for the workspace base; the recording stub never dereferences
 * the dst pointer so the buffer only needs to exist as an address anchor. */
static uint8 g_ws[0x10000];

#define WIN_OX 0x10u
#define WIN_OY 0x20u
#define WIN_MX 0x08u   /* x in [0x10, 0x18) */
#define WIN_MY 0x06u   /* y in [0x20, 0x26) */

static void setup_blittile(void)
{
    int i;
    uint32 *table;

    g_blitpass_calls = 0;
    table = (uint32 *)(g_atlas + 6);
    for (i = 0; i < 64; i++) {
        table[i] = (uint32)i * 0x10u;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_atlas;
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ws;
    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;
}

static uint32 expect_dst(uint32 x, uint32 y)
{
    return (uint32)g_ws +
        (y - WIN_OY) * 0x2AC0u +
        (x - WIN_OX) * 0x18u +
        0x8088u;
}

static uint32 expect_src(uint32 idx)
{
    return (uint32)g_atlas + idx * 0x10u;
}

/* In-window blit forwards exactly one passthrough call with the correct
 * src (atlas + table[idx]), dst (row/col stride offset), and stride 0x1C8. */
static void test_in_window_blit_args(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(0x13, 0x23, 5);
    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitpass_src[0], expect_src(5));
    ASSERT_EQ(g_blitpass_dst[0], expect_dst(0x13, 0x23));
    ASSERT_EQ(g_blitpass_stride[0], 0x1c8u);
}

/* Origin corner (x==ox, y==oy) is inside the window: dst == base + 0x8088. */
static void test_origin_corner_in_window(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX, WIN_OY, 0);
    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitpass_dst[0], (uint32)g_ws + 0x8088u);
    ASSERT_EQ(g_blitpass_src[0], expect_src(0));
}

/* Far in-window corner (x==ox+max_x-1, y==oy+max_y-1) still blits. */
static void test_far_corner_in_window(void)
{
    uint32 x;
    uint32 y;

    setup_blittile();
    x = WIN_OX + WIN_MX - 1u;
    y = WIN_OY + WIN_MY - 1u;
    fd2_blit_24x24_at_window_relative_pos(x, y, 7);
    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitpass_dst[0], expect_dst(x, y));
}

/* x just left of window (x == ox-1) is a silent no-op. */
static void test_x_below_window_noop(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX - 1u, WIN_OY, 0);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* x at right edge (x == ox+max_x) is out of window (half-open interval). */
static void test_x_at_right_edge_noop(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX + WIN_MX, WIN_OY, 0);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* y just above window (y == oy-1) is a silent no-op. */
static void test_y_below_window_noop(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX, WIN_OY - 1u, 0);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* y at bottom edge (y == oy+max_y) is out of window. */
static void test_y_at_bottom_edge_noop(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX, WIN_OY + WIN_MY, 0);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* Bounds tests use signed comparison (decompile casts world coords to int):
 * a coord far below origin (treated as negative when origin is small) must be
 * rejected, not wrap to a huge unsigned value that passes the upper bound. */
static void test_signed_lower_bound(void)
{
    setup_blittile();
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x10;
    data_fd2_battle_view_window_max_y = 0x10;
    /* 0xFFFFFFFF as signed int = -1 < origin 0 -> rejected */
    fd2_blit_24x24_at_window_relative_pos(0xFFFFFFFFu, 0, 0);
    ASSERT_EQ(g_blitpass_calls, 0);
}

void run_gfx_blittile_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/blittile\n");
    RUN_TEST(test_in_window_blit_args);
    RUN_TEST(test_origin_corner_in_window);
    RUN_TEST(test_far_corner_in_window);
    RUN_TEST(test_x_below_window_noop);
    RUN_TEST(test_x_at_right_edge_noop);
    RUN_TEST(test_y_below_window_noop);
    RUN_TEST(test_y_at_bottom_edge_noop);
    RUN_TEST(test_signed_lower_bound);
    printf("\n");
}
