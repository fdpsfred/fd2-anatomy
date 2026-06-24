/*
 * aniui.c — UI animations: money / tutorial / shop-scroll / party-add / misc
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ----------------------------------------------------------------
 * fd2_tick_tutorial_progress_with_sfx @ 0x2C9EC
 *
 * Per-frame footstep SFX dispatcher. Ticked once per frame inside the
 * 6-frame walk-step slide loop by all four directional walk_step
 * handlers and by fd2_cutscene_event_trigger (cutscene-driven walk
 * simulation). Picks a cadence (divisor + SFX id) for the walking unit,
 * plays the footstep SFX on each divisor-aligned frame, then advances
 * the cadence counter.
 *
 * Cadence selection (char_idx = walking unit index):
 *   - status-immune (flying/lifted): divisor 6, sfx 10.
 *   - else by per-job cadence class job_tbl[job_id - 1]
 *     (job_tbl copied from data_fd2_audio_footstep_sfx_per_job_cadence_class_table,
 *      29 bytes covering job ids 1..0x1C):
 *       class 0  -> divisor 6, sfx 9
 *       class 1  -> divisor 4, sfx 9
 *       other    -> divisor 9, sfx 11
 *   SFX fires when data_fd2_audio_walk_step_sfx_cadence_counter % divisor
 *   == 0; the counter is then incremented.
 *
 * Globals: reads runtime_char[char_idx].job_id and the per-job cadence
 * table; reads/writes data_fd2_audio_walk_step_sfx_cadence_counter (that
 * counter is touched ONLY here -- it has no external reader).
 *
 * NOTE: the "tutorial_progress" framing in the name is a misnomer; the
 * counter is a pure footstep cadence counter, not a tutorial milestone.
 * Rename candidate: fd2_tick_walk_step_footstep_sfx.
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
 * up/down jitter, with a ~20ms delay (fd2_delay_ms) per frame.
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
        fd2_delay_ms(0x14);
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
 * Cadence: 9 rolling frames per advance step, 10ms (fd2_delay_ms)
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
                fd2_delay_ms(10);
            }
        }
    } while (!all_match);
}

/* ----------------------------------------------------------------
 * fd2_animate_money_decrement @ 0x2D516 (2 callers)
 *
 * Animated DECREMENT of the party gold counter
 * (data_fd2_shared_party_total_gold @ 0x53BF3) by delta with a
 * rolling-digit visual. Inverse of fd2_animate_money_increment @
 * 0x2D3FF — counts each mismatching digit DOWN with borrow from
 * 0 -> 9.
 *
 * Snapshots the 8 current decimal digits, subtracts delta from the
 * gold total immediately, snapshots the 8 target digits, then per
 * outer iteration diffs the two. Each mismatching digit position is
 * flagged (anim_state=9) and pre-decremented in the diff loop itself
 * (with a 0xFF -> 9 borrow), so the inner 9-frame roll renders the
 * descending sprite index (anim_state + cur_digit*9 - 1) while
 * anim_state counts 9..1 down to 0. Borrows to higher positions are
 * resolved on the next outer re-diff. Loops until current digits
 * equal target digits.
 *
 * Cadence: 9 rolling frames per advance step, 10ms (fd2_delay_ms)
 * per frame.
 *
 * Callers:
 *   fd2_run_buy_item_menu     — debit gold on shop purchase.
 *   fd2_run_revive_menu_main  — debit gold on revive payment.
 *
 * Args (cdecl):
 *   delta — amount of gold to subtract.
 * ---------------------------------------------------------------- */
