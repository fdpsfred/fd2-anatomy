/*
 * unit tests for src/field/chevt1.c (part 3: handler 18 +
 * fd2_show_chapter_intro_text_dialog_mode_3 + handler 19)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1/2 (chevt11.c /
 * chevt12.c) cover handlers 00..17; this part covers handler 18, the named
 * helper fd2_show_chapter_intro_text_dialog_mode_3 @ 0x34906, and handler 19.
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

/* ================================================================
 * fd2_chapter_event_handler_19__ch7_first_time @ 0x34924
 *
 * Dispatch idx 0x19 of the per-event handler table at 0x51B91 (chapter 7
 * turn-event slot 0). A first-time-gated SECOND-STAGE beat:
 *   if (tile_event_consumed_flags[0x10] == 1):        // prior event 0x10 fired
 *     chapter_init_phase_flag = 1;
 *     load_chapter_portraits_and_dump_tmp(2);          // portrait set 2
 *     chapter_init_phase_flag = 0;
 *     pan_cursor_and_window(0x10, 10);
 *     cutscene_event_trigger(0x1E);
 *     display_dialog_scene(page 2, ...);
 *     tile_event_consumed_flags[0x11] = 1;             // consume own slot
 *
 * The testable risk core is the second-stage GATE (a state-transition branch
 * whose guard is a memory byte, == 1 rather than the usual == 0 first-time
 * sense), so BOTH paths are exercised. The gate byte [0x10] and the own
 * consume byte [0x11] live in the 0x20-byte tile-event consumed-flags block
 * pointed at by data_fd2_field_map_tile_event_consumed_flags_ptr, here backed
 * by a local buffer so both the read gate and the write consume are observable.
 *
 * Every callee on the gate-pass path is a REAL emitted function driven against
 * the shared fieldfix "ch25-style real portrait reload" env, plus a zero-group
 * cutscene script for event 0x1E so the real fd2_cutscene_event_trigger
 * composites once and returns:
 *   - fd2_load_chapter_portraits_and_dump_tmp(2) runs FOR REAL against the
 *     staged FDICON.B24 + FDFIELD.DAT and rewrites FD2.TMP to its full
 *     0x32A00 bytes (the init-phase flag is set to 1 around it and reset to 0);
 *   - fd2_display_dialog_scene runs FOR REAL on the per-page-distinct-glyph
 *     program (page p -> single TEXT glyph idx 0x50+p, then END), so a correct
 *     page-2 dispatch must emit exactly one glyph with idx 0x52 and any wrong
 *     page fails loudly. The glyph blit is the testglob recorder
 *     (g_dlg_glyph_calls / g_dlg_glyph_last_idx), making the dispatched page
 *     observable WITHOUT touching real VGA;
 *   - fd2_pan_cursor_and_window / fd2_composite_battle_frame run against the
 *     staged camera + compositor workspace with the empty active party.
 *
 * The pure blit/display side effects (dialog glyphs, cutscene/pan compositing,
 * portrait pixels) are deferred to Phase 9 integration.
 * ================================================================ */

/* zero-group cutscene script for event 0x1E: n_groups byte = 0, so the real
 * fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev19_script_1e[1] = { 0 };

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), so the dispatched page is identifiable by the recorded glyph idx.
 * Layout (int16 words):
 *   [0..0x10]      header words: page p -> byte offset of its glyph word
 *   [0x11+2*p]     page p glyph (0x50+p)
 *   [0x12+2*p]     page p END (-1)                                          */
static int16 g_ev19_dlg[0x11 + 2 * 0x11];

/* backing for the 0x20-byte tile-event consumed-flags block: byte [0x10] is the
 * second-stage gate this handler reads, byte [0x11] is the slot it consumes. */
static uint8 g_ev19_consumed_flags[0x20];

