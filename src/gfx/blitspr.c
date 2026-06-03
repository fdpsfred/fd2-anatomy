/*
 * blitspr.c — sprite / rectangle blit primitives
 */

#include <string.h>
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
