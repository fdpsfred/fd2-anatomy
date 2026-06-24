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
 * fd2_close_settings_dialog_with_slide @ 0x176B4  (5 callers:
 *   fd2_field_command_menu_loop, fd2_field_menu_status_save_load_quit_dispatch,
 *   fd2_game_options_menu_loop, fd2_item_command_menu_dispatch,
 *   fd2_player_inline_action_menu_dispatch)
 *
 * Counterpart to fd2_open_settings_dialog_with_slide: close the 2-panel
 * settings dialog with a 4-frame inward-converge slide animation.
 *
 * Plays the close chime, remembers the character under the cursor (for
 * restamp), computes the panel anchor inside the work buffer from the
 * on-screen cursor cell. The 4 corner offsets start at their outer extents
 * and converge inward each frame (TL/BR by 0x8E8, TR/BL by 6 each, opposite
 * sense to open). On every frame the dialog area is restored from backup, the
 * 4 corner sprites are blitted at their current offsets, the saved char (if
 * any) is restamped, and the dialog region is flushed to screen. After the
 * loop the dialog area is restored once more and the keyboard buffer is
 * cleared.
 *
 * Per-corner sprite atlas index = menu_options[c]*3 + menu_state[c]*2; the
 * index selects a dword offset from the dialog-state handle's offset table,
 * and the sprite address is handle + that offset.
 *
 * void __cdecl with the __CHK(0x44) stack-probe prologue.
 * ---------------------------------------------------------------- */
