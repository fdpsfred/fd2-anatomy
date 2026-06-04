/*
 * unit tests for src/gfx/rndstat.c
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* recording spies for the two dialog blit primitives (testglob.c) */
extern int    g_dlg_blit_normal_calls;
extern int    g_dlg_blit_mirrored_calls;
extern uint32 g_dlg_blit_last_dst;
extern uint32 g_dlg_blit_last_sprite;
extern uint32 g_dlg_blit_last_stride;

/* 12-int sprite-offset table + payload area, matching the DATO.DAT layout
 * head that fd2_paint_portrait_to_dialog_area indexes by frame*4. */
static int32 g_portrait_buf[64];

static void portrait_reset(void)
{
    int i;

    for (i = 0; i < 64; i++) {
        g_portrait_buf[i] = 0;
    }
    /* frame -> byte offset into the buffer for that frame's sprite payload */
    g_portrait_buf[0] = 0x30;
    g_portrait_buf[1] = 0x44;
    g_portrait_buf[2] = 0x58;

    data_fd2_portrait_sprite_buffer = (uint8 *)g_portrait_buf;

    g_dlg_blit_normal_calls = 0;
    g_dlg_blit_mirrored_calls = 0;
    g_dlg_blit_last_dst = 0;
    g_dlg_blit_last_sprite = 0;
    g_dlg_blit_last_stride = 0;
}

/*
 * Left-side slot (offset 0x728, != 0x9017) takes the normal-blit path:
 *   dst    = 0xA0000 + 0x728 = 0xA0728
 *   sprite = buffer + offset_table[0]
 *   stride = 0x140
 * and the mirrored primitive must NOT be called.
 */
static void test_normal_path_left_slot(void)
{
    portrait_reset();
    data_fd2_dialog_active_portrait_blit_offset = 0x728;

    fd2_paint_portrait_to_dialog_area(0);

    ASSERT_EQ((long)g_dlg_blit_normal_calls, 1);
    ASSERT_EQ((long)g_dlg_blit_mirrored_calls, 0);
    ASSERT_EQ((long)g_dlg_blit_last_dst, (long)0xA0728u);
    ASSERT_EQ((long)g_dlg_blit_last_sprite,
              (long)((uint32)(uint8 *)g_portrait_buf + 0x30u));
    ASSERT_EQ((long)g_dlg_blit_last_stride, (long)0x140u);
}

/*
 * Right-side ally slot (offset == 0x9017) takes the mirrored-blit path:
 *   dst    = 0xA9017 (hard-coded literal in the binary, not offset+0xA0000)
 *   sprite = buffer + offset_table[frame]
 *   stride = 0x140
 * and the normal primitive must NOT be called.
 */
static void test_mirrored_path_right_slot(void)
{
    portrait_reset();
    data_fd2_dialog_active_portrait_blit_offset = 0x9017;

    fd2_paint_portrait_to_dialog_area(1);

    ASSERT_EQ((long)g_dlg_blit_mirrored_calls, 1);
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 0);
    ASSERT_EQ((long)g_dlg_blit_last_dst, (long)0xA9017u);
    ASSERT_EQ((long)g_dlg_blit_last_sprite,
              (long)((uint32)(uint8 *)g_portrait_buf + 0x44u));
    ASSERT_EQ((long)g_dlg_blit_last_stride, (long)0x140u);
}

/*
 * The frame index selects the int (4-byte) entry frame*4 of the offset
 * table — frame 2 must resolve to offset_table[2], proving the stride-4
 * int indexing rather than a byte read.
 */
static void test_frame_index_selects_int_entry(void)
{
    portrait_reset();
    data_fd2_dialog_active_portrait_blit_offset = 0x728;

    fd2_paint_portrait_to_dialog_area(2);

    ASSERT_EQ((long)g_dlg_blit_normal_calls, 1);
    ASSERT_EQ((long)g_dlg_blit_last_sprite,
              (long)((uint32)(uint8 *)g_portrait_buf + 0x58u));
}

/*
 * A non-0x9017, non-0x728 offset still takes the normal path and adds
 * 0xA0000 to whatever the active offset is (e.g. 0 -> 0xA0000).
 */
static void test_normal_path_zero_offset(void)
{
    portrait_reset();
    data_fd2_dialog_active_portrait_blit_offset = 0;

    fd2_paint_portrait_to_dialog_area(0);

    ASSERT_EQ((long)g_dlg_blit_normal_calls, 1);
    ASSERT_EQ((long)g_dlg_blit_mirrored_calls, 0);
    ASSERT_EQ((long)g_dlg_blit_last_dst, (long)0xA0000u);
}

/* ----------------------------------------------------------------
 * fd2_render_horizontal_bar_segments @ 0x17d6f
 *
 * Drive the REAL render fn -> REAL fd2_blit_sheet_sprite_at_offset
 * -> stubbed fd2_blit_sprite_raw_with_header (logged in testglob.c via
 * g_blitraw_log_*).  We install a fake sprite sheet whose 4-byte offset
 * table (at sheet+6) maps sprite_idx i -> table[i] = i, so the resolved
 * sprite_addr = sheet + table[sprite_idx] = sheet + sprite_idx, letting us
 * recover the exact sprite index each blit used as (logged_sprite - sheet).
 * The logged dst gives us the per-segment destination offset.
 * ---------------------------------------------------------------- */
extern int    g_blitraw_log_on;
extern int    g_blitraw_count;
extern uint32 g_blitraw_log_dst[512];
extern uint32 g_blitraw_log_sprite[512];

/* sheet header (6 bytes) + 256-entry int32 offset table */
static int32 g_bar_sheet[2 + 256];

static uint32 bar_setup_sheet(void)
{
    int i;

    /* table[i] = i, located at byte offset 6 (= 1.5 int32 slots) from base.
     * Place the table so its bytes start exactly at sheet+6: bytes 0..5 are
     * header, byte 6 begins entry 0. We allocate the int32 table at a 6-byte
     * offset by treating the buffer as bytes. */
    uint8 *base = (uint8 *)g_bar_sheet;
    for (i = 0; i < 256; i++) {
        *(int32 *)(base + 6 + i * 4) = i;
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)base;
    return (uint32)base;
}

static void bar_reset(void)
{
    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
}

/* fully-empty bar: filled_count == 0 emits 0x65 (101) middle 0x1D segments
 * at offset+1..offset+0x65, then an empty right cap 0x1E at offset+0x66
 * (the carried EAX = dst_offset + 0x66 from the final loop LEA). 102 blits. */
static void test_empty_bar_segments(void)
{
    uint32 sheet;
    uint32 off = 0x1000;
    int i;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_horizontal_bar_segments(off, 0x140, 0, 0x17);

    ASSERT_EQ((long)g_blitraw_count, 102);
    /* first 101: sprite 0x1D at off+1 .. off+0x65 */
    for (i = 0; i < 0x65; i++) {
        ASSERT_EQ((long)(g_blitraw_log_sprite[i] - sheet), 0x1D);
        ASSERT_EQ((long)g_blitraw_log_dst[i], (long)(off + 1 + i));
    }
    /* final: empty right cap 0x1E at off+0x66 */
    ASSERT_EQ((long)(g_blitraw_log_sprite[101] - sheet), 0x1E);
    ASSERT_EQ((long)g_blitraw_log_dst[101], (long)(off + 0x66));
}

/* single-segment filled bar: left cap then right cap, no middles. */
static void test_single_segment_bar(void)
{
    uint32 sheet;
    uint32 off = 0x2000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_horizontal_bar_segments(off, 0x140, 1, 0x17);

    ASSERT_EQ((long)g_blitraw_count, 2);
    /* left cap 0x17 @ off */
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x17);
    ASSERT_EQ((long)g_blitraw_log_dst[0], (long)off);
    /* right cap 0x19 @ off+1 */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x19);
    ASSERT_EQ((long)g_blitraw_log_dst[1], (long)(off + 1));
}

/* multi-segment filled bar (HP, base 0x17): left cap + 2 middles + right cap. */
static void test_filled_bar_three_segments(void)
{
    uint32 sheet;
    uint32 off = 0x3000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_horizontal_bar_segments(off, 0x140, 3, 0x17);

    ASSERT_EQ((long)g_blitraw_count, 4);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x17);   /* left cap  */
    ASSERT_EQ((long)g_blitraw_log_dst[0],    (long)off);
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x18);   /* middle    */
    ASSERT_EQ((long)g_blitraw_log_dst[1],    (long)(off + 1));
    ASSERT_EQ((long)(g_blitraw_log_sprite[2] - sheet), 0x18);   /* middle    */
    ASSERT_EQ((long)g_blitraw_log_dst[2],    (long)(off + 2));
    ASSERT_EQ((long)(g_blitraw_log_sprite[3] - sheet), 0x19);   /* right cap */
    ASSERT_EQ((long)g_blitraw_log_dst[3],    (long)(off + 3));
}

