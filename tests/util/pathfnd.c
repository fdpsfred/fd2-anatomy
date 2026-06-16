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
 * (fd2_pathfind_recursive_with_direction @ 0x4E27C driving the now real inner
 * step fd2_pathfind_neighbor_step_with_tiebreak @ 0x4E330, both in
 * src/util/pathfnd.c). In mode 0 the inner step writes the residual into the
 * marker exactly like the flood fill, so the marker grid below matches the flood
 * fill's -- proving the direction-tracked recursion walks the same tiles in the
 * same order with the same bounds / +-4 / +-stride / termination -- while also
 * packing each visit's direction code into the tile's [-2] (attr-hi) byte, which
 * these tests assert directly. The real inner step is exercised branch-by-branch
 * by the dedicated pstep_* unit tests further down. */

/* Scratch destination output buffer for the real fd2_pathfind_check_destination_
 * save_path call the inner step makes on every mode-0 commit. */
static uint8 pf_outbuf[64];

/* Reset the shared ff map + cost tables (open, every step costs 1), zero the
 * recursion stack + depth marker the orchestrator would have cleared, set mode 0,
 * point the path output buffer at pf_outbuf, and (unless a test overrides them)
 * park the destination off-map (0xFF,0xFF) so the inner step's destination check
 * is inert and best_path_length starts at the orchestrator's 0xFF sentinel. */
static void pf_reset(void)
{
    ff_reset();
    memset(data_fd2_battle_pathfind_step_stack, 0, 64 * 8);
    memset(pf_outbuf, 0, sizeof(pf_outbuf));
    data_fd2_battle_pathfind_current_depth = 0;
    data_fd2_battle_pathfind_mode_flags = 0;
    data_fd2_battle_pathfind_path_output_buffer_ptr = (uint32)pf_outbuf;
    data_fd2_battle_pathfind_dst_x = 0xFF;
    data_fd2_battle_pathfind_dst_y = 0xFF;
    data_fd2_battle_pathfind_best_path_length = 0xFF;
}

/* attr-hi (the [-2] direction byte) of a tile = its packed direction code. */
#define FF_DIR(col, row) FF_MAP[((row) * MAPW + (col)) * 4 + 1]

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

    pf_reset();
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
 * only the two in-bounds neighbours get marked -- same clamp as the flood fill.
 * Each of the two marked neighbours improved exactly once and was reached at
 * depth 1 with the origin frame's single direction byte recorded, so its
 * direction code = count_unique_directions() over one frame = 1 transition * 4 =
 * 4, packed into the tile's [-2] (attr-hi) byte. */
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

    pf_reset();
    FF_MARK(0, 0) = 2;
    fd2_pathfind_recursive_with_direction(0, 0, 2, &FF_MARK(0, 0));

    for (row = 0; row < MAPH; row++) {
        for (col = 0; col < MAPW; col++) {
            ASSERT_EQ(FF_MARK(col, row), expect[row][col]);
        }
    }
    /* direction code (count*4) packed into [-2] of the two marked neighbours */
    ASSERT_EQ(FF_DIR(1, 0), 4);        /* right neighbour */
    ASSERT_EQ(FF_DIR(0, 1), 4);        /* down neighbour */
    ASSERT_TRUE(ff_guards_intact());
    ASSERT_EQ(data_fd2_battle_pathfind_current_depth, 0);
}

/* 0x40 impassable blocks expansion (never marked); 0x80 sink is marked but
 * pinned to 0 so the recursion stops past it -- same as the flood fill. */
