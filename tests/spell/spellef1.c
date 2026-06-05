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


/* fd2_cast_status_spell_via_d1b @ 0x22CDA is the inflict-side wrapper (spell id
 * 0x16 and the 0x1A/0x1B sibling thunks). Its three observable acts: (1) zero
 * the AoE/fx queue index (asm 0x22CE4 MOV [0x53EC4],0); (2) deduct the caster's
 * MP for the spell via the REAL fd2_deduct_caster_mp (asm 0x22CF6); (3) forward
 * ALL FIVE args verbatim into the REAL inflict worker @0x22D1B (asm
 * 0x22CFE..0x22D12 push caster/spell/n_tgt/p_tgt/sprite then CALL). There is no
 * post-delegate animate tail (the worker owns the projectile pass), so the
 * wrapper just returns.
 *
 * Drives the real worker and pins each forwarded arg via an observable effect:
 *   - AoE index pre-stained 0x99 must end 0 (the reset);
 *   - caster=0 + spell_id=0x16 -> fd2_deduct_caster_mp subtracts
 *     spell_effect_table[0x16].mp_cost(6) from runtime_char[0].mp_current(50)
 *     -> 44; entry [0] is poisoned with 99 so a caster/spell mix-up would drop
 *     mp to 50-99 (underflow) instead, proving BOTH caster and spell forward;
 *   - p_targets -> {unit 3}, n_targets=1, sprite_id=0x27 all forward iff the
 *     real worker afflicts unit 3 at its +0x27 byte: seed 0 -> roll 0x80A4
 *     %100=32 < 50 -> HIT -> combat_aux_block[0] set to timer (0xAEA0%4)+2 = 2,
 *     and XP 5*8=40. A dropped/reordered arg or a missed reset/deduct fails an
 *     assert. */
static void test_status_via_d1b_resets_deducts_and_forwards(void)
{
    uint8 target_arr[1];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;   /* bound impact/flicker loops */
    g_test_rc_array[0].mp_current = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 99;   /* poison entry 0 */
    data_fd2_battle_spell_effect_table[0x16].mp_cost = 6;
    g_test_rc_array[3].hp_current = 50;
    g_test_rc_array[3].hp_max = 200;
    g_test_rc_array[3].portrait_id = 0x00;          /* < 0x44 -> no inner XP */
    g_test_rc_array[3].job_id = 1;                  /* not immune */
    g_test_rc_array[3].status_flags_block[0] = 5;   /* level -> XP 40 */
    g_test_rc_array[3].combat_aux_block[0] = 0;     /* +0x27 must move */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_arr[0] = 3;
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0x99;
    fd2_cast_status_spell_via_d1b(0, 0x16, 1, (int)target_arr, 0x27);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0); /* reset */
    ASSERT_EQ(g_test_rc_array[0].mp_current, 44);                   /* 50 - 6 */
    ASSERT_EQ(g_test_rc_array[3].combat_aux_block[0], 2);  /* forwarded -> hit */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 40);      /* worker ran */
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

/* ---- fd2_execute_status_clear_holy_word_spell_id_25 @ 0x22C04 ---- */

/* Status-clear ("holy word", spell id 0x19) on a single target whose status
 * bit-7 (flags & 0x80) IS set: the worker deducts the caster's MP for spell
 * 0x19, clears ONLY bit-7 (flags &= 0x7f, asm 0x22c7f), and credits
 * status_value*8 pending XP -- the 8x multiplier (asm 0x22c98 SHL EDX,3) that
 * distinguishes status-clear from the 4x cure / 2x buff workers. The level
 * byte status_flags_block[0] = 5 with a non-intermediate job (1, not 9..0x18)
 * gives XP 5*8 = 40. flags starts 0x81 (bit0=dead + bit7) -> must become 0x01,
 * proving the mask preserves the low bits and only bit-7 is cleared. The
 * caster sits at idx 0 (mp_current 100, cost 7 -> 93); the target at (0,0) is
 * window-culled by the real impact/flicker animations (no blit). */
static void test_holy_word_clears_status_and_credits_xp(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;   /* bound impact/flicker loops */
    g_test_rc_array[0].mp_current = 100;
    data_fd2_battle_spell_effect_table[0x19].mp_cost = 7;
    g_test_rc_array[1].flags = 0x81;                /* bit7 status + bit0 dead */
    g_test_rc_array[1].job_id = 1;                  /* not 9..0x18 -> no +30 */
    g_test_rc_array[1].status_flags_block[0] = 5;   /* level */
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 1;
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_execute_status_clear_holy_word_spell_id_25(0, 1, &target_id);
    ASSERT_EQ(g_test_rc_array[1].flags, 0x01);               /* bit7 cleared, bit0 kept */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 40);        /* level*8 */
    ASSERT_EQ(g_test_rc_array[0].mp_current, 93);            /* MP cost 7 deducted */
}

