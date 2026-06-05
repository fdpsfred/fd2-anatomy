/*
 * unit tests for src/field/chevt1.c (part 4: handler 20 + shared body
 * fd2_show_chapter_dialog_with_portrait_set_1)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1/2/3 (chevt11.c /
 * chevt12.c / chevt13.c) cover handlers 00..1F; this part covers handler 20 and
 * the shared portrait+dialog body it falls through into.
 *
 * fd2_chapter_event_handler_20__ch10_dialog @ 0x34BE2 is dispatch idx 0x20 of
 * that table — chapter 10 turn-event slot 0, fired at turn 5 / phase 1 when the
 * player's 5th turn ends and the reinforcements (援軍) arrive. In the binary it
 * is a 5-byte adapter stub (PUSH 0x28) that falls through (no JMP) into the
 * shared body fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7, so its
 * full effect is that body: reload portrait set 1, then show dialog page 1. It
 * is a straight-line, no-branch beat with no camera pan, no cutscene trigger,
 * no state writes beyond the portrait reload, no RNG, no numeric computation,
 * and no CALL-return value used.
 *
 * fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7 is that shared body as
 * a directly-callable void(void) helper. Its other entry path is
 * fd2_chapter_event_handler_05__ch13_thunk @ 0x34D68 ("PUSH 0x28; JMP 0x34BE7",
 * ch13). Calling the helper directly exercises the same two-callee chain
 * (real portrait set 1 reload + real page-1 dialog dispatch) as handler 20.
 *
 * Two observable, deterministic contracts are checked:
 *   - the real fd2_load_chapter_portraits_and_dump_tmp(1) runs FOR REAL against
 *     the staged FDICON.B24 + FDFIELD.DAT and rewrites FD2.TMP to its full
 *     0x32A00 bytes (portrait set 1);
 *   - fd2_display_dialog_scene runs FOR REAL on a per-page-distinct-glyph
 *     program (page p -> single TEXT glyph idx 0x50+p, then END), so a correct
 *     page-1 dispatch must emit exactly one glyph with idx 0x51 and any wrong
 *     page fails loudly. The glyph blit is the testglob recorder
 *     (g_dlg_glyph_calls / g_dlg_glyph_last_idx), making the dispatched page
 *     observable WITHOUT touching real VGA.
 *
 * The pure blit/display side effects (the real glyph render path, portrait
 * pixels) are deferred to Phase 9 integration.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "fieldfix.h"

/* testglob.c records each glyph the real dialog VM blits, so the dispatched
 * page is observable without touching real VGA. */
extern int    g_dlg_glyph_calls;
extern uint32 g_dlg_glyph_last_idx;

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), so the dispatched page is identifiable by the recorded glyph idx.
 * Layout (int16 words):
 *   [0..0x10]      header words: page p -> byte offset of its glyph word
 *   [0x11+2*p]     page p glyph (0x50+p)
 *   [0x12+2*p]     page p END (-1)                                          */
static int16 g_ev20_dlg[0x11 + 2 * 0x11];

