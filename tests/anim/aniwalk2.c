/*
 * unit tests for src/anim/aniwalk.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

#define USE_ITEM_ID 10

extern runtime_char g_test_rc_array[8];
extern int g_build_spell_list_return;
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;
extern uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
extern int g_ending_menu_return;
extern int g_slot_selector_return;
extern int g_chapter_transition_return;
extern int g_play_sfx_with_handle_calls;
extern int g_play_sfx_sample_from_bank_calls;
extern int g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int g_blit_indexed_sprite_last_x;
extern int g_blit_indexed_sprite_last_y;
extern int g_find_equipped_return;
extern int g_composite_call_count;
extern int g_attack_dispatch_return;
extern int g_attack_dispatch_calls;
extern int g_seek_optimal_return;
extern int g_advance_nearest_return;
extern int g_walk_return;
extern int g_score_physical_return;
extern int g_pass_turn_calls;
extern int g_execute_spell_calls;
extern int g_execute_physical_calls;
extern int g_pathfind_return;
extern int g_pathfind_walk_return;
extern int g_pathfind_write_dst;
extern int g_pathfind_dst_x;
extern int g_pathfind_dst_y;
extern int g_pathfind_seq_enable;
extern int g_pathfind_seq[4];
extern int g_pathfind_seq_idx;
extern int g_pathfind_seq_steps;
extern uint8 g_pathfind_step_bytes[8];
extern int g_pathfind_md0_dst_x;
extern int g_pathfind_md0_dst_y;
extern int g_count_usable_slots_return;
extern uint8 g_spell_list_buf[12];
extern int g_remove_inventory_calls;
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;


/* fd2_slide_panel_step_left_main copies 0x75 rows of `row_bytes` from
 * src_buffer (offset src_x+0x2E40) into large_game_state_buffer
 * (offset dst_x+0x2E40), stride 0x140. row_bytes/src_x/dst_x are computed
 * from frame_idx by the verified prologue (0001af30-0001af5d):
 *   frame>=5 (stationary): row_bytes=0xAA, src_x=0x4B, dst_x=0x4B
 *   frame<5: x_shift=(4-frame)*0x32; dst_x=0x4B-x_shift; if dst_x<0 then
 *            row_bytes=0xAA+dst_x, src_x=0x4B-dst_x, dst_x=0 (full left clip).
 * Tests below pin all three derived quantities at once: src is poisoned with
 * 0xFE outside the expected read window and 0xBB inside it, so a wrong src_x
 * or row_bytes leaks 0xFE into dst; dst is pre-filled 0x11 so a wrong dst_x or
 * row_bytes leaves 0x11 where 0xBB is expected (or writes 0xBB where 0x11 is).
 * Buffers are 64000 (>= max touched offset 49205). */
static uint8 g_lp_dst[64000];

static uint8 g_lp_src[64000];


/* fd2_slide_panel_step_bottom_main @0x1B0AD copies a 0xAA-wide band from a
 * FIXED source window (src_buffer + 0xC20B, never frame-adjusted — asm 0001b112
 * ADD EDI,0xc20b) down into large_game_state_buffer at x-offset 0x4B, stride
 * 0x140. dst_y/row_count are computed from frame_idx by the verified prologue
 * (0001b0c3-0001b108):
 *   frame_idx < 5            -> early return, no write at all.
 *   frame_idx > 9 (>=10)     -> stationary: dst_y=0x9B, row_count=0x10.
 *   frame_idx 5..9           -> y_shift=(4-(frame_idx-5))*9; dst_y=y_shift+0x9B;
 *                               if (dst_y+0x10 > 200) BOTTOM clip:
 *                               row_count = dst_y - 200  (asm 0001b102
 *                               LEA EBP,[EAX-200], EAX=dst_y; signed loop
 *                               CMP EBX,EBP / JL @0001b142,0001b144).
 * Unlike left/right_main the clip is VERTICAL and SIGNED: only frame 5 reaches
 * it (dst_y=191, 191+0x10=207>200) and yields row_count = 191-200 = -9, a
 * NEGATIVE count -> the signed loop copies ZERO rows. This is the vendor's
 * genuine behavior on the clipped frame, not a bug to "correct" to positive.
 *
 * Derived per-frame values (exact integer arithmetic straight from the
 * SUB/SUB/SHL/ADD/LEA/CMP/LEA sequence; emulate_function cannot derive them
 * because the function's __CHK stack-probe prologue (CALL 00036cd7) hits an
 * "Unimplemented CALLOTHER pcodeop (LOCK)" in the Ghidra emulator):
 *   frame 4 : early return (frame_idx < 5).
 *   frame 5 : y_shift=36 -> dst_y=191(0xBF); 207>200 CLIP -> row_count=-9 -> 0 rows.
 *   frame 6 : y_shift=27 -> dst_y=182(0xB6); 198<=200 no clip -> row_count=0x10.
 *   frame 10: stationary -> dst_y=0x9B(155); row_count=0x10.
 * The shared 64000-byte g_lp_dst/g_lp_src cover the max touched offsets
 * (dst (182+15)*0x140+0x4B+0xAA = 40565; src 0xC20B+16*0x140+0xAA = 54965).
 */

