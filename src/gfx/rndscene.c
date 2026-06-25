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
 * File-scope render state (zero-initialized at startup)
 * ---------------------------------------------------------------- */

/* @ 0x53A04 -- character-sprite sleep shake-jitter toggle bit (u8, value
 * 0 or 1). Read/written by fd2_paint_char_sprite_at_world_pos: when the
 * BIOS tick changes the bit is flipped (bit ^= 1), and when the painted
 * char has the sleep status the bit is added onto blit_offset (a +0 or
 * +1 pixel shake). Zero-initialized at startup. Sibling of the tick
 * latch below at 0x53A08. */
uint8 data_fd2_graphics_char_sprite_shake_jitter_bit;

/* @ 0x53A08 -- last BIOS tick (signed, sign-extended from the 16-bit
 * counter at 0x46C) that toggled the sleep shake jitter bit. Read/
 * written by fd2_paint_char_sprite_at_world_pos to flip the jitter bit
 * at most once per tick. Accessed as a 32-bit dword: the writer does
 * MOVSX EAX,word[0x46C] then CMP EAX,[0x53A08] / MOV [0x53A08],EAX, so
 * it holds a sign-extended tick. Zero-initialized at startup. */
int32 data_fd2_graphics_char_sprite_paint_jitter_tick_latch;

/* @ 0x53C1F -- battle tile-map animation sub-counter (the "compose
 * state_b" frame index). Free-running 0..19 cycle that advances once
 * every 3 BIOS ticks; wraps at 0x14 (20). Used as a byte index into
 * data_fd2_graphics_tile_anim_palette_phase_lookup[20] to pick the
 * per-frame tile palette-remap phase. Accessed as a 32-bit dword:
 * fd2_composite_battle_tile_map does INC/CMP/MOV dword[0x53C1F] for the
 * free-running advance (or loads it from the forced-frame override),
 * and fd2_execute_ai_item_use resets it to 0 then drives it 1..8 for
 * the long-range item-cast burst animation. Zero-initialized at
 * startup (memory image is all zero; first free-running use is a
 * read-modify-write increment). Multi-writer (also written from
 * battle AI item-use path). */
uint32 data_fd2_battle_tile_map_anim_frame_counter;

/* @ 0x539FC -- background shimmer animation frame index (u32). Passed by
 * fd2_composite_battle_tile_map to fd2_blit_buffer_with_per_row_offset as the
 * starting per-row offset into the 16-entry shimmer table; incremented once
 * per tick and wrapped at 0x10 on the animated-background chapters (9, 0x18,
 * 0x19, 0x1C, 0x1D). Zero-initialized at startup. */
uint32 data_fd2_graphics_bg_animation_frame_idx;

/* @ 0x51A93 -- forced tile-animation frame override (u32). The default
 * sentinel 0xFFFFFFFF lets the tile-map palette-phase counter free-run; any
 * other value would be copied into data_fd2_battle_tile_map_anim_frame_counter
 * by fd2_composite_battle_tile_map to pin the tile palette phase. The shipped
 * binary only reads this (no writer), so it stays at the sentinel;
 * initialized to 0xFFFFFFFF. */
