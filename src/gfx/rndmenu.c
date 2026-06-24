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
 *   5. Blit the chapter-intro character portrait icon. The animation
 *      frame index is remapped 3 -> 1; the destination is keyed off two
 *      per-chapter pose-byte tables indexed by
 *      chapter_category * 6 + chapter_intro_menu_cursor_state. One table
 *      (data_fd2_chapter_intro_portrait_pose_x_column_table) is multiplied
 *      by the 0x1C8 row pitch -> the row (Y) contribution; the other
 *      (data_fd2_chapter_intro_portrait_pose_y_row_table) is added
 *      directly -> the within-row (X) contribution. NOTE the two global
 *      names read inverted vs this behaviour (see issues).
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
 * mode 0  -- single-corner cursor indicator:
 *   blit corner_offs[cursor_idx]+0xAD430 with atlas sprite
 *   atlas[+6 + (frame_idx/2 + cursor_idx*2 + 3)*4].
 *
 * mode 1 / 3 -- two scroll panels (+ party roster overlay when mode 3):
 *   if mode 3: fd2_render_party_roster_grid(cursor_idx, 0xA0000).
 *   left  @ 0xA972A : scroll==0 ? atlas[+0x4A] : atlas[+6+(frame_idx/2+0xB)*4]
 *   right @ 0xAE36A : (scroll+6 < visible_count) ? atlas[+6+(frame_idx/2+0xD)*4]
 *                                                 : atlas[+0x4A]
 *
 * mode 2  -- same two panels (right cap uses scroll+3 instead of +6) plus a
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
                *(int32 *)(data_fd2_portrait_sprite_cache
                           + portrait_id * 0x30 + anim_phase * 4)
                    + data_fd2_portrait_sprite_cache,
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
            (void *)data_fd2_battle_scene_snapshot, 0x25680);

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
        *(int32 *)(data_fd2_portrait_sprite_cache + frame_idx * 4) + data_fd2_portrait_sprite_cache,
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
            fd2_blit_indexed_sprite_rle(
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

/* ----------------------------------------------------------------
 * fd2_render_party_roster_grid @ 0x2EA90  (2 callers)
 *
 * Render the party-roster grid (used by all 5 party-roster select
 * menus) — up to 6 visible chars in a 2-column x 3-row layout, each
 * showing portrait + class/job name + selection highlight.
 *
 * Callers: fd2_party_roster_single_select_loop @ 0x2E6B8 and
 * fd2_render_chapter_intro_dialog_panels @ 0x2D9FE (mode 3).
 *
 * Blink-frame mapping:
 *   blink_frame = (subframe_counter == 3) ? 1 : counter   // 0,1,2,3->0,1,2,1
 *
 * Visible-count cap:
 *   draw_count = menu_party_member_count
 *   if (member_count > 6) { draw_count = 6;
 *       if (member_count < scroll_offset + 6) draw_count = 5; }   // tail-clamp
 *
 * Per char (iter = 0..draw_count-1):
 *   char_idx = scroll_offset + iter
 *   col_off  = (iter % 2) * 0x84                          // 2 columns, 0x84 apart
 *   row_off  = (iter / 2) * 0x1A                          // 3 rows, 0x1A apart
 *   24x24 portrait blit with blink variant:
 *     portrait_ptr = cache + cache[char_idx*0x30 + blink_frame*4]
 *     fd2_tile_blit_24x24_with_dialog_bg_fill(portrait_ptr,
 *         surface_offset + 0x0E + col_off + (row_off + 0x75)*0x140, 0x140)
 *   border_glyph = (char_idx == highlight_idx) ? 0xC9 : 0xCD
 *   class/job name via dialog scene (FDTXT idx = char.char_id + 1):
 *     fd2_display_dialog_scene(all_game_text,
 *         rt_chars[char_idx].char_id + 1,
 *         surface_offset + 0x28 + col_off + (row_off + 0x79)*0x140,
 *         0x140, border_glyph, 0x4C, 0, 0, 0)
 *
 * void __cdecl. EBX/ESI/EDI/EBP callee-saved; the __CHK(0x44)
 * stack-probe prologue is compiler-injected and omitted here. iter/2
 * is a signed divide (matches the SAR idiom in the disassembly).
 * ---------------------------------------------------------------- */
void fd2_render_party_roster_grid(uint32 highlight_idx, uint32 surface_offset)
{
    uint32 blink_frame;
    uint32 draw_count;
    uint32 iter;
    uint32 char_idx;
    uint32 col_off;
    uint32 row_off;
    uint8  border_glyph;
    runtime_char *rt_chars;

    blink_frame = data_fd2_chapter_intro_dialog_subframe_anim_counter;
    if (data_fd2_chapter_intro_dialog_subframe_anim_counter == 3) {
        blink_frame = 1;
    }

    draw_count = data_fd2_shared_menu_party_member_count;
    if (((int32)data_fd2_shared_menu_party_member_count > 6)
        && (draw_count = 6,
            (int32)data_fd2_shared_menu_party_member_count
                < (int32)data_fd2_ui_menu_scroll_offset + 6)) {
        draw_count = 5;
    }

    for (iter = 0; (rt_chars = data_fd2_battle_runtime_char_array_ptr,
                    (int32)iter < (int32)draw_count); iter++) {
        char_idx = data_fd2_ui_menu_scroll_offset + iter;
        col_off = ((int32)iter % 2) * 0x84;
        row_off = ((int32)iter / 2) * 0x1a;

        fd2_tile_blit_24x24_with_dialog_bg_fill(
            *(int32 *)(data_fd2_portrait_sprite_cache
                       + char_idx * 0x30 + blink_frame * 4)
                + data_fd2_portrait_sprite_cache,
            (row_off + 0x75) * 0x140 + surface_offset + 0xe + col_off,
            0x140);

        border_glyph = 0xcd;
        if (data_fd2_ui_menu_scroll_offset + iter == highlight_idx) {
            border_glyph = 0xc9;
        }
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            rt_chars[char_idx].char_id + 1,
            (row_off + 0x79) * 0x140 + surface_offset + 0x28 + col_off,
            0x140, border_glyph, 0x4c, 0, 0, 0);
    }
}

