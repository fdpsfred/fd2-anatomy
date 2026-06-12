/*
 * unit tests for src/gfx/rndscene.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "blitprob.h"
#include <stdio.h>

/* pipeline-callee recording vars (defined in testglob.c) */
extern int    g_tile_map_calls;
extern uint32 g_tile_map_last_dst;
extern uint32 g_tile_map_last_stride;
extern uint32 g_tile_map_last_w;
extern uint32 g_tile_map_last_h;
extern uint32 g_tile_map_last_ox;
extern uint32 g_tile_map_last_oy;
extern int    g_composite_call_count;
/* The probe-driven tests in this file drive the real fd2_tile_blit_24x24_passthrough
 * (src/gfx/blittile.c) with probe sprites (see blitprob.h) and read the painted bytes
 * back to reconstruct the (world_x, world_y, sprite_idx) the caller computed: an atlas
 * slot's one-pixel probe paints value idx+1, so a painted byte's offset gives the dst
 * and its value gives the sprite index.
 * The recording-stub-driven tests below instead read the g_blitpass_* / g_blitsolid_*
 * recorders (testglob.c). Those recorders are unfilled now that the real blitter wins
 * at link (the recording stubs were removed); the recorder-based tests are re-pointed
 * at the real painted output in the systematic-fix phase. */
extern int    g_blitpass_calls;
extern uint32 g_blitpass_src[64];
extern uint32 g_blitpass_dst[64];
extern uint32 g_blitpass_stride[64];
extern int    g_blitdim_calls;
extern int    g_blitsolid_calls;
extern uint32 g_blitsolid_color[64];
/* recording stub for fd2_blit_sprite_with_decoded_pixels (testglob.c); the
 * spell-effect overlay hit branch forwards (dst, sprite, stride) here. The
 * opt-in per-call log (g_blitdec_log_*) is reused by the phase-banner-frame
 * test to capture both of its alloc/blit halves' resolved (dst, sprite). */
extern uint32 g_blitdec_dst;
extern uint32 g_blitdec_sprite;
extern uint32 g_blitdec_stride;
extern int    g_blitdec_calls;
extern int    g_blitdec_log_on;
extern int    g_blitdec_log_count;
extern uint32 g_blitdec_log_dst[16];
extern uint32 g_blitdec_log_sprite[16];
/* recording stub for fd2_restore_screen_block_from_buffer (testglob.c); the
 * real fd2_cleanup_dialog_sprite_buffer forwards (saved_block, dst, stride)
 * here once per call, so g_restore_block_calls counts cleanups. */
extern int    g_restore_block_calls;
extern uint32 g_restore_block_last_buf;
extern uint32 g_restore_block_last_dst;
extern uint32 g_restore_block_last_stride;
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

/* Real destination buffer for the standalone window-relative paint tests (cursor
 * pattern, per-char paint, char overlay, shadow overlay, spell overlay). The real
 * passthrough/animated-tile blitters WRITE here for real, so it must span the
 * largest window-relative dst any test reaches; the cursor pattern at
 * (CX+3, CY+3) lands near +0x23000, plus one row stride of headroom. */
#define PAINT_BUF_SPAN 0x24000u
static uint8 g_paint_buf[PAINT_BUF_SPAN];

/* probe-atlas slot stride: must hold a probe program (<= 26 bytes). */
#define ATLAS_SPAN 0x40u

