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

/* testglob fake for the still-unemitted fd2_check_party_has_char_id: the
 * chapter-17 suite drives its return value and reads back the queried arg. */
extern uint32 g_has_char_fake;
extern uint32 g_has_char_last_arg;
extern int    g_has_char_calls;

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

/* ============================================================
 * fd2_chapter_17_post_action @ 0x20872
 *
 * Same default win/lose check (fd2_check_battle_end_condition, linked
 * real), then a COMPOUND lose condition gated by two different callees:
 *   gate 1: the party no longer contains the char with char_id 0x12 (蜜蒂),
 *           tested via fd2_check_party_has_char_id(0x12) == 0;
 *   gate 2: the NPC at runtime_char[0x34] is dead, via
 *           fd2_check_char_is_dead(0x34) != 0.
 * Both must hold; gate 1 short-circuits (when 蜜蒂 is still present the
 * dead-check and dialog are skipped). When both hold the handler sets
 * game_event_flag = 1 AND plays current_chapter_text page 2 through the
 * REAL fd2_display_dialog_scene.
 *
 * Unlike the other handlers in this file, the first gate is NOT a
 * runtime_char read: fd2_check_party_has_char_id is still unemitted and
 * resolves to the testglob fake whose return value is g_has_char_fake and
 * which records its argument in g_has_char_last_arg. These tests drive that
 * fake directly to pick each branch. The second gate uses the testglob
 * array-reading mode (g_check_char_is_dead_use_array = 1) so per-slot
 * .flags drive deadness; slot 0x34 (52) lies within the 64-slot t_rc13
 * buffer, which is reused here. As elsewhere every slot is team=2 / alive
 * so the default check yields flag=2 and the override is a clean 2 -> 1.
 *
 * The dialog write and the CALL sit in the same basic block (disassembly
 * has no branch between MOV [0x53ECC],1 and CALL 0x15F84), so the flag's
 * 2 -> 1 transition fully pins that the override block ran and the dialog
 * call follows. current_chapter_text is pointed at an immediate-END program
 * (the same fixture shape used by the chapter-13 suite) so the real VM
 * returns at once without touching the framebuffer or loading DATO.DAT; a
 * clean pass also confirms the real VM survives the chapter-17 call shape.
 *
 * Coverage is risk-driven for the two-callee AND, its short-circuit, the
 * exact char-id argument, and the exact dead-checked slot:
 *   - party HAS 0x12                         -> gate 1 false, no override
 *                                               (dead-check short-circuited)
 *   - party lacks 0x12, slot 0x34 alive      -> gate 2 false, no override
 *   - party lacks 0x12, slot 0x34 dead       -> both gates true, override
 *                                               fires (flag 2 -> 1, page 2)
 *                                               and arg to the party query
 *                                               is exactly 0x12
 *   - party lacks 0x12, neighbors 0x33/0x35 dead but 0x34 alive -> NO
 *     override, pinning the dead-checked slot as exactly 0x34.
 * ============================================================ */

/* Immediate-END dialog program for current_chapter_text (covers page 2). */
static uint16 t_ch17_text[0x400];

static void ch17_text_all_end(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        t_ch17_text[i] = 0;
    }
    *(int16 *)((uint8 *)t_ch17_text + 0x780) = -1;     /* END marker */
    for (i = 0; i < 0x3c0; i++) {
        t_ch17_text[i] = (uint16)0x780;                /* byte offset of END */
    }
    current_chapter_text = (uint32)t_ch17_text;
}

static void chpost17_setup(void)
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
    g_check_char_is_dead_use_array = 1;   /* per-slot .flags drive deadness */
    g_has_char_fake = 0;                  /* default: party lacks the char */
    g_has_char_last_arg = 0;
    g_has_char_calls = 0;
    ch17_text_all_end();
}

static void chpost17_teardown(void)
{
    g_check_char_is_dead_use_array = 0;   /* restore index-agnostic default */
    g_has_char_fake = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
}

/* Party still contains char 0x12 (蜜蒂 present) -> gate 1
 * (party_has_char_id == 0) is false, so the override is skipped and the
 * dead-check is short-circuited even though slot 0x34 is dead. The default
 * flag (2) survives. Pins gate-1 direction and the short-circuit. */
static void test_chpost17_party_has_char_keeps_default(void)
{
    chpost17_setup();
    g_has_char_fake = 1;                  /* party HAS char 0x12 */
    t_rc13[0x34].flags = CHARFLAG_DEAD;   /* would fire gate 2 if reached */

    fd2_chapter_17_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost17_teardown();
}

/* Party lacks char 0x12 but the key NPC at slot 0x34 is alive -> gate 1 is
 * true, gate 2 false, so the override does not fire. Pins gate-2 direction:
 * an ALIVE slot must NOT trigger game over. */
static void test_chpost17_char_absent_npc_alive_keeps_default(void)
{
    chpost17_setup();
    /* g_has_char_fake = 0 (absent) and slot 0x34 alive from setup */

    fd2_chapter_17_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost17_teardown();
}

/* Both gates true: party lacks char 0x12 AND slot 0x34 dead -> the override
 * fires (flag 2 -> 1) and page 2 plays via the real (immediate-END) dialog
 * VM. Also pins that the party query was made with char-id exactly 0x12. */
