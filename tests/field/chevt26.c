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
#include <stdlib.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx (h52 dialog VM) */

/* shared runtime_char array (defined in testglob.c) — restored as the default
 * runtime_char_array_ptr in the h4f teardown after the suite points it at its own
 * fixture. */
extern runtime_char g_test_rc_array[8];

/* testglob recorders used by the handler_52 cinematic suite */
extern int    g_composite_call_count;      /* real pan/composite tile-map blit proxy */
extern int    g_dlg_glyph_calls;           /* real dialog VM glyph recorder          */
extern int    g_warp_char_calls;           /* unemitted warp-helper recording stub   */
extern uint32 g_warp_char_id[4];
extern uint32 g_warp_tile_x[4];
extern uint32 g_warp_tile_y[4];

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

/* ================================================================
 * fd2_chapter_event_handler_50__ch30_ai_ctrl @ 0x35F5A
 *
 * ch30 AI setup (dispatch idx 0x50 @ table 0x51B91). Body (1-arg cdecl; arg
 * ignored): a single self-contained call
 *   fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x14, 0x14, 0xB)
 * which arms AI control flag 0xB (the low nibble of runtime_char.combat_aux_block
 * [0xD], absolute offset 0x34) for the single char index 0x14. The range
 * 0x14..0x14 is exactly one char and the high nibble of the target byte is
 * preserved.
 *
 * In the binary this handler pushes its 3 args then JMP 0x356AE, borrowing the
 * CALL + ADD ESP,0xC + RET tail of handler_3c. The borrowed tail is pure code
 * sharing; this test drives the REAL handler and its REAL callee against a local
 * runtime_char array big enough for index 0x14 (the shared g_test_rc_array[8] is
 * too small). No game files, no display.
 *
 * Risk-bearing (state mutation / AI-flag setter):
 *   (a) char 0x14's low nibble is set to 0xB,
 *   (b) the high nibble of that byte is PRESERVED (the callee does
 *       (byte & 0xF0) | new_val, not a blind overwrite) — the OR-with-mask
 *       semantics that distinguish this from a full-byte store,
 *   (c) the range is a SINGLE char (0x14..0x14): the immediate neighbours 0x13
 *       and 0x15 stay untouched (guards against an off-by-one wide range),
 *   (d) the dispatch arg is ignored (passed nonzero).
 * ================================================================ */

#define CE50_NCHARS      0x20       /* must cover index 0x14 with neighbours */
#define CE50_AI_OFF      0x34       /* combat_aux_block[0xD] absolute offset */

static runtime_char g_ce50_rc[CE50_NCHARS];

/* offset 0x34 of char `idx`, read as raw byte */
static uint8 ce50_ai(int idx)
{
    return ((uint8 *)&g_ce50_rc[idx])[CE50_AI_OFF];
}

/* Seed offset 0x34 of every char with a non-zero HIGH nibble (0xA0) and a
 * non-zero LOW nibble (0x05) so we can prove: (a) char 0x14's low nibble is set
 * to 0xB with the high nibble preserved -> 0xAB, and (b) every other char keeps
 * 0xA5 untouched. */
static void ce50_setup(void)
{
    int i;

    memset(g_ce50_rc, 0, sizeof(g_ce50_rc));
    for (i = 0; i < CE50_NCHARS; i++) {
        ((uint8 *)&g_ce50_rc[i])[CE50_AI_OFF] = 0xA5;
    }
    data_fd2_battle_runtime_char_array_ptr = g_ce50_rc;
}

static void ce50_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}

/* ----------------------------------------------------------------
 * Char 0x14 gets its low nibble set to 0xB with the high nibble preserved:
 * seeded 0xA5 -> (0xA0) | 0xB = 0xAB. Proves (a) the flag value 0xB lands in the
 * low nibble and (b) the high nibble is NOT clobbered (OR-with-mask, not a full
 * overwrite). The dispatch arg is passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h50_sets_ai_flag_b_for_char_0x14(void)
{
    ce50_setup();

    fd2_chapter_event_handler_50__ch30_ai_ctrl(0x77);

    /* (a) low nibble = 0xB, (b) high nibble 0xA preserved -> 0xAB */
    ASSERT_EQ((long)ce50_ai(0x14), 0xAB);

    ce50_teardown();
}

