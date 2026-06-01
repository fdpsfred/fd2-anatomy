/*
 * testspel.c — Unit tests for spell handler dispatch functions
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

static void test_apply_item_stat_modifier(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    target_id = 1;
    fd2_apply_item_stat_modifier_with_anim(
        0, 10, 0x48, 0, 1, (uint32)&target_id, 0x11);
    ASSERT_EQ(g_test_rc_array[1].ap, 10);
}

static void test_set_full_palette_smoke(void)
{
    fd2_set_full_vga_palette_to_color(0x3F, 0x3F, 0x3F);
    ASSERT_TRUE(1);
}

static void test_spell_handler_0_smoke(void)
{
    uint8 target_id;
    memset(g_test_rc_array, 0, sizeof(runtime_char) * 8);
    target_id = 1;
    fd2_spell_handler_id_0_via_targeted_blink(0, 1, &target_id);
    ASSERT_TRUE(1);
}

void run_spell_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: spell_handlers\n");
    RUN_TEST(test_spell_17_deducts_mp);
    RUN_TEST(test_spell_17_xp_with_job_bonus);
    RUN_TEST(test_spell_17_xp_no_job_bonus);
    RUN_TEST(test_apply_status_effect_deducts_mp);
    RUN_TEST(test_apply_item_stat_modifier);
    RUN_TEST(test_set_full_palette_smoke);
    RUN_TEST(test_spell_handler_0_smoke);
    printf("\n");
}
