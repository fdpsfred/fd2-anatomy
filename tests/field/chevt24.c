/*
 * unit tests for src/field/chevt2.c (part 4)
 *
 * fd2_chapter_event_handler_43__unref_dyn_turn_event @ 0x35A2F
 * fd2_chapter_event_handler_44__ch28_dialog_with_state @ 0x35A48
 * fd2_chapter_event_handler_45__ch28_dyn_turn_event   @ 0x35AB8
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
 * --- handler_45 ---
 * Pure state-machine mutator (no display side effects, no real-file I/O). The
 * functionally-exact body is a three-way AND gate guarding one armed write plus
 * a consume store:
 *     if (runtime_char_array[stepping_char_id].team != 0
 *         && tile_event_consumed_flags[0x11] == 0
 *         && tile_event_consumed_flags[0x12] != 0) {
 *         tile_event_data_table[9] = (uint8)turn_counter;
 *         tile_event_consumed_flags[0x11] = 1;
 *     }
 *
 * The risk-bearing contract pinned here:
 *   (a) THREE-WAY GATE: all three conditions must hold to fire. Each is pinned
 *       independently by a case that flips exactly one off and asserts NOTHING is
 *       written (data_table[+9] stays 0 and the consume flag stays 0):
 *         - team == 0 (an enemy steps instead of a non-enemy; encoding
 *           0=enemy 1=npc 2=player, so the team != 0 firing path is npc/player),
 *         - own slot flags[0x11] already consumed (!= 0),
 *         - PREREQ slot flags[0x12] not yet consumed (== 0) — the defining
 *           difference from handler_3e, which gates on its own slot only.
 *   (b) ARG IS THE CHAR INDEX: unlike the no-arg handlers in this family, the arg
 *       is the stepping char_id and indexes runtime_char_array by stride 0x50 at
 *       team (+6). A nonzero index with team set ONLY on that slot (and the team
 *       byte of a decoy slot left 0) pins both the stride and the +6 field.
 *   (c) OFFSET +9: the armed byte is hook entry 2's turn byte (data_table[+9]),
 *       not handler_41's +3 nor handler_43's +6; those slots stay untouched.
 *   (d) NO VALUE OFFSET: the scheduled value is turn_counter EXACTLY — no +1
 *       (contrast handler_3e); a value whose +1 would differ pins it.
 *   (e) BYTE-width store: the turn counter is read as one byte and stored as one
 *       byte (MOV DL,[turn_counter] / MOV [data_table+9],DL), so only the low
 *       byte reaches data_table[+9] and the high bytes never leak.
 *   (f) CONSUME store: when fired, flags[0x11] becomes 1 and only that byte
 *       (neighbours stay untouched).
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
#include "blitprob.h"   /* tg_install/restore_compositor_safe_atlases */

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
extern int    g_kill_from_calls;             /* kill-from-index recording stub (handler_47) */
extern uint32 g_kill_from_index[4];

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

/* ================================================================
 * fd2_chapter_event_handler_45__ch28_dyn_turn_event @ 0x35AB8
 * ================================================================ */

/* Own runtime_char array (stride 0x50) so the suite never aliases other parts'
 * char state; index 9 of the data table is hook entry 2's turn byte, with
 * headroom guarding the +3/+6/+8/+10 neighbours; consume-flags index 0x11 is
 * this handler's own slot and 0x12 the prerequisite slot. */
static runtime_char g_ce45_rc[8];
static uint8        g_ce45_dtable[0x10];
static uint8        g_ce45_consumed[0x20];

/* Arm all three gate conditions in the firing state, then let each test knock
 * one condition out. char index `idx` is the stepping char; its team is set
 * nonzero (non-enemy: npc/player) while the rest stay 0. */
static void ce45_setup(uint32 idx)
{
    memset(g_ce45_rc, 0, sizeof(g_ce45_rc));
    memset(g_ce45_dtable, 0, sizeof(g_ce45_dtable));
    memset(g_ce45_consumed, 0, sizeof(g_ce45_consumed));

    g_ce45_rc[idx].team = 1;                 /* non-enemy steps (team != 0)      */
    g_ce45_consumed[0x11] = 0;               /* own slot unconsumed              */
    g_ce45_consumed[0x12] = 1;               /* PREREQ slot already consumed     */
    data_fd2_battle_turn_counter = 0;

    data_fd2_battle_runtime_char_array_ptr = g_ce45_rc;
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce45_dtable;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce45_consumed;
}

