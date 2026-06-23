/*
 * main.c -- FD2 game entry point and main-menu dispatcher.
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
 * main @ 0x25BF4  (1 caller: __CMain @ 0x45D4B)
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
 * NAMING (rebuild special case): this is literally `main` -- the C entry point
 * the Watcom CRT startup (__CMain) calls, so it MUST carry that exact name. It is
 * the one game function exempt from the project's `fd2_` prefix convention (every
 * other game-logic function is named fd2_*). The Ghidra/src name had followed that
 * convention as fd2_main, but the CRT entry contract requires `main`, so it is
 * restored to `main` here for the src-only FD2.EXE link. Separately, because the
 * TEST build also links tests/testmain.c's own main() (the test runner), genbuild
 * compiles ONLY this file with -Dmain=fd2_main there to avoid a duplicate-main
 * link error; fd2.exe compiles it as `main`.
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
void main(void)
{
    uint32 menu_result;
    uint32 game_loop_result;
    uint32 exit_inner_loop;
    uint32 calibration_iter;
    uint32 rng_warmup_count;

    AIL_startup();

    data_fd2_audio_bgm_driver_handle = AIL_install_MDI_INI();
    if (data_fd2_audio_bgm_driver_handle != NULL) {
        data_fd2_audio_bgm_driver_available_flag = 1;
        data_fd2_audio_bgm_sequence_handle =
            AIL_allocate_sequence_handle(
                data_fd2_audio_bgm_driver_handle);
    }

    data_fd2_audio_sfx_dig_driver_handle = AIL_install_DIG_INI();
    if (data_fd2_audio_sfx_dig_driver_handle != NULL) {
        data_fd2_audio_sfx_driver_available_flag = 1;
        data_fd2_audio_sfx_sample_handle_0 =
            AIL_allocate_sample_handle(
                data_fd2_audio_sfx_dig_driver_handle);
        data_fd2_audio_sfx_sample_handle_1 =
            AIL_allocate_sample_handle(
                data_fd2_audio_sfx_dig_driver_handle);
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
#ifdef FD2_REPLAY
    rng_warmup_count = 0;   /* deterministic warm-up; script SEED sets state */
#else
    rng_warmup_count = rand();
#endif
    for (calibration_iter = 0;
         (int32)calibration_iter < (int32)rng_warmup_count % 0x100;
         calibration_iter++) {
        fd2_advance_rng_state();
    }

#ifdef FD2_REPLAY
    fd2_replay_init();
#endif
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
 * Main-menu dispatcher. Runs the title/record-clear menu
 * (fd2_play_ending_and_record_clear) and branches on its result:
 *   choice 0 -> NEW GAME    (chapter 1 init, BGM, returns 0)
 *   choice 1 -> CONTINUE    (load FD2.SAV slot via selector UI,
 *                            returns fd2_chapter_transition_menu
 *                            result: 0 commit, -1 back out)
 *   else     -> fallback    (engine reload, returns 0)
 *
 * Return code consumed by main's outer loop (NOTE inverted from
 * what the labels suggest):
 *   0  -> run gameplay this iteration, then re-show the menu
 *   1  -> quit the game (exit outer loop)
 *   -1 -> stay in / re-enter the main menu
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
    const char *err_msg;
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
    data_fd2_chapter_portrait_load_buffer =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdfield_dat_51a59,
            data_fd2_chapter_portrait_load_buffer,
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
    data_fd2_current_chapter_text =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdtxt_dat,
            data_fd2_current_chapter_text,
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
    data_fd2_battle_scene_snapshot =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
            data_fd2_battle_scene_snapshot, (uint32)scene_id * 2);
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
    if (data_fd2_portrait_sprite_cache != 0)
        free((void *)data_fd2_portrait_sprite_cache);

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
    fwrite((void *)data_fd2_portrait_sprite_cache, 1, 0x32A00, fp);
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
    free((void *)data_fd2_chapter_portrait_load_buffer);
    data_fd2_chapter_portrait_load_buffer = 0;

    fd2_set_bgm_track_with_fade(
        (uint32)data_fd2_audio_per_chapter_player_turn_bgm_track
            [data_fd2_chapter_current_chapter_id], 0);
    data_fd2_battle_anim_phase = 0;
    fd2_tick_tile_event_animations();
    fd2_composite_battle_frame(1);
    fd2_play_palette_fade_in();

    /* Cinematic intro: 9 sprite frames @ sheet idx 0x53..0x5B;
       last 3 overlay the restored turn counter (data_fd2_battle_turn_counter). */
    for (i = 0; i < 9; i++) {
        saved_block = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr, 0xA0000, 0x140, 0x78,
            0x54, (uint32)(i + 0x53));
        if (i > 6) {
            fd2_render_decimal_number_to_buffer(
                0xA726B, 0x140, data_fd2_battle_turn_counter, 0x2A, 3);
        }
        fd2_delay_ms(0x46);
        if (i == 8)
            fd2_delay_ms(500);
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
    fd2_delay_ms(200);
    data_fd2_battle_current_active_char_idx = 0;
    data_fd2_battle_anim_phase = 1;
    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * Owned global data
 * ---------------------------------------------------------------- */

