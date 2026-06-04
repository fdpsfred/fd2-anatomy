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
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;


static uint8 t_tile_map[48];

static uint8 t_attr_flags[32];

static uint8 t_consumed[32];


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


/* Lay one row's worth of src markers and assert one row's worth of dst result
 * for the given derived (src_x, dst_x, row_bytes) at row `r`. Reused by every
 * path so the expected-value math lives in exactly one place per call site. */
static void lp_check_row(int r, long dst_x, long row_bytes)
{
    long d_base;
    long i;

    d_base = dst_x + 0x2E40 + (long)r * 0x140;

    /* copied span: every byte 0xBB (a wrong src_x on row 0 leaks the 0xFE
     * guard here; a wrong row_bytes shortens/lengthens the span). */
    for (i = 0; i < row_bytes; i++) {
        ASSERT_EQ((long)g_lp_dst[d_base + i], 0xBB);
    }
    /* byte just before the dst span: still background (proves dst_x exact). */
    ASSERT_EQ((long)g_lp_dst[d_base - 1], 0x11);
    /* byte just after the dst span: still background (proves row_bytes exact). */
    ASSERT_EQ((long)g_lp_dst[d_base + row_bytes], 0x11);
}


/* fd2_slide_panel_step_right_main is the right-side mirror. It copies 0x75 rows
 * from src_buffer (offset *always* 0x2E8B, verified at asm 0001afe2 ADD EDI,0x2e8b
 * — the src start is NEVER adjusted) into large_game_state_buffer (offset
 * dst_x+0x2E40), stride 0x140. row_bytes/dst_x are computed from frame_idx by
 * the verified prologue (0001afab-0001afde):
 *   frame>=5 (stationary): dst_x=0x4B, row_bytes=0xAA.
 *   frame<5: x_shift=(4-frame)*0x32; dst_x=x_shift+0x4B;
 *            if (x_shift+0xF5 > 0x140) then row_bytes = 0x140-dst_x  (right clip).
 * CRITICAL difference from left_main: the clip drops bytes off the RIGHT edge —
 * src start stays at 0x2E8B and only row_bytes shrinks (left_main instead shifts
 * src_x forward on a LEFT clip). So row 0's first copied byte must equal
 * src_buffer[0x2E8B]; the 0xFE poison goes at src[0x2E8B+row_bytes] (right side)
 * and at src[0x2E8B-1] (proves the start is exact, not shifted). dst guards are
 * the shared lp_check_row checks (0x11 at dst_x+0x2E40-1 and +row_bytes). Reuses
 * the shared 64000-byte g_lp_dst/g_lp_src (max touched dst offset 0xC080 < 64000),
 * memset fresh each call. Expected dst_x/row_bytes are exact integer arithmetic
 * read straight from the IMUL/LEA/CMP/SUB sequence above. */

/* Set src markers for right_main: copy window starts at 0x2E8B (fixed), with
 * 0xFE guards flanking [0x2E8B-1] and [0x2E8B+row_bytes]. */
static void rp_set_src(long row_bytes)
{
    long s_row0;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    s_row0 = 0x2E8B;
    g_lp_src[s_row0 - 1] = 0xFE;            /* start not shifted left */
    g_lp_src[s_row0 + row_bytes] = 0xFE;    /* clip drops the RIGHT edge */
}


/* slide_panel_down_step restores the *background snapshot* (0x53C5F) into the
 * workspace, then overlays src_buffer rows. This test pins the global the
 * first memmove reads from: snapshot bytes (0xAA) must reach workspace top,
 * NOT composed_target bytes (0xCC). Catches the wrong-source-global
 * regression. Row 0 copies src_buffer+0x8C05 (0xBB) to workspace+5, so
 * workspace[0..4] stay snapshot, workspace[5] becomes src. With y_offset=0,
 * row_count = min(0x56,200) = 0x56. Assertions read only dst_workspace,
 * which is fully written before the final VGA blit. */
static uint8 g_dp_workspace[64000];

static uint8 g_dp_snapshot[64000];

static uint8 g_dp_composed[64000];

static uint8 g_dp_src[64000];