/* ----------------------------------------------------------------
 * The range is a SINGLE char (0x14..0x14): only index 0x14 changes; the
 * immediate neighbours 0x13 and 0x15 stay at their seeded 0xA5. Guards against an
 * off-by-one that would widen the range. Also pins a far slot (0) untouched.
 * ---------------------------------------------------------------- */
static void test_h50_range_is_single_char_exact(void)
{
    int i;

    ce50_setup();

    fd2_chapter_event_handler_50__ch30_ai_ctrl(0);

    ASSERT_EQ((long)ce50_ai(0x13), 0xA5);       /* just before the single-char range */
    ASSERT_EQ((long)ce50_ai(0x14), 0xAB);       /* the one char that is written      */
    ASSERT_EQ((long)ce50_ai(0x15), 0xA5);       /* just after the single-char range  */

    /* every other char is left untouched */
    for (i = 0; i < CE50_NCHARS; i++) {
        if (i != 0x14) {
            ASSERT_EQ((long)ce50_ai(i), 0xA5);
        }
    }

    ce50_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_51__unref_dyn_turn_event @ 0x35F6F
 *
 * ch self-looping turn-event pump (dispatch idx 0x51 @ table 0x51B91, entry
 * @ 0x51CD5). Body (1-arg cdecl; arg ignored):
 *   tile_event_consumed_flags[0x10] = (uint8)(consumed_flags[0x10] + 1)  -- advance stage
 *   tile_event_data_table[+3]       = (uint8)(turn_counter + 1)          -- arm hook 0 next turn
 *
 * Risk-bearing (state mutation + 8-bit arithmetic + two offset-exact stores):
 *   (a) the stage counter at consumed_flags[0x10] advances by exactly 1,
 *   (b) the +3 scheduler byte = (uint8)(turn_counter + 1),
 *   (c) the advance is UNGATED and CUMULATIVE — unlike the gated schedulers
 *       (handler_3e/41 only write when their slot reads 0), this one has no
 *       CMP/JNZ gate; a pre-seeded non-zero stage still increments and the
 *       scheduler byte is re-armed every call,
 *   (d) BOTH stores are 8-bit (INC byte ptr / MOV DL,[turn]; INC DL): stage
 *       0xFF wraps to 0x00 and turn 0xFF -> +3 byte wraps to 0x00,
 *   (e) the two stores hit exactly consumed_flags[0x10] and data_table[+3]:
 *       the immediate neighbours stay untouched,
 *   (f) the dispatch arg is ignored.
 *
 * Driven over own in-memory flags + data-table fixtures (no game files, no
 * display, no RNG) so the suite never aliases the h4d/h4e/h4f state.
 * ================================================================ */
static uint8 g_ce51_flags[0x20];    /* [0x10] = stage counter         */
static uint8 g_ce51_dtable[0x10];   /* +3 = hook 0 scheduler target   */

static void ce51_setup(uint8 stage, uint8 turn)
{
    memset(g_ce51_flags, 0, sizeof(g_ce51_flags));
    memset(g_ce51_dtable, 0, sizeof(g_ce51_dtable));
    g_ce51_flags[0x10] = stage;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce51_flags;
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce51_dtable;
    data_fd2_battle_turn_counter = turn;
}

static void ce51_teardown(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_turn_counter = 0;
}

/* ----------------------------------------------------------------
 * From a zeroed stage and turn_counter = 0x20: the stage counter advances 0 -> 1
 * and the +3 scheduler byte = turn_counter + 1 = 0x21. Both stores land at their
 * exact offsets; the dispatch arg is passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h51_advances_stage_and_schedules(void)
{
    ce51_setup(0, 0x20);

    fd2_chapter_event_handler_51__unref_dyn_turn_event(0x77);

    /* (a) stage counter advanced 0 -> 1 */
    ASSERT_EQ((long)g_ce51_flags[0x10], 1);
    /* (b) +3 scheduler byte = turn_counter + 1 = 0x21 */
    ASSERT_EQ((long)g_ce51_dtable[3], 0x21);

    ce51_teardown();
}