static void test_pf_blocked_and_sink(void)
{
    pf_reset();
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

/* Destination recording (mode 0): with a real destination set at the corner
 * (0,0) of an open 5x5 and the origin at the centre, the search reaches (0,0)
 * and fd2_pathfind_check_destination_save_path snapshots the direction sequence
 * of the best (shortest) path into the output buffer. The shortest path centre
 * (2,2) -> (0,0) is 4 steps; best_path_length is updated to that depth and the 4
 * recorded direction bytes are the per-level frame [+3] codes copied out. */
static void test_pf_destination_path_recorded(void)
{
    pf_reset();
    data_fd2_battle_pathfind_dst_x = 0;
    data_fd2_battle_pathfind_dst_y = 0;
    FF_MARK(2, 2) = 5;                 /* budget 5 reaches (0,0) at distance 4 */
    fd2_pathfind_recursive_with_direction(2, 2, 5, &FF_MARK(2, 2));

    /* the destination was reached at depth 4 (a 4-step path) */
    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 4);
    /* the 4 output bytes are valid direction codes (3=right/1=left/0=down/2=up)
     * and the tile got marked with its residual (5 - 4 steps = 1) */
    ASSERT_EQ(FF_MARK(0, 0), 1);
    ASSERT_TRUE(pf_outbuf[0] <= 3 && pf_outbuf[1] <= 3
        && pf_outbuf[2] <= 3 && pf_outbuf[3] <= 3);
    ASSERT_EQ(data_fd2_battle_pathfind_current_depth, 0);
}

/* --- direct inner-step tests (fd2_pathfind_neighbor_step_with_tiebreak
 * @ 0x4E330). Drive the real step against a single tile so each
 * cost/improvement/tiebreak/mode/flag branch is hit in isolation. The marker
 * byte sits at +3 of a 4-byte tile; the step reads the attr word at [-3..-2], the
 * flags byte at [-1], and -- crucially -- packs the direction code into the
 * [-2] byte, which is the attribute word's HIGH byte. So the "direction nibble"
 * and the attribute high byte are the same storage: a commit OR's the direction
 * weight into [-2] while preserving its low 2 bits (the attribute's bits 8-9).
 *
 * These tests use their own cost tables (ps_tct / ps_costtab) sized to span the
 * full 10-bit attribute index, so any attribute value (including one with bits
 * 8-9 set, used to exercise the low-2-bit preservation) resolves to a defined
 * cost. ps_setup(cost) wires every attribute to that single cost; the indexing
 * test overrides a specific tct/costtab entry. count_unique_directions() walks
 * `current_depth` step-stack frames, so every test that reaches it sets a real
 * depth (>=1) and frame contents -- depth 0 is never valid (the binary only calls
 * it from inside the recursion, which always holds at least the origin frame). */

#define PS_TCT_N 0x1000             /* spans (0x3FF<<2)+1 = 4093 */
static uint8 ps_tct[PS_TCT_N];
static uint8 ps_costtab[256];

/* Wire every attribute -> tile cost `cost` (all tct entries 0 -> costtab[0]). */
static void ps_setup(uint8 cost)
{
    pf_reset();
    memset(ps_tct, 0, sizeof(ps_tct));
    memset(ps_costtab, 0, sizeof(ps_costtab));
    ps_costtab[0] = cost;
    data_fd2_battle_pathfind_tile_cost_table_ptr = (uint32)ps_tct;
    data_fd2_battle_pathfind_caller_context = (uint32)ps_costtab;
    /* default: a single origin frame whose dir matches prev sentinel so
     * count_unique_directions() yields weight 0 unless a test sets otherwise. */
    data_fd2_battle_pathfind_current_depth = 1;
    data_fd2_battle_pathfind_step_stack[0 * 8 + 3] = 0xFF;   /* == prev -> 0 */
}

/* Place attr/flags/marker on tile (2,2), set the mode, run the step with the
 * given residual and neighbour coords, return what it returned; *out receives the
 * residual handed back. The [-2] byte is the attr high byte ((attr>>8)&0xFF). */
static int pstep_run(uint16 attr, uint8 flags, uint8 marker,
    uint8 mode, uint8 remaining, uint8 nx, uint8 ny, uint8 *out)
{
    uint8 *tile;

    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    tile[0] = (uint8)(attr & 0xFF);
    tile[1] = (uint8)(attr >> 8);      /* [-2] relative to the marker = attr hi */
    tile[2] = flags;
    tile[3] = marker;
    data_fd2_battle_pathfind_mode_flags = mode;
    *out = 0;
    return fd2_pathfind_neighbor_step_with_tiebreak(nx, ny, remaining, &tile[3], out);
}

