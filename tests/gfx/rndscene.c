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
extern int    g_check_char_is_dead_return;
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
/* recording stub for fd2_tile_blit_24x24_solid_color (testglob.c). It records
 * (src,dst,stride) into the shared g_blitpass_* arrays plus the 4th (color)
 * arg into g_blitsolid_color[], and bumps g_blitsolid_calls; lets the
 * mode-aware paint test verify the solid-color overlay dispatch + dst/src. */
extern int    g_blitsolid_calls;
extern uint32 g_blitsolid_color[64];
/* recording stub for fd2_blit_sprite_with_decoded_pixels (testglob.c); the
 * spell-effect overlay hit branch forwards (dst, sprite, stride) here. */
extern uint32 g_blitdec_dst;
extern uint32 g_blitdec_sprite;
extern uint32 g_blitdec_stride;
extern int    g_blitdec_calls;
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
    g_check_char_is_dead_return = 0;   /* all party slots alive */
    g_composite_call_count = 0;

    /* fd2_render_terrain_info_hud_panel is now the real emitted routine; keep
     * its HUD-enable gate OFF so the compositor's HUD call early-returns and
     * contributes no extra passthrough blit (its full behavior is covered by
     * tests/gfx/rndstat.c). */
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;

    /* fd2_paint_cursor_overlay_pattern is now the real emitted routine; force a
     * single-blit phase so the compositor pipeline sees exactly one overlay blit
     * (cursor pattern semantics are covered by the dedicated tests below). */
    data_fd2_battle_anim_phase = 1;

    /* fd2_composite_all_chars_overlay now calls the real per-char paint and the
     * real fd2_paint_chars_shadow_overlay; empty the party so both loops iterate
     * zero times and the only recorded passthrough blit stays the single
     * cursor-overlay one. */
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

/* ----------------------------------------------------------------
 * Tile-map fixture for the real fd2_blit_animated_tile_at_pos.
 *
 * The routine reads the tile id from the word at tile_meta_addr+4 and
 * the cursor-overlay flag from the byte at tile_meta_addr+7, where
 * tile_meta_addr = map_ptr + (y*W + x)*4. The +4/+7 read therefore
 * lands in the FOLLOWING 4-byte slot, so filling every slot with
 * {0x01, 0x00, 0x00, 0xFF} makes any cell resolve to tile id 1 with
 * overlay flag 0xFF (no overlay -> plain passthrough branch).
 *
 * The tile-attribute buffer entry for tile id 1 (stride 4) carries the
 * renderable bit 0x80 when `renderable` is set; bit 0x08 stays clear so
 * no anim-flip occurs. With renderable set, every in-window (x, y) the
 * shadow overlay asks to paint reaches the recording passthrough stub,
 * and dst encodes (x, y) (recovered by recover_anim_tile). With
 * renderable clear the routine reads attr then returns without blitting
 * (used where only the *caller's* loop is under test). map width is
 * fixed at 0x20 so (y*0x20 + x) covers every shadow-test coord. */
#define ANIM_MAP_W      0x20
#define ANIM_MAP_ROWS   0x20
static uint8 g_anim_tile_map[ANIM_MAP_W * ANIM_MAP_ROWS * 4];
static uint8 g_anim_attr_buf[64];
static uint8 g_anim_scene_snapshot[64];

static void install_anim_tile_map(int renderable)
{
    int i;

    for (i = 0; i < ANIM_MAP_W * ANIM_MAP_ROWS; i++) {
        g_anim_tile_map[i * 4 + 0] = 0x01;  /* slot k+1 low byte -> tile id 1 */
        g_anim_tile_map[i * 4 + 1] = 0x00;
        g_anim_tile_map[i * 4 + 2] = 0x00;
        g_anim_tile_map[i * 4 + 3] = 0xFF;  /* slot k+1 byte 3 -> no overlay */
    }
    memset(g_anim_attr_buf, 0, sizeof(g_anim_attr_buf));
    g_anim_attr_buf[1 * 4] = renderable ? 0x80 : 0x00;
    memset(g_anim_scene_snapshot, 0, sizeof(g_anim_scene_snapshot));

    data_fd2_battle_tile_map_ptr = (uint32)g_anim_tile_map;
    data_fd2_battle_map_width_tiles = ANIM_MAP_W;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_anim_attr_buf;
    battle_scene_snapshot = (uint32)g_anim_scene_snapshot;
    data_fd2_graphics_bg_anim_flip_flag = 0;
}

/* recover (x, y) of recorded anim-tile blit #i from its dst offset:
 *   dst = buf + 0x8088 + (y-oy)*0x2AC0 + (x-ox)*0x18    (oy=ox=0 here) */
