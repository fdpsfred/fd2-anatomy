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

/* ----------------------------------------------------------------
 * fd2_blit_scaled_chapter_pose @ 0x2FB9F (4 callers)
 *
 * Software rasterizer: nearest-neighbour scale a 320x200 source
 * bitmap (1 byte/pixel, row stride 0x140) into the full 320x200
 * working surface data_fd2_large_game_state_buffer_ptr (0x53A49),
 * centred on (src_cx, src_cy) in fixed-point. Used by the chapter
 * intro/outro pose-zoom animations:
 *   fd2_chapter_transition_with_intro      @ 0x2D093 (10-frame zoom-in)
 *   fd2_run_chapter_intro_menu_main        @ 0x2E341
 *   fd2_run_chapter_intro_menu_typeB       @ 0x2FC85
 *   fd2_run_chapter_intro_menu_typeC       @ 0x3072F
 *
 * Coordinate format: 7-bit fractional fixed-point (>>7 recovers the
 * integer source pixel index, 0x80 = 1 source pixel). scale_fp_step
 * is the per-output-pixel source step: < 0x80 magnifies (zoom-in),
 * > 0x80 shrinks (zoom-out).
 *
 * Top-left source = centre - step*(half-extent):
 *   src_x_fp = src_cx - step*0xA0   (0xA0 = 160 = 320/2 cols)
 *   src_y_fp = src_cy - step*0x64   (0x64 = 100 = 200/2 rows)
 * Per output pixel the fixed coord is >>7 to pick the source byte;
 * source rows use stride 0x140 (320 bytes/row). The whole surface is
 * memset-cleared to 0 first; output pixels whose source maps outside
 * [0, 320)x[0, 200) (fixed bounds 0xA000 / 0x6400) stay background.
 *
 * Globals:
 *   data_fd2_large_game_state_buffer_ptr (0x53A49) — output surface
 *
 * Cdecl, 4 stack params; void return. The binary's __CHK(0x28)
 * stack-probe prologue is compiler-injected, not emitted here.
 *
 * The fixed-point fraction strip is an arithmetic right shift
 * ((int)>>7); the source coord is always >= 0 at the point of use
 * (the in-bounds guard rejects negatives), so >>7 reproduces the
 * Watcom signed-shift flooring idiom seen in the disassembly exactly.
 * ---------------------------------------------------------------- */
void fd2_blit_scaled_chapter_pose(uint32 src_cx, uint32 src_cy,
                                  uint32 src_bitmap, int32 scale_fp_step)
{
    uint32 src_x_fp_start;
    uint32 src_x_fp;
    uint32 src_y_fp;
    uint32 src_row_base;
    uint32 out_row_ptr;
    uint32 out_row;
    uint32 out_col;

    src_x_fp_start = src_cx + scale_fp_step * -0xA0;
    src_y_fp = src_cy + scale_fp_step * -0x64;
    out_row_ptr = data_fd2_large_game_state_buffer_ptr;
    memset((void *)data_fd2_large_game_state_buffer_ptr, 0, 64000);

    for (out_row = 0; (int)out_row < 200; out_row = out_row + 1) {
        if ((int)src_y_fp >= 0 && (int)src_y_fp < 0x6400) {
            src_x_fp = src_x_fp_start;
            src_row_base = src_bitmap + ((int)src_y_fp >> 7) * 0x140;
            for (out_col = 0; (int)out_col < 0x140; out_col = out_col + 1) {
                if ((int)src_x_fp >= 0 && (int)src_x_fp < 0xA000) {
                    *(uint8 *)(out_col + out_row_ptr) =
                        *(uint8 *)(src_row_base + ((int)src_x_fp >> 7));
                }
                src_x_fp = src_x_fp + scale_fp_step;
            }
        }
        src_y_fp = src_y_fp + scale_fp_step;
        out_row_ptr = out_row_ptr + 0x140;
    }
}

/* ----------------------------------------------------------------
 * fd2_blit_24x24_tile_to_battle_grid_position @ 0x3415E (1 caller)
 *
 * 24x24 tile blit helper for the battle preview/intro composer
 * (fd2_render_battle_scene_with_portrait_grid_layout @ 0x34010, 4 call
 * sites: chapter-digit pair, player tile row, enemy tile row, and the
 * reserved-position highlight). Resolves a tile sprite pointer from an
 * atlas table, computes the destination pixel address, and tail-calls
 * the passthrough blitter.
 *
 * Atlas indexing (same offset-table form as the sibling blit helpers):
 *   src = atlas_base + *(int*)(atlas_base + tile_index*4 + 6)
 * Each atlas entry is 4 bytes; the 4-byte value at +6 from the entry is
 * the sprite's absolute byte offset within the atlas.
 *
 * Destination address (caller-supplied row stride, not the fixed 0x1C8
 * the window-relative helpers use):
 *   dst = dst_buffer + dst_y * dst_row_stride + dst_x
 *
 * The blit pitch forwarded to fd2_tile_blit_24x24_passthrough is the
 * same dst_row_stride (typically 0x140 = 320).
 *
 * Cdecl, 6 stack params; void return. The binary's __CHK(0x14)
 * stack-probe prologue is compiler-injected, not emitted here.
 * ---------------------------------------------------------------- */
