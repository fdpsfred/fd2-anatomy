/*
 * chend2.c — chapter end handlers (group 2)
 *
 * fd2_chapter_20_end @ 0x23E74 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[20])
 * fd2_chapter_21_end @ 0x240FA (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[21])
 * fd2_chapter_22_end @ 0x244B6 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[22])
 * fd2_chapter_23_end @ 0x24754 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[23])
 * fd2_chapter_24_end @ 0x24C1E (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[24])
 * fd2_chapter_25_end @ 0x24DF2 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[25])
 * fd2_chapter_26_end @ 0x24E80 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[26])
 * fd2_chapter_27_end @ 0x250CC (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[27])
 * fd2_chapter_28_end @ 0x25464 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[28])
 * fd2_chapter_29_end @ 0x2548C (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[29])
 * fd2_chapter_30_end @ 0x25757 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[30])
 */

#include <string.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * Chapter-20 end-scene character position tables (FD2.LE data @
 * 0x521F6 / 0x52206 / 0x52216 / 0x5221F). Private read-only tables
 * referenced only by fd2_chapter_20_end; the Watcom source materialized
 * them as const arrays that the prologue copies into stack scratch before
 * the placement loops index them.
 *   scene1 (chars 0..15)   : 16-byte x + 16-byte y
 *   scene2 (chars 0x34..0x3C): 9-byte x + 9-byte y
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch20_end_scene1_char_pos_x_table[16] = {
    0x21, 0x21, 0x21, 0x22, 0x22, 0x22, 0x23, 0x23,
    0x23, 0x23, 0x23, 0x24, 0x24, 0x24, 0x24, 0x24
};
const uint8 data_fd2_chapter_ch20_end_scene1_char_pos_y_table[16] = {
    0x23, 0x24, 0x22, 0x22, 0x23, 0x24, 0x21, 0x22,
    0x23, 0x24, 0x25, 0x21, 0x22, 0x23, 0x24, 0x25
};
const uint8 data_fd2_chapter_ch20_end_scene2_char_pos_x_table[9] = {
    0x1E, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1D, 0x1D, 0x1D
};
const uint8 data_fd2_chapter_ch20_end_scene2_char_pos_y_table[9] = {
    0x23, 0x25, 0x24, 0x23, 0x22, 0x21, 0x24, 0x23, 0x22
};

/* ----------------------------------------------------------------
 * fd2_chapter_20_end @ 0x23E74  — Chapter 20「死亡般的沈寂」end handler.
 *
 * Large cutscene transition: fade out, place the surviving party (slots
 * 0..15) and the NPC/scene-2 cast (slots 0x34..0x3C) at scripted world
 * positions, re-aim the camera, fade in, then run the post-battle dialog
 * sequence. Always recruits 謝多 (char 0x19); if the battle was won within
 * 15 turns (turn_counter < 16) it also plays the extended 達可賽 scene and
 * recruits 達可賽 (char 0x1C). Finishes by advancing current_chapter_id.
 *
 * Walkthrough SOT: assets/chapters/chapter_20.md
 * ---------------------------------------------------------------- */
void fd2_chapter_20_end(void)
{
    uint8 scene1_x[16];
    uint8 scene1_y[16];
    uint8 scene2_x[9];
    uint8 scene2_y[9];
    runtime_char *pChar;
    int i;

    for (i = 0; i < 16; i++) {
        scene1_x[i] = data_fd2_chapter_ch20_end_scene1_char_pos_x_table[i];
        scene1_y[i] = data_fd2_chapter_ch20_end_scene1_char_pos_y_table[i];
    }
    for (i = 0; i < 9; i++) {
        scene2_x[i] = data_fd2_chapter_ch20_end_scene2_char_pos_x_table[i];
        scene2_y[i] = data_fd2_chapter_ch20_end_scene2_char_pos_y_table[i];
    }

    fd2_play_palette_fade_to_black();
    fd2_clear_all_chars_acted_flag();

    for (i = 0; i < 16; i++) {
        pChar = data_fd2_battle_runtime_char_array_ptr + i;
        pChar->pos_x = scene1_x[i];
        pChar->pos_y = scene1_y[i];
        pChar->sprite_state[1] = 1;
    }
    for (i = 0; i < 9; i++) {
        pChar = data_fd2_battle_runtime_char_array_ptr + i + 0x34;
        pChar->pos_x = scene2_x[i];
        pChar->pos_y = scene2_y[i];
        pChar->sprite_state[1] = 3;
    }

    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_view_window_origin_x = 0x1A;
    data_fd2_battle_view_window_origin_y = 0x1F;
    data_fd2_battle_cursor_world_x = 0x1A;
    data_fd2_battle_cursor_world_y = 0x1F;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;

    fd2_composite_battle_frame(1);
    fd2_play_palette_fade_in();
    fd2_delay_ms(200);

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x3B);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xC, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_init_runtime_char_from_base_growth(0x19);
    fd2_save_runtime_char_to_template();

    if ((int32)data_fd2_battle_turn_counter < 0x10) {
        fd2_load_chapter_portraits_and_dump_tmp(1);
        fd2_cutscene_event_trigger(0x3C);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xE, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x3D);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xF, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x3E);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0x10, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_init_runtime_char_from_base_growth(0x1C);
    }

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xD, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * Chapter-21 end-scene character tables (FD2.LE data @ 0x52228 /
 * 0x52241 / 0x5225A). Private read-only tables referenced only by
 * fd2_chapter_21_end. Each has a 25-byte extent (7 chars x 4-byte
 * stride minus the trailing 1 byte); the Watcom prologue copies them
 * onto stack scratch (6 dwords + 1 byte each) before passing pointers
 * into fd2_setup_chars_and_camera_for_intro, which reads the bytes
 * indexed by char slot.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch21_end_scene_char_pos_x_table[25] = {
    0x15, 0x14, 0x16, 0x16, 0x13, 0x13, 0x13, 0x13,
    0x12, 0x14, 0x14, 0x14, 0x12, 0x15, 0x15, 0x15,
    0x15, 0x13, 0x12, 0x16, 0x11, 0x11, 0x11, 0x17,
    0x17
};
const uint8 data_fd2_chapter_ch21_end_scene_char_pos_y_table[25] = {
    0x0E, 0x0E, 0x0D, 0x0E, 0x0E, 0x0F, 0x10, 0x11,
    0x0E, 0x0F, 0x10, 0x11, 0x0D, 0x0F, 0x10, 0x11,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0C, 0x0D, 0x0E, 0x0C,
    0x0D
};
const uint8 data_fd2_chapter_ch21_end_scene_char_facing_table[25] = {
    0x02, 0x02, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x01,
    0x01
};

