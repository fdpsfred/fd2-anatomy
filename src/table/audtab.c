/*
 * audtab.c -- audio read-only data tables (.object2)
 *
 * Per-chapter BGM track-id lookup tables, indexed by chapter id (0..29).
 * Each entry is an 8-bit FDMUS track id; accessors read with MOVZX byte ptr
 * [base + chapter_id] and pass the value to fd2_set_bgm_track_with_fade.
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * data_fd2_audio_per_chapter_player_turn_bgm_track @ 0x51E63  (30 bytes)
 *
 * BGM track id played during the player's turn, one entry per chapter
 * (index 0 = chapter 1 .. index 29 = chapter 30). Read-only (no writers).
 * Accessor: MOVZX EAX, byte ptr [chapter_id + 0x51E63] -> uint8 FDMUS track
 * id, unsigned, stride 1; the value is passed to fd2_set_bgm_track_with_fade.
 * Read by: fd2_run_full_turn_cycle (new-player-turn phase),
 * fd2_main_menu_continue_dispatcher (NEW GAME / CONTINUE / fallback-reload),
 * fd2_load_save_and_init_engine (full engine reload), and main (NEW-GAME
 * re-entry after a chapter event). fd2_run_full_turn_cycle also compares this
 * table against the enemy-turn table to decide whether to fade BGM out
 * between phases.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_audio_per_chapter_player_turn_bgm_track[30] = {
    0x13, 0x13, 0x13, 0x13, 0x03, 0x13, 0x13, 0x13, 0x03, 0x04,
    0x13, 0x13, 0x13, 0x13, 0x03, 0x13, 0x04, 0x13, 0x13, 0x03,
    0x13, 0x03, 0x04, 0x13, 0x03, 0x13, 0x04, 0x13, 0x13, 0x08
};

/* ----------------------------------------------------------------
 * data_fd2_audio_per_chapter_enemy_turn_bgm_track @ 0x51E81  (30 bytes)
 *
 * BGM track id played during the enemy's turn, one entry per chapter
 * (index 0 = chapter 1 .. index 29 = chapter 30). Read-only; consumed by
 * fd2_run_full_turn_cycle: indexed by current_chapter_id, passed to
 * fd2_set_bgm_track_with_fade for the enemy-turn phase, and compared
 * against the player-turn table to decide whether to fade BGM out.
 * Accessor: MOVZX EAX, byte ptr [chapter_id + 0x51E81]  -> uint8, unsigned.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_audio_per_chapter_enemy_turn_bgm_track[30] = {
    0x0c, 0x0c, 0x01, 0x0c, 0x06, 0x0c, 0x0c, 0x01, 0x06, 0x04,
    0x0c, 0x01, 0x0c, 0x01, 0x06, 0x0c, 0x08, 0x0c, 0x0c, 0x06,
    0x0c, 0x06, 0x08, 0x0c, 0x06, 0x01, 0x08, 0x01, 0x01, 0x08
};

/* ----------------------------------------------------------------
 * data_fd2_audio_figani_sfx_bank_fdother_index_lut @ 0x525D6  (6 bytes)
 *
 * Translates a FIGANI header's SFX-bank reference byte (figani_data[+4])
 * into an FDOTHER.DAT entry index. fd2_load_figani_sfx_bank copies the 6
 * bytes onto an 8-byte stack local (MOVSD+MOVSW) and indexes it 1-based:
 *   fdother_idx = lut[sfx_id_byte - 1]
 * then loads that FDOTHER.DAT entry as the animation's SFX bank.
 * Accessor: MOVZX EAX, byte ptr [ESP + sfx_id*1 - 1]  -> uint8, unsigned,
 * stride 1. Read-only (only ever copied out, never written).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_audio_figani_sfx_bank_fdother_index_lut[6] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35
};

/* ----------------------------------------------------------------
 * data_fd2_audio_footstep_sfx_per_job_cadence_class_table @ 0x52618  (29 bytes)
 *
 * Per-job footstep-SFX cadence-class table, indexed as table[job_id - 1] by
 * fd2_tick_tutorial_progress_with_sfx; each byte selects the walk-step SFX
 * cadence class (0/1/other) for that job. 28 entries cover job_id 1..0x1C plus
 * 1 trailing byte that the caller's dword block-copy (7 dwords + 1 byte = 29)
 * sweeps along. Accessor: MOVZX -> unsigned byte per element. Read-only.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_audio_footstep_sfx_per_job_cadence_class_table[29] = {
    1, 1, 2, 1, 0, 0, 1, 1, 1, 1, 2, 1, 0, 0, 3, 1,
    1, 1, 3, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1
};
