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

/* ============================================================
 * fd2_chapter_13_post_action @ 0x20765
 *
 * The most complex non-default handler. Same default win/lose check
 * (fd2_check_battle_end_condition, linked real), then TWO independent
 * lose conditions, each of which sets game_event_flag = 1 AND plays a
 * chapter dialog page through the REAL fd2_display_dialog_scene:
 *   1. all of runtime_char[0xF..0x1A] (12 slots) dead -> page 10.
 *      The loop does NOT early-exit: it sets a "some slot alive" flag the
 *      instant any slot reports alive and scans the full range, so the
 *      condition fires only when no slot in [0xF,0x1A] is alive.
 *   2. turn counter (0x53BEF) > 5 AND runtime_char[0x3B] dead -> page 2.
 *
 * Deadness for both conditions is queried through fd2_check_char_is_dead,
 * the real engine computing it as runtime_char[idx].flags bit0; these tests
 * opt into the testglob array-reading mode (g_check_char_is_dead_use_array
 * = 1) so per-slot .flags drive each result. Slot 0x3B (59) is reached, so
 * a 64-slot local buffer is used (t_rc10 is only 56 slots).
 *
 * fd2_display_dialog_scene is linked real (a dialog-bytecode VM). Each
 * override sets the flag and calls it within the SAME basic block (the
 * disassembly has no branch between MOV [0x53ECC],1 and CALL 0x15F84), so
 * asserting the flag's 2 -> 1 transition fully pins that the override block
 * ran, and the dialog call is guaranteed to follow. To keep the real VM
 * side-effect-free here, current_chapter_text is pointed at an immediate-END
 * program (every page word references a -1 END marker): the VM dereferences
 * current_chapter_text + page*2, reads END, and returns at once without
 * touching the framebuffer or loading DATO.DAT. A clean (non-crashing) pass
 * therefore also confirms the real VM survives the chapter-13 call shape.
 *
 * Coverage is risk-driven for: the two independent conditions and their
 * interaction, the full-scan-no-early-exit loop over [0xF,0x1A] and its
 * exact bounds, and the strict turn `> 5` comparator (easy to read as >=):
 *   - nothing triggered (all NPC alive, turn<=5)     -> flag stays 2
 *   - all 12 NPC dead, turn<=5                        -> cond1 fires (page 10)
 *   - first slot 0xF alive (rest dead)               -> cond1 does NOT fire
 *   - last slot 0x1A alive (rest dead)               -> cond1 does NOT fire
 *   - neighbors 0xE/0x1B alive, [0xF,0x1A] dead      -> cond1 fires
 *   - turn==5, slot 0x3B dead                        -> cond2 does NOT fire
 *   - turn==6, slot 0x3B alive                       -> cond2 does NOT fire
 *   - turn==6, slot 0x3B dead, NPCs alive            -> cond2 fires (page 2)
 *   - turn==6, neighbors 0x3A/0x3C dead, 0x3B alive  -> cond2 does NOT fire
 *   - all 12 NPC dead AND turn==6 AND 0x3B dead       -> both fire
 * ============================================================ */

#define CH13_RC_SLOTS 64
static runtime_char t_rc13[CH13_RC_SLOTS];

/* Immediate-END dialog program for current_chapter_text: every page word
 * (pages 0..0x3F, covering pages 2 and 10) points at a -1 END marker parked
 * high in the buffer, so the real fd2_display_dialog_scene returns at once. */
static uint16 t_ch13_text[0x400];

static void ch13_text_all_end(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        t_ch13_text[i] = 0;
    }
    *(int16 *)((uint8 *)t_ch13_text + 0x780) = -1;     /* END marker */
    for (i = 0; i < 0x3c0; i++) {
        t_ch13_text[i] = (uint16)0x780;                /* byte offset of END */
    }
    current_chapter_text = (uint32)t_ch13_text;
}

/* All slots team=2 / alive so fd2_check_battle_end_condition yields flag=2,
 * making each override observable as a clean 2 -> 1. Default turn counter is
 * 0 (<= 5) so condition 2 is inert unless a test raises it. */
static void chpost13_setup(void)
{
    int i;

    memset(t_rc13, 0, sizeof(t_rc13));
    for (i = 0; i < CH13_RC_SLOTS; i++) {
        t_rc13[i].team = 2;     /* player team: never an alive enemy */
        t_rc13[i].flags = 0;    /* alive */
    }
    data_fd2_battle_runtime_char_array_ptr = t_rc13;
    data_fd2_battle_party_member_count = CH13_RC_SLOTS;
    data_fd2_chapter_event_or_battle_end_code = 0;
    data_fd2_battle_turn_counter = 0;
    g_check_char_is_dead_use_array = 1;   /* per-slot .flags drive deadness */
    ch13_text_all_end();
}

