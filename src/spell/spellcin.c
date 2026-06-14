/*
 * spellcin.c — Spell cinematic worker functions (full-screen FX casts)
 *
 * Functions:
 *   fd2_cast_earthquake_spell_with_screen_shake @ 0x21548 (1 caller)
 *   fd2_play_rising_pre_cast_effect @ 0x2189a (6 callers)
 *   fd2_dispatch_variant_b_cast @ 0x21b18 (1 caller)
 *   fd2_scatter_sprite_around_origin_with_random_offset @ 0x21db2 (1 caller)
 *   fd2_execute_aoe_spell_with_caster_portrait_radial_scatter @ 0x21bd0 (0 callers)
 *   fd2_play_variant_b_slide_pre_effect @ 0x21eb1 (4 callers)
 *   fd2_animate_warp_teleport_char @ 0x22253 (4 callers)
 *   fd2_animate_warp_portal_open_at @ 0x22470 (1 caller)
 *   fd2_animate_warp_out_collapse @ 0x22547 (1 caller)
 *   fd2_animate_warp_in_expand @ 0x22656 (1 caller)
 *   fd2_cast_screen_wide_spell_with_fade @ 0x24618 (6 callers)
 *   fd2_execute_special_attack_skill @ 0x276ec (1 caller)
 *   fd2_execute_summon_spell_cast @ 0x27fc9 (1 caller)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * fd2_cast_earthquake_spell_with_screen_shake @ 0x21548  (1 caller)
 *
 * 地震 spell worker: full-screen shake + scaled tile-map redraw +
 * SFX burst + per-target magic damage. Reached from spell ids 0xA/0xB/0xC
 * via fd2_cast_spell_0a_basic @ 0x21527 (shared tail).
 *
 * Shake params live in data_fd2_animation_earthquake_screen_shake_params_table
 * (int[9] @ 0x52096), laid out column-major as 3 X-offsets [0..2],
 * 3 Y-offsets [3..5], 3 scales [6..8]. The binary copies these onto
 * the stack; here we index the global directly (Layer-2 equivalent).
 *
 * buf_table[4] mirrors the binary's overlapping stack-array used as the
 * frame-blit source: {orig large_game_state_buffer, buf_1, buf_3, buf_1}.
 * The original frees buf_1/buf_3 once and never frees the 0x4000 tile-table
 * buffer (an original leak, preserved verbatim).
 *
 * Cdecl, 4 stack params. The binary's __CHK(0x6c) stack-probe prologue is
 * compiler-injected and not part of the source. Explicit RET at 0x2185E.
 * ---------------------------------------------------------------- */
void fd2_cast_earthquake_spell_with_screen_shake(
    uint32 caster_unit_id, uint32 spell_id,
    uint32 num_targets, uint8 *target_id_array)
{
    int32 *shake_params;
    uint32 orig_battle_scene_snapshot;
    uint32 orig_large_game_state_buffer;
    void *buf0;
    uint32 *buf_1_alloc;
    uint32 *buf_3_alloc;
    void *tile_table;
    uint32 buf_table[4];
    uint16 tile_attr_buf[4];
    uint32 x;
    uint32 y;
    int i;
    int chr;
    int target_idx;
    uint8 target_id;
    int damage;

    shake_params = data_fd2_animation_earthquake_screen_shake_params_table;

    orig_battle_scene_snapshot = data_fd2_battle_scene_snapshot;
    data_fd2_battle_scene_snapshot = (uint32)fd2_convert_battle_tiles_to_24px();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster_unit_id, spell_id);

    buf0 = malloc(64000);
    if (buf0 == (void *)0) goto out_of_memory;
    buf_1_alloc = (uint32 *)malloc(0x25680);
    if (buf_1_alloc == (uint32 *)0) goto out_of_memory;
    buf_3_alloc = (uint32 *)malloc(0x25680);
    if (buf_3_alloc == (uint32 *)0) goto out_of_memory;

    orig_large_game_state_buffer = data_fd2_large_game_state_buffer_ptr;
    buf_table[0] = orig_large_game_state_buffer;
    buf_table[1] = (uint32)buf_1_alloc;
    buf_table[2] = (uint32)buf_3_alloc;
    buf_table[3] = (uint32)buf_1_alloc;
    tile_table = malloc(0x4000);

    for (y = 0; (int)y < (int)data_fd2_battle_map_height_tiles; y++) {
        for (x = 0; (int)x < (int)data_fd2_battle_map_width_tiles; x++) {
            fd2_read_tile_attribute_at_pos(x, y, (uint32)tile_attr_buf);
            *(uint32 *)((char *)tile_table + (y * 0x40 + x) * 4) =
                data_fd2_battle_scene_snapshot + (uint32)tile_attr_buf[0] * 0x240 + 6;
        }
    }

    /* slow shake: 3 large-displacement frames */
    for (i = 0; i < 3; i++) {
        data_fd2_large_game_state_buffer_ptr = (uint32)buf0;
        fd2_blit_scaled_tile_map_view(
            data_fd2_battle_view_window_max_x * 0x600 +
                data_fd2_battle_view_window_origin_x * 0xc00 +
                (uint32)shake_params[i],
            data_fd2_battle_view_window_max_y * 0x600 +
                data_fd2_battle_view_window_origin_y * 0xc00 +
                (uint32)shake_params[i + 3],
            (uint32)shake_params[i + 6], (uint32)tile_table);
        fd2_blit_rectangle(buf_table[i] + 0x8088, 0x1c8,
            data_fd2_large_game_state_buffer_ptr + 0x504, 0x140, 0x138, 0xc0);
        data_fd2_large_game_state_buffer_ptr = buf_table[i];
        for (chr = 0; chr < (int)data_fd2_battle_party_member_count; chr++) {
            if ((data_fd2_battle_runtime_char_array_ptr[chr].flags & 1) == 0) {
                fd2_paint_char_sprite_at_world_pos((uint32)chr);
            }
        }
    }

    /* fast tremor: 60 frames, SFX every 6th up to frame 0x2B */
    for (i = 0; i < 0x3c; i++) {
        if (i < 0x2b && i % 6 == 0) {
            fd2_play_sfx_with_handle(
                data_fd2_audio_status_effect_sfx_handle_ptr, 0xd, 1);
        }
        fd2_blit_rectangle(0xa0504, 0x140, buf_table[i % 4] + 0x8088,
            0x1c8, 0x138, 0xc0);
        __delay_thunk_375b2(10);
    }

    free(buf0);
    free(buf_1_alloc);
    free(buf_3_alloc);
    data_fd2_large_game_state_buffer_ptr = orig_large_game_state_buffer;
    free((void *)data_fd2_battle_scene_snapshot);
    data_fd2_battle_scene_snapshot = orig_battle_scene_snapshot;
    fd2_composite_battle_frame(0);

    for (target_idx = 0; target_idx < (int)num_targets; target_idx++) {
        target_id = target_id_array[target_idx];
        damage = fd2_calc_magic_damage((uint32)target_id, spell_id);
        if (damage == 0) {
            fd2_show_miss_indicator((uint32)target_id);
        } else {
            fd2_show_damage_number((uint32)damage, 0x5e, (uint32)target_id);
        }
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
    return;

out_of_memory:
    printf("Out of memory at Earth Quack !!\n");
    exit(0);
}

/* ----------------------------------------------------------------
 * fd2_play_rising_pre_cast_effect @ 0x2189a  (6 callers)
 *
 * 10-frame "rising sparkle" effect originating at a unit's screen
 * position. Used both as a spell pre-effect (spell 0xB / 0xC) and as a
 * generic summon / chapter-event rising sparkle.
 *
 * Params (cdecl, 3 stack args):
 *   caster_unit_id  — index into runtime_char_array (* 0x50 stride)
 *   initial_height  — circle-band radius on frame 0 (ESI accumulator)
 *   rise_step       — radius increment applied after each frame
 *
 * Per frame (0..9):
 *   restore the saved backdrop into the large game-state buffer, draw the
 *   filled-circle band at the caster's screen pos with the current radius,
 *   repaint all unit sprites over it, then blit the composed scene to the
 *   mode-13h framebuffer (0xA0504). The 10-entry sprite table is read from
 *   the tile-anim table base; each entry is a self-relative offset.
 *
 * caster_screen_x/y are precomputed to (tile - origin)*0x18 + offset, matching
 * the binary which stores both fully-scaled values on the stack before the
 * loop (the Ghidra decompile mis-splits the x scaling into the call site; the
 * assembly computes both up front).
 *
 * The binary's __CHK(0x38) stack-probe prologue is compiler-injected. The
 * normal-path tail JMPs into the shared inline-epilogue fragment
 * fd2_noop_stub_b43 @ 0x10b43 (ADD ESP / POP regs / RET); emitted here as a
 * plain return and regenerated by the compiler (see emit_issues.json 00010b43).
 * ---------------------------------------------------------------- */
void fd2_play_rising_pre_cast_effect(int caster_unit_id, int initial_height,
                                     int rise_step)
{
    uint32 caster_screen_x;
    uint32 caster_screen_y;
    uint32 snapshot;
    uint32 sprite_addr;
    int frame_iter;

    caster_screen_x =
        ((uint32)data_fd2_battle_runtime_char_array_ptr[caster_unit_id].pos_x -
         data_fd2_battle_view_window_origin_x) * 0x18 + 0xc;
    caster_screen_y =
        ((uint32)data_fd2_battle_runtime_char_array_ptr[caster_unit_id].pos_y -
         data_fd2_battle_view_window_origin_y) * 0x18 + 0x12;

    snapshot = (uint32)malloc(0x25680);
    fd2_composite_battle_tile_map(snapshot + 0x8088, 0x1c8, 0xd, 8,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);

    for (frame_iter = 0; frame_iter < 10; frame_iter++) {
        sprite_addr =
            *(uint32 *)(data_fd2_tile_anim_table_base + 6 + frame_iter * 4) +
            data_fd2_tile_anim_table_base;
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
            (void *)snapshot, 0x25680);
        fd2_render_circle_anim_row(caster_screen_x, caster_screen_y,
            initial_height, 0xc, 0, 0xc0, (uint8 *)sprite_addr);
        fd2_composite_all_chars_overlay();
        fd2_blit_rectangle(0xa0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);
        initial_height += rise_step;
    }

    free((void *)snapshot);
    fd2_composite_battle_frame(0);
    return;
}

