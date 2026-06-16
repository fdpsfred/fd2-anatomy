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
#include <string.h>

extern int g_ce_find_have_d6;
extern int g_ce_find_have_item100;
extern int g_ce_find_calls;

/* fd2_delay_ms recorder (tests/testglob.c): the stub increments
 * g_delay375b2_calls and stores the last ticks argument, which lets the
 * delay-wrapper test prove the exact 400-tick argument and single call. */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;

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

/* ----------------------------------------------------------------
 * fd2_check_party_has_char_id @ 0x33499: byte-identical twin of
 * fd2_find_template_char_by_id. Reuses the same in-memory template-roster
 * fixture (tmpl_roster_reset / tmpl_roster_set_char_id).
 *
 * Match in the very first slot (idx 0): early return 1.
 * ---------------------------------------------------------------- */
static void test_check_party_has_char_found_first(void)
{
    uint32 result;

    tmpl_roster_reset(TMPL_SLOTS);
    tmpl_roster_set_char_id(0, 0x0C);   /* slot 0 holds char_id 0x0C (凱麗) */

    result = fd2_check_party_has_char_id(0x0C);

    ASSERT_EQ((int)result, 1);
}

/* ----------------------------------------------------------------
 * fd2_check_party_has_char_id: match at a non-zero slot. Proves the
 * 0x50-byte stride and +0x08 offset, walking past earlier entries.
 * ---------------------------------------------------------------- */
static void test_check_party_has_char_found_mid(void)
{
    uint32 result;

    tmpl_roster_reset(TMPL_SLOTS);
    tmpl_roster_set_char_id(6, 0x12);   /* slot 6 holds char_id 0x12 (蜜蒂) */

    result = fd2_check_party_has_char_id(0x12);

    ASSERT_EQ((int)result, 1);
}

/* ----------------------------------------------------------------
 * fd2_check_party_has_char_id: id absent from all populated slots.
 * The scan exhausts the count and returns 0.
 * ---------------------------------------------------------------- */
static void test_check_party_has_char_not_found(void)
{
    uint32 result;

    tmpl_roster_reset(TMPL_SLOTS);
    tmpl_roster_set_char_id(1, 0x03);
    tmpl_roster_set_char_id(3, 0x09);   /* present ids, but not the query */

    result = fd2_check_party_has_char_id(0x12);

    ASSERT_EQ((int)result, 0);
}

/* ----------------------------------------------------------------
 * fd2_check_party_has_char_id: empty roster (count 0). The loop body
 * never runs even though a matching byte sits in slot 0's memory, so the
 * function returns 0. Proves the signed count gate, not buffer contents.
 * ---------------------------------------------------------------- */
static void test_check_party_has_char_empty_roster(void)
{
    uint32 result;

    tmpl_roster_reset(0);
    tmpl_roster_set_char_id(0, 0x12);   /* byte present but count == 0 */

    result = fd2_check_party_has_char_id(0x12);

    ASSERT_EQ((int)result, 0);
}

/* ----------------------------------------------------------------
 * fd2_check_party_has_char_id: full-width comparison. The roster byte is
 * loaded with MOVZX (zero-extended to 32 bits) and compared against the
 * whole uint32 argument — there is no (uint8) narrowing of the argument
 * (unlike the require/pin helpers). So a wide argument whose low byte
 * equals a slot's char_id but whose upper bits are set must NOT match.
 * slot 4 holds 0x12; querying 0x1212 finds nothing and returns 0.
 * ---------------------------------------------------------------- */
