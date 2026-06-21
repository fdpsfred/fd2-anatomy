/*
 * aniwalk.c — field walk-step + status/menu panel slide-step animations
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_tick_tile_event_animations @ 0x12263
 *
 * Advance the per-tile animation frame counter for every consumed
 * event tile so a just-consumed tile (opened chest / picked-up item /
 * triggered event) redraws in its "open" / "empty" sprite frame.
 *
 * Invoked once right after an event tile is marked consumed -- NOT a
 * per-frame tick. Callers: field pickup, enemy event-tile action,
 * scripted chapter pickups, and load-save engine init.
 *
 * Scans the whole battle map grid; for each event tile
 * ((attr & 0x60) == 0x20) whose consumed flag is set, bumps the +4
 * word (frame counter) and zeroes the +6 byte (phase flag) in the
 * tile-map record. The +4/+6 writes land in the next tile's record by
 * design (intended vendor stride; see fd2_obfuscate_battle_tile_map).
 * ---------------------------------------------------------------- */
void fd2_tick_tile_event_animations(void)
{
    int row;
    int col;
    uint8 tile_buf[8];
    uint16 tile_idx_combined;
    uint8 tile_attr_flags;
    uint8 *anim_ptr;

    for (row = 0; row < (int)data_fd2_battle_map_height_tiles; row++) {
        for (col = 0; col < (int)data_fd2_battle_map_width_tiles; col++) {
            fd2_read_tile_attribute_at_pos(col, row, (uint32)tile_buf);
            tile_idx_combined = *(uint16 *)(tile_buf + 2);
            tile_attr_flags = tile_buf[4];
            if ((tile_attr_flags & 0x60) == 0x20 &&
                *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr +
                           tile_idx_combined) != 0) {
                anim_ptr = (uint8 *)(data_fd2_battle_tile_map_ptr +
                    (data_fd2_battle_map_width_tiles * row + col) * 4 + 4);
                *(uint16 *)anim_ptr += 1;
                *(uint8 *)(anim_ptr + 2) = 0;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_walk_step_down @ 0x12EAA
 *
 * Animated 6-frame walk step DOWN (+1 y) for runtime_char[char_idx].
 * Sets facing = 0 (south). Scrolls viewport if needed.
 * Shared epilogue calls fd2_check_tile_event_post_action.
 * ---------------------------------------------------------------- */
void fd2_walk_step_down(uint32 char_idx)
{
    uint8 *pChar;
    uint32 saved_pos_y;
    uint32 scroll_delta_y;
    uint32 scroll_window_flag;
    uint32 blit_offset;
    uint32 composite_height;
    int frame;

    scroll_delta_y = 0;
    scroll_window_flag = 0;
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    saved_pos_y = (uint32)pChar[1];
    pChar[3] = 0;
    blit_offset = 0x8088;
    data_fd2_battle_walk_anim_y_scroll_rows = 0;

    if ((int)(saved_pos_y - data_fd2_battle_view_window_origin_y) <= 5 ||
        data_fd2_battle_map_height_tiles -
            data_fd2_battle_view_window_max_y ==
            data_fd2_battle_view_window_origin_y) {
        data_fd2_battle_cursor_screen_y++;
    } else {
        scroll_delta_y = 0x720;
        scroll_window_flag = 1;
    }

    for (frame = 1; frame < 7; frame++) {
        fd2_tick_tutorial_progress_with_sfx(char_idx);
        fd2_update_palette_cycle_anim();
        pChar[4] = (uint8)frame;
        fd2_tick_chapter_palette_animation();
        data_fd2_battle_compose_walk_step_y_sub_pixel_offset +=
            scroll_delta_y;
        data_fd2_battle_walk_anim_y_scroll_rows += scroll_window_flag;
        composite_height = (scroll_delta_y == 0) ? 8 : 9;
        fd2_composite_battle_tile_map(
            data_fd2_large_game_state_buffer_ptr + 0x8088,
            0x1C8, 0xD, composite_height,
            data_fd2_battle_view_window_origin_x,
            data_fd2_battle_view_window_origin_y);
        fd2_composite_all_chars_overlay();
        blit_offset += scroll_delta_y;
        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + blit_offset,
            0x1C8, 0x138, 0xC0);
        fd2_wait_one_bios_tick();
    }

    saved_pos_y++;
    pChar[1] = (uint8)saved_pos_y;
    data_fd2_battle_view_window_origin_y += scroll_window_flag;
    data_fd2_battle_cursor_world_y++;
    pChar[4] = 0;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;
    data_fd2_battle_walk_anim_y_scroll_rows = 0;

    fd2_check_tile_event_post_action(
        data_fd2_battle_cursor_world_x,
        data_fd2_battle_cursor_world_y, 0);
}

/* ----------------------------------------------------------------
 * fd2_walk_step_left @ 0x1300D
 *
 * Animated 6-frame walk step LEFT (-1 x) for runtime_char[char_idx].
 * Sets facing = 1 (west). Scrolls viewport if needed.
 * Shared epilogue with walk_step_down.
 * ---------------------------------------------------------------- */
void fd2_walk_step_left(uint32 char_idx)
{
    uint8 *pChar;
    uint32 saved_pos_x;
    uint32 scroll_delta_x;
    uint32 scroll_window_flag;
    uint32 blit_offset;
    int frame;

    scroll_delta_x = 0;
    scroll_window_flag = 0;
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    saved_pos_x = (uint32)pChar[0];
    pChar[3] = 1;
    blit_offset = 0x8088;
    data_fd2_battle_walk_anim_x_scroll_offset = 6;

    if ((int)(saved_pos_x - data_fd2_battle_view_window_origin_x) < 2 &&
        data_fd2_battle_view_window_origin_x != 0) {
        scroll_delta_x = 0xFFFFFFFC;
        scroll_window_flag = 0xFFFFFFFF;
    } else {
        data_fd2_battle_cursor_screen_x--;
    }

    for (frame = 1; frame < 7; frame++) {
        fd2_tick_tutorial_progress_with_sfx(char_idx);
        fd2_update_palette_cycle_anim();
        pChar[4] = (uint8)frame;
        fd2_tick_chapter_palette_animation();
        data_fd2_battle_compose_left_edge_clip_offset = 0x18;
        data_fd2_battle_compose_walk_step_y_sub_pixel_offset +=
            scroll_delta_x;
        data_fd2_battle_walk_anim_x_scroll_offset += scroll_window_flag;
        fd2_composite_battle_tile_map(
            data_fd2_large_game_state_buffer_ptr + 0x8070,
            0x1C8, 0xE, 8,
            data_fd2_battle_view_window_origin_x - 1,
            data_fd2_battle_view_window_origin_y);
        data_fd2_battle_compose_left_edge_clip_offset = 0;
        fd2_composite_all_chars_overlay();
        blit_offset += scroll_delta_x;
        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + blit_offset,
            0x1C8, 0x138, 0xC0);
        fd2_wait_one_bios_tick();
    }

    saved_pos_x--;
    pChar[0] = (uint8)saved_pos_x;
    data_fd2_battle_view_window_origin_x += scroll_window_flag;
    data_fd2_battle_cursor_world_x--;
    pChar[4] = 0;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;
    data_fd2_battle_walk_anim_x_scroll_offset = 0;

    fd2_check_tile_event_post_action(
        data_fd2_battle_cursor_world_x,
        data_fd2_battle_cursor_world_y, 0);
}

/* ----------------------------------------------------------------
 * fd2_walk_step_up @ 0x13185
 *
 * Animated 6-frame walk step UP (-1 y) for runtime_char[char_idx].
 * Sets facing = 2 (north). Scrolls viewport if needed.
 * ---------------------------------------------------------------- */
void fd2_walk_step_up(uint32 char_idx)
{
    uint8 *pChar;
    uint32 saved_pos_y;
    uint32 scroll_delta_y;
    uint32 scroll_window_flag;
    uint32 blit_offset;
    int frame;

    scroll_delta_y = 0;
    scroll_window_flag = 0;
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    saved_pos_y = (uint32)pChar[1];
    pChar[3] = 2;
    blit_offset = 0x8088;

    if (data_fd2_battle_view_window_origin_y != 0) {
        data_fd2_battle_walk_anim_y_scroll_rows = 6;
    } else {
        data_fd2_battle_walk_anim_y_scroll_rows = 0;
    }

    if ((int)(saved_pos_y - data_fd2_battle_view_window_origin_y) < 2 &&
        data_fd2_battle_view_window_origin_y != 0) {
        scroll_delta_y = 0xFFFFF8E0;
        scroll_window_flag = 0xFFFFFFFF;
    } else {
        data_fd2_battle_cursor_screen_y--;
    }

    for (frame = 1; frame < 7; frame++) {
        fd2_tick_tutorial_progress_with_sfx(char_idx);
        fd2_update_palette_cycle_anim();
        pChar[4] = (uint8)frame;
        fd2_tick_chapter_palette_animation();
        data_fd2_battle_compose_walk_step_y_sub_pixel_offset +=
            scroll_delta_y;
        data_fd2_battle_walk_anim_y_scroll_rows += scroll_window_flag;

        if (data_fd2_battle_view_window_origin_y == 0) {
            fd2_composite_battle_tile_map(
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, 0xD, 9,
                data_fd2_battle_view_window_origin_x, 0);
        } else {
            data_fd2_battle_compose_parallax_scroll_y_rows = 0x18;
            fd2_composite_battle_tile_map(
                data_fd2_large_game_state_buffer_ptr + 0x55C8,
                0x1C8, 0xD, 9,
                data_fd2_battle_view_window_origin_x,
                data_fd2_battle_view_window_origin_y - 1);
            data_fd2_battle_compose_parallax_scroll_y_rows = 0;
        }

        fd2_composite_all_chars_overlay();
        blit_offset += scroll_delta_y;
        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + blit_offset,
            0x1C8, 0x138, 0xC0);
        fd2_wait_one_bios_tick();
    }

    saved_pos_y--;
    pChar[1] = (uint8)saved_pos_y;
    data_fd2_battle_view_window_origin_y += scroll_window_flag;
    data_fd2_battle_cursor_world_y--;
    pChar[4] = 0;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;
    data_fd2_battle_walk_anim_y_scroll_rows = 0;

    fd2_check_tile_event_post_action(
        data_fd2_battle_cursor_world_x,
        data_fd2_battle_cursor_world_y, 0);
}

