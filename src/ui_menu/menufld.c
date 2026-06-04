/*
 * menufld.c — Field-map tile interaction handlers (treasure / gold / event tiles)
 *             + the field command-menu Save/Load/Quit dispatcher.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_handle_tile_event_interaction @ 0x190AC  (1 caller: fd2_player_inline_action_menu_dispatch)
 *
 * Field-map treasure / gold / event-tile interaction handler. Invoked
 * when the player confirms on a cursor tile whose terrain attribute has
 * an event bit (0x20 / 0x60) set and which has not yet been consumed.
 *
 *  1. Read the tile attribute at the cursor world position -> tile_y_idx,
 *     tile_attr byte.
 *  2. Gate: (tile_attr & 0x60)==0 -> return; consumed[tile_y_idx]!=0 -> return.
 *  3. Clear keyboard, load the actor's chapter portrait.
 *  4. Opening dialog: bit 0x20 -> text 0x1A5, else 0x1AC.
 *  5. Paint portrait, run typewriter loop; YES = (ret==1 && cursor_idx==0).
 *  6. YES: play SFX, read tile_event_data_table[tile_y_idx] 3-byte entry
 *     (type, value word). type 0 = ITEM (add to inventory, full-bag swap
 *     UI), type 1 = GOLD (add amount to party gold), type other = EVENT
 *     (chapter-scripted post-action dispatch via the consequence table,
 *     indexed by the value word). Tick tile animations afterward.
 *  7. NO: cancel dialog 0x19C, close.
 * ---------------------------------------------------------------- */
