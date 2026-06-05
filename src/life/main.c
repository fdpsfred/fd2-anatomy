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
#include <dos.h>

/* ----------------------------------------------------------------
 * fd2_main @ 0x25BF4  (1 caller: __CMain @ 0x45D4B)
 *
 * FD2 game entry point. __CMain pushes (argv, argc) and consumes the
 * EAX result as the DOS exit code (PUSH EAX; JMP _exit), but the
 * original source is void main(): neither argc/argv nor a return value
 * is used by the body, and the function physically tail-jumps into a
 * shared epilogue (Watcom merged-epilogue optimization). Emitted as a
 * normal void function (pipeline_spec pattern A, rule A-1): Watcom 9.5a
 * regenerates an equivalent POP/RET epilogue; the shared tail-jump is
 * not reproduced and is not required for Layer-2 equivalence.
 *
 * One-shot init: AIL sound startup + driver/handle allocation, eight
 * fd2_load_dat_resource loads (FDOTHER/FDTXT banks), three work-buffer
 * mallocs, INT 10h mode-13h set, and an RNG warm-up of rand()%256
 * fd2_advance_rng_state() iterations seeded off the BIOS tick low word.
 *
 * Main loop: outer = main-menu BGM + fd2_main_menu_continue_dispatcher;
 * if it returns 0, run the inner gameplay loop (fd2_game_main_loop +
 * chapter-clear / chapter-switch dispatch keyed on
 * data_fd2_chapter_event_or_battle_end_code @ 0x53ECC). Quit drops to
 * AIL_shutdown + INT 10h text mode 3.
 * ---------------------------------------------------------------- */
