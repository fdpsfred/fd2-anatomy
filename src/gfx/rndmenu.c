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
            *(int32 *)(portrait_sprite_cache
                       + char_idx * 0x30 + blink_frame * 4)
                + portrait_sprite_cache,
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

        portrait_src = *(int32 *)(portrait_sprite_cache
                                  + char_idx * 0x30 + blink_frame * 4)
                     + portrait_sprite_cache;
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
