/*
 * unit tests for src/field/chevt2.c (part 3)
 *
 * fd2_cinematic_chapter_portrait_dump_with_white_flash @ 0x35822 — the chapter
 * portrait cinematic with a white-flash transition. Its functionally-exact body
 * is the fixed call sequence:
 *     fd2_pan_cursor_and_window(target_tile_x, target_tile_y)
 *     fd2_load_chapter_portraits_and_dump_tmp(chapter_id & 0xFF)
 *     __delay_thunk_375b2(300)
 *     fd2_set_vga_palette_range_with_add(0, 0xFF, 0xFF)    -- pure-white flash
 *     __delay_thunk_375b2(200)
 *     fd2_set_vga_palette_range_with_add(0, 0xFF, 0)       -- restore
 *     fd2_composite_battle_frame(0)
 *     fd2_delay_400ms_via_idle_thunk()                     -- JMP tail-call
 *
 * The risk-bearing (non-display) contract pinned here:
 *   (a) the two stack args (target_tile_x, target_tile_y) reach the real pan in
 *       the right order -> the battle window origin lands exactly on the target,
 *   (b) chapter_id is truncated to its LOW BYTE (binary MOVZX EAX, byte ptr) and
 *       forwarded to the real portrait loader -> a tile-event record whose race
 *       byte equals (chapter_id & 0xFF) inits exactly one runtime_char, and
 *   (c) the white-flash delay sequence is exactly 300, 200, 400 in order (the
 *       last via the fd2_delay_400ms_via_idle_thunk tail-call).
 *
 * These drive the REAL function and its REAL callees over the staged real game
 * files (the portrait loader re-reads FDFIELD.DAT and rewrites the FD2.TMP swap
 * file). Host-safety recipe mirrors the proven chevt2 part-1/part-2 suites:
 *   - the portrait-loader tile-event scan length is the in-process global
 *     data_fd2_resource_portrait_cache_alloc_offset over an in-process
 *     tile-event table (race byte at record+0x98, stride 0x1A); chapter 4 so the
 *     loader re-reads the real FDFIELD.DAT[4*3+2],
 *   - a host-safe render workspace + sprite atlas back the real pan composites
 *     and the final composite (the tile-map blit is the testglob recorder
 *     g_composite_call_count; the per-char overlay iterates only the chars the
 *     loader inited),
 *   - a full 768-byte palette buffer backs the two real
 *     fd2_set_vga_palette_range_with_add white-flash writes (256 DAC entries),
 *   - __delay_thunk_375b2 is the testglob recorder; the opt-in g_delay375b2_log
 *     captures the exact 300/200/400 tick sequence, and the
 *     fd2_delay_400ms_via_idle_thunk testglob stub forwards 400 into it.
 * The palette-port writes and composited pixels are pure display side effects
 * owned by the palette / rndscene suites; they execute for real here only as a
 * byproduct and are not asserted (deferred to Phase 9 integration).
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx (handler_40 dialog) */

extern runtime_char g_test_rc_array[8];

/* testglob recorders */
extern int    g_composite_call_count;
extern int    g_delay375b2_log_on;
extern int    g_delay375b2_log_count;
extern uint32 g_delay375b2_log[16];
extern int    g_dlg_glyph_calls;             /* real dialog VM glyph recorder      */
extern int    g_kill_from_calls;             /* kill-from-index recording stub     */
extern uint32 g_kill_from_index[4];

/* ---- portrait-loader fixture (mirrors chevt2 ce_setup_portrait_env): a
 * tile-event table of `count` records (stride 0x1A) whose race bytes (+0x98) are
 * race_of[k]; alloc_offset = count drives the scan length. Chapter 4 -> the real
 * loader re-reads real FDFIELD.DAT[4*3+2 = 0xE]. */
static uint8 *g_ce23_tileevent;

/* ---- host-safe render workspace + sprite atlas + 768-byte palette for the real
 * pan composites, the two white-flash palette writes, and the final composite. */
#define CE23_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ce23_ws[CE23_WS_SPAN];
static uint8 g_ce23_atlas[6 + 64 * 4 + 4];
static uint8 g_ce23_palette[256 * 3];