static void test_check_party_has_char_wide_arg_no_match(void)
{
    uint32 result;

    tmpl_roster_reset(TMPL_SLOTS);
    tmpl_roster_set_char_id(4, 0x12);   /* byte 0x12 present */

    result = fd2_check_party_has_char_id(0x1212);  /* low byte 0x12, hi set */

    ASSERT_EQ((int)result, 0);
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

/* ================================================================
 * fd2_count_selected_chars @ 0x320CE
 *
 * Counts non-zero bytes in the selection_state array passed by pointer,
 * over the range [0, menu_party_member_count - 1) — the final sentinel
 * slot is excluded by the -1. Returns the count. Pure in-memory: the
 * tests pass a pointer to a local byte array (mirroring the caller, which
 * passes a stack array) and drive the loop bound via the global
 * data_fd2_shared_menu_party_member_count.
 * ================================================================ */

/* ----------------------------------------------------------------
 * Mixed selection: with count == 8 the scan covers slots [0..6] (slot 7
 * is the excluded sentinel). Three in-range slots are non-zero, so the
 * result is 3. Proves the accumulator and the per-byte != 0 test.
 * ---------------------------------------------------------------- */
static void test_count_selected_mixed(void)
{
    unsigned char sel[8];
    int result;

    memset(sel, 0, sizeof(sel));
    data_fd2_shared_menu_party_member_count = 8;   /* scans slots 0..6 */
    sel[0] = 1;
    sel[3] = 1;
    sel[6] = 1;   /* last in-range slot (idx == count-2) */

    result = fd2_count_selected_chars((uint32)sel);

    ASSERT_EQ(result, 3);
}

/* ----------------------------------------------------------------
 * The final slot (index count-1) is the sentinel and is NOT scanned:
 * marking only slot count-1 yields 0. Pins the -1 loop bound (an
 * off-by-one that scanned count slots would return 1 here).
 * ---------------------------------------------------------------- */
static void test_count_selected_last_slot_excluded(void)
{
    unsigned char sel[8];
    int result;

    memset(sel, 0, sizeof(sel));
    data_fd2_shared_menu_party_member_count = 8;   /* slot 7 is the sentinel */
    sel[7] = 1;   /* index count-1: outside the scan window */

    result = fd2_count_selected_chars((uint32)sel);

    ASSERT_EQ(result, 0);
}

/* ----------------------------------------------------------------
 * Every in-range slot selected: with count == 8 slots [0..6] are all
 * non-zero, so the result is count-1 == 7. Proves the full-range walk
 * and that the upper bound is exclusive of count-1.
 * ---------------------------------------------------------------- */
static void test_count_selected_all_in_range(void)
{
    unsigned char sel[8];
    int result;
    int i;

    for (i = 0; i < 8; i++) {
        sel[i] = 1;
    }
    data_fd2_shared_menu_party_member_count = 8;

    result = fd2_count_selected_chars((uint32)sel);

    ASSERT_EQ(result, 7);
}

/* ----------------------------------------------------------------
 * High-bit-set bytes (0xFF, 0x80) count as selected: the binary tests
 * byte != 0, not a signed > 0, so values that are negative when read as
 * a signed char must still increment the count. Two such slots in range
 * (count == 4 -> scans slots 0..2) give 2. Guards against a signedness
 * regression in the != 0 test.
 * ---------------------------------------------------------------- */
static void test_count_selected_high_bit_bytes(void)
{
    unsigned char sel[4];
    int result;

    memset(sel, 0, sizeof(sel));
    data_fd2_shared_menu_party_member_count = 4;   /* scans slots 0..2 */
    sel[0] = 0xFF;
    sel[2] = 0x80;

    result = fd2_count_selected_chars((uint32)sel);

    ASSERT_EQ(result, 2);
}

/* ----------------------------------------------------------------
 * Loop-bound gate: when count <= 1 the bound (count-1) is <= 0, so the
 * loop body never runs and the result is 0 even though slot 0 is marked
 * selected. Proves the count gate rather than the buffer contents.
 * ---------------------------------------------------------------- */
static void test_count_selected_empty_when_count_one(void)
{
    unsigned char sel[4];
    int result;

    memset(sel, 0, sizeof(sel));
    sel[0] = 1;                                    /* marked but unscanned */
    data_fd2_shared_menu_party_member_count = 1;   /* bound = 0: no iterations */

    result = fd2_count_selected_chars((uint32)sel);

    ASSERT_EQ(result, 0);
}

/* ================================================================
 * fd2_reorder_party_by_selection @ 0x320FC
 *
 * Reorders the template roster (data_fd2_shared_menu_party_roster_buffer_ptr,
 * 0x50-byte entries) by sel_state: selected members (sel_state[i] != 0) pack
 * to the front (slots 1..K), unselected drop to the back, slot 0 preserved.
 * It snapshots the roster, then runs two passes over [0, member_count-1)
 * with a SHARED out_slot index (starts at 1, advances across both passes).
 * Source entries come from snapshot index (iter+1), so snapshot slot 0 (the
 * lord) is skipped and slot 0 of the live roster is left untouched. Pure
 * in-memory: a real roster buffer is wired to the globals and each entry is
 * stamped with a unique tag byte so the test can assert exact placement.
 *
 * Tag convention: slot i is filled with byte value g_reorder_tag(i) across
 * the whole 0x50-byte entry; after the reorder we read back the tag at each
 * destination slot to prove which source entry landed there (the entry moved
 * wholesale, not just one field).
 * ================================================================ */

#define REORD_STRIDE   0x50
#define REORD_SLOTS    32          /* 0xA00 / 0x50 = snapshot capacity */
static uint8 g_reorder_roster[REORD_STRIDE * REORD_SLOTS];

/* Distinct, non-zero tag for slot i (0x40 + i keeps every slot's tag unique
 * and clear of 0). */
static uint8 g_reorder_tag(int slot)
{
    return (uint8)(0x40 + slot);
}

/* Fill the roster so slot i is entirely g_reorder_tag(i); point the globals
 * at it and set the member count. */
static void reorder_roster_reset(int count)
{
    int slot;
    int b;
    for (slot = 0; slot < REORD_SLOTS; slot++) {
        for (b = 0; b < REORD_STRIDE; b++) {
            g_reorder_roster[slot * REORD_STRIDE + b] = g_reorder_tag(slot);
        }
    }
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_reorder_roster;
    data_fd2_shared_menu_party_member_count = (uint32)count;
}

/* Tag byte currently at the start of roster slot i. */
static uint8 reorder_slot_tag(int slot)
{
    return g_reorder_roster[slot * REORD_STRIDE];
}

/* ----------------------------------------------------------------
 * Mixed selection. count == 6, so iter runs 0..4 and source entries are
 * snapshot slots 1..5 (slot 0 = lord, never sourced). sel_state marks
 * members at iter 0,3 selected and 1,2,4 unselected. Expected live roster:
 *   slot 0: untouched lord (tag 0x40)
 *   slots 1..2: selected block in original order -> snapshot 1, 4
 *   slots 3..5: unselected block in original order -> snapshot 2, 3, 5
 * Proves the front/back partition, the shared out_slot advancing across both
 * passes, the (iter+1) source mapping, and slot-0 preservation.
 * ---------------------------------------------------------------- */
static void test_reorder_mixed_partition(void)
{
    unsigned char sel[6];

    reorder_roster_reset(6);
    memset(sel, 0, sizeof(sel));
    sel[0] = 1;   /* -> snapshot slot 1 */
    sel[3] = 1;   /* -> snapshot slot 4 */
    /* iter 1,2,4 unselected -> snapshot slots 2,3,5 */

    fd2_reorder_party_by_selection((uint32)sel);

    ASSERT_EQ((int)reorder_slot_tag(0), (int)g_reorder_tag(0));  /* lord kept */
    ASSERT_EQ((int)reorder_slot_tag(1), (int)g_reorder_tag(1));  /* sel iter0 */
    ASSERT_EQ((int)reorder_slot_tag(2), (int)g_reorder_tag(4));  /* sel iter3 */
    ASSERT_EQ((int)reorder_slot_tag(3), (int)g_reorder_tag(2));  /* uns iter1 */
    ASSERT_EQ((int)reorder_slot_tag(4), (int)g_reorder_tag(3));  /* uns iter2 */
    ASSERT_EQ((int)reorder_slot_tag(5), (int)g_reorder_tag(5));  /* uns iter4 */
}

/* ----------------------------------------------------------------
 * All in-range members selected. count == 5 -> iter 0..3, sources snapshot
 * 1..4. Every member is selected, so pass 1 lays them into slots 1..4 in
 * original order and pass 2 copies nothing. Slot 0 untouched. Proves the
 * pure-front path and that out_slot reaches count-1.
 * ---------------------------------------------------------------- */
static void test_reorder_all_selected(void)
{
    unsigned char sel[5];
    int i;

    reorder_roster_reset(5);
    for (i = 0; i < 5; i++) {
        sel[i] = 1;
    }

    fd2_reorder_party_by_selection((uint32)sel);

    ASSERT_EQ((int)reorder_slot_tag(0), (int)g_reorder_tag(0));
    ASSERT_EQ((int)reorder_slot_tag(1), (int)g_reorder_tag(1));
    ASSERT_EQ((int)reorder_slot_tag(2), (int)g_reorder_tag(2));
    ASSERT_EQ((int)reorder_slot_tag(3), (int)g_reorder_tag(3));
    ASSERT_EQ((int)reorder_slot_tag(4), (int)g_reorder_tag(4));
}

/* ----------------------------------------------------------------
 * No member selected. count == 5 -> iter 0..3, sources snapshot 1..4. Pass 1
 * copies nothing; pass 2 lays snapshot 1..4 into slots 1..4 in original
 * order (out_slot still starts at 1). Slot 0 untouched. Confirms the
 * back-block path starts at slot 1, not slot 0.
 * ---------------------------------------------------------------- */
static void test_reorder_none_selected(void)
{
    unsigned char sel[5];

    reorder_roster_reset(5);
    memset(sel, 0, sizeof(sel));

    fd2_reorder_party_by_selection((uint32)sel);

    ASSERT_EQ((int)reorder_slot_tag(0), (int)g_reorder_tag(0));
    ASSERT_EQ((int)reorder_slot_tag(1), (int)g_reorder_tag(1));
    ASSERT_EQ((int)reorder_slot_tag(2), (int)g_reorder_tag(2));
    ASSERT_EQ((int)reorder_slot_tag(3), (int)g_reorder_tag(3));
    ASSERT_EQ((int)reorder_slot_tag(4), (int)g_reorder_tag(4));
}

/* ----------------------------------------------------------------
 * Loop bound + source offset. count == 4 -> iter runs 0..2 only (member at
 * index count-1 == 3 is the sentinel, never processed). Sources are snapshot
 * 1,2,3 (never snapshot 0). Mark iter 0 selected, 1,2 unselected. Even though
 * sel[3] is set, it is outside the scan window and must not move snapshot
 * slot 4 anywhere. Expected: slot1 <- snap1 (sel), slot2 <- snap2, slot3 <-
 * snap3 (unsel); slot 4 keeps its own tag (never written). Pins the count-1
 * bound and the (iter+1) mapping at the high end.
 * ---------------------------------------------------------------- */
static void test_reorder_bound_and_offset(void)
{
    unsigned char sel[5];

    reorder_roster_reset(4);
    memset(sel, 0, sizeof(sel));
    sel[0] = 1;   /* in range, selected   -> snapshot slot 1 */
    /* iter 1,2 unselected -> snapshot slots 2,3 */
    sel[3] = 1;   /* index count-1: sentinel, outside the scan window */

    fd2_reorder_party_by_selection((uint32)sel);

    ASSERT_EQ((int)reorder_slot_tag(0), (int)g_reorder_tag(0));  /* lord kept */
    ASSERT_EQ((int)reorder_slot_tag(1), (int)g_reorder_tag(1));  /* sel iter0 */
    ASSERT_EQ((int)reorder_slot_tag(2), (int)g_reorder_tag(2));  /* uns iter1 */
    ASSERT_EQ((int)reorder_slot_tag(3), (int)g_reorder_tag(3));  /* uns iter2 */
    ASSERT_EQ((int)reorder_slot_tag(4), (int)g_reorder_tag(4));  /* untouched */
}

/* ----------------------------------------------------------------
 * High-bit selection bytes. The binary tests sel_state[i] != 0, not a signed
 * > 0, so 0x80 and 0xFF must count as selected. count == 4 -> iter 0..2.
 * Mark iter 0 = 0xFF, iter 1 = 0x80 (both selected), iter 2 = 0 (unselected).
 * Expected front block snapshot 1,2 then back block snapshot 3. Guards the
 * != 0 byte test against a signedness regression.
 * ---------------------------------------------------------------- */
static void test_reorder_high_bit_selected(void)
{
    unsigned char sel[5];

    reorder_roster_reset(4);
    memset(sel, 0, sizeof(sel));
    sel[0] = 0xFF;   /* selected -> snapshot slot 1 */
    sel[1] = 0x80;   /* selected -> snapshot slot 2 */
    /* iter 2 unselected -> snapshot slot 3 */

    fd2_reorder_party_by_selection((uint32)sel);

    ASSERT_EQ((int)reorder_slot_tag(0), (int)g_reorder_tag(0));
    ASSERT_EQ((int)reorder_slot_tag(1), (int)g_reorder_tag(1));  /* 0xFF sel */
    ASSERT_EQ((int)reorder_slot_tag(2), (int)g_reorder_tag(2));  /* 0x80 sel */
    ASSERT_EQ((int)reorder_slot_tag(3), (int)g_reorder_tag(3));  /* uns iter2 */
}

/* ================================================================
 * fd2_pin_required_char_to_party_slot1 @ 0x321C8
 *
 * Pins the char whose char_id == char_id into template-roster slot 1 and
 * packs the remaining non-lord chars into slots 2.. ; slot 0 (the lord) is
 * left untouched. match_idx is found by scanning the ACTIVE runtime array
 * data_fd2_battle_runtime_char_array_ptr[1 .. member_count) for the entry
 * whose char_id (+0x08) matches (last match wins, no early-out); the matched
 * index then drives a reorder of the SEPARATE template roster
 * data_fd2_shared_menu_party_roster_buffer_ptr (snapshot -> slot 1 gets
 * snapshot[match_idx]; slots 2.. get snapshot[1..N) skipping match_idx, in
 * order). It finishes by reloading the portrait cache from the REAL staged
 * FDICON.B24: free(data_fd2_portrait_sprite_cache); fopen; count=0; for each roster
 * slot [0,member_count) load roster[slot].portrait_id (+0x07); fclose.
 *
 * The reorder logic is the load-bearing core and is asserted exactly via a
 * per-slot tag fixture. The portrait reload is driven against the real
 * FDICON.B24 (staged into the test cwd by build_test.py) with valid portrait
 * ids in every slot, and asserted via the resulting cache count (one fresh
 * append per distinct id => count == member_count). No fabricated FDICON.
 *
 * Slot-i invariant: the runtime array slot i and the template roster slot i
 * describe the same char, so the search index found in the runtime array
 * maps directly to the reorder index in the template roster. The tests honor
 * that by seeding both arrays at the same indices.
 * ================================================================ */

#define PIN_STRIDE   0x50
#define PIN_SLOTS    32          /* 0xA00 / 0x50 = snapshot capacity */
static uint8 g_pin_roster[PIN_STRIDE * PIN_SLOTS];

/* Distinct, non-zero tag for slot i. 0x40 + i keeps every slot's tag unique,
 * clear of 0, AND a valid FDICON.B24 portrait id (entries 0x40.. carry real
 * sprite data), so the same byte serves both the placement assertion (read at
 * entry +0x00) and the real portrait reload (read at entry +0x07). */
static uint8 g_pin_tag(int slot)
{
    return (uint8)(0x40 + slot);
}

/* Fill template roster slot i entirely with g_pin_tag(i); point the template
 * globals at it and set member_count. */
static void pin_roster_reset(int count)
{
    int slot;
    int b;
    for (slot = 0; slot < PIN_SLOTS; slot++) {
        for (b = 0; b < PIN_STRIDE; b++) {
            g_pin_roster[slot * PIN_STRIDE + b] = g_pin_tag(slot);
        }
    }
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_pin_roster;
    data_fd2_shared_menu_party_member_count = (uint32)count;
}

/* Tag byte at the start of template-roster slot i (proves which source entry
 * landed there: the whole 0x50 entry was moved by memmove). */
static uint8 pin_slot_tag(int slot)
{
    return g_pin_roster[slot * PIN_STRIDE];
}

/* Seed the ACTIVE runtime array used for the match-find scan. Slot 0 is the
 * lord (never matched against in [1..N)); a 0xFF sentinel in the rest means
 * only slots a test populates can match. */
static void pin_rc_reset(void)
{
    int i;
    for (i = 0; i < 8; i++) {
        g_test_rc_array[i].char_id = 0xFF;
    }
}

/* free()/reset the portrait cache so the function's reload starts clean and
 * its leading free(data_fd2_portrait_sprite_cache) is a safe free(NULL). */
static void pin_cache_reset(void)
{
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
    memset(data_fd2_resource_portrait_cache_id_list_base, 0,
           sizeof(data_fd2_resource_portrait_cache_id_list_base));
}

static void pin_cache_teardown(void)
{
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
}

/* ----------------------------------------------------------------
 * Match mid-array. member_count == 6, runtime slots 1..5 scanned; the required
 * char_id 0x0A sits in runtime slot 2, so match_idx == 2. Expected template
 * roster after the pin:
 *   slot 0: untouched lord            (tag 0x40)
 *   slot 1: snapshot[match_idx == 2]  (tag 0x42)
 *   slots 2..5: snapshot 1,3,4,5 (every other non-lord, in order, skipping 2)
 * Proves the match-find index, the pin into slot 1, the skip-match pack, and
 * slot-0 preservation. The portrait reload runs over all 6 slots (6 distinct
 * valid ids) -> cache count == 6.
 * ---------------------------------------------------------------- */
static void test_pin_match_mid(void)
{
    pin_roster_reset(6);
    pin_rc_reset();
    pin_cache_reset();
    g_test_rc_array[2].char_id = 0x0A;   /* runtime slot 2 holds required id */

    fd2_pin_required_char_to_party_slot1(0x0A);

    ASSERT_EQ((int)pin_slot_tag(0), (int)g_pin_tag(0));  /* lord kept      */
    ASSERT_EQ((int)pin_slot_tag(1), (int)g_pin_tag(2));  /* matched -> s1  */
    ASSERT_EQ((int)pin_slot_tag(2), (int)g_pin_tag(1));  /* pack: snap 1   */
    ASSERT_EQ((int)pin_slot_tag(3), (int)g_pin_tag(3));  /* pack: snap 3   */
    ASSERT_EQ((int)pin_slot_tag(4), (int)g_pin_tag(4));  /* pack: snap 4   */
    ASSERT_EQ((int)pin_slot_tag(5), (int)g_pin_tag(5));  /* pack: snap 5   */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 6);
    pin_cache_teardown();
}

