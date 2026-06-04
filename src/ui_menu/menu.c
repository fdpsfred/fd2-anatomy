/*
 * menu.c — Top-level per-frame game event / input dispatcher
 */

#include <stdlib.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_game_main_loop @ 0x117E7  (1 caller: fd2_main)
 *
 * Per-frame game event handler, dispatched from fd2_main's main loop.
 * Covers both field/map exploration and in-battle input. Reads one
 * keyboard scancode from fd2_wait_for_input_with_idle and dispatches.
 *
 * Returns an int consumed by fd2_main: the field-command path returns
 * the command-loop result (0 mapped to non-zero / 1 mapped to 0), every
 * other path returns 0. (Ghidra decompiles this as void and drops the
 * EAX return values; the disassembly shows MOV EAX,EBX / XOR EAX,EAX
 * return paths and fd2_main consuming EAX via MOV ESI,EAX.)
 * ---------------------------------------------------------------- */
/* ----------------------------------------------------------------
 * fd2_field_command_menu_loop @ 0x16F55  (1 caller: fd2_game_main_loop)
 *
 * Field command menu — modal popup when the player presses Space/Enter
 * on an empty tile. Items: Save/Load, End Turn, Options, Suspend.
 *
 * Copies the 16-byte options template @ 0x51E9F and the 16-byte state
 * template @ 0x53EF2 into locals, zeroes the menu cursor, renders the
 * menu and loops settings-menu input until it returns non-zero.
 * Cancel (-1) returns 1. Dispatch on cursor idx:
 *   0 SAVE/LOAD/NEW GAME -> fd2_field_menu_status_save_load_quit_dispatch().
 *   1 END MY TURN  -> "End your turn?" prompt; if confirmed, finalize each
 *     player char's move (walk-to-tile + post-action consequence dispatch),
 *     run the turn cycle, return 1.
 *   2 OPTIONS -> fd2_game_options_menu_loop(); return 0.
 *   3 SUSPEND -> "Suspend the game?" prompt; if confirmed, run turn cycle,
 *     return 1.
 * Sub-prompt cancellation shows the "Aborted" dialog (0x19C) and returns 1.
 *
 * EAX-bug note: the dialog_result of fd2_text_dialog_typewriter_loop is
 * MOV EBX,EAX immediately after the CALL (0x1704c / 0x17213) — captured
 * faithfully here.
 * ---------------------------------------------------------------- */