/*
 * data_fd2_battle_map_height_tiles @ 0x53AC5 -- height of the current battle map
 * in tiles. Companion of data_fd2_battle_map_width_tiles @ 0x53AC1. Loaded at
 * chapter/save-load time from the FDFIELD.DAT tile-map header: both writers
 * (fd2_load_chapter_battle_data @ 0x10932 and
 * fd2_load_save_and_init_engine @ 0x1022e) do
 *   data_fd2_battle_map_height_tiles = (int)*(short *)(_battle_tile_map + 2);
 * i.e. read the second 16-bit field of the decrypted tile map (MOVSX, signed)
 * and widen it into this 32-bit slot. It is then consumed by ~17 readers as the
 * row count / y-axis bottom-edge limit for tile-grid traversal, e.g. cursor
 * clamping uses `data_fd2_battle_map_height_tiles - 1`. Every access is a full 32-bit
 * dword load/store and the value is a small positive tile count. Zero-initialized
 * in the image; the first use is the load-time write, so this is a zero-init
 * (BSS) scalar.
 */
uint32 data_fd2_battle_map_height_tiles;

/*
 * data_fd2_resource_portrait_sheet_ptr @ 0x53AD1 -- base pointer of the loaded
 * portrait / combat-panel sprite sheet (FDOTHER.DAT resource index 6). Holds the
 * malloc'd resource buffer returned by fd2_load_dat_resource; stored as a 32-bit
 * address slot (uint32), matching the engine-wide convention for DAT resource
 * pointers (siblings data_fd2_ui_anim_sprite_sheet_ptr / data_fd2_all_game_text_ptr
 * in the same main load block). The sole writer main @ 0x25BF4 does
 *   data_fd2_resource_portrait_sheet_ptr =
 *       fd2_load_dat_resource(<FDOTHER.DAT name>, data_fd2_resource_portrait_sheet_ptr, 6);
 * passing the prior value (NULL on first call) so the loader frees-then-reloads.
 * Readers (fd2_render_combat_combatant_panels @ 0x1E66C, the combat/spell animation
 * routines in anicombt.c / rndscene.c / spellcin.c) treat it as the sprite-sheet
 * base address: passed directly to fd2_alloc_and_blit_indexed_sprite_chunk, and
 * indexed via the in-buffer offset table, e.g.
 *   *(uint32 *)(data_fd2_resource_portrait_sheet_ptr + 6 + idx*4)
 *       + data_fd2_resource_portrait_sheet_ptr
 * to resolve each packed sub-sprite. Zero-initialized in the image; the first use
 * is the load-time write, so this is a zero-init (BSS) pointer slot.
 */
uint32 data_fd2_resource_portrait_sheet_ptr;

/*
 * data_fd2_field_map_tile_event_consumed_flags_ptr @ 0x53AD5 -- base pointer of
 * the per-tile-event "consumed" flag block: a 32-byte heap array of one-byte
 * booleans (one slot per field-map tile event), recording which events have
 * already been triggered so they fire only once. Stored as a 32-bit address slot
 * (uint32), matching the engine-wide convention for malloc'd buffer pointers
 * (siblings data_fd2_large_game_state_buffer_ptr / data_fd2_resource_portrait_sheet_ptr).
 * The sole writer main @ 0x25BF4 does
 *   data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)malloc(0x20);
 * allocating the 32-byte block once at startup. fd2_load_save_and_init_engine
 * @ 0x10010 then memmove's 0x20 bytes from the save buffer (pBuf + 0x30A3) into
 * it to restore per-event state on load. Readers index it as a byte array,
 *   MOV EAX,[0x53AD5]; MOVZX EAX,byte ptr [tile_id + EAX]; TEST EAX,EAX
 * (e.g. fd2_handle_tile_event_interaction @ 0x190F1, fd2_tick_tile_event_animations
 * @ 0x122A8), and writers set a flag with
 *   MOV byte ptr [tile_id + EAX],0x1
 * (fd2_handle_tile_event_interaction @ 0x19246/0x194EE); the ~60 chapter event
 * handlers test/set individual slots the same way. Element stride is 1 byte; the
 * index is the tile's 5-bit terrain_class (0..31), which is why the block is
 * exactly 32 bytes -- one consumed-flag slot per terrain_class. Zero-initialized
 * in the image; the first use is the startup malloc write, so this is a zero-init
 * (BSS) pointer slot.
 */
uint32 data_fd2_field_map_tile_event_consumed_flags_ptr;

/*
 * data_fd2_ui_game_speed_flag @ 0x53AF9 -- one-byte game-speed toggle set from the
 * options menu (Slow = 0 / Fast = 1). Single-byte boolean accessed only at 8-bit
 * width: the options menu fd2_game_options_menu_loop @ 0x172F0 renders the label as
 *   menu_slot_2_speed = 0x16 + (data_fd2_ui_game_speed_flag != 0)   (Slow 0x16 / Fast 0x17)
 * and toggles it with data_fd2_ui_game_speed_flag ^= 1; the battle AI gates the fast
 * (animation-skipping) cast path on it, e.g. fd2_execute_ai_offensive_spell @ 0x153E7
 *   if (spell_id < 10 && data_fd2_ui_game_speed_flag == '\0') play full cast sequence
 * and likewise fd2_execute_ai_physical_attack @ 0x154F5. The flag is persisted in the
 * save slot: fd2_load_save_and_init_engine @ 0x10010 restores it with a byte copy
 *   MOV AL,byte ptr [ESI + 0xE]; MOV [0x53AF9],AL
 * fd2_load_state_from_selected_slot @ 0x30337 does data_fd2_ui_game_speed_flag =
 * *(uint8 *)(slot_base + 0xA07), and fd2_save_current_state_to_slot @ 0x30101 /
 * fd2_main_menu_continue_dispatcher @ 0x26088 read/write it back. Zero in the image;
 * the first runtime touch is the engine-init byte write that loads it from the saved
 * default state, so this is a zero-init (BSS) byte flag.
 */
uint8 data_fd2_ui_game_speed_flag;

