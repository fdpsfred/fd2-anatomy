/*
 * unit tests for src/field/chevt2.c (part 4)
 *
 * fd2_chapter_event_handler_43__unref_dyn_turn_event @ 0x35A2F
 * fd2_chapter_event_handler_44__ch28_dialog_with_state @ 0x35A48
 *
 * --- handler_43 ---
 * Pure state-machine mutator (no display side effects, no real-file I/O). The
 * functionally-exact body is a single unconditional byte store:
 *     tile_event_data_table[6] = (uint8)turn_counter;
 *
 * The risk-bearing contract pinned here:
 *   (a) NO GATE: unlike handler_41 (which only arms when flags[0x10] == 0 and
 *       then consumes the slot), this handler has no consume-flag check at all —
 *       it writes data_table[+6] every single call. Calling it twice in a row
 *       with two different turn_counter values must leave the SECOND value (the
 *       slot is overwritten, never locked out). This is the defining behavioural
 *       difference and pins the absence of any CMP/JNZ gate in the binary.
 *   (b) OFFSET +6: the armed byte is hook entry 1's turn byte (data_table[+6]),
 *       not handler_41's entry-0 byte (+3); the +3 slot must stay untouched.
 *   (c) NO VALUE OFFSET: the scheduled value is turn_counter EXACTLY — no +1
 *       (contrast handler_3e, which stores turn_counter + 1); a value whose +1
 *       would differ pins it.
 *   (d) BYTE-width store: the turn counter is read as one byte and stored as one
 *       byte (MOV DL,[turn_counter] / MOV [data_table+6],DL), so only the low
 *       byte reaches data_table[+6] and the high bytes never leak.
 *   (e) exact byte offset: only data_table[+6] is written; its neighbours stay
 *       untouched.
 *   (f) the dispatch arg is ignored (the handler reads no param).
 *
 * --- handler_44 ---
 * Straight-line three-call ch28 turn-FF marker scene followed by one state
 * mutation. Functionally-exact body:
 *     fd2_display_dialog_scene(current_chapter_text, 4, 0xA0000, ...);   page 4
 *     fd2_cinematic_chapter_portrait_dump_with_white_flash(0xE, 7, 2);
 *     fd2_display_dialog_scene(current_chapter_text, 6, 0xA0000, ...);   page 6
 *     tile_event_consumed_flags[0x12] = 1;                               consume
 *
 * The cinematic helper's own contract and the dialog VM opcode handling are
 * pinned elsewhere (chevt23 white-flash cases + the dialog suite); what is
 * risk-bearing HERE is this handler's own argument routing and its lone state
 * mutation — driven over the REAL dialog VM + REAL cinematic helper + REAL
 * portrait loader (real FDFIELD.DAT):
 *   (a) BOTH dialog pages render: pages 4 and 6 each point at a shared 1-glyph
 *       body, so g_dlg_glyph_calls == 2 proves both pages ran (a missing/extra
 *       dialog call would make it 1 or 3),
 *   (b) EXACTLY ONE cutscene runs with chapter id 2: a single race-2 tile-event
 *       record makes party_member_count == 1 (the loader inits one runtime_char
 *       per record whose race == the forwarded chapter id); a decoy record with a
 *       different race must NOT match, proving the literal id is 2,
 *   (c) the cutscene's pan target is the literal (0xE, 7): the window origin
 *       (started away on both axes) lands exactly there,
 *   (d) exactly one white-flash delay triple (300/200/400) fires -> one cutscene,
 *   (e) CONSUME store: tile_event_consumed_flags[0x12] becomes 1 and only that
 *       byte (neighbours +0x11 and +0x13 stay untouched),
 *   (f) the consume store is UNCONDITIONAL (no gate): with the slot pre-set to a
 *       sentinel it still ends at 1 (the binary has no CMP/JNZ before the store),
 *   (g) the dispatch arg is ignored (passed nonzero).
 * The dialog glyph pixels and the cutscene pan/flash composites are pure display
 * side effects (deferred to Phase 9); they execute for real here only as a
 * byproduct and are not asserted.
 *
 * Each handler keeps its own in-memory fixture so the suite never aliases the
 * other chevt2 part suites' state.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx (handler_44 dialog) */

/* ================================================================
 * fd2_chapter_event_handler_43__unref_dyn_turn_event @ 0x35A2F
 * ================================================================ */

