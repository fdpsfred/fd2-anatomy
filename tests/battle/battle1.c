/*
 * unit tests for src/battle/battle.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

#define USE_ITEM_ID 10

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
extern uint8 g_spell_list_buf[12];
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;


/* ================================================================
 * fd2_execute_attack_damage_calculation @ 0x1ECC7 — full-path tests.
 *
 * CORE PHYSICAL COMBAT FORMULA. Drives the REAL accessors compiled into
 * the test build: fd2_get_item_effect_entry / fd2_get_enemy_data_entry
 * (table.c), fd2_get_inventory_slot_item_id / fd2_check_char_status_immunity
 * / fd2_read_tile_attribute_at_pos (battle.c), fd2_advance_rng_state (misc),
 * and the REAL VGA palette routines (palette.c) on the crit/poison branches.
 * The attacker (char 0) is equipped at slot 0 (eatk_reset) so the REAL
 * fd2_find_equipped_item_by_kind(attacker,0) returns slot 0 -> weapon =
 * item_effect_table[0]. fd2_delay_ticks -> no-op.
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


static void eatk_reset(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    memset(data_fd2_battle_enemy_data_table, 0,
           sizeof(data_fd2_battle_enemy_data_table));
    memset(data_fd2_battle_job_crit_rate_table, 0,
           sizeof(data_fd2_battle_job_crit_rate_table));
    /* Attacker (char 0) holds an equipped weapon in slot 0 so the REAL
     * fd2_find_equipped_item_by_kind(attacker,0) returns slot 0; item id 0
     * (the memset default) -> weapon_entry = item_effect_table[0], which the
     * poison/double-hit cases configure directly. */
    g_test_rc_array[0].inventory_slots[0] = 0x40;   /* equipped flag */
    g_test_rc_array[0].inventory_slots[1] = 0;      /* weapon item id 0 */
    data_fd2_shared_rng_seed = 0;
    data_fd2_battle_last_hit_or_miss_flag = 1;
    data_fd2_battle_pending_xp_credit = 0;
}


/* ---- Tests: get_inventory_slot_item_id ---- */

static void test_get_inventory_slot_item_id(void)
{
    uint8 r;
    g_test_rc_array[1].inventory_slots[2] = 0x40;
    g_test_rc_array[1].inventory_slots[3] = 0x2A;
    r = fd2_get_inventory_slot_item_id(1, 1);
    ASSERT_EQ(r, 0x2A);
}


/* ---- Tests: read_tile_attribute ---- */

/* sprite_idx==0 baseline: pins the +0 attr lookup and the terrain 0x1F mask
 * (meta byte 0xA3 -> 0x03). The sprite word is 0 here so the 0x3FF mask is not
 * exercised; that is covered by test_read_tile_attribute_sprite_mask below. */
static void test_read_tile_attribute(void)
{
    uint8 fake_map[16];
    uint8 fake_attr[4];
    uint8 out[8];

    memset(fake_map, 0, sizeof(fake_map));
    fake_map[4] = 0x00; fake_map[5] = 0x00;
    fake_map[6] = 0xA3;
    data_fd2_battle_tile_map_ptr = (uint32)fake_map;
    data_fd2_battle_map_width_tiles = 1;
    fake_attr[0] = 0xAA; fake_attr[1] = 0xBB;
    fake_attr[2] = 0xCC; fake_attr[3] = 0xDD;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)fake_attr;
    fd2_read_tile_attribute_at_pos(0, 0, (uint32)out);
    ASSERT_EQ(*(uint16 *)out, 0);
    ASSERT_EQ(*(uint16 *)(out + 2), 3);
    ASSERT_EQ(out[4], 0xAA);
    ASSERT_EQ(out[7], 0xDD);
}


/* 10-bit sprite mask: asm `MOV BX,[EAX]; AND BH,0x3` (0x12e67/0x12e6a) masks the
 * sprite word to 0x3FF before both the out[+0] store and the attr-table index.
 * Sprite word 0xFC07 -> 0xFC07 & 0x3FF == 0x0007 (high 6 bits dropped). A
 * mistranscribed mask (0x1FF / 0xFFF / missing) would change out[+0] and the
 * looked-up attr bytes, so both are asserted. Index 7 -> attr base + (int16)7*4
 * == +28, so fake_attr is sized to 32 with distinct bytes at 28..31; the
 * (int16) sign-extension equals zero-extension here since masked idx <= 0x3FF.
 * Terrain meta 0x5C & 0x1F == 0x1C also keeps the 5-bit mask exercised. */
