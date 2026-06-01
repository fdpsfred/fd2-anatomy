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
