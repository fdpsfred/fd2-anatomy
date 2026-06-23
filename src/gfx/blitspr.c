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
 * blit-state globals: graphics scratch state, all game-writable
 * (verified WRITE xrefs) -> mutable, zero-initialised. Definitions live
 * here in the using file (matches the house pattern for gfx-state globals).
 * ---------------------------------------------------------------- */
glyph_blit_state data_fd2_graphics_glyph_blit_state;      /* 0x627A3 */
uint16 data_fd2_graphics_sprite_mask_blit_width;          /* 0x6017B */
uint16 data_fd2_graphics_rle_blit_cur_width;              /* 0x627B4 */
uint16 data_fd2_graphics_rle_blit_remaining_rows;         /* 0x627B6 */
uint8  data_fd2_graphics_sprite_blit_scaler_loop_state[6]; /* 0x627BA */
uint16 data_fd2_graphics_sprite_blit_src_width;           /* 0x627C0 */
uint16 data_fd2_graphics_sprite_blit_src_height;          /* 0x627C2 */
uint16 data_fd2_graphics_sprite_blit_scale_num;           /* 0x627C4 */
uint16 data_fd2_graphics_sprite_blit_scale_den;           /* 0x627C6 */

/* @ 0x627C8 -- 16-entry per-row x-shift table for the chapter-background
 * shimmer / heat-haze effect. Read-only (no game WRITE xrefs -> const) by
 * fd2_blit_buffer_with_per_row_offset, indexed by a cyclic 0..15 counter to
 * displace each copied source row horizontally. */
const uint8 data_fd2_graphics_shimmer_offset_table_16b[16] = {
    2, 3, 3, 4, 4, 4, 3, 3, 2, 1, 1, 0, 0, 0, 1, 1
};

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
 * the shared epilogue at 0x22BBE) and thus returned, but every caller
 * discards the result: the save-under snapshot is restored / freed on a
 * separate path (e.g. via fd2_cleanup_dialog_sprite_buffer), so per call
 * the returned pointer is effectively leaked at the call site while the
 * function itself still returns it.
 *
 * The 8 callers are the combat-overlay and chapter-intro render paths:
 * fd2_render_combat_combatant_panels (VS panel, sprite 0x30),
 * fd2_render_phase_banner_frame / fd2_animate_phase_banner_slide_in /
 * fd2_animate_phase_banner_slide_out (turn banners),
 * fd2_animate_attack_hit_sequence, fd2_animate_combat_speech_bubbles,
 * fd2_run_full_turn_cycle, and fd2_load_save_and_init_engine
 * (chapter-intro slideshow).
 *
 * Args (cdecl, 6x uint32 on stack):
 *   sheet_base    -- sprite atlas base linear address
 *   dst           -- destination surface base linear address
 *   surface_pitch -- destination row stride
 *   col_offset    -- column byte offset within the destination row
 *   row_idx       -- destination row index
 *   sprite_idx    -- index into the sheet's offset table
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
 * from 17 tile sprites; also called by other panel/grid renderers
 * (9 callers total: the dialog assembler plus 8 stat / inventory / shop /
 * promote / spell-list / HP-bar panel renderers).
 *
 * Args (cdecl, 4x uint32 on stack):
 *   dst        -- destination base linear address
 *   dst_pitch  -- destination row stride
 *   sheet      -- sprite atlas base linear address
 *   sprite_idx -- index into the sheet's offset table
 *
 * The binary's __CHK(0x14) stack-probe prologue is compiler-generated
 * and omitted here.
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
 *   dst        -- destination base linear address
 *   dst_pitch  -- destination row stride
 *   sheet      -- sprite atlas base linear address
 *   sprite_idx -- index into the sheet's offset table
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

/* ----------------------------------------------------------------
 * fd2_blit_money_digit_sprite @ 0x2D620 (2 callers)
 *
 * Per-digit blit primitive for the slot-machine money-roller
 * animation. Copies a 9-row x 6-pixel money-digit sprite into the
 * destination buffer at row stride dst_stride.
 *
 * The sprite data lives in the chapter-intro sprite atlas
 * (data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, FDOTHER.DAT[0xD]
 * payload). The digit-sprite section starts at the byte offset stored
 * in atlas[+0xE]; a 4-byte section header is skipped, then sprite_idx
 * selects a 6-byte-wide row run:
 *
 *   src_row = atlas
 *             + *(int32 *)(atlas + 0xE)   // offset to digit-sprite section
 *             + 4                          // skip 4-byte section header
 *             + sprite_idx * 6             // sprite stride within row
 *
 * sprite_idx = digit_value * 9 + animation_frame, so 0..89 covers all
 * 10 digits x 9 rolling frames. Each of the 9 rows is a 6-byte
 * memmove; dst advances by dst_stride and src by 6 per row. The loop
 * bound is a signed compare (row < 9).
 *
 * Called 8 x 9 = 72 times per slot-machine rolling step by
 * fd2_animate_money_increment / fd2_animate_money_decrement.
 *
 * Args (cdecl, 3x uint32 on stack):
 *   dst_buf    — destination base linear address
 *   dst_stride — bytes to advance dst per row
 *   sprite_idx — digit_value * 9 + animation_frame
 *
 * The binary's __CHK(0x20) stack-probe prologue is compiler-generated
 * and omitted here.
 * ---------------------------------------------------------------- */
void fd2_blit_money_digit_sprite(uint32 dst_buf, uint32 dst_stride,
                                 uint32 sprite_idx)
{
    uint32 src_row;
    uint32 row;

    src_row = data_fd2_ui_menu_screen_sprite_atlas_buf_ptr
              + *(int32 *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0xE)
              + 4 + sprite_idx * 6;
    for (row = 0; (int32)row < 9; row = row + 1) {
        memmove((void *)dst_buf, (void *)src_row, 6);
        dst_buf += dst_stride;
        src_row += 6;
    }
}

/* ----------------------------------------------------------------
 * fd2_blit_palette_remap_with_sprite_mask @ 0x4E445 (1 caller)
 *
 * In-place palette-remap blit gated by a 1-byte-per-pixel sprite
 * mask. Walks the mask row-by-row; for each non-zero mask byte it
 * replaces the underlying dst pixel with remap_table[dst_pixel]:
 *
 *   width  = sprite_mask[0]            // width header word
 *   height = sprite_mask[1]            // height header word
 *   mask   = sprite_mask + 4 bytes     // skip the 4-byte header
 *   for row in 0..height-1:
 *       for col in 0..width-1:
 *           if (*mask++ != 0): dst[col] = remap_table[dst[col]]
 *       dst += stride - width          // step to next row
 *
 * The mask pointer advances continuously across all rows (it is not
 * reset per row); only the column counter is reloaded each row. The
 * remap_table base is a 256-entry palette translation table, indexed
 * by the existing dst pixel value.
 *
 * Used for cursor highlights, selection overlays and status-indicator
 * outlines -- anywhere a 1-byte mask selects which pixels of the
 * underlying frame are recolored. The sole caller is the AoE-spell
 * caster portrait radial scatter effect.
 *
 * Side effect: width is written into the global
 * data_fd2_graphics_sprite_mask_blit_width (0x6017B); the per-row
 * column counter is then reloaded from that global each row, matching
 * the binary's word store / word reload.
 *
 * Args (cdecl, 4x on stack; void return):
 *   dst         -- destination base linear address (byte *)
 *   sprite_mask -- mask source: ushort width, ushort height, then
 *                  width*height mask bytes
 *   stride      -- destination row stride in bytes
 *   remap_table -- 256-entry palette translation table base address
 * ---------------------------------------------------------------- */
