/*
 * chtrans.c — per-chapter cutscene walk-animation script interpreter
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>

/* ----------------------------------------------------------------
 * fd2_cutscene_event_trigger @ 0x1366A  (51 callers)
 *
 * Interpret a per-chapter cutscene byte-script (selected by event_id)
 * to drive runtime-char walk-animation sequences.
 *
 * Script byte-stream:
 *   [n_groups: u8]
 *   per group:
 *     [walk_count_or_flags: u8]  bit7 = special mode; low7 = walk-step count
 *     [step_count: u8]           number of chars moving simultaneously (<= 32)
 *     [pairs * step_count]:      byte char_idx, byte dir
 *       dir: 0 = south (+y), 1 = west (-x), 2 = north (-y), 3 = east (+x)
 *
 * Three modes by bit7 of walk_count_or_flags:
 *   bit7 == 0           : normal 6-frame walk, low7 repeats, commits position.
 *   bit7 == 1, low7 == 0: sprite-refresh blit pass.
 *   bit7 == 1, low7 != 0: facing-only / camera hold for low7 composites.
 * ---------------------------------------------------------------- */
void fd2_cutscene_event_trigger(uint32 event_id)
{
    uint8 *script_ptr;
    uint8 *pPair;
    uint8 n_groups;
    uint8 walk_count_or_flags;
    uint8 step_count;
    uint8 group_iter;
    uint8 walk_repeat_iter;
    uint8 frame_iter;
    uint8 step_iter;
    uint8 dir;
    uint8 step_chars[32];
    uint8 step_dirs[32];
    runtime_char *pChar;
    int member_iter;
    int saved_buf;

    script_ptr = fd2_get_cutscene_event_script((int)event_id);
    n_groups = script_ptr[0];
    pPair = script_ptr + 1;

    for (group_iter = 0; group_iter < n_groups; group_iter++) {
        walk_count_or_flags = pPair[0];
        step_count = pPair[1];
        pPair += 2;

        for (step_iter = 0; step_iter < step_count; step_iter++) {
            step_chars[step_iter] = pPair[0];
            step_dirs[step_iter] = pPair[1];
            pPair += 2;
        }

        if ((walk_count_or_flags & 0x80) == 0) {
            for (walk_repeat_iter = 0; walk_repeat_iter < walk_count_or_flags;
                 walk_repeat_iter++) {
                for (frame_iter = 1; frame_iter < 7; frame_iter++) {
                    fd2_tick_walk_step_footstep_sfx((uint32)step_chars[0]);
                    for (step_iter = 0; step_iter < step_count; step_iter++) {
                        pChar = data_fd2_battle_runtime_char_array_ptr +
                                step_chars[step_iter];
                        pChar->sprite_state[1] = step_dirs[step_iter];
                        pChar->sprite_state[2] = frame_iter;
                    }
                    if (data_fd2_chapter_cutscene_event_state == 0 ||
                        data_fd2_chapter_cutscene_event_state == 0x40) {
                        fd2_composite_battle_frame(0);
                    } else {
                        data_fd2_chapter_cutscene_event_state++;
                        fd2_composite_battle_frame(1);
                        fd2_set_vga_palette_range(0, 0xFF,
                            data_fd2_chapter_cutscene_event_state);
                    }
                    fd2_wait_n_bios_ticks(1);
                    fd2_clear_keyboard_buffer();
                }
                for (step_iter = 0; step_iter < step_count; step_iter++) {
                    pChar = data_fd2_battle_runtime_char_array_ptr +
                            step_chars[step_iter];
                    dir = step_dirs[step_iter];
                    if (dir == 0) {
                        pChar->pos_y++;
                    } else if (dir == 1) {
                        pChar->pos_x--;
                    } else if (dir == 3) {
                        pChar->pos_x++;
                    } else {
                        pChar->pos_y--;
                    }
                    pChar->sprite_state[2] = 0;
                }
            }
        } else if ((walk_count_or_flags & 0x7F) == 0) {
            fd2_wait_n_bios_ticks(1);
            fd2_composite_battle_tile_map(
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, 0xD, 8,
                data_fd2_battle_view_window_origin_x,
                data_fd2_battle_view_window_origin_y);
            saved_buf = (int)data_fd2_large_game_state_buffer_ptr;
            for (member_iter = 0;
                 member_iter < (int)data_fd2_battle_party_member_count;
                 member_iter++) {
                data_fd2_large_game_state_buffer_ptr = (uint32)saved_buf;
                pChar = data_fd2_battle_runtime_char_array_ptr + member_iter;
                for (step_iter = 0; step_iter < step_count; step_iter++) {
                    if ((uint32)member_iter == step_chars[step_iter]) {
                        data_fd2_large_game_state_buffer_ptr =
                            (uint32)(saved_buf - 0x1560);
                        pChar->sprite_state[1] = step_dirs[step_iter];
                    }
                }
                if ((pChar->flags & 1) == 0) {
                    fd2_paint_char_sprite_at_world_pos((uint32)member_iter);
                }
                data_fd2_large_game_state_buffer_ptr = (uint32)saved_buf;
            }
            fd2_redraw_terrain_tiles_under_chars();
            fd2_blit_rectangle(0xA0504, 0x140,
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, 0x138, 0xC0);
            fd2_wait_n_bios_ticks(2);
            fd2_composite_battle_frame(0);
            fd2_clear_keyboard_buffer();
        } else {
            for (step_iter = 0; step_iter < step_count; step_iter++) {
                data_fd2_battle_runtime_char_array_ptr[step_chars[step_iter]]
                    .sprite_state[1] = step_dirs[step_iter];
            }
            for (step_iter = 0; step_iter < (walk_count_or_flags & 0x7F);
                 step_iter++) {
                fd2_composite_battle_frame(0);
                fd2_wait_n_bios_ticks(1);
                fd2_clear_keyboard_buffer();
            }
        }
    }

    fd2_composite_battle_frame(1);
}