/* ----------------------------------------------------------------
 * fd2_dispatch_variant_b_cast @ 0x21b18  (1 caller)
 *
 * Variant-B spell executor (heal-style worker). Reached from spell ids
 * 0xD / 0xE / 0xF / 0x10 via fd2_cast_spell_0d_variant_b @ 0x21AD9's
 * shared tail. Distinct from the offensive A-variant @ 0x21227 in that it
 * applies fd2_apply_heal_spell_to_target (returns a heal amount) rather
 * than fd2_calc_magic_damage.
 *
 * Sequence:
 *   reset the AoE fx-queue counter, run the per-target impact animation,
 *   run the 2nd-pass status-effect overlay flicker, deduct the caster's MP,
 *   then for each target apply the heal and show the healed amount with the
 *   'i' (0x69, heal/info) indicator. Finishes via the shared spell-finale
 *   helper (composite frame + projectile-path animation).
 *
 * Params (cdecl, 4 stack args, the proto types the 4th as int):
 *   caster      — caster unit id            (EDI binds spell_id, EBP n_targets)
 *   spell_id    — spell id (0xD..0x10)
 *   n_targets   — target count
 *   p_targets   — pointer to the uint8 target-id array (reloaded into EBX in
 *                 the per-target loop; Ghidra's auto-name "caster_idx" is
 *                 misleading — it is the target-array base).
 *
 * The binary's __CHK(0x24) stack-probe prologue is compiler-injected and not
 * part of the source. There is no explicit RET: the normal path tail-JMPs to
 * fd2_composite_then_animate_projectiles @ 0x21190 (which shares both the
 * finale code and the parent's register restore); emitted here as a plain
 * call to that helper followed by the compiler-generated return.
 * ---------------------------------------------------------------- */
void fd2_dispatch_variant_b_cast(int caster, int spell_id, int n_targets,
                                 int p_targets)
{
    int target_idx;
    uint8 target_id;
    int heal_amount;

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_animate_spell_impact_per_target((uint32)caster, (uint32)spell_id,
        (uint32)n_targets, (uint32)p_targets);
    fd2_animate_status_effect_overlay_flicker((uint32)caster, (uint32)spell_id,
        (uint32)n_targets, (uint32)p_targets);
    fd2_deduct_caster_mp((uint32)caster, (uint32)spell_id);

    for (target_idx = 0; target_idx < n_targets; target_idx++) {
        target_id = ((uint8 *)p_targets)[target_idx];
        heal_amount = fd2_apply_heal_spell_to_target((uint32)target_id,
            (uint32)spell_id);
        fd2_show_damage_number((uint32)heal_amount, 0x69, (uint32)target_id);
    }

    fd2_composite_then_animate_projectiles();
    return;
}

/* ----------------------------------------------------------------
 * fd2_scatter_sprite_around_origin_with_random_offset @ 0x21db2  (1 caller)
 *
 * Write one random polar-offset entry into the parallel (x, y, type) sprite
 * arrays for an AoE radial-scatter animation. Pure in-memory computation: it
 * touches no VRAM, only the three caller-supplied arrays and the shared RNG.
 *
 * Per call (three RNG advances, in this exact order):
 *   rng1   = fd2_advance_rng_state();
 *   radius = (rng1 % 0x40) * scatter_range_max / 0x40 - 1;  // 0..(range-1)-ish
 *   rng2   = fd2_advance_rng_state();
 *   angle  = rng2 % 360;                                    // whole degrees
 *   x = origin_x + cos(angle deg) * radius;                 // polar -> Cartesian
 *   y = origin_y + sin(angle deg) * radius + (-8.0);        // -8 = isometric skew
 *   rng3   = fd2_advance_rng_state();
 *   type[index] = (rng3 % 8) + 1;                           // 1..8 shrink rate
 *
 * The angle is converted to radians by the binary's stored literal
 * data_fd2_graphics_radian_per_degree_const = 0.0174532 (a 7-digit pi/180
 * approximation, NOT full-precision pi/180), then fed to the Watcom math-lib
 * cos/sin. Both x and y are converted back to int by the FPU helper __CHP
 * (@0x377A4), which sets RC=round-toward-zero and FRNDINT before FISTP — i.e.
 * the binary TRUNCATES toward zero, so plain C (int) casts are exact here.
 *
 * Only the low byte of sprite_array_index selects the slot; the x/y arrays are
 * short[] (index*2) and the type array is byte[] (index).
 *
 * KNOWN DECOMPILER BUG: Ghidra's EAX-tracking loss made the decompile attribute
 * the first RNG result to the __CHK stack-probe return (iVar2). The assembly
 * (MOV EDX,EAX right after each CALL 0x4E893) proves all three values come from
 * the RNG; emitted accordingly.
 *
 * Cdecl, 7 stack params; void return (explicit RET @0x21DB1, caller cleans the
 * 0x1c=28 arg bytes). The binary's __CHK(0x38) stack-probe prologue is
 * compiler-injected and omitted here. Sole caller: the orphan AoE executor
 * fd2_execute_aoe_spell_with_caster_portrait_radial_scatter @0x21BD0 (initial
 * scatter when a slot opens + re-scatter when a sprite rises off-screen).
 * ---------------------------------------------------------------- */
void fd2_scatter_sprite_around_origin_with_random_offset(
    int scatter_range_max, int sprite_array_index,
    uint32 sprite_x_array_addr, uint32 sprite_y_array_addr,
    uint32 sprite_type_array_addr, int origin_x, int origin_y)
{
    uint32 rng1;
    uint32 rng2;
    uint32 rng3;
    int    radius;
    int    angle_deg;
    double angle_rad;
    uint32 index;

    rng1 = fd2_advance_rng_state();
    radius = ((int)(rng1 % 0x40) * scatter_range_max) / 0x40 - 1;

    rng2 = fd2_advance_rng_state();
    angle_deg = (int)(rng2 % 0x168);
    angle_rad = (double)angle_deg * data_fd2_graphics_radian_per_degree_const;

    index = (uint32)sprite_array_index & 0xff;

    *(int16 *)(sprite_x_array_addr + index * 2) =
        (int16)(int)((double)origin_x + cos(angle_rad) * (double)radius);

    *(int16 *)(sprite_y_array_addr + index * 2) =
        (int16)(int)((double)origin_y + sin(angle_rad) * (double)radius +
                     data_fd2_graphics_scatter_y_offset_neg8);

    rng3 = fd2_advance_rng_state();
    *(uint8 *)(sprite_type_array_addr + index) = (uint8)((rng3 % 8) + 1);
}

/* ----------------------------------------------------------------
 * fd2_execute_aoe_spell_with_caster_portrait_radial_scatter @ 0x21bd0
 *   (0 callers — ORPHAN / UNREACHABLE)
 *
 * AoE radial sprite-scatter cinematic with a caster portrait. Implemented in
 * the binary but never invoked by any caller, not present in the spell dispatch
 * table @ 0x51D01, and its address bytes never appear as a function pointer.
 * Emitted verbatim for completeness; it is a vestige of a cut/planned spell.
 *
 * The body shares its frame-cleanup epilogue (0x21DAD: ADD ESP,0x10C + POP
 * regs + RET) with fd2_render_filled_circle_band_anim @ 0x22046, which has a
 * conditional JMP at 0x220FF into 0x21DAD (Watcom shared-epilogue between two
 * adjacent functions of identical frame shape). That is a binary layout detail;
 * each function is emitted as its own self-contained C routine.
 *
 * Params (cdecl, 7 stack args):
 *   origin_x, origin_y       — AoE center pixel coordinates
 *   portrait_idx             — caster portrait_sheet entry index
 *   scatter_range_max        — sprite radial scatter max radius
 *   animation_frame_count    — total animation frames
 *   max_active_sprites       — simultaneous active-sprite cap (<= 50)
 *   ptr_game_state_snapshot  — backdrop source, memmove'd in each frame
 *
 * sprite_mask is the caster portrait's pixel data, located via the portrait
 * sheet's self-relative offset table: *(int*)(sheet + 6 + portrait_idx*4) is
 * a sheet-relative offset, added back to the sheet base.
 *
 * Per animation frame:
 *   - if active_sprite_count < max_active_sprites: scatter one new sprite
 *     around the origin (random polar offset) and bump the count.
 *   - restore the backdrop into the large game-state buffer (memmove 0x25680).
 *   - draw every active sprite that lands inside [1,0x135]x[1,0xbd] into the
 *     buffer at +0x8088 via the palette-remap sprite blitter (remap table from
 *     the tile-anim table base + *(base+0x12)).
 *   - blit the composed buffer (+0x8088) to the mode-13h framebuffer 0xA0504.
 *   - move every sprite up by its per-sprite shrink rate (sprite_type); any
 *     sprite that rises past y < -10 is re-scattered.
 *   - delay 10 ticks.
 *
 * The binary's __CHK(0x13C) stack-probe prologue is compiler-injected and not
 * part of the source. Explicit RET at 0x21DB1 (cdecl, caller cleans the 7
 * args). The 50-entry sprite arrays live on the stack (sprite_type_array is 52
 * bytes in the binary's frame layout).
 * ---------------------------------------------------------------- */
