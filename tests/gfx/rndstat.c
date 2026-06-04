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
 *   - fd2_render_number_red_when_full       (recording spy, testglob)
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
extern int    g_render_bar_count;
extern uint32 g_render_bar_dst[8];
extern uint32 g_render_bar_base[8];
extern uint32 g_render_bar_cur[8];
extern uint32 g_render_bar_max[8];
extern int    g_render_red_count;
extern uint32 g_render_red_dst[8];
extern uint32 g_render_red_cur[8];
extern uint32 g_render_red_max[8];
extern uint32 g_render_red_digits[8];

extern runtime_char g_test_rc_array[8];

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
    g_render_bar_count = 0;
    g_render_red_count = 0;
    g_blitraw_count = 0;
    g_render_log_on = 1;
    g_blitraw_log_on = 1;
}

/* HP/MP bars + the 4 red-when-full numbers carry the exact (sign-extended)
 * stat values to the proper surface offsets and digit widths. */
static void test_panel_bars_and_full_numbers(void)
{
    runtime_char *rc;
    uint32 buf = 0x100000;

    bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    /* two bars: HP (base 0x17) then MP (base 0x1A) */
    ASSERT_EQ((long)g_render_bar_count, 2);
    ASSERT_EQ((long)g_render_bar_dst[0],  (long)(buf + 0x2a06));
    ASSERT_EQ((long)g_render_bar_base[0], 0x17);
    ASSERT_EQ((long)g_render_bar_cur[0],  0x50);
    ASSERT_EQ((long)g_render_bar_max[0],  0x64);
    ASSERT_EQ((long)g_render_bar_dst[1],  (long)(buf + 0x41c6));
    ASSERT_EQ((long)g_render_bar_base[1], 0x1a);
    ASSERT_EQ((long)g_render_bar_cur[1],  0x10);
    ASSERT_EQ((long)g_render_bar_max[1],  0x20);

    /* four red-when-full numbers: HP cur/max then MP cur/max, all 3-digit.
     * the "max" variants pass current==max so the wrapper paints them red. */
    ASSERT_EQ((long)g_render_red_count, 4);
    ASSERT_EQ((long)g_render_red_dst[0], (long)(buf + 0x344b));
    ASSERT_EQ((long)g_render_red_cur[0], 0x50);
    ASSERT_EQ((long)g_render_red_max[0], 0x64);
    ASSERT_EQ((long)g_render_red_digits[0], 3);
    ASSERT_EQ((long)g_render_red_dst[1], (long)(buf + 0x3465));
    ASSERT_EQ((long)g_render_red_cur[1], 0x64);
    ASSERT_EQ((long)g_render_red_max[1], 0x64);
    ASSERT_EQ((long)g_render_red_dst[2], (long)(buf + 0x4acb));
    ASSERT_EQ((long)g_render_red_cur[2], 0x10);
    ASSERT_EQ((long)g_render_red_max[2], 0x20);
    ASSERT_EQ((long)g_render_red_dst[3], (long)(buf + 0x4ae5));
    ASSERT_EQ((long)g_render_red_cur[3], 0x20);
    ASSERT_EQ((long)g_render_red_max[3], 0x20);
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

    ASSERT_EQ((long)g_render_dec_count, 8);

    /* [0] level (2-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[0], (long)(buf + 0x29dd));
    ASSERT_EQ((long)g_render_dec_val[0], 0x0A);
    ASSERT_EQ((long)g_render_dec_color[0], 0x2a);
    ASSERT_EQ((long)g_render_dec_digits[0], 2);
    /* [1] movement (2-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[1], (long)(buf + 0x379d));
    ASSERT_EQ((long)g_render_dec_val[1], 0x05);
    ASSERT_EQ((long)g_render_dec_digits[1], 2);
    /* [2] magic resist (2-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[2], (long)(buf + 0x455d));
    ASSERT_EQ((long)g_render_dec_val[2], 0x07);
    ASSERT_EQ((long)g_render_dec_digits[2], 2);
    /* [3] AP (3-digit, white because boost flag clear) */
    ASSERT_EQ((long)g_render_dec_dst[3], (long)(buf + 0x545d));
    ASSERT_EQ((long)g_render_dec_val[3], 0x11);
    ASSERT_EQ((long)g_render_dec_color[3], 0x2a);
    ASSERT_EQ((long)g_render_dec_digits[3], 3);
    /* [4] DP (3-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[4], (long)(buf + 0x635d));
    ASSERT_EQ((long)g_render_dec_val[4], 0x22);
    ASSERT_EQ((long)g_render_dec_color[4], 0x2a);
    /* [5] DX base (3-digit, ALWAYS white) — word at dx_block[1] = 0x0055 */
    ASSERT_EQ((long)g_render_dec_dst[5], (long)(buf + 0x4535));
    ASSERT_EQ((long)g_render_dec_val[5], 0x55);
    ASSERT_EQ((long)g_render_dec_color[5], 0x2a);
    /* [6] DX current (3-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[6], (long)(buf + 0x5435));
    ASSERT_EQ((long)g_render_dec_val[6], 0x33);
    ASSERT_EQ((long)g_render_dec_color[6], 0x2a);
    /* [7] Evade (3-digit, white) */
    ASSERT_EQ((long)g_render_dec_dst[7], (long)(buf + 0x6335));
    ASSERT_EQ((long)g_render_dec_val[7], 0x44);
    ASSERT_EQ((long)g_render_dec_color[7], 0x2a);
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

    ASSERT_EQ((long)g_render_dec_count, 8);
    ASSERT_EQ((long)g_render_dec_color[3], 0x77);  /* AP red   */
    ASSERT_EQ((long)g_render_dec_color[4], 0x77);  /* DP red   */
    ASSERT_EQ((long)g_render_dec_color[5], 0x2a);  /* DX base white */
    ASSERT_EQ((long)g_render_dec_color[6], 0x2a);  /* DX cur white */
    ASSERT_EQ((long)g_render_dec_color[7], 0x2a);  /* Evade white */
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

    ASSERT_EQ((long)g_render_dec_color[3], 0x2a);  /* AP white */
    ASSERT_EQ((long)g_render_dec_color[4], 0x2a);  /* DP white */
    ASSERT_EQ((long)g_render_dec_color[6], 0x77);  /* DX cur red */
    ASSERT_EQ((long)g_render_dec_color[7], 0x77);  /* Evade red (shared) */
}

