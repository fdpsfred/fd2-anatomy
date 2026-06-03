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
/* fd2_composite_all_chars_overlay per-char callee record (testglob.c) */
extern int    g_paint_char_calls;
extern uint32 g_paint_char_idx[64];
extern int    g_shadow_overlay_calls;
extern int    g_check_char_is_dead_return;
extern int    g_terrain_hud_calls;
extern uint32 g_terrain_hud_last_buf;
extern uint32 g_terrain_hud_last_stride;
extern int    g_composite_call_count;
/* recording stub for fd2_tile_blit_24x24_passthrough (testglob.c). The real
 * fd2_blit_24x24_at_window_relative_pos (src/gfx/blittile.c) forwards every
 * in-window blit to it; recording (src, dst) lets these tests reconstruct the
 * (world_x, world_y, sprite_idx) the caller computed. */
extern int    g_blitpass_calls;
extern uint32 g_blitpass_src[64];
extern uint32 g_blitpass_dst[64];
extern uint32 g_blitpass_stride[64];
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
    g_blitpass_calls = 0;
    g_chars_overlay_calls = 0;
    g_paint_char_calls = 0;
    g_shadow_overlay_calls = 0;
    g_check_char_is_dead_return = 0;   /* all party slots alive */
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

/* Sprite atlas backing data_fd2_runtime_battle_state_ptr. The real blit reads a
 * 4-byte absolute offset from the table at +6 (index*4), so an identity-ish
 * table where table[i] == i lets the tests recover sprite_idx from the recorded
 * src pointer: sprite_idx == src - atlas_base. 64 entries cover all cursor
 * sprite indices (0x00..0x12). */
static uint8 g_sprite_atlas[6 + 64 * 4 + 4];

static void install_sprite_atlas(void)
{
    int i;
    uint32 *table;

    table = (uint32 *)(g_sprite_atlas + 6);
    for (i = 0; i < 64; i++) {
        table[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_sprite_atlas;
}

/* Make the battle window large enough that every cursor-pattern coord is
 * in-window, so each caller blit reaches the recording passthrough stub. */
static void install_full_window(void)
{
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ws_buffer - 0x8088;
}

/* Reconstruct (world_x, world_y, sprite_idx) of recorded blit #i from the
 * (src, dst) the real fd2_blit_24x24_at_window_relative_pos forwarded.
 *   dst = base + (y-oy)*0x2AC0 + (x-ox)*0x18 + 0x8088
 *   src = atlas + table[sprite_idx]  (table[idx] == idx here) */
static void recover_blit(int i, uint32 *x, uint32 *y, uint32 *s)
{
    uint32 base;
    uint32 oy;
    uint32 ox;
    uint32 rel;

    base = data_fd2_large_game_state_buffer_ptr;
    oy = data_fd2_battle_view_window_origin_y;
    ox = data_fd2_battle_view_window_origin_x;
    rel = g_blitpass_dst[i] - base - 0x8088u;
    *y = rel / 0x2AC0u + oy;
    *x = (rel % 0x2AC0u) / 0x18u + ox;
    *s = g_blitpass_src[i] - (uint32)g_sprite_atlas;
}


/*
 * Full pipeline with skip_palette_cycle == 0: every stage runs exactly once
 * and receives the correct workspace address + pixel constants. ws is the
 * render back-buffer at large_game_state_buffer_ptr + 0x8088; the overlay blit
 * forwards to the passthrough stub exactly once (anim phase 1).
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
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    install_sprite_atlas();
    data_fd2_battle_cursor_world_x = 0x14;
    data_fd2_battle_cursor_world_y = 0x25;
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

    ASSERT_EQ(g_blitpass_calls, 1);
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
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    install_sprite_atlas();
    data_fd2_battle_cursor_world_x = 0x03;
    data_fd2_battle_cursor_world_y = 0x03;
    reset_pipeline_record();

    fd2_composite_battle_frame(1);

    ASSERT_EQ(g_tile_map_calls, 1);
    ASSERT_EQ(g_tile_map_last_dst, ws);
    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_chars_overlay_calls, 1);
    ASSERT_EQ(g_terrain_hud_calls, 1);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* ----------------------------------------------------------------
 * fd2_paint_cursor_overlay_pattern — per-phase pattern verification.
 *
 * Drives the real routine for each anim phase and asserts the exact
 * sequence of (world_x, world_y, sprite_idx) blits, matching the
 * 0x122DC disassembly. The window is set wide enough that every coord is
 * in-window, so each caller blit forwards to the recording passthrough
 * stub; the (x, y, sprite) tuple is reconstructed from (dst, src).
 * ---------------------------------------------------------------- */
#define CX 0x14u
#define CY 0x0Au

static void set_cursor_phase(uint32 phase)
{
    g_blitpass_calls = 0;
    install_full_window();
    install_sprite_atlas();
    data_fd2_battle_cursor_world_x = CX;
    data_fd2_battle_cursor_world_y = CY;
    data_fd2_battle_anim_phase = phase;
}

static void check_blit(int i, uint32 ex, uint32 ey, uint32 es)
{
    uint32 x;
    uint32 y;
    uint32 s;

    recover_blit(i, &x, &y, &s);
    ASSERT_EQ(x, ex);
    ASSERT_EQ(y, ey);
    ASSERT_EQ(s, es);
}

static void test_cursor_phase1(void)
{
    set_cursor_phase(1);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blitpass_calls, 1);
    check_blit(0, CX, CY, 0);
}

static void test_cursor_phase2(void)
{
    set_cursor_phase(2);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blitpass_calls, 1);
    check_blit(0, CX, CY, 1);
}