/* Assert one fully-written dst row: the 0xAA-wide span at column 0x4B is all
 * 0xBB, flanked by 0x11 background. A wrong source window leaks the 0xFE
 * row-guard (planted one row above/below the FIXED read window) into the span. */
static void bp_check_dst_row(long dst_row)
{
    long d_base;
    long i;

    d_base = dst_row * 0x140 + 0x4B;

    /* copied span: every byte is 0xBB (catches wrong src window via 0xFE leak). */
    for (i = 0; i < 0xAA; i++) {
        ASSERT_EQ((long)g_lp_dst[d_base + i], 0xBB);
    }
    /* byte just before the dst span: still background (pins x-offset 0x4B). */
    ASSERT_EQ((long)g_lp_dst[d_base - 1], 0x11);
    /* byte just after the dst span: still background (pins width 0xAA). */
    ASSERT_EQ((long)g_lp_dst[d_base + 0xAA], 0x11);
}


/* Plant 0xFE across the 0xAA-wide src band on the rows immediately ABOVE the
 * first read row (src_buffer+0xC20B) and BELOW the last read row, so reading
 * one row too high/low (wrong src start or row_count) leaks 0xFE into the dst
 * band. The src start is FIXED, so this depends only on row_count. */
static void bp_poison_src_vguards(long row_count)
{
    long i;
    long above;
    long below;

    above = 0xC20B - 0x140;
    below = 0xC20B + row_count * 0x140;
    for (i = 0; i < 0xAA; i++) {
        g_lp_src[above + i] = 0xFE;
        g_lp_src[below + i] = 0xFE;
    }
}