void fd2_execute_aoe_spell_with_caster_portrait_radial_scatter(
    int origin_x, int origin_y, int portrait_idx, int scatter_range_max,
    int animation_frame_count, int max_active_sprites,
    uint32 *ptr_game_state_snapshot)
{
    short sprite_y_array[50];
    short sprite_x_array[50];
    uint8 sprite_type_array[52];
    uint16 *sprite_mask;
    uint8 active_sprite_count;
    uint8 inner_sprite_idx;
    uint8 outer_frame_idx;
    uint32 idx;
    int sprite_x;
    int sprite_y;
    uint8 shrink_rate;
    short new_y;

    active_sprite_count = 0;
    sprite_mask = (uint16 *)(*(int *)(data_fd2_resource_portrait_sheet_ptr +
                                      6 + portrait_idx * 4) +
                             data_fd2_resource_portrait_sheet_ptr);

    for (outer_frame_idx = 0;
         (int)(uint32)outer_frame_idx < animation_frame_count;
         outer_frame_idx++) {
        if ((int)(uint32)active_sprite_count < max_active_sprites) {
            fd2_scatter_sprite_around_origin_with_random_offset(
                scatter_range_max, (int)(uint32)active_sprite_count,
                (uint32)sprite_x_array, (uint32)sprite_y_array,
                (uint32)sprite_type_array, origin_x, origin_y);
            active_sprite_count++;
        }

        memmove((void *)data_fd2_large_game_state_buffer_ptr,
                (void *)ptr_game_state_snapshot, 0x25680);

        for (inner_sprite_idx = 0;
             (idx = (uint32)inner_sprite_idx,
              idx < (uint32)active_sprite_count);
             inner_sprite_idx++) {
            sprite_x = (int)sprite_x_array[idx];
            if (0 < sprite_x && sprite_x < 0x136 &&
                (sprite_y = (int)sprite_y_array[idx], 0 < sprite_y) &&
                sprite_y < 0xbe) {
                fd2_blit_palette_remap_with_sprite_mask(
                    (uint8 *)(data_fd2_large_game_state_buffer_ptr +
                              sprite_x + sprite_y * 0x1c8 + 0x8088),
                    sprite_mask, 0x1c8,
                    data_fd2_tile_anim_table_base +
                        *(int *)(data_fd2_tile_anim_table_base + 0x12));
            }
        }

        fd2_blit_rectangle(0xa0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);

        for (inner_sprite_idx = 0;
             (idx = (uint32)inner_sprite_idx,
              idx < (uint32)active_sprite_count);
             inner_sprite_idx++) {
            shrink_rate = sprite_type_array[idx];
            new_y = (short)(sprite_y_array[idx] - (uint16)shrink_rate);
            sprite_y_array[idx] = new_y;
            if (new_y < -10) {
                fd2_scatter_sprite_around_origin_with_random_offset(
                    scatter_range_max, (int)idx,
                    (uint32)sprite_x_array, (uint32)sprite_y_array,
                    (uint32)sprite_type_array, origin_x, origin_y);
            }
        }

        __delay_thunk_375b2(10);
    }

    return;
}

/* ----------------------------------------------------------------
 * fd2_play_variant_b_slide_pre_effect @ 0x21eb1  (4 callers)
 *
 * 16-frame radial circle-band "slide" pre-cast animation centered on the
 * cursor tile. Shared by the 4 variant-B spell thunks (spell 0xD/0xE/0xF/0x10)
 * which each pass a per-spell (offset0, step) tuning before
 * fd2_dispatch_variant_b_cast applies the healing:
 *   0xD (1,2)   0xE (2,4)   0xF (8,4)   0x10 (6,6)
 *
 * Setup: malloc a 0x25680 working buffer and snapshot the battle tile map into
 * it at +0x8088 (stride 0x1c8, tile 0xD x 8, at the battle view-window origin).
 *
 * cursor_screen_x/y are the cursor tile coords scaled to pixels
 * (tile*0x18 + offset); both fully computed before the loops, matching the
 * assembly (ESI holds the radius accumulator, EBP the cursor_screen_y).
 *
 * Phase 1 — slide-DOWN (frame_iter 9..1, 9 frames): restore the tile backdrop,
 * draw the filled circle-band at the cursor pixel pos with the current radius
 * (offset0, growing by step each frame), repaint chars (inside the band-anim
 * helper) and blit the composed buffer to the mode-13h framebuffer 0xA0504,
 * then 5-tick delay. 200ms pause follows.
 *
 * Phase 2 — slide-UP (frame_iter 3..9, 7 frames): same per-frame work but the
 * radius is held fixed at (offset0 - step) for all 7 frames. The assembly
 * subtracts step from the accumulator once (SUB ESI @0x21fa0) and reuses ESI
 * unchanged through the loop, so passing (offset0 - step) each iteration is
 * exact (offset0 was already advanced 9*step by phase 1; offset0-step =
 * initial + 8*step).
 *
 * The 16 sprite frames are read from the tile-anim table: each entry
 * *(int*)(base + 6 + frame_iter*4) is a self-relative offset added back to the
 * base, then handed to fd2_render_filled_circle_band_anim as its 6th arg (the
 * palette-remap / sprite source the band helper passes straight through).
 *
 * Cleanup: free the snapshot, recomposite the battle frame, 200ms pause.
 *
 * Cdecl, 2 stack params (offset0, step); void return. The binary's __CHK(0x34)
 * stack-probe prologue is compiler-injected. There is no explicit RET: the
 * normal-path tail-JMPs into the shared inline-epilogue fragment
 * fd2_noop_stub_b43 @ 0x10b43 (ADD ESP / POP regs / RET); emitted here as a
 * plain return and regenerated by the compiler (see emit_issues.json
 * 00010b43, same +0x00 entry used by fd2_play_rising_pre_cast_effect above).
 * ---------------------------------------------------------------- */
void fd2_play_variant_b_slide_pre_effect(int offset0, int step)
{
    uint32 cursor_screen_x;
    uint32 cursor_screen_y;
    uint32 snapshot;
    uint32 sprite_addr;
    int frame_iter;

    cursor_screen_x = data_fd2_battle_cursor_screen_x * 0x18 + 0xc;
    cursor_screen_y = data_fd2_battle_cursor_screen_y * 0x18 + 0x10;

    snapshot = (uint32)malloc(0x25680);
    fd2_composite_battle_tile_map(snapshot + 0x8088, 0x1c8, 0xd, 8,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);

    /* slide-DOWN: radius grows from offset0 by step each frame */
    for (frame_iter = 9; frame_iter > 0; frame_iter--) {
        sprite_addr =
            *(uint32 *)(data_fd2_tile_anim_table_base + 6 + frame_iter * 4) +
            data_fd2_tile_anim_table_base;
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
            (void *)snapshot, 0x25680);
        fd2_render_filled_circle_band_anim(cursor_screen_x, cursor_screen_y,
            (uint32)offset0, 0, 0xc0, (int)sprite_addr);
        fd2_blit_rectangle(0xa0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);
        offset0 += step;
        __delay_thunk_375b2(5);
    }

    __delay_thunk_375b2(200);

    /* slide-UP: radius held at (offset0 - step) for all 7 frames */
    for (frame_iter = 3; frame_iter < 10; frame_iter++) {
        sprite_addr =
            *(uint32 *)(data_fd2_tile_anim_table_base + 6 + frame_iter * 4) +
            data_fd2_tile_anim_table_base;
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
            (void *)snapshot, 0x25680);
        fd2_render_filled_circle_band_anim(cursor_screen_x, cursor_screen_y,
            (uint32)(offset0 - step), 0, 0xc0, (int)sprite_addr);
        fd2_blit_rectangle(0xa0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);
        __delay_thunk_375b2(5);
    }

    free((void *)snapshot);
    fd2_composite_battle_frame(0);
    __delay_thunk_375b2(200);
    return;
}