/* ----------------------------------------------------------------
 * fd2_setup_chars_and_camera_for_intro @ 0x233C6  (15 callers)
 *
 * Chapter cutscene -> battle transition helper. Places a contiguous
 * range of runtime-chars (and one optional extra char) at fixed map
 * positions/facings, then resets the battle camera/cursor origin and
 * does one composite-with-fade reveal of the new arrangement.
 *
 * Fades to black first so the re-positioning is hidden, lays out the
 * chars, resets camera + anim phase, composites one frame, then fades
 * back in and holds for 200 ticks.
 *
 * Params (all 11 are cdecl stack slots, each a 4-byte push):
 *   pX_byte_array  = base of per-char X-position byte table
 *   pY_byte_array  = base of per-char Y-position byte table
 *   sprite_facing_fixed_or_array:
 *       value < 4  -> use that value as a fixed facing for every char
 *       value >= 4 -> treat as base of a per-char facing byte table
 *   char_start / char_end = inclusive runtime_char index range to place
 *   extra_char_idx = one additional runtime_char index, or 0 to skip
 *   extra_x / extra_y / extra_facing = position/facing for the extra char
 *       (low byte of each used)
 *   origin_x / origin_y = battle view-window + cursor world origin
 *
 * Callers: 15 chapter end handlers (ch 03/05/07/08/12/14/16/17/18/21/
 * 22/23/26/27/30 end).
 *
 * Body ends by tail-jumping into fd2_composite_battle_frame_zero's
 * shared epilogue (past its composite call, straight to the register
 * restore + RET); semantically a plain return after the fade-in/delay.
 * ---------------------------------------------------------------- */