void fd2_handle_tile_event_interaction(uint32 char_idx)
{
    uint8   attr_scratch[20];       /* tile-attribute out buffer: +2 row, +4 attr */
    uint8   tile_attr_byte;
    uint32  tile_y_idx;
    uint32  tile_attr;
    uint32  dialog_text_id;
    int     typewriter_result;
    uint8  *p_event_entry;
    uint8   event_type;
    uint32  event_value;
    int     add_result;
    int     swap_result;
    uint8   swapped_out_id;

    fd2_read_tile_attribute_at_pos(data_fd2_battle_cursor_world_x,
        data_fd2_battle_cursor_world_y, (uint32)attr_scratch);
    tile_y_idx = (uint32)(int)*(int16 *)(attr_scratch + 2);
    tile_attr_byte = attr_scratch[4];
    tile_attr = (uint32)tile_attr_byte;
    if ((tile_attr_byte & 0x60) == 0)
        return;
    if (*(int8 *)(tile_y_idx + data_fd2_field_map_tile_event_consumed_flags_ptr)
            != 0)
        return;

    fd2_clear_keyboard_buffer();
    fd2_load_chapter_portrait(
        (uint32)data_fd2_battle_runtime_char_array_ptr[char_idx].portrait_id);

    if ((tile_attr & 0x20) == 0)
        dialog_text_id = 0x1ac;
    else
        dialog_text_id = 0x1a5;
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, dialog_text_id,
        0xa9f23, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);
    typewriter_result = fd2_text_dialog_typewriter_loop();
    fd2_animate_dialog_page_advance_collapse();

    if ((typewriter_result != 1) || (data_fd2_ui_menu_cursor_idx != 0)) {
        /* NO branch */
        __delay_thunk_375b2(100);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19c,
            0xab6e3, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        __delay_thunk_375b2(200);
        fd2_close_status_screen_with_slide_out();
        return;
    }

    /* YES branch */
    fd2_play_sfx_sample_from_bank(data_fd2_audio_fdother_sfx_bank_buf_ptr,
        0xc, 1);
    __delay_thunk_375b2(300);
    p_event_entry = (uint8 *)(data_fd2_tile_event_data_table_ptr
                  + tile_y_idx * 3);
    event_value = (uint32)*(uint16 *)(p_event_entry + 0x54);
    event_type = p_event_entry[0x53];

    if (event_type == 0) {
        /* ITEM */
        data_fd2_dialog_last_action_sprite_id_param = event_value + 0xb5;
        if ((tile_attr & 0x20) == 0)
            dialog_text_id = 0x1ad;
        else
            dialog_text_id = 0x1a6;
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, dialog_text_id,
            0xab6e3, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        add_result = fd2_add_item_to_inventory(char_idx, event_value);
        if (add_result == -1) {
            /* inventory full -> swap UI */
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(0);
            fd2_close_status_screen_with_slide_out();
            __delay_thunk_375b2(100);
            fd2_load_chapter_portrait((uint32)
                data_fd2_battle_runtime_char_array_ptr[char_idx].portrait_id);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a7,
                0xa9f23, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            typewriter_result = fd2_text_dialog_typewriter_loop();
            fd2_animate_dialog_page_advance_collapse();
            if ((typewriter_result == 1) && (data_fd2_ui_menu_cursor_idx == 0)) {
                fd2_close_status_screen_with_slide_out();
                swap_result = fd2_inventory_selection_modal_dispatch(char_idx, 0);
                if (swap_result != 0) {
                    swapped_out_id = fd2_get_inventory_slot_item_id(char_idx,
                        data_fd2_ui_menu_cursor_idx);
                    fd2_remove_inventory_slot_at(char_idx,
                        data_fd2_ui_menu_cursor_idx);
                    fd2_add_item_to_inventory(char_idx, event_value);
                    *(int16 *)(tile_y_idx * 3
                        + data_fd2_tile_event_data_table_ptr + 0x54) =
                        (int16)swapped_out_id;
                    __delay_thunk_375b2(100);
                    fd2_load_chapter_portrait((uint32)
                        data_fd2_battle_runtime_char_array_ptr[char_idx]
                            .portrait_id);
                    data_fd2_dialog_drop_swap_text_id_param =
                        (uint32)swapped_out_id + 0xb5;
                    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a9,
                        0xa9f23, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
                    fd2_paint_portrait_to_dialog_area(0);
                    fd2_wait_for_input_dialog_with_blink(0);
                    fd2_close_status_screen_with_slide_out();
                    return;
                }
                __delay_thunk_375b2(100);
                fd2_load_chapter_portrait((uint32)
                    data_fd2_battle_runtime_char_array_ptr[char_idx]
                        .portrait_id);
                fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a8,
                    0xa9f23, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            } else {
                fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a8,
                    0xab6e3, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            }
            __delay_thunk_375b2(200);
            fd2_close_status_screen_with_slide_out();
            return;
        }
        /* inventory not full */
        *(uint8 *)(tile_y_idx + data_fd2_field_map_tile_event_consumed_flags_ptr)
            = 1;
        fd2_paint_portrait_to_dialog_area(0);
        fd2_wait_for_input_dialog_with_blink(0);
        fd2_close_status_screen_with_slide_out();
        fd2_tick_tile_event_animations();
        return;
    }

    if (event_type != 1) {
        /* EVENT: chapter-scripted post-action dispatch */
        __delay_thunk_375b2(200);
        fd2_close_status_screen_with_slide_out();
        data_fd2_battle_ai_post_action_consequence_table[event_value](char_idx);
        return;
    }

    /* GOLD (event_type == 1) */
    if (event_value == 0) {
        if ((tile_attr & 0x20) == 0)
            dialog_text_id = 0x1af;
        else
            dialog_text_id = 0x1ab;
    } else {
        data_fd2_dialog_last_action_value_param = event_value;
        if ((tile_attr & 0x20) == 0)
            dialog_text_id = 0x1ae;
        else
            dialog_text_id = 0x1aa;
    }
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, dialog_text_id,
        0xab6e3, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);
    fd2_wait_for_input_dialog_with_blink(0);
    fd2_close_status_screen_with_slide_out();
    data_fd2_shared_party_total_gold =
        data_fd2_shared_party_total_gold + data_fd2_dialog_last_action_value_param;
    *(uint8 *)(tile_y_idx + data_fd2_field_map_tile_event_consumed_flags_ptr) = 1;
    fd2_tick_tile_event_animations();
}

