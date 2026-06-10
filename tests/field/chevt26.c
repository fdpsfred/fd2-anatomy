/*
 * unit tests for src/field/chevt2.c (part 6)
 *
 * fd2_chapter_event_handler_4d__unref_sentinel @ 0x35EBE
 * fd2_chapter_event_handler_4e__unref_sentinel @ 0x35ED2
 * fd2_chapter_event_handler_4f__ch29_dyn_turn_event @ 0x35EE6
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
 * handler_4e is the same shape one slot up — an unconditional, self-contained
 * single byte store of tile_event_consumed_flags[0x14] = 1 (also a 10-byte stub
 * with no borrowed tail). It gets the identical risk-bearing coverage, with the
 * adjacency pin flipped: index 0x13 (handler_4d's slot) and 0x15 (the slot
 * handler_4c writes) bracket it and must stay untouched.
 *
 * Own in-memory flags fixture (0x20 bytes) so the suite never aliases the other
 * chevt2 part suites' state.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* shared runtime_char array (defined in testglob.c) — restored as the default
 * runtime_char_array_ptr in the h4f teardown after the suite points it at its own
 * fixture. */
extern runtime_char g_test_rc_array[8];

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

/* ================================================================
 * fd2_chapter_event_handler_4e__unref_sentinel @ 0x35ED2
 *
 * Pure single unconditional byte store: tile_event_consumed_flags[0x14] = 1.
 * Own flags fixture (0x20 bytes) so the suite never aliases other suites' state.
 * ================================================================ */
static uint8 g_ce4e_flags[0x20];

static void ce4e_setup(void)
{
    memset(g_ce4e_flags, 0, sizeof(g_ce4e_flags));
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce4e_flags;
}

static void ce4e_teardown(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
}

/* ----------------------------------------------------------------
 * The handler sets tile_event_consumed_flags[0x14] = 1 and nothing else. Start
 * with a zeroed flags buffer; after the call index 0x14 is exactly 1 while its
 * immediate neighbours 0x13 and 0x15 stay 0. Pinning 0x13 untouched is the exact
 * contrast against handler_4d (which sets the adjacent 0x13). The dispatch arg is
 * passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h4e_sets_consumed_flag_0x14(void)
{
    ce4e_setup();

    fd2_chapter_event_handler_4e__unref_sentinel(0x77);

    /* (a) the store wrote the immediate 1 into index 0x14 */
    ASSERT_EQ((long)g_ce4e_flags[0x14], 1);
    /* (c) only index 0x14 changed: immediate neighbours untouched. 0x13 is
     *     handler_4d's slot, so its staying 0 proves the adjacent-index split. */
    ASSERT_EQ((long)g_ce4e_flags[0x13], 0);
    ASSERT_EQ((long)g_ce4e_flags[0x15], 0);

    ce4e_teardown();
}

/* ----------------------------------------------------------------
 * The store is UNCONDITIONAL (the binary has no CMP/JNZ gate, just
 * MOV byte [EAX+0x14],1) — the defining contrast against the gated sentinels
 * handler_3e/41 which only write when their slot reads 0. Pre-seed index 0x14
 * with a non-1 sentinel; after the call it must equal 1 (the store always fires
 * and always writes the immediate 1, never preserving the prior value). The
 * neighbours, also pre-seeded non-zero, must be left exactly as they were. The
 * dispatch arg is ignored.
 * ---------------------------------------------------------------- */