/* ----------------------------------------------------------------
 * fd2_render_party_roster_with_item_stat_preview @ 0x2EBE0  (1 caller)
 *
 * Render the class-filtered party-roster grid with an item-stat
 * PREVIEW — up to 3 visible chars in a single column, each showing the
 * current AP/DP/DX/Stat4 stat block side-by-side with the stats they
 * would have if they equipped the candidate item. Each preview value is
 * colour-coded against the current value via fd2_pick_stat_compare_color.
 *
 * Sole caller: fd2_party_roster_class_select_loop @ 0x2E8CF (the buy-item
 * "give the bought gear to someone" stat-preview flow).
 *
 * Blink-frame mapping (portrait blink cycler):
 *   blink_frame = (subframe_counter == 3) ? 1 : counter
 *
 * Visible-count cap: draw_count = min(candidate_count, 3).
 *
 * Per char (iter = 0..draw_count-1):
 *   char_idx = candidate_array[scroll_offset + iter]
 *   preview  = fd2_compute_equipped_stats_with_item_preview(char_idx,
 *                  item_id, &preview)            // [AP, DP, DX, Stat4]
 *   row_y    = iter * 0x1A + 0x75                 // 3-row spacing
 *   24x24 portrait bg-fill blit (blink variant):
 *     src = cache + cache[char_idx*0x30 + blink_frame*4]
 *     dst = row_y*0x140 + surface_offset + 0x0E
 *   border_glyph = (scroll_offset + iter == highlight_idx) ? 0xC9 : 0xCD
 *   char name via dialog scene (FDTXT page = char.char_id + 1):
 *     dst = (row_y+4)*0x140 + surface_offset + 0x28
 *   four stat pairs (current value + preview value, sharing one compare
 *   colour each), with a unit-icon sprite blit per value:
 *     AP    : current icon atlas[+0x4E], preview icon atlas[+0x5E]
 *     DP    : current icon atlas[+0x52], preview icon atlas[+0x5E]
 *     DX    : current icon atlas[+0x56], preview icon atlas[+0x5E]
 *     Stat4 : current icon atlas[+0x5A], preview icon atlas[+0x5E]
 *   AP/DX share row bases (row_y+3) for the value digits and (row_y+4)
 *   for the preview-icon column; DP/Stat4 share (row_y+0xC) / (row_y+0xD).
 *
 * This renderer does NOT display the item name (that text id is computed
 * and stashed by the caller chain, not here).
 *
 * void __cdecl. EBX/ESI/EDI/EBP callee-saved; the __CHK(0x70) stack-probe
 * prologue is compiler-injected and omitted here. The current stats are
 * zero-extended uint16 fields; the preview stats are the int32 outputs of
 * the compute helper.
 * ---------------------------------------------------------------- */