/* ----------------------------------------------------------------
 * fd2_chapter_21_end @ 0x240FA  — Chapter 21「亞述森林」end handler.
 *
 * Copies the three 25-byte end-scene tables onto the stack, places the
 * party / NPC cast and re-aims the camera via
 * fd2_setup_chars_and_camera_for_intro, then runs the post-battle dialog
 * (page 5). Counts how many of the six collectible items 0xD1..0xD6
 * (黃金徽章 + 5 顆眼) the party currently holds across inventory slots
 * 0..15. If all six are held it consumes them, awards item 100 (天空之鑰,
 * the hidden-stage key), plays the extended cutscene (pages 7/8/9 with
 * events 0x3F/0x40 and the intro sprite slideshow) and shows final page
 * 10; otherwise it shows page 6. Finishes by re-initialising 希爾法 (0x18)
 * and 羅蘭 (0x17) from base+growth, saving the runtime char templates, and
 * advancing current_chapter_id.
 *
 * Walkthrough SOT: assets/chapters/chapter_21.md
 * ---------------------------------------------------------------- */
void fd2_chapter_21_end(void)
{
    uint8 pos_x[25];
    uint8 pos_y[25];
    uint8 facing[25];
    int collected;
    int slot_idx;
    uint8 item_id;
    uint8 slot;
    uint32 final_page;
    int i;

    for (i = 0; i < 25; i++) {
        pos_x[i] = data_fd2_chapter_ch21_end_scene_char_pos_x_table[i];
        pos_y[i] = data_fd2_chapter_ch21_end_scene_char_pos_y_table[i];
        facing[i] = data_fd2_chapter_ch21_end_scene_char_facing_table[i];
    }

    collected = 0;
    fd2_setup_chars_and_camera_for_intro((uint32)pos_x, (uint32)pos_y,
                                         (uint32)facing, 0, 0x18, 0x19,
                                         0x17, 0xE, 1, 0xE, 10);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);

    for (item_id = 0xD1; item_id < 0xD7; item_id++) {
        for (slot = 0; slot < 0x10; slot++) {
            if (fd2_find_inventory_slot_with_item(slot, item_id) != -1) {
                collected++;
            }
        }
    }

    if (collected == 6) {
        for (item_id = 0xD1; item_id < 0xD7; item_id++) {
            for (slot = 0; slot < 0x10; slot++) {
                slot_idx = fd2_find_inventory_slot_with_item(slot, item_id);
                if (slot_idx != -1) {
                    fd2_remove_inventory_slot_at(slot, slot_idx);
                }
            }
        }
        fd2_give_item_to_first_player_char(100);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x3F);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x40);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_play_chapter_intro_sprite_slideshow();
        final_page = 10;
    } else {
        final_page = 6;
    }

    fd2_display_dialog_scene(data_fd2_current_chapter_text, final_page, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_init_runtime_char_from_base_growth(0x18);
    fd2_init_runtime_char_from_base_growth(0x17);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * Chapter-22 end-scene character tables (FD2.LE data @ 0x52273 /
 * 0x52283 / 0x52293). Private read-only tables referenced only by
 * fd2_chapter_22_end; the Watcom prologue copies each 16-byte table onto
 * stack scratch as four dwords before fd2_setup_chars_and_camera_for_intro
 * indexes them by char slot. The facing table is uniform 0x02 except slot 1
 * (希爾法) which faces 0x00.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch22_end_scene_char_pos_x_table[16] = {
    0x16, 0x16, 0x15, 0x17, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x14, 0x15, 0x16, 0x17, 0x18, 0x15, 0x17
};
const uint8 data_fd2_chapter_ch22_end_scene_char_pos_y_table[16] = {
    0x16, 0x14, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x17, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19
};
const uint8 data_fd2_chapter_ch22_end_scene_char_facing_table[16] = {
    0x02, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
};

/* ----------------------------------------------------------------
 * fd2_chapter_22_end @ 0x244B6  — Chapter 22「遠古呼喚」end handler.
 *
 * Copies the three 16-byte end-scene tables onto the stack, places chars
 * 0..15 plus the additional NPC slot 0x48 and re-aims the camera via
 * fd2_setup_chars_and_camera_for_intro, then runs the post-battle dialog
 * (pages 4/5/6) interleaved with cutscene events 0x41/0x42 and two
 * cursor/window pans. The FD2-unique white→black fade ending follows:
 * fd2_cast_screen_wide_spell_with_fade centred on the cursor, a 500-tick
 * hold, a white-screen flash (memset 0xA0000 to 0xFF), a palette fade to
 * black, then a black-screen clear (memset 0xA0000 to 0). Finishes by
 * saving the runtime char templates and advancing current_chapter_id (the
 * tail shares fd2_chapter_14_end's epilogue snippet @ 0x239AC). No char is
 * added in the handler — 龍騎士莎拉 joins via an FDFIELD event.
 *
 * Walkthrough SOT: assets/chapters/chapter_22.md
 * ---------------------------------------------------------------- */
void fd2_chapter_22_end(void)
{
    uint8 pos_x[16];
    uint8 pos_y[16];
    uint8 facing[16];
    int i;

    for (i = 0; i < 16; i++) {
        pos_x[i] = data_fd2_chapter_ch22_end_scene_char_pos_x_table[i];
        pos_y[i] = data_fd2_chapter_ch22_end_scene_char_pos_y_table[i];
        facing[i] = data_fd2_chapter_ch22_end_scene_char_facing_table[i];
    }

    fd2_setup_chars_and_camera_for_intro((uint32)pos_x, (uint32)pos_y,
                                         (uint32)facing, 0, 0xF, 0x48, 0x16,
                                         0x19, 2, 0x10, 0x12);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_cutscene_event_trigger(0x41);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 5, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_pan_cursor_and_window(0x10, 0x10);
    fd2_cutscene_event_trigger(0x42);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_pan_cursor_and_window(0x10, 0xE);

    fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x,
                                         data_fd2_battle_cursor_screen_y + 3,
                                         10, 8);
    fd2_delay_ms(500);
    memset((void *)0xA0000, 0xFF, 64000);
    fd2_play_palette_fade_to_black();
    memset((void *)0xA0000, 0, 64000);

    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * Chapter-23 end-scene character tables (FD2.LE data @ 0x522A3 /
 * 0x522B4 / 0x522C5). Private read-only tables referenced only by
 * fd2_chapter_23_end; the Watcom prologue copies each as four dwords plus a
 * trailing byte (17-byte extent) onto stack scratch before
 * fd2_setup_chars_and_camera_for_intro indexes them by char slot. Five chars
 * are placed (slots 0, 0x10, 0x11). The facing table is uniform 0x00 except
 * the trailing byte (0x02).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch23_end_scene_char_pos_x_table[17] = {
    0x14, 0x14, 0x12, 0x13, 0x14, 0x15, 0x16, 0x12,
    0x16, 0x12, 0x13, 0x15, 0x16, 0x13, 0x14, 0x15,
    0x13
};
const uint8 data_fd2_chapter_ch23_end_scene_char_pos_y_table[17] = {
    0x13, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x11,
    0x11, 0x10, 0x10, 0x10, 0x10, 0x0F, 0x0F, 0x0F,
    0x15
};
const uint8 data_fd2_chapter_ch23_end_scene_char_facing_table[17] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02
};

