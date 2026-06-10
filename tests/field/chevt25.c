/*
 * unit tests for src/field/chevt2.c (part 5)
 *
 * fd2_chapter_event_handler_49__unref_sentinel @ 0x35C23
 *
 * 10-byte sentinel stub: PUSH 4; CALL __CHK; JMP 0x35AAE into the Class-3 shared
 * tail (MOV EAX, [tile_event_consumed_flags]; MOV byte [EAX + 0x12], 1; RET)
 * hosted as alt_66 in fd2_chapter_event_handler_44 @ 0x35A48. Functionally-exact
 * body is the single unconditional byte store:
 *     *(uint8 *)(tile_event_consumed_flags_ptr + 0x12) = 1;
 *
 * Risk-bearing (state mutation / consumed-flag setter); driven over a real
 * in-memory flags buffer:
 *   (a) the store writes the immediate 1 into index 0x12,
 *   (b) it is UNCONDITIONAL — pre-seeding 0x12 to a non-1 sentinel still ends at 1
 *       (the binary has no CMP/JNZ gate, just MOV byte [EAX+0x12],1), which is the
 *       defining contrast against the gated sentinels (handler_3e/41 only write
 *       when their slot reads 0),
 *   (c) ONLY index 0x12 changes: the immediate neighbours 0x11 and 0x13 stay
 *       untouched, pinning the exact index (distinct from handler_45's 0x11 and
 *       the handler_41/43 data-table stores),
 *   (d) the dispatch arg is ignored (passed nonzero).
 *
 * fd2_chapter_event_handler_48__unref_ai_ctrl @ 0x35BF2
 *
 * Straight-line 2-portrait cinematic pair followed by one state mutation.
 * Functionally-exact body:
 *     fd2_cinematic_chapter_portrait_dump_with_white_flash(4,   0x23, 2);
 *     fd2_cinematic_chapter_portrait_dump_with_white_flash(0xE, 0x23, 3);
 *     data_fd2_battle_anim_phase = 1;
 *
 * In the binary the SECOND cutscene call falls through (no JMP) into
 * fd2_set_battle_anim_phase_to_1 @ 0x35C15, borrowing that function's
 * ADD ESP,0xC (the second call's arg cleanup) + MOV battle_anim_phase,1 + RET
 * tail. The functionally-exact source is the two complete calls plus the
 * battle_anim_phase store.
 *
 * The cinematic helper's own contract (arg order, low-byte chapter_id, the
 * 300/200/400 white-flash delay triple) is already pinned by the chevt23
 * test_white_flash_* cases; what is risk-bearing HERE is this handler's own
 * argument routing AND its distinguishing tail store, driven over the REAL
 * cinematic helper + REAL portrait loader (real FDFIELD.DAT):
 *   (a) BOTH cutscenes run, in order: the delay log holds exactly two
 *       300/200/400 triples (6 ticks),
 *   (b) the two chapter ids are exactly {2, 3} in that order: the tile-event
 *       table carries one race-2 record and TWO race-3 records. Cutscene 1
 *       (chapter 2) matches the single race-2 record (count += 1); cutscene 2
 *       (chapter 3) matches both race-3 records (count += 2) -> total 3. This
 *       count is unique to the correct {2, 3} pair: a duplicated {2,2} would
 *       total 2, a duplicated {3,3} would total 4, so 3 proves both the values
 *       AND their order,
 *   (c) the SECOND cutscene targets (0xE, 0x23) and runs last: the final window
 *       origin lands on (0xE, 0x23); the FIRST targets the distinct (4, 0x23)
 *       (same row y=0x23, different column),
 *   (d) ANIM_PHASE STORE (the defining tail vs handler_34, which has no such
 *       store): data_fd2_battle_anim_phase is written the immediate 1. Seeding
 *       it to a non-1 sentinel and observing it end at 1 pins this handler's own
 *       fall-through tail. The store is UNCONDITIONAL (it fires even when the
 *       cutscenes match no record, so it is the handler's own code, not a
 *       side effect of the loader),
 *   (e) the dispatch arg is ignored (passed nonzero).
 * The two pan composites and the white-flash palette writes execute for real as
 * a byproduct (pure display side effects, deferred to Phase 9 integration); only
 * the routing observables and the anim_phase store are asserted.
 *
 * Own in-memory fixture so the suite never aliases the other chevt2 part suites'
 * state.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];

/* testglob recorders */
extern int    g_composite_call_count;
extern int    g_delay375b2_log_on;
extern int    g_delay375b2_log_count;
extern uint32 g_delay375b2_log[16];