void fd2_render_party_roster_with_item_stat_preview(uint32 candidate_count,
                                                    uint32 candidate_array_ptr,
                                                    uint32 item_id,
                                                    int32 highlight_idx,
                                                    int32 surface_offset)
{
    uint32 preview[4];          /* [0]=AP [1]=DP [2]=DX [3]=Stat4 */
    uint32 blink_frame;
    uint32 draw_count;
    uint32 iter;
    uint32 char_idx;
    uint32 cur_ap;
    uint32 cur_dp;
    uint32 cur_dx;
    uint32 cur_stat4;
    uint32 row_y;
    uint32 portrait_src;
    uint8  border_glyph;
    uint32 row_base3;           /* surface + (row_y+3)*0x140  */
    uint32 row_base4;           /* surface + (row_y+4)*0x140  */
    uint32 row_base12;          /* surface + (row_y+0xC)*0x140 */
    uint32 row_base13;          /* surface + (row_y+0xD)*0x140 */
    uint32 color;
    uint32 atlas;
    runtime_char *rt_chars;

    blink_frame = data_fd2_chapter_intro_dialog_subframe_anim_counter;
    if (data_fd2_chapter_intro_dialog_subframe_anim_counter == 3) {
        blink_frame = 1;
    }

    draw_count = candidate_count;
    if ((int32)candidate_count > 3) {
        draw_count = 3;
    }

    for (iter = 0; (int32)iter < (int32)draw_count; iter++) {
        char_idx = (uint32)*(uint8 *)(data_fd2_ui_menu_scroll_offset + iter
                                      + candidate_array_ptr);

        fd2_compute_equipped_stats_with_item_preview(
            char_idx, item_id, (uint32)preview);

        rt_chars = data_fd2_battle_runtime_char_array_ptr;
        cur_ap    = (uint32)rt_chars[char_idx].ap;
        cur_dp    = (uint32)rt_chars[char_idx].dp;
        cur_dx    = (uint32)rt_chars[char_idx].dx_current;
        cur_stat4 = (uint32)rt_chars[char_idx].stat4_current;
        row_y = iter * 0x1a + 0x75;

        portrait_src = *(int32 *)(data_fd2_portrait_sprite_cache
                                  + char_idx * 0x30 + blink_frame * 4)
                     + data_fd2_portrait_sprite_cache;
        fd2_tile_blit_24x24_with_dialog_bg_fill(
            portrait_src, row_y * 0x140 + surface_offset + 0xe, 0x140);

        border_glyph = 0xcd;
        if ((int32)(data_fd2_ui_menu_scroll_offset + iter) == highlight_idx) {
            border_glyph = 0xc9;
        }

        row_base3  = surface_offset + (row_y + 3) * 0x140;
        row_base4  = surface_offset + (row_y + 4) * 0x140;
        row_base12 = surface_offset + (row_y + 0xc) * 0x140;
        row_base13 = surface_offset + (row_y + 0xd) * 0x140;

        atlas = data_fd2_ui_menu_screen_sprite_atlas_buf_ptr;
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            rt_chars[char_idx].char_id + 1,
            row_base4 + 0x28, 0x140, border_glyph, 0x4c, 0, 0, 0);

        /* --- AP --- */
        color = fd2_pick_stat_compare_color((int32)cur_ap, (int32)preview[0]);
        fd2_dialog_sprite_blit_normal(
            row_base3 + 0x7a, atlas + *(int32 *)(atlas + 0x4e), 0x140);
        fd2_render_decimal_number_to_buffer(
            row_base3 + 0x89, 0x140, cur_ap, color, 3);
        fd2_dialog_sprite_blit_normal(
            row_base4 + 0x9d, atlas + *(int32 *)(atlas + 0x5e), 0x140);
        fd2_render_decimal_number_to_buffer(
            row_base3 + 0xa5, 0x140, preview[0], color, 3);

        /* --- DP --- */
        color = fd2_pick_stat_compare_color((int32)cur_dp, (int32)preview[1]);
        fd2_dialog_sprite_blit_normal(
            row_base12 + 0x7a, atlas + *(int32 *)(atlas + 0x52), 0x140);
        fd2_render_decimal_number_to_buffer(
            row_base12 + 0x89, 0x140, cur_dp, color, 3);
        fd2_dialog_sprite_blit_normal(
            row_base13 + 0x9d, atlas + *(int32 *)(atlas + 0x5e), 0x140);
        fd2_render_decimal_number_to_buffer(
            row_base12 + 0xa5, 0x140, preview[1], color, 3);

        /* --- DX --- */
        color = fd2_pick_stat_compare_color((int32)cur_dx, (int32)preview[2]);
        fd2_dialog_sprite_blit_normal(
            row_base3 + 0xc4, atlas + *(int32 *)(atlas + 0x56), 0x140);
        fd2_render_decimal_number_to_buffer(
            row_base3 + 0xd6, 0x140, cur_dx, color, 3);
        fd2_dialog_sprite_blit_normal(
            row_base4 + 0xea, atlas + *(int32 *)(atlas + 0x5e), 0x140);
        fd2_render_decimal_number_to_buffer(
            row_base3 + 0xf2, 0x140, preview[2], color, 3);

        /* --- Stat4 --- */
        color = fd2_pick_stat_compare_color((int32)cur_stat4,
                                            (int32)preview[3]);
        fd2_dialog_sprite_blit_normal(
            row_base12 + 0xc4, atlas + *(int32 *)(atlas + 0x5a), 0x140);
        fd2_render_decimal_number_to_buffer(
            row_base12 + 0xd6, 0x140, cur_stat4, color, 3);
        fd2_dialog_sprite_blit_normal(
            row_base13 + 0xea, atlas + *(int32 *)(atlas + 0x5e), 0x140);
        fd2_render_decimal_number_to_buffer(
            row_base12 + 0xf2, 0x140, preview[3], color, 3);
    }
}

