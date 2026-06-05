/*
 * unit tests for src/field/chevt1.c (part 5: handler 0D)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1..4 (chevt11.c /
 * chevt12.c / chevt13.c / chevt14.c) cover the other handlers; this part covers
 * handler 0D.
 *
 * fd2_chapter_event_handler_0d__ch15_dialog_with_state @ 0x34E90 is dispatch
 * idx 0x0D of that table — chapter 15 turn-event slot 0, fired at turn 4 /
 * phase 1. Its beat is a straight-line, no-branch sequence (no RNG, no numeric
 * computation, no CALL-return value used):
 *   - dialog page 6 is shown first;
 *   - the boss group (runtime-char slots 0x40..0x49 inclusive, 10 chars) is
 *     armed for AI mode 3 in two steps — every slot's AI param byte
 *     combat_aux_block[0xE] (struct offset 0x35) is preset to 0, then the low
 *     nibble of combat_aux_block[0xD] (struct offset 0x34) is set to 3 across
 *     the same range via the real fd2_set_combat_aux_block_byte_d_low4_for_char_range
 *     (which masks the byte to (old & 0xF0) | new_val, so only the low nibble
 *     moves and the high nibble survives);
 *   - the per-event AI/dialog control flag (low 4 bits of combat_aux_block[0xD])
 *     is disarmed by writing low nibble 0 across chars 0x23..0x31 (15 chars),
 *     also via the real range setter.
 *
 * All three callees are REAL. Two observable, deterministic contracts are
 * checked in one comprehensive test (the handler has no branch, no gate):
 *   - fd2_display_dialog_scene runs FOR REAL on a per-page-distinct-glyph
 *     program (page p -> single TEXT glyph idx 0x50+p, then END), so a correct
 *     page-6 dispatch must emit exactly one glyph with idx 0x56 and any wrong
 *     page fails loudly. The glyph blit is the testglob recorder
 *     (g_dlg_glyph_calls / g_dlg_glyph_last_idx), making the dispatched page
 *     observable WITHOUT touching real VGA;
 *   - the AI-byte writes land exactly on the documented ranges and bytes: the
 *     [0xE]=0 loop spans 0x40..0x49, the [0xD] low-nibble=3 set spans 0x40..0x49,
 *     and the [0xD] low-nibble=0 clear spans 0x23..0x31; every range boundary is
 *     exact (the char just below/above each range is untouched) and each
 *     in-slot neighbour byte survives (single-byte / low-nibble-only writes).
 *
 * handler_0d writes runtime-char slots up to index 0x49, beyond the shared
 * g_ev_rc[0x48] fixture (max index 0x47). This leaf therefore points
 * data_fd2_battle_runtime_char_array_ptr at a private oversized array
 * (g_ev0d_rc[0x4A], indices 0..0x49) for the duration of the test, restoring it
 * afterwards. The base safe env (dialog VM workspace, empty party, gated HUD,
 * throttled palette, empty keyboard buffer) still comes from ev_install_safe_env().
 *
 * The pure blit/display side effects (the real glyph render pixels) are deferred
 * to Phase 9 integration; only the dispatched-page identity is asserted here.
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
 *   [0..0x10]      header words: page p -> byte offset of its (glyph, END) pair
 *   [0x11+2*p]     page p glyph (0x50+p)
 *   [0x12+2*p]     page p END (-1)                                          */
static int16 g_ev0d_dlg[0x11 + 2 * 0x11];

/* private oversized runtime-char array: handler_0d writes index 0x49 (boss
 * group 0x40..0x49), one past the shared g_ev_rc[0x48] fixture. Sized 0x4A so
 * index 0x49 (= 73 < 74) is in bounds. */
static runtime_char g_ev0d_rc[0x4A];
static runtime_char *g_ev0d_saved_rc_ptr;

