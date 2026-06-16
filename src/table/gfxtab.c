/*
 * gfxtab.c -- graphics read-only data constants (.object2)
 *
 * Fixed numeric constants used by the graphics rendering primitives.
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * data_fd2_graphics_circle_anim_div_10 @ 0x501F0  (8 bytes, double)
 *
 * Fixed divisor 10.0 used by fd2_render_circle_anim_row when computing the
 * half-width of each horizontal band slice of the filled-circle animation:
 *   half_width = trunc(sqrt(r*r - dy*dy) * scale_num / 10.0)
 * Loaded as an 8-byte double via x87 FDIV. Read-only; single reader.
 */
const double data_fd2_graphics_circle_anim_div_10 = 10.0;

/* ----------------------------------------------------------------
 * data_fd2_graphics_radian_per_degree_const @ 0x501F8  (8 bytes, double)
 *
 * Degrees-to-radians multiplier used by the AoE radial-scatter geometry
 * (fd2_scatter_sprite_around_origin_with_random_offset):
 *   angle_rad = (double)angle_deg * data_fd2_graphics_radian_per_degree_const;
 * Loaded as an 8-byte double via x87 FMUL (FMUL double ptr [0x501F8]).
 * The stored literal is 0.0174532 -- a 7-digit deg->rad approximation, NOT
 * full-precision pi/180 (0.017453292519943295). C literal 0.0174532 encodes
 * byte-exact to af 99 d7 6c 40 df 91 3f (LE). Read-only; single reader.
 */
const double data_fd2_graphics_radian_per_degree_const = 0.0174532;

/* ----------------------------------------------------------------
 * data_fd2_graphics_scatter_y_offset_neg8 @ 0x50200  (8 bytes, double)
 *
 * Y-axis skew added to the AoE radial-scatter sprite Y coordinate
 * (fd2_scatter_sprite_around_origin_with_random_offset):
 *   Y = trunc(origin_y + r*sin(theta) + data_fd2_graphics_scatter_y_offset_neg8);
 * Loaded as an 8-byte double via x87 FADD (FADD double ptr [0x50200] @ 0x21e75).
 * Shifts the scatter pattern upward to match the isometric battlefield view.
 * Stored value is -8.0; C literal -8.0 encodes byte-exact to 00 00 00 00 00 00
 * 20 c0 (LE, IEEE-754 0xC020000000000000). Read-only; single reader.
 */
const double data_fd2_graphics_scatter_y_offset_neg8 = -8.0;

/* ----------------------------------------------------------------
 * data_fd2_graphics_circle_band_radius_scale_16 @ 0x50208  (8 bytes, double)
 *
 * Band half-width scale factor used by fd2_render_filled_circle_band_anim when
 * sizing the fully-filled middle section of the AoE filled-circle visual:
 *   half_width = trunc(radius_factor * data_fd2_graphics_circle_band_radius_scale_16);
 * Loaded as an 8-byte double via x87 FMUL (FMUL double ptr [0x50208] @ 0x220a3),
 * then __CHP forces round-toward-zero before FISTP so the product TRUNCATES.
 * Stored value is 1.6; C literal 1.6 encodes byte-exact to 9a 99 99 99 99 99
 * f9 3f (LE, IEEE-754 0x3FF999999999999A). Read-only; single reader.
 */
const double data_fd2_graphics_circle_band_radius_scale_16 = 1.6;

/* ----------------------------------------------------------------
 * data_fd2_graphics_tile_anim_palette_phase_lookup @ 0x51A97  (20 bytes, uint8[20])
 *
 * Triangle-wave phase lookup that maps the 20-frame tile-animation frame
 * counter (data_fd2_battle_tile_map_anim_frame_counter, wraps 0..0x13) to a
 * ping-pong palette-remap slot in the per-chapter tile-anim table:
 *   slot   = data_fd2_graphics_tile_anim_palette_phase_lookup[frame_counter];
 *   remap  = *(int*)(_data_fd2_tile_anim_table_base + slot*4 + 6)
 *            + _data_fd2_tile_anim_table_base;
 * Values rise 0..10 then fall 9..1, so the palette cycles forward then back
 * for a smooth shimmer instead of a hard wrap.
 *
 * Type/width proof (two readers, no writers -> read-only const):
 *   fd2_composite_battle_tile_map @0x12169 / fd2_blit_animated_tile_at_pos:
 *     MOV   EAX,[battle_tile_map_anim_frame_counter]
 *     MOVZX EAX,byte ptr [EAX + 0x51A97]      ; unsigned 8-bit load, stride 1
 *     MOV   EAX,dword ptr [EDX + EAX*0x4 + 6] ; slot used as *4 table index
 *   MOVZX of a single byte => uint8; counter wrap at 0x14 matches len 20.
 */
const uint8 data_fd2_graphics_tile_anim_palette_phase_lookup[20] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
};