/* ----------------------------------------------------------------
 * fd2_render_save_slot_grid @ 0x30437  (1 caller)
 *
 * Render the 4-slot SAVE-SLOT grid — per slot a "Slot N" header plus a
 * chapter intro-icon + chapter title (or a single "EMPTY" sentinel
 * sprite), with the selected slot drawn in the highlight border glyph.
 * Reads the decrypted FD2.SAV buffer to obtain each slot's stored
 * chapter id.
 *
 * Sole caller: fd2_save_slot_selector_ui @ 0x30550 (the slot picker
 * shared by both the save and load flows), which passes the current
 * cursor index, the compose target surface, and the decrypted SAV buf.
 *
 * Per slot (slot_iter = 0..3):
 *   slot_base    = sav_decrypted_buf + 0x312B + slot_iter*0xA28
 *   border_glyph = (slot_iter == highlight_slot) ? 0xC9 : 0xCD
 *   display_slot_number @ [0x53AE1] = slot_iter + 1   // 1-indexed, used
 *                                                     // by the dialog VM
 *                                                     // literal-number op
 *   row_off      = surface_offset + (slot_iter*0x13 + 0x77)*0x140
 *
 *   // "Slot N" header (FDTXT page 0x225):
 *   fd2_display_dialog_scene(all_game_text, 0x225, row_off + 0x0A,
 *                            0x140, border_glyph, 0x4C, 0, 0, 0)
 *
 *   chapter_id = slot_base[0xA00]                     // 0xFF = empty slot
 *   if (chapter_id != 0xFF):
 *     // chapter intro icon (page = chapter_id + 0x202):
 *     fd2_display_dialog_scene(all_game_text, chapter_id + 0x202,
 *                              row_off + 0x28, 0x140, border_glyph,
 *                              0x4C, 0, 0, 0)
 *     // shared tail renders the chapter title (page = chapter_id + 0x226)
 *     // at row_off + 0x82
 *   else:
 *     // shared tail renders "EMPTY" sprite (page 0x202) at row_off + 0x58
 *
 *   // shared tail call:
 *   fd2_display_dialog_scene(all_game_text, tail_page, tail_pos,
 *                            0x140, border_glyph, 0x4C, 0, 0, 0)
 *
 * Save-slot binary layout (per slot, 0xA28 bytes); the chapter byte sits
 * at +0xA00 after the 0xA00-byte per-slot map_terrain dump. SAV header =
 * 0x312B, 4 slots x 0xA28.
 *
 * void __cdecl. EBX/ESI/EDI/EBP callee-saved; the __CHK(0x38) stack-probe
 * prologue is compiler-injected and omitted here. tail_page/tail_pos are
 * the shared third dialog call: in the chapter branch they are the title;
 * in the empty branch they are the "EMPTY" sentinel.
 * ---------------------------------------------------------------- */
void fd2_render_save_slot_grid(uint32 highlight_slot, uint32 surface_offset,
                               uint8 *sav_decrypted_buf)
{
    uint32 slot_iter;
    uint8 *slot_base;
    uint8  border_glyph;
    uint32 row_off;
    uint32 chapter_id;
    uint32 tail_page;
    uint32 tail_pos;

    for (slot_iter = 0; (int32)slot_iter < 4; slot_iter++) {
        slot_base = sav_decrypted_buf + 0x312b + slot_iter * 0xa28;

        if (slot_iter == highlight_slot) {
            border_glyph = 0xc9;
        } else {
            border_glyph = 0xcd;
        }
        data_fd2_dialog_last_action_value_param = slot_iter + 1;

        row_off = surface_offset + (slot_iter * 0x13 + 0x77) * 0x140;

        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr, 0x225, row_off + 0xa,
            0x140, border_glyph, 0x4c, 0, 0, 0);

        chapter_id = (uint32)slot_base[0xa00];
        if (chapter_id == 0xff) {
            tail_pos = row_off + 0x58;
            tail_page = 0x202;
        } else {
            fd2_display_dialog_scene(
                data_fd2_all_game_text_ptr, chapter_id + 0x202,
                row_off + 0x28, 0x140, border_glyph, 0x4c, 0, 0, 0);
            tail_pos = row_off + 0x82;
            tail_page = chapter_id + 0x226;
        }

        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr, tail_page, tail_pos,
            0x140, border_glyph, 0x4c, 0, 0, 0);
    }
}

