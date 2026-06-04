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
