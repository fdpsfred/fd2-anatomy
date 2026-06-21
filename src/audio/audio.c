/*
 * audio.c — BGM / SFX management.
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_set_bgm_track_with_fade @ 0x25977
 *
 * BGM track manager. No-op if track_id already matches the cached
 * last-set id (data_fd2_audio_bgm_last_set_track_id); otherwise
 * caches the new id and acts:
 *   - track_id == 0xFFFFFFFF: stop with a 4s fade-out
 *     (AIL_set_sequence_volume vol=0, 4000ms) and return.
 *   - else, only if the MDI driver is present
 *     (data_fd2_audio_bgm_driver_available_flag != 0): stop any
 *     current sequence, load FDMUS.DAT[track_id] via
 *     fd2_load_dat_resource, DPMI-lock it, then init + start.
 *     Initial volume/fade:
 *       BGM disabled        -> vol 0,    fade 0   (muted)
 *       track 0x10 or 0x11  -> vol 0x7F, fade 0   (instant cues)
 *       other tracks        -> vol 0x7F, fade 2000ms (anchored
 *                              by a prior vol 0, fade 0)
 *     Finally set the AIL loop count to loop_count.
 *
 * loop_count is passed straight to AIL_set_sequence_loop_count
 * (0 = loop indefinitely per AIL convention).
 * ---------------------------------------------------------------- */
void fd2_set_bgm_track_with_fade(uint32 track_id,
                                  uint32 loop_count)
{
    if ((uint32)data_fd2_audio_bgm_last_set_track_id == track_id)
        return;

    data_fd2_audio_bgm_last_set_track_id = (uint8)track_id;

    if (track_id == 0xFFFFFFFF) {
        AIL_set_sequence_volume(
            data_fd2_audio_bgm_sequence_handle, 0, 4000);
        return;
    }

    if (data_fd2_audio_bgm_driver_available_flag == 0) return;

    if (data_fd2_audio_bgm_sequence_data_buf_ptr != 0) {
        AIL_stop_sequence(
            data_fd2_audio_bgm_sequence_handle);
    }

    data_fd2_audio_bgm_sequence_data_buf_ptr =
        (uint32)fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdmus_dat,
            data_fd2_audio_bgm_sequence_data_buf_ptr,
            track_id);
    fd2_dpmi_lock_size(
        data_fd2_audio_bgm_sequence_data_buf_ptr,
        data_fd2_resource_last_loaded_resource_size);
    AIL_init_sequence(
        data_fd2_audio_bgm_sequence_handle,
        data_fd2_audio_bgm_sequence_data_buf_ptr, 0);
    AIL_start_sequence(
        data_fd2_audio_bgm_sequence_handle);

    if (data_fd2_audio_bgm_enabled_flag == 0) {
        AIL_set_sequence_volume(
            data_fd2_audio_bgm_sequence_handle, 0, 0);
    } else if (track_id == 0x10 || track_id == 0x11) {
        AIL_set_sequence_volume(
            data_fd2_audio_bgm_sequence_handle, 0x7F, 0);
    } else {
        AIL_set_sequence_volume(
            data_fd2_audio_bgm_sequence_handle, 0, 0);
        AIL_set_sequence_volume(
            data_fd2_audio_bgm_sequence_handle,
            0x7F, 2000);
    }

    AIL_set_sequence_loop_count(
        data_fd2_audio_bgm_sequence_handle, loop_count);
}

/* ----------------------------------------------------------------
 * fd2_load_status_effect_sfx @ 0x1d4cb (6 callers)
 *
 * Load the status-effect / spell SFX sample bank (FDOTHER.DAT
 * entry 0x50) into data_fd2_audio_status_effect_sfx_handle_ptr.
 * fd2_play_sfx_with_handle plays from this base; the matching
 * fd2_play_and_free_status_effect_sfx releases it after the
 * animation finishes.
 *
 * Cdecl, void(void). The handle is cleared to 0 first, then set
 * to the loader's return value. The binary's __CHK(0x10)
 * stack-probe prologue is compiler-injected and not source.
 * ---------------------------------------------------------------- */
void fd2_load_status_effect_sfx(void)
{
    data_fd2_audio_status_effect_sfx_handle_ptr = 0;
    data_fd2_audio_status_effect_sfx_handle_ptr =
        (uint32)fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            0, 0x50);
}

/* ----------------------------------------------------------------
 * fd2_play_and_free_status_effect_sfx @ 0x1d4f6 (5 callers)
 *
 * Teardown counterpart to fd2_load_status_effect_sfx: stop the
 * status-effect SFX and release the bank buffer. Despite the name,
 * this does NOT start playback -- passing sfx_id == -1 routes
 * fd2_play_sfx_with_handle to its stop-only branch (it issues
 * AIL_stop_sample on the shared sample slot and returns before the
 * playback code). The actual SFX playback for this bank happens
 * earlier in the cast sequence via fd2_play_spell_palette_flash_with_sfx
 * (which passes sfx_id 0). After stopping, free() releases the bank
 * loaded into data_fd2_audio_status_effect_sfx_handle_ptr.
 *
 * Cdecl, void(void). The binary's __CHK(0x10) stack-probe prologue
 * is compiler-injected and not source. The final free() is emitted
 * by Watcom as a tail call (JMP into fd2_maybe_free_speed_mode_overlay's
 * shared `CALL free; ADD ESP,4; RET` epilogue); it is reproduced here
 * as a plain free() at function end.
 * ---------------------------------------------------------------- */