/* ----------------------------------------------------------------
 * fd2_field_menu_status_save_load_quit_dispatch @ 0x19DF7
 *   (1 caller: fd2_field_command_menu_loop, cursor-0 branch @ 0x16FED)
 *
 * Field command-menu sub-dispatcher: pops up a 4-option cross-shape window
 * (Status / Save / Load / Quit) and dispatches the chosen option.
 *
 * Setup:
 *   menu_options[0..3] = data_fd2_ui_save_load_newgame_menu_template (0x51EF5)
 *   menu_state[0..3]   = data_fd2_ui_save_load_menu_state_template    (0x53F22, all 0)
 *   Probe FD2.SAV ("rb"): if it does NOT open, gray the Load option
 *     (menu_state[2] = 1); if it does open, malloc+fread+free (probe only).
 *   Scan the party: any char with (flags & 1)==0 && (flags & 0x80)!=0 grays
 *     the Save option (menu_state[1] = 1) — a unit acted but is still alive,
 *     so the field turn is mid-flight and saving is disallowed.
 *   Open the slide-in dialog, poll fd2_settings_menu_input_step until non-zero,
 *     close the dialog, recomposite. input_result == -1 (Esc) -> return 0.
 *
 * Dispatch on data_fd2_ui_menu_cursor_idx:
 *   0 STATUS: fd2_open_party_status_overview_screen(); return 1.
 *   1 SAVE  : portrait 0x4B, dialog 0x19A prompt, typewriter. On YES
 *             (typewriter==1 && cursor==0): assemble the FD2.SAV snapshot into a
 *             0x59CB buffer (probe-read the old file or stamp empty-slot markers,
 *             then copy live engine state at the fixed offsets), checksum @ +0x59C7,
 *             encrypt, write FD2.SAV ("wb"); dialog 0x19B "saved". On NO or the
 *             gate failing: dialog 0x19C "cancelled". Then return 1.
 *   2 LOAD  : portrait 0x4B, dialog 0x19D prompt, typewriter. On YES: dialog 0x19E
 *             "loading", fade BGM off, fd2_load_save_and_init_engine(); else
 *             dialog 0x19C. Then return 1.
 *   3 QUIT  : portrait 0x4B, dialog 0x19F prompt, typewriter. On YES: dialog 0x1A0
 *             "quitting", fade BGM off, close -> return -1. On NO: dialog 0x19C,
 *             close -> return -1.
 *
 * The body tail-jumps into fd2_field_command_menu_loop's epilogue (JMP 0x16FDD /
 * 0x16FD8); the recovered return values (0 / 1 / -1) are emitted directly.
 *
 * Returns: 0 = Esc-cancelled selection; 1 = Status/Save/Load done; -1 = Quit.
 *
 * int __cdecl with the __CHK(0x54) stack-probe prologue.
 * ---------------------------------------------------------------- */