/* ----------------------------------------------------------------
 * fd2_chapter_23_end @ 0x24754  — Chapter 23「向天空之旅」end handler
 * (largest end handler at 960 bytes; 0 direct callers, dispatched via
 * data_fd2_chapter_end_handler_table[23]).
 *
 * Phase 1 (conditional joins + cutscenes): copies the three 17-byte
 * end-scene tables onto the stack and places the cast / re-aims the camera
 * via fd2_setup_chars_and_camera_for_intro, then runs three story-branch
 * decisions, saves the runtime char templates, and advances
 * current_chapter_id:
 *   - 天空之鑰 (item 100) NOT held -> dialog page 9, cutscene 0x47;
 *     otherwise dialog page 8 and recruit 卡里斯 (char 0x16).
 *   - 蜜蒂 (char_id 0x12) present in the template roster -> dialog page 10,
 *     cutscene 0x48, mark 蜜蒂 (char 0x11) dead, dialog page 11.
 *   - 蜜蒂 absent and the battle ended within 15 turns (turn_counter < 15)
 *     -> dialog page 13 and recruit 羅德曼 (char 0x13); otherwise dialog
 *     page 12, cutscene 0x48, mark 蜜蒂 (char 0x11) dead.
 *
 * Phase 2 (mid-end transition to the second battlefield): three dialog
 * pages (14/15/16) interleaved with rising pre-cast effects, two screen
 * shakes, and 400-tick holds, a palette fade-in loop, then it reloads the
 * battlefield resources (FDFIELD.DAT[0x45] tile map, FDSHAP.DAT[0x2E] scene
 * snapshot, FDSHAP.DAT[0x2F] tile-attribute flags), obfuscates the tile map,
 * reloads the chapter background layers, performs two cursor/window pans
 * around cutscene event 0x49 (fired three times), and shows final page 17.
 *
 * Walkthrough SOT: assets/chapters/chapter_23.md
 * ---------------------------------------------------------------- */
