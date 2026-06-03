/*
 * menucfg.c — Game options / settings menu loop
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_game_options_menu_loop @ 0x1728C  (1 caller: fd2_field_command_menu_loop)
 *
 * Game options / settings menu loop (BGM / SE / Speed / Other toggles).
 *
 * Copies the 16-byte slots template @ 0x51EAF into menu_options[0..3] and
 * the 16-byte (all-zero) state template @ 0x53F02 into menu_state[0..3],
 * then zeroes the menu cursor. Each iteration rebuilds the 4 text-token
 * labels from the current toggle states, opens the slide-in dialog, polls
 * fd2_settings_menu_input_step until it returns non-zero (0 = navigating,
 * 1 = selection, -1 = cancel), then closes the dialog. On cancel: return.
 * On selection: dispatch on the cursor index —
 *   0 -> toggle BGM enable; AIL_set_sequence_volume fades the BGM sequence
 *        to 0x7F (on) or 0 (off) over 1000 ms.
 *   2 -> toggle game-speed flag.
 *   3 -> toggle terrain-HUD user-enabled flag.
 *   else -> toggle SFX enable (explicit 0/1 store, matching the disassembly).
 * Then re-render with updated labels and continue.
 *
 * void __cdecl with the __CHK(0x3c) stack-probe prologue.
 * ---------------------------------------------------------------- */
