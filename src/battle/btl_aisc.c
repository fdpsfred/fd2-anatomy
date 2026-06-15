/*
 * btl_aisc.c — Battle AI: action scoring (physical attack / item use / spell)
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
 * Battle AI scratch state (zero-initialized BSS scalars).
 * fd2_ai_score_offensive_spell @ 0x1598A resets this to 0 at the start
 * of each evaluation pass, then accumulates the best offensive spell
 * candidate. Read by attack/turn dispatchers (compared signed).
 * ---------------------------------------------------------------- */

/* 0x53C23: best offensive-spell candidate score (signed max accumulator) */
int32 data_fd2_battle_ai_best_spell_score;

/* 0x53C27: best offensive-spell target tile X (zero-extended tile coord; full dword store @0x15AFF) */
uint32 data_fd2_battle_ai_best_spell_target_x;

/* 0x53C2B: best offensive-spell target tile Y (zero-extended tile coord; full dword store @0x15B08) */
uint32 data_fd2_battle_ai_best_spell_target_y;

/* 0x53C2F: chosen offensive-spell id (zero-extended spell id 0x00-0x23; full dword store @0x15B12;
            readers compare signed against 0xB/10 and index the spell handler table) */
uint32 data_fd2_battle_ai_best_spell_id;

/* ----------------------------------------------------------------
 * Battle AI scratch state for offensive ITEM use (zero-initialized BSS scalars).
 * fd2_ai_score_item_use @ 0x1568F resets the score to 0 at the start of each
 * evaluation pass, then keeps the best item candidate. Read by attack/turn
 * dispatchers (compared signed against the spell/physical scores and 0x6).
 * ---------------------------------------------------------------- */

/* 0x53C33: best item candidate score (signed max accumulator).
            Writer @0x1568F (init 0) and @0x15808 (store best); attack dispatch
            @0x14F74.. compares it as a full signed dword against 0x6 and the
            sibling spell/physical scores. Item-use analogue of
            data_fd2_battle_ai_best_spell_score @0x53C23. */
int32 data_fd2_battle_ai_best_item_score;
