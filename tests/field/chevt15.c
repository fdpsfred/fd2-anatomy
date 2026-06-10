/*
 * unit tests for src/field/chevt1.c (part 5: handlers 0D, 12, 26, 27, 28)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1..4 (chevt11.c /
 * chevt12.c / chevt13.c / chevt14.c) cover the other handlers; this part covers
 * handlers 0D, 12 and 26 (chapter-15 turn-event slots), 27 (unref drop) and 28
 * (chapter-17 turn-event slot 0).
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

/* ================================================================
 * handler 12 — fd2_chapter_event_handler_12__ch15_dialog_with_state @ 0x34F02
 *   chapter 15 turn-event slot 1, fired at turn 9 / phase 0. Its beat is a
 *   straight-line, no-branch sequence (no RNG, no numeric computation, no
 *   CALL-return value used):
 *     - dialog page 8 is shown;
 *     - the per-event AI/dialog control flag (low nibble of combat_aux_block[0xD],
 *       struct offset 0x34) is disarmed by writing low nibble 0 across chars
 *       0x10..0x22 inclusive (19 chars) via the real range setter (which masks
 *       the byte to (old & 0xF0) | new_val, so only the low nibble moves and the
 *       high nibble survives).
 *
 * Both callees are REAL. The 0x10..0x22 writes fit the shared g_ev_rc[0x48]
 * fixture (max index 0x47), so no private oversized array is needed; only the
 * dialog program and glyph recorder are overridden on top of ev_install_safe_env.
 * ================================================================ */

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), identical layout to g_ev0d_dlg, so the dispatched page is
 * identifiable by the recorded glyph idx. */
static int16 g_ev12_dlg[0x11 + 2 * 0x11];

static void ev12_install_env(void)
{
    int p;

    /* shared real dialog-VM / safe env; points the runtime-char pointer at the
     * shared g_ev_rc fixture (g_ev_rc[0x48] covers the 0x10..0x22 writes) and
     * installs an immediate-END dialog program — both the program and the glyph
     * recorder are overridden below. */
    ev_install_safe_env();

    for (p = 0; p <= 0x10; p++) {
        g_ev12_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev12_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev12_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev12_dlg;

    /* no portrait open on entry, so the dialog END path skips the close
     * sequence and returns at once (one glyph for the dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* Seed combat_aux_block[0xD] of every touched char (and the boundary guards) to
 * 0xA5 (high nibble 0xA must survive, low nibble 0x5 must move to 0), and the
 * in-slot neighbour bytes [0xC]/[0xE] to distinct sentinels so the low-nibble-
 * only write is independently witnessed. */
static void ev12_seed(void)
{
    int i;

    for (i = 0x10; i <= 0x22; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xA5;
        g_ev_rc[i].combat_aux_block[0xC] = 0xCC;   /* in-slot neighbour guard */
        g_ev_rc[i].combat_aux_block[0xE] = 0xEE;   /* in-slot neighbour guard */
    }

    /* chars just outside the range: must stay at their seeded sentinel. */
    g_ev_rc[0x0F].combat_aux_block[0xD] = 0xA5;    /* below range */
    g_ev_rc[0x23].combat_aux_block[0xD] = 0xA5;    /* above range */
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: dialog page 8, then disarm the AI flag of
 * chars 0x10..0x22 ([0xD] low nibble 0). All deterministic and observable;
 * checked in one pass.
 * ---------------------------------------------------------------- */
static void test_ch15_event12_page8_clears_ai_flag_0x10_to_0x22(void)
{
    ev12_install_env();
    ev12_seed();

    fd2_chapter_event_handler_12__ch15_dialog_with_state(0);

    /* (1) exactly page 8 was shown: one glyph, idx 0x58 (= 0x50 + page 8). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x58);

    /* (2) combat_aux_block[0xD] low nibble cleared to 0, high nibble preserved
     * (0xA5 -> 0xA0) across the whole range — boundaries + an interior char. */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xD], (long)0xA0);
    ASSERT_EQ((long)g_ev_rc[0x19].combat_aux_block[0xD], (long)0xA0);
    ASSERT_EQ((long)g_ev_rc[0x22].combat_aux_block[0xD], (long)0xA0);

    /* (3) range boundaries are exact: the chars just below/above the range keep
     * their seeded sentinel. */
    ASSERT_EQ((long)g_ev_rc[0x0F].combat_aux_block[0xD], (long)0xA5);
    ASSERT_EQ((long)g_ev_rc[0x23].combat_aux_block[0xD], (long)0xA5);

    /* (4) in-slot neighbour bytes survive: the low-nibble-only [0xD] write did
     * not touch [0xC] or [0xE]. */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xE], (long)0xEE);
    ASSERT_EQ((long)g_ev_rc[0x22].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev_rc[0x22].combat_aux_block[0xE], (long)0xEE);

    ev_restore_rc_ptr();
}