/* ---- portrait-loader fixture: a tile-event table of `count` records (stride
 * 0x1A) whose race bytes (+0x98) are races[k]; alloc_offset = count drives the
 * scan length. Chapter 4 -> the real loader re-reads real FDFIELD.DAT[4*3+2]. */
static uint8 *g_ce48_tileevent;

/* ---- host-safe render workspace + sprite atlas + 768-byte palette for the real
 * pan composites, the two white-flash palette writes, and the final composite. */
#define CE48_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ce48_ws[CE48_WS_SPAN];
static uint8 g_ce48_atlas[6 + 64 * 4 + 4];
static uint8 g_ce48_palette[256 * 3];

/* Stand up the full real-cinematic env (mirrors the proven chevt23 ce23_setup).
 * `count`/`races` drive the two cutscenes' chapter-id observation; the window
 * origin starts at (start_ox, start_oy) so the final pan target is observable.
 * anim_phase is seeded to a sentinel so the handler's tail store to 1 is
 * observable. */
static void ce48_setup(int count, const uint8 *races,
                       uint32 start_ox, uint32 start_oy)
{
    int i;
    uint32 *atlas_tbl;

    /* --- portrait loader env --- */
    g_ce48_tileevent =
        (uint8 *)malloc((size_t)0x98 + (size_t)count * 0x1a + 0x20);
    memset(g_ce48_tileevent, 0, (size_t)0x98 + (size_t)count * 0x1a + 0x20);
    for (i = 0; i < count; i++) {
        g_ce48_tileevent[i * 0x1a + 0x98] = races[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce48_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = (uint32)count;
    chapter_portrait_load_buffer = 0;            /* loaded fresh by the loader  */
    data_fd2_chapter_init_phase_flag = 1;        /* spawn = field value verbatim */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE    */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    /* --- render env for pan composites + final composite --- */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce48_ws - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_view_window_origin_x = start_ox;
    data_fd2_battle_view_window_origin_y = start_oy;
    atlas_tbl = (uint32 *)(g_ce48_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce48_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    /* --- 768-byte palette for the two fd2_set_vga_palette_range_with_add calls -- */
    for (i = 0; i < 256 * 3; i++) {
        g_ce48_palette[i] = 0x20;
    }
    data_fd2_vga_palette_data_ptr = (uint32)g_ce48_palette;

    /* anim_phase sentinel: the handler's own tail store must overwrite this with
     * the immediate 1; the cinematic body reads anim_phase as a nonzero composite
     * gate, and 0x55 is nonzero so the real pan/composite still run host-safely. */
    data_fd2_battle_anim_phase = 0x55;

    /* --- delay-tick log + composite counter --- */
    g_delay375b2_log_on = 1;
    g_delay375b2_log_count = 0;
    g_composite_call_count = 0;
}

static void ce48_teardown(void)
{
    free(g_ce48_tileevent);
    g_ce48_tileevent = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    data_fd2_battle_anim_phase = 0;
    g_delay375b2_log_on = 0;
    g_delay375b2_log_count = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * Full sequence: the handler runs two portrait cutscenes (chapter ids 2, 3 at
 * tiles (4, 0x23) / (0xE, 0x23)) then writes data_fd2_battle_anim_phase = 1. The
 * tile-event table carries one race-2 record (cutscene 1's id) and one race-3
 * record (cutscene 2's id) plus an off-by-one decoy (race 4) that must NOT match,
 * so both intended records init and the party count reaches 2. The window starts
 * away from (0xE, 0x23) on both axes so the second pan is observable on the final
 * origin. The dispatch arg is passed nonzero to prove it is ignored. anim_phase
 * flips from the 0x55 sentinel to 1.
 * ---------------------------------------------------------------- */
static void test_h48_two_cutscenes_then_anim_phase(void)
{
    /* idx0 race=2 (call-1 id), idx1 race=3 (call-2 id), idx2 decoy 4 */
    static const uint8 races[3] = { 2, 3, 4 };

    ce48_setup(3, races, 0x40, 0x40);

    fd2_chapter_event_handler_48__unref_ai_ctrl(0x77);

    /* (a) both cutscenes ran fully, in order: two 300/200/400 triples */
    ASSERT_EQ((long)g_delay375b2_log_count, 6);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);
    ASSERT_EQ((long)g_delay375b2_log[3], 300);
    ASSERT_EQ((long)g_delay375b2_log[4], 200);
    ASSERT_EQ((long)g_delay375b2_log[5], 400);
    /* (b) ids 2 and 3 each matched their record; the decoy (4) did not -> 2 */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 2);
    /* (c) the second (last) cutscene panned to the literal target (0xE, 0x23) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0xE);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x23);
    /* (d) the handler's tail wrote anim_phase = 1 (overwrote the 0x55 sentinel) */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);

    ce48_teardown();
}

/* ----------------------------------------------------------------
 * The two chapter ids are the literals {2, 3} in that order — not the pan coords
 * and not a single shared id. Seed the tile-event table with one race-2 record
 * and TWO race-3 records, plus coord decoys carrying the pan coords (4, 0xE, 0x23)
 * as their race. Cutscene 1 (chapter 2) matches the single race-2 record
 * (count += 1); cutscene 2 (chapter 3) matches BOTH race-3 records (count += 2);
 * the coord decoys (4, 0xE, 0x23) must match nothing. The count must reach
 * EXACTLY 3 — a duplicated {2,2} would total 2, a duplicated {3,3} would total 4,
 * and any cutscene forwarding a coord as its id would over-count. This pins both
 * ids and their order distinct from the (x, y) arguments.
 * ---------------------------------------------------------------- */
static void test_h48_chapter_ids_are_2_then_3_not_coords(void)
{
    /* race-2 x1 + race-3 x2 (the ids) + coord decoys 4, 0xE, 0x23 (must not match) */
    static const uint8 races[6] = { 2, 3, 3, 4, 0xE, 0x23 };

    ce48_setup(6, races, 0x40, 0x40);

    fd2_chapter_event_handler_48__unref_ai_ctrl(0);

    /* (b) ids were exactly {2, 3}: 2 (race-2 x1) + 3 (race-3 x2) = 3 total */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 3);
    /* the final pan still landed on (0xE, 0x23) and the tail still fired */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0xE);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x23);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);

    ce48_teardown();
}

