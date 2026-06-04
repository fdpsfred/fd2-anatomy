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

/* ----------------------------------------------------------------
 * fd2_chapter_18_post_action @ 0x208CF  (dispatched, 0 direct callers)
 *
 * Chapter 18 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[17] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site pushes one cdecl arg
 * (active char_idx) and cleans it; event_arg is unused by the body.
 *
 * Unlike every other handler in this file, chapter 18 does NOT call the
 * default fd2_check_battle_end_condition — it implements the full win/lose
 * decision itself with two sequential, independent flag writes:
 *
 *   1. If any of the three protected chars runtime_char[0], [0x10] or [0x11]
 *      is dead, set game_event_flag (0x53ECC) to 1 (LOSE). The OR short-
 *      circuits: the first dead char sets the flag and the remaining checks
 *      are skipped.
 *   2. If the boss NPC runtime_char[0x34] is dead, set game_event_flag to 2
 *      (WIN). This runs unconditionally after step 1, so a dead boss
 *      overrides a LOSE produced by step 1 (fall-through "win-overrides-loss":
 *      kill the boss before allies fall and the chapter is still won).
 *
 * Deadness is queried through fd2_check_char_is_dead (runtime_char[idx].flags
 * bit0) for all four slots; the body reads no bFlags inline.
 * ---------------------------------------------------------------- */
