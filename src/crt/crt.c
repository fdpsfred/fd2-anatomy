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
 *   crt_equivalent_lx_module_loader_3647b @ 0x3647b (0 callers)
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

/* ----------------------------------------------------------------
 * crt_equivalent_lx_module_loader_3647b @ 0x3647b  (0 callers)
 *
 * LX/LE module loader: reads MZ->LX header, parses the object table +
 * page table + fixup records, and writes the loaded image into either a
 * caller-supplied buffer or a freshly allocated one. Dead in FD2 (the
 * static-link single-LE never invokes runtime LE loading), but re-emitted
 * because the Watcom CRT __loadm-family obj is pulled in by the CLIB3S
 * overlay/dynamic-load chain and this body does not byte-match it.
 *
 * Args (3-arg __cdecl):
 *   path        - flags&1==0: null-terminated path to open;
 *                 flags&1==1: caller-supplied int handle reused as an
 *                 in-memory base (every chunk read becomes a memcpy).
 *   flags       - bit0 = use-caller-handle (skip open/close);
 *                 bit2 = allocate the output buffer via the AIL alloc
 *                        fnptr at 0x52758 (else use caller_buf).
 *   caller_buf  - output buffer; used only when flags&4==0 (when flags&4
 *                 is set, caller_buf is overwritten by the alloc result).
 *
 * Flow mirrors crt_equivalent_lx_header_reader_36344 for the header/object
 * walk, then additionally: (a) for each page reads min(remaining_obj_size,
 * page_byte_count) bytes into the running output cursor, applying a 16-byte
 * inter-object alignment skip on the first page of flag-3 (bit0|bit1)
 * objects; (b) walks the per-page fixup records, validates each record's
 * src/target type bits (invalid -> abort, close, return 0), and applies
 * 32-bit relocations (obj_base + target_disp) into the page buffer.
 *
 * The AIL alloc indirection at 0x52758 is a __cdecl fnptr taking the total
 * image size and returning the buffer (Ghidra renders it arg-less; the
 * disassembly pushes the size). Returns the output buffer, or 0 on any
 * failure (open fail / alloc fail / magic mismatch / bad fixup record).
 *
 * __cdecl: the body ends with a plain RET (no callee stack cleanup); a
 * caller would push 3 args and clean up with ADD ESP,0xC.
 * ---------------------------------------------------------------- */