void fd2_play_and_free_status_effect_sfx(void)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0xFFFFFFFF, 1);
    free((void *)data_fd2_audio_status_effect_sfx_handle_ptr);
}

/* ----------------------------------------------------------------
 * fd2_play_sfx_with_handle @ 0x25a96 (55 callers)
 *
 * Generic AIL (Miles Sound System) one-shot SFX player. Used widely
 * across the codebase for menu beeps, attack hits, damage thuds and
 * UI confirmations.
 *
 * Cdecl, void(uint32 sfx_table_base, int sfx_id, int loop_count). The
 * binary's __CHK(0x1c) stack-probe prologue and the unused PUSH/POP EBX
 * register reservation are compiler-injected and not source.
 *
 * Three gates (silent return if any fails):
 *   data_fd2_audio_sfx_driver_available_flag (0x53EF1) != 0
 *   data_fd2_audio_sfx_enabled_flag          (0x51E62) != 0
 *   data_fd2_battle_scripted_cinematic_mode_or_terrain_idx (0x540FF) == 0
 *
 * Then it always stops the shared sample slot. sfx_id == -1 is the
 * stop-only path (return after the stop). Otherwise it resolves the
 * sample bank entry (entry = base + sfx_id*4): the dword at entry+6 is
 * the sample's byte offset from the bank base, the dword at entry+10 is
 * the sample's end offset; length = end - offset. It then (re)programs
 * the sample slot and starts playback.
 * ---------------------------------------------------------------- */
void fd2_play_sfx_with_handle(uint32 sfx_table_base, int sfx_id,
                              int loop_count)
{
    uint32 entry;
    uint32 sample_offset;
    uint32 sample_end;

    if (data_fd2_audio_sfx_driver_available_flag == 0) return;
    if (data_fd2_audio_sfx_enabled_flag == 0) return;
    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx != 0) return;

    AIL_stop_sample(data_fd2_audio_sfx_sample_handle_0);
    if (sfx_id == -1) return;

    entry = sfx_table_base + (uint32)sfx_id * 4;
    sample_offset = *(uint32 *)(entry + 6);
    sample_end = *(uint32 *)(entry + 10);

    AIL_init_sample(data_fd2_audio_sfx_sample_handle_0);
    AIL_set_sample_address(data_fd2_audio_sfx_sample_handle_0,
                           sfx_table_base + sample_offset,
                           sample_end - sample_offset);
    AIL_set_sample_loop_count(data_fd2_audio_sfx_sample_handle_0,
                              loop_count);
    AIL_start_sample(data_fd2_audio_sfx_sample_handle_0);
}

/* ----------------------------------------------------------------
 * fd2_play_sfx_sample_from_bank @ 0x25b45 (11 callers)
 *
 * One-shot AIL (Miles Sound System) SFX player on the second sample
 * slot (data_fd2_audio_sfx_sample_handle_1, 0x53EE8). Used by battle
 * SFX (enemy_turn_dispatcher, summon variants b/c/d/e/main/minor,
 * execute_summon_spell_cast), field SFX (tile_event_interaction,
 * recruitment_or_branch) and the ending cinematic.
 *
 * Structurally identical to fd2_play_sfx_with_handle except it drives
 * sample slot 1 rather than slot 0; the two slots let an SFX on this
 * channel play without cutting off one on the other.
 *
 * Cdecl, void(uint32 bank_ptr, uint32 sfx_id, uint32 loop_count). The
 * binary's __CHK(0x1c) stack-probe prologue and the unused PUSH/POP EBX
 * register reservation are compiler-injected and not source.
 *
 * Three gates (silent return if any fails):
 *   data_fd2_audio_sfx_driver_available_flag (0x53EF1) != 0
 *   data_fd2_audio_sfx_enabled_flag          (0x51E62) != 0
 *   data_fd2_battle_scripted_cinematic_mode_or_terrain_idx (0x540FF) == 0
 *
 * Then it always stops sample slot 1. sfx_id == -1 is the stop-only
 * path (return after the stop). Otherwise it resolves the sample bank
 * entry (entry = bank_ptr + sfx_id*4): the dword at entry+6 is the
 * sample's byte offset from the bank base, the dword at entry+10 is the
 * sample's end offset; length = end - offset. It then (re)programs the
 * sample slot and starts playback.
 * ---------------------------------------------------------------- */
