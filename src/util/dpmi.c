/*
 * dpmi.c — DPMI (INT 31h) memory management wrappers
 */

#include "types.h"
#include "globals.h"
#include "protos.h"
#include <dos.h>
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_dpmi_alloc_dos_memory @ 0x361CC
 *
 * DPMI fn 0x100: allocate DOS conventional memory block.
 * Outputs linear address, real-mode segment, selector handle.
 * Locks allocated region via fd2_dpmi_lock_region.
 * ---------------------------------------------------------------- */
int fd2_dpmi_alloc_dos_memory(uint32 paragraphs,
    uint32 *out_linear, uint32 *out_segment, uint32 *out_selector)
{
    union REGS in_r, out_r;
    memset(&in_r, 0, sizeof(in_r));
    in_r.x.eax = 0x100;
    in_r.x.ebx = paragraphs;
    int386(0x31, &in_r, &out_r);
    if (out_r.x.cflag == 0) {
        *out_segment = out_r.x.eax << 16;
        *out_linear = (out_r.x.eax & 0xFFFF) << 4;
        *out_selector = out_r.x.edx & 0xFFFF;
        fd2_dpmi_lock_region(
            *out_segment >> 12,
            (paragraphs * 16 + (*out_segment >> 12)) - 1);
    }
    return out_r.x.cflag == 0;
}

/* ----------------------------------------------------------------
 * fd2_dpmi_free_dos_memory @ 0x36255
 *
 * DPMI fn 0x101: free DOS conventional memory by selector.
 * ---------------------------------------------------------------- */
void fd2_dpmi_free_dos_memory(uint32 selector)
{
    union REGS in_r, out_r;
    memset(&in_r, 0, sizeof(in_r));
    in_r.x.eax = 0x101;
    in_r.x.edx = selector & 0xFFFF;
    int386(0x31, &in_r, &out_r);
}

/* ----------------------------------------------------------------
 * fd2_dpmi_lock_region @ 0x36284
 *
 * DPMI fn 0x600: lock linear memory region (prevent page-out).
 * page_start and page_end are page numbers (addr >> 12).
 * ---------------------------------------------------------------- */
int fd2_dpmi_lock_region(uint32 page_start, uint32 page_end)
{
    union REGS in_r, out_r;
    uint32 base;
    uint32 size;

    base = (page_start < page_end) ? page_start : page_end;
    size = ((page_start >= page_end) ? page_start : page_end)
         - base + 1;
    memset(&in_r, 0, sizeof(in_r));
    in_r.x.eax = 0x600;
    in_r.x.ebx = base >> 16;
    in_r.x.ecx = base & 0xFFFF;
    in_r.x.esi = size >> 16;
    in_r.x.edi = size & 0xFFFF;
    int386(0x31, &in_r, &out_r);
    return out_r.x.cflag == 0;
}

/* ----------------------------------------------------------------
 * fd2_dpmi_unlock_region @ 0x362F1
 *
 * DPMI fn 0x601: unlock linear memory region.
 * Same parameter semantics as fd2_dpmi_lock_region.
 * ---------------------------------------------------------------- */
int fd2_dpmi_unlock_region(uint32 page_start, uint32 page_end)
{
    union REGS in_r, out_r;
    uint32 base;
    uint32 size;

    base = (page_start < page_end) ? page_start : page_end;
    size = ((page_start >= page_end) ? page_start : page_end)
         - base + 1;
    memset(&in_r, 0, sizeof(in_r));
    in_r.x.eax = 0x601;
    in_r.x.ebx = base >> 16;
    in_r.x.ecx = base & 0xFFFF;
    in_r.x.esi = size >> 16;
    in_r.x.edi = size & 0xFFFF;
    int386(0x31, &in_r, &out_r);
    return out_r.x.cflag == 0;
}

/* ----------------------------------------------------------------
 * fd2_dpmi_lock_size @ 0x36316
 *
 * Lock a region starting at `base` of `size` bytes.
 * Wrapper: calls fd2_dpmi_lock_region(base, base + size).
 * ---------------------------------------------------------------- */
int fd2_dpmi_lock_size(uint32 base, uint32 size)
{
    return fd2_dpmi_lock_region(base, base + size);
}

/* ----------------------------------------------------------------
 * fd2_dpmi_unlock_size @ 0x3632D
 *
 * Unlock a region starting at `base` of `size` bytes.
 * Wrapper: calls fd2_dpmi_unlock_region(base, base + size).
 * ---------------------------------------------------------------- */
int fd2_dpmi_unlock_size(uint32 base, uint32 size)
{
    return fd2_dpmi_unlock_region(base, base + size);
}
