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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    for (i = 0; i < 0xd; i++) {
        fd2_walk_step_up(2);
    }
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x66);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x67);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x68);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5b);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5c);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_load_chapter_portraits_and_dump_tmp(3);
    fd2_pan_cursor_and_window(4, 0x29);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5d);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_mark_char_as_dead(2);
    fd2_load_chapter_portraits_and_dump_tmp(5);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5e);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x5f);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x60);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x61);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_pan_cursor_and_window(0, 0);
    fd2_animate_party_addition_with_appear_effect(1);
    fd2_cutscene_event_trigger(1);
    fd2_pan_cursor_and_window(0, 0xf);
    fd2_animate_party_addition_with_appear_effect(2);
    fd2_cutscene_event_trigger(2);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(5);
    fd2_mark_char_as_dead(9);
    fd2_composite_battle_frame(0);
    __delay_thunk_375b2(100);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0xa);
    __delay_thunk_375b2(200);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_composite_battle_frame(0);
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0xb);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_and_window(6, 0xc);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(2);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0xc);
    fd2_clear_all_chars_facing();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x12);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_pan_cursor_and_window(3, 6);
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0x11);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x13);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(3, 0x11);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_pan_cursor_and_window(4, 0);
    __delay_thunk_375b2(200);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(3, 3);
    __delay_thunk_375b2(200);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_composite_battle_frame(0);
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0x16);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(8, 0xe);
    fd2_cutscene_event_trigger(0x15);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(1);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_pan_cursor_and_window(8, 1);
    fd2_cutscene_event_trigger(0x1c);
    fd2_pan_cursor_and_window(8, 0);
    fd2_cutscene_event_trigger(0x1d);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_and_window(7, 0x17);
    fd2_cutscene_event_trigger(0x20);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x23);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(10, 7);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_cutscene_event_trigger(0x26);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_cutscene_event_trigger(0x27);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_12_init @ 0x333F5  (dispatched, 0 direct callers)
 *
 * Chapter 12「北山道」init handler. A flat chapter-prologue
 * orchestrator that is CUTSCENE-FIRST (unlike chapters 02..11, which
 * lead with a dialog page): it loads portrait set 1, plays two
 * cutscenes (event ids 0x28 / 0x29) each preceded by a camera pan,
 * clears all facings, then plays a single dialog page (page 0) before
 * the final camera-to-char pan. There is NO char init — chapter 12
 * carries the party over from chapter 11.
 *
 * data_fd2_chapter_init_phase_flag is set to 1 around the portrait
 * load (set 1) and cleared afterward. There is NO
 * data_fd2_battle_anim_phase reset anywhere on this handler's code
 * path (no MOV [0x51A83],0): page 0 is the sole, tail dialog page.
 * Note the clear-facing happens BEFORE the dialog page here (between
 * cutscene 0x29 and the dialog), not after it.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the page-0 dialog call + the final
 * fd2_pan_cursor_to_char(0) are emitted as a tail-JMP from this
 * handler's own dialog-arg push (0x3344D) into the shared epilogue at
 * 0x33206 (owned by fd2_chapter_07_init: PUSH data_fd2_current_chapter_text;
 * CALL fd2_display_dialog_scene; then JMP 0x33140, owned by
 * fd2_chapter_05_init: pan_cursor_to_char(0); RET — entered directly
 * without a clear-facing, hence the clear-facing sits earlier here).
 * The straight-line form here is the functionally-equivalent (Layer 2)
 * reconstruction. This function itself OWNS two shared alt-entry
 * points that other chapter inits tail-JMP into:
 *   0x33440 (cutscene-0x29 CALL onward) — chapter_22_init.
 *   0x3344D (page-0 dialog-arg push onward) — the shared page-0
 *           dialog tail used by chapters 06 / 10 / 13 / 14 / 17.
 *
 * Linked handlers:
 *   End:         fd2_chapter_12_end @ 0x237D5
 *   Post-action: fd2_chapter_12_post_action @ 0x2073D
 *                (extra lose if char[0xE] dead — 米亞斯多德)
 *
 * Walkthrough SOT: assets/chapters/chapter_12.md
 * ---------------------------------------------------------------- */
void fd2_chapter_12_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(4, 4);
    data_fd2_chapter_init_phase_flag = 1;
    fd2_load_chapter_portraits_and_dump_tmp(1);
    data_fd2_chapter_init_phase_flag = 0;
    fd2_cutscene_event_trigger(0x28);
    fd2_pan_cursor_and_window(0xb, 0x28);
    fd2_cutscene_event_trigger(0x29);
    fd2_clear_all_chars_facing();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_13_init @ 0x3346B  (dispatched, 0 direct callers)
 *
 * Chapter 13「哈斯米爾之戰」init handler — the smallest chapter init
 * in the game (17 bytes). It only re-inits battle state, plays a
 * single dialog page (page 0), and pans the camera to char 0. There
 * is NO cutscene, NO portrait load, NO char init, NO camera-pan-and-
 * window prelude, NO data_fd2_battle_anim_phase reset, and NO
 * clear-facing — chapter 13 carries the party over from chapter 12.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this handler physically contains only its entry block
 * (init battle state) and then tail-JMPs through the same shared chain
 * used by fd2_chapter_06_init / fd2_chapter_10_init: 0x3344D (page-0
 * dialog-arg push, owned by fd2_chapter_12_init) -> 0x33206 (the
 * fd2_display_dialog_scene call, owned by fd2_chapter_07_init) ->
 * 0x33140 (the fd2_pan_cursor_to_char(0) + RET tail, owned by
 * fd2_chapter_05_init, entered directly without a clear-facing). The
 * straight-line form here is the functionally-equivalent (Layer 2)
 * reconstruction.
 *
 * Shared alt-entry:
 *   0x33470 (the fd2_init_battle_state_for_chapter CALL onward) is
 *   itself tail-JMPed into by fd2_chapter_16_init and
 *   fd2_chapter_19_20_21_init_shared.
 *
 * Linked handlers:
 *   End:         fd2_chapter_13_end @ 0x2389F
 *   Post-action: fd2_chapter_13_post_action @ 0x20765 — non-default:
 *     (1) chars[0xF..0x1A] (12 NPCs) all dead = lose + dialog page 10;
 *     (2) save_metadata > 5 AND char[0x3B] dead = lose + dialog page 2.
 *
 * Walkthrough SOT: assets/chapters/chapter_13.md
 * ---------------------------------------------------------------- */
