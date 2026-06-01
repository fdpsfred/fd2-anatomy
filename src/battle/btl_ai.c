/*
 * btl_ai.c — Battle AI: scoring, movement, attack dispatch,
 *             pattern marking, target scanning.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <dos.h>

/* ----------------------------------------------------------------
 * fd2_score_spell_candidate @ 0x15B77
 *
 * Per-candidate spell scorer for AI. Dispatch by spell_id range:
 *   <0xD  damage (kill=0x18, else=8, priority*=1.5)
 *   0xD-0x10 heal (HP deficit scoring, heal-boost doubles)
 *   0x11-0x13 status-effect (tally with spell_id+0x11)
 *   0x14 cure poison, 0x15 cure sleep, 0x16 silence
 *   0x1A/0x1B summon (tally with 0x25/0x26)
 * ---------------------------------------------------------------- */
int fd2_score_spell_candidate(uint32 spell_id, uint32 n_targets,
                               uint32 target_array_ptr)
{
    int total_score;
    uint8 *pSpell;
    uint16 spell_damage;
    int i;
    uint8 target_id;
    uint8 *pChar;
    uint16 hp_current;
    uint16 hp_max;
    int per_score;

    total_score = 0;
    pSpell = fd2_get_spell_effect_entry(spell_id);
    spell_damage = *(uint16 *)pSpell;

    if ((int)spell_id < 0xd) {
        for (i = 0; i < (int)n_targets; i++) {
            if ((int)spell_id >= 10) {
                if (fd2_check_char_status_immunity(
                        (uint32)*(uint8 *)(i + target_array_ptr))
                    != 0)
                    continue;
            }
            target_id = *(uint8 *)(i + target_array_ptr);
            pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                  + (uint32)target_id * RUNTIME_CHAR_SIZE;
            if (*(uint16 *)(pChar + 0x40) < spell_damage) {
                per_score = 0x18;
            } else {
                per_score = 8;
            }
            if (pChar[8] == 0) {
                per_score = (int)((double)per_score *
                    data_fd2_battle_ai_enemy_spell_score_multiplier_15);
            }
            total_score += per_score;
        }
    } else if ((int)spell_id < 0x11) {
        for (i = 0; i < (int)n_targets; i++) {
            target_id = *(uint8 *)(i + target_array_ptr);
            pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                  + (uint32)target_id * RUNTIME_CHAR_SIZE;
            hp_current = *(uint16 *)(pChar + 0x40);
            hp_max = *(uint16 *)(pChar + 0x42);
            if ((uint32)hp_current < (uint32)hp_max / 3) {
                per_score = 8;
            } else if ((uint32)hp_current <
                       (uint32)hp_max / 2) {
                per_score = 3;
            } else {
                per_score = 0;
            }
            if (pChar[0x34] & 0x01) {
                per_score <<= 1;
            }
            total_score += per_score;
        }
    } else if ((int)spell_id < 0x14) {
        total_score = fd2_tally_chars_with_zero_at_field(
            (int)n_targets, target_array_ptr,
            (int)spell_id + 0x11, 3);
    } else if (spell_id == 0x14) {
        for (i = 0; i < (int)n_targets; i++) {
            pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                  + (uint32)*(uint8 *)(i + target_array_ptr)
                    * RUNTIME_CHAR_SIZE;
            if (pChar[0x25] != 0) {
                total_score += 6;
            }
        }
    } else if (spell_id == 0x15) {
        for (i = 0; i < (int)n_targets; i++) {
            pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                  + (uint32)*(uint8 *)(i + target_array_ptr)
                    * RUNTIME_CHAR_SIZE;
            if (pChar[0x26] != 0) {
                total_score += 6;
            }
        }
    } else if (spell_id == 0x16) {
        for (i = 0; i < (int)n_targets; i++) {
            target_id = *(uint8 *)(i + target_array_ptr);
            pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                  + (uint32)target_id * RUNTIME_CHAR_SIZE;
            if (pChar[0x27] == 0 &&
                fd2_build_usable_spell_list(
                    (uint32)target_id, 0) != 0) {
                total_score += 6;
            }
        }
    } else if (spell_id == 0x1a) {
        total_score = fd2_tally_chars_with_zero_at_field(
            (int)n_targets, target_array_ptr, 0x25, 4);
    } else if (spell_id == 0x1b) {
        total_score = fd2_tally_chars_with_zero_at_field(
            (int)n_targets, target_array_ptr, 0x26, 4);
    }
    return total_score;
}

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
 * fd2_npc_turn_phase_team1 @ 0x1D80B
 *
 * AI turn loop for team 1 (NPC allies). Iterates chars, runs
 * enemy_turn_action_dispatcher, then post-action consequences
 * and chapter handlers.
 * ---------------------------------------------------------------- */
void fd2_npc_turn_phase_team1(void)
{
    int i;
    uint8 *pChar;

    data_fd2_battle_anim_phase = 0;
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        data_fd2_battle_anim_phase = 0;
        fd2_clear_keyboard_buffer();
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        data_fd2_battle_ai_post_action_consequence_idx = 0xFF;
        if (pChar[6] == 1 &&
            (pChar[5] & 0x81) == 0 &&
            pChar[0x26] == 0) {
            fd2_enemy_turn_action_dispatcher(i, 1);
        }
        if (data_fd2_battle_ai_post_action_consequence_idx != 0xFF) {
            data_fd2_battle_ai_post_action_consequence_table
                [data_fd2_battle_ai_post_action_consequence_idx](i);
        }
        data_fd2_chapter_post_action_handler_table
            [data_fd2_chapter_current_chapter_id](i);
        if (data_fd2_chapter_event_or_battle_end_code != 0) break;
    }
}

/* ----------------------------------------------------------------
 * fd2_enemy_turn_phase_team0 @ 0x1D8BA
 *
 * Two-pass enemy AI: pass 1 lets smart casters (spell/item score
 * >= 6) act first, pass 2 runs everyone else.
 * ---------------------------------------------------------------- */
