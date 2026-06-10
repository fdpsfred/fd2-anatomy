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

/* --- flood-fill tests (fd2_flood_fill_movement_range_recursive @ 0x4E0DC and
 * its inner step fd2_flood_fill_neighbor_step @ 0x4E16E, both real-emitted in
 * src/util/pathfnd.c). The recursion tests seed a small real tile map + cost
 * tables and assert the resulting marker grid (which exercises the neighbour
 * step's cost lookup, affordability/improvement gates and 0x40/0x80 flags end
 * to end); dedicated neighbour-step tests below cover each branch directly. */

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

/* A budget too small to improve any neighbour: cost=1 with a step cost of 1
 * yields new_cost 0, which never improves on the existing 0 marker, so no
 * neighbour is marked and the fill cannot recurse -- the whole grid stays 0
 * except the seeded origin. */
static void test_floodfill_no_improvement_no_spread(void)
{
    int col;
    int row;

    ff_reset();
    FF_MARK(2, 2) = 1;
    fd2_flood_fill_movement_range_recursive(2, 2, 1, &FF_MARK(2, 2));

    for (row = 0; row < MAPH; row++) {
        for (col = 0; col < MAPW; col++) {
            ASSERT_EQ(FF_MARK(col, row), (col == 2 && row == 2) ? 1 : 0);
        }
    }
    ASSERT_TRUE(ff_guards_intact());
}

/* --- direct neighbour-step tests (fd2_flood_fill_neighbor_step @ 0x4E16E) ---
 * Drive the real helper against a single tile in the shared ff map. The marker
 * byte sits at offset +3 of a 4-byte tile; the helper reads the attribute word
 * at [-3..-2] and the flags byte at [-1] relative to that marker pointer, i.e.
 * the tile's own [attr_lo, attr_hi, flags] -> marker layout. */

/* Wire the cost tables so that attribute `attr` -> tile cost `cost`, place that
 * attribute + `flags` on tile (2,2), seed its marker, then run the step and
 * return what it returned; *out receives the residual handed back. */
static int nstep_run(uint16 attr, uint8 flags, uint8 marker, uint8 remaining,
    uint8 *out)
{
    uint8 *tile;

    ff_reset();
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    tile[0] = (uint8)(attr & 0xFF);
    tile[1] = (uint8)(attr >> 8);
    tile[2] = flags;
    tile[3] = marker;
    *out = 0;
    return fd2_flood_fill_neighbor_step(remaining, &tile[3], out);
}

/* Affordable + strictly improves + passable: marker rewritten to residual,
 * returns 1, and the out-param mirrors the new residual. attr 0 -> tct[1]=0 ->
 * costtab[0]=1, so cost 1; remaining 5 -> new 4 > existing 0. */
static void test_nstep_improves_writes_marker(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ret = nstep_run(0, 0x00, 0, 5, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 4);
    ASSERT_EQ(out, 4);
    ASSERT_TRUE(ff_guards_intact());
}

/* tile_cost > remaining (unsigned borrow / JC): no write, returns 0. Here
 * remaining 0, cost 1 -> 0 - 1 underflows, must be rejected. */