/* ----------------------------------------------------------------
 * Match in the LAST scanned slot. member_count == 6 -> runtime slots 1..5;
 * the id sits in slot 5, so the scan must reach the end to set match_idx == 5
 * (proves the inclusive upper bound and that the loop does not stop early).
 * Expected: slot 1 <- snapshot 5; slots 2..5 <- snapshot 1,2,3,4 (skip 5).
 * ---------------------------------------------------------------- */
static void test_pin_match_last(void)
{
    pin_roster_reset(6);
    pin_rc_reset();
    pin_cache_reset();
    g_test_rc_array[5].char_id = 0x15;   /* last scanned runtime slot */

    fd2_pin_required_char_to_party_slot1(0x15);

    ASSERT_EQ((int)pin_slot_tag(0), (int)g_pin_tag(0));  /* lord kept     */
    ASSERT_EQ((int)pin_slot_tag(1), (int)g_pin_tag(5));  /* matched -> s1 */
    ASSERT_EQ((int)pin_slot_tag(2), (int)g_pin_tag(1));  /* snap 1        */
    ASSERT_EQ((int)pin_slot_tag(3), (int)g_pin_tag(2));  /* snap 2        */
    ASSERT_EQ((int)pin_slot_tag(4), (int)g_pin_tag(3));  /* snap 3        */
    ASSERT_EQ((int)pin_slot_tag(5), (int)g_pin_tag(4));  /* snap 4        */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 6);
    pin_cache_teardown();
}