/* MP bar (base 0x1A) uses the blue theme: caps 0x1A / 0x1B / 0x1C. */
static void test_mp_bar_theme_base(void)
{
    uint32 sheet;
    uint32 off = 0x4000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_horizontal_bar_segments(off, 0x140, 2, 0x1A);

    ASSERT_EQ((long)g_blitraw_count, 3);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x1A);   /* left cap  */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x1B);   /* middle    */
    ASSERT_EQ((long)(g_blitraw_log_sprite[2] - sheet), 0x1C);   /* right cap */
    ASSERT_EQ((long)g_blitraw_log_dst[2], (long)(off + 2));
}

/* ----------------------------------------------------------------
 * fd2_render_full_char_stat_panel @ 0x17fc0
 *
 * Drive the REAL panel painter and capture its dispatch to:
 *   - fd2_render_hp_or_mp_bar_proportional  (recording spy, testglob)
 *   - fd2_render_number_red_when_full (REAL, src/gfx/rndstat.c) -> forwards
 *     into the fd2_render_decimal_number_to_buffer spy with a red/white color
 *     chosen by current==max, so its 4 numbers appear in g_render_dec_* too
 *   - fd2_render_decimal_number_to_buffer   (recording spy, testglob)
 *   - fd2_blit_sheet_sprite_at_offset (REAL) -> g_blitraw log (team flag +
 *     status icons)
 *   - fd2_display_dialog_scene (REAL) for the 3 text labels, against a
 *     minimal text program whose every page entry points at an immediate
 *     END (-1) opcode so the dialog VM returns at once (no fopen / no wait).
 *
 * The panel indexes data_fd2_battle_runtime_char_array_ptr (= g_test_rc_array,
 * 8 slots) so all tests use slot 0.
 * ---------------------------------------------------------------- */
extern int    g_render_log_on;
extern int    g_render_dec_count;
extern uint32 g_render_dec_dst[32];
extern uint32 g_render_dec_val[32];
extern uint32 g_render_dec_color[32];
extern uint32 g_render_dec_digits[32];

extern runtime_char g_test_rc_array[8];

/* Locate the g_blitraw log entry whose destination equals `dst` and return its
 * sprite-stream pointer (sheet + sprite_index); fail if no entry matches. Used
 * by the panel tests to pick the team-flag / status-icon blits out of the log,
 * which (now that fd2_render_hp_or_mp_bar_proportional is the real function)
 * also contains the HP/MP bar segment blits ahead of them. */
static uint32 panel_find_blit_sprite(uint32 dst)
{
    int i;

    for (i = 0; i < g_blitraw_count; i++) {
        if (g_blitraw_log_dst[i] == dst) {
            return g_blitraw_log_sprite[i];
        }
    }
    /* not found: return 0 so the caller's (result - sheet) underflows far from
     * any valid sprite index and its ASSERT_EQ fails visibly. */
    return 0;
}

/* count how many g_blitraw entries target destination `dst` (0 if none). */
static int panel_count_blit_at(uint32 dst)
{
    int i;
    int n = 0;

    for (i = 0; i < g_blitraw_count; i++) {
        if (g_blitraw_log_dst[i] == dst) {
            n++;
        }
    }
    return n;
}

/* immediate-END text program for the 3 text-label dialog calls. The dialog VM
 * computes cur_op = base + *(int16*)(base + page*2); if that points at int16
 * -1 it returns immediately. We park one END marker high in the buffer and
 * point every used page entry at it. */
#define PANEL_TEXT_WORDS 0x400
#define PANEL_END_OFF    0x780     /* byte offset of the END marker */
static uint16 g_panel_text[PANEL_TEXT_WORDS];

static void panel_setup_text(void)
{
    int i;

    for (i = 0; i < PANEL_TEXT_WORDS; i++) {
        g_panel_text[i] = 0;
    }
    *(int16 *)((uint8 *)g_panel_text + PANEL_END_OFF) = -1;
    /* every page entry in the table region points at the END marker */
    for (i = 0; i < (PANEL_END_OFF / 2); i++) {
        g_panel_text[i] = (uint16)PANEL_END_OFF;
    }
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)g_panel_text;
}

/* zero a runtime_char slot and load a known stat profile. char_id/archetype/
 * job kept small so their page indices (+1 / +0x8C / +0x96) stay inside the
 * text table region. */
static runtime_char *panel_setup_char(void)
{
    runtime_char *rc = &g_test_rc_array[0];
    memset(rc, 0, sizeof(*rc));

    rc->hp_current = 0x0050;
    rc->hp_max     = 0x0064;
    rc->mp_current = 0x0010;
    rc->mp_max     = 0x0020;
    rc->ap         = 0x0011;
    rc->dp         = 0x0022;
    rc->dx_current = 0x0033;
    rc->stat4_current = 0x0044;
    rc->status_flags_block[0] = 0x0A;   /* level    -> +0x29dd */
    rc->movement_order        = 0x05;   /* MV       -> +0x379d */
    rc->combat_aux_block[0x14] = 0x07;  /* mag res  -> +0x455d */
    rc->ai_target_and_dx_block[1] = 0x55;
    rc->ai_target_and_dx_block[2] = 0x00; /* dx-base word = 0x0055 */
    rc->char_id        = 0x03;
    rc->archetype_flag = 0x02;
    rc->job_id         = 0x04;
    rc->team           = 2;             /* player */
    return rc;
}

static void panel_reset_logs(void)
{
    g_render_dec_count = 0;
    g_blitraw_count = 0;
    g_render_log_on = 1;
    g_blitraw_log_on = 1;
}

/* HP/MP bars + the 4 red-when-full numbers carry the exact (sign-extended)
 * stat values to the proper surface offsets and digit widths.
 *
 * Now that fd2_render_hp_or_mp_bar_proportional is the real function, the panel
 * drives it end-to-end: cur/max flow through the proportional segment formula
 * into fd2_render_horizontal_bar_segments, whose left/right cap sprites land in
 * the g_blitraw log. We pin the bar at its left cap (base sprite at the bar
 * origin) and at its right cap, whose offset = origin + segment_count encodes
 * the exact (cur,max) the panel forwarded:
 *   HP: segments = (0x50*0x65)/0x64 + 1 = 81 -> right cap 0x19 @ +0x2a06+81
 *   MP: segments = (0x10*0x65)/0x20 + 1 = 51 -> right cap 0x1C @ +0x41c6+51 */
static void test_panel_bars_and_full_numbers(void)
{
    runtime_char *rc;
    uint32 sheet;
    uint32 buf = 0x100000;

    sheet = bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    /* HP bar (base 0x17): left cap @origin, right cap @origin+81 */
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x2a06) - sheet), 0x17);
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x2a06 + 81) - sheet), 0x19);
    /* MP bar (base 0x1A): left cap @origin, right cap @origin+51 */
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x41c6) - sheet), 0x1a);
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x41c6 + 51) - sheet), 0x1c);

    /* The real fd2_render_number_red_when_full forwards into the decimal spy,
     * so the 4 HP/MP cur/max numbers are g_render_dec_* entries [0..3] (the 8
     * direct stat numbers follow at [4..11], total 12). Each carries the
     * sign-extended value to the right surface offset, 3-digit, with the
     * "full" color: cur==max -> red 0x1F, else white 0x2A. The two "max"
     * variants pass current==max so they are red. */
    ASSERT_EQ((long)g_render_dec_count, 12);
    /* HP current: 0x50 != max 0x64 -> white */
    ASSERT_EQ((long)g_render_dec_dst[0],    (long)(buf + 0x344b));
    ASSERT_EQ((long)g_render_dec_val[0],    0x50);
    ASSERT_EQ((long)g_render_dec_color[0],  0x2a);
    ASSERT_EQ((long)g_render_dec_digits[0], 3);
    /* HP max: 0x64 == max 0x64 -> red */
    ASSERT_EQ((long)g_render_dec_dst[1],    (long)(buf + 0x3465));
    ASSERT_EQ((long)g_render_dec_val[1],    0x64);
    ASSERT_EQ((long)g_render_dec_color[1],  0x1f);
    ASSERT_EQ((long)g_render_dec_digits[1], 3);
    /* MP current: 0x10 != max 0x20 -> white */
    ASSERT_EQ((long)g_render_dec_dst[2],    (long)(buf + 0x4acb));
    ASSERT_EQ((long)g_render_dec_val[2],    0x10);
    ASSERT_EQ((long)g_render_dec_color[2],  0x2a);
    /* MP max: 0x20 == max 0x20 -> red */
    ASSERT_EQ((long)g_render_dec_dst[3],    (long)(buf + 0x4ae5));
    ASSERT_EQ((long)g_render_dec_val[3],    0x20);
    ASSERT_EQ((long)g_render_dec_color[3],  0x1f);
}