static void recover_anim_tile(int i, uint32 buf, int32 *x, int32 *y)
{
    uint32 rel = g_blitpass_dst[i] - buf - 0x8088u;
    *y = (int32)(rel / 0x2AC0u);
    *x = (int32)((rel % 0x2AC0u) / 0x18u);
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

    /* empty party -> real chars overlay + shadow overlay both paint nothing;
     * the real terrain HUD is gated OFF (early-return, no blit), so the single
     * passthrough blit is the cursor overlay's. The compositor->terrain-HUD
     * call itself (ws, 456) and the HUD's own rendering are covered in
     * tests/gfx/rndstat.c. */
    ASSERT_EQ(g_blitpass_calls, 1);

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
 * fd2_paint_char_sprite_at_world_with_mode — mode-aware per-char paint
 * onto a caller-supplied surface.
 *
 * Reuses g_paint_atlas (portrait_sprite_cache, table[i]==i) so the
 * recorded src recovers sprite_idx. dst_buf is the test-chosen base
 * (a large constant so the negative -stride*6 term never underflows
 * a check), and the recorded dst is matched against the closed-form
 * expression below. The shared passthrough / solid-color recording
 * stubs (g_blitpass_*, g_blitsolid_*) capture every blit.
 * ---------------------------------------------------------------- */

/* Window wide-open at origin 0 so every test coord is in-window, and the
 * mode-paint atlas installed. dst_buf/stride are supplied per call. */
static void reset_mode_paint(void)
{
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x40;
    data_fd2_battle_view_window_max_y = 0x40;
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 0;
    g_blitpass_calls = 0;
    g_blitdim_calls = 0;
    g_blitsolid_calls = 0;
    install_paint_atlas();
}

/* Closed-form dst for the mode-aware paint at origin 0:
 *   dst = dst_buf - stride*6 + py*stride*0x18 + px*0x18 + walk_phase*x_offset */
static uint32 expect_mode_dst(uint32 dst_buf, uint32 stride, int32 px, int32 py,
                              uint32 walk_phase, uint32 x_offset)
{
    return dst_buf - stride * 6 +
           (uint32)py * stride * 0x18 +
           (uint32)px * 0x18 +
           walk_phase * x_offset;
}

/* mode 0 (passthrough): full dst arithmetic + sprite_idx lookup with a
 * non-trivial stride. facing 0 (down) -> x_offset = stride<<2, walk_phase 1
 * so the jitter term contributes. */
static void test_mode_passthrough_dst_and_src(void)
{
    uint32 stride = 0x140;
    uint32 base = 0x00100000;
    uint32 xoff;
    int32 idx;

    reset_mode_paint();
    /* facing 0, cache_idx 2, walk_phase 1, ambient palette 0 */
    setup_paint_char(0, 0x05, 0x03, 2, 0, 1, 0x00, 0);
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);

    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitsolid_calls, 0);
    ASSERT_EQ(g_blitpass_stride[0], stride);
    xoff = stride << 2;                  /* facing 0 -> stride*4 */
    ASSERT_EQ(g_blitpass_dst[0], expect_mode_dst(base, stride, 5, 3, 1, xoff));
    /* idx = facing*3 + cache_idx*0xC + palette(0) = 0 + 24 + 0 = 24 */
    idx = 0 * 3 + 2 * 0xc + 0;
    ASSERT_EQ(g_blitpass_src[0] - (uint32)g_paint_atlas, (uint32)idx);
}

/* All four facings drive distinct x_offsets; walk_phase 1 so each appears in
 * the dst directly. facing 0: stride<<2, 1: -4, 2: -(stride<<2), 3: +4. */
static void test_mode_facing_x_offsets(void)
{
    uint32 stride = 0x140;
    uint32 base = 0x00100000;

    reset_mode_paint();

    setup_paint_char(0, 0x06, 0x04, 0, 0, 1, 0x00, 0);   /* facing 0 (down) */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);
    ASSERT_EQ(g_blitpass_dst[0],
              expect_mode_dst(base, stride, 6, 4, 1, stride << 2));

    setup_paint_char(0, 0x06, 0x04, 0, 1, 1, 0x00, 0);   /* facing 1 (left) -> -4 */
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);
    ASSERT_EQ(g_blitpass_dst[1],
              expect_mode_dst(base, stride, 6, 4, 1, 0xfffffffc));

    setup_paint_char(0, 0x06, 0x04, 0, 2, 1, 0x00, 0);   /* facing 2 (up) -> -(stride<<2) */
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);
    ASSERT_EQ(g_blitpass_dst[2],
              expect_mode_dst(base, stride, 6, 4, 1, (0 - stride) << 2));

    setup_paint_char(0, 0x06, 0x04, 0, 3, 1, 0x00, 0);   /* facing 3 (right) -> +4 */
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);
    ASSERT_EQ(g_blitpass_dst[3],
              expect_mode_dst(base, stride, 6, 4, 1, 4));

    ASSERT_EQ(g_blitpass_calls, 4);
}

/* walk_phase 0 zeroes the jitter term entirely (x_offset * 0). Even facing 2
 * (whose x_offset is large/negative) must not move the dst. */
static void test_mode_walkphase0_no_jitter(void)
{
    uint32 stride = 0x140;
    uint32 base = 0x00100000;

    reset_mode_paint();
    setup_paint_char(0, 0x06, 0x04, 0, 2, 0, 0x00, 0);   /* facing 2, walk_phase 0 */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);

    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitpass_dst[0], expect_mode_dst(base, stride, 6, 4, 0, 0));
}

/* mode 2 (solid color): routes to the solid-color blitter, which receives the
 * stride as arg3 and the caller color as arg4. The passthrough blitter must
 * NOT run. */
static void test_mode_solid_color_dispatch(void)
{
    uint32 stride = 0x140;
    uint32 base = 0x00100000;
    int32 idx;

    reset_mode_paint();
    setup_paint_char(0, 0x05, 0x03, 2, 1, 1, 0x00, 0);   /* facing 1 -> x_offset -4 */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 2, 0x37);

    ASSERT_EQ(g_blitsolid_calls, 1);
    /* exactly one blit total, and it was solid-color (not passthrough) */
    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_blitpass_stride[0], stride);     /* arg3 = dst_stride */
    ASSERT_EQ(g_blitsolid_color[0], 0x37u);      /* arg4 = color, passed verbatim */
    ASSERT_EQ(g_blitpass_dst[0],
              expect_mode_dst(base, stride, 5, 3, 1, 0xfffffffc));
    /* sprite still resolved through the cache:
     * idx = facing(1)*3 + cache_idx(2)*0xC + palette(0) = 3 + 24 = 27 */
    idx = 1 * 3 + 2 * 0xc + 0;
    ASSERT_EQ(g_blitpass_src[0] - (uint32)g_paint_atlas, (uint32)idx);
}

/* Any mode other than 0 / 2 draws nothing (silent no-op). */
static void test_mode_unknown_draws_nothing(void)
{
    reset_mode_paint();
    setup_paint_char(0, 0x05, 0x03, 0, 0, 1, 0x00, 0);
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    fd2_paint_char_sprite_at_world_with_mode(0x00100000, 0x140, 0, 1, 0);
    fd2_paint_char_sprite_at_world_with_mode(0x00100000, 0x140, 0, 3, 0);
    fd2_paint_char_sprite_at_world_with_mode(0x00100000, 0x140, 0, 99, 0);

    ASSERT_EQ(g_blitpass_calls, 0);
    ASSERT_EQ(g_blitsolid_calls, 0);
}

/* Window cull: out-of-window on each of the four edges -> early return, no
 * blit. Margins: origin_x-1 .. origin_x+max_x and origin_y-1 .. origin_y+max_y+1. */