static void chpost13_teardown(void)
{
    g_check_char_is_dead_use_array = 0;   /* restore index-agnostic default */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_turn_counter = 0;
}

/* mark slots [0xF, 0x1A] (the 12 condition-1 NPCs) dead */
static void ch13_kill_npc_range(void)
{
    int i;

    for (i = 0xF; i <= 0x1A; i++) {
        t_rc13[i].flags = CHARFLAG_DEAD;
    }
}

/* No condition triggered: every NPC alive (cond1 false) and turn 0 <= 5
 * (cond2 inert). The default flag (2) survives -> neither override ran. */
static void test_chpost13_nothing_triggered_keeps_default(void)
{
    chpost13_setup();

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost13_teardown();
}

/* All 12 NPCs (slots 0xF..0x1A) dead, turn <= 5 -> condition 1 fires: the
 * loop scans the whole range finding none alive, sets game_event_flag = 1
 * and plays page 10 via the real (immediate-END) dialog VM. */
static void test_chpost13_all_npc_dead_cond1_game_over(void)
{
    chpost13_setup();
    ch13_kill_npc_range();

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost13_teardown();
}

/* First condition-1 slot (0xF) alive, 0x10..0x1A dead -> the loop sets the
 * "some alive" flag on iteration 0 and (without early-exit) still completes,
 * leaving cond1 false. Pins the loop start index = 0xF. */
static void test_chpost13_cond1_first_slot_alive_keeps_default(void)
{
    chpost13_setup();
    ch13_kill_npc_range();
    t_rc13[0xF].flags = 0;          /* slot 0xF alive */

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost13_teardown();
}

/* Last condition-1 slot (0x1A) alive, 0xF..0x19 dead -> the loop survives 11
 * dead slots and only sees the survivor at its final iteration, so cond1 does
 * NOT fire. Pins the inclusive upper bound = 0x1A (i < 0xC). */
static void test_chpost13_cond1_last_slot_alive_keeps_default(void)
{
    chpost13_setup();
    ch13_kill_npc_range();
    t_rc13[0x1A].flags = 0;         /* slot 0x1A alive */

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost13_teardown();
}

/* Neighbors 0xE and 0x1B alive while [0xF,0x1A] are all dead -> cond1 still
 * fires. Proves the scanned range is exactly [0xF,0x1A] (a survivor just
 * below or just above the range does not prevent game over). turn stays <= 5
 * so cond2 cannot interfere. */
static void test_chpost13_cond1_neighbors_outside_range_ignored(void)
{
    chpost13_setup();
    ch13_kill_npc_range();
    t_rc13[0xE].flags = 0;          /* below range, alive */
    t_rc13[0x1B].flags = 0;         /* above range, alive */

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost13_teardown();
}

/* Condition 2 with turn == 5 (NOT > 5) and slot 0x3B dead -> cond2 does NOT
 * fire. NPCs left alive so cond1 is also false. Pins the strict `turn > 5`
 * comparator (the boundary value 5 must not trigger). */
static void test_chpost13_cond2_turn_eq_5_keeps_default(void)
{
    chpost13_setup();
    data_fd2_battle_turn_counter = 5;
    t_rc13[0x3B].flags = CHARFLAG_DEAD;

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost13_teardown();
}

/* Condition 2 with turn == 6 (> 5) but slot 0x3B alive -> cond2 does NOT
 * fire. Pins that the deadness of 0x3B is required, not just the turn gate. */
static void test_chpost13_cond2_boss_alive_keeps_default(void)
{
    chpost13_setup();
    data_fd2_battle_turn_counter = 6;
    /* slot 0x3B already alive from setup */

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost13_teardown();
}

/* Condition 2 fires: turn == 6 (> 5) AND slot 0x3B dead, NPCs left alive so
 * cond1 stays false. game_event_flag goes 2 -> 1 and page 2 plays via the
 * real (immediate-END) dialog VM. Isolates cond2 from cond1. */
static void test_chpost13_cond2_boss_dead_game_over(void)
{
    chpost13_setup();
    data_fd2_battle_turn_counter = 6;
    t_rc13[0x3B].flags = CHARFLAG_DEAD;

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost13_teardown();
}