static void test_chpost17_char_absent_npc_dead_game_over(void)
{
    chpost17_setup();
    t_rc13[0x34].flags = CHARFLAG_DEAD;

    fd2_chapter_17_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    ASSERT_EQ(g_has_char_last_arg, 0x12);
    chpost17_teardown();
}

/* Neighbors 0x33 and 0x35 dead while the key NPC (0x34) is alive, party
 * lacks char 0x12 -> the override must NOT fire. Proves the dead-checked
 * slot is exactly 0x34 (no off-by-one in either direction). */
static void test_chpost17_neighbor_slots_ignored(void)
{
    chpost17_setup();
    t_rc13[0x33].flags = CHARFLAG_DEAD;
    t_rc13[0x35].flags = CHARFLAG_DEAD;

    fd2_chapter_17_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost17_teardown();
}

/* ============================================================
 * fd2_chapter_18_post_action @ 0x208CF
 *
 * UNLIKE every other handler in this file, chapter 18 does NOT call the
 * default fd2_check_battle_end_condition: it implements the full win/lose
 * decision itself with two sequential, independent flag writes:
 *   1. If any of the three protected chars runtime_char[0], [0x10] or [0x11]
 *      is dead -> game_event_flag = 1 (LOSE). The OR short-circuits.
 *   2. If the boss runtime_char[0x34] is dead -> game_event_flag = 2 (WIN).
 *      This runs unconditionally after step 1, so a dead boss OVERRIDES a
 *      LOSE from step 1 (fall-through "win-overrides-loss").
 *
 * Because no default check runs, the flag has no baseline value here: setup
 * pre-clears it to 0, so "no condition" leaves 0, a pure LOSE leaves 1, and
 * any WIN leaves 2. Deadness for all four slots is queried through
 * fd2_check_char_is_dead; these tests use the testglob array-reading mode
 * (g_check_char_is_dead_use_array = 1) so per-slot .flags drive each result.
 * Slot 0x34 (52) is reached, so the 64-slot t_rc13 buffer is reused.
 *
 * Coverage is risk-driven for: the three-term short-circuit OR and its exact
 * slot indices, the independent boss write, and the win-overrides-loss
 * fall-through:
 *   - nobody dead                         -> flag stays 0 (neither write)
 *   - char[0] dead, boss alive            -> LOSE (1); pins first OR term
 *   - char[0x10] dead, boss alive         -> LOSE (1); pins second OR term
 *                                            (reached only if char[0] alive)
 *   - char[0x11] dead, boss alive         -> LOSE (1); pins third OR term
 *   - boss dead, all protected alive      -> WIN (2)
 *   - boss dead AND char[0] dead          -> WIN (2) overrides the LOSE
 *   - neighbors 1/0xF/0x12/0x33/0x35 dead while the four checked slots are
 *     alive -> flag stays 0, pinning the checked slots as exactly 0, 0x10,
 *     0x11 and 0x34 (no off-by-one on any of the four).
 * ============================================================ */

static void chpost18_setup(void)
{
    int i;

    memset(t_rc13, 0, sizeof(t_rc13));
    for (i = 0; i < CH13_RC_SLOTS; i++) {
        t_rc13[i].team = 2;     /* player team (irrelevant: no default check) */
        t_rc13[i].flags = 0;    /* alive */
    }
    data_fd2_battle_runtime_char_array_ptr = t_rc13;
    data_fd2_battle_party_member_count = CH13_RC_SLOTS;
    data_fd2_chapter_event_or_battle_end_code = 0;  /* no default check sets a baseline */
    g_check_char_is_dead_use_array = 1;   /* per-slot .flags drive deadness */
}

static void chpost18_teardown(void)
{
    g_check_char_is_dead_use_array = 0;   /* restore index-agnostic default */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
}

/* Nobody dead -> the OR is false (no LOSE) and the boss is alive (no WIN), so
 * neither flag write executes and the pre-cleared flag (0) survives. Confirms
 * chapter 18 writes nothing on the all-alive path (i.e. it really skips the
 * default win/lose check that every sibling runs). */
static void test_chpost18_nobody_dead_no_write(void)
{
    chpost18_setup();

    fd2_chapter_18_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 0);
    chpost18_teardown();
}

/* Protected char[0] dead, boss alive -> the first OR term fires, LOSE (flag
 * = 1). Pins the first checked slot = 0 and the OR's set-on-dead direction. */
static void test_chpost18_char0_dead_lose(void)
{
    chpost18_setup();
    t_rc13[0].flags = CHARFLAG_DEAD;

    fd2_chapter_18_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost18_teardown();
}

/* Protected char[0x10] dead while char[0] alive, boss alive -> the OR's first
 * term is false so the second term must be evaluated; it fires, LOSE (1).
 * Pins the second checked slot = 0x10 and that it is genuinely reached. */
static void test_chpost18_char10_dead_lose(void)
{
    chpost18_setup();
    t_rc13[0x10].flags = CHARFLAG_DEAD;

    fd2_chapter_18_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost18_teardown();
}

/* Protected char[0x11] dead while char[0] and char[0x10] alive, boss alive ->
 * the first two OR terms are false so the third must be evaluated; it fires,
 * LOSE (1). Pins the third checked slot = 0x11 and that it is genuinely
 * reached (it is the term whose branch shape is inverted in the disassembly:
 * JZ-skips-the-set when alive). */
static void test_chpost18_char11_dead_lose(void)
{
    chpost18_setup();
    t_rc13[0x11].flags = CHARFLAG_DEAD;

    fd2_chapter_18_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost18_teardown();
}