/* ----------------------------------------------------------------
 * fd2_animate_warp_teleport_char @ 0x22253  (4 callers)
 *
 * CHARACTER WARP / TELEPORT animation. Moves a unit from its current cursor
 * position to a destination tile, with the full three-stage warp visual:
 * source-side portal-open + collapse, a row-by-row "pop in" at the
 * destination, and a destination-side expand. Used by spell 0x17 (dual warp)
 * and by story-chapter cinematics.
 *
 * Params (cdecl, 5 stack args; void return):
 *   char_slot    — runtime_char_array index of the unit being warped (its
 *                  pos_x/pos_y are overwritten with the new tile after the
 *                  source-side collapse).
 *   new_pos_x    — destination tile X written into runtime_char[char_slot].pos_x
 *   new_pos_y    — destination tile Y written into runtime_char[char_slot].pos_y
 *   dst_tile_x   — destination tile X for the framebuffer math (subtracted from
 *                  the battle view-window origin X). (Ghidra auto-name
 *                  "char_idx" is misleading.)
 *   dst_tile_y   — destination tile Y for the framebuffer math (subtracted from
 *                  the battle view-window origin Y). (Ghidra auto-name "dst_x"
 *                  is misleading.)
 *
 * Sequence:
 *   warp_sfx_buf = fd2_load_dat_resource("FDOTHER.DAT", 0x51);   // warp SFX bank
 *   snapshot     = malloc(0x25680);                              // 150KB backdrop backup
 *   fd2_composite_battle_tile_map(snapshot + 0x8088, ...);       // snapshot the scene
 *   fd2_animate_warp_portal_open_at(dst_tile_x, dst_tile_y, snapshot);
 *   warp_in_sprite = portrait_sheet + *(int*)(portrait_sheet + 0x1F6);
 *   src_x = cursor_screen_x*0x18 + 0xC;  src_y = cursor_screen_y*0x18 + 0xF;
 *   same_pos = (new_pos_x == dst_tile_x && new_pos_y == dst_tile_y);
 *   fd2_play_sfx_with_handle(warp_sfx_buf, same_pos, 1);
 *   radius = fd2_animate_warp_out_collapse(dst_tile_x, dst_tile_y, snapshot,
 *                                          src_x, src_y, warp_in_sprite);
 *   rt_char->pos_x = new_pos_x;  rt_char->pos_y = new_pos_y;     // ACTUAL TELEPORT
 *   memmove(large_game_state_buffer, snapshot, 0x25680);         // restore backdrop
 *   fd2_render_filled_circle_band_anim(src_x, src_y, 0xB, 0, 0xC0, radius);
 *
 *   // "Pop in" copy: 24-byte rows from the working surface to the mode-13h
 *   // framebuffer at the destination tile. row_count is 0x18, or 0x12 (and the
 *   // src/dst start rows shift) when the destination sits on the view-window
 *   // top edge (dst_tile_y == origin_y).
 *   for i in 0..row_count-1:
 *     memmove(fb_dst_row, src_row, 0x18);
 *     fb_dst_row += 0x140;  src_row += 0x1C8;  __delay_thunk_375b2(10);
 *
 *   fd2_animate_warp_in_expand(dst_tile_x, dst_tile_y, snapshot, src_x, src_y,
 *                              warp_in_sprite, radius);
 *   free(snapshot);  free(warp_sfx_buf);
 *
 * Framebuffer math (verified against the assembly @0x22390..0x22406):
 *   row_off  = (dst_tile_x - origin_x) * 0x18;
 *   src_base = large_game_state_buffer + (dst_tile_y - origin_y)*0x2AC0 + row_off + 0x8088;
 *   fb_dst   = 0x9FD84 + row_off + (dst_tile_y - origin_y)*0x1E00;
 *   default (not top edge): row_count=0x18, src_row = src_base - 0xAB0;
 *   top edge:               row_count=0x12, src_row = src_base, fb_dst += 0x780.
 *
 * Resources: FDOTHER.DAT[0x51] (warp SFX bank).
 *
 * Cdecl, 5 stack params; void return. The binary's __CHK(0x4C) stack-probe
 * prologue is compiler-injected and not part of the source. There is no
 * explicit RET: the normal path tail-JMPs into the shared epilogue at 0x18888
 * (the ADD ESP,0x1C / POP EBP/EDI/ESI/EBX / RET tail of
 * fd2_render_decimal_number_to_buffer @ 0x187D6 — an identical Watcom epilogue
 * shared between two adjacent same-frame functions). Emitted here as a plain
 * return; the compiler regenerates the matching epilogue.
 * ---------------------------------------------------------------- */
void fd2_animate_warp_teleport_char(uint32 char_slot, uint32 new_pos_x,
                                    uint32 new_pos_y, uint32 dst_tile_x,
                                    uint32 dst_tile_y)
{
    uint32 warp_sfx_buf;
    uint32 snapshot;
    uint32 src_x;
    uint32 src_y;
    uint32 same_pos;
    uint32 warp_in_sprite;
    int radius;
    runtime_char *rt_char;
    uint32 row_off;
    uint32 src_base;
    uint32 fb_dst_row;
    uint32 src_row;
    int row_count;
    int i;

    warp_sfx_buf = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x51);
    snapshot = (uint32)malloc(0x25680);
    fd2_composite_battle_tile_map(snapshot + 0x8088, 0x1c8, 0xd, 8,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);

    rt_char = &data_fd2_battle_runtime_char_array_ptr[char_slot];
    fd2_animate_warp_portal_open_at(dst_tile_x, dst_tile_y, snapshot);

    warp_in_sprite =
        *(uint32 *)(data_fd2_resource_portrait_sheet_ptr + 0x1f6) +
        data_fd2_resource_portrait_sheet_ptr;
    src_x = data_fd2_battle_cursor_screen_x * 0x18 + 0xc;
    src_y = data_fd2_battle_cursor_screen_y * 0x18 + 0xf;

    same_pos = 0;
    if (new_pos_x == dst_tile_x && new_pos_y == dst_tile_y) {
        same_pos = 1;
    }
    fd2_play_sfx_with_handle(warp_sfx_buf, (int)same_pos, 1);

    radius = fd2_animate_warp_out_collapse((int)dst_tile_x, (int)dst_tile_y,
        (void *)snapshot, src_x, src_y, (int)warp_in_sprite);

    rt_char->pos_x = (uint8)new_pos_x;
    rt_char->pos_y = (uint8)new_pos_y;

    memmove((void *)data_fd2_large_game_state_buffer_ptr,
        (void *)snapshot, 0x25680);
    fd2_render_filled_circle_band_anim(src_x, src_y, 0xb, 0, 0xc0, radius);

    row_off = (dst_tile_x - data_fd2_battle_view_window_origin_x) * 0x18;
    src_base = data_fd2_large_game_state_buffer_ptr +
        (dst_tile_y - data_fd2_battle_view_window_origin_y) * 0x2ac0 +
        row_off + 0x8088;
    fb_dst_row = row_off + 0x9fd84 +
        (dst_tile_y - data_fd2_battle_view_window_origin_y) * 0x1e00;
    row_count = 0x18;
    src_row = src_base - 0xab0;
    if (dst_tile_y == data_fd2_battle_view_window_origin_y) {
        row_count = 0x12;
        fb_dst_row = fb_dst_row + 0x780;
        src_row = src_base;
    }

    for (i = 0; i < row_count; i++) {
        memmove((void *)fb_dst_row, (void *)src_row, 0x18);
        fb_dst_row = fb_dst_row + 0x140;
        src_row = src_row + 0x1c8;
        __delay_thunk_375b2(10);
    }

    fd2_animate_warp_in_expand(dst_tile_x, dst_tile_y, snapshot,
        src_x, src_y, (uint32 *)warp_in_sprite, radius);
    free((void *)snapshot);
    free((void *)warp_sfx_buf);
    return;
}

/* ----------------------------------------------------------------
 * fd2_animate_warp_portal_open_at @ 0x22470  (1 caller)
 *
 * 11-frame warp-portal OPEN animation drawn at battle tile
 * (tile_x, tile_y). First half of the character-warp sequence: the
 * portal materialises at the source tile before the character
 * collapses into it.
 *
 * Frames come from the 11-entry sprite table at offset +0x1B8 of the
 * portrait sheet (entry index = frame + 0x72, picked up as
 * portrait_sheet[6 + (frame+0x72)*4] + portrait_sheet).
 *
 * Each frame: restore the backdrop from the caller's snapshot
 * (backup_buffer), blit one portal sprite at the tile's working-surface
 * address, repaint characters on top, push the viewport to mode13h, then
 * wait one BIOS tick.
 *
 * Tile -> working-surface address:
 *   large_game_state_buffer
 *     + (tile_y - battle_window_origin_y) * 0x2AC0   (row pitch)
 *     + (tile_x - battle_window_origin_x) * 0x18     (column width)
 *     + 0x8250                                        (0x8088 + 0x1C8 origin)
 *
 * Sole caller: fd2_animate_warp_teleport_char @ 0x22253, which passes
 * (dst_tile_x, dst_tile_y, snapshot).
 *
 * Cdecl, 3 stack params. The binary's __CHK(0x2c) stack-probe prologue is
 * compiler-injected and not part of the source. Explicit RET at 0x22546.
 * ---------------------------------------------------------------- */
void fd2_animate_warp_portal_open_at(uint32 tile_x, uint32 tile_y,
    uint32 backup_buffer)
{
    uint32 frame_iter;
    uint32 sprite_addr;
    uint32 target_addr;

    for (frame_iter = 0; (int)frame_iter < 0xb; frame_iter++) {
        sprite_addr =
            *(uint32 *)(data_fd2_resource_portrait_sheet_ptr + 6 +
                        (frame_iter + 0x72) * 4) +
            data_fd2_resource_portrait_sheet_ptr;
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
            (void *)backup_buffer, 0x25680);
        target_addr = data_fd2_large_game_state_buffer_ptr +
            (tile_y - data_fd2_battle_view_window_origin_y) * 0x2ac0 +
            (tile_x - data_fd2_battle_view_window_origin_x) * 0x18 + 0x8250;
        fd2_blit_sprite_with_decoded_pixels(target_addr, sprite_addr, 0x1c8);
        fd2_composite_all_chars_overlay();
        fd2_blit_rectangle(0xa0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);
        fd2_wait_n_bios_ticks(1);
    }
    return;
}

/* ----------------------------------------------------------------
 * fd2_animate_warp_out_collapse @ 0x22547  (1 caller)
 *
 * 6-frame warp-OUT collapse animation: the character vanishes into a
 * shrinking filled circle. Second half of the character-warp sequence
 * (sole caller fd2_animate_warp_teleport_char @ 0x22253, source-side
 * collapse), played right after the portal opens at the source tile.
 *
 * First the snapshot-backed character sprite is blitted onto the working
 * surface at the tile's position. Then for frame_iter = 5..0 (counting
 * down) the backdrop is restored from the snapshot, one band sprite from
 * the 6-entry tile-anim table is rendered as a filled circle whose
 * vertical extent shrinks with frame_iter, and the viewport is blitted to
 * the mode-13h framebuffer. A final 2-tick hold ends the animation.
 *
 * Sprite table (6 entries) at offset +6 of data_fd2_tile_anim_table_base:
 *   sprite_addr = *(int *)(table_base + 6 + frame_iter*4) + table_base;
 * Working-surface blit position (verified @0x2255d..0x22594, same idiom as
 * fd2_animate_warp_portal_open_at):
 *   pos = snapshot + (tile_y-origin_y)*0x2AC0 + (tile_x-origin_x)*0x18 + 0x8250;
 * Shrinking radius band top:  top_y = (src_y / 5) * frame_iter   (signed
 * IDIV, verified @0x225dd..0x225eb).
 *
 * Returns the frame_iter==0 sprite_addr (the last value computed by the
 * loop, held in EDI through the shared epilogue). The caller uses it as
 * the warp-IN initial sprite state (its "radius").
 *
 * Cdecl, 6 stack params; int return. The binary's __CHK(0x2C) stack-probe
 * prologue is compiler-injected and not part of the source. There is no
 * explicit RET: the tail JMPs into the shared epilogue at 0x1E5B9 (the
 * MOV EAX,EDI / POP EBP/EDI/ESI/EBX / RET tail of
 * fd2_roll_stat_gain_and_show_message @ 0x1E529 — an identical Watcom
 * epilogue shared between adjacent same-frame functions). Emitted here as a
 * plain return of sprite_addr;
 * the compiler regenerates the matching epilogue.
 * ---------------------------------------------------------------- */