/* the 8 fd2_render_decimal_number_to_buffer calls carry the right field
 * values, surface offsets, digit widths and (with all boost flags clear)
 * the normal white color 0x2A. */
static void test_panel_decimal_numbers_unboosted(void)
{
    runtime_char *rc;
    uint32 buf = 0x200000;

    bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    /* 4 red-when-full numbers ([0..3]) precede the 8 direct stat numbers
     * ([4..11]) in the decimal spy now that the wrapper is real. */
    ASSERT_EQ((long)g_render_dec_count, 12);

    /* [4] level (2-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[4], (long)(buf + 0x29dd));
    ASSERT_EQ((long)g_render_dec_val[4], 0x0A);
    ASSERT_EQ((long)g_render_dec_color[4], 0x2a);
    ASSERT_EQ((long)g_render_dec_digits[4], 2);
    /* [5] movement (2-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[5], (long)(buf + 0x379d));
    ASSERT_EQ((long)g_render_dec_val[5], 0x05);
    ASSERT_EQ((long)g_render_dec_digits[5], 2);
    /* [6] magic resist (2-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[6], (long)(buf + 0x455d));
    ASSERT_EQ((long)g_render_dec_val[6], 0x07);
    ASSERT_EQ((long)g_render_dec_digits[6], 2);
    /* [7] AP (3-digit, white because boost flag clear) */
    ASSERT_EQ((long)g_render_dec_dst[7], (long)(buf + 0x545d));
    ASSERT_EQ((long)g_render_dec_val[7], 0x11);
    ASSERT_EQ((long)g_render_dec_color[7], 0x2a);
    ASSERT_EQ((long)g_render_dec_digits[7], 3);
    /* [8] DP (3-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[8], (long)(buf + 0x635d));
    ASSERT_EQ((long)g_render_dec_val[8], 0x22);
    ASSERT_EQ((long)g_render_dec_color[8], 0x2a);
    /* [9] DX base (3-digit, ALWAYS white) — word at dx_block[1] = 0x0055 */
    ASSERT_EQ((long)g_render_dec_dst[9], (long)(buf + 0x4535));
    ASSERT_EQ((long)g_render_dec_val[9], 0x55);
    ASSERT_EQ((long)g_render_dec_color[9], 0x2a);
    /* [10] DX current (3-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[10], (long)(buf + 0x5435));
    ASSERT_EQ((long)g_render_dec_val[10], 0x33);
    ASSERT_EQ((long)g_render_dec_color[10], 0x2a);
    /* [11] Evade (3-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[11], (long)(buf + 0x6335));
    ASSERT_EQ((long)g_render_dec_val[11], 0x44);
    ASSERT_EQ((long)g_render_dec_color[11], 0x2a);
}

/* each combat-stat boost flag independently flips its number to red 0x77;
 * the DX-base number (index 5) is never colored. */
static void test_panel_boost_colors_independent(void)
{
    runtime_char *rc;
    uint32 buf = 0x300000;

    bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    rc->status_flags_block[1] = 1;   /* AP boosted */
    rc->status_flags_block[2] = 1;   /* DP boosted */
    rc->status_flags_block[3] = 0;   /* DX/Evade NOT boosted */
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    /* direct stat numbers are dec indices [4..11] (4 red-when-full precede) */
    ASSERT_EQ((long)g_render_dec_count, 12);
    ASSERT_EQ((long)g_render_dec_color[7],  0x77);  /* AP red   */
    ASSERT_EQ((long)g_render_dec_color[8],  0x77);  /* DP red   */
    ASSERT_EQ((long)g_render_dec_color[9],  0x2a);  /* DX base white */
    ASSERT_EQ((long)g_render_dec_color[10], 0x2a);  /* DX cur white */
    ASSERT_EQ((long)g_render_dec_color[11], 0x2a);  /* Evade white */
}

/* the binary reuses one color (ESI) for BOTH DX current and Evade: setting
 * only status_flags_block[3] must turn indices 6 AND 7 red while AP/DP stay
 * white. This locks the shared-flag behavior. */
static void test_panel_evade_shares_dx_color(void)
{
    runtime_char *rc;
    uint32 buf = 0x340000;

    bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    rc->status_flags_block[1] = 0;   /* AP normal */
    rc->status_flags_block[2] = 0;   /* DP normal */
    rc->status_flags_block[3] = 1;   /* DX + Evade boosted */
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    ASSERT_EQ((long)g_render_dec_color[7],  0x2a);  /* AP white */
    ASSERT_EQ((long)g_render_dec_color[8],  0x2a);  /* DP white */
    ASSERT_EQ((long)g_render_dec_color[10], 0x77);  /* DX cur red */
    ASSERT_EQ((long)g_render_dec_color[11], 0x77);  /* Evade red (shared) */
}

/* 16-bit stat reads are sign-extended (MOVSX in the binary): a value with
 * bit15 set propagates as 0xFFFFxxxx through the bar/number primitives. */
static void test_panel_stat_sign_extension(void)
{
    runtime_char *rc;
    uint32 sheet;
    uint32 buf = 0x380000;

    sheet = bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    rc->hp_current = 0x8001;         /* (int16)0x8001 = -32767 */
    rc->ap         = 0x8002;
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    /* HP bar still paints its left cap at the origin: the sign-extended
     * (negative, non-zero) hp_current took the proportional branch of the real
     * fd2_render_hp_or_mp_bar_proportional (current != 0), not the empty
     * branch, so the base sprite 0x17 lands at the bar origin. */
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x2a06) - sheet), 0x17);
    /* HP-current red number (dec spy [0], via the real wrapper) is likewise
     * sign-extended; its color is white because cur 0xFFFF8001 != max 0x64 */
    ASSERT_EQ((long)g_render_dec_val[0], (long)0xFFFF8001u);
    ASSERT_EQ((long)g_render_dec_color[0], 0x2a);
    /* AP decimal value (dec spy [7]) sign-extended */
    ASSERT_EQ((long)g_render_dec_val[7], (long)0xFFFF8002u);
}

/* team flag: enemy (team 0) blits sprite 0x36, player/npc blits 0x35, at
 * surface offset +0x25E5. The bar blits now precede it in g_blitraw, so the
 * flag is located by its destination (exactly one blit targets +0x25E5, and
 * no status-icon blits exist because all status bytes are zero). */
static void test_panel_team_flag_sprite(void)
{
    runtime_char *rc;
    uint32 sheet;
    uint32 buf = 0x400000;

    sheet = bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    rc->team = 0;                    /* enemy */
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    ASSERT_EQ((long)panel_count_blit_at(buf + 0x25e5), 1);
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x25e5) - sheet), 0x36);
    /* no status-icon blits when all status bytes are zero */
    ASSERT_EQ((long)panel_count_blit_at(buf + 0x55c2), 0);

    /* player team -> 0x35 */
    rc->team = 2;
    panel_reset_logs();
    fd2_render_full_char_stat_panel(0, buf);
    ASSERT_EQ((long)panel_count_blit_at(buf + 0x25e5), 1);
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x25e5) - sheet), 0x35);
}

/* status-icon loop: for i=0..2, when the byte at struct offset 0x25+i
 * (status_flags_block[4], status_sleep_flag, combat_aux_block[0]) is non-zero,
 * blit sprite 0x37+i at +0x55C2 + i*0x23. Zero bytes are skipped. The team
 * flag blit precedes the icons. Here slots 0 and 2 are set, slot 1 clear. */
