/*
 * unit tests for src/field/chevt1.c (part 6: handlers 2D, 2E)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1..5 (chevt11.c /
 * chevt12.c / chevt13.c / chevt14.c / chevt15.c) cover the other handlers; this
 * part covers handler 2D (a chapter-19 AI-control beat) and handler 2E (a
 * chapter-19 reinforcement beat).
 *
 * handler 2D — fd2_chapter_event_handler_2d__ch19_ai_ctrl @ 0x350B9
 *   chapter 19 AI-control beat, dispatch idx 0x2D of that table. Its body is a
 *   straight-line single call (no dialog, no RNG, no numeric computation, no
 *   CALL-return value used):
 *     - the per-event AI/dialog control flag (low nibble of combat_aux_block[0xD],
 *       struct offset 0x34) is set to 3 for the runtime-char range [0x10, 0x1F]
 *       inclusive (16 chars) via the real range setter
 *       fd2_set_combat_aux_block_byte_d_low4_for_char_range (which masks the byte
 *       to (old & 0xF0) | new_val, so only the low nibble moves and the high
 *       nibble survives).
 *
 * In the original binary this handler pre-pushes the flag (3) and end (0x1F)
 * args and JMPs (0x350C7 -> 0x34F37) into the class-3 shared tail hosted by
 * handler 12, which supplies the fixed start arg 0x10 — the same borrowed tail
 * handler 2B uses, so the effect is the 0x2B single-char write generalised to
 * the [0x10, 0x1F] range.
 *
 * The single callee is REAL. The writes span indices 0x10..0x1F, all inside the
 * shared g_ev_rc[0x48] fixture (max index 0x47), so no private oversized array is
 * needed and no dialog program / glyph recorder is involved. The distinguishing
 * feature is the 16-char [0x10, 0x1F] range, so the boundary guards (chars 0x0F
 * below and 0x20 above) are the load-bearing assertions.
 *
 * handler 2E — fd2_chapter_event_handler_2e__ch19_reinforcement @ 0x350CC
 *   chapter 19 turn-event slot 1, fired at turn 6 / phase 1. Its beat is a
 *   straight-line, no-branch sequence (no RNG, no numeric computation, no
 *   CALL-return value used) that drives THREE real callees in order:
 *     - reload portrait set 1 via the REAL fd2_load_chapter_portraits_and_dump_tmp
 *       (re-reads FDICON.B24 + FDFIELD.DAT and rewrites the 0x32A00-byte FD2.TMP
 *       swap file);
 *     - show dialog page 1 via the REAL fd2_display_dialog_scene;
 *     - recruit char_id 0x1B as reinforcement via the REAL
 *       fd2_init_runtime_char_from_base_growth, which appends one template slot to
 *       the menu-party roster (slot +6 team = 2 player, slot +7 / +8 char_id copy)
 *       and increments data_fd2_shared_menu_party_member_count.
 *
 * All three callees are REAL. Three observable, deterministic contracts are
 * checked in one comprehensive test (the handler has no branch, no gate):
 *   - the real portrait set 1 reload rewrote FD2.TMP to its full 0x32A00 bytes
 *     (proving the whole real reload callee chain ran against the staged real
 *     game files);
 *   - fd2_display_dialog_scene runs FOR REAL on a per-page-distinct-glyph program
 *     (page p -> single TEXT glyph idx 0x50+p, then END), so a correct page-1
 *     dispatch must emit exactly one glyph with idx 0x51 (= 0x50 + 1) and any
 *     wrong page fails loudly. The glyph blit is the testglob recorder
 *     (g_dlg_glyph_calls / g_dlg_glyph_last_idx), making the dispatched page
 *     observable WITHOUT touching real VGA;
 *   - the real recruit appended exactly one menu-roster slot for char 0x1B: the
 *     member count went 0 -> 1 and roster slot 0 has team byte (+6) = 2 and the
 *     char_id bytes (+7 / +8) = 0x1B. (The base/growth stat tables in testglob.c
 *     are zero-filled, so the derived HP/MP/AP/DP/DX and level are 0; the
 *     structural recruit fields — team and the char_id copies, taken straight
 *     from the dispatched arg — are the load-bearing recruit assertions, matching
 *     the handler_00 recruit test in chevt11.c.)
 *
 * The pure blit/display side effects (the real glyph render pixels, portrait
 * pixels) are deferred to Phase 9 integration; only the real FD2.TMP rewrite, the
 * dispatched-page identity, and the recruit's structural outcome are asserted
 * here.
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

/* ================================================================
 * handler 2D — fd2_chapter_event_handler_2d__ch19_ai_ctrl @ 0x350B9
 * ================================================================ */

/* Seed combat_aux_block[0xD] of every touched char (and the boundary guards) to
 * 0xA5 (high nibble 0xA must survive, low nibble 0x5 must move to 3), and the
 * in-slot neighbour bytes [0xC]/[0xE] to distinct sentinels so the low-nibble-
 * only write is independently witnessed. */