void fd2_blit_palette_remap_with_sprite_mask(uint8 *dst, uint16 *sprite_mask,
                                             uint32 stride, uint32 remap_table)
{
    uint32 width;
    uint32 col_remain;
    uint16 row_remain;
    uint8 *mask_ptr;
    uint32 row_advance;

    data_fd2_graphics_sprite_mask_blit_width = *sprite_mask;
    width = data_fd2_graphics_sprite_mask_blit_width;
    row_advance = stride - width;
    mask_ptr = (uint8 *)(sprite_mask + 2);
    row_remain = sprite_mask[1];
    do {
        col_remain = data_fd2_graphics_sprite_mask_blit_width;
        do {
            if (*mask_ptr != 0) {
                *dst = *(uint8 *)(remap_table + *dst);
            }
            mask_ptr++;
            dst++;
            col_remain--;
        } while (col_remain != 0);
        dst += row_advance;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_rle_blit_sprite @ 0x4E63D (20 callers)
 *
 * THE sprite drawing function -- every visible non-tile graphic in the
 * game (characters, portraits, spells, UI sprites) is painted here.
 * Decodes a custom RLE stream and writes pixels to the destination
 * buffer with one of three palette operations.
 *
 * Stream header: the first two 16-bit words are width and height (the
 * height is the row count). The command byte stream begins 4 bytes in.
 *
 * State globals (written at entry, reloaded across iterations to match
 * the binary's word store / word reload):
 *   data_fd2_graphics_rle_blit_cur_width      (0x627B4) row width
 *   data_fd2_graphics_rle_blit_remaining_rows (0x627B6) rows left
 *
 * RLE command byte (high 2 bits select the op, low 6 bits are len-1;
 * len = (cmd & 0x3F) + 1):
 *   0b00 (RLE fill):        fill len dst pixels with the NEXT byte
 *   0b01 (stretched fill):  take the NEXT byte, write it to len dst
 *                           pixels spaced every other pixel (dst += 2),
 *                           consuming 2*len columns of row width
 *   0b10 (literal copy):    copy len LITERAL bytes from the stream
 *   0b11 (skip):            advance dst by len pixels (transparent)
 *
 * After each command, len (or 2*len for stretched) is consumed from the
 * row width counter; when it reaches 0, dst advances to the next row
 * (dst += stride - width) and the remaining-row counter is decremented.
 *
 * palette_op modes:
 *   0xFFFFFFFF -- PASSTHROUGH: bytes copied unchanged. Direct paste.
 *   (ushort) > 0xFF -- TRANSLUCENT OVERLAY:
 *       color_base   = palette_op & 0xFF        (palette base offset)
 *       rotation_key = (palette_op >> 8) & 0xFF (color rotation key)
 *       out = ((src + rotation_key) & 7) + color_base
 *       Used for spell-effect tints and chapter palette swaps.
 *   (ushort) <= 0xFF -- SILHOUETTE FILL: every opaque pixel becomes the
 *       single fill byte (palette_op & 0xFF); source pixel bytes are
 *       skipped. Used for shadows, flash silhouettes, dying-char
 *       white-out.
 *
 * The "stretched" and "fill" ops still advance the source stream past
 * their (now-ignored) source byte(s) in silhouette mode, exactly as in
 * passthrough/translucent, so the stream stays in sync across modes.
 *
 * Args (cdecl, 6x on stack; void return):
 *   rle_stream -- RLE stream base (ushort width, ushort height, then
 *                 command bytes)
 *   dst_x      -- destination column offset (added to the start address)
 *   dst_y      -- destination row offset (scaled by stride)
 *   dst_buf    -- destination base linear address
 *   stride     -- destination row stride in bytes
 *   palette_op -- palette operation selector (see modes above)
 * ---------------------------------------------------------------- */
void fd2_rle_blit_sprite(uint32 rle_stream, int32 dst_x, int32 dst_y,
                         uint32 dst_buf, int32 stride, uint32 palette_op)
{
    uint8 *stream;
    uint32 dst_iter;
    uint32 row_advance;
    uint16 x_remain;
    uint16 delta_x;
    uint8 cmd;
    uint8 len;
    uint8 src_byte;
    uint8 fill_byte;
    uint8 color_base;
    uint8 rotation_key;
    uint32 run_iter;

    data_fd2_graphics_rle_blit_cur_width = *(uint16 *)rle_stream;
    data_fd2_graphics_rle_blit_remaining_rows = *(uint16 *)(rle_stream + 2);
    stream = (uint8 *)(rle_stream + 4);
    dst_iter = dst_buf + dst_y * stride + dst_x;
    row_advance = stride - (uint32)data_fd2_graphics_rle_blit_cur_width;
    x_remain = data_fd2_graphics_rle_blit_cur_width;

    if (palette_op == 0xFFFFFFFF) {
        /* PASSTHROUGH: copy source bytes unchanged. */
        do {
            do {
                cmd = *stream++;
                if ((int8)cmd < 0) {
                    if ((int8)(cmd << 1) < 0) {
                        /* skip (transparent) */
                        len = (cmd & 0x3F) + 1;
                        dst_iter += len;
                        delta_x = -(uint16)len;
                    } else {
                        /* literal copy of len bytes */
                        len = (cmd & 0x3F) + 1;
                        delta_x = -(uint16)len;
                        for (run_iter = len; run_iter != 0; run_iter--) {
                            *(uint8 *)dst_iter = *stream++;
                            dst_iter++;
                        }
                    }
                } else {
                    if ((int8)(cmd << 1) < 0) {
                        /* stretched fill: 1 byte -> len every-other pixels */
                        len = (cmd & 0x3F) + 1;
                        delta_x = (uint16)len * -2;
                        src_byte = *stream++;
                        for (run_iter = len; run_iter != 0; run_iter--) {
                            *(uint8 *)(dst_iter + 1) = src_byte;
                            dst_iter += 2;
                        }
                    } else {
                        /* RLE fill of len bytes with the next byte */
                        len = (cmd & 0x3F) + 1;
                        delta_x = -(uint16)len;
                        src_byte = *stream++;
                        for (run_iter = len; run_iter != 0; run_iter--) {
                            *(uint8 *)dst_iter = src_byte;
                            dst_iter++;
                        }
                    }
                }
                x_remain = x_remain + delta_x;
            } while (x_remain != 0);
            dst_iter += row_advance;
            data_fd2_graphics_rle_blit_remaining_rows--;
            x_remain = data_fd2_graphics_rle_blit_cur_width;
        } while (data_fd2_graphics_rle_blit_remaining_rows != 0);
        return;
    }

    if ((uint16)palette_op > 0xFF) {
        /* TRANSLUCENT OVERLAY: out = ((src + rot) & 7) + base. */
        color_base = (uint8)(palette_op & 0xFF);
        rotation_key = (uint8)((palette_op >> 8) & 0xFF);
        do {
            do {
                cmd = *stream++;
                if ((int8)cmd < 0) {
                    if ((int8)(cmd << 1) < 0) {
                        len = (cmd & 0x3F) + 1;
                        dst_iter += len;
                        delta_x = -(uint16)len;
                    } else {
                        len = (cmd & 0x3F) + 1;
                        delta_x = -(uint16)len;
                        for (run_iter = len; run_iter != 0; run_iter--) {
                            src_byte = *stream++;
                            *(uint8 *)dst_iter =
                                ((src_byte + rotation_key) & 7) + color_base;
                            dst_iter++;
                        }
                    }
                } else {
                    if ((int8)(cmd << 1) < 0) {
                        len = (cmd & 0x3F) + 1;
                        delta_x = (uint16)len * -2;
                        src_byte = *stream++;
                        for (run_iter = len; run_iter != 0; run_iter--) {
                            *(uint8 *)(dst_iter + 1) =
                                ((src_byte + rotation_key) & 7) + color_base;
                            dst_iter += 2;
                        }
                    } else {
                        len = (cmd & 0x3F) + 1;
                        delta_x = -(uint16)len;
                        src_byte = *stream++;
                        for (run_iter = len; run_iter != 0; run_iter--) {
                            *(uint8 *)dst_iter =
                                ((src_byte + rotation_key) & 7) + color_base;
                            dst_iter++;
                        }
                    }
                }
                x_remain = x_remain + delta_x;
            } while (x_remain != 0);
            dst_iter += row_advance;
            data_fd2_graphics_rle_blit_remaining_rows--;
            x_remain = data_fd2_graphics_rle_blit_cur_width;
        } while (data_fd2_graphics_rle_blit_remaining_rows != 0);
        return;
    }

    /* SILHOUETTE FILL: every opaque pixel becomes the single fill byte;
     * source pixel bytes are still skipped to keep the stream in sync. */
    fill_byte = (uint8)(palette_op & 0xFF);
    do {
        do {
            cmd = *stream++;
            if ((int8)cmd < 0) {
                if ((int8)(cmd << 1) < 0) {
                    len = (cmd & 0x3F) + 1;
                    dst_iter += len;
                    delta_x = -(uint16)len;
                } else {
                    /* literal run: skip len source bytes, fill len pixels */
                    len = (cmd & 0x3F) + 1;
                    delta_x = -(uint16)len;
                    stream += len;
                    for (run_iter = len; run_iter != 0; run_iter--) {
                        *(uint8 *)dst_iter = fill_byte;
                        dst_iter++;
                    }
                }
            } else {
                if ((int8)(cmd << 1) < 0) {
                    /* stretched: skip 1 source byte, fill every-other pixel */
                    len = (cmd & 0x3F) + 1;
                    delta_x = (uint16)len * -2;
                    stream++;
                    for (run_iter = len; run_iter != 0; run_iter--) {
                        *(uint8 *)(dst_iter + 1) = fill_byte;
                        dst_iter += 2;
                    }
                } else {
                    /* RLE fill: skip 1 source byte, fill len pixels */
                    len = (cmd & 0x3F) + 1;
                    delta_x = -(uint16)len;
                    stream++;
                    for (run_iter = len; run_iter != 0; run_iter--) {
                        *(uint8 *)dst_iter = fill_byte;
                        dst_iter++;
                    }
                }
            }
            x_remain = x_remain + delta_x;
        } while (x_remain != 0);
        dst_iter += row_advance;
        data_fd2_graphics_rle_blit_remaining_rows--;
        x_remain = data_fd2_graphics_rle_blit_cur_width;
    } while (data_fd2_graphics_rle_blit_remaining_rows != 0);
}

/* ----------------------------------------------------------------
 * fd2_scroll_buffer_block_with_wrap @ 0x4E809 (2 callers)
 *
 * Vertical-scroll block copy with a double (column + row) wrap. Copies
 * a 0xC0-row x 0x138-col window from src_buf into dst_buf, both starting
 * at the inset origin +0x504 (= 4 rows * 0x140 stride + 4 cols).
 * wrap_param's low byte doubles as the column-wrap modulus and the
 * row-wrap counter, producing a circular-in-both-dimensions sliding
 * window read:
 *
 *   src_row  = src_buf + 0x504
 *   dst_iter = dst_buf + 0x504
 *   for row in 0..0xC0 (192 rows):
 *       src_iter = src_row
 *       reset col-wrap counter to (byte)wrap_param
 *       for col in 0..0x138 (312 cols):
 *           *dst_iter++ = *src_iter
 *           if (--col_wrap_iter == 0):
 *               src_iter += wrap_param          // column wrap point
 *               col_wrap_iter = (byte)wrap_param
 *       dst_iter += 8                            // one-past-last -> +9 from
 *                                                // last byte = full 0x140 stride
 *       if (--wrap_remain == 0):
 *           wrap_remain = (byte)wrap_param
 *           src_row += (uint16)(0x140 * (uint16)wrap_param)  // row wrap point
 *
 * Widths mirror the binary's register usage: the row-wrap advance is a
 * 16-bit MUL kept to BP (low 16), the column wrap adds the full 32-bit
 * wrap_param, and the three counters (row/row-wrap/col-wrap) are byte
 * registers. The +8 inter-row step applies to dst_iter while it already
 * points one past the last written byte, so consecutive rows are exactly
 * 0x140 apart (312 written + 8 = 320).
 *
 * Driven by the phase-banner slide transitions: the slide-in caller ramps
 * wrap_param 1..16, the slide-out caller ramps it 16..1, producing the
 * vertical text-slide effect for the「PLAYER TURN / ENEMY TURN」banners.
 *
 * Args (cdecl, 3x on stack; void return):
 *   wrap_param -- low byte = column wrap modulus + row wrap counter
 *   dst_buf    -- destination buffer base
 *   src_buf    -- source buffer base (circular layout in both dims)
 * ---------------------------------------------------------------- */
void fd2_scroll_buffer_block_with_wrap(uint32 wrap_param, void *dst_buf,
                                       void *src_buf)
{
    uint8 *src_row;
    uint8 *src_iter;
    uint8 *dst_iter;
    uint16 row_wrap_advance;
    uint8 row_remain;
    uint8 wrap_remain;
    uint8 col_wrap_iter;
    uint16 col_remain;

    src_row = (uint8 *)src_buf + 0x504;
    dst_iter = (uint8 *)dst_buf + 0x504;
    row_wrap_advance = (uint16)(0x140 * (uint16)wrap_param);
    row_remain = 0xC0;
    wrap_remain = (uint8)wrap_param;
    do {
        col_remain = 0x138;
        src_iter = src_row;
        col_wrap_iter = (uint8)wrap_param;
        do {
            *dst_iter++ = *src_iter;
            col_wrap_iter--;
            if (col_wrap_iter == 0) {
                src_iter += wrap_param;
                col_wrap_iter = (uint8)wrap_param;
            }
            col_remain--;
        } while (col_remain != 0);
        dst_iter += 8;
        wrap_remain--;
        if (wrap_remain == 0) {
            wrap_remain = (uint8)wrap_param;
            src_row += row_wrap_advance;
        }
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_decode_dialog_pixel_byte @ 0x4E916 (3 callers)
 *
 * Per-pixel state-machine decoder for the dialog-portrait sprite
 * encoded byte stream. Returns the next pixel together with the
 * remaining run length, packed into one 16-bit value:
 *
 *   bits 15..8 = run_remain (how many MORE pixels reuse this value
 *                after the one being returned now)
 *   bits  7..0 = pixel value to write now
 *
 * The caller chains calls, feeding each returned value back in as the
 * next `state`, and writes the low byte (pixel) per destination pixel.
 *
 * Binary ABI: state is passed in/out via AX, and the stream cursor is
 * held in ESI and advanced by LODSB across calls (so the cursor must
 * persist between calls). To preserve that side effect in portable C,
 * the cursor is modeled here as an in/out pointer-to-pointer: each
 * stream byte consumed advances *stream by one, exactly as LODSB does.
 *
 * Algorithm:
 *   if (run_remain != 0):              -- still inside a run
 *       return ((run_remain - 1) << 8) | pixel   -- no stream read
 *   b = *(*stream)++                   -- read next stream byte
 *   if (b <= 0xC0):
 *       return b                       -- direct pixel, run_remain = 0
 *   else:                              -- b >= 0xC1: run marker
 *       next = *(*stream)++            -- read run pixel value
 *       return ((b - 0xC1) << 8) | next
 *
 * Stream encoding:
 *   bytes 0x00..0xC0 : direct pixel write (193 distinct values).
 *   bytes 0xC1..0xFF : start a run of (b - 0xC1) + 1 extra pixels of
 *                      the following byte's value (so the run paints
 *                      (b - 0xC1) + 2 pixels total of that value).
 *                      Used to compress the large background-color
 *                      runs in portrait sprites.
 *
 * (b - 0xC1) and (b + 0x3F) are the same value for a byte; the binary
 * computes it with SUB AH,0xC1, mirrored here.
 *
 * No stack frame in the binary; register-only.
 *
 * Args:
 *   state  -- packed (run_remain << 8) | pixel from the previous call;
 *             pass 0 on the first call of a row run
 *   stream -- in/out pointer to the stream cursor; advanced past every
 *             byte consumed
 * Returns: packed (run_remain << 8) | pixel for the next pixel.
 * ---------------------------------------------------------------- */
uint16 fd2_decode_dialog_pixel_byte(uint16 state, uint8 **stream)
{
    uint8 run_remain;
    uint8 b;
    uint8 next;

    run_remain = (uint8)(state >> 8);
    if (run_remain != 0) {
        return (uint16)(((run_remain - 1) << 8) | (state & 0xFF));
    }

    b = *(*stream)++;
    if (b <= 0xC0) {
        return (uint16)b;
    }

    next = *(*stream)++;
    return (uint16)(((b - 0xC1) << 8) | next);
}

/* ----------------------------------------------------------------
 * fd2_rle_blit_with_palette_remap @ 0x4E583 (4 call sites)
 *
 * RLE sprite decoder that remaps every painted pixel through a
 * 256-entry palette translation table: out = remap[src]. Same custom
 * RLE stream format and 4-mode command encoding as fd2_rle_blit_sprite
 * (above), but the sprite width comes from the stream header (not a
 * hardcoded tile size) and painting starts at an arbitrary (dst_x,
 * dst_y) offset within dst_buf.
 *
 * Stream header: the first two 16-bit words are width and height (the
 * height is the row count). The command byte stream begins 4 bytes in.
 *
 * State globals (written at entry, reloaded across iterations to match
 * the binary's word store / word reload):
 *   data_fd2_graphics_rle_blit_cur_width      (0x627B4) row width
 *   data_fd2_graphics_rle_blit_remaining_rows (0x627B6) rows left
 *
 * RLE command byte (high 2 bits select the op, low 6 bits are len-1;
 * len = (cmd & 0x3F) + 1):
 *   0b00 (RLE fill):       take the NEXT byte, write remap[byte] to len
 *                          dst pixels
 *   0b01 (stretched fill): take the NEXT byte, write remap[byte] to len
 *                          dst pixels spaced every other pixel
 *                          (dst += 2), consuming 2*len row columns
 *   0b10 (literal copy):   copy len bytes from the stream, each painted
 *                          as remap[byte]
 *   0b11 (skip):           advance dst by len pixels (transparent)
 *
 * After each command, len (or 2*len for stretched) is consumed from the
 * row-width counter; when it reaches 0, dst advances to the next row
 * (dst += stride - width) and the remaining-row counter is decremented.
 *
 * Used by the special-attack / spell-cast cinematic paths
 * (fd2_play_spell_cast_sequence, fd2_play_figani_animation_loop) to
 * recolor the backdrop and name-banner sprites per animation frame; the
 * remap_table is selected from the per-spell palette-remap table at
 * data_fd2_tile_anim_table_base.
 *
 * Args (cdecl, 6x on stack; void return):
 *   rle_stream   -- RLE stream base (ushort width, ushort height, then
 *                   command bytes)
 *   dst_x        -- destination column offset (added to the start addr)
 *   dst_y        -- destination row offset (scaled by stride)
 *   dst_buf      -- destination base linear address
 *   stride       -- destination row stride in bytes
 *   palette_remap -- 256-entry palette translation table base address,
 *                    indexed by the source pixel byte
 * ---------------------------------------------------------------- */
void fd2_rle_blit_with_palette_remap(uint16 *rle_stream, int32 dst_x,
                                     int32 dst_y, int32 dst_buf, int32 stride,
                                     int32 palette_remap)
{
    uint8 *stream;
    uint32 dst_iter;
    uint32 row_advance;
    uint32 width;
    uint16 x_remain;
    uint16 delta_x;
    uint8 cmd;
    uint8 len;
    uint8 src_byte;
    uint32 run_iter;

    data_fd2_graphics_rle_blit_cur_width = *rle_stream;
    data_fd2_graphics_rle_blit_remaining_rows = rle_stream[1];
    stream = (uint8 *)(rle_stream + 2);
    dst_iter = dst_buf + dst_y * stride + dst_x;
    width = (uint32)data_fd2_graphics_rle_blit_cur_width;
    row_advance = stride - width;
    x_remain = data_fd2_graphics_rle_blit_cur_width;

    do {
        do {
            cmd = *stream++;
            if ((int8)cmd < 0) {
                if ((int8)(cmd << 1) < 0) {
                    /* skip (transparent) */
                    len = (cmd & 0x3F) + 1;
                    dst_iter += len;
                    delta_x = -(uint16)len;
                } else {
                    /* literal copy: len bytes, each painted remap[byte] */
                    len = (cmd & 0x3F) + 1;
                    delta_x = -(uint16)len;
                    for (run_iter = len; run_iter != 0; run_iter--) {
                        src_byte = *stream++;
                        *(uint8 *)dst_iter =
                            *(uint8 *)(palette_remap + src_byte);
                        dst_iter++;
                    }
                }
            } else {
                if ((int8)(cmd << 1) < 0) {
                    /* stretched fill: 1 byte -> remap, every-other pixel */
                    len = (cmd & 0x3F) + 1;
                    delta_x = (uint16)len * -2;
                    src_byte = *stream++;
                    for (run_iter = len; run_iter != 0; run_iter--) {
                        *(uint8 *)(dst_iter + 1) =
                            *(uint8 *)(palette_remap + src_byte);
                        dst_iter += 2;
                    }
                } else {
                    /* RLE fill: 1 byte -> remap, written len times */
                    len = (cmd & 0x3F) + 1;
                    delta_x = -(uint16)len;
                    src_byte = *stream++;
                    for (run_iter = len; run_iter != 0; run_iter--) {
                        *(uint8 *)dst_iter =
                            *(uint8 *)(palette_remap + src_byte);
                        dst_iter++;
                    }
                }
            }
            x_remain = x_remain + delta_x;
        } while (x_remain != 0);
        dst_iter += row_advance;
        data_fd2_graphics_rle_blit_remaining_rows--;
        x_remain = data_fd2_graphics_rle_blit_cur_width;
    } while (data_fd2_graphics_rle_blit_remaining_rows != 0);
}

/* ----------------------------------------------------------------
 * fd2_restore_block_loop @ 0x4E954 (1 caller)
 *
 * Inner copy loop for screen-block restore: copies a width x height
 * pixel block from a contiguous source buffer to the destination,
 * advancing the destination by stride per row. The source pointer runs
 * continuously across all rows (the saved block is stored as
 * width*height contiguous bytes), while the destination resets to the
 * row start and then steps by stride:
 *
 *   row_remain = data_fd2_graphics_rle_blit_remaining_rows   // 0x627B6 (height)
 *   do:
 *       col_remain = data_fd2_graphics_rle_blit_cur_width     // 0x627B4 (width)
 *       copy col_remain bytes  src -> dst   (src and dst both advance)
 *       dst = row_start + stride                              // next row
 *       row_remain--
 *   while (row_remain != 0)
 *
 * The width global is re-read at the top of every row (the binary loops
 * back to the MOV CX,[0x627B4] reload), so the per-row byte count tracks
 * the global if a caller were to change it mid-restore; the height global
 * is read once into the row counter. The inner span is a REP MOVSB, here
 * a per-row memmove of col_remain bytes.
 *
 * In the binary the source/destination/stride arrive in registers
 * (ESI=src, EDI=dst, EBP=stride) seeded by the sole caller
 * fd2_restore_screen_block_from_buffer @ 0x4E92C after it reads the
 * saved-block header (width/height words) into the two globals; they are
 * exposed here as explicit parameters to keep the function portable.
 *
 * Args (register-carried in the binary; void return):
 *   dst    -- destination base linear address (row start of the block)
 *   src    -- source pixel data (contiguous width*height bytes)
 *   stride -- bytes to advance the destination per row
 * ---------------------------------------------------------------- */
void fd2_restore_block_loop(uint32 dst, uint32 src, uint32 stride)
{
    uint32 col_remain;
    uint16 row_remain;

    row_remain = data_fd2_graphics_rle_blit_remaining_rows;
    do {
        col_remain = data_fd2_graphics_rle_blit_cur_width;
        memmove((void *)dst, (void *)src, col_remain);
        src += col_remain;
        dst += stride;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_save_block_loop @ 0x4E9A0 (1 caller)
 *
 * Inner copy loop for screen-block save: the mirror of
 * fd2_restore_block_loop @ 0x4E954 with the source/destination roles
 * swapped. Copies a width x height pixel block from the destination
 * (a live screen region) into a contiguous backup buffer, advancing the
 * SOURCE by stride per row. The destination (backup buffer) runs
 * continuously across all rows (the saved block is packed as
 * width*height contiguous bytes), while the source resets to the row
 * start and then steps by stride:
 *
 *   row_remain = data_fd2_graphics_rle_blit_remaining_rows   // 0x627B6 (height)
 *   do:
 *       col_remain = data_fd2_graphics_rle_blit_cur_width     // 0x627B4 (width)
 *       copy col_remain bytes  src -> dst   (src and dst both advance)
 *       src = row_start + stride                              // next source row
 *       row_remain--
 *   while (row_remain != 0)
 *
 * The width global is re-read at the top of every row (the binary loops
 * back to the MOV CX,[0x627B4] reload), so the per-row byte count tracks
 * the global if a caller were to change it mid-save; the height global is
 * read once into the row counter. The inner span is a REP MOVSB, here a
 * per-row memmove of col_remain bytes.
 *
 * In the binary the source/destination/stride arrive in registers
 * (ESI=src, EDI=dst, EBP=src_stride) seeded by the sole caller
 * fd2_save_screen_block_to_buffer @ 0x4E96F after it writes the
 * width/height words into the two globals and the buffer header; they are
 * exposed here as explicit parameters to keep the function portable.
 *
 * Args (register-carried in the binary; void return):
 *   src    -- source pixel data (live screen region, strided per row)
 *   dst    -- destination base linear address (contiguous backup buffer,
 *             packed width*height bytes)
 *   stride -- bytes to advance the source per row
 * ---------------------------------------------------------------- */
void fd2_save_block_loop(uint32 src, uint32 dst, uint32 stride)
{
    uint32 col_remain;
    uint16 row_remain;

    row_remain = data_fd2_graphics_rle_blit_remaining_rows;
    do {
        col_remain = data_fd2_graphics_rle_blit_cur_width;
        memmove((void *)dst, (void *)src, col_remain);
        dst += col_remain;
        src += stride;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_blit_sprite_raw_with_header @ 0x4E9BB (1 caller)
 *
 * Generic raw-sprite blit (uncompressed, no transparency): copies every
 * pixel including zero. The sprite header carries width/height as the
 * first two 16-bit words, immediately followed by width*height raw pixel
 * bytes in row-major order:
 *
 *   width      = *(uint16 *)sprite_hdr
 *   height     = *(uint16 *)(sprite_hdr + 2)
 *   src_pixels = sprite_hdr + 4            // skip the 4-byte header
 *   for row in 0..height-1:
 *       copy width bytes  src_pixels -> dst   (contiguous, REP MOVSB)
 *       src_pixels += width
 *       dst        += stride               // next dst row
 *
 * The source pointer runs continuously across all rows (the pixels are
 * stored contiguously); the destination resets to the row base each row
 * and steps by stride. The row loop is a post-test do/while, so a height
 * of 0 still paints one row (matching the binary's DEC DX; JNZ tail).
 *
 * Smallest variant of the blit family: used for fixed-size opaque sprites
 * where every pixel is meaningful (no transparency, no RLE). Reached via
 * fd2_blit_sheet_sprite_at_offset, which resolves a sprite-atlas entry
 * and paints it opaquely.
 *
 * The binary leaves EAX untouched (PUSHAD/POPAD wrap the whole body), so
 * the "return in_EAX" of the decompiler is a pass-through signature
 * artifact; the sole caller discards the result, so this is a void
 * function. Hand-written asm leaf: no __CHK probe, no CALLs.
 *
 * Args (cdecl, 3x uint32 on stack; void return):
 *   dst        -- destination base linear address (row base)
 *   sprite_hdr -- sprite header pointer (width word 0, height word 2,
 *                 then width*height raw pixel bytes)
 *   stride     -- bytes to advance dst per row
 * ---------------------------------------------------------------- */
void fd2_blit_sprite_raw_with_header(uint32 dst, uint32 sprite_hdr,
                                     uint32 stride)
{
    uint32 src_pixels;
    uint32 width;
    uint16 row_remain;

    width = *(uint16 *)sprite_hdr;
    src_pixels = sprite_hdr + 4;
    row_remain = *(uint16 *)(sprite_hdr + 2);
    do {
        memmove((void *)dst, (void *)src_pixels, width);
        src_pixels += width;
        dst += stride;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_blit_sprite_with_stride_loop @ 0x4E9FF (1 caller)
 *
 * Inner sprite blit with a custom destination stride and transparent
 * zero pixels. The mirror of fd2_blit_sprite_raw_with_header @ 0x4E9BB,
 * but skips zero bytes (transparency) instead of copying them, and takes
 * the row stride from a global rather than a register:
 *
 *   width      = *(uint16 *)sprite_hdr
 *   src_pixels = sprite_hdr + 4            // skip the 4-byte header
 *   row_remain = *(uint16 *)(sprite_hdr + 2)
 *   stride     = data_fd2_graphics_glyph_blit_state.wPitch   // 0x627A3
 *   for row in 0..height-1:
 *       dst_iter = dst_buf
 *       for col in 0..width-1:
 *           if (*src_pixels != 0): *dst_iter = *src_pixels
 *           src_pixels++; dst_iter++
 *       dst_buf += stride                  // next dst row
 *
 * The source pointer runs continuously across all rows (the pixels are
 * stored contiguously); the destination resets to the row base each row
 * (PUSH/POP EDI) and steps by the stride global. Both loops are post-test
 * do/while, so a height of 0 still paints one row and a width of 0 wraps
 * the column counter, matching the binary's LOOP / DEC DX; JNZ tails.
 *
 * In the binary the destination and sprite-header pointers arrive in
 * registers (EDI=dst_buf, ESI=sprite_hdr) seeded by the sole caller
 * fd2_blit_sprite_with_stride_setup @ 0x4E9E4 after it stores the stride
 * into the wPitch global; they are exposed here as explicit parameters to
 * keep the function portable. Hand-written asm leaf: no __CHK probe, no
 * CALLs; void return.
 *
 * Args (register-carried in the binary; void return):
 *   dst_buf    -- destination base linear address (row base)
 *   sprite_hdr -- sprite header pointer (width word 0, height word 2,
 *                 then width*height raw pixel bytes; zero = transparent)
 * ---------------------------------------------------------------- */
void fd2_blit_sprite_with_stride_loop(uint32 dst_buf, uint32 sprite_hdr)
{
    uint32 src_pixels;
    uint32 dst_iter;
    uint32 col_remain;
    uint32 stride;
    uint16 width;
    uint16 row_remain;

    width = *(uint16 *)sprite_hdr;
    src_pixels = sprite_hdr + 4;
    row_remain = *(uint16 *)(sprite_hdr + 2);
    stride = data_fd2_graphics_glyph_blit_state.wPitch;
    do {
        dst_iter = dst_buf;
        col_remain = width;
        do {
            if (*(uint8 *)src_pixels != 0) {
                *(uint8 *)dst_iter = *(uint8 *)src_pixels;
            }
            src_pixels++;
            dst_iter++;
            col_remain--;
        } while (col_remain != 0);
        dst_buf += stride;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_blit_glyph_2bpp_with_outline @ 0x4EA2A (2 call sites)
 *
 * NOTE: the Ghidra-era name says "2bpp" but the font sheet is in fact
 * 1bpp -- 0x20 bytes per glyph = 16 rows x 2 bytes = 16 bits/row at one
 * bit per pixel. The name is kept here to stay byte-identical with the
 * current Ghidra symbol and the routing table; it is a misnomer.
 *
 * Renders a 16x16 monochrome glyph from a 1bpp font sheet into dst with
 * an optional solid background fill and a 1-pixel L-shaped drop shadow.
 * The per-glyph text renderer for dialog text.
 *
 * All 7 arguments are first copied into the shared global render-state
 * struct data_fd2_graphics_glyph_blit_state (0x627A3), then the body
 * re-reads them from that struct (matching the binary, which spills the
 * args to the struct and reloads them). The struct also lets later
 * callers update a single field (e.g. fd2_blit_sprite_with_stride_loop
 * reuses just wPitch as its stride).
 *
 * Pipeline:
 *   1. If bg_color != 0: fill a 16x16 box at dst with bg_color (16 rows
 *      x 4 dword writes = 16 bytes/row), advancing by pitch each row.
 *   2. If glyph_idx != 10 (whitespace skip):
 *        glyph_row = font_data + glyph_idx * 0x20   // 16 rows x 2 bytes
 *        for each of 16 rows:
 *          row_bits = byte-swapped ushort at glyph_row  // big-endian:
 *                     first stream byte = left 8 pixels
 *          for each of 16 cols (MSB first via shift-left of the sign bit):
 *            if bit set:
 *              *dst_pixel               = fill_color    // glyph body
 *              *(dst_pixel + pitch - 1) = outline_color // shadow below-left
 *              *(dst_pixel + pitch)     = outline_color // shadow below
 *            dst_pixel++
 *          glyph_row += 2
 *          dst_row   += pitch
 *
 * Each "on" bit becomes a fill_color pixel with a 1-pixel L-shaped drop
 * shadow at its lower-left. glyph_idx 10 paints nothing (the space
 * glyph), but a non-zero bg fill still runs for it.
 *
 * The binary wraps the whole body in PUSHAD/POPAD and leaves EAX at its
 * entry value (RET, no return set), so the decompiler's "return in_EAX"
 * is a pass-through artifact; both call sites in fd2_display_dialog_scene
 * discard it, so this is a void function.
 *
 * pitch is read as the low 16 bits of its stack slot, and fill_color /
 * outline_color / bg_color as the low byte of theirs; all 7 args occupy a
 * full dword on the stack (cdecl, ADD ESP,0x1C cleanup at both sites).
 *
 * Args (cdecl, 7x on stack; void return):
 *   font_data     -- 1bpp font sheet base (0x20 bytes per glyph)
 *   glyph_idx     -- index into the font sheet (10 = space, paints nothing)
 *   dst_buf       -- destination base linear address
 *   pitch         -- destination row stride in bytes (low 16 bits used)
 *   fill_color    -- palette index of the glyph body (low byte used)
 *   outline_color -- palette index of the drop shadow (low byte used)
 *   bg_color      -- background fill before the glyph; 0 = skip bg fill
 *                    (low byte used)
 * ---------------------------------------------------------------- */
void fd2_blit_glyph_2bpp_with_outline(uint32 font_data, uint32 glyph_idx,
                                      uint32 dst_buf, uint32 pitch,
                                      uint32 fill_color, uint32 outline_color,
                                      uint16 bg_color)
{
    uint32 pitch_val;
    uint32 bg_dword;
    uint32 dst_row;
    uint32 dst_pixel;
    uint32 glyph_row;
    uint32 fill_iter;
    uint16 row_bits;
    uint8 col_remain;
    uint8 row_remain;
    uint8 fill_byte;
    uint8 outline_byte;

    data_fd2_graphics_glyph_blit_state.pFont_data = (uint8 *)font_data;
    data_fd2_graphics_glyph_blit_state.nGlyph_idx = (int)glyph_idx;
    data_fd2_graphics_glyph_blit_state.pDst_buf = (uint8 *)dst_buf;
    data_fd2_graphics_glyph_blit_state.wPitch = (uint16)pitch;
    data_fd2_graphics_glyph_blit_state.bFill_color = (uint8)fill_color;
    data_fd2_graphics_glyph_blit_state.bOutline_color = (uint8)outline_color;
    data_fd2_graphics_glyph_blit_state.bBg_color = (uint8)bg_color;

    pitch_val = data_fd2_graphics_glyph_blit_state.wPitch;

    if (data_fd2_graphics_glyph_blit_state.bBg_color != 0) {
        bg_dword = (uint32)data_fd2_graphics_glyph_blit_state.bBg_color
                   * 0x01010101;
        dst_row = (uint32)data_fd2_graphics_glyph_blit_state.pDst_buf;
        row_remain = 0x10;
        do {
            dst_pixel = dst_row;
            for (fill_iter = 4; fill_iter != 0; fill_iter--) {
                *(uint32 *)dst_pixel = bg_dword;
                dst_pixel += 4;
            }
            dst_row += pitch_val;
            row_remain--;
        } while (row_remain != 0);
    }

    fill_byte = data_fd2_graphics_glyph_blit_state.bFill_color;
    outline_byte = data_fd2_graphics_glyph_blit_state.bOutline_color;

    if (data_fd2_graphics_glyph_blit_state.nGlyph_idx != 10) {
        glyph_row = (uint32)data_fd2_graphics_glyph_blit_state.pFont_data
                    + (uint32)data_fd2_graphics_glyph_blit_state.nGlyph_idx
                      * 0x20;
        dst_row = (uint32)data_fd2_graphics_glyph_blit_state.pDst_buf;
        row_remain = 0x10;
        do {
            /* byte-swap the little-endian word so the first stream byte
             * (the left 8 pixels) becomes the high byte; columns are then
             * consumed MSB-first by shifting the sign bit out. */
            row_bits = (uint16)(((uint16)(*(uint8 *)glyph_row) << 8)
                                | (uint16)(*(uint8 *)(glyph_row + 1)));
            dst_pixel = dst_row;
            col_remain = 0x10;
            do {
                if ((int16)row_bits < 0) {
                    *(uint8 *)dst_pixel = fill_byte;
                    *(uint8 *)(dst_pixel + (pitch_val - 1)) = outline_byte;
                    *(uint8 *)(dst_pixel + pitch_val) = outline_byte;
                }
                row_bits = (uint16)(row_bits << 1);
                dst_pixel++;
                col_remain--;
            } while (col_remain != 0);
            dst_row += pitch_val;
            glyph_row += 2;
            row_remain--;
        } while (row_remain != 0);
    }
}

/* ----------------------------------------------------------------
 * fd2_blit_sprite_scaled_with_skip @ 0x4EAE6 (no resolvable callers;
 * reached via an indirect/pointer dispatch)
 *
 * Bresenham-style nearest-neighbor sprite scaler with transparent-zero
 * skip. Paints a scale_num-wide x scale_den-tall output block, sampling
 * an arbitrary src.width x src.height source sprite with fractional
 * (accumulator) step in both axes; source pixel 0 is transparent.
 *
 * Sprite header: ushort width (word 0), ushort height (word 2), then
 * width*height raw pixel bytes (row-major). src_row_ptr starts at the
 * pixel data (sprite_hdr + 4).
 *
 * Vertical Bresenham (per output row): advance src_row_ptr by one source
 * row (src_width bytes) while the row accumulator < src.height, stepping
 * the accumulator by scale_den; then subtract src.height. The row
 * accumulator persists across output rows (it is the running vertical
 * fraction), seeded once from scale_den.
 *
 * Horizontal Bresenham (per output column): advance src_iter by one
 * pixel while the column accumulator < src.width, stepping it by
 * scale_num; then subtract src.width. The column accumulator restarts at
 * scale_num for every output row. If the sampled source pixel is
 * non-zero it is written to dst; zero is skipped (transparent).
 *
 * State globals (written at entry, reloaded across iterations to match
 * the binary's word store / word reload):
 *   data_fd2_graphics_sprite_blit_scaler_loop_state (0x627BA) byte[6]:
 *       [0..3] = dst row stride (dword); [4..5] = output-row down-counter
 *   data_fd2_graphics_sprite_blit_scale_num   (0x627C4) output width / col step
 *   data_fd2_graphics_sprite_blit_scale_den   (0x627C6) vertical step / row count
 *   data_fd2_graphics_sprite_blit_src_width   (0x627C0) source width
 *   data_fd2_graphics_sprite_blit_src_height  (0x627C2) source height
 *
 * The row accumulator, column accumulator and column step are all 16-bit
 * in the binary (DX / BP word ops); the output-column counter is a
 * zero-extended ECX (LOOP). Modeled here at those widths. The output-row
 * down-counter is read/decremented as a word at scaler_loop_state[4].
 *
 * Used for scaled portrait blits and zoom transitions (e.g. title-screen
 * logo zoom-in, status-screen portrait scaling).
 *
 * The binary wraps the whole body in PUSHAD/POPAD and ends with a plain
 * RET (no immediate): cdecl, EAX restored at exit, void return.
 *
 * Args (cdecl, 5x uint32 on stack; void return):
 *   sprite_hdr -- sprite header pointer (width word 0, height word 2,
 *                 then width*height raw pixel bytes; 0 = transparent)
 *   dst_buf    -- destination base linear address
 *   stride     -- bytes to advance dst per output row
 *   scale_num  -- output width / horizontal sample step (word)
 *   scale_den  -- output row count / vertical sample step (word)
 * ---------------------------------------------------------------- */
void fd2_blit_sprite_scaled_with_skip(uint32 sprite_hdr, uint32 dst_buf,
                                      uint32 stride, uint32 scale_num,
                                      uint32 scale_den)
{
    uint32 src_row_ptr;
    uint32 src_iter;
    uint32 dst_iter;
    uint32 out_col_remain;
    uint16 src_width;
    uint16 scale_num_local;
    uint16 col_acc;
    uint16 row_acc;

    *(uint32 *)&data_fd2_graphics_sprite_blit_scaler_loop_state[0] = stride;
    data_fd2_graphics_sprite_blit_scale_num = (uint16)scale_num;
    data_fd2_graphics_sprite_blit_scale_den = (uint16)scale_den;

    src_width = *(uint16 *)sprite_hdr;
    data_fd2_graphics_sprite_blit_src_width = src_width;
    data_fd2_graphics_sprite_blit_src_height = *(uint16 *)(sprite_hdr + 2);
    src_row_ptr = sprite_hdr + 4;

    /* row accumulator and the output-row down-counter are both seeded
     * from scale_den; they then evolve independently. */
    row_acc = (uint16)scale_den;
    *(uint16 *)&data_fd2_graphics_sprite_blit_scaler_loop_state[4] =
        (uint16)scale_den;

    do {
        scale_num_local = data_fd2_graphics_sprite_blit_scale_num;
        while (row_acc < data_fd2_graphics_sprite_blit_src_height) {
            src_row_ptr += src_width;
            row_acc = row_acc + data_fd2_graphics_sprite_blit_scale_den;
        }
        row_acc = row_acc - data_fd2_graphics_sprite_blit_src_height;

        out_col_remain = data_fd2_graphics_sprite_blit_scale_num;
        src_iter = src_row_ptr;
        dst_iter = dst_buf;
        col_acc = data_fd2_graphics_sprite_blit_scale_num;
        do {
            while (col_acc < src_width) {
                src_iter = src_iter + 1;
                col_acc = col_acc + scale_num_local;
            }
            col_acc = col_acc - src_width;
            if (*(uint8 *)src_iter != 0) {
                *(uint8 *)dst_iter = *(uint8 *)src_iter;
            }
            dst_iter = dst_iter + 1;
            out_col_remain = out_col_remain - 1;
        } while (out_col_remain != 0);

        dst_buf += *(uint32 *)&data_fd2_graphics_sprite_blit_scaler_loop_state[0];
        *(uint16 *)&data_fd2_graphics_sprite_blit_scaler_loop_state[4] =
            *(uint16 *)&data_fd2_graphics_sprite_blit_scaler_loop_state[4] - 1;
    } while (*(uint16 *)&data_fd2_graphics_sprite_blit_scaler_loop_state[4] != 0);
}

/* ----------------------------------------------------------------
 * fd2_blit_buffer_with_per_row_offset @ 0x4EB90 (1 caller)
 *
 * Per-row x-offset blit for the chapter background shimmer / heat-haze
 * effect. Copies a 312-byte-wide x 192-row block from src to dst; each
 * source row is shifted horizontally by a small amount read from the
 * 16-entry offset table data_fd2_graphics_shimmer_offset_table_16b
 * (0x627C8), indexed by a cyclic counter that starts at offset_idx and
 * wraps every 16 rows:
 *
 *   src_row_base = src_buf + 4              // skip the 4-byte header
 *   for row in 0..0xC0 (192 rows):
 *       src_iter = src_row_base + shimmer_offset_table[offset_idx]
 *       copy 0x4E (78) dwords  src_iter -> dst_buf   (both advance by 4)
 *       src_row_base += 0x140               // next source row (320 stride)
 *       offset_idx = (offset_idx + 1), wrap to 0 at 16
 *       dst_buf += 2 dwords (8 bytes)        // 320 - 78*4 = 8 byte row gap
 *
 * The inner span is a REP MOVSD of 78 dwords (312 bytes); dst_buf is a
 * uint32* so its post-row +2 advance is the 8-byte inter-row gap, making
 * the destination a 320-byte-stride surface to match the source. The
 * offset counter is incremented and wrapped at 16 as a 16-bit quantity in
 * the binary (INC DX / CMP DX,0x10 / XOR DX,DX); offset_idx arrives in
 * 0..15 (the caller passes data_fd2_graphics_bg_animation_frame_idx),
 * so the wrap keeps it there.
 *
 * The sole caller fd2_composite_battle_tile_map runs this once per tick on
 * the animated-background chapters (9, 0x18, 0x19, 0x1C, 0x1D) to copy the
 * static backdrop into the animated work buffer with the per-row cyclic
 * displacement, producing the rippling background shimmer.
 *
 * Args (cdecl, 3x uint32 on stack; void return):
 *   src_buf    -- source buffer base linear address (pixel data at +4)
 *   dst_buf    -- destination base (uint32*, 320-byte-stride surface)
 *   offset_idx -- starting index into the 16-entry offset table (0..15)
 * ---------------------------------------------------------------- */
void fd2_blit_buffer_with_per_row_offset(uint32 src_buf, uint32 *dst_buf,
                                         uint32 offset_idx)
{
    uint32 src_row_base;
    uint32 src_iter;
    uint32 col_remain;
    uint16 row_remain;

    src_row_base = src_buf + 4;
    row_remain = 0xC0;
    do {
        src_iter = src_row_base
                   + data_fd2_graphics_shimmer_offset_table_16b[offset_idx];
        for (col_remain = 0x4E; col_remain != 0; col_remain--) {
            *dst_buf = *(uint32 *)src_iter;
            src_iter += 4;
            dst_buf += 1;
        }
        src_row_base += 0x140;
        offset_idx = offset_idx + 1;
        if (offset_idx > 0xF) {
            offset_idx = 0;
        }
        dst_buf += 2;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_blit_sprite_with_decoded_pixels @ 0x4E85B (9 call sites)
 *
 * Generic rectangular-sprite painter for the dialog-pixel encoded
 * stream, with transparent (zero) pixels skipped. The sprite header
 * carries width/height as its first two 16-bit words, immediately
 * followed by the encoded pixel byte stream:
 *
 *   col_count = *(uint16 *)(sprite_hdr + 0)   // pixels per row
 *   row_count = *(uint16 *)(sprite_hdr + 2)   // number of rows
 *   cursor    = sprite_hdr + 4                 // start of the byte stream
 *   for row in 0..row_count-1:
 *       dst_row = dst
 *       for col in 0..col_count-1:
 *           pixel = low byte of fd2_decode_dialog_pixel_byte(state, &cursor)
 *           if (pixel != 0): *dst_row = pixel   // zero = transparent
 *           dst_row++
 *       dst += stride                           // next destination row
 *
 * Each output pixel is produced by fd2_decode_dialog_pixel_byte: the
 * packed (run_remain << 8) | pixel value it returns is fed back in as
 * the next state, and its in/out stream cursor advances 0/1/2 bytes per
 * call (the C model of the binary's ESI + LODSB run-length decoder). The
 * decode state is seeded to 0 once per blit and threaded through the
 * whole rectangle, so runs straddle row boundaries.
 *
 * Unlike fd2_rle_blit_sprite (the high-2-bits command RLE format), this
 * is the simpler per-pixel run encoding used for the portrait / dialog
 * sprites; the inner loop count is the literal column count from the
 * header, not a stream-driven marker.
 *
 * The binary wraps the whole body in PUSHAD/POPAD and ends with a plain
 * RET (cdecl, no callee stack cleanup): EAX is left at its entry value,
 * so the decompiler's "return in_EAX" is a pass-through artifact. All
 * call sites discard the result, so this is a void function. The two
 * loops are post-test (LOOP on CX = col_count, DEC DX; JNZ on row_count)
 * and seeded from the 16-bit header words, so they are modeled as 16-bit
 * down-counters that wrap on a zero header value, matching the binary.
 *
 * Args (cdecl, 3x uint32 on stack; void return):
 *   dst        -- destination base linear address (row base)
 *   sprite_hdr -- sprite header pointer (width word 0, height word 2,
 *                 then the encoded pixel byte stream)
 *   stride     -- bytes to advance dst per row
 * ---------------------------------------------------------------- */
void fd2_blit_sprite_with_decoded_pixels(uint32 dst, uint32 sprite_hdr,
                                         uint32 stride)
{
    uint8 *cursor;
    uint8 *dst_row;
    uint16 state;
    uint16 pixel;
    uint16 col;
    uint16 col_remain;
    uint16 row_remain;

    col_remain = *(uint16 *)sprite_hdr;
    row_remain = *(uint16 *)(sprite_hdr + 2);
    cursor = (uint8 *)(sprite_hdr + 4);
    state = 0;
    do {
        dst_row = (uint8 *)dst;
        col = col_remain;
        do {
            state = fd2_decode_dialog_pixel_byte(state, &cursor);
            pixel = (uint16)(state & 0xFF);
            if (pixel != 0) {
                *dst_row = (uint8)pixel;
            }
            dst_row++;
            col--;
        } while (col != 0);
        dst += stride;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_dialog_sprite_blit_normal @ 0x4E8AF (20 callers)
 *
 * Opaque rectangular-sprite painter for the dialog-pixel encoded
 * stream: writes EVERY decoded pixel to dst (including zero), with no
 * transparency skip. The opaque sister of
 * fd2_blit_sprite_with_decoded_pixels @ 0x4E85B (which treats a zero
 * byte as transparent); both share the same header layout and the same
 * per-pixel decoder.
 *
 *   width     = *(uint16 *)(sprite_hdr + 0)   // pixels per row
 *   row_count = *(uint16 *)(sprite_hdr + 2)   // number of rows
 *   cursor    = sprite_hdr + 4                 // start of the byte stream
 *   for row in 0..row_count-1:
 *       dst_row = dst
 *       for col in 0..width-1:
 *           *dst_row = low byte of fd2_decode_dialog_pixel_byte(state, &cursor)
 *           dst_row++                          // every pixel written, no skip
 *       dst += stride                          // next destination row
 *
 * Each output pixel is produced by fd2_decode_dialog_pixel_byte: the
 * packed (run_remain << 8) | pixel value it returns is fed back in as
 * the next state, and its in/out stream cursor advances 0/1/2 bytes per
 * call (the C model of the binary's ESI + LODSB run-length decoder). The
 * decode state is seeded to 0 once per blit and threaded through the
 * whole rectangle, so runs straddle row boundaries.
 *
 * The inner per-pixel loop count is the literal width header word: the
 * binary seeds CX from BP (= header word 0) and runs a LOOP, with ECX
 * pre-zeroed so the high half stays clear. (The decompiler's
 * "while (ECX != 1)" is a phantom: fd2_decode_dialog_pixel_byte never
 * touches ECX, and there is no end-of-line marker -- the row length is
 * the fixed width.) Both loops are post-test do/while seeded from the
 * 16-bit header words, so they are modeled as 16-bit down-counters that
 * wrap on a zero header value, matching the binary's LOOP / DEC DX; JNZ.
 *
 * The binary wraps the whole body in PUSHAD/POPAD and ends with a plain
 * RET (cdecl, caller does ADD ESP,0xC): EAX is left at its entry value,
 * so the decompiler's "return in_EAX" is a pass-through artifact. All
 * call sites discard the result, so this is a void function.
 *
 * The general-purpose opaque painter for every dialog/UI sprite that
 * must overwrite its background (status panels, shop/party-roster panels,
 * chapter-intro overlays, save-slot selector, typewriter dialog, ...). One
 * representative caller is fd2_paint_portrait_to_dialog_area, which uses
 * this normal (left-to-right) variant when
 * data_fd2_dialog_active_portrait_blit_offset != 0x9017 and the mirrored
 * sister fd2_dialog_sprite_blit_mirrored @ 0x4E8E1 when it == 0x9017.
 *
 * Args (cdecl, 3x uint32 on stack; void return):
 *   dst        -- destination base linear address (row base)
 *   sprite_hdr -- sprite header pointer (width word 0, height word 2,
 *                 then the encoded pixel byte stream)
 *   stride     -- bytes to advance dst per row
 * ---------------------------------------------------------------- */
void fd2_dialog_sprite_blit_normal(uint32 dst, uint32 sprite_hdr,
                                   uint32 stride)
{
    uint8 *cursor;
    uint8 *dst_row;
    uint16 state;
    uint16 width_remain;
    uint16 row_remain;
    uint16 col;

    width_remain = *(uint16 *)sprite_hdr;
    row_remain = *(uint16 *)(sprite_hdr + 2);
    cursor = (uint8 *)(sprite_hdr + 4);
    state = 0;
    do {
        dst_row = (uint8 *)dst;
        col = width_remain;
        do {
            state = fd2_decode_dialog_pixel_byte(state, &cursor);
            *dst_row = (uint8)state;
            dst_row++;
            col--;
        } while (col != 0);
        dst += stride;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_dialog_sprite_blit_mirrored @ 0x4E8E1 (5 call sites)
 *
 * Horizontally-flipped sister of fd2_dialog_sprite_blit_normal @
 * 0x4E8AF: same opaque per-pixel decoder and same header layout, but
 * dst is the RIGHT edge of the destination row and each decoded pixel
 * walks LEFT (dst_row--) instead of right, producing a mirror image.
 *
 *   width     = *(uint16 *)(sprite_hdr + 0)   // pixels per row
 *   row_count = *(uint16 *)(sprite_hdr + 2)   // number of rows
 *   cursor    = sprite_hdr + 4                 // start of the byte stream
 *   for row in 0..row_count-1:
 *       dst_row = dst                          // starts at the row's RIGHT edge
 *       for col in 0..width-1:
 *           *dst_row = low byte of fd2_decode_dialog_pixel_byte(state, &cursor)
 *           dst_row--                          // walk left; every pixel written
 *       dst += stride                          // next row's right-edge start
 *
 * Differs from fd2_dialog_sprite_blit_normal only in the per-pixel step:
 * the binary uses MOV [EDI],AL; DEC EDI here, versus STOSB (store + INC)
 * in the normal variant. The row-to-row step is unchanged: dst (the saved
 * row-start, restored via PUSH/POP EDI) advances by +stride each row, so
 * the image flips horizontally but not vertically.
 *
 * Each output pixel is produced by fd2_decode_dialog_pixel_byte: the
 * packed (run_remain << 8) | pixel value it returns is fed back in as the
 * next state, and its in/out stream cursor advances 0/1/2 bytes per call
 * (the C model of the binary's ESI + LODSB run-length decoder). The
 * decode state is seeded to 0 once per blit and threaded through the whole
 * rectangle, so runs straddle row boundaries.
 *
 * The inner per-pixel loop count is the literal width header word: the
 * binary seeds CX from BP (= header word 0) and runs a LOOP, with ECX
 * pre-zeroed so the high half stays clear. (The decompiler's
 * "while (ECX != 1)" is a phantom: fd2_decode_dialog_pixel_byte never
 * touches ECX, and there is no end-of-line marker -- the row length is the
 * fixed width.) Both loops are post-test do/while seeded from the 16-bit
 * header words, so they are modeled as 16-bit down-counters that wrap on a
 * zero header value, matching the binary's LOOP / DEC DX; JNZ.
 *
 * The binary wraps the whole body in PUSHAD/POPAD and ends with a plain
 * RET (cdecl, caller does ADD ESP,0xC): EAX is left at its entry value, so
 * the decompiler's "return in_EAX" is a pass-through artifact. All call
 * sites discard the result, so this is a void function.
 *
 * Reached via fd2_paint_portrait_to_dialog_area when
 * data_fd2_dialog_active_portrait_blit_offset == 0x9017 (right-side ally
 * portrait slot) -- flips the ally face so they appear to face the enemy
 * in conversation; the != 0x9017 branch uses the un-flipped normal sister.
 *
 * Args (cdecl, 3x uint32 on stack; void return):
 *   dst        -- destination right-edge linear address (row's last pixel)
 *   sprite_hdr -- sprite header pointer (width word 0, height word 2,
 *                 then the encoded pixel byte stream)
 *   stride     -- bytes to advance dst per row
 * ---------------------------------------------------------------- */
void fd2_dialog_sprite_blit_mirrored(uint32 dst, uint32 sprite_hdr,
                                     uint32 stride)
{
    uint8 *cursor;
    uint8 *dst_row;
    uint16 state;
    uint16 width_remain;
    uint16 row_remain;
    uint16 col;

    width_remain = *(uint16 *)sprite_hdr;
    row_remain = *(uint16 *)(sprite_hdr + 2);
    cursor = (uint8 *)(sprite_hdr + 4);
    state = 0;
    do {
        dst_row = (uint8 *)dst;
        col = width_remain;
        do {
            state = fd2_decode_dialog_pixel_byte(state, &cursor);
            *dst_row = (uint8)state;
            dst_row--;
            col--;
        } while (col != 0);
        dst += stride;
        row_remain--;
    } while (row_remain != 0);
}

/* ----------------------------------------------------------------
 * fd2_restore_screen_block_from_buffer @ 0x4E92C (1 caller)
 *
 * Outer setter for a screen-block restore: seeds the width/height
 * globals from a saved-block buffer header, resolves the destination
 * pointer, then hands off to the inner copy loop fd2_restore_block_loop
 * @ 0x4E954. The inverse of fd2_save_screen_block_to_buffer @ 0x4E96F,
 * which created the snapshot.
 *
 * The saved-block buffer (built by the save companion) is laid out as:
 *   [+0] uint16 width
 *   [+2] uint16 height
 *   [+4] uint32 dst_offset   (the offset added to dst to reach the
 *                             top-left of the region being restored)
 *   [+8] width*height contiguous saved pixel bytes
 *
 *   data_fd2_graphics_rle_blit_cur_width      = *(uint16 *)saved_block
 *   data_fd2_graphics_rle_blit_remaining_rows = *(uint16 *)(saved_block + 2)
 *   restore_dst = dst + *(uint32 *)(saved_block + 4)
 *   src         = saved_block + 8        // start of the saved pixels
 *   fd2_restore_block_loop(restore_dst, src, stride)
 *
 * In the binary the inner loop's src/dst/stride arrive in registers
 * (ESI=src walked to saved_block+8 by the two LODSW + one LODSD, EDI=the
 * resolved destination, EBP=stride); here they are passed explicitly to
 * fd2_restore_block_loop, which is exposed with the matching parameters.
 *
 * Used to restore screen state after a transient overlay sprite finishes
 * (dialog box closes, cursor moves); the sole caller
 * fd2_cleanup_dialog_sprite_buffer restores then frees the buffer.
 *
 * The binary wraps the whole body in PUSHAD/POPAD and ends with a plain
 * RET (cdecl, caller does ADD ESP,0xC): EAX is left at its entry value,
 * so the decompiler's "return in_EAX" is a pass-through artifact; the
 * sole caller discards it, so this is a void function.
 *
 * Args (cdecl, 3x uint32 on stack; void return):
 *   saved_block -- saved-block buffer header (width word 0, height word 2,
 *                  dst_offset dword 4, then width*height saved pixels)
 *   dst         -- destination base linear address
 *   stride      -- bytes to advance dst per row
 * ---------------------------------------------------------------- */
void fd2_restore_screen_block_from_buffer(uint32 saved_block, uint32 dst,
                                          uint32 stride)
{
    uint32 restore_dst;
    uint32 src;

    data_fd2_graphics_rle_blit_cur_width = *(uint16 *)saved_block;
    data_fd2_graphics_rle_blit_remaining_rows = *(uint16 *)(saved_block + 2);
    restore_dst = dst + *(uint32 *)(saved_block + 4);
    src = saved_block + 8;
    fd2_restore_block_loop(restore_dst, src, stride);
}

/* ----------------------------------------------------------------
 * fd2_save_screen_block_to_buffer @ 0x4E96F (3 callers)
 *
 * Outer setter for a screen-block save -- the exact inverse of
 * fd2_restore_screen_block_from_buffer @ 0x4E92C. Snapshots a live screen
 * region into a backup buffer for later restore: writes an 8-byte header
 * (width, height, src_offset) then hands off to the inner copy loop
 * fd2_save_block_loop @ 0x4E9A0, which packs the pixels contiguously.
 *
 * The width/height words are stored both into the globals
 * data_fd2_graphics_rle_blit_cur_width (0x627B4) /
 * data_fd2_graphics_rle_blit_remaining_rows (0x627B6) -- which the inner
 * loop reads -- and into the buffer header. The header is written in place
 * via three sequential stores (STOSW/STOSW/STOSD in the binary), leaving
 * the buffer cursor at out_buf + 8, the start of the saved pixels.
 *
 * Backup buffer layout (consumed by the restore companion):
 *   [+0] uint16 width
 *   [+2] uint16 height
 *   [+4] uint32 src_offset  (offset added to src_base to reach the
 *                            region's top-left; mirrors the restore
 *                            companion's dst_offset)
 *   [+8] width*height contiguous saved pixel bytes
 *
 *   data_fd2_graphics_rle_blit_cur_width      = (uint16)width
 *   *(uint16 *)out_buf       = (uint16)width
 *   data_fd2_graphics_rle_blit_remaining_rows = (uint16)height
 *   *(uint16 *)(out_buf + 2) = (uint16)height
 *   *(uint32 *)(out_buf + 4) = src_offset
 *   src = src_base + src_offset           // top-left of the live region
 *   dst = out_buf + 8                     // start of the saved pixels
 *   fd2_save_block_loop(src, dst, stride)
 *
 * In the binary the inner loop's src/dst/stride arrive in registers
 * (ESI=src_base advanced by ADD ESI,src_offset, EDI=out_buf walked to
 * out_buf+8 by the two STOSW + one STOSD, EBP=stride); here they are
 * passed explicitly to fd2_save_block_loop, which is exposed with the
 * matching parameters.
 *
 * Used as the "save under" snapshot before painting a transient overlay
 * sprite (dialog box, cursor frame); the saved block is later handed to
 * fd2_restore_screen_block_from_buffer to repaint the underlying screen.
 *
 * The binary wraps the whole body in PUSHAD/POPAD and ends with a plain
 * RET (cdecl, every caller does ADD ESP,0x18): EAX is left at its entry
 * value, so the decompiler's "return in_EAX" is a pass-through artifact;
 * all three callers discard it, so this is a void function.
 *
 * Args (cdecl, 6x uint32 on stack; void return):
 *   out_buf    -- backup buffer base (receives the 8-byte header, then the
 *                 width*height saved pixels)
 *   width      -- columns copied per row (stored as a word in the header
 *                 and the width global)
 *   height     -- number of rows (stored as a word in the header and the
 *                 height global)
 *   src_base   -- source base linear address (the live screen region)
 *   src_offset -- offset added to src_base to reach the region's top-left;
 *                 also stored in the header at +4 for the restore
 *   stride     -- bytes to advance the source per row
 * ---------------------------------------------------------------- */
void fd2_save_screen_block_to_buffer(uint32 out_buf, uint32 width,
                                     uint32 height, uint32 src_base,
                                     uint32 src_offset, uint32 stride)
{
    uint32 src;
    uint32 dst;

    data_fd2_graphics_rle_blit_cur_width = (uint16)width;
    *(uint16 *)out_buf = (uint16)width;
    data_fd2_graphics_rle_blit_remaining_rows = (uint16)height;
    *(uint16 *)(out_buf + 2) = (uint16)height;
    *(uint32 *)(out_buf + 4) = src_offset;
    src = src_base + src_offset;
    dst = out_buf + 8;
    fd2_save_block_loop(src, dst, stride);
}

/* ----------------------------------------------------------------
 * fd2_blit_sprite_with_stride_setup @ 0x4E9E4 (9 callers)
 *
 * Outer setter for a transparent-zero sprite blit with a custom
 * destination stride. Stores the requested stride into the shared
 * glyph/blit render-state struct, then hands off to the inner copy loop
 * fd2_blit_sprite_with_stride_loop @ 0x4E9FF (which reads the stride back
 * from that global per row):
 *
 *   data_fd2_graphics_glyph_blit_state.wPitch = (uint16)stride
 *   fd2_blit_sprite_with_stride_loop(dst, sprite_hdr)
 *
 * The state separation (stride lives in a global rather than a register)
 * lets the inner loop be entered directly when the wPitch field is already
 * set to the desired stride; this wrapper is the entry point used when the
 * caller needs a stride different from whatever was last left in wPitch.
 *
 * In the binary the inner loop's destination and sprite-header pointers
 * arrive in registers (EDI=dst, ESI=sprite_hdr), seeded here right before
 * the CALL; they are passed explicitly to fd2_blit_sprite_with_stride_loop,
 * which is exposed with the matching parameters.
 *
 * The whole body is wrapped in PUSHAD/POPAD and ends with a plain RET
 * (cdecl, every caller does ADD ESP,0xC): EAX is left at its entry value,
 * so the decompiler's "return in_EAX" is a pass-through artifact; all nine
 * callers discard it, so this is a void function. Hand-written asm leaf:
 * no __CHK probe.
 *
 * Only the low 16 bits of stride are stored (the wPitch field is a word);
 * dst and sprite_hdr occupy a full dword each on the stack.
 *
 * Args (cdecl, 3x uint32 on stack; void return):
 *   dst        -- destination base linear address (row base)
 *   sprite_hdr -- sprite header pointer (width word 0, height word 2,
 *                 then width*height raw pixel bytes; zero = transparent)
 *   stride     -- bytes to advance dst per row (stored as wPitch word)
 * ---------------------------------------------------------------- */
void fd2_blit_sprite_with_stride_setup(uint32 dst, uint32 sprite_hdr,
                                       uint32 stride)
{
    data_fd2_graphics_glyph_blit_state.wPitch = (uint16)stride;
    fd2_blit_sprite_with_stride_loop(dst, sprite_hdr);
}

/* ----------------------------------------------------------------
 * fd2_blit_indexed_sprite @ 0x2935B (~30 callers)
 *
 * Indexed-sprite blit dispatcher: looks up sprite #sprite_idx in the
 * sheet's offset table and forwards the actual decode + paint to
 * fd2_rle_blit_sprite. Thin wrapper used throughout battle / event /
 * cinematic rendering.
 *
 * Sheet format:
 *   byte[0..7]  -- header (sprite count + flags, not read here)
 *   int[8..]    -- sprite offset table; entry i = byte offset (relative
 *                  to the sheet start) of sprite_i's data block.
 *   Sprite data block at that offset:
 *     ushort[0]  = width
 *     ushort[1]  = height
 *     byte[4..8] = (reserved; not read here)
 *     byte[9..]  = RLE-packed pixel stream (decoded by fd2_rle_blit_sprite)
 *
 *   sprite_data = sheet_ptr + *(int *)(sheet_ptr + 8 + sprite_idx * 4)
 *   fd2_rle_blit_sprite(sprite_data + 9,            // RLE stream
 *                       width  = *(ushort *)sprite_data,
 *                       height = *(ushort *)(sprite_data + 2),
 *                       dst_buf, dst_stride, palette_op)
 *
 * The width/height words are zero-extended (MOVZX) into the dst_x/dst_y
 * argument slots of fd2_rle_blit_sprite. palette_op is forwarded
 * unchanged (0xFFFFFFFF passthrough / >0xFF translucent / <=0xFF
 * silhouette; see fd2_rle_blit_sprite).
 *
 * Cdecl, 5 stack params; void return. The binary's __CHK(0x20)
 * stack-probe prologue is compiler-generated and omitted here.
 *
 * Args (cdecl, 5x on stack):
 *   sheet_ptr  -- sprite atlas base linear address
 *   sprite_idx -- index into the sheet's offset table
 *   dst_buf    -- destination base linear address
 *   dst_stride -- bytes to advance dst per row
 *   palette_op -- palette operation selector (passed through)
 * ---------------------------------------------------------------- */
void fd2_blit_indexed_sprite(uint32 sheet_ptr, uint32 sprite_idx, uint32 dst_buf,
                             int32 dst_stride, uint32 palette_op)
{
    uint32 sprite_data;

    sprite_data = sheet_ptr + *(int32 *)(sheet_ptr + 8 + sprite_idx * 4);
    fd2_rle_blit_sprite(sprite_data + 9, (int32)*(uint16 *)sprite_data,
                        (int32)*(uint16 *)(sprite_data + 2), dst_buf, dst_stride,
                        palette_op);
}
