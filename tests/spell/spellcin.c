/*
 * unit tests for src/spell/spellcin.c
 *
 * fd2_cast_earthquake_spell_with_screen_shake @ 0x21548 is pure full-screen
 * cinematic orchestration: its fast-tremor loop unconditionally runs
 *   fd2_blit_rectangle(0xA0504, 0x140, buf+0x8088, 0x1C8, 0x138, 0xC0)
 * 60 times, and fd2_blit_rectangle is the real emitted blitter that
 * memmove()s into the absolute destination — here the DOS VGA framebuffer at
 * 0xA0000. The slow-shake loop likewise drives the real
 * fd2_blit_scaled_tile_map_view / fd2_paint_char_sprite_at_world_pos against
 * the live tile snapshot. None of that can execute in the hosted TEST.EXE
 * without writing to real VRAM, and the 0xA0504 destination is a hardcoded
 * literal in the emit code (not redirectable via globals).
 *
 * Behavioral verification (MP deduct, AOE-count reset, snapshot/large-buffer
 * save+restore, the per-target fd2_calc_magic_damage hit/miss branch, and the
 * SFX cadence) is therefore deferred to Phase 9 integration on DOSBox-X,
 * matching the project convention for VGA/VRAM-touching workers (see the
 * status-screen modal deferral note in tests/testglob.c). No isolated path
 * reaches the damage loop without first executing the VRAM blits.
 */

#include "testharn.h"
#include <stdio.h>

void run_spell_spellcin_tests(void)
{
    printf("Suite: spell/spellcin (deferred to Phase 9 integration -- "
           "pure VGA/VRAM cinematic, see file header)\n");
}