/* ----------------------------------------------------------------
 * The stage advance is UNGATED and CUMULATIVE — there is no CMP/JNZ gate (just
 * INC byte ptr), the defining contrast against the gated schedulers handler_3e/41
 * which only write when their slot reads 0. Pre-seed the stage with a non-zero
 * value: it still increments by exactly 1 (0x05 -> 0x06), and the scheduler byte
 * is re-armed on this call too. The stores hit exactly consumed_flags[0x10] and
 * data_table[+3]: the flag neighbours 0x0F/0x11 and the data-table neighbours
 * +2/+4, pre-seeded with sentinels, are left untouched.
 * ---------------------------------------------------------------- */
static void test_h51_advance_ungated_and_offsets_exact(void)
{
    ce51_setup(0x05, 0x40);
    g_ce51_flags[0x0F] = 0xAB;     /* flag neighbour decoys: must stay untouched */
    g_ce51_flags[0x11] = 0xCD;
    g_ce51_dtable[2]   = 0xAA;     /* data-table neighbour decoys (around +3)     */
    g_ce51_dtable[4]   = 0xBB;

    fd2_chapter_event_handler_51__unref_dyn_turn_event(0);

    /* (c) ungated: a non-zero stage still advances by exactly 1 */
    ASSERT_EQ((long)g_ce51_flags[0x10], 0x06);
    /* the scheduler byte is re-armed every call regardless of stage */
    ASSERT_EQ((long)g_ce51_dtable[3], 0x41);   /* turn 0x40 + 1 */
    /* (e) only the two target bytes changed: neighbours preserved verbatim */
    ASSERT_EQ((long)g_ce51_flags[0x0F], 0xAB);
    ASSERT_EQ((long)g_ce51_flags[0x11], 0xCD);
    ASSERT_EQ((long)g_ce51_dtable[2], 0xAA);
    ASSERT_EQ((long)g_ce51_dtable[4], 0xBB);

    ce51_teardown();
}

/* ----------------------------------------------------------------
 * Both stores are 8-bit. Seed stage = 0xFF and turn_counter = 0xFF: the stage
 * INC wraps 0xFF -> 0x00 and the +3 store = (uint8)(0xFF + 1) = 0x00, pinning the
 * truncation on both writes. Neighbours stay untouched so the wrap is the only
 * effect at each offset.
 * ---------------------------------------------------------------- */
static void test_h51_both_stores_are_8bit_wrap(void)
{
    ce51_setup(0xFF, 0xFF);
    g_ce51_flags[0x0F] = 0x11;
    g_ce51_flags[0x11] = 0x22;
    g_ce51_dtable[2]   = 0x33;
    g_ce51_dtable[4]   = 0x44;

    fd2_chapter_event_handler_51__unref_dyn_turn_event(0x33);

    /* (d) stage INC wraps 0xFF -> 0x00 (8-bit INC byte ptr) */
    ASSERT_EQ((long)g_ce51_flags[0x10], 0x00);
    /* (d) +3 = (uint8)(0xFF + 1) = 0x00 (8-bit turn+1) */
    ASSERT_EQ((long)g_ce51_dtable[3], 0x00);
    /* neighbours preserved */
    ASSERT_EQ((long)g_ce51_flags[0x0F], 0x11);
    ASSERT_EQ((long)g_ce51_flags[0x11], 0x22);
    ASSERT_EQ((long)g_ce51_dtable[2], 0x33);
    ASSERT_EQ((long)g_ce51_dtable[4], 0x44);

    ce51_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_52__ch30_major_cinematic @ 0x35F92
 *
 * ch30 final-boss multi-stage cinematic (dispatch idx 0x52 @ table 0x51B91).
 * Body (1-arg cdecl; arg ignored), stage = tile_event_consumed_flags[0x10]:
 *   fd2_pan_cursor_and_window(0x10, 1)
 *   page-(stage+2) dialog (0xA0000)
 *   battle_anim_phase = 0
 *   fd2_pan_cursor_and_window(0x10, 0xE)
 *   fd2_cinematic_warp_char_to_tile(0x18-stage, 0x16, 0x12)        -- boss line
 *   if (stage != 4):                                               -- stages 0..3
 *     fd2_load_chapter_portraits_and_dump_tmp(stage)
 *     fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x18-stage, 0x18-stage, 0)  -- disarm
 *     fd2_cinematic_warp_char_to_tile(2*stage+0x19, 0x15, 0x12)    -- pair A
 *     fd2_cinematic_warp_char_to_tile(2*stage+0x1A, 0x17, 0x12)    -- pair B
 *     battle_anim_phase = 1; return
 *   // stage == 4
 *   fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x14, 0x14, 0xB)  -- arm AI 0xB
 *   battle_anim_phase = 1
 *
 * Risk-bearing HERE is this handler's own routing: the control-flow branch on
 * stage==4, the warp char-index arithmetic (boss = 0x18-stage; pair =
 * 2*stage+0x19 / +0x1A), the AI-nibble writer's char/value split (disarm 0 of the
 * descending boss vs arm 0xB of fixed char 0x14, high nibble preserved each), the
 * dialog page = stage+2 routing, and the battle_anim_phase end state. These drive
 * the REAL handler + its REAL callees over the proven chevt2 host-safe env: the
 * real pan (window origin pre-set to the literal x target so the x-loop is empty;
 * the y-loop steps through the testglob composite recorder), the real dialog VM
 * over an immediate-END page program (pages 2..6 share a 1-glyph body; the glyph
 * blit is the testglob recorder), the real portrait loader with alloc_offset 0 so
 * the race scan is a host-safe no-op (still re-reads FDFIELD.DAT + rewrites
 * FD2.TMP), and the real AI-flag writer over a 0x20-entry runtime_char array.
 * The still-unemitted fd2_cinematic_warp_char_to_tile is the testglob recording
 * stub, so the warp char/tile arguments are observable directly (its full
 * teleport animation is pure display deferred to Phase 9). The dialog glyph
 * pixels and the pan composites are pure display side effects, deferred to Phase
 * 9; they execute for real here only as a byproduct and are not asserted.
 *
 * Own in-memory fixtures (own flags buffer + render workspace + dialog program +
 * 0x20-entry runtime_char array) so the suite never aliases the h4d/h4e/h4f state.
 * ================================================================ */