/* fd2_slide_panel_step_top_small @0x1B019 copies a 0x66-wide column band from
 * src_buffer down into large_game_state_buffer, both at x-offset 0x6D, stride
 * 0x140. src_y/dst_y/row_count are computed from frame_idx by the verified
 * prologue (0001b02f-0001b082):
 *   frame_idx < 3            -> early return, no write at all.
 *   frame_idx > 7 (>=8)      -> stationary: src_y=0x13, dst_y=0x13, row_count=0x11.
 *   frame_idx 3..7           -> y_shift=(4-(frame_idx-3))*6; dst_y=0x13-y_shift;
 *                               if dst_y<0 (TOP clip): src_y=0x13-dst_y,
 *                               row_count=0x11+dst_y, dst_y=0.
 * Unlike left/right_main the clip is VERTICAL: a wrong src_y reads the wrong
 * source ROW, a wrong dst_y/row_count writes the wrong destination ROWS. The
 * shared 64000-byte g_lp_dst/g_lp_src cover the max touched offset (0x2C93).
 *
 * Derived per-frame values (exact integer arithmetic straight from the
 * IMUL/SUB/TEST/JGE/ADD/XOR sequence; emulate_function cannot derive them
 * because the function's __CHK stack-probe prologue (CALL 00036cd7) hits an
 * "Unimplemented CALLOTHER pcodeop (LOCK)" in the Ghidra emulator):
 *   frame 2: early return.
 *   frame 3: y_shift=24 -> dst_y=-5<0 -> CLIP: src_y=0x18(24), row_count=0x0C(12),
 *            dst_y=0.
 *   frame 4: y_shift=18 -> dst_y=1 (no clip): src_y=0x13(19), row_count=0x11(17).
 *   frame 8: stationary: src_y=0x13, dst_y=0x13, row_count=0x11(17).
 */

/* Assert one fully-written dst row: the 0x66-wide span at column 0x6D is all
 * 0xBB, flanked by 0x11 background. A wrong src_y leaks the 0xFE row-guard
 * (planted one row above/below the read window) into the 0xBB span. */
static void tp_check_dst_row(long dst_row)
{
    long d_base;
    long i;

    d_base = dst_row * 0x140 + 0x6D;

    /* copied span: every byte is 0xBB (catches wrong src_y via 0xFE leak). */
    for (i = 0; i < 0x66; i++) {
        ASSERT_EQ((long)g_lp_dst[d_base + i], 0xBB);
    }
    /* byte just before the dst span: still background (pins x-offset 0x6D). */
    ASSERT_EQ((long)g_lp_dst[d_base - 1], 0x11);
    /* byte just after the dst span: still background (pins width 0x66). */
    ASSERT_EQ((long)g_lp_dst[d_base + 0x66], 0x11);
}


/* Plant 0xFE across the 0x66-wide src band on the rows immediately ABOVE the
 * first read row and BELOW the last read row, so reading one row too high/low
 * (wrong src_y or row_count) leaks 0xFE into the dst band. */
static void tp_poison_src_vguards(long src_y, long row_count)
{
    long i;
    long above;
    long below;

    above = (src_y - 1) * 0x140 + 0x6D;
    below = (src_y + row_count) * 0x140 + 0x6D;
    for (i = 0; i < 0x66; i++) {
        g_lp_src[above + i] = 0xFE;
        g_lp_src[below + i] = 0xFE;
    }
}


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


static void test_tick_tile_event_anim_consumed_event(void)
{
    uint32 save_w;
    uint32 save_h;
    uint32 save_tm;
    uint32 save_af;
    uint32 save_cf;

    save_w  = data_fd2_battle_map_width_tiles;
    save_h  = data_fd2_battle_map_height_tiles;
    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;
    save_cf = data_fd2_field_map_tile_event_consumed_flags_ptr;

    data_fd2_battle_map_width_tiles  = 2;
    data_fd2_battle_map_height_tiles = 2;
    data_fd2_battle_tile_map_ptr     = (uint32)t_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr_flags;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)t_consumed;

    memset(t_tile_map, 0, sizeof(t_tile_map));
    memset(t_attr_flags, 0, sizeof(t_attr_flags));
    memset(t_consumed, 0, sizeof(t_consumed));

    /*
     * Tile (0,0) at tile_map offset 0.
     * fd2_read_tile_attribute_at_pos reads:
     *   sprite_idx    = *(uint16*)(tile_map + 4) & 0x3FF
     *   terrain_byte  = tile_map[6]
     *   terrain_class = terrain_byte & 0x1F  → buf+2
     *   attr_flags    = attr_buf[sprite_idx*4..+3] → buf+4..+7
     *
     * Set sprite_idx = 3, terrain_class = 7.
     * Set attr_flags[3*4] = 0x20  → (0x20 & 0x60) == 0x20.
     * Set consumed[7] = 1.
     */
    *(uint16 *)(t_tile_map + 4) = 3;
    t_tile_map[6] = 7;
    t_attr_flags[3 * 4] = 0x20;
    t_consumed[7] = 1;

    fd2_tick_tile_event_animations();

    ASSERT_EQ(*(uint16 *)(t_tile_map + 4), 4);
    ASSERT_EQ(t_tile_map[6], 0);

    data_fd2_battle_map_width_tiles  = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_tile_map_ptr     = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
    data_fd2_field_map_tile_event_consumed_flags_ptr = save_cf;
}