/* ----------------------------------------------------------------
 * fd2_walk_step_right @ 0x13315
 *
 * Animated 6-frame walk step RIGHT (+1 x) for runtime_char[char_idx].
 * Sets facing = 3 (east). Scrolls viewport if needed.
 * ---------------------------------------------------------------- */
void fd2_walk_step_right(uint32 char_idx)
{
    uint8 *pChar;
    uint32 saved_pos_x;
    uint32 scroll_delta_x;
    uint32 scroll_window_flag;
    uint32 blit_offset;
    uint32 composite_width;
    int frame;

    scroll_delta_x = 0;
    scroll_window_flag = 0;
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    saved_pos_x = (uint32)pChar[0];
    pChar[3] = 3;
    blit_offset = 0x8088;
    data_fd2_battle_walk_anim_x_scroll_offset = 0;

    if ((int)(saved_pos_x - data_fd2_battle_view_window_origin_x) <= 0xA
        || data_fd2_battle_map_width_tiles -
               data_fd2_battle_view_window_max_x ==
               data_fd2_battle_view_window_origin_x) {
        data_fd2_battle_cursor_screen_x++;
    } else {
        scroll_delta_x = 4;
        scroll_window_flag = 1;
    }

    for (frame = 1; frame < 7; frame++) {
        fd2_tick_tutorial_progress_with_sfx(char_idx);
        fd2_update_palette_cycle_anim();
        pChar[4] = (uint8)frame;
        fd2_tick_chapter_palette_animation();
        data_fd2_battle_compose_walk_step_y_sub_pixel_offset +=
            scroll_delta_x;
        data_fd2_battle_walk_anim_x_scroll_offset += scroll_window_flag;
        composite_width = (scroll_delta_x == 0) ? 0xD : 0xE;
        fd2_composite_battle_tile_map(
            data_fd2_large_game_state_buffer_ptr + 0x8088,
            0x1C8, composite_width, 8,
            data_fd2_battle_view_window_origin_x,
            data_fd2_battle_view_window_origin_y);
        fd2_composite_all_chars_overlay();
        blit_offset += scroll_delta_x;
        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + blit_offset,
            0x1C8, 0x138, 0xC0);
        fd2_wait_one_bios_tick();
    }

    saved_pos_x++;
    pChar[0] = (uint8)saved_pos_x;
    data_fd2_battle_view_window_origin_x += scroll_window_flag;
    data_fd2_battle_cursor_world_x++;
    pChar[4] = 0;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;
    data_fd2_battle_walk_anim_x_scroll_offset = 0;

    fd2_check_tile_event_post_action(
        data_fd2_battle_cursor_world_x,
        data_fd2_battle_cursor_world_y, 0);
}