/* Strictly-better commit (mode 0): affordable, passable, new residual beats the
 * marker -> marker rewritten to the residual, the direction weight OR'd into [-2]
 * (preserving its low 2 attr bits), returns 1, out mirrors the residual. attr
 * 0x0300 sets [-2]'s low 2 bits (attr bits 8-9); with the default weight-0 frame
 * [-2] keeps exactly those preserved bits. */
static void test_pstep_strict_improve_commits(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    ret = pstep_run(0x0300, 0x00, 0, 0, 5, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 4);             /* residual 5 - cost 1 */
    ASSERT_EQ(out, 4);
    ASSERT_EQ(tile[1], 0x03);          /* weight 0 | preserved low 2 bits */
    ASSERT_TRUE(ff_guards_intact());
}

/* count_unique_directions() drives the packed direction weight: with depth 1 and
 * a single frame whose [+3] = 2 the transition count from prev 0xFF is 1 ->
 * weight 4, OR'd into [-2] over its preserved low 2 bits (attr 0x0200 -> 0x02). */
static void test_pstep_direction_code_packed(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    data_fd2_battle_pathfind_current_depth = 1;
    data_fd2_battle_pathfind_step_stack[0 * 8 + 3] = 2;
    ret = pstep_run(0x0200, 0x00, 0, 0, 5, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[1], (uint8)(4 | 0x02));   /* weight 4 | preserved low 2 bits */
    ASSERT_TRUE(ff_guards_intact());
}

/* Unaffordable (binary SUB borrow / JC): tile cost > remaining -> no write at
 * all (neither marker nor direction byte), returns 0. cost 3, remaining 2. */
