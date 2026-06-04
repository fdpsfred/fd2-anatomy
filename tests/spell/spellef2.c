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



/* ---- fd2_cast_group_hp_heal_spell @ 0x211A4 ---- */

/* The group-heal loop must visit EVERY entry of the byte target array (asm
 * 0x211DA XOR ESI,ESI .. 0x21200 CMP ESI,EDI / 0x21202 JL), not just the
 * first. Two live targets (hp 50/max 200) both get healed through the real
 * fd2_apply_hp_heal_and_award_xp: target[0] consumes RNG call 1 (seed 0 ->
 * 0x80A4, %100=32 -> extra (32*100)/1000=3 -> heal 90+3=93 -> hp 50->143);
 * target[1] consumes RNG call 2 (-> 0x85C0, %100=40 -> extra 4 -> heal 94 ->
 * hp 50->144). Asserting BOTH hp values changed (and to the exact per-call
 * amounts) proves the loop iterates both indices in order. If the loop ever
 * stopped after one target, target[1] would stay at 50. */
static void test_group_heal_visits_all_targets(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;   /* bound impact/flicker loops */
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x50;    /* >= 0x4b -> skip XP block */
    g_test_rc_array[1].hp_current = 50;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].portrait_id = 0x50;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 0;
    target_ids[1] = 1;
    fd2_cast_group_hp_heal_spell(0, 2, (uint32)target_ids, 100);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 143);   /* 50 + 90 + 3 */
    ASSERT_EQ(g_test_rc_array[1].hp_current, 144);   /* 50 + 90 + 4 */
}


/* The heal target is read from targets[iter] as a BYTE (asm 0x211E2
 * MOVZX EAX,byte ptr [ESI+EBP]), so the array entries select WHICH chars heal.
 * Place the two live targets at non-adjacent indices 2 and 5 and leave a char
 * at index 0 untouched: only chars 2 and 5 must change, char 0 must stay put.
 * A bug that healed char index 0 (e.g. ignoring the array and using iter as the
 * char id) would bump g_test_rc_array[0] and fail the unchanged assert. */
static void test_group_heal_indexes_target_array(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 77;       /* bystander, must NOT change */
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[2].hp_current = 50;
    g_test_rc_array[2].hp_max = 200;
    g_test_rc_array[2].portrait_id = 0x50;
    g_test_rc_array[5].hp_current = 50;
    g_test_rc_array[5].hp_max = 200;
    g_test_rc_array[5].portrait_id = 0x50;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_cast_group_hp_heal_spell(0, 2, (uint32)target_ids, 100);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 77);    /* untouched */
    ASSERT_EQ(g_test_rc_array[2].hp_current, 143);   /* 50 + 90 + 3 */
    ASSERT_EQ(g_test_rc_array[5].hp_current, 144);   /* 50 + 90 + 4 */
}


/* Per-target heal goes through the real worker INCLUDING its HP-max cap branch
 * (battle.c L70-71). target[0] sits at 195/200: 195 + 93 = 288 > 200, so it
 * must clamp to exactly 200, not overflow. Confirms the loop body delegates to
 * the real fd2_apply_hp_heal_and_award_xp rather than an unclamped add. */
static void test_group_heal_caps_at_max(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[3].hp_current = 195;
    g_test_rc_array[3].hp_max = 200;
    g_test_rc_array[3].portrait_id = 0x50;
    data_fd2_shared_rng_seed = 0;
    target_id = 3;
    fd2_cast_group_hp_heal_spell(0, 1, (uint32)&target_id, 100);
    ASSERT_EQ(g_test_rc_array[3].hp_current, 200);   /* clamped */
}


/* Empty target list (count 0): the loop body never runs, so no char is healed,
 * but the function must still complete (impact + flicker + shared-epilogue
 * composite). Guards the loop guard (JL on count) against an off-by-one that
 * would heal char targets[0] when count is 0. */
static void test_group_heal_zero_targets_no_heal(void)
{
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 60;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x50;
    data_fd2_shared_rng_seed = 0;
    fd2_cast_group_hp_heal_spell(0, 0, (uint32)0, 100);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 60);    /* unchanged */
}


/* ---- fd2_execute_offensive_targeted_spell @ 0x21227 ---- */