/* Condition 2 with turn > 5 but neighbors 0x3A and 0x3C dead while 0x3B is
 * alive -> cond2 does NOT fire. Proves the checked slot is exactly 0x3B (no
 * off-by-one in either direction). */
static void test_chpost13_cond2_neighbor_slots_ignored(void)
{
    chpost13_setup();
    data_fd2_battle_turn_counter = 6;
    t_rc13[0x3A].flags = CHARFLAG_DEAD;
    t_rc13[0x3C].flags = CHARFLAG_DEAD;

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost13_teardown();
}

/* Both conditions satisfied at once: all 12 NPCs dead AND turn == 6 AND slot
 * 0x3B dead. Both override blocks run (each sets the flag and plays its page
 * through the real dialog VM); the resulting flag is 1. Exercises the
 * sequential, independent structure of the two conditions in one call. */
static void test_chpost13_both_conditions_game_over(void)
{
    chpost13_setup();
    ch13_kill_npc_range();
    data_fd2_battle_turn_counter = 6;
    t_rc13[0x3B].flags = CHARFLAG_DEAD;

    fd2_chapter_13_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost13_teardown();
}

/* ============================================================
 * fd2_chapter_15_post_action @ 0x20822
 *
 * Same default win/lose check (fd2_check_battle_end_condition, linked
 * real), then a single-slot lose-condition override: if key NPC
 * runtime_char[0x40] is dead, set game_event_flag = 1. Deadness is queried
 * through fd2_check_char_is_dead, the real engine computing it as
 * runtime_char[idx].flags bit0.
 *
 * Structurally identical to chapter 12 but for slot 0x40 (64) instead of
 * 0xE; that slot is reached, so a 72-slot local buffer is used (t_rc13 is
 * only 64 slots, indices 0..0x3F, one short of 0x40). Reuses the testglob
 * array-reading mode (g_check_char_is_dead_use_array = 1) so per-slot
 * .flags drive the result, and the same all-team-2 / alive arrangement that
 * pins the default check to flag=2; the override is then observable as a
 * clean 2 -> 1.
 *
 * Coverage is risk-driven for the inverted-looking branch (the disassembly
 * is "JZ skip-set / fall through to set", i.e. set-the-flag-when-DEAD; it is
 * exactly the kind of test that is easy to read backwards) and the single
 * checked slot index:
 *   - slot 0x40 alive            -> no override (flag stays 2)
 *   - slot 0x40 dead             -> override fires (flag -> 1)
 *   - neighbors 0x3F/0x41 dead, 0x40 alive -> NO override, pinning the
 *     checked slot as exactly 0x40 (no off-by-one in either direction).
 * ============================================================ */

#define CH15_RC_SLOTS 72
static runtime_char t_rc15[CH15_RC_SLOTS];

static void chpost15_setup(void)
{
    int i;

    memset(t_rc15, 0, sizeof(t_rc15));
    for (i = 0; i < CH15_RC_SLOTS; i++) {
        t_rc15[i].team = 2;     /* player team: never an alive enemy */
        t_rc15[i].flags = 0;    /* alive */
    }
    data_fd2_battle_runtime_char_array_ptr = t_rc15;
    data_fd2_battle_party_member_count = CH15_RC_SLOTS;
    data_fd2_chapter_event_or_battle_end_code = 0;
    g_check_char_is_dead_use_array = 1;   /* per-slot .flags drive deadness */
}

static void chpost15_teardown(void)
{
    g_check_char_is_dead_use_array = 0;   /* restore index-agnostic default */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
}

/* Key NPC slot 0x40 alive -> the dead-check returns 0, the override does not
 * fire, and the default flag (2) survives. Pins the branch direction: an
 * ALIVE slot must NOT trigger game over. */
static void test_chpost15_npc_alive_keeps_default(void)
{
    chpost15_setup();
    /* slot 0x40 already alive from setup */

    fd2_chapter_15_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost15_teardown();
}

/* Key NPC slot 0x40 dead -> fd2_check_char_is_dead returns nonzero and the
 * override fires (flag 2 -> 1). */
static void test_chpost15_npc_dead_game_over(void)
{
    chpost15_setup();
    t_rc15[0x40].flags = CHARFLAG_DEAD;

    fd2_chapter_15_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost15_teardown();
}

