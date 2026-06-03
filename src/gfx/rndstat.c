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