static void test_tick_tile_event_anim_non_event_no_change(void)
{
    uint32 save_w;
    uint32 save_h;
    uint32 save_tm;
    uint32 save_af;
    uint32 save_cf;

    save_w  = data_fd2_battle_map_width_tiles;
    save_h  = data_fd2_battle_map_height_tiles;
    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;
    save_cf = data_fd2_field_map_tile_event_consumed_flags_ptr;

    data_fd2_battle_map_width_tiles  = 2;
    data_fd2_battle_map_height_tiles = 2;
    data_fd2_battle_tile_map_ptr     = (uint32)t_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr_flags;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)t_consumed;

    memset(t_tile_map, 0, sizeof(t_tile_map));
    memset(t_attr_flags, 0, sizeof(t_attr_flags));
    memset(t_consumed, 0, sizeof(t_consumed));

    /* Tile (0,0): sprite_idx = 2, terrain_class = 5.
     * attr_flags = 0x40 → (0x40 & 0x60) = 0x40 != 0x20 → skip.
     */
    *(uint16 *)(t_tile_map + 4) = 2;
    t_tile_map[6] = 5;
    t_attr_flags[2 * 4] = 0x40;
    t_consumed[5] = 1;

    fd2_tick_tile_event_animations();

    ASSERT_EQ(*(uint16 *)(t_tile_map + 4), 2);
    ASSERT_EQ(t_tile_map[6], 5);

    data_fd2_battle_map_width_tiles  = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_tile_map_ptr     = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
    data_fd2_field_map_tile_event_consumed_flags_ptr = save_cf;
}


static void test_walk_path_animation_loop_2steps(void)
{
    uint8 path[2];
    uint32 save_cx;
    uint32 save_cy;

    save_cx = data_fd2_battle_cursor_world_x;
    save_cy = data_fd2_battle_cursor_world_y;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 5;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_map_height_tiles = 15;

    path[0] = 0;
    path[1] = 3;
    fd2_walk_path_animation_loop(0, (uint32)path, 2);

    ASSERT_EQ(g_test_rc_array[0].pos_x, 6);
    ASSERT_EQ(g_test_rc_array[0].pos_y, 6);

    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_world_y = save_cy;
}


/* frame_idx >= 5: stationary placement. row_bytes=0xAA, src_x=dst_x=0x4B. */
static void test_slide_panel_left_main_stationary(void)
{
    uint32 save_buf;
    long src_x;
    long dst_x;
    long row_bytes;
    long s_row0;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    src_x = 0x4B;
    dst_x = 0x4B;
    row_bytes = 0xAA;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    /* poison the bytes flanking row 0's expected read window with 0xFE. */
    s_row0 = src_x + 0x2E40;
    g_lp_src[s_row0 - 1] = 0xFE;
    g_lp_src[s_row0 + row_bytes] = 0xFE;

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_left_main((uint32)g_lp_src, 5);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* row 0: exact widths + guards (catches 0xFE leak from wrong src_x). */
    lp_check_row(0, dst_x, row_bytes);
    /* last copied row 0x74: proves loop ran 0x75 rows at stride 0x140. */
    lp_check_row(0x74, dst_x, row_bytes);
    /* row 0x75 would start at dst_x+0x2E40+0x75*0x140: must stay background. */
    ASSERT_EQ((long)g_lp_dst[dst_x + 0x2E40 + 0x75 * 0x140], 0x11);
}


/* frame_idx = 0: full left clip. x_shift=200 -> dst_x=-125<0 ->
 * row_bytes=0xAA-125=0x2D, src_x=0x4B+125=0xC8, dst_x=0 (left-aligned). */