/* This worker's defining behavior vs the sister fd2_apply_attack_spell_damage
 * @0x2111A is the MP deduction: asm 0x2126B-0x21275 PUSH EDI(spell_id) /
 * PUSH caster / CALL fd2_deduct_caster_mp. The real deduct subtracts
 * spell_effect_table[spell_id].mp_cost from runtime_char[caster].mp_current.
 * Drive spell_id 0 with mp_cost 8 and caster mp 50 -> 42. The lone target sits
 * at (0,0) (outside the impact/overlay/show view window) and hit_rate 0 forces
 * fd2_calc_magic_damage to return 0 (the window-culled show_miss path is a pure
 * queue no-op), isolating the MP deduction. Drop the deduct CALL and mp stays
 * 50. */
static void test_offensive_targeted_deducts_mp(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].mp_current = 50;
    g_test_rc_array[1].job_id = 1;            /* keep calc in-bounds */
    data_fd2_battle_spell_effect_table[0].mp_cost = 8;
    data_fd2_battle_spell_effect_table[0].damage = 0;
    data_fd2_battle_spell_effect_table[0].hit_rate = 0;   /* always miss */
    data_fd2_shared_rng_seed = 0;
    target_id = 1;
    fd2_execute_offensive_targeted_spell(1, 0, 1, (int)&target_id);
    ASSERT_EQ(g_test_rc_array[1].mp_current, 42);
}


/* The worker resets data_fd2_battle_spell_aoe_count_and_fx_queue_idx to 0 at
 * entry (asm 0x2123D MOV [0x53EC4],0). The only callees that re-write that
 * counter are show_damage/show_miss, and each is viewport-culled: a target at
 * (0,0) is outside the window so neither enqueues, leaving the counter at the
 * reset value. Pre-stain it 0x99 and confirm the entry reset wins. */
static void test_offensive_targeted_resets_aoe_count(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_battle_spell_effect_table[0].damage = 0;
    data_fd2_battle_spell_effect_table[0].hit_rate = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0x99;
    target_id = 1;
    fd2_execute_offensive_targeted_spell(1, 0, 1, (int)&target_id);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0);
}


/* The damage loop must visit EVERY entry of the byte target array (asm
 * 0x21278 XOR ESI,ESI .. 0x2128C CMP ESI,EBP / 0x2128E JGE) and feed each
 * target through the real fd2_calc_magic_damage, whose return (EAX) is the
 * applied HP loss (forwarded straight into show_damage with no EAX clobber).
 * Two live targets at non-adjacent indices 2 and 5 (hit_rate 100 always hits;
 * job_id 1 + resist 10 keep the formula in-bounds) must BOTH lose HP from
 * their starting value, while a bystander at index 0 stays put. The exact
 * damage VALUE is owned by testbtl's magic-damage tests; here only the per-
 * target HP change is asserted, proving the loop iterates both array indices.
 * (A loop that stopped after one target would leave index-5 HP unchanged.) */
static void test_offensive_targeted_damages_all_targets(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 88;       /* bystander, must NOT change */
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[2].hp_current = 200;
    g_test_rc_array[2].hp_max = 200;
    g_test_rc_array[2].job_id = 1;
    g_test_rc_array[2].portrait_id = 0x01;
    g_test_rc_array[5].hp_current = 200;
    g_test_rc_array[5].hp_max = 200;
    g_test_rc_array[5].job_id = 1;
    g_test_rc_array[5].portrait_id = 0x01;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 100;
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_execute_offensive_targeted_spell(0, 0, 2, (int)target_ids);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 88);          /* untouched */
    ASSERT_NE(g_test_rc_array[2].hp_current, 200);         /* took damage */
    ASSERT_NE(g_test_rc_array[5].hp_current, 200);         /* took damage */
}


/* Pipeline-structure pin (and BLINK-vs-FLASH discriminator). This worker uses
 * the overlay-BLINK animator (asm 0x21263 CALL fd2_animate_spell_overlay_blink),
 * which composites ZERO times (it blits via fd2_blit_rectangle only). So the
 * total composite count is: impact animation = 2 (entry + finalize) + overlay
 * blink = 0 + the Pattern-A shared epilogue fd2_composite_battle_frame(0) = 1,
 * giving exactly 3. The sister flash worker would instead total 6 (its
 * full_screen_flash composites 3x). Two targets at (0,0) are window-culled so
 * the per-target show paths add no composites. Asserting 3 simultaneously pins
 * the shared-epilogue tail and proves the blink (not flash) animator is wired. */
