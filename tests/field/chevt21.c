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
#include "blitprob.h"   /* tg_install/restore_compositor_safe_atlases */

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
    tg_install_compositor_safe_atlases();          /* camera sweep -> real compositor */
    data_fd2_battle_turn_counter = 6;              /* (int)6/2 == 3 */
    current_chapter_text = 0;

    fd2_chapter_event_handler_2f__ch21_turn_gated(0);

    /* exactly the race==3 record matched -> one char inited */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);  /* loader freed+nulled */

    tg_restore_compositor_safe_atlases();
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
    tg_install_compositor_safe_atlases();          /* camera sweep -> real compositor */
    data_fd2_battle_turn_counter = 8;              /* (int)8/2 == 4 */
    current_chapter_text = 0;

    fd2_chapter_event_handler_31__ch22_turn_gated(0);

    /* exactly the race==4 record matched -> one char inited */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);  /* loader freed+nulled */

    tg_restore_compositor_safe_atlases();
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
 * fd2_cinematic_chapter_portrait_dump_with_white_flash is now a REAL emitted
 * function (src/field/chevt2.c). These tests drive the REAL handler and the REAL
 * cinematic, observing the two forwarded portrait ids one level down through the
 * real portrait loader: the cinematic forwards (id & 0xFF) to
 * fd2_load_chapter_portraits_and_dump_tmp, whose tile-event race-scan inits one
 * runtime_char per record whose race byte equals the forwarded id. By seeding a
 * tile-event table whose records carry exactly the two expected pair ids (plus
 * off-by-one decoys that must NOT match), the party-member count after the
 * handler == 2 proves both ids were computed and forwarded correctly, and the
 * 6-entry delay log (3 per cinematic call) proves both calls ran (the second via
 * the borrowed tail). The pan target of the second call is pinned via the final
 * window origin. Reuses ce_setup_portrait_env above for the real loader env and
 * adds a host-safe render workspace + 768-byte palette so the cinematic's pan /
 * white-flash / composite execute for real without wild reads. No display
 * assertions: the pixel output is owned by the cursor / palette / rndscene
 * suites and is deferred to Phase 9 integration.
 * ================================================================ */

/* testglob ordered delay-tick log (opt-in): pins the 300/200/400 per-call
 * white-flash sequence and counts cinematic calls. */
extern int    g_delay375b2_log_on;
extern int    g_delay375b2_log_count;
extern uint32 g_delay375b2_log[16];

/* host-safe render workspace + sprite atlas + 768-byte palette for the real
 * cinematic (pan composites + the two white-flash palette writes + the final
 * composite). Mirrors chevt22 ce22_setup's render env. */
#define CE34_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ce34_ws[CE34_WS_SPAN];
static uint8 g_ce34_atlas[6 + 64 * 4 + 4];
static uint8 g_ce34_palette[256 * 3];

/* Stand up the full real-cinematic env on top of the portrait-loader fixture:
 * `count` tile-event records with races `races[]` drive the id observation, and
 * the render workspace + palette let the cinematic body run for real. The window
 * origin starts at `start_ox`,`start_oy` so the final pan target is observable. */
