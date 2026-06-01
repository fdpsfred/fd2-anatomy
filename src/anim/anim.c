/*
 * anim.c — Animation tick functions: tile events, walk steps,
 *          slide panels, summon spells, tutorial progress.
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
 * fd2_tick_sprite_animation_step @ 0x2673F
 *
 * One tick of frame-paced sprite animation. Renders current frame,
 * advances tick counter, and moves to next frame when hold expires.
 * ---------------------------------------------------------------- */
void fd2_tick_sprite_animation_step(uint8 *p_frame_idx, uint8 *p_tick,
                                     int x, int y, uint32 atlas)
{
    uint32 frame_off;

    fd2_blit_indexed_sprite(atlas, (uint32)*p_frame_idx, x, y, -1);
    frame_off = *(uint32 *)(atlas + (uint32)*p_frame_idx * 4 + 8);
    (*p_tick)++;
    if (*p_tick == *(uint8 *)(atlas + frame_off + 6)) {
        *p_tick = 0;
        (*p_frame_idx)++;
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
            row_count = dst_y - 200 + 0x10;
            row_count = 200 - dst_y;
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
            row_count = 200 - dst_y;
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
 * fd2_tick_summon_spell_minor_animation_state @ 0x275D6
 *
 * Single-sprite minor animation state machine for summon spells.
 * Dispatch table entry #9 (last) at 0x523B9.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_spell_minor_animation_state(
    uint32 sprite_handle, uint32 sprite_atlas,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    (void)sprite_handle;

    if (state_code == 0) {
        data_fd2_battle_summon_minor_anim_alternating_blit_toggle = 0;
        data_fd2_battle_summon_minor_anim_state5_frame_counter = 1;
        return 0x14;
    }
    if (state_code == 3) return 0x3C;
    if (state_code == 6) return 0x14;

    if (state_code == 1 || state_code == 7) {
        if (data_fd2_battle_summon_minor_anim_alternating_blit_toggle
            == 0) {
            fd2_blit_indexed_sprite(
                sprite_atlas, 0, (int)origin_y,
                (int)row_stride, -1);
        }
        data_fd2_battle_summon_minor_anim_alternating_blit_toggle ^= 1;
        return 0;
    }

    if (state_code == 4) {
        fd2_blit_indexed_sprite(
            sprite_atlas, 0, (int)origin_y,
            (int)row_stride, -1);
        return 0;
    }

    if (state_code == 5) {
        fd2_blit_indexed_sprite(
            sprite_atlas,
            (int)data_fd2_battle_summon_minor_anim_state5_frame_counter
                / 2,
            (int)origin_y, (int)row_stride, -1);
        if (data_fd2_battle_summon_minor_anim_state5_frame_counter
            == 6) {
            fd2_play_sfx_with_handle(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                1, 1);
        } else if (
            data_fd2_battle_summon_minor_anim_state5_frame_counter
            == 0x24) {
            fd2_play_sfx_sample_from_bank(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                2, 1);
        }
        data_fd2_battle_summon_minor_anim_state5_frame_counter++;
        if (data_fd2_battle_summon_minor_anim_state5_frame_counter
                < 0x2C &&
            data_fd2_battle_summon_minor_anim_state5_frame_counter
                > 0x10) {
            return 1;
        }
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_d_3slot @ 0x272B8
 *
 * Variant-D 3-active-slot summon animation. Dispatch table #7.
 * 10 color row offsets, mod-2 frame toggle, mod-10 color rotation.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_d_3slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    uint32 local_offsets[10];
    int i;
    int done_flag;
    int frame;
    int color;
    uint8 *pChar;

    done_flag = 0;
    memcpy(local_offsets,
           data_fd2_animation_summon_variant_d_3slot_color_row_offsets,
           40);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 10; i++)
            local_offsets[i] += 0x82;
    }

    if (state_code == 0) {
        for (i = 0; i < 4; i++) {
            data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]
                = -3 * i;
            data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i]
                = i;
        }
        data_fd2_battle_summon_anim_variant_d_color_rotation_counter
            = 4;
        data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
        data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle
            = 0;
        return 2;
    }
    if (state_code == 3) return 0x20;
    if (state_code == 6) {
        data_fd2_battle_summon_anim_variant_d_terminate_flag = 1;
        return 0x10;
    }

    if (state_code == 2 || state_code == 5 || state_code == 8) {
        data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle =
            (data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle
             + 1) % 2;

        for (i = 0; i < 3; i++) {
            frame =
                data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i];
            color =
                data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i];

            if (frame >= 0 && frame < 5) {
                fd2_blit_indexed_sprite(
                    sprite_handle, frame,
                    (int)origin_y + (int)local_offsets[color],
                    (int)row_stride, -1);
            }

            if (data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle
                == 0) {
                if (frame == 1) {
                    if (i == 0)
                        fd2_play_sfx_with_handle(
                            data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                            1, 1);
                    else if (i == 1)
                        fd2_play_sfx_sample_from_bank(
                            data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                            1, 1);
                }
                data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]++;
                if (data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]
                    == 2)
                    done_flag = 1;
                if (data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]
                        == 7 &&
                    data_fd2_battle_summon_anim_variant_d_terminate_flag
                        == 0) {
                    data_fd2_battle_summon_anim_variant_d_color_rotation_counter =
                        (data_fd2_battle_summon_anim_variant_d_color_rotation_counter
                         + 1) % 10;
                    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i] =
                        (int32)data_fd2_battle_summon_anim_variant_d_color_rotation_counter;
                    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i]
                        = 0;
                }
            }
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_e_16slot @ 0x274B0
 *
 * Variant-E 16-slot summon animation. Simplest variant — no team
 * adjust, no color rotation, per-slot sprite base offsets.
 * Dispatch table #8.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_e_16slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    uint8 sprite_bases[16];
    int i;
    int done_flag;
    int frame;

    (void)caster_unit_id;
    memcpy(sprite_bases,
           data_fd2_animation_summon_variant_e_16slot_sprite_base_table,
           16);
    done_flag = 0;

    if (state_code == 0) {
        for (i = 0; i < 16; i++)
            data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i]
                = -2 * i;
        return 3;
    }
    if (state_code == 3) return 0x22;
    if (state_code == 6) return 2;

    if (state_code == 2 || state_code == 5) {
        for (i = 0; i < 16; i++) {
            frame =
                data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i];
            if (frame >= 0 && frame < 8) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    frame + (int)(uint32)sprite_bases[i],
                    (int)origin_y, (int)row_stride, -1);
            }
            if (frame == 0)
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                    1, 1);
            if (frame == 4)
                fd2_play_sfx_sample_from_bank(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                    2, 1);
            data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i]++;
            if (data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i]
                == 4)
                done_flag = 1;
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_a_6slot @ 0x269D3
 *
 * Variant-A 6-slot summon animation with RNG-based jitter.
 * Dispatch table #4. EAX tracking bug in decompiler corrected:
 * jitter uses fd2_advance_rng_state() return, not loop index.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_a_6slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    int32 local_offsets[10];
    int i;
    int done_flag;
    int frame;
    int color;
    uint32 rng_val;
    int remainder;
    uint8 *pChar;

    done_flag = 0;
    memcpy(local_offsets,
           data_fd2_battle_summon_anim_variant_a_10color_y_offset_table,
           40);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 10; i++)
            local_offsets[i] += 0x8F;
    }

    if (state_code == 0) {
        for (i = 0; i < 6; i++) {
            data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]
                = -2 * i;
            data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i]
                = i;
            rng_val = fd2_advance_rng_state();
            remainder = (int)(rng_val % 2);
            data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[i]
                = (uint8)((remainder << 3) - remainder);
        }
        data_fd2_battle_summon_anim_variant_a_color_rotation_counter
            = 6;
        data_fd2_battle_summon_anim_variant_a_terminate_flag = 0;
        return 2;
    }
    if (state_code == 3) return 0xC;
    if (state_code == 6) {
        data_fd2_battle_summon_anim_variant_a_terminate_flag = 1;
        return 8;
    }

    if (state_code == 2 || state_code == 5 || state_code == 8) {
        for (i = 0; i < 6; i++) {
            frame =
                data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i];
            color =
                data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i];

            if (frame >= 0 && frame < 7) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    (int)(uint32)data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[i]
                        + frame,
                    (int)origin_y + local_offsets[color],
                    (int)row_stride, -1);
            }
            if (frame == 0)
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                    1, 1);

            data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]++;
            if (data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]
                == 3)
                done_flag = 1;

            if (data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]
                    == 8 &&
                data_fd2_battle_summon_anim_variant_a_terminate_flag
                    == 0) {
                data_fd2_battle_summon_anim_variant_a_color_rotation_counter++;
                data_fd2_battle_summon_anim_variant_a_color_rotation_counter =
                    data_fd2_battle_summon_anim_variant_a_color_rotation_counter
                    % 10;
                data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i]
                    = (int32)data_fd2_battle_summon_anim_variant_a_color_rotation_counter;
                data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i]
                    = 0;
                rng_val = fd2_advance_rng_state();
                remainder = (int)(rng_val % 2);
                data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[i]
                    = (uint8)((remainder << 3) - remainder);
            }
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_anim_variant_b_6slot @ 0x26BFD
 *
 * Variant-B 6-slot summon animation with RNG jitter (0 or 6).
 * Dispatch table #5. Similar to variant_a but different timings,
 * 2-bucket SFX, and jitter multiplier 6.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_anim_variant_b_6slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    int32 local_offsets[10];
    int i;
    int done_flag;
    int frame;
    int color;
    uint32 rng_val;
    uint8 *pChar;

    done_flag = 0;
    memcpy(local_offsets,
           data_fd2_battle_summon_anim_variant_b_10color_y_offset_table,
           40);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 10; i++)
            local_offsets[i] += 0x8F;
    }

    if (state_code == 0) {
        for (i = 0; i < 6; i++) {
            data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]
                = -2 * i;
            data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i]
                = i;
            rng_val = fd2_advance_rng_state();
            data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[i]
                = (uint8)((rng_val % 2) * 6);
        }
        data_fd2_battle_summon_anim_variant_b_color_rotation_counter
            = 6;
        data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
        return 1;
    }
    if (state_code == 3) return 0xC;
    if (state_code == 6) {
        data_fd2_battle_summon_anim_variant_b_terminate_flag = 1;
        return 8;
    }

    if (state_code == 2 || state_code == 5 || state_code == 8) {
        for (i = 0; i < 6; i++) {
            frame =
                data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i];
            color =
                data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i];

            if (frame >= 0 && frame < 6) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    (int)(uint32)data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[i]
                        + frame,
                    (int)origin_y + local_offsets[color],
                    (int)row_stride, -1);
            }
            if (frame == 0) {
                if (i == 0)
                    fd2_play_sfx_with_handle(
                        data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                        1, 1);
                else if (i == 3)
                    fd2_play_sfx_sample_from_bank(
                        data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                        1, 1);
            }

            data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]++;
            if (data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]
                == 2)
                done_flag = 1;

            if (data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]
                    == 7 &&
                data_fd2_battle_summon_anim_variant_b_terminate_flag
                    == 0) {
                data_fd2_battle_summon_anim_variant_b_color_rotation_counter++;
                data_fd2_battle_summon_anim_variant_b_color_rotation_counter =
                    data_fd2_battle_summon_anim_variant_b_color_rotation_counter
                    % 10;
                data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i]
                    = (int32)data_fd2_battle_summon_anim_variant_b_color_rotation_counter;
                data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i]
                    = 0;
                rng_val = fd2_advance_rng_state();
                data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[i]
                    = (uint8)((rng_val % 2) * 6);
            }
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_spell_animation_state @ 0x26528
 *
 * Generic single-target summon animation. Dispatch table #2.
 * Plate comment had incorrect is_enemy condition for state {1,7}
 * and state 4 — assembly verified: NON-enemy does the tick/blit.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_spell_animation_state(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    int is_enemy;
    int done_flag;
    uint8 *pChar;

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    is_enemy = (pChar[6] == 0) ? 1 : 0;
    done_flag = 0;

    if (state_code == 0) {
        data_fd2_battle_summon_spell_anim_phase_byte = 0;
        data_fd2_battle_summon_spell_anim_aux_state_byte_unread = 0;
        data_fd2_battle_summon_spell_sprite_anim_tick_counter = 0;
        return 0x1D;
    }
    if (state_code == 3) {
        data_fd2_battle_summon_spell_anim_phase_byte = 0x10;
        return 0xC;
    }
    if (state_code == 6) {
        fd2_play_sfx_with_handle(
            data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 3, 1);
        data_fd2_battle_summon_spell_anim_phase_byte = 0xA;
        return 0xA;
    }

    if ((state_code == 1 || state_code == 7) && !is_enemy) {
        if (data_fd2_battle_summon_spell_anim_phase_byte == 0xA
            && state_code == 1)
            data_fd2_battle_summon_spell_anim_phase_byte = 0xF;
        fd2_tick_sprite_animation_step(
            &data_fd2_battle_summon_spell_anim_phase_byte,
            &data_fd2_battle_summon_spell_sprite_anim_tick_counter,
            origin_y, row_stride, sprite_handle);
        return 0;
    }

    if (state_code == 2 || state_code == 8) {
        if (data_fd2_battle_summon_spell_anim_phase_byte == 7)
            fd2_play_sfx_with_handle(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                1, 1);
        if (is_enemy) {
            if (data_fd2_battle_summon_spell_anim_phase_byte
                    == 0xA && state_code == 2)
                data_fd2_battle_summon_spell_anim_phase_byte
                    = 0xF;
            fd2_tick_sprite_animation_step(
                &data_fd2_battle_summon_spell_anim_phase_byte,
                &data_fd2_battle_summon_spell_sprite_anim_tick_counter,
                origin_y, row_stride, sprite_handle);
        }
        if (data_fd2_battle_summon_spell_anim_phase_byte == 0x10)
            fd2_blit_indexed_sprite(
                sprite_handle, 0x10, (int)origin_y,
                (int)row_stride, -1);
        return 0;
    }

    if (state_code == 4 && !is_enemy) {
        fd2_blit_indexed_sprite(
            sprite_handle, 0xF,
            (int)origin_y + 1 - (int)row_stride,
            (int)row_stride, -1);
        return 0;
    }

    if (state_code == 5) {
        if (is_enemy)
            fd2_blit_indexed_sprite(
                sprite_handle, 0xF,
                (int)origin_y - 1 - (int)row_stride,
                (int)row_stride, -1);
        fd2_blit_indexed_sprite(
            sprite_handle,
            (int)(uint32)data_fd2_battle_summon_spell_anim_phase_byte,
            (int)origin_y + 1 - (int)row_stride,
            (int)row_stride, -1);
        data_fd2_battle_summon_spell_anim_phase_byte++;
        if (data_fd2_battle_summon_spell_anim_phase_byte == 0x11) {
            fd2_play_sfx_with_handle(
                data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                2, 1);
            done_flag = 1;
        } else if (data_fd2_battle_summon_spell_anim_phase_byte
                   == 0x12) {
            data_fd2_battle_summon_spell_anim_phase_byte = 0x10;
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_spell_main_animation_state @ 0x26795
 *
 * Main 12-slot summon animation. Dispatch table #3.
 * 12 color rotation mod 12, odd/even frame toggle,
 * 3 rodata tables (y-offsets, v-offsets, sprite offsets).
 * ---------------------------------------------------------------- */
int fd2_tick_summon_spell_main_animation_state(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    int32 y_offsets[12];
    uint8 v_offsets[12];
    uint8 spr_offsets[12];
    int i;
    int done_flag;
    int frame;
    int color;
    uint8 *pChar;

    done_flag = 0;
    memcpy(y_offsets,
           data_fd2_battle_summon_main_anim_12slot_y_offset_table, 48);
    memcpy(v_offsets,
           data_fd2_battle_summon_main_anim_12color_v_offset_table, 12);
    memcpy(spr_offsets,
           data_fd2_battle_summon_main_anim_12color_sprite_offset_table,
           12);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 12; i++)
            y_offsets[i] += 0x14;
    }

    if (state_code == 0) {
        for (i = 0; i < 12; i++) {
            data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                = -2 * i;
            data_fd2_battle_summon_main_anim_12slot_color_idx_array[i]
                = i;
        }
        data_fd2_battle_summon_main_anim_color_rotation_counter = 12;
        data_fd2_battle_summon_main_anim_terminate_flag = 0;
        data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
        return 2;
    }
    if (state_code == 3) return 0x28;
    if (state_code == 6) {
        data_fd2_battle_summon_main_anim_terminate_flag = 1;
        return 0x14;
    }

    if (state_code == 2 || state_code == 5 || state_code == 8) {
        data_fd2_battle_summon_main_anim_odd_even_frame_toggle =
            (data_fd2_battle_summon_main_anim_odd_even_frame_toggle
             + 1) % 2;

        for (i = 0; i < 12; i++) {
            frame =
                data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i];
            if (frame >= 0 && frame < 0xB) {
                color =
                    data_fd2_battle_summon_main_anim_12slot_color_idx_array[i];
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    (int)(uint32)spr_offsets[color] + frame,
                    (int)origin_y + y_offsets[color]
                        - (int)(uint32)v_offsets[color]
                          * (int)row_stride,
                    (int)row_stride, -1);
            }

            if (data_fd2_battle_summon_main_anim_odd_even_frame_toggle
                == 0) {
                color =
                    data_fd2_battle_summon_main_anim_12slot_color_idx_array[i];
                if (data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                        == 0 &&
                    spr_offsets[color] != 0) {
                    fd2_play_sfx_with_handle(
                        data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                        2, 1);
                }
                data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]++;

                if (data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                    == 3) {
                    if (spr_offsets[color] == 0)
                        fd2_play_sfx_sample_from_bank(
                            data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                            1, 1);
                    done_flag = 1;
                }

                if (data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                        == 0xB &&
                    data_fd2_battle_summon_main_anim_terminate_flag
                        == 0) {
                    data_fd2_battle_summon_main_anim_color_rotation_counter++;
                    data_fd2_battle_summon_main_anim_color_rotation_counter =
                        data_fd2_battle_summon_main_anim_color_rotation_counter
                        % 12;
                    data_fd2_battle_summon_main_anim_12slot_color_idx_array[i]
                        = (int32)data_fd2_battle_summon_main_anim_color_rotation_counter;
                    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i]
                        = 0;
                }
            }
        }
        return done_flag;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_tick_summon_spell_setup_pre_animation_8slot @ 0x26152
 *
 * Stage-0 pre-animation 8-slot orbit setup. Dispatch table #0.
 * State 3=init stagger, 4=blit-if-visible, 5=advance+blit-if-hidden.
 * ---------------------------------------------------------------- */
