/*
 * cursor.c — Cursor movement + camera pan functions
 *
 * 4 directional cursor moves + 3 pan functions.
 * All operate on battle viewport globals (cursor world/screen pos,
 * window origin, map dimensions).
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_cursor_move_up @ 0x11B48  (3 callers)
 * ---------------------------------------------------------------- */
void fd2_cursor_move_up(void)
{
    if (data_fd2_battle_cursor_world_y != 0) {
        if ((data_fd2_battle_cursor_screen_y < 2)
            && (data_fd2_battle_view_window_origin_y != 0)) {
            data_fd2_battle_cursor_world_y =
                data_fd2_battle_cursor_world_y - 1;
            data_fd2_battle_view_window_origin_y =
                data_fd2_battle_view_window_origin_y - 1;
        }
        else {
            data_fd2_battle_cursor_world_y =
                data_fd2_battle_cursor_world_y - 1;
            data_fd2_battle_cursor_screen_y =
                data_fd2_battle_cursor_screen_y - 1;
            if (data_fd2_battle_anim_phase == 0) {
                return;
            }
        }
    }
    fd2_composite_battle_frame(0);
}

/* ----------------------------------------------------------------
 * fd2_cursor_move_down @ 0x11B9B  (3 callers)
 * ---------------------------------------------------------------- */
void fd2_cursor_move_down(void)
{
    uint32 y_limit;
    y_limit = data_fd2_battle_map_height_tiles - 1;
    if (y_limit != data_fd2_battle_cursor_world_y) {
        if ((data_fd2_battle_cursor_screen_y < 6)
            || (data_fd2_battle_map_height_tiles - 8
                == data_fd2_battle_view_window_origin_y)) {
            data_fd2_battle_cursor_world_y =
                data_fd2_battle_cursor_world_y + 1;
            data_fd2_battle_cursor_screen_y =
                data_fd2_battle_cursor_screen_y + 1;
            if (data_fd2_battle_anim_phase == 0) {
                return;
            }
        }
        else {
            data_fd2_battle_cursor_world_y =
                data_fd2_battle_cursor_world_y + 1;
            data_fd2_battle_view_window_origin_y =
                data_fd2_battle_view_window_origin_y + 1;
        }
    }
    fd2_composite_battle_frame(0);
}

/* ----------------------------------------------------------------
 * fd2_cursor_move_right @ 0x11BFA  (3 callers)
 * ---------------------------------------------------------------- */
void fd2_cursor_move_right(void)
{
    uint32 x_limit;
    x_limit = data_fd2_battle_map_width_tiles - 1;
    if (x_limit != data_fd2_battle_cursor_world_x) {
        if ((data_fd2_battle_cursor_screen_x < 0xb)
            || (data_fd2_battle_map_width_tiles - 0xd
                == data_fd2_battle_view_window_origin_x)) {
            data_fd2_battle_cursor_world_x =
                data_fd2_battle_cursor_world_x + 1;
            data_fd2_battle_cursor_screen_x =
                data_fd2_battle_cursor_screen_x + 1;
            if (data_fd2_battle_anim_phase == 0) {
                return;
            }
        }
        else {
            data_fd2_battle_cursor_world_x =
                data_fd2_battle_cursor_world_x + 1;
            data_fd2_battle_view_window_origin_x =
                data_fd2_battle_view_window_origin_x + 1;
        }
    }
    fd2_composite_battle_frame(0);
}

/* ----------------------------------------------------------------
 * fd2_cursor_move_left @ 0x11C59  (3 callers)
 * ---------------------------------------------------------------- */
void fd2_cursor_move_left(void)
{
    if (data_fd2_battle_cursor_world_x != 0) {
        if ((data_fd2_battle_cursor_screen_x < 2)
            && (data_fd2_battle_view_window_origin_x != 0)) {
            data_fd2_battle_cursor_world_x =
                data_fd2_battle_cursor_world_x - 1;
            data_fd2_battle_view_window_origin_x =
                data_fd2_battle_view_window_origin_x - 1;
        }
        else {
            data_fd2_battle_cursor_world_x =
                data_fd2_battle_cursor_world_x - 1;
            data_fd2_battle_cursor_screen_x =
                data_fd2_battle_cursor_screen_x - 1;
            if (data_fd2_battle_anim_phase == 0) {
                return;
            }
        }
    }
    fd2_composite_battle_frame(0);
}

