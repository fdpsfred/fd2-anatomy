/*
 * anidec.c — ANI file frame decoder chunk handlers.
 *
 * Each handler processes one chunk type from the ANI delta stream.
 * Called via dispatch table data_fd2_animation_ani_decoder_frame_dispatch_table.
 *
 * Original binary uses ESI register as stream cursor (LODSB/LODSW).
 * C version uses file-scope static g_ani_cursor instead.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

static uint8 *g_ani_cursor;

/* ANI frame decoder target-buffer state @ 0x52760.
 * Row stride in bytes. Written each frame by fd2_ani_decoder_set_target_buffer
 * before the decoder runs; read by the row chunk handlers. Zero-initialized in
 * the binary (filled at runtime). */
uint16 data_fd2_animation_ani_decoder_target_width;

/* ANI frame decoder destination buffer address @ 0x52762.
 * Linear address of the destination row start. Written each frame by
 * fd2_ani_decoder_set_target_buffer; read by the row/sparse chunk handlers
 * (REP STOSD/memcpy target). Zero-initialized in the binary (filled at
 * runtime by the setter before the decoder runs). */
uint32 data_fd2_animation_ani_decoder_dst_buf;

/* ANI frame decoder source/palette-area base address @ 0x52766.
 * Linear address used as the palette write base by the palette chunk
 * handlers (fill/literal/RLE/run-pairs). Written each frame by
 * fd2_ani_decoder_set_target_buffer (32-bit store of src_buf); read by the
 * palette chunk handlers which cast it to a byte/dword pointer base.
 * Zero-initialized in the binary (filled at runtime by the setter before
 * the decoder runs). */
uint32 data_fd2_animation_ani_decoder_src_buf;