static void test_mode_window_cull(void)
{
    reset_mode_paint();
    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x10;
    data_fd2_battle_view_window_max_x = 0x08;
    data_fd2_battle_view_window_max_y = 0x08;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    setup_paint_char(0, 0x0e, 0x12, 0, 0, 0, 0x00, 0);   /* x < origin_x-1 (0x0F) */
    fd2_paint_char_sprite_at_world_with_mode(0x00100000, 0x140, 0, 0, 0);
    ASSERT_EQ(g_blitpass_calls, 0);

    setup_paint_char(0, 0x19, 0x12, 0, 0, 0, 0x00, 0);   /* x > origin_x+max_x (0x18) */
    fd2_paint_char_sprite_at_world_with_mode(0x00100000, 0x140, 0, 0, 0);
    ASSERT_EQ(g_blitpass_calls, 0);

    setup_paint_char(0, 0x12, 0x0e, 0, 0, 0, 0x00, 0);   /* y < origin_y-1 (0x0F) */
    fd2_paint_char_sprite_at_world_with_mode(0x00100000, 0x140, 0, 0, 0);
    ASSERT_EQ(g_blitpass_calls, 0);

    setup_paint_char(0, 0x12, 0x1a, 0, 0, 0, 0x00, 0);   /* y > origin_y+max_y+1 (0x19) */
    fd2_paint_char_sprite_at_world_with_mode(0x00100000, 0x140, 0, 0, 0);
    ASSERT_EQ(g_blitpass_calls, 0);

    /* lower-x edge and upper-y edge are inclusive -> paints */
    setup_paint_char(0, 0x0f, 0x19, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_with_mode(0x00100000, 0x140, 0, 0, 0);
    ASSERT_EQ(g_blitpass_calls, 1);
}

/* Frame selection: walk_phase 0 reads the ambient palette idx, walk_phase != 0
 * reads the alt palette idx; a palette value of 3 folds to 1. Recover the
 * frame component from the sprite_idx (facing 0, cache_idx 0 -> idx == frame). */
static void test_mode_frame_palette_selection(void)
{
    uint32 stride = 0x140;
    uint32 base = 0x00100000;

    reset_mode_paint();
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    /* walk_phase 0 -> ambient idx (set to 2) */
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 2;
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 1;
    setup_paint_char(0, 0x05, 0x03, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);
    ASSERT_EQ(g_blitpass_src[0] - (uint32)g_paint_atlas, 2u);   /* frame = ambient(2) */

    /* walk_phase != 0 -> alt idx (set to 1) */
    setup_paint_char(0, 0x05, 0x03, 0, 0, 1, 0x00, 0);
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);
    ASSERT_EQ(g_blitpass_src[1] - (uint32)g_paint_atlas, 1u);   /* frame = alt(1) */

    /* palette 3 folds to 1 (drive via walk -> alt = 3) */
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 3;
    setup_paint_char(0, 0x05, 0x03, 0, 0, 1, 0x00, 0);
    fd2_paint_char_sprite_at_world_with_mode(base, stride, 0, 0, 0);
    ASSERT_EQ(g_blitpass_src[2] - (uint32)g_paint_atlas, 1u);   /* 3 -> 1 */
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

    g_check_char_is_dead_return = 0;
    reset_paint_window();
    /* shadow overlay runs the real fd2_blit_animated_tile_at_pos for every
     * char footprint tile; install a transparent tile-map (renderable bit
     * clear) so those redraws read attr then return without a blit. This
     * isolates g_blitpass_calls to the per-char paint loop, whose dst/order
     * this test verifies; the shadow tile fan-out itself is covered by the
     * dedicated fd2_paint_chars_shadow_overlay tests below. */
    install_anim_tile_map(0);
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
    /* the shadow overlay also runs after the loop, but with the transparent
     * tile-map its per-char tile redraws produce no blit; the shadow tile
     * fan-out itself is verified in the dedicated shadow tests below. */
}

/* All party slots dead: every slot skipped, still exactly one shadow pass. */
static void test_overlay_all_dead(void)
{
    reset_overlay_record();
    data_fd2_battle_party_member_count = 4;
    g_check_char_is_dead_return = 1;

    fd2_composite_all_chars_overlay();

    /* shadow ran but every char is dead -> skipped, no per-char paint either */
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* Empty party (count == 0): loop body never runs; shadow pass still runs.
 * Guards the (int) signed compare so count 0 does not underflow. */
static void test_overlay_empty_party(void)
{
    reset_overlay_record();
    data_fd2_battle_party_member_count = 0;

    fd2_composite_all_chars_overlay();

    ASSERT_EQ(g_blitpass_calls, 0);
}

/* ----------------------------------------------------------------
 * fd2_paint_chars_shadow_overlay — per-char tile-trail redraw.
 *
 * Drives the real routine and asserts the exact (x, y) sequence the real
 * fd2_blit_animated_tile_at_pos paints, matching the 0x129EC disassembly:
 * base footprint at (x,y)+(x,y-1), then a per-facing walk-trail tile only
 * when sprite_state[2] (walk_phase) != 0. A renderable tile-map (all tiles
 * id 1, attr 0x80, no cursor overlay) makes every requested tile resolve
 * to a passthrough blit; (x, y) is recovered from the recorded dst offset.
 * ---------------------------------------------------------------- */
#define SHADOW_BUF 0xCAFE1234u

static void setup_shadow_char(int slot, uint8 px, uint8 py, uint8 facing,
                              uint8 walk_phase)
{
    runtime_char *c = &g_test_rc_array[slot];
    memset(c, 0, sizeof(*c));
    c->pos_x = px;
    c->pos_y = py;
    c->sprite_state[1] = facing;
    c->sprite_state[2] = walk_phase;
    /* default job/archetype/portrait 0 -> fd2_check_char_status_immunity == 0 */
}

static void reset_shadow_record(void)
{
    g_blitpass_calls = 0;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_large_game_state_buffer_ptr = SHADOW_BUF;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x40;
    data_fd2_battle_view_window_max_y = 0x40;
    install_anim_tile_map(1);   /* renderable: every requested tile blits */
}

/* assert recorded anim-tile blit #i resolved to world (ex, ey) */
static void assert_anim_tile(int i, int32 ex, int32 ey)
{
    int32 gx;
    int32 gy;

    recover_anim_tile(i, SHADOW_BUF, &gx, &gy);
    ASSERT_EQ(gx, ex);
    ASSERT_EQ(gy, ey);
}

/* walk_phase == 0: only the 2-tile base footprint, no trail. */
static void test_shadow_stationary_base_only(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 0, 0);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(g_blitpass_calls, 2);
    assert_anim_tile(0, 0x0a, 0x07);
    assert_anim_tile(1, 0x0a, 0x06);
}

