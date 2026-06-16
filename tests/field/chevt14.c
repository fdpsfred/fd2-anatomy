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
    data_fd2_current_chapter_text = (uint32)g_ev20_dlg;

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
 * fd2_chapter_event_handler_05__ch13_thunk @ 0x34D68 (dispatch idx 0x05) —
 * chapter 13 (哈斯米爾之戰) turn-event slot. In the binary it is a 7-byte
 * adapter stub ("PUSH 0x28; JMP 0x34BE7") that tail-jumps into the shared body
 * fd2_show_chapter_dialog_with_portrait_set_1 @ 0x34BE7, so its full effect is
 * that body: reload portrait set 1, then show dialog page 1. It is the ch13
 * entry path of the same body handler_20 falls through into, so it must produce
 * the SAME observable effect as handler_20 / the helper. A straight-line,
 * no-branch beat with no camera pan, no cutscene trigger, no state write beyond
 * the portrait reload, no RNG, no numeric computation, and no CALL-return value
 * used.
 *
 * Observable, deterministic contract (identical to handler_20): the real
 * fd2_load_chapter_portraits_and_dump_tmp(1) rewrites FD2.TMP to its full
 * 0x32A00 bytes, exactly one glyph is emitted and it is page 1's glyph (idx
 * 0x51 = 0x50 + page 1) — proving the ch13 path dispatches page 1 (not any
 * other page) — and the whole real callee chain runs to completion without
 * faulting. The pure blit/display side effects are deferred to Phase 9
 * integration.
 * ---------------------------------------------------------------- */
static void test_ch13_event05_thunk_reloads_portrait1_and_shows_page1(void)
{
    ev20_install_safe_env();

    remove("FD2.TMP");

    fd2_chapter_event_handler_05__ch13_thunk(0);

    /* the real portrait reload ran: FD2.TMP rewritten to its full 0x32A00. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* exactly page 1 was shown: one glyph, idx 0x51 (= 0x50 + page 1) — the
     * ch13 entry path lands on the same page-1 body as handler_20. */
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

/* ================================================================
 * fd2_chapter_event_handler_23__ch12_cinematic @ 0x34C76 (dispatch idx 0x23) —
 * chapter 12 turn-event slot 0, fired at turn 1 / phase 0 (when the player's
 * first turn ends the first reinforcement wave arrives and 龍劍士米亞斯多德
 * joins). A cinematic, no-dialog beat with no branch, no RNG, no numeric
 * computation, and no CALL-return value used:
 *   pan_cursor_and_window(0xC, 5);
 *   chapter_init_phase_flag = 1; load_chapter_portraits_and_dump_tmp(2);
 *   chapter_init_phase_flag = 0;
 *   cutscene_event_trigger(0x2A);
 *   clear_all_chars_facing;                                  // tail-JMP
 *
 * Every callee is a REAL emitted function driven against the shared fieldfix
 * "ch25-style real portrait reload" env, plus a zero-group cutscene script for
 * event 0x2A so the real fd2_cutscene_event_trigger composites once and returns:
 *   - fd2_load_chapter_portraits_and_dump_tmp(2) runs FOR REAL against the
 *     staged FDICON.B24 + FDFIELD.DAT (alloc_offset 0 -> empty per-record scan,
 *     current_chapter_id 4 -> valid FDFIELD index 0xE), freeing+nulling the
 *     field buffer and rewriting FD2.TMP to its full 0x32A00 bytes;
 *   - fd2_pan_cursor_and_window / fd2_composite_battle_frame run against the
 *     staged camera + compositor workspace with the empty active party;
 *   - fd2_clear_all_chars_facing iterates party_member_count (= 0) so its facing
 *     loop is a no-op and only its __delay_thunk_375b2(20) busy-wait runs.
 *
 * The observable, deterministic contract: the init-phase flag is set to 1 around
 * the reload and reset to 0 afterward, the real reload happened (field buffer
 * nulled, FD2.TMP at its full 0x32A00 size), and the whole real callee chain
 * (camera pan, real portrait reload, zero-group cutscene 0x2A, facing reset) runs
 * to completion without faulting. The pure blit/display side effects (camera pan,
 * cutscene compositing, portrait pixels) are deferred to Phase 9 integration.
 * ================================================================ */

/* zero-group cutscene script for event 0x2A: n_groups byte = 0, so the real
 * fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev23_script_2a[1] = { 0 };

static void ev23_install_safe_env(void)
{
    /* shared ch25-style real-portrait-reload env (empty party, gated HUD,
     * throttled palette, real compositor workspace, empty keyboard buffer,
     * alloc_offset 0, current_chapter_id 4, fresh field buffer, 64-slot
     * g_ev_rc). */
    ev_install_safe_env();

    /* handler_23 fires cutscene EVENT 0x2A; register its own zero-group script
     * so the real fd2_cutscene_event_trigger returns fast. */
    g_ev23_script_2a[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x2A] = g_ev23_script_2a;
}