void fd2_chapter_18_post_action(uint32 event_arg)
{
    (void)event_arg;

    if (fd2_check_char_is_dead(0) != 0 ||
        fd2_check_char_is_dead(0x10) != 0 ||
        fd2_check_char_is_dead(0x11) != 0) {
        data_fd2_chapter_event_or_battle_end_code = 1;
    }

    if (fd2_check_char_is_dead(0x34) != 0) {
        data_fd2_chapter_event_or_battle_end_code = 2;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_19_post_action @ 0x20926  (dispatched, 0 direct callers)
 *
 * Chapter 19 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[18] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site invokes the handler
 * with no arguments; event_arg is the Watcom __CHK-prologue artifact and
 * is unused by the body.
 *
 * Runs the default win/lose check, then adds a turn-GATED single-slot
 * lose-condition override: only from turn 7 onwards (turn counter at
 * 0x53BEF strictly greater than 6) AND key NPC runtime_char[0x40] dead is
 * game_event_flag (0x53ECC) set to 1 (game over). Before turn 7 the slot
 * is unprotected (off-map or in an invulnerable scripted state), so the
 * dead-check is gated behind the turn comparison. Structurally this is
 * chapter 15's single-slot 0x40 lose check wrapped in chapter 13's strict
 * `turn > 6` gate. Deadness is queried through fd2_check_char_is_dead
 * (runtime_char[idx].flags bit0); the turn comparison is signed.
 * ---------------------------------------------------------------- */
void fd2_chapter_19_post_action(uint32 event_arg)
{
    (void)event_arg;

    fd2_check_battle_end_condition();

    if ((int32)data_fd2_battle_turn_counter > 6) {
        if (fd2_check_char_is_dead(0x40) != 0) {
            data_fd2_chapter_event_or_battle_end_code = 1;
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_20_post_action @ 0x20957  (dispatched, 0 direct callers)
 *
 * Chapter 20 turn-cycle post-action handler — the largest non-default
 * handler in this file. Reached via
 * data_fd2_chapter_post_action_handler_table[19] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site invokes the handler
 * with no real arguments; event_arg is the Watcom __CHK-prologue artifact
 * and is unused by the body.
 *
 * A large multi-faction battle: three protected sides and three enemy
 * groups. Runs the default win/lose check, then layers three independent
 * stages on top, each writing game_event_flag (0x53ECC):
 *
 *   Stage 1 — NPC group extinction (LOSE + dialog). Scan the 8-slot NPC
 *     group runtime_char[0x35..0x3C] (loop i in 0x26..0x2D, slot i + 0xF).
 *     The loop does NOT early-exit; it sets a "some slot still alive" flag
 *     the instant any slot reports alive (fd2_check_char_is_dead == 0) and
 *     runs to completion. If every slot is dead, set the flag to 1 (game
 *     over) and show current_chapter_text page 10. Flag is written before
 *     the dialog call.
 *
 *   Stage 2 — key-char extinction (LOSE). If the hero runtime_char[0] OR
 *     the boss-ally runtime_char[0x34] is dead, set the flag to 1. The OR
 *     short-circuits: [0x34] is only tested when [0] is alive.
 *
 *   Stage 3 — enemy wipe (WIN). Reset the "alive" flag, then scan two
 *     enemy ranges as a union: runtime_char[0x24..0x33] (loop i in
 *     0x15..0x24) and runtime_char[0x3D..0x52] (loop i in 0x2E..0x43),
 *     each slot i + 0xF. Neither loop early-exits. If no slot in either
 *     range is alive (all three enemy groups wiped), set the flag to 2
 *     (WIN). Because stage 3 runs after stages 1-2, a full enemy wipe
 *     overrides a LOSE produced earlier (win-overrides-loss).
 *
 * Deadness is queried through fd2_check_char_is_dead (runtime_char[idx].flags
 * bit0) for every slot; the body reads no bFlags inline. The dialog call uses
 * the chapter's standard glyph geometry (render base 0xA0000, pitch 0x140,
 * glyph params 0xCD/0x4C/0x4A, height 0x13) with blink_flag = 1.
 * ---------------------------------------------------------------- */
void fd2_chapter_20_post_action(uint32 event_arg)
{
    int i;
    int some_alive;

    (void)event_arg;

    fd2_check_battle_end_condition();

    some_alive = 0;
    for (i = 0x26; i < 0x2E; i++) {
        if (fd2_check_char_is_dead(i + 0xF) == 0) {
            some_alive = 1;
        }
    }
    if (some_alive == 0) {
        data_fd2_chapter_event_or_battle_end_code = 1;
        fd2_display_dialog_scene(current_chapter_text, 10, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
    }

    if (fd2_check_char_is_dead(0) != 0 ||
        fd2_check_char_is_dead(0x34) != 0) {
        data_fd2_chapter_event_or_battle_end_code = 1;
    }

    some_alive = 0;
    for (i = 0x15; i < 0x25; i++) {
        if (fd2_check_char_is_dead(i + 0xF) == 0) {
            some_alive = 1;
        }
    }
    for (i = 0x2E; i < 0x44; i++) {
        if (fd2_check_char_is_dead(i + 0xF) == 0) {
            some_alive = 1;
        }
    }
    if (some_alive == 0) {
        data_fd2_chapter_event_or_battle_end_code = 2;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_21_post_action @ 0x20A51  (dispatched, 0 direct callers)
 *
 * Chapter 21 turn-cycle post-action handler. Reached via
 * data_fd2_chapter_post_action_handler_table[20] (table @ 0x51B19,
 * indexed by current_chapter_id). The dispatch site invokes the handler
 * with no real arguments; event_arg is the Watcom __CHK-prologue artifact
 * and is unused by the body.
 *
 * Runs the default win/lose check, then adds a two-slot lose-condition
 * override: if either escort NPC at runtime_char[0x10] OR [0x11] is dead,
 * set game_event_flag (0x53ECC) to 1 (game over). The OR short-circuits:
 * [0x11] is only tested when [0x10] is alive. Deadness is queried through
 * fd2_check_char_is_dead (runtime_char[idx].flags bit0); the body reads no
 * bFlags inline. (Chapter 21 has two escort NPCs at slots 0x10, 0x11 that
 * must survive.)
 * ---------------------------------------------------------------- */
void fd2_chapter_21_post_action(uint32 event_arg)
{
    (void)event_arg;

    fd2_check_battle_end_condition();

    if (fd2_check_char_is_dead(0x10) != 0 ||
        fd2_check_char_is_dead(0x11) != 0) {
        data_fd2_chapter_event_or_battle_end_code = 1;
    }
}
