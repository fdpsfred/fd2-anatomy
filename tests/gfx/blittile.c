/*
 * unit tests for src/gfx/blittile.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "blitprob.h"
#include <stdio.h>

/* fd2_tile_blit_24x24_passthrough is emitted for real, so these tests drive it
 * through its callers with probe sprites (see blitprob.h) and read the painted
 * destination bytes back: each atlas slot's one-pixel probe paints value idx+1,
 * so a painted byte of value idx+1 at the expected dst offset proves the caller
 * resolved that sprite index AND computed that dst; bp_count_painted gives the
 * blit count (one painted pixel per blit). */

/* Sprite atlas backing data_fd2_runtime_battle_state_ptr: a 4-byte absolute
 * offset table at +6 (index*4) pointing slot i at bp_atlas_slot_off (a sprite
 * region past the table, so slot 0 does not clobber the table), with a one-pixel
 * probe (value i+1) at each slot. */
#define ATLAS_SPAN 0x40u
#define ATLAS_N    64
static uint8 g_atlas[6 + ATLAS_N * 4 + ATLAS_N * ATLAS_SPAN];
/* Real backing for the workspace; the real blitter writes the painted pixel(s)
 * here, so it must span the largest dst any in-window blit below reaches
 * (the far anim-tile rows approach ~0x1AC00 + one row stride). */
static uint8 g_ws[0x1C000];

#define WIN_OX 0x10u
#define WIN_OY 0x20u
#define WIN_MX 0x08u   /* x in [0x10, 0x18) */
#define WIN_MY 0x06u   /* y in [0x20, 0x26) */

static void setup_blittile(void)
{
    memset(g_atlas, 0, sizeof(g_atlas));
    bp_build_atlas1(g_atlas, 6u, ATLAS_SPAN, ATLAS_N);
    memset(g_ws, 0, sizeof(g_ws));
    data_fd2_runtime_battle_state_ptr = (uint32)g_atlas;
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ws;
    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;
}

/* destination byte offset (into g_ws) of the painted pixel for a blit at (x,y) */
static uint32 expect_off(uint32 x, uint32 y)
{
    return (y - WIN_OY) * 0x2AC0u + (x - WIN_OX) * 0x18u + 0x8088u;
}

/* In-window blit paints exactly one pixel; its value (idx+1) identifies the
 * resolved sprite index and its offset confirms the row/col dst arithmetic. */
static void test_in_window_blit_args(void)
{
    uint32 first;

    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(0x13, 0x23, 5);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    /* sprite index 5 -> probe value 6, painted at the (0x13,0x23) dst offset */
    ASSERT_EQ(bp_count_value(g_ws, sizeof(g_ws), 6u, &first), 1);
    ASSERT_EQ(first, expect_off(0x13, 0x23));
}

/* Stride forwarded is 0x1C8: a two-pixel probe's second pixel lands exactly one
 * stride (0x1C8) past the first. */
static void test_in_window_blit_stride(void)
{
    uint32 base;

    setup_blittile();
    /* overwrite slot 5 with a two-pixel probe (value 6) to expose the stride */
    bp_probe2(g_atlas + bp_atlas_slot_off(6u, ATLAS_N, ATLAS_SPAN, 5), 6u);
    fd2_blit_24x24_at_window_relative_pos(0x13, 0x23, 5);
    base = expect_off(0x13, 0x23);
    ASSERT_EQ((int)g_ws[base], 6);
    ASSERT_EQ((int)g_ws[base + 0x1c8u], 6);   /* stride 0x1C8 */
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 2);
}

/* Origin corner (x==ox, y==oy) is inside the window: pixel at base + 0x8088. */
static void test_origin_corner_in_window(void)
{
    uint32 first;

    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX, WIN_OY, 0);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    /* sprite index 0 -> probe value 1 at the origin-cell offset 0x8088 */
    ASSERT_EQ(bp_count_value(g_ws, sizeof(g_ws), 1u, &first), 1);
    ASSERT_EQ(first, 0x8088u);
}

/* Far in-window corner (x==ox+max_x-1, y==oy+max_y-1) still blits, at its
 * row/col offset. */
static void test_far_corner_in_window(void)
{
    uint32 x;
    uint32 y;
    uint32 first;

    setup_blittile();
    x = WIN_OX + WIN_MX - 1u;
    y = WIN_OY + WIN_MY - 1u;
    fd2_blit_24x24_at_window_relative_pos(x, y, 7);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    ASSERT_EQ(bp_count_value(g_ws, sizeof(g_ws), 8u, &first), 1);  /* idx 7 -> 8 */
    ASSERT_EQ(first, expect_off(x, y));
}