/* ----------------------------------------------------------------
 * fd2_render_promote_members_grid @ 0x30A47  (1 caller)
 *
 * Render the church-revive candidate grid -- up to 3 visible chars in
 * a single column, each showing portrait + char name + archetype + job +
 * a per-job revive price (5-digit decimal). Used ONLY by the church-revive
 * picker; class promotion has its own grid fd2_render_promote_candidates_grid
 * @ 0x31019 (which shows the post-promotion target job instead of a price).
 *
 * Sole caller: fd2_promote_members_select_loop @ 0x30C22 (the in-grid
 * Up/Down cursor loop of the church-revive picker), which passes the
 * candidate count, the compose surface, the highlight cursor index, and
 * the candidate index list.
 *
 * Blink-frame mapping:
 *   blink_frame = (subframe_counter == 3) ? 1 : counter   // 0,1,2,3->0,1,2,1
 *
 * Visible cap: draw_count = min(candidate_count, 3).
 *
 * Per char (iter = 0..draw_count-1):
 *   char_idx = candidate_idx_list[scroll_offset + iter]
 *   row_off  = iter * 0x1A
 *   24x24 portrait bg-fill blit (blink variant):
 *     src = cache + cache[char_idx*0x30 + blink_frame*4]
 *     dst = (row_off+0x75)*0x140 + surface_offset + 0x0E
 *   border_glyph = (scroll_offset + iter == highlight_idx) ? 0xC9 : 0xCD
 *   three FDTXT labels at text_col = surface_offset + (row_off+0x79)*0x140:
 *     char name : page = char.char_id        + 1,    pos = text_col + 0x28
 *     archetype : page = char.archetype_flag + 0x8C, pos = text_col + 0x82
 *     job name  : page = char.job_id         + 0x96, pos = text_col + 0xAF
 *   price + gold icon at price_y = surface_offset + (row_off+0x7D)*0x140:
 *     coin icon sprite 0x0F (menu atlas) at price_y + 0xDC
 *     price = char.level * cost_table[char.job_id - 1]   // int16 cost mult
 *     5-digit orange (colour 0x77) at price_y + 0xE4
 *
 * void __cdecl. EBX/ESI/EDI/EBP callee-saved; the __CHK(0x4C) stack-probe
 * prologue is compiler-injected and omitted here. The cost-table read is a
 * sign-extended int16 (MOVSX) indexed by job_id-1; the price multiply is a
 * signed int * level (the table values are all positive, so the low 32 bits
 * match an unsigned multiply either way).
 * ---------------------------------------------------------------- */
void fd2_render_promote_members_grid(uint32 candidate_count,
                                     uint32 surface_offset,
                                     uint32 highlight_idx,
                                     uint8 *candidate_idx_list)
{
    uint32 blink_frame;
    uint32 draw_count;
    uint32 iter;
    uint32 char_idx;
    uint8  job_id;
    uint8  level;
    uint32 row_off;
    uint32 portrait_src;
    uint8  border_glyph;
    uint32 text_col;
    uint32 price_y;
    runtime_char *rt_chars;

    blink_frame = data_fd2_chapter_intro_dialog_subframe_anim_counter;
    if (data_fd2_chapter_intro_dialog_subframe_anim_counter == 3) {
        blink_frame = 1;
    }

    draw_count = candidate_count;
    if ((int32)candidate_count > 3) {
        draw_count = 3;
    }

    for (iter = 0; (rt_chars = data_fd2_battle_runtime_char_array_ptr,
                    (int32)iter < (int32)draw_count); iter++) {
        char_idx = (uint32)candidate_idx_list[data_fd2_ui_menu_scroll_offset
                                              + iter];
        job_id = rt_chars[char_idx].job_id;
        level  = rt_chars[char_idx].status_flags_block[0];
        row_off = iter * 0x1a;

        portrait_src = *(int32 *)(data_fd2_portrait_sprite_cache
                                  + char_idx * 0x30 + blink_frame * 4)
                     + data_fd2_portrait_sprite_cache;
        fd2_tile_blit_24x24_with_dialog_bg_fill(
            portrait_src,
            (row_off + 0x75) * 0x140 + surface_offset + 0xe,
            0x140);

        border_glyph = 0xcd;
        if (data_fd2_ui_menu_scroll_offset + iter == highlight_idx) {
            border_glyph = 0xc9;
        }

        text_col = surface_offset + (row_off + 0x79) * 0x140;
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            rt_chars[char_idx].char_id + 1,
            text_col + 0x28, 0x140, border_glyph, 0x4c, 0, 0, 0);
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            rt_chars[char_idx].archetype_flag + 0x8c,
            text_col + 0x82, 0x140, border_glyph, 0x4c, 0, 0, 0);
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            rt_chars[char_idx].job_id + 0x96,
            text_col + 0xaf, 0x140, border_glyph, 0x4c, 0, 0, 0);

        price_y = surface_offset + (row_off + 0x7d) * 0x140;
        fd2_blit_sheet_sprite_at_offset(
            price_y + 0xdc, 0x140,
            data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0xf);
        fd2_render_decimal_number_to_buffer(
            price_y + 0xe4, 0x140,
            (int32)data_fd2_ui_per_job_revive_or_promote_cost_table[job_id - 1]
                * (uint32)level,
            0x77, 5);
    }
}