/* fd2_slide_panel_step_bottom_small @0x1B14B copies a 0x3F-wide band from a
 * FIXED source window (src_buffer + 0xD781, never frame-adjusted — asm 0001b1ae
 * ADD EDI,0xd781) down into large_game_state_buffer at x-offset 0x81, stride
 * 0x140. dst_y/row_count are computed from frame_idx by the verified prologue
 * (0001b161-0001b1a4):
 *   frame_idx < 8            -> early return, no write at all.
 *   frame_idx > 0xC (>=0xD)  -> stationary: dst_y=0xAC, row_count=0xF.
 *   frame_idx 8..0xC         -> y_shift=(4-(frame_idx-8))*4; dst_y=y_shift+0xAC;
 *                               if (dst_y+0xF > 200) BOTTOM clip:
 *                               row_count = dst_y - 200  (asm 0001b19e
 *                               LEA EBP,[EAX-200], EAX=dst_y; signed loop
 *                               CMP EBX,EBP / JL @0001b1de,0001b1be).
 * As in bottom_main the clip is VERTICAL and SIGNED: only frame 8 reaches it
 * (dst_y=188, 188+0xF=203>200) and yields row_count = 188-200 = -12, a NEGATIVE
 * count -> the signed loop copies ZERO rows. This is the vendor's genuine
 * behavior on the clipped frame, not a bug to "correct" to positive. The
 * baseline emit had the sign inverted here (200-dst_y = +12 would have copied
 * 12 rows); these tests pin the corrected dst_y-200.
 *
 * Derived per-frame values (exact integer arithmetic straight from the
 * SUB/SUB/SHL/ADD/LEA/CMP/LEA sequence; emulate_function cannot derive them
 * because the function's __CHK stack-probe prologue (CALL 00036cd7) hits an
 * "Unimplemented CALLOTHER pcodeop (LOCK)" in the Ghidra emulator):
 *   frame 7 : early return (frame_idx < 8).
 *   frame 8 : y_shift=16 -> dst_y=188(0xBC); 203>200 CLIP -> row_count=-12 -> 0 rows.
 *   frame 9 : y_shift=12 -> dst_y=184(0xB8); 199<=200 no clip -> row_count=0xF(15).
 *   frame 13: stationary -> dst_y=0xAC(172); row_count=0xF(15).
 * The shared 64000-byte g_lp_dst/g_lp_src cover the max touched offsets
 * (dst (184+14)*0x140+0x81+0x3E = 63551; src 0xD781+15*0x140 = 59969).
 */

/* Assert one fully-written dst row: the 0x3F-wide span at column 0x81 is all
 * 0xBB, flanked by 0x11 background. A wrong source window leaks the 0xFE
 * row-guard (planted one row above/below the FIXED read window) into the span. */
static void bps_check_dst_row(long dst_row)
{
    long d_base;
    long i;

    d_base = dst_row * 0x140 + 0x81;

    /* copied span: every byte is 0xBB (catches wrong src window via 0xFE leak). */
    for (i = 0; i < 0x3F; i++) {
        ASSERT_EQ((long)g_lp_dst[d_base + i], 0xBB);
    }
    /* byte just before the dst span: still background (pins x-offset 0x81). */
    ASSERT_EQ((long)g_lp_dst[d_base - 1], 0x11);
    /* byte just after the dst span: still background (pins width 0x3F). */
    ASSERT_EQ((long)g_lp_dst[d_base + 0x3F], 0x11);
}


/* Plant 0xFE across the 0x3F-wide src band on the rows immediately ABOVE the
 * first read row (src_buffer+0xD781) and BELOW the last read row, so reading
 * one row too high/low (wrong src start or row_count) leaks 0xFE into the dst
 * band. The src start is FIXED, so this depends only on row_count. */
static void bps_poison_src_vguards(long row_count)
{
    long i;
    long above;
    long below;

    above = 0xD781 - 0x140;
    below = 0xD781 + row_count * 0x140;
    for (i = 0; i < 0x3F; i++) {
        g_lp_src[above + i] = 0xFE;
        g_lp_src[below + i] = 0xFE;
    }
}


/* frame_idx = 10 (>9): stationary. dst_y=0x9B, row_count=0x10 (16 rows). */
static void test_slide_panel_bottom_main_stationary(void)
{
    uint32 save_buf;
    long dst_y;
    long row_count;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    dst_y = 0x9B;
    row_count = 0x10;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    bp_poison_src_vguards(row_count);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_bottom_main((uint32)g_lp_src, 10);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst row just above first written row (dst_y-1=0x9A) stays background. */
    ASSERT_EQ((long)g_lp_dst[(dst_y - 1) * 0x140 + 0x4B], 0x11);
    bp_check_dst_row(dst_y + 0);
    bp_check_dst_row(dst_y + (row_count - 1));
    ASSERT_EQ((long)g_lp_dst[(dst_y + row_count) * 0x140 + 0x4B], 0x11);
}


/* frame_idx = 7 (<8): function must early-return; dst stays fully background. */
static void test_slide_panel_bottom_small_early_return(void)
{
    uint32 save_buf;

    save_buf = data_fd2_large_game_state_buffer_ptr;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_bottom_small((uint32)g_lp_src, 7);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* nothing copied: the would-be dst row at every reachable dst_y stays 0x11. */
    ASSERT_EQ((long)g_lp_dst[0xAC * 0x140 + 0x81], 0x11);
    ASSERT_EQ((long)g_lp_dst[0xBC * 0x140 + 0x81], 0x11);
}


