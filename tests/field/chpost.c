/*
 * unit tests for src/field/chpost.c
 *
 * fd2_chapter_02_post_action runs the default win/lose check
 * (fd2_check_battle_end_condition, linked real) and then applies a
 * game-over override that fires ONLY when all 6 key NPCs at
 * runtime_char[5..10] are dead; the override loop early-exits the instant
 * any one of those slots is still alive.
 *
 * These tests redirect data_fd2_battle_runtime_char_array_ptr at a local
 * 16-entry array because the shared g_test_rc_array is only 8 entries and
 * this handler reaches index 10. Each case arranges the default check to
 * deterministically yield flag=2 (no alive enemies, protagonist alive) so
 * the override is observable as a 2 -> 1 transition: if the override fires
 * the flag becomes 1, otherwise it stays 2.
 *
 * Coverage is risk-driven for the inverted-looking early-exit branch (the
 * disassembly's "JZ exit / JMP continue" is exactly the kind of test that
 * is easy to read backwards) and the [5..10] loop bounds:
 *   - all 6 dead          -> override fires (flag 1)
 *   - first slot (5) alive-> early exit (flag stays 2)
 *   - last slot (10) alive-> early exit at the final iteration (flag 2)
 *   - one middle slot alive-> early exit (flag 2)
 *   - slots 4 and 11 alive while 5..10 dead -> override still fires,
 *     pinning that the loop neither starts at 4 nor extends to 11.
 */

#include <string.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];

/* Local 16-slot runtime_char array (handler reaches index 10; the shared
 * 8-slot g_test_rc_array is too small). */
static runtime_char t_rc[16];

/* Arrange the default fd2_check_battle_end_condition to yield flag=2:
 * every slot is team=2 (player) so no "team==0 && alive" enemy resets the
 * flag to 0, and the protagonist (slot 0) is alive so the trailing
 * dead-protagonist check does not force flag=1. The override's effect is
 * then a clean 2 -> 1 transition (or no change). */
static void chpost_setup(void)
{
    int i;

    memset(t_rc, 0, sizeof(t_rc));
    for (i = 0; i < 16; i++) {
        t_rc[i].team = 2;       /* player team: never an alive enemy */
        t_rc[i].flags = 0;      /* alive */
    }
    data_fd2_battle_runtime_char_array_ptr = t_rc;
    data_fd2_battle_party_member_count = 16;
    data_fd2_chapter_event_or_battle_end_code = 0;
}

static void chpost_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
}

/* All 6 key NPCs (slots 5..10) dead -> the loop runs to completion and the
 * override sets game_event_flag = 1. */
static void test_chpost02_all_six_dead_game_over(void)
{
    int i;

    chpost_setup();
    for (i = 5; i <= 10; i++) {
        t_rc[i].flags = CHARFLAG_DEAD;
    }

    fd2_chapter_02_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost_teardown();
}

/* First key slot (5) still alive -> the loop early-exits on iteration 0 and
 * leaves the default flag (2) untouched. Pins the loop start index = 5 and
 * the early-exit-on-alive branch direction. */
static void test_chpost02_first_slot_alive_keeps_default(void)
{
    int i;

    chpost_setup();
    for (i = 5; i <= 10; i++) {
        t_rc[i].flags = CHARFLAG_DEAD;
    }
    t_rc[5].flags = 0;          /* slot 5 alive */

    fd2_chapter_02_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost_teardown();
}

/* Last key slot (10) alive, 5..9 dead -> the loop survives 5 dead slots and
 * only early-exits at the final iteration. Pins the inclusive upper bound
 * = 10 (the override must NOT fire). */
static void test_chpost02_last_slot_alive_keeps_default(void)
{
    int i;

    chpost_setup();
    for (i = 5; i <= 10; i++) {
        t_rc[i].flags = CHARFLAG_DEAD;
    }
    t_rc[10].flags = 0;         /* slot 10 alive */

    fd2_chapter_02_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost_teardown();
}

/* A middle key slot (7) alive -> early exit, flag stays 2. */
static void test_chpost02_middle_slot_alive_keeps_default(void)
{
    int i;

    chpost_setup();
    for (i = 5; i <= 10; i++) {
        t_rc[i].flags = CHARFLAG_DEAD;
    }
    t_rc[7].flags = 0;          /* slot 7 alive */

    fd2_chapter_02_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost_teardown();
}