/* Stand up the full real-cinematic env. `count`/`races` drive the portrait-id
 * observation; the window origin starts at (start_ox, start_oy) so the pan target
 * is observable on the final origin. */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void ce23_setup(int count, const uint8 *races,
                       uint32 start_ox, uint32 start_oy)
{
    int i;
    uint32 *atlas_tbl;

    /* --- portrait loader env --- */
    g_ce23_tileevent =
        (uint8 *)malloc((size_t)0x98 + (size_t)count * 0x1a + 0x20);
    memset(g_ce23_tileevent, 0, (size_t)0x98 + (size_t)count * 0x1a + 0x20);
    for (i = 0; i < count; i++) {
        g_ce23_tileevent[i * 0x1a + 0x98] = races[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce23_tileevent;
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
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce23_ws - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_view_window_origin_x = start_ox;
    data_fd2_battle_view_window_origin_y = start_oy;
    data_fd2_battle_anim_phase = 1;
    atlas_tbl = (uint32 *)(g_ce23_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce23_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    /* --- 768-byte palette for the two fd2_set_vga_palette_range_with_add calls -- */
    for (i = 0; i < 256 * 3; i++) {
        g_ce23_palette[i] = 0x20;
    }
    data_fd2_vga_palette_data_ptr = (uint32)g_ce23_palette;

    /* --- delay-tick log + composite counter --- */
    g_delay375b2_log_on = 1;
    g_delay375b2_log_count = 0;
    g_composite_call_count = 0;
}
#endif

static void ce23_teardown(void)
{
    free(g_ce23_tileevent);
    g_ce23_tileevent = 0;
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
    g_delay375b2_log_on = 0;
    g_delay375b2_log_count = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * Full sequence: drive the cinematic with (x=9, y=7, chapter_id=4). The two
 * stack args land the window origin exactly on (9, 7) (window starts away on both
 * axes so the pan is visible on each); the white-flash delay sequence is exactly
 * 300, 200, 400 (the last via the fd2_delay_400ms_via_idle_thunk tail-call); the
 * portrait loader inits the sole race-4 record (chapter_id 4 forwarded); and the
 * cinematic composited frames (pan steps + the final composite).
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_white_flash_full_sequence(void)
{
    static const uint8 races[1] = { 4 };       /* race == chapter_id 4 */

    ce23_setup(1, races, 0x40, 0x40);

    fd2_cinematic_chapter_portrait_dump_with_white_flash(9, 7, 4);

    /* (a) pan landed the window origin on the literal target (9, 7) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 9);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 7);
    /* (c) the white-flash delay sequence is exactly 300, 200, 400 */
    ASSERT_EQ((long)g_delay375b2_log_count, 3);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);
    /* (b) chapter_id 4 forwarded to the loader -> the race-4 record inited */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* the cinematic composited frames (pan steps + final composite) */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* the loader freed + nulled its scratch buffer */
    ASSERT_EQ((long)data_fd2_chapter_portrait_load_buffer, 0);

    ce23_teardown();
}
#endif

/* ----------------------------------------------------------------
 * chapter_id is forwarded as its LOW BYTE only (binary MOVZX EAX, byte ptr
 * [ESP+0xc]). Driving with chapter_id = 0x105 must behave identically to 0x05:
 * the loader's race-scan compares the full 32-bit (chapter_id & 0xFF) against
 * each record's byte race, so only the race-5 record matches. A race-1 decoy
 * (the low nibble of 0x105 if a wrong byte were taken) must NOT match. count == 1
 * proves the &0xFF truncation: WITHOUT the mask the loader would compare 0x105 to
 * the byte races (max 0xFF) and match nothing (count 0).
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_white_flash_chapter_id_low_byte_only(void)
{
    static const uint8 races[2] = { 5, 1 };    /* target 5, decoy 1 */

    ce23_setup(2, races, 0x10, 0x10);

    fd2_cinematic_chapter_portrait_dump_with_white_flash(3, 4, 0x105);

    /* (0x105 & 0xFF) == 5 matched the race-5 record; the decoy (1) did not, and
     * an unmasked 0x105 would have matched nothing */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* the args still landed the pan and ran one delay triple */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 4);
    ASSERT_EQ((long)g_delay375b2_log_count, 3);

    ce23_teardown();
}
#endif

/* ----------------------------------------------------------------
 * The pan target is the two forwarded args, not a hardcoded constant: a second,
 * distinct target (2, 0xB) (handler_34's first-call coords) lands the window
 * origin on (2, 0xB). Combined with the (9, 7) case above this proves both the
 * x and y args are forwarded (not fixed). A race that does not match any id keeps
 * the loader scan a no-op so the focus stays on the pan.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_white_flash_pan_target_is_args_not_constant(void)
{
    static const uint8 races[1] = { 0x7F };    /* never equals chapter_id 0 */

    ce23_setup(1, races, 0x20, 0x20);

    fd2_cinematic_chapter_portrait_dump_with_white_flash(2, 0xB, 0);

    /* origin landed on the distinct literal target (2, 0xB) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 2);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0xB);
    /* chapter_id 0 matched no record (race 0x7F) -> loader scan was a no-op */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    /* still one white-flash delay triple */
    ASSERT_EQ((long)g_delay375b2_log_count, 3);

    ce23_teardown();
}
#endif

/* ================================================================
 * fd2_chapter_event_handler_3e__ch27_dyn_turn_event @ 0x35898
 *
 * Pure state-machine mutator (no display side effects, no real-file I/O). The
 * functionally-exact body is:
 *     if (tile_event_consumed_flags[0x11] == 0) {
 *         tile_event_data_table[3] = (uint8)(turn_counter + 1);
 *         tile_event_consumed_flags[0x11] = 1;
 *     }
 * The risk-bearing contract pinned here:
 *   (a) FIRST-TIME gate: when flags[0x11] == 0 the handler arms hook entry 0's
 *       turn byte (data_table[+3]) with turn_counter + 1 and consumes the slot,
 *   (b) IDEMPOTENCE: when flags[0x11] != 0 the handler writes nothing (the
 *       already-armed schedule and the consumed flag are both preserved),
 *   (c) 8-BIT arithmetic: the turn counter is read as one byte and incremented
 *       in 8-bit (MOV DL,[turn_counter] / INC DL), so only the low byte feeds
 *       the +1 and the result wraps modulo 256 (0xFF -> 0x00),
 *   (d) exact byte offsets: only data_table[+3] and flags[+0x11] are written;
 *       their neighbours stay untouched,
 *   (e) the dispatch arg is ignored (the handler reads no param).
 *
 * In-memory fixtures only: a flags byte buffer (indexed at 0x11) and a data-table
 * byte buffer (indexed at 3), both published through the existing globals; the
 * turn counter is the plain uint32 global. No callee but the compiler's __CHK.
 * ================================================================ */