void fd2_setup_chars_and_camera_for_intro(uint32 pX_byte_array,
                                          uint32 pY_byte_array,
                                          uint32 sprite_facing_fixed_or_array,
                                          int char_start, int char_end,
                                          uint32 extra_char_idx,
                                          int extra_x, int extra_y,
                                          int extra_facing,
                                          int origin_x, int origin_y)
{
    runtime_char *pChar;
    uint8 facing;
    int char_idx;

    fd2_play_palette_fade_to_black();
    fd2_clear_all_chars_acted_flag();

    for (char_idx = char_start; char_idx <= char_end; char_idx++) {
        pChar = data_fd2_battle_runtime_char_array_ptr + char_idx;
        pChar->pos_x = *(uint8 *)(char_idx + pX_byte_array);
        pChar->pos_y = *(uint8 *)(char_idx + pY_byte_array);
        if (sprite_facing_fixed_or_array < 4) {
            facing = (uint8)sprite_facing_fixed_or_array;
        } else {
            facing = *(uint8 *)(char_idx + sprite_facing_fixed_or_array);
        }
        pChar->sprite_state[1] = facing;
    }

    if (extra_char_idx != 0) {
        pChar = data_fd2_battle_runtime_char_array_ptr + extra_char_idx;
        pChar->pos_x = (uint8)extra_x;
        pChar->pos_y = (uint8)extra_y;
        pChar->sprite_state[1] = (uint8)extra_facing;
    }

    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_view_window_origin_x = (uint32)origin_x;
    data_fd2_battle_view_window_origin_y = (uint32)origin_y;
    data_fd2_battle_cursor_world_x = (uint32)origin_x;
    data_fd2_battle_cursor_world_y = (uint32)origin_y;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;

    fd2_composite_battle_frame(1);
    fd2_play_palette_fade_in();
    fd2_delay_ms(200);
}

/* ----------------------------------------------------------------
 * fd2_chapter_transition_menu @ 0x2CAD7  (2 callers)
 *
 * Between-chapter dispatch + intro/save menu. Invoked from the main loop
 * (main / fd2_main_menu_dispatcher) when a chapter transition
 * is pending. Returns:
 *   1 - story-chapter intro committed without choosing "save" (cursor != 2)
 *       => caller continues normal flow.
 *   0 - story-chapter "save" chosen (cursor == 2), or battle-chapter branch
 *       completed (recruitment/branch screen done).
 *
 * Phase 1 frees the per-chapter dynamic state (idempotent, NULL-guarded).
 * Phase 2 rebuilds the portrait cache from FDICON.B24 for every menu-party
 * member. Phase 3 dispatches on the per-chapter category:
 *   category == 0  -> STORY: render the FDOTHER intro panel + menu overlay,
 *                     run the radio-option input loop (left/right cycle the
 *                     5-option cursor, F8 cycles a debug BGM, a metadata
 *                     hotkey jumps to option 5), commit via
 *                     fd2_chapter_transition_with_intro.
 *   category != 0  -> BATTLE: show the save-prompt dialog, optionally save to
 *                     slot 0, then run the recruitment/branch screen loop.
 *
 * The story branch's "cursor != 2 -> return 1" early-out and the battle
 * branch share the single trailing "return 0" (the original's
 * XOR EAX,EAX; JMP epilogue at 0x2CCFD is reached both from the battle
 * tail and from the story cursor==2 case).
 *
 * intro_panel_idx_lut: the original copies the 3 bytes of
 * data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table
 * (@0x526D7) onto its stack frame and indexes them by the chapter-intro
 * category (= metadata[0]). The disasm's base/index rebasing is reproduced
 * here as a direct lut[category] read over the 3 copied bytes; for the
 * in-range categories (0..2) this is byte-identical to the original access.
 *
 * Globals first defined here (see globals.h):
 *   data_fd2_chapter_intro_menu_cursor_state            (0x5412B) radio 0..5
 *   data_fd2_chapter_intro_active_metadata_entry_ptr    (0x54137)
 *   data_fd2_chapter_intro_menu_overlay_buf_ptr         (0x5413B)
 *   data_fd2_chapter_per_chapter_category_table         (0x526B9) 30B
 *   data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table
 *                                                       (0x526D7) 3B
 * ---------------------------------------------------------------- */
