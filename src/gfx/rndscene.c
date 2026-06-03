/*
 * rndscene.c — battle scene frame compositor / finalizer
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_composite_battle_frame @ 0x11CAC (61 callers)
 *
 * Battle screen frame finalizer. Composites tile map + character
 * sprites + UI overlays into the render workspace at
 * (data_fd2_large_game_state_buffer_ptr + 0x8088), then blits the
 * visible 312x192 region to the mode13h primary surface starting at
 * 0xA0504 (first visible pixel after the HUD strip; stride 320).
 *
 * Pipeline (asm order):
 *   fd2_tick_chapter_palette_animation();
 *   if (skip_palette_cycle == 0) fd2_update_palette_cycle_anim();
 *   fd2_composite_battle_tile_map(ws, 456, 13, 8, origin_x, origin_y);
 *   fd2_paint_cursor_overlay_pattern();
 *   fd2_composite_all_chars_overlay();
 *   fd2_render_terrain_info_hud_panel(ws, 456);
 *   fd2_blit_rectangle(0xA0504, 320, ws, 456, 312, 192);
 *
 * Pixel constants:
 *   0x1C8 = 456 — workspace pitch
 *   0x140 = 320 — mode13h primary stride
 *   0x138 = 312 — visible clipped width
 *   0xC0  = 192 — visible clipped height
 *
 * skip_palette_cycle: 0 = advance palette cycle this frame;
 *   non-zero = skip (caller drives palette timing).
 * ---------------------------------------------------------------- */
void fd2_composite_battle_frame(int skip_palette_cycle)
{
    uint32 ws;

    fd2_tick_chapter_palette_animation();
    if (skip_palette_cycle == 0) {
        fd2_update_palette_cycle_anim();
    }

    ws = data_fd2_large_game_state_buffer_ptr + 0x8088;
    fd2_composite_battle_tile_map(ws, 0x1c8, 0xd, 8,
                                  data_fd2_battle_view_window_origin_x,
                                  data_fd2_battle_view_window_origin_y);
    fd2_paint_cursor_overlay_pattern();
    fd2_composite_all_chars_overlay();
    fd2_render_terrain_info_hud_panel(ws, 0x1c8);
    fd2_blit_rectangle(0xa0504, 0x140, ws, 0x1c8, 0x138, 0xc0);
}