void fd2_chapter_23_end(void)
{
    uint8 pos_x[17];
    uint8 pos_y[17];
    uint8 facing[17];
    uint32 v;
    int i;

    for (i = 0; i < 17; i++) {
        pos_x[i] = data_fd2_chapter_ch23_end_scene_char_pos_x_table[i];
        pos_y[i] = data_fd2_chapter_ch23_end_scene_char_pos_y_table[i];
        facing[i] = data_fd2_chapter_ch23_end_scene_char_facing_table[i];
    }

    fd2_setup_chars_and_camera_for_intro((uint32)pos_x, (uint32)pos_y,
                                         (uint32)facing, 0, 0x10, 0x11, 0x15,
                                         0x15, 2, 0xE, 0xE);

    if (fd2_any_char_has_item(100) == -1) {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x47);
    } else {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_init_runtime_char_from_base_growth(0x16);
    }

    if (fd2_find_template_char_by_id(0x12) != 0) {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 10, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x48);
        fd2_mark_char_as_dead(0x11);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
    } else if ((int32)data_fd2_battle_turn_counter < 0xF) {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xD, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_init_runtime_char_from_base_growth(0x13);
    } else {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xC, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x48);
        fd2_mark_char_as_dead(0x11);
    }

    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xE, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_delay_ms(400);
    fd2_play_rising_pre_cast_effect(1, 0xF, 10);
    fd2_animate_screen_shake(0x1E);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xF, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_delay_ms(400);
    fd2_play_rising_pre_cast_effect(1, 0xF, 10);
    fd2_animate_screen_shake(0x1E);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0x10, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_delay_ms(400);
    fd2_play_rising_pre_cast_effect(1, 0x1E, 0x10);

    for (v = 0; (int32)v < 0x40; v += 2) {
        fd2_set_vga_palette_range_with_add(0, 0xFF, v);
        fd2_delay_ms(4);
    }

    data_fd2_battle_tile_map_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdfield_dat_51a59,
        data_fd2_battle_tile_map_ptr, 0x45);
    data_fd2_battle_scene_snapshot = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
        data_fd2_battle_scene_snapshot, 0x2E);
    data_fd2_tile_attribute_flags_buffer_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
        data_fd2_tile_attribute_flags_buffer_ptr, 0x2F);
    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
    fd2_load_chapter_background_layers();
    fd2_pan_cursor_and_window(0xE, 0x1D);
    fd2_set_vga_palette_range_with_add(0, 0xFF, 0);
    fd2_cutscene_event_trigger(0x49);
    fd2_pan_cursor_and_window(0xE, 0xE);
    fd2_cutscene_event_trigger(0x49);
    fd2_cutscene_event_trigger(0x49);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0x11, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* ----------------------------------------------------------------
 * fd2_chapter_24_end @ 0x24C1E  — Chapter 24「在天空的彼方」end handler
 * (0 direct callers, dispatched via data_fd2_chapter_end_handler_table[24]).
 *
 * Text-scroll cinematic (no char added). Shows dialog page 2, then scrolls
 * text lines 2..9 with a 30-frame composite hold per line; shows dialog
 * page 3, then scrolls text lines 10..14 with a 12-frame palette fade-out
 * per line (brightness_sub continues across all five lines for 60 total
 * increments -> full fade-out). Finishes by blacking the framebuffer,
 * saving the runtime char templates, and advancing current_chapter_id.
 *
 * Walkthrough SOT: assets/chapters/chapter_24.md
 * ---------------------------------------------------------------- */
