/*
 * battle.c — Battle system core: damage pipeline, stat calc, heal/XP
 *
 * Functions emitted from Ghidra decompilation + assembly verification.
 * Strict semantic preservation — logic matches original binary.
 *
 * KNOWN DECOMPILER BUG: Ghidra loses EAX tracking after CALL instructions.
 * Every fd2_advance_rng_state() call site was verified against assembly
 * to ensure the RNG return value is correctly used.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>

/* ----------------------------------------------------------------
 * fd2_advance_rng_state @ 0x4E893
 *
 * LFSR-style RNG: seed = ROL16(seed + 0x9014, 3).
 * Returns new seed in EAX (callers use return value).
 * ---------------------------------------------------------------- */
uint32 fd2_advance_rng_state(void)
{
    uint16 seed;
    seed = data_fd2_shared_rng_seed;
    seed = seed + 0x9014u;
    seed = (seed << 3) | (seed >> 13);
    data_fd2_shared_rng_seed = seed;
    return (uint32)seed;
}

/* ----------------------------------------------------------------
 * fd2_deduct_caster_mp @ 0x1CA89  (14 callers)
 * ---------------------------------------------------------------- */
void fd2_deduct_caster_mp(uint32 caster_idx, uint32 spell_id)
{
    uint8 *spell_entry;
    spell_entry = fd2_get_spell_effect_entry(spell_id);
    data_fd2_battle_runtime_char_array_ptr[caster_idx].mp_current =
        data_fd2_battle_runtime_char_array_ptr[caster_idx].mp_current
        - (uint16)spell_entry[5];
}

/* ----------------------------------------------------------------
 * fd2_apply_hp_heal_and_award_xp @ 0x1C916  (3 callers)
 *
 * heal = base*9/10 + (rng%100 * base)/1000.  XP for player chars.
 * ---------------------------------------------------------------- */
int fd2_apply_hp_heal_and_award_xp(uint32 target_idx, uint32 base_heal)
{
    runtime_char *rc;
    uint16 hp_before;
    uint32 hp_max;
    uint32 base_heal_90;
    uint32 rng_val;
    uint32 extra_heal;
    uint32 hp_after;
    uint32 hp_gained;
    uint32 level_mod;

    rc = data_fd2_battle_runtime_char_array_ptr;
    hp_before = rc[target_idx].hp_current;
    hp_max = (uint32)rc[target_idx].hp_max;
    base_heal_90 = (int)(base_heal * 9) / 10;
    rng_val = fd2_advance_rng_state();
    extra_heal = (int)((int)(rng_val % 100) * (int)base_heal) / 1000;
    hp_after = (uint32)hp_before + base_heal_90 + extra_heal;
    if ((int)hp_max < (int)hp_after) {
        hp_after = hp_max;
    }
    hp_gained = hp_after - (uint32)rc[target_idx].hp_current;
    rc[target_idx].hp_current = (uint16)hp_after;
    level_mod = (uint32)rc[target_idx].status_flags_block[0];
    if ((8 < rc[target_idx].job_id) && (rc[target_idx].job_id < 0x19)) {
        level_mod = level_mod + 0x1e;
    }
    if (rc[target_idx].portrait_id < 0x4b) {
        data_fd2_battle_pending_xp_credit =
            data_fd2_battle_pending_xp_credit +
            (int)(level_mod * 0x28 * hp_gained) / (int)hp_max;
    }
    return (int)(extra_heal + base_heal_90);
}

/* ----------------------------------------------------------------
 * fd2_apply_heal_spell_to_target @ 0x1C8ED  (1 caller)
 *
 * Returns the heal amount (EAX). The body tail-propagates the inner
 * heal fn's EAX: asm 0x1c90c CALL fd2_apply_hp_heal_and_award_xp then
 * ADD ESP,8 / POP EBX / RET (none touch EAX), so the inner return
 * (extra_heal + base_heal_90) IS this fn's return. The sole caller
 * fd2_dispatch_variant_b_cast consumes it: @0x21b77 CALL, @0x21b86
 * PUSH EAX -> fd2_show_damage_number(heal_amount, 0x69, target_id).
 * ---------------------------------------------------------------- */
int fd2_apply_heal_spell_to_target(uint32 target_idx, uint32 spell_id)
{
    int16 *spell_entry;
    spell_entry = (int16 *)fd2_get_spell_effect_entry(spell_id);
    return fd2_apply_hp_heal_and_award_xp(target_idx,
                                          (uint32)(int)*spell_entry);
}

/* ----------------------------------------------------------------
 * fd2_apply_damage_and_award_xp @ 0x1C81F  (3 callers)
 *
 * damage = base*9/10 + (rng%100 * base)/1000.  XP for enemy kills.
 * ---------------------------------------------------------------- */