/* Slots 4 and 11 alive while 5..10 are all dead -> override still fires.
 * Proves the checked range is exactly [5,10]: a survivor just below (4) or
 * just above (11) the range does not prevent game over. */
static void test_chpost02_neighbors_outside_range_ignored(void)
{
    int i;

    chpost_setup();
    for (i = 5; i <= 10; i++) {
        t_rc[i].flags = CHARFLAG_DEAD;
    }
    t_rc[4].flags = 0;          /* below range, alive */
    t_rc[11].flags = 0;         /* above range, alive */

    fd2_chapter_02_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost_teardown();
}

/* ============================================================
 * fd2_chapter_10_post_action @ 0x20707
 *
 * Same default win/lose check (fd2_check_battle_end_condition, linked
 * real), then a lose-condition override: if escort NPC runtime_char[0x32]
 * OR [0x33] is dead, set game_event_flag = 1. Deadness is queried through
 * fd2_check_char_is_dead, which the real engine computes as
 * runtime_char[idx].flags bit0.
 *
 * fd2_check_char_is_dead is a testglob stub. Its default index-agnostic
 * mode cannot distinguish slot 0x32 from 0x33, so these tests opt into its
 * array-reading mode (g_check_char_is_dead_use_array = 1), which mirrors
 * the real function by returning runtime_char[idx].flags bit0 — exactly the
 * per-slot behavior chapter 10 depends on.
 *
 * Slot 0x33 (51) is reached, so these tests redirect the array at a
 * 56-slot local buffer (the shared 8-slot g_test_rc_array and the
 * chapter-2 16-slot t_rc are both too small). As in the chapter-2 suite
 * every slot is team=2 / alive so the default check deterministically
 * yields flag=2, making the override observable as a clean 2 -> 1.
 *
 * Coverage is risk-driven for the OR short-circuit and the inverted-
 * looking branch shape in the disassembly (JNZ-to-set on the first dead,
 * JZ-to-return on the second alive):
 *   - both escorts alive          -> no override (flag stays 2)
 *   - [0x32] dead, [0x33] alive    -> override fires via the first test
 *   - [0x32] alive, [0x33] dead    -> override fires via the second test
 *                                     (proves [0x33] is still evaluated)
 *   - both dead                    -> override fires
 *   - neighbors 0x31/0x34 dead, escorts alive -> NO override, pinning the
 *     checked slots as exactly 0x32 and 0x33 (not off-by-one).
 * ============================================================ */

extern int g_check_char_is_dead_use_array;

#define CH10_RC_SLOTS 56
static runtime_char t_rc10[CH10_RC_SLOTS];

static void chpost10_setup(void)
{
    int i;

    memset(t_rc10, 0, sizeof(t_rc10));
    for (i = 0; i < CH10_RC_SLOTS; i++) {
        t_rc10[i].team = 2;     /* player team: never an alive enemy */
        t_rc10[i].flags = 0;    /* alive */
    }
    data_fd2_battle_runtime_char_array_ptr = t_rc10;
    data_fd2_battle_party_member_count = CH10_RC_SLOTS;
    data_fd2_chapter_event_or_battle_end_code = 0;
    g_check_char_is_dead_use_array = 1;   /* per-slot .flags drive deadness */
}

static void chpost10_teardown(void)
{
    g_check_char_is_dead_use_array = 0;   /* restore index-agnostic default */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
}

/* Both escorts alive -> the OR is false, override does not fire, the
 * default flag (2) survives. */
static void test_chpost10_both_escorts_alive_keeps_default(void)
{
    chpost10_setup();
    /* slots 0x32, 0x33 already alive from setup */

    fd2_chapter_10_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost10_teardown();
}

/* Escort 0x32 dead -> the first fd2_check_char_is_dead returns nonzero and
 * the override fires (short-circuits before testing 0x33). */
static void test_chpost10_first_escort_dead_game_over(void)
{
    chpost10_setup();
    t_rc10[0x32].flags = CHARFLAG_DEAD;

    fd2_chapter_10_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost10_teardown();
}

/* Escort 0x32 alive, 0x33 dead -> the first test passes (alive) so the
 * second test must run; it returns nonzero and the override fires. Pins
 * that 0x33 is genuinely evaluated, not dead code. */
static void test_chpost10_second_escort_dead_game_over(void)
{
    chpost10_setup();
    t_rc10[0x33].flags = CHARFLAG_DEAD;

    fd2_chapter_10_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost10_teardown();
}

