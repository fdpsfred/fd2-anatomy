/*
 * chevt2.c — chapter event handlers (per-event dispatch table @ 0x51B91)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>     /* free (FDOTHER.DAT cinematic sprite teardown) */

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

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_34__ch23_ai_ctrl @ 0x352E2  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x34. Triggered
 * in chapter 23 at turns 13, 15, 18, 22 (all phase 0; ch23 turn-event slots
 * 0-3). Category: AI/cinematic, no dialog. Dispatch-table signature is 1-arg
 * cdecl (void fn(uint event_arg)); this handler does not read the arg.
 *
 * Effect: ch23 turn cinematic — two portrait white-flash cutscenes per call (a
 * 2-portrait pair) at fixed tile positions (2, 0xB) and (0x1A, 0xB), with the
 * portrait id derived from the current turn counter. The id is computed in 8-bit
 * (AL) arithmetic: ((uint8)turn - 0x0E) * 2 for the first portrait and the same
 * value + 1 for the second, both truncated to a byte. As the counter advances
 * (0xE -> 0xF -> 0x10 -> 0x11), each call uses a different pair (0/1, 2/3, 4/5,
 * 6/7).
 *
 * In the binary the SECOND call shares a borrowed tail: after pushing its 3 args
 * the handler falls through (no JMP — its body ends at 0x35317) into the
 * CALL fd2_cinematic_chapter_portrait_dump_with_white_flash; ADD ESP,0xC; RET
 * tail hosted in fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash
 * @ 0x35318. That tail-merge is a binary size optimisation; the
 * functionally-exact source is simply two complete calls.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_34__ch23_ai_ctrl(uint32 event_arg)
{
    (void)event_arg;

    fd2_cinematic_chapter_portrait_dump_with_white_flash(
        2, 0xB,
        (uint32)(uint8)(((uint8)data_fd2_battle_turn_counter - 0x0E) * 2));
    fd2_cinematic_chapter_portrait_dump_with_white_flash(
        0x1A, 0xB,
        (uint32)(uint8)(((uint8)data_fd2_battle_turn_counter - 0x0E) * 2 + 1));
}

/* ----------------------------------------------------------------
 * fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash @ 0x35318
 *   (1 caller: fd2_chapter_event_handler_3f__ch27_ai_ctrl)
 *
 * Transparent thunk that forwards its 3 stack args (12 bytes) to
 * fd2_cinematic_chapter_portrait_dump_with_white_flash @ 0x35822 and cleans
 * them on return. The binary body is just:
 *     CALL fd2_cinematic_chapter_portrait_dump_with_white_flash
 *     ADD ESP, 0xC
 *     RET
 * No __CHK, no own stack frame.
 *
 * The thunk exists purely as a layer-insertion / binary size optimisation: it
 * is the tail-JMP target of fd2_chapter_event_handler_3f__ch27_ai_ctrl
 * @ 0x358C7, which pushes its 3 args (0xF, 0x1B, 2) and JMPs here so it can
 * borrow this thunk's 0xC-byte cleanup tail instead of emitting its own. It
 * carries no independent game semantics — it just passes the 3 args straight
 * through to the cinematic helper (target_tile_x, target_tile_y, chapter_id).
 *
 * The functionally-exact source is a plain cdecl forwarding wrapper; Watcom
 * lowers the wrapper to the same push-args / call / cleanup / ret shape.
 * ---------------------------------------------------------------- */
void fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash(
    uint32 target_tile_x, uint32 target_tile_y, uint32 chapter_id)
{
    fd2_cinematic_chapter_portrait_dump_with_white_flash(
        target_tile_x, target_tile_y, chapter_id);
}