#define CE52_AI_OFF      0x34       /* combat_aux_block[0xD] absolute offset */
#define CE52_NCHARS      0x30       /* must cover boss/pair indices up to ~0x20 */

static uint8        g_ce52_flags[0x20];        /* [0x10] = stage counter            */
static runtime_char g_ce52_rc[CE52_NCHARS];    /* AI-nibble + warp char targets     */
static uint8       *g_ce52_tileevent;          /* portrait loader scan base         */
#define CE52_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ce52_ws[CE52_WS_SPAN];          /* render workspace for pan composite */
static uint8 g_ce52_atlas[6 + 64 * 4 + 4];
static int16 g_ce52_text[0x10];                /* dialog program: pages 2..6 -> body */

/* offset 0x34 of char `idx` (combat_aux_block[0xD]), read as a raw byte */
static uint8 ce52_ai(int idx)
{
    return ((uint8 *)&g_ce52_rc[idx])[CE52_AI_OFF];
}

/* Stand up the full real-pan + real-dialog + real-portrait + AI-flag env. `stage`
 * seeds the stage counter; the window origin starts at (0x10, start_oy) so the two
 * pans' x-loops are empty (x already on target) and the y-loops step to 1 then
 * 0xE. Every char's combat_aux_block[0xD] is seeded 0xA5 (non-zero high + low
 * nibble) so the AI writes (disarm-to-0 / arm-to-0xB) and the out-of-range
 * preservation are all observable. */
