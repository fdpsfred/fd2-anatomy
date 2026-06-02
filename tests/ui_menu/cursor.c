/*
 * unit tests for src/ui_menu/cursor.c
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


/* ---- Tests: cursor ---- */

static void test_cursor_move_up_basic(void)
{
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_anim_phase = 0;
    fd2_cursor_move_up();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 4);
}


static void test_cursor_move_up_at_top(void)
{
    data_fd2_battle_cursor_world_y = 0;
    g_composite_call_count = 0;
    fd2_cursor_move_up();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 0);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* Scroll branch: screen_y<2 && origin_y!=0 -> world_y-- AND origin_y--,
 * screen_y unchanged, then composite (JMP 0x11B90). */
static void test_cursor_move_up_scroll(void)
{
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 1;
    data_fd2_battle_view_window_origin_y = 3;
    g_composite_call_count = 0;
    fd2_cursor_move_up();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 4);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 2);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 1);
    ASSERT_EQ(g_composite_call_count, 1);
}


static void test_cursor_move_down_basic(void)
{
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 3;
    data_fd2_battle_anim_phase = 0;
    fd2_cursor_move_down();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 6);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 4);
}


/* Scroll branch: world_y!=height-1 && screen_y>=6 && origin_y!=height-8 ->
 * world_y++ AND origin_y++, screen_y unchanged, then composite (JMP 0x11BEF).
 * height=15 (default) so height-8=7 != origin_y(3); screen_y(10)>5.
 * Mirrors test_cursor_move_up_scroll for the down direction. */
static void test_cursor_move_down_scroll(void)
{
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 10;
    data_fd2_battle_view_window_origin_y = 3;
    data_fd2_battle_anim_phase = 1;
    g_composite_call_count = 0;
    fd2_cursor_move_down();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 6);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 10);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* Bottom-edge no-op: world_y == map_height_tiles-1 -> JZ 0x11BEF, no INC at
 * all, world_y unchanged, only composite refresh. height=15 -> world_y=14. */
static void test_cursor_move_down_at_bottom(void)
{
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_battle_cursor_world_y = 14;
    g_composite_call_count = 0;
    fd2_cursor_move_down();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 14);
    ASSERT_EQ(g_composite_call_count, 1);
}


static void test_cursor_move_right_basic(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_anim_phase = 0;
    fd2_cursor_move_right();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 6);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 6);
}


/* Scroll branch: world_x!=width-1 && screen_x>0xa && origin_x!=width-0xd ->
 * world_x++ AND origin_x++, screen_x unchanged, then composite (JMP 0x11C37).
 * width=20 so width-0xd=7 != origin_x(3); screen_x(11)>0xa.
 * Mirrors test_cursor_move_down_scroll for the right (X) direction; note the
 * X viewport width is 0xd (13) vs Y's 8, so down's tests do not cover this. */
static void test_cursor_move_right_scroll(void)
{
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 11;
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_anim_phase = 1;
    g_composite_call_count = 0;
    fd2_cursor_move_right();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 6);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 11);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* Right-edge no-op: world_x == map_width_tiles-1 -> JZ 0x11C10, no INC at all,
 * world_x unchanged, only composite refresh. width=20 -> world_x=19. */
static void test_cursor_move_right_at_right_edge(void)
{
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_cursor_world_x = 19;
    g_composite_call_count = 0;
    fd2_cursor_move_right();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 19);
    ASSERT_EQ(g_composite_call_count, 1);
}


static void test_cursor_move_left_basic(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_anim_phase = 0;
    fd2_cursor_move_left();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 4);
}


/* Scroll branch: world_x!=0 && screen_x<2 && origin_x!=0 -> world_x-- AND
 * origin_x--, screen_x unchanged, then composite (JMP 0x11CA1).
 * X-axis mirror of test_cursor_move_up_scroll. */
static void test_cursor_move_left_scroll(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 1;
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_anim_phase = 1;
    g_composite_call_count = 0;
    fd2_cursor_move_left();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 4);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 2);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 1);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* Left-edge no-op: world_x == 0 -> JZ 0x11CA1, no DEC at all, world_x
 * unchanged, only composite refresh. Mirrors the up/right edge tests. */
static void test_cursor_move_left_at_left_edge(void)
{
    data_fd2_battle_cursor_world_x = 0;
    g_composite_call_count = 0;
    fd2_cursor_move_left();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 0);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* Inner-step with animation: world_x!=0 && screen_x>=2 -> world_x-- AND
 * screen_x--; anim_phase!=0 so the early-return (JZ 0x11CAB) is NOT taken and
 * composite still runs. Complements test_cursor_move_left_basic, which sets
 * anim_phase==0 to take the early-return (no composite). */