/* Both escorts dead -> override fires. */
static void test_chpost10_both_escorts_dead_game_over(void)
{
    chpost10_setup();
    t_rc10[0x32].flags = CHARFLAG_DEAD;
    t_rc10[0x33].flags = CHARFLAG_DEAD;

    fd2_chapter_10_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost10_teardown();
}

/* Neighbors 0x31 and 0x34 dead while the two escorts (0x32, 0x33) are
 * alive -> the override must NOT fire. Proves the checked slots are
 * exactly 0x32 and 0x33 (no off-by-one in either direction). */
static void test_chpost10_neighbor_slots_ignored(void)
{
    chpost10_setup();
    t_rc10[0x31].flags = CHARFLAG_DEAD;
    t_rc10[0x34].flags = CHARFLAG_DEAD;

    fd2_chapter_10_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost10_teardown();
}

/* ============================================================
 * fd2_chapter_12_post_action @ 0x2073D
 *
 * Same default win/lose check (fd2_check_battle_end_condition, linked
 * real), then a single-slot lose-condition override: if key NPC
 * runtime_char[0xE] is dead, set game_event_flag = 1. Deadness is queried
 * through fd2_check_char_is_dead, the real engine computing it as
 * runtime_char[idx].flags bit0.
 *
 * Reuses the chapter-10 array-reading mode (g_check_char_is_dead_use_array
 * = 1) so per-slot .flags drive the result, and the same 56-slot t_rc10
 * buffer / chpost10_setup arrangement that pins the default check to flag=2;
 * the override is then observable as a clean 2 -> 1.
 *
 * Coverage is risk-driven for the inverted-looking branch (the disassembly
 * is "JZ skip-set / fall through to set", i.e. set-the-flag-when-DEAD; it is
 * exactly the kind of test that is easy to read backwards) and the single
 * checked slot index:
 *   - slot 0xE alive            -> no override (flag stays 2)
 *   - slot 0xE dead             -> override fires (flag -> 1)
 *   - neighbors 0xD/0xF dead, 0xE alive -> NO override, pinning the checked
 *     slot as exactly 0xE (no off-by-one in either direction).
 * ============================================================ */

/* Key NPC slot alive -> the dead-check returns 0, the override does not
 * fire, and the default flag (2) survives. Pins the branch direction:
 * an ALIVE slot must NOT trigger game over. */
static void test_chpost12_npc_alive_keeps_default(void)
{
    chpost10_setup();
    /* slot 0xE already alive from setup */

    fd2_chapter_12_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost10_teardown();
}

/* Key NPC slot 0xE dead -> fd2_check_char_is_dead returns nonzero and the
 * override fires (flag 2 -> 1). */
static void test_chpost12_npc_dead_game_over(void)
{
    chpost10_setup();
    t_rc10[0xE].flags = CHARFLAG_DEAD;

    fd2_chapter_12_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost10_teardown();
}

/* Neighbors 0xD and 0xF dead while the key NPC (0xE) is alive -> the
 * override must NOT fire. Proves the checked slot is exactly 0xE (no
 * off-by-one in either direction). */
static void test_chpost12_neighbor_slots_ignored(void)
{
    chpost10_setup();
    t_rc10[0xD].flags = CHARFLAG_DEAD;
    t_rc10[0xF].flags = CHARFLAG_DEAD;

    fd2_chapter_12_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost10_teardown();
}

void run_field_chpost_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chpost\n");
    RUN_TEST(test_chpost02_all_six_dead_game_over);
    RUN_TEST(test_chpost02_first_slot_alive_keeps_default);
    RUN_TEST(test_chpost02_last_slot_alive_keeps_default);
    RUN_TEST(test_chpost02_middle_slot_alive_keeps_default);
    RUN_TEST(test_chpost02_neighbors_outside_range_ignored);
    RUN_TEST(test_chpost10_both_escorts_alive_keeps_default);
    RUN_TEST(test_chpost10_first_escort_dead_game_over);
    RUN_TEST(test_chpost10_second_escort_dead_game_over);
    RUN_TEST(test_chpost10_both_escorts_dead_game_over);
    RUN_TEST(test_chpost10_neighbor_slots_ignored);
    RUN_TEST(test_chpost12_npc_alive_keeps_default);
    RUN_TEST(test_chpost12_npc_dead_game_over);
    RUN_TEST(test_chpost12_neighbor_slots_ignored);
    printf("\n");
}