static void h34_setup_real_cinematic(int count, const uint8 *races,
                                     uint32 start_ox, uint32 start_oy)
{
    int i;
    uint32 *atlas_tbl;

    ce_setup_portrait_env(count, races);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce34_ws - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_view_window_origin_x = start_ox;
    data_fd2_battle_view_window_origin_y = start_oy;
    data_fd2_battle_anim_phase = 1;
    atlas_tbl = (uint32 *)(g_ce34_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce34_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    for (i = 0; i < 256 * 3; i++) {
        g_ce34_palette[i] = 0x20;
    }
    data_fd2_vga_palette_data_ptr = (uint32)g_ce34_palette;

    g_delay375b2_log_on = 1;
    g_delay375b2_log_count = 0;
    g_composite_call_count = 0;
}

static void h34_teardown_real_cinematic(void)
{
    g_delay375b2_log_on = 0;
    g_delay375b2_log_count = 0;
    ce_teardown_portrait_env();
}

/* ----------------------------------------------------------------
 * A real ch23 trigger turn (22 = 0x16): the id math is on the turn-counter byte
 * directly: ((uint8)22 - 0x0E) * 2 = 8*2 = 16 (0x10) for the first portrait and
 * 17 (0x11) for the second. The tile-event table carries one record per expected
 * id (race 16, race 17) plus off-by-one decoys (race 15, race 18) that must NOT
 * match. Both cinematic calls run (the second via the borrowed tail), so exactly
 * the two intended records init (party_member_count 0 -> 2), the delay log holds
 * 6 ticks (3 per call: 300, 200, 400), and the final pan lands the window origin
 * on the second call's target (0x1A, 0xB).
 * ---------------------------------------------------------------- */
static void test_h34_two_portrait_flashes_with_paired_ids(void)
{
    /* idx0 race=16 (call-1 id), idx1 race=17 (call-2 id), idx2/3 decoys */
    static const uint8 races[4] = { 16, 17, 15, 18 };

    h34_setup_real_cinematic(4, races, 0x1A, 0xB);
    data_fd2_battle_turn_counter = 22;          /* real ch23 trigger turn      */

    fd2_chapter_event_handler_34__ch23_ai_ctrl(0);

    /* both ids (16, 17) matched their record; the 15/18 decoys did not -> the
     * pair was computed exactly, not off by one */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 2);
    /* exactly two cinematic calls ran (3 delays each: 300, 200, 400) */
    ASSERT_EQ((long)g_delay375b2_log_count, 6);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);
    ASSERT_EQ((long)g_delay375b2_log[3], 300);
    ASSERT_EQ((long)g_delay375b2_log[4], 200);
    ASSERT_EQ((long)g_delay375b2_log[5], 400);
    /* the second (last) call panned to the fixed target (0x1A, 0xB) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0x1A);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0xB);

    h34_teardown_real_cinematic();
}

/* ----------------------------------------------------------------
 * Portrait-id pair tracks the turn counter at the lowest documented value 0xE:
 * ((0x0E - 0x0E) * 2) = 0 and +1 = 1 -> pair 0/1. Records race 0 (call-1 id) and
 * race 1 (call-2 id) with a race-2 decoy; alloc_offset scans exactly these three
 * so the zero-filled tail beyond them is never reached (no spurious id-0 match).
 * Both records init -> count 2.
 * ---------------------------------------------------------------- */
static void test_h34_portrait_id_pair_tracks_counter(void)
{
    static const uint8 races[3] = { 0, 1, 2 };  /* call-1 id 0, call-2 id 1, decoy 2 */

    h34_setup_real_cinematic(3, races, 0x1A, 0xB);
    data_fd2_battle_turn_counter = 0x0E;        /* (0x0E-0x0E)*2 = 0 -> pair 0/1 */

    fd2_chapter_event_handler_34__ch23_ai_ctrl(0);

    /* ids 0 and 1 each matched their record; the decoy (2) did not */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 2);
    ASSERT_EQ((long)g_delay375b2_log_count, 6);

    h34_teardown_real_cinematic();
}

/* ----------------------------------------------------------------
 * The id is computed in 8-bit (AL) arithmetic and TRUNCATED to a byte, NOT a
 * 32-bit int. turn_counter = 0xFF00: the loader reads only the low byte (0x00),
 * so ((uint8)0x00 - 0x0E) = 0xF2, *2 = 0x1E4 -> low byte 0xE4 (228) for the first
 * portrait, 0xE5 (229) for the second. A naive 32-bit `((int)turn - 0x0E) * 2`
 * would instead yield -28 / -27 (no record matches), and the upper bytes of the
 * counter must not leak into the id. Records race 0xE4 (call-1) and 0xE5 (call-2)
 * plus a 0xE3 decoy guard the byte-wrap (MOVZX EAX,AL) semantics: count 2 proves
 * both wrapped ids matched.
 * ---------------------------------------------------------------- */
static void test_h34_portrait_id_is_8bit_truncated(void)
{
    static const uint8 races[3] = { 0xE4, 0xE5, 0xE3 };  /* wrapped ids + decoy */

    h34_setup_real_cinematic(3, races, 0x1A, 0xB);
    /* 0xFF00 -> low byte 0x00; the high byte must be masked off (MOV AL,[...]) */
    data_fd2_battle_turn_counter = 0xFF00;

    fd2_chapter_event_handler_34__ch23_ai_ctrl(0);

    /* both wrapped ids (0xE4, 0xE5) matched; the 0xE3 decoy did not -> the
     * arithmetic truncated to a byte and ignored the high bytes */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 2);
    ASSERT_EQ((long)g_delay375b2_log_count, 6);

    h34_teardown_real_cinematic();
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
 * Drives the REAL wrapper and the REAL cinematic over the same host-safe env as
 * the handler_34 suite. Because the wrapper makes exactly ONE cinematic call, all
 * three forwarded args are pinned precisely: the pan target (x, y) via the final
 * window origin, and the masked portrait id via the single race-scan match
 * (party_member_count 0 -> 1). A single delay triple (300, 200, 400) confirms one
 * call. No game-file display assertions.
 * ================================================================ */