void fd2_chapter_13_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_14_init @ 0x3347C  (dispatched, 0 direct callers)
 *
 * Chapter 14「平原的會戰」init handler. A minimal chapter init: it
 * re-inits battle state, pans the camera-and-window once
 * (target 0x14, 0x14), plays exactly one dialog page (page 0), and
 * pans the camera to char 0. There is NO cutscene, NO portrait load,
 * NO char init, and NO clear-facing — chapter 14 carries the party
 * over from chapter 13. The single camera-pan-and-window prelude is
 * the only thing that distinguishes it from the bare chapter 13.
 *
 * Unlike chapter 06, this handler never resets
 * data_fd2_battle_anim_phase (there is no MOV [0x51A83],0 on its code
 * path) — the entry block does init battle state + the one
 * pan_cursor_and_window, then tail-JMPs straight into the shared
 * dialog-arg-push chain, which sits after every anim_phase reset.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the page-0 dialog call + the final
 * fd2_pan_cursor_to_char(0) are emitted as a tail-JMP through the same
 * shared chain used by fd2_chapter_06_init / fd2_chapter_10_init /
 * fd2_chapter_13_init: 0x3344D (page-0 dialog-arg push, owned by
 * fd2_chapter_12_init) -> 0x33206 (the fd2_display_dialog_scene call,
 * in fd2_chapter_07_init) -> 0x33140 (the fd2_pan_cursor_to_char(0) +
 * RET tail, owned by fd2_chapter_05_init, entered directly without a
 * clear-facing). The straight-line form here is the
 * functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_14_end @ 0x238DC
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_14.md
 * ---------------------------------------------------------------- */
