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

/* ---- shared decimal-number (fd2_render_decimal_number_to_buffer) observation.
 * The real digit renderer blits each glyph through the real
 * fd2_blit_indexed_sprite_at_xy -> fd2_rle_blit_sprite spy. With the fake
 * sheet (bar_setup_sheet, table[i]=i) each call records resolved
 * sprite = sheet + sprite_index in g_rle_blit_log_* (gated by g_rle_blit_log_on)
 * so a glyph's sprite index is (logged_sprite - sheet) and its dst the logged
 * dst. These helpers are shared by the dedicated decimal tests and the panel /
 * inventory / redfull caller tests. */
extern int    g_rle_blit_calls;
extern int    g_rle_blit_log_on;
extern uint32 g_rle_blit_log_sprite[64];
extern uint32 g_rle_blit_log_dst[64];
extern int32  g_rle_blit_log_stride[64];
extern uint32 g_rle_blit_log_palette[64];

static uint32 g_dec_sheet;

static void dec_setup(void)
{
    g_dec_sheet = bar_setup_sheet();   /* table[i] = i; sets sprite-sheet ptr */
    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;
}

/* assert the `digits`-glyph run for a normal (non-overflow) number that the
 * binary renders as "%0.<digits>d" of `value`, drawn at `dst` with sprite
 * base `color`, starting at rle-log index `from`. The caller advances its
 * cursor by `digits` (void return because ASSERT_EQ early-returns void). */
static void dec_assert_number(int from, uint32 dst, uint32 value,
                              uint32 color, uint32 digits)
{
    char fmt[8];
    char s[20];
    int  i;

    fmt[0] = '%'; fmt[1] = '0'; fmt[2] = '.';
    fmt[3] = (char)('0' + digits);
    fmt[4] = 'd'; fmt[5] = '\0';
    sprintf(s, fmt, value);

    for (i = 0; i < (int)digits; i++) {
        ASSERT_EQ((long)(g_rle_blit_log_sprite[from + i] - g_dec_sheet),
                  (long)(color + (uint32)(uint8)s[i] - 0x30));
        ASSERT_EQ((long)g_rle_blit_log_dst[from + i],
                  (long)(dst + (uint32)(i * 6)));
    }
}

/* assert a single overflow/placeholder glyph (sprite index `sprite`) drawn at
 * `dst` at rle-log index `from` (caller advances its cursor by 1). */
static void dec_assert_overflow(int from, uint32 dst, uint32 sprite)
{
    ASSERT_EQ((long)(g_rle_blit_log_sprite[from] - g_dec_sheet), (long)sprite);
    ASSERT_EQ((long)g_rle_blit_log_dst[from], (long)dst);
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
 *   - fd2_render_hp_or_mp_bar_proportional  (REAL) -> g_blitraw bar caps
 *   - fd2_render_number_red_when_full (REAL, src/gfx/rndstat.c) -> forwards
 *     into the REAL fd2_render_decimal_number_to_buffer with a red/white color
 *     chosen by current==max; its 4 numbers' glyphs lead the rle log
 *   - fd2_render_decimal_number_to_buffer (REAL) -> digit glyphs via the
 *     fd2_blit_indexed_sprite_at_xy -> fd2_rle_blit_sprite pipeline, logged in
 *     g_rle_blit_log_* (the 12 panel numbers' glyph runs, in call order)
 *   - fd2_blit_sheet_sprite_at_offset (REAL) -> g_blitraw log (team flag +
 *     status icons + bar caps)
 *   - fd2_display_dialog_scene (REAL) for the 3 text labels, against a
 *     minimal text program whose every page entry points at an immediate
 *     END (-1) opcode so the dialog VM returns at once (no fopen / no wait).
 *
 * The panel indexes data_fd2_battle_runtime_char_array_ptr (= g_test_rc_array,
 * 8 slots) so all tests use slot 0. Decimal numbers do NOT touch g_blitraw;
 * bar caps / team flag / status icons do NOT touch the rle log, so the two
 * logs stay cleanly separated.
 * ---------------------------------------------------------------- */
extern runtime_char g_test_rc_array[8];

/* expected one decimal number: glyph run at `dst`, "%0.<digits>d" of `val`
 * with sprite base `color`. The 12 panel numbers render in this fixed order. */
typedef struct {
    uint32 dst;
    uint32 val;
    uint32 color;
    uint32 digits;
} dec_exp;

/* fill the 12-number expected table the panel renders for panel_setup_char()'s
 * profile at surface base `buf`. Callers override individual entries (boost
 * color / sign-extended value) before panel_assert_numbers(). */
static void panel_fill_expected(dec_exp *e, uint32 buf)
{
    /* 4 red-when-full HP/MP numbers (cur white unless cur==max, max always red
     * since it passes current==max): */
    e[0].dst = buf + 0x344b; e[0].val = 0x50; e[0].color = 0x2a; e[0].digits = 3;
    e[1].dst = buf + 0x3465; e[1].val = 0x64; e[1].color = 0x1f; e[1].digits = 3;
    e[2].dst = buf + 0x4acb; e[2].val = 0x10; e[2].color = 0x2a; e[2].digits = 3;
    e[3].dst = buf + 0x4ae5; e[3].val = 0x20; e[3].color = 0x1f; e[3].digits = 3;
    /* 8 direct stat numbers (white when their boost flag is clear): */
    e[4].dst = buf + 0x29dd; e[4].val = 0x0a; e[4].color = 0x2a; e[4].digits = 2;
    e[5].dst = buf + 0x379d; e[5].val = 0x05; e[5].color = 0x2a; e[5].digits = 2;
    e[6].dst = buf + 0x455d; e[6].val = 0x07; e[6].color = 0x2a; e[6].digits = 2;
    e[7].dst = buf + 0x545d; e[7].val = 0x11; e[7].color = 0x2a; e[7].digits = 3;
    e[8].dst = buf + 0x635d; e[8].val = 0x22; e[8].color = 0x2a; e[8].digits = 3;
    e[9].dst = buf + 0x4535; e[9].val = 0x55; e[9].color = 0x2a; e[9].digits = 3;
    e[10].dst = buf + 0x5435; e[10].val = 0x33; e[10].color = 0x2a; e[10].digits = 3;
    e[11].dst = buf + 0x6335; e[11].val = 0x44; e[11].color = 0x2a; e[11].digits = 3;
}

/* walk the rle digit log asserting the `n` expected numbers in order; each
 * normal number consumes `digits` glyphs. void (ASSERT_EQ early-returns); the
 * total glyph count is verified separately via g_rle_blit_calls. */
static void panel_assert_numbers(const dec_exp *e, int n)
{
    int cur = 0;
    int k;

    for (k = 0; k < n; k++) {
        dec_assert_number(cur, e[k].dst, e[k].val, e[k].color, e[k].digits);
        cur += (int)e[k].digits;
    }
}

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
    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    g_dec_sheet = data_fd2_ui_anim_sprite_sheet_ptr;  /* set by bar_setup_sheet */
    g_rle_blit_calls = 0;       /* rle digit-log cursor */
    g_rle_blit_log_on = 1;
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

    /* The real fd2_render_number_red_when_full forwards into the real decimal
     * renderer, so the 4 HP/MP cur/max numbers' glyph runs lead the rle log,
     * followed by the 8 direct stat numbers (33 glyphs total). Each renders the
     * value at its surface offset with the "full" color: cur==max -> red 0x1F,
     * else white 0x2A. The two "max" variants pass current==max so they are red.
     * dec_exp[0..3]: HP cur(white), HP max(red), MP cur(white), MP max(red). */
    {
        dec_exp e[12];
        panel_fill_expected(e, buf);
        panel_assert_numbers(e, 12);
        ASSERT_EQ((long)g_rle_blit_calls, 33);   /* 9x3 + 3x2 digit glyphs */
    }
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

    /* The 4 red-when-full numbers ([0..3]) precede the 8 direct stat numbers
     * ([4..11]). With every boost flag clear, each direct stat renders white
     * (0x2A): level/MV/mag-res are 2-digit, AP/DP/DX-base/DX-cur/Evade 3-digit.
     * panel_fill_expected encodes the exact value + dst + digits + white color
     * for all 12, and panel_assert_numbers verifies each glyph run in order. */
    {
        dec_exp e[12];
        panel_fill_expected(e, buf);
        panel_assert_numbers(e, 12);
        ASSERT_EQ((long)g_rle_blit_calls, 33);   /* 9x3 + 3x2 digit glyphs */
    }
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

    /* AP (idx 7) and DP (idx 8) flip to red 0x77; DX-base (9), DX-cur (10) and
     * Evade (11) stay white because their flag is clear. The boosted color is
     * the sprite base of those numbers' digit glyphs. */
    {
        dec_exp e[12];
        panel_fill_expected(e, buf);
        e[7].color = 0x77;   /* AP red */
        e[8].color = 0x77;   /* DP red */
        panel_assert_numbers(e, 12);
        ASSERT_EQ((long)g_rle_blit_calls, 33);   /* 9x3 + 3x2 digit glyphs */
    }
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

    /* one flag (status_flags_block[3]) reddens BOTH DX-current (idx 10) and
     * Evade (idx 11) while AP (7) and DP (8) stay white, locking the shared
     * ESI color. The red base shows in those two numbers' digit glyphs. */
    {
        dec_exp e[12];
        panel_fill_expected(e, buf);
        e[10].color = 0x77;  /* DX cur red */
        e[11].color = 0x77;  /* Evade red (shared flag) */
        panel_assert_numbers(e, 12);
        ASSERT_EQ((long)g_rle_blit_calls, 33);   /* 9x3 + 3x2 digit glyphs */
    }
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

    /* HP-current (idx 0) and AP (idx 7) are read MOVSX: hp_current 0x8001 and
     * ap 0x8002 arrive as negative 0xFFFF80xx, which the renderer clamps to 0,
     * so each draws its 3-digit value as "000". Had the read been zero-extended
     * (0x8001 = 32769 > 999) those 3-digit slots would instead emit a single
     * "MAX" glyph and the glyph total would drop below 33, so the all-zero runs
     * + 33-glyph total prove the value was sign-extended negative. HP-current
     * stays white (cur 0xFFFF8001 != max 0x64). */
    {
        dec_exp e[12];
        panel_fill_expected(e, buf);
        e[0].val = 0;   /* 0xFFFF8001 clamped -> "000", white */
        e[7].val = 0;   /* 0xFFFF8002 clamped -> "000" */
        panel_assert_numbers(e, 12);
        ASSERT_EQ((long)g_rle_blit_calls, 33);   /* 9x3 + 3x2 digit glyphs */
    }
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
 *   - fd2_render_decimal_number_to_buffer (REAL) -> digit glyphs via the
 *     fd2_blit_indexed_sprite_at_xy -> fd2_rle_blit_sprite pipeline, logged in
 *     g_rle_blit_log_*: the numeric value (as "%0.<digits>d" glyphs) for
 *     valued items, asserted with dec_assert_number against the value dst.
 *   - fd2_blit_indexed_sprite_at_xy (REAL) -> fd2_rle_blit_sprite spy
 *     (g_rle_blit_last_sprite / g_rle_blit_log_*): the placeholder dot 0x29 for
 *     unrecognized items. resolved sprite = sheet + table[0x29] = sheet + 0x29.
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
 * clear the item table, install fake sheet + immediate-END text, arm logs.
 * The digit glyphs of valued items go through the real decimal renderer ->
 * fd2_rle_blit_sprite spy (g_rle_blit_log_*), as does the placeholder dot
 * 0x29; the two are told apart by sprite index. */
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
    g_dec_sheet = sheet;        /* fake sheet for dec_assert_number */
    g_rle_blit_calls = 0;       /* rle log cursor: digit glyphs + placeholder */
    g_rle_blit_log_on = 1;
    return sheet;
}

/* all 8 slots empty (flag bit7 set) -> nothing is drawn at all. */
static void test_inv_all_empty_draws_nothing(void)
{
    uint32 buf = 0x100000;

    inv_setup();

    fd2_render_inventory_item_grid(0, -1, buf);

    ASSERT_EQ((long)g_blitraw_count, 0);
    ASSERT_EQ((long)g_rle_blit_calls, 0);   /* no digit glyphs, no placeholder */
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

    /* the number: value 0x123 (291), 3-digit white, in-range -> "291" glyphs at
     * the value dst (3 rle blits, no 0x29 placeholder on the weapon path). */
    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, buf + col_x + 0x5d + (row_y + 0x6b) * 0x140,
                      0x0123, 0x2a, 3);
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
    /* value = item->dp 0x0044 (68), 3-digit white -> "068" glyphs at the value
     * dst (cell 0 -> col_x 0x2A, row_y 0). */
    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, buf + 0x2a + 0x5d + 0x6b * 0x140, 0x0044, 0x2a, 3);
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
    /* value = *(int16*)(item+0xE) = 0x0032 (50), 3-digit white -> "050" */
    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, buf + 0x2a + 0x5d + 0x6b * 0x140, 0x0032, 0x2a, 3);
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
    /* value = *(int16*)(item+0xE) = 0x0014 (20), 3-digit white -> "020" at the
     * recomputed value dst. */
    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, buf + col_x + 0x5d + (row_y + 0x6b) * 0x140,
                      0x0014, 0x2a, 3);
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
    /* exactly one rle blit: the placeholder dot 0x29 (no decimal digits, since
     * an unrecognized item draws no value number). resolved = sheet+table[0x29]. */
    ASSERT_EQ((long)g_rle_blit_calls, 1);
    ASSERT_EQ((long)(g_rle_blit_last_sprite - sheet), 0x29);
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

