/*
 * testbtl.c — Unit tests for battle core + spell handler functions
 */

#include <stdio.h>
#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* globals + stubs in testglob.c */
extern runtime_char g_test_rc_array[8];
extern int g_build_spell_list_return;
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

/* ---- Test: RNG ---- */

static void test_rng_advance(void)
{
    uint32 r1;
    uint32 r2;
    data_fd2_shared_rng_seed = 0;
    r1 = fd2_advance_rng_state();
    ASSERT_NE(r1, 0);
    r2 = fd2_advance_rng_state();
    ASSERT_NE(r2, r1);
}

static void test_rng_deterministic(void)
{
    uint32 r1;
    uint32 r2;
    data_fd2_shared_rng_seed = 12345;
    r1 = fd2_advance_rng_state();
    data_fd2_shared_rng_seed = 12345;
    r2 = fd2_advance_rng_state();
    ASSERT_EQ(r1, r2);
}

/* ---- Test: deduct MP ---- */

static void test_deduct_mp(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].mp_current = 100;
    data_fd2_battle_spell_effect_table[5].mp_cost = 10;
    fd2_deduct_caster_mp(0, 5);
    ASSERT_EQ(g_test_rc_array[0].mp_current, 90);
}

/* ---- Test: heal ---- */

static void test_heal_basic(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x50;
    data_fd2_shared_rng_seed = 0;
    result = fd2_apply_hp_heal_and_award_xp(0, 100);
    ASSERT_TRUE(result >= 90);
    ASSERT_TRUE(g_test_rc_array[0].hp_current > 50);
}

static void test_heal_cap_at_max(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 195;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x50;
    data_fd2_shared_rng_seed = 0;
    result = fd2_apply_hp_heal_and_award_xp(0, 100);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 200);
}

/* Heal XP-credit path WITH the mid-tier-job modifier (the two heal tests
 * above use portrait 0x50 >= 0x4b, so the whole XP block at battle.c L79-83 /
 * asm 0x1c9ba-0x1c9cf is skipped). portrait 0 < 0x4b -> the 0x1c9b8 JGE is NOT
 * taken, so the XP block runs. job_id 10 is in [9,24] (asm 0x1c9a4 CMP EDX,8
 * JLE / 0x1c9a9 CMP EDX,0x19 JGE both fall through), so the +0x1e modifier at
 * 0x1c9ae ADD EAX,0x1e IS applied -> level_mod = level(5) + 0x1e = 35.
 * Deterministic (seed 0 -> fd2_advance_rng_state returns 0x80A4 = 32932,
 * emulation-confirmed; %100 = 32):
 *   base_heal_90 = (100*9)/10 = 90
 *   extra_heal   = (32*100)/1000 = 3
 *   hp_after     = 50 + 90 + 3 = 143  (<= max 200, no cap)
 *   hp_gained    = 143 - 50 = 93
 *   return       = extra_heal + base_heal_90 = 3 + 90 = 93
 *   pending_xp  += (level_mod 35 * 0x28 * 93) / hp_max 200
 *               = (35*40*93)/200 = 130200/200 = 651
 * Asserting EXACT pending_xp_credit pins the level_mod*40*hp_gained/hp_max
 * formula AND the modifier-branch-taken side (this is a unique numeric+branch+
 * state-transition path; the sibling damage XP formula enemy[9]*level has no
 * job modifier so it gives no coverage here). */
static void test_heal_xp_job_modifier(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x00;          /* < 0x4b -> XP runs */
    g_test_rc_array[0].status_flags_block[0] = 5;   /* level */
    g_test_rc_array[0].job_id = 10;                 /* in [9,24] -> +0x1e */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    result = fd2_apply_hp_heal_and_award_xp(0, 100);
    ASSERT_EQ(result, 93);                           /* 90 + 3 */
    ASSERT_EQ(g_test_rc_array[0].hp_current, 143);   /* 50 + 90 + 3 */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 651); /* (35*40*93)/200 */
}

/* Companion to test_heal_xp_job_modifier proving the +0x1e modifier branch is
 * CONDITIONAL: job_id 5 (<= 8) takes the 0x1c9a7 JLE, so level_mod stays at the
 * raw level (5, no +0x1e). Everything else identical, so the XP credit drops to
 * (level_mod 5 * 0x28 * 93) / 200 = (5*40*93)/200 = 18600/200 = 93. If the
 * emitted C ever dropped the job guard and always added 0x1e, this would read
 * 651 like the other test and fail. */
static void test_heal_xp_no_job_modifier(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x00;          /* < 0x4b -> XP runs */
    g_test_rc_array[0].status_flags_block[0] = 5;   /* level */
    g_test_rc_array[0].job_id = 5;                  /* <= 8 -> no +0x1e */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    result = fd2_apply_hp_heal_and_award_xp(0, 100);
    ASSERT_EQ(result, 93);                           /* 90 + 3 */
    ASSERT_EQ(g_test_rc_array[0].hp_current, 143);   /* 50 + 90 + 3 */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 93);  /* (5*40*93)/200 */
}

/* ---- Test: damage ---- */

static void test_damage_basic(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x01;
    data_fd2_shared_rng_seed = 0;
    result = fd2_apply_damage_and_award_xp(0, 50);
    ASSERT_TRUE(result >= 45);
    ASSERT_TRUE(g_test_rc_array[0].hp_current < 200);
}

static void test_damage_floor_at_zero(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 5;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x01;
    data_fd2_shared_rng_seed = 0;
    result = fd2_apply_damage_and_award_xp(0, 500);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 0);
}

/* XP-award branch, KILL case (portrait_id >= 0x44 -> the 0x1c8aa JL is NOT
 * taken, so the whole XP block runs). This block contains three high-risk
 * elements the two tests above never reach (both use portrait 0x01 < 0x44):
 *   (a) CALL fd2_get_enemy_data_entry then dereference the RETURN value as a
 *       pointer (asm 0x1c8b4 CALL; 0x1c8bc MOVZX EDX,[EAX+9]) -- the Ghidra
 *       EAX-tracking-bug class. fd2_get_enemy_data_entry is the REAL accessor
 *       (&enemy_data_table[idx]), so seeding the table drives the read.
 *   (b) xp = exp_reward * level  (0x1c8c4 IMUL).
 *   (c) the kill/survive split (0x1c8cd JZ on HP_after==0).
 * portrait_id 0x44 -> enemy index 0x44-0x44 = 0. seed 0 -> jitter 32, so
 * actual_damage = (500*9)/10 + (32*500)/1000 = 450 + 16 = 466. HP 5 - 466
 * floors to 0 => KILL: HP_after==0 takes the JZ, skipping the proportional
 * IDIV, so the FULL xp is credited: exp_reward(10) * level(1) = 10.
 * Pure integer arithmetic (RNG value 0x80a4 confirmed via emulation). */
static void test_damage_xp_kill_full_reward(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_enemy_data_table, 0,
           sizeof(data_fd2_battle_enemy_data_table));
    g_test_rc_array[0].hp_current = 5;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x44;        /* enemy idx 0 */
    g_test_rc_array[0].status_flags_block[0] = 1; /* level */
    data_fd2_battle_enemy_data_table[0].exp_reward = 10;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    result = fd2_apply_damage_and_award_xp(0, 500);
    ASSERT_EQ(result, 466);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 0);             /* kill */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 10);        /* full xp */
}

/* XP-award branch, SURVIVE case: HP_after != 0 so the 0x1c8cd JZ is NOT taken
 * and the proportional IDIV runs (0x1c8cf IMUL xp,actual_damage; 0x1c8d7 IDIV
 * by HP_max). portrait 0x44 -> idx 0, level 1, exp_reward 10. seed 0 -> jitter
 * 32, base 50 -> actual_damage = (50*9)/10 + (32*50)/1000 = 45 + 1 = 46.
 * HP 200 - 46 = 154 (> 0, survives). xp = exp_reward(10) * level(1) = 10, then
 * scaled by damage: (10 * 46) / HP_max(200) = 460 / 200 = 2. This case asserts
 * the proportional path distinct from the kill case's full-reward path. */
static void test_damage_xp_survive_proportional(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_enemy_data_table, 0,
           sizeof(data_fd2_battle_enemy_data_table));
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].portrait_id = 0x44;        /* enemy idx 0 */
    g_test_rc_array[0].status_flags_block[0] = 1; /* level */
    data_fd2_battle_enemy_data_table[0].exp_reward = 10;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    result = fd2_apply_damage_and_award_xp(0, 50);
    ASSERT_EQ(result, 46);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 154);          /* survives */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 2);        /* (10*46)/200 */
}

/* ================================================================
 * fd2_execute_attack_damage_calculation @ 0x1ECC7 — full-path tests.
 *
 * CORE PHYSICAL COMBAT FORMULA. Drives the REAL accessors compiled into
 * the test build: fd2_get_item_effect_entry / fd2_get_enemy_data_entry
 * (table.c), fd2_get_inventory_slot_item_id / fd2_check_char_status_immunity
 * / fd2_read_tile_attribute_at_pos (battle.c), fd2_advance_rng_state (misc),
 * and the REAL VGA palette routines (palette.c) on the crit/poison branches.
 * Stubs: fd2_find_equipped_item_by_kind -> g_find_equipped_return (slot 0),
 * fd2_delay_ticks -> no-op.
 *
 * RNG is the real ROL16(seed+0x9014,3) LFSR. seed 0 draws (each call returns
 * the NEW seed; values confirmed via emulate_function on fd2_advance_rng_state,
 * which returns 0x80A4 for seed 0):
 *   draw1 = 0x80A4 = 32932  (%100 = 32)
 *   draw2 = 0x85C0 = 34240  (%100 = 40, %4 = 0)
 *   draw3 = 0xAEA0 = 44704  (jitter numerator)
 * Per-hit RNG order: hit-roll (draw1); on hit, crit-roll (draw2, ALWAYS drawn
 * after a hit); on hit with jitter_range!=0, jitter-roll (draw3). weapon_class
 * 2 adds one (poison-roll) draw BEFORE the hit-roll, and a second (duration)
 * draw only if the poison-roll lands.
 *
 * A 768-byte fake VGA palette (g_eatk_pal) backs data_fd2_vga_palette_data_ptr
 * for the crit/poison flash (fd2_set_vga_palette_range_with_add reads
 * pal[idx*3] for idx 0..0xFF). g_eatk_map / g_eatk_attr back the tile-attribute
 * reader for the terrain-bonus test. ---------------------------------------- */
static uint8 g_eatk_pal[768];
static uint8 g_eatk_map[3 * 3 * 4];
static uint8 g_eatk_attr[8];

extern int g_find_equipped_return;

static void eatk_reset(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    memset(data_fd2_battle_enemy_data_table, 0,
           sizeof(data_fd2_battle_enemy_data_table));
    memset(data_fd2_battle_job_crit_rate_table, 0,
           sizeof(data_fd2_battle_job_crit_rate_table));
    g_find_equipped_return = 0;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_last_hit_or_miss_flag = 1;
    data_fd2_battle_pending_xp_credit = 0;
}

/* (a) HIT-vs-MISS boundary. dx_diff = attacker_DX - defender_DX = 0; the hit
 * test is (draw1 % 100 == 32) < dx_diff, i.e. 32 < 0 -> FALSE -> MISS. Only one
 * RNG draw happens (no crit/jitter on a miss). Both combatants immune (job 0x13)
 * so the terrain blocks are skipped; team 1 (npc) so the XP block is skipped.
 * Expect: HP untouched (200), return == 200, last_hit flag stays 1 (MISS). */
static void test_eatk_miss_boundary(void)
{
    int result;
    eatk_reset();
    g_test_rc_array[0].team = 1;          /* attacker: skip XP block */
    g_test_rc_array[0].job_id = 0x13;     /* immune -> no terrain */
    g_test_rc_array[0].dx_current = 10;
    g_test_rc_array[1].job_id = 0x13;     /* immune -> no terrain */
    g_test_rc_array[1].stat4_current = 10;/* dx_diff = 10 - 10 = 0 */
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    g_test_rc_array[1].ap = 0;
    g_test_rc_array[1].dp = 0;
    result = fd2_execute_attack_damage_calculation(0, 1);
    ASSERT_EQ(result, 200);
    ASSERT_EQ(g_test_rc_array[1].hp_current, 200);
    ASSERT_EQ(data_fd2_battle_last_hit_or_miss_flag, 1);   /* MISS */
}

/* (b) HIT, NO crit: damage = (AP-DP)*9/10 + jitter. dx_diff = 100 > 32 -> HIT
 * (last_hit -> 0). crit-roll draw2 % 100 = 40; total_crit = job_crit[job-1] = 0
 * -> 40 < 0 FALSE -> no crit, DP unchanged. AP 110, DP 10 -> base = (100*9)/10 =
 * 90; jitter_range = 90/9 = 10; jitter = draw3 % 10 = 44704 % 10 = 4 -> damage =
 * 94. HP 200 - 94 = 106. Both immune (skip terrain); team 1 (skip XP). */
static void test_eatk_hit_no_crit_damage(void)
{
    int result;
    eatk_reset();
    g_test_rc_array[0].team = 1;
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].ap = 110;
    g_test_rc_array[0].dx_current = 100;
    g_test_rc_array[1].job_id = 0x13;
    g_test_rc_array[1].stat4_current = 0;  /* dx_diff = 100 */
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    result = fd2_execute_attack_damage_calculation(0, 1);
    ASSERT_EQ(result, 106);
    ASSERT_EQ(g_test_rc_array[1].hp_current, 106);
    ASSERT_EQ(data_fd2_battle_last_hit_or_miss_flag, 0);   /* HIT */
}

/* (c) CRIT branch halves defender DP. total_crit = job_crit[job-1] = 50; crit-
 * roll draw2 % 100 = 40 < 50 -> CRIT. DP 20 -> 10 (halved). damage = (110-10)*
 * 9/10 = 90; jitter_range = 10; jitter = 44704 % 10 = 4 -> 94; HP 200 -> 106.
 * The no-crit counterfactual (DP stays 20) would give (110-20)*9/10 = 81, jr =
 * 9, jitter = 44704 % 9 = 1 -> 82, HP 118 — so asserting 106 (not 118) proves
 * the crit DP-halving executed. The crit path also fires the white-flash
 * fd2_set_vga_palette_range_with_add, backed by g_eatk_pal. job 0x13 keeps both
 * immune (no terrain); team 1 (skip XP). */
static void test_eatk_crit_halves_dp(void)
{
    int result;
    uint32 save_pal;
    eatk_reset();
    save_pal = data_fd2_vga_palette_data_ptr;
    memset(g_eatk_pal, 0, sizeof(g_eatk_pal));
    data_fd2_vga_palette_data_ptr = (uint32)g_eatk_pal;
    g_test_rc_array[0].team = 1;
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].ap = 110;
    g_test_rc_array[0].dx_current = 100;
    data_fd2_battle_job_crit_rate_table[0x13 - 1] = 50;  /* total_crit = 50 */
    g_test_rc_array[1].job_id = 0x13;
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].dp = 20;
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    result = fd2_execute_attack_damage_calculation(0, 1);
    ASSERT_EQ(result, 106);
    ASSERT_EQ(g_test_rc_array[1].hp_current, 106);
    data_fd2_vga_palette_data_ptr = save_pal;
}

/* (d) POISON weapon (weapon_class == 2) writes defender status_flags_block[4].
 * Weapon item 0 with special_type(+10) = 2 and poison-chance(+11) = 50. The
 * poison-roll (draw1 % 100 = 32) < 50 -> lands; duration draw2 % 4 = 0 ->
 * status_flags_block[4] = (0)+2 = 2. The poison flash uses g_eatk_pal. Then the
 * hit-roll uses draw3 (44704 % 100 = 4); dx_diff = 0 -> 4 < 0 FALSE -> MISS, so
 * HP is untouched and the poison write is isolated. fd2_get_item_effect_entry
 * returns &item_effect_table[id].type (struct+1), so weapon_entry[9] = struct
 * byte +10 and weapon_entry[10] = struct byte +11. */
static void test_eatk_poison_sets_status(void)
{
    int result;
    uint32 save_pal;
    uint8 *wp;
    eatk_reset();
    save_pal = data_fd2_vga_palette_data_ptr;
    memset(g_eatk_pal, 0, sizeof(g_eatk_pal));
    data_fd2_vga_palette_data_ptr = (uint32)g_eatk_pal;
    wp = (uint8 *)&data_fd2_battle_item_effect_table[0];
    wp[10] = 2;                            /* special_type -> weapon_class 2 */
    wp[11] = 50;                           /* poison chance % */
    g_test_rc_array[0].team = 1;
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].dx_current = 4;
    g_test_rc_array[1].job_id = 0x13;
    g_test_rc_array[1].stat4_current = 4;  /* dx_diff = 0 -> miss */
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].hp_max = 100;
    result = fd2_execute_attack_damage_calculation(0, 1);
    ASSERT_EQ(g_test_rc_array[1].status_flags_block[4], 2);  /* poison 2 */
    ASSERT_EQ(g_test_rc_array[1].hp_current, 100);           /* miss: no dmg */
    ASSERT_EQ(result, 100);
    data_fd2_vga_palette_data_ptr = save_pal;
}

/* (e) XP KILL = full reward. attacker team 2 + defender portrait 0x44 (>=0x44)
 * enters the XP block; enemy index = 0x44-0x44 = 0. Both combatants immune via
 * archetype_flag 4 (portrait != 0x1C) so terrain is skipped without touching
 * job; attacker job 5 (<=8) and char_id 0 (!=0x1C) so the +0x1E level modifier
 * is NOT applied. AP 200, DP 10, no crit (job_crit[4]=0): base = (190*9)/10 =
 * 171; jitter_range = 171/9 = 19; jitter = 44704 % 19 = 16 -> damage = 187.
 * HP 5 - 187 underflows -> floored to 0 -> KILL. KILL takes the HP==0 path so
 * pending_xp = exp_reward(10) * def_level(3) / atk_level(4) = 30/4 = 7 with NO
 * proportional scaling. This exercises the EAX-as-pointer return of
 * fd2_get_enemy_data_entry (asm 0x1EFE8 CALL then 0x1F00C MOVZX [EAX+9]). */