static void test_nstep_unaffordable_rejected(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ret = nstep_run(0, 0x00, 7, 0, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(tile[3], 7);          /* marker untouched */
    ASSERT_TRUE(ff_guards_intact());
}

/* new_cost == existing must NOT improve (JLE is <=, signed): remaining 5, cost
 * 1 -> new 4; existing already 4 -> rejected, no write, returns 0. */
static void test_nstep_equal_not_improving(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ret = nstep_run(0, 0x00, 4, 5, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(tile[3], 4);
    ASSERT_TRUE(ff_guards_intact());
}

/* Improvement test is SIGNED: an existing marker with bit7 set (e.g. 0x80) is
 * negative as int8, so any non-negative new_cost beats it. new 1 (0x01) >
 * existing -128 -> writes, returns 1. */
static void test_nstep_signed_improvement(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ret = nstep_run(0, 0x00, 0x80, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 1);
    ASSERT_EQ(out, 1);
    ASSERT_TRUE(ff_guards_intact());
}

/* flags & 0x40 (impassable): even when affordable and improving, never write,
 * returns 0. */
static void test_nstep_impassable_flag(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ret = nstep_run(0, 0x40, 0, 5, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(tile[3], 0);          /* marker untouched */
    ASSERT_TRUE(ff_guards_intact());
}

/* flags & 0x80 (movement sink): reachable, but the residual written is forced
 * to 0 (and handed back as 0) so the recursion stops; still returns 1. */
static void test_nstep_sink_flag_forces_zero(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ret = nstep_run(0, 0x80, 0, 5, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 0);          /* forced to 0, not 4 */
    ASSERT_EQ(out, 0);
    ASSERT_TRUE(ff_guards_intact());
}

/* Two-level lookup: the low 10 bits of the attribute word index the primary
 * table at [(attr<<2)+1] to get a secondary index, which indexes the cost
 * table. Use attr=1 -> tct[(1<<2)+1] = tct[5]; set tct[5]=2 -> costtab[2]=3,
 * so tile cost 3; remaining 10 -> new 7. Bits above bit9 must be masked off, so
 * attr (0x400 | 1) resolves identically to attr 1. */
static void test_nstep_cost_table_indexing(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ff_reset();
    ff_tct[5] = 2;
    ff_costtab[2] = 3;
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    tile[0] = (uint8)((0x0400 | 1) & 0xFF);   /* attr_lo */
    tile[1] = (uint8)((0x0400 | 1) >> 8);     /* attr_hi: bit10 set, masked out */
    tile[2] = 0x00;
    tile[3] = 0;
    out = 0;
    ret = fd2_flood_fill_neighbor_step(10, &tile[3], &out);
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 7);          /* 10 - 3 */
    ASSERT_EQ(out, 7);
    ASSERT_TRUE(ff_guards_intact());
}


/* --- direction-tracked pathfind recursion tests
 * (fd2_pathfind_recursive_with_direction @ 0x4E27C). Until its inner step
 * fd2_pathfind_neighbor_step_with_tiebreak @ 0x4E330 is emitted for real, the
 * recursion is driven against that helper's faithful stub in tests/testglob.c
 * (coordinated landing per open_issues #33). The stub reproduces the binary's
 * mode-0 marker logic (so the marker grid below matches the flood fill's,
 * proving identical traversal order / bounds / +-4 / +-stride / termination)
 * AND records per call the btm offset, residual cost, neighbour x/y and the
 * direction byte found in the active step-stack frame (so this function's
 * load-bearing step-stack bookkeeping -- the part that is NOT just recursion
 * plumbing -- is asserted directly). */

extern uint8 *g_ptbs_origin;
extern int   g_ptbs_calls;
extern int   g_ptbs_off[64];
extern uint8 g_ptbs_cost[64];
extern uint8 g_ptbs_x[64];
extern uint8 g_ptbs_y[64];
extern uint8 g_ptbs_dir[64];
extern int   g_ptbs_depth_max;

/* Reset the shared ff map + cost tables (open, every step costs 1), zero the
 * recursion stack + depth marker the orchestrator would have cleared, point the
 * stub's offset base at the origin tile's marker, and clear the call recorder.
 * `pf_origin_col/row` is the seed tile. */
static void pf_reset(int origin_col, int origin_row)
{
    ff_reset();
    memset(data_fd2_battle_pathfind_step_stack, 0, 64 * 8);
    data_fd2_battle_pathfind_current_depth = 0;
    g_ptbs_origin = &FF_MARK(origin_col, origin_row);
    g_ptbs_calls = 0;
    g_ptbs_depth_max = 0;
    memset(g_ptbs_off, 0, sizeof(g_ptbs_off));
    memset(g_ptbs_cost, 0, sizeof(g_ptbs_cost));
    memset(g_ptbs_x, 0, sizeof(g_ptbs_x));
    memset(g_ptbs_y, 0, sizeof(g_ptbs_y));
    memset(g_ptbs_dir, 0, sizeof(g_ptbs_dir));
}

/* Open 5x5, origin centre, cost 3: identical residual diamond to the flood
 * fill (mode-0 commits the residual into the marker exactly as the flood fill
 * does), confirming the direction-tracked recursion walks the same tiles in the
 * same order with the same bounds and termination. Depth returns to 0. */
static void test_pf_open_diamond(void)
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

    pf_reset(2, 2);
    FF_MARK(2, 2) = 3;                 /* origin seeded by the orchestrator */
    fd2_pathfind_recursive_with_direction(2, 2, 3, &FF_MARK(2, 2));

    for (row = 0; row < MAPH; row++) {
        for (col = 0; col < MAPW; col++) {
            ASSERT_EQ(FF_MARK(col, row), expect[row][col]);
        }
    }
    ASSERT_TRUE(ff_guards_intact());
    ASSERT_EQ(data_fd2_battle_pathfind_current_depth, 0);   /* balanced */
}

/* Corner origin (0,0), cost 2: left/up are skipped (x==0 / y==0) so no OOB and
 * only the two in-bounds neighbours get marked -- same clamp as the flood fill. */
static void test_pf_corner_clamp(void)
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

    pf_reset(0, 0);
    FF_MARK(0, 0) = 2;
    fd2_pathfind_recursive_with_direction(0, 0, 2, &FF_MARK(0, 0));

    for (row = 0; row < MAPH; row++) {
        for (col = 0; col < MAPW; col++) {
            ASSERT_EQ(FF_MARK(col, row), expect[row][col]);
        }
    }
    ASSERT_TRUE(ff_guards_intact());
    ASSERT_EQ(data_fd2_battle_pathfind_current_depth, 0);
}