/* value sign-extension: the value is read with MOVSX (signed 16-bit). A weapon
 * ap of 0x8001 arrives at the decimal renderer as 0xFFFF8001 (negative), which
 * the renderer's signed (int32)<0 guard clamps to 0 -> the 3-digit value draws
 * "000". Had the read been zero-extended (0x8001 = 32769 > 999) the 3-digit
 * path would instead emit the single overflow "MAX" glyph, so the all-zero
 * digit run proves the value was sign-extended negative. */
static void test_inv_value_sign_extension(void)
{
    uint32 buf = 0x2c0000;

    inv_setup();
    data_fd2_battle_item_effect_table[5].type = 0x01;
    data_fd2_battle_item_effect_table[5].ap   = 0x8001;
    inv_set_slot(0, 0x00, 5);

    fd2_render_inventory_item_grid(0, -1, buf);

    /* 3 digit glyphs "000" at the weapon value dst (cell 0); not a MAX glyph */
    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, buf + 0x2a + 0x5d + 0x6b * 0x140, 0, 0x2a, 3);
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
     * blits. */
    ASSERT_EQ((long)g_blitraw_count, 3);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x3D);   /* slot0 ph bg */
    ASSERT_EQ((long)(g_blitraw_log_sprite[1] - sheet), 0x3B);   /* slot1 wp bg */
    /* slot1 weapon bg landed in cell 1 (col 0 row 1), proving the placeholder
     * advanced active_slot_count from 0 to 1. */
    ASSERT_EQ((long)g_blitraw_log_dst[1],
              (long)(buf + 0x2a - 0x1d + (1 * 0x16 + 0x65) * 0x140));
    /* rle log: slot0 placeholder dot 0x29 (index 0) then slot1 weapon value
     * ap 9 -> "009" 3 digit glyphs (indices 1..3) at the cell-1 value dst. */
    ASSERT_EQ((long)g_rle_blit_calls, 4);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - sheet), 0x29);  /* placeholder */
    dec_assert_number(1, buf + 0x2a + 0x5d + (1 * 0x16 + 0x6b) * 0x140,
                      9, 0x2a, 3);
}

/* ----------------------------------------------------------------
 * fd2_render_number_red_when_full @ 0x1875d
 *
 * Thin wrapper: color = (current == max) ? 0x1F : 0x2A, then forward
 * (dst, pitch, current, color, digits) to the real
 * fd2_render_decimal_number_to_buffer. Driven directly here and observed
 * through the real digit pipeline (g_rle_blit_log_*, fake sheet table[i]=i):
 * the chosen color is the sprite base of every rendered glyph, and the
 * rendered value is `current` (never `max`). Risk-based coverage: both color
 * branches, value = current, and the full-width 32-bit equality compare.
 * Values are kept in-range (3-digit < 1000, 2-digit < 100) so the color shows
 * in the digit glyphs rather than a color-agnostic overflow placeholder.
 * ---------------------------------------------------------------- */

/* current == max -> red glow 0x1F; current is the value drawn at dst with the
 * red base. value 0x64 (100), 3 digits -> "100" glyphs based at 0x1F. */
static void test_redfull_equal_is_red(void)
{
    dec_setup();
    fd2_render_number_red_when_full(0x1234, 0x140, 0x64, 0x64, 3);

    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, 0x1234, 0x64, 0x1f, 3);   /* value=current, red base */
}

/* current < max -> white 0x2A, and value is current (not max). value 0x50
 * (80) -> "080" glyphs based at 0x2A; had it forwarded max (0x64) the glyphs
 * would be "100" instead. */
static void test_redfull_below_is_white(void)
{
    dec_setup();
    fd2_render_number_red_when_full(0x5678, 0x140, 0x50, 0x64, 3);

    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, 0x5678, 0x50, 0x2a, 3);   /* current 0x50, white base */
}

/* current > max -> still not equal -> white. current 0x40 (64), max 0x32 (50),
 * 2 digits, 64 < 100 so no overflow -> "64" glyphs based at 0x2A. */
static void test_redfull_above_is_white(void)
{
    dec_setup();
    fd2_render_number_red_when_full(0x9abc, 0x140, 0x40, 0x32, 2);

    ASSERT_EQ((long)g_rle_blit_calls, 2);
    dec_assert_number(0, 0x9abc, 0x40, 0x2a, 2);   /* white, value=current */
}

/* equality is a full 32-bit compare (CMP of two dwords): two values matching
 * only in their low 16 bits must NOT be treated as equal. The forwarded value
 * is large and >999 at 3 digits, so the white/red base is read off the single
 * overflow "MAX" glyph (sprite_base + 10): white -> 0x2A+10, red -> 0x1F+10. */
static void test_redfull_full_width_compare(void)
{
    /* low 16 bits both 0x0000 but high halves differ -> not equal -> white */
    dec_setup();
    fd2_render_number_red_when_full(0x10, 0x140, 0x00010000u, 0x00020000u, 3);
    ASSERT_EQ((long)g_rle_blit_calls, 1);
    dec_assert_overflow(0, 0x10, 0x2a + 10);        /* white base -> MAX glyph */

    /* exact 32-bit match -> red */
    dec_setup();
    fd2_render_number_red_when_full(0x10, 0x140, 0x00020000u, 0x00020000u, 3);
    ASSERT_EQ((long)g_rle_blit_calls, 1);
    dec_assert_overflow(0, 0x10, 0x1f + 10);        /* red base -> MAX glyph */
}

/* zero == zero counts as "full" (red) — boundary where both are 0. value 0,
 * 3 digits -> "000" glyphs based at the red 0x1F. */