/* ----------------------------------------------------------------
 * The handler fires its fixed ch12 turn-1 cinematic end-to-end. Observable,
 * deterministic contract: the init-phase flag is set to 1 during the real
 * portrait reload and reset to 0 afterward, the real reload runs (field buffer
 * nulled, FD2.TMP rewritten to its full 0x32A00-byte size), and the whole real
 * callee chain (camera pan, real portrait reload, zero-group cutscene 0x2A,
 * facing reset) runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch12_event23_reloads_portrait2_brackets_initphase(void)
{
    ev23_install_safe_env();

    /* perturb the init-phase flag so the handler's set-then-reset is observable
     * (it must end back at 0, not at this sentinel). */
    data_fd2_chapter_init_phase_flag = 0x55;

    remove("FD2.TMP");

    fd2_chapter_event_handler_23__ch12_cinematic(0);

    /* the flag was set to 1 around the reload and reset to 0 at the end. */
    ASSERT_EQ(data_fd2_chapter_init_phase_flag, 0);

    /* the real portrait reload ran: field buffer freed+nulled, and FD2.TMP was
     * rewritten to its full 0x32A00-byte size. */
    ASSERT_EQ(data_fd2_chapter_portrait_load_buffer, 0);
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_24__ch12_ai_ctrl @ 0x34CB3 (dispatch idx 0x24) —
 * chapter 12 turn-event slot 1, fired at turn 5 / phase 1. A single
 * straight-line write with no branch, no RNG, no numeric computation, and no
 * CALL-return value used: the AI-class byte combat_aux_block[0xD] (struct
 * offset 0x34) of character slot 0x0E is set to 0x83 (bit 7 locked + low bits
 * 0/1 selecting AI mode 3). No other side effects, no portrait reload, no
 * dialog — so FD2.TMP and the glyph recorder are not part of its contract.
 *
 * The observable, deterministic contract checked here:
 *   (1) slot 0x0E's combat_aux_block[0xD] becomes exactly 0x83 (the precise AI
 *       flag value, not merely "bit 7 set");
 *   (2) ONLY that one byte is written — the immediate in-slot neighbours of
 *       [0xD] (combat_aux_block[0xC] at struct offset 0x33, combat_aux_block[0xE]
 *       at 0x35) and the AI-class byte of the adjacent slots 0x0D / 0x0F are left
 *       untouched, proving the char index (0x0E) and struct offset (0x34) are
 *       exact with no off-by-one.
 *
 * The target byte is pre-seeded to a distinct sentinel and the guard bytes/slots
 * to another, so a correct run must overwrite exactly one byte with 0x83.
 * ================================================================ */