void fd2_blit_24x24_tile_to_battle_grid_position(uint32 atlas_base,
                                                 uint32 tile_index,
                                                 uint32 dst_buffer,
                                                 uint32 dst_row_stride,
                                                 uint32 dst_x, uint32 dst_y)
{
    uint32 src;
    uint32 dst;

    src = atlas_base + *(int *)(atlas_base + tile_index * 4 + 6);
    dst = dst_buffer + dst_y * dst_row_stride + dst_x;
    fd2_tile_blit_24x24_passthrough(src, dst, dst_row_stride);
}

/* ----------------------------------------------------------------
 * fd2_tile_blit_24x24_with_tint_offset @ 0x4DC34 (1 caller)
 *
 * Hand-written RLE blit of a 24x24 sprite into dst_buf, recolouring
 * every painted pixel into an 8-colour palette band:
 *
 *   out = (uint8)(((uint8)(src_pixel + team_offset) & 7) + color_base)
 *
 * team_offset rotates the source pixel within its 0..7 octet (giving a
 * per-team / per-fade colour variation), then color_base anchors the
 * band; no remap LUT is needed. Sole caller is the spell-overlay blink
 * animator fd2_animate_spell_overlay_blink @ 0x1CD17, which sweeps
 * team_offset 7..0 across 10 frames to fade a hit-mark overlay.
 *
 * The RLE stream is decoded one command byte at a time. The top two
 * bits of the command select the mode; the low 6 bits + 1 are the run
 * length:
 *   bits 7..6 = 00 (0x00..0x3F)  RUN: paint len tinted pixels from a
 *               single following source byte, dst += len; x -= len.
 *   bits 7..6 = 01 (0x40..0x7F)  STRIDE-2 RUN: paint len tinted pixels
 *               from a single following source byte, spaced every other
 *               dst byte (dst += 2 per pixel); x -= 2*len.
 *   bits 7..6 = 10 (0x80..0xBF)  LITERAL: copy len tinted pixels, one
 *               following source byte each, dst += len; x -= len.
 *   bits 7..6 = 11 (0xC0..0xFF)  SKIP: advance dst by len (transparent
 *               run, no source bytes consumed); x -= len.
 *
 * x is the per-row remaining-column counter (starts at 0x18). When it
 * reaches 0 the row ends: dst advances by stride - 0x18 to the next
 * row start, and 24 rows are rendered in total.
 *
 * Args (cdecl, 5x stack params; caller pops 0x14):
 *   rle_stream  — source RLE-encoded 24x24 sprite stream
 *   dst_buf     — destination base linear address
 *   stride      — destination row stride in bytes (0x1C8 from the
 *                 caller; the row reset advances stride - 0x18)
 *   color_base  — palette band anchor (low byte used)
 *   team_offset — per-team / per-fade add value (low byte used)
 *
 * Hand-written asm leaf: no __CHK probe, no CALLs.
 * ---------------------------------------------------------------- */
void fd2_tile_blit_24x24_with_tint_offset(uint32 rle_stream, uint32 dst_buf,
                                          uint32 stride, uint32 color_base,
                                          uint32 team_offset)
{
    uint32 src;
    uint32 dst;
    uint32 row_advance;
    uint8  off;
    uint8  base;
    uint8  cmd;
    uint8  pixel;
    uint8  x_remain;
    uint32 count;
    int    row;

    off = (uint8)team_offset;
    base = (uint8)color_base;
    src = rle_stream;
    dst = dst_buf;
    row_advance = stride - 0x18;

    for (row = 0x18; row != 0; row--) {
        x_remain = 0x18;
        do {
            cmd = *(uint8 *)src;
            src = src + 1;
            if ((cmd & 0x80) == 0) {
                if ((cmd & 0x40) == 0) {
                    /* RUN: len pixels from one source byte */
                    count = (uint32)(cmd & 0x3F) + 1;
                    x_remain = (uint8)(x_remain - (uint8)count);
                    pixel = (uint8)((((uint8)(*(uint8 *)src + off)) & 7) + base);
                    src = src + 1;
                    do {
                        *(uint8 *)dst = pixel;
                        dst = dst + 1;
                    } while (--count != 0);
                } else {
                    /* STRIDE-2 RUN: len pixels, every other dst byte */
                    count = (uint32)(cmd & 0x3F) + 1;
                    x_remain = (uint8)(x_remain - (uint8)count - (uint8)count);
                    pixel = (uint8)((((uint8)(*(uint8 *)src + off)) & 7) + base);
                    src = src + 1;
                    do {
                        dst = dst + 1;
                        *(uint8 *)dst = pixel;
                        dst = dst + 1;
                    } while (--count != 0);
                }
            } else {
                if ((cmd & 0x40) == 0) {
                    /* LITERAL: len pixels, one source byte each */
                    count = (uint32)(cmd & 0x3F) + 1;
                    x_remain = (uint8)(x_remain - (uint8)count);
                    do {
                        *(uint8 *)dst =
                            (uint8)((((uint8)(*(uint8 *)src + off)) & 7) + base);
                        src = src + 1;
                        dst = dst + 1;
                    } while (--count != 0);
                } else {
                    /* SKIP: advance dst (transparent run) */
                    count = (uint32)(cmd & 0x3F) + 1;
                    dst = dst + count;
                    x_remain = (uint8)(x_remain - (uint8)count);
                }
            }
        } while (x_remain != 0);

        dst = dst + row_advance;
    }
}
