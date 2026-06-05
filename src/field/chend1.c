/*
 * chend1.c — Chapter 1「初試身手」end handler
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_chapter_01_end @ 0x22EF6  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 1 end handler. Shows the chapter-end dialog page, persists the
 * party's runtime-character state back to the template store, then advances
 * the current-chapter id to 1 (the next chapter the engine will load).
 *
 * Paired init handler: fd2_chapter_01_init @ 0x3231B.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_01.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_01_end(void)
{
    fd2_display_dialog_scene(current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_02_end @ 0x22F37  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot 1)
 *
 * Chapter 2「羅德鎮」end handler. Scans the 6 villager units
 * (chars[5..0xA]); if any is dead (flags bit0) it shows the failure
 * dialog page 7, otherwise it shows page 6 and grants 力量藥水 (item
 * 0xC6, AP+9) to the first player char. It then plays the post-battle
 * cutscene: pan window, load portraits, fire cutscene events 0xE/0xF/0x10
 * with dialog pages 8/9/10 interleaved (resetting battle_anim_phase and
 * delaying between beats), recruits char #8 (弓兵希莉亞), persists the
 * party's runtime state to the template store, then advances the
 * current-chapter id to 2.
 *
 * Paired init handler: fd2_chapter_02_init @ 0x32D18.
 * Post-action handler: fd2_chapter_02_post_action @ 0x206C5.
 * Walkthrough: assets/chapters/chapter_02.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_02_end(void)
{
    int villager_dead;
    int i;

    villager_dead = 0;
    for (i = 5; i < 0xb; i = i + 1) {
        if ((data_fd2_battle_runtime_char_array_ptr[i].flags & 1) != 0) {
            villager_dead = 1;
        }
    }

    if (villager_dead) {
        fd2_display_dialog_scene(current_chapter_text, 7, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
    }
    else {
        fd2_display_dialog_scene(current_chapter_text, 6, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        fd2_give_item_to_first_player_char(0xc6);
    }

    fd2_pan_cursor_and_window(0xe, 2);
    fd2_load_chapter_portraits_and_dump_tmp(4);
    __delay_thunk_375b2(100);
    fd2_cutscene_event_trigger(0xe);

    fd2_display_dialog_scene(current_chapter_text, 8, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0xf);

    fd2_display_dialog_scene(current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_pan_cursor_and_window(0xe, 1);
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0x10);
    __delay_thunk_375b2(200);

    fd2_display_dialog_scene(current_chapter_text, 10, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_init_runtime_char_from_base_growth(8);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = 2;
}
