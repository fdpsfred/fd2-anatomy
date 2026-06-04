/*
 * unit tests for src/spell/spelleff.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

#define USE_ITEM_ID 10

/* Back-buffer + portrait-sheet backing for the real fd2_animate_spell_impact_
 * per_target, which fd2_apply_attack_spell_damage invokes before its damage
 * loop. The impact body memmoves 0x25680 bytes through
 * data_fd2_large_game_state_buffer_ptr and reads a dword table out of
 * data_fd2_resource_portrait_sheet_ptr, so both must reference real memory. */
#define SPELLEFF_LGS_SPAN 0x26000u
static uint8 g_spelleff_lgs[SPELLEFF_LGS_SPAN];
static uint8 g_spelleff_sheet[2048];

static void setup_impact_buffers(void)
{
    memset(g_spelleff_lgs, 0, sizeof(g_spelleff_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_spelleff_lgs;
    memset(g_spelleff_sheet, 0, sizeof(g_spelleff_sheet));
    data_fd2_resource_portrait_sheet_ptr = (uint32)g_spelleff_sheet;
    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x20;
    data_fd2_battle_view_window_max_x = 0x0D;
    data_fd2_battle_view_window_max_y = 0x08;
}

extern runtime_char g_test_rc_array[8];
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;
extern uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
extern int g_ending_menu_return;
extern int g_slot_selector_return;
extern int g_chapter_transition_return;
extern int g_play_sfx_with_handle_calls;
extern int g_play_sfx_sample_from_bank_calls;
extern int g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int g_blit_indexed_sprite_last_x;
extern int g_blit_indexed_sprite_last_y;
extern int g_composite_call_count;
extern int g_attack_dispatch_return;
extern int g_attack_dispatch_calls;
extern int g_seek_optimal_return;
extern int g_advance_nearest_return;
extern int g_walk_return;
extern int g_score_physical_return;
extern int g_pass_turn_calls;
extern int g_execute_spell_calls;
extern int g_execute_physical_calls;
extern int g_pathfind_return;
extern int g_pathfind_walk_return;
extern int g_pathfind_write_dst;
extern int g_pathfind_dst_x;
extern int g_pathfind_dst_y;
extern int g_pathfind_seq_enable;
extern int g_pathfind_seq[4];
extern int g_pathfind_seq_idx;
extern int g_pathfind_seq_steps;
extern uint8 g_pathfind_step_bytes[8];
extern int g_pathfind_md0_dst_x;
extern int g_pathfind_md0_dst_y;
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;


static void setup_use_effect(uint8 effect_code, uint16 effect_param)
{
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    g_test_rc_array[0].inventory_slots[1] = USE_ITEM_ID;   /* slot 0 -> item */
    data_fd2_battle_item_effect_table[USE_ITEM_ID].use_effect = effect_code;
    data_fd2_battle_item_effect_table[USE_ITEM_ID].use_param_lo =
        (uint8)(effect_param & 0xFF);
    data_fd2_battle_item_effect_table[USE_ITEM_ID].use_param_hi =
        (uint8)((effect_param >> 8) & 0xFF);
    data_fd2_battle_party_member_count = 0;   /* finale drop loop = no-op */
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    /* g_test_rc_array was just zeroed, so slot[7].flag (inventory_slots[14])
     * starts 0x00; the real fd2_remove_inventory_slot_at(caster,inv_slot=0)
     * stamps it 0x80 when it consumes the slot. */
    /* Several effect codes (0x08-0x13, 0x14/0x18, 0x15) dispatch into the real
     * fd2_animate_spell_impact_per_target, which memmoves the back-buffer and
     * reads the portrait sheet, so wire valid memory for those paths. */
    setup_impact_buffers();
}


/* Codes 5/6/7/0x0B must spend the inventory slot exactly once. */

/* Effect 0x05 routes into the REAL fd2_cast_group_hp_heal_spell, whose per-
 * target fd2_apply_hp_heal_and_award_xp divides the XP credit by the target's
 * hp_max (battle.c L82 / asm 0x1c9cc IDIV [ESP]=hp_max) whenever portrait_id <
 * 0x4b. A real heal target always has hp_max > 0, so give target[1] valid HP
 * (the zeroed fixture would otherwise feed hp_max=0 + portrait 0 -> the divide
 * faults). portrait 0x50 (>= 0x4b) also skips the XP block outright. */
static void test_use_effect_code5_consumes(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x05, 50);
    g_test_rc_array[1].hp_current = 50;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].portrait_id = 0x50;
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x80);  /* slot consumed */
}


