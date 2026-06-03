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

/* ----------------------------------------------------------------
 * fd2_paint_cursor_overlay_pattern @ 0x122DC (2 callers)
 *
 * Paint the cursor highlight + range-indicator pattern around
 * (data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y).
 * Pattern shape is selected by data_fd2_battle_anim_phase:
 *
 *   phase 1: blit sprite 0 at cursor          — simple cursor box
 *   phase 2: blit sprite 1 at cursor          — alternate cursor
 *   phase 3: 5-sprite manhattan-range-1 cross
 *   phase 4: 13-sprite manhattan-range-2 area
 *   phase 5: 21-sprite manhattan-range-3 area
 *   phase 6: clear cursor flag — write 0 to
 *            tile_map[(y*map_width + x)*4 + 7]
 *
 * fd2_blit_24x24_at_window_relative_pos(world_x, world_y, sprite_idx)
 * is bounds-clipped: off-window coords are silent no-ops, so the
 * x±k / y±k arithmetic below can produce out-of-window values safely.
 * Sprite indices map to fdother's cursor sprite atlas.
 * ---------------------------------------------------------------- */
void fd2_paint_cursor_overlay_pattern(void)
{
    uint32 x;
    uint32 y;

    x = data_fd2_battle_cursor_world_x;
    y = data_fd2_battle_cursor_world_y;

    switch (data_fd2_battle_anim_phase) {
    case 1:
        fd2_blit_24x24_at_window_relative_pos(x, y, 0);
        return;
    case 2:
        fd2_blit_24x24_at_window_relative_pos(x, y, 1);
        return;
    case 3:
        fd2_blit_24x24_at_window_relative_pos(x,     y,     0xe);
        fd2_blit_24x24_at_window_relative_pos(x,     y - 1, 2);
        fd2_blit_24x24_at_window_relative_pos(x - 1, y,     3);
        fd2_blit_24x24_at_window_relative_pos(x + 1, y,     4);
        fd2_blit_24x24_at_window_relative_pos(x,     y + 1, 5);
        return;
    case 4:
        fd2_blit_24x24_at_window_relative_pos(x,     y,     1);
        fd2_blit_24x24_at_window_relative_pos(x,     y - 2, 2);
        fd2_blit_24x24_at_window_relative_pos(x - 2, y,     3);
        fd2_blit_24x24_at_window_relative_pos(x + 2, y,     4);
        fd2_blit_24x24_at_window_relative_pos(x,     y + 2, 5);
        fd2_blit_24x24_at_window_relative_pos(x - 1, y - 1, 6);
        fd2_blit_24x24_at_window_relative_pos(x + 1, y - 1, 7);
        fd2_blit_24x24_at_window_relative_pos(x - 1, y + 1, 8);
        fd2_blit_24x24_at_window_relative_pos(x + 1, y + 1, 9);
        fd2_blit_24x24_at_window_relative_pos(x,     y - 1, 0xa);
        fd2_blit_24x24_at_window_relative_pos(x - 1, y,     0xb);
        fd2_blit_24x24_at_window_relative_pos(x + 1, y,     0xc);
        fd2_blit_24x24_at_window_relative_pos(x,     y + 1, 0xd);
        return;
    case 5:
        fd2_blit_24x24_at_window_relative_pos(x,     y,     1);
        fd2_blit_24x24_at_window_relative_pos(x,     y - 3, 2);
        fd2_blit_24x24_at_window_relative_pos(x - 3, y,     3);
        fd2_blit_24x24_at_window_relative_pos(x + 3, y,     4);
        fd2_blit_24x24_at_window_relative_pos(x,     y + 3, 5);
        fd2_blit_24x24_at_window_relative_pos(x - 1, y - 2, 6);
        fd2_blit_24x24_at_window_relative_pos(x - 2, y - 1, 6);
        fd2_blit_24x24_at_window_relative_pos(x + 1, y - 2, 7);
        fd2_blit_24x24_at_window_relative_pos(x + 2, y - 1, 7);
        fd2_blit_24x24_at_window_relative_pos(x - 1, y + 2, 8);
        fd2_blit_24x24_at_window_relative_pos(x - 2, y + 1, 8);
        fd2_blit_24x24_at_window_relative_pos(x + 1, y + 2, 9);
        fd2_blit_24x24_at_window_relative_pos(x + 2, y + 1, 9);
        fd2_blit_24x24_at_window_relative_pos(x,     y - 2, 0xa);
        fd2_blit_24x24_at_window_relative_pos(x - 2, y,     0xb);
        fd2_blit_24x24_at_window_relative_pos(x + 2, y,     0xc);
        fd2_blit_24x24_at_window_relative_pos(x,     y + 2, 0xd);
        fd2_blit_24x24_at_window_relative_pos(x - 1, y - 1, 0xf);
        fd2_blit_24x24_at_window_relative_pos(x + 1, y - 1, 0x10);
        fd2_blit_24x24_at_window_relative_pos(x - 1, y + 1, 0x11);
        fd2_blit_24x24_at_window_relative_pos(x + 1, y + 1, 0x12);
        return;
    case 6:
        *(uint8 *)(data_fd2_battle_tile_map_ptr + 7 +
                   (y * data_fd2_battle_map_width_tiles + x) * 4) = 0;
        return;
    default:
        return;
    }
}