/* Miss path: a target whose status bit-7 is already 0 (flags & 0x80 == 0, asm
 * 0x22c7d JZ) has no status to clear -> the worker draws the miss indicator and
 * changes NOTHING. flags stays put and no XP is credited. (Drop the bit-7 test
 * and this would clear/credit a unit with no status, failing both asserts.) */
static void test_holy_word_no_status_shows_miss_no_change(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].mp_current = 100;
    data_fd2_battle_spell_effect_table[0x19].mp_cost = 0;
    g_test_rc_array[1].flags = 0x05;                /* bit7 NOT set */
    g_test_rc_array[1].job_id = 1;
    g_test_rc_array[1].status_flags_block[0] = 5;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 1;
    fd2_execute_status_clear_holy_word_spell_id_25(0, 1, &target_id);
    ASSERT_EQ(g_test_rc_array[1].flags, 0x05);               /* unchanged */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);         /* no credit */
}

/* Intermediate-class job (9..0x18) adds 30 to the status_value used for XP (asm
 * 0x22c95 ADD EDX,0x1e), so the credit becomes (level + 30)*8. job 9 (first
 * intermediate value), level 5 -> status_value 35 -> XP 35*8 = 280. bit-7 still
 * gets cleared. */
static void test_holy_word_intermediate_class_xp_bonus(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].mp_current = 100;
    data_fd2_battle_spell_effect_table[0x19].mp_cost = 0;
    g_test_rc_array[1].flags = 0x80;                /* bit7 set */
    g_test_rc_array[1].job_id = 9;                  /* intermediate -> +30 */
    g_test_rc_array[1].status_flags_block[0] = 5;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 1;
    fd2_execute_status_clear_holy_word_spell_id_25(0, 1, &target_id);
    ASSERT_EQ(g_test_rc_array[1].flags, 0x00);               /* bit7 cleared */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 280);       /* (5+30)*8 */
}

/* Multi-target: the per-target loop reads target_id_array[iter] as a BYTE (asm
 * 0x22c63 MOVZX ECX,[ESI+EBX]), so the char id comes from the array, not the
 * loop counter. Non-adjacent targets 2 and 5 (both bit-7 set) must both be
 * cleared and credited while a bit-7-set bystander at index 0 (not in the list)
 * stays set. Both targets: level 5, non-intermediate job -> XP 5*8 = 40 each,
 * total 80. A loop that used iter as the char id, or stopped after one target,
 * would leave index 5 (or 0) wrong. */
static void test_holy_word_visits_all_targets_by_array_index(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    data_fd2_battle_spell_effect_table[0x19].mp_cost = 0;
    g_test_rc_array[0].flags = 0x80;                /* bystander, must NOT clear */
    g_test_rc_array[2].flags = 0x80;
    g_test_rc_array[2].job_id = 1;
    g_test_rc_array[2].status_flags_block[0] = 5;
    g_test_rc_array[5].flags = 0x80;
    g_test_rc_array[5].job_id = 1;
    g_test_rc_array[5].status_flags_block[0] = 5;
    data_fd2_battle_pending_xp_credit = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_execute_status_clear_holy_word_spell_id_25(0, 2, target_ids);
    ASSERT_EQ(g_test_rc_array[0].flags, 0x80);               /* untouched */
    ASSERT_EQ(g_test_rc_array[2].flags, 0x00);               /* cleared */
    ASSERT_EQ(g_test_rc_array[5].flags, 0x00);               /* cleared */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 80);        /* 40 + 40 */
}

/* ---- fd2_cast_status_inflict_spell @ 0x22D1B ---- */

