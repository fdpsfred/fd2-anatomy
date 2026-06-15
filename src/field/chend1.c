/*
 * chend1.c — Chapter end handlers (chapters 1-19)
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
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
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
    }
    else {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        fd2_give_item_to_first_player_char(0xc6);
    }

    fd2_pan_cursor_and_window(0xe, 2);
    fd2_load_chapter_portraits_and_dump_tmp(4);
    __delay_thunk_375b2(100);
    fd2_cutscene_event_trigger(0xe);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0xf);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    __delay_thunk_375b2(200);
    fd2_pan_cursor_and_window(0xe, 1);
    __delay_thunk_375b2(200);
    fd2_cutscene_event_trigger(0x10);
    __delay_thunk_375b2(200);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 10, 0xa0000, 0x140, 0xcd,
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
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        fd2_init_runtime_char_from_base_growth(2);
    }
    else {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xa0000, 0x140, 0xcd,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140, 0xcd,
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
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
 * fd2_chapter_04_end @ 0x231DF (PUSH data_fd2_current_chapter_text; dialog; cleanup;
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
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_07_end @ 0x232E8  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 7「往王城的途中」end handler. Unconditionally copies three 9-byte
 * scene tables (recruit-scene X / Y / facing @ 0x520E4 / 0x520ED / 0x520F6)
 * into on-stack placement blocks and persists the party (real
 * fd2_save_runtime_char_to_template), then takes a dual-conditional recruit:
 *   if tile_event_consumed_flags[0x11] == 1 AND runtime char #0x2B (凱麗
 *   herself) is alive (fd2_check_char_is_dead(0x2B) == 0):
 *     stages the scene via fd2_setup_chars_and_camera_for_intro (chars 0..8,
 *     plus extra char 0x2B placed at (0xC,7) facing 2, camera origin (6,2)),
 *     shows the recruit dialog page 4, then recruits char #12 (武者凱麗) via
 *     fd2_init_runtime_char_from_base_growth.
 *   else: shows the no-recruit dialog page 5 only — no scene, no recruit.
 * It then advances the current-chapter id by 1.
 *
 * The position tables are read unconditionally into the stack blocks before
 * the branch (matching the binary), but the blocks are only consumed on the
 * recruit path.
 *
 * Paired init handler: fd2_chapter_07_init @ 0x33169.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_07.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_07_end(void)
{
    uint8 recruit_block_a[9];
    uint8 recruit_block_b[9];
    uint8 recruit_block_c[9];
    int i;

    for (i = 0; i < 9; i++) {
        recruit_block_a[i] = data_fd2_chapter_ch07_end_scene_char_pos_x_table[i];
        recruit_block_b[i] = data_fd2_chapter_ch07_end_scene_char_pos_y_table[i];
        recruit_block_c[i] = data_fd2_chapter_ch07_end_scene_char_facing_table[i];
    }

    fd2_save_runtime_char_to_template();

    if (*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0x11) == 1 &&
        fd2_check_char_is_dead(0x2b) == 0) {
        fd2_setup_chars_and_camera_for_intro(
            (uint32)recruit_block_a, (uint32)recruit_block_b,
            (uint32)recruit_block_c, 0, 8, 0x2b, 0xc, 7, 2, 6, 2);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        fd2_init_runtime_char_from_base_growth(0xc);
    }
    else {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
    }

    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_08_end @ 0x234BB  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot 7 @ 0x51E05)
 *
 * Chapter 8「王城前的戰鬥」end handler. Unconditionally copies two 10-byte
 * scene position tables (recruit-scene X / Y @ 0x520FF / 0x52109) into on-stack
 * placement blocks and stages the recruit scene via
 * fd2_setup_chars_and_camera_for_intro — here the facing argument is the inline
 * fixed value 2 (< 4), so every placed char faces direction 2 and there is no
 * facing table (chars 0..9, plus an extra char 0x1C placed at (0xE,0x10) facing
 * 0, camera origin (8,0xE)). It then plays the post-battle cutscene:
 *   dialog page 3; battle_anim_phase = 0; cutscene event 0x21;
 *   dialog page 4; cutscene_event_state = 1; battle_anim_phase = 0;
 *   cutscene event 0x22; cutscene_event_state = 0.
 * It fades the screen to black (fd2_set_vga_palette_range(0,0xff,0x40) then
 * memset(0xA0000, 0, 64000)), recruits char #5 (騎士洛娜) via
 * fd2_init_runtime_char_from_base_growth, persists the party's runtime-char
 * state to the template store, then advances the current-chapter id by 1.
 *
 * In the binary the function ends with `PUSH 5; JMP 0x2327D`, a tail-jump into
 * the shared tail of fd2_chapter_05_end @ 0x2327D (recruit char #5; save; INC
 * chapter id; cleanup; RET). It is emitted here as a self-contained function
 * (Layer 2 functional equivalence — the jump-into-middle sharing is not
 * preserved in source).
 *
 * Paired init handler: fd2_chapter_08_init @ 0x33219.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_08.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_08_end(void)
{
    uint8 scene_block_x[10];
    uint8 scene_block_y[10];
    int i;

    for (i = 0; i < 10; i++) {
        scene_block_x[i] = data_fd2_chapter_ch08_end_scene_char_pos_x_table[i];
        scene_block_y[i] = data_fd2_chapter_ch08_end_scene_char_pos_y_table[i];
    }

    fd2_setup_chars_and_camera_for_intro(
        (uint32)scene_block_x, (uint32)scene_block_y, 2, 0, 9, 0x1c, 0xe, 0x10,
        0, 8, 0xe);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x21);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_chapter_cutscene_event_state = 1;
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x22);
    data_fd2_chapter_cutscene_event_state = 0;

    fd2_set_vga_palette_range(0, 0xff, 0x40);
    memset((void *)0xa0000, 0, 64000);

    fd2_init_runtime_char_from_base_growth(5);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_09_end @ 0x235BC  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 9「騎士的抉擇」end handler. Revives runtime char #11 by clearing all
 * of its status flags (flags = 0; the char was previously asleep/disabled —
 * this re-enables it rather than recruiting a new member), pans the view window
 * to (6,1), refreshes the portrait cache for race 4, fires cutscene event 0x24,
 * shows the chapter-end dialog page 4, persists the party's runtime-character
 * state back to the template store, then advances the current-chapter id by 1.
 *
 * In the binary the function ends with `JMP 0x231C6`, a tail-jump into the
 * shared tail of fd2_chapter_04_end @ 0x231C6 (+0xA: PUSH data_fd2_current_chapter_text;
 * dialog page 4; cleanup; save; INC chapter id; RET). It is emitted here as a
 * self-contained function (Layer 2 functional equivalence — the jump-into-middle
 * sharing is not preserved in source).
 *
 * Paired init handler: fd2_chapter_09_init @ 0x3327D.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_09.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_09_end(void)
{
    data_fd2_battle_runtime_char_array_ptr[0xb].flags = 0;
    fd2_pan_cursor_and_window(6, 1);
    fd2_load_chapter_portraits_and_dump_tmp(4);
    fd2_cutscene_event_trigger(0x24);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_10_end @ 0x235F9  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 10「洞窟中的激戰」end handler. Plays the end cutscene that
 * restores and repositions the party after the cave rescue:
 *   fades the screen to black, clears every char's acted flag, then copies
 *   the 11-byte X / Y scene position tables (@ 0x52113 / 0x5211E) onto the
 *   stack and places the 11 party units (chars 0..0xA) at those tiles, each
 *   with sprite_state[1] (facing) = 2.
 *   It then revives/repositions the rescued NPCs that started the battle
 *   asleep or disabled: char 0x32 (索菲亞) -> (15,35) sleep flag cleared;
 *   char 0x33 (卡納恩三世) -> (14,35) sleep flag cleared; char 0x34 -> (16,35)
 *   flags cleared; char 5 flags cleared.
 *   Resets battle_anim_phase, sets the view window origin and cursor-world to
 *   (9,34) and cursor-screen to (0,0), composites one battle frame, fades the
 *   palette back in, and delays 200 ticks.
 *   Shows the chapter-end dialog page 4, resets battle_anim_phase, fires
 *   cutscene event 0x25, shows dialog page 5, persists the party's runtime
 *   state to the template store, recruits char 11 (索菲亞) and char 6 (萊汀)
 *   via fd2_init_runtime_char_from_base_growth, then advances the
 *   current-chapter id by 1.
 *
 * Paired init handler: fd2_chapter_10_init @ 0x3332B (sets chars 0x32/0x33
 *   sleep flag = 100 so they start the battle asleep).
 * Post-action handler: fd2_chapter_10_post_action @ 0x20707 (extra lose if
 *   char 0x32 OR char 0x33 is dead).
 * Walkthrough: assets/chapters/chapter_10.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_10_end(void)
{
    uint8 scene_block_x[11];
    uint8 scene_block_y[11];
    runtime_char *party;
    int i;

    for (i = 0; i < 11; i++) {
        scene_block_x[i] = data_fd2_chapter_ch10_end_scene_char_pos_x_table[i];
        scene_block_y[i] = data_fd2_chapter_ch10_end_scene_char_pos_y_table[i];
    }

    fd2_play_palette_fade_to_black();
    fd2_clear_all_chars_acted_flag();

    for (i = 0; i < 0xb; i++) {
        party = data_fd2_battle_runtime_char_array_ptr + i;
        party->pos_x = scene_block_x[i];
        party->pos_y = scene_block_y[i];
        party->sprite_state[1] = 2;
    }

    data_fd2_battle_runtime_char_array_ptr[0x32].pos_x = 0xf;
    data_fd2_battle_runtime_char_array_ptr[0x32].pos_y = 0x23;
    data_fd2_battle_runtime_char_array_ptr[0x32].status_sleep_flag = 0;
    data_fd2_battle_runtime_char_array_ptr[0x33].pos_x = 0xe;
    data_fd2_battle_runtime_char_array_ptr[0x33].pos_y = 0x23;
    data_fd2_battle_runtime_char_array_ptr[0x33].status_sleep_flag = 0;
    data_fd2_battle_runtime_char_array_ptr[0x34].pos_x = 0x10;
    data_fd2_battle_runtime_char_array_ptr[0x34].pos_y = 0x23;
    data_fd2_battle_runtime_char_array_ptr[0x34].flags = 0;
    data_fd2_battle_runtime_char_array_ptr[5].flags = 0;

    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_view_window_origin_x = 9;
    data_fd2_battle_view_window_origin_y = 0x22;
    data_fd2_battle_cursor_world_x = 9;
    data_fd2_battle_cursor_world_y = 0x22;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;

    fd2_composite_battle_frame(1);
    fd2_play_palette_fade_in();
    __delay_thunk_375b2(200);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x25);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    fd2_init_runtime_char_from_base_growth(0xb);
    fd2_init_runtime_char_from_base_growth(6);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_11_end @ 0x23790  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot @ 0x51E11)
 *
 * Chapter 11「幻之森林」end handler. Shows the chapter-end dialog page 3,
 * persists the party's runtime-character state back to the template store,
 * recruits char #14 (珊) via fd2_init_runtime_char_from_base_growth, then
 * advances the current-chapter id by 1.
 *
 * In the binary the function ends with `PUSH 0xE; CALL
 * fd2_init_runtime_char_from_base_growth; ADD ESP,4; JMP 0x231F2`, a tail-jump
 * into the shared snippet @ 0x231F2 (INC current_chapter_id; RET). That same
 * shared snippet is reached by fd2_chapter_19_end as well; chapter 11 is
 * emitted here as a self-contained function (Layer 2 functional equivalence —
 * the jump-into-middle sharing is not preserved in source).
 *
 * Paired init handler: fd2_chapter_11_init @ 0x33367.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_11.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_11_end(void)
{
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    fd2_init_runtime_char_from_base_growth(0xe);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_12_end @ 0x237D5  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot @ 0x51E15)
 *
 * Chapter 12「北山道」end handler. Unconditionally copies three 14-byte scene
 * tables (recruit-scene X / Y / facing @ 0x52129 / 0x52137 / 0x52145) into
 * on-stack placement blocks and stages the post-battle scene via
 * fd2_setup_chars_and_camera_for_intro (place chars 0..0xD, extra char 0xE at
 * (0xA,2) facing 0, camera origin (4,0)). It then plays the cutscene: shows
 * dialog page 3, fires cutscene event 0x2D, shows dialog page 4, persists the
 * party's runtime-char state to the template store, recruits char #17
 * (米亞斯多德) via fd2_init_runtime_char_from_base_growth, then advances the
 * current-chapter id by 1.
 *
 * In the binary the function ends with `JMP 0x239B1`, a tail-jump into the
 * shared `INC [0x53c03]; RET` snippet that closes fd2_chapter_13_end. It is
 * emitted here as a self-contained function (Layer 2 functional equivalence —
 * the jump-into-middle sharing is not preserved in source).
 *
 * Paired init handler: fd2_chapter_12_init @ 0x333F5.
 * Post-action handler: fd2_chapter_12_post_action @ 0x2073D (extra lose if
 *   char 0xE (米亞斯多德) is dead).
 * Walkthrough: assets/chapters/chapter_12.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_12_end(void)
{
    uint8 scene_block_x[14];
    uint8 scene_block_y[14];
    uint8 scene_block_facing[14];
    int i;

    for (i = 0; i < 14; i++) {
        scene_block_x[i] = data_fd2_chapter_ch12_end_scene_char_pos_x_table[i];
        scene_block_y[i] = data_fd2_chapter_ch12_end_scene_char_pos_y_table[i];
        scene_block_facing[i] = data_fd2_chapter_ch12_end_scene_char_facing_table[i];
    }

    fd2_setup_chars_and_camera_for_intro(
        (uint32)scene_block_x, (uint32)scene_block_y, (uint32)scene_block_facing,
        0, 0xd, 0xe, 0xa, 2, 0, 4, 0);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_cutscene_event_trigger(0x2d);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);

    fd2_save_runtime_char_to_template();
    fd2_init_runtime_char_from_base_growth(0x11);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_13_end @ 0x2389F  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 13「哈斯米爾之戰」end handler. Shows the chapter-end dialog page 9,
 * persists the party's runtime-character state back to the template store,
 * recruits char #3 (哈瓦特) via fd2_init_runtime_char_from_base_growth, then
 * advances the current-chapter id by 1.
 *
 * In the binary the function ends with `PUSH 3; JMP 0x237C8`, a tail-jump into
 * the shared snippet @ 0x237C8 (CALL fd2_init_runtime_char_from_base_growth;
 * ADD ESP,4; JMP 0x231F2 — INC current_chapter_id; RET), reusing the tail of
 * fd2_chapter_11_end. It is emitted here as a self-contained function (Layer 2
 * functional equivalence — the jump-into-middle sharing is not preserved in
 * source).
 *
 * Note: the walkthrough's「哈瓦諾」is a typo for 哈瓦特 (char 3).
 *
 * Paired init handler: fd2_chapter_13_init @ 0x3346B.
 * Post-action handler: fd2_chapter_13_post_action @ 0x20765 (non-default lose
 *   checks: (1) chars[0xF..0x1A] 12 NPC all dead -> lose + dialog page 10;
 *   (2) save_metadata > 5 AND char[0x3B] dead -> lose + dialog page 2).
 * Walkthrough: assets/chapters/chapter_13.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_13_end(void)
{
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    fd2_init_runtime_char_from_base_growth(3);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_14_end @ 0x238DC  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot @ 0x51E1D)
 *
 * Chapter 14「平原的會戰」end handler. No char is recruited — it is a pure
 * scene + dialog + cutscene closer. It unconditionally copies three 16-byte
 * scene tables (post-battle X / Y / facing @ 0x52153 / 0x52163 / 0x52173) into
 * on-stack placement blocks, refreshes the portrait cache for race 1, then
 * stages the post-battle scene via fd2_setup_chars_and_camera_for_intro (place
 * chars 0..0xF, no extra char (extra idx 0 at (0,0) facing 0), camera origin
 * (0xC,10)). It then plays the cutscene: shows dialog page 2, resets
 * battle_anim_phase, fires cutscene event 0x2F, shows dialog page 3, persists
 * the party's runtime-char state to the template store, then advances the
 * current-chapter id by 1.
 *
 * In the binary the function body carries two mid-function entry points reused
 * by other chapter-end handlers as tail-jump targets:
 *   0x239AC <- fd2_chapter_22_end (save; INC chapter id; RET)
 *   0x239B1 <- fd2_chapter_12_end (INC chapter id; RET)
 * Chapter 14 is emitted here as a self-contained function (Layer 2 functional
 * equivalence — the jump-into-middle sharing is not preserved in source).
 *
 * Paired init handler: fd2_chapter_14_init @ 0x3347C.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_14.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_14_end(void)
{
    uint8 scene_block_x[16];
    uint8 scene_block_y[16];
    uint8 scene_block_facing[16];
    int i;

    for (i = 0; i < 16; i++) {
        scene_block_x[i] = data_fd2_chapter_ch14_end_scene_char_pos_x_table[i];
        scene_block_y[i] = data_fd2_chapter_ch14_end_scene_char_pos_y_table[i];
        scene_block_facing[i] = data_fd2_chapter_ch14_end_scene_char_facing_table[i];
    }

    fd2_load_chapter_portraits_and_dump_tmp(1);
    fd2_setup_chars_and_camera_for_intro(
        (uint32)scene_block_x, (uint32)scene_block_y, (uint32)scene_block_facing,
        0, 0xf, 0, 0, 0, 0, 0xc, 10);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x2f);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);

    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_15_end @ 0x239BD  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot @ 0x51E21)
 *
 * Chapter 15「拉卡湖的激戰」end handler. Shows a chapter-end dialog page whose
 * index depends on whether 凱麗 (char #0xC) is currently in the party:
 * fd2_check_party_has_char_id(0xC) returns 1 when present / 0 when absent, and
 * the page is ((al ^ 1) + 0xC) -> page 12 when 凱麗 is present, page 13 when
 * absent. It then persists the party's runtime-character state back to the
 * template store, recruits char #15 (賽可邦勒) via
 * fd2_init_runtime_char_from_base_growth, then advances the current-chapter id
 * by 1.
 *
 * In the binary the function ends with `PUSH 0xF; JMP 0x237C8`, a tail-jump
 * into the shared snippet @ 0x237C8 (CALL fd2_init_runtime_char_from_base_growth;
 * ADD ESP,4; JMP 0x231F2 — INC current_chapter_id; RET), reusing the tail of
 * fd2_chapter_11_end (the same snippet that closes fd2_chapter_13_end). It is
 * emitted here as a self-contained function (Layer 2 functional equivalence —
 * the jump-into-middle sharing is not preserved in source).
 *
 * Paired init handler: fd2_chapter_15_init @ 0x334D9.
 * Post-action handler: fd2_chapter_15_post_action @ 0x20822 (extra lose if
 *   char 0x40 (賽可邦勒) is dead).
 * Walkthrough: assets/chapters/chapter_15.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_15_end(void)
{
    uint8 page;

    page = (uint8)(((uint8)fd2_check_party_has_char_id(0xc) ^ 1) + 0xc);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, page, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    fd2_init_runtime_char_from_base_growth(0xf);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_17_end @ 0x23B5F  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 17「血與冰之刃」end handler. Unconditionally copies two 16-byte scene
 * position tables (post-battle X / Y @ 0x521A3 / 0x521B3) into on-stack placement
 * blocks and persists the party's runtime-char state to the template store. It
 * then branches on whether 蜜蒂 (char #0x12) is currently in the party
 * (fd2_check_party_has_char_id(0x12)):
 *   蜜蒂未加入 (returns 0): stages the post-battle scene via
 *     fd2_setup_chars_and_camera_for_intro — the facing argument is the inline
 *     fixed value 0 (< 4), so every placed char faces direction 0 and there is no
 *     facing table (chars 0..0xF, plus an extra char 0x34 placed at (0x17,0x17)
 *     facing 2, camera origin (0x11,0x11)). Shows the 蜜蒂 farewell dialog page 7,
 *     resets battle_anim_phase, fires cutscene event 0x32, pans the camera/window
 *     to (0x11,0xE), refreshes the portrait cache for race 3, and selects next
 *     cutscene event 0x33.
 *   蜜蒂已加入 (returns non-zero): shows dialog page 5, resets battle_anim_phase,
 *     pans the camera/window to (0x11,0xE), refreshes the portrait cache for race
 *     3, and selects next cutscene event 0x34.
 * It then fires the selected cutscene event, shows dialog page 6, fires cutscene
 * event 0x35, shows dialog page 8, recruits char #16 (凱拉斯, id 0x10) via
 * fd2_init_runtime_char_from_base_growth, then advances the current-chapter id by 1.
 *
 * The position tables are read unconditionally into the stack blocks (matching the
 * binary), but the blocks are only consumed on the 蜜蒂未加入 branch. In the binary
 * this function is self-contained (no fall-through / no jump-into-middle sharing).
 *
 * Paired init handler: fd2_chapter_17_init @ 0x335AA.
 * Post-action handler: fd2_chapter_17_post_action @ 0x20872 (gated lose: 蜜蒂
 *   (char 0x12) 未加入 AND char[0x34] dead -> dialog page 2 + lose).
 * Walkthrough: assets/chapters/chapter_17.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_17_end(void)
{
    uint8 scene_block_x[16];
    uint8 scene_block_y[16];
    uint32 next_cutscene;
    int i;

    for (i = 0; i < 16; i++) {
        scene_block_x[i] = data_fd2_chapter_ch17_end_scene_char_pos_x_table[i];
        scene_block_y[i] = data_fd2_chapter_ch17_end_scene_char_pos_y_table[i];
    }

    fd2_save_runtime_char_to_template();

    if (fd2_check_party_has_char_id(0x12) == 0) {
        fd2_setup_chars_and_camera_for_intro(
            (uint32)scene_block_x, (uint32)scene_block_y, 0, 0, 0xf, 0x34, 0x17,
            0x17, 2, 0x11, 0x11);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x32);
        fd2_pan_cursor_and_window(0x11, 0xe);
        fd2_load_chapter_portraits_and_dump_tmp(3);
        next_cutscene = 0x33;
    }
    else {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_pan_cursor_and_window(0x11, 0xe);
        fd2_load_chapter_portraits_and_dump_tmp(3);
        next_cutscene = 0x34;
    }

    fd2_cutscene_event_trigger(next_cutscene);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_cutscene_event_trigger(0x35);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_init_runtime_char_from_base_growth(0x10);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_18_end @ 0x23CD5  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 18「遙遠的彼岸」end handler. Unconditionally copies three 17-byte
 * scene tables (post-battle X / Y / facing @ 0x521C3 / 0x521D4 / 0x521E5) into
 * on-stack placement blocks, persists the party's runtime-char state to the
 * template store, then stages the post-battle scene via
 * fd2_setup_chars_and_camera_for_intro (place chars 0..0x10, extra char 0x11
 * at (0x19,8) facing 1, camera origin (0x12,4)). It then plays the closing
 * cutscene, interleaving dialog pages with cutscene events:
 *   dialog page 7; battle_anim_phase = 0; cutscene event 0x38;
 *   dialog page 8; battle_anim_phase = 0; cutscene event 0x39;
 *   dialog page 9; battle_anim_phase = 0; cutscene event 0x3A;
 *   dialog page 10 (no trailing cutscene event).
 * It then recruits char #21 (約拿, id 0x15) and char #7 (蘭斯洛特, id 7) via
 * fd2_init_runtime_char_from_base_growth, then advances the current-chapter id
 * by 1.
 *
 * Unlike most sibling handlers this one does not save the party at its tail —
 * fd2_save_runtime_char_to_template runs up front (before the scene setup),
 * matching the binary. In the binary this function is self-contained (no
 * fall-through / no jump-into-middle sharing).
 *
 * Paired init handler: fd2_chapter_18_init @ 0x335DA.
 * Post-action handler: fd2_chapter_18_post_action @ 0x208CF (non-default win/
 *   lose: chars[0, 0x10, 0x11] any dead -> lose; char[0x34] dead -> win).
 * Walkthrough: assets/chapters/chapter_18.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_18_end(void)
{
    uint8 scene_block_x[17];
    uint8 scene_block_y[17];
    uint8 scene_block_facing[17];
    int i;

    for (i = 0; i < 17; i++) {
        scene_block_x[i] = data_fd2_chapter_ch18_end_scene_char_pos_x_table[i];
        scene_block_y[i] = data_fd2_chapter_ch18_end_scene_char_pos_y_table[i];
        scene_block_facing[i] = data_fd2_chapter_ch18_end_scene_char_facing_table[i];
    }

    fd2_save_runtime_char_to_template();
    fd2_setup_chars_and_camera_for_intro(
        (uint32)scene_block_x, (uint32)scene_block_y, (uint32)scene_block_facing,
        0, 0x10, 0x11, 0x19, 8, 1, 0x12, 4);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x38);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x39);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x3a);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 10, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_init_runtime_char_from_base_growth(0x15);
    fd2_init_runtime_char_from_base_growth(7);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_16_end @ 0x23A0A  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9, slot @ 0x51E25)
 *
 * Chapter 16「冰原之戰」end handler. Unconditionally copies two 16-byte scene
 * position tables (post-battle X / Y @ 0x52183 / 0x52193) into on-stack placement
 * blocks and stages the post-battle scene via fd2_setup_chars_and_camera_for_intro
 * — the facing argument is the inline fixed value 0 (< 4), so every placed char
 * faces direction 0 and there is no facing table (chars 0..0xF, plus an extra char
 * 0x41 (蜜蒂) placed at (0x1C,0x1E) facing 2, camera origin (0x16,0x19)).
 *
 * It then counts how many of the 8 cave-NPC subordinates chars[0x42..0x49] died:
 * for i in 0..7, fd2_check_char_is_dead(i + 0x42) increments a dead counter, and
 * dead_exceeds_4 := (dead_count > 4) ? 1 : 0. It persists the party's runtime-char
 * state to the template store, then takes the 蜜蒂 recruit branch:
 *   if save_metadata (turn counter) < 19  AND  dead_exceeds_4 == 0  AND
 *      runtime_char[0] (索爾) hp_max >= 320:
 *     shows the recruit dialog page 4, then recruits char #18 (蜜蒂, id 0x12) via
 *     fd2_init_runtime_char_from_base_growth.
 *   else: shows dialog page 2, resets battle_anim_phase, fires cutscene event 0x31,
 *     shows dialog page 3 — no recruit.
 * It then advances the current-chapter id by 1.
 *
 * 蜜蒂三條件招募: chars[0].hp_max >= 320  +  turn counter <= 18 (< 19)  +
 * chars[0x42..0x49] 8 subordinates dead <= 4.
 *
 * The position tables are read unconditionally into the stack blocks (matching the
 * binary). In the binary this function is self-contained (no fall-through / no
 * jump-into-middle sharing).
 *
 * Paired init handler: fd2_chapter_16_init @ 0x335A0.
 * Post-action handler: fd2_chapter_16_post_action @ 0x2084A (extra lose if char
 *   0x41 (蜜蒂) is dead).
 * Walkthrough: assets/chapters/chapter_16.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_16_end(void)
{
    uint8 scene_block_x[16];
    uint8 scene_block_y[16];
    uint8 dead_count;
    uint8 dead_exceeds_4;
    int   i;

    dead_exceeds_4 = 0;
    for (i = 0; i < 16; i++) {
        scene_block_x[i] = data_fd2_chapter_ch16_end_scene_char_pos_x_table[i];
        scene_block_y[i] = data_fd2_chapter_ch16_end_scene_char_pos_y_table[i];
    }

    dead_count = 0;
    fd2_setup_chars_and_camera_for_intro(
        (uint32)scene_block_x, (uint32)scene_block_y, 0, 0, 0xf, 0x41, 0x1c, 0x1e,
        2, 0x16, 0x19);

    for (i = 0; i < 8; i++) {
        if (fd2_check_char_is_dead(i + 0x42) != 0) {
            dead_count = dead_count + 1;
        }
    }
    if (dead_count > 4) {
        dead_exceeds_4 = 1;
    }

    fd2_save_runtime_char_to_template();

    if ((int32)data_fd2_battle_turn_counter < 0x13 && dead_exceeds_4 != 1 &&
        data_fd2_battle_runtime_char_array_ptr->hp_max >= 0x140) {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        fd2_init_runtime_char_from_base_growth(0x12);
    }
    else {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x31);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140, 0xcd,
                                 0x4c, 0x4a, 0x13, 1);
    }

    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_19_end @ 0x23E39  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 19「黑暗中的狙擊」end handler. Persists the party's runtime-char
 * state to the template store, shows the chapter-end dialog page 3, then
 * advances the current-chapter id by 1. Unlike most sibling handlers the save
 * runs up front (before the dialog), matching the binary's instruction order.
 *
 * In the binary the function ends with `ADD ESP,0x24; JMP 0x231F2`, a tail-jump
 * into the shared snippet @ 0x231F2 (INC current_chapter_id; RET) that also
 * closes fd2_chapter_11_end. It is emitted here as a self-contained function
 * (Layer 2 functional equivalence — the jump-into-middle sharing is not
 * preserved in source). No char is recruited in this handler; 龍劍士巴拿羅西亞
 * recruitment is triggered by an FDFIELD event, not this handler.
 *
 * Paired init handler: fd2_chapter_19_init @ 0x33674 (shared with ch20/21).
 * Post-action handler: fd2_chapter_19_post_action @ 0x20926 (gated lose:
 *   save_metadata > 6 AND char[0x40] dead -> lose, 巴拿羅西亞 死亡視為失敗).
 * Walkthrough: assets/chapters/chapter_19.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_19_end(void)
{
    fd2_save_runtime_char_to_template();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * data_fd2_chapter_cutscene_event_state @ 0x53AFB  (.object2, 4 bytes)
 *
 * Cutscene palette fade-in tween counter / state flag. Read+written as a
 * 32-bit dword by fd2_cutscene_event_trigger @ 0x138B8 (CMP ==0, CMP ==0x40,
 * INC, then PUSH as the palette index arg to fd2_set_vga_palette_range).
 * Writers fd2_chapter_08_end @ 0x23568/0x23586 and fd2_chapter_01_init set it
 * to 1 (arm the fade-in) then back to 0 (disarm); value range 0..0x40.
 *
 * Zero-initialized runtime state (memory image all-zero; first touched by an
 * init handler write before any read). Lives here with chend1.c per the
 * data-emit home assignment (multi-writer across field/chinit.c,
 * field/chend1.c, field/chtrans.c).
 * ---------------------------------------------------------------- */