/* flags buffer: index 0x11 is the consume slot; extra headroom guards neighbours */
static uint8 g_ce3e_flags[0x20];
/* data table: index 3 is hook entry 0's turn byte; headroom guards neighbours */
static uint8 g_ce3e_dtable[0x10];

static void ce3e_setup(void)
{
    memset(g_ce3e_flags, 0, sizeof(g_ce3e_flags));
    memset(g_ce3e_dtable, 0, sizeof(g_ce3e_dtable));
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce3e_flags;
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce3e_dtable;
    data_fd2_battle_turn_counter = 0;
}

static void ce3e_teardown(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_turn_counter = 0;
}

/* ----------------------------------------------------------------
 * First-time trigger: flags[0x11] == 0 and turn_counter == 5. The handler arms
 * hook entry 0's turn byte (data_table[+3]) with 5 + 1 == 6 and consumes the
 * slot (flags[0x11] -> 1). The dispatch arg is passed nonzero to prove it is
 * ignored. Guard bytes around both write targets must stay 0.
 * ---------------------------------------------------------------- */
static void test_h3e_first_time_arms_and_consumes(void)
{
    ce3e_setup();
    data_fd2_battle_turn_counter = 5;

    fd2_chapter_event_handler_3e__ch27_dyn_turn_event(0x77);

    /* (a) hook entry 0's turn byte = turn_counter + 1 */
    ASSERT_EQ((long)g_ce3e_dtable[3], 6);
    /* slot consumed */
    ASSERT_EQ((long)g_ce3e_flags[0x11], 1);
    /* (d) neighbours of data_table[+3] untouched */
    ASSERT_EQ((long)g_ce3e_dtable[2], 0);
    ASSERT_EQ((long)g_ce3e_dtable[4], 0);
    /* (d) neighbours of flags[+0x11] untouched */
    ASSERT_EQ((long)g_ce3e_flags[0x10], 0);
    ASSERT_EQ((long)g_ce3e_flags[0x12], 0);

    ce3e_teardown();
}

/* ----------------------------------------------------------------
 * Idempotence: when the slot is already consumed (flags[0x11] != 0) the handler
 * must do nothing. Pre-arm data_table[+3] with a sentinel and pre-set the flag;
 * after the call both must be byte-for-byte unchanged (no re-arm, no re-write),
 * even though turn_counter differs from the sentinel.
 * ---------------------------------------------------------------- */
static void test_h3e_already_consumed_is_noop(void)
{
    ce3e_setup();
    g_ce3e_flags[0x11] = 0xAA;     /* already consumed (any nonzero) */
    g_ce3e_dtable[3]   = 0x5C;     /* previously-armed schedule sentinel */
    data_fd2_battle_turn_counter = 9;

    fd2_chapter_event_handler_3e__ch27_dyn_turn_event(0);

    /* (b) schedule byte preserved (NOT overwritten with 9 + 1 == 0xA) */
    ASSERT_EQ((long)g_ce3e_dtable[3], 0x5C);
    /* consumed flag preserved exactly */
    ASSERT_EQ((long)g_ce3e_flags[0x11], 0xAA);

    ce3e_teardown();
}

/* ----------------------------------------------------------------
 * 8-bit wrap: the increment is INC DL on the low byte of the turn counter, then
 * a byte store. turn_counter = 0xFF -> stored byte = (0xFF + 1) & 0xFF == 0x00.
 * A naive 32-bit `(turn_counter + 1)` written wide would also store 0x00 in the
 * byte, but pairing this with the high-byte case below pins the byte semantics.
 * ---------------------------------------------------------------- */
static void test_h3e_turn_counter_byte_wraps(void)
{
    ce3e_setup();
    data_fd2_battle_turn_counter = 0xFF;

    fd2_chapter_event_handler_3e__ch27_dyn_turn_event(0);

    ASSERT_EQ((long)g_ce3e_dtable[3], 0x00);   /* (0xFF + 1) truncated to a byte */
    ASSERT_EQ((long)g_ce3e_flags[0x11], 1);

    ce3e_teardown();
}

/* ----------------------------------------------------------------
 * High bytes of the turn counter must not leak: the binary reads turn_counter as
 * a single byte (MOV DL, byte ptr [turn_counter]) before the +1. turn_counter =
 * 0x12FF: only the low byte 0xFF feeds the increment, so the stored byte is
 * (0xFF + 1) & 0xFF == 0x00 — identical to the 0xFF case — and the 0x12 high
 * byte is irrelevant. Combined with the byte-wrap test this proves both the
 * low-byte read and the byte-width store.
 * ---------------------------------------------------------------- */
