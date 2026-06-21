/*
 * btl_aitg.c — Battle AI: target & tile scanning, AoE tile marking, movement planning
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <dos.h>

/* ----------------------------------------------------------------
 * fd2_tally_chars_with_zero_at_field @ 0x15DA2
 *
 * Sum weight for each char in char_idx_arr[0..len-1] whose
 * runtime_char byte at field_offset is zero.
 * ---------------------------------------------------------------- */
int fd2_tally_chars_with_zero_at_field(int len, uint32 char_idx_arr,
                                        int field_offset, int weight)
{
    int total;
    int i;
    uint8 char_idx;
    uint8 *pChar;

    total = 0;
    for (i = 0; i < len; i++) {
        char_idx = *(uint8 *)(char_idx_arr + i);
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + (uint32)char_idx * RUNTIME_CHAR_SIZE;
        if (pChar[field_offset] == 0) {
            total += weight;
        }
    }
    return total;
}

/* ----------------------------------------------------------------
 * fd2_find_tile_with_attribute_match @ 0x15DF3
 *
 * Find first event-type-1 tile with secondary_attr == target_tag.
 * Writes x,y to out_pos[0..1] as bytes. Returns 0 or -1.
 * ---------------------------------------------------------------- */
int fd2_find_tile_with_attribute_match(uint32 target_tag, uint32 out_pos)
{
    int y;
    int x;
    uint8 tile_buf[8];
    uint16 secondary_attr;
    uint8 attr_flags;

    for (y = 0; y < (int)data_fd2_battle_map_height_tiles; y++) {
        for (x = 0; x < (int)data_fd2_battle_map_width_tiles; x++) {
            fd2_read_tile_attribute_at_pos(x, y, (uint32)tile_buf);
            secondary_attr = *(uint16 *)(tile_buf + 2);
            attr_flags = tile_buf[4];
            if ((attr_flags & 0x60) == 0x20 &&
                (uint32)secondary_attr == target_tag) {
                *(uint8 *)out_pos = (uint8)x;
                *(uint8 *)(out_pos + 1) = (uint8)y;
                return 0;
            }
        }
    }
    return -1;
}

/* ----------------------------------------------------------------
 * fd2_collect_unmarked_tile_positions @ 0x14B16
 *
 * Collect all tiles whose overlay byte +7 is not 0xFF.
 * Write (x,y) byte-pairs to out_buf, return count.
 * ---------------------------------------------------------------- */
int fd2_collect_unmarked_tile_positions(uint32 out_buf)
{
    int count;
    int y;
    int x;
    uint8 *tile_ptr;

    count = 0;
    tile_ptr = (uint8 *)(data_fd2_battle_tile_map_ptr + 7);
    for (y = 0; y < (int)data_fd2_battle_map_height_tiles; y++) {
        for (x = 0; x < (int)data_fd2_battle_map_width_tiles; x++) {
            if (*tile_ptr != 0xFF) {
                *(uint8 *)out_buf = (uint8)x;
                *(uint8 *)(out_buf + 1) = (uint8)y;
                out_buf += 2;
                count++;
            }
            tile_ptr += 4;
        }
    }
    return count;
}

/* ----------------------------------------------------------------
 * fd2_mark_char_occupant_tiles_for_team @ 0x146D1
 *
 * Mark tile overlay byte +7 to 0xFF for all alive chars matching
 * team_selector, except exclude_idx.
 * team_selector==0 → mark team 0; !=0 → mark team !=0.
 * ---------------------------------------------------------------- */