void fd2_close_settings_dialog_with_slide(int32 *menu_options, int32 *menu_state)
{
    int32 corner_offsets[4];
    int saved_char_idx;
    uint32 panel_anchor;
    int frame_iter;
    int corner_iter;
    int sprite_id;
    uint32 sprite_addr;

    fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 8, 1);
    saved_char_idx = fd2_find_char_at_cursor_pos();

    panel_anchor = data_fd2_large_game_state_buffer_ptr + 0x8088 +
                   data_fd2_battle_cursor_screen_x * 0x18 +
                   data_fd2_battle_cursor_screen_y * 0x2AC0;

    corner_offsets[0] = -0x23A0;
    corner_offsets[1] = 0x378;
    corner_offsets[2] = 0x3A8;
    corner_offsets[3] = 0x2AC0;

    for (frame_iter = 0; frame_iter < 4; frame_iter++) {
        corner_offsets[0] += 0x8E8;
        corner_offsets[1] += 6;
        corner_offsets[2] -= 6;
        corner_offsets[3] -= 0x8E8;

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

    fd2_restore_dialog_area_from_buffer();
    fd2_clear_keyboard_buffer();
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

/* ----------------------------------------------------------------
 * fd2_settings_menu_input_step @ 0x177FC  (5 callers:
 *   fd2_field_command_menu_loop, fd2_field_menu_status_save_load_quit_dispatch,
 *   fd2_game_options_menu_loop, fd2_item_command_menu_dispatch,
 *   fd2_player_inline_action_menu_dispatch)
 *
 * Single input-step for a 4-direction (cross-shape) settings menu.
 *
 * Waits for one keystroke via fd2_wait_input_with_dialog_repaint (which keeps
 * the slide-in dialog repainting / palette cycling while idle), then maps the
 * scancode:
 *   0x01 Esc                -> return -1 (cancelled)
 *   0x39 Space / 0x1C Enter -> return  1 (committed)
 *   0x48 Up    -> if menu_state[0] == 0: cursor = 0;  return 0
 *   0x50 Down  -> if menu_state[3] == 0: cursor = 3;  return 0
 *   0x4B Left  -> if menu_state[1] == 0: cursor = 1;  return 0
 *   0x4D Right -> if menu_state[2] == 0: cursor = 2;  return 0
 *   anything else / disabled slot -> return 0 (no-op)
 *
 * menu_state is the int[4] slot-disable array indexed by direction
 * (Up/Left/Right/Down = slot 0/1/2/3); 0 = enabled, non-zero = disabled.
 * The committed cursor index is stored in data_fd2_ui_menu_cursor_idx.
 * menu_options is forwarded unmodified to fd2_wait_input_with_dialog_repaint.
 *
 * int __cdecl with the __CHK(0x10) stack-probe prologue.
 * ---------------------------------------------------------------- */
int fd2_settings_menu_input_step(int32 *menu_options, int32 *menu_state)
{
    int scancode;

    scancode = fd2_wait_input_with_dialog_repaint((uint32)menu_options,
        (uint32)menu_state);

    if (scancode == 0x01) {
        return -1;
    }
    if (scancode == 0x39 || scancode == 0x1c) {
        return 1;
    }

    if (scancode == 0x48) {
        if (menu_state[0] == 0) {
            data_fd2_ui_menu_cursor_idx = 0;
        }
    } else if (scancode == 0x50) {
        if (menu_state[3] == 0) {
            data_fd2_ui_menu_cursor_idx = 3;
        }
    } else if (scancode == 0x4b) {
        if (menu_state[1] == 0) {
            data_fd2_ui_menu_cursor_idx = 1;
        }
    } else if (scancode == 0x4d) {
        if (menu_state[2] == 0) {
            data_fd2_ui_menu_cursor_idx = 2;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_repaint_settings_dialog_borders @ 0x179D5  (1 caller:
 *   fd2_wait_input_with_dialog_repaint)
 *
 * Repaint the 4 corner border sprites of a settings dialog. Called every
 * frame from the dialog idle/repaint loop.
 *
 * Remembers the character under the cursor (for restamp), computes the panel
 * anchor inside the work buffer from the on-screen cursor cell, then for each
 * of the 4 corners blits the corner sprite at its fixed offset (the same
 * offsets as the close-slide initial state). Per-corner sprite atlas index =
 * menu_options[c]*3 + menu_state[c]*2; the index selects a dword offset from
 * the dialog-state handle's offset table, and the sprite address is handle +
 * that offset. The currently-selected corner (c == data_fd2_ui_menu_cursor_idx)
 * adds the 0/1 blink-phase oscillator to its sprite index, alternating that
 * corner between two sprite frames to produce the cursor highlight animation.
 * Finally the saved char (if any) is restamped over the dialog.
 *
 * menu_options is the *3 array (param_1); menu_state is the *2 array (param_2).
 *
 * void __cdecl with the __CHK(0x38) stack-probe prologue.
 * ---------------------------------------------------------------- */
void fd2_repaint_settings_dialog_borders(uint32 menu_options, uint32 menu_state)
{
    int32 corner_offsets[4];
    int saved_char_idx;
    uint32 panel_anchor;
    int corner_iter;
    int sprite_id;
    uint32 sprite_addr;

    saved_char_idx = fd2_find_char_at_cursor_pos();

    panel_anchor = data_fd2_large_game_state_buffer_ptr + 0x8088 +
                   data_fd2_battle_cursor_screen_x * 0x18 +
                   data_fd2_battle_cursor_screen_y * 0x2AC0;

    corner_offsets[0] = -0x23A0;
    corner_offsets[1] = 0x378;
    corner_offsets[2] = 0x3A8;
    corner_offsets[3] = 0x2AC0;

    for (corner_iter = 0; corner_iter < 4; corner_iter++) {
        sprite_id = *(int32 *)(menu_options + corner_iter * 4) * 3 +
                    *(int32 *)(menu_state + corner_iter * 4) * 2;
        if ((uint32)corner_iter == data_fd2_ui_menu_cursor_idx) {
            sprite_id += data_fd2_dialog_blink_phase_oscillator;
        }
        sprite_addr = data_fd2_menu_dialog_state_handle +
            *(int32 *)(data_fd2_menu_dialog_state_handle + sprite_id * 4);
        fd2_blit_sprite_with_stride_setup(
            panel_anchor + corner_offsets[corner_iter], sprite_addr, 0x1C8);
    }

    if (saved_char_idx != -1) {
        fd2_paint_char_sprite_at_world_pos(saved_char_idx);
    }
}

/* ----------------------------------------------------------------
 * fd2_maybe_load_speed_mode_overlay @ 0x1A7BD  (1 caller:
 *   fd2_run_full_turn_cycle)
 *
 * Fast-mode gate: load the fast-walk animation overlay.
 *
 * When the player has set data_fd2_ui_game_speed_flag to 1 (fast mode) in the
 * settings menu, the AI / enemy turns swap to the trimmed walk-animation
 * resource at FDOTHER.DAT index 0x40. The overlay pointer is first cleared to
 * NULL, then assigned the freshly loaded resource. When fast mode is off the
 * pointer stays NULL and the AI / enemy turns use the standard walk animation.
 *
 * Called at the entry of the NPC turn (Phase C) and the ENEMY turn (Phase E)
 * in fd2_run_full_turn_cycle; released on exit by
 * fd2_maybe_free_speed_mode_overlay.
 *
 * void __cdecl with the __CHK(0x10) stack-probe prologue.
 * ---------------------------------------------------------------- */
void fd2_maybe_load_speed_mode_overlay(void)
{
    if (data_fd2_ui_game_speed_flag != 0) {
        data_fd2_battle_fast_mode_walk_overlay_ptr = 0;
        data_fd2_battle_fast_mode_walk_overlay_ptr = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x40);
    }
}

/* ----------------------------------------------------------------
 * fd2_maybe_free_speed_mode_overlay @ 0x1A7F1  (1 caller:
 *   fd2_run_full_turn_cycle)
 *
 * Fast-mode cleanup: release the fast-walk animation overlay allocated by
 * fd2_maybe_load_speed_mode_overlay.
 *
 * When data_fd2_ui_game_speed_flag is set (fast mode), the overlay resource at
 * data_fd2_battle_fast_mode_walk_overlay_ptr is freed. When fast mode is off
 * the pointer was never loaded and nothing is freed.
 *
 * Called at the exit of the NPC turn (Phase C) and the ENEMY turn (Phase E)
 * in fd2_run_full_turn_cycle, pairing with fd2_maybe_load_speed_mode_overlay.
 *
 * void __cdecl with the __CHK(8) stack-probe prologue.
 * ---------------------------------------------------------------- */
void fd2_maybe_free_speed_mode_overlay(void)
{
    if (data_fd2_ui_game_speed_flag != 0) {
        free((void *)data_fd2_battle_fast_mode_walk_overlay_ptr);
    }
}

/* ----------------------------------------------------------------
 * data_fd2_battle_fast_mode_walk_overlay_ptr @ 0x53B0F  (4 bytes, .object2)
 *
 * Speed-mode attack-hit SFX sample bank pointer (NOT a walk-animation
 * overlay -- the only reader uses it solely as an SFX bank base). Holds
 * FDOTHER.DAT resource index 0x40, the SFX sample bank used for weapon
 * attack-hit sounds while fast/speed mode is on. Sibling of the audio-domain
 * SFX banks (data_fd2_audio_fdother_sfx_bank_buf_ptr = FDOTHER.DAT[0x1F],
 * the default bank loaded at startup); this one is loaded lazily only in
 * fast mode and holds index 0x40 instead.
 *
 * NULL in the initial image (zero-bss); first touched by a write. When fast
 * mode is on, fd2_maybe_load_speed_mode_overlay stores NULL then assigns the
 * fd2_load_dat_resource(FDOTHER.DAT, 0, 0x40) result here;
 * fd2_maybe_free_speed_mode_overlay frees it; fd2_animate_attack_hit_sequence
 * passes it as the sfx_table_base (arg1) to fd2_play_sfx_with_handle. When
 * fast mode is off it stays NULL and is never read. Accessor: MOV dword ptr
 * [0x53B0F] (32-bit), a single pointer-sized SFX-bank handle.
 *
 * Name note: the live "walk_overlay" name is misleading; Stage 2 renames to
 * data_fd2_audio_speed_mode_attack_sfx_bank_buf_ptr (audio-domain SFX-bank
 * family, *_sfx_bank_buf_ptr suffix).
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_fast_mode_walk_overlay_ptr;
