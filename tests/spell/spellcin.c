/*
 * unit tests for src/spell/spellcin.c
 *
 * Eight of the nine workers in this file are pure VGA/VRAM cinematic
 * orchestration and have no isolated numeric path that avoids a write to the
 * hardcoded mode-13h framebuffer literal 0xA0504 (not redirectable via
 * globals). Their behavioral verification is deferred to Phase 9 integration on
 * DOSBox-X, matching the project convention for VGA/VRAM-touching workers:
 *
 *   fd2_cast_earthquake_spell_with_screen_shake @ 0x21548 — its fast-tremor
 *   loop unconditionally blits 60 frames to 0xA0504; the per-target
 *   fd2_calc_magic_damage hit/miss branch is reached only after the blits.
 *
 *   fd2_play_rising_pre_cast_effect @ 0x2189a — only computed state is the
 *   caster_screen_x/y tile->pixel transform and the radius accumulator; both
 *   observable only through fd2_render_circle_anim_row, behind an
 *   unconditional per-frame blit to 0xA0504.
 *
 *   fd2_dispatch_variant_b_cast @ 0x21b18 — its heal loop is gated behind two
 *   real animation passes (fd2_animate_spell_impact_per_target,
 *   fd2_animate_status_effect_overlay_flicker), each of which blits to 0xA0504.
 *
 *   fd2_execute_aoe_spell_with_caster_portrait_radial_scatter @ 0x21bd0 —
 *   ORPHAN / UNREACHABLE AoE cinematic; every frame memmove()s the backdrop and
 *   unconditionally blits the composed buffer to 0xA0504.
 *
 *   fd2_play_variant_b_slide_pre_effect @ 0x21eb1 — variant-B spell pre-cast
 *   slide animation; its only computed state is the cursor tile->pixel
 *   transform and the radius accumulator, both observable only through
 *   fd2_render_filled_circle_band_anim (which writes the large game-state
 *   buffer), behind an unconditional per-frame blit to 0xA0504. No RNG / damage
 *   / state-transition branch exists to assert at unit level.
 *
 *   fd2_animate_warp_teleport_char @ 0x22253 — character warp/teleport
 *   cinematic. It fopens the real FDOTHER.DAT (warp SFX bank) via
 *   fd2_load_dat_resource, runs three warp sub-animations (portal-open,
 *   collapse, expand) that each blit to 0xA0504, and its "pop-in" row-copy loop
 *   memmove()s 24-byte rows into the hardcoded VRAM literal 0x9FD84.. (not
 *   redirectable via globals). The only computed state — the same-tile detection
 *   (same_pos), the destination teleport write (rt_char->pos_x/pos_y), and the
 *   framebuffer pop-in address math (row_off / src_base / fb_dst_row / src_row /
 *   row_count, including the top-edge row_count=0x12 branch) — was verified
 *   statically against the disassembly @0x22306 and @0x22390..0x22406; none of
 *   it is observable without driving the full warp animation + real warp sibling
 *   functions + VRAM, so it is deferred to Phase 9 integration.
 *
 *   fd2_animate_warp_portal_open_at @ 0x22470 — the source-tile portal-open
 *   half of the warp sequence (sole caller fd2_animate_warp_teleport_char). Its
 *   11-frame loop restores the backdrop, blits one portal sprite to the working
 *   surface, repaints chars, and unconditionally blits the viewport to 0xA0504
 *   every frame. The only computed state — the sprite-table index
 *   (portrait_sheet[6 + (frame+0x72)*4]) and the tile->working-surface address
 *   math ((tile_y-origin_y)*0x2AC0 + (tile_x-origin_x)*0x18 + 0x8250) — was
 *   verified statically against the disassembly @0x22489..0x224E9; it is
 *   observable only through the VRAM-touching blit callees, so it is deferred to
 *   Phase 9 integration.
 *
 *   fd2_animate_warp_out_collapse @ 0x22547 — the source-tile collapse half
 *   of the warp sequence (sole caller fd2_animate_warp_teleport_char). After an
 *   initial sprite blit to the working surface, its 6-frame countdown loop
 *   restores the backdrop, renders one shrinking filled-circle band, and
 *   unconditionally blits the viewport to 0xA0504 every frame. The only computed
 *   state — the working-surface blit position ((tile_y-origin_y)*0x2AC0 +
 *   (tile_x-origin_x)*0x18 + 0x8250), the 6-entry sprite-table lookup
 *   (table_base[6 + frame*4]+table_base), the shrinking band top
 *   ((src_y/5)*frame, signed IDIV), and the returned frame-0 sprite_addr — was
 *   verified statically against the disassembly @0x2255D..0x225EB and @0x225B5;
 *   it is observable only through the VRAM-touching blit callees (and the return
 *   only after the full loop drives 0xA0504), so it is deferred to Phase 9
 *   integration.
 *
 * The remaining worker, fd2_scatter_sprite_around_origin_with_random_offset
 * @ 0x21db2, is the scatter *leaf* called by the orphan executor. Unlike its
 * parent it touches NO VRAM: it only advances the shared RNG three times and
 * writes one (x, y, type) entry into three caller-supplied arrays. It is pure
 * deterministic computation (RNG + FPU cos/sin + truncation), so it IS unit
 * tested here directly.
 *
 * Ground-truth derivation (verified independently):
 *   - The RNG step seed = ROL16(seed + 0x9014, 3) and the exact seed sequence
 *     for the chosen seeds were confirmed via Ghidra emulate_function on
 *     fd2_advance_rng_state @ 0x4E893:
 *       0x1234 -> 0x1245 -> 0x12CD -> 0x170D
 *       0x5555 -> 0x2B4F -> 0xDB1D -> 0x598B
 *   - radius/angle/type are pure integer math from those seeds.
 *   - x/y use cos/sin of (angle degrees * 0.0174532) then TRUNCATE toward zero
 *     (the binary's __CHP sets RC=round-toward-zero before FISTP). The chosen
 *     seeds yield fractional parts far from any integer boundary (x ~ .32/.34,
 *     y ~ .73/.48 of a pixel), so 64-bit vs 80-bit FP truncation agree.
 *
 * Seed 0x1234, range 0x20, index 0, origin (160, 96):
 *   rng1=0x1245 -> radius = (0x1245%0x40=5)*0x20/0x40 - 1 = 160/64 - 1 = 1
 *   rng2=0x12CD -> angle  = 0x12CD%0x168 = 133 deg
 *     x = trunc(160 + cos(133 deg)*1)        = trunc(159.318) = 159
 *     y = trunc( 96 + sin(133 deg)*1 + (-8)) = trunc( 88.731) = 88
 *   rng3=0x170D -> type   = 0x170D%8 + 1 = 5 + 1 = 6
 *
 * Seed 0x5555, range 0x20, index 1, origin (160, 96):
 *   rng1=0x2B4F -> radius = (0x2B4F%0x40=15)*0x20/0x40 - 1 = 480/64 - 1 = 6
 *   rng2=0xDB1D -> angle  = 0xDB1D%0x168 = 293 deg
 *     x = trunc(160 + cos(293 deg)*6)        = trunc(162.344) = 162
 *     y = trunc( 96 + sin(293 deg)*6 + (-8)) = trunc( 82.477) = 82
 *   rng3=0x598B -> type   = 0x598B%8 + 1 = 3 + 1 = 4
 *   (index 1 also exercises the *2 / *1 slot offsetting into the arrays.)
 */