/* data table: index 6 is hook entry 1's turn byte; headroom guards neighbours */
static uint8 g_ce43_dtable[0x10];

static void ce43_setup(void)
{
    memset(g_ce43_dtable, 0, sizeof(g_ce43_dtable));
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce43_dtable;
    data_fd2_battle_turn_counter = 0;
}

static void ce43_teardown(void)
{
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_turn_counter = 0;
}

/* ----------------------------------------------------------------
 * Unconditional arm + NO VALUE OFFSET: turn_counter == 5. The handler writes
 * hook entry 1's turn byte (data_table[+6]) with 5 EXACTLY (not 6 — the contrast
 * against handler_3e's +1). The dispatch arg is passed nonzero to prove it is
 * ignored. Guard bytes around the write target (and handler_41's +3 slot) must
 * stay 0.
 * ---------------------------------------------------------------- */
static void test_h43_writes_turn_counter_at_offset_6(void)
{
    ce43_setup();
    data_fd2_battle_turn_counter = 5;

    fd2_chapter_event_handler_43__unref_dyn_turn_event(0x77);

    /* (b)+(c) hook entry 1's turn byte = turn_counter EXACTLY (5, not 5+1) */
    ASSERT_EQ((long)g_ce43_dtable[6], 5);
    /* (b) handler_41's +3 slot must NOT be the one written */
    ASSERT_EQ((long)g_ce43_dtable[3], 0);
    /* (e) immediate neighbours of data_table[+6] untouched */
    ASSERT_EQ((long)g_ce43_dtable[5], 0);
    ASSERT_EQ((long)g_ce43_dtable[7], 0);

    ce43_teardown();
}

/* ----------------------------------------------------------------
 * NO GATE / repeatability: there is no consume flag, so a second call overwrites
 * the slot. Pre-arm data_table[+6] with a sentinel, then call with a different
 * turn_counter; the slot must take the NEW value (not be preserved like the
 * consumed-out handler_41). Then call again with a third value to prove every
 * call keeps overwriting.
 * ---------------------------------------------------------------- */
static void test_h43_no_gate_overwrites_every_call(void)
{
    ce43_setup();
    g_ce43_dtable[6] = 0x5C;       /* stale previously-armed value */
    data_fd2_battle_turn_counter = 9;

    fd2_chapter_event_handler_43__unref_dyn_turn_event(0);
    /* (a) overwritten with the new turn_counter, NOT preserved */
    ASSERT_EQ((long)g_ce43_dtable[6], 9);

    /* second call with a fresh counter keeps overwriting (no lock-out) */
    data_fd2_battle_turn_counter = 0x21;
    fd2_chapter_event_handler_43__unref_dyn_turn_event(0);
    ASSERT_EQ((long)g_ce43_dtable[6], 0x21);

    ce43_teardown();
}

/* ----------------------------------------------------------------
 * Byte-width store, high bytes ignored: the binary reads turn_counter as a single
 * byte (MOV DL, byte ptr [turn_counter]) and stores it as a byte. turn_counter =
 * 0x1234: only the low byte 0x34 reaches data_table[+6]; the 0x12 high byte never
 * leaks into the neighbour. Pairing the low byte with the no-offset value pins
 * both the byte width and the absence of the +1.
 * ---------------------------------------------------------------- */