void fd2_animate_money_decrement(uint32 delta)
{
    uint8 new_digits[20];
    uint8 anim_state[20];
    uint8 cur_digits[20];
    uint8 all_match;
    uint32 screen_pos;
    uint32 sprite_idx;
    uint32 digit_iter;
    int32 iter;
    int32 i;

    sprintf((char *)cur_digits, "%0.8d", data_fd2_shared_party_total_gold);
    for (iter = 0; iter < 8; iter++) {
        cur_digits[iter] = cur_digits[iter] - 0x30;
    }

    data_fd2_shared_party_total_gold = data_fd2_shared_party_total_gold - delta;

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
                anim_state[i] = 9;
                all_match = 0;
                cur_digits[i] = cur_digits[i] - 1;
                if (cur_digits[i] == 0xFF) {
                    cur_digits[i] = 9;
                }
            }
        }

        if (!all_match) {
            for (i = 0; i < 9; i++) {
                for (digit_iter = 0; (int32)digit_iter < 8; digit_iter++) {
                    if (anim_state[digit_iter] != 0) {
                        sprite_idx = ((uint32)anim_state[digit_iter] +
                            (uint32)cur_digits[digit_iter] * 9) - 1;
                        screen_pos = digit_iter * 6 + 0xA7A90;
                        fd2_blit_money_digit_sprite(screen_pos, 0x140, sprite_idx);
                        anim_state[digit_iter] = anim_state[digit_iter] - 1;
                    }
                }
                fd2_delay_ms(10);
            }
        }
    } while (!all_match);
}

/* ----------------------------------------------------------------
 * fd2_animate_tutorial_dialog_intro_or_outro @ 0x2D669 (3 callers)
 *
 * 4-frame "speech-bubble wing" deploy/retract animation for the
 * chapter-intro dialog panel. Used by all three chapter-intro menu
 * types (main / typeB / typeC) when opening or closing the panel.
 * NOTE: the "tutorial" framing in the name is a misnomer -- this drives
 * the chapter-intro shop/menu dialog panel, not any tutorial.
 * Rename candidate: fd2_animate_chapter_intro_dialog_wings.
 *
 * Setup: backs up the current mode13h framebuffer (0xA0000, 64000 B)
 * into a malloc'd scratch, paints a 20-row dark band (palette 0x4A,
 * 104 px wide at x=0xC9, rows y=0xA9..0xBC) into that backup, then
 * seeds the working composite buffer with the banded backup.
 *
 * Per-frame loop (frame = 0..3): restores the banded backdrop into the
 * working buffer, then for each of the 4 corners blits the corner
 * sprite into the buffer at base + corner_offs[corner]/divisor + 0xD430.
 * The divisor scales each corner's signed offset, so a larger divisor
 * pulls the wings closer to the center (0xD430). open_or_close picks the
 * ramp direction:
 *   open_or_close == 0  -> OPEN/deploy:  divisor 4,3,2,1 over the frames,
 *                          so the offsets grow 1/4 -> full and the wings
 *                          spread OUT from center. Matches the menu-open
 *                          call site, which passes 0.
 *   open_or_close != 0  -> CLOSE/retract: divisor 1,2,3,4 over the frames,
 *                          so the offsets shrink full -> 1/4 and the wings
 *                          converge IN toward center. Matches the menu-
 *                          close call site, which passes 1.
 * Each frame is then committed to 0xA0000. corner_offs is the 4-entry
 * signed offset table @ 0x526DA (-39,-13,13,39); the per-corner divide is
 * signed (truncates toward 0).
 *
 * Cleanup: OPEN (param 0) leaves the band+wings on screen; CLOSE (param
 * non-zero) repaints the band-only backdrop to 0xA0000 (erasing the
 * wings). The scratch is freed.
 *
 * The sprite source is atlas-indexed exactly like the sibling
 * fd2_wait_input_with_chapter_dialog_blink corner blit: atlas base +
 * *(int *)(atlas + 6 + (corner*2+3)*4).
 *
 * Callers: fd2_run_chapter_intro_menu_main, _typeB, _typeC.
 *
 * Cdecl, 1 stack param (open_or_close); void return. The binary's
 * __CHK(0x34) stack-probe prologue is compiler-generated and omitted
 * here. The memmove traffic to/from 0xA0000 hits the VGA aperture
 * (real VGA RAM under DOS/4GW).
 * ---------------------------------------------------------------- */
