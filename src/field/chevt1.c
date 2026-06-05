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
 * fd2_chapter_event_handler_06__ch2_reinforcement @ 0x34422
 *     (0 direct callers; dispatched as idx 0x06 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_09__ch3_char_cond @ 0x344C2
 *     (0 direct callers; dispatched as idx 0x09 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_0b__ch4_dialog @ 0x34565
 *     (0 direct callers; dispatched as idx 0x0B of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_0c__unref_first_time @ 0x34594
 *     (0 direct callers; dispatched as idx 0x0C of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_0e__ch5_dialog_with_state @ 0x345EA
 *     (0 direct callers; dispatched as idx 0x0E of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_0f__ch5_dialog_with_state @ 0x3462E
 *     (0 direct callers; dispatched as idx 0x0F of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_10__ch5_dialog @ 0x34696
 *     (0 direct callers; dispatched as idx 0x10 of the per-event
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

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_06__ch2_reinforcement @ 0x34422
 *   — Chapter 2 turn-event slot 0 (triggered at turn 3 / phase 1),
 *     dispatched as idx 0x06 of the per-event handler table at 0x51B91.
 *
 * ch2 reinforcement beat: the camera pans to world (9, 1), portrait
 * set 3 reloads — bracketed by setting data_fd2_chapter_init_phase_flag
 * to 1 before the reload and back to 0 after, so it is treated as an
 * "init phase" load — cutscene event 0xD plays, and dialog page 4 is
 * shown. Then six reinforcement enemies (runtime-char slots 5..0xA) are
 * armed with the AI behaviour pair (combat_aux_block[0xE]=0x1A,
 * combat_aux_block[0xF]=0x0F).
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is
 * callee-saved; the __CHK(0x2C) stack-probe prologue is
 * compiler-injected and omitted here.
 *
 * Walkthrough SOT: assets/chapters/chapter_02.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_06__ch2_reinforcement(uint32 event_arg)
{
    int32 i;

    (void)event_arg;

    fd2_pan_cursor_and_window(9, 1);
    __delay_thunk_375b2(100);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(3);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0xD);
    __delay_thunk_375b2(200);
    fd2_display_dialog_scene(current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);

    for (i = 5; i < 0xB; i++) {
        data_fd2_battle_runtime_char_array_ptr[i].combat_aux_block[0xE] = 0x1A;
        data_fd2_battle_runtime_char_array_ptr[i].combat_aux_block[0xF] = 0x0F;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_09__ch3_char_cond @ 0x344C2
 *   — Chapter 3 turn-event slot 0 (triggered at turn 3 / phase 2),
 *     dispatched as idx 0x09 of the per-event handler table at 0x51B91.
 *
 * char-conditional beat: gated on 沃斯 (char_id 6) still being alive
 * (flags bit0 clear). If alive, portrait set 2 reloads, the camera pans
 * from world (3, 0) to (3, 0x11) with an ~800ms / ~200ms hold between
 * the two pans, and dialog page 4 is shown. If 沃斯 is already dead the
 * whole beat is skipped.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary the trailing dialog call is the head of a
 * shared tail at 0x34516 (PUSH page=4 .. PUSH current_chapter_text;
 * CALL fd2_display_dialog_scene; ADD ESP,0x24; RET) that
 * fd2_chapter_event_handler_0F @ 0x3462E JMPs into for its own page-4
 * dialog; reproduced here as the inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_03.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_09__ch3_char_cond(uint32 event_arg)
{
    (void)event_arg;

    if (fd2_check_char_is_dead(6) == 0) {
        fd2_load_chapter_portraits_and_dump_tmp(2);
        fd2_pan_cursor_and_window(3, 0);
        __delay_thunk_375b2(800);
        fd2_pan_cursor_and_window(3, 0x11);
        __delay_thunk_375b2(200);
        fd2_display_dialog_scene(current_chapter_text, 4, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_0b__ch4_dialog @ 0x34565
 *   — Chapter 4 turn-event slot 0 (triggered at turn 4 / phase 1),
 *     dispatched as idx 0x0B of the per-event handler table at 0x51B91.
 *
 * The simplest dialog-only beat in the group: a straight-line, no-branch
 * sequence with no RNG, no numeric computation, and no CALL-return value
 * used. Portrait set 2 reloads, then dialog page 2 is shown.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler prepares its own 8 PUSHes (page=2
 * plus the fixed dialog geometry) and then JMPs into handler_09's shared
 * tail at 0x3452F (PUSH current_chapter_text; CALL fd2_display_dialog_scene;
 * ADD ESP,0x24; RET); reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_04.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_0b__ch4_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(2);
    fd2_display_dialog_scene(current_chapter_text, 2, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_0c__unref_first_time @ 0x34594
 *   — Dispatch idx 0x0C of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced — possibly cut content). It is a first-time-gated beat:
 * the body runs only while tile_event_consumed_flags[0x10] is still 0,
 * and consuming the flag (set to 1) at the end makes every later call a
 * no-op. Its single beat arms AI flag 7 on four enemies (the low nibble
 * of combat_aux_block[0xD] becomes 7 for chars 0x18..0x1B) and then shows
 * dialog page 3.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * The pointer global data_fd2_field_map_tile_event_consumed_flags_ptr
 * holds the base of the 0x20-byte tile-event consumed-flags block; the
 * gate flag is byte [0x10] of that block.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_0c__unref_first_time(uint32 event_arg)
{
    (void)event_arg;

    if (*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) == 0) {
        fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x18, 0x1B, 7);
        fd2_display_dialog_scene(current_chapter_text, 3, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_0e__ch5_dialog_with_state @ 0x345EA
 *   — Chapter 5 turn-event slot 0 (triggered at turn 3 / phase 0),
 *     dispatched as idx 0x0E of the per-event handler table at 0x51B91.
 *
 * ch5 turn-3 beat: disarm the per-event AI/dialog control flag (low 4
 * bits of combat_aux_block[0xD]) by writing 0 across two character
 * ranges — chars 0x25..0x28 (4 chars) and chars 0x0D..0x18 (12 chars)
 * — then show dialog page 3.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler prepares its own 8 PUSHes (page=3
 * plus the fixed dialog geometry) and then JMPs into handler_09's shared
 * tail at 0x3452F (PUSH current_chapter_text; CALL fd2_display_dialog_scene;
 * ADD ESP,0x24; RET); reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_05.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_0e__ch5_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x25, 0x28, 0);
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0xD, 0x18, 0);
    fd2_display_dialog_scene(current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_0f__ch5_dialog_with_state @ 0x3462E
 *   — Chapter 5 turn-event slot 1 (triggered at turn 4 / phase 1),
 *     dispatched as idx 0x0F of the per-event handler table at 0x51B91.
 *
 * ch5 turn-4 beat: the battle-animation phase is reset to 0, then
 * portrait set 2 reloads — bracketed by setting
 * data_fd2_chapter_init_phase_flag to 1 before the reload and back to 0
 * after, so the reload is treated as an "init phase" load. The camera
 * pans to world (0xE, 0), cutscene event 0x17 plays, every character's
 * facing is reset, and the per-event AI/dialog control flag (low 4 bits
 * of combat_aux_block[0xD]) is disarmed by writing 0 across two
 * character ranges — chars 0x07..0x0C (6 chars) and chars 0x21..0x23
 * (3 chars). Finally dialog page 4 is shown.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler prepares its own 8 PUSHes (page=4
 * plus the fixed dialog geometry) and then JMPs into handler_09's shared
 * tail at 0x34516 (PUSH current_chapter_text; CALL fd2_display_dialog_scene;
 * ADD ESP,0x24; RET); reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_05.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_0f__ch5_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    data_fd2_battle_anim_phase = 0;
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(2);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_pan_cursor_and_window(0xE, 0);
    fd2_cutscene_event_trigger(0x17);
    fd2_clear_all_chars_facing();
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(7, 0xC, 0);
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x21, 0x23, 0);
    fd2_display_dialog_scene(current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_10__ch5_dialog @ 0x34696
 *   — Chapter 5 turn-event slot 2 (triggered at turn 7 / phase 0),
 *     dispatched as idx 0x10 of the per-event handler table at 0x51B91.
 *
 * A dialog-only beat: a straight-line, no-branch sequence with no RNG,
 * no numeric computation, and no CALL-return value used. Portrait set 3
 * reloads, then dialog page 5 is shown.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler prepares its own 8 PUSHes (page=5
 * plus the fixed dialog geometry) and then JMPs into handler_09's shared
 * tail at 0x3452F (PUSH current_chapter_text; CALL fd2_display_dialog_scene;
 * ADD ESP,0x24; RET); reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_05.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_10__ch5_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(3);
    fd2_display_dialog_scene(current_chapter_text, 5, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}