void fd2_chapter_14_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(0x14, 0x14);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_15_init @ 0x334D9  (dispatched, 0 direct callers)
 *
 * Chapter 15「拉卡湖的激戰」init handler. The first chapter init with
 * a DATA-DEPENDENT dialog page selection: it swaps the three-page
 * narrative block based on whether 凱麗 (char id 0xC) is currently in
 * the party, then plays the three pages chained with one camera pan
 * and one cutscene (event id 0x30), before handing the chapter off to
 * the player. There is NO char init and NO portrait load — chapter 15
 * carries the party over from chapter 14.
 *
 * The page base is computed as page_base = (has_char(0xC) ^ 1) * 3:
 *   凱麗 present (fd2_check_party_has_char_id returns 1) -> base 0,
 *     so dialog pages 0/1/2 play;
 *   凱麗 absent  (returns 0)                            -> base 3,
 *     so dialog pages 3/4/5 play.
 * In the disassembly this is XOR AL,1; MOV AH,3; MUL AH (8-bit AX =
 * AL*AH) with the low byte taken via MOVZX EBX,AL; EBX then carries
 * page_base, page_base+1 (LEA EAX,[EBX+1]) and page_base+2 (ADD EBX,2)
 * to the three dialog calls. The XOR AL,1 consumes the genuine byte
 * return of the CALL (not a Ghidra EAX-tracking artifact).
 *
 * data_fd2_battle_anim_phase is reset to 0 after the page_base+1 dialog
 * (and the cutscene) only; the page_base and page_base+2 dialogs have
 * no reset.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x2C)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the page_base+2 dialog call + the final
 * fd2_pan_cursor_to_char(0) are physically self-contained here (no
 * tail-JMP into another chapter's epilogue). This handler OWNS one
 * shared alt-entry point that other chapter inits tail-JMP into:
 *   0x33594 (the fd2_pan_cursor_to_char(0) + RET tail) — entered by
 *           fd2_chapter_23_init and fd2_chapter_28_init.
 *
 * Linked handlers:
 *   End:         fd2_chapter_15_end @ 0x239BD
 *   Post-action: fd2_chapter_15_post_action @ 0x20822
 *                (extra lose if char[0x40] dead — 賽可邦勒)
 *
 * Walkthrough SOT: assets/chapters/chapter_15.md
 * ---------------------------------------------------------------- */
void fd2_chapter_15_init(void)
{
    int page_base;

    fd2_init_battle_state_for_chapter();
    page_base = (int)((((fd2_check_party_has_char_id(0xc) & 0xff) ^ 1) * 3)
                      & 0xff);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, (uint32)page_base,
                             0xa0000, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_and_window(0x18, 0x11);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, (uint32)(page_base + 1),
                             0xa0000, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x30);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, (uint32)(page_base + 2),
                             0xa0000, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_16_init @ 0x335A0  (dispatched, 0 direct callers)
 *
 * Chapter 16「冰原之戰」init handler. In the binary this is a PURE
 * THUNK — physically just two instructions, PUSH 0x28 then JMP 0x33470
 * — that tail-jumps into the shared body owned by fd2_chapter_13_init.
 * 0x33470 is fd2_chapter_13_init + 5 (its CALL __CHK site), so chapter
 * 16 shares the entire chapter-13 tail and runs the IDENTICAL body:
 * re-init battle state, play a single dialog page (page 0), pan the
 * camera to char 0. There is NO cutscene, NO portrait load, NO char
 * init, NO camera-pan-and-window prelude, NO data_fd2_battle_anim_phase
 * reset, and NO clear-facing — chapter 16 carries the party over from
 * the previous chapter. It is a member of the minimal/smallest init
 * family (cf. chapter 13 @0x3346B / chapter 06 @0x3314B).
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe (the PUSH 0x28 here, consumed by the shared CALL __CHK at
 * 0x33470) is the Watcom-injected frame-size check and is not part of
 * the source body.
 *
 * In the binary the whole body after the frame check is reached by the
 * tail-JMP 0x335A5 -> 0x33470 (= fd2_chapter_13_init + 5): CALL
 * fd2_init_battle_state_for_chapter, then JMP 0x3344D into the same
 * shared page-0 dialog chain used by chapters 06/10/13/14 — 0x3344D
 * (page-0 dialog-arg push, owned by fd2_chapter_12_init) -> 0x33206
 * (the fd2_display_dialog_scene call, in fd2_chapter_07_init) -> 0x33140
 * (the fd2_pan_cursor_to_char(0) + RET tail, owned by fd2_chapter_05_init,
 * entered directly without a clear-facing). The straight-line form here
 * is the functionally-equivalent (Layer 2) reconstruction — identical to
 * the fd2_chapter_13_init body, as documented at its 0x33470 alt-entry.
 *
 * Linked handlers:
 *   End:         fd2_chapter_16_end @ 0x23A0A
 *   Post-action: fd2_chapter_16_post_action @ 0x2084A
 *                (extra lose if char[0x41] dead — 蜜蒂 NPC)
 *
 * (蜜蒂 conditional recruit logic lives in fd2_chapter_16_end: HP_max >= 320
 * + save_metadata < 19 + chars[0x42..0x49] dead <= 4; char id 0x12 added.)
 *
 * Walkthrough SOT: assets/chapters/chapter_16.md
 * ---------------------------------------------------------------- */
void fd2_chapter_16_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_17_init @ 0x335AA  (dispatched, 0 direct callers)
 *
 * Chapter 17「血與冰之刃」init handler. A near-minimal chapter init
 * distinguished only by a DATA-DEPENDENT portrait load: it re-inits
 * battle state, conditionally loads portrait set 1 ONLY when 蜜蒂
 * (char id 0x12) is NOT currently in the party, plays a single dialog
 * page (page 0), and pans the camera to char 0. There is NO cutscene,
 * NO char init, NO camera-pan-and-window prelude, NO
 * data_fd2_battle_anim_phase reset, and NO clear-facing — chapter 17
 * carries the party over from the previous chapter.
 *
 * The portrait load is gated by fd2_check_party_has_char_id(0x12): the
 * disassembly is TEST EAX,EAX; JNZ (skip the load) — so the load runs
 * only on the return == 0 (蜜蒂 absent) branch. The TEST EAX,EAX
 * consumes the genuine return value of the CALL (not a Ghidra
 * EAX-tracking artifact).
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this handler physically contains only its entry block
 * (init battle state + the gated portrait load); both the load-taken
 * and load-skipped paths converge on a tail-JMP into the same shared
 * page-0 dialog chain used by chapters 06/10/13/14/16: 0x3344D (page-0
 * dialog-arg push, owned by fd2_chapter_12_init) -> 0x33206 (the
 * fd2_display_dialog_scene call, in fd2_chapter_07_init) -> 0x33140
 * (the fd2_pan_cursor_to_char(0) + RET tail, owned by
 * fd2_chapter_05_init, entered directly without a clear-facing). The
 * straight-line form here is the functionally-equivalent (Layer 2)
 * reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_17_end @ 0x23B5F
 *   Post-action: fd2_chapter_17_post_action @ 0x20872 — gated lose:
 *     蜜蒂(char 0x12) not joined AND char[0x34] dead -> page 2 + lose.
 *
 * Walkthrough SOT: assets/chapters/chapter_17.md
 * ---------------------------------------------------------------- */
void fd2_chapter_17_init(void)
{
    fd2_init_battle_state_for_chapter();
    if (fd2_check_party_has_char_id(0x12) == 0) {
        fd2_load_chapter_portraits_and_dump_tmp(1);
    }
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_18_init @ 0x335DA  (dispatched, 0 direct callers)
 *
 * Chapter 18「遙遠的彼岸」init handler. A flat chapter-prologue
 * orchestrator that plays three dialog pages (pages 0/1/2) chained
 * with two cutscenes (event ids 0x36 / 0x37), each cutscene preceded
 * by a camera pan to the same (0x10, 4) target, before handing the
 * chapter off to the player. There is NO portrait load and NO char
 * init — chapter 18 carries the party over from the previous chapter.
 *
 * data_fd2_battle_anim_phase is reset to 0 after pages 0 and 1 only;
 * page 2 has no reset — it is the tail before the final clear-facing +
 * camera-to-char pan.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this handler ends by pushing the cutscene-0x37 arg and
 * tail-JMPing (0x3366f -> 0x3310c) into the shared epilogue owned by
 * fd2_chapter_05_init: the cutscene-trigger CALL, then PUSH text;
 * PUSH 2; CALL fd2_display_dialog_scene (page 2); clear-facing;
 * pan_cursor_to_char(0); RET. (This is the same 0x3310C alt-entry that
 * fd2_chapter_11_init also tail-JMPs into.) The straight-line body here
 * is the functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_18_end @ 0x23CD5
 *   Post-action: fd2_chapter_18_post_action @ 0x208CF — bypass default:
 *     chars[0, 0x10, 0x11] any dead = lose;
 *     char[0x34] dead = win (擊殺黑暗騎士勝利條件 — 首章 boss-kill-win 設計).
 *
 * Walkthrough SOT: assets/chapters/chapter_18.md
 * ---------------------------------------------------------------- */
void fd2_chapter_18_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(0x10, 4);
    fd2_cutscene_event_trigger(0x36);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(0x10, 4);
    fd2_cutscene_event_trigger(0x37);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_19_20_21_init_shared @ 0x33674  (dispatched, 0 direct
 *                                              callers; 3 table slots)
 *
 * Chapters 19/20/21 shared init handler — the game's ONLY three-chapter
 * shared init. In the binary it is a 10-byte PURE THUNK (PUSH 0x28;
 * JMP 0x33470) that tail-jumps into the shared body owned by
 * fd2_chapter_13_init (0x33470 = fd2_chapter_13_init + 5, its CALL
 * __CHK site) — the SAME entry that fd2_chapter_16_init also tail-JMPs
 * into. It therefore runs the IDENTICAL body to fd2_chapter_13_init
 * @0x3346B and fd2_chapter_16_init @0x335A0: re-init battle state, play
 * a single dialog page (page 0), pan the camera to char 0. There is NO
 * cutscene, NO portrait load, NO char init, NO camera-pan-and-window
 * prelude, NO data_fd2_battle_anim_phase reset, and NO clear-facing —
 * a member of the minimal/smallest init family (cf. chapter 13
 * @0x3346B / chapter 16 @0x335A0 / chapter 06 @0x3314B). The three
 * chapters share one init because their opening battle-id is selected
 * by current_chapter_id inside fd2_init_battle_state_for_chapter; all
 * chapter-specific behavior lives in each chapter's own post-action /
 * end handler.
 *
 * It is dispatched from THREE consecutive slots of the chapter-init
 * data table (xrefs @0x51DB9 / 0x51DBD / 0x51DC1 = chapters 19/20/21);
 * unlike the other chapter inits (one table slot each), this single
 * function backs all three. It has 0 direct callers.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe (the PUSH 0x28 here, consumed by the shared CALL __CHK at
 * 0x33470) is the Watcom-injected frame-size check and is not part of
 * the source body.
 *
 * In the binary the whole body after the frame check is reached by the
 * tail-JMP 0x33679 -> 0x33470 (= fd2_chapter_13_init + 5): CALL
 * fd2_init_battle_state_for_chapter, then JMP 0x3344D into the same
 * shared page-0 dialog chain used by chapters 06/10/13/14/16 — 0x3344D
 * (page-0 dialog-arg push, owned by fd2_chapter_12_init) -> 0x33206
 * (the fd2_display_dialog_scene call, in fd2_chapter_07_init) -> 0x33140
 * (the fd2_pan_cursor_to_char(0) + RET tail, owned by fd2_chapter_05_init,
 * entered directly without a clear-facing). The straight-line form here
 * is the functionally-equivalent (Layer 2) reconstruction — identical to
 * the fd2_chapter_13_init body, as documented at its 0x33470 alt-entry.
 *
 * Linked handlers:
 *   Ends:        fd2_chapter_19_end @ 0x23E39
 *                fd2_chapter_20_end @ 0x23E74
 *                fd2_chapter_21_end @ 0x240FA
 *   Post-action: fd2_chapter_19_post_action @ 0x20926
 *                  (gated lose if save_metadata > 6 AND char[0x40] dead
 *                   — 巴拿羅西亞)
 *                fd2_chapter_20_post_action @ 0x20957
 *                  (largest non-default handler — 3-stage NPC
 *                   group/single/merge win)
 *                fd2_chapter_21_post_action @ 0x20A51
 *                  (extra lose if char[0x10] OR char[0x11] dead
 *                   — 羅蘭/希爾法)
 *
 * Walkthrough SOT: assets/chapters/chapter_19.md, chapter_20.md,
 *                  chapter_21.md
 * ---------------------------------------------------------------- */
void fd2_chapter_19_20_21_init_shared(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_22_init @ 0x3367E  (dispatched, 0 direct callers)
 *
 * Chapter 22「遠古呼喚」init handler. A minimal flat chapter-prologue
 * orchestrator: it re-inits battle state, pans the camera-and-window
 * once (target 0x10, 0x1C), plays a single cutscene (event id 0x43),
 * clears all facings, plays exactly one dialog page (page 0), and pans
 * the camera to char 0. There is NO portrait load and NO char init —
 * chapter 22 carries the party over from the previous chapter.
 *
 * Unlike chapter 06, this handler never resets
 * data_fd2_battle_anim_phase (there is no MOV [0x51A83],0 anywhere on
 * its code path) — page 0 is the sole, tail dialog page. Note the
 * clear-facing happens BEFORE the dialog page here (between cutscene
 * 0x43 and the dialog), not after it.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this handler physically contains only its entry block
 * (init battle state + the one fd2_pan_cursor_and_window(0x10,0x1C))
 * and then pushes the cutscene-0x43 arg and tail-JMPs (0x3369B ->
 * 0x33440) into the shared body owned by fd2_chapter_12_init: the
 * cutscene-trigger CALL, then fd2_clear_all_chars_facing(), then the
 * shared page-0 dialog chain 0x3344D (page-0 dialog-arg push, owned by
 * fd2_chapter_12_init) -> 0x33206 (the fd2_display_dialog_scene call,
 * owned by fd2_chapter_07_init) -> 0x33140 (the fd2_pan_cursor_to_char(0)
 * + RET tail, owned by fd2_chapter_05_init, entered directly without a
 * clear-facing). The straight-line form here is the
 * functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_22_end @ 0x244B6
 *   Post-action: fd2_chapter_22_post_action @ 0x20A87 (shared with
 *                ch27/28) — extra lose if char[1] dead (希爾法).
 *
 * Walkthrough SOT: assets/chapters/chapter_22.md
 * ---------------------------------------------------------------- */
void fd2_chapter_22_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(0x10, 0x1c);
    fd2_cutscene_event_trigger(0x43);
    fd2_clear_all_chars_facing();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_23_init @ 0x336A0  (dispatched, 0 direct callers)
 *
 * Chapter 23「向天空之旅」init handler — the LARGEST chapter init in
 * the game (548 bytes). A cinematic "reassemble the party" prologue
 * built around a screen-wide spell effect: it wipes all 16 active
 * party slots, casts a dramatic full-screen radial spell at the
 * cursor, then revives only the HP-survivors, before playing five
 * dialog pages chained with three cutscenes (event ids 0x44/0x45/0x46)
 * and a portrait-load palette-flash sequence. There is NO char init —
 * chapter 23 carries the party over from the previous chapter.
 *
 * Two constant-bound loops over the 16 active slots:
 *   - Pre-spell mark-dead loop: fd2_mark_char_as_dead(i) for
 *     i in 0..0xF (clears every active party slot's HP).
 *   - Post-spell revive filter: for i in 0..0xF, any unit whose
 *     hp_current != 0 gets flags = 0 (clear dead/acted bits) and
 *     sprite_state[1] = 2 (face north). In the disassembly the
 *     element address is base + i*0x50 (i*5 << 4); the survivor test
 *     reads word [EAX+0x40] (hp_current), then writes byte [EAX+5]
 *     (flags) and byte [EAX+3] (sprite_state[1]).
 * After the dialog/cutscene block, two further fixed writes set slot
 * 0x10 and slot 0x11 sprite_state[1] = 2 (disasm [EAX+0x503] =
 * 0x10*0x50+3, [EAX+0x553] = 0x11*0x50+3).
 *
 * The spell call is fd2_cast_screen_wide_spell_with_fade(
 *   cursor_screen_x + 6, cursor_screen_y + 5, 10, 8) — epicenter at
 * the current cursor (+6/+5 tile offset), starting radius 10, radius
 * increment 8 per frame. The palette-flash sequence around the
 * portrait load saturates the VGA palette (add 0xFF) then restores
 * it (add 0), bracketed by composite-frame redraws and timed delays.
 *
 * data_fd2_battle_anim_phase is reset to 0 once, after the page-3
 * dialog only (pages 0/1/2 and the final page 4 have no reset).
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x2C)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the final fd2_pan_cursor_to_char(0) is emitted as a
 * tail-JMP (0x336BF -> 0x33594) into the shared epilogue owned by
 * fd2_chapter_15_init (PUSH 0; CALL fd2_pan_cursor_to_char; POP EBX;
 * RET); the preceding fd2_clear_all_chars_facing() is physically the
 * last instruction of this function's own body. The straight-line
 * form here is the functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_23_end @ 0x24754
 *   Post-action: fd2_chapter_23_post_action @ 0x20AAF — bypass default:
 *     chars[0, 1, 0x10, 0x11] any dead = lose; char[0x12] dead = win
 *     (機甲隊長).
 *
 * Walkthrough SOT: assets/chapters/chapter_23.md
 * ---------------------------------------------------------------- */
void fd2_chapter_23_init(void)
{
    int i;

    fd2_init_battle_state_for_chapter();
    for (i = 0; i < 0x10; i++) {
        fd2_mark_char_as_dead(i);
    }
    fd2_pan_cursor_and_window(0xe, 0x20);
    fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x + 6,
                                         data_fd2_battle_cursor_screen_y + 5,
                                         10, 8);
    for (i = 0; i < 0x10; i++) {
        if (data_fd2_battle_runtime_char_array_ptr[i].hp_current != 0) {
            data_fd2_battle_runtime_char_array_ptr[i].flags = 0;
            data_fd2_battle_runtime_char_array_ptr[i].sprite_state[1] = 2;
        }
    }
    fd2_composite_battle_frame(0);
    fd2_set_vga_palette_range_with_add(0, 0xff, 0);
    __delay_thunk_375b2(500);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_and_window(0xe, 0x1d);
    fd2_cutscene_event_trigger(0x44);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_cutscene_event_trigger(0x45);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_cutscene_event_trigger(0x46);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(0xe, 0xd);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    __delay_thunk_375b2(200);
    fd2_set_vga_palette_range_with_add(0, 0xff, 0xff);
    __delay_thunk_375b2(100);
    fd2_composite_battle_frame(0);
    fd2_set_vga_palette_range_with_add(0, 0xff, 0);
    __delay_thunk_375b2(500);
    data_fd2_battle_runtime_char_array_ptr[0x10].sprite_state[1] = 2;
    data_fd2_battle_runtime_char_array_ptr[0x11].sprite_state[1] = 2;
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_24_init @ 0x338C4  (dispatched, 0 direct callers)
 *
 * Chapter 24「在天空的彼方」init handler. A flat chapter-prologue
 * orchestrator whose one distinguishing flourish is a 4-corner camera
 * scan that previews the map (the reinforcement points) before the
 * battle begins: it re-inits battle state, plays a dialog page
 * (page 0), loads portrait set 1, sweeps the camera-and-window to the
 * four map corners holding 400ms at each, plays a second dialog page
 * (page 1), and pans the camera to char 0. There is NO cutscene, NO
 * char init, NO data_fd2_battle_anim_phase reset, and NO clear-facing
 * — chapter 24 carries the party over from the previous chapter.
 *
 * The four corner pans are fd2_pan_cursor_and_window(ox, oy) to
 * (0, 4) -> (0, 0x16) -> (0x1A, 0x18) -> (0x1A, 2), each immediately
 * followed by __delay_thunk_375b2(400) (0x190) to hold the view.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body. It is a pure straight-line orchestrator: NO
 * numeric computation, NO RNG, NO data-dependent branch, and NO
 * CALL-result consumption (no Ghidra EAX-tracking-bug exposure) — the
 * dialog-scene CALL returns are discarded.
 *
 * In the binary this handler physically contains only its entry block
 * (init battle state, page-0 dialog, portrait load, and the 4-corner
 * scan); after the final hold it tail-JMPs (0x33965 -> 0x331EA) into
 * the alt-entry owned by fd2_chapter_07_init — the page-1 dialog-arg
 * push (PUSH text; PUSH 1; CALL fd2_display_dialog_scene) — which in
 * turn JMPs (0x33214 -> 0x33140) into the shared epilogue owned by
 * fd2_chapter_05_init (fd2_pan_cursor_to_char(0); RET, entered directly
 * without a clear-facing). The straight-line form here is the
 * functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_24_end @ 0x24C1E
 *   Post-action: (default — fd2_check_battle_end_default_handler
 *                @ 0x205B4)
 *
 * Walkthrough SOT: assets/chapters/chapter_24.md
 * ---------------------------------------------------------------- */
void fd2_chapter_24_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_pan_cursor_and_window(0, 4);
    __delay_thunk_375b2(400);
    fd2_pan_cursor_and_window(0, 0x16);
    __delay_thunk_375b2(400);
    fd2_pan_cursor_and_window(0x1a, 0x18);
    __delay_thunk_375b2(400);
    fd2_pan_cursor_and_window(0x1a, 2);
    __delay_thunk_375b2(400);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_25_init @ 0x3396A  (dispatched, 0 direct callers)
 *
 * Chapter 25「火焰的審判」init handler — the only chapter init that
 * stages an earthquake set-piece. It re-inits battle state, loads the
 * earthquake SFX wave from FDOTHER.DAT (entry 0x58) into the shared
 * status-effect SFX handle, plays dialog page 1, wipes the large game-
 * state buffer, then runs four screen-shake cycles each prefixed with
 * the quake SFX: three normal-magnitude (0x14 frames) shakes separated
 * by 600ms holds, followed by a final 3x-magnitude (0x3C frames) shake
 * with no trailing hold. After the quake it plays dialog page 2, pans
 * the camera to char 0, and frees the status-effect SFX. There is NO
 * char init and NO portrait load — chapter 25 carries the party over
 * from the previous chapter.
 *
 * The earthquake SFX handle is stored to / replayed from the shared
 * data_fd2_audio_status_effect_sfx_handle_ptr global; it is cleared to
 * 0 before the load. The memset zeroes the 0x25680-byte
 * data_fd2_large_game_state_buffer. The fd2_load_dat_resource return
 * (the loaded wave handle) is the sole CALL-result consumed — stored to
 * the handle global, matching the disassembly (MOV [0x53B13],EAX); the
 * dialog-scene CALL returns are discarded (no Ghidra EAX-tracking-bug
 * exposure on those).
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body. In the binary the final
 * fd2_play_and_free_status_effect_sfx() is emitted as a tail-JMP
 * (0x33AA9 -> 0x1D4F6) to that self-contained handler; the straight-
 * line call form here is the functionally-equivalent reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_25_end @ 0x24DF2
 *   Post-action: fd2_chapter_25_post_action @ 0x20B14
 *                (extra lose if char[0x10] dead — 聖寇拉斯)
 *
 * Walkthrough SOT: assets/chapters/chapter_25.md
 * ---------------------------------------------------------------- */
void fd2_chapter_25_init(void)
{
    fd2_init_battle_state_for_chapter();
    data_fd2_audio_status_effect_sfx_handle_ptr = 0;
    data_fd2_audio_status_effect_sfx_handle_ptr =
        fd2_load_dat_resource(0x51a4d, 0, 0x58);
    fd2_pan_cursor_and_window(5, 0);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    memset((void *)data_fd2_large_game_state_buffer_ptr, 0, 0x25680);

    fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr,
                             1, 1);
    fd2_animate_screen_shake(0x14);
    __delay_thunk_375b2(600);
    fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr,
                             1, 1);
    fd2_animate_screen_shake(0x14);
    __delay_thunk_375b2(600);
    fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr,
                             1, 1);
    fd2_animate_screen_shake(0x14);
    __delay_thunk_375b2(600);
    fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr,
                             1, 1);
    fd2_animate_screen_shake(0x3c);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_to_char(0);
    fd2_play_and_free_status_effect_sfx();
}