void fd2_animate_tutorial_dialog_intro_or_outro(uint32 open_or_close)
{
    int32 corner_offs[4];
    void *dst;
    int32 row;
    int32 divisor;
    uint32 corner_iter;
    uint32 frame;

    corner_offs[0] = data_fd2_ui_chapter_intro_dialog_corner_offset_table_a[0];
    corner_offs[1] = data_fd2_ui_chapter_intro_dialog_corner_offset_table_a[1];
    corner_offs[2] = data_fd2_ui_chapter_intro_dialog_corner_offset_table_a[2];
    corner_offs[3] = data_fd2_ui_chapter_intro_dialog_corner_offset_table_a[3];

    dst = malloc(64000);
    memmove(dst, (void *)0xA0000, 64000);
    for (row = 0; row < 0x14; row++) {
        memset((void *)((int32)dst + (row + 0xA9) * 0x140 + 0xC9), 0x4A, 0x68);
    }
    memmove((void *)data_fd2_large_game_state_buffer_ptr, dst, 64000);

    for (frame = 0; (int32)frame < 4; frame++) {
        memmove((void *)data_fd2_large_game_state_buffer_ptr, dst, 64000);
        for (corner_iter = 0; (int32)corner_iter < 4; corner_iter++) {
            divisor = (open_or_close != 0) ? (int32)(frame + 1)
                                           : (int32)(4 - frame);
            fd2_blit_sprite_with_stride_setup(
                data_fd2_large_game_state_buffer_ptr
                    + corner_offs[corner_iter] / divisor + 0xD430,
                data_fd2_ui_menu_screen_sprite_atlas_buf_ptr
                    + *(int32 *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr
                                 + 6 + (corner_iter * 2 + 3) * 4),
                0x140);
        }
        memmove((void *)0xA0000,
                (void *)data_fd2_large_game_state_buffer_ptr, 64000);
    }

    if (open_or_close != 0) {
        memmove((void *)0xA0000, dst, 64000);
    }
    free(dst);
}

/* ----------------------------------------------------------------
 * fd2_animate_scroll_up_in_shop_dialog @ 0x2E19B (5 callers)
 *
 * Animate a 0x4A-row x 0x11C-byte block scrolling UP within the shop
 * dialog area at framebuffer offset 0xA8FCA. Three staged shifts of 6
 * rows each (with 10ms pacing), then a final 8-row shift to land 2 rows
 * below original -- total scroll distance 0x1A rows (the per-row height
 * in the shop grid layout). Row stride is 0x140 (mode13h scanline).
 *
 * Phase 1 (3 x 6-row shifts, each followed by a 6-row dark-grey fill):
 *   per step: shift the 0x4A-row block up by 6 rows
 *   (src 0xA974A = dst 0xA8FCA + 0x780 = +6*0x140), clear the bottom 6
 *   rows to palette 0x49 at 0xAEC4A, then fd2_delay_ms(10).
 * Phase 2 (single 8-row final shift): shift the 0x48-row block up by 8
 *   rows (src 0xA99CA = dst 0xA8FCA + 0xA00 = +8*0x140), clear the
 *   bottom 8 rows to palette 0x49 at 0xAE9CA.
 *
 * Callers (5):
 *   fd2_shop_menu_input_loop @ 0x2DF6B (Right/Down past the viewport)
 *   fd2_party_roster_single_select_loop @ 0x2E7D5
 *   fd2_party_roster_class_select_loop @ 0x2EA08
 *   fd2_promote_members_select_loop @ 0x30D94
 *   fd2_promote_member_select_loop @ 0x31356
 *
 * Cdecl, no params, void return. The binary's __CHK(0x18) stack-probe
 * prologue is compiler-generated and omitted here. All memmove/memset
 * traffic hits the mode13h aperture (real VGA RAM under DOS/4GW).
 * ---------------------------------------------------------------- */
void fd2_animate_scroll_up_in_shop_dialog(void)
{
    int row;
    int step;

    for (step = 0; step < 3; step++) {
        for (row = 0; row < 0x4A; row++) {
            memmove((void *)(row * 0x140 + 0xA8FCA),
                    (void *)(row * 0x140 + 0xA974A), 0x11C);
        }
        for (row = 0; row < 6; row++) {
            memset((void *)(row * 0x140 + 0xAEC4A), 0x49, 0x11C);
        }
        fd2_delay_ms(10);
    }

    for (row = 0; row < 0x48; row++) {
        memmove((void *)(row * 0x140 + 0xA8FCA),
                (void *)(row * 0x140 + 0xA99CA), 0x11C);
    }
    for (row = 0; row < 8; row++) {
        memset((void *)(row * 0x140 + 0xAE9CA), 0x49, 0x11C);
    }
}

