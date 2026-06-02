/*
 * unit tests for src/battle/btl_aitg.c
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

#include "battlfix.h"


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


/* Genuine unreachable: the seek's "find optimal cell" pathfind (md==2)
 * returns 0xFF -> the function must return 0 and leave anim_phase untouched
 * (the no-anim early-out at btl_ai.c). reset_ai_stubs() sets g_pathfind_return
 * to a deterministic value, so here we force 0xFF explicitly. */
static void test_ai_seek_optimal_unreachable(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_pathfind_return = 0xFF;
    data_fd2_battle_anim_phase = 7;     /* sentinel: must stay untouched */
    result = fd2_ai_seek_optimal_position(0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 7);
}


/* Already at optimum: pathfind reports a destination equal to the source
 * (dst==src) -> no walk, no animation, return 0, anim_phase untouched. */
static void test_ai_seek_optimal_already_at_best(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_pathfind_return = 3;              /* reachable (not 0xFF) */
    g_pathfind_write_dst = 1;
    g_pathfind_dst_x = 5;               /* dst == src */
    g_pathfind_dst_y = 5;
    data_fd2_battle_anim_phase = 7;     /* sentinel: must stay untouched */
    result = fd2_ai_seek_optimal_position(0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 7);
}


/* EAX-tracking lock-in: did_move must derive from the WALK return value
 * (asm 0x1421d TEST EAX,EAX on the fd2_ai_walk_to_target_tile CALL at 0x14215),
 * NOT from the pathfind result that the buggy decompiled C reinterprets (iVar1).
 * Setup: the seek's md==2 pathfind returns NONZERO (3) and reports dst!=src, so
 * the buggy "did_move = pathfind_result != 0" would yield 1; the walk routine's
 * own pathfinds (md==0/1) return 0 so the real walk returns 0. Correct semantics
 * => did_move = (walk_result==0) => result 0. Walk returning 0 also means no
 * animation pipeline runs, keeping this case fully deterministic. anim_phase is
 * driven 0 then 1 across the walk branch, so it ends at 1. */
static void test_ai_seek_optimal_walk_branch_returns_zero(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_pathfind_return = 3;              /* seek pathfind: reachable, nonzero */
    g_pathfind_write_dst = 1;
    g_pathfind_dst_x = 8;              /* dst != src -> enters walk branch */
    g_pathfind_dst_y = 8;
    g_pathfind_walk_return = 0;         /* walk routes -> walk returns 0 */
    data_fd2_battle_anim_phase = 7;
    result = fd2_ai_seek_optimal_position(0, 0);
    ASSERT_EQ(result, 0);              /* fails on buggy iVar1-based did_move */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
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


/* ---- High-risk coverage for fd2_ai_walk_to_target_tile @ 0x14B78 ----
 *
 * The two tests below drive the routine's two unasserted high-risk regions:
 * the candidate-selection loop with its abs()-chain + taxi/diag tiebreak
 * (asm 0x14dc5-0x14e56) and the Stage B furthest-passable step-decode scan
 * (asm 0x14c4d-0x14d47). Both keep the FINAL md==0 route returning 0 so the
 * REAL fd2_walk_path_animation_loop never runs -> fully deterministic, return 0.
 *
 * On the test path fd2_collect_unmarked_tile_positions and
 * fd2_mark_char_occupant_tiles_for_team are the REAL btl_ai.c functions, and
 * fd2_check_char_status_immunity / fd2_get_movement_cost_table_for_job are REAL.
 * fd2_pathfind_to_destination, fd2_init_movement_range_floodfill,
 * fd2_obfuscate_battle_tile_map and fd2_paint_threat_overlay_for_team are stubs.
 * party_member_count is pinned to 1 so the occupant-mark pass (which excludes the
 * mover, char 0) marks nothing and cannot pollute the candidate tiles. The
 * non-0xFF cells of t_ai_tile_map (byte (y*width + x)*4 + 7) are exactly the
 * candidate / walkable tiles the REAL collection + scan loops read. char 0 is
 * memset-zero: portrait/job/archetype 0 -> non-immune; char_id(+8) 0 != 0x1c, so
 * the movement class stays job_id 0 (no override). The chosen "best adjacent
 * tile" is observed via g_pathfind_md0_dst_x/y (the stub records the LAST md==0
 * destination, which is the routine's final route target). Expected winners are
 * enumerated directly from the verbatim taxi/diag formula (identical in disasm
 * and decompiler): taxi=|dx|+|dy|, diag=||dx|-|dy||, update when taxi<best_taxi
 * OR (taxi==best_taxi AND diag<best_diag), best_taxi/best_diag seeded 0xFF. */

/* Candidate-selection loop + abs-chain + taxi/diag tiebreak. Stage A md==0
 * returns 0 (reset default) != 0xFF, so Stage B is skipped and target stays the
 * passed (4,0). Two cells are unmarked -> 2 candidates (row-major order):
 *   cand0 (0,0): dx=0-4=-4, dy=0-0=0 -> taxi 4, diag |4-0|=4
 *   cand1 (2,2): dx=2-4=-2, dy=2-0= 2 -> taxi 4, diag |2-2|=0
 * cand0 seeds best (taxi 4 < 0xFF). cand1 ties on taxi (4==4) and wins the diag
 * tiebreak (0 < 4) -> best becomes (2,2). This locks the negative-delta abs
 * handling, the |dx|-|dy| diag term, and the tiebreak firing on a LATER
 * candidate. The final md==0 route then targets (2,2). */
static void test_ai_walk_candidate_taxi_tiebreak(void)
{
    int result;
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_party_member_count = 1;     /* only the (excluded) mover */
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    g_test_rc_array[0].pos_x = 0;
    g_test_rc_array[0].pos_y = 0;
    /* unmark the two candidate cells: (0,0) and (2,2), width 5 */
    t_ai_tile_map[(0 * 5 + 0) * 4 + 7] = 0;
    t_ai_tile_map[(2 * 5 + 2) * 4 + 7] = 0;
    g_pathfind_walk_return = 0;                 /* Stage A !=0xFF, final route 0 */
    result = fd2_ai_walk_to_target_tile(4, 0, 0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)g_pathfind_md0_dst_x, 2);   /* tiebreak winner (2,2) */
    ASSERT_EQ((long)g_pathfind_md0_dst_y, 2);
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
}


