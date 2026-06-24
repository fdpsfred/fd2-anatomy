/*
 * menu.c — Top-level per-frame game event / input dispatcher
 */

#include <stdlib.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_game_main_loop @ 0x117E7  (1 caller: main)
 *
 * Per-frame game event handler, dispatched from main's main loop.
 * Covers both field/map exploration and in-battle input. Reads one
 * keyboard scancode from fd2_wait_for_input_with_idle and dispatches.
 *
 * Returns an int consumed by main (which re-invokes this on 0, exits its
 * inner loop on non-zero). Only the field-command path (Space/Enter on an
 * empty tile) can return non-zero: it loops fd2_field_command_menu_loop
 * until that returns non-zero, then maps 1 back to 0 and returns the rest
 * unchanged. Every other dispatch path returns 0. (Ghidra decompiles this
 * as void and drops the EAX return values; the disassembly shows MOV
 * EAX,EBX / XOR EAX,EAX return paths and main consuming EAX via MOV
 * ESI,EAX.)
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
            fd2_delay_ms(200);
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
            fd2_delay_ms(200);
            fd2_close_status_screen_with_slide_out();
            data_fd2_ui_play_active_flag = 0;
            fd2_run_full_turn_cycle();
            data_fd2_ui_play_active_flag = 1;
            return 1;
        }
    }

    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19c, 0xab6e3, 0x140,
        0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_delay_ms(200);
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

/* ----------------------------------------------------------------
 * fd2_player_inline_action_menu_dispatch @ 0x18D8C  (1 caller:
 *                                          fd2_player_action_menu_loop)
 *
 * Inline action submenu (Attack / Spell / Item / Wait) opened after the
 * player picks a destination tile. pSlot_disable_arr is a 4-int gating
 * array (non-zero = disabled); have_moved = 1 after actual movement, 0
 * before.
 *   Return: 1 = action committed, -1 = cancelled all the way out,
 *           0 = re-prompt outer menu.
 *
 * Slot gating: Attack disabled if no weapon equipped or no targets in
 * range of the weapon's AoE; Item disabled if no usable inventory slots;
 * Spell disabled if no usable spells or the unit is silenced
 * (combat_aux_block[0] != 0). The 4-int menu template @ 0x51ED5 is
 * { 0, 1, 2, 3 } (the four slot ids).
 *
 * Selection dispatch:
 *   0 Attack — AoE target pick, then combat cinematic + damage, death
 *     animation, loot-drop processing.
 *   1 Spell  — spell menu; on commit, divide pending_xp_credit by the cast
 *     divisor = character level (status_flags_block[0]), +30 when job_id > 8
 *     (i.e. a promoted/advanced class, 09h and up) -- throttles spell XP for
 *     higher-level and promoted casters.
 *   2 Item   — item menu; item use grants no XP (pending_xp_credit = 0).
 *   3 Wait   — recover 20% HP if not yet moved, run tile-event interaction.
 *
 * int __cdecl with the __CHK(0xB0) stack-probe prologue. saved_cursor_x/y
 * capture the destination tile before the Attack target-pick so cancel can
 * pan back. EAX-bug notes: the settings-menu input result is captured
 * MOV EBX,EAX after the CALL and reused at CMP EBX,-1; the malloc result
 * (target id buffer) is MOV ESI,EAX; fd2_compute_aoe_targets's count is
 * passed straight into fd2_wait_for_action_target_input; the attack target
 * id (fd2_find_char_at_cursor_pos) is MOV EBX,EAX and reused. pCharArray is
 * latched (MOV ESI,[0x53A45]+idx) before the input loop and read in the
 * Spell case (ESI is only clobbered inside the Attack branch).
 * ---------------------------------------------------------------- */