/* ----------------------------------------------------------------
 * Both pan targets sit on the SAME row y=0x23 with distinct columns: the first
 * cutscene targets (4, 0x23), the second (0xE, 0x23). Start the window ON the
 * FIRST cutscene's tile (4, 0x23): if the second call were dropped the origin
 * would stay at (4, 0x23); landing on (0xE, 0x23) proves the second cutscene ran
 * and forwarded its own distinct column (0xE) while keeping the shared row 0x23.
 * The race never matches any id so the loader stays a host-safe no-op, keeping the
 * focus on the second call's pan target. Both cutscenes still log 6 delay ticks.
 * ---------------------------------------------------------------- */
static void test_h48_second_cutscene_pans_to_distinct_column_same_row(void)
{
    static const uint8 races[1] = { 0x7F };      /* never equals id 2 or 3 */

    ce48_setup(1, races, 4, 0x23);                /* start ON the 1st tile (4, 0x23) */

    fd2_chapter_event_handler_48__unref_ai_ctrl(0x33);

    /* origin moved off column 4 onto the 2nd cutscene's literal column 0xE while
     * staying on the shared row 0x23 -> the second call ran with its own coords */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0xE);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x23);
    /* both cutscenes fired -> 6 delay ticks (2 x 300/200/400) */
    ASSERT_EQ((long)g_delay375b2_log_count, 6);
    /* no record matched any id -> loader scan was a no-op both times */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);

    ce48_teardown();
}

/* ----------------------------------------------------------------
 * The anim_phase store is the handler's OWN unconditional tail, not a side effect
 * of the cutscene loader: drive a case where the cutscenes match no tile-event
 * record (race 0x7F never equals id 2 or 3), so the loader inits nothing
 * (party_member_count stays 0), yet data_fd2_battle_anim_phase must STILL flip
 * from the 0x55 sentinel to the immediate 1. This is the defining contrast against
 * handler_34 (two cutscenes, NO anim_phase write): the write fires regardless of
 * whether any portrait matched, proving it is this handler's fall-through tail.
 * The dispatch arg is passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h48_anim_phase_store_is_unconditional(void)
{
    static const uint8 races[1] = { 0x7F };      /* never equals id 2 or 3 */

    ce48_setup(1, races, 0x20, 0x20);

    fd2_chapter_event_handler_48__unref_ai_ctrl(0x55);

    /* the tail always fires and always writes the immediate 1, even with no
     * portrait match */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
    /* cutscene ids matched no record -> loader was a no-op */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    /* both cutscenes still ran (the tail does not gate them) -> 6 delay ticks */
    ASSERT_EQ((long)g_delay375b2_log_count, 6);

    ce48_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_49__unref_sentinel @ 0x35C23
 *
 * Pure single unconditional byte store: tile_event_consumed_flags[0x12] = 1.
 * Own flags fixture (0x20 bytes) so the suite never aliases other suites' state.
 * ================================================================ */
