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
 * fd2_chapter_event_handler_05__ch13_thunk @ 0x34D68
 *     (0 direct callers; dispatched as idx 0x05 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_06__ch2_reinforcement @ 0x34422
 *     (0 direct callers; dispatched as idx 0x06 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_08__ch13_first_time @ 0x34DCD
 *     (0 direct callers; dispatched as idx 0x08 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_09__ch3_char_cond @ 0x344C2
 *     (0 direct callers; dispatched as idx 0x09 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_0a__ch14_first_time @ 0x34E3B
 *     (0 direct callers; dispatched as idx 0x0A of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_0b__ch4_dialog @ 0x34565
 *     (0 direct callers; dispatched as idx 0x0B of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_0c__unref_first_time @ 0x34594
 *     (0 direct callers; dispatched as idx 0x0C of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_0d__ch15_dialog_with_state @ 0x34E90
 *     (0 direct callers; dispatched as idx 0x0D of the per-event
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
 * fd2_chapter_event_handler_11__ch5_dialog_with_state @ 0x346C8
 *     (0 direct callers; dispatched as idx 0x11 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_12__ch15_dialog_with_state @ 0x34F02
 *     (0 direct callers; dispatched as idx 0x12 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_13__unref_char_cond @ 0x34716
 *     (0 direct callers; dispatched as idx 0x13 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_14__ch6_dialog @ 0x347B1
 *     (0 direct callers; dispatched as idx 0x14 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_15__ch6_char_cond @ 0x347D9
 *     (0 direct callers; dispatched as idx 0x15 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_16__ch6_char_cond @ 0x34819
 *     (0 direct callers; dispatched as idx 0x16 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_17__unref_turn_gated @ 0x34844
 *     (0 direct callers; dispatched as idx 0x17 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_18__unref_dialog @ 0x348FC
 *     (0 direct callers; dispatched as idx 0x18 of the per-event
 *      handler table at 0x51B91)
 * fd2_show_chapter_intro_text_dialog_mode_3 @ 0x34906
 *     (1 caller: fd2_chapter_event_handler_16__ch6_char_cond @ 0x34819)
 * fd2_chapter_event_handler_19__ch7_first_time @ 0x34924
 *     (0 direct callers; dispatched as idx 0x19 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_1a__ch7_char_cond @ 0x3499B
 *     (0 direct callers; dispatched as idx 0x1A of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_1c__ch8_ai_ctrl @ 0x34A0E
 *     (1 caller: fd2_chapter_event_handler_1d__unref_dialog_with_state
 *      @ 0x34A3C; also dispatched as idx 0x1C of the per-event handler
 *      table at 0x51B91)
 * fd2_chapter_event_handler_1d__unref_dialog_with_state @ 0x34A3C
 *     (0 direct callers; dispatched as idx 0x1D of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_1e__unref_major_cinematic @ 0x34A7A
 *     (0 direct callers; dispatched as idx 0x1E of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_1f__ch9_reinforcement @ 0x34B5D
 *     (0 direct callers; dispatched as idx 0x1F of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_20__ch10_dialog @ 0x34BE2
 *     (0 direct callers; dispatched as idx 0x20 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_21__ch10_dialog_with_state @ 0x34C1E
 *     (0 direct callers; dispatched as idx 0x21 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_22__unref_dialog @ 0x34C6C
 *     (0 direct callers; dispatched as idx 0x22 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_23__ch12_cinematic @ 0x34C76
 *     (0 direct callers; dispatched as idx 0x23 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_24__ch12_ai_ctrl @ 0x34CB3
 *     (0 direct callers; dispatched as idx 0x24 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_25__unref_major_cinematic @ 0x34CCC
 *     (0 direct callers; dispatched as idx 0x25 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_27__unref_drop @ 0x34F74
 *     (0 direct callers; dispatched as idx 0x27 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_28__ch17_dialog_with_state @ 0x34FCB
 *     (0 direct callers; dispatched as idx 0x28 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_29__unref_drop @ 0x34FF0
 *     (0 direct callers; dispatched as idx 0x29 of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_2a__ch18_dialog @ 0x3505F
 *     (0 direct callers; dispatched as idx 0x2A of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_2b__ch18_ai_ctrl @ 0x35091
 *     (0 direct callers; dispatched as idx 0x2B of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_2c__ch19_ai_ctrl @ 0x350A4
 *     (0 direct callers; dispatched as idx 0x2C of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_2d__ch19_ai_ctrl @ 0x350B9
 *     (0 direct callers; dispatched as idx 0x2D of the per-event
 *      handler table at 0x51B91)
 * fd2_chapter_event_handler_2e__ch19_reinforcement @ 0x350CC
 *     (0 direct callers; dispatched as idx 0x2E of the per-event
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
    fd2_delay_ms(100);
    fd2_cutscene_event_trigger(7);
    fd2_clear_keyboard_buffer();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;

    fd2_load_chapter_portraits_and_dump_tmp(7);
    fd2_composite_battle_frame(1);
    fd2_delay_ms(100);
    fd2_cutscene_event_trigger(8);
    fd2_clear_keyboard_buffer();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xA0000, 0x140,
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
 * into the shared tail of handler_01 (PUSH data_fd2_current_chapter_text;
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xA0000, 0x140,
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
 * into the shared tail of handler_01 (PUSH data_fd2_current_chapter_text;
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xA0000, 0x140,
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
 * tail at 0x343FA (PUSH page=7..PUSH data_fd2_current_chapter_text; CALL
 * fd2_display_dialog_scene; ADD ESP,0x24; RET) that
 * fd2_chapter_event_handler_11 @ 0x346C8 JMPs into for its own
 * page-7 dialog; reproduced here as the inline call for Layer-2
 * equivalence.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_04__unref_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    data_fd2_battle_runtime_char_array_ptr[0xD].team = 1;
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xA0000, 0x140,
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
    fd2_delay_ms(100);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(3);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0xD);
    fd2_delay_ms(200);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);

    for (i = 5; i < 0xB; i++) {
        data_fd2_battle_runtime_char_array_ptr[i].combat_aux_block[0xE] = 0x1A;
        data_fd2_battle_runtime_char_array_ptr[i].combat_aux_block[0xF] = 0x0F;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_07__ch13_dialog_with_state @ 0x34D72
 *   — Chapter 13 turn-event slot 1 (triggered at turn 9 / phase 0),
 *     dispatched as idx 0x07 of the per-event handler table at 0x51B91.
 *
 * ch13 turn-9 beat: the camera pans to world (0x1B, 5), portrait set 2
 * reloads — bracketed by setting data_fd2_chapter_init_phase_flag to 1
 * before the reload and back to 0 after, so the reload is treated as an
 * "init phase" load — cutscene event 0x2E plays, every character's facing
 * is reset, and dialog page 8 is shown. A straight-line, no-branch
 * sequence with no RNG, no numeric computation, and no CALL-return value
 * used.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler prepares its own 8 PUSHes (page=8
 * plus the fixed dialog geometry) and then JMPs (0x34DC8 -> 0x34C0F) into
 * the shared tail of fd2_show_chapter_dialog_with_portrait_set_1 (PUSH
 * data_fd2_current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24; RET);
 * reproduced here as the inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_13.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_07__ch13_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_pan_cursor_and_window(0x1B, 5);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(2);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0x2E);
    fd2_clear_all_chars_facing();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_08__ch13_first_time @ 0x34DCD
 *   — Chapter 13 tile-step event slot 0 (tile-step event_type 0x01),
 *     dispatched as idx 0x08 of the per-event handler table at 0x51B91.
 *
 * first-time-gated tile-step item pickup. The dispatch arg is the id of
 * the char who stepped onto the trigger tile; the beat fires only when the
 * lord (char 0) steps on it, the lord's inventory is not full, and the
 * trigger has not yet been consumed. When all three gates pass it gives the
 * lord item id 0x59, shows dialog page 0xB, and consumes the trigger so it
 * runs at most once. The three gates are: stepping_char_id == 0,
 * fd2_count_usable_inventory_slots(0) != 8 (the count of the lord's used
 * slots — 8 means full), and tile-event slot [0x10] == 0. No RNG, no numeric
 * computation; the only CALL-return value used is the slot count.
 *
 * void __cdecl(uint stepping_char_id) per the tile-step dispatch hooks: the
 * stepping char id arrives as a single stack arg (the table at 0x51B91 is
 * uniform 1-arg cdecl, but tile-step slots pass the stepping char id rather
 * than the unread turn-event arg). EBX is not touched; the __CHK(0x28)
 * stack-probe prologue is compiler-injected and omitted here. Inside the
 * char-0 gate stepping_char_id is provably 0, so it is the value passed to
 * both inventory calls (matching the original's PUSH of the stack arg).
 *
 * The pointer global data_fd2_field_map_tile_event_consumed_flags_ptr holds
 * the base of the 0x20-byte tile-event consumed-flags block; this handler's
 * trigger is byte [0x10] of that block (read as the gate, written 1 to
 * consume).
 *
 * Walkthrough SOT: assets/chapters/chapter_13.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_08__ch13_first_time(uint32 stepping_char_id)
{
    if (stepping_char_id == 0) {
        if (fd2_count_usable_inventory_slots(stepping_char_id) != 8 &&
            *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) == 0) {
            fd2_add_item_to_inventory(stepping_char_id, 0x59);
            fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                                     0xCD, 0x4C, 0x4A, 0x13, 1);
            *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) = 1;
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_09__ch3_char_cond @ 0x344C2
 *   — Chapter 3 turn-event slot 0 (triggered at turn 3 / phase 2),
 *     dispatched as idx 0x09 of the per-event handler table at 0x51B91.
 *
 * char-conditional beat: gated on the ch3 ally swordsman 鐵諾
 * (runtime char #6, the NPC the party protects this battle) still being
 * alive (flags bit0 clear). If alive, portrait set 2 reloads, the camera
 * pans from world (3, 0) to (3, 0x11) with an ~800ms / ~200ms hold
 * between the two pans, and dialog page 4 is shown. If 鐵諾 is already
 * dead the whole beat is skipped. (Surviving runtime char #6 is later
 * recruited as the permanent party slot char #2 by fd2_chapter_03_end,
 * which gates on the same fd2_check_char_is_dead(6) test.)
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary the trailing dialog call is the head of a
 * shared tail at 0x34516 (PUSH page=4 .. PUSH data_fd2_current_chapter_text;
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
        fd2_delay_ms(800);
        fd2_pan_cursor_and_window(3, 0x11);
        fd2_delay_ms(200);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xA0000, 0x140,
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
 * tail at 0x3452F (PUSH data_fd2_current_chapter_text; CALL fd2_display_dialog_scene;
 * ADD ESP,0x24; RET); reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_04.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_0b__ch4_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(2);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xA0000, 0x140,
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
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_0d__ch15_dialog_with_state @ 0x34E90
 *   — Chapter 15 turn-event slot 0 (triggered at turn 4 / phase 1),
 *     dispatched as idx 0x0D of the per-event handler table at 0x51B91.
 *
 * ch15 turn-4 beat: dialog page 6 is shown first, then the boss group
 * (runtime-char slots 0x40..0x49, 10 chars) is armed for AI mode 3 in
 * two steps — every slot's AI param byte combat_aux_block[0xE] is preset
 * to 0, then the low nibble of combat_aux_block[0xD] is set to 3 across
 * the same range — and finally the per-event AI/dialog control flag (low
 * 4 bits of combat_aux_block[0xD]) is disarmed by writing 0 across chars
 * 0x23..0x31 (15 mid-tier chars). A straight-line, no-branch sequence
 * with no RNG, no numeric computation, and no CALL-return value used.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is
 * callee-saved; the __CHK(0x2C) stack-probe prologue is compiler-injected
 * and omitted here. fd2_display_dialog_scene's uint32 return is discarded.
 *
 * Walkthrough SOT: assets/chapters/chapter_15.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_0d__ch15_dialog_with_state(uint32 event_arg)
{
    int32 i;

    (void)event_arg;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);

    for (i = 0x40; i < 0x4A; i++) {
        data_fd2_battle_runtime_char_array_ptr[i].combat_aux_block[0xE] = 0;
    }
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x40, 0x49, 3);
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x23, 0x31, 0);
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
 * tail at 0x3452F (PUSH data_fd2_current_chapter_text; CALL fd2_display_dialog_scene;
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
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
 * tail at 0x34516 (PUSH data_fd2_current_chapter_text; CALL fd2_display_dialog_scene;
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xA0000, 0x140,
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
 * tail at 0x3452F (PUSH data_fd2_current_chapter_text; CALL fd2_display_dialog_scene;
 * ADD ESP,0x24; RET); reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_05.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_10__ch5_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(3);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_11__ch5_dialog_with_state @ 0x346C8
 *   — Chapter 5 turn-event slot 3 (triggered at turn 8 / phase 0),
 *     dispatched as idx 0x11 of the per-event handler table at 0x51B91.
 *
 * ch5 turn-8 beat: a straight-line, no-branch sequence (no RNG, no
 * numeric computation, and no CALL-return value used). It arms the
 * per-event AI/dialog control flag (low 4 bits of combat_aux_block[0xD])
 * to 7 across chars 0x30..0x33 (4 chars), shows dialog page 6, plays
 * cutscene event 0x18, then shows dialog page 7.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary the trailing page-7 dialog call is reached by
 * a JMP into the shared tail of handler_04 at 0x343FA (PUSH page=7 ..
 * PUSH data_fd2_current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24;
 * RET); reproduced here as the inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_05.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_11__ch5_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x30, 0x33, 7);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_cutscene_event_trigger(0x18);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_12__ch15_dialog_with_state @ 0x34F02
 *   — Chapter 15 turn-event slot 1 (triggered at turn 9 / phase 0),
 *     dispatched as idx 0x12 of the per-event handler table at 0x51B91.
 *
 * ch15 turn-9 beat: dialog page 8 is shown, then the per-event AI/dialog
 * control flag (low 4 bits of combat_aux_block[0xD]) is disarmed by
 * writing 0 across chars 0x10..0x22 (19 chars). A straight-line, no-branch
 * sequence with no RNG, no numeric computation, and no CALL-return value
 * used (fd2_display_dialog_scene's uint32 return is discarded).
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this function also HOSTS two class-3 shared
 * tails that other handlers JMP into to reuse this AI-range call:
 *   @ 0x34F37 (PUSH 0x10; CALL fd2_set_combat_aux_block_byte_d_low4_for_
 *     char_range; ADD ESP,0xC; RET) — shared by handlers 0x2B (ch18),
 *     0x2D (ch19), 0x54 (ch27), which pre-push the type and end args.
 *   @ 0x34F39 (CALL ...; ADD ESP,0xC; RET) — shared by handlers 0x2C
 *     (ch19) and 0x30 (ch21), which pre-push all three args themselves.
 * Those tails are an in-binary layout artifact of code-folding and are
 * reproduced here only as this function's own inline call; the consuming
 * handlers emit their own equivalent calls for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_15.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_12__ch15_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x10, 0x22, 0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_13__unref_char_cond @ 0x34716
 *   -- Dispatch idx 0x13 of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced -- possibly cut content / non-chapter dispatcher). It is
 * a char-conditional beat that arms an AI flag across a wide character
 * band, shows a dialog page unconditionally, then re-scans the same band
 * and shows a second dialog page only if any of those characters is still
 * alive:
 *   set_combat_aux_block_byte_d_low4_for_char_range(7, 0x24, 7);
 *   display_dialog_scene(page 8, ...);
 *   any_alive = false;
 *   for (i = 7; i < 0x25; i++):
 *     if (check_char_is_dead(i) == 0): any_alive = true;
 *   if (any_alive):
 *     display_dialog_scene(page 0xB, ...);
 * No RNG and no numeric computation; the only branch is the any_alive
 * gate driven by the fd2_check_char_is_dead return value.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is
 * callee-saved; the __CHK(0x30) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * The alive scan walks all 30 chars (0x07..0x24) even after the first
 * alive one is found -- there is no early break in the original; the loop
 * just keeps re-setting the flag. Reproduced faithfully here for Layer-2
 * equivalence.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_13__unref_char_cond(uint32 event_arg)
{
    uint8 any_alive;
    uint32 i;

    (void)event_arg;

    any_alive = 0;
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(7, 0x24, 7);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    for (i = 7; (int32)i < 0x25; i++) {
        if (fd2_check_char_is_dead(i) == 0) {
            any_alive = 1;
        }
    }
    if (any_alive != 0) {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_14__ch6_dialog @ 0x347B1
 *   — Chapter 6 turn-event slot 0 (triggered at turn 5 / phase 2),
 *     dispatched as idx 0x14 of the per-event handler table at 0x51B91.
 *
 * The minimal dialog-only beat: no portrait reload, no camera pan, no
 * state writes — a single straight-line dialog call with no RNG, no
 * numeric computation, and no CALL-return value used. Dialog page 1 is
 * shown; nothing else happens.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler prepares its own 8 PUSHes (page=1
 * plus the fixed dialog geometry) and then JMPs into handler_09's shared
 * tail at 0x3452F (PUSH data_fd2_current_chapter_text; CALL fd2_display_dialog_scene;
 * ADD ESP,0x24; RET); reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_06.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_14__ch6_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_15__ch6_char_cond @ 0x347D9
 *   — Chapter 6 turn-event slot 1 (triggered at turn 10 / phase 2),
 *     dispatched as idx 0x15 of the per-event handler table at 0x51B91.
 *
 * char-conditional beat: gated on 索倫 (char_id 8) still being alive
 * (flags bit0 clear). If alive, dialog page 2 is shown; if 索倫 is
 * already dead the beat is skipped. The only branch is the alive gate
 * driven by the fd2_check_char_is_dead return value — no RNG, no numeric
 * computation, and the call's return value is used only as a zero test.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary the trailing dialog call is the head of a
 * shared tail at 0x347F1 (PUSH page=2 .. PUSH data_fd2_current_chapter_text;
 * CALL fd2_display_dialog_scene; ADD ESP,0x24; RET) that
 * fd2_chapter_event_handler_32 (ch22 reinforcement) JMPs into for its
 * own page-2 dialog; reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_06.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_15__ch6_char_cond(uint32 event_arg)
{
    (void)event_arg;

    if (fd2_check_char_is_dead(8) == 0) {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_16__ch6_char_cond @ 0x34819
 *   — Chapter 6 turn-event slot 2 (triggered at turn 15 / phase 2),
 *     dispatched as idx 0x16 of the per-event handler table at 0x51B91.
 *
 * char-conditional beat: gated on 索倫 (char_id 8) still being alive
 * (flags bit0 clear). If alive, portrait set 1 reloads and the chapter
 * intro dialog page 3 is shown; if 索倫 is already dead the beat is
 * skipped. The only branch is the alive gate driven by the
 * fd2_check_char_is_dead return value — no RNG, no numeric computation,
 * and the call's return value is used only as a zero test.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * Unlike the other char-conditional handlers in this group, the
 * original does NOT inline the dialog call: it tail-JMPs (0x3483F ->
 * 0x34906) to the named helper fd2_show_chapter_intro_text_dialog_mode_3
 * @ 0x34906, which is the sole consumer of that helper. The "dead"
 * branch (JNZ 0x34C1D) falls into a bare RET borrowed from the shared
 * tail of fd2_show_chapter_dialog_with_portrait_set_1, i.e. a plain
 * return; reproduced here as the call to the named helper for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_06.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_16__ch6_char_cond(uint32 event_arg)
{
    (void)event_arg;

    if (fd2_check_char_is_dead(8) == 0) {
        fd2_load_chapter_portraits_and_dump_tmp(1);
        fd2_show_chapter_intro_text_dialog_mode_3();
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_17__unref_turn_gated @ 0x34844
 *   — Dispatch idx 0x17 of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced — possibly cut content / non-chapter dispatcher). It is
 * a turn-counter-gated beat. Its unconditional head disarms the
 * per-event AI/dialog control flag (low 4 bits of combat_aux_block[0xD])
 * by writing 0 across chars 0x08..0x1C (21 chars) and shows dialog
 * page 4. Then, only while the battle turn counter is still below 0x0F
 * (i.e. before turn 15), it runs a two-cutscene boss-death cinematic:
 * portrait set 2 reloads, the camera pans to world (5, 0x11), cutscene
 * event 0x19 plays, dialog page 5 is shown, the camera pans to (5, 0x11)
 * again, cutscene event 0x1A plays, char 0x21 (狄歐?) is killed, and the
 * battle-animation phase is flipped to 1.
 *
 * The gate (data_fd2_battle_turn_counter < 0x0F) is a signed compare in
 * the original (CMP [0x53BEF],0xF; JGE); reproduced as the (int32) cast
 * here. There is no RNG and no numeric computation; no CALL-return value
 * is used.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary the if-true branch JMPs into the shared __CHK
 * epilogue tail at 0x35C18 (MOV [0x51A83],1; RET), so the final
 * battle_anim_phase = 1 store lives in that shared tail; the gate-fail
 * path JGEs straight to the bare RET at 0x35C22. Reproduced here as the
 * inline store inside the gated block for Layer-2 equivalence.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_17__unref_turn_gated(uint32 event_arg)
{
    (void)event_arg;

    fd2_set_combat_aux_block_byte_d_low4_for_char_range(8, 0x1C, 0);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    if ((int32)data_fd2_battle_turn_counter < 0xF) {
        fd2_load_chapter_portraits_and_dump_tmp(2);
        fd2_pan_cursor_and_window(5, 0x11);
        fd2_cutscene_event_trigger(0x19);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_pan_cursor_and_window(5, 0x11);
        fd2_cutscene_event_trigger(0x1A);
        fd2_mark_char_as_dead(0x21);
        data_fd2_battle_anim_phase = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_18__unref_dialog @ 0x348FC
 *   — Dispatch idx 0x18 of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced — possibly cut content / non-chapter dispatcher). Its
 * single beat is the minimal dialog-only call: a straight-line, no-branch
 * sequence with no portrait reload, no camera pan, no state writes, no
 * RNG, no numeric computation, and no CALL-return value used — it just
 * shows dialog page 3 and returns.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler prepares its own 8 PUSHes (page=3
 * plus the fixed dialog geometry) and then JMPs (0x3491F -> 0x34C0F) into
 * the shared tail of fd2_show_chapter_dialog_with_portrait_set_1 (PUSH
 * data_fd2_current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24; RET);
 * reproduced here as the inline call for Layer-2 equivalence. The handler
 * also exposes a Class-3 shared entry at 0x34901 (the CALL __CHK
 * instruction): fd2_chapter_event_handler_22 @ 0x34C6C borrows the entire
 * body via "PUSH 0x28; JMP 0x34901", reusing this __CHK + page-3 dialog
 * + RET unchanged.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_18__unref_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_show_chapter_intro_text_dialog_mode_3 @ 0x34906
 *   — Named helper that shows data_fd2_current_chapter_text dialog page 3 with
 *     the standard dialog geometry. Its sole caller is
 *     fd2_chapter_event_handler_16__ch6_char_cond @ 0x34819, which
 *     tail-JMPs here (0x3483F -> 0x34906) when 索倫 (char_id 8) is
 *     still alive at chapter 6 turn 15.
 *
 * void __cdecl(void); no stack frame and no __CHK probe. EBX is not
 * touched. The body is a pure 8-PUSH chain (page=3 plus the fixed
 * dialog geometry) followed by a JMP (0x3491F -> 0x34C0F) into the
 * shared tail of fd2_show_chapter_dialog_with_portrait_set_1 (PUSH
 * data_fd2_current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24;
 * RET). The borrowed tail supplies the 9th argument
 * (data_fd2_current_chapter_text) and performs the cdecl 0x24-byte (9-arg)
 * cleanup; reproduced here as the inline call for Layer-2 equivalence.
 *
 * Magic numbers (matching every dialog call in this group):
 *   0xA0000 VGA framebuffer base, 0x140 (=320) row stride,
 *   0xCD/0x4C dialog window position (X, Y), 0x4A charset/style code,
 *   0x13 (=19) max line count, 1 wait-for-input flag.
 *
 * Walkthrough SOT: assets/chapters/chapter_06.md
 * ---------------------------------------------------------------- */
void fd2_show_chapter_intro_text_dialog_mode_3(void)
{
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_19__ch7_first_time @ 0x34924
 *   -- Chapter 7 turn-event slot 0 (triggered at turn 10 / phase 0),
 *     dispatched as idx 0x19 of the per-event handler table at 0x51B91.
 *
 * A first-time-gated SECOND-STAGE beat: unlike the first-time handlers
 * whose gate fires while their slot is still 0, this one runs only AFTER
 * a prior event (tile-event slot 0x10) has been consumed (its byte set to
 * 1) -- i.e. it is the second half of a two-stage trigger. Once it fires it
 * consumes its OWN slot (byte [0x11] set to 1), so it runs at most once.
 *
 * When the gate passes its single beat is: portrait set 2 reloads --
 * bracketed by setting data_fd2_chapter_init_phase_flag to 1 before the
 * reload and back to 0 after, so the reload is treated as an "init phase"
 * load -- the camera pans to world (0x10, 10), cutscene event 0x1E plays,
 * dialog page 2 is shown, and finally tile-event slot 0x11 is consumed.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * The pointer global data_fd2_field_map_tile_event_consumed_flags_ptr
 * holds the base of the 0x20-byte tile-event consumed-flags block; the
 * second-stage gate flag is byte [0x10] of that block and this handler's
 * own consumed flag is byte [0x11].
 *
 * Walkthrough SOT: assets/chapters/chapter_07.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_19__ch7_first_time(uint32 event_arg)
{
    (void)event_arg;

    if (*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) == 1) {
        data_fd2_chapter_init_phase_flag = 1;
        fd2_load_chapter_portraits_and_dump_tmp(2);
        data_fd2_chapter_init_phase_flag = 0;
        fd2_pan_cursor_and_window(0x10, 10);
        fd2_cutscene_event_trigger(0x1E);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x11) = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_1a__ch7_char_cond @ 0x3499B
 *   -- Chapter 7 tile-step event slot 0 (tile-step event_type 0x00),
 *     dispatched as idx 0x1A of the per-event handler table at 0x51B91.
 *
 * char-conditional, tile-step variant. Unlike the turn-event handlers in
 * this group (which ignore their dispatch arg), this tile-step handler
 * reads the dispatch arg as the id of the char who stepped onto the
 * trigger tile and gates on that char's team: the beat fires only when a
 * non-enemy (team != 0, i.e. an NPC or player unit) steps on the tile.
 * When it fires it disarms the per-event AI/dialog control flag (low 4
 * bits of combat_aux_block[0xD]) by writing 0 across chars 0x09..0x1B
 * (19 chars) and consumes tile-event slot 0x10. No dialog. The only
 * branch is the team gate; no RNG, no numeric computation, and no
 * CALL-return value is used.
 *
 * void __cdecl(uint stepping_char_id) per the tile-step dispatch hooks:
 * the stepping char id arrives as a single stack arg (the table at
 * 0x51B91 is uniform 1-arg cdecl, but tile-step slots pass the stepping
 * char id rather than the unread turn-event arg). EBX is not touched; the
 * __CHK(0x10) stack-probe prologue is compiler-injected and omitted here.
 *
 * The pointer global data_fd2_field_map_tile_event_consumed_flags_ptr
 * holds the base of the 0x20-byte tile-event consumed-flags block; this
 * handler consumes byte [0x10] of that block (the same slot whose
 * consumption gates the second-stage handler_19 @ 0x34924).
 *
 * Walkthrough SOT: assets/chapters/chapter_07.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_1a__ch7_char_cond(uint32 stepping_char_id)
{
    if (data_fd2_battle_runtime_char_array_ptr[stepping_char_id].team != 0) {
        fd2_set_combat_aux_block_byte_d_low4_for_char_range(9, 0x1B, 0);
        *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_1b__ch8_cinematic @ 0x349D9
 *   — Chapter 8 every-turn cinematic, dispatched as idx 0x1B of the
 *     per-event handler table at 0x51B91. Bound to turn-event slots 0-5
 *     (turns 2, 3, 4, 5, 6, 7, all phase 0), so it fires once on every one
 *     of chapter 8's first six player turns.
 *
 * cinematic, no dialog. A straight-line beat with no branch, no RNG, no
 * numeric computation and no CALL-return value used:
 *   fd2_pan_cursor_and_window(8, 2);                            // pan camera to (8,2)
 *   fd2_delay_ms(100);                                   // ~100ms hold
 *   fd2_load_chapter_portraits_and_dump_tmp(turn_counter);      // reload portrait set
 *   fd2_delay_ms(100);                                   // ~100ms hold
 *
 * The portrait set reloaded each turn is keyed off the battle turn counter
 * (data_fd2_battle_turn_counter @ 0x53BEF), which fd2_run_full_turn_cycle
 * increments by one at the start of every new player turn. Because the
 * counter advances between turns, each of turns 2..7 passes a different
 * value as the portrait loader's target_race_id and so selects a different
 * NPC portrait pose — producing an animated NPC sequence across the six
 * turns.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91 (1-arg
 * uniform cdecl); the body never reads the arg. EBX is not touched; the
 * __CHK(0xC) stack-probe prologue is compiler-injected and omitted here.
 *
 * In the binary the final 100ms hold is emitted as "PUSH 0x64; JMP 0x353D1":
 * a tail-jump into the shared CALL fd2_delay_ms / ADD ESP,4 / RET
 * tail of fd2_delay_400ms_via_idle_thunk (0x353CC..0x353D9). The borrowed
 * tail performs the cdecl 4-byte cleanup and RET; reproduced here as the
 * inline fd2_delay_ms(100) call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_08.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_1b__ch8_cinematic(uint32 event_arg)
{
    (void)event_arg;

    fd2_pan_cursor_and_window(8, 2);
    fd2_delay_ms(100);
    fd2_load_chapter_portraits_and_dump_tmp(data_fd2_battle_turn_counter);
    fd2_delay_ms(100);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_1c__ch8_ai_ctrl @ 0x34A0E
 *   -- Chapter 8 turn-event slot 6 (triggered at turn 15 / phase 0),
 *     dispatched as idx 0x1C of the per-event handler table at 0x51B91.
 *     Also tail-called from
 *     fd2_chapter_event_handler_1d__unref_dialog_with_state @ 0x34A3C.
 *
 * ch8 turn-15 AI-control beat: for the 18 runtime-char slots 0x0A..0x1B,
 * clear bits 0-6 of combat_aux_block[0xD] (the AI-class / sub-state byte),
 * preserving only bit 7 (the high "locked" bit):
 *   for (i = 0; i < 0x12; i++)
 *       runtime_char[i + 10].combat_aux_block[0xD] &= 0x80;
 *
 * No dialog, no RNG, no CALL-return value used; a single masked write per
 * slot. (This clears the low 7 bits, unlike
 * fd2_set_combat_aux_block_byte_d_low4_for_char_range, which only touches
 * the low 4 bits.)
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91 (1-arg
 * uniform cdecl); the body never reads the arg. EBX is callee-saved; the
 * __CHK(8) stack-probe prologue is compiler-injected and omitted here.
 *
 * Walkthrough SOT: assets/chapters/chapter_08.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_1c__ch8_ai_ctrl(uint32 event_arg)
{
    int32 i;

    (void)event_arg;

    for (i = 0; i < 0x12; i++) {
        data_fd2_battle_runtime_char_array_ptr[i + 10].combat_aux_block[0xD] &=
            0x80;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_1d__unref_dialog_with_state @ 0x34A3C
 *   — Dispatch idx 0x1D of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced — possibly cut content or a non-chapter dispatcher).
 * Two-beat handler: show dialog page 2, then tail-chain to
 * fd2_chapter_event_handler_1c__ch8_ai_ctrl (the ch8 AI-control beat
 * that clears the low 7 bits of combat_aux_block[0xD] for the 18
 * runtime-char slots 0x0A..0x1B).
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl). EBX is not touched; the __CHK(0x28)
 * stack-probe prologue is compiler-injected and omitted here.
 *
 * Unlike the sibling dialog handlers (which discard event_arg), this
 * one forwards event_arg unchanged to handler_1c: the original tail is
 * PUSH dword ptr [ESP+4]; CALL fd2_chapter_event_handler_1c; ADD ESP,4;
 * RET. (handler_1c ignores the value, but the pass-through is kept for
 * byte-faithful equivalence.)
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_1d__unref_dialog_with_state(uint32 event_arg)
{
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_chapter_event_handler_1c__ch8_ai_ctrl(event_arg);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_1e__unref_major_cinematic @ 0x34A7A
 *   — Dispatch idx 0x1E of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced — possibly cut content or a non-chapter dispatcher).
 * Major-cinematic beat with a character spawn and dynamic turn-event
 * scheduling:
 *   - Clear the AI/sub-state byte (combat_aux_block[0xD]) for the 22
 *     runtime-char slots 0x0C..0x21.
 *   - Schedule two future turn events relative to the current battle
 *     turn counter: the trigger-turn byte of table entry +3 is set to
 *     turn_counter+1 and that of entry +6 to turn_counter+2 (entries
 *     begin at byte offset +3 with a 3-byte stride inside the table
 *     pointed to by data_fd2_tile_event_data_table_ptr).
 *   - Spawn / configure runtime-char slot 0x0B on the NPC/ally side
 *     (team=1; 0=enemy 1=npc 2=player) as 萊汀 (char_id 6, portrait_id 6):
 *     clear its flags byte (revive if dead), set combat_aux_block[0x0A]=0xFF,
 *     combat_aux_block[0x0D]=0x80 (AI byte with the locked bit 7 set), and
 *     hp_current=1 (spawn at 1 HP).
 *   - Show dialog page 2, reload portrait set 1, show dialog page 3.
 *   - Reset pending XP (data_fd2_battle_pending_xp_credit = 0).
 *   - Consume tile-event slot 0x10 with value 2 (distinct from the "1"
 *     written by the other handlers).
 *
 * The only branch is the AI-clear loop; no RNG, no numeric computation,
 * and no CALL-return value is used.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91 (1-arg
 * uniform cdecl); the body never reads the arg. EBX is callee-saved; the
 * __CHK(0x2C) stack-probe prologue is compiler-injected and omitted here.
 *
 * The byte read from data_fd2_battle_turn_counter for the two scheduled
 * turns is a byte read + byte increment in the original (MOV DL,[..];
 * INC DL / ADD DL,2), reproduced here as a (uint8) truncating cast.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_1e__unref_major_cinematic(uint32 event_arg)
{
    int32 i;
    runtime_char *p;

    (void)event_arg;

    for (i = 0xC; i < 0x22; i++) {
        data_fd2_battle_runtime_char_array_ptr[i].combat_aux_block[0xD] = 0;
    }

    *((uint8 *)data_fd2_tile_event_data_table_ptr + 3) =
        (uint8)(data_fd2_battle_turn_counter + 1);
    *((uint8 *)data_fd2_tile_event_data_table_ptr + 6) =
        (uint8)(data_fd2_battle_turn_counter + 2);

    p = data_fd2_battle_runtime_char_array_ptr;
    p[0xB].flags = 0;
    p[0xB].team = 1;
    p[0xB].portrait_id = 6;
    p[0xB].char_id = 6;
    p[0xB].combat_aux_block[0x0A] = 0xFF;
    p[0xB].combat_aux_block[0x0D] = 0x80;
    p[0xB].hp_current = 1;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);

    data_fd2_battle_pending_xp_credit = 0;
    *((uint8 *)data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) = 2;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_1f__ch9_reinforcement @ 0x34B5D
 *   — Chapter-9 reinforcement-spawner cinematic, dispatched as idx
 *     0x1F of the per-event handler table at 0x51B91.
 *
 * Activated by chapter 9 turn-event slots 0+1 (the race_id 0+1 batch
 * reinforcements). The batch counter is byte [0x10] of the tile-event
 * consumed-flags block: the current value selects which portrait batch
 * is loaded (and dumped to FD2.TMP), then the counter is advanced so the
 * next invocation loads the following batch. After loading, the camera
 * performs a 4-corner scan (TL -> TR -> BR -> BL) holding ~200ms at each
 * corner, revealing the spawn arrivals at the four map corners. No dialog
 * and no cutscene trigger — a pure visual transition.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0xC) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * The pointer global data_fd2_field_map_tile_event_consumed_flags_ptr
 * holds the base of the 0x20-byte tile-event consumed-flags block; the
 * batch counter is byte [0x10] of that block.
 *
 * In the original binary the final ~200ms hold is the head of a shared
 * tail at 0x353D1 (CALL fd2_delay_ms; ADD ESP,4; RET) that this
 * handler reaches via "PUSH 0xC8; JMP 0x353D1"; reproduced here as the
 * inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_09.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_1f__ch9_reinforcement(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(
        *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10));
    (*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10))++;

    fd2_pan_cursor_and_window(0, 0);
    fd2_delay_ms(200);
    fd2_pan_cursor_and_window(0xC, 0);
    fd2_delay_ms(200);
    fd2_pan_cursor_and_window(0xC, 0xB);
    fd2_delay_ms(200);
    fd2_pan_cursor_and_window(0, 0xB);
    fd2_delay_ms(200);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_20__ch10_dialog @ 0x34BE2
 *   -- Chapter 10 turn-event slot 0 (triggered at turn 5 / phase 1).
 *
 * ch10 reinforcement-arrival beat: when the player's 5th turn ends the
 * reinforcements (援軍) appear, and this dialog-only handler shows the
 * accompanying line. Its single beat reloads portrait set 1 and shows
 * dialog page 1 -- a straight-line, no-branch sequence with no camera
 * pan, no cutscene trigger, no state writes, no RNG, no numeric
 * computation, and no CALL-return value used.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler is a 5-byte adapter stub
 * (PUSH 0x28) that falls through (no JMP; 0x34BE2 -> 0x34BE7) into the
 * shared body fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7,
 * which begins with the CALL __CHK that consumes the pushed 0x28 frame
 * size, then runs fd2_load_chapter_portraits_and_dump_tmp(1) and the
 * page-1 dialog call before returning. That shared body has no params
 * and expects every caller to push __CHK arg 0x28 first; the sibling
 * handler_05 @ 0x34D68 reaches the same body via "PUSH 0x28; JMP". The
 * shared body's effect is reproduced inline here for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_10.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_20__ch10_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7
 *   (1 caller: fd2_chapter_event_handler_05__ch13_thunk @ 0x34D68)
 *
 * Shared portrait+dialog body: reload portrait set 1, then show
 * data_fd2_current_chapter_text dialog page 1 with the standard dialog
 * geometry. A straight-line, no-branch sequence with no camera pan,
 * no cutscene trigger, no state writes, no RNG, no numeric
 * computation, and no CALL-return value used.
 *
 * void __cdecl(void). EBX is not touched; the __CHK(0x28) stack-probe
 * prologue is compiler-injected and omitted here (the original body
 * starts directly with CALL __CHK and expects every entry path to push
 * the 0x28 frame size first).
 *
 * Two entry paths reach this body in the original binary, each pushing
 * the 0x28 __CHK frame size first:
 *   - fd2_chapter_event_handler_05__ch13_thunk @ 0x34D68 tail-JMPs here
 *     ("PUSH 0x28; JMP 0x34BE7") for its ch13 beat — emitted as a call
 *     to this helper.
 *   - fd2_chapter_event_handler_20__ch10_dialog @ 0x34BE2 falls through
 *     ("PUSH 0x28; 0x34BE2 -> 0x34BE7") for its ch10 beat — emitted with
 *     this body's effect reproduced inline.
 *
 * The original body also hosts three Class-3 shared entry points borrowed
 * by other handlers, all reproduced inline in their respective consumers
 * for Layer-2 equivalence:
 *   - 0x34BF6 (+0x0F): start of the 8-PUSH chain (page=1 plus the fixed
 *     dialog geometry) — fd2_chapter_event_handler_28 reaches it for its
 *     own page-1 dialog.
 *   - 0x34C0F (+0x28): the "PUSH data_fd2_current_chapter_text; CALL
 *     fd2_display_dialog_scene; ADD ESP,0x24; RET" tail —
 *     fd2_show_chapter_intro_text_dialog_mode_3 (page=3),
 *     fd2_chapter_event_handler_07 (ch13), fd2_chapter_event_handler_26
 *     (ch15) and one more JMP into it as their dialog tail.
 *   - 0x34C1D (+0x36): the bare RET — fd2_chapter_event_handler_16 (ch6)
 *     uses it as a "ret-only" path join.
 *
 * Magic numbers (matching every dialog call in this group):
 *   0xA0000 VGA framebuffer base, 0x140 (=320) row stride,
 *   0xCD/0x4C dialog window position (X, Y), 0x4A charset/style code,
 *   0x13 (=19) max line count, 1 wait-for-input flag.
 *
 * Walkthrough SOT: assets/chapters/chapter_10.md (ch10 reinforcement
 * arrival) and assets/chapters/chapter_13.md (ch13).
 * ---------------------------------------------------------------- */
void fd2_show_chapter_dialog_with_portrait_set_1(void)
{
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_05__ch13_thunk @ 0x34D68
 *   — Chapter 13 turn-event slot (哈斯米爾之戰 / Battle of Hasmir).
 *
 * ch13 beat: reload portrait set 1, then show data_fd2_current_chapter_text
 * dialog page 1 with the standard dialog geometry — the exact effect
 * of fd2_show_chapter_dialog_with_portrait_set_1. A straight-line,
 * no-branch sequence with no camera pan, no cutscene trigger, no state
 * writes, no RNG, no numeric computation, and no CALL-return value used.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched.
 *
 * In the original binary this handler is a 7-byte adapter stub
 * ("PUSH 0x28; JMP 0x34BE7") that tail-jumps into the shared body
 * fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7. The pushed
 * 0x28 is the __CHK stack-probe frame size that the shared body's
 * compiler-injected CALL __CHK prologue consumes on entry; that probe
 * is omitted here, so the thunk is emitted as a direct call to the
 * shared helper. The sibling handler_20 @ 0x34BE2 reaches the same body
 * by falling through ("PUSH 0x28; 0x34BE2 -> 0x34BE7").
 *
 * Walkthrough SOT: assets/chapters/chapter_13.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_05__ch13_thunk(uint32 event_arg)
{
    (void)event_arg;

    fd2_show_chapter_dialog_with_portrait_set_1();
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_21__ch10_dialog_with_state @ 0x34C1E
 *   — Chapter 10 turn-event slot 1 (triggered at the end of turn 20).
 *
 * ch10 turn-20 beat (walkthrough: 第二十回合結束時，敵軍開始攻擊國王和
 * 索菲亞): dialog page 2 is shown, then the AI-class byte
 * (combat_aux_block[0xD]) of the two protected NPC units 0x0C and 0x0D
 * is cleared to 0. Zeroing the AI-class flips both units out of their
 * passive guard behaviour so the enemy host begins attacking them on
 * the following turns. A straight-line, no-branch sequence with no
 * camera pan, no cutscene trigger, no RNG, no numeric computation, and
 * no CALL-return value used.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * The two state writes are byte stores at struct offset 0x34
 * (combat_aux_block[0xD]) of runtime_char_array[0x0C] (base +0x3C0)
 * and runtime_char_array[0x0D] (base +0x410).
 *
 * Magic numbers (matching every dialog call in this group):
 *   0xA0000 VGA framebuffer base, 0x140 (=320) row stride,
 *   0xCD/0x4C dialog window position (X, Y), 0x4A charset/style code,
 *   0x13 (=19) max line count, 1 wait-for-input flag.
 *
 * Walkthrough SOT: assets/chapters/chapter_10.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_21__ch10_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_runtime_char_array_ptr[0xC].combat_aux_block[0xD] = 0;
    data_fd2_battle_runtime_char_array_ptr[0xD].combat_aux_block[0xD] = 0;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_22__unref_dialog @ 0x34C6C
 *   -- Dispatch idx 0x22 of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced -- possibly cut content / non-chapter dispatcher). Its
 * single beat is the minimal dialog-only call: a straight-line, no-branch
 * sequence with no portrait reload, no camera pan, no state writes, no
 * RNG, no numeric computation, and no CALL-return value used -- it just
 * shows dialog page 3 and returns. Its effect is identical to
 * fd2_chapter_event_handler_18__unref_dialog @ 0x348FC.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler is a 7-byte Class-3 shared-body
 * consumer: its entire body is "PUSH 0x28; JMP 0x34901", borrowing the
 * shared entry of fd2_chapter_event_handler_18 @ 0x34901 (the CALL __CHK
 * instruction). It thereby reuses handler_18's __CHK probe, the 8-PUSH
 * chain (page=3 plus the fixed dialog geometry) at 0x34906, and the
 * JMP (0x3491F -> 0x34C0F) into the shared tail of
 * fd2_show_chapter_dialog_with_portrait_set_1 (PUSH data_fd2_current_chapter_text;
 * CALL fd2_display_dialog_scene; ADD ESP,0x24; RET) unchanged. The whole
 * borrowed body's effect is reproduced here as the inline page-3 dialog
 * call for Layer-2 equivalence.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_22__unref_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_23__ch12_cinematic @ 0x34C76
 *   — Chapter 12 turn-event slot 0 (triggered at turn 1 / phase 0),
 *     dispatched as idx 0x23 of the per-event handler table at 0x51B91.
 *
 * ch12 turn-1 cinematic, no dialog (walkthrough: 第一回合己方結束時敵方
 * 第一波援軍出現在右上洞口，龍劍士米亞斯多德加入). A straight-line beat
 * with no branch, no RNG, no numeric computation, and no CALL-return value
 * used: the camera pans to world (0xC, 5), portrait set 2 reloads —
 * bracketed by setting data_fd2_chapter_init_phase_flag to 1 before the
 * reload and back to 0 after, so the reload is treated as an "init phase"
 * load — cutscene event 0x2A plays, and every character's facing is reset.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0xC) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary the trailing facing-reset is a tail-JMP
 * (0x34CAE -> 0x000134E4) into fd2_clear_all_chars_facing (void, no args);
 * reproduced here as the inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_12.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_23__ch12_cinematic(uint32 event_arg)
{
    (void)event_arg;

    fd2_pan_cursor_and_window(0xC, 5);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(2);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0x2A);
    fd2_clear_all_chars_facing();
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_24__ch12_ai_ctrl @ 0x34CB3
 *   — Chapter 12 turn-event slot 1 (triggered at turn 5 / phase 1),
 *     dispatched as idx 0x24 of the per-event handler table at 0x51B91.
 *
 * ch12 turn-5 AI-setup beat. A single straight-line write with no
 * branch, no RNG, no numeric computation, and no CALL-return value
 * used: character 0x0E's AI-class byte (combat_aux_block[0xD]) is set
 * to 0x83 — bit 7 locked plus low bits 0/1 selecting AI mode 3. No
 * other side effects.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(4) stack-probe prologue is compiler-injected and
 * omitted here.
 *
 * Walkthrough SOT: assets/chapters/chapter_12.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_24__ch12_ai_ctrl(uint32 event_arg)
{
    (void)event_arg;

    data_fd2_battle_runtime_char_array_ptr[0xE].combat_aux_block[0xD] = 0x83;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_25__unref_major_cinematic @ 0x34CCC
 *   -- Dispatch idx 0x25 of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced -- possibly cut content / non-chapter dispatcher),
 * categorised as a major endgame cinematic. A straight-line beat with no
 * branch, no RNG, no numeric computation, and no CALL-return value used:
 * dialog page 1 is shown, then a two-stage cutscene cinematic plays.
 *
 *   Stage A: the camera pans to world (0xF, 0x22), portrait set 3 reloads
 *     -- bracketed by setting data_fd2_chapter_init_phase_flag to 1 before
 *     the reload and back to 0 after, so it is treated as an "init phase"
 *     load -- cutscene event 0x2B plays, and every character's facing is
 *     reset.
 *   Stage B: the camera pans to world (0, 0x1A), portrait set 4 reloads
 *     (same init-phase bracket), cutscene event 0x2C plays, and every
 *     character's facing is reset again.
 *
 * It closes by flipping the battle-animation phase to 1.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary the closing battle_anim_phase = 1 store is
 * reached by a tail-JMP (0x34D63 -> 0x35C18) into the shared __CHK
 * epilogue tail at 0x35C18 (MOV [0x51A83],1; RET) -- the same shared tail
 * fd2_chapter_event_handler_17 @ 0x34844 jumps into. Reproduced here as
 * the inline store for Layer-2 equivalence.
 *
 * Magic numbers (matching every dialog call in this group):
 *   0xA0000 VGA framebuffer base, 0x140 (=320) row stride,
 *   0xCD/0x4C dialog window position (X, Y), 0x4A charset/style code,
 *   0x13 (=19) max line count, 1 wait-for-input flag.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_25__unref_major_cinematic(uint32 event_arg)
{
    (void)event_arg;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);

    fd2_pan_cursor_and_window(0xF, 0x22);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(3);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0x2B);
    fd2_clear_all_chars_facing();

    fd2_pan_cursor_and_window(0, 0x1A);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(4);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0x2C);
    fd2_clear_all_chars_facing();

    data_fd2_battle_anim_phase = 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_0a__ch14_first_time @ 0x34E3B
 *   -- Dispatch idx 0x0A of the per-event handler table at 0x51B91.
 *
 * Triggered in chapter 14 as tile-step event_type 0x00 (ch14 tile-step
 * slot 0). First-time-gated: the body runs only while
 * tile_event_consumed_flags[0x10] is still 0, and consuming the flag
 * (set to 1) at the end makes every later call a no-op. Its single beat
 * disarms the AI flag on 56 chars -- the low nibble of combat_aux_block[0xD]
 * becomes 0 for chars 0x10..0x47 (the largest range in this group) -- and
 * then shows dialog page 1.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here. fd2_display_dialog_scene's uint32 return is discarded.
 *
 * The pointer global data_fd2_field_map_tile_event_consumed_flags_ptr
 * holds the base of the 0x20-byte tile-event consumed-flags block; the
 * gate flag is byte [0x10] of that block.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_0a__ch14_first_time(uint32 event_arg)
{
    (void)event_arg;

    if (*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) == 0) {
        fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x10, 0x47, 0);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x10) = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_26__ch15_dialog @ 0x34F42
 *   — Chapter 15 turn-event slot 2 (triggered at turn 7 / phase 0),
 *     dispatched as idx 0x26 of the per-event handler table at 0x51B91.
 *
 * ch15 turn-7 dialog-only beat: reload portrait set 1, then show dialog
 * page 10 (0xA). A straight-line, no-branch sequence with no camera pan,
 * no cutscene trigger, no state writes beyond the portrait reload, no RNG,
 * no numeric computation, and no CALL-return value used. Its effect is the
 * page-10 twin of fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7
 * (which shows page 1).
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler prepares its own 8 PUSHes (page=0xA
 * plus the fixed dialog geometry) and then JMPs (0x34F6F -> 0x34C0F) into
 * the shared tail of fd2_show_chapter_dialog_with_portrait_set_1 (PUSH
 * data_fd2_current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24; RET);
 * reproduced here as the inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_15.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_26__ch15_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xA, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* Inline 3-byte battle-drop entry blob baked into the binary at 0x52742,
 * read only by fd2_chapter_event_handler_27__unref_drop. Layout matches the
 * drop-entry ABI fd2_process_battle_drop_entries consumes: byte[0] = entry
 * type (0 = ITEM pickup), byte[1..2] = little-endian uint16 value. Here the
 * value 0x00D3 is item id 0xD3, so the handler grants item 0xD3 to the
 * stepping char (when that char is on the player team). */
const uint8 data_fd2_chapter_event_handler_27_drop_entry_inline[3] = {
    0x00, 0xD3, 0x00
};

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_27__unref_drop @ 0x34F74
 *   — Dispatch idx 0x27 of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced — possibly cut content / non-chapter dispatcher),
 * categorised as battle drop + dialog. It runs the standard tile-step
 * ABI: the dispatch arg is the id of the char who stepped onto the
 * trigger tile, and that id is the recipient of the drop. Its single
 * beat copies the inline 3-byte drop-entry blob
 * (data_fd2_chapter_event_handler_27_drop_entry_inline = {0x00, 0xD3,
 * 0x00}, i.e. type 0 / item id 0xD3) onto a local, hands it as a
 * one-entry array to fd2_process_battle_drop_entries(recipient, 1,
 * &blob), then shows dialog page 0xB. A straight-line, no-branch
 * sequence with no RNG, no numeric computation, and no CALL-return value
 * used (both calls are void-context; the dialog's uint32 return is
 * discarded).
 *
 * fd2_process_battle_drop_entries only opens the drop-reward dialog when
 * the recipient is on the player team (team == 2); for an enemy/NPC
 * recipient it returns after reading the entry, so the item dialog is
 * suppressed but the entry is still consumed.
 *
 * void __cdecl(uint stepping_char_id) per the tile-step dispatch hooks:
 * the stepping char id arrives as a single stack arg (the table at
 * 0x51B91 is uniform 1-arg cdecl, but tile-step slots pass the stepping
 * char id rather than the unread turn-event arg). EBX is not touched; the
 * __CHK(0x34) stack-probe prologue is compiler-injected and omitted here.
 *
 * In the original binary the inline blob is copied byte-for-byte into a
 * 4-byte stack slot (MOVSW + MOVSB over the 3 used bytes); reproduced
 * here as the three byte copies into drop_entry[3]. The final page-0xB
 * dialog is emitted inline; the original ends with the cdecl cleanup +
 * register restore that this function also hosts as the shared tails
 * alt_43 (@ 0x34FB7) and alt_51 (@ 0x34FC5) borrowed by handlers 0x29 /
 * 0x33 / 0x37 — those tails are an in-binary code-folding artifact and
 * are reproduced only as this function's own inline call; the consuming
 * handlers emit their own equivalent calls for Layer-2 equivalence.
 *
 * Magic numbers (matching every dialog call in this group):
 *   0xA0000 VGA framebuffer base, 0x140 (=320) row stride,
 *   0xCD/0x4C dialog window position (X, Y), 0x4A charset/style code,
 *   0x13 (=19) max line count, 1 wait-for-input flag.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_27__unref_drop(uint32 stepping_char_id)
{
    uint8 drop_entry[3];

    drop_entry[0] = data_fd2_chapter_event_handler_27_drop_entry_inline[0];
    drop_entry[1] = data_fd2_chapter_event_handler_27_drop_entry_inline[1];
    drop_entry[2] = data_fd2_chapter_event_handler_27_drop_entry_inline[2];
    fd2_process_battle_drop_entries(stepping_char_id, 1, (uint32)drop_entry);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_28__ch17_dialog_with_state @ 0x34FCB
 *   — Chapter 17 turn-event slot 0 (triggered at turn 4 / phase 1),
 *     dispatched as idx 0x28 of the per-event handler table at 0x51B91.
 *
 * ch17 turn-4 beat: reload portrait set 2, pan the camera to (0x11,
 * 0x25), then show dialog page 1 with the standard dialog geometry. A
 * straight-line, no-branch sequence with no cutscene trigger, no state
 * writes beyond the portrait reload and the camera pan, no RNG, no
 * numeric computation, and no CALL-return value used.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler runs its own prefix
 * (fd2_load_chapter_portraits_and_dump_tmp(2); fd2_pan_cursor_and_window(
 * 0x11, 0x25)) and then JMPs (0x34FEB -> 0x34BF6) into the +0x0F shared
 * entry point of fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7:
 * the start of its 8-PUSH chain (page=1 plus the fixed dialog geometry)
 * followed by the "PUSH data_fd2_current_chapter_text; CALL fd2_display_dialog_scene;
 * ADD ESP,0x24; RET" tail. The JMP skips that body's own portrait reload
 * (PUSH 1; CALL fd2_load_chapter_portraits_and_dump_tmp at 0x34BEC) because
 * this handler has already reloaded portrait set 2. The borrowed body's
 * effect — the page-1 dialog — is reproduced here as the inline call for
 * Layer-2 equivalence.
 *
 * Magic numbers (matching every dialog call in this group):
 *   0xA0000 VGA framebuffer base, 0x140 (=320) row stride,
 *   0xCD/0x4C dialog window position (X, Y), 0x4A charset/style code,
 *   0x13 (=19) max line count, 1 wait-for-input flag.
 *
 * Walkthrough SOT: assets/chapters/chapter_17.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_28__ch17_dialog_with_state(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(2);
    fd2_pan_cursor_and_window(0x11, 0x25);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* Inline 3-byte battle-drop entry blob baked into the binary at 0x52745,
 * read only by fd2_chapter_event_handler_29__unref_drop. Layout matches the
 * drop-entry ABI fd2_process_battle_drop_entries consumes: byte[0] = entry
 * type (0 = ITEM pickup), byte[1..2] = little-endian uint16 value. Here the
 * value 0x00D5 is item id 0xD5 (冰之眼, the "enter hidden chapter" item), so
 * the handler grants item 0xD5 to the stepping char (when that char is on the
 * player team). */
const uint8 data_fd2_chapter_event_handler_29_drop_entry_inline[3] = {
    0x00, 0xD5, 0x00
};

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_29__unref_drop @ 0x34FF0
 *   — Dispatch idx 0x29 of the per-event handler table at 0x51B91.
 *
 * No chapter FDFIELD turn-event / tile-step hook references this slot
 * (unreferenced — possibly cut content / non-chapter dispatcher),
 * categorised as dialog + battle drop + dialog. It runs the standard
 * tile-step ABI: the dispatch arg is the id of the char who stepped onto
 * the trigger tile, and that id is the recipient of the drop. Its beat
 * shows dialog page 3, copies the inline 3-byte drop-entry blob
 * (data_fd2_chapter_event_handler_29_drop_entry_inline = {0x00, 0xD5,
 * 0x00}, i.e. type 0 / item id 0xD5) onto a local, hands it as a one-entry
 * array to fd2_process_battle_drop_entries(recipient, 1, &blob), then shows
 * dialog page 4. A straight-line, no-branch sequence with no RNG, no
 * numeric computation, and no CALL-return value used (all three calls are
 * void-context; each dialog's uint32 return is discarded).
 *
 * fd2_process_battle_drop_entries only opens the drop-reward dialog when
 * the recipient is on the player team (team == 2); for an enemy/NPC
 * recipient it returns after reading the entry, so the item dialog is
 * suppressed but the entry is still consumed.
 *
 * void __cdecl(uint stepping_char_id) per the tile-step dispatch hooks:
 * the stepping char id arrives as a single stack arg (the table at
 * 0x51B91 is uniform 1-arg cdecl, but tile-step slots pass the stepping
 * char id rather than the unread turn-event arg). EBX is not touched; the
 * __CHK(0x34) stack-probe prologue is compiler-injected and omitted here.
 *
 * In the original binary the inline blob is copied byte-for-byte into a
 * 4-byte stack slot (MOVSW + MOVSB over the 3 used bytes); reproduced here
 * as the three byte copies into drop_entry[3]. The page-3 dialog and the
 * drop call are emitted inline; the handler then pre-pushes its own 8 args
 * (page=4 plus the fixed dialog geometry) and JMPs (0x3505A -> 0x34FB7)
 * into the shared display_dialog tail alt_43 hosted in
 * fd2_chapter_event_handler_27 (PUSH data_fd2_current_chapter_text; CALL
 * fd2_display_dialog_scene; ADD ESP,0x24; ADD ESP,4; POP EDI; POP ESI;
 * RET). That borrowed tail is an in-binary code-folding artifact; its
 * effect — the page-4 dialog plus cdecl cleanup + register restore — is
 * reproduced here as the inline page-4 dialog call for Layer-2 equivalence.
 *
 * Magic numbers (matching every dialog call in this group):
 *   0xA0000 VGA framebuffer base, 0x140 (=320) row stride,
 *   0xCD/0x4C dialog window position (X, Y), 0x4A charset/style code,
 *   0x13 (=19) max line count, 1 wait-for-input flag.
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_29__unref_drop(uint32 stepping_char_id)
{
    uint8 drop_entry[3];

    drop_entry[0] = data_fd2_chapter_event_handler_29_drop_entry_inline[0];
    drop_entry[1] = data_fd2_chapter_event_handler_29_drop_entry_inline[1];
    drop_entry[2] = data_fd2_chapter_event_handler_29_drop_entry_inline[2];
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_process_battle_drop_entries(stepping_char_id, 1, (uint32)drop_entry);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_2a__ch18_dialog @ 0x3505F
 *   — Chapter 18 turn-event slot 1 (triggered at turn 8 / phase 0),
 *     dispatched as idx 0x2A of the per-event handler table at 0x51B91.
 *
 * ch18 turn-8 beat: the dialog that accompanies the enemy reinforcement
 * wave appearing on the left at the end of the player's 8th turn. Portrait
 * set 1 reloads, then dialog page 6 is shown. A straight-line, no-branch
 * sequence with no cutscene trigger, no camera pan, no state writes beyond
 * the portrait reload, no RNG, no numeric computation, and no CALL-return
 * value used (fd2_display_dialog_scene's uint32 return is discarded).
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91
 * (1-arg uniform cdecl); the body never reads the arg. EBX is not
 * touched; the __CHK(0x28) stack-probe prologue is compiler-injected
 * and omitted here.
 *
 * In the original binary this handler reloads portrait set 1, prepares its
 * own 8 PUSHes (page=6 plus the fixed dialog geometry), and then JMPs
 * (0x3508C -> 0x34C0F) into the "PUSH data_fd2_current_chapter_text; CALL
 * fd2_display_dialog_scene; ADD ESP,0x24; RET" tail of
 * fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7 (entry +0x28).
 * That borrowed tail is an in-binary code-folding artifact; its effect —
 * the page-6 dialog — is reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Magic numbers (matching every dialog call in this group):
 *   0xA0000 VGA framebuffer base, 0x140 (=320) row stride,
 *   0xCD/0x4C dialog window position (X, Y), 0x4A charset/style code,
 *   0x13 (=19) max line count, 1 wait-for-input flag.
 *
 * Walkthrough SOT: assets/chapters/chapter_18.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_2a__ch18_dialog(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_2b__ch18_ai_ctrl @ 0x35091
 *   — Chapter 18 turn-event slot 0 (triggered at turn 3 / phase 0),
 *     dispatched as idx 0x2B of the per-event handler table at 0x51B91.
 *
 * ch18 turn-3 AI-control beat: set the per-event AI/dialog control flag
 * (low 4 bits of combat_aux_block[0xD]) to 3 for the single runtime-char
 * slot 0x10 (the range [0x10, 0x10] is inclusive and one char wide):
 *   fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x10, 0x10, 3);
 *
 * No dialog, no RNG, no numeric computation, no CALL-return value used;
 * straight-line single call.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91 (1-arg
 * uniform cdecl); the body never reads the arg. EBX is not touched; the
 * __CHK(0x10) stack-probe prologue is compiler-injected and omitted here.
 *
 * In the original binary this handler pre-pushes the flag (3) and end
 * (0x10) args, then JMPs (0x3509F -> 0x34F37) into the class-3 shared tail
 * hosted by fd2_chapter_event_handler_12__ch15_dialog_with_state @ 0x34F02
 * (the tail "PUSH 0x10 start; CALL fd2_set_combat_aux_block_byte_d_low4_
 * for_char_range; ADD ESP,0xC; RET" fixes the start arg at 0x10). That
 * borrowed tail is an in-binary code-folding artifact; its effect — the
 * single-char AI-flag write — is reproduced here as the inline call for
 * Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_18.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_2b__ch18_ai_ctrl(uint32 event_arg)
{
    (void)event_arg;

    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x10, 0x10, 3);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_2c__ch19_ai_ctrl @ 0x350A4
 *   — Chapter 19 turn-event handler, dispatched as idx 0x2C of the
 *     per-event handler table at 0x51B91.
 *
 * ch19 AI-control beat: set the per-event AI/dialog control flag
 * (low 4 bits of combat_aux_block[0xD]) to 3 for the runtime-char
 * range [0x1D, 0x3B] inclusive (31 chars):
 *   fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x1D, 0x3B, 3);
 *
 * No dialog, no RNG, no numeric computation, no CALL-return value used;
 * straight-line single call.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91 (1-arg
 * uniform cdecl); the body never reads the arg. EBX is not touched; the
 * __CHK(0x10) stack-probe prologue is compiler-injected and omitted here.
 *
 * In the original binary this handler pre-pushes all three args (start
 * 0x1D, end 0x3B, flag 3) then JMPs (0x350B4 -> 0x34F39) into the class-3
 * shared tail hosted by fd2_chapter_event_handler_12__ch15_dialog_with_state
 * @ 0x34F02. The borrowed tail is just "CALL fd2_set_combat_aux_block_byte_
 * d_low4_for_char_range; ADD ESP,0xC; RET" (tighter than the 0x2B variant,
 * which lets the tail supply the start arg). That borrowed tail is an
 * in-binary code-folding artifact; its effect — the AI-flag range write —
 * is reproduced here as the inline call for Layer-2 equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_19.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_2c__ch19_ai_ctrl(uint32 event_arg)
{
    (void)event_arg;

    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x1D, 0x3B, 3);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_2d__ch19_ai_ctrl @ 0x350B9
 *   — Chapter 19 turn-event handler, dispatched as idx 0x2D of the
 *     per-event handler table at 0x51B91.
 *
 * ch19 AI-control beat: set the per-event AI/dialog control flag
 * (low 4 bits of combat_aux_block[0xD]) to 3 for the runtime-char
 * range [0x10, 0x1F] inclusive (16 chars):
 *   fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x10, 0x1F, 3);
 *
 * No dialog, no RNG, no numeric computation, no CALL-return value used;
 * straight-line single call.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91 (1-arg
 * uniform cdecl); the body never reads the arg. EBX is not touched; the
 * __CHK(0x10) stack-probe prologue is compiler-injected and omitted here.
 *
 * In the original binary this handler pre-pushes the flag (3) and end
 * (0x1F) args, then JMPs (0x350C7 -> 0x34F37) into the class-3 shared tail
 * hosted by fd2_chapter_event_handler_12__ch15_dialog_with_state @ 0x34F02
 * (the tail "PUSH 0x10 start; CALL fd2_set_combat_aux_block_byte_d_low4_
 * for_char_range; ADD ESP,0xC; RET" fixes the start arg at 0x10). That
 * borrowed tail is an in-binary code-folding artifact; its effect — the
 * AI-flag range write — is reproduced here as the inline call for Layer-2
 * equivalence.
 *
 * Walkthrough SOT: assets/chapters/chapter_19.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_2d__ch19_ai_ctrl(uint32 event_arg)
{
    (void)event_arg;

    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x10, 0x1F, 3);
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_2e__ch19_reinforcement @ 0x350CC
 *   — Chapter 19 turn-event slot 1 (triggered at turn 6 / phase 1),
 *     dispatched as idx 0x2E of the per-event handler table at 0x51B91.
 *
 * ch19 turn-6 reinforcement beat: a straight-line, no-branch sequence
 * (no RNG, no numeric computation, no CALL-return value used):
 *   - reload portrait set 1 via fd2_load_chapter_portraits_and_dump_tmp(1);
 *   - show dialog page 1;
 *   - recruit char_id 0x1B as reinforcement via
 *     fd2_init_runtime_char_from_base_growth(0x1B) — appends one template
 *     slot (team=2, char_id at +7/+8) to the menu-party roster and
 *     increments the member count.
 *
 * void __cdecl(uint event_arg) per the dispatch table at 0x51B91 (1-arg
 * uniform cdecl); the body never reads the arg. EBX is not touched; the
 * __CHK(0x28) stack-probe prologue is compiler-injected and omitted here.
 *
 * Walkthrough SOT: assets/chapters/chapter_19.md
 * ---------------------------------------------------------------- */
void fd2_chapter_event_handler_2e__ch19_reinforcement(uint32 event_arg)
{
    (void)event_arg;

    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_init_runtime_char_from_base_growth(0x1B);
}

/* ----------------------------------------------------------------
 * data_fd2_chapter_init_phase_flag @ 0x53AFA (.object2, 1 byte)
 *
 * Single-byte chapter-init phase flag. Spawn-placement mode toggle
 * read by fd2_init_runtime_char_for_battle @ 0x10CB7: when the flag
 * is 0 it runs the nearest-available-tile search to place a newly
 * spawned battle char; when nonzero it uses the desired x/y from the
 * chapter portrait-load record as-is (no search).
 *
 * The chapter init/event handlers in this file (and the chinit.c
 * chapter init handlers) bracket their mid-chapter portrait reloads
 * with `flag = 1` ... `flag = 0`, so the value is 0 at rest. The
 * binary image is zero at this address and the first runtime touch
 * is a write, hence a zero-initialized tentative definition.
 *
 * Accessed strictly as a byte (MOV byte ptr [0x53AFA], imm8 by every
 * writer; byte compare-to-0 by the reader).
 * ---------------------------------------------------------------- */
uint8 data_fd2_chapter_init_phase_flag;
