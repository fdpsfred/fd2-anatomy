/*
 * anisummn.c — summon-spell animation tick state machines
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_tick_sprite_animation_step @ 0x2673F
 *
 * One tick of frame-paced sprite animation. Renders current frame,
 * advances tick counter, and moves to next frame when hold expires.
 * ---------------------------------------------------------------- */
void fd2_tick_sprite_animation_step(uint8 *p_frame_idx, uint8 *p_tick,
                                     int x, int y, uint32 atlas)
{
    uint32 frame_off;

    fd2_blit_indexed_sprite(atlas, (uint32)*p_frame_idx, x, y, -1);
    frame_off = *(uint32 *)(atlas + (uint32)*p_frame_idx * 4 + 8);
    (*p_tick)++;
    if (*p_tick == *(uint8 *)(atlas + frame_off + 6)) {
        *p_tick = 0;
        (*p_frame_idx)++;
    }
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_spell_minor_animation_state @ 0x275D6
 *
 * Single-sprite minor animation state machine for summon spells.
 * Dispatch table entry #9 (last) at 0x523B9.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_spell_minor_animation_state(
    uint32 sprite_handle, uint32 sprite_atlas,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    (void)sprite_handle;

    if (state_code == 0) {
        data_fd2_battle_summon_minor_anim_alternating_blit_toggle = 0;
        data_fd2_battle_summon_minor_anim_state5_frame_counter = 1;
        return 0x14;
    }
    if (state_code == 3) return 0x3C;
    if (state_code == 6) return 0x14;

    if (state_code == 1 || state_code == 7) {
        if (data_fd2_battle_summon_minor_anim_alternating_blit_toggle
            == 0) {
            fd2_blit_indexed_sprite(
                sprite_atlas, 0, (int)origin_y,
                (int)row_stride, -1);
        }
        data_fd2_battle_summon_minor_anim_alternating_blit_toggle ^= 1;
        return 0;
    }

    if (state_code == 4) {
        fd2_blit_indexed_sprite(
            sprite_atlas, 0, (int)origin_y,
            (int)row_stride, -1);
        return 0;
    }

    if (state_code == 5) {
        fd2_blit_indexed_sprite(
            sprite_atlas,
            (int)data_fd2_battle_summon_minor_anim_state5_frame_counter
                / 2,
            (int)origin_y, (int)row_stride, -1);
        if (data_fd2_battle_summon_minor_anim_state5_frame_counter
            == 6) {
            fd2_play_sfx_with_handle(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                1, 1);
        } else if (
            data_fd2_battle_summon_minor_anim_state5_frame_counter
            == 0x24) {
            fd2_play_sfx_sample_from_bank(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                2, 1);
        }
        data_fd2_battle_summon_minor_anim_state5_frame_counter++;
        if (data_fd2_battle_summon_minor_anim_state5_frame_counter
                < 0x2C &&
            data_fd2_battle_summon_minor_anim_state5_frame_counter
                > 0x10) {
            return 1;
        }
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_d_3slot @ 0x272B8
 *
 * Variant-D 3-active-slot summon animation. Dispatch table #7.
 * 10 color row offsets, mod-2 frame toggle, mod-10 color rotation.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_d_3slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    uint32 local_offsets[10];
    int i;
    int done_flag;
    int frame;
    int color;
    uint8 *pChar;

    done_flag = 0;
    memcpy(local_offsets,
           data_fd2_animation_summon_variant_d_3slot_color_row_offsets,
           40);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 10; i++)
            local_offsets[i] += 0x82;
    }

    if (state_code == 0) {
        for (i = 0; i < 4; i++) {
            data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]
                = -3 * i;
            data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i]
                = i;
        }
        data_fd2_battle_summon_anim_variant_d_color_rotation_counter
            = 4;
        data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
        data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle
            = 0;
        return 2;
    }
    if (state_code == 3) return 0x20;
    if (state_code == 6) {
        data_fd2_battle_summon_anim_variant_d_terminate_flag = 1;
        return 0x10;
    }

    if (state_code == 2 || state_code == 5 || state_code == 8) {
        data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle =
            (data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle
             + 1) % 2;

        for (i = 0; i < 3; i++) {
            frame =
                data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i];
            color =
                data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i];

            if (frame >= 0 && frame < 5) {
                fd2_blit_indexed_sprite(
                    sprite_handle, frame,
                    (int)origin_y + (int)local_offsets[color],
                    (int)row_stride, -1);
            }

            if (data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle
                == 0) {
                if (frame == 1) {
                    if (i == 0)
                        fd2_play_sfx_with_handle(
                            data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                            1, 1);
                    else if (i == 1)
                        fd2_play_sfx_sample_from_bank(
                            data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                            1, 1);
                }
                data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]++;
                if (data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]
                    == 2)
                    done_flag = 1;
                if (data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]
                        == 7 &&
                    data_fd2_battle_summon_anim_variant_d_terminate_flag
                        == 0) {
                    data_fd2_battle_summon_anim_variant_d_color_rotation_counter =
                        (data_fd2_battle_summon_anim_variant_d_color_rotation_counter
                         + 1) % 10;
                    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i] =
                        (int32)data_fd2_battle_summon_anim_variant_d_color_rotation_counter;
                    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]
                        = 0;
                }
            }
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_e_16slot @ 0x274B0
 *
 * Variant-E 16-slot summon animation. Simplest variant — no team
 * adjust, no color rotation, per-slot sprite base offsets.
 * Dispatch table #8.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_e_16slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    uint8 sprite_bases[16];
    int i;
    int done_flag;
    int frame;

    (void)caster_unit_id;
    memcpy(sprite_bases,
           data_fd2_animation_summon_variant_e_16slot_sprite_base_table,
           16);
    done_flag = 0;

    if (state_code == 0) {
        for (i = 0; i < 16; i++)
            data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i]
                = -2 * i;
        return 3;
    }
    if (state_code == 3) return 0x22;
    if (state_code == 6) return 2;

    if (state_code == 2 || state_code == 5) {
        for (i = 0; i < 16; i++) {
            frame =
                data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i];
            if (frame >= 0 && frame < 8) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    frame + (int)(uint32)sprite_bases[i],
                    (int)origin_y, (int)row_stride, -1);
            }
            if (frame == 0)
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                    1, 1);
            if (frame == 4)
                fd2_play_sfx_sample_from_bank(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                    2, 1);
            data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i]++;
            if (data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i]
                == 4)
                done_flag = 1;
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_a_6slot @ 0x269D3
 *
 * Variant-A 6-slot summon animation with RNG-based jitter.
 * Dispatch table #4. EAX tracking bug in decompiler corrected:
 * jitter uses fd2_advance_rng_state() return, not loop index.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_a_6slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    int32 local_offsets[10];
    int i;
    int done_flag;
    int frame;
    int color;
    uint32 rng_val;
    int remainder;
    uint8 *pChar;

    done_flag = 0;
    memcpy(local_offsets,
           data_fd2_battle_summon_anim_variant_a_10color_y_offset_table,
           40);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 10; i++)
            local_offsets[i] += 0x8F;
    }

    if (state_code == 0) {
        for (i = 0; i < 6; i++) {
            data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]
                = -2 * i;
            data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i]
                = i;
            rng_val = fd2_advance_rng_state();
            remainder = (int)(rng_val % 2);
            data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[i]
                = (uint8)((remainder << 3) - remainder);
        }
        data_fd2_battle_summon_anim_variant_a_color_rotation_counter
            = 6;
        data_fd2_battle_summon_anim_variant_a_terminate_flag = 0;
        return 2;
    }
    if (state_code == 3) return 0xC;
    if (state_code == 6) {
        data_fd2_battle_summon_anim_variant_a_terminate_flag = 1;
        return 8;
    }

    if (state_code == 2 || state_code == 5 || state_code == 8) {
        for (i = 0; i < 6; i++) {
            frame =
                data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i];
            color =
                data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i];

            if (frame >= 0 && frame < 7) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    (int)(uint32)data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[i]
                        + frame,
                    (int)origin_y + local_offsets[color],
                    (int)row_stride, -1);
            }
            if (frame == 0)
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                    1, 1);

            data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]++;
            if (data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]
                == 3)
                done_flag = 1;

            if (data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]
                    == 8 &&
                data_fd2_battle_summon_anim_variant_a_terminate_flag
                    == 0) {
                data_fd2_battle_summon_anim_variant_a_color_rotation_counter++;
                data_fd2_battle_summon_anim_variant_a_color_rotation_counter =
                    data_fd2_battle_summon_anim_variant_a_color_rotation_counter
                    % 10;
                data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i]
                    = (int32)data_fd2_battle_summon_anim_variant_a_color_rotation_counter;
                data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]
                    = 0;
                rng_val = fd2_advance_rng_state();
                remainder = (int)(rng_val % 2);
                data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[i]
                    = (uint8)((remainder << 3) - remainder);
            }
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_b_6slot @ 0x26BFD
 *
 * Variant-B 6-slot summon animation with RNG jitter (0 or 6).
 * Dispatch table #5. Similar to variant_a but different timings,
 * 2-bucket SFX, and jitter multiplier 6.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_b_6slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    int32 local_offsets[10];
    int i;
    int done_flag;
    int frame;
    int color;
    uint32 rng_val;
    uint8 *pChar;

    done_flag = 0;
    memcpy(local_offsets,
           data_fd2_battle_summon_anim_variant_b_10color_y_offset_table,
           40);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 10; i++)
            local_offsets[i] += 0x8F;
    }

    if (state_code == 0) {
        for (i = 0; i < 6; i++) {
            data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]
                = -2 * i;
            data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i]
                = i;
            rng_val = fd2_advance_rng_state();
            data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[i]
                = (uint8)((rng_val % 2) * 6);
        }
        data_fd2_battle_summon_anim_variant_b_color_rotation_counter
            = 6;
        data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
        return 1;
    }
    if (state_code == 3) return 0xC;
    if (state_code == 6) {
        data_fd2_battle_summon_anim_variant_b_terminate_flag = 1;
        return 8;
    }

    if (state_code == 2 || state_code == 5 || state_code == 8) {
        for (i = 0; i < 6; i++) {
            frame =
                data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i];
            color =
                data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i];

            if (frame >= 0 && frame < 6) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    (int)(uint32)data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[i]
                        + frame,
                    (int)origin_y + local_offsets[color],
                    (int)row_stride, -1);
            }
            if (frame == 0) {
                if (i == 0)
                    fd2_play_sfx_with_handle(
                        data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                        1, 1);
                else if (i == 3)
                    fd2_play_sfx_sample_from_bank(
                        data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                        1, 1);
            }

            data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]++;
            if (data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]
                == 2)
                done_flag = 1;

            if (data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]
                    == 7 &&
                data_fd2_battle_summon_anim_variant_b_terminate_flag
                    == 0) {
                data_fd2_battle_summon_anim_variant_b_color_rotation_counter++;
                data_fd2_battle_summon_anim_variant_b_color_rotation_counter =
                    data_fd2_battle_summon_anim_variant_b_color_rotation_counter
                    % 10;
                data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i]
                    = (int32)data_fd2_battle_summon_anim_variant_b_color_rotation_counter;
                data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]
                    = 0;
                rng_val = fd2_advance_rng_state();
                data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[i]
                    = (uint8)((rng_val % 2) * 6);
            }
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_spell_animation_state @ 0x26528
 *
 * Generic single-target summon animation. Dispatch table #2.
 * Plate comment had incorrect is_enemy condition for state {1,7}
 * and state 4 — assembly verified: NON-enemy does the tick/blit.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_spell_animation_state(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    int is_enemy;
    int done_flag;
    uint8 *pChar;

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    is_enemy = (pChar[6] == 0) ? 1 : 0;
    done_flag = 0;

    if (state_code == 0) {
        data_fd2_battle_summon_spell_anim_phase_byte = 0;
        data_fd2_battle_summon_spell_anim_aux_state_byte_unread = 0;
        data_fd2_battle_summon_spell_sprite_anim_tick_counter = 0;
        return 0x1D;
    }
    if (state_code == 3) {
        data_fd2_battle_summon_spell_anim_phase_byte = 0x10;
        return 0xC;
    }
    if (state_code == 6) {
        fd2_play_sfx_with_handle(
            data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 3, 1);
        data_fd2_battle_summon_spell_anim_phase_byte = 0xA;
        return 0xA;
    }

    if ((state_code == 1 || state_code == 7) && !is_enemy) {
        if (data_fd2_battle_summon_spell_anim_phase_byte == 0xA
            && state_code == 1)
            data_fd2_battle_summon_spell_anim_phase_byte = 0xF;
        fd2_tick_sprite_animation_step(
            &data_fd2_battle_summon_spell_anim_phase_byte,
            &data_fd2_battle_summon_spell_sprite_anim_tick_counter,
            origin_y, row_stride, sprite_handle);
        return 0;
    }

    if (state_code == 2 || state_code == 8) {
        if (data_fd2_battle_summon_spell_anim_phase_byte == 7)
            fd2_play_sfx_with_handle(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                1, 1);
        if (is_enemy) {
            if (data_fd2_battle_summon_spell_anim_phase_byte
                    == 0xA && state_code == 2)
                data_fd2_battle_summon_spell_anim_phase_byte
                    = 0xF;
            fd2_tick_sprite_animation_step(
                &data_fd2_battle_summon_spell_anim_phase_byte,
                &data_fd2_battle_summon_spell_sprite_anim_tick_counter,
                origin_y, row_stride, sprite_handle);
        }
        if (data_fd2_battle_summon_spell_anim_phase_byte == 0x10)
            fd2_blit_indexed_sprite(
                sprite_handle, 0x10, (int)origin_y,
                (int)row_stride, -1);
        return 0;
    }

    if (state_code == 4 && !is_enemy) {
        fd2_blit_indexed_sprite(
            sprite_handle, 0xF,
            (int)origin_y + 1 - (int)row_stride,
            (int)row_stride, -1);
        return 0;
    }

    if (state_code == 5) {
        if (is_enemy)
            fd2_blit_indexed_sprite(
                sprite_handle, 0xF,
                (int)origin_y - 1 - (int)row_stride,
                (int)row_stride, -1);
        fd2_blit_indexed_sprite(
            sprite_handle,
            (int)(uint32)data_fd2_battle_summon_spell_anim_phase_byte,
            (int)origin_y + 1 - (int)row_stride,
            (int)row_stride, -1);
        data_fd2_battle_summon_spell_anim_phase_byte++;
        if (data_fd2_battle_summon_spell_anim_phase_byte == 0x11) {
            fd2_play_sfx_with_handle(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                2, 1);
            done_flag = 1;
        } else if (data_fd2_battle_summon_spell_anim_phase_byte
                   == 0x12) {
            data_fd2_battle_summon_spell_anim_phase_byte = 0x10;
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_spell_main_animation_state @ 0x26795
 *
 * Main 12-slot summon animation. Dispatch table #3.
 * 12 color rotation mod 12, odd/even frame toggle,
 * 3 rodata tables (y-offsets, v-offsets, sprite offsets).
 * ---------------------------------------------------------------- */
int fd2_tick_summon_spell_main_animation_state(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    int32 y_offsets[12];
    uint8 v_offsets[12];
    uint8 spr_offsets[12];
    int i;
    int done_flag;
    int frame;
    int color;
    uint8 *pChar;

    done_flag = 0;
    memcpy(y_offsets,
           data_fd2_battle_summon_main_anim_12slot_y_offset_table, 48);
    memcpy(v_offsets,
           data_fd2_battle_summon_main_anim_12color_v_offset_table, 12);
    memcpy(spr_offsets,
           data_fd2_battle_summon_main_anim_12color_sprite_offset_table,
           12);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 12; i++)
            y_offsets[i] += 0x14;
    }

    if (state_code == 0) {
        for (i = 0; i < 12; i++) {
            data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                = -2 * i;
            data_fd2_battle_summon_main_anim_12slot_color_idx_array[i]
                = i;
        }
        data_fd2_battle_summon_main_anim_color_rotation_counter = 12;
        data_fd2_battle_summon_main_anim_terminate_flag = 0;
        data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
        return 2;
    }
    if (state_code == 3) return 0x28;
    if (state_code == 6) {
        data_fd2_battle_summon_main_anim_terminate_flag = 1;
        return 0x14;
    }

    if (state_code == 2 || state_code == 5 || state_code == 8) {
        data_fd2_battle_summon_main_anim_odd_even_frame_toggle =
            (data_fd2_battle_summon_main_anim_odd_even_frame_toggle
             + 1) % 2;

        for (i = 0; i < 12; i++) {
            frame =
                data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i];
            if (frame >= 0 && frame < 0xB) {
                color =
                    data_fd2_battle_summon_main_anim_12slot_color_idx_array[i];
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    (int)(uint32)spr_offsets[color] + frame,
                    (int)origin_y + y_offsets[color]
                        - (int)(uint32)v_offsets[color]
                          * (int)row_stride,
                    (int)row_stride, -1);
            }

            if (data_fd2_battle_summon_main_anim_odd_even_frame_toggle
                == 0) {
                color =
                    data_fd2_battle_summon_main_anim_12slot_color_idx_array[i];
                if (data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                        == 0 &&
                    spr_offsets[color] != 0) {
                    fd2_play_sfx_with_handle(
                        data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                        2, 1);
                }
                data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]++;

                if (data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                    == 3) {
                    if (spr_offsets[color] == 0)
                        fd2_play_sfx_sample_from_bank(
                            data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                            1, 1);
                    done_flag = 1;
                }

                if (data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                        == 0xB &&
                    data_fd2_battle_summon_main_anim_terminate_flag
                        == 0) {
                    data_fd2_battle_summon_main_anim_color_rotation_counter++;
                    data_fd2_battle_summon_main_anim_color_rotation_counter =
                        data_fd2_battle_summon_main_anim_color_rotation_counter
                        % 12;
                    data_fd2_battle_summon_main_anim_12slot_color_idx_array[i]
                        = (int32)data_fd2_battle_summon_main_anim_color_rotation_counter;
                    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                        = 0;
                }
            }
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_spell_setup_pre_animation_8slot @ 0x26152
 *
 * Stage-0 pre-animation 8-slot orbit setup. Dispatch table #0.
 * State 3=init stagger, 4=blit-if-visible, 5=advance+blit-if-hidden.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_spell_setup_pre_animation_8slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    uint8 vis[7];
    uint32 y_off[7];
    int32 row_mul[7];
    int i;
    int done_flag;
    uint8 *pChar;

    done_flag = 0;
    memcpy(vis, data_fd2_battle_summon_spell_8slot_visibility_table, 7);
    memcpy(y_off, data_fd2_battle_summon_spell_8slot_y_offset_table, 28);
    memcpy(row_mul, data_fd2_battle_summon_spell_8slot_row_multiplier_table, 28);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 7; i++)
            y_off[i] += 0x94;
    }

    if (state_code == 3) {
        for (i = 0; i < 8; i++)
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                = -2 * i;
        return 0x1C;
    }

    if (state_code == 4) {
        for (i = 0; i < 7; i++) {
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                == 3)
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                    1, 1);
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                    >= 0 &&
                data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                    < 0x10 &&
                vis[i] == 1) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i],
                    row_mul[i] * (int)row_stride
                        + (int)origin_y + (int)y_off[i],
                    (int)row_stride, -1);
            }
        }
        return 0;
    }

    if (state_code == 5) {
        for (i = 0; i < 7; i++) {
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                    >= 0 &&
                data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                    < 0x10 &&
                vis[i] == 0) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i],
                    row_mul[i] * (int)row_stride
                        + (int)origin_y + (int)y_off[i],
                    (int)row_stride, -1);
            }
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]++;
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                == 9)
                done_flag = 1;
        }
        return done_flag;
    }

    return 0;
}