static uint8 g_ce49_flags[0x20];

static void ce49_setup(void)
{
    memset(g_ce49_flags, 0, sizeof(g_ce49_flags));
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce49_flags;
}

static void ce49_teardown(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
}

/* ----------------------------------------------------------------
 * The handler sets tile_event_consumed_flags[0x12] = 1 and nothing else. Start
 * with a zeroed flags buffer; after the call index 0x12 is exactly 1 while its
 * immediate neighbours 0x11 and 0x13 stay 0 (pins the exact index, distinct from
 * handler_45's 0x11). The dispatch arg is passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h49_sets_consumed_flag_0x12(void)
{
    ce49_setup();

    fd2_chapter_event_handler_49__unref_sentinel(0x77);

    /* (a) the store wrote the immediate 1 into index 0x12 */
    ASSERT_EQ((long)g_ce49_flags[0x12], 1);
    /* (c) only index 0x12 changed: immediate neighbours untouched */
    ASSERT_EQ((long)g_ce49_flags[0x11], 0);
    ASSERT_EQ((long)g_ce49_flags[0x13], 0);

    ce49_teardown();
}

/* ----------------------------------------------------------------
 * The store is UNCONDITIONAL (the binary has no CMP/JNZ gate, just
 * MOV byte [EAX+0x12],1) — the defining contrast against the gated sentinels
 * handler_3e/41 which only write when their slot reads 0. Pre-seed index 0x12
 * with a non-1 sentinel; after the call it must equal 1 (the store always fires
 * and always writes the immediate 1, never preserving the prior value). The
 * neighbours, also pre-seeded non-zero, must be left exactly as they were. The
 * dispatch arg is ignored.
 * ---------------------------------------------------------------- */
