/*
 * rndmenu.c — chapter-intro / transition menu overlay rendering
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_render_chapter_intro_overlay @ 0x2CF71  (1 caller)
 *
 * Renders the chapter-intro overlay (panel + chapter title text +
 * portrait icon) into the large game-state working buffer, then
 * commits the visible region to the VGA framebuffer. Used during the
 * chapter transition / pre-chapter intro screen.
 *
 * Sole caller: fd2_chapter_transition_menu @ 0x2CAD7.
 *
 * Sequence:
 *   1. Look up the current chapter's intro metadata record; its first
 *      byte (chapter category) drives the portrait pose-table index.
 *   2. memmove the battle-scene snapshot into the working surface.
 *   3. Blit the intro panel sprite at working-surface offset 0x1A20C.
 *   4. Render the chapter title text at offset 0x1ACC4 (FDTXT page
 *      = chapter_intro_menu_cursor_state + 0x1EF).
 *   5. Blit the speaker portrait icon. The animation frame index is
 *      remapped 3 -> 1; the destination is keyed off the per-chapter
 *      pose X (column) / Y (row) tables indexed by
 *      chapter_category * 6 + chapter_intro_menu_cursor_state.
 *   6. Commit the visible 312x192 region from working-surface +0x8088
 *      to the VGA primary at 0xA0504.
 *
 * void __cdecl, no params. EBX is callee-saved; the __CHK(0x34)
 * stack-probe prologue is compiler-injected and omitted here.
 * ---------------------------------------------------------------- */
/* ----------------------------------------------------------------
 * fd2_render_chapter_intro_dialog_panels @ 0x2D9FE  (1 caller)
 *
 * Render the chapter-intro dialog panels (background frame + animated
 * foreground), called per BIOS-tick from
 * fd2_wait_input_with_chapter_dialog_blink @ 0x2D85F. Three layout
 * modes are dispatched on `mode`.
 *
 * Sub-frame cycler (runs first, every call):
 *   if (frame_idx & 1) subframe_counter++;
 *   if (subframe_counter == 4) subframe_counter = 0;     // unconditional
 *
 * mode 0  — single-corner cursor indicator:
 *   blit corner_offs[cursor_idx]+0xAD430 with atlas sprite
 *   atlas[+6 + (frame_idx/2 + cursor_idx*2 + 3)*4].
 *
 * mode 1 / 3 — two scroll panels (+ party roster overlay when mode 3):
 *   if mode 3: fd2_render_party_roster_grid(cursor_idx, 0xA0000).
 *   left  @ 0xA972A : scroll==0 ? atlas[+0x4A] : atlas[+6+(frame_idx/2+0xB)*4]
 *   right @ 0xAE36A : (scroll+6 < visible_count) ? atlas[+6+(frame_idx/2+0xD)*4]
 *                                                 : atlas[+0x4A]
 *
 * mode 2  — same two panels (right cap uses scroll+3 instead of +6) plus a
 *   3-icon party roster row:
 *     icon_count = min(3, visible_count)
 *     anim_phase = (subframe_counter == 3) ? 1 : subframe_counter  // 0,1,2,3->0,1,2,1
 *     for i in 0..icon_count-1:
 *       portrait_id = candidate_array[scroll + i]
 *       fd2_tile_blit_24x24_with_dialog_bg_fill(
 *           cache + cache[portrait_id*0x30 + anim_phase*4],
 *           0xA000E + (0x75 + i*0x1A)*0x140, 0x140)
 *
 * void __cdecl. corner_offs_ptr points at a u32[4] of corner offsets.
 * EBX/ESI/EDI/EBP are callee-saved; the __CHK(0x20) stack-probe
 * prologue is compiler-injected and omitted here. frame_idx/2 is a
 * signed divide (matches the SAR idiom in the disassembly).
 * ---------------------------------------------------------------- */