/* frame_idx = 8: BOTTOM clip. dst_y=188, row_count = 188-200 = -12 (signed) ->
 * the loop copies ZERO rows. This is the exact path that was emitted with the
 * wrong sign (200-dst_y = +12 would have copied 12 rows); pin it at 0 writes. */
static void test_slide_panel_bottom_small_clip_zero_rows(void)
{
    uint32 save_buf;

    save_buf = data_fd2_large_game_state_buffer_ptr;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_bottom_small((uint32)g_lp_src, 8);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst row 0 (the row that WOULD be written if row_count were +12) at
     * dst_y=188 must remain background: the negative count copies nothing. */
    ASSERT_EQ((long)g_lp_dst[188 * 0x140 + 0x81], 0x11);
    ASSERT_EQ((long)g_lp_dst[188 * 0x140 + 0x81 + 0x3E], 0x11);
    /* and row 11 (the last of the would-be 12 rows) likewise stays background. */
    ASSERT_EQ((long)g_lp_dst[(188 + 11) * 0x140 + 0x81], 0x11);
}


/* frame_idx = 9: moved but NOT clipped. dst_y=184, row_count=0xF (15 rows). */
static void test_slide_panel_bottom_small_no_clip(void)
{
    uint32 save_buf;
    long dst_y;
    long row_count;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    dst_y = 184;        /* 0xB8 */
    row_count = 0xF;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    bps_poison_src_vguards(row_count);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_bottom_small((uint32)g_lp_src, 9);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst row just ABOVE the first written row (dst_y-1) stays background:
     * proves dst_y is exactly 184, not the stationary 172. */
    ASSERT_EQ((long)g_lp_dst[(dst_y - 1) * 0x140 + 0x81], 0x11);
    bps_check_dst_row(dst_y + 0);
    bps_check_dst_row(dst_y + (row_count - 1));
    /* row just past the last written row stays background (pins row_count=0xF). */
    ASSERT_EQ((long)g_lp_dst[(dst_y + row_count) * 0x140 + 0x81], 0x11);
}


/* frame_idx = 13 (>0xC): stationary. dst_y=0xAC, row_count=0xF (15 rows). */
static void test_slide_panel_bottom_small_stationary(void)
{
    uint32 save_buf;
    long dst_y;
    long row_count;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    dst_y = 0xAC;
    row_count = 0xF;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    bps_poison_src_vguards(row_count);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_bottom_small((uint32)g_lp_src, 13);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst row just above first written row (dst_y-1=0xAB) stays background. */
    ASSERT_EQ((long)g_lp_dst[(dst_y - 1) * 0x140 + 0x81], 0x11);
    bps_check_dst_row(dst_y + 0);
    bps_check_dst_row(dst_y + (row_count - 1));
    ASSERT_EQ((long)g_lp_dst[(dst_y + row_count) * 0x140 + 0x81], 0x11);
}


static void test_walk_step_right_basic(void)
{
    uint32 save_ox;
    uint32 save_cx;
    uint32 save_sx;

    save_ox = data_fd2_battle_view_window_origin_x;
    save_cx = data_fd2_battle_cursor_world_x;
    save_sx = data_fd2_battle_cursor_screen_x;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;

    fd2_walk_step_right(0);

    ASSERT_EQ(g_test_rc_array[0].pos_x, 6);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 3);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 6);

    data_fd2_battle_view_window_origin_x = save_ox;
    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_screen_x = save_sx;
}


