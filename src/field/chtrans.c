/*
 * chtrans.c — per-chapter cutscene walk-animation script interpreter
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_cutscene_event_trigger @ 0x1366A  (51 callers)
 *
 * Interpret a per-chapter cutscene byte-script (selected by event_id)
 * to drive runtime-char walk-animation sequences.
 *
 * Script byte-stream:
 *   [n_groups: u8]
 *   per group:
 *     [walk_count_or_flags: u8]  bit7 = special mode; low7 = walk-step count
 *     [step_count: u8]           number of chars moving simultaneously (<= 32)
 *     [pairs * step_count]:      byte char_idx, byte dir
 *       dir: 0 = south (+y), 1 = west (-x), 2 = north (-y), 3 = east (+x)
 *
 * Three modes by bit7 of walk_count_or_flags:
 *   bit7 == 0           : normal 6-frame walk, low7 repeats, commits position.
 *   bit7 == 1, low7 == 0: sprite-refresh blit pass.
 *   bit7 == 1, low7 != 0: facing-only / camera hold for low7 composites.
 * ---------------------------------------------------------------- */
void fd2_cutscene_event_trigger(uint32 event_id)
{
    uint8 *script_ptr;
    uint8 *pPair;
    uint8 n_groups;
    uint8 walk_count_or_flags;
    uint8 step_count;
    uint8 group_iter;
    uint8 walk_repeat_iter;
    uint8 frame_iter;
    uint8 step_iter;
    uint8 dir;
    uint8 step_chars[32];
    uint8 step_dirs[32];
    runtime_char *pChar;
    int member_iter;
    int saved_buf;

    script_ptr = fd2_get_cutscene_event_script((int)event_id);
    n_groups = script_ptr[0];
    pPair = script_ptr + 1;

    for (group_iter = 0; group_iter < n_groups; group_iter++) {
        walk_count_or_flags = pPair[0];
        step_count = pPair[1];
        pPair += 2;

        for (step_iter = 0; step_iter < step_count; step_iter++) {
            step_chars[step_iter] = pPair[0];
            step_dirs[step_iter] = pPair[1];
            pPair += 2;
        }

        if ((walk_count_or_flags & 0x80) == 0) {
            for (walk_repeat_iter = 0; walk_repeat_iter < walk_count_or_flags;
                 walk_repeat_iter++) {
                for (frame_iter = 1; frame_iter < 7; frame_iter++) {
                    fd2_tick_tutorial_progress_with_sfx((uint32)step_chars[0]);
                    for (step_iter = 0; step_iter < step_count; step_iter++) {
                        pChar = data_fd2_battle_runtime_char_array_ptr +
                                step_chars[step_iter];
                        pChar->sprite_state[1] = step_dirs[step_iter];
                        pChar->sprite_state[2] = frame_iter;
                    }
                    if (data_fd2_chapter_cutscene_event_state == 0 ||
                        data_fd2_chapter_cutscene_event_state == 0x40) {
                        fd2_composite_battle_frame(0);
                    } else {
                        data_fd2_chapter_cutscene_event_state++;
                        fd2_composite_battle_frame(1);
                        fd2_set_vga_palette_range(0, 0xFF,
                            data_fd2_chapter_cutscene_event_state);
                    }
                    fd2_wait_n_bios_ticks(1);
                    fd2_clear_keyboard_buffer();
                }
                for (step_iter = 0; step_iter < step_count; step_iter++) {
                    pChar = data_fd2_battle_runtime_char_array_ptr +
                            step_chars[step_iter];
                    dir = step_dirs[step_iter];
                    if (dir == 0) {
                        pChar->pos_y++;
                    } else if (dir == 1) {
                        pChar->pos_x--;
                    } else if (dir == 3) {
                        pChar->pos_x++;
                    } else {
                        pChar->pos_y--;
                    }
                    pChar->sprite_state[2] = 0;
                }
            }
        } else if ((walk_count_or_flags & 0x7F) == 0) {
            fd2_wait_n_bios_ticks(1);
            fd2_composite_battle_tile_map(
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, 0xD, 8,
                data_fd2_battle_view_window_origin_x,
                data_fd2_battle_view_window_origin_y);
            saved_buf = (int)data_fd2_large_game_state_buffer_ptr;
            for (member_iter = 0;
                 member_iter < (int)data_fd2_battle_party_member_count;
                 member_iter++) {
                data_fd2_large_game_state_buffer_ptr = (uint32)saved_buf;
                pChar = data_fd2_battle_runtime_char_array_ptr + member_iter;
                for (step_iter = 0; step_iter < step_count; step_iter++) {
                    if ((uint32)member_iter == step_chars[step_iter]) {
                        data_fd2_large_game_state_buffer_ptr =
                            (uint32)(saved_buf - 0x1560);
                        pChar->sprite_state[1] = step_dirs[step_iter];
                    }
                }
                if ((pChar->flags & 1) == 0) {
                    fd2_paint_char_sprite_at_world_pos((uint32)member_iter);
                }
                data_fd2_large_game_state_buffer_ptr = (uint32)saved_buf;
            }
            fd2_paint_chars_shadow_overlay();
            fd2_blit_rectangle(0xA0504, 0x140,
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, 0x138, 0xC0);
            fd2_wait_n_bios_ticks(2);
            fd2_composite_battle_frame(0);
            fd2_clear_keyboard_buffer();
        } else {
            for (step_iter = 0; step_iter < step_count; step_iter++) {
                data_fd2_battle_runtime_char_array_ptr[step_chars[step_iter]]
                    .sprite_state[1] = step_dirs[step_iter];
            }
            for (step_iter = 0; step_iter < (walk_count_or_flags & 0x7F);
                 step_iter++) {
                fd2_composite_battle_frame(0);
                fd2_wait_n_bios_ticks(1);
                fd2_clear_keyboard_buffer();
            }
        }
    }

    fd2_composite_battle_frame(1);
}