void fd2_render_chapter_intro_dialog_panels(uint32 corner_offs_ptr, uint32 mode)
{
    int32  sprite_off;
    uint32 cursor_idx;
    uint32 atlas;
    uint32 icon_count;
    uint32 icon_iter;
    uint32 anim_phase;
    uint32 portrait_id;

    if ((data_fd2_chapter_intro_dialog_anim_frame_idx & 1) != 0) {
        data_fd2_chapter_intro_dialog_subframe_anim_counter++;
    }
    if (data_fd2_chapter_intro_dialog_subframe_anim_counter == 4) {
        data_fd2_chapter_intro_dialog_subframe_anim_counter = 0;
    }

    atlas = data_fd2_ui_menu_screen_sprite_atlas_buf_ptr;

    if (mode == 0) {
        cursor_idx = data_fd2_ui_menu_cursor_idx;
        sprite_off = *(int32 *)(atlas + 6
            + ((int32)data_fd2_chapter_intro_dialog_anim_frame_idx / 2
               + cursor_idx * 2 + 3) * 4);
        fd2_blit_sprite_with_stride_setup(
            *(int32 *)(cursor_idx * 4 + corner_offs_ptr) + 0xAD430,
            atlas + sprite_off,
            0x140);
    } else if (mode == 1 || mode == 3) {
        if (mode == 3) {
            fd2_render_party_roster_grid(data_fd2_ui_menu_cursor_idx, 0xA0000);
        }

        /* left panel @ 0xA972A */
        if (data_fd2_ui_menu_scroll_offset == 0) {
            sprite_off = *(int32 *)(atlas + 0x4A);
        } else {
            sprite_off = *(int32 *)(atlas + 6
                + ((int32)data_fd2_chapter_intro_dialog_anim_frame_idx / 2 + 0xB) * 4);
        }
        fd2_dialog_sprite_blit_normal(0xA972A, atlas + sprite_off, 0x140);

        /* right panel @ 0xAE36A */
        if ((int32)(data_fd2_ui_menu_scroll_offset + 6)
                < (int32)data_fd2_ui_menu_visible_item_count) {
            sprite_off = *(int32 *)(atlas + 6
                + ((int32)data_fd2_chapter_intro_dialog_anim_frame_idx / 2 + 0xD) * 4);
        } else {
            sprite_off = *(int32 *)(atlas + 0x4A);
        }
        fd2_dialog_sprite_blit_normal(0xAE36A, atlas + sprite_off, 0x140);
    } else if (mode == 2) {
        /* left panel @ 0xA972A */
        if (data_fd2_ui_menu_scroll_offset == 0) {
            sprite_off = *(int32 *)(atlas + 0x4A);
        } else {
            sprite_off = *(int32 *)(atlas + 6
                + ((int32)data_fd2_chapter_intro_dialog_anim_frame_idx / 2 + 0xB) * 4);
        }
        fd2_dialog_sprite_blit_normal(0xA972A, atlas + sprite_off, 0x140);

        /* right panel @ 0xAE36A (cap uses scroll+3) */
        if ((int32)(data_fd2_ui_menu_scroll_offset + 3)
                < (int32)data_fd2_ui_menu_visible_item_count) {
            sprite_off = *(int32 *)(atlas + 6
                + ((int32)data_fd2_chapter_intro_dialog_anim_frame_idx / 2 + 0xD) * 4);
        } else {
            sprite_off = *(int32 *)(atlas + 0x4A);
        }
        fd2_dialog_sprite_blit_normal(0xAE36A, atlas + sprite_off, 0x140);

        /* 3-icon party roster row */
        icon_count = data_fd2_ui_menu_visible_item_count;
        if ((int32)data_fd2_ui_menu_visible_item_count > 3) {
            icon_count = 3;
        }
        anim_phase = data_fd2_chapter_intro_dialog_subframe_anim_counter;
        if (data_fd2_chapter_intro_dialog_subframe_anim_counter == 3) {
            anim_phase = 1;
        }
        for (icon_iter = 0; (int32)icon_iter < (int32)icon_count; icon_iter++) {
            portrait_id = (uint32)data_fd2_ui_menu_candidate_array_ptr[
                data_fd2_ui_menu_scroll_offset + icon_iter];
            fd2_tile_blit_24x24_with_dialog_bg_fill(
                *(int32 *)(portrait_sprite_cache
                           + portrait_id * 0x30 + anim_phase * 4)
                    + portrait_sprite_cache,
                (icon_iter * 0x1A + 0x75) * 0x140 + 0xA000E,
                0x140);
        }
    }
}