static void test_eatk_xp_kill_full(void)
{
    int result;
    eatk_reset();
    g_test_rc_array[0].team = 2;           /* player attacker */
    g_test_rc_array[0].archetype_flag = 4; /* immune -> no terrain */
    g_test_rc_array[0].portrait_id = 0x10; /* != 0x1C */
    g_test_rc_array[0].char_id = 0;        /* != 0x1C -> no +0x1E */
    g_test_rc_array[0].job_id = 5;         /* <= 8 -> no +0x1E */
    g_test_rc_array[0].status_flags_block[0] = 4;  /* attacker level */
    g_test_rc_array[0].ap = 200;
    g_test_rc_array[0].dx_current = 100;   /* dx_diff = 100 -> hit */
    g_test_rc_array[1].archetype_flag = 4; /* immune -> no terrain */
    g_test_rc_array[1].portrait_id = 0x44; /* enemy idx 0 */
    g_test_rc_array[1].status_flags_block[0] = 3;  /* defender level */
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].hp_max = 200;
    data_fd2_battle_enemy_data_table[0].exp_reward = 10;
    result = fd2_execute_attack_damage_calculation(0, 1);
    ASSERT_EQ(result, 0);                                   /* kill */
    ASSERT_EQ(g_test_rc_array[1].hp_current, 0);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 7);        /* 10*3/4 full */
}

/* (f) XP SURVIVE = proportional reward. Same XP entry as (e) but the defender
 * survives, so pending_xp is scaled by (damage / HP_max). AP 80, DP 10, no crit:
 * base = (70*9)/10 = 63; jitter_range = 63/9 = 7; jitter = 44704 % 7 = 2 ->
 * damage = 65; HP 200 - 65 = 135 (> 0, SURVIVE). full = 10*3/4 = 7; proportional
 * = (7 * 65) / 200 = 455/200 = 2. Asserting 2 (not the full 7) pins the
 * survive-path proportional IDIV distinct from the kill path in (e). */
static void test_eatk_xp_survive_proportional(void)
{
    int result;
    eatk_reset();
    g_test_rc_array[0].team = 2;
    g_test_rc_array[0].archetype_flag = 4;
    g_test_rc_array[0].portrait_id = 0x10;
    g_test_rc_array[0].char_id = 0;
    g_test_rc_array[0].job_id = 5;
    g_test_rc_array[0].status_flags_block[0] = 4;  /* attacker level */
    g_test_rc_array[0].ap = 80;
    g_test_rc_array[0].dx_current = 100;
    g_test_rc_array[1].archetype_flag = 4;
    g_test_rc_array[1].portrait_id = 0x44;
    g_test_rc_array[1].status_flags_block[0] = 3;  /* defender level */
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    data_fd2_battle_enemy_data_table[0].exp_reward = 10;
    result = fd2_execute_attack_damage_calculation(0, 1);
    ASSERT_EQ(result, 135);                                 /* survive */
    ASSERT_EQ(g_test_rc_array[1].hp_current, 135);
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 2);        /* (7*65)/200 */
}

/* (g) TERRAIN AP bonus — locks the fixed tile_id = tile_attr_buf[5] index.
 * Attacker is NON-immune (job 1, archetype 0) so the attacker terrain block
 * runs: fd2_read_tile_attribute_at_pos reads the (1,1) cell of g_eatk_map (width
 * 3); cell+4 sprite_idx word = 0 so attr_ptr = g_eatk_attr+0 and tile_attr_buf
 * [5] = g_eatk_attr[1] = 9 (the tile_id). mv_modifier[9] = 50 -> attacker_AP =
 * 20 + (20*50)/100 = 30. Defender immune (job 0x13) so its DP is unmodified.
 * No crit (job_crit[0]=0). damage = (30-10)*9/10 = 18; jitter_range = 18/9 = 2;
 * jitter = 44704 % 2 = 0 -> 18; HP 200 - 18 = 182. If tile_id were the old
 * uninitialized/garbage value the bonus would differ (e.g. modifier 0 -> AP 20
 * -> damage (20-10)*9/10 = 9, jr 1, jitter 0 -> 9 -> HP 191), so asserting 182
 * locks the tile_attr_buf[5] read AND the 8-byte buffer. team 1 -> skip XP. */
static void test_eatk_terrain_ap_bonus(void)
{
    int result;
    uint32 save_map;
    uint32 save_attr;
    uint32 save_w;
    eatk_reset();
    save_map = data_fd2_battle_tile_map_ptr;
    save_attr = data_fd2_tile_attribute_flags_buffer_ptr;
    save_w = data_fd2_battle_map_width_tiles;
    memset(g_eatk_map, 0, sizeof(g_eatk_map));
    memset(g_eatk_attr, 0, sizeof(g_eatk_attr));
    g_eatk_attr[1] = 9;                    /* tile_id T = 9 (= attr_ptr[1]) */
    data_fd2_battle_tile_map_ptr = (uint32)g_eatk_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_eatk_attr;
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_tile_attr_mv_modifier_table[9] = 50;   /* +50% AP */
    g_test_rc_array[0].team = 1;           /* skip XP */
    g_test_rc_array[0].job_id = 1;         /* NON-immune -> terrain runs */
    g_test_rc_array[0].archetype_flag = 0;
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 1;          /* cell (1,1) -> sprite_idx 0 */
    g_test_rc_array[0].ap = 20;
    g_test_rc_array[0].dx_current = 100;   /* dx_diff = 100 -> hit */
    g_test_rc_array[1].job_id = 0x13;      /* immune -> defender no terrain */
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 200;
    g_test_rc_array[1].hp_max = 200;
    result = fd2_execute_attack_damage_calculation(0, 1);
    ASSERT_EQ(result, 182);
    ASSERT_EQ(g_test_rc_array[1].hp_current, 182);
    data_fd2_battle_tile_map_ptr = save_map;
    data_fd2_tile_attribute_flags_buffer_ptr = save_attr;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_tile_attr_mv_modifier_table[9] = 0;
}

/* ---- Test: magic damage ---- */

static void test_magic_damage_miss(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 1;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 0;
    data_fd2_shared_rng_seed = 100;
    result = fd2_calc_magic_damage(0, 0);
    ASSERT_EQ(result, 0);
}

/* Hit path: hit_rate=100 always lands -> damage formula (50*10)/10=50
 * is applied via fd2_apply_damage_and_award_xp, dropping HP below max
 * and returning the non-zero actual damage. Exercises the branch the
 * miss test never reaches. */
static void test_magic_damage_hit(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].portrait_id = 0x01;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 100;
    data_fd2_shared_rng_seed = 0;
    result = fd2_calc_magic_damage(0, 0);
    ASSERT_TRUE(result != 0);
    ASSERT_TRUE(g_test_rc_array[0].hp_current < 200);
}

/* EAX-bug boundary (deterministic): seed=0 -> first fd2_advance_rng_state
 * returns 0x80A4=32932 -> rng%100=32. The correct emitted form is
 * "(rng%100) >= chance -> miss"; the buggy Ghidra-decompiled form compares
 * chance_pct%100 against itself and would never reflect the RNG roll.
 * hit_rate=33: 32>=33 false -> HIT (result!=0).
 * hit_rate=32: 32>=32 true  -> MISS (result==0).
 * This pair fails unless the RNG value (not chance_pct) drives the compare,
 * so it distinguishes the fix from the decompiler bug. */
static void test_magic_damage_hit_boundary_33(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].portrait_id = 0x01;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 33;
    data_fd2_shared_rng_seed = 0;
    result = fd2_calc_magic_damage(0, 0);
    ASSERT_TRUE(result != 0);
}

static void test_magic_damage_miss_boundary_32(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[0].portrait_id = 0x01;
    data_fd2_battle_job_magic_resist_table[0] = 10;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].hit_rate = 32;
    data_fd2_shared_rng_seed = 0;
    result = fd2_calc_magic_damage(0, 0);
    ASSERT_EQ(result, 0);
}

/* Immunity branch: status spell (id 10, in 10..12) on an immune target
 * (job_id 0x13, portrait != 0x1C) returns 0 before any RNG roll, even
 * with hit_rate=100. Covers the spell_id 10-12 pre-check the miss test
 * skips (spell_id 0). */
static void test_magic_damage_status_immune(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].portrait_id = 0x01;
    g_test_rc_array[0].archetype_flag = 0;
    data_fd2_battle_job_magic_resist_table[0x12] = 10;
    data_fd2_battle_spell_effect_table[10].damage = 50;
    data_fd2_battle_spell_effect_table[10].hit_rate = 100;
    data_fd2_shared_rng_seed = 0;
    result = fd2_calc_magic_damage(0, 10);
    ASSERT_EQ(result, 0);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 200);
}

/* ---- Test: check counter attack ---- */

static void test_counter_attack_sleep(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[1].status_sleep_flag = 1;
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, -1);
}

/* Full-path coverage for fd2_check_can_counter_attack @ 0x1F0DC. Defender is
 * idx 1 (the call is always counter(attacker=0, defender=1)). The weapon-range
 * gate reads weapon_entry[0xB]; fd2_get_item_effect_entry returns &item.type
 * (struct base +1), so weapon_entry[0xB] == item_effect.range_min (struct +0xC).
 * fd2_find_equipped_item_by_kind is the g_find_equipped_return stub; the slot it
 * returns indexes inventory_slots[slot*2+1] (the REAL fd2_get_inventory_slot_item_id),
 * which holds the equipped item id fed to the REAL fd2_get_item_effect_entry.
 *
 * The success path is the load-bearing one: in the binary the returned 1 is NOT
 * an explicit MOV EAX,1 -- it is the fall-through of the MOVZX'd range_min byte
 * that CMP EAX,1 already proved ==1 (shared epilogue at 0x1F17F; the JZ at
 * 0x1F15E for find==-1 lands there with EAX=-1, the fall-through at 0x1F17D with
 * EAX=1). The emit rewrote both to explicit return -1 / return 1, so that
 * equivalence is exercised here. Outcomes are pure integer/branch results
 * derivable from the disasm; no emulate needed. */

/* (a) adjacency fail: awake, non-adjacent (dx+dy = 10 != 1).
 * 0x1F148 CMP EAX,1 / 0x1F14B JNZ 0x1F117 -> EAX=-1. */
static void test_counter_attack_not_adjacent(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 0;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].pos_x = 5;
    g_test_rc_array[1].pos_y = 5;
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, -1);
}

/* (b) no weapon: adjacent, awake, equip lookup returns -1.
 * 0x1F15B CMP EAX,-1 / 0x1F15E JZ 0x1F17F reaches the epilogue with EAX=-1. */
static void test_counter_attack_no_weapon(void)
{
    int result;
    int save_eq;
    save_eq = g_find_equipped_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].pos_x = 0;        /* dx=1, dy=0 -> adjacent */
    g_test_rc_array[1].pos_y = 0;
    g_find_equipped_return = -1;
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, -1);
    g_find_equipped_return = save_eq;
}

/* (c) weapon range != 1 (bow/spear): adjacent, awake, slot 0 holds item 5,
 * item 5 range_min = 2. 0x1F176 MOVZX [EAX+0xB] / 0x1F17A CMP 1 /
 * 0x1F17D JNZ 0x1F117 -> EAX=-1. */
static void test_counter_attack_weapon_range_not_one(void)
{
    int result;
    int save_eq;
    save_eq = g_find_equipped_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].pos_x = 0;
    g_test_rc_array[1].pos_y = 0;
    g_test_rc_array[1].inventory_slots[1] = 5;   /* slot 0 item id = 5 */
    g_find_equipped_return = 0;
    data_fd2_battle_item_effect_table[5].range_min = 2;  /* range != 1 */
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, -1);
    g_find_equipped_return = save_eq;
}

/* (d) SUCCESS, positive delta: adjacent, awake, slot 0 holds item 5 with
 * range_min == 1 (melee). Fall-through to 0x1F17F with EAX=1 -> returns 1.
 * This is the EAX-fall-through equivalence the emit's explicit return 1 claims. */
static void test_counter_attack_success_melee(void)
{
    int result;
    int save_eq;
    save_eq = g_find_equipped_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].pos_x = 0;        /* dx=1, dy=0 -> adjacent */
    g_test_rc_array[1].pos_y = 0;
    g_test_rc_array[1].inventory_slots[1] = 5;
    g_find_equipped_return = 0;
    data_fd2_battle_item_effect_table[5].range_min = 1;  /* melee */
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, 1);
    g_find_equipped_return = save_eq;
}

/* (e) SUCCESS with NEGATIVE delta: attacker pos < defender pos so the SUB
 * underflows (e.g. 4-5 = -1) before abs(). attacker (5,4), defender (5,5):
 * dx=abs(0)=0, dy=abs(-1)=1, sum=1. Proves abs() handles the signed delta;
 * a broken abs would yield a huge sum != 1 and return -1 instead of 1. */
static void test_counter_attack_success_negative_delta(void)
{
    int result;
    int save_eq;
    save_eq = g_find_equipped_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 4;        /* attacker.y < defender.y */
    g_test_rc_array[1].pos_x = 5;
    g_test_rc_array[1].pos_y = 5;        /* SUB y: 4-5 = -1 -> abs -> 1 */
    g_test_rc_array[1].inventory_slots[1] = 5;
    g_find_equipped_return = 0;
    data_fd2_battle_item_effect_table[5].range_min = 1;
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, 1);
    g_find_equipped_return = save_eq;
}

/* ---- Test: heal spell wrapper ---- */

/* The wrapper RETURNS the heal amount (EAX), which its sole caller
 * fd2_dispatch_variant_b_cast feeds into fd2_show_damage_number. The
 * return value must be the inner heal fn's (extra_heal + base_heal_90),
 * not discarded. spell.damage @+0 = 80 -> base_heal = 80. seed 0 ->
 * fd2_advance_rng_state returns 0x80A4 (emulation-confirmed) -> rng%100
 * = 32. base_heal_90 = (80*9)/10 = 72; extra_heal = (32*80)/1000 = 2;
 * return = 74. HP 50 + 72 + 2 = 124 (< max 300, no cap). Exact integer
 * arithmetic. Asserting the EXACT return distinguishes the int contract
 * from the old void declaration (which discarded EAX). */
static void test_heal_spell_to_target(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 300;
    g_test_rc_array[0].portrait_id = 0x50;
    data_fd2_battle_spell_effect_table[3].damage = 80;
    data_fd2_shared_rng_seed = 0;
    result = fd2_apply_heal_spell_to_target(0, 3);
    ASSERT_EQ(result, 74);                          /* 72 + 2 */
    ASSERT_EQ(g_test_rc_array[0].hp_current, 124);  /* 50 + 72 + 2 */
}

/* ---- Test: recompute_runtime_char_total_stats ---- */

static void test_recompute_stats_basic(void)
{
    uint32 buf[0x50 / 4 + 1];
    uint8 *slot;
    memset(buf, 0, sizeof(buf));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)buf;
    slot = (uint8 *)buf;
    *(int16 *)(slot + 0x37) = 20;
    *(int16 *)(slot + 0x39) = 15;
    *(int16 *)(slot + 0x3e) = 10;
    fd2_recompute_runtime_char_total_stats(0);
    ASSERT_EQ(*(uint16 *)(slot + 0x48), 20);
    ASSERT_EQ(*(uint16 *)(slot + 0x4a), 15);
    ASSERT_EQ(*(uint16 *)(slot + 0x4c), 10);
    ASSERT_EQ(*(uint16 *)(slot + 0x4e), 10);
}

/* Exercises the equipped-item summation loop body (the path skipped by
 * test_recompute_stats_basic). Inventory slot 0's marker byte (slot+0xA)
 * gets bit 0x40, id byte (slot+0xB) selects table entry 5. The function
 * calls fd2_get_item_effect_entry (the REAL table.c accessor, returning
 * &item_effect_table[5].type, i.e. struct base +1) and uses the returned
 * pointer for four MOVSX accumulations:
 *   AP    += item[+1] = table[5].ap  (.ap is struct +2 = ptr +1)
 *   DP    += item[+5] = table[5].dp
 *   DX    += item[+3] = table[5].ht
 *   Evade += item[+7] = table[5].ev
 * This covers the EAX-return-value-as-pointer use (000114b7 CALL then
 * 000114bf/c7/cf/d6 MOVSX [EAX+1/5/3/7]) and the branch-taken side of the
 * equipped-marker test (000114ad TEST byte [EAX],0x40). Slot index 0 keeps
 * the marker/id bytes (+0xA/+0xB) clear of the +0x37/0x39/0x3e base stats
 * and the +0x48..0x4f outputs. Exact sums (no emulation needed). */
static void test_recompute_stats_equipped(void)
{
    uint32 buf[0x50 / 4 + 1];
    uint8 *slot;
    memset(buf, 0, sizeof(buf));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)buf;
    slot = (uint8 *)buf;
    *(int16 *)(slot + 0x37) = 20;   /* base AP    */
    *(int16 *)(slot + 0x39) = 15;   /* base DP    */
    *(int16 *)(slot + 0x3e) = 10;   /* base DX (Evade baseline = DX) */
    slot[0xA] = 0x40;               /* inv slot 0: equipped marker  */
    slot[0xB] = 5;                  /* inv slot 0: item id = 5      */
    data_fd2_battle_item_effect_table[5].ap = 3;  /* item[+1] -> AP    */
    data_fd2_battle_item_effect_table[5].ht = 4;  /* item[+3] -> DX    */
    data_fd2_battle_item_effect_table[5].dp = 5;  /* item[+5] -> DP    */
    data_fd2_battle_item_effect_table[5].ev = 6;  /* item[+7] -> Evade */
    fd2_recompute_runtime_char_total_stats(0);
    ASSERT_EQ(*(uint16 *)(slot + 0x48), 23);   /* 20 + 3  */
    ASSERT_EQ(*(uint16 *)(slot + 0x4a), 20);   /* 15 + 5  */
    ASSERT_EQ(*(uint16 *)(slot + 0x4c), 14);   /* 10 + 4  */
    ASSERT_EQ(*(uint16 *)(slot + 0x4e), 16);   /* 10 + 6  */
}

/* ---- Test: recalculate_combat_stats ---- */

static void test_recalc_combat_stats_basic(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 30;
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x12) = 20;
    *(uint16 *)(g_test_rc_array[0].ai_target_and_dx_block + 1) = 15;
    fd2_recalculate_combat_stats(0);
    ASSERT_EQ(g_test_rc_array[0].ap, 30);
    ASSERT_EQ(g_test_rc_array[0].dp, 20);
    ASSERT_EQ(g_test_rc_array[0].dx_current, 15);
    ASSERT_EQ(g_test_rc_array[0].stat4_current, 15);
}

