#ifndef AUDIOFIX_H
#define AUDIOFIX_H
/* Shared in-memory fixture for the SFX path. Include AFTER the common preamble
 * (needs the test externs, the data_fd2_* globals, and <string.h>).
 *
 * Background: fd2_play_sfx_with_handle (src/audio/audio.c) is a real emitted
 * function whose normal play path resolves a sample-bank entry
 *   entry      = bank_base + sfx_id*4
 *   sample_off = *(uint32 *)(entry + 6)   (offset of the sample from bank base)
 *   sample_end = *(uint32 *)(entry + 10)
 *   length     = sample_end - sample_off
 * and calls AIL_set_sample_address(handle, bank_base + sample_off, length). The
 * testglob AIL_set_sample_address spy records `length` as the fired sfx id
 * (g_sfx_last_id / g_sfx_id_log). audiofix_make_bank() lays out a bank so that
 * length(id) == id for every id, which lets caller tests assert the exact sfx
 * id their function under test plays.
 *
 * The +6 and +10 read windows of adjacent ids overlap (entry stride is 4 but
 * each field is a 4-byte read), so a per-id off/end pair cannot be chosen
 * independently. Instead the single shared sequence D[k] at byte (k*4 + 6) is
 * filled with the triangular numbers k*(k-1)/2; then
 *   off(id) = D[id]              = id*(id-1)/2
 *   end(id) = D[id+1]            = (id+1)*id/2     (this byte == off(id+1))
 *   length  = D[id+1] - D[id]    = id
 * which is consistent for every id simultaneously, matching the real engine's
 * monotonically increasing per-sample offset layout. */

/* big enough for the highest sfx id any test uses (<= 0x1F); needs
 * (max_id+1)*4 + 10 bytes of headroom for the D[max_id+1] end window. */
#define AUDIOFIX_BANK_BYTES 256u

static uint8 t_sfx_bank[AUDIOFIX_BANK_BYTES];

/* Fill t_sfx_bank with the triangular-number offset sequence for ids 0..max_id
 * and point the given bank-base global at it via the returned address. */
static uint32 audiofix_make_bank(int max_id)
{
    int k;
    memset(t_sfx_bank, 0, sizeof(t_sfx_bank));
    /* D[k] lives at byte k*4 + 6; need k up to max_id+1 for end(max_id). */
    for (k = 0; k <= max_id + 1; k++) {
        *(uint32 *)(t_sfx_bank + (unsigned)k * 4u + 6u) =
            (uint32)((unsigned)k * (unsigned)(k - 1) / 2u);
    }
    return (uint32)t_sfx_bank;
}

/* Open all three audio gates so the real fd2_play_sfx_with_handle proceeds to
 * the AIL spy layer (driver available + sample system ready + not cinematic). */
static void audiofix_enable_sfx(void)
{
    data_fd2_audio_sfx_driver_available_flag = 1;
    data_fd2_audio_sfx_enabled_flag = 1;
    data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 0;
}

/* Close the master SFX gate so the real player no-ops (no bank deref). A suite
 * that opens the gates MUST call this at the end of its run function: the gate
 * flags are process-global, and a later suite that drives a real caller into
 * fd2_play_sfx_with_handle with a null/invalid bank would otherwise inherit an
 * open gate and dereference it. Resetting the master (driver) flag is enough to
 * make every gate combination inert. */
static void audiofix_disable_sfx(void)
{
    data_fd2_audio_sfx_driver_available_flag = 0;
}

#endif
