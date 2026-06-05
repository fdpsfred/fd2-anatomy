/*
 * unit tests for src/field/chevt1.c (part 3: handler 18 +
 * fd2_show_chapter_intro_text_dialog_mode_3)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1/2 (chevt11.c /
 * chevt12.c) cover handlers 00..17; this part covers handler 18 and the named
 * helper fd2_show_chapter_intro_text_dialog_mode_3 @ 0x34906.
 *
 * fd2_chapter_event_handler_18__unref_dialog @ 0x348FC is dispatch idx 0x18 of
 * that table. No chapter FDFIELD turn-event / tile-step hook references the
 * slot (unreferenced — possibly cut content). It is the MINIMAL dialog-only
 * beat: a single straight-line call with no branch, no RNG, no numeric
 * computation, and no CALL-return value used — it just shows dialog page 3 and
 * does nothing else (no portrait reload, no camera pan, no state writes). In
 * the binary it prepares its own 8 PUSHes (page=3 + the fixed dialog geometry)
 * and JMPs into the shared tail of fd2_show_chapter_dialog_with_portrait_set_1
 * at 0x34C0F.
 *
 * The one observable, deterministic contract is which PAGE it dispatches into
 * the real fd2_display_dialog_scene VM. As in the handler_14 tests, a custom
 * dialog program is installed where the targeted page resolves to a single TEXT
 * glyph then END; the glyph blit is the testglob recorder
 * (g_dlg_glyph_calls / g_dlg_glyph_last_idx), so the page selection is
 * observable WITHOUT touching real VGA. To make a wrong-page dispatch fail
 * loudly, EVERY page is given its own distinct glyph idx (page p -> glyph
 * 0x50+p): a correct page-3 dispatch must emit exactly one glyph with idx 0x53.
 * With no portrait open (active_portrait_blit_offset 0) the END opcode returns
 * at once — no portrait, scroll, file load, or page-break busy-wait — and an
 * empty BIOS keyboard buffer keeps the per-glyph poll deterministic.
 *
 * The pure blit/display side effects (the real glyph render path) are deferred
 * to Phase 9 integration.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* testglob.c records each glyph the real dialog VM blits, so the dispatched
 * page is observable without touching real VGA. */
extern int    g_dlg_glyph_calls;
extern uint32 g_dlg_glyph_last_idx;

/* custom dialog program: each page 0..0x10 resolves to its own single glyph
 * (idx 0x50+page) then END, so the page the handler selects is identifiable by
 * the recorded glyph idx. Layout (int16 words):
 *   [0..0x10]      header words: page p -> byte offset of its glyph word
 *   [0x11+2*p]     page p glyph (0x50+p)
 *   [0x12+2*p]     page p END (-1)                                          */
static int16 g_ev18_dlg[0x11 + 2 * 0x11];

static void ev18_install_safe_env(void)
{
    int p;

    /* per-page (glyph, END) pairs start right after the 0x11 header words. */
    for (p = 0; p <= 0x10; p++) {
        g_ev18_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev18_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev18_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev18_dlg;

    /* no portrait open on entry, so the END path skips the close sequence and
     * returns immediately. */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* empty BIOS keyboard buffer (head==tail) so the real keyboard poll after
     * the glyph returns 0 and the run stays deterministic. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: show dialog page 3 via the real dialog VM.
 * Observable, deterministic contract: exactly one glyph is emitted and it is
 * page 3's glyph (idx 0x53) — proving the handler dispatches page 3 (not any
 * other page) — and the real dialog call runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch_event18_shows_dialog_page3(void)
{
    ev18_install_safe_env();

    fd2_chapter_event_handler_18__unref_dialog(0);

    /* exactly page 3 was shown: one glyph, idx 0x53 (= 0x50 + page 3). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x53);
}

/* ----------------------------------------------------------------
 * fd2_show_chapter_intro_text_dialog_mode_3 @ 0x34906 is the named helper
 * handler_16 tail-JMPs to; it shows current_chapter_text dialog page 3 with
 * the same fixed geometry as handler_18 (it borrows the same shared dialog
 * tail at 0x34C0F). It takes no args and uses no CALL-return value, so the one
 * observable, deterministic contract is again the dispatched PAGE. Reusing the
 * per-page glyph program (page p -> glyph 0x50+p), a correct page-3 dispatch
 * must emit exactly one glyph with idx 0x53; any other page would emit a
 * different idx and fail loudly.
 * ---------------------------------------------------------------- */
static void test_show_chapter_intro_text_dialog_mode_3_shows_page3(void)
{
    ev18_install_safe_env();

    fd2_show_chapter_intro_text_dialog_mode_3();

    /* exactly page 3 was shown: one glyph, idx 0x53 (= 0x50 + page 3). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x53);
}

void run_field_chevt13_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt13\n");
    RUN_TEST(test_ch_event18_shows_dialog_page3);
    RUN_TEST(test_show_chapter_intro_text_dialog_mode_3_shows_page3);
    printf("\n");
}