static void test_use_effect_code6_consumes(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x06, 0);
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x80);  /* slot consumed */
}


static void test_use_effect_code7_consumes(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x07, 0);
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x80);  /* slot consumed */
}


/* Bug-catcher: code 0x0B (回MP consumable) must also consume the slot.
 * target.mp_max = 0 takes the show_miss branch (pure stubs), isolating
 * the post-loop consume decision. A missing consume here -> count 0. */
static void test_use_effect_code0B_consumes(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x0B, 30);
    g_test_rc_array[1].mp_max = 0;
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x80);  /* slot consumed */
}


/* Code 0x14 (attack spell) is NON-consuming: slot must be left intact, so the
 * real fd2_remove_inventory_slot_at is never called and slot[7].flag
 * (inventory_slots[14]) stays 0x00 (the setup zeroed it).
 * target.job_id = 1 keeps the REAL fd2_calc_magic_damage in-bounds. */
static void test_use_effect_code14_no_consume(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x14, 0);
    g_test_rc_array[1].job_id = 1;
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x00);  /* not consumed */
}


/* Code 0x13 (永久+移動力) bumps a stat via the scroll helper but must
 * RESTORE movement_order afterward. The helper does a 16-bit write at
 * field_offset 0x3B; its high byte lands on movement_order (+0x3C), so a
 * stat_delta of 0x200 deliberately spills into movement_order (0xAB->0xAD)
 * and the dispatcher's save/restore must put it back to 0xAB. (Drop the
 * restore line and this asserts 0xAD, failing.) */
static void test_use_effect_code13_restores_movement_order(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x13, 0x200);
    g_test_rc_array[1].job_id = 1;
    g_test_rc_array[1].movement_order = 0xAB;
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[1].movement_order, 0xAB);
}


/* Finale unconditionally clears pending_xp_credit before returning. */
static void test_use_effect_resets_xp_credit(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x14, 0);
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_pending_xp_credit = 999;
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);
}



static void test_spell_17_deducts_mp(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    g_test_rc_array[0].mp_current = 100;
    data_fd2_battle_spell_effect_table[0x17].mp_cost = 15;
    g_test_rc_array[1].pos_x = 5;
    g_test_rc_array[1].pos_y = 5;
    g_test_rc_array[1].job_id = 1;
    g_test_rc_array[1].status_flags_block[0] = 10;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_teleport_dest_world_x = 5;
    data_fd2_battle_teleport_dest_world_y = 5;
    target_id = 1;
    fd2_cast_spell_17_complex(0, 0, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].mp_current, 85);
}


static void test_spell_17_xp_with_job_bonus(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    g_test_rc_array[0].mp_current = 200;
    data_fd2_battle_spell_effect_table[0x17].mp_cost = 10;
    g_test_rc_array[2].pos_x = 3;
    g_test_rc_array[2].pos_y = 3;
    g_test_rc_array[2].job_id = 10;
    g_test_rc_array[2].status_flags_block[0] = 5;
    data_fd2_battle_cursor_world_x = 3;
    data_fd2_battle_cursor_world_y = 3;
    data_fd2_battle_teleport_dest_world_x = 3;
    data_fd2_battle_teleport_dest_world_y = 3;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 2;
    fd2_cast_spell_17_complex(0, 0, (uint32)&target_id);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, (5 + 0x1e) * 10);
}