/*
 * AP/DP x1.15 buff path (status_flags_block[1]=AP buff, [2]=DP buff).
 * asm 0x1B7EF-0x1B819: FILD / FMUL m64[0x5018D] / CALL __CHP / FISTP.
 * __CHP (0x377A4) does FRNDINT with RC=11 (round-toward-zero) => TRUNCATION,
 * so the stored result is trunc((double)base * 1.15), NOT round-to-nearest.
 * The m64 constant 0x5018D = 0x3FF2666666666666 = the double 1.15, which is
 * stored as 1.1499999999999999..., so products land just below the integer:
 *   100 * 1.15 = 114.9999... -> trunc 114 (NOT 115)
 *   200 * 1.15 = 229.9999... -> trunc 229 (NOT 230)
 * These two bases are stable: exact-rational (x87 80-bit) and 64-bit double
 * truncation agree, and the product is far from the integer boundary. Bases
 * like 40 (exact 46.0 in 64-bit but 45.9999.. exact -> 45) are deliberately
 * AVOIDED because they straddle the 80-bit/64-bit boundary.
 * DX/stat4 carry NO multiplicative buff, so they stay at their base here.
 */
static void test_recalc_combat_stats_ap_dp_buff(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 100; /* base AP */
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x12) = 200; /* base DP */
    *(uint16 *)(g_test_rc_array[0].ai_target_and_dx_block + 1) = 50; /* base DX */
    g_test_rc_array[0].status_flags_block[1] = 1;  /* AP x1.15 */
    g_test_rc_array[0].status_flags_block[2] = 1;  /* DP x1.15 */
    fd2_recalculate_combat_stats(0);
    ASSERT_EQ(g_test_rc_array[0].ap, 114);          /* trunc(100*1.15) */
    ASSERT_EQ(g_test_rc_array[0].dp, 229);          /* trunc(200*1.15) */
    ASSERT_EQ(g_test_rc_array[0].dx_current, 50);   /* no buff on DX */
    ASSERT_EQ(g_test_rc_array[0].stat4_current, 50);
}

/*
 * DX +0xF buff (status_flags_block[3]). asm 0x1B796: ADD [ESP+4],0xF runs
 * BEFORE stat4 is seeded from dx (0x1B79B: stat4 = dx), so the +0xF must
 * reach BOTH dx_current (+0x4C) and stat4_current/Evade (+0x4E). With no
 * equipped item and no x1.15 buff, base 40 -> 40+15 = 55 in both outputs.
 */
static void test_recalc_combat_stats_dx_buff(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)(g_test_rc_array[0].ai_target_and_dx_block + 1) = 40; /* base DX */
    g_test_rc_array[0].status_flags_block[3] = 1;  /* +0xF DX buff */
    fd2_recalculate_combat_stats(0);
    ASSERT_EQ(g_test_rc_array[0].dx_current, 55);   /* 40 + 0xF */
    ASSERT_EQ(g_test_rc_array[0].stat4_current, 55);/* +0xF reaches stat4 too */
}

/*
 * Equipped-item accumulation slot mapping for THIS function.
 * fd2_get_item_effect_entry returns &item.type (struct +1), so the loop's
 * (pItem+1/+3/+5/+7) read item_effect fields .ap/.ht/.dp/.ev (struct +2/+4/
 * +6/+8) and route them to AP / DX_current / DP / stat4_current respectively.
 * Distinct ht(4) vs ev(6) prove +3 and +7 land in different outputs.
 */
static void test_recalc_combat_stats_equipped_item(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 20; /* base AP */
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x12) = 15; /* base DP */
    *(uint16 *)(g_test_rc_array[0].ai_target_and_dx_block + 1) = 10; /* base DX */
    g_test_rc_array[0].inventory_slots[0] = 0x40;  /* slot 0 equipped */
    g_test_rc_array[0].inventory_slots[1] = 5;     /* item id = 5    */
    data_fd2_battle_item_effect_table[5].ap = 3;   /* item+1 -> AP    */
    data_fd2_battle_item_effect_table[5].ht = 4;   /* item+3 -> DX    */
    data_fd2_battle_item_effect_table[5].dp = 5;   /* item+5 -> DP    */
    data_fd2_battle_item_effect_table[5].ev = 6;   /* item+7 -> Evade */
    fd2_recalculate_combat_stats(0);
    ASSERT_EQ(g_test_rc_array[0].ap, 23);           /* 20 + 3 */
    ASSERT_EQ(g_test_rc_array[0].dp, 20);           /* 15 + 5 */
    ASSERT_EQ(g_test_rc_array[0].dx_current, 14);   /* 10 + 4 */
    ASSERT_EQ(g_test_rc_array[0].stat4_current, 16);/* 10 + 6 */
}

/* ---- Test: check_can_default_attack_target ---- */

static void test_default_attack_sleep(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].status_sleep_flag = 1;
    result = fd2_check_can_default_attack_target(0, 1, 1);
    ASSERT_EQ(result, -1);
}

static void test_default_attack_not_adjacent(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 5;
    result = fd2_check_can_default_attack_target(0, 8, 8);
    ASSERT_EQ(result, -1);
}

/* ---- Test: mp heal ---- */

static void test_mp_heal_basic(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].mp_current = 10;
    g_test_rc_array[0].mp_max = 100;
    g_test_rc_array[0].portrait_id = 0x01;
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    result = fd2_apply_mp_heal_and_award_xp(0, 50);
    ASSERT_TRUE(result >= 45);
    ASSERT_TRUE(g_test_rc_array[0].mp_current > 10);
}

static void test_mp_heal_cap_at_max(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].mp_current = 95;
    g_test_rc_array[0].mp_max = 100;
    g_test_rc_array[0].portrait_id = 0x01;
    data_fd2_shared_rng_seed = 0;
    result = fd2_apply_mp_heal_and_award_xp(0, 500);
    ASSERT_EQ(g_test_rc_array[0].mp_current, 100);
}

/* MP XP-credit path (the two MP tests above leave status_flags_block[0]=0, so
 * their XP block computes 0 and verifies nothing). portrait 0x01 < 0x4b -> the
 * 0x1ca6d JGE is NOT taken, so the XP block runs. Deterministic (seed 0 ->
 * fd2_advance_rng_state returns 0x80A4 = 32932, emulation-confirmed; %100 = 32):
 *   base_heal_90 = (50*9)/10 = 45
 *   extra_heal   = (32*50)/1000 = 1
 *   mp_after     = 10 + 45 + 1 = 56  (<= max 100, no cap)
 *   mp_gained    = 56 - 10 = 46
 *   return       = extra_heal + base_heal_90 = 1 + 45 = 46
 * The XP epilogue is SHARED with hp_heal via tail-JMP 0x1c9c7, but the BODY
 * differs in two MP-specific ways this asserts:
 *   - multiplier = status_flags_block[0]*0x28 (asm 0x1ca73-0x1ca7e: level*0x28),
 *     with NO +0x1e job modifier (the HP version's 0x1c9a4-0x1c9ae job branch is
 *     absent from the MP body);
 *   - divisor = mp_max (asm 0x1ca0c pushes [ESI+0x46]=wMP_max into [ESP], which
 *     the shared 0x1c9cc IDIV [ESP] consumes -- it BORROWS the hp_max slot but
 *     the value is mp_max, not hp_max).
 *   pending_xp += (level 5 * 0x28 * 46) / mp_max 100
 *              = (5*40*46)/100 = 9200/100 = 92
 * Asserting EXACT pending_xp_credit pins the *0x28 multiplier and the
 * mp_max(borrowed-slot) divisor; the companion below pins the no-job-modifier
 * behavior. */
static void test_mp_heal_xp_credit(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].mp_current = 10;
    g_test_rc_array[0].mp_max = 100;
    g_test_rc_array[0].portrait_id = 0x01;          /* < 0x4b -> XP runs */
    g_test_rc_array[0].status_flags_block[0] = 5;   /* level */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    result = fd2_apply_mp_heal_and_award_xp(0, 50);
    ASSERT_EQ(result, 46);                            /* 45 + 1 */
    ASSERT_EQ(g_test_rc_array[0].mp_current, 56);     /* 10 + 45 + 1 */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 92); /* (5*40*46)/100 */
}

/* Companion to test_mp_heal_xp_credit proving the MP body has NO +0x1e mid-tier
 * job modifier (unlike the HP sibling, whose body adds 0x1e when job_id is in
 * [9,24]). Here job_id 10 IS in [9,24], yet the MP body never reads job_id
 * (+0x20) -- it goes straight from status_flags_block[0] (+0x21) to the *0x28
 * multiply. So with everything else identical to the test above, the XP credit
 * stays 92. If the MP emit ever grew the HP version's +0x1e branch, level_mod
 * would become 5+0x1e = 35 and this would read (35*40*46)/100 = 644 and fail. */
static void test_mp_heal_xp_no_job_modifier(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].mp_current = 10;
    g_test_rc_array[0].mp_max = 100;
    g_test_rc_array[0].portrait_id = 0x01;          /* < 0x4b -> XP runs */
    g_test_rc_array[0].status_flags_block[0] = 5;   /* level */
    g_test_rc_array[0].job_id = 10;                 /* in [9,24], but ignored */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_pending_xp_credit = 0;
    result = fd2_apply_mp_heal_and_award_xp(0, 50);
    ASSERT_EQ(result, 46);                            /* 45 + 1 */
    ASSERT_EQ(g_test_rc_array[0].mp_current, 56);     /* 10 + 45 + 1 */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 92); /* STILL 92, no +0x1e */
}

/* ---- Test: combat bubble pos ---- */

static void test_stat_preview_basic(void)
{
    int32 stats[4];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    fd2_compute_equipped_stats_with_item_preview(0, 0, (uint32)stats);
    ASSERT_EQ(stats[0], 0);
    ASSERT_EQ(stats[1], 0);
}

static void test_combat_bubble_pos_facing_down(void)
{
    int32 xy[2];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].sprite_state[1] = 0;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    fd2_compute_combat_bubble_screen_pos((uint32)xy, 0);
    ASSERT_EQ(xy[0], 5 * 0x18 + 4 + 0x1C);
    ASSERT_EQ(xy[1], 5 * 0x18 - 0x12);
}

/* ---- Test: flash_char_hit_sprite ---- */

static void test_combat_hit_outcome_zero_stats(void)
{
    uint32 outcome[6];
    uint8 fake_map[16];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    memset(fake_map, 0, sizeof(fake_map));
    data_fd2_battle_tile_map_ptr = (uint32)fake_map;
    data_fd2_battle_map_width_tiles = 1;
    g_test_rc_array[0].job_id = 1;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].hp_max = 100;
    data_fd2_shared_rng_seed = 0;
    fd2_calculate_combat_hit_outcome(0, 1, outcome);
    ASSERT_EQ(outcome[5], 0);
}

static void test_flash_char_hit_enemy(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = TEAM_ENEMY;
    data_fd2_chapter_current_chapter_id = 1;
    fd2_flash_char_hit_sprite(0, 0);
    ASSERT_TRUE(1);
}

/* ---- Test: check_char_status_immunity ---- */

static void test_face_toward_target_down(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[1].pos_x = 5; g_test_rc_array[1].pos_y = 8;
    fd2_face_char_toward_target(0, 1);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 0);
}

static void test_face_toward_target_left(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[1].pos_x = 2; g_test_rc_array[1].pos_y = 5;
    fd2_face_char_toward_target(0, 1);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 1);
}

/* abs_dx(0) <= abs_dy(3) -> vertical; target.y(2) < actor.y(5) -> up=2 */
static void test_face_toward_target_up(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[1].pos_x = 5; g_test_rc_array[1].pos_y = 2;
    fd2_face_char_toward_target(0, 1);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 2);
}

/* abs_dx(3) > abs_dy(0) -> horizontal; actor.x(5) !< target.x(8) -> right=3 */
static void test_face_toward_target_right(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[1].pos_x = 8; g_test_rc_array[1].pos_y = 5;
    fd2_face_char_toward_target(0, 1);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 3);
}

/* abs_dx(3) == abs_dy(3) tie -> vertical preferred; target.y !< actor.y -> down=0 */
static void test_face_toward_target_tie_prefers_vertical(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[1].pos_x = 8; g_test_rc_array[1].pos_y = 8;
    fd2_face_char_toward_target(0, 1);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 0);
}

static void test_immunity_job_0x13(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].job_id = 0x13;
    ASSERT_EQ(fd2_check_char_status_immunity(0), 1);
}

static void test_immunity_portrait_0x1c_overrides(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].portrait_id = 0x1C;
    ASSERT_EQ(fd2_check_char_status_immunity(0), 0);
}

static void test_immunity_archetype_4(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].archetype_flag = 4;
    ASSERT_EQ(fd2_check_char_status_immunity(0), 1);
}

static void test_no_immunity_normal(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].job_id = 5;
    ASSERT_EQ(fd2_check_char_status_immunity(0), 0);
}

/* ---- Test: AI dispatcher ---- */

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
extern int g_find_equipped_return;
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

static uint8 t_ai_tile_map[20 * 15 * 4];
static uint8 t_ai_attr_buf[8];

static void reset_ai_stubs(void)
{
    g_composite_call_count = 0;
    g_attack_dispatch_return = 0;
    g_attack_dispatch_calls = 0;
    g_seek_optimal_return = 0;
    g_advance_nearest_return = 0;
    g_walk_return = 0;
    g_score_physical_return = 0;
    g_pass_turn_calls = 0;
    g_execute_spell_calls = 0;
    g_execute_physical_calls = 0;
    g_pathfind_return = 0;
    g_pathfind_walk_return = 0;
    g_pathfind_write_dst = 0;
    g_pathfind_dst_x = 0;
    g_pathfind_dst_y = 0;
    g_pathfind_seq_enable = 0;
    g_pathfind_seq[0] = 0; g_pathfind_seq[1] = 0;
    g_pathfind_seq[2] = 0; g_pathfind_seq[3] = 0;
    g_pathfind_seq_idx = 0;
    g_pathfind_seq_steps = 0;
    memset(g_pathfind_step_bytes, 0, sizeof(g_pathfind_step_bytes));
    g_pathfind_md0_dst_x = -1;
    g_pathfind_md0_dst_y = -1;
    memset(t_ai_tile_map, 0xFF, sizeof(t_ai_tile_map));
    data_fd2_battle_tile_map_ptr = (uint32)t_ai_tile_map;
}

static void test_ai_dispatch_early_exit_dead(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].flags = 0x01;
    fd2_enemy_turn_action_dispatcher(0, 0);
    ASSERT_EQ(g_composite_call_count, 0);
}

static void test_ai_dispatch_case0_runs(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].combat_aux_block[0xD] = 0;
    fd2_enemy_turn_action_dispatcher(0, 0);
    ASSERT_TRUE(g_composite_call_count >= 1);
}

static void test_ai_dispatch_case8_skip(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].combat_aux_block[0xD] = 8;
    fd2_enemy_turn_action_dispatcher(0, 0);
    ASSERT_EQ(g_composite_call_count, 0);
}

static void test_ai_dispatch_case11_both_execute(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].combat_aux_block[0xD] = 11;
    data_fd2_battle_ai_best_spell_score = 7;
    data_fd2_battle_ai_best_physical_score = 7;
    fd2_enemy_turn_action_dispatcher(0, 0);
    data_fd2_battle_ai_best_spell_score = 0;
    data_fd2_battle_ai_best_physical_score = 0;
}

static void test_ai_pass_turn_hp_full(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 200;
    g_test_rc_array[0].hp_max = 200;
    result = fd2_ai_pass_turn_with_heal(0);
    ASSERT_EQ(result, 0);
}

static void test_ai_pass_turn_poisoned(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 200;
    g_test_rc_array[0].status_flags_block[4] = 1;
    result = fd2_ai_pass_turn_with_heal(0);
    ASSERT_EQ(result, 0);
}

static void test_ai_pass_turn_heals_20pct(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].hp_current = 100;
    g_test_rc_array[0].hp_max = 200;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    result = fd2_ai_pass_turn_with_heal(0);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 140);
}

static void test_ai_pass_turn_clamps_at_max(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].hp_current = 195;
    g_test_rc_array[0].hp_max = 200;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    result = fd2_ai_pass_turn_with_heal(0);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(g_test_rc_array[0].hp_current, 200);
}

/* Genuine unreachable: the seek's "find optimal cell" pathfind (md==2)
 * returns 0xFF -> the function must return 0 and leave anim_phase untouched
 * (the no-anim early-out at btl_ai.c). reset_ai_stubs() sets g_pathfind_return
 * to a deterministic value, so here we force 0xFF explicitly. */
static void test_ai_seek_optimal_unreachable(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_pathfind_return = 0xFF;
    data_fd2_battle_anim_phase = 7;     /* sentinel: must stay untouched */
    result = fd2_ai_seek_optimal_position(0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 7);
}

/* Already at optimum: pathfind reports a destination equal to the source
 * (dst==src) -> no walk, no animation, return 0, anim_phase untouched. */
static void test_ai_seek_optimal_already_at_best(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_pathfind_return = 3;              /* reachable (not 0xFF) */
    g_pathfind_write_dst = 1;
    g_pathfind_dst_x = 5;               /* dst == src */
    g_pathfind_dst_y = 5;
    data_fd2_battle_anim_phase = 7;     /* sentinel: must stay untouched */
    result = fd2_ai_seek_optimal_position(0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 7);
}

/* EAX-tracking lock-in: did_move must derive from the WALK return value
 * (asm 0x1421d TEST EAX,EAX on the fd2_ai_walk_to_target_tile CALL at 0x14215),
 * NOT from the pathfind result that the buggy decompiled C reinterprets (iVar1).
 * Setup: the seek's md==2 pathfind returns NONZERO (3) and reports dst!=src, so
 * the buggy "did_move = pathfind_result != 0" would yield 1; the walk routine's
 * own pathfinds (md==0/1) return 0 so the real walk returns 0. Correct semantics
 * => did_move = (walk_result==0) => result 0. Walk returning 0 also means no
 * animation pipeline runs, keeping this case fully deterministic. anim_phase is
 * driven 0 then 1 across the walk branch, so it ends at 1. */
static void test_ai_seek_optimal_walk_branch_returns_zero(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_pathfind_return = 3;              /* seek pathfind: reachable, nonzero */
    g_pathfind_write_dst = 1;
    g_pathfind_dst_x = 8;              /* dst != src -> enters walk branch */
    g_pathfind_dst_y = 8;
    g_pathfind_walk_return = 0;         /* walk routes -> walk returns 0 */
    data_fd2_battle_anim_phase = 7;
    result = fd2_ai_seek_optimal_position(0, 0);
    ASSERT_EQ(result, 0);              /* fails on buggy iVar1-based did_move */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
}

