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
 * Advance per-tile animation counter for all consumed event tiles.
 * Called once per frame from main composite.
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
 * Party status overview left panel slide-in single frame.
 * Copies src_buffer rows into large_game_state_buffer with
 * x-offset based on frame_idx (0..4 slide in, >=5 stationary).
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
 * Right panel slide-in (mirror of left_main).
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
 * Top narrow panel slide-in from above (0x66-wide, 0x24-high max).
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
 * Bottom main panel slide-in from below (0xAA-wide, 0x10 rows).
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
 * Restore background, copy panel rows, blit to VGA.
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