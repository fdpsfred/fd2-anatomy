/* audtab.c -- audio per-chapter BGM track lookup tables (read-only const data). */
#include "types.h"
#include "globals.h"

/* Per-chapter BGM track id for the PLAYER turn phase.
   Indexed by data_fd2_chapter_current_chapter_id (0..29); each element is an
   unsigned 8-bit AIL sequence handle index passed to
   fd2_set_bgm_track_with_fade. Callers read it as `MOVZX r32, byte ptr [base
   + chapter]` (stride 1, zero-extended), e.g. fd2_run_full_turn_cycle @
   0x1A507 / 0x1A5AB / 0x1A610. Track 0x13 = default theme; 0x03/0x04/0x08 are
   chapter-specific themes; ch30 (0x08) = final boss / climax track. */
const uint8 data_fd2_audio_per_chapter_player_turn_bgm_track[30] = {
    0x13, 0x13, 0x13, 0x13, 0x03, 0x13, 0x13, 0x13, 0x03, 0x04,
    0x13, 0x13, 0x13, 0x13, 0x03, 0x13, 0x04, 0x13, 0x13, 0x03,
    0x13, 0x03, 0x04, 0x13, 0x03, 0x13, 0x04, 0x13, 0x13, 0x08
};

/* Per-chapter BGM track id for the ENEMY turn phase.
   Indexed by data_fd2_chapter_current_chapter_id (0..29); each element is an
   unsigned 8-bit AIL sequence handle index passed to
   fd2_set_bgm_track_with_fade for the enemy phase. Callers read it as `MOVZX
   r32, byte ptr [base + chapter]` (stride 1, zero-extended) and compare it
   against the player-turn track to decide whether to cross-fade, e.g.
   fd2_run_full_turn_cycle @ 0x1A50E / 0x1A57A / 0x1A5B2. Immediately follows
   the player-turn table in memory (0x51E63 + 30 = 0x51E81). */
const uint8 data_fd2_audio_per_chapter_enemy_turn_bgm_track[30] = {
    0x0C, 0x0C, 0x01, 0x0C, 0x06, 0x0C, 0x0C, 0x01, 0x06, 0x04,
    0x0C, 0x01, 0x0C, 0x01, 0x06, 0x0C, 0x08, 0x0C, 0x0C, 0x06,
    0x0C, 0x06, 0x08, 0x0C, 0x06, 0x01, 0x08, 0x01, 0x01, 0x08
};

/* FIGANI SFX-bank reference byte -> FDOTHER.DAT entry index translation table.
   fd2_load_figani_sfx_bank reads a FIGANI header's SFX-bank byte at [+4], then
   indexes this table 1-based to obtain the FDOTHER.DAT entry index loaded as the
   SFX bank: fdother_idx = lut[sfx_id_byte - 1]. The caller copies all 6 bytes
   onto its stack (MOVSD + MOVSW from 0x525D6) and indexes them as
   `MOVZX r32, byte ptr [base + sfx_id_byte - 1]` (stride 1, zero-extended), so
   the element type is an unsigned 8-bit FDOTHER index. The six values map
   id 1..6 -> FDOTHER entries 0x30..0x35. See fd2_load_figani_sfx_bank @ 0x2BC9A
   (xref read site 0x2BCB6). */
const uint8 data_fd2_audio_figani_sfx_bank_fdother_index_lut[6] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35
};

/* Per-job footstep/cadence class byte, used by the per-step SFX dispatcher.
   fd2_tick_tutorial_progress_with_sfx copies all 29 bytes onto its stack
   (REP MOVSD x7 + MOVSB from 0x52618) and, for a non-immune walking char,
   indexes them 1-based by job id as `MOVZX r32, byte ptr [base + job_id - 1]`
   (stride 1, zero-extended), so the element type is an unsigned 8-bit class.
   The class selects the cadence: 0 -> divisor 6 / sfx 9, 1 -> divisor 4 /
   sfx 9, anything else -> divisor 9 / sfx 11 (immune chars bypass the table:
   divisor 6 / sfx 10). Job ids run 1..0x1C, so indices span 0..0x1B; the 29th
   byte is the tail copied by the trailing MOVSB. The 0x00 slots correspond to
   job ids with the default (slow) cadence class. See
   fd2_tick_tutorial_progress_with_sfx @ 0x2C9EC (xref read site 0x2CA07). */
const uint8 data_fd2_audio_footstep_sfx_per_job_cadence_class_table[29] = {
    0x01, 0x01, 0x02, 0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x01, 0x00, 0x00, 0x03, 0x01, 0x01, 0x01, 0x03, 0x01,
    0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
};
