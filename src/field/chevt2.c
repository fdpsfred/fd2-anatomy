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

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_30__ch21_ai_ctrl @ 0x351C6  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x30.
 * Triggered in chapter 21 at turn-event slot 1 (turn 5, phase 0). Category:
 * AI setup. Dispatch-table signature is 1-arg cdecl (void fn(uint event_arg));
 * this handler does not read the arg.
 *
 * Effect: arm AI control flag 3 (combat_aux_block[0xD] low nibble) for two
 * NPC char ranges — 0x23..0x2A and 0x43..0x4A (8 + 8 = 16 chars).
 *
 * In the binary the second call shares a borrowed tail: after pushing its
 * 3 args the handler does JMP 0x34F39, falling through into the
 * CALL fd2_set_combat_aux_block_byte_d_low4_for_char_range; ADD ESP,0xC; RET
 * tail of fd2_chapter_event_handler_12 @ 0x34F02. That tail-merge is a binary
 * size optimisation; the functionally-exact source is simply two calls.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_30__ch21_ai_ctrl(uint32 event_arg)
{
    (void)event_arg;

    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x23, 0x2A, 3);
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x43, 0x4A, 3);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_31__ch22_turn_gated @ 0x351E9  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x31.
 * Triggered in chapter 22 at turns 3 and 7 (both phase 0; ch22 turn-event
 * slots 0 and 2). Category: turn-gated cinematic. Dispatch-table signature is
 * 1-arg cdecl (void fn(uint event_arg)); this handler does not read the arg.
 *
 * Effect: ch22 turn-gated — load portrait set indexed by save_metadata/2
 * (data_fd2_battle_turn_counter, signed /2, rotates per call), a 2-corner pan
 * across row y=0x23 (right edge x=0x20 then left edge x=0) with an 8-tick
 * pause after each, finally show dialog page 1 only when the counter equals 3.
 *
 * In the binary the dialog call and the early return share a borrowed tail:
 * when turn != 3 the handler does JNZ into the shared RET, and when turn == 3
 * it pushes its 9 args then JMP 0x35A20 — falling into the
 * PUSH current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24; RET
 * tail of fd2_chapter_event_handler_42__ch28_dialog_with_state @ 0x359C8.
 * That tail-merge is a binary size optimisation; the functionally-exact source
 * is a single self-contained dialog call.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_31__ch22_turn_gated(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(
        (uint32)((int)data_fd2_battle_turn_counter / 2));

    fd2_pan_cursor_and_window(0x20, 0x23);
    fd2_wait_n_bios_ticks(8);
    fd2_pan_cursor_and_window(0, 0x23);
    fd2_wait_n_bios_ticks(8);

    if (data_fd2_battle_turn_counter == 3) {
        fd2_display_dialog_scene(current_chapter_text, 1, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_32__ch22_reinforcement @ 0x35261  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x32.
 * Triggered in chapter 22 at turn 5 / phase 2 (ch22 turn-event slot 1).
 * Category: reinforcement spawner. Dispatch-table signature is 1-arg cdecl
 * (void fn(uint event_arg)); this handler does not read the arg.
 *
 * Effect: ch22 turn-5 reinforcement — load portrait set 2, a single-corner pan
 * to window origin (0x10, 0x2A), an 8-tick hold, spawn reinforcement char id
 * 0x14 from base+growth, then unconditionally show dialog page 2.
 *
 * In the binary the dialog call shares a borrowed tail: after the spawn the
 * handler does JMP 0x347F1, falling into the
 * PUSH 1/0x13/0x4A/0x4C/0xCD/0x140/0xA0000/2; PUSH current_chapter_text;
 * CALL fd2_display_dialog_scene; ADD ESP,0x24; RET tail of
 * fd2_chapter_event_handler_15 @ 0x347D9 (the page=2 dialog body). That
 * tail-merge is a binary size optimisation; the functionally-exact source is a
 * single self-contained dialog call.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_32__ch22_reinforcement(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(2);
    fd2_pan_cursor_and_window(0x10, 0x2A);
    fd2_wait_n_bios_ticks(8);
    fd2_init_runtime_char_from_base_growth(0x14);

    fd2_display_dialog_scene(current_chapter_text, 2, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_33__unref_drop @ 0x3529A  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x33. No chapter
 * FDFIELD turn-event / tile-step hook references this slot (unref / possibly
 * cut content). Category: battle drop + dialog. Dispatch-table signature is
 * 1-arg cdecl (the stepping char id under the tile-step ABI); this handler
 * forwards the arg as the drop recipient.
 *
 * Effect: drop one battle item from an inline 3-byte drop entry
 * (type=0 ITEM, value=0x65 -> item id 101), then unconditionally show dialog
 * page 3.
 *
 * In the binary the dialog call shares a borrowed tail: after pushing its 8
 * args (page=3) the handler does JMP 0x34FB7, falling into the
 * PUSH current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24 tail
 * hosted in fd2_chapter_event_handler_27__unref_drop @ 0x34F74. That tail-merge
 * is a binary size optimisation; the functionally-exact source is a single
 * self-contained dialog call.
 * ---------------------------------------------------------------- */
static const unsigned char data_fd2_chapter_event_handler_33_drop_entry_inline[3] =
    { 0x00, 0x65, 0x00 };

void fd2_chapter_event_handler_33__unref_drop(uint32 stepping_char_id)
{
    uint8 drop_entry[3];

    drop_entry[0] = data_fd2_chapter_event_handler_33_drop_entry_inline[0];
    drop_entry[1] = data_fd2_chapter_event_handler_33_drop_entry_inline[1];
    drop_entry[2] = data_fd2_chapter_event_handler_33_drop_entry_inline[2];
    fd2_process_battle_drop_entries(stepping_char_id, 1, (uint32)drop_entry);

    fd2_display_dialog_scene(current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}
