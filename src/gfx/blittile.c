/*
 * blittile.c — 24x24 tile/sprite blit helpers (battle render workspace)
 */

#include <string.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_blit_24x24_at_window_relative_pos @ 0x126F7 (1 caller)
 *
 * Paint a 24x24 sprite at world coords (world_x, world_y) into the
 * battle render workspace, only if the tile is visible inside the
 * battle window. Out-of-window calls are silent no-ops, so callers
 * (the cursor-overlay range patterns) can pass x+-k / y+-k offsets
 * that fall outside the window without guarding.
 *
 * Window test:
 *   origin_x <= world_x < origin_x + max_x
 *   origin_y <= world_y < origin_y + max_y
 *
 * Sprite source: cursor/UI sprite atlas at
 *   data_fd2_runtime_battle_state_ptr; the per-index absolute byte
 *   offset lives in an offset table at +6 (index*4), so
 *   sprite_src = base + *(int*)(base + 6 + sprite_idx*4).
 *
 * Destination: battle render workspace at
 *   data_fd2_large_game_state_buffer_ptr + 0x8088, with row stride
 *   0x2AC0 (= 24 rows * 0x1C8 pitch) and column stride 0x18 (24
 *   bytes per tile). Blit pitch passed to the passthrough blitter
 *   is 0x1C8.
 * ---------------------------------------------------------------- */
void fd2_blit_24x24_at_window_relative_pos(uint32 world_x, uint32 world_y,
                                           uint32 sprite_idx)
{
    uint32 sprite_src;
    uint32 dst;

    if ((int)data_fd2_battle_view_window_origin_x <= (int)world_x &&
        (int)world_x < (int)(data_fd2_battle_view_window_origin_x +
                             data_fd2_battle_view_window_max_x) &&
        (int)data_fd2_battle_view_window_origin_y <= (int)world_y &&
        (int)world_y < (int)(data_fd2_battle_view_window_origin_y +
                             data_fd2_battle_view_window_max_y)) {

        sprite_src = data_fd2_runtime_battle_state_ptr +
            *(int *)(data_fd2_runtime_battle_state_ptr + 6 + sprite_idx * 4);

        dst = data_fd2_large_game_state_buffer_ptr +
            (world_y - data_fd2_battle_view_window_origin_y) * 0x2AC0 +
            (world_x - data_fd2_battle_view_window_origin_x) * 0x18 +
            0x8088;

        fd2_tile_blit_24x24_passthrough(sprite_src, dst, 0x1C8);
    }
}

/* ----------------------------------------------------------------
 * fd2_blit_animated_tile_at_pos @ 0x12AC6 (1 caller)
 *
 * Paint a single battle-map tile sprite at world (tile_x, tile_y)
 * into dst_buf, with animation flip + optional palette remap.
 *
 * Visible-window bounds check (with +-1 tile margin):
 *   origin_x - 1 <= tile_x <= origin_x + max_x
 *   origin_y - 1 <= tile_y <= origin_y + max_y + 1   and  tile_y >= 0
 * Out-of-window calls are silent no-ops.
 *
 * Per-cell tile metadata (battle_tile_map, 4 bytes/cell):
 *   +4..+5  ushort: low 10 bits = tile id (upper 6 bits are metadata)
 *   +6      animation phase counter
 *   +7      cursor-overlay flag: 0xFF = no overlay (plain blit),
 *           else = blit with palette remap
 *
 * Tile-attribute flags (tile_attribute_flags_buffer, 4 bytes/tile):
 *   0x08  animated: tile id += bg_anim_flip_flag * 2 per anim tick
 *   0x80  renderable: transparent tiles have this clear -> skip blit
 *
 * Destination: dst_buf + 0x8088, row stride 0x2AC0, column stride
 * 0x18; blit pitch 0x1C8.
 * ---------------------------------------------------------------- */
