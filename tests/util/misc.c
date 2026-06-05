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

/* Active-party array (defined in tests/testglob.c) backing
 * data_fd2_battle_runtime_char_array_ptr; the require-char-id tests drive
 * its char_id bytes directly. */
extern runtime_char g_test_rc_array[8];

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

/* ================================================================
 * fd2_require_char_id_in_active_party @ 0x31DBE
 *
 * Found-path coverage (return 1, no side effects). The function scans
 * the active party array data_fd2_battle_runtime_char_array_ptr at
 * slots [1 .. max_chars], comparing each entry's char_id byte (+0x08)
 * against (uint8)req_char_id; if any match it sets found=1 (and keeps
 * scanning — no early return); on a miss it returns 0 after emitting an
 * error dialog. These tests drive the in-memory g_test_rc_array (backing
 * data_fd2_battle_runtime_char_array_ptr) and assert the return value on
 * the found path, which exercises all of the load-bearing scan logic:
 * the slot-(iter+1) offset, the 0x50-byte stride, the inclusive cap, and
 * the low-byte comparison.
 *
 * The MISSING path (return 0) is deferred to Phase 9 integration: on a
 * miss the function calls fd2_wait_for_input_dialog_with_blink(0), which
 * is the REAL blocking busy-wait released only by async keyboard input
 * (the same deferral testglob.c already documents for every caller that
 * reaches that wait), after running the real heavy chapter-portrait load
 * and dialog-VM render. See src/emit_issues.json @00031dbe.
 * ================================================================ */

/* Zero the active-party array and set a sentinel char_id (0xFF, distinct
 * from every id queried below) in every slot, so only the slots a test
 * explicitly populates can match. */
static void rcp_reset(void)
{
    int i;
    for (i = 0; i < 8; i++) {
        g_test_rc_array[i].char_id = 0xFF;
    }
}

/* ----------------------------------------------------------------
 * Match at slot 1 (the first checked slot; slot 0 is skipped). With a
 * cap of 7, slots 1..7 are scanned. Returns 1.
 * ---------------------------------------------------------------- */
static void test_require_char_found_first_slot(void)
{
    char result;

    rcp_reset();
    g_test_rc_array[1].char_id = 0x0A;   /* slot 1 holds the required id */

    result = fd2_require_char_id_in_active_party(7, 0x0A);

    ASSERT_EQ((int)result, 1);
}

/* ----------------------------------------------------------------
 * Match at the last checked slot (slot == max_chars). With cap 7 the id
 * sits in slot 7; the scan must reach iter==6 (slot 7) to find it. Proves
 * the inclusive [1..max_chars] range, the (iter+1) offset and the 0x50
 * stride, and that the loop does not stop short. Returns 1.
 * ---------------------------------------------------------------- */
static void test_require_char_found_last_slot(void)
{
    char result;

    rcp_reset();
    g_test_rc_array[7].char_id = 0x15;   /* slot 7 == max_chars holds the id */

    result = fd2_require_char_id_in_active_party(7, 0x15);

    ASSERT_EQ((int)result, 1);
}

/* ----------------------------------------------------------------
 * The required id is present in the array but ONLY outside the scan
 * window: in slot 0 (the always-present lord, never checked) and in a
 * slot beyond the cap. A second, in-range slot also carries it, so the
 * function still returns 1 — proving the in-range hit is what counts
 * while the scan window stays bounded by (iter+1) on the low end. A
 * smaller cap (3) is used so slots >3 are out of range.
 * ---------------------------------------------------------------- */
static void test_require_char_found_inrange_with_out_of_window_copies(void)
{
    char result;

    rcp_reset();
    g_test_rc_array[0].char_id = 0x20;   /* lord slot: skipped */
    g_test_rc_array[2].char_id = 0x20;   /* in range (1..3): the real hit */
    g_test_rc_array[6].char_id = 0x20;   /* beyond cap 3: out of window */

    result = fd2_require_char_id_in_active_party(3, 0x20);

    ASSERT_EQ((int)result, 1);
}

/* ----------------------------------------------------------------
 * Only the low byte of req_char_id is compared: the binary loads the
 * required id with MOVZX from a single byte, so a wide argument whose
 * low byte equals an in-range slot's char_id must still match. Passing
 * 0x1100 | 0x0C with slot 4 holding 0x0C returns 1, pinning the
 * (uint8)req_char_id narrowing. Returns 1.
 * ---------------------------------------------------------------- */
static void test_require_char_found_low_byte_only(void)
{
    char result;

    rcp_reset();
    g_test_rc_array[4].char_id = 0x0C;   /* slot 4 holds low byte 0x0C */

    result = fd2_require_char_id_in_active_party(7, 0x1100 | 0x0C);

    ASSERT_EQ((int)result, 1);
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
    RUN_TEST(test_require_char_found_first_slot);
    RUN_TEST(test_require_char_found_last_slot);
    RUN_TEST(test_require_char_found_inrange_with_out_of_window_copies);
    RUN_TEST(test_require_char_found_low_byte_only);
    printf("\n");
}