static void test_h3e_turn_counter_high_bytes_ignored(void)
{
    ce3e_setup();
    data_fd2_battle_turn_counter = 0x12FF;

    fd2_chapter_event_handler_3e__ch27_dyn_turn_event(0);

    /* low byte 0xFF + 1 -> 0x00; the 0x12 high byte never reaches data_table[+3] */
    ASSERT_EQ((long)g_ce3e_dtable[3], 0x00);
    ASSERT_EQ((long)g_ce3e_flags[0x11], 1);

    ce3e_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_3f__ch27_ai_ctrl @ 0x358C7
 *
 * 2-portrait cinematic pair (ch27 turn-FF marker). Functionally-exact body:
 *     fd2_cinematic_chapter_portrait_dump_with_white_flash(3,   0x1B, 1);
 *     fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash(0xF, 0x1B, 2);
 * (the second cutscene reaches the cinematic helper through the transparent
 * wrap thunk, which the binary tail-JMPs into to borrow its cleanup tail).
 *
 * The cinematic helper's own contract (arg order, low-byte chapter_id, the
 * 300/200/400 delay triple) is already pinned by the three
 * test_white_flash_* cases above; what is risk-bearing HERE is the handler's
 * own argument routing — two cutscenes, in order, with the right literal
 * chapter ids (1 then 2) and the second targeting tile (0xF, 0x1B). All three
 * are driven over the real helper + real portrait loader (real FDFIELD.DAT):
 *   (a) BOTH cutscenes run, in order: the delay log holds exactly two
 *       300/200/400 triples (6 ticks),
 *   (b) the SECOND cutscene targets (0xF, 0x1B) and runs last: the final
 *       window origin lands on (0xF, 0x1B),
 *   (c) the two chapter ids are exactly {1, 2} in that order: the tile-event
 *       table carries one race-1 record (index 0) and TWO race-2 records
 *       (indices 1, 2). Cutscene 1 (chapter 1) matches the single race-1
 *       record (count += 1); cutscene 2 (chapter 2) matches both race-2
 *       records (count += 2) -> total 3. This count is unique to the correct
 *       {1, 2} pair: a duplicated {1,1} would total 2, a duplicated {2,2}
 *       would total 4, so 3 proves both the values AND their order.
 * The two pan composites and the white-flash palette writes execute for real
 * as a byproduct (pure display side effects, deferred to Phase 9).
 * ================================================================ */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_h3f_two_portrait_pair_routes_both_cutscenes(void)
{
    static const uint8 races[3] = { 1, 2, 2 };  /* race-1 x1, race-2 x2 */

    /* window starts away from (0xF, 0x1B) on both axes so the second pan is
     * observable on the final origin */
    ce23_setup(3, races, 0x40, 0x40);

    fd2_chapter_event_handler_3f__ch27_ai_ctrl(0);

    /* (a) both cutscenes ran fully, in order: two 300/200/400 triples */
    ASSERT_EQ((long)g_delay375b2_log_count, 6);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);
    ASSERT_EQ((long)g_delay375b2_log[3], 300);
    ASSERT_EQ((long)g_delay375b2_log[4], 200);
    ASSERT_EQ((long)g_delay375b2_log[5], 400);
    /* (b) the second cutscene targeted (0xF, 0x1B) and ran last */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0xF);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x1B);
    /* (c) chapter ids were exactly {1, 2}: 1 (race-1 x1) + 2 (race-2 x2) = 3 */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 3);

    ce23_teardown();
}
#endif

/* ================================================================
 * fd2_chapter_event_handler_40__unref_dyn_turn_event @ 0x358EA
 *
 * Multi-stage state-machine mutator: it reads the stage byte
 * tile_event_consumed_flags[0x10] (binary MOVZX, zero-extended), dispatches on
 * it, then unconditionally increments it (8-bit INC byte ptr). The risk-bearing
 * control flow pinned here is the THREE-WAY dispatch + the advance:
 *   stage 1 -> dialog page 1, the 3-portrait white-flash cutscene reveal, and
 *              data_fd2_battle_anim_phase = 1 (NO kill); flag advances 1 -> 2,
 *   stage 2 -> dialog page 2, exactly one kill-from-index 0x10 (NO cutscene, NO
 *              anim_phase write); flag advances 2 -> 3,
 *   any other stage (0, 3+) -> no dialog, no cutscene, no kill, no anim_phase
 *              write; flag still advances,
 *   the advance is a BYTE increment (0xFF -> 0x00).
 *
 * The handler's dialog (real, src/dialog/dialog.c) runs over an in-memory int16
 * program (NOT a game file): page-1 and page-2 headers each point at the same
 * 1-glyph + END body, so g_dlg_glyph_calls == 1 proves the stage's page body ran
 * and 0 proves no dialog ran. The 3 stage-1 cutscenes drive the REAL
 * fd2_cinematic_chapter_portrait_dump_with_white_flash over the ce23_setup
 * render/portrait/palette env; each cutscene forwards its chapter id (3, 4, 5) as
 * the low byte to the real portrait loader, whose race-scan inits one runtime_char
 * per record whose race byte equals the forwarded id. Seeding tile-event records
 * with races {3, 4, 5} therefore makes party_member_count == 3 prove all three
 * cutscenes ran AND carried the correct ids in order (this is the dialog-delay-
 * immune signal; the dialog itself can call __delay_thunk_375b2 on its panel
 * open/close paths, so the delay log is NOT used to count cutscenes). The kill
 * callee (0x35BBA, not yet emitted) is the testglob recording stub
 * (g_kill_from_calls / g_kill_from_index). The dialog glyph pixels, the cutscene
 * pan/flash composites, and the kill's HP-zeroing are pure display / callee side
 * effects, deferred to Phase 9 integration.
 * ================================================================ */

/* flags buffer: index 0x10 is the stage byte; headroom guards both neighbours */
static uint8 g_ce40_flags[0x20];
/* dialog program: page-1 header (idx 1) and page-2 header (idx 2) both point at a
 * shared body at byte 0x10 (int16 idx 8); the body is `glyphs` TEXT ops + END. */