/* Stage B furthest-passable step-decode scan. Sequenced pathfind:
 *   call 0 (Stage A, md==0) -> 0xFF  -> forces Stage B
 *   call 1 (Stage B, md==1) -> 5     -> 5 step bytes written into the path buf
 *   call 2 (final,   md==0) -> 0     -> no walk animation, deterministic
 * src = char 0 at (0,0). Step bytes [3,0,3,2,1] exercise all four decode
 * branches (0:S y++, 1:W x--, 2:N y--, 3:E x++) and trace:
 *   step0 E -> (1,0)   step1 S -> (1,1)   step2 E -> (2,1)
 *   step3 N -> (2,0)   step4 W -> (1,0)
 * Walkable cells (+7 != 0xFF) are (1,1) and (2,0). The scan keeps the FURTHEST
 * (last-visited) walkable tile: (1,1) at step1 then overridden by (2,0) at step3;
 * step4 (1,0) is 0xFF so best stays (2,0). Stage B therefore sets target=(2,0).
 * The candidate loop then collects the same two unmarked cells (row-major: (2,0)
 * at y=0, (1,1) at y=1); (2,0) has taxi 0 to the Stage B target (2,0) and wins,
 * so the final route targets (2,0). Asserting the final md==0 dst == (2,0) proves
 * Stage B was entered, all four step decodes ran, the furthest-walkable override
 * works, and the result propagated into the final route. */
static void test_ai_walk_stage_b_furthest_tile(void)
{
    int result;
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_party_member_count = 1;     /* only the (excluded) mover */
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    g_test_rc_array[0].pos_x = 0;
    g_test_rc_array[0].pos_y = 0;
    /* walkable cells visited by the scan: (1,1) then (2,0) */
    t_ai_tile_map[(1 * 5 + 1) * 4 + 7] = 0;
    t_ai_tile_map[(0 * 5 + 2) * 4 + 7] = 0;
    g_pathfind_seq_enable = 1;
    g_pathfind_seq[0] = 0xFF;                   /* Stage A unreachable */
    g_pathfind_seq[1] = 5;                      /* Stage B: 5 steps */
    g_pathfind_seq[2] = 0;                      /* final route: no walk */
    g_pathfind_seq_steps = 5;
    g_pathfind_step_bytes[0] = 3;               /* E -> (1,0) */
    g_pathfind_step_bytes[1] = 0;               /* S -> (1,1) walkable */
    g_pathfind_step_bytes[2] = 3;               /* E -> (2,1) */
    g_pathfind_step_bytes[3] = 2;               /* N -> (2,0) walkable */
    g_pathfind_step_bytes[4] = 1;               /* W -> (1,0) */
    result = fd2_ai_walk_to_target_tile(9, 9, 0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)g_pathfind_md0_dst_x, 2);   /* furthest walkable (2,0) */
    ASSERT_EQ((long)g_pathfind_md0_dst_y, 0);
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
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


