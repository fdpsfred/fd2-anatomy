/*
 * unit tests for src/field/chevt2.c
 *
 * fd2_chapter_event_handler_2f__ch21_turn_gated is a ch21 turn-gated cinematic
 * orchestrator (dispatch idx 0x2F @ table 0x51B91). Its only computed logic is
 *   (a) the portrait-set index = (int)data_fd2_battle_turn_counter / 2  (a SIGNED
 *       divide-by-2; asm SAR idiom), forwarded to the real portrait loader, and
 *   (b) the dialog gate `if (turn_counter == 2)` which decides whether the real
 *       dialog VM (fd2_display_dialog_scene, page 3) runs.
 * Everything else (the 4-corner camera sweep via fd2_pan_cursor_and_window +
 * fd2_wait_n_bios_ticks, and the dialog glyph pixels) is pure display side
 * effect and is deferred to Phase 9 integration.
 *
 * These tests drive the REAL handler and its REAL callees against the staged
 * real game files (the portrait loader re-reads FDFIELD.DAT and rewrites the
 * FD2.TMP swap file). Host-safety recipe follows the established suites:
 *   - portrait loader scan length is the in-process global
 *     data_fd2_resource_portrait_cache_alloc_offset over an in-process
 *     tile-event table (rsrc.c setup_pt_fixture); alloc_offset 0 makes the scan
 *     a no-op (only re-reads FDFIELD + writes FD2.TMP),
 *   - fd2_pan_cursor_and_window / fd2_composite_battle_frame are exercised for
 *     real; the tile-map blit (the composite's first stage) is the testglob
 *     recording stub (g_composite_call_count),
 *   - fd2_wait_n_bios_ticks is REAL and paces ~55ms per tick against the
 *     host-advancing BIOS tick word at 0x46C (the handler waits 1+8*4 ticks),
 *   - the dialog gate, when taken, runs the real dialog VM over an in-memory
 *     int16 program (NOT a game file): page-3 header -> a 1-glyph + END body.
 *     The glyph blitter is the testglob recorder (g_dlg_glyph_calls); audio is
 *     gated through audiofix so the per-glyph blink/typewriter path is host-safe.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx */

extern runtime_char g_test_rc_array[8];

/* testglob recorders */
extern int g_composite_call_count;
extern int g_dlg_glyph_calls;

/* ---- shared portrait-loader fixture (mirrors rsrc.c setup_pt_fixture) ----
 * Build a tile-event table of `count` records (stride 0x1A); record k has its
 * race byte (+0x98) = race_of[k]. alloc_offset = count drives the scan length.
 * Chapter 4 -> the real loader re-reads real FDFIELD.DAT[4*3+2 = 0xE]. */
static uint8 *g_ce_tileevent;