/* Single un-afflicted target, ordinary job (1): the status lands. The
 * success roll is the FIRST RNG draw (asm 0x22DBA CALL fd2_advance_rng_state
 * -> 0x22DBF MOV EDX,EAX / IDIV 100), seed 0 -> 0x80A4, %100 = 32 < 0x32 ->
 * HIT. The real fd2_apply_damage_and_award_xp(target,10) consumes the SECOND
 * RNG draw (0x85C0) internally and, with base 10 -> floor(10*9/10)=9 + the
 * variance floor((0x85C0%100)*10/1000)=floor(400/1000)=0, deals 9 damage:
 * hp 50 -> 41 (portrait_id 0 < 0x44 so apply_damage early-returns the damage
 * via the JL 0x14230 tail, awarding no inner XP). The status byte at the raw
 * struct offset sprite_id (0x26 = status_sleep_flag) is then set from the
 * THIRD RNG draw (asm 0x22DED CALL / IDIV 4): 0xAEA0 % 4 = 0, +2 -> 2. XP
 * credit is status_flags_block[0]*8 = 5*8 = 40 with NO intermediate-class
 * bonus (the inflict worker has no +30, unlike the buff/cure siblings).
 * Guards the dual EAX-bug fix: both modulos read the RNG return, not the
 * job_id / apply_damage return that Ghidra mis-attributed. */
static void test_status_inflict_applies_status_and_timer(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;   /* bound impact/flicker loops */
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x00;          /* < 0x44 -> no inner XP */
    g_test_rc_array[0].job_id = 1;                  /* not 0x19/0x1A immune */
    g_test_rc_array[0].status_flags_block[0] = 5;   /* level */
    g_test_rc_array[0].status_sleep_flag = 0;       /* +0x26: not afflicted */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_status_inflict_spell(0, 0x1b, 1, (uint32)&target_id, 0x26);
    ASSERT_EQ(g_test_rc_array[0].status_sleep_flag, 2);   /* timer 2..5 -> 2 */
    ASSERT_EQ(g_test_rc_array[0].hp_current, 41);         /* 50 - 9 damage */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 40);     /* 5 * 8, no +30 */
}

/* RNG-roll miss: un-afflicted, ordinary job, but the success roll lands
 * >= 0x32. seed 3 -> first RNG draw 0x80BC, %100 = 56 >= 50 (asm 0x22DCE
 * JGE -> MISS branch), so the else path draws the miss indicator and NOTHING
 * changes -- status byte stays 0, HP stays put, no XP. Only ONE RNG draw is
 * consumed (the roll) before the miss, distinguishing this from the hit path
 * which draws three. */
static void test_status_inflict_rng_roll_miss_no_change(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 60;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].status_flags_block[0] = 5;
    g_test_rc_array[0].status_sleep_flag = 0;
    data_fd2_shared_rng_seed = 3;                   /* roll 0x80BC %100=56 */
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_status_inflict_spell(0, 0x1b, 1, (uint32)&target_id, 0x26);
    ASSERT_EQ(g_test_rc_array[0].status_sleep_flag, 0);   /* not afflicted */
    ASSERT_EQ(g_test_rc_array[0].hp_current, 60);         /* no damage */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);      /* no credit */
}

/* Boss/immune class gate: job_id 0x19 (asm 0x22DB0 CMP 0x19 JZ) and 0x1A
 * (0x22DB5 CMP 0x1A JZ) both short-circuit to the miss path BEFORE the RNG is
 * touched, even though the status byte is 0. The status byte stays 0, no XP
 * is credited, and the RNG seed is UNCHANGED (proving the gate precedes the
 * success roll). Both immune ids are checked in one test. */
static void test_status_inflict_immune_job_no_affliction(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].job_id = 0x19;               /* boss/immune */
    g_test_rc_array[0].status_flags_block[0] = 5;
    g_test_rc_array[0].status_sleep_flag = 0;
    data_fd2_shared_rng_seed = 0x1234;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_status_inflict_spell(0, 0x1b, 1, (uint32)&target_id, 0x26);
    ASSERT_EQ(g_test_rc_array[0].status_sleep_flag, 0);   /* immune */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);
    ASSERT_EQ(data_fd2_shared_rng_seed, 0x1234);          /* RNG untouched */

    g_test_rc_array[0].job_id = 0x1a;               /* second immune class */
    fd2_cast_status_inflict_spell(0, 0x1b, 1, (uint32)&target_id, 0x26);
    ASSERT_EQ(g_test_rc_array[0].status_sleep_flag, 0);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);
    ASSERT_EQ(data_fd2_shared_rng_seed, 0x1234);
}

/* Already-afflicted gate: the status byte at sprite_id is non-zero (asm
 * 0x22DA5 MOVZX EAX,[EBP] / 0x22DAB JNZ -> miss), so the spell will not stack
 * -- it draws the miss indicator, leaves the existing timer untouched, awards
 * no XP, and does not advance the RNG (the affliction test precedes the
 * roll). */