/* ----------------------------------------------------------------
 * fd2_cinematic_chapter_portrait_dump_with_white_flash @ 0x35822
 *   (11 callers: chapter 28/29 init, chapter event handlers 34/3f/40/42/44/
 *    46/48/4a, and the transparent thunk above)
 *
 * Chapter portrait cinematic with a white-flash transition. Pans the cursor /
 * window to the target tile, swaps the portrait set (chapter_id truncated to its
 * low byte), then plays a brief pure-white screen flash (palette +0xFF then +0)
 * to mask the portrait change.
 *
 * Sequence (functionally-exact):
 *   fd2_pan_cursor_and_window(target_tile_x, target_tile_y)
 *   fd2_load_chapter_portraits_and_dump_tmp(chapter_id & 0xFF)
 *   __delay_thunk_375b2(300)                            -- hold the new portrait
 *   fd2_set_vga_palette_range_with_add(0, 0xFF, 0xFF)   -- +0xFF = pure white
 *   __delay_thunk_375b2(200)                            -- white screen
 *   fd2_set_vga_palette_range_with_add(0, 0xFF, 0)      -- restore palette
 *   fd2_composite_battle_frame(0)
 *   fd2_delay_400ms_via_idle_thunk()                    -- 400ms recovery hold
 *
 * The chapter_id arg arrives as a full 32-bit stack word; the binary applies a
 * MOVZX of its low byte (param_3 & 0xFF) before forwarding it to the portrait
 * loader. In the binary the final delay is reached by JMP into
 * fd2_delay_400ms_via_idle_thunk @ 0x353CC (a tail-call that borrows that
 * function's PUSH 0x190 / CALL __delay_thunk_375b2 / cleanup / RET); the
 * functionally-exact source is a plain call followed by return.
 * ---------------------------------------------------------------- */