/* Boss (slot 0x34) dead, all three protected chars alive -> the LOSE OR is
 * false but the independent boss write fires, WIN (flag = 2). Pins the boss
 * slot = 0x34 and the WIN write. */
static void test_chpost18_boss_dead_win(void)
{
    chpost18_setup();
    t_rc13[0x34].flags = CHARFLAG_DEAD;

    fd2_chapter_18_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost18_teardown();
}

/* Boss dead AND protected char[0] dead -> step 1 sets LOSE (1) but step 2 runs
 * unconditionally afterward and overwrites it with WIN (2). Pins the
 * win-overrides-loss fall-through: the boss write is sequenced AFTER the LOSE
 * write and is not gated by it. */
static void test_chpost18_boss_and_ally_dead_win_overrides(void)
{
    chpost18_setup();
    t_rc13[0].flags = CHARFLAG_DEAD;
    t_rc13[0x34].flags = CHARFLAG_DEAD;

    fd2_chapter_18_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost18_teardown();
}

/* Neighbors of every checked slot dead (1, 0xF, 0x12, 0x33, 0x35) while the
 * four checked slots (0, 0x10, 0x11, 0x34) are alive -> neither write fires
 * and the flag stays 0. Proves the checked slots are EXACTLY 0, 0x10, 0x11 and
 * 0x34 with no off-by-one in either direction on any of them. */
static void test_chpost18_neighbor_slots_ignored(void)
{
    chpost18_setup();
    t_rc13[1].flags = CHARFLAG_DEAD;     /* neighbor of 0 */
    t_rc13[0xF].flags = CHARFLAG_DEAD;   /* neighbor below 0x10 */
    t_rc13[0x12].flags = CHARFLAG_DEAD;  /* neighbor above 0x11 */
    t_rc13[0x33].flags = CHARFLAG_DEAD;  /* neighbor below 0x34 */
    t_rc13[0x35].flags = CHARFLAG_DEAD;  /* neighbor above 0x34 */

    fd2_chapter_18_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 0);
    chpost18_teardown();
}

/* ============================================================
 * fd2_chapter_19_post_action @ 0x20926
 *
 * Same default win/lose check (fd2_check_battle_end_condition, linked
 * real), then a turn-GATED single-slot lose-condition override: only when
 * the turn counter (0x53BEF) is strictly greater than 6 (turn 7+) AND key
 * NPC runtime_char[0x40] is dead does game_event_flag become 1. This is
 * chapter 15's single-slot 0x40 check wrapped in a strict `turn > 6` gate
 * (same comparator shape as chapter 13 condition 2). Deadness is queried
 * through fd2_check_char_is_dead (runtime_char[idx].flags bit0).
 *
 * Reuses the 72-slot t_rc15 buffer (slot 0x40 = 64 is in range) and the
 * testglob array-reading mode so per-slot .flags drive the dead-check, with
 * the all-team-2 / alive arrangement pinning the default check to flag=2;
 * the override is then observable as a clean 2 -> 1.
 *
 * Coverage is risk-driven for the two stacked, easy-to-read-backwards
 * branches — the strict `JLE`-skip turn gate and the inner `JZ`-skip
 * dead-check — and their conjunction:
 *   - turn 6 (== 6, not > 6), slot 0x40 dead -> gate blocks (flag stays 2),
 *     pinning the strict comparator (boundary value 6 must not trigger).
 *   - turn 7 (> 6), slot 0x40 alive          -> dead-check fails (flag 2).
 *   - turn 7 (> 6), slot 0x40 dead           -> override fires (flag -> 1).
 *   - turn 0, slot 0x40 dead                  -> gate blocks (flag 2), the
 *     pre-turn-7 unprotected window.
 *   - turn 7, neighbors 0x3F/0x41 dead, 0x40 alive -> NO override, pinning
 *     the checked slot as exactly 0x40 (no off-by-one in either direction).
 * ============================================================ */

static void chpost19_setup(void)
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
    data_fd2_battle_turn_counter = 0;
    g_check_char_is_dead_use_array = 1;   /* per-slot .flags drive deadness */
}

static void chpost19_teardown(void)
{
    g_check_char_is_dead_use_array = 0;   /* restore index-agnostic default */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_turn_counter = 0;
}

/* Turn == 6 (NOT > 6) with slot 0x40 dead -> the turn gate blocks the
 * override and the default flag (2) survives. Pins the strict `turn > 6`
 * comparator: the boundary value 6 must not trigger. */
static void test_chpost19_turn_eq_6_keeps_default(void)
{
    chpost19_setup();
    data_fd2_battle_turn_counter = 6;
    t_rc15[0x40].flags = CHARFLAG_DEAD;

    fd2_chapter_19_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost19_teardown();
}

/* Turn == 7 (> 6) but slot 0x40 alive -> the gate opens yet the dead-check
 * fails, so no override. Pins that deadness of 0x40 is required, not just
 * the turn gate, and the dead-check branch direction (ALIVE must NOT set). */
static void test_chpost19_turn_gt_6_npc_alive_keeps_default(void)
{
    chpost19_setup();
    data_fd2_battle_turn_counter = 7;
    /* slot 0x40 already alive from setup */

    fd2_chapter_19_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost19_teardown();
}

