/*
 * unit tests for src/gfx/rndscene.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* pipeline-callee recording vars (defined in testglob.c) */
extern int    g_tile_map_calls;
extern uint32 g_tile_map_last_dst;
extern uint32 g_tile_map_last_stride;
extern uint32 g_tile_map_last_w;
extern uint32 g_tile_map_last_h;
extern uint32 g_tile_map_last_ox;
extern uint32 g_tile_map_last_oy;
extern int    g_chars_overlay_calls;
extern int    g_terrain_hud_calls;
extern uint32 g_terrain_hud_last_buf;
extern uint32 g_terrain_hud_last_stride;
extern int    g_composite_call_count;
/* recording stub for fd2_blit_24x24_at_window_relative_pos (testglob.c) */
extern int    g_blit24_calls;
extern uint32 g_blit24_x[64];
extern uint32 g_blit24_y[64];
extern uint32 g_blit24_sprite[64];
/* The compositor's final stage is the real fd2_blit_rectangle (src/gfx/blitspr.c).
 * It memmoves the visible 312x192 region from the workspace (src == ws) to the
 * mode13h primary at 0xA0504 (VGA RAM, writable under DOS/4GW). To keep the
 * read side host-safe we point the workspace at a real allocated buffer large
 * enough to span (h-1)*sstride + w = 191*456 + 312 bytes; the VGA-side write
 * lands in emulated video memory and is harmless. */
#define WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ws_buffer[WS_SPAN];

static void reset_pipeline_record(void)
{
    g_tile_map_calls = 0;
    g_blit24_calls = 0;
    g_chars_overlay_calls = 0;
    g_terrain_hud_calls = 0;
    g_composite_call_count = 0;

    /* fd2_paint_cursor_overlay_pattern is now the real emitted routine; force a
     * single-blit phase so the compositor pipeline sees exactly one overlay blit
     * (cursor pattern semantics are covered by the dedicated tests below). */
    data_fd2_battle_anim_phase = 1;

    /* throttle the real palette-cycle routine to its early-return path
     * (last_tick == now) so it performs NO VGA port writes when invoked;
     * keeps the test host-safe and deterministic. */
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;
}


/*
 * Full pipeline with skip_palette_cycle == 0: every stage runs exactly once
 * and receives the correct workspace address + pixel constants. ws is the
 * render back-buffer at large_game_state_buffer_ptr + 0x8088; the blit reads
 * directly from ws (src == ws), not from an offset sub-region.
 */
static void test_composite_pipeline_args(void)
{
    uint32 ws;

    /* ws is the render back-buffer at large_game_state_buffer_ptr + 0x8088;
     * back it with a real allocation so the real blit's source reads are safe. */
    ws = (uint32)g_ws_buffer;
    data_fd2_large_game_state_buffer_ptr = ws - 0x8088;
    data_fd2_battle_view_window_origin_x = 0x11;
    data_fd2_battle_view_window_origin_y = 0x22;
    reset_pipeline_record();

    fd2_composite_battle_frame(0);

    /* tile map: (ws, 456, 13, 8, origin_x, origin_y) */
    ASSERT_EQ(g_tile_map_calls, 1);
    ASSERT_EQ(g_tile_map_last_dst, ws);
    ASSERT_EQ(g_tile_map_last_stride, 0x1c8u);
    ASSERT_EQ(g_tile_map_last_w, 0xdu);
    ASSERT_EQ(g_tile_map_last_h, 8u);
    ASSERT_EQ(g_tile_map_last_ox, 0x11u);
    ASSERT_EQ(g_tile_map_last_oy, 0x22u);

    ASSERT_EQ(g_blit24_calls, 1);
    ASSERT_EQ(g_chars_overlay_calls, 1);

    /* terrain HUD: (ws, 456) */
    ASSERT_EQ(g_terrain_hud_calls, 1);
    ASSERT_EQ(g_terrain_hud_last_buf, ws);
    ASSERT_EQ(g_terrain_hud_last_stride, 0x1c8u);

    /* final stage = real fd2_blit_rectangle(0xA0504, 320, ws, 456, 312, 192);
     * composite ran to completion (tile-map proxy counts it once). The blit's
     * own copy semantics are covered by tests/gfx/blitspr.c. */
    ASSERT_EQ(g_composite_call_count, 1);
}


/*
 * skip_palette_cycle != 0 takes the branch that omits fd2_update_palette_cycle_anim,
 * but the rest of the compositing pipeline must still run identically. The
 * distinguishing side-effect of the omitted call is VGA-DAC port output (deferred
 * to Phase 9 integration); here we confirm the branch is reachable and the full
 * pipeline still composites + blits with the same arguments.
 */