static void test_redfull_zero_equal_is_red(void)
{
    dec_setup();
    fd2_render_number_red_when_full(0x20, 0x140, 0, 0, 3);

    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, 0x20, 0, 0x1f, 3);   /* "000", red base */
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

/* ----------------------------------------------------------------
 * fd2_render_decimal_number_to_buffer @ 0x187d6
 *
 * The REAL digit renderer, driven directly here (the dec_setup / dec_assert_*
 * shared helpers defined near the top of this file replay the binary's own
 * "%0.Nd" sprintf and assert the resulting glyph run through the g_rle_blit_log_*
 * pipeline). The panel / inventory / redfull caller tests reuse the same
 * helpers to observe the numbers their callers forward here.
 * ---------------------------------------------------------------- */
/* value 0x50 (80), 3 digits, white 0x2A -> "080": glyphs '0','8','0' map to
 * sprites 0x2A,0x32,0x2A at dst, dst+6, dst+12. */
static void test_dec_three_digit_basic(void)
{
    uint32 dst = 0x100000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 0x50, 0x2a, 3);

    ASSERT_EQ((long)g_rle_blit_calls, 3);   /* "080" -> 3 glyphs */
    dec_assert_number(0, dst, 0x50, 0x2a, 3);
}

/* a 2-digit white number uses the same zero-padded path: value 5 -> "05". */
static void test_dec_two_digit_basic(void)
{
    uint32 dst = 0x120000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 5, 0x2a, 2);

    ASSERT_EQ((long)g_rle_blit_calls, 2);
    dec_assert_number(0, dst, 5, 0x2a, 2);
}

/* negative value clamps to 0 before rendering: (int32)0xFFFF8001 < 0 -> 0,
 * so a 3-digit render produces "000", NOT an overflow placeholder. This is
 * the signed TEST EAX,EAX / JGE branch. */
static void test_dec_negative_clamps_to_zero(void)
{
    uint32 dst = 0x140000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 0xFFFF8001u, 0x2a, 3);

    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, dst, 0, 0x2a, 3);   /* "000" */
}

/* digit_count == 3 AND value > 999: a single overflow "MAX" glyph at
 * sprite_base + 10, drawn at dst; no digit glyphs. */
static void test_dec_three_digit_overflow_max(void)
{
    uint32 dst = 0x160000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 1000, 0x2a, 3);

    ASSERT_EQ((long)g_rle_blit_calls, 1);
    dec_assert_overflow(0, dst, 0x2a + 10);
}

/* boundary: value == 999 with 3 digits is NOT overflow (strict >999) -> "999"
 * digit glyphs; value 1000 (above) is overflow (covered above). */
static void test_dec_three_digit_overflow_boundary(void)
{
    uint32 dst = 0x180000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 999, 0x2a, 3);

    ASSERT_EQ((long)g_rle_blit_calls, 3);
    dec_assert_number(0, dst, 999, 0x2a, 3);   /* "999", no overflow */
}

/* digit_count == 2 AND value >= 100: single fixed "99+" glyph 0x5D
 * (color-agnostic), drawn at dst; no digit glyphs. The 0x5D sprite is NOT
 * offset by sprite_base. */
static void test_dec_two_digit_overflow_99plus(void)
{
    uint32 dst = 0x1a0000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 100, 0x77, 2);

    ASSERT_EQ((long)g_rle_blit_calls, 1);
    dec_assert_overflow(0, dst, 0x5d);   /* fixed, ignores sprite_base 0x77 */
}

/* boundary: value == 99 with 2 digits is NOT overflow (>=100) -> "99"; the
 * 2-digit overflow is value >= 100 (covered above). */
static void test_dec_two_digit_overflow_boundary(void)
{
    uint32 dst = 0x1c0000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 99, 0x2a, 2);

    ASSERT_EQ((long)g_rle_blit_calls, 2);
    dec_assert_number(0, dst, 99, 0x2a, 2);   /* "99" */
}

/* a 3-digit value in [100..999] with no overflow still renders all 3 digits;
 * confirms the digit glyphs index off the supplied sprite_base (red 0x77). */
static void test_dec_color_base_applied(void)
{
    uint32 dst = 0x1e0000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 0x123, 0x77, 3);

    ASSERT_EQ((long)g_rle_blit_calls, 3);   /* "291" */
    dec_assert_number(0, dst, 0x123, 0x77, 3);
}

/* a large value with a high digit_count (4) does NOT hit either overflow
 * guard (those are digit_count-specific to 3 and 2): value 0x1000 (4096) with
 * 4 digits renders "4096" via the zero-padded path -> 4 digit glyphs. This
 * pins that the overflow branches are gated on digit_count exactly. */
static void test_dec_four_digits_no_overflow_guard(void)
{
    uint32 dst = 0x220000;

    dec_setup();
    fd2_render_decimal_number_to_buffer(dst, 0x140, 0x1000, 0x2a, 4);

    ASSERT_EQ((long)g_rle_blit_calls, 4);
    dec_assert_number(0, dst, 0x1000, 0x2a, 4);
}

/* ----------------------------------------------------------------
 * fd2_render_mini_char_status_panel @ 0x18c6d
 *
 * Drive the REAL mini-panel painter and capture its dispatch to the same
 * five REAL render primitives the full panel uses:
 *   - fd2_dialog_sprite_blit_normal (spy) -> g_dlg_blit_* : background
 *     sprite at dst=buf, sprite = sheet + *(int*)(sheet+0x5E), stride.
 *     With bar_setup_sheet's table[i]=i, *(int*)(sheet+0x5E) = table[22]
 *     = 0x16, so the resolved bg sprite is sheet + 0x16.
 *   - fd2_render_hp_or_mp_bar_proportional (REAL) -> REAL segment painter
 *     -> g_blitraw caps; the right-cap offset = origin + segment_count
 *     encodes the exact (cur,max) forwarded.
 *   - fd2_render_decimal_number_to_buffer (REAL, sleep indicator) and
 *     fd2_render_number_red_when_full (REAL -> REAL decimal) -> glyph runs
 *     in the g_rle_blit_log_* digit log (call order: sleep[2] then HP[3]
 *     then MP[3]).
 *   - fd2_display_dialog_scene (REAL) for the name label against an
 *     immediate-END text program (returns at once, no glyph blits).
 *
 * The panel indexes data_fd2_battle_runtime_char_array_ptr (= g_test_rc_array,
 * 8 slots). Decimal numbers do NOT touch g_blitraw; bar caps do NOT touch the
 * rle log; the background blit is the only fd2_dialog_sprite_blit_normal call,
 * so the three logs stay cleanly separated.
 *
 * Stride 0x1C8 (456) is the production value the in-battle caller passes; a
 * couple of cases use 0x140 to vary it. The render-position offsets are
 * stride*K + buf + off, so the bar / number / name destinations move with
 * the chosen stride.
 * ---------------------------------------------------------------- */

/* dialog-VM glyph recorder (testglob.c), used to prove the name-label page +
 * render position. */
extern int    g_dlg_glyph_calls;
extern uint32 g_dlg_glyph_last_pos;

/* zero rc slot 0 and load a known HP/MP/sleep/char_id profile for the mini
 * panel. Values chosen so HP cur < max (white HP number) and the bar segment
 * counts are exact: HP (0x50,0x64) -> 81, MP (0x10,0x20) -> 51. */
static runtime_char *mini_setup_char(void)
{
    runtime_char *rc = &g_test_rc_array[0];
    memset(rc, 0, sizeof(*rc));

    rc->hp_current = 0x0050;
    rc->hp_max     = 0x0064;
    rc->mp_current = 0x0010;
    rc->mp_max     = 0x0020;
    rc->status_flags_block[0] = 0x07;   /* sleep/status indicator */
    rc->char_id    = 0x03;
    return rc;
}

/* background sprite blit: dst = buf, sprite = sheet + *(int*)(sheet+0x5E)
 * (= sheet + 0x16 under bar_setup_sheet's table), stride passed through. It is
 * the sole fd2_dialog_sprite_blit_normal call. */
static void test_mini_background_blit(void)
{
    uint32 sheet;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;

    sheet = bar_setup_sheet();
    panel_setup_text();
    mini_setup_char();
    panel_reset_logs();
    g_dlg_blit_normal_calls = 0;
    g_dlg_blit_mirrored_calls = 0;

    fd2_render_mini_char_status_panel(buf, stride, 0);

    ASSERT_EQ((long)g_dlg_blit_normal_calls, 1);
    ASSERT_EQ((long)g_dlg_blit_mirrored_calls, 0);
    ASSERT_EQ((long)g_dlg_blit_last_dst, (long)buf);
    ASSERT_EQ((long)g_dlg_blit_last_sprite, (long)(sheet + 0x16u));
    ASSERT_EQ((long)g_dlg_blit_last_stride, (long)stride);
}

/* HP/MP bars: origin at stride*0x16+buf+0x15 / stride*0x1F+buf+0x15, base
 * sprite 0x17 / 0x1A, and the right cap at origin + segment_count proves the
 * (cur,max) forwarded through the proportional formula:
 *   HP segments = (0x50*0x65)/0x64 + 1 = 81 -> right cap 0x19
 *   MP segments = (0x10*0x65)/0x20 + 1 = 51 -> right cap 0x1C */