int fd2_apply_damage_and_award_xp(uint32 target_idx, uint32 base_damage)
{
    runtime_char *rc;
    uint16 hp_current;
    uint16 hp_max;
    uint32 base_damage_90;
    uint32 rng_val;
    uint32 extra_damage;
    int actual_damage;
    uint32 hp_after;
    uint8 *enemy_entry;
    uint32 xp_value;

    rc = data_fd2_battle_runtime_char_array_ptr;
    hp_current = rc[target_idx].hp_current;
    hp_max = rc[target_idx].hp_max;
    base_damage_90 = (int)(base_damage * 9) / 10;
    rng_val = fd2_advance_rng_state();
    extra_damage = (int)((int)(rng_val % 100) * (int)base_damage) / 1000;
    actual_damage = (int)(base_damage_90 + extra_damage);
    hp_after = (uint32)hp_current - (uint32)actual_damage;
    if ((int)hp_after < 0) {
        hp_after = 0;
    }
    rc[target_idx].hp_current = (uint16)hp_after;
    if (rc[target_idx].portrait_id > 0x43) {
        enemy_entry = fd2_get_enemy_data_entry(
            rc[target_idx].portrait_id - 0x44);
        xp_value = (uint32)enemy_entry[9]
                 * (uint32)rc[target_idx].status_flags_block[0];
        if (hp_after != 0) {
            xp_value = (int)(xp_value * (uint32)actual_damage)
                     / (int)(uint32)hp_max;
        }
        data_fd2_battle_pending_xp_credit =
            data_fd2_battle_pending_xp_credit + xp_value;
    }
    return actual_damage;
}

/* ----------------------------------------------------------------
 * fd2_calc_magic_damage @ 0x1C75E  (8 callers)
 *
 * Resist lookup + RNG hit check + damage apply.
 * Status spells (id 10-12) check immunity first.
 * ---------------------------------------------------------------- */
int fd2_calc_magic_damage(uint32 target_idx, uint32 spell_id)
{
    uint8 job_id;
    uint8 *spell_entry;
    int16 base_power;
    uint32 resist;
    int damage;
    uint32 chance_pct;
    uint32 rng_val;
    int immunity;

    job_id = data_fd2_battle_runtime_char_array_ptr[target_idx].job_id;
    spell_entry = fd2_get_spell_effect_entry(spell_id);
    base_power = *(int16 *)spell_entry;
    resist = data_fd2_battle_job_magic_resist_table[(uint32)job_id - 1];
    damage = (int)((int)base_power * (int)resist) / 10;
    chance_pct = (uint32)spell_entry[2];

    if (((int)spell_id > 9) && ((int)spell_id < 0xd)) {
        immunity = fd2_check_char_status_immunity(target_idx);
        if (immunity != 0) {
            return 0;
        }
    }

    rng_val = fd2_advance_rng_state();
    if ((int)(rng_val % 100) >= (int)chance_pct) {
        return 0;
    }

    return fd2_apply_damage_and_award_xp(target_idx, (uint32)damage);
}

/* ----------------------------------------------------------------
 * fd2_face_char_toward_target @ 0x1F04A  (6 callers)
 *
 * Set actor's facing direction toward target based on position.
 * 0=down, 1=left, 2=up, 3=right. Prefers vertical when dx==dy.
 * ---------------------------------------------------------------- */
void fd2_face_char_toward_target(uint32 actor_idx, uint32 target_idx)
{
    runtime_char *actor;
    runtime_char *target;
    int abs_dx;
    int abs_dy;

    actor = &data_fd2_battle_runtime_char_array_ptr[actor_idx];
    target = &data_fd2_battle_runtime_char_array_ptr[target_idx];
    abs_dx = abs((int)(uint32)actor->pos_x
               - (int)(uint32)target->pos_x);
    abs_dy = abs((int)(uint32)actor->pos_y
               - (int)(uint32)target->pos_y);
    if (abs_dx <= abs_dy) {
        actor->sprite_state[1] =
            (target->pos_y < actor->pos_y) ? 2 : 0;
    } else {
        actor->sprite_state[1] =
            (target->pos_x < actor->pos_x) ? 1 : 3;
    }
}

/* ----------------------------------------------------------------
 * fd2_check_char_status_immunity @ 0x1F183  (17 callers)
 *
 * Returns 1 if char is immune to status effects / terrain bonuses.
 * Immune: job_id==0x13 or archetype_flag 4 or 5, UNLESS portrait==0x1C.
 * ---------------------------------------------------------------- */
