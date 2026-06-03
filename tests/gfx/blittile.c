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

/* ================================================================
 * fd2_blit_animated_tile_at_pos @ 0x12AC6
 *
 * Per-cell battle-map tile sprite paint with animation flip and
 * optional palette remap. Drives the real routine and observes the
 * blitter call (passthrough or remap stub) it emits.
 * ================================================================ */

/* fd2_tile_blit_24x24_with_remap_table recording (testglob.c) */
extern int    g_blitremap_calls;
extern uint32 g_blitremap_table[64];

/* battle_tile_map: 4 bytes/cell. +4..+5 word = tile id (low 10 bits),
 * +7 byte = cursor-overlay flag (0xFF = plain passthrough). */
#define ATM_W     0x20
#define ATM_ROWS  0x40   /* enough rows to cover oy+max_y+2 */
static uint8 g_atm_map[ATM_W * ATM_ROWS * 4];
/* tile-attribute flags, 4 bytes/tile */
static uint8 g_atm_attr[0x400 * 4];
/* battle scene snapshot: 4-byte absolute offset table at +10 (tile_id*4) */
static uint8 g_atm_scene[0x400 * 4 + 16];
/* per-chapter tile anim remap table base: 4-byte offset table at +6 */
static uint8 g_atm_anim_tbl[256];

#define ATM_OX 0x10u
#define ATM_OY 0x20u
#define ATM_MX 0x08u   /* x in [ATM_OX-1, ATM_OX+ATM_MX] */
#define ATM_MY 0x06u   /* y in [ATM_OY-1, ATM_OY+ATM_MY+1], y>=0 */

/* place tile id `tid` at cell (x, y); set +7 overlay flag. */
static void atm_cell(uint32 x, uint32 y, uint16 tid, uint8 overlay_flag)
{
    uint32 cell = (y * ATM_W + x) * 4;
    *(uint16 *)(g_atm_map + cell + 4) = tid;
    g_atm_map[cell + 6] = 0;
    g_atm_map[cell + 7] = overlay_flag;
}

static void setup_atm(void)
{
    int i;
    uint32 *snap_tbl;

    g_blitpass_calls = 0;
    g_blitremap_calls = 0;

    memset(g_atm_map, 0, sizeof(g_atm_map));
    memset(g_atm_attr, 0, sizeof(g_atm_attr));

    /* snapshot offset table: entry[tid] = tid*0x100 so a recorded sprite
     * src uniquely identifies which tile id was used. */
    snap_tbl = (uint32 *)(g_atm_scene + 10);
    for (i = 0; i < 0x400; i++) {
        snap_tbl[i] = (uint32)i * 0x100u;
    }
    /* anim remap table: entry[k] = k*0x10 + 0x40 so the remap arg is
     * identifiable; the lookup[frame_counter] selects which entry. */
    for (i = 0; i < 60; i++) {
        *(uint32 *)(g_atm_anim_tbl + 6 + i * 4) = (uint32)i * 0x10u + 0x40u;
    }

    data_fd2_battle_tile_map_ptr = (uint32)g_atm_map;
    data_fd2_battle_map_width_tiles = ATM_W;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_atm_attr;
    battle_scene_snapshot = (uint32)g_atm_scene;
    data_fd2_tile_anim_table_base = (uint32)g_atm_anim_tbl;
    data_fd2_graphics_bg_anim_flip_flag = 0;
    data_fd2_battle_tile_map_anim_frame_counter = 0;
    memset(data_fd2_graphics_tile_anim_palette_phase_lookup, 0,
           sizeof(data_fd2_graphics_tile_anim_palette_phase_lookup));

    data_fd2_large_game_state_buffer_ptr = (uint32)g_ws;
    data_fd2_battle_view_window_origin_x = ATM_OX;
    data_fd2_battle_view_window_origin_y = ATM_OY;
    data_fd2_battle_view_window_max_x = ATM_MX;
    data_fd2_battle_view_window_max_y = ATM_MY;
}

static uint32 atm_expect_dst(uint32 x, uint32 y)
{
    return (uint32)g_ws + 0x8088u +
        (y - ATM_OY) * 0x2AC0u +
        (x - ATM_OX) * 0x18u;
}

/* renderable tile, no overlay -> exactly one passthrough blit with the
 * sprite src derived from the tile id and the window-relative dst. */
static void test_anim_passthrough_branch(void)
{
    setup_atm();
    atm_cell(0x13, 0x23, 7, 0xFF);   /* tile id 7, no overlay */
    g_atm_attr[7 * 4] = 0x80;        /* renderable, not animated */

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitremap_calls, 0);
    ASSERT_EQ(g_blitpass_stride[0], 0x1c8u);
    ASSERT_EQ(g_blitpass_dst[0], atm_expect_dst(0x13, 0x23));
    /* src = snapshot + snap_tbl[7] = scene + 7*0x100 */
    ASSERT_EQ(g_blitpass_src[0], (uint32)g_atm_scene + 7u * 0x100u);
}

/* tile id is masked to 10 bits: a +4 word of 0xFC07 -> id 0x007. */
static void test_anim_tile_id_masked_10_bits(void)
{
    setup_atm();
    atm_cell(0x13, 0x23, 0xFC07, 0xFF);
    g_atm_attr[7 * 4] = 0x80;

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitpass_src[0], (uint32)g_atm_scene + 7u * 0x100u);
}