/* ----------------------------------------------------------------
 * fd2_animate_scroll_down_in_shop_dialog @ 0x2E26C (5 callers)
 *
 * Animate the shop item list scrolling DOWN within the framebuffer
 * area at 0xA8FCA. Counterpart of fd2_animate_scroll_up_in_shop_dialog
 * @ 0x2E19B — same shift sizes but reversed direction, with a backward
 * row iteration so the overlapping memmove copies high-to-low and does
 * not clobber not-yet-moved source rows. Row stride is 0x140 (mode13h
 * scanline).
 *
 * Phase 1 (3 x 6-row shifts, each followed by a 6-row dark-grey fill):
 *   per step: shift the 0x4A-row block down by 6 rows (dst 0xA974A =
 *   src 0xA8FCA + 0x780 = +6*0x140) iterating row 0x49..0 descending,
 *   then clear the top 6 rows to palette 0x49 at 0xA8FCA, then
 *   fd2_delay_ms(10).
 * Phase 2 (single 8-row final shift): shift the 0x48-row block down by 8
 *   rows (dst 0xA99CA = src 0xA8FCA + 0xA00 = +8*0x140) iterating row
 *   0x47..0 descending, then clear the top 8 rows to palette 0x49 at
 *   0xA8FCA.
 *
 * Callers (5):
 *   fd2_shop_menu_input_loop @ 0x2DF6B
 *   fd2_party_roster_single_select_loop @ 0x2E840
 *   fd2_party_roster_class_select_loop @ 0x2EA61
 *   fd2_promote_members_select_loop @ 0x30D4C
 *   fd2_promote_member_select_loop @ 0x3130A
 *   (all invoked when Up/Left wraps past the visible top).
 *
 * Cdecl, no params, void return. The binary's __CHK(0x18) stack-probe
 * prologue is compiler-generated and omitted here. All memmove/memset
 * traffic hits the mode13h aperture (real VGA RAM under DOS/4GW).
 * ---------------------------------------------------------------- */
void fd2_animate_scroll_down_in_shop_dialog(void)
{
    int row;
    int step;

    for (step = 0; step < 3; step++) {
        for (row = 0x49; row >= 0; row--) {
            memmove((void *)(row * 0x140 + 0xA974A),
                    (void *)(row * 0x140 + 0xA8FCA), 0x11C);
        }
        for (row = 0; row < 6; row++) {
            memset((void *)(row * 0x140 + 0xA8FCA), 0x49, 0x11C);
        }
        fd2_delay_ms(10);
    }

    for (row = 0x47; row >= 0; row--) {
        memmove((void *)(row * 0x140 + 0xA99CA),
                (void *)(row * 0x140 + 0xA8FCA), 0x11C);
    }
    for (row = 0; row < 8; row++) {
        memset((void *)(row * 0x140 + 0xA8FCA), 0x49, 0x11C);
    }
}

/* ----------------------------------------------------------------
 * fd2_animate_shop_transaction_feedback @ 0x2F4C6 (3 callers)
 *
 * Animation feedback for a successful gold transaction in one of the
 * shop-style menus (buy / sell / revive). Plays a per-chapter-type
 * sprite cycle and (for state 4 only) a cyan additive palette flash.
 * Dispatches on the per-chapter byte
 * data_fd2_chapter_intro_menu_cursor_state @ 0x5412B:
 *
 *   state 1: 5-frame sprite cycle (atlas frames 0x17..0x1B at framebuffer
 *            0xA38E9), 2 BIOS ticks per frame.
 *   state 3: single-sprite splash (frame 0x17 at 0xA3154), held 8 ticks
 *            after a 1-tick pre-roll.
 *   state 4: paint mouth-CLOSED portrait (frame 3), wait 2 ticks, 9-frame
 *            sprite cycle (0x17..0x1F at 0xA2893, 2 ticks each), then a cyan
 *            additive palette flash — ramp brightness UP 0..0x3E by step 2
 *            (each step + a 4ms delay), wait 10 ticks, ramp DOWN 0x3E..0 by
 *            step 2 (mirror), then wait 5 ticks.
 *   state 5: 7-frame sprite cycle (0x17..0x1D at 0xA2383), 2 ticks per frame.
 *   state 2 / other: no animation.
 *
 * After playing, states 1/3/4 restore via fd2_paint_portrait_to_dialog_area(0);
 * states 5 and 2/other skip that restore. Every path ends with
 * fd2_clear_keyboard_buffer().
 *
 * The frame blits go through fd2_blit_indexed_sprite_rle(dst, 0x140,
 * data_fd2_ui_menu_screen_sprite_atlas_buf_ptr @ 0x54147, frame_idx); the
 * destinations are fixed mode13h aperture addresses (real VGA RAM under
 * DOS/4GW). The palette flash drives the DAC via the real
 * fd2_set_vga_palette_range_with_add (port 0x3C8/0x3C9 writes). Invoked by
 * the buy / sell / revive menus after a successful gold transaction.
 *
 * Cdecl, no params, void return. The binary's __CHK(0x18) stack-probe
 * prologue is compiler-generated and omitted here.
 *
 * Callers:
 *   fd2_run_buy_item_menu     @ 0x2F0B0
 *   fd2_run_sell_item_menu    @ 0x2F642
 *   fd2_run_revive_menu_main  @ 0x30DC3
 * ---------------------------------------------------------------- */