int fd2_field_menu_status_save_load_quit_dispatch(void)
{
    int32   menu_options[4];
    int32   menu_state[4];
    int     input_result;
    int     i;
    FILE   *probe_fp;
    void   *probe_buf;
    uint8  *save_buf;
    FILE   *save_fp;
    int     typewriter_result;
    uint32  checksum;
    uint32  result_text_id;

    menu_options[0] = data_fd2_ui_save_load_newgame_menu_template[0];
    menu_options[1] = data_fd2_ui_save_load_newgame_menu_template[1];
    menu_options[2] = data_fd2_ui_save_load_newgame_menu_template[2];
    menu_options[3] = data_fd2_ui_save_load_newgame_menu_template[3];

    menu_state[0] = data_fd2_ui_save_load_menu_state_template[0];
    menu_state[1] = data_fd2_ui_save_load_menu_state_template[1];
    menu_state[2] = data_fd2_ui_save_load_menu_state_template[2];
    menu_state[3] = data_fd2_ui_save_load_menu_state_template[3];

    probe_fp = fopen("FD2.SAV", "rb");
    if (probe_fp == NULL) {
        menu_state[2] = 1;                  /* no save file -> gray Load */
    } else {
        probe_buf = malloc(0x59CB);
        fread(probe_buf, 1, 0x59CB, probe_fp);
        fclose(probe_fp);
        free(probe_buf);
    }

    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        if ((data_fd2_battle_runtime_char_array_ptr[i].flags & 1) == 0 &&
            (data_fd2_battle_runtime_char_array_ptr[i].flags & 0x80) != 0) {
            menu_state[1] = 1;              /* turn mid-flight -> gray Save */
        }
    }

    fd2_open_settings_dialog_with_slide(menu_options, menu_state);
    do {
        input_result = fd2_settings_menu_input_step(menu_options, menu_state);
    } while (input_result == 0);
    fd2_close_settings_dialog_with_slide(menu_options, menu_state);
    fd2_composite_battle_frame(0);

    if (input_result == -1) {
        return 0;
    }

    if (data_fd2_ui_menu_cursor_idx == 0) {
        fd2_open_party_status_overview_screen();
        return 1;
    }

    if (data_fd2_ui_menu_cursor_idx == 1) {
        /* SAVE */
        fd2_load_chapter_portrait(0x4B);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19A,
            0xA9F23, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        typewriter_result = fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();

        if (typewriter_result != 1 || data_fd2_ui_menu_cursor_idx != 0) {
            result_text_id = 0x19C;        /* cancelled */
        } else {
            save_buf = (uint8 *)malloc(0x59CB);
            probe_fp = fopen("FD2.SAV", "rb");
            if (probe_fp == NULL) {
                save_buf[0x3B2B] = 0xFF;   /* empty-slot markers */
                save_buf[0x4553] = 0xFF;
                save_buf[0x4F7B] = 0xFF;
                save_buf[0x59A3] = 0xFF;
            } else {
                fread(save_buf, 1, 0x59CB, probe_fp);
                fd2_save_crypt_buffer((uint32)save_buf, 0x59CB);
                fclose(probe_fp);
            }

            memmove(save_buf,
                (void *)data_fd2_tile_event_data_table_ptr, 0x8A3);
            memmove(save_buf + 0x8A3,
                (void *)data_fd2_shared_menu_party_roster_buffer_ptr, 0xA00);
            memmove(save_buf + 0x12A3,
                (void *)data_fd2_battle_runtime_char_array_ptr,
                data_fd2_battle_party_member_count * 0x50);
            memmove(save_buf + 0x30A3,
                (void *)data_fd2_field_map_tile_event_consumed_flags_ptr, 0x20);

            save_buf[0x30C3] = (uint8)data_fd2_battle_turn_counter;
            save_buf[0x30C4] = (uint8)data_fd2_battle_party_member_count;
            save_buf[0x30C5] = (uint8)data_fd2_chapter_current_chapter_id;
            save_buf[0x30C6] = (uint8)data_fd2_battle_view_window_origin_x;
            save_buf[0x30C7] = (uint8)data_fd2_battle_view_window_origin_y;
            save_buf[0x30C8] = (uint8)data_fd2_battle_cursor_world_x;
            save_buf[0x30C9] = (uint8)data_fd2_battle_cursor_world_y;
            save_buf[0x30CA] = (uint8)data_fd2_battle_cursor_screen_x;
            save_buf[0x30CB] = (uint8)data_fd2_battle_cursor_screen_y;
            save_buf[0x30CC] = (uint8)data_fd2_shared_menu_party_member_count;
            *(uint32 *)(save_buf + 0x30CD) = data_fd2_shared_party_total_gold;
            save_buf[0x30D1] = data_fd2_ui_game_speed_flag;
            save_buf[0x30D2] = data_fd2_ui_terrain_hud_user_enabled;
            save_buf[0x30D3] = data_fd2_audio_bgm_enabled_flag;
            save_buf[0x30D4] = data_fd2_audio_sfx_enabled_flag;

            save_fp = fopen("FD2.SAV", "wb");
            checksum = fd2_save_compute_checksum((uint32)save_buf, 0x59CB);
            *(uint32 *)(save_buf + 0x59C7) = checksum;
            fd2_save_crypt_buffer((uint32)save_buf, 0x59CB);
            fwrite(save_buf, 1, 0x59CB, save_fp);
            fclose(save_fp);
            free(save_buf);
            result_text_id = 0x19B;        /* saved */
        }
    } else if (data_fd2_ui_menu_cursor_idx == 2) {
        /* LOAD */
        fd2_load_chapter_portrait(0x4B);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19D,
            0xA9F23, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        typewriter_result = fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();

        if (typewriter_result == 1 && data_fd2_ui_menu_cursor_idx == 0) {
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19E,
                0xAB6E3, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
            __delay_thunk_375b2(200);
            fd2_close_status_screen_with_slide_out();
            fd2_set_bgm_track_with_fade(0xFFFFFFFF, 0);
            fd2_load_save_and_init_engine();
            fd2_clear_keyboard_buffer();
            return 1;
        }
        result_text_id = 0x19C;            /* cancelled */
    } else {
        /* QUIT */
        fd2_load_chapter_portrait(0x4B);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19F,
            0xA9F23, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        typewriter_result = fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();

        if (typewriter_result == 1 && data_fd2_ui_menu_cursor_idx == 0) {
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1A0,
                0xAB6E3, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
            fd2_set_bgm_track_with_fade(0xFFFFFFFF, 1);
            __delay_thunk_375b2(200);
            fd2_close_status_screen_with_slide_out();
            return -1;
        }
        /* QUIT-NO tail-jumps into the caller's shared "cancelled" epilogue
         * (0x1716F): dialog 0x19C, delay, close, return 1 — and, unlike the
         * Save/Load cancel tail, WITHOUT the trailing clear-keyboard call. */
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19C,
            0xAB6E3, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        __delay_thunk_375b2(200);
        fd2_close_status_screen_with_slide_out();
        return 1;
    }

    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, result_text_id,
        0xAB6E3, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
    __delay_thunk_375b2(200);
    fd2_close_status_screen_with_slide_out();
    fd2_clear_keyboard_buffer();
    return 1;
}
