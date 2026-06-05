/*
 * aniui.c — UI animations: money / tutorial / shop-scroll / party-add / misc
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
#include <stdio.h>

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

/* ----------------------------------------------------------------
 * fd2_animate_screen_shake @ 0x24B4D (3 callers)
 *
 * Animates a screen-shake effect for num_frames frames by alternating
 * the blit source row (a 1-row vertical jitter).
 *
 * Snapshots the current battle scene once into the render workspace
 * (large_game_state_buffer + 0x8088) via a mode-9 tile-map composite,
 * then runs the finalizer composite, then loops num_frames times:
 * each frame blits the visible 312x192 region to the mode13h primary
 * at 0xA0504 from either the workspace base (even iterations) or one
 * row (0x1C8 bytes) further down (odd iterations), giving a perceived
 * up/down jitter, with a 20-tick (~1100ms) delay per frame.
 *
 * Used after big spells (earthquake / boss attacks) and chapter event
 * cinematics (earthquake intros).
 *
 * Callers:
 *   fd2_chapter_23_end  — chapter 23 ending shake
 *   fd2_chapter_25_init — chapter 25 init earthquake sfx + shake chain
 *   fd2_chapter_29_end  — chapter 29 climax shake
 *
 * Args (cdecl):
 *   num_frames — number of jitter frames to play
 * ---------------------------------------------------------------- */
void fd2_animate_screen_shake(uint32 num_frames)
{
    uint32 i;

    fd2_composite_battle_tile_map(
        data_fd2_large_game_state_buffer_ptr + 0x8088,
        0x1C8, 0xD, 9,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);
    fd2_composite_battle_frame(0);

    for (i = 0; (int32)i < (int32)num_frames; i++) {
        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088 +
                (i & 1) * 0x1C8,
            0x1C8, 0x138, 0xC0);
        __delay_thunk_375b2(0x14);
    }
}

/* ----------------------------------------------------------------
 * fd2_animate_money_increment @ 0x2D3FF (1 caller)
 *
 * Animated INCREMENT of the party gold counter
 * (data_fd2_shared_party_total_gold @ 0x53BF3) by delta with a
 * rolling-digit slot-machine visual. Mirror of
 * fd2_animate_money_decrement @ 0x2D516.
 *
 * Snapshots the 8 current decimal digits, applies delta to the gold
 * total immediately, snapshots the 8 target digits, then per outer
 * iteration diffs the two: each mismatching digit position is flagged
 * (anim_state=1) and animated through a 9-frame roll. On each frame
 * the per-digit blit primitive draws sprite (cur_digit*9 + anim_state)
 * at slot offset 0xA7A90 + pos*6, advancing anim_state; when a digit's
 * roll completes (anim_state hits 10) its cur_digit advances by one
 * (wrapping 10->0). Carries to higher positions are resolved on the
 * next outer re-diff. Loops until current digits equal target digits.
 *
 * Cadence: 9 rolling frames per advance step, 10ms (__delay_thunk_375b2)
 * per frame.
 *
 * Caller:
 *   fd2_run_sell_item_menu — credit gold from item sale.
 *
 * Args (cdecl):
 *   delta — amount of gold to add.
 * ---------------------------------------------------------------- */
void fd2_animate_money_increment(uint32 delta)
{
    uint8 new_digits[20];
    uint8 cur_digits[20];
    uint8 anim_state[20];
    uint8 all_match;
    uint32 screen_pos;
    uint32 anim_phase;
    uint32 digit_iter;
    int32 iter;
    int32 i;

    sprintf((char *)cur_digits, "%0.8d", data_fd2_shared_party_total_gold);
    for (iter = 0; iter < 8; iter++) {
        cur_digits[iter] = cur_digits[iter] - 0x30;
    }

    data_fd2_shared_party_total_gold = data_fd2_shared_party_total_gold + delta;

    sprintf((char *)new_digits, "%0.8d", data_fd2_shared_party_total_gold);
    for (i = 0; i < 8; i++) {
        new_digits[i] = new_digits[i] - 0x30;
    }

    do {
        all_match = 1;
        for (i = 0; i < 8; i++) {
            if (cur_digits[i] == new_digits[i]) {
                anim_state[i] = 0;
            } else {
                anim_state[i] = 1;
                all_match = 0;
            }
        }

        if (!all_match) {
            for (i = 0; i < 9; i++) {
                for (digit_iter = 0; (int32)digit_iter < 8; digit_iter++) {
                    anim_phase = (uint32)anim_state[digit_iter];
                    if (anim_phase != 0) {
                        screen_pos = digit_iter * 6 + 0xA7A90;
                        fd2_blit_money_digit_sprite(screen_pos, 0x140,
                            (uint32)cur_digits[digit_iter] * 9 + anim_phase);
                        anim_state[digit_iter] = anim_state[digit_iter] + 1;
                        if (anim_state[digit_iter] == 10) {
                            cur_digits[digit_iter] = cur_digits[digit_iter] + 1;
                            if (cur_digits[digit_iter] == 10) {
                                cur_digits[digit_iter] = 0;
                            }
                        }
                    }
                }
                __delay_thunk_375b2(10);
            }
        }
    } while (!all_match);
}