/* Turn == 7 (> 6) AND slot 0x40 dead -> both conditions hold, the override
 * fires (flag 2 -> 1). The canonical game-over path. */
static void test_chpost19_turn_gt_6_npc_dead_game_over(void)
{
    chpost19_setup();
    data_fd2_battle_turn_counter = 7;
    t_rc15[0x40].flags = CHARFLAG_DEAD;

    fd2_chapter_19_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost19_teardown();
}

/* Turn == 0 (the pre-turn-7 unprotected window) with slot 0x40 dead -> the
 * gate blocks the override, flag stays 2. Confirms char[0x40] is deliberately
 * unprotected before turn 7. */
static void test_chpost19_turn_zero_npc_dead_keeps_default(void)
{
    chpost19_setup();
    /* turn counter already 0 from setup */
    t_rc15[0x40].flags = CHARFLAG_DEAD;

    fd2_chapter_19_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost19_teardown();
}

/* Turn == 7 (> 6) with neighbors 0x3F and 0x41 dead while the key NPC (0x40)
 * is alive -> the override must NOT fire. Proves the checked slot is exactly
 * 0x40 (no off-by-one in either direction). */
static void test_chpost19_neighbor_slots_ignored(void)
{
    chpost19_setup();
    data_fd2_battle_turn_counter = 7;
    t_rc15[0x3F].flags = CHARFLAG_DEAD;
    t_rc15[0x41].flags = CHARFLAG_DEAD;

    fd2_chapter_19_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost19_teardown();
}

/* ============================================================
 * fd2_chapter_20_post_action @ 0x20957
 *
 * The largest non-default handler. Same default win/lose check
 * (fd2_check_battle_end_condition, linked real), then THREE independent
 * stages layered on top, each writing game_event_flag (0x53ECC):
 *
 *   Stage 1 — NPC group extinction (LOSE + dialog). Full-scan (no early
 *     exit) of the 8-slot NPC group runtime_char[0x35..0x3C] (loop i in
 *     0x26..0x2D, slot i + 0xF): if every slot is dead, set flag = 1 and
 *     play current_chapter_text page 10 through the REAL
 *     fd2_display_dialog_scene. Flag is written before the dialog call,
 *     both in the same basic block.
 *   Stage 2 — key-char extinction (LOSE). If hero runtime_char[0] OR
 *     boss-ally runtime_char[0x34] is dead, set flag = 1. Short-circuit OR
 *     ([0x34] only tested when [0] alive).
 *   Stage 3 — enemy wipe (WIN). Full-scan of two enemy ranges as a union,
 *     runtime_char[0x24..0x33] (loop i in 0x15..0x24) and
 *     runtime_char[0x3D..0x52] (loop i in 0x2E..0x43): if NO slot in either
 *     range is alive, set flag = 2 (WIN). Stage 3 runs last, so a full
 *     enemy wipe OVERRIDES a LOSE set by stage 1 or 2 (win-overrides-loss).
 *
 * The three protected/enemy regions are disjoint: stage-1 NPCs [0x35,0x3C]
 * and the stage-2 boss-ally [0x34] sit in the gap BETWEEN the two stage-3
 * enemy ranges ([0x24,0x33] and [0x3D,0x52]), so each region can be toggled
 * independently. Deadness for every slot is queried through
 * fd2_check_char_is_dead; these tests use the testglob array-reading mode
 * (g_check_char_is_dead_use_array = 1) so per-slot .flags drive each result.
 * The highest slot reached is 0x52 (82), so an 88-slot local buffer is used.
 *
 * As elsewhere every slot is team=2 / alive at setup so the default check
 * yields flag=2; stage writes are then observable against that baseline.
 * current_chapter_text points at an immediate-END program (same fixture
 * shape as the chapter-13 suite) so the real dialog VM returns at once
 * without touching the framebuffer or loading DATO.DAT; a clean pass also
 * confirms the real VM survives the chapter-20 page-10 call shape.
 *
 * Coverage is risk-driven for the three full-scan loops and their exact
 * bounds, the stage-2 short-circuit OR, and the stage-3 union + the
 * win-overrides-loss ordering:
 *   - nothing triggered (all alive)                 -> flag stays 2
 *   Stage 1 (full-scan over [0x35,0x3C]):
 *   - all 8 NPCs dead, keys/enemies alive           -> stage 1 fires (1, page 10)
 *   - first NPC 0x35 alive (rest dead)              -> stage 1 does NOT fire
 *   - last NPC 0x3C alive (rest dead)               -> stage 1 does NOT fire
 *   - neighbors 0x34/0x3D dead, [0x35,0x3C] dead    -> stage 1 fires (proves
 *       the scanned range excludes both the boss-ally and the enemy range,
 *       i.e. exactly [0x35,0x3C]); slot 0x34's death here is stage 2's, not
 *       stage 1's, so the flag is still 1 either way -> see dedicated stage-2
 *       neighbor test below for the off-by-one proof
 *   Stage 2 (short-circuit OR of slot 0, slot 0x34):
 *   - hero 0 dead, 0x34 alive                       -> stage 2 fires (1)
 *   - hero 0 alive, 0x34 dead                       -> stage 2 fires via the
 *       second term (1), proving 0x34 is genuinely evaluated
 *   - neighbors 0x33/0x35 dead, 0 and 0x34 alive    -> stage 2 does NOT fire
 *       (also keeps stage 1 inert: 0x35 alive), pinning the OR's second slot
 *       as exactly 0x34
 *   Stage 3 (union [0x24,0x33] U [0x3D,0x52], WIN, runs last):
 *   - all enemies in BOTH ranges dead, hero 0 dead  -> stage 3 fires and
 *       overrides the stage-2 LOSE: final flag = 2 (win-overrides-loss)
 *   - range A all dead but one range-B slot alive   -> stage 3 does NOT fire
 *       (union requires BOTH ranges empty); with hero 0 dead the flag is 1
 *   - first range-A slot 0x24 alive (rest of both dead) -> stage 3 does NOT
 *       fire, pinning range-A lower bound
 *   - last range-B slot 0x52 alive (rest of both dead)  -> stage 3 does NOT
 *       fire, pinning range-B upper bound
 *   - neighbors 0x23 (below A) and 0x53 (above B) alive while both ranges
 *       dead -> stage 3 STILL fires, pinning the union as exactly
 *       [0x24,0x33] U [0x3D,0x52]
 * ============================================================ */