static void test_panel_status_icons_overflow_walk(void)
{
    runtime_char *rc;
    uint32 sheet;
    uint32 buf = 0x440000;

    sheet = bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    rc->team = 2;                      /* player -> team flag 0x35 first */
    rc->status_flags_block[4] = 0x09;  /* offset 0x25 -> icon 0 (0x37) */
    rc->status_sleep_flag     = 0x00;  /* offset 0x26 -> skipped */
    rc->combat_aux_block[0]   = 0x07;  /* offset 0x27 -> icon 2 (0x39) */
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    /* team flag 0x35 @ +0x25E5 */
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x25e5) - sheet), 0x35);
    /* icon slot 0 (byte 0x25 set): sprite 0x37 @ +0x55C2 */
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x55c2) - sheet), 0x37);
    /* icon slot 1 (byte 0x26 clear): no blit at +0x55C2 + 0x23 */
    ASSERT_EQ((long)panel_count_blit_at(buf + 0x55c2 + 0x23), 0);
    /* icon slot 2 (byte 0x27 set): sprite 0x39 @ +0x55C2 + 2*0x23 */
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x55c2 + 2 * 0x23) - sheet),
              0x39);
}

/* all three status-icon bytes non-zero -> all three icons paint, in order. */
static void test_panel_status_icons_all_three(void)
{
    runtime_char *rc;
    uint32 sheet;
    uint32 buf = 0x480000;

    sheet = bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    rc->team = 2;
    rc->status_flags_block[4] = 1;
    rc->status_sleep_flag     = 1;
    rc->combat_aux_block[0]   = 1;
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    /* all three icon slots paint, each at its own +0x55C2 + i*0x23 offset */
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x55c2) - sheet), 0x37);
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x55c2 + 0x23) - sheet), 0x38);
    ASSERT_EQ((long)(panel_find_blit_sprite(buf + 0x55c2 + 2 * 0x23) - sheet),
              0x39);
}

/* ----------------------------------------------------------------
 * fd2_paint_status_panel_layer_left @ 0x182ad
 *
 * Drives the REAL function -> REAL memmove over two in-memory mode13h-sized
 * buffers (64000 bytes each). The function copies 0x56 rows x 0x56 bytes of
 * the left panel with horizontal-shift clipping:
 *   row r:  dst + 0x8C0 + x_off' + r*0x140  <-  src + 0x8C5 + skip + r*0x140
 * where (x_off', skip, row_bytes) = (x_offset, 0, 0x56) when x_offset >= 0,
 * else (0, -x_offset, x_offset + 0x56).
 *
 * The source is filled so byte at source index k equals (k & 0xFF); each
 * destination byte is then checked against the source index it was copied
 * from, and the bytes flanking each copied window are checked untouched.
 * ---------------------------------------------------------------- */
#define PANEL_BUF_BYTES 64000
#define PANEL_ROWS      0x56
#define PANEL_STRIDE    0x140
#define PANEL_DST_BASE  0x8c0
#define PANEL_SRC_BASE  0x8c5

static uint8 g_panel_src[PANEL_BUF_BYTES];
static uint8 g_panel_dst[PANEL_BUF_BYTES];

static void panel_left_setup(void)
{
    int i;

    for (i = 0; i < PANEL_BUF_BYTES; i++) {
        g_panel_src[i] = (uint8)(i & 0xFF);
        g_panel_dst[i] = 0xAA;             /* sentinel: "not written" */
    }
}

/* Verify every copied byte and the immediate flanks for one configuration.
 * x_off_in is the signed argument; the expected per-row geometry is recomputed
 * here independently of the function under test. */
static void panel_left_check(int32 x_off_in)
{
    uint32 src_base = (uint32)g_panel_src;
    uint32 dst_base = (uint32)g_panel_dst;
    int    row;
    int    col;
    int    row_bytes;
    int    src_skip;
    int    x_off;

    row_bytes = 0x56;
    src_skip  = 0;
    x_off     = x_off_in;
    if (x_off_in < 0) {
        row_bytes = x_off_in + 0x56;
        src_skip  = -x_off_in;
        x_off     = 0;
    }

    fd2_paint_status_panel_layer_left((uint32)x_off_in, dst_base, src_base);

    for (row = 0; row < PANEL_ROWS; row++) {
        uint32 d0 = PANEL_DST_BASE + x_off + row * PANEL_STRIDE;
        uint32 s0 = PANEL_SRC_BASE + src_skip + row * PANEL_STRIDE;

        /* byte just before the copied window stays at the sentinel */
        ASSERT_EQ((long)g_panel_dst[d0 - 1], 0xAA);
        for (col = 0; col < row_bytes; col++) {
            /* each dst byte equals the source byte it was copied from */
            ASSERT_EQ((long)g_panel_dst[d0 + col],
                      (long)(uint8)((s0 + col) & 0xFF));
        }
        /* byte just after the copied window stays at the sentinel */
        ASSERT_EQ((long)g_panel_dst[d0 + row_bytes], 0xAA);
    }
}

/* x_offset >= 0 (in-place, x = 5 as the outro step uses for frames < 6):
 * full 0x56-byte rows, no source skip, dst x = 5. */
static void test_panel_left_inplace_positive(void)
{
    panel_left_setup();
    panel_left_check(5);
}

/* x_offset == 0 edge of the non-clip branch: still full rows, dst x = 0. */
static void test_panel_left_zero_offset(void)
{
    panel_left_setup();
    panel_left_check(0);
}

/* moderate negative shift (x = -0x10): row_bytes shrinks to 0x46, source
 * advances by 0x10, dst x clamps to 0. */
static void test_panel_left_clip_moderate(void)
{
    panel_left_setup();
    panel_left_check(-0x10);
}

/* extreme negative shift (x = -0x4B, the real frame-11 value emitted by
 * fd2_play_status_screen_outro_step: 5 - (11*16 - 0x60) = -75): row_bytes
 * narrows to 0xB, source advances by 0x4B, dst x = 0. This is the largest
 * |shift| the caller ever produces, so row_bytes stays a valid small count. */
static void test_panel_left_clip_extreme(void)
{
    panel_left_setup();
    panel_left_check(-0x4B);
}

/* Loop bound: exactly 0x56 rows copied. Row 0x55 (last) is written; the
 * region where a hypothetical row 0x56 would land must remain untouched. */
static void test_panel_left_row_count_bound(void)
{
    uint32 last_d0;
    uint32 past_d0;

    panel_left_setup();
    fd2_paint_status_panel_layer_left(5, (uint32)g_panel_dst,
                                      (uint32)g_panel_src);

    /* last copied row (0x55) wrote its first byte */
    last_d0 = PANEL_DST_BASE + 5 + 0x55 * PANEL_STRIDE;
    ASSERT_EQ((long)g_panel_dst[last_d0],
              (long)(uint8)((PANEL_SRC_BASE + 0x55 * PANEL_STRIDE) & 0xFF));

    /* one row past the end (0x56) must be entirely sentinel */
    past_d0 = PANEL_DST_BASE + 5 + 0x56 * PANEL_STRIDE;
    ASSERT_EQ((long)g_panel_dst[past_d0], 0xAA);
    ASSERT_EQ((long)g_panel_dst[past_d0 + 0x55], 0xAA);
}

/* ----------------------------------------------------------------
 * fd2_paint_status_panel_layer_right @ 0x18312
 *
 * Sister of _left, but the panel shifts VERTICALLY: it copies 0x56 rows
 * x 0xDF bytes of the right panel into the workspace, with the y_offset
 * applied as a per-row vertical destination shift (not a horizontal byte
 * offset). Per row r:
 *   dst + 0x5C  + (y_off' + r)*0x140  <-  src + 0x91C + (skip + r)*0x140
 * where (y_off', skip, row_count) = (y_offset, 0, 0x56) when y_offset >= 0,
 * else (0, -y_offset, y_offset + 0x56).
 *
 * Reuses g_panel_src/g_panel_dst (64000 bytes each, src[k] = k & 0xFF).
 * Each destination byte is checked against the source index it was copied
 * from, and the byte flanking each copied window on the left is checked
 * untouched. Geometry is recomputed here independently of the function.
 * ---------------------------------------------------------------- */
#define PANELR_WIDTH    0xDF
#define PANELR_DST_BASE 0x5c
#define PANELR_SRC_BASE 0x91c