static void test_ai_advance_no_target(void)
{
    int result;
    uint32 save_pmc;
    save_pmc = data_fd2_battle_party_member_count;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    data_fd2_battle_party_member_count = 2;
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].pos_x = 8; g_test_rc_array[1].pos_y = 5;
    g_test_rc_array[1].team = 0;
    result = fd2_ai_advance_to_nearest_team_target(0, 0);
    ASSERT_EQ(result, 0);
    data_fd2_battle_party_member_count = save_pmc;
}

static void test_ai_advance_target_found(void)
{
    int result;
    uint32 save_pmc;
    save_pmc = data_fd2_battle_party_member_count;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    data_fd2_battle_party_member_count = 2;
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].pos_x = 8; g_test_rc_array[1].pos_y = 5;
    g_test_rc_array[1].team = 2;
    g_pathfind_return = 0;
    result = fd2_ai_advance_to_nearest_team_target(0, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
    data_fd2_battle_party_member_count = save_pmc;
}

static void test_attack_dispatch_all_low(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    result = fd2_attack_action_dispatch(0, 0);
    ASSERT_EQ(result, 0);
}

static void test_ai_walk_no_path(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 5;
    g_pathfind_return = 0;
    result = fd2_ai_walk_to_target_tile(8, 8, 0, 0);
    ASSERT_EQ(result, 0);
}

/* ---- High-risk coverage for fd2_ai_walk_to_target_tile @ 0x14B78 ----
 *
 * The two tests below drive the routine's two unasserted high-risk regions:
 * the candidate-selection loop with its abs()-chain + taxi/diag tiebreak
 * (asm 0x14dc5-0x14e56) and the Stage B furthest-passable step-decode scan
 * (asm 0x14c4d-0x14d47). Both keep the FINAL md==0 route returning 0 so the
 * REAL fd2_walk_path_animation_loop never runs -> fully deterministic, return 0.
 *
 * On the test path fd2_collect_unmarked_tile_positions and
 * fd2_mark_char_occupant_tiles_for_team are the REAL btl_ai.c functions, and
 * fd2_check_char_status_immunity / fd2_get_movement_cost_table_for_job are REAL.
 * fd2_pathfind_to_destination, fd2_init_movement_range_floodfill,
 * fd2_obfuscate_battle_tile_map and fd2_paint_threat_overlay_for_team are stubs.
 * party_member_count is pinned to 1 so the occupant-mark pass (which excludes the
 * mover, char 0) marks nothing and cannot pollute the candidate tiles. The
 * non-0xFF cells of t_ai_tile_map (byte (y*width + x)*4 + 7) are exactly the
 * candidate / walkable tiles the REAL collection + scan loops read. char 0 is
 * memset-zero: portrait/job/archetype 0 -> non-immune; char_id(+8) 0 != 0x1c, so
 * the movement class stays job_id 0 (no override). The chosen "best adjacent
 * tile" is observed via g_pathfind_md0_dst_x/y (the stub records the LAST md==0
 * destination, which is the routine's final route target). Expected winners are
 * enumerated directly from the verbatim taxi/diag formula (identical in disasm
 * and decompiler): taxi=|dx|+|dy|, diag=||dx|-|dy||, update when taxi<best_taxi
 * OR (taxi==best_taxi AND diag<best_diag), best_taxi/best_diag seeded 0xFF. */

/* Candidate-selection loop + abs-chain + taxi/diag tiebreak. Stage A md==0
 * returns 0 (reset default) != 0xFF, so Stage B is skipped and target stays the
 * passed (4,0). Two cells are unmarked -> 2 candidates (row-major order):
 *   cand0 (0,0): dx=0-4=-4, dy=0-0=0 -> taxi 4, diag |4-0|=4
 *   cand1 (2,2): dx=2-4=-2, dy=2-0= 2 -> taxi 4, diag |2-2|=0
 * cand0 seeds best (taxi 4 < 0xFF). cand1 ties on taxi (4==4) and wins the diag
 * tiebreak (0 < 4) -> best becomes (2,2). This locks the negative-delta abs
 * handling, the |dx|-|dy| diag term, and the tiebreak firing on a LATER
 * candidate. The final md==0 route then targets (2,2). */
static void test_ai_walk_candidate_taxi_tiebreak(void)
{
    int result;
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_party_member_count = 1;     /* only the (excluded) mover */
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    g_test_rc_array[0].pos_x = 0;
    g_test_rc_array[0].pos_y = 0;
    /* unmark the two candidate cells: (0,0) and (2,2), width 5 */
    t_ai_tile_map[(0 * 5 + 0) * 4 + 7] = 0;
    t_ai_tile_map[(2 * 5 + 2) * 4 + 7] = 0;
    g_pathfind_walk_return = 0;                 /* Stage A !=0xFF, final route 0 */
    result = fd2_ai_walk_to_target_tile(4, 0, 0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)g_pathfind_md0_dst_x, 2);   /* tiebreak winner (2,2) */
    ASSERT_EQ((long)g_pathfind_md0_dst_y, 2);
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
}

/* Stage B furthest-passable step-decode scan. Sequenced pathfind:
 *   call 0 (Stage A, md==0) -> 0xFF  -> forces Stage B
 *   call 1 (Stage B, md==1) -> 5     -> 5 step bytes written into the path buf
 *   call 2 (final,   md==0) -> 0     -> no walk animation, deterministic
 * src = char 0 at (0,0). Step bytes [3,0,3,2,1] exercise all four decode
 * branches (0:S y++, 1:W x--, 2:N y--, 3:E x++) and trace:
 *   step0 E -> (1,0)   step1 S -> (1,1)   step2 E -> (2,1)
 *   step3 N -> (2,0)   step4 W -> (1,0)
 * Walkable cells (+7 != 0xFF) are (1,1) and (2,0). The scan keeps the FURTHEST
 * (last-visited) walkable tile: (1,1) at step1 then overridden by (2,0) at step3;
 * step4 (1,0) is 0xFF so best stays (2,0). Stage B therefore sets target=(2,0).
 * The candidate loop then collects the same two unmarked cells (row-major: (2,0)
 * at y=0, (1,1) at y=1); (2,0) has taxi 0 to the Stage B target (2,0) and wins,
 * so the final route targets (2,0). Asserting the final md==0 dst == (2,0) proves
 * Stage B was entered, all four step decodes ran, the furthest-walkable override
 * works, and the result propagated into the final route. */
static void test_ai_walk_stage_b_furthest_tile(void)
{
    int result;
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_party_member_count = 1;     /* only the (excluded) mover */
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    g_test_rc_array[0].pos_x = 0;
    g_test_rc_array[0].pos_y = 0;
    /* walkable cells visited by the scan: (1,1) then (2,0) */
    t_ai_tile_map[(1 * 5 + 1) * 4 + 7] = 0;
    t_ai_tile_map[(0 * 5 + 2) * 4 + 7] = 0;
    g_pathfind_seq_enable = 1;
    g_pathfind_seq[0] = 0xFF;                   /* Stage A unreachable */
    g_pathfind_seq[1] = 5;                      /* Stage B: 5 steps */
    g_pathfind_seq[2] = 0;                      /* final route: no walk */
    g_pathfind_seq_steps = 5;
    g_pathfind_step_bytes[0] = 3;               /* E -> (1,0) */
    g_pathfind_step_bytes[1] = 0;               /* S -> (1,1) walkable */
    g_pathfind_step_bytes[2] = 3;               /* E -> (2,1) */
    g_pathfind_step_bytes[3] = 2;               /* N -> (2,0) walkable */
    g_pathfind_step_bytes[4] = 1;               /* W -> (1,0) */
    result = fd2_ai_walk_to_target_tile(9, 9, 0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)g_pathfind_md0_dst_x, 2);   /* furthest walkable (2,0) */
    ASSERT_EQ((long)g_pathfind_md0_dst_y, 0);
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
}

static void test_compute_aoe_no_targets(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    data_fd2_battle_party_member_count = 2;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 3; g_test_rc_array[1].pos_y = 3;
    result = fd2_compute_aoe_targets(5, 5, 0, 2, 0, 0);
    ASSERT_EQ(result, 0);
    data_fd2_battle_party_member_count = 4;
}

/* ---- Direct coverage for the two high-risk marking modes of
 *      fd2_compute_aoe_targets @ 0x14818 ----
 *
 * fd2_init_movement_range_floodfill is a no-op stub (testglob.c), so in Mode 1
 * the floodfill leaves the tile map exactly as the test left it; this lets each
 * test pin a known +7-overlay state before the radius/cross marking runs.
 * fd2_get_movement_cost_table_for_job is the REAL table.c accessor (its returned
 * pointer is only consumed by the stubbed floodfill, so the value is irrelevant).
 * All expected values are hand-derived from the disassembly trace (the +7 byte of
 * each cell at (y*width + x)*4 + 7 is the marker the collection loop reads). */

/* Mode 2 (spell_range >= 0x10): orthogonal cross. asm 0x148c7-0x1493e.
 * extent = spell_range - 0x10. The X-stripe clears +7 along row center_y for
 * |col-center_x| <= extent; the Y-stripe clears +7 along col center_x for
 * |row-center_y| <= extent. Map starts all-0xFF, so the cleared (0) plus-shape
 * tiles are the only includable ones. Call (2,2, buf, 0x11, 0, 0): extent 1 ->
 * plus = {(1,2),(2,2),(3,2),(2,1),(2,3)}. team_filter 0 collects team==0 chars
 * whose tile is cleared. char0 (3,2) lies on the X arm, char1 (2,3) on the Y arm
 * -> both collected (covers both stripes + the sequential out_buf writes at
 * count 0 and 1); char2 (0,0) is outside the plus -> excluded. */
static void test_compute_aoe_mode2_cross(void)
{
    int result;
    uint8 buf[8];
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();                  /* tile map -> 0xFF, ptr wired */
    memset(buf, 0xAA, sizeof(buf));
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    data_fd2_battle_party_member_count = 3;
    g_test_rc_array[0].team = 0; g_test_rc_array[0].pos_x = 3; g_test_rc_array[0].pos_y = 2;
    g_test_rc_array[1].team = 0; g_test_rc_array[1].pos_x = 2; g_test_rc_array[1].pos_y = 3;
    g_test_rc_array[2].team = 0; g_test_rc_array[2].pos_x = 0; g_test_rc_array[2].pos_y = 0;
    result = fd2_compute_aoe_targets(2, 2, (uint32)buf, 0x11, 0, 0);
    ASSERT_EQ(result, 2);
    ASSERT_EQ((long)buf[0], 0);
    ASSERT_EQ((long)buf[1], 1);
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_party_member_count = save_pmc;
}

/* Mode 1 (spell_range < 0x10) with aoe_radius != 0: manhattan bubble.
 * asm 0x1486a-0x148c5. After the (stubbed, no-op) floodfill the map is whatever
 * the test set it to; here it is memset to 0x00 so every tile is includable
 * (+7 == 0). The radius block then re-marks +7 = 0xFF for every cell with
 * manhattan((col,row),(center)) < aoe_radius. Call (2,2, buf, 2, 2, 0): radius 2
 * -> bubble (manhattan 0 or 1) = {(2,2),(1,2),(3,2),(2,1),(2,3)}. char0 team0 at
 * (2,2) is inside the bubble -> +7 == 0xFF -> excluded; char1 team0 at (0,0) has
 * manhattan 4 (not < 2) -> +7 stays 0 -> collected, out_buf[0] = 1. Pins the
 * abs()+manhattan bubble AND the *(out_buf+count)=ci write for this path. */
static void test_compute_aoe_mode1_radius_bubble(void)
{
    int result;
    uint8 buf[8];
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    memset(t_ai_tile_map, 0, sizeof(t_ai_tile_map));   /* includable baseline */
    memset(buf, 0xAA, sizeof(buf));
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    data_fd2_battle_party_member_count = 2;
    g_test_rc_array[0].team = 0; g_test_rc_array[0].pos_x = 2; g_test_rc_array[0].pos_y = 2;
    g_test_rc_array[1].team = 0; g_test_rc_array[1].pos_x = 0; g_test_rc_array[1].pos_y = 0;
    result = fd2_compute_aoe_targets(2, 2, (uint32)buf, 2, 2, 0);
    ASSERT_EQ(result, 1);
    ASSERT_EQ((long)buf[0], 1);
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_party_member_count = save_pmc;
}

/* team_filter branches 1/2/3 of the collection loop (asm 0x149a1/0x149b0/0x149c0).
 * Isolate the team filter from the marking: Mode 1, aoe_radius 0 (no bubble), map
 * memset to 0x00 so the (no-op) floodfill leaves every tile includable -> every
 * alive char qualifies on the tile test and only the team predicate decides.
 *   char0 team0, char1 team1, char2 team2 (all alive, distinct tiles).
 *   team_filter 1 (any ally, team != 0) -> {char1,char2}: result 2, buf 1 then 2.
 *   team_filter 2 (NPC ally, team == 1) -> {char1}:       result 1, buf[0] 1.
 *   team_filter 3 (player,   team == 2) -> {char2}:       result 1, buf[0] 2. */
static void test_compute_aoe_team_filter_branches(void)
{
    int result;
    uint8 buf[8];
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    memset(t_ai_tile_map, 0, sizeof(t_ai_tile_map));   /* all tiles includable */
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    data_fd2_battle_party_member_count = 3;
    g_test_rc_array[0].team = 0; g_test_rc_array[0].pos_x = 0; g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].team = 1; g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 0;
    g_test_rc_array[2].team = 2; g_test_rc_array[2].pos_x = 2; g_test_rc_array[2].pos_y = 0;

    memset(buf, 0xAA, sizeof(buf));
    result = fd2_compute_aoe_targets(0, 0, (uint32)buf, 2, 0, 1);
    ASSERT_EQ(result, 2);
    ASSERT_EQ((long)buf[0], 1);
    ASSERT_EQ((long)buf[1], 2);

    memset(buf, 0xAA, sizeof(buf));
    result = fd2_compute_aoe_targets(0, 0, (uint32)buf, 2, 0, 2);
    ASSERT_EQ(result, 1);
    ASSERT_EQ((long)buf[0], 1);

    memset(buf, 0xAA, sizeof(buf));
    result = fd2_compute_aoe_targets(0, 0, (uint32)buf, 2, 0, 3);
    ASSERT_EQ(result, 1);
    ASSERT_EQ((long)buf[0], 2);

    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_party_member_count = save_pmc;
}

static void test_ai_score_phys_no_weapon(void)
{
    int result;
    int save_eq;
    save_eq = g_find_equipped_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_find_equipped_return = -1;
    data_fd2_battle_ai_best_physical_score = 99;
    result = fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 0);
    g_find_equipped_return = save_eq;
}

/* ---- Full scoring-path tests for fd2_ai_score_physical_attack @ 0x14237 ----
 *
 * Drive the real per-tile / per-target scoring loop end to end. All callees on
 * the path are the REAL functions: fd2_get_inventory_slot_item_id /
 * fd2_get_item_effect_entry / fd2_get_movement_cost_table_for_job (table.c),
 * fd2_check_char_status_immunity / fd2_read_tile_attribute_at_pos /
 * fd2_check_can_default_attack_target (battle.c), and
 * fd2_mark_char_occupant_tiles_for_team / fd2_collect_unmarked_tile_positions /
 * fd2_compute_aoe_targets (btl_ai.c). Only fd2_find_equipped_item_by_kind,
 * fd2_init_movement_range_floodfill, fd2_paint_threat_overlay_for_team and
 * fd2_obfuscate_battle_tile_map are stubs (the equip stub via
 * g_find_equipped_return; the rest are no-ops that leave the tile map intact).
 *
 * Geometry (shared by ti_setup): 3x3 map, party_member_count=2.
 *   char 0 = caster, team 0 (TEAM_ENEMY), pos (0,0), non-immune.
 *   char 1 = target, team 2 (player), pos (1,1).
 * Only tile (1,1) is left unmarked (+7 != 0xFF); every other tile stays 0xFF
 * from reset_ai_stubs. The caster-occupant pass marks nothing (exclude_idx==0
 * skips the caster; char 1 is team 2 so the team_selector==0 pass ignores it),
 * so the candidate list is exactly [(1,1)]. ctx_flag==0 -> use_smaller_aoe=1 ->
 * fd2_compute_aoe_targets runs with team_filter==1, collecting char 1 (team!=0)
 * because its tile (1,1) is unmarked. aoe_radius (item range_min, +0xB) is 0 so
 * no radius re-marking; spell_range (item range_max, +0xC) is 1 (<0x10).
 * Both attacker and target are non-immune, so terrain modifiers are skipped and
 * effective_AP/DP == base AP/DP. With the single candidate equal to the target
 * tile (Manhattan distance 0) the counter-bonus default-attack check returns -1
 * (dist != 1), isolating raw_damage = AP - DP for tests 1-3.
 *
 * raw_damage paths (verified against disasm 0x14586/0x1458c kill at 0x1447b):
 *   raw <= 2            -> score_class 0
 *   raw  > 2            -> score_class 8
 *   raw  > target HP    -> raw *= 2, score_class 0x12  (kill-shot)
 * Best-slot update fires when score_class > best OR (== best && raw > tiebreak);
 * both start at 0. */
static void ti_setup_phys(uint32 *save_pmc, uint32 *save_w,
                          uint32 *save_h, int *save_eq)
{
    *save_pmc = data_fd2_battle_party_member_count;
    *save_w = data_fd2_battle_map_width_tiles;
    *save_h = data_fd2_battle_map_height_tiles;
    *save_eq = g_find_equipped_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    reset_ai_stubs();                 /* tile map -> 0xFF, ptr wired */
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;
    g_find_equipped_return = 0;        /* slot 0 valid for all chars */

    g_test_rc_array[0].pos_x = 0;      /* caster */
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].inventory_slots[1] = 5;   /* equipped item id 5 */

    g_test_rc_array[1].pos_x = 1;      /* target on the single candidate */
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].team = 2;

    /* item 5: range_min(+0xB)=0 -> aoe_radius 0; range_max(+0xC)=1 -> range 1 */
    data_fd2_battle_item_effect_table[5].range_min = 0;
    data_fd2_battle_item_effect_table[5].range_max = 1;

    /* unmark tile (1,1) only: byte (y*w + x)*4 + 7 with w=3 */
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
}

static void ti_restore_phys(uint32 save_pmc, uint32 save_w,
                            uint32 save_h, int save_eq)
{
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    g_find_equipped_return = save_eq;
}