#define CH20_RC_SLOTS 88
static runtime_char t_rc20[CH20_RC_SLOTS];

/* Immediate-END dialog program for current_chapter_text (covers page 10). */
static uint16 t_ch20_text[0x400];

static void ch20_text_all_end(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        t_ch20_text[i] = 0;
    }
    *(int16 *)((uint8 *)t_ch20_text + 0x780) = -1;     /* END marker */
    for (i = 0; i < 0x3c0; i++) {
        t_ch20_text[i] = (uint16)0x780;                /* byte offset of END */
    }
    current_chapter_text = (uint32)t_ch20_text;
}

static void chpost20_setup(void)
{
    int i;

    memset(t_rc20, 0, sizeof(t_rc20));
    for (i = 0; i < CH20_RC_SLOTS; i++) {
        t_rc20[i].team = 2;     /* player team: never an alive enemy */
        t_rc20[i].flags = 0;    /* alive */
    }
    data_fd2_battle_runtime_char_array_ptr = t_rc20;
    data_fd2_battle_party_member_count = CH20_RC_SLOTS;
    data_fd2_chapter_event_or_battle_end_code = 0;
    g_check_char_is_dead_use_array = 1;   /* per-slot .flags drive deadness */
    ch20_text_all_end();
}

static void chpost20_teardown(void)
{
    g_check_char_is_dead_use_array = 0;   /* restore index-agnostic default */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
}

/* mark the stage-1 NPC group [0x35,0x3C] (8 slots) dead */
static void ch20_kill_npc_group(void)
{
    int i;

    for (i = 0x35; i <= 0x3C; i++) {
        t_rc20[i].flags = CHARFLAG_DEAD;
    }
}

/* mark both stage-3 enemy ranges [0x24,0x33] and [0x3D,0x52] dead */
static void ch20_kill_all_enemies(void)
{
    int i;

    for (i = 0x24; i <= 0x33; i++) {
        t_rc20[i].flags = CHARFLAG_DEAD;
    }
    for (i = 0x3D; i <= 0x52; i++) {
        t_rc20[i].flags = CHARFLAG_DEAD;
    }
}

/* Everyone alive -> no stage fires. The default check yields flag=2 and no
 * override touches it. Confirms the all-alive path is a clean victory and
 * none of the three stages writes spuriously. */
static void test_chpost20_nothing_triggered_keeps_default(void)
{
    chpost20_setup();

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost20_teardown();
}

/* Stage 1: all 8 NPCs [0x35,0x3C] dead while hero/boss-ally and the enemy
 * ranges stay alive -> the full-scan finds none alive, sets flag = 1 and
 * plays page 10 via the real (immediate-END) dialog VM. Stage 2 is inert
 * (slot 0 and 0x34 alive) and stage 3 is inert (enemies alive), so the
 * observed 2 -> 1 is stage 1's alone. */
static void test_chpost20_stage1_all_npc_dead_game_over(void)
{
    chpost20_setup();
    ch20_kill_npc_group();

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost20_teardown();
}

/* Stage 1 first slot (0x35) alive, 0x36..0x3C dead -> the loop sets the
 * "some alive" flag on iteration 0 and (no early exit) still completes,
 * leaving stage 1 false. Pins the loop start index = 0x35 (i = 0x26). */
static void test_chpost20_stage1_first_slot_alive_keeps_default(void)
{
    chpost20_setup();
    ch20_kill_npc_group();
    t_rc20[0x35].flags = 0;         /* slot 0x35 alive */

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost20_teardown();
}

/* Stage 1 last slot (0x3C) alive, 0x35..0x3B dead -> the loop survives 7 dead
 * slots and only sees the survivor at its final iteration, so stage 1 does NOT
 * fire. Pins the inclusive upper bound = 0x3C (i < 0x2E). */
static void test_chpost20_stage1_last_slot_alive_keeps_default(void)
{
    chpost20_setup();
    ch20_kill_npc_group();
    t_rc20[0x3C].flags = 0;         /* slot 0x3C alive */

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost20_teardown();
}

/* Stage 2 hero path: slot 0 dead, boss-ally 0x34 alive, NPCs and enemies
 * alive -> stage 1 inert, stage 2's first OR term fires (flag = 1), stage 3
 * inert. (Slot 0 dead also makes the default check set 1, but stage 2 would
 * set it regardless; the post-state is unambiguously 1.) Pins stage 2's first
 * slot = 0 and the set-on-dead direction. */