/* ----------------------------------------------------------------
 * LAST match wins. Two runtime slots (2 and 4) carry the required id; the scan
 * has no early-out and keeps overwriting match_idx, so match_idx ends at 4.
 * Expected: slot 1 <- snapshot 4; slots 2..5 <- snapshot 1,2,3,5 (skip 4).
 * Pins the "loop continues after a match -> latest index wins" semantics.
 * ---------------------------------------------------------------- */
static void test_pin_last_match_wins(void)
{
    pin_roster_reset(6);
    pin_rc_reset();
    pin_cache_reset();
    g_test_rc_array[2].char_id = 0x07;   /* first match */
    g_test_rc_array[4].char_id = 0x07;   /* later match: this one wins */

    fd2_pin_required_char_to_party_slot1(0x07);

    ASSERT_EQ((int)pin_slot_tag(0), (int)g_pin_tag(0));  /* lord kept      */
    ASSERT_EQ((int)pin_slot_tag(1), (int)g_pin_tag(4));  /* last match -> s1 */
    ASSERT_EQ((int)pin_slot_tag(2), (int)g_pin_tag(1));  /* snap 1         */
    ASSERT_EQ((int)pin_slot_tag(3), (int)g_pin_tag(2));  /* snap 2         */
    ASSERT_EQ((int)pin_slot_tag(4), (int)g_pin_tag(3));  /* snap 3         */
    ASSERT_EQ((int)pin_slot_tag(5), (int)g_pin_tag(5));  /* snap 5 (skip 4) */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 6);
    pin_cache_teardown();
}