static void test_slide_panel_left_main_full_clip(void)
{
    uint32 save_buf;
    long src_x;
    long dst_x;
    long row_bytes;
    long s_row0;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    src_x = 0xC8;       /* 0x4B - (-125) */
    dst_x = 0;
    row_bytes = 0x2D;   /* 0xAA + (-125) */

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    s_row0 = src_x + 0x2E40;
    g_lp_src[s_row0 - 1] = 0xFE;
    g_lp_src[s_row0 + row_bytes] = 0xFE;

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_left_main((uint32)g_lp_src, 0);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst_off must be 0x2E40 (dst_x=0), NOT 0x4B+0x2E40: the byte at
     * 0x2E40-1 is background and 0x2E40 itself is the copied span start. */
    ASSERT_EQ((long)g_lp_dst[0x2E40], 0xBB);
    lp_check_row(0, dst_x, row_bytes);
    lp_check_row(0x74, dst_x, row_bytes);
    ASSERT_EQ((long)g_lp_dst[dst_x + 0x2E40 + 0x75 * 0x140], 0x11);
}


/* frame_idx = 2: partial left clip. x_shift=100 -> dst_x=-25<0 ->
 * row_bytes=0xAA-25=0x91, src_x=0x4B+25=0x64, dst_x=0. */
static void test_slide_panel_left_main_partial_clip(void)
{
    uint32 save_buf;
    long src_x;
    long dst_x;
    long row_bytes;
    long s_row0;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    src_x = 0x64;       /* 0x4B - (-25) */
    dst_x = 0;
    row_bytes = 0x91;   /* 0xAA + (-25) */

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    s_row0 = src_x + 0x2E40;
    g_lp_src[s_row0 - 1] = 0xFE;
    g_lp_src[s_row0 + row_bytes] = 0xFE;

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_left_main((uint32)g_lp_src, 2);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    lp_check_row(0, dst_x, row_bytes);
    lp_check_row(0x74, dst_x, row_bytes);
    ASSERT_EQ((long)g_lp_dst[dst_x + 0x2E40 + 0x75 * 0x140], 0x11);
}


/* frame_idx >= 5: stationary placement. dst_x=0x4B, row_bytes=0xAA, no clip. */
static void test_slide_panel_right_main_stationary(void)
{
    uint32 save_buf;
    long dst_x;
    long row_bytes;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    dst_x = 0x4B;
    row_bytes = 0xAA;

    rp_set_src(row_bytes);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_right_main((uint32)g_lp_src, 5);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* row 0: exact widths + guards (a wrong src start leaks 0xFE here). */
    lp_check_row(0, dst_x, row_bytes);
    /* last copied row 0x74: proves loop ran 0x75 rows at stride 0x140. */
    lp_check_row(0x74, dst_x, row_bytes);
    /* row 0x75 would start one stride past row 0x74: must stay background. */
    ASSERT_EQ((long)g_lp_dst[dst_x + 0x2E40 + 0x75 * 0x140], 0x11);
}


/* frame_idx = 0: max right clip. x_shift=(4-0)*0x32=200 -> dst_x=200+0x4B=0x113;
 * x_shift+0xF5=200+245=445(0x1BD)>320(0x140) -> row_bytes=0x140-0x113=0x2D.
 * src start stays 0x2E8B. */
static void test_slide_panel_right_main_full_clip(void)
{
    uint32 save_buf;
    long dst_x;
    long row_bytes;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    dst_x = 0x113;
    row_bytes = 0x2D;

    rp_set_src(row_bytes);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_right_main((uint32)g_lp_src, 0);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* row 0's first copied byte is src[0x2E8B] (0xBB) reaching dst[0x113+0x2E40];
     * a wrong (shifted) src start would put 0xFE here instead. */
    ASSERT_EQ((long)g_lp_dst[dst_x + 0x2E40], 0xBB);
    lp_check_row(0, dst_x, row_bytes);
    lp_check_row(0x74, dst_x, row_bytes);
    ASSERT_EQ((long)g_lp_dst[dst_x + 0x2E40 + 0x75 * 0x140], 0x11);
}


/* frame_idx = 2: partial right clip. x_shift=100 -> dst_x=100+0x4B=0xAF;
 * x_shift+0xF5=100+245=345(0x159)>320(0x140) -> row_bytes=0x140-0xAF=0x91. */
