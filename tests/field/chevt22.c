/*
 * unit tests for src/field/chevt2.c (part 2)
 *
 * fd2_chapter_event_handler_38__ch25_dialog_with_state (dispatch idx 0x38 @
 * table 0x51B91) is a straight-line ch25 turn-6 establishing scene with no
 * computed logic and no turn gate. Its functionally-exact contract is the fixed
 * call sequence, in order:
 *     fd2_pan_cursor_and_window(6, 0x28)
 *     fd2_load_chapter_portraits_and_dump_tmp(1)
 *     fd2_cutscene_event_trigger(0x4A)
 *     fd2_display_dialog_scene(current_chapter_text, page 5, ...)
 *     fd2_clear_all_chars_facing()
 * In the binary the last call is a tail-JMP into fd2_clear_all_chars_facing
 * (a binary-size optimisation); the source is a plain call + return.
 *
 * These tests drive the REAL handler and its REAL callees. The deterministic,
 * host-observable contract pinned here is:
 *   (a) the camera pan lands the battle window origin exactly on (6, 0x28)
 *       (fd2_pan_cursor_and_window steps origin one tile per composite until it
 *       equals the target), and
 *   (b) the page-5 dialog VM actually executes its body (g_dlg_glyph_calls).
 * The cutscene-0x4A walk-animation interpreter and the facing reset are pure
 * display/state side effects; per the same deferral the sibling handlers apply
 * to display branches, their pixel/sprite output is deferred to Phase 9
 * integration. They are still driven for real here over host-safe inputs (a
 * 0-group cutscene script + an empty party) so the real handler executes its
 * whole body without any wild reads.
 *
 * Host-safety recipe mirrors the proven part-1 (chevt2) and chtrans suites:
 *   - empty party (member_count = 0): the cutscene per-char paint, the shadow
 *     overlay, and the facing-reset loop all iterate zero chars,
 *   - the portrait loader scan length is the in-process alloc_offset; 0 makes
 *     the scan a no-op (the loader still re-reads the real staged FDFIELD.DAT
 *     and rewrites FD2.TMP, exactly as the part-1 h36 test exercises),
 *   - a real workspace buffer + sprite atlas back the real compositor/blit so
 *     the pan + cutscene composites read valid memory (the tile-map blit itself
 *     is the testglob recorder g_composite_call_count),
 *   - cutscene_event_state = 0 skips the palette-fade-in tween (no VGA palette
 *     writes); the cutscene script for slot 0x4A is an in-memory 0-group script
 *     so the interpreter just composites a final clean frame,
 *   - the page-5 dialog runs the real dialog VM over an in-memory int16 program
 *     (NOT a game file); the glyph blitter is the testglob recorder, the empty
 *     BIOS keyboard buffer keeps blink set, and audiofix gates the per-glyph
 *     blink/typewriter path host-safely.
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
extern int g_composite_call_count;   /* tile-map blit (composite stage 1)   */
extern int g_dlg_glyph_calls;        /* dialog glyph blitter                 */

extern void *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];

/* ---- host-safe render workspace (mirrors chtrans ct_install_safe_render_env) */
#define CE22_WS_SPAN (191u * 0x1c8u + 0x138u)   /* (h-1)*sstride + w the blit reads */
static uint8 g_ce22_ws_buffer[CE22_WS_SPAN];
static uint8 g_ce22_sprite_atlas[6 + 64 * 4 + 4];

/* ---- in-memory cutscene script for slot 0x4A: a single n_groups=0 byte, so
 * the interpreter parses zero groups and only composites the final frame. ---- */
static uint8 g_ce22_cutscene_script[1] = { 0x00 };

/* ---- in-memory page-5 dialog program (mirrors part-1 ce35_setup) ---- */
static int16 g_ce22_dlg_prog[12];

