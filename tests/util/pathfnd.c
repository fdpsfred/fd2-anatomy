/*
 * unit tests for src/util/pathfnd.c
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


static void test_pathfind_count_unique_dirs(void)
{
    uint8 result;

    memset(data_fd2_battle_pathfind_step_stack, 0, 32);
    data_fd2_battle_pathfind_current_depth = 4;
    data_fd2_battle_pathfind_step_stack[0*8+3] = 0;
    data_fd2_battle_pathfind_step_stack[1*8+3] = 0;
    data_fd2_battle_pathfind_step_stack[2*8+3] = 1;
    data_fd2_battle_pathfind_step_stack[3*8+3] = 1;
    result = fd2_pathfind_count_unique_directions();
    ASSERT_EQ(result, 8);
}

/* --- flood-fill recursion tests (fd2_flood_fill_movement_range_recursive) ---
 * The neighbour step (fd2_flood_fill_neighbor_step) is a faithful test stub in
 * testglob.c that records each call and applies the real marker/carry logic, so
 * these tests assert both the resulting marker grid and the traversal. */
extern uint8 *g_ffns_origin;
extern int    g_ffns_calls;
extern int    g_ffns_off[64];
extern uint8  g_ffns_cost[64];

/* Tile map = MAPW*MAPH tiles of 4 bytes each ([attr_lo,attr_hi,flags,marker]),
 * wrapped in GUARD bytes on each side to catch any out-of-bounds write. */
#define MAPW 5
#define MAPH 5
#define GUARD 16
#define TILES (MAPW * MAPH)
static uint8 ff_buf[GUARD + TILES * 4 + GUARD];
static uint8 ff_tct[8];     /* tile_cost_table: index [(attr<<2)+1] -> cost_idx */
static uint8 ff_costtab[4]; /* cost table indexed by cost_idx -> movement cost  */

#define FF_MAP   (ff_buf + GUARD)
#define FF_MARK(col, row) FF_MAP[((row) * MAPW + (col)) * 4 + 3]
#define FF_FLAGS(col, row) FF_MAP[((row) * MAPW + (col)) * 4 + 2]

/* Reset map + cost tables: all tiles attr=0/flags=0/marker=0, every step costs
 * 1 (attr 0 -> tct[1]=0 -> costtab[0]=1). Wire the pathfind globals. */
static void ff_reset(void)
{
    memset(ff_buf, 0, sizeof(ff_buf));
    memset(ff_tct, 0, sizeof(ff_tct));
    memset(ff_costtab, 0, sizeof(ff_costtab));
    ff_tct[1] = 0;
    ff_costtab[0] = 1;
    data_fd2_battle_pathfind_tile_cost_table_ptr = (uint32)ff_tct;
    data_fd2_battle_pathfind_caller_context = (uint32)ff_costtab;
    data_fd2_battle_pathfind_map_width = MAPW;
    data_fd2_battle_pathfind_map_height = MAPH;
    g_ffns_calls = 0;
    g_ffns_origin = 0;
}

/* Assert the GUARD margins were never touched (no OOB btm write). */
static int ff_guards_intact(void)
{
    int i;
    for (i = 0; i < GUARD; i++) {
        if (ff_buf[i] != 0) return 0;
        if (ff_buf[GUARD + TILES * 4 + i] != 0) return 0;
    }
    return 1;
}

/* cost=3 from the centre of an open 5x5 -> diamond of residual cost
 * (3 - manhattan distance) for distance<=2, else 0. */
static void test_floodfill_open_diamond(void)
{
    static const uint8 expect[MAPH][MAPW] = {
        { 0, 0, 1, 0, 0 },
        { 0, 1, 2, 1, 0 },
        { 1, 2, 3, 2, 1 },
        { 0, 1, 2, 1, 0 },
        { 0, 0, 1, 0, 0 }
    };
    int col;
    int row;

    ff_reset();
    g_ffns_origin = &FF_MARK(2, 2);
    FF_MARK(2, 2) = 3;                 /* origin seeded by the orchestrator */
    fd2_flood_fill_movement_range_recursive(2, 2, 3, &FF_MARK(2, 2));

    for (row = 0; row < MAPH; row++) {
        for (col = 0; col < MAPW; col++) {
            ASSERT_EQ(FF_MARK(col, row), expect[row][col]);
        }
    }
    ASSERT_TRUE(ff_guards_intact());
}