/* Normal hit: AP 20, DP 10 -> raw 10 (>2 -> class 8), HP 100 (no kill),
 * char_id != 0 (no flank), distance 0 (no counter). Best slots take the
 * single candidate (1,1) targeting char 1. */
static void test_ai_score_phys_normal_hit_score8(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_eq;
    ti_setup_phys(&save_pmc, &save_w, &save_h, &save_eq);
    g_test_rc_array[0].ap = 20;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].char_id = 7;          /* non-leader: no flank */
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 1);
    ti_restore_phys(save_pmc, save_w, save_h, save_eq);
}

/* Kill shot: AP 20, DP 10 -> raw 10 > HP 5 -> raw doubled, class 0x12. */
static void test_ai_score_phys_kill_shot_score12(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_eq;
    ti_setup_phys(&save_pmc, &save_w, &save_h, &save_eq);
    g_test_rc_array[0].ap = 20;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].char_id = 7;
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 0x12);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 1);
    ti_restore_phys(save_pmc, save_w, save_h, save_eq);
}

/* Negligible damage: AP 11, DP 10 -> raw 1 (<=2 -> class 0), no kill. Score
 * class stays 0 but the (score==best && raw>tiebreak) branch still writes the
 * target slots once (raw 1 > initial tiebreak 0). Covers the class-0 path and
 * the tiebreak-only update. */
static void test_ai_score_phys_negligible_score0(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_eq;
    ti_setup_phys(&save_pmc, &save_w, &save_h, &save_eq);
    g_test_rc_array[0].ap = 11;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].char_id = 7;
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 1);
    ti_restore_phys(save_pmc, save_w, save_h, save_eq);
}

/* Counter-bonus + flank: two unmarked candidate tiles (1,0) and (1,1); target
 * char 1 (leader: char_id 0, awake) sits on (1,1). For candidate (1,0) the
 * target is Manhattan-distance 1 away, so fd2_check_can_default_attack_target
 * returns 1 (awake, adjacent, weapon range_min 0<=1) and adds effective_DP -
 * target_AP; then the leader flank multiplies raw by 3/2. Candidate (1,1) has
 * distance 0 (no counter) and a lower final raw, so it loses the score-8
 * tiebreak. Best slot must therefore be (1,0).
 *   AP 30, DP(target) 10 -> raw 20 (class 8, 20<=HP100 no kill)
 *   counter: + caster_DP(10) - target_AP(8) = +2 -> 22
 *   flank: 22*3/2 = 33
 * Asserts the counter+flank candidate (1,0) won. */
static void test_ai_score_phys_counter_and_flank(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_eq;
    ti_setup_phys(&save_pmc, &save_w, &save_h, &save_eq);
    /* second candidate tile (1,0) */
    t_ai_tile_map[(0 * 3 + 1) * 4 + 7] = 0;
    g_test_rc_array[0].ap = 30;
    g_test_rc_array[0].dp = 10;        /* caster effective_DP */
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].ap = 8;         /* target_AP for counter term */
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].char_id = 0;    /* leader -> flank */
    g_test_rc_array[1].status_sleep_flag = 0;  /* awake -> counter eligible */
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 0);
    ti_restore_phys(save_pmc, save_w, save_h, save_eq);
}

/* Terrain modifier (signed *AP/100 IDIV at disasm 0x143f1): make the CASTER
 * immune (job 0x13) so the attacker terrain bonus applies; the target stays
 * non-immune so its DP is unmodified. Base AP 12, DP 10 -> base raw 2 (class 0).
 * The candidate tile (1,1) resolves to tile_id T via fd2_read_tile_attribute_at_pos:
 * the tile cell's sprite_idx word (cell+4) is 0, so attr_ptr = attr_buffer+0 and
 * tile_buf[5] = attr_buffer[1] = T. mv_modifier[T] = 50 (%):
 *   effective_AP = 12 + (50 * 12)/100 = 12 + 6 = 18  ->  raw = 18 - 10 = 8 (class 8).
 * Asserting score == 8 (not 0) proves the terrain IDIV path executed with the
 * expected magnitude; a missing/incorrect terrain read would leave raw 2 -> 0.
 * Tile-map bytes for the (1,1) cell (k=4, stride 4): overlay byte base+7+k*4=+23
 * cleared so the tile is the single candidate; sprite_idx word at cell+4 = +20/+21
 * zeroed. The target (also at (1,1)) is non-immune so no target terrain read. */
static void test_ai_score_phys_terrain_bonus_lifts_class(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_eq;
    uint32 save_attr_ptr;
    ti_setup_phys(&save_pmc, &save_w, &save_h, &save_eq);
    save_attr_ptr = data_fd2_tile_attribute_flags_buffer_ptr;
    memset(t_ai_attr_buf, 0, sizeof(t_ai_attr_buf));
    t_ai_attr_buf[1] = 9;                 /* tile_id T = 9 */
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_ai_attr_buf;
    data_fd2_battle_tile_attr_mv_modifier_table[9] = 50;   /* +50% AP */
    data_fd2_battle_tile_attr_def_modifier_table[9] = 0;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 4] = 0;    /* sprite_idx low byte = 0 */
    t_ai_tile_map[(1 * 3 + 1) * 4 + 5] = 0;    /* sprite_idx high byte = 0 */

    g_test_rc_array[0].job_id = 0x13;     /* caster immune -> terrain applies */
    g_test_rc_array[0].ap = 12;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].char_id = 7;
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 1);
    data_fd2_tile_attribute_flags_buffer_ptr = save_attr_ptr;
    data_fd2_battle_tile_attr_mv_modifier_table[9] = 0;
    ti_restore_phys(save_pmc, save_w, save_h, save_eq);
}

/* ---- Full scoring-path tests for fd2_ai_score_item_use @ 0x1567E ----
 *
 * Drive the real per-slot / per-tile / per-target item-scoring loop end to end.
 * Callees on the path that are REAL in the test build:
 *   fd2_get_item_effect_entry / fd2_get_movement_cost_table_for_job (table.c),
 *   fd2_compute_aoe_targets / fd2_collect_unmarked_tile_positions /
 *   fd2_scan_chars_along_line_with_team_filter / fd2_score_item_candidate
 *   (btl_ai.c), fd2_find_char_at_cursor_pos (btl_turn.c).
 * Stubs: fd2_count_usable_inventory_slots (controllable via
 *   g_count_usable_slots_return — status.c not yet emitted),
 *   fd2_check_char_is_dead (-> 0, never dead),
 *   fd2_init_movement_range_floodfill / fd2_obfuscate_battle_tile_map (no-ops).
 *
 * fd2_get_item_effect_entry returns &item_effect_table[id]+1 (struct base +1),
 * so the function's pItem[N] reads item_effect_table[id] byte (N+1):
 *   pItem[0xD]  = +14 use_effect      (offensive gate; also score effect_code)
 *   pItem[0x10] = +17 cast_range_flags (range_class: <0x10 short / >=0x10 line)
 *   pItem[0x11] = +18 target_side      (ctx_flag==0: 0->aoe_arg 1, !=0->aoe_arg 0)
 *   pItem[0x12] = +19 area             (short-range aoe radius arg)
 *
 * Map: 3x3, party set per test. The precompute fd2_compute_aoe_targets(caster,0,
 * range_for_aoe, range_class>0xf, 0) runs floodfill (no-op stub) then, only for
 * the long-range case (range_for_aoe=1, aoe_radius=1), re-marks just the caster
 * tile 0xFF (manhattan<1). So the unmarked-tile set collected for candidates is
 * exactly the tiles whose +7 byte the test cleared (minus the caster tile in the
 * long-range case). Because the floodfill that would gate reachability is a
 * no-op, the SHORT-range per-tile fd2_compute_aoe_targets target set does NOT
 * depend on (cx,cy): it is every team-matching char on an unmarked tile. The
 * LONG-range per-tile fd2_scan_chars_along_line_with_team_filter DOES depend on
 * (cx,cy) (it walks the caster->candidate line), so per-candidate score
 * divergence (gating tests) is driven through the long-range branch.
 *
 * fd2_score_item_candidate with effect_code 5 (use_effect=5) is the HP-threshold
 * path (asm 0x158b9-0x158de): per target hp_cur<=hp_max/3 -> 8; hp_cur>hp_max/2
 * -> 0; else 3; *3 if pChar[0x34]&0x80. ai_class byte (pChar[0x34]) left 0 so no
 * x3. All expected scores below are exact (no emulation needed). */
extern int g_count_usable_slots_return;

static void ti_setup_item(uint32 *save_pmc, uint32 *save_w, uint32 *save_h,
                          int *save_cnt)
{
    *save_pmc = data_fd2_battle_party_member_count;
    *save_w = data_fd2_battle_map_width_tiles;
    *save_h = data_fd2_battle_map_height_tiles;
    *save_cnt = g_count_usable_slots_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    reset_ai_stubs();                 /* tile map -> 0xFF, ptr wired */
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;
    g_count_usable_slots_return = 1;  /* one usable inventory slot */

    g_test_rc_array[0].pos_x = 0;     /* caster */
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].inventory_slots[1] = 7;   /* slot 0 item id = 7 */

    data_fd2_battle_ai_best_item_target_x = 0xEE;
    data_fd2_battle_ai_best_item_target_y = 0xEE;
    data_fd2_battle_ai_best_item_slot = 0xEE;
}

static void ti_restore_item(uint32 save_pmc, uint32 save_w, uint32 save_h,
                            int save_cnt)
{
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    g_count_usable_slots_return = save_cnt;
}

/* Short-range item (range_class 2 < 0x10), ctx_flag 0, target_side 0 ->
 * aoe_arg 1 -> team_filter 1 (team != 0). Single candidate tile (1,1) with a
 * team-2 target (char 1) on it -> n_targets 1. HP 5/100 -> score 8. Asserts the
 * best globals captured slot 0 at (1,1). */
static void test_ai_score_item_short_range_score8(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_cnt;
    ti_setup_item(&save_pmc, &save_w, &save_h, &save_cnt);
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_item_effect_table[7].use_effect = 5;       /* +14 effect/gate */
    data_fd2_battle_item_effect_table[7].cast_range_flags = 2; /* +17 short range  */
    data_fd2_battle_item_effect_table[7].target_side = 0;      /* +18 -> aoe_arg 1  */
    data_fd2_battle_item_effect_table[7].area = 2;             /* +19 short aoe arg */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 1;
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].hp_max = 100;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;   /* candidate (1,1) */
    data_fd2_battle_ai_best_item_score = 99;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_slot, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 1);
    ti_restore_item(save_pmc, save_w, save_h, save_cnt);
}

/* Long-range item (range_class 0x12 -> line scan, step_count 2). Precompute
 * marks only the caster tile (0,0); candidate (0,2) stays unmarked. The line
 * scan from caster (0,0) toward (0,2) walks (0,1),(0,2) and collects the team-2
 * char on (0,2) (team_filter hardcoded 0 -> team != 0). HP 5/100 -> score 8.
 * Exercises the pItem[0x10]>=0x10 branch + EAX-return use from the line scan. */
static void test_ai_score_item_long_range_line(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_cnt;
    ti_setup_item(&save_pmc, &save_w, &save_h, &save_cnt);
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_item_effect_table[7].use_effect = 5;          /* +14 */
    data_fd2_battle_item_effect_table[7].cast_range_flags = 0x12; /* +17 line, step 2 */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 0;
    g_test_rc_array[1].pos_y = 2;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].hp_max = 100;
    t_ai_tile_map[(2 * 3 + 0) * 4 + 7] = 0;   /* candidate (0,2) */
    data_fd2_battle_ai_best_item_score = 99;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_slot, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 2);
    ti_restore_item(save_pmc, save_w, save_h, save_cnt);
}

/* aoe_arg divergence on ctx_flag with target_side(+18)=2 (!=0):
 *   ctx_flag 0 -> aoe_arg = (pItem[0x11]==0)?1:0 = 0 -> team_filter 0 (team==0)
 *   ctx_flag 1 -> aoe_arg = pItem[0x11] = 2          -> team_filter 2 (team==1)
 * One team-0 char (char 2) sits on candidate (1,1). ctx 0 collects it (score 8);
 * ctx 1 (team_filter 2, no team-1 char) collects nothing -> no update -> score
 * stays 0 with target/slot untouched (sentinels). Short-range (range_class 1). */
static void test_ai_score_item_ctx_flag_aoe_arg(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_cnt;
    /* ctx_flag 0 path: team_filter 0 collects the team-0 char -> score 8 */
    ti_setup_item(&save_pmc, &save_w, &save_h, &save_cnt);
    data_fd2_battle_party_member_count = 3;
    data_fd2_battle_item_effect_table[7].use_effect = 5;
    data_fd2_battle_item_effect_table[7].cast_range_flags = 1;  /* short */
    data_fd2_battle_item_effect_table[7].target_side = 2;       /* +18 != 0 */
    data_fd2_battle_item_effect_table[7].area = 2;
    g_test_rc_array[2].team = 0;
    g_test_rc_array[2].pos_x = 1;
    g_test_rc_array[2].pos_y = 1;
    g_test_rc_array[2].hp_current = 5;
    g_test_rc_array[2].hp_max = 100;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_item_score = 0;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 1);
    ti_restore_item(save_pmc, save_w, save_h, save_cnt);

    /* ctx_flag 1 path: team_filter 2 finds no team-1 char -> no update */
    ti_setup_item(&save_pmc, &save_w, &save_h, &save_cnt);
    data_fd2_battle_party_member_count = 3;
    data_fd2_battle_item_effect_table[7].use_effect = 5;
    data_fd2_battle_item_effect_table[7].cast_range_flags = 1;
    data_fd2_battle_item_effect_table[7].target_side = 2;
    data_fd2_battle_item_effect_table[7].area = 2;
    g_test_rc_array[2].team = 0;
    g_test_rc_array[2].pos_x = 1;
    g_test_rc_array[2].pos_y = 1;
    g_test_rc_array[2].hp_current = 5;
    g_test_rc_array[2].hp_max = 100;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_item_score = 0;
    fd2_ai_score_item_use(0, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 0xEE);  /* untouched */
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_slot, 0xEE);
    ti_restore_item(save_pmc, save_w, save_h, save_cnt);
}

/* Non-offensive item: use_effect(+14)=0 -> pItem[0xD]==0 -> slot skipped before
 * any tile work. No global update; score reset to 0 at entry, target/slot keep
 * their sentinels. A target is present on an unmarked tile to prove the skip is
 * the gate (not an empty candidate list). */
static void test_ai_score_item_non_offensive_skip(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_cnt;
    ti_setup_item(&save_pmc, &save_w, &save_h, &save_cnt);
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_item_effect_table[7].use_effect = 0;        /* +14 -> skip */
    data_fd2_battle_item_effect_table[7].cast_range_flags = 2;
    data_fd2_battle_item_effect_table[7].target_side = 0;
    data_fd2_battle_item_effect_table[7].area = 2;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 1;
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].hp_max = 100;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_item_score = 77;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_slot, 0xEE);
    ti_restore_item(save_pmc, save_w, save_h, save_cnt);
}

/* Best-candidate gating across two long-range candidates. Candidates collected
 * row-major: (2,0) (y=0) before (0,2) (y=2). The line scan is per-candidate
 * (caster->candidate), so each tile resolves a distinct target with its own
 * score. Scenario A: (2,0) target score 3, (0,2) target score 8 -> the higher
 * later score overwrites (best 8 at (0,2)). Scenario B swaps the HPs: (2,0)
 * score 8 first, (0,2) score 3 second -> the lower later score does NOT
 * overwrite (best 8 stays at (2,0)). char1 on (2,0), char2 on (0,2); both
 * team 2 so the team_filter-0 line scan collects each. score 8 = hp 5/100;
 * score 3 = hp 40/100 (40 > 100/3=33 and 40 <= 100/2=50). */
static void test_ai_score_item_best_candidate_gating(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    int save_cnt;
    /* Scenario A: higher (later) overwrites */
    ti_setup_item(&save_pmc, &save_w, &save_h, &save_cnt);
    data_fd2_battle_party_member_count = 3;
    data_fd2_battle_item_effect_table[7].use_effect = 5;
    data_fd2_battle_item_effect_table[7].cast_range_flags = 0x12;  /* line, step 2 */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 2; g_test_rc_array[1].pos_y = 0;   /* on (2,0) */
    g_test_rc_array[1].hp_current = 40; g_test_rc_array[1].hp_max = 100;  /* score 3 */
    g_test_rc_array[2].team = 2;
    g_test_rc_array[2].pos_x = 0; g_test_rc_array[2].pos_y = 2;   /* on (0,2) */
    g_test_rc_array[2].hp_current = 5; g_test_rc_array[2].hp_max = 100;   /* score 8 */
    t_ai_tile_map[(0 * 3 + 2) * 4 + 7] = 0;   /* candidate (2,0) */
    t_ai_tile_map[(2 * 3 + 0) * 4 + 7] = 0;   /* candidate (0,2) */
    data_fd2_battle_ai_best_item_score = 0;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 2);
    ti_restore_item(save_pmc, save_w, save_h, save_cnt);

    /* Scenario B: lower (later) does NOT overwrite */
    ti_setup_item(&save_pmc, &save_w, &save_h, &save_cnt);
    data_fd2_battle_party_member_count = 3;
    data_fd2_battle_item_effect_table[7].use_effect = 5;
    data_fd2_battle_item_effect_table[7].cast_range_flags = 0x12;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 2; g_test_rc_array[1].pos_y = 0;   /* on (2,0) */
    g_test_rc_array[1].hp_current = 5; g_test_rc_array[1].hp_max = 100;   /* score 8 */
    g_test_rc_array[2].team = 2;
    g_test_rc_array[2].pos_x = 0; g_test_rc_array[2].pos_y = 2;   /* on (0,2) */
    g_test_rc_array[2].hp_current = 40; g_test_rc_array[2].hp_max = 100;  /* score 3 */
    t_ai_tile_map[(0 * 3 + 2) * 4 + 7] = 0;
    t_ai_tile_map[(2 * 3 + 0) * 4 + 7] = 0;
    data_fd2_battle_ai_best_item_score = 0;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 2);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 0);
    ti_restore_item(save_pmc, save_w, save_h, save_cnt);
}