/* Verify every copied byte (and the left flank) for one y_offset config. */
static void panel_right_check(int32 y_off_in)
{
    uint32 src_base = (uint32)g_panel_src;
    uint32 dst_base = (uint32)g_panel_dst;
    int    row;
    int    col;
    int    row_count;
    int    src_skip;
    int    y_off;

    row_count = 0x56;
    src_skip  = 0;
    y_off     = y_off_in;
    if (y_off_in < 0) {
        row_count = y_off_in + 0x56;
        src_skip  = -y_off_in;
        y_off     = 0;
    }

    fd2_paint_status_panel_layer_right((uint32)y_off_in, dst_base, src_base);

    for (row = 0; row < row_count; row++) {
        /* vertical shift: the row index carries both the per-row stride AND
         * the (clamped) y_off / src_skip — distinct from _left's byte shift */
        uint32 d0 = PANELR_DST_BASE + (y_off + row) * PANEL_STRIDE;
        uint32 s0 = PANELR_SRC_BASE + (src_skip + row) * PANEL_STRIDE;

        /* byte just before the copied window stays at the sentinel */
        ASSERT_EQ((long)g_panel_dst[d0 - 1], 0xAA);
        for (col = 0; col < PANELR_WIDTH; col++) {
            ASSERT_EQ((long)g_panel_dst[d0 + col],
                      (long)(uint8)((s0 + col) & 0xFF));
        }
        /* byte just after the copied window stays at the sentinel */
        ASSERT_EQ((long)g_panel_dst[d0 + PANELR_WIDTH], 0xAA);
    }
}

/* y_offset >= 0 (in-place, y = 7 as the outro step uses for frames <= 2):
 * full 0x56 rows, no source-row skip, dst rows start at +7*0x140. */
static void test_panel_right_inplace_positive(void)
{
    panel_left_setup();          /* shared fill: src[k]=k&0xFF, dst=0xAA */
    panel_right_check(7);
}

/* y_offset == 0 edge of the non-clip branch: full rows, dst rows start at 0. */
static void test_panel_right_zero_offset(void)
{
    panel_left_setup();
    panel_right_check(0);
}

/* moderate negative shift (y = -0x10): row_count shrinks to 0x46, the source
 * advances by 0x10 WHOLE ROWS (src_skip*0x140), dst row base clamps to 0. */
static void test_panel_right_clip_moderate(void)
{
    panel_left_setup();
    panel_right_check(-0x10);
}

/* extreme negative shift (y = -0x49, the real frame-8 value emitted by
 * fd2_play_status_screen_outro_step: 7 - (8*16 - 0x30) = -73): row_count
 * narrows to 0x0D, source advances by 0x49 rows, dst row base = 0. Frame 8
 * is the last frame that draws the right panel, so this is the largest
 * |shift| the caller ever produces. */
static void test_panel_right_clip_extreme(void)
{
    panel_left_setup();
    panel_right_check(-0x49);
}

/* Loop bound: exactly 0x56 rows copied. Row 0x55 (last) is written; the
 * region where a hypothetical row 0x56 would land must remain untouched. */
static void test_panel_right_row_count_bound(void)
{
    uint32 last_d0;
    uint32 past_d0;

    panel_left_setup();
    fd2_paint_status_panel_layer_right(7, (uint32)g_panel_dst,
                                       (uint32)g_panel_src);

    /* last copied row (0x55) wrote its first byte from src row 0x55 */
    last_d0 = PANELR_DST_BASE + (7 + 0x55) * PANEL_STRIDE;
    ASSERT_EQ((long)g_panel_dst[last_d0],
              (long)(uint8)((PANELR_SRC_BASE + 0x55 * PANEL_STRIDE) & 0xFF));

    /* one row past the end (0x56) must be entirely sentinel */
    past_d0 = PANELR_DST_BASE + (7 + 0x56) * PANEL_STRIDE;
    ASSERT_EQ((long)g_panel_dst[past_d0], 0xAA);
    ASSERT_EQ((long)g_panel_dst[past_d0 + (PANELR_WIDTH - 1)], 0xAA);
}

/* ----------------------------------------------------------------
 * fd2_render_inventory_item_grid @ 0x184c0
 *
 * Drive the REAL grid renderer and observe its dispatch to:
 *   - fd2_blit_sheet_sprite_at_offset (REAL) -> g_blitraw log: emits the
 *     background icon sprite per drawn slot, plus a value-label sprite for
 *     weapon/armor/HP/MP items. The fake sheet (bar_setup_sheet, table[i]=i)
 *     lets us recover sprite index = logged_sprite - sheet and the dst.
 *   - fd2_render_decimal_number_to_buffer (recording spy, g_render_dec_*):
 *     the numeric value + surface dst + digits for valued items.
 *   - fd2_blit_indexed_sprite_at_xy (REAL) -> fd2_rle_blit_sprite spy
 *     (g_rle_blit_last_sprite): the placeholder dot 0x29 for unrecognized
 *     items. resolved sprite = sheet + table[0x29] = sheet + 0x29.
 *   - fd2_display_dialog_scene (REAL) for the per-slot name label, against
 *     panel_setup_text()'s immediate-END program (returns without fopen and
 *     without blitting, so it produces no g_blitraw entries).
 *
 * Items live in data_fd2_battle_item_effect_table (in-memory .object3 table
 * in testglob.c); the character's inventory lives in g_test_rc_array[0].
 *
 * IMPORTANT: fd2_get_item_effect_entry returns &entry[id].type (struct base
 * + 1), so the renderer's raw item-pointer is offset +1 from the struct. The
 * raw offsets the renderer reads therefore map to struct fields as:
 *   item[0]    = struct.type           (type discriminator)
 *   item[0xD]  = struct +0xE = use_effect    (HP=5 / MP=0xB discriminator)
 *   item+1     = struct +2  = ap        (weapon value, int16)
 *   item+5     = struct +6  = dp        (armor value, int16)
 *   item+0xE   = struct +0xF.. = use_param_lo|use_param_hi  (HP/MP value)
 * Tests fill those struct fields accordingly.
 * ---------------------------------------------------------------- */

/* rle-blit spy (testglob.c): fd2_blit_indexed_sprite_at_xy -> fd2_rle_blit_sprite
 * records the resolved sprite stream + call count for the placeholder path. */
extern int    g_rle_blit_calls;
extern uint32 g_rle_blit_last_sprite;

/* set inventory slot `slot` (0..7) of g_test_rc_array[0]: flag + item id.
 * each slot is 2 bytes [flag,item_id] at inventory_slots[2*slot]. */
static void inv_set_slot(int slot, uint8 flag, uint8 item_id)
{
    g_test_rc_array[0].inventory_slots[slot * 2]     = flag;
    g_test_rc_array[0].inventory_slots[slot * 2 + 1] = item_id;
}

/* reset the whole inventory grid test fixture: zero char, empty all slots,
 * clear the item table, install fake sheet + immediate-END text, arm logs. */
static uint32 inv_setup(void)
{
    uint32 sheet;
    int    i;

    memset(&g_test_rc_array[0], 0, sizeof(g_test_rc_array[0]));
    for (i = 0; i < 8; i++) {
        inv_set_slot(i, 0x80, 0);          /* 0x80 = empty slot */
    }
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(item_effect) * 215);

    sheet = bar_setup_sheet();
    panel_setup_text();

    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    g_render_dec_count = 0;
    g_render_log_on = 1;
    g_rle_blit_calls = 0;
    return sheet;
}

/* all 8 slots empty (flag bit7 set) -> nothing is drawn at all. */
static void test_inv_all_empty_draws_nothing(void)
{
    uint32 buf = 0x100000;

    inv_setup();

    fd2_render_inventory_item_grid(0, -1, buf);

    ASSERT_EQ((long)g_blitraw_count, 0);
    ASSERT_EQ((long)g_render_dec_count, 0);
    ASSERT_EQ((long)g_rle_blit_calls, 0);
}

/* a single weapon item (type < 0x15) in slot 0:
 *   background icon 0x3B (not equipped) at col_x-0x1D + (row_y+0x65)*0x140
 *   value-label sprite 0x40 at col_x+0x44 + (row_y+0x6B)*0x140
 *   decimal value = item->ap (item+1), 3-digit, color 0x2A, at
 *     col_x+0x5D + (row_y+0x6B)*0x140
 * with active_slot_count 0 => col 0 row 0 => col_x 0x2A, row_y 0. */
