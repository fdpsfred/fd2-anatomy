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
 *
 * Up-arrow (scancode 0x48) battle cursor handler. Moves the cursor one
 * tile up unless already at the map top (world_y == 0, no-op). Near the
 * top viewport edge (screen_y < 2) with room to scroll (window_origin_y
 * != 0) it scrolls the view up instead of moving the cursor on screen;
 * otherwise it steps the cursor up one screen row. Composites a fresh
 * frame except in the plain inner-step case while no battle animation is
 * running (anim_phase == 0), where the main loop redraws on its next tick.
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
 *
 * Down-arrow (scancode 0x50) battle cursor handler; vertical mirror of
 * fd2_cursor_move_up. Moves the cursor one tile down unless already at the
 * map bottom (world_y == map_height_tiles - 1, no-op). When the cursor sits
 * in the upper part of the viewport (screen_y < 6) or the view is already
 * scrolled to its lowest position (window_origin_y == map_height_tiles - 8),
 * it steps the cursor down one screen row; otherwise it scrolls the view
 * down instead of moving the cursor on screen. Composites a fresh frame
 * except in the plain inner-step case while no battle animation is running
 * (anim_phase == 0), where the main loop redraws on its next tick.
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
 *
 * Right-arrow (scancode 0x4D) battle cursor handler; horizontal mirror
 * of fd2_cursor_move_down. Moves the cursor one tile right unless already
 * at the map right edge (world_x == map_width_tiles - 1, no-op). When the
 * cursor sits in the left part of the viewport (screen_x < 0xB) or the view
 * is already scrolled to its rightmost position (window_origin_x ==
 * map_width_tiles - 0xD, the 13-tile-wide viewport's max scroll), it steps
 * the cursor right one screen column; otherwise it scrolls the view right
 * instead of moving the cursor on screen. Composites a fresh frame except
 * in the plain inner-step case while no battle animation is running
 * (anim_phase == 0), where the main loop redraws on its next tick.
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
 *
 * Left-arrow (scancode 0x4B) battle cursor handler; horizontal mirror
 * of fd2_cursor_move_up. Moves the cursor one tile left unless already
 * at the map left edge (world_x == 0, no-op). Near the left viewport
 * edge (screen_x < 2) with room to scroll (window_origin_x != 0) it
 * scrolls the view left instead of moving the cursor on screen;
 * otherwise it steps the cursor left one screen column. Composites a
 * fresh frame except in the plain inner-step case while no battle
 * animation is running (anim_phase == 0), where the main loop redraws
 * on its next tick.
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
 * Animated camera pan: scroll the viewport window AND the cursor together,
 * one tile per frame, until window_origin reaches (target_ox, target_oy).
 * Resets anim_phase to 0, then pans the X axis to target_ox first and the
 * Y axis to target_oy second; each step composites a fresh frame and drains
 * the keyboard buffer. The cursor's WORLD position is moved in lockstep with
 * the window origin (same delta each step), so the cursor stays at the same
 * SCREEN position while the world scrolls beneath it. Used for cutscene /
 * chapter-intro camera focus that scrolls to a fixed viewport origin.
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

/* ----------------------------------------------------------------
 * Battle viewport window origin Y (tile row of top edge) @ 0x53AAD
 *
 * Runtime camera-scroll state, not a constant. Zero at load (BSS);
 * first set by battle/chapter init + save-load, then incremented/
 * decremented by the cursor-move and pan functions above as the
 * viewport scrolls vertically. uint32 matches the DWORD access width
 * seen in the writers (DEC/INC/CMP/MOV dword ptr [0x53AAD]); range is
 * a small non-negative tile row (0 .. map_height_tiles - 8).
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_view_window_origin_y;

/* ----------------------------------------------------------------
 * Battle cursor world X (tile column the cursor points at) @ 0x53AB1
 *
 * Runtime cursor state, not a constant. Zero at load (BSS); first set
 * by battle/chapter init + save-load (engine init loads it from the
 * map header via MOVZX byte->dword), then incremented/decremented by
 * the cursor-move, walk-step and pan functions as the cursor traverses
 * the map. uint32 matches the DWORD access width seen in every
 * reader/writer (INC/CMP/MOV dword ptr [0x53AB1]); range is a small
 * non-negative tile column (0 .. map_width_tiles - 1).
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_cursor_world_x;

/* ----------------------------------------------------------------
 * Battle cursor world Y (tile row the cursor points at) @ 0x53AB5
 *
 * Runtime cursor state, not a constant. Zero at load (BSS); first set
 * by battle/chapter init + save-load (engine init / chapter-end
 * handlers store it, init zeroes the whole cursor/window block at
 * 0x53AA9..0x53ABD), then incremented/decremented by the cursor-move,
 * walk-step and pan functions as the cursor traverses the map. uint32
 * matches the DWORD access width seen in every reader/writer
 * (DEC/INC/CMP/MOV dword ptr [0x53AB5]); range is a small
 * non-negative tile row (0 .. map_height_tiles - 1).
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_cursor_world_y;

/* ----------------------------------------------------------------
 * Battle cursor screen X (cursor column within the 13-tile viewport) @ 0x53AB9
 *
 * Runtime cursor state, not a constant. SIGNED int: the move handlers
 * fd2_cursor_move_left/right compare it against the viewport edges with
 * SIGNED branches (JGE/JLE), and it DOES go negative when the cursor or a
 * walking char reaches the map's left edge -- once view_window_origin_x
 * hits 0 the walk-step inner branch keeps decrementing cursor_screen_x
 * past 0. Declaring it unsigned makes those edge comparisons unsigned
 * (JAE/JB), which mis-classifies a negative column as a huge value and
 * takes the wrong scroll-vs-inner-step branch.
 *
 * Zero at load (BSS); first set by engine init / save-load (MOVZX
 * byte->dword from the map header at 0x1040C), then INC/DEC by the
 * cursor-move and walk-step functions. Width is dword (every access is
 * dword ptr [0x53AB9]).
 * ---------------------------------------------------------------- */
int data_fd2_battle_cursor_screen_x;

/* ----------------------------------------------------------------
 * Battle cursor screen Y (cursor row within the on-screen viewport) @ 0x53ABD
 *
 * Runtime cursor state, not a constant. SIGNED int: the move handlers
 * fd2_cursor_move_up/down compare it against the top/bottom viewport edges
 * with SIGNED branches (JGE/JLE; e.g. < 2 to scroll up, <= 5 to step down),
 * and it DOES go negative in a cinematic walk that reaches the map top --
 * once view_window_origin_y hits 0 the walk-step scroll branch is disabled
 * (its origin_y != 0 guard fails) so the inner branch keeps decrementing
 * cursor_screen_y past 0. The chapter-1 prologue walk-up does exactly this;
 * with an unsigned declaration the subsequent dialog camera pan reads the
 * negative row as a huge value, mis-takes the scroll branch in
 * fd2_cursor_move_down and scrolls the view to the map bottom (the prologue
 * "scene jumps back to the bottom" bug). Signed is required for a faithful
 * match to the original's JGE/JLE.
 *
 * Zero at load (BSS); cleared to 0 by fd2_init_battle_state_for_chapter
 * (0x2064B) and set from the save header via MOVZX byte->dword in save-load
 * (pBuf[0x30CB] at 0x10415), then INC/DEC by the cursor-move and walk-step
 * functions. Width is dword (every access is dword ptr [0x53ABD]).
 * ---------------------------------------------------------------- */
int data_fd2_battle_cursor_screen_y;