static void ce_setup_portrait_env(int count, const uint8 *race_of)
{
    int i;

    g_ce_tileevent = (uint8 *)malloc((size_t)0x98 + (size_t)count * 0x1a + 0x20);
    memset(g_ce_tileevent, 0, (size_t)0x98 + (size_t)count * 0x1a + 0x20);
    for (i = 0; i < count; i++) {
        g_ce_tileevent[i * 0x1a + 0x98] = race_of[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = (uint32)count;

    chapter_portrait_load_buffer = 0;           /* loaded fresh by the loader   */
    data_fd2_chapter_init_phase_flag = 1;        /* spawn = field value verbatim */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE    */

    /* the real fd2_load_portrait_to_cache (reached via the real
     * fd2_init_runtime_char_for_battle for matching races) parses real
     * FDICON.B24; reset the cache so it first-inits cleanly. */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
}

static void ce_teardown_portrait_env(void)
{
    free(g_ce_tileevent);
    g_ce_tileevent = 0;
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
    current_chapter_text = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * Gate NOT taken (turn_counter != 2): the 4-corner camera sweep runs and the
 * window origin lands on the LAST swept corner (0, 0x20). The dialog VM is NOT
 * entered. turn_counter = 4 is a real ch21 trigger turn; alloc_offset = 0 makes
 * the portrait scan a host-safe no-op (still re-reads FDFIELD + writes FD2.TMP).
 * ---------------------------------------------------------------- */
static void test_gate_skips_dialog_on_non_trigger_turn(void)
{
    ce_setup_portrait_env(0, (const uint8 *)0);
    data_fd2_battle_turn_counter = 4;

    /* start the window away from every sweep target */
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;
    current_chapter_text = 0;          /* gate must not deref this */
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_2f__ch21_turn_gated(0);

    /* sweep ended on the 4th corner (0, 0x20) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x20);
    /* camera sweep composited frames */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* gate (turn != 2) skipped the dialog VM entirely */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce_teardown_portrait_env();
}

/* ----------------------------------------------------------------
 * Portrait index == (int)turn_counter / 2 (SIGNED /2). turn_counter = 6 must
 * select race index 3. The tile-event table holds a decoy record race=2 (the
 * value that an off-by-one or wrong-shift would pick) and a target record
 * race=3; only the race=3 record matches, so the real
 * fd2_init_runtime_char_for_battle runs exactly once (party_member_count 0->1).
 * gate not taken (turn != 2).
 * ---------------------------------------------------------------- */
static void test_portrait_index_is_turn_div_2(void)
{
    static const uint8 races[2] = { 0x02, 0x03 };  /* decoy 2, target 3 */

    ce_setup_portrait_env(2, races);
    data_fd2_battle_turn_counter = 6;              /* (int)6/2 == 3 */
    current_chapter_text = 0;

    fd2_chapter_event_handler_2f__ch21_turn_gated(0);

    /* exactly the race==3 record matched -> one char inited */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);  /* loader freed+nulled */

    ce_teardown_portrait_env();
}

/* ----------------------------------------------------------------
 * Gate TAKEN (turn_counter == 2): the handler runs the real dialog VM with
 * page_idx 3. An in-memory int16 program whose page-3 header points to a
 * 1-glyph + END body proves the gate fired (g_dlg_glyph_calls == 1). The empty
 * keyboard buffer keeps blink_flag set so the typewriter/blink path is taken;
 * audiofix gates that path host-safely. alloc_offset = 0 keeps the portrait
 * load a no-op.
 * ---------------------------------------------------------------- */
static void test_gate_fires_dialog_on_turn_2(void)
{
    static int16 prog[8];

    ce_setup_portrait_env(0, (const uint8 *)0);
    data_fd2_battle_turn_counter = 2;             /* gate taken */

    /* page-3 header word (index 3 -> byte offset 6) -> opcode body at byte 8
     * (= int16 index 4). Body: one glyph then END. */
    memset(prog, 0, sizeof(prog));
    prog[3] = 8;        /* byte offset of the page-3 opcode body */
    prog[4] = 0x41;     /* TEXT glyph */
    prog[5] = -1;       /* END */
    current_chapter_text = (uint32)prog;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated so
     * the per-glyph blink/typewriter step is host-safe. No active portrait, so
     * END does not run the portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_2f__ch21_turn_gated(0);

    /* the gate fired: the dialog VM rendered the single glyph */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);

    ce_teardown_portrait_env();
}

/* ================================================================
 * fd2_chapter_event_handler_30__ch21_ai_ctrl @ 0x351C6
 *
 * Pure in-memory computation: arms AI control flag 3 (the low nibble of
 * runtime_char.combat_aux_block[0xD], absolute offset 0x34) for two inclusive
 * char ranges 0x23..0x2A and 0x43..0x4A. The second range's call shares a
 * borrowed tail in the binary; these tests confirm BOTH ranges are written.
 *
 * Drives the REAL handler and its REAL callee
 * (fd2_set_combat_aux_block_byte_d_low4_for_char_range) against a local
 * 0x50-entry runtime_char array (the shared g_test_rc_array[8] is too small for
 * index 0x4A). No game files, no display.
 * ================================================================ */

#define CE30_NCHARS      0x50       /* must cover the highest index 0x4A */
#define CE30_AI_OFF      0x34       /* combat_aux_block[0xD] absolute offset */

static runtime_char g_ce30_rc[CE30_NCHARS];

/* offset 0x34 of char `idx`, read as raw byte */
static uint8 ce30_ai(int idx)
{
    return ((uint8 *)&g_ce30_rc[idx])[CE30_AI_OFF];
}

static void ce30_setup(void)
{
    int i;

    memset(g_ce30_rc, 0, sizeof(g_ce30_rc));
    /* Seed offset 0x34 of every char with a non-zero HIGH nibble (0xA0) and a
     * non-3 LOW nibble (0x05) so we can prove: (a) in-range chars get their low
     * nibble rewritten to 3 with the high nibble preserved -> 0xA3, and
     * (b) out-of-range chars keep 0xA5 untouched. */
    for (i = 0; i < CE30_NCHARS; i++) {
        ((uint8 *)&g_ce30_rc[i])[CE30_AI_OFF] = 0xA5;
    }
    data_fd2_battle_runtime_char_array_ptr = g_ce30_rc;
}

static void ce30_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}