int fd2_animate_warp_out_collapse(int tile_x, int tile_y, void *snapshot,
    uint32 src_x, uint32 src_y, int initial_sprite_addr)
{
    uint32 blit_pos;
    int frame_iter;
    uint32 sprite_addr;
    uint32 top_y_param;

    blit_pos = (uint32)snapshot +
        (tile_y - data_fd2_battle_view_window_origin_y) * 0x2ac0 +
        (tile_x - data_fd2_battle_view_window_origin_x) * 0x18 + 0x8250;
    fd2_blit_sprite_with_decoded_pixels(blit_pos, initial_sprite_addr, 0x1c8);

    sprite_addr = 0;
    for (frame_iter = 5; frame_iter >= 0; frame_iter--) {
        sprite_addr =
            *(uint32 *)(data_fd2_tile_anim_table_base + 6 + frame_iter * 4) +
            data_fd2_tile_anim_table_base;
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
            snapshot, 0x25680);
        top_y_param = ((int)src_y / 5) * frame_iter;
        fd2_render_filled_circle_band_anim(src_x, src_y, 0xb, top_y_param,
            0xc0, sprite_addr);
        fd2_blit_rectangle(0xa0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);
        __delay_thunk_375b2(10);
    }

    fd2_wait_n_bios_ticks(1);
    fd2_wait_n_bios_ticks(1);
    return (int)sprite_addr;
}

/* ----------------------------------------------------------------
 * fd2_animate_warp_in_expand @ 0x22656  (1 caller)
 *
 * 10-frame warp-IN expand animation: the character materializes at the
 * destination tile inside an expanding filled-circle band. Third visual
 * stage of the character-warp sequence (sole caller
 * fd2_animate_warp_teleport_char @ 0x22253, destination-side expand,
 * played right after the "pop-in" row copy).
 *
 * For frame_iter = 0..9 the backdrop is restored from the snapshot, one
 * band sprite from the tile-anim table is rendered as a filled circle, and
 * the viewport is blitted to the mode-13h framebuffer. Counterpart to
 * fd2_animate_warp_out_collapse @ 0x22547, which counts down with a
 * shrinking band; this one runs the band at a *constant* radius 0xB with a
 * constant band-top of 0, so the visible circle grows only as the sprite
 * table index advances.
 *
 * Sprite table at offset +6 of data_fd2_tile_anim_table_base (same idiom
 * as the collapse half):
 *   sprite_addr = *(int *)(table_base + 6 + frame_iter*4) + table_base;
 *
 * Of the seven cdecl params the caller passes
 * (dst_tile_x, dst_tile_y, snapshot, src_x, src_y, warp_in_sprite, radius)
 * only snapshot (backdrop source), src_x and src_y (band center) are read
 * by the body; the other four are vestigial (the binary never references
 * them), preserved in the signature to match the call site.
 *
 * Cdecl, 7 stack params; void return. The binary's __CHK(0x2C) stack-probe
 * prologue is compiler-injected and not part of the source. Explicit
 * self-contained RET at 0x226E9 (POP EBP/EDI/ESI/EBX; RET, no MOV EAX,EDI
 * since void). Unlike the collapse half (which tail-JMPs at 0x22651 into the
 * shared epilogue of fd2_roll_stat_gain_and_show_message @ 0x1E5B9), this
 * function owns its full epilogue and shares nothing with it.
 * ---------------------------------------------------------------- */
void fd2_animate_warp_in_expand(uint32 dst_tile_x, uint32 dst_tile_y,
    uint32 snapshot, uint32 src_x, uint32 src_y,
    uint32 *warp_in_sprite, int radius)
{
    uint32 frame_iter;
    uint32 sprite_addr;

    for (frame_iter = 0; (int)frame_iter < 10; frame_iter++) {
        sprite_addr =
            *(uint32 *)(data_fd2_tile_anim_table_base + 6 + frame_iter * 4) +
            data_fd2_tile_anim_table_base;
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
            (void *)snapshot, 0x25680);
        fd2_render_filled_circle_band_anim(src_x, src_y, 0xb, 0, 0xc0,
            sprite_addr);
        fd2_blit_rectangle(0xa0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);
        fd2_wait_n_bios_ticks(1);
    }
    return;
}

/* ----------------------------------------------------------------
 * fd2_cast_screen_wide_spell_with_fade @ 0x24618  (6 callers)
 *
 * BOSS / END-CHAPTER screen-wide spell visual: a full-screen radial
 * shockwave that expands over 9 frames, a long hold, then a palette
 * flash fade-in. Used by ULTIMATE / DRAGON-BREATH style spells and by
 * chapter init/end cinematics (chapters 22/23/27/28/30).
 *
 * Params (cdecl, 4 stack args; void return):
 *   epicenter_tile_x  — battle tile X of the shockwave epicenter
 *                       (pixel = tile_x*0x18 + 0xC). (Ghidra param_1.)
 *   epicenter_tile_y  — battle tile Y of the shockwave epicenter
 *                       (pixel = tile_y*0x18 + 0x10). (Ghidra param_2.)
 *   radius            — starting band radius; grows by radius_increment
 *                       every frame. (Ghidra param_3.)
 *   radius_increment  — per-frame radius growth. (Ghidra's 4th param name
 *                       "epicenter_tile_x" is MISLEADING — the assembly
 *                       @0x246FB does ESI += [ESP+0x28], i.e. radius +=
 *                       this 4th arg each frame.)
 *
 * Sequence (assembly @0x24618..0x2474F):
 *   fd2_load_status_effect_sfx();                       // preload status SFX bank
 *   snapshot = malloc(0x25680);                         // 150KB backdrop backup
 *   fd2_composite_battle_tile_map(snapshot+0x8088, ...);// snapshot battle tile map
 *   fd2_play_sfx_with_handle(status_sfx_handle, 0xB, 1);// boss-spell SFX
 *
 *   // 9-frame growing shockwave (frame_iter 9 -> 1):
 *   for frame_iter = 9..1:
 *     sprite = *(int*)(tile_anim_base + 6 + frame_iter*4) + tile_anim_base;
 *     memmove(large_game_state_buffer, snapshot, 0x25680);   // restore backdrop
 *     fd2_render_filled_circle_band_anim(ex*0x18+0xC, ey*0x18+0x10,
 *                                        radius, 0, 0xC0, sprite);
 *     fd2_blit_rectangle(0xA0504, 0x140, large_game_state_buffer+0x8088,
 *                        0x1C8, 0x138, 0xC0);
 *     radius += radius_increment;
 *     __delay_thunk_375b2(5);
 *
 *   free(snapshot);
 *   __delay_thunk_375b2(500);                           // long hold for impact
 *
 *   // palette flash fade-in (brightness 0 -> 0x3E in steps of 2):
 *   for brightness = 0; brightness < 0x40; brightness += 2:
 *     fd2_set_vga_palette_range_with_add(0, 0xFF, brightness);
 *     __delay_thunk_375b2(4);
 *
 *   fd2_load_status_effect_sfx();                       // re-arm SFX bank for next use
 *
 * The sprite atlas is the same self-relative tile-anim table used by the
 * other shockwave workers in this file: each entry
 * *(int*)(base + 6 + frame_iter*4) is an offset added back to the base and
 * handed to fd2_render_filled_circle_band_anim as its 6th arg (the
 * palette-remap / sprite source it passes straight through). The malloc'd
 * snapshot is the backdrop the band helper renders over each frame.
 *
 * Cdecl, 4 stack params; void return. The binary's __CHK(0x34) stack-probe
 * prologue is compiler-injected and not part of the source. There is no
 * explicit RET: the normal path tail-JMPs (JMP 0x10B46 @0x2474F) into the
 * shared inline-epilogue fragment fd2_noop_stub_b43 @ 0x10B43, entering at
 * +0x03 (0x10B46 = ADD ESP,0x8 / POP EBP/EDI/ESI/EBX / RET, matching this
 * function's SUB ESP,0x8 + 4-saved-reg frame). Emitted as a plain return;
 * the compiler regenerates the matching epilogue (see emit_issues.json
 * 00024618 + the 00010b43 fragment_equivalence_handoff family).
 * ---------------------------------------------------------------- */
void fd2_cast_screen_wide_spell_with_fade(uint32 epicenter_tile_x,
                                          uint32 epicenter_tile_y,
                                          uint32 radius, int radius_increment)
{
    uint32 epicenter_screen_x;
    uint32 epicenter_screen_y;
    uint32 snapshot;
    uint32 sprite_addr;
    int frame_iter;
    uint32 brightness;

    epicenter_screen_x = epicenter_tile_x * 0x18 + 0xc;
    epicenter_screen_y = epicenter_tile_y * 0x18 + 0x10;

    fd2_load_status_effect_sfx();

    snapshot = (uint32)malloc(0x25680);
    fd2_composite_battle_tile_map(snapshot + 0x8088, 0x1c8, 0xd, 8,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);
    fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr,
        0xb, 1);

    for (frame_iter = 9; frame_iter > 0; frame_iter--) {
        sprite_addr =
            *(uint32 *)(data_fd2_tile_anim_table_base + 6 + frame_iter * 4) +
            data_fd2_tile_anim_table_base;
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
            (void *)snapshot, 0x25680);
        fd2_render_filled_circle_band_anim(epicenter_screen_x,
            epicenter_screen_y, radius, 0, 0xc0, (int)sprite_addr);
        fd2_blit_rectangle(0xa0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);
        radius += radius_increment;
        __delay_thunk_375b2(5);
    }

    free((void *)snapshot);
    __delay_thunk_375b2(500);

    for (brightness = 0; (int)brightness < 0x40; brightness += 2) {
        fd2_set_vga_palette_range_with_add(0, 0xff, brightness);
        __delay_thunk_375b2(4);
    }

    fd2_load_status_effect_sfx();
    return;
}

