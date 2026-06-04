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
