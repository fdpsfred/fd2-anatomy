/*
 * crt.c — FD2-specific CRT-equivalent routines.
 *
 * These functions behave like Watcom CRT helpers but do not byte-match
 * any CLIB3S / MATH .obj, so they are re-emitted as FD2 rebuild source
 * rather than linked from the vendor library.
 *
 * Functions in this file:
 *   crt_equivalent_lx_chunk_read_36107 @ 0x36107  (2 callers)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>     /* memcpy */
#include <io.h>         /* lseek, read, SEEK_SET */

/* ----------------------------------------------------------------
 * crt_equivalent_lx_chunk_read_36107 @ 0x36107
 *
 * Mode-flag dispatcher used by the LX module-loader pipeline to fetch a
 * chunk of bytes into dest, from one of two backing stores:
 *   - mode bit0 set : in-memory copy  -> memcpy(dest, file_handle+offset, length)
 *                     (file_handle is reused as a base address here).
 *   - mode bit0 clear: file-backed read -> lseek(file_handle, offset, SEEK_SET)
 *                     then read(file_handle, dest, length).
 *
 * Returns offset+length (the end position) so callers can chain reads.
 *
 * __cdecl: callers push 5 args and clean up with ADD ESP,0x14; the body
 * ends with a plain RET. The return value is offset+length, computed
 * before any callee call (EBX in the original), so it is unaffected by
 * the called helpers' return values.
 * ---------------------------------------------------------------- */
int crt_equivalent_lx_chunk_read_36107(int file_handle, int offset,
                                       uint8 mode, void *dest, uint32 length)
{
    if ((mode & 1) == 0) {
        lseek(file_handle, offset, SEEK_SET);
        read(file_handle, dest, length);
    }
    else {
        memcpy(dest, (void *)(offset + file_handle), length);
    }
    return length + offset;
}
