/*
 * rndstat.c — dialog portrait / status-area rendering helpers
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_paint_portrait_to_dialog_area @ 0x16559 (30 callers)
 *
 * Blit one portrait sprite frame into the active dialog portrait slot,
 * choosing a normal vs horizontally-mirrored blit based on the active
 * portrait slot offset (data_fd2_dialog_active_portrait_blit_offset).
 *
 * The portrait sprite source is the active DATO.DAT entry stored in
 * data_fd2_portrait_sprite_buffer. The first ints at the buffer head
 * form a per-frame sprite-offset table (mouth-open/close cycle); the
 * chosen frame's sprite payload is at buffer + offset_table[frame].
 *
 * data_fd2_dialog_active_portrait_blit_offset:
 *   0x0728  left-side portrait slot   -> normal blit
 *   0x9017  right-side ally portrait  -> mirrored blit
 * The destination linear address is 0xA0000 + that offset.
 *
 * Cdecl, 1 stack param; void return.
 * ---------------------------------------------------------------- */
void fd2_paint_portrait_to_dialog_area(uint32 frame)
{
    uint32 sprite_addr;

    sprite_addr = *(int32 *)(data_fd2_portrait_sprite_buffer + frame * 4)
                  + (uint32)data_fd2_portrait_sprite_buffer;

    if (data_fd2_dialog_active_portrait_blit_offset != 0x9017) {
        fd2_dialog_sprite_blit_normal(
            data_fd2_dialog_active_portrait_blit_offset + 0xa0000,
            sprite_addr, 0x140);
        return;
    }
    fd2_dialog_sprite_blit_mirrored(0xa9017, sprite_addr, 0x140);
}

/* ----------------------------------------------------------------
 * fd2_render_horizontal_bar_segments @ 0x17d6f (1 caller)
 *
 * Render a 1-pixel-segmented horizontal HP/MP bar at dst_offset.
 * Each segment sprite is 1 pixel wide; the bar advances one pixel
 * per segment.
 *
 * filled_count == 0 -> fully empty bar:
 *   0x65 (101) empty-middle sprites (0x1D) at dst_offset+1 .. dst_offset+0x65,
 *   then empty right cap (0x1E) at dst_offset+0x66 (the offset the loop counter
 *   reaches; NOT dst_offset+filled_count, which would be +0 here).
 *
 * filled_count != 0 -> filled bar:
 *   left cap (sprite_base) at dst_offset,
 *   (filled_count-1) middle sprites (sprite_base+1) at dst_offset+1 ..,
 *   then right cap (sprite_base+2) at dst_offset+filled_count.
 *
 * Sprite base indices: 0x17 = HP bar (red), 0x1A = MP bar (blue).
 *
 * Cdecl, 4 stack params; void return. Mirrors the binary's shared-final-blit
 * control flow: the offset of the trailing cap (uVar1) is the value left in
 * EAX by the last LEA inside whichever branch ran.
 * ---------------------------------------------------------------- */
void fd2_render_horizontal_bar_segments(uint32 dst_offset, uint32 dst_pitch,
                                        uint32 filled_count, uint32 sprite_base)
{
    uint32 cap_offset;
    uint32 empty_iter;
    uint32 filled_iter;
    uint32 final_sprite;

    cap_offset = 0;
    empty_iter = 0;
    if (filled_count == 0) {
        for (;;) {
            empty_iter = empty_iter + 1;
            cap_offset = dst_offset + empty_iter;
            if ((int32)empty_iter > 0x65) {
                break;
            }
            fd2_blit_sheet_sprite_at_offset(cap_offset, dst_pitch,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x1d);
        }
        final_sprite = 0x1e;
    } else {
        fd2_blit_sheet_sprite_at_offset(dst_offset, dst_pitch,
                                        data_fd2_ui_anim_sprite_sheet_ptr,
                                        sprite_base);
        for (filled_iter = 1;
             cap_offset = dst_offset + filled_iter,
                 (int32)filled_iter < (int32)filled_count;
             filled_iter = filled_iter + 1) {
            fd2_blit_sheet_sprite_at_offset(cap_offset, dst_pitch,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            sprite_base + 1);
        }
        final_sprite = sprite_base + 2;
    }
    fd2_blit_sheet_sprite_at_offset(cap_offset, dst_pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr,
                                    final_sprite);
}

/* ----------------------------------------------------------------
 * fd2_render_status_screen_static_layout @ 0x17eef (3 callers)
 *
 * Render the static border + portrait + UI sprites of the character
 * status panel into overlay_buffer, then call the stat-detail painter.
 *
 * Sets the active portrait blit offset to the status-screen mode
 * (0xC88), loads the character's portrait sprite from DATO.DAT, draws
 * the layered dialog frame, blits the portrait and two UI sprites, then
 * fills HP/MP bars and numeric stats via fd2_render_full_char_stat_panel.
 *
 * data_fd2_dialog_active_portrait_blit_offset values:
 *   0x0000  inactive
 *   0x0728  left-side dialog portrait
 *   0x9017  right-side ally dialog portrait
 *   0x0C88  status screen portrait
 *
 * Cdecl, 2 stack params; void return.
 * ---------------------------------------------------------------- */
void fd2_render_status_screen_static_layout(uint32 char_idx, uint32 overlay_buffer)
{
    uint32 portrait_pixels;

    data_fd2_dialog_active_portrait_blit_offset = 0xc88;
    data_fd2_portrait_sprite_buffer =
        (uint8 *)fd2_load_dat_resource(
            0x51a70, (uint32)data_fd2_portrait_sprite_buffer,
            (uint32)data_fd2_battle_runtime_char_array_ptr[char_idx].portrait_id);

    portrait_pixels = (uint32)*data_fd2_portrait_sprite_buffer
                      + (uint32)data_fd2_portrait_sprite_buffer;

    fd2_assemble_dialog_frame_layered(overlay_buffer, 0x140, 5, 7, 5, 5);

    fd2_dialog_sprite_blit_normal(
        overlay_buffer + data_fd2_dialog_active_portrait_blit_offset,
        portrait_pixels, 0x140);
    fd2_dialog_sprite_blit_normal(
        overlay_buffer + 0x91c,
        data_fd2_ui_anim_sprite_sheet_ptr
            + *(int32 *)(data_fd2_ui_anim_sprite_sheet_ptr + 0x56),
        0x140);
    fd2_dialog_sprite_blit_normal(
        overlay_buffer + 0x7585,
        data_fd2_ui_anim_sprite_sheet_ptr
            + *(int32 *)(data_fd2_ui_anim_sprite_sheet_ptr + 0x5a),
        0x140);

    fd2_render_full_char_stat_panel(char_idx, overlay_buffer);
}