/* ----------------------------------------------------------------
 * fd2_pan_cursor_to_tile_animated @ 0x12CEA  (15 callers)
 *
 * Animated pan: step cursor one tile per frame, X first then Y.
 * ---------------------------------------------------------------- */
void fd2_pan_cursor_to_tile_animated(int target_x, int target_y)
{
    fd2_composite_battle_frame(0);
    while (target_x != (int)data_fd2_battle_cursor_world_x) {
        if (target_x < (int)data_fd2_battle_cursor_world_x) {
            fd2_cursor_move_left();
        }
        else {
            fd2_cursor_move_right();
        }
        if ((data_fd2_battle_anim_phase != 0)
            && (data_fd2_battle_anim_phase != 6)) {
            fd2_wait_n_bios_ticks(1);
        }
        fd2_clear_keyboard_buffer();
    }
    while (target_y != (int)data_fd2_battle_cursor_world_y) {
        if (target_y < (int)data_fd2_battle_cursor_world_y) {
            fd2_cursor_move_up();
        }
        else {
            fd2_cursor_move_down();
        }
        if ((data_fd2_battle_anim_phase != 0)
            && (data_fd2_battle_anim_phase != 6)) {
            fd2_wait_n_bios_ticks(1);
        }
        fd2_clear_keyboard_buffer();
    }
}

/* ----------------------------------------------------------------
 * fd2_pan_cursor_to_char @ 0x12D7B  (28 callers)
 *
 * Convenience: pan to runtime_char[char_idx]'s tile position.
 * ---------------------------------------------------------------- */
void fd2_pan_cursor_to_char(uint32 char_idx)
{
    fd2_pan_cursor_to_tile_animated(
        (int)(uint32)
            data_fd2_battle_runtime_char_array_ptr[char_idx].pos_x,
        (int)(uint32)
            data_fd2_battle_runtime_char_array_ptr[char_idx].pos_y);
}

/* ----------------------------------------------------------------
 * fd2_pan_cursor_and_window @ 0x135DD  (98 callers)
 *
 * Scroll viewport + cursor together until window origin matches target.
 * ---------------------------------------------------------------- */
void fd2_pan_cursor_and_window(uint32 target_ox, uint32 target_oy)
{
    data_fd2_battle_anim_phase = 0;
    while (target_ox != data_fd2_battle_view_window_origin_x) {
        if ((int)target_ox
            < (int)data_fd2_battle_view_window_origin_x) {
            data_fd2_battle_cursor_world_x =
                data_fd2_battle_cursor_world_x - 1;
            data_fd2_battle_view_window_origin_x =
                data_fd2_battle_view_window_origin_x - 1;
        }
        else {
            data_fd2_battle_cursor_world_x =
                data_fd2_battle_cursor_world_x + 1;
            data_fd2_battle_view_window_origin_x =
                data_fd2_battle_view_window_origin_x + 1;
        }
        fd2_composite_battle_frame(0);
        fd2_clear_keyboard_buffer();
    }
    while (target_oy != data_fd2_battle_view_window_origin_y) {
        if ((int)target_oy
            < (int)data_fd2_battle_view_window_origin_y) {
            data_fd2_battle_cursor_world_y =
                data_fd2_battle_cursor_world_y - 1;
            data_fd2_battle_view_window_origin_y =
                data_fd2_battle_view_window_origin_y - 1;
        }
        else {
            data_fd2_battle_cursor_world_y =
                data_fd2_battle_cursor_world_y + 1;
            data_fd2_battle_view_window_origin_y =
                data_fd2_battle_view_window_origin_y + 1;
        }
        fd2_composite_battle_frame(0);
        fd2_clear_keyboard_buffer();
    }
}

/* ----------------------------------------------------------------
 * Battle viewport window origin X (tile column of left edge) @ 0x53AA9
 *
 * Runtime camera-scroll state, not a constant. Zero at load (BSS);
 * first set by battle/chapter init + save-load, then incremented/
 * decremented by the cursor-move and pan functions above as the
 * viewport scrolls horizontally. uint32 matches the DWORD access
 * width seen in the writers (INC/CMP dword ptr [0x53AA9]); range is a
 * small non-negative tile column (0 .. map_width_tiles - 13).
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_view_window_origin_x;