void fd2_mark_char_occupant_tiles_for_team(uint32 exclude_idx,
                                            uint32 team_selector)
{
    int i;
    uint8 *pChar;

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr;
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        if ((uint32)i != exclude_idx &&
            (pChar[5] & CHARFLAG_DEAD) == 0) {
            if (team_selector == 0) {
                if (pChar[6] == TEAM_ENEMY) {
                    *(uint8 *)(data_fd2_battle_tile_map_ptr +
                        ((uint32)pChar[1] *
                         data_fd2_battle_map_width_tiles +
                         (uint32)pChar[0]) * 4 + 7) = 0xFF;
                }
            } else {
                if (pChar[6] != TEAM_ENEMY) {
                    *(uint8 *)(data_fd2_battle_tile_map_ptr +
                        ((uint32)pChar[1] *
                         data_fd2_battle_map_width_tiles +
                         (uint32)pChar[0]) * 4 + 7) = 0xFF;
                }
            }
        }
        pChar += RUNTIME_CHAR_SIZE;
    }
}

/* ----------------------------------------------------------------
 * fd2_scan_chars_within_manhattan_range @ 0x14742
 *
 * Find alive chars within manhattan distance of (center_x, center_y)
 * matching team_filter. Optionally write indices to out_buf.
 * Returns count.
 * ---------------------------------------------------------------- */
int fd2_scan_chars_within_manhattan_range(uint32 center_x, uint32 center_y,
                                          uint32 max_range,
                                          uint32 out_buf, int team_filter)
{
    int count;
    int i;
    uint8 *pChar;
    int dx;
    int dy;
    int manhattan;
    uint8 team;
    int matches;

    count = 0;
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        dx = abs((int)(uint32)pChar[0] - (int)center_x);
        dy = abs((int)(uint32)pChar[1] - (int)center_y);
        manhattan = dx + dy;
        if (pChar[5] & CHARFLAG_DEAD) continue;
        if (manhattan >= (int)max_range) continue;
        team = pChar[6];
        matches = 0;
        if (team_filter == 0 && team == 0) matches = 1;
        if (team_filter == 1 && team != 0) matches = 1;
        if (team_filter == 2 && team == 1) matches = 1;
        if (team_filter == 3 && team == 2) matches = 1;
        if (!matches) continue;
        if (out_buf != 0) {
            *(uint8 *)(out_buf + count) = (uint8)i;
        }
        count++;
    }
    return count;
}

/* ----------------------------------------------------------------
 * fd2_scan_chars_along_line_with_team_filter @ 0x149F8
 *
 * Walk a 4-cardinal line from start toward target for step_count
 * steps. Collect matching-team char indices into out_buf.
 * Returns hit_count.
 * ---------------------------------------------------------------- */
int fd2_scan_chars_along_line_with_team_filter(
    uint32 target_x, uint32 target_y, uint32 out_buf,
    uint32 start_x, uint32 start_y, uint32 step_count,
    uint32 team_filter)
{
    uint32 saved_cx;
    uint32 saved_cy;
    int step_dx;
    int step_dy;
    int hit_count;
    int step;
    int char_idx;
    uint8 *pChar;

    saved_cx = data_fd2_battle_cursor_world_x;
    saved_cy = data_fd2_battle_cursor_world_y;
    step_dx = 0;
    step_dy = 0;
    hit_count = 0;

    if (start_x == target_x) {
        if ((int)start_y <= (int)target_y) {
            step_dy = 1;
        } else {
            step_dy = -1;
        }
    } else if ((int)target_x < (int)start_x) {
        step_dx = -1;
    } else {
        step_dx = 1;
    }

    data_fd2_battle_cursor_world_x = start_x;
    data_fd2_battle_cursor_world_y = start_y;

    for (step = 0; step < (int)step_count; step++) {
        data_fd2_battle_cursor_world_x += step_dx;
        data_fd2_battle_cursor_world_y += step_dy;
        if ((int)data_fd2_battle_cursor_world_x >= 0 &&
            data_fd2_battle_cursor_world_x <
                data_fd2_battle_map_width_tiles &&
            (int)data_fd2_battle_cursor_world_y >= 0 &&
            data_fd2_battle_cursor_world_y <
                data_fd2_battle_map_height_tiles) {
            char_idx = fd2_find_char_at_cursor_pos();
            if (char_idx != -1) {
                pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                      + char_idx * RUNTIME_CHAR_SIZE;
                if ((team_filter == 0 && pChar[6] != 0) ||
                    (team_filter != 0 && pChar[6] == 0)) {
                    *(uint8 *)(out_buf + hit_count) =
                        (uint8)char_idx;
                    hit_count++;
                }
            }
        }
    }

    data_fd2_battle_cursor_world_x = saved_cx;
    data_fd2_battle_cursor_world_y = saved_cy;
    return hit_count;
}