int fd2_tick_summon_spell_setup_pre_animation_8slot(
    uint32 caster_unit_id, uint32 sprite_handle,
    uint32 origin_y, uint32 row_stride, uint32 state_code)
{
    uint8 vis[7];
    uint32 y_off[7];
    int32 row_mul[7];
    int i;
    int done_flag;
    uint8 *pChar;

    done_flag = 0;
    memcpy(vis, data_fd2_battle_summon_spell_8slot_visibility_table, 7);
    memcpy(y_off, data_fd2_battle_summon_spell_8slot_y_offset_table, 28);
    memcpy(row_mul, data_fd2_battle_summon_spell_8slot_row_multiplier_table, 28);

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pChar[6] == 0) {
        for (i = 0; i < 7; i++)
            y_off[i] += 0x94;
    }

    if (state_code == 3) {
        for (i = 0; i < 8; i++)
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                = -2 * i;
        return 0x1C;
    }

    if (state_code == 4) {
        for (i = 0; i < 7; i++) {
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                == 3)
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr,
                    1, 1);
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                    >= 0 &&
                data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                    < 0x10 &&
                vis[i] == 1) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i],
                    row_mul[i] * (int)row_stride
                        + (int)origin_y + (int)y_off[i],
                    (int)row_stride, -1);
            }
        }
        return 0;
    }

    if (state_code == 5) {
        for (i = 0; i < 7; i++) {
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                    >= 0 &&
                data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                    < 0x10 &&
                vis[i] == 0) {
                fd2_blit_indexed_sprite(
                    sprite_handle,
                    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i],
                    row_mul[i] * (int)row_stride
                        + (int)origin_y + (int)y_off[i],
                    (int)row_stride, -1);
            }
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]++;
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i]
                == 9)
                done_flag = 1;
        }
        return done_flag;
    }

    return 0;
}