static void test_mini_bars_origins_and_segments(void)
{
    uint32 sheet;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;
    uint32 hp_org = stride * 0x16 + buf + 0x15;
    uint32 mp_org = stride * 0x1f + buf + 0x15;

    sheet = bar_setup_sheet();
    panel_setup_text();
    mini_setup_char();
    panel_reset_logs();

    fd2_render_mini_char_status_panel(buf, stride, 0);

    ASSERT_EQ((long)(panel_find_blit_sprite(hp_org) - sheet), 0x17);
    ASSERT_EQ((long)(panel_find_blit_sprite(hp_org + 81) - sheet), 0x19);
    ASSERT_EQ((long)(panel_find_blit_sprite(mp_org) - sheet), 0x1a);
    ASSERT_EQ((long)(panel_find_blit_sprite(mp_org + 51) - sheet), 0x1c);
}

/* the three decimal numbers, in call order, with the right dst / value /
 * color base / digit width:
 *   [0] sleep indicator  buf+0x84+stride*4   value 0x07  color 0x1F  2 digits
 *   [1] HP current       stride*0x15+buf+0x7E value 0x50 white 0x2A  3 digits
 *       (cur 0x50 != max 0x64 -> white)
 *   [2] MP current       stride*0x1E+buf+0x7E value 0x10 white 0x2A  3 digits
 *       (cur 0x10 != max 0x20 -> white)
 * 2 + 3 + 3 = 8 digit glyphs total. */
static void test_mini_numbers_order_and_colors(void)
{
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;

    bar_setup_sheet();
    panel_setup_text();
    mini_setup_char();
    panel_reset_logs();

    fd2_render_mini_char_status_panel(buf, stride, 0);

    ASSERT_EQ((long)g_rle_blit_calls, 8);
    dec_assert_number(0, buf + 0x84 + stride * 4, 0x07, 0x1f, 2);
    dec_assert_number(2, stride * 0x15 + buf + 0x7e, 0x50, 0x2a, 3);
    dec_assert_number(5, stride * 0x1e + buf + 0x7e, 0x10, 0x2a, 3);
}

/* when current == max the HP/MP numbers switch to the red "full" color 0x1F
 * (fd2_render_number_red_when_full picks 0x1F on equality, else 0x2A). Sleep
 * indicator stays 0x1F regardless. Profile: HP and MP both at full. */
static void test_mini_numbers_red_when_full(void)
{
    runtime_char *rc;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;

    bar_setup_sheet();
    panel_setup_text();
    rc = mini_setup_char();
    rc->hp_current = 0x0064;   /* == hp_max */
    rc->mp_current = 0x0020;   /* == mp_max */
    panel_reset_logs();

    fd2_render_mini_char_status_panel(buf, stride, 0);

    ASSERT_EQ((long)g_rle_blit_calls, 8);
    dec_assert_number(0, buf + 0x84 + stride * 4, 0x07, 0x1f, 2);
    dec_assert_number(2, stride * 0x15 + buf + 0x7e, 0x64, 0x1f, 3);
    dec_assert_number(5, stride * 0x1e + buf + 0x7e, 0x20, 0x1f, 3);
}

/* negative HP/MP words (int16 0xFFFF = -1) are sign-extended to int32 -1: the
 * decimal renderer clamps value<0 to 0 so the HP/MP numbers render "000", and
 * the bar's max==0 guard (mp_max=0) draws no MP bar. Here hp_max stays positive
 * so the HP bar still draws (cur clamped negative -> current==0 path? no: the
 * proportional fn tests current==0 by equality, -1 != 0, so it runs the signed
 * (-1*0x65)/0x64 + 1 = 0 + 1 = 1 segment path). We pin the clamp via the "000"
 * HP number and the absent MP bar via max==0. */
static void test_mini_sign_extension_and_guards(void)
{
    runtime_char *rc;
    uint32 buf    = 0x100000;
    uint32 stride = 0x140;
    uint32 mp_org = stride * 0x1f + buf + 0x15;

    bar_setup_sheet();
    panel_setup_text();
    rc = mini_setup_char();
    rc->hp_current = 0xFFFF;   /* int16 -1 -> int32 -1 */
    rc->hp_max     = 0x0064;
    rc->mp_current = 0x0000;
    rc->mp_max     = 0x0000;   /* div-by-zero guard: no MP bar */
    panel_reset_logs();

    fd2_render_mini_char_status_panel(buf, stride, 0);

    /* HP number: cur is the sign-extended -1 (!= max 0x64) -> white 0x2A, and
     * the decimal renderer clamps the negative value to 0 -> "000". */
    dec_assert_number(2, stride * 0x15 + buf + 0x7e, 0, 0x2a, 3);
    /* MP number: cur == max (both 0) -> red-when-full picks red 0x1F; value 0
     * -> "000". */
    dec_assert_number(5, stride * 0x1e + buf + 0x7e, 0, 0x1f, 3);
    /* mp_max == 0 -> proportional bar returns before any blit: no MP cap */
    ASSERT_EQ((long)panel_count_blit_at(mp_org), 0);
}

/* the name label calls fd2_display_dialog_scene with page = char_id + 1 at
 * render_pos buf + 5 + stride*4. We build a text table where ONLY the entry at
 * page (char_id+1) redirects to a single-glyph body and every other entry is an
 * immediate END; observing exactly one glyph at buf+5+stride*4 proves both the
 * page-index arithmetic (char_id+1 selected) and the name-label dst. char_id=3
 * -> page 4. */
static uint16 g_mini_name_text[0x200];

static void test_mini_name_label_page_and_pos(void)
{
    runtime_char *rc;
    uint32 buf      = 0x100000;
    uint32 stride   = 0x1c8;
    uint32 name_pos = buf + 5 + stride * 4;
    int    page;
    int    i;
    int    body_word;

    bar_setup_sheet();
    rc = mini_setup_char();        /* char_id = 3 -> page 4 */
    page = (int)rc->char_id + 1;

    /* immediate-END marker high in the buffer; every page entry points there */
    for (i = 0; i < 0x200; i++) {
        g_mini_name_text[i] = 0;
    }
    g_mini_name_text[0x180] = (uint16)-1;          /* END at word 0x180 */
    for (i = 0; i < 0x180; i++) {
        g_mini_name_text[i] = (uint16)(0x180 * 2); /* byte offset of END */
    }
    /* page (char_id+1) instead redirects to a 1-glyph body + END */
    body_word = 0x100;                              /* a free region */
    g_mini_name_text[page] = (uint16)(body_word * 2);
    g_mini_name_text[body_word]     = 0x41;         /* one glyph */
    g_mini_name_text[body_word + 1] = (uint16)-1;   /* END */
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)g_mini_name_text;

    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_pos = 0;
    panel_reset_logs();

    fd2_render_mini_char_status_panel(buf, stride, 0);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)name_pos);
}

/* char_idx selects the runtime_char slot: driving slot 2 with a distinct HP
 * profile renders that slot's values. Confirms the rc = base[char_idx] index
 * (stride 0x50 per entry) by reading slot 2's HP cur into the HP number. */
static void test_mini_char_idx_selects_slot(void)
{
    runtime_char *rc2;
    uint32 buf    = 0x100000;
    uint32 stride = 0x140;

    bar_setup_sheet();
    panel_setup_text();
    /* slot 0 left as some other profile; slot 2 is the one we render */
    memset(&g_test_rc_array[0], 0, sizeof(g_test_rc_array[0]));
    rc2 = &g_test_rc_array[2];
    memset(rc2, 0, sizeof(*rc2));
    rc2->hp_current = 0x0021;
    rc2->hp_max     = 0x0099;
    rc2->mp_current = 0x0000;
    rc2->mp_max     = 0x0000;
    rc2->status_flags_block[0] = 0x09;
    rc2->char_id    = 0x01;
    panel_reset_logs();

    fd2_render_mini_char_status_panel(buf, stride, 2);

    /* HP number renders slot 2's hp_current 0x21 (white, < max) */
    dec_assert_number(2, stride * 0x15 + buf + 0x7e, 0x21, 0x2a, 3);
    /* sleep indicator renders slot 2's status_flags_block[0] 0x09 */
    dec_assert_number(0, buf + 0x84 + stride * 4, 0x09, 0x1f, 2);
}

/* ================================================================
 * fd2_render_terrain_info_hud_panel @ 0x1ACF3
 *
 * Corner terrain-info HUD panel. These tests pin the HUD-enable gate, the
 * panel_offset auto-positioning branches, the panel_base address arithmetic,
 * the per-tile MV/DEF modifier-table lookup + destination offsets, and the
 * char-present portrait/HP sub-path with its exclusion conditions. Blits are
 * observed through the recording spies: g_rle_blit_* for the backdrop, the HP
 * digits, and the real fd2_render_signed_modifier_with_icon sign-icon + 2-digit
 * MV/DEF modifier glyphs (g_rle_blit_log_* per-call log); g_blitpass_* for the
 * 24x24 terrain icon / portrait passthrough.
 * ================================================================ */
extern int    g_blitpass_calls;
extern uint32 g_blitpass_src[64];
extern uint32 g_blitpass_dst[64];
extern uint32 g_blitpass_stride[64];
extern uint32 g_rle_blit_last_sprite;
extern uint32 g_rle_blit_last_buf;
extern int32  g_rle_blit_last_stride;
extern uint32 g_rle_blit_last_palette;

/* tile-map / attr buffers backing fd2_read_tile_attribute_at_pos: the cursor
 * tile resolves to sprite word HUD_TILE_WORD and attr byte[1] HUD_TILE_ATTR2. */
#define HUD_TILE_WORD   0x0002u
#define HUD_TILE_ATTR2  0x05u
/* g_hud_map must hold cell (cy*width+cx)*4 + 8; cursor (4,4) width 8 -> 152. */
static uint8  g_hud_map[256];
static uint8  g_hud_attr[64];
/* battle_scene_snapshot: the terrain icon source = snapshot + *(snapshot +
 * HUD_TILE_WORD*4 + 6). Park a known offset there so the icon src is derivable. */