/* ----------------------------------------------------------------
 * fd2_render_promote_candidates_grid @ 0x31019  (1 caller)
 *
 * Render the CLASS PROMOTION candidate grid — up to 3 visible chars in a
 * single column, each showing portrait + char name + current job + a "->"
 * promotion icon + the target post-promotion job. The target job comes from
 * the per-class promotion data table looked up via
 * fd2_get_class_promotion_data_entry (entry[0] = post-promotion job_id).
 *
 * Sole caller: fd2_promote_member_select_loop @ 0x311DC (the in-grid Up/Down
 * cursor loop), which passes the candidate count, the compose surface, the
 * highlight cursor index, the candidate index list, and the parallel
 * promotion-target class list.
 *
 * Blink-frame mapping:
 *   blink_frame = (subframe_counter == 3) ? 1 : counter   // 0,1,2,3->0,1,2,1
 *
 * Visible cap: draw_count = min(candidate_count, 3).
 *
 * Per char (iter = 0..draw_count-1):
 *   char_idx       = candidate_idx_list[scroll_offset + iter]
 *   row_off        = iter * 0x1A
 *   24x24 portrait bg-fill blit (blink variant):
 *     src = cache + cache[char_idx*0x30 + blink_frame*4]
 *     dst = (row_off+0x75)*0x140 + surface_offset + 0x0E
 *   border_glyph = (scroll_offset + iter == highlight_idx) ? 0xC9 : 0xCD
 *   four FDTXT labels at text_col = surface_offset + (row_off+0x79)*0x140:
 *     char name  : page = char.char_id + 1,                      pos = +0x28
 *     current job: page = char.job_id  + 0x96,                   pos = +0x82
 *     "-> 轉職"  : page = 0x251,                                 pos = +0xAF
 *     target job : page = promo_entry[0] + 0x96,                 pos = +0xEF
 *       where promo_entry = fd2_get_class_promotion_data_entry(
 *                               promotion_target_list[scroll_offset + iter])
 *
 * void __cdecl. EBX/ESI/EDI/EBP callee-saved; the __CHK(0x48) stack-probe
 * prologue is compiler-injected and omitted here. The target-job page reads
 * the first byte of the looked-up 2-byte promotion entry; the EAX returned by
 * fd2_get_class_promotion_data_entry is the pointer dereferenced for that byte
 * (verified against the disassembly, not the EAX-tracking decompiler output).
 * ---------------------------------------------------------------- */
void fd2_render_promote_candidates_grid(uint32 candidate_count,
                                        uint32 surface_offset,
                                        uint32 highlight_idx,
                                        uint8 *candidate_idx_list,
                                        uint8 *promotion_target_list)
{
    uint32 blink_frame;
    uint32 draw_count;
    uint32 iter;
    uint32 char_idx;
    uint32 row_off;
    uint32 portrait_src;
    uint8  border_glyph;
    uint32 text_col;
    uint8 *promo_entry;
    runtime_char *rt_chars;

    blink_frame = data_fd2_chapter_intro_dialog_subframe_anim_counter;
    if (data_fd2_chapter_intro_dialog_subframe_anim_counter == 3) {
        blink_frame = 1;
    }

    draw_count = candidate_count;
    if ((int32)candidate_count > 3) {
        draw_count = 3;
    }

    for (iter = 0; (rt_chars = data_fd2_battle_runtime_char_array_ptr,
                    (int32)iter < (int32)draw_count); iter++) {
        char_idx = (uint32)candidate_idx_list[data_fd2_ui_menu_scroll_offset
                                              + iter];
        row_off = iter * 0x1a;

        portrait_src = *(int32 *)(data_fd2_portrait_sprite_cache
                                  + char_idx * 0x30 + blink_frame * 4)
                     + data_fd2_portrait_sprite_cache;
        fd2_tile_blit_24x24_with_dialog_bg_fill(
            portrait_src,
            (row_off + 0x75) * 0x140 + surface_offset + 0xe,
            0x140);

        border_glyph = 0xcd;
        if (data_fd2_ui_menu_scroll_offset + iter == highlight_idx) {
            border_glyph = 0xc9;
        }

        text_col = surface_offset + (row_off + 0x79) * 0x140;
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            rt_chars[char_idx].char_id + 1,
            text_col + 0x28, 0x140, border_glyph, 0x4c, 0, 0, 0);
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            rt_chars[char_idx].job_id + 0x96,
            text_col + 0x82, 0x140, border_glyph, 0x4c, 0, 0, 0);
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            0x251,
            text_col + 0xaf, 0x140, border_glyph, 0x4c, 0, 0, 0);

        promo_entry = fd2_get_class_promotion_data_entry(
            (int)promotion_target_list[data_fd2_ui_menu_scroll_offset + iter]);
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr,
            promo_entry[0] + 0x96,
            text_col + 0xef, 0x140, border_glyph, 0x4c, 0, 0, 0);
    }
}