/* ---- Full scoring-path tests for fd2_ai_score_offensive_spell @ 0x1598A ----
 *
 * Drive the real per-spell / per-tile / per-target offensive-spell scoring loop
 * end to end. Callees on the path that are REAL in the test build:
 *   fd2_get_spell_effect_entry / fd2_get_movement_cost_table_for_job (table.c),
 *   fd2_compute_aoe_targets / fd2_collect_unmarked_tile_positions /
 *   fd2_score_spell_candidate (btl_ai.c).
 * Stubs: fd2_build_usable_spell_list (count via g_build_spell_list_return AND
 *   buffer contents via g_spell_list_buf[] — extended in testglob.c to mirror the
 *   real "write spell ids into out_buf, return count" contract; without that the
 *   loop's spell_list[i] would read garbage), fd2_init_movement_range_floodfill /
 *   fd2_obfuscate_battle_tile_map (no-ops).
 *
 * fd2_get_spell_effect_entry returns &spell_effect_table[id] (struct base, no +1
 * offset), so the function's pSpell[N] reads spell_effect_table[id] byte N and
 * *(uint16*)pSpell reads the 'damage' word:
 *   *(uint16*)pSpell = +0 damage     (base_dmg tiebreak key; also the
 *                                      score_spell_candidate kill threshold)
 *   pSpell[3] = +3 cast_range_flags  (floodfill range — floodfill is a no-op stub)
 *   pSpell[4] = +4 area              (spell_range arg to compute_aoe_targets; <0x10)
 *   pSpell[5] = +5 mp_cost           (gate: skip spell if > caster mp_current)
 *   pSpell[6] = +6 target_side       (ctx_flag==0: 0->aoe_arg 1, !=0->aoe_arg 0)
 *
 * Map: 3x3, party set per test. The per-tile fd2_compute_aoe_targets(cx,cy,buf,
 * area, 0, aoe_arg) takes the spell_range<0x10 branch -> runs floodfill (no-op
 * stub) and, with aoe_radius 0, skips the radius clear. So the collected target
 * set does NOT depend on (cx,cy): it is every team-matching, alive char whose own
 * tile (+7 byte) is not 0xFF. reset_ai_stubs() sets the whole map 0xFF, so the
 * test clears the +7 byte of each candidate tile (for collect_unmarked to surface
 * it) and places each target char on a cleared tile (so compute_aoe_targets keeps
 * it). aoe_arg maps to compute_aoe_targets team_filter: 0->team==0, 1->team!=0,
 * 2->team==1, 3->team==2.
 *
 * fd2_score_spell_candidate damage path (spell_id<0xD, asm 0x15baf-0x15c1f): per
 * target per_score = (hp_current < damage) ? 0x18 : 8, then *1.5 (FILD/FMUL/FISTP)
 * if pChar[8] (char_id) == 0. The happy-path / tiebreak targets set char_id != 0
 * so the multiplier is skipped and per_score is the exact integer 0x18 (=24) for a
 * kill shot (hp < damage) — matching the existing test_spell_score_damage_kill_shot
 * ground truth. spell ids stay < 0xA so the score path's >=0xA immunity check is
 * not entered. */
extern uint8 g_spell_list_buf[12];

static void ts_setup_spell(uint32 *save_pmc, uint32 *save_w, uint32 *save_h,
                           int *save_ret)
{
    *save_pmc = data_fd2_battle_party_member_count;
    *save_w = data_fd2_battle_map_width_tiles;
    *save_h = data_fd2_battle_map_height_tiles;
    *save_ret = g_build_spell_list_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_spell_effect_table, 0,
           sizeof(data_fd2_battle_spell_effect_table));
    memset(g_spell_list_buf, 0, sizeof(g_spell_list_buf));
    reset_ai_stubs();                 /* tile map -> 0xFF, ptr wired */
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;

    g_test_rc_array[0].pos_x = 0;     /* caster */
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].mp_current = 50;
    g_test_rc_array[0].combat_aux_block[0] = 0;   /* +0x27 not silenced */

    data_fd2_battle_ai_best_spell_target_x = 0xEE;
    data_fd2_battle_ai_best_spell_target_y = 0xEE;
    data_fd2_battle_ai_best_spell_id = 0xEE;
}

static void ts_restore_spell(uint32 save_pmc, uint32 save_w, uint32 save_h,
                             int save_ret)
{
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    g_build_spell_list_return = save_ret;
}

/* Gate: no castable spell (count 0) -> early return at entry. ai_best_spell_score
 * is cleared to 0 first thing; target/id sentinels stay untouched. A live target
 * sits on an unmarked tile to prove the gate (not an empty candidate list) is what
 * stops the scan. */
static void test_ai_spell_gate_no_castable(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 0;                 /* gate fail */
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].area = 2;
    g_spell_list_buf[0] = 0;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 99;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0xEE);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}

/* Gate: caster combat_aux_block[0] (+0x27, silence) != 0 -> early return even with
 * a castable spell and a valid target present. */
static void test_ai_spell_gate_silenced(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 5;
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 0;
    g_test_rc_array[0].combat_aux_block[0] = 1;    /* +0x27 silenced */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 77;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0xEE);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}

/* MP gate: the single castable spell costs more MP (+5 mp_cost) than the caster's
 * mp_current -> spell skipped before any tile work -> no update, score stays 0. A
 * killable target is present to prove the MP gate is the stopper. */
static void test_ai_spell_mp_gate_skips(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 99;   /* > caster mp 50 */
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 0;
    g_test_rc_array[0].mp_current = 50;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0xEE);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}

/* Happy path: one castable damage spell (id 0, damage 50, mp_cost 5, area 2,
 * target_side 0 -> ctx_flag 0 gives aoe_arg 1 -> team_filter 1 collects team!=0).
 * One team-2 target (char 1) on candidate tile (1,1), hp 5 < damage 50, char_id 9
 * (!=0 -> no x1.5) -> per_score 0x18 = 24. The expected score is cross-checked
 * against the REAL fd2_score_spell_candidate with the same (spell_id, target) so
 * no value is assumed. Asserts the best globals captured (1,1) and spell id 0. */
static void test_ai_spell_happy_path_capture(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    uint8 xcheck_buf[1];
    int expected;
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;                        /* spell id 0 */
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 5;
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 0;
    g_test_rc_array[0].mp_current = 50;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;         /* candidate + target tile (1,1) */
    data_fd2_battle_ai_best_spell_score = 0;

    /* ground truth from the real scorer for spell 0 against target char 1 */
    xcheck_buf[0] = 1;
    expected = fd2_score_spell_candidate(0, 1, (uint32)xcheck_buf);

    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, (long)expected);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0x18);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_y, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}

/* aoe_arg divergence on ctx_flag with target_side(+6)=2 (!=0):
 *   ctx_flag 0 -> aoe_arg = (pSpell[6]==0)?1:0 = 0 -> team_filter 0 (team==0)
 *   ctx_flag 1 -> aoe_arg = pSpell[6] = 2          -> team_filter 2 (team==1)
 * A team-0 char (char 1) sits on candidate (1,1). ctx 0 collects it (kill shot,
 * char_id 9 -> score 0x18); ctx 1 (team_filter 2, no team-1 char) collects
 * nothing -> no update -> score stays 0, target/id keep sentinels. Same map/target
 * for both runs, only ctx_flag differs. */
static void test_ai_spell_ctx_flag_aoe_arg(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    /* ctx_flag 0: team_filter 0 collects the team-0 char -> score 0x18 */
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 5;
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 2;   /* +6 != 0 */
    g_test_rc_array[1].team = 0;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0x18);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_y, 1);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);

    /* ctx_flag 1: team_filter 2 finds no team-1 char -> no update */
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 5;
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 2;
    g_test_rc_array[1].team = 0;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0xEE);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}

/* base_dmg tiebreak (asm 0x15aec-0x15af3, untested by any other test): when a
 * later candidate produces a score EQUAL to the current best, it overwrites only
 * if its spell 'damage' (base_dmg) is strictly greater than the best so far.
 * Both spells (ids 2 and 3) are damage spells; the single target (char 1, hp 5,
 * char_id 9) is a kill shot for any damage > 5, so BOTH score 0x18 (equal). One
 * candidate tile so target coords are fixed; spell-list order decides which sets
 * best_dmg first.
 *   Scenario A: list [2(dmg50), 3(dmg60)] -> spell 3 score==best, 60>50 -> wins.
 *   Scenario B: list [2(dmg60), 3(dmg50)] -> spell 3 score==best, 50<=60 -> JLE,
 *               best stays spell 2. */
static void test_ai_spell_base_dmg_tiebreak(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    /* Scenario A: later higher base_dmg wins the equal-score tiebreak */
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 2;
    g_spell_list_buf[0] = 2;                        /* evaluated first */
    g_spell_list_buf[1] = 3;                        /* evaluated second */
    data_fd2_battle_spell_effect_table[2].damage = 50;
    data_fd2_battle_spell_effect_table[2].mp_cost = 5;
    data_fd2_battle_spell_effect_table[2].area = 2;
    data_fd2_battle_spell_effect_table[2].target_side = 0;
    data_fd2_battle_spell_effect_table[3].damage = 60;
    data_fd2_battle_spell_effect_table[3].mp_cost = 5;
    data_fd2_battle_spell_effect_table[3].area = 2;
    data_fd2_battle_spell_effect_table[3].target_side = 0;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0x18);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 3);   /* higher base_dmg */
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);

    /* Scenario B: later lower base_dmg does NOT overwrite (JLE skip) */
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 2;
    g_spell_list_buf[0] = 2;
    g_spell_list_buf[1] = 3;
    data_fd2_battle_spell_effect_table[2].damage = 60;
    data_fd2_battle_spell_effect_table[2].mp_cost = 5;
    data_fd2_battle_spell_effect_table[2].area = 2;
    data_fd2_battle_spell_effect_table[2].target_side = 0;
    data_fd2_battle_spell_effect_table[3].damage = 50;
    data_fd2_battle_spell_effect_table[3].mp_cost = 5;
    data_fd2_battle_spell_effect_table[3].area = 2;
    data_fd2_battle_spell_effect_table[3].target_side = 0;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0x18);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 2);   /* first kept */
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}

static void test_pan_cursor_to_origin(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 3;
    g_test_rc_array[0].pos_y = 3;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_to_char(0);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_x, 3);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_y, 3);
}

/* ---- Test: fd2_score_spell_candidate ---- */

/* ---- Test: fd2_main_menu_continue_dispatcher ---- */

static void test_main_menu_new_game(void)
{
    int r;
    g_ending_menu_return = 0;
    data_fd2_chapter_current_chapter_id = 5;
    r = fd2_main_menu_continue_dispatcher();
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 0);
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count, 0);
    ASSERT_EQ((long)data_fd2_ui_play_active_flag, 1);
}

static void test_main_menu_fallback(void)
{
    int r;
    g_ending_menu_return = 2;
    r = fd2_main_menu_continue_dispatcher();
    ASSERT_EQ((long)r, 0);
}

static void test_main_menu_continue_quit(void)
{
    int r;
    g_ending_menu_return = 1;
    g_slot_selector_return = -1;
    r = fd2_main_menu_continue_dispatcher();
    ASSERT_EQ((long)r, -1);
}

/* ---- Test: fd2_tick_summon_spell_minor_animation_state ---- */

/* ---- Test: fd2_tick_summon_anim_variant_d_3slot ---- */

static void test_summon_d_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1], -3);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[3], -9);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_color_rotation_counter, 4);
}

static void test_summon_d_state3_hold(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x20);
}

static void test_summon_d_state6_terminate(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 0x10);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_terminate_flag, 1);
}

/* TICK (states 2/5/8) first toggles odd_even (mod 2). When the pre-call
 * toggle is 0 it becomes 1, so the whole even-frame update block is gated
 * OFF: slots still blit if visible, but frame counters do NOT advance, no
 * SFX, no done. State 2. */
static void test_summon_d_tick_toggle_skips_update(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 3; i++) {
        data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i] = 0;
        data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle, 1);
    for (i = 0; i < 3; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i], 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 3);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}

/* TICK with pre-call toggle 1 -> becomes 0 -> update block runs. Only the
 * 3 active slots iterate; each in-range (post-init staggered) slot advances
 * frame by 1, slot 0 frame 0 (in range) blits, slots 1/2 hidden (negative).
 * No slot hits frame 1 (no SFX) or post-inc 2 (no done). State 2. */
static void test_summon_d_tick_advance_no_done(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 1;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1] = -3;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2] = -6;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[0] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[1] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[2] = 0;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1], -2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2], -5);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}

/* done_flag (return 1) is set iff some slot's post-increment counter == 2.
 * slot 0 goes 1->2 (done); slots 1/2 go 0->1 (no done). Pre-call toggle 1
 * -> 0 so update runs. slot 0 frame 1 also fires the i==0 SFX bucket. */
static void test_summon_d_tick_done_at_frame2(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 1;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0] = 1;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[0] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[1] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[2] = 0;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0], 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2], 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 3);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}

/* Frame-7 color rotation (terminate_flag==0): on the slot whose post-inc
 * frame reaches 7, rotation_counter=(counter+1)%10, color_idx[i]=counter,
 * frame_counter[i]=0. slot 0 frame 6 (hidden, no blit) -> post-inc 7 ->
 * rotation. counter 3 -> 4. State 5 also TICKs. Pre-call toggle 1 -> 0. */
static void test_summon_d_tick_color_rotation(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_anim_variant_d_color_rotation_counter = 3;
    g_blit_indexed_sprite_calls = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0] = 6;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1] = -3;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2] = -6;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[0] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[1] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[2] = 0;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_color_rotation_counter, 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[0], 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1], -2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2], -5);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
}

/* 2-bucket frame-1 SFX: all 3 slots at frame 1 -> slot 0 plays with_handle,
 * slot 1 plays sample_from_bank, slot 2 silent. All 3 in range so all blit;
 * each post-inc 1->2 sets done (return 1). Pre-call toggle 1 -> 0. State 8. */
static void test_summon_d_tick_sfx_bucket_split(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 1;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 3; i++) {
        data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i] = 1;
        data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 8);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 3);
    for (i = 0; i < 3; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i], 2);
}

static void test_summon_a_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_shared_rng_seed = 42;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[2], -4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_color_rotation_counter, 6);
}

static void test_summon_a_state6_terminate(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 8);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_terminate_flag, 1);
}

/* state 3 is a pure constant return (disasm 0x26a9c CMP EAX,3 -> MOV EAX,0xc). */
static void test_summon_a_state3(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0xC);
}

/* TICK (state 2) blit-gate + frame-0 SFX + done_flag, rotation suppressed via
 * terminate_flag==1 so the frame-8 path is isolated out:
 *   slot0 frame 0  -> in [0,7) blit, frame==0 SFX (with_handle), ++ ->1
 *   slot1 frame 2  -> blit, ++ ->3 sets done_flag (disasm 0x26b64 CMP ...,3)
 *   slot2 frame 6  -> blit (6<7), ++ ->7
 *   slot3 frame -1 -> NOT in [0,7), no blit, ++ ->0
 *   slot4 frame -2 -> no blit, ++ ->-1
 *   slot5 frame 3  -> blit, ++ ->4
 * Variant-A TICK only ever calls fd2_play_sfx_with_handle (single CALL 0x25a96
 * at 0x26b51); it never calls fd2_play_sfx_sample_from_bank. */
static void test_summon_a_tick_blit_gate_sfx_done(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_a_terminate_flag = 1;
    for (i = 0; i < 6; i++)
        data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i] = 0;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0] = 0;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[1] = 2;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[2] = 6;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[3] = -1;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[4] = -2;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[5] = 3;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 4);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[1], 3);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[2], 7);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[3], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[5], 4);
}

/* TICK color rotation at frame 8 (terminate_flag==0): counter=(counter+1)%10,
 * color_idx[i]=counter, frame_counter[i]=0, then RNG jitter=7*(rng%2)
 * (disasm 0x26bc8 CALL rng; 0x26bcf..0x26bde IDIV 2 -> 7*(rng%2); the emitter
 * corrected the decompiler EAX bug that used the loop index). seed=8192 ->
 * fd2_advance_rng_state ROL16(0x2000+0x9014,3)=0x80A5 (odd) -> jitter 7. slot0
 * is the only slot at frame 7 (->8); other slots stay at frame 4 (blit, ++ ->5,
 * never reach 3 or 8) so done stays 0. */
static void test_summon_a_tick_color_rotation(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_a_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_a_color_rotation_counter = 3;
    data_fd2_shared_rng_seed = 8192;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i] = 0;
        data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0] = 7;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_color_rotation_counter, 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[0], 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[0], 7);
    ASSERT_EQ((long)r, 0);
}

/* TICK rotation mod-10 wrap + even-RNG jitter branch: counter 9 -> (9+1)%10==0,
 * and seed=0 -> fd2_advance_rng_state ROL16(0x9014,3)=0x80A4 (even) ->
 * jitter 7*(0)=0 (overwriting the preset 7, proving the RNG branch). Only slot0
 * reaches frame 8; other slots at frame -2 stay out of every gate. */
static void test_summon_a_tick_rotation_mod10_wrap(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_a_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_a_color_rotation_counter = 9;
    data_fd2_shared_rng_seed = 0;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i] = -2;
        data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0] = 7;
    data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[0] = 7;
    fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_color_rotation_counter, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[0], 0);
}

static void test_summon_8slot_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x1C);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[7], -14);
}

/* Helper-free shared setup note: in the unit-test build the three rodata
 * tables (visibility / y_offset / row_multiplier) are the zero-initialised
 * globals in testglob.c, so every 8slot test that depends on them sets the
 * entries it needs explicitly. */

/* state 5 done_flag return: loop increments each of slots 0..6, then sets
 * done=1 iff a post-increment counter == 9 (disasm 0x262cb INC; 0x262d2 CMP
 * ...,9). vis all 1 -> state-5 inverse gate (vis==0) never fires, isolating
 * the counter/return. slot 0 preset to 8 -> ++ -> 9 -> done; slot 1 at 0 ->
 * 1. Returns done_flag (read at 0x262e6 from [ESP+0x40]). */
static void test_summon_8slot_state5_done_flag(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 1;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = 0;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 8;
    g_blit_indexed_sprite_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0], 9);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[1], 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
}

/* state 5 no-done: all slots 0..6 at 0 -> ++ -> 1, none reaches 9 -> return 0.
 * vis all 1 keeps the inverse gate closed (no blit). */