static void test_pstep_unaffordable_rejected(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(3);
    ret = pstep_run(0x0000, 0x00, 7, 0, 2, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(tile[3], 7);             /* marker untouched */
    ASSERT_EQ(tile[1], 0x00);          /* direction byte (attr hi) untouched */
    ASSERT_TRUE(ff_guards_intact());
}

/* Existing strictly better (CMP CL,[btm]; JL): new residual < marker -> reject,
 * no write, returns 0. The improvement test is SIGNED. cost 1, new 4 < marker 6. */
static void test_pstep_existing_better_rejected(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    ret = pstep_run(0x0000, 0x00, 6, 0, 5, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(tile[3], 6);
    ASSERT_EQ(tile[1], 0x00);
    ASSERT_TRUE(ff_guards_intact());
}

/* Tie in mode 0: new residual == marker is not strictly better and mode != 1, so
 * the tie path rejects (no write), returns 0. cost 1, new 4 == marker 4. */
static void test_pstep_tie_mode0_rejected(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    ret = pstep_run(0x0000, 0x00, 4, 0, 5, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(tile[3], 4);
    ASSERT_EQ(tile[1], 0x00);
    ASSERT_TRUE(ff_guards_intact());
}

/* Tie in mode 1, new weight WINS: on a tie mode 1 compares the new direction
 * weight (count_unique_directions) against the marker's [-2] & 0xFC. With depth 1
 * frame[+3]=0 the new weight is 4; [-2] starts 0 (& 0xFC = 0) so 4 > 0 -> commit:
 * marker rewritten to the residual, [-2] OR'd to 4, returns 1. */
static void test_pstep_tie_mode1_wins(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    data_fd2_battle_pathfind_current_depth = 1;
    data_fd2_battle_pathfind_step_stack[0 * 8 + 3] = 0;    /* 1 transition -> 4 */
    ret = pstep_run(0x0000, 0x00, 4, 1, 5, 3, 2, &out);    /* tie: new 4 == marker 4 */
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 4);             /* committed residual */
    ASSERT_EQ(tile[1], 4);             /* weight 4 | preserved 0 */
    ASSERT_EQ(out, 4);
    ASSERT_TRUE(ff_guards_intact());
}

/* Tie in mode 1, new weight does NOT win: new weight 4 vs existing [-2] & 0xFC =
 * 8 -> 4 <= 8 (JBE) rejects, no write, returns 0. attr 0x0800 sets [-2] = 0x08. */
static void test_pstep_tie_mode1_loses(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    data_fd2_battle_pathfind_current_depth = 1;
    data_fd2_battle_pathfind_step_stack[0 * 8 + 3] = 0;    /* new weight 4 */
    ret = pstep_run(0x0800, 0x00, 4, 1, 5, 3, 2, &out);    /* [-2]=0x08 -> weight 8 */
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(tile[3], 4);             /* marker untouched */
    ASSERT_EQ(tile[1], 0x08);          /* direction byte untouched */
    ASSERT_TRUE(ff_guards_intact());
}

/* Mode 0/1 impassable (flags & 0x40): the direction byte is still written (the
 * binary packs [-2] before the passability gate), but the marker is NOT written
 * and the step returns 0 (no recurse). attr 0x0300 -> preserved low 2 bits 0x03,
 * default weight-0 frame -> [-2] becomes 0x03. */
static void test_pstep_impassable_writes_dir_not_marker(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    ret = pstep_run(0x0300, 0x40, 0, 0, 5, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(tile[3], 0);             /* marker NOT written */
    ASSERT_EQ(tile[1], 0x03);          /* weight 0 | preserved low 2 bits */
    ASSERT_TRUE(ff_guards_intact());
}

/* Mode 0/1 sink (flags & 0x80): reachable but the residual written is forced to
 * 0 (and handed back as 0) so the recursion stops; still returns 1. */
static void test_pstep_sink_forces_zero(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    ret = pstep_run(0x0000, 0x80, 0, 0, 5, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 0);             /* forced to 0, not 4 */
    ASSERT_EQ(out, 0);
    ASSERT_TRUE(ff_guards_intact());
}

/* Mode 2 (ignore-obstacles + dst-record): commit writes the residual marker even
 * for a tile that mode 0/1 would treat as impassable, and -- because the tile
 * carries 0x40 -- fd2_pathfind_record_destination_xy snapshots the neighbour x/y
 * into the output buffer and sets best_path_length = 1. Returns 1. */
static void test_pstep_mode2_records_destination(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(1);
    /* flags 0x40 makes record_destination_xy fire (it gates on btm[-1] & 0x40) */
    ret = pstep_run(0x0000, 0x40, 0, 2, 5, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 4);             /* mode 2 writes the residual marker */
    ASSERT_EQ(out, 4);
    ASSERT_EQ(pf_outbuf[0], 3);        /* neighbour x */
    ASSERT_EQ(pf_outbuf[1], 2);        /* neighbour y */
    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 1);
    ASSERT_TRUE(ff_guards_intact());
}

/* Two-level cost lookup in the step: attr=1 -> tct[(1<<2)+1]=tct[5]; set tct[5]=2
 * -> costtab[2]=3, so tile cost 3; remaining 10 -> new 7 (> marker 0 -> commit).
 * attr bit10 (0x400) is masked off so it resolves identically to attr 1. */
static void test_pstep_cost_table_indexing(void)
{
    uint8 *tile;
    uint8 out;
    int ret;

    ps_setup(0);                       /* baseline cost 0; override the attr-1 path */
    ps_tct[5] = 2;
    ps_costtab[2] = 3;
    ret = pstep_run((uint16)(0x0400 | 1), 0x00, 0, 0, 10, 3, 2, &out);
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(tile[3], 7);             /* 10 - 3 */
    ASSERT_EQ(out, 7);
    ASSERT_TRUE(ff_guards_intact());
}

/* --- direct destination-record tests (fd2_pathfind_record_destination_xy
 * @ 0x4E3B3, real-emitted in src/util/pathfnd.c). The function gates on the tile
 * flags byte at [btm_attr_ptr-1] & 0x40: when set it writes the neighbour (x, y)
 * into the path output buffer ([0]/[1]) and forces best_path_length = 1; when
 * clear it does nothing. We point btm_attr_ptr at a tile's marker (+3) so [-1]
 * is the tile's flags byte (+2), and watch pf_outbuf + best_path_length. */

/* 0x40 flag set: records x into [0], y into [1], best_path_length := 1. The
 * neighbour coords passed (here 0x12 / 0x34) are written verbatim, and only the
 * first two output bytes are touched. */
static void test_record_dst_flag_set_writes_xy(void)
{
    uint8 *tile;

    pf_reset();
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    tile[2] = 0x40;                    /* flags byte (== [-1] of &tile[3]) */
    fd2_pathfind_record_destination_xy(0x12, 0x34, &tile[3]);

    ASSERT_EQ(pf_outbuf[0], 0x12);     /* x */
    ASSERT_EQ(pf_outbuf[1], 0x34);     /* y */
    ASSERT_EQ(pf_outbuf[2], 0);        /* nothing past the two bytes */
    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 1);
    ASSERT_TRUE(ff_guards_intact());
}

/* 0x40 flag clear: no write at all. Other flag bits (here 0x80 | 0x3F) must not
 * trigger it -- only bit 0x40 does. pf_outbuf stays zero and best_path_length
 * keeps the orchestrator's 0xFF sentinel that pf_reset installs. */
static void test_record_dst_flag_clear_noop(void)
{
    uint8 *tile;

    pf_reset();
    tile = &FF_MAP[(2 * MAPW + 2) * 4];
    tile[2] = (uint8)(0x80 | 0x3F);    /* every bit except 0x40 */
    fd2_pathfind_record_destination_xy(0x12, 0x34, &tile[3]);

    ASSERT_EQ(pf_outbuf[0], 0);
    ASSERT_EQ(pf_outbuf[1], 0);
    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 0xFF);
    ASSERT_TRUE(ff_guards_intact());
}