/* ----------------------------------------------------------------
 * fd2_set_tile_overlay_bit_80 @ 0x146A7
 *
 * Set bit 0x80 on tile overlay byte +6 at (x, y).
 * ---------------------------------------------------------------- */
void fd2_set_tile_overlay_bit_80(uint32 x, uint32 y)
{
    *(uint8 *)(data_fd2_battle_tile_map_ptr +
        (y * data_fd2_battle_map_width_tiles + x) * 4 + 6) |= 0x80;
}

/* ----------------------------------------------------------------
 * fd2_mark_aoe_plus_pattern_at @ 0x14625
 *
 * Mark a "+" 5-tile pattern on tile overlay: center gets bit
 * 0x40, four neighbors get bit 0x80.
 * ---------------------------------------------------------------- */
void fd2_mark_aoe_plus_pattern_at(uint32 x, uint32 y)
{
    if (x != 0)
        fd2_set_tile_overlay_bit_80(x - 1, y);
    if (y != 0)
        fd2_set_tile_overlay_bit_80(x, y - 1);
    if ((int)x < (int)data_fd2_battle_map_width_tiles - 1)
        fd2_set_tile_overlay_bit_80(x + 1, y);
    if ((int)y < (int)data_fd2_battle_map_height_tiles - 1)
        fd2_set_tile_overlay_bit_80(x, y + 1);
    *(uint8 *)(data_fd2_battle_tile_map_ptr +
        (y * data_fd2_battle_map_width_tiles + x) * 4 + 6) |= 0x40;
}

/* ----------------------------------------------------------------
 * fd2_ai_pass_turn_with_heal @ 0x13FD4
 *
 * "Pass turn" / Rest action, shared by AI characters (enemy-turn
 * dispatcher fall-through) and the player's Wait command. If HP < max
 * and the char is not poisoned (status[0x25]) and not paralyzed
 * (status[0x26]), heal 20% of max HP (clamped to max) with a brief
 * glow animation + recovery SFX. Returns 1 if healed, 0 otherwise.
 * Note: FD2 has no "sleep" ailment; the 0x26 flag is paralysis (麻痹).
 * ---------------------------------------------------------------- */
int fd2_ai_pass_turn_with_heal(uint32 char_idx)
{
    uint8 *pChar;
    uint32 hp_current;
    uint32 hp_max;
    uint32 ws_buf;

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    hp_current = (uint32)*(uint16 *)(pChar + 0x40);
    hp_max = (uint32)*(uint16 *)(pChar + 0x42);

    if (hp_current == hp_max) return 0;
    if (pChar[0x25] != 0) return 0;
    if (pChar[0x26] != 0) return 0;

    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_to_char(char_idx);
    fd2_wait_n_bios_ticks(1);
    fd2_play_sfx_with_handle(
        data_fd2_audio_fdother_sfx_bank_buf_ptr, 4, 1);

    ws_buf = data_fd2_large_game_state_buffer_ptr + 0x8088;
    fd2_paint_char_sprite_at_world_with_mode(
        ws_buf, 0x1c8, char_idx, 2, 0xfd);
    fd2_blit_rectangle(
        0xa0504, 0x140, ws_buf, 0x1c8, 0x138, 0xc0);
    fd2_wait_n_bios_ticks(1);

    fd2_paint_char_sprite_at_world_with_mode(
        ws_buf, 0x1c8, char_idx, 0, 0);
    fd2_blit_rectangle(
        0xa0504, 0x140, ws_buf, 0x1c8, 0x138, 0xc0);
    fd2_wait_n_bios_ticks(1);

    hp_current = hp_current + hp_max / 5;
    if (hp_current > hp_max) hp_current = hp_max;
    *(uint16 *)(pChar + 0x40) = (uint16)hp_current;

    data_fd2_battle_anim_phase = 1;
    return 1;
}