static void test_inv_weapon_slot0(void)
{
    uint32 buf = 0x100000;
    uint32 sheet;
    uint32 col_x = 0x2a;
    uint32 row_y = 0;

    sheet = inv_setup();
    data_fd2_battle_item_effect_table[7].type = 0x10;   /* weapon */
    data_fd2_battle_item_effect_table[7].ap   = 0x0123; /* value at item+1 */
    inv_set_slot(0, 0x00, 7);

    fd2_render_inventory_item_grid(0, -1, buf);

    /* two sheet blits: [0] bg icon, [1] value label */
    ASSERT_EQ((long)g_blitraw_count, 2);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3B);
    ASSERT_EQ((long)g_blitraw_log_dst[0],
              (long)(buf + col_x - 0x1d + (row_y + 0x65) * 0x140));
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x40);
    ASSERT_EQ((long)g_blitraw_log_dst[1],
              (long)(buf + col_x + 0x44 + (row_y + 0x6b) * 0x140));

    /* the number */
    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_val[0], 0x0123);
    ASSERT_EQ((long)g_render_dec_digits[0], 3);
    ASSERT_EQ((long)g_render_dec_color[0], 0x2a);
    ASSERT_EQ((long)g_render_dec_dst[0],
              (long)(buf + col_x + 0x5d + (row_y + 0x6b) * 0x140));

    /* no placeholder rle blit on the weapon path */
    ASSERT_EQ((long)g_rle_blit_calls, 0);
}

/* equipped flag (bit6) bumps the background icon sprite by +3:
 * weapon 0x3B -> 0x3E. */
static void test_inv_equipped_bg_plus3(void)
{
    uint32 buf = 0x140000;
    uint32 sheet;

    sheet = inv_setup();
    data_fd2_battle_item_effect_table[3].type = 0x05;   /* weapon */
    data_fd2_battle_item_effect_table[3].ap   = 7;
    inv_set_slot(0, 0x40, 3);                            /* equipped */

    fd2_render_inventory_item_grid(0, -1, buf);

    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3E);   /* 0x3B + 3 */
}

/* armor (0x15 <= type < 0x20): background icon 0x3C, value sprite 0x41,
 * value = item->dp (item+5). */
static void test_inv_armor_slot0(void)
{
    uint32 buf = 0x180000;
    uint32 sheet;

    sheet = inv_setup();
    data_fd2_battle_item_effect_table[9].type = 0x18;   /* armor band */
    data_fd2_battle_item_effect_table[9].dp   = 0x0044; /* value at item+5 */
    inv_set_slot(0, 0x00, 9);

    fd2_render_inventory_item_grid(0, -1, buf);

    ASSERT_EQ((long)g_blitraw_count, 2);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3C);   /* bg armor */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x41);   /* label    */
    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_val[0], 0x0044);
    ASSERT_EQ((long)g_render_dec_digits[0], 3);
}

/* HP-restore consumable (type==0x20, item[0xD]==5): bg icon 0x3D, value
 * sprite 0x42, value = *(int16*)(item+0xE) = use_effect | use_param_lo<<8. */
static void test_inv_hp_consumable(void)
{
    uint32 buf = 0x1c0000;
    uint32 sheet;

    sheet = inv_setup();
    data_fd2_battle_item_effect_table[20].type        = 0x20;
    data_fd2_battle_item_effect_table[20].use_effect   = 0x05; /* item[0xD] disc */
    data_fd2_battle_item_effect_table[20].use_param_lo = 0x32; /* item[0xE] lo  */
    data_fd2_battle_item_effect_table[20].use_param_hi = 0x00; /* item[0xF] hi  */
    inv_set_slot(0, 0x00, 20);

    fd2_render_inventory_item_grid(0, -1, buf);

    ASSERT_EQ((long)g_blitraw_count, 2);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3D);   /* bg other */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x42);   /* HP label */
    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_val[0], 0x0032);
}

/* MP-restore consumable (type==0x20, item[0xD]==0xB): bg icon 0x3D, value
 * sprite 0x43, value = *(int16*)(item+0xE). This exercises the dense
 * EDI-recomputed value/label-address block in the binary. */
static void test_inv_mp_consumable(void)
{
    uint32 buf = 0x200000;
    uint32 sheet;
    uint32 col_x = 0x2a;
    uint32 row_y = 0;

    sheet = inv_setup();
    data_fd2_battle_item_effect_table[30].type        = 0x20;
    data_fd2_battle_item_effect_table[30].use_effect   = 0x0b; /* item[0xD] disc */
    data_fd2_battle_item_effect_table[30].use_param_lo = 0x14; /* item[0xE] lo  */
    data_fd2_battle_item_effect_table[30].use_param_hi = 0x00; /* item[0xF] hi  */
    inv_set_slot(0, 0x00, 30);

    fd2_render_inventory_item_grid(0, -1, buf);

    ASSERT_EQ((long)g_blitraw_count, 2);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3D);   /* bg other */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x43);   /* MP label */
    /* the recomputed value/label addresses match the common formula */
    ASSERT_EQ((long)g_blitraw_log_dst[1],
              (long)(buf + col_x + 0x44 + (row_y + 0x6b) * 0x140));
    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_val[0], 0x0014);
    ASSERT_EQ((long)g_render_dec_dst[0],
              (long)(buf + col_x + 0x5d + (row_y + 0x6b) * 0x140));
}

/* unrecognized item (type==0x20 but item[0xD] neither 5 nor 0xB): background
 * icon 0x3D, NO value sprite via the sheet, placeholder dot 0x29 via the rle
 * path, and NO decimal number. The slot still counts (verified separately). */
static void test_inv_placeholder_other(void)
{
    uint32 buf = 0x240000;
    uint32 sheet;

    sheet = inv_setup();
    data_fd2_battle_item_effect_table[40].type       = 0x20;
    data_fd2_battle_item_effect_table[40].use_effect = 0x01;  /* item[0xD] != 5/0xB */
    inv_set_slot(0, 0x00, 40);

    fd2_render_inventory_item_grid(0, -1, buf);

    /* only the background icon goes through the sheet blit */
    ASSERT_EQ((long)g_blitraw_count, 1);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3D);
    /* placeholder dot via rle path, resolved sprite = sheet + table[0x29] */
    ASSERT_EQ((long)g_rle_blit_calls, 1);
    ASSERT_EQ((long)(g_rle_blit_last_sprite - sheet), 0x29);
    /* no number */
    ASSERT_EQ((long)g_render_dec_count, 0);
}

/* type boundary: type 0x14 is still a weapon (< 0x15 -> 0x3B/0x40), type 0x15
 * is armor (< 0x20 -> 0x3C/0x41), type 0x1F is armor, type 0x20 with bad sub
 * is placeholder. Two separate single-item renders pin the < 0x15 / < 0x20
 * edges. */
static void test_inv_type_boundaries(void)
{
    uint32 buf = 0x280000;
    uint32 sheet;

    /* type 0x14 -> weapon */
    sheet = inv_setup();
    data_fd2_battle_item_effect_table[5].type = 0x14;
    inv_set_slot(0, 0x00, 5);
    fd2_render_inventory_item_grid(0, -1, buf);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3B);
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x40);

    /* type 0x15 -> armor */
    sheet = inv_setup();
    data_fd2_battle_item_effect_table[5].type = 0x15;
    inv_set_slot(0, 0x00, 5);
    fd2_render_inventory_item_grid(0, -1, buf);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3C);
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x41);

    /* type 0x1F -> still armor */
    sheet = inv_setup();
    data_fd2_battle_item_effect_table[5].type = 0x1F;
    inv_set_slot(0, 0x00, 5);
    fd2_render_inventory_item_grid(0, -1, buf);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3C);
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x41);
}

/* value sign-extension: the value is read with MOVSX (signed 16-bit). A
 * weapon ap of 0x8001 must arrive at the decimal renderer as 0xFFFF8001. */
static void test_inv_value_sign_extension(void)
{
    uint32 buf = 0x2c0000;

    inv_setup();
    data_fd2_battle_item_effect_table[5].type = 0x01;
    data_fd2_battle_item_effect_table[5].ap   = 0x8001;
    inv_set_slot(0, 0x00, 5);

    fd2_render_inventory_item_grid(0, -1, buf);

    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_val[0], (long)0xFFFF8001u);
}

/* empty slots are skipped without consuming a grid cell: with slot 0 empty
 * and slot 3 holding a weapon, the (only) drawn item still lands in grid cell
 * 0 (active_slot_count 0 -> col_x 0x2A, row_y 0), NOT cell 3. */