/* x just left of window (x == ox-1) is a silent no-op. */
static void test_x_below_window_noop(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX - 1u, WIN_OY, 0);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* x at right edge (x == ox+max_x) is out of window (half-open interval). */
static void test_x_at_right_edge_noop(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX + WIN_MX, WIN_OY, 0);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* y just above window (y == oy-1) is a silent no-op. */
static void test_y_below_window_noop(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX, WIN_OY - 1u, 0);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* y at bottom edge (y == oy+max_y) is out of window. */
static void test_y_at_bottom_edge_noop(void)
{
    setup_blittile();
    fd2_blit_24x24_at_window_relative_pos(WIN_OX, WIN_OY + WIN_MY, 0);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
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
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* ================================================================
 * fd2_blit_animated_tile_at_pos @ 0x12AC6
 *
 * Per-cell battle-map tile sprite paint with animation flip and
 * optional palette remap. Drives the real routine and observes the
 * blitter call (passthrough or remap stub) it emits.
 * ================================================================ */

/* fd2_tile_blit_24x24_with_remap_table is emitted for real (src/gfx/blittile.c),
 * so the remap branch here drives the real blitter against a one-pixel sprite at
 * the in-bounds window-origin cell and reads back the painted LUT byte; there is
 * no recording stub for it. The passthrough / dimmed blitters are still stubs. */

/* battle_tile_map: 4 bytes/cell. +4..+5 word = tile id (low 10 bits),
 * +7 byte = cursor-overlay flag (0xFF = plain passthrough). */
#define ATM_W     0x20
#define ATM_ROWS  0x40   /* enough rows to cover oy+max_y+2 */
static uint8 g_atm_map[ATM_W * ATM_ROWS * 4];
/* tile-attribute flags, 4 bytes/tile */
static uint8 g_atm_attr[0x400 * 4];
/* battle scene snapshot: 4-byte absolute offset table at +10 (tile_id*4) that
 * points each tile id at a real probe sprite in a dedicated sprite region past
 * the table, so the REAL passthrough/remap blitters decode an actual sprite.
 * SCENE_SPRITE_OFF clears the 0x1010-byte table; ATM_SPRITE_SPAN holds a probe
 * (<= 26 bytes) per id; ATM_SPRITE_N ids are provisioned (covers the 7/13 used). */
#define SCENE_SPRITE_OFF  0x1100u
#define ATM_SPRITE_SPAN   0x40u
#define ATM_SPRITE_N      0x20
static uint8 g_atm_scene[0x2000];
/* per-chapter tile anim remap table base: 4-byte offset table at +6. Sized to
 * hold a full 256-entry LUT starting at the offset the remap branch selects
 * (entry 5 -> base+0x90), so the real blitter can map a source pixel through a
 * real LUT placed there. */
static uint8 g_atm_anim_tbl[0x200];

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

/* scene-table byte offset of tile id `tid`'s probe sprite slot */
static uint32 atm_slot_off(uint32 tid)
{
    return SCENE_SPRITE_OFF + (tid % (uint32)ATM_SPRITE_N) * ATM_SPRITE_SPAN;
}

/* probe value the scene sprite for tile id `tid` paints (1-based, id-specific) */
static uint8 atm_tid_value(uint32 tid)
{
    return (uint8)((tid % (uint32)ATM_SPRITE_N) + 1u);
}

static void setup_atm(void)
{
    int i;
    uint32 *snap_tbl;

    memset(g_atm_map, 0, sizeof(g_atm_map));
    memset(g_atm_attr, 0, sizeof(g_atm_attr));
    memset(g_atm_scene, 0, sizeof(g_atm_scene));

    /* snapshot offset table: entry[tid] points at a real probe sprite in the
     * dedicated sprite region; the painted byte value (atm_tid_value) then
     * identifies which tile id the caller resolved. */
    snap_tbl = (uint32 *)(g_atm_scene + 10);
    for (i = 0; i < 0x400; i++) {
        snap_tbl[i] = atm_slot_off((uint32)i);
    }
    for (i = 0; i < ATM_SPRITE_N; i++) {
        bp_probe1(g_atm_scene + SCENE_SPRITE_OFF + (uint32)i * ATM_SPRITE_SPAN,
                  (uint8)(i + 1));
    }
    /* anim remap table: entry[k] = k*0x10 + 0x40 so the remap arg is
     * identifiable; the lookup[frame_counter] selects which entry. */
    for (i = 0; i < 60; i++) {
        *(uint32 *)(g_atm_anim_tbl + 6 + i * 4) = (uint32)i * 0x10u + 0x40u;
    }

    memset(g_ws, 0, sizeof(g_ws));

    data_fd2_battle_tile_map_ptr = (uint32)g_atm_map;
    data_fd2_battle_map_width_tiles = ATM_W;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_atm_attr;
    data_fd2_battle_scene_snapshot = (uint32)g_atm_scene;
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

/* destination byte offset (into g_ws) of the painted pixel for tile (x,y) */
static uint32 atm_expect_off(uint32 x, uint32 y)
{
    return 0x8088u + (y - ATM_OY) * 0x2AC0u + (x - ATM_OX) * 0x18u;
}

/* renderable tile, no overlay -> exactly one passthrough blit with the sprite
 * src derived from the tile id and the window-relative dst. The painted byte's
 * value (atm_tid_value(7)) proves the src resolved to tile id 7's slot and its
 * offset proves the window-relative dst; the +0x18 transparent-skip rows leave
 * exactly one painted pixel, so a remap-branch blit would have painted a second
 * (the remap path uses a different sprite/dst and is exercised separately). */
static void test_anim_passthrough_branch(void)
{
    uint32 first;

    setup_atm();
    atm_cell(0x13, 0x23, 7, 0xFF);   /* tile id 7, no overlay */
    g_atm_attr[7 * 4] = 0x80;        /* renderable, not animated */

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    ASSERT_EQ(bp_count_value(g_ws, sizeof(g_ws), atm_tid_value(7), &first), 1);
    ASSERT_EQ(first, atm_expect_off(0x13, 0x23));
}

/* the forwarded stride is 0x1C8: a two-pixel probe at tile 7's slot lands its
 * second pixel exactly one stride past the first. */
static void test_anim_passthrough_stride(void)
{
    uint32 base;

    setup_atm();
    atm_cell(0x13, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    bp_probe2(g_atm_scene + atm_slot_off(7), atm_tid_value(7));

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    base = atm_expect_off(0x13, 0x23);
    ASSERT_EQ((int)g_ws[base], (int)atm_tid_value(7));
    ASSERT_EQ((int)g_ws[base + 0x1c8u], (int)atm_tid_value(7));
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 2);
}

/* tile id is masked to 10 bits: a +4 word of 0xFC07 -> id 0x007, so the painted
 * byte is tile id 7's value. */
static void test_anim_tile_id_masked_10_bits(void)
{
    uint32 first;

    setup_atm();
    atm_cell(0x13, 0x23, 0xFC07, 0xFF);
    g_atm_attr[7 * 4] = 0x80;

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    ASSERT_EQ(bp_count_value(g_ws, sizeof(g_ws), atm_tid_value(7), &first), 1);
    ASSERT_EQ(first, atm_expect_off(0x13, 0x23));
}

/* attr bit 0x80 clear -> transparent tile, no blit at all. */
static void test_anim_transparent_skip(void)
{
    setup_atm();
    atm_cell(0x13, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x00;        /* not renderable */

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    /* not renderable -> no blit of either kind painted anything. */
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* attr bit 0x08 set -> tile id += bg_anim_flip_flag*2 for the sprite lookup.
 * attr is indexed by the ORIGINAL id; the sprite src uses the flipped id.
 * flip_flag 3 -> id 7 + 6 = 13, so the painted byte is tile id 13's value. */
static void test_anim_flip_offsets_sprite(void)
{
    uint32 first;

    setup_atm();
    atm_cell(0x13, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80 | 0x08; /* renderable + animated */
    data_fd2_graphics_bg_anim_flip_flag = 3;

    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, 0x23);

    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    /* flipped src = scene + snap_tbl[7 + 3*2] = tile id 13's slot */
    ASSERT_EQ(bp_count_value(g_ws, sizeof(g_ws), atm_tid_value(13), &first), 1);
    ASSERT_EQ(first, atm_expect_off(0x13, 0x23));
}

/* +7 overlay flag != 0xFF -> remap branch: the caller dispatches to the real
 * fd2_tile_blit_24x24_with_remap_table. Drive it end-to-end with a one-pixel
 * sprite at the in-bounds window-origin cell and read back the single painted
 * byte. A correct result (g_ws[0x8088] == LUT[src_pixel]) simultaneously proves
 * the caller computed: the right sprite src (else a different/zero RLE byte is
 * read), the right dst (g_ws + 0x8088 at the origin cell), and the right
 * remap_table = anim_base + *(int*)(anim_base + 6 + lookup[frame]*4); the
 * remapped value (not the raw src_pixel a passthrough would paint) confirms the
 * remap branch, not passthrough, was selected. */
static void test_anim_remap_branch(void)
{
    uint8 *sprite;
    uint8 *lut;
    uint32 remap_off;
    uint8  src_pixel;
    int    i;

    setup_atm();
    /* place the renderable tile id 7 at the window-origin cell so its dst is
     * g_ws + 0x8088 (well inside g_ws), with overlay flag set (not 0xFF). */
    atm_cell(ATM_OX, ATM_OY, 7, 0x00);
    g_atm_attr[7 * 4] = 0x80;
    data_fd2_battle_tile_map_anim_frame_counter = 2;
    data_fd2_graphics_tile_anim_palette_phase_lookup[2] = 5;

    /* lookup[2]=5 -> offset-table entry 5 -> value 5*0x10+0x40 = 0x90, so the
     * caller's remap_table = g_atm_anim_tbl + 0x90. Lay a real 256-entry LUT
     * there: out = in ^ 0xA5, a non-identity map. */
    remap_off = *(uint32 *)(g_atm_anim_tbl + 6 + 5 * 4);
    ASSERT_EQ(remap_off, 0x90u);
    lut = g_atm_anim_tbl + remap_off;
    for (i = 0; i < 256; i++) {
        lut[i] = (uint8)(i ^ 0xA5);
    }

    /* overwrite tile id 7's slot with a custom one-pixel sprite carrying a known
     * source byte (src_pixel), so the ONLY dst write is dst[0] = LUT[src_pixel]. */
    src_pixel = 0xABu;
    sprite = g_atm_scene + atm_slot_off(7);
    sprite[0] = (uint8)(0x80u | (1u - 1u));   /* LITERAL of 1 */
    sprite[1] = src_pixel;
    sprite[2] = (uint8)(0xC0u | (23u - 1u));  /* SKIP 23: finish 24-col row 0 */
    for (i = 1; i < 24; i++) {
        sprite[2 + i] = (uint8)(0xC0u | (24u - 1u)); /* rows 1..23: SKIP 24 */
    }

    fd2_blit_animated_tile_at_pos((uint32)g_ws, ATM_OX, ATM_OY);

    /* exactly one painted byte; its value is the LUT-remapped src_pixel, proving
     * src, dst, remap_table arithmetic and that the remap (not passthrough)
     * branch ran (passthrough would have painted the raw src_pixel 0xAB). */
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    ASSERT_EQ((int)g_ws[0x8088], (int)lut[src_pixel]);
    ASSERT_EQ((int)g_ws[0x8088], (int)(uint8)(src_pixel ^ 0xA5));
}

/* window rejects: x == ox-2 (below ox-1 margin) -> no blit. */
static void test_anim_x_below_margin_noop(void)
{
    setup_atm();
    atm_cell(ATM_OX - 2u, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    fd2_blit_animated_tile_at_pos((uint32)g_ws, (int32)(ATM_OX - 2u), 0x23);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* x == ox-1 IS inside the +-1 margin -> blits. */
static void test_anim_x_left_margin_in(void)
{
    setup_atm();
    atm_cell(ATM_OX - 1u, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    fd2_blit_animated_tile_at_pos((uint32)g_ws, (int32)(ATM_OX - 1u), 0x23);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
}

/* x == ox+max_x is inclusive (<=) -> blits; x == ox+max_x+1 -> no blit. */
static void test_anim_x_right_bound(void)
{
    setup_atm();
    atm_cell(ATM_OX + ATM_MX, 0x23, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    fd2_blit_animated_tile_at_pos((uint32)g_ws, (int32)(ATM_OX + ATM_MX), 0x23);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);

    setup_atm();
    fd2_blit_animated_tile_at_pos((uint32)g_ws,
                                  (int32)(ATM_OX + ATM_MX + 1u), 0x23);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* y == oy+max_y+1 inclusive -> blits; y == oy+max_y+2 -> no blit. */
static void test_anim_y_bottom_bound(void)
{
    setup_atm();
    atm_cell(0x13, ATM_OY + ATM_MY + 1u, 7, 0xFF);
    g_atm_attr[7 * 4] = 0x80;
    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13,
                                  (int32)(ATM_OY + ATM_MY + 1u));
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);

    setup_atm();
    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13,
                                  (int32)(ATM_OY + ATM_MY + 2u));
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* negative y is rejected even when it would pass the lower margin test
 * (origin small): the explicit y >= 0 guard. */
static void test_anim_negative_y_noop(void)
{
    setup_atm();
    data_fd2_battle_view_window_origin_y = 0;
    /* oy-1 = -1 <= -1 would pass, but y>=0 guard rejects -1 */
    fd2_blit_animated_tile_at_pos((uint32)g_ws, 0x13, -1);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 0);
}

/* ================================================================
 * fd2_blit_scaled_tile_map_view @ 0x1F558
 *
 * Software rasterizer: scale-render a tile map into the large game
 * state buffer (+0x504). 12.12 fixed-point coords (0xC00 = 1 tile);
 * each output pixel divides the sub-tile fixed coord by 0x80 to pick a
 * 0..23 byte inside a 24x24 tile sprite. High-risk: fixed-point math,
 * signed division, nested loops, per-axis bounds checks.
 *
 * Fixtures below build a real output surface + a caller tile_data_table
 * of absolute sprite-data pointers, then compare the rendered output
 * against an independent reference implementation (ref_scaled_view,
 * structured differently from the emit) across several center/scale
 * combos including negative-coord and out-of-map edges. One case
 * (scale==0x80, center mapping the top-left to tile (0,0)) is also
 * checked against hand-computed bytes.
 * ================================================================ */

#define SV_SURF_BYTES 0x10000u           /* >= 64000 memset extent */
#define SV_TILES_X 6
#define SV_TILES_Y 5
#define SV_TILE_BYTES 0x240u             /* 24 rows * 0x18 bytes/row */
#define SV_NCELLS (0x40 * SV_TILES_Y)    /* table stride is 0x40 per row */

static uint8 g_sv_surf[SV_SURF_BYTES];
static uint8 g_sv_ref[SV_SURF_BYTES];
static uint8 g_sv_sprite[SV_NCELLS][SV_TILE_BYTES];
static uint32 g_sv_table[SV_NCELLS];

/* sprite byte encoding: identifies (tile_x, tile_y, inner_row, inner_col)
 * with a nonzero value (0 is reserved for cleared background). */
static uint8 sv_sprite_byte(int tx, int ty, int ir, int ic)
{
    return (uint8)(1u + ((((unsigned)tx * 5u + (unsigned)ty) * 7u
                          + (unsigned)ir) * 3u + (unsigned)ic));
}

static void setup_scaled_view(void)
{
    int tx;
    int ty;
    int ir;
    int ic;
    int cell;

    data_fd2_large_game_state_buffer_ptr = (uint32)g_sv_surf;
    data_fd2_battle_map_width_tiles = SV_TILES_X;
    data_fd2_battle_map_height_tiles = SV_TILES_Y;

    for (ty = 0; ty < SV_TILES_Y; ty++) {
        for (tx = 0; tx < SV_TILES_X; tx++) {
            cell = ty * 0x40 + tx;
            for (ir = 0; ir < 24; ir++) {
                for (ic = 0; ic < 24; ic++) {
                    g_sv_sprite[cell][ir * 0x18 + ic] =
                        sv_sprite_byte(tx, ty, ir, ic);
                }
            }
        }
    }
    /* table holds absolute sprite-data pointers per cell; cells outside
     * the populated grid get a harmless pointer (never read because the
     * map bounds guard rejects them). */
    for (cell = 0; cell < SV_NCELLS; cell++) {
        g_sv_table[cell] = (uint32)g_sv_sprite[0];
    }
    for (ty = 0; ty < SV_TILES_Y; ty++) {
        for (tx = 0; tx < SV_TILES_X; tx++) {
            cell = ty * 0x40 + tx;
            g_sv_table[cell] = (uint32)g_sv_sprite[cell];
        }
    }
}

/* Independent reference, derived from the rasterizer spec but written in
 * a deliberately different shape (per-pixel float-free recompute of the
 * fixed-point walk) to cross-check the emit rather than mirror it. */
static void ref_scaled_view(uint32 cx, uint32 cy, uint32 scale)
{
    int row;
    int col;
    int tx0;
    int ty0;
    int subx0;
    int suby0;
    int sx;
    int tx;
    int sy;
    int ty;
    int rowbase;
    long fx;
    long fy;

    memset(g_sv_ref, 0, 64000);

    fx = (long)(int)cx - (long)(int)scale * 0x9C;
    fy = (long)(int)cy - (long)(int)scale * 0x60;

    /* floor-toward-(-inf) split into tile index + sub-tile remainder,
     * matching the decompiled negative-remainder fixups. */
    tx0 = (int)(fx / 0xC00);
    subx0 = (int)(fx % 0xC00);
    if (subx0 < 0) { subx0 += 0xC00; tx0 -= 1; }
    ty0 = (int)(fy / 0xC00);
    suby0 = (int)(fy % 0xC00);
    if (suby0 < 0) { suby0 += 0xC00; ty0 -= 1; }

    sy = suby0;
    ty = ty0;
    for (row = 0; row < 0xC0; row++) {
        if (ty >= 0 && ty < (int)data_fd2_battle_map_height_tiles) {
            rowbase = (sy / 0x80) * 0x18;
            sx = subx0;
            tx = tx0;
            for (col = 0; col < 0x138; col++) {
                if (tx >= 0 && tx < (int)data_fd2_battle_map_width_tiles) {
                    uint32 sp = g_sv_table[ty * 0x40 + tx];
                    g_sv_ref[0x504 + row * 0x140 + col] =
                        *(uint8 *)(sp + rowbase + (sx / 0x80));
                }
                sx += (int)scale;
                if (sx > 0xBFF) { tx += 1; sx -= 0xC00; }
            }
        }
        sy += (int)scale;
        if (sy > 0xBFF) { ty += 1; sy -= 0xC00; }
    }
}

static int sv_compare_against_ref(void)
{
    /* compare the full memset extent so background-clear is also checked. */
    return memcmp(g_sv_surf, g_sv_ref, 64000);
}

/* scale 0x80: each output pixel steps the source by exactly one sprite
 * byte. Centering so the top-left source lands on tile (0,0) byte (0,0)
 * gives a clean 1:1 tile blit; verify against hand-computed bytes and
 * the reference. */
static void test_scaled_identity_scale_0x80(void)
{
    uint32 scale = 0x80u;
    uint32 cx;
    uint32 cy;

    setup_scaled_view();
    /* src_x_fp = cx - scale*0x9C ; want = 0 -> cx = 0x80*0x9C = 0x4E00 */
    cx = scale * 0x9Cu;
    /* src_y_fp = cy - scale*0x60 ; want = 0 -> cy = 0x80*0x60 = 0x3000 */
    cy = scale * 0x60u;

    fd2_blit_scaled_tile_map_view(cx, cy, scale, (uint32)g_sv_table);

    /* row 0, col 0 -> tile (0,0), inner (0,0) */
    ASSERT_EQ(g_sv_surf[0x504 + 0],
              sv_sprite_byte(0, 0, 0, 0));
    /* row 0, col 23 -> tile (0,0), inner row 0 col 23 */
    ASSERT_EQ(g_sv_surf[0x504 + 23],
              sv_sprite_byte(0, 0, 0, 23));
    /* row 0, col 24 -> next tile (1,0), inner (0,0) */
    ASSERT_EQ(g_sv_surf[0x504 + 24],
              sv_sprite_byte(1, 0, 0, 0));
    /* row 23, col 0 -> tile (0,0), inner row 23 col 0 */
    ASSERT_EQ(g_sv_surf[0x504 + 23 * 0x140 + 0],
              sv_sprite_byte(0, 0, 23, 0));
    /* row 24, col 0 -> tile (0,1), inner (0,0) */
    ASSERT_EQ(g_sv_surf[0x504 + 24 * 0x140 + 0],
              sv_sprite_byte(0, 1, 0, 0));

    ref_scaled_view(cx, cy, scale);
    ASSERT_EQ(sv_compare_against_ref(), 0);
}

/* the whole 64000-byte surface is memset to 0 first; with the camera far
 * below the map every row's tile_y starts past map_height and only
 * increases, so every row is rejected, leaving an all-zero surface. */
static void test_scaled_memset_clears_offmap(void)
{
    uint32 scale = 0x80u;
    uint32 cy;
    int i;

    setup_scaled_view();
    memset(g_sv_surf, 0xAB, sizeof(g_sv_surf)); /* sentinel before render */

    /* src_y_start = cy - scale*0x60 ; choose so tile_y0 = 100 (>> map_height,
     * which is SV_TILES_Y) and only grows -> every row rejected. */
    cy = (uint32)(100 * 0xC00) + scale * 0x60u;
    fd2_blit_scaled_tile_map_view(0x4E00u, cy, scale, (uint32)g_sv_table);

    for (i = 0; i < 64000; i++) {
        if (g_sv_surf[i] != 0) {
            ASSERT_EQ((int)g_sv_surf[i], 0); /* report first nonzero */
            return;
        }
    }
    /* bytes beyond the 64000 memset extent keep the sentinel */
    ASSERT_EQ((int)g_sv_surf[64000], 0xAB);
}

/* fractional scale (zoom-in): scale 0x40 < 0x80 magnifies, so each source
 * byte spans two output pixels. Pure cross-check against the reference. */
static void test_scaled_zoom_in_half_step(void)
{
    uint32 scale = 0x40u;
    uint32 cx = scale * 0x9Cu + 0x600u;  /* offset into tile (0,0) interior */
    uint32 cy = scale * 0x60u + 0x300u;

    setup_scaled_view();
    fd2_blit_scaled_tile_map_view(cx, cy, scale, (uint32)g_sv_table);
    ref_scaled_view(cx, cy, scale);
    ASSERT_EQ(sv_compare_against_ref(), 0);
}

/* zoom-out (scale > 0x80) skips source bytes; also lands the camera so the
 * top-left source is negative, exercising the tile_x--/tile_y-- and
 * +0xC00 sub-tile fixups for negative coords. */
static void test_scaled_zoom_out_negative_origin(void)
{
    uint32 scale = 0x140u;
    /* small center so cx - scale*0x9C and cy - scale*0x60 go negative */
    uint32 cx = 0x900u;
    uint32 cy = 0x500u;

    setup_scaled_view();
    fd2_blit_scaled_tile_map_view(cx, cy, scale, (uint32)g_sv_table);
    ref_scaled_view(cx, cy, scale);
    ASSERT_EQ(sv_compare_against_ref(), 0);
}

/* camera placed so the visible span runs off the right/bottom map edge:
 * out-of-map columns/rows stay background (0) while in-map ones render.
 * Cross-checked against the reference, which encodes the same per-axis
 * bounds guards. */
static void test_scaled_partial_offmap_edges(void)
{
    uint32 scale = 0x100u;
    /* center near the map's far corner so the right/bottom edge clips */
    uint32 cx = (uint32)(SV_TILES_X * 0xC00) - 0x600u + scale * 0x9Cu;
    uint32 cy = (uint32)(SV_TILES_Y * 0xC00) - 0x600u + scale * 0x60u;

    setup_scaled_view();
    fd2_blit_scaled_tile_map_view(cx, cy, scale, (uint32)g_sv_table);
    ref_scaled_view(cx, cy, scale);
    ASSERT_EQ(sv_compare_against_ref(), 0);

    /* sanity: the reference must contain at least one cleared (off-map)
     * pixel and at least one rendered pixel, so the edge-clip path is
     * actually exercised by this fixture. */
    {
        int i;
        int saw_zero = 0;
        int saw_nonzero = 0;
        for (i = 0x504; i < 64000; i++) {
            if (g_sv_ref[i] == 0) { saw_zero = 1; }
            else { saw_nonzero = 1; }
        }
        ASSERT_EQ(saw_zero, 1);
        ASSERT_EQ(saw_nonzero, 1);
    }
}

/* ================================================================
 * fd2_blit_scaled_chapter_pose @ 0x2FB9F
 *
 * Nearest-neighbour scale of a 320x200 source bitmap (stride 0x140)
 * into the full 320x200 working surface, centred on (src_cx, src_cy)
 * in 7-bit fixed-point. High-risk: fixed-point math, signed bounds
 * guards on each axis, nested loops, signed-shift fraction strip.
 *
 * Fixtures build a real 320x200 source whose bytes encode (row, col),
 * render into a real surface, then compare against an independent
 * reference implementation (ref_scaled_pose, written differently from
 * the emit) across identity / zoom-in / zoom-out-negative-origin /
 * partial-offmap / fully-offmap cases. The identity case is also
 * checked against hand-computed source bytes.
 * ================================================================ */

#define CP_W      0x140              /* source row stride (320) */
#define CP_H      200                /* source rows */
#define CP_BYTES  (CP_W * CP_H)      /* 64000 = source size */

static uint8 g_cp_src[CP_BYTES];

/* source byte encoding: identifies (row, col) with a nonzero value
 * (0 is reserved for the memset-cleared background). */
static uint8 cp_src_byte(int row, int col)
{
    return (uint8)(1u + (((unsigned)row * 7u + (unsigned)col) * 3u));
}

static void setup_chapter_pose(void)
{
    int row;
    int col;

    data_fd2_large_game_state_buffer_ptr = (uint32)g_sv_surf;
    for (row = 0; row < CP_H; row++) {
        for (col = 0; col < CP_W; col++) {
            g_cp_src[row * CP_W + col] = cp_src_byte(row, col);
        }
    }
}

/* Independent reference for the scaler, written in a deliberately
 * different shape (explicit signed source walk) to cross-check the
 * emit rather than mirror it. */
static void ref_scaled_pose(uint32 cx, uint32 cy, int scale)
{
    int row;
    int col;
    int sxs;
    int sx;
    int sy;
    int six;
    int siy;

    memset(g_sv_ref, 0, 64000);

    sxs = (int)cx - scale * 0xA0;
    sy = (int)cy - scale * 0x64;
    for (row = 0; row < 200; row++) {
        if (sy >= 0 && sy < 0x6400) {
            siy = sy >> 7;
            sx = sxs;
            for (col = 0; col < 0x140; col++) {
                if (sx >= 0 && sx < 0xA000) {
                    six = sx >> 7;
                    g_sv_ref[row * 0x140 + col] =
                        g_cp_src[siy * 0x140 + six];
                }
                sx += scale;
            }
        }
        sy += scale;
    }
}

static int cp_compare_against_ref(void)
{
    /* full memset extent so background-clear is also checked. */
    return memcmp(g_sv_surf, g_sv_ref, 64000);
}

/* scale 0x80: each output pixel steps the source by exactly one byte.
 * Centre so the top-left source lands on (0,0) -> clean 1:1 copy of
 * the 320x200 source. Hand-check several pixels and ref-compare. */
static void test_pose_identity_scale_0x80(void)
{
    int scale = 0x80;
    uint32 cx;
    uint32 cy;

    setup_chapter_pose();
    /* src_x_fp = cx - scale*0xA0 ; want 0 -> cx = 0x80*0xA0 = 0x5000 */
    cx = (uint32)(scale * 0xA0);
    /* src_y_fp = cy - scale*0x64 ; want 0 -> cy = 0x80*0x64 = 0x3200 */
    cy = (uint32)(scale * 0x64);

    fd2_blit_scaled_chapter_pose(cx, cy, (uint32)g_cp_src, scale);

    /* row 0 col 0 -> source (0,0) */
    ASSERT_EQ(g_sv_surf[0], cp_src_byte(0, 0));
    /* row 0 col 1 -> source (0,1) */
    ASSERT_EQ(g_sv_surf[1], cp_src_byte(0, 1));
    /* row 0 last col (0x13F) -> source (0, 0x13F) */
    ASSERT_EQ(g_sv_surf[0x13F], cp_src_byte(0, 0x13F));
    /* row 1 col 0 -> source (1,0) */
    ASSERT_EQ(g_sv_surf[0x140], cp_src_byte(1, 0));
    /* last row (199) col 0 -> source (199,0) */
    ASSERT_EQ(g_sv_surf[199 * 0x140], cp_src_byte(199, 0));
    /* last row col last -> source (199, 0x13F) */
    ASSERT_EQ(g_sv_surf[199 * 0x140 + 0x13F], cp_src_byte(199, 0x13F));

    ref_scaled_pose(cx, cy, scale);
    ASSERT_EQ(cp_compare_against_ref(), 0);
}

/* whole 64000-byte surface is memset to 0 first; with the camera far
 * below the bitmap every row's source-Y starts past 0x6400 and only
 * increases, so every row is rejected, leaving an all-zero surface
 * (and bytes beyond the 64000 extent keep their sentinel). */
static void test_pose_memset_clears_offmap(void)
{
    int scale = 0x80;
    uint32 cy;
    int i;

    setup_chapter_pose();
    memset(g_sv_surf, 0xAB, sizeof(g_sv_surf)); /* sentinel before render */

    /* src_y_fp = cy - scale*0x64 ; choose so it starts >= 0x6400 and
     * only grows (scale>0) -> every row rejected. */
    cy = (uint32)(0x6400 + scale * 0x64);
    fd2_blit_scaled_chapter_pose(0x5000u, cy, (uint32)g_cp_src, scale);

    for (i = 0; i < 64000; i++) {
        if (g_sv_surf[i] != 0) {
            ASSERT_EQ((int)g_sv_surf[i], 0); /* report first nonzero */
            return;
        }
    }
    ASSERT_EQ((int)g_sv_surf[64000], 0xAB);
}

/* zoom-in (scale 0x40 < 0x80) magnifies: each source byte spans two
 * output pixels. Centre offset into the bitmap interior. Pure
 * cross-check against the reference. */
static void test_pose_zoom_in_half_step(void)
{
    int scale = 0x40;
    uint32 cx = (uint32)(scale * 0xA0) + 0x2000u; /* interior centre */
    uint32 cy = (uint32)(scale * 0x64) + 0x1800u;

    setup_chapter_pose();
    fd2_blit_scaled_chapter_pose(cx, cy, (uint32)g_cp_src, scale);
    ref_scaled_pose(cx, cy, scale);
    ASSERT_EQ(cp_compare_against_ref(), 0);
}

/* zoom-out (scale 0x140 > 0x80) skips source bytes; small centre makes
 * the top-left source coordinate NEGATIVE, exercising the signed
 * lower-bound guard ((int)src_x_fp >= 0 / (int)src_y_fp >= 0) and the
 * arithmetic-shift fraction strip across the zero crossing. */
static void test_pose_zoom_out_negative_origin(void)
{
    int scale = 0x140;
    /* small centre so cx - scale*0xA0 and cy - scale*0x64 go negative */
    uint32 cx = 0x800u;
    uint32 cy = 0x500u;

    setup_chapter_pose();
    fd2_blit_scaled_chapter_pose(cx, cy, (uint32)g_cp_src, scale);
    ref_scaled_pose(cx, cy, scale);
    ASSERT_EQ(cp_compare_against_ref(), 0);

    /* sanity: this fixture must exercise both the rejected (cleared)
     * and the rendered paths, i.e. negative source coords really are
     * being clipped while in-range ones render. */
    {
        int i;
        int saw_zero = 0;
        int saw_nonzero = 0;
        for (i = 0; i < 64000; i++) {
            if (g_sv_ref[i] == 0) { saw_zero = 1; }
            else { saw_nonzero = 1; }
        }
        ASSERT_EQ(saw_zero, 1);
        ASSERT_EQ(saw_nonzero, 1);
    }
}

/* camera placed so the visible span runs off the right/bottom bitmap
 * edge: source coords >= 0xA000 (x) / 0x6400 (y) stay background while
 * in-range ones render. Cross-checked against the reference. */
static void test_pose_partial_offmap_edges(void)
{
    int scale = 0x100;
    /* centre near the far corner so the right/bottom edge clips */
    uint32 cx = (uint32)(0xA000 - 0x1000) + (uint32)(scale * 0xA0);
    uint32 cy = (uint32)(0x6400 - 0x1000) + (uint32)(scale * 0x64);

    setup_chapter_pose();
    fd2_blit_scaled_chapter_pose(cx, cy, (uint32)g_cp_src, scale);
    ref_scaled_pose(cx, cy, scale);
    ASSERT_EQ(cp_compare_against_ref(), 0);

    {
        int i;
        int saw_zero = 0;
        int saw_nonzero = 0;
        for (i = 0; i < 64000; i++) {
            if (g_sv_ref[i] == 0) { saw_zero = 1; }
            else { saw_nonzero = 1; }
        }
        ASSERT_EQ(saw_zero, 1);
        ASSERT_EQ(saw_nonzero, 1);
    }
}

/* ================================================================
 * fd2_blit_24x24_tile_to_battle_grid_position @ 0x3415E
 *
 * 24x24 tile blit helper for the battle preview/intro composer.
 * Leaf: resolves a sprite pointer from an atlas offset table, computes
 * dst = dst_buffer + dst_y*dst_row_stride + dst_x, and tail-calls the
 * passthrough blitter forwarding the caller-supplied stride (NOT the
 * fixed 0x1C8 the window-relative helpers use). Drives the real routine
 * and reads the painted destination byte(s) back.
 *
 * Reuses g_atlas (offset table at +6, probe value idx+1) and g_ws.
 * ================================================================ */

/* dst byte offset (into g_ws) = dst_y*stride + dst_x, computed directly from
 * the passed-through stride (not assumed 0x140). */
static uint32 grid_expect_off(uint32 dst_y, uint32 stride, uint32 dst_x)
{
    return dst_y * stride + dst_x;
}

/* Exactly one passthrough blit: src from atlas indexing (painted value idx+1),
 * dst from the dst_y*stride+dst_x formula. */
static void test_grid_blit_arg_forwarding(void)
{
    uint32 first;

    setup_blittile();
    /* arbitrary atlas index, dst buffer = g_ws, stride 0x140, (x,y) */
    fd2_blit_24x24_tile_to_battle_grid_position((uint32)g_atlas, 9u,
                                                (uint32)g_ws, 0x140u,
                                                0x96u, 0x4Bu);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    /* tile index 9 -> probe value 10, at the dst_y*stride+dst_x offset */
    ASSERT_EQ(bp_count_value(g_ws, sizeof(g_ws), 10u, &first), 1);
    ASSERT_EQ(first, grid_expect_off(0x4Bu, 0x140u, 0x96u));
}

/* The blit pitch is the caller's dst_row_stride, not a hardcoded 0x1C8: use a
 * distinctive stride and confirm both the dst arithmetic and the forwarded pitch
 * follow it (the two-pixel probe's second pixel lands one stride later). */
static void test_grid_blit_stride_passthrough(void)
{
    uint32 stride = 0x123u;
    uint32 base;

    setup_blittile();
    /* index 3 -> value 4, 2 pixels */
    bp_probe2(g_atlas + bp_atlas_slot_off(6u, ATLAS_N, ATLAS_SPAN, 3), 4u);
    fd2_blit_24x24_tile_to_battle_grid_position((uint32)g_atlas, 3u,
                                                (uint32)g_ws, stride,
                                                7u, 5u);
    base = grid_expect_off(5u, stride, 7u);
    ASSERT_EQ((int)g_ws[base], 4);
    ASSERT_EQ((int)g_ws[base + stride], 4);     /* forwarded pitch */
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 2);
}

/* tile_index 0 selects atlas entry 0 (src = atlas_base + table[0] -> slot 0's
 * probe); dst with x==0,y==0 == dst_buffer exactly. Matches the reserved-pos
 * highlight call site (tile id 0). */
static void test_grid_blit_index_zero_origin(void)
{
    uint32 first;

    setup_blittile();
    fd2_blit_24x24_tile_to_battle_grid_position((uint32)g_atlas, 0u,
                                                (uint32)g_ws, 0x140u,
                                                0u, 0u);
    ASSERT_EQ(bp_count_painted(g_ws, sizeof(g_ws)), 1);
    /* index 0 -> probe value 1, painted at offset 0 (dst == dst_buffer) */
    ASSERT_EQ(bp_count_value(g_ws, sizeof(g_ws), 1u, &first), 1);
    ASSERT_EQ(first, 0u);
}

void run_gfx_blittile_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/blittile\n");
    RUN_TEST(test_in_window_blit_args);
    RUN_TEST(test_in_window_blit_stride);
    RUN_TEST(test_origin_corner_in_window);
    RUN_TEST(test_far_corner_in_window);
    RUN_TEST(test_x_below_window_noop);
    RUN_TEST(test_x_at_right_edge_noop);
    RUN_TEST(test_y_below_window_noop);
    RUN_TEST(test_y_at_bottom_edge_noop);
    RUN_TEST(test_signed_lower_bound);
    RUN_TEST(test_anim_passthrough_branch);
    RUN_TEST(test_anim_passthrough_stride);
    RUN_TEST(test_anim_tile_id_masked_10_bits);
    RUN_TEST(test_anim_transparent_skip);
    RUN_TEST(test_anim_flip_offsets_sprite);
    RUN_TEST(test_anim_remap_branch);
    RUN_TEST(test_anim_x_below_margin_noop);
    RUN_TEST(test_anim_x_left_margin_in);
    RUN_TEST(test_anim_x_right_bound);
    RUN_TEST(test_anim_y_bottom_bound);
    RUN_TEST(test_anim_negative_y_noop);
    RUN_TEST(test_scaled_identity_scale_0x80);
    RUN_TEST(test_scaled_memset_clears_offmap);
    RUN_TEST(test_scaled_zoom_in_half_step);
    RUN_TEST(test_scaled_zoom_out_negative_origin);
    RUN_TEST(test_scaled_partial_offmap_edges);
    RUN_TEST(test_pose_identity_scale_0x80);
    RUN_TEST(test_pose_memset_clears_offmap);
    RUN_TEST(test_pose_zoom_in_half_step);
    RUN_TEST(test_pose_zoom_out_negative_origin);
    RUN_TEST(test_pose_partial_offmap_edges);
    RUN_TEST(test_grid_blit_arg_forwarding);
    RUN_TEST(test_grid_blit_stride_passthrough);
    RUN_TEST(test_grid_blit_index_zero_origin);
    printf("\n");
}
