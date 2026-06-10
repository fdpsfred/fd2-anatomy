/*
 * chinit.c — per-chapter init handlers
 *            (data_fd2_chapter_init_handler_table entries)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_chapter_01_init @ 0x3231B  (dispatched, 0 direct callers)
 *
 * Chapter 1「初試身手」init handler. The only chapter init that
 * contains a full prologue; it plays four staged scenes by walking
 * current_chapter_id backward through three prologue map ids
 * (0x20 -> 0x1F -> 0) before the chapter-1 main battle begins.
 *
 * Phase A (map 0x20): walk-up cutscene + 2 dialog pages, stop BGM,
 *   then a state=1 cutscene transition (0x63, 0x64).
 * Phase B (still 0x20): pan camera, BGM 11, fade-in, then dialog
 *   pages 2..5 chained with cutscenes 0x65..0x69.
 * Phase C (map 0x1F): re-init battle state, load portrait sets 1/3/5,
 *   dialog pages 0..9 chained with cutscenes 0x5A..0x62, mark NPC
 *   slot 2 dead, stop BGM, state=1 cutscene 0x62.
 * Phase D (map 0): init runtime chars 0/9/4/0x1E, two party-addition
 *   appear animations with cutscenes 0/1/2, mark NPC slot 9 dead,
 *   composite a battle frame, final dialog, clear facings, pan to
 *   char 0, zero party gold.
 *
 * void __cdecl, no real params, void return. The leading
 * __CHK(0x2C) stack-probe is the Watcom-injected frame-size check
 * and is not part of the source body.
 *
 * Walkthrough SOT: assets/chapters/chapter_01.md
 * ---------------------------------------------------------------- */