#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* ---- Test: scatter leaf (RNG + polar geometry + truncation) ---- */

static void test_scatter_seed_1234_index0(void)
{
    int16 x_arr[4];
    int16 y_arr[4];
    uint8 type_arr[4];

    memset(x_arr, 0, sizeof(x_arr));
    memset(y_arr, 0, sizeof(y_arr));
    memset(type_arr, 0, sizeof(type_arr));

    data_fd2_shared_rng_seed = 0x1234;
    fd2_scatter_sprite_around_origin_with_random_offset(
        0x20, 0, (uint32)x_arr, (uint32)y_arr, (uint32)type_arr, 160, 96);

    ASSERT_EQ(x_arr[0], 159);
    ASSERT_EQ(y_arr[0], 88);
    ASSERT_EQ(type_arr[0], 6);
    /* three RNG advances consumed: seed must land on the 3rd value */
    ASSERT_EQ(data_fd2_shared_rng_seed, 0x170d);
}

static void test_scatter_seed_5555_index1(void)
{
    int16 x_arr[4];
    int16 y_arr[4];
    uint8 type_arr[4];

    memset(x_arr, 0, sizeof(x_arr));
    memset(y_arr, 0, sizeof(y_arr));
    memset(type_arr, 0, sizeof(type_arr));

    data_fd2_shared_rng_seed = 0x5555;
    fd2_scatter_sprite_around_origin_with_random_offset(
        0x20, 1, (uint32)x_arr, (uint32)y_arr, (uint32)type_arr, 160, 96);

    /* slot 1 is written; slot 0 stays untouched (verifies index*2 / index offset) */
    ASSERT_EQ(x_arr[0], 0);
    ASSERT_EQ(y_arr[0], 0);
    ASSERT_EQ(type_arr[0], 0);
    ASSERT_EQ(x_arr[1], 162);
    ASSERT_EQ(y_arr[1], 82);
    ASSERT_EQ(type_arr[1], 4);
    ASSERT_EQ(data_fd2_shared_rng_seed, 0x598b);
}

/* type is always in [1,8] regardless of seed (rng3 % 8 + 1) */
static void test_scatter_type_range(void)
{
    int16 x_arr[1];
    int16 y_arr[1];
    uint8 type_arr[1];
    int   i;

    data_fd2_shared_rng_seed = 0;
    for (i = 0; i < 32; i++) {
        type_arr[0] = 0;
        fd2_scatter_sprite_around_origin_with_random_offset(
            0x20, 0, (uint32)x_arr, (uint32)y_arr, (uint32)type_arr, 160, 96);
        ASSERT_TRUE(type_arr[0] >= 1 && type_arr[0] <= 8);
    }
}

void run_spell_spellcin_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: spell/spellcin (scatter leaf tested; 8 VGA/VRAM cinematic "
           "workers deferred to Phase 9, see file header)\n");
    RUN_TEST(test_scatter_seed_1234_index0);
    RUN_TEST(test_scatter_seed_5555_index1);
    RUN_TEST(test_scatter_type_range);
}
