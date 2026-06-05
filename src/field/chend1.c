/*
 * chend1.c — Chapter end handlers (chapters 1-6)
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

/* ----------------------------------------------------------------
 * fd2_chapter_03_end @ 0x230F2  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot 2)
 *
 * Chapter 3「往塞拉村途中」end handler. Persists the party's runtime-char
 * state to the template store, then branches on whether chapter survivor
 * runtime char #6 is alive (fd2_check_char_is_dead(6) == 0):
 *   alive: stages the recruit scene by copying the chapter-3 end scene
 *     position tables (X / Y / facing, 7 bytes each @ 0x520BA / 0x520C1 /
 *     0x520C8) into three on-stack char-placement blocks and handing them to
 *     fd2_setup_chars_and_camera_for_intro (place chars 0..6, camera origin
 *     (2,0)), shows the recruit dialog page 7, then recruits char #2
 *     (劍士鐵諾) via fd2_init_runtime_char_from_base_growth.
 *   dead: shows the no-recruit dialog page 6 only.
 * It then advances the current-chapter id by 1.
 *
 * The position tables are read unconditionally into the stack blocks before
 * the branch (matching the binary), but the blocks are only consumed on the
 * alive path.
 *
 * Paired init handler: fd2_chapter_03_init @ 0x32E8C.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_03.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_03_end(void)
{
    uint8 recruit_block_a[7];
    uint8 recruit_block_b[7];
    uint8 recruit_block_c[7];
    uint8 char6_dead;
    int i;

    for (i = 0; i < 7; i++) {
        recruit_block_a[i] = data_fd2_chapter_ch03_end_scene_char_pos_x_table[i];
        recruit_block_b[i] = data_fd2_chapter_ch03_end_scene_char_pos_y_table[i];
        recruit_block_c[i] = data_fd2_chapter_ch03_end_scene_char_facing_table[i];
    }

    fd2_save_runtime_char_to_template();
    char6_dead = (uint8)fd2_check_char_is_dead(6);

    if (char6_dead == 0) {
        fd2_setup_chars_and_camera_for_intro(
            (uint32)recruit_block_a, (uint32)recruit_block_b,
            (uint32)recruit_block_c, 0, 6, 0, 0, 0, 0, 2, char6_dead);
        fd2_display_dialog_scene(current_chapter_text, 7, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        fd2_init_runtime_char_from_base_growth(2);
    }
    else {
        fd2_display_dialog_scene(current_chapter_text, 6, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
    }

    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_04_end @ 0x231BC  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DF5)
 *
 * Chapter 4「塞拉村前」end handler. Shows the chapter-end dialog page 4,
 * persists the party's runtime-character state back to the template store,
 * then advances the current-chapter id by 1.
 *
 * The function body carries three mid-function entry points reused by other
 * chapter-end handlers, which JMP into the shared tail rather than calling:
 *   0x231C6 (+0xA)  <- fd2_chapter_09_end
 *   0x231DF (+0x23) <- fd2_chapter_06_end, fd2_chapter_28_end
 *   0x231F2 (+0x36) <- fd2_chapter_11_end, fd2_chapter_19_end
 * Those handlers replicate the relevant portion of this tail when emitted;
 * chapter 4 itself is emitted as a self-contained function (Layer 2 functional
 * equivalence — the jump-into-middle sharing is not preserved in source).
 *
 * Paired init handler: fd2_chapter_04_init @ 0x32FB2.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_04.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_04_end(void)
{
    fd2_display_dialog_scene(current_chapter_text, 4, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_05_end @ 0x231F9  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot @ 0x51DF9)
 *
 * Chapter 5「塞拉村」end handler. Stages the recruit scene by copying the
 * chapter-5 end scene position tables (X / Y / facing, 7 bytes each @
 * 0x520CF / 0x520D6 / 0x520DD) into three on-stack char-placement blocks and
 * handing them to fd2_setup_chars_and_camera_for_intro (place chars 0..6,
 * extra char 0x29 at (0xC,8) facing 0, camera origin (6,4)). It then shows
 * the recruit dialog page 9, recruits char #10 (僧侶瑪琳) via
 * fd2_init_runtime_char_from_base_growth, persists the party's runtime-char
 * state to the template store, then advances the current-chapter id by 1.
 *
 * Paired init handler: fd2_chapter_05_init @ 0x33049.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_05.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_05_end(void)
{
    uint8 recruit_block_a[7];
    uint8 recruit_block_b[7];
    uint8 recruit_block_c[7];
    int i;

    for (i = 0; i < 7; i++) {
        recruit_block_a[i] = data_fd2_chapter_ch05_end_scene_char_pos_x_table[i];
        recruit_block_b[i] = data_fd2_chapter_ch05_end_scene_char_pos_y_table[i];
        recruit_block_c[i] = data_fd2_chapter_ch05_end_scene_char_facing_table[i];
    }

    fd2_setup_chars_and_camera_for_intro(
        (uint32)recruit_block_a, (uint32)recruit_block_b,
        (uint32)recruit_block_c, 0, 6, 0x29, 0xc, 8, 0, 6, 4);
    fd2_display_dialog_scene(current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_init_runtime_char_from_base_growth(10);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_06_end @ 0x23296  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 6「普里茲港」end handler. Recruits char #13 (弓兵貝克威) via
 * fd2_init_runtime_char_from_base_growth, refreshes the portrait cache for
 * race 3, pans the camera/window to (5, 0xE), fires cutscene event 0x1B,
 * shows the chapter-end dialog page 6, persists the party's runtime-char
 * state to the template store, then advances the current-chapter id by 1.
 *
 * In the binary the function falls through into the shared tail of
 * fd2_chapter_04_end @ 0x231DF (PUSH current_chapter_text; dialog; cleanup;
 * save; INC chapter id; RET). It is emitted here as a self-contained
 * function (Layer 2 functional equivalence — the jump-into-middle sharing is
 * not preserved in source).
 *
 * Paired init handler: fd2_chapter_06_init @ 0x3314B.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_06.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_06_end(void)
{
    fd2_init_runtime_char_from_base_growth(0xd);
    fd2_load_chapter_portraits_and_dump_tmp(3);
    fd2_pan_cursor_and_window(5, 0xe);
    fd2_cutscene_event_trigger(0x1b);
    fd2_display_dialog_scene(current_chapter_text, 6, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}