/* facing 0 (down), walking: base 2 + single trail tile at (x, y+1). */
static void test_shadow_facing_down_trail(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 0, 1);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(g_blitpass_calls, 3);
    assert_anim_tile(0, 0x0a, 0x07);
    assert_anim_tile(1, 0x0a, 0x06);
    assert_anim_tile(2, 0x0a, 0x08);
}

/* facing 1 (left), walking: base 2 + (x-1,y) + (x-1,y-1). */
static void test_shadow_facing_left_trail(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 1, 1);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(g_blitpass_calls, 4);
    assert_anim_tile(0, 0x0a, 0x07);
    assert_anim_tile(1, 0x0a, 0x06);
    assert_anim_tile(2, 0x09, 0x07);
    assert_anim_tile(3, 0x09, 0x06);
}

/* facing 2 (up), walking: base 2 + single trail tile at (x, y-2). */
static void test_shadow_facing_up_trail(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 2, 1);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(g_blitpass_calls, 3);
    assert_anim_tile(0, 0x0a, 0x07);
    assert_anim_tile(1, 0x0a, 0x06);
    assert_anim_tile(2, 0x0a, 0x05);
}

/* facing 3 (right), walking: base 2 + (x+1,y) + (x+1,y-1). */
static void test_shadow_facing_right_trail(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 3, 1);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(g_blitpass_calls, 4);
    assert_anim_tile(0, 0x0a, 0x07);
    assert_anim_tile(1, 0x0a, 0x06);
    assert_anim_tile(2, 0x0b, 0x07);
    assert_anim_tile(3, 0x0b, 0x06);
}

/* immune char (job_id 0x13) is skipped entirely -> no anim-tile redraw. */
static void test_shadow_immune_skipped(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 0, 1);
    g_test_rc_array[0].job_id = 0x13;   /* immune per status check */

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(g_blitpass_calls, 0);
}

/* dead char is skipped entirely -> no anim-tile redraw. */
static void test_shadow_dead_skipped(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 0, 1);
    g_check_char_is_dead_return = 1;

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(g_blitpass_calls, 0);
}

/* multi-char: each alive non-immune slot contributes its own footprint+trail,
 * in party order; verifies the loop advances per slot. */
static void test_shadow_multi_char(void)
{
    reset_shadow_record();
    data_fd2_battle_party_member_count = 2;
    setup_shadow_char(0, 0x04, 0x05, 0, 0);   /* stationary -> 2 blits */
    setup_shadow_char(1, 0x08, 0x09, 2, 1);   /* up, walking -> 3 blits */

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(g_blitpass_calls, 5);
    assert_anim_tile(0, 0x04, 0x05);
    assert_anim_tile(1, 0x04, 0x04);
    assert_anim_tile(2, 0x08, 0x09);
    assert_anim_tile(3, 0x08, 0x08);
    assert_anim_tile(4, 0x08, 0x07);
}

/* ----------------------------------------------------------------
 * fd2_paint_threat_overlay_for_team — mark alive chars of the selected
 * team with a "+" AoE pattern on the tile-map threat overlay (+6 byte).
 *
 * Drives the real routine (and the real fd2_mark_aoe_plus_pattern_at /
 * fd2_set_tile_overlay_bit_80 callees). A char at (x,y) marks center
 * tile_map[(y*W+x)*4+6] |= 0x40 and the 4 plus-neighbors |= 0x80.
 * Asserts the asymmetric team filter and the dead-skip directly on the
 * resulting tile-map bytes.
 * ---------------------------------------------------------------- */
#define THREAT_W 16
#define THREAT_H 16
static uint8 g_threat_tile_map[THREAT_W * THREAT_H * 4];

static void reset_threat_map(void)
{
    memset(g_threat_tile_map, 0, sizeof(g_threat_tile_map));
    data_fd2_battle_tile_map_ptr = (uint32)g_threat_tile_map;
    data_fd2_battle_map_width_tiles = THREAT_W;
    data_fd2_battle_map_height_tiles = THREAT_H;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}

/* overlay byte at tile (x,y) */
static uint8 threat_byte(int x, int y)
{
    return g_threat_tile_map[(y * THREAT_W + x) * 4 + 6];
}

static void setup_threat_char(int slot, uint8 px, uint8 py, uint8 flags,
                              uint8 team)
{
    runtime_char *c = &g_test_rc_array[slot];
    memset(c, 0, sizeof(*c));
    c->pos_x = px;
    c->pos_y = py;
    c->flags = flags;
    c->team = team;
}

/* assert the full "+" pattern landed for a char at interior (x,y) */
static void assert_plus_marked(int x, int y)
{
    ASSERT_EQ((uint32)threat_byte(x, y),     0x40u);   /* center */
    ASSERT_EQ((uint32)threat_byte(x - 1, y), 0x80u);   /* left   */
    ASSERT_EQ((uint32)threat_byte(x + 1, y), 0x80u);   /* right  */
    ASSERT_EQ((uint32)threat_byte(x, y - 1), 0x80u);   /* upper  */
    ASSERT_EQ((uint32)threat_byte(x, y + 1), 0x80u);   /* lower  */
}

/* assert no overlay bits set anywhere around (x,y) */
static void assert_unmarked(int x, int y)
{
    ASSERT_EQ((uint32)threat_byte(x, y),     0u);
    ASSERT_EQ((uint32)threat_byte(x - 1, y), 0u);
    ASSERT_EQ((uint32)threat_byte(x + 1, y), 0u);
    ASSERT_EQ((uint32)threat_byte(x, y - 1), 0u);
    ASSERT_EQ((uint32)threat_byte(x, y + 1), 0u);
}