static void ce22_setup(int glyphs)
{
    int i;
    uint32 *atlas_tbl;

    /* --- portrait-loader env: alloc_offset 0 => scan is a no-op (still re-reads
     * the real staged FDFIELD.DAT[4*3+2] and rewrites FD2.TMP). --- */
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;            /* loaded fresh by the loader  */
    data_fd2_chapter_init_phase_flag = 1;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE   */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    /* --- shared real-render env for pan + cutscene composites --- */
    data_fd2_battle_party_member_count = 0;      /* all per-char loops are no-ops */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce22_ws_buffer - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_anim_phase = 1;
    atlas_tbl = (uint32 *)(g_ce22_sprite_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce22_sprite_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;
    data_fd2_chapter_cutscene_event_state = 0;   /* skip palette-fade-in tween   */

    /* --- cutscene slot 0x4A: 0-group in-memory script --- */
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x4A] =
        g_ce22_cutscene_script;

    /* --- page-5 dialog program: header word 5 -> body at byte 0x10 (int16 idx
     * 8); body is `glyphs` TEXT opcodes then END (-1). --- */
    memset(g_ce22_dlg_prog, 0, sizeof(g_ce22_dlg_prog));
    g_ce22_dlg_prog[5] = 0x10;
    for (i = 0; i < glyphs; i++) {
        g_ce22_dlg_prog[8 + i] = 0x41;           /* TEXT glyph */
    }
    g_ce22_dlg_prog[8 + glyphs] = -1;            /* END */
    current_chapter_text = (uint32)g_ce22_dlg_prog;

    /* --- deterministic dialog VM env: empty BIOS keyboard buffer + audio gated;
     * no active portrait so END skips the portrait-close path. --- */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;
}

static void ce22_teardown(void)
{
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x4A] = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    current_chapter_text = 0;
    remove("FD2.TMP");                  /* generated swap file (not a game file) */
}

/* ----------------------------------------------------------------
 * Full handler contract: the camera pan lands the window origin exactly on the
 * literal target (6, 0x28), and the real page-5 dialog VM executes its body
 * (one glyph rendered). The window starts away from the target so the pan is
 * observable on both axes; g_composite_call_count > 0 proves the pan composited
 * frames. A 1-glyph page-5 program proves the dialog body (page 5, not some
 * other page) ran. The cutscene-0x4A interpreter and facing reset run for real
 * over the host-safe 0-group script + empty party.
 * ---------------------------------------------------------------- */
static void test_h38_pan_to_6_28_then_page5_dialog(void)
{
    ce22_setup(1);

    /* start the window away from (6, 0x28) on both axes so the pan is visible */
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;

    fd2_chapter_event_handler_38__ch25_dialog_with_state(0);

    /* pan landed the window origin on the literal target (6, 0x28) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 6);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x28);
    /* the camera pan composited frames on the way to the target */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* the page-5 dialog body executed (rendered the single glyph) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    /* the portrait loader ran its no-op scan and nulled its load buffer */
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);

    ce22_teardown();
}

/* ----------------------------------------------------------------
 * The handler ignores its dispatch arg: it is the table's 1-arg cdecl ABI
 * placeholder and the body reads only literals. Driving the handler with a
 * non-zero arg (0x55) must produce the identical observable effect — origin on
 * (6, 0x28) and the page-5 dialog body running — proving the arg does not leak
 * into any call. The dialog body is empty (immediate END) here so the pan +
 * dialog-entry are the sole effects under test.
 * ---------------------------------------------------------------- */
static void test_h38_ignores_dispatch_arg(void)
{
    ce22_setup(0);                      /* 0 glyphs: immediate END */

    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x05;

    fd2_chapter_event_handler_38__ch25_dialog_with_state(0x55);

    /* identical fixed target regardless of the arg */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 6);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x28);
    /* dialog was entered but rendered nothing (immediate END) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce22_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_39__ch26_cinematic @ 0x354DD
 *
 * ch26 establishing-shot stub (dispatch idx 0x39 @ table 0x51B91). Body:
 *   fd2_load_chapter_portraits_and_dump_tmp(data_fd2_battle_turn_counter)
 *       -- the RAW counter, same as the ch24 handler_36, NOT the signed /2 used
 *          by the ch21/ch22 handlers
 *   fd2_pan_cursor_and_window(9, 0)
 *   __delay_thunk_375b2(400)
 * No turn gate, no dialog. In the binary the pan + 400ms hold + RET is a
 * Class-3 shared tail borrowed from handler_36 @ 0x353C4; these tests pin THIS
 * handler's functionally-exact contract: the RAW-counter portrait index and the
 * single pan target (9, 0). Reuses ce22_setup's real render/portrait env above
 * (the portrait loader re-reads the staged real FDFIELD.DAT and rewrites
 * FD2.TMP); __delay_thunk_375b2 is REAL and paces 400ms against the
 * host-advancing BIOS tick word.
 * ================================================================ */