/* ----------------------------------------------------------------
 * Both ranges are armed: every char in 0x23..0x2A and 0x43..0x4A gets its low
 * nibble set to 3 (high nibble 0xA preserved -> 0xA3). Exercises the borrowed
 * tail (second call) by asserting the 0x43..0x4A range too.
 * ---------------------------------------------------------------- */
static void test_h30_both_ranges_armed(void)
{
    int i;

    ce30_setup();

    fd2_chapter_event_handler_30__ch21_ai_ctrl(0);

    for (i = 0x23; i <= 0x2A; i++) {
        ASSERT_EQ((long)ce30_ai(i), 0xA3);
    }
    for (i = 0x43; i <= 0x4A; i++) {
        ASSERT_EQ((long)ce30_ai(i), 0xA3);
    }

    ce30_teardown();
}

/* ----------------------------------------------------------------
 * Range boundaries are exact and inclusive: the chars just outside each range
 * (0x22, 0x2B, 0x42, 0x4B) and the whole gap 0x2B..0x42 stay 0xA5. This guards
 * against off-by-one bounds and against the second call being dropped (which
 * would leave 0x43..0x4A == 0xA5).
 * ---------------------------------------------------------------- */
static void test_h30_boundaries_and_gap_untouched(void)
{
    int i;

    ce30_setup();

    fd2_chapter_event_handler_30__ch21_ai_ctrl(0);

    /* just-before / just-after each inclusive range */
    ASSERT_EQ((long)ce30_ai(0x22), 0xA5);
    ASSERT_EQ((long)ce30_ai(0x2B), 0xA5);
    ASSERT_EQ((long)ce30_ai(0x42), 0xA5);
    ASSERT_EQ((long)ce30_ai(0x4B), 0xA5);

    /* the entire gap between the two ranges is untouched */
    for (i = 0x2B; i <= 0x42; i++) {
        ASSERT_EQ((long)ce30_ai(i), 0xA5);
    }
    /* char 0 (well below) is also untouched */
    ASSERT_EQ((long)ce30_ai(0), 0xA5);

    ce30_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_31__ch22_turn_gated @ 0x351E9
 *
 * Same turn-gated cinematic shape as handler_2f but for chapter 22: the
 * portrait index is the SAME signed (int)turn_counter/2, the pan sweep is only
 * 2 corners across row y=0x23 (x=0x20 then x=0), and the dialog gate fires on
 * turn_counter == 3 with dialog page 1 (vs handler_2f: turn==2, page 3). Reuses
 * the ce_setup_portrait_env / ce_teardown_portrait_env fixtures above.
 * ================================================================ */

/* ----------------------------------------------------------------
 * Gate NOT taken (turn_counter != 3): the 2-corner sweep runs and the window
 * origin lands on the LAST swept corner (0, 0x23). The dialog VM is NOT
 * entered. turn_counter = 7 is a real ch22 trigger turn; alloc_offset = 0 makes
 * the portrait scan a host-safe no-op (still re-reads FDFIELD + writes FD2.TMP).
 * ---------------------------------------------------------------- */
static void test_h31_gate_skips_dialog_on_non_trigger_turn(void)
{
    ce_setup_portrait_env(0, (const uint8 *)0);
    data_fd2_battle_turn_counter = 7;

    /* start the window away from both sweep targets */
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;
    current_chapter_text = 0;          /* gate must not deref this */
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_31__ch22_turn_gated(0);

    /* sweep ended on the 2nd (last) corner (0, 0x23) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x23);
    /* camera sweep composited frames */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* gate (turn != 3) skipped the dialog VM entirely */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce_teardown_portrait_env();
}

/* ----------------------------------------------------------------
 * Portrait index == (int)turn_counter / 2 (SIGNED /2). turn_counter = 8 must
 * select race index 4. The tile-event table holds a decoy record race=3 (the
 * value a wrong-shift / off-by-one would pick) and a target record race=4; only
 * the race=4 record matches, so the real fd2_init_runtime_char_for_battle runs
 * exactly once (party_member_count 0->1). Gate not taken (turn != 3).
 * ---------------------------------------------------------------- */
static void test_h31_portrait_index_is_turn_div_2(void)
{
    static const uint8 races[2] = { 0x03, 0x04 };  /* decoy 3, target 4 */

    ce_setup_portrait_env(2, races);
    data_fd2_battle_turn_counter = 8;              /* (int)8/2 == 4 */
    current_chapter_text = 0;

    fd2_chapter_event_handler_31__ch22_turn_gated(0);

    /* exactly the race==4 record matched -> one char inited */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);  /* loader freed+nulled */

    ce_teardown_portrait_env();
}