static void test_offensive_targeted_composites_three(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 100;
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 0;
    target_ids[1] = 1;
    g_composite_call_count = 0;
    fd2_execute_offensive_targeted_spell(0, 0, 2, (int)target_ids);
    ASSERT_EQ(g_composite_call_count, 3);
}


/* ---- fd2_execute_offensive_targeted_spell_variant_b @ 0x212B9 ----
 * Byte-identical dead clone of fd2_execute_offensive_targeted_spell @
 * 0x21227 (no callers / no xrefs; second physical copy kept by the
 * linker). The tests mirror the original worker's risk-oriented coverage
 * to prove the clone is wired with the same callees and operands:
 * the MP-deduct CALL, the entry aoe-count reset, the per-target damage
 * loop (EAX of fd2_calc_magic_damage forwarded into show_damage), and
 * the composite count of 3 (blink animator + Pattern-A shared epilogue,
 * discriminating it from the full-screen-flash sibling that totals 6). */

/* MP deduction: asm 0x21302 PUSH spell_id / PUSH caster / CALL
 * fd2_deduct_caster_mp subtracts spell_effect_table[0].mp_cost(8) from
 * runtime_char[caster].mp_current(50) -> 42. Lone target at (0,0) is
 * window-culled and hit_rate 0 forces calc_magic_damage 0, isolating the
 * deduct. */
static void test_offensive_variantb_deducts_mp(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].mp_current = 50;
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_spell_effect_table[0].mp_cost = 8;
    data_fd2_battle_spell_effect_table[0].damage = 0;
    data_fd2_battle_spell_effect_table[0].hit_rate = 0;
    data_fd2_shared_rng_seed = 0;
    target_id = 1;
    fd2_execute_offensive_targeted_spell_variant_b(1, 0, 1, (int)&target_id);
    ASSERT_EQ(g_test_rc_array[1].mp_current, 42);
}

/* Entry reset: asm 0x212CF MOV [0x53EC4],0 clears the AoE/fx-queue
 * counter. Window-culled target adds no enqueue, so a pre-stain of 0x99
 * must be overwritten with 0. */
static void test_offensive_variantb_resets_aoe_count(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_battle_spell_effect_table[0].damage = 0;
    data_fd2_battle_spell_effect_table[0].hit_rate = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0x99;
    target_id = 1;
    fd2_execute_offensive_targeted_spell_variant_b(1, 0, 1, (int)&target_id);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0);
}

/* Damage loop visits EVERY byte-array entry (asm 0x2130A XOR ESI,ESI ..
 * 0x2131E CMP / 0x21320 JGE). Two live targets at non-adjacent indices 2
 * and 5 (hit_rate 100; job_id 1 + resist 10 keep the formula in-bounds)
 * must BOTH lose HP, a bystander at index 0 stays put. */
static void test_offensive_variantb_damages_all_targets(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 88;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[2].hp_current = 200;
    g_test_rc_array[2].hp_max = 200;
    g_test_rc_array[2].job_id = 1;
    g_test_rc_array[2].portrait_id = 0x01;
    g_test_rc_array[5].hp_current = 200;
    g_test_rc_array[5].hp_max = 200;
    g_test_rc_array[5].job_id = 1;
    g_test_rc_array[5].portrait_id = 0x01;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 100;
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_execute_offensive_targeted_spell_variant_b(0, 0, 2, (int)target_ids);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 88);
    ASSERT_NE(g_test_rc_array[2].hp_current, 200);
    ASSERT_NE(g_test_rc_array[5].hp_current, 200);
}

/* Pipeline-structure pin + blink-vs-flash discriminator. The clone uses
 * the overlay-BLINK animator (asm 0x212F5 CALL fd2_animate_spell_overlay_
 * blink, 0 composites); total = impact 2 + blink 0 + shared-epilogue
 * fd2_composite_battle_frame(0) 1 = exactly 3 (the flash sibling totals
 * 6). Two targets at (0,0) are window-culled so per-target show adds none. */
static void test_offensive_variantb_composites_three(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 100;
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 0;
    target_ids[1] = 1;
    g_composite_call_count = 0;
    fd2_execute_offensive_targeted_spell_variant_b(0, 0, 2, (int)target_ids);
    ASSERT_EQ(g_composite_call_count, 3);
}


