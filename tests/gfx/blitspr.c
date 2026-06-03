/*
 * unit tests for src/gfx/blitspr.c
 */

#include <string.h>
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

void run_gfx_blitspr_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/blitspr\n");
    RUN_TEST(test_blit_strided_window);
    RUN_TEST(test_blit_overlap_memmove);
    RUN_TEST(test_blit_nonpositive_height);
    printf("\n");
}