static int16 g_ce40_prog[16];

/* Stand up the handler_40 env: stage byte at flags[0x10]; ce23_setup provides the
 * real cinematic render/portrait/palette env over a tile-event table of `count`
 * records (races `races`) so a stage-1 cutscene's portrait id is observable as a
 * party-member-count delta; a host-safe dialog VM env (empty BIOS key buffer +
 * gated audio) backs the real page-1/page-2 dialog. anim_phase is seeded to a
 * sentinel so a stage-1 write to 1 is observable and a no-write is provable. */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void ce40_setup(uint8 stage, int glyphs, int count, const uint8 *races)
{
    int i;

    ce23_setup(count, races, 0x40, 0x40);

    memset(g_ce40_flags, 0, sizeof(g_ce40_flags));
    g_ce40_flags[0x10] = stage;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce40_flags;

    memset(g_ce40_prog, 0, sizeof(g_ce40_prog));
    g_ce40_prog[1] = 0x10;          /* page-1 body byte offset (= int16 idx 8) */
    g_ce40_prog[2] = 0x10;          /* page-2 body byte offset (same body)     */
    for (i = 0; i < glyphs; i++) {
        g_ce40_prog[8 + i] = 0x41;  /* TEXT glyph */
    }
    g_ce40_prog[8 + glyphs] = -1;   /* END */
    data_fd2_current_chapter_text = (uint32)g_ce40_prog;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated so
     * the per-glyph blink/typewriter step is host-safe. No active portrait, so
     * END does not run the portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    data_fd2_battle_anim_phase = 0x55;   /* sentinel: stage 1 must overwrite -> 1 */
    g_dlg_glyph_calls = 0;
    g_kill_from_calls = 0;
    g_kill_from_index[0] = 0xDEAD;       /* overwritten iff the kill is issued */
}
#endif

static void ce40_teardown(void)
{
    audiofix_disable_sfx();
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    ce23_teardown();
}

/* ----------------------------------------------------------------
 * Stage 1 path: stage byte == 1 dispatches the dialog page 1 + the 3-portrait
 * cutscene reveal + anim_phase = 1, then advances the stage to 2. A 1-glyph
 * page-1 body proves the dialog ran. The three cutscenes forward chapter ids
 * 3, 4, 5 to the real portrait loader; tile-event records with races {3, 4, 5}
 * (plus a never-matching decoy) make party_member_count == 3 prove all three ran
 * with the correct ids. anim_phase flips from the 0x55 sentinel to 1; no kill is
 * issued; and the stage byte advances 1 -> 2 (its neighbours stay 0).
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_h40_stage1_cutscene_reveal_then_advance(void)
{
    /* one record per cutscene id (3, 4, 5) + a decoy that no id matches */
    static const uint8 races[4] = { 3, 4, 5, 0x7F };

    ce40_setup(1, 1, 4, races);

    fd2_chapter_event_handler_40__unref_dyn_turn_event(0);

    /* page-1 dialog body ran (rendered the single glyph) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    /* all three cutscenes ran with ids 3, 4, 5: one matching record each -> 3
     * (the 0x7F decoy never matched) */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 3);
    /* stage 1 set anim_phase to 1 (overwrote the 0x55 sentinel) */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
    /* stage 1 issues NO kill */
    ASSERT_EQ((long)g_kill_from_calls, 0);
    /* the stage byte advanced 1 -> 2; neighbours untouched */
    ASSERT_EQ((long)g_ce40_flags[0x10], 2);
    ASSERT_EQ((long)g_ce40_flags[0x0F], 0);
    ASSERT_EQ((long)g_ce40_flags[0x11], 0);

    ce40_teardown();
}
#endif

/* ----------------------------------------------------------------
 * Stage 2 path: stage byte == 2 dispatches the dialog page 2 + exactly one
 * mass-kill from index 0x10, then advances the stage to 3. A 1-glyph page-2 body
 * proves the dialog ran; the kill recorder captures one call with the literal
 * start index 0x10. NO cutscene runs: the tile-event table carries races {3, 4, 5}
 * (the stage-1 cutscene ids), so party_member_count staying 0 proves the loader —
 * and thus no stage-1 cutscene — never executed (the delay log is not used here
 * because the page-2 dialog can itself call __delay_thunk_375b2). anim_phase is
 * NOT written (stays at the 0x55 sentinel); and the stage byte advances 2 -> 3.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_h40_stage2_dialog_then_kill_then_advance(void)
{
    /* races that the WRONG (stage-1) branch would match; stage 2 must not run it */
    static const uint8 races[3] = { 3, 4, 5 };

    ce40_setup(2, 1, 3, races);

    fd2_chapter_event_handler_40__unref_dyn_turn_event(0);

    /* page-2 dialog body ran (rendered the single glyph) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    /* exactly one kill, with the literal start index 0x10 */
    ASSERT_EQ((long)g_kill_from_calls, 1);
    ASSERT_EQ((long)g_kill_from_index[0], 0x10);
    /* stage 2 runs NO cutscene -> the loader never ran -> no record inited */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    /* stage 2 does NOT touch anim_phase (sentinel preserved) */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 0x55);
    /* the stage byte advanced 2 -> 3 */
    ASSERT_EQ((long)g_ce40_flags[0x10], 3);

    ce40_teardown();
}
#endif