/* ----------------------------------------------------------------
 * fd2_render_recruitment_select_screen @ 0x31E80  (2 callers)
 *
 * Composes one frame of the recruitment / chapter-branch party-select
 * screen into the shared composed-target working surface
 * (data_fd2_ui_slide_composed_target_buf_ptr, 64000 bytes). The caller
 * later memmoves the surface to VGA 0xA0000.
 *
 *   panel_buf  — backup panel buffer (caller's pvVar2); the pre-built
 *                base panel with header banner + grid frame. Restored
 *                into the working surface each frame.
 *   max_chars  — recruit cap (0x0F or 0x13).
 *   sel_state  — selection_state byte array (1 = picked, 0 = not).
 *   cursor_idx — current cursor slot (passed as uint*, used as an int
 *                grid index).
 *
 * Frame composition:
 *   1. memmove(surface, panel_buf, 64000): restore base panel.
 *   2. Top "max" digits at surface+0x2BFD (value = max_chars).
 *   3. fd2_count_selected_chars(sel_state) called twice; only the 2nd
 *      return is used (preserved as-is — the 1st call's result is
 *      discarded by the original code).
 *   4. Bottom "remaining" digits at surface+0x5B7D (value =
 *      max_chars - count).
 *   5. Tick the chapter ambient palette animation, then read the anim
 *      index and collapse 3 -> 1 (4 phases map to 3 frame slots).
 *   6. fd2_render_full_char_stat_panel(cursor_idx + 1, surface): cursor
 *      character's stat panel on the right.
 *   7. Cursor highlight sprite blitted at the cursor's grid cell.
 *      highlight_sprite = runtime_battle_state +
 *      *(int*)(runtime_battle_state + 6). Cell offset uses a 10-wide
 *      grid: column 28 px, row 30 px, base y = 0x68.
 *   8. For each grid slot iter in [0, menu_party_member_count - 1):
 *        portrait RLE stream = cache + cache[(iter*0xC + palette_idx
 *        + 0xC)*4]  (12 ptr entries per char; +0xC skips lord/leader).
 *        cell offset uses the same 10-wide grid (base y = 100).
 *        sel_state[iter] == 0 -> dimmed grayscale blit at the cell;
 *        else passthrough blit 3 rows lower (cell + 0x3C0) = "pressed".
 *
 * Returns void. __cdecl, 4 stack params. The __CHK(0x28) stack-probe
 * prologue is compiler-injected and omitted here.
 *
 * Callers (2): fd2_run_recruitment_or_branch_screen @ 0x318AD,
 *              fd2_wait_input_with_recruitment_repaint @ 0x32004.
 * ---------------------------------------------------------------- */
