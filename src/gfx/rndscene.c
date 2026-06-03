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

/* ----------------------------------------------------------------
 * fd2_composite_all_chars_overlay @ 0x127A9 (18 callers)
 *
 * Paint all alive party-member sprites onto the battle render
 * workspace. For each party slot, skip dead members; otherwise blit
 * the character sprite (with facing + status icons). A final pass
 * draws the drop-shadow under each char.
 *
 * Called by fd2_composite_battle_frame as the per-char layer (after
 * the tile map, before the UI text).
 * ---------------------------------------------------------------- */
void fd2_composite_all_chars_overlay(void)
{
    int32 i;

    for (i = 0; i < (int32)data_fd2_battle_party_member_count; i++) {
        if (!fd2_check_char_is_dead((uint32)i)) {
            fd2_paint_char_sprite_at_world_pos((uint32)i);
        }
    }
    fd2_paint_chars_shadow_overlay();
}

/* ----------------------------------------------------------------
 * fd2_paint_char_sprite_at_world_pos @ 0x127E0 (8 callers)
 *
 * Paint one runtime_char's facing sprite onto the battle render
 * workspace, applying the chapter palette, sleep-status shake jitter,
 * and a greyed-out look once the unit has already acted this turn.
 *
 * Steps (asm order):
 *   - If the BIOS tick (signed word @ 0x46C) changed since the last
 *     paint, flip the 1-bit shake jitter and latch the new tick.
 *   - Read the char's world (x,y) and facing (sprite_state[1]).
 *   - Reject (return) if outside the visible window (±1 tile margin).
 *   - Pick a per-facing pitch delta:
 *       facing 0 (down):  +0x720
 *       facing 1 (left):  -4
 *       facing 2 (up):    -0x720
 *       facing 3 (right): +4
 *   - blit_offset = walk_phase * pitch_delta
 *                 + (pos_y - origin_y) * 0x2AC0
 *                 + (pos_x - origin_x) * 0x18
 *     plus the shake jitter byte when sleeping.
 *   - Palette: walk_phase==0 -> ambient palette idx, else alt palette
 *     idx; palette 3 falls back to 1; sleeping forces palette 0.
 *   - Sprite lookup through portrait_sprite_cache:
 *       idx  = facing*3 + sprite_state[0]*0xC + palette_idx
 *       base = portrait_sprite_cache
 *       sprite_ptr = base + *(int32 *)(base + idx*4)
 *   - blit_offset += 0x75D8 (char layer base in the workspace);
 *     if non-negative, blit the 24x24 tile into
 *     (large_game_state_buffer + blit_offset) with stride 0x1C8,
 *     using the dimmed/grayscale blitter when flags bit7 (acted) is
 *     set, otherwise the passthrough blitter.
 *
 * 0x2AC0 = 10944 (one tile row in the workspace), 0x18 = 24 (one tile
 * column), 0x1C8 = 456 (workspace pitch).
 * ---------------------------------------------------------------- */