/* Neighbors 0x3F and 0x41 dead while the key NPC (0x40) is alive -> the
 * override must NOT fire. Proves the checked slot is exactly 0x40 (no
 * off-by-one in either direction). */
static void test_chpost15_neighbor_slots_ignored(void)
{
    chpost15_setup();
    t_rc15[0x3F].flags = CHARFLAG_DEAD;
    t_rc15[0x41].flags = CHARFLAG_DEAD;

    fd2_chapter_15_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost15_teardown();
}

/* ============================================================
 * fd2_chapter_16_post_action @ 0x2084A
 *
 * Same default win/lose check (fd2_check_battle_end_condition, linked
 * real), then a single-slot lose-condition override: if key NPC
 * runtime_char[0x41] is dead, set game_event_flag = 1. Deadness is queried
 * through fd2_check_char_is_dead, the real engine computing it as
 * runtime_char[idx].flags bit0.
 *
 * Structurally identical to chapter 15 but for slot 0x41 (65) instead of
 * 0x40; the disassembly is byte-for-byte the same modulo that one PUSH
 * immediate. Slot 0x41 lies within the 72-slot t_rc15 buffer (indices
 * 0..0x47), so these tests reuse t_rc15 / chpost15_setup directly. As in
 * the chapter-15 suite every slot is team=2 / alive so the default check
 * deterministically yields flag=2, making the override observable as a
 * clean 2 -> 1.
 *
 * Coverage is risk-driven for the inverted-looking branch (the disassembly
 * is "JZ skip-set / fall through to set", i.e. set-the-flag-when-DEAD; it is
 * exactly the kind of test that is easy to read backwards) and the single
 * checked slot index:
 *   - slot 0x41 alive            -> no override (flag stays 2)
 *   - slot 0x41 dead             -> override fires (flag -> 1)
 *   - neighbors 0x40/0x42 dead, 0x41 alive -> NO override, pinning the
 *     checked slot as exactly 0x41 (no off-by-one in either direction).
 * ============================================================ */

/* Key NPC slot 0x41 alive -> the dead-check returns 0, the override does not
 * fire, and the default flag (2) survives. Pins the branch direction: an
 * ALIVE slot must NOT trigger game over. */
static void test_chpost16_npc_alive_keeps_default(void)
{
    chpost15_setup();
    /* slot 0x41 already alive from setup */

    fd2_chapter_16_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost15_teardown();
}

/* Key NPC slot 0x41 dead -> fd2_check_char_is_dead returns nonzero and the
 * override fires (flag 2 -> 1). */
static void test_chpost16_npc_dead_game_over(void)
{
    chpost15_setup();
    t_rc15[0x41].flags = CHARFLAG_DEAD;

    fd2_chapter_16_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost15_teardown();
}

/* Neighbors 0x40 and 0x42 dead while the key NPC (0x41) is alive -> the
 * override must NOT fire. Proves the checked slot is exactly 0x41 (no
 * off-by-one in either direction). */
static void test_chpost16_neighbor_slots_ignored(void)
{
    chpost15_setup();
    t_rc15[0x40].flags = CHARFLAG_DEAD;
    t_rc15[0x42].flags = CHARFLAG_DEAD;

    fd2_chapter_16_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost15_teardown();
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
    RUN_TEST(test_chpost13_nothing_triggered_keeps_default);
    RUN_TEST(test_chpost13_all_npc_dead_cond1_game_over);
    RUN_TEST(test_chpost13_cond1_first_slot_alive_keeps_default);
    RUN_TEST(test_chpost13_cond1_last_slot_alive_keeps_default);
    RUN_TEST(test_chpost13_cond1_neighbors_outside_range_ignored);
    RUN_TEST(test_chpost13_cond2_turn_eq_5_keeps_default);
    RUN_TEST(test_chpost13_cond2_boss_alive_keeps_default);
    RUN_TEST(test_chpost13_cond2_boss_dead_game_over);
    RUN_TEST(test_chpost13_cond2_neighbor_slots_ignored);
    RUN_TEST(test_chpost13_both_conditions_game_over);
    RUN_TEST(test_chpost15_npc_alive_keeps_default);
    RUN_TEST(test_chpost15_npc_dead_game_over);
    RUN_TEST(test_chpost15_neighbor_slots_ignored);
    RUN_TEST(test_chpost16_npc_alive_keeps_default);
    RUN_TEST(test_chpost16_npc_dead_game_over);
    RUN_TEST(test_chpost16_neighbor_slots_ignored);
    printf("\n");
}
