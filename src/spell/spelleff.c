/*
 * spelleff.c — Spell/item effect appliers: use-effect dispatch, stat/status, targeted casts
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_apply_use_effect_dispatch @ 0x20C6F  (2 callers)
 *
 * Top-level dispatcher for item/spell USE effects. Reads the item's
 * effect_code and dispatches to the appropriate handler. Effect codes
 * 5-0x18 are supported. Finalizes with XP reset + death/drop processing.
 * ---------------------------------------------------------------- */
void fd2_apply_use_effect_dispatch(uint32 caster_idx, uint32 inv_slot,
                                    uint32 target_count,
                                    uint32 p_target_array)
{
    uint8 item_id;
    uint8 *item_entry;
    uint32 effect_param;
    uint8 effect_code;
    uint32 drops_buf[25];
    uint32 pending_drops;
    uint8 target_id;
    uint8 saved_mv;

    fd2_load_status_effect_sfx();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    item_id = fd2_get_inventory_slot_item_id(caster_idx, inv_slot);
    item_entry = fd2_get_item_effect_entry((uint32)item_id);
    effect_param = (uint32)*(uint16 *)(item_entry + 0xE);
    effect_code = item_entry[0xD];

    if (effect_code == 0x05 || effect_code == 0x0D) {
        fd2_cast_group_hp_heal_spell(
            caster_idx, target_count, p_target_array, effect_param);
        if (effect_code == 0x05) goto consume_item;
    } else if (effect_code == 0x06) {
        fd2_cast_status_cure_spell(
            caster_idx, 0x14, target_count, p_target_array, 0x25);
        if (data_fd2_battle_spell_aoe_count_and_fx_queue_idx != 0) {
            fd2_animate_spell_projectile_paths();
        }
consume_item:
        fd2_remove_inventory_slot_at(caster_idx, inv_slot);
    } else if (effect_code == 0x07) {
        fd2_cast_status_cure_spell(
            caster_idx, 0x15, target_count, p_target_array, 0x26);
        if (data_fd2_battle_spell_aoe_count_and_fx_queue_idx != 0) {
            fd2_animate_spell_projectile_paths();
        }
        goto consume_item;
    } else if (effect_code == 0x08) {
        fd2_apply_item_stat_modifier_with_anim(
            caster_idx, effect_param, 0x37, inv_slot,
            target_count, p_target_array, 0x11);
    } else if (effect_code == 0x09) {
        fd2_apply_item_stat_modifier_with_anim(
            caster_idx, effect_param, 0x39, inv_slot,
            target_count, p_target_array, 0x12);
    } else if (effect_code == 0x0A) {
        fd2_apply_item_stat_modifier_with_anim(
            caster_idx, effect_param, 0x3E, inv_slot,
            target_count, p_target_array, 0x13);
    } else if (effect_code == 0x0B) {
        uint32 i;
        fd2_animate_spell_impact_per_target(
            caster_idx, 0x0D, target_count, p_target_array);
        fd2_animate_status_effect_overlay_flicker(
            caster_idx, 0x0D, target_count, p_target_array);
        for (i = 0; (int)i < (int)target_count; i++) {
            uint8 tid;
            tid = *((uint8 *)p_target_array + i);
            if (data_fd2_battle_runtime_char_array_ptr[
                    (uint32)tid].mp_max == 0) {
                fd2_show_miss_indicator((uint32)tid);
            } else {
                int heal;
                heal = fd2_apply_mp_heal_and_award_xp(
                           (uint32)tid, effect_param);
                fd2_show_damage_number((uint32)heal, 0x69,
                                        (uint32)tid);
            }
        }
        fd2_composite_battle_frame(0);
        fd2_animate_spell_projectile_paths();
        goto consume_item;
    } else if (effect_code == 0x0C) {
        fd2_cast_speed_boost_spell(
            caster_idx, target_count, p_target_array);
    } else if (effect_code == 0x0E) {
        fd2_cast_status_inflict_spell(
            caster_idx, 0x1B, target_count, p_target_array, 0x26);
    } else if (effect_code == 0x0F) {
        fd2_cast_dp_boost_spell(
            caster_idx, target_count, p_target_array);
    } else if (effect_code == 0x10) {
        fd2_cast_ap_boost_spell(
            caster_idx, target_count, (uint8 *)p_target_array);
    } else if (effect_code == 0x11) {
        fd2_apply_item_stat_modifier_with_anim(
            caster_idx, effect_param, 0x42, inv_slot,
            target_count, p_target_array, 0x0D);
    } else if (effect_code == 0x12) {
        fd2_apply_item_stat_modifier_with_anim(
            caster_idx, effect_param, 0x46, inv_slot,
            target_count, p_target_array, 0x0D);
    } else if (effect_code == 0x13) {
        target_id = *(uint8 *)p_target_array;
        saved_mv = data_fd2_battle_runtime_char_array_ptr[
                       (uint32)target_id].movement_order;
        fd2_apply_item_stat_modifier_with_anim(
            caster_idx, effect_param, 0x3B, inv_slot,
            target_count, p_target_array, 0x13);
        data_fd2_battle_runtime_char_array_ptr[
            (uint32)target_id].movement_order = saved_mv;
    } else if (effect_code == 0x14 || effect_code == 0x18) {
        uint32 j;
        fd2_animate_spell_impact_per_target(
            caster_idx, effect_param, target_count, p_target_array);
        fd2_animate_spell_overlay_blink(
            caster_idx, effect_param, target_count, p_target_array);
        for (j = 0; (int)j < (int)target_count; j++) {
            uint8 tid2;
            int dmg;
            tid2 = *((uint8 *)p_target_array + j);
            dmg = fd2_calc_magic_damage((uint32)tid2, effect_param);
            if (dmg != 0) {
                fd2_show_damage_number((uint32)dmg, 0x5E,
                                        (uint32)tid2);
            } else {
                fd2_show_miss_indicator((uint32)tid2);
            }
        }
        fd2_composite_battle_frame(0);
        fd2_animate_spell_projectile_paths();
    } else if (effect_code == 0x15) {
        fd2_apply_attack_spell_damage(
            caster_idx, target_count, p_target_array, effect_param);
    } else if (effect_code == 0x16) {
        fd2_cast_status_inflict_spell(
            caster_idx, 0x16, target_count, p_target_array, 0x27);
    } else if (effect_code == 0x17) {
        fd2_cast_spell_17_teleport(
            caster_idx, target_count, p_target_array);
    }

    data_fd2_battle_pending_xp_credit = 0;
    fd2_stop_and_free_status_effect_sfx();
    pending_drops = fd2_collect_pending_death_drops((uint32)drops_buf);
    fd2_play_death_animation_and_mark_dead();
    fd2_process_battle_drop_entries(
        caster_idx, pending_drops, (uint32)drops_buf);
}

/* ----------------------------------------------------------------
 * fd2_apply_item_stat_modifier_with_anim @ 0x21082
 *
 * Permanent stat-up (scrolls): animate impact, bump the stat word
 * at field_offset in the target's runtime_char, then consume item.
 * Only first target in array receives the stat change.
 * ---------------------------------------------------------------- */