#define HUD_ICON_TABLE_OFF  (HUD_TILE_WORD * 4u + 6u)   /* = 14 */
#define HUD_ICON_PAYLOAD    0x40u
static uint8  g_hud_snapshot[256];
/* portrait cache: portrait src = cache + *(cache + (frame_mod + cache_idx*0xC)*4). */
static uint8  g_hud_portrait_cache[512];

/* common setup: gate ON, sprite sheet + tile/attr/snapshot fixtures, cursor at
 * (cx,cy). Leaves the runtime-char array empty (no unit under cursor) unless a
 * test installs one. Returns the panel sprite sheet base. */
static uint32 hud_setup(uint32 cx, uint32 cy)
{
    uint32 sheet;
    uint32 cell;

    sheet = bar_setup_sheet();          /* sets data_fd2_ui_anim_sprite_sheet_ptr */
    /* backdrop sprite chunk pointer at sheet+0x20E */
    *(int32 *)((uint8 *)data_fd2_ui_anim_sprite_sheet_ptr + 0x20e) = 0x123;

    data_fd2_ui_terrain_hud_user_enabled = 1;
    data_fd2_ui_play_active_flag = 1;
    data_fd2_ui_terrain_hud_panel_offset_51a0c = 0;   /* known latch start */

    data_fd2_battle_cursor_world_x = cx;
    data_fd2_battle_cursor_world_y = cy;
    data_fd2_battle_map_width_tiles = 8;

    memset(g_hud_map, 0, sizeof(g_hud_map));
    memset(g_hud_attr, 0, sizeof(g_hud_attr));
    cell = (cy * 8u + cx) * 4u;
    *(uint16 *)(g_hud_map + cell + 4) = (uint16)HUD_TILE_WORD;
    g_hud_map[cell + 6] = 0x00;
    g_hud_attr[HUD_TILE_WORD * 4u + 1u] = HUD_TILE_ATTR2;
    data_fd2_battle_tile_map_ptr = (uint32)g_hud_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_hud_attr;

    memset(g_hud_snapshot, 0, sizeof(g_hud_snapshot));
    *(int32 *)(g_hud_snapshot + HUD_ICON_TABLE_OFF) = (int32)HUD_ICON_PAYLOAD;
    battle_scene_snapshot = (uint32)g_hud_snapshot;

    /* MV/DEF modifier tables keyed by tile_attr2 */
    data_fd2_battle_tile_attr_mv_modifier_table[HUD_TILE_ATTR2] = 0xFFFFFFFFu; /* -1 */
    data_fd2_battle_tile_attr_def_modifier_table[HUD_TILE_ATTR2] = 0x00000007u; /* +7 */

    /* empty roster: no unit at the cursor */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 4;
    /* park all units off the cursor cell so find returns -1 by default */
    {
        int i;
        for (i = 0; i < 8; i++) {
            g_test_rc_array[i].pos_x = (uint8)(cx + 20 + i);
            g_test_rc_array[i].pos_y = (uint8)(cy + 20 + i);
        }
    }

    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;

    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;   /* log sign-icon + digit glyphs of the real
                              * fd2_render_signed_modifier_with_icon */
    g_blitpass_calls = 0;
    return sheet;
}

/* Gate: terrain-HUD user-disable flag clears -> nothing renders at all. */
static void test_hud_gate_user_disabled(void)
{
    hud_setup(4, 4);
    data_fd2_ui_terrain_hud_user_enabled = 0;

    fd2_render_terrain_info_hud_panel(0x100000, 0x1c8);

    ASSERT_EQ((long)g_rle_blit_calls, 0);
    ASSERT_EQ((long)g_blitpass_calls, 0);
}

/* Gate: play-active flag clears -> nothing renders. */
static void test_hud_gate_play_inactive(void)
{
    hud_setup(4, 4);
    data_fd2_ui_play_active_flag = 0;

    fd2_render_terrain_info_hud_panel(0x100000, 0x1c8);

    ASSERT_EQ((long)g_rle_blit_calls, 0);
    ASSERT_EQ((long)g_blitpass_calls, 0);
}

/* Auto-position RIGHT column: cursor_screen_y > 5 && cursor_screen_x < 3 latches
 * panel_offset = 0xF2. panel_base = buf + stride*0x9D + 0xF2. Verifies the
 * latch, the backdrop blit (src = sheet + *(sheet+0x20E), dst = panel_base,
 * stride, palette 0xFFFFFFFF) and the terrain-icon passthrough at +stride*5+6
 * (src = snapshot + payload). No unit under cursor: exactly one passthrough. */
static void test_hud_position_right_and_backdrop(void)
{
    uint32 sheet;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;
    uint32 panel_base;

    sheet = hud_setup(4, 4);
    data_fd2_battle_cursor_screen_x = 2;   /* < 3 */
    data_fd2_battle_cursor_screen_y = 7;   /* > 5 */

    fd2_render_terrain_info_hud_panel(buf, stride);

    ASSERT_EQ((long)data_fd2_ui_terrain_hud_panel_offset_51a0c, 0xf2);
    panel_base = buf + stride * 0x9d + 0xf2;

    /* backdrop is the FIRST rle blit (log[0]); the real signed-modifier renderer
     * appends sign-icon + digit blits after it, so g_rle_blit_last_* no longer
     * holds the backdrop. fd2_rle_blit_sprite(sheet + *(sheet+0x20E), 0,0,
     * panel_base, stride, -1). */
    ASSERT_EQ((long)g_rle_blit_log_sprite[0], (long)(sheet + 0x123));
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)panel_base);
    ASSERT_EQ((long)g_rle_blit_log_stride[0], (long)stride);
    ASSERT_EQ((long)g_rle_blit_log_palette[0], (long)0xffffffffu);

    /* terrain icon: passthrough(snapshot + payload, panel_base+stride*5+6, stride) */
    ASSERT_EQ((long)g_blitpass_calls, 1);
    ASSERT_EQ((long)g_blitpass_src[0],
              (long)(battle_scene_snapshot + HUD_ICON_PAYLOAD));
    ASSERT_EQ((long)g_blitpass_dst[0], (long)(panel_base + stride * 5 + 6));
    ASSERT_EQ((long)g_blitpass_stride[0], (long)stride);
}

/* Auto-position LEFT column: cursor_screen_y > 5 && cursor_screen_x > 9 latches
 * panel_offset = 1. Also drives the two REAL fd2_render_signed_modifier_with_icon
 * calls end-to-end: each forwards MV/DEF_modifier_table[tile_attr2] to a dst of
 * panel_base + stride*K + 0x2B (K = 8 for MV, 0x13 for DEF). The real function
 * blits a sign icon (0x83 for >=0, 0x84 for <0) at that dst then renders the
 * abs() magnitude as a 2-digit red (0x1F) number 8 bytes to the right; all land
 * in the g_rle_blit_log_*. With no unit under the cursor the full log is:
 *   [0] backdrop, [1] MV sign, [2..3] MV "01" digits,
 *   [4] DEF sign, [5..6] DEF "07" digits  (7 rle blits total).
 * mv_table[5] = -1 (negative -> 0x84 icon, magnitude 1 -> "01");
 * def_table[5] = +7 (positive -> 0x83 icon, magnitude 7 -> "07"). */
static void test_hud_position_left_and_modifiers(void)
{
    uint32 sheet;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;
    uint32 panel_base;
    uint32 mv_dst;
    uint32 def_dst;

    sheet = hud_setup(4, 4);
    data_fd2_battle_cursor_screen_x = 10;  /* > 9 */
    data_fd2_battle_cursor_screen_y = 7;   /* > 5 */

    fd2_render_terrain_info_hud_panel(buf, stride);

    ASSERT_EQ((long)data_fd2_ui_terrain_hud_panel_offset_51a0c, 1);
    panel_base = buf + stride * 0x9d + 1;
    mv_dst  = panel_base + stride * 8 + 0x2b;
    def_dst = panel_base + stride * 0x13 + 0x2b;

    /* backdrop + 2 modifier displays (sign + 2 digits each) = 7 rle blits */
    ASSERT_EQ((long)g_rle_blit_calls, 7);

    /* [0] backdrop sprite (sheet + *(sheet+0x20E) = sheet + 0x123) @ panel_base */
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - sheet), (long)0x123);
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)panel_base);

    /* MV modifier = mv_table[5] = -1: sign icon 0x84 @ mv_dst, then "01" red */
    ASSERT_EQ((long)(g_rle_blit_log_sprite[1] - sheet), (long)0x84);
    ASSERT_EQ((long)g_rle_blit_log_dst[1], (long)mv_dst);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[2] - sheet), (long)(0x1f + 0)); /* '0' */
    ASSERT_EQ((long)g_rle_blit_log_dst[2], (long)(mv_dst + 8));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[3] - sheet), (long)(0x1f + 1)); /* '1' */
    ASSERT_EQ((long)g_rle_blit_log_dst[3], (long)(mv_dst + 8 + 6));

    /* DEF modifier = def_table[5] = +7: sign icon 0x83 @ def_dst, then "07" red */
    ASSERT_EQ((long)(g_rle_blit_log_sprite[4] - sheet), (long)0x83);
    ASSERT_EQ((long)g_rle_blit_log_dst[4], (long)def_dst);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[5] - sheet), (long)(0x1f + 0)); /* '0' */
    ASSERT_EQ((long)g_rle_blit_log_dst[5], (long)(def_dst + 8));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[6] - sheet), (long)(0x1f + 7)); /* '7' */
    ASSERT_EQ((long)g_rle_blit_log_dst[6], (long)(def_dst + 8 + 6));
}