void fd2_enemy_turn_phase_team0(void)
{
    int i;
    uint8 *pChar;

    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        data_fd2_battle_anim_phase = 0;
        fd2_clear_keyboard_buffer();
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        data_fd2_battle_ai_post_action_consequence_idx = 0xFF;
        if (pChar[6] == TEAM_ENEMY &&
            (pChar[5] & 0x81) == 0 &&
            pChar[0x26] == 0) {
            fd2_ai_score_offensive_spell(i, 0);
            fd2_ai_score_item_use(i, 0);
            if ((int)data_fd2_battle_ai_best_spell_score >= 6 ||
                (int)data_fd2_battle_ai_best_item_score >= 6) {
                fd2_enemy_turn_action_dispatcher(i, 0);
            }
        }
        if (data_fd2_battle_ai_post_action_consequence_idx != 0xFF) {
            data_fd2_battle_ai_post_action_consequence_table
                [data_fd2_battle_ai_post_action_consequence_idx](i);
        }
        data_fd2_chapter_post_action_handler_table
            [data_fd2_chapter_current_chapter_id](i);
        if (data_fd2_chapter_event_or_battle_end_code != 0) return;
    }

    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        fd2_clear_keyboard_buffer();
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        data_fd2_battle_ai_post_action_consequence_idx = 0xFF;
        if (pChar[6] == TEAM_ENEMY &&
            (pChar[5] & 0x81) == 0 &&
            pChar[0x26] == 0) {
            fd2_enemy_turn_action_dispatcher(i, 0);
        }
        if (data_fd2_battle_ai_post_action_consequence_idx != 0xFF) {
            data_fd2_battle_ai_post_action_consequence_table
                [data_fd2_battle_ai_post_action_consequence_idx](i);
        }
        data_fd2_chapter_post_action_handler_table
            [data_fd2_chapter_current_chapter_id](i);
        if (data_fd2_chapter_event_or_battle_end_code != 0) return;
    }
}

/* ----------------------------------------------------------------
 * fd2_score_item_candidate @ 0x15880
 *
 * Score an item for AI use: sum per-target priority based on
 * HP thresholds and effect type.
 * ---------------------------------------------------------------- */
