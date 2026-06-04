/*
 * menufld.c — Field-map tile interaction handlers (treasure / gold / event tiles)
 */

#include <stdlib.h>
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