static void test_ch12_event24_sets_ai_flag_0x83_for_char_0e(void)
{
    ev_install_safe_env();

    /* seed the target AI-class byte to a non-0x83 sentinel so the write is
     * unambiguously observable. */
    g_ev_rc[0x0E].combat_aux_block[0xD] = 0x55;

    /* distinct guard sentinels: the immediate in-slot neighbours of [0xD] and
     * the AI-class byte of the adjacent slots must survive untouched. */
    g_ev_rc[0x0E].combat_aux_block[0xC] = 0xAA;
    g_ev_rc[0x0E].combat_aux_block[0xE] = 0xAA;
    g_ev_rc[0x0D].combat_aux_block[0xD] = 0xAA;
    g_ev_rc[0x0F].combat_aux_block[0xD] = 0xAA;

    fd2_chapter_event_handler_24__ch12_ai_ctrl(0);

    /* (1) the AI-class byte of slot 0x0E is exactly 0x83. */
    ASSERT_EQ((long)g_ev_rc[0x0E].combat_aux_block[0xD], (long)0x83);

    /* (2) nothing adjacent was disturbed (precise single-byte write). */
    ASSERT_EQ((long)g_ev_rc[0x0E].combat_aux_block[0xC], (long)0xAA);
    ASSERT_EQ((long)g_ev_rc[0x0E].combat_aux_block[0xE], (long)0xAA);
    ASSERT_EQ((long)g_ev_rc[0x0D].combat_aux_block[0xD], (long)0xAA);
    ASSERT_EQ((long)g_ev_rc[0x0F].combat_aux_block[0xD], (long)0xAA);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_25__unref_major_cinematic @ 0x34CCC (dispatch idx
 * 0x25) — an unreferenced "major endgame cinematic" slot. A straight-line beat
 * with no branch, no RNG, no numeric computation, and no CALL-return value used:
 *   display_dialog_scene(page 1, ...);                          // opening dialog
 *   pan_cursor_and_window(0xF, 0x22);                           // stage A pan
 *   chapter_init_phase_flag = 1; load_chapter_portraits_and_dump_tmp(3);
 *   chapter_init_phase_flag = 0;
 *   cutscene_event_trigger(0x2B); clear_all_chars_facing;
 *   pan_cursor_and_window(0, 0x1A);                             // stage B pan
 *   chapter_init_phase_flag = 1; load_chapter_portraits_and_dump_tmp(4);
 *   chapter_init_phase_flag = 0;
 *   cutscene_event_trigger(0x2C); clear_all_chars_facing;
 *   battle_anim_phase = 1;                                      // tail-JMP 0x35C18
 *
 * Every callee is a REAL emitted function driven against the shared
 * ev20_install_safe_env() env (the same per-page-distinct-glyph dialog program
 * handler_20 uses, so the dispatched page is observable), plus zero-group
 * cutscene scripts for events 0x2B and 0x2C so the real fd2_cutscene_event_trigger
 * composites once per stage and returns:
 *   - fd2_display_dialog_scene runs FOR REAL on the per-page-distinct-glyph
 *     program (page p -> single TEXT glyph idx 0x50+p, then END), so a correct
 *     page-1 dispatch emits exactly one glyph with idx 0x51 and any wrong page
 *     fails loudly;
 *   - fd2_load_chapter_portraits_and_dump_tmp(3) then (4) each run FOR REAL
 *     against the staged FDICON.B24 + FDFIELD.DAT, the second leaving FD2.TMP at
 *     its full 0x32A00-byte size;
 *   - fd2_pan_cursor_and_window / fd2_clear_all_chars_facing run against the
 *     staged camera + compositor workspace with the empty active party (the
 *     facing loop iterates 0).
 *
 * Two deterministic, observable contracts are checked, on top of the whole real
 * callee chain running to completion without faulting:
 *   (1) exactly one glyph is emitted and it is page 1's glyph (idx 0x51),
 *       proving the opening dialog dispatches page 1 (and that the two cutscene
 *       stages, which carry no dialog, emit no further glyphs);
 *   (2) the init-phase flag ends back at 0 (set/reset bracketed the two reloads),
 *       the second real reload left FD2.TMP at its full 0x32A00 bytes, and the
 *       battle-animation phase ends at 1 (the shared-tail close at 0x35C18).
 *
 * The pure blit/display side effects (camera pan, cutscene compositing, portrait
 * pixels, glyph render path) are deferred to Phase 9 integration.
 * ================================================================ */

/* zero-group cutscene scripts for events 0x2B and 0x2C: n_groups byte = 0, so the
 * real fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev25_script_2b[1] = { 0 };
static uint8 g_ev25_script_2c[1] = { 0 };

static void ev25_install_safe_env(void)
{
    /* per-page-distinct-glyph dialog program + ch25-style real-portrait-reload
     * env (empty party, gated HUD, throttled palette, real compositor workspace,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh field
     * buffer, 64-slot g_ev_rc); also resets the glyph recorder. */
    ev20_install_safe_env();

    /* handler_25 fires cutscene EVENTS 0x2B and 0x2C; register their own
     * zero-group scripts so the real fd2_cutscene_event_trigger returns fast. */
    g_ev25_script_2b[0] = 0;
    g_ev25_script_2c[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x2B] = g_ev25_script_2b;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x2C] = g_ev25_script_2c;
}