static void ev20_install_safe_env(void)
{
    int p;

    /* shared ch25-style real-portrait-reload env (empty party, gated HUD,
     * throttled palette, real compositor workspace, empty keyboard buffer,
     * alloc_offset 0, current_chapter_id 4, fresh field buffer, 64-slot
     * g_ev_rc). It installs an immediate-END dialog program, which the
     * per-page-distinct-glyph program below then overrides. */
    ev_install_safe_env();

    /* per-page (glyph, END) pairs start right after the 0x11 header words, so
     * the page the handler selects is identifiable by the recorded glyph idx. */
    for (p = 0; p <= 0x10; p++) {
        g_ev20_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev20_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev20_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev20_dlg;

    /* no portrait open on entry, so the dialog END path skips the close
     * sequence and returns at once (one glyph for the dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* reset the glyph recorder so the per-test count is clean
     * (ev_install_safe_env does not touch it). */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: reload portrait set 1 (real) then show
 * dialog page 1 via the real dialog VM. Observable, deterministic contract:
 * the real portrait reload rewrote FD2.TMP to its full 0x32A00 bytes, exactly
 * one glyph is emitted and it is page 1's glyph (idx 0x51) — proving the
 * handler dispatches page 1 (not any other page) — and the whole real callee
 * chain runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch10_event20_reloads_portrait1_and_shows_dialog_page1(void)
{
    ev20_install_safe_env();

    remove("FD2.TMP");

    fd2_chapter_event_handler_20__ch10_dialog(0);

    /* the real portrait reload ran: FD2.TMP rewritten to its full 0x32A00. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* exactly page 1 was shown: one glyph, idx 0x51 (= 0x50 + page 1). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x51);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * The shared body, called directly (handler_05's ch13 entry path). It must
 * produce the SAME effect as handler_20: reload portrait set 1 (real) then show
 * dialog page 1 via the real dialog VM. Observable, deterministic contract: the
 * real portrait reload rewrote FD2.TMP to its full 0x32A00 bytes, exactly one
 * glyph is emitted and it is page 1's glyph (idx 0x51) — proving the body
 * dispatches page 1 (not any other page) — and the whole real callee chain runs
 * to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_show_chapter_dialog_portrait_set_1_reloads_portrait1_page1(void)
{
    ev20_install_safe_env();

    remove("FD2.TMP");

    fd2_show_chapter_dialog_with_portrait_set_1();

    /* the real portrait reload ran: FD2.TMP rewritten to its full 0x32A00. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* exactly page 1 was shown: one glyph, idx 0x51 (= 0x50 + page 1). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x51);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_21__ch10_dialog_with_state @ 0x34C1E (dispatch idx
 * 0x21) — chapter 10 turn-event slot 1, fired at the end of turn 20. Two
 * deterministic, observable effects, both checked here:
 *   (1) dialog page 2 is dispatched (the real dialog VM emits exactly one glyph
 *       whose idx is 0x52 = 0x50 + page 2; any wrong page fails loudly);
 *   (2) the AI-class byte combat_aux_block[0xD] (struct offset 0x34) of the two
 *       protected NPC units 0x0C and 0x0D is cleared to 0, and ONLY that byte
 *       of those two slots — neighbouring bytes within each slot
 *       (combat_aux_block[0xC] at 0x33, combat_aux_block[0xE] at 0x35) and the
 *       neighbouring slots 0x0B / 0x0E are left untouched.
 *
 * Both target slots are pre-seeded to 0xFF (and the guard bytes/slots to a
 * distinct 0xAA sentinel) so a correct run must zero exactly two bytes. Unlike
 * handler_20 this handler has its own __CHK and no portrait reload, so FD2.TMP
 * is not part of its contract and is not asserted.
 * ---------------------------------------------------------------- */
static void test_ch10_event21_shows_page2_and_clears_ai_flag_for_0c_0d(void)
{
    ev20_install_safe_env();

    /* seed the two target AI-class bytes non-zero so a real clear is visible. */
    g_ev_rc[0x0C].combat_aux_block[0xD] = 0xFF;
    g_ev_rc[0x0D].combat_aux_block[0xD] = 0xFF;

    /* distinct guard sentinels: the immediate in-slot neighbours of [0xD] and
     * the adjacent slots must survive untouched. */
    g_ev_rc[0x0C].combat_aux_block[0xC] = 0xAA;
    g_ev_rc[0x0C].combat_aux_block[0xE] = 0xAA;
    g_ev_rc[0x0D].combat_aux_block[0xC] = 0xAA;
    g_ev_rc[0x0D].combat_aux_block[0xE] = 0xAA;
    g_ev_rc[0x0B].combat_aux_block[0xD] = 0xAA;
    g_ev_rc[0x0E].combat_aux_block[0xD] = 0xAA;

    fd2_chapter_event_handler_21__ch10_dialog_with_state(0);

    /* (1) exactly page 2 was shown: one glyph, idx 0x52 (= 0x50 + page 2). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x52);

    /* (2) the two AI-class bytes were cleared to 0. */
    ASSERT_EQ((long)g_ev_rc[0x0C].combat_aux_block[0xD], 0L);
    ASSERT_EQ((long)g_ev_rc[0x0D].combat_aux_block[0xD], 0L);

    /* and nothing adjacent was disturbed (precise single-byte writes). */
    ASSERT_EQ((long)g_ev_rc[0x0C].combat_aux_block[0xC], (long)0xAA);
    ASSERT_EQ((long)g_ev_rc[0x0C].combat_aux_block[0xE], (long)0xAA);
    ASSERT_EQ((long)g_ev_rc[0x0D].combat_aux_block[0xC], (long)0xAA);
    ASSERT_EQ((long)g_ev_rc[0x0D].combat_aux_block[0xE], (long)0xAA);
    ASSERT_EQ((long)g_ev_rc[0x0B].combat_aux_block[0xD], (long)0xAA);
    ASSERT_EQ((long)g_ev_rc[0x0E].combat_aux_block[0xD], (long)0xAA);

    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_22__unref_dialog @ 0x34C6C (dispatch idx 0x22) —
 * an unreferenced dialog-only slot whose entire 7-byte body is
 * "PUSH 0x28; JMP 0x34901", borrowing handler_18's shared entry so its effect
 * is identical to handler_18: show dialog page 3 and return. No portrait
 * reload, no camera pan, no state write, no branch, no RNG.
 *
 * The single deterministic, observable contract: the real dialog VM emits
 * exactly one glyph and it is page 3's glyph (idx 0x53 = 0x50 + page 3),
 * proving the handler dispatches page 3 (not any other page) and the borrowed
 * shared body runs to completion without faulting. Like handler_21 it has its
 * own __CHK and no portrait reload, so FD2.TMP is not part of its contract and
 * is not asserted.
 * ---------------------------------------------------------------- */
static void test_event22_shows_dialog_page3(void)
{
    ev20_install_safe_env();

    fd2_chapter_event_handler_22__unref_dialog(0);

    /* exactly page 3 was shown: one glyph, idx 0x53 (= 0x50 + page 3). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x53);

    ev_restore_rc_ptr();
}

void run_field_chevt14_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt14\n");
    RUN_TEST(test_ch10_event20_reloads_portrait1_and_shows_dialog_page1);
    RUN_TEST(test_show_chapter_dialog_portrait_set_1_reloads_portrait1_page1);
    RUN_TEST(test_ch10_event21_shows_page2_and_clears_ai_flag_for_0c_0d);
    RUN_TEST(test_event22_shows_dialog_page3);
    printf("\n");
}