void fd2_chapter_24_end(void)
{
    uint32 brightness_sub;
    uint32 line;
    int f;

    brightness_sub = 0;
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 2, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;

    for (line = 2; (int32)line < 10; line++) {
        fd2_scroll_text_screen_up_by_lines(line);
        for (f = 0; f < 30; f++) {
            fd2_composite_battle_frame(1);
            fd2_wait_n_bios_ticks(1);
        }
    }

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;

    for (; (int32)line < 15; line++) {
        fd2_scroll_text_screen_up_by_lines(line);
        for (f = 0; f < 12; f++) {
            fd2_set_vga_palette_range(0, 0xFF, brightness_sub);
            fd2_composite_battle_frame(0);
            fd2_wait_n_bios_ticks(1);
            brightness_sub++;
        }
    }

    memset((void *)0xA0000, 0, 64000);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_25_end @ 0x24DF2  — Chapter 25「火焰的審判」end handler
 * (0 direct callers, dispatched via data_fd2_chapter_end_handler_table[25]).
 *
 * Straight-line, no-branch dialog/cutscene handler (no RNG, no numeric
 * computation, no CALL-return value used). It shows dialog page 6, pans the
 * cursor/window to (4,0x10), reloads the chapter portrait set into the
 * FD2.TMP swap file (fd2_load_chapter_portraits_and_dump_tmp with race 2),
 * fires cutscene event 0x4B, shows dialog page 7, then re-initialises 聖寇拉斯
 * (char 0x1A) from base+growth and saves the runtime char templates — so
 * 聖寇拉斯 is persisted. It finishes through fd2_chapter_11_end's shared tail
 * snippet @ 0x237C8 (entered via a PUSH 0x1D ; JMP): re-initialise 亞奇梅吉
 * (char 0x1D) from base+growth and advance current_chapter_id. Because 亞奇梅吉
 * is initialised AFTER the save, it joins the runtime roster but is NOT
 * persisted into the template chars (binary design; the walkthrough omits it).
 *
 * Walkthrough SOT: assets/chapters/chapter_25.md
 * ---------------------------------------------------------------- */
void fd2_chapter_25_end(void)
{
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_pan_cursor_and_window(4, 0x10);
    fd2_load_chapter_portraits_and_dump_tmp(2);
    fd2_cutscene_event_trigger(0x4B);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_init_runtime_char_from_base_growth(0x1A);
    fd2_save_runtime_char_to_template();

    /* shared tail @ 0x237C8 (fd2_chapter_11_end's epilogue snippet), entered
     * via PUSH 0x1D ; JMP: recruit 亞奇梅吉 then advance the chapter id. */
    fd2_init_runtime_char_from_base_growth(0x1D);
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * Chapter-26 end-scene character tables (FD2.LE data @ 0x522D6 /
 * 0x522E6 / 0x522F6). Private read-only tables referenced only by
 * fd2_chapter_26_end; the Watcom prologue copies each 16-byte table onto
 * stack scratch as four dwords before fd2_setup_chars_and_camera_for_intro
 * indexes them by char slot. The facing table is uniform 0x02 except slots 0
 * and 2 which face 0x00.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch26_end_scene_char_pos_x_table[16] = {
    0x0E, 0x0F, 0x0F, 0x0E, 0x10, 0x0E, 0x0F, 0x10,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x0E, 0x0F, 0x10
};
const uint8 data_fd2_chapter_ch26_end_scene_char_pos_y_table[16] = {
    0x06, 0x09, 0x06, 0x09, 0x09, 0x0A, 0x0A, 0x0A,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C, 0x0C
};
const uint8 data_fd2_chapter_ch26_end_scene_char_facing_table[16] = {
    0x00, 0x02, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
};

/* ----------------------------------------------------------------
 * fd2_chapter_26_end @ 0x24E80  -- Chapter 26「未知的迴廊」end handler
 * (0 direct callers, dispatched via data_fd2_chapter_end_handler_table[26]).
 *
 * Copies the three 16-byte end-scene tables onto the stack, then force-
 * positions any party char in slots 0x10..party_member_count whose portrait
 * id is 0x1F (機器人渥德) at world tile (0x10, 6) before placing the cast and
 * re-aiming the camera via fd2_setup_chars_and_camera_for_intro. It then runs
 * five post-battle dialog pages interleaved with cutscene events 0x4D..0x50.
 * Two of the pages are chosen dynamically from which of the five 寶箱 (treasure
 * boxes) the party opened, recorded in tile_event_consumed_flags[0xC] (value
 * 0..4): the first dialog uses page (flag + 5) and the third uses page
 * (flag + 8); pages 7, 10 and 11 are fixed. Finishes by saving the runtime
 * char templates and advancing current_chapter_id by one. No char is added in
 * the handler -- 機器人渥德 joins via an FDFIELD event; this handler only
 * positions it.
 *
 * Walkthrough SOT: assets/chapters/chapter_26.md
 * ---------------------------------------------------------------- */
void fd2_chapter_26_end(void)
{
    uint8 pos_x[16];
    uint8 pos_y[16];
    uint8 facing[16];
    runtime_char *pChar;
    int i;

    for (i = 0; i < 16; i++) {
        pos_x[i] = data_fd2_chapter_ch26_end_scene_char_pos_x_table[i];
        pos_y[i] = data_fd2_chapter_ch26_end_scene_char_pos_y_table[i];
        facing[i] = data_fd2_chapter_ch26_end_scene_char_facing_table[i];
    }

    for (i = 0x10; (uint32)i < data_fd2_battle_party_member_count; i++) {
        pChar = data_fd2_battle_runtime_char_array_ptr + i;
        if (pChar->portrait_id == 0x1F) {
            pChar->pos_x = 0x10;
            pChar->pos_y = 6;
        }
    }

    fd2_setup_chars_and_camera_for_intro((uint32)pos_x, (uint32)pos_y,
                                         (uint32)facing, 0, 0xF, 0, 0, 0, 0,
                                         9, 5);

    fd2_display_dialog_scene(
        data_fd2_current_chapter_text,
        (uint32)*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0xC) + 5,
        0xA0000, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x4D);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x4E);
    fd2_display_dialog_scene(
        data_fd2_current_chapter_text,
        (uint32)*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr + 0xC) + 8,
        0xA0000, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x4F);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 10, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x50);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);

    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * Chapter-27 end-scene character tables (FD2.LE data @ 0x52306 /
 * 0x52316 / 0x52326). Private read-only data referenced only by
 * fd2_chapter_27_end; the Watcom prologue copies each 16-byte table onto
 * stack scratch as four dwords (the placement of slots 0..15) before
 * fd2_setup_chars_and_camera_for_intro indexes them by char slot. The
 * trailing byte @ 0x52326 (value 0x01) is copied as a one-byte scratch
 * (var_10) whose address is later handed to
 * fd2_animate_status_effect_overlay_flicker on the bad-ending path; the
 * routine ignores the pointee, so the byte is vestigial.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch27_end_scene_char_pos_x_table[16] = {
    0x0F, 0x0F, 0x0C, 0x0D, 0x11, 0x12, 0x0D, 0x0E,
    0x10, 0x11, 0x0E, 0x0F, 0x10, 0x0E, 0x0F, 0x10
};
const uint8 data_fd2_chapter_ch27_end_scene_char_pos_y_table[16] = {
    0x0D, 0x0B, 0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D,
    0x0D, 0x0D, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F
};
const uint8 data_fd2_chapter_ch27_end_scene_vestigial_byte = 0x01;

