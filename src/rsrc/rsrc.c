/*
 * rsrc.c — chapter resource (background layer) loading
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>

/* ----------------------------------------------------------------
 * fd2_load_chapter_background_layers @ 0x10652  (1 caller)
 *
 * Chapter-specific background layer load from FDOTHER.DAT.
 *
 * Frees + nulls both static_bg_buffer and animated_bg_buffer, then
 * selects one of three load shapes by current chapter id:
 *   - Single-sprite default (chapters 9/0x18/0x19 -> idx 0xF,
 *     0x1C/0x1D -> idx 0x37, all others -> idx 0x10): load sprite
 *     into static_bg_buffer, allocate a 320x200 work buffer.
 *   - 2-sprite widescreen (chapters 0x11/0x15/0x16/0x1B): allocate
 *     static_bg_buffer sized bg_width*bg_height, blit a top half and
 *     a bottom half (top at y=0, bottom at y=bg_height/2).
 *   - Text-scroll cinematic (chapter 0x17): allocate a 312x200
 *     buffer, blit sheet idx 0x2A, arm the text-scroll cinematic.
 * ---------------------------------------------------------------- */
void fd2_load_chapter_background_layers(void)
{
    uint32 bg_width;
    uint32 bg_height;
    uint32 fdother_idx;
    uint32 idx_base;
    uint32 top_sprite;
    uint32 bot_sprite;

    bg_width = 0x1ce;
    bg_height = 0xe2;
    fdother_idx = 0x10;

    if (data_fd2_graphics_static_bg_buffer_ptr != 0) {
        free((void *)data_fd2_graphics_static_bg_buffer_ptr);
    }
    data_fd2_graphics_static_bg_buffer_ptr = 0;

    if (data_fd2_graphics_animated_bg_buffer_ptr != 0) {
        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
    }
    data_fd2_graphics_animated_bg_buffer_ptr = 0;

    if (data_fd2_chapter_current_chapter_id == 9 ||
        data_fd2_chapter_current_chapter_id == 0x18 ||
        data_fd2_chapter_current_chapter_id == 0x19) {
        fdother_idx = 0xf;
    } else if (data_fd2_chapter_current_chapter_id == 0x11 ||
               data_fd2_chapter_current_chapter_id == 0x15 ||
               data_fd2_chapter_current_chapter_id == 0x16 ||
               data_fd2_chapter_current_chapter_id == 0x1b) {
        idx_base = 0x10;
        if (data_fd2_chapter_current_chapter_id == 0x15) {
            bg_width = 0x198;
            bg_height = 0x114;
            idx_base = 0x23;
        } else if (data_fd2_chapter_current_chapter_id == 0x16) {
            bg_width = 0x198;
            bg_height = 0x100;
            idx_base = 0x28;
        } else if (data_fd2_chapter_current_chapter_id == 0x1b) {
            bg_height = 0xf4;
            idx_base = 0x2e;
        }

        data_fd2_graphics_static_bg_buffer_ptr =
            (uint32)malloc(bg_width * bg_height);

        top_sprite = fd2_load_dat_resource(0x51a4d,
            data_fd2_graphics_animated_bg_buffer_ptr, idx_base);
        data_fd2_graphics_animated_bg_buffer_ptr = top_sprite;
        fd2_rle_blit_sprite(top_sprite, 0, 0,
            data_fd2_graphics_static_bg_buffer_ptr, bg_width, 0xffffffff);

        bot_sprite = fd2_load_dat_resource(0x51a4d,
            data_fd2_graphics_animated_bg_buffer_ptr, idx_base + 1);
        data_fd2_graphics_animated_bg_buffer_ptr = bot_sprite;
        fd2_rle_blit_sprite(bot_sprite, 0, (int32)bg_height / 2,
            data_fd2_graphics_static_bg_buffer_ptr, bg_width, 0xffffffff);

        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
        data_fd2_graphics_animated_bg_buffer_ptr = 0;
        return;
    } else if (data_fd2_chapter_current_chapter_id == 0x17) {
        data_fd2_graphics_static_bg_buffer_ptr = (uint32)malloc(0xea00);

        data_fd2_graphics_animated_bg_buffer_ptr = fd2_load_dat_resource(
            0x51a4d, data_fd2_graphics_animated_bg_buffer_ptr, 0x2a);
        fd2_rle_blit_sprite(data_fd2_graphics_animated_bg_buffer_ptr, 0, 0,
            data_fd2_graphics_static_bg_buffer_ptr, 0x138, 0xffffffff);

        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
        data_fd2_graphics_animated_bg_buffer_ptr = 0;
        fd2_scroll_text_screen_up_by_lines(0);
        return;
    } else if (data_fd2_chapter_current_chapter_id != 0x1c &&
               data_fd2_chapter_current_chapter_id != 0x1d) {
        data_fd2_graphics_animated_bg_buffer_ptr = 0;
        return;
    } else {
        fdother_idx = 0x37;
    }

    data_fd2_graphics_static_bg_buffer_ptr = fd2_load_dat_resource(
        0x51a4d, data_fd2_graphics_static_bg_buffer_ptr, fdother_idx);
    data_fd2_graphics_animated_bg_buffer_ptr = (uint32)malloc(64000);
}