void fd2_blit_animated_tile_at_pos(uint32 buf, int32 tile_x, int32 tile_y)
{
    uint32 tile_id;
    uint32 tile_meta_addr;
    uint32 sprite_src;
    uint32 dst;
    uint32 remap_table;
    uint8 tile_attr_flags;

    if ((int)data_fd2_battle_view_window_origin_x - 1 <= tile_x &&
        tile_x <= (int)(data_fd2_battle_view_window_origin_x +
                        data_fd2_battle_view_window_max_x) &&
        (int)data_fd2_battle_view_window_origin_y - 1 <= tile_y &&
        tile_y <= (int)(data_fd2_battle_view_window_origin_y +
                        data_fd2_battle_view_window_max_y + 1) &&
        tile_y >= 0) {

        tile_meta_addr = data_fd2_battle_tile_map_ptr +
            (tile_y * (int)data_fd2_battle_map_width_tiles + tile_x) * 4;

        tile_id = *(uint16 *)(tile_meta_addr + 4) & 0x3FF;

        tile_attr_flags = *(uint8 *)(data_fd2_tile_attribute_flags_buffer_ptr +
                                     tile_id * 4);

        if ((tile_attr_flags & 0x08) != 0) {
            tile_id = tile_id + data_fd2_graphics_bg_anim_flip_flag * 2;
        }

        if ((tile_attr_flags & 0x80) != 0) {
            sprite_src = battle_scene_snapshot +
                *(int *)(battle_scene_snapshot + 10 + tile_id * 4);

            dst = buf + 0x8088 +
                (tile_y - (int)data_fd2_battle_view_window_origin_y) * 0x2AC0 +
                (tile_x - (int)data_fd2_battle_view_window_origin_x) * 0x18;

            if (*(int8 *)(tile_meta_addr + 7) == -1) {
                fd2_tile_blit_24x24_passthrough(sprite_src, dst, 0x1C8);
            } else {
                remap_table = data_fd2_tile_anim_table_base +
                    *(int *)(data_fd2_tile_anim_table_base + 6 +
                             data_fd2_graphics_tile_anim_palette_phase_lookup
                                 [data_fd2_battle_tile_map_anim_frame_counter] * 4);
                fd2_tile_blit_24x24_with_remap_table(sprite_src, dst, 0x1C8,
                                                     remap_table);
            }
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_blit_scaled_tile_map_view @ 0x1F558 (2 callers)
 *
 * Software rasterizer: scale-render the battle tile map into
 * data_fd2_large_game_state_buffer_ptr + 0x504 (row 1 of a 320x192
 * surface) at an arbitrary zoom factor. Used by the tactical-overview
 * zoom animation (fd2_open_tactical_overview_zoom) and the earthquake
 * spell screen-shake (fd2_cast_earthquake_spell_with_screen_shake).
 *
 * Coordinate format: 12.12 fixed-point, 0xC00 fixed units = 1 tile.
 * (src_cx_fp, src_cy_fp) is the screen-center source position; scale is
 * the per-pixel source step (0x800 ~= 1:1; smaller = more zoomed in).
 *
 * Top-left source = center - scale*(half-extent):
 *   src_x_fp = src_cx_fp - scale*0x9C   (0x9C = 156 = 312/2 cols)
 *   src_y_fp = src_cy_fp - scale*0x60   (0x60 =  96 = 192/2 rows)
 * Split into whole-tile index (/0xC00, floored toward -inf via the
 * negative-remainder fixups) and sub-tile offset (%0xC00).
 *
 * Per output pixel the sub-tile fixed coord is divided by 0x80 to pick
 * a 0..23 byte within the 24x24 tile sprite (0xC00/0x80 = 24); rows use
 * stride 0x18 (24 bytes/tile row). tile_data_table is a caller-provided
 * table of 0x40-stride rows, each entry a 4-byte absolute sprite-data
 * pointer for cell (tile_y*0x40 + tile_x). Out-of-map cells are left as
 * the memset-cleared background.
 *
 * Globals:
 *   data_fd2_large_game_state_buffer_ptr (0x53A49) — output surface
 *   data_fd2_battle_map_width_tiles  (0x53AC1) — column bound
 *   data_fd2_battle_map_height_tiles (0x53AC5) — row bound
 *
 * Cdecl, 4 stack params; void return. The binary's __CHK(0x3C)
 * stack-probe prologue is compiler-injected, not emitted here.
 * ---------------------------------------------------------------- */
void fd2_blit_scaled_tile_map_view(uint32 src_cx_fp, uint32 src_cy_fp,
                                   uint32 scale, uint32 tile_data_table)
{
    uint32 src_x_fp;
    int32 src_y_start;
    uint32 tile_x;
    uint32 tile_y;
    uint32 sub_tile_x;
    uint32 src_y_fp;
    uint32 subtile_y_byte;
    uint32 tile_data_base;
    uint32 src_x_step;
    uint32 tile_x_step;
    uint32 out_row;
    uint32 out_col;
    uint32 out_row_ptr;

    src_x_fp = src_cx_fp + scale * -0x9C;
    src_y_start = (int)src_cy_fp + (int)scale * -0x60;
    tile_x = (int)src_x_fp / 0xC00;
    sub_tile_x = (int)src_x_fp % 0xC00;
    tile_y = src_y_start / 0xC00;
    src_y_fp = src_y_start % 0xC00;
    if ((int)src_y_fp < 0) {
        src_y_fp = src_y_fp + 0xC00;
        tile_y = tile_y - 1;
    }
    if ((int)sub_tile_x < 0) {
        sub_tile_x = sub_tile_x + 0xC00;
        tile_x = tile_x - 1;
    }

    out_row_ptr = data_fd2_large_game_state_buffer_ptr + 0x504;
    memset((void *)data_fd2_large_game_state_buffer_ptr, 0, 64000);

    for (out_row = 0; (int)out_row < 0xC0; out_row = out_row + 1) {
        subtile_y_byte = ((int)src_y_fp / 0x80) * 0x18;
        if ((int)tile_y >= 0 && (int)tile_y < (int)data_fd2_battle_map_height_tiles) {
            tile_data_base =
                *(int *)((tile_y * 0x40 + tile_x) * 4 + tile_data_table) +
                subtile_y_byte;
            src_x_step = sub_tile_x;
            tile_x_step = tile_x;
            for (out_col = 0; (int)out_col < 0x138; out_col = out_col + 1) {
                if ((int)tile_x_step >= 0 &&
                    (int)tile_x_step < (int)data_fd2_battle_map_width_tiles) {
                    *(uint8 *)(out_col + out_row_ptr) =
                        *(uint8 *)(((int)src_x_step / 0x80) + tile_data_base);
                }
                src_x_step = src_x_step + scale;
                if ((int)src_x_step > 0xBFF) {
                    tile_x_step = tile_x_step + 1;
                    src_x_step = src_x_step - 0xC00;
                    tile_data_base =
                        *(int *)((tile_y * 0x40 + tile_x_step) * 4 +
                                 tile_data_table) +
                        subtile_y_byte;
                }
            }
        }
        src_y_fp = src_y_fp + scale;
        if ((int)src_y_fp > 0xBFF) {
            tile_y = tile_y + 1;
            src_y_fp = src_y_fp - 0xC00;
        }
        out_row_ptr = out_row_ptr + 0x140;
    }
}