void fd2_apply_item_stat_modifier_with_anim(
    uint32 caster_idx, uint32 stat_delta, uint32 field_offset,
    uint32 inv_slot, uint32 target_count,
    uint32 p_target_array, uint32 anim_idx)
{
    uint8 target_id;
    uint32 rc_addr;

    fd2_animate_spell_impact_per_target(
        caster_idx, anim_idx, target_count, p_target_array);
    fd2_animate_status_effect_overlay_flicker(
        caster_idx, anim_idx, target_count, p_target_array);

    target_id = *(uint8 *)p_target_array;
    rc_addr = (uint32)&data_fd2_battle_runtime_char_array_ptr[
                  (uint32)target_id];
    *(int16 *)(rc_addr + field_offset) =
        *(int16 *)(rc_addr + field_offset) + (int16)stat_delta;

    fd2_show_damage_number(stat_delta, 0x5E, (uint32)target_id);
    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
    fd2_recalculate_combat_stats(caster_idx);
    fd2_remove_inventory_slot_at(caster_idx, inv_slot);
}

/* ----------------------------------------------------------------
 * fd2_apply_attack_spell_damage @ 0x2111A  (2 callers)
 *
 * Attack-spell damage applier with full impact + full-screen-flash
 * animations. Plays the per-target impact animation and the full-screen
 * flash (both keyed by the spell id in arg 4), then for every target in
 * the byte array calls fd2_calc_magic_damage(target_id, spell_id): a 0
 * return is a miss (draw the miss indicator), otherwise draw the damage
 * number with glyph 0x5E ('^'). Closes with fd2_composite_battle_frame(0)
 * + fd2_animate_spell_projectile_paths().
 *
 * fd2_calc_magic_damage applies the HP decrement internally; this
 * function only drives the visual presentation (impact + flash + per-
 * target number + composite + projectile trail) and never touches HP.
 *
 * Arg 4 is a spell id (used for the animations and the damage calc), not
 * an item field: caller fd2_apply_use_effect_dispatch @ 0x20C6F passes
 * the item's effect_param (item effect 0x15 = the attack-spell variant),
 * and caller fd2_execute_summon_spell_cast @ 0x27FC9 passes literal 0x20
 * (the 熾天使 summon finale).
 * ---------------------------------------------------------------- */
void fd2_apply_attack_spell_damage(uint32 caster_idx,
                                    uint32 target_count,
                                    uint32 p_target_array,
                                    uint32 effect_param)
{
    uint32 i;
    uint8 target_id;
    int dmg;

    fd2_animate_spell_impact_per_target(
        caster_idx, effect_param, target_count, p_target_array);
    fd2_animate_spell_full_screen_flash(
        caster_idx, effect_param, target_count, p_target_array);

    for (i = 0; (int)i < (int)target_count; i++) {
        target_id = *((uint8 *)p_target_array + i);
        dmg = fd2_calc_magic_damage((uint32)target_id, effect_param);
        if (dmg != 0) {
            fd2_show_damage_number((uint32)dmg, 0x5E,
                                    (uint32)target_id);
        } else {
            fd2_show_miss_indicator((uint32)target_id);
        }
    }
    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}

/* ----------------------------------------------------------------
 * fd2_execute_offensive_full_screen_flash_spell @ 0x213B7 (1 caller)
 *
 * Generic offensive spell worker using the FULL-SCREEN-FLASH
 * animation variant (spell ids 4-7). Byte-for-byte the same algorithm
 * as fd2_execute_offensive_targeted_spell @ 0x21227, the only
 * difference being the second animation call: overlay_blink there vs
 * full_screen_flash here. Resets the AoE/fx-queue counter, plays the
 * per-target impact + full-screen-flash animations, deducts the
 * caster's MP for spell_id, then applies magic damage to every target
 * in the byte array: a miss (damage 0) shows the miss indicator,
 * otherwise the damage number is drawn with glyph 0x5E ('^').
 *
 * Pattern-A SHARED EPILOGUE: the loop-exit JGE 0x2141E falls into
 * fd2_composite_then_animate_projectiles @ 0x21190, whose body is
 * fd2_composite_battle_frame(0) then fd2_animate_spell_projectile_
 * paths() (inlined here; the wrapper is a shared-epilogue fragment,
 * not a standalone C function). The function has no explicit RET of
 * its own -- it borrows 0x21190's POP/RET epilogue.
 *
 * damage is the per-target return of fd2_calc_magic_damage: asm
 * 0x2142F CALL leaves it in EAX, 0x21434 ADD ESP,8 / 0x21412 PUSH EAX
 * forward it straight into fd2_show_damage_number (no EAX clobber
 * between TEST and PUSH), so the inner return IS the displayed number.
 *
 * Sole caller: fd2_spell_handler_id_4_via_full_screen_flash @ 0x21396
 * (spell_id literal 4); ids 5/6/7 reach it via that handler's
 * shared-tail. Sibling worker: fd2_execute_offensive_targeted_spell @
 * 0x21227 (identical algo with full_screen_flash -> overlay_blink).
 * ---------------------------------------------------------------- */
void fd2_execute_offensive_full_screen_flash_spell(
    int caster, int spell_id, int n_targets, int p_targets)
{
    int iter;
    uint8 target_id;
    uint32 damage;

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;

    fd2_animate_spell_impact_per_target(
        (uint32)caster, (uint32)spell_id,
        (uint32)n_targets, (uint32)p_targets);
    fd2_animate_spell_full_screen_flash(
        (uint32)caster, (uint32)spell_id,
        (uint32)n_targets, (uint32)p_targets);
    fd2_deduct_caster_mp((uint32)caster, (uint32)spell_id);

    for (iter = 0; iter < n_targets; iter++) {
        target_id = *((uint8 *)p_targets + iter);
        damage = (uint32)fd2_calc_magic_damage(
                     (uint32)target_id, (uint32)spell_id);
        if (damage != 0) {
            fd2_show_damage_number(damage, 0x5E, (uint32)target_id);
        } else {
            fd2_show_miss_indicator((uint32)target_id);
        }
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}

/* ----------------------------------------------------------------
 * fd2_apply_status_effect_with_anim @ 0x22AA8
 *
 * Wrapper: reset aoe count, deduct MP, then delegate to
 * fd2_cast_status_cure_spell @ 0x22AF6. If any targets affected,
 * tail-calls fd2_animate_spell_projectile_paths.
 * ---------------------------------------------------------------- */
void fd2_apply_status_effect_with_anim(int caster_idx,
    int status_spell_id, int target_count,
    int p_target_array, int status_byte_offset)
{
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster_idx, status_spell_id);
    fd2_cast_status_cure_spell(caster_idx, status_spell_id,
        target_count, p_target_array, status_byte_offset);
    if (data_fd2_battle_spell_aoe_count_and_fx_queue_idx != 0) {
        fd2_animate_spell_projectile_paths();
    }
}