/*
 * data_fd2_resource_portrait_cache_alloc_offset @ 0x53BE3 -- per-chapter count of
 * field-map portrait/character records, taken from the third byte of the loaded
 * FDFIELD.DAT tile-event table (tile_event_data_table[2]). Stored as a 32-bit
 * scalar (uint32), matching its two siblings in the same load block
 * data_fd2_resource_portrait_cache_count @ 0x53BDF and
 * data_fd2_resource_portrait_cache_total_size @ 0x53BE7. Both writers widen a
 * zero-extended byte into the full dword slot:
 *   fd2_load_chapter_battle_data @ 0x10991  MOVZX EAX,byte ptr [EAX+2]; MOV [0x53BE3],EAX
 *   fd2_load_save_and_init_engine @ 0x10291 (same MOVZX byte -> MOV dword) i.e.
 *   data_fd2_resource_portrait_cache_alloc_offset = (uint32)tile_event_data_table[2];
 * so the value is an unsigned record count. It is consumed at 32-bit width as both
 * an index base and a loop bound: fd2_load_chapter_battle_data @ 0x10A5B does
 *   IMUL EAX,dword ptr [0x53BE3],0x6      (record index * 6-byte position stride)
 * to seed the field-position pointer (chapter_portrait_load_buffer + offset*6 + 2),
 * and fd2_load_chapter_portraits_and_dump_tmp @ 0x10BCC uses it as the record-scan
 * loop count (entries of stride 0x1A, race byte at +0x98). Every access is a full
 * dword load/store of a small positive count. Zero-initialized in the image; the
 * first use on every path is the load-time write, so this is a zero-init (BSS)
 * scalar.
 */
uint32 data_fd2_resource_portrait_cache_alloc_offset;

/*
 * data_fd2_resource_portrait_cache_total_size @ 0x53BE7 -- per-chapter active
 * party/character count for the upcoming battle, taken from the second byte of the
 * loaded FDFIELD.DAT tile-event table (tile_event_data_table[1]). Sibling of
 * data_fd2_resource_portrait_cache_alloc_offset @ 0x53BE3 and
 * data_fd2_resource_portrait_cache_count @ 0x53BDF in the same load block; both
 * writers widen a zero-extended byte into the full dword slot:
 *   fd2_load_chapter_battle_data    @ 0x10987 MOVZX EDX,byte ptr [EAX+1]
 *                                   @ 0x1098B MOV dword ptr [0x53BE7],EDX
 *   fd2_load_save_and_init_engine   @ 0x10287 (same MOVZX byte -> MOV dword) i.e.
 *   data_fd2_resource_portrait_cache_total_size = (uint32)tile_event_data_table[1];
 * The value is immediately copied into data_fd2_battle_party_member_count and is
 * consumed at 32-bit width as a signed loop bound: fd2_load_chapter_battle_data
 * @ 0x10AD9 does CMP EBP,dword ptr [0x53BE7]; JGE (the per-slot runtime_char fill
 * loop). Every access is a full dword load/store of a small positive count.
 * Zero-initialized in the image; the first use on every path is the load-time
 * write, so this is a zero-init (BSS) scalar.
 */
uint32 data_fd2_resource_portrait_cache_total_size;

/*
 * data_fd2_battle_party_member_count @ 0x53BEB -- number of runtime_char slots in
 * the active battle party. Established at chapter/save load time and then consumed
 * throughout the battle as both a loop bound and the runtime_char_array element
 * count. Every access is a full 32-bit dword load/store of a small positive count;
 * it is widened from a byte at write time and used signed as a loop bound, so the
 * declared width is uint32 (not byte). Writers, in order of init:
 *   fd2_load_chapter_battle_data  @ 0x1099A  data_fd2_battle_party_member_count =
 *                                            data_fd2_resource_portrait_cache_total_size;
 *                                            (the FDFIELD.DAT tile_event_data_table[1] count)
 *   fd2_load_save_and_init_engine @ 0x1029A  = (uint32)pBuf[0x30C4]; i.e. the saved
 *                                            party_member_count byte from FD2.SAV +0x30C4
 *   fd2_init_runtime_char_for_battle @ 0x1100B  INC dword ptr [0x53BEB] -- post-add
 *                                            increment when a member is appended.
 * Representative reads (all dword): fd2_init_runtime_char_for_battle @ 0x10C69 uses
 *   MOV EBX,dword ptr [0x53BEB]; (member_count*0x50) to index runtime_char_array
 *   (data_fd2_battle_runtime_char_array_ptr @ 0x53A45, 0x50-byte stride), and
 *   @ 0x10FFD PUSH dword ptr [0x53BEB] passes it as a count argument. Zero in the
 *   image; the first use on every path is the load-time write, so this is a
 *   zero-init (BSS) scalar.
 */
uint32 data_fd2_battle_party_member_count;

/*
 * data_fd2_battle_turn_counter @ 0x53BEF -- current battle turn number (starts at
 * 1, +1 each completed player->NPC->enemy->player cycle). A full 32-bit dword slot:
 * every store is a MOV dword and reads use the whole 32-bit value (rendered as a
 * decimal "TURN N", compared as both unsigned and (int32) by chapter turn-gated
 * event handlers). Writers:
 *   fd2_init_battle_state_for_chapter @ 0x2066E  MOV dword ptr [0x53BEF],1
 *                                                (data_fd2_battle_turn_counter = 1)
 *   fd2_run_full_turn_cycle           @ 0x1A5B9  data_fd2_battle_turn_counter += 1
 *                                                (new-player-turn phase)
 *   fd2_load_save_and_init_engine     @ 0x103DF  restored from FD2.SAV (pBuf[0x30C3])
 * Representative read: fd2_run_full_turn_cycle @ 0x1A668 passes it to
 *   fd2_render_decimal_number_to_buffer(..., data_fd2_battle_turn_counter, 0x2A, 3).
 * Zero in the image; the first use on every path is the chapter/load-time write, so
 * this is a zero-init (BSS) scalar. Storage width is uint32; signed vs unsigned is
 * a per-callsite cast (matches globals.h extern).
 */