/* ----------------------------------------------------------------
 * The handler fires its fixed major-cinematic beat end-to-end. Observable,
 * deterministic contract: exactly page 1 is shown (one glyph, idx 0x51), the
 * init-phase flag is set/reset around the two reloads and ends at 0, the second
 * real reload left FD2.TMP at its full 0x32A00 bytes, the battle-animation phase
 * ends at 1, and the whole real callee chain (opening dialog, two camera pans,
 * two real portrait reloads, zero-group cutscenes 0x2B/0x2C, two facing resets)
 * runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_event25_dialog_page1_two_stage_cinematic(void)
{
    ev25_install_safe_env();

    /* perturb the init-phase flag so the handler's set-then-reset is observable
     * (it must end back at 0, not at this sentinel). */
    data_fd2_chapter_init_phase_flag = 0x55;

    /* perturb the anim phase so the closing battle_anim_phase = 1 store is
     * observable (the env installs 0; a wrong tail would leave it != 1). */
    data_fd2_battle_anim_phase = 0x77;

    remove("FD2.TMP");

    fd2_chapter_event_handler_25__unref_major_cinematic(0);

    /* (1) exactly page 1 was shown: one glyph, idx 0x51 (= 0x50 + page 1). The
     * two cutscene stages carry no dialog, so no further glyph is emitted. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x51);

    /* (2) the init-phase flag was set to 1 around each reload and reset to 0. */
    ASSERT_EQ(data_fd2_chapter_init_phase_flag, 0);

    /* the second real portrait reload ran: FD2.TMP rewritten to its full 0x32A00. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* the shared-tail close ran: battle-animation phase flipped to 1. */
    ASSERT_EQ(data_fd2_battle_anim_phase, 1);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_07__ch13_dialog_with_state @ 0x34D72 (dispatch idx
 * 0x07) — chapter 13 turn-event slot 1, fired at turn 9 / phase 0. A
 * straight-line beat with no branch, no RNG, no numeric computation, and no
 * CALL-return value used:
 *   pan_cursor_and_window(0x1B, 5);
 *   chapter_init_phase_flag = 1; load_chapter_portraits_and_dump_tmp(2);
 *   chapter_init_phase_flag = 0;
 *   cutscene_event_trigger(0x2E); clear_all_chars_facing;
 *   display_dialog_scene(page 8, ...);                          // borrowed tail
 *
 * In the binary it prepares its own 8 PUSHes (page=8 plus the fixed dialog
 * geometry) and JMPs (0x34DC8 -> 0x34C0F) into the shared tail of
 * fd2_show_chapter_dialog_with_portrait_set_1, which supplies the 9th arg
 * (data_fd2_current_chapter_text) and performs the cdecl 0x24-byte cleanup.
 *
 * Every callee is a REAL emitted function driven against the shared
 * ev20_install_safe_env() env (the same per-page-distinct-glyph dialog program
 * handler_20 uses, so the dispatched page is observable), plus a zero-group
 * cutscene script for event 0x2E so the real fd2_cutscene_event_trigger
 * composites once and returns:
 *   - fd2_load_chapter_portraits_and_dump_tmp(2) runs FOR REAL against the
 *     staged FDICON.B24 + FDFIELD.DAT (alloc_offset 0 -> empty per-record scan,
 *     current_chapter_id 4 -> valid FDFIELD index 0xE), rewriting FD2.TMP to its
 *     full 0x32A00 bytes;
 *   - fd2_display_dialog_scene runs FOR REAL on the per-page-distinct-glyph
 *     program (page p -> single TEXT glyph idx 0x50+p, then END), so a correct
 *     page-8 dispatch emits exactly one glyph with idx 0x58 and any wrong page
 *     fails loudly;
 *   - fd2_pan_cursor_and_window / fd2_clear_all_chars_facing run against the
 *     staged camera + compositor workspace with the empty active party (the
 *     facing loop iterates 0).
 *
 * Two deterministic, observable contracts are checked, on top of the whole real
 * callee chain running to completion without faulting:
 *   (1) exactly one glyph is emitted and it is page 8's glyph (idx 0x58 = 0x50 +
 *       page 8), proving the borrowed tail dispatches page 8 (and that the
 *       cutscene stage, which carries no dialog, emits no further glyphs);
 *   (2) the init-phase flag is set to 1 around the real reload and reset to 0
 *       afterward, and the real reload left FD2.TMP at its full 0x32A00 bytes.
 *
 * The pure blit/display side effects (camera pan, cutscene compositing, portrait
 * pixels, glyph render path) are deferred to Phase 9 integration.
 * ================================================================ */

/* zero-group cutscene script for event 0x2E: n_groups byte = 0, so the real
 * fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev07_script_2e[1] = { 0 };

static void ev07_install_safe_env(void)
{
    /* per-page-distinct-glyph dialog program + ch25-style real-portrait-reload
     * env (empty party, gated HUD, throttled palette, real compositor workspace,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh field
     * buffer, 64-slot g_ev_rc); also resets the glyph recorder. */
    ev20_install_safe_env();

    /* handler_07 fires cutscene EVENT 0x2E; register its own zero-group script
     * so the real fd2_cutscene_event_trigger returns fast. */
    g_ev07_script_2e[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x2E] = g_ev07_script_2e;
}