/* ----------------------------------------------------------------
 * fd2_setup_chars_and_camera_for_intro @ 0x233C6  (15 callers)
 *
 * Chapter cutscene -> battle transition helper. Places a contiguous
 * range of runtime-chars (and one optional extra char) at fixed map
 * positions/facings, then resets the battle camera/cursor origin and
 * does one composite-with-fade reveal of the new arrangement.
 *
 * Fades to black first so the re-positioning is hidden, lays out the
 * chars, resets camera + anim phase, composites one frame, then fades
 * back in and holds for 200 ticks.
 *
 * Params (all 11 are cdecl stack slots, each a 4-byte push):
 *   pX_byte_array  = base of per-char X-position byte table
 *   pY_byte_array  = base of per-char Y-position byte table
 *   sprite_facing_fixed_or_array:
 *       value < 4  -> use that value as a fixed facing for every char
 *       value >= 4 -> treat as base of a per-char facing byte table
 *   char_start / char_end = inclusive runtime_char index range to place
 *   extra_char_idx = one additional runtime_char index, or 0 to skip
 *   extra_x / extra_y / extra_facing = position/facing for the extra char
 *       (low byte of each used)
 *   origin_x / origin_y = battle view-window + cursor world origin
 *
 * Callers: 15 chapter end handlers (ch 03/05/07/08/12/14/16/17/18/21/
 * 22/23/26/27/30 end).
 *
 * Body ends by tail-jumping into fd2_composite_battle_frame_zero's
 * shared epilogue (past its composite call, straight to the register
 * restore + RET); semantically a plain return after the fade-in/delay.
 * ---------------------------------------------------------------- */
void fd2_setup_chars_and_camera_for_intro(uint32 pX_byte_array,
                                          uint32 pY_byte_array,
                                          uint32 sprite_facing_fixed_or_array,
                                          int char_start, int char_end,
                                          uint32 extra_char_idx,
                                          int extra_x, int extra_y,
                                          int extra_facing,
                                          int origin_x, int origin_y)
{
    runtime_char *pChar;
    uint8 facing;
    int char_idx;

    fd2_play_palette_fade_to_black();
    fd2_clear_all_chars_acted_flag();

    for (char_idx = char_start; char_idx <= char_end; char_idx++) {
        pChar = data_fd2_battle_runtime_char_array_ptr + char_idx;
        pChar->pos_x = *(uint8 *)(char_idx + pX_byte_array);
        pChar->pos_y = *(uint8 *)(char_idx + pY_byte_array);
        if (sprite_facing_fixed_or_array < 4) {
            facing = (uint8)sprite_facing_fixed_or_array;
        } else {
            facing = *(uint8 *)(char_idx + sprite_facing_fixed_or_array);
        }
        pChar->sprite_state[1] = facing;
    }

    if (extra_char_idx != 0) {
        pChar = data_fd2_battle_runtime_char_array_ptr + extra_char_idx;
        pChar->pos_x = (uint8)extra_x;
        pChar->pos_y = (uint8)extra_y;
        pChar->sprite_state[1] = (uint8)extra_facing;
    }

    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_view_window_origin_x = (uint32)origin_x;
    data_fd2_battle_view_window_origin_y = (uint32)origin_y;
    data_fd2_battle_cursor_world_x = (uint32)origin_x;
    data_fd2_battle_cursor_world_y = (uint32)origin_y;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;

    fd2_composite_battle_frame(1);
    fd2_play_palette_fade_in();
    __delay_thunk_375b2(200);
}