/* ----------------------------------------------------------------
 * fd2_chapter_27_end @ 0x250CC  — Chapter 27「命運的交會點」end handler
 * (0 direct callers, dispatched via data_fd2_chapter_end_handler_table[27]).
 * This is the FD2 GOOD/BAD ending fork.
 *
 * Copies the two 16-byte end-scene tables onto the stack, snapshots the
 * vestigial byte, resets every active runtime_char's flags byte (slots
 * 0..15), then places the cast / re-aims the camera via
 * fd2_setup_chars_and_camera_for_intro and runs the opening dialog (page 8)
 * and cutscene event 0x52. The ending then forks on whether any party char
 * holds 天空之鑰 (item 100):
 *
 *   GOOD PATH (key held): dialog pages 9/10/11/12 interleaved with cutscene
 *     events 0x53/0x54 and a cursor/window pan, a sequence of additive
 *     over-bright palette pulses (fd2_palette_overbright_settle_step_loop)
 *     with shrinking 500/250/100/50-tick holds, a screen-wide spell cast
 *     centred on the cursor, a 500-tick hold, a white-screen flash
 *     (memset 0xA0000 to 0xFF), a palette fade to black, then a black-screen
 *     clear (memset 0xA0000 to 0). It finishes by saving the runtime char
 *     templates, advancing current_chapter_id, restoring all chars to full
 *     HP/MP, and returning (the tail shares the epilogue of
 *     fd2_render_party_status_overview_content @ 0x1B5EA) so play continues
 *     into chapter 28.
 *
 *   BAD PATH (key NOT held): dialog pages 13/14/15 interleaved with cutscene
 *     events 0x54/0x52, a status-effect overlay flicker, then 悠妮
 *     (runtime_char[1]) is warped off the field
 *     (fd2_animate_warp_teleport_char from her current tile), final dialog
 *     page 16, all chars restored to full HP/MP, and the game-over cinematic
 *     (fd2_play_game_ending_cinematic). It then hard-locks in an infinite
 *     loop — the binary's "沒天空之鑰悠妮獨自回黃金城無法玩" game-over.
 *
 * Walkthrough SOT: assets/chapters/chapter_27.md
 * ---------------------------------------------------------------- */
void fd2_chapter_27_end(void)
{
    uint8 pos_x[16];
    uint8 pos_y[16];
    uint8 vestigial;
    int i;

    for (i = 0; i < 16; i++) {
        pos_x[i] = data_fd2_chapter_ch27_end_scene_char_pos_x_table[i];
        pos_y[i] = data_fd2_chapter_ch27_end_scene_char_pos_y_table[i];
    }
    vestigial = data_fd2_chapter_ch27_end_scene_vestigial_byte;

    for (i = 0; i < 16; i++) {
        data_fd2_battle_runtime_char_array_ptr[i].flags = 0;
    }

    fd2_setup_chars_and_camera_for_intro((uint32)pos_x, (uint32)pos_y, 2, 0,
                                         0xF, 0, 0, 0, 0, 9, 8);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 8, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x52);

    if (fd2_any_char_has_item(100) != -1) {
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x53);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 10, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_pan_cursor_and_window(9, 8);
        fd2_cutscene_event_trigger(0x54);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_palette_overbright_settle_step_loop(0x50, 5);
        fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xC, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_palette_overbright_settle_step_loop(0x50, 4);
        fd2_delay_ms(500);
        fd2_palette_overbright_settle_step_loop(0x50, 3);
        fd2_delay_ms(250);
        fd2_palette_overbright_settle_step_loop(0x50, 2);
        fd2_delay_ms(100);
        fd2_palette_overbright_settle_step_loop(0x50, 2);
        fd2_delay_ms(50);
        fd2_palette_overbright_settle_step_loop(0x50, 2);
        fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x,
                                             data_fd2_battle_cursor_screen_y - 1,
                                             10, 10);
        fd2_delay_ms(500);
        memset((void *)0xA0000, 0xFF, 64000);
        fd2_play_palette_fade_to_black();
        memset((void *)0xA0000, 0, 64000);
        fd2_save_runtime_char_to_template();
        data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
        fd2_restore_all_chars_full_hp_mp();
        return;
    }

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xD, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x54);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xE, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x52);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xF, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_animate_status_effect_overlay_flicker(0, 0x13, 1, (uint32)&vestigial);
    fd2_animate_warp_teleport_char(
        1, 0xFF, 0xFF,
        (uint32)data_fd2_battle_runtime_char_array_ptr[1].pos_x,
        (uint32)data_fd2_battle_runtime_char_array_ptr[1].pos_y);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0x10, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_restore_all_chars_full_hp_mp();
    fd2_play_game_ending_cinematic();
    for (;;) {
    }
}

