/*
 * unit tests for src/field/chevt2.c (part 4)
 *
 * fd2_chapter_event_handler_43__unref_dyn_turn_event @ 0x35A2F
 *
 * Pure state-machine mutator (no display side effects, no real-file I/O). The
 * functionally-exact body is a single unconditional byte store:
 *     tile_event_data_table[6] = (uint8)turn_counter;
 *
 * The risk-bearing contract pinned here:
 *   (a) NO GATE: unlike handler_41 (which only arms when flags[0x10] == 0 and
 *       then consumes the slot), this handler has no consume-flag check at all —
 *       it writes data_table[+6] every single call. Calling it twice in a row
 *       with two different turn_counter values must leave the SECOND value (the
 *       slot is overwritten, never locked out). This is the defining behavioural
 *       difference and pins the absence of any CMP/JNZ gate in the binary.
 *   (b) OFFSET +6: the armed byte is hook entry 1's turn byte (data_table[+6]),
 *       not handler_41's entry-0 byte (+3); the +3 slot must stay untouched.
 *   (c) NO VALUE OFFSET: the scheduled value is turn_counter EXACTLY — no +1
 *       (contrast handler_3e, which stores turn_counter + 1); a value whose +1
 *       would differ pins it.
 *   (d) BYTE-width store: the turn counter is read as one byte and stored as one
 *       byte (MOV DL,[turn_counter] / MOV [data_table+6],DL), so only the low
 *       byte reaches data_table[+6] and the high bytes never leak.
 *   (e) exact byte offset: only data_table[+6] is written; its neighbours stay
 *       untouched.
 *   (f) the dispatch arg is ignored (the handler reads no param).
 *
 * Its own in-memory data table fixture so the suite never aliases the other
 * chevt2 part suites' state.
 */

#include <string.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ================================================================
 * fd2_chapter_event_handler_43__unref_dyn_turn_event @ 0x35A2F
 * ================================================================ */

/* data table: index 6 is hook entry 1's turn byte; headroom guards neighbours */
static uint8 g_ce43_dtable[0x10];

static void ce43_setup(void)
{
    memset(g_ce43_dtable, 0, sizeof(g_ce43_dtable));
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce43_dtable;
    data_fd2_battle_turn_counter = 0;
}

static void ce43_teardown(void)
{
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_turn_counter = 0;
}

/* ----------------------------------------------------------------
 * Unconditional arm + NO VALUE OFFSET: turn_counter == 5. The handler writes
 * hook entry 1's turn byte (data_table[+6]) with 5 EXACTLY (not 6 — the contrast
 * against handler_3e's +1). The dispatch arg is passed nonzero to prove it is
 * ignored. Guard bytes around the write target (and handler_41's +3 slot) must
 * stay 0.
 * ---------------------------------------------------------------- */
static void test_h43_writes_turn_counter_at_offset_6(void)
{
    ce43_setup();
    data_fd2_battle_turn_counter = 5;

    fd2_chapter_event_handler_43__unref_dyn_turn_event(0x77);

    /* (b)+(c) hook entry 1's turn byte = turn_counter EXACTLY (5, not 5+1) */
    ASSERT_EQ((long)g_ce43_dtable[6], 5);
    /* (b) handler_41's +3 slot must NOT be the one written */
    ASSERT_EQ((long)g_ce43_dtable[3], 0);
    /* (e) immediate neighbours of data_table[+6] untouched */
    ASSERT_EQ((long)g_ce43_dtable[5], 0);
    ASSERT_EQ((long)g_ce43_dtable[7], 0);

    ce43_teardown();
}

/* ----------------------------------------------------------------
 * NO GATE / repeatability: there is no consume flag, so a second call overwrites
 * the slot. Pre-arm data_table[+6] with a sentinel, then call with a different
 * turn_counter; the slot must take the NEW value (not be preserved like the
 * consumed-out handler_41). Then call again with a third value to prove every
 * call keeps overwriting.
 * ---------------------------------------------------------------- */
static void test_h43_no_gate_overwrites_every_call(void)
{
    ce43_setup();
    g_ce43_dtable[6] = 0x5C;       /* stale previously-armed value */
    data_fd2_battle_turn_counter = 9;

    fd2_chapter_event_handler_43__unref_dyn_turn_event(0);
    /* (a) overwritten with the new turn_counter, NOT preserved */
    ASSERT_EQ((long)g_ce43_dtable[6], 9);

    /* second call with a fresh counter keeps overwriting (no lock-out) */
    data_fd2_battle_turn_counter = 0x21;
    fd2_chapter_event_handler_43__unref_dyn_turn_event(0);
    ASSERT_EQ((long)g_ce43_dtable[6], 0x21);

    ce43_teardown();
}

/* ----------------------------------------------------------------
 * Byte-width store, high bytes ignored: the binary reads turn_counter as a single
 * byte (MOV DL, byte ptr [turn_counter]) and stores it as a byte. turn_counter =
 * 0x1234: only the low byte 0x34 reaches data_table[+6]; the 0x12 high byte never
 * leaks into the neighbour. Pairing the low byte with the no-offset value pins
 * both the byte width and the absence of the +1.
 * ---------------------------------------------------------------- */
static void test_h43_turn_counter_low_byte_only(void)
{
    ce43_setup();
    data_fd2_battle_turn_counter = 0x1234;

    fd2_chapter_event_handler_43__unref_dyn_turn_event(0);

    /* low byte 0x34 stored verbatim (no +1); high byte 0x12 never reaches it */
    ASSERT_EQ((long)g_ce43_dtable[6], 0x34);
    /* neighbour past the byte must not catch a high byte */
    ASSERT_EQ((long)g_ce43_dtable[7], 0);

    ce43_teardown();
}

void run_field_chevt24_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 4)\n");
    RUN_TEST(test_h43_writes_turn_counter_at_offset_6);
    RUN_TEST(test_h43_no_gate_overwrites_every_call);
    RUN_TEST(test_h43_turn_counter_low_byte_only);
    printf("\n");
}