static void test_slide_panel_right_main_partial_clip(void)
{
    uint32 save_buf;
    long dst_x;
    long row_bytes;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    dst_x = 0xAF;
    row_bytes = 0x91;

    rp_set_src(row_bytes);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_right_main((uint32)g_lp_src, 2);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    lp_check_row(0, dst_x, row_bytes);
    lp_check_row(0x74, dst_x, row_bytes);
    ASSERT_EQ((long)g_lp_dst[dst_x + 0x2E40 + 0x75 * 0x140], 0x11);
}


/* frame_idx = 3: moved but NOT clipped. x_shift=50 -> dst_x=50+0x4B=0x7D;
 * x_shift+0xF5=50+245=295(0x127) <= 320(0x140) -> NO clip, row_bytes=0xAA.
 * Pins the JLE-skips-clip branch with a non-stationary dst_x. */
static void test_slide_panel_right_main_no_clip(void)
{
    uint32 save_buf;
    long dst_x;
    long row_bytes;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    dst_x = 0x7D;
    row_bytes = 0xAA;

    rp_set_src(row_bytes);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_right_main((uint32)g_lp_src, 3);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    lp_check_row(0, dst_x, row_bytes);
    lp_check_row(0x74, dst_x, row_bytes);
    ASSERT_EQ((long)g_lp_dst[dst_x + 0x2E40 + 0x75 * 0x140], 0x11);
}


static void test_slide_panel_down_step_restores_snapshot(void)
{
    uint32 save_snap;
    uint32 save_comp;
    long last_row_dst;

    save_snap = data_fd2_ui_slide_bg_snapshot_buf_ptr;
    save_comp = data_fd2_ui_slide_composed_target_buf_ptr;

    memset(g_dp_workspace, 0x11, sizeof(g_dp_workspace));
    memset(g_dp_snapshot, 0xAA, sizeof(g_dp_snapshot));
    memset(g_dp_composed, 0xCC, sizeof(g_dp_composed));
    memset(g_dp_src, 0xBB, sizeof(g_dp_src));

    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)g_dp_snapshot;
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)g_dp_composed;

    fd2_slide_panel_down_step(0, (uint32)g_dp_workspace, (uint32)g_dp_src);

    /* workspace top 5 bytes untouched by row copy -> must be snapshot (0xAA),
     * proving the restore read bg_snapshot, not composed_target (0xCC). */
    ASSERT_EQ((long)g_dp_workspace[0], 0xAA);
    ASSERT_EQ((long)g_dp_workspace[4], 0xAA);
    /* row 0 copy: dst offset 5, 0x136 bytes from src (0xBB). */
    ASSERT_EQ((long)g_dp_workspace[5], 0xBB);
    ASSERT_EQ((long)g_dp_workspace[5 + 0x135], 0xBB);
    /* byte just past row 0's 0x136-wide copy is snapshot again. */
    ASSERT_EQ((long)g_dp_workspace[5 + 0x136], 0xAA);
    /* last copied row is row 0x55: dst = 5 + 0x55*0x140. */
    last_row_dst = 5 + 0x55 * 0x140;
    ASSERT_EQ((long)g_dp_workspace[last_row_dst], 0xBB);
    /* row 0x56 would start at 5 + 0x56*0x140; must NOT be copied (snapshot). */
    ASSERT_EQ((long)g_dp_workspace[5 + 0x56 * 0x140], 0xAA);

    data_fd2_ui_slide_bg_snapshot_buf_ptr = save_snap;
    data_fd2_ui_slide_composed_target_buf_ptr = save_comp;
}


/* Clip branch: y_offset = 195 -> 195+0x56=281 >= 200 -> row_count = 200-195 = 5.
 * Only 5 rows (195..199) get overlaid; row 200 region stays snapshot. */