/* ---- Direct coverage for the two high-risk marking modes of
 *      fd2_compute_aoe_targets @ 0x14818 ----
 *
 * fd2_init_movement_range_floodfill is a no-op stub (testglob.c), so in Mode 1
 * the floodfill leaves the tile map exactly as the test left it; this lets each
 * test pin a known +7-overlay state before the radius/cross marking runs.
 * fd2_get_movement_cost_table_for_job is the REAL table.c accessor (its returned
 * pointer is only consumed by the stubbed floodfill, so the value is irrelevant).
 * All expected values are hand-derived from the disassembly trace (the +7 byte of
 * each cell at (y*width + x)*4 + 7 is the marker the collection loop reads). */

/* Mode 2 (spell_range >= 0x10): orthogonal cross. asm 0x148c7-0x1493e.
 * extent = spell_range - 0x10. The X-stripe clears +7 along row center_y for
 * |col-center_x| <= extent; the Y-stripe clears +7 along col center_x for
 * |row-center_y| <= extent. Map starts all-0xFF, so the cleared (0) plus-shape
 * tiles are the only includable ones. Call (2,2, buf, 0x11, 0, 0): extent 1 ->
 * plus = {(1,2),(2,2),(3,2),(2,1),(2,3)}. team_filter 0 collects team==0 chars
 * whose tile is cleared. char0 (3,2) lies on the X arm, char1 (2,3) on the Y arm
 * -> both collected (covers both stripes + the sequential out_buf writes at
 * count 0 and 1); char2 (0,0) is outside the plus -> excluded. */
static void test_compute_aoe_mode2_cross(void)
{
    int result;
    uint8 buf[8];
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();                  /* tile map -> 0xFF, ptr wired */
    memset(buf, 0xAA, sizeof(buf));
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    data_fd2_battle_party_member_count = 3;
    g_test_rc_array[0].team = 0; g_test_rc_array[0].pos_x = 3; g_test_rc_array[0].pos_y = 2;
    g_test_rc_array[1].team = 0; g_test_rc_array[1].pos_x = 2; g_test_rc_array[1].pos_y = 3;
    g_test_rc_array[2].team = 0; g_test_rc_array[2].pos_x = 0; g_test_rc_array[2].pos_y = 0;
    result = fd2_compute_aoe_targets(2, 2, (uint32)buf, 0x11, 0, 0);
    ASSERT_EQ(result, 2);
    ASSERT_EQ((long)buf[0], 0);
    ASSERT_EQ((long)buf[1], 1);
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_party_member_count = save_pmc;
}


/* Mode 1 (spell_range < 0x10) with aoe_radius != 0: manhattan bubble.
 * asm 0x1486a-0x148c5. After the (stubbed, no-op) floodfill the map is whatever
 * the test set it to; here it is memset to 0x00 so every tile is includable
 * (+7 == 0). The radius block then re-marks +7 = 0xFF for every cell with
 * manhattan((col,row),(center)) < aoe_radius. Call (2,2, buf, 2, 2, 0): radius 2
 * -> bubble (manhattan 0 or 1) = {(2,2),(1,2),(3,2),(2,1),(2,3)}. char0 team0 at
 * (2,2) is inside the bubble -> +7 == 0xFF -> excluded; char1 team0 at (0,0) has
 * manhattan 4 (not < 2) -> +7 stays 0 -> collected, out_buf[0] = 1. Pins the
 * abs()+manhattan bubble AND the *(out_buf+count)=ci write for this path. */
static void test_compute_aoe_mode1_radius_bubble(void)
{
    int result;
    uint8 buf[8];
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    memset(t_ai_tile_map, 0, sizeof(t_ai_tile_map));   /* includable baseline */
    memset(buf, 0xAA, sizeof(buf));
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    data_fd2_battle_party_member_count = 2;
    g_test_rc_array[0].team = 0; g_test_rc_array[0].pos_x = 2; g_test_rc_array[0].pos_y = 2;
    g_test_rc_array[1].team = 0; g_test_rc_array[1].pos_x = 0; g_test_rc_array[1].pos_y = 0;
    result = fd2_compute_aoe_targets(2, 2, (uint32)buf, 2, 2, 0);
    ASSERT_EQ(result, 1);
    ASSERT_EQ((long)buf[0], 1);
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_party_member_count = save_pmc;
}