/* ----------------------------------------------------------------
 * fd2_walk_path_animation_loop @ 0x13488
 *
 * Animate character walking along a direction-byte path.
 * Each byte: 0=down, 1=left, 2=up, 3=right.
 * ---------------------------------------------------------------- */
void fd2_walk_path_animation_loop(uint32 char_idx, uint32 path_buf,
                                   uint32 step_count)
{
    int step_iter;
    uint32 step_dir;

    for (step_iter = 0; step_iter < (int)step_count; step_iter++) {
        step_dir = (uint32)*(uint8 *)(path_buf + step_iter);
        if (step_dir == 0) {
            fd2_walk_step_down(char_idx);
        } else if (step_dir == 1) {
            fd2_walk_step_left(char_idx);
        } else if (step_dir == 2) {
            fd2_walk_step_up(char_idx);
        } else {
            fd2_walk_step_right(char_idx);
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_slide_panel_step_left_main @ 0x1AF1E
 *
 * Party status overview wide (0xAA-byte) main panel, one slide-OUT
 * frame toward the left edge. Left-edge mirror of
 * fd2_slide_panel_step_right_main; driven by the OUTRO loop in
 * fd2_open_party_status_overview_screen (frame_idx counts 0xB->0),
 * while the right variant drives the INTRO (slide-in).
 *
 * Copies 0x75 (117) rows, 0xAA bytes wide, from src_buffer into
 * data_fd2_large_game_state_buffer_ptr (both at row-0 base 0x2E40).
 * For frame_idx < 5 the dst x descends from 0x4B toward 0 by 0x32 per
 * frame; once the panel runs past x=0 the copy width is left-clipped
 * (row_bytes shrinks, src_x advances, dst_x pins to 0). For
 * frame_idx >= 5 the panel sits stationary at x = 0x4B.
 * ---------------------------------------------------------------- */
void fd2_slide_panel_step_left_main(uint32 src_buffer, uint32 frame_idx)
{
    int row_bytes;
    int src_x;
    int dst_x;
    int x_shift;
    uint32 src_ptr;
    uint32 dst_off;
    int row;

    row_bytes = 0xAA;
    src_x = 0x4B;
    dst_x = 0x4B;

    if ((int)frame_idx < 5) {
        x_shift = (4 - (int)frame_idx) * 0x32;
        dst_x = 0x4B - x_shift;
        if (dst_x < 0) {
            row_bytes += dst_x;
            src_x -= dst_x;
            dst_x = 0;
        }
    }

    src_ptr = src_buffer + src_x + 0x2E40;
    dst_off = dst_x + 0x2E40;

    for (row = 0; row < 0x75; row++) {
        memmove((void *)(data_fd2_large_game_state_buffer_ptr + dst_off),
                (void *)src_ptr, (uint32)row_bytes);
        src_ptr += 0x140;
        dst_off += 0x140;
    }
}

/* ----------------------------------------------------------------
 * fd2_slide_panel_step_right_main @ 0x1AF99
 *
 * Party status overview wide (0xAA-byte) main panel, one slide-IN
 * frame entering from the right edge. Right-edge mirror of
 * fd2_slide_panel_step_left_main; driven by the INTRO loop in
 * fd2_open_party_status_overview_screen (frame_idx counts 0->0xB),
 * while the left variant drives the OUTRO (slide-out).
 *
 * Copies 0x75 (117) rows, 0xAA bytes wide, from src_buffer into
 * data_fd2_large_game_state_buffer_ptr. The source always starts at
 * row-0 offset 0x2E8B (= src x 0x4B + 0xAA, i.e. the panel's right
 * portion) and never advances, because the right edge clips by
 * shrinking the copy width only. For frame_idx <= 4 the dst x
 * descends from 0xF5 toward 0x4B by 0x32 per frame; while the panel
 * right edge (dst_x + 0xAA) runs past the screen width 0x140 the copy
 * width is right-clipped (row_bytes = 0x140 - dst_x). For
 * frame_idx >= 5 the panel sits stationary at dst x = 0x4B.
 * ---------------------------------------------------------------- */
void fd2_slide_panel_step_right_main(uint32 src_buffer, uint32 frame_idx)
{
    int row_bytes;
    int dst_x;
    int x_shift;
    uint32 src_ptr;
    uint32 dst_off;
    int row;

    row_bytes = 0xAA;
    if ((int)frame_idx > 4) {
        dst_x = 0x4B;
    } else {
        x_shift = (4 - (int)frame_idx) * 0x32;
        dst_x = x_shift + 0x4B;
        if (x_shift + 0xF5 > 0x140) {
            row_bytes = 0x140 - dst_x;
        }
    }

    src_ptr = src_buffer + 0x2E8B;
    dst_off = dst_x + 0x2E40;

    for (row = 0; row < 0x75; row++) {
        memmove((void *)(data_fd2_large_game_state_buffer_ptr + dst_off),
                (void *)src_ptr, (uint32)row_bytes);
        src_ptr += 0x140;
        dst_off += 0x140;
    }
}

/* ----------------------------------------------------------------
 * fd2_slide_panel_step_top_small @ 0x1B019
 *
 * Party Status Overview screen: one frame of the top narrow panel
 * slide-in from above (0x66 bytes wide, up to 0x24 rows high),
 * settling at y = 0x13. Driven by fd2_open_party_status_overview_screen
 * (intro + outro). Frames < 3 are skipped (main panel not yet in place);
 * frames 3..7 descend by 6 rows/frame with top-edge clipping; frame >= 8
 * is fully settled. Copies row_count rows from src_buffer into
 * data_fd2_large_game_state_buffer_ptr.
 * ---------------------------------------------------------------- */
void fd2_slide_panel_step_top_small(uint32 src_buffer, uint32 frame_idx)
{
    int row_count;
    int src_y;
    int dst_y;
    int y_shift;
    uint32 src_ptr;
    uint32 dst_off;
    int row;

    row_count = 0x11;
    src_y = 0x13;

    if ((int)frame_idx < 3) return;

    if ((int)frame_idx > 7) {
        dst_y = 0x13;
    } else {
        y_shift = (4 - ((int)frame_idx - 3)) * 6;
        dst_y = 0x13 - y_shift;
        if (dst_y < 0) {
            src_y = 0x13 - dst_y;
            row_count += dst_y;
            dst_y = 0;
        }
    }

    src_ptr = src_buffer + (uint32)src_y * 0x140 + 0x6D;
    dst_off = (uint32)dst_y * 0x140 + 0x6D;

    for (row = 0; row < row_count; row++) {
        memmove((void *)(data_fd2_large_game_state_buffer_ptr + dst_off),
                (void *)src_ptr, 0x66);
        src_ptr += 0x140;
        dst_off += 0x140;
    }
}

/* ----------------------------------------------------------------
 * fd2_slide_panel_step_bottom_main @ 0x1B0AD
 *
 * Party Status Overview screen: one frame of the bottom wide ("main")
 * panel sliding up from below into its settled position at y = 0x9B
 * (0xAA bytes wide, up to 0x10 rows high). Driven by
 * fd2_open_party_status_overview_screen (intro + outro), one of the
 * four per-frame panel steps. Frames < 5 are skipped (other panels not
 * yet in place). Frames 5..9 rise toward y = 0x9B by 9 rows/frame; the
 * first slide frame (frame 5) clips off the screen bottom (row count
 * goes non-positive, so nothing is drawn that frame). Frame >= 10 is
 * fully settled. Source row 0 is at src_buffer + 0xC20B; copies
 * row_count rows into data_fd2_large_game_state_buffer_ptr.
 * ---------------------------------------------------------------- */
void fd2_slide_panel_step_bottom_main(uint32 src_buffer, uint32 frame_idx)
{
    int row_count;
    int dst_y;
    int y_shift;
    uint32 src_ptr;
    uint32 dst_off;
    int row;

    row_count = 0x10;

    if ((int)frame_idx < 5) return;

    if ((int)frame_idx > 9) {
        dst_y = 0x9B;
    } else {
        y_shift = (4 - ((int)frame_idx - 5)) * 9;
        dst_y = y_shift + 0x9B;
        if (dst_y + 0x10 > 200) {
            row_count = dst_y - 200;
        }
    }

    src_ptr = src_buffer + 0xC20B;
    dst_off = (uint32)dst_y * 0x140 + 0x4B;

    for (row = 0; row < row_count; row++) {
        memmove((void *)(data_fd2_large_game_state_buffer_ptr + dst_off),
                (void *)src_ptr, 0xAA);
        src_ptr += 0x140;
        dst_off += 0x140;
    }
}

/* ----------------------------------------------------------------
 * fd2_slide_panel_step_bottom_small @ 0x1B14B
 *
 * Bottom narrow strip slide-in from below (0x3F-wide, 0xF rows).
 * ---------------------------------------------------------------- */
void fd2_slide_panel_step_bottom_small(uint32 src_buffer, uint32 frame_idx)
{
    int row_count;
    int dst_y;
    int y_shift;
    uint32 src_ptr;
    uint32 dst_off;
    int row;

    row_count = 0xF;

    if ((int)frame_idx < 8) return;

    if ((int)frame_idx > 0xC) {
        dst_y = 0xAC;
    } else {
        y_shift = (4 - ((int)frame_idx - 8)) * 4;
        dst_y = y_shift + 0xAC;
        if (dst_y + 0xF > 200) {
            row_count = dst_y - 200;
        }
    }

    src_ptr = src_buffer + 0xD781;
    dst_off = (uint32)dst_y * 0x140 + 0x81;

    for (row = 0; row < row_count; row++) {
        memmove((void *)(data_fd2_large_game_state_buffer_ptr + dst_off),
                (void *)src_ptr, 0x3F);
        src_ptr += 0x140;
        dst_off += 0x140;
    }
}

/* ----------------------------------------------------------------
 * fd2_slide_panel_up_partial_step @ 0x1839B
 *
 * Copy up to 102 panel rows from src to dst with bottom clipping.
 * ---------------------------------------------------------------- */
void fd2_slide_panel_up_partial_step(uint32 y_offset,
                                      uint32 dst_workspace,
                                      uint32 src_buffer)
{
    int row_count;
    int row;
    uint32 row_stride;

    row_count = 0x66;
    if ((int)(y_offset + 0x66) >= 200) {
        row_count = 200 - (int)y_offset;
    }

    for (row = 0; row < row_count; row++) {
        row_stride = (uint32)row * 0x140;
        memmove((void *)(dst_workspace + 5 + y_offset * 0x140 + row_stride),
                (void *)(src_buffer + 0x7585 + row_stride),
                0x136);
    }
}

/* ----------------------------------------------------------------
 * fd2_slide_panel_down_step @ 0x1974C
 *
 * Render one full frame of the chapter-portrait dialog-panel slide.
 *   1. Restore dst_workspace from the clean background snapshot.
 *   2. Copy up to 0x56 panel rows (310 bytes wide) from
 *      src_buffer + 0x8C05 (composed dialog buffer at y=0x70, x=5)
 *      into dst_workspace at screen row y_offset, x=5, clipping the
 *      row count to the screen bottom (row 200).
 *   3. Blit the whole 320x200 workspace to mode-13h VRAM (0xA0000).
 *
 * Direction-agnostic: the caller drives y_offset per frame
 * (slide-in y descends toward 0x70, slide-out y ascends off-screen).
 * Unlike the sibling fd2_slide_panel_up_partial_step, this one owns
 * the background restore and VGA flush, so it is a full-frame step.
 * ---------------------------------------------------------------- */
void fd2_slide_panel_down_step(uint32 y_offset,
                                uint32 dst_workspace,
                                uint32 src_buffer)
{
    int row_count;
    int row;
    uint32 row_stride;

    memmove((void *)dst_workspace,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr,
            0xFA00);

    row_count = 0x56;
    if ((int)(y_offset + 0x56) >= 200) {
        row_count = 200 - (int)y_offset;
    }

    for (row = 0; row < row_count; row++) {
        row_stride = (uint32)row * 0x140;
        memmove((void *)(dst_workspace + 5 + y_offset * 0x140 + row_stride),
                (void *)(src_buffer + 0x8C05 + row_stride),
                0x136);
    }

    memmove((void *)0xA0000, (void *)dst_workspace, 0xFA00);
}

/* ----------------------------------------------------------------
 * fd2_play_status_screen_outro_step @ 0x18409  (6 callers)
 *
 * Render one frame of the 12-frame status/menu-panel slide animation.
 * frame_idx runs 0..0xB; callers drive it 0xB->0 (intro) or 0->0xB (outro).
 *
 * Steps:
 *   1. Reset workspace to the clean background snapshot.
 *   2. Left panel  — slides horizontally: x = 5 while frame < 6, else
 *      slides left as frame increases.
 *   3. Right panel — slides vertically; drawn only while frame <= 8
 *      (frame >= 9 is fully off-screen and skipped). y = 7 while
 *      frame <= 2, else slides up over frames 3..8.
 *   4. Middle panel — slides down (drawn only while frame < 6).
 *   5. Composite the workspace to mode-13h VRAM (0xA0000).
 *
 * Cdecl, 4 stack params; void return. The binary's __CHK(0x18)
 * stack-probe prologue is compiler-injected and not part of the source.
 * Callers (8 sites in 6 functions; each open-screen routine drives this
 * both intro 0xB->0 and outro 0->0xB): fd2_open_char_status_screen,
 * fd2_open_status_screen_with_slide_in, fd2_inventory_selection_modal_
 * dispatch, fd2_equip_unequip_inventory_menu, fd2_spell_selection_menu_
 * main, fd2_run_recruitment_or_branch_screen.
 * ---------------------------------------------------------------- */
void fd2_play_status_screen_outro_step(uint32 frame_idx,
                                        uint32 workspace,
                                        uint32 src_buffer,
                                        int snapshot_b)
{
    uint32 x_left;
    uint32 y_right;

    memmove((void *)workspace, (void *)snapshot_b, 0xFA00);

    if ((int)frame_idx < 6) {
        x_left = 5;
    } else {
        x_left = 5 - (frame_idx * 0x10 - 0x60);
    }
    fd2_paint_status_panel_layer_left(x_left, workspace, src_buffer);

    if ((int)frame_idx < 9 && (int)frame_idx > 2) {
        y_right = 7 - (frame_idx * 0x10 - 0x30);
        fd2_paint_status_panel_layer_right(y_right, workspace, src_buffer);
    } else if ((int)frame_idx <= 2) {
        y_right = 7;
        fd2_paint_status_panel_layer_right(y_right, workspace, src_buffer);
    }

    if ((int)frame_idx < 6) {
        fd2_slide_panel_up_partial_step(frame_idx * 0x10 + 0x5E,
            workspace, src_buffer);
    }

    memmove((void *)0xA0000, (void *)workspace, 0xFA00);
}

/* ----------------------------------------------------------------
 * Walk-step composite left-edge clip offset @ 0x53AED  (.object2, zero-init)
 *
 * Per-frame X clip marker added into the battle tile-map composite source
 * offset. fd2_walk_step_left sets it to 0x18 just before each composite
 * pass (clip the leftmost 24px column while the +1-column-wide map slides in)
 * and clears it to 0 right after, so the static image is zero.
 * Read by fd2_composite_battle_tile_map as a dword added to the source
 * offset alongside the sub-pixel/parallax offsets.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_compose_left_edge_clip_offset;

/* ----------------------------------------------------------------
 * Walk-step composite parallax scroll Y-row count @ 0x53AF1  (.object2, zero-init)
 *
 * Top-margin row count for the scroll-up battle tile-map composite.
 * fd2_walk_step_up sets it to 0x18 just before each composite pass
 * (scroll-up render uses a +1 top-margin row from the lower workspace
 * position) and clears it to 0 right after, so the static image is zero.
 * Read by fd2_composite_battle_tile_map as a dword multiplied by 0x1C8
 * (dst stride) and added into the source/dst offset for parallax chapters.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_compose_parallax_scroll_y_rows;

/* ----------------------------------------------------------------
 * Walk-step composite Y sub-pixel scroll offset @ 0x53AF5  (.object2, zero-init)
 *
 * Cumulative sub-pixel Y scroll accumulator for the smooth walk-step slide.
 * Each of fd2_walk_step_down/left/up/right adds the per-frame scroll delta
 * (0x720) into it once per slide frame (6 frames) and clears it to 0 after
 * the step completes, so the static image is zero.
 * Read by fd2_composite_battle_tile_map as a dword added into the background
 * source offset alongside the left-edge clip / parallax-scroll offsets.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_compose_walk_step_y_sub_pixel_offset;

/* ----------------------------------------------------------------
 * Walk-step horizontal (X) parallax scroll offset @ 0x53B07  (.object2, zero-init)
 *
 * Cumulative sub-pixel X scroll accumulator for the smooth walk-step slide.
 * fd2_walk_step_left and fd2_walk_step_right seed it to 6 before the slide
 * loop, add the per-frame window-scroll delta (-1 / 0 / +1) into it each of
 * the 6 slide frames, and clear it to 0 after the step completes, so the
 * static image is zero.
 * Read by fd2_composite_battle_tile_map for the extra-wide parallax chapters
 * (0x11/0x15/0x16/0x1B): the value is signed-divided by 2 (asm uses the
 * SAR/SUB/SAR signed /2 idiom) and added into the background source offset,
 * so the stored value is a signed int and does take negative values via the
 * left-scroll path.
 * ---------------------------------------------------------------- */
int data_fd2_battle_walk_anim_x_scroll_offset;

/* ----------------------------------------------------------------
 * Walk-step vertical (Y) parallax scroll row counter @ 0x53B0B  (.object2, zero-init)
 *
 * Cumulative row-scroll counter for the smooth walk-step slide, the Y/row
 * counterpart of data_fd2_battle_walk_anim_x_scroll_offset.
 * fd2_walk_step_down clears it to 0 before the slide loop and adds the
 * per-frame window-scroll flag (0 / +1) into it each of the 6 slide frames;
 * fd2_walk_step_up seeds it to 6 and adds the per-frame delta (-1 / 0) each
 * frame. Both clear it to 0 after the step completes, so the static image is
 * zero.
 * Read by fd2_composite_battle_tile_map for the extra-wide parallax chapters
 * (0x11/0x15/0x16/0x1B): the value is signed-divided by 3 (asm uses the
 * MOV/SAR EDX,0x1f + IDIV signed-divide idiom), multiplied by the row stride,
 * and added into the background source offset, so the stored value is a
 * signed int. All accesses are dword (32-bit).
 * ---------------------------------------------------------------- */
int data_fd2_battle_walk_anim_y_scroll_rows;
