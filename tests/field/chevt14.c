/*
 * unit tests for src/field/chevt1.c (part 4: handler 20)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1/2/3 (chevt11.c /
 * chevt12.c / chevt13.c) cover handlers 00..1F; this part covers handler 20.
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

void run_field_chevt14_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt14\n");
    RUN_TEST(test_ch10_event20_reloads_portrait1_and_shows_dialog_page1);
    printf("\n");
}