static void ce45_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_battle_turn_counter = 0;
}

/* ----------------------------------------------------------------
 * All three gate conditions hold -> the handler arms hook entry 2's turn byte
 * (data_table[+9]) with turn_counter EXACTLY (7, not 7+1) and consumes its own
 * slot (flags[0x11] = 1). Pins (a) the fire path, (b) char-index stride (the
 * stepping char is index 3 and only its team is set), (c) offset +9 (the +3/+6
 * sibling slots and the immediate +8/+10 neighbours stay 0), (d) no +1, (f) the
 * consume store and its untouched neighbour. The prerequisite slot 0x12 must
 * remain set (the handler reads it, never clears it).
 * ---------------------------------------------------------------- */
static void test_h45_fires_when_all_conditions_met(void)
{
    ce45_setup(3);
    data_fd2_battle_turn_counter = 7;

    fd2_chapter_event_handler_45__ch28_dyn_turn_event(3);

    /* (c)+(d) hook entry 2's turn byte = turn_counter EXACTLY */
    ASSERT_EQ((long)g_ce45_dtable[9], 7);
    /* (c) sibling slots used by handler_41 (+3) and handler_43 (+6) untouched */
    ASSERT_EQ((long)g_ce45_dtable[3], 0);
    ASSERT_EQ((long)g_ce45_dtable[6], 0);
    /* (c) immediate neighbours of data_table[+9] untouched */
    ASSERT_EQ((long)g_ce45_dtable[8], 0);
    ASSERT_EQ((long)g_ce45_dtable[10], 0);
    /* (f) own slot consumed, exactly that byte */
    ASSERT_EQ((long)g_ce45_consumed[0x11], 1);
    ASSERT_EQ((long)g_ce45_consumed[0x10], 0);
    ASSERT_EQ((long)g_ce45_consumed[0x13], 0);
    /* prerequisite slot is read-only — still set, never cleared */
    ASSERT_EQ((long)g_ce45_consumed[0x12], 1);

    ce45_teardown();
}

/* ----------------------------------------------------------------
 * GATE 1 (team): an enemy steps. With the stepping char's team == 0 (enemy; the
 * firing path needs a non-enemy, team != 0) the gate fails first and NOTHING is
 * written — neither the armed byte nor the consume flag. Indexed by char 3 (only
 * the OTHER fixture state differs from the firing case), so this isolates the
 * team condition.
 * ---------------------------------------------------------------- */
static void test_h45_gated_off_when_enemy(void)
{
    ce45_setup(3);
    g_ce45_rc[3].team = 0;            /* enemy steps (team == 0) -> first gate fails */
    data_fd2_battle_turn_counter = 7;

    fd2_chapter_event_handler_45__ch28_dyn_turn_event(3);

    ASSERT_EQ((long)g_ce45_dtable[9], 0);       /* not armed */
    ASSERT_EQ((long)g_ce45_consumed[0x11], 0);  /* not consumed */

    ce45_teardown();
}

/* ----------------------------------------------------------------
 * GATE 2 (own slot): this slot is already consumed (flags[0x11] != 0). The
 * second gate fails so the handler does not re-arm and does not re-consume; the
 * armed byte stays 0 and flags[0x11] keeps its pre-set value. Pins the
 * idempotence of the own-slot consume.
 * ---------------------------------------------------------------- */
static void test_h45_gated_off_when_own_slot_consumed(void)
{
    ce45_setup(3);
    g_ce45_consumed[0x11] = 1;       /* already consumed -> second gate fails */
    data_fd2_battle_turn_counter = 7;

    fd2_chapter_event_handler_45__ch28_dyn_turn_event(3);

    ASSERT_EQ((long)g_ce45_dtable[9], 0);       /* not re-armed */
    ASSERT_EQ((long)g_ce45_consumed[0x11], 1);  /* unchanged (was already 1) */

    ce45_teardown();
}