void *crt_equivalent_lx_module_loader_3647b(char *path, int flags,
                                            void *caller_buf)
{
    uint32 page_base_table[100]; /* per-page output base, indexed by page#  */
    uint8  lx_header[0xAC];      /* full 0xAC-byte LX header copy            */
    uint32 obj_base_table[10];   /* obj_base_table[obj_idx+1] = object base  */
    int    obj_record[6];        /* 0x18-byte object-table record buffer     */
    int    page_entry[2];        /* 0x8-byte page-table entry buffer         */
    uint32 magic_buf;            /* 4-byte slot; low 2 bytes = LX magic      */
    int    e_lfanew;             /* LX header file offset (MZ+0x3C)          */
    void  *out_cursor;           /* running write cursor into the image      */
    void  *alloc_base;           /* base used for inter-object alignment      */
    uint32 total_size;           /* image size from the header reader        */
    uint32 obj_idx;
    uint32 page_idx;
    uint32 page_count;           /* running page index for page_base_table   */
    int    handle;
    int    obj_tbl_off;          /* running object-table read position       */
    int    page_tbl_off;         /* running page-table read position         */
    int    save_obj_off;         /* object-loop saved obj_tbl_off            */
    int    save_page_off;        /* page-loop saved page_tbl_off             */
    int    fixup_off;            /* running fixup read position              */
    int    save_fixup_off;       /* fixup-record-loop saved fixup_off        */
    uint32 prev_cum;             /* previous cumulative fixup-page offset     */
    uint32 this_cum;             /* current cumulative fixup-page offset      */
    uint32 fixup_page_idx;       /* outer fixup-page loop index              */
    uint32 rec_cursor;           /* running fixup-record byte position       */
    uint32 obj_remaining;        /* bytes left of the current object         */
    uint32 page_bytes;           /* bytes contributed by the current page    */
    uint8  src_type;             /* fixup source-flags byte                  */
    uint8  target_type;          /* fixup target-flags byte                  */
    uint8  target_obj;           /* fixup target object number               */
    uint16 src_offset;           /* fixup source offset within the page      */
    uint16 target_disp;          /* fixup target displacement                */

    magic_buf = 0x00002020;      /* pre-fill; magic read overwrites low 2B   */
    page_count = 0;

    handle = (int)path;
    if (((flags & 1) == 0) &&
        (handle = open(path, 0x200), handle == -1)) {
        return (void *)0;
    }

    total_size = crt_equivalent_lx_header_reader_36344(path, (uint8)flags);

    if ((flags & 4) != 0) {
        caller_buf = (*(void *(*)(uint32))data_ail_alloc_fnptr)(total_size);
        if (caller_buf == (void *)0) {
            if ((flags & 1) == 0) {
                close(handle);
            }
            return (void *)0;
        }
    }

    memset(caller_buf, 0, total_size);
    alloc_base = caller_buf;
    out_cursor = caller_buf;

    crt_equivalent_lx_chunk_read_36107(handle, 0x3c, (uint8)flags,
                                       &e_lfanew, 4);
    crt_equivalent_lx_chunk_read_36107(handle, e_lfanew, (uint8)flags,
                                       &magic_buf, 2);
    if (strcmp((char *)&magic_buf, "LX") != 0) {
        if ((flags & 1) == 0) {
            close(handle);
        }
        return (void *)0;
    }

    crt_equivalent_lx_chunk_read_36107(handle, e_lfanew, (uint8)flags,
                                       lx_header, 0xac);

    obj_tbl_off = e_lfanew + *(int *)(lx_header + 0x40);
    for (obj_idx = 0; obj_idx < *(uint32 *)(lx_header + 0x44); obj_idx++) {
        save_obj_off = crt_equivalent_lx_chunk_read_36107(
            handle, obj_tbl_off, (uint8)flags, obj_record, 0x18);
        page_tbl_off = e_lfanew + *(int *)(lx_header + 0x48);
        for (page_idx = 0; page_idx < *(uint32 *)((uint8 *)obj_record + 0x10);
             page_idx++) {
            page_tbl_off = crt_equivalent_lx_chunk_read_36107(
                handle, page_tbl_off, (uint8)flags, page_entry, 8);
            if (page_idx == 0) {
                if (((*((uint8 *)obj_record + 0x08) & 2) != 0) &&
                    ((*((uint8 *)obj_record + 0x08) & 1) != 0) &&
                    (((uint32)out_cursor & 0xf) != 0) && (obj_idx != 0)) {
                    out_cursor = (void *)((int)out_cursor +
                                 (0x10 - ((uint32)alloc_base & 0xf)));
                }
                obj_base_table[obj_idx + 1] = (uint32)out_cursor;
            }
            page_base_table[page_count] = (uint32)out_cursor;
            save_page_off = page_tbl_off;
            page_count++;
            obj_remaining = (uint32)obj_record[0] -
                ((uint32)out_cursor - obj_base_table[obj_idx + 1]);
            page_bytes = (uint32)*(uint16 *)((uint8 *)page_entry + 0x04);
            if (obj_remaining < page_bytes) {
                page_bytes = obj_remaining;
            }
            crt_equivalent_lx_chunk_read_36107(
                handle,
                (page_entry[0] << (*(uint8 *)(lx_header + 0x2c) & 0x1f)) +
                    *(int *)(lx_header + 0x80),
                (uint8)flags, out_cursor, page_bytes);
            out_cursor = (void *)((int)out_cursor + page_bytes);
            page_tbl_off = save_page_off;
        }
        obj_tbl_off = save_obj_off;
    }

    fixup_off = crt_equivalent_lx_chunk_read_36107(
        handle, e_lfanew + *(int *)(lx_header + 0x68), (uint8)flags,
        &prev_cum, 4);
    fixup_page_idx = 0;
    for (;;) {
        if (*(uint32 *)(lx_header + 0x14) <= fixup_page_idx) {
            if ((flags & 1) == 0) {
                close(handle);
            }
            return caller_buf;
        }
        fixup_off = crt_equivalent_lx_chunk_read_36107(
            handle, fixup_off, (uint8)flags, &this_cum, 4);
        if (prev_cum != this_cum) {
            rec_cursor = prev_cum + e_lfanew + *(int *)(lx_header + 0x6c);
            save_fixup_off = fixup_off;
            do {
                fixup_off = crt_equivalent_lx_chunk_read_36107(
                    handle, rec_cursor, (uint8)flags, &src_type, 1);
                fixup_off = crt_equivalent_lx_chunk_read_36107(
                    handle, fixup_off, (uint8)flags, &target_type, 1);
                if (((src_type & 7) == 0) ||
                    (((target_type & 4) != 0) &&
                     ((target_type & 0x20) == 0)) ||
                    ((src_type & 0x20) != 0)) {
                    goto loader_abort;
                }
                fixup_off = crt_equivalent_lx_chunk_read_36107(
                    handle, fixup_off, (uint8)flags, &src_offset, 2);
                fixup_off = crt_equivalent_lx_chunk_read_36107(
                    handle, fixup_off, (uint8)flags, &target_obj, 1);
                rec_cursor = crt_equivalent_lx_chunk_read_36107(
                    handle, fixup_off, (uint8)flags, &target_disp, 2);
                if (src_offset <= *(uint32 *)(lx_header + 0x28)) {
                    *(uint32 *)(page_base_table[fixup_page_idx] + src_offset) =
                        obj_base_table[target_obj] + target_disp;
                }
                fixup_off = save_fixup_off;
            } while (rec_cursor <
                     (uint32)(this_cum + e_lfanew +
                              *(int *)(lx_header + 0x6c)));
        }
        prev_cum = this_cum;
        fixup_page_idx++;
    }

loader_abort:
    if ((flags & 1) == 0) {
        close(handle);
    }
    return (void *)0;
}