/* ----------------------------------------------------------------
 * fd2_execute_special_attack_skill @ 0x276ec  (1 caller)
 *
 * Character-specific special attack technique (必殺技). Used for the
 * high-id combat-skill spell IDs: 0x18 (淒煌斬), 0x1C (熾炎刀, multi-hit
 * variant), 0x1D (音速刃), 0x1E (default fallback). Weapon-tied cinematic
 * attacks with bigger-than-normal damage and a full character-vs-character
 * FIGANI animation overlay. Sole caller: fd2_play_spell_cast_sequence
 * @ 0x2A6BD (the special-spell branch of the cast-sequence dispatch).
 *
 * Damage: multiplier = {0x18:15, 0x1C:20, 0x1D:12, default:18}[spell_id];
 *   raw_damage = (int16)caster.ap * multiplier / 10 (signed div). Per target
 *   applied_dmg = clamp(target.hp_current,
 *                       fd2_apply_damage_and_award_xp(tid, raw_damage - dp)).
 *   HP is restored to its pre-call value, then re-applied progressively over
 *   the animation: hp = original_HP - (hit_count * applied_dmg / max_hits),
 *   with max_hits = 8 for 0x1C, 1 otherwise.
 *
 * Resources: BG.DAT[tile_attr_byte], TAI.DAT[tile_attr_byte] (caster base
 *   sprite), BG.DAT[terrain], where tile_attr_byte is the 3rd of the 4 tile
 *   attribute-flag bytes that fd2_read_tile_attribute_at_pos writes at offset
 *   +6 of its 8-byte out-buffer (= attr_ptr[+2]; verified MOVZX EDI,[ESP+0x7e]
 *   @0x27861, buffer base LEA [ESP+0x78] @0x2783d). The out-buffer is sized 8
 *   to hold the full +0..+7 write the callee performs.
 *   BG.DAT[0..2] (3 parallax cinematic sub-layers @ 0x5410B/0F/13),
 *   FIGANI.DAT[caster.portrait*3 / *3+2] (intro + per-hit anim data),
 *   FIGANI.DAT[target.portrait*3] per target. The caster_figani_b SFX-bank
 *   byte feeds fd2_load_figani_sfx_bank → special_attack_sfx_bank @ 0x54117.
 *
 * The 6-byte X-offset shake table @ 0x52549 is copied onto the stack by the
 * binary; here it is indexed directly (const data, Layer-2 equivalent, same
 * convention as the earthquake worker above). fade_steps (0..5) selects the
 * per-sub-frame horizontal shake displacement during a hit.
 *
 * Cdecl, 4 stack params; void return. The binary's __CHK(0x100) stack-probe
 * prologue is compiler-injected and not part of the source. Self-contained
 * epilogue with explicit RET at 0x27FC8 (POP EBP/EDI/ESI/EBX). The plate at
 * 0x52393 previously mis-labelled the string as FDSHAP/FIGANI; it is TAI.DAT
 * (corrected during emit).
 *
 * KNOWN DECOMPILER NOTE: every CALL-then-EAX-use site here is a genuine
 * return-value capture (resource ptrs, terrain byte, applied damage, SFX
 * bank); verified against the assembly — no spurious EAX-tracking artifact.
 * ---------------------------------------------------------------- */
void fd2_execute_special_attack_skill(uint32 caster_idx, uint32 spell_id,
                                      int n_targets, uint8 *target_idx_buf)
{
    uint32 target_figani_arr[30];
    runtime_char *caster_char;
    runtime_char *target_char;
    uint32 multiplier;
    int32 raw_damage_div_10;
    uint8 tile_attr_buf[8];
    uint8 tile_attr_byte;
    uint8 resolved_terrain;
    uint32 pBg_layer;
    uint32 pBg_layer_saved;
    uint32 pTai_resource;
    uint32 pTai_layer;
    uint32 pBg_resource;
    uint32 pCaster_figani_a;
    uint32 pCaster_figani_b;
    uint32 pAnimWorkBuf1;
    uint32 pAnimWorkBuf2;
    uint32 portrait_x3;
    uint32 wrkbuf;
    uint32 frame_entry;
    uint32 frame_iter;
    uint32 original_HP;
    uint32 applied_dmg;
    uint8  max_hits;
    uint8  hit_count;
    uint8  fade_steps;
    uint32 palette_color_or_neg1;
    int i;
    int sub_iter;

    pTai_resource = 0;
    pBg_resource = 0;
    pCaster_figani_b = 0;
    pCaster_figani_a = 0;
    fade_steps = 0;
    palette_color_or_neg1 = 0xffffffff;

    free((void *)data_fd2_portrait_sprite_cache);
    free((void *)data_fd2_large_game_state_buffer_ptr);
    free((void *)data_fd2_battle_scene_snapshot);
    data_fd2_battle_scene_snapshot = 0;

    for (i = 0; i < 0x1e; i++) {
        target_figani_arr[i] = 0;
    }

    if (spell_id == 0x18) {
        multiplier = 0xf;
    } else if (spell_id == 0x1c) {
        multiplier = 0x14;
    } else if (spell_id == 0x1d) {
        multiplier = 0xc;
    } else {
        multiplier = 0x12;
    }

    caster_char = &data_fd2_battle_runtime_char_array_ptr[caster_idx];
    portrait_x3 = (int32)(int16)caster_char->ap * multiplier;
    raw_damage_div_10 = (int32)portrait_x3 / 10;

    fd2_read_tile_attribute_at_pos((uint32)caster_char->pos_x,
        (uint32)caster_char->pos_y, (uint32)tile_attr_buf);
    tile_attr_byte = tile_attr_buf[6];
    resolved_terrain =
        fd2_resolve_terrain_for_aoe_targets(n_targets, (uint8 *)target_idx_buf);

    pBg_layer = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381,
        pBg_resource, (uint32)tile_attr_byte);
    pBg_layer_saved = pBg_layer;
    pTai_resource = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_tai_dat,
        pTai_resource, (uint32)tile_attr_byte);
    pTai_layer = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381,
        pBg_resource, (uint32)resolved_terrain);

    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = 0;
    pBg_resource = pTai_layer;
    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381, 0, 0);
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381,
        data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr, 1);
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381,
        data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr, 2);

    pAnimWorkBuf1 = (uint32)malloc(64000);
    pAnimWorkBuf2 = (uint32)malloc(0x1f400);
    memset((void *)pAnimWorkBuf1, 0, 64000);
    fd2_flash_char_hit_sprite(pAnimWorkBuf1, caster_idx);
    if (spell_id == 0x1c) {
        fd2_rle_blit_sprite(pTai_layer, 0, 0x32, pAnimWorkBuf1, 0x140, 0xffffffff);
        fd2_flash_char_hit_sprite(pAnimWorkBuf1, (uint32)target_idx_buf[0]);
    } else {
        fd2_rle_blit_sprite(pBg_layer, 0, 0x32, pAnimWorkBuf1, 0x140, 0xffffffff);
    }

    portrait_x3 = (uint32)caster_char->portrait_id * 3;
    pCaster_figani_a = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388,
        pCaster_figani_a, portrait_x3);
    pCaster_figani_b = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388,
        pCaster_figani_b, portrait_x3 + 2);
    data_fd2_audio_figani_sfx_bank_buf_ptr =
        fd2_load_figani_sfx_bank(pCaster_figani_b);

    fd2_play_palette_fade_to_black();

    for (i = 0; i < n_targets; i++) {
        target_figani_arr[i] = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_figani_dat_52388,
            target_figani_arr[i],
            (uint32)data_fd2_battle_runtime_char_array_ptr[target_idx_buf[i]]
                .portrait_id * 3);
    }

    fd2_play_char_intro_zoom_anim(caster_idx, (uint32)(spell_id != 0x1c),
        pCaster_figani_a, target_figani_arr[0], pAnimWorkBuf2, pAnimWorkBuf1,
        pTai_resource);
    fd2_play_figani_animation_loop(caster_idx, spell_id, (uint8 *)pCaster_figani_b,
        (uint8 *)target_figani_arr[0], pAnimWorkBuf2, pAnimWorkBuf1, pBg_layer_saved,
        pTai_resource);

    for (i = 0; i < n_targets; i++) {
        memset((void *)pAnimWorkBuf2, 0, 0x1f400);
        fd2_blit_rectangle(pAnimWorkBuf2 + 0x140, 0x280, 0xa0000, 0x140,
            0x140, 0xc8);
        if (spell_id != 0x1c) {
            fd2_animate_bg_zoom_transition_in((uint32)target_idx_buf[i],
                target_figani_arr[i], pAnimWorkBuf1, pAnimWorkBuf2,
                pBg_resource);
        }
        fd2_step_figani_pose_animation(target_figani_arr[i], 0,
            pAnimWorkBuf2, 0x140);

        target_char =
            &data_fd2_battle_runtime_char_array_ptr[target_idx_buf[i]];
        original_HP = (uint32)(int16)target_char->hp_current;
        applied_dmg = (uint32)fd2_apply_damage_and_award_xp(
            (uint32)target_idx_buf[i],
            (uint32)(raw_damage_div_10 - (int32)(int16)target_char->dp));
        if ((int32)original_HP < (int32)applied_dmg) {
            applied_dmg = original_HP;
        }
        target_char->hp_current = (uint16)original_HP;

        if (spell_id == 0x1c) {
            max_hits = 8;
        } else {
            max_hits = 1;
        }
        hit_count = 0;

        for (frame_iter = (uint32)*(uint8 *)(pCaster_figani_b + 2);
             (int32)frame_iter < (int32)(uint32)*(uint8 *)pCaster_figani_b;
             frame_iter++) {
            frame_entry = pCaster_figani_b +
                *(int32 *)(pCaster_figani_b + 8 + frame_iter * 4);
            if (*(int8 *)(frame_entry + 5) != 0) {
                fd2_play_sfx_with_handle(data_fd2_audio_figani_sfx_bank_buf_ptr,
                    (uint32)*(uint8 *)(frame_entry + 5), 1);
            }
            if (*(int8 *)(frame_entry + 4) == 1) {
                fade_steps = 5;
                palette_color_or_neg1 = 0x21;
                hit_count = (uint8)(hit_count + 1);
                target_char->hp_current = (uint16)((int16)original_HP -
                    (int16)((int32)(hit_count * applied_dmg) /
                            (int32)(uint32)max_hits));
                fd2_flash_char_hit_sprite(pAnimWorkBuf1,
                    (uint32)target_idx_buf[i]);
            }
            for (sub_iter = 0;
                 sub_iter < (int32)(uint32)*(uint8 *)(frame_entry + 6);
                 sub_iter++) {
                wrkbuf = pAnimWorkBuf2 + 0x140;
                fd2_blit_rectangle(wrkbuf, 0x280, pAnimWorkBuf1, 0x140,
                    0x140, 0xc8);
                fd2_step_figani_pose_animation(target_figani_arr[i],
                    palette_color_or_neg1,
                    wrkbuf - data_fd2_battle_special_attack_shake_x_offset_table
                                 [fade_steps],
                    0x280);
                fd2_blit_indexed_sprite(pCaster_figani_b, frame_iter, wrkbuf,
                    0x280, -1);
                fd2_blit_rectangle(0xa0000, 0x140, wrkbuf, 0x280, 0x140, 0xc8);
                fd2_wait_n_bios_ticks(1);
                if (fade_steps != 0) {
                    fade_steps = (uint8)(fade_steps - 1);
                }
                palette_color_or_neg1 = 0xffffffff;
            }
        }
    }

    for (i = 0; i < n_targets; i++) {
        free((void *)target_figani_arr[i]);
    }
    free((void *)pBg_layer_saved);
    free((void *)pBg_resource);
    free((void *)pTai_resource);
    free((void *)pBg_layer_saved);
    free((void *)pAnimWorkBuf1);
    free((void *)pAnimWorkBuf2);
    free((void *)data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr);
    free((void *)data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr);
    free((void *)data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr);
    free((void *)pCaster_figani_a);
    free((void *)pCaster_figani_b);

    fd2_play_sfx_with_handle(data_fd2_audio_figani_sfx_bank_buf_ptr,
        0xffffffff, 1);
    if (data_fd2_audio_figani_sfx_bank_buf_ptr != 0) {
        free((void *)data_fd2_audio_figani_sfx_bank_buf_ptr);
    }

    data_fd2_large_game_state_buffer_ptr = (uint32)malloc(0x25680);
    data_fd2_battle_scene_snapshot = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
        data_fd2_battle_scene_snapshot,
        (uint32)*(uint8 *)data_fd2_tile_event_data_table_ptr * 2);
    fd2_restore_portrait_cache_from_tmp();
    fd2_wait_n_bios_ticks(6);
    fd2_play_palette_fade_to_black();
    memset((void *)0xa0000, 0, 64000);
    fd2_composite_battle_frame(1);
    fd2_play_palette_fade_in();
    return;
}