/* ----------------------------------------------------------------
 * GATE 3 (prerequisite): the defining dependency. The prerequisite slot 0x12 is
 * NOT yet consumed (== 0), so even with a non-enemy stepping and the own slot
 * free, the third gate fails and nothing fires. This is what distinguishes
 * handler_45 from handler_3e (which has no prerequisite-slot check): with the
 * same non-enemy + free-own-slot setup, handler_3e would arm, but handler_45 must stay inert
 * until slot 0x12 is consumed elsewhere first.
 * ---------------------------------------------------------------- */
static void test_h45_gated_off_when_prereq_not_met(void)
{
    ce45_setup(3);
    g_ce45_consumed[0x12] = 0;       /* prereq NOT consumed -> third gate fails */
    data_fd2_battle_turn_counter = 7;

    fd2_chapter_event_handler_45__ch28_dyn_turn_event(3);

    ASSERT_EQ((long)g_ce45_dtable[9], 0);       /* not armed */
    ASSERT_EQ((long)g_ce45_consumed[0x11], 0);  /* not consumed */

    ce45_teardown();
}

/* ----------------------------------------------------------------
 * BYTE-width store + char-index isolation: turn_counter = 0x1234, so only the
 * low byte 0x34 may reach data_table[+9] (the binary reads turn_counter as a
 * single byte and stores a byte); the 0x12 high byte must never leak into the
 * neighbour. The stepping char is index 5 and its team is the only nonzero team
 * in the array — if the handler indexed the wrong slot (e.g. index 0, whose team
 * is 0) the first gate would fail and nothing would arm, so a successful armed
 * write also proves the char_id * 0x50 indexing.
 * ---------------------------------------------------------------- */
static void test_h45_turn_counter_low_byte_only(void)
{
    ce45_setup(5);
    data_fd2_battle_turn_counter = 0x1234;

    fd2_chapter_event_handler_45__ch28_dyn_turn_event(5);

    /* low byte 0x34 stored verbatim (no +1); high byte 0x12 never reaches it */
    ASSERT_EQ((long)g_ce45_dtable[9], 0x34);
    ASSERT_EQ((long)g_ce45_dtable[10], 0);      /* neighbour catches no high byte */
    /* the armed write happened -> index 5 (not 0) was used, slot consumed */
    ASSERT_EQ((long)g_ce45_consumed[0x11], 1);

    ce45_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_46__ch28_dialog_with_state @ 0x35B05
 *
 * Straight-line ch28 turn-FF marker scene. Functionally-exact body:
 *     fd2_set_combat_aux_block_byte_d_low4_for_char_range(0x29, 0x2D, 0);  disarm
 *     fd2_display_dialog_scene(current_chapter_text, 5, 0xA0000, ...);     page 5
 *     fd2_cinematic_chapter_portrait_dump_with_white_flash(8, 7, 3);
 *     fd2_cinematic_chapter_portrait_dump_with_white_flash(4, 7, 4);
 *     fd2_cinematic_chapter_portrait_dump_with_white_flash(0, 7, 5);
 *     fd2_display_dialog_scene(current_chapter_text, 6, 0xA0000, ...);     page 6
 *
 * The dialog VM opcode handling, the cinematic helper's own white-flash contract
 * and the AI-flag writer's range semantics are each pinned elsewhere (the dialog
 * suite, the chevt23 white-flash cases, the chevt21 handler_30 range cases). What
 * is risk-bearing HERE is this handler's own argument routing across the REAL
 * AI-flag writer + REAL dialog VM + REAL cinematic helper + REAL portrait loader
 * (real FDICON.B24 + FDFIELD.DAT):
 *   (a) AI DISARM: the low nibble of combat_aux_block[0xD] (abs offset 0x34) is
 *       written 0 for the inclusive range 0x29..0x2D (5 chars) with the high
 *       nibble preserved; the just-outside chars 0x28 and 0x2E stay untouched,
 *   (b) BOTH dialog pages render: pages 5 and 6 point at a shared 1-glyph body,
 *       so g_dlg_glyph_calls == 2 proves both pages ran,
 *   (c) THREE distinct cutscenes run, with chapter ids 3, 4, 5: the portrait
 *       loader increments party_member_count once per matching tile-event record
 *       and never resets it, so one record each for races 3/4/5 makes the final
 *       count == 3 (a dropped or merged cutscene would make it < 3; an id other
 *       than 3/4/5 would not match its record),
 *   (d) the THIRD cutscene (the one hosted in the borrowed alt_37 tail) actually
 *       runs and pans LAST to its literal tile (0, 7): the window origin (started
 *       away on both axes) lands exactly there,
 *   (e) exactly three white-flash delay triples (300/200/400) fire -> three
 *       cutscenes (9 logged ticks in the repeating 300,200,400 order),
 *   (f) the dispatch arg is ignored (passed nonzero).
 * The dialog glyph pixels and the cutscene pan/flash composites are pure display
 * side effects (deferred to Phase 9); they execute for real here only as a
 * byproduct and are not asserted. A single 0x50-entry runtime_char array backs
 * both the AI-flag range (chars 0x29..0x2D) and the loader's count-indexed spawn
 * slots (0,1,2) — the two regions do not overlap.
 * ================================================================ */

