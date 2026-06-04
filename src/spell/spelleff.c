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
        fd2_cast_spell_17_complex(
            caster_idx, target_count, p_target_array);
    }

    data_fd2_battle_pending_xp_credit = 0;
    fd2_play_and_free_status_effect_sfx();
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
 * fd2_apply_attack_spell_damage @ 0x2111A
 *
 * Attack spell (effect 0x15): animate impact + full-screen flash,
 * then apply magic damage per target. Shows miss or damage number.
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
 * its own — it borrows 0x21190's POP/RET epilogue.
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
 * fd2_cast_spell_17_complex @ 0x2218A  (2 callers)
 *
 * Teleport spell: job-based XP + dual-position warp animation.
 * ---------------------------------------------------------------- */
void fd2_cast_spell_17_complex(uint32 caster, uint32 spell_arg,
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
 * into the body. Dedicated SINGLE-TARGET offensive worker: unlike the
 * looping siblings (0x21227 / 0x213B7) it hits only target_id_array[0],
 * has no per-target loop, plays NO second (blink/flash) animation, and
 * ends with its own explicit RET instead of borrowing the 0x21190
 * shared epilogue.
 *
 * Resets the AoE/fx-queue counter, plays the per-target impact
 * animation (spell_arg is forwarded as its 3rd arg = n_targets so the
 * sprite covers every selected target even though only target[0] is
 * damaged), deducts the caster's MP for spell 9, then applies magic
 * damage to target[0]: a miss (damage 0) shows the miss indicator,
 * otherwise the damage number is drawn with glyph 0x5E ('^'). Finishes
 * by compositing the battle frame and animating the projectile paths.
 *
 * damage is the return of fd2_calc_magic_damage: asm 0x214ED CALL
 * leaves it in EAX, and on the hit path 0x21513 .. only MOVZX EBX /
 * PUSH 0x5E intervene before 0x2150D PUSH EAX (no EAX clobber between
 * TEST and PUSH), so the inner return IS the displayed number.
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
