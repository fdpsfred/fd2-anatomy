/*
 * anisummn.c — summon-spell animation tick state machines
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
#include <math.h>

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
 * Per-slot frame counter array for the variant-D summon animation
 * @ 0x54097
 *
 * Four 32-bit signed entries (int[4]), one per sprite slot. The INIT
 * state (state_code 0) seeds each slot with a staggered negative value
 * (frame_counter[i] = -3*i, i.e. 0,-3,-6,-9) before any read this run,
 * so it is plain zero-bss runtime scratch. Treated signed: the tick
 * blit phase tests 0 <= frame_counter[i] < 5 with a signed compare
 * (CMP dword ptr [EAX + 0x54097],0x0 / JL at 0x27434), and the value
 * may be negative while staggered slots ramp up. All access is
 * dword-wide and 4-byte strided (init MOV dword ptr [EBX*4 +
 * 0x54097],EAX at 0x2731F; INC at 0x273C9; tests/wrap at 0x273D0/
 * 0x273E4/0x2741B/0x2743D/0x2745B/0x27478). Init seeds 4 slots; the
 * tick state advances only the first 3. Used only by
 * fd2_tick_summon_anim_variant_d_3slot.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[4];

/* ----------------------------------------------------------------
 * Per-slot color index array for the variant-D summon animation
 * @ 0x540A7
 *
 * Four 32-bit entries (int[4]), one per sprite slot, holding the
 * current per-color row-offset index (0..9). The INIT state
 * (state_code 0) seeds each slot with color_idx[i] = i (i = 0..3)
 * before any read this run, so it is plain zero-bss runtime scratch.
 * All access is dword-wide and 4-byte strided: init store
 * MOV dword ptr [EBX*4 + 0x540A7],EBX at 0x27326; rotation store
 * MOV dword ptr [ECX + 0x540A7],EAX at 0x27415; read
 * MOV EDX,dword ptr [EAX + 0x540A7] at 0x2744C (used as an index into
 * the per-color row-offset table). Values written are always in 0..9
 * (the slot index at init, then color_rotation_counter % 10), so the
 * sign of the element is immaterial. Init seeds 4 slots; the tick
 * state advances only the first 3. Used only by
 * fd2_tick_summon_anim_variant_d_3slot.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[4];

/* ----------------------------------------------------------------
 * Color rotation counter for the variant-D summon animation
 * @ 0x540B7
 *
 * Single unsigned byte holding a mod-10 counter that drives the
 * per-color row-offset cycling. The INIT state (state_code 0) writes
 * it before any read this run (MOV byte ptr [0x540B7],BL at 0x27333,
 * where BL = 4, the loop counter left over from seeding the 4 slots),
 * so it is plain zero-bss runtime scratch; the on-disk image is 0x00.
 * All access is byte-wide and unsigned: increment INC byte ptr
 * [0x540B7] at 0x273F8; read-back MOVZX EDX,byte ptr [0x540B7] at
 * 0x273FE feeding an IDIV by 10; wrapped store MOV byte ptr
 * [0x540B7],DL at 0x2740C. The stored value stays in 0..9 (counter
 * % 10) and is also copied into the active slot's color index. Used
 * only by fd2_tick_summon_anim_variant_d_3slot.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_d_color_rotation_counter;

/* ----------------------------------------------------------------
 * Terminate flag for the variant-D summon animation
 * @ 0x540B8
 *
 * Single byte latch that suppresses color-rotation once the spell's
 * hold phase begins. The INIT state (state_code 0) clears it before
 * any read this run (MOV byte ptr [0x540B8],0x0 at 0x27339), so it is
 * plain zero-bss runtime scratch; the on-disk image is 0x00. The
 * state-6 hold path sets it (MOV byte ptr [0x540B8],0x1 at 0x27365).
 * It is read byte-wide in the tick path (MOVZX EAX,byte ptr [0x540B8]
 * at 0x273ED) as a frame-7 guard: when nonzero, the per-color rotation
 * step is skipped. Used only by fd2_tick_summon_anim_variant_d_3slot.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_d_terminate_flag;

/* ----------------------------------------------------------------
 * Odd/even frame toggle for the variant-D summon animation
 * @ 0x540B9
 *
 * Single byte mod-2 toggle that gates the even-frame state updates
 * (frame advance, SFX, color rotation). The INIT state (state_code 0)
 * clears it before any read this run (MOV byte ptr [0x540B9],0x0 at
 * 0x27340), so it is plain zero-bss runtime scratch; the on-disk image
 * is 0x00. Each tick path (state_code 2/5/8) does INC byte ptr
 * [0x540B9] at 0x27389, then reads it back MOVZX EDX,byte ptr [0x540B9]
 * at 0x2738F feeding an IDIV by 2 and writes the remainder MOV byte ptr
 * [0x540B9],DL at 0x273A2, so the value stays 0..1. It is then read
 * byte-wide MOVZX EAX,byte ptr [0x540B9] at 0x2746D as the even-frame
 * gate: when zero, the per-slot frame advance / SFX / color-rotation
 * block runs. All access is byte-wide and unsigned. Used only by
 * fd2_tick_summon_anim_variant_d_3slot.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle;

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
 * Per-slot frame counter array for the variant-E summon animation
 * @ 0x540BA
 *
 * Sixteen 32-bit signed entries (int[16]), one per sprite slot. The
 * INIT state (state_code 0) seeds each slot with a staggered negative
 * value (frame_counter[i] = -2*i, i.e. 0,-2,-4,..,-30) before any read
 * this run, so it is plain zero-bss runtime scratch; the on-disk image
 * is all 0x00. Treated signed: the tick blit phase tests
 * 0 <= frame_counter[i] < 8 with a signed compare (CMP dword ptr
 * [EAX + 0x540BA],0x0 / JL at 0x2753E), and the value may be negative
 * while staggered slots ramp up. All access is dword-wide and 4-byte
 * strided (init store MOV dword ptr [EBX*4 + 0x540BA],EDX at 0x274E8;
 * INC dword ptr [EBX*4 + 0x540BA] at 0x275A9; tests at 0x2753E/
 * 0x27547/0x27571/0x2758D/0x275B0). Used only by
 * fd2_tick_summon_anim_variant_e_16slot.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[16];

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
 * Color rotation counter for the variant-A 6-slot summon animation
 * @ 0x5401A
 *
 * Single unsigned byte holding a mod-10 counter that drives the
 * per-slot color index. The INIT state (state_code 0) seeds it to 6
 * before any read this run, so it is plain zero-bss runtime scratch;
 * the on-disk image is 0x00. During a frame-8 color rotation it is
 * incremented and reduced mod 10, then copied into the active slot's
 * color index. All access is byte-wide and unsigned (zero-extending
 * read). Used only by fd2_tick_summon_anim_variant_a_6slot.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_a_color_rotation_counter;

/* ----------------------------------------------------------------
 * Terminate flag for the variant-A 6-slot summon animation @ 0x5401B
 *
 * Single byte latch that suppresses the frame-8 color rotation once
 * the animation is told to wind down. The INIT state (state_code 0)
 * clears it before any read this run, so it is plain zero-bss runtime
 * scratch; the on-disk image is 0x00. The state-6 path sets it to 1.
 * During a tick frame it is read byte-wide to decide whether to keep
 * rotating colors. Used only by fd2_tick_summon_anim_variant_a_6slot.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_a_terminate_flag;

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
 * Color-rotation counter for the variant-B 6-slot summon animation
 * @ 0x54052
 *
 * Single byte mod-10 counter that drives the per-slot color index
 * during TICK frames, used by fd2_tick_summon_anim_variant_b_6slot.
 * On INIT (state_code 0) it is seeded to 6 (MOV [0x54052],AL at
 * 0x26CAC, AL holds the loop counter value 6). During a frame-7 color
 * rotation it is incremented and reduced mod 10 (INC byte ptr
 * [0x54052] at 0x26D53; MOVZX EDX,byte ptr [0x54052] at 0x26D59 then
 * IDIV by 10; MOV byte ptr [0x54052],DL at 0x26D6C). All access is
 * byte-wide with a zero-extending read (MOVZX): element type is uint8.
 * Seeded at INIT before any read each animation run, so it is plain
 * zero-bss runtime state.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_b_color_rotation_counter;

/* ----------------------------------------------------------------
 * Terminate flag for the variant-B 6-slot summon animation
 * @ 0x54053
 *
 * Single byte boolean gate that suppresses the frame-7 color rotation
 * once the animation is told to wind down, used by
 * fd2_tick_summon_anim_variant_b_6slot. On INIT (state_code 0) it is
 * cleared to 0 (MOV byte ptr [0x54053],0x0 at 0x26CB1). On state_code 6
 * it is set to 1 (MOV byte ptr [0x54053],0x1 at 0x26CD6). During a TICK
 * frame it is read to decide whether to keep rotating colors (MOVZX
 * EAX,byte ptr [0x54053] at 0x26D48, then TEST/JNZ). All access is
 * byte-wide with a zero-extending read (MOVZX): element type is uint8.
 * Cleared at INIT before any read each animation run, so it is plain
 * zero-bss runtime state.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_b_terminate_flag;

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
 * Animation phase byte for the generic single-target summon animation
 * @ 0x53F7E
 *
 * Single byte holding the current sprite frame index / phase of the
 * generic summon animation. The INIT state (state_code 0) clears it
 * before any read this run, so it is plain zero-bss runtime scratch;
 * the on-disk image is 0x00. Seeded/advanced across states (set to
 * 0x10 in state 3, 0xA in state 6, ramped 0x10..0x12 in state 5) and
 * passed by address to fd2_tick_sprite_animation_step as the frame
 * index. All access is byte-wide and unsigned. Used only by
 * fd2_tick_summon_spell_animation_state.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_spell_anim_phase_byte;

/* ----------------------------------------------------------------
 * Auxiliary state byte (write-only) for the generic single-target
 * summon animation @ 0x53F7F
 *
 * Single byte cleared to 0 by the INIT state (state_code 0) and never
 * read back by the animation logic, so it is plain zero-bss runtime
 * scratch; the on-disk image is 0x00. Retained as a distinct named
 * symbol so the INIT store keeps its original target address. Used
 * only by fd2_tick_summon_spell_animation_state.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_spell_anim_aux_state_byte_unread;

/* ----------------------------------------------------------------
 * Sprite animation tick counter for the generic single-target summon
 * animation @ 0x53F80
 *
 * Single unsigned byte hold-counter paired with the phase byte. The
 * INIT state (state_code 0) clears it before any read this run, so it
 * is plain zero-bss runtime scratch; the on-disk image is 0x00. Passed
 * by address to fd2_tick_sprite_animation_step, which increments it
 * each tick and resets it to 0 (advancing the phase) when it reaches
 * the per-frame hold value. All access is byte-wide and unsigned.
 * Used only by fd2_tick_summon_spell_animation_state.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_spell_sprite_anim_tick_counter;

/* ----------------------------------------------------------------
 * fd2_tick_summon_spell_animation_state @ 0x26528
 *
 * Generic single-target summon animation tick state machine.
 * Dispatch table entry #2 (at 0x523C1). is_enemy = caster's
 * runtime_char.bTeam == 0; it gates which states drive the
 * frame-paced sprite tick vs. a direct blit.
 *
 * Per state_code (phase byte = current sprite frame index):
 *   0     : reset phase/aux/tick to 0; return 0x1D.
 *   3     : phase = 0x10; return 0xC.
 *   6     : SFX chime; phase = 0xA; return 0xA.
 *   1 / 7 : ally only -- on entry (state 1 && phase 0xA) snap
 *           phase to 0xF, then advance the paced sprite tick.
 *   2 / 8 : SFX click when phase == 7; enemy only runs the paced
 *           tick (with the same 0xA->0xF entry snap on state 2);
 *           when phase reaches 0x10 blit frame 0x10.
 *   4     : ally only -- blit frame 0xF one row above origin.
 *   5     : enemy adds a frame-0xF blit one row higher; always
 *           blit phase one row above origin, then phase++. At 0x11
 *           SFX + return 1 (done); at 0x12 wrap phase back to 0x10.
 *   other : return 0.
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
 * Per-slot frame counter array for the main 12-slot summon animation
 * @ 0x53F81
 *
 * Twelve 32-bit signed entries (int[12]), one per sprite slot. The
 * INIT state (state_code 0) seeds each slot with a staggered negative
 * value (frame_counter[i] = -2*i, i.e. 0,-2,..,-22) before any read
 * this run, so it is plain zero-bss runtime scratch; the on-disk image
 * is all 0x00. Treated signed: the tick blit phase tests
 * 0 <= frame_counter[i] < 0xB with a signed compare, and the value may
 * be negative while staggered slots ramp up. All access is dword-wide
 * and 4-byte strided. Used only by
 * fd2_tick_summon_spell_main_animation_state.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_main_anim_12slot_frame_counter_array[12];

/* ----------------------------------------------------------------
 * Per-slot color index array for the main 12-slot summon animation
 * @ 0x53FB1
 *
 * Twelve 32-bit entries (int[12]), one per sprite slot, holding the
 * current per-color row-offset index (0..11). The INIT state
 * (state_code 0) seeds each slot with color_idx[i] = i before any read
 * this run, so it is plain zero-bss runtime scratch; the on-disk image
 * is all 0x00. Values written stay in 0..11 (slot index at init, then
 * color_rotation_counter % 12). All access is dword-wide and 4-byte
 * strided. Used only by fd2_tick_summon_spell_main_animation_state.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_main_anim_12slot_color_idx_array[12];

/* ----------------------------------------------------------------
 * Color rotation counter for the main 12-slot summon animation
 * @ 0x53FE1
 *
 * Single unsigned byte holding a mod-12 counter that drives the
 * per-color row-offset cycling. The INIT state (state_code 0) seeds it
 * to 12 before any read this run, so it is plain zero-bss runtime
 * scratch; the on-disk image is 0x00. During a frame-0xB color
 * rotation it is incremented and reduced mod 12, then copied into the
 * active slot's color index. All access is byte-wide and unsigned
 * (zero-extending read). Used only by
 * fd2_tick_summon_spell_main_animation_state.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_main_anim_color_rotation_counter;

/* ----------------------------------------------------------------
 * Terminate flag for the main 12-slot summon animation @ 0x53FE2
 *
 * Single byte latch that suppresses color rotation once the spell's
 * hold phase begins. The INIT state (state_code 0) clears it before
 * any read this run, so it is plain zero-bss runtime scratch; the
 * on-disk image is 0x00. The state-6 hold path sets it to 1. It is
 * read byte-wide in the tick path as a guard: when nonzero, the
 * per-color rotation step is skipped. Used only by
 * fd2_tick_summon_spell_main_animation_state.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_main_anim_terminate_flag;

/* ----------------------------------------------------------------
 * Odd/even frame toggle for the main 12-slot summon animation
 * @ 0x53FE3
 *
 * Single byte mod-2 toggle that gates the even-frame state updates
 * (frame advance, SFX, color rotation). The INIT state (state_code 0)
 * clears it before any read this run, so it is plain zero-bss runtime
 * scratch; the on-disk image is 0x00. Each tick path flips it
 * (value % 2) then reads it byte-wide as the even-frame gate: when
 * zero, the per-slot frame advance / SFX / color-rotation block runs.
 * All access is byte-wide and unsigned. Used only by
 * fd2_tick_summon_spell_main_animation_state.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_main_anim_odd_even_frame_toggle;

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
 * Shared per-slot frame counter array for the summon-spell sprite
 * orbit @ 0x53F42
 *
 * Fifteen 32-bit signed entries (int[15]), one per orbit slot. Plain
 * zero-bss runtime scratch; the on-disk image is all 0x00. The
 * pre-animation setup (state_code 3) seeds the first 8 slots with a
 * staggered negative value (frame_counter[i] = -2*i) before any read
 * this run; the state-5 advance walks the first 7. Treated signed: the
 * blit phases test 0 <= frame_counter[i] < 0x10 with a signed compare,
 * and the value may be negative while staggered slots ramp up. All
 * access is dword-wide and 4-byte strided. Shared with the scene
 * renderer (fd2_render_summon_aura_sprite_ring in src/gfx/rndscene.c),
 * but seeded/owned here by fd2_tick_summon_spell_setup_pre_animation_8slot.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[15];

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

/* ----------------------------------------------------------------
 * Per-slot computed x-coordinate array for the variant-C 5-slot
 * radial summon animation @ 0x54054
 *
 * Five 32-bit signed entries (int[5]), one per ring slot. Each frame
 * the radial-blit phase writes x_coord[i] = round(sweep +
 * angle_accumulator * cos(angle_rad)) before any read this run, so it
 * is plain zero-bss runtime scratch. All access is dword-wide and
 * 4-byte strided (MOV dword ptr [EBX*4 + 0x54054],EAX at 0x26FFE;
 * reads at 0x2709B/0x270DC/0x27180/0x2724C), used only by
 * fd2_tick_summon_anim_variant_c_5slot_radial.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[5];

/* ----------------------------------------------------------------
 * Per-slot computed y-coordinate array for the variant-C 5-slot
 * radial summon animation @ 0x54068
 *
 * Five 32-bit signed entries (int[5]), one per ring slot. Each frame
 * the radial-blit phase writes y_coord[i] = round(angle_accumulator *
 * sin(angle_rad) * 1.2 + 30.0) before any read this run, so it is
 * plain zero-bss runtime scratch. Values may be negative (sin half
 * the ring) and are used signed: multiplied by row_stride and added
 * to origin_y. All access is dword-wide and 4-byte strided (MOV dword
 * ptr [EBX*4 + 0x54068],EAX at 0x2704C; reads at
 * 0x2708D/0x270CE/0x27172/0x2723B), used only by
 * fd2_tick_summon_anim_variant_c_5slot_radial.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[5];

/* ----------------------------------------------------------------
 * Per-slot frame counter array for the variant-C 5-slot radial
 * summon animation @ 0x5407C
 *
 * Five 32-bit signed entries (int[5]), one per ring slot. The state-3
 * transition seeds each slot with a staggered negative value
 * (frame_counter[i] = -i, i.e. 0,-1,-2,-3,-4) before any read this
 * run, so it is plain zero-bss runtime scratch. Treated signed: the
 * advance phase tests frame_counter[i] < 0 with a signed compare (CMP
 * dword ptr [EAX + 0x5407C],0x0 / JL at 0x271A4) and wraps at 5 back
 * to 0. One low byte of an entry is also reused as a small sprite-id
 * (MOVZX from byte ptr [EAX + 0x5407C] at 0x271B1). All updates are
 * dword-wide and 4-byte strided (MOV dword ptr [EBX*4 + 0x5407C],ESI
 * at 0x26EFF; INC at 0x271FB; wraps/tests at 0x27201/0x2720A/0x27214/
 * 0x2727F), used only by fd2_tick_summon_anim_variant_c_5slot_radial.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[5];

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_c_5slot_radial @ 0x26E39  (0 callers)
 *
 * Variant-C 5-slot summon animation that places sprites at radial
 * positions computed via sin/cos. Dispatch table entry #6 at 0x523B9;
 * no direct CALL xrefs (invoked indirectly via that table).
 *
 * Per-slot angle = i * 0x48 (degrees) * pi/180 (single-precision float
 * 0x3C8EFA2D), promoted to double for cos/sin.
 *   x_coord[i] = round(sweep + angle_accumulator * cos(angle_rad))
 *   y_coord[i] = round(angle_accumulator * sin(angle_rad) * 1.2 + 30.0)
 * sweep = 0x1E (player/ally) or 0x5A (enemy, with first-3 offsets negated).
 *
 * The local copies (offs[], byte_offs[]) come from the rodata tables at
 * 0x524F8 / 0x5250C and are used only as the state-4/5 per-frame sprite
 * displacement, indexed by the current frame value.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_c_5slot_radial(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    union { uint32 u; float f; } pi180;
    int32 offs[5];
    uint8 byte_offs[5];
    int i;
    int done_flag;
    uint8 sweep;
    uint8 team;
    double angle_rad;
    uint32 acc;

    pi180.u = 0x3c8efa2d;       /* (float) pi/180, exact binary constant */
    done_flag = 0;
    memcpy(offs, data_fd2_animation_summon_variant_c_radial_5slot_offsets, 20);
    memcpy(byte_offs,
           data_fd2_animation_summon_variant_c_radial_5slot_byte_offsets, 5);
    sweep = 0x1e;

    team = ((uint8 *)data_fd2_battle_runtime_char_array_ptr
            + caster_unit_id * RUNTIME_CHAR_SIZE)[6];
    if (team == 0) {
        sweep = 0x5a;
        for (i = 0; i < 3; i++) {
            offs[i] = -offs[i];
            byte_offs[i] = 0;
        }
    }

    if (state_code == 0) {
        fd2_play_sfx_with_handle(
            data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 2, 1);
        data_fd2_battle_summon_anim_variant_c_angle_accumulator = 0;
        data_fd2_battle_summon_anim_variant_c_swap_done_latch = 0;
        return 7;
    }

    if (state_code == 3) {
        if (data_fd2_battle_summon_anim_variant_c_swap_done_latch == 0) {
            int x2;
            x2 = data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[2];
            for (i = 0; i < 5; i++) {
                data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[i]
                    = -i;
                data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[i]
                    = 0;
            }
            data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[2] =
                data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[4];
            data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[4] = x2;
            data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[2] =
                data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[4];
            data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[4] =
                (int32)data_fd2_battle_summon_anim_variant_c_angle_accumulator;
            data_fd2_battle_summon_anim_variant_c_swap_done_latch = 1;
        }
        return 0xc;
    }

    if (state_code == 6) {
        fd2_play_sfx_with_handle(
            data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 3, 1);
        data_fd2_battle_summon_anim_variant_c_angle_accumulator = 0x2a;
        return 7;
    }

    if (state_code == 1 || state_code == 2 ||
        state_code == 7 || state_code == 8) {
        for (i = 0; i < 5; i++) {
            int coord;

            angle_rad = (double)((float)(i * 0x48) * pi180.f);
            data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[i] =
                (int)((double)(int)sweep
                      + (double)data_fd2_battle_summon_anim_variant_c_angle_accumulator
                        * cos(angle_rad));
            data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[i] =
                (int)((double)data_fd2_battle_summon_anim_variant_c_angle_accumulator
                      * sin(angle_rad)
                      * data_fd2_animation_summon_radial_angle_step_12
                      + data_fd2_animation_summon_radial_radius_30);

            if (team == 0) {
                if (state_code == 2 || state_code == 8) {
                    coord = data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[i]
                            * (int)row_stride + (int)origin_y
                            + data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[i];
                    fd2_blit_indexed_sprite(
                        sprite_handle, 4, coord, (int)row_stride, -1);
                }
            } else if ((((state_code == 1) || (state_code == 7)) && i < 2) ||
                       (((state_code == 2) || (state_code == 8)) && i > 1)) {
                coord = data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[i]
                        + data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[i]
                          * (int)row_stride + (int)origin_y;
                fd2_blit_indexed_sprite(
                    sprite_handle, 4, coord, (int)row_stride, -1);
            }
        }

        if (state_code == 2)
            data_fd2_battle_summon_anim_variant_c_angle_accumulator += 6;
        else if (state_code == 8)
            data_fd2_battle_summon_anim_variant_c_angle_accumulator -= 6;

        return 0;
    }

    if (state_code == 4 || state_code == 5) {
        for (i = 0; i < 5; i++) {
            if (data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[i]
                < 0) {
                data_fd2_battle_summon_anim_variant_c_angle_accumulator = 4;
            } else {
                data_fd2_battle_summon_anim_variant_c_angle_accumulator =
                    (uint8)data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[i];
            }
            if (data_fd2_battle_summon_anim_variant_c_angle_accumulator == 1)
                data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[i]
                    = 5;

            if (team == 0) {
                if (state_code != 5)
                    continue;
            } else if (!(((state_code == 4) && i < 2) ||
                         ((state_code == 5) && i > 1))) {
                continue;
            }

            acc = (uint32)data_fd2_battle_summon_anim_variant_c_angle_accumulator;
            fd2_blit_indexed_sprite(
                sprite_handle, acc,
                ((int)(uint32)byte_offs[acc]
                 + data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[i])
                    * (int)row_stride + (int)origin_y
                + data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[i]
                + offs[acc],
                (int)row_stride, -1);
        }

        if (state_code == 5) {
            for (i = 0; i < 5; i++) {
                if (data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[i]
                    != 0) {
                    fd2_blit_indexed_sprite(
                        sprite_handle,
                        (uint32)data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[i],
                        (data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[i]
                         + (data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[i]
                            - 0x14) * (int)row_stride + (int)origin_y) - 0x3c,
                        (int)row_stride, -1);
                    data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[i]++;
                    if (data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[i]
                        == 10)
                        data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[i]
                            = 0;
                }

                if (data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[i]
                    == 0) {
                    if (i == 0 || i == 2)
                        fd2_play_sfx_with_handle(
                            data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                            1, 1);
                    else if (i != 5)
                        fd2_play_sfx_sample_from_bank(
                            data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                            1, 1);
                }

                data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[i]++;
                if (data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[i]
                    == 5)
                    data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[i]
                        = 0;
                if (data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[i]
                    == 2)
                    done_flag = 1;
            }
        }

        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array @ 0x54090
 *
 * Per-slot transient blit counter (5 slots), runtime state for the
 * variant-C 5-slot radial summon animation. Zero-initialized (BSS):
 * the host tick fd2_tick_summon_anim_variant_c_5slot_radial clears it
 * to 0 in the state-3 setup loop, seeds a slot to 5 in state 4/5 when
 * the scratch sprite-id is 1, then increments per advance and wraps
 * back to 0 at 10. Accessed only as byte[5] indexed 0..4 (stride 1).
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[5];

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_c_angle_accumulator @ 0x54095
 *
 * Ring-radius accumulator for the variant-C 5-slot radial summon
 * animation, runtime state. Zero-initialized (BSS): the host tick
 * fd2_tick_summon_anim_variant_c_5slot_radial writes it to 0 in the
 * state-0 INIT phase before any read, then ramps it +6 (state 2,
 * open ring) / -6 (state 8, close ring), and resets it to 0x2A in
 * state 6. In state 4/5 it is reused as a scratch sprite-id. A single
 * unsigned byte: every access is byte-width and every read uses MOVZX
 * (zero-extend), so it is an unsigned 8-bit scalar.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_c_angle_accumulator;

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_c_swap_done_latch @ 0x54096
 *
 * One-shot latch guarding the state-3 transition of the variant-C
 * 5-slot radial summon animation, runtime state. Zero-initialized
 * (BSS): the host tick fd2_tick_summon_anim_variant_c_5slot_radial
 * clears it to 0 in the state-0 INIT phase (MOV byte ptr
 * [0x54096],0x0 at 0x26ED6) before any read. State 3 reads it (MOVZX
 * from byte ptr [0x54096] at 0x26EEC); the first time it is 0 the
 * setup runs once (swap x slots 2/4, seed frame/blit counters) and
 * then it is set to 1 (MOV byte ptr [0x54096],0x1 at 0x26F3F) so the
 * setup never repeats. A single unsigned byte: every access is
 * byte-width and the read uses MOVZX (zero-extend), so it is an
 * unsigned 8-bit boolean flag.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_anim_variant_c_swap_done_latch;

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_minor_anim_state5_frame_counter @ 0x540FA
 *
 * State-5 (RAMP phase) frame counter of the summon-spell single-
 * sprite minor animation, runtime state. Zero-initialized (BSS): the
 * host tick fd2_tick_summon_spell_minor_animation_state seeds it to 1
 * in the state-0 INIT phase (MOV byte ptr [0x540FA],0x1 at 0x275F0)
 * before any read. During state 5 it drives the ramp 1..0x2C: the
 * sprite-id is frame/2 (slow ramp 0..0x16), frame==6 triggers one SFX
 * and frame==0x24 another, then it is INC'd per tick (INC byte ptr
 * [0x540FA] at 0x276CC) and the range 0x10 < frame < 0x2C returns the
 * "in-flight" signal 1. A single unsigned byte: every access is
 * byte-width and every read uses MOVZX (zero-extend), so it is an
 * unsigned 8-bit scalar.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_minor_anim_alternating_blit_toggle @ 0x540FB
 *
 * Alternating-blit toggle of the summon-spell single-sprite minor
 * animation, runtime state. Zero-initialized (BSS): the host tick
 * fd2_tick_summon_spell_minor_animation_state clears it to 0 in the
 * state-0 INIT phase (MOV byte ptr [0x540FB],0x0 at 0x275E9) before
 * any read. In the alternating-blit phase (states 1/7) it is read
 * (CMP byte ptr [0x540FB],0x0 at 0x27617; MOVZX from byte ptr
 * [0x540FB] at 0x2762A) -- when 0 the sprite is blitted -- then it is
 * XOR-toggled (XOR byte ptr [0x540FB],0x1 at 0x2763E) so the sprite
 * blits only every other tick. A single unsigned byte: every access
 * is byte-width and every read uses MOVZX (zero-extend), so it is an
 * unsigned 8-bit boolean flag.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