static void ev0d_install_env(void)
{
    int p;

    /* shared ch25-style real-portrait-reload / dialog-VM env (empty party,
     * gated HUD, throttled palette, real compositor workspace, empty keyboard
     * buffer). It installs an immediate-END dialog program and points the
     * runtime-char pointer at g_ev_rc; both are overridden below. */
    ev_install_safe_env();

    /* per-page (glyph, END) pairs start right after the 0x11 header words, so
     * the page the handler selects is identifiable by the recorded glyph idx. */
    for (p = 0; p <= 0x10; p++) {
        g_ev0d_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev0d_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev0d_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev0d_dlg;

    /* no portrait open on entry, so the dialog END path skips the close
     * sequence and returns at once (one glyph for the dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* point the runtime-char pointer at the private oversized array so the
     * 0x40..0x49 writes stay in bounds. */
    memset(g_ev0d_rc, 0, sizeof(g_ev0d_rc));
    g_ev0d_saved_rc_ptr = data_fd2_battle_runtime_char_array_ptr;
    data_fd2_battle_runtime_char_array_ptr = g_ev0d_rc;

    /* reset the glyph recorder so the per-test count is clean
     * (ev_install_safe_env does not touch it). */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

static void ev0d_restore_env(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_ev0d_saved_rc_ptr;
    ev_restore_rc_ptr();
}

/* Seed every byte the handler touches (and the boundary guards) to distinct
 * sentinels so each write is independently witnessed:
 *   - combat_aux_block[0xE] of 0x40..0x49 -> 0xEE (a non-zero a real clear
 *     must drive to 0);
 *   - combat_aux_block[0xD] of 0x40..0x49 and 0x23..0x31 -> 0xA5 (high nibble
 *     0xA survives, low nibble 0x5 must move to 3 / 0);
 *   - in-slot neighbour bytes [0xC]/[0xD] (for the [0xE] writes) and [0xC]/[0xE]
 *     (for the [0xD] writes), plus the chars just below/above each range. */
static void ev0d_seed(void)
{
    int i;

    for (i = 0x40; i <= 0x49; i++) {
        g_ev0d_rc[i].combat_aux_block[0xE] = 0xEE;
        g_ev0d_rc[i].combat_aux_block[0xD] = 0xA5;
        g_ev0d_rc[i].combat_aux_block[0xC] = 0xCC;   /* in-slot neighbour guard */
    }
    for (i = 0x23; i <= 0x31; i++) {
        g_ev0d_rc[i].combat_aux_block[0xD] = 0xA5;
        g_ev0d_rc[i].combat_aux_block[0xC] = 0xCC;   /* in-slot neighbour guard */
        g_ev0d_rc[i].combat_aux_block[0xE] = 0xEE;   /* in-slot neighbour guard */
    }

    /* chars just outside each range: must stay at their seeded sentinels. */
    g_ev0d_rc[0x3F].combat_aux_block[0xD] = 0xA5;    /* below boss range */
    g_ev0d_rc[0x3F].combat_aux_block[0xE] = 0xEE;
    g_ev0d_rc[0x22].combat_aux_block[0xD] = 0xA5;    /* below clear range */
    g_ev0d_rc[0x32].combat_aux_block[0xD] = 0xA5;    /* above clear range */
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: dialog page 6, then arm AI mode 3 for the
 * boss range 0x40..0x49 ([0xE] preset 0, [0xD] low nibble 3) and disarm the
 * AI flag of chars 0x23..0x31 ([0xD] low nibble 0). All deterministic and
 * observable; checked in one pass.
 * ---------------------------------------------------------------- */
static void test_ch15_event0d_page6_arms_boss_ai3_and_clears_midtier(void)
{
    ev0d_install_env();
    ev0d_seed();

    fd2_chapter_event_handler_0d__ch15_dialog_with_state(0);

    /* (1) exactly page 6 was shown: one glyph, idx 0x56 (= 0x50 + page 6). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x56);

    /* (2) combat_aux_block[0xE] cleared to 0 across the whole boss range,
     * boundaries + an interior char. */
    ASSERT_EQ((long)g_ev0d_rc[0x40].combat_aux_block[0xE], 0L);
    ASSERT_EQ((long)g_ev0d_rc[0x45].combat_aux_block[0xE], 0L);
    ASSERT_EQ((long)g_ev0d_rc[0x49].combat_aux_block[0xE], 0L);

    /* (3) combat_aux_block[0xD] low nibble set to 3, high nibble preserved
     * (0xA5 -> 0xA3) across the boss range. */
    ASSERT_EQ((long)g_ev0d_rc[0x40].combat_aux_block[0xD], (long)0xA3);
    ASSERT_EQ((long)g_ev0d_rc[0x45].combat_aux_block[0xD], (long)0xA3);
    ASSERT_EQ((long)g_ev0d_rc[0x49].combat_aux_block[0xD], (long)0xA3);

    /* (4) combat_aux_block[0xD] low nibble cleared to 0, high nibble preserved
     * (0xA5 -> 0xA0) across the mid-tier range. */
    ASSERT_EQ((long)g_ev0d_rc[0x23].combat_aux_block[0xD], (long)0xA0);
    ASSERT_EQ((long)g_ev0d_rc[0x2A].combat_aux_block[0xD], (long)0xA0);
    ASSERT_EQ((long)g_ev0d_rc[0x31].combat_aux_block[0xD], (long)0xA0);

    /* (5) range boundaries are exact: the char just below the boss range and
     * just below/above the clear range keep their seeded sentinel. */
    ASSERT_EQ((long)g_ev0d_rc[0x3F].combat_aux_block[0xD], (long)0xA5);
    ASSERT_EQ((long)g_ev0d_rc[0x3F].combat_aux_block[0xE], (long)0xEE);
    ASSERT_EQ((long)g_ev0d_rc[0x22].combat_aux_block[0xD], (long)0xA5);
    ASSERT_EQ((long)g_ev0d_rc[0x32].combat_aux_block[0xD], (long)0xA5);

    /* (6) in-slot neighbour bytes survive: the [0xE] writes did not touch [0xC]
     * or [0xD] beyond their own contract, and the [0xD] writes did not touch
     * [0xC] or [0xE]. (Boss [0xD] is checked at 0xA3 above; here verify [0xC]
     * survives at both boundaries and the clear-range neighbours survive.) */
    ASSERT_EQ((long)g_ev0d_rc[0x40].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev0d_rc[0x49].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev0d_rc[0x23].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev0d_rc[0x23].combat_aux_block[0xE], (long)0xEE);
    ASSERT_EQ((long)g_ev0d_rc[0x31].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev0d_rc[0x31].combat_aux_block[0xE], (long)0xEE);

    ev0d_restore_env();
}

void run_field_chevt15_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt15\n");
    RUN_TEST(test_ch15_event0d_page6_arms_boss_ai3_and_clears_midtier);
    printf("\n");
}
