/*
 * unit tests for src/util/misc.c
 *
 * Covers fd2_any_char_has_item @ 0x24B14: scans chars 0..15 calling
 * fd2_find_inventory_slot_with_item(char, item) and returns 1 on the first
 * hit (slot != -1), else -1. Driven against the real function through the
 * programmable fd2_find_inventory_slot_with_item double in tests/testglob.c:
 *   - g_ce_find_have_item100 makes char 0 hold item 100 (天空之鑰);
 *   - g_ce_find_have_d6 makes char 5 hold item 0xD6;
 *   - g_ce_find_calls counts the find calls, which proves the loop bound and
 *     the early-return index.
 */

#include "testharn.h"
#include "types.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

extern int g_ce_find_have_d6;
extern int g_ce_find_have_item100;
extern int g_ce_find_calls;

/* ----------------------------------------------------------------
 * Hit on the very first char (char 0): early return with EAX=1 after a
 * single find call.
 * ---------------------------------------------------------------- */
static void test_any_char_has_item_found_first_char(void)
{
    int result;

    g_ce_find_have_d6 = 0;
    g_ce_find_have_item100 = 1;   /* char 0 holds item 100 */
    g_ce_find_calls = 0;

    result = fd2_any_char_has_item(100);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(g_ce_find_calls, 1);   /* returned on the first char */
}

/* ----------------------------------------------------------------
 * Hit mid-loop (char 5 holds 0xD6): the scan walks chars 0..5 and returns 1
 * on the 6th find call. Proves traversal past char 0 plus early return at a
 * non-zero index.
 * ---------------------------------------------------------------- */
static void test_any_char_has_item_found_mid_loop(void)
{
    int result;

    g_ce_find_have_item100 = 0;
    g_ce_find_have_d6 = 1;        /* char 5 holds item 0xD6 */
    g_ce_find_calls = 0;

    result = fd2_any_char_has_item(0xD6);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(g_ce_find_calls, 6);   /* chars 0..5, hit on the 6th */
}

/* ----------------------------------------------------------------
 * No char holds the item: the full 16-char loop runs (16 find calls) and the
 * fallback path returns -1 (EAX=0xFFFFFFFF). Proves the 0x10 loop bound and
 * the not-found return.
 * ---------------------------------------------------------------- */
static void test_any_char_has_item_not_found(void)
{
    int result;

    g_ce_find_have_d6 = 0;
    g_ce_find_have_item100 = 0;   /* nobody holds item 100 */
    g_ce_find_calls = 0;

    result = fd2_any_char_has_item(100);

    ASSERT_EQ(result, -1);
    ASSERT_EQ(g_ce_find_calls, 16);  /* scanned all 16 chars */
}

void run_util_misc_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: util/misc\n");
    RUN_TEST(test_any_char_has_item_found_first_char);
    RUN_TEST(test_any_char_has_item_found_mid_loop);
    RUN_TEST(test_any_char_has_item_not_found);
    printf("\n");
}
