/*
 * unit tests for src/table/table.c
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


/* globals + stubs in testglob.c */

/* ---- Tests ---- */

static void test_item_effect_entry_0(void)
{
    uint8 *result = fd2_get_item_effect_entry(0);
    uint8 *expected = (uint8 *)&data_fd2_battle_item_effect_table[0].type;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_item_effect_entry_100(void)
{
    uint8 *result = fd2_get_item_effect_entry(100);
    uint8 *expected = (uint8 *)&data_fd2_battle_item_effect_table[100].type;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_spell_effect_entry_0(void)
{
    uint8 *result = fd2_get_spell_effect_entry(0);
    uint8 *expected = (uint8 *)&data_fd2_battle_spell_effect_table[0];
    ASSERT_EQ((long)result, (long)expected);
}


static void test_spell_effect_entry_35(void)
{
    uint8 *result = fd2_get_spell_effect_entry(35);
    uint8 *expected = (uint8 *)&data_fd2_battle_spell_effect_table[35];
    ASSERT_EQ((long)result, (long)expected);
}


static void test_enemy_data_entry(void)
{
    uint8 *result = fd2_get_enemy_data_entry(5);
    uint8 *expected = (uint8 *)&data_fd2_battle_enemy_data_table[5];
    ASSERT_EQ((long)result, (long)expected);
}


static void test_char_base_entry(void)
{
    uint8 *result = fd2_get_char_base_entry(0);
    uint8 *expected = (uint8 *)&data_fd2_battle_character_base_table[0];
    ASSERT_EQ((long)result, (long)expected);
}


static void test_char_growth_entry(void)
{
    uint8 *result = fd2_get_char_growth_entry(10);
    uint8 *expected = (uint8 *)&data_fd2_battle_character_growth_table[10];
    ASSERT_EQ((long)result, (long)expected);
}


static void test_chapter_intro_ch1(void)
{
    uint8 *result = fd2_get_chapter_intro_metadata_entry(1);
    uint8 *expected = data_fd2_chapter_intro_metadata_table;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_chapter_intro_ch5(void)
{
    uint8 *result = fd2_get_chapter_intro_metadata_entry(5);
    uint8 *expected = data_fd2_chapter_intro_metadata_table + 4 * 0x1F;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_spell_learning(void)
{
    uint8 *result = fd2_get_spell_learning_entry(3);
    uint8 *expected = data_fd2_spell_learning_table + 3 * 0xC;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_class_promotion(void)
{
    uint8 *result = fd2_get_class_promotion_data_entry(0x20);
    uint8 *expected = data_fd2_class_promotion_data_table;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_class_promotion_offset(void)
{
    uint8 *result = fd2_get_class_promotion_data_entry(0x25);
    uint8 *expected = data_fd2_class_promotion_data_table + 5 * 2;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_weapon_anim_deref(void)
{
    uint8 dummy_script[4];
    uint8 *result;
    dummy_script[0] = 1; dummy_script[1] = 2;
    dummy_script[2] = 3; dummy_script[3] = 4;
    data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21[3] = dummy_script;
    result = fd2_get_attack_anim_pattern_for_weapon(3);
    ASSERT_EQ((long)result, (long)dummy_script);
}


static void test_job_allowed_items(void)
{
    uint8 *result = fd2_get_job_allowed_items_table_entry(2);
    uint8 *expected = data_fd2_job_allowed_items_table + 2 * 7;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_movement_cost(void)
{
    uint8 *result = fd2_get_movement_cost_table_for_job(5);
    uint8 *expected = data_fd2_movement_cost_table + 5 * 0x14;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_cutscene_script_deref(void)
{
    uint8 dummy_script[4];
    uint8 *result;
    dummy_script[0] = 0xA; dummy_script[1] = 0xB;
    dummy_script[2] = 0xC; dummy_script[3] = 0xD;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[10] = dummy_script;
    result = fd2_get_cutscene_event_script(10);
    ASSERT_EQ((long)result, (long)dummy_script);
}


static void test_orphan_table_60181_0(void)
{
    uint8 *result = fd2_get_orphan_table_60181_entry(0);
    uint8 *expected = data_fd2_orphan_table_60181;
    ASSERT_EQ((long)result, (long)expected);
}


static void test_orphan_table_60181_offset(void)
{
    uint8 *result = fd2_get_orphan_table_60181_entry(7);
    uint8 *expected = data_fd2_orphan_table_60181 + 7 * 3;
    ASSERT_EQ((long)result, (long)expected);
}


void run_table_table_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: table/table\n");
    RUN_TEST(test_item_effect_entry_0);
    RUN_TEST(test_item_effect_entry_100);
    RUN_TEST(test_spell_effect_entry_0);
    RUN_TEST(test_spell_effect_entry_35);
    RUN_TEST(test_enemy_data_entry);
    RUN_TEST(test_char_base_entry);
    RUN_TEST(test_char_growth_entry);
    RUN_TEST(test_chapter_intro_ch1);
    RUN_TEST(test_chapter_intro_ch5);
    RUN_TEST(test_spell_learning);
    RUN_TEST(test_class_promotion);
    RUN_TEST(test_class_promotion_offset);
    RUN_TEST(test_weapon_anim_deref);
    RUN_TEST(test_job_allowed_items);
    RUN_TEST(test_movement_cost);
    RUN_TEST(test_cutscene_script_deref);
    RUN_TEST(test_orphan_table_60181_0);
    RUN_TEST(test_orphan_table_60181_offset);
    printf("\n");
}
