/*
 * audio.c — BGM / SFX management.
 */

#include <stdlib.h>

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_set_bgm_track_with_fade @ 0x25977
 *
 * BGM track manager. Stop (0xFF), change track with fade, or
 * no-op if already playing.
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
            (uint32)data_fd2_string_fdmus_dat,
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
 * Companion to fd2_load_status_effect_sfx: play the loaded
 * status-effect SFX bank in kill-all mode (handle == -1 tells
 * fd2_play_sfx_with_handle to stop every currently-playing sample
 * first), then release the bank buffer.
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