int fd2_player_inline_action_menu_dispatch(int char_idx,
    int32 *pSlot_disable_arr, int have_moved)
{
    int32 menu_template[4];
    uint8 drops_buffer[100];
    runtime_char *pCharArray;
    uint8 *pWeapon;
    uint8 weapon_id;
    uint32 weapon_slot;
    uint32 weapon_aoe_x;
    uint32 weapon_aoe_y;
    uint32 saved_cursor_x;
    uint32 saved_cursor_y;
    uint32 target_ids_buf;
    int n_targets;
    int sel;
    int target_idx;
    int input_result;
    uint32 drops_ptr;
    uint32 ap_divisor;

    menu_template[0] = data_fd2_ui_inline_action_menu_template[0];
    menu_template[1] = data_fd2_ui_inline_action_menu_template[1];
    menu_template[2] = data_fd2_ui_inline_action_menu_template[2];
    menu_template[3] = data_fd2_ui_inline_action_menu_template[3];

    pSlot_disable_arr[0] = 0;
    data_fd2_battle_pending_xp_credit = 0;

    weapon_slot = fd2_find_equipped_item_by_kind(char_idx, 0);
    if (weapon_slot == 0xffffffff) {
        pSlot_disable_arr[0] = 1;
    }
    else {
        weapon_id = fd2_get_inventory_slot_item_id(char_idx, weapon_slot);
        pWeapon = fd2_get_item_effect_entry(weapon_id);
        weapon_aoe_x = pWeapon[0xb];
        weapon_aoe_y = pWeapon[0xc];
        if (fd2_compute_aoe_targets(data_fd2_battle_cursor_world_x,
                data_fd2_battle_cursor_world_y, 0, weapon_aoe_y,
                weapon_aoe_x, 0) == 0) {
            pSlot_disable_arr[0] = 1;
        }
        fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
    }

    fd2_count_active_menu_items_until_zero(pSlot_disable_arr);
    fd2_open_settings_dialog_with_slide(menu_template, pSlot_disable_arr);

    if (fd2_count_usable_inventory_slots(char_idx) == 0) {
        pSlot_disable_arr[2] = 1;
    }

    if (fd2_build_usable_spell_list(char_idx, 0) == 0) {
        pSlot_disable_arr[1] = 1;
    }
    pCharArray = data_fd2_battle_runtime_char_array_ptr;
    if (pCharArray[char_idx].combat_aux_block[0] != 0) {
        pSlot_disable_arr[1] = 1;
    }

    fd2_count_active_menu_items_until_zero(pSlot_disable_arr);

    do {
        input_result =
            fd2_settings_menu_input_step(menu_template, pSlot_disable_arr);
    } while (input_result == 0);
    fd2_close_settings_dialog_with_slide(menu_template, pSlot_disable_arr);
    fd2_composite_battle_frame(0);

    saved_cursor_y = data_fd2_battle_cursor_world_y;
    saved_cursor_x = data_fd2_battle_cursor_world_x;
    if (input_result == -1) {
        return -1;
    }

    if (data_fd2_ui_menu_cursor_idx == 0) {
        target_ids_buf = (uint32)malloc(100);
        n_targets = fd2_compute_aoe_targets(data_fd2_battle_cursor_world_x,
            data_fd2_battle_cursor_world_y, target_ids_buf, weapon_aoe_y,
            weapon_aoe_x, 0);
        sel = fd2_wait_for_action_target_input(0, n_targets,
            (uint8 *)target_ids_buf);
        fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
        free((void *)target_ids_buf);
        if (sel == -1) {
            fd2_pan_cursor_to_tile_animated(saved_cursor_x, saved_cursor_y);
            return 0;
        }
        target_idx = fd2_find_char_at_cursor_pos();
        fd2_face_char_toward_target(char_idx, target_idx);
        fd2_play_full_combat_cinematic(char_idx, target_idx);
        fd2_clear_all_chars_facing();
        drops_ptr = fd2_collect_pending_death_drops((uint32)drops_buffer);
        fd2_play_death_animation_and_mark_dead();
        fd2_process_battle_drop_entries(char_idx, drops_ptr,
            (uint32)drops_buffer);
        fd2_mark_char_acted_this_turn(char_idx);
        fd2_clear_keyboard_buffer();
    }
    else if (data_fd2_ui_menu_cursor_idx == 1) {
        do {
            input_result = fd2_spell_selection_menu_main(char_idx);
        } while (input_result == 0);
        if (input_result == -1) {
            return 0;
        }
        fd2_mark_char_acted_this_turn(char_idx);
        ap_divisor = pCharArray[char_idx].status_flags_block[0];
        if (pCharArray[char_idx].job_id > 8) {
            ap_divisor = ap_divisor + 0x1e;
        }
        data_fd2_battle_pending_xp_credit =
            (uint32)((int)data_fd2_battle_pending_xp_credit / (int)ap_divisor);
    }
    else if (data_fd2_ui_menu_cursor_idx == 2) {
        do {
            input_result = fd2_item_command_menu_dispatch(char_idx);
        } while (input_result == 0);
        if (input_result == -1) {
            return 0;
        }
        data_fd2_battle_pending_xp_credit = 0;
    }
    else {
        if (have_moved == 0) {
            fd2_ai_pass_turn_with_heal(char_idx);
        }
        fd2_handle_tile_event_interaction(char_idx);
        fd2_mark_char_acted_this_turn(char_idx);
    }

    return 1;
}

