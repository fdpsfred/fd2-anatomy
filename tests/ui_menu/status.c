/*
 * unit tests for src/ui_menu/status.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>

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
extern int g_composite_call_count;
/* data_fd2_ui_slide_* workspace ptr globals are declared in globals.h */


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


/*
 * Candidate = WEAPON (preview_cat 0x01 <= 0x14). Exercises the loop add-block
 * and the category-opposition branch (asm 0x2f052-0x2f08e), which the all-zero
 * smoke test above never enters.
 *   Base stats: AP=50 (aux+0x10), DP=20 (aux+0x12), DX=Stat4=30 (dx_block+1).
 *   Candidate item[1] (type 0x01): .ap10 .ht3 .dp2 .ev1
 *     -> AP=60 DP=22 DX=33 Stat4=31.
 *   slot0 = equipped ARMOR item[2] (type 0x15, OPPOSITE category) added:
 *     .ap0 .ht0 .dp8 .ev2 -> AP=60 DP=30 DX=33 Stat4=33.
 *   slot1 = equipped WEAPON item[3] (type 0x05, SAME category) MUST be excluded
 *     (preview replaces it): .ap100 .dp100 -> no effect, proving the exclusion.
 * fd2_get_item_effect_entry returns &item.type, so item+1/+3/+5/+7 read
 * .ap/.ht/.dp/.ev and route to AP/DX/DP/Stat4 (mirrors the sibling
 * test_recalc_combat_stats_equipped_item mapping).
 */
static void test_stat_preview_weapon_opposite_and_same_category(void)
{
    int32 stats[4];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 50;   /* base AP */
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x12) = 20;   /* base DP */
    *(int16 *)(g_test_rc_array[0].ai_target_and_dx_block + 1) = 30;/* base DX */
    data_fd2_battle_item_effect_table[1].type = 0x01;  /* candidate weapon */
    data_fd2_battle_item_effect_table[1].ap = 10;
    data_fd2_battle_item_effect_table[1].ht = 3;
    data_fd2_battle_item_effect_table[1].dp = 2;
    data_fd2_battle_item_effect_table[1].ev = 1;
    g_test_rc_array[0].inventory_slots[0] = 0x40;      /* slot0 equipped     */
    g_test_rc_array[0].inventory_slots[1] = 2;         /* armor, opposite    */
    data_fd2_battle_item_effect_table[2].type = 0x15;
    data_fd2_battle_item_effect_table[2].dp = 8;
    data_fd2_battle_item_effect_table[2].ev = 2;
    g_test_rc_array[0].inventory_slots[2] = 0x40;      /* slot1 equipped     */
    g_test_rc_array[0].inventory_slots[3] = 3;         /* weapon, same cat   */
    data_fd2_battle_item_effect_table[3].type = 0x05;
    data_fd2_battle_item_effect_table[3].ap = 100;     /* must be excluded   */
    data_fd2_battle_item_effect_table[3].dp = 100;
    fd2_compute_equipped_stats_with_item_preview(0, 1, (uint32)stats);
    ASSERT_EQ(stats[0], 60);   /* AP    = 50 + 10 + 0 */
    ASSERT_EQ(stats[1], 30);   /* DP    = 20 +  2 + 8 */
    ASSERT_EQ(stats[2], 33);   /* DX    = 30 +  3 + 0 */
    ASSERT_EQ(stats[3], 33);   /* Stat4 = 30 +  1 + 2 */
}


/*
 * Candidate = ARMOR (preview_cat 0x16 > 0x14): covers the SECOND branch path
 * (asm 0x2f061-0x2f06e). Distinct base values catch any field mis-routing.
 *   Base: AP=5 DP=7 DX=Stat4=11 (DX/Stat4 share dx_block+1).
 *   Candidate armor item[4] (type 0x16): .ap1 .ht2 .dp3 .ev4
 *     -> AP=6 DP=10 DX=13 Stat4=15.
 *   slot0 = equipped WEAPON item[5] (type 0x05, OPPOSITE) added:
 *     .ap40 .ht50 .dp60 .ev70 -> AP=46 DP=70 DX=63 Stat4=85.
 *   slot1 = equipped ARMOR item[6] (type 0x1F, SAME category) excluded.
 *   slot2 = WEAPON item[7] but NOT equipped (flag bit 0x40 clear) -> the
 *     equipped-flag gate (asm 0x2f04d TEST/JZ) skips it despite opposite cat.
 */