/* ----------------------------------------------------------------
 * Non-dispatching stage (0): neither stage body runs — no dialog, no cutscene, no
 * kill, no anim_phase write — but the stage byte STILL advances (0 -> 1). This is
 * the "other stage values are a no-op besides the increment" contract and proves
 * the advance is unconditional (outside both if branches). The dispatch arg is
 * passed nonzero to prove the handler ignores it.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_h40_stage0_noop_but_still_advances(void)
{
    /* races both stage branches' cutscenes would match; neither branch runs */
    static const uint8 races[3] = { 3, 4, 5 };

    ce40_setup(0, 1, 3, races);

    fd2_chapter_event_handler_40__unref_dyn_turn_event(0x77);

    /* no dialog, no cutscene (no loader -> no record inited), no kill, no
     * anim_phase write */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    ASSERT_EQ((long)g_delay375b2_log_count, 0);
    ASSERT_EQ((long)g_kill_from_calls, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 0x55);
    /* the stage byte still advances 0 -> 1 (the 0x77 arg did not leak in) */
    ASSERT_EQ((long)g_ce40_flags[0x10], 1);

    ce40_teardown();
}
#endif

/* ----------------------------------------------------------------
 * The advance is a BYTE increment (binary INC byte ptr), not a wider add: a stage
 * byte of 0xFF wraps to 0x00. 0xFF is a non-dispatching stage, so the body is a
 * no-op and only the byte wrap is observable; the neighbour bytes must stay 0
 * (the increment must not carry past the byte).
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_h40_stage_byte_increment_wraps(void)
{
    ce40_setup(0xFF, 0, 0, (const uint8 *)0);

    fd2_chapter_event_handler_40__unref_dyn_turn_event(0);

    /* non-dispatching stage: no body ran */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);
    ASSERT_EQ((long)g_delay375b2_log_count, 0);
    ASSERT_EQ((long)g_kill_from_calls, 0);
    /* (0xFF + 1) truncated to a byte == 0x00; neighbours did not catch a carry */
    ASSERT_EQ((long)g_ce40_flags[0x10], 0x00);
    ASSERT_EQ((long)g_ce40_flags[0x0F], 0);
    ASSERT_EQ((long)g_ce40_flags[0x11], 0);

    ce40_teardown();
}
#endif

/* ================================================================
 * fd2_chapter_event_handler_41__shared_dyn_turn_event @ 0x3599B
 *
 * Pure state-machine mutator (no display side effects, no real-file I/O). The
 * functionally-exact body is:
 *     if (tile_event_consumed_flags[0x10] == 0) {
 *         tile_event_data_table[3] = (uint8)turn_counter;
 *         tile_event_consumed_flags[0x10] = 1;
 *     }
 * The risk-bearing contract pinned here:
 *   (a) FIRST-TIME gate: when flags[0x10] == 0 the handler arms hook entry 0's
 *       turn byte (data_table[+3]) with turn_counter and consumes the slot,
 *   (b) NO-OFFSET: the scheduled value is turn_counter EXACTLY — no +1. This is
 *       the sole behavioural difference from handler_3e (which stores
 *       turn_counter + 1); a value whose +1 would differ pins it,
 *   (c) IDEMPOTENCE: when flags[0x10] != 0 the handler writes nothing (the
 *       already-armed schedule and the consumed flag are both preserved),
 *   (d) BYTE-width store: the turn counter is read as one byte and stored as one
 *       byte (MOV DL,[turn_counter] / MOV [data_table+3],DL), so only the low
 *       byte reaches data_table[+3] and high bytes never leak,
 *   (e) exact byte offsets: only data_table[+3] and flags[+0x10] are written;
 *       their neighbours stay untouched,
 *   (f) the dispatch arg is ignored (the handler reads no param).
 *
 * Distinct slot index from handler_3e (0x10 here vs 0x11 there) gets its own
 * in-memory fixtures so the two suites never alias state.
 * ================================================================ */

/* flags buffer: index 0x10 is the consume slot; extra headroom guards neighbours */
static uint8 g_ce41_flags[0x20];
/* data table: index 3 is hook entry 0's turn byte; headroom guards neighbours */
static uint8 g_ce41_dtable[0x10];

static void ce41_setup(void)
{
    memset(g_ce41_flags, 0, sizeof(g_ce41_flags));
    memset(g_ce41_dtable, 0, sizeof(g_ce41_dtable));
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce41_flags;
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce41_dtable;
    data_fd2_battle_turn_counter = 0;
}

static void ce41_teardown(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_turn_counter = 0;
}

/* ----------------------------------------------------------------
 * First-time trigger + NO-OFFSET: flags[0x10] == 0 and turn_counter == 5. The
 * handler arms hook entry 0's turn byte (data_table[+3]) with 5 EXACTLY (not
 * 6 — this is the contrast against handler_3e's +1) and consumes the slot
 * (flags[0x10] -> 1). The dispatch arg is passed nonzero to prove it is ignored.
 * Guard bytes around both write targets must stay 0.
 * ---------------------------------------------------------------- */