static void test_chpost20_stage2_hero_dead_game_over(void)
{
    chpost20_setup();
    t_rc20[0].flags = CHARFLAG_DEAD;

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost20_teardown();
}

/* Stage 2 boss-ally path: slot 0 alive, slot 0x34 dead, NPCs and enemies
 * alive -> the OR's first term is false so the second must be evaluated; it
 * fires (flag = 1). Default check stays 2 (slot 0 alive), so the clean 2 -> 1
 * proves slot 0x34 is genuinely evaluated, not dead code. */
static void test_chpost20_stage2_boss_ally_dead_game_over(void)
{
    chpost20_setup();
    t_rc20[0x34].flags = CHARFLAG_DEAD;

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost20_teardown();
}

/* Neighbors 0x33 and 0x35 dead while the stage-2 slots (0, 0x34) are alive
 * -> stage 2 must NOT fire. 0x35 (alive's neighbor) being dead also leaves a
 * survivor in the NPC group so stage 1 stays false, and a single dead slot in
 * range A keeps stage 3 false. The flag stays 2, pinning stage 2's second
 * slot as exactly 0x34 (no off-by-one in either direction). */
static void test_chpost20_stage2_neighbor_slots_ignored(void)
{
    chpost20_setup();
    t_rc20[0x33].flags = CHARFLAG_DEAD;   /* below 0x34 (also a stage-3 enemy) */
    t_rc20[0x35].flags = CHARFLAG_DEAD;   /* above 0x34 (also a stage-1 NPC) */

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost20_teardown();
}

/* Stage 3 WIN, win-overrides-loss: every enemy in BOTH ranges dead AND hero 0
 * dead. Stage 2 (and the default check) set flag = 1, then stage 3 runs last,
 * finds no enemy alive, and overrides to flag = 2. Pins that stage 3 is
 * sequenced after stages 1-2 and that a full enemy wipe wins even with the
 * hero down. */
static void test_chpost20_stage3_enemy_wipe_win_overrides_lose(void)
{
    chpost20_setup();
    ch20_kill_all_enemies();
    t_rc20[0].flags = CHARFLAG_DEAD;      /* force a stage-2/default LOSE first */

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost20_teardown();
}

/* Stage 3 union requires BOTH ranges empty: range A [0x24,0x33] fully dead but
 * one range-B slot (0x4A) left alive, hero 0 dead -> stage 3 does NOT fire and
 * the stage-2 LOSE survives (flag = 1). Proves the second loop's survivor
 * still counts (the two ranges are OR-combined into one "any alive"). */
static void test_chpost20_stage3_rangeB_survivor_blocks_win(void)
{
    chpost20_setup();
    ch20_kill_all_enemies();
    t_rc20[0x4A].flags = 0;               /* one range-B enemy alive */
    t_rc20[0].flags = CHARFLAG_DEAD;      /* stage-2 LOSE baseline */

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost20_teardown();
}

/* Stage 3 range-A lower bound: first range-A slot (0x24) alive, the rest of
 * both ranges dead, hero 0 dead -> stage 3 does NOT fire (a survivor at the
 * very start of range A blocks the win), flag = 1. Pins range-A start = 0x24
 * (i = 0x15). */
static void test_chpost20_stage3_rangeA_first_slot_alive_blocks_win(void)
{
    chpost20_setup();
    ch20_kill_all_enemies();
    t_rc20[0x24].flags = 0;               /* first range-A enemy alive */
    t_rc20[0].flags = CHARFLAG_DEAD;

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost20_teardown();
}

/* Stage 3 range-B upper bound: last range-B slot (0x52) alive, the rest of
 * both ranges dead, hero 0 dead -> stage 3 does NOT fire, flag = 1. Pins
 * range-B end = 0x52 (i < 0x44, last i = 0x43, slot 0x43 + 0xF = 0x52). */
static void test_chpost20_stage3_rangeB_last_slot_alive_blocks_win(void)
{
    chpost20_setup();
    ch20_kill_all_enemies();
    t_rc20[0x52].flags = 0;               /* last range-B enemy alive */
    t_rc20[0].flags = CHARFLAG_DEAD;

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost20_teardown();
}

/* Stage 3 boundary proof from the outside: neighbors just below range A (0x23)
 * and just above range B (0x53) left alive while BOTH ranges are fully dead,
 * hero 0 dead -> stage 3 STILL fires and overrides to WIN (flag = 2). Proves
 * the scanned union is exactly [0x24,0x33] U [0x3D,0x52]: survivors outside
 * the union do not block the win. (0x34..0x3C, the gap between the ranges, are
 * left dead here too but are never scanned by stage 3.) */
static void test_chpost20_stage3_outside_neighbors_ignored(void)
{
    chpost20_setup();
    ch20_kill_all_enemies();
    /* also kill the gap [0x34,0x3C] so only 0x23 and 0x53 are alive near the
     * union; none of these is in a stage-3 range. */
    {
        int i;
        for (i = 0x34; i <= 0x3C; i++) {
            t_rc20[i].flags = CHARFLAG_DEAD;
        }
    }
    t_rc20[0x23].flags = 0;               /* below range A, alive */
    t_rc20[0x53].flags = 0;               /* above range B, alive */
    t_rc20[0].flags = CHARFLAG_DEAD;      /* stage-2 LOSE baseline */

    fd2_chapter_20_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost20_teardown();
}