/* ----------------------------------------------------------------
 * fd2_chapter_26_init @ 0x33AAE  (dispatched, 0 direct callers)
 *
 * Chapter 26「未知的迴廊」init handler. A minimal flat chapter-prologue
 * orchestrator: it re-inits battle state, pans the camera-and-window
 * once (target 9, 0x27), plays a single cutscene (event id 0x4C),
 * plays exactly one dialog page (page 0), clears all facings, and pans
 * the camera to char 0. There is NO portrait load and NO char init —
 * chapter 26 carries the party over from the previous chapter.
 *
 * There is NO data_fd2_battle_anim_phase reset anywhere on this
 * handler's code path (no MOV [0x51A83],0): page 0 is the sole, tail
 * dialog page. It is a pure straight-line orchestrator: NO numeric
 * computation, NO RNG, NO data-dependent branch, and NO CALL-result
 * consumption (no Ghidra EAX-tracking-bug exposure) — the dialog-scene
 * CALL return is discarded.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this handler physically contains only its entry block
 * (init battle state, the one fd2_pan_cursor_and_window(9,0x27), and the
 * cutscene-0x4C trigger); after pushing the 9 dialog-scene args (page 0)
 * it tail-JMPs (0x33AEC -> 0x3312D) into the shared epilogue owned by
 * fd2_chapter_05_init (PUSH data_fd2_current_chapter_text; CALL
 * fd2_display_dialog_scene; clear-facing; pan_cursor_to_char(0); RET).
 * The straight-line form here is the functionally-equivalent (Layer 2)
 * reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_26_end @ 0x24E80
 *   Post-action: fd2_chapter_26_post_action @ 0x20B3C
 *                (extra lose if char[1] OR char[2] dead — 悠妮/亞奇梅吉)
 *
 * (9 階段密集 reinforcement turn 2/4/6/8/10/12/15/16/17 = FDFIELD event
 * 觸發, 非此 init handler.)
 *
 * Walkthrough SOT: assets/chapters/chapter_26.md
 * ---------------------------------------------------------------- */