/* --- direct path-completion tests (fd2_pathfind_check_destination_save_path
 * @ 0x4E401, real-emitted in src/util/pathfnd.c). The function fires only when
 * the passed (x, y) equals (dst_x, dst_y) AND current_depth <= best_path_length;
 * on a fire it lowers best_path_length to current_depth and (when depth>0) copies
 * current_depth direction bytes -- the [+3] byte of each 8-byte step-stack frame
 * -- into the path output buffer. We drive it directly with seeded globals. */

/* Arrival at the destination with a strictly shorter depth: best_path_length is
 * lowered and the per-frame direction bytes are copied out with the 8-byte
 * stride (each frame's [+3] byte), leaving the rest of the buffer untouched. */
static void test_save_path_records_shorter(void)
{
    pf_reset();
    data_fd2_battle_pathfind_dst_x = 7;
    data_fd2_battle_pathfind_dst_y = 9;
    data_fd2_battle_pathfind_current_depth = 3;
    data_fd2_battle_pathfind_best_path_length = 5;   /* 3 <= 5 -> fires */
    /* direction byte lives at frame[+3]; other frame bytes must be ignored */
    data_fd2_battle_pathfind_step_stack[0 * 8 + 3] = 3;   /* right */
    data_fd2_battle_pathfind_step_stack[1 * 8 + 3] = 1;   /* left  */
    data_fd2_battle_pathfind_step_stack[2 * 8 + 3] = 2;   /* up    */
    data_fd2_battle_pathfind_step_stack[3 * 8 + 3] = 0;   /* must NOT be copied */

    fd2_pathfind_check_destination_save_path(7, 9);

    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 3);
    ASSERT_EQ(pf_outbuf[0], 3);
    ASSERT_EQ(pf_outbuf[1], 1);
    ASSERT_EQ(pf_outbuf[2], 2);
    ASSERT_EQ(pf_outbuf[3], 0);        /* loop stopped after 3 bytes */
}