uint32 data_fd2_battle_turn_counter;

/*
 * data_fd2_shared_party_total_gold @ 0x53BF3 -- the party's gold/money total. A
 * full 32-bit dword slot: every store is a MOV/ADD/SUB dword and reads use the whole
 * 32-bit value. The type is SIGNED int32, proven by caller usage (storage authority,
 * not the per-callsite-cast pattern of the turn counter): the shop affordability test
 * in fd2_run_buy_item_menu does
 *   @ 0x2F2C6  MOV EAX,[0x53BF3]; CMP EAX,[price]; JGE ...
 * where JGE is the signed conditional (an unsigned compare would emit JAE); and both
 * money animators render it with a signed sprintf("%0.8d", gold). No unsigned compare
 * of this global exists at any reader.
 * Writers:
 *   fd2_chapter_01_init               @ 0x32969  MOV dword ptr [0x53BF3],0 (start gold)
 *   fd2_load_save_and_init_engine     @ 0x10426  restored from FD2.SAV
 *   fd2_main_menu_continue_dispatcher @ 0x26078  restored on continue
 *   fd2_load_state_from_selected_slot @ 0x30327  restored from save slot
 *   fd2_animate_money_increment       @ 0x2D43A  gold += delta (ADD dword)
 *   fd2_animate_money_decrement       @ 0x2D551  gold -= delta (SUB dword)
 *   fd2_handle_tile_event_interaction @ 0x194E8  field-event gold gain (READ_WRITE)
 *   fd2_process_battle_drop_entries   @ 0x1ABFD  battle-drop gold gain (READ_WRITE)
 * Zero in the image; the first use on every path is a chapter/load-time write, so this
 * is a zero-init (BSS) scalar.
 */
int32 data_fd2_shared_party_total_gold;

/*
 * data_fd2_shared_menu_party_roster_buffer_ptr @ 0x53BF7 -- base pointer of the
 * out-of-battle "menu / template" party roster: a heap buffer of 0xA00 bytes =
 * 32 entries of 0x50-byte runtime_char (the permanent recruited party, distinct
 * from data_fd2_battle_runtime_char_array_ptr @ 0x53A45 which is the per-chapter
 * active-battle copy). Stored as a 32-bit address slot (uint32), matching the
 * engine-wide convention for malloc'd buffer pointers (siblings
 * data_fd2_large_game_state_buffer_ptr / data_fd2_field_map_tile_event_consumed_flags_ptr).
 * The sole writer main @ 0x25BF4 does
 *   data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)malloc(0xA00);
 * allocating the buffer once at startup. Readers (~28 sites across the
 * menu/save/load/recruit/promotion code) all treat it as the base of an array of
 * 0x50-byte runtime_char records walked with member_count
 * (data_fd2_shared_menu_party_member_count @ 0x53BFB):
 *   fd2_find_char_by_id_or_template @ 0x12CBB
 *     pChar = data_fd2_shared_menu_party_roster_buffer_ptr;
 *     for (i = 0; i < member_count; i++) { if (*(byte *)(pChar+8)==id) ...; pChar += 0x50; }
 *   fd2_init_runtime_char_from_base_growth @ 0x112C6
 *     slot = data_fd2_shared_menu_party_roster_buffer_ptr + member_count * 0x50;
 *     (then fills the 0x50-byte runtime_char slot field-by-field)
 *   fd2_reorder_party_by_selection @ 0x32127
 *     memmove(tmp, data_fd2_shared_menu_party_roster_buffer_ptr, 0xA00); (snapshot all 32 slots)
 *     memmove(roster + out*0x50, tmp + (i+1)*0x50, 0x50); (reorder by 0x50 stride)
 * The whole buffer is also block-copied to/from the save image as one 0xA00 chunk
 * (fd2_main_menu_continue_dispatcher @ 0x26050 memmove from FD2.SAV slot,
 * fd2_save_current_state_to_slot / fd2_load_state_from_selected_slot the reverse).
 * Element stride is 0x50 (runtime_char); the index is the menu party member count.
 * Zero-initialized in the image; the first use is the startup malloc write, so this
 * is a zero-init (BSS) pointer slot.
 */
uint32 data_fd2_shared_menu_party_roster_buffer_ptr;

/*
 * data_fd2_shared_menu_party_member_count @ 0x53BFB -- number of members in the
 * out-of-battle "menu / template" party roster (the live count of recruited
 * characters held in data_fd2_shared_menu_party_roster_buffer_ptr @ 0x53BF7).
 * A full 32-bit dword slot: every store is a MOV/INC dword and every read uses
 * the whole 32-bit value, so the type is uint32 (the value range is small, a
 * party member count, but storage and access width are 32-bit). It serves dual
 * duty as both the roster element count and the append index.
 * Writers:
 *   fd2_main_menu_continue_dispatcher @ 0x25EFA  MOV dword ptr [0x53BFB],0 (new-game reset)
 *   fd2_main_menu_continue_dispatcher @ 0x26070  restored from FD2.SAV slot byte
 *   fd2_load_save_and_init_engine     @ 0x1041E  restored from FD2.SAV (= pBuf[0x30CC])
 *   fd2_load_state_from_selected_slot @ 0x3031F  restored from save slot
 *   fd2_init_runtime_char_from_base_growth @ 0x1144C  INC dword (append a new member, READ_WRITE)
 * Index/count reads (representative):
 *   fd2_init_runtime_char_from_base_growth @ 0x112B6
 *     slot = roster_buffer_ptr + member_count * 0x50; (member_count used as append index)
 *   fd2_find_char_by_id_or_template @ 0x12CC9
 *     for (i = 0; i < member_count; i++) { ...; pChar += 0x50; } (member_count used as loop bound)
 * Zero in the image; the first use on every path is a write (the new-game reset
 * to 0, or a load-time restore from the save buffer), so this is a zero-init
 * (BSS) scalar.
 */
