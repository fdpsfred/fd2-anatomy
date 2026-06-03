/*
 * menu.c — Top-level per-frame game event / input dispatcher
 */

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
