/*
 * chevt2.c — chapter event handlers (per-event dispatch table @ 0x51B91)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_2f__ch21_turn_gated @ 0x35112  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x2F.
 * Triggered in chapter 21 at turns 2, 4, 6, 8 (all phase 0; ch21 turn-event
 * slots 0, 2, 3, 4). Category: turn-gated cinematic.
 *
 * Dispatch-table signature is 1-arg cdecl (void fn(uint event_arg)); this
 * handler does not read the arg.
 *
 * Effect: ch21 every-2-turns reinforcement scan — load portrait set indexed
 * by save_metadata/2 (data_fd2_battle_turn_counter, signed /2, rotates per
 * call), 4-corner camera sweep with 8-tick pauses, finally show dialog
 * page 3 only when the counter equals 2 (the 5th call).
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_2f__ch21_turn_gated(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(
        (uint32)((int)data_fd2_battle_turn_counter / 2));
    fd2_wait_n_bios_ticks(1);

    fd2_pan_cursor_and_window(0, 0);
    fd2_wait_n_bios_ticks(8);
    fd2_pan_cursor_and_window(0x1C, 0);
    fd2_wait_n_bios_ticks(8);
    fd2_pan_cursor_and_window(0x1C, 0x20);
    fd2_wait_n_bios_ticks(8);
    fd2_pan_cursor_and_window(0, 0x20);
    fd2_wait_n_bios_ticks(8);

    if (data_fd2_battle_turn_counter == 2) {
        fd2_display_dialog_scene(current_chapter_text, 3, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
    }
}