/* ---- fd2_execute_offensive_full_screen_flash_spell @ 0x213B7 ----
 * Byte-for-byte the same algorithm as fd2_execute_offensive_targeted_spell
 * @0x21227, the sole difference being the second animation call:
 * fd2_animate_spell_full_screen_flash (asm 0x213F3) instead of overlay_blink.
 * Risk-oriented coverage mirrors the sister worker: the MP-deduct CALL, the
 * entry aoe-count reset, the per-target damage loop (EAX of
 * fd2_calc_magic_damage forwarded into show_damage), and a composite count of
 * 6 that discriminates the full-screen-flash animator from the blink sibling
 * (which totals 3). */

/* MP deduction: asm 0x213FB PUSH EDI(spell_id) / PUSH caster / CALL
 * fd2_deduct_caster_mp subtracts spell_effect_table[0].mp_cost(8) from
 * runtime_char[caster].mp_current(50) -> 42. Lone target at (0,0) is
 * window-culled and hit_rate 0 forces calc_magic_damage 0, isolating the
 * deduct. Drop the deduct CALL and mp stays 50. */
static void test_offensive_flash_deducts_mp(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].mp_current = 50;
    g_test_rc_array[1].job_id = 1;            /* keep calc in-bounds */
    data_fd2_battle_spell_effect_table[0].mp_cost = 8;
    data_fd2_battle_spell_effect_table[0].damage = 0;
    data_fd2_battle_spell_effect_table[0].hit_rate = 0;   /* always miss */
    data_fd2_shared_rng_seed = 0;
    target_id = 1;
    fd2_execute_offensive_full_screen_flash_spell(1, 0, 1, (int)&target_id);
    ASSERT_EQ(g_test_rc_array[1].mp_current, 42);
}

/* Entry reset: asm 0x213CD MOV [0x53EC4],0 clears the AoE/fx-queue counter.
 * Window-culled target adds no enqueue, so a pre-stain of 0x99 must be
 * overwritten with 0. */
static void test_offensive_flash_resets_aoe_count(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_battle_spell_effect_table[0].damage = 0;
    data_fd2_battle_spell_effect_table[0].hit_rate = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0x99;
    target_id = 1;
    fd2_execute_offensive_full_screen_flash_spell(1, 0, 1, (int)&target_id);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0);
}

/* Damage loop visits EVERY byte-array entry (asm 0x21408 XOR ESI,ESI ..
 * 0x2141C CMP ESI,EBP / 0x2141E JGE) and feeds each target through the real
 * fd2_calc_magic_damage, whose return (EAX) is forwarded straight into
 * show_damage with no clobber. Two live targets at non-adjacent indices 2 and
 * 5 (hit_rate 100; job_id 1 + resist 10 keep the formula in-bounds) must BOTH
 * lose HP, a bystander at index 0 stays put. A loop that stopped after one
 * target would leave index-5 HP unchanged. */
static void test_offensive_flash_damages_all_targets(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].hp_current = 88;       /* bystander, must NOT change */
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[2].hp_current = 200;
    g_test_rc_array[2].hp_max = 200;
    g_test_rc_array[2].job_id = 1;
    g_test_rc_array[2].portrait_id = 0x01;
    g_test_rc_array[5].hp_current = 200;
    g_test_rc_array[5].hp_max = 200;
    g_test_rc_array[5].job_id = 1;
    g_test_rc_array[5].portrait_id = 0x01;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 100;
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_execute_offensive_full_screen_flash_spell(0, 0, 2, (int)target_ids);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 88);          /* untouched */
    ASSERT_NE(g_test_rc_array[2].hp_current, 200);         /* took damage */
    ASSERT_NE(g_test_rc_array[5].hp_current, 200);         /* took damage */
}

/* Pipeline-structure pin + FLASH-vs-BLINK discriminator. This worker uses the
 * full-screen-FLASH animator (asm 0x213F3 CALL fd2_animate_spell_full_screen_
 * flash), which composites THREE times (two real
 * fd2_composite_chars_with_spell_effect_overlay tile-map composites + its
 * closing finalize). Total = impact animation 2 (entry + finalize) + flash 3 +
 * Pattern-A shared epilogue fd2_composite_battle_frame(0) 1 = exactly 6. The
 * sister blink worker would instead total 3. Two targets at (0,0) are window-
 * culled so the per-target show paths add no composites. Asserting 6
 * simultaneously pins the shared-epilogue tail and proves the flash (not blink)
 * animator is wired. */