/* ctx == 0 marks team != 0 (ally overlay); leaves team == 0 untouched. */
static void test_threat_ctx0_marks_nonzero_team(void)
{
    reset_threat_map();
    data_fd2_battle_party_member_count = 2;
    setup_threat_char(0, 5, 5, 0x00, 2);   /* team 2 -> marked */
    setup_threat_char(1, 9, 9, 0x00, 0);   /* team 0 -> skipped */

    fd2_paint_threat_overlay_for_team(0);

    assert_plus_marked(5, 5);
    assert_unmarked(9, 9);
}

/* ctx != 0 marks team == 0 (enemy overlay); leaves team != 0 untouched. */
static void test_threat_ctx1_marks_zero_team(void)
{
    reset_threat_map();
    data_fd2_battle_party_member_count = 2;
    setup_threat_char(0, 5, 5, 0x00, 0);   /* team 0 -> marked */
    setup_threat_char(1, 9, 9, 0x00, 3);   /* team 3 -> skipped */

    fd2_paint_threat_overlay_for_team(1);

    assert_plus_marked(5, 5);
    assert_unmarked(9, 9);
}

/* dead chars (flags bit0) are skipped even when the team filter matches. */
static void test_threat_dead_skipped(void)
{
    reset_threat_map();
    data_fd2_battle_party_member_count = 1;
    setup_threat_char(0, 5, 5, 0x01, 2);   /* dead, team 2, ctx 0 would match */

    fd2_paint_threat_overlay_for_team(0);

    assert_unmarked(5, 5);
}

/* empty party -> no overlay writes at all. */
static void test_threat_empty_party(void)
{
    reset_threat_map();
    data_fd2_battle_party_member_count = 0;
    setup_threat_char(0, 5, 5, 0x00, 2);

    fd2_paint_threat_overlay_for_team(0);

    assert_unmarked(5, 5);
}

/* ----------------------------------------------------------------
 * fd2_composite_chars_with_spell_effect_overlay @ 0x1CB94 — per-char
 * spell-cast layer: hit-list chars get the spell-effect sprite (via the
 * fd2_blit_sprite_with_decoded_pixels stub, recorded in g_blitdec_*),
 * every other alive in-window char keeps its normal portrait (via the
 * fd2_tile_blit_24x24_passthrough stub, recorded in g_blitpass_*).
 *
 * dst_buf is a sentinel: none of the three callee stubs dereference it,
 * so the address arithmetic can be checked directly from the recorded
 * dst. char_screen_addr = dst_buf + 0x75D8 + (py-oy)*0x2AC0 + (px-ox)*0x18.
 *
 * Effect-sprite source: the sheet at data_fd2_resource_portrait_sheet_ptr
 * holds a dword table at +6 (index*4) of absolute offsets; an identity
 * table (table[i]==i) makes fx_sprite_addr == sheet + fx_sprite_idx, so
 * the recorded g_blitdec_sprite reveals which fx index was loaded.
 *
 * Portrait source: portrait_sprite_cache is install_paint_atlas()'s
 * identity table, so g_blitpass_src - g_paint_atlas == frame_idx, where
 * frame_idx = cache_idx*0xC + (ambient_palette==3 ? 2 : ambient_palette).
 * ---------------------------------------------------------------- */
#define SPELL_BUF 0xB0000000u

/* Sheet fixture for data_fd2_resource_portrait_sheet_ptr: identity dword
 * table at +6 so fx_sprite_addr == sheet + fx_sprite_idx. 0x80 entries
 * cover the 0x4A/0x4B fx indices the caller uses. */
static uint8 g_spell_sheet[6 + 0x80 * 4];

static void install_spell_sheet(void)
{
    int i;
    int32 *table;

    table = (int32 *)(g_spell_sheet + 6);
    for (i = 0; i < 0x80; i++) {
        table[i] = i;
    }
    data_fd2_resource_portrait_sheet_ptr = (uint32)g_spell_sheet;
}

/* Open the window wide, pin the sentinel buffer + both atlases, clear all
 * recorders. Party count is left for the test to set. */
static void reset_spell_overlay(void)
{
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x40;
    data_fd2_battle_view_window_max_y = 0x40;
    data_fd2_large_game_state_buffer_ptr = SPELL_BUF;   /* unused by this fn */
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;
    install_paint_atlas();      /* portrait_sprite_cache identity table */
    install_spell_sheet();      /* effect-sprite sheet identity table */
    g_blitpass_calls = 0;
    g_blitdim_calls = 0;
    g_blitdec_calls = 0;
    g_tile_map_calls = 0;
    g_composite_call_count = 0;
}

/* expected char_screen_addr for (px,py) at given origin and dst_buf */
static uint32 expect_screen_addr(uint32 dst_buf, int32 px, int32 py,
                                 int32 ox, int32 oy)
{
    return dst_buf + 0x75d8u + (uint32)(py - oy) * 0x2ac0u +
           (uint32)(px - ox) * 0x18u;
}

/* The unconditional tile-map composite runs once into dst_buf + 0x8088 with
 * the documented constants (456, 13, 8, origin_x, origin_y), regardless of
 * party contents. */
static void test_spell_tilemap_composite(void)
{
    reset_spell_overlay();
    data_fd2_battle_party_member_count = 0;
    data_fd2_battle_view_window_origin_x = 0x11;
    data_fd2_battle_view_window_origin_y = 0x22;

    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);

    ASSERT_EQ(g_tile_map_calls, 1);
    ASSERT_EQ(g_tile_map_last_dst, SPELL_BUF + 0x8088u);
    ASSERT_EQ(g_tile_map_last_stride, 0x1c8u);
    ASSERT_EQ(g_tile_map_last_w, 0xdu);
    ASSERT_EQ(g_tile_map_last_h, 8u);
    ASSERT_EQ(g_tile_map_last_ox, 0x11u);
    ASSERT_EQ(g_tile_map_last_oy, 0x22u);
    /* empty party -> no per-char blits of either kind */
    ASSERT_EQ(g_blitpass_calls, 0);
    ASSERT_EQ(g_blitdec_calls, 0);
}

