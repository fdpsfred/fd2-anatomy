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

/* ---- in-memory template-roster fixture for fd2_find_template_char_by_id ----
 * The function scans data_fd2_shared_menu_party_roster_buffer_ptr as an array
 * of 0x50-byte entries, count = data_fd2_shared_menu_party_member_count, and
 * matches the char_id byte at offset +0x08. We back it with a real buffer and
 * point the globals at it (pure in-memory; no game files involved). */
#define TMPL_STRIDE   0x50
#define TMPL_SLOTS    8
static uint8 g_tmpl_roster[TMPL_STRIDE * TMPL_SLOTS];

static void tmpl_roster_reset(int count)
{
    int i;
    for (i = 0; i < (int)sizeof(g_tmpl_roster); i++) {
        g_tmpl_roster[i] = 0xFF;   /* fill with a sentinel != any test id */
    }
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_tmpl_roster;
    data_fd2_shared_menu_party_member_count = (uint32)count;
}

static void tmpl_roster_set_char_id(int slot, uint8 char_id)
{
    g_tmpl_roster[slot * TMPL_STRIDE + 8] = char_id;
}

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

/* ----------------------------------------------------------------
 * fd2_find_template_char_by_id: match in the very first slot (idx 0).
 * Early return 1.
 * ---------------------------------------------------------------- */
static void test_find_template_char_found_first(void)
{
    int result;

    tmpl_roster_reset(TMPL_SLOTS);
    tmpl_roster_set_char_id(0, 0x12);   /* slot 0 holds char_id 0x12 (蜜蒂) */

    result = fd2_find_template_char_by_id(0x12);

    ASSERT_EQ(result, 1);
}

/* ----------------------------------------------------------------
 * fd2_find_template_char_by_id: match at a non-zero slot. Proves the
 * 0x50-byte stride and +0x08 offset, and that the scan walks past
 * earlier (non-matching) entries before returning 1.
 * ---------------------------------------------------------------- */
static void test_find_template_char_found_mid(void)
{
    int result;

    tmpl_roster_reset(TMPL_SLOTS);
    tmpl_roster_set_char_id(5, 0x0C);   /* slot 5 holds char_id 0x0C (凱麗) */

    result = fd2_find_template_char_by_id(0x0C);

    ASSERT_EQ(result, 1);
}

/* ----------------------------------------------------------------
 * fd2_find_template_char_by_id: id absent from all populated slots.
 * The scan exhausts the count and returns 0.
 * ---------------------------------------------------------------- */
static void test_find_template_char_not_found(void)
{
    int result;

    tmpl_roster_reset(TMPL_SLOTS);
    tmpl_roster_set_char_id(2, 0x05);
    tmpl_roster_set_char_id(4, 0x07);   /* present ids, but not the query */

    result = fd2_find_template_char_by_id(0x12);

    ASSERT_EQ(result, 0);
}

/* ----------------------------------------------------------------
 * fd2_find_template_char_by_id: empty roster (count 0). The loop body
 * never runs even though a matching byte sits in slot 0's memory, so the
 * function returns 0. Proves the count gate, not the buffer contents.
 * ---------------------------------------------------------------- */
static void test_find_template_char_empty_roster(void)
{
    int result;

    tmpl_roster_reset(0);
    tmpl_roster_set_char_id(0, 0x12);   /* byte present but count == 0 */

    result = fd2_find_template_char_by_id(0x12);

    ASSERT_EQ(result, 0);
}

void run_util_misc_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: util/misc\n");
    RUN_TEST(test_any_char_has_item_found_first_char);
    RUN_TEST(test_any_char_has_item_found_mid_loop);
    RUN_TEST(test_any_char_has_item_not_found);
    RUN_TEST(test_find_template_char_found_first);
    RUN_TEST(test_find_template_char_found_mid);
    RUN_TEST(test_find_template_char_not_found);
    RUN_TEST(test_find_template_char_empty_roster);
    printf("\n");
}