/* ----------------------------------------------------------------
 * No match. The required id is absent from the scanned runtime slots, so
 * match_idx stays 0 (its init). The pin then copies snapshot[0] (the lord's
 * template entry) into slot 1, and the pack loop (skipping iter == 0, but iter
 * starts at 1 so nothing is skipped) lays snapshot 1..N-1 into slots 2..N-1.
 * member_count == 5. Expected:
 *   slot 0: untouched (tag 0x40)
 *   slot 1: snapshot 0  (tag 0x40)  -- lord template duplicated into slot 1
 *   slots 2..4: snapshot 1,2,3
 * Exercises the match_idx == 0 default path (the != match_idx test never
 * skips), a corner the matched-path cases do not reach.
 *
 * Because slot 0 and slot 1 then carry the SAME portrait id (0x40, the
 * lord's), the portrait reload's second load(0x40) is an idempotent cache hit
 * and does not grow the cache, so the distinct-id count is member_count - 1 ==
 * 4. This dedup is itself a faithful consequence of the lord-duplication on
 * the no-match path, so the test pins it.
 * ---------------------------------------------------------------- */
static void test_pin_no_match(void)
{
    pin_roster_reset(5);
    pin_rc_reset();
    pin_cache_reset();
    /* every runtime char_id is the 0xFF sentinel; 0x33 matches none */

    fd2_pin_required_char_to_party_slot1(0x33);

    ASSERT_EQ((int)pin_slot_tag(0), (int)g_pin_tag(0));  /* lord kept       */
    ASSERT_EQ((int)pin_slot_tag(1), (int)g_pin_tag(0));  /* snapshot[0]->s1 */
    ASSERT_EQ((int)pin_slot_tag(2), (int)g_pin_tag(1));  /* snap 1          */
    ASSERT_EQ((int)pin_slot_tag(3), (int)g_pin_tag(2));  /* snap 2          */
    ASSERT_EQ((int)pin_slot_tag(4), (int)g_pin_tag(3));  /* snap 3          */
    /* slot0 and slot1 share portrait id 0x40 -> reload dedups to 4 distinct */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 4);
    pin_cache_teardown();
}