/* ----------------------------------------------------------------
 * Gate TAKEN (turn_counter == 3): the handler runs the real dialog VM with
 * page_idx 1. An in-memory int16 program whose page-1 header points to a
 * 1-glyph + END body proves the gate fired (g_dlg_glyph_calls == 1). The empty
 * keyboard buffer keeps blink_flag set so the typewriter/blink path is taken;
 * audiofix gates that path host-safely. alloc_offset = 0 keeps the portrait
 * load a no-op.
 * ---------------------------------------------------------------- */
static void test_h31_gate_fires_dialog_on_turn_3(void)
{
    static int16 prog[8];

    ce_setup_portrait_env(0, (const uint8 *)0);
    data_fd2_battle_turn_counter = 3;             /* gate taken */

    /* page-1 header word (index 1 -> byte offset 2) -> opcode body at byte 8
     * (= int16 index 4). Body: one glyph then END. */
    memset(prog, 0, sizeof(prog));
    prog[1] = 8;        /* byte offset of the page-1 opcode body */
    prog[4] = 0x41;     /* TEXT glyph */
    prog[5] = -1;       /* END */
    current_chapter_text = (uint32)prog;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated so
     * the per-glyph blink/typewriter step is host-safe. No active portrait, so
     * END does not run the portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_31__ch22_turn_gated(0);

    /* the gate fired: the dialog VM rendered the single glyph */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);

    ce_teardown_portrait_env();
}

/* ================================================================
 * fd2_chapter_event_handler_32__ch22_reinforcement @ 0x35261
 *
 * Straight-line reinforcement spawner (no turn gate, no computed logic): load
 * portrait set 2, a single-corner pan to window origin (0x10, 0x2A), an 8-tick
 * hold, spawn reinforcement char id 0x14, then UNCONDITIONALLY show dialog
 * page 2. The dialog call is reached via a borrowed tail (JMP 0x347F1) in the
 * binary; these tests prove that tail is correctly inlined here by checking the
 * dialog VM fires with page 2 and that the single pan corner is applied. Reuses
 * the ce_setup_portrait_env / ce_teardown_portrait_env fixtures above.
 * ================================================================ */

/* ----------------------------------------------------------------
 * The single-corner pan lands the window origin on exactly (0x10, 0x2A), and
 * the dialog tail (borrowed via JMP 0x347F1) fires unconditionally — there is
 * no turn gate. An in-memory int16 program whose page-2 header points to a
 * 1-glyph + END body proves the page-2 dialog body ran (g_dlg_glyph_calls == 1).
 * The empty keyboard buffer keeps blink_flag set so the typewriter/blink path
 * is taken; audiofix gates that path host-safely. alloc_offset = 0 keeps the
 * portrait load a host-safe no-op (still re-reads FDFIELD + writes FD2.TMP).
 * ---------------------------------------------------------------- */
static void test_h32_pan_corner_and_unconditional_page2_dialog(void)
{
    static int16 prog[8];

    ce_setup_portrait_env(0, (const uint8 *)0);

    /* start the window away from the pan target so the move is observable */
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;

    /* page-2 header word (index 2 -> byte offset 4) -> opcode body at byte 8
     * (= int16 index 4). Body: one glyph then END. */
    memset(prog, 0, sizeof(prog));
    prog[2] = 8;        /* byte offset of the page-2 opcode body */
    prog[4] = 0x41;     /* TEXT glyph */
    prog[5] = -1;       /* END */
    current_chapter_text = (uint32)prog;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated so
     * the per-glyph blink/typewriter step is host-safe. No active portrait, so
     * END does not run the portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_32__ch22_reinforcement(0);

    /* single pan corner applied */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0x10);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x2A);
    /* pan composited a frame */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* the borrowed tail fired the page-2 dialog VM (rendered the single glyph) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);

    ce_teardown_portrait_env();
}