static void test_walk_step_up_basic(void)
{
    uint32 save_oy;
    uint32 save_cy;
    uint32 save_sy;

    save_oy = data_fd2_battle_view_window_origin_y;
    save_cy = data_fd2_battle_cursor_world_y;
    save_sy = data_fd2_battle_cursor_screen_y;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_y = 5;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;

    fd2_walk_step_up(0);

    ASSERT_EQ(g_test_rc_array[0].pos_y, 4);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 2);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 4);

    data_fd2_battle_view_window_origin_y = save_oy;
    data_fd2_battle_cursor_world_y = save_cy;
    data_fd2_battle_cursor_screen_y = save_sy;
}


static void test_walk_step_left_basic(void)
{
    uint32 save_ox;
    uint32 save_cx;
    uint32 save_sx;

    save_ox = data_fd2_battle_view_window_origin_x;
    save_cx = data_fd2_battle_cursor_world_x;
    save_sx = data_fd2_battle_cursor_screen_x;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 8;
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_cursor_world_x = 8;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;

    fd2_walk_step_left(0);

    ASSERT_EQ(g_test_rc_array[0].pos_x, 7);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 1);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 7);

    data_fd2_battle_view_window_origin_x = save_ox;
    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_screen_x = save_sx;
}


static void test_walk_step_down_basic(void)
{
    uint32 save_oy;
    uint32 save_cy;
    uint32 save_sy;

    save_oy = data_fd2_battle_view_window_origin_y;
    save_cy = data_fd2_battle_cursor_world_y;
    save_sy = data_fd2_battle_cursor_screen_y;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].sprite_state[1] = 2;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;
    data_fd2_battle_walk_anim_y_scroll_rows = 0;

    fd2_walk_step_down(0);

    ASSERT_EQ(g_test_rc_array[0].pos_y, 6);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 0);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 6);

    data_fd2_battle_view_window_origin_y = save_oy;
    data_fd2_battle_cursor_world_y = save_cy;
    data_fd2_battle_cursor_screen_y = save_sy;
}


/* ----------------------------------------------------------------
 * fd2_play_status_screen_outro_step @ 0x18409
 *
 * One frame of the 12-frame status-screen slide. These tests drive the
 * REAL function over three in-memory mode13h-sized buffers (workspace,
 * src, snapshot) and the REAL panel painters (fd2_paint_status_panel_
 * layer_left/right in src/gfx/rndstat.c and fd2_slide_panel_up_partial_
 * step above). They pin the per-frame OFFSET ARITHMETIC and BRANCH
 * SELECTION (which panel draws, and at what x/y) — the risk-bearing
 * logic of this function. The painters' own internal copy geometry is
 * covered by tests/gfx/rndstat.c.
 *
 * The function's trailing memmove(0xA0000, workspace, 64000) writes the
 * VGA aperture; under DOS/4GW 0xA0000 is real VGA RAM and the write is
 * harmless (same convention as tests/gfx/rndscene.c's real blit to
 * 0xA0504). We only read back the workspace buffer.
 *
 * Buffer fill: src[k] = k & 0xFF (so a copied dst byte reveals which
 * source index it came from); snapshot = sentinel 0xC7; workspace is
 * pre-dirtied 0x33 so a missing "reset to snapshot" memmove is visible.
 *
 * Panel footprints in workspace (recomputed independently below):
 *   left  : row 8.. (dst base 0x8C0), col x_left.. , src base 0x8C5
 *   right : row y_right.. (dst base +0x5C), col 0x5C.. , src base 0x91C
 *   middle: row y_mid.. (dst base +5), col 5.., src base 0x7585
 * x_left  = 5 (frame<6) else 5-(frame*0x10-0x60)   [<0 clamps dst-x to 0]
 * y_right = 7 (frame<=2), 7-(frame*0x10-0x30) (3..8) [<0 clamps], skip (>=9)
 * y_mid   = frame*0x10+0x5E, drawn only when frame<6
 * ---------------------------------------------------------------- */
#define OUTRO_BUF_BYTES 64000

static uint8 g_outro_ws[OUTRO_BUF_BYTES];
static uint8 g_outro_src[OUTRO_BUF_BYTES];
static uint8 g_outro_snap[OUTRO_BUF_BYTES];