static void test_slide_panel_down_step_bottom_clip(void)
{
    uint32 save_snap;
    long row5_dst;

    save_snap = data_fd2_ui_slide_bg_snapshot_buf_ptr;

    memset(g_dp_workspace, 0x11, sizeof(g_dp_workspace));
    memset(g_dp_snapshot, 0xAA, sizeof(g_dp_snapshot));
    memset(g_dp_src, 0xBB, sizeof(g_dp_src));
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)g_dp_snapshot;

    fd2_slide_panel_down_step(195, (uint32)g_dp_workspace, (uint32)g_dp_src);

    /* row 0 (screen y=195): dst = 5 + 195*0x140 -> src (0xBB). */
    ASSERT_EQ((long)g_dp_workspace[5 + 195 * 0x140], 0xBB);
    /* row 4 (screen y=199, last): dst = 5 + 199*0x140 -> src (0xBB). */
    row5_dst = 5 + 199 * 0x140;
    ASSERT_EQ((long)g_dp_workspace[row5_dst], 0xBB);
    /* row 5 (screen y=200) clipped off: never reached (200*0x140 = 64000,
     * past buffer end), so verify a still-snapshot interior byte at y=199
     * just before the copied span start instead: workspace[199*0x140] is
     * outside the 5-byte left margin? offset 199*0x140 = 63680 < 63685 (copy
     * start), so it stays snapshot. */
    ASSERT_EQ((long)g_dp_workspace[199 * 0x140], 0xAA);

    data_fd2_ui_slide_bg_snapshot_buf_ptr = save_snap;
}


/* frame_idx = 2 (<3): function must early-return; dst stays fully background. */
static void test_slide_panel_top_small_early_return(void)
{
    uint32 save_buf;

    save_buf = data_fd2_large_game_state_buffer_ptr;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_top_small((uint32)g_lp_src, 2);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* nothing copied: the would-be first/last destination bytes stay 0x11. */
    ASSERT_EQ((long)g_lp_dst[0x13 * 0x140 + 0x6D], 0x11);
    ASSERT_EQ((long)g_lp_dst[0x6D], 0x11);
}


/* frame_idx = 3: TOP clip. dst_y=0, src_y=0x18, row_count=0x0C (12 rows). */
static void test_slide_panel_top_small_top_clip(void)
{
    uint32 save_buf;
    long src_y;
    long dst_y;
    long row_count;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    src_y = 0x18;
    dst_y = 0;
    row_count = 0x0C;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    tp_poison_src_vguards(src_y, row_count);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_top_small((uint32)g_lp_src, 3);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* first written row 0 maps dst_y=0 <- src_y=0x18 (top-aligned). */
    tp_check_dst_row(dst_y + 0);
    /* last written row 11 maps dst_y=11 <- src_y=0x18+11=0x23. */
    tp_check_dst_row(dst_y + (row_count - 1));
    /* row just past the last written row stays background (pins row_count). */
    ASSERT_EQ((long)g_lp_dst[(dst_y + row_count) * 0x140 + 0x6D], 0x11);
}


/* frame_idx = 4: moved but NOT clipped. dst_y=1, src_y=0x13, row_count=0x11. */
static void test_slide_panel_top_small_no_clip(void)
{
    uint32 save_buf;
    long src_y;
    long dst_y;
    long row_count;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    src_y = 0x13;
    dst_y = 1;
    row_count = 0x11;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    tp_poison_src_vguards(src_y, row_count);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_top_small((uint32)g_lp_src, 4);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst row just ABOVE the first written row (dst_y-1=0) stays background:
     * proves dst_y is exactly 1, not 0 (the clip path was NOT taken). */
    ASSERT_EQ((long)g_lp_dst[0 * 0x140 + 0x6D], 0x11);
    tp_check_dst_row(dst_y + 0);
    tp_check_dst_row(dst_y + (row_count - 1));
    ASSERT_EQ((long)g_lp_dst[(dst_y + row_count) * 0x140 + 0x6D], 0x11);
}


/* frame_idx = 8 (>7): stationary. dst_y=0x13, src_y=0x13, row_count=0x11. */
static void test_slide_panel_top_small_stationary(void)
{
    uint32 save_buf;
    long src_y;
    long dst_y;
    long row_count;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    src_y = 0x13;
    dst_y = 0x13;
    row_count = 0x11;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    tp_poison_src_vguards(src_y, row_count);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_top_small((uint32)g_lp_src, 8);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst row just above first written row (dst_y-1=0x12) stays background. */
    ASSERT_EQ((long)g_lp_dst[0x12 * 0x140 + 0x6D], 0x11);
    tp_check_dst_row(dst_y + 0);
    tp_check_dst_row(dst_y + (row_count - 1));
    ASSERT_EQ((long)g_lp_dst[(dst_y + row_count) * 0x140 + 0x6D], 0x11);
}