static void ce52_setup(uint8 stage, uint32 start_oy)
{
    int i;
    uint32 *atlas_tbl;

    /* --- handler state: stage byte --- */
    memset(g_ce52_flags, 0, sizeof(g_ce52_flags));
    g_ce52_flags[0x10] = stage;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce52_flags;

    /* --- runtime_char array: AI nibbles seeded 0xA5 across the whole array --- */
    memset(g_ce52_rc, 0, sizeof(g_ce52_rc));
    for (i = 0; i < CE52_NCHARS; i++) {
        ((uint8 *)&g_ce52_rc[i])[CE52_AI_OFF] = 0xA5;
    }
    data_fd2_battle_runtime_char_array_ptr = g_ce52_rc;

    /* --- portrait loader env: alloc_offset 0 -> race scan is a host-safe no-op --- */
    g_ce52_tileevent = (uint8 *)malloc(0x98 + 0x20);
    memset(g_ce52_tileevent, 0, 0x98 + 0x20);
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce52_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 1;
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;       /* re-read idx = 4*3+2 = 0xE */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    /* --- render env for the two pan composites --- */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce52_ws - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_view_window_origin_x = 0x10;   /* x already on target -> empty x-loop */
    data_fd2_battle_view_window_origin_y = start_oy;
    data_fd2_battle_anim_phase = 1;
    atlas_tbl = (uint32 *)(g_ce52_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce52_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    /* --- dialog VM program: page headers 2..6 each point at a shared 1-glyph +
     * END body (byte offset 14 = int16 index 7). Each dialog call renders one
     * glyph then returns (no page-break wait). --- */
    for (i = 0; i < 0x10; i++) {
        g_ce52_text[i] = 0;
    }
    for (i = 2; i <= 6; i++) {
        g_ce52_text[i] = (int16)(7 * 2);
    }
    g_ce52_text[7] = 0x41;                          /* one TEXT glyph */
    g_ce52_text[8] = -1;                            /* END */
    current_chapter_text = (uint32)(uint8 *)g_ce52_text;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated + no
     * active portrait so END takes neither the page-break wait nor the
     * portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    /* --- recorders --- */
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;
    g_warp_char_calls = 0;
    memset(g_warp_char_id, 0, sizeof(g_warp_char_id));
    memset(g_warp_tile_x, 0, sizeof(g_warp_tile_x));
    memset(g_warp_tile_y, 0, sizeof(g_warp_tile_y));
}

static void ce52_teardown(void)
{
    audiofix_disable_sfx();
    free(g_ce52_tileevent);
    g_ce52_tileevent = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    current_chapter_text = 0;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * STAGE 0 (the != 4 spawn branch): drive the handler at stage 0.
 *   - dialog page = stage + 2 = 2 renders (one glyph),
 *   - THREE warps fire with the exact computed indices and literal tiles:
 *       boss   = 0x18 - 0 = 0x18 onto (0x16, 0x12),
 *       pair A = 2*0 + 0x19 = 0x19 onto (0x15, 0x12),
 *       pair B = 2*0 + 0x1A = 0x1A onto (0x17, 0x12),
 *   - the boss char 0x18 is DISARMED: combat_aux_block[0xD] low nibble = 0 with
 *     the high nibble preserved (0xA5 -> 0xA0); the just-outside chars 0x17 and
 *     0x19 keep 0xA5,
 *   - battle_anim_phase ends at 1.
 * The dispatch arg is passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h52_stage0_spawn_branch(void)
{
    ce52_setup(0, 0x20);

    fd2_chapter_event_handler_52__ch30_major_cinematic(0x77);

    /* dialog ran for page stage+2 = 2: exactly one glyph rendered */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);

    /* THREE warps with exact indices + literal tiles */
    ASSERT_EQ((long)g_warp_char_calls, 3);
    ASSERT_EQ((long)g_warp_char_id[0], 0x18);   /* boss = 0x18 - stage */
    ASSERT_EQ((long)g_warp_tile_x[0], 0x16);
    ASSERT_EQ((long)g_warp_tile_y[0], 0x12);
    ASSERT_EQ((long)g_warp_char_id[1], 0x19);   /* pair A = 2*stage + 0x19 */
    ASSERT_EQ((long)g_warp_tile_x[1], 0x15);
    ASSERT_EQ((long)g_warp_tile_y[1], 0x12);
    ASSERT_EQ((long)g_warp_char_id[2], 0x1A);   /* pair B = 2*stage + 0x1A */
    ASSERT_EQ((long)g_warp_tile_x[2], 0x17);
    ASSERT_EQ((long)g_warp_tile_y[2], 0x12);

    /* boss char 0x18 disarmed: low nibble 0, high nibble preserved -> 0xA0 */
    ASSERT_EQ((long)ce52_ai(0x18), 0xA0);
    /* single-char range: immediate neighbours untouched */
    ASSERT_EQ((long)ce52_ai(0x17), 0xA5);
    ASSERT_EQ((long)ce52_ai(0x19), 0xA5);

    /* end state */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);

    ce52_teardown();
}

