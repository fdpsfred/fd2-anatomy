/*
 * rndscene.c — battle scene frame compositor / finalizer
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <math.h>

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

/* ----------------------------------------------------------------
 * fd2_paint_threat_overlay_for_team @ 0x145CD (5 callers)
 *
 * Mark all alive chars of the selected team with an AoE "+" pattern
 * on the threat / AoE-coverage overlay layer (tile_map +6 bytes).
 * Used during action target selection to preview which tiles the
 * enemy (or ally) units threaten.
 *
 * Asymmetric team filter (matches the AI targeting convention):
 *   ctx == 0 -> mark chars with team != 0  (ally overlay)
 *   ctx != 0 -> mark chars with team == 0  (enemy overlay)
 *
 * Per party slot: skip dead (flags bit0), then apply the team filter;
 * pass the char's (pos_x, pos_y) to fd2_mark_aoe_plus_pattern_at.
 * ---------------------------------------------------------------- */
void fd2_paint_threat_overlay_for_team(uint32 ctx)
{
    uint32 i;
    runtime_char *pchar;

    for (i = 0; (int32)i < (int32)data_fd2_battle_party_member_count; i++) {
        pchar = &data_fd2_battle_runtime_char_array_ptr[i];
        if ((pchar->flags & 1) == 0) {
            if (((ctx == 0) && (pchar->team != 0)) ||
                ((ctx != 0) && (pchar->team == 0))) {
                fd2_mark_aoe_plus_pattern_at(pchar->pos_x, pchar->pos_y);
            }
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_composite_chars_with_spell_effect_overlay @ 0x1CB94 (1 caller)
 *
 * Render the per-char layer of a spell-cast frame: chars that are in
 * the target/hit list get the spell-effect sprite (spark/glow) drawn
 * over them, every other alive char keeps its normal portrait. Used by
 * the full-screen flash animator, which double-buffers two variants
 * (fx_sprite_idx 0x4A vs 0x4B) to produce the strobe.
 *
 * Steps (asm order):
 *   - Composite the battle tile map into dst_buf + 0x8088.
 *   - Resolve the effect sprite address from the portrait sheet:
 *       fx_sprite_addr = sheet + *(int32 *)(sheet + 6 + fx_sprite_idx*4)
 *   - For each party slot (skip dead, flags bit0):
 *       - Window-cull against the battle view window (origin +/- max,
 *         with the same +/-1 margins as the normal char paint).
 *       - char_screen_addr = dst_buf + 0x75D8
 *                          + (pos_y - origin_y) * 0x2AC0
 *                          + (pos_x - origin_x) * 0x18
 *       - Scan the target list (n_targets bytes at target_array). If
 *         char_idx appears (no early break -- the original scans the
 *         whole list), it is a hit.
 *       - Hit  -> fd2_blit_sprite_with_decoded_pixels(char_screen_addr,
 *                 fx_sprite_addr, 456).
 *       - Miss -> normal portrait via the sprite cache:
 *           frame = sprite_state[0]*0xC
 *                 + (chapter_palette==3 ? 2 : chapter_palette)
 *           src   = cache + *(int32 *)(cache + frame*4)
 *           fd2_tile_blit_24x24_passthrough(src, char_screen_addr, 456).
 *
 * 0x8088/0x75D8 = char-layer bases in the render workspace,
 * 0x2AC0 = 10944 (one tile row), 0x18 = 24 (one tile col),
 * 0x1C8 = 456 (workspace pitch).
 *
 * The original tail-calls a shared epilogue (JMP 0x1317D); the C
 * equivalent is the loop simply running to completion.
 * ---------------------------------------------------------------- */
void fd2_composite_chars_with_spell_effect_overlay(uint32 dst_buf, uint32 n_targets,
                                                   uint32 target_array, int fx_sprite_idx)
{
    uint32 fx_sprite_addr;
    uint32 char_idx;
    runtime_char *pchar;
    int32 pos_x;
    int32 pos_y;
    uint32 cache_idx;
    uint32 char_screen_addr;
    int hit;
    uint32 scan_idx;
    int32 frame_idx;
    uint32 src_ptr;

    fd2_composite_battle_tile_map(dst_buf + 0x8088, 0x1c8, 0xd, 8,
                                  data_fd2_battle_view_window_origin_x,
                                  data_fd2_battle_view_window_origin_y);

    fx_sprite_addr =
        (uint32)*(int32 *)(data_fd2_resource_portrait_sheet_ptr + 6 + fx_sprite_idx * 4) +
        data_fd2_resource_portrait_sheet_ptr;

    for (char_idx = 0; (int32)char_idx < (int32)data_fd2_battle_party_member_count;
         char_idx++) {
        pchar = &data_fd2_battle_runtime_char_array_ptr[char_idx];
        if ((pchar->flags & 1) != 0) {
            continue;
        }

        pos_x = (int32)pchar->pos_x;
        pos_y = (int32)pchar->pos_y;
        cache_idx = (uint32)pchar->sprite_state[0];

        if (pos_x < (int32)(data_fd2_battle_view_window_origin_x - 1) ||
            pos_x > (int32)(data_fd2_battle_view_window_origin_x +
                            data_fd2_battle_view_window_max_x) ||
            pos_y < (int32)(data_fd2_battle_view_window_origin_y - 1) ||
            pos_y > (int32)(data_fd2_battle_view_window_origin_y +
                            data_fd2_battle_view_window_max_y + 1)) {
            continue;
        }

        char_screen_addr =
            (uint32)(pos_y - (int32)data_fd2_battle_view_window_origin_y) * 0x2ac0 +
            dst_buf + (uint32)(pos_x - (int32)data_fd2_battle_view_window_origin_x) * 0x18 +
            0x75d8;

        hit = 0;
        for (scan_idx = 0; (int32)scan_idx < (int32)n_targets; scan_idx++) {
            if (*(uint8 *)(target_array + scan_idx) == char_idx) {
                hit = 1;
            }
        }

        if (hit) {
            fd2_blit_sprite_with_decoded_pixels(char_screen_addr, fx_sprite_addr, 0x1c8);
        } else {
            frame_idx = (int32)cache_idx * 0xc;
            if (data_fd2_graphics_chapter_ambient_palette_anim_idx == 3) {
                frame_idx += 2;
            } else {
                frame_idx += (int32)data_fd2_graphics_chapter_ambient_palette_anim_idx;
            }
            src_ptr = portrait_sprite_cache +
                      (uint32)*(int32 *)(portrait_sprite_cache + (uint32)frame_idx * 4);
            fd2_tile_blit_24x24_passthrough(src_ptr, char_screen_addr, 0x1c8);
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_paint_char_sprite_at_world_with_mode @ 0x1DA16 (3 callers)
 *
 * Paint one runtime_char's facing sprite onto an arbitrary surface at
 * its battle-world position, with a selectable render mode. This is the
 * mode-aware sibling of fd2_paint_char_sprite_at_world_pos @ 0x127E0
 * (which always blits passthrough into the fixed render workspace);
 * here the caller supplies the destination buffer, the destination
 * stride, and a mode that picks between a normal blit and a solid-color
 * silhouette overlay (used for the heal / status-effect highlight).
 *
 * Unlike the workspace version, this routine has NO BIOS-tick shake
 * jitter and NO sleep-status handling — it is the plain world->surface
 * compositor.
 *
 * Window cull (battle view window, inclusive +/-1 margins; off-window
 * is a silent no-op):
 *   pos_x <  origin_x - 1            -> return
 *   pos_x >  origin_x + max_x        -> return
 *   pos_y <  origin_y - 1            -> return
 *   pos_y >  origin_y + max_y + 1    -> return
 *
 * Per-facing sub-pixel x_offset (facing = sprite_state[1]), multiplied
 * by the walk_phase (sprite_state[2]) for the moving-frame jitter:
 *   facing 0 (down):  x_offset =  dst_stride << 2   (= stride*4)
 *   facing 1 (left):  x_offset = -4
 *   facing 2 (up):    x_offset = (-dst_stride) << 2 (= -(stride*4))
 *   facing 3 (right): x_offset =  4
 *
 * Animation frame:
 *   walk_phase == 0 -> frame = chapter_ambient_palette_anim_idx
 *   walk_phase != 0 -> frame = chapter_walk_anim_alt_palette_idx
 *   frame == 3      -> frame = 1   (fold)
 *   sprite_idx = facing*3 + sprite_state[0]*0xC + frame
 *   rle_stream = portrait_sprite_cache
 *              + *(int32 *)(portrait_sprite_cache + sprite_idx*4)
 *
 * Destination address (note: uses dst_stride, not the workspace pitch):
 *   dst = dst_buf - dst_stride*6
 *       + (pos_y - origin_y) * dst_stride * 0x18
 *       + (pos_x - origin_x) * 0x18
 *       + walk_phase * x_offset
 *
 * Mode dispatch:
 *   mode 0 -> fd2_tile_blit_24x24_passthrough(rle_stream, dst, dst_stride)
 *   mode 2 -> fd2_tile_blit_24x24_solid_color(rle_stream, dst, dst_stride, color)
 *   other  -> draw nothing (not expected to be passed)
 *
 * The mode-2 blitter derives the silhouette color from its stride arg
 * (color = stride & 0xFF) and ignores the 4th arg; the original still
 * pushes `color` as that 4th slot, so it is passed here verbatim.
 *
 * 0x18 = 24 (tile column / one sprite row pitch in tiles).
 *
 * 3 callers: fd2_ai_pass_turn_with_heal, fd2_run_full_turn_cycle,
 * fd2_animate_attack_hit_sequence.
 * ---------------------------------------------------------------- */
void fd2_paint_char_sprite_at_world_with_mode(uint32 dst_buf, uint32 dst_stride,
                                              uint32 char_idx, uint32 mode,
                                              uint32 color)
{
    runtime_char *pchar;
    int32 pos_x;
    int32 pos_y;
    uint32 facing;
    uint32 walk_phase;
    uint32 x_offset;
    uint32 frame;
    uint32 sprite_idx;
    uint32 rle_stream;
    uint32 dst;

    pchar = &data_fd2_battle_runtime_char_array_ptr[char_idx];
    pos_x = (int32)pchar->pos_x;
    pos_y = (int32)pchar->pos_y;
    facing = (uint32)pchar->sprite_state[1];

    if (pos_x < (int32)(data_fd2_battle_view_window_origin_x - 1)) {
        return;
    }
    if ((int32)(data_fd2_battle_view_window_origin_x +
                data_fd2_battle_view_window_max_x) < pos_x) {
        return;
    }
    if (pos_y < (int32)(data_fd2_battle_view_window_origin_y - 1)) {
        return;
    }
    if ((int32)(data_fd2_battle_view_window_origin_y +
                data_fd2_battle_view_window_max_y + 1) < pos_y) {
        return;
    }

    if (facing == 0) {
        x_offset = dst_stride << 2;
    } else if (facing == 1) {
        x_offset = 0xfffffffc;          /* -4 */
    } else if (facing == 2) {
        x_offset = (0 - dst_stride) << 2;
    } else {
        x_offset = 4;
    }

    walk_phase = (uint32)pchar->sprite_state[2];
    if (walk_phase == 0) {
        frame = data_fd2_graphics_chapter_ambient_palette_anim_idx;
    } else {
        frame = data_fd2_graphics_chapter_walk_anim_alt_palette_idx;
    }
    if (frame == 3) {
        frame = 1;
    }

    sprite_idx = facing * 3 + (uint32)pchar->sprite_state[0] * 0xc + frame;
    rle_stream = portrait_sprite_cache +
                 (uint32)*(int32 *)(portrait_sprite_cache + sprite_idx * 4);

    dst = dst_buf - dst_stride * 6 +
          (uint32)(pos_y - (int32)data_fd2_battle_view_window_origin_y) *
              dst_stride * 0x18 +
          (uint32)(pos_x - (int32)data_fd2_battle_view_window_origin_x) * 0x18 +
          walk_phase * x_offset;

    if (mode == 0) {
        fd2_tile_blit_24x24_passthrough(rle_stream, dst, dst_stride);
    } else if (mode == 2) {
        fd2_tile_blit_24x24_solid_color(rle_stream, dst, dst_stride, color);
    }
}

/* ----------------------------------------------------------------
 * fd2_render_combat_combatant_panels @ 0x1E611 (1 caller)
 *
 * Battle VS-panel renderer: rebuilds the battle backdrop, then draws
 * the attacker's portrait panel + HP bar, and (optionally) the
 * defender's panel + HP bar, finally blitting the composed 312x192
 * region to the mode13h primary surface.
 *
 * xy is a 4-int array:
 *   [0] = attacker_x;  [1] = attacker_y
 *   [2] = defender_x (-1 = no defender panel);  [3] = defender_y
 *
 * Pipeline (asm order):
 *   // 1. rebuild backdrop
 *   fd2_composite_battle_tile_map(ws + 0x8088, 456, 13, 8, origin_x, origin_y);
 *   fd2_composite_all_chars_overlay();
 *   // 2. attacker panel (sprite 0x30) + HP segments + proportional bar
 *   fd2_alloc_and_blit_indexed_sprite_chunk(portrait_sheet, ws + 0x8088, 456,
 *                                           attacker_x - 4, attacker_y - 4, 0x30);
 *   fd2_render_combat_hp_bar_segments(
 *       ws + 0x808B + (attacker_y+2)*456 + attacker_x, 456, 0x37);
 *   fd2_render_combatant_hp_bar_proportional(ws + 0x7964, 456, attacker_idx, &xy[0]);
 *   // 3. defender panel (only if xy[2] != -1)
 *   if (xy[2] != -1) {
 *       fd2_alloc_and_blit_indexed_sprite_chunk(portrait_sheet, ws + 0x8088, 456,
 *                                               defender_x - 4, defender_y - 4, 0x30);
 *       fd2_render_combatant_hp_bar_proportional(ws + 0x7964, 456, defender_idx, &xy[2]);
 *   }
 *   // 4. blit composed frame
 *   fd2_blit_rectangle(0xA0504, 320, ws + 0x8088, 456, 312, 192);
 *
 * The original tail-calls the shared blit epilogue inside
 * fd2_composite_battle_frame (JMP 0x11D2C, which pushes the 0xA0504/320
 * destination args and calls fd2_blit_rectangle); the C equivalent is
 * the explicit fd2_blit_rectangle call below.
 *
 * Constants: 0x8088 = backdrop base, 0x808B = +3 into that, 0x7964 =
 * HP-bar-state base, 0x37 = 55 (HP-segment bar width), 0x30 = combatant
 * panel sprite, 0x1C8 = 456 (workspace pitch), 0x140 = 320 (primary
 * stride), 0x138 = 312 / 0xC0 = 192 (visible region).
 *
 * ws = data_fd2_large_game_state_buffer_ptr. Sole caller:
 * fd2_execute_ai_physical_attack (AI attack VS-panel display).
 * ---------------------------------------------------------------- */
void fd2_render_combat_combatant_panels(uint32 xy_array_ptr, uint32 defender_idx,
                                        uint32 attacker_idx)
{
    int *xy;
    uint32 hp_seg_dst;

    xy = (int *)xy_array_ptr;

    fd2_composite_battle_tile_map(data_fd2_large_game_state_buffer_ptr + 0x8088,
                                  0x1c8, 0xd, 8,
                                  data_fd2_battle_view_window_origin_x,
                                  data_fd2_battle_view_window_origin_y);
    fd2_composite_all_chars_overlay();

    fd2_alloc_and_blit_indexed_sprite_chunk(
        data_fd2_resource_portrait_sheet_ptr,
        data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8,
        (uint32)(xy[0] - 4), (uint32)(xy[1] - 4), 0x30);

    hp_seg_dst = data_fd2_large_game_state_buffer_ptr + 0x808b +
                 (uint32)((xy[1] + 2) * 0x1c8) + (uint32)xy[0];
    fd2_render_combat_hp_bar_segments(hp_seg_dst, 0x1c8, 0x37);

    fd2_render_combatant_hp_bar_proportional(
        data_fd2_large_game_state_buffer_ptr + 0x7964, 0x1c8,
        attacker_idx, xy_array_ptr);

    if (xy[2] != -1) {
        fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_resource_portrait_sheet_ptr,
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8,
            (uint32)(xy[2] - 4), (uint32)(xy[3] - 4), 0x30);
        fd2_render_combatant_hp_bar_proportional(
            data_fd2_large_game_state_buffer_ptr + 0x7964, 0x1c8,
            defender_idx, xy_array_ptr + 8);
    }

    fd2_blit_rectangle(0xa0504, 0x140,
                       data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8,
                       0x138, 0xc0);
}

/* ----------------------------------------------------------------
 * fd2_render_combat_hp_bar_segments @ 0x1E739 (3 callers)
 *
 * Draw the segmented HP bar inside a combat VS panel by blitting a
 * horizontal run of 1-pixel-wide sprite segments from the UI/anim
 * sprite sheet, advancing dst_addr by one byte per segment.
 *
 * filled_count is the number of "filled" head segments to draw:
 *
 *   filled_count <= 0 (empty bar):
 *     for seg in 0..0x44:  blit sprite 0x1D (empty middle) at dst+seg
 *     then one sprite 0x1E (final right cap) at dst+0x45
 *
 *   filled_count > 0:
 *     blit sprite 0x17 (left cap) at dst
 *     for seg in 1..filled_count-1:  blit sprite 0x18 (filled middle) at dst+seg
 *     blit sprite 0x19 (fill right cap) at dst+filled_count
 *     if filled_count > 0x45: return     (exceeds max width, no final cap)
 *     for the remaining middles up to seg 0x45: blit sprite 0x1D (empty middle)
 *     then one sprite 0x1E (final right cap)
 *
 * Both the empty-bar loop exit and the filled-bar empty-middle loop
 * break fall through to a single trailing sprite-0x1E blit whose dst
 * is the last computed dst_addr + segment index (asm: shared EAX held
 * across the merge at 0x1E7DF).
 *
 * Sprite encoding (UI/anim sheet, sprite_idx):
 *   0x17 left cap | 0x18 filled middle | 0x19 fill right cap
 *   0x1D empty middle | 0x1E final right cap
 * Bar max width = 0x45 (69) segments.
 *
 * Callers: fd2_render_combat_combatant_panels,
 *          fd2_render_combatant_hp_bar_proportional,
 *          fd2_animate_combat_hit_with_hp_drain.
 * ---------------------------------------------------------------- */
void fd2_render_combat_hp_bar_segments(uint32 dst_addr, uint32 stride,
                                       uint32 filled_count)
{
    uint32 seg_addr;
    uint32 seg;

    seg = 0;
    if ((int32)filled_count < 1) {
        for (; seg_addr = dst_addr + seg, (int32)seg < 0x45; seg = seg + 1) {
            fd2_blit_sheet_sprite_at_offset(seg_addr, stride,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x1d);
        }
    } else {
        fd2_blit_sheet_sprite_at_offset(dst_addr, stride,
                                        data_fd2_ui_anim_sprite_sheet_ptr,
                                        0x17);
        for (seg = 1; (int32)seg < (int32)filled_count; seg = seg + 1) {
            fd2_blit_sheet_sprite_at_offset(dst_addr + seg, stride,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x18);
        }
        fd2_blit_sheet_sprite_at_offset(dst_addr + seg, stride,
                                        data_fd2_ui_anim_sprite_sheet_ptr,
                                        0x19);
        if (0x45 < (int32)filled_count) {
            return;
        }
        for (;;) {
            seg_addr = dst_addr + seg + 1;
            if (0x44 < (int32)seg) {
                break;
            }
            fd2_blit_sheet_sprite_at_offset(seg_addr, stride,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x1d);
            seg = seg + 1;
        }
    }
    fd2_blit_sheet_sprite_at_offset(seg_addr, stride,
                                    data_fd2_ui_anim_sprite_sheet_ptr,
                                    0x1e);
}

/* ----------------------------------------------------------------
 * fd2_render_combatant_hp_bar_proportional @ 0x1E7F6 (4 callers)
 *
 * Draw a proportional HP bar inside a combatant panel. Reads the
 * combatant's current/max HP, scales the bar fill to its HP fraction,
 * and forwards the filled-segment count to the shared segmented-bar
 * renderer (fd2_render_combat_hp_bar_segments).
 *
 *   char = runtime_char_array[char_idx]
 *   if (char.hp_current == 0) return;            // dead -> draw nothing
 *                                                // (panel is normally hidden)
 *   segments = char.hp_current * 0x45 / char.hp_max + 1
 *              // 0x45 = 69 = max bar width; +1 keeps a >=1 head while alive
 *   bar_addr = dst_buf + anchor.x + 7            // x + 7 left inset
 *            + (anchor.y + 6) * stride           // y + 6 row offset
 *   fd2_render_combat_hp_bar_segments(bar_addr, stride, segments);
 *
 * The HP scale/divide is the signed asm sequence (IMUL .,0x45 then
 * SAR/IDIV); hp_current is a zero-extended word, so the guard's signed
 * "<= 0" reduces to "== 0". anchor_xy_ptr points at a 2-int {x, y} pair
 * supplied by fd2_render_combat_combatant_panels (the panel position).
 *
 * 0x45 = 69 (max bar width), 6 = HP-bar row within the panel, 7 = left
 * inset within the panel.
 *
 * 4 callers: fd2_execute_ai_physical_attack (x2),
 * fd2_render_combat_combatant_panels (x2 attacker + defender).
 * ---------------------------------------------------------------- */
void fd2_render_combatant_hp_bar_proportional(uint32 dst_buf, uint32 stride,
                                              uint32 char_idx, uint32 anchor_xy_ptr)
{
    runtime_char *pchar;
    int *anchor;
    int32 hp_scaled;
    int32 segments;
    uint32 bar_addr;

    pchar = &data_fd2_battle_runtime_char_array_ptr[char_idx];
    if (pchar->hp_current == 0) {
        return;
    }

    anchor = (int *)anchor_xy_ptr;
    hp_scaled = (int32)pchar->hp_current * 0x45;
    segments = hp_scaled / (int32)pchar->hp_max + 1;

    bar_addr = (uint32)((anchor[1] + 6) * (int32)stride) + dst_buf +
               (uint32)anchor[0] + 7;

    fd2_render_combat_hp_bar_segments(bar_addr, stride, (uint32)segments);
}

/* ----------------------------------------------------------------
 * fd2_render_phase_banner_frame @ 0x1F42D (2 callers)
 *
 * Render one frame of the "PLAYER TURN" / "ENEMY TURN" phase banner: a
 * pair of sprites forming the two halves of the banner, painted into the
 * battle render workspace, then blitted to the mode13h primary surface.
 *
 * Pipeline (asm order):
 *   left_buf  = fd2_alloc_and_blit_indexed_sprite_chunk(
 *                   ui_anim_sprite_sheet, ws + 0x8088, 456,
 *                   0x55 - x_offset, 0x52, banner_sprite_id);
 *                   // main banner sprite (left half / centre)
 *   right_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
 *                   ui_anim_sprite_sheet, ws + 0x8088, 456,
 *                   x_offset + 0xA5, 0x52, 0x51);
 *                   // 0x51 = right-side frame corner sprite
 *   fd2_blit_rectangle(0xA0504, 320, ws + 0x8088, 456, 312, 192);
 *   fd2_wait_n_bios_ticks(1);
 *   fd2_cleanup_dialog_sprite_buffer(left_buf,  ws + 0x8088, 456);
 *   fd2_cleanup_dialog_sprite_buffer(right_buf, ws + 0x8088, 456);
 *
 * x_offset slides 0 (settled) .. 0x64 (each half 100px off-centre) as the
 * banner animates in/out. banner_sprite_id: 0x50 = PLAYER TURN,
 * 0x52 = ENEMY TURN. The corner sprite 0x51 is fixed.
 *
 * Constants: 0x8088 = render workspace base, 0x1C8 = 456 (workspace pitch),
 * 0x52 = banner sprite row, 0xA0504 = first visible mode13h pixel,
 * 0x140 = 320 (primary stride), 0x138 = 312 / 0xC0 = 192 (visible region).
 *
 * NOTE (Ghidra EAX-tracking bug): the decompiler attributed left_buf to
 * the __CHK stack-probe return and lost right_buf entirely (rendering it as
 * the workspace pointer). The assembly is unambiguous: MOV ESI,EAX after the
 * first chunk call (left_buf) and MOV EBX,EAX after the second (right_buf);
 * those two saved registers are exactly the saved_block args of the two
 * cleanup calls. The terminal JMP 0x184BA is a tail-jump into another
 * function's shared epilogue (ADD ESP,0xC; POP ESI; POP EBX; RET) that cleans
 * the last cleanup call's args and returns — the C equivalent is simply the
 * second cleanup call followed by return.
 *
 * ws = data_fd2_large_game_state_buffer_ptr. Called per frame by
 * fd2_animate_phase_banner_slide_in / fd2_animate_phase_banner_slide_out.
 * ---------------------------------------------------------------- */
void fd2_render_phase_banner_frame(uint32 x_offset, uint32 banner_sprite_id)
{
    uint32 left_buf;
    uint32 right_buf;
    uint32 ws;

    ws = data_fd2_large_game_state_buffer_ptr + 0x8088;

    left_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
        data_fd2_ui_anim_sprite_sheet_ptr, ws, 0x1c8,
        0x55 - x_offset, 0x52, banner_sprite_id);
    right_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
        data_fd2_ui_anim_sprite_sheet_ptr, ws, 0x1c8,
        x_offset + 0xa5, 0x52, 0x51);

    fd2_blit_rectangle(0xa0504, 0x140, ws, 0x1c8, 0x138, 0xc0);
    fd2_wait_n_bios_ticks(1);

    fd2_cleanup_dialog_sprite_buffer(left_buf, ws, 0x1c8);
    fd2_cleanup_dialog_sprite_buffer(right_buf, ws, 0x1c8);
}

/* ----------------------------------------------------------------
 * fd2_composite_then_animate_projectiles @ 0x21190 (6 tail-JMP entry sites)
 *
 * Spell-finale helper: recomposite the battle frame, then run the
 * queued spell projectile-path animation. This is the common tail of
 * every offensive/heal spell handler.
 *
 * Body (asm 0x21190..0x2119f):
 *   fd2_composite_battle_frame(0);
 *   fd2_animate_spell_projectile_paths();
 *
 * In the binary this body is immediately followed by a shared epilogue
 * (0x211A0..0x211A3: POP EBP / POP EDI / POP ESI / POP EBX / RET) that
 * restores the *parent's* saved registers and returns to the parent's
 * caller. Every reaching edge is a tail-JMP (4 conditional JGE
 * early-exits plus 2 unconditional terminal JMPs), never a CALL, so
 * the body and the parent register-restore are physically shared. The
 * six reaching parents are fd2_apply_attack_spell_damage,
 * fd2_cast_group_hp_heal_spell, fd2_execute_offensive_targeted_spell,
 * fd2_execute_offensive_targeted_spell_variant_b,
 * fd2_execute_offensive_full_screen_flash_spell and
 * fd2_dispatch_variant_b_cast.
 *
 * Layer-2 equivalent: emit only the body as a plain no-arg helper. Each
 * parent calls it at its tail; the compiler regenerates that parent's
 * own register-restore epilogue, which is exactly what the shared POP
 * sequence performed. Takes no arguments (__cdecl, param_count=0).
 * ---------------------------------------------------------------- */
void fd2_composite_then_animate_projectiles(void)
{
    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}

/* ----------------------------------------------------------------
 * fd2_render_circle_anim_row @ 0x219AD (2 callers, 3 sites)
 *
 * Render the horizontal slices of a filled-circle (band) animation
 * across a row range, palette-remapping a sprite-sourced run into the
 * battle render workspace one scanline at a time. For each row in
 * [start_row, end_row) that lies strictly inside the circle's vertical
 * extent (cy-r, cy+r), it computes the scanline's half-width from the
 * circle equation, clamps the run to the visible 0..0x138 span, and
 * forwards the clamped run to fd2_apply_palette_remap_run.
 *
 * Geometry per row:
 *   dy         = abs(cy - row)
 *   half_width = trunc( sqrt(r*r - dy*dy) * scale_num / 10.0 )
 *     // x87: FILD/FMULP/FDIV[10.0] then __CHP forces RC=round-toward-
 *     // zero before FRNDINT, so this is a TRUNCATION toward zero, not a
 *     // round-to-nearest (the Ghidra ROUND() macro is misleading here).
 *   left_clip  = cx - half_width, right_off = half_width;
 *   if (left_clip < 0)  { left_clip = 0; right_off = cx; }      // clamp left to 0
 *   if (cx + half_width > 0x137) half_width = 0x138 - cx;       // clamp right to 0x138
 *   run_width = right_off + half_width;
 *   dst = large_game_state_buffer + 0x8088 + row*0x1C8 + left_clip;
 *   fd2_apply_palette_remap_run(palette_remap_src, run_width, dst);
 *
 * scale_num shapes the band thickness/curvature (callers pass 0xC for
 * the rising-sparkle effect, 0x10 for the filled-circle band). The
 * 7th arg is the per-frame sprite/palette-remap source row passed
 * straight through as the remap's 1st arg.
 *
 * 0x8088 = char-layer base in the render workspace, 0x1C8 = 456
 * (workspace pitch), 0x138 = 312 (visible clipped width).
 *
 * The original has no explicit RET: it tail-jumps (JGE 0x1951B) to a
 * shared Watcom epilogue (ADD ESP,0x14 + POP EBP/EDI/ESI/EBX + RET) that
 * another same-frame-shape function ends with; the C equivalent is the
 * loop simply running to completion.
 *
 * 2 callers (3 sites): fd2_play_rising_pre_cast_effect,
 * fd2_render_filled_circle_band_anim (x2).
 * ---------------------------------------------------------------- */
void fd2_render_circle_anim_row(int cx, int cy, int r, int scale_num,
                                int start_row, int end_row,
                                uint8 *palette_remap_src)
{
    int32 dy;
    int32 rsq_minus_dy2;
    uint32 half_width;
    uint32 left_clip;
    uint32 right_off;

    for (; start_row < end_row; start_row++) {
        if ((cy - r < start_row) && (start_row < cy + r)) {
            dy = abs(cy - start_row);
            rsq_minus_dy2 = r * r - dy * dy;
            half_width = (uint32)(int32)
                (sqrt((double)rsq_minus_dy2) * scale_num /
                 data_fd2_graphics_circle_anim_div_10);

            left_clip = (uint32)cx - half_width;
            right_off = half_width;
            if ((int32)left_clip < 0) {
                left_clip = 0;
                right_off = (uint32)cx;
            }
            if (0x137 < (int32)(half_width + (uint32)cx)) {
                half_width = 0x138 - (uint32)cx;
            }

            fd2_apply_palette_remap_run(
                (uint32)palette_remap_src, right_off + half_width,
                (uint8 *)(left_clip + data_fd2_large_game_state_buffer_ptr +
                          0x8088 + (uint32)start_row * 0x1c8));
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_render_filled_circle_band_anim @ 0x22046 (5 callers, 6 sites)
 *
 * Render one frame of a filled circular band animation: a top arc, a
 * bottom arc, and a solid middle band, with the live party characters
 * composited on top (the AoE-with-chars overlay look). Built from two
 * fd2_render_circle_anim_row passes plus a straight rectangular fill.
 *
 * Param overloading note (Ghidra names kept verbatim):
 *   param_1 = column center cx (workspace pixels)
 *   param_2 = bottom row index (band-fill end, and the bottom arc's
 *             vertical center / row start)
 *   param_3 = radius factor for the band half-width
 *   cx      = top band-fill start row (also the top arc's row start)
 *   cy      = arc row-loop exclusive end (5th arg)
 *   radius  = the palette-remap SOURCE pointer forwarded straight
 *             through to every fd2_apply_palette_remap_run / row call
 *             (the name "radius" is the Ghidra label, not a length)
 *
 * Sequence (asm order):
 *   fd2_render_circle_anim_row(param_1, param_2, param_3, 0x10, cx, cy, radius);
 *     // top arc rows
 *   fd2_composite_all_chars_overlay();        // paint party chars on top
 *   fd2_render_circle_anim_row(param_1, param_2, param_3, 0x10, param_2, cy, radius);
 *     // bottom arc rows (5th arg = param_2 = bottom_y, not cx)
 *
 *   // solid middle band:
 *   half_width = trunc( (double)param_3 * 1.6 );
 *     // x87: FILD param_3 / FMUL m64[0x50208]=1.6 then __CHP forces
 *     // RC=round-toward-zero before FRNDINT, so this is a TRUNCATION
 *     // toward zero, not a round-to-nearest (Ghidra ROUND() misleads).
 *   left_clip = param_1 - half_width, right_off = half_width;
 *   if (left_clip < 0)  { left_clip = 0; right_off = param_1; }   // clamp left to 0
 *   if (param_1 + half_width > 0x137) half_width = 0x138 - param_1; // clamp right to 0x138
 *   run_width = half_width + right_off;
 *   row_ptr = large_game_state_buffer + 0x8088 + cx*0x1C8 + left_clip;
 *   for (; cx < param_2; cx++) {
 *       fd2_apply_palette_remap_run(radius, run_width, row_ptr);
 *       row_ptr += 0x1C8;
 *   }
 *
 * 0x8088 = char-layer base in the render workspace, 0x1C8 = 456
 * (workspace pitch), 0x138 = 312 (visible clipped width).
 * 1.6 = data_fd2_graphics_circle_band_radius_scale_16 (0x50208).
 *
 * The original has no explicit RET: on loop exit (JGE 0x21DAD) it tail-
 * jumps into the shared POP EBP/EDI/ESI/EBX + RET that the orphan fn
 * @ 0x21BD0 ends with (same PUSH EBX/ESI/EDI/EBP frame shape, so Watcom
 * merges the register-restore epilogue); the C equivalent is the loop
 * simply running to completion.
 *
 * 5 callers (6 sites): fd2_play_variant_b_slide_pre_effect (x2),
 * fd2_animate_warp_teleport_char, fd2_animate_warp_out_collapse,
 * fd2_animate_warp_in_expand, fd2_cast_screen_wide_spell_with_fade.
 * ---------------------------------------------------------------- */
void fd2_render_filled_circle_band_anim(uint32 param_1, uint32 param_2,
                                        uint32 param_3, int cx, int cy,
                                        int radius)
{
    uint32 half_width;
    uint32 left_clip;
    uint32 right_off;
    uint32 row_ptr;

    fd2_render_circle_anim_row(param_1, param_2, param_3, 0x10, cx, cy,
                               (uint8 *)radius);
    fd2_composite_all_chars_overlay();
    fd2_render_circle_anim_row(param_1, param_2, param_3, 0x10, param_2, cy,
                               (uint8 *)radius);

    half_width = (uint32)(int32)((double)(int32)param_3 *
                                 data_fd2_graphics_circle_band_radius_scale_16);

    left_clip = param_1 - half_width;
    right_off = half_width;
    if ((int32)left_clip < 0) {
        left_clip = 0;
        right_off = param_1;
    }
    if (0x137 < (int32)(half_width + param_1)) {
        half_width = 0x138 - param_1;
    }

    row_ptr = left_clip + data_fd2_large_game_state_buffer_ptr + 0x8088 +
              (uint32)cx * 0x1c8;
    for (; cx < (int32)param_2; cx++) {
        fd2_apply_palette_remap_run(radius, half_width + right_off,
                                    (uint8 *)row_ptr);
        row_ptr += 0x1c8;
    }
}

/* ----------------------------------------------------------------
 * fd2_composite_battle_frame_zero @ 0x22BB7 (2 reaching entry sites)
 *
 * Zero-arg dispatch helper: recomposite the battle frame with the
 * palette-cycle-advancing variant (skip_palette_cycle == 0). The whole
 * body is just `PUSH 0x0; CALL fd2_composite_battle_frame`.
 *
 * In the binary the body is immediately followed by a shared epilogue
 * (0x22BBE..0x22BC5: ADD ESP,4 / POP EBP / POP EDI / POP ESI / POP EBX /
 * RET) that restores the *parent's* saved registers — entry 0x22BB7 pops
 * those registers without ever pushing them at entry, relying on the
 * reaching frame to have saved them. The two reaching sites are
 * fd2_cast_status_cure_spell (conditional tail-JMP @ 0x22B49) and
 * fd2_play_chapter_intro_sprite_slideshow (unconditional CALL @ 0x244B1);
 * the +0x22BBE label is additionally reused as a folded epilogue by many
 * other functions (pure Watcom epilogue-fold, no source-level meaning).
 *
 * Layer-2 equivalent: emit only the body as a plain no-arg helper; the
 * compiler regenerates each reaching parent's own register-restore
 * epilogue, which is exactly what the shared POP sequence performed.
 * Takes no arguments (__cdecl, param_count=0).
 * ---------------------------------------------------------------- */
void fd2_composite_battle_frame_zero(void)
{
    fd2_composite_battle_frame(0);
}