void fd2_main(void)
{
    uint32 menu_result;
    uint32 game_loop_result;
    uint32 exit_inner_loop;
    uint32 calibration_iter;
    uint32 rng_warmup_count;

    AIL_startup();

    data_fd2_audio_bgm_driver_handle = (void *)AIL_install_MDI_INI();
    if (data_fd2_audio_bgm_driver_handle != NULL) {
        data_fd2_audio_bgm_driver_available_flag = 1;
        data_fd2_audio_bgm_sequence_handle =
            (uint32)AIL_allocate_sequence_handle(
                data_fd2_audio_bgm_driver_handle);
    }

    data_fd2_audio_sfx_dig_driver_handle = (uint32)AIL_install_DIG_INI();
    if (data_fd2_audio_sfx_dig_driver_handle != 0) {
        data_fd2_audio_sfx_driver_available_flag = 1;
        data_fd2_audio_sfx_sample_handle_0 =
            (uint32)AIL_allocate_sample_handle(
                (void *)data_fd2_audio_sfx_dig_driver_handle);
        data_fd2_audio_sfx_sample_handle_1 =
            (uint32)AIL_allocate_sample_handle(
                (void *)data_fd2_audio_sfx_dig_driver_handle);
    }

    data_fd2_audio_fdother_sfx_bank_buf_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_audio_fdother_sfx_bank_buf_ptr, 0x1F);
    data_fd2_runtime_battle_state_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_runtime_battle_state_ptr, 1);
    data_fd2_menu_dialog_state_handle =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_menu_dialog_state_handle, 2);
    data_fd2_tile_anim_table_base =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_tile_anim_table_base, 3);
    data_fd2_chinese_font_sheet =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_chinese_font_sheet, 4);
    data_fd2_ui_anim_sprite_sheet_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_ui_anim_sprite_sheet_ptr, 5);
    data_fd2_all_game_text_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdtxt_dat,
            data_fd2_all_game_text_ptr, 0);
    data_fd2_resource_portrait_sheet_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_resource_portrait_sheet_ptr, 6);

    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)malloc(0x20);
    data_fd2_large_game_state_buffer_ptr = (uint32)malloc(0x25680);
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)malloc(0xA00);

    *(uint16 *)&data_fd2_input_last_key_pressed = 0x13;
    int386(0x10, (union REGS *)&data_fd2_input_last_key_pressed,
                 (union REGS *)&data_fd2_input_last_key_pressed);

    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)(int32)*(int16 *)0x46C;
    rng_warmup_count = rand();
    for (calibration_iter = 0;
         (int32)calibration_iter < (int32)rng_warmup_count % 0x100;
         calibration_iter++) {
        fd2_advance_rng_state();
    }

    do {
        fd2_set_bgm_track_with_fade(0x12, 0);
        menu_result = fd2_main_menu_continue_dispatcher();
        if (menu_result == 0) {
            do {
                game_loop_result = fd2_game_main_loop();
                if (data_fd2_chapter_event_or_battle_end_code == 1) {
                    data_fd2_ui_play_active_flag = 0;
                    fd2_play_chapter_clear_fanfare();
                    data_fd2_ui_play_active_flag = 1;
                    data_fd2_chapter_event_or_battle_end_code = 0;
                    game_loop_result = 1;
                } else if (data_fd2_chapter_event_or_battle_end_code == 2) {
                    data_fd2_ui_play_active_flag = 0;
                    fd2_set_bgm_track_with_fade(0xFFFFFFFF, 1);
                    data_fd2_chapter_end_handler_table
                        [data_fd2_chapter_current_chapter_id]();
                    exit_inner_loop = fd2_chapter_transition_menu();
                    if (exit_inner_loop == 0) {
                        data_fd2_chapter_init_handler_table
                            [data_fd2_chapter_current_chapter_id]();
                        fd2_set_bgm_track_with_fade(
                            (uint32)data_fd2_audio_per_chapter_player_turn_bgm_track
                                [data_fd2_chapter_current_chapter_id], 0);
                    } else {
                        menu_result = 1;
                    }
                    data_fd2_ui_play_active_flag = 1;
                    data_fd2_chapter_event_or_battle_end_code = 0;
                    fd2_clear_keyboard_buffer();
                    game_loop_result = exit_inner_loop;
                }
            } while (game_loop_result == 0);
            if (game_loop_result == 0xFFFFFFFF) {
                menu_result = 1;
            }
        } else if (menu_result == 0xFFFFFFFF) {
            menu_result = 0;
        }
    } while (menu_result == 0);

    AIL_shutdown();
    *(uint16 *)&data_fd2_input_last_key_pressed = 3;
    int386(0x10, (union REGS *)&data_fd2_input_last_key_pressed,
                 (union REGS *)&data_fd2_input_last_key_pressed);
}

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
            fd2_load_dat_resource(
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
        free((void *)data_fd2_ui_menu_screen_sprite_atlas_buf_ptr);
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

/* ----------------------------------------------------------------
 * fd2_load_save_and_init_engine @ 0x10010  (2 callers)
 *
 * Main LOAD GAME path. Reads FD2.SAV, validates checksum, decrypts,
 * restores all engine state + reloads chapter resources, then plays
 * the load-game cinematic intro.
 *
 * FD2.SAV layout (0x59CB bytes); tail 4 bytes = checksum @ +0x59C7.
 * On any malloc failure: reset video mode (INT 10h AX=0003) then
 * printf(" Out of Memory !!!\n") + exit(1).
 * ---------------------------------------------------------------- */
void fd2_load_save_and_init_engine(void)
{
    char *err_msg;
    uint8 *pBuf;
    void *fp;
    uint8 *pTileEvent;
    uint8 scene_id;
    int i;
    int sq;
    uint32 saved_block;
    uint32 row_offset;

    pBuf = (uint8 *)malloc(0x59CB);
    if (pBuf == NULL) {
        *(uint16 *)&data_fd2_input_last_key_pressed = 3;
        int386(0x10, (union REGS *)&data_fd2_input_last_key_pressed,
                     (union REGS *)&data_fd2_input_last_key_pressed);
        err_msg = data_fd2_string_save_load_oom_msg_load_pbuf_50004;
        printf(err_msg);
        exit(1);
    }

    fp = fopen("FD2.SAV", "rb");
    fread(pBuf, 1, 0x59CB, fp);
    fclose(fp);
    fd2_save_crypt_buffer((uint32)pBuf, 0x59CB);
    if (fd2_save_compute_checksum((uint32)pBuf, 0x59CB)
            != *(uint32 *)(pBuf + 0x59C7)) {
        fd2_load_chapter_portrait(0x4B);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1B4,
                                 0xA9F23, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        fd2_wait_for_input_dialog_with_blink(0);
        fd2_close_status_screen_with_slide_out();
    }

    fd2_play_palette_fade_to_black();
    memmove((void *)data_fd2_shared_menu_party_roster_buffer_ptr,
            pBuf + 0x8A3, 0xA00);
    data_fd2_vga_palette_data_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_vga_palette_data_ptr, 0);
    data_fd2_chapter_current_chapter_id = (uint32)pBuf[0x30C5];
    chapter_portrait_load_buffer =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdfield_dat_51a59,
            chapter_portrait_load_buffer,
            data_fd2_chapter_current_chapter_id * 3 + 2);

    if (data_fd2_tile_event_data_table_ptr != 0)
        free((void *)data_fd2_tile_event_data_table_ptr);
    data_fd2_tile_event_data_table_ptr = (uint32)malloc(0x8A3);
    if (data_fd2_tile_event_data_table_ptr == 0) {
        *(uint16 *)&data_fd2_input_last_key_pressed = 3;
        int386(0x10, (union REGS *)&data_fd2_input_last_key_pressed,
                     (union REGS *)&data_fd2_input_last_key_pressed);
        err_msg = data_fd2_string_save_load_oom_msg_tile_event_50023;
        printf(err_msg);
        exit(1);
    }

    memmove((void *)data_fd2_tile_event_data_table_ptr, pBuf, 0x8A3);
    fd2_load_chapter_background_layers();
    current_chapter_text =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdtxt_dat,
            current_chapter_text,
            data_fd2_chapter_current_chapter_id + 1);
    data_fd2_battle_tile_map_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdfield_dat_51a59,
            data_fd2_battle_tile_map_ptr,
            data_fd2_chapter_current_chapter_id * 3);
    data_fd2_battle_map_width_tiles =
        (int)*(int16 *)data_fd2_battle_tile_map_ptr;
    data_fd2_battle_map_height_tiles =
        (int)*(int16 *)(data_fd2_battle_tile_map_ptr + 2);
    scene_id = *(uint8 *)data_fd2_tile_event_data_table_ptr;
    battle_scene_snapshot =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
            battle_scene_snapshot, (uint32)scene_id * 2);
    data_fd2_tile_attribute_flags_buffer_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
            data_fd2_tile_attribute_flags_buffer_ptr,
            (uint32)scene_id * 2 + 1);
    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
    pTileEvent = (uint8 *)data_fd2_tile_event_data_table_ptr;
    data_fd2_resource_portrait_cache_total_size = (uint32)pTileEvent[1];
    data_fd2_resource_portrait_cache_alloc_offset = (uint32)pTileEvent[2];
    data_fd2_battle_party_member_count = (uint32)pBuf[0x30C4];

    if (data_fd2_battle_runtime_char_array_ptr != NULL)
        free(data_fd2_battle_runtime_char_array_ptr);
    data_fd2_battle_runtime_char_array_ptr =
        (runtime_char *)malloc(0x1E00);
    if (data_fd2_battle_runtime_char_array_ptr == NULL) {
        *(uint16 *)&data_fd2_input_last_key_pressed = 3;
        int386(0x10, (union REGS *)&data_fd2_input_last_key_pressed,
                     (union REGS *)&data_fd2_input_last_key_pressed);
        err_msg = data_fd2_string_save_load_oom_msg_runtime_char_50037;
        printf(err_msg);
        exit(1);
    }

    memmove(data_fd2_battle_runtime_char_array_ptr, pBuf + 0x12A3,
            data_fd2_battle_party_member_count * 0x50);
    memmove((void *)data_fd2_field_map_tile_event_consumed_flags_ptr,
            pBuf + 0x30A3, 0x20);
    if (portrait_sprite_cache != 0)
        free((void *)portrait_sprite_cache);

    fp = fopen("FDICON.B24", "rb");
    data_fd2_resource_portrait_cache_count = 0;
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        int slot;
        slot = fd2_load_portrait_to_cache(
            (uint32)data_fd2_battle_runtime_char_array_ptr[i].portrait_id,
            (uint32)fp);
        data_fd2_battle_runtime_char_array_ptr[i].sprite_state[0] =
            (uint8)slot;
    }
    fclose(fp);

    fp = fopen("FD2.TMP", "wb");
    fwrite((void *)portrait_sprite_cache, 1, 0x32A00, fp);
    fclose(fp);

    data_fd2_battle_turn_counter = (uint32)pBuf[0x30C3];
    data_fd2_battle_view_window_origin_x = (uint32)pBuf[0x30C6];
    data_fd2_battle_view_window_origin_y = (uint32)pBuf[0x30C7];
    data_fd2_battle_cursor_world_x = (uint32)pBuf[0x30C8];
    data_fd2_battle_cursor_world_y = (uint32)pBuf[0x30C9];
    data_fd2_battle_cursor_screen_x = (uint32)pBuf[0x30CA];
    data_fd2_battle_cursor_screen_y = (uint32)pBuf[0x30CB];
    data_fd2_shared_menu_party_member_count = (uint32)pBuf[0x30CC];
    data_fd2_shared_party_total_gold = *(uint32 *)(pBuf + 0x30CD);
    data_fd2_ui_game_speed_flag = pBuf[0x30D1];
    data_fd2_ui_terrain_hud_user_enabled = pBuf[0x30D2];
    data_fd2_audio_bgm_enabled_flag = pBuf[0x30D3];
    data_fd2_audio_sfx_enabled_flag = pBuf[0x30D4];

    free(pBuf);
    free((void *)chapter_portrait_load_buffer);
    chapter_portrait_load_buffer = 0;

    fd2_set_bgm_track_with_fade(
        (uint32)data_fd2_audio_per_chapter_player_turn_bgm_track
            [data_fd2_chapter_current_chapter_id], 0);
    data_fd2_battle_anim_phase = 0;
    fd2_tick_tile_event_animations();
    fd2_composite_battle_frame(1);
    fd2_play_palette_fade_in();

    /* Cinematic intro: 9 sprite frames @ sheet idx 0x53..0x5B;
       last 3 overlay save_metadata number. */
    for (i = 0; i < 9; i++) {
        saved_block = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr, 0xA0000, 0x140, 0x78,
            0x54, (uint32)(i + 0x53));
        if (i > 6) {
            fd2_render_decimal_number_to_buffer(
                0xA726B, 0x140, data_fd2_battle_turn_counter, 0x2A, 3);
        }
        __delay_thunk_375b2(0x46);
        if (i == 8)
            __delay_thunk_375b2(500);
        fd2_cleanup_dialog_sprite_buffer(saved_block, 0xA0000, 0x140);
    }

    /* 4-frame zoom-in cinematic into large_game_state_buffer overlay
       (row ratio i*i); skips from i=4 directly to i=9. */
    for (i = 2; i < 6; i++) {
        if (i == 5)
            i = 9;
        sq = i * i;
        saved_block = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8, 0x74,
            (uint32)(sq + 0x54), 0x5B);
        row_offset = (uint32)(sq + 0x5A) * 0x1C8;
        fd2_render_decimal_number_to_buffer(
            data_fd2_large_game_state_buffer_ptr + 0x812F + row_offset,
            0x1C8, data_fd2_battle_turn_counter, 0x2A, 3);
        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8,
            0x138, 0xC0);
        fd2_wait_n_bios_ticks(1);
        fd2_cleanup_dialog_sprite_buffer(saved_block,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8);
    }

    fd2_composite_battle_frame(0);
    __delay_thunk_375b2(200);
    data_fd2_battle_current_active_char_idx = 0;
    data_fd2_battle_anim_phase = 1;
    fd2_clear_keyboard_buffer();
}
