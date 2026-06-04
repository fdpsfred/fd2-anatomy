/*
 * chend2.c — chapter end handlers (group 2)
 *
 * fd2_chapter_20_end @ 0x23E74 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[20])
 * fd2_chapter_21_end @ 0x240FA (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[21])
 * fd2_chapter_22_end @ 0x244B6 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[22])
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
    __delay_thunk_375b2(200);

    fd2_display_dialog_scene(current_chapter_text, 0xB, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_cutscene_event_trigger(0x3B);
    fd2_display_dialog_scene(current_chapter_text, 0xC, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    data_fd2_battle_anim_phase = 0;
    fd2_init_runtime_char_from_base_growth(0x19);
    fd2_save_runtime_char_to_template();

    if ((int32)data_fd2_battle_turn_counter < 0x10) {
        fd2_load_chapter_portraits_and_dump_tmp(1);
        fd2_cutscene_event_trigger(0x3C);
        fd2_display_dialog_scene(current_chapter_text, 0xE, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x3D);
        fd2_display_dialog_scene(current_chapter_text, 0xF, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x3E);
        fd2_display_dialog_scene(current_chapter_text, 0x10, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_init_runtime_char_from_base_growth(0x1C);
    }

    fd2_display_dialog_scene(current_chapter_text, 0xD, 0xA0000, 0x140,
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
    fd2_display_dialog_scene(current_chapter_text, 5, 0xA0000, 0x140,
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
        fd2_display_dialog_scene(current_chapter_text, 7, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x3F);
        fd2_display_dialog_scene(current_chapter_text, 8, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        data_fd2_battle_anim_phase = 0;
        fd2_cutscene_event_trigger(0x40);
        fd2_display_dialog_scene(current_chapter_text, 9, 0xA0000, 0x140,
                                 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_play_chapter_intro_sprite_slideshow();
        final_page = 10;
    } else {
        final_page = 6;
    }

    fd2_display_dialog_scene(current_chapter_text, final_page, 0xA0000, 0x140,
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
    fd2_display_dialog_scene(current_chapter_text, 4, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_cutscene_event_trigger(0x41);
    fd2_display_dialog_scene(current_chapter_text, 5, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_pan_cursor_and_window(0x10, 0x10);
    fd2_cutscene_event_trigger(0x42);
    fd2_display_dialog_scene(current_chapter_text, 6, 0xA0000, 0x140,
                             0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_pan_cursor_and_window(0x10, 0xE);

    fd2_cast_screen_wide_spell_with_fade(data_fd2_battle_cursor_screen_x,
                                         data_fd2_battle_cursor_screen_y + 3,
                                         10, 8);
    __delay_thunk_375b2(500);
    memset((void *)0xA0000, 0xFF, 64000);
    fd2_play_palette_fade_to_black();
    memset((void *)0xA0000, 0, 64000);

    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = data_fd2_chapter_current_chapter_id + 1;
}
