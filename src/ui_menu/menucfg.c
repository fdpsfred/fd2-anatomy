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
