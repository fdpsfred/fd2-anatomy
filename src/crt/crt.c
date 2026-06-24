/*
 * crt.c — FD2-specific CRT-equivalent routines.
 *
 * These functions behave like Watcom CRT helpers but do not byte-match
 * any CLIB3S / MATH .obj, so they are re-emitted as FD2 rebuild source
 * rather than linked from the vendor library.
 *
 * Functions in this file:
 *   crt_equivalent_lx_chunk_read  @ 0x36107  (2 callers)
 *   crt_equivalent_lx_header_reader @ 0x36344 (1 caller)
 *   crt_equivalent_lx_module_loader @ 0x3647b (0 callers)
 *   crt_equivalent_exit_chain_stub_36de3 @ 0x36de3 (2 callers)
 *   crt_equivalent_get_eflags_thunk     @ 0x37f86 (2 callers)
 *   crt_equivalent_get_eflags           @ 0x3ed58 (0 callers; thunk JMP target)
 *   crt_equivalent_fpe_default_handler_3d26e @ 0x3d26e (2 callers)
 *   crt_equivalent_matherr_default_thunk_4d340 @ 0x4d340 (1 caller)
 *   crt_equivalent_matherr_default_return_zero_4d8ea @ 0x4d8ea (0 callers)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>     /* memcpy, strcmp */
#include <io.h>         /* open, close, lseek, read, SEEK_SET */
#include <fcntl.h>      /* O_* flags for open() */

/* ----------------------------------------------------------------
 * crt_equivalent_lx_chunk_read @ 0x36107
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
int crt_equivalent_lx_chunk_read(int file_handle, int offset,
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
 * crt_equivalent_lx_header_reader @ 0x36344  (1 caller)
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
int crt_equivalent_lx_header_reader(char *path_or_base, uint8 mode_byte)
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
        handle = (int)path_or_base;
    }
    else {
        handle = open(path_or_base, 0x200);
        if (handle == -1) {
            return 0;
        }
    }

    crt_equivalent_lx_chunk_read(handle, 0x3c, mode_byte, &e_lfanew, 4);
    crt_equivalent_lx_chunk_read(handle, e_lfanew, mode_byte,
                                       &lx_magic_buf, 2);

    if (strcmp((char *)&lx_magic_buf, "LX") != 0) {
        close(handle);
        return 0;
    }

    crt_equivalent_lx_chunk_read(handle, e_lfanew, mode_byte,
                                       lx_header, 0xac);
    num_objects = *(uint32 *)(lx_header + 0x44);
    obj_tbl_off = e_lfanew + *(int *)(lx_header + 0x40);

    for (i = 0; i < num_objects; i++) {
        obj_tbl_off = crt_equivalent_lx_chunk_read(handle, obj_tbl_off,
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
 * crt_equivalent_lx_module_loader @ 0x3647b  (0 callers)
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
 * Flow mirrors crt_equivalent_lx_header_reader for the header/object
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
void *crt_equivalent_lx_module_loader(char *path, int flags,
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

    total_size = crt_equivalent_lx_header_reader(path, (uint8)flags);

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

    crt_equivalent_lx_chunk_read(handle, 0x3c, (uint8)flags,
                                       &e_lfanew, 4);
    crt_equivalent_lx_chunk_read(handle, e_lfanew, (uint8)flags,
                                       &magic_buf, 2);
    if (strcmp((char *)&magic_buf, "LX") != 0) {
        if ((flags & 1) == 0) {
            close(handle);
        }
        return (void *)0;
    }

    crt_equivalent_lx_chunk_read(handle, e_lfanew, (uint8)flags,
                                       lx_header, 0xac);

    obj_tbl_off = e_lfanew + *(int *)(lx_header + 0x40);
    for (obj_idx = 0; obj_idx < *(uint32 *)(lx_header + 0x44); obj_idx++) {
        save_obj_off = crt_equivalent_lx_chunk_read(
            handle, obj_tbl_off, (uint8)flags, obj_record, 0x18);
        page_tbl_off = e_lfanew + *(int *)(lx_header + 0x48);
        for (page_idx = 0; page_idx < *(uint32 *)((uint8 *)obj_record + 0x10);
             page_idx++) {
            page_tbl_off = crt_equivalent_lx_chunk_read(
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
            crt_equivalent_lx_chunk_read(
                handle,
                (page_entry[0] << (*(uint8 *)(lx_header + 0x2c) & 0x1f)) +
                    *(int *)(lx_header + 0x80),
                (uint8)flags, out_cursor, page_bytes);
            out_cursor = (void *)((int)out_cursor + page_bytes);
            page_tbl_off = save_page_off;
        }
        obj_tbl_off = save_obj_off;
    }

    fixup_off = crt_equivalent_lx_chunk_read(
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
        fixup_off = crt_equivalent_lx_chunk_read(
            handle, fixup_off, (uint8)flags, &this_cum, 4);
        if (prev_cum != this_cum) {
            rec_cursor = prev_cum + e_lfanew + *(int *)(lx_header + 0x6c);
            save_fixup_off = fixup_off;
            do {
                fixup_off = crt_equivalent_lx_chunk_read(
                    handle, rec_cursor, (uint8)flags, &src_type, 1);
                fixup_off = crt_equivalent_lx_chunk_read(
                    handle, fixup_off, (uint8)flags, &target_type, 1);
                if (((src_type & 7) == 0) ||
                    (((target_type & 4) != 0) &&
                     ((target_type & 0x20) == 0)) ||
                    ((src_type & 0x20) != 0)) {
                    goto loader_abort;
                }
                fixup_off = crt_equivalent_lx_chunk_read(
                    handle, fixup_off, (uint8)flags, &src_offset, 2);
                fixup_off = crt_equivalent_lx_chunk_read(
                    handle, fixup_off, (uint8)flags, &target_obj, 1);
                rec_cursor = crt_equivalent_lx_chunk_read(
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

/* ----------------------------------------------------------------
 * crt_equivalent_exit_chain_stub_36de3 @ 0x36de3  (2 callers)
 *
 * atexit default no-op handler. 1-byte RET stub. The Watcom CRT
 * initializes the three atexit chain slots @ 0x527d8 / 0x527dc / 0x527e0
 * with a pointer to this RET. When exit / _exit walk the chain via
 * CALL [0x527d8] (etc.), an empty slot lands on this stub and returns
 * immediately, so an unregistered atexit slot is a harmless no-op. When
 * the program calls atexit(fn), the slot is overwritten with fn instead.
 *
 * Its address is taken (referenced as DATA from the three atexit slots),
 * so it must remain a real, callable function — not folded away.
 *
 * __cdecl, no parameters, no return value: the original body is the
 * single instruction RET (no callee stack cleanup; callers do not adjust
 * ESP because nothing was pushed). No CALL inside, so there is no
 * EAX-tracking concern.
 * ---------------------------------------------------------------- */
