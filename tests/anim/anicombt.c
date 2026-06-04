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
extern int    g_sfx_last_id;
extern int    g_sfx_id_count;
extern int    g_sfx_id_log[64];

/* decoded-pixel sprite blit recording (testglob.c) */
extern uint32 g_blitdec_dst, g_blitdec_sprite, g_blitdec_stride;
extern int    g_blitdec_calls;

/* per-spell animation parameter tables (testglob.c, real binary bytes) */
extern uint8  data_fd2_animation_spell_sprite_offset_table[33];
extern uint8  data_fd2_animation_spell_frame_count_table[33];
extern uint8  data_fd2_animation_spell_sfx_frame_table[33];

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

/* ================================================================
 * fd2_animate_spell_impact_per_target tests
 * ================================================================ */

/* Scratch sheet for data_fd2_resource_portrait_sheet_ptr. The function reads a
 * dword at [6 + (sprite_off+frame)*4] and adds the sheet base to it to form the
 * frame sprite pointer. Sized for the worst case (spell 9: sprite_off 0x57 + 26
 * frames -> byte index 6 + (0x57+26)*4 = 458). */
static uint8 g_impact_sheet[2048];

static void setup_impact(void)
{
    g_play_sfx_with_handle_calls = 0;
    g_sfx_id_count = 0;
    g_sfx_last_id = 0;
    memset(g_sfx_id_log, 0, sizeof(g_sfx_id_log));
    g_blitdec_calls = 0;
    g_blitdec_dst = 0;
    g_blitdec_sprite = 0;
    g_blitdec_stride = 0;

    memset(g_lgs, 0, sizeof(g_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgs;

    memset(g_impact_sheet, 0, sizeof(g_impact_sheet));
    data_fd2_resource_portrait_sheet_ptr = (uint32)g_impact_sheet;

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
}

/*
 * Drives the per-spell SFX dispatch chain with target_count = 0 (no blits),
 * so only the per-frame SFX logic runs. Asserts the exact fired-id sequence
 * against the real tables for every special-cased spell plus a table-only and
 * a silent spell. This is the highest-risk control flow in the function.
 */
static void test_impact_sfx_dispatch_sequences(void)
{
    /* spell 0x16: frame_count 13; sfx_tbl[0x16]=3; special frame 7 -> 3.
       expected fires: frame0=3, frame7=3 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0xDEAD, 0x16, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 2);
    ASSERT_EQ(g_sfx_id_log[0], 3);
    ASSERT_EQ(g_sfx_id_log[1], 3);

    /* spell 0x19: frame_count 13; sfx_tbl[0x19]=5; special frames 3,6 -> 5.
       expected: frame0=5, frame3=5, frame6=5 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x19, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 3);
    ASSERT_EQ(g_sfx_id_log[0], 5);
    ASSERT_EQ(g_sfx_id_log[1], 5);
    ASSERT_EQ(g_sfx_id_log[2], 5);

    /* spell 0x12: frame_count 12; sfx_tbl[0x12]=7; special frame 4 -> 7.
       expected: frame0=7, frame4=7 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x12, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 2);
    ASSERT_EQ(g_sfx_id_log[0], 7);
    ASSERT_EQ(g_sfx_id_log[1], 7);

    /* spell 0x13: frame_count 13; sfx_tbl[0x13]=8; special frames 3,6 -> 8.
       expected: frame0=8, frame3=8, frame6=8 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x13, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 3);
    ASSERT_EQ(g_sfx_id_log[0], 8);
    ASSERT_EQ(g_sfx_id_log[1], 8);
    ASSERT_EQ(g_sfx_id_log[2], 8);

    /* spell 0x08: frame_count 11; sfx_tbl[8]=0x0A; special frames 3,6 -> 0x0A.
       expected: frame0=10, frame3=10, frame6=10 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x08, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 3);
    ASSERT_EQ(g_sfx_id_log[0], 10);
    ASSERT_EQ(g_sfx_id_log[1], 10);
    ASSERT_EQ(g_sfx_id_log[2], 10);

    /* spell 0x09: frame_count 27; sfx_tbl[9]=0x0E; special frames 0xF,0x13 -> 0xF.
       expected: frame0=14, frame15=15, frame19=15 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x09, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 3);
    ASSERT_EQ(g_sfx_id_log[0], 14);
    ASSERT_EQ(g_sfx_id_log[1], 15);
    ASSERT_EQ(g_sfx_id_log[2], 15);

    /* spell 0x00: table-only sfx (sfx_tbl[0]=6), not special; one fire on frame0 */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x00, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 1);
    ASSERT_EQ(g_sfx_id_log[0], 6);

    /* spell 0x0A: sfx_tbl[0x0A]=0 and not special -> zero fires over 8 frames */
    setup_impact();
    fd2_animate_spell_impact_per_target(0, 0x0A, 0, 0);
    ASSERT_EQ(g_sfx_id_count, 0);
}

/*
 * Window-cull predicate, per-frame blit count, dst/frame-sprite arithmetic.
 * Uses spell 0x0A (8 frames, no SFX) with one in-window and one out-of-window
 * target so each frame blits exactly once (the in-window char).
 */
static void test_impact_cull_and_arithmetic(void)
{
    uint8 idx_array[2];
    uint32 *sheet;
    uint32 sprite_off;
    uint32 last_frame;
    uint32 exp_src;
    uint32 exp_dst;

    setup_impact();

    /* seed the sheet dword table: entry[i] = i*0x10 so the resolved frame
       pointer (sheet + table[6+(off+frame)*4]) is frame-distinguishable */
    sheet = (uint32 *)(g_impact_sheet + 6);
    {
        int i;
        for (i = 0; i < 500; i++) {
            sheet[i] = (uint32)i * 0x10u;
        }
    }

    /* char 0 inside the window */
    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;
    /* char 1 past the right edge (OX+MX = 0x1D) -> culled */
    g_test_rc_array[1].pos_x = 0x1E;
    g_test_rc_array[1].pos_y = 0x24;

    idx_array[0] = 0;
    idx_array[1] = 1;

    /* spell 0x0A: 8 frames, no SFX, sprite_off table[0x0A] = 0x31 */
    sprite_off = data_fd2_animation_spell_sprite_offset_table[0x0A];
    fd2_animate_spell_impact_per_target(0, 0x0A, 2, (uint32)idx_array);

    /* one blit per frame (in-window char only); 8 frames */
    ASSERT_EQ(g_blitdec_calls, 8);
    ASSERT_EQ(data_fd2_animation_spell_frame_count_table[0x0A], 8);

    /* last recorded blit is the final frame (frame 7) of the in-window char.
       frame_sprite_addr = sheet_base + table[6 + (sprite_off+frame)*4],
       and table[k] == k*0x10 where k = sprite_off + frame */
    last_frame = 7u;
    exp_src = (uint32)g_impact_sheet
            + (sprite_off + last_frame) * 0x10u;
    ASSERT_EQ(g_blitdec_sprite, exp_src);

    /* dst = lgs + (pos_y-OY)*0x2AC0 + (pos_x-OX)*0x18 + 0x75D8 */
    exp_dst = (uint32)g_lgs
            + (0x24u - WIN_OY) * 0x2ac0u
            + (0x15u - WIN_OX) * 0x18u
            + 0x75d8u;
    ASSERT_EQ(g_blitdec_dst, exp_dst);

    /* stride is the fixed 0x1C8 */
    ASSERT_EQ(g_blitdec_stride, 0x1c8u);

    /* spell 0x0A fires no SFX */
    ASSERT_EQ(g_sfx_id_count, 0);
}

/*
 * Zero-frame guard: a spell whose frame_count table entry is 0 (index 30/31)
 * runs no frames at all -> no blits, no SFX, no waits.
 */
static void test_impact_zero_frames(void)
{
    uint8 idx_array[1];

    setup_impact();

    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;
    idx_array[0] = 0;

    /* spell 30 (0x1E): frame_count table[30] = 0 */
    ASSERT_EQ(data_fd2_animation_spell_frame_count_table[30], 0);
    fd2_animate_spell_impact_per_target(0, 30, 1, (uint32)idx_array);

    ASSERT_EQ(g_blitdec_calls, 0);
    ASSERT_EQ(g_sfx_id_count, 0);
}

/* ================================================================
 * fd2_animate_spell_full_screen_flash tests
 * ================================================================ */

/* fd2_composite_chars_with_spell_effect_overlay is now a real emitted function
 * (src/gfx/rndscene.c). The full-screen-flash caller drives it twice; the two
 * invocations are observed through (a) the fd2_composite_battle_tile_map dst log
 * (the overlay's first action -> reveals each call's dst buffer) and (b) the
 * fd2_blit_sprite_with_decoded_pixels log (the overlay's hit-branch blit ->
 * reveals the resolved fx-sprite addr, hence the variant index). */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;
extern int    g_composite_call_count;
extern int    g_tile_map_log_on;
extern int    g_tile_map_log_count;
extern uint32 g_tile_map_log_dst[16];
extern int    g_blitdec_log_on;
extern int    g_blitdec_log_count;
extern uint32 g_blitdec_log_dst[16];
extern uint32 g_blitdec_log_sprite[16];

/* Effect-sprite sheet for the real overlay (data_fd2_resource_portrait_sheet_ptr):
 * dword table at +6, identity (table[i]==i) so fx_sprite_addr == sheet+fx_idx and
 * the recorded blit sprite reveals the variant index. */
static uint8 g_flash_sheet[6 + 0x80 * 4];
/* Transparent tile map so the finalizer's real shadow-overlay tile redraws read
 * the meta + attr and return without a blit (attr 0x80 clear), keeping the
 * closing fd2_composite_battle_frame(0) host-safe and side-effect-free. */
#define FLASH_MAP_W 0x20
static uint8 g_flash_tile_map[FLASH_MAP_W * FLASH_MAP_W * 4];
static uint8 g_flash_attr_buf[64];

static void setup_fullflash(void)
{
    int i;
    uint32 *sheet_tbl;

    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    g_composite_call_count = 0;
    g_tile_map_log_on = 0;
    g_tile_map_log_count = 0;
    g_blitdec_log_on = 0;
    g_blitdec_log_count = 0;
    g_blitdec_calls = 0;
    g_blitpass_calls = 0;

    /* the real fd2_composite_battle_frame(0) finalizer + the real
     * fd2_blit_rectangle strobe both read +0x8088 out of this buffer */
    memset(g_lgs, 0, sizeof(g_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgs;

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;

    /* portrait cache (miss branch / finalizer per-char paint) */
    setup_overlay(0);   /* installs g_portrait_cache, clears g_test_rc_array */

    /* effect-sprite sheet (hit branch) */
    sheet_tbl = (uint32 *)(g_flash_sheet + 6);
    for (i = 0; i < 0x80; i++) {
        sheet_tbl[i] = (uint32)i;
    }
    data_fd2_resource_portrait_sheet_ptr = (uint32)g_flash_sheet;

    /* transparent tile map for the finalizer shadow overlay */
    memset(g_flash_tile_map, 0, sizeof(g_flash_tile_map));
    memset(g_flash_attr_buf, 0, sizeof(g_flash_attr_buf));   /* attr 0x80 clear */
    data_fd2_battle_tile_map_ptr = (uint32)g_flash_tile_map;
    data_fd2_battle_map_width_tiles = FLASH_MAP_W;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_flash_attr_buf;
    data_fd2_graphics_bg_anim_flip_flag = 0;

    /* finalizer sub-stage gating: HUD off (early return), cursor phase off
     * (default branch, no overlay blit), palette throttled to no-op */
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;
    data_fd2_battle_anim_phase = 0;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;
}

/* expected char_screen_addr for the targeted char at (px,py) into dst_buf */
static uint32 flash_screen_rel(int32 px, int32 py)
{
    return 0x75d8u + (uint32)(py - (int32)WIN_OY) * 0x2ac0u +
           (uint32)(px - (int32)WIN_OX) * 0x18u;
}

/*
 * Drives the whole flash pipeline and checks the two-composite + strobe
 * structure, now against the REAL spell-effect overlay. One alive, in-window,
 * targeted char (idx 0) makes each overlay call land exactly one hit-branch
 * effect blit, so the decoded-pixels log captures both variants in order; the
 * tile-map dst log captures each overlay's compose target plus the finalizer's.
 * Verifies variant ordering (0x4A then 0x4B), dst routing (live back-buffer then
 * a distinct malloc'd buffer), that the forwarded target args reach the overlay
 * (the char is hit, not portrait-painted), the 8-delay strobe, and the single
 * closing composite_battle_frame(0).
 */
static void test_fullflash_two_composites_and_strobe(void)
{
    uint8 idx_array[3];
    uint32 rel;
    uint32 bufB;

    setup_fullflash();

    /* one alive, in-window char at idx 0; idx 0 is in the target list */
    data_fd2_battle_party_member_count = 1;
    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;
    g_test_rc_array[0].sprite_state[0] = 0;
    g_test_rc_array[0].flags = 0;        /* alive */
    idx_array[0] = 0;
    idx_array[1] = 5;
    idx_array[2] = 1;

    g_tile_map_log_on = 1;
    g_blitdec_log_on = 1;

    /* param_1 and spell_id are body-unused; pass sentinels */
    fd2_animate_spell_full_screen_flash(0xDEAD, 0xBEEF, 3, (uint32)idx_array);

    /* three tile-map composites: overlay A, overlay B, then the finalizer */
    ASSERT_EQ(g_tile_map_log_count, 3);
    /* overlay A composes into the live back-buffer (+0x8088) */
    ASSERT_EQ(g_tile_map_log_dst[0], (uint32)g_lgs + 0x8088u);
    /* overlay B composes into a freshly malloc'd buffer (distinct, non-null) */
    bufB = g_tile_map_log_dst[1] - 0x8088u;
    ASSERT_TRUE(bufB != 0);
    ASSERT_TRUE(bufB != (uint32)g_lgs);
    /* the closing finalizer composes back into the live back-buffer */
    ASSERT_EQ(g_tile_map_log_dst[2], (uint32)g_lgs + 0x8088u);

    /* exactly two hit-branch effect blits: variant A (0x4A) then B (0x4B).
     * identity sheet table -> sprite == sheet + fx_idx */
    ASSERT_EQ(g_blitdec_log_count, 2);
    ASSERT_EQ(g_blitdec_log_sprite[0], (uint32)g_flash_sheet + 0x4au);
    ASSERT_EQ(g_blitdec_log_sprite[1], (uint32)g_flash_sheet + 0x4bu);
    /* the forwarded targets reached the overlay: the targeted char was hit in
     * both buffers (dst == buffer base + screen-relative offset) */
    rel = flash_screen_rel(0x15, 0x24);
    ASSERT_EQ(g_blitdec_log_dst[0], (uint32)g_lgs + rel);
    ASSERT_EQ(g_blitdec_log_dst[1], bufB + rel);

    /* strobe = 4 iterations x 2 delays = 8 delays, each of 0x5A ticks */
    ASSERT_EQ(g_delay375b2_calls, 8);
    ASSERT_EQ(g_delay375b2_last_ticks, 0x5au);

    /* the closing fd2_composite_battle_frame(0) ran exactly once: total
     * composites = 2 overlay tile-maps + 1 finalizer tile-map = 3 */
    ASSERT_EQ(g_composite_call_count, 3);
}

/* ================================================================
 * fd2_animate_spell_overlay_blink tests
 * ================================================================ */

/* tint-blit recording (testglob.c): shared g_blitpass_* (src/dst/stride) plus a
 * dedicated colour_base / team_offset log and a call counter. */
extern int    g_blittint_calls;
extern uint32 g_blittint_color_base[64];
extern uint32 g_blittint_team_offset[64];

/* per-spell tint-mask byte table (testglob.c, real binary bytes) */
extern uint8  data_fd2_animation_spell_overlay_blink_mask_table[30];

/*
 * Drives the full 10-frame blink over one in-window char and one out-of-window
 * char. Verifies: the window-cull predicate (only the in-window char blits, so
 * 10 tint blits over 10 frames), the dst screen-position arithmetic, the
 * frame-source arithmetic on the palette!=3 branch, the constant colour_base
 * (mask_tbl[spell_id]), the fixed 0x1C8 stride, and the distinctive per-frame
 * fade-step team_offset sequence 7..0 then wrapping (7 - frame%8).
 */
static void test_blink_cull_arith_and_fade(void)
{
    uint8 idx_array[2];
    uint32 exp_dst;
    uint32 frame_idx;
    uint32 exp_src;
    uint32 exp_color;
    int f;
    static const int exp_team[10] = { 7, 6, 5, 4, 3, 2, 1, 0, 7, 6 };

    setup_overlay(1);   /* palette 1 -> frame_idx = frame_off + palette branch */

    /* char 0: inside the window */
    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;
    g_test_rc_array[0].sprite_state[0] = 4;   /* frame_off = 4*0xC = 0x30 */

    /* char 1: pos_x past the right edge (OX+MX = 0x1D) -> culled */
    g_test_rc_array[1].pos_x = 0x1E;
    g_test_rc_array[1].pos_y = 0x24;
    g_test_rc_array[1].sprite_state[0] = 7;

    idx_array[0] = 0;
    idx_array[1] = 1;

    g_blittint_calls = 0;

    /* spell_id 8 -> mask_tbl[8] = 0xC8 (the one outlier byte in the table) */
    fd2_animate_spell_overlay_blink(0xDEAD, 8, 2, (uint32)idx_array);

    /* one tint blit per frame (in-window char only), 10 frames */
    ASSERT_EQ(g_blittint_calls, 10);
    ASSERT_EQ(g_blitpass_calls, 10);

    /* dst = lgs + (pos_y-OY)*0x2AC0 + (pos_x-OX)*0x18 + 0x75D8 (frame-invariant) */
    exp_dst = (uint32)g_lgs
            + (0x24u - WIN_OY) * 0x2ac0u
            + (0x15u - WIN_OX) * 0x18u
            + 0x75d8u;

    /* frame_idx = sprite_state[0]*0xC + palette(=1); src = cache + table[frame_idx] */
    frame_idx = 4u * 0xcu + 1u;                 /* 0x31 */
    exp_src = (uint32)g_portrait_cache + frame_idx * 0x100u;

    /* colour_base = mask_tbl[spell_id]; spell 8 -> 0xC8 */
    exp_color = data_fd2_animation_spell_overlay_blink_mask_table[8];
    ASSERT_EQ(exp_color, 0xc8u);

    for (f = 0; f < 10; f++) {
        ASSERT_EQ(g_blitpass_dst[f], exp_dst);
        ASSERT_EQ(g_blitpass_src[f], exp_src);
        ASSERT_EQ(g_blitpass_stride[f], 0x1c8u);
        ASSERT_EQ(g_blittint_color_base[f], exp_color);
        ASSERT_EQ(g_blittint_team_offset[f], (uint32)exp_team[f]);
    }
}

/*
 * palette_idx == 3 forces frame_idx = frame_off + 2 (clash-avoidance branch),
 * independent of the palette value. One char on the window origin, single frame
 * source checked (frame-invariant), confirming the special-case offset and the
 * origin dst collapse.
 */
static void test_blink_palette3_offset(void)
{
    uint8 idx_array[1];
    uint32 frame_idx;
    uint32 exp_src;

    setup_overlay(3);

    g_test_rc_array[0].pos_x = WIN_OX;          /* on the left window edge */
    g_test_rc_array[0].pos_y = WIN_OY;          /* on the top window edge  */
    g_test_rc_array[0].sprite_state[0] = 2;     /* frame_off = 2*0xC = 0x18 */
    idx_array[0] = 0;

    g_blittint_calls = 0;
    fd2_animate_spell_overlay_blink(0, 0, 1, (uint32)idx_array);

    /* 10 frames, one in-window char -> 10 blits */
    ASSERT_EQ(g_blittint_calls, 10);

    /* palette==3 -> frame_idx = frame_off + 2 = 0x18 + 2 = 0x1A */
    frame_idx = 2u * 0xcu + 2u;
    exp_src = (uint32)g_portrait_cache + frame_idx * 0x100u;
    ASSERT_EQ(g_blitpass_src[0], exp_src);

    /* dst at the window origin: offsets collapse to the +0x75D8 base */
    ASSERT_EQ(g_blitpass_dst[0], (uint32)g_lgs + 0x75d8u);

    /* spell 0 -> mask_tbl[0] = 0x20 */
    ASSERT_EQ(g_blittint_color_base[0], 0x20u);
}

/*
 * Lower-edge culling: a char one row above the top window edge (pos_y = OY-2,
 * below the OY-1 lower bound) is rejected, so zero tint blits are drawn while
 * the 10-frame snapshot/restore/composite plumbing still runs to completion.
 */
static void test_blink_cull_top_edge(void)
{
    uint8 idx_array[1];

    setup_overlay(0);

    g_test_rc_array[0].pos_x = WIN_OX;
    g_test_rc_array[0].pos_y = (uint8)(WIN_OY - 2);   /* below OY-1 -> culled */
    g_test_rc_array[0].sprite_state[0] = 1;
    idx_array[0] = 0;

    g_blittint_calls = 0;
    fd2_animate_spell_overlay_blink(0, 0, 1, (uint32)idx_array);

    ASSERT_EQ(g_blittint_calls, 0);
}

/* ================================================================
 * fd2_play_death_animation_and_mark_dead tests
 * ================================================================
 *
 * Coverage is risk-oriented. The two high-value, host-cheap behaviours are
 * exercised here through the n_dying_onscreen == 0 EARLY-EXIT branch:
 *   (1) the Phase-1 window-cull predicate (the control flow that decides
 *       whether a dying char is collected), driven across all four boundary
 *       rejections plus the flags-bit0 and hp>0 gates; and
 *   (2) the silent-off-screen mark-dead state transition (flags := 1 on every
 *       hp_current==0 char, hp>0 chars untouched).
 * Taking the early-exit branch is observable by the absence of any animation
 * side effect (g_composite_call_count and the death SFX both stay 0).
 *
 * The full on-screen animation path (Phase 2 13-frame flicker + Phase 3
 * 12-frame decay) is a pure blit/display side-effect sequence: it composites
 * the battle frame, blits the back-buffer to the mode13h primary, strobes the
 * death sprite, and calls fd2_wait_n_bios_ticks(1) ~25 times (each a real
 * ~55ms BIOS-tick spin). Per the project's risk-oriented test policy, that
 * display-only path (and its dst screen-position routing, which only feeds the
 * blit destination) is deferred to Phase 9 integration; it carries no numeric
 * result, RNG, or persisted state beyond the mark-dead flag already covered
 * by the early-exit tests, and running it here would add no logic coverage at
 * a multi-second wall-clock cost. */

static void setup_death(void)
{
    g_composite_call_count = 0;
    g_play_sfx_with_handle_calls = 0;
    g_blitdec_calls = 0;
    g_blitpass_calls = 0;

    memset(g_lgs, 0, sizeof(g_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgs;

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
}

/*
 * Off-screen silent death: several hp==0 chars all positioned outside the
 * window are never collected (n_dying stays 0), so the early-exit branch runs.
 * It must set flags := 1 on every hp==0 char and leave hp>0 chars untouched,
 * with zero animation (no composite, no SFX).
 */
static void test_death_offscreen_marks_all_hp0_dead(void)
{
    setup_death();
    data_fd2_battle_party_member_count = 5;

    /* idx0: hp==0, far off-screen (left of OX-1) -> not collected, mark dead */
    g_test_rc_array[0].pos_x = 0;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].hp_current = 0;
    g_test_rc_array[0].flags = 0;

    /* idx1: hp>0, inside window -> never a death candidate, must stay alive */
    g_test_rc_array[1].pos_x = 0x15;
    g_test_rc_array[1].pos_y = 0x24;
    g_test_rc_array[1].hp_current = 30;
    g_test_rc_array[1].flags = 0;

    /* idx2: hp==0 but already-dead (flags bit0 set), off-screen; mark loop in
       the early-exit path keys only on hp==0, so flags stays 1 (idempotent) */
    g_test_rc_array[2].pos_x = 0;
    g_test_rc_array[2].pos_y = 0;
    g_test_rc_array[2].hp_current = 0;
    g_test_rc_array[2].flags = 1;

    /* idx3: hp==0, off-screen (below OY+MY+1) -> mark dead */
    g_test_rc_array[3].pos_x = 0x15;
    g_test_rc_array[3].pos_y = 0x7f;
    g_test_rc_array[3].hp_current = 0;
    g_test_rc_array[3].flags = 4;       /* unrelated bit preserved? see assert */

    /* idx4: hp>0, off-screen -> untouched */
    g_test_rc_array[4].pos_x = 0;
    g_test_rc_array[4].pos_y = 0;
    g_test_rc_array[4].hp_current = 10;
    g_test_rc_array[4].flags = 0;

    fd2_play_death_animation_and_mark_dead();

    /* early-exit branch: no animation ran */
    ASSERT_EQ(g_composite_call_count, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_blitdec_calls, 0);

    /* every hp==0 char is now flags == 1 (the store is an assignment, so idx3's
       prior bit2 is overwritten, matching MOV byte ptr [flags],1) */
    ASSERT_EQ(g_test_rc_array[0].flags, 1);
    ASSERT_EQ(g_test_rc_array[2].flags, 1);
    ASSERT_EQ(g_test_rc_array[3].flags, 1);

    /* hp>0 chars are left exactly as they were */
    ASSERT_EQ(g_test_rc_array[1].flags, 0);
    ASSERT_EQ(g_test_rc_array[1].hp_current, 30);
    ASSERT_EQ(g_test_rc_array[4].flags, 0);
    ASSERT_EQ(g_test_rc_array[4].hp_current, 10);
}

/*
 * Window-cull boundary rejections. Each dying (hp==0, alive) char sits one tile
 * outside one of the four window edges, so none is collected and the early-exit
 * branch runs (no animation). Also covers the two non-position gates: an
 * already-dead char (flags bit0) and an hp>0 char that happen to be inside the
 * window are likewise never collected. The just-inside extreme corners are NOT
 * placed here (they would enter the deferred animation path); their accept side
 * is covered structurally by the identical predicate in the sibling overlays.
 */
static void test_death_cull_boundary_rejections(void)
{
    setup_death();
    data_fd2_battle_party_member_count = 6;

    /* idx0: pos_x = OX-2  (below the OX-1 left bound) */
    g_test_rc_array[0].pos_x = (uint8)(WIN_OX - 2);
    g_test_rc_array[0].pos_y = WIN_OY;
    g_test_rc_array[0].hp_current = 0;

    /* idx1: pos_x = OX+MX+1 (above the OX+MX right bound) */
    g_test_rc_array[1].pos_x = (uint8)(WIN_OX + WIN_MX + 1);
    g_test_rc_array[1].pos_y = WIN_OY;
    g_test_rc_array[1].hp_current = 0;

    /* idx2: pos_y = OY-2  (below the OY-1 top bound) */
    g_test_rc_array[2].pos_x = WIN_OX;
    g_test_rc_array[2].pos_y = (uint8)(WIN_OY - 2);
    g_test_rc_array[2].hp_current = 0;

    /* idx3: pos_y = OY+MY+2 (above the OY+MY+1 bottom bound) */
    g_test_rc_array[3].pos_x = WIN_OX;
    g_test_rc_array[3].pos_y = (uint8)(WIN_OY + WIN_MY + 2);
    g_test_rc_array[3].hp_current = 0;

    /* idx4: inside the window, hp==0, but already-dead (flags bit0) -> gated */
    g_test_rc_array[4].pos_x = 0x15;
    g_test_rc_array[4].pos_y = 0x24;
    g_test_rc_array[4].hp_current = 0;
    g_test_rc_array[4].flags = 1;

    /* idx5: inside the window, alive, hp>0 -> not a death candidate */
    g_test_rc_array[5].pos_x = 0x15;
    g_test_rc_array[5].pos_y = 0x24;
    g_test_rc_array[5].hp_current = 7;
    g_test_rc_array[5].flags = 0;

    fd2_play_death_animation_and_mark_dead();

    /* nothing collected -> early-exit, no animation side effects */
    ASSERT_EQ(g_composite_call_count, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);

    /* the four boundary-rejected hp==0 chars are still marked dead by the
       early-exit mark loop (it ignores position) */
    ASSERT_EQ(g_test_rc_array[0].flags, 1);
    ASSERT_EQ(g_test_rc_array[1].flags, 1);
    ASSERT_EQ(g_test_rc_array[2].flags, 1);
    ASSERT_EQ(g_test_rc_array[3].flags, 1);
    ASSERT_EQ(g_test_rc_array[4].flags, 1);   /* hp==0 -> set (was already 1) */
    ASSERT_EQ(g_test_rc_array[5].flags, 0);   /* hp>0 -> untouched */
}

/*
 * Empty party guard: party_member_count == 0 collects nothing, takes the
 * early-exit branch, and marks nothing (both loops iterate zero times).
 */
static void test_death_empty_party(void)
{
    setup_death();
    data_fd2_battle_party_member_count = 0;

    /* seed a stale hp==0 slot that must NOT be touched (out of party range) */
    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;
    g_test_rc_array[0].hp_current = 0;
    g_test_rc_array[0].flags = 0;

    fd2_play_death_animation_and_mark_dead();

    ASSERT_EQ(g_composite_call_count, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_test_rc_array[0].flags, 0);   /* outside party count -> untouched */
}

/* ================================================================
 * fd2_animate_spell_projectile_paths tests
 * ================================================================ */

/* decoded-pixel sprite blit opt-in per-call log (testglob.c) */
extern int    g_blitdec_log_on;
extern int    g_blitdec_log_count;
extern uint32 g_blitdec_log_dst[16];
extern uint32 g_blitdec_log_sprite[16];

/* delay thunk recording (testglob.c) */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;

/* floating-damage FX queue tables (testglob.c, BSS) */
extern uint8  data_fd2_battle_floating_damage_sprite_id_queue[200];
extern uint8  data_fd2_battle_floating_damage_x_offset_queue[200];
extern uint8  data_fd2_battle_floating_damage_target_char_idx_queue[200];
/* projectile y-offset table (testglob.c, real binary bytes) */
extern uint8  data_fd2_animation_spell_projectile_y_offset_table[28];

/* Effect sprite sheet for the projectile blit: a dword table at +6 indexed by
 * sprite_id; entry[i] == i*0x10 so the resolved sprite addr (sheet + table[6 +
 * sprite_id*4]) uniquely identifies the sprite_id. */
static uint8 g_proj_sheet[6 + 256 * 4];

static void setup_projectile(void)
{
    int i;
    uint32 *tbl;

    g_blitdec_calls = 0;
    g_blitdec_dst = 0;
    g_blitdec_sprite = 0;
    g_blitdec_stride = 0;
    g_blitdec_log_on = 0;
    g_blitdec_log_count = 0;
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;

    /* the real per-frame restore memmoves 0x25680 out of the snapshot back into
     * this buffer, and the real fd2_blit_rectangle reads from +0x8088 */
    memset(g_lgs, 0, sizeof(g_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgs;

    tbl = (uint32 *)(g_proj_sheet + 6);
    for (i = 0; i < 256; i++) {
        tbl[i] = (uint32)i * 0x10u;
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_proj_sheet;

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_floating_damage_sprite_id_queue, 0, 200);
    memset(data_fd2_battle_floating_damage_x_offset_queue, 0, 200);
    memset(data_fd2_battle_floating_damage_target_char_idx_queue, 0, 200);
}

/*
 * Gate: a zero FX queue count takes the immediate-return path before any
 * malloc / snapshot / frame loop runs, so no blit and no delay happen.
 */
static void test_projectile_zero_queue_gate(void)
{
    setup_projectile();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;

    /* seed a stale slot that must NOT be drawn (count gates it out) */
    data_fd2_battle_floating_damage_sprite_id_queue[0] = 5;
    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;

    fd2_animate_spell_projectile_paths();

    ASSERT_EQ(g_blitdec_calls, 0);
    ASSERT_EQ(g_delay375b2_calls, 0);
}

/*
 * Full 22-frame flight with one active FX slot. Verifies:
 *  - one blit per frame -> 22 blits total (sprite_id != 0 every frame);
 *  - the sprite-source arithmetic sheet + sheet[6 + sprite_id*4] (frame-
 *    invariant);
 *  - the per-frame dst arithmetic, including the distinctive y-offset table
 *    progression y_offset_table[fx%4 + frame] across the first 16 frames
 *    (the descending-then-rising rise pattern), the -3 bias, and the
 *    0x2AC0 / 0x18 / 0x1C8 strides plus the +x_offset and +0x8088 base;
 *  - the per-frame delay(2) cadence plus the closing delay(500).
 */
static void test_projectile_full_flight_arithmetic(void)
{
    uint32 sprite_id;
    uint32 x_off;
    uint32 px;
    uint32 py;
    uint32 base;
    uint32 exp_sprite;
    int f;

    setup_projectile();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 1;

    sprite_id = 0x5e;          /* '^' damage-number marker base */
    x_off = 7;
    px = 0x15;
    py = 0x24;

    data_fd2_battle_floating_damage_sprite_id_queue[0] = (uint8)sprite_id;
    data_fd2_battle_floating_damage_x_offset_queue[0] = (uint8)x_off;
    data_fd2_battle_floating_damage_target_char_idx_queue[0] = 0;
    g_test_rc_array[0].pos_x = (uint8)px;
    g_test_rc_array[0].pos_y = (uint8)py;

    g_blitdec_log_on = 1;
    fd2_animate_spell_projectile_paths();

    /* one blit per frame, all 22 frames (slot active every frame) */
    ASSERT_EQ(g_blitdec_calls, 22);
    /* the opt-in log caps at 16 -> first 16 frames captured */
    ASSERT_EQ(g_blitdec_log_count, 16);

    /* frame-invariant sprite source: sheet + table[6 + sprite_id*4],
     * table[k] == k*0x10 with k = sprite_id */
    exp_sprite = (uint32)g_proj_sheet + sprite_id * 0x10u;

    /* dst base shared by every frame (everything except the y-offset term) */
    base = (uint32)g_lgs + 0x8088u
         + (py - WIN_OY) * 0x2ac0u
         + (px - WIN_OX) * 0x18u
         + x_off;

    for (f = 0; f < 16; f++) {
        /* fx_iter == 0 so the table index is just the frame number */
        int yval = (int)data_fd2_animation_spell_projectile_y_offset_table[f];
        uint32 exp_dst = base + (uint32)((yval - 3) * 0x1c8);
        ASSERT_EQ(g_blitdec_log_dst[f], exp_dst);
        ASSERT_EQ(g_blitdec_log_sprite[f], exp_sprite);
    }

    /* the last recorded (frame 21) blit confirms the loop ran to completion:
     * index = 0%4 + 21 = 21 -> y_offset_table[21] = 0x0F */
    {
        int yval21 = (int)data_fd2_animation_spell_projectile_y_offset_table[21];
        uint32 exp_dst21 = base + (uint32)((yval21 - 3) * 0x1c8);
        ASSERT_EQ(g_blitdec_dst, exp_dst21);
        ASSERT_EQ(g_blitdec_sprite, exp_sprite);
        ASSERT_EQ(g_blitdec_stride, 0x1c8u);
    }

    /* 22 per-frame delays of 2 ticks + one closing 500-tick settle */
    ASSERT_EQ(g_delay375b2_calls, 23);
    ASSERT_EQ(g_delay375b2_last_ticks, 500u);
}

/*
 * A queued slot whose sprite_id is 0 (a blank damage digit) is skipped every
 * frame, so it contributes no blits, while an active slot beside it still
 * draws. Confirms the per-slot sprite_id==0 continue and that the frame loop
 * still runs its full 22 passes (and closing delay) around the skip.
 */
static void test_projectile_zero_sprite_id_skip(void)
{
    setup_projectile();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 3;

    /* slot 0: blank (sprite_id 0) -> skipped */
    data_fd2_battle_floating_damage_sprite_id_queue[0] = 0;
    data_fd2_battle_floating_damage_target_char_idx_queue[0] = 0;
    /* slot 1: active */
    data_fd2_battle_floating_damage_sprite_id_queue[1] = 0x60;
    data_fd2_battle_floating_damage_x_offset_queue[1] = 2;
    data_fd2_battle_floating_damage_target_char_idx_queue[1] = 0;
    /* slot 2: blank (sprite_id 0) -> skipped */
    data_fd2_battle_floating_damage_sprite_id_queue[2] = 0;
    data_fd2_battle_floating_damage_target_char_idx_queue[2] = 0;

    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;

    fd2_animate_spell_projectile_paths();

    /* only slot 1 draws: exactly one blit per frame -> 22 over 22 frames */
    ASSERT_EQ(g_blitdec_calls, 22);
    /* last blit resolved from slot 1's sprite_id (0x60): sheet + 0x60*0x10 */
    ASSERT_EQ(g_blitdec_sprite, (uint32)g_proj_sheet + 0x60u * 0x10u);
    /* the loop still completed: closing 500-tick delay fired */
    ASSERT_EQ(g_delay375b2_last_ticks, 500u);
    ASSERT_EQ(g_delay375b2_calls, 23);
}

void run_anim_anicombt_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anicombt\n");
    RUN_TEST(test_overlay_cull_and_arithmetic);
    RUN_TEST(test_overlay_palette3_offset);
    RUN_TEST(test_overlay_cull_top_edge);
    RUN_TEST(test_impact_sfx_dispatch_sequences);
    RUN_TEST(test_impact_cull_and_arithmetic);
    RUN_TEST(test_impact_zero_frames);
    RUN_TEST(test_fullflash_two_composites_and_strobe);
    RUN_TEST(test_blink_cull_arith_and_fade);
    RUN_TEST(test_blink_palette3_offset);
    RUN_TEST(test_blink_cull_top_edge);
    RUN_TEST(test_death_offscreen_marks_all_hp0_dead);
    RUN_TEST(test_death_cull_boundary_rejections);
    RUN_TEST(test_death_empty_party);
    RUN_TEST(test_projectile_zero_queue_gate);
    RUN_TEST(test_projectile_full_flight_arithmetic);
    RUN_TEST(test_projectile_zero_sprite_id_skip);
    printf("\n");
}