/* attr bit 0x80 clear -> transparent tile, no blit at all. */
static void test_anim_transparent_skip(void)
{
    setup_atm();
    atm_cell(0x13, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x00;        /* not renderable */

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    ASSERT_EQ(g_blitpass_calls, 0);
    ASSERT_EQ(g_blitremap_calls, 0);
}

/* attr bit 0x08 set -> tile id += bg_anim_flip_flag*2 for the sprite
 * lookup. attr is indexed by the ORIGINAL id; the sprite src uses the
 * flipped id. flip_flag 3 -> id 7 + 6 = 13. */
static void test_anim_flip_offsets_sprite(void)
{
    setup_atm();
    atm_cell(0x13, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80 | 0x08; /* renderable + animated */
    data_fd2_graphics_bg_anim_flip_flag = 3;

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    ASSERT_EQ(g_blitpass_calls, 1);
    /* src = scene + snap_tbl[7 + 3*2] = scene + 13*0x100 */
    ASSERT_EQ(g_blitpass_src[0], (uint32)g_atm_scene + 13u * 0x100u);
}

/* +7 overlay flag != 0xFF -> remap branch: one with_remap_table call,
 * remap arg = anim_base + *(int*)(anim_base + 6 + lookup[frame]*4). */
static void test_anim_remap_branch(void)
{
    uint32 expect_remap;

    setup_atm();
    atm_cell(0x13, 0x23, 7, 0x00);   /* overlay flag set (not 0xFF) */
    g_atm_attr[7 * 4] = 0x80;
    data_fd2_battle_tile_map_anim_frame_counter = 2;
    data_fd2_graphics_tile_anim_palette_phase_lookup[2] = 5;

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitremap_calls, 1);
    ASSERT_EQ(g_blitpass_dst[0], atm_expect_dst(0x13, 0x23));
    ASSERT_EQ(g_blitpass_src[0], (uint32)g_atm_scene + 7u * 0x100u);
    /* lookup[2]=5 -> table entry 5 -> value 5*0x10+0x40 = 0x90 */
    expect_remap = (uint32)g_atm_anim_tbl +
        *(uint32 *)(g_atm_anim_tbl + 6 + 5 * 4);
    ASSERT_EQ(g_blitremap_table[0], expect_remap);
}

/* window rejects: x == ox-2 (below ox-1 margin) -> no blit. */
static void test_anim_x_below_margin_noop(void)
{
    setup_atm();
    atm_cell(ATM_OX - 2u, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    fd2_blit_animated_tile_at_pos((uint32)g_ws, (int32)(ATM_OX - 2u), 0x23);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* x == ox-1 IS inside the +-1 margin -> blits. */
static void test_anim_x_left_margin_in(void)
{
    setup_atm();
    atm_cell(ATM_OX - 1u, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    fd2_blit_animated_tile_at_pos((uint32)g_ws, (int32)(ATM_OX - 1u), 0x23);
    ASSERT_EQ(g_blitpass_calls, 1);
}

/* x == ox+max_x is inclusive (<=) -> blits; x == ox+max_x+1 -> no blit. */
static void test_anim_x_right_bound(void)
{
    setup_atm();
    atm_cell(ATM_OX + ATM_MX, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    fd2_blit_animated_tile_at_pos((uint32)g_ws, (int32)(ATM_OX + ATM_MX), 0x23);
    ASSERT_EQ(g_blitpass_calls, 1);

    setup_atm();
    fd2_blit_animated_tile_at_pos((uint32)g_ws,
                                  (int32)(ATM_OX + ATM_MX + 1u), 0x23);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* y == oy+max_y+1 inclusive -> blits; y == oy+max_y+2 -> no blit. */
static void test_anim_y_bottom_bound(void)
{
    setup_atm();
    atm_cell(0x13, ATM_OY + ATM_MY + 1u, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13,
                                  (int32)(ATM_OY + ATM_MY + 1u));
    ASSERT_EQ(g_blitpass_calls, 1);

    setup_atm();
    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13,
                                  (int32)(ATM_OY + ATM_MY + 2u));
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* negative y is rejected even when it would pass the lower margin test
 * (origin small): the explicit y >= 0 guard. */
static void test_anim_negative_y_noop(void)
{
    setup_atm();
    data_fd2_battle_view_window_origin_y = 0;
    /* oy-1 = -1 <= -1 would pass, but y>=0 guard rejects -1 */
    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, -1);
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
    RUN_TEST(test_anim_passthrough_branch);
    RUN_TEST(test_anim_tile_id_masked_10_bits);
    RUN_TEST(test_anim_transparent_skip);
    RUN_TEST(test_anim_flip_offsets_sprite);
    RUN_TEST(test_anim_remap_branch);
    RUN_TEST(test_anim_x_below_margin_noop);
    RUN_TEST(test_anim_x_left_margin_in);
    RUN_TEST(test_anim_x_right_bound);
    RUN_TEST(test_anim_y_bottom_bound);
    RUN_TEST(test_anim_negative_y_noop);
    printf("\n");
}