static void test_read_tile_attribute_sprite_mask(void)
{
    uint8 fake_map[16];
    uint8 fake_attr[32];
    uint8 out[8];

    memset(fake_map, 0, sizeof(fake_map));
    memset(fake_attr, 0, sizeof(fake_attr));
    *(uint16 *)(fake_map + 4) = 0xFC07;
    fake_map[6] = 0x5C;
    data_fd2_battle_tile_map_ptr = (uint32)fake_map;
    data_fd2_battle_map_width_tiles = 1;
    fake_attr[28] = 0x11; fake_attr[29] = 0x22;
    fake_attr[30] = 0x33; fake_attr[31] = 0x44;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)fake_attr;
    fd2_read_tile_attribute_at_pos(0, 0, (uint32)out);
    ASSERT_EQ(*(uint16 *)out, 0x0007);
    ASSERT_EQ(*(uint16 *)(out + 2), 0x1C);
    ASSERT_EQ(out[4], 0x11);
    ASSERT_EQ(out[5], 0x22);
    ASSERT_EQ(out[6], 0x33);
    ASSERT_EQ(out[7], 0x44);
}


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
 * fd2_find_equipped_item_by_kind is the REAL function scanning the defender's
 * inventory; the slot it returns indexes inventory_slots[slot*2+1] (the REAL
 * fd2_get_inventory_slot_item_id), which holds the equipped item id fed to the
 * REAL fd2_get_item_effect_entry. Each success case equips defender slot 0
 * (flag 0x40 + item id).
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


/* (b) no weapon: adjacent, awake, defender has no equipped slot so the REAL
 * fd2_find_equipped_item_by_kind(1,0) returns 0xFFFFFFFF.
 * 0x1F15B CMP EAX,-1 / 0x1F15E JZ 0x1F17F reaches the epilogue with EAX=-1. */
static void test_counter_attack_no_weapon(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].pos_x = 0;        /* dx=1, dy=0 -> adjacent */
    g_test_rc_array[1].pos_y = 0;        /* char 1 inventory empty -> no equip */
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, -1);
}


/* (c) weapon range != 1 (bow/spear): adjacent, awake, defender slot 0 equipped
 * with item 5 (range_min = 2). The REAL find_equipped returns slot 0.
 * 0x1F176 MOVZX [EAX+0xB] / 0x1F17A CMP 1 / 0x1F17D JNZ 0x1F117 -> EAX=-1. */
static void test_counter_attack_weapon_range_not_one(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].pos_x = 0;
    g_test_rc_array[1].pos_y = 0;
    g_test_rc_array[1].inventory_slots[0] = 0x40; /* slot 0 equipped */
    g_test_rc_array[1].inventory_slots[1] = 5;    /* slot 0 item id = 5 */
    data_fd2_battle_item_effect_table[5].range_min = 2;  /* range != 1 */
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, -1);
}


/* (d) SUCCESS, positive delta: adjacent, awake, defender slot 0 equipped with
 * item 5 (range_min == 1, melee). Fall-through to 0x1F17F with EAX=1 -> 1.
 * This is the EAX-fall-through equivalence the emit's explicit return 1 claims. */
static void test_counter_attack_success_melee(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].pos_x = 0;        /* dx=1, dy=0 -> adjacent */
    g_test_rc_array[1].pos_y = 0;
    g_test_rc_array[1].inventory_slots[0] = 0x40; /* slot 0 equipped */
    g_test_rc_array[1].inventory_slots[1] = 5;
    data_fd2_battle_item_effect_table[5].range_min = 1;  /* melee */
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, 1);
}


/* (e) SUCCESS with NEGATIVE delta: attacker pos < defender pos so the SUB
 * underflows (e.g. 4-5 = -1) before abs(). attacker (5,4), defender (5,5):
 * dx=abs(0)=0, dy=abs(-1)=1, sum=1. Proves abs() handles the signed delta;
 * a broken abs would yield a huge sum != 1 and return -1 instead of 1. */
static void test_counter_attack_success_negative_delta(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 4;        /* attacker.y < defender.y */
    g_test_rc_array[1].pos_x = 5;
    g_test_rc_array[1].pos_y = 5;        /* SUB y: 4-5 = -1 -> abs -> 1 */
    g_test_rc_array[1].inventory_slots[0] = 0x40; /* slot 0 equipped */
    g_test_rc_array[1].inventory_slots[1] = 5;
    data_fd2_battle_item_effect_table[5].range_min = 1;
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, 1);
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


void run_battle_battle1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: battle/battle (1/2)\n");
    RUN_TEST(test_get_inventory_slot_item_id);
    RUN_TEST(test_read_tile_attribute);
    RUN_TEST(test_read_tile_attribute_sprite_mask);
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
    printf("\n");
}