/* ----------------------------------------------------------------
 * fd2_chapter_28_end @ 0x25464  -- Chapter 28「探索者」end handler
 * (0 direct callers, dispatched via data_fd2_chapter_end_handler_table[28]).
 * The simplest end handler (40 bytes).
 *
 * Pushes the eight standard fd2_display_dialog_scene args (page 7) then
 * tail-jumps (JMP 0x231DF) into fd2_chapter_04_end's shared epilogue tail,
 * which pushes data_fd2_current_chapter_text, runs the dialog scene, saves the
 * runtime char templates, and advances current_chapter_id by one. No char
 * added, no cutscene -- pure dialog (page 7) + save + chapter advance.
 *
 * Walkthrough SOT: assets/chapters/chapter_28.md
 * ---------------------------------------------------------------- */
void fd2_chapter_28_end(void)
{
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 7, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * fd2_chapter_29_end @ 0x2548C  — Chapter 29「無邊的黑暗之中」end handler
 * (0 direct callers, dispatched via data_fd2_chapter_end_handler_table[29]).
 *
 * Straight-line dramatic cutscene (no branch, no char added). It shows dialog
 * page 10, wipes the HP of every runtime_char from slot 0x14 to the party tail
 * (fd2_kill_runtime_chars_from_index_to_end), then transmutes slot 0x14 into
 * its 變身 form by overwriting both its portrait_id and char_id with 0x7E. It
 * shows page 11, reloads the chapter portrait set into the FD2.TMP swap file
 * (race 9), pans the cursor/window to (9,8) and animates the cursor to tile
 * (0xF,10), warps the last party member (party_member_count - 1) onto tile
 * (0xF,10), and shows page 12.
 *
 * It then plays three rounds of earthquake screen-shake interleaved with
 * dialog pages 13/14/15, each round resetting battle_anim_phase to 0 first:
 *   round 1 (12->13): shake 0x14, 600ms, shake 0x14, 600ms, shake 0x14;
 *   round 2 (13->14): shake 0x14, 200ms, shake 0x14, 200ms, shake 0x14;
 *   round 3 (14->15): shake 0x14, 200ms, shake 0x14, 100ms, triple-strength
 *     shake 0x28, 200ms, then three white palette-flash pulses with 300ms
 *     holds, then page 15.
 *
 * Finishes with a 64-step palette fade-out (brightness 0->0x3F, 4ms/step), a
 * black-screen clear (memset 0xA0000 to 0) held 800ms, a 63-step palette
 * fade-in (brightness 0x3E->0, 4ms/step), then saves the runtime char
 * templates and advances current_chapter_id by one.
 *
 * Walkthrough SOT: assets/chapters/chapter_29.md
 * ---------------------------------------------------------------- */
void fd2_chapter_29_end(void)
{
    uint32 v;

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 10, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_kill_runtime_chars_from_index_to_end(0x14);
    data_fd2_battle_runtime_char_array_ptr[0x14].portrait_id = 0x7E;
    data_fd2_battle_runtime_char_array_ptr[0x14].char_id = 0x7E;
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_load_chapter_portraits_and_dump_tmp(9);
    fd2_pan_cursor_and_window(9, 8);
    fd2_pan_cursor_to_tile_animated(0xF, 10);
    fd2_animate_warp_teleport_char(data_fd2_battle_party_member_count - 1,
                                   0xF, 10, 0xF, 10);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xC, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;

    fd2_animate_screen_shake(0x14);
    fd2_delay_ms(600);
    fd2_animate_screen_shake(0x14);
    fd2_delay_ms(600);
    fd2_animate_screen_shake(0x14);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xD, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;

    fd2_animate_screen_shake(0x14);
    fd2_delay_ms(200);
    fd2_animate_screen_shake(0x14);
    fd2_delay_ms(200);
    fd2_animate_screen_shake(0x14);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xE, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;

    fd2_animate_screen_shake(0x14);
    fd2_delay_ms(200);
    fd2_animate_screen_shake(0x14);
    fd2_delay_ms(100);
    fd2_animate_screen_shake(0x28);
    fd2_delay_ms(200);
    fd2_animate_palette_flash_pulse_white();
    fd2_delay_ms(300);
    fd2_animate_palette_flash_pulse_white();
    fd2_delay_ms(300);
    fd2_animate_palette_flash_pulse_white();
    fd2_delay_ms(300);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xF, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);

    for (v = 0; (int32)v < 0x40; v++) {
        fd2_set_vga_palette_range_with_add(0, 0xFF, v);
        fd2_delay_ms(4);
    }
    memset((void *)0xA0000, 0, 64000);
    fd2_delay_ms(800);
    for (v = 0x3E; -1 < (int32)v; v--) {
        fd2_set_vga_palette_range_with_add(0, 0xFF, v);
        fd2_delay_ms(4);
    }

    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}