/* ----------------------------------------------------------------
 * The wrapper forwards all 3 args verbatim and in order, exactly once. Uses
 * three distinct, non-equal values so any argument swap / drop / duplication
 * would change an observable: x=0x11 and y=0x22 (distinct) land the window origin
 * exactly, and id=0x33 matches the sole race-0x33 record. The window starts away
 * from (0x11, 0x22) on both axes so the pan is visible on each.
 * ---------------------------------------------------------------- */
static void test_wrap_forwards_three_args_in_order(void)
{
    static const uint8 races_a[1] = { 0x33 };
    static const uint8 races_b[1] = { 2 };

    /* distinct values: x != y != id, none zero, so order is observable */
    h34_setup_real_cinematic(1, races_a, 0x40, 0x10);
    fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash(0x11, 0x22, 0x33);

    /* exactly one cinematic call: one delay triple */
    ASSERT_EQ((long)g_delay375b2_log_count, 3);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);
    /* x=0x11, y=0x22 forwarded (and in order) -> window origin landed on them */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0x11);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x22);
    /* id=0x33 forwarded (low byte) -> the sole race-0x33 record inited */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    h34_teardown_real_cinematic();

    /* the exact arg triple the live caller (handler_3f, ch27) tail-JMPs with */
    h34_setup_real_cinematic(1, races_b, 0x40, 0x10);
    fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash(0xF, 0x1B, 2);

    ASSERT_EQ((long)g_delay375b2_log_count, 3);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0xF);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x1B);
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    h34_teardown_real_cinematic();
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

/* ================================================================
 * fd2_chapter_event_handler_36__ch24_cinematic @ 0x3535D
 *
 * ch24 establishing-shot cinematic: load the portrait set indexed by the RAW
 * data_fd2_battle_turn_counter (NOT the signed /2 of handler_2f/handler_31),
 * then a 4-corner camera sweep top-left (0,4) -> bottom-left (0,0x16) ->
 * bottom-right (0x1A,0x18) -> top-right (0x1A,2) with a 400ms (__delay_thunk)
 * hold at each. No turn gate, no dialog. The final pan + delay + RET is a
 * Class-3 shared tail in the binary; these tests confirm all four corners run
 * here (the window origin ends on the LAST corner). Reuses the
 * ce_setup_portrait_env / ce_teardown_portrait_env fixtures above.
 * ================================================================ */

/* ----------------------------------------------------------------
 * Portrait index is the RAW counter (NO /2). turn_counter = 5 must select race
 * index 5. The tile-event table holds a decoy record race=2 (the value a wrong
 * signed-/2 port — 5/2 == 2 — would pick) and a target record race=5; only the
 * race=5 record matches, so the real fd2_init_runtime_char_for_battle runs
 * exactly once (party_member_count 0->1). This guards against accidentally
 * copying the ch21/ch22 handlers' /2.
 * ---------------------------------------------------------------- */
static void test_h36_portrait_index_is_raw_counter(void)
{
    static const uint8 races[6] = { 0, 0, 0x02, 0, 0, 0x05 }; /* decoy@2, target@5 */

    ce_setup_portrait_env(6, races);
    tg_install_compositor_safe_atlases();          /* camera sweep -> real compositor */
    data_fd2_battle_turn_counter = 5;              /* raw 5 (NOT 5/2 == 2) */
    current_chapter_text = 0;

    fd2_chapter_event_handler_36__ch24_cinematic(0);

    /* exactly the race==5 record matched -> one char inited (race==2 decoy
     * would have matched a /2 port and is the only other non-zero record) */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);  /* loader freed+nulled */

    tg_restore_compositor_safe_atlases();
    ce_teardown_portrait_env();
}

/* ----------------------------------------------------------------
 * The full 4-corner sweep runs in order and the window origin lands on the LAST
 * swept corner — top-right (0x1A, 2). The window starts away from every target
 * so the moves are observable, and g_composite_call_count > 0 proves the pans
 * composited frames. Landing on (0x1A, 2) (and not an earlier corner) proves
 * the final pan — the Class-3 shared tail at 0x353C4 — is inlined here.
 * alloc_offset = 0 keeps the portrait load a host-safe no-op.
 * ---------------------------------------------------------------- */
static void test_h36_four_corner_sweep_ends_top_right(void)
{
    ce_setup_portrait_env(0, (const uint8 *)0);
    data_fd2_battle_turn_counter = 4;             /* real ch24 trigger turn */

    /* start the window away from all four sweep targets */
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_36__ch24_cinematic(0);

    /* sweep ended on the 4th corner top-right (0x1A, 2) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0x1A);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 2);
    /* camera sweep composited frames */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* no dialog in this handler */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce_teardown_portrait_env();
}