static void test_summon_8slot_state5_no_done(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 1;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = 0;
    g_blit_indexed_sprite_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    for (i = 0; i < 7; i++)
        ASSERT_EQ(
            (long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i],
            1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
}

/* state 5 inverse blit gate vis[i]==0 (disasm 0x262a0 TEST/JNZ skips blit when
 * vis!=0): slot 0 vis==0 + counter in [0,0x10) -> blit; slot 1 vis==1 +
 * in-range -> no blit; remaining slots out of range. Pre-increment counter
 * gates the blit, so counter 2 (in range) blits then becomes 3. No slot hits
 * post-inc 9 -> return 0. Exactly 1 blit. */
static void test_summon_8slot_state5_inverse_gate(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 1;
    data_fd2_battle_summon_spell_8slot_visibility_table[0] = 0;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 2;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[1] = 2;
    g_blit_indexed_sprite_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
}

/* state 4 SFX trigger: fd2_play_sfx_with_handle fires when a slot counter == 3
 * (disasm 0x2620b CMP ...,3; 0x2621f CALL). vis all 0 keeps the state-4 gate
 * (vis==1) closed so no blit confounds the count. slot 0 == 3 -> exactly one
 * SFX; state 4 always returns 0. */
static void test_summon_8slot_state4_sfx_trigger(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 3;
    g_play_sfx_with_handle_calls = 0;
    g_blit_indexed_sprite_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 4);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
}

/* state 4 SFX negative: no slot 0..6 counter == 3 -> no SFX. */
static void test_summon_8slot_state4_sfx_no_trigger(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = 4;
    g_play_sfx_with_handle_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 4);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* state 4 visibility gate vis[i]==1 (disasm 0x2623e MOVZX; 0x26243 CMP ...,1):
 * slot 0 vis==1 + counter in range -> blit; slot 1 vis==0 + in range -> no
 * blit; rest out of range. Counters kept != 3 so no SFX confounds the count.
 * Exactly 1 blit. */
static void test_summon_8slot_state4_visibility_gate(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
    data_fd2_battle_summon_spell_8slot_visibility_table[0] = 1;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 2;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[1] = 2;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* state 4 blit-y arithmetic, player team (no enemy adjust): single visible
 * in-range slot 0. The display-y expression row_mul[0]*row_stride + origin_y +
 * y_off[0] (disasm 0x26257 IMUL EBP; 0x2625a ADD) is the 3rd blit argument, so
 * it lands in the stub's x slot (g_blit_indexed_sprite_last_x); the 4th
 * argument row_stride lands in last_y. With row_mul[0]=-10, row_stride=10,
 * origin_y=100, y_off[0]=40 -> -10*10 + 100 + 40 = 40. frame = counter = 5
 * (!=3 so no SFX). */
static void test_summon_8slot_state4_blit_y_arithmetic(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++) {
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
        data_fd2_battle_summon_spell_8slot_y_offset_table[i] = 0;
        data_fd2_battle_summon_spell_8slot_row_multiplier_table[i] = 0;
    }
    data_fd2_battle_summon_spell_8slot_visibility_table[0] = 1;
    data_fd2_battle_summon_spell_8slot_y_offset_table[0] = 40;
    data_fd2_battle_summon_spell_8slot_row_multiplier_table[0] = -10;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 5;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_x = 0;
    g_blit_indexed_sprite_last_y = 0;
    g_blit_indexed_sprite_last_frame = 0;
    g_play_sfx_with_handle_calls = 0;
    fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 100, 10, 4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 5);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 40);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_y, 10);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* state 4 blit-y enemy-team offset: team==0 adds 0x94 (148) to every y_off
 * before the blit math (disasm 0x261b3 TEST/JZ then 0x261bb ADD ...,0x94).
 * Same inputs as the arithmetic test but team=0 -> y_off[0] 40+148=188 ->
 * -10*10 + 100 + 188 = 188, landing in last_x (3rd arg). frame unchanged (5). */
static void test_summon_8slot_state4_blit_y_enemy_team_offset(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    for (i = 0; i < 7; i++) {
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
        data_fd2_battle_summon_spell_8slot_y_offset_table[i] = 0;
        data_fd2_battle_summon_spell_8slot_row_multiplier_table[i] = 0;
    }
    data_fd2_battle_summon_spell_8slot_visibility_table[0] = 1;
    data_fd2_battle_summon_spell_8slot_y_offset_table[0] = 40;
    data_fd2_battle_summon_spell_8slot_row_multiplier_table[0] = -10;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 5;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_x = 0;
    g_blit_indexed_sprite_last_y = 0;
    g_blit_indexed_sprite_last_frame = 0;
    fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 100, 10, 4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 5);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 188);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_y, 10);
}

/* default state: any state_code other than 3/4/5 returns 0 with no SFX/blit
 * (fall-through epilogue, EAX=0). */
static void test_summon_8slot_default_state(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 7);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

static void test_summon_main_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[11], -22);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_color_rotation_counter, 12);
}

/* state 3: pure 40-tick hold, returns 0x28, no side effects. */
static void test_summon_main_state3_hold(void)
{
    int r;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x28);
}

/* state 6: sets terminate_flag=1 (stops color rotation), returns 0x14. */
static void test_summon_main_state6_terminate(void)
{
    int r;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 0x14);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_terminate_flag, 1);
}

/* default branch: any other state_code returns 0. */
static void test_summon_main_default_state(void)
{
    int r;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 7);
    ASSERT_EQ((long)r, 0);
}

/* TICK (states 2/5/8) first toggles odd_even (mod 2). When the pre-call
 * toggle is 0 it becomes 1, gating the whole even-frame update block OFF:
 * in-range slots still blit, but frame counters do NOT advance, no SFX,
 * no done. All 12 slots frame 5 (in range [0,0xB)). State 2. */
static void test_summon_main_tick_toggle_skips_update(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = 5;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_odd_even_frame_toggle, 1);
    for (i = 0; i < 12; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i], 5);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 12);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}

/* TICK with pre-call toggle 1 -> becomes 0 -> update block runs. Staggered
 * init frames (slot 0 at 0, others negative): only slot 0 is in range, so
 * 1 blit; every slot's frame advances by 1; no slot reaches post-inc 3
 * (no done) or post-inc 0xB (no rotation). color 0 spr_offset is left 0 so
 * slot 0 frame 0 fires no with_handle. State 2. */
static void test_summon_main_tick_advance_no_done(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -2 * i;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_odd_even_frame_toggle, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[1], -1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[11], -21);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}

/* done-at-frame-3: a slot whose post-increment counter reaches 3 sets
 * return 1. spr_offset[color]==0 for that slot, so frame-3 also fires
 * sample_from_bank. slot 0 frame 2 -> post-inc 3 (done + sample); other
 * slots negative (no effect). Pre-call toggle 1 -> 0. State 5 also TICKs. */
static void test_summon_main_tick_done_at_frame3(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 2;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 3);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* SFX bucket split driven by spr_offsets[color]:
 *  - frame 0 with spr_offsets[color]!=0  -> with_handle (sfx 2)
 *  - post-inc frame 3 with spr_offsets[color]==0 -> sample_from_bank (sfx 1)
 * Assembly note: done_flag is set whenever post-inc==3 REGARDLESS of
 * spr_offsets; the sample SFX is the spr==0-only extra. Here slot 0 (color
 * 0, spr=0x16) is at frame 0 -> with_handle, and slot 1 (color 1, spr=0)
 * is at frame 2 -> post-inc 3 -> sample + done. Pre-call toggle 1 -> 0.
 * State 8. */
static void test_summon_main_tick_sfx_bucket_split(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0x16;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[1] = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++)
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -8;
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 0;
    data_fd2_battle_summon_main_anim_12slot_color_idx_array[0] = 0;
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[1] = 2;
    data_fd2_battle_summon_main_anim_12slot_color_idx_array[1] = 1;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 8);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    /* both slots in range -> 2 blits */
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[1], 3);
}

/* done at post-inc 3 fires even when spr_offsets[color]!=0 (no sample SFX):
 * slot 0 color 0 spr=0x16 at frame 2 -> post-inc 3 -> done=1 but NO
 * sample_from_bank. Isolates the done/sample decoupling. Toggle 1 -> 0. */
static void test_summon_main_tick_done_without_sample_when_spr_nonzero(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0x16;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 2;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 3);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
    /* frame went 2->3 so it was never 0 at entry: no with_handle either */
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* Color rotation at post-inc frame 0xB (terminate==0): rotation_counter =
 * (counter+1)%12, color_idx[i]=counter, frame_counter[i]=0. slot 0 frame
 * 0xA (in range, blits) -> post-inc 0xB -> rotation. counter 5 -> 6.
 * Other slots negative. Pre-call toggle 1 -> 0. State 2. */
static void test_summon_main_tick_color_rotation(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_color_rotation_counter = 5;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    g_blit_indexed_sprite_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 0xA;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_color_rotation_counter, 6);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_color_idx_array[0], 6);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
}

/* terminate_flag==1 blocks the frame-0xB rotation entirely: frame stays
 * 0xB, rotation_counter and color_idx unchanged. slot 0 frame 0xA ->
 * post-inc 0xB but rotation skipped. Toggle 1 -> 0. */
static void test_summon_main_tick_rotation_blocked_by_terminate(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 1;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_color_rotation_counter = 5;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 0xA;
    fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 0xB);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_color_rotation_counter, 5);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_color_idx_array[0], 0);
}

/* Rotation counter wraps mod 12: counter 11 -> (11+1)%12 == 0. The signed
 * IDIV-by-12 path computes the modulus; assert the wrap lands on 0. slot 0
 * frame 0xA -> post-inc 0xB -> rotation. Toggle 1 -> 0. */
static void test_summon_main_tick_rotation_mod12_wrap(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_color_rotation_counter = 11;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 0xA;
    fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_color_rotation_counter, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_color_idx_array[0], 0);
}

/* Blit position arithmetic (high-risk numeric path), non-enemy team (no
 * +0x14). The vertical blit position is the 3rd fd2_blit_indexed_sprite
 * argument (captured as last_x by the stub; the stub's "y" param receives
 * row_stride):
 *   pos = origin_y + y_offsets[color] - v_offsets[color]*row_stride
 *   sprite_id = spr_offsets[color] + frame
 * color 3: y_off=50, v_off=2, spr_off=7. origin_y=100, row_stride=10,
 * frame=4 (in range, !=0/3/0xB so no SFX/done/rotation) -> only slot 0
 * blits. Expected sprite_id = 7+4 = 11; pos = 100 + 50 - 2*10 = 130.
 * Toggle 0 -> 1 so the update block is OFF (frame stays 4), isolating
 * the blit math. State 2. */
static void test_summon_main_tick_blit_y_arithmetic(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_y_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12color_v_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12color_sprite_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_y_offset_table[3] = 50;
    data_fd2_battle_summon_main_anim_12color_v_offset_table[3] = 2;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[3] = 7;
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 4;
    data_fd2_battle_summon_main_anim_12slot_color_idx_array[0] = 3;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_frame = 0;
    g_blit_indexed_sprite_last_x = 0;
    fd2_tick_summon_spell_main_animation_state(0, 0, 100, 10, 2);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 11);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 130);
    /* toggle was 0 -> became 1: update gated off, frame unchanged */
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 4);
}

/* Team (enemy) Y-adjust: bTeam==0 adds 0x14 to every y_offset before the
 * blit math. Same inputs as the arithmetic test but team=0 ->
 * pos = origin_y + (y_offsets[color]+0x14) - v_offsets[color]*row_stride
 *     = 100 + (50+20) - 2*10 = 150. sprite_id unchanged (11). State 2,
 * toggle 0 -> 1 isolates blit. */
static void test_summon_main_tick_blit_y_enemy_team_offset(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_y_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12color_v_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12color_sprite_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_y_offset_table[3] = 50;
    data_fd2_battle_summon_main_anim_12color_v_offset_table[3] = 2;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[3] = 7;
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 4;
    data_fd2_battle_summon_main_anim_12slot_color_idx_array[0] = 3;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_frame = 0;
    g_blit_indexed_sprite_last_x = 0;
    fd2_tick_summon_spell_main_animation_state(0, 0, 100, 10, 2);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 11);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 150);
}

static void test_summon_generic_reset(void)
{
    int r;
    data_fd2_battle_summon_spell_anim_phase_byte = 0xFF;
    r = fd2_tick_summon_spell_animation_state(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 0x1D);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0);
}

static void test_summon_generic_state3(void)
{
    int r;
    r = fd2_tick_summon_spell_animation_state(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0xC);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0x10);
}

static void test_summon_generic_state5_advance(void)
{
    int r;
    data_fd2_battle_summon_spell_anim_phase_byte = 0x10;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_spell_animation_state(0, 0, 100, 320, 5);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0x11);
    ASSERT_EQ((long)r, 1);
}

/* State 6: SFX chime + persistent phase write 0xA + distinct return 0xA.
 * asm 0x265af-0x265cd: PUSH 3/CALL sfx; MOV [phase],0xa; MOV EAX,0xa; return.
 * Same risk class (persistent mutation + distinct frame-budget value) as
 * the tested state 3. Inputs other than state_code do not affect this path. */
static void test_summon_generic_state6(void)
{
    int r;
    data_fd2_battle_summon_spell_anim_phase_byte = 0xFF;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_play_sfx_with_handle_calls = 0;
    r = fd2_tick_summon_spell_animation_state(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 0xA);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0xA);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
}

/* State 5 wrap path: phase 0x11 -> INC -> 0x12 -> reset to 0x10, return 0.
 * Complements state5_advance (0x10->0x11). asm 0x26701 INC; 0x2670e CMP 0x11
 * (miss); 0x2672c CMP 0x12 (hit) -> 0x26731 MOV [phase],0x10; return EDI=0.
 * team=2 -> is_enemy=0, so only the single unconditional blit fires; the
 * 0x11 SFX branch is skipped (phase is 0x12 after INC). */
static void test_summon_generic_state5_wrap(void)
{
    int r;
    data_fd2_battle_summon_spell_anim_phase_byte = 0x11;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    r = fd2_tick_summon_spell_animation_state(0, 0, 100, 320, 5);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0x10);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

static void test_summon_b_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_shared_rng_seed = 100;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[3], -6);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_color_rotation_counter, 6);
}

static void test_summon_b_state3(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0xC);
}

static void test_summon_b_state6_terminate(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 8);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_terminate_flag, 1);
}

/* TICK (state 2): every in-range slot advances frame_counter by 1.
 * Frames all 3 -> all become 4: in visible range so all 6 blit, none
 * reaches done(2) or rotation(7), no slot at frame 0 so no SFX. */
static void test_summon_b_tick_increment_no_done(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 3;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    for (i = 0; i < 6; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i], 4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 6);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}

/* done_flag (return 1) is set iff some slot's post-increment counter == 2.
 * slot 2 goes 1->2 (done); other slots 4->5 (no done). State 5 also TICKs. */
static void test_summon_b_tick_done_at_frame2(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[2] = 1;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[2], 2);
}

/* Blit visibility guard: only slots with pre-increment frame in [0,6) blit.
 * frames [-1,0,5,6,3,-3] -> in range: slots 1,2,4 (3 blits). State 8 TICKs. */
static void test_summon_b_tick_blit_visibility_guard(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    g_blit_indexed_sprite_calls = 0;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0] = -1;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[1] = 0;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[2] = 5;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[3] = 6;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[4] = 3;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[5] = -3;
    fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 8);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 3);
}

/* 2-bucket frame-0 SFX: all frames 0 -> slot 0 plays with_handle, slot 3
 * plays sample_from_bank, slots 1,2,4,5 silent. All 6 blit (0 in range). */
static void test_summon_b_tick_sfx_bucket_split(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 0;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 6);
    ASSERT_EQ((long)r, 0);
    for (i = 0; i < 6; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i], 1);
}

/* Color rotation at frame 7 (terminate_flag==0): counter=(counter+1)%10,
 * color_idx[i]=counter, frame_counter[i]=0, jitter[i]=(rng%2)*6.
 * seed=8192 -> first RNG output 0x80a5 (odd) -> jitter 6. */
static void test_summon_b_tick_color_rotation(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_b_color_rotation_counter = 3;
    data_fd2_shared_rng_seed = 8192;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
        data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0] = 6;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_color_rotation_counter, 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[0], 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[0], 6);
    ASSERT_EQ((long)r, 0);
}

/* terminate_flag==1 blocks the frame-7 rotation entirely: frame stays 7,
 * counter and color_idx unchanged. */
static void test_summon_b_tick_rotation_blocked_by_terminate(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 1;
    data_fd2_battle_summon_anim_variant_b_color_rotation_counter = 3;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0] = 6;
    fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0], 7);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_color_rotation_counter, 3);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[0], 0);
}

/* Rotation counter wraps mod 10: counter 9 -> (9+1)%10 == 0. */
static void test_summon_b_tick_rotation_mod10_wrap(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_b_color_rotation_counter = 9;
    data_fd2_shared_rng_seed = 8192;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0] = 6;
    fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_color_rotation_counter, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[0], 0);
}

static void test_summon_e_init(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 3);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[15], -30);
}

static void test_summon_e_state3(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x22);
}

/* TICK (state 2): every slot advances frame_counter by 1; done_flag (return 1)
 * is set iff some slot's post-increment counter == 4. All 16 frames 3 -> all
 * become 4: all in visible range [0,8) so all 16 blit, every slot reaches
 * done(4), and frame 3 triggers no SFX (not 0, not 4). */
static void test_summon_e_tick_done_and_counter(void)
{
    int r;
    int i;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 16; i++)
        data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i] = 3;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 1);
    for (i = 0; i < 16; i++)
        ASSERT_EQ(
            (long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i],
            4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 16);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}

/* frame==0 SFX branch: all 16 frames 0 -> each plays with_handle, none plays
 * sample_from_bank; 0 is in [0,8) so all 16 blit; post-increment 1 != 4 so no
 * done. State 5 also TICKs (same path as state 2). */
static void test_summon_e_tick_sfx_frame0(void)
{
    int r;
    int i;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 16; i++)
        data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i] = 0;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 16);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 16);
    for (i = 0; i < 16; i++)
        ASSERT_EQ(
            (long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i],
            1);
}

/* frame==4 SFX branch: all 16 frames 4 -> each plays sample_from_bank, none
 * plays with_handle; 4 is in [0,8) so all 16 blit; post-increment 5 != 4 so no
 * done. */
static void test_summon_e_tick_sfx_frame4(void)
{
    int r;
    int i;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 16; i++)
        data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i] = 4;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 16);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 16);
    for (i = 0; i < 16; i++)
        ASSERT_EQ(
            (long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i],
            5);
}

