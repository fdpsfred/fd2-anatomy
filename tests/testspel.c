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
extern int g_remove_inventory_calls;

/* ----------------------------------------------------------------
 * fd2_apply_use_effect_dispatch coverage.
 *
 * Drives the dispatcher deterministically through the REAL table
 * accessors: fd2_get_inventory_slot_item_id(c,slot) reads
 * inventory_slots[slot*2+1]; fd2_get_item_effect_entry(id) returns
 * &item_effect_table[id].type (struct base +1), so the dispatcher's
 * item_entry[0xD]=effect_code maps to item_effect.use_effect (+0xE)
 * and item_entry[0xE]=effect_param to the u16 at struct +0xF
 * (use_param_lo|use_param_hi<<8).
 *
 * The high-risk state transition under test is the per-effect-code
 * inventory-consume decision (codes 5/6/7/0x0B consume; others do
 * not) plus code 0x13's movement_order save/restore and the finale
 * XP-credit reset. Consume is observed via the g_remove_inventory_calls
 * counter on the fd2_remove_inventory_slot_at stub. The drop-collection
 * finale runs the REAL fd2_collect_pending_death_drops over
 * data_fd2_battle_party_member_count chars into drops_buf; member_count
 * is pinned to 0 so that loop is a no-op and the buffer-arg fix (passing
 * drops_buf, not garbage) is exercised without depending on char data.
 * Pure blit/animation side effects (impact/blink/composite) and the
 * RNG-driven mp_heal/magic-damage VALUES are covered by their own direct
 * unit tests in testbtl.c and are intentionally not re-asserted here. */

#define USE_ITEM_ID 10

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
    g_remove_inventory_calls = 0;
}

/* Codes 5/6/7/0x0B must spend the inventory slot exactly once. */

static void test_use_effect_code5_consumes(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x05, 50);
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_remove_inventory_calls, 1);
}

static void test_use_effect_code6_consumes(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x06, 0);
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_remove_inventory_calls, 1);
}

static void test_use_effect_code7_consumes(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x07, 0);
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_remove_inventory_calls, 1);
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
    ASSERT_EQ(g_remove_inventory_calls, 1);
}

/* Code 0x14 (attack spell) is NON-consuming: slot must be left intact.
 * target.job_id = 1 keeps the REAL fd2_calc_magic_damage in-bounds. */
static void test_use_effect_code14_no_consume(void)
{
    uint8 target_id = 1;
    setup_use_effect(0x14, 0);
    g_test_rc_array[1].job_id = 1;
    fd2_apply_use_effect_dispatch(0, 0, 1, (uint32)&target_id);
    ASSERT_EQ(g_remove_inventory_calls, 0);
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
    RUN_TEST(test_use_effect_code5_consumes);
    RUN_TEST(test_use_effect_code6_consumes);
    RUN_TEST(test_use_effect_code7_consumes);
    RUN_TEST(test_use_effect_code0B_consumes);
    RUN_TEST(test_use_effect_code14_no_consume);
    RUN_TEST(test_use_effect_code13_restores_movement_order);
    RUN_TEST(test_use_effect_resets_xp_credit);
    printf("\n");
}