static void test_spell_17_xp_no_job_bonus(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    g_test_rc_array[0].mp_current = 200;
    data_fd2_battle_spell_effect_table[0x17].mp_cost = 10;
    g_test_rc_array[1].pos_x = 1;
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].job_id = 5;
    g_test_rc_array[1].status_flags_block[0] = 8;
    data_fd2_battle_cursor_world_x = 1;
    data_fd2_battle_cursor_world_y = 1;
    data_fd2_battle_teleport_dest_world_x = 1;
    data_fd2_battle_teleport_dest_world_y = 1;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 1;
    fd2_cast_spell_17_complex(0, 0, (uint32)&target_id);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 8 * 10);
}


static void test_apply_status_effect_deducts_mp(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    g_test_rc_array[0].mp_current = 50;
    data_fd2_battle_spell_effect_table[0x14].mp_cost = 8;
    target_id = 1;
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_apply_status_effect_with_anim(0, 0x14, 1,
        (int)&target_id, 0x25);
    ASSERT_EQ(g_test_rc_array[0].mp_current, 42);
}


/* Correct-callee regression: the @0x22AA8 wrapper's CALL @0x22AE0 targets the
 * REAL fd2_cast_status_cure_spell @0x22AF6 (the heal-status worker), NOT the
 * sister wrapper fd2_cast_status_spell_via_d1b @0x22CDA (which routes to the
 * inflict worker @0x22D1B). Both names are linker-distinct functions, so a
 * dispatch to the wrong one would apply the wrong status-spell logic. Now that
 * the cure worker is emitted for real, observe its side effect instead of a
 * stub counter: a target whose status byte at offset 0x25 (poison) is set must
 * have that byte CLEARED by the cure worker. The inflict-side stub
 * fd2_cast_status_spell_via_d1b would not touch it, so g_cast_status_via_d1b_
 * calls must also stay 0. Target sits at (0,0) (outside the impact/flicker view
 * window -> blit-culled); portrait 0x50 (>= 0x4b) makes the real heal helper
 * skip its own XP block so only the cure path's effect is exercised. */
static void test_apply_status_effect_calls_cure_worker(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].mp_current = 50;
    data_fd2_battle_spell_effect_table[0x14].mp_cost = 8;
    g_test_rc_array[1].hp_current = 50;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].portrait_id = 0x50;
    g_test_rc_array[1].status_flags_block[4] = 1;   /* poison set (offset 0x25) */
    data_fd2_shared_rng_seed = 0;
    target_id = 1;
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    g_cast_status_via_d1b_calls = 0;
    fd2_apply_status_effect_with_anim(0, 0x14, 1,
        (int)&target_id, 0x25);
    ASSERT_EQ(g_test_rc_array[1].status_flags_block[4], 0);  /* cure cleared it */
    ASSERT_EQ(g_cast_status_via_d1b_calls, 0);               /* inflict not hit */
}


static void test_apply_item_stat_modifier(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();   /* anim_idx 0x11 drives the real impact path */
    target_id = 1;
    fd2_apply_item_stat_modifier_with_anim(
        0, 10, 0x48, 0, 1, (uint32)&target_id, 0x11);
    ASSERT_EQ(g_test_rc_array[1].ap, 10);
}