/* ----------------------------------------------------------------
 * fd2_ai_walk_to_target_tile @ 0x14B78
 *
 * Execute AI walk toward target. Two-stage pathfind with fallback
 * to closest approachable tile. Returns 1 if walked, 0 if not.
 * ---------------------------------------------------------------- */
int fd2_ai_walk_to_target_tile(uint32 target_x, uint32 target_y,
                                uint32 char_idx, uint32 ctx)
{
    uint8 *pChar;
    uint32 src_x;
    uint32 src_y;
    uint32 range_rem;
    uint32 mvclass;
    uint8 *pCostTbl;
    uint32 pPathBuf;
    uint32 pTileBuf;
    int pf_result;
    int step_count;
    uint32 walk_x;
    uint32 walk_y;
    uint32 best_x;
    uint32 best_y;
    uint32 best_taxi;
    uint32 best_diag;
    int n_cand;
    int i;
    uint32 cx;
    uint32 cy;
    int dx;
    int dy;
    uint32 taxi;
    uint32 diag;
    uint8 stepd;
    int walked;

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    range_rem = (uint32)pChar[0x3B];
    src_x = (uint32)pChar[0];
    src_y = (uint32)pChar[1];
    mvclass = (uint32)pChar[0x20];
    if (fd2_check_char_status_immunity(char_idx) != 0)
        mvclass = 0x13;
    if (pChar[8] == 0x1C) mvclass = 1;

    pCostTbl = fd2_get_movement_cost_table_for_job(mvclass);
    pPathBuf = (uint32)malloc(0x20);
    pTileBuf = (uint32)malloc(0x800);
    fd2_paint_threat_overlay_for_team(ctx);

    pf_result = fd2_pathfind_to_destination(
        (uint32)pCostTbl, src_x, src_y, range_rem,
        pPathBuf, target_x, target_y, 0,
        data_fd2_battle_tile_map_ptr,
        data_fd2_tile_attribute_flags_buffer_ptr);

    if (pf_result == 0xFF) {
        fd2_obfuscate_battle_tile_map(
            data_fd2_battle_tile_map_ptr);
        step_count = fd2_pathfind_to_destination(
            (uint32)pCostTbl, src_x, src_y, 0x1C,
            pPathBuf, target_x, target_y, 1,
            data_fd2_battle_tile_map_ptr,
            data_fd2_tile_attribute_flags_buffer_ptr);
        if (step_count != 0xFF) {
            fd2_obfuscate_battle_tile_map(
                data_fd2_battle_tile_map_ptr);
            fd2_paint_threat_overlay_for_team(ctx);
            fd2_init_movement_range_floodfill(
                (uint32)pCostTbl, src_x, src_y,
                range_rem, data_fd2_battle_tile_map_ptr,
                data_fd2_tile_attribute_flags_buffer_ptr);
            best_x = target_x;
            best_y = target_y;
            walk_x = src_x;
            walk_y = src_y;
            for (i = 0; i < step_count; i++) {
                stepd = *(uint8 *)(pPathBuf + i);
                if (stepd == 0) walk_y++;
                else if (stepd == 1) walk_x--;
                else if (stepd == 2) walk_y--;
                else walk_x++;
                if (*(uint8 *)(data_fd2_battle_tile_map_ptr
                    + (data_fd2_battle_map_width_tiles
                       * walk_y + walk_x) * 4 + 7)
                    != 0xFF) {
                    best_x = walk_x;
                    best_y = walk_y;
                }
            }
            target_x = best_x;
            target_y = best_y;
        }
    }

    fd2_obfuscate_battle_tile_map(
        data_fd2_battle_tile_map_ptr);
    fd2_paint_threat_overlay_for_team(ctx);
    fd2_init_movement_range_floodfill(
        (uint32)pCostTbl, src_x, src_y, range_rem,
        data_fd2_battle_tile_map_ptr,
        data_fd2_tile_attribute_flags_buffer_ptr);
    fd2_mark_char_occupant_tiles_for_team(char_idx, ctx);
    n_cand = fd2_collect_unmarked_tile_positions(pTileBuf);

    best_x = target_x;
    best_y = target_y;
    best_taxi = 0xFF;
    best_diag = 0xFF;
    for (i = 0; i < n_cand; i++) {
        cx = (uint32)*(uint8 *)(pTileBuf + i * 2);
        cy = (uint32)*(uint8 *)(pTileBuf + i * 2 + 1);
        dx = (int)cx - (int)target_x;
        dy = (int)cy - (int)target_y;
        taxi = (uint32)(abs(dx) + abs(dy));
        diag = (uint32)abs(abs(dx) - abs(dy));
        if ((int)taxi < (int)best_taxi ||
            (taxi == best_taxi &&
             (int)diag < (int)best_diag)) {
            best_x = cx;
            best_y = cy;
            best_taxi = taxi;
            best_diag = diag;
        }
    }

    fd2_obfuscate_battle_tile_map(
        data_fd2_battle_tile_map_ptr);
    fd2_paint_threat_overlay_for_team(ctx);
    walked = fd2_pathfind_to_destination(
        (uint32)pCostTbl, src_x, src_y, range_rem,
        pPathBuf, best_x, best_y, 0,
        data_fd2_battle_tile_map_ptr,
        data_fd2_tile_attribute_flags_buffer_ptr);
    fd2_obfuscate_battle_tile_map(
        data_fd2_battle_tile_map_ptr);

    if (walked != 0) {
        fd2_walk_path_animation_loop(
            char_idx, pPathBuf, (uint32)walked);
        walked = 1;
    }

    free((void *)pPathBuf);
    free((void *)pTileBuf);
    return walked;
}

