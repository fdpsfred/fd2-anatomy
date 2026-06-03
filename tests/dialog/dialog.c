/*
 * unit tests for src/dialog/dialog.c
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* recording stub for fd2_restore_screen_block_from_buffer (testglob.c) */
extern int    g_restore_block_calls;
extern uint32 g_restore_block_last_buf;
extern uint32 g_restore_block_last_dst;
extern uint32 g_restore_block_last_stride;

/*
 * fd2_cleanup_dialog_sprite_buffer forwards (saved_block, dst, stride) to
 * fd2_restore_screen_block_from_buffer in that exact order, then frees
 * saved_block. Assert the three args land in the right slots (guards against
 * a swapped argument order) and that the restore happens exactly once.
 */
static void test_cleanup_forwards_args(void)
{
    void *blk;

    blk = malloc(64);
    ASSERT_TRUE(blk != NULL);

    g_restore_block_calls = 0;
    g_restore_block_last_buf = 0;
    g_restore_block_last_dst = 0;
    g_restore_block_last_stride = 0;

    fd2_cleanup_dialog_sprite_buffer((uint32)blk, 0xA0000u, 320u);

    ASSERT_EQ((long)g_restore_block_calls, 1);
    ASSERT_EQ((long)g_restore_block_last_buf, (long)(uint32)blk);
    ASSERT_EQ((long)g_restore_block_last_dst, (long)0xA0000u);
    ASSERT_EQ((long)g_restore_block_last_stride, (long)320u);
}

/*
 * The buffer passed as saved_block must be returned to the heap. malloc a
 * block, run cleanup (which frees it), then malloc the same size again: a
 * correctly-freed block is reused by the allocator, so we get the identical
 * address back. (If cleanup leaked the block, the second malloc would have to
 * carve fresh storage and return a different address.)
 */
static void test_cleanup_frees_buffer(void)
{
    void *blk;
    void *again;

    blk = malloc(128);
    ASSERT_TRUE(blk != NULL);

    fd2_cleanup_dialog_sprite_buffer((uint32)blk, 0xA0000u, 320u);

    again = malloc(128);
    ASSERT_EQ((long)(uint32)again, (long)(uint32)blk);
    free(again);
}

void run_dialog_dialog_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: dialog/dialog\n");
    RUN_TEST(test_cleanup_forwards_args);
    RUN_TEST(test_cleanup_frees_buffer);
    printf("\n");
}
