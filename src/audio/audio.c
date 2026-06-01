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