void fd2_render_chapter_intro_overlay(void)
{
    uint8 *chapter_meta;
    uint32 frame_idx;
    uint32 table_off;
    uint8  chapter_meta_byte;

    chapter_meta = fd2_get_chapter_intro_metadata_entry(
                       data_fd2_chapter_current_chapter_id);
    chapter_meta_byte = *chapter_meta;

    memmove((void *)data_fd2_large_game_state_buffer_ptr,
            (void *)battle_scene_snapshot, 0x25680);

    fd2_dialog_sprite_blit_normal(data_fd2_large_game_state_buffer_ptr + 0x1a20c,
                                  data_fd2_chapter_intro_menu_overlay_buf_ptr,
                                  0x1c8);

    fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                             data_fd2_chapter_intro_menu_cursor_state + 0x1ef,
                             data_fd2_large_game_state_buffer_ptr + 0x1acc4,
                             0x1c8, 0xcd, 0x4c, 0x4a, 0x13, 0);

    frame_idx = data_fd2_chapter_intro_dialog_anim_frame_idx;
    if (data_fd2_chapter_intro_dialog_anim_frame_idx == 3) {
        frame_idx = 1;
    }

    table_off = (uint32)chapter_meta_byte * 6
              + data_fd2_chapter_intro_menu_cursor_state;

    fd2_tile_blit_24x24_passthrough(
        *(int32 *)(portrait_sprite_cache + frame_idx * 4) + portrait_sprite_cache,
        data_fd2_large_game_state_buffer_ptr
            + (uint32)data_fd2_chapter_intro_portrait_pose_x_column_table[table_off] * 0x1c8
            + data_fd2_chapter_intro_portrait_pose_y_row_table[table_off] + 0x8088,
        0x1c8);

    fd2_blit_rectangle(0xa0504, 0x140,
                       data_fd2_large_game_state_buffer_ptr + 0x8088,
                       0x1c8, 0x138, 0xc0);
}

/* ----------------------------------------------------------------
 * fd2_render_shop_item_grid @ 0x2DC55  (2 callers)
 *
 * Render the SHOP item grid — up to 6 visible items in a 2-column ×
 * 3-row layout, with item name, category icon, primary stat
 * (AP/DP/HP/MP) and price (full, or 3/4-discounted in sell mode).
 *
 * Callers: fd2_open_shop_dialog_panel @ 0x2E0BD and
 * fd2_shop_menu_input_loop @ 0x2DF6B (the buy / sell / give / equip
 * shop-style flows).
 *
 * Visible-count cap:
 *   draw_count = item_count
 *   if (item_count > 6) { draw_count = 6;
 *       if (item_count < scroll_offset + 6) draw_count = 5; }   // tail-clamp
 *
 * Per item (iter = 0..draw_count-1):
 *   item_id    = item_id_array[scroll_offset + iter]
 *   item_entry = fd2_get_item_effect_entry(item_id)
 *   col_x      = (iter % 2) * 0x94 + 10                          // L/R column
 *   row_off    = (iter / 2) * 0x1A                               // row spacing
 *   kind       = item_entry[+0]
 *   category icon = kind<0x15 ? 0x3B (weapon) : kind<0x20 ? 0x3C (armor)
 *                                                          : 0x3D (other)
 *   border glyph  = (scroll_offset + iter == highlight_slot) ? 0xC9 : 0xCD
 *   name = FDTXT dialog scene, page = item_id + 0xB5
 *   primary stat icon + 3-digit value:
 *     kind<0x15            -> AP icon 0x40, value = entry[+1] (int16)
 *     kind<0x20            -> DP icon 0x41, value = entry[+5] (int16)
 *     kind==0x20,[+0xD]==5 -> HP icon 0x42, value = entry[+0xE] (int16)
 *     kind==0x20,[+0xD]==B -> MP icon 0x43, value = entry[+0xE] (int16)
 *     else                 -> "—" placeholder sprite 0x29, no number
 *   price (5-digit): coin icon 0x0F (from menu atlas), price = entry[+0x13]
 *     if sell_mode_flag: price = price * 3 / 4                    // 75%
 *
 * void __cdecl. EBX/ESI/EDI/EBP callee-saved; the __CHK(0x54)
 * stack-probe prologue is compiler-injected and omitted here.
 * The (price*3)/4 sell discount is a signed divide-by-4; price is a
 * zero-extended uint16 (always positive) so >>2 == /4.
 * ---------------------------------------------------------------- */
