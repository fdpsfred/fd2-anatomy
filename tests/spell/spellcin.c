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
 *
 * fd2_play_rising_pre_cast_effect @ 0x2189a is the same category: its only
 * computed state is caster_screen_x/y (a fixed tile->pixel transform) and the
 * per-frame radius accumulator initial_height += rise_step. Both are observable
 * only through the radius argument of fd2_render_circle_anim_row, which runs
 * FPU sqrt and writes into the large game-state buffer; every one of its 10
 * frames also memmove()s the backdrop, repaints chars, and unconditionally
 * blits the composed scene to the hardcoded mode-13h framebuffer literal
 * 0xA0504 (not redirectable via globals). There is no branch-free numeric path
 * that avoids the VRAM blit, so its behavioral verification is likewise
 * deferred to Phase 9 integration.
 *
 * fd2_dispatch_variant_b_cast @ 0x21b18 (variant-B heal worker) is also
 * deferred for the same reason. Its only computed logic is the per-target heal
 * loop (target_id = p_targets[i]; heal = fd2_apply_heal_spell_to_target(...);
 * fd2_show_damage_number(heal, 'i', target_id)) plus the AOE-count reset and
 * MP deduct. But that loop is gated behind two real animation passes that run
 * first: fd2_animate_spell_impact_per_target and
 * fd2_animate_status_effect_overlay_flicker (both emitted in anim/anicombt.c),
 * each of which mallocs a 0x25680 snapshot, runs FPU/SFX/BIOS-tick work, and
 * unconditionally blits to the hardcoded VRAM literal 0xA0504. Those two are
 * real linked functions, not stubs, so the heal loop cannot be reached in the
 * hosted TEST.EXE without writing to real VRAM. Behavioral verification of the
 * heal loop (and the return-value forwarding from apply_heal to
 * show_damage_number) is therefore deferred to Phase 9 integration.
 *
 * fd2_execute_aoe_spell_with_caster_portrait_radial_scatter @ 0x21bd0 is an
 * ORPHAN / UNREACHABLE AoE radial-scatter cinematic: it has no caller, is not
 * in the spell dispatch table, and its address never appears as a function
 * pointer. It is emitted verbatim for completeness but is unreachable from any
 * live code path, so there is no in-game entry to drive. Its per-frame work is
 * also pure VGA/VRAM cinematic: the sprite scatter geometry lives entirely in
 * fd2_scatter_sprite_around_origin_with_random_offset (FPU cos/sin, not yet
 * emitted), every frame memmove()s the backdrop into the large game-state
 * buffer and unconditionally blits the composed buffer to the hardcoded mode-13h
 * framebuffer literal 0xA0504 (not redirectable via globals), and the in-bounds
 * sprite blit goes through fd2_blit_palette_remap_with_sprite_mask (also not yet
 * emitted). The only branch-free arithmetic (the per-sprite y -= shrink_rate
 * rise + off-screen re-scatter test) operates on arrays populated by the FPU
 * scatter callee and is gated behind the unconditional VRAM blit. Its
 * verification is therefore deferred to Phase 9 integration along with the rest
 * of this file's cinematic workers.
 */

#include "testharn.h"
#include <stdio.h>

void run_spell_spellcin_tests(void)
{
    printf("Suite: spell/spellcin (deferred to Phase 9 integration -- "
           "pure VGA/VRAM cinematic, see file header)\n");
}