/* Probe offsets that each belong to exactly ONE panel (or to none). */
#define LEFT_DST0     0x8C0u                    /* left row 8, col 0   */
#define RIGHT_ROW0    0x5Cu                     /* right col 0x5C, row 0 (only written when clamped) */
#define RIGHT_ROW7    (0x5Cu + 7u * 0x140u)     /* right col 0x5C, row 7 (in-place top row) */
#define MID_ROW5E     (5u + 0x5Eu * 0x140u)     /* middle frame-0 row 0x5E, col 5 (== 0x7585) */
#define UNTOUCHED_OFF (199u * 0x140u + 0x130u)  /* row 199: no panel ever writes here */

static void outro_setup(void)
{
    int i;

    for (i = 0; i < OUTRO_BUF_BYTES; i++) {
        g_outro_src[i] = (uint8)(i & 0xFF);
        g_outro_snap[i] = 0xC7;
        g_outro_ws[i] = 0x33;      /* dirty: proves the snapshot reset ran */
    }
}

static void outro_call(uint32 frame_idx)
{
    fd2_play_status_screen_outro_step(frame_idx, (uint32)g_outro_ws,
        (uint32)g_outro_src, (int)(uint32)g_outro_snap);
}

/* frame 0: left in-place (x=5), right in-place (y=7), middle drawn (y=0x5E). */
static void test_outro_frame0_all_inplace(void)
{
    outro_setup();
    outro_call(0);

    /* snapshot reset ran: a pixel no panel touches is the snapshot sentinel,
     * not the 0x33 we pre-dirtied. */
    ASSERT_EQ((long)g_outro_ws[UNTOUCHED_OFF], 0xC7);

    /* left edge pinned at x=5: first copied byte == src[0x8C5]; the byte just
     * left of it stays sentinel (so the panel starts exactly at col 5). */
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0 + 5], (long)(uint8)(0x8C5u & 0xFF));
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0 + 5 - 1], 0xC7);

    /* right drawn in-place at y=7: its TOP row is row 7, so row-7 col-0x5C is
     * written (== src[0x91C]) while row 0 at col 0x5C stays the sentinel (the
     * in-place panel never reaches up to row 0). */
    ASSERT_EQ((long)g_outro_ws[RIGHT_ROW7], (long)(uint8)(0x91Cu & 0xFF));
    ASSERT_EQ((long)g_outro_ws[RIGHT_ROW0], 0xC7);

    /* middle drawn at y=0x5E: row-0x5E col-5 byte == src[0x7585]. */
    ASSERT_EQ((long)g_outro_ws[MID_ROW5E], (long)(uint8)(0x7585u & 0xFF));
}

/* frame 5: left still in-place (x=5, frame<6); right slides up to a NEGATIVE
 * y (-0x19) so the painter clamps its dst base to row 0 (top row pushed off
 * the top edge); middle drawn at y=0xAE. The discriminators vs frame 0:
 * (a) the right panel's TOP row is now row 0, not row 7; (b) the middle row
 * moved to 0xAE. */
static void test_outro_frame5_right_clipped_middle_low(void)
{
    uint32 mid_off;

    outro_setup();
    outro_call(5);

    /* left unchanged: still x=5. */
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0 + 5], (long)(uint8)(0x8C5u & 0xFF));

    /* right y = 7-(5*0x10-0x30) = -0x19 < 0 -> dst clamped to row 0 with
     * src_y_skip = 0x19. Row 0 at col 0x5C is now WRITTEN (== src[0x91C +
     * 0x19*0x140]); the in-place frame leaves this same cell at the sentinel,
     * so a non-clamped emit would fail here. */
    ASSERT_EQ((long)g_outro_ws[RIGHT_ROW0],
              (long)(uint8)((0x91Cu + 0x19u * 0x140u) & 0xFF));

    /* middle drawn at y = 5*0x10+0x5E = 0xAE: first row at col 5. */
    mid_off = 5u + 0xAEu * 0x140u;
    ASSERT_EQ((long)g_outro_ws[mid_off], (long)(uint8)(0x7585u & 0xFF));
    /* and the frame-0 middle row (0x5E) is NOT where this frame draws it. */
    ASSERT_EQ((long)g_outro_ws[MID_ROW5E], 0xC7);
}