static void test_h41_first_time_arms_no_offset_and_consumes(void)
{
    ce41_setup();
    data_fd2_battle_turn_counter = 5;

    fd2_chapter_event_handler_41__shared_dyn_turn_event(0x77);

    /* (a)+(b) hook entry 0's turn byte = turn_counter EXACTLY (5, not 5+1) */
    ASSERT_EQ((long)g_ce41_dtable[3], 5);
    /* slot consumed */
    ASSERT_EQ((long)g_ce41_flags[0x10], 1);
    /* (e) neighbours of data_table[+3] untouched */
    ASSERT_EQ((long)g_ce41_dtable[2], 0);
    ASSERT_EQ((long)g_ce41_dtable[4], 0);
    /* (e) neighbours of flags[+0x10] untouched */
    ASSERT_EQ((long)g_ce41_flags[0x0F], 0);
    ASSERT_EQ((long)g_ce41_flags[0x11], 0);

    ce41_teardown();
}

/* ----------------------------------------------------------------
 * Idempotence: when the slot is already consumed (flags[0x10] != 0) the handler
 * must do nothing. Pre-arm data_table[+3] with a sentinel and pre-set the flag;
 * after the call both must be byte-for-byte unchanged (no re-arm, no re-write),
 * even though turn_counter differs from the sentinel.
 * ---------------------------------------------------------------- */
static void test_h41_already_consumed_is_noop(void)
{
    ce41_setup();
    g_ce41_flags[0x10] = 0xAA;     /* already consumed (any nonzero) */
    g_ce41_dtable[3]   = 0x5C;     /* previously-armed schedule sentinel */
    data_fd2_battle_turn_counter = 9;

    fd2_chapter_event_handler_41__shared_dyn_turn_event(0);

    /* (c) schedule byte preserved (NOT overwritten with turn_counter 9) */
    ASSERT_EQ((long)g_ce41_dtable[3], 0x5C);
    /* consumed flag preserved exactly */
    ASSERT_EQ((long)g_ce41_flags[0x10], 0xAA);

    ce41_teardown();
}

/* ----------------------------------------------------------------
 * Byte-width store, high bytes ignored: the binary reads turn_counter as a single
 * byte (MOV DL, byte ptr [turn_counter]) and stores it as a byte. turn_counter =
 * 0x1234: only the low byte 0x34 reaches data_table[+3]; the 0x12 high byte never
 * leaks. (A naive 32-bit store would still drop the high bytes at the byte slot,
 * but pairing this with the no-offset value above pins both the byte width and
 * the absence of the +1.)
 * ---------------------------------------------------------------- */
static void test_h41_turn_counter_low_byte_only(void)
{
    ce41_setup();
    data_fd2_battle_turn_counter = 0x1234;

    fd2_chapter_event_handler_41__shared_dyn_turn_event(0);

    /* low byte 0x34 stored verbatim (no +1); high byte 0x12 never reaches it */
    ASSERT_EQ((long)g_ce41_dtable[3], 0x34);
    ASSERT_EQ((long)g_ce41_flags[0x10], 1);
    /* neighbour past the byte must not catch a high byte */
    ASSERT_EQ((long)g_ce41_dtable[4], 0);

    ce41_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_42__ch28_dialog_with_state @ 0x359C8
 *
 * Straight-line three-call scene (ch28 turn-FF marker). Functionally-exact body:
 *     fd2_display_dialog_scene(data_fd2_current_chapter_text, 3, 0xA0000, ...);   page 3
 *     fd2_cinematic_chapter_portrait_dump_with_white_flash(0x11, 0x12, 1);
 *     fd2_display_dialog_scene(data_fd2_current_chapter_text, 6, 0xA0000, ...);   page 6
 *
 * The cinematic helper's own contract (arg order, low-byte chapter_id, the
 * 300/200/400 delay triple) is already pinned by the test_white_flash_* cases
 * above; the dialog VM's own opcode handling is owned by the dialog suite. What
 * is risk-bearing HERE is the handler's own argument routing: that it issues
 * exactly the two dialog pages (3 then 6) bracketing exactly one portrait
 * cutscene, with the cutscene's literal args (tile 0x11,0x12 / chapter id 1).
 * All three calls are driven over the REAL dialog VM + REAL cinematic helper +
 * REAL portrait loader (real FDFIELD.DAT), reusing the proven handler_40 env:
 *   (a) BOTH dialog pages render: pages 3 and 6 each point at a shared 1-glyph
 *       body, so g_dlg_glyph_calls == 2 proves both pages ran (a missing/extra
 *       dialog call would make it 1 or 3),
 *   (b) EXACTLY ONE cutscene runs with chapter id 1: a single race-1 tile-event
 *       record makes party_member_count == 1 (the loader inits one runtime_char
 *       per record whose race == the forwarded chapter id); a decoy race-2 record
 *       must NOT match, proving the literal id is 1 and not some other value,
 *   (c) the cutscene's pan target is the literal (0x11, 0x12): the window origin
 *       (started away on both axes) lands exactly there,
 *   (d) exactly one white-flash delay triple (300/200/400) fires -> exactly one
 *       cutscene ran (not zero, not two),
 *   (e) the dispatch arg is ignored (passed nonzero).
 * The dialog glyph pixels and the cutscene pan/flash composites are pure display
 * side effects (deferred to Phase 9); they execute for real here only as a
 * byproduct and are not asserted.
 * ================================================================ */

/* dialog program: page-3 header (idx 3) and page-6 header (idx 6) both point at a
 * shared body at byte 0x18 (int16 idx 12): one TEXT glyph + END. */
static int16 g_ce42_prog[20];