/* fd2_apply_attack_spell_damage @ 0x2111A first plays the impact + flash
 * animations, then runs the per-target damage loop, then a Pattern-A SHARED
 * EPILOGUE (loop-exit JGE 0x21190 falls into fd2_composite_then_animate_
 * projectiles): fd2_composite_battle_frame(0) then
 * fd2_animate_spell_projectile_paths(). The composite calls are the observable
 * state transition — pinned via g_composite_call_count.
 *
 * The real fd2_animate_spell_impact_per_target (spell_id 0 -> 8 frames)
 * composites twice (one at entry, one on finalize); the real
 * fd2_animate_spell_full_screen_flash composites THREE times (its two real
 * fd2_composite_chars_with_spell_effect_overlay calls each compose a tile map,
 * plus its closing fd2_composite_battle_frame finalize); the caller's own
 * epilogue composites once. Total = 6.
 * Two live targets exercise the loop with the REAL fd2_calc_magic_damage
 * (hit_rate=100 -> damage-number branch each iter); job_id=1 + nonzero HP
 * keep the damage formula in-bounds (mirrors testbtl setup). The damage
 * VALUE and the per-iter hit/miss branch are owned by testbtl's magic-damage
 * tests. The targets sit at (0,0), outside the impact/overlay view window, so
 * both the impact animation and the spell-effect overlay window-cull them (no
 * per-target blit); only the tile-map composite count is asserted here. */
static void test_attack_spell_damage_composites_once(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 2;   /* bound overlay/finalizer loops */
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].portrait_id = 0x01;
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].job_id = 1;
    g_test_rc_array[1].portrait_id = 0x01;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 100;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 0;
    target_ids[1] = 1;
    g_composite_call_count = 0;
    fd2_apply_attack_spell_damage(0, 2, (uint32)target_ids, 0);
    ASSERT_EQ(g_composite_call_count, 6);
}


/* Empty target list (count 0): loop body never runs. The impact animation
 * still composites twice (entry + finalize), the flash composites three times
 * (its two real overlay tile-map composites + its finalize), and the shared
 * epilogue composites once -> 6. Guards against the epilogue composite being
 * mistakenly placed inside the loop (which, with 0 targets, would drop the
 * count to 5). */
static void test_attack_spell_damage_zero_targets_still_composites(void)
{
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;   /* bound overlay/finalizer loops */
    g_composite_call_count = 0;
    fd2_apply_attack_spell_damage(0, 0, (uint32)0, 0);
    ASSERT_EQ(g_composite_call_count, 6);
}

/* ---- fd2_cast_speed_boost_spell @ 0x22997 ---- */

/* Single target, not yet speed-buffed: the buff must land. Unlike the AP/DP
 * variants the boost is a FLAT +15 (no FPU scaling) applied to BOTH the speed
 * word dx_current (asm field +0x4c) and the evade word stat4_current (asm
 * field +0x4e): dx 100 -> 115, evade 50 -> 65. The buff timer is the dx slot
 * status_flags_block[3] (asm field +0x24, distinct from the AP slot [1] and DP
 * slot [2]); it must be set from the RNG: seed 0 -> fd2_advance_rng_state
 * returns 0x80A4 (32932), (int)32932 % 4 = 0, +2 -> 2. level byte
 * status_flags_block[0] = 5 with a non-intermediate job (1) gives XP credit
 * 5*2 = 10. The AP [1] and DP [2] timer slots must stay untouched, proving the
 * speed variant writes [3] only. Guards the EAX-bug fix: the timer comes from
 * the RNG return, not the old (==0) __CHK probe value. */
static void test_speed_boost_applies_buff_and_timer(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;   /* bound impact/flicker loops */
    g_test_rc_array[0].dx_current = 100;
    g_test_rc_array[0].stat4_current = 50;
    g_test_rc_array[0].job_id = 1;            /* not 9..0x18 -> no +30 */
    g_test_rc_array[0].status_flags_block[0] = 5;   /* level */
    g_test_rc_array[0].status_flags_block[3] = 0;   /* dx slot: not yet boosted */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_speed_boost_spell(0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].dx_current, 115);            /* +15 speed */
    ASSERT_EQ(g_test_rc_array[0].stat4_current, 65);          /* +15 evade */
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[3], 2);   /* dx slot set */
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[1], 0);   /* AP slot untouched */
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[2], 0);   /* DP slot untouched */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 10);
}