/* frame 6: left ENTERS the else branch but arithmetic yields x=5 again
 * (5-(6*0x10-0x60) = 5); right still drawn (3..8) clamped; middle NOT drawn
 * (frame not < 6). Pins the frame<6 boundary on the middle panel. */
static void test_outro_frame6_middle_off(void)
{
    outro_setup();
    outro_call(6);

    /* else-branch left still resolves to x=5. */
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0 + 5], (long)(uint8)(0x8C5u & 0xFF));
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0 + 5 - 1], 0xC7);

    /* middle skipped: the row it would occupy at frame 0 stays sentinel, and
     * the row it would occupy at frame 6 (y=6*0x10+0x5E=0xFE) is off-screen /
     * not drawn -> sentinel. */
    ASSERT_EQ((long)g_outro_ws[MID_ROW5E], 0xC7);
}

/* frame 9: left slides far left (x=-0x2B -> clamped dst-x 0); right SKIPPED
 * entirely (frame>=9); middle NOT drawn. Pins the right-panel skip branch. */
static void test_outro_frame9_right_skipped(void)
{
    outro_setup();
    outro_call(9);

    /* right skipped: row 7 col 0x5C (uniquely the right panel's in-place first
     * row; left starts at row 8, middle not drawn) stays the snapshot sentinel. */
    ASSERT_EQ((long)g_outro_ws[RIGHT_ROW7], 0xC7);

    /* left clamped to dst-x 0 with src advanced by 0x2B: first byte at col 0
     * == src[0x8C5 + 0x2B]. */
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0],
              (long)(uint8)((0x8C5u + 0x2Bu) & 0xFF));

    /* middle not drawn. */
    ASSERT_EQ((long)g_outro_ws[MID_ROW5E], 0xC7);
}

/* frame 0xB (11, last outro frame): left fully clipped (x=-0x4B -> dst-x 0,
 * src advanced 0x4B, only 0xB bytes wide); right skipped; middle not drawn.
 * This is the extreme x the function ever produces. */
static void test_outro_frame11_left_extreme_clip(void)
{
    outro_setup();
    outro_call(0xB);

    /* left first byte at col 0 == src[0x8C5 + 0x4B]; the clipped row is only
     * 0xB bytes wide so col 0xB onward stays the snapshot sentinel. */
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0],
              (long)(uint8)((0x8C5u + 0x4Bu) & 0xFF));
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0 + 0xB - 1],
              (long)(uint8)((0x8C5u + 0x4Bu + 0xB - 1) & 0xFF));
    ASSERT_EQ((long)g_outro_ws[LEFT_DST0 + 0xB], 0xC7);

    /* right skipped, middle not drawn. */
    ASSERT_EQ((long)g_outro_ws[RIGHT_ROW7], 0xC7);
    ASSERT_EQ((long)g_outro_ws[MID_ROW5E], 0xC7);
}


void run_anim_aniwalk2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/aniwalk (2/2)\n");
    RUN_TEST(test_walk_step_down_basic);
    RUN_TEST(test_walk_step_left_basic);
    RUN_TEST(test_walk_step_up_basic);
    RUN_TEST(test_walk_step_right_basic);
    RUN_TEST(test_slide_panel_bottom_main_stationary);
    RUN_TEST(test_slide_panel_bottom_small_early_return);
    RUN_TEST(test_slide_panel_bottom_small_clip_zero_rows);
    RUN_TEST(test_slide_panel_bottom_small_no_clip);
    RUN_TEST(test_slide_panel_bottom_small_stationary);
    RUN_TEST(test_outro_frame0_all_inplace);
    RUN_TEST(test_outro_frame5_right_clipped_middle_low);
    RUN_TEST(test_outro_frame6_middle_off);
    RUN_TEST(test_outro_frame9_right_skipped);
    RUN_TEST(test_outro_frame11_left_extreme_clip);
    printf("\n");
}