int fd2_field_command_menu_loop(void)
{
    int32 menu_options[4];
    int32 menu_state[4];
    int input_result;
    int dialog_result;
    runtime_char *pchar;
    uint32 char_idx;
    int cursor_world_x;
    int cursor_world_y;

    menu_options[0] = data_fd2_ui_field_command_menu_options_template[0];
    menu_options[1] = data_fd2_ui_field_command_menu_options_template[1];
    menu_options[2] = data_fd2_ui_field_command_menu_options_template[2];
    menu_options[3] = data_fd2_ui_field_command_menu_options_template[3];

    menu_state[0] = data_fd2_ui_field_command_menu_state_template[0];
    menu_state[1] = data_fd2_ui_field_command_menu_state_template[1];
    menu_state[2] = data_fd2_ui_field_command_menu_state_template[2];
    menu_state[3] = data_fd2_ui_field_command_menu_state_template[3];

    data_fd2_ui_menu_cursor_idx = 0;
    fd2_open_settings_dialog_with_slide(menu_options, menu_state);
    do {
        input_result = fd2_settings_menu_input_step(menu_options, menu_state);
    } while (input_result == 0);
    fd2_close_settings_dialog_with_slide(menu_options, menu_state);
    fd2_composite_battle_frame(0);

    if (input_result == -1) {
        return 1;
    }

    if (data_fd2_ui_menu_cursor_idx == 0) {
        return fd2_field_menu_status_save_load_quit_dispatch();
    }

    if (data_fd2_ui_menu_cursor_idx == 1) {
        fd2_load_chapter_portrait(
            data_fd2_battle_runtime_char_array_ptr->portrait_id);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a1, 0xa9f23,
            0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        dialog_result = fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();

        if ((dialog_result == 1) && (data_fd2_ui_menu_cursor_idx == 0)) {
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a2, 0xab6e3,
                0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            __delay_thunk_375b2(200);
            fd2_close_status_screen_with_slide_out();
            cursor_world_x = data_fd2_battle_cursor_world_x;
            cursor_world_y = data_fd2_battle_cursor_world_y;
            data_fd2_battle_anim_phase = 0;
            data_fd2_ui_play_active_flag = 0;
            for (char_idx = 0;
                 (int)char_idx < (int)data_fd2_battle_party_member_count;
                 char_idx = char_idx + 1) {
                pchar = data_fd2_battle_runtime_char_array_ptr + char_idx;
                if (((pchar->flags & 0x85) == 0) && (pchar->team == 2)) {
                    fd2_pan_cursor_to_tile_animated(pchar->pos_x, pchar->pos_y);
                    data_fd2_battle_ai_post_action_consequence_idx = 0xff;
                    fd2_ai_walk_to_target_tile(cursor_world_x, cursor_world_y,
                        char_idx, 1);
                    if (data_fd2_battle_ai_post_action_consequence_idx
                            != 0xff) {
                        data_fd2_battle_ai_post_action_consequence_table
                            [data_fd2_battle_ai_post_action_consequence_idx](
                            char_idx);
                    }
                    fd2_clear_all_chars_facing();
                    fd2_mark_char_acted_this_turn(char_idx);
                }
            }
            fd2_composite_battle_frame(0);
            fd2_run_full_turn_cycle();
            data_fd2_battle_anim_phase = 1;
            data_fd2_ui_play_active_flag = 1;
            return 1;
        }
    }
    else {
        if (data_fd2_ui_menu_cursor_idx == 2) {
            fd2_game_options_menu_loop();
            return 0;
        }
        if (data_fd2_ui_menu_cursor_idx != 3) {
            return 0;
        }
        fd2_load_chapter_portrait(0x4b);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a3, 0xa9f23,
            0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        dialog_result = fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();

        if ((dialog_result == 1) && (data_fd2_ui_menu_cursor_idx == 0)) {
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a4, 0xab6e3,
                0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            __delay_thunk_375b2(200);
            fd2_close_status_screen_with_slide_out();
            data_fd2_ui_play_active_flag = 0;
            fd2_run_full_turn_cycle();
            data_fd2_ui_play_active_flag = 1;
            return 1;
        }
    }

    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19c, 0xab6e3, 0x140,
        0xcd, 0x4c, 0x4a, 0x13, 1);
    __delay_thunk_375b2(200);
    fd2_close_status_screen_with_slide_out();
    return 1;
}

