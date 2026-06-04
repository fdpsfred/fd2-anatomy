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
    g_blitraw_log_on = 0;
    printf("\n");
}