/* Coordinate mismatch on either axis -> the routine is inert (no best update, no
 * copy). Covers both the DL (x) and DH (y) guard branches. */
static void test_save_path_wrong_xy_noop(void)
{
    pf_reset();
    data_fd2_battle_pathfind_dst_x = 7;
    data_fd2_battle_pathfind_dst_y = 9;
    data_fd2_battle_pathfind_current_depth = 3;
    data_fd2_battle_pathfind_best_path_length = 5;
    data_fd2_battle_pathfind_step_stack[0 * 8 + 3] = 3;

    fd2_pathfind_check_destination_save_path(6, 9);   /* x wrong */
    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 5);
    ASSERT_EQ(pf_outbuf[0], 0);

    fd2_pathfind_check_destination_save_path(7, 8);   /* y wrong */
    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 5);
    ASSERT_EQ(pf_outbuf[0], 0);
}

/* At the destination but at a worse (deeper) depth than the best so far: the
 * unsigned depth-vs-best test (binary JA) rejects, so best_path_length is left
 * alone and nothing is copied. */
static void test_save_path_worse_depth_rejected(void)
{
    pf_reset();
    data_fd2_battle_pathfind_dst_x = 7;
    data_fd2_battle_pathfind_dst_y = 9;
    data_fd2_battle_pathfind_current_depth = 6;
    data_fd2_battle_pathfind_best_path_length = 4;   /* 6 > 4 -> rejected */
    data_fd2_battle_pathfind_step_stack[0 * 8 + 3] = 3;

    fd2_pathfind_check_destination_save_path(7, 9);

    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 4);   /* unchanged */
    ASSERT_EQ(pf_outbuf[0], 0);                                /* no copy */
}

/* Zero-depth arrival that still passes the depth-vs-best test: best_path_length
 * is overwritten with 0 BEFORE the depth-nonzero guard (binary stores AH then
 * OR AH,AH / JZ), but the copy loop is skipped. Exercises the assign-before-
 * zero-check ordering. */
static void test_save_path_zero_depth_sets_best_no_copy(void)
{
    pf_reset();
    data_fd2_battle_pathfind_dst_x = 7;
    data_fd2_battle_pathfind_dst_y = 9;
    data_fd2_battle_pathfind_current_depth = 0;
    data_fd2_battle_pathfind_best_path_length = 5;   /* 0 <= 5 -> fires */
    data_fd2_battle_pathfind_step_stack[0 * 8 + 3] = 3;

    fd2_pathfind_check_destination_save_path(7, 9);

    ASSERT_EQ(data_fd2_battle_pathfind_best_path_length, 0);   /* set to 0 */
    ASSERT_EQ(pf_outbuf[0], 0);                                /* loop skipped */
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
    RUN_TEST(test_pf_destination_path_recorded);
    RUN_TEST(test_pstep_strict_improve_commits);
    RUN_TEST(test_pstep_direction_code_packed);
    RUN_TEST(test_pstep_unaffordable_rejected);
    RUN_TEST(test_pstep_existing_better_rejected);
    RUN_TEST(test_pstep_tie_mode0_rejected);
    RUN_TEST(test_pstep_tie_mode1_wins);
    RUN_TEST(test_pstep_tie_mode1_loses);
    RUN_TEST(test_pstep_impassable_writes_dir_not_marker);
    RUN_TEST(test_pstep_sink_forces_zero);
    RUN_TEST(test_pstep_mode2_records_destination);
    RUN_TEST(test_pstep_cost_table_indexing);
    RUN_TEST(test_record_dst_flag_set_writes_xy);
    RUN_TEST(test_record_dst_flag_clear_noop);
    RUN_TEST(test_save_path_records_shorter);
    RUN_TEST(test_save_path_wrong_xy_noop);
    RUN_TEST(test_save_path_worse_depth_rejected);
    RUN_TEST(test_save_path_zero_depth_sets_best_no_copy);
    printf("\n");
}