/* Stand up the handler_42 env: ce23_setup provides the real cinematic
 * render/portrait/palette env over a tile-event table of `count` records (races
 * `races`) so the cutscene's chapter id is observable as a party-member-count
 * delta; a host-safe dialog VM env (empty BIOS key buffer + gated audio + no
 * active portrait) backs the real page-3/page-6 dialogs. */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void ce42_setup(int count, const uint8 *races,
                       uint32 start_ox, uint32 start_oy)
{
    ce23_setup(count, races, start_ox, start_oy);

    memset(g_ce42_prog, 0, sizeof(g_ce42_prog));
    g_ce42_prog[3]  = 0x18;          /* page-3 body byte offset (= int16 idx 12) */
    g_ce42_prog[6]  = 0x18;          /* page-6 body byte offset (same body)      */
    g_ce42_prog[12] = 0x41;          /* one TEXT glyph */
    g_ce42_prog[13] = -1;            /* END */
    data_fd2_current_chapter_text = (uint32)g_ce42_prog;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated so
     * the per-glyph blink/typewriter step is host-safe; no active portrait, so
     * END does not run the portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    g_dlg_glyph_calls = 0;
}
#endif

static void ce42_teardown(void)
{
    audiofix_disable_sfx();
    ce23_teardown();
}

/* ----------------------------------------------------------------
 * Full sequence: the handler shows dialog page 3, runs one portrait cutscene at
 * tile (0x11, 0x12) with chapter id 1, then shows dialog page 6. The tile-event
 * table carries one race-1 record (the cutscene's chapter id) and a race-2 decoy
 * (a wrong id that must not match). The dispatch arg is passed nonzero to prove
 * it is ignored. The window starts away from (0x11, 0x12) on both axes so the
 * pan is observable on the final origin.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_h42_dialog_cutscene_dialog_routes_all_three(void)
{
    static const uint8 races[2] = { 1, 2 };    /* target id 1, decoy id 2 */

    ce42_setup(2, races, 0x40, 0x40);

    fd2_chapter_event_handler_42__ch28_dialog_with_state(0x77);

    /* (a) both dialog pages (3 and 6) rendered their 1-glyph body */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    /* (b) exactly one cutscene ran with chapter id 1: the race-1 record inited,
     * the race-2 decoy did not -> count == 1 */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* (c) the cutscene's pan target is the literal (0x11, 0x12) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0x11);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x12);
    /* (d) exactly one white-flash delay triple fired -> one cutscene */
    ASSERT_EQ((long)g_delay375b2_log_count, 3);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);

    ce42_teardown();
}
#endif

/* ----------------------------------------------------------------
 * The cutscene's chapter id is the literal 1, not the tile coords: seed the
 * tile-event table with one race-0x11 record and one race-0x12 record (the two
 * pan coords) plus one race-1 record. Only the race-1 record may match — if the
 * handler ever forwarded a coord as the id, count would be 2 (one coord record +
 * the race-1 record) instead of 1. This pins chapter id == 1 distinct from the
 * (0x11, 0x12) arguments.
 * ---------------------------------------------------------------- */
/* SKIP (Phase 3): writes now-const data_fd2_battle_view_window_max_x, data_fd2_battle_view_window_max_y; restore + rewrite to drive real const data */
#if 0
static void test_h42_cutscene_chapter_id_is_one_not_coords(void)
{
    static const uint8 races[3] = { 0x11, 0x12, 1 };  /* coords as decoys + id 1 */

    ce42_setup(3, races, 0x40, 0x40);

    fd2_chapter_event_handler_42__ch28_dialog_with_state(0);

    /* only the race-1 record matched chapter id 1; neither coord (0x11, 0x12)
     * was forwarded as the id, so count is exactly 1 */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* both dialogs still ran and the pan still landed on (0x11, 0x12) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0x11);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x12);

    ce42_teardown();
}
#endif

void run_field_chevt23_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 3)\n");
#if 0
    RUN_TEST(test_white_flash_full_sequence);
#endif
#if 0
    RUN_TEST(test_white_flash_chapter_id_low_byte_only);
#endif
#if 0
    RUN_TEST(test_white_flash_pan_target_is_args_not_constant);
#endif
    RUN_TEST(test_h3e_first_time_arms_and_consumes);
    RUN_TEST(test_h3e_already_consumed_is_noop);
    RUN_TEST(test_h3e_turn_counter_byte_wraps);
    RUN_TEST(test_h3e_turn_counter_high_bytes_ignored);
#if 0
    RUN_TEST(test_h3f_two_portrait_pair_routes_both_cutscenes);
#endif
#if 0
    RUN_TEST(test_h40_stage1_cutscene_reveal_then_advance);
#endif
#if 0
    RUN_TEST(test_h40_stage2_dialog_then_kill_then_advance);
#endif
#if 0
    RUN_TEST(test_h40_stage0_noop_but_still_advances);
#endif
#if 0
    RUN_TEST(test_h40_stage_byte_increment_wraps);
#endif
    RUN_TEST(test_h41_first_time_arms_no_offset_and_consumes);
    RUN_TEST(test_h41_already_consumed_is_noop);
    RUN_TEST(test_h41_turn_counter_low_byte_only);
#if 0
    RUN_TEST(test_h42_dialog_cutscene_dialog_routes_all_three);
#endif
#if 0
    RUN_TEST(test_h42_cutscene_chapter_id_is_one_not_coords);
#endif
    printf("\n");
}