static void ev19_install_safe_env(void)
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
        g_ev19_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev19_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev19_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev19_dlg;

    /* no portrait open on entry, so each dialog END path skips the close
     * sequence and returns at once (one glyph per dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* handler_19 fires cutscene EVENT 0x1E; register its own zero-group script
     * so the real fd2_cutscene_event_trigger returns fast. */
    g_ev19_script_1e[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x1E] = g_ev19_script_1e;

    /* point the consumed-flags global at the local block; the per-test gate
     * byte [0x10] is seeded by each case. */
    memset(g_ev19_consumed_flags, 0, sizeof(g_ev19_consumed_flags));
    data_fd2_field_map_tile_event_consumed_flags_ptr =
        (uint32)g_ev19_consumed_flags;

    /* reset the glyph recorder so the per-test count is clean (ev_install_safe_env
     * does not touch it, and a prior suite may have left it non-zero). */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * GATE-PASS path: the prior event slot [0x10] is already consumed (== 1), so
 * the second-stage beat runs. Observable, deterministic contract: the init-phase
 * flag is set to 1 during the real portrait reload and reset to 0 afterward, the
 * real reload rewrote FD2.TMP to its full 0x32A00 bytes, dialog page 2 is shown
 * exactly once (one glyph, idx 0x52), this handler's own slot [0x11] is consumed
 * (set to 1), and the whole real callee chain runs to completion without
 * faulting.
 * ---------------------------------------------------------------- */
static void test_ch7_event19_gate_set_runs_beat_and_consumes_slot(void)
{
    ev19_install_safe_env();

    /* gate passes: prior event slot [0x10] already consumed. own slot [0x11]
     * starts 0 (from the memset) so its consume is observable. */
    g_ev19_consumed_flags[0x10] = 1;

    /* perturb the init-phase flag so the handler's reset to 0 is observable. */
    data_fd2_chapter_init_phase_flag = 0x55;

    remove("FD2.TMP");

    fd2_chapter_event_handler_19__ch7_first_time(0);

    /* the init-phase flag was set to 1 around the reload and reset to 0. */
    ASSERT_EQ(data_fd2_chapter_init_phase_flag, 0);

    /* the real portrait reload ran: FD2.TMP rewritten to its full 0x32A00. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* exactly page 2 was shown: one glyph, idx 0x52 (= 0x50 + page 2). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x52);

    /* this handler's own slot [0x11] was consumed; the gate byte [0x10] is
     * left untouched. */
    ASSERT_EQ(g_ev19_consumed_flags[0x11], 1);
    ASSERT_EQ(g_ev19_consumed_flags[0x10], 1);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * GATE-FAIL path: the prior event slot [0x10] is NOT consumed (!= 1), so the
 * whole second-stage beat is SKIPPED. Observable, deterministic contract: the
 * init-phase flag keeps its perturbed sentinel (never set to 1), no portrait
 * reload happened (FD2.TMP absent), no dialog dispatch (no glyph emitted), and
 * this handler's own slot [0x11] stays 0 (never consumed). Seeding [0x10] = 0
 * also pins the gate sense (== 1, not the usual == 0 first-time sense): a 0 byte
 * must take the skip path.
 * ---------------------------------------------------------------- */
static void test_ch7_event19_gate_clear_skips_beat(void)
{
    ev19_install_safe_env();

    /* gate fails: prior event slot [0x10] not consumed (0, the memset value). */
    g_ev19_consumed_flags[0x10] = 0;

    /* perturb the init-phase flag to a sentinel the skip path must leave alone. */
    data_fd2_chapter_init_phase_flag = 0x55;

    remove("FD2.TMP");

    fd2_chapter_event_handler_19__ch7_first_time(0);

    /* beat skipped: init-phase flag keeps its sentinel, no FD2.TMP written, no
     * dialog dispatch, and the own slot [0x11] stays 0. */
    ASSERT_EQ(data_fd2_chapter_init_phase_flag, 0x55);
    ASSERT_EQ(ev_fd2_tmp_size(), -1);
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);
    ASSERT_EQ(g_ev19_consumed_flags[0x11], 0);

    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

void run_field_chevt13_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt13\n");
    RUN_TEST(test_ch_event18_shows_dialog_page3);
    RUN_TEST(test_show_chapter_intro_text_dialog_mode_3_shows_page3);
    RUN_TEST(test_ch7_event19_gate_set_runs_beat_and_consumes_slot);
    RUN_TEST(test_ch7_event19_gate_clear_skips_beat);
    printf("\n");
}