/* ----------------------------------------------------------------
 * fd2_cast_status_cure_spell @ 0x22AF6  (2 callers)
 *
 * STATUS-CURE spell worker (Antidote / De-Paralyze family). Plays the
 * per-target impact + status-overlay-flicker animations, then for each
 * target in the byte array checks the status byte at the raw runtime_char
 * byte offset `sprite_id` (item effect 6 / antidote passes 0x25 = the
 * poison byte at pStatus_flags_block[4]; effect 7 / de-paralyze passes
 * 0x26 = bStatus_sleep_flag; sprite_id is a struct byte offset, not a
 * fixed status enum): if the byte is 0 the unit has no such status ->
 * draw the miss indicator; otherwise heal +10 HP via
 * fd2_apply_hp_heal_and_award_xp, draw the heal
 * number (glyph 0x69 = 'i'), clear the status byte, and credit level_mod*4
 * pending XP (cure XP is 4x level_mod vs 2x for the AP/DP/speed buffs).
 * Closes with fd2_composite_battle_frame(0) + its own POP/RET epilogue.
 *
 * level_mod = target.status_flags_block[0] (the unit's level byte), +30 if
 * its job_id is an intermediate class (9..0x18). It is computed for every
 * target (asm 0x22B64..0x22B76, before the status-byte test) but only used
 * on the cure path; the pre-test computation is preserved here for fidelity.
 *
 * The status byte is addressed as a raw offset from the runtime_char base
 * (asm 0x22B79 MOV ESI,[sprite_id] / 0x22B7D ADD ESI,&rc[tid] /
 * 0x22B7F MOVZX [ESI]); the Ghidra decompiler renders it through its own
 * struct as pSprite_state[sprite_id-2], which is the same byte. Emitted
 * here as a byte pointer arithmetic from &runtime_char[target_id].
 *
 * heal is the per-target return of fd2_apply_hp_heal_and_award_xp: asm
 * 0x22B92 CALL leaves it in EAX, and only 0x22B9A MOVZX EBX intervenes
 * (writes EBX, not EAX) before 0x22BA0 PUSH EAX, so the inner return is
 * forwarded straight into fd2_show_damage_number as the displayed number.
 *
 * Callers: fd2_apply_status_effect_with_anim @ 0x22AA8 (delegates after MP
 * deduct, spells 0x14/0x15), and fd2_apply_use_effect_dispatch @ 0x20C6F
 * (item effects 6 / antidote and 7 / de-paralyze).
 * ---------------------------------------------------------------- */
void fd2_cast_status_cure_spell(uint32 caster, uint32 spell_id,
                                uint32 n_targets, uint32 p_targets,
                                uint32 sprite_id)
{
    int iter;
    uint8 target_id;
    runtime_char *target_rc;
    uint8 *status_byte;
    uint32 level_mod;
    uint32 heal;

    fd2_animate_spell_impact_per_target(
        caster, spell_id, n_targets, p_targets);
    fd2_animate_status_effect_overlay_flicker(
        caster, spell_id, n_targets, p_targets);

    for (iter = 0; iter < (int)n_targets; iter++) {
        target_id = ((uint8 *)p_targets)[iter];
        target_rc = &data_fd2_battle_runtime_char_array_ptr[
                        (uint32)target_id];
        level_mod = (uint32)target_rc->status_flags_block[0];
        if (target_rc->job_id > 8 && target_rc->job_id < 0x19) {
            level_mod = level_mod + 0x1e;
        }
        status_byte = (uint8 *)target_rc + sprite_id;
        if (*status_byte == 0) {
            fd2_show_miss_indicator((uint32)target_id);
        } else {
            heal = (uint32)fd2_apply_hp_heal_and_award_xp(
                       (uint32)target_id, 10);
            fd2_show_damage_number(heal, 0x69, (uint32)target_id);
            *status_byte = 0;
            data_fd2_battle_pending_xp_credit =
                data_fd2_battle_pending_xp_credit + level_mod * 4;
        }
    }

    fd2_composite_battle_frame(0);
}

/* ----------------------------------------------------------------
 * fd2_cast_spell_17_teleport @ 0x2218A  (2 callers)
 *
 * Teleport spell: job-based XP + dual-position warp animation.
 * ---------------------------------------------------------------- */
void fd2_cast_spell_17_teleport(uint32 caster, uint32 target_count,
                               uint32 p_target_byte)
{
    uint8 target_id;
    runtime_char *target_rc;
    uint32 base_dmg;

    target_id = *(uint8 *)p_target_byte;
    fd2_pan_cursor_to_char((uint32)target_id);
    fd2_deduct_caster_mp(caster, 0x17);

    target_rc = &data_fd2_battle_runtime_char_array_ptr[target_id];
    base_dmg = (uint32)target_rc->status_flags_block[0];
    if (target_rc->job_id > 8 && target_rc->job_id < 0x19) {
        base_dmg = base_dmg + 0x1e;
    }
    data_fd2_battle_pending_xp_credit =
        data_fd2_battle_pending_xp_credit + base_dmg * 10;

    fd2_animate_warp_teleport_char(
        (uint32)target_id, 0xff, 0xff,
        (uint32)target_rc->pos_x, (uint32)target_rc->pos_y);

    data_fd2_battle_anim_phase = 0;

    fd2_pan_cursor_to_tile_animated(
        (int)data_fd2_battle_teleport_dest_world_x,
        (int)data_fd2_battle_teleport_dest_world_y);

    fd2_animate_warp_teleport_char(
        (uint32)target_id,
        data_fd2_battle_teleport_dest_world_x,
        data_fd2_battle_teleport_dest_world_y,
        data_fd2_battle_teleport_dest_world_x,
        data_fd2_battle_teleport_dest_world_y);

    data_fd2_battle_anim_phase = 1;
}

/* ----------------------------------------------------------------
 * fd2_cast_group_hp_heal_spell @ 0x211A4  (2 callers)
 *
 * Group HP-heal spell effect (Cure / Heal-line, multi-target).
 * Plays the heal sparkles (sprite id 0x0D) impact + overlay flicker,
 * then heals every target in the byte array (HP delta + XP credit via
 * fd2_apply_hp_heal_and_award_xp) and shows the heal indicator number
 * (0x69 = 'i') over each. Ends with a Pattern-A SHARED EPILOGUE: the
 * loop-exit JMP 0x21190 falls into fd2_composite_then_animate_
 * projectiles, whose body is fd2_composite_battle_frame(0) then
 * fd2_animate_spell_projectile_paths() (inlined here; the wrapper is a
 * shared-epilogue fragment, not a standalone C function).
 *
 * heal_amount is the per-target return of fd2_apply_hp_heal_and_award_
 * xp: asm 0x211E7 CALL leaves it in EAX, 0x211EC ADD ESP,8 / 0x211F6
 * PUSH EAX forward it straight into fd2_show_damage_number (no EAX
 * clobber between), so the inner return IS the displayed number.
 * ---------------------------------------------------------------- */