/* ================================================================
 * fd2_chapter_event_handler_33__unref_drop @ 0x3529A
 *
 * Straight-line battle-drop + dialog (no turn gate, no computed logic): copy a
 * fixed inline 3-byte drop entry {type=0 ITEM, value=0x65 -> item id 101} into a
 * local, run it through the REAL fd2_process_battle_drop_entries with recipient =
 * the dispatch arg (stepping char id) and count = 1, then UNCONDITIONALLY show
 * dialog page 3. The dialog is reached via a borrowed tail (JMP 0x34FB7) in the
 * binary; this test proves that tail is correctly inlined here.
 *
 * The drop entry's ITEM path (type 0) only renders when the recipient is on the
 * player team (bTeam==2); that branch drives the real item-pickup dialog +
 * portrait load + VGA (0xA0000) and is deferred to Phase 9 integration — the
 * same deferral the battle/btl_turn.c drop-processor tests apply. This test
 * pins the deterministic, display-free contract:
 *   (a) recipient routing + ITEM-type team gate: the entry is dispatched to
 *       recipient_idx == the handler arg, and with that recipient on a NON-player
 *       team the drop processor early-returns BEFORE fd2_add_item_to_inventory,
 *       so the recipient's pre-seeded empty slot stays untouched, and
 *   (b) the handler's own unconditional page-3 dialog still fires afterwards
 *       (proves the borrowed dialog tail with page_idx 3).
 * The page-3 dialog runs the REAL dialog VM over an in-memory int16 program (NOT
 * a game file): page-3 header -> a 1-glyph + END body; the glyph blitter is the
 * testglob recorder (g_dlg_glyph_calls); the empty BIOS keyboard buffer keeps
 * blink_flag set and audiofix gates the per-glyph blink path host-safely.
 * ================================================================ */
static void test_h33_item_drop_gated_off_then_page3_dialog(void)
{
    static int16 prog[8];

    /* recipient = char 1 on a NON-player team -> the type-0 ITEM drop hits the
     * team gate and early-returns before any item dialog / portrait / VGA. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    g_test_rc_array[1].team = 0;                     /* enemy team -> gate fails */
    g_test_rc_array[1].inventory_slots[0] = 0x80;    /* slot 0 empty            */
    g_test_rc_array[1].inventory_slots[1] = 0xEE;    /* sentinel item id        */
    data_fd2_battle_party_member_count = 4;

    /* page-3 header word (index 3 -> byte offset 6) -> opcode body at byte 8
     * (= int16 index 4). Body: one glyph then END. */
    memset(prog, 0, sizeof(prog));
    prog[3] = 8;        /* byte offset of the page-3 opcode body */
    prog[4] = 0x41;     /* TEXT glyph */
    prog[5] = -1;       /* END */
    current_chapter_text = (uint32)prog;

    /* deterministic dialog VM env: empty BIOS keyboard buffer + audio gated so
     * the per-glyph blink/typewriter step is host-safe. No active portrait, so
     * END does not run the portrait-close path. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_33__unref_drop(1);

    /* (a) ITEM drop gated off: recipient's slot 0 untouched, sentinel intact
     * (a reached add-item would stamp flag=0 + item id 0x65). This also proves
     * the entry's type byte is the ITEM type (0): a type-2 entry would instead
     * dispatch the chapter-event handler table, and the recipient is the handler
     * arg (1), not 0. */
    ASSERT_EQ(g_test_rc_array[1].inventory_slots[0], 0x80);
    ASSERT_EQ(g_test_rc_array[1].inventory_slots[1], 0xEE);
    /* (b) the borrowed tail fired the page-3 dialog VM (rendered the single
     * glyph) unconditionally after the drop. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);

    /* minimal teardown */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    current_chapter_text = 0;
}

/* ================================================================
 * fd2_chapter_event_handler_34__ch23_ai_ctrl @ 0x352E2
 *
 * Straight-line ch23 turn cinematic (no turn gate, no dialog): two paired
 * portrait white-flash cutscenes at fixed tile positions (2, 0xB) and
 * (0x1A, 0xB). The only computed logic is the portrait id, derived from the turn
 * counter in 8-bit (AL) arithmetic:
 *   id0 = (uint8)((uint8)turn_counter - 0x0E) * 2          (first portrait)
 *   id1 = id0 + 1 (also 8-bit truncated)                   (second portrait)
 * The second call is reached via a borrowed tail (fall-through into the
 * CALL;ADD ESP;RET tail of fd2_wrap_... @ 0x35318) in the binary; these tests
 * prove that tail is correctly inlined here as a complete second call.
 *
 * The real portrait-flash cinematic is pure display side effect (deferred to
 * Phase 9); these tests drive the REAL handler against the recording stub
 * fd2_cinematic_chapter_portrait_dump_with_white_flash (testglob.c), which
 * captures each call's (x, y, id). No game files, no display.
 * ================================================================ */

