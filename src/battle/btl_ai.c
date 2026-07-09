/*
 * btl_ai.c — Battle AI: enemy-turn dispatch + attack/item/spell execution
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <dos.h>

/* ----------------------------------------------------------------
 * fd2_npc_turn_phase_team1 @ 0x1D80B
 *
 * AI turn loop for team 1 (NPC allies that act on their own, e.g.
 * kingdom soldiers in joint missions). Called as "Phase C" of the
 * full turn cycle, between the player turn and the enemy turn.
 *
 * Iterates runtime chars [0 .. party_member_count). Skips a char
 * unless: bTeam(+6)==1, (bFlags(+5) & 0x81)==0 (not dead 0x01 /
 * not acted-this-turn 0x80), and paralysis flag(+0x26)==0. Eligible
 * chars run through fd2_enemy_turn_action_dispatcher(i, 1).
 *
 * After each char (whether it acted or not):
 *   - if a post-action consequence index was set (!=0xFF), invoke
 *     the consequence handler from the table (counter-attack /
 *     death / status proc);
 *   - always run the per-chapter post-action handler for the
 *     current chapter (scripted-event probe);
 *   - break the loop if the chapter event / battle-end code became
 *     non-zero.
 *
 * anim_phase is forced to 0 before the loop and again each
 * iteration, and the keyboard buffer is flushed each iteration.
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
 * Enemy-phase entry point per turn. Two-pass AI over runtime chars
 * [0 .. party_member_count). Eligibility filter (both passes): team
 * 0 (TEAM_ENEMY) and (bFlags(+5) & 0x81)==0 (not dead 0x01 / not
 * acted-this-turn 0x80) and paralysis flag(+0x26)==0.
 *
 * Pass 1 -- smart casters first: score offensive spell + item; only
 * if best spell score >= 6 OR best item score >= 6 dispatch the
 * action now. Low-score chars skip and fall through to pass 2.
 * Pass 2 -- everyone else: dispatch unconditionally; chars that
 * already acted in pass 1 are blocked by the 0x80 (acted) bit.
 *
 * Shared per-char postlude (both passes): reset consequence index to
 * 0xFF before acting, then -- if it was set (!=0xFF) -- invoke the
 * consequence handler (counter/death/status proc), always run the
 * per-chapter post-action handler, and return early if the chapter
 * event / battle-end code became non-zero. anim_phase forced to 0
 * and keyboard buffer flushed each iteration (pass 1 only forces
 * anim_phase). Mirrors fd2_npc_turn_phase_team1 (team 1) but adds
 * the caster-priority first pass.
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
 * fd2_execute_ai_physical_attack @ 0x1548E
 *
 * Execute the physical attack chosen by the enemy AI, with full
 * sprite animation. Called when the physical option wins the 3-way
 * contest in fd2_attack_action_dispatch, or from the AI dispatcher
 * (ai_class 11) physical fallback. ctx_flag is the caster's team/side
 * context, forwarded to fd2_ai_walk_to_target_tile.
 *
 * Reads the ai_best_physical_* selection globals (target x/y/idx) set
 * by fd2_ai_score_physical_attack. Walks caster to the target tile,
 * faces it, then branches on game_speed_flag:
 *   speed == 0 -> fd2_play_full_combat_cinematic (pre-rendered).
 *   speed != 0 -> fast path: paint HP bars, play the hit with HP
 *                 drain, and -- if the hit landed and the defender
 *                 can counter -- play one retaliation hit back.
 * Then resolves deaths, processes drops (target =
 * ai_best_physical_target_idx) and XP/level-up. Always returns 1.
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
 * Execute the offensive spell chosen by the enemy AI. Reads the
 * ai_best_spell_* selection globals (id, target x/y, score) set by
 * fd2_ai_score_offensive_spell. Returns 0 (no-op) if the gate
 * ai_best_spell_score < 6, else 1 after casting.
 *
 * ctx_flag selects how the spell's AOE field pSpell[6] is read when
 * gathering targets: ctx_flag == 0 -> pass (pSpell[6] == 0) as the
 * AOE flag; otherwise pass pSpell[6] directly.
 *
 * Cast path split: spell id < 10 (basic offensive spells) with
 * game_speed_flag == 0 -> fd2_play_spell_cast_sequence; otherwise
 * dispatch through data_fd2_battle_spell_handler_table[id] (special
 * spells, or any spell when fast-speed is on), bracketed by the
 * status-effect SFX setup/teardown hooks. Then resolves deaths,
 * processes drops (target = ai_best_physical_target_idx) and clears
 * pending_xp_credit / anim_phase.
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

    fd2_battle_reset_tile_transient_state(data_fd2_battle_tile_map_ptr);
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
        fd2_stop_and_free_status_effect_sfx();
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
 * Execute the item-use action chosen by the enemy AI (called from
 * fd2_attack_action_dispatch when the item score wins). Reads the
 * ai_best_item_* selection globals (slot, target x/y), resolves the
 * item id and its effect entry, then runs the use animation and
 * applies the effect.
 *
 * ctx_flag selects how the item's small-AOE field (pItem[0x11]) is
 * interpreted when gathering targets: ctx_flag == 0 -> AI usage,
 * pass (small_aoe == 0) as the AOE flag; otherwise pass small_aoe
 * directly.
 *
 * Branch on pItem[0x10] (range class): < 0x10 = short-range
 * (fd2_compute_aoe_targets + tile-flash animation); >= 0x10 =
 * long-range projectile (fd2_scan_chars_along_line + caster figani
 * intro, palette fade, projectile-tile interpolation/clamp and an
 * 8-frame burst). Effect is applied via fd2_apply_use_effect_dispatch.
 * Caller discards the result (Ghidra: returns int 0).
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

    fd2_battle_reset_tile_transient_state(data_fd2_battle_tile_map_ptr);
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
        fd2_battle_reset_tile_transient_state(
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
 * Enemy-AI offensive action selector. Scores the 3 candidate
 * categories (physical / offensive spell / item) via the
 * fd2_ai_score_* trio, then executes the highest-scoring one.
 * Strict-max wins; ties are resolved by tie_break (caster
 * pCombat_aux_block[0xD] & 0x40 forces physical) and, for a
 * phys==spell tie with a low spell id, by comparing the spell's
 * base damage against caster wAP - target wDP.
 * Returns 1 if an action was dispatched, 0 if all scores < 6.
 * ctx_flag is passed through unchanged to every score/execute call.
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

/* ----------------------------------------------------------------
 * Battle AI scratch state for offensive ITEM use (zero-initialized
 * BSS scalars). Target-tile members of the ai_best_item_* result
 * group; the score (0x53C33) and slot (0x53C3F) members live in
 * btl_aisc.c. fd2_ai_score_item_use stores the winning candidate
 * tile here; fd2_execute_ai_item_use reads them to drive the AOE /
 * projectile resolution and tile animation (compared signed and
 * clamped to the map bounds).
 * ---------------------------------------------------------------- */