void fd2_animate_shop_transaction_feedback(void)
{
    int frame;
    uint32 brightness;
    uint32 wait_ticks;

    if (data_fd2_chapter_intro_menu_cursor_state == 1) {
        for (frame = 0; frame < 5; frame++) {
            fd2_blit_indexed_sprite_rle(0xA38E9, 0x140,
                data_fd2_ui_menu_screen_sprite_atlas_buf_ptr,
                (uint32)(frame + 0x17));
            fd2_wait_n_bios_ticks(2);
        }
        fd2_paint_portrait_to_dialog_area(0);
    } else if (data_fd2_chapter_intro_menu_cursor_state == 3) {
        fd2_wait_n_bios_ticks(1);
        fd2_blit_indexed_sprite_rle(0xA3154, 0x140,
            data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0x17);
        wait_ticks = 8;
        fd2_wait_n_bios_ticks(wait_ticks);
        fd2_paint_portrait_to_dialog_area(0);
    } else if (data_fd2_chapter_intro_menu_cursor_state == 4) {
        fd2_paint_portrait_to_dialog_area(3);
        fd2_wait_n_bios_ticks(2);
        for (frame = 0; frame < 9; frame++) {
            fd2_blit_indexed_sprite_rle(0xA2893, 0x140,
                data_fd2_ui_menu_screen_sprite_atlas_buf_ptr,
                (uint32)(frame + 0x17));
            fd2_wait_n_bios_ticks(2);
        }
        for (brightness = 0; (int32)brightness < 0x40; brightness += 2) {
            fd2_set_vga_palette_range_with_add(0, 0xFF, brightness);
            fd2_delay_ms(4);
        }
        fd2_wait_n_bios_ticks(10);
        for (brightness = 0x3E; -1 < (int32)brightness; brightness -= 2) {
            fd2_set_vga_palette_range_with_add(0, 0xFF, brightness);
            fd2_delay_ms(4);
        }
        wait_ticks = 5;
        fd2_wait_n_bios_ticks(wait_ticks);
        fd2_paint_portrait_to_dialog_area(0);
    } else if (data_fd2_chapter_intro_menu_cursor_state == 5) {
        for (frame = 0; frame < 7; frame++) {
            fd2_blit_indexed_sprite_rle(0xA2383, 0x140,
                data_fd2_ui_menu_screen_sprite_atlas_buf_ptr,
                (uint32)(frame + 0x17));
            fd2_wait_n_bios_ticks(2);
        }
    }
    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * fd2_animate_party_addition_with_appear_effect @ 0x32999 (4 callers)
 *
 * Plays the "new char appearance" 12-frame animation with explosion
 * sprites + SFX for newly added party members. target_race_id is the
 * joining recruit / party-slot id used as the target race-id filter; it is
 * forwarded to fd2_load_chapter_portraits_and_dump_tmp, which spawns
 * only the field chars whose race byte matches it. The actual chapter
 * index is read separately from data_fd2_chapter_current_chapter_id
 * (inside that loader), NOT from this parameter.
 *
 * Setup:
 *   - fd2_load_dat_resource(0x51A4D = FDOTHER.DAT base, 0, 0x5F)
 *       -> sfx_buf (appearance chime).
 *   - fd2_load_dat_resource(0x51A4D, 0, 9) -> explosion_sprite
 *       (sparkle/explosion sprite-sheet; explosion_sprite[snapshot*4 + 6]
 *       = per-frame source offset into the sheet).
 *   - malloc(0x25680) -> backup buf; memmove(backup,
 *       large_game_state_buffer, 0x25680) (snapshot working surface).
 *   - old_char_count = party_member_count (record pre-join count).
 *   - fd2_load_chapter_portraits_and_dump_tmp(target_race_id) (spawns
 *       the field chars whose race byte matches target_race_id, so may
 *       grow party_member_count by adding the new chars).
 *
 * 12-frame loop (snapshot = 0..0xB):
 *   - if snapshot == 1: fd2_play_sfx_with_handle(sfx_buf, 0, 1).
 *   - memmove(large_game_state_buffer, backup, 0x25680): restore the
 *       working surface (each frame redraws from scratch).
 *   - src_off = explosion_sprite[snapshot*4 + 6].
 *   - For each newly added char (char_iter in [old_char_count,
 *       party_member_count)): if its (pos_x, pos_y) is inside the
 *       battle view window, blit the explosion sprite into the buffer at
 *       (pos_y - origin_y)*0x2AC0 + (pos_x - origin_x - 1)*0x18 + 0x75D8.
 *   - fd2_blit_rectangle(0xA0504, 0x140, buffer + 0x8088, 0x1C8, 0x138,
 *       0xC0): paint the frame to the mode13h primary.
 *   - Frame 6 special path: restore backup, paint old chars
 *       (iter < old_char_count, skip flags&1 = dead), shift buffer base
 *       by -0xE40, paint new chars (skip dead), shift base back +0xE40,
 *       fd2_redraw_terrain_tiles_under_chars, save back to backup.
 *   - Frame 7 special path: fd2_composite_battle_tile_map(buffer+0x8088,
 *       0x1C8, 0xD, 8, origin_x, origin_y), paint old chars, shift base
 *       by -0x8E8, paint new chars, shift base back +0x8E8,
 *       fd2_redraw_terrain_tiles_under_chars, save back to backup.
 *   - Frame 8 special path: fd2_composite_battle_tile_map(...),
 *       fd2_composite_all_chars_overlay (handles its own shadow), save
 *       back to backup.
 *   - All frames: fd2_clear_keyboard_buffer; fd2_wait_n_bios_ticks(1).
 *
 * Exit (snapshot > 0xB): free(explosion_sprite); free(backup);
 * free(sfx_buf); return.
 *
 * The large_game_state_buffer pointer-shift trick (base -= 0xE40 / -=
 * 0x8E8 then restored) is a temporary offset adjustment so the existing
 * paint helper fd2_paint_char_sprite_at_world_pos writes to a shifted
 * base, achieving a layered composite without separate buffer args.
 *
 * Callers (4): fd2_chapter_01_init @ 0x3289B + 0x328BB,
 *   fd2_chapter_event_handler_01__ch1_dialog_with_state @ 0x342CE,
 *   fd2_chapter_event_handler_02__ch1_dialog_with_state @ 0x34336.
 *
 * Cdecl, 1 stack param (target_race_id); void return. The binary's
 * __CHK(0x3C) stack-probe prologue is compiler-generated and omitted
 * here.
 *
 * Args (cdecl):
 *   target_race_id — joining recruit / party-slot id, forwarded to
 *     fd2_load_chapter_portraits_and_dump_tmp as the race-match filter
 *     that selects which field chars spawn into the party. (The 4 call
 *     sites pass small slot ids 1, 2, 4, 5.) The chapter index itself
 *     comes from data_fd2_chapter_current_chapter_id, not this arg.
 * ---------------------------------------------------------------- */
void fd2_animate_party_addition_with_appear_effect(uint32 target_race_id)
{
    uint32 sfx_buf;
    uint32 explosion_sprite;
    uint32 backup;
    int32 src_off;
    uint32 src_ptr;
    uint32 pos_x;
    uint32 pos_y;
    uint32 char_iter;
    uint32 old_char_count;
    uint32 snapshot;

    sfx_buf = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x5F);
    explosion_sprite = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 9);
    backup = (uint32)malloc(0x25680);
    memmove((void *)backup,
            (void *)data_fd2_large_game_state_buffer_ptr, 0x25680);
    old_char_count = data_fd2_battle_party_member_count;
    fd2_load_chapter_portraits_and_dump_tmp(target_race_id);

    snapshot = 0;
    do {
        if ((int32)snapshot > 0xB) {
            free((void *)explosion_sprite);
            free((void *)backup);
            free((void *)sfx_buf);
            return;
        }
        if (snapshot == 1) {
            fd2_play_sfx_with_handle(sfx_buf, 0, 1);
        }
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
                (void *)backup, 0x25680);
        src_off = *(int32 *)(snapshot * 4 + explosion_sprite + 6);
        src_ptr = explosion_sprite + src_off;
        for (char_iter = old_char_count;
             (int32)char_iter < (int32)data_fd2_battle_party_member_count;
             char_iter++) {
            pos_x = data_fd2_battle_runtime_char_array_ptr[char_iter].pos_x;
            pos_y = data_fd2_battle_runtime_char_array_ptr[char_iter].pos_y;
            if ((int32)data_fd2_battle_view_window_origin_x - 1 <= (int32)pos_x &&
                (int32)pos_x <= (int32)(data_fd2_battle_view_window_origin_x +
                                        data_fd2_battle_view_window_max_x) &&
                (int32)data_fd2_battle_view_window_origin_y <= (int32)pos_y &&
                (int32)pos_y <= (int32)(data_fd2_battle_view_window_origin_y +
                                        data_fd2_battle_view_window_max_y + 1)) {
                fd2_blit_sprite_with_decoded_pixels(
                    data_fd2_large_game_state_buffer_ptr +
                        (pos_y - data_fd2_battle_view_window_origin_y) * 0x2AC0 +
                        ((pos_x - data_fd2_battle_view_window_origin_x) - 1) * 0x18 +
                        0x75D8,
                    src_ptr, 0x1C8);
            }
        }
        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8, 0x138, 0xC0);

        if (snapshot == 6) {
            memmove((void *)data_fd2_large_game_state_buffer_ptr,
                    (void *)backup, 0x25680);
            for (char_iter = 0; (int32)char_iter < (int32)old_char_count;
                 char_iter++) {
                if ((data_fd2_battle_runtime_char_array_ptr[char_iter].flags & 1)
                        == 0) {
                    fd2_paint_char_sprite_at_world_pos(char_iter);
                }
            }
            data_fd2_large_game_state_buffer_ptr -= 0xE40;
            for (; (int32)char_iter < (int32)data_fd2_battle_party_member_count;
                 char_iter++) {
                if ((data_fd2_battle_runtime_char_array_ptr[char_iter].flags & 1)
                        == 0) {
                    fd2_paint_char_sprite_at_world_pos(char_iter);
                }
            }
            data_fd2_large_game_state_buffer_ptr += 0xE40;
            fd2_redraw_terrain_tiles_under_chars();
            memmove((void *)backup,
                    (void *)data_fd2_large_game_state_buffer_ptr, 0x25680);
        } else if (snapshot == 7) {
            fd2_composite_battle_tile_map(
                data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8, 0xD, 8,
                data_fd2_battle_view_window_origin_x,
                data_fd2_battle_view_window_origin_y);
            for (char_iter = 0; (int32)char_iter < (int32)old_char_count;
                 char_iter++) {
                if ((data_fd2_battle_runtime_char_array_ptr[char_iter].flags & 1)
                        == 0) {
                    fd2_paint_char_sprite_at_world_pos(char_iter);
                }
            }
            data_fd2_large_game_state_buffer_ptr -= 0x8E8;
            for (; (int32)char_iter < (int32)data_fd2_battle_party_member_count;
                 char_iter++) {
                if ((data_fd2_battle_runtime_char_array_ptr[char_iter].flags & 1)
                        == 0) {
                    fd2_paint_char_sprite_at_world_pos(char_iter);
                }
            }
            data_fd2_large_game_state_buffer_ptr += 0x8E8;
            fd2_redraw_terrain_tiles_under_chars();
            memmove((void *)backup,
                    (void *)data_fd2_large_game_state_buffer_ptr, 0x25680);
        } else if (snapshot == 8) {
            fd2_composite_battle_tile_map(
                data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8, 0xD, 8,
                data_fd2_battle_view_window_origin_x,
                data_fd2_battle_view_window_origin_y);
            fd2_composite_all_chars_overlay();
            memmove((void *)backup,
                    (void *)data_fd2_large_game_state_buffer_ptr, 0x25680);
        }

        fd2_clear_keyboard_buffer();
        fd2_wait_n_bios_ticks(1);
        snapshot++;
    } while (1);
}