uint32 data_fd2_graphics_forced_tile_anim_frame = 0xffffffff;

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
 *   0x1C8 = 456 -- workspace pitch
 *   0x140 = 320 -- mode13h primary stride
 *   0x138 = 312 -- visible clipped width
 *   0xC0  = 192 -- visible clipped height
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
    fd2_redraw_terrain_tiles_under_chars();
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
 *   - Sprite lookup through data_fd2_portrait_sprite_cache:
 *       idx  = facing*3 + sprite_state[0]*0xC + palette_idx
 *       base = data_fd2_portrait_sprite_cache
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

    if (pchar->status_paralysis_flag != 0) {
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
    if (pchar->status_paralysis_flag != 0) {
        palette_idx = 0;
    }

    sprite_idx = facing * 3 + (uint32)pchar->sprite_state[0] * 0xc + palette_idx;
    cache_base = data_fd2_portrait_sprite_cache;
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
 * fd2_redraw_terrain_tiles_under_chars @ 0x129EC (4 callers)
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
void fd2_redraw_terrain_tiles_under_chars(void)
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
void fd2_paint_threat_overlay_for_team(uint32 team_selector)
{
    uint32 i;
    runtime_char *pchar;

    for (i = 0; (int32)i < (int32)data_fd2_battle_party_member_count; i++) {
        pchar = &data_fd2_battle_runtime_char_array_ptr[i];
        if ((pchar->flags & 1) == 0) {
            if (((team_selector == 0) && (pchar->team != 0)) ||
                ((team_selector != 0) && (pchar->team == 0))) {
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
            src_ptr = data_fd2_portrait_sprite_cache +
                      (uint32)*(int32 *)(data_fd2_portrait_sprite_cache + (uint32)frame_idx * 4);
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
 *   rle_stream = data_fd2_portrait_sprite_cache
 *              + *(int32 *)(data_fd2_portrait_sprite_cache + sprite_idx*4)
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
    rle_stream = data_fd2_portrait_sprite_cache +
                 (uint32)*(int32 *)(data_fd2_portrait_sprite_cache + sprite_idx * 4);

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
 *                   ui_anim_sprite_sheet, ws, 456,
 *                   0x55 - x_offset, 0x52, banner_sprite_id);
 *                   // main banner sprite (left half / centre)
 *   right_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
 *                   ui_anim_sprite_sheet, ws, 456,
 *                   x_offset + 0xA5, 0x52, 0x51);
 *                   // 0x51 = right-side frame corner sprite
 *   fd2_blit_rectangle(0xA0504, 320, ws, 456, 312, 192);
 *   fd2_wait_n_bios_ticks(1);
 *   fd2_cleanup_dialog_sprite_buffer(left_buf,  ws, 456);
 *   fd2_cleanup_dialog_sprite_buffer(right_buf, ws, 456);
 *
 * x_offset slides 0 (settled) .. 0x64 (each half 100px off-centre) as the
 * banner animates in/out. banner_sprite_id: 0x50 = PLAYER TURN,
 * 0x52 = ENEMY TURN. The corner sprite 0x51 is fixed.
 *
 * Constants: ws = data_fd2_large_game_state_buffer_ptr + 0x8088 (render
 * workspace), 0x1C8 = 456 (workspace pitch), 0x52 = banner sprite row,
 * 0xA0504 = first visible mode13h pixel, 0x140 = 320 (primary stride),
 * 0x138 = 312 / 0xC0 = 192 (visible region).
 *
 * NOTE (Ghidra EAX-tracking bug): the decompiler attributed left_buf to
 * the __CHK stack-probe return and lost right_buf entirely (rendering it as
 * the workspace pointer). The assembly is unambiguous: MOV ESI,EAX after the
 * first chunk call (left_buf) and MOV EBX,EAX after the second (right_buf);
 * those two saved registers are exactly the saved_block args of the two
 * cleanup calls. The terminal JMP 0x184BA is a tail-jump into another
 * function's shared epilogue (ADD ESP,0xC; POP ESI; POP EBX; RET) that cleans
 * the last cleanup call's args and returns -- the C equivalent is simply the
 * second cleanup call followed by return.
 *
 * Called per frame by fd2_animate_phase_banner_slide_in /
 * fd2_animate_phase_banner_slide_out.
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
 * fd2_execute_variant_b_heal_cast.
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
 * Param note (names reflect the overloaded roles, not size/coords):
 *   col_center        = column center cx (workspace pixels)
 *   bottom_row        = bottom row index (band-fill end, and the bottom
 *                       arc's vertical center / row start)
 *   radius_factor     = radius factor for the band half-width
 *   top_row           = top band-fill start row (also top arc's row start)
 *   row_loop_end      = arc row-loop exclusive end (5th arg)
 *   palette_remap_src = the palette-remap SOURCE pointer forwarded
 *                       straight through to every
 *                       fd2_apply_palette_remap_run / row call (a uint8*
 *                       color-remap table, not a length)
 *
 * Sequence (asm order):
 *   fd2_render_circle_anim_row(col_center, bottom_row, radius_factor, 0x10, top_row, row_loop_end, palette_remap_src);
 *     // top arc rows
 *   fd2_composite_all_chars_overlay();        // paint party chars on top
 *   fd2_render_circle_anim_row(col_center, bottom_row, radius_factor, 0x10, bottom_row, row_loop_end, palette_remap_src);
 *     // bottom arc rows (5th arg = bottom_row = bottom_y, not top_row)
 *
 *   // solid middle band:
 *   half_width = trunc( (double)radius_factor * 1.6 );
 *     // x87: FILD radius_factor / FMUL m64[0x50208]=1.6 then __CHP forces
 *     // RC=round-toward-zero before FRNDINT, so this is a TRUNCATION
 *     // toward zero, not a round-to-nearest (Ghidra ROUND() misleads).
 *   left_clip = col_center - half_width, right_off = half_width;
 *   if (left_clip < 0)  { left_clip = 0; right_off = col_center; }   // clamp left to 0
 *   if (col_center + half_width > 0x137) half_width = 0x138 - col_center; // clamp right to 0x138
 *   run_width = half_width + right_off;
 *   row_ptr = large_game_state_buffer + 0x8088 + top_row*0x1C8 + left_clip;
 *   for (; top_row < bottom_row; top_row++) {
 *       fd2_apply_palette_remap_run(palette_remap_src, run_width, row_ptr);
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
void fd2_render_filled_circle_band_anim(uint32 col_center, uint32 bottom_row,
                                        uint32 radius_factor, int top_row,
                                        int row_loop_end, int palette_remap_src)
{
    uint32 half_width;
    uint32 left_clip;
    uint32 right_off;
    uint32 row_ptr;

    fd2_render_circle_anim_row(col_center, bottom_row, radius_factor, 0x10,
                               top_row, row_loop_end, (uint8 *)palette_remap_src);
    fd2_composite_all_chars_overlay();
    fd2_render_circle_anim_row(col_center, bottom_row, radius_factor, 0x10,
                               bottom_row, row_loop_end, (uint8 *)palette_remap_src);

    half_width = (uint32)(int32)((double)(int32)radius_factor *
                                 data_fd2_graphics_circle_band_radius_scale_16);

    left_clip = col_center - half_width;
    right_off = half_width;
    if ((int32)left_clip < 0) {
        left_clip = 0;
        right_off = col_center;
    }
    if (0x137 < (int32)(half_width + col_center)) {
        half_width = 0x138 - col_center;
    }

    row_ptr = left_clip + data_fd2_large_game_state_buffer_ptr + 0x8088 +
              (uint32)top_row * 0x1c8;
    for (; top_row < (int32)bottom_row; top_row++) {
        fd2_apply_palette_remap_run(palette_remap_src, half_width + right_off,
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
 * fd2_play_chapter_21_hidden_stage_unlock_cinematic (unconditional CALL @ 0x244B1);
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

/* ----------------------------------------------------------------
 * fd2_render_summon_aura_sprite_ring @ 0x262EF (0 direct callers)
 *
 * Renders the 8-directional summon-spell aura sprite ring around the
 * caster. Entry +1 (offset +0x04) of the 10-element summon-spell tick
 * dispatch table data_fd2_battle_spell_cast_cinematic_phase_handler_table
 * @ 0x523B9; invoked indirectly through that table, no direct CALL xref.
 *
 * Prologue copies two 8-entry .rodata tables into local frame:
 *   data_fd2_battle_summon_aura_ring_8slot_x_offset_table   @ 0x52420
 *   data_fd2_battle_summon_aura_ring_8slot_row_multiplier_table @ 0x52440
 *
 * Per-slot phase counters live in the upper 8 slots [7..14] of the shared
 * 15-slot frame-counter array (asm indexes [EBX*4 + 0x53F5E], and
 * 0x53F5E - 0x53F42 = 0x1C = 7 int32s), i.e. counter[i + 7].
 *
 * Team adjust: if caster's bTeam (runtime_char +6) == 0 (enemy), shift
 * every slot's baseline X by 0x94.
 *
 * state_code dispatch:
 *   3 (INIT)  : counter[i+7] = -2*i for i in 0..7 ; return 0x1F.
 *   4 (BLIT N->S): i in 0..3 blit unrotated frame=counter[i+7] at slot i;
 *                  i in 4..7 blit tail frame=counter[i+7]+0xF at slot
 *                  j=(i+4)%8 ; return 0.
 *   5 (BLIT S->N + ADVANCE, mirror of 4): i in 0..3 tail frame+0xF at
 *                  slot j=(i+4)%8 ; i in 4..7 unrotated frame at slot i ;
 *                  then advance every counter[i+7]: if ==9 set done; if
 *                  ==5 play per-slot chime SFX ; return done.
 *   other     : return 0.
 *
 * Params (caller fd2_play_spell_cast_sequence pushes, per phase):
 *   caster_unit_id : runtime_char index for the team-baseline test.
 *   sprite_handle  : sprite-sheet handle (blit sheet_ptr).
 *   dst_buf_base   : destination work-buffer base (caller passes the
 *                    0x2A300 frame-scratch buffer or that + an offset);
 *                    it is the blit dst_buf base, NOT a y coordinate
 *                    despite the name.
 *   row_stride     : destination row stride (0x140 / 0x280); also the
 *                    per-slot row multiplier.
 *   state_code     : phase/state dispatch code.
 *
 * Blit gate per slot: 0 <= counter < 0xF. The blit dst_buf is
 * row_mul[k]*row_stride + x_off[k] + 0x50 + dst_buf_base; row_stride is the
 * blit dst_stride and -1 is the palette_op.
 *
 * cc __cdecl (caller cleans 5 stack args; callees blit/sfx are __cdecl).
 * System=battle.
 * ---------------------------------------------------------------- */
int fd2_render_summon_aura_sprite_ring(int caster_unit_id, int sprite_handle,
                                       int dst_buf_base, int row_stride,
                                       char state_code)
{
    int x_off[8];
    int row_mul[8];
    int i;
    int j;
    int done_flag;
    uint8 *pCaster;

    done_flag = 0;
    memcpy(x_off, data_fd2_battle_summon_aura_ring_8slot_x_offset_table, 32);
    memcpy(row_mul, data_fd2_battle_summon_aura_ring_8slot_row_multiplier_table,
           32);

    pCaster = (uint8 *)data_fd2_battle_runtime_char_array_ptr
            + caster_unit_id * RUNTIME_CHAR_SIZE;
    if (pCaster[6] == 0) {
        for (i = 0; i < 8; i++) {
            x_off[i] += 0x94;
        }
    }

    if (state_code == 3) {
        for (i = 0; i < 8; i++) {
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i + 7]
                = -2 * i;
        }
        return 0x1f;
    }

    if (state_code == 4) {
        for (i = 0; i < 4; i++) {
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] >= 0 &&
                data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] < 0xf) {
                fd2_blit_indexed_sprite(
                    (uint32)sprite_handle,
                    (uint32)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                        i + 7],
                    row_mul[i] * row_stride + x_off[i] + 0x50 + dst_buf_base,
                    row_stride, -1);
            }
        }
        for (; i < 8; i++) {
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] >= 0 &&
                data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] < 0xf) {
                j = (i + 4) % 8;
                fd2_blit_indexed_sprite(
                    (uint32)sprite_handle,
                    (uint32)(data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                        i + 7] + 0xf),
                    row_mul[j] * row_stride + x_off[j] + 0x50 + dst_buf_base,
                    row_stride, -1);
            }
        }
        return 0;
    }

    if (state_code == 5) {
        for (i = 0; i < 4; i++) {
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] >= 0 &&
                data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] < 0xf) {
                j = (i + 4) % 8;
                fd2_blit_indexed_sprite(
                    (uint32)sprite_handle,
                    (uint32)(data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                        i + 7] + 0xf),
                    row_mul[j] * row_stride + x_off[j] + 0x50 + dst_buf_base,
                    row_stride, -1);
            }
        }
        for (; i < 8; i++) {
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] >= 0 &&
                data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] < 0xf) {
                fd2_blit_indexed_sprite(
                    (uint32)sprite_handle,
                    (uint32)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                        i + 7],
                    row_mul[i] * row_stride + x_off[i] + 0x50 + dst_buf_base,
                    row_stride, -1);
            }
        }
        for (i = 0; i < 8; i++) {
            data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i + 7]
                += 1;
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] == 9) {
                done_flag = 1;
            }
            if (data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[
                    i + 7] == 5) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 1, 1);
            }
        }
        return done_flag;
    }

    return 0;
}