/* ----------------------------------------------------------------
 * Low-byte-only match. The required id is loaded with MOVZX from a single
 * byte, so a wide argument whose low byte equals an in-range slot's char_id
 * must still match. member_count == 4 (runtime slots 1..3); slot 3 holds 0x0C
 * and the argument is 0x2200 | 0x0C, so match_idx == 3. Expected: slot 1 <-
 * snapshot 3; slots 2..3 <- snapshot 1,2. Pins the (uint8)char_id narrowing.
 * ---------------------------------------------------------------- */
static void test_pin_low_byte_only(void)
{
    pin_roster_reset(4);
    pin_rc_reset();
    pin_cache_reset();
    g_test_rc_array[3].char_id = 0x0C;   /* low byte 0x0C */

    fd2_pin_required_char_to_party_slot1(0x2200 | 0x0C);

    ASSERT_EQ((int)pin_slot_tag(0), (int)g_pin_tag(0));  /* lord kept      */
    ASSERT_EQ((int)pin_slot_tag(1), (int)g_pin_tag(3));  /* matched -> s1  */
    ASSERT_EQ((int)pin_slot_tag(2), (int)g_pin_tag(1));  /* snap 1         */
    ASSERT_EQ((int)pin_slot_tag(3), (int)g_pin_tag(2));  /* snap 2         */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 4);
    pin_cache_teardown();
}