/* 0x53C37: best item-use target tile X coordinate (zero-init BSS scalar).
            Writer @0x15811 stores the winning candidate X as a full dword
            (MOV [0x53C37],EAX) where the source is a byte tile coord
            zero-extended into the dword (MOVZX @0x1583A). fd2_execute_ai_item_use
            reads it many times (@0x150EB/0x1510E/0x15155/0x15281), rewrites it
            in-place during the long-range projectile interpolation, and clamps
            it signed to [0, data_fd2_battle_map_width_tiles-1]. Unsigned tile
            coordinate. First target member of the ai_best_item_* result group
            (X @0x53C37, Y @0x53C3B); analogue of
            data_fd2_battle_ai_best_spell_target_x @0x53C27. */
uint32 data_fd2_battle_ai_best_item_target_x;

/* 0x53C3B: best item-use target tile Y coordinate (zero-init BSS scalar).
            Writer @0x1581A stores the winning candidate Y as a full dword
            (MOV [0x53C3B],EAX) where the source is a byte tile coord
            zero-extended into the dword (MOVZX @0x15841). fd2_execute_ai_item_use
            reads it many times (@0x150E5/0x15108/0x1514F/0x1527B), rewrites it
            in-place during the long-range projectile interpolation, and clamps
            it signed to [0, data_fd2_battle_map_height_tiles-1]. Unsigned tile
            coordinate. Second target member of the ai_best_item_* result group
            (X @0x53C37, Y @0x53C3B); analogue of
            data_fd2_battle_ai_best_spell_target_y @0x53C2B. */
uint32 data_fd2_battle_ai_best_item_target_y;