/* Background-animation flip flag (0x53A40). Every-other-frame toggle used by
   the battle tile-map compositor to swap animated tile sprites. Owned and
   driven by fd2_composite_battle_tile_map, which toggles it (flag ^= 1) once
   per BIOS-tick change; readers add it (or it*2) onto the tile id. Storage is
   a 32-bit slot read as a dword; the writer touches only the low byte (the
   value never leaves 0/1), so the upper bytes stay zero. Established at
   runtime, zero-initialized. */
uint32 data_fd2_graphics_bg_anim_flip_flag;

/* Per-frame BIOS-tick latches owned by fd2_composite_battle_tile_map (it reads
 * then writes each on a tick change). All three are game-written scratch state
 * (verified WRITE xrefs) -> mutable, zero-initialised; dword tick snapshots. */
uint32 data_fd2_battle_tile_anim_last_advance_tick;      /* 0x539F4 */
uint32 data_fd2_battle_bg_anim_last_advance_tick;        /* 0x539F8 */
uint32 data_fd2_graphics_battle_compose_flip_tick_latch; /* 0x53A00 */

/* ----------------------------------------------------------------
 * fd2_composite_battle_tile_map @ 0x11EEE (22 callers)
 *
 * Composite the battle map's tile layer onto the destination buffer
 * with chapter-aware background animation. First rendering pass of
 * fd2_composite_battle_frame.
 *
 * Signature (true 6-param order, confirmed by all 22 callers which pass
 * (ws, 456, 13, 8, origin_x, origin_y)):
 *   dst_buf       -- render workspace base
 *   dst_stride    -- destination/workspace pitch (456 = 0x1C8)
 *   n_cols        -- per-row tile count (inner loop bound, 13)
 *   n_rows        -- tile-row count (outer loop bound, 8)
 *   win_origin_x  -- battle view window origin tile X
 *   win_origin_y  -- battle view window origin tile Y
 * (Ghidra's decompiler scrambled these into param_1..3 + dst_buf/
 *  dst_stride/n_cols; the ESP-relative arg offsets and the caller call
 *  sites give the true order above.)
 *
 * Setup (animation tick management):
 *   if BIOS tick (signed word @ 0x46C) changed since last call:
 *     bg_anim_flip_flag ^= 1   (every-other-frame flip for tile swaps)
 *     compose_flip_tick_latch = current tick
 *
 * Chapter-specific background source (current_chapter_id dispatch):
 *   Chapters 9, 0x18, 0x19, 0x1C, 0x1D (animated):
 *     if tick changed: blit animated bg into static work buffer via the
 *       per-row offset table indexed by bg_animation_frame_idx (0..15);
 *       advance + wrap that index at 0x10.
 *     src = animated_bg_buffer; src_stride = 0x140.
 *   Chapters 0x11, 0x15, 0x16, 0x1B (extra-wide parallax):
 *     src = static_bg_buffer + (walk_anim_y_scroll/3)*stride
 *           + walk_anim_x_scroll/2 + win_origin_y*stride*2 + win_origin_x*3
 *     src_stride = 0x1CE (0x11/0x1B) or 0x198 (others).
 *   Chapter 0x17 (text-screen background):
 *     if tick changed: fd2_scroll_text_screen_up_by_lines(0).
 *     src = static_bg_buffer; src_stride = 0x138.
 *   Default: skip the background blit entirely (goto the tile pass).
 *
 * If a source resolved: fd2_blit_rectangle(dst + clip/scroll offset,
 *   dst_stride, src, src_stride, 0x138, 0xC0) -- 312x192 visible area.
 *
 * Tile-animation sub-counter (data_fd2_battle_tile_map_anim_frame_counter):
 *   forced_tile_anim_frame == -1 (free-running): advance once every 3
 *     BIOS ticks, wrapping at 0x14 (20 frames). Else: lock to the forced
 *     value.
 *   palette_remap = tile_anim_table_base
 *                 + *(tile_anim_table_base + 6
 *                     + tile_anim_palette_phase_lookup[counter]*4)
 *
 * Per-tile loop (n_rows x n_cols):
 *   pTile_meta = tile_map_ptr + ((row + win_origin_y)*map_width
 *                                 + win_origin_x)*4 + 4
 *   tile_id = meta.word[2] & 0x3FF; flags = attr_buffer[tile_id*4]
 *     flags & 0x08 : tile_id += bg_anim_flip_flag * 2
 *     flags & 0x10 : tile_id += chapter_ambient_palette_anim_idx / 2
 *     flags & 0x04 : tile_id += bg_anim_flip_flag
 *   sprite = scene_snapshot + *(scene_snapshot + 6 + tile_id*4)
 *   meta.byte[7] == -1 : passthrough blit; else remap blit (palette_remap)
 *
 * One of the hottest functions in the battle render path. __cdecl
 * (caller cleans 6 stack args; all callees __cdecl). No callee return
 * value is consumed (no EAX-after-CALL reads).
 * ---------------------------------------------------------------- */