/* 16-bit stat reads are sign-extended (MOVSX in the binary): a value with
 * bit15 set propagates as 0xFFFFxxxx through the bar/number primitives. */
static void test_panel_stat_sign_extension(void)
{
    runtime_char *rc;
    uint32 buf = 0x380000;

    bar_setup_sheet();
    panel_setup_text();
    rc = panel_setup_char();
    rc->hp_current = 0x8001;         /* (int16)0x8001 = -32767 */
    rc->ap         = 0x8002;
    panel_reset_logs();

    fd2_render_full_char_stat_panel(0, buf);

    /* HP bar current arg = sign-extended hp_current */
    ASSERT_EQ((long)g_render_bar_cur[0], (long)0xFFFF8001u);
    /* HP-current red number likewise sign-extended */
    ASSERT_EQ((long)g_render_red_cur[0], (long)0xFFFF8001u);
    /* AP decimal value sign-extended */
    ASSERT_EQ((long)g_render_dec_val[3], (long)0xFFFF8002u);
}

/* team flag: enemy (team 0) blits sprite 0x36, player/npc blits 0x35, at
 * surface offset +0x25E5. With no status flags set, the team flag is the
 * only blit. */
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

    ASSERT_EQ((long)g_blitraw_count, 1);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x36);
    ASSERT_EQ((long)g_blitraw_log_dst[0], (long)(buf + 0x25e5));

    /* player team -> 0x35 */
    rc->team = 2;
    panel_reset_logs();
    fd2_render_full_char_stat_panel(0, buf);
    ASSERT_EQ((long)g_blitraw_count, 1);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x35);
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

    /* team flag + 2 icons = 3 blits */
    ASSERT_EQ((long)g_blitraw_count, 3);
    /* [0] team flag 0x35 @ +0x25E5 */
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x35);
    ASSERT_EQ((long)g_blitraw_log_dst[0], (long)(buf + 0x25e5));
    /* [1] icon slot 0: sprite 0x37 @ +0x55C2 */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x37);
    ASSERT_EQ((long)g_blitraw_log_dst[1], (long)(buf + 0x55c2));
    /* [2] icon slot 2: sprite 0x39 @ +0x55C2 + 2*0x23 */
    ASSERT_EQ((long)(g_blitraw_log_sprite[2] - sheet), 0x39);
    ASSERT_EQ((long)g_blitraw_log_dst[2], (long)(buf + 0x55c2 + 2 * 0x23));
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

    ASSERT_EQ((long)g_blitraw_count, 4);   /* team flag + 3 icons */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x37);
    ASSERT_EQ((long)g_blitraw_log_dst[1], (long)(buf + 0x55c2));
    ASSERT_EQ((long)(g_blitraw_log_sprite[2] - sheet), 0x38);
    ASSERT_EQ((long)g_blitraw_log_dst[2], (long)(buf + 0x55c2 + 0x23));
    ASSERT_EQ((long)(g_blitraw_log_sprite[3] - sheet), 0x39);
    ASSERT_EQ((long)g_blitraw_log_dst[3], (long)(buf + 0x55c2 + 2 * 0x23));
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
    g_render_log_on = 0;
    g_blitraw_log_on = 0;
    printf("\n");
}