/* ----------------------------------------------------------------
 * fd2_cinematic_warp_char_to_tile @ 0x33F78 (2 callers)
 *
 * Cinematic warp: pans the camera/cursor to a tile, then plays the
 * warp-teleport char animation onto that tile.
 *
 * fd2_animate_warp_teleport_char takes both source and destination
 * tile coords; here both are set to the same target tile, giving
 * "appear at target tile" semantics (no separate source pan).
 *
 * Cdecl, three params, void return. The binary's __CHK(0x18)
 * stack-probe prologue is compiler-generated and omitted here.
 *
 * Params: char_id = runtime_char_array index of the unit being warped,
 *         tile_x / tile_y = target tile coordinates.
 *
 * Callers:
 *   fd2_chapter_30_init                              @ 0x33E3C
 *   fd2_chapter_event_handler_52__ch30_major_cinematic @ 0x35F92
 * ---------------------------------------------------------------- */
void fd2_cinematic_warp_char_to_tile(uint32 char_id, uint32 tile_x, uint32 tile_y)
{
    fd2_pan_cursor_to_tile_animated((int)tile_x, (int)tile_y);
    fd2_animate_warp_teleport_char(char_id, tile_x, tile_y, tile_x, tile_y);
}

/* ----------------------------------------------------------------
 * fd2_animate_palette_flash_pulse_white @ 0x35E5A (7 callers)
 *
 * Pulse-white palette flash effect. The whole DAC (indices 0..0xFF) is
 * brightened toward white, held at peak, then faded back, via the real
 * fd2_set_vga_palette_range_with_add (port 0x3C8/0x3C9 writes).
 *
 *   Fade UP:   brightness 0..0x3F  (64 steps, 8ms each = 512ms)
 *   Hold:      fd2_delay_ms(400)  (400ms at peak brightness)
 *   Fade DOWN: brightness 0x3E..0  (63 steps, 8ms each = 504ms)
 *
 * Total duration ~1.4s. Used for dramatic endgame transitions;
 * all 7 call sites are in the ch29/ch30 endgame cinematics.
 *
 * Cdecl, no params, void return. EBX is the loop counter (callee-saved).
 * The binary's __CHK(0x14) stack-probe prologue is compiler-generated
 * and omitted here.
 *
 * Callers:
 *   fd2_chapter_29_end                                 @ 0x25682, 0x25694, 0x256A6
 *   fd2_chapter_30_init                                @ 0x33EFD
 *   fd2_chapter_event_handler_4c__ch29_major_cinematic @ 0x35DF4, 0x35E06, 0x35E1E
 * ---------------------------------------------------------------- */