uint32 data_fd2_shared_menu_party_member_count;

/*
 * data_fd2_graphics_chapter_ambient_palette_anim_tick_latch @ 0x53C0F -- the
 * BIOS-tick snapshot used to pace the slow chapter ambient-palette animation
 * cycle (data_fd2_graphics_chapter_ambient_palette_anim_idx @ 0x53C0B). Holds
 * the value of the BDA tick counter at word [0x46C], sign-extended to 32 bits;
 * every access is a full 32-bit dword (MOV/SUB), so the type is a 4-byte slot.
 * The latch is read via signed elapsed-tick deltas (SUB EAX,[0x53C0F] then a
 * signed CMP/JG/JGE against 4), so it behaves as a signed timer snapshot, but
 * storage and access width are 32-bit (kept uint32 to match the slot width).
 * Writers:
 *   main @ 0x25D8B  MOV [0x53C0F],EAX  (startup: latch = (int32)*(int16*)0x46C)
 *   fd2_tick_chapter_palette_animation @ 0x129CD  re-latch to current tick once
 *     >4 ticks have elapsed (advances the ambient-palette index and resets latch)
 * Readers:
 *   fd2_tick_chapter_palette_animation @ 0x1298F, 0x129A2
 *     delta = (int32)*(int16*)0x46C - latch; if (delta > 4 || delta < 0) advance.
 * Zero in the image; the first use on every path is the startup write in main
 * (latched before the animation tick ever reads it), so this is a zero-init (BSS)
 * scalar.
 */
uint32 data_fd2_graphics_chapter_ambient_palette_anim_tick_latch;

/*
 * data_fd2_audio_bgm_sequence_handle @ 0x53ED0 -- the AIL (Miles Sound
 * System) sequence handle used for all BGM playback. Allocated once at
 * startup and then handed to every AIL sequence call as an opaque 4-byte
 * handle. Every access is a full 32-bit dword (the writer stores EAX, every
 * reader does PUSH dword ptr [0x53ED0]), so the slot is a 4-byte value;
 * kept uint32 to match the handle width (the byte_data size hint was wrong --
 * caller width is dword, not byte).
 * Writer:
 *   main @ 0x25C26  MOV [0x53ED0],EAX
 *     handle = AIL_allocate_sequence_handle(data_fd2_audio_bgm_driver_handle),
 *     done only when the MDI driver installed successfully.
 * Readers (all in fd2_set_bgm_track_with_fade @ 0x259AA..0x25A86, passed as
 * the sequence argument):
 *   AIL_set_sequence_volume / AIL_stop_sequence / AIL_init_sequence /
 *   AIL_start_sequence / AIL_set_sequence_loop_count.
 * Also read once in fd2_game_options_menu_loop @ 0x17380 for live volume.
 * Zero in the image; the first write on every path is the startup allocation
 * in main (before any BGM playback reads it), so this is a zero-init (BSS)
 * scalar.
 */
void *data_fd2_audio_bgm_sequence_handle;

/*
 * data_fd2_audio_bgm_driver_handle @ 0x53ED8 -- the AIL (Miles Sound System)
 * MDI driver handle for BGM. Installed once at startup and then used only as
 * the opaque mdi_driver argument to AIL_allocate_sequence_handle. Every access
 * is a full 32-bit pointer; declared void * to match the install return value
 * and the AIL_allocate_sequence_handle(void *mdi_driver) parameter type.
 * Writer:
 *   main @ 0x25C0D  MOV [0x53ED8],EAX
 *     data_fd2_audio_bgm_driver_handle = (void *)AIL_install_MDI_INI().
 * Reader (same function, gated on a non-NULL install):
 *   main @ 0x25C1D  passes the handle to
 *     data_fd2_audio_bgm_sequence_handle = AIL_allocate_sequence_handle(handle);
 *     it also sets data_fd2_audio_bgm_driver_available_flag = 1.
 * Zero in the image; the first access is the startup install write, so this is
 * a zero-init (BSS) scalar pointer.
 */
void *data_fd2_audio_bgm_driver_handle;

/*
 * data_fd2_audio_sfx_dig_driver_handle @ 0x53EDC -- the AIL (Miles Sound
 * System) DIG driver handle for digital SFX. Installed once at startup and
 * then used only as the opaque dig_driver argument to
 * AIL_allocate_sample_handle (twice, for the two SFX channels). Every access
 * is a full 32-bit dword; kept uint32 to match the handle width and the
 * already-emitted main body (the byte_data size hint was wrong -- caller
 * width is dword, not byte). The handle is cast to void * at each
 * AIL_allocate_sample_handle(void *dig_driver) call site.
 * Writer:
 *   main @ 0x25C32  MOV [0x53EDC],EAX
 *     data_fd2_audio_sfx_dig_driver_handle = (uint32)AIL_install_DIG_INI().
 * Readers (same function, gated on a non-NULL install):
 *   main @ 0x25C50  passes the handle to AIL_allocate_sample_handle for
 *     data_fd2_audio_sfx_sample_handle_1 (channel 1); the channel-0 allocation
 *     at 0x25C42 reuses the value still live in EAX from the install. It also
 *     sets data_fd2_audio_sfx_driver_available_flag = 1.
 * Zero in the image; the first access is the startup install write, so this is
 * a zero-init (BSS) scalar handle.
 */