/* ----------------------------------------------------------------
 * The handler fires its fixed ch13 turn-9 beat end-to-end. Observable,
 * deterministic contract: exactly page 8 is shown (one glyph, idx 0x58), the
 * init-phase flag is set to 1 around the real portrait set 2 reload and reset to
 * 0 afterward, the real reload left FD2.TMP at its full 0x32A00 bytes, and the
 * whole real callee chain (camera pan, real portrait reload, zero-group cutscene
 * 0x2E, facing reset, page-8 dialog) runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch13_event07_reloads_portrait2_brackets_initphase_page8(void)
{
    ev07_install_safe_env();

    /* perturb the init-phase flag so the handler's set-then-reset is observable
     * (it must end back at 0, not at this sentinel). */
    data_fd2_chapter_init_phase_flag = 0x55;

    remove("FD2.TMP");

    fd2_chapter_event_handler_07__ch13_dialog_with_state(0);

    /* (1) exactly page 8 was shown: one glyph, idx 0x58 (= 0x50 + page 8). The
     * cutscene stage carries no dialog, so no further glyph is emitted. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x58);

    /* (2) the init-phase flag was set to 1 around the reload and reset to 0. */
    ASSERT_EQ(data_fd2_chapter_init_phase_flag, 0);

    /* the real portrait reload ran: FD2.TMP rewritten to its full 0x32A00. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_08__ch13_first_time @ 0x34DCD (dispatch idx 0x08)
 * — chapter 13 tile-step event slot 0 (tile-step event_type 0x01): a
 * first-time-gated item pickup for the lord. The dispatch arg is the id of the
 * char who stepped onto the trigger tile; the beat fires only when ALL THREE
 * gates pass — the stepping char is the lord (char 0), the lord's inventory is
 * not full (fd2_count_usable_inventory_slots(0) != 8 — the one CALL-return
 * value the handler tests), and the trigger byte [0x10] of the tile-event
 * consumed-flags block is still 0. When all pass it gives the lord item id 0x59
 * (fd2_add_item_to_inventory drops it in the first empty slot), shows dialog
 * page 0xB, and writes consumed-flags byte [0x10] = 1 so it runs at most once.
 *
 * Every callee is REAL: fd2_count_usable_inventory_slots / fd2_add_item_to_inventory
 * operate on the lord's inventory_slots[] in the 64-slot g_ev_rc fixture (slot i:
 * [i*2] = flag, bit 0x80 set = empty; [i*2+1] = item id), and the real dialog VM
 * runs on the per-page-distinct-glyph program ev20_install_safe_env() installs
 * (page p -> single TEXT glyph idx 0x50+p, then END), so a correct page-0xB
 * dispatch emits exactly one glyph with idx 0x5B and any wrong page fails loudly
 * via the testglob glyph recorder. The consumed-flags block is a private
 * 0x20-byte buffer so both the read gate and the write consume are observable.
 *
 * Four cases pin every branch of the three-gate cascade:
 *   (1) all gates pass: item 0x59 lands in the lord's first empty slot
 *       (flag -> 0, item id -> 0x59), page 0xB is shown (one glyph idx 0x5B),
 *       and byte [0x10] is consumed to 1;
 *   (2) a non-lord steps (char 5): the char gate blocks everything — no glyph,
 *       byte [0x10] stays 0, the lord's inventory is untouched;
 *   (3) the lord's inventory is full (all 8 slots occupied -> count == 8): the
 *       slot-count gate blocks everything — no glyph, byte [0x10] stays 0
 *       (exercises the CALL-return-value gate, the Ghidra EAX-tracking risk
 *       point);
 *   (4) the trigger was already consumed (byte [0x10] == 1 on entry): the
 *       consumed gate blocks everything — no glyph, byte [0x10] stays 1, and no
 *       item is added even though a slot is free.
 *
 * The pure blit/display side effects (the real glyph render path) are deferred
 * to Phase 9 integration; here only the dispatched-page identity, the inventory
 * write, and the consume flag are asserted.
 * ================================================================ */

/* private 0x20-byte tile-event consumed-flags block: the handler reads/writes
 * byte [0x10] of this block via data_fd2_field_map_tile_event_consumed_flags_ptr. */
static uint8   g_ev08_consumed_flags[0x20];
static uint32  g_ev08_saved_consumed_ptr;

/* Set all 8 inventory slots of char `ci` empty (flag bit 0x80 set), so
 * fd2_count_usable_inventory_slots(ci) == 0 (not full) and the first empty slot
 * is slot 0. */
static void ev08_set_inventory_all_empty(int ci)
{
    int i;
    for (i = 0; i < 8; i++) {
        g_ev_rc[ci].inventory_slots[i * 2]     = 0x80;  /* empty */
        g_ev_rc[ci].inventory_slots[i * 2 + 1] = 0x00;  /* item id */
    }
}

/* Set all 8 inventory slots of char `ci` occupied (flag bit 0x80 clear), so
 * fd2_count_usable_inventory_slots(ci) == 8 (full). */
static void ev08_set_inventory_full(int ci)
{
    int i;
    for (i = 0; i < 8; i++) {
        g_ev_rc[ci].inventory_slots[i * 2]     = 0x00;  /* occupied */
        g_ev_rc[ci].inventory_slots[i * 2 + 1] = 0x10;  /* arbitrary held item */
    }
}

