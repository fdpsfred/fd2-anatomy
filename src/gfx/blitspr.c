/*
 * blitspr.c — sprite / rectangle blit primitives
 */

#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_blit_rectangle @ 0x11EB0 (53 callers)
 *
 * Universal 2D rectangle blit. Copies an h x w_bytes block from src
 * to dst, advancing each pointer by its own stride per row:
 *
 *   for i in 0..height:
 *       memmove(dst, src, src_w_bytes)
 *       dst += dst_stride
 *       src += src_stride
 *
 * The simplest and most-called blit primitive in the codebase: it is
 * the final composite stage for UI / battle / dialog rendering
 * (e.g. copying the render workspace to the mode13h primary at
 * 0xA0000, snapshot copies, rectangle moves).
 *
 * Args (cdecl, 6x uint32 on stack):
 *   dst         — destination base linear address
 *   dst_stride  — bytes to advance dst per row
 *   src         — source base linear address
 *   src_stride  — bytes to advance src per row
 *   src_w_bytes — bytes copied per row (the memmove count)
 *   height      — number of rows; signed compare, <=0 copies nothing
 * ---------------------------------------------------------------- */
void fd2_blit_rectangle(uint32 dst, uint32 dst_stride, uint32 src,
                        uint32 src_stride, uint32 src_w_bytes, uint32 height)
{
    uint32 row;

    for (row = 0; (int32)row < (int32)height; row++) {
        memmove((void *)dst, (void *)src, src_w_bytes);
        dst += dst_stride;
        src += src_stride;
    }
}

/* ----------------------------------------------------------------
 * fd2_blit_indexed_sprite_with_alloc @ 0x15E9E (2 callers)
 *
 * Allocate a "save under" buffer, snapshot the destination region into
 * it, then blit one indexed sprite into the destination.
 *
 * The sprite header (sprite_hdr) carries width/height as the first two
 * signed 16-bit words. The source pixel address is computed as a flat
 * offset from sheet_base:
 *
 *   sprite_data = sheet_base + sprite_idx * dst_pitch
 *
 * (in the dialog-open caller this resolves a destination page offset:
 * sprite_idx and sheet_base are interp_y and interp_x respectively, so
 * sprite_data = interp_y * 0x140 + interp_x).
 *
 * A temp buffer sized width*height + 8 is malloc'd: the +8 is the
 * snapshot header (width, height, src_ptr) written by
 * fd2_save_screen_block_to_buffer, followed by the saved pixels. After
 * saving, the sprite is blitted into dst at (sprite_data + dst).
 *
 * Returns the allocated save buffer; the CALLER frees it (via
 * fd2_cleanup_dialog_sprite_buffer, which restores the snapshot first).
 *
 * Args (cdecl, 5x uint32 on stack):
 *   sprite_hdr — sprite header pointer (width word 0, height word 2)
 *   dst        — destination base linear address
 *   dst_pitch  — destination row stride / per-sprite source stride
 *   sheet_base — source base linear address
 *   sprite_idx — source row index (scaled by dst_pitch)
 * ---------------------------------------------------------------- */
void *fd2_blit_indexed_sprite_with_alloc(uint32 sprite_hdr, uint32 dst,
                                         uint32 dst_pitch, uint32 sheet_base,
                                         uint32 sprite_idx)
{
    int32 width;
    int32 height;
    uint32 sprite_data;
    void *save_buf;

    width = *(int16 *)sprite_hdr;
    height = *(int16 *)(sprite_hdr + 2);
    sprite_data = sprite_idx * dst_pitch + sheet_base;

    save_buf = malloc(width * height + 8);
    fd2_save_screen_block_to_buffer((uint32)save_buf, (uint32)width,
                                    (uint32)height, dst, sprite_data, dst_pitch);
    fd2_blit_sprite_with_stride_setup(sprite_data + dst, sprite_hdr, dst_pitch);
    return save_buf;
}

/* ----------------------------------------------------------------
 * fd2_alloc_and_blit_indexed_sprite_chunk @ 0x15F0E (8 callers)
 *
 * One-shot decode + paint of an indexed sprite from a sprite-atlas
 * sheet into a destination surface, allocating a scratch "save under"
 * buffer on the fly.
 *
 * Sprite atlas layout: a 4-byte-per-entry offset table starts at
 * sheet_base + 6. Entry sprite_idx gives the byte offset (from
 * sheet_base) of that sprite's header:
 *
 *   sprite_hdr = sheet_base + *(int32 *)(sheet_base + 6 + sprite_idx * 4);
 *   width  = *(int16 *)(sprite_hdr + 0);   (signed)
 *   height = *(int16 *)(sprite_hdr + 2);   (signed)
 *
 * The paint position is dst_off = row_idx * surface_pitch + col_offset.
 * A scratch buffer of width*height + 8 bytes is malloc'd (the +8 is the
 * snapshot header written by fd2_save_screen_block_to_buffer), the
 * destination block under the sprite is snapshotted into it, then the
 * sprite pixels are decoded and painted at dst + dst_off.
 *
 * The malloc'd buffer pointer is left in EAX (asm tail: MOV EAX,EDI into
 * the shared epilogue at 0x22BBE) and thus returned, but the sole live
 * caller (fd2_load_save_and_init_engine's chapter-intro slideshow)
 * discards it and frees the snapshot separately via
 * fd2_cleanup_dialog_sprite_buffer.
 *
 * Args (cdecl, 6x uint32 on stack):
 *   sheet_base    — sprite atlas base linear address
 *   dst           — destination surface base linear address
 *   surface_pitch — destination row stride
 *   col_offset    — column byte offset within the destination row
 *   row_idx       — destination row index
 *   sprite_idx    — index into the sheet's offset table
 * ---------------------------------------------------------------- */
uint32 fd2_alloc_and_blit_indexed_sprite_chunk(uint32 sheet_base, uint32 dst,
                                               uint32 surface_pitch,
                                               uint32 col_offset, uint32 row_idx,
                                               uint32 sprite_idx)
{
    int32 width;
    int32 height;
    uint32 sprite_hdr;
    uint32 dst_off;
    void *save_buf;

    sprite_hdr = sheet_base + *(int32 *)(sheet_base + 6 + sprite_idx * 4);
    width = *(int16 *)sprite_hdr;
    height = *(int16 *)(sprite_hdr + 2);
    dst_off = row_idx * surface_pitch + col_offset;

    save_buf = malloc(width * height + 8);
    fd2_save_screen_block_to_buffer((uint32)save_buf, (uint32)width,
                                    (uint32)height, dst, dst_off, surface_pitch);
    fd2_blit_sprite_with_decoded_pixels(dst_off + dst, sprite_hdr, surface_pitch);
    return (uint32)save_buf;
}
