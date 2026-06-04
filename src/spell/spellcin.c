/*
 * spellcin.c — Spell cinematic worker functions (full-screen FX casts)
 *
 * Functions:
 *   fd2_cast_earthquake_spell_with_screen_shake @ 0x21548 (1 caller)
 *   fd2_play_rising_pre_cast_effect @ 0x2189a (6 callers)
 *   fd2_dispatch_variant_b_cast @ 0x21b18 (1 caller)
 *   fd2_execute_aoe_spell_with_caster_portrait_radial_scatter @ 0x21bd0 (0 callers)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

    orig_battle_scene_snapshot = battle_scene_snapshot;
    battle_scene_snapshot = (uint32)fd2_convert_battle_tiles_to_24px();
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
                battle_scene_snapshot + (uint32)tile_attr_buf[0] * 0x240 + 6;
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
    free((void *)battle_scene_snapshot);
    battle_scene_snapshot = orig_battle_scene_snapshot;
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