static void test_h49_store_is_unconditional_and_index_exact(void)
{
    ce49_setup();
    g_ce49_flags[0x12] = 0x5C;       /* stale sentinel, NOT 1 */
    g_ce49_flags[0x11] = 0xAB;       /* neighbour decoys: must be left untouched */
    g_ce49_flags[0x13] = 0xCD;

    fd2_chapter_event_handler_49__unref_sentinel(0);

    /* (b) the store always fires and always writes the immediate 1 */
    ASSERT_EQ((long)g_ce49_flags[0x12], 1);
    /* (c) neighbours preserved verbatim -> only 0x12 is touched */
    ASSERT_EQ((long)g_ce49_flags[0x11], 0xAB);
    ASSERT_EQ((long)g_ce49_flags[0x13], 0xCD);

    ce49_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_4a__ch29_dyn_turn_event @ 0x35C32
 *
 * ch29 8-stage rotating portrait cinematic. stage = consumed_flags[0x10]:
 *   fd2_cinematic_chapter_portrait_dump_with_white_flash(0xA, 0x1D, stage);
 *   if (stage != 7) tile_event_data_table[+3] = (uint8)(turn_counter + 1);
 *   consumed_flags[0x10] += 1;            (8-bit INC, the borrowed shared tail)
 *
 * Risk-bearing (RNG-free but: conditional scheduler branch on stage==7, 8-bit
 * turn+1 arithmetic, unconditional 8-bit stage advance, and the portrait id =
 * stage routing). Driven over the REAL cinematic helper + REAL portrait loader
 * (real FDICON.B24 / FDFIELD.DAT) plus an in-memory tile-event table (whose +3
 * slot is the scheduler target, never touched by the loader which only reads
 * race at +0x98) and an in-memory consumed_flags buffer (whose [0x10] is the
 * stage counter). Pure display side effects (pan composite, white-flash palette
 * writes) execute host-safely as a byproduct; only the routing observables, the
 * scheduler store, and the stage advance are asserted.
 *
 * Own fixture (own tile-event table + own flags buffer) so the suite never
 * aliases the h48/h49 state; reuses the module render-scratch workspace.
 * ================================================================ */
static uint8 *g_ce4a_tileevent;
static uint8  g_ce4a_flags[0x20];

/* Stand up the real-cinematic env (same shape as ce48_setup) plus the stage
 * counter. `count`/`races` drive whether the single cutscene's portrait id
 * (= stage) matches a record; `stage` seeds consumed_flags[0x10]; `turn` seeds
 * data_fd2_battle_turn_counter for the scheduler store. */
static void ce4a_setup(int count, const uint8 *races, uint8 stage, uint8 turn)
{
    int i;
    uint32 *atlas_tbl;

    /* --- portrait loader env (tile-event table @ data_table_ptr) --- */
    g_ce4a_tileevent =
        (uint8 *)malloc((size_t)0x98 + (size_t)count * 0x1a + 0x20);
    memset(g_ce4a_tileevent, 0, (size_t)0x98 + (size_t)count * 0x1a + 0x20);
    for (i = 0; i < count; i++) {
        g_ce4a_tileevent[i * 0x1a + 0x98] = races[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce4a_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = (uint32)count;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 1;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    /* --- render env for the pan composite + final composite --- */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce48_ws - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;
    atlas_tbl = (uint32 *)(g_ce48_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce48_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;
    for (i = 0; i < 256 * 3; i++) {
        g_ce48_palette[i] = 0x20;
    }
    data_fd2_vga_palette_data_ptr = (uint32)g_ce48_palette;
    data_fd2_battle_anim_phase = 1;              /* nonzero composite gate */

    /* --- the handler's own state: stage counter + turn counter --- */
    memset(g_ce4a_flags, 0, sizeof(g_ce4a_flags));
    g_ce4a_flags[0x10] = stage;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce4a_flags;
    data_fd2_battle_turn_counter = turn;

    g_delay375b2_log_on = 1;
    g_delay375b2_log_count = 0;
    g_composite_call_count = 0;
}

static void ce4a_teardown(void)
{
    free(g_ce4a_tileevent);
    g_ce4a_tileevent = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_turn_counter = 0;
    g_delay375b2_log_on = 0;
    g_delay375b2_log_count = 0;
    remove("FD2.TMP");
}

/* ----------------------------------------------------------------
 * The single portrait cutscene forwards chapter_id = the current stage value and
 * targets the fixed tile (0xA, 0x1D). Seed stage=3 and a tile-event table with
 * one race-3 record plus an off-by-one decoy (race 4): the cutscene's id (3)
 * matches the race-3 record (party_member_count += 1) and not the decoy, proving
 * chapter_id == stage. The window starts away from (0xA, 0x1D) on both axes, so
 * landing there proves the pan target. One cutscene -> exactly one 300/200/400
 * delay triple. The dispatch arg is passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h4a_portrait_uses_stage_value_and_fixed_tile(void)
{
    static const uint8 races[2] = { 3, 4 };      /* id 3 matches; 4 is a decoy */

    ce4a_setup(2, races, 3, 0x10);

    fd2_chapter_event_handler_4a__ch29_dyn_turn_event(0x77);

    /* one cutscene ran fully: exactly one 300/200/400 triple */
    ASSERT_EQ((long)g_delay375b2_log_count, 3);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);
    /* chapter_id forwarded == stage (3): only the race-3 record matched */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* the cutscene panned to the literal fixed tile (0xA, 0x1D) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0xA);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x1D);

    ce4a_teardown();
}

/* ----------------------------------------------------------------
 * When the stage is NOT the final one (stage != 7) the handler arms the next
 * turn-event: tile_event_data_table[+3] = (uint8)(turn_counter + 1). Seed
 * stage=2, turn_counter=0x40, and a race that never equals the stage (loader is a
 * host-safe no-op). Pre-seed +3 with a stale sentinel; after the call +3 holds
 * 0x41 (= 0x40 + 1), proving both the store and the +1. The neighbouring table
 * bytes (+2, +4) stay at their seeded sentinels, pinning the exact +3 offset. The
 * stage byte also advances 2 -> 3.
 * ---------------------------------------------------------------- */