/* ----------------------------------------------------------------
 * Portrait index is the RAW counter (NO /2) — guards against copying the
 * ch21/ch22 handlers' signed /2. The tile-event table holds a decoy record
 * race=2 (the value a wrong /2 port — 5/2 == 2 — would select) and a target
 * record race=5; with turn_counter = 5 only the race=5 record matches, so the
 * real fd2_init_runtime_char_for_battle runs exactly once (party_member_count
 * 0 -> 1). ce22_setup primes the real loader env; we then install a non-zero
 * tile-event scan (ce22_setup leaves alloc_offset 0) with the decoy records.
 * ---------------------------------------------------------------- */
static uint8 *g_ce22_h39_tileevent;

static void test_h39_portrait_index_is_raw_counter(void)
{
    static const uint8 races[6] = { 0, 0, 0x02, 0, 0, 0x05 }; /* decoy@2, target@5 */
    int i;

    ce22_setup(0);

    /* install a 6-record tile-event scan (stride 0x1A, race byte at +0x98) over
     * the no-op env ce22_setup left in place. */
    g_ce22_h39_tileevent =
        (uint8 *)malloc((size_t)0x98 + (size_t)6 * 0x1a + 0x20);
    memset(g_ce22_h39_tileevent, 0, (size_t)0x98 + (size_t)6 * 0x1a + 0x20);
    for (i = 0; i < 6; i++) {
        g_ce22_h39_tileevent[i * 0x1a + 0x98] = races[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce22_h39_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = 6;
    data_fd2_battle_party_member_count = 0;

    data_fd2_battle_turn_counter = 5;             /* raw 5 (NOT 5/2 == 2) */

    fd2_chapter_event_handler_39__ch26_cinematic(0);

    /* exactly the race==5 record matched -> one char inited; the race==2 decoy
     * (the only other non-zero record) would have matched a wrong /2 port. */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);  /* loader freed+nulled */

    free(g_ce22_h39_tileevent);
    g_ce22_h39_tileevent = 0;
    ce22_teardown();
}

/* ----------------------------------------------------------------
 * The single pan lands the window origin exactly on the borrowed-tail target
 * (9, 0). The window starts away from the target on both axes so the move is
 * observable, and g_composite_call_count > 0 proves the pan composited frames.
 * No dialog runs in this handler. A non-zero dispatch arg (0x55) is passed to
 * confirm the arg does not leak into the pan target (the body reads only
 * literals + the turn counter). alloc_offset 0 keeps the portrait load a
 * host-safe no-op.
 * ---------------------------------------------------------------- */
static void test_h39_pan_to_9_0_ignores_arg(void)
{
    ce22_setup(0);                       /* alloc_offset 0: portrait scan no-op */
    data_fd2_battle_turn_counter = 2;    /* real ch26 trigger turn */

    /* start the window away from the target on both axes */
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_39__ch26_cinematic(0x55);

    /* pan landed the window origin on the literal target (9, 0) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 9);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0);
    /* the camera pan composited frames on the way to the target */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* no dialog in this handler */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce22_teardown();
}

void run_field_chevt22_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 2)\n");
    RUN_TEST(test_h38_pan_to_6_28_then_page5_dialog);
    RUN_TEST(test_h38_ignores_dispatch_arg);
    RUN_TEST(test_h39_portrait_index_is_raw_counter);
    RUN_TEST(test_h39_pan_to_9_0_ignores_arg);
    printf("\n");
}