void fd2_game_options_menu_loop(void)
{
    int32 menu_options[4];
    int32 menu_state[4];
    int input_result;
    int new_volume;

    menu_options[0] = data_fd2_ui_game_options_menu_slots_template[0];
    menu_options[1] = data_fd2_ui_game_options_menu_slots_template[1];
    menu_options[2] = data_fd2_ui_game_options_menu_slots_template[2];
    menu_options[3] = data_fd2_ui_game_options_menu_slots_template[3];

    menu_state[0] = data_fd2_ui_game_options_menu_state_template[0];
    menu_state[1] = data_fd2_ui_game_options_menu_state_template[1];
    menu_state[2] = data_fd2_ui_game_options_menu_state_template[2];
    menu_state[3] = data_fd2_ui_game_options_menu_state_template[3];

    data_fd2_ui_menu_cursor_idx = 0;

    while (1) {
        menu_options[0] = 0x12 + (data_fd2_audio_bgm_enabled_flag == 0);
        menu_options[1] = 0x14 + (data_fd2_audio_sfx_enabled_flag == 0);
        menu_options[2] = 0x16 + (data_fd2_ui_game_speed_flag != 0);
        menu_options[3] = 0x18 + (data_fd2_ui_terrain_hud_user_enabled == 0);

        fd2_open_settings_dialog_with_slide(menu_options, menu_state);
        do {
            input_result = fd2_settings_menu_input_step(menu_options,
                menu_state);
        } while (input_result == 0);
        fd2_close_settings_dialog_with_slide(menu_options, menu_state);

        if (input_result == -1) {
            return;
        }

        if (data_fd2_ui_menu_cursor_idx == 0) {
            data_fd2_audio_bgm_enabled_flag =
                (data_fd2_audio_bgm_enabled_flag == 0);
            if (data_fd2_audio_bgm_enabled_flag) {
                new_volume = 0x7f;
            } else {
                new_volume = 0;
            }
            AIL_set_sequence_volume(data_fd2_audio_bgm_sequence_handle,
                new_volume, 1000);
        } else if (data_fd2_ui_menu_cursor_idx == 2) {
            data_fd2_ui_game_speed_flag = data_fd2_ui_game_speed_flag ^ 1;
        } else if (data_fd2_ui_menu_cursor_idx == 3) {
            data_fd2_ui_terrain_hud_user_enabled =
                data_fd2_ui_terrain_hud_user_enabled ^ 1;
        } else {
            if (data_fd2_audio_sfx_enabled_flag == 0) {
                data_fd2_audio_sfx_enabled_flag = 1;
            } else {
                data_fd2_audio_sfx_enabled_flag = 0;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_open_settings_dialog_with_slide @ 0x1741C  (5 callers:
 *   fd2_field_command_menu_loop, fd2_field_menu_status_save_load_quit_dispatch,
 *   fd2_game_options_menu_loop, fd2_item_command_menu_dispatch,
 *   fd2_player_inline_action_menu_dispatch)
 *
 * Open a 2-panel settings dialog at the cursor position with a 4-frame
 * outward-corner slide animation.
 *
 * Remembers the character under the cursor (for restamp), computes the panel
 * anchor inside the work buffer from the on-screen cursor cell, plays the open
 * chime, pre-renders the battle base scene + terrain HUD, then backs up the
 * dialog area. Over 4 frames the 4 corner offsets spread outward (TL/BR by
 * 0x8E8, TR/BL by 6 each), and on every frame the 4 corner sprites are blitted
 * at their current offsets, the saved char (if any) is restamped, and the
 * dialog region is flushed to screen.
 *
 * Per-corner sprite atlas index = menu_options[c]*3 + menu_state[c]*2; the
 * index selects a dword offset from the dialog-state handle's offset table,
 * and the sprite address is handle + that offset.
 *
 * Counterpart: fd2_close_settings_dialog_with_slide @ 0x176B4 (4-frame
 * inward-converge close animation).
 *
 * void __cdecl with the __CHK(0x44) stack-probe prologue.
 * ---------------------------------------------------------------- */
void fd2_open_settings_dialog_with_slide(int32 *menu_options, int32 *menu_state)
{
    int32 corner_offsets[4];
    int saved_char_idx;
    uint32 panel_anchor;
    int frame_iter;
    int corner_iter;
    int sprite_id;
    uint32 sprite_addr;

    saved_char_idx = fd2_find_char_at_cursor_pos();

    panel_anchor = data_fd2_large_game_state_buffer_ptr + 0x8088 +
                   data_fd2_battle_cursor_screen_x * 0x18 +
                   data_fd2_battle_cursor_screen_y * 0x2AC0;

    corner_offsets[0] = 0x390;
    corner_offsets[1] = 0x390;
    corner_offsets[2] = 0x390;
    corner_offsets[3] = 0x390;

    fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 8, 1);
    fd2_tick_chapter_palette_animation();

    fd2_composite_battle_tile_map(
        data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8, 0xD, 8,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);
    fd2_composite_all_chars_overlay();
    fd2_render_terrain_info_hud_panel(
        data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8);
    fd2_backup_dialog_area_to_buffer();

    for (frame_iter = 0; frame_iter < 4; frame_iter++) {
        corner_offsets[0] -= 0x8E8;
        corner_offsets[1] -= 6;
        corner_offsets[2] += 6;
        corner_offsets[3] += 0x8E8;

        fd2_restore_dialog_area_from_buffer();

        for (corner_iter = 0; corner_iter < 4; corner_iter++) {
            sprite_id = menu_options[corner_iter] * 3 +
                        menu_state[corner_iter] * 2;
            sprite_addr = data_fd2_menu_dialog_state_handle +
                *(int32 *)(data_fd2_menu_dialog_state_handle + sprite_id * 4);
            fd2_blit_sprite_with_stride_setup(
                panel_anchor + corner_offsets[corner_iter], sprite_addr, 0x1C8);
        }

        if (saved_char_idx != -1) {
            fd2_paint_char_sprite_at_world_pos(saved_char_idx);
        }

        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8, 0x138, 0xC0);
    }
}

/* ----------------------------------------------------------------
 * fd2_count_active_menu_items_until_zero @ 0x173E7  (2 callers:
 *   fd2_item_command_menu_dispatch, fd2_player_inline_action_menu_dispatch)
 *
 * Count the leading non-zero entries (up to 4) of a 4-slot int menu
 * definition and store the count in data_fd2_ui_menu_cursor_idx. Used by
 * the settings / item-command UI to position the cursor at the first
 * empty (zeroed) slot, i.e. just past the last active entry.
 *
 * After return: data_fd2_ui_menu_cursor_idx is in [0, 4].
 *
 * void __cdecl with the __CHK(4) stack-probe prologue.
 * ---------------------------------------------------------------- */
void fd2_count_active_menu_items_until_zero(int32 *menu_def)
{
    for (data_fd2_ui_menu_cursor_idx = 0;
         data_fd2_ui_menu_cursor_idx < 4 &&
             menu_def[data_fd2_ui_menu_cursor_idx] != 0;
         data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1) {
        /* empty body — search-and-store */
    }
}