static void ev2d_seed(void)
{
    int i;

    for (i = 0x10; i <= 0x1F; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xA5;
        g_ev_rc[i].combat_aux_block[0xC] = 0xCC;   /* in-slot neighbour guard */
        g_ev_rc[i].combat_aux_block[0xE] = 0xEE;   /* in-slot neighbour guard */
    }

    /* chars just outside the range: must stay at their seeded sentinel. */
    g_ev_rc[0x0F].combat_aux_block[0xD] = 0xA5;    /* below range */
    g_ev_rc[0x20].combat_aux_block[0xD] = 0xA5;    /* above range */
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: set the AI flag of chars 0x10..0x1F
 * ([0xD] low nibble 3). All deterministic and observable; checked in one pass.
 * ---------------------------------------------------------------- */
static void test_ch19_event2d_sets_ai_flag3_range_0x10_to_0x1f(void)
{
    /* ev_install_safe_env zeroes g_ev_rc and points the runtime-char pointer at
     * it; this handler only needs that pointer (it never touches the dialog VM). */
    ev_install_safe_env();
    ev2d_seed();

    fd2_chapter_event_handler_2d__ch19_ai_ctrl(0);

    /* (1) combat_aux_block[0xD] low nibble set to 3, high nibble preserved
     * (0xA5 -> 0xA3) across the whole range — boundaries + an interior char. */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xD], (long)0xA3);
    ASSERT_EQ((long)g_ev_rc[0x17].combat_aux_block[0xD], (long)0xA3);
    ASSERT_EQ((long)g_ev_rc[0x1F].combat_aux_block[0xD], (long)0xA3);

    /* (2) range boundaries are exact: the chars just below (0x0F) and above
     * (0x20) keep their seeded sentinel — load-bearing for [0x10, 0x1F]. */
    ASSERT_EQ((long)g_ev_rc[0x0F].combat_aux_block[0xD], (long)0xA5);
    ASSERT_EQ((long)g_ev_rc[0x20].combat_aux_block[0xD], (long)0xA5);

    /* (3) in-slot neighbour bytes survive: the low-nibble-only [0xD] write did
     * not touch [0xC] or [0xE]. */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xE], (long)0xEE);
    ASSERT_EQ((long)g_ev_rc[0x1F].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev_rc[0x1F].combat_aux_block[0xE], (long)0xEE);

    ev_restore_rc_ptr();
}

/* ================================================================
 * handler 2E — fd2_chapter_event_handler_2e__ch19_reinforcement @ 0x350CC
 * ================================================================ */

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), identical layout to the chevt15.c suites, so the dispatched page
 * is identifiable by the recorded glyph idx. */
static int16 g_ev2e_dlg[0x11 + 2 * 0x11];

static void ev2e_install_env(void)
{
    int p;

    /* shared ch25-style real-portrait-reload / dialog-VM safe env (empty party,
     * gated HUD, throttled palette, real compositor workspace, empty keyboard
     * buffer, alloc_offset 0, current_chapter_id 4, fresh field buffer, and a
     * 64-slot menu roster started empty for the real recruit). It installs an
     * immediate-END dialog program, overridden below. */
    ev_install_safe_env();

    for (p = 0; p <= 0x10; p++) {
        g_ev2e_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev2e_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev2e_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev2e_dlg;

    /* no portrait open on entry, so the dialog END path skips the close
     * sequence and returns at once (one glyph for the dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: reload portrait set 1 (real), show dialog
 * page 1 via the real dialog VM, then recruit char 0x1B into the menu roster
 * (real). Observable, deterministic contract: FD2.TMP rewritten to its full
 * 0x32A00 bytes; exactly one glyph is emitted and it is page 1's glyph (idx
 * 0x51) — proving the handler dispatches page 1 (not any other page); and the
 * recruit appended exactly one slot for char 0x1B (member count 0 -> 1, team
 * byte = 2 player, char_id copies = 0x1B). The whole real callee chain runs to
 * completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch19_event2e_reload1_dialog1_recruits_char0x1b(void)
{
    uint8 *roster;

    ev2e_install_env();

    /* clear any stale swap file so the size check proves THIS reload wrote it
     * (FD2.TMP is the loader's output scratch file, not a real game input). */
    remove("FD2.TMP");

    fd2_chapter_event_handler_2e__ch19_reinforcement(0);

    /* (1) the real portrait set 1 reload ran: FD2.TMP rewritten to its full
     * 0x32A00 bytes. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* (2) exactly page 1 was shown: one glyph, idx 0x51 (= 0x50 + page 1). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x51);

    /* (3) exactly one recruit: char 0x1B appended to the menu roster, team =
     * player, char_id copied to both +7 and +8. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 1);
    roster = (uint8 *)data_fd2_shared_menu_party_roster_buffer_ptr;
    ASSERT_EQ((long)roster[0x06], 2L);      /* team = player */
    ASSERT_EQ((long)roster[0x07], (long)0x1B);   /* char_id = reinforcement 0x1B */
    ASSERT_EQ((long)roster[0x08], (long)0x1B);   /* char_id combat-byte copy */

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

void run_field_chevt16_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt16\n");
    RUN_TEST(test_ch19_event2d_sets_ai_flag3_range_0x10_to_0x1f);
    RUN_TEST(test_ch19_event2e_reload1_dialog1_recruits_char0x1b);
    printf("\n");
}