/* Auto-position KEEP: neither branch taken (y in 6..., x mid) -> latch unchanged
 * from its previous value (pre-seed 0x55). */
static void test_hud_position_keep_previous(void)
{
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;

    hud_setup(4, 4);
    /* y >= 6 fails the first branch's y<6; x in 4..9 fails the second's x>9 */
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 8;
    data_fd2_ui_terrain_hud_panel_offset_51a0c = 0x55;

    fd2_render_terrain_info_hud_panel(buf, stride);

    ASSERT_EQ((long)data_fd2_ui_terrain_hud_panel_offset_51a0c, 0x55);
    /* panel_base used the kept offset: the backdrop (first rle blit, log[0])
     * targets panel_base = buf + stride*0x9d + kept_offset. */
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)(buf + stride * 0x9d + 0x55));
}

/* Auto-position y-axis guard: screen_x < 3 but screen_y <= 5 must NOT latch the
 * right column (the asm's first branch requires screen_y > 5, i.e. >= 6, not
 * < 6). Latch stays at its pre-seeded value. Pins against inverting the y test. */
static void test_hud_position_low_y_keeps_previous(void)
{
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;

    hud_setup(4, 4);
    data_fd2_battle_cursor_screen_x = 1;   /* < 3 */
    data_fd2_battle_cursor_screen_y = 4;   /* <= 5 -> first branch must NOT fire */
    data_fd2_ui_terrain_hud_panel_offset_51a0c = 0x33;

    fd2_render_terrain_info_hud_panel(buf, stride);

    ASSERT_EQ((long)data_fd2_ui_terrain_hud_panel_offset_51a0c, 0x33);
    /* backdrop (first rle blit, log[0]) targets panel_base = the kept offset. */
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)(buf + stride * 0x9d + 0x33));
}

/* Unit present under cursor (visible portrait, player team): the portrait
 * OVERWRITES the terrain icon at +stride*5+6 (second passthrough), and the HP /
 * HP_max render as 3 digits at +stride*0x15+9. frame_mod = palette idx (0 here),
 * portrait src = cache + *(cache + (0 + cache_idx*0xC)*4). */
static void test_hud_char_present_portrait_and_hp(void)
{
    runtime_char *rc;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;
    uint32 panel_base;
    uint32 cache_idx = 2;
    uint32 portrait_payload = 0x80;

    hud_setup(4, 4);
    data_fd2_battle_cursor_screen_x = 5;   /* keep branch */
    data_fd2_battle_cursor_screen_y = 8;
    data_fd2_ui_terrain_hud_panel_offset_51a0c = 0;

    /* portrait cache: entry (frame_mod 0 + cache_idx*0xC) */
    memset(g_hud_portrait_cache, 0, sizeof(g_hud_portrait_cache));
    *(int32 *)(g_hud_portrait_cache + (0 + cache_idx * 0xc) * 4) =
        (int32)portrait_payload;
    portrait_sprite_cache = (uint32)g_hud_portrait_cache;

    /* place a visible player unit at the cursor cell */
    rc = &g_test_rc_array[1];
    rc->pos_x = 4; rc->pos_y = 4;
    rc->portrait_id   = 0x10;          /* != 0x79 */
    rc->archetype_flag = 0x03;         /* != 10 */
    rc->team          = 2;             /* player */
    rc->sprite_state[0] = (uint8)cache_idx;
    rc->hp_current    = 123;
    rc->hp_max        = 200;

    /* rle_blit log order before the HP digits: backdrop (log[0]), then the two
     * real signed-modifier displays — MV (sign + 2 digits, log[1..3]) and DEF
     * (sign + 2 digits, log[4..6]) — so the 3 HP glyphs occupy log indices 7..9. */
    g_dec_sheet = data_fd2_ui_anim_sprite_sheet_ptr;
    g_rle_blit_log_on = 1;
    g_rle_blit_calls = 0;
    g_blitpass_calls = 0;

    fd2_render_terrain_info_hud_panel(buf, stride);

    panel_base = buf + stride * 0x9d + 0;

    /* two passthroughs: [0] terrain icon, [1] portrait overwrite (same dst) */
    ASSERT_EQ((long)g_blitpass_calls, 2);
    ASSERT_EQ((long)g_blitpass_src[1],
              (long)(portrait_sprite_cache + portrait_payload));
    ASSERT_EQ((long)g_blitpass_dst[1], (long)(panel_base + stride * 5 + 6));

    /* HP digits: fd2_render_number_red_when_full(panel_base+stride*0x15+9,
     * stride, 123, 200, 3) -> white (123 != 200), 3 glyphs "123" from the digit
     * log starting at index 7 (backdrop[0] + MV[1..3] + DEF[4..6] precede them). */
    dec_assert_number(7, panel_base + stride * 0x15 + 9, 123, 0x2a, 3);
}

/* Hidden portrait (portrait_id == 0x79) suppresses the portrait + HP sub-path:
 * the terrain icon passthrough still runs (1), but no portrait overwrite and no
 * HP digits. */
static void test_hud_char_hidden_portrait_excluded(void)
{
    runtime_char *rc;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;

    hud_setup(4, 4);
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 8;

    rc = &g_test_rc_array[1];
    rc->pos_x = 4; rc->pos_y = 4;
    rc->portrait_id   = 0x79;          /* hidden -> excluded */
    rc->team          = 2;
    rc->hp_current    = 50;
    rc->hp_max        = 99;

    fd2_render_terrain_info_hud_panel(buf, stride);

    /* only the terrain icon passthrough; portrait overwrite suppressed */
    ASSERT_EQ((long)g_blitpass_calls, 1);
}

/* Archetype-10 enemy (archetype_flag == 10 && team == 1) is also excluded
 * (boss with hidden info): terrain icon only, no portrait/HP. */
static void test_hud_char_archetype10_enemy_excluded(void)
{
    runtime_char *rc;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;

    hud_setup(4, 4);
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 8;

    rc = &g_test_rc_array[1];
    rc->pos_x = 4; rc->pos_y = 4;
    rc->portrait_id    = 0x10;         /* visible */
    rc->archetype_flag = 10;           /* archetype 10 ... */
    rc->team           = 1;            /* ... on team 1 -> excluded */
    rc->hp_current     = 50;
    rc->hp_max         = 99;

    fd2_render_terrain_info_hud_panel(buf, stride);

    ASSERT_EQ((long)g_blitpass_calls, 1);
}

/* frame_mod remap: chapter ambient palette idx == 3 collapses to 1 before the
 * portrait-cache index. With cache_idx 0, idx==3 selects cache entry (1+0)=1,
 * NOT entry 3; pin that the portrait src came from entry 1's payload. */
static void test_hud_char_palette_idx3_remaps_to_1(void)
{
    runtime_char *rc;
    uint32 buf    = 0x100000;
    uint32 stride = 0x1c8;

    hud_setup(4, 4);
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 8;
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 3;   /* -> frame_mod 1 */

    memset(g_hud_portrait_cache, 0, sizeof(g_hud_portrait_cache));
    *(int32 *)(g_hud_portrait_cache + 1 * 4) = (int32)0x90;    /* entry 1 */
    *(int32 *)(g_hud_portrait_cache + 3 * 4) = (int32)0xDEAD;  /* entry 3 (unused) */
    portrait_sprite_cache = (uint32)g_hud_portrait_cache;

    rc = &g_test_rc_array[1];
    rc->pos_x = 4; rc->pos_y = 4;
    rc->portrait_id    = 0x10;
    rc->archetype_flag = 0x03;
    rc->team           = 2;
    rc->sprite_state[0] = 0;           /* cache_idx 0 -> index = frame_mod = 1 */
    rc->hp_current     = 10;
    rc->hp_max         = 10;

    g_blitpass_calls = 0;
    fd2_render_terrain_info_hud_panel(buf, stride);

    ASSERT_EQ((long)g_blitpass_calls, 2);
    ASSERT_EQ((long)g_blitpass_src[1],
              (long)(portrait_sprite_cache + 0x90));
}

/* ================================================================
 * fd2_render_signed_modifier_with_icon @ 0x1AEB1
 *
 * Drive the REAL signed-modifier renderer directly (its only in-binary caller
 * is the HUD panel, exercised above; these pin the function in isolation).
 * It blits a sign icon then the abs() magnitude as 2 red digits:
 *   modifier >= 0 -> sign sprite 0x83, value as-is
 *   modifier <  0 -> sign sprite 0x84, value abs()'d
 * Both the sign icon (fd2_rle_blit_sprite) and the digit glyphs
 * (fd2_render_decimal_number_to_buffer -> fd2_blit_indexed_sprite_at_xy ->
 * fd2_rle_blit_sprite) land in g_rle_blit_log_* in order: [0] sign icon @ dst,
 * [1..] digits "%0.2d" of the magnitude @ dst+8 (6px apart), color 0x1F.
 * With the fake sheet (bar_setup_sheet, table[i]=i) the resolved sign stream is
 * sheet + idx and each digit glyph sheet + 0x1F + (digit-'0').
 * ================================================================ */

/* arm the fake sheet (table[i]=i) + rle per-call log for a signmod test. */
static uint32 signmod_setup(void)
{
    uint32 sheet = bar_setup_sheet();   /* table[i]=i; sets sprite-sheet ptr */
    g_dec_sheet = sheet;                /* for dec_assert_number */
    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;
    return sheet;
}