static void test_cursor_move_left_inner_step_anim(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_anim_phase = 1;
    g_composite_call_count = 0;
    fd2_cursor_move_left();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 4);
    ASSERT_EQ(g_composite_call_count, 1);
}


/* ---- Tests: pan ---- */

static void test_pan_to_char(void)
{
    g_test_rc_array[2].pos_x = 10;
    g_test_rc_array[2].pos_y = 8;
    data_fd2_battle_cursor_world_x = 10;
    data_fd2_battle_cursor_world_y = 8;
    data_fd2_battle_anim_phase = 1;
    fd2_pan_cursor_to_char(2);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 10);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 8);
}


/* ---- Tests: pan_cursor_to_tile_animated ---- */

static void test_pan_to_tile_same_pos(void)
{
    data_fd2_battle_cursor_world_x = 7;
    data_fd2_battle_cursor_world_y = 4;
    data_fd2_battle_anim_phase = 1;
    fd2_pan_cursor_to_tile_animated(7, 4);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 7);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 4);
}


static void test_pan_to_tile_moves_x(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_to_tile_animated(8, 5);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 8);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 5);
}


/* ---- Tests: pan_cursor_and_window ---- */

static void test_pan_and_window_same_pos(void)
{
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_view_window_origin_y = 2;
    fd2_pan_cursor_and_window(3, 2);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 2);
}


/* X-loop increment path: target_ox(3) > origin_x(0) -> JGE 0x13614 taken each
 * step, INC cursor_world_x AND INC origin_x in lockstep (asm 0x13614/0x1361a),
 * 3 steps until origin_x==target_ox. Y-loop skipped (origin_y==target_oy==4 ->
 * JZ 0x13181). Per-step composite pinned: g_composite_call_count==3. Deltas
 * hand-derived from the +1-per-step DEC/INC asm (emulate blocked by __CHK LOCK
 * pcodeop). cursor_world_x advances +1 each step: 5 -> 8. */
static void test_pan_and_window_inc_x_lockstep(void)
{
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 4;
    data_fd2_battle_cursor_world_x = 5;
    g_composite_call_count = 0;
    fd2_pan_cursor_and_window(3, 4);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 8);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 4);
    ASSERT_EQ(g_composite_call_count, 3);
}


/* Y-loop decrement path: target_oy(2) < origin_y(5) -> JGE 0x1364d NOT taken,
 * DEC cursor_world_y AND DEC origin_y in lockstep (asm 0x1363f/0x13645), 3
 * steps until origin_y==target_oy. X-loop skipped (origin_x==target_ox==3 ->
 * JZ 0x13631). Per-step composite pinned: g_composite_call_count==3. cursor_
 * world_y retreats -1 each step: 9 -> 6. Complements the inc-X test to cover
 * the opposite signed branch and the Y axis. */
static void test_pan_and_window_dec_y_lockstep(void)
{
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_view_window_origin_y = 5;
    data_fd2_battle_cursor_world_y = 9;
    g_composite_call_count = 0;
    fd2_pan_cursor_and_window(3, 2);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 2);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 6);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ(g_composite_call_count, 3);
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


void run_ui_menu_cursor_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/cursor\n");
    RUN_TEST(test_cursor_move_up_basic);
    RUN_TEST(test_cursor_move_up_at_top);
    RUN_TEST(test_cursor_move_up_scroll);
    RUN_TEST(test_cursor_move_down_basic);
    RUN_TEST(test_cursor_move_down_scroll);
    RUN_TEST(test_cursor_move_down_at_bottom);
    RUN_TEST(test_cursor_move_right_basic);
    RUN_TEST(test_cursor_move_right_scroll);
    RUN_TEST(test_cursor_move_right_at_right_edge);
    RUN_TEST(test_cursor_move_left_basic);
    RUN_TEST(test_cursor_move_left_scroll);
    RUN_TEST(test_cursor_move_left_at_left_edge);
    RUN_TEST(test_cursor_move_left_inner_step_anim);
    RUN_TEST(test_pan_to_char);
    RUN_TEST(test_pan_to_tile_same_pos);
    RUN_TEST(test_pan_to_tile_moves_x);
    RUN_TEST(test_pan_and_window_same_pos);
    RUN_TEST(test_pan_and_window_inc_x_lockstep);
    RUN_TEST(test_pan_and_window_dec_y_lockstep);
    printf("\n");
}