/* ----------------------------------------------------------------
 * fd2_ani_decoder_set_target_buffer @ 0x36C7D
 *
 * Stores the three decoder target-buffer parameters into the adjacent
 * globals consumed by the chunk handlers:
 *   width   -> target_width @ 0x52760 (per-chunk output byte length)
 *   dst_buf -> dst_buf      @ 0x52762 (destination linear address)
 *   src_buf -> src_buf      @ 0x52766 (palette-area base address)
 * Called once per frame by fd2_play_ani_file_animation_sequence before
 * fd2_ani_decoder_decode_frame_bytes runs. Void return, no state read.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_set_target_buffer(uint16 width, uint32 dst_buf,
                                        uint32 src_buf)
{
    data_fd2_animation_ani_decoder_target_width = width;
    data_fd2_animation_ani_decoder_dst_buf = dst_buf;
    data_fd2_animation_ani_decoder_src_buf = src_buf;
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_decode_frame_bytes @ 0x36C9E
 *
 * Top-level ANI frame decoder. Points the shared stream cursor
 * g_ani_cursor at src_buf_ptr, then runs chunk_count iterations: each
 * reads one chunk-type byte and dispatches it through
 * data_fd2_animation_ani_decoder_frame_dispatch_table[chunk_type]().
 * Only the chunk-type byte is consumed here; each handler advances the
 * same g_ani_cursor by however many operand bytes it needs, so the
 * cursor walks the whole stream cooperatively (mirrors the binary's
 * single ESI loaded once before the loop, advanced by LODSB and by
 * every handler). Called once per frame after
 * fd2_ani_decoder_set_target_buffer sets the output triple. No return.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_decode_frame_bytes(uint16 chunk_count,
                                         uint32 src_buf_ptr)
{
    uint8 chunk_type;

    if (chunk_count == 0) return;
    g_ani_cursor = (uint8 *)src_buf_ptr;
    while (chunk_count != 0) {
        chunk_type = *g_ani_cursor++;
        ((void (*)(void))
            data_fd2_animation_ani_decoder_frame_dispatch_table
                [chunk_type])();
        chunk_count--;
    }
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_palette_fill_byte @ 0x36AE7
 *
 * Fill entire 768-byte palette area with one byte value.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_palette_fill_byte(void)
{
    uint8 val;

    val = *g_ani_cursor++;
    memset((void *)data_fd2_animation_ani_decoder_src_buf, val, 768);
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_palette_load_literal @ 0x36B01
 *
 * Copy 768 bytes from stream to palette area.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_palette_load_literal(void)
{
    memcpy((void *)data_fd2_animation_ani_decoder_src_buf,
           g_ani_cursor, 768);
    g_ani_cursor += 768;
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_palette_load_rle @ 0x36B0F
 *
 * RLE decode 768 bytes to palette area. C0-FF = run header.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_palette_load_rle(void)
{
    uint8 *dst;
    int pos;
    uint8 b;
    int run_len;
    uint8 fill;

    dst = (uint8 *)data_fd2_animation_ani_decoder_src_buf;
    pos = 0;
    while (pos < 0x300) {
        b = *g_ani_cursor++;
        if ((b & 0xC0) == 0xC0) {
            run_len = b & 0x3F;
            pos += run_len;
            fill = *g_ani_cursor++;
            memset(dst, fill, run_len);
            dst += run_len;
        } else {
            *dst++ = b;
            pos++;
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_palette_load_run_pairs @ 0x36B51
 *
 * Read count, then count pairs of (start_color, n_colors)
 * each followed by n_colors*3 literal RGB bytes.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_palette_load_run_pairs(void)
{
    uint8 *base;
    uint8 count;
    uint8 start;
    uint8 n_colors;
    int byte_count;
    uint8 *dst;

    count = *g_ani_cursor++;
    base = (uint8 *)data_fd2_animation_ani_decoder_src_buf;
    while (count != 0) {
        start = *g_ani_cursor++;
        n_colors = *g_ani_cursor++;
        byte_count = (int)(uint32)n_colors * 3;
        dst = base + (uint32)start * 3;
        memcpy(dst, g_ani_cursor, byte_count);
        g_ani_cursor += byte_count;
        base = (uint8 *)data_fd2_animation_ani_decoder_src_buf;
        count--;
    }
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_row_fill_byte @ 0x36B8A
 *
 * Fill target_width bytes in row buffer with one byte value.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_row_fill_byte(void)
{
    uint8 val;
    uint16 width;

    val = *g_ani_cursor++;
    width = data_fd2_animation_ani_decoder_target_width;
    memset((void *)data_fd2_animation_ani_decoder_dst_buf, val, width);
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_row_copy_literal @ 0x36BB2
 *
 * Copy target_width bytes from stream to row buffer.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_row_copy_literal(void)
{
    uint16 width;

    width = data_fd2_animation_ani_decoder_target_width;
    memcpy((void *)data_fd2_animation_ani_decoder_dst_buf,
           g_ani_cursor, width);
    g_ani_cursor += width;
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_row_decode_rle @ 0x36BCE
 *
 * RLE decode target_width bytes to row buffer. Same format as
 * palette RLE (C0-FF = run header).
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_row_decode_rle(void)
{
    uint8 *dst;
    int pos;
    uint16 width;
    uint8 b;
    int run_len;
    uint8 fill;

    dst = (uint8 *)data_fd2_animation_ani_decoder_dst_buf;
    width = data_fd2_animation_ani_decoder_target_width;
    pos = 0;
    while (pos < (int)width) {
        b = *g_ani_cursor++;
        if ((b & 0xC0) == 0xC0) {
            run_len = b & 0x3F;
            pos += run_len;
            fill = *g_ani_cursor++;
            memset(dst, fill, run_len);
            dst += run_len;
        } else {
            *dst++ = b;
            pos++;
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_sparse_set_byte @ 0x36C13
 *
 * Read count, then count (offset, byte) pairs. Set one byte at
 * each offset in row buffer.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_sparse_set_byte(void)
{
    uint8 *base;
    uint16 count;
    uint16 offset;

    count = *(uint16 *)g_ani_cursor;
    g_ani_cursor += 2;
    base = (uint8 *)data_fd2_animation_ani_decoder_dst_buf;
    while (count != 0) {
        offset = *(uint16 *)g_ani_cursor;
        g_ani_cursor += 2;
        base[offset] = *g_ani_cursor++;
        count--;
    }
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_sparse_set_run_byte @ 0x36C2C
 *
 * Read count, then count (offset, length, fill_byte) records.
 * Run-fill at each offset in row buffer.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_sparse_set_run_byte(void)
{
    uint8 *base;
    uint16 count;
    uint16 offset;
    uint8 length;
    uint8 fill;

    count = *(uint16 *)g_ani_cursor;
    g_ani_cursor += 2;
    base = (uint8 *)data_fd2_animation_ani_decoder_dst_buf;
    while (count != 0) {
        offset = *(uint16 *)g_ani_cursor;
        g_ani_cursor += 2;
        length = *g_ani_cursor++;
        fill = *g_ani_cursor++;
        memset(base + offset, fill, length);
        count--;
    }
}

/* ----------------------------------------------------------------
 * fd2_ani_decoder_chunk_sparse_copy_literal @ 0x36C56
 *
 * Read count, then count (offset, length) + literal bytes.
 * Copy literal bytes at each offset in row buffer.
 * ---------------------------------------------------------------- */
void fd2_ani_decoder_chunk_sparse_copy_literal(void)
{
    uint8 *base;
    uint16 count;
    uint16 offset;
    uint8 length;

    count = *(uint16 *)g_ani_cursor;
    g_ani_cursor += 2;
    base = (uint8 *)data_fd2_animation_ani_decoder_dst_buf;
    while (count != 0) {
        offset = *(uint16 *)g_ani_cursor;
        g_ani_cursor += 2;
        length = *g_ani_cursor++;
        memcpy(base + offset, g_ani_cursor, length);
        g_ani_cursor += length;
        count--;
    }
}