void fd2_animate_palette_flash_pulse_white(void)
{
    uint32 brightness;

    for (brightness = 0; (int32)brightness < 0x40; brightness++) {
        fd2_set_vga_palette_range_with_add(0, 0xFF, brightness);
        fd2_delay_ms(8);
    }
    fd2_delay_ms(400);
    for (brightness = 0x3E; -1 < (int32)brightness; brightness--) {
        fd2_set_vga_palette_range_with_add(0, 0xFF, brightness);
        fd2_delay_ms(8);
    }
}

/* ----------------------------------------------------------------
 * data_fd2_audio_walk_step_sfx_cadence_counter @ 0x540FE  (.object2)
 *
 * Free-running per-step footstep-SFX cadence counter; the sole state of
 * fd2_tick_tutorial_progress_with_sfx. Each walk step does
 * (counter % divisor) to gate a footstep SFX, then increments it. The
 * divisor (4/6/9) is chosen per job cadence class.
 *
 * uint8 scalar, accessed exclusively as byte ptr (MOVZX = unsigned).
 * Zero-init (BSS); NOT const -- written by the increment each step.
 * Read/written ONLY here -- there is no tutorial-progress consumer
 * despite the "tutorial_progress" framing in the function name.
 * ---------------------------------------------------------------- */
uint8 data_fd2_audio_walk_step_sfx_cadence_counter;