void fd2_cinematic_chapter_portrait_dump_with_white_flash(
    uint32 target_tile_x, uint32 target_tile_y, uint32 chapter_id)
{
    fd2_pan_cursor_and_window(target_tile_x, target_tile_y);
    fd2_load_chapter_portraits_and_dump_tmp(chapter_id & 0xFF);
    __delay_thunk_375b2(300);
    fd2_set_vga_palette_range_with_add(0, 0xFF, 0xFF);
    __delay_thunk_375b2(200);
    fd2_set_vga_palette_range_with_add(0, 0xFF, 0);
    fd2_composite_battle_frame(0);
    fd2_delay_400ms_via_idle_thunk();
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_35__unref_dialog_with_state @ 0x35321
 *   (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x35. No chapter
 * FDFIELD turn-event / tile-step hook references this slot (unref / possibly
 * cut content). Category: dialog with state. Dispatch-table signature is 1-arg
 * cdecl (void fn(uint event_arg)); this handler does not read the arg.
 *
 * Effect: show dialog page 5, then kill every runtime_char_array slot from
 * index 0x12 to the end (sets hp_current = 0 for slots 0x12..count-1, then
 * plays the death animation once) — a cinematic terminator-style mass kill.
 *
 * In the binary the kill call hosts a borrowed shared tail at 0x35354
 * (CALL fd2_kill_runtime_chars_from_index_to_end; ADD ESP,4; RET):
 * fd2_chapter_event_handler_53 does its own inline dialog then JMPs here
 * pre-pushing its own kill-from index to reuse this 0xC-byte cleanup tail.
 * That tail-merge is a binary size optimisation; the functionally-exact source
 * for this handler is simply the dialog call followed by the kill call.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_35__unref_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_display_dialog_scene(current_chapter_text, 5, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_kill_runtime_chars_from_index_to_end(0x12);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_36__ch24_cinematic @ 0x3535D  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x36. Triggered
 * in chapter 24 at turns 2, 4, 7, 10 (all phase 0; ch24 turn-event slots 0-3).
 * Category: cinematic, no dialog. Dispatch-table signature is 1-arg cdecl
 * (void fn(uint event_arg)); this handler does not read the arg.
 *
 * Effect: ch24 establishing shot — load the portrait set indexed directly by
 * data_fd2_battle_turn_counter (the RAW counter, NOT the signed /2 used by the
 * ch21/ch22 handlers; the set rotates per turn), then sweep the camera around
 * the four map corners with a 400ms hold at each: top-left (0, 4), bottom-left
 * (0, 0x16), bottom-right (0x1A, 0x18), top-right (0x1A, 2). No dialog.
 *
 * In the binary the final pan + 400ms hold + RET is a Class-3 shared tail at
 * 0x353C4: fd2_chapter_event_handler_39__ch26_cinematic performs its own
 * initial pans then JMPs here for the last pan-and-delay-and-RET. Additionally
 * the 4th delay block (PUSH 0x190; CALL delay; ADD ESP,4; RET) at 0x353CC is
 * registered as a separate callable fd2_delay_400ms_via_idle_thunk. Both are
 * binary size optimisations; the functionally-exact source for this handler is
 * the portrait load followed by all four pan + 400ms-hold pairs.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_36__ch24_cinematic(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(data_fd2_battle_turn_counter);

    fd2_pan_cursor_and_window(0, 4);
    __delay_thunk_375b2(400);
    fd2_pan_cursor_and_window(0, 0x16);
    __delay_thunk_375b2(400);
    fd2_pan_cursor_and_window(0x1A, 0x18);
    __delay_thunk_375b2(400);
    fd2_pan_cursor_and_window(0x1A, 2);
    __delay_thunk_375b2(400);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_37__ch25_first_time @ 0x353DA  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x37. Triggered
 * in chapter 25 as tile-step event_type 0x01 (ch25 tile-step slot 0). Category:
 * first-time gated tile-step combat trigger. Dispatch-table signature is 1-arg
 * cdecl (void fn(uint event_arg)); the arg is the stepping char id (read from
 * [ESP+0x10]).
 *
 * Effect: ch25 lord-only tile trigger. Copy the inline 3-byte battle-drop entry
 * (type=0 ITEM, value=0x0B -> item id 11) into a local. When the lord (char 0)
 * steps and the tile event has not yet been consumed (consumed_flags[0] == 0):
 * show dialog page 0, play the full combat cinematic against target char 0x11,
 * run the death animation, and only if char 0x11 was actually killed mark the
 * tile event consumed, tick tile-event animations, recomposite the battle frame,
 * and grant char 0 the battle drop. The pending XP credit is always cleared.
 *
 * In the binary the handler ends with JMP 0x34FC5 — the cleanup-only Class-3
 * shared tail (ADD ESP,4; POP EDI; POP ESI; RET) hosted in
 * fd2_chapter_event_handler_27__unref_drop. That tail-merge is a binary size
 * optimisation; the borrowed teardown is just this handler's own local-slot
 * cleanup and register restore, so the functionally-exact source is a plain
 * return.
 * ---------------------------------------------------------------- */
static const unsigned char data_fd2_chapter_event_handler_37_drop_entry_inline[3] =
    { 0x00, 0x0B, 0x00 };

void fd2_chapter_event_handler_37__ch25_first_time(uint32 event_arg)
{
    uint8 drop_entry[3];

    drop_entry[0] = data_fd2_chapter_event_handler_37_drop_entry_inline[0];
    drop_entry[1] = data_fd2_chapter_event_handler_37_drop_entry_inline[1];
    drop_entry[2] = data_fd2_chapter_event_handler_37_drop_entry_inline[2];

    if (event_arg == 0 &&
        *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr) == 0) {
        fd2_display_dialog_scene(current_chapter_text, 0, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        /* second arg is the stepping char id from [ESP+0x10] (= event_arg);
         * inside this branch event_arg is provably 0, so the lord (char 0) is
         * the defender_idx fighting attacker char 0x11. */
        fd2_play_full_combat_cinematic(0x11, event_arg);
        fd2_play_death_animation_and_mark_dead();
        if (fd2_check_char_is_dead(0x11) != 0) {
            *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr) = 1;
            fd2_tick_tile_event_animations();
            fd2_composite_battle_frame(1);
            /* drop recipient is the stepping char id from [ESP+0x18]
             * (= event_arg), provably 0 here so the lord (char 0) gets it. */
            fd2_process_battle_drop_entries(event_arg, 1, (uint32)drop_entry);
        }
    }

    data_fd2_battle_pending_xp_credit = 0;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_38__ch25_dialog_with_state @ 0x35487
 *   (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x38. Triggered
 * in chapter 25 at turn 6 / phase 1 (ch25 turn-event slot 0). Category: dialog
 * with state. Dispatch-table signature is 1-arg cdecl (void fn(uint event_arg));
 * this handler does not read the arg.
 *
 * Effect: ch25 turn-6 establishing scene — pan camera/window to (6, 0x28), load
 * portrait set 1, fire cutscene event 0x4A, show dialog page 5, then reset every
 * char's facing direction.
 *
 * In the binary the handler ends with JMP 0x134E4 — a tail-call into
 * fd2_clear_all_chars_facing (borrowing that function's body instead of a
 * CALL/RET pair). That tail-merge is a binary size optimisation; the
 * functionally-exact source is a plain call followed by return.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_38__ch25_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_pan_cursor_and_window(6, 0x28);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_cutscene_event_trigger(0x4A);
    fd2_display_dialog_scene(current_chapter_text, 5, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_clear_all_chars_facing();
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_39__ch26_cinematic @ 0x354DD  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x39 (table
 * entry @ 0x51C75). Triggered in chapter 26 across turn-event slots 0-7+
 * (turns 2/4/6/8/A/C/F/0x10, all phase 0). Category: cinematic, no dialog.
 * Dispatch-table signature is 1-arg cdecl (void fn(uint event_arg)); this
 * handler does not read the arg.
 *
 * Effect: ch26 establishing-shot stub — load the portrait set indexed directly
 * by data_fd2_battle_turn_counter (the RAW counter, same as the ch24 handler,
 * NOT the signed /2 used by the ch21/ch22 handlers; the set rotates per turn),
 * pan the camera/window to (9, 0), and hold 400ms.
 *
 * In the binary the final pan + 400ms hold + RET is a Class-3 shared tail at
 * 0x353C4 hosted in fd2_chapter_event_handler_36__ch24_cinematic: after pushing
 * its pan args (Y=0, X=9) this handler does JMP 0x353C4, falling into the
 * CALL fd2_pan_cursor_and_window; ADD ESP,8; PUSH 0x190; CALL __delay_thunk_375b2;
 * ADD ESP,4; RET tail. That tail-merge is a binary size optimisation; the
 * functionally-exact source is the portrait load followed by one pan + 400ms hold.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_39__ch26_cinematic(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(data_fd2_battle_turn_counter);
    fd2_pan_cursor_and_window(9, 0);
    __delay_thunk_375b2(400);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_3a__unref_pickup @ 0x354FE  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x3A (table entry
 * @ 0x51C79). No chapter FDFIELD turn-event / tile-step hook references this
 * slot (unref / possibly cut content). Category: item pickup at cursor tile.
 * Dispatch-table signature is 1-arg cdecl (the stepping char id under the
 * tile-step ABI); this handler uses it as the pickup recipient.
 *
 * Effect: tile-pickup — copy the inline 5-byte item-id lookup table (from
 * 0x5274E: { 0x1D, 0x2B, 0x33, 0x3D, 0x47 }, indexed by tile terrain class)
 * into a local, clear the keyboard buffer, load the stepping char's portrait.
 * If the char's inventory is full (8 usable slots) show the "inventory full"
 * dialog page 0x1E0 and slide the status screen back out. Otherwise read the
 * cursor tile's attribute, take its terrain-class byte as the table index,
 * publish the picked-up item sprite id (table[idx] + 0xB5) for the dialog, show
 * the "you got [item]" dialog page 0x1A6, grant the item, slide the status
 * screen out, mark all 5 tile-event slots consumed (broad lockout), and tick
 * the tile-event animations.
 *
 * Unlike the chapter-dialog handlers in this file this one renders against
 * data_fd2_all_game_text_ptr with the 0xA9F23 render buffer (a different text
 * scope from current_chapter_text), matching the shop / battle item dialogs.
 *
 * The tile-attribute read fills an 8-byte buffer; the index byte is the low
 * byte of the +2 ushort terrain_class field (0..0x1F). The lookup table is only
 * 5 entries, so only terrain classes 0..4 select a defined item; the binary
 * reads the raw frame slot for any larger index (see emit_issues 000354fe).
 * ---------------------------------------------------------------- */
static const unsigned char data_fd2_chapter_event_handler_3a_pickup_item_id_table_inline[5] =
    { 0x1D, 0x2B, 0x33, 0x3D, 0x47 };

void fd2_chapter_event_handler_3a__unref_pickup(uint32 stepping_char_id)
{
    unsigned char item_id_table[5];
    uint8 tile_read_buf[8];
    uint8 tile_attr;
    int usable_slots;
    uint8 i;

    item_id_table[0] = data_fd2_chapter_event_handler_3a_pickup_item_id_table_inline[0];
    item_id_table[1] = data_fd2_chapter_event_handler_3a_pickup_item_id_table_inline[1];
    item_id_table[2] = data_fd2_chapter_event_handler_3a_pickup_item_id_table_inline[2];
    item_id_table[3] = data_fd2_chapter_event_handler_3a_pickup_item_id_table_inline[3];
    item_id_table[4] = data_fd2_chapter_event_handler_3a_pickup_item_id_table_inline[4];

    fd2_clear_keyboard_buffer();
    fd2_load_chapter_portrait(
        (uint32)data_fd2_battle_runtime_char_array_ptr[stepping_char_id].portrait_id);

    usable_slots = fd2_count_usable_inventory_slots(stepping_char_id);
    if (usable_slots == 8) {
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1E0, 0xA9F23,
                                 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        fd2_wait_for_input_dialog_with_blink(0);
        fd2_close_status_screen_with_slide_out();
    } else {
        fd2_read_tile_attribute_at_pos(data_fd2_battle_cursor_world_x,
                                       data_fd2_battle_cursor_world_y,
                                       (uint32)tile_read_buf);
        tile_attr = tile_read_buf[2];
        data_fd2_dialog_last_action_sprite_id_param =
            (uint32)item_id_table[tile_attr] + 0xB5;
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1A6, 0xA9F23,
                                 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        fd2_wait_for_input_dialog_with_blink(0);
        fd2_add_item_to_inventory(stepping_char_id, (uint32)item_id_table[tile_attr]);
        fd2_close_status_screen_with_slide_out();
        for (i = 0; i < 5; i = i + 1) {
            *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + i) = 1;
        }
        fd2_tick_tile_event_animations();
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_3b__ch26_ai_ctrl @ 0x35641  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x3B (table
 * entry @ 0x51C7D). Triggered in chapter 26 as tile-step event_type 0x00
 * (ch26 tile-step slot 0). Category: char-conditional AI setup.
 * Dispatch-table signature is 1-arg cdecl (the stepping char id under the
 * tile-step ABI, read from [ESP+0x4]).
 *
 * Effect: when the stepping char is a non-enemy (team != 0, i.e. npc or
 * player; an enemy stepper with team==0 is skipped), disarm AI control flag
 * (combat_aux_block[0xD] low nibble) for the 6 chars 0x27..0x2C. No dialog.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_3b__ch26_ai_ctrl(uint32 stepping_char_id)
{
    if (data_fd2_battle_runtime_char_array_ptr[stepping_char_id].team != 0) {
        fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x27, 0x2C, 0);
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_3c__ch26_ai_ctrl @ 0x35675  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x3C (table
 * entry @ 0x51C81). Triggered in chapter 26 as tile-step event_type 0x00
 * (ch26 tile-step slot 1). Category: char-conditional AI setup.
 * Dispatch-table signature is 1-arg cdecl (the stepping char id under the
 * tile-step ABI, read from [ESP+0x4]).
 *
 * Effect: when the stepping char is a non-enemy (team != 0, i.e. npc or
 * player; an enemy stepper with team==0 is skipped), disarm AI control flag
 * (combat_aux_block[0xD] low nibble) for two char ranges — 0x17..0x18 and
 * 0x35..0x38 (2 + 4 = 6 chars). No dialog.
 *
 * In the binary the SECOND call + cleanup + RET is a Class-3 shared tail at
 * 0x356AE: fd2_chapter_event_handler_50__ch30_ai_ctrl pushes its own 3 args
 * then JMPs here to borrow this call + ADD ESP,0xC + RET tail. That tail-merge
 * is a binary size optimisation; the functionally-exact source is simply two
 * complete calls.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_3c__ch26_ai_ctrl(uint32 stepping_char_id)
{
    if (data_fd2_battle_runtime_char_array_ptr[stepping_char_id].team != 0) {
        fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x17, 0x18, 0);
        fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x35, 0x38, 0);
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_3d__ch26_pickup @ 0x356B7  (0 direct callers)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x3D. Triggered
 * in chapter 26 as tile-step event_type 0x01 (ch26 tile-step slot 2). Category:
 * major-quest item pickup with cinematic + char spawn. Dispatch-table signature
 * is 1-arg cdecl (the stepping char id under the tile-step ABI, read from
 * [ESP+0x4]).
 *
 * First-time only (tile_event_consumed_flags[0xC] == 0): load the stepping
 * char's portrait, then check whether that char is carrying key item 0xD0.
 *   - NOT carrying it: show the "you don't have it" dialog (page 2), paint the
 *     portrait, wait for input, slide the status screen back out, and return
 *     WITHOUT consuming the tile event (it may be retried later).
 *   - Carrying it: consume the item (remove its inventory slot), show dialog
 *     page 3, wait, slide out, then play the 59-frame FDOTHER.DAT[0x2D] cinematic
 *     (blit each frame to the 0xABCE4 VGA target, 2 BIOS ticks per frame), free
 *     the sprite, mark the tile event consumed (flags[0xC] = 1), tick the
 *     tile-event animations, reload the chapter portraits, spawn the joining
 *     char id 0x1F from base+growth, and show the full-screen dialog page 4.
 *
 * The dialog render target differs by page: 0xA951F for the in-frame portrait
 * dialogs (pages 2, 3) and 0xA0000 for the final full-screen page 4. The blit
 * loop runs over [0, 0x3B) = 59 frames.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_3d__ch26_pickup(uint32 stepping_char_id)
{
    uint32 slot;
    uint32 sprite_atlas;
    uint32 frame_idx;

    if (*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0xC) != 0) {
        return;
    }

    fd2_load_chapter_portrait(
        (uint32)data_fd2_battle_runtime_char_array_ptr[stepping_char_id].portrait_id);

    slot = (uint32)fd2_find_inventory_slot_with_item(stepping_char_id, 0xD0);
    if (slot == 0xFFFFFFFF) {
        fd2_display_dialog_scene(current_chapter_text, 2, 0xA951F, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        fd2_wait_for_input_dialog_with_blink(0);
        fd2_close_status_screen_with_slide_out();
        return;
    }

    fd2_remove_inventory_slot_at(stepping_char_id, slot);
    fd2_display_dialog_scene(current_chapter_text, 3, 0xA951F, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_wait_for_input_dialog_with_blink(0);
    fd2_close_status_screen_with_slide_out();

    sprite_atlas = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x2D);
    for (frame_idx = 0; (int)frame_idx < 0x3B; frame_idx = frame_idx + 1) {
        fd2_blit_indexed_sprite(sprite_atlas, frame_idx, 0xABCE4, 0x140, -1);
        fd2_wait_n_bios_ticks(2);
    }
    free((void *)sprite_atlas);

    *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0xC) = 1;
    fd2_tick_tile_event_animations();
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_init_runtime_char_from_base_growth(0x1F);
    fd2_display_dialog_scene(current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_3e__ch27_dyn_turn_event @ 0x35898
 *   (1 caller: dispatch table @ 0x51B91, entry @ 0x51C89)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x3E. Triggered
 * in chapter 27 as tile-step event_type 0x00 (ch27 tile-step slot 0). Category:
 * state-machine mutator (dynamic turn-event scheduler). Dispatch-table
 * signature is 1-arg cdecl (void fn(uint event_arg)); this handler does not
 * read the arg.
 *
 * Effect: ch27 first-time tile trigger. The first time this tile is stepped
 * (tile_event_consumed_flags[0x11] == 0): write turn_counter + 1 into the
 * turn-event hook table at tile_event_data_table[+3] (hook entry 0's turn
 * byte), which arms a dynamic turn-event one player turn ahead, then consume
 * the slot (flags[0x11] = 1) so it never re-arms.
 *
 * The turn counter is read as a single byte and incremented in 8-bit before
 * the byte store (MOV DL,[turn_counter] / INC DL / MOV [data_table+3],DL); the
 * (uint8) truncation on store reproduces that 8-bit arithmetic exactly.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_3e__ch27_dyn_turn_event(uint32 event_arg)
{
    (void)event_arg;

    if (*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x11) == 0) {
        *(uint8 *)(data_fd2_tile_event_data_table_ptr + 3) =
            (uint8)(data_fd2_battle_turn_counter + 1);
        *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x11) = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_3f__ch27_ai_ctrl @ 0x358C7
 *   (1 caller: dispatch table @ 0x51B91, entry @ 0x51C8D)
 *
 * Invoked via per-event handler table @ 0x51B91, dispatch idx 0x3F. Triggered
 * in chapter 27 at turn-event slot 0 (turn=0xFF / sentinel marker). Category:
 * 2-portrait cinematic pair. Dispatch-table signature is 1-arg cdecl
 * (void fn(uint event_arg)); this handler does not read the arg.
 *
 * Effect: ch27 turn-FF marker — a 2-portrait reveal. The first portrait white-
 * flash cutscene is shown at tile (3, 0x1B) with chapter id 1, the second at
 * tile (0xF, 0x1B) with chapter id 2.
 *
 * In the binary the second cutscene is reached by pushing its 3 args (0xF,
 * 0x1B, 2) and tail-JMPing into fd2_wrap_cinematic_chapter_portrait_dump_with_
 * white_flash @ 0x35318, so the handler borrows that thunk's 0xC-byte cleanup
 * tail instead of emitting its own. The thunk forwards the args straight to
 * fd2_cinematic_chapter_portrait_dump_with_white_flash @ 0x35822; calling the
 * thunk here keeps that documented tail-JMP relationship intact and is
 * functionally exact.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_3f__ch27_ai_ctrl(uint32 event_arg)
{
    (void)event_arg;

    fd2_cinematic_chapter_portrait_dump_with_white_flash(3, 0x1B, 1);
    fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash(0xF, 0x1B, 2);
}