/* testglob recorders for the portrait-flash cinematic stub */
extern int    g_portrait_flash_calls;
extern uint32 g_portrait_flash_x[4];
extern uint32 g_portrait_flash_y[4];
extern uint32 g_portrait_flash_id[4];

static void h34_reset_flash_log(void)
{
    int i;
    g_portrait_flash_calls = 0;
    for (i = 0; i < 4; i++) {
        g_portrait_flash_x[i] = 0;
        g_portrait_flash_y[i] = 0;
        g_portrait_flash_id[i] = 0;
    }
}

/* ----------------------------------------------------------------
 * A real ch23 trigger turn (22) maps to save_metadata_block 0x11 -> portrait
 * pair 6/7. With turn_counter = 22 (0x16): (0x16 - 0x0E) * 2 = 16 ... wait, the
 * id math is on the turn counter byte directly: ((uint8)22 - 0x0E) * 2 = (8)*2 =
 * 16 (0x10) for the first, 17 (0x11) for the second. Two calls are made, at the
 * fixed positions (2, 0xB) then (0x1A, 0xB), proving BOTH calls run (the second
 * via the borrowed tail) and the +1 odd/even pairing.
 * ---------------------------------------------------------------- */
static void test_h34_two_portrait_flashes_with_paired_ids(void)
{
    h34_reset_flash_log();
    data_fd2_battle_turn_counter = 22;          /* real ch23 trigger turn      */

    fd2_chapter_event_handler_34__ch23_ai_ctrl(0);

    /* exactly two paired portrait flashes */
    ASSERT_EQ((long)g_portrait_flash_calls, 2);

    /* first flash: fixed pos (2, 0xB), id = (22-0x0E)*2 = 16 */
    ASSERT_EQ((long)g_portrait_flash_x[0], 2);
    ASSERT_EQ((long)g_portrait_flash_y[0], 0xB);
    ASSERT_EQ((long)g_portrait_flash_id[0], 16);

    /* second flash (borrowed tail): fixed pos (0x1A, 0xB), id = 16 + 1 = 17 */
    ASSERT_EQ((long)g_portrait_flash_x[1], 0x1A);
    ASSERT_EQ((long)g_portrait_flash_y[1], 0xB);
    ASSERT_EQ((long)g_portrait_flash_id[1], 17);
}

/* ----------------------------------------------------------------
 * Portrait-id pairs track the turn counter exactly as documented: when the
 * counter advances 0xE -> 0xF -> 0x10 -> 0x11 the pair advances 0/1 -> 2/3 ->
 * 4/5 -> 6/7 (= ((turn-0xE)*2) and +1). Pins the per-call arithmetic at the
 * lowest documented counter value 0xE (-> pair 0/1).
 * ---------------------------------------------------------------- */
static void test_h34_portrait_id_pair_tracks_counter(void)
{
    h34_reset_flash_log();
    data_fd2_battle_turn_counter = 0x0E;        /* (0x0E-0x0E)*2 = 0 -> pair 0/1 */

    fd2_chapter_event_handler_34__ch23_ai_ctrl(0);

    ASSERT_EQ((long)g_portrait_flash_calls, 2);
    ASSERT_EQ((long)g_portrait_flash_id[0], 0);
    ASSERT_EQ((long)g_portrait_flash_id[1], 1);
}

/* ----------------------------------------------------------------
 * The id is computed in 8-bit (AL) arithmetic and TRUNCATED to a byte, NOT a
 * 32-bit int. turn_counter = 0 drives a borrow: ((uint8)0 - 0x0E) = 0xF2 (242),
 * *2 = 0x1E4 -> low byte 0xE4 (228) for the first portrait, 0xE5 (229) for the
 * second. A naive 32-bit `((int)turn - 0x0E) * 2` would instead yield -28 / -27,
 * so this case guards the byte-wrap (MOVZX EAX,AL) semantics. The upper bytes of
 * the turn counter must also be ignored: a high garbage byte in the dword must
 * not leak into the id.
 * ---------------------------------------------------------------- */