void fd2_render_recruitment_select_screen(uint32 panel_buf,
                                          uint32 max_chars,
                                          uint32 sel_state,
                                          uint32 cursor_idx)
{
    uint32 surface;
    int    count;
    int    palette_idx;
    uint32 highlight_src;
    uint32 cursor_off;
    int    iter;
    uint32 char_off;
    uint32 rle_stream;

    surface = data_fd2_ui_slide_composed_target_buf_ptr;
    memmove((void *)surface, (void *)panel_buf, 64000);

    fd2_render_decimal_number_to_buffer(
        surface + 0x2bfd, 0x140, max_chars, 0x1f, 2);

    fd2_count_selected_chars(sel_state);
    count = fd2_count_selected_chars(sel_state);
    fd2_render_decimal_number_to_buffer(
        surface + 0x5b7d, 0x140, max_chars - count, 0x2a, 2);

    fd2_tick_chapter_palette_animation();
    palette_idx = (int)data_fd2_graphics_chapter_ambient_palette_anim_idx;
    if (palette_idx == 3) {
        palette_idx = 1;
    }

    fd2_render_full_char_stat_panel(cursor_idx + 1, surface);

    highlight_src = data_fd2_runtime_battle_state_ptr
                  + *(int32 *)(data_fd2_runtime_battle_state_ptr + 6);
    cursor_off = ((int)cursor_idx % 10) * 0x1c + 0x17
               + (((int)cursor_idx / 10) * 0x1e + 0x68) * 0x140;
    fd2_tile_blit_24x24_passthrough(highlight_src, surface + cursor_off, 0x140);

    for (iter = 0; iter < (int32)data_fd2_shared_menu_party_member_count - 1;
         iter++) {
        char_off = (iter % 10) * 0x1c + 0x17
                 + ((iter / 10) * 0x1e + 100) * 0x140;
        rle_stream = data_fd2_portrait_sprite_cache
                   + *(int32 *)(data_fd2_portrait_sprite_cache
                                + (iter * 0xc + palette_idx + 0xc) * 4);
        if (*(char *)(sel_state + iter) == '\0') {
            fd2_tile_blit_24x24_dimmed_grayscale(
                rle_stream, surface + char_off, 0x140);
        } else {
            fd2_tile_blit_24x24_passthrough(
                rle_stream, surface + char_off + 0x3c0, 0x140);
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_render_battle_scene_with_portrait_grid_layout @ 0x34010  (0 callers)
 *
 * Composes the battle preview/intro screen into a private 64000-byte
 * working canvas, then commits it to VGA 0xA0000. Draws, in order:
 *   - the two-digit chapter number (tens + ones) as glyph tiles,
 *   - a row of 6 player-character portrait tiles,
 *   - a row of `enemy_count` enemy portrait tiles,
 *   - one reserved-position highlight overlay tile on the player row.
 * Every tile is drawn via fd2_blit_24x24_tile_to_battle_grid_position
 * (atlas_base, tile_id, dst_buf, row_stride=0x140, dst_x, dst_y).
 *
 * Sequence:
 *   1. dst = malloc(64000); memmove(dst, src_framebuffer, 64000) — copy
 *      the supplied background as the starting canvas.
 *   2. sprintf(scratch, "%02d", chapter_id) -> two ASCII digits; subtract
 *      '0' from each to get 0..9 nibbles.
 *   3. tens digit  -> tile (0x40 + digit) at (x=0x96, y=0x4B).
 *      ones digit  -> tile (0x40 + digit) at (x=0xA2, y=0x4B). 0xC px apart.
 *   4. for i in 0..5: player_char_id_array[i] tile at (i*0x19 + 0x56, 0x84).
 *   5. for i in 0..enemy_count-1: enemy_id_array[i] tile at
 *      (i*0x20 + 0x74, 0x61).
 *   6. highlight overlay: tile 0 from the runtime_battle_state atlas
 *      (atlas base = the pointer value at data_fd2_runtime_battle_state_ptr)
 *      at the player-row slot reserved_char_pos: (pos*0x19 + 0x56, 0x84).
 *   7. memmove(0xA0000, dst, 64000); free(dst).
 *
 * The "%02d" format string lives at 0x502E8 in the binary; reproduced
 * here as a literal. chapter_id is the sprintf vararg.
 *
 * void __cdecl, 7 stack params. EBX/ESI/EDI/EBP are callee-saved; the
 * __CHK(0x34) stack-probe prologue is compiler-injected and omitted here.
 * No direct xref callers — invoked indirectly (battle preview / intro
 * dispatcher).
 * ---------------------------------------------------------------- */
void fd2_render_battle_scene_with_portrait_grid_layout(
    uint32 tile_atlas_base, uint32 src_framebuffer, uint32 chapter_id,
    uint8 *player_char_id_array, int32 enemy_count, uint8 *enemy_id_array,
    int32 reserved_char_pos)
{
    void  *dst;
    char   digits[4];
    int32  i;

    dst = malloc(64000);
    memmove(dst, (void *)src_framebuffer, 64000);

    sprintf(digits, "%02d", chapter_id);
    digits[0] = (char)(digits[0] - '0');
    digits[1] = (char)(digits[1] - '0');

    fd2_blit_24x24_tile_to_battle_grid_position(
        tile_atlas_base, (uint8)digits[0] + 0x40, (uint32)dst, 0x140, 0x96, 0x4b);
    fd2_blit_24x24_tile_to_battle_grid_position(
        tile_atlas_base, (uint8)digits[1] + 0x40, (uint32)dst, 0x140, 0xa2, 0x4b);

    for (i = 0; i < 6; i++) {
        fd2_blit_24x24_tile_to_battle_grid_position(
            tile_atlas_base, (uint32)player_char_id_array[i], (uint32)dst,
            0x140, i * 0x19 + 0x56, 0x84);
    }

    for (i = 0; i < enemy_count; i++) {
        fd2_blit_24x24_tile_to_battle_grid_position(
            tile_atlas_base, (uint32)enemy_id_array[i], (uint32)dst,
            0x140, i * 0x20 + 0x74, 0x61);
    }

    fd2_blit_24x24_tile_to_battle_grid_position(
        data_fd2_runtime_battle_state_ptr, 0, (uint32)dst, 0x140,
        reserved_char_pos * 0x19 + 0x56, 0x84);

    memmove((void *)0xa0000, dst, 64000);
    free(dst);
}

/* ----------------------------------------------------------------
 * Module data definitions (owned by this translation unit)
 * ---------------------------------------------------------------- */

/* @ 0x54153  Sub-frame animation counter for the chapter-intro dialog
 * panels. Advanced by fd2_render_chapter_intro_dialog_panels on every
 * odd master frame_idx, wrapping 0->1->2->3->0 (reset at 4). Read by
 * the dialog/roster renderers to pick the portrait animation phase.
 * Zero-initialized; first runtime use is the increment/reset path. */
uint32 data_fd2_chapter_intro_dialog_subframe_anim_counter;