void *data_fd2_audio_sfx_dig_driver_handle;

/*
 * data_fd2_audio_sfx_sample_handle_0 @ 0x53EE4 -- the AIL (Miles Sound
 * System) sample handle for SFX channel 0. Allocated once at startup from
 * the DIG driver and then used only as the opaque sample handle argument to
 * the AIL one-shot sample calls. Every access is a full 32-bit dword; kept
 * uint32 to match the handle width (the byte_data size hint was wrong --
 * caller width is dword, not byte).
 * Writer:
 *   main @ 0x25C4B  MOV [0x53EE4],EAX
 *     data_fd2_audio_sfx_sample_handle_0 = AIL_allocate_sample_handle(
 *         data_fd2_audio_sfx_dig_driver_handle); gated on a non-NULL DIG
 *     install (the value is the return still live in EAX from the install).
 * Readers:
 *   fd2_play_sfx_with_handle @ 0x25AC7/0x25AFD/0x25B12/0x25B24/0x25B32
 *     passes the handle to AIL_stop_sample, AIL_init_sample,
 *     AIL_set_sample_address, AIL_set_sample_loop_count, AIL_start_sample.
 * Zero in the image; the first access is the startup allocation write, so
 * this is a zero-init (BSS) scalar handle.
 */
void *data_fd2_audio_sfx_sample_handle_0;

/*
 * data_fd2_audio_sfx_sample_handle_1 @ 0x53EE8 -- the AIL (Miles Sound
 * System) sample handle for SFX channel 1. Allocated once at startup from
 * the DIG driver and then used only as the opaque sample handle argument to
 * the AIL one-shot sample calls. Every access is a full 32-bit dword; kept
 * uint32 to match the handle width and the sibling channel-0 handle (the
 * byte_data size hint was wrong -- caller width is dword, not byte).
 * Writer:
 *   main @ 0x25C5E  MOV [0x53EE8],EAX
 *     data_fd2_audio_sfx_sample_handle_1 = AIL_allocate_sample_handle(
 *         data_fd2_audio_sfx_dig_driver_handle); gated on a non-NULL DIG
 *     install.
 * Readers:
 *   fd2_play_sfx_sample_from_bank @ 0x25B76/0x25BAC/0x25BC1/0x25BD3/0x25BE1
 *     passes the handle by value to AIL_stop_sample, AIL_init_sample,
 *     AIL_set_sample_address (cast to int), AIL_set_sample_loop_count,
 *     AIL_start_sample.
 * Zero in the image; the first access is the startup allocation write, so
 * this is a zero-init (BSS) scalar handle.
 */
void *data_fd2_audio_sfx_sample_handle_1;

/*
 * data_fd2_audio_fdother_sfx_bank_buf_ptr @ 0x53EEC -- base pointer of the
 * loaded SFX sample bank (FDOTHER.DAT resource index 0x1F). Holds the malloc'd
 * resource buffer returned by fd2_load_dat_resource; stored as a 32-bit address
 * slot (uint32), matching the engine-wide convention for DAT resource pointers
 * (siblings data_fd2_resource_portrait_sheet_ptr / data_fd2_ui_anim_sprite_sheet_ptr).
 * The sole writer main @ 0x25BF4 does
 *   data_fd2_audio_fdother_sfx_bank_buf_ptr =
 *       fd2_load_dat_resource(<FDOTHER.DAT name>,
 *           data_fd2_audio_fdother_sfx_bank_buf_ptr, 0x1F);
 * passing the prior value (NULL on first call) so the loader frees-then-reloads.
 * Every reader (~50 sites across battle / field / menu / dialog / save / anim)
 * passes it as the bank_base_ptr (first) argument of the two AIL one-shot
 * dispatchers, which treat it as the base of the in-bank sample directory:
 *   fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, sfx_id, loop)
 *   fd2_play_sfx_sample_from_bank @ 0x25B76 (param_1 = bank base):
 *     entry         = param_1 + sample_index*4;   (4-byte directory stride)
 *     sample_offset = *(uint *)(entry + 6);
 *     sample_end    = *(uint *)(entry + 10);
 *     AIL_set_sample_address(handle, param_1 + sample_offset,
 *                            sample_end - sample_offset);
 * i.e. the sample PCM lives at bank_base + per-entry offset. Every access is a
 * full 32-bit dword load/store of the buffer address. Zero-initialized in the
 * image; the first use is the startup load-time write, so this is a zero-init
 * (BSS) pointer slot.
 */
uint32 data_fd2_audio_fdother_sfx_bank_buf_ptr;

/*
 * data_fd2_audio_bgm_driver_available_flag @ 0x53EF0 -- boolean "MDI (BGM)
 * driver installed" flag. Set to 1 once at startup only when the Miles MDI
 * driver installs successfully, and then tested as a gate before any BGM
 * track load/playback. Every access is byte-width, so the slot is a single
 * uint8 used as a boolean (the byte_data size hint is correct here).
 * Writer:
 *   main @ 0x25C16  MOV byte ptr [0x53EF0],0x1
 *     done only inside the AIL_install_MDI_INI() != NULL branch (right after
 *     storing data_fd2_audio_bgm_driver_handle and allocating
 *     data_fd2_audio_bgm_sequence_handle).
 * Reader:
 *   fd2_set_bgm_track_with_fade @ 0x259BA  CMP byte ptr [0x53EF0],0
 *     if non-zero (driver present) the function proceeds to stop/load/init/
 *     start the requested FDMUS.DAT sequence; if zero it skips all BGM work.
 * Zero in the image; the value 1 is written only at runtime when the driver
 * installs (the image never carries a non-zero initializer), so this is a
 * zero-init (BSS) byte flag.
 */