uint32 data_fd2_chapter_cutscene_event_state;


/* ----------------------------------------------------------------
 * data_fd2_chapter_current_chapter_id @ 0x53C03  (.object2, 4 bytes)
 *
 * Current chapter id (0-based engine chapter index). Read+written as a 32-bit
 * dword. Used everywhere as an unsigned index: resource loads
 * (FDFIELD.DAT idx = id*3, FDTXT.DAT idx = id+1) in
 * fd2_load_save_and_init_engine @ 0x10147, and per-chapter dispatch tables
 * (CALL [id*4 + 0x51DE9], CALL [id*4 + 0x51D71], MOVZX [id + 0x51E63]) in
 * fd2_main_menu_continue_dispatcher @ 0x25E1E/0x25E35/0x25E42.
 *
 * Writers set it from the save header byte pBuf[0x30C5] (load), or to a chapter
 * constant on chapter entry/exit: fd2_chapter_01_init @ 0x32326/0x3252E/0x327EB
 * (MOV dword [0x53C03], 0x20 / 0x1F / 0), the chapter-end handlers in this file
 * (INC current_chapter_id), and fd2_load_state_from_selected_slot @ 0x30316.
 *
 * Zero-initialized runtime state (memory image all-zero; first touched by a
 * load/init handler write before any read). Lives here with chend1.c per the
 * data-emit home assignment (multi-writer across field/chend1.c, field/chend2.c,
 * field/chinit.c, life/main.c, save/save.c).
 * ---------------------------------------------------------------- */
uint32 data_fd2_chapter_current_chapter_id;
