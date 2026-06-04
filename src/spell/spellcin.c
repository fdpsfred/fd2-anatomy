/*
 * spellcin.c — Spell cinematic worker functions (full-screen FX casts)
 *
 * Functions:
 *   fd2_cast_earthquake_spell_with_screen_shake @ 0x21548 (1 caller)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <stdio.h>

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