static void test_h4e_store_is_unconditional_and_index_exact(void)
{
    ce4e_setup();
    g_ce4e_flags[0x14] = 0x5C;       /* stale sentinel, NOT 1 */
    g_ce4e_flags[0x13] = 0xAB;       /* neighbour decoys: must be left untouched */
    g_ce4e_flags[0x15] = 0xCD;

    fd2_chapter_event_handler_4e__unref_sentinel(0);

    /* (b) the store always fires and always writes the immediate 1 */
    ASSERT_EQ((long)g_ce4e_flags[0x14], 1);
    /* (c) neighbours preserved verbatim -> only 0x14 is touched */
    ASSERT_EQ((long)g_ce4e_flags[0x13], 0xAB);
    ASSERT_EQ((long)g_ce4e_flags[0x15], 0xCD);

    ce4e_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_4f__ch29_dyn_turn_event @ 0x35EE6
 *
 * ch29 turn-FF RNG-driven skip scheduler (dispatch idx 0x4F @ table 0x51B91).
 * Body (1-arg cdecl; arg ignored):
 *   tile_event_data_table[+9] = (uint8)(turn_counter + 1)     -- arm hook 2 next turn
 *   rng  = fd2_advance_rng_state()                            -- pull next RNG value
 *   base = tile_event_consumed_flags[0x15]                    -- = party_member_count - 3
 *   fd2_mark_char_acted_this_turn((rng % 3) + base)           -- skip 1st of the pair
 *   fd2_mark_char_acted_this_turn(((rng + 1) % 3) + base)     -- skip 2nd of the pair
 *
 * Risk-bearing on every axis: it is RNG-driven (the % 3 pair selection), it carries
 * the EAX-tracking-bug fix (the value fed into % 3 is the RETURN of
 * fd2_advance_rng_state captured in EBX, NOT the __CHK stack-probe thunk the
 * decompiler mislabels as iVar1), it does 8-bit turn+1 arithmetic for the +9
 * scheduler store, and it offsets both marks by base = consumed_flags[0x15].
 *
 * Driven over the REAL fd2_advance_rng_state (a pure leaf: state =
 * data_fd2_shared_rng_seed; state = rol16(state + 0x9014, 3); return state) and the
 * REAL fd2_mark_char_acted_this_turn (sets runtime_char[idx].flags bit 0x80) over
 * in-memory fixtures. The RNG is deterministic, so seeding data_fd2_shared_rng_seed
 * fixes the returned value exactly; the three seeds 0/1/2 (verified against Ghidra
 * emulate_function: returns 0x80A4 / 0x80AC / 0x80B4) yield rng%3 = 1/0/2, i.e.
 * three distinct (kept, skipped, skipped) outcomes that walk every residue. Because
 * rng%3 and (rng+1)%3 are consecutive residues mod 3 they are always distinct, so
 * exactly two of the three candidate slots {base, base+1, base+2} are marked and one
 * is left untouched — the slot left untouched is the witness for which RNG value
 * actually drove the selection.
 *
 * Own in-memory fixtures (own flags + data-table buffers, own runtime_char array) so
 * the suite never aliases the h4d/h4e state.
 * ================================================================ */
static uint8        g_ce4f_flags[0x20];   /* [0x15] = base for the two marks       */
static uint8        g_ce4f_dtable[0x10];  /* +9 = scheduler target                 */
static runtime_char g_ce4f_rc[8];         /* the marked candidate slots live here  */

/* rol16(state + 0x9014, 3) — exact replica of fd2_advance_rng_state's transform,
 * cross-checked against Ghidra emulate_function for seeds 0/1/2. The handler reads
 * the post-advance value, so this returns the value the handler will see for a
 * given pre-seed. */
static uint16 ce4f_expected_rng(uint16 seed)
{
    uint32 v;

    v = (uint32)(uint16)(seed + 0x9014);
    return (uint16)(((v << 3) | (v >> 13)) & 0xFFFF);
}

/* base = the value the handler reads from consumed_flags[0x15]; turn seeds the +9
 * scheduler store; seed primes the RNG so the marked pair is deterministic. */
static void ce4f_setup(uint16 seed, uint8 base, uint8 turn)
{
    memset(g_ce4f_flags, 0, sizeof(g_ce4f_flags));
    memset(g_ce4f_dtable, 0, sizeof(g_ce4f_dtable));
    memset(g_ce4f_rc, 0, sizeof(g_ce4f_rc));
    g_ce4f_flags[0x15] = base;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce4f_flags;
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce4f_dtable;
    data_fd2_battle_runtime_char_array_ptr = g_ce4f_rc;
    data_fd2_battle_turn_counter = turn;
    data_fd2_shared_rng_seed = seed;
}

static void ce4f_teardown(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_turn_counter = 0;
    data_fd2_shared_rng_seed = 0;
}

/* ----------------------------------------------------------------
 * Full sequence with seed=1, base=0: the real RNG returns 0x80AC, so rng%3 = 0 and
 * (rng+1)%3 = 1 -> slots 0 and 1 are marked acted while slot 2 is left untouched.
 * The +9 scheduler byte holds turn_counter + 1 (0x21 from turn 0x20). The RNG state
 * advanced exactly once (seed 1 -> 0x80AC), proving fd2_advance_rng_state ran a
 * single time and its RETURN (not the __CHK thunk) fed the % 3. The dispatch arg is
 * passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h4f_rng_picks_pair_and_schedules(void)
{
    ce4f_setup(1, 0, 0x20);

    fd2_chapter_event_handler_4f__ch29_dyn_turn_event(0x77);

    /* seed 1 -> rng 0x80AC -> rng%3=0, (rng+1)%3=1: slots 0 and 1 marked */
    ASSERT_EQ((long)(g_ce4f_rc[0].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    ASSERT_EQ((long)(g_ce4f_rc[1].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    /* slot 2 (the third candidate) was NOT marked — the pair is exactly two slots */
    ASSERT_EQ((long)(g_ce4f_rc[2].flags & CHARFLAG_ACTED), 0);
    /* +9 scheduler byte = turn_counter + 1 = 0x21 */
    ASSERT_EQ((long)g_ce4f_dtable[9], 0x21);
    /* the real RNG advanced exactly once: seed 1 -> 0x80AC (its return fed the %3) */
    ASSERT_EQ((long)data_fd2_shared_rng_seed, (long)ce4f_expected_rng(1));
    ASSERT_EQ((long)data_fd2_shared_rng_seed, 0x80AC);

    ce4f_teardown();
}

/* ----------------------------------------------------------------
 * The marked pair is driven by the RNG RETURN VALUE, not a constant: re-running with
 * different seeds picks a different pair. This is the direct witness for the
 * EAX-tracking-bug fix — if the code fed the __CHK thunk's result (or any fixed
 * value) into % 3, the kept/skipped slots could not rotate with the seed.
 *   seed 0 -> rng 0x80A4 -> rng%3=1, (rng+1)%3=2 -> marks {1,2}, leaves slot 0
 *   seed 2 -> rng 0x80B4 -> rng%3=2, (rng+1)%3=0 -> marks {2,0}, leaves slot 1
 * Together with the seed-1 case above (leaves slot 2), the three seeds walk all
 * three "left-untouched" outcomes, so the unmarked slot uniquely identifies rng%3.
 * ---------------------------------------------------------------- */
static void test_h4f_rng_pair_rotates_with_seed(void)
{
    /* seed 0 -> leaves slot 0, marks {1,2} */
    ce4f_setup(0, 0, 0x10);
    fd2_chapter_event_handler_4f__ch29_dyn_turn_event(0);
    ASSERT_EQ((long)(g_ce4f_rc[0].flags & CHARFLAG_ACTED), 0);
    ASSERT_EQ((long)(g_ce4f_rc[1].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    ASSERT_EQ((long)(g_ce4f_rc[2].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    ASSERT_EQ((long)data_fd2_shared_rng_seed, 0x80A4);
    ce4f_teardown();

    /* seed 2 -> leaves slot 1, marks {0,2} */
    ce4f_setup(2, 0, 0x10);
    fd2_chapter_event_handler_4f__ch29_dyn_turn_event(0);
    ASSERT_EQ((long)(g_ce4f_rc[1].flags & CHARFLAG_ACTED), 0);
    ASSERT_EQ((long)(g_ce4f_rc[0].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    ASSERT_EQ((long)(g_ce4f_rc[2].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    ASSERT_EQ((long)data_fd2_shared_rng_seed, 0x80B4);
    ce4f_teardown();
}

/* ----------------------------------------------------------------
 * Both marks are offset by base = consumed_flags[0x15]: the candidate window is
 * {base, base+1, base+2}, not {0,1,2}. Seed=1 (rng%3=0, (rng+1)%3=1) with base=2 ->
 * the marked slots are 2+0=2 and 2+1=3, while 2+2=4 (the third candidate) is left
 * untouched. Pin the window's lower edge too: slots 0 and 1 (below base) stay
 * untouched. This proves the +0x15 read is the true offset source and that the two
 * mark indices are (residue + base), not bare residues.
 * ---------------------------------------------------------------- */
static void test_h4f_base_offset_from_flags_0x15(void)
{
    ce4f_setup(1, 2, 0x10);            /* base = 2; seed 1 -> residues 0 and 1 */

    fd2_chapter_event_handler_4f__ch29_dyn_turn_event(0x33);

    /* marked: base+0 = 2 and base+1 = 3 */
    ASSERT_EQ((long)(g_ce4f_rc[2].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    ASSERT_EQ((long)(g_ce4f_rc[3].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    /* the third candidate base+2 = 4 is left untouched */
    ASSERT_EQ((long)(g_ce4f_rc[4].flags & CHARFLAG_ACTED), 0);
    /* slots below the base window were never touched */
    ASSERT_EQ((long)(g_ce4f_rc[0].flags & CHARFLAG_ACTED), 0);
    ASSERT_EQ((long)(g_ce4f_rc[1].flags & CHARFLAG_ACTED), 0);

    ce4f_teardown();
}

/* ----------------------------------------------------------------
 * The +9 scheduler store reads turn_counter as one byte and writes
 * (uint8)(turn_counter + 1) (binary MOV BL,[turn] / INC BL). Seed turn=0xFF: the +9
 * byte wraps to 0x00 (8-bit INC), pinning the truncation. The store lands at the
 * exact +9 offset: the immediate neighbours +8 and +10, pre-seeded with sentinels,
 * are left untouched. (The two marks still fire as a byproduct — seed 1 marks slots
 * 0 and 1 — but here the focus is the scheduler byte.)
 * ---------------------------------------------------------------- */
static void test_h4f_schedule_turn_plus1_is_8bit_at_offset_9(void)
{
    ce4f_setup(1, 0, 0xFF);
    g_ce4f_dtable[8]  = 0xAA;          /* +8 neighbour decoy (just below +9) */
    g_ce4f_dtable[10] = 0xBB;          /* +10 neighbour decoy (just above +9) */

    fd2_chapter_event_handler_4f__ch29_dyn_turn_event(0);

    /* +9 = (uint8)(0xFF + 1) = 0x00 (8-bit wrap) */
    ASSERT_EQ((long)g_ce4f_dtable[9], 0x00);
    /* only +9 changed: immediate neighbours preserved verbatim */
    ASSERT_EQ((long)g_ce4f_dtable[8], 0xAA);
    ASSERT_EQ((long)g_ce4f_dtable[10], 0xBB);

    ce4f_teardown();
}

void run_field_chevt26_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 6)\n");
    RUN_TEST(test_h4d_sets_consumed_flag_0x13);
    RUN_TEST(test_h4d_store_is_unconditional_and_index_exact);
    RUN_TEST(test_h4e_sets_consumed_flag_0x14);
    RUN_TEST(test_h4e_store_is_unconditional_and_index_exact);
    RUN_TEST(test_h4f_rng_picks_pair_and_schedules);
    RUN_TEST(test_h4f_rng_pair_rotates_with_seed);
    RUN_TEST(test_h4f_base_offset_from_flags_0x15);
    RUN_TEST(test_h4f_schedule_turn_plus1_is_8bit_at_offset_9);
    printf("\n");
}
