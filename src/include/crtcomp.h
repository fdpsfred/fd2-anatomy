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
 */

/* cstart pair */
void crt_equivalent_entry_start(void);
void crt_equivalent_dos_main_bootstrap(void);

/* _disable primitive pair */
unsigned long crt_equivalent_get_eflags(void);
unsigned long crt_equivalent_get_eflags_thunk(void);

/* LX module loader chain */
int  crt_equivalent_lx_chunk_read_36107(int file_handle, int offset,
                                        uint8 mode, void *dest, uint32 length);
void crt_equivalent_lx_header_reader_36344(void);
void crt_equivalent_lx_module_loader_3647b(void);

/* exit / error handlers */
void crt_equivalent_exit_chain_stub_36de3(void);
void crt_equivalent_fpe_default_handler_3d26e(void);
int  crt_equivalent_matherr_default_thunk_4d340(void *exc);
void crt_equivalent_matherr_default_return_zero_4d8ea(void);

/* softfp */
void crt_equivalent_softfp_tan_worker_4c630(void);

#endif /* CRT_COMPAT_H */
