/*
 * crt.c — FD2-specific CRT-equivalent routines.
 *
 * These functions behave like Watcom CRT helpers but do not byte-match
 * any CLIB3S / MATH .obj, so they are re-emitted as FD2 rebuild source
 * rather than linked from the vendor library.
 *
 * Functions in this file:
 *   crt_equivalent_lx_chunk_read_36107  @ 0x36107  (2 callers)
 *   crt_equivalent_lx_header_reader_36344 @ 0x36344 (1 caller)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>     /* memcpy, strcmp */
#include <io.h>         /* open, close, lseek, read, SEEK_SET */
#include <fcntl.h>      /* O_* flags for open() */

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

/* ----------------------------------------------------------------
 * crt_equivalent_lx_header_reader_36344 @ 0x36344  (1 caller)
 *
 * LX executable header introspection: verify the MZ->LX wiring and
 * aggregate the object table's virtual sizes.
 *
 * arg0 is overloaded by mode_byte bit0:
 *   - bit0 clear : arg0 is a null-terminated path string; open it
 *                  (oflag 0x200) and read via lseek+read; on open
 *                  failure (-1) return 0.
 *   - bit0 set   : arg0 is a caller-supplied int file handle reused as
 *                  an in-memory base address; every chunk read becomes a
 *                  memcpy from that base (open/close are skipped).
 * mode_byte is forwarded verbatim to the chunk reader so the same store
 * (file vs in-memory) is selected for each read.
 *
 * Steps:
 *   1. Resolve the handle (open path, or take arg0 directly).
 *   2. Read 4-byte e_lfanew at MZ+0x3C (no MZ-magic check).
 *   3. Read 2-byte LX magic at e_lfanew into a 4-byte slot pre-filled
 *      with 0x00002020; strcmp the slot vs "LX". The upper two slot
 *      bytes stay 0, so the slot is the C string {magic0,magic1,0}.
 *      On mismatch close(handle) UNCONDITIONALLY (the original ignores
 *      bit0 here) and return 0.
 *   4. Re-read the full 0xAC LX header into a local buffer.
 *   5. obj_tbl_off = e_lfanew + lx_header[+0x40] (object_table_offset).
 *   6. For each of lx_header[+0x44] (number_of_objects) records: read
 *      the 0x18-byte record (chaining obj_tbl_off through the chunk
 *      reader's returned end position) and add record[+0x00]
 *      (object virtual size) to the accumulator.
 *   7. close(handle) only when bit0 is clear.
 *   8. return number_of_objects*15 + Sum(virtual_size).
 *
 * Return formula (num_obj*15 + Sum vsize) is an FD2-specific aggregate,
 * not a standard loader-API result. __cdecl: the sole caller pushes 2
 * args and cleans up with ADD ESP,0x8; the body ends with a plain RET.
 * ---------------------------------------------------------------- */
int crt_equivalent_lx_header_reader_36344(char *path, uint8 mode_byte)
{
    uint8  lx_header[0xAC];      /* full 0xAC-byte LX header copy        */
    int    obj_record[6];        /* 0x18-byte object-table record buffer */
    uint32 lx_magic_buf;         /* 4-byte slot; low 2 bytes = LX magic  */
    int    e_lfanew;             /* LX header file offset (MZ+0x3C)      */
    int    obj_tbl_off;          /* running object-table read position   */
    uint32 num_objects;          /* lx_header[+0x44]                     */
    int    handle;
    int    acc;
    uint32 i;

    lx_magic_buf = 0x00002020;   /* pre-fill; read overwrites low 2 bytes */
    acc = 0;

    if ((mode_byte & 1) != 0) {
        handle = (int)path;
    }
    else {
        handle = open(path, 0x200);
        if (handle == -1) {
            return 0;
        }
    }

    crt_equivalent_lx_chunk_read_36107(handle, 0x3c, mode_byte, &e_lfanew, 4);
    crt_equivalent_lx_chunk_read_36107(handle, e_lfanew, mode_byte,
                                       &lx_magic_buf, 2);

    if (strcmp((char *)&lx_magic_buf, "LX") != 0) {
        close(handle);
        return 0;
    }

    crt_equivalent_lx_chunk_read_36107(handle, e_lfanew, mode_byte,
                                       lx_header, 0xac);
    num_objects = *(uint32 *)(lx_header + 0x44);
    obj_tbl_off = e_lfanew + *(int *)(lx_header + 0x40);

    for (i = 0; i < num_objects; i++) {
        obj_tbl_off = crt_equivalent_lx_chunk_read_36107(handle, obj_tbl_off,
                                                         mode_byte,
                                                         obj_record, 0x18);
        acc += obj_record[0];
    }

    if ((mode_byte & 1) == 0) {
        close(handle);
    }
    return num_objects * 0xf + acc;
}
