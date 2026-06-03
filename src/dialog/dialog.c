/*
 * dialog.c — FD2 dialog-box sprite/overlay helpers.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>

/* ----------------------------------------------------------------
 * fd2_cleanup_dialog_sprite_buffer @ 0x15E71 (7 callers)
 *
 * Restore the screen block saved inside saved_block back to dst at
 * the given stride, then free saved_block. Undoes a transient sprite
 * blit (pairs with fd2_blit_indexed_sprite_with_alloc /
 * fd2_alloc_and_blit_indexed_sprite_chunk): the pre-blit screen
 * content was snapshotted into saved_block's saved-pixels area, and
 * this restores it before releasing the temp buffer.
 * ---------------------------------------------------------------- */
void fd2_cleanup_dialog_sprite_buffer(uint32 saved_block, uint32 dst, uint32 stride)
{
    fd2_restore_screen_block_from_buffer(saved_block, dst, stride);
    free((void *)saved_block);
}