int fd2_chapter_transition_menu(void)
{
    void *fp;
    int i;
    uint8 intro_panel_idx_lut[3];
    uint8 *metadata;
    uint8 category;
    void *intro_rle;
    uint8 commit_result;
    uint8 debug_bgm_idx;
    uint16 saved_tick;
    int dialog_result;
    int recruit_result;

    commit_result = 0;
    intro_panel_idx_lut[0] =
        data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table[0];
    intro_panel_idx_lut[1] =
        data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table[1];
    intro_panel_idx_lut[2] =
        data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table[2];
    debug_bgm_idx = 0;

    /* Phase 1: free per-chapter dynamic state (idempotent, NULL-guarded). */
    if (data_fd2_battle_runtime_char_array_ptr != (runtime_char *)0) {
        free(data_fd2_battle_runtime_char_array_ptr);
    }
    data_fd2_battle_runtime_char_array_ptr = (runtime_char *)0;
    if (data_fd2_tile_event_data_table_ptr != 0) {
        free((void *)data_fd2_tile_event_data_table_ptr);
    }
    data_fd2_tile_event_data_table_ptr = 0;
    if (data_fd2_battle_scene_snapshot != 0) {
        free((void *)data_fd2_battle_scene_snapshot);
    }
    data_fd2_battle_scene_snapshot = 0;
    if (data_fd2_battle_tile_map_ptr != 0) {
        free((void *)data_fd2_battle_tile_map_ptr);
    }
    data_fd2_battle_tile_map_ptr = 0;
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
    }

    /* Phase 2: rebuild portrait cache from FDICON.B24 for menu-party members. */
    fp = fopen("FDICON.B24", "rb");
    data_fd2_resource_portrait_cache_count = 0;
    for (i = 0; i < (int)data_fd2_shared_menu_party_member_count; i++) {
        fd2_load_portrait_to_cache(
            (uint32)((runtime_char *)data_fd2_shared_menu_party_roster_buffer_ptr)[i]
                .portrait_id,
            (uint32)fp);
    }
    fclose(fp);

    /* Phase 3: dispatch on per-chapter category. */
    if (data_fd2_chapter_per_chapter_category_table[
            data_fd2_chapter_current_chapter_id] == 0) {
        /* ---- STORY CHAPTER: intro panel + radio menu ---- */
        data_fd2_battle_scene_snapshot = (uint32)malloc(0x25680);
        metadata = fd2_get_chapter_intro_metadata_entry(
            (int)data_fd2_chapter_current_chapter_id);
        category = metadata[0];
        data_fd2_chapter_intro_active_metadata_entry_ptr = (uint32)metadata;
        fd2_play_palette_fade_to_black();
        fd2_set_bgm_track_with_fade(10, 0);
        data_fd2_chapter_intro_menu_cursor_state = 0;

        intro_rle = (void *)fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat, 0, (uint32)intro_panel_idx_lut[category]);
        fd2_rle_blit_sprite((uint32)intro_rle, 0, 0,
                            data_fd2_battle_scene_snapshot + 0x8088, 0x1c8, 0xffffffff);
        free(intro_rle);

        data_fd2_chapter_intro_menu_overlay_buf_ptr = 0;
        data_fd2_chapter_intro_menu_overlay_buf_ptr =
            fd2_load_dat_resource((uint32)data_fd2_string_resource_filename_fdother_dat, 0, 10);
        fd2_render_chapter_intro_overlay();
        fd2_play_palette_fade_in();
        fd2_clear_keyboard_buffer();

        do {
            fd2_render_chapter_intro_overlay();
            saved_tick = BIOS_TICK_WORD;
            while (fd2_check_keyboard_buffer_nonempty() == 0) {
                if ((int)(int16)BIOS_TICK_WORD - (int)(int16)saved_tick > 3
                    || (int)(int16)BIOS_TICK_WORD - (int)(int16)saved_tick < 0) {
                    data_fd2_chapter_intro_dialog_anim_frame_idx++;
                    if (data_fd2_chapter_intro_dialog_anim_frame_idx == 4) {
                        data_fd2_chapter_intro_dialog_anim_frame_idx = 0;
                    }
                    fd2_render_chapter_intro_overlay();
                    saved_tick = BIOS_TICK_WORD;
                }
            }
            data_fd2_input_key_input_mode = 0x10;
            int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                         (union REGS *)&data_fd2_input_last_key_pressed);
            if (data_fd2_input_key_input_mode == 0xe0
                || data_fd2_input_key_input_mode == 0x52) {
                data_fd2_input_key_input_mode = 0x1c;
            } else if (data_fd2_input_key_input_mode == 0x22) {
                debug_bgm_idx++;
                if (debug_bgm_idx == 10) {
                    debug_bgm_idx = 0;
                }
                fd2_set_bgm_track_with_fade((uint32)debug_bgm_idx, 0);
            } else if (data_fd2_input_key_input_mode == 0x4d) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_chapter_intro_menu_cursor_state--;
                if ((int)data_fd2_chapter_intro_menu_cursor_state < 0) {
                    data_fd2_chapter_intro_menu_cursor_state = 4;
                }
            } else if (data_fd2_input_key_input_mode == 0x4b) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_chapter_intro_menu_cursor_state++;
                if ((int)data_fd2_chapter_intro_menu_cursor_state > 4) {
                    data_fd2_chapter_intro_menu_cursor_state = 0;
                }
            } else if (data_fd2_input_key_input_mode ==
                       *(uint8 *)(data_fd2_chapter_intro_active_metadata_entry_ptr + 2)
                       && *(uint8 *)(data_fd2_chapter_intro_active_metadata_entry_ptr
                                     + 1) == data_fd2_chapter_intro_menu_cursor_state) {
                data_fd2_chapter_intro_menu_cursor_state = 5;
            }
            if (data_fd2_input_key_input_mode == 0x1c
                || data_fd2_input_last_key_pressed == ' ') {
                if (data_fd2_chapter_intro_menu_cursor_state != 2) {
                    fd2_play_sfx_with_handle(
                        data_fd2_audio_fdother_sfx_bank_buf_ptr, 1, 3);
                }
                commit_result = (uint8)fd2_chapter_transition_with_intro();
            }
        } while (commit_result == 0);

        free((void *)data_fd2_chapter_intro_menu_overlay_buf_ptr);
        if (data_fd2_chapter_intro_menu_cursor_state != 2) {
            return 1;
        }
    } else {
        /* ---- BATTLE CHAPTER: save prompt + recruitment screen ---- */
        memset((void *)0xa0000, 0, 64000);
        fd2_set_vga_palette_range(0, 0xff, 0);
        fd2_dialog_open_speaker_portrait(0x4b);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19a, 0xa9524,
                                 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        data_fd2_battle_tile_map_ptr = 1;
        dialog_result = (int)fd2_text_dialog_typewriter_loop();
        data_fd2_battle_tile_map_ptr = 0;
        fd2_animate_dialog_page_advance_collapse();
        fd2_close_intro_dialog_with_slide_out();
        if (dialog_result != -1 && data_fd2_ui_menu_cursor_idx == 0) {
            data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat, data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0xd);
            fd2_save_current_state_to_slot(0);
            free((void *)data_fd2_ui_menu_screen_sprite_atlas_buf_ptr);
        }
        do {
            data_fd2_battle_runtime_char_array_ptr =
                (runtime_char *)data_fd2_shared_menu_party_roster_buffer_ptr;
            recruit_result = fd2_run_recruitment_or_branch_screen();
            data_fd2_battle_runtime_char_array_ptr = (runtime_char *)0;
        } while (recruit_result == 0);
        fd2_set_vga_palette_range(0, 0xff, 0xff);
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_chapter_transition_with_intro @ 0x2D093  (1 caller)
 *
 * Inter-chapter dispatch with chapter-intro pose zoom-in. Sole caller:
 * fd2_chapter_transition_menu @ 0x2CAD7 (the story-chapter radio menu).
 * Returns 1 if the user committed the advance, 0 if cancelled.
 *
 * Setup: reads the chapter-intro metadata entry for the current chapter,
 * keeps its category byte, and stops the current BGM (track -1).
 *
 * If transition_state == 2 (story-dialog branch): shows the portrait +
 * dialog scene, runs the typewriter loop, and tears the dialog down. A
 * typewriter result of -1 or a non-zero menu cursor cancels (return 0).
 * Otherwise, past a late-chapter/game-clear party-size threshold it splices
 * the menu roster into the runtime char ptr and runs the recruitment/branch
 * screen (cancel there also returns 0). On success result_code = 1.
 *
 * Common pose zoom-in (10 frames): splices the menu roster into the runtime
 * char ptr (so the intro menus can read char data), backs up the VGA frame
 * to a malloc'd 64000-byte buffer, then for i in 1..10 blits the scaled pose
 * toward its per-chapter target position (table index = transition_state +
 * category*6) with a shrinking scale, commits the per-frame composite to
 * VGA, and ramps palette brightness. Then it settles the palette at full
 * intensity and clears VGA.
 *
 * Post-anim menu dispatch on transition_state selects a BGM and an intro
 * menu (typeB / typeC / main); state 2 skips this (result already set by the
 * dialog branch). After the menu, the post-intro ambient BGM (track 10) is
 * started. Cleanup frees the pose backup, nulls the runtime char ptr splice,
 * and returns result_code.
 *
 * The "_pose_y_row_table" feeds the blit's X arg and "_pose_x_column_table"
 * feeds the Y arg (the per-frame coordinate math: (table[off]-bias)*i/10 is a
 * signed 32-bit divide, *0x80, +screen_base).
 * ---------------------------------------------------------------- */
