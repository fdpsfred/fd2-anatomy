/*
 * aniui.c — UI animations: money / tutorial / shop-scroll / party-add / misc
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_tick_tutorial_progress_with_sfx @ 0x2C9EC
 *
 * Per-step tick + footstep SFX dispatcher.
 * Selects cadence divisor and SFX based on char job/immunity.
 * Plays SFX when counter aligns, increments counter.
 * ---------------------------------------------------------------- */
void fd2_tick_tutorial_progress_with_sfx(uint32 char_idx)
{
    uint8 job_tbl[32];
    int divisor;
    int sfx_id;
    uint8 job_mod;
    uint8 *pChar;

    memcpy(job_tbl,
           data_fd2_audio_footstep_sfx_per_job_cadence_class_table, 29);

    if (fd2_check_char_status_immunity(char_idx) != 0) {
        divisor = 6;
        sfx_id = 10;
    } else {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + char_idx * RUNTIME_CHAR_SIZE;
        job_mod = job_tbl[pChar[0x20] - 1];
        if (job_mod == 0) {
            divisor = 6;
            sfx_id = 9;
        } else if (job_mod == 1) {
            divisor = 4;
            sfx_id = 9;
        } else {
            divisor = 9;
            sfx_id = 11;
        }
    }

    if ((uint32)data_fd2_audio_walk_step_sfx_cadence_counter %
        (uint32)divisor == 0) {
        fd2_play_sfx_with_handle(
            data_fd2_audio_fdother_sfx_bank_buf_ptr, sfx_id, 1);
    }
    data_fd2_audio_walk_step_sfx_cadence_counter++;
}