/* ============================================================
 * fd2_chapter_21_post_action @ 0x20A51
 *
 * Same default win/lose check (fd2_check_battle_end_condition, linked real),
 * then a two-slot lose-condition override: if either escort NPC
 * runtime_char[0x10] OR [0x11] is dead -> game_event_flag = 1 (game over).
 * The OR short-circuits ([0x11] is only tested when [0x10] is alive).
 *
 * Because the default check runs first, the flag's baseline is 2 here, not 0:
 * setup arranges every slot team=2 (player) and slot 0 (protagonist) alive so
 * the default deterministically yields flag=2, making the override observable
 * as a clean 2 -> 1 transition. Deadness for slots 0x10/0x11 is queried through
 * fd2_check_char_is_dead in the testglob array-reading mode
 * (g_check_char_is_dead_use_array = 1) so per-slot .flags drive each result;
 * the 64-slot t_rc13 buffer is reused (slot 0x11 is well within range).
 *
 * Coverage is risk-driven for: the two-term short-circuit OR, its exact slot
 * indices, the override's set-on-dead branch direction, and that the default
 * baseline is preserved when the override does not fire:
 *   - both escorts alive                  -> default flag (2) survives
 *   - escort[0x10] dead, [0x11] alive     -> LOSE (1); pins first OR term and
 *                                            set-on-dead direction
 *   - escort[0x11] dead while [0x10] alive-> LOSE (1); pins second OR term is
 *                                            genuinely reached (short-circuit)
 *   - both escorts dead                   -> LOSE (1)
 *   - neighbors 0xF/0x12 dead while 0x10/0x11 alive -> flag stays 2, pinning
 *     the checked slots as EXACTLY 0x10 and 0x11 (no off-by-one either way)
 * ============================================================ */

static void chpost21_setup(void)
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
    g_check_char_is_dead_use_array = 1;   /* per-slot .flags drive deadness */
}

static void chpost21_teardown(void)
{
    g_check_char_is_dead_use_array = 0;   /* restore index-agnostic default */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
}

/* Both escort NPCs alive -> the override OR is false, so the default check's
 * flag (2) survives. Confirms the handler writes nothing on the all-alive
 * path and that the default win/lose check really runs (flag is 2, not 0). */
static void test_chpost21_both_escorts_alive_keeps_default(void)
{
    chpost21_setup();

    fd2_chapter_21_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost21_teardown();
}

/* Escort[0x10] dead, [0x11] alive -> the first OR term fires, LOSE (flag = 1).
 * Pins the first checked slot = 0x10 and the override's set-on-dead direction
 * (the disassembly's JNZ-to-set on the first dead-check). */
static void test_chpost21_first_escort_dead_game_over(void)
{
    chpost21_setup();
    t_rc13[0x10].flags = CHARFLAG_DEAD;

    fd2_chapter_21_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost21_teardown();
}

/* Escort[0x11] dead while [0x10] alive -> the first OR term is false so the
 * second must be evaluated; it fires, LOSE (1). Pins the second checked slot
 * = 0x11 and that it is genuinely reached (the JZ-skips-set-when-alive term). */
static void test_chpost21_second_escort_dead_game_over(void)
{
    chpost21_setup();
    t_rc13[0x11].flags = CHARFLAG_DEAD;

    fd2_chapter_21_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost21_teardown();
}

/* Both escorts dead -> override fires, LOSE (1). */
static void test_chpost21_both_escorts_dead_game_over(void)
{
    chpost21_setup();
    t_rc13[0x10].flags = CHARFLAG_DEAD;
    t_rc13[0x11].flags = CHARFLAG_DEAD;

    fd2_chapter_21_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost21_teardown();
}

/* Neighbors just below (0xF) and just above (0x12) the checked pair dead while
 * escorts 0x10/0x11 are alive -> the override does NOT fire and the default
 * flag (2) survives. Proves the checked slots are EXACTLY 0x10 and 0x11 with no
 * off-by-one in either direction. */