/* ----------------------------------------------------------------
 * fd2_compute_aoe_targets @ 0x14818
 *
 * Paint affected-tile overlay for spell/skill at (center_x,
 * center_y), gather char indices in range by team filter.
 * ---------------------------------------------------------------- */
int fd2_compute_aoe_targets(uint32 center_x, uint32 center_y,
                             uint32 out_buf, uint32 spell_range,
                             uint32 aoe_radius, uint32 team_filter)
{
    int count;
    uint8 *pTile;
    int row;
    int col;
    int dx;
    int dy;
    uint32 extent;
    int ci;
    uint8 *pChar;
    uint32 tile_off;
    uint8 *pCostTbl;
    int matches;

    count = 0;

    if ((int)spell_range < 0x10) {
        pCostTbl = fd2_get_movement_cost_table_for_job(0);
        fd2_init_movement_range_floodfill(
            (uint32)pCostTbl, center_x, center_y,
            spell_range, data_fd2_battle_tile_map_ptr,
            data_fd2_tile_attribute_flags_buffer_ptr);
        if (aoe_radius != 0) {
            pTile = (uint8 *)(data_fd2_battle_tile_map_ptr + 7);
            for (row = 0;
                 row < (int)data_fd2_battle_map_height_tiles;
                 row++) {
                for (col = 0;
                     col < (int)data_fd2_battle_map_width_tiles;
                     col++) {
                    dx = abs(col - (int)center_x);
                    dy = abs(row - (int)center_y);
                    if (dx + dy < (int)aoe_radius)
                        *pTile = 0xFF;
                    pTile += 4;
                }
            }
        }
    } else {
        extent = spell_range - 0x10;
        for (col = 0;
             col < (int)data_fd2_battle_map_width_tiles;
             col++) {
            if (abs(col - (int)center_x) <= (int)extent) {
                *(uint8 *)(data_fd2_battle_tile_map_ptr +
                    (center_y *
                     data_fd2_battle_map_width_tiles + col)
                    * 4 + 7) = 0;
            }
        }
        for (row = 0;
             row < (int)data_fd2_battle_map_height_tiles;
             row++) {
            if (abs(row - (int)center_y) <= (int)extent) {
                *(uint8 *)(data_fd2_battle_tile_map_ptr +
                    ((uint32)row *
                     data_fd2_battle_map_width_tiles +
                     center_x) * 4 + 7) = 0;
            }
        }
    }

    for (ci = 0;
         ci < (int)data_fd2_battle_party_member_count; ci++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + ci * RUNTIME_CHAR_SIZE;
        if (pChar[5] & 1) continue;
        tile_off = ((uint32)pChar[0] +
            (uint32)pChar[1] *
            data_fd2_battle_map_width_tiles) * 4 + 7;
        if (*(uint8 *)(data_fd2_battle_tile_map_ptr +
            tile_off) == 0xFF)
            continue;
        matches = 0;
        if (team_filter == 0 && pChar[6] == 0) matches = 1;
        if (team_filter == 1 && pChar[6] != 0) matches = 1;
        if (team_filter == 2 && pChar[6] == 1) matches = 1;
        if (team_filter == 3 && pChar[6] == 2) matches = 1;
        if (!matches) continue;
        if (out_buf != 0)
            *(uint8 *)(out_buf + count) = (uint8)ci;
        count++;
    }
    return count;
}