static void test_h34_portrait_id_is_8bit_truncated(void)
{
    h34_reset_flash_log();
    /* 0xFF00 -> low byte 0x00; the high byte must be masked off (MOV AL,[...]) */
    data_fd2_battle_turn_counter = 0xFF00;

    fd2_chapter_event_handler_34__ch23_ai_ctrl(0);

    ASSERT_EQ((long)g_portrait_flash_calls, 2);
    /* (0x00 - 0x0E) * 2 = -28 -> (uint8) = 0xE4 = 228; +1 = 0xE5 = 229 */
    ASSERT_EQ((long)g_portrait_flash_id[0], 0xE4);
    ASSERT_EQ((long)g_portrait_flash_id[1], 0xE5);
    /* positions are unaffected by the id arithmetic */
    ASSERT_EQ((long)g_portrait_flash_x[0], 2);
    ASSERT_EQ((long)g_portrait_flash_x[1], 0x1A);
}

/* ================================================================
 * fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash @ 0x35318
 *
 * Transparent forwarding thunk: passes its 3 args straight through to
 * fd2_cinematic_chapter_portrait_dump_with_white_flash and returns. No own
 * frame, no computed logic — it exists only as the borrowed cleanup tail that
 * fd2_chapter_event_handler_3f__ch27_ai_ctrl tail-JMPs into after pushing its 3
 * args. The functionally-exact contract is "call the target exactly once with
 * the 3 args unchanged, in order".
 *
 * Drives the REAL wrapper against the recording stub
 * fd2_cinematic_chapter_portrait_dump_with_white_flash (testglob.c, reused from
 * the handler_34 suite), which captures each call's (x, y, id). No game files,
 * no display.
 * ================================================================ */

/* ----------------------------------------------------------------
 * The wrapper forwards all 3 args verbatim and in order, exactly once. Uses
 * three distinct, non-equal values so any argument swap / drop / duplication
 * would change the recorded tuple. Includes the live caller's own arg triple
 * (0xF, 0x1B, 2) as the second case to pin the real ch27 usage.
 * ---------------------------------------------------------------- */
static void test_wrap_forwards_three_args_in_order(void)
{
    h34_reset_flash_log();

    /* distinct values: x != y != id, none zero, so order is observable */
    fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash(0x11, 0x22, 0x33);

    ASSERT_EQ((long)g_portrait_flash_calls, 1);
    ASSERT_EQ((long)g_portrait_flash_x[0], 0x11);
    ASSERT_EQ((long)g_portrait_flash_y[0], 0x22);
    ASSERT_EQ((long)g_portrait_flash_id[0], 0x33);

    /* the exact arg triple the live caller (handler_3f, ch27) tail-JMPs with */
    h34_reset_flash_log();
    fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash(0xF, 0x1B, 2);

    ASSERT_EQ((long)g_portrait_flash_calls, 1);
    ASSERT_EQ((long)g_portrait_flash_x[0], 0xF);
    ASSERT_EQ((long)g_portrait_flash_y[0], 0x1B);
    ASSERT_EQ((long)g_portrait_flash_id[0], 2);
}

/* ================================================================
 * fd2_chapter_event_handler_35__unref_dialog_with_state @ 0x35321
 *
 * Straight-line dialog + mass-kill (no turn gate, no computed logic): show
 * dialog page 5, then kill every runtime_char_array slot from index 0x12 to the
 * end via fd2_kill_runtime_chars_from_index_to_end(0x12). A Class-3 shared tail
 * at 0x35354 lets fd2_chapter_event_handler_53 borrow this handler's kill-call
 * cleanup tail; these tests pin the functionally-exact contract of THIS handler
 * (dialog page 5, then kill-from index 0x12).
 *
 * The kill callee (0x35BBA, routing target battle/btl_turn.c) is not yet
 * emitted, so it is the recording stub in testglob.c (g_kill_from_index /
 * g_kill_from_calls). The kill's own HP-zeroing loop + death animation is the
 * callee's behavior and is covered when 0x35BBA is emitted into btl_turn.c;
 * here we verify only that THIS handler issues exactly one kill with the literal
 * start index 0x12.
 *
 * The page-5 dialog runs the REAL dialog VM over an in-memory int16 program
 * (NOT a game file): page-5 header -> a 1-glyph + END body; the glyph blitter
 * is the testglob recorder (g_dlg_glyph_calls); the empty BIOS keyboard buffer
 * keeps blink_flag set and audiofix gates the per-glyph blink path host-safely.
 * ================================================================ */

