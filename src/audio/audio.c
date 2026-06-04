/*
 * audio.c — BGM / SFX management.
 */

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