static void test_stat_preview_armor_branch_and_flag_gate(void)
{
    int32 stats[4];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 5;    /* base AP */
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x12) = 7;    /* base DP */
    *(int16 *)(g_test_rc_array[0].ai_target_and_dx_block + 1) = 11;/* base DX */
    data_fd2_battle_item_effect_table[4].type = 0x16;  /* candidate armor   */
    data_fd2_battle_item_effect_table[4].ap = 1;
    data_fd2_battle_item_effect_table[4].ht = 2;
    data_fd2_battle_item_effect_table[4].dp = 3;
    data_fd2_battle_item_effect_table[4].ev = 4;
    g_test_rc_array[0].inventory_slots[0] = 0x40;      /* slot0 equipped     */
    g_test_rc_array[0].inventory_slots[1] = 5;         /* weapon, opposite   */
    data_fd2_battle_item_effect_table[5].type = 0x05;
    data_fd2_battle_item_effect_table[5].ap = 40;
    data_fd2_battle_item_effect_table[5].ht = 50;
    data_fd2_battle_item_effect_table[5].dp = 60;
    data_fd2_battle_item_effect_table[5].ev = 70;
    g_test_rc_array[0].inventory_slots[2] = 0x40;      /* slot1 equipped     */
    g_test_rc_array[0].inventory_slots[3] = 6;         /* armor, same cat    */
    data_fd2_battle_item_effect_table[6].type = 0x1F;
    data_fd2_battle_item_effect_table[6].ap = 1000;    /* must be excluded   */
    data_fd2_battle_item_effect_table[6].dp = 2000;
    g_test_rc_array[0].inventory_slots[4] = 0x00;      /* slot2 NOT equipped */
    g_test_rc_array[0].inventory_slots[5] = 7;         /* weapon, opposite   */
    data_fd2_battle_item_effect_table[7].type = 0x03;
    data_fd2_battle_item_effect_table[7].ap = 999;     /* flag-gated out     */
    fd2_compute_equipped_stats_with_item_preview(0, 4, (uint32)stats);
    ASSERT_EQ(stats[0], 46);   /* AP    = 5  + 1 + 40 */
    ASSERT_EQ(stats[1], 70);   /* DP    = 7  + 3 + 60 */
    ASSERT_EQ(stats[2], 63);   /* DX    = 11 + 2 + 50 */
    ASSERT_EQ(stats[3], 85);   /* Stat4 = 11 + 4 + 70 */
}


/*
 * Signed-16-bit guard: the function reads item bonuses with MOVSX (asm
 * 0x2f070 MOVSX word). A negative bonus must DECREASE the stat. If the emit
 * ever regressed to a zero/unsigned read, AP would become 100 or 100+0xFFE2.
 *   Base AP=100; candidate weapon item[1] all-zero; slot0 equipped armor
 *   item[8] (opposite) with .ap = -30 (int16) -> AP = 70.
 */
static void test_stat_preview_signed_negative_bonus(void)
{
    int32 stats[4];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 100;  /* base AP */
    data_fd2_battle_item_effect_table[1].type = 0x01;  /* candidate weapon  */
    g_test_rc_array[0].inventory_slots[0] = 0x40;      /* slot0 equipped    */
    g_test_rc_array[0].inventory_slots[1] = 8;         /* armor, opposite   */
    data_fd2_battle_item_effect_table[8].type = 0x15;
    data_fd2_battle_item_effect_table[8].ap = (uint16)(-30);  /* int16 -30  */
    fd2_compute_equipped_stats_with_item_preview(0, 1, (uint32)stats);
    ASSERT_EQ(stats[0], 70);   /* AP = 100 + (-30), proves MOVSX sign-extend */
    ASSERT_EQ(stats[1], 0);    /* DP unchanged (no DP bonuses)               */
}


/*
 * fd2_close_status_screen_with_slide_out @ 0x196CB — end-to-end smoke.
 *
 * The function is a blit/free/recomposite teardown (no return value, no
 * branching logic beyond the fixed 1..5 slide loop), so the host-observable
 * proxy is g_composite_call_count: it must run the 5-frame slide loop, the
 * VRAM-restore memmove, the three free()s, and then composite exactly one
 * battle frame. We pre-allocate the three workspace buffers (the open
 * counterpart's job) so the real fd2_slide_panel_down_step memmoves stay in
 * bounds; the function itself free()s all three, so the test must NOT free
 * them again and resets the globals to 0 afterward to avoid dangling ptrs.
 *
 * Buffer math (proves the allocation size is sufficient): the largest
 * fd2_slide_panel_down_step write in this loop is at y_offset=0x7D
 * (frame 1: 1*0xD+0x70=0x7D) — row_count = 200-125 = 75, top write
 * 5 + 199*320 + 0x136 = 63995 < 64000; the largest read from src is
 * 0x8C05 + 0x55*0x140 + 0x136 = 63355 < 64000. All five frames stay
 * within the 64000-byte mode-13h workspaces.
 *
 * memmove((void*)0xA0000, ...) and the in-loop blit to 0xA0000 target the
 * VGA aperture; under DOS/4GW 0xA0000 is real VGA RAM so the writes are
 * harmless (same convention as tests/anim/aniwalk2.c and tests/gfx).
 */
static void test_close_status_screen_slide_out_runs_full_teardown(void)
{
    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);
    ASSERT_TRUE(data_fd2_ui_slide_anim_accumulator_buf_ptr != 0);
    ASSERT_TRUE(data_fd2_ui_slide_bg_snapshot_buf_ptr != 0);
    ASSERT_TRUE(data_fd2_ui_slide_composed_target_buf_ptr != 0);

    /* fd2_composite_battle_frame's pipeline stages are stubbed; phase 0
     * makes the real fd2_paint_cursor_overlay_pattern a no-op. */
    data_fd2_battle_anim_phase = 0;
    g_composite_call_count = 0;

    fd2_close_status_screen_with_slide_out();

    /* Step 4 recomposites exactly one frame after the teardown. */
    ASSERT_EQ(g_composite_call_count, 1);

    /* The function already free()d all three; drop the dangling globals so
     * later tests in the suite never reuse a freed pointer. */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
}


void run_ui_menu_status_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/status\n");
    RUN_TEST(test_stat_preview_basic);
    RUN_TEST(test_stat_preview_weapon_opposite_and_same_category);
    RUN_TEST(test_stat_preview_armor_branch_and_flag_gate);
    RUN_TEST(test_stat_preview_signed_negative_bonus);
    RUN_TEST(test_close_status_screen_slide_out_runs_full_teardown);
    printf("\n");
}
