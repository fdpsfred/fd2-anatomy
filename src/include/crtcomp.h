#ifndef CRT_COMPAT_H
#define CRT_COMPAT_H

#include "types.h"

/*
 * Prototypes for the crt_equivalent_* functions.
 * These behave identically to Watcom CRT counterparts but do not
 * byte-match any CLIB3S .obj, so they are emitted as FD2 source.
 *
 * See rebuild_info/crt/symbol_inventory.md for details.
 *
 * The authoritative prototypes for emitted crt_equivalent_* functions
 * live in protos.h (the header every .c includes). Entries here are kept
 * in sync as each function is emitted.
 *
 * NOTE: crt_equivalent_linker_padding_4cbce (0x4cbce) is intentionally
 * absent: it is a lone 0xC3 byte of Watcom CRT linker alignment padding
 * between __int7 and __init_80x87 (zero xref, zero caller). It is not a
 * real function and is not emitted; wlink re-pads the segment on relink.
 *
 * NOTE: 0x4C630 (now crt_emu387_int7_fptan_opcode_worker_4c630 in Ghidra)
 * is intentionally absent: it was reclassified to link_vendor_lib as an
 * internal __int7 subroutine (the x87 FPTAN-opcode software-emulation worker
 * of emu387.obj). Its bytes lie inside the byte-matched __int7 PUBDEF body
 * (0x49D98..0x4CBCD) and have no separate PUBDEF, so it is resolved by linking
 * that one module and is not emitted as FD2 C source.
 */

/* _disable primitive pair */
unsigned long crt_equivalent_get_eflags(void);
unsigned long crt_equivalent_get_eflags_thunk(void);

/* LX module loader chain */
int  crt_equivalent_lx_chunk_read(int file_handle, int offset,
                                        uint8 mode, void *dest, uint32 length);
void crt_equivalent_lx_header_reader(void);
void crt_equivalent_lx_module_loader(void);

/* exit / error handlers */
void crt_equivalent_atexit_default_stub(void);
void crt_equivalent_fpe_default_handler(void);
int  crt_equivalent_matherr_default_thunk(void *exc);
int  crt_equivalent_matherr_default_return_zero(void *exc);

#endif /* CRT_COMPAT_H */
