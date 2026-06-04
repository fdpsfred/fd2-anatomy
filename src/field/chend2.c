/*
 * chend2.c — chapter end handlers (group 2)
 *
 * fd2_chapter_20_end @ 0x23E74 (0 direct callers; dispatched via
 *                               data_fd2_chapter_end_handler_table[20])
 */

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
