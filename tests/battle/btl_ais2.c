/*
 * unit tests for src/battle/btl_aisc.c
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
extern int    g_mini_panel_calls;
extern uint32 g_mini_panel_last_buf;
extern uint32 g_mini_panel_last_stride;
extern uint32 g_mini_panel_last_char;
extern int g_find_equipped_return;
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
extern int g_count_usable_slots_return;
extern uint8 g_spell_list_buf[12];
extern int g_remove_inventory_calls;
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;


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


static void test_spell_score_unknown_id(void)
{
    uint8 tgt_buf[2];
    int score;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0x20, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 0);
}


static void test_score_item_candidate_damage(void)
{
    uint8 tgt[1];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 10;
    g_test_rc_array[0].hp_max = 100;
    data_fd2_battle_item_effect_table[0].use_effect = 5;
    tgt[0] = 0;
    result = fd2_score_item_candidate(0, 1, (uint32)tgt);
    ASSERT_EQ(result, 8);
}


/* HP-damage effect path (use_effect 5/0xD), middle band:
 * hp_max/3 < hp <= hp_max/2  -> per_score 3 (asm 0x158c4 CMP/JG, 0x158c8 MOV 3).
 * hp 40, max 100: max/3=33 (40>33), max/2=50 (40<=50) -> 3. aux[0x34] clear. */
static void test_score_item_candidate_score3(void)
{
    uint8 tgt[1];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 40;
    g_test_rc_array[0].hp_max = 100;
    data_fd2_battle_item_effect_table[0].use_effect = 5;
    tgt[0] = 0;
    result = fd2_score_item_candidate(0, 1, (uint32)tgt);
    ASSERT_EQ(result, 3);
}


/* HP-damage effect path, high band: hp > hp_max/2 -> per_score 0
 * (asm 0x158c6 JG -> 0x158cf XOR EAX,EAX). hp 60, max 100: 60>50 -> 0. */
static void test_score_item_candidate_score0(void)
{
    uint8 tgt[1];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 60;
    g_test_rc_array[0].hp_max = 100;
    data_fd2_battle_item_effect_table[0].use_effect = 5;
    tgt[0] = 0;
    result = fd2_score_item_candidate(0, 1, (uint32)tgt);
    ASSERT_EQ(result, 0);
}


/* HP-damage effect path, high-value-target amplification: if
 * combat_aux_block[0x0D] bit 0x80 set (struct +0x34), per_score *= 3
 * (asm 0x158d1 TEST .. 0x158d9 SHL EAX,2 / SUB EAX,EBX). Two targets in one
 * call cover both amplified bands: base 8 (hp<=max/3) -> 24, base 3 -> 9;
 * total 33. Asserts the *3 is applied per-target before summation. */
static void test_score_item_candidate_x3_amplify(void)
{
    uint8 tgt[2];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 10;          /* base 8  -> *3 = 24 */
    g_test_rc_array[0].hp_max = 100;
    g_test_rc_array[0].combat_aux_block[0x0D] = 0x80;
    g_test_rc_array[1].hp_current = 40;          /* base 3  -> *3 = 9  */
    g_test_rc_array[1].hp_max = 100;
    g_test_rc_array[1].combat_aux_block[0x0D] = 0x80;
    data_fd2_battle_item_effect_table[0].use_effect = 5;
    tgt[0] = 0;
    tgt[1] = 1;
    result = fd2_score_item_candidate(0, 2, (uint32)tgt);
    ASSERT_EQ(result, 33);
}


/* Spell-wrapper effect path, non-0x18 (use_effect 0x14/0x15): threshold comes
 * from the wrapped spell's damage (pSpell[0]); the spell id is item.use_param
 * (uint16 at struct +15/+16, read via fd2_get_spell_effect_entry's EAX return,
 * asm 0x15938 CALL / 0x15946 MOVZX EBP,[EAX]). Per target: hp<=threshold ->
 * 0x12 (kill), hp>threshold -> 8. spell 4 damage 50; two targets hp 30 (<=50
 * kill 0x12) and hp 70 (>50 normal 8) -> 0x12+8 = 0x1A. Item index 9. */
static void test_score_item_candidate_spell_wrapper(void)
{
    uint8 tgt[2];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 30;          /* 30 <= 50 -> 0x12 */
    g_test_rc_array[0].hp_max = 100;
    g_test_rc_array[1].hp_current = 70;          /* 70 >  50 -> 8    */
    g_test_rc_array[1].hp_max = 100;
    data_fd2_battle_spell_effect_table[4].damage = 50;
    data_fd2_battle_item_effect_table[9].use_effect = 0x14;
    data_fd2_battle_item_effect_table[9].use_param_lo = 4;   /* spell id 4 */
    data_fd2_battle_item_effect_table[9].use_param_hi = 0;
    tgt[0] = 0;
    tgt[1] = 1;
    result = fd2_score_item_candidate(9, 2, (uint32)tgt);
    ASSERT_EQ(result, 0x12 + 8);
}


/* Spell-wrapper effect path, 0x18: threshold is item.use_param itself (the
 * uint16 at struct +15/+16); the spell getter's result is IGNORED (asm 0x15941
 * CMP ESI,0x18 / JZ 0x15949 skips MOVZX EBP,[EAX]). use_param 50, wrapped spell
 * id 4 damage deliberately 999 to prove its damage is NOT used as threshold.
 * Two targets hp 30 (<=50 -> 0x12) and hp 70 (>50 -> 8) -> 0x1A. Item index 10. */
static void test_score_item_candidate_spell_0x18(void)
{
    uint8 tgt[2];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 30;          /* 30 <= 50 -> 0x12 */
    g_test_rc_array[0].hp_max = 100;
    g_test_rc_array[1].hp_current = 70;          /* 70 >  50 -> 8    */
    g_test_rc_array[1].hp_max = 100;
    data_fd2_battle_spell_effect_table[4].damage = 999; /* must be ignored */
    data_fd2_battle_item_effect_table[10].use_effect = 0x18;
    data_fd2_battle_item_effect_table[10].use_param_lo = 50; /* threshold 50 */
    data_fd2_battle_item_effect_table[10].use_param_hi = 0;
    tgt[0] = 0;
    tgt[1] = 1;
    result = fd2_score_item_candidate(10, 2, (uint32)tgt);
    ASSERT_EQ(result, 0x12 + 8);
}


void run_battle_btl_aisc2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: battle/btl_aisc (2/2)\n");
    RUN_TEST(test_spell_score_cure_poison);
    RUN_TEST(test_spell_score_silence);
    RUN_TEST(test_spell_score_unknown_id);
    RUN_TEST(test_score_item_candidate_damage);
    RUN_TEST(test_score_item_candidate_score3);
    RUN_TEST(test_score_item_candidate_score0);
    RUN_TEST(test_score_item_candidate_x3_amplify);
    RUN_TEST(test_score_item_candidate_spell_wrapper);
    RUN_TEST(test_score_item_candidate_spell_0x18);
    printf("\n");
}
