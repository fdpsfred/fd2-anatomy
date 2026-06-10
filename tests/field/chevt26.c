/*
 * unit tests for src/field/chevt2.c (part 6)
 *
 * fd2_chapter_event_handler_4d__unref_sentinel @ 0x35EBE
 *
 * 10-byte self-contained sentinel stub (no borrowed tail):
 *     PUSH 4; CALL __CHK;
 *     MOV EAX, [tile_event_consumed_flags_ptr];
 *     MOV byte [EAX + 0x13], 1;
 *     RET
 * Functionally-exact body is the single unconditional byte store:
 *     *(uint8 *)(tile_event_consumed_flags_ptr + 0x13) = 1;
 *
 * Risk-bearing (state mutation / consumed-flag setter); driven over a real
 * in-memory flags buffer:
 *   (a) the store writes the immediate 1 into index 0x13,
 *   (b) it is UNCONDITIONAL — pre-seeding 0x13 to a non-1 sentinel still ends at 1
 *       (the binary has no CMP/JNZ gate, just MOV byte [EAX+0x13],1), the defining
 *       contrast against the gated sentinels (handler_3e/41 only write when their
 *       slot reads 0),
 *   (c) ONLY index 0x13 changes: the immediate neighbours 0x12 and 0x14 stay
 *       untouched. Index 0x12 is the slot handler_49 sets, so pinning 0x12 as an
 *       untouched neighbour proves this handler targets the ADJACENT 0x13 — the
 *       exact slot handler_47 reads on its mass-kill gate (this sentinel primes it),
 *   (d) the dispatch arg is ignored (passed nonzero).
 *
 * Own in-memory flags fixture (0x20 bytes) so the suite never aliases the other
 * chevt2 part suites' state.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "globals.h"
#include "protos.h"

/* ================================================================
 * fd2_chapter_event_handler_4d__unref_sentinel @ 0x35EBE
 *
 * Pure single unconditional byte store: tile_event_consumed_flags[0x13] = 1.
 * Own flags fixture (0x20 bytes) so the suite never aliases other suites' state.
 * ================================================================ */
static uint8 g_ce4d_flags[0x20];

static void ce4d_setup(void)
{
    memset(g_ce4d_flags, 0, sizeof(g_ce4d_flags));
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce4d_flags;
}

static void ce4d_teardown(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
}

/* ----------------------------------------------------------------
 * The handler sets tile_event_consumed_flags[0x13] = 1 and nothing else. Start
 * with a zeroed flags buffer; after the call index 0x13 is exactly 1 while its
 * immediate neighbours 0x12 and 0x14 stay 0. Pinning 0x12 untouched is the exact
 * contrast against handler_49 (which sets the adjacent 0x12); pinning 0x13 == 1 is
 * what primes handler_47's 2nd-call mass-kill gate. The dispatch arg is passed
 * nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h4d_sets_consumed_flag_0x13(void)
{
    ce4d_setup();

    fd2_chapter_event_handler_4d__unref_sentinel(0x77);

    /* (a) the store wrote the immediate 1 into index 0x13 */
    ASSERT_EQ((long)g_ce4d_flags[0x13], 1);
    /* (c) only index 0x13 changed: immediate neighbours untouched. 0x12 is
     *     handler_49's slot, so its staying 0 proves the adjacent-index split. */
    ASSERT_EQ((long)g_ce4d_flags[0x12], 0);
    ASSERT_EQ((long)g_ce4d_flags[0x14], 0);

    ce4d_teardown();
}

/* ----------------------------------------------------------------
 * The store is UNCONDITIONAL (the binary has no CMP/JNZ gate, just
 * MOV byte [EAX+0x13],1) — the defining contrast against the gated sentinels
 * handler_3e/41 which only write when their slot reads 0. Pre-seed index 0x13
 * with a non-1 sentinel; after the call it must equal 1 (the store always fires
 * and always writes the immediate 1, never preserving the prior value). The
 * neighbours, also pre-seeded non-zero, must be left exactly as they were. The
 * dispatch arg is ignored.
 * ---------------------------------------------------------------- */
static void test_h4d_store_is_unconditional_and_index_exact(void)
{
    ce4d_setup();
    g_ce4d_flags[0x13] = 0x5C;       /* stale sentinel, NOT 1 */
    g_ce4d_flags[0x12] = 0xAB;       /* neighbour decoys: must be left untouched */
    g_ce4d_flags[0x14] = 0xCD;

    fd2_chapter_event_handler_4d__unref_sentinel(0);

    /* (b) the store always fires and always writes the immediate 1 */
    ASSERT_EQ((long)g_ce4d_flags[0x13], 1);
    /* (c) neighbours preserved verbatim -> only 0x13 is touched */
    ASSERT_EQ((long)g_ce4d_flags[0x12], 0xAB);
    ASSERT_EQ((long)g_ce4d_flags[0x14], 0xCD);

    ce4d_teardown();
}

void run_field_chevt26_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 6)\n");
    RUN_TEST(test_h4d_sets_consumed_flag_0x13);
    RUN_TEST(test_h4d_store_is_unconditional_and_index_exact);
    printf("\n");
}
