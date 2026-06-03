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
extern int    g_blitdim_calls;
/* the real per-char paint reads the runtime_char array through this ptr */
extern runtime_char g_test_rc_array[8];
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
    g_shadow_overlay_calls = 0;
    g_check_char_is_dead_return = 0;   /* all party slots alive */
    g_terrain_hud_calls = 0;
    g_composite_call_count = 0;

    /* fd2_paint_cursor_overlay_pattern is now the real emitted routine; force a
     * single-blit phase so the compositor pipeline sees exactly one overlay blit
     * (cursor pattern semantics are covered by the dedicated tests below). */
    data_fd2_battle_anim_phase = 1;

    /* fd2_composite_all_chars_overlay now calls the real per-char paint; empty
     * the party so the overlay loop paints nothing and the only recorded blit
     * stays the single cursor-overlay one. The shadow-overlay stub still bumps
     * g_chars_overlay_calls once (unconditional), preserving that assertion. */
    data_fd2_battle_party_member_count = 0;

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
 * fd2_paint_char_sprite_at_world_pos — per-char sprite paint.
 *
 * Backing for the real routine: a sprite atlas whose offset table holds
 * table[i] == i, so the recorded src lets the test recover
 * sprite_idx = src - atlas_base. large_game_state_buffer is pinned to 0 so
 * the recorded dst == blit_offset directly. The window is opened wide so
 * the paint reaches the recording blitter.
 * ---------------------------------------------------------------- */
static uint8 g_paint_atlas[256 * 4];
static void install_paint_atlas(void)
{
    int i;
    int32 *table;

    table = (int32 *)g_paint_atlas;
    for (i = 0; i < 256; i++) {
        table[i] = i;                 /* src = atlas + idx -> recover idx */
    }
    portrait_sprite_cache = (uint32)g_paint_atlas;
}

/* Configure one runtime_char slot for the real paint. Returns nothing; the
 * caller drives fd2_paint_char_sprite_at_world_pos(slot). */
static void setup_paint_char(int slot, uint8 px, uint8 py, uint8 cache_idx,
                             uint8 facing, uint8 walk_phase, uint8 flags,
                             uint8 sleep)
{
    runtime_char *c = &g_test_rc_array[slot];
    memset(c, 0, sizeof(*c));
    c->pos_x = px;
    c->pos_y = py;
    c->sprite_state[0] = cache_idx;
    c->sprite_state[1] = facing;
    c->sprite_state[2] = walk_phase;
    c->flags = flags;
    c->status_sleep_flag = sleep;
}

static void reset_paint_window(void)
{
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x40;
    data_fd2_battle_view_window_max_y = 0x40;
    data_fd2_large_game_state_buffer_ptr = 0;   /* dst == blit_offset */
    data_fd2_graphics_char_sprite_shake_jitter_bit = 0;
    data_fd2_graphics_char_sprite_paint_jitter_tick_latch = (int32)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 0;
    g_blitpass_calls = 0;
    g_blitdim_calls = 0;
    install_paint_atlas();
}

/* expected blit_offset for given (px,py,walk_phase,pitch,jitter) at origin 0 */
static int32 expect_offset(int32 px, int32 py, int32 walk_phase, int32 pitch,
                           int32 jitter)
{
    return walk_phase * pitch + py * 0x2ac0 + px * 0x18 + jitter + 0x75d8;
}

/* facing 0 (down): pitch +0x720, not acted -> passthrough. Verify dst offset,
 * sprite_idx lookup, and that the passthrough (not dimmed) blitter ran. */
static void test_paint_facing_down_passthrough(void)
{
    int32 idx;

    reset_paint_window();
    /* facing 0, cache_idx 2, walk_phase 1, ambient palette 0 */
    setup_paint_char(0, 0x05, 0x03, 2, 0, 1, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);

    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitdim_calls, 0);
    ASSERT_EQ(g_blitpass_stride[0], 0x1c8u);
    ASSERT_EQ((int32)g_blitpass_dst[0],
              expect_offset(5, 3, 1, 0x720, 0));
    /* idx = facing*3 + cache_idx*0xC + palette(0) = 0 + 24 + 0 = 24 */
    idx = 0 * 3 + 2 * 0xc + 0;
    ASSERT_EQ(g_blitpass_src[0] - (uint32)g_paint_atlas, (uint32)idx);
}

/* facing 1 (left): pitch -4. facing 2 (up): pitch -0x720. facing 3 (right): +4.
 * Drives all three remaining facings and checks the dst offset. walk_phase 0 so
 * palette uses ambient idx (0). */