void fd2_chapter_26_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(9, 0x27);
    fd2_cutscene_event_trigger(0x4c);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_27_init @ 0x33AF1  (dispatched, 0 direct callers)
 *
 * Chapter 27「命運的交會點」init handler — the GOOD/BAD ENDING fork
 * chapter. A cinematic prologue built around three screen-wide spell
 * visual effects: it re-inits battle state, pans the camera-and-window
 * once (target 9, 0x31), plays a cutscene (event id 0x4C) and dialog
 * page 0, then — ONLY if any party member is carrying item 100
 * (天空之鑰 / Sky Key) — plays a bonus dialog page 3. It then plays
 * dialog page 4, re-pans the camera, and runs three spell-effect beats
 * each followed by a full VGA palette reset (add 0) and a dialog page,
 * before clearing all facings and panning the camera to char 0. There
 * is NO portrait load and NO char init — chapter 27 carries the party
 * over from the previous chapter.
 *
 * The Sky-Key gate is fd2_any_char_has_item(100): the disassembly is
 * CALL; CMP EAX,-1; JZ (skip page 3) — so the bonus page plays only on
 * the return != -1 (Sky Key present) branch. The CMP EAX,-1 consumes
 * the genuine return value of the CALL (not a Ghidra EAX-tracking
 * artifact); it is the sole CALL-result consumed in this handler (the
 * dialog-scene / spell / palette CALL returns are all discarded).
 *
 * The three spell beats are fd2_cast_screen_wide_spell_with_fade with
 * a fixed starting-radius/increment of (2, 2), epicentered at the live
 * battle cursor with a per-beat tile offset:
 *   beat 1: (cursor_x,     cursor_y + 3, 2, 2)  -> page 5
 *   beat 2: (cursor_x,     cursor_y,     2, 2)  -> cutscene 0x51, page 6
 *   beat 3: (cursor_x + 2, cursor_y,     2, 2)  -> page 7
 * Each beat is immediately followed by
 * fd2_set_vga_palette_range_with_add(0, 0xFF, 0) to restore the palette.
 * There is NO data_fd2_battle_anim_phase reset anywhere on this
 * handler's code path (no MOV [0x51A83],0).
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary this handler physically contains its own body up to the
 * page-7 dialog-arg push; it then tail-JMPs (0x33C98 -> 0x3312D) into
 * the shared epilogue owned by fd2_chapter_05_init (PUSH
 * data_fd2_current_chapter_text; CALL fd2_display_dialog_scene [page 7];
 * clear-facing; pan_cursor_to_char(0); RET). The straight-line form
 * here is the functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_27_end @ 0x250CC (BAD: game-over hard-lock
 *                if the party has no 天空之鑰).
 *   Post-action: fd2_chapter_22_27_28_post_action_shared @ 0x20A87
 *                (shared with ch22/28) — extra lose if char[1] dead
 *                (悠妮).
 *
 * Walkthrough SOT: assets/chapters/chapter_27.md
 * ---------------------------------------------------------------- */