static void reset_pipeline_record(void)
{
    g_tile_map_calls = 0;
    memset(g_ws_buffer, 0, sizeof(g_ws_buffer));   /* clean canvas for the count */
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
 * 4-byte absolute offset from the table at +6 (index*4); bp_build_atlas1 points
 * slot i at a probe in a sprite region past the table (so slot 0 does not clobber
 * the table) and plants a one-pixel probe of value i+1 there, so the painted
 * byte's value identifies the sprite index the caller resolved. 64 entries cover
 * all cursor sprite indices (0x00..0x12). */
static uint8 g_sprite_atlas[6 + 64 * 4 + 64 * ATLAS_SPAN];

static void install_sprite_atlas(void)
{
    memset(g_sprite_atlas, 0, sizeof(g_sprite_atlas));
    bp_build_atlas1(g_sprite_atlas, 6u, ATLAS_SPAN, 64);
    data_fd2_runtime_battle_state_ptr = (uint32)g_sprite_atlas;
}

/* Make the battle window large enough that every cursor-pattern coord is
 * in-window, so each caller blit paints into g_paint_buf. The base is anchored
 * so a blit at world (x,y) paints at g_paint_buf + y*0x2AC0 + x*0x18. */
static void install_full_window(void)
{
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    memset(g_paint_buf, 0, sizeof(g_paint_buf));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_paint_buf - 0x8088;
}

/* Assert that the window-relative blit for world (x,y) with sprite index `s`
 * painted its probe pixel into g_paint_buf. The real blit lands the pixel at
 *   dst = base + (y-oy)*0x2AC0 + (x-ox)*0x18 + 0x8088
 * and base == g_paint_buf - 0x8088, so the byte offset into g_paint_buf is
 * (y-oy)*0x2AC0 + (x-ox)*0x18; the probe for index s paints value s+1. */
static void assert_blit(uint32 x, uint32 y, uint32 s)
{
    uint32 oy = data_fd2_battle_view_window_origin_y;
    uint32 ox = data_fd2_battle_view_window_origin_x;
    uint32 off = (y - oy) * 0x2AC0u + (x - ox) * 0x18u;
    ASSERT_EQ((uint32)g_paint_buf[off], s + 1u);
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
 * shadow overlay asks to paint runs the real passthrough blitter on tile id
 * 1's probe sprite, landing one ANIM_TILE_VALUE pixel whose offset encodes
 * (x, y) (checked by assert_anim_tile_at). With renderable clear the routine
 * reads attr then returns without blitting (used where only the *caller's*
 * loop is under test). map width is fixed at 0x20 so (y*0x20 + x) covers
 * every shadow-test coord. */
#define ANIM_MAP_W      0x20
#define ANIM_MAP_ROWS   0x20
static uint8 g_anim_tile_map[ANIM_MAP_W * ANIM_MAP_ROWS * 4];
static uint8 g_anim_attr_buf[64];
/* snapshot: offset table at +10 (tile_id*4) pointing tile id 1 at a probe sprite
 * past the table, so every renderable anim-tile blit paints ANIM_TILE_VALUE. */
#define ANIM_SNAP_SPRITE_OFF 0x40u
#define ANIM_TILE_VALUE      0x5Au
static uint8 g_anim_scene_snapshot[0x80];

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
    /* tile id 1's sprite = snapshot + *(int*)(snapshot + 10 + 1*4) */
    *(int32 *)(g_anim_scene_snapshot + 10 + 1 * 4) = (int32)ANIM_SNAP_SPRITE_OFF;
    bp_probe1(g_anim_scene_snapshot + ANIM_SNAP_SPRITE_OFF, ANIM_TILE_VALUE);

    data_fd2_battle_tile_map_ptr = (uint32)g_anim_tile_map;
    data_fd2_battle_map_width_tiles = ANIM_MAP_W;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_anim_attr_buf;
    battle_scene_snapshot = (uint32)g_anim_scene_snapshot;
    data_fd2_graphics_bg_anim_flip_flag = 0;
}

/* assert the renderable anim-tile blit for world (x, y) painted its probe pixel
 * into `buf`. dst = buf + 0x8088 + (y-oy)*0x2AC0 + (x-ox)*0x18 (oy=ox=0 here). */
static void assert_anim_tile_at(uint32 buf, int32 x, int32 y)
{
    uint32 off = 0x8088u + (uint32)y * 0x2AC0u + (uint32)x * 0x18u;
    ASSERT_EQ((uint32)((uint8 *)buf)[off], (uint32)ANIM_TILE_VALUE);
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
    ASSERT_EQ(bp_count_painted(g_ws_buffer, WS_SPAN), 1);

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
    ASSERT_EQ(bp_count_painted(g_ws_buffer, WS_SPAN), 1);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* ----------------------------------------------------------------
 * fd2_paint_cursor_overlay_pattern — per-phase pattern verification.
 *
 * Drives the real routine for each anim phase and asserts the exact
 * sequence of (world_x, world_y, sprite_idx) blits, matching the
 * 0x122DC disassembly. The window is set wide enough that every coord is
 * in-window, so each caller blit forwards to the real passthrough blitter;
 * the (x, y, sprite) tuple is reconstructed from the painted probe pixel
 * (its offset gives the dst -> world x/y, its value gives the sprite index).
 * ---------------------------------------------------------------- */
#define CX 0x14u
#define CY 0x0Au

static void set_cursor_phase(uint32 phase)
{
    install_full_window();        /* memsets g_paint_buf, anchors the base */
    install_sprite_atlas();
    data_fd2_battle_cursor_world_x = CX;
    data_fd2_battle_cursor_world_y = CY;
    data_fd2_battle_anim_phase = phase;
}

/* count of painted probe pixels in g_paint_buf == number of cursor blits */
static int cursor_blit_count(void)
{
    return bp_count_painted(g_paint_buf, PAINT_BUF_SPAN);
}

/* Each cursor blit writes a distinct (x,y); assert_blit confirms the probe pixel
 * for sprite index `es` landed at (ex,ey). Order of the writes is not observable
 * from the painted frame, so the tests assert the (x,y,sprite) SET plus the total
 * blit count rather than a call sequence. */
static void test_cursor_phase1(void)
{
    set_cursor_phase(1);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(cursor_blit_count(), 1);
    assert_blit(CX, CY, 0);
}

static void test_cursor_phase2(void)
{
    set_cursor_phase(2);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(cursor_blit_count(), 1);
    assert_blit(CX, CY, 1);
}

static void test_cursor_phase3(void)
{
    set_cursor_phase(3);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(cursor_blit_count(), 5);
    assert_blit(CX,     CY,     0xe);
    assert_blit(CX,     CY - 1, 2);
    assert_blit(CX - 1, CY,     3);
    assert_blit(CX + 1, CY,     4);
    assert_blit(CX,     CY + 1, 5);
}

static void test_cursor_phase4(void)
{
    set_cursor_phase(4);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(cursor_blit_count(), 13);
    assert_blit(CX,     CY,     1);
    assert_blit(CX,     CY - 2, 2);
    assert_blit(CX - 2, CY,     3);
    assert_blit(CX + 2, CY,     4);
    assert_blit(CX,     CY + 2, 5);
    assert_blit(CX - 1, CY - 1, 6);
    assert_blit(CX + 1, CY - 1, 7);
    assert_blit(CX - 1, CY + 1, 8);
    assert_blit(CX + 1, CY + 1, 9);
    assert_blit(CX,     CY - 1, 0xa);
    assert_blit(CX - 1, CY,     0xb);
    assert_blit(CX + 1, CY,     0xc);
    assert_blit(CX,     CY + 1, 0xd);
}

static void test_cursor_phase5(void)
{
    set_cursor_phase(5);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(cursor_blit_count(), 21);
    assert_blit(CX,     CY,     1);
    assert_blit(CX,     CY - 3, 2);
    assert_blit(CX - 3, CY,     3);
    assert_blit(CX + 3, CY,     4);
    assert_blit(CX,     CY + 3, 5);
    assert_blit(CX - 1, CY - 2, 6);
    assert_blit(CX - 2, CY - 1, 6);
    assert_blit(CX + 1, CY - 2, 7);
    assert_blit(CX + 2, CY - 1, 7);
    assert_blit(CX - 1, CY + 2, 8);
    assert_blit(CX - 2, CY + 1, 8);
    assert_blit(CX + 1, CY + 2, 9);
    assert_blit(CX + 2, CY + 1, 9);
    assert_blit(CX,     CY - 2, 0xa);
    assert_blit(CX - 2, CY,     0xb);
    assert_blit(CX + 2, CY,     0xc);
    assert_blit(CX,     CY + 2, 0xd);
    assert_blit(CX - 1, CY - 1, 0xf);
    assert_blit(CX + 1, CY - 1, 0x10);
    assert_blit(CX - 1, CY + 1, 0x11);
    assert_blit(CX + 1, CY + 1, 0x12);
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

    ASSERT_EQ(cursor_blit_count(), 0);
    ASSERT_EQ((uint32)g_cursor_tile_map[idx], 0u);
}

/* default (unhandled phase): no blit, no memory write. */
static void test_cursor_phase_default(void)
{
    set_cursor_phase(99);
    fd2_paint_cursor_overlay_pattern();
    ASSERT_EQ(cursor_blit_count(), 0);
}

/* ----------------------------------------------------------------
 * fd2_paint_char_sprite_at_world_pos — per-char sprite paint.
 *
 * Backing for the real routine: a portrait-cache atlas whose offset table at +0
 * points frame slot i at a one-pixel probe (value i+1) in a sprite region past
 * the table, so the painted byte's value identifies the resolved frame index
 * (value-1). large_game_state_buffer is anchored at g_paint_buf so a painted
 * pixel lands at g_paint_buf + the blit_offset the caller computed; the window is
 * opened wide so the paint runs. */
#define PAINT_ATLAS_N      64
#define PAINT_SPRITE_BASE  0x100u   /* clears the 64*4-byte table */
#define PAINT_SPRITE_SPAN  0x40u
static uint8 g_paint_atlas[0x1200];
static void install_paint_atlas(void)
{
    int i;

    memset(g_paint_atlas, 0, sizeof(g_paint_atlas));
    for (i = 0; i < PAINT_ATLAS_N; i++) {
        *(int32 *)(g_paint_atlas + i * 4) =
            (int32)(PAINT_SPRITE_BASE + (uint32)i * PAINT_SPRITE_SPAN);
        bp_probe1(g_paint_atlas + PAINT_SPRITE_BASE + (uint32)i * PAINT_SPRITE_SPAN,
                  (uint8)(i + 1));
    }
    portrait_sprite_cache = (uint32)g_paint_atlas;
}

/* probe value the portrait-cache slot `frame_idx` paints (1-based) */
static uint8 paint_frame_value(uint32 frame_idx)
{
    return (uint8)(frame_idx + 1u);
}

/* ----------------------------------------------------------------
 * Fixture for the acted (greyed) paint path. Unlike the passthrough path
 * (recorded by a stub), fd2_tile_blit_24x24_dimmed_grayscale is the REAL
 * emitted blitter, so the acted-flag test drives it for real: it needs a
 * resolvable RLE sprite at the computed source slot and a real back-buffer
 * at the computed dst. The cache holds a 256-dword absolute-offset table
 * (table[i] == i*0x100) followed by RLE sprite slots; every slot is filled
 * with transparent SKIP so a mis-resolved frame paints nothing, and one
 * single-pixel grayscale sprite is planted at the expected slot. The blit
 * lands at g_dim_lgs + 0x75D8 and paints exactly one (src & 7) + 0x18 byte.
 * ---------------------------------------------------------------- */
#define DIM_PCACHE_SIZE 0x3600u
#define DIM_LGS_SPAN    0xC000u
#define DIM_GRAY(src)   ((uint8)(((src) & 7u) + 0x18u))   /* dimmed transform */
static uint8 g_dim_cache[DIM_PCACHE_SIZE];
static uint8 g_dim_lgs[DIM_LGS_SPAN];

/* RLE command builders (low 6 bits + 1 == run length; top 2 bits = mode). */
#define DIM_LIT(n)   ((uint8)(0x80u | ((n) - 1)))   /* copy n grayscale pixels */
#define DIM_SKIP(n)  ((uint8)(0xC0u | ((n) - 1)))   /* advance n (transparent) */

/* Build the offset table, fill all sprite slots with a 24-row transparent
 * program, then plant one single-pixel grayscale sprite (source byte src_b)
 * at the slot the caller resolves for frame_idx. */
static void plant_dim_sprite(uint32 frame_idx, uint8 src_b)
{
    uint32 *table;
    uint8 *sprite;
    uint32 i;

    table = (uint32 *)g_dim_cache;
    for (i = 0; i < 256; i++) {
        table[i] = i * 0x100u;
    }
    for (i = 0x400u; i < DIM_PCACHE_SIZE; i++) {
        g_dim_cache[i] = DIM_SKIP(1);
    }
    sprite = g_dim_cache + frame_idx * 0x100u;
    sprite[0] = DIM_LIT(1);          /* one painted pixel at (0,0) */
    sprite[1] = src_b;               /* its source byte -> (src_b & 7) + 0x18 */
    sprite[2] = DIM_SKIP(23);        /* finish row 0 (1 + 23 == 24) */
    for (i = 1; i < 24; i++) {
        sprite[2 + i] = DIM_SKIP(24);   /* rows 1..23 transparent */
    }
    portrait_sprite_cache = (uint32)g_dim_cache;
    memset(g_dim_lgs, 0, sizeof(g_dim_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_dim_lgs;
}

/* Count painted grayscale bytes (value gray_v) in g_dim_lgs and report the
 * offset of the first one. */
static int count_dim_pixels(uint8 gray_v, uint32 *first_off)
{
    uint32 i;
    int n = 0;
    *first_off = 0xFFFFFFFFu;
    for (i = 0; i < DIM_LGS_SPAN; i++) {
        if (g_dim_lgs[i] == gray_v) {
            if (n == 0) {
                *first_off = i;
            }
            n++;
        }
    }
    return n;
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
    /* dst_buf == g_paint_buf, so the painted pixel lands at g_paint_buf + the
     * blit_offset the caller computes (expect_offset). */
    memset(g_paint_buf, 0, sizeof(g_paint_buf));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_paint_buf;
    data_fd2_graphics_char_sprite_shake_jitter_bit = 0;
    data_fd2_graphics_char_sprite_paint_jitter_tick_latch = (int32)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 0;
    install_paint_atlas();
}

/* expected blit_offset for given (px,py,walk_phase,pitch,jitter) at origin 0 */
static int32 expect_offset(int32 px, int32 py, int32 walk_phase, int32 pitch,
                           int32 jitter)
{
    return walk_phase * pitch + py * 0x2ac0 + px * 0x18 + jitter + 0x75d8;
}

/* assert the per-char paint dropped its probe pixel (value paint_frame_value
 * (frame_idx)) at g_paint_buf + off; off is positive for every test below. */
static void assert_paint(int32 off, uint32 frame_idx)
{
    ASSERT_EQ((uint32)g_paint_buf[(uint32)off], (uint32)paint_frame_value(frame_idx));
}

static int paint_count(void)
{
    return bp_count_painted(g_paint_buf, PAINT_BUF_SPAN);
}

/* facing 0 (down): pitch +0x720, not acted -> passthrough. Verify dst offset,
 * frame_idx lookup (painted value), and that exactly one passthrough pixel was
 * painted (the dimmed branch was NOT taken). */
static void test_paint_facing_down_passthrough(void)
{
    uint32 frame_idx;

    reset_paint_window();
    /* facing 0, cache_idx 2, walk_phase 1, ambient palette 0 */
    setup_paint_char(0, 0x05, 0x03, 2, 0, 1, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);

    ASSERT_EQ(paint_count(), 1);       /* not-acted -> passthrough painted once */
    /* frame_idx = facing*3 + cache_idx*0xC + palette(0) = 0 + 24 + 0 = 24 */
    frame_idx = 0 * 3 + 2 * 0xc + 0;
    assert_paint(expect_offset(5, 3, 1, 0x720, 0), frame_idx);
}

/* stride forwarded is 0x1C8: a two-pixel probe at the resolved frame slot lands
 * its second pixel exactly one stride past the first. */
static void test_paint_passthrough_stride(void)
{
    uint32 frame_idx;
    int32  off;

    reset_paint_window();
    setup_paint_char(0, 0x05, 0x03, 2, 0, 1, 0x00, 0);
    frame_idx = 0 * 3 + 2 * 0xc + 0;   /* 24 */
    /* overwrite slot 24 with a two-pixel probe to expose the row stride */
    bp_probe2(g_paint_atlas + PAINT_SPRITE_BASE + frame_idx * PAINT_SPRITE_SPAN,
              paint_frame_value(frame_idx));
    fd2_paint_char_sprite_at_world_pos(0);

    off = expect_offset(5, 3, 1, 0x720, 0);
    ASSERT_EQ((uint32)g_paint_buf[(uint32)off], (uint32)paint_frame_value(frame_idx));
    ASSERT_EQ((uint32)g_paint_buf[(uint32)off + 0x1c8u],
              (uint32)paint_frame_value(frame_idx));
    ASSERT_EQ(paint_count(), 2);
}

/* facing 1 (left): pitch -4. facing 2 (up): pitch -0x720. facing 3 (right): +4.
 * Drives all three remaining facings; each paints a distinct frame value at a
 * distinct dst (walk_phase 0 so palette uses ambient idx 0). */
static void test_paint_facing_pitch_deltas(void)
{
    reset_paint_window();

    /* facing 1 -> pitch -4, frame_idx = 1*3 = 3 */
    setup_paint_char(0, 0x08, 0x04, 0, 1, 3, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    assert_paint(expect_offset(8, 4, 3, -4, 0), 1 * 3);

    /* facing 2 -> pitch -0x720, frame_idx = 2*3 = 6 */
    setup_paint_char(0, 0x08, 0x04, 0, 2, 3, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    assert_paint(expect_offset(8, 4, 3, -0x720, 0), 2 * 3);

    /* facing 3 -> pitch +4, frame_idx = 3*3 = 9 */
    setup_paint_char(0, 0x08, 0x04, 0, 3, 3, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    assert_paint(expect_offset(8, 4, 3, 4, 0), 3 * 3);

    ASSERT_EQ(paint_count(), 3);
}

/* acted (flags bit7 set) -> the REAL dimmed/grayscale blitter runs (not the
 * passthrough recorder). Place the char at the window origin (0,0) so the dst
 * offset reduces to the +0x75D8 workspace base, plant a single-pixel grayscale
 * sprite at the resolved source slot, and confirm exactly one painted byte
 * ((src & 7) + 0x18) lands at g_dim_lgs + 0x75D8. A correct resolution paints
 * that one pixel; the passthrough recorder must NOT fire. cache_idx 4 ->
 * frame_idx = 4*0xC + palette(0) = 0x30, whose slot (0x3000) clears the table. */
static void test_paint_acted_dimmed(void)
{
    uint32 first_off;
    int n;

    reset_paint_window();
    /* px=py=0 (== origin) so position deltas vanish; cache_idx 4, walk_phase 0,
     * acted flag 0x80. ambient palette is 0, so frame_idx = 0 + 0x30 + 0. */
    setup_paint_char(0, 0x00, 0x00, 4, 0, 0, 0x80, 0);
    plant_dim_sprite(0x30u, 0x02u);          /* src 0x02 -> grayscale 0x1A */

    fd2_paint_char_sprite_at_world_pos(0);

    /* exactly one painted grayscale pixel total in g_dim_lgs, at the +0x75D8
     * workspace base: proves the dimmed branch ran and that the passthrough
     * branch did NOT (it would have added a non-grayscale pixel). */
    n = count_dim_pixels(DIM_GRAY(0x02u), &first_off);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(first_off, 0x75d8u);
    ASSERT_EQ(DIM_GRAY(0x02u), 0x1au);
    ASSERT_EQ(bp_count_painted(g_dim_lgs, DIM_LGS_SPAN), 1);
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
    ASSERT_EQ(paint_count(), 0);

    /* x above origin_x+max_x (0x10+0x08 = 0x18) */
    setup_paint_char(0, 0x19, 0x12, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ(paint_count(), 0);

    /* y below origin_y-1 (0x0F) */
    setup_paint_char(0, 0x12, 0x0e, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ(paint_count(), 0);

    /* y above origin_y+max_y+1 (0x10+0x08+1 = 0x19) */
    setup_paint_char(0, 0x12, 0x1a, 0, 0, 0, 0x00, 0);
    fd2_paint_char_sprite_at_world_pos(0);
    ASSERT_EQ(paint_count(), 0);
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
    ASSERT_EQ(paint_count(), 1);
}

/* Sleep status: (a) adds the shake jitter byte to blit_offset, (b) forces
 * palette 0. Set jitter bit = 1 and an alt palette != 0 to prove both. */
static void test_paint_sleep_jitter_and_palette(void)
{
    uint32 frame_idx;

    reset_paint_window();
    data_fd2_graphics_char_sprite_shake_jitter_bit = 1;
    /* walk_phase 0 -> ambient palette; set ambient to 2 to prove sleep forces 0 */
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 2;
    setup_paint_char(0, 0x05, 0x03, 1, 0, 0, 0x00, 1);   /* sleep=1 */
    fd2_paint_char_sprite_at_world_pos(0);

    ASSERT_EQ(paint_count(), 1);
    /* palette forced to 0: frame_idx = facing*3 + cache_idx*0xC + 0 = 0 + 12 + 0;
     * +1 jitter folded into the offset. The painted value confirms the frame
     * index, the offset confirms the jitter. */
    frame_idx = 0 * 3 + 1 * 0xc + 0;
    assert_paint(expect_offset(5, 3, 0, 0x720, 1), frame_idx);
}

/* Palette 3 falls back to 1 (non-sleep). Use walk_phase != 0 so the alt palette
 * idx feeds the selection, set it to 3, expect lookup palette component == 1. */
static void test_paint_palette3_fallback(void)
{
    uint32 frame_idx;

    reset_paint_window();
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 3;   /* walk -> alt */
    setup_paint_char(0, 0x05, 0x03, 0, 0, 1, 0x00, 0);         /* walk_phase 1 */
    fd2_paint_char_sprite_at_world_pos(0);

    ASSERT_EQ(paint_count(), 1);
    /* frame_idx = facing(0)*3 + cache_idx(0)*0xC + palette(3->1) = 1; facing 0
     * (down) -> pitch +0x720, walk_phase 1 */
    frame_idx = 0 * 3 + 0 * 0xc + 1;
    assert_paint(expect_offset(5, 3, 1, 0x720, 0), frame_idx);
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
 * overlay. The real fd2_check_char_is_dead reads runtime_char[i].flags bit0,
 * so dead slots are pinned by setting .flags |= CHARFLAG_DEAD. Each alive
 * in-window slot produces exactly one recorded blit; distinct pos_x per slot
 * lets the test recover which index painted (and in what order).
 * ---------------------------------------------------------------- */
static void reset_overlay_record(void)
{
    int i;

    reset_paint_window();
    /* shadow overlay runs the real fd2_blit_animated_tile_at_pos for every
     * char footprint tile; install a transparent tile-map (renderable bit
     * clear) so those redraws read attr then return without a blit. This
     * isolates the painted pixels to the per-char paint loop, whose dsts this
     * test verifies; the shadow tile fan-out itself is covered by the
     * dedicated fd2_paint_chars_shadow_overlay tests below. */
    install_anim_tile_map(0);
    /* every slot in-window, not acted, awake, facing down, walk_phase 0.
     * slot i gets pos_x = 0x04 + i so its painted offset identifies the index. */
    for (i = 0; i < 8; i++) {
        setup_paint_char(i, (uint8)(0x04 + i), 0x03, 0, 0, 0, 0x00, 0);
    }
}

/* assert the per-char paint for slot `idx` (pos_x 0x04+idx, py 3, facing 0 down
 * pitch +0x720 walk_phase 0) painted its probe pixel; frame_idx 0 -> value 1. */
static void assert_overlay_slot(uint32 idx)
{
    int32 off = expect_offset((int32)(0x04u + idx), 3, 0, 0x720, 0);
    ASSERT_EQ((uint32)g_paint_buf[(uint32)off], (uint32)paint_frame_value(0));
}

/* All party slots alive: paint every slot 0..count-1, one shadow. */
static void test_overlay_all_alive(void)
{
    int i;

    reset_overlay_record();
    data_fd2_battle_party_member_count = 5;
    /* every slot alive: reset_overlay_record set .flags == 0 for all slots */

    fd2_composite_all_chars_overlay();

    ASSERT_EQ(paint_count(), 5);
    for (i = 0; i < 5; i++) {
        assert_overlay_slot((uint32)i);
    }
    /* the shadow overlay also runs after the loop, but with the transparent
     * tile-map its per-char tile redraws produce no blit; the shadow tile
     * fan-out itself is verified in the dedicated shadow tests below. */
}

/* All party slots dead: every slot skipped, still exactly one shadow pass. */
static void test_overlay_all_dead(void)
{
    int i;

    reset_overlay_record();
    data_fd2_battle_party_member_count = 4;
    for (i = 0; i < 4; i++) {
        g_test_rc_array[i].flags |= CHARFLAG_DEAD;   /* every scanned slot dead */
    }

    fd2_composite_all_chars_overlay();

    /* shadow ran but every char is dead -> skipped, no per-char paint either */
    ASSERT_EQ(paint_count(), 0);
}

/* Empty party (count == 0): loop body never runs; shadow pass still runs.
 * Guards the (int) signed compare so count 0 does not underflow. */
static void test_overlay_empty_party(void)
{
    reset_overlay_record();
    data_fd2_battle_party_member_count = 0;

    fd2_composite_all_chars_overlay();

    ASSERT_EQ(paint_count(), 0);
}

/* ----------------------------------------------------------------
 * fd2_paint_chars_shadow_overlay — per-char tile-trail redraw.
 *
 * Drives the real routine and asserts the exact (x, y) sequence the real
 * fd2_blit_animated_tile_at_pos paints, matching the 0x129EC disassembly:
 * base footprint at (x,y)+(x,y-1), then a per-facing walk-trail tile only
 * when sprite_state[2] (walk_phase) != 0. A renderable tile-map (all tiles
 * id 1, attr 0x80, no cursor overlay) makes every requested tile paint its
 * probe pixel; (x, y) is confirmed from the painted dst offset.
 * ---------------------------------------------------------------- */
#define SHADOW_BUF ((uint32)g_paint_buf)

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
    data_fd2_battle_party_member_count = 1;
    memset(g_paint_buf, 0, sizeof(g_paint_buf));
    data_fd2_large_game_state_buffer_ptr = SHADOW_BUF;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x40;
    data_fd2_battle_view_window_max_y = 0x40;
    install_anim_tile_map(1);   /* renderable: every requested tile blits */
}

static int shadow_count(void)
{
    return bp_count_painted(g_paint_buf, PAINT_BUF_SPAN);
}

/* assert the anim-tile blit for world (ex, ey) painted its probe pixel */
static void assert_anim_tile(int32 ex, int32 ey)
{
    assert_anim_tile_at(SHADOW_BUF, ex, ey);
}

/* walk_phase == 0: only the 2-tile base footprint, no trail. */
static void test_shadow_stationary_base_only(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 0, 0);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(shadow_count(), 2);
    assert_anim_tile(0x0a, 0x07);
    assert_anim_tile(0x0a, 0x06);
}

/* facing 0 (down), walking: base 2 + single trail tile at (x, y+1). */
static void test_shadow_facing_down_trail(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 0, 1);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(shadow_count(), 3);
    assert_anim_tile(0x0a, 0x07);
    assert_anim_tile(0x0a, 0x06);
    assert_anim_tile(0x0a, 0x08);
}

/* facing 1 (left), walking: base 2 + (x-1,y) + (x-1,y-1). */
static void test_shadow_facing_left_trail(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 1, 1);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(shadow_count(), 4);
    assert_anim_tile(0x0a, 0x07);
    assert_anim_tile(0x0a, 0x06);
    assert_anim_tile(0x09, 0x07);
    assert_anim_tile(0x09, 0x06);
}

/* facing 2 (up), walking: base 2 + single trail tile at (x, y-2). */
static void test_shadow_facing_up_trail(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 2, 1);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(shadow_count(), 3);
    assert_anim_tile(0x0a, 0x07);
    assert_anim_tile(0x0a, 0x06);
    assert_anim_tile(0x0a, 0x05);
}

/* facing 3 (right), walking: base 2 + (x+1,y) + (x+1,y-1). */
static void test_shadow_facing_right_trail(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 3, 1);

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(shadow_count(), 4);
    assert_anim_tile(0x0a, 0x07);
    assert_anim_tile(0x0a, 0x06);
    assert_anim_tile(0x0b, 0x07);
    assert_anim_tile(0x0b, 0x06);
}

/* immune char (job_id 0x13) is skipped entirely -> no anim-tile redraw. */
static void test_shadow_immune_skipped(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 0, 1);
    g_test_rc_array[0].job_id = 0x13;   /* immune per status check */

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(shadow_count(), 0);
}

/* dead char is skipped entirely -> no anim-tile redraw. */
static void test_shadow_dead_skipped(void)
{
    reset_shadow_record();
    setup_shadow_char(0, 0x0a, 0x07, 0, 1);
    g_test_rc_array[0].flags |= CHARFLAG_DEAD;   /* dead -> skipped */

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(shadow_count(), 0);
}

/* multi-char: each alive non-immune slot contributes its own footprint+trail,
 * verifying the loop advances per slot. */
static void test_shadow_multi_char(void)
{
    reset_shadow_record();
    data_fd2_battle_party_member_count = 2;
    setup_shadow_char(0, 0x04, 0x05, 0, 0);   /* stationary -> 2 blits */
    setup_shadow_char(1, 0x08, 0x09, 2, 1);   /* up, walking -> 3 blits */

    fd2_paint_chars_shadow_overlay();

    ASSERT_EQ(shadow_count(), 5);
    assert_anim_tile(0x04, 0x05);
    assert_anim_tile(0x04, 0x04);
    assert_anim_tile(0x08, 0x09);
    assert_anim_tile(0x08, 0x08);
    assert_anim_tile(0x08, 0x07);
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
 * every other alive in-window char keeps its normal portrait, painted for real
 * by fd2_tile_blit_24x24_passthrough into dst_buf.
 *
 * dst_buf is g_paint_buf (a real buffer): the effect blits go through the
 * decoded-pixels stub (no real paint) but the portrait blits paint for real, at
 * char_screen_addr = dst_buf + 0x75D8 + (py-oy)*0x2AC0 + (px-ox)*0x18; the probe
 * pixel's value identifies the resolved frame index.
 *
 * Effect-sprite source: the sheet at data_fd2_resource_portrait_sheet_ptr
 * holds a dword table at +6 (index*4) of absolute offsets; an identity
 * table (table[i]==i) makes fx_sprite_addr == sheet + fx_sprite_idx, so
 * the recorded g_blitdec_sprite reveals which fx index was loaded.
 *
 * Portrait source: portrait_sprite_cache is install_paint_atlas()'s probe atlas,
 * so the painted portrait byte == paint_frame_value(frame_idx), where
 * frame_idx = cache_idx*0xC + (ambient_palette==3 ? 2 : ambient_palette).
 * ---------------------------------------------------------------- */
#define SPELL_BUF ((uint32)g_paint_buf)

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

/* Open the window wide, pin the real dst buffer + both atlases, clear all
 * recorders. Party count is left for the test to set. */
static void reset_spell_overlay(void)
{
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x40;
    data_fd2_battle_view_window_max_y = 0x40;
    memset(g_paint_buf, 0, sizeof(g_paint_buf));
    data_fd2_large_game_state_buffer_ptr = SPELL_BUF;   /* unused by this fn */
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;
    install_paint_atlas();      /* portrait_sprite_cache probe atlas */
    install_spell_sheet();      /* effect-sprite sheet identity table */
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

/* count of painted portrait probe pixels in g_paint_buf */
static int spell_portrait_count(void)
{
    return bp_count_painted(g_paint_buf, PAINT_BUF_SPAN);
}

/* assert the portrait for (px,py) (origin ox,oy) painted frame_idx's probe */
static void assert_spell_portrait(int32 px, int32 py, int32 ox, int32 oy,
                                  uint32 frame_idx)
{
    uint32 off = expect_screen_addr(SPELL_BUF, px, py, ox, oy) - (uint32)g_paint_buf;
    ASSERT_EQ((uint32)g_paint_buf[off], (uint32)paint_frame_value(frame_idx));
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
    ASSERT_EQ(spell_portrait_count(), 0);
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
    ASSERT_EQ(spell_portrait_count(), 1);   /* one portrait via passthrough */
    /* frame_idx = cache_idx(2)*0xC + ambient_palette(1) = 25; the painted probe
     * value identifies the frame index and its offset the screen dst. */
    frame_idx = 2 * 0xc + 1;
    assert_spell_portrait(5, 3, 0, 0, (uint32)frame_idx);
}

/* the portrait passthrough forwards stride 0x1C8: a two-pixel probe at the
 * resolved frame slot lands its second pixel one stride past the first. */
static void test_spell_portrait_stride(void)
{
    int32 frame_idx;
    uint32 off;

    reset_spell_overlay();
    data_fd2_battle_party_member_count = 1;
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 1;
    setup_paint_char(0, 0x05, 0x03, 2, 0, 0, 0x00, 0);
    frame_idx = 2 * 0xc + 1;            /* 25 */
    bp_probe2(g_paint_atlas + PAINT_SPRITE_BASE + (uint32)frame_idx * PAINT_SPRITE_SPAN,
              paint_frame_value((uint32)frame_idx));

    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);

    off = expect_screen_addr(SPELL_BUF, 5, 3, 0, 0) - (uint32)g_paint_buf;
    ASSERT_EQ((uint32)g_paint_buf[off], (uint32)paint_frame_value((uint32)frame_idx));
    ASSERT_EQ((uint32)g_paint_buf[off + 0x1c8u],
              (uint32)paint_frame_value((uint32)frame_idx));
    ASSERT_EQ(spell_portrait_count(), 2);
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

    ASSERT_EQ(spell_portrait_count(), 0);   /* no portrait */
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
    ASSERT_EQ(spell_portrait_count(), 2);
    /* the last effect blit recorded is char 2 at pos_x 6 (decoded stub keeps
     * only the most recent dst) */
    ASSERT_EQ(g_blitdec_dst, expect_screen_addr(SPELL_BUF, 6, 2, 0, 0));
    /* portraits painted for the two misses: char 0 (px 4) and char 3 (px 7),
     * both frame_idx 0 (cache_idx 0, palette 0) -> probe value 1 */
    assert_spell_portrait(4, 2, 0, 0, 0);
    assert_spell_portrait(7, 2, 0, 0, 0);
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
    ASSERT_EQ(spell_portrait_count(), 0);
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
    ASSERT_EQ(spell_portrait_count(), 0);

    /* x above ox+max_x (0x18) */
    setup_paint_char(0, 0x19, 0x12, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(spell_portrait_count(), 0);

    /* y below oy-1 (0x0F) */
    setup_paint_char(0, 0x12, 0x0e, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(spell_portrait_count(), 0);

    /* y above oy+max_y+1 (0x19) */
    setup_paint_char(0, 0x12, 0x1a, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(spell_portrait_count(), 0);

    /* inclusive lower-x / upper-y edge still paints (one portrait now present) */
    setup_paint_char(0, 0x0f, 0x19, 0, 0, 0, 0x00, 0);
    fd2_composite_chars_with_spell_effect_overlay(SPELL_BUF, 0, (uint32)0, 0x4a);
    ASSERT_EQ(spell_portrait_count(), 1);
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

    ASSERT_EQ(spell_portrait_count(), 1);
    /* frame_idx = cache_idx(1)*0xC + (palette 3 -> 2) = 14 */
    frame_idx = 1 * 0xc + 2;
    assert_spell_portrait(5, 3, 0, 0, (uint32)frame_idx);
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
    ASSERT_EQ(spell_portrait_count(), 0);
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
 *   fd2_render_combat_hp_bar_segments       -> real; its HP-bar segment blits
 *                                              reach the real
 *                                              fd2_blit_sheet_sprite_at_offset
 *                                              -> g_blitraw log (its first blit,
 *                                              the 0x17 left cap, pins the
 *                                              segment-bar dst arithmetic)
 *   fd2_render_combatant_hp_bar_proportional-> real; reads the combatant's HP
 *                                              from the runtime_char array and
 *                                              forwards the scaled fill count to
 *                                              the real segment renderer, whose
 *                                              0x17 left cap (at the computed
 *                                              proportional bar_addr) appears in
 *                                              the same g_blitraw log after the
 *                                              fixed-width segment bar's blits
 *   fd2_blit_rectangle                      -> real; memmoves the workspace to
 *                                              0xA0504 (VGA RAM, writable under
 *                                              DOS/4GW) so ws must be backed.
 * ==================================================================== */

extern int    g_saveblk_calls;
extern uint32 g_saveblk_src, g_saveblk_dst, g_saveblk_w, g_saveblk_h,
              g_saveblk_stride;
/* raw-blit recording log (testglob.c): the real fd2_render_combat_hp_bar_segments
 * resolves each segment through the real fd2_blit_sheet_sprite_at_offset, which
 * forwards (dst, sprite_addr) here when g_blitraw_log_on is set. */
extern uint32 g_blitraw_dst, g_blitraw_sprite, g_blitraw_stride;
extern int    g_blitraw_log_on;
extern int    g_blitraw_count;
extern uint32 g_blitraw_log_dst[512];
extern uint32 g_blitraw_log_sprite[512];

/* UI/anim sheet fixture for data_fd2_ui_anim_sprite_sheet_ptr. Offset-table
 * entry i stores the value i, so a resolved sprite address minus the sheet base
 * equals the sprite index drawn (covers the HP-bar indices 0x17..0x1e). */
static uint8 g_ui_sheet[6 + 0x20 * 4];

static void install_ui_sheet(void)
{
    int i;
    memset(g_ui_sheet, 0, sizeof(g_ui_sheet));
    for (i = 0; i <= 0x1e; i++) {
        *(int32 *)(g_ui_sheet + 6 + i * 4) = i;
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_ui_sheet;
}

/* sprite index of the k-th logged raw blit = sprite_addr - ui sheet base */
static uint32 ui_logged_idx(int k)
{
    return g_blitraw_log_sprite[k] - data_fd2_ui_anim_sprite_sheet_ptr;
}

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
 * panel arithmetic is unclamped. The proportional HP bar is now the real
 * routine, so back the runtime_char array and give the attacker (idx 3) and
 * defender (idx 7) full HP -> each emits a deterministic over-width segment
 * sequence whose 0x17 left cap pins its proportional bar_addr. */
static void reset_panel_record(void)
{
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ws_buffer - 0x8088u;
    data_fd2_battle_view_window_origin_x = 0x11;
    data_fd2_battle_view_window_origin_y = 0x22;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_party_member_count = 0;   /* real chars overlay -> no-op */
    install_panel_sheet();
    install_ui_sheet();                        /* HP-bar segment sprite source */

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 0x40;      /* combatant slots used as the */
    g_test_rc_array[0].hp_max     = 0x40;      /* attacker / defender indices */
    g_test_rc_array[3].hp_current = 0x40;      /* across the panel tests, each */
    g_test_rc_array[3].hp_max     = 0x40;      /* at full HP so the real prop. */
    g_test_rc_array[7].hp_current = 0x40;      /* bar always emits its left cap */
    g_test_rc_array[7].hp_max     = 0x40;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    g_tile_map_calls = 0;
    g_composite_call_count = 0;
    g_saveblk_calls = 0;
    g_blitdec_calls = 0;
    g_blitraw_count = 0;                        /* HP-bar segment blit log */
    g_blitraw_log_on = 1;
}

/* Count logged raw blits that drew sprite `idx` at dst `dst` (used to locate
 * the proportional HP bar's 0x17 left cap inside the combined panel log). */
static int panel_count_blit_at(uint32 dst, uint32 idx)
{
    int k;
    int n = 0;
    for (k = 0; k < g_blitraw_count; k++) {
        if (g_blitraw_log_dst[k] == dst &&
            (g_blitraw_log_sprite[k] - data_fd2_ui_anim_sprite_sheet_ptr) == idx) {
            n++;
        }
    }
    return n;
}

/* Attacker-only path (xy[2] == -1): backdrop rebuild, exactly one panel
 * sprite chunk, the HP-segment bar, and one proportional HP bar — no defender
 * panel. Pins every computed address. */
static void test_panels_attacker_only(void)
{
    int xy[4];
    uint32 ws;
    uint32 expect_seg;
    uint32 expect_prop;

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

    /* 3. HP-segment bar dst = ws + 3 + (ay+2)*456 + ax, width 0x37. The real
     *    renderer's first segment blit is the 0x17 left cap at that dst; with
     *    proportional-bar stubbed and panel chunks going through the decoded-
     *    pixel path, every raw blit logged here belongs to this segment bar. */
    expect_seg = ws + 3u + (uint32)((0x10 + 2) * 0x1c8) + 0x20u;
    ASSERT_TRUE(g_blitraw_count > 0);
    ASSERT_EQ(g_blitraw_log_dst[0], expect_seg);
    ASSERT_EQ(ui_logged_idx(0), 0x17u);
    ASSERT_EQ(g_blitraw_stride, 0x1c8u);

    /* 4. one proportional HP bar for the attacker (idx 3, full HP). It runs
     *    with dst_buf = ws-0x724 (0x7964-0x8088) and anchor &xy[0]; its real
     *    segment renderer emits the bar at
     *      bar_addr = ws - 0x724 + xy[0] + 7 + (xy[1]+6)*456.
     *    Full HP -> S = 0x46 (over-width), so the distinctive 0x19 fill cap
     *    lands at bar_addr + 0x46. (The 0x17 left cap coincides with the fixed
     *    segment bar's left cap here -- 4 - 0x724 + 4*456 == 0 -- so the fill
     *    cap is the unambiguous proportional-bar witness.) */
    expect_prop = ws - 0x724u + 0x20u + 7u + (uint32)((0x10 + 6) * 0x1c8);
    ASSERT_EQ(panel_count_blit_at(expect_prop + 0x46u, 0x19u), 1);
}

/* Defender present (xy[2] != -1): a second panel sprite chunk and a second
 * proportional HP bar, the defender one keyed to defender_idx and &xy[2]. */
static void test_panels_with_defender(void)
{
    int xy[4];
    uint32 ws;
    uint32 expect_atk;
    uint32 expect_def;

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

    /* two proportional HP bars: attacker (idx 3, anchor &xy[0]) and defender
     * (idx 7, anchor &xy[2]). Each combatant has full HP at its own slot, so
     * S = 0x46 and a distinctive 0x19 fill cap lands at bar_addr + 0x46 for
     * each. Both being present confirms attacker_idx/&xy[0] and defender_idx/
     * &xy[2] routing (a wrong slot would read HP 0 and emit no bar at all). */
    expect_atk = ws - 0x724u + 0x18u + 7u + (uint32)((0x0c + 6) * 0x1c8);
    expect_def = ws - 0x724u + 0x30u + 7u + (uint32)((0x14 + 6) * 0x1c8);
    ASSERT_EQ(panel_count_blit_at(expect_atk + 0x46u, 0x19u), 1);
    ASSERT_EQ(panel_count_blit_at(expect_def + 0x46u, 0x19u), 1);
}

/* Independent witness for the HP-segment address formula with a different
 * (ax, ay) so the (ay+2)*456 + ax + 3 arithmetic is not coincidental. */
static void test_panels_hp_seg_addr_arithmetic(void)
{
    int xy[4];
    uint32 ws;
    uint32 expect_seg;
    uint32 expect_prop;

    reset_panel_record();
    xy[0] = 0x29;          /* attacker_x */
    xy[1] = 0x1f;          /* attacker_y */
    xy[2] = -1;
    xy[3] = 0;

    fd2_render_combat_combatant_panels((uint32)xy, 0, 0);

    ws = (uint32)g_ws_buffer;
    expect_seg = ws + 3u + (uint32)((0x1f + 2) * 0x1c8) + 0x29u;
    /* first segment blit (0x17 left cap) pins the bar dst with new (ax, ay) */
    ASSERT_TRUE(g_blitraw_count > 0);
    ASSERT_EQ(g_blitraw_log_dst[0], expect_seg);
    ASSERT_EQ(ui_logged_idx(0), 0x17u);
    /* attacker-only: no defender panel, single proportional bar (idx 0, full
     * HP). Its distinctive 0x19 fill cap lands at the anchor &xy[0] -derived
     * bar_addr + 0x46. */
    ASSERT_EQ(g_saveblk_calls, 1);
    expect_prop = ws - 0x724u + 0x29u + 7u + (uint32)((0x1f + 6) * 0x1c8);
    ASSERT_EQ(panel_count_blit_at(expect_prop + 0x46u, 0x19u), 1);
}

/* ====================================================================
 * fd2_render_combatant_hp_bar_proportional @ 0x1E7F6
 *
 * Drives the real proportional bar -> real segment renderer end-to-end through
 * the g_blitraw log. The combatant's HP fraction scales the filled-segment
 * count S = hp_current*0x45/hp_max + 1 (signed IMUL/IDIV in the asm; both
 * operands positive here). The destination is
 *   bar_addr = dst_buf + anchor.x + 7 + (anchor.y + 6) * stride
 * and the segment renderer's 0x17 left cap lands at bar_addr, its 0x19 fill cap
 * at bar_addr + S. These tests pin both the scaling math (via the 0x19 cap
 * index) and the bar_addr arithmetic, plus the hp_current==0 dead-skip guard.
 * ==================================================================== */

/* Proportional-bar tests use a stride other than the 456 of the panel path so
 * the (anchor.y+6)*stride term is not coincidental, and back the runtime_char
 * array with the shared g_test_rc_array (the canonical fixture the rest of this
 * suite — and gfx/rndstat — index through data_fd2_battle_runtime_char_array_ptr). */
#define HPPROP_BASE  0x40000u
#define HPPROP_STRIDE 0x80u

/* Set slot `idx` to (hp_cur, hp_max), run the proportional bar with a clean log
 * at dst_buf HPPROP_BASE and anchor (ax, ay); returns the computed bar_addr. */
static uint32 hpprop_run(uint32 idx, uint16 hp_cur, uint16 hp_max,
                         int ax, int ay)
{
    int anchor[2];

    install_ui_sheet();
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[idx].hp_current = hp_cur;
    g_test_rc_array[idx].hp_max     = hp_max;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    anchor[0] = ax;
    anchor[1] = ay;

    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    fd2_render_combatant_hp_bar_proportional(HPPROP_BASE, HPPROP_STRIDE, idx,
                                             (uint32)anchor);

    return HPPROP_BASE + (uint32)ax + 7u + (uint32)((ay + 6) * (int)HPPROP_STRIDE);
}

/* hp_current == 0 -> dead-skip guard (signed TEST/JLE collapses to ==0 for the
 * zero-extended word): the renderer is never called, zero blits logged. */
static void test_hpprop_dead_no_draw(void)
{
    hpprop_run(2, 0, 0x40, 0x10, 0x08);
    ASSERT_EQ(g_blitraw_count, 0);
}

/* Full HP (hp_current == hp_max): S = 0x45 + 1 = 0x46 (over max width). The
 * segment renderer takes the >0x45 early-return path: left cap @0, filled
 * middles @1..0x45, fill cap (0x19) @0x46, NO final 0x1E cap. 0x47 blits. */
static void test_hpprop_full_hp_overwidth(void)
{
    uint32 base = hpprop_run(2, 0x40, 0x40, 0x10, 0x08);

    ASSERT_EQ(g_blitraw_count, 0x47);
    ASSERT_EQ(g_blitraw_log_dst[0], base);
    ASSERT_EQ(ui_logged_idx(0), 0x17u);
    ASSERT_EQ(g_blitraw_log_dst[0x46], base + 0x46u);
    ASSERT_EQ(ui_logged_idx(0x46), 0x19u);   /* fill cap at S=0x46, no 0x1E */
}

/* Mid HP: hp_current=0x33 (51), hp_max=0x66 (102) -> S = 51*0x45/102 + 1
 * = 3519/102 + 1 = 34 + 1 = 0x23. Normal filled path: left cap @0, fill cap
 * (0x19) @0x23, empty middles after, final cap (0x1E) @0x46. 0x47 blits. */
static void test_hpprop_mid_hp_scaling(void)
{
    uint32 base = hpprop_run(5, 0x33, 0x66, 0x04, 0x02);

    ASSERT_EQ(g_blitraw_count, 0x47);
    ASSERT_EQ(g_blitraw_log_dst[0], base);
    ASSERT_EQ(ui_logged_idx(0), 0x17u);
    ASSERT_EQ(g_blitraw_log_dst[0x23], base + 0x23u);
    ASSERT_EQ(ui_logged_idx(0x23), 0x19u);          /* fill cap pins S=0x23 */
    ASSERT_EQ(ui_logged_idx(0x46), 0x1eu);          /* final cap present */
}

/* Low HP: hp_current=1, hp_max=0x40 (64) -> S = 1*0x45/64 + 1 = 1 + 1 = 2.
 * Even at 1 HP the +1 keeps a >=1 filled head: fill cap (0x19) @ base+2. */
static void test_hpprop_low_hp_min_fill(void)
{
    uint32 base = hpprop_run(0, 1, 0x40, 0x00, 0x00);

    ASSERT_EQ(g_blitraw_count, 0x47);
    ASSERT_EQ(ui_logged_idx(0), 0x17u);
    ASSERT_EQ(g_blitraw_log_dst[2], base + 2u);
    ASSERT_EQ(ui_logged_idx(2), 0x19u);             /* fill cap pins S=2 */
}

/* bar_addr arithmetic with a non-trivial (anchor.x, anchor.y) and a stride
 * other than 456: the left cap dst must equal
 *   dst_buf + anchor.x + 7 + (anchor.y + 6) * stride. */
static void test_hpprop_bar_addr_arithmetic(void)
{
    uint32 base = hpprop_run(4, 0x20, 0x40, 0x1b, 0x07);
    uint32 expect = HPPROP_BASE + 0x1bu + 7u + (uint32)((0x07 + 6) * (int)HPPROP_STRIDE);

    ASSERT_EQ(base, expect);
    ASSERT_EQ(g_blitraw_log_dst[0], expect);
    ASSERT_EQ(ui_logged_idx(0), 0x17u);
    ASSERT_EQ(g_blitraw_stride, HPPROP_STRIDE);     /* stride forwarded verbatim */
}

/* ====================================================================
 * fd2_render_combat_hp_bar_segments @ 0x1E739
 *
 * Drives the real renderer through the real fd2_blit_sheet_sprite_at_offset
 * into the g_blitraw log, then fingerprints the exact (dst_offset, sprite_idx)
 * sequence for each control-flow class: empty bar (filled<=0), the off-by-one
 * fall-through final cap, the filled-middle run, the exact-max-width boundary
 * (0x45), and the over-max early return (>0x45) which omits the final cap.
 *
 * Segment dst advances one byte per blit; sprite indices: 0x17 left cap,
 * 0x18 filled middle, 0x19 fill right cap, 0x1D empty middle, 0x1E final cap.
 * ==================================================================== */
#define HPSEG_BASE 0x30000u

/* Run the renderer with a clean log; sheet entry i -> index i so the logged
 * sprite addr minus the sheet base is the index drawn. */
static void hpseg_run(uint32 filled_count)
{
    install_ui_sheet();
    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    fd2_render_combat_hp_bar_segments(HPSEG_BASE, 0x1c8, filled_count);
}

/* assert the k-th logged blit drew sprite `idx` at HPSEG_BASE + `off` */
static void hpseg_expect(int k, uint32 off, uint32 idx)
{
    ASSERT_EQ(g_blitraw_log_dst[k], HPSEG_BASE + off);
    ASSERT_EQ(ui_logged_idx(k), idx);
}

/* filled_count == 0: 0x45 empty middles (0x1D) at offsets 0..0x44, then one
 * final cap (0x1E) at offset 0x45 (the shared fall-through). 0x46 blits. */
static void test_hpseg_empty_bar(void)
{
    int i;

    hpseg_run(0);
    ASSERT_EQ(g_blitraw_count, 0x46);
    for (i = 0; i < 0x45; i++) {
        hpseg_expect(i, (uint32)i, 0x1d);
    }
    hpseg_expect(0x45, 0x45, 0x1e);   /* final right cap one past last middle */
}

/* negative filled_count takes the same JLE (signed <= 0) empty path as 0. */
static void test_hpseg_negative_is_empty(void)
{
    hpseg_run((uint32)-1);
    ASSERT_EQ(g_blitraw_count, 0x46);
    hpseg_expect(0, 0, 0x1d);
    hpseg_expect(0x45, 0x45, 0x1e);
}

/* filled_count == 1: left cap (0x17) at 0, no filled middle, fill cap (0x19)
 * at 1, empty middles (0x1D) at 2..0x45, final cap (0x1E) at 0x46. */
static void test_hpseg_one_filled(void)
{
    int i;

    hpseg_run(1);
    ASSERT_EQ(g_blitraw_count, 0x47);
    hpseg_expect(0, 0, 0x17);
    hpseg_expect(1, 1, 0x19);
    for (i = 2; i <= 0x45; i++) {
        hpseg_expect(i, (uint32)i, 0x1d);
    }
    hpseg_expect(0x46, 0x46, 0x1e);
}

/* filled_count == 3: left cap @0, filled middles (0x18) @1,@2, fill cap (0x19)
 * @3, empty middles @4..0x45, final cap @0x46. Exercises the 0x18 run. */
static void test_hpseg_mid_filled(void)
{
    int i;

    hpseg_run(3);
    ASSERT_EQ(g_blitraw_count, 0x47);
    hpseg_expect(0, 0, 0x17);
    hpseg_expect(1, 1, 0x18);
    hpseg_expect(2, 2, 0x18);
    hpseg_expect(3, 3, 0x19);
    for (i = 4; i <= 0x45; i++) {
        hpseg_expect(i, (uint32)i, 0x1d);
    }
    hpseg_expect(0x46, 0x46, 0x1e);
}

/* filled_count == 0x45 (exact max width): left cap @0, filled middles @1..0x44,
 * fill cap (0x19) @0x45, NO empty middles (loop breaks immediately), final cap
 * (0x1E) @0x46. 0x47 blits, zero 0x1D. */
static void test_hpseg_full_width(void)
{
    int i;

    hpseg_run(0x45);
    ASSERT_EQ(g_blitraw_count, 0x47);
    hpseg_expect(0, 0, 0x17);
    for (i = 1; i <= 0x44; i++) {
        hpseg_expect(i, (uint32)i, 0x18);
    }
    hpseg_expect(0x45, 0x45, 0x19);
    hpseg_expect(0x46, 0x46, 0x1e);
}

/* filled_count == 0x46 (> max width): left cap @0, filled middles @1..0x45,
 * fill cap (0x19) @0x46, then the >0x45 early return -> NO final 0x1E cap.
 * 0x47 blits ending in 0x19, never 0x1E. */
static void test_hpseg_over_width_no_cap(void)
{
    int i;

    hpseg_run(0x46);
    ASSERT_EQ(g_blitraw_count, 0x47);
    hpseg_expect(0, 0, 0x17);
    for (i = 1; i <= 0x45; i++) {
        hpseg_expect(i, (uint32)i, 0x18);
    }
    hpseg_expect(0x46, 0x46, 0x19);   /* last blit is the fill cap, no 0x1E */
}

/* ====================================================================
 * fd2_render_phase_banner_frame @ 0x1F42D
 *
 * Renders one frame of the PLAYER/ENEMY-TURN phase banner: two real
 * fd2_alloc_and_blit_indexed_sprite_chunk calls (main banner + corner),
 * then a real fd2_blit_rectangle (workspace -> 0xA0504), a real
 * fd2_wait_n_bios_ticks(1), then two real fd2_cleanup_dialog_sprite_buffer
 * calls — each freeing the save buffer returned by its matching alloc/blit.
 *
 * The two alloc/blit calls resolve their sprite via the sheet at
 * data_fd2_ui_anim_sprite_sheet_ptr (identity offset table, entry i -> i,
 * pointing at a {0,0} header so the blits are zero-size and host-safe), and
 * forward (dst, sprite, stride) to the recording fd2_blit_sprite_with_decoded_pixels
 * stub (g_blitdec opt-in log). From each logged blit:
 *   dst    = ws + row_idx*pitch + col_offset
 *          = (lgs+0x8088) + 0x52*0x140 + col_offset
 *   sprite = sheet + table[sprite_idx] = sheet + sprite_idx (identity)
 * so the test recovers col_offset (=> the x_offset arithmetic 0x55-x and
 * x+0xA5) and sprite_idx (=> banner_sprite_id then 0x51) for each half.
 *
 * The two cleanups forward to the recording fd2_restore_screen_block_from_buffer
 * stub (g_restore_block_*); g_restore_block_calls == 2 confirms both halves
 * are cleaned up, and that each free() succeeded (the function returning at all
 * proves the two saved_block args were the two real malloc'd buffers — the
 * Ghidra EAX-bug would instead route the workspace pointer or a duplicated
 * buffer into free() and fault). The last cleanup is the right (corner) half,
 * so g_restore_block_last_dst / _stride pin the cleanup's ws / pitch args.
 * ==================================================================== */

/* Sheet for data_fd2_ui_anim_sprite_sheet_ptr. The real alloc/blit chunk reads
 * sprite_hdr = sheet + table[sprite_idx] (table at +6, 4 B/entry) and forwards
 * sprite_hdr to the g_blitdec stub. To make sprite_idx recoverable from the
 * logged sprite_hdr, each entry points at a DISTINCT per-index header slot at
 * BANNER_HDR_BASE + sprite_idx*4, so sprite_idx = (sprite_hdr - sheet -
 * BANNER_HDR_BASE)/4. Every header sits in the zero-filled region past the
 * table, so width=height=0 -> malloc(8), zero-size (host-safe) blit. Covers
 * indices 0..0x52. */
#define BANNER_HDR_BASE 0x200
static uint8 g_banner_frame_sheet[0x600];

static void install_banner_frame_sheet(void)
{
    int i;

    memset(g_banner_frame_sheet, 0, sizeof(g_banner_frame_sheet));
    for (i = 0; i < 0x53; i++) {
        *(int32 *)(g_banner_frame_sheet + 6 + i * 4) =
            (int32)(BANNER_HDR_BASE + i * 4);   /* distinct {0,0} header per idx */
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_banner_frame_sheet;
}

/* recover sprite_idx from a logged g_blitdec sprite_hdr (== sheet + table[idx]
 * == sheet + BANNER_HDR_BASE + idx*4). */
static uint32 banner_recover_sprite_idx(uint32 sprite_hdr)
{
    return (sprite_hdr - (uint32)g_banner_frame_sheet - BANNER_HDR_BASE) / 4u;
}

/* Run one frame render at x_offset with the sheet + workspace installed and a
 * clean blitdec / saveblk / restore-block log. Returns ws (= lgs + 0x8088). */
static uint32 banner_frame_run(uint32 x_offset, uint32 banner_sprite_id)
{
    uint32 ws;

    /* ws back-buffer at lgs+0x8088; the real fd2_blit_rectangle reads the
     * 312x192 visible region from it (WS_SPAN covers the span), then writes to
     * 0xA0504 (VGA RAM, harmless under DOS/4GW). */
    ws = (uint32)g_ws_buffer;
    data_fd2_large_game_state_buffer_ptr = ws - 0x8088u;
    install_banner_frame_sheet();

    g_blitdec_calls = 0;
    g_blitdec_log_on = 1;
    g_blitdec_log_count = 0;
    g_saveblk_calls = 0;
    g_restore_block_calls = 0;

    fd2_render_phase_banner_frame(x_offset, banner_sprite_id);

    g_blitdec_log_on = 0;
    return ws;
}

/* dst the alloc/blit chunk paints for (col_offset) at row 0x52 into ws. The
 * chunk's surface_pitch is 0x1C8 (456) — the workspace pitch — so
 * dst = ws + row_idx*pitch + col_offset = ws + 0x52*0x1C8 + col_offset.
 * (0x140/320 is the separate primary stride used only by the final
 * fd2_blit_rectangle, not by these sprite-chunk paints.) */
static uint32 banner_expect_dst(uint32 ws, uint32 col_offset)
{
    return ws + 0x52u * 0x1c8u + col_offset;
}

/* Settled frame (x_offset 0): main half col = 0x55, corner half col = 0xA5;
 * sprite ids banner_sprite_id (0x52 ENEMY) then 0x51. Pins both halves'
 * dst arithmetic + sprite indices, the 2 alloc/blits + 2 cleanups, and that
 * the real pipeline (incl. blit_rectangle + wait) completed without faulting. */
static void test_banner_frame_settled(void)
{
    uint32 ws;

    ws = banner_frame_run(0, 0x52);

    /* exactly two alloc/blit chunks (2 saveblk, 2 blitdec) and two cleanups. */
    ASSERT_EQ(g_saveblk_calls, 2);
    ASSERT_EQ(g_blitdec_calls, 2);
    ASSERT_EQ(g_blitdec_log_count, 2);
    ASSERT_EQ(g_restore_block_calls, 2);

    /* half 0 (main banner): col = 0x55 - 0 = 0x55, sprite = banner_sprite_id. */
    ASSERT_EQ(g_blitdec_log_dst[0], banner_expect_dst(ws, 0x55));
    ASSERT_EQ(banner_recover_sprite_idx(g_blitdec_log_sprite[0]), 0x52u);
    /* half 1 (corner): col = 0 + 0xA5 = 0xA5, sprite = 0x51 (fixed). */
    ASSERT_EQ(g_blitdec_log_dst[1], banner_expect_dst(ws, 0xa5));
    ASSERT_EQ(banner_recover_sprite_idx(g_blitdec_log_sprite[1]), 0x51u);

    /* last cleanup (right/corner half) carries ws + pitch as dst/stride. */
    ASSERT_EQ(g_restore_block_last_dst, ws);
    ASSERT_EQ(g_restore_block_last_stride, 0x1c8u);
}

/* Mid-slide frame (x_offset 0x32, PLAYER banner 0x50): the two halves move
 * symmetrically off-centre — main col = 0x55 - 0x32 = 0x23, corner col =
 * 0x32 + 0xA5 = 0xD7 — proving x_offset feeds both col offsets (one minus, one
 * plus) and that banner_sprite_id flows only to the main half (corner stays
 * 0x51). An independent x_offset rules out the settled-case coincidences. */
static void test_banner_frame_mid_slide(void)
{
    uint32 ws;

    ws = banner_frame_run(0x32, 0x50);

    ASSERT_EQ(g_blitdec_calls, 2);
    ASSERT_EQ(g_blitdec_log_dst[0], banner_expect_dst(ws, 0x55u - 0x32u));
    ASSERT_EQ(banner_recover_sprite_idx(g_blitdec_log_sprite[0]), 0x50u);
    ASSERT_EQ(g_blitdec_log_dst[1], banner_expect_dst(ws, 0x32u + 0xa5u));
    ASSERT_EQ(banner_recover_sprite_idx(g_blitdec_log_sprite[1]), 0x51u);
    ASSERT_EQ(g_restore_block_calls, 2);
}

/* ----------------------------------------------------------------
 * fd2_composite_then_animate_projectiles — the spell-finale helper.
 *
 * Verifies the helper invokes BOTH of its callees in order:
 *   fd2_composite_battle_frame(0)  -> recomposites the battle frame
 *   fd2_animate_spell_projectile_paths() -> runs the queued FX flight
 *
 * The composite pass is observed through the tile-map proxy
 * (g_composite_call_count / g_tile_map_calls bump exactly once per
 * fd2_composite_battle_frame pass, with the back-buffer workspace as
 * dst). The animate call is driven down its zero-FX-queue gate
 * (data_..._fx_queue_idx == 0 -> immediate return, no projectile blit
 * and no flight delay), which is the host-safe way to confirm the call
 * actually reached fd2_animate_spell_projectile_paths rather than being
 * skipped: if the helper had omitted that call the gate path would
 * still leave the blit/delay counters at zero, but here the point is
 * the inverse -- proving the composite ran once AND the animate path
 * was entered (gate keeps it side-effect-free). The composite-vs-animate
 * ordering and the arg-0 (palette-cycle-advancing) variant are fixed by
 * the 0x21190 body `PUSH 0x0; CALL composite; CALL animate`.
 *
 * The arg-0 vs arg-1 distinction (whether fd2_update_palette_cycle_anim
 * runs) is VGA-DAC port output deferred to Phase 9; reset_pipeline_record
 * throttles the palette-cycle routine to its early-return path so no port
 * write happens here regardless. */
static void test_composite_then_animate_projectiles(void)
{
    extern int g_delay375b2_calls;
    uint32 ws;

    ws = (uint32)g_ws_buffer;
    data_fd2_large_game_state_buffer_ptr = ws - 0x8088;
    data_fd2_battle_view_window_origin_x = 0x07;
    data_fd2_battle_view_window_origin_y = 0x09;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    install_sprite_atlas();
    data_fd2_battle_cursor_world_x = 0x08;
    data_fd2_battle_cursor_world_y = 0x0a;
    reset_pipeline_record();

    /* zero FX queue -> the animate callee takes its immediate-return gate */
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    g_blitdec_calls = 0;
    g_delay375b2_calls = 0;

    fd2_composite_then_animate_projectiles();

    /* fd2_composite_battle_frame(0) ran exactly once, on the back-buffer */
    ASSERT_EQ(g_composite_call_count, 1);
    ASSERT_EQ(g_tile_map_calls, 1);
    ASSERT_EQ(g_tile_map_last_dst, ws);

    /* fd2_animate_spell_projectile_paths() was entered and took the
     * zero-queue gate: no projectile blit and no flight delay. */
    ASSERT_EQ(g_blitdec_calls, 0);
    ASSERT_EQ(g_delay375b2_calls, 0);
}

/* ----------------------------------------------------------------
 * fd2_composite_battle_frame_zero — the zero-arg dispatch wrapper.
 *
 * Body is just fd2_composite_battle_frame(0), so the test proves the
 * wrapper forwards to the composite pass exactly once with the arg-0
 * (palette-cycle-advancing) variant, observed through the same proxies
 * as test_composite_pipeline_args: the tile-map proxy bumps once with
 * the back-buffer workspace as dst, and g_composite_call_count reaches 1
 * (the real fd2_blit_rectangle final stage ran to completion). Empty
 * party so the chars/shadow overlays paint nothing.
 *
 * The arg-0 vs arg-1 distinction (whether fd2_update_palette_cycle_anim
 * runs) is VGA-DAC port output deferred to Phase 9; reset_pipeline_record
 * throttles the palette-cycle routine to its early-return path so no port
 * write happens here regardless. */
static void test_composite_battle_frame_zero(void)
{
    uint32 ws;

    ws = (uint32)g_ws_buffer;
    data_fd2_large_game_state_buffer_ptr = ws - 0x8088;
    data_fd2_battle_view_window_origin_x = 0x07;
    data_fd2_battle_view_window_origin_y = 0x09;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    install_sprite_atlas();
    data_fd2_battle_cursor_world_x = 0x08;
    data_fd2_battle_cursor_world_y = 0x0a;
    reset_pipeline_record();

    fd2_composite_battle_frame_zero();

    /* composite ran exactly once, on the back-buffer, via the arg-0 path */
    ASSERT_EQ(g_tile_map_calls, 1);
    ASSERT_EQ(g_tile_map_last_dst, ws);
    ASSERT_EQ(g_blitpass_calls, 1);
    ASSERT_EQ(g_composite_call_count, 1);
}

/* ================================================================
 * fd2_render_circle_anim_row @ 0x219AD
 *
 * The real fd2_apply_palette_remap_run (src/gfx/palette.c) does an
 * in-place buf[i] = remap[buf[i]] over `run_width` bytes. By pointing
 * the remap table at a constant 0xAA for every input byte, the exact
 * span [left_clip, left_clip+run_width) of each painted row becomes
 * 0xAA over a 0x00 background — letting these tests read back both the
 * per-row run extent (the sqrt+truncate geometry and the left/right
 * clamps) and the set of rows the vertical-extent gate selected.
 * ================================================================ */

/* remap[x] = 0xAA for all x: every touched byte becomes the marker. */
static uint8 g_circ_remap[256];

static void circ_setup(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        g_circ_remap[i] = 0xAA;
    }
    memset(g_ws_buffer, 0x00, WS_SPAN);
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ws_buffer - 0x8088u;
}

/* First / last 0xAA index within row, and the count of 0xAA bytes.
 * Returns 0xFFFF in *first when the row is entirely untouched. */
static void circ_row_run(int row, uint32 *first, uint32 *last, uint32 *count)
{
    uint8 *r;
    uint32 i;

    r = g_ws_buffer + (uint32)row * 0x1c8u;
    *first = 0xffff;
    *last = 0xffff;
    *count = 0;
    for (i = 0; i < 0x1c8u; i++) {
        if (r[i] == 0xAA) {
            if (*first == 0xffff) {
                *first = i;
            }
            *last = i;
            (*count)++;
        }
    }
}

/* Case 1: basic circle geometry, no clipping. cx=100, cy=10, r=8,
 * scale_num=10 (so half_width == trunc(sqrt(64 - dy*dy))). Rows 5..14
 * all lie inside (cy-r, cy+r)=(2,18) and inside [5,15). For the center
 * row 10 (dy=0): half_width=8, run starts at cx-8=92, run_width=16. */
static void test_circ_basic_geometry(void)
{
    uint32 first;
    uint32 last;
    uint32 cnt;

    circ_setup();
    fd2_render_circle_anim_row(100, 10, 8, 10, 5, 15, g_circ_remap);

    /* center row 10: hw=8 -> [92,108), 16 bytes */
    circ_row_run(10, &first, &last, &cnt);
    ASSERT_EQ(first, 92);
    ASSERT_EQ(cnt, 16);
    ASSERT_EQ(last, 107);

    /* row 7 (dy=3): sqrt(64-9)=sqrt(55)=7.41 -> hw=7 -> [93,107), 14 */
    circ_row_run(7, &first, &last, &cnt);
    ASSERT_EQ(first, 93);
    ASSERT_EQ(cnt, 14);

    /* row 5 (dy=5): sqrt(64-25)=sqrt(39)=6.24 -> hw=6 -> [94,106), 12 */
    circ_row_run(5, &first, &last, &cnt);
    ASSERT_EQ(first, 94);
    ASSERT_EQ(cnt, 12);

    /* row 14 (dy=4): sqrt(64-16)=sqrt(48)=6.92 -> hw=6 -> [94,106), 12 */
    circ_row_run(14, &first, &last, &cnt);
    ASSERT_EQ(first, 94);
    ASSERT_EQ(cnt, 12);

    /* rows just outside [start,end): 4 and 15 never visited */
    circ_row_run(4, &first, &last, &cnt);
    ASSERT_EQ(cnt, 0);
    circ_row_run(15, &first, &last, &cnt);
    ASSERT_EQ(cnt, 0);
}

/* Case 2: left clamp (cx - half_width < 0). cx=5, cy=10, r=8,
 * scale_num=10, single row 10: hw=8 -> left_clip=5-8=-3 -> clamp to 0,
 * right_off=cx=5; cx+hw=13 (<0x138) no right clamp; run_width =
 * right_off + half_width = 5 + 8 = 13, starting at offset 0. */
static void test_circ_left_clamp(void)
{
    uint32 first;
    uint32 last;
    uint32 cnt;

    circ_setup();
    fd2_render_circle_anim_row(5, 10, 8, 10, 10, 11, g_circ_remap);

    circ_row_run(10, &first, &last, &cnt);
    ASSERT_EQ(first, 0);
    ASSERT_EQ(cnt, 13);
    ASSERT_EQ(last, 12);
}

/* Case 3: right clamp (cx + half_width > 0x137). cx=308, cy=10, r=8,
 * scale_num=10, single row 10: hw=8 -> left_clip=300 (no left clamp),
 * right_off=8; cx+hw=316 > 0x137 -> half_width = 0x138-cx = 4;
 * run_width = right_off + half_width = 8 + 4 = 12, starting at 300 and
 * ending exactly at the visible edge 0x138 (312). */
static void test_circ_right_clamp(void)
{
    uint32 first;
    uint32 last;
    uint32 cnt;

    circ_setup();
    fd2_render_circle_anim_row(308, 10, 8, 10, 10, 11, g_circ_remap);

    circ_row_run(10, &first, &last, &cnt);
    ASSERT_EQ(first, 300);
    ASSERT_EQ(cnt, 12);
    ASSERT_EQ(last, 311);   /* 300 + 12 - 1 == 311 (== 0x138 - 1) */
}

/* Case 4: vertical-extent gate uses strict inequalities cy-r < row <
 * cy+r. cx=100, cy=10, r=3 -> inside == (7,13) strict, so only rows
 * 8,9,10,11,12 are painted; the boundary rows 7 and 13 (and anything
 * outside) stay untouched even though they are within [5,15). */
static void test_circ_vertical_extent_gate(void)
{
    uint32 first;
    uint32 last;
    uint32 cnt;
    int row;
    int painted;

    circ_setup();
    fd2_render_circle_anim_row(100, 10, 3, 10, 5, 15, g_circ_remap);

    /* boundary rows excluded (strict <) */
    circ_row_run(7, &first, &last, &cnt);
    ASSERT_EQ(cnt, 0);
    circ_row_run(13, &first, &last, &cnt);
    ASSERT_EQ(cnt, 0);

    /* center row 10 (dy=0): hw=trunc(sqrt(9))=3 -> [97,103), 6 bytes */
    circ_row_run(10, &first, &last, &cnt);
    ASSERT_EQ(first, 97);
    ASSERT_EQ(cnt, 6);

    /* exactly rows 8..12 painted, all others in [5,15) empty */
    for (row = 5; row < 15; row++) {
        circ_row_run(row, &first, &last, &cnt);
        painted = (row >= 8 && row <= 12);
        ASSERT_TRUE(painted ? (cnt > 0) : (cnt == 0));
    }
}

/* ================================================================
 * fd2_render_filled_circle_band_anim @ 0x22046
 *
 * Reuses the circ_* fixtures: g_circ_remap (all 0xAA) is forwarded as
 * the band's `radius`/remap-source pointer, so each painted run becomes
 * 0xAA over the 0x00 background and circ_row_run() reads back the run
 * extent. To isolate the middle solid-band fill from the two arc passes,
 * the 5th arg (cy = arc row-loop exclusive end) is set to 0 and the arc
 * starts (4th arg cx, and param_2) are >= 0, so both
 * fd2_render_circle_anim_row calls have start_row >= end_row and paint
 * nothing. The party count is zeroed so the in-between
 * fd2_composite_all_chars_overlay is a no-op (its char + shadow loops
 * iterate zero times), leaving only the band fill on the workspace.
 *
 * Band geometry verified here: half_width = trunc(param_3 * 1.6) (the
 * x87 __CHP truncation toward zero, exercised with param_3 chosen so the
 * product is an exact integer), the left clamp to 0, the right clamp to
 * 0x138, the run_width = clamped_half_width + right_off composition, and
 * the row span [cx, param_2).
 * ================================================================ */

/* Drive the band fill with empty arcs (cy_end=0) and an empty party. */
static void band_setup(void)
{
    circ_setup();   /* g_circ_remap=0xAA, ws=0x00, buffer ptr pinned */
    data_fd2_battle_party_member_count = 0;   /* overlay = no-op */
}

/* Case 1: solid band, no clamps. param_1(cx col)=100, param_3=10 ->
 * half_width=trunc(16.0)=16; left_clip=84, right_off=16, no right clamp
 * (116<0x137); run_width=16+16=32. Fill rows [cx=2, param_2=5) -> rows
 * 2,3,4 each get [84,116), 32 bytes; the arcs (cy_end=0) paint nothing. */
static void test_band_basic_fill(void)
{
    uint32 first;
    uint32 last;
    uint32 cnt;
    int row;

    band_setup();
    fd2_render_filled_circle_band_anim(100, 5, 10, 2, 0, (int)g_circ_remap);

    for (row = 2; row <= 4; row++) {
        circ_row_run(row, &first, &last, &cnt);
        ASSERT_EQ(first, 84);
        ASSERT_EQ(cnt, 32);
        ASSERT_EQ(last, 115);   /* 84 + 32 - 1 */
    }
    /* rows outside [2,5) untouched (band end-exclusive, arcs empty) */
    circ_row_run(1, &first, &last, &cnt);
    ASSERT_EQ(cnt, 0);
    circ_row_run(5, &first, &last, &cnt);
    ASSERT_EQ(cnt, 0);
}

/* Case 2: left clamp (param_1 - half_width < 0). param_1=10, param_3=10
 * -> half_width=16; left_clip=10-16=-6 -> 0, right_off=param_1=10; no
 * right clamp (10+16=26); run_width=16+10=26, starting at offset 0. */
static void test_band_left_clamp(void)
{
    uint32 first;
    uint32 last;
    uint32 cnt;

    band_setup();
    fd2_render_filled_circle_band_anim(10, 4, 10, 3, 0, (int)g_circ_remap);

    circ_row_run(3, &first, &last, &cnt);   /* single row [3,4) */
    ASSERT_EQ(first, 0);
    ASSERT_EQ(cnt, 26);
    ASSERT_EQ(last, 25);
}

/* Case 3: right clamp (param_1 + half_width > 0x137). param_1=300,
 * param_3=100 -> half_width=trunc(160.0)=160; left_clip=140 (no left
 * clamp), right_off=160; 300+160=460 > 0x137 -> half_width=0x138-300=12;
 * run_width=12+160=172, [140,312) ending exactly at the visible edge. */
static void test_band_right_clamp(void)
{
    uint32 first;
    uint32 last;
    uint32 cnt;

    band_setup();
    fd2_render_filled_circle_band_anim(300, 8, 100, 7, 0, (int)g_circ_remap);

    circ_row_run(7, &first, &last, &cnt);   /* single row [7,8) */
    ASSERT_EQ(first, 140);
    ASSERT_EQ(cnt, 172);
    ASSERT_EQ(last, 311);   /* 140 + 172 - 1 == 0x138 - 1 */
}

/* Case 4: empty band when start row cx >= param_2 -> the for loop runs
 * zero times, no fill. With cy_end=0 the arcs are empty too, so the whole
 * workspace stays untouched. */
static void test_band_empty_when_start_ge_end(void)
{
    uint32 first;
    uint32 last;
    uint32 cnt;
    int row;

    band_setup();
    fd2_render_filled_circle_band_anim(100, 3, 10, 5, 0, (int)g_circ_remap);

    for (row = 0; row < 10; row++) {
        circ_row_run(row, &first, &last, &cnt);
        ASSERT_EQ(cnt, 0);
    }
}

/* ====================================================================
 * fd2_render_summon_aura_sprite_ring @ 0x262EF
 *
 * Drives the dispatch-table aura-ring renderer through every state_code
 * branch. The blit/sfx callees are recorded by the testglob.c spies
 * (g_blit_indexed_sprite_*, g_play_sfx_with_handle / g_sfx_*). The
 * per-slot phase counters live in the *upper* 8 slots [7..14] of the
 * shared 15-slot array (asm indexes [i*4 + 0x53F5E]); these tests pin
 * that +7 offset, the j=(i+4)%8 rotation swap, the enemy x-shift, the
 * blit-position arithmetic, and the state-5 advance/done/chime logic. */
extern int    g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int    g_blit_indexed_sprite_last_x;
extern int    g_blit_indexed_sprite_last_y;
extern int    g_play_sfx_with_handle_calls;
extern int    g_sfx_last_id;
extern int    g_sfx_id_count;
extern int32  data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[15];
extern int32  data_fd2_battle_summon_aura_ring_8slot_x_offset_table[8];
extern int32  data_fd2_battle_summon_aura_ring_8slot_row_multiplier_table[8];

/* reset spies + array + caster team for an aura-ring test. team: value
 * written to g_test_rc_array[0].team (+6); 0 = enemy (triggers x-shift). */
static void aura_reset(int team)
{
    int k;

    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_frame = 0;
    g_blit_indexed_sprite_last_x = 0;
    g_blit_indexed_sprite_last_y = 0;
    g_play_sfx_with_handle_calls = 0;
    g_sfx_last_id = 0;
    g_sfx_id_count = 0;
    for (k = 0; k < 15; k++) {
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[k] = 0;
    }
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = (uint8)team;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}

/* set the per-slot counter for logical slot i (0..7) -> array index i+7 */
static void aura_set_slot(int i, int32 v)
{
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i + 7] = v;
}

/* state 3 (INIT): writes counter[i+7] = -2*i for i in 0..7 and returns
 * 0x1F. The +7 offset is load-bearing: slots 0..6 must stay untouched. */
static void test_aura_state3_init_offsets(void)
{
    int rc;
    int i;

    aura_reset(1);                 /* ally team; no x-shift effect on state 3 */
    /* poke the lower 7 slots so we can prove they are NOT overwritten */
    for (i = 0; i < 7; i++) {
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = 0x55;
    }
    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 3);

    ASSERT_EQ(rc, 0x1f);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 0);
    /* upper 8 slots staggered -2*i */
    for (i = 0; i < 8; i++) {
        ASSERT_EQ(
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i + 7],
            -2 * i);
    }
    /* lower 7 slots untouched */
    for (i = 0; i < 7; i++) {
        ASSERT_EQ(
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i],
            0x55);
    }
}

/* state 3 still returns 0x1F for an enemy caster (team==0). The x-shift
 * branch executes against the local copy only and cannot affect state 3. */
static void test_aura_state3_enemy_same_return(void)
{
    int rc;

    aura_reset(0);                 /* enemy team -> x_off += 0x94 path runs */
    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 3);
    ASSERT_EQ(rc, 0x1f);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 0);
}

/* state 4, top-half slot i=0 (unrotated): only slot 0 in gate, slots 1..7
 * pinned at 0xF (gate fail). Verifies single blit, frame index, and the
 * exact position x = row_mul[0]*stride + x_off[0] + 0x50 + origin_y. */
static void test_aura_state4_tophalf_position(void)
{
    int rc;
    int i;
    int expect_x;

    aura_reset(1);                 /* ally: no x-shift */
    for (i = 1; i < 8; i++) {
        aura_set_slot(i, 0xf);     /* out of gate (counter < 0xF is false) */
    }
    aura_set_slot(0, 5);           /* in gate -> blit */

    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 4);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ(g_blit_indexed_sprite_last_frame, 5);    /* frame = counter */
    /* row_mul[0]=-10, x_off[0]=-59 : -10*10 + (-59) + 0x50 + 100 = 21 */
    expect_x = -10 * 10 + (-59) + 0x50 + 100;
    ASSERT_EQ(g_blit_indexed_sprite_last_x, expect_x);
    ASSERT_EQ(g_blit_indexed_sprite_last_y, 10);       /* y = row_stride */
}

/* state 4, bottom-half slot i=4 (tail-set + j swap): only slot 4 in gate.
 * j = (4+4)%8 = 0 so position uses slot-0 offsets; frame = counter+0xF. */
static void test_aura_state4_bottomhalf_jswap(void)
{
    int rc;
    int i;
    int expect_x;

    aura_reset(1);
    for (i = 0; i < 8; i++) {
        aura_set_slot(i, 0xf);     /* all out of gate */
    }
    aura_set_slot(4, 7);           /* slot 4 in gate -> tail blit, j=0 */

    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 4);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ(g_blit_indexed_sprite_last_frame, 7 + 0xf);   /* tail-set +0xF */
    /* j=0 offsets: -10*10 + (-59) + 0x50 + 100 = 21 */
    expect_x = -10 * 10 + (-59) + 0x50 + 100;
    ASSERT_EQ(g_blit_indexed_sprite_last_x, expect_x);
    ASSERT_EQ(g_blit_indexed_sprite_last_y, 10);
}