uint8 data_fd2_audio_bgm_driver_available_flag;

/*
 * data_fd2_audio_sfx_driver_available_flag @ 0x53EF1 -- boolean "DIG (SFX)
 * driver installed" flag. Set to 1 once at startup only when the Miles DIG
 * driver installs successfully, and then tested as a gate before any one-shot
 * SFX sample playback. Every access is byte-width, so the slot is a single
 * uint8 used as a boolean (the byte_data size hint is correct here).
 * Writer:
 *   main @ 0x25C3B  MOV byte ptr [0x53EF1],0x1
 *     done only inside the AIL_install_DIG_INI() != 0 branch (right after
 *     storing data_fd2_audio_sfx_dig_driver_handle and allocating
 *     data_fd2_audio_sfx_sample_handle_0 / data_fd2_audio_sfx_sample_handle_1).
 * Readers (byte gate, CMP byte ptr [0x53EF1],0):
 *   fd2_play_sfx_with_handle      @ 0x25AA4
 *   fd2_play_sfx_sample_from_bank @ 0x25B53
 *     each proceeds to AIL stop/init/set-address/start the requested sample
 *     only when this flag (and the SFX-enabled flag) is non-zero; otherwise it
 *     is a silent no-op.
 * Zero in the image; the value 1 is written only at runtime when the driver
 * installs (the image never carries a non-zero initializer), so this is a
 * zero-init (BSS) byte flag.
 */
uint8 data_fd2_audio_sfx_driver_available_flag;

/*
 * data_fd2_ui_terrain_hud_user_enabled @ 0x51AAB -- user toggle for the
 * battle terrain-info HUD panel. uint8 boolean; ships enabled (image value 1).
 * Gated by the options menu and restored from FD2.SAV (+0xA06 / +0x30D2).
 */
uint8 data_fd2_ui_terrain_hud_user_enabled = 1;

/*
 * data_fd2_ui_play_active_flag @ 0x51AAC -- "gameplay active" gate. uint8
 * boolean; ships set (image value 1). Sole reader is the terrain-info HUD
 * panel render in fd2_render_terrain_info_hud_panel (gfx/rndstat.c): the
 * panel is suppressed while this flag is 0. Cleared (0) then re-set (1) around
 * any transition where that HUD must not draw -- the player-turn -> enemy-turn
 * cycle (fd2_check_all_player_acted_or_asleep / fd2_field_command_menu_loop),
 * chapter init/end + clear fanfare here in main, and the chapter
 * transition / save-load paths in fd2_main_menu_continue_dispatcher.
 */
uint8 data_fd2_ui_play_active_flag = 1;

/*
 * data_fd2_audio_bgm_enabled_flag @ 0x51E61 -- user "music on" option. uint8
 * boolean; ships enabled (image value 1). Restored from FD2.SAV (+0xA08 /
 * +0x30D3); gates BGM track changes.
 */
uint8 data_fd2_audio_bgm_enabled_flag = 1;

/*
 * data_fd2_audio_sfx_enabled_flag @ 0x51E62 -- user "sound effects on" option.
 * uint8 boolean; ships enabled (image value 1). Restored from FD2.SAV (+0xA09 /
 * +0x30D4); gates one-shot SFX playback.
 */
uint8 data_fd2_audio_sfx_enabled_flag = 1;

/*
 * data_fd2_runtime_battle_state_ptr @ 0x53A4D -- base pointer of the runtime
 * battle/cursor state block (FDOTHER.DAT resource index 1). uint32 address slot.
 * Written by main's load block; zero-init (BSS) pointer slot.
 */
uint32 data_fd2_runtime_battle_state_ptr;

/*
 * data_fd2_tile_event_data_table_ptr @ 0x53A55 -- base pointer of the per-chapter
 * tile-event data table (0x8A3-byte malloc'd block restored from FD2.SAV).
 * uint32 address slot; zero-init (BSS) pointer slot.
 */
uint32 data_fd2_tile_event_data_table_ptr;

/*
 * data_fd2_vga_palette_data_ptr @ 0x53A65 -- base pointer of the loaded VGA
 * palette resource (FDOTHER.DAT resource index 0). uint32 address slot; reloaded
 * (free-then-load) on each new-game / continue / load path. Zero-init (BSS).
 *
 * Points at a 768-byte buffer = 256 palette entries x 3 RGB bytes (6-bit DAC
 * values 0..0x3F). Palette primitives index it as ptr[idx*3 + component] to
 * source the base colour for fade/flash/tint DAC writes. Cinematic / ending
 * code reassigns it to other FDOTHER.DAT palettes for each scene (some paths
 * save and restore the previous pointer).
 */
uint32 data_fd2_vga_palette_data_ptr;

/*
 * data_fd2_tile_attribute_flags_buffer_ptr @ 0x53A69 -- base pointer of the
 * battle tile-attribute buffer (FDSHAP.DAT resource, scene_id*2+1). uint32
 * address slot; zero-init (BSS), reloaded (free-then-load) on each chapter /
 * new-game / continue / load path.
 *
 * Points at an array of 4-byte attribute records, one per tile-sheet sprite,
 * indexed as ptr[tile_id*4] where tile_id is the 10-bit (0..0x3FF) sprite index
 * from the battle tile-map meta word. Byte 0 holds the animation/flag bits read
 * by the tile compositor and tile-query accessors (0x04 / 0x08 swap-every-frame
 * tile-id advance, 0x10 chapter-palette half-step; 0x60 event class, 0x80
 * renderable also live in the record). Readers: fd2_read_tile_attribute_at_pos,
 * fd2_composite_battle_tile_map, plus AI / menu / cursor tile-property queries.
 */