static void test_offensive_flash_composites_six(void)
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
    data_fd2_battle_spell_effect_table[0].mp_cost = 0;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 0;
    target_ids[1] = 1;
    g_composite_call_count = 0;
    fd2_execute_offensive_full_screen_flash_spell(0, 0, 2, (int)target_ids);
    ASSERT_EQ(g_composite_call_count, 6);
}


/* ---- fd2_execute_offensive_single_target_spell_id_9 @ 0x214AD ----
 * Dedicated SINGLE-TARGET offensive worker reached only via the spell
 * dispatch table (entry index 9); spell_id literal 9 is baked into the
 * body (asm 0x214CB/0x214D9/0x214E7 all PUSH 0x9). Unlike the looping
 * siblings (0x21227 / 0x213B7) it hits only target_id_array[0], plays NO
 * second blink/flash animation, and ends with its own explicit RET
 * running fd2_composite_battle_frame(0) + fd2_animate_spell_projectile_
 * paths() inline (not the 0x21190 shared epilogue). Risk-oriented
 * coverage: the spell-9 MP deduct, the entry aoe-count reset, the
 * single-target (no-loop) damage application, and the composite count of
 * 3 (impact 2 + no second anim + inline epilogue 1). */

/* MP deduction with the BAKED spell_id 9: asm 0x214D9 PUSH 0x9 /
 * 0x214DB PUSH caster / CALL fd2_deduct_caster_mp subtracts
 * spell_effect_table[9].mp_cost from runtime_char[caster].mp_current.
 * Entry 9 has mp_cost 8 (50 -> 42) while entry 0 is poisoned with 99: if
 * the literal were ever read as 0 the caster would drop to -49 (0xFFCF),
 * so 42 simultaneously proves the deduct fires AND that it indexes spell
 * entry 9, not 0. Lone target at (0,0) is window-culled and hit_rate 0
 * forces calc_magic_damage 0, isolating the deduct. */
static void test_offensive_single9_deducts_mp(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].mp_current = 50;
    g_test_rc_array[1].job_id = 1;            /* keep calc in-bounds */
    data_fd2_battle_spell_effect_table[0].mp_cost = 99;   /* poison entry 0 */
    data_fd2_battle_spell_effect_table[9].mp_cost = 8;
    data_fd2_battle_spell_effect_table[9].damage = 0;
    data_fd2_battle_spell_effect_table[9].hit_rate = 0;   /* always miss */
    data_fd2_shared_rng_seed = 0;
    target_id = 1;
    fd2_execute_offensive_single_target_spell_id_9(1, 1, &target_id);
    ASSERT_EQ(g_test_rc_array[1].mp_current, 42);
}

/* Entry reset: asm 0x214BC MOV [0x53EC4],0 clears the AoE/fx-queue
 * counter before anything else. The window-culled target adds no
 * enqueue, so a pre-stain of 0x99 must be overwritten with 0. */
static void test_offensive_single9_resets_aoe_count(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_spell_effect_table[9].mp_cost = 0;
    data_fd2_battle_spell_effect_table[9].damage = 0;
    data_fd2_battle_spell_effect_table[9].hit_rate = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0x99;
    target_id = 1;
    fd2_execute_offensive_single_target_spell_id_9(1, 1, &target_id);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0);
}

/* SINGLE-TARGET discriminator (the defining trait vs the looping
 * siblings). The body reads ONLY target_id_array[0] (asm 0x214E9 MOVZX
 * EAX,byte ptr [EBX]; no XOR ESI/loop) and damages that one char through
 * the real fd2_calc_magic_damage. The target array holds {2, 5} and the
 * spell_arg (3rd asm arg = claimed n_targets) is deliberately 2, yet
 * only char 2 (= array[0]) may lose HP; char 5 (= array[1]) must stay at
 * 200. A regression that looped over the array (like 0x21227) would also
 * damage char 5 and fail the unchanged assert. job_id 1 + resist 10 +
 * hit_rate 100 keep the formula in-bounds and guarantee the hit. */
