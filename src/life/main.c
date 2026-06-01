/*
 * main.c — FD2 game entry point and main-menu dispatcher.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_main_menu_continue_dispatcher @ 0x25EBB
 *
 * Main-menu: NEW GAME / CONTINUE / fallback. Returns 0 (menu),
 * 1 (gameplay), or -1 (quit) for fd2_main's outer loop.
 * ---------------------------------------------------------------- */
int fd2_main_menu_continue_dispatcher(void)
{
    int menu_choice;
    uint8 *pBuf;
    void *fp;
    uint8 *src;
    int slot_result;

    menu_choice = fd2_play_ending_and_record_clear();

    if (menu_choice == 0) {
        fd2_play_palette_fade_to_black();
        data_fd2_chapter_current_chapter_id = 0;
        data_fd2_vga_palette_data_ptr =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                data_fd2_vga_palette_data_ptr, 0);
        data_fd2_shared_menu_party_member_count = 0;
        data_fd2_ui_play_active_flag = 0;
        data_fd2_chapter_init_handler_table
            [data_fd2_chapter_current_chapter_id]();
        fd2_set_bgm_track_with_fade(
            (uint32)data_fd2_audio_per_chapter_player_turn_bgm_track
                [data_fd2_chapter_current_chapter_id], 0);
        data_fd2_ui_play_active_flag = 1;
        fd2_clear_keyboard_buffer();
        return 0;
    }

    if (menu_choice == 1) {
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr =
            (void *)fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                (uint32)data_fd2_ui_menu_screen_sprite_atlas_buf_ptr,
                0xD);
        fd2_play_palette_fade_to_black();
        data_fd2_vga_palette_data_ptr =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                data_fd2_vga_palette_data_ptr, 0);
        memset((void *)0xA0000, 0, 64000);
        fd2_set_vga_palette_range(0, 0xFF, 0);

        pBuf = (uint8 *)malloc(0x59CB);
        fp = fopen("FD2.SAV", "rb");
        if (fp == NULL) {
            memset(pBuf, 0xFF, 0x59CB);
        } else {
            fread(pBuf, 1, 0x59CB, fp);
            fd2_save_crypt_buffer((uint32)pBuf, 0x59CB);
            fclose(fp);
        }

        data_fd2_ui_menu_cursor_idx = 0;
        do {
            slot_result = fd2_save_slot_selector_ui(
                (uint32)pBuf, 0);
            if (slot_result != -1) {
                src = pBuf
                    + data_fd2_ui_menu_cursor_idx * 0xA28
                    + 0x312B;
                memmove(
                    (void *)data_fd2_shared_menu_party_roster_buffer_ptr,
                    src, 0xA00);
                data_fd2_chapter_current_chapter_id =
                    (uint32)src[0xA00];
                data_fd2_shared_menu_party_member_count =
                    (uint32)src[0xA01];
                data_fd2_shared_party_total_gold =
                    *(uint32 *)(src + 0xA02);
                data_fd2_ui_terrain_hud_user_enabled =
                    src[0xA06];
                data_fd2_ui_game_speed_flag = src[0xA07];
                data_fd2_audio_bgm_enabled_flag = src[0xA08];
                data_fd2_audio_sfx_enabled_flag = src[0xA09];
                if (data_fd2_chapter_current_chapter_id == 0xFF)
                    slot_result = 0;
            }
            fd2_close_intro_dialog_with_slide_out();
        } while (slot_result == 0);

        free(pBuf);
        free(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr);
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;

        if (slot_result == 1) {
            data_fd2_ui_play_active_flag = 0;
            slot_result = fd2_chapter_transition_menu();
            if (slot_result == 0) {
                data_fd2_chapter_init_handler_table
                    [data_fd2_chapter_current_chapter_id]();
                fd2_set_bgm_track_with_fade(
                    (uint32)data_fd2_audio_per_chapter_player_turn_bgm_track
                        [data_fd2_chapter_current_chapter_id],
                    0);
            }
            data_fd2_ui_play_active_flag = 1;
        }

        fd2_clear_keyboard_buffer();
        return slot_result;
    }

    fd2_set_bgm_track_with_fade(0xFFFFFFFF, 0);
    fd2_load_save_and_init_engine();
    fd2_set_bgm_track_with_fade(
        (uint32)data_fd2_audio_per_chapter_player_turn_bgm_track
            [data_fd2_chapter_current_chapter_id], 0);
    return 0;
}