int fd2_chapter_transition_with_intro(void)
{
    uint8 *chapter_meta;
    uint8 chapter_meta_byte;
    int typewriter_result;
    int recruit_result;
    uint32 pose_bitmap;
    int iVar2;
    uint32 table_off;
    int result_code;

    result_code = 0;
    chapter_meta = fd2_get_chapter_intro_metadata_entry(
        (int)data_fd2_chapter_current_chapter_id);
    chapter_meta_byte = chapter_meta[0];
    fd2_set_bgm_track_with_fade(0xffffffff, 0);

    if (data_fd2_chapter_intro_menu_cursor_state == 2) {
        fd2_dialog_open_speaker_portrait(0x4b);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x201, 0xa951f,
                                 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        data_fd2_battle_tile_map_ptr = 1;
        fd2_paint_portrait_to_dialog_area(0);
        typewriter_result = (int)fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();
        data_fd2_battle_tile_map_ptr = 0;
        fd2_close_intro_dialog_with_slide_out();

        if (typewriter_result == -1 || data_fd2_ui_menu_cursor_idx != 0) {
            return 0;
        }

        if (((int)data_fd2_chapter_current_chapter_id < 0x1b
             && (int)data_fd2_shared_menu_party_member_count > 0x10)
            || ((int)data_fd2_chapter_current_chapter_id >= 0x1b
                && (int)data_fd2_shared_menu_party_member_count > 0x14)) {
            data_fd2_battle_runtime_char_array_ptr =
                (runtime_char *)data_fd2_shared_menu_party_roster_buffer_ptr;
            recruit_result = fd2_run_recruitment_or_branch_screen();
            if (recruit_result == 0) {
                data_fd2_battle_runtime_char_array_ptr = (runtime_char *)0;
                return 0;
            }
        }
        result_code = 1;
    }

    data_fd2_battle_runtime_char_array_ptr =
        (runtime_char *)data_fd2_shared_menu_party_roster_buffer_ptr;
    pose_bitmap = (uint32)malloc(64000);
    memmove((void *)pose_bitmap, (void *)0xa0000, 64000);

    for (iVar2 = 1; iVar2 < 0xb; iVar2++) {
        table_off = data_fd2_chapter_intro_menu_cursor_state
                  + (uint32)chapter_meta_byte * 6;
        fd2_blit_scaled_chapter_pose(
            (uint32)(((int)(data_fd2_chapter_intro_portrait_pose_y_row_table[
                                table_off] - 0x96) * iVar2 / 10) * 0x80
                     + 0x5000),
            (uint32)(((int)(data_fd2_chapter_intro_portrait_pose_x_column_table[
                                table_off] - 0x64) * iVar2 / 10) * 0x80
                     + 0x3200),
            pose_bitmap,
            (uint32)(0x80 - iVar2 * 9));
        memmove((void *)0xa0000,
                (void *)data_fd2_large_game_state_buffer_ptr, 64000);
        fd2_set_vga_palette_range(0, 0xff, (uint32)(iVar2 * 4));
    }
    fd2_set_vga_palette_range(0, 0xff, 0x40);
    memset((void *)0xa0000, 0, 64000);

    if (data_fd2_chapter_intro_menu_cursor_state == 0) {
        fd2_set_bgm_track_with_fade(0xd, 0);
        result_code = fd2_run_chapter_intro_menu_typeB(pose_bitmap);
    } else if (data_fd2_chapter_intro_menu_cursor_state == 4) {
        fd2_set_bgm_track_with_fade(0xb, 0);
        result_code = fd2_run_chapter_intro_menu_typeC(pose_bitmap);
    } else if (data_fd2_chapter_intro_menu_cursor_state == 2) {
        goto cleanup;
    } else {
        if (data_fd2_chapter_intro_menu_cursor_state == 3) {
            fd2_set_bgm_track_with_fade(0xf, 0);
        } else {
            fd2_set_bgm_track_with_fade(0xe, 0);
        }
        result_code = fd2_run_chapter_intro_menu_main(pose_bitmap);
    }
    fd2_set_bgm_track_with_fade(10, 0);

cleanup:
    free((void *)pose_bitmap);
    data_fd2_battle_runtime_char_array_ptr = (runtime_char *)0;
    return result_code;
}