void fd2_play_sfx_sample_from_bank(uint32 bank_ptr, uint32 sfx_id,
                                   uint32 loop_count)
{
    uint32 entry;
    uint32 sample_offset;
    uint32 sample_end;

    if (data_fd2_audio_sfx_driver_available_flag == 0) return;
    if (data_fd2_audio_sfx_enabled_flag == 0) return;
    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx != 0) return;

    AIL_stop_sample(data_fd2_audio_sfx_sample_handle_1);
    if (sfx_id == 0xFFFFFFFF) return;

    entry = bank_ptr + sfx_id * 4;
    sample_offset = *(uint32 *)(entry + 6);
    sample_end = *(uint32 *)(entry + 10);

    AIL_init_sample(data_fd2_audio_sfx_sample_handle_1);
    AIL_set_sample_address(data_fd2_audio_sfx_sample_handle_1,
                           bank_ptr + sample_offset,
                           sample_end - sample_offset);
    AIL_set_sample_loop_count(data_fd2_audio_sfx_sample_handle_1,
                              loop_count);
    AIL_start_sample(data_fd2_audio_sfx_sample_handle_1);
}

/* ----------------------------------------------------------------
 * fd2_load_figani_sfx_bank @ 0x2bc9a (3 callers)
 *
 * Translate a FIGANI header's SFX-bank reference byte (figani_data[+4])
 * into an FDOTHER.DAT entry index, then load that entry as the
 * animation's SFX bank. Returns the loaded buffer pointer, or 0 when
 * figani_data is NULL or has no SFX-bank reference (byte == 0).
 *
 * Translation table: the binary copies the 6-byte
 * data_fd2_audio_figani_sfx_bank_fdother_index_lut (0x525D6) onto the
 * stack (MOVSD+MOVSW into an 8-byte local) and indexes it 1-based:
 *   fdother_idx = lut[sfx_id_byte - 1]
 * Reproduced verbatim here via an 8-byte local + memcpy of 6 bytes so
 * the bounded-buffer / 1-based-index behaviour is preserved exactly.
 *
 * Cdecl, uint32(uint32). The binary's __CHK(0x20) stack-probe prologue
 * and the unused PUSH/POP ESI/EDI register reservation are
 * compiler-injected and not source. The return pointer is stored by
 * each caller into data_fd2_audio_figani_sfx_bank_buf_ptr (0x54117)
 * and freed by the caller after the cinematic.
 * ---------------------------------------------------------------- */
uint32 fd2_load_figani_sfx_bank(uint32 figani_data)
{
    uint8  idx_lut[8];
    uint8  sfx_id_byte;
    uint32 sfx_bank;

    memcpy(idx_lut,
           data_fd2_audio_figani_sfx_bank_fdother_index_lut, 6);

    sfx_bank = 0;
    if (figani_data != 0 && *(uint8 *)(figani_data + 4) != 0) {
        sfx_id_byte = *(uint8 *)(figani_data + 4);
        sfx_bank = (uint32)fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            0,
            (uint32)idx_lut[sfx_id_byte - 1]);
    }
    return sfx_bank;
}

/* ----------------------------------------------------------------
 * Audio module data
 * ---------------------------------------------------------------- */

/* data_fd2_audio_bgm_last_set_track_id @ 0x51A11
 *
 * Cache of the most recently requested BGM track id, used by
 * fd2_set_bgm_track_with_fade to skip reloading a track that is
 * already playing. Read via MOVZX byte and written via MOV AL
 * (unsigned 8-bit). Static initial value 0xFF marks "no track set
 * yet" (the stop sentinel, since 0xFFFFFFFF requests fade-out). */
uint8 data_fd2_audio_bgm_last_set_track_id = 0xFF;

/* data_fd2_audio_status_effect_sfx_handle_ptr @ 0x53B13
 *
 * Runtime-loaded pointer to the status-effect / spell SFX sample bank
 * (an FDOTHER.DAT entry buffer). Zero in the image; first use is a
 * write. fd2_load_status_effect_sfx and fd2_chapter_25_init clear it to
 * 0 then store the fd2_load_dat_resource() return pointer; many spell /
 * battle SFX readers pass it as the sample-bank base to
 * fd2_play_sfx_with_handle, and fd2_play_and_free_status_effect_sfx
 * free()s it. Accessed as a single 32-bit value (MOV dword ptr). */
uint32 data_fd2_audio_status_effect_sfx_handle_ptr;

/* data_fd2_audio_bgm_sequence_data_buf_ptr @ 0x53EE0
 *
 * Runtime-loaded pointer to the current BGM sequence (an FDMUS.DAT
 * entry buffer) used by fd2_set_bgm_track_with_fade. Zero in the image;
 * the first access is a NULL test (skip AIL_stop_sequence on the very
 * first track), then it is overwritten by the fd2_load_dat_resource()
 * return pointer and passed to fd2_dpmi_lock_size / AIL_init_sequence.
 * Accessed as a single 32-bit value (CMP/PUSH/MOV dword ptr [0x53EE0]). */
uint32 data_fd2_audio_bgm_sequence_data_buf_ptr;