int fd2_game_main_loop(void)
{
    int scancode;
    uint32 arr_idx;
    uint32 iter;
    int found_actor;
    uint32 sel;
    int cmd_result;

    scancode = fd2_wait_for_input_with_idle();

    if ((scancode == 0x01) || (scancode == 0x2c) || (scancode == 0x4c)) {
        /* "next actor" path */
        found_actor = 0;
        arr_idx = data_fd2_battle_current_active_char_idx;
        for (iter = 0; (int)iter < (int)data_fd2_battle_party_member_count;
             iter = iter + 1) {
            if (((data_fd2_battle_runtime_char_array_ptr[arr_idx].flags & 0x85)
                    == 0)
                && (data_fd2_battle_runtime_char_array_ptr[arr_idx].team == 2)
                && (found_actor == 0)) {
                fd2_pan_cursor_to_char(arr_idx);
                data_fd2_battle_current_active_char_idx = arr_idx + 1;
                if (data_fd2_battle_current_active_char_idx
                        == data_fd2_battle_party_member_count) {
                    data_fd2_battle_current_active_char_idx = 0;
                }
                found_actor = 1;
            }
            arr_idx = arr_idx + 1;
            if (arr_idx == data_fd2_battle_party_member_count) {
                arr_idx = 0;
            }
        }
        fd2_clear_keyboard_buffer();
        return 0;
    }

    if ((scancode == 0x39) || (scancode == 0x1c)) {
        /* "action / confirm" */
        if (data_fd2_ui_click_debounce_skip_count != 0) {
            data_fd2_ui_click_debounce_skip_count =
                data_fd2_ui_click_debounce_skip_count - 1;
        }
        else {
            while (data_fd2_chapter_chapter_init_done_flag == 0) {
                fd2_set_chapter_init_done_flag();
            }
        }

        sel = (uint32)fd2_find_char_at_cursor_pos();
        if (sel == 0xffffffff) {
            do {
                cmd_result = fd2_field_command_menu_loop();
            } while (cmd_result == 0);
            if (cmd_result == 1) {
                cmd_result = 0;
            }
            return cmd_result;
        }

        data_fd2_battle_pending_xp_credit = 0;
        if ((data_fd2_battle_runtime_char_array_ptr[sel].portrait_id != 0x79)
            && (data_fd2_battle_runtime_char_array_ptr[sel].archetype_flag
                    != 10)) {
            if ((data_fd2_battle_runtime_char_array_ptr[sel].team == 2)
                && ((data_fd2_battle_runtime_char_array_ptr[sel].flags & 0x80)
                        == 0)
                && (data_fd2_battle_runtime_char_array_ptr[sel].status_sleep_flag
                        == 0)) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 7, 1);
                while (fd2_player_action_menu_loop(sel) == 0) {
                }
            }
            else {
                fd2_open_char_status_screen(sel);
            }

            fd2_composite_battle_frame(0);
            if (99 < (int)data_fd2_battle_pending_xp_credit) {
                data_fd2_battle_pending_xp_credit = 99;
            }
            fd2_process_xp_and_level_up_for_char(sel);
            data_fd2_chapter_post_action_handler_table
                [data_fd2_chapter_current_chapter_id](sel);
            fd2_check_all_player_acted_or_asleep();
            if (data_fd2_battle_ai_post_action_consequence_idx != 0xff) {
                data_fd2_battle_ai_post_action_consequence_table
                    [data_fd2_battle_ai_post_action_consequence_idx](sel);
            }
            data_fd2_battle_ai_post_action_consequence_idx = 0xff;
            fd2_clear_keyboard_buffer();
        }
        return 0;
    }

    if (scancode == 0x22) {
        /* placeholder / no-op (chapter-specific reserved) */
        return 0;
    }

    if ((scancode == 0x3b) || (scancode == 0x49)) {
        fd2_open_tactical_overview_zoom();
        return 0;
    }

    if ((scancode == 0x3c) || (scancode == 0x47)) {
        sel = (uint32)fd2_find_char_at_cursor_pos();
        if ((sel != 0xffffffff)
            && (data_fd2_battle_runtime_char_array_ptr[sel].portrait_id != 0x79)
            && (data_fd2_battle_runtime_char_array_ptr[sel].archetype_flag
                    != 10)) {
            fd2_open_char_status_screen(sel);
        }
        return 0;
    }

    if (scancode == 0x48) {
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
        fd2_cursor_move_up();
        return 0;
    }
    if (scancode == 0x50) {
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
        fd2_cursor_move_down();
        return 0;
    }
    if (scancode == 0x4b) {
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
        fd2_cursor_move_left();
        return 0;
    }
    if (scancode == 0x4d) {
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
        fd2_cursor_move_right();
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_player_action_menu_loop @ 0x18890  (1 caller: fd2_game_main_loop)
 *
 * Player turn action UI for the selected character: shows the movement-
 * range highlight, lets the player pick a destination tile + action
 * (Attack/Spell/Item/Stand). The human-side counterpart of the enemy
 * turn action dispatcher.
 *
 * Setup: copy the 16-byte (4-int) player_action_menu_state_template
 * @ 0x53F12 into menu_state; result_code @ 0x53C53 = 0; consequence_idx
 * @ 0x51A8F = 0xFF. Movement-cost source = char job_id (+0x20), overridden
 * to 0x13 when fd2_check_char_status_immunity != 0 (flying), or to 0x10
 * when the +0x07 portrait_id byte == 0x1C. Flood-fill reachable tiles,
 * block ally tiles, wait for a target. On cancel (-1): pan back, free,
 * return 1. Otherwise pathfind to the destination; if a path with steps
 * exists, animate the walk and (for portrait_id not in {0x12,0x13,0x22})
 * enable the "moved" submenu, then run the inline action submenu until it
 * commits or cancels.
 *
 * Returns 0 = "cancelled, re-prompt" (caller loops until non-zero),
 * non-zero = "action committed, advance turn".
 *
 * NOTE on offsets: the +0x07 byte (compared to 0x1C / 0x12 / 0x13 / 0x22)
 * is the runtime_char portrait_id field. The +0x3B byte (movement range
 * remaining) is combat_aux_block[0x14].
 * NOTE: saved_x/saved_y hold the cursor position captured *before* the
 * target-input call (the player's origin tile); after target input the
 * global cursor holds the chosen destination. The Ghidra decomp names
 * these locals target_x/target_y but they are the saved origin.
 * ---------------------------------------------------------------- */
int fd2_player_action_menu_loop(uint32 char_idx)
{
    int32 menu_state[4];
    runtime_char *pchar;
    uint8 *move_cost_table;
    void *alloc_buf;
    uint8 range_remaining;
    uint32 movement_class;
    uint32 saved_x;
    uint32 saved_y;
    int target_result;
    int path_step_count;
    int dispatch_result;

    menu_state[0] = data_fd2_ui_player_action_menu_state_template[0];
    menu_state[1] = data_fd2_ui_player_action_menu_state_template[1];
    menu_state[2] = data_fd2_ui_player_action_menu_state_template[2];
    menu_state[3] = data_fd2_ui_player_action_menu_state_template[3];

    data_fd2_battle_player_action_result_code = 0;
    data_fd2_battle_ai_post_action_consequence_idx = 0xff;

    pchar = data_fd2_battle_runtime_char_array_ptr + char_idx;
    range_remaining = pchar->combat_aux_block[0x14];
    movement_class = pchar->job_id;
    if (fd2_check_char_status_immunity(char_idx) != 0) {
        movement_class = 0x13;
    }
    else if (pchar->portrait_id == 0x1c) {
        movement_class = 0x10;
    }
    move_cost_table = fd2_get_movement_cost_table_for_job(movement_class);
    alloc_buf = malloc(range_remaining);

    fd2_paint_threat_overlay_for_team(1);
    fd2_init_movement_range_floodfill((uint32)move_cost_table,
        data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y,
        range_remaining, data_fd2_battle_tile_map_ptr,
        data_fd2_tile_attribute_flags_buffer_ptr);
    fd2_mark_char_occupant_tiles_for_team(char_idx, 1);

    saved_x = data_fd2_battle_cursor_world_x;
    saved_y = data_fd2_battle_cursor_world_y;
    fd2_wait_input_with_status_panel_repaint(char_idx);

    target_result = fd2_wait_for_action_target_input(4, 0, (uint8 *)0);
    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);

    if (target_result == -1) {
        fd2_pan_cursor_to_tile_animated(saved_x, saved_y);
        free(alloc_buf);
        return 1;
    }

    fd2_paint_threat_overlay_for_team(1);
    path_step_count = fd2_pathfind_to_destination((uint32)move_cost_table,
        saved_x, saved_y, range_remaining, (uint32)alloc_buf,
        data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y, 0,
        data_fd2_battle_tile_map_ptr, data_fd2_tile_attribute_flags_buffer_ptr);
    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_to_tile_animated(saved_x, saved_y);
    data_fd2_battle_anim_phase = 1;

    if ((path_step_count == 0) || (path_step_count == 0xff)) {
        if (path_step_count != 0) {
            return 1;
        }
        do {
            dispatch_result =
                fd2_player_inline_action_menu_dispatch(char_idx, menu_state, 0);
        } while (dispatch_result == 0);
        if (dispatch_result != -1) {
            fd2_check_tile_event_post_action(pchar->pos_x, pchar->pos_y, 1);
            free(alloc_buf);
            return 1;
        }
        free(alloc_buf);
        if (data_fd2_battle_player_action_result_code == 0) {
            return 0;
        }
    }
    else {
        fd2_walk_path_animation_loop(char_idx, (uint32)alloc_buf,
            path_step_count);
        fd2_clear_all_chars_facing();
        if ((pchar->portrait_id != 0x12) && (pchar->portrait_id != 0x13)
            && (pchar->portrait_id != 0x22)) {
            menu_state[1] = 1;
        }
        do {
            dispatch_result =
                fd2_player_inline_action_menu_dispatch(char_idx, menu_state, 1);
        } while (dispatch_result == 0);
        if (dispatch_result != -1) {
            fd2_check_tile_event_post_action(pchar->pos_x, pchar->pos_y, 1);
            free(alloc_buf);
            return dispatch_result;
        }
        free(alloc_buf);
        if (data_fd2_battle_player_action_result_code == 0) {
            pchar = data_fd2_battle_runtime_char_array_ptr + char_idx;
            pchar->pos_x = (uint8)saved_x;
            pchar->pos_y = (uint8)saved_y;
            fd2_pan_cursor_to_tile_animated(saved_x, saved_y);
            return data_fd2_battle_player_action_result_code;
        }
    }

    fd2_mark_char_acted_this_turn(char_idx);
    fd2_check_tile_event_post_action(pchar->pos_x, pchar->pos_y, 1);
    return data_fd2_battle_player_action_result_code;
}
