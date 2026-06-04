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

void run_field_chpost_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chpost\n");
    RUN_TEST(test_chpost02_all_six_dead_game_over);
    RUN_TEST(test_chpost02_first_slot_alive_keeps_default);
    RUN_TEST(test_chpost02_last_slot_alive_keeps_default);
    RUN_TEST(test_chpost02_middle_slot_alive_keeps_default);
    RUN_TEST(test_chpost02_neighbors_outside_range_ignored);
    printf("\n");
}