/* team_filter branches 1/2/3 of the collection loop (asm 0x149a1/0x149b0/0x149c0).
 * Isolate the team filter from the marking: Mode 1, aoe_radius 0 (no bubble), map
 * memset to 0x00 so the (no-op) floodfill leaves every tile includable -> every
 * alive char qualifies on the tile test and only the team predicate decides.
 *   char0 team0, char1 team1, char2 team2 (all alive, distinct tiles).
 *   team_filter 1 (any ally, team != 0) -> {char1,char2}: result 2, buf 1 then 2.
 *   team_filter 2 (NPC ally, team == 1) -> {char1}:       result 1, buf[0] 1.
 *   team_filter 3 (player,   team == 2) -> {char2}:       result 1, buf[0] 2. */
static void test_compute_aoe_team_filter_branches(void)
{
    int result;
    uint8 buf[8];
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    reset_ai_stubs();
    memset(t_ai_tile_map, 0, sizeof(t_ai_tile_map));   /* all tiles includable */
    save_pmc = data_fd2_battle_party_member_count;
    save_w = data_fd2_battle_map_width_tiles;
    save_h = data_fd2_battle_map_height_tiles;
    data_fd2_battle_map_width_tiles = 5;
    data_fd2_battle_map_height_tiles = 5;
    data_fd2_battle_party_member_count = 3;
    g_test_rc_array[0].team = 0; g_test_rc_array[0].pos_x = 0; g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[1].team = 1; g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 0;
    g_test_rc_array[2].team = 2; g_test_rc_array[2].pos_x = 2; g_test_rc_array[2].pos_y = 0;

    memset(buf, 0xAA, sizeof(buf));
    result = fd2_compute_aoe_targets(0, 0, (uint32)buf, 2, 0, 1);
    ASSERT_EQ(result, 2);
    ASSERT_EQ((long)buf[0], 1);
    ASSERT_EQ((long)buf[1], 2);

    memset(buf, 0xAA, sizeof(buf));
    result = fd2_compute_aoe_targets(0, 0, (uint32)buf, 2, 0, 2);
    ASSERT_EQ(result, 1);
    ASSERT_EQ((long)buf[0], 1);

    memset(buf, 0xAA, sizeof(buf));
    result = fd2_compute_aoe_targets(0, 0, (uint32)buf, 2, 0, 3);
    ASSERT_EQ(result, 1);
    ASSERT_EQ((long)buf[0], 2);

    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_party_member_count = save_pmc;
}


static void test_mark_aoe_plus_pattern(void)
{
    uint8 t_tmap[48];
    uint32 save_tm;

    save_tm = data_fd2_battle_tile_map_ptr;
    memset(t_tmap, 0, sizeof(t_tmap));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;

    fd2_mark_aoe_plus_pattern_at(1, 1);

    ASSERT_EQ(t_tmap[(1*3+1)*4+6] & 0x40, 0x40);
    ASSERT_EQ(t_tmap[(1*3+0)*4+6] & 0x80, 0x80);
    ASSERT_EQ(t_tmap[(0*3+1)*4+6] & 0x80, 0x80);
    ASSERT_EQ(t_tmap[(1*3+2)*4+6] & 0x80, 0x80);
    ASSERT_EQ(t_tmap[(2*3+1)*4+6] & 0x80, 0x80);

    data_fd2_battle_tile_map_ptr = save_tm;
}


static void test_scan_chars_along_line(void)
{
    uint8 out[8];
    uint32 save_cx;
    uint32 save_cy;
    int result;

    save_cx = data_fd2_battle_cursor_world_x;
    save_cy = data_fd2_battle_cursor_world_y;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 3; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].pos_x = 4; g_test_rc_array[1].pos_y = 5;
    g_test_rc_array[1].team = 2;
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_map_width_tiles = 10;
    data_fd2_battle_map_height_tiles = 10;

    memset(out, 0xFF, sizeof(out));
    result = fd2_scan_chars_along_line_with_team_filter(
        5, 5, (uint32)out, 2, 5, 3, 0);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(out[0], 1);

    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_world_y = save_cy;
}


static void test_scan_chars_manhattan_enemy(void)
{
    uint8 idx_out[8];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].pos_x = 6; g_test_rc_array[1].pos_y = 5;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[2].pos_x = 5; g_test_rc_array[2].pos_y = 4;
    g_test_rc_array[2].team = 0;
    g_test_rc_array[2].flags = 1;
    data_fd2_battle_party_member_count = 3;

    memset(idx_out, 0xFF, sizeof(idx_out));
    result = fd2_scan_chars_within_manhattan_range(5, 5, 3,
        (uint32)idx_out, 0);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(idx_out[0], 0);
}