void fd2_chapter_27_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_pan_cursor_and_window(9, 0x31);
    fd2_cutscene_event_trigger(0x4c);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    if (fd2_any_char_has_item(100) != -1) {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140,
                                 0xcd, 0x4c, 0x4a, 0x13, 1);
    }
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_pan_cursor_and_window(9, 0x31);
    fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x,
                                         data_fd2_battle_cursor_screen_y + 3,
                                         2, 2);
    fd2_set_vga_palette_range_with_add(0, 0xff, 0);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x,
                                         data_fd2_battle_cursor_screen_y,
                                         2, 2);
    fd2_set_vga_palette_range_with_add(0, 0xff, 0);
    fd2_cutscene_event_trigger(0x51);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x + 2,
                                         data_fd2_battle_cursor_screen_y,
                                         2, 2);
    fd2_set_vga_palette_range_with_add(0, 0xff, 0);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_28_init @ 0x33C9D  (dispatched, 0 direct callers)
 *
 * Chapter 28「探索者」init handler. A cinematic "reassemble the
 * party" prologue, near-twin of fd2_chapter_23_init: it wipes all 20
 * active party slots, casts a screen-wide radial spell at the cursor,
 * then revives only the HP-survivors, before a triple parallel-walk
 * cutscene and a two-pass portrait dump. There is NO char init —
 * chapter 28 carries the party over from the previous chapter.
 *
 * Two constant-bound loops over the first 20 active slots:
 *   - Pre-spell mark-dead loop: fd2_mark_char_as_dead(i) for
 *     i in 0..0x13 (clears every active party slot's HP). The 20-slot
 *     bound (0x14) is wider than chapter 23's 16-slot (0x10) wipe.
 *   - Post-spell revive filter: for i in 0..0x13, any unit whose
 *     hp_current != 0 gets flags = 0 (clear dead/acted bits). In the
 *     disassembly the element address is base + i*0x50 (i*5 << 4); the
 *     survivor test reads word [EAX+0x40] (hp_current) and, on the
 *     non-zero branch, writes byte [EAX+5] (flags). Unlike chapter 23
 *     there is NO accompanying sprite_state[1] = 2 (facing) write.
 *
 * The spell call is fd2_cast_screen_wide_spell_with_fade(
 *   cursor_screen_x + 6, cursor_screen_y + 5, 10, 8) — epicenter at
 * the current cursor (+6/+5 tile offset), starting radius 10, radius
 * increment 8 per frame (identical params to chapter 23). After the
 * spell the screen is composited and the VGA palette restored
 * (add 0) before a 500ms hold.
 *
 * Three back-to-back fd2_cutscene_event_trigger(0x55) calls fire the
 * three parallel walk-in groups, then all facings are cleared and a
 * single dialog page (page 0) plays. data_fd2_battle_anim_phase is
 * driven as a phase fork around the two portrait dumps: it is reset
 * to 0 right after the dialog page, then the two
 * fd2_cinematic_chapter_portrait_dump_with_white_flash calls run
 * (set 0 white-flash slot 6, then set 7 white-flash slot 7), then it
 * is set to 1 before the final camera-to-char pan.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x2C)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the final fd2_pan_cursor_to_char(0) is emitted as a
 * tail-JMP (0x33DB5 -> 0x33594) into the shared epilogue owned by
 * fd2_chapter_15_init (PUSH 0; CALL fd2_pan_cursor_to_char; POP EBX;
 * RET); the entry PUSH EBX is the matching callee-save restored by
 * that shared POP EBX. The straight-line form here is the
 * functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_28_end @ 0x25464 (smallest end @ 40B —
 *                pure dialog 7 + save + chapter advance)
 *   Post-action: fd2_chapter_22_27_28_post_action_shared @ 0x20A87
 *                (shared with ch22/27) — extra lose if char[1] dead
 *                (悠妮).
 *
 * Walkthrough SOT: assets/chapters/chapter_28.md
 * ---------------------------------------------------------------- */