void crt_equivalent_exit_chain_stub_36de3(void)
{
    return;
}

/* ----------------------------------------------------------------
 * crt_equivalent_get_eflags_thunk @ 0x37f86  (2 callers)
 *
 * Watcom `_disable`-style critical-section entry primitive. The two AIL
 * ISRs reach it by near CALL:
 *   AIL_internal_driver_timer_isr @ 0x3f22a: CALL 0x37f86; MOV [..],EAX
 *   AIL_internal_audio_mix_isr    @ 0x4019a: CALL 0x37f86; MOV EDI,EAX
 *                                            ; MOV [..],EAX
 * Both immediately save the returned EAX, so the function RETURNS the prior
 * EFLAGS image (consumed by the caller, later restored via PUSH+POPFD on
 * critical-section exit). It is __cdecl with no parameters: the body ends
 * in a plain near RET and the callers push nothing / do not adjust ESP.
 *
 * The original 0x37f86 is a 5-byte JMP into the 4-byte primitive
 * crt_equivalent_get_eflags @ 0x3ed58:
 *     0x3ed58: PUSHFD ; 0x3ed59: POP EAX ; 0x3ed5a: CLI ; 0x3ed5b: RET
 * i.e. it atomically (a) captures the current EFLAGS into EAX and (b)
 * disables interrupts (clears IF) so the ISR can run its body in a critical
 * section. crt_equivalent_get_eflags is a SEPARATE emit target; this thunk
 * must not re-emit it.
 *
 * Emit form: a tail-JMP into a distinct symbol carries a symbolic relative
 * displacement that #pragma aux opcode bytes cannot encode, and re-emitting
 * the target (crt_equivalent_get_eflags) here would duplicate a function
 * owned by another unit. The thunk is therefore expressed as the equivalent
 * in-line primitive itself (PUSHFD; POP EAX; CLI), which is functionally
 * identical at the call boundary — same EAX return (prior EFLAGS) and the
 * same IF=0 side effect. The JMP-vs-inline difference is a Layer 3
 * (byte-exact) detail the project does not pursue; the Layer 2
 * (functionally-exact) contract is met.
 *
 * It is emitted as a REAL out-of-line function (not a header-only #pragma
 * aux) so it has a genuine PUBDEF symbol: both the AIL vendor .obj EXTDEF
 * (the ISR near-CALL at link integration) and any address-taken use resolve
 * to it. Watcom 9.5a never emits a standalone symbol for a pragma-aux
 * in-line function (an address-take becomes an undefined external), so the
 * raw opcodes live in an in-line helper (crt_capture_eflags_cli) that is
 * only ever called (never address-taken) and therefore expands in place
 * with no symbol of its own; this externally-linked wrapper splices it into
 * a real callable body (the optimiser inlines the helper ->
 * PUSHFD; POP EAX; CLI; RET). A pragma-aux in-line function must have
 * external linkage in 9.5a -- a `static` one yields E1035 "not defined" --
 * hence the bare-extern helper.
 * ---------------------------------------------------------------- */