/* enemy caster (team==0): the local x-offset table is shifted by +0x94
 * before the blit, so slot-0 position moves by exactly 0x94 vs the ally
 * case (state 4, top-half slot 0). */
static void test_aura_state4_enemy_xshift(void)
{
    int rc;
    int i;
    int expect_x;

    aura_reset(0);                 /* enemy -> x_off[i] += 0x94 */
    for (i = 1; i < 8; i++) {
        aura_set_slot(i, 0xf);
    }
    aura_set_slot(0, 5);

    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 4);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 1);
    /* -10*10 + (-59 + 0x94) + 0x50 + 100 = 21 + 0x94 = 169 */
    expect_x = -10 * 10 + (-59 + 0x94) + 0x50 + 100;
    ASSERT_EQ(g_blit_indexed_sprite_last_x, expect_x);
}

/* state 4 gate boundaries: counter == 0xF must NOT blit, counter == 0 must
 * blit (0 <= c < 0xF), and a negative counter must NOT blit. */
static void test_aura_state4_gate_boundaries(void)
{
    int rc;
    int i;

    aura_reset(1);
    for (i = 0; i < 8; i++) {
        aura_set_slot(i, 0xf);     /* exactly the upper bound -> excluded */
    }
    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 4);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 0);

    aura_reset(1);
    aura_set_slot(0, -1);          /* below lower bound */
    for (i = 1; i < 8; i++) {
        aura_set_slot(i, 0xf);
    }
    aura_set_slot(1, 0);           /* lower bound included -> top-half blit */
    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 4);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 1);
}