static void test_h43_turn_counter_low_byte_only(void)
{
    ce43_setup();
    data_fd2_battle_turn_counter = 0x1234;

    fd2_chapter_event_handler_43__unref_dyn_turn_event(0);

    /* low byte 0x34 stored verbatim (no +1); high byte 0x12 never reaches it */
    ASSERT_EQ((long)g_ce43_dtable[6], 0x34);
    /* neighbour past the byte must not catch a high byte */
    ASSERT_EQ((long)g_ce43_dtable[7], 0);

    ce43_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_44__ch28_dialog_with_state @ 0x35A48
 * ================================================================ */

extern runtime_char g_test_rc_array[8];

/* testglob recorders (shared real dialog VM + cinematic instrumentation) */
extern int    g_delay375b2_log_on;
extern int    g_delay375b2_log_count;
extern uint32 g_delay375b2_log[16];
extern int    g_dlg_glyph_calls;             /* real dialog VM glyph recorder */
extern int    g_composite_call_count;

/* ---- portrait-loader fixture: a tile-event table of `count` records
 * (stride 0x1A) whose race bytes (+0x98) are races[k]; alloc_offset = count
 * drives the scan length. Chapter 4 -> the real loader re-reads real
 * FDFIELD.DAT[4*3+2 = 0xE]. */
static uint8 *g_ce44_tileevent;

/* ---- host-safe render workspace + sprite atlas + 768-byte palette for the real
 * pan composites, the two white-flash palette writes, and the final composite. */
#define CE44_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ce44_ws[CE44_WS_SPAN];
static uint8 g_ce44_atlas[6 + 64 * 4 + 4];
static uint8 g_ce44_palette[256 * 3];

/* ---- consume-flags buffer: index 0x12 is this handler's slot; headroom
 * guards neighbours so the byte-exact store is observable. */
static uint8 g_ce44_consumed[0x20];

/* dialog program: page-4 header (idx 4) and page-6 header (idx 6) both point at a
 * shared body at byte 0x18 (int16 idx 12): one TEXT glyph + END. */
static int16 g_ce44_prog[20];

/* Stand up the full real-cinematic + real-dialog env (mirrors the proven
 * handler_42 env). `count`/`races` drive the cutscene's chapter-id observation;
 * the window origin starts at (start_ox, start_oy) so the pan target is
 * observable on the final origin; the consume-flags buffer is owned here. */
static void ce44_setup(int count, const uint8 *races,
                       uint32 start_ox, uint32 start_oy)
{
    int i;
    uint32 *atlas_tbl;

    /* --- portrait loader env --- */
    g_ce44_tileevent =
        (uint8 *)malloc((size_t)0x98 + (size_t)count * 0x1a + 0x20);
    memset(g_ce44_tileevent, 0, (size_t)0x98 + (size_t)count * 0x1a + 0x20);
    for (i = 0; i < count; i++) {
        g_ce44_tileevent[i * 0x1a + 0x98] = races[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce44_tileevent;
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
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce44_ws - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_view_window_origin_x = start_ox;
    data_fd2_battle_view_window_origin_y = start_oy;
    data_fd2_battle_anim_phase = 1;
    atlas_tbl = (uint32 *)(g_ce44_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce44_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    /* --- 768-byte palette for the two fd2_set_vga_palette_range_with_add calls -- */
    for (i = 0; i < 256 * 3; i++) {
        g_ce44_palette[i] = 0x20;
    }
    data_fd2_vga_palette_data_ptr = (uint32)g_ce44_palette;

    /* --- dialog program + deterministic dialog VM env --- */
    memset(g_ce44_prog, 0, sizeof(g_ce44_prog));
    g_ce44_prog[4]  = 0x18;          /* page-4 body byte offset (= int16 idx 12) */
    g_ce44_prog[6]  = 0x18;          /* page-6 body byte offset (same body)      */
    g_ce44_prog[12] = 0x41;          /* one TEXT glyph */
    g_ce44_prog[13] = -1;            /* END */
    current_chapter_text = (uint32)g_ce44_prog;

    /* empty BIOS keyboard buffer + audio gated so the per-glyph blink/typewriter
     * step is host-safe; no active portrait, so END does not run the
     * portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    /* --- consume-flags buffer (cleared) --- */
    memset(g_ce44_consumed, 0, sizeof(g_ce44_consumed));
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce44_consumed;

    /* --- delay-tick log + composite counter + glyph counter --- */
    g_delay375b2_log_on = 1;
    g_delay375b2_log_count = 0;
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;
}

static void ce44_teardown(void)
{
    audiofix_disable_sfx();
    free(g_ce44_tileevent);
    g_ce44_tileevent = 0;
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
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    g_delay375b2_log_on = 0;
    g_delay375b2_log_count = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * Full sequence: the handler shows dialog page 4, runs one portrait cutscene at
 * tile (0xE, 7) with chapter id 2, shows dialog page 6, then consumes the slot
 * (tile_event_consumed_flags[0x12] = 1). The tile-event table carries one race-2
 * record (the cutscene's chapter id) and a race-1 decoy (a wrong id that must not
 * match). The dispatch arg is passed nonzero to prove it is ignored. The window
 * starts away from (0xE, 7) on both axes so the pan is observable.
 * ---------------------------------------------------------------- */
static void test_h44_dialog_cutscene_dialog_then_consume(void)
{
    static const uint8 races[2] = { 2, 1 };    /* target id 2, decoy id 1 */

    ce44_setup(2, races, 0x40, 0x40);
    /* guard bytes around the consume slot start clear */
    g_ce44_consumed[0x11] = 0;
    g_ce44_consumed[0x13] = 0;

    fd2_chapter_event_handler_44__ch28_dialog_with_state(0x77);

    /* (a) both dialog pages (4 and 6) rendered their 1-glyph body */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    /* (b) exactly one cutscene ran with chapter id 2: the race-2 record inited,
     * the race-1 decoy did not -> count == 1 */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* (c) the cutscene's pan target is the literal (0xE, 7) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0xE);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 7);
    /* (d) exactly one white-flash delay triple fired -> one cutscene */
    ASSERT_EQ((long)g_delay375b2_log_count, 3);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);
    /* (e) the consume store hit exactly slot 0x12 and nothing else */
    ASSERT_EQ((long)g_ce44_consumed[0x12], 1);
    ASSERT_EQ((long)g_ce44_consumed[0x11], 0);
    ASSERT_EQ((long)g_ce44_consumed[0x13], 0);

    ce44_teardown();
}

/* ----------------------------------------------------------------
 * The cutscene's chapter id is the literal 2, not the tile coords: seed the
 * tile-event table with one race-0xE record and one race-7 record (the two pan
 * coords) plus one race-2 record. Only the race-2 record may match — if the
 * handler ever forwarded a coord as the id, count would be 2 (one coord record +
 * the race-2 record) instead of 1. This pins chapter id == 2 distinct from the
 * (0xE, 7) arguments. The consume store still fires.
 * ---------------------------------------------------------------- */
static void test_h44_cutscene_chapter_id_is_two_not_coords(void)
{
    static const uint8 races[3] = { 0xE, 7, 2 };  /* coords as decoys + id 2 */

    ce44_setup(3, races, 0x40, 0x40);

    fd2_chapter_event_handler_44__ch28_dialog_with_state(0);

    /* only the race-2 record matched chapter id 2; neither coord (0xE, 7) was
     * forwarded as the id, so count is exactly 1 */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* both dialogs still ran, the pan still landed on (0xE, 7), slot consumed */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0xE);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 7);
    ASSERT_EQ((long)g_ce44_consumed[0x12], 1);

    ce44_teardown();
}

/* ----------------------------------------------------------------
 * The consume store is UNCONDITIONAL: the binary writes
 * tile_event_consumed_flags[0x12] = 1 with no CMP/JNZ gate (contrast the gated
 * handlers 3e/41, which only write when their slot reads 0). Pre-set slot 0x12 to
 * a non-1 sentinel; after the call it must equal 1 (the store always fires and
 * always writes the immediate 1, never preserving the prior value). The race
 * never matches any id so the cutscene loader stays a no-op, keeping the focus on
 * the store itself. The dispatch arg is ignored.
 * ---------------------------------------------------------------- */
static void test_h44_consume_store_is_unconditional(void)
{
    static const uint8 races[1] = { 0x7F };    /* never equals chapter id 2 */

    ce44_setup(1, races, 0x20, 0x20);
    g_ce44_consumed[0x12] = 0x5C;              /* stale sentinel, NOT 1 */

    fd2_chapter_event_handler_44__ch28_dialog_with_state(0x33);

    /* the store always fires and always writes the immediate 1 */
    ASSERT_EQ((long)g_ce44_consumed[0x12], 1);
    /* both dialogs still ran (the consume path does not gate them out) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    /* chapter id 2 matched no record (race 0x7F) -> loader scan was a no-op */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);

    ce44_teardown();
}

void run_field_chevt24_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 4)\n");
    RUN_TEST(test_h43_writes_turn_counter_at_offset_6);
    RUN_TEST(test_h43_no_gate_overwrites_every_call);
    RUN_TEST(test_h43_turn_counter_low_byte_only);
    RUN_TEST(test_h44_dialog_cutscene_dialog_then_consume);
    RUN_TEST(test_h44_cutscene_chapter_id_is_two_not_coords);
    RUN_TEST(test_h44_consume_store_is_unconditional);
    printf("\n");
}