/* Already-speed-buffed target (dx timer [3] != 0): the else branch shows the
 * miss indicator and must NOT stack the buff -- dx_current, stat4_current, the
 * timer, and XP credit all stay put. */
static void test_speed_boost_skips_already_boosted(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].dx_current = 100;
    g_test_rc_array[0].stat4_current = 50;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].status_flags_block[0] = 5;
    g_test_rc_array[0].status_flags_block[3] = 3;   /* dx slot: already boosted */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_speed_boost_spell(0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].dx_current, 100);            /* unchanged */
    ASSERT_EQ(g_test_rc_array[0].stat4_current, 50);          /* unchanged */
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[3], 3);   /* unchanged */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);          /* no credit */
}

/* Intermediate-class job (9..0x18) adds 30 to the level_mod used for XP credit
 * (asm 0x22a1e ADD [ESP],0x1e), and the flat +15/+15 boost still applies.
 * job_id 9 (first intermediate value) + level 5 -> level_mod 35 -> XP 35*2 =
 * 70. dx 80 -> 95, evade 20 -> 35. timer from seed 0 -> 2. */
static void test_speed_boost_intermediate_class_xp_bonus(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].dx_current = 80;
    g_test_rc_array[0].stat4_current = 20;
    g_test_rc_array[0].job_id = 9;            /* intermediate class -> +30 */
    g_test_rc_array[0].status_flags_block[0] = 5;
    g_test_rc_array[0].status_flags_block[3] = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_speed_boost_spell(0, 1, (uint32)&target_id);
    ASSERT_EQ(g_test_rc_array[0].dx_current, 95);
    ASSERT_EQ(g_test_rc_array[0].stat4_current, 35);
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[3], 2);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 70);
}

/* The per-target loop reads ((uint8 *)target_id_array)[iter] as a BYTE (asm
 * 0x229f3 -> the target index comes from the array, not the loop counter), so
 * non-adjacent indices 2 and 5 must both be boosted while a bystander at index
 * 0 stays put. Both targets start un-boosted (dx timer 0) with dx 100/evade
 * 40; target[2] consumes RNG call 1 (seed 0 -> 0x80A4, %4=0 -> timer 2) and
 * target[5] consumes RNG call 2 (-> 0x85C0, %4=0 -> timer 2). Each gets a flat
 * +15 -> dx 115, evade 55. A loop that stopped after one target, or used iter
 * as the char id, would leave index 5 (or index 0) wrong. */
static void test_speed_boost_visits_all_targets(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].dx_current = 200;      /* bystander, must NOT change */
    g_test_rc_array[2].dx_current = 100;
    g_test_rc_array[2].stat4_current = 40;
    g_test_rc_array[2].job_id = 1;
    g_test_rc_array[2].status_flags_block[3] = 0;
    g_test_rc_array[5].dx_current = 100;
    g_test_rc_array[5].stat4_current = 40;
    g_test_rc_array[5].job_id = 1;
    g_test_rc_array[5].status_flags_block[3] = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_cast_speed_boost_spell(0, 2, (uint32)target_ids);
    ASSERT_EQ(g_test_rc_array[0].dx_current, 200);   /* untouched */
    ASSERT_EQ(g_test_rc_array[2].dx_current, 115);
    ASSERT_EQ(g_test_rc_array[2].stat4_current, 55);
    ASSERT_EQ(g_test_rc_array[5].dx_current, 115);
    ASSERT_EQ(g_test_rc_array[5].stat4_current, 55);
    ASSERT_EQ(g_test_rc_array[2].status_flags_block[3], 2);
    ASSERT_EQ(g_test_rc_array[5].status_flags_block[3], 2);
}

/* ---- fd2_cast_status_cure_spell @ 0x22AF6 ---- */