void fd2_cast_group_hp_heal_spell(uint32 caster_idx, uint32 n_targets,
                                   uint32 targets, uint32 heal_base)
{
    uint32 iter;
    uint8 target_id;
    uint32 heal_amount;

    fd2_animate_spell_impact_per_target(
        caster_idx, 0x0D, n_targets, targets);
    fd2_animate_status_effect_overlay_flicker(
        caster_idx, 0x0D, n_targets, targets);

    for (iter = 0; (int)iter < (int)n_targets; iter++) {
        target_id = *((uint8 *)targets + iter);
        heal_amount = (uint32)fd2_apply_hp_heal_and_award_xp(
                          (uint32)target_id, heal_base);
        fd2_show_damage_number(heal_amount, 0x69, (uint32)target_id);
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}

/* ----------------------------------------------------------------
 * fd2_execute_offensive_targeted_spell @ 0x21227  (1 caller)
 *
 * Generic offensive targeted-spell worker using the BLINK-OVERLAY
 * animation variant. Resets the AoE/fx-queue counter, plays the
 * per-target impact + overlay-blink animations, deducts the caster's
 * MP for spell_id, then applies magic damage to every target in the
 * byte array: a miss (damage 0) shows the miss indicator, otherwise
 * the damage number is drawn with glyph 0x5E ('^').
 *
 * Pattern-A SHARED EPILOGUE: the loop-exit JGE 0x2128E falls into
 * fd2_composite_then_animate_projectiles @ 0x21190, whose body is
 * fd2_composite_battle_frame(0) then fd2_animate_spell_projectile_
 * paths() (inlined here; the wrapper is a shared-epilogue fragment,
 * not a standalone C function). The function has no explicit RET of
 * its own — it borrows 0x21190's POP/RET epilogue.
 *
 * damage is the per-target return of fd2_calc_magic_damage: asm
 * 0x2129F CALL leaves it in EAX, 0x212A4 ADD ESP,8 / 0x21282 PUSH EAX
 * forward it straight into fd2_show_damage_number (no EAX clobber
 * between TEST and PUSH), so the inner return IS the displayed number.
 *
 * Sibling worker: fd2_execute_offensive_full_screen_flash_spell @
 * 0x213B7 (identical algo with overlay_blink -> full_screen_flash).
 * ---------------------------------------------------------------- */
void fd2_execute_offensive_targeted_spell(int caster, int spell_id,
                                           int n_targets, int p_targets)
{
    int iter;
    uint8 target_id;
    uint32 damage;

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;

    fd2_animate_spell_impact_per_target(
        (uint32)caster, (uint32)spell_id,
        (uint32)n_targets, (uint32)p_targets);
    fd2_animate_spell_overlay_blink(
        (uint32)caster, (uint32)spell_id,
        (uint32)n_targets, (uint32)p_targets);
    fd2_deduct_caster_mp((uint32)caster, (uint32)spell_id);

    for (iter = 0; iter < n_targets; iter++) {
        target_id = *((uint8 *)p_targets + iter);
        damage = (uint32)fd2_calc_magic_damage(
                     (uint32)target_id, (uint32)spell_id);
        if (damage != 0) {
            fd2_show_damage_number(damage, 0x5E, (uint32)target_id);
        } else {
            fd2_show_miss_indicator((uint32)target_id);
        }
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}

/* ----------------------------------------------------------------
 * fd2_execute_offensive_single_target_spell_id_9 @ 0x214AD (0 callers)
 *
 * Reached only through the spell dispatch table @ 0x51D01 (entry index
 * 9 = 0x51D01 + 0x24); no direct callers. spell_id literal 9 is baked
 * into the body. Dedicated SINGLE-TARGET offensive worker for spell 9
 * (咒殺術, an attack spell whose AoE radius is 0, so by design it can
 * only strike one unit). Unlike the looping siblings (0x21227 / 0x213B7)
 * it hits only target_id_array[0], has no per-target loop, plays NO
 * second (blink/flash) animation, and ends with its own explicit RET
 * instead of borrowing the 0x21190 shared epilogue.
 *
 * Resets the AoE/fx-queue counter, plays the per-target impact
 * animation (the 2nd parameter is the selected-target count, forwarded
 * as the impact animation's 3rd arg = n_targets so the sprite covers
 * every selected target even though only target[0] is damaged), deducts
 * the caster's MP for spell 9, then applies magic damage to target[0]:
 * a miss (damage 0) shows the miss indicator, otherwise the damage
 * number is drawn with glyph 0x5E ('^'). Finishes by compositing the
 * battle frame and animating the projectile paths.
 *
 * damage is the return of fd2_calc_magic_damage: asm 0x214ED CALL leaves
 * it in EAX; on the hit path only 0x21507 MOVZX EBX / 0x2150B PUSH 0x5E
 * intervene before 0x2150D PUSH EAX (no EAX clobber between the TEST at
 * 0x214F5 and the PUSH), so the inner return IS the displayed number.
 * ---------------------------------------------------------------- */
void fd2_execute_offensive_single_target_spell_id_9(
    int caster_unit_id, int spell_arg, uint8 *target_id_array)
{
    uint32 damage;

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;

    fd2_animate_spell_impact_per_target(
        (uint32)caster_unit_id, 9, (uint32)spell_arg,
        (uint32)target_id_array);
    fd2_deduct_caster_mp((uint32)caster_unit_id, 9);

    damage = (uint32)fd2_calc_magic_damage(
                 (uint32)*target_id_array, 9);
    if (damage == 0) {
        fd2_show_miss_indicator((uint32)*target_id_array);
    } else {
        fd2_show_damage_number(damage, 0x5E, (uint32)*target_id_array);
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}

/* ----------------------------------------------------------------
 * fd2_execute_offensive_targeted_spell_variant_b @ 0x212B9 (0 callers)
 *
 * DEAD CODE / orphan clone of fd2_execute_offensive_targeted_spell @
 * 0x21227. The two function bodies are byte-identical (all 6 basic-block
 * hashes match); 0x212B9 has no callers and no xrefs, and is NOT in the
 * 24-entry spell dispatch table @ 0x51D01 (only 0x21227 is the live
 * blink-overlay worker). Open Watcom emitted this second physical copy
 * (duplicate translation unit / dead source kept by the linker), so a
 * faithful rebuild must keep both copies present at distinct addresses.
 * The "_variant_b" suffix denotes a second placement, not a semantic
 * variant: identical algorithm, identical operands.
 *
 * Body is identical to fd2_execute_offensive_targeted_spell: reset the
 * AoE/fx-queue counter, play per-target impact + overlay-blink anims,
 * deduct caster MP, then apply magic damage per target (miss indicator
 * when damage 0, else damage number with glyph 0x5E). The loop-exit
 * JGE 0x21320 falls into the Pattern-A SHARED EPILOGUE @ 0x21190
 * (fd2_composite_battle_frame(0) then fd2_animate_spell_projectile_
 * paths(), inlined here) — no explicit RET of its own.
 *
 * damage is the per-target return of fd2_calc_magic_damage: asm
 * 0x21331 CALL leaves it in EAX, 0x21336 ADD ESP,8 / 0x21314 PUSH EAX
 * forward it straight into fd2_show_damage_number (no EAX clobber
 * between TEST and PUSH), so the inner return IS the displayed number.
 * ---------------------------------------------------------------- */
void fd2_execute_offensive_targeted_spell_variant_b(
    int caster, int spell_id, int n_targets, int p_targets)
{
    int iter;
    uint8 target_id;
    uint32 damage;

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;

    fd2_animate_spell_impact_per_target(
        (uint32)caster, (uint32)spell_id,
        (uint32)n_targets, (uint32)p_targets);
    fd2_animate_spell_overlay_blink(
        (uint32)caster, (uint32)spell_id,
        (uint32)n_targets, (uint32)p_targets);
    fd2_deduct_caster_mp((uint32)caster, (uint32)spell_id);

    for (iter = 0; iter < n_targets; iter++) {
        target_id = *((uint8 *)p_targets + iter);
        damage = (uint32)fd2_calc_magic_damage(
                     (uint32)target_id, (uint32)spell_id);
        if (damage != 0) {
            fd2_show_damage_number(damage, 0x5E, (uint32)target_id);
        } else {
            fd2_show_miss_indicator((uint32)target_id);
        }
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}

/* ----------------------------------------------------------------
 * fd2_cast_ap_boost_spell @ 0x22721  (3 callers)
 *
 * AP-boost (Attack-Up) spell/item effect. Plays the per-target impact
 * + status-overlay-flicker animations (sprite/effect id 0x11), then for
 * each target in the byte array applies a one-shot AP buff guarded by a
 * stack-protection timer:
 *   level_mod = target.status_flags_block[0] (the unit's level byte),
 *               +30 if its job_id is an intermediate class (9..0x18);
 *   if status_flags_block[1] (the AP-buff timer) is 0 (not yet boosted):
 *     advance the shared RNG and set the timer to (rng % 4) + 2 turns,
 *     compute delta = (int)(1.0 + ap * 0.15) (Watcom truncates toward
 *     zero), draw it as a heal-style number (glyph 0x69 = 'i'), add it
 *     to the unit's ap word, and credit level_mod*2 pending XP;
 *   otherwise the unit is already boosted -> draw the miss indicator
 *   (no stacking). Closes with the standard composite + projectile
 *   animation pass.
 *
 * EAX-bug correction: at asm 0x227bc CALL fd2_advance_rng_state the EAX
 * return feeds 0x227c1 MOV EDX,EAX / SAR EDX,0x1f / IDIV EBX(=4), so the
 * timer is (rng_return % 4) + 2. The Ghidra decompiler instead printed
 * (int)uVar3 % 4 using the old flag value (==0) — wrong source. The RNG
 * return is the 16-bit seed zero-extended (always 0..0xFFFF, so signed
 * % 4 stays 0..3), giving a 2..5 turn timer.
 *
 * AP-boost factor 0.15 lives at 0x50210
 * (data_fd2_battle_spell_ap_boost_factor_015, IEEE754 0x3FC3333333333333);
 * the FILD/FMUL/FLD1/FADDP/__CHP/FISTP idiom at 0x227d3..0x227ee is the
 * Watcom (int) cast of (1.0 + ap*0.15) (__CHP @ 0x377a4 sets RC=truncate
 * then FRNDINTs).
 *
 * Pattern-A SHARED EPILOGUE: the loop-exit JGE 0x2281B leads into
 * PUSH 0 / fd2_composite_battle_frame / fd2_animate_spell_projectile_
 * paths / JMP 0x1317d (a RET-only POP stub shared across many battle
 * functions); emitted here as the two inlined calls + the implicit C
 * return. Sibling fd2_cast_dp_boost_spell @ 0x22866 (DP variant, id 0x12,
 * timer byte +0x23, dp word, factor 0x50218) tail-shares this same
 * epilogue via its own JGE 0x2281B; it is emitted separately with its
 * own equivalent epilogue (Layer-2 functional equivalence, the JMP
 * sharing is a compiler size optimization not reproduced in source).
 *
 * Callers: fd2_apply_use_effect_dispatch @ 0x20C6F (item effect 0x10),
 * fd2_cast_spell_11_stage_a @ 0x226EA (spell 0x11), and
 * fd2_execute_summon_spell_cast @ 0x27FC9 (summon combo).
 * ---------------------------------------------------------------- */
void fd2_cast_ap_boost_spell(int caster_unit_id, int num_targets,
                             uint8 *target_id_array)
{
    int iter;
    uint8 target_id;
    runtime_char *target_rc;
    uint32 level_mod;

    fd2_animate_spell_impact_per_target(
        (uint32)caster_unit_id, 0x11,
        (uint32)num_targets, (uint32)target_id_array);
    fd2_animate_status_effect_overlay_flicker(
        (uint32)caster_unit_id, 0x11,
        (uint32)num_targets, (uint32)target_id_array);

    for (iter = 0; iter < num_targets; iter++) {
        target_id = target_id_array[iter];
        target_rc = &data_fd2_battle_runtime_char_array_ptr[
                        (uint32)target_id];
        level_mod = (uint32)target_rc->status_flags_block[0];
        if (target_rc->job_id > 8 && target_rc->job_id < 0x19) {
            level_mod = level_mod + 0x1e;
        }
        if (target_rc->status_flags_block[1] == 0) {
            int delta;
            target_rc->status_flags_block[1] =
                (uint8)((int)fd2_advance_rng_state() % 4 + 2);
            delta = (int)(1.0 + (double)target_rc->ap *
                          data_fd2_battle_spell_ap_boost_factor_015);
            fd2_show_damage_number((uint32)delta, 0x69,
                                    (uint32)target_id);
            target_rc->ap = (uint16)(target_rc->ap + (int16)delta);
            data_fd2_battle_pending_xp_credit =
                data_fd2_battle_pending_xp_credit + level_mod * 2;
        } else {
            fd2_show_miss_indicator((uint32)target_id);
        }
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}


/* ----------------------------------------------------------------
 * fd2_cast_dp_boost_spell @ 0x22866  (3 callers)
 *
 * DP-boost (Defense-Up / "Shield") spell/item effect. Byte-for-byte
 * sibling of fd2_cast_ap_boost_spell @ 0x22721: identical loop body but
 * with the shield effect id 0x12, the dp word (asm field +0x4a), and the
 * DP-buff timer status_flags_block[2] (asm field +0x23). Plays the
 * per-target impact + status-overlay-flicker animations (id 0x12), then
 * for each target in the byte array applies a one-shot DP buff guarded by
 * its stack-protection timer:
 *   level_mod = target.status_flags_block[0] (the unit's level byte),
 *               +30 if its job_id is an intermediate class (9..0x18);
 *   if status_flags_block[2] (the DP-buff timer) is 0 (not yet boosted):
 *     advance the shared RNG and set the timer to (rng % 4) + 2 turns,
 *     compute delta = (int)(1.0 + dp * 0.15) (Watcom truncates toward
 *     zero), draw it as a heal-style number (glyph 0x69 = 'i'), add it
 *     to the unit's dp word, and credit level_mod*2 pending XP;
 *   otherwise the unit is already boosted -> draw the miss indicator
 *   (no stacking). Closes with the standard composite + projectile
 *   animation pass.
 *
 * EAX-bug correction: at asm 0x22901 CALL fd2_advance_rng_state the EAX
 * return feeds 0x22906 MOV EDX,EAX / SAR EDX,0x1f / IDIV EBX(=4), so the
 * timer is (rng_return % 4) + 2. The Ghidra decompiler instead printed
 * (int)uVar3 % 4 using the old flag value (==0) -- wrong source. The RNG
 * return is the 16-bit seed zero-extended (always 0..0xFFFF, so signed
 * % 4 stays 0..3), giving a 2..5 turn timer.
 *
 * DP-boost factor 0.15 lives at 0x50218
 * (data_fd2_battle_spell_dp_boost_factor_015, IEEE754 0x3FC3333333333333,
 * the same 0.15 constant as the AP variant but a distinct .rodata copy);
 * the FILD/FMUL/FLD1/FADDP/__CHP/FISTP idiom at 0x22918..0x22933 is the
 * Watcom (int) cast of (1.0 + dp*0.15) (__CHP @ 0x377a4 sets RC=truncate
 * then FRNDINTs).
 *
 * Pattern-B shared epilogue: the loop-exit JGE 0x2281B jumps into the
 * tail of fd2_cast_ap_boost_spell (PUSH 0 / fd2_composite_battle_frame /
 * fd2_animate_spell_projectile_paths / JMP 0x1317d shared POP stub),
 * which both casters share by a compiler size optimization. Re-emitted
 * here as the two inlined calls + the implicit C return (Layer-2
 * functional equivalence; the JMP sharing is not reproduced in source).
 *
 * The third parameter keeps the original uint address contract (Ghidra
 * uint param_3, call sites pass a uint32); the per-target byte index is
 * read as ((uint8 *)target_id_array)[iter], matching the asm
 * *(byte *)(param_3 + iVar4).
 *
 * Callers: fd2_apply_use_effect_dispatch @ 0x20C6F (item effect 0x0F),
 * fd2_cast_spell_12_stage_b @ 0x2282F (spell 0x12), and
 * fd2_execute_summon_spell_cast @ 0x27FC9 (summon combo).
 * ---------------------------------------------------------------- */
void fd2_cast_dp_boost_spell(int caster_unit_id, int num_targets,
                             uint32 target_id_array)
{
    int iter;
    uint8 target_id;
    runtime_char *target_rc;
    uint32 level_mod;

    fd2_animate_spell_impact_per_target(
        (uint32)caster_unit_id, 0x12,
        (uint32)num_targets, target_id_array);
    fd2_animate_status_effect_overlay_flicker(
        (uint32)caster_unit_id, 0x12,
        (uint32)num_targets, target_id_array);

    for (iter = 0; iter < num_targets; iter++) {
        target_id = ((uint8 *)target_id_array)[iter];
        target_rc = &data_fd2_battle_runtime_char_array_ptr[
                        (uint32)target_id];
        level_mod = (uint32)target_rc->status_flags_block[0];
        if (target_rc->job_id > 8 && target_rc->job_id < 0x19) {
            level_mod = level_mod + 0x1e;
        }
        if (target_rc->status_flags_block[2] == 0) {
            int delta;
            target_rc->status_flags_block[2] =
                (uint8)((int)fd2_advance_rng_state() % 4 + 2);
            delta = (int)(1.0 + (double)target_rc->dp *
                          data_fd2_battle_spell_dp_boost_factor_015);
            fd2_show_damage_number((uint32)delta, 0x69,
                                    (uint32)target_id);
            target_rc->dp = (uint16)(target_rc->dp + (int16)delta);
            data_fd2_battle_pending_xp_credit =
                data_fd2_battle_pending_xp_credit + level_mod * 2;
        } else {
            fd2_show_miss_indicator((uint32)target_id);
        }
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}


/* ----------------------------------------------------------------
 * fd2_cast_speed_boost_spell @ 0x22997  (3 callers)
 *
 * Speed-boost ("Haste" / 風行術) spell/item effect. Same family as
 * fd2_cast_ap_boost_spell / fd2_cast_dp_boost_spell but uses the
 * speed effect id 0x13 and the dx-buff timer status_flags_block[3]
 * (asm field +0x24). Plays the per-target impact + status-overlay-
 * flicker animations (id 0x13), then for each target in the byte
 * array applies a one-shot speed buff guarded by its stack-protection
 * timer:
 *   level_mod = target.status_flags_block[0] (the unit's level byte),
 *               +30 if its job_id is an intermediate class (9..0x18);
 *   if status_flags_block[3] (the dx-buff timer) is 0 (not yet
 *   boosted):
 *     advance the shared RNG and set the timer to (rng % 4) + 2 turns,
 *     add a fixed +15 to dx_current (speed) and +15 to stat4_current
 *     (evade), draw the +15 as a heal-style number (glyph 0x69 = 'i'),
 *     and credit level_mod*2 pending XP;
 *   otherwise the unit is already boosted -> draw the miss indicator
 *   (no stacking). Closes with the standard composite + projectile
 *   animation pass.
 *
 * Unlike the AP/DP variants there is no FPU scaling: the buff is a
 * flat +15 to both dx_current and stat4_current (asm 0x22A47/0x22A4C
 * ADD word ptr [...],0xf), and the displayed number is the same
 * constant 0xF.
 *
 * EAX-bug correction: at asm 0x22A30 CALL fd2_advance_rng_state the
 * EAX return feeds 0x22A35 MOV EDX,EAX / SAR EDX,0x1f / IDIV EBX(=4),
 * so the timer is (rng_return % 4) + 2. The Ghidra decompiler instead
 * printed (longlong)iVar4 % 4 using the __CHK probe return -- wrong
 * source. The RNG return is the 16-bit seed zero-extended (always
 * 0..0xFFFF, so signed % 4 stays 0..3), giving a 2..5 turn timer.
 *
 * Pattern-A shared epilogue: the loop-exit JGE 0x22A71 leads into
 * PUSH 0 / fd2_composite_battle_frame / fd2_animate_spell_projectile_
 * paths / JMP 0x22BBE (a shared POP/RET stub also reached by sibling
 * casters); emitted here as the two inlined calls + the implicit C
 * return (Layer-2 functional equivalence; the JMP sharing is a
 * compiler size optimization not reproduced in source). The
 * status-inflict spell fd2_cast_status_inflict_spell @ 0x22D1B
 * additionally JMPs into this body's tail (0x22A7B) to share the
 * projectile pass; it is emitted as its own separate function.
 *
 * The third parameter keeps the original uint address contract
 * (Ghidra uint param_3, call sites pass a uint32); the per-target
 * byte index is read as ((uint8 *)target_id_array)[iter], matching
 * the asm *(byte *)(param_3 + iVar5).
 *
 * Callers: fd2_apply_use_effect_dispatch @ 0x20C6F (item effect 0x0C),
 * fd2_cast_spell_13_stage_c @ 0x22960 (spell 0x13), and
 * fd2_execute_summon_spell_cast @ 0x27FC9 (summon combo).
 * ---------------------------------------------------------------- */
void fd2_cast_speed_boost_spell(uint32 caster_unit_id, uint32 num_targets,
                                uint32 target_id_array)
{
    int iter;
    uint8 target_id;
    runtime_char *target_rc;
    uint32 level_mod;

    fd2_animate_spell_impact_per_target(
        caster_unit_id, 0x13, num_targets, target_id_array);
    fd2_animate_status_effect_overlay_flicker(
        caster_unit_id, 0x13, num_targets, target_id_array);

    for (iter = 0; iter < (int)num_targets; iter++) {
        target_id = ((uint8 *)target_id_array)[iter];
        target_rc = &data_fd2_battle_runtime_char_array_ptr[
                        (uint32)target_id];
        level_mod = (uint32)target_rc->status_flags_block[0];
        if (target_rc->job_id > 8 && target_rc->job_id < 0x19) {
            level_mod = level_mod + 0x1e;
        }
        if (target_rc->status_flags_block[3] == 0) {
            target_rc->status_flags_block[3] =
                (uint8)((int)fd2_advance_rng_state() % 4 + 2);
            target_rc->dx_current = (uint16)(target_rc->dx_current + 0xf);
            target_rc->stat4_current =
                (uint16)(target_rc->stat4_current + 0xf);
            fd2_show_damage_number(0xf, 0x69, (uint32)target_id);
            data_fd2_battle_pending_xp_credit =
                data_fd2_battle_pending_xp_credit + level_mod * 2;
        } else {
            fd2_show_miss_indicator((uint32)target_id);
        }
    }

    fd2_composite_battle_frame(0);
    fd2_animate_spell_projectile_paths();
}


/* ----------------------------------------------------------------
 * fd2_execute_status_clear_holy_word_spell_id_25 @ 0x22C04  (0 callers;
 * dispatched via the spell table @ 0x51D01, entry index 0x19 = 25,
 * data xref at 0x51D65)
 *
 * Worker for spell id 0x19 (25) = 行動術 ("act again"): lets a unit that has
 * already acted this turn act again. The runtime_char "acted" state is
 * flags bit-7 (flags & 0x80); the spell hits a unit only if it has acted,
 * and grants the re-action by clearing that bit. Resets the AoE/fx queue
 * index, deducts the caster's MP for spell 0x19, then plays the per-target
 * impact + status-overlay-flicker animations (id 0x19). For each target in
 * the byte array:
 *   if flags bit-7 is NOT set -> the unit has not acted yet, nothing to
 *   re-enable, draw the miss indicator;
 *   otherwise clear bit-7 (flags &= 0x7F) so the unit may act again, take
 *   status_value = status_flags_block[0] (the unit's level byte), add +30
 *   if its job_id is an intermediate class (9..0x18), and credit
 *   status_value*8 pending XP (the 8x multiplier is the highest reward tier,
 *   shared with the status-inflict worker; vs 4x cure / 2x buff). Closes
 *   with fd2_composite_battle_frame(0) followed by a conditional
 *   fd2_animate_spell_projectile_paths() when the AoE/fx queue index is
 *   non-zero.
 *
 * Naming note: the current symbol calls this "status_clear_holy_word", which
 * is a misnomer -- 0x19 is 行動術 (re-activate), not the heal spell 神恩術;
 * the bit it clears is specifically the "acted" bit. Pending Stage-2 rename
 * to fd2_execute_reactivate_spell_id_25.
 *
 * The third parameter is a byte array of target unit ids (Ghidra
 * byte *target_id_array); each entry is read as target_id_array[iter],
 * matching the asm MOVZX from *(byte *)(ESI + iter).
 * ---------------------------------------------------------------- */
void fd2_execute_status_clear_holy_word_spell_id_25(int caster_unit_id,
    int num_targets, uint8 *target_id_array)
{
    int iter;
    uint8 target_id;
    runtime_char *target_rc;
    uint32 status_value;

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp((uint32)caster_unit_id, 0x19);
    fd2_animate_spell_impact_per_target(
        (uint32)caster_unit_id, 0x19,
        (uint32)num_targets, (uint32)target_id_array);
    fd2_animate_status_effect_overlay_flicker(
        (uint32)caster_unit_id, 0x19,
        (uint32)num_targets, (uint32)target_id_array);

    for (iter = 0; iter < num_targets; iter++) {
        target_id = target_id_array[iter];
        target_rc = &data_fd2_battle_runtime_char_array_ptr[
                        (uint32)target_id];
        if ((target_rc->flags & 0x80) == 0) {
            fd2_show_miss_indicator((uint32)target_id);
        } else {
            target_rc->flags = (uint8)(target_rc->flags & 0x7f);
            status_value = (uint32)target_rc->status_flags_block[0];
            if (target_rc->job_id > 8 && target_rc->job_id < 0x19) {
                status_value = status_value + 0x1e;
            }
            data_fd2_battle_pending_xp_credit =
                data_fd2_battle_pending_xp_credit + status_value * 8;
        }
    }

    fd2_composite_battle_frame(0);
    if (data_fd2_battle_spell_aoe_count_and_fx_queue_idx != 0) {
        fd2_animate_spell_projectile_paths();
    }
}


/* ----------------------------------------------------------------
 * fd2_cast_status_spell_via_d1b @ 0x22CDA  (1 caller)
 *
 * Thin wrapper for status-effect (inflict) spells. Resets the AoE/fx
 * queue index, deducts the caster's MP for the spell, then delegates to
 * the worker fd2_cast_status_inflict_spell @ 0x22D1B and returns. Unlike
 * the sister wrapper fd2_apply_status_effect_with_anim @ 0x22AA8 there is
 * no post-delegate animate tail here -- the worker itself handles the
 * projectile pass (its body tail at 0x22A7B re-enters the shared pass).
 *
 * The 4th param is the target-id byte-array pointer and the 5th is the
 * status sprite/byte offset; both are forwarded verbatim to the worker.
 * Ghidra mislabels the 4th formal as caster_idx; it is the target array
 * pointer per the sole caller (asm 0x22CDA..0x22D1A, RET).
 *
 * Sole caller: fd2_cast_spell_16_dispatch_cda @ 0x22BE1 (spell id 0x16);
 * the sibling thunks for ids 0x1A/0x1B also reach it through that caller.
 * ---------------------------------------------------------------- */
void fd2_cast_status_spell_via_d1b(int caster_idx,
    int status_spell_id, int target_count,
    int p_target_array, int status_byte_offset)
{
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster_idx, status_spell_id);
    fd2_cast_status_inflict_spell(caster_idx, status_spell_id,
        target_count, p_target_array, status_byte_offset);
}


/* ----------------------------------------------------------------
 * fd2_cast_status_inflict_spell @ 0x22D1B  (3 callers)
 *
 * STATUS-INFLICT spell worker (sleep / silence / freeze / paralyze on
 * opponents). Plays the per-target impact + status-overlay-flicker
 * animations, then for each target in the byte array attempts to inflict
 * a status: the affliction lands only if the target does not already
 * carry that status (the status byte at struct offset sprite_id is 0),
 * its job_id is not a boss/immune class (0x19 or 0x1A), and a ~50% RNG
 * roll succeeds. On a hit it deals a flat 10-HP bonus damage (drawn with
 * glyph 0x5E = '^'), sets the status byte to a (rng%4)+2 turn timer, and
 * credits status_flags_block[0]*8 pending XP (the 8x multiplier is the
 * highest reward tier). On any miss it draws the miss indicator. Closes
 * with fd2_composite_battle_frame(0) followed by a conditional
 * fd2_animate_spell_projectile_paths() when the AoE/fx queue index is
 * non-zero.
 *
 * The status byte is addressed by the raw struct byte offset sprite_id
 * (callers pass 0x26 = status_sleep_flag for spell 0x1B / 麻痺術, and
 * 0x27 = combat_aux_block[0] for spell 0x16 / 封咒術), so it is read and
 * written as ((uint8 *)target_rc)[sprite_id], matching the asm
 * byte ptr [target_rc + sprite_id] (Ghidra prints this as the artificial
 * sprite_state[sprite_id-2]; sprite_state sits at +0x02 so that index
 * resolves to the same +sprite_id byte).
 *
 * The 4th param is the target-id byte-array pointer (Ghidra mislabels it
 * caster_idx); each entry is read as ((uint8 *)target_id_array)[iter],
 * matching the asm MOVZX from *(byte *)(p_targets + iter).
 *
 * EAX-bug corrections (two RNG sites): at asm 0x22DBA CALL
 * fd2_advance_rng_state the EAX return feeds 0x22DBF MOV EDX,EAX /
 * SAR EDX,0x1F / IDIV EBX(=100), so the success roll is
 * (rng_return % 100); and at 0x22DED the EAX return feeds the same
 * idiom with IDIV EBX(=4) so the timer is (rng_return % 4) + 2. The
 * Ghidra decompiler instead reused uVar4 (the job_id, then the
 * fd2_apply_damage_and_award_xp return) for both modulos -- wrong source.
 * The RNG return is the 16-bit seed zero-extended (always 0..0xFFFF), so
 * the signed modulos stay non-negative.
 *
 * Pattern-A shared epilogue / cross-fn body sharing: after the loop the
 * asm runs fd2_composite_battle_frame(0) then, when the AoE/fx queue
 * index is non-zero, JMPs (0x22E3C) into the body of
 * fd2_cast_speed_boost_spell @ 0x22997 at 0x22A7B to reuse its
 * fd2_animate_spell_projectile_paths() call + the shared POP/RET stub at
 * 0x22BBE; the AoE==0 case JMPs straight to 0x22BBE. Emitted here as the
 * two inlined calls under the explicit AoE guard plus the implicit C
 * return (Layer-2 functional equivalence; the JMP body-sharing is a
 * compiler size optimization not reproduced in source).
 *
 * Callers: fd2_apply_use_effect_dispatch @ 0x20C6F (item effects 0x0E /
 * 麻痺術 and 0x16 / 封咒術), fd2_cast_status_spell_via_d1b @ 0x22CDA
 * (wrapper delegate), and fd2_execute_summon_spell_cast @ 0x27FC9
 * (summon combo).
 * ---------------------------------------------------------------- */
void fd2_cast_status_inflict_spell(uint32 caster_unit_id, uint32 spell_id,
                                   uint32 num_targets, uint32 target_id_array,
                                   uint32 sprite_id)
{
    int iter;
    uint8 target_id;
    runtime_char *target_rc;
    int damage;

    fd2_animate_spell_impact_per_target(
        caster_unit_id, spell_id, num_targets, target_id_array);
    fd2_animate_status_effect_overlay_flicker(
        caster_unit_id, spell_id, num_targets, target_id_array);

    for (iter = 0; iter < (int)num_targets; iter++) {
        target_id = ((uint8 *)target_id_array)[iter];
        target_rc = &data_fd2_battle_runtime_char_array_ptr[
                        (uint32)target_id];
        if (((uint8 *)target_rc)[sprite_id] == 0 &&
            target_rc->job_id != 0x19 && target_rc->job_id != 0x1a &&
            (int)fd2_advance_rng_state() % 100 < 0x32) {
            damage = fd2_apply_damage_and_award_xp((uint32)target_id, 10);
            fd2_show_damage_number((uint32)damage, 0x5e, (uint32)target_id);
            ((uint8 *)target_rc)[sprite_id] =
                (uint8)((int)fd2_advance_rng_state() % 4 + 2);
            data_fd2_battle_pending_xp_credit =
                data_fd2_battle_pending_xp_credit +
                target_rc->status_flags_block[0] * 8;
        } else {
            fd2_show_miss_indicator((uint32)target_id);
        }
    }

    fd2_composite_battle_frame(0);
    if (data_fd2_battle_spell_aoe_count_and_fx_queue_idx != 0) {
        fd2_animate_spell_projectile_paths();
    }
}

/* ----------------------------------------------------------------
 * data_fd2_battle_spell_aoe_count_and_fx_queue_idx @ 0x53EC4  (4 bytes)
 *
 * Dual-purpose 32-bit battle-FX state cell, shared by the spell/item
 * effect, animation and summon code paths.
 *
 *   1) Per-cast AOE hit counter: the use-effect dispatcher resets it to 0
 *      at the start of a cast, and the cure/inflict helpers test it (!= 0)
 *      before replaying projectile-path animation.
 *   2) FX-queue write cursor: the damage-number and miss-indicator
 *      routines load it as a base index/offset (MOV EAX,[0x53EC4]) into the
 *      parallel FX queue byte arrays at 0x53C6C / 0x53D34 / 0x53DFC, write
 *      a 4-entry batch (some sprite-id slots may be 0 for blank digits),
 *      then advance it (ADD dword [0x53EC4],4).
 *
 * Accessed exclusively as a full 32-bit cell: written via
 * MOV dword ptr [0x53EC4],0 and ADD dword ptr [0x53EC4],4; read via
 * MOV EAX,[0x53EC4]. Used unsigned (a non-negative running index/count).
 * Zero-initialized at load; first touch on every path is a write (the
 * dispatcher's reset), so it carries no static non-zero seed.
 *
 * Writers: fd2_apply_use_effect_dispatch, fd2_show_damage_number,
 * fd2_show_miss_indicator, fd2_execute_summon_spell_cast and the
 * earthquake / variant / staged / status-clear casts. Readers: the same
 * plus fd2_animate_spell_projectile_paths and fd2_cast_status_inflict_spell.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_spell_aoe_count_and_fx_queue_idx;

/* ----------------------------------------------------------------
 * data_fd2_battle_pending_xp_credit @ 0x53EC8  (4 bytes)
 *
 * Battle-wide pending experience accumulator. Damage/heal/status appliers
 * add the XP earned by an action into this cell; the level-up handler
 * (fd2_process_xp_and_level_up_for_char) later drains it.
 *
 *   - Writers accumulate: e.g. fd2_apply_damage_and_award_xp does
 *     ADD dword ptr [0x53EC8],EAX with the kill/damage XP value; the
 *     spell/item casts in this file add their per-action XP.
 *   - The level-up handler gates on it (!= 0), adds it (plus the carried
 *     remainder) to drive the level-up loop, mirrors it into
 *     data_fd2_dialog_last_action_value_param for the "gained N XP" dialog,
 *     then resets it to 0.
 *
 * Accessed exclusively as a full 32-bit cell (ADD/MOV dword ptr [0x53EC8]),
 * used unsigned (a non-negative running XP total that can exceed 255 since
 * per-kill XP = enemy_xp * level products are summed). Zero-initialized at
 * load; first touch on every path is a write (an accumulate or the
 * handler's reset), so it carries no static non-zero seed.
 *
 * Writers: fd2_apply_damage_and_award_xp, fd2_apply_hp_heal_and_award_xp,
 * fd2_execute_attack_damage_calculation, fd2_calculate_combat_hit_outcome,
 * the spell/item casts here (cast_status_inflict/cure, ap/dp/speed boost,
 * spell 17, status-clear holy word), and the chapter-event scripted credits.
 * Readers/drainers: fd2_process_xp_and_level_up_for_char.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_pending_xp_credit;