/* frame_idx = 4 (<5): function must early-return; dst stays fully background. */
static void test_slide_panel_bottom_main_early_return(void)
{
    uint32 save_buf;

    save_buf = data_fd2_large_game_state_buffer_ptr;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_bottom_main((uint32)g_lp_src, 4);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* nothing copied: the would-be dst row at every reachable dst_y stays 0x11. */
    ASSERT_EQ((long)g_lp_dst[0x9B * 0x140 + 0x4B], 0x11);
    ASSERT_EQ((long)g_lp_dst[0xBF * 0x140 + 0x4B], 0x11);
}


/* frame_idx = 5: BOTTOM clip. dst_y=191, row_count = 191-200 = -9 (signed) ->
 * the loop copies ZERO rows. This is the exact path that was emitted with the
 * wrong sign (200-dst_y = +9 would have copied 9 rows); pin it at 0 writes. */
static void test_slide_panel_bottom_main_clip_zero_rows(void)
{
    uint32 save_buf;

    save_buf = data_fd2_large_game_state_buffer_ptr;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_bottom_main((uint32)g_lp_src, 5);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst row 0 (the row that WOULD be written if row_count were +9) at
     * dst_y=191 must remain background: the negative count copies nothing. */
    ASSERT_EQ((long)g_lp_dst[191 * 0x140 + 0x4B], 0x11);
    ASSERT_EQ((long)g_lp_dst[191 * 0x140 + 0x4B + 0xA9], 0x11);
    /* and row 8 (the last of the would-be 9 rows) likewise stays background. */
    ASSERT_EQ((long)g_lp_dst[(191 + 8) * 0x140 + 0x4B], 0x11);
}


/* frame_idx = 6: moved but NOT clipped. dst_y=182, row_count=0x10 (16 rows). */
static void test_slide_panel_bottom_main_no_clip(void)
{
    uint32 save_buf;
    long dst_y;
    long row_count;

    save_buf = data_fd2_large_game_state_buffer_ptr;
    dst_y = 182;        /* 0xB6 */
    row_count = 0x10;

    memset(g_lp_dst, 0x11, sizeof(g_lp_dst));
    memset(g_lp_src, 0xBB, sizeof(g_lp_src));
    bp_poison_src_vguards(row_count);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lp_dst;
    fd2_slide_panel_step_bottom_main((uint32)g_lp_src, 6);
    data_fd2_large_game_state_buffer_ptr = save_buf;

    /* dst row just ABOVE the first written row (dst_y-1) stays background:
     * proves dst_y is exactly 182, not the stationary 155. */
    ASSERT_EQ((long)g_lp_dst[(dst_y - 1) * 0x140 + 0x4B], 0x11);
    bp_check_dst_row(dst_y + 0);
    bp_check_dst_row(dst_y + (row_count - 1));
    /* row just past the last written row stays background (pins row_count=0x10). */
    ASSERT_EQ((long)g_lp_dst[(dst_y + row_count) * 0x140 + 0x4B], 0x11);
}


void run_anim_aniwalk1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/aniwalk (1/2)\n");
    RUN_TEST(test_tick_tile_event_anim_consumed_event);
    RUN_TEST(test_tick_tile_event_anim_non_event_no_change);
    RUN_TEST(test_walk_path_animation_loop_2steps);
    RUN_TEST(test_slide_panel_left_main_stationary);
    RUN_TEST(test_slide_panel_left_main_full_clip);
    RUN_TEST(test_slide_panel_left_main_partial_clip);
    RUN_TEST(test_slide_panel_right_main_stationary);
    RUN_TEST(test_slide_panel_right_main_full_clip);
    RUN_TEST(test_slide_panel_right_main_partial_clip);
    RUN_TEST(test_slide_panel_right_main_no_clip);
    RUN_TEST(test_slide_panel_down_step_restores_snapshot);
    RUN_TEST(test_slide_panel_down_step_bottom_clip);
    RUN_TEST(test_slide_panel_top_small_early_return);
    RUN_TEST(test_slide_panel_top_small_top_clip);
    RUN_TEST(test_slide_panel_top_small_no_clip);
    RUN_TEST(test_slide_panel_top_small_stationary);
    RUN_TEST(test_slide_panel_bottom_main_early_return);
    RUN_TEST(test_slide_panel_bottom_main_clip_zero_rows);
    RUN_TEST(test_slide_panel_bottom_main_no_clip);
    printf("\n");
}