/* Cure path: a target whose status byte at offset sprite_id (0x25 = poison,
 * = status_flags_block[4]) is non-zero gets it CLEARED, is HP-healed via the
 * real fd2_apply_hp_heal_and_award_xp, and credits level_mod*4 pending XP
 * (the cure worker's distinctive 4x, vs the AP/DP/speed buffs' 2x). base_heal
 * is the literal 10 (asm 0x22B8C PUSH 0xa); the heal helper returns
 * (base_heal*9)/10 + ((that %100)*base_heal)/1000 = 9 + 0 = 9 deterministically
 * (the helper's rng call result is discarded for the extra term), so
 * hp_current 50 -> 59. portrait 0x50 (>= 0x4b) makes the heal helper skip its
 * OWN XP block, isolating the cure worker's credit: level 5, non-intermediate
 * job 1 -> level_mod 5 -> XP 5*4 = 20. Target at (0,0) is window-culled by the
 * real impact/flicker animations (no blit). */
static void test_cure_clears_status_heals_and_credits_xp(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].hp_current = 50;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].portrait_id = 0x50;          /* >= 0x4b: heal skips its XP */
    g_test_rc_array[1].job_id = 1;                  /* not 9..0x18 -> no +30 */
    g_test_rc_array[1].status_flags_block[0] = 5;   /* level */
    g_test_rc_array[1].status_flags_block[4] = 1;   /* poison set (offset 0x25) */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 1;
    fd2_cast_status_cure_spell(0, 0x14, 1, (uint32)&target_id, 0x25);
    ASSERT_EQ(g_test_rc_array[1].status_flags_block[4], 0);   /* cleared */
    ASSERT_EQ(g_test_rc_array[1].hp_current, 59);             /* +9 heal */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 20);         /* level_mod*4 */
}

/* Miss path: a target whose status byte at sprite_id is already 0 has no status
 * to cure -> the worker draws the miss indicator and changes NOTHING. HP stays
 * put and no XP is credited. (Drop the `else` guard and this would heal/credit
 * a unit that has no status, failing both asserts.) */
static void test_cure_no_status_shows_miss_no_change(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].hp_current = 50;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].portrait_id = 0x50;
    g_test_rc_array[1].status_flags_block[0] = 5;
    g_test_rc_array[1].status_flags_block[4] = 0;   /* no poison */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 1;
    fd2_cast_status_cure_spell(0, 0x14, 1, (uint32)&target_id, 0x25);
    ASSERT_EQ(g_test_rc_array[1].hp_current, 50);            /* unchanged */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);         /* no credit */
}

/* Intermediate-class job (9..0x18) adds 30 to the cure worker's level_mod (asm
 * 0x22B76 ADD EBP,0x1e), so the XP credit on the cure path becomes
 * (level + 30)*4. job 9, level 5 -> level_mod 35 -> XP 35*4 = 140. portrait
 * 0x50 again isolates the cure credit from the heal helper's own XP. */
static void test_cure_intermediate_class_xp_bonus(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].hp_current = 50;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].portrait_id = 0x50;
    g_test_rc_array[1].job_id = 9;                  /* intermediate -> +30 */
    g_test_rc_array[1].status_flags_block[0] = 5;
    g_test_rc_array[1].status_flags_block[4] = 1;   /* poison set */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 1;
    fd2_cast_status_cure_spell(0, 0x14, 1, (uint32)&target_id, 0x25);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 140);       /* (5+30)*4 */
}

/* sprite_id selects WHICH status byte is checked/cleared, addressed as a raw
 * offset from the runtime_char base (asm 0x22B7D ADD ESI,&rc[tid]; sprite_id
 * 0x26 = status_sleep_flag at +0x26). With sprite_id 0x26, only +0x26 is
 * cleared; a different status byte (+0x25 poison) set on the same unit must be
 * left untouched, proving the offset is parameterized and not hardcoded. */