void fd2_composite_battle_tile_map(uint32 dst_buf, uint32 dst_stride,
                                   uint32 n_cols, uint32 n_rows,
                                   uint32 win_origin_x, uint32 win_origin_y)
{
    int32 cur_tick;
    uint32 src_stride;
    uint32 bg_src;
    uint32 dst_off;
    uint32 base_off;
    int32 palette_remap;
    uint32 row_iter;
    int32 col_iter;
    uint32 pTile_meta;
    uint32 pDst_row;
    uint32 tile_id;
    uint8 tile_attr_flags;
    int32 tile_id_adjust;
    uint32 pTile_sprite;

    cur_tick = (int32)(int16)BIOS_TICK_WORD;
    if (cur_tick != (int32)data_fd2_graphics_battle_compose_flip_tick_latch) {
        data_fd2_graphics_bg_anim_flip_flag ^= 1;
        data_fd2_graphics_battle_compose_flip_tick_latch = (uint32)cur_tick;
    }

    if (data_fd2_chapter_current_chapter_id == 9 ||
        data_fd2_chapter_current_chapter_id == 0x18 ||
        data_fd2_chapter_current_chapter_id == 0x19 ||
        data_fd2_chapter_current_chapter_id == 0x1c ||
        data_fd2_chapter_current_chapter_id == 0x1d) {
        cur_tick = (int32)(int16)BIOS_TICK_WORD;
        if (cur_tick != (int32)data_fd2_battle_bg_anim_last_advance_tick) {
            fd2_blit_buffer_with_per_row_offset(
                data_fd2_graphics_static_bg_buffer_ptr,
                (uint32 *)data_fd2_graphics_animated_bg_buffer_ptr,
                data_fd2_graphics_bg_animation_frame_idx);
            data_fd2_battle_bg_anim_last_advance_tick = (uint32)cur_tick;
            data_fd2_graphics_bg_animation_frame_idx += 1;
            if (data_fd2_graphics_bg_animation_frame_idx == 0x10) {
                data_fd2_graphics_bg_animation_frame_idx = 0;
            }
        }
        src_stride = 0x140;
        dst_off = data_fd2_battle_compose_walk_step_sub_pixel_offset +
                  dst_buf + data_fd2_battle_compose_left_edge_clip_offset;
        base_off = data_fd2_battle_compose_parallax_scroll_y_rows * 0x1c8;
        bg_src = data_fd2_graphics_animated_bg_buffer_ptr;
    } else if (data_fd2_chapter_current_chapter_id == 0x11 ||
               data_fd2_chapter_current_chapter_id == 0x15 ||
               data_fd2_chapter_current_chapter_id == 0x16 ||
               data_fd2_chapter_current_chapter_id == 0x1b) {
        if (data_fd2_chapter_current_chapter_id == 0x11 ||
            data_fd2_chapter_current_chapter_id == 0x1b) {
            src_stride = 0x1ce;
        } else {
            src_stride = 0x198;
        }
        bg_src = data_fd2_graphics_static_bg_buffer_ptr +
                 (uint32)(data_fd2_battle_walk_anim_y_scroll_rows / 3) * src_stride +
                 (uint32)(data_fd2_battle_walk_anim_x_scroll_offset / 2) +
                 win_origin_y * src_stride * 2 + win_origin_x * 3;
        dst_off = dst_buf + data_fd2_battle_compose_left_edge_clip_offset +
                  data_fd2_battle_compose_walk_step_sub_pixel_offset;
        base_off = data_fd2_battle_compose_parallax_scroll_y_rows * 0x1c8;
    } else if (data_fd2_chapter_current_chapter_id == 0x17) {
        cur_tick = (int32)(int16)BIOS_TICK_WORD;
        if (cur_tick != (int32)data_fd2_battle_bg_anim_last_advance_tick) {
            fd2_scroll_text_screen_up_by_lines(0);
            data_fd2_battle_bg_anim_last_advance_tick = (uint32)cur_tick;
        }
        src_stride = 0x138;
        dst_off = data_fd2_battle_compose_walk_step_sub_pixel_offset +
                  dst_buf + data_fd2_battle_compose_left_edge_clip_offset;
        base_off = data_fd2_battle_compose_parallax_scroll_y_rows * 0x1c8;
        bg_src = data_fd2_graphics_static_bg_buffer_ptr;
    } else {
        goto tile_pass;
    }

    fd2_blit_rectangle(base_off + dst_off, dst_stride, bg_src, src_stride,
                       0x138, 0xc0);

tile_pass:
    if (data_fd2_graphics_forced_tile_anim_frame == 0xffffffff) {
        cur_tick = (int32)(int16)BIOS_TICK_WORD;
        if (cur_tick - (int32)data_fd2_battle_tile_anim_last_advance_tick > 2 ||
            cur_tick < (int32)data_fd2_battle_tile_anim_last_advance_tick) {
            data_fd2_battle_tile_map_anim_frame_counter += 1;
            if (data_fd2_battle_tile_map_anim_frame_counter == 0x14) {
                data_fd2_battle_tile_map_anim_frame_counter = 0;
            }
            data_fd2_battle_tile_anim_last_advance_tick = (uint32)cur_tick;
        }
    } else {
        data_fd2_battle_tile_map_anim_frame_counter =
            data_fd2_graphics_forced_tile_anim_frame;
    }

    palette_remap =
        *(int32 *)(data_fd2_tile_anim_table_base + 6 +
                   (uint32)data_fd2_graphics_tile_anim_palette_phase_lookup
                       [data_fd2_battle_tile_map_anim_frame_counter] * 4) +
        data_fd2_tile_anim_table_base;

    for (row_iter = 0; (int32)row_iter < (int32)n_rows; row_iter++) {
        pDst_row = dst_buf + row_iter * dst_stride * 0x18;
        pTile_meta = ((row_iter + win_origin_y) * data_fd2_battle_map_width_tiles +
                      win_origin_x) * 4 + data_fd2_battle_tile_map_ptr + 4;

        for (col_iter = 0; col_iter < (int32)n_cols; col_iter++) {
            tile_id = (uint32)(*(uint16 *)pTile_meta & 0x3ff);
            tile_attr_flags =
                *(uint8 *)(tile_id * 4 + data_fd2_tile_attribute_flags_buffer_ptr);

            if ((tile_attr_flags & 8) != 0) {
                tile_id_adjust = (int32)data_fd2_graphics_bg_anim_flip_flag * 2;
                tile_id += (uint32)tile_id_adjust;
            } else if ((tile_attr_flags & 0x10) != 0) {
                tile_id_adjust =
                    data_fd2_graphics_chapter_ambient_palette_anim_idx / 2;
                tile_id += (uint32)tile_id_adjust;
            } else if ((tile_attr_flags & 4) != 0) {
                tile_id += data_fd2_graphics_bg_anim_flip_flag;
            }

            pTile_sprite =
                (uint32)*(int32 *)(data_fd2_battle_scene_tile_gfx_ptr + 6 +
                                   tile_id * 4) +
                data_fd2_battle_scene_tile_gfx_ptr;

            if (*(int8 *)(pTile_meta + 3) == -1) {
                fd2_tile_blit_24x24_passthrough(pTile_sprite, pDst_row,
                                                dst_stride);
            } else {
                fd2_tile_blit_24x24_remap(pTile_sprite, pDst_row, dst_stride,
                                          (uint32)palette_remap);
            }

            pDst_row += 0x18;
            pTile_meta += 4;
        }
    }
}