int fd2_check_char_status_immunity(uint32 char_idx)
{
    runtime_char *rc;
    rc = &data_fd2_battle_runtime_char_array_ptr[char_idx];
    if (rc->portrait_id != 0x1C
        && (rc->job_id == 0x13
            || rc->archetype_flag == 4
            || rc->archetype_flag == 5)) {
        return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_calculate_combat_hit_outcome @ 0x29F72  (1 caller)
 *
 * Pre-compute a single combat hit outcome into a 6-element uint
 * struct. Same formula as fd2_execute_attack_damage_calculation
 * but without visual effects. Writes poison to defender on hit.
 * Output: [miss, crit, poison, reserved, double_hit, damage].
 * ---------------------------------------------------------------- */
void fd2_calculate_combat_hit_outcome(uint32 attacker_idx,
                                       uint32 defender_idx,
                                       uint32 *outcome_out_ptr)
{
    uint8 *pAttacker;
    uint8 *pDefender;
    uint32 atk_ap;
    uint32 def_dp;
    uint32 def_hp_after;
    uint32 def_hp_max;
    uint32 atk_hit;
    uint32 def_evade;
    uint32 atk_job_idx;
    uint8 atk_level;
    uint8 def_level;
    uint32 weapon_slot;
    uint8 wpn_item_id;
    uint8 *weapon_entry;
    uint8 weapon_class;
    uint32 wpn_special_chance;
    uint32 crit_total;
    uint32 damage;
    uint32 jitter_range;
    uint32 rng_val;
    uint8 tile_attr_buf[8];
    uint8 tile_id;
    uint8 *enemy_entry;

    damage = 0;
    outcome_out_ptr[0] = 1;
    outcome_out_ptr[1] = 0;
    outcome_out_ptr[2] = 0;
    outcome_out_ptr[3] = 0;
    outcome_out_ptr[4] = 0;
    outcome_out_ptr[5] = 0;

    pAttacker = (uint8 *)&data_fd2_battle_runtime_char_array_ptr[
                              attacker_idx];
    pDefender = (uint8 *)&data_fd2_battle_runtime_char_array_ptr[
                              defender_idx];
    atk_ap = (uint32)*(uint16 *)(pAttacker + 0x48);
    def_dp = (uint32)*(uint16 *)(pDefender + 0x4A);
    def_hp_after = (uint32)*(uint16 *)(pDefender + 0x40);
    def_hp_max = (uint32)*(uint16 *)(pDefender + 0x42);
    atk_hit = (uint32)*(uint16 *)(pAttacker + 0x4C);
    def_evade = (uint32)*(uint16 *)(pDefender + 0x4E);
    atk_job_idx = (uint32)pAttacker[0x20] - 1;
    atk_level = pAttacker[0x21];
    def_level = pDefender[0x21];

    weapon_slot = fd2_find_equipped_item_by_kind(attacker_idx, 0);
    wpn_item_id = fd2_get_inventory_slot_item_id(
                      attacker_idx, weapon_slot);
    weapon_entry = fd2_get_item_effect_entry((uint32)wpn_item_id);
    weapon_class = weapon_entry[9];
    wpn_special_chance = (uint32)weapon_entry[10];

    if (fd2_check_char_status_immunity(attacker_idx) == 0) {
        fd2_read_tile_attribute_at_pos(
            (uint32)pAttacker[0], (uint32)pAttacker[1],
            (uint32)tile_attr_buf);
        tile_id = tile_attr_buf[5];
        atk_ap = atk_ap +
            (int)(data_fd2_battle_tile_attr_mv_modifier_table[tile_id]
                  * atk_ap) / 100;
    }
    if (fd2_check_char_status_immunity(defender_idx) == 0) {
        fd2_read_tile_attribute_at_pos(
            (uint32)pDefender[0], (uint32)pDefender[1],
            (uint32)tile_attr_buf);
        tile_id = tile_attr_buf[5];
        def_dp = def_dp +
            (int)(data_fd2_battle_tile_attr_def_modifier_table[tile_id]
                  * def_dp) / 100;
    }

    crit_total =
        (uint32)data_fd2_battle_job_crit_rate_table[atk_job_idx];

    if (weapon_class == 4) {
        crit_total = crit_total + wpn_special_chance;
    } else if (weapon_class == 2) {
        rng_val = fd2_advance_rng_state();
        if ((int)(rng_val % 100) < (int)wpn_special_chance) {
            rng_val = fd2_advance_rng_state();
            *(uint8 *)(pDefender + 0x25) =
                (uint8)((int)rng_val % 4) + 2;
            outcome_out_ptr[2] = 1;
        }
    } else if (weapon_class == 3) {
        outcome_out_ptr[4] = 1;
    }

    rng_val = fd2_advance_rng_state();
    if ((int)(rng_val % 100)
        < (int)atk_hit - (int)def_evade) {
        outcome_out_ptr[0] = 0;

        rng_val = fd2_advance_rng_state();
        if ((int)(rng_val % 100) < (int)crit_total) {
            def_dp = (int)def_dp / 2;
            outcome_out_ptr[1] = 1;
        }

        damage = (int)((int)(atk_ap - def_dp) * 9) / 10;
        if ((int)damage < 0) {
            damage = 0;
        }
        jitter_range = (int)damage / 9;
        if (jitter_range != 0) {
            rng_val = fd2_advance_rng_state();
            damage = damage + (int)rng_val % (int)jitter_range;
        }
        def_hp_after = def_hp_after - damage;
        if ((int)def_hp_after < 0) {
            def_hp_after = 0;
        }
    }

    if (pAttacker[6] == TEAM_PLAYER && pDefender[7] > 0x43) {
        enemy_entry = fd2_get_enemy_data_entry(pDefender[7] - 0x44);
        if ((pAttacker[0x20] > 8 && pAttacker[0x20] < 0x19)
            || pAttacker[8] == 0x1C) {
            atk_level = atk_level + 0x1E;
        }
        data_fd2_battle_pending_xp_credit =
            ((uint32)def_level * (uint32)enemy_entry[9])
            / (uint32)atk_level;
        if (def_hp_after != 0) {
            data_fd2_battle_pending_xp_credit =
                (int)(data_fd2_battle_pending_xp_credit * damage)
                / (int)def_hp_max;
        }
    }

    outcome_out_ptr[5] = damage;
}

/* ----------------------------------------------------------------
 * fd2_flash_char_hit_sprite @ 0x2A289
 *
 * Paint a "character takes damage" flash overlay. Routes to the
 * correct screen position based on team (enemy→lower, ally→upper).
 * Chapter 24 / char 0x11 (Sumeti) overrides to enemy position.
 * ---------------------------------------------------------------- */
void fd2_flash_char_hit_sprite(uint32 workspace_buf,
                                uint32 char_unit_id)
{
    uint32 screen_off;

    if (data_fd2_battle_runtime_char_array_ptr[char_unit_id].team
        == 0) {
        screen_off = 0xC080;
    } else {
        screen_off = 0x05AB;
    }
    if (data_fd2_chapter_current_chapter_id == 0x18
        && char_unit_id == 0x11) {
        screen_off = 0xC080;
    }
    fd2_render_mini_char_status_panel(
        workspace_buf + screen_off, 0x140, char_unit_id);
}

/* ----------------------------------------------------------------
 * fd2_compute_combat_bubble_screen_pos @ 0x1EC2A  (2 callers)
 *
 * Compute speech bubble screen position for a char, adjusting
 * for facing direction and screen boundaries. Output is two
 * int32 values at out_xy_ptr: [0]=x, [4]=y.
 * ---------------------------------------------------------------- */
void fd2_compute_combat_bubble_screen_pos(uint32 out_xy_ptr,
                                           uint32 char_idx)
{
    runtime_char *rc;
    int new_y;

    rc = &data_fd2_battle_runtime_char_array_ptr[char_idx];
    *(int32 *)out_xy_ptr =
        ((int)(uint32)rc->pos_x
         - (int)data_fd2_battle_view_window_origin_x) * 0x18 + 4;
    *(int32 *)(out_xy_ptr + 4) =
        ((int)(uint32)rc->pos_y
         - (int)data_fd2_battle_view_window_origin_y) * 0x18;

    if (rc->sprite_state[1] < 2) {
        new_y = *(int32 *)(out_xy_ptr + 4) - 0x12;
        if (new_y < 0) {
            *(int32 *)(out_xy_ptr + 4) += 5;
        } else {
            *(int32 *)(out_xy_ptr + 4) = new_y;
        }
        if (*(int32 *)out_xy_ptr + 0x6C > 0x13F) {
            *(int32 *)out_xy_ptr -= 0x56;
            return;
        }
    } else {
        if (*(int32 *)(out_xy_ptr + 4) + 0x25 <= 0xC7) {
            *(int32 *)(out_xy_ptr + 4) += 0x16;
        } else {
            *(int32 *)(out_xy_ptr + 4) += 5;
        }
        if (*(int32 *)out_xy_ptr - 0x56 >= 0) {
            *(int32 *)out_xy_ptr -= 0x58;
            return;
        }
    }
    *(int32 *)out_xy_ptr += 0x1C;
}

/* ----------------------------------------------------------------
 * fd2_apply_mp_heal_and_award_xp @ 0x1C9DD  (1 caller)
 *
 * MP version of fd2_apply_hp_heal_and_award_xp. Same 90-100% RNG
 * formula but for MP. No job bonus on level (unlike HP version).
 * Tail-jumps into hp_heal's shared XP epilogue at 0x1C9C7.
 * ---------------------------------------------------------------- */
int fd2_apply_mp_heal_and_award_xp(uint32 target_idx, uint32 base_heal)
{
    runtime_char *rc;
    uint16 mp_before;
    uint32 mp_max;
    uint32 base_heal_90;
    uint32 rng_val;
    uint32 extra_heal;
    uint32 mp_after;
    uint32 mp_gained;

    rc = data_fd2_battle_runtime_char_array_ptr;
    mp_before = rc[target_idx].mp_current;
    mp_max = (uint32)rc[target_idx].mp_max;
    base_heal_90 = (int)(base_heal * 9) / 10;
    rng_val = fd2_advance_rng_state();
    extra_heal = (int)((int)(rng_val % 100) * (int)base_heal) / 1000;
    mp_after = (uint32)mp_before + base_heal_90 + extra_heal;
    if ((int)mp_max < (int)mp_after) {
        mp_after = mp_max;
    }
    mp_gained = mp_after - (uint32)rc[target_idx].mp_current;
    rc[target_idx].mp_current = (uint16)mp_after;
    if (rc[target_idx].portrait_id < 0x4b) {
        data_fd2_battle_pending_xp_credit =
            data_fd2_battle_pending_xp_credit +
            (int)((uint32)rc[target_idx].status_flags_block[0]
                  * 0x28 * mp_gained) / (int)mp_max;
    }
    return (int)(extra_heal + base_heal_90);
}

/* ----------------------------------------------------------------
 * fd2_get_inventory_slot_item_id @ 0x1B722
 *
 * Returns item_id from runtime_char inventory. Each slot is 2 bytes
 * (flag + item_id); item_id is at inventory_slots[slot*2 + 1].
 * ---------------------------------------------------------------- */
uint8 fd2_get_inventory_slot_item_id(uint32 char_idx, uint32 slot_idx)
{
    return data_fd2_battle_runtime_char_array_ptr[char_idx]
               .inventory_slots[slot_idx * 2 + 1];
}

/* ----------------------------------------------------------------
 * fd2_read_tile_attribute_at_pos @ 0x12E38
 *
 * Unpack battle-map tile metadata + attribute flags into an
 * 8-byte caller-supplied buffer.
 *
 * Out-buf layout:
 *   +0 ushort sprite_idx      (10-bit tile-sheet index)
 *   +2 ushort terrain_class   (5-bit terrain type)
 *   +4..+7   tile_attr_flags  (4 bytes from attr flag table)
 * ---------------------------------------------------------------- */
void fd2_read_tile_attribute_at_pos(uint32 world_x, uint32 world_y,
                                     uint32 out_buf)
{
    uint32 tile_meta_addr;
    uint16 sprite_idx;
    uint8 terrain_byte;
    uint32 attr_ptr;

    tile_meta_addr = data_fd2_battle_tile_map_ptr
                   + (world_y * data_fd2_battle_map_width_tiles
                      + world_x) * 4;
    sprite_idx = *(uint16 *)(tile_meta_addr + 4) & 0x3FF;
    terrain_byte = *(uint8 *)(tile_meta_addr + 6);
    *(uint16 *)out_buf = sprite_idx;
    *(uint16 *)(out_buf + 2) = (uint16)(terrain_byte & 0x1F);
    attr_ptr = data_fd2_tile_attribute_flags_buffer_ptr
             + (int16)sprite_idx * 4;
    *(uint8 *)(out_buf + 4) = *(uint8 *)attr_ptr;
    *(uint8 *)(out_buf + 5) = *(uint8 *)(attr_ptr + 1);
    *(uint8 *)(out_buf + 6) = *(uint8 *)(attr_ptr + 2);
    *(uint8 *)(out_buf + 7) = *(uint8 *)(attr_ptr + 3);
}

/* ----------------------------------------------------------------
 * fd2_recompute_runtime_char_total_stats @ 0x1145A  (2 callers)
 *
 * Sums base + equipped-item boosts. Operates on menu roster buffer.
 * ---------------------------------------------------------------- */
void fd2_recompute_runtime_char_total_stats(uint32 slot_idx)
{
    uint32 slot_addr;
    uint32 inv_slot_addr;
    uint8 *item_entry;
    uint32 slot_iter;
    int ap_total;
    int dp_total;
    int dx_total;
    int stat4_total;

    slot_addr = data_fd2_shared_menu_party_roster_buffer_ptr
              + slot_idx * 0x50;
    ap_total = (int)*(int16 *)(slot_addr + 0x37);
    dp_total = (int)*(int16 *)(slot_addr + 0x39);
    dx_total = (int)*(int16 *)(slot_addr + 0x3e);
    stat4_total = dx_total;
    for (slot_iter = 0; (int)slot_iter < 8; slot_iter = slot_iter + 1) {
        inv_slot_addr = slot_iter * 2 + slot_addr;
        if ((*(uint8 *)(inv_slot_addr + 10) & 0x40) != 0) {
            item_entry = fd2_get_item_effect_entry(
                (uint32)*(uint8 *)(inv_slot_addr + 0xb));
            ap_total = ap_total + (int)*(int16 *)(item_entry + 1);
            dp_total = dp_total + (int)*(int16 *)(item_entry + 5);
            dx_total = dx_total + (int)*(int16 *)(item_entry + 3);
            stat4_total = stat4_total + (int)*(int16 *)(item_entry + 7);
        }
    }
    *(uint16 *)(slot_addr + 0x48) = (uint16)ap_total;
    *(uint16 *)(slot_addr + 0x4a) = (uint16)dp_total;
    *(uint16 *)(slot_addr + 0x4c) = (uint16)dx_total;
    *(uint16 *)(slot_addr + 0x4e) = (uint16)stat4_total;
}

/* ----------------------------------------------------------------
 * fd2_recalculate_combat_stats @ 0x1B750  (12 callers)
 *
 * Same as recompute_runtime_char_total_stats but operates on
 * battle runtime char array. Adds DX buff (+15) and AP/DP buff
 * (x1.15 via FPU multiply).
 * ---------------------------------------------------------------- */
void fd2_recalculate_combat_stats(uint32 char_idx)
{
    runtime_char *rc;
    uint8 *item_entry;
    uint32 slot_iter;
    int ap_total;
    int dp_total;
    int dx_total;
    int stat4_total;

    rc = data_fd2_battle_runtime_char_array_ptr;
    ap_total = (int)*(int16 *)(rc[char_idx].combat_aux_block + 0x10);
    dp_total = (int)*(int16 *)(rc[char_idx].combat_aux_block + 0x12);
    dx_total = (int)*(int16 *)(rc[char_idx].ai_target_and_dx_block + 1);
    if (rc[char_idx].status_flags_block[3] != 0) {
        dx_total = dx_total + 0xf;
    }
    stat4_total = dx_total;
    for (slot_iter = 0; (int)slot_iter < 8; slot_iter = slot_iter + 1) {
        if ((rc[char_idx].inventory_slots[slot_iter * 2] & 0x40) != 0) {
            item_entry = fd2_get_item_effect_entry(
                (uint32)rc[char_idx].inventory_slots[slot_iter * 2 + 1]);
            ap_total = ap_total + (int)*(int16 *)(item_entry + 1);
            dp_total = dp_total + (int)*(int16 *)(item_entry + 5);
            dx_total = dx_total + (int)*(int16 *)(item_entry + 3);
            stat4_total = stat4_total + (int)*(int16 *)(item_entry + 7);
        }
    }
    if (rc[char_idx].status_flags_block[1] != 0) {
        ap_total = (int)((double)ap_total * 1.15);
    }
    if (rc[char_idx].status_flags_block[2] != 0) {
        dp_total = (int)((double)dp_total * 1.15);
    }
    rc[char_idx].ap = (uint16)ap_total;
    rc[char_idx].dp = (uint16)dp_total;
    rc[char_idx].dx_current = (uint16)dx_total;
    rc[char_idx].stat4_current = (uint16)stat4_total;
}

/* ----------------------------------------------------------------
 * fd2_check_can_counter_attack @ 0x1F0DC  (6 callers)
 *
 * Returns 1 if defender can counter (adjacent + awake + melee weapon).
 * Returns -1 otherwise.
 * ---------------------------------------------------------------- */
int fd2_check_can_counter_attack(uint32 attacker_idx, uint32 defender_idx)
{
    runtime_char *attacker;
    runtime_char *defender;
    uint32 dx_dist;
    uint32 dy_dist;
    uint32 weapon_slot;
    uint8 weapon_id;
    uint8 *weapon_entry;

    attacker = &data_fd2_battle_runtime_char_array_ptr[attacker_idx];
    defender = &data_fd2_battle_runtime_char_array_ptr[defender_idx];
    if (defender->status_sleep_flag != 0) {
        return -1;
    }
    dx_dist = abs((int)(uint32)attacker->pos_x
                - (int)(uint32)defender->pos_x);
    dy_dist = abs((int)(uint32)attacker->pos_y
                - (int)(uint32)defender->pos_y);
    if (dx_dist + dy_dist != 1) {
        return -1;
    }
    weapon_slot = fd2_find_equipped_item_by_kind(defender_idx, 0);
    if (weapon_slot == 0xffffffff) {
        return -1;
    }
    weapon_id = fd2_get_inventory_slot_item_id(defender_idx, weapon_slot);
    weapon_entry = fd2_get_item_effect_entry((uint32)weapon_id);
    if (weapon_entry[0xb] != 1) {
        return -1;
    }
    return 1;
}

/* ----------------------------------------------------------------
 * fd2_check_can_default_attack_target @ 0x1DEBE  (1 caller)
 *
 * Returns 1 if char can default-attack tile (x,y).
 * Returns -1 otherwise.
 * ---------------------------------------------------------------- */
int fd2_check_can_default_attack_target(uint32 char_idx,
                                         uint32 tile_x, uint32 tile_y)
{
    runtime_char *rc;
    uint32 dx_dist;
    uint32 dy_dist;
    uint32 weapon_slot;
    uint8 weapon_id;
    uint8 *weapon_entry;

    rc = &data_fd2_battle_runtime_char_array_ptr[char_idx];
    if (rc->status_sleep_flag != 0) {
        return -1;
    }
    dx_dist = abs((int)tile_x - (int)(uint32)rc->pos_x);
    dy_dist = abs((int)tile_y - (int)(uint32)rc->pos_y);
    if (dx_dist + dy_dist != 1) {
        return -1;
    }
    weapon_slot = fd2_find_equipped_item_by_kind(char_idx, 0);
    if (weapon_slot == 0xffffffff) {
        return -1;
    }
    weapon_id = fd2_get_inventory_slot_item_id(char_idx, weapon_slot);
    weapon_entry = fd2_get_item_effect_entry((uint32)weapon_id);
    if (weapon_entry[0xb] > 1) {
        return -1;
    }
    return 1;
}

/* ----------------------------------------------------------------
 * fd2_execute_attack_damage_calculation @ 0x1ECC7  (1 caller)
 *
 * CORE PHYSICAL COMBAT FORMULA.
 * All RNG sites verified against assembly — decompiler had 4 bugs.
 * ---------------------------------------------------------------- */
int fd2_execute_attack_damage_calculation(int attacker_idx, int defender_idx)
{
    runtime_char *rc_arr;
    uint8 *pAttacker;
    uint8 *pDefender;
    uint32 attacker_AP;
    uint32 defender_DP;
    uint32 defender_HP_cur;
    uint32 defender_HP_max;
    uint32 attacker_DX;
    uint32 defender_DX;
    uint32 job_id_m1;
    uint8 attacker_level;
    uint8 defender_level;
    uint32 weapon_slot;
    uint8 weapon_item_id;
    uint8 *weapon_entry;
    uint8 weapon_class;
    uint32 weapon_elem;
    uint32 base_crit_pct;
    uint32 total_crit_pct;
    int dx_diff;
    uint32 damage;
    uint32 jitter_range;
    int immunity;
    uint8 tile_attr_buf[8];
    uint8 tile_id;
    uint8 *enemy_entry;
    uint32 rng_val;

    damage = 0;
    data_fd2_battle_last_hit_or_miss_flag = 1;

    rc_arr = data_fd2_battle_runtime_char_array_ptr;
    pAttacker = (uint8 *)&rc_arr[attacker_idx];
    pDefender = (uint8 *)&rc_arr[defender_idx];

    attacker_AP = (uint32)*(uint16 *)(pAttacker + 0x48);
    defender_DP = (uint32)*(uint16 *)(pDefender + 0x4a);
    defender_HP_cur = (uint32)*(uint16 *)(pDefender + 0x40);
    defender_HP_max = (uint32)*(uint16 *)(pDefender + 0x42);
    attacker_DX = (uint32)*(uint16 *)(pAttacker + 0x4c);
    defender_DX = (uint32)*(uint16 *)(pDefender + 0x4e);
    job_id_m1 = (uint32)*(uint8 *)(pAttacker + 0x20) - 1;
    attacker_level = *(uint8 *)(pAttacker + 0x21);
    defender_level = *(uint8 *)(pDefender + 0x21);

    weapon_slot = fd2_find_equipped_item_by_kind(attacker_idx, 0);
    weapon_item_id = fd2_get_inventory_slot_item_id(attacker_idx, weapon_slot);
    weapon_entry = fd2_get_item_effect_entry((uint32)weapon_item_id);
    weapon_class = weapon_entry[9];
    weapon_elem = (uint32)weapon_entry[10];

    immunity = fd2_check_char_status_immunity(attacker_idx);
    if (immunity == 0) {
        fd2_read_tile_attribute_at_pos(
            (uint32)pAttacker[0], (uint32)pAttacker[1], (uint32)tile_attr_buf);
        tile_id = tile_attr_buf[5];
        attacker_AP = attacker_AP +
            (int)(data_fd2_battle_tile_attr_mv_modifier_table[tile_id]
                  * attacker_AP) / 100;
    }
    immunity = fd2_check_char_status_immunity(defender_idx);
    if (immunity == 0) {
        fd2_read_tile_attribute_at_pos(
            (uint32)pDefender[0], (uint32)pDefender[1], (uint32)tile_attr_buf);
        tile_id = tile_attr_buf[5];
        defender_DP = defender_DP +
            (int)(data_fd2_battle_tile_attr_def_modifier_table[tile_id]
                  * defender_DP) / 100;
    }

    base_crit_pct = (uint32)data_fd2_battle_job_crit_rate_table[job_id_m1];
    total_crit_pct = base_crit_pct;

    if (weapon_class == 4) {
        total_crit_pct = base_crit_pct + weapon_elem;
    }
    else if (weapon_class == 2) {
        rng_val = fd2_advance_rng_state();
        if ((int)(rng_val % 100) < (int)weapon_elem) {
            rng_val = fd2_advance_rng_state();
            *(uint8 *)(pDefender + 0x25) =
                (uint8)((int)rng_val % 4) + 2;
            fd2_set_full_vga_palette_to_color(1, 0x20, 0);
            fd2_delay_ticks(0x14);
            fd2_set_vga_palette_range_with_add(0, 0xff, 0);
            fd2_delay_ticks(0x28);
            fd2_set_full_vga_palette_to_color(1, 0x20, 0);
            fd2_delay_ticks(0x14);
            fd2_set_vga_palette_range_with_add(0, 0xff, 0);
        }
    }

    rng_val = fd2_advance_rng_state();
    dx_diff = (int)attacker_DX - (int)defender_DX;
    if ((int)(rng_val % 100) < dx_diff) {
        data_fd2_battle_last_hit_or_miss_flag = 0;

        rng_val = fd2_advance_rng_state();
        if ((int)(rng_val % 100) < (int)total_crit_pct) {
            fd2_set_vga_palette_range_with_add(0, 0xff, 0x3f);
            fd2_delay_ticks(0x14);
            fd2_set_vga_palette_range_with_add(0, 0xff, 0);
            fd2_delay_ticks(0x28);
            fd2_set_vga_palette_range_with_add(0, 0xff, 0x3f);
            fd2_delay_ticks(0x14);
            fd2_set_vga_palette_range_with_add(0, 0xff, 0);
            defender_DP = (int)defender_DP / 2;
        }

        damage = (int)((int)(attacker_AP - defender_DP) * 9) / 10;
        if ((int)damage < 0) {
            damage = 0;
        }
        jitter_range = (int)damage / 9;
        if (jitter_range != 0) {
            rng_val = fd2_advance_rng_state();
            damage = damage + (int)rng_val % (int)jitter_range;
        }

        defender_HP_cur = defender_HP_cur - damage;
        if ((int)defender_HP_cur < 0) {
            defender_HP_cur = 0;
        }
    }

    *(uint16 *)(pDefender + 0x40) = (uint16)defender_HP_cur;

    if (pAttacker[6] == TEAM_PLAYER && pDefender[7] > 0x43) {
        enemy_entry = fd2_get_enemy_data_entry(pDefender[7] - 0x44);
        if ((pAttacker[0x20] > 8 && pAttacker[0x20] < 0x19)
            || pAttacker[8] == 0x1c) {
            attacker_level = attacker_level + 0x1e;
        }
        data_fd2_battle_pending_xp_credit =
            ((uint32)enemy_entry[9] * (uint32)defender_level)
            / (uint32)attacker_level;
        if (defender_HP_cur != 0) {
            data_fd2_battle_pending_xp_credit =
                (int)(data_fd2_battle_pending_xp_credit * damage)
                / (int)defender_HP_max;
        }
    }

    return (int)defender_HP_cur;
}

/* ----------------------------------------------------------------
 * fd2_find_equipped_item_by_kind @ 0x1B83D  (8 callers)
 *
 * Scan the 8 inventory slots of runtime_char[char_idx]. Each slot is
 * 2 bytes: [0]=bSlot_flag (bit6/0x40 = equipped), [1]=bItem_id.
 * Returns the index of the first equipped slot whose item_id matches
 * the requested kind, else 0xFFFFFFFF (-1):
 *   kind == 0 (physical / weapon / armor): item_id <  0x80
 *   kind != 0 (magical / spellbook):       item_id >= 0x80
 * Item id 0x80 is the physical/magical split (assets/items.md).
 * ---------------------------------------------------------------- */
uint32 fd2_find_equipped_item_by_kind(uint32 char_idx, uint32 kind)
{
    runtime_char *rc;
    uint32 slot_iter;

    rc = data_fd2_battle_runtime_char_array_ptr;
    for (slot_iter = 0; (int)slot_iter < 8; slot_iter = slot_iter + 1) {
        if ((rc[char_idx].inventory_slots[slot_iter * 2] & 0x40) != 0
            && ((kind == 0
                 && rc[char_idx].inventory_slots[slot_iter * 2 + 1] < 0x80)
                || (kind != 0
                 && 0x7f < rc[char_idx].inventory_slots[slot_iter * 2 + 1]))) {
            return slot_iter;
        }
    }
    return 0xffffffff;
}