/* ----------------------------------------------------------------
 * STAGE 3 (still the != 4 spawn branch): the warp/AI indices track the stage, so
 * a second stage proves the arithmetic is computed, not constant.
 *   - dialog page = 3 + 2 = 5,
 *   - boss   = 0x18 - 3 = 0x15 onto (0x16, 0x12),
 *   - pair A = 2*3 + 0x19 = 0x1F onto (0x15, 0x12),
 *   - pair B = 2*3 + 0x1A = 0x20 onto (0x17, 0x12),
 *   - the boss char 0x15 is disarmed to low-nibble 0 (0xA5 -> 0xA0); char 0x14
 *     (the stage-4 target) and char 0x16 stay 0xA5,
 *   - battle_anim_phase ends at 1.
 * ---------------------------------------------------------------- */
static void test_h52_stage3_indices_track_stage(void)
{
    ce52_setup(3, 0x20);

    fd2_chapter_event_handler_52__ch30_major_cinematic(0);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);          /* page 5 rendered one glyph */

    ASSERT_EQ((long)g_warp_char_calls, 3);
    ASSERT_EQ((long)g_warp_char_id[0], 0x15);       /* boss = 0x18 - 3 */
    ASSERT_EQ((long)g_warp_char_id[1], 0x1F);       /* pair A = 6 + 0x19 */
    ASSERT_EQ((long)g_warp_char_id[2], 0x20);       /* pair B = 6 + 0x1A */

    ASSERT_EQ((long)ce52_ai(0x15), 0xA0);           /* boss disarmed */
    ASSERT_EQ((long)ce52_ai(0x14), 0xA5);           /* stage-4 target untouched here */
    ASSERT_EQ((long)ce52_ai(0x16), 0xA5);

    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);

    ce52_teardown();
}

/* ----------------------------------------------------------------
 * STAGE 4 (the final branch): the handler takes the OTHER path — no portrait
 * load, no paired warps, and a DIFFERENT AI write.
 *   - dialog page = 4 + 2 = 6 renders,
 *   - exactly ONE warp fires: boss = 0x18 - 4 = 0x14 onto (0x16, 0x12)
 *     (the boss-line bottom; this is the only warp on the final stage),
 *   - char 0x14 is ARMED with AI mode 0xB: low nibble = 0xB, high nibble
 *     preserved (0xA5 -> 0xAB) — the defining contrast against the stage 0..3
 *     disarm-to-0; neighbours 0x13 and 0x15 stay 0xA5,
 *   - battle_anim_phase ends at 1.
 * The dispatch arg is passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h52_stage4_final_branch(void)
{
    ce52_setup(4, 0x20);

    fd2_chapter_event_handler_52__ch30_major_cinematic(0x77);

    /* dialog ran for page 6 */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);

    /* exactly ONE warp on the final stage (no paired spawns) */
    ASSERT_EQ((long)g_warp_char_calls, 1);
    ASSERT_EQ((long)g_warp_char_id[0], 0x14);       /* boss = 0x18 - 4 */
    ASSERT_EQ((long)g_warp_tile_x[0], 0x16);
    ASSERT_EQ((long)g_warp_tile_y[0], 0x12);

    /* char 0x14 ARMED to 0xB (high nibble preserved) -> 0xAB */
    ASSERT_EQ((long)ce52_ai(0x14), 0xAB);
    /* single-char range: immediate neighbours untouched */
    ASSERT_EQ((long)ce52_ai(0x13), 0xA5);
    ASSERT_EQ((long)ce52_ai(0x15), 0xA5);

    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);

    ce52_teardown();
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
    RUN_TEST(test_h50_sets_ai_flag_b_for_char_0x14);
    RUN_TEST(test_h50_range_is_single_char_exact);
    RUN_TEST(test_h51_advances_stage_and_schedules);
    RUN_TEST(test_h51_advance_ungated_and_offsets_exact);
    RUN_TEST(test_h51_both_stores_are_8bit_wrap);
    RUN_TEST(test_h52_stage0_spawn_branch);
    RUN_TEST(test_h52_stage3_indices_track_stage);
    RUN_TEST(test_h52_stage4_final_branch);
    printf("\n");
}