/* Origin at corner (0,0): left/up must be skipped (x==0 / y==0) so no OOB
 * access, and cost=2 marks only the origin + its two in-bounds neighbours. */
static void test_floodfill_corner_clamp(void)
{
    static const uint8 expect[MAPH][MAPW] = {
        { 2, 1, 0, 0, 0 },
        { 1, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0 }
    };
    int col;
    int row;

    ff_reset();
    g_ffns_origin = &FF_MARK(0, 0);
    FF_MARK(0, 0) = 2;
    fd2_flood_fill_movement_range_recursive(0, 0, 2, &FF_MARK(0, 0));

    for (row = 0; row < MAPH; row++) {
        for (col = 0; col < MAPW; col++) {
            ASSERT_EQ(FF_MARK(col, row), expect[row][col]);
        }
    }
    ASSERT_TRUE(ff_guards_intact());
}

/* A 0x40 "impassable" tile is never marked and blocks expansion through it; a
 * 0x80 "sink" tile is marked (forced to 0) but does not expand further. */
static void test_floodfill_blocked_and_sink(void)
{
    int col;
    int row;

    ff_reset();
    FF_FLAGS(3, 2) = 0x40;             /* impassable, east of origin */
    FF_FLAGS(2, 1) = 0x80;             /* sink, north of origin */
    g_ffns_origin = &FF_MARK(2, 2);
    FF_MARK(2, 2) = 4;
    fd2_flood_fill_movement_range_recursive(2, 2, 4, &FF_MARK(2, 2));

    /* impassable tile stays unmarked even though the budget could reach it */
    ASSERT_EQ(FF_MARK(3, 2), 0);
    /* (4,2) is distance 2 from the origin and its only length-2 path runs
     * through the impassable (3,2); every detour is length 3, whose residual
     * (cost 4 - 3 = 1, then -1 = 0) never improves on 0. So with the wall in
     * place (4,2) stays unmarked. */
    ASSERT_EQ(FF_MARK(4, 2), 0);
    /* sink tile is marked but pinned to 0 (reachable, no expansion) */
    ASSERT_EQ(FF_MARK(2, 1), 0);
    /* tile north of the sink (2,0) must stay unmarked: the sink forces cost 0,
     * so it cannot expand into (2,0); and (2,0) is distance 2 from origin on
     * every other path too (>budget after each step) -> remains 0. */
    ASSERT_EQ(FF_MARK(2, 0), 0);
    /* west arm is unobstructed: (1,2)=3, (0,2)=2 with budget 4 */
    ASSERT_EQ(FF_MARK(1, 2), 3);
    ASSERT_EQ(FF_MARK(0, 2), 2);
    ASSERT_TRUE(ff_guards_intact());

    (void)col; (void)row;
}

/* Traversal contract: with a budget that cannot improve any neighbour
 * (cost=1, step cost=1 -> new_cost 0 <= existing 0), the routine calls the
 * neighbour step exactly once per in-bounds direction, in right/left/down/up
 * order, with offsets +4 / -4 / +stride / -stride and never recurses. */
static void test_floodfill_visit_order(void)
{
    int stride;

    ff_reset();
    stride = MAPW * 4;
    g_ffns_origin = &FF_MARK(2, 2);
    FF_MARK(2, 2) = 1;
    fd2_flood_fill_movement_range_recursive(2, 2, 1, &FF_MARK(2, 2));

    ASSERT_EQ(g_ffns_calls, 4);
    ASSERT_EQ(g_ffns_off[0], 4);
    ASSERT_EQ(g_ffns_off[1], -4);
    ASSERT_EQ(g_ffns_off[2], stride);
    ASSERT_EQ(g_ffns_off[3], -stride);
    ASSERT_EQ(g_ffns_cost[0], 1);
    ASSERT_EQ(g_ffns_cost[1], 1);
    ASSERT_EQ(g_ffns_cost[2], 1);
    ASSERT_EQ(g_ffns_cost[3], 1);
    /* nothing improved -> no marker beyond the seeded origin */
    ASSERT_EQ(FF_MARK(2, 1), 0);
    ASSERT_EQ(FF_MARK(3, 2), 0);
}


void run_util_pathfnd_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: util/pathfnd\n");
    RUN_TEST(test_pathfind_count_unique_dirs);
    RUN_TEST(test_floodfill_open_diamond);
    RUN_TEST(test_floodfill_corner_clamp);
    RUN_TEST(test_floodfill_blocked_and_sink);
    RUN_TEST(test_floodfill_visit_order);
    printf("\n");
}