/* ---- file-scope data owned by menu.c ---- */

/*
 * data_fd2_ui_click_debounce_skip_count @ 0x51A42 (.object2), 1 byte.
 * Click/keypress debounce counter for the Space/Enter action path in
 * fd2_game_main_loop: while >0 it is decremented (one skipped press per
 * frame); once it reaches 0 the spin-wait-for-chapter-init logic engages.
 * Mutable state with a writer (DEC byte ptr [0x51A42]); initial value 0x03.
 */
uint8  data_fd2_ui_click_debounce_skip_count = 3;

/*
 * data_fd2_battle_ai_post_action_consequence_idx @ 0x51A8F (.object2), 4 bytes.
 * Pending post-action consequence selector: an index into the 90-entry
 * data_fd2_battle_ai_post_action_consequence_table (whose slots are the
 * fd2_chapter_event_handler_NN__* chapter-event handlers). Set to 0xFF ("none")
 * before each actor finishes its action; fd2_check_tile_event_post_action stores
 * the event record's consequence byte here when the actor lands on a matching
 * event tile. After the action, callers (fd2_game_main_loop,
 * fd2_field_command_menu_loop, the battle AI turn phases, etc.) test it: if
 * != 0xFF they tail-call the indexed handler with the active char_idx, then
 * reset it to 0xFF.
 * Accessed as a full dword (MOV dword ptr [0x51A8F],EDX); the stored value
 * itself is an 8-bit id (MOVZX from a byte). Initial value 0xFF.
 */
uint32 data_fd2_battle_ai_post_action_consequence_idx = 0xFF;

/*
 * data_fd2_battle_current_active_char_idx @ 0x53AE9 (.object2), 4 bytes.
 * Index of the party slot the turn cursor advances from -- a runtime battle
 * state scalar, zero-initialized (all bytes 0 in the image). The engine writes
 * it before it is ever read: fd2_load_save_and_init_engine sets it to 0 at the
 * end of a LOAD GAME, and fd2_run_full_turn_cycle sets it to 0 when a new player
 * turn begins. fd2_game_main_loop's "next actor" path reads it as the starting
 * slot, then writes back (slot+1, wrapping to 0 at party_member_count).
 * Accessed as a full dword (MOV EBX,dword ptr [0x53AE9] / MOV [0x53AE9],EAX);
 * used as an unsigned index into data_fd2_battle_runtime_char_array_ptr.
 */
uint32 data_fd2_battle_current_active_char_idx;

/*
 * data_fd2_battle_player_action_result_code @ 0x53C53 (.object2), 4 bytes.
 * Player-turn action outcome flag, a runtime battle-state scalar that is
 * zero-initialized in the image (all bytes 0). The engine always writes it
 * before reading: fd2_player_action_menu_loop stores 0 at its entry
 * (MOV dword ptr [0x53C53],0x0), then later returns it as the function's
 * int result; fd2_item_command_menu_dispatch stores 1 on a committed GIVE.
 * Read back as a full dword (MOV EAX,dword ptr [0x53C53]) and compared
 * against 0 (CMP dword ptr [0x53C53],0x0) on several re-prompt paths.
 * 0 = "cancelled / no commit, re-prompt"; non-zero = "action committed".
 */
uint32 data_fd2_battle_player_action_result_code;