static void ev08_install_env(void)
{
    /* per-page-distinct-glyph dialog program + ch25-style real env (64-slot
     * g_ev_rc, empty party, ...); also resets the glyph recorder. */
    ev20_install_safe_env();

    /* aim the consumed-flags pointer at a private 0x20-byte buffer; start every
     * flag clear (each case seeds byte [0x10] as needed). */
    memset(g_ev08_consumed_flags, 0, sizeof(g_ev08_consumed_flags));
    g_ev08_saved_consumed_ptr = data_fd2_field_map_tile_event_consumed_flags_ptr;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ev08_consumed_flags;
}

static void ev08_restore_env(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = g_ev08_saved_consumed_ptr;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * (1) All three gates pass: the lord (char 0) steps on the tile with a free
 * inventory slot and an un-consumed trigger. The handler must give the lord
 * item 0x59 in his first empty slot (flag -> 0, item id -> 0x59), show dialog
 * page 0xB (one glyph, idx 0x5B), and consume byte [0x10] to 1.
 * ---------------------------------------------------------------- */
static void test_ch13_event08_lord_pickup_gives_item_shows_page_consumes(void)
{
    ev08_install_env();

    /* lord has empty inventory slots -> count != 8 (not full); first empty slot
     * is slot 0 so the added item lands there. */
    ev08_set_inventory_all_empty(0);

    /* trigger not yet consumed. */
    g_ev08_consumed_flags[0x10] = 0;

    fd2_chapter_event_handler_08__ch13_first_time(0);

    /* item 0x59 was added to the lord's first empty slot: flag cleared to
     * occupied-unequipped (0), item id byte set to 0x59. */
    ASSERT_EQ((long)g_ev_rc[0].inventory_slots[0], 0L);
    ASSERT_EQ((long)g_ev_rc[0].inventory_slots[1], (long)0x59);

    /* exactly page 0xB was shown: one glyph, idx 0x5B (= 0x50 + page 0xB). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x5B);

    /* the trigger was consumed. */
    ASSERT_EQ((long)g_ev08_consumed_flags[0x10], 1L);

    ev08_restore_env();
}

/* ----------------------------------------------------------------
 * (2) Char gate: a non-lord unit (char 5) steps on the tile. Even with a free
 * slot and an un-consumed trigger, NOTHING fires — no dialog, the trigger stays
 * clear, and the lord's inventory is untouched.
 * ---------------------------------------------------------------- */
static void test_ch13_event08_non_lord_step_does_nothing(void)
{
    ev08_install_env();

    /* both the lord (char 0) and the stepping char (char 5) have free slots, so
     * only the char gate can block the pickup. */
    ev08_set_inventory_all_empty(0);
    ev08_set_inventory_all_empty(5);

    g_ev08_consumed_flags[0x10] = 0;

    fd2_chapter_event_handler_08__ch13_first_time(5);

    /* no dialog dispatched. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    /* trigger untouched. */
    ASSERT_EQ((long)g_ev08_consumed_flags[0x10], 0L);

    /* the lord's first slot is still empty (no item added). */
    ASSERT_EQ((long)g_ev_rc[0].inventory_slots[0], (long)0x80);
    ASSERT_EQ((long)g_ev_rc[0].inventory_slots[1], 0L);

    ev08_restore_env();
}

/* ----------------------------------------------------------------
 * (3) Slot-count gate: the lord steps on the tile but his inventory is full
 * (all 8 slots occupied -> fd2_count_usable_inventory_slots(0) == 8). NOTHING
 * fires. This exercises the one CALL-return value the handler tests (the Ghidra
 * EAX-tracking risk point): no dialog and the trigger stays clear.
 * ---------------------------------------------------------------- */
static void test_ch13_event08_full_inventory_does_nothing(void)
{
    ev08_install_env();

    /* lord's inventory full -> count == 8 -> the != 8 gate blocks the pickup. */
    ev08_set_inventory_full(0);

    g_ev08_consumed_flags[0x10] = 0;

    fd2_chapter_event_handler_08__ch13_first_time(0);

    /* no dialog dispatched. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    /* trigger untouched. */
    ASSERT_EQ((long)g_ev08_consumed_flags[0x10], 0L);

    /* the lord's slot 0 still holds its pre-seeded occupied item (unchanged). */
    ASSERT_EQ((long)g_ev_rc[0].inventory_slots[0], 0L);
    ASSERT_EQ((long)g_ev_rc[0].inventory_slots[1], (long)0x10);

    ev08_restore_env();
}

/* ----------------------------------------------------------------
 * (4) Consumed gate: the trigger byte [0x10] is already 1 on entry. Even though
 * the lord steps on the tile with a free slot, NOTHING fires — no dialog, the
 * trigger stays 1, and no item is added.
 * ---------------------------------------------------------------- */
static void test_ch13_event08_already_consumed_does_nothing(void)
{
    ev08_install_env();

    /* lord has a free slot (so only the consumed gate can block the pickup). */
    ev08_set_inventory_all_empty(0);

    /* trigger already consumed. */
    g_ev08_consumed_flags[0x10] = 1;

    fd2_chapter_event_handler_08__ch13_first_time(0);

    /* no dialog dispatched. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    /* trigger still consumed (unchanged). */
    ASSERT_EQ((long)g_ev08_consumed_flags[0x10], 1L);

    /* the lord's first slot is still empty (no item added). */
    ASSERT_EQ((long)g_ev_rc[0].inventory_slots[0], (long)0x80);
    ASSERT_EQ((long)g_ev_rc[0].inventory_slots[1], 0L);

    ev08_restore_env();
}

/* ================================================================
 * fd2_chapter_event_handler_0a__ch14_first_time @ 0x34E3B (dispatch idx 0x0A)
 * — chapter 14 tile-step event slot 0 (tile-step event_type 0x00): a single
 * first-time-gated beat. It runs only while the trigger byte [0x10] of the
 * tile-event consumed-flags block is still 0; when it fires it (a) clears the
 * AI-class flag of the WIDEST char range in this group — the low 4 bits of
 * combat_aux_block[0xD] become 0 for every char 0x10..0x47 inclusive (56 chars)
 * via the real fd2_set_combat_aux_block_byte_d_low4_for_char_range, which masks
 * with 0xF0 and ORs the new low nibble so the HIGH nibble is preserved — (b)
 * shows dialog page 1, and (c) writes consumed-flags byte [0x10] = 1 so it runs
 * at most once.
 *
 * Both callees are REAL: the AI-flag write lands on combat_aux_block[0xD] of
 * each g_ev_rc slot in the range (the fixture is sized 0x48 so index 0x47 is in
 * bounds), and the real dialog VM runs on the per-page-distinct-glyph program
 * ev20_install_safe_env() installs (page p -> single TEXT glyph idx 0x50+p, then
 * END), so a correct page-1 dispatch emits exactly one glyph with idx 0x51 and
 * any wrong page fails loudly via the testglob glyph recorder. The consumed-
 * flags block is a private 0x20-byte buffer so both the read gate and the write
 * consume are observable.
 *
 * Two cases pin both sides of the single gate:
 *   (1) trigger un-consumed (byte [0x10] == 0): the body fires — the low nibble
 *       of combat_aux_block[0xD] is cleared to 0 (high nibble preserved) at the
 *       two range boundaries 0x10 and 0x47 and an interior char 0x30, the chars
 *       just OUTSIDE the range (0x0F below, the absent-from-range 0x47+1 cannot
 *       be checked as it is the last slot) and the in-slot neighbour bytes
 *       ([0xC]/[0xE]) are untouched, page 1 is shown (one glyph idx 0x51), and
 *       byte [0x10] is consumed to 1;
 *   (2) trigger already consumed (byte [0x10] == 1 on entry): NOTHING fires —
 *       no glyph, byte [0x10] stays 1, and the AI-class bytes in the range are
 *       left at their seeded sentinel (no clear).
 *
 * The pure blit/display side effects (the real glyph render path) are deferred
 * to Phase 9 integration; here only the dispatched-page identity, the precise
 * low-nibble AI-flag clear over the exact range, and the consume flag are
 * asserted.
 * ================================================================ */

/* private 0x20-byte tile-event consumed-flags block: the handler reads/writes
 * byte [0x10] of this block via data_fd2_field_map_tile_event_consumed_flags_ptr. */
static uint8   g_ev0a_consumed_flags[0x20];
static uint32  g_ev0a_saved_consumed_ptr;

static void ev0a_install_env(void)
{
    /* per-page-distinct-glyph dialog program + ch25-style real env (0x48-slot
     * g_ev_rc, empty party, ...); also resets the glyph recorder. */
    ev20_install_safe_env();

    /* aim the consumed-flags pointer at a private 0x20-byte buffer; start every
     * flag clear (each case seeds byte [0x10] as needed). */
    memset(g_ev0a_consumed_flags, 0, sizeof(g_ev0a_consumed_flags));
    g_ev0a_saved_consumed_ptr = data_fd2_field_map_tile_event_consumed_flags_ptr;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ev0a_consumed_flags;
}

static void ev0a_restore_env(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = g_ev0a_saved_consumed_ptr;
    ev_restore_rc_ptr();
}

/* Seed combat_aux_block[0xD] of every char the range covers (and the guards)
 * to the sentinel 0xA5: high nibble 0xA (a survivor witness) + low nibble 0x5
 * (a non-zero value a real clear must drive to 0). */
static void ev0a_seed_ai_flags(void)
{
    int i;
    for (i = 0x0F; i <= 0x47; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xA5;
    }
    /* in-slot neighbour bytes of the two boundary slots: must survive. */
    g_ev_rc[0x10].combat_aux_block[0xC] = 0xCC;
    g_ev_rc[0x10].combat_aux_block[0xE] = 0xEE;
    g_ev_rc[0x47].combat_aux_block[0xC] = 0xCC;
    g_ev_rc[0x47].combat_aux_block[0xE] = 0xEE;
}

/* ----------------------------------------------------------------
 * (1) Trigger un-consumed: the beat fires. The low nibble of
 * combat_aux_block[0xD] is cleared to 0 (high nibble preserved -> 0xA5 becomes
 * 0xA0) for every char in 0x10..0x47, the char just below the range (0x0F) and
 * the in-slot neighbour bytes are untouched, dialog page 1 is shown (one glyph
 * idx 0x51), and byte [0x10] is consumed to 1.
 * ---------------------------------------------------------------- */
static void test_ch14_event0a_first_time_clears_ai_range_shows_page_consumes(void)
{
    ev0a_install_env();
    ev0a_seed_ai_flags();

    /* trigger not yet consumed. */
    g_ev0a_consumed_flags[0x10] = 0;

    fd2_chapter_event_handler_0a__ch14_first_time(0);

    /* low nibble cleared, high nibble preserved (low-nibble-only write) at both
     * range boundaries and an interior char. */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xD], (long)0xA0);
    ASSERT_EQ((long)g_ev_rc[0x30].combat_aux_block[0xD], (long)0xA0);
    ASSERT_EQ((long)g_ev_rc[0x47].combat_aux_block[0xD], (long)0xA0);

    /* the char just BELOW the range was not touched (lower bound is exact). */
    ASSERT_EQ((long)g_ev_rc[0x0F].combat_aux_block[0xD], (long)0xA5);

    /* in-slot neighbour bytes of the boundary slots survive (single-byte write
     * to [0xD] only). */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xE], (long)0xEE);
    ASSERT_EQ((long)g_ev_rc[0x47].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev_rc[0x47].combat_aux_block[0xE], (long)0xEE);

    /* exactly page 1 was shown: one glyph, idx 0x51 (= 0x50 + page 1). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x51);

    /* the trigger was consumed. */
    ASSERT_EQ((long)g_ev0a_consumed_flags[0x10], 1L);

    ev0a_restore_env();
}