void fd2_paint_char_sprite_at_world_pos(uint32 char_idx)
{
    runtime_char *pchar;
    int32 cur_tick;
    int32 pos_x;
    int32 pos_y;
    uint32 facing;
    int32 pitch_delta;
    int32 blit_offset;
    uint32 palette_idx;
    uint32 sprite_idx;
    uint32 cache_base;
    uint32 sprite_ptr;
    uint32 dst;

    cur_tick = (int32)(int16)BIOS_TICK_WORD;
    if (cur_tick != data_fd2_graphics_char_sprite_paint_jitter_tick_latch) {
        data_fd2_graphics_char_sprite_shake_jitter_bit ^= 1;
        data_fd2_graphics_char_sprite_paint_jitter_tick_latch = cur_tick;
    }

    pchar = &data_fd2_battle_runtime_char_array_ptr[char_idx];
    pos_x = pchar->pos_x;
    pos_y = pchar->pos_y;
    facing = pchar->sprite_state[1];

    if (!((int32)(data_fd2_battle_view_window_origin_x - 1) <= pos_x &&
          pos_x <= (int32)(data_fd2_battle_view_window_origin_x +
                           data_fd2_battle_view_window_max_x) &&
          (int32)(data_fd2_battle_view_window_origin_y - 1) <= pos_y &&
          pos_y <= (int32)(data_fd2_battle_view_window_origin_y +
                           data_fd2_battle_view_window_max_y + 1))) {
        return;
    }

    if (facing == 0) {
        pitch_delta = 0x720;
    } else if (facing == 1) {
        pitch_delta = -4;
    } else if (facing == 2) {
        pitch_delta = -0x720;
    } else {
        pitch_delta = 4;
    }

    blit_offset = (int32)pchar->sprite_state[2] * pitch_delta +
                  (pos_y - (int32)data_fd2_battle_view_window_origin_y) * 0x2ac0 +
                  (pos_x - (int32)data_fd2_battle_view_window_origin_x) * 0x18;

    if (pchar->status_sleep_flag != 0) {
        blit_offset += data_fd2_graphics_char_sprite_shake_jitter_bit;
    }

    if (pchar->sprite_state[2] == 0) {
        palette_idx = data_fd2_graphics_chapter_ambient_palette_anim_idx;
    } else {
        palette_idx = data_fd2_graphics_chapter_walk_anim_alt_palette_idx;
    }
    if (palette_idx == 3) {
        palette_idx = 1;
    }
    if (pchar->status_sleep_flag != 0) {
        palette_idx = 0;
    }

    sprite_idx = facing * 3 + (uint32)pchar->sprite_state[0] * 0xc + palette_idx;
    cache_base = portrait_sprite_cache;
    sprite_ptr = cache_base + (uint32)*(int32 *)(cache_base + sprite_idx * 4);

    blit_offset += 0x75d8;
    if (blit_offset >= 0) {
        dst = data_fd2_large_game_state_buffer_ptr + (uint32)blit_offset;
        if ((pchar->flags & 0x80) == 0) {
            fd2_tile_blit_24x24_passthrough(sprite_ptr, dst, 0x1c8);
        } else {
            fd2_tile_blit_24x24_dimmed_grayscale(sprite_ptr, dst, 0x1c8);
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_paint_chars_shadow_overlay @ 0x129EC (4 callers)
 *
 * Redraw the animated terrain tiles around each char position to
 * clean up the walk-animation tile-trail left under moving chars
 * (the 24x24 char sprite over the 24x24 tile grid leaves smears).
 *
 * For each party slot, skip immune and dead chars. Otherwise read the
 * char's tile position and facing, then:
 *   - Base footprint (char sprite is 1x2 tiles): redraw the animated
 *     tile at (pos_x, pos_y) and (pos_x, pos_y-1).
 *   - Walk-anim trail: when sprite_state[2] (walk phase) != 0, redraw
 *     one extra trailing tile in the facing direction
 *     (sprite_state[1]):
 *       facing 0 (down):  (pos_x,   pos_y+1)
 *       facing 1 (left):  (pos_x-1, pos_y) + (pos_x-1, pos_y-1)
 *       facing 2 (up):    (pos_x,   pos_y-2)
 *       facing 3 (right): (pos_x+1, pos_y) + (pos_x+1, pos_y-1)
 *
 * Called by fd2_composite_all_chars_overlay after the per-char sprite
 * paint pass.
 * ---------------------------------------------------------------- */
void fd2_paint_chars_shadow_overlay(void)
{
    uint32 i;
    runtime_char *pchar;
    int32 pos_x;
    int32 pos_y;
    uint8 facing;
    uint8 walk_phase;
    int32 blit_y;

    for (i = 0; (int32)i < (int32)data_fd2_battle_party_member_count; i++) {
        if (fd2_check_char_status_immunity(i) != 0) {
            continue;
        }
        if (fd2_check_char_is_dead(i) != 0) {
            continue;
        }

        pchar = &data_fd2_battle_runtime_char_array_ptr[i];
        pos_x = (int32)pchar->pos_x;
        pos_y = (int32)pchar->pos_y;
        facing = pchar->sprite_state[1];
        walk_phase = pchar->sprite_state[2];

        fd2_blit_animated_tile_at_pos(data_fd2_large_game_state_buffer_ptr,
                                      pos_x, pos_y);
        blit_y = pos_y - 1;
        fd2_blit_animated_tile_at_pos(data_fd2_large_game_state_buffer_ptr,
                                      pos_x, blit_y);

        if (walk_phase != 0) {
            if (facing == 0) {
                blit_y = pos_y + 1;
            } else {
                if (facing == 1) {
                    pos_x = pos_x - 1;
                } else if (facing == 2) {
                    blit_y = pos_y - 2;
                    fd2_blit_animated_tile_at_pos(
                        data_fd2_large_game_state_buffer_ptr, pos_x, blit_y);
                    continue;
                } else {
                    pos_x = pos_x + 1;
                }
                fd2_blit_animated_tile_at_pos(
                    data_fd2_large_game_state_buffer_ptr, pos_x, pos_y);
            }
            fd2_blit_animated_tile_at_pos(
                data_fd2_large_game_state_buffer_ptr, pos_x, blit_y);
        }
    }
}
