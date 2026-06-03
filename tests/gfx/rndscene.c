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
extern int    g_cursor_overlay_calls;
extern int    g_chars_overlay_calls;
extern int    g_terrain_hud_calls;
extern uint32 g_terrain_hud_last_buf;
extern uint32 g_terrain_hud_last_stride;
extern int    g_composite_call_count;
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
    g_cursor_overlay_calls = 0;
    g_chars_overlay_calls = 0;
    g_terrain_hud_calls = 0;
    g_composite_call_count = 0;

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

    ASSERT_EQ(g_cursor_overlay_calls, 1);
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
    ASSERT_EQ(g_cursor_overlay_calls, 1);
    ASSERT_EQ(g_chars_overlay_calls, 1);
    ASSERT_EQ(g_terrain_hud_calls, 1);
    ASSERT_EQ(g_composite_call_count, 1);
}


void run_gfx_rndscene_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/rndscene\n");
    RUN_TEST(test_composite_pipeline_args);
    RUN_TEST(test_composite_skip_palette_cycle);
    printf("\n");
}