/* ----------------------------------------------------------------
 * Data symbol owned by this module (.object2 @ 0x53A45)
 *
 * data_fd2_battle_runtime_char_array_ptr -- pointer to the active
 * runtime_char[] array (each entry 0x50 bytes). Zero (NULL) at program
 * start; assigned a malloc(0x1E00) buffer on chapter load / save restore,
 * freed and reset to NULL on chapter transition. Indexed throughout the
 * battle/field engine as data_fd2_battle_runtime_char_array_ptr[idx].field.
 * ---------------------------------------------------------------- */
runtime_char *data_fd2_battle_runtime_char_array_ptr;

/* ----------------------------------------------------------------
 * Data symbol owned by this module (.object2 @ 0x53A51)
 *
 * data_fd2_battle_tile_map_ptr -- pointer to the decoded per-chapter
 * battle tile map (loaded from FDFIELD.DAT, then reset in place by
 * fd2_battle_reset_tile_transient_state). The map header packs map_width_tiles /
 * map_height_tiles as the first two 16-bit words, followed by a packed
 * array of 4-byte tile-meta records indexed as
 * ((row * map_width + col) * 4 + base). Zero (NULL) at program start;
 * assigned on chapter load / save restore, freed and reset to 0 on
 * chapter transition.
 *
 * This same slot is overloaded by the dialog/menu typewriter code as a
 * small 3-state mode flag, so it is held in a 32-bit integer slot rather
 * than a typed pointer: 0 = no battle map / no dialog, 1 = full-screen
 * intro dialog active (set before the dialog loop, reset to 0 after),
 * >= 2 = a real tile-map pointer is loaded (in battle) -- the dialog code
 * tests "1 < ptr" to detect in-battle and re-composite the battle scene
 * behind a small dialog band instead of a full-screen dialog.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_tile_map_ptr;

/* ----------------------------------------------------------------
 * Data symbol owned by this module (.object2 @ 0x5412B)
 *
 * data_fd2_chapter_intro_menu_cursor_state -- chapter-intro / save menu
 * radio cursor. Zero at program start (BSS); reset to 0 each time the
 * intro panel opens, then driven by left/right input as a wrapped 0..4
 * selection (5 = special-hotkey commit, 2 = "save" branch). The wrap is
 * computed with signed arithmetic: decrement below 0 wraps to 4 and
 * increment above 4 wraps to 0, so use sites read it via an (int) cast
 * even though the slot is an unsigned 32-bit word. Held in a 32-bit
 * integer slot to match the dword loads/stores in the menu loop.
 * ---------------------------------------------------------------- */