/* ----------------------------------------------------------------
 * fd2_execute_summon_spell_cast @ 0x27fc9  (1 caller)
 *
 * 召喚系 (summon) spell cinematic for spell ids 0x20 (熾天使), 0x21 (風妖精),
 * 0x22 (破壞神), 0x23 (暗邪鬼). A long multi-phase cinematic: caster cast pose,
 * a sprite slide-in / slide-out, a per-pose-frame summon animation with
 * per-summon SFX hooks, an optional strobe with palette-cycle FX, a palette
 * fade-in back to the battle scene, then a spell-id-specific gameplay effect.
 *
 * Params (cdecl, 4 stack args; void return):
 *   caster_idx       — runtime_char_array index of the caster
 *   spell_id         — 0x20..0x23 (the four summon spells)
 *   n_targets        — target count
 *   target_id_array  — pointer to the uint8 target-id array (Ghidra's auto-name
 *                      "caster_idx" for this 4th param is MISLEADING: it is the
 *                      target-id array base, reloaded in the per-target loops).
 *
 * Per-summon palette + SFX-bank-index tables (each is a 4-byte table indexed
 * by spell_id - 0x20; the binary copies each onto the stack as a dword and
 * byte-indexes it, reproduced verbatim here via local dword copies):
 *   data_fd2_battle_summon_spell_palette_r_table   @ 0x5254F = {3F,33,35,35}
 *   data_fd2_battle_summon_spell_palette_g_table   @ 0x52553 = {3F,39,00,3A}
 *   data_fd2_battle_summon_spell_palette_b_table   @ 0x52557 = {3F,3F,00,09}
 *   data_fd2_battle_summon_spell_sfx_bank_index_table @ 0x5255B = {5B,5C,5D,5E}
 *     (FDOTHER.DAT entry index of the per-summon SFX bank — the binary
 *      dword-loads it to [ESP+0x14] then byte-indexes it at 0x28141 by
 *      spell_id-0x20; the Ghidra plate's "anim length factor" label is wrong,
 *      the sfx_bank_index name is correct, verified MOVZX EAX,[ESP+EAX-0xc].)
 *
 * Resources (verified by address; the Ghidra plate's FDSHAP/FDOTHER labels are
 * MISLABELLED — the actual filename strings at these addresses are):
 *   TAI.DAT[0x52393]    → caster cast-pose figani (plate wrongly said FDSHAP)
 *   BG.DAT[0x52381]     → battle background
 *   FIGANI.DAT[0x52388] (idx caster.portrait_id*3 and +1) → caster pose A/B
 *                         (plate wrongly said FDOTHER)
 *   FDOTHER.DAT[0x51A4D] (idx spell_id+0x21) → summon sprite
 *   FDOTHER.DAT[0x51A4D] (idx sfx_bank_index_table[spell_id-0x20]) → SFX bank
 *   FDSHAP.DAT[0x51A65] (idx *tile_event_data*2) → data_fd2_battle_scene_snapshot
 *                         (plate wrongly said FDOTHER)
 *
 * Phases (each blits to the mode-13h framebuffer at 0xA0000):
 *   1. Caster cast pose: intro-zoom + figani loop, then a 6-tick pre-cast hold.
 *   2. 8-frame slide-in of pose A (offset loop*0x14, 20px/frame).
 *   3. 9-frame slide-out of the summon sprite (offset iVar2*0x1E, 30px/frame).
 *   4. (0x21/0x22 only) one extra summon-sprite blit at base.
 *   5. Per-pose-frame loop (uVar3 1..summon.frame_count-1): blit summon frame
 *      uVar3, with per-summon SFX hooks at specific frames.
 *   6. (0x20/0x23 only) 11-frame strobe between the last two summon frames with
 *      a shrinking palette-interpolation toward the per-summon RGB.
 *   7. Reset to game state: free temp buffers, re-alloc the large game-state
 *      buffer + data_fd2_battle_scene_snapshot, then a 0x29-step palette fade-in.
 *   8. Gameplay effect dispatch by spell_id:
 *      0x20 — attack-spell damage (spell 0x20).
 *      0x21 — clear per-target status bytes [+0x25..+0x27] then group HP heal 800.
 *      0x22 — AP + DP + speed boost (queue idx reset between each).
 *      0x23 — three status-inflict casts (spell 0x1A/0x16/0x1B, sprite 0x25/0x27/0x26).
 *
 * Cdecl, 4 stack params; void return. The binary's __CHK(0x6c) stack-probe
 * prologue is compiler-injected and not part of the source. Self-contained
 * epilogue with explicit RET at 0x286BC (ADD ESP,0x38 / POP EBP/EDI/ESI/EBX).
 *
 * KNOWN DECOMPILER NOTE: every CALL-then-EAX-use site here is a genuine return
 * capture (fd2_load_dat_resource / malloc pointers); verified against the
 * assembly — no spurious EAX-tracking artifact. Sole caller:
 * fd2_play_spell_cast_sequence @ 0x2A6BD via the spell handler table @ 0x51D01.
 * ---------------------------------------------------------------- */