/* recording stub log for the still-unemitted kill callee (testglob.c) */
extern int    g_kill_from_calls;
extern uint32 g_kill_from_index[4];

/* Drive the page-5 dialog over an in-memory program whose body is `glyphs`
 * TEXT opcodes followed by END, with the host-safe dialog VM env. The kill
 * recorder is reset so the caller can assert the forwarded start index. */
static int16 g_ce35_prog[12];

static void ce35_setup(int glyphs)
{
    int i;

    /* page-5 header word (int16 index 5) holds the byte offset of the page-5
     * body; put the body at byte 0x10 (= int16 index 8), clear of the 6 page
     * header words (indices 0..5). */
    memset(g_ce35_prog, 0, sizeof(g_ce35_prog));
    g_ce35_prog[5] = 0x10;
    for (i = 0; i < glyphs; i++) {
        g_ce35_prog[8 + i] = 0x41;        /* TEXT glyph */
    }
    g_ce35_prog[8 + glyphs] = -1;         /* END */
    current_chapter_text = (uint32)g_ce35_prog;

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
    g_kill_from_index[0] = 0xDEAD;        /* sentinel: overwritten iff called */
}

static void ce35_teardown(void)
{
    current_chapter_text = 0;
}

/* ----------------------------------------------------------------
 * Full handler contract: the page-5 dialog VM runs for real (g_dlg_glyph_calls
 * == 1 proves the page-5 body executed), and the handler then issues exactly
 * one mass-kill with the literal start index 0x12. A 1-glyph page-5 program
 * exercises the dialog body; the kill recorder captures the forwarded index.
 * ---------------------------------------------------------------- */
static void test_h35_dialog_page5_then_kill_from_0x12(void)
{
    ce35_setup(1);

    fd2_chapter_event_handler_35__unref_dialog_with_state(0);

    /* dialog page-5 body ran (rendered the single glyph) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    /* exactly one kill, with the literal start index 0x12 */
    ASSERT_EQ((long)g_kill_from_calls, 1);
    ASSERT_EQ((long)g_kill_from_index[0], 0x12);

    ce35_teardown();
}

/* ----------------------------------------------------------------
 * The kill start index is the literal 0x12 regardless of the dispatch arg: the
 * handler ignores its arg (it is the table's tile-step ABI placeholder) and
 * always kills from 0x12. Driving the handler with a non-zero, non-0x12 arg
 * (0x55) must NOT change the forwarded start index, guarding against the arg
 * leaking into the kill call. The dialog body is empty (immediate END) so the
 * kill is the sole effect under test.
 * ---------------------------------------------------------------- */
static void test_h35_kill_index_is_literal_ignores_arg(void)
{
    ce35_setup(0);                         /* 0 glyphs: immediate END */

    fd2_chapter_event_handler_35__unref_dialog_with_state(0x55);

    /* dialog entered but rendered nothing (immediate END) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);
    /* still exactly one kill from 0x12 — the 0x55 arg did not leak through */
    ASSERT_EQ((long)g_kill_from_calls, 1);
    ASSERT_EQ((long)g_kill_from_index[0], 0x12);

    ce35_teardown();
}

void run_field_chevt2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2\n");
    RUN_TEST(test_gate_skips_dialog_on_non_trigger_turn);
    RUN_TEST(test_portrait_index_is_turn_div_2);
    RUN_TEST(test_gate_fires_dialog_on_turn_2);
    RUN_TEST(test_h30_both_ranges_armed);
    RUN_TEST(test_h30_boundaries_and_gap_untouched);
    RUN_TEST(test_h31_gate_skips_dialog_on_non_trigger_turn);
    RUN_TEST(test_h31_portrait_index_is_turn_div_2);
    RUN_TEST(test_h31_gate_fires_dialog_on_turn_3);
    RUN_TEST(test_h32_pan_corner_and_unconditional_page2_dialog);
    RUN_TEST(test_h33_item_drop_gated_off_then_page3_dialog);
    RUN_TEST(test_h34_two_portrait_flashes_with_paired_ids);
    RUN_TEST(test_h34_portrait_id_pair_tracks_counter);
    RUN_TEST(test_h34_portrait_id_is_8bit_truncated);
    RUN_TEST(test_wrap_forwards_three_args_in_order);
    RUN_TEST(test_h35_dialog_page5_then_kill_from_0x12);
    RUN_TEST(test_h35_kill_index_is_literal_ignores_arg);
    printf("\n");
}
