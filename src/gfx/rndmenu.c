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
