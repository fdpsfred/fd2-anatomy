/*
 * noop.c — No-op linker-artifact stub
 */

#include "types.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_noop_ret_pad @ 0x4E915  (0 callers, verified 0 xref)
 *
 * 1-byte RET stub. Linker artifact sitting between
 * fd2_dialog_sprite_blit_mirrored @ 0x4E8E1 and
 * fd2_decode_dialog_pixel_byte @ 0x4E916 — a padding/placeholder
 * slot that never got a real definition. Body: RET, no stack frame.
 * ---------------------------------------------------------------- */
void fd2_noop_ret_pad(void)
{
}