void fd2_chapter_01_init(void)
{
    int i;

    /* Phase A — current_chapter_id = 0x20 (prologue map 1) */
    data_fd2_chapter_current_chapter_id = 0x20;
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(3, 0x22);
    fd2_cutscene_event_trigger(0x63);
    for (i = 0; i < 0xf; i++) {
        fd2_walk_step_up(2);
    }
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    for (i = 0; i < 0xd; i++) {
        fd2_walk_step_up(2);
    }
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_set_bgm_track_with_fade(0xffffffff, 0);
    data_fd2_chapter_cutscene_event_state = 1;
    fd2_cutscene_event_trigger(0x64);
    data_fd2_chapter_cutscene_event_state = 0;

    /* Phase B — still map 0x20; cutscene-driven transition to 0x1F */
    fd2_pan_cursor_and_window(0, 0x2b);
    fd2_set_bgm_track_with_fade(0xb, 0);
    fd2_play_palette_fade_in();
    fd2_cutscene_event_trigger(0x65);
    fd2_display_dialog_scene(current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x66);
    fd2_display_dialog_scene(current_chapter_text, 3, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x67);
    fd2_display_dialog_scene(current_chapter_text, 4, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x68);
    fd2_display_dialog_scene(current_chapter_text, 5, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_chapter_cutscene_event_state = 1;
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x69);
    data_fd2_chapter_cutscene_event_state = 0;

    /* Phase C — current_chapter_id = 0x1F (prologue map 2) */
    data_fd2_chapter_current_chapter_id = 0x1f;
    fd2_init_battle_state_for_chapter();
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(5, 0x2a);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_cutscene_event_trigger(0x5a);
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5b);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5c);
    fd2_display_dialog_scene(current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_load_chapter_portraits_and_dump_tmp(3);
    fd2_pan_cursor_and_window(4, 0x29);
    fd2_display_dialog_scene(current_chapter_text, 3, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5d);
    fd2_display_dialog_scene(current_chapter_text, 4, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_mark_char_as_dead(2);
    fd2_load_chapter_portraits_and_dump_tmp(5);
    fd2_display_dialog_scene(current_chapter_text, 5, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5e);
    fd2_display_dialog_scene(current_chapter_text, 6, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5f);
    fd2_display_dialog_scene(current_chapter_text, 7, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x60);
    fd2_display_dialog_scene(current_chapter_text, 8, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x61);
    fd2_display_dialog_scene(current_chapter_text, 9, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_set_bgm_track_with_fade(0xffffffff, 0);
    data_fd2_battle_anim_phase = 0;
    data_fd2_chapter_cutscene_event_state = 1;
    fd2_cutscene_event_trigger(0x62);
    data_fd2_chapter_cutscene_event_state = 0;

    /* Phase D — current_chapter_id = 0 (chapter 1 main battle) */
    data_fd2_chapter_current_chapter_id = 0;
    fd2_init_runtime_char_from_base_growth(0);
    fd2_init_runtime_char_from_base_growth(9);
    fd2_init_runtime_char_from_base_growth(4);
    fd2_init_runtime_char_from_base_growth(0x1e);
    fd2_init_battle_state_for_chapter();
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(4, 0xc);
    fd2_cutscene_event_trigger(0);
    __delay_thunk_375b2(200);
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_pan_cursor_and_window(0, 0);
    fd2_animate_party_addition_with_appear_effect(1);
    fd2_cutscene_event_trigger(1);
    fd2_pan_cursor_and_window(0, 0xf);
    fd2_animate_party_addition_with_appear_effect(2);
    fd2_cutscene_event_trigger(2);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(5);
    fd2_mark_char_as_dead(9);
    fd2_composite_battle_frame(0);
    __delay_thunk_375b2(100);
    fd2_display_dialog_scene(current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
    data_fd2_shared_party_total_gold = 0;
}

/* ----------------------------------------------------------------
 * fd2_chapter_02_init @ 0x32D18  (dispatched, 0 direct callers)
 *
 * Chapter 2「羅德鎮」init handler. A flat chapter-prologue
 * orchestrator: it plays four dialog pages chained with four
 * cutscenes (event ids 0x9/0xA/0xB/0xC), loads portrait sets 1/2,
 * composites one battle frame, and pans the camera, before handing
 * the chapter off to the player. There is NO char init — chapter 2
 * starts with party-only carry-over from chapter 1.
 *
 * The middle of the run sets data_fd2_chapter_init_phase_flag = 1
 * around the second portrait load (set 2) and clears it afterward;
 * data_fd2_battle_anim_phase is reset to 0 after each of the first
 * three dialog pages (page 3 has no reset — it is the tail before
 * the final camera pan).
 *
 * void __cdecl, no real params, void return. The leading
 * __CHK(0x28) stack-probe is the Watcom-injected frame-size check
 * and is not part of the source body.
 *
 * In the binary the page-3 dialog call + the final
 * fd2_pan_cursor_to_char(0) are emitted as a tail-JMP into the
 * shared epilogues of fd2_chapter_07_init (@0x33206) and
 * fd2_chapter_05_init (@0x33140); the straight-line form here is
 * the functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_02_end @ 0x22F37
 *   Post-action: fd2_chapter_02_post_action @ 0x206C5
 *                (extra lose if any of chars[5..10] dead — 6 villagers)
 *
 * Walkthrough SOT: assets/chapters/chapter_02.md
 * ---------------------------------------------------------------- */
void fd2_chapter_02_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(0xd, 0xb);
    fd2_cutscene_event_trigger(9);
    __delay_thunk_375b2(0x32);
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0xa);
    __delay_thunk_375b2(200);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_composite_battle_frame(0);
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0xb);
    fd2_display_dialog_scene(current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_and_window(6, 0xc);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(2);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0xc);
    fd2_clear_all_chars_facing();
    fd2_display_dialog_scene(current_chapter_text, 3, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_03_init @ 0x32E8C  (dispatched, 0 direct callers)
 *
 * Chapter 3「往塞拉村途中」init handler. A flat chapter-prologue
 * orchestrator that plays four dialog pages chained with three
 * cutscenes (event ids 0x12 / 0x11 / 0x13), loads portrait set 1
 * after the first cutscene, and pans the camera between scenes,
 * before handing the chapter off to the player. There is NO char
 * init — chapter 3 carries the party over from chapter 2.
 *
 * data_fd2_battle_anim_phase is reset to 0 after each of the first
 * three dialog pages (pages 0/1/2). Page 3 has no reset — it is the
 * tail before the final camera-to-char pan.
 *
 * void __cdecl, no real params, void return. The leading
 * __CHK(0x28) stack-probe is the Watcom-injected frame-size check
 * and is not part of the source body.
 *
 * In the binary the page-3 dialog call + the trailing
 * fd2_clear_all_chars_facing() and fd2_pan_cursor_to_char(0) are
 * emitted as a tail-JMP into the shared epilogue at 0x3312D (also
 * reached by the chapter init at 0x33049); the straight-line form
 * here is the functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_03_end @ 0x230F2
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_03.md
 * ---------------------------------------------------------------- */
void fd2_chapter_03_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(3, 0x11);
    __delay_thunk_375b2(200);
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x12);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_pan_cursor_and_window(3, 6);
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0x11);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x13);
    fd2_display_dialog_scene(current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(3, 0x11);
    fd2_display_dialog_scene(current_chapter_text, 3, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_04_init @ 0x32FB2  (dispatched, 0 direct callers)
 *
 * Chapter 4「塞拉村前」init handler. A flat chapter-prologue
 * orchestrator that plays two dialog pages bracketing one cutscene
 * (event id 0x14), loads portrait set 1 after page 0, and pans the
 * camera between scenes, before handing the chapter off to the
 * player. There is NO char init — chapter 4 carries the party over
 * from chapter 3.
 *
 * data_fd2_battle_anim_phase is reset to 0 after page 0 only; page 1
 * is the tail before the final camera-to-char pan and has no reset.
 *
 * void __cdecl, no real params, void return. The leading
 * __CHK(0x28) stack-probe is the Watcom-injected frame-size check
 * and is not part of the source body.
 *
 * In the binary the page-1 dialog call + the trailing
 * fd2_clear_all_chars_facing() and fd2_pan_cursor_to_char(0) are
 * emitted as a tail-JMP into the shared epilogue at 0x3312D (also
 * reached by the chapter inits at 0x33049 and chapters 26/27/29);
 * the straight-line form here is the functionally-equivalent
 * (Layer 2) reconstruction. The trailing 4 calls (page-1 dialog +
 * clear_facing + pan_cursor_to_char + ret) are themselves shared as
 * an alt-entry (0x33028) by fd2_chapter_08_init.
 *
 * Linked handlers:
 *   End:         fd2_chapter_04_end @ 0x231BC
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_04.md
 * ---------------------------------------------------------------- */
void fd2_chapter_04_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(4, 0xb);
    fd2_cutscene_event_trigger(0x14);
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_pan_cursor_and_window(4, 0);
    __delay_thunk_375b2(200);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_05_init @ 0x33049  (dispatched, 0 direct callers)
 *
 * Chapter 5「塞拉村」init handler. A flat chapter-prologue
 * orchestrator that plays three dialog pages chained with two
 * cutscenes (event ids 0x16 / 0x15), loads portrait set 1 mid-run
 * (after composing one battle frame), and pans the camera between
 * scenes, before handing the chapter off to the player. There is NO
 * char init — chapter 5 carries the party over from chapter 4.
 *
 * data_fd2_battle_anim_phase is reset to 0 after pages 0 and 1 only;
 * page 2 has no reset — it is the tail before the final
 * clear-facing + camera-to-char pan.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this function OWNS four shared alt-entry points that
 * other chapter_NN_init handlers tail-JMP into:
 *   0x3310C (cutscene 0x15 onward) — chapter_11_init, chapter_18_init.
 *   0x3312D (page-2 dialog onward) — chapter_03/04/26 (+2 more).
 *   0x3313B (clear-facing onward)  — chapter_30_init.
 *   0x33140 (pan-to-char onward)   — chapter_07_init.
 * The straight-line body here is the canonical full code path; the
 * sharing chapters reconstruct their own straight-line equivalents
 * (Layer 2) per the established pattern.
 *
 * Linked handlers:
 *   End:         fd2_chapter_05_end @ 0x231F9
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_05.md
 * ---------------------------------------------------------------- */
void fd2_chapter_05_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(3, 3);
    __delay_thunk_375b2(200);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_composite_battle_frame(0);
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0x16);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(8, 0xe);
    fd2_cutscene_event_trigger(0x15);
    fd2_display_dialog_scene(current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_06_init @ 0x3314B  (dispatched, 0 direct callers)
 *
 * Chapter 6「普里茲港」init handler. The minimal chapter init: it
 * re-inits battle state, plays exactly one dialog page (page 0), and
 * pans the camera to char 0. There is NO cutscene, NO portrait load,
 * NO char init, and NO camera-pan-and-window prelude — chapter 6
 * carries the party over from chapter 5.
 *
 * data_fd2_battle_anim_phase is reset to 0 once, before the single
 * dialog page (mirroring the entry-block MOV at 0x3315A which precedes
 * the dialog call), and there is no clear-facing before the final pan.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this function physically contains only its entry block
 * (init battle state + battle_anim_phase=0) and then tail-JMPs through
 * three shared alt-entries: 0x3344D (page-0 dialog-arg push, owned by
 * fd2_chapter_12_init) -> 0x33206 (the fd2_display_dialog_scene call,
 * owned by fd2_chapter_07_init) -> 0x33140 (the fd2_pan_cursor_to_char(0)
 * + RET tail, owned by fd2_chapter_05_init). The straight-line form here
 * is the functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_06_end @ 0x23296
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_06.md
 * ---------------------------------------------------------------- */
void fd2_chapter_06_init(void)
{
    fd2_init_battle_state_for_chapter();
    data_fd2_battle_anim_phase = 0;
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_07_init @ 0x33169  (dispatched, 0 direct callers)
 *
 * Chapter 7「往王城的途中」init handler. A flat chapter-prologue
 * orchestrator that plays two dialog pages (pages 0/1), loads
 * portrait set 1 between them, and chains two cutscenes (event ids
 * 0x1C / 0x1D) each preceded by a camera pan, before handing the
 * chapter off to the player. There is NO char init — chapter 7
 * carries the party over from chapter 6.
 *
 * data_fd2_battle_anim_phase is reset to 0 after page 0 only; page 1
 * is the tail before the final camera-to-char pan and has no reset.
 * data_fd2_chapter_init_phase_flag is set to 1 around the portrait
 * load (set 1) and cleared afterward.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the page-1 dialog call + the final
 * fd2_pan_cursor_to_char(0) are emitted as a tail-JMP into the shared
 * epilogue at 0x33140 (owned by fd2_chapter_05_init, which enters it
 * directly without a clear-facing — so there is NO
 * fd2_clear_all_chars_facing() here); the straight-line form here is
 * the functionally-equivalent (Layer 2) reconstruction. This function
 * also OWNS two shared alt-entry points that other chapter inits
 * tail-JMP into:
 *   0x331EA (post-cutscene-0x1D, page-1 dialog onward) — chapter_24.
 *   0x33206 (page-1 dialog text-arg push onward) — chapters 02 / 12.
 *
 * Linked handlers:
 *   End:         fd2_chapter_07_end @ 0x232E8
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_07.md
 * ---------------------------------------------------------------- */
void fd2_chapter_07_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(1);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_pan_cursor_and_window(8, 1);
    fd2_cutscene_event_trigger(0x1c);
    fd2_pan_cursor_and_window(8, 0);
    fd2_cutscene_event_trigger(0x1d);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_08_init @ 0x33219  (dispatched, 0 direct callers)
 *
 * Chapter 8「王城前的戰鬥」init handler. The simplest chapter init:
 * a flat chapter-prologue orchestrator that plays two dialog pages
 * (pages 0/1) bracketing two cutscenes (event ids 0x1F / 0x20), each
 * cutscene preceded by a camera pan, before handing the chapter off
 * to the player. There is NO char init, NO portrait load, and NO
 * global-state writes at all — chapter 8 carries the party over from
 * chapter 7.
 *
 * Unlike the other chapter inits, this handler never resets
 * data_fd2_battle_anim_phase (there is no MOV [0x51A83],0 anywhere on
 * its code path, not even between the two dialog pages) — it is a pure
 * sequence of void side-effect calls.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this function is a two-hop tail consumer: after pushing
 * the cutscene-0x20 arg it tail-JMPs (0x33278 -> 0x33028) into the
 * alt-entry physically owned by fd2_chapter_04_init (its trailing
 * page-1 dialog-arg push), which in turn JMPs (0x33044 -> 0x3312D) into
 * the shared epilogue owned by fd2_chapter_05_init (PUSH text; CALL
 * fd2_display_dialog_scene; clear-facing; pan_cursor_to_char(0); RET).
 * chapter_08 is the sole consumer of chapter_04's 0x33028 alt-entry and
 * owns no alt-entry of its own. The straight-line form here is the
 * functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_08_end @ 0x234BB
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_08.md
 * ---------------------------------------------------------------- */
void fd2_chapter_08_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(7, 0x20);
    fd2_cutscene_event_trigger(0x1f);
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_and_window(7, 0x17);
    fd2_cutscene_event_trigger(0x20);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_09_init @ 0x3327D  (dispatched, 0 direct callers)
 *
 * Chapter 9「騎士的抉擇」init handler. A flat chapter-prologue
 * orchestrator that turns the 11 on-field units to face north, plays
 * two dialog pages (pages 0/1) bracketing one cutscene (event id
 * 0x23), and pans the camera, before handing the chapter off to the
 * player. There is NO char init and NO portrait load — chapter 9
 * carries the party over from chapter 8.
 *
 * The leading loop walks runtime_char[0..10] and writes
 * sprite_state[1] (the facing field, struct offset +0x03) = 2 (north)
 * for all 11 units — the disassembly computes the element address as
 * base + i*0x50 + 3 (i*5 << 4). data_fd2_battle_anim_phase is reset to
 * 0 after page 0 only; page 1 is the tail before the final
 * camera-to-char pan and has no reset.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x2C)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body. The body is fully self-contained (no tail-JMP
 * into another chapter's epilogue and no alt-entry of its own); the
 * straight-line form here is the direct translation of the
 * disassembly @0x3327D.
 *
 * Linked handlers:
 *   End:         fd2_chapter_09_end @ 0x235BC
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_09.md
 * ---------------------------------------------------------------- */
void fd2_chapter_09_init(void)
{
    int i;

    fd2_init_battle_state_for_chapter();
    for (i = 0; i < 0xb; i++) {
        data_fd2_battle_runtime_char_array_ptr[i].sprite_state[1] = 2;
    }
    fd2_pan_cursor_and_window(6, 0);
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x23);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
    fd2_clear_all_chars_facing();
}

/* ----------------------------------------------------------------
 * fd2_chapter_10_init @ 0x3332B  (dispatched, 0 direct callers)
 *
 * Chapter 10「洞窟中的激戰」init handler. The first chapter init to
 * seed per-unit status: it re-inits battle state, pans the camera,
 * puts two NPC units to sleep at full sleep-counter, plays one dialog
 * page (page 0), and pans the camera to char 0. There is NO cutscene,
 * NO portrait load, and NO char init — chapter 10 carries the party
 * over from chapter 9.
 *
 * The two sleep writes set runtime_char[0x32] (索菲亞 / Sophia) and
 * runtime_char[0x33] (卡納恩三世 / Kanaan III) status_sleep_flag
 * (struct offset +0x26) = 100 — both NPCs start the battle asleep.
 * In the disassembly each write is base[0x53A45] + idx*0x50 + 0x26
 * (0xFA0 = 0x32*0x50, 0xFF0 = 0x33*0x50). There is no
 * data_fd2_battle_anim_phase reset on the code path and no
 * clear-facing before the final pan.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the page-0 dialog call + the final
 * fd2_pan_cursor_to_char(0) are emitted as a tail-JMP into the same
 * shared chain used by fd2_chapter_06_init: 0x3344D (page-0 dialog-arg
 * push, owned by fd2_chapter_12_init) -> 0x33206 (the
 * fd2_display_dialog_scene call, in fd2_chapter_07_init) -> 0x33140
 * (the fd2_pan_cursor_to_char(0) + RET, owned by fd2_chapter_05_init,
 * entered directly without a clear-facing). The straight-line form
 * here is the functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_10_end @ 0x235F9
 *   Post-action: fd2_chapter_10_post_action @ 0x20707
 *                (extra lose if char[0x32] OR char[0x33] dead —
 *                 索菲亞/卡納恩三世)
 *
 * Walkthrough SOT: assets/chapters/chapter_10.md
 * ---------------------------------------------------------------- */
void fd2_chapter_10_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(0xa, 0);
    data_fd2_battle_runtime_char_array_ptr[0x32].status_sleep_flag = 100;
    data_fd2_battle_runtime_char_array_ptr[0x33].status_sleep_flag = 100;
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_11_init @ 0x33367  (dispatched, 0 direct callers)
 *
 * Chapter 11「幻之森林」init handler. A flat chapter-prologue
 * orchestrator that plays three dialog pages (pages 0/1/2), loads
 * portrait set 1 after page 0, and chains two cutscenes (event ids
 * 0x26 / 0x27) between the pages, before handing the chapter off to
 * the player. There is NO char init — chapter 11 carries the party
 * over from chapter 10.
 *
 * data_fd2_battle_anim_phase is reset to 0 after page 0 only; pages 1
 * and 2 have no reset — page 2 is the tail before the final
 * clear-facing + camera-to-char pan.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this handler ends by pushing the cutscene-0x27 arg and
 * tail-JMPing (0x333f0 -> 0x3310c) into the shared epilogue owned by
 * fd2_chapter_05_init: the cutscene-trigger CALL, then PUSH text;
 * PUSH 2; CALL fd2_display_dialog_scene (page 2); clear-facing;
 * pan_cursor_to_char(0); RET. The straight-line form here is the
 * functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_11_end @ 0x23790
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_11.md
 * ---------------------------------------------------------------- */
void fd2_chapter_11_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_display_dialog_scene(current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(10, 7);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_cutscene_event_trigger(0x26);
    fd2_display_dialog_scene(current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_cutscene_event_trigger(0x27);
    fd2_display_dialog_scene(current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}