static void test_inv_empty_slots_skipped_packing(void)
{
    uint32 buf = 0x300000;
    uint32 sheet;
    uint32 col_x = 0x2a;
    uint32 row_y = 0;

    sheet = inv_setup();
    /* slots 0,1,2 empty (already 0x80 from inv_setup); slot 3 holds a weapon */
    data_fd2_battle_item_effect_table[5].type = 0x01;
    data_fd2_battle_item_effect_table[5].ap   = 11;
    inv_set_slot(3, 0x00, 5);

    fd2_render_inventory_item_grid(0, -1, buf);

    ASSERT_EQ((long)g_blitraw_count, 2);
    /* drawn at packed cell 0, proving empties did not advance the counter */
    ASSERT_EQ((long)g_blitraw_log_dst[0],
              (long)(buf + col_x - 0x1d + (row_y + 0x65) * 0x140));
}

/* grid packing across cells: five weapons in slots 0..4 pack into cells
 * 0,1,2,3,4 -> (col,row) (0,0)(0,1)(0,2)(0,3)(1,0). Verify the background-icon
 * dst of the 5th drawn item lands at col 1 row 0:
 *   col_x = 1*0x96 + 0x2A = 0xC0, row_y = 0. Each drawn slot emits 2 sheet
 * blits (bg + value), so the 5th item's bg icon is g_blitraw entry index 8. */
static void test_inv_grid_packing_cells(void)
{
    uint32 buf = 0x340000;
    uint32 sheet;
    int    i;
    uint32 c4_col_x = 1 * 0x96 + 0x2a;
    uint32 c4_row_y = 0;

    sheet = inv_setup();
    data_fd2_battle_item_effect_table[5].type = 0x01;   /* weapon */
    data_fd2_battle_item_effect_table[5].ap   = 1;
    for (i = 0; i < 5; i++) {
        inv_set_slot(i, 0x00, 5);
    }

    fd2_render_inventory_item_grid(0, -1, buf);

    /* 5 drawn slots x 2 sheet blits = 10 */
    ASSERT_EQ((long)g_blitraw_count, 10);

    /* cell 1 (active_slot_count 1) bg dst: col 0 row 1 */
    ASSERT_EQ((long)g_blitraw_log_dst[2],
              (long)(buf + 0x2a - 0x1d + (1 * 0x16 + 0x65) * 0x140));
    /* cell 4 (active_slot_count 4) bg dst: col 1 row 0 */
    ASSERT_EQ((long)g_blitraw_log_dst[8],
              (long)(buf + c4_col_x - 0x1d + (c4_row_y + 0x65) * 0x140));
}

/* placeholder items STILL consume a grid cell: an unrecognized item in slot 0
 * followed by a weapon in slot 1 must place the weapon in cell 1 (not cell 0),
 * proving active_slot_count was incremented on the placeholder path. */
static void test_inv_placeholder_still_counts(void)
{
    uint32 buf = 0x380000;
    uint32 sheet;

    sheet = inv_setup();
    /* slot 0: unrecognized (type 0x20, bad sub) -> placeholder */
    data_fd2_battle_item_effect_table[40].type       = 0x20;
    data_fd2_battle_item_effect_table[40].use_effect = 0x01;
    inv_set_slot(0, 0x00, 40);
    /* slot 1: weapon */
    data_fd2_battle_item_effect_table[5].type = 0x01;
    data_fd2_battle_item_effect_table[5].ap   = 9;
    inv_set_slot(1, 0x00, 5);

    fd2_render_inventory_item_grid(0, -1, buf);

    /* blit order: slot0 bg(0x3D), slot1 bg(0x3B), slot1 value(0x40) = 3 sheet
     * blits; slot0 also did one rle placeholder. */
    ASSERT_EQ((long)g_blitraw_count, 3);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3D);   /* slot0 ph bg */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x3B);   /* slot1 wp bg */
    /* slot1 weapon bg landed in cell 1 (col 0 row 1), proving the placeholder
     * advanced active_slot_count from 0 to 1. */
    ASSERT_EQ((long)g_blitraw_log_dst[1],
              (long)(buf + 0x2a - 0x1d + (1 * 0x16 + 0x65) * 0x140));
    ASSERT_EQ((long)g_rle_blit_calls, 1);
}

/* ----------------------------------------------------------------
 * fd2_render_number_red_when_full @ 0x1875d
 *
 * Thin wrapper: color = (current == max) ? 0x1F : 0x2A, then forward
 * (dst, pitch, current, color, digits) to fd2_render_decimal_number_to_buffer.
 * Driven directly here and observed through the decimal recording spy
 * (g_render_dec_*). Risk-based coverage: both color branches plus exact
 * pass-through of dst / value / digits (and that the forwarded value is
 * `current`, never `max`).
 * ---------------------------------------------------------------- */
static void redfull_reset(void)
{
    g_render_dec_count = 0;
    g_render_log_on = 1;
}

/* current == max -> red glow 0x1F; current is the value drawn, dst/digits
 * are forwarded verbatim. */
static void test_redfull_equal_is_red(void)
{
    redfull_reset();
    fd2_render_number_red_when_full(0x1234, 0x140, 0x64, 0x64, 3);

    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_dst[0],    0x1234);
    ASSERT_EQ((long)g_render_dec_val[0],    0x64);   /* value = current */
    ASSERT_EQ((long)g_render_dec_color[0],  0x1f);   /* red */
    ASSERT_EQ((long)g_render_dec_digits[0], 3);
}

/* current < max -> white 0x2A, and value is current (not max). */
static void test_redfull_below_is_white(void)
{
    redfull_reset();
    fd2_render_number_red_when_full(0x5678, 0x140, 0x50, 0x64, 3);

    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_dst[0],    0x5678);
    ASSERT_EQ((long)g_render_dec_val[0],    0x50);   /* current, NOT max */
    ASSERT_EQ((long)g_render_dec_color[0],  0x2a);   /* white */
    ASSERT_EQ((long)g_render_dec_digits[0], 3);
}

/* current > max (current need not be capped) -> still not equal -> white. */
static void test_redfull_above_is_white(void)
{
    redfull_reset();
    fd2_render_number_red_when_full(0x9abc, 0x140, 0x70, 0x64, 2);

    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_val[0],    0x70);
    ASSERT_EQ((long)g_render_dec_color[0],  0x2a);   /* white */
    ASSERT_EQ((long)g_render_dec_digits[0], 2);      /* digits forwarded */
}

/* equality is a full 32-bit compare (CMP of two dwords): two large values
 * that match only in their low 16 bits must NOT be treated as equal. */
static void test_redfull_full_width_compare(void)
{
    redfull_reset();
    /* low 16 bits both 0x0000 but high halves differ -> not equal -> white */
    fd2_render_number_red_when_full(0x10, 0x140, 0x00010000u, 0x00020000u, 3);
    ASSERT_EQ((long)g_render_dec_color[0], 0x2a);

    /* exact 32-bit match -> red */
    redfull_reset();
    fd2_render_number_red_when_full(0x10, 0x140, 0x00020000u, 0x00020000u, 3);
    ASSERT_EQ((long)g_render_dec_color[0], 0x1f);
}

/* zero == zero counts as "full" (red) — boundary where both are 0. */
static void test_redfull_zero_equal_is_red(void)
{
    redfull_reset();
    fd2_render_number_red_when_full(0x20, 0x140, 0, 0, 3);
    ASSERT_EQ((long)g_render_dec_count, 1);
    ASSERT_EQ((long)g_render_dec_val[0],   0);
    ASSERT_EQ((long)g_render_dec_color[0], 0x1f);
}

/* ----------------------------------------------------------------
 * fd2_render_hp_or_mp_bar_proportional @ 0x18795
 *
 * Compute segments from (current/max) and dispatch to the REAL
 * fd2_render_horizontal_bar_segments -> fd2_blit_sheet_sprite_at_offset
 * pipeline, observed through the g_blitraw_* sprite log (fake sheet via
 * bar_setup_sheet so resolved sprite = sheet + sprite_index, and the per-blit
 * dst is the segment destination offset).
 *
 *   segments = 0                       -> empty bar: 101 middle 0x1D @ off+1.. +
 *                                          empty cap 0x1E @ off+0x66 (102 blits)
 *   segments = N (>=1, filled)         -> left cap(base) @ off, N-1 middles,
 *                                          right cap(base+2) @ off+N
 * The right-cap offset (off + N) pins the exact segment count, which encodes
 * the (current,max) the function computed.
 *
 *   max == 0      -> div-by-zero guard: nothing drawn (0 blits)
 *   current == 0  -> segments = 0 (empty bar)
 *   else          -> segments = (current * 0x65) / max + 1  (SIGNED)
 * ---------------------------------------------------------------- */