/* A char NOT in the target list draws its normal portrait through the
 * passthrough blitter: verify dst arithmetic, stride, frame_idx lookup,
 * and that the effect blitter did NOT run. */
static void test_spell_miss_draws_portrait(void)
{
    int32 frame_idx;

    reset_spell_overlay();
    data_fd2_battle_party_member_count = 1;
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 1;
    /* slot 0: pos (5,3), cache_idx 2, alive (flags 0) */
    setup_paint_char(0, 0x05, 0x03, 2, 0, 0, 0x00, 0);

    /* empty target list (n_targets 0) -> char 0 is a miss */
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);

    ASSERT_EQ(g_blitdec_calls, 0);          /* no effect sprite */
    ASSERT_EQ(g_blitpass_calls, 1);         /* one portrait */
    ASSERT_EQ(g_blitdim_calls, 0);          /* passthrough, not dimmed */
    ASSERT_EQ(g_blitpass_stride[0], 0x1c8u);
    ASSERT_EQ(g_blitpass_dst[0], expect_screen_addr(SPELL_BUF, 5, 3, 0, 0));
    /* frame_idx = cache_idx(2)*0xC + ambient_palette(1) = 25; identity cache
     * table -> src - atlas == frame_idx */
    frame_idx = 2 * 0xc + 1;
    ASSERT_EQ(g_blitpass_src[0] - (uint32)g_paint_atlas, (uint32)frame_idx);
}

/* A char IN the target list draws the spell effect sprite through the
 * decoded-pixels blitter: verify dst, stride, the resolved fx sprite addr,
 * and that the portrait blitter did NOT run. */
static void test_spell_hit_draws_effect(void)
{
    uint8 targets[1];

    reset_spell_overlay();
    data_fd2_battle_party_member_count = 1;
    setup_paint_char(0, 0x07, 0x05, 3, 0, 0, 0x00, 0);
    targets[0] = 0;     /* char_idx 0 is targeted */

    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 1,
                                                  (uint32)targets, 0x4b);

    ASSERT_EQ(g_blitpass_calls, 0);         /* no portrait */
    ASSERT_EQ(g_blitdec_calls, 1);          /* one effect sprite */
    ASSERT_EQ(g_blitdec_stride, 0x1c8u);
    ASSERT_EQ(g_blitdec_dst, expect_screen_addr(SPELL_BUF, 7, 5, 0, 0));
    /* identity sheet table -> fx_sprite_addr == sheet + fx_sprite_idx(0x4B) */
    ASSERT_EQ(g_blitdec_sprite, (uint32)g_spell_sheet + 0x4bu);
}

/* Mixed party: only the listed indices get the effect; the rest get
 * portraits. Targets = {1, 2} out of 4 alive chars. */
static void test_spell_mixed_hit_and_miss(void)
{
    uint8 targets[2];

    reset_spell_overlay();
    data_fd2_battle_party_member_count = 4;
    /* distinct pos_x so each recorded dst is unique; all alive */
    setup_paint_char(0, 0x04, 0x02, 0, 0, 0, 0x00, 0);
    setup_paint_char(1, 0x05, 0x02, 0, 0, 0, 0x00, 0);
    setup_paint_char(2, 0x06, 0x02, 0, 0, 0, 0x00, 0);
    setup_paint_char(3, 0x07, 0x02, 0, 0, 0, 0x00, 0);
    targets[0] = 1;
    targets[1] = 2;

    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 2,
                                                  (uint32)targets, 0x4a);

    /* chars 1 and 2 -> effect; chars 0 and 3 -> portrait */
    ASSERT_EQ(g_blitdec_calls, 2);
    ASSERT_EQ(g_blitpass_calls, 2);
    /* the last effect blit recorded is char 2 at pos_x 6 (decoded stub keeps
     * only the most recent dst) */
    ASSERT_EQ(g_blitdec_dst, expect_screen_addr(SPELL_BUF, 6, 2, 0, 0));
    /* portrait blits recorded in loop order: char 0 (px 4) then char 3 (px 7) */
    ASSERT_EQ(g_blitpass_dst[0], expect_screen_addr(SPELL_BUF, 4, 2, 0, 0));
    ASSERT_EQ(g_blitpass_dst[1], expect_screen_addr(SPELL_BUF, 7, 2, 0, 0));
}

/* Dead chars (flags bit0) are skipped entirely: no blit of either kind even
 * if the dead char's index is in the target list. */
static void test_spell_dead_skipped(void)
{
    uint8 targets[1];

    reset_spell_overlay();
    data_fd2_battle_party_member_count = 1;
    setup_paint_char(0, 0x05, 0x03, 0, 0, 0, 0x01, 0);   /* flags bit0 = dead */
    targets[0] = 0;

    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 1,
                                                  (uint32)targets, 0x4a);

    ASSERT_EQ(g_blitdec_calls, 0);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* Out-of-window chars are culled (no blit) on each of the four edges; the
 * margins match the per-char paint: x in [ox-1, ox+max_x], y in
 * [oy-1, oy+max_y+1]. */