/* positive modifier: sign icon 0x83, value drawn as-is ("05"), digits at dst+8. */
static void test_signmod_positive_plus_icon(void)
{
    uint32 sheet;
    uint32 dst = 0x100000;

    sheet = signmod_setup();
    fd2_render_signed_modifier_with_icon(dst, 0x140, 5);

    /* 1 sign icon + 2 digits = 3 blits */
    ASSERT_EQ((long)g_rle_blit_calls, 3);
    /* [0] cyan "+" icon 0x83 at dst */
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - sheet), (long)0x83);
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)dst);
    /* [1..2] "05" red (0x1F) at dst+8 */
    dec_assert_number(1, dst + 8, 5, 0x1f, 2);
}

/* negative modifier: sign icon 0x84, value abs()'d so the MAGNITUDE renders.
 * -5 must produce the exact same "05" digits as +5 (proves abs ran and that the
 * abs result — not the raw negative — flows into the digit renderer; this is the
 * post-CALL EAX-result point in the binary). */
static void test_signmod_negative_minus_icon_abs(void)
{
    uint32 sheet;
    uint32 dst = 0x200000;

    sheet = signmod_setup();
    fd2_render_signed_modifier_with_icon(dst, 0x140, -5);

    ASSERT_EQ((long)g_rle_blit_calls, 3);
    /* [0] red "-" icon 0x84 at dst */
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - sheet), (long)0x84);
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)dst);
    /* [1..2] magnitude "05" (abs(-5)=5) red at dst+8 */
    dec_assert_number(1, dst + 8, 5, 0x1f, 2);
}

/* zero is NON-negative (the binary's JGE takes the >=0 path at 0): sign icon
 * 0x83, magnitude "00". Pins the comparison boundary (0 -> '+', not '-'). */
static void test_signmod_zero_is_positive(void)
{
    uint32 sheet;
    uint32 dst = 0x300000;

    sheet = signmod_setup();
    fd2_render_signed_modifier_with_icon(dst, 0x140, 0);

    ASSERT_EQ((long)g_rle_blit_calls, 3);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - sheet), (long)0x83);   /* '+' */
    dec_assert_number(1, dst + 8, 0, 0x1f, 2);                         /* "00" */
}

/* the sign-icon sprite stream is resolved via the sheet offset table
 * (sheet + *(int*)(sheet + 6 + idx*4)), NOT sheet + idx directly. Install a
 * sheet whose table[0x83]/[0x84] hold distinct non-identity offsets and confirm
 * the resolved stream uses those table values; also confirm the stride argument
 * is forwarded verbatim to the sign-icon blit. */
static int32 g_signmod_sheet[2 + 256];
static void test_signmod_sprite_table_indexing_and_stride(void)
{
    uint8 *base = (uint8 *)g_signmod_sheet;
    uint32 dst = 0x340000;
    uint32 stride = 0x1c8;
    int i;

    for (i = 0; i < 256; i++) {
        *(int32 *)(base + 6 + i * 4) = i;       /* default identity */
    }
    *(int32 *)(base + 6 + 0x83 * 4) = 0x511;    /* non-identity for '+' icon */
    *(int32 *)(base + 6 + 0x84 * 4) = 0x733;    /* non-identity for '-' icon */
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)base;
    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;

    /* positive -> '+' icon via table[0x83] = 0x511 */
    fd2_render_signed_modifier_with_icon(dst, stride, 7);
    ASSERT_EQ((long)g_rle_blit_log_sprite[0], (long)((uint32)base + 0x511));
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)dst);
    /* stride forwarded to the sign-icon blit */
    ASSERT_EQ((long)g_rle_blit_last_stride, (long)stride);

    /* negative -> '-' icon via table[0x84] = 0x733 */
    g_rle_blit_calls = 0;
    fd2_render_signed_modifier_with_icon(dst, stride, -7);
    ASSERT_EQ((long)g_rle_blit_log_sprite[0], (long)((uint32)base + 0x733));
}

/* the abs() magnitude flows through the 2-digit overflow rule of the decimal
 * renderer: abs(-150) = 150 >= 100 -> the renderer emits the single fixed "99+"
 * glyph (sprite 0x5D) instead of two digits, so the icon (0x84) plus one
 * overflow glyph = 2 blits total. Locks that the abs result (not the raw value)
 * drives the magnitude path. */
static void test_signmod_negative_magnitude_overflow(void)
{
    uint32 sheet;
    uint32 dst = 0x380000;

    sheet = signmod_setup();
    fd2_render_signed_modifier_with_icon(dst, 0x140, -150);

    /* icon + single "99+" overflow glyph */
    ASSERT_EQ((long)g_rle_blit_calls, 2);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - sheet), (long)0x84);   /* '-' */
    /* overflow placeholder 0x5D at dst+8 (abs=150 >= 100, 2-digit overflow) */
    dec_assert_overflow(1, dst + 8, 0x5d);
}

/* ----------------------------------------------------------------
 * fd2_render_party_status_overview_content @ 0x1b41d
 *
 * Renders the whole static "Army Status" overview onto dst_surface at the
 * given stride. Driven end-to-end here over in-memory fixtures:
 *   - 4 icon labels via the REAL fd2_blit_indexed_sprite_at_xy ->
 *     fd2_rle_blit_sprite spy (sprite idx 0x85..0x88)
 *   - 6 decimal fields (chapter+1, turn, gold, 3 team alive counts) via the
 *     REAL fd2_render_decimal_number_to_buffer -> rle spy
 *   - 2 chapter title/subtitle dialogs via the REAL fd2_display_dialog_scene
 *     against an immediate-END / single-glyph text program
 * fd2_count_active_chars_for_team_filter is emitted for real; the overview
 * tests seed g_test_rc_array with a known per-team alive distribution so its
 * counts flow into the per-team decimal renders, exercising the
 * "CALL then PUSH EAX" return-value plumbing. fd2_check_party_has_char_id is
 * still faked in testglob.c.
 * ---------------------------------------------------------------- */
extern uint32 g_dlg_glyph_last_idx;

/* In-memory template-roster fixture driving the REAL fd2_check_party_has_char_id
 * (src/util/misc.c) that the overview renderer calls on the Mitti chapter. The
 * function scans data_fd2_shared_menu_party_roster_buffer_ptr as 0x50-byte
 * entries (count = data_fd2_shared_menu_party_member_count) for the char_id byte
 * at +0x08. We seed it to make char_id 0x12 (蜜蒂) present or absent, so the real
 * return value gates the subtitle-page branch — no fake seam. */
#define OV_TMPL_STRIDE  0x50
#define OV_TMPL_SLOTS   8
static uint8 g_ov_tmpl_roster[OV_TMPL_STRIDE * OV_TMPL_SLOTS];

/* Seed the roster so fd2_check_party_has_char_id(0x12) returns has_mitti.
 * Every char_id byte is the 0xFF sentinel (never 0x12); when has_mitti, slot 1
 * carries 0x12 so the scan finds it. */
static void ov_roster_set_mitti(int has_mitti)
{
    int i;

    for (i = 0; i < (int)sizeof(g_ov_tmpl_roster); i++) {
        g_ov_tmpl_roster[i] = 0xFF;
    }
    if (has_mitti) {
        g_ov_tmpl_roster[1 * OV_TMPL_STRIDE + 8] = 0x12;
    }
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ov_tmpl_roster;
    data_fd2_shared_menu_party_member_count = (uint32)OV_TMPL_SLOTS;
}

/* shared text buffer for the overview dialog-plumbing tests */
static uint16 g_ov_text[0x400];

/* point every page word at an immediate END marker parked high in the buffer */
static void ov_text_all_end(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        g_ov_text[i] = 0;
    }
    *(int16 *)((uint8 *)g_ov_text + 0x780) = -1;      /* END marker */
    for (i = 0; i < 0x3c0; i++) {
        g_ov_text[i] = (uint16)0x780;                 /* byte offset of END */
    }
    data_fd2_all_game_text_ptr = (uint32)g_ov_text;
}

/* all 23 sprite/decimal blits land at the right surface offsets with the right
 * sprite indices and field values; the 3 team alive counts carry the fake
 * fd2_count_active_chars_for_team_filter return for teams 0, 2, 1 (that call
 * order, mirroring the binary). */