void fd2_render_shop_item_grid(uint32 item_count, uint8 *item_id_array,
                               uint32 highlight_slot, int32 surface_offset,
                               int32 sell_mode_flag)
{
    uint32 draw_count;
    uint32 iter;
    uint32 item_id;
    uint8 *item_entry;
    uint8  kind;
    int32  col_x;
    uint32 row_off;
    uint32 category_icon;
    uint8  border_glyph;
    int32  name_x;
    uint32 primary_x;
    uint8 *primary_y_dst;
    int32  stat_value;
    uint32 price;

    draw_count = item_count;
    if (((int32)item_count > 6)
        && (draw_count = 6,
            (int32)item_count < (int32)data_fd2_ui_menu_scroll_offset + 6)) {
        draw_count = 5;
    }

    for (iter = 0; (int32)iter < (int32)draw_count; iter++) {
        item_id = (uint32)item_id_array[data_fd2_ui_menu_scroll_offset + iter];
        item_entry = fd2_get_item_effect_entry(item_id);
        col_x = ((int32)iter % 2) * 0x94 + 10;
        row_off = ((int32)iter / 2) * 0x1a;

        kind = *item_entry;
        if (kind < 0x15) {
            category_icon = 0x3b;
        } else if (kind < 0x20) {
            category_icon = 0x3c;
        } else {
            category_icon = 0x3d;
        }
        fd2_blit_sheet_sprite_at_offset(
            (row_off + 0x77) * 0x140 + surface_offset + col_x,
            0x140, data_fd2_ui_anim_sprite_sheet_ptr, category_icon);

        border_glyph = 0xcd;
        if (data_fd2_ui_menu_scroll_offset + iter == highlight_slot) {
            border_glyph = 0xc9;
        }
        name_x = surface_offset + col_x;
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr, item_id + 0xb5,
            name_x + 0x1c + (row_off + 0x7a) * 0x140,
            0x140, border_glyph, 0x4c, 0, 0, 0);

        kind = *item_entry;
        primary_y_dst = (uint8 *)(name_x + 0x76 + (row_off + 0x79) * 0x140);
        primary_x = (row_off + 0x79) * 0x140 + name_x + 0x5f;
        if (kind < 0x15) {
            fd2_blit_sheet_sprite_at_offset(
                primary_x, 0x140, data_fd2_ui_anim_sprite_sheet_ptr, 0x40);
            stat_value = *(int16 *)(item_entry + 1);
            fd2_render_decimal_number_to_buffer(
                (uint32)primary_y_dst, 0x140, stat_value, 0x2a, 3);
        } else if (kind < 0x20) {
            fd2_blit_sheet_sprite_at_offset(
                primary_x, 0x140, data_fd2_ui_anim_sprite_sheet_ptr, 0x41);
            stat_value = *(int16 *)(item_entry + 5);
            fd2_render_decimal_number_to_buffer(
                (uint32)primary_y_dst, 0x140, stat_value, 0x2a, 3);
        } else if (kind == 0x20 && item_entry[0xd] == 0x05) {
            fd2_blit_sheet_sprite_at_offset(
                primary_x, 0x140, data_fd2_ui_anim_sprite_sheet_ptr, 0x42);
            stat_value = *(int16 *)(item_entry + 0xe);
            fd2_render_decimal_number_to_buffer(
                (uint32)primary_y_dst, 0x140, stat_value, 0x2a, 3);
        } else if (*item_entry == 0x20 && item_entry[0xd] == 0x0b) {
            fd2_blit_sheet_sprite_at_offset(
                primary_x, 0x140, data_fd2_ui_anim_sprite_sheet_ptr, 0x43);
            stat_value = *(int16 *)(item_entry + 0xe);
            fd2_render_decimal_number_to_buffer(
                (uint32)primary_y_dst, 0x140, stat_value, 0x2a, 3);
        } else {
            fd2_blit_indexed_sprite_at_xy(
                (row_off + 0x7b) * 0x140 + surface_offset + col_x + 0x5f,
                0x140, data_fd2_ui_anim_sprite_sheet_ptr, 0x29);
        }

        fd2_blit_sheet_sprite_at_offset(
            (row_off + 0x83) * 0x140 + surface_offset + col_x + 0x5f,
            0x140, data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0xf);
        price = (uint32)*(uint16 *)(item_entry + 0x13);
        if ((sell_mode_flag & 0xff) != 0) {
            price = (int32)(price * 3) >> 2;
        }
        fd2_render_decimal_number_to_buffer(
            col_x + surface_offset + 0x68 + (row_off + 0x83) * 0x140,
            0x140, price, 0x77, 5);
    }
}
