/*
 * btltab3.c -- battle summon-spell animation runtime state (.object2 BSS)
 *
 * Zero-initialized per-slot state arrays for the variant-A 6-slot summon-spell
 * animation state machine (fd2_tick_summon_anim_variant_a_6slot). All entries
 * start at zero and are filled at runtime during the INIT state (state_code==0)
 * before any TICK-state read; no static initializer in the original binary.
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array @ 0x53FFC  (24 bytes)
 *
 * Per-slot color index (one int32 per slot, 6 slots). Written by
 * fd2_tick_summon_anim_variant_a_6slot: INIT seeds slot i with i; the color
 * rotation path stores (rotation_counter % 10). Read in TICK frames as the
 * index into the local per-color vertical row-offset table. Accessed via
 * "[idx*4 + 0x53FFC]" DWORD moves (see disasm 0x26A4E / 0x26B0C / 0x26BB8),
 * confirming int32 elements with stride 4. Zero-bss (runtime-initialized).
 */
int32 data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[6];