static void test_status_inflict_already_afflicted_no_restack(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].status_flags_block[0] = 5;
    g_test_rc_array[0].status_sleep_flag = 4;       /* already afflicted */
    data_fd2_shared_rng_seed = 0x55aa;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_status_inflict_spell(0, 0x1b, 1, (uint32)&target_id, 0x26);
    ASSERT_EQ(g_test_rc_array[0].status_sleep_flag, 4);   /* unchanged */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);
    ASSERT_EQ(data_fd2_shared_rng_seed, 0x55aa);          /* RNG untouched */
}

/* The status byte is addressed by the RAW struct byte offset sprite_id (asm
 * 0x22D9F MOV EBP,[ESP+0x28] / 0x22DA3 ADD EBP,EDI -> byte ptr [target+
 * sprite_id]), NOT a fixed field. Passing sprite_id 0x27 (combat_aux_block[0])
 * must set the byte at +0x27 to the timer (2) while the +0x26 status_sleep_
 * flag stays 0. If the worker used a hard-coded offset, the wrong byte would
 * move. Same seed-0 hit chain as the first test (timer 0xAEA0%4+2 = 2). */
static void test_status_inflict_sprite_id_selects_correct_byte(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x00;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].status_flags_block[0] = 5;
    g_test_rc_array[0].status_sleep_flag = 0;        /* +0x26 must stay 0 */
    g_test_rc_array[0].combat_aux_block[0] = 0;      /* +0x27 target byte */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_status_inflict_spell(0, 0x16, 1, (uint32)&target_id, 0x27);
    ASSERT_EQ(g_test_rc_array[0].combat_aux_block[0], 2);  /* +0x27 set */
    ASSERT_EQ(g_test_rc_array[0].status_sleep_flag, 0);    /* +0x26 untouched */
}

/* The per-target loop reads the unit id from the BYTE array by array index
 * (asm 0x22D81 MOV ESI,[ESP+0x24] / 0x22D85 ADD ESI,EAX / 0x22D87 MOVZX from
 * *ESI), not from the loop counter. With target_ids = {5, 2}: the first entry
 * (value 5) consumes RNG call 1 (0x80A4 %100=32 -> HIT) and is afflicted
 * (timer from call 3 -> 2, XP 5*8=40); the second entry (value 2) consumes
 * RNG call 4 (0xF5A1 %100=81 >= 50 -> MISS) and stays 0. Bystanders at the
 * loop-counter indices 0 and 1 must stay 0, proving the char id comes from
 * the array value (5) and not the iteration index. */
static void test_status_inflict_visits_targets_by_array_index(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[5].hp_current = 50;
    g_test_rc_array[5].hp_max = 200;
    g_test_rc_array[5].portrait_id = 0x00;
    g_test_rc_array[5].job_id = 1;
    g_test_rc_array[5].status_flags_block[0] = 5;
    g_test_rc_array[5].status_sleep_flag = 0;
    g_test_rc_array[2].job_id = 1;
    g_test_rc_array[2].status_sleep_flag = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_ids[0] = 5;
    target_ids[1] = 2;
    fd2_cast_status_inflict_spell(0, 0x1b, 2, (uint32)target_ids, 0x26);
    ASSERT_EQ(g_test_rc_array[5].status_sleep_flag, 2);   /* array entry 0 hit */
    ASSERT_EQ(g_test_rc_array[2].status_sleep_flag, 0);   /* entry 1 rolled miss */
    ASSERT_EQ(g_test_rc_array[0].status_sleep_flag, 0);   /* not loop-indexed */
    ASSERT_EQ(g_test_rc_array[1].status_sleep_flag, 0);   /* not loop-indexed */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 40);     /* only unit 5 hit */
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
    RUN_TEST(test_status_via_d1b_resets_deducts_and_forwards);
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
    RUN_TEST(test_holy_word_clears_status_and_credits_xp);
    RUN_TEST(test_holy_word_no_status_shows_miss_no_change);
    RUN_TEST(test_holy_word_intermediate_class_xp_bonus);
    RUN_TEST(test_holy_word_visits_all_targets_by_array_index);
    RUN_TEST(test_status_inflict_applies_status_and_timer);
    RUN_TEST(test_status_inflict_rng_roll_miss_no_change);
    RUN_TEST(test_status_inflict_immune_job_no_affliction);
    RUN_TEST(test_status_inflict_already_afflicted_no_restack);
    RUN_TEST(test_status_inflict_sprite_id_selects_correct_byte);
    RUN_TEST(test_status_inflict_visits_targets_by_array_index);
    printf("\n");
}
