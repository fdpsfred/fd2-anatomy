/*
 * unit tests for src/life/main.c
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


void run_life_main_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: life/main\n");
    RUN_TEST(test_main_menu_new_game);
    RUN_TEST(test_main_menu_fallback);
    RUN_TEST(test_main_menu_continue_quit);
    printf("\n");
}