/* max == 0: the guard returns before any dispatch -> zero blits. */
static void test_prop_zero_max_draws_nothing(void)
{
    bar_setup_sheet();
    bar_reset();

    fd2_render_hp_or_mp_bar_proportional(0x1000, 0x140, 0x17, 0x40, 0);

    ASSERT_EQ((long)g_blitraw_count, 0);
}

/* current == 0 (max != 0): segments = 0 -> the empty-bar pattern (102 blits,
 * 101 middle 0x1D then empty cap 0x1E at off+0x66). */
static void test_prop_zero_current_empty_bar(void)
{
    uint32 sheet;
    uint32 off = 0x1000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_hp_or_mp_bar_proportional(off, 0x140, 0x17, 0, 0x64);

    ASSERT_EQ((long)g_blitraw_count, 102);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x1D);   /* first middle */
    ASSERT_EQ((long)(g_blitraw_log_sprite[101] - sheet), 0x1E); /* empty cap   */
    ASSERT_EQ((long)g_blitraw_log_dst[101], (long)(off + 0x66));
}

/* current == max: segments = (max*0x65)/max + 1 = 0x65 + 1 = 0x66 (102).
 * Fully-filled bar: left cap(0x17) @ off, 101 middles, right cap(0x19) @
 * off+0x66 (103 blits total). The right cap at off+0x66 confirms 102 segments,
 * i.e. the maximum fill plus the +1. */
static void test_prop_full_bar_max_segments(void)
{
    uint32 sheet;
    uint32 off = 0x2000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_hp_or_mp_bar_proportional(off, 0x140, 0x17, 0x64, 0x64);

    ASSERT_EQ((long)g_blitraw_count, 103);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x17);   /* left cap  */
    ASSERT_EQ((long)g_blitraw_log_dst[0], (long)off);
    ASSERT_EQ((long)(g_blitraw_log_sprite[102] - sheet), 0x19); /* right cap */
    ASSERT_EQ((long)g_blitraw_log_dst[102], (long)(off + 0x66));
}

/* proportional mid value: cur 0x32 (50), max 0x64 (100) ->
 * segments = (50*101)/100 + 1 = 5050/100 + 1 = 50 + 1 = 51. Right cap @ off+51.
 * (104 blits: left cap + 50 middles + right cap = 52.) */
static void test_prop_half_value_segment_count(void)
{
    uint32 sheet;
    uint32 off = 0x3000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_hp_or_mp_bar_proportional(off, 0x140, 0x17, 0x32, 0x64);

    ASSERT_EQ((long)g_blitraw_count, 52);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x17);    /* left cap  */
    ASSERT_EQ((long)(g_blitraw_log_sprite[51] - sheet), 0x19);   /* right cap */
    ASSERT_EQ((long)g_blitraw_log_dst[51], (long)(off + 51));
}

/* +1 minimum sliver: any non-zero current yields at least 1 segment. With
 * cur 1, max 10000: (1*0x65)/10000 = 0, +1 = 1 -> single-segment bar
 * (left cap + right cap @ off+1, 2 blits). Proves the +1 floor. */
static void test_prop_min_one_segment_floor(void)
{
    uint32 sheet;
    uint32 off = 0x4000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_hp_or_mp_bar_proportional(off, 0x140, 0x17, 1, 10000);

    ASSERT_EQ((long)g_blitraw_count, 2);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x17);    /* left cap  */
    ASSERT_EQ((long)g_blitraw_log_dst[0], (long)off);
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x19);    /* right cap */
    ASSERT_EQ((long)g_blitraw_log_dst[1], (long)(off + 1));
}

/* sprite_base routing: base 0x1A (MP theme) -> caps 0x1A / 0x1C. cur 0x10,
 * max 0x20 -> segments = (16*101)/32 + 1 = 1616/32 + 1 = 50 + 1 = 51. */
static void test_prop_sprite_base_routing(void)
{
    uint32 sheet;
    uint32 off = 0x5000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_hp_or_mp_bar_proportional(off, 0x140, 0x1A, 0x10, 0x20);

    ASSERT_EQ((long)g_blitraw_count, 52);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x1A);    /* left cap  */
    ASSERT_EQ((long)(g_blitraw_log_sprite[51] - sheet), 0x1C);   /* right cap */
    ASSERT_EQ((long)g_blitraw_log_dst[51], (long)(off + 51));
}

/* SIGNED division (binary uses IMUL/SAR EDX,0x1F/IDIV): a current of
 * 0xFFFFFFFF (= -1 as int32, but != 0 so it skips the empty branch) computes
 * (-1*0x65)/0x64 + 1 = -101/100 + 1 = -1 + 1 = 0 -> segments 0 -> empty bar.
 * Unsigned division would instead produce a huge positive count, so the
 * empty-bar pattern proves the arithmetic is signed. */
static void test_prop_signed_division(void)
{
    uint32 sheet;
    uint32 off = 0x6000;

    sheet = bar_setup_sheet();
    bar_reset();

    fd2_render_hp_or_mp_bar_proportional(off, 0x140, 0x17, 0xFFFFFFFFu, 0x64);

    ASSERT_EQ((long)g_blitraw_count, 102);                        /* empty bar */
    ASSERT_EQ((long)(g_blitraw_log_sprite[101] - sheet), 0x1E);   /* empty cap */
    ASSERT_EQ((long)g_blitraw_log_dst[101], (long)(off + 0x66));
}

void run_gfx_rndstat_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/rndstat\n");
    RUN_TEST(test_normal_path_left_slot);
    RUN_TEST(test_mirrored_path_right_slot);
    RUN_TEST(test_frame_index_selects_int_entry);
    RUN_TEST(test_normal_path_zero_offset);
    RUN_TEST(test_empty_bar_segments);
    RUN_TEST(test_single_segment_bar);
    RUN_TEST(test_filled_bar_three_segments);
    RUN_TEST(test_mp_bar_theme_base);
    RUN_TEST(test_panel_bars_and_full_numbers);
    RUN_TEST(test_panel_decimal_numbers_unboosted);
    RUN_TEST(test_panel_boost_colors_independent);
    RUN_TEST(test_panel_evade_shares_dx_color);
    RUN_TEST(test_panel_stat_sign_extension);
    RUN_TEST(test_panel_team_flag_sprite);
    RUN_TEST(test_panel_status_icons_overflow_walk);
    RUN_TEST(test_panel_status_icons_all_three);
    RUN_TEST(test_panel_left_inplace_positive);
    RUN_TEST(test_panel_left_zero_offset);
    RUN_TEST(test_panel_left_clip_moderate);
    RUN_TEST(test_panel_left_clip_extreme);
    RUN_TEST(test_panel_left_row_count_bound);
    RUN_TEST(test_panel_right_inplace_positive);
    RUN_TEST(test_panel_right_zero_offset);
    RUN_TEST(test_panel_right_clip_moderate);
    RUN_TEST(test_panel_right_clip_extreme);
    RUN_TEST(test_panel_right_row_count_bound);
    RUN_TEST(test_inv_all_empty_draws_nothing);
    RUN_TEST(test_inv_weapon_slot0);
    RUN_TEST(test_inv_equipped_bg_plus3);
    RUN_TEST(test_inv_armor_slot0);
    RUN_TEST(test_inv_hp_consumable);
    RUN_TEST(test_inv_mp_consumable);
    RUN_TEST(test_inv_placeholder_other);
    RUN_TEST(test_inv_type_boundaries);
    RUN_TEST(test_inv_value_sign_extension);
    RUN_TEST(test_inv_empty_slots_skipped_packing);
    RUN_TEST(test_inv_grid_packing_cells);
    RUN_TEST(test_inv_placeholder_still_counts);
    RUN_TEST(test_redfull_equal_is_red);
    RUN_TEST(test_redfull_below_is_white);
    RUN_TEST(test_redfull_above_is_white);
    RUN_TEST(test_redfull_full_width_compare);
    RUN_TEST(test_redfull_zero_equal_is_red);
    RUN_TEST(test_prop_zero_max_draws_nothing);
    RUN_TEST(test_prop_zero_current_empty_bar);
    RUN_TEST(test_prop_full_bar_max_segments);
    RUN_TEST(test_prop_half_value_segment_count);
    RUN_TEST(test_prop_min_one_segment_floor);
    RUN_TEST(test_prop_sprite_base_routing);
    RUN_TEST(test_prop_signed_division);
    g_render_log_on = 0;
    g_blitraw_log_on = 0;
    printf("\n");
}