void fd2_chapter_28_init(void)
{
    int i;

    fd2_init_battle_state_for_chapter();
    for (i = 0; i < 0x14; i++) {
        fd2_mark_char_as_dead(i);
    }
    fd2_pan_cursor_and_window(0x1d, 0xf);
    fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x + 6,
                                         data_fd2_battle_cursor_screen_y + 5,
                                         10, 8);
    for (i = 0; i < 0x14; i++) {
        if (data_fd2_battle_runtime_char_array_ptr[i].hp_current != 0) {
            data_fd2_battle_runtime_char_array_ptr[i].flags = 0;
        }
    }
    fd2_composite_battle_frame(0);
    fd2_set_vga_palette_range_with_add(0, 0xff, 0);
    __delay_thunk_375b2(500);
    fd2_cutscene_event_trigger(0x55);
    fd2_cutscene_event_trigger(0x55);
    fd2_cutscene_event_trigger(0x55);
    fd2_clear_all_chars_facing();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cinematic_chapter_portrait_dump_with_white_flash(0, 0x10, 6);
    fd2_cinematic_chapter_portrait_dump_with_white_flash(7, 0x10, 7);
    data_fd2_battle_anim_phase = 1;
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_29_init @ 0x33DBA  (dispatched, 0 direct callers)
 *
 * Chapter 29「無邊的黑暗之中」init handler. A flat chapter-prologue
 * orchestrator that plays two dialog pages (pages 7/8) and one
 * white-flash portrait dump between them, bracketed by a single
 * camera pan and one cutscene (event id 0x56), before handing the
 * chapter off to the player. There is NO char init and NO portrait
 * pre-load — chapter 29 carries the party over from chapter 28.
 *
 * Unusually for an init handler the dialog page indices start at 7
 * (not 0): pages 0..6 of this chapter's text block belong to the
 * end handler, so the init consumes pages 7 and 8. data_fd2_battle
 * _anim_phase is reset to 0 exactly once — at the very top, right
 * after fd2_init_battle_state_for_chapter() and before the camera
 * pan; neither dialog page has a trailing reset.
 *
 * This is the ONLY chapter whose win/lose is decided by tile-event
 * consumption rather than character death: its post-action handler
 * fd2_chapter_29_post_action @ 0x20B72 wins when
 * tile_event_consumed_flags[0x12,0x13,0x14] are all set and loses
 * when chars[0,1] are dead.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the page-8 dialog call + the trailing
 * fd2_clear_all_chars_facing() and fd2_pan_cursor_to_char(0) are
 * emitted as a tail-JMP (0x33E37 -> 0x3312D) into the shared epilogue
 * owned by fd2_chapter_05_init (PUSH data_fd2_current_chapter_text; CALL
 * fd2_display_dialog_scene; CALL fd2_clear_all_chars_facing; PUSH 0;
 * CALL fd2_pan_cursor_to_char; RET) — the same 0x3312D alt-entry used
 * by chapters 03 / 04 / 26 / 27. The straight-line form here is the
 * functionally-equivalent (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_29_end @ 0x2548C
 *   Post-action: fd2_chapter_29_post_action @ 0x20B72 (non-default —
 *                tile_event_consumed_flags[0x12,0x13,0x14] all set =
 *                win; chars[0,1] dead = lose)
 *
 * Walkthrough SOT: assets/chapters/chapter_29.md
 * ---------------------------------------------------------------- */
void fd2_chapter_29_init(void)
{
    fd2_init_battle_state_for_chapter();
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(9, 0x38);
    fd2_cutscene_event_trigger(0x56);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_cinematic_chapter_portrait_dump_with_white_flash(9, 0x13, 8);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}

/* ----------------------------------------------------------------
 * fd2_chapter_30_init @ 0x33E3C  (dispatched, 0 direct callers)
 *
 * Chapter 30「傳說的終章－結局」init handler — the final chapter.
 * A cinematic chapter-prologue orchestrator that warps the 魔神
 * (demon-god) group onto the map in two staged batches separated by
 * a dramatic white palette-flash, plays three dialog pages (pages
 * 0/1/2), and pans the camera, before handing the final battle off
 * to the player. There is NO char init and NO portrait pre-load —
 * chapter 30 carries the party over from chapter 29.
 *
 * Batch 1 (上排, tile_y = 5) warps in 4 units, batch 2 (下排,
 * tile_y = 0x12) warps in 3 units, for 7 total
 * fd2_cinematic_warp_char_to_tile calls. Each call is
 * (char_id, tile_x, tile_y); the third argument is the destination
 * tile Y coordinate (written through to runtime_char.bPos_y by
 * fd2_animate_warp_teleport_char), which is why batch 1 uses y=5
 * and batch 2 uses y=0x12 — the two rows of demon-gods.
 * data_fd2_battle_anim_phase is reset to 0 after pages 0 and 1, then
 * set to 1 at the very end (before clear-facing) to flag the final
 * battle scene.
 *
 * void __cdecl, no real params, void return. The leading __CHK(0x28)
 * stack-probe is the Watcom-injected frame-size check and is not part
 * of the source body.
 *
 * In the binary the trailing fd2_clear_all_chars_facing() and
 * fd2_pan_cursor_to_char(0) are emitted as a tail-JMP (0x33F73 ->
 * 0x3313B) into the shared epilogue owned by fd2_chapter_05_init
 * (CALL fd2_clear_all_chars_facing; PUSH 0; CALL
 * fd2_pan_cursor_to_char; RET) — entered at its clear-facing point.
 * The straight-line form here is the functionally-equivalent
 * (Layer 2) reconstruction.
 *
 * Linked handlers:
 *   End:         fd2_chapter_30_end @ 0x25757
 *                (GOOD ENDING + staff roll)
 *   Post-action: fd2_chapter_30_post_action @ 0x20BF5 (non-default —
 *                char[0x14] dead = win (空魔神); chars[0,1] dead =
 *                lose)
 *
 * Walkthrough SOT: assets/chapters/chapter_30.md
 * ---------------------------------------------------------------- */
void fd2_chapter_30_init(void)
{
    fd2_init_battle_state_for_chapter();
    fd2_cutscene_event_trigger(0x57);
    fd2_pan_cursor_and_window(0x10, 0x13);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(0x10, 1);
    fd2_cinematic_warp_char_to_tile(0x15, 0x15, 5);
    fd2_cinematic_warp_char_to_tile(0x16, 0x17, 5);
    fd2_cinematic_warp_char_to_tile(0x17, 0x14, 5);
    fd2_cinematic_warp_char_to_tile(0x18, 0x18, 5);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_animate_palette_flash_pulse_white();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(0x10, 0xe);
    fd2_cinematic_warp_char_to_tile(0x18, 0x16, 0x12);
    fd2_cinematic_warp_char_to_tile(0x19, 0x15, 0x12);
    fd2_cinematic_warp_char_to_tile(0x1a, 0x17, 0x12);
    data_fd2_battle_anim_phase = 1;
    fd2_clear_all_chars_facing();
    fd2_pan_cursor_to_char(0);
}