/* 0x40 impassable blocks expansion (never marked); 0x80 sink is marked but
 * pinned to 0 so the recursion stops past it -- same as the flood fill. */
static void test_pf_blocked_and_sink(void)
{
    pf_reset(2, 2);
    FF_FLAGS(3, 2) = 0x40;             /* impassable, east of origin */
    FF_FLAGS(2, 1) = 0x80;             /* sink, north of origin */
    FF_MARK(2, 2) = 4;
    fd2_pathfind_recursive_with_direction(2, 2, 4, &FF_MARK(2, 2));

    ASSERT_EQ(FF_MARK(3, 2), 0);       /* impassable stays unmarked */
    ASSERT_EQ(FF_MARK(4, 2), 0);       /* only reachable via the wall -> 0 */
    ASSERT_EQ(FF_MARK(2, 1), 0);       /* sink marked, pinned to 0 */
    ASSERT_EQ(FF_MARK(2, 0), 0);       /* sink cannot expand into it */
    ASSERT_EQ(FF_MARK(1, 2), 3);       /* west arm unobstructed */
    ASSERT_EQ(FF_MARK(0, 2), 2);
    ASSERT_TRUE(ff_guards_intact());
    ASSERT_EQ(data_fd2_battle_pathfind_current_depth, 0);
}

/* Direction bookkeeping in isolation: with cost 1 and a step cost of 1 every
 * neighbour's new residual is 0, which never improves the existing 0 marker, so
 * the inner step returns "no improve" for all four neighbours and the recursion
 * never descends. Exactly four calls happen, all from the origin, in the binary
 * order right/left/down/up with direction codes 3/1/0/2 and btm offsets
 * +4 / -4 / +stride / -stride; the neighbour coords and the active-frame
 * direction byte the stub observed must match. Depth was 1 during the calls and
 * returns to 0 afterwards. */