static void test_spell_window_cull(void)
{
    reset_spell_overlay();
    data_fd2_battle_party_member_count = 1;
    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x10;
    data_fd2_battle_view_window_max_x = 0x08;
    data_fd2_battle_view_window_max_y = 0x08;

    /* x below ox-1 (0x0F) */
    setup_paint_char(0, 0x0e, 0x12, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(g_blitpass_calls, 0);

    /* x above ox+max_x (0x18) */
    setup_paint_char(0, 0x19, 0x12, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(g_blitpass_calls, 0);

    /* y below oy-1 (0x0F) */
    setup_paint_char(0, 0x12, 0x0e, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(g_blitpass_calls, 0);

    /* y above oy+max_y+1 (0x19) */
    setup_paint_char(0, 0x12, 0x1a, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(g_blitpass_calls, 0);

    /* inclusive lower-x / upper-y edge still paints */
    setup_paint_char(0, 0x0f, 0x19, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(g_blitpass_calls, 1);
}

/* Palette-3 special case: when ambient_palette == 3 the portrait frame_idx
 * uses +2 (not +3). cache_idx 1 -> frame = 1*0xC + 2 = 14. */
static void test_spell_palette3_frame(void)
{
    int32 frame_idx;

    reset_spell_overlay();
    data_fd2_battle_party_member_count = 1;
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 3;
    setup_paint_char(0, 0x05, 0x03, 1, 0, 0, 0x00, 0);

    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);

    ASSERT_EQ(g_blitpass_calls, 1);
    frame_idx = 1 * 0xc + 2;
    ASSERT_EQ(g_blitpass_src[0] - (uint32)g_paint_atlas, (uint32)frame_idx);
}

/* The target scan has no early break: a char_idx appearing multiple times in
 * the list still resolves to a single hit (one effect blit, no portrait). */
static void test_spell_duplicate_target_single_hit(void)
{
    uint8 targets[3];

    reset_spell_overlay();
    data_fd2_battle_party_member_count = 1;
    setup_paint_char(0, 0x06, 0x04, 0, 0, 0, 0x00, 0);
    targets[0] = 0;
    targets[1] = 0;
    targets[2] = 0;

    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 3,
                                                  (uint32)targets, 0x4a);

    ASSERT_EQ(g_blitdec_calls, 1);
    ASSERT_EQ(g_blitpass_calls, 0);
}

/* ====================================================================
 * fd2_render_combat_combatant_panels @ 0x1E611
 *
 * Drives the real function. Its callees here are a mix of real emitted
 * routines and recording stubs:
 *   fd2_composite_battle_tile_map           -> recording stub (g_tile_map_*)
 *   fd2_composite_all_chars_overlay         -> real (party=0 -> no-op),
 *                                              counted via g_composite_call_count
 *   fd2_alloc_and_blit_indexed_sprite_chunk -> real; its one save-screen-block
 *                                              call per invocation makes
 *                                              g_saveblk_calls an exact panel
 *                                              counter, and g_saveblk_src pins
 *                                              the panel dst_off arithmetic
 *   fd2_render_combat_hp_bar_segments       -> recording stub (g_hpseg_*)
 *   fd2_render_combatant_hp_bar_proportional-> recording stub (g_hpbar_prop_*)
 *   fd2_blit_rectangle                      -> real; memmoves the workspace to
 *                                              0xA0504 (VGA RAM, writable under
 *                                              DOS/4GW) so ws must be backed.
 * ==================================================================== */

extern int    g_saveblk_calls;
extern uint32 g_saveblk_src, g_saveblk_dst, g_saveblk_w, g_saveblk_h,
              g_saveblk_stride;
extern int    g_hpbar_prop_calls;
extern uint32 g_hpbar_prop_d[4];
extern uint32 g_hpbar_prop_s[4];
extern uint32 g_hpbar_prop_ci[4];
extern uint32 g_hpbar_prop_st[4];
extern int    g_hpseg_calls;
extern uint32 g_hpseg_dst, g_hpseg_stride, g_hpseg_count;

/* Panel sheet fixture for data_fd2_resource_portrait_sheet_ptr. The real
 * fd2_alloc_and_blit_indexed_sprite_chunk resolves sprite 0x30 as
 *   hdr = sheet + *(int32 *)(sheet + 6 + 0x30*4)
 * then reads (width, height) = int16 words at hdr+0 / hdr+2. We give entry
 * 0x30 an explicit offset to an 8x8 header so malloc(8*8+8) is small and the
 * decode/save calls run deterministically. */
static uint8 g_panel_sheet[6 + 0x40 * 4 + 64];

static void install_panel_sheet(void)
{
    int32 *table;
    uint32 hdr_off;

    memset(g_panel_sheet, 0, sizeof(g_panel_sheet));
    table = (int32 *)(g_panel_sheet + 6);
    hdr_off = 6 + 0x40 * 4;                  /* header sits past the table */
    table[0x30] = (int32)hdr_off;
    *(int16 *)(g_panel_sheet + hdr_off)     = 8;   /* width  */
    *(int16 *)(g_panel_sheet + hdr_off + 2) = 8;   /* height */
    data_fd2_resource_portrait_sheet_ptr = (uint32)g_panel_sheet;
}

/* ws back-buffer at large_game_state_buffer_ptr + 0x8088; window wide so the
 * panel arithmetic is unclamped. All combatant-panel recorders cleared. */
static void reset_panel_record(void)
{
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ws_buffer - 0x8088u;
    data_fd2_battle_view_window_origin_x = 0x11;
    data_fd2_battle_view_window_origin_y = 0x22;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_party_member_count = 0;   /* real chars overlay -> no-op */
    install_panel_sheet();

    g_tile_map_calls = 0;
    g_composite_call_count = 0;
    g_saveblk_calls = 0;
    g_blitdec_calls = 0;
    g_hpbar_prop_calls = 0;
    g_hpseg_calls = 0;
}

/* Attacker-only path (xy[2] == -1): backdrop rebuild, exactly one panel
 * sprite chunk, the HP-segment bar, and one proportional HP bar — no defender
 * panel. Pins every computed address. */
static void test_panels_attacker_only(void)
{
    int xy[4];
    uint32 ws;
    uint32 expect_seg;

    reset_panel_record();
    xy[0] = 0x20;          /* attacker_x */
    xy[1] = 0x10;          /* attacker_y */
    xy[2] = -1;            /* no defender */
    xy[3] = 0x55;

    fd2_render_combat_combatant_panels((uint32)xy, 7, 3);  /* def=7, atk=3 */

    ws = (uint32)g_ws_buffer;   /* == large_game_state_buffer_ptr + 0x8088 */

    /* 1. backdrop: tile map into ws with the documented constants */
    ASSERT_EQ(g_tile_map_calls, 1);
    ASSERT_EQ(g_tile_map_last_dst, ws);
    ASSERT_EQ(g_tile_map_last_stride, 0x1c8u);
    ASSERT_EQ(g_tile_map_last_w, 0xdu);
    ASSERT_EQ(g_tile_map_last_h, 8u);
    ASSERT_EQ(g_tile_map_last_ox, 0x11u);
    ASSERT_EQ(g_tile_map_last_oy, 0x22u);
    ASSERT_EQ(g_composite_call_count, 1);   /* all-chars overlay ran */

    /* 2. exactly one panel sprite chunk; its dst_off = (ay-4)*456 + (ax-4) */
    ASSERT_EQ(g_saveblk_calls, 1);
    ASSERT_EQ(g_saveblk_dst, ws);
    ASSERT_EQ(g_saveblk_src, (uint32)((0x10 - 4) * 0x1c8 + (0x20 - 4)));
    ASSERT_EQ(g_saveblk_stride, 0x1c8u);

    /* 3. HP-segment bar dst = ws + 3 + (ay+2)*456 + ax, width 0x37 */
    expect_seg = ws + 3u + (uint32)((0x10 + 2) * 0x1c8) + 0x20u;
    ASSERT_EQ(g_hpseg_calls, 1);
    ASSERT_EQ(g_hpseg_dst, expect_seg);
    ASSERT_EQ(g_hpseg_stride, 0x1c8u);
    ASSERT_EQ(g_hpseg_count, 0x37u);

    /* 4. one proportional HP bar: (ws-0x724, 456, attacker_idx, &xy[0]) */
    ASSERT_EQ(g_hpbar_prop_calls, 1);
    ASSERT_EQ(g_hpbar_prop_d[0], ws - 0x724u);   /* 0x7964 - 0x8088 = -0x724 */
    ASSERT_EQ(g_hpbar_prop_s[0], 0x1c8u);
    ASSERT_EQ(g_hpbar_prop_ci[0], 3u);
    ASSERT_EQ(g_hpbar_prop_st[0], (uint32)xy);
}

/* Defender present (xy[2] != -1): a second panel sprite chunk and a second
 * proportional HP bar, the defender one keyed to defender_idx and &xy[2]. */
static void test_panels_with_defender(void)
{
    int xy[4];
    uint32 ws;

    reset_panel_record();
    xy[0] = 0x18;          /* attacker_x */
    xy[1] = 0x0c;          /* attacker_y */
    xy[2] = 0x30;          /* defender_x */
    xy[3] = 0x14;          /* defender_y */

    fd2_render_combat_combatant_panels((uint32)xy, 7, 3);  /* def=7, atk=3 */

    ws = (uint32)g_ws_buffer;

    /* two panel sprite chunks; last save-block src = (dy-4)*456 + (dx-4) */
    ASSERT_EQ(g_saveblk_calls, 2);
    ASSERT_EQ(g_saveblk_src, (uint32)((0x14 - 4) * 0x1c8 + (0x30 - 4)));

    /* two proportional HP bars, in attacker-then-defender order */
    ASSERT_EQ(g_hpbar_prop_calls, 2);
    ASSERT_EQ(g_hpbar_prop_ci[0], 3u);                 /* attacker_idx */
    ASSERT_EQ(g_hpbar_prop_st[0], (uint32)xy);         /* &xy[0]       */
    ASSERT_EQ(g_hpbar_prop_ci[1], 7u);                 /* defender_idx */
    ASSERT_EQ(g_hpbar_prop_st[1], (uint32)xy + 8u);    /* &xy[2]       */
    ASSERT_EQ(g_hpbar_prop_d[1], ws - 0x724u);
}

/* Independent witness for the HP-segment address formula with a different
 * (ax, ay) so the (ay+2)*456 + ax + 3 arithmetic is not coincidental. */
static void test_panels_hp_seg_addr_arithmetic(void)
{
    int xy[4];
    uint32 ws;
    uint32 expect_seg;

    reset_panel_record();
    xy[0] = 0x29;          /* attacker_x */
    xy[1] = 0x1f;          /* attacker_y */
    xy[2] = -1;
    xy[3] = 0;

    fd2_render_combat_combatant_panels((uint32)xy, 0, 0);

    ws = (uint32)g_ws_buffer;
    expect_seg = ws + 3u + (uint32)((0x1f + 2) * 0x1c8) + 0x29u;
    ASSERT_EQ(g_hpseg_calls, 1);
    ASSERT_EQ(g_hpseg_dst, expect_seg);
    ASSERT_EQ(g_hpseg_count, 0x37u);
    /* attacker-only: no defender panel, single proportional bar */
    ASSERT_EQ(g_saveblk_calls, 1);
    ASSERT_EQ(g_hpbar_prop_calls, 1);
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
    RUN_TEST(test_mode_passthrough_dst_and_src);
    RUN_TEST(test_mode_facing_x_offsets);
    RUN_TEST(test_mode_walkphase0_no_jitter);
    RUN_TEST(test_mode_solid_color_dispatch);
    RUN_TEST(test_mode_unknown_draws_nothing);
    RUN_TEST(test_mode_window_cull);
    RUN_TEST(test_mode_frame_palette_selection);
    RUN_TEST(test_overlay_all_alive);
    RUN_TEST(test_overlay_all_dead);
    RUN_TEST(test_overlay_empty_party);
    RUN_TEST(test_shadow_stationary_base_only);
    RUN_TEST(test_shadow_facing_down_trail);
    RUN_TEST(test_shadow_facing_left_trail);
    RUN_TEST(test_shadow_facing_up_trail);
    RUN_TEST(test_shadow_facing_right_trail);
    RUN_TEST(test_shadow_immune_skipped);
    RUN_TEST(test_shadow_dead_skipped);
    RUN_TEST(test_shadow_multi_char);
    RUN_TEST(test_threat_ctx0_marks_nonzero_team);
    RUN_TEST(test_threat_ctx1_marks_zero_team);
    RUN_TEST(test_threat_dead_skipped);
    RUN_TEST(test_threat_empty_party);
    RUN_TEST(test_spell_tilemap_composite);
    RUN_TEST(test_spell_miss_draws_portrait);
    RUN_TEST(test_spell_hit_draws_effect);
    RUN_TEST(test_spell_mixed_hit_and_miss);
    RUN_TEST(test_spell_dead_skipped);
    RUN_TEST(test_spell_window_cull);
    RUN_TEST(test_spell_palette3_frame);
    RUN_TEST(test_spell_duplicate_target_single_hit);
    RUN_TEST(test_panels_attacker_only);
    RUN_TEST(test_panels_with_defender);
    RUN_TEST(test_panels_hp_seg_addr_arithmetic);
    printf("\n");
}