static void test_cursor_phase3(void)
{
    set_cursor_phase(3);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blitpass_calls, 5);
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
    ASSERT_EQ(g_blitpass_calls, 13);
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
    ASSERT_EQ(g_blitpass_calls, 21);
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

    ASSERT_EQ(g_blitpass_calls, 0);
    ASSERT_EQ((uint32)g_cursor_tile_map[idx], 0u);
}

/* default (unhandled phase): no blit, no memory write. */
static void test_cursor_phase_default(void)
{
    set_cursor_phase(99);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* ----------------------------------------------------------------
 * fd2_composite_all_chars_overlay — loop / dead-skip / ordering.
 *
 * Real routine: for i in [0, party_member_count): if !is_dead(i)
 * paint_char_sprite(i); then one unconditional shadow overlay.
 * Callees are testglob stubs that record (index list, shadow count).
 * ---------------------------------------------------------------- */
static void reset_overlay_record(void)
{
    g_paint_char_calls = 0;
    g_shadow_overlay_calls = 0;
    g_chars_overlay_calls = 0;
    g_check_char_is_dead_return = 0;
}

/* All party slots alive: paint every slot 0..count-1 in order, one shadow. */
static void test_overlay_all_alive(void)
{
    int i;

    reset_overlay_record();
    data_fd2_battle_party_member_count = 5;
    g_check_char_is_dead_return = 0;

    fd2_composite_all_chars_overlay();

    ASSERT_EQ(g_paint_char_calls, 5);
    for (i = 0; i < 5; i++) {
        ASSERT_EQ(g_paint_char_idx[i], (uint32)i);
    }
    /* shadow overlay runs exactly once, after the loop */
    ASSERT_EQ(g_shadow_overlay_calls, 1);
}

/* All party slots dead: every slot skipped, still exactly one shadow pass. */
static void test_overlay_all_dead(void)
{
    reset_overlay_record();
    data_fd2_battle_party_member_count = 4;
    g_check_char_is_dead_return = 1;

    fd2_composite_all_chars_overlay();

    ASSERT_EQ(g_paint_char_calls, 0);
    ASSERT_EQ(g_shadow_overlay_calls, 1);
}

/* Empty party (count == 0): loop body never runs; shadow pass still runs.
 * Guards the (int) signed compare so count 0 does not underflow. */
static void test_overlay_empty_party(void)
{
    reset_overlay_record();
    data_fd2_battle_party_member_count = 0;

    fd2_composite_all_chars_overlay();

    ASSERT_EQ(g_paint_char_calls, 0);
    ASSERT_EQ(g_shadow_overlay_calls, 1);
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
    RUN_TEST(test_overlay_all_alive);
    RUN_TEST(test_overlay_all_dead);
    RUN_TEST(test_overlay_empty_party);
    printf("\n");
}