/* ----------------------------------------------------------------
 * fd2_delay_400ms_via_idle_thunk @ 0x353CC: a one-line wrapper that calls
 * fd2_delay_ms(400) exactly once. Drive it through the testglob
 * recorder and assert the single call and the load-bearing 400-tick
 * (0x190) argument.
 * ---------------------------------------------------------------- */
static void test_delay_400ms_calls_thunk_once_with_400(void)
{
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;

    fd2_delay_400ms_via_idle_thunk();

    ASSERT_EQ((long)g_delay375b2_calls, 1);
    ASSERT_EQ((long)g_delay375b2_last_ticks, (long)400);
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
    RUN_TEST(test_check_party_has_char_found_first);
    RUN_TEST(test_check_party_has_char_found_mid);
    RUN_TEST(test_check_party_has_char_not_found);
    RUN_TEST(test_check_party_has_char_empty_roster);
    RUN_TEST(test_check_party_has_char_wide_arg_no_match);
    RUN_TEST(test_require_char_found_first_slot);
    RUN_TEST(test_require_char_found_last_slot);
    RUN_TEST(test_require_char_found_inrange_with_out_of_window_copies);
    RUN_TEST(test_require_char_found_low_byte_only);
    RUN_TEST(test_count_selected_mixed);
    RUN_TEST(test_count_selected_last_slot_excluded);
    RUN_TEST(test_count_selected_all_in_range);
    RUN_TEST(test_count_selected_high_bit_bytes);
    RUN_TEST(test_count_selected_empty_when_count_one);
    RUN_TEST(test_reorder_mixed_partition);
    RUN_TEST(test_reorder_all_selected);
    RUN_TEST(test_reorder_none_selected);
    RUN_TEST(test_reorder_bound_and_offset);
    RUN_TEST(test_reorder_high_bit_selected);
    RUN_TEST(test_pin_match_mid);
    RUN_TEST(test_pin_match_last);
    RUN_TEST(test_pin_last_match_wins);
    RUN_TEST(test_pin_no_match);
    RUN_TEST(test_pin_low_byte_only);
    RUN_TEST(test_delay_400ms_calls_thunk_once_with_400);
    printf("\n");
}
