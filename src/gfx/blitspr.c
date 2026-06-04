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

/* ----------------------------------------------------------------
 * fd2_blit_sheet_sprite_at_offset @ 0x1685C (9 callers)
 *
 * Look up sprite #sprite_idx in a sprite-atlas sheet's offset table and
 * perform a raw header+pixel blit at dst.
 *
 * The atlas's 4-byte-per-entry offset table starts at sheet + 6. Entry
 * sprite_idx gives the byte offset (from sheet) of that sprite's
 * header, so:
 *
 *   sprite_addr = sheet + *(int32 *)(sheet + 6 + sprite_idx * 4);
 *
 * The resolved sprite (width word 0, height word 2, then raw pixels) is
 * then painted opaquely via fd2_blit_sprite_raw_with_header.
 *
 * Used 17x by fd2_assemble_dialog_frame_layered to compose a dialog box
 * from 17 tile sprites; also called by other panel/grid renderers.
 *
 * Args (cdecl, 4x uint32 on stack):
 *   dst        — destination base linear address
 *   dst_pitch  — destination row stride
 *   sheet      — sprite atlas base linear address
 *   sprite_idx — index into the sheet's offset table
 * ---------------------------------------------------------------- */
void fd2_blit_sheet_sprite_at_offset(uint32 dst, uint32 dst_pitch,
                                     uint32 sheet, uint32 sprite_idx)
{
    uint32 sprite_addr;

    sprite_addr = sheet + *(int32 *)(sheet + 6 + sprite_idx * 4);
    fd2_blit_sprite_raw_with_header(dst, sprite_addr, dst_pitch);
}

/* ----------------------------------------------------------------
 * fd2_blit_indexed_sprite_at_xy @ 0x16886 (11 callers)
 *
 * Thin wrapper around fd2_rle_blit_sprite that picks sub-sprite
 * `sprite_idx` from a sprite-atlas sheet and RLE-blits it with
 * transparent passthrough at (0, 0) within dst.
 *
 * The atlas's 4-byte-per-entry offset table starts at sheet + 6. Entry
 * sprite_idx gives the byte offset (from sheet) of that sprite's RLE
 * stream:
 *
 *   sprite_addr = sheet + *(int32 *)(sheet + 6 + sprite_idx * 4);
 *
 * The resolved stream is RLE-decoded and painted into dst at (0, 0)
 * with palette_op = 0xFFFFFFFF (-1), the transparent-passthrough mode.
 *
 * (The Ghidra-era name is misleading: this takes no xy coords; the RLE
 * blit always writes from (0, 0). The 4th param is sprite_idx, not an
 * x coordinate.)
 *
 * Used for compositing UI / battle-overlay icons that need RLE-decoded
 * transparency.
 *
 * Args (cdecl, 4x uint32 on stack):
 *   dst        — destination base linear address
 *   dst_pitch  — destination row stride
 *   sheet      — sprite atlas base linear address
 *   sprite_idx — index into the sheet's offset table
 * ---------------------------------------------------------------- */
void fd2_blit_indexed_sprite_at_xy(uint32 dst, uint32 dst_pitch,
                                   uint32 sheet, uint32 sprite_idx)
{
    uint32 sprite_addr;

    sprite_addr = sheet + *(int32 *)(sheet + 6 + sprite_idx * 4);
    fd2_rle_blit_sprite(sprite_addr, 0, 0, dst, dst_pitch, 0xFFFFFFFF);
}

/* ----------------------------------------------------------------
 * fd2_fill_screen_rect_with_byte @ 0x1F6EF (2 callers)
 *
 * Fill a solid (size-1) x (size-1) byte square directly into the
 * mode13h VGA framebuffer (0xA0000), no backbuffer:
 *
 *   row_ptr = 0xA0000 + y * 320 + x
 *   for row in 0..(size - 1):
 *       memset(row_ptr, color, size - 1)      // (size-1) bytes wide
 *       row_ptr += 320
 *
 * Both the row count and the per-row byte count are (size - 1); the
 * loop bound is a signed compare ((int)row < (int)(size - 1)), so
 * size 1 (and size 0) paint nothing.
 *
 * Used by fd2_open_tactical_overview_zoom to draw the per-unit colored
 * marker squares in the tactical overview (player=green / enemy=red /
 * NPC=blue), at two render points.
 *
 * Cdecl, 4 stack params; void return. The binary's __CHK(0x20)
 * stack-probe prologue is compiler-generated and omitted here.
 * ---------------------------------------------------------------- */
void fd2_fill_screen_rect_with_byte(uint32 x, uint32 y, uint32 color,
                                    uint32 size)
{
    uint32 row_ptr;
    uint32 row;

    row_ptr = y * 0x140 + 0xA0000 + x;
    for (row = 0; (int32)row < (int32)(size - 1); row = row + 1) {
        memset((void *)row_ptr, color, size - 1);
        row_ptr = row_ptr + 0x140;
    }
}