static void test_cure_sprite_id_selects_correct_byte(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].hp_current = 50;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].portrait_id = 0x50;
    g_test_rc_array[1].job_id = 1;
    g_test_rc_array[1].status_flags_block[0] = 5;
    g_test_rc_array[1].status_flags_block[4] = 7;   /* poison (+0x25): must stay */
    g_test_rc_array[1].status_sleep_flag = 3;       /* sleep  (+0x26): cure target */
    data_fd2_shared_rng_seed = 0;
    target_id = 1;
    fd2_cast_status_cure_spell(0, 0x15, 1, (uint32)&target_id, 0x26);
    ASSERT_EQ(g_test_rc_array[1].status_sleep_flag, 0);          /* +0x26 cleared */
    ASSERT_EQ(g_test_rc_array[1].status_flags_block[4], 7);      /* +0x25 untouched */
}

/* Multi-target: the per-target loop reads ((uint8 *)p_targets)[iter] as a BYTE
 * (asm 0x22B4F MOVZX ESI,[EDI+EBX]), so the char id comes from the array, not
 * the loop counter. Non-adjacent targets 2 and 5 (both poisoned) must both be
 * cured while a poisoned bystander at index 0 (not in the list) stays set. A
 * loop that used iter as the char id, or stopped after one target, would leave
 * index 5 (or 0) wrong. */
static void test_cure_visits_all_targets_by_array_index(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].status_flags_block[4] = 9;   /* bystander, must NOT clear */
    g_test_rc_array[2].hp_current = 50;
    g_test_rc_array[2].hp_max = 200;
    g_test_rc_array[2].portrait_id = 0x50;
    g_test_rc_array[2].status_flags_block[4] = 1;
    g_test_rc_array[5].hp_current = 50;
    g_test_rc_array[5].hp_max = 200;
    g_test_rc_array[5].portrait_id = 0x50;
    g_test_rc_array[5].status_flags_block[4] = 1;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_cast_status_cure_spell(0, 0x14, 2, (uint32)target_ids, 0x25);
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[4], 9);   /* untouched */
    ASSERT_EQ(g_test_rc_array[2].status_flags_block[4], 0);   /* cured */
    ASSERT_EQ(g_test_rc_array[5].status_flags_block[4], 0);   /* cured */
}

void run_spell_spelleff1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: spell/spelleff1\n");
    RUN_TEST(test_spell_17_deducts_mp);
    RUN_TEST(test_spell_17_xp_with_job_bonus);
    RUN_TEST(test_spell_17_xp_no_job_bonus);
    RUN_TEST(test_apply_status_effect_deducts_mp);
    RUN_TEST(test_apply_status_effect_calls_cure_worker);
    RUN_TEST(test_apply_item_stat_modifier);
    RUN_TEST(test_attack_spell_damage_composites_once);
    RUN_TEST(test_attack_spell_damage_zero_targets_still_composites);
    RUN_TEST(test_use_effect_code5_consumes);
    RUN_TEST(test_use_effect_code6_consumes);
    RUN_TEST(test_use_effect_code7_consumes);
    RUN_TEST(test_use_effect_code0B_consumes);
    RUN_TEST(test_use_effect_code14_no_consume);
    RUN_TEST(test_use_effect_code13_restores_movement_order);
    RUN_TEST(test_use_effect_resets_xp_credit);
    RUN_TEST(test_speed_boost_applies_buff_and_timer);
    RUN_TEST(test_speed_boost_skips_already_boosted);
    RUN_TEST(test_speed_boost_intermediate_class_xp_bonus);
    RUN_TEST(test_speed_boost_visits_all_targets);
    RUN_TEST(test_cure_clears_status_heals_and_credits_xp);
    RUN_TEST(test_cure_no_status_shows_miss_no_change);
    RUN_TEST(test_cure_intermediate_class_xp_bonus);
    RUN_TEST(test_cure_sprite_id_selects_correct_byte);
    RUN_TEST(test_cure_visits_all_targets_by_array_index);
    printf("\n");
}