static void test_offensive_single9_hits_only_first_target(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[2].hp_current = 200;
    g_test_rc_array[2].hp_max = 200;
    g_test_rc_array[2].job_id = 1;
    g_test_rc_array[2].portrait_id = 0x01;
    g_test_rc_array[5].hp_current = 200;      /* array[1]: must NOT change */
    g_test_rc_array[5].hp_max = 200;
    g_test_rc_array[5].job_id = 1;
    g_test_rc_array[5].portrait_id = 0x01;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[9].damage = 50;
    data_fd2_battle_spell_effect_table[9].hit_rate = 100;
    data_fd2_battle_spell_effect_table[9].mp_cost = 0;
    data_fd2_shared_rng_seed = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_execute_offensive_single_target_spell_id_9(0, 2, target_ids);
    ASSERT_NE(g_test_rc_array[2].hp_current, 200);   /* array[0] took damage */
    ASSERT_EQ(g_test_rc_array[5].hp_current, 200);   /* array[1] untouched */
}

/* Pipeline-structure pin + NO-SECOND-ANIMATION discriminator. This
 * worker plays ONLY the per-target impact animation (no blink, no
 * flash), then runs fd2_composite_battle_frame(0) + projectile paths
 * inline via its own RET. Composite total = impact 2 (entry + finalize)
 * + inline epilogue 1 = exactly 3 (fd2_animate_spell_projectile_paths
 * composites 0). Matching 3 here while the function makes only ONE
 * animation call proves no extra blink/flash animator (which would push
 * the count to 6) sneaked in, and that the explicit-RET tail composites
 * once. Lone target at (0,0) is window-culled so the show path adds no
 * composite. */
static void test_offensive_single9_composites_three(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].job_id = 1;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[9].damage = 50;
    data_fd2_battle_spell_effect_table[9].hit_rate = 100;
    data_fd2_battle_spell_effect_table[9].mp_cost = 0;
    data_fd2_shared_rng_seed = 0;
    target_id = 1;
    g_composite_call_count = 0;
    fd2_execute_offensive_single_target_spell_id_9(0, 1, &target_id);
    ASSERT_EQ(g_composite_call_count, 3);
}

/* ---- fd2_cast_ap_boost_spell @ 0x22721 ---- */

/* Single target, not yet boosted: the buff must land. ap 100 -> delta =
 * (int)(1.0 + 100*0.15) = (int)16.0 = 16 (Watcom __CHP truncates toward zero),
 * so ap 100 -> 116. The buff timer status_flags_block[1] (asm field +0x22) must
 * be set from the RNG: seed 0 -> fd2_advance_rng_state returns 0x80A4 (32932),
 * (int)32932 % 4 = 0, +2 -> 2. level byte status_flags_block[0] = 5 with a
 * non-intermediate job (1) gives XP credit 5*2 = 10. Guards the EAX-bug fix:
 * the timer comes from the RNG return, not the old (==0) flag value. */
static void test_ap_boost_applies_buff_and_timer(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;   /* bound impact/flicker loops */
    g_test_rc_array[0].ap = 100;
    g_test_rc_array[0].job_id = 1;            /* not 9..0x18 -> no +30 */
    g_test_rc_array[0].status_flags_block[0] = 5;   /* level */
    g_test_rc_array[0].status_flags_block[1] = 0;   /* not yet boosted */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_ap_boost_spell(0, 1, &target_id);
    ASSERT_EQ(g_test_rc_array[0].ap, 116);
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[1], 2);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 10);
}


/* Already-boosted target (timer != 0): the else branch shows the miss indicator
 * and must NOT stack the buff -- ap, timer, and XP credit all stay put. */
static void test_ap_boost_skips_already_boosted(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].ap = 100;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].status_flags_block[0] = 5;
    g_test_rc_array[0].status_flags_block[1] = 3;   /* already boosted */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_ap_boost_spell(0, 1, &target_id);
    ASSERT_EQ(g_test_rc_array[0].ap, 100);                 /* unchanged */
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[1], 3);/* unchanged */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);       /* no credit */
}