extern unsigned long crt_capture_eflags_cli(void);
#pragma aux crt_capture_eflags_cli = \
    0x9c    /* pushfd  : push EFLAGS                       */ \
    0x58    /* pop eax : EAX = prior EFLAGS (return value) */ \
    0xfa    /* cli     : disable interrupts (IF -> 0)      */ \
    value [eax] modify exact [eax];

unsigned long crt_equivalent_get_eflags_thunk(void)
{
    return crt_capture_eflags_cli();
}

/* ----------------------------------------------------------------
 * crt_equivalent_get_eflags @ 0x3ed58  (0 callers)
 *
 * The 4-byte Watcom `_disable` primitive that the thunk @ 0x37f86 above
 * JMPs into. It captures the current EFLAGS into EAX and disables
 * interrupts (clears IF), so the AIL ISRs run their body in a critical
 * section, later restoring the prior interrupt state via PUSH+POPFD on
 * exit. No direct callers: it is reached only through the thunk's JMP
 * (and is address-taken nowhere else), but it owns its own PUBDEF so the
 * thunk's JMP target resolves to a real symbol.
 *
 * Original body (4 bytes):
 *     0x3ed58: PUSHFD ; 0x3ed59: POP EAX ; 0x3ed5a: CLI ; 0x3ed5b: RET
 * i.e. EAX = prior EFLAGS image (the return value), with IF cleared as a
 * side effect. The decompiler renders this as an EFLAGS bit-reassembly
 * expression (its way of showing PUSHFD;POP EAX) and cannot represent the
 * CLI; the disassembly is authoritative.
 *
 * Emit form: identical to the thunk above -- the raw opcodes
 * PUSHFD; POP EAX; CLI are spliced in from the shared #pragma aux in-line
 * helper crt_capture_eflags_cli (declared once above; reused here, NOT
 * re-declared). The optimiser inlines it, so this externally-linked
 * wrapper expands to PUSHFD; POP EAX; CLI; RET -- exactly the original
 * 4-byte body. As anticipated when the thunk was emitted, the program now
 * holds two byte copies of these opcodes (the thunk's inline copy and this
 * one); that is Layer 2 (functionally exact) -- byte-exact deduplication of
 * the JMP-to-shared-target structure is a Layer 3 detail not pursued.
 *
 * __cdecl unsigned long(void): no parameters, returns EFLAGS in EAX. The
 * original body ends in a plain near RET (no callee stack cleanup); no CALL
 * precedes the EAX result (the helper's PUSHFD produces it directly), so
 * there is no EAX-tracking concern.
 * ---------------------------------------------------------------- */
unsigned long crt_equivalent_get_eflags(void)
{
    return crt_capture_eflags_cli();
}