uint32 data_fd2_chapter_intro_menu_cursor_state;

/* ----------------------------------------------------------------
 * Data symbol owned by this module (.object2 @ 0x54133)
 *
 * data_fd2_chapter_intro_dialog_anim_frame_idx -- chapter-intro dialog
 * blink/idle animation frame index, wrapped 0..3. Zero at program start
 * (BSS). Advanced once every few BIOS ticks while the intro panel waits
 * for input (and re-seeded to 2 at the top of the dialog-blink wait
 * loop), wrapping back to 0 after frame 3; read by the intro-overlay /
 * dialog-panel renderers to pick the current blink frame. Held in a
 * 32-bit integer slot to match the dword INC/CMP/MOV accesses at the
 * use sites.
 * ---------------------------------------------------------------- */
uint32 data_fd2_chapter_intro_dialog_anim_frame_idx;

/* ----------------------------------------------------------------
 * Data symbol owned by this module (.object2 @ 0x54137)
 *
 * data_fd2_chapter_intro_active_metadata_entry_ptr -- cached pointer to
 * the active chapter's intro metadata entry (the byte array returned by
 * fd2_get_chapter_intro_metadata_entry). Zero (NULL) at program start
 * (BSS); assigned at runtime when a chapter intro opens
 * (fd2_chapter_transition_menu) or a save slot is restored
 * (fd2_load_state_from_selected_slot). Dereferenced byte-wise at fixed
 * offsets into the entry: [+1]/[+2] are the special-hotkey cursor/scancode
 * pair tested by the intro menu loop, and [+3..]/[+0x0F..]/[+0x17..] are
 * per-state party-roster byte slices read by fd2_load_chapter_shop_item_ids.
 * Held in a 32-bit integer slot to match the dword load/store of the
 * pointer at the use sites (the entry is then read via *(uint8 *)(ptr+N)).
 * ---------------------------------------------------------------- */
uint32 data_fd2_chapter_intro_active_metadata_entry_ptr;

/* ----------------------------------------------------------------
 * Data symbol owned by this module (.object2 @ 0x5413B)
 *
 * data_fd2_chapter_intro_menu_overlay_buf_ptr -- heap pointer to the
 * chapter-intro menu overlay sprite buffer (FDOTHER.DAT entry 10). Zero
 * (NULL) at program start (BSS); set to 0 then assigned the malloc'd
 * resource buffer at the top of the story-chapter intro branch in
 * fd2_chapter_transition_menu (= fd2_load_dat_resource("FDOTHER.DAT", 10)),
 * read by fd2_render_chapter_intro_overlay as the panel sprite source for
 * fd2_dialog_sprite_blit_normal, and free'd when the intro loop ends. Held
 * in a 32-bit integer slot to match the dword load/store of the pointer at
 * the use sites (the buffer is then cast to void * for free / blit).
 * ---------------------------------------------------------------- */
uint32 data_fd2_chapter_intro_menu_overlay_buf_ptr;