void fd2_execute_summon_spell_cast(uint32 caster_idx, uint32 spell_id,
                                   uint32 n_targets, int target_id_array)
{
    uint32 palette_R;
    uint32 palette_G;
    uint32 palette_B;
    uint32 sfx_bank_index;
    runtime_char *caster_char;
    uint8 tile_attr_buf[8];
    uint8 tile_attr_byte;
    uint32 pTai_resource;
    uint32 pBg_layer;
    uint32 pCaster_figani;
    uint32 pWorkbuf_64k;
    uint32 pCaster_figani_a;
    uint32 pCaster_figani_b;
    uint32 pSummon_sprite;
    uint32 pSfx_bank;
    uint32 figani_idx_x3;
    uint8  summon_idx;
    uint32 palette_offset;
    int loop_iter;
    int iVar2;
    uint32 uVar3;
    int strobe_iter;
    int target_idx;

    /* Per-summon palette + sfx-bank-index tables: copy each 4-byte table into
     * a local dword (matching the binary's MOV [ESP+...],EAX), then byte-index
     * by spell_id-0x20. */
    palette_R      = data_fd2_battle_summon_spell_palette_r_table;
    palette_G      = data_fd2_battle_summon_spell_palette_g_table;
    palette_B      = data_fd2_battle_summon_spell_palette_b_table;
    sfx_bank_index = data_fd2_battle_summon_spell_sfx_bank_index_table;

    free((void *)data_fd2_large_game_state_buffer_ptr);
    free((void *)data_fd2_battle_scene_snapshot);
    data_fd2_battle_scene_snapshot = 0;

    caster_char = &data_fd2_battle_runtime_char_array_ptr[caster_idx];
    /* tile_attr_buf is sized 8 to hold the full +0..+7 write fd2_read_tile_-
     * attribute_at_pos performs (verified @0x12e76..0x12ea2); the 3rd tile
     * attribute-flag byte at offset +6 is the background variant (read here
     * as [ESP+0x6] @0x2804e), same convention as the sibling above. */
    fd2_read_tile_attribute_at_pos((uint32)caster_char->pos_x,
        (uint32)caster_char->pos_y, (uint32)tile_attr_buf);
    tile_attr_byte = tile_attr_buf[6];

    pTai_resource = (uint32)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_tai_dat, 0,
        (uint32)tile_attr_byte);
    pBg_layer = (uint32)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381, 0,
        (uint32)tile_attr_byte);

    pCaster_figani = (uint32)malloc(64000);
    pWorkbuf_64k = (uint32)malloc(0x1f400);
    memset((void *)pCaster_figani, 0, 64000);
    fd2_rle_blit_sprite(pBg_layer, 0, 0x32, pCaster_figani, 0x140, 0xffffffff);
    fd2_flash_char_hit_sprite(pCaster_figani, caster_idx);
    fd2_play_palette_fade_to_black();

    figani_idx_x3 = (uint32)caster_char->portrait_id * 3;
    pCaster_figani_a = (uint32)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
        figani_idx_x3);
    pCaster_figani_b = (uint32)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
        figani_idx_x3 + 1);
    pSummon_sprite = (uint32)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0,
        spell_id + 0x21);
    data_fd2_audio_summon_spell_sfx_bank_buf_ptr = 0;
    pSfx_bank = (uint32)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0,
        (uint32)((uint8 *)&sfx_bank_index)[spell_id - 0x20]);
    data_fd2_audio_summon_spell_sfx_bank_buf_ptr = pSfx_bank;

    /* Phase 1 — caster cast pose. */
    fd2_play_char_intro_zoom_anim(caster_idx, 1, pCaster_figani_a, 0,
        pWorkbuf_64k, (int)pCaster_figani, pTai_resource);
    fd2_play_figani_animation_loop(caster_idx, spell_id, (uint8 *)pCaster_figani_b,
        (uint8 *)pCaster_figani_b, pWorkbuf_64k, pCaster_figani, pBg_layer,
        pTai_resource);
    fd2_wait_n_bios_ticks(6);

    /* Phase 2 — 8-frame slide-in of pose A. */
    for (loop_iter = 0; loop_iter < 8; loop_iter++) {
        fd2_blit_rectangle(pWorkbuf_64k, 0x280, pCaster_figani, 0x140,
            0x140, 0xc8);
        fd2_blit_indexed_sprite(pCaster_figani_a, 0,
            loop_iter * 0x14 + pWorkbuf_64k, 0x280, -1);
        fd2_blit_rectangle(0xa0000, 0x140, pWorkbuf_64k, 0x280, 0x140, 0xc8);
        fd2_wait_n_bios_ticks(1);
    }

    /* Phase 3 — 9-frame slide-out of the summon sprite. */
    for (iVar2 = 8; iVar2 >= 0; iVar2--) {
        fd2_blit_rectangle(pWorkbuf_64k, 0x280, pCaster_figani, 0x140,
            0x140, 0xc8);
        fd2_blit_indexed_sprite(pSummon_sprite, 0,
            iVar2 * 0x1e + pWorkbuf_64k, 0x280, -1);
        fd2_blit_rectangle(0xa0000, 0x140, pWorkbuf_64k, 0x280, 0x140, 0xc8);
        fd2_wait_n_bios_ticks(1);
    }

    /* Phase 4 — single extra summon-sprite blit (0x21 / 0x22 only). */
    if (spell_id == 0x21 || spell_id == 0x22) {
        fd2_blit_indexed_sprite(pSummon_sprite, 0, pCaster_figani, 0x140, -1);
    }

    /* Phase 5 — per-pose-frame summon animation with per-summon SFX hooks. */
    for (uVar3 = 1; (int32)uVar3 < (int32)(uint32)*(uint8 *)pSummon_sprite;
         uVar3++) {
        memmove((void *)pWorkbuf_64k, (void *)pCaster_figani, 64000);
        fd2_blit_indexed_sprite(pSummon_sprite, uVar3, pWorkbuf_64k, 0x140, -1);
        fd2_blit_rectangle(0xa0000, 0x140, pWorkbuf_64k, 0x140, 0x140, 0xc8);
        if (spell_id == 0x22 && uVar3 == 2) {
            fd2_play_sfx_with_handle(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 1, 1);
        } else if (spell_id == 0x23 && uVar3 == 1) {
            fd2_play_sfx_sample_from_bank(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 2, uVar3);
        } else if (spell_id == 0x21 && uVar3 == 6) {
            fd2_play_sfx_with_handle(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 1, 1);
        } else if (spell_id == 0x20 && uVar3 == 1) {
            fd2_play_sfx_sample_from_bank(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 2, uVar3);
        }
        fd2_wait_n_bios_ticks(2);
    }

    summon_idx = (uint8)(spell_id - 0x20);

    /* Phase 6 — 11-frame strobe with palette interpolation (0x20 / 0x23 only). */
    if (spell_id == 0x20 || spell_id == 0x23) {
        for (strobe_iter = 0; strobe_iter < 0xb; strobe_iter++) {
            if (strobe_iter % 2 == 0) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 1, 1);
            }
            memmove((void *)pWorkbuf_64k, (void *)pCaster_figani, 64000);
            fd2_blit_indexed_sprite(pSummon_sprite,
                (uint32)(*(uint8 *)pSummon_sprite - 2) + (strobe_iter & 1),
                pWorkbuf_64k, 0x140, -1);
            fd2_blit_rectangle(0xa0000, 0x140, pWorkbuf_64k, 0x140, 0x140,
                0xc8);
            fd2_wait_n_bios_ticks(2);
            palette_offset = (uint32)summon_idx;
            fd2_interpolate_palette_range_toward_color(0, 0xff,
                strobe_iter * -4 + 0x28,
                (uint32)((uint8 *)&palette_R)[palette_offset],
                (uint32)((uint8 *)&palette_G)[palette_offset],
                (uint32)((uint8 *)&palette_B)[palette_offset]);
        }
    }

    /* Phase 7 — free temp buffers, re-alloc game state, palette fade-in. */
    free((void *)pSummon_sprite);
    free((void *)pBg_layer);
    free((void *)pTai_resource);
    free((void *)pCaster_figani);
    free((void *)pWorkbuf_64k);
    free((void *)pCaster_figani_a);
    free((void *)pCaster_figani_b);

    data_fd2_large_game_state_buffer_ptr = (uint32)malloc(0x25680);
    data_fd2_battle_scene_snapshot = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
        data_fd2_battle_scene_snapshot,
        (uint32)*(uint8 *)data_fd2_tile_event_data_table_ptr * 2);

    palette_offset = (uint32)summon_idx;
    fd2_interpolate_palette_range_toward_color(0, 0xff, 0,
        (uint32)((uint8 *)&palette_R)[palette_offset],
        (uint32)((uint8 *)&palette_G)[palette_offset],
        (uint32)((uint8 *)&palette_B)[palette_offset]);
    memset((void *)0xa0000, 0, 64000);
    fd2_composite_battle_frame(1);

    for (uVar3 = 0; (int32)uVar3 < 0x29; uVar3++) {
        palette_offset = (uint32)summon_idx;
        fd2_interpolate_palette_range_toward_color(0, 0xff, uVar3,
            (uint32)((uint8 *)&palette_R)[palette_offset],
            (uint32)((uint8 *)&palette_G)[palette_offset],
            (uint32)((uint8 *)&palette_B)[palette_offset]);
        __delay_thunk_375b2(6);
    }

    fd2_play_sfx_with_handle(data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
        0xffffffff, 1);
    free((void *)data_fd2_audio_summon_spell_sfx_bank_buf_ptr);

    /* Phase 8 — gameplay effect dispatch by spell_id. */
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_load_status_effect_sfx();

    if (spell_id == 0x20) {
        fd2_apply_attack_spell_damage(caster_idx, n_targets,
            (uint32)target_id_array, 0x20);
    } else if (spell_id == 0x21) {
        for (target_idx = 0; target_idx < (int)n_targets; target_idx++) {
            memset(&data_fd2_battle_runtime_char_array_ptr[
                ((uint8 *)target_id_array)[target_idx]].status_flags_block[4],
                0, 3);
        }
        fd2_cast_group_hp_heal_spell(caster_idx, n_targets,
            (uint32)target_id_array, 800);
    } else if (spell_id == 0x22) {
        fd2_cast_ap_boost_spell((int)caster_idx, (int)n_targets,
            (uint8 *)target_id_array);
        data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
        fd2_cast_dp_boost_spell((int)caster_idx, (int)n_targets,
            (uint32)target_id_array);
        data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
        fd2_cast_speed_boost_spell(caster_idx, n_targets,
            (uint32)target_id_array);
    } else if (spell_id == 0x23) {
        fd2_cast_status_inflict_spell(caster_idx, 0x1a, n_targets,
            (uint32)target_id_array, 0x25);
        data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
        fd2_cast_status_inflict_spell(caster_idx, 0x16, n_targets,
            (uint32)target_id_array, 0x27);
        data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
        fd2_cast_status_inflict_spell(caster_idx, 0x1b, n_targets,
            (uint32)target_id_array, 0x26);
    }

    fd2_play_and_free_status_effect_sfx();
    return;
}
