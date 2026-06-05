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

void run_field_chevt2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2\n");
    RUN_TEST(test_gate_skips_dialog_on_non_trigger_turn);
    RUN_TEST(test_portrait_index_is_turn_div_2);
    RUN_TEST(test_gate_fires_dialog_on_turn_2);
    RUN_TEST(test_h30_both_ranges_armed);
    RUN_TEST(test_h30_boundaries_and_gap_untouched);
    printf("\n");
}
