/*
 * chpost.c — per-chapter turn-cycle post-action handlers
 *            (data_fd2_chapter_post_action_handler_table entries)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_chapter_02_post_action @ 0x206C5  (dispatched, 0 direct callers)
 *
 * Chapter 2「逆境之友」turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[1] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site pushes one cdecl
 * arg (active char_idx) and cleans it with ADD ESP,4; event_arg is
 * unused by the body.
 *
 * Runs the default win/lose check, then adds a game-over override that
 * fires ONLY when all 6 key NPCs at runtime_char[5..10] are dead: the
 * loop early-exits the instant any one of them is still alive, leaving
 * the default flag intact. game_event_flag (0x53ECC) is set to 1 (game
 * over) only after every slot 5..10 has flags bit0 set.
 * ---------------------------------------------------------------- */
void fd2_chapter_02_post_action(uint32 event_arg)
{
    int i;
    uint8 *pChar;

    (void)event_arg;

    fd2_check_battle_end_condition();

    for (i = 5; i <= 10; i++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        if ((pChar[5] & CHARFLAG_DEAD) == 0) {
            return;
        }
    }
    data_fd2_chapter_event_or_battle_end_code = 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_10_post_action @ 0x20707  (dispatched, 0 direct callers)
 *
 * Chapter 10 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[9] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site pushes one cdecl
 * arg (active char_idx) and cleans it; event_arg is unused by the body.
 *
 * Runs the default win/lose check, then adds a lose-condition override:
 * if either escort NPC at runtime_char[0x32] or [0x33] is dead, set
 * game_event_flag (0x53ECC) to 1 (game over). Unlike chapter 2 this
 * handler calls fd2_check_char_is_dead for each slot rather than reading
 * bFlags inline; OR short-circuits so [0x33] is only tested when [0x32]
 * is alive. (Chapter 10 has 2 escort NPCs at slots 0x32, 0x33 that must
 * survive.)
 * ---------------------------------------------------------------- */
void fd2_chapter_10_post_action(uint32 event_arg)
{
    (void)event_arg;

    fd2_check_battle_end_condition();

    if (fd2_check_char_is_dead(0x32) == 0 &&
        fd2_check_char_is_dead(0x33) == 0) {
        return;
    }
    data_fd2_chapter_event_or_battle_end_code = 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_12_post_action @ 0x2073D  (dispatched, 0 direct callers)
 *
 * Chapter 12 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[11] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site pushes one cdecl
 * arg (active char_idx) and cleans it; event_arg is unused by the body.
 *
 * Runs the default win/lose check, then adds a lose-condition override:
 * if the single key NPC at runtime_char[0xE] is dead, set game_event_flag
 * (0x53ECC) to 1 (game over). (Chapter 12 has one escort NPC at slot 0xE
 * that must survive.)
 * ---------------------------------------------------------------- */
void fd2_chapter_12_post_action(uint32 event_arg)
{
    (void)event_arg;

    fd2_check_battle_end_condition();

    if (fd2_check_char_is_dead(0xE) != 0) {
        data_fd2_chapter_event_or_battle_end_code = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_13_post_action @ 0x20765  (dispatched, 0 direct callers)
 *
 * Chapter 13 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[12] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site pushes one cdecl
 * arg (active char_idx) and cleans it; event_arg is unused by the body.
 *
 * The most complex non-default handler: it runs the default win/lose
 * check, then adds two independent lose conditions, each of which sets
 * game_event_flag (0x53ECC) to 1 AND plays a chapter dialog page:
 *
 *   1. If every one of the 12 NPC/enemy slots runtime_char[0xF..0x1A] is
 *      dead, show current_chapter_text page 10. The loop does NOT early
 *      exit; it sets a "some slot still alive" flag the instant any slot
 *      reports alive (fd2_check_char_is_dead == 0) and runs to completion,
 *      so the condition fires only when no slot in the range is alive.
 *   2. If the turn counter (0x53BEF) is greater than 5 AND the boss-ish
 *      NPC at runtime_char[0x3B] is dead, show current_chapter_text page 2.
 *
 * The dialog calls use the chapter's standard glyph geometry (render base
 * 0xA0000, pitch 0x140, glyph params 0xCD/0x4C/0x4A, height 0x13) with
 * blink_flag = 1.
 * ---------------------------------------------------------------- */
void fd2_chapter_13_post_action(uint32 event_arg)
{
    int i;
    int some_npc_alive;

    (void)event_arg;

    some_npc_alive = 0;

    fd2_check_battle_end_condition();

    for (i = 0; i < 0xC; i++) {
        if (fd2_check_char_is_dead(i + 0xF) == 0) {
            some_npc_alive = 1;
        }
    }

    if (some_npc_alive == 0) {
        data_fd2_chapter_event_or_battle_end_code = 1;
        fd2_display_dialog_scene(current_chapter_text, 10, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
    }

    if ((int32)data_fd2_battle_turn_counter > 5) {
        if (fd2_check_char_is_dead(0x3B) != 0) {
            data_fd2_chapter_event_or_battle_end_code = 1;
            fd2_display_dialog_scene(current_chapter_text, 2, 0xA0000, 0x140,
                                     0xCD, 0x4C, 0x4A, 0x13, 1);
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_15_post_action @ 0x20822  (dispatched, 0 direct callers)
 *
 * Chapter 15 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[14] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site pushes one cdecl
 * arg (active char_idx) and cleans it; event_arg is unused by the body.
 *
 * Runs the default win/lose check, then adds a lose-condition override:
 * if the single key NPC at runtime_char[0x40] is dead, set game_event_flag
 * (0x53ECC) to 1 (game over). (Chapter 15 has one escort NPC at slot 0x40
 * that must survive.)
 * ---------------------------------------------------------------- */
void fd2_chapter_15_post_action(uint32 event_arg)
{
    (void)event_arg;

    fd2_check_battle_end_condition();

    if (fd2_check_char_is_dead(0x40) != 0) {
        data_fd2_chapter_event_or_battle_end_code = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_16_post_action @ 0x2084A  (dispatched, 0 direct callers)
 *
 * Chapter 16 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[15] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site pushes one cdecl
 * arg (active char_idx) and cleans it; event_arg is unused by the body.
 *
 * Runs the default win/lose check, then adds a lose-condition override:
 * if the single key NPC at runtime_char[0x41] is dead, set game_event_flag
 * (0x53ECC) to 1 (game over). (Chapter 16 has one escort NPC at slot 0x41
 * that must survive.) Structurally identical to chapter 15 with slot 0x41
 * instead of 0x40.
 * ---------------------------------------------------------------- */
void fd2_chapter_16_post_action(uint32 event_arg)
{
    (void)event_arg;

    fd2_check_battle_end_condition();

    if (fd2_check_char_is_dead(0x41) != 0) {
        data_fd2_chapter_event_or_battle_end_code = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_17_post_action @ 0x20872  (dispatched, 0 direct callers)
 *
 * Chapter 17 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[16] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site pushes one cdecl
 * arg (active char_idx) and cleans it; event_arg is unused by the body.
 *
 * Runs the default win/lose check, then adds a compound lose condition:
 * if the party no longer contains the char with char_id 0x12 (蜜蒂,
 * tested against the template/snapshot roster via
 * fd2_check_party_has_char_id) AND the NPC at runtime_char[0x34] is dead,
 * set game_event_flag (0x53ECC) to 1 (game over) and show
 * current_chapter_text page 2. Both conditions must hold: the char-id
 * check short-circuits (when 蜜蒂 is still present the dead-check and
 * dialog are skipped entirely). The dialog call uses the chapter's
 * standard glyph geometry (render base 0xA0000, pitch 0x140, glyph params
 * 0xCD/0x4C/0x4A, height 0x13) with blink_flag = 1.
 * ---------------------------------------------------------------- */
void fd2_chapter_17_post_action(uint32 event_arg)
{
    (void)event_arg;

    fd2_check_battle_end_condition();

    if (fd2_check_party_has_char_id(0x12) == 0) {
        if (fd2_check_char_is_dead(0x34) != 0) {
            fd2_display_dialog_scene(current_chapter_text, 2, 0xA0000, 0x140,
                                     0xCD, 0x4C, 0x4A, 0x13, 1);
            data_fd2_chapter_event_or_battle_end_code = 1;
        }
    }
}