static void test_chpost21_neighbor_slots_ignored(void)
{
    chpost21_setup();
    t_rc13[0xF].flags = CHARFLAG_DEAD;   /* neighbor below 0x10 */
    t_rc13[0x12].flags = CHARFLAG_DEAD;  /* neighbor above 0x11 */

    fd2_chapter_21_post_action(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost21_teardown();
}

/* ============================================================
 * fd2_chapter_22_27_28_post_action_shared @ 0x20A87
 *
 * Shared handler for chapters 22, 27 and 28 (table slots [21]/[26]/[27]
 * all point here). Same default win/lose check (fd2_check_battle_end_condition,
 * linked real), then a single-slot lose-condition override: if the must-protect
 * NPC runtime_char[1] is dead, set game_event_flag = 1. Deadness is queried
 * through fd2_check_char_is_dead, the real engine computing it as
 * runtime_char[idx].flags bit0.
 *
 * Structurally identical to chapters 12/15/16 but for slot 1. Reuses the
 * chapter-10/12 array-reading mode (g_check_char_is_dead_use_array = 1) so
 * per-slot .flags drive the result, and the same 56-slot t_rc10 buffer /
 * chpost10_setup arrangement that pins the default check to flag=2; the
 * override is then observable as a clean 2 -> 1.
 *
 * Coverage is risk-driven for the inverted-looking branch (the disassembly is
 * "JZ skip-set / fall through to set", i.e. set-the-flag-when-DEAD; it is
 * exactly the kind of test that is easy to read backwards) and the single
 * checked slot index. Note slot 0 is the protagonist, whose death the real
 * default check itself reports as game over, so it cannot be used as a clean
 * lower-neighbor probe; instead the slot-1-dead case (with slot 0 left alive,
 * yielding a clean 2 -> 1) already rules out "checks slot 0 instead of 1", and
 * a dedicated upper-neighbor case rules out "checks slot 2":
 *   - slot 1 alive            -> no override (flag stays 2)
 *   - slot 1 dead, slot 0 alive-> override fires (flag -> 1); also pins that the
 *     checked slot is 1, not 0 (slot 0 alive leaves the default at 2)
 *   - neighbor 2 dead, slot 1 alive -> NO override, pinning the checked slot as
 *     exactly 1 on the upper side (no off-by-one to slot 2).
 * ============================================================ */

/* Key NPC slot 1 alive -> the dead-check returns 0, the override does not fire,
 * and the default flag (2) survives. Pins the branch direction: an ALIVE slot
 * must NOT trigger game over. */
static void test_chpost222728_npc_alive_keeps_default(void)
{
    chpost10_setup();
    /* slot 1 already alive from setup */

    fd2_chapter_22_27_28_post_action_shared(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
    chpost10_teardown();
}

/* Key NPC slot 1 dead (slot 0 / protagonist left alive so the default check
 * still yields 2) -> fd2_check_char_is_dead returns nonzero and the override
 * fires (flag 2 -> 1). The clean 2 -> 1 also proves the checked slot is 1, not
 * the protagonist at slot 0. */
static void test_chpost222728_npc_dead_game_over(void)
{
    chpost10_setup();
    t_rc10[1].flags = CHARFLAG_DEAD;

    fd2_chapter_22_27_28_post_action_shared(0);

    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
    chpost10_teardown();
}

/* Upper neighbor (slot 2) dead while the key NPC (slot 1) is alive -> the
 * override must NOT fire. Proves the checked slot is exactly 1 on the upper
 * side (no off-by-one to slot 2). The lower side is pinned by the slot-1-dead
 * case above (slot 0 alive there). */
static void test_chpost222728_upper_neighbor_ignored(void)
{
    chpost10_setup();
    t_rc10[2].flags = CHARFLAG_DEAD;

    fd2_chapter_22_27_28_post_action_shared(0);

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
    RUN_TEST(test_chpost17_party_has_char_keeps_default);
    RUN_TEST(test_chpost17_char_absent_npc_alive_keeps_default);
    RUN_TEST(test_chpost17_char_absent_npc_dead_game_over);
    RUN_TEST(test_chpost17_neighbor_slots_ignored);
    RUN_TEST(test_chpost18_nobody_dead_no_write);
    RUN_TEST(test_chpost18_char0_dead_lose);
    RUN_TEST(test_chpost18_char10_dead_lose);
    RUN_TEST(test_chpost18_char11_dead_lose);
    RUN_TEST(test_chpost18_boss_dead_win);
    RUN_TEST(test_chpost18_boss_and_ally_dead_win_overrides);
    RUN_TEST(test_chpost18_neighbor_slots_ignored);
    RUN_TEST(test_chpost19_turn_eq_6_keeps_default);
    RUN_TEST(test_chpost19_turn_gt_6_npc_alive_keeps_default);
    RUN_TEST(test_chpost19_turn_gt_6_npc_dead_game_over);
    RUN_TEST(test_chpost19_turn_zero_npc_dead_keeps_default);
    RUN_TEST(test_chpost19_neighbor_slots_ignored);
    RUN_TEST(test_chpost20_nothing_triggered_keeps_default);
    RUN_TEST(test_chpost20_stage1_all_npc_dead_game_over);
    RUN_TEST(test_chpost20_stage1_first_slot_alive_keeps_default);
    RUN_TEST(test_chpost20_stage1_last_slot_alive_keeps_default);
    RUN_TEST(test_chpost20_stage2_hero_dead_game_over);
    RUN_TEST(test_chpost20_stage2_boss_ally_dead_game_over);
    RUN_TEST(test_chpost20_stage2_neighbor_slots_ignored);
    RUN_TEST(test_chpost20_stage3_enemy_wipe_win_overrides_lose);
    RUN_TEST(test_chpost20_stage3_rangeB_survivor_blocks_win);
    RUN_TEST(test_chpost20_stage3_rangeA_first_slot_alive_blocks_win);
    RUN_TEST(test_chpost20_stage3_rangeB_last_slot_alive_blocks_win);
    RUN_TEST(test_chpost20_stage3_outside_neighbors_ignored);
    RUN_TEST(test_chpost21_both_escorts_alive_keeps_default);
    RUN_TEST(test_chpost21_first_escort_dead_game_over);
    RUN_TEST(test_chpost21_second_escort_dead_game_over);
    RUN_TEST(test_chpost21_both_escorts_dead_game_over);
    RUN_TEST(test_chpost21_neighbor_slots_ignored);
    RUN_TEST(test_chpost222728_npc_alive_keeps_default);
    RUN_TEST(test_chpost222728_npc_dead_game_over);
    RUN_TEST(test_chpost222728_upper_neighbor_ignored);
    printf("\n");
}