uint32 data_fd2_tile_attribute_flags_buffer_ptr;

/*
 * data_fd2_tile_anim_table_base @ 0x53A6D -- base pointer of the tile animation
 * table (FDOTHER.DAT resource index 3). uint32 address slot; zero-init (BSS).
 */
uint32 data_fd2_tile_anim_table_base;

/*
 * data_fd2_chinese_font_sheet @ 0x53A75 -- base pointer of the Chinese glyph
 * sprite sheet (FDOTHER.DAT resource index 4). uint32 address slot; zero-init
 * (BSS), written once at startup in main(). Game-mutable (not const).
 *
 * Passed as the font_data argument to fd2_blit_glyph_2bpp_with_outline, which
 * indexes into the sheet by glyph id to render each character (the dialog VM
 * fd2_display_dialog_scene uses it for both the literal-number path and the
 * general text path).
 */
uint32 data_fd2_chinese_font_sheet;

/*
 * data_fd2_current_chapter_text @ 0x53A79 -- base pointer of the current
 * chapter's FDTXT.DAT text bank (resource = chapter_id+1). uint32 address slot;
 * zero-init (BSS) pointer slot.
 */
uint32 data_fd2_current_chapter_text;

/*
 * data_fd2_all_game_text_ptr @ 0x53A7D -- base pointer of the global FDTXT.DAT
 * text bank (resource index 0): the engine-wide, non-chapter-specific text used
 * by system/menu dialog (shop buy/sell/give, save/load, revive, class promotion,
 * recruitment, chapter-intro, level-up and status messages, ending), as distinct
 * from data_fd2_current_chapter_text @ 0x53A79 which holds the per-chapter bank.
 * uint32 address slot; zero-init (BSS) pointer slot. Loaded once at startup by the
 * sole writer main @ 0x25BF4 (passing the prior value so the loader frees-then-
 * reloads); ~90 readers pass it as the text_base argument of
 * fd2_display_dialog_scene, which reads it as a page-index header table followed
 * by an int16 dialog-opcode stream.
 */
uint32 data_fd2_all_game_text_ptr;

/*
 * data_fd2_ui_anim_sprite_sheet_ptr @ 0x53A81 -- base pointer of the UI/menu
 * animation sprite sheet (FDOTHER.DAT resource index 5). Holds the malloc'd
 * resource buffer returned by fd2_load_dat_resource; uint32 address slot,
 * zero-init (BSS), matching the engine-wide convention for DAT resource
 * pointers (siblings data_fd2_resource_portrait_sheet_ptr /
 * data_fd2_all_game_text_ptr in the same main load block). Sole writer main @
 * 0x25BF4 does
 *   data_fd2_ui_anim_sprite_sheet_ptr =
 *       fd2_load_dat_resource(<FDOTHER.DAT name>,
 *                             data_fd2_ui_anim_sprite_sheet_ptr, 5);
 * (passing the prior value so the loader frees-then-reloads). ~70 readers pass
 * it as the sprite-sheet argument of the blit helpers
 * (fd2_blit_sheet_sprite_at_offset / fd2_alloc_and_blit_indexed_sprite_chunk):
 * the dialog/window frame is composed from 17 tiles in this sheet (corners,
 * stretchable edges, center fill), and the rest of the menu UI, status/inventory
 * panels, HP/phase banners, projectile animations, shop and recruitment screens
 * index further sprites out of it.
 */
uint32 data_fd2_ui_anim_sprite_sheet_ptr;

/*
 * data_fd2_menu_dialog_state_handle @ 0x53A89 -- base pointer of the menu/dialog
 * state resource (FDOTHER.DAT resource index 2). uint32 address slot; zero-init
 * (BSS) pointer slot.
 */
uint32 data_fd2_menu_dialog_state_handle;

/*
 * data_fd2_input_int16_regs @ 0x53A8D -- the shared 28-byte union REGS scratch
 * every int386() INT 10h/16h call reuses (video-mode set, keyboard read). Its
 * two byte aliases data_fd2_input_last_key_pressed (byte 0 = AL / ascii) and
 * data_fd2_input_key_input_mode (byte 1 = AH / scancode) are macros over this
 * union in globals.h, which preserves the vendor union-REGS overlap the original
 * relies on (see that comment). zero-init (BSS).
 */
union REGS data_fd2_input_int16_regs;

/*
 * data_fd2_battle_map_width_tiles @ 0x53AC1 -- width of the current battle map in
 * tiles. Companion of data_fd2_battle_map_height_tiles @ 0x53AC5. Loaded at
 * chapter\save-load time from the FDFIELD.DAT tile-map header: both writers
 * (fd2_load_chapter_battle_data @ 0x10932 and
 * fd2_load_save_and_init_engine @ 0x1022e) do
 *   data_fd2_battle_map_width_tiles = (int)*(short *)_battle_tile_map;
 * i.e. read the first 16-bit field of the decrypted tile map (MOVSX, signed) and
 * widen it into this 32-bit slot. It is then consumed by many readers as the
 * row-major column stride for tile-grid addressing -- e.g. the render loop indexes
 * `((row + win_y) * data_fd2_battle_map_width_tiles + win_x) * 4` -- and as the
 * right-edge limit for cursor clamping (`data_fd2_battle_map_width_tiles - 1`).
 * uint32; zero-init (BSS) scalar.
 */
uint32 data_fd2_battle_map_width_tiles;