/* ================================================================
 * handler 26 — fd2_chapter_event_handler_26__ch15_dialog @ 0x34F42
 *   chapter 15 turn-event slot 2, fired at turn 7 / phase 0. Its beat is a
 *   straight-line, no-branch sequence (no RNG, no numeric computation, no
 *   CALL-return value used):
 *     - reload portrait set 1 via the REAL fd2_load_chapter_portraits_and_dump_tmp,
 *       which re-reads FDICON.B24 + FDFIELD.DAT[chapter*3+2] into a fresh field
 *       buffer and rewrites the 0x32A00-byte FD2.TMP swap file;
 *     - dialog page 10 (0xA) is shown.
 *
 * Both callees are REAL — this is the page-10 twin of handler_20 (which reloads
 * portrait set 1 and shows page 1). Two observable, deterministic contracts are
 * checked:
 *   - the real portrait set 1 reload rewrote FD2.TMP to its full 0x32A00 bytes
 *     (proving the whole real reload callee chain ran against the staged real
 *     game files);
 *   - fd2_display_dialog_scene runs FOR REAL on a per-page-distinct-glyph program
 *     (page p -> single TEXT glyph idx 0x50+p, then END), so a correct page-10
 *     dispatch must emit exactly one glyph with idx 0x5A (= 0x50 + 0xA) and any
 *     wrong page fails loudly. The glyph blit is the testglob recorder
 *     (g_dlg_glyph_calls / g_dlg_glyph_last_idx), making the dispatched page
 *     observable WITHOUT touching real VGA.
 *
 * The 0x10-page distinct-glyph program reaches page 0xA (its glyph word is at
 * index 0x11 + 2*0xA, well within the 0x11 + 2*0x11 array). No runtime-char
 * writes occur, so the shared g_ev_rc fixture is left as ev_install_safe_env
 * sets it; only the dialog program and glyph recorder are overridden.
 *
 * The pure blit/display side effects (the real glyph render pixels, portrait
 * pixels) are deferred to Phase 9 integration; only the real FD2.TMP rewrite and
 * the dispatched-page identity are asserted here.
 * ================================================================ */

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), identical layout to g_ev0d_dlg, so the dispatched page is
 * identifiable by the recorded glyph idx. */
static int16 g_ev26_dlg[0x11 + 2 * 0x11];

