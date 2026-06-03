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

void run_gfx_rndstat_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/rndstat\n");
    RUN_TEST(test_normal_path_left_slot);
    RUN_TEST(test_mirrored_path_right_slot);
    RUN_TEST(test_frame_index_selects_int_entry);
    RUN_TEST(test_normal_path_zero_offset);
    printf("\n");
}