/* offset 0x34 of char `idx` (combat_aux_block[0xD]), read as a raw byte */
#define CE46_AI_OFF      0x34

static runtime_char g_ce46_rc[0x50];           /* must cover index 0x2D */
static uint8       *g_ce46_tileevent;
#define CE46_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ce46_ws[CE46_WS_SPAN];
static uint8 g_ce46_atlas[6 + 64 * 4 + 4];
static uint8 g_ce46_palette[256 * 3];
static int16 g_ce46_prog[20];

static uint8 ce46_ai(int idx)
{
    return ((uint8 *)&g_ce46_rc[idx])[CE46_AI_OFF];
}

/* Stand up the full real-cinematic + real-dialog + AI-flag env (mirrors the
 * proven handler_44 env). `count`/`races` drive the three cutscenes' chapter-id
 * observation; the window starts at (start_ox, start_oy) so the final pan target
 * is observable on the origin. Every char's combat_aux_block[0xD] is seeded 0xA5
 * (non-zero high nibble, non-zero low nibble) so the disarm-to-0 write and the
 * out-of-range preservation are both observable. */
static void ce46_setup(int count, const uint8 *races,
                       uint32 start_ox, uint32 start_oy)
{
    int i;
    uint32 *atlas_tbl;

    /* --- runtime_char array: AI nibbles seeded 0xA5 across the whole array --- */
    memset(g_ce46_rc, 0, sizeof(g_ce46_rc));
    for (i = 0; i < 0x50; i++) {
        ((uint8 *)&g_ce46_rc[i])[CE46_AI_OFF] = 0xA5;
    }
    data_fd2_battle_runtime_char_array_ptr = g_ce46_rc;

    /* --- portrait loader env --- */
    g_ce46_tileevent =
        (uint8 *)malloc((size_t)0x98 + (size_t)count * 0x1a + 0x20);
    memset(g_ce46_tileevent, 0, (size_t)0x98 + (size_t)count * 0x1a + 0x20);
    for (i = 0; i < count; i++) {
        g_ce46_tileevent[i * 0x1a + 0x98] = races[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce46_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = (uint32)count;
    chapter_portrait_load_buffer = 0;            /* loaded fresh by the loader  */
    data_fd2_chapter_init_phase_flag = 1;        /* spawn = field value verbatim */
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE    */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    /* --- render env for pan composites + final composite --- */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce46_ws - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_view_window_origin_x = start_ox;
    data_fd2_battle_view_window_origin_y = start_oy;
    data_fd2_battle_anim_phase = 1;
    atlas_tbl = (uint32 *)(g_ce46_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce46_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    /* --- 768-byte palette for the white-flash palette writes --- */
    for (i = 0; i < 256 * 3; i++) {
        g_ce46_palette[i] = 0x20;
    }
    data_fd2_vga_palette_data_ptr = (uint32)g_ce46_palette;

    /* --- dialog program + deterministic dialog VM env --- */
    memset(g_ce46_prog, 0, sizeof(g_ce46_prog));
    g_ce46_prog[5]  = 0x18;          /* page-5 body byte offset (= int16 idx 12) */
    g_ce46_prog[6]  = 0x18;          /* page-6 body byte offset (same body)      */
    g_ce46_prog[12] = 0x41;          /* one TEXT glyph */
    g_ce46_prog[13] = -1;            /* END */
    current_chapter_text = (uint32)g_ce46_prog;

    /* empty BIOS keyboard buffer + audio gated so the per-glyph blink/typewriter
     * step is host-safe; no active portrait, so END does not run the
     * portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    /* --- delay-tick log + composite counter + glyph counter --- */
    g_delay375b2_log_on = 1;
    g_delay375b2_log_count = 0;
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;

    /* the three cutscenes pan the camera -> real fd2_composite_battle_frame; its
     * per-char painter (party count reaches 3 over the real portrait cache),
     * shadow overlay and cursor pass feed the now-real RLE blitters. Override the
     * compositor's sprite sources with terminating all-SKIP atlases so every blit
     * is a deterministic no-op (the cutscene pan/flash composites are deferred
     * display side effect, not asserted here). Must run AFTER the env above sets
     * portrait_sprite_cache / runtime_battle_state_ptr. */
    tg_install_compositor_safe_atlases();
}

static void ce46_teardown(void)
{
    tg_restore_compositor_safe_atlases();
    audiofix_disable_sfx();
    free(g_ce46_tileevent);
    g_ce46_tileevent = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    g_delay375b2_log_on = 0;
    g_delay375b2_log_count = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * Full sequence: the handler disarms the AI flag for chars 0x29..0x2D, shows
 * dialog page 5, runs three portrait cutscenes (chapter ids 3, 4, 5 at tiles
 * (8,7)/(4,7)/(0,7)) and shows dialog page 6. The tile-event table carries one
 * record each for races 3, 4 and 5 so each cutscene matches exactly one record
 * and the running party count reaches 3. The window starts away from the final
 * tile (0, 7) on both axes so the last pan is observable. The dispatch arg is
 * passed nonzero to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h46_disarm_dialog_three_cutscenes_dialog(void)
{
    static const uint8 races[3] = { 3, 4, 5 };   /* one per cutscene chapter id */
    int i;

    ce46_setup(3, races, 0x40, 0x40);

    fd2_chapter_event_handler_46__ch28_dialog_with_state(0x77);

    /* (a) AI disarm: chars 0x29..0x2D got low nibble 0 (0xA5 -> 0xA0) */
    for (i = 0x29; i <= 0x2D; i++) {
        ASSERT_EQ((long)ce46_ai(i), 0xA0);
    }
    /* (a) just-outside chars stay 0xA5 (range is inclusive 0x29..0x2D only) */
    ASSERT_EQ((long)ce46_ai(0x28), 0xA5);
    ASSERT_EQ((long)ce46_ai(0x2E), 0xA5);
    /* (b) both dialog pages (5 and 6) rendered their 1-glyph body */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    /* (c) three cutscenes ran with distinct ids 3,4,5 -> count accumulated to 3 */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 3);
    /* (d) the final (3rd) cutscene panned LAST to the literal tile (0, 7) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 7);
    /* (e) three white-flash delay triples fired (9 ticks, repeating 300/200/400) */
    ASSERT_EQ((long)g_delay375b2_log_count, 9);
    for (i = 0; i < 3; i++) {
        ASSERT_EQ((long)g_delay375b2_log[i * 3 + 0], 300);
        ASSERT_EQ((long)g_delay375b2_log[i * 3 + 1], 200);
        ASSERT_EQ((long)g_delay375b2_log[i * 3 + 2], 400);
    }

    ce46_teardown();
}

/* ----------------------------------------------------------------
 * The three cutscene chapter ids are the literals 3, 4, 5 — not the pan coords
 * and not a single shared id. Seed the tile-event table with one record each for
 * races 3, 4, 5 plus decoy records carrying the pan coords (8, 7, 4, 0) as their
 * race. The count must reach EXACTLY 3: only the race-3/4/5 records may match
 * (one per cutscene); the coord decoys (and the duplicate-coord 4) must not add
 * spurious matches. A merged/dropped cutscene would make count < 3; a cutscene
 * that forwarded a coord as its id would over-count. This pins all three ids
 * distinct from each other and from the (x, y) arguments.
 * ---------------------------------------------------------------- */
static void test_h46_three_chapter_ids_are_3_4_5_not_coords(void)
{
    /* races 3,4,5 (the ids, one match each) + coord decoys 8,7,0 that must not
     * match any forwarded id (4 already present as an id record above) */
    static const uint8 races[6] = { 3, 4, 5, 8, 7, 0 };

    ce46_setup(6, races, 0x40, 0x40);

    fd2_chapter_event_handler_46__ch28_dialog_with_state(0);

    /* exactly the race-3, race-4 and race-5 records matched (one per cutscene);
     * the coord decoys 8/7/0 matched nothing, so count is exactly 3 */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 3);
    /* both dialogs still ran and the final pan still landed on (0, 7) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 7);

    ce46_teardown();
}

/* ----------------------------------------------------------------
 * The THIRD cutscene (hosted in the borrowed alt_37 tail of handler_42) actually
 * executes and pans to its own literal target (0, 7), distinct from the first
 * two cutscenes' tiles. Start the window on the SECOND cutscene's tile (4, 7):
 * if the borrowed-tail 3rd call were dropped the origin would stay at (4, 7);
 * landing on (0, 7) proves the 3rd cutscene ran and forwarded its own (0, 7)
 * args. The full triple of cutscenes still logs 9 delay ticks. The race never
 * matches any id so the loader stays a host-safe no-op, keeping the focus on the
 * pan target of the tail-hosted call.
 * ---------------------------------------------------------------- */
static void test_h46_third_cutscene_runs_and_pans_to_zero_seven(void)
{
    static const uint8 races[1] = { 0x7F };      /* never equals id 3, 4 or 5 */

    ce46_setup(1, races, 4, 7);                   /* start ON the 2nd tile (4,7) */

    fd2_chapter_event_handler_46__ch28_dialog_with_state(0x33);

    /* origin moved off (4, 7) onto the 3rd cutscene's literal target (0, 7):
     * the tail-hosted 3rd call ran and forwarded its own coords */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 7);
    /* all three cutscenes fired -> 9 delay ticks (3 x 300/200/400) */
    ASSERT_EQ((long)g_delay375b2_log_count, 9);
    /* no record matched any id -> loader scan was a no-op every time */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    /* both dialog pages still rendered */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);

    ce46_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_47__unref_dyn_turn_event @ 0x35B6B
 *
 * Priming state-machine mutator: gate the dialog + mass-kill behind the byte
 * counter tile_event_consumed_flags[0x13], then UNCONDITIONALLY advance that
 * byte. Functionally-exact body:
 *     if (tile_event_consumed_flags[0x13] != 0) {
 *         fd2_display_dialog_scene(current_chapter_text, 2, 0xA0000, ...);   page 2
 *         fd2_kill_runtime_chars_from_index_to_end(0x14);                    kill
 *     }
 *     tile_event_consumed_flags[0x13]++;                                     advance
 *
 * The kill callee (0x35BBA, routing target battle/btl_turn.c) is not yet emitted,
 * so it is the recording stub in testglob.c (g_kill_from_calls / g_kill_from_index);
 * its own HP-zeroing loop + death animation is the callee's behavior, covered when
 * 0x35BBA is emitted into btl_turn.c. The page-2 dialog runs the REAL dialog VM
 * over an in-memory int16 program (NOT a game file): the page-2 header points at a
 * 1-glyph + END body, the glyph blitter is the testglob recorder
 * (g_dlg_glyph_calls), an empty BIOS keyboard buffer keeps blink_flag set, and
 * audiofix gates the per-glyph blink path host-safely. The dialog glyph pixels and
 * the kill's HP-zeroing are pure display / callee side effects (deferred to
 * Phase 9); the glyph recorder is used only to prove the page-2 body ran.
 *
 * The risk-bearing control flow pinned here:
 *   (a) PRIMING gate: flags[0x13] == 0 (first invocation) runs NEITHER the dialog
 *       NOR the kill; flags[0x13] != 0 (2nd+) runs BOTH,
 *   (b) FIRE path: dialog page 2 (1-glyph body -> g_dlg_glyph_calls == 1) followed
 *       by exactly one kill from the literal start index 0x14,
 *   (c) PAGE literal 2: a 1-glyph body wired ONLY to the page-2 header (page 5,
 *       handler_35's page, left empty) renders iff page 2 was selected,
 *   (d) KILL index literal 0x14: distinct from handler_35's 0x12 and handler_40's
 *       0x10; the dispatch arg must not leak in,
 *   (e) UNCONDITIONAL advance: both the gated-off and the fired paths increment
 *       flags[0x13]; the increment is a BYTE INC (0xFF wraps to 0x00, no carry),
 *   (f) the dispatch arg is ignored (passed nonzero),
 *   (g) only flags[0x13] is read/written; its neighbours stay untouched.
 *
 * Own in-memory fixture so the suite never aliases the other chevt2 part suites'
 * state.
 * ================================================================ */

/* flags buffer: index 0x13 is the gate/counter byte; headroom guards neighbours.
 * dialog program: the page-2 header (idx 2) points at a 1-glyph + END body at
 * byte 0x10 (= int16 idx 8); the page-5 header (idx 5) is left 0 so a page-5
 * selection would render nothing. */
static uint8 g_ce47_flags[0x20];
static int16 g_ce47_prog[12];

/* Stand up the handler_47 env: gate byte at flags[0x13] = `gate`; the page-2
 * dialog runs over an in-memory program whose body is `glyphs` TEXT opcodes + END
 * with the host-safe dialog VM env; the kill recorder is reset so the forwarded
 * start index is observable. */
static void ce47_setup(uint8 gate, int glyphs)
{
    int i;

    memset(g_ce47_flags, 0, sizeof(g_ce47_flags));
    g_ce47_flags[0x13] = gate;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce47_flags;

    memset(g_ce47_prog, 0, sizeof(g_ce47_prog));
    g_ce47_prog[2] = 0x10;                /* page-2 body byte offset (= int16 idx 8) */
    for (i = 0; i < glyphs; i++) {
        g_ce47_prog[8 + i] = 0x41;        /* TEXT glyph */
    }
    g_ce47_prog[8 + glyphs] = -1;         /* END */
    current_chapter_text = (uint32)g_ce47_prog;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated so
     * the per-glyph blink/typewriter step is host-safe. No active portrait, so
     * END does not run the portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    g_dlg_glyph_calls = 0;
    g_kill_from_calls = 0;
    g_kill_from_index[0] = 0xDEAD;        /* sentinel: overwritten iff kill issued */
}

static void ce47_teardown(void)
{
    audiofix_disable_sfx();
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    current_chapter_text = 0;
}

/* ----------------------------------------------------------------
 * PRIMING (first invocation): flags[0x13] == 0 -> NEITHER the dialog NOR the kill
 * runs; the counter advances 0 -> 1. A 1-glyph page-2 body is wired so that if the
 * dialog erroneously ran it would record a glyph; it must stay 0. The dispatch arg
 * is passed nonzero to prove it is ignored. Only flags[0x13] changes; neighbours
 * stay 0.
 * ---------------------------------------------------------------- */
static void test_h47_priming_no_dialog_no_kill_advance(void)
{
    ce47_setup(0, 1);

    fd2_chapter_event_handler_47__unref_dyn_turn_event(0x77);

    /* (a) gated off: no dialog body ran, no kill issued */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);
    ASSERT_EQ((long)g_kill_from_calls, 0);
    /* (e) the counter still advanced 0 -> 1 (0x77 arg did not leak in) */
    ASSERT_EQ((long)g_ce47_flags[0x13], 1);
    /* (g) neighbours untouched */
    ASSERT_EQ((long)g_ce47_flags[0x12], 0);
    ASSERT_EQ((long)g_ce47_flags[0x14], 0);

    ce47_teardown();
}

/* ----------------------------------------------------------------
 * FIRE path (2nd+ invocation): flags[0x13] already non-zero -> dialog page 2 runs
 * (1-glyph body -> g_dlg_glyph_calls == 1) AND exactly one kill from the literal
 * start index 0x14 is issued, then the counter advances. Pins the page-2 selection
 * (the page-5 header is empty, so a page-5 read would render nothing) and the
 * literal kill index 0x14 (distinct from handler_35's 0x12). Pre-set gate = 1.
 * ---------------------------------------------------------------- */
static void test_h47_fire_dialog_page2_then_kill_from_0x14(void)
{
    ce47_setup(1, 1);

    fd2_chapter_event_handler_47__unref_dyn_turn_event(0);

    /* (b)+(c) page-2 dialog body ran (rendered the single glyph) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    /* (b)+(d) exactly one kill, with the literal start index 0x14 */
    ASSERT_EQ((long)g_kill_from_calls, 1);
    ASSERT_EQ((long)g_kill_from_index[0], 0x14);
    /* (e) the counter advanced 1 -> 2; neighbours untouched */
    ASSERT_EQ((long)g_ce47_flags[0x13], 2);
    ASSERT_EQ((long)g_ce47_flags[0x12], 0);
    ASSERT_EQ((long)g_ce47_flags[0x14], 0);

    ce47_teardown();
}

/* ----------------------------------------------------------------
 * The kill start index is the literal 0x14 regardless of the dispatch arg, and any
 * non-zero gate value (not just 1) takes the fire path: the binary gate is
 * CMP byte ptr,0 / JZ, so a gate of 0x5C still fires. Drive with a non-zero,
 * non-0x14 arg (0x55) and an empty dialog body (immediate END) so the kill is the
 * sole rendered effect; the forwarded start index must stay 0x14 and exactly one
 * kill must be issued.
 * ---------------------------------------------------------------- */
static void test_h47_fire_kill_index_literal_ignores_arg(void)
{
    ce47_setup(0x5C, 0);              /* non-1 non-zero gate, 0 glyphs (immediate END) */

    fd2_chapter_event_handler_47__unref_dyn_turn_event(0x55);

    /* fired (gate 0x5C != 0): dialog entered but rendered nothing (immediate END) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);
    /* still exactly one kill from 0x14 — the 0x55 arg did not leak through */
    ASSERT_EQ((long)g_kill_from_calls, 1);
    ASSERT_EQ((long)g_kill_from_index[0], 0x14);
    /* counter advanced 0x5C -> 0x5D */
    ASSERT_EQ((long)g_ce47_flags[0x13], 0x5D);

    ce47_teardown();
}

/* ----------------------------------------------------------------
 * The advance is a BYTE increment (binary INC byte ptr), not a wider add: a gate
 * byte of 0xFF takes the fire path (non-zero) and then wraps to 0x00. The dialog
 * body is empty so only the wrap is observable on the counter; the neighbour bytes
 * must stay 0 (the increment must not carry past the byte).
 * ---------------------------------------------------------------- */
static void test_h47_advance_byte_increment_wraps(void)
{
    ce47_setup(0xFF, 0);

    fd2_chapter_event_handler_47__unref_dyn_turn_event(0);

    /* fired (0xFF != 0): empty body, but the kill still issued once from 0x14 */
    ASSERT_EQ((long)g_kill_from_calls, 1);
    ASSERT_EQ((long)g_kill_from_index[0], 0x14);
    /* (e) (0xFF + 1) truncated to a byte == 0x00; neighbours caught no carry */
    ASSERT_EQ((long)g_ce47_flags[0x13], 0x00);
    ASSERT_EQ((long)g_ce47_flags[0x12], 0);
    ASSERT_EQ((long)g_ce47_flags[0x14], 0);

    ce47_teardown();
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
    RUN_TEST(test_h45_fires_when_all_conditions_met);
    RUN_TEST(test_h45_gated_off_when_enemy);
    RUN_TEST(test_h45_gated_off_when_own_slot_consumed);
    RUN_TEST(test_h45_gated_off_when_prereq_not_met);
    RUN_TEST(test_h45_turn_counter_low_byte_only);
    RUN_TEST(test_h46_disarm_dialog_three_cutscenes_dialog);
    RUN_TEST(test_h46_three_chapter_ids_are_3_4_5_not_coords);
    RUN_TEST(test_h46_third_cutscene_runs_and_pans_to_zero_seven);
    RUN_TEST(test_h47_priming_no_dialog_no_kill_advance);
    RUN_TEST(test_h47_fire_dialog_page2_then_kill_from_0x14);
    RUN_TEST(test_h47_fire_kill_index_literal_ignores_arg);
    RUN_TEST(test_h47_advance_byte_increment_wraps);
    printf("\n");
}