static void ev26_install_env(void)
{
    int p;

    /* shared ch25-style real-portrait-reload / dialog-VM safe env (empty party,
     * gated HUD, throttled palette, real compositor workspace, empty keyboard
     * buffer, alloc_offset 0, current_chapter_id 4, fresh field buffer). It
     * installs an immediate-END dialog program, overridden below. */
    ev_install_safe_env();

    for (p = 0; p <= 0x10; p++) {
        g_ev26_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev26_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev26_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev26_dlg;

    /* no portrait open on entry, so the dialog END path skips the close
     * sequence and returns at once (one glyph for the dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: reload portrait set 1 (real) then show
 * dialog page 10 via the real dialog VM. Observable, deterministic contract:
 * the real portrait reload rewrote FD2.TMP to its full 0x32A00 bytes, exactly
 * one glyph is emitted and it is page 10's glyph (idx 0x5A) — proving the
 * handler dispatches page 10 (not any other page) — and the whole real callee
 * chain runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch15_event26_reloads_portrait1_and_shows_dialog_page10(void)
{
    ev26_install_env();

    /* clear any stale swap file so the size check proves THIS reload wrote it
     * (FD2.TMP is the loader's output scratch file, not a real game input). */
    remove("FD2.TMP");

    fd2_chapter_event_handler_26__ch15_dialog(0);

    /* (1) the real portrait set 1 reload ran: FD2.TMP rewritten to its full
     * 0x32A00 bytes. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* (2) exactly page 10 was shown: one glyph, idx 0x5A (= 0x50 + page 0xA). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x5A);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * handler 27 — fd2_chapter_event_handler_27__unref_drop @ 0x34F74
 *   dispatch idx 0x27 of the per-event handler table at 0x51B91. No chapter
 *   FDFIELD hook references this slot (unref / possibly cut content); it runs
 *   the tile-step ABI (dispatch arg = stepping char id = drop recipient). Its
 *   beat is a straight-line, no-branch sequence (no RNG, no numeric computation,
 *   no CALL-return value used):
 *     - copy the inline 3-byte drop-entry blob
 *       data_fd2_chapter_event_handler_27_drop_entry_inline (= {0x00, 0xD3,
 *       0x00}: type 0 / item id 0xD3) onto a local;
 *     - hand it as a one-entry array to the REAL fd2_process_battle_drop_entries
 *       (recipient = stepping char id, count = 1, &blob);
 *     - show dialog page 0xB.
 *
 * fd2_process_battle_drop_entries only opens the drop-reward UI when the
 * recipient is on the player team (team == 2); for an enemy/NPC recipient it
 * reads the entry and returns at once (item dialog suppressed). The full
 * player-team item-grant UI (real "你獲得 X，要嗎？" dialog page 0x1B0 on
 * data_fd2_all_game_text_ptr + portrait/paint/wait/close) is a pure display
 * side effect deferred to Phase 9 integration; this suite drives the non-player
 * recipient (team != 2) early-return path so the real drop processor runs end to
 * end through its team gate without the heavy UI, then witnesses the handler's
 * own page-0xB dialog. The blob's byte-exactness (the only "computation" — the
 * table copy source) is asserted directly against the const definition.
 * ================================================================ */

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), identical layout to g_ev0d_dlg, so the dispatched page is
 * identifiable by the recorded glyph idx. */
static int16 g_ev27_dlg[0x11 + 2 * 0x11];

static void ev27_install_env(void)
{
    int p;

    /* shared real dialog-VM / safe env; points the runtime-char pointer at the
     * shared g_ev_rc fixture and installs an immediate-END dialog program — both
     * the program and the glyph recorder are overridden below. */
    ev_install_safe_env();

    for (p = 0; p <= 0x10; p++) {
        g_ev27_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev27_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev27_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev27_dlg;

    /* no portrait open on entry, so the dialog END path skips the close
     * sequence and returns at once (one glyph for the dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * The inline drop-entry blob baked into the binary at 0x52742 must be exactly
 * {type 0, value 0x00D3} = item id 0xD3 — this is the source the handler's
 * inline copy reads, so its byte-exactness is the one correctness gate for the
 * copy semantics.
 * ---------------------------------------------------------------- */
static void test_ch15_event27_drop_entry_blob_is_item_0xd3(void)
{
    ASSERT_EQ((long)data_fd2_chapter_event_handler_27_drop_entry_inline[0],
              (long)0x00);   /* entry type 0 = ITEM pickup */
    ASSERT_EQ((long)data_fd2_chapter_event_handler_27_drop_entry_inline[1],
              (long)0xD3);   /* value low byte */
    ASSERT_EQ((long)data_fd2_chapter_event_handler_27_drop_entry_inline[2],
              (long)0x00);   /* value high byte (LE uint16 0x00D3 = item id 0xD3) */
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: forward the inline drop entry to the REAL
 * fd2_process_battle_drop_entries with the stepping char as recipient, then show
 * dialog page 0xB. Driven with a non-player recipient (team != 2) so the drop
 * processor reads the entry and returns at its team gate (no item UI); the only
 * dialog that runs is the handler's own page-0xB call. Observable, deterministic
 * contract: exactly one glyph is emitted and it is page 0xB's glyph (idx 0x5B),
 * proving the handler reaches its page-0xB dispatch after the real drop call
 * returns — and that the drop processor emitted no dialog of its own.
 * ---------------------------------------------------------------- */
static void test_ch15_event27_forwards_drop_then_shows_dialog_page0xb(void)
{
    ev27_install_env();

    /* recipient = stepping char id 5; mark it a non-player (npc) unit so the
     * real drop processor hits its team!=2 gate and returns after reading the
     * entry (drop-reward UI suppressed). The other slots stay zeroed (team 0 =
     * enemy), so a mis-targeted recipient would also be non-player. */
    g_ev_rc[5].team = 1;

    fd2_chapter_event_handler_27__unref_drop(5);

    /* exactly page 0xB was shown: one glyph, idx 0x5B (= 0x50 + page 0xB). The
     * single-glyph count also proves the drop processor ran no dialog of its own
     * (it returned at the team gate). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x5B);

    ev_restore_rc_ptr();
}

/* ================================================================
 * handler 28 — fd2_chapter_event_handler_28__ch17_dialog_with_state @ 0x34FCB
 *   chapter 17 turn-event slot 0, fired at turn 4 / phase 1. Its beat is a
 *   straight-line, no-branch sequence (no RNG, no numeric computation, no
 *   CALL-return value used):
 *     - reload portrait set 2 via the REAL fd2_load_chapter_portraits_and_dump_tmp
 *       (re-reads FDICON.B24 + FDFIELD.DAT and rewrites the 0x32A00-byte FD2.TMP
 *       swap file);
 *     - pan the camera+cursor to window origin (0x11, 0x25) via the REAL
 *       fd2_pan_cursor_and_window (scrolls battle_window_origin_x/y one step per
 *       frame until it reaches the target, moving cursor_world_x/y in lockstep);
 *     - show dialog page 1 (borrowed +0x0F shared body of
 *       fd2_show_chapter_dialog_with_portrait_set_1, page=1).
 *
 * All three callees are REAL. Three observable, deterministic contracts are
 * checked in one comprehensive test (the handler has no branch, no gate):
 *   - the real portrait set 2 reload rewrote FD2.TMP to its full 0x32A00 bytes
 *     (proving the whole real reload callee chain ran against the staged real
 *     game files);
 *   - the pan drove battle_window_origin_x/y to EXACTLY the target (0x11, 0x25)
 *     and moved cursor_world_x/y by the same signed delta as the window. Seeding
 *     origin_x ABOVE the target and origin_y BELOW it witnesses both the
 *     decrement and the increment pan branch in one pass;
 *   - fd2_display_dialog_scene runs FOR REAL on a per-page-distinct-glyph program
 *     (page p -> single TEXT glyph idx 0x50+p, then END), so a correct page-1
 *     dispatch must emit exactly one glyph with idx 0x51 (= 0x50 + 1) and any
 *     wrong page fails loudly. The glyph blit is the testglob recorder
 *     (g_dlg_glyph_calls / g_dlg_glyph_last_idx), making the dispatched page
 *     observable WITHOUT touching real VGA.
 *
 * The handler writes no runtime-char slots, so the shared g_ev_rc fixture is
 * left as ev_install_safe_env sets it (the empty active party also makes the
 * pan's per-frame fd2_composite_battle_frame iterate zero char overlays). Only
 * the dialog program, glyph recorder and the pan start state are overridden.
 *
 * The pure blit/display side effects (the real glyph render pixels, portrait
 * pixels, per-frame composite pixels) are deferred to Phase 9 integration; only
 * the real FD2.TMP rewrite, the final pan position + cursor delta, and the
 * dispatched-page identity are asserted here.
 * ================================================================ */

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), identical layout to g_ev0d_dlg, so the dispatched page is
 * identifiable by the recorded glyph idx. */
static int16 g_ev28_dlg[0x11 + 2 * 0x11];

static void ev28_install_env(void)
{
    int p;

    /* shared ch25-style real-portrait-reload / dialog-VM / compositor safe env
     * (empty party, gated HUD, throttled palette, real compositor workspace,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh field
     * buffer). It installs an immediate-END dialog program, overridden below. */
    ev_install_safe_env();

    for (p = 0; p <= 0x10; p++) {
        g_ev28_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev28_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev28_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev28_dlg;

    /* no portrait open on entry, so the dialog END path skips the close
     * sequence and returns at once (one glyph for the dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: reload portrait set 2 (real), pan the camera
 * to window origin (0x11, 0x25) (real), then show dialog page 1 via the real
 * dialog VM. Observable, deterministic contract: FD2.TMP rewritten to its full
 * 0x32A00 bytes; the window origin lands EXACTLY on (0x11, 0x25) with the cursor
 * moved by the same signed delta; exactly one glyph is emitted and it is page
 * 1's glyph (idx 0x51) — proving the handler dispatches page 1 (not any other
 * page) — and the whole real callee chain runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch17_event28_reload2_pan_and_shows_dialog_page1(void)
{
    ev28_install_env();

    /* clear any stale swap file so the size check proves THIS reload wrote it
     * (FD2.TMP is the loader's output scratch file, not a real game input). */
    remove("FD2.TMP");

    /* seed the pan start state: window origin_x ABOVE the target 0x11 (so the
     * pan decrements 3 steps) and origin_y BELOW the target 0x25 (so it
     * increments 2 steps) — witnessing both pan branches. The cursor must move
     * one-for-one with the window, so it ends at start_cursor + window_delta. */
    data_fd2_battle_view_window_origin_x = 0x14;   /* 3 above target 0x11 */
    data_fd2_battle_view_window_origin_y = 0x23;   /* 2 below target 0x25 */
    data_fd2_battle_cursor_world_x = 0x80;
    data_fd2_battle_cursor_world_y = 0x90;

    fd2_chapter_event_handler_28__ch17_dialog_with_state(0);

    /* (1) the real portrait set 2 reload ran: FD2.TMP rewritten to its full
     * 0x32A00 bytes. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* (2) the pan reached EXACTLY the target window origin (0x11, 0x25). */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, (long)0x11);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, (long)0x25);

    /* (3) the cursor moved in lockstep with the window: origin_x stepped down by
     * 3 (0x14 -> 0x11) so cursor_x = 0x80 - 3 = 0x7D; origin_y stepped up by 2
     * (0x23 -> 0x25) so cursor_y = 0x90 + 2 = 0x92. */
    ASSERT_EQ((long)data_fd2_battle_cursor_world_x, (long)0x7D);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_y, (long)0x92);

    /* (4) exactly page 1 was shown: one glyph, idx 0x51 (= 0x50 + page 1). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x51);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * handler 29 — fd2_chapter_event_handler_29__unref_drop @ 0x34FF0
 *   dispatch idx 0x29 of the per-event handler table at 0x51B91. No chapter
 *   FDFIELD hook references this slot (unref / possibly cut content); it runs
 *   the tile-step ABI (dispatch arg = stepping char id = drop recipient). Its
 *   beat is a straight-line, no-branch sequence (no RNG, no numeric computation,
 *   no CALL-return value used), and unlike handler 27 it BRACKETS the drop with
 *   two dialog pages:
 *     - show dialog page 3;
 *     - copy the inline 3-byte drop-entry blob
 *       data_fd2_chapter_event_handler_29_drop_entry_inline (= {0x00, 0xD5,
 *       0x00}: type 0 / item id 0xD5) onto a local;
 *     - hand it as a one-entry array to the REAL fd2_process_battle_drop_entries
 *       (recipient = stepping char id, count = 1, &blob);
 *     - show dialog page 4 (via the borrowed alt_43 shared tail hosted in
 *       handler 27).
 *
 * fd2_process_battle_drop_entries only opens the drop-reward UI when the
 * recipient is on the player team (team == 2); for an enemy/NPC recipient it
 * reads the entry and returns at once (item dialog suppressed). The full
 * player-team item-grant UI (real "你獲得 X，要嗎？" dialog page 0x1B0 on
 * data_fd2_all_game_text_ptr + portrait/paint/wait/close) is a pure display
 * side effect deferred to Phase 9 integration; this suite drives the non-player
 * recipient (team != 2) early-return path so the real drop processor runs end to
 * end through its team gate without the heavy UI, then witnesses the handler's
 * own page-3 + page-4 dialogs in order. The blob's byte-exactness (the only
 * "computation" — the table copy source) is asserted directly against the const
 * definition.
 * ================================================================ */

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), identical layout to g_ev27_dlg, so the dispatched page is
 * identifiable by the recorded glyph idx. */
static int16 g_ev29_dlg[0x11 + 2 * 0x11];

static void ev29_install_env(void)
{
    int p;

    /* shared real dialog-VM / safe env; points the runtime-char pointer at the
     * shared g_ev_rc fixture and installs an immediate-END dialog program — both
     * the program and the glyph recorder are overridden below. */
    ev_install_safe_env();

    for (p = 0; p <= 0x10; p++) {
        g_ev29_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev29_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev29_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev29_dlg;

    /* no portrait open on entry, so the dialog END path skips the close
     * sequence and returns at once (one glyph for each dispatched page). */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * The inline drop-entry blob baked into the binary at 0x52745 must be exactly
 * {type 0, value 0x00D5} = item id 0xD5 — this is the source the handler's
 * inline copy reads, so its byte-exactness is the one correctness gate for the
 * copy semantics.
 * ---------------------------------------------------------------- */
static void test_ch15_event29_drop_entry_blob_is_item_0xd5(void)
{
    ASSERT_EQ((long)data_fd2_chapter_event_handler_29_drop_entry_inline[0],
              (long)0x00);   /* entry type 0 = ITEM pickup */
    ASSERT_EQ((long)data_fd2_chapter_event_handler_29_drop_entry_inline[1],
              (long)0xD5);   /* value low byte */
    ASSERT_EQ((long)data_fd2_chapter_event_handler_29_drop_entry_inline[2],
              (long)0x00);   /* value high byte (LE uint16 0x00D5 = item id 0xD5) */
}

/* ----------------------------------------------------------------
 * The handler fires its beat in order: dialog page 3, forward the inline drop
 * entry to the REAL fd2_process_battle_drop_entries with the stepping char as
 * recipient, then dialog page 4. Driven with a non-player recipient (team != 2)
 * so the drop processor reads the entry and returns at its team gate (no item
 * UI); the only dialogs that run are the handler's own page-3 and page-4 calls.
 * Observable, deterministic contract: exactly TWO glyphs are emitted and the
 * LAST is page 4's glyph (idx 0x54) — proving the handler reaches its page-4
 * dispatch AFTER the real drop call returns, that the page-3 dialog also ran
 * (count includes it), and that the drop processor emitted no dialog of its own
 * (otherwise the count would exceed 2 or the last glyph would not be page 4's).
 * ---------------------------------------------------------------- */
static void test_ch15_event29_dialog3_drop_then_dialog4(void)
{
    ev29_install_env();

    /* recipient = stepping char id 5; mark it a non-player (npc) unit so the
     * real drop processor hits its team!=2 gate and returns after reading the
     * entry (drop-reward UI suppressed). The other slots stay zeroed (team 0 =
     * enemy), so a mis-targeted recipient would also be non-player. */
    g_ev_rc[5].team = 1;

    fd2_chapter_event_handler_29__unref_drop(5);

    /* exactly two pages were shown (page 3 then page 4): the count is 2 and the
     * LAST glyph is idx 0x54 (= 0x50 + page 4). The count of 2 also proves the
     * drop processor ran no dialog of its own (it returned at the team gate). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x54);

    ev_restore_rc_ptr();
}

void run_field_chevt15_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt15\n");
    RUN_TEST(test_ch15_event0d_page6_arms_boss_ai3_and_clears_midtier);
    RUN_TEST(test_ch15_event12_page8_clears_ai_flag_0x10_to_0x22);
    RUN_TEST(test_ch15_event26_reloads_portrait1_and_shows_dialog_page10);
    RUN_TEST(test_ch15_event27_drop_entry_blob_is_item_0xd3);
    RUN_TEST(test_ch15_event27_forwards_drop_then_shows_dialog_page0xb);
    RUN_TEST(test_ch17_event28_reload2_pan_and_shows_dialog_page1);
    RUN_TEST(test_ch15_event29_drop_entry_blob_is_item_0xd5);
    RUN_TEST(test_ch15_event29_dialog3_drop_then_dialog4);
    printf("\n");
}
