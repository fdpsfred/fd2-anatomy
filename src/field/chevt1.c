/*
 * chevt1.c — chapter turn-event handlers (group 1)
 *
 * fd2_chapter_event_handler_00__ch1_dialog_with_state @ 0x341DB
 *     (0 direct callers; dispatched as idx 0x00 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_01__ch1_dialog_with_state @ 0x342B5
 *     (0 direct callers; dispatched as idx 0x01 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_02__ch1_dialog_with_state @ 0x3431D
 *     (0 direct callers; dispatched as idx 0x02 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_03__ch1_dialog_with_state @ 0x34377
 *     (0 direct callers; dispatched as idx 0x03 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_04__unref_dialog_with_state @ 0x343E2
 *     (0 direct callers; dispatched as idx 0x04 of the per-event
 *      handler table at 0x51B91)
 */

#include <string.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_00__ch1_dialog_with_state @ 0x341DB
 *   — Chapter 1 turn-event slot 0 (triggered at turn 3 / phase 1).
 *
 * ch1 prologue beat: 哈諾 (char_id 1) appears, the camera pans to
 * world (5, 8), cutscene event 7 plays followed by dialog page 0xB;
 * then the portrait set swaps (3 -> 7), cutscene event 8 plays
 * followed by dialog page 3, and every character's facing is reset.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is
 * callee-saved; the __CHK(0x2C) stack-probe prologue is
 * compiler-injected and omitted here.
 *
 * Walkthrough SOT: assets/chapters/chapter_01.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_00__ch1_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_init_runtime_char_from_base_growth(1);
    fd2_load_chapter_portraits_and_dump_tmp(3);
    fd2_pan_cursor_and_window(5, 8);
    fd2_composite_battle_frame(1);
    __delay_thunk_375b2(100);
    fd2_cutscene_event_trigger(7);
    fd2_clear_keyboard_buffer();
    fd2_display_dialog_scene(current_chapter_text, 0xB, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;

    fd2_load_chapter_portraits_and_dump_tmp(7);
    fd2_composite_battle_frame(1);
    __delay_thunk_375b2(100);
    fd2_cutscene_event_trigger(8);
    fd2_clear_keyboard_buffer();
    fd2_display_dialog_scene(current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_clear_all_chars_facing();
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_01__ch1_dialog_with_state @ 0x342B5
 *   — Chapter 1 turn-event slot 1 (triggered at turn 4 / phase 0).
 *
 * ch1 mid-turn beat: the camera pans to world (0xB, 0x10), party
 * slot 4 joins the field with the "new char appearance" explosion
 * animation, cutscene event 3 plays, every character's facing is
 * reset, and dialog page 4 is shown.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is
 * callee-saved; the __CHK(0x2C) stack-probe prologue is
 * compiler-injected and omitted here.
 *
 * Walkthrough SOT: assets/chapters/chapter_01.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_01__ch1_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_pan_cursor_and_window(0xB, 0x10);
    fd2_animate_party_addition_with_appear_effect(4);
    fd2_clear_keyboard_buffer();
    fd2_composite_battle_frame(1);
    fd2_cutscene_event_trigger(3);
    fd2_clear_all_chars_facing();
    fd2_display_dialog_scene(current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_02__ch1_dialog_with_state @ 0x3431D
 *   — Chapter 1 turn-event slot 2 (triggered at turn 5 / phase 0).
 *
 * ch1 mid-turn beat: the camera pans to world (0, 0x10), party
 * slot 5 joins the field with the "new char appearance" explosion
 * animation, cutscene event 4 plays, every character's facing is
 * reset, and dialog page 5 is shown.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is
 * callee-saved; the __CHK(0x2C) stack-probe prologue is
 * compiler-injected and omitted here.
 *
 * In the original binary the final dialog call is reached by a JMP
 * into the shared tail of handler_01 (PUSH current_chapter_text;
 * CALL fd2_display_dialog_scene; ADD ESP,0x24; POP EBX; RET) at
 * 0x3430D; reproduced here as the inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_01.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_02__ch1_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_pan_cursor_and_window(0, 0x10);
    fd2_animate_party_addition_with_appear_effect(5);
    fd2_clear_keyboard_buffer();
    fd2_composite_battle_frame(1);
    fd2_cutscene_event_trigger(4);
    fd2_clear_all_chars_facing();
    fd2_display_dialog_scene(current_chapter_text, 5, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_03__ch1_dialog_with_state @ 0x34377
 *   — Chapter 1 turn-event slot 3 (triggered at turn 6 / phase 1).
 *
 * ch1 mid-turn beat: the camera pans to world (0xB, 0xB), then the
 * portrait set swaps to race 6 — bracketed by setting
 * data_fd2_chapter_init_phase_flag to 1 before the reload and back
 * to 0 after, so the reload is treated as an "init phase" load.
 * The battle frame is recomposited, cutscene event 6 plays, every
 * character's facing is reset, the keyboard buffer is flushed, and
 * dialog page 6 is shown.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is
 * callee-saved; the __CHK(0x2C) stack-probe prologue is
 * compiler-injected and omitted here.
 *
 * In the original binary the final dialog call is reached by a JMP
 * into the shared tail of handler_01 (PUSH current_chapter_text;
 * CALL fd2_display_dialog_scene; ADD ESP,0x24; POP EBX; RET) at
 * 0x3430D; reproduced here as the inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_01.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_03__ch1_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_pan_cursor_and_window(0xB, 0xB);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(6);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_composite_battle_frame(1);
    fd2_cutscene_event_trigger(6);
    fd2_clear_all_chars_facing();
    fd2_clear_keyboard_buffer();
    fd2_display_dialog_scene(current_chapter_text, 6, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_04__unref_dialog_with_state @ 0x343E2
 *   — Dispatch idx 0x04 of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this
 * slot (unreferenced — possibly cut content). Its single beat flips
 * 哈瓦特 (char_id 0xD) to the ally side (team = 1) and then shows
 * dialog page 7.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary the dialog call is the head of a shared
 * tail at 0x343FA (PUSH page=7..PUSH current_chapter_text; CALL
 * fd2_display_dialog_scene; ADD ESP,0x24; RET) that
 * fd2_chapter_event_handler_11 @ 0x346C8 JMPs into for its own
 * page-7 dialog; reproduced here as the inline call for Layer-2
 * equivalence.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_04__unref_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    data_fd2_battle_runtime_char_array_ptr[0xD].team = 1;
    fd2_display_dialog_scene(current_chapter_text, 7, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}