static void test_paint_facing_pitch_deltas(void)
{
    reset_paint_window();

    /* facing 1 -> pitch -4 */
    setup_paint_char(0, 0x08, 0x04, 0, 1, 3, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ((int32)g_blitpass_dst[0], expect_offset(8, 4, 3, -4, 0));

    /* facing 2 -> pitch -0x720 */
    setup_paint_char(0, 0x08, 0x04, 0, 2, 3, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ((int32)g_blitpass_dst[1], expect_offset(8, 4, 3, -0x720, 0));

    /* facing 3 -> pitch +4 */
    setup_paint_char(0, 0x08, 0x04, 0, 3, 3, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ((int32)g_blitpass_dst[2], expect_offset(8, 4, 3, 4, 0));

    ASSERT_EQ(g_blitpass_calls, 3);
}

/* acted (flags bit7 set) -> dimmed/grayscale blitter, not passthrough. */
static void test_paint_acted_dimmed(void)
{
    reset_paint_window();
    setup_paint_char(0, 0x05, 0x03, 0, 0, 0, 0x80, 0);
    fd2_paint_char_sprite_at_world_pos(0);

    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitdim_calls, 1);
    ASSERT_EQ((int32)g_blitpass_dst[0], expect_offset(5, 3, 0, 0x720, 0));
}

/* Out-of-window in each direction -> early return, no blit. The window margin
 * is origin_x-1 .. origin_x+max_x and origin_y-1 .. origin_y+max_y+1. */
static void test_paint_out_of_window(void)
{
    reset_paint_window();
    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x10;
    data_fd2_battle_view_window_max_x = 0x08;
    data_fd2_battle_view_window_max_y = 0x08;

    /* x below origin_x-1 (origin 0x10 -> min 0x0F) */
    setup_paint_char(0, 0x0e, 0x12, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ(g_blitpass_calls, 0);

    /* x above origin_x+max_x (0x10+0x08 = 0x18) */
    setup_paint_char(0, 0x19, 0x12, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ(g_blitpass_calls, 0);

    /* y below origin_y-1 (0x0F) */
    setup_paint_char(0, 0x12, 0x0e, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ(g_blitpass_calls, 0);

    /* y above origin_y+max_y+1 (0x10+0x08+1 = 0x19) */
    setup_paint_char(0, 0x12, 0x1a, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* In-window boundary: x == origin_x-1 and y == origin_y+max_y+1 are inclusive,
 * so a char exactly on those edges still paints. */
static void test_paint_window_boundary_inclusive(void)
{
    reset_paint_window();
    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x10;
    data_fd2_battle_view_window_max_x = 0x08;
    data_fd2_battle_view_window_max_y = 0x08;

    /* x == origin_x - 1 (lower inclusive edge), y == origin_y + max_y + 1 (upper) */
    setup_paint_char(0, 0x0f, 0x19, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ(g_blitpass_calls, 1);
}

/* Sleep status: (a) adds the shake jitter byte to blit_offset, (b) forces
 * palette 0. Set jitter bit = 1 and an alt palette != 0 to prove both. */
static void test_paint_sleep_jitter_and_palette(void)
{
    int32 idx;

    reset_paint_window();
    data_fd2_graphics_char_sprite_shake_jitter_bit = 1;
    /* walk_phase 0 -> ambient palette; set ambient to 2 to prove sleep forces 0 */
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 2;
    setup_paint_char(0, 0x05, 0x03, 1, 0, 0, 0x00, 1);   /* sleep=1 */
    fd2_paint_char_sprite_at_world_pos(0);

    ASSERT_EQ(g_blitpass_calls, 1);
    /* +1 jitter folded into the offset */
    ASSERT_EQ((int32)g_blitpass_dst[0], expect_offset(5, 3, 0, 0x720, 1));
    /* palette forced to 0: idx = facing*3 + cache_idx*0xC + 0 = 0 + 12 + 0 */
    idx = 0 * 3 + 1 * 0xc + 0;
    ASSERT_EQ(g_blitpass_src[0] - (uint32)g_paint_atlas, (uint32)idx);
}

/* Palette 3 falls back to 1 (non-sleep). Use walk_phase != 0 so the alt palette
 * idx feeds the selection, set it to 3, expect lookup palette component == 1. */
static void test_paint_palette3_fallback(void)
{
    int32 idx;

    reset_paint_window();
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 3;   /* walk -> alt */
    setup_paint_char(0, 0x05, 0x03, 0, 0, 1, 0x00, 0);         /* walk_phase 1 */
    fd2_paint_char_sprite_at_world_pos(0);

    ASSERT_EQ(g_blitpass_calls, 1);
    /* idx = facing(0)*3 + cache_idx(0)*0xC + palette(3->1) = 1 */
    idx = 0 * 3 + 0 * 0xc + 1;
    ASSERT_EQ(g_blitpass_src[0] - (uint32)g_paint_atlas, (uint32)idx);
}

/* jitter toggle: a changed BIOS tick latch flips the shake jitter bit once. */
static void test_paint_jitter_bit_toggles_on_tick_change(void)
{
    reset_paint_window();
    data_fd2_graphics_char_sprite_shake_jitter_bit = 0;
    /* force "tick changed" by setting the latch to a value that cannot equal
     * the live signed-word tick (0x46C word is 0..0xFFFF -> signed -0x8000..0x7FFF;
     * INT32_MIN is unreachable). */
    data_fd2_graphics_char_sprite_paint_jitter_tick_latch = (int32)0x80000000;
    setup_paint_char(0, 0x05, 0x03, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);

    /* bit flipped 0 -> 1, and the latch was updated to the live tick */
    ASSERT_EQ((uint32)data_fd2_graphics_char_sprite_shake_jitter_bit, 1u);
    ASSERT_EQ(data_fd2_graphics_char_sprite_paint_jitter_tick_latch,
              (int32)(int16)BIOS_TICK_WORD);
}

/* ----------------------------------------------------------------
 * fd2_composite_all_chars_overlay — loop / dead-skip / ordering.
 *
 * Real routine: for i in [0, party_member_count): if !is_dead(i)
 * fd2_paint_char_sprite_at_world_pos(i); then one unconditional shadow
 * overlay. fd2_check_char_is_dead is a testglob stub; the per-char paint
 * and shadow overlay are the real / stub routines. Each alive in-window
 * slot produces exactly one recorded blit; distinct pos_x per slot lets the
 * test recover which index painted (and in what order).
 * ---------------------------------------------------------------- */
static void reset_overlay_record(void)
{
    int i;

    g_shadow_overlay_calls = 0;
    g_chars_overlay_calls = 0;
    g_check_char_is_dead_return = 0;
    reset_paint_window();
    /* every slot in-window, not acted, awake, facing down, walk_phase 0.
     * slot i gets pos_x = 0x04 + i so the recovered x identifies the index. */
    for (i = 0; i < 8; i++) {
        setup_paint_char(i, (uint8)(0x04 + i), 0x03, 0, 0, 0, 0x00, 0);
    }
}

/* recover the painted slot index of recorded blit #i from its dst offset:
 * dst = py*0x2AC0 + px*0x18 + 0x75D8 (origin 0, walk_phase 0, no jitter),
 * so px = (dst - 3*0x2AC0 - 0x75D8) / 0x18, and index = px - 0x04. */
static uint32 recover_paint_index(int i)
{
    int32 off = (int32)g_blitpass_dst[i];
    int32 px = (off - 3 * 0x2ac0 - 0x75d8) / 0x18;
    return (uint32)(px - 0x04);
}

/* All party slots alive: paint every slot 0..count-1 in order, one shadow. */
static void test_overlay_all_alive(void)
{
    int i;

    reset_overlay_record();
    data_fd2_battle_party_member_count = 5;
    g_check_char_is_dead_return = 0;

    fd2_composite_all_chars_overlay();

    ASSERT_EQ(g_blitpass_calls, 5);
    for (i = 0; i < 5; i++) {
        ASSERT_EQ(recover_paint_index(i), (uint32)i);
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

    ASSERT_EQ(g_blitpass_calls, 0);
    ASSERT_EQ(g_shadow_overlay_calls, 1);
}

/* Empty party (count == 0): loop body never runs; shadow pass still runs.
 * Guards the (int) signed compare so count 0 does not underflow. */
static void test_overlay_empty_party(void)
{
    reset_overlay_record();
    data_fd2_battle_party_member_count = 0;

    fd2_composite_all_chars_overlay();

    ASSERT_EQ(g_blitpass_calls, 0);
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
    RUN_TEST(test_paint_facing_down_passthrough);
    RUN_TEST(test_paint_facing_pitch_deltas);
    RUN_TEST(test_paint_acted_dimmed);
    RUN_TEST(test_paint_out_of_window);
    RUN_TEST(test_paint_window_boundary_inclusive);
    RUN_TEST(test_paint_sleep_jitter_and_palette);
    RUN_TEST(test_paint_palette3_fallback);
    RUN_TEST(test_paint_jitter_bit_toggles_on_tick_change);
    RUN_TEST(test_overlay_all_alive);
    RUN_TEST(test_overlay_all_dead);
    RUN_TEST(test_overlay_empty_party);
    printf("\n");
}