/* ----------------------------------------------------------------
 * (2) Consumed gate: byte [0x10] is already 1 on entry. NOTHING fires — no
 * dialog, byte [0x10] stays 1, and the AI-class bytes in the range keep their
 * seeded sentinel (no clear).
 * ---------------------------------------------------------------- */
static void test_ch14_event0a_already_consumed_does_nothing(void)
{
    ev0a_install_env();
    ev0a_seed_ai_flags();

    /* trigger already consumed. */
    g_ev0a_consumed_flags[0x10] = 1;

    fd2_chapter_event_handler_0a__ch14_first_time(0);

    /* no dialog dispatched. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    /* trigger still consumed (unchanged). */
    ASSERT_EQ((long)g_ev0a_consumed_flags[0x10], 1L);

    /* the AI-class bytes were NOT cleared — boundaries + interior keep 0xA5. */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xD], (long)0xA5);
    ASSERT_EQ((long)g_ev_rc[0x30].combat_aux_block[0xD], (long)0xA5);
    ASSERT_EQ((long)g_ev_rc[0x47].combat_aux_block[0xD], (long)0xA5);

    ev0a_restore_env();
}

void run_field_chevt14_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt14\n");
    RUN_TEST(test_ch10_event20_reloads_portrait1_and_shows_dialog_page1);
    RUN_TEST(test_show_chapter_dialog_portrait_set_1_reloads_portrait1_page1);
    RUN_TEST(test_ch13_event05_thunk_reloads_portrait1_and_shows_page1);
    RUN_TEST(test_ch10_event21_shows_page2_and_clears_ai_flag_for_0c_0d);
    RUN_TEST(test_event22_shows_dialog_page3);
    RUN_TEST(test_ch12_event23_reloads_portrait2_brackets_initphase);
    RUN_TEST(test_ch12_event24_sets_ai_flag_0x83_for_char_0e);
    RUN_TEST(test_event25_dialog_page1_two_stage_cinematic);
    RUN_TEST(test_ch13_event07_reloads_portrait2_brackets_initphase_page8);
    RUN_TEST(test_ch13_event08_lord_pickup_gives_item_shows_page_consumes);
    RUN_TEST(test_ch13_event08_non_lord_step_does_nothing);
    RUN_TEST(test_ch13_event08_full_inventory_does_nothing);
    RUN_TEST(test_ch13_event08_already_consumed_does_nothing);
    RUN_TEST(test_ch14_event0a_first_time_clears_ai_range_shows_page_consumes);
    RUN_TEST(test_ch14_event0a_already_consumed_does_nothing);
    printf("\n");
}