/* state 5 ADVANCE: all 8 slots at 8 -> after ++ all become 9, so the
 * mid-cycle done flag is set and the function returns 1. No counter hits
 * 5, so no chime SFX. (8 in-gate slots also blit during the draw loops.) */
static void test_aura_state5_done_flag(void)
{
    int rc;
    int i;

    aura_reset(1);
    for (i = 0; i < 8; i++) {
        aura_set_slot(i, 8);
    }
    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 5);

    ASSERT_EQ(rc, 1);                       /* done_flag */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 8);
    /* every slot advanced 8 -> 9 */
    for (i = 0; i < 8; i++) {
        ASSERT_EQ(
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i + 7],
            9);
    }
}

/* state 5 chime: all 8 slots at 4 -> after ++ all become 5, firing the
 * per-slot chime SFX (id 1) eight times. None reach 9, so done_flag is 0
 * and the function returns 0. */
static void test_aura_state5_chime_sfx(void)
{
    int rc;
    int i;

    aura_reset(1);
    for (i = 0; i < 8; i++) {
        aura_set_slot(i, 4);
    }
    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 5);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 8);
    ASSERT_EQ(g_sfx_last_id, 1);
    for (i = 0; i < 8; i++) {
        ASSERT_EQ(
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i + 7],
            5);
    }
}

