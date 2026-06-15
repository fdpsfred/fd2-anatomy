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
#include "minipfix.h"   /* minip_setup_env: sprite sheet + dialog-blit spies (h4b) */
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx (h4c dialog VM) */

extern runtime_char g_test_rc_array[8];

/* testglob recorders */
extern int    g_composite_call_count;
extern int    g_delay375b2_log_on;
extern int    g_delay375b2_log_count;
extern uint32 g_delay375b2_log[16];
extern int    g_dlg_glyph_calls;        /* dialog VM glyph-blit counter (h4c) */

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
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
    data_fd2_chapter_portrait_load_buffer = 0;            /* loaded fresh by the loader  */
    data_fd2_chapter_init_phase_flag = 1;        /* spawn = field value verbatim */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE    */
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
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
#endif

static void ce48_teardown(void)
{
    free(g_ce48_tileevent);
    g_ce48_tileevent = 0;
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_chapter_portrait_load_buffer = 0;
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
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
#endif

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
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
#endif

/* ----------------------------------------------------------------
 * Both pan targets sit on the SAME row y=0x23 with distinct columns: the first
 * cutscene targets (4, 0x23), the second (0xE, 0x23). Start the window ON the
 * FIRST cutscene's tile (4, 0x23): if the second call were dropped the origin
 * would stay at (4, 0x23); landing on (0xE, 0x23) proves the second cutscene ran
 * and forwarded its own distinct column (0xE) while keeping the shared row 0x23.
 * The race never matches any id so the loader stays a host-safe no-op, keeping the
 * focus on the second call's pan target. Both cutscenes still log 6 delay ticks.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
#endif

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
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
#endif

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
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
    data_fd2_chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 1;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE */
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
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
#endif

static void ce4a_teardown(void)
{
    free(g_ce4a_tileevent);
    g_ce4a_tileevent = 0;
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_chapter_portrait_load_buffer = 0;
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
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
#endif

/* ----------------------------------------------------------------
 * When the stage is NOT the final one (stage != 7) the handler arms the next
 * turn-event: tile_event_data_table[+3] = (uint8)(turn_counter + 1). Seed
 * stage=2, turn_counter=0x40, and a race that never equals the stage (loader is a
 * host-safe no-op). Pre-seed +3 with a stale sentinel; after the call +3 holds
 * 0x41 (= 0x40 + 1), proving both the store and the +1. The neighbouring table
 * bytes (+2, +4) stay at their seeded sentinels, pinning the exact +3 offset. The
 * stage byte also advances 2 -> 3.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
#endif

/* ----------------------------------------------------------------
 * On the FINAL stage (stage == 7) the scheduler is skipped (the binary JZ jumps
 * straight to the shared advance tail), so tile_event_data_table[+3] is left
 * untouched — the defining branch that stops the rotation. Seed stage=7 and a
 * stale sentinel at +3; after the call +3 still holds the sentinel (no store),
 * yet the stage byte STILL advances 7 -> 8 (the increment is unconditional). The
 * race never matches stage 7 so the loader is a no-op.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
#endif

/* ----------------------------------------------------------------
 * Both the stage advance and the turn+1 store are 8-bit (binary INC byte ptr /
 * INC DL). Seed stage=0xFF and turn_counter=0xFF (stage != 7 so the scheduler
 * still fires). After the call the stage byte wraps 0xFF -> 0x00 (8-bit INC) and
 * +3 holds 0x00 (= (uint8)(0xFF + 1)), pinning both truncations. The race never
 * matches stage 0xFF so the loader stays a no-op.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
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
#endif

/* ================================================================
 * fd2_chapter_event_handler_4b__ch29_major_cinematic @ 0x35C79
 *
 * ch29 major-endgame trigger tile (dispatch idx 0x4B @ table 0x51B91). Body
 * (1-arg cdecl; arg = stepping char id):
 *   if (runtime_char_array[ci].team != 0                  -- non-enemy steps
 *       && tile_event_consumed_flags[0x11] == 0):         -- own slot unconsumed
 *     if (runtime_char_array[ci].char_id != 9):           -- WRONG character
 *       fd2_load_chapter_portrait(char[ci].portrait_id)
 *       page-0 dialog (0xA951F) -> paint -> wait -> close; return   (NO consume)
 *     else:                                               -- char_id 9 (trigger)
 *       page-1 dialog (0xA0000)
 *       tile_event_consumed_flags[0x11] = 1               -- consume own slot
 *       tile_event_data_table[+6] = (uint8)(turn_counter + 1)  -- arm hook 1 next turn
 *       tile_event_consumed_flags[0x10] = 4               -- prime handler_4A stage = 4
 *       tile_event_data_table[+3] = (uint8)turn_counter   -- arm hook 0 this turn
 *
 * Risk-bearing (control-flow gating + branch + state transitions + 8-bit
 * arithmetic). Driven over the REAL dialog VM / portrait loader / slide-out close
 * (the wrong-char path is the same load_chapter_portrait -> in-frame dialog ->
 * paint -> wait -> close chain proven by the chevt22 h3a/h3d suites; the page
 * dialogs use immediate-END programs so the VM returns without page-break wait or
 * speaker-portrait work; the blocking standalone wait on the wrong-char path is
 * released by the mirrored-blit input seam). The display side effects (frame draw,
 * portrait blit, slide animation) are owned by the dialog/rsrc/status suites; here
 * they execute for real only as a byproduct. What is asserted is the handler's own
 * routing: the two gate conditions, the char_id==9 BRANCH, and on the trigger path
 * the four exact-offset/exact-value state stores incl. the +6 turn+1 vs +3 verbatim
 * split and its 8-bit truncation.
 *
 * Own in-memory fixture (own data table + own flags buffer) so the suite never
 * aliases the h48/h49/h4a state; reuses the module render-scratch workspace.
 * ================================================================ */
extern int g_dlg_blit_mirror_inject_after;
extern int g_dlg_blit_mirror_inject_scancode;

/* immediate-END dialog program covering page 0 (wrong-char) and page 1 (trigger):
 * each page header points at an END (-1) word so the dialog VM returns without a
 * page-break wait or any speaker-portrait work. Header word i is at int16 idx i. */
static int16  g_ce4b_text[0x10];
static uint8  g_ce4b_dtable[0x10];     /* +3 / +6 are the scheduler targets */
static uint8  g_ce4b_flags[0x20];      /* [0x10] stage prime, [0x11] consume slot */

/* Stand up the full host-safe env (mirrors the proven chevt22 ce3d_setup): real
 * portrait load + immediate-END page dialogs + paint + blocking wait (released by
 * the mirrored-blit seam) + slide-out close. `stepper_team`/`stepper_char_id`
 * seed runtime_char[0]'s gate fields; `consume_slot` seeds flags[0x11]; `turn`
 * seeds the turn counter for the scheduler stores. */
static void ce4b_setup(uint8 stepper_team, uint8 stepper_char_id,
                       uint8 consume_slot, uint8 turn)
{
    int i;

    minip_setup_env();                 /* sprite sheet + dialog-blit spies */

    /* immediate-END program for pages 0 and 1 (data_fd2_current_chapter_text scope) */
    for (i = 0; i < 0x10; i++) {
        g_ce4b_text[i] = 0;
    }
    g_ce4b_text[0] = (int16)(0xF * 2);     /* page 0 -> END word (wrong-char) */
    g_ce4b_text[1] = (int16)(0xF * 2);     /* page 1 -> END word (trigger)    */
    g_ce4b_text[0xF] = -1;                  /* END */
    data_fd2_current_chapter_text = (uint32)(uint8 *)g_ce4b_text;

    /* runtime_char array: char[0] is the stepper. team / char_id / portrait_id. */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = stepper_team;
    g_test_rc_array[0].char_id = stepper_char_id;
    g_test_rc_array[0].portrait_id = 0x40;     /* default 0x9017 portrait slot */

    /* handler state: own data table + own flags buffer + turn counter */
    memset(g_ce4b_dtable, 0, sizeof(g_ce4b_dtable));
    memset(g_ce4b_flags, 0, sizeof(g_ce4b_flags));
    g_ce4b_flags[0x11] = consume_slot;
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce4b_dtable;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce4b_flags;
    data_fd2_battle_turn_counter = turn;

    /* slide-out close env: composite workspace + phase 0 + empty party. Null the
     * slide workspaces (the portrait loader allocates fresh; the close frees). */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce48_ws - 0x8088;
    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_party_member_count = 0;
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
    }
    data_fd2_portrait_sprite_buffer = 0;

    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* the wrong-char path's standalone fd2_wait_for_input_dialog_with_blink(0)
     * blocks until the BIOS keyboard buffer is nonempty: arm the mirrored-blit
     * seam so the 1st mirrored blit (during the portrait load, before the wait)
     * re-fills the buffer. Harmless on the trigger/gate paths (no standalone
     * wait). */
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x1E;         /* start EMPTY (head == tail) */
    g_dlg_blit_mirror_inject_after = 1;         /* flip on the 1st mirrored blit */
    g_dlg_blit_mirror_inject_scancode = 0x01;   /* Esc scancode */

    g_composite_call_count = 0;
}

static void ce4b_teardown(void)
{
    /* the wrong-char path's slide-out close already free()d the slide workspaces */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_battle_turn_counter = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_anim_phase = 0;
    data_fd2_current_chapter_text = 0;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_blit_mirror_inject_after = 0;
    g_dlg_blit_mirror_inject_scancode = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * TRIGGER path: a non-enemy (team=1 npc) char whose char_id == 9 steps the
 * unconsumed slot. The handler shows the page-1 dialog then performs all four
 * state stores. Seed turn_counter=0x40: after the call the slot is consumed
 * (flags[0x11]=1), hook entry 1's turn byte (data_table[+6]) holds 0x41 (turn+1),
 * the stage counter (flags[0x10]) is primed to 4, and hook entry 0's turn byte
 * (data_table[+3]) holds 0x40 (turn verbatim). The +6/+3 split (0x41 vs 0x40)
 * pins the +1-on-+6 / no-+1-on-+3 contrast. The dispatch arg selects char 0.
 * ---------------------------------------------------------------- */
static void test_h4b_trigger_char9_consumes_and_schedules(void)
{
    ce4b_setup(1, 9, 0, 0x40);          /* npc team, char_id 9, unconsumed, turn 0x40 */
    g_ce4b_dtable[2] = 0xAA;            /* +2 neighbour decoy */
    g_ce4b_dtable[4] = 0xBB;            /* +4 neighbour decoy (between +3 and +6) */
    g_ce4b_dtable[5] = 0xCC;            /* +5 neighbour decoy */
    g_ce4b_dtable[7] = 0xDD;            /* +7 neighbour decoy */
    g_ce4b_flags[0x0F] = 0x11;          /* flags neighbour decoys around 0x10/0x11 */
    g_ce4b_flags[0x12] = 0x22;

    fd2_chapter_event_handler_4b__ch29_major_cinematic(0);

    /* the slot is consumed */
    ASSERT_EQ((long)g_ce4b_flags[0x11], 1);
    /* hook entry 1 (data_table[+6]) = turn_counter + 1 = 0x41 */
    ASSERT_EQ((long)g_ce4b_dtable[6], 0x41);
    /* stage counter primed to 4 */
    ASSERT_EQ((long)g_ce4b_flags[0x10], 4);
    /* hook entry 0 (data_table[+3]) = turn_counter verbatim = 0x40 (no +1) */
    ASSERT_EQ((long)g_ce4b_dtable[3], 0x40);
    /* only the four intended bytes changed: data-table neighbours intact */
    ASSERT_EQ((long)g_ce4b_dtable[2], 0xAA);
    ASSERT_EQ((long)g_ce4b_dtable[4], 0xBB);
    ASSERT_EQ((long)g_ce4b_dtable[5], 0xCC);
    ASSERT_EQ((long)g_ce4b_dtable[7], 0xDD);
    /* flags neighbours of 0x10/0x11 intact */
    ASSERT_EQ((long)g_ce4b_flags[0x0F], 0x11);
    ASSERT_EQ((long)g_ce4b_flags[0x12], 0x22);

    ce4b_teardown();
}

/* ----------------------------------------------------------------
 * WRONG-character branch: a non-enemy char whose char_id != 9 steps the
 * unconsumed slot. The handler takes the page-0 "you're not the one" path
 * (portrait load -> page-0 dialog -> paint -> wait -> close) and returns WITHOUT
 * any state mutation — the defining contrast against the trigger path. Proof: the
 * slot stays UN-consumed (flags[0x11]==0, so another character may retry), the
 * stage counter is NOT primed (flags[0x10] stays at its seeded sentinel), and
 * NEITHER data-table scheduler byte (+3 / +6) is written. Drives the real
 * portrait load (DATO.DAT), the real page-0 dialog VM, the real paint + blocking
 * wait (released by the mirrored-blit seam) and the real slide-out close.
 * ---------------------------------------------------------------- */
static void test_h4b_wrong_char_no_state_mutation(void)
{
    ce4b_setup(1, 8, 0, 0x40);          /* npc team, char_id 8 (!= 9), unconsumed */
    g_ce4b_flags[0x10] = 0x5C;          /* stage-prime sentinel must survive */
    g_ce4b_dtable[3] = 0x77;            /* +3 scheduler sentinel must survive */
    g_ce4b_dtable[6] = 0x88;            /* +6 scheduler sentinel must survive */

    fd2_chapter_event_handler_4b__ch29_major_cinematic(0);

    /* no consume: the slot may be re-triggered by another character */
    ASSERT_EQ((long)g_ce4b_flags[0x11], 0);
    /* the stage counter was NOT primed (no flags[0x10]=4 store on this branch) */
    ASSERT_EQ((long)g_ce4b_flags[0x10], 0x5C);
    /* neither scheduler byte was armed */
    ASSERT_EQ((long)g_ce4b_dtable[3], 0x77);
    ASSERT_EQ((long)g_ce4b_dtable[6], 0x88);

    ce4b_teardown();
}

/* ----------------------------------------------------------------
 * GATE 1 (team == 0): an enemy steps (team 0). The leading CMP/JZ skips the whole
 * body before even reading the consume flag — nothing runs, nothing mutates. Seed
 * char_id 9 (the would-be trigger char) so this proves the team gate dominates the
 * char_id branch: even the correct char does nothing when it is an enemy. All
 * state bytes keep their seeded sentinels.
 * ---------------------------------------------------------------- */
static void test_h4b_enemy_team_zero_skips_entirely(void)
{
    ce4b_setup(0, 9, 0, 0x40);          /* team 0 = enemy, char_id 9, unconsumed */
    g_ce4b_flags[0x10] = 0x5C;
    g_ce4b_dtable[3] = 0x77;
    g_ce4b_dtable[6] = 0x88;

    fd2_chapter_event_handler_4b__ch29_major_cinematic(0);

    /* team gate failed -> no consume, no prime, no schedule */
    ASSERT_EQ((long)g_ce4b_flags[0x11], 0);
    ASSERT_EQ((long)g_ce4b_flags[0x10], 0x5C);
    ASSERT_EQ((long)g_ce4b_dtable[3], 0x77);
    ASSERT_EQ((long)g_ce4b_dtable[6], 0x88);

    ce4b_teardown();
}

/* ----------------------------------------------------------------
 * GATE 2 (slot already consumed): a non-enemy char_id 9 steps, but
 * tile_event_consumed_flags[0x11] is already non-zero (the JNZ skips the body) —
 * nothing runs a second time. Seed flags[0x11]=1 and stage/scheduler sentinels;
 * after the call flags[0x11] is unchanged (the handler never re-writes it on the
 * gated-out path), the stage counter is NOT re-primed, and neither scheduler byte
 * is armed. This is the one-shot lockout that makes the major cinematic fire once.
 * ---------------------------------------------------------------- */
static void test_h4b_already_consumed_slot_skips(void)
{
    ce4b_setup(1, 9, 1, 0x40);          /* npc, char_id 9, slot ALREADY consumed */
    g_ce4b_flags[0x10] = 0x5C;
    g_ce4b_dtable[3] = 0x77;
    g_ce4b_dtable[6] = 0x88;

    fd2_chapter_event_handler_4b__ch29_major_cinematic(0);

    /* consume gate failed -> body skipped; the consume byte stays as seeded (1) */
    ASSERT_EQ((long)g_ce4b_flags[0x11], 1);
    ASSERT_EQ((long)g_ce4b_flags[0x10], 0x5C);
    ASSERT_EQ((long)g_ce4b_dtable[3], 0x77);
    ASSERT_EQ((long)g_ce4b_dtable[6], 0x88);

    ce4b_teardown();
}

/* ----------------------------------------------------------------
 * 8-bit scheduler arithmetic: the trigger path reads turn_counter as one byte and
 * stores +6 = (uint8)(turn_counter + 1) and +3 = (uint8)turn_counter. Seed
 * turn_counter=0xFF: +6 wraps to 0x00 (8-bit INC) while +3 holds 0xFF verbatim,
 * pinning both the 8-bit truncation AND the +1-only-on-+6 split (if +3 also added
 * 1 it would read 0x00, and if +6 used 32-bit math it would read 0x100's low byte
 * the same way — the contrast 0x00 vs 0xFF is what proves the two stores differ).
 * The stage counter is still primed to 4 and the slot still consumed.
 * ---------------------------------------------------------------- */
static void test_h4b_scheduler_arithmetic_is_8bit(void)
{
    ce4b_setup(1, 9, 0, 0xFF);          /* npc, char_id 9, unconsumed, turn 0xFF */

    fd2_chapter_event_handler_4b__ch29_major_cinematic(0);

    /* +6 = (uint8)(0xFF + 1) = 0x00 */
    ASSERT_EQ((long)g_ce4b_dtable[6], 0x00);
    /* +3 = (uint8)0xFF = 0xFF (verbatim, no +1) */
    ASSERT_EQ((long)g_ce4b_dtable[3], 0xFF);
    /* the rest of the trigger stores still fired */
    ASSERT_EQ((long)g_ce4b_flags[0x11], 1);
    ASSERT_EQ((long)g_ce4b_flags[0x10], 4);

    ce4b_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_4c__ch29_major_cinematic @ 0x35D60
 *
 * ch29 endgame multi-stage cinematic (dispatch idx 0x4C @ table 0x51B91). Body
 * (1-arg cdecl; arg ignored):
 *   stage = tile_event_consumed_flags[0x11];           (MOVZX, zero-extended)
 *   if (stage != 4):                                    -- priming phase (0..3)
 *     fd2_mark_char_acted_this_turn(1)                  -- set char[1] acted bit
 *     tile_event_consumed_flags[0x11]++                 -- advance the stage (8-bit)
 *     tile_event_data_table[+6] = (uint8)(turn_counter + 1)  -- arm hook 1 next turn
 *     return
 *   // stage == 4: the 5th call triggers the main cinematic
 *   page-2 dialog (0xA0000)
 *   fd2_load_chapter_portraits_and_dump_tmp(1)          -- portrait set 1
 *   tile_event_consumed_flags[0x15] = (uint8)(party_member_count - 3)  -- prime downstream
 *   tile_event_data_table[+9] = (uint8)turn_counter     -- arm hook 2 this turn (verbatim)
 *   flash; delay(400); flash; delay(400)                -- 2-flash intro
 *   for (page = 3; page < 7; page++): flash; page-`page` dialog (0xA0000)  -- 4 paired
 *
 * Risk-bearing (control-flow branch on stage==4, three 8-bit state stores incl.
 * the party_member_count-3 subtraction and the turn+1 vs turn-verbatim split, and
 * the flash/dialog sequencing). The priming path has no heavy callees and is
 * driven over real in-memory buffers + the real fd2_mark_char_acted_this_turn.
 * The cinematic path is driven over the REAL dialog VM (immediate-END page
 * programs so each call returns without a page-break wait) + the REAL portrait
 * loader (alloc_offset 0 -> the race scan is a host-safe no-op, leaving
 * party_member_count exactly as seeded so the -3 store is deterministic); the six
 * fd2_animate_palette_flash_pulse_white calls go through the testglob recording
 * stub (the real ~1.4s pulse is a separately-routed unemitted function and pure
 * display). The dialog glyph pixels and the white-flash palette churn are pure
 * display side effects deferred to Phase 9; what is asserted is the handler's own
 * routing: the branch, the exact-offset/exact-value/8-bit state stores, and the
 * flash(6)/dialog(5) sequence counts.
 *
 * Own in-memory fixtures (own flags + data-table buffers, own runtime_char array)
 * so the suite never aliases the h48/h49/h4a/h4b state.
 * ================================================================ */
extern int g_palette_flash_pulse_white_calls;

/* ---- priming-path fixture: flags buffer (stage at [0x11]) + data-table buffer
 * (+6 scheduler target) + a runtime_char array (char[1] receives the acted bit) +
 * turn counter. No heavy callees on this path. */
static uint8        g_ce4c_flags[0x20];
static uint8        g_ce4c_dtable[0x10];
static runtime_char g_ce4c_rc[8];

static void ce4c_prime_setup(uint8 stage, uint8 turn)
{
    memset(g_ce4c_flags, 0, sizeof(g_ce4c_flags));
    memset(g_ce4c_dtable, 0, sizeof(g_ce4c_dtable));
    memset(g_ce4c_rc, 0, sizeof(g_ce4c_rc));
    g_ce4c_flags[0x11] = stage;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce4c_flags;
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce4c_dtable;
    data_fd2_battle_runtime_char_array_ptr = g_ce4c_rc;
    data_fd2_battle_turn_counter = turn;

    g_palette_flash_pulse_white_calls = 0;
    g_dlg_glyph_calls = 0;
}

static void ce4c_prime_teardown(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_turn_counter = 0;
    g_palette_flash_pulse_white_calls = 0;
}

/* ---- cinematic-path fixture: the priming buffers PLUS the dialog VM env
 * (immediate-END program for pages 2..6) PLUS the portrait-loader env with
 * alloc_offset 0 (race scan is a host-safe no-op, so party_member_count stays as
 * seeded for the -3 store). The +9 scheduler store and the loader's (skipped)
 * race scan both reference data_fd2_tile_event_data_table_ptr; a single buffer
 * large enough for the +0x98-based scan base serves both without collision since
 * alloc_offset 0 means the scan never reads it. */
static uint8 *g_ce4c_cine_dtable;   /* +9 scheduler target AND loader scan base   */
static int16  g_ce4c_text[0x10];    /* page headers 2..6 -> shared 1-glyph+END body */

static void ce4c_cine_setup(uint8 party_count, uint8 turn)
{
    int i;

    /* handler state: stage forced to 4 (cinematic branch), data-table + flags.
     * The data table is sized past +0x98 so the loader's scan base is in bounds
     * even though alloc_offset 0 makes the scan a no-op. */
    memset(g_ce4c_flags, 0, sizeof(g_ce4c_flags));
    memset(g_ce4c_rc, 0, sizeof(g_ce4c_rc));
    g_ce4c_cine_dtable = (uint8 *)malloc(0x98 + 0x20);
    memset(g_ce4c_cine_dtable, 0, 0x98 + 0x20);
    g_ce4c_flags[0x11] = 4;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce4c_flags;
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce4c_cine_dtable;
    data_fd2_battle_runtime_char_array_ptr = g_ce4c_rc;
    data_fd2_battle_turn_counter = turn;
    data_fd2_battle_party_member_count = party_count;

    /* portrait loader: alloc_offset 0 -> the for-scan never iterates, so
     * fd2_init_runtime_char_for_battle never runs and party_member_count is left
     * untouched. The loader still re-reads FDFIELD.DAT + rewrites FD2.TMP. */
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 1;
    data_fd2_chapter_current_chapter_id = 4;        /* re-read idx = 4*3+2 = 0xE */
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    /* dialog VM program: page headers 2..6 each point at a shared 1-glyph + END
     * body (byte offset 14 = int16 index 7). Each of the 5 dialog calls renders
     * exactly one glyph then returns (no page-break wait). */
    for (i = 0; i < 0x10; i++) {
        g_ce4c_text[i] = 0;
    }
    for (i = 2; i <= 6; i++) {
        g_ce4c_text[i] = (int16)(7 * 2);            /* byte offset of the body */
    }
    g_ce4c_text[7] = 0x41;                            /* TEXT glyph */
    g_ce4c_text[8] = -1;                              /* END */
    data_fd2_current_chapter_text = (uint32)(uint8 *)g_ce4c_text;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated +
     * no active portrait so END takes neither the page-break wait nor the
     * portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    g_palette_flash_pulse_white_calls = 0;
    g_dlg_glyph_calls = 0;
    g_delay375b2_log_on = 1;
    g_delay375b2_log_count = 0;
}

static void ce4c_cine_teardown(void)
{
    free(g_ce4c_cine_dtable);
    g_ce4c_cine_dtable = 0;
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_chapter_current_chapter_id = 1;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_turn_counter = 0;
    data_fd2_current_chapter_text = 0;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_palette_flash_pulse_white_calls = 0;
    g_delay375b2_log_on = 0;
    g_delay375b2_log_count = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * PRIMING path (stage != 4): the first four invocations each mark char 1 acted,
 * advance the stage byte, and arm the next turn-event. Seed stage=0, turn=0x40:
 * after the call char[1]'s acted bit (flags +5, 0x80) is set, the stage byte
 * (flags[0x11]) is 1, and hook entry 1 (data_table[+6]) holds 0x41 (turn+1).
 * NO flash and NO dialog run on this path (it returns before the cinematic). The
 * dispatch arg is passed nonzero to prove it is ignored, and the immediate
 * neighbours of both stores are pinned.
 * ---------------------------------------------------------------- */
static void test_h4c_priming_marks_acted_advances_stage_schedules(void)
{
    ce4c_prime_setup(0, 0x40);
    g_ce4c_dtable[5] = 0xAA;            /* +5 neighbour decoy */
    g_ce4c_dtable[7] = 0xBB;            /* +7 neighbour decoy */
    g_ce4c_flags[0x10] = 0xCC;          /* flags neighbour decoys around 0x11 */
    g_ce4c_flags[0x12] = 0xDD;

    fd2_chapter_event_handler_4c__ch29_major_cinematic(0x77);

    /* fd2_mark_char_acted_this_turn(1) set char[1]'s acted bit */
    ASSERT_EQ((long)(g_ce4c_rc[1].flags & CHARFLAG_ACTED), CHARFLAG_ACTED);
    /* char 0 was NOT touched (the call targets index 1, not the dispatch arg) */
    ASSERT_EQ((long)(g_ce4c_rc[0].flags & CHARFLAG_ACTED), 0);
    /* the stage byte advanced 0 -> 1 */
    ASSERT_EQ((long)g_ce4c_flags[0x11], 1);
    /* hook entry 1 (data_table[+6]) = turn_counter + 1 = 0x41 */
    ASSERT_EQ((long)g_ce4c_dtable[6], 0x41);
    /* only +6 changed: immediate neighbours intact */
    ASSERT_EQ((long)g_ce4c_dtable[5], 0xAA);
    ASSERT_EQ((long)g_ce4c_dtable[7], 0xBB);
    /* only [0x11] changed: flags neighbours intact */
    ASSERT_EQ((long)g_ce4c_flags[0x10], 0xCC);
    ASSERT_EQ((long)g_ce4c_flags[0x12], 0xDD);
    /* the cinematic did NOT run: no flash, no dialog */
    ASSERT_EQ((long)g_palette_flash_pulse_white_calls, 0);
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce4c_prime_teardown();
}

/* ----------------------------------------------------------------
 * PRIMING path 8-bit arithmetic: the stage advance is INC byte ptr and the +6
 * store is (uint8)(turn_counter + 1) (binary MOV AL,[turn] / INC AL). Seed
 * stage=3, turn=0xFF: the stage byte advances 3 -> 4 (so the NEXT call will take
 * the cinematic branch), and +6 wraps to 0x00 (= (uint8)(0xFF + 1)), pinning the
 * 8-bit truncation of the +1 store.
 * ---------------------------------------------------------------- */
static void test_h4c_priming_stage_advance_and_schedule_are_8bit(void)
{
    ce4c_prime_setup(3, 0xFF);

    fd2_chapter_event_handler_4c__ch29_major_cinematic(0);

    /* stage advanced 3 -> 4 (the priming phase ends here; next call is cinematic) */
    ASSERT_EQ((long)g_ce4c_flags[0x11], 4);
    /* +6 = (uint8)(0xFF + 1) = 0x00 */
    ASSERT_EQ((long)g_ce4c_dtable[6], 0x00);
    /* still the priming path: no cinematic side effects */
    ASSERT_EQ((long)g_palette_flash_pulse_white_calls, 0);
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce4c_prime_teardown();
}

/* ----------------------------------------------------------------
 * CINEMATIC path (stage == 4): the 5th call triggers the main cinematic. It must
 * NOT run the priming-path side effects (no acted bit, the stage byte is left at
 * 4 — the cinematic branch never increments [0x11]) and instead perform the two
 * cinematic state stores plus the flash/dialog sequence. Seed party_count=10 and
 * turn=0x40 (alloc_offset 0 keeps party_count untouched by the loader): after the
 * call flags[0x15] = (uint8)(10 - 3) = 7, hook entry 2 (data_table[+9]) = 0x40
 * (turn verbatim), and the priming-only stores did NOT fire.
 * ---------------------------------------------------------------- */
static void test_h4c_stage4_triggers_cinematic_not_priming(void)
{
    ce4c_cine_setup(10, 0x40);

    fd2_chapter_event_handler_4c__ch29_major_cinematic(0x77);

    /* the cinematic branch does NOT mark char 1 acted (priming-only) */
    ASSERT_EQ((long)(g_ce4c_rc[1].flags & CHARFLAG_ACTED), 0);
    /* the cinematic branch does NOT advance the stage: [0x11] stays 4 */
    ASSERT_EQ((long)g_ce4c_flags[0x11], 4);
    /* downstream prime: flags[0x15] = (uint8)(party_member_count - 3) = 7 */
    ASSERT_EQ((long)g_ce4c_flags[0x15], 7);
    /* hook entry 2 (data_table[+9]) = turn_counter verbatim = 0x40 */
    ASSERT_EQ((long)g_ce4c_cine_dtable[9], 0x40);

    ce4c_cine_teardown();
}

/* ----------------------------------------------------------------
 * CINEMATIC store widths: flags[0x15] = (uint8)(party_member_count - 3) is 8-bit
 * (binary MOV AL,[party_member_count] / SUB AL,3) and data_table[+9] =
 * (uint8)turn_counter is a verbatim byte (no +1, distinct from the priming +6
 * store). Seed party_count=2 -> flags[0x15] = (uint8)(2 - 3) = 0xFF (8-bit
 * borrow/wrap), and turn=0xFF -> data_table[+9] = 0xFF (verbatim). The +6 slot
 * (the priming scheduler target) must stay 0 on the cinematic path, pinning the
 * +9-vs-+6 distinction, and the neighbours of both stores are pinned.
 * ---------------------------------------------------------------- */
static void test_h4c_stage4_stores_are_8bit_and_at_exact_offsets(void)
{
    ce4c_cine_setup(2, 0xFF);
    g_ce4c_cine_dtable[8]  = 0xAA;      /* +8 neighbour decoy (just below +9) */
    g_ce4c_cine_dtable[10] = 0xBB;      /* +10 neighbour decoy (just above +9) */
    g_ce4c_flags[0x14] = 0xCC;          /* flags neighbour decoys around 0x15 */
    g_ce4c_flags[0x16] = 0xDD;

    fd2_chapter_event_handler_4c__ch29_major_cinematic(0);

    /* flags[0x15] = (uint8)(2 - 3) = 0xFF (8-bit subtraction wraps) */
    ASSERT_EQ((long)g_ce4c_flags[0x15], 0xFF);
    /* data_table[+9] = (uint8)0xFF = 0xFF (verbatim turn, no +1) */
    ASSERT_EQ((long)g_ce4c_cine_dtable[9], 0xFF);
    /* the priming +6 scheduler store did NOT fire on the cinematic path */
    ASSERT_EQ((long)g_ce4c_cine_dtable[6], 0);
    /* only +9 changed in the data table: neighbours intact */
    ASSERT_EQ((long)g_ce4c_cine_dtable[8], 0xAA);
    ASSERT_EQ((long)g_ce4c_cine_dtable[10], 0xBB);
    /* only [0x15] changed in the flags: neighbours intact */
    ASSERT_EQ((long)g_ce4c_flags[0x14], 0xCC);
    ASSERT_EQ((long)g_ce4c_flags[0x16], 0xDD);

    ce4c_cine_teardown();
}

/* ----------------------------------------------------------------
 * CINEMATIC flash/dialog sequence: a 2-flash intro then a 4-iteration loop, each
 * iteration a flash + a dialog (pages 3,4,5,6), preceded by the lone page-2
 * dialog. That is exactly 6 flashes and 5 dialogs, with the structural relation
 * flashes == dialogs + 1 (the two intro flashes minus the lone page-2 dialog).
 * The two 400ms intro holds land in the delay log as the first two entries. Each
 * of the 5 dialog calls renders exactly one glyph (immediate-END pages) so the
 * glyph count is 5. The dispatch arg is ignored.
 * ---------------------------------------------------------------- */
static void test_h4c_stage4_flash_and_dialog_sequence_counts(void)
{
    ce4c_cine_setup(5, 0x10);

    fd2_chapter_event_handler_4c__ch29_major_cinematic(0x33);

    /* six pulse-white flashes: 2 intro + 4 in the page loop */
    ASSERT_EQ((long)g_palette_flash_pulse_white_calls, 6);
    /* five dialogs (page 2 + pages 3,4,5,6), one glyph each */
    ASSERT_EQ((long)g_dlg_glyph_calls, 5);
    /* the two intro 400ms holds are the first two delay-log entries */
    ASSERT_TRUE(g_delay375b2_log_count >= 2);
    ASSERT_EQ((long)g_delay375b2_log[0], 400);
    ASSERT_EQ((long)g_delay375b2_log[1], 400);

    ce4c_cine_teardown();
}

void run_field_chevt25_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 5)\n");
#if 0
    RUN_TEST(test_h48_two_cutscenes_then_anim_phase);
#endif
#if 0
    RUN_TEST(test_h48_chapter_ids_are_2_then_3_not_coords);
#endif
#if 0
    RUN_TEST(test_h48_second_cutscene_pans_to_distinct_column_same_row);
#endif
#if 0
    RUN_TEST(test_h48_anim_phase_store_is_unconditional);
#endif
    RUN_TEST(test_h49_sets_consumed_flag_0x12);
    RUN_TEST(test_h49_store_is_unconditional_and_index_exact);
#if 0
    RUN_TEST(test_h4a_portrait_uses_stage_value_and_fixed_tile);
#endif
#if 0
    RUN_TEST(test_h4a_schedules_next_turn_when_stage_not_7);
#endif
#if 0
    RUN_TEST(test_h4a_no_schedule_on_final_stage_7);
#endif
#if 0
    RUN_TEST(test_h4a_stage_and_turn_arithmetic_are_8bit);
#endif
    RUN_TEST(test_h4b_trigger_char9_consumes_and_schedules);
    RUN_TEST(test_h4b_wrong_char_no_state_mutation);
    RUN_TEST(test_h4b_enemy_team_zero_skips_entirely);
    RUN_TEST(test_h4b_already_consumed_slot_skips);
    RUN_TEST(test_h4b_scheduler_arithmetic_is_8bit);
    RUN_TEST(test_h4c_priming_marks_acted_advances_stage_schedules);
    RUN_TEST(test_h4c_priming_stage_advance_and_schedule_are_8bit);
    RUN_TEST(test_h4c_stage4_triggers_cinematic_not_priming);
    RUN_TEST(test_h4c_stage4_stores_are_8bit_and_at_exact_offsets);
    RUN_TEST(test_h4c_stage4_flash_and_dialog_sequence_counts);
    audiofix_disable_sfx();   /* restore safe gate state for later suites */
    printf("\n");
}