/* Blit visibility gate boundaries: blit fires iff pre-increment frame in [0,8).
 * slot0=-1 (below range, no blit), slot1=7 (top in-range, blit),
 * slot2=8 (above range boundary, no blit), all others -1 (no blit) -> 1 blit.
 * No frame is 0 or 4, so no SFX fires (isolates the blit gate). */
static void test_summon_e_tick_blit_visibility_guard(void)
{
    int i;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 16; i++)
        data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i] = -1;
    data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[1] = 7;
    data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[2] = 8;
    fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}

/* state 6: variant-E has no terminate logic, returns 2 with no side effects. */
static void test_summon_e_state6(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 2);
}

/* default branch: any other state_code returns 0. */
static void test_summon_e_default_state(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 7);
    ASSERT_EQ((long)r, 0);
}

static void test_summon_minor_init_state(void)
{
    int r;
    r = fd2_tick_summon_spell_minor_animation_state(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 0x14);
    ASSERT_EQ((long)data_fd2_battle_summon_minor_anim_alternating_blit_toggle, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_minor_anim_state5_frame_counter, 1);
}

static void test_summon_minor_state3_hold(void)
{
    int r;
    r = fd2_tick_summon_spell_minor_animation_state(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x3C);
}

static void test_summon_minor_state5_ramp(void)
{
    int r;
    data_fd2_battle_summon_minor_anim_state5_frame_counter = 0x11;
    r = fd2_tick_summon_spell_minor_animation_state(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_minor_anim_state5_frame_counter, 0x12);
}

static void test_summon_minor_state5_done(void)
{
    int r;
    data_fd2_battle_summon_minor_anim_state5_frame_counter = 0x2B;
    r = fd2_tick_summon_spell_minor_animation_state(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_minor_anim_state5_frame_counter, 0x2C);
}

/* ---- Test: fd2_set_bgm_track_with_fade ---- */

static void test_bgm_stop_with_fade(void)
{
    data_fd2_audio_bgm_last_set_track_id = 5;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(0xFFFFFFFF, 0);
    ASSERT_EQ((long)g_ail_vol_calls, 1);
    ASSERT_EQ((long)g_ail_last_vol, 0);
    ASSERT_EQ((long)g_ail_last_ramp, 4000);
}

static void test_bgm_same_track_noop(void)
{
    data_fd2_audio_bgm_last_set_track_id = 3;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(3, 1);
    ASSERT_EQ((long)g_ail_vol_calls, 0);
}

static void test_bgm_change_regular_track(void)
{
    data_fd2_audio_bgm_last_set_track_id = 0xFF;
    data_fd2_audio_bgm_enabled_flag = 1;
    data_fd2_audio_bgm_driver_available_flag = 1;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(5, 1);
    ASSERT_EQ((long)data_fd2_audio_bgm_last_set_track_id, 5);
    ASSERT_EQ((long)g_ail_vol_calls, 2);
    ASSERT_EQ((long)g_ail_last_vol, 0x7F);
    ASSERT_EQ((long)g_ail_last_ramp, 2000);
}

static void test_bgm_disabled_zero_volume(void)
{
    data_fd2_audio_bgm_last_set_track_id = 0xFF;
    data_fd2_audio_bgm_enabled_flag = 0;
    data_fd2_audio_bgm_driver_available_flag = 1;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(5, 1);
    ASSERT_EQ((long)g_ail_last_vol, 0);
    ASSERT_EQ((long)g_ail_last_ramp, 0);
    data_fd2_audio_bgm_enabled_flag = 1;
}

/* ---- Test: fd2_init_battle_state_for_chapter ---- */

static void test_init_battle_state_zeros_cursor(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_view_window_origin_y = 3;
    data_fd2_chapter_event_or_battle_end_code = 99;
    fd2_init_battle_state_for_chapter();
    ASSERT_EQ((long)data_fd2_battle_cursor_world_x, 0);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_y, 0);
    ASSERT_EQ((long)data_fd2_battle_cursor_screen_x, 0);
    ASSERT_EQ((long)data_fd2_battle_cursor_screen_y, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0);
    ASSERT_EQ((long)data_fd2_chapter_event_or_battle_end_code, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
    ASSERT_EQ((long)data_fd2_battle_turn_counter, 1);
}

/* ---- Test: fd2_score_spell_candidate ---- */

static void test_spell_score_damage_kill_shot(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].char_id = 5;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 30;
    data_fd2_battle_spell_effect_table[2].damage = 50;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(2, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 0x18);
}

static void test_spell_score_damage_non_kill(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[1].char_id = 5;
    *(uint16 *)((uint8 *)&g_test_rc_array[1] + 0x40) = 200;
    data_fd2_battle_spell_effect_table[3].damage = 50;
    tgt_buf[0] = 1;
    score = fd2_score_spell_candidate(3, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 8);
}

static void test_spell_score_damage_priority_enemy(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].char_id = 0;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 200;
    data_fd2_battle_spell_effect_table[1].damage = 50;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(1, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 12);
}

static void test_spell_score_heal_critical(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 10;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 100;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0xd, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 8);
}

static void test_spell_score_heal_moderate(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 40;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 100;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0xd, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 3);
}

static void test_spell_score_heal_full(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 90;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 100;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0xd, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 0);
}

static void test_spell_score_heal_boost_doubles(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 10;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 100;
    ((uint8 *)&g_test_rc_array[0])[0x34] = 0x01;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0xd, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 16);
}

static void test_spell_score_cure_poison(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    ((uint8 *)&g_test_rc_array[0])[0x25] = 1;
    ((uint8 *)&g_test_rc_array[1])[0x25] = 0;
    tgt_buf[0] = 0;
    tgt_buf[1] = 1;
    score = fd2_score_spell_candidate(0x14, 2, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 6);
}

static void test_spell_score_silence(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    ((uint8 *)&g_test_rc_array[0])[0x27] = 0;
    g_build_spell_list_return = 3;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0x16, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 6);
    g_build_spell_list_return = 0;
}

/* ---- Test: fd2_tick_status_effects_and_show_messages ---- */

static void test_status_tick_poison_damage(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
    ((uint8 *)&g_test_rc_array[0])[0x25] = 1;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 100;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 200;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)*(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40), 80);
    data_fd2_battle_party_member_count = 4;
}

static void test_status_tick_poison_clamp_zero(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
    ((uint8 *)&g_test_rc_array[0])[0x25] = 1;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 5;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 200;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)*(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40), 0);
    data_fd2_battle_party_member_count = 4;
}

static void test_status_tick_poison_skip_dead(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 1;
    ((uint8 *)&g_test_rc_array[0])[0x25] = 1;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 100;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 200;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)*(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40), 100);
    data_fd2_battle_party_member_count = 4;
}

static void test_status_tick_timer_decrement(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
    ((uint8 *)&g_test_rc_array[0])[0x22] = 3;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)((uint8 *)&g_test_rc_array[0])[0x22], 2);
    data_fd2_battle_party_member_count = 4;
}

static void test_status_tick_timer_expires_recalc(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
    ((uint8 *)&g_test_rc_array[0])[0x22] = 1;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)((uint8 *)&g_test_rc_array[0])[0x22], 0);
    data_fd2_battle_party_member_count = 4;
}

static void test_spell_score_unknown_id(void)
{
    uint8 tgt_buf[2];
    int score;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0x20, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 0);
}

void run_battle_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: battle_core\n");
    RUN_TEST(test_rng_advance);
    RUN_TEST(test_rng_deterministic);
    RUN_TEST(test_deduct_mp);
    RUN_TEST(test_heal_basic);
    RUN_TEST(test_heal_cap_at_max);
    RUN_TEST(test_heal_xp_job_modifier);
    RUN_TEST(test_heal_xp_no_job_modifier);
    RUN_TEST(test_heal_spell_to_target);
    RUN_TEST(test_damage_basic);
    RUN_TEST(test_damage_floor_at_zero);
    RUN_TEST(test_damage_xp_kill_full_reward);
    RUN_TEST(test_damage_xp_survive_proportional);
    RUN_TEST(test_eatk_miss_boundary);
    RUN_TEST(test_eatk_hit_no_crit_damage);
    RUN_TEST(test_eatk_crit_halves_dp);
    RUN_TEST(test_eatk_poison_sets_status);
    RUN_TEST(test_eatk_xp_kill_full);
    RUN_TEST(test_eatk_xp_survive_proportional);
    RUN_TEST(test_eatk_terrain_ap_bonus);
    RUN_TEST(test_magic_damage_miss);
    RUN_TEST(test_magic_damage_hit);
    RUN_TEST(test_magic_damage_hit_boundary_33);
    RUN_TEST(test_magic_damage_miss_boundary_32);
    RUN_TEST(test_magic_damage_status_immune);
    RUN_TEST(test_counter_attack_sleep);
    RUN_TEST(test_counter_attack_not_adjacent);
    RUN_TEST(test_counter_attack_no_weapon);
    RUN_TEST(test_counter_attack_weapon_range_not_one);
    RUN_TEST(test_counter_attack_success_melee);
    RUN_TEST(test_counter_attack_success_negative_delta);
    RUN_TEST(test_recompute_stats_basic);
    RUN_TEST(test_recompute_stats_equipped);
    RUN_TEST(test_recalc_combat_stats_basic);
    RUN_TEST(test_recalc_combat_stats_ap_dp_buff);
    RUN_TEST(test_recalc_combat_stats_dx_buff);
    RUN_TEST(test_recalc_combat_stats_equipped_item);
    RUN_TEST(test_default_attack_sleep);
    RUN_TEST(test_default_attack_not_adjacent);
    RUN_TEST(test_mp_heal_basic);
    RUN_TEST(test_mp_heal_cap_at_max);
    RUN_TEST(test_mp_heal_xp_credit);
    RUN_TEST(test_mp_heal_xp_no_job_modifier);
    RUN_TEST(test_combat_bubble_pos_facing_down);
    RUN_TEST(test_stat_preview_basic);
    RUN_TEST(test_flash_char_hit_enemy);
    RUN_TEST(test_combat_hit_outcome_zero_stats);
    RUN_TEST(test_face_toward_target_down);
    RUN_TEST(test_face_toward_target_left);
    RUN_TEST(test_face_toward_target_up);
    RUN_TEST(test_face_toward_target_right);
    RUN_TEST(test_face_toward_target_tie_prefers_vertical);
    RUN_TEST(test_immunity_job_0x13);
    RUN_TEST(test_immunity_portrait_0x1c_overrides);
    RUN_TEST(test_immunity_archetype_4);
    RUN_TEST(test_no_immunity_normal);
    RUN_TEST(test_ai_dispatch_early_exit_dead);
    RUN_TEST(test_ai_dispatch_case0_runs);
    RUN_TEST(test_ai_dispatch_case8_skip);
    RUN_TEST(test_ai_dispatch_case11_both_execute);
    RUN_TEST(test_ai_pass_turn_hp_full);
    RUN_TEST(test_ai_pass_turn_poisoned);
    RUN_TEST(test_ai_pass_turn_heals_20pct);
    RUN_TEST(test_ai_pass_turn_clamps_at_max);
    RUN_TEST(test_attack_dispatch_all_low);
    RUN_TEST(test_ai_walk_no_path);
    RUN_TEST(test_ai_walk_candidate_taxi_tiebreak);
    RUN_TEST(test_ai_walk_stage_b_furthest_tile);
    RUN_TEST(test_compute_aoe_no_targets);
    RUN_TEST(test_compute_aoe_mode2_cross);
    RUN_TEST(test_compute_aoe_mode1_radius_bubble);
    RUN_TEST(test_compute_aoe_team_filter_branches);
    RUN_TEST(test_ai_score_phys_no_weapon);
    RUN_TEST(test_ai_score_phys_normal_hit_score8);
    RUN_TEST(test_ai_score_phys_kill_shot_score12);
    RUN_TEST(test_ai_score_phys_negligible_score0);
    RUN_TEST(test_ai_score_phys_counter_and_flank);
    RUN_TEST(test_ai_score_phys_terrain_bonus_lifts_class);
    RUN_TEST(test_ai_score_item_short_range_score8);
    RUN_TEST(test_ai_score_item_long_range_line);
    RUN_TEST(test_ai_score_item_ctx_flag_aoe_arg);
    RUN_TEST(test_ai_score_item_non_offensive_skip);
    RUN_TEST(test_ai_score_item_best_candidate_gating);
    RUN_TEST(test_ai_spell_gate_no_castable);
    RUN_TEST(test_ai_spell_gate_silenced);
    RUN_TEST(test_ai_spell_mp_gate_skips);
    RUN_TEST(test_ai_spell_happy_path_capture);
    RUN_TEST(test_ai_spell_ctx_flag_aoe_arg);
    RUN_TEST(test_ai_spell_base_dmg_tiebreak);
    RUN_TEST(test_ai_seek_optimal_unreachable);
    RUN_TEST(test_ai_seek_optimal_already_at_best);
    RUN_TEST(test_ai_seek_optimal_walk_branch_returns_zero);
    RUN_TEST(test_ai_advance_no_target);
    RUN_TEST(test_ai_advance_target_found);
    RUN_TEST(test_spell_score_damage_kill_shot);
    RUN_TEST(test_spell_score_damage_non_kill);
    RUN_TEST(test_spell_score_damage_priority_enemy);
    RUN_TEST(test_spell_score_heal_critical);
    RUN_TEST(test_spell_score_heal_moderate);
    RUN_TEST(test_spell_score_heal_full);
    RUN_TEST(test_spell_score_heal_boost_doubles);
    RUN_TEST(test_spell_score_cure_poison);
    RUN_TEST(test_spell_score_silence);
    RUN_TEST(test_spell_score_unknown_id);
    RUN_TEST(test_main_menu_new_game);
    RUN_TEST(test_main_menu_fallback);
    RUN_TEST(test_main_menu_continue_quit);
    RUN_TEST(test_summon_d_init);
    RUN_TEST(test_summon_d_state3_hold);
    RUN_TEST(test_summon_d_state6_terminate);
    RUN_TEST(test_summon_d_tick_toggle_skips_update);
    RUN_TEST(test_summon_d_tick_advance_no_done);
    RUN_TEST(test_summon_d_tick_done_at_frame2);
    RUN_TEST(test_summon_d_tick_color_rotation);
    RUN_TEST(test_summon_d_tick_sfx_bucket_split);
    RUN_TEST(test_summon_8slot_init);
    RUN_TEST(test_summon_8slot_state5_done_flag);
    RUN_TEST(test_summon_8slot_state5_no_done);
    RUN_TEST(test_summon_8slot_state5_inverse_gate);
    RUN_TEST(test_summon_8slot_state4_sfx_trigger);
    RUN_TEST(test_summon_8slot_state4_sfx_no_trigger);
    RUN_TEST(test_summon_8slot_state4_visibility_gate);
    RUN_TEST(test_summon_8slot_state4_blit_y_arithmetic);
    RUN_TEST(test_summon_8slot_state4_blit_y_enemy_team_offset);
    RUN_TEST(test_summon_8slot_default_state);
    RUN_TEST(test_summon_main_init);
    RUN_TEST(test_summon_main_state3_hold);
    RUN_TEST(test_summon_main_state6_terminate);
    RUN_TEST(test_summon_main_default_state);
    RUN_TEST(test_summon_main_tick_toggle_skips_update);
    RUN_TEST(test_summon_main_tick_advance_no_done);
    RUN_TEST(test_summon_main_tick_done_at_frame3);
    RUN_TEST(test_summon_main_tick_sfx_bucket_split);
    RUN_TEST(test_summon_main_tick_done_without_sample_when_spr_nonzero);
    RUN_TEST(test_summon_main_tick_color_rotation);
    RUN_TEST(test_summon_main_tick_rotation_blocked_by_terminate);
    RUN_TEST(test_summon_main_tick_rotation_mod12_wrap);
    RUN_TEST(test_summon_main_tick_blit_y_arithmetic);
    RUN_TEST(test_summon_main_tick_blit_y_enemy_team_offset);
    RUN_TEST(test_summon_generic_reset);
    RUN_TEST(test_summon_generic_state3);
    RUN_TEST(test_summon_generic_state5_advance);
    RUN_TEST(test_summon_generic_state6);
    RUN_TEST(test_summon_generic_state5_wrap);
    RUN_TEST(test_summon_a_init);
    RUN_TEST(test_summon_a_state6_terminate);
    RUN_TEST(test_summon_a_state3);
    RUN_TEST(test_summon_a_tick_blit_gate_sfx_done);
    RUN_TEST(test_summon_a_tick_color_rotation);
    RUN_TEST(test_summon_a_tick_rotation_mod10_wrap);
    RUN_TEST(test_summon_b_init);
    RUN_TEST(test_summon_b_state3);
    RUN_TEST(test_summon_b_state6_terminate);
    RUN_TEST(test_summon_b_tick_increment_no_done);
    RUN_TEST(test_summon_b_tick_done_at_frame2);
    RUN_TEST(test_summon_b_tick_blit_visibility_guard);
    RUN_TEST(test_summon_b_tick_sfx_bucket_split);
    RUN_TEST(test_summon_b_tick_color_rotation);
    RUN_TEST(test_summon_b_tick_rotation_blocked_by_terminate);
    RUN_TEST(test_summon_b_tick_rotation_mod10_wrap);
    RUN_TEST(test_summon_e_init);
    RUN_TEST(test_summon_e_state3);
    RUN_TEST(test_summon_e_tick_done_and_counter);
    RUN_TEST(test_summon_e_tick_sfx_frame0);
    RUN_TEST(test_summon_e_tick_sfx_frame4);
    RUN_TEST(test_summon_e_tick_blit_visibility_guard);
    RUN_TEST(test_summon_e_state6);
    RUN_TEST(test_summon_e_default_state);
    RUN_TEST(test_summon_minor_init_state);
    RUN_TEST(test_summon_minor_state3_hold);
    RUN_TEST(test_summon_minor_state5_ramp);
    RUN_TEST(test_summon_minor_state5_done);
    RUN_TEST(test_bgm_stop_with_fade);
    RUN_TEST(test_bgm_same_track_noop);
    RUN_TEST(test_bgm_change_regular_track);
    RUN_TEST(test_bgm_disabled_zero_volume);
    RUN_TEST(test_init_battle_state_zeros_cursor);
    RUN_TEST(test_status_tick_poison_damage);
    RUN_TEST(test_status_tick_poison_clamp_zero);
    RUN_TEST(test_status_tick_poison_skip_dead);
    RUN_TEST(test_status_tick_timer_decrement);
    RUN_TEST(test_status_tick_timer_expires_recalc);
    printf("\n");
}