static void test_overview_static_blits(void)
{
    uint32 sheet;
    uint32 buf    = 0x100000;
    uint32 stride = 0x140;
    uint32 row;

    sheet = bar_setup_sheet();
    ov_text_all_end();              /* both dialogs return at once, no glyphs */
    panel_reset_logs();
    g_dlg_glyph_calls = 0;

    data_fd2_chapter_current_chapter_id = 5;      /* number = 6, off Mitti case */
    data_fd2_battle_turn_counter        = 123;
    data_fd2_shared_party_total_gold    = 1234;
    /* Seed a known per-team alive distribution for the REAL counter:
     * team 0 (ENEMY) = 3, team 2 (PLAYER) = 4, team 1 (NPC ALLY) = 1.
     * memset leaves portrait_id/archetype_flag = 0 (both pass the filter)
     * and .flags == 0 keeps every char alive for the real is_dead check. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].team = 0;
    g_test_rc_array[2].team = 0;
    g_test_rc_array[3].team = 2;
    g_test_rc_array[4].team = 2;
    g_test_rc_array[5].team = 2;
    g_test_rc_array[6].team = 2;
    g_test_rc_array[7].team = 1;
    data_fd2_battle_party_member_count = 8;
    /* chapter 5 (off the Mitti branch): the has-char query is never made. */

    fd2_render_party_status_overview_content(buf, stride);

    /* 4 icon labels (sprite idx == logged sprite - sheet under table[i]=i) */
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - sheet), 0x85);
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)(buf + 0x6d + stride * 0x13));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[1] - sheet), 0x86);
    ASSERT_EQ((long)g_rle_blit_log_dst[1], (long)(buf + 0x4b + stride * 0x25));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[2] - sheet), 0x87);
    ASSERT_EQ((long)g_rle_blit_log_dst[2], (long)(buf + 0x4b + stride * 0x9b));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[3] - sheet), 0x88);
    ASSERT_EQ((long)g_rle_blit_log_dst[3], (long)(buf + 0x81 + stride * 0xac));

    /* chapter number = chapter_id + 1 = 6, white 0x2a, 2 digits */
    dec_assert_number(4, buf + 0x8f + stride * 0x18, 6, 0x2a, 2);
    /* turn counter = 123, white 0x2a, 3 digits */
    dec_assert_number(6, buf + 0xbc + stride * 0x18, 123, 0x2a, 3);
    /* gold = 1234, yellow 0x1f, 8 digits */
    dec_assert_number(9, buf + 0x8c + stride * 0xb0, 1234, 0x1f, 8);

    /* per-team alive counts: team 0 then team 2 then team 1 (binary order),
     * each white 0x2a / 2 digits, sharing row offset stride*0x9f. Values are
     * what the REAL counter returns for the seeded distribution; the three
     * renders landing at the distinct team rows prove the renderer invoked the
     * counter once per team. */
    row = stride * 0x9f;
    dec_assert_number(17, buf + 0x78 + row, 3, 0x2a, 2);   /* team 0 -> 3 */
    dec_assert_number(19, buf + 0xb6 + row, 4, 0x2a, 2);   /* team 2 -> 4 */
    dec_assert_number(21, buf + 0xe4 + row, 1, 0x2a, 2);   /* team 1 -> 1 */

    ASSERT_EQ((long)g_rle_blit_calls, 23);
    /* the dialogs rendered nothing (immediate END) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    data_fd2_battle_party_member_count = 4;
}

/* chapter id 0x10 (Mitti chapter) with NO Mitti in party (has-char fake 0):
 * the subtitle base text_id is shifted by -2, so the subtitle dialog uses page
 * (chapter*2 + 0x253) + 1 = 0x274. We point that page at a 1-glyph body and the
 * normal subtitle page (0x276) at a different glyph; observing the 0x274 glyph
 * proves the branch was taken. Also verifies the has-char query used arg 0x12. */
static void test_overview_subtitle_mitti_absent(void)
{
    uint32 buf    = 0x100000;
    uint32 stride = 0x140;

    bar_setup_sheet();
    ov_text_all_end();
    g_ov_text[0x275] = (uint16)0x780;             /* title -> END (no glyph) */
    g_ov_text[0x274] = (uint16)0x782;             /* Mitti-absent subtitle */
    g_ov_text[0x276] = (uint16)0x786;             /* normal subtitle */
    *(int16 *)((uint8 *)g_ov_text + 0x782) = (int16)0xAA;  /* glyph */
    *(int16 *)((uint8 *)g_ov_text + 0x784) = -1;
    *(int16 *)((uint8 *)g_ov_text + 0x786) = (int16)0xBB;  /* glyph */
    *(int16 *)((uint8 *)g_ov_text + 0x788) = -1;
    panel_reset_logs();

    data_fd2_chapter_current_chapter_id = 0x10;
    data_fd2_battle_turn_counter        = 1;
    data_fd2_shared_party_total_gold    = 0;
    data_fd2_battle_party_member_count = 0;        /* real counter -> 0 */
    ov_roster_set_mitti(0);                        /* 蜜蒂 0x12 NOT in roster */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    fd2_render_party_status_overview_content(buf, stride);

    /* The real fd2_check_party_has_char_id(0x12) returns 0, so the renderer
     * picks the Mitti-absent subtitle page (0x274 -> 0xAA). Exactly one glyph
     * proves the -2-shifted branch was taken via the real query. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0xAA);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(buf + 0x50 + stride * 0x74));
}

/* chapter id 0x10 but Mitti IS in party (has-char fake 1): the -2 shift is NOT
 * applied, so the subtitle uses the normal page (chapter*2 + 0x255) + 1 = 0x276
 * (glyph 0xBB). Confirms the branch is gated by the has-char return. */
static void test_overview_subtitle_mitti_present(void)
{
    uint32 buf    = 0x100000;
    uint32 stride = 0x140;

    bar_setup_sheet();
    ov_text_all_end();
    g_ov_text[0x275] = (uint16)0x780;             /* title -> END */
    g_ov_text[0x274] = (uint16)0x782;
    g_ov_text[0x276] = (uint16)0x786;
    *(int16 *)((uint8 *)g_ov_text + 0x782) = (int16)0xAA;
    *(int16 *)((uint8 *)g_ov_text + 0x784) = -1;
    *(int16 *)((uint8 *)g_ov_text + 0x786) = (int16)0xBB;
    *(int16 *)((uint8 *)g_ov_text + 0x788) = -1;
    panel_reset_logs();

    data_fd2_chapter_current_chapter_id = 0x10;
    data_fd2_battle_turn_counter        = 1;
    data_fd2_shared_party_total_gold    = 0;
    data_fd2_battle_party_member_count = 0;        /* real counter -> 0 */
    ov_roster_set_mitti(1);                        /* 蜜蒂 0x12 IN roster */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    fd2_render_party_status_overview_content(buf, stride);

    /* The real fd2_check_party_has_char_id(0x12) returns 1, so no -2 shift:
     * normal subtitle page (0x276 -> 0xBB) used. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0xBB);
}

/* off the Mitti chapter (id != 0x10) the has-char query is never made and the
 * subtitle uses the normal page (chapter*2 + 0x255) + 1. chapter 5 -> title
 * page 0x25f, subtitle page 0x260; the title renders glyph 0xC1 and the
 * subtitle glyph 0xC2, proving both page indices and the two render positions. */
static void test_overview_title_subtitle_pages_normal(void)
{
    uint32 buf    = 0x100000;
    uint32 stride = 0x140;

    bar_setup_sheet();
    ov_text_all_end();
    g_ov_text[0x25f] = (uint16)0x782;             /* title page */
    g_ov_text[0x260] = (uint16)0x786;             /* subtitle page */
    *(int16 *)((uint8 *)g_ov_text + 0x782) = (int16)0xC1;
    *(int16 *)((uint8 *)g_ov_text + 0x784) = -1;
    *(int16 *)((uint8 *)g_ov_text + 0x786) = (int16)0xC2;
    *(int16 *)((uint8 *)g_ov_text + 0x788) = -1;
    panel_reset_logs();

    data_fd2_chapter_current_chapter_id = 5;
    data_fd2_battle_turn_counter        = 1;
    data_fd2_shared_party_total_gold    = 0;
    data_fd2_battle_party_member_count = 0;        /* real counter -> 0 */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    fd2_render_party_status_overview_content(buf, stride);

    /* chapter 5 (not the Mitti chapter): the has-char query is never made, so
     * the subtitle uses the normal page unconditionally.
     * title then subtitle: 2 glyphs, last is the subtitle (0xC2) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0xC2);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(buf + 0x50 + stride * 0x74));
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
    RUN_TEST(test_dec_three_digit_basic);
    RUN_TEST(test_dec_two_digit_basic);
    RUN_TEST(test_dec_negative_clamps_to_zero);
    RUN_TEST(test_dec_three_digit_overflow_max);
    RUN_TEST(test_dec_three_digit_overflow_boundary);
    RUN_TEST(test_dec_two_digit_overflow_99plus);
    RUN_TEST(test_dec_two_digit_overflow_boundary);
    RUN_TEST(test_dec_color_base_applied);
    RUN_TEST(test_dec_four_digits_no_overflow_guard);
    RUN_TEST(test_mini_background_blit);
    RUN_TEST(test_mini_bars_origins_and_segments);
    RUN_TEST(test_mini_numbers_order_and_colors);
    RUN_TEST(test_mini_numbers_red_when_full);
    RUN_TEST(test_mini_sign_extension_and_guards);
    RUN_TEST(test_mini_name_label_page_and_pos);
    RUN_TEST(test_mini_char_idx_selects_slot);
    RUN_TEST(test_hud_gate_user_disabled);
    RUN_TEST(test_hud_gate_play_inactive);
    RUN_TEST(test_hud_position_right_and_backdrop);
    RUN_TEST(test_hud_position_left_and_modifiers);
    RUN_TEST(test_hud_position_keep_previous);
    RUN_TEST(test_hud_position_low_y_keeps_previous);
    RUN_TEST(test_hud_char_present_portrait_and_hp);
    RUN_TEST(test_hud_char_hidden_portrait_excluded);
    RUN_TEST(test_hud_char_archetype10_enemy_excluded);
    RUN_TEST(test_hud_char_palette_idx3_remaps_to_1);
    RUN_TEST(test_signmod_positive_plus_icon);
    RUN_TEST(test_signmod_negative_minus_icon_abs);
    RUN_TEST(test_signmod_zero_is_positive);
    RUN_TEST(test_signmod_sprite_table_indexing_and_stride);
    RUN_TEST(test_signmod_negative_magnitude_overflow);
    RUN_TEST(test_overview_static_blits);
    RUN_TEST(test_overview_subtitle_mitti_absent);
    RUN_TEST(test_overview_subtitle_mitti_present);
    RUN_TEST(test_overview_title_subtitle_pages_normal);
    g_blitraw_log_on = 0;
    g_rle_blit_log_on = 0;
    printf("\n");
}