static void test_mark_occupant_tiles_team0(void)
{
    uint8 t_tmap[48];
    uint32 save_tm;

    save_tm = data_fd2_battle_tile_map_ptr;
    memset(t_tmap, 0, sizeof(t_tmap));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;
    data_fd2_battle_party_member_count = 2;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].pos_x = 2;
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].team = 2;

    fd2_mark_char_occupant_tiles_for_team(0xFFFFFFuL, 0);

    ASSERT_EQ(t_tmap[(0*3+1)*4+7], 0xFF);
    ASSERT_EQ(t_tmap[(1*3+2)*4+7], 0);

    data_fd2_battle_tile_map_ptr = save_tm;
}


static void test_collect_unmarked_tiles(void)
{
    uint8 t_tmap[32];
    uint8 out[16];
    uint32 save_tm;
    int result;

    save_tm = data_fd2_battle_tile_map_ptr;
    memset(t_tmap, 0, sizeof(t_tmap));
    memset(out, 0, sizeof(out));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_battle_map_width_tiles = 2;
    data_fd2_battle_map_height_tiles = 2;
    t_tmap[7] = 0;
    t_tmap[11] = 0xFF;
    t_tmap[15] = 0;
    t_tmap[19] = 0xFF;

    result = fd2_collect_unmarked_tile_positions((uint32)out);
    ASSERT_EQ(result, 2);
    ASSERT_EQ(out[0], 0);
    ASSERT_EQ(out[1], 0);
    ASSERT_EQ(out[2], 0);
    ASSERT_EQ(out[3], 1);

    data_fd2_battle_tile_map_ptr = save_tm;
}


static void test_find_tile_attr_match_miss(void)
{
    uint8 t_tmap[24];
    uint8 t_attr[32];
    uint8 out[2];
    uint32 save_tm;
    uint32 save_af;
    int result;

    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;

    memset(t_tmap, 0, sizeof(t_tmap));
    memset(t_attr, 0, sizeof(t_attr));
    memset(out, 0xFF, sizeof(out));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr;
    data_fd2_battle_map_width_tiles = 2;
    data_fd2_battle_map_height_tiles = 2;

    result = fd2_find_tile_with_attribute_match(99, (uint32)out);
    ASSERT_EQ(result, -1);

    data_fd2_battle_tile_map_ptr = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
}


static void test_tally_chars_zero_field(void)
{
    uint8 idx_arr[3];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].status_sleep_flag = 0;
    g_test_rc_array[1].status_sleep_flag = 1;
    g_test_rc_array[2].status_sleep_flag = 0;
    idx_arr[0] = 0;
    idx_arr[1] = 1;
    idx_arr[2] = 2;
    data_fd2_battle_party_member_count = 4;

    result = fd2_tally_chars_with_zero_at_field(3, (uint32)idx_arr,
        0x26, 10);
    ASSERT_EQ(result, 20);
}


void run_battle_btl_aitg_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: battle/btl_aitg\n");
    RUN_TEST(test_ai_pass_turn_hp_full);
    RUN_TEST(test_ai_pass_turn_poisoned);
    RUN_TEST(test_ai_pass_turn_heals_20pct);
    RUN_TEST(test_ai_pass_turn_clamps_at_max);
    RUN_TEST(test_ai_walk_no_path);
    RUN_TEST(test_ai_walk_candidate_taxi_tiebreak);
    RUN_TEST(test_ai_walk_stage_b_furthest_tile);
    RUN_TEST(test_compute_aoe_no_targets);
    RUN_TEST(test_compute_aoe_mode2_cross);
    RUN_TEST(test_compute_aoe_mode1_radius_bubble);
    RUN_TEST(test_compute_aoe_team_filter_branches);
    RUN_TEST(test_ai_seek_optimal_unreachable);
    RUN_TEST(test_ai_seek_optimal_already_at_best);
    RUN_TEST(test_ai_seek_optimal_walk_branch_returns_zero);
    RUN_TEST(test_ai_advance_no_target);
    RUN_TEST(test_ai_advance_target_found);
    RUN_TEST(test_tally_chars_zero_field);
    RUN_TEST(test_find_tile_attr_match_miss);
    RUN_TEST(test_collect_unmarked_tiles);
    RUN_TEST(test_mark_occupant_tiles_team0);
    RUN_TEST(test_scan_chars_manhattan_enemy);
    RUN_TEST(test_mark_aoe_plus_pattern);
    RUN_TEST(test_scan_chars_along_line);
    printf("\n");
}