/* ----------------------------------------------------------------
 * fd2_ai_seek_optimal_position @ 0x14121
 *
 * AI "move toward best cell" fallback. Paints the team threat
 * overlay, pathfinds to the highest-scoring reachable tile for the
 * char's movement-cost class, then walks one step there.
 *
 * Movement-cost class = runtime_char[char_idx][0x20] (job-based),
 * with two overrides:
 *   - status-immunity set    -> class 0x13 (flying / unrestricted)
 *   - identity byte 8 == 0x1C -> class 1   (cheap movement)
 *
 * Returns 1 if it walked, 0 if the target is unreachable (pathfind
 * 0xFF) or the char is already at the optimum.
 *
 * Called by fd2_enemy_turn_action_dispatcher AI classes 1/3/5/11 as
 * the "scoring/attack failed, move instead" branch.
 * ---------------------------------------------------------------- */
int fd2_ai_seek_optimal_position(uint32 char_idx, uint32 ctx)
{
    uint8 *pChar;
    uint32 src_x;
    uint32 src_y;
    uint32 movement_class;
    uint8 *pMove_cost_table;
    uint8 dst_xy[4];
    uint32 dst_x;
    uint32 dst_y;
    int pathfind_result;
    int walk_result;
    int did_move;

    did_move = 0;
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    src_x = (uint32)pChar[0];
    src_y = (uint32)pChar[1];
    movement_class = (uint32)pChar[0x20];

    if (fd2_check_char_status_immunity(char_idx) != 0)
        movement_class = 0x13;
    if (pChar[8] == 0x1C)
        movement_class = 1;

    pMove_cost_table =
        fd2_get_movement_cost_table_for_job(movement_class);
    fd2_paint_threat_overlay_for_team(ctx);
    pathfind_result = fd2_pathfind_to_destination(
        (uint32)pMove_cost_table, src_x, src_y, 0x1C,
        (uint32)dst_xy, 0, 0, 2,
        data_fd2_battle_tile_map_ptr,
        data_fd2_tile_attribute_flags_buffer_ptr);
    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);

    if (pathfind_result == 0xFF) return 0;

    dst_x = (uint32)dst_xy[0];
    dst_y = (uint32)dst_xy[1];
    if (dst_x == src_x && dst_y == src_y) return did_move;

    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_to_char(char_idx);
    walk_result = fd2_ai_walk_to_target_tile(
        dst_x, dst_y, char_idx, ctx);
    if (walk_result != 0) did_move = 1;
    data_fd2_battle_anim_phase = 1;
    return did_move;
}