/* ================================================================
 * fd2_chapter_event_handler_37__ch25_first_time @ 0x353DA
 *
 * ch25 lord-only, first-time-gated tile-step combat trigger. The handler's
 * computed control flow is the entry gate
 *     if (event_arg == 0 && consumed_flags[0] == 0)
 * which decides whether the whole cinematic body runs, and the nested kill gate
 *     if (fd2_check_char_is_dead(0x11)) { consume; ...; drop }
 * which decides whether the tile event is consumed and char 0 gets the drop.
 * The unconditional tail clears data_fd2_battle_pending_xp_credit.
 *
 * The gate-ON body (dialog page 0 + the full combat cinematic
 * fd2_play_full_combat_cinematic against char 0x11 + death animation +
 * recomposite) is heavy display/combat orchestration that loads real combat
 * resources (BG/TAI/FIGANI/FDSHAP.DAT) and is pure side effect; per the same
 * deferral the h32/h33 suites apply to display branches, the gate-ON cinematic
 * body and its kill-gated consume+drop are deferred to Phase 9 integration.
 *
 * These tests pin the deterministic, display-free contract: both halves of the
 * entry AND-gate. When the gate is NOT taken the handler must touch nothing but
 * the unconditional XP clear — no dialog VM, no cinematic, no consume. The
 * consumed-flags global is backed by a local byte array (the established
 * aniwalk1.c recipe) so the "not consumed" observation is real memory, and the
 * dialog-glyph / composite recorders confirm no heavy path executed.
 * ================================================================ */

/* ----------------------------------------------------------------
 * Gate fails on the FIRST condition: a non-lord char (event_arg != 0) steps the
 * tile. Even with the tile un-consumed, the body must be skipped entirely; only
 * the unconditional pending-XP clear runs. Pins event_arg == 0 as a hard gate
 * (the arg is the stepping char id, read from [ESP+0x10]).
 * ---------------------------------------------------------------- */
static void test_h37_gate_off_when_non_lord_steps(void)
{
    uint8 t_consumed[4];
    uint32 save_cf;

    save_cf = data_fd2_field_map_tile_event_consumed_flags_ptr;
    t_consumed[0] = 0;                 /* tile NOT yet consumed */
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)t_consumed;

    current_chapter_text = 0;          /* gate must not deref this */
    data_fd2_battle_pending_xp_credit = 0x1234;   /* sentinel: must be cleared */
    g_dlg_glyph_calls = 0;
    g_composite_call_count = 0;

    fd2_chapter_event_handler_37__ch25_first_time(1);   /* non-lord stepping id */

    /* gate skipped the body: no dialog VM ran, nothing composited, and the tile
     * event was NOT consumed (the kill-gated consume lives inside the body). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);
    ASSERT_EQ((long)g_composite_call_count, 0);
    ASSERT_EQ((long)t_consumed[0], 0);
    /* pending XP is cleared unconditionally */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);

    data_fd2_field_map_tile_event_consumed_flags_ptr = save_cf;
    current_chapter_text = 0;
}

/* ----------------------------------------------------------------
 * Gate fails on the SECOND condition: the lord (event_arg == 0) steps, but the
 * tile event was already consumed (consumed_flags[0] != 0). The body must be
 * skipped, the consumed flag must stay set (the body never re-stamps it on this
 * path), and only the unconditional pending-XP clear runs. Pins the
 * consumed_flags[0] == 0 half of the AND gate.
 * ---------------------------------------------------------------- */
static void test_h37_gate_off_when_already_consumed(void)
{
    uint8 t_consumed[4];
    uint32 save_cf;

    save_cf = data_fd2_field_map_tile_event_consumed_flags_ptr;
    t_consumed[0] = 1;                 /* tile ALREADY consumed */
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)t_consumed;

    current_chapter_text = 0;          /* gate must not deref this */
    data_fd2_battle_pending_xp_credit = 0x5678;   /* sentinel: must be cleared */
    g_dlg_glyph_calls = 0;
    g_composite_call_count = 0;

    fd2_chapter_event_handler_37__ch25_first_time(0);   /* lord (char 0) steps */

    /* gate skipped the body even though the stepping char IS the lord */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);
    ASSERT_EQ((long)g_composite_call_count, 0);
    ASSERT_EQ((long)t_consumed[0], 1);            /* flag untouched */
    /* pending XP is cleared unconditionally */
    ASSERT_EQ(data_fd2_battle_pending_xp_credit, 0);

    data_fd2_field_map_tile_event_consumed_flags_ptr = save_cf;
    current_chapter_text = 0;
}

void run_field_chevt21_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 1)\n");
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
    RUN_TEST(test_h36_portrait_index_is_raw_counter);
    RUN_TEST(test_h36_four_corner_sweep_ends_top_right);
    RUN_TEST(test_h37_gate_off_when_non_lord_steps);
    RUN_TEST(test_h37_gate_off_when_already_consumed);
    printf("\n");
}