/* ----------------------------------------------------------------
 * crt_equivalent_fpe_default_handler_3d26e @ 0x3d26e  (2 callers)
 *
 * SIGFPE / FPU-exception default no-op handler. 1-byte RET stub. The
 * Watcom CRT seeds the FPE-handler dispatch slot @ 0x5283c with a pointer
 * to this RET. The exception deliverers invoke the slot indirectly:
 *   __FPE_exception_ @ 0x4d3d6 : PUSH EAX; CALL [0x5283c]; ADD ESP,4
 *   __int7           @ 0x49feb : MOVZX EAX,AH; CALL [0x5283c]
 * If the program never called signal(SIGFPE, fn), the slot still points
 * here and the indirect CALL lands on this stub, which returns at once,
 * making an unhandled FPU exception a harmless no-op. When the program
 * calls signal(SIGFPE, fn), the slot is overwritten with fn instead.
 *
 * Its address is taken (referenced as DATA from the dispatch slot
 * @ 0x5283c), so it must remain a real, callable function — not folded
 * away.
 *
 * __cdecl void(int fpe_code): the FPE code is the argument the deliverer
 * supplies (pushed by __FPE_exception_ as a cdecl stack arg; placed in
 * EAX by __int7). The original body is the single instruction RET — it
 * neither reads the argument nor cleans the stack (plain RET, not RET 4),
 * so the cdecl caller is responsible for reclaiming the pushed arg, which
 * __FPE_exception_ does via ADD ESP,4. No CALL inside, so there is no
 * EAX-tracking concern.
 * ---------------------------------------------------------------- */
void crt_equivalent_fpe_default_handler_3d26e(int fpe_code)
{
    (void)fpe_code;
    return;
}

/* ----------------------------------------------------------------
 * crt_equivalent_matherr_default_thunk_4d340 @ 0x4d340  (1 caller)
 *
 * Default value of the user-matherr-handler slot @ 0x539A8. _matherr
 * (vendor CLIB3S obj) reads the slot and CALLs it before any diagnostic
 * output: PUSH exc; CALL [0x539A8]; ADD ESP,4 (__cdecl, 1 stack arg, the
 * exception-struct pointer; caller cleans up). A handler returning 0 means
 * "I did not handle this error", so _matherr proceeds with its default
 * behaviour (fputs diagnostic + __set_EDOM/__set_ERANGE + the struct's
 * pre-populated retval). _set_matherr overwrites the slot to install a
 * custom handler, bypassing this thunk.
 *
 * Original body is a single 5-byte JMP:
 *     0x4d340: JMP 0x4d8ea
 * forwarding to the separate emit target crt_equivalent_matherr_default_
 * return_zero_4d8ea @ 0x4d8ea, whose body is the "return 0" primitive
 * (PUSH EBP; MOV EBP,ESP; XOR EAX,EAX; POP EBP; RET). Net effect at the
 * call boundary: EAX = 0, stack balanced.
 *
 * Its address is taken — it is the default contents of slot [0x539A8]
 * (written by _set_matherr, read+CALLed by _matherr) — so it must remain a
 * real, callable function with a genuine PUBDEF symbol, not folded away.
 *
 * Emit form (mirrors crt_equivalent_get_eflags_thunk @ 0x37f86): the control transfer MUST be
 * a JMP to a distinct symbol, not a C `return ...4d8ea();` CALL. The
 * original thunk has no frame at all — it JMPs straight through, and 0x4d8ea
 * RETs directly back to _matherr. A C return-call would push a 4-byte
 * return address and route the RET back here instead of to _matherr; the
 * tail-JMP semantics (and byte-level fidelity) are preserved only by an
 * actual JMP. crt_equivalent_matherr_default_return_zero_4d8ea is a SEPARATE
 * emit target (its own routing.json entry, same target file); it must NOT be
 * re-emitted here.
 *
 * A tail-JMP to a distinct symbol carries a symbolic relative displacement
 * that raw #pragma aux opcode bytes cannot encode, so the JMP is expressed
 * via a #pragma aux in-line helper whose body is the single instruction
 * `jmp <target>` (Watcom resolves the symbol with a relocation). The helper
 * (crt_matherr_jmp_to_return_zero) is only ever called, never address-taken,
 * so Watcom 9.5a expands it in place with no symbol of its own.
 * crt_equivalent_matherr_default_thunk_4d340 is a REAL out-of-line function
 * so it owns the PUBDEF that slot [0x539A8] resolves to. It has no locals and
 * no stack frame, so Watcom emits no __CHK probe / prologue before the JMP —
 * ESP reaches the target exactly as _matherr left it (exc still pushed,
 * cleaned by _matherr's ADD ESP,4 after control returns). Verified emitted
 * body (WDISASM): `E9 <disp32> jmp crt_equivalent_matherr_default_return_zero
 * _4d8ea` — a single relative JMP, matching the original 5-byte `JMP 0x4d8ea`.
 * Watcom also appends an unreachable `xor eax,eax; ret` (the int wrapper's
 * `return 0;` epilogue) after the JMP; it is never executed because the JMP
 * always transfers control away. See the per-function note at the definition.
 *
 * __cdecl int(void *exc): the matherr ABI passes the exception-struct
 * pointer as a single cdecl stack arg (PUSH exc by _matherr). The thunk
 * never reads it (it JMPs straight through); it is declared only so the
 * thunk's PUBDEF carries the correct cdecl signature for the slot. The
 * return value is 0, produced by the JMP target in EAX. No CALL precedes any
 * EAX use here, so there is no EAX-tracking concern.
 * ---------------------------------------------------------------- */