/* ----------------------------------------------------------------
 * Chapter-30 end-scene character tables (FD2.LE data @ 0x52327 /
 * 0x5233B / 0x5234F). Private read-only data referenced only by
 * fd2_chapter_30_end; three 20-byte tables (one byte per char slot,
 * slots 0..0x13) that the Watcom prologue copies onto stack scratch as
 * five dwords each (MOVSD x5) before fd2_setup_chars_and_camera_for_intro
 * indexes them by char slot. The facing table's slot-1 byte is 0x00
 * (the remaining 19 are 0x02).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch30_end_scene_char_pos_x_table[20] = {
    0x16, 0x16, 0x14, 0x15, 0x17, 0x18, 0x14, 0x15, 0x17, 0x18,
    0x14, 0x15, 0x16, 0x17, 0x18, 0x14, 0x15, 0x16, 0x17, 0x18
};
const uint8 data_fd2_chapter_ch30_end_scene_char_pos_y_table[20] = {
    0x17, 0x13, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19, 0x19
};
const uint8 data_fd2_chapter_ch30_end_scene_char_facing_table[20] = {
    0x02, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
};

/* ----------------------------------------------------------------
 * fd2_chapter_30_end @ 0x25757  — Chapter 30「傳說的終章－結局」end handler
 * (0 direct callers, dispatched via data_fd2_chapter_end_handler_table[30]).
 * The FD2 GOOD ENDING.
 *
 * Copies the three 20-byte end-scene tables onto the stack, places the cast
 * (slots 0..0x13) / re-aims the camera via fd2_setup_chars_and_camera_for_intro,
 * and runs dialog page 9, cutscene event 0x58, and dialog page 10. It pans the
 * cursor/window to (0x10,0x12) and animates the cursor to tile (0x16,0x17), then
 * plays the final-boss death visual: a screen-wide spell centred on the cursor
 * (cursor_screen_y + 1) and a full HP/MP restore. It advances current_chapter_id
 * to 31 (the out-of-range epilogue index), restores HP/MP again, and loads the
 * chapter 31 epilogue map (fd2_load_chapter_battle_data).
 *
 * It re-centres the battle window / cursor at world (0xB,5) with cursor_screen at
 * (0,0), composites one frame, then runs a 64-step palette fade-in
 * (brightness 0x3E->0, 4ms/step) and a 40-frame composite hold (1 BIOS tick each).
 * It shows epilogue dialog page 0, pans the cursor/window to (0xB,0xC), runs
 * cutscene event 0x59, and shows epilogue dialog page 1. Finally it plays the
 * staff-roll cinematic (fd2_play_game_ending_cinematic) and hard-locks in an
 * infinite loop — the game terminates here.
 *
 * Walkthrough SOT: assets/chapters/chapter_30.md
 * ---------------------------------------------------------------- */
void fd2_chapter_30_end(void)
{
    uint8 pos_x[20];
    uint8 pos_y[20];
    uint8 facing[20];
    int i;
    uint32 v;

    for (i = 0; i < 20; i++) {
        pos_x[i] = data_fd2_chapter_ch30_end_scene_char_pos_x_table[i];
        pos_y[i] = data_fd2_chapter_ch30_end_scene_char_pos_y_table[i];
        facing[i] = data_fd2_chapter_ch30_end_scene_char_facing_table[i];
    }

    fd2_setup_chars_and_camera_for_intro((uint32)pos_x, (uint32)pos_y,
                                         (uint32)facing, 0, 0x13, 0, 0, 0, 0,
                                         0x10, 0x12);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 9, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x58);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 10, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_pan_cursor_and_window(0x10, 0x12);
    fd2_pan_cursor_to_tile_animated(0x16, 0x17);

    fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x,
                                         data_fd2_battle_cursor_screen_y + 1,
                                         10, 8);
    fd2_restore_all_chars_full_hp_mp();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
    fd2_restore_all_chars_full_hp_mp();
    data_fd2_battle_anim_phase = 0;
    fd2_load_chapter_battle_data(data_fd2_chapter_current_chapter_id);

    data_fd2_battle_view_window_origin_x = 0xB;
    data_fd2_battle_view_window_origin_y = 5;
    data_fd2_battle_cursor_world_x = 0xB;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;
    fd2_composite_battle_frame(1);

    for (v = 0x3E; -1 < (int32)v; v--) {
        fd2_set_vga_palette_range_with_add(0, 0xFF, v);
        fd2_delay_ms(4);
    }
    for (i = 0; i < 0x28; i++) {
        fd2_composite_battle_frame(0);
        fd2_wait_n_bios_ticks(1);
    }

    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_and_window(0xB, 0xC);
    fd2_cutscene_event_trigger(0x59);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 1, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_play_game_ending_cinematic();
    for (;;) {
    }
}