static void test_composite_skip_palette_cycle(void)
{
    uint32 ws;

    ws = (uint32)g_ws_buffer;
    data_fd2_large_game_state_buffer_ptr = ws - 0x8088;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    reset_pipeline_record();

    fd2_composite_battle_frame(1);

    ASSERT_EQ(g_tile_map_calls, 1);
    ASSERT_EQ(g_tile_map_last_dst, ws);
    ASSERT_EQ(g_blit24_calls, 1);
    ASSERT_EQ(g_chars_overlay_calls, 1);
    ASSERT_EQ(g_terrain_hud_calls, 1);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* ----------------------------------------------------------------
 * fd2_paint_cursor_overlay_pattern — per-phase pattern verification.
 *
 * Drives the real routine for each anim phase and asserts the exact
 * sequence of (world_x, world_y, sprite_idx) blit calls, matching the
 * 0x122DC disassembly. Cursor at a fixed (x, y); the blit stub records
 * every call. ---------------------------------------------------------------- */
#define CX 0x14u
#define CY 0x0Au

static void set_cursor_phase(uint32 phase)
{
    g_blit24_calls = 0;
    data_fd2_battle_cursor_world_x = CX;
    data_fd2_battle_cursor_world_y = CY;
    data_fd2_battle_anim_phase = phase;
}

static void check_blit(int i, uint32 ex, uint32 ey, uint32 es)
{
    ASSERT_EQ(g_blit24_x[i], ex);
    ASSERT_EQ(g_blit24_y[i], ey);
    ASSERT_EQ(g_blit24_sprite[i], es);
}

static void test_cursor_phase1(void)
{
    set_cursor_phase(1);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blit24_calls, 1);
    check_blit(0, CX, CY, 0);
}

static void test_cursor_phase2(void)
{
    set_cursor_phase(2);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blit24_calls, 1);
    check_blit(0, CX, CY, 1);
}

static void test_cursor_phase3(void)
{
    set_cursor_phase(3);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blit24_calls, 5);
    check_blit(0, CX,     CY,     0xe);
    check_blit(1, CX,     CY - 1, 2);
    check_blit(2, CX - 1, CY,     3);
    check_blit(3, CX + 1, CY,     4);
    check_blit(4, CX,     CY + 1, 5);
}

static void test_cursor_phase4(void)
{
    set_cursor_phase(4);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blit24_calls, 13);
    check_blit(0,  CX,     CY,     1);
    check_blit(1,  CX,     CY - 2, 2);
    check_blit(2,  CX - 2, CY,     3);
    check_blit(3,  CX + 2, CY,     4);
    check_blit(4,  CX,     CY + 2, 5);
    check_blit(5,  CX - 1, CY - 1, 6);
    check_blit(6,  CX + 1, CY - 1, 7);
    check_blit(7,  CX - 1, CY + 1, 8);
    check_blit(8,  CX + 1, CY + 1, 9);
    check_blit(9,  CX,     CY - 1, 0xa);
    check_blit(10, CX - 1, CY,     0xb);
    check_blit(11, CX + 1, CY,     0xc);
    check_blit(12, CX,     CY + 1, 0xd);
}

static void test_cursor_phase5(void)
{
    set_cursor_phase(5);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blit24_calls, 21);
    check_blit(0,  CX,     CY,     1);
    check_blit(1,  CX,     CY - 3, 2);
    check_blit(2,  CX - 3, CY,     3);
    check_blit(3,  CX + 3, CY,     4);
    check_blit(4,  CX,     CY + 3, 5);
    check_blit(5,  CX - 1, CY - 2, 6);
    check_blit(6,  CX - 2, CY - 1, 6);
    check_blit(7,  CX + 1, CY - 2, 7);
    check_blit(8,  CX + 2, CY - 1, 7);
    check_blit(9,  CX - 1, CY + 2, 8);
    check_blit(10, CX - 2, CY + 1, 8);
    check_blit(11, CX + 1, CY + 2, 9);
    check_blit(12, CX + 2, CY + 1, 9);
    check_blit(13, CX,     CY - 2, 0xa);
    check_blit(14, CX - 2, CY,     0xb);
    check_blit(15, CX + 2, CY,     0xc);
    check_blit(16, CX,     CY + 2, 0xd);
    check_blit(17, CX - 1, CY - 1, 0xf);
    check_blit(18, CX + 1, CY - 1, 0x10);
    check_blit(19, CX - 1, CY + 1, 0x11);
    check_blit(20, CX + 1, CY + 1, 0x12);
}

/* phase 6: clear cursor flag byte tile_map[(y*map_width + x)*4 + 7] = 0;
 * no blit calls. */
static uint8 g_cursor_tile_map[4096];
static void test_cursor_phase6_clear_flag(void)
{
    uint32 idx;

    set_cursor_phase(6);
    data_fd2_battle_map_width_tiles = 0x10;          /* map stride in tiles */
    data_fd2_battle_tile_map_ptr = (uint32)g_cursor_tile_map;
    idx = ((CY * 0x10u + CX) * 4u) + 7u;
    g_cursor_tile_map[idx] = 0xAB;                   /* pre-set non-zero */

    fd2_paint_cursor_overlay_pattern();

    ASSERT_EQ(g_blit24_calls, 0);
    ASSERT_EQ((uint32)g_cursor_tile_map[idx], 0u);
}

/* default (unhandled phase): no blit, no memory write. */
static void test_cursor_phase_default(void)
{
    set_cursor_phase(99);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blit24_calls, 0);
}

void run_gfx_rndscene_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/rndscene\n");
    RUN_TEST(test_composite_pipeline_args);
    RUN_TEST(test_composite_skip_palette_cycle);
    RUN_TEST(test_cursor_phase1);
    RUN_TEST(test_cursor_phase2);
    RUN_TEST(test_cursor_phase3);
    RUN_TEST(test_cursor_phase4);
    RUN_TEST(test_cursor_phase5);
    RUN_TEST(test_cursor_phase6_clear_flag);
    RUN_TEST(test_cursor_phase_default);
    printf("\n");
}