int fd2_score_item_candidate(uint32 item_id, uint32 n_targets,
                              uint32 target_array_ptr)
{
    int total;
    uint8 *pItem;
    uint8 *pSpell;
    uint16 spell_or_damage;
    uint8 effect_code;
    int i;
    uint8 target_id;
    uint8 *pChar;
    uint16 hp;
    uint16 hp_max;
    int per_score;

    total = 0;
    pItem = fd2_get_item_effect_entry(item_id);
    spell_or_damage = *(uint16 *)(pItem + 0xE);
    effect_code = pItem[0xD];

    if (effect_code == 5 || effect_code == 0xD) {
        for (i = 0; i < (int)n_targets; i++) {
            target_id = *(uint8 *)(target_array_ptr + i);
            pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                  + (uint32)target_id * RUNTIME_CHAR_SIZE;
            hp = *(uint16 *)(pChar + 0x40);
            hp_max = *(uint16 *)(pChar + 0x42);
            if ((int)hp <= (int)(hp_max / 3)) {
                per_score = 8;
            } else if ((int)hp > (int)(hp_max / 2)) {
                per_score = 0;
            } else {
                per_score = 3;
            }
            if (pChar[0x34] & 0x80) {
                per_score = per_score * 3;
            }
            total += per_score;
        }
    } else if (effect_code == 0x14 || effect_code == 0x15 ||
               effect_code == 0x18) {
        pSpell = fd2_get_spell_effect_entry(spell_or_damage);
        if (effect_code != 0x18) {
            spell_or_damage = *(uint16 *)pSpell;
        }
        for (i = 0; i < (int)n_targets; i++) {
            target_id = *(uint8 *)(target_array_ptr + i);
            pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                  + (uint32)target_id * RUNTIME_CHAR_SIZE;
            hp = *(uint16 *)(pChar + 0x40);
            if ((int)spell_or_damage >= (int)hp) {
                per_score = 0x12;
            } else {
                per_score = 8;
            }
            total += per_score;
        }
    }
    return total;
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
 * AI "pass turn": if HP < max and no poison/sleep, heal 20% of
 * max HP with glow animation. Returns 1 if healed, 0 otherwise.
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
 * fd2_ai_score_offensive_spell @ 0x1598A
 *
 * Score every castable spell × every reachable tile. Writes best
 * to ai_best_spell_* globals. Returns 0.
 * ---------------------------------------------------------------- */
void fd2_ai_score_offensive_spell(uint32 caster_idx,
                                   uint32 ctx_flag)
{
    uint8 *pCaster;
    uint32 caster_x;
    uint32 caster_y;
    uint32 caster_mp;
    uint8 *pCostTbl;
    uint32 pTileBuf;
    uint8 spell_list[12];
    int n_spells;
    int spell_i;
    int tile_j;
    uint8 spell_id;
    uint8 *pSpell;
    int n_reachable;
    uint32 cx;
    uint32 cy;
    uint32 aoe_arg;
    int n_targets;
    uint8 tgt_buf[32];
    int score;
    uint32 best_dmg;

    data_fd2_battle_ai_best_spell_score = 0;
    pCostTbl = fd2_get_movement_cost_table_for_job(0);
    pCaster = (uint8 *)data_fd2_battle_runtime_char_array_ptr
            + caster_idx * RUNTIME_CHAR_SIZE;
    caster_x = (uint32)pCaster[0];
    caster_y = (uint32)pCaster[1];
    pTileBuf = (uint32)malloc(400);
    caster_mp = (uint32)*(uint16 *)(pCaster + 0x44);

    n_spells = fd2_build_usable_spell_list(
        caster_idx, (uint32)spell_list);
    if (n_spells == 0 || pCaster[0x27] != 0) return;

    best_dmg = 0;
    for (spell_i = 0; spell_i < n_spells; spell_i++) {
        spell_id = spell_list[spell_i];
        pSpell = fd2_get_spell_effect_entry((uint32)spell_id);
        if ((uint32)pSpell[5] > caster_mp) continue;

        fd2_init_movement_range_floodfill(
            (uint32)pCostTbl, caster_x, caster_y,
            (uint32)pSpell[3],
            data_fd2_battle_tile_map_ptr,
            data_fd2_tile_attribute_flags_buffer_ptr);
        n_reachable = fd2_collect_unmarked_tile_positions(
            pTileBuf);
        fd2_obfuscate_battle_tile_map(
            data_fd2_battle_tile_map_ptr);

        for (tile_j = 0; tile_j < n_reachable; tile_j++) {
            cx = (uint32)*(uint8 *)(pTileBuf + tile_j * 2);
            cy = (uint32)*(uint8 *)(pTileBuf + tile_j * 2 + 1);

            if (ctx_flag == 0) {
                aoe_arg = (pSpell[6] == 0) ? 1 : 0;
            } else {
                aoe_arg = (uint32)pSpell[6];
            }

            n_targets = fd2_compute_aoe_targets(
                cx, cy, (uint32)tgt_buf,
                (uint32)pSpell[4], 0, aoe_arg);
            fd2_obfuscate_battle_tile_map(
                data_fd2_battle_tile_map_ptr);

            if (n_targets != 0) {
                score = fd2_score_spell_candidate(
                    (uint32)spell_id, (uint32)n_targets,
                    (uint32)tgt_buf);
                if (score >
                    (int)data_fd2_battle_ai_best_spell_score
                    || (score ==
                    (int)data_fd2_battle_ai_best_spell_score
                    && (int)(uint32)*(uint16 *)pSpell >
                       (int)best_dmg)) {
                    data_fd2_battle_ai_best_spell_score =
                        (uint32)score;
                    data_fd2_battle_ai_best_spell_target_x =
                        cx;
                    data_fd2_battle_ai_best_spell_target_y =
                        cy;
                    data_fd2_battle_ai_best_spell_id =
                        (uint32)spell_id;
                    best_dmg =
                        (uint32)*(uint16 *)pSpell;
                }
            }
        }
    }

    free((void *)pTileBuf);
}

/* ----------------------------------------------------------------
 * fd2_ai_score_item_use @ 0x1567E
 *
 * Score every inventory item × every reachable tile. Writes best
 * candidate to ai_best_item_* globals. Returns 0.
 * ---------------------------------------------------------------- */
void fd2_ai_score_item_use(uint32 caster_idx, uint32 ctx_flag)
{
    uint8 *pCaster;
    uint32 caster_x;
    uint32 caster_y;
    uint32 pTileBuf;
    int n_slots;
    int slot_i;
    uint32 item_id;
    uint8 *pItem;
    uint8 range_class;
    uint8 range_for_aoe;
    int n_reachable;
    int tile_j;
    uint32 cx;
    uint32 cy;
    uint32 aoe_arg;
    int n_targets;
    uint8 tgt_buf[32];
    int score;

    data_fd2_battle_ai_best_item_score = 0;
    fd2_get_movement_cost_table_for_job(0);

    pCaster = (uint8 *)data_fd2_battle_runtime_char_array_ptr
            + caster_idx * RUNTIME_CHAR_SIZE;
    caster_x = (uint32)pCaster[0];
    caster_y = (uint32)pCaster[1];
    pTileBuf = (uint32)malloc(400);
    n_slots = fd2_count_usable_inventory_slots(caster_idx);
    if (n_slots == 0) return;

    for (slot_i = 0; slot_i < n_slots; slot_i++) {
        item_id = (uint32)pCaster[0xB + slot_i * 2];
        pItem = fd2_get_item_effect_entry((int)item_id);
        range_class = pItem[0x10];
        range_for_aoe = range_class;
        if (range_class > 0x0F) range_for_aoe = 1;
        if (pItem[0xD] == 0) continue;

        fd2_compute_aoe_targets(
            caster_x, caster_y, 0,
            (uint32)range_for_aoe,
            (uint32)(range_class > 0x0F), 0);
        n_reachable = fd2_collect_unmarked_tile_positions(
            pTileBuf);
        fd2_obfuscate_battle_tile_map(
            data_fd2_battle_tile_map_ptr);

        for (tile_j = 0; tile_j < n_reachable; tile_j++) {
            cx = (uint32)*(uint8 *)(pTileBuf + tile_j * 2);
            cy = (uint32)*(uint8 *)(pTileBuf + tile_j * 2 + 1);

            if (ctx_flag == 0) {
                aoe_arg = (pItem[0x11] == 0) ? 1 : 0;
            } else {
                aoe_arg = (uint32)pItem[0x11];
            }

            if (pItem[0x10] < 0x10) {
                n_targets = fd2_compute_aoe_targets(
                    cx, cy, (uint32)tgt_buf,
                    (uint32)pItem[0x12], 0, aoe_arg);
            } else {
                n_targets =
                    fd2_scan_chars_along_line_with_team_filter(
                        cx, cy, (uint32)tgt_buf,
                        caster_x, caster_y,
                        (uint32)pItem[0x10] - 0x10, 0);
            }
            fd2_obfuscate_battle_tile_map(
                data_fd2_battle_tile_map_ptr);

            if (n_targets != 0) {
                score = fd2_score_item_candidate(
                    item_id, (uint32)n_targets,
                    (uint32)tgt_buf);
                if (score >
                    (int)data_fd2_battle_ai_best_item_score) {
                    data_fd2_battle_ai_best_item_score =
                        (uint32)score;
                    data_fd2_battle_ai_best_item_target_x = cx;
                    data_fd2_battle_ai_best_item_target_y = cy;
                    data_fd2_battle_ai_best_item_slot =
                        (uint32)slot_i;
                }
            }
        }
    }

    free((void *)pTileBuf);
}

/* ----------------------------------------------------------------
 * fd2_execute_ai_physical_attack @ 0x1548E
 *
 * Execute AI physical attack with animation + retaliation.
 * Always returns 1.
 * ---------------------------------------------------------------- */
int fd2_execute_ai_physical_attack(uint32 caster_idx,
                                    uint32 ctx_flag)
{
    uint32 target_idx;
    uint32 desc;
    int can_counter;
    int first_hit;
    int n_drops;
    uint8 drops_buf[12];

    target_idx = data_fd2_battle_ai_best_physical_target_idx;

    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_to_char(caster_idx);
    fd2_ai_walk_to_target_tile(
        data_fd2_battle_ai_best_physical_target_x,
        data_fd2_battle_ai_best_physical_target_y,
        caster_idx, ctx_flag);
    data_fd2_battle_anim_phase = 1;
    fd2_pan_cursor_to_char(target_idx);
    fd2_face_char_toward_target(caster_idx, target_idx);

    if (data_fd2_ui_game_speed_flag == 0) {
        fd2_play_full_combat_cinematic(caster_idx, target_idx);
    } else {
        data_fd2_battle_anim_phase = 0;
        fd2_composite_battle_frame(0);
        data_fd2_battle_anim_phase = 1;

        can_counter = fd2_check_can_counter_attack(
            caster_idx, target_idx);
        if (can_counter == 1)
            fd2_face_char_toward_target(target_idx, caster_idx);

        desc = fd2_animate_combat_speech_bubbles(
            caster_idx, target_idx);
        fd2_render_combatant_hp_bar_proportional(
            0xa0000, 0x140, target_idx, desc);

        if (fd2_check_can_counter_attack(
                caster_idx, target_idx) == 1)
            fd2_render_combatant_hp_bar_proportional(
                0xa0000, 0x140, caster_idx, desc + 8);

        fd2_clear_all_chars_facing();
        first_hit = fd2_animate_combat_hit_with_hp_drain(
            caster_idx, target_idx, desc);

        if (first_hit != 0 &&
            fd2_check_can_counter_attack(
                caster_idx, target_idx) == 1) {
            fd2_face_char_toward_target(
                caster_idx, target_idx);
            fd2_face_char_toward_target(
                target_idx, caster_idx);
            fd2_render_combat_combatant_panels(
                desc, caster_idx, target_idx);
            fd2_animate_combat_hit_with_hp_drain(
                target_idx, caster_idx, desc + 8);
        }
    }

    fd2_clear_all_chars_facing();
    n_drops = fd2_collect_pending_death_drops(
        (uint32)drops_buf);
    fd2_composite_battle_frame(0);
    fd2_play_death_animation_and_mark_dead();
    fd2_process_battle_drop_entries(
        target_idx, (uint32)n_drops, (uint32)drops_buf);
    fd2_composite_battle_frame(0);
    fd2_process_xp_and_level_up_for_char(target_idx);
    return 1;
}

/* ----------------------------------------------------------------
 * fd2_execute_ai_offensive_spell @ 0x15311
 *
 * Execute AI offensive spell. Gate: score < 6 → return 0.
 * Dispatch via spell handler table or basic cast sequence.
 * ---------------------------------------------------------------- */
int fd2_execute_ai_offensive_spell(uint32 caster_idx,
                                    uint32 ctx_flag)
{
    uint8 *pSpell;
    uint32 aoe_flag;
    int n_targets;
    uint8 target_buf[32];
    uint8 drops_buf[12];
    int n_drops;

    pSpell = fd2_get_spell_effect_entry(
        data_fd2_battle_ai_best_spell_id);
    if (ctx_flag == 0) {
        aoe_flag = (pSpell[6] == 0) ? 1 : 0;
    } else {
        aoe_flag = (uint32)pSpell[6];
    }

    if ((int)data_fd2_battle_ai_best_spell_score < 6)
        return 0;

    fd2_pan_cursor_to_char(caster_idx);
    n_targets = fd2_compute_aoe_targets(
        data_fd2_battle_ai_best_spell_target_x,
        data_fd2_battle_ai_best_spell_target_y,
        (uint32)target_buf, (uint32)pSpell[4], 0, aoe_flag);

    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
    delay(200);
    data_fd2_battle_anim_phase = (uint32)pSpell[4] + 2;
    fd2_pan_cursor_to_tile_animated(
        (int)data_fd2_battle_ai_best_spell_target_x,
        (int)data_fd2_battle_ai_best_spell_target_y);
    data_fd2_battle_anim_phase = 0;
    fd2_composite_battle_frame(0);

    if ((int)data_fd2_battle_ai_best_spell_id < 10 &&
        data_fd2_ui_game_speed_flag == 0) {
        fd2_play_spell_cast_sequence(
            caster_idx,
            data_fd2_battle_ai_best_spell_id,
            (uint32)n_targets, (uint32)target_buf);
    } else {
        fd2_load_status_effect_sfx();
        data_fd2_battle_spell_handler_table
            [data_fd2_battle_ai_best_spell_id](
                caster_idx, (uint32)n_targets, target_buf);
        fd2_play_and_free_status_effect_sfx();
    }

    n_drops = fd2_collect_dead_char_drops((uint32)drops_buf);
    fd2_composite_battle_frame(0);
    fd2_play_death_animation_and_mark_dead();
    fd2_process_battle_drop_entries(
        data_fd2_battle_ai_best_physical_target_idx,
        (uint32)n_drops, (uint32)drops_buf);
    fd2_composite_battle_frame(0);
    data_fd2_battle_pending_xp_credit = 0;
    data_fd2_battle_anim_phase = 0;
    return 1;
}

/* ----------------------------------------------------------------
 * fd2_execute_ai_item_use @ 0x15055
 *
 * Execute AI item use. Reads ai_best_item_* globals, applies
 * item effect with animation. Returns 0.
 * ---------------------------------------------------------------- */
void fd2_execute_ai_item_use(uint32 caster_idx, uint32 ctx_flag)
{
    uint8 *pCaster;
    uint8 item_id;
    uint8 *pItem;
    uint32 tmp_u;
    uint8 range_class;
    int n_targets;
    uint8 target_idxs[32];
    int fade_i;
    int burst_i;

    pCaster = (uint8 *)data_fd2_battle_runtime_char_array_ptr
            + caster_idx * RUNTIME_CHAR_SIZE;
    item_id = fd2_get_inventory_slot_item_id(
        caster_idx, data_fd2_battle_ai_best_item_slot);
    data_fd2_ui_menu_cursor_idx = (uint32)item_id;
    pItem = fd2_get_item_effect_entry(
        (int)data_fd2_ui_menu_cursor_idx);

    if (ctx_flag == 0) {
        tmp_u = (pItem[0x11] == 0) ? 1 : 0;
    } else {
        tmp_u = (uint32)pItem[0x11];
    }

    fd2_pan_cursor_to_char(caster_idx);
    range_class = pItem[0x10];

    if ((uint32)range_class < 0x10) {
        n_targets = fd2_compute_aoe_targets(
            data_fd2_battle_ai_best_item_target_x,
            data_fd2_battle_ai_best_item_target_y,
            (uint32)target_idxs,
            (uint32)pItem[0x12], 0, tmp_u);
    } else {
        n_targets = fd2_scan_chars_along_line_with_team_filter(
            data_fd2_battle_ai_best_item_target_x,
            data_fd2_battle_ai_best_item_target_y,
            (uint32)target_idxs,
            (uint32)pCaster[0], (uint32)pCaster[1],
            (uint32)range_class - 0x10, 0);
    }

    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
    delay(200);
    data_fd2_battle_anim_phase = (uint32)pItem[0x12] + 2;

    if ((uint32)range_class < 0x10) {
        fd2_pan_cursor_to_tile_animated(
            (int)data_fd2_battle_ai_best_item_target_x,
            (int)data_fd2_battle_ai_best_item_target_y);
    } else {
        fd2_face_char_toward_target(
            caster_idx, (uint32)target_idxs[0]);
        fd2_composite_battle_frame(1);
        fd2_wait_n_bios_ticks(1);
        fd2_wait_n_bios_ticks(2);
        fd2_play_figani_char_intro_animation(caster_idx);
        fd2_play_rising_pre_cast_effect(
            caster_idx, 0x50, -4);
        delay(200);
        for (fade_i = 0x40; fade_i >= 0; fade_i--) {
            fd2_set_vga_palette_range_with_add(
                0, 0xFF, (uint32)fade_i);
            delay(4);
        }
        data_fd2_battle_anim_phase = 6;
        data_fd2_battle_ai_best_item_target_x =
            data_fd2_battle_cursor_world_x +
            (uint32)(uint8)(range_class - 0x10) *
            (data_fd2_battle_ai_best_item_target_x -
             data_fd2_battle_cursor_world_x);
        if ((int)data_fd2_battle_ai_best_item_target_x >=
            (int)data_fd2_battle_map_width_tiles) {
            data_fd2_battle_ai_best_item_target_x =
                data_fd2_battle_map_width_tiles - 1;
        } else if ((int)data_fd2_battle_ai_best_item_target_x
                   < 0) {
            data_fd2_battle_ai_best_item_target_x = 0;
        }
        data_fd2_battle_ai_best_item_target_y =
            data_fd2_battle_cursor_world_y +
            (data_fd2_battle_ai_best_item_target_y -
             data_fd2_battle_cursor_world_y) *
            (uint32)(uint8)(range_class - 0x10);
        if ((int)data_fd2_battle_ai_best_item_target_y >=
            (int)data_fd2_battle_map_height_tiles) {
            data_fd2_battle_ai_best_item_target_y =
                data_fd2_battle_map_height_tiles - 1;
        } else if ((int)data_fd2_battle_ai_best_item_target_y
                   < 0) {
            data_fd2_battle_ai_best_item_target_y = 0;
        }
        data_fd2_battle_tile_map_anim_frame_counter = 0;
        fd2_pan_cursor_to_tile_animated(
            (int)data_fd2_battle_ai_best_item_target_x,
            (int)data_fd2_battle_ai_best_item_target_y);
        data_fd2_battle_anim_phase = 0;
        for (burst_i = 1; burst_i < 9; burst_i++) {
            data_fd2_battle_tile_map_anim_frame_counter =
                (uint32)burst_i;
            fd2_composite_battle_frame(1);
            fd2_wait_n_bios_ticks(1);
        }
        fd2_obfuscate_battle_tile_map(
            data_fd2_battle_tile_map_ptr);
        fd2_wait_n_bios_ticks(2);
        fd2_pan_cursor_to_char((uint32)target_idxs[0]);
    }

    fd2_apply_use_effect_dispatch(
        caster_idx, data_fd2_battle_ai_best_item_slot,
        (uint32)n_targets, (uint32)target_idxs);
    fd2_clear_all_chars_facing();
    data_fd2_battle_pending_xp_credit = 0;
}

/* ----------------------------------------------------------------
 * fd2_attack_action_dispatch @ 0x14EF0
 *
 * Score phys/spell/item, pick best, execute winner. Returns 1 if
 * action executed, 0 if all scores < 6.
 * ---------------------------------------------------------------- */
int fd2_attack_action_dispatch(uint32 caster_idx, uint32 ctx_flag)
{
    uint8 *pCaster;
    uint8 *pTarget;
    uint8 *pSpell;
    uint32 tie_break;
    int shared_raw;

    fd2_ai_score_physical_attack(caster_idx, ctx_flag);
    fd2_ai_score_offensive_spell(caster_idx, ctx_flag);
    fd2_ai_score_item_use(caster_idx, ctx_flag);

    pCaster = (uint8 *)data_fd2_battle_runtime_char_array_ptr
            + caster_idx * RUNTIME_CHAR_SIZE;
    pTarget = (uint8 *)data_fd2_battle_runtime_char_array_ptr
            + data_fd2_battle_ai_best_physical_target_idx
              * RUNTIME_CHAR_SIZE;
    tie_break = (uint32)(pCaster[0x34] & 0x40);
    shared_raw = (int)(uint32)*(uint16 *)(pCaster + 0x48)
               - (int)(uint32)*(uint16 *)(pTarget + 0x4A);

    if ((int)data_fd2_battle_ai_best_physical_score < 6 &&
        (int)data_fd2_battle_ai_best_spell_score < 6 &&
        (int)data_fd2_battle_ai_best_item_score < 6)
        return 0;

    if ((int)data_fd2_battle_ai_best_physical_score >
            (int)data_fd2_battle_ai_best_spell_score &&
        (int)data_fd2_battle_ai_best_physical_score >
            (int)data_fd2_battle_ai_best_item_score) {
        fd2_execute_ai_physical_attack(caster_idx, ctx_flag);
    } else if (data_fd2_battle_ai_best_physical_score ==
               data_fd2_battle_ai_best_spell_score &&
               (int)data_fd2_battle_ai_best_physical_score >
               (int)data_fd2_battle_ai_best_item_score) {
        pSpell = fd2_get_spell_effect_entry(
            data_fd2_battle_ai_best_spell_id);
        if ((int)data_fd2_battle_ai_best_spell_id < 0xB) {
            if (shared_raw <=
                (int)(uint32)*(uint16 *)pSpell) {
                fd2_execute_ai_offensive_spell(
                    caster_idx, ctx_flag);
            } else {
                fd2_execute_ai_physical_attack(
                    caster_idx, ctx_flag);
            }
        } else if (tie_break == 0) {
            fd2_execute_ai_offensive_spell(
                caster_idx, ctx_flag);
        } else {
            fd2_execute_ai_physical_attack(
                caster_idx, ctx_flag);
        }
    } else if (data_fd2_battle_ai_best_physical_score ==
               data_fd2_battle_ai_best_item_score &&
               (int)data_fd2_battle_ai_best_physical_score >
               (int)data_fd2_battle_ai_best_spell_score) {
        if (tie_break != 0) {
            fd2_execute_ai_physical_attack(
                caster_idx, ctx_flag);
        } else {
            fd2_execute_ai_item_use(caster_idx, ctx_flag);
        }
    } else if ((int)data_fd2_battle_ai_best_spell_score >
               (int)data_fd2_battle_ai_best_physical_score &&
               (int)data_fd2_battle_ai_best_spell_score >=
               (int)data_fd2_battle_ai_best_item_score) {
        fd2_execute_ai_offensive_spell(
            caster_idx, ctx_flag);
    } else if ((int)data_fd2_battle_ai_best_item_score >
               (int)data_fd2_battle_ai_best_physical_score &&
               (int)data_fd2_battle_ai_best_item_score >
               (int)data_fd2_battle_ai_best_spell_score) {
        fd2_execute_ai_item_use(caster_idx, ctx_flag);
    }

    data_fd2_battle_anim_phase = 0;
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
 * fd2_ai_score_physical_attack @ 0x14237
 *
 * Score every reachable tile for physical attack. Writes best
 * candidate to ai_best_physical_* globals. Returns 0.
 * ---------------------------------------------------------------- */
int fd2_ai_score_physical_attack(uint32 caster_idx, uint32 ctx_flag)
{
    uint8 *pCaster;
    uint8 tile_buf[8];
    uint32 attacker_AP;
    uint32 attacker_DP;
    uint32 effective_AP;
    uint32 effective_DP;
    uint32 weapon_aoe_x;
    uint32 weapon_aoe_y;
    uint32 mp_remaining;
    uint32 movement_class;
    uint8 *pMove_cost_table;
    uint32 pAlloc_a;
    uint32 pTile_pos_buf;
    uint32 pTarget_id_buf;
    uint32 use_smaller_aoe;
    uint32 n_reachable;
    uint32 n_aoe;
    uint32 best_tiebreak;
    int tile_i;
    int tgt_i;
    uint32 cand_x;
    uint32 cand_y;
    uint32 target_idx;
    uint8 *pTarget;
    uint32 target_AP;
    uint32 target_DP;
    int raw_dmg;
    uint32 score_class;
    uint32 slot;
    uint8 item_id;
    uint8 *pItem;
    uint8 tile_id;

    best_tiebreak = 0;
    use_smaller_aoe = 0;
    pCaster = (uint8 *)data_fd2_battle_runtime_char_array_ptr
            + caster_idx * RUNTIME_CHAR_SIZE;
    attacker_AP = (uint32)*(uint16 *)(pCaster + 0x48);
    attacker_DP = (uint32)*(uint16 *)(pCaster + 0x4A);
    data_fd2_battle_ai_best_physical_score = 0;

    slot = fd2_find_equipped_item_by_kind(caster_idx, 0);
    if (slot == 0xFFFFFFFF) return 0;

    item_id = fd2_get_inventory_slot_item_id(caster_idx, slot);
    pItem = fd2_get_item_effect_entry((int)item_id);
    weapon_aoe_x = (uint32)pItem[0xB];
    weapon_aoe_y = (uint32)pItem[0xC];
    mp_remaining = (uint32)pCaster[0x3B];

    if (fd2_check_char_status_immunity(caster_idx) != 0) {
        movement_class = 0x13;
    } else {
        movement_class = (uint32)pCaster[0x20];
    }

    pMove_cost_table =
        fd2_get_movement_cost_table_for_job(movement_class);
    pAlloc_a = (uint32)malloc(0x20);
    pTile_pos_buf = (uint32)malloc(0x800);
    if (ctx_flag == 0) use_smaller_aoe = 1;

    fd2_paint_threat_overlay_for_team(ctx_flag);
    fd2_init_movement_range_floodfill(
        (uint32)pMove_cost_table,
        (uint32)pCaster[0], (uint32)pCaster[1],
        mp_remaining, data_fd2_battle_tile_map_ptr,
        data_fd2_tile_attribute_flags_buffer_ptr);
    fd2_mark_char_occupant_tiles_for_team(caster_idx, ctx_flag);
    n_reachable = (uint32)fd2_collect_unmarked_tile_positions(
        pTile_pos_buf);
    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
    pTarget_id_buf = (uint32)malloc(100);

    for (tile_i = 0; tile_i < (int)n_reachable; tile_i++) {
        cand_x = (uint32)*(uint8 *)(pTile_pos_buf + tile_i * 2);
        cand_y = (uint32)*(uint8 *)(pTile_pos_buf + tile_i * 2 + 1);
        effective_AP = attacker_AP;
        effective_DP = attacker_DP;

        if (fd2_check_char_status_immunity(caster_idx) != 0) {
            fd2_read_tile_attribute_at_pos(
                cand_x, cand_y, (uint32)tile_buf);
            tile_id = tile_buf[5];
            effective_AP = attacker_AP
                + (int)data_fd2_battle_tile_attr_mv_modifier_table
                    [tile_id] * (int)attacker_AP / 100;
            effective_DP = attacker_DP
                + (int)data_fd2_battle_tile_attr_def_modifier_table
                    [tile_id] * (int)attacker_DP / 100;
        }

        n_aoe = fd2_compute_aoe_targets(
            cand_x, cand_y, pTarget_id_buf,
            weapon_aoe_y, weapon_aoe_x, use_smaller_aoe);
        fd2_obfuscate_battle_tile_map(
            data_fd2_battle_tile_map_ptr);
        if (n_aoe == 0) continue;

        for (tgt_i = 0; tgt_i < (int)n_aoe; tgt_i++) {
            target_idx =
                (uint32)*(uint8 *)(pTarget_id_buf + tgt_i);
            pTarget =
                (uint8 *)data_fd2_battle_runtime_char_array_ptr
                + target_idx * RUNTIME_CHAR_SIZE;
            target_AP = (uint32)*(uint16 *)(pTarget + 0x48);
            target_DP = (uint32)*(uint16 *)(pTarget + 0x4A);

            if (fd2_check_char_status_immunity(target_idx)
                != 0) {
                fd2_read_tile_attribute_at_pos(
                    (uint32)pTarget[0], (uint32)pTarget[1],
                    (uint32)tile_buf);
                tile_id = tile_buf[5];
                target_AP = target_AP
                    + (int)data_fd2_battle_tile_attr_mv_modifier_table
                        [tile_id] * (int)target_AP / 100;
                target_DP = target_DP
                    + (int)data_fd2_battle_tile_attr_def_modifier_table
                        [tile_id] * (int)target_DP / 100;
            }

            raw_dmg = (int)effective_AP - (int)target_DP;
            if (raw_dmg <= 2) {
                score_class = 0;
            } else {
                score_class = 8;
            }
            if (raw_dmg >
                (int)(uint32)*(uint16 *)(pTarget + 0x40)) {
                raw_dmg = raw_dmg * 2;
                score_class = 0x12;
            }
            if (fd2_check_can_default_attack_target(
                    target_idx, cand_x, cand_y) == 1) {
                raw_dmg += (int)effective_DP - (int)target_AP;
            }
            if (pTarget[8] == 0) {
                raw_dmg = raw_dmg * 3 / 2;
            }
            if ((int)score_class >
                    (int)data_fd2_battle_ai_best_physical_score
                || (score_class ==
                    data_fd2_battle_ai_best_physical_score
                    && raw_dmg > (int)best_tiebreak)) {
                best_tiebreak = (uint32)raw_dmg;
                data_fd2_battle_ai_best_physical_target_x =
                    cand_x;
                data_fd2_battle_ai_best_physical_target_y =
                    cand_y;
                data_fd2_battle_ai_best_physical_target_idx =
                    target_idx;
                data_fd2_battle_ai_best_physical_score =
                    score_class;
            }
        }
    }

    free((void *)pTarget_id_buf);
    free((void *)pAlloc_a);
    free((void *)pTile_pos_buf);
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_ai_seek_optimal_position @ 0x14121
 *
 * Pathfind to best cell for char's job movement class, then walk.
 * Returns 1 if walked, 0 if unreachable or already at optimum.
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
 * fd2_enemy_turn_action_dispatcher @ 0x13A9F
 *
 * Master per-character AI turn handler. Selects action by AI
 * behavior class (runtime_char[+0x34] low nibble, 12 cases).
 * Shared postlude: tile_event_post_action, mark_acted,
 * clear_facing, composite.
 * ---------------------------------------------------------------- */
void fd2_enemy_turn_action_dispatcher(uint32 char_idx, uint32 team)
{
    uint8 *pChar;
    int ai_class;
    uint32 ai_aux_byte;
    uint32 target_pos;
    uint32 ai_target_id;
    int result;
    int found;
    uint8 *pTarget;
    uint8 pickup_xy[4];
    uint8 *pTileEvt;
    uint8 pickup_kind;
    uint16 pickup_param;

    result = 0;
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    if (pChar[5] & 0x05) return;

    ai_class = pChar[0x34] & 0x0F;
    ai_aux_byte = (uint32)pChar[0x35];
    target_pos = (uint32)pChar[0x36];
    ai_target_id = (uint32)pChar[0x3d];

    if (ai_class == 0) goto case_0_attack;

    if (ai_class == 1) {
        if (fd2_attack_action_dispatch(char_idx, team) != 0)
            goto postlude;
        result = fd2_ai_seek_optimal_position(char_idx, team);
        goto check_pass;
    }

    if (ai_class == 2) {
        if (fd2_attack_action_dispatch(char_idx, team) != 0)
            goto postlude;
        result = fd2_ai_score_physical_attack(char_idx, team);
        goto check_pass;
    }

    if (ai_class == 3) {
        if (fd2_attack_action_dispatch(char_idx, team) != 0)
            goto postlude;
        found = fd2_find_char_by_id_or_template(ai_aux_byte);
        if (found == -1) goto seek_advance;
        pTarget = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                + (uint32)found * RUNTIME_CHAR_SIZE;
        fd2_pan_cursor_to_char(char_idx);
        result = fd2_ai_walk_to_target_tile(
            (uint32)pTarget[0], (uint32)pTarget[1],
            char_idx, team);
        if (result == 0)
            fd2_ai_pass_turn_with_heal(char_idx);
        data_fd2_battle_anim_phase = 0;
        goto postlude;
    }

    if (ai_class == 4) {
        data_fd2_battle_anim_phase = 0;
        goto pan_walk;
    }

    if (ai_class == 5) {
        if (fd2_attack_action_dispatch(char_idx, team) != 0)
            goto postlude;
        data_fd2_battle_anim_phase = 0;
        fd2_pan_cursor_to_char(char_idx);
        if (*(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr
                      + ai_target_id) != 0)
            goto seek_advance;
        if (fd2_find_tile_with_attribute_match(
                ai_target_id, (uint32)pickup_xy) != 0)
            goto seek_advance;
        result = fd2_ai_walk_to_target_tile(
            (uint32)pickup_xy[0], (uint32)pickup_xy[1],
            char_idx, team);
        if (result == 0)
            fd2_ai_pass_turn_with_heal(char_idx);
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + char_idx * RUNTIME_CHAR_SIZE;
        if (pChar[0] == pickup_xy[0] && pChar[1] == pickup_xy[1]) {
            pTileEvt = (uint8 *)(data_fd2_tile_event_data_table_ptr
                      + ai_target_id * 3);
            pickup_kind = pTileEvt[0x53];
            pickup_param = *(uint16 *)(pTileEvt + 0x54);
            if ((int)pickup_kind < 2) {
                pChar[0x31] = pickup_kind;
                *(uint16 *)(pChar + 0x32) = pickup_param;
                if (pickup_kind == 0) {
                    fd2_add_item_to_inventory(
                        char_idx, (uint32)pickup_param);
                }
            }
            *(uint8 *)(data_fd2_field_map_tile_event_consumed_flags_ptr
                      + ai_target_id) = 1;
            fd2_play_sfx_sample_from_bank(
                data_fd2_audio_fdother_sfx_bank_buf_ptr, 0xc, 1);
            fd2_tick_tile_event_animations();
            pChar[0x34] = 7;
        }
        goto postlude;
    }

    if (ai_class == 7) {
        data_fd2_battle_anim_phase = 0;
        fd2_pan_cursor_to_char(char_idx);
        result = fd2_ai_walk_to_target_tile(
            ai_aux_byte, target_pos, char_idx, team);
        if (result == 0)
            fd2_ai_pass_turn_with_heal(char_idx);
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + char_idx * RUNTIME_CHAR_SIZE;
        if ((uint32)pChar[0] == ai_aux_byte &&
            (uint32)pChar[1] == target_pos) {
            fd2_mark_char_as_dead(char_idx);
        }
        goto postlude;
    }

    if (ai_class == 8) return;

    if (ai_class == 9) {
        found = fd2_find_char_by_id_or_template(ai_aux_byte);
        if (found == -1) goto case_0_attack;
        pTarget = (uint8 *)data_fd2_battle_runtime_char_array_ptr
                + (uint32)found * RUNTIME_CHAR_SIZE;
        fd2_pan_cursor_to_char(char_idx);
        ai_aux_byte = (uint32)pTarget[0];
        target_pos = (uint32)pTarget[1];
        goto walk_check;
    }

    if (ai_class == 10) {
        if (fd2_attack_action_dispatch(char_idx, team) != 0)
            goto postlude;
        data_fd2_battle_anim_phase = 0;
        goto pan_walk;
    }

    if (ai_class == 11) {
        fd2_ai_score_offensive_spell(char_idx, team);
        if ((int)data_fd2_battle_ai_best_spell_score >= 6)
            fd2_execute_ai_offensive_spell(char_idx, team);
        fd2_ai_score_physical_attack(char_idx, team);
        if ((int)data_fd2_battle_ai_best_physical_score >= 6) {
            fd2_execute_ai_physical_attack(char_idx, team);
        } else {
            result = fd2_ai_seek_optimal_position(char_idx, team);
            if (result == 0)
                fd2_ai_pass_turn_with_heal(char_idx);
        }
        goto postlude;
    }

    goto postlude;

case_0_attack:
    if (fd2_attack_action_dispatch(char_idx, team) != 0)
        goto postlude;
seek_advance:
    if (fd2_ai_seek_optimal_position(char_idx, team) != 0)
        goto postlude;
    result = fd2_ai_advance_to_nearest_team_target(char_idx, team);
    goto check_pass;

pan_walk:
    fd2_pan_cursor_to_char(char_idx);
walk_check:
    result = fd2_ai_walk_to_target_tile(
        ai_aux_byte, target_pos, char_idx, team);
check_pass:
    if (result == 0)
        fd2_ai_pass_turn_with_heal(char_idx);
postlude:
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    fd2_check_tile_event_post_action(
        (uint32)pChar[0], (uint32)pChar[1], 1);
    fd2_mark_char_acted_this_turn(char_idx);
    fd2_clear_all_chars_facing();
    fd2_composite_battle_frame(0);
}