extern int crt_equivalent_matherr_default_return_zero_4d8ea(void *exc);

extern void crt_matherr_jmp_to_return_zero(void);
#pragma aux crt_matherr_jmp_to_return_zero = \
    "jmp crt_equivalent_matherr_default_return_zero_4d8ea";

/* The helper's `jmp` is the entire executed body and matches the original
 * 5-byte `JMP 0x4d8ea`. The trailing `return 0;` is required only to silence
 * Watcom's W107 (this is an int-typed function); the optimiser keeps it as an
 * unreachable `xor eax,eax; ret` epilogue AFTER the jmp, which is never
 * executed (the jmp always transfers control away). Those few unreachable
 * trailing bytes are the only divergence from the original — a Layer 3
 * (byte-exact) thunk-tail detail the project does not pursue; the Layer 2
 * (functionally-exact) contract is met (return 0 in EAX, stack balanced, the
 * jmp tail-transfers so 0x4d8ea's RET returns straight to _matherr). */
int crt_equivalent_matherr_default_thunk_4d340(void *exc)
{
    (void)exc;
    crt_matherr_jmp_to_return_zero();
    return 0;
}

/* ----------------------------------------------------------------
 * crt_equivalent_matherr_default_return_zero_4d8ea @ 0x4d8ea  (0 callers)
 *
 * The "return 0" primitive the default _matherr handler forwards to. It is
 * the JMP target of crt_equivalent_matherr_default_thunk_4d340 @ 0x4d340
 * (slot [0x539A8]'s default contents); no direct callers -- control only
 * arrives via that thunk's tail JMP, then RETs straight back to _matherr.
 * Returning 0 signals "I did not handle this error", so _matherr proceeds
 * with its default behaviour.
 *
 * Original body (7 bytes): PUSH EBP; MOV EBP,ESP; XOR EAX,EAX; POP EBP; RET
 * -- the standard Watcom prologue/epilogue around `return 0`. This is exactly
 * what Watcom 9.5a emits for an `int f(args){ return 0; }` with a referenced
 * (kept) frame, so the C source below reproduces it.
 *
 * __cdecl int(void *exc): the matherr ABI passes the exception-struct pointer
 * as a single cdecl stack arg. The body never reads it (it just returns 0);
 * the parameter exists so the symbol _matherr binds to (via the thunk/slot)
 * carries the correct cdecl signature. RET has no operand (caller -- _matherr,
 * via its ADD ESP,4 -- cleans the arg), confirming __cdecl. The only EAX write
 * is `XOR EAX,EAX`; no CALL precedes it, so there is no EAX-tracking concern.
 * ---------------------------------------------------------------- */
int crt_equivalent_matherr_default_return_zero_4d8ea(void *exc)
{
    (void)exc;
    return 0;
}