static void test_h4a_schedules_next_turn_when_stage_not_7(void)
{
    static const uint8 races[1] = { 0x7F };      /* never equals stage 2 */

    ce4a_setup(1, races, 2, 0x40);
    g_ce4a_tileevent[2] = 0xAA;                  /* +2 neighbour decoy */
    g_ce4a_tileevent[3] = 0x5C;                  /* +3 stale sentinel  */
    g_ce4a_tileevent[4] = 0xBB;                  /* +4 neighbour decoy */

    fd2_chapter_event_handler_4a__ch29_dyn_turn_event(0);

    /* scheduler wrote turn_counter + 1 into +3 */
    ASSERT_EQ((long)g_ce4a_tileevent[3], 0x41);
    /* only +3 changed: immediate neighbours preserved */
    ASSERT_EQ((long)g_ce4a_tileevent[2], 0xAA);
    ASSERT_EQ((long)g_ce4a_tileevent[4], 0xBB);
    /* the stage counter advanced 2 -> 3 */
    ASSERT_EQ((long)g_ce4a_flags[0x10], 3);

    ce4a_teardown();
}

/* ----------------------------------------------------------------
 * On the FINAL stage (stage == 7) the scheduler is skipped (the binary JZ jumps
 * straight to the shared advance tail), so tile_event_data_table[+3] is left
 * untouched — the defining branch that stops the rotation. Seed stage=7 and a
 * stale sentinel at +3; after the call +3 still holds the sentinel (no store),
 * yet the stage byte STILL advances 7 -> 8 (the increment is unconditional). The
 * race never matches stage 7 so the loader is a no-op.
 * ---------------------------------------------------------------- */
static void test_h4a_no_schedule_on_final_stage_7(void)
{
    static const uint8 races[1] = { 0x7F };      /* never equals stage 7 */

    ce4a_setup(1, races, 7, 0x40);
    g_ce4a_tileevent[3] = 0x5C;                  /* +3 stale sentinel must survive */

    fd2_chapter_event_handler_4a__ch29_dyn_turn_event(0);

    /* stage == 7: scheduler skipped, +3 untouched (rotation stops) */
    ASSERT_EQ((long)g_ce4a_tileevent[3], 0x5C);
    /* but the stage advance is unconditional: 7 -> 8 */
    ASSERT_EQ((long)g_ce4a_flags[0x10], 8);

    ce4a_teardown();
}

/* ----------------------------------------------------------------
 * Both the stage advance and the turn+1 store are 8-bit (binary INC byte ptr /
 * INC DL). Seed stage=0xFF and turn_counter=0xFF (stage != 7 so the scheduler
 * still fires). After the call the stage byte wraps 0xFF -> 0x00 (8-bit INC) and
 * +3 holds 0x00 (= (uint8)(0xFF + 1)), pinning both truncations. The race never
 * matches stage 0xFF so the loader stays a no-op.
 * ---------------------------------------------------------------- */
static void test_h4a_stage_and_turn_arithmetic_are_8bit(void)
{
    static const uint8 races[1] = { 0x33 };      /* never equals stage 0xFF */

    ce4a_setup(1, races, 0xFF, 0xFF);
    g_ce4a_tileevent[3] = 0x5C;                  /* stale sentinel, overwritten */

    fd2_chapter_event_handler_4a__ch29_dyn_turn_event(0x12);

    /* turn+1 in 8-bit: (uint8)(0xFF + 1) == 0 stored at +3 */
    ASSERT_EQ((long)g_ce4a_tileevent[3], 0);
    /* stage advance in 8-bit: 0xFF wraps to 0 */
    ASSERT_EQ((long)g_ce4a_flags[0x10], 0);

    ce4a_teardown();
}

void run_field_chevt25_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 5)\n");
    RUN_TEST(test_h48_two_cutscenes_then_anim_phase);
    RUN_TEST(test_h48_chapter_ids_are_2_then_3_not_coords);
    RUN_TEST(test_h48_second_cutscene_pans_to_distinct_column_same_row);
    RUN_TEST(test_h48_anim_phase_store_is_unconditional);
    RUN_TEST(test_h49_sets_consumed_flag_0x12);
    RUN_TEST(test_h49_store_is_unconditional_and_index_exact);
    RUN_TEST(test_h4a_portrait_uses_stage_value_and_fixed_tile);
    RUN_TEST(test_h4a_schedules_next_turn_when_stage_not_7);
    RUN_TEST(test_h4a_no_schedule_on_final_stage_7);
    RUN_TEST(test_h4a_stage_and_turn_arithmetic_are_8bit);
    printf("\n");
}
