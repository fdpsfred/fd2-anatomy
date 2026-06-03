/*
 * unit tests for src/gfx/blitspr.c
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/*
 * Per-row strided copy: copy a 3-row x 4-byte block out of a wider source
 * (src stride 8) into a wider destination (dst stride 6). Only the w_bytes
 * window of each row is touched; bytes outside the window must stay intact,
 * and the stride advance must select the right source/destination rows.
 */
static void test_blit_strided_window(void)
{
    static uint8 src[3 * 8];
    static uint8 dst[3 * 6];
    int r, c;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 8; c++) {
            src[r * 8 + c] = (uint8)(0x10 * (r + 1) + c);
        }
    }
    memset(dst, 0xEE, sizeof(dst));

    fd2_blit_rectangle((uint32)dst, 6, (uint32)src, 8, 4, 3);

    /* each dst row got src[row][0..3]; dst[row][4..5] untouched (0xEE) */
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            ASSERT_EQ((long)dst[r * 6 + c], (long)(uint8)(0x10 * (r + 1) + c));
        }
        ASSERT_EQ((long)dst[r * 6 + 4], 0xEE);
        ASSERT_EQ((long)dst[r * 6 + 5], 0xEE);
    }
}

/*
 * Each per-row copy is memmove, not memcpy: with src and dst overlapping
 * within a single row (dst = src + 1, forward overlap), memmove preserves the
 * original source bytes while a naive forward byte copy would smear buf[0]
 * across the whole window. Single row (height 1) isolates the per-row copy.
 */
static void test_blit_overlap_memmove(void)
{
    static uint8 buf[8];
    int i;

    for (i = 0; i < 8; i++) {
        buf[i] = (uint8)(i + 1);   /* 1,2,3,4,5,6,7,8 */
    }

    /* copy buf[0..5] -> buf[1..6], one row of 6 bytes, overlapping forward */
    fd2_blit_rectangle((uint32)(buf + 1), 6, (uint32)buf, 6, 6, 1);

    ASSERT_EQ((long)buf[0], 1);          /* source head untouched */
    /* buf[1..6] == original buf[0..5] == 1,2,3,4,5,6 (memmove semantics) */
    for (i = 1; i <= 6; i++) {
        ASSERT_EQ((long)buf[i], (long)i);
    }
    ASSERT_EQ((long)buf[7], 8);          /* tail untouched */
}

/*
 * Signed height compare (asm: CMP EBX,height / JL): height 0 and any negative
 * height copy zero rows, leaving dst pristine. Guards against an unsigned
 * compare that would treat a negative height as a huge loop count.
 */
static void test_blit_nonpositive_height(void)
{
    static uint8 src[4];
    static uint8 dst[4];

    memset(src, 0x55, sizeof(src));
    memset(dst, 0xCC, sizeof(dst));

    fd2_blit_rectangle((uint32)dst, 4, (uint32)src, 4, 4, 0);
    ASSERT_EQ((long)dst[0], 0xCC);

    fd2_blit_rectangle((uint32)dst, 4, (uint32)src, 4, 4, (uint32)-1);
    ASSERT_EQ((long)dst[0], 0xCC);
    ASSERT_EQ((long)dst[3], 0xCC);
}

/* capture vars from testglob.c stubs of the two callees */
extern uint32 g_blitsetup_dst, g_blitsetup_sprite, g_blitsetup_stride;
extern uint32 g_saveblk_out, g_saveblk_w, g_saveblk_h, g_saveblk_dst,
              g_saveblk_src, g_saveblk_stride;

/*
 * fd2_blit_indexed_sprite_with_alloc wires its 5 cdecl args into:
 *   - sprite_data = sprite_idx * dst_pitch + sheet_base
 *   - malloc(width*height + 8) with width/height read as signed 16-bit
 *     words from sprite_hdr[0] and sprite_hdr[2]
 *   - fd2_save_screen_block_to_buffer(save_buf, width, height, dst,
 *                                     sprite_data, dst_pitch)
 *   - fd2_blit_sprite_with_stride_setup(sprite_data + dst, sprite_hdr,
 *                                       dst_pitch)
 *   - returns the malloc'd save_buf
 * Drive it with distinct values and verify every captured argument plus
 * the returned pointer; the save buffer must be at least width*height+8.
 */
static void test_indexed_sprite_alloc_wiring(void)
{
    static int16 hdr[2];
    uint32 sheet[64];      /* backing for sprite_data address arithmetic */
    void *ret;
    uint32 expect_sprite_data;
    uint32 dst, dst_pitch, sprite_idx;

    hdr[0] = 6;            /* width  */
    hdr[1] = 5;            /* height */

    dst = 0xA0000;
    dst_pitch = 0x140;
    sprite_idx = 3;
    /* sheet_base is just a base value combined arithmetically */
    expect_sprite_data = sprite_idx * dst_pitch + (uint32)sheet;

    ret = fd2_blit_indexed_sprite_with_alloc((uint32)hdr, dst, dst_pitch,
                                             (uint32)sheet, sprite_idx);

    ASSERT_TRUE(ret != NULL);

    /* save-under snapshot call */
    ASSERT_EQ((long)g_saveblk_out, (long)(uint32)ret);
    ASSERT_EQ((long)g_saveblk_w, 6);
    ASSERT_EQ((long)g_saveblk_h, 5);
    ASSERT_EQ((long)g_saveblk_dst, (long)dst);
    ASSERT_EQ((long)g_saveblk_src, (long)expect_sprite_data);
    ASSERT_EQ((long)g_saveblk_stride, (long)dst_pitch);

    /* sprite blit call: dst arg = sprite_data + dst */
    ASSERT_EQ((long)g_blitsetup_dst, (long)(expect_sprite_data + dst));
    ASSERT_EQ((long)g_blitsetup_sprite, (long)(uint32)hdr);
    ASSERT_EQ((long)g_blitsetup_stride, (long)dst_pitch);

    free(ret);
}

/*
 * width/height are MOVSX'd from 16-bit words: a header word of 0xFFFF
 * means -1, so width*height is a signed product. Confirm the signed
 * read by using a header whose raw word would be a huge value if read
 * unsigned but is a small signed product here (-1 * -1 == 1, so the
 * malloc size is 1 + 8 = 9, not a multi-GB request that would fail).
 */
static void test_indexed_sprite_signed_dims(void)
{
    static int16 hdr[2];
    uint32 sheet[8];
    void *ret;

    hdr[0] = -1;          /* 0xFFFF read as signed short */
    hdr[1] = -1;

    ret = fd2_blit_indexed_sprite_with_alloc((uint32)hdr, 0, 0x10,
                                             (uint32)sheet, 0);

    /* (-1)*(-1) + 8 = 9 bytes; alloc must succeed (proves signed read) */
    ASSERT_TRUE(ret != NULL);
    ASSERT_EQ((long)g_saveblk_w, -1);
    ASSERT_EQ((long)g_saveblk_h, -1);

    free(ret);
}

void run_gfx_blitspr_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/blitspr\n");
    RUN_TEST(test_blit_strided_window);
    RUN_TEST(test_blit_overlap_memmove);
    RUN_TEST(test_blit_nonpositive_height);
    RUN_TEST(test_indexed_sprite_alloc_wiring);
    RUN_TEST(test_indexed_sprite_signed_dims);
    printf("\n");
}
