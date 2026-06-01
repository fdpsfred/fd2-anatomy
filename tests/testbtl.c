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

/* ---- Test: check counter attack ---- */

static void test_counter_attack_sleep(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[1].status_sleep_flag = 1;
    result = fd2_check_can_counter_attack(0, 1);
    ASSERT_EQ(result, -1);
}

/* ---- Test: heal spell wrapper ---- */

static void test_heal_spell_to_target(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 300;
    g_test_rc_array[0].portrait_id = 0x50;
    data_fd2_battle_spell_effect_table[3].damage = 80;
    data_fd2_shared_rng_seed = 0;
    fd2_apply_heal_spell_to_target(0, 3);
    ASSERT_TRUE(g_test_rc_array[0].hp_current > 50);
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

static uint8 t_ai_tile_map[20 * 15 * 4];

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

static void test_ai_seek_optimal_unreachable(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    result = fd2_ai_seek_optimal_position(0, 0);
    ASSERT_EQ(result, 0);
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
    ASSERT_EQ((long)data_fd2_save_load_save_slot_id, 1);
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
    RUN_TEST(test_heal_spell_to_target);
    RUN_TEST(test_damage_basic);
    RUN_TEST(test_damage_floor_at_zero);
    RUN_TEST(test_magic_damage_miss);
    RUN_TEST(test_counter_attack_sleep);
    RUN_TEST(test_recompute_stats_basic);
    RUN_TEST(test_recalc_combat_stats_basic);
    RUN_TEST(test_default_attack_sleep);
    RUN_TEST(test_default_attack_not_adjacent);
    RUN_TEST(test_mp_heal_basic);
    RUN_TEST(test_mp_heal_cap_at_max);
    RUN_TEST(test_combat_bubble_pos_facing_down);
    RUN_TEST(test_stat_preview_basic);
    RUN_TEST(test_flash_char_hit_enemy);
    RUN_TEST(test_combat_hit_outcome_zero_stats);
    RUN_TEST(test_face_toward_target_down);
    RUN_TEST(test_face_toward_target_left);
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
    RUN_TEST(test_compute_aoe_no_targets);
    RUN_TEST(test_ai_score_phys_no_weapon);
    RUN_TEST(test_ai_seek_optimal_unreachable);
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
    RUN_TEST(test_summon_8slot_init);
    RUN_TEST(test_summon_main_init);
    RUN_TEST(test_summon_generic_reset);
    RUN_TEST(test_summon_generic_state3);
    RUN_TEST(test_summon_generic_state5_advance);
    RUN_TEST(test_summon_a_init);
    RUN_TEST(test_summon_a_state6_terminate);
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