static void test_pf_direction_codes_and_offsets(void)
{
    int stride;

    pf_reset(2, 2);
    FF_MARK(2, 2) = 1;
    fd2_pathfind_recursive_with_direction(2, 2, 1, &FF_MARK(2, 2));

    stride = (int)data_fd2_battle_pathfind_map_width * 4;

    /* nothing improved -> no recursion -> exactly the four origin neighbours */
    ASSERT_EQ(g_ptbs_calls, 4);

    /* right: dir 3, btm +4, neighbour (3,2) */
    ASSERT_EQ(g_ptbs_dir[0], 3);
    ASSERT_EQ(g_ptbs_off[0], 4);
    ASSERT_EQ(g_ptbs_x[0], 3);
    ASSERT_EQ(g_ptbs_y[0], 2);
    /* left: dir 1, btm -4, neighbour (1,2) */
    ASSERT_EQ(g_ptbs_dir[1], 1);
    ASSERT_EQ(g_ptbs_off[1], -4);
    ASSERT_EQ(g_ptbs_x[1], 1);
    ASSERT_EQ(g_ptbs_y[1], 2);
    /* down: dir 0, btm +stride, neighbour (2,3) */
    ASSERT_EQ(g_ptbs_dir[2], 0);
    ASSERT_EQ(g_ptbs_off[2], stride);
    ASSERT_EQ(g_ptbs_x[2], 2);
    ASSERT_EQ(g_ptbs_y[2], 3);
    /* up: dir 2, btm -stride, neighbour (2,1) */
    ASSERT_EQ(g_ptbs_dir[3], 2);
    ASSERT_EQ(g_ptbs_off[3], -stride);
    ASSERT_EQ(g_ptbs_x[3], 2);
    ASSERT_EQ(g_ptbs_y[3], 1);

    /* every call was made at depth 1 (origin level); never deeper */
    ASSERT_EQ(g_ptbs_depth_max, 1);
    /* the four residuals passed in are all the origin's cost (1) */
    ASSERT_EQ(g_ptbs_cost[0], 1);
    ASSERT_EQ(g_ptbs_cost[3], 1);
    ASSERT_EQ(data_fd2_battle_pathfind_current_depth, 0);
}

/* When the recursion does descend, the depth marker climbs: from the centre of
 * an open map with cost 3 the search reaches tiles two steps out, so a
 * neighbour step is issued at depth 3 (origin depth 1 -> neighbour depth 2 ->
 * its neighbour depth 3). The per-level step-stack frames at successive depths
 * carry the branch direction; verify the deepest observed call depth and that
 * the recorder logged more than the four origin neighbours. */
static void test_pf_depth_climbs_on_descent(void)
{
    pf_reset(2, 2);
    FF_MARK(2, 2) = 3;
    fd2_pathfind_recursive_with_direction(2, 2, 3, &FF_MARK(2, 2));

    ASSERT_EQ(g_ptbs_depth_max, 3);
    ASSERT_TRUE(g_ptbs_calls > 4);
    ASSERT_EQ(data_fd2_battle_pathfind_current_depth, 0);
}

void run_util_pathfnd_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: util/pathfnd\n");
    RUN_TEST(test_pathfind_count_unique_dirs);
    RUN_TEST(test_floodfill_open_diamond);
    RUN_TEST(test_floodfill_corner_clamp);
    RUN_TEST(test_floodfill_blocked_and_sink);
    RUN_TEST(test_floodfill_no_improvement_no_spread);
    RUN_TEST(test_nstep_improves_writes_marker);
    RUN_TEST(test_nstep_unaffordable_rejected);
    RUN_TEST(test_nstep_equal_not_improving);
    RUN_TEST(test_nstep_signed_improvement);
    RUN_TEST(test_nstep_impassable_flag);
    RUN_TEST(test_nstep_sink_flag_forces_zero);
    RUN_TEST(test_nstep_cost_table_indexing);
    RUN_TEST(test_pf_open_diamond);
    RUN_TEST(test_pf_corner_clamp);
    RUN_TEST(test_pf_blocked_and_sink);
    RUN_TEST(test_pf_direction_codes_and_offsets);
    RUN_TEST(test_pf_depth_climbs_on_descent);
    printf("\n");
}