/* Intermediate-class job (9..0x18) adds 30 to the level_mod used for XP credit
 * (asm 0x227a9 ADD [ESP+8],0x1e), and the boost still applies. job_id 9 (first
 * intermediate value) + level 5 -> level_mod 35 -> XP 35*2 = 70. ap 50 -> delta
 * = (int)(1.0 + 50*0.15) = (int)8.5 = 8 -> ap 58. timer from seed 0 -> 2. */
static void test_ap_boost_intermediate_class_xp_bonus(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].ap = 50;
    g_test_rc_array[0].job_id = 9;            /* intermediate class -> +30 */
    g_test_rc_array[0].status_flags_block[0] = 5;
    g_test_rc_array[0].status_flags_block[1] = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_id = 0;
    fd2_cast_ap_boost_spell(0, 1, &target_id);
    ASSERT_EQ(g_test_rc_array[0].ap, 58);
    ASSERT_EQ(g_test_rc_array[0].status_flags_block[1], 2);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 70);
}


/* The per-target loop reads target_id_array[iter] as a BYTE (asm 0x227d -> the
 * target index comes from the array, not the loop counter), so non-adjacent
 * indices 2 and 5 must both be boosted while a bystander at index 0 stays put.
 * Both targets start un-boosted (timer 0) with ap 100; target[2] consumes RNG
 * call 1 (seed 0 -> 0x80A4, %4=0 -> timer 2) and target[5] consumes RNG call 2
 * (-> 0x85C0, %4=0 -> timer 2). Each gets delta 16 -> ap 116. A loop that
 * stopped after one target, or used iter as the char id, would leave index 5
 * (or index 0) wrong. */
static void test_ap_boost_visits_all_targets(void)
{
    uint8 target_ids[2];
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    setup_impact_buffers();
    data_fd2_battle_party_member_count = 0;
    g_test_rc_array[0].ap = 200;             /* bystander, must NOT change */
    g_test_rc_array[2].ap = 100;
    g_test_rc_array[2].job_id = 1;
    g_test_rc_array[2].status_flags_block[1] = 0;
    g_test_rc_array[5].ap = 100;
    g_test_rc_array[5].job_id = 1;
    g_test_rc_array[5].status_flags_block[1] = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    target_ids[0] = 2;
    target_ids[1] = 5;
    fd2_cast_ap_boost_spell(0, 2, target_ids);
    ASSERT_EQ(g_test_rc_array[0].ap, 200);   /* untouched */
    ASSERT_EQ(g_test_rc_array[2].ap, 116);
    ASSERT_EQ(g_test_rc_array[5].ap, 116);
    ASSERT_EQ(g_test_rc_array[2].status_flags_block[1], 2);
    ASSERT_EQ(g_test_rc_array[5].status_flags_block[1], 2);
}

void run_spell_spelleff2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: spell/spelleff2\n");
    RUN_TEST(test_group_heal_visits_all_targets);
    RUN_TEST(test_group_heal_indexes_target_array);
    RUN_TEST(test_group_heal_caps_at_max);
    RUN_TEST(test_group_heal_zero_targets_no_heal);
    RUN_TEST(test_offensive_targeted_deducts_mp);
    RUN_TEST(test_offensive_targeted_resets_aoe_count);
    RUN_TEST(test_offensive_targeted_damages_all_targets);
    RUN_TEST(test_offensive_targeted_composites_three);
    RUN_TEST(test_offensive_variantb_deducts_mp);
    RUN_TEST(test_offensive_variantb_resets_aoe_count);
    RUN_TEST(test_offensive_variantb_damages_all_targets);
    RUN_TEST(test_offensive_variantb_composites_three);
    RUN_TEST(test_offensive_flash_deducts_mp);
    RUN_TEST(test_offensive_flash_resets_aoe_count);
    RUN_TEST(test_offensive_flash_damages_all_targets);
    RUN_TEST(test_offensive_flash_composites_six);
    RUN_TEST(test_offensive_single9_deducts_mp);
    RUN_TEST(test_offensive_single9_resets_aoe_count);
    RUN_TEST(test_offensive_single9_hits_only_first_target);
    RUN_TEST(test_offensive_single9_composites_three);
    RUN_TEST(test_ap_boost_applies_buff_and_timer);
    RUN_TEST(test_ap_boost_skips_already_boosted);
    RUN_TEST(test_ap_boost_intermediate_class_xp_bonus);
    RUN_TEST(test_ap_boost_visits_all_targets);
    printf("\n");
}