/* ----------------------------------------------------------------
 * fd2_ai_advance_to_nearest_team_target @ 0x13E9C
 *
 * Find nearest char matching team filter and walk toward it.
 * Returns 1 if walk succeeded, 0 otherwise.
 * ---------------------------------------------------------------- */
int fd2_ai_advance_to_nearest_team_target(uint32 char_idx,
                                           uint32 team)
{
    uint32 self_x;
    uint32 self_y;
    uint32 best_dist;
    uint32 best_x;
    uint32 best_y;
    int moved;
    int scan;
    uint8 *pScan;
    int taxi;
    int walk_result;

    best_dist = 0xFFFF;
    best_x = 0xFFFFFFFF;
    best_y = 0;
    moved = 0;
    self_x = (uint32)((uint8 *)data_fd2_battle_runtime_char_array_ptr
           + char_idx * RUNTIME_CHAR_SIZE)[0];
    self_y = (uint32)((uint8 *)data_fd2_battle_runtime_char_array_ptr
           + char_idx * RUNTIME_CHAR_SIZE)[1];

    for (scan = 0; scan < (int)data_fd2_battle_party_member_count;
         scan++) {
        pScan = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + scan * RUNTIME_CHAR_SIZE;
        if ((team == 0 && pScan[6] != 0) ||
            (team != 0 && pScan[6] == 0)) {
            taxi = abs((int)self_x - (int)(uint32)pScan[0])
                 + abs((int)self_y - (int)(uint32)pScan[1]);
            if (taxi < (int)best_dist) {
                best_x = (uint32)pScan[0];
                best_y = (uint32)pScan[1];
                best_dist = (uint32)taxi;
            }
        }
    }

    if (best_x == 0xFFFFFFFF) return 0;
    if (best_x == self_x && best_y == self_y) return moved;

    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_to_char(char_idx);
    walk_result = fd2_ai_walk_to_target_tile(
        best_x, best_y, char_idx, team);
    if (walk_result != 0) moved = 1;
    data_fd2_battle_anim_phase = 1;
    return moved;
}

/* ----------------------------------------------------------------
 * fd2_resolve_terrain_for_aoe_targets @ 0x2B5E1  (2 callers)
 *
 * Resolve the terrain-attribute byte that should back an AoE spell's
 * cinematic, given n_chars target chars in target_byte_array.
 *
 * Starts with the per-chapter override byte, then walks the target
 * array backwards (last non-immune wins): for each target, read its
 * tile-attribute byte (buf[+6] = attr_ptr[+2]) and, when the target
 * is not status-immune OR the running fallback is still 0, adopt that
 * tile byte. Immune targets keep a nonzero chapter override.
 * ---------------------------------------------------------------- */
char fd2_resolve_terrain_for_aoe_targets(int n_chars,
                                         uint8 *target_byte_array)
{
    uint8 fallback;
    uint8 *pChar;
    int immune;
    int i;
    uint8 tile_attr_buf[8];

    fallback = data_fd2_chapter_combat_cinematic_mode_per_chapter[
                   data_fd2_chapter_current_chapter_id];
    for (i = n_chars - 1; i >= 0; i--) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + (uint32)target_byte_array[i] * RUNTIME_CHAR_SIZE;
        fd2_read_tile_attribute_at_pos((uint32)pChar[0],
                                       (uint32)pChar[1],
                                       (uint32)tile_attr_buf);
        immune = fd2_check_char_status_immunity(
                     (uint32)target_byte_array[i]);
        if (immune == 0 || fallback == 0) {
            fallback = tile_attr_buf[6];
        }
    }
    return (char)fallback;
}