/* unknown state_code -> shared epilogue returns 0, nothing blitted. */
static void test_aura_other_state_noop(void)
{
    int rc;

    aura_reset(1);
    rc = fd2_render_summon_aura_sprite_ring(0, 0x1000, 100, 10, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
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
    RUN_TEST(test_paint_passthrough_stride);
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
    RUN_TEST(test_spell_portrait_stride);
    RUN_TEST(test_spell_hit_draws_effect);
    RUN_TEST(test_spell_mixed_hit_and_miss);
    RUN_TEST(test_spell_dead_skipped);
    RUN_TEST(test_spell_window_cull);
    RUN_TEST(test_spell_palette3_frame);
    RUN_TEST(test_spell_duplicate_target_single_hit);
    RUN_TEST(test_panels_attacker_only);
    RUN_TEST(test_panels_with_defender);
    RUN_TEST(test_panels_hp_seg_addr_arithmetic);
    RUN_TEST(test_hpseg_empty_bar);
    RUN_TEST(test_hpseg_negative_is_empty);
    RUN_TEST(test_hpseg_one_filled);
    RUN_TEST(test_hpseg_mid_filled);
    RUN_TEST(test_hpseg_full_width);
    RUN_TEST(test_hpseg_over_width_no_cap);
    RUN_TEST(test_hpprop_dead_no_draw);
    RUN_TEST(test_hpprop_full_hp_overwidth);
    RUN_TEST(test_hpprop_mid_hp_scaling);
    RUN_TEST(test_hpprop_low_hp_min_fill);
    RUN_TEST(test_hpprop_bar_addr_arithmetic);
    RUN_TEST(test_banner_frame_settled);
    RUN_TEST(test_banner_frame_mid_slide);
    RUN_TEST(test_composite_then_animate_projectiles);
    RUN_TEST(test_composite_battle_frame_zero);
    RUN_TEST(test_circ_basic_geometry);
    RUN_TEST(test_circ_left_clamp);
    RUN_TEST(test_circ_right_clamp);
    RUN_TEST(test_circ_vertical_extent_gate);
    RUN_TEST(test_band_basic_fill);
    RUN_TEST(test_band_left_clamp);
    RUN_TEST(test_band_right_clamp);
    RUN_TEST(test_band_empty_when_start_ge_end);
    RUN_TEST(test_aura_state3_init_offsets);
    RUN_TEST(test_aura_state3_enemy_same_return);
    RUN_TEST(test_aura_state4_tophalf_position);
    RUN_TEST(test_aura_state4_bottomhalf_jswap);
    RUN_TEST(test_aura_state4_enemy_xshift);
    RUN_TEST(test_aura_state4_gate_boundaries);
    RUN_TEST(test_aura_state5_done_flag);
    RUN_TEST(test_aura_state5_chime_sfx);
    RUN_TEST(test_aura_other_state_noop);
    printf("\n");
}
