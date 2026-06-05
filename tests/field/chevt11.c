/*
 * unit tests for src/field/chevt1.c (part 1 of 2: handlers 00..03)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. This part covers the four
 * chapter-1 handlers (00..03); part 2 (chevt12.c) covers 04/06/09/0b/0c/0e. The
 * shared "ch25-style real portrait reload" safe env both parts drive the real
 * callees through lives in tests/include/fieldfix.h.
 *
 * fd2_chapter_event_handler_00__ch1_dialog_with_state @ 0x341DB is the
 * chapter-1 turn-event slot 0 (dispatch idx 0). It is a straight-line, no-branch
 * dialog/cutscene beat: no RNG, no numeric computation, no CALL-return value
 * used. Its testable risk core is the deterministic, non-display state it mutates
 * and that the whole call sequence runs to completion without faulting:
 *   - it recruits 哈諾 (char_id 1) FOR REAL via
 *     fd2_init_runtime_char_from_base_growth, which appends one template slot
 *     (team=2, char_id=1 at +7/+8) and increments the menu-party member count;
 *   - it sets battle_anim_phase = 0 after the first dialog (and never sets it
 *     back), so it ends at 0;
 *   - it reloads the chapter portrait set TWICE (set 3 then set 7) via the REAL
 *     fd2_load_chapter_portraits_and_dump_tmp, which re-reads
 *     FDFIELD.DAT[chapter*3+2], frees+nulls the field buffer, and rewrites the
 *     0x32A00-byte FD2.TMP swap file (the final rewrite is observable).
 *
 * The handler is driven end-to-end on-host with the shared fieldfix safe env
 * plus the ch25-style real portrait reload:
 *   - both fd2_load_chapter_portraits_and_dump_tmp calls run FOR REAL against
 *     the staged real FDICON.B24 + FDFIELD.DAT (copied into the test cwd by
 *     build_test.py); alloc_offset is set to 0 so the per-record race scan
 *     iterates zero entries (no fd2_init_runtime_char_for_battle calls), and
 *     current_chapter_id is seeded to 4 so the FDFIELD re-read index
 *     (4*3+2 = 0xE) is the same valid index the rsrc loader suite exercises;
 *   - the recruit fd2_init_runtime_char_from_base_growth(1) runs FOR REAL,
 *     appending into the staged 64-slot menu roster;
 *   - the two fd2_display_dialog_scene calls (pages 0xB and 3, both <= 0x10)
 *     take the immediate-END dialog program so each returns at once with no
 *     glyph blits and never reaches the page-break busy-wait;
 *   - the two fd2_cutscene_event_trigger calls take zero-group scripts (events
 *     7 and 8) so each just composites once and returns;
 *   - the real fd2_composite_battle_frame runs against the staged compositor
 *     workspace (HUD gated off, anim_phase=0 -> cursor overlay no-op, palette
 *     cycle throttled), fd2_pan_cursor_and_window runs against the staged
 *     camera, the two __delay_thunk_375b2(100) busy-waits spin on the live BIOS
 *     tick, and the empty active party makes the real callees' char loops
 *     iterate zero chars.
 *
 * The pure blit/display side effects (dialog glyphs, cutscene compositing,
 * camera pan, portrait pixels) are deferred to Phase 9 integration.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "fieldfix.h"

/* ----------------------------------------------------------------
 * The handler runs the whole ch1 prologue beat end-to-end: it recruits 哈諾
 * (char 1) into the menu roster, sets battle_anim_phase to 0, and reloads the
 * portrait set twice (rewriting FD2.TMP). The recruit, the anim-phase reset,
 * and the real FD2.TMP rewrite are the observable, non-display artifacts.
 * ---------------------------------------------------------------- */
static void test_ch1_event0_recruits_hanuo_and_reloads_portraits(void)
{
    uint8 *roster;

    ev_install_safe_env();

    /* perturb anim_phase so the handler's reset to 0 is observable. */
    data_fd2_battle_anim_phase = 0x77;

    remove("FD2.TMP");

    fd2_chapter_event_handler_00__ch1_dialog_with_state(0);

    /* exactly one recruit: 哈諾 (char 1) appended to the menu roster. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 1);
    roster = (uint8 *)data_fd2_shared_menu_party_roster_buffer_ptr;
    ASSERT_EQ(roster[0x06], 2);      /* team = player */
    ASSERT_EQ(roster[0x07], 1);      /* char_id = 哈諾 */
    ASSERT_EQ(roster[0x08], 1);      /* char_id combat-byte copy */

    /* battle_anim_phase reset to 0 after the first dialog and left there. */
    ASSERT_EQ(data_fd2_battle_anim_phase, 0);

    /* both real portrait reloads ran: field buffer freed+nulled, and FD2.TMP
     * was rewritten to its full 0x32A00-byte size. */
    ASSERT_EQ(chapter_portrait_load_buffer, 0);
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_01__ch1_dialog_with_state @ 0x342B5
 *
 * Like handler_00 this is a straight-line, no-branch ch1 turn-event beat: no
 * RNG, no numeric computation, no CALL-return value used, and (unlike
 * handler_00) NO state of its own — it just fires a fixed sequence of callees:
 *   pan_cursor_and_window(0xB,0x10); animate_party_addition_with_appear_effect(4);
 *   clear_keyboard_buffer; composite_battle_frame(1); cutscene_event_trigger(3);
 *   clear_all_chars_facing; display_dialog_scene(page 4, ...).
 *
 * The handler's distinguishing contract versus handler_00 is the
 * animate_party_addition_with_appear_effect(4) call (party slot/chapter 4 joins
 * with the appear explosion). That callee (@0x32999) is not yet emitted, so the
 * shared recording stub in testglob.c stands in for it; the test asserts the
 * handler fires it exactly once with chapter id 4. All the OTHER callees are
 * real emitted functions and run end-to-end against the same proven safe env
 * handler_00 uses (empty active party, gated HUD, throttled palette cycle, a
 * zero-group cutscene script for event 3 so it composites once and returns, an
 * immediate-END dialog program for page 4, and an empty BIOS keyboard buffer).
 *
 * The heavy animation's own display + portrait-reload (FD2.TMP rewrite) state is
 * covered when fd2_animate_party_addition_with_appear_effect is itself emitted;
 * the pure blit/display side effects of this beat (camera pan, cutscene
 * compositing, dialog glyphs, frame blits) are deferred to Phase 9 integration.
 * ================================================================ */

extern int    g_animate_party_addition_calls;
extern uint32 g_animate_party_addition_last_chapter;

/* zero-group cutscene script for event 3: n_groups byte = 0, so the real
 * fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev1_script_03[1] = { 0 };

static void ev1_install_safe_env(void)
{
    g_animate_party_addition_calls = 0;
    g_animate_party_addition_last_chapter = 0;

    /* shared ch25-style real-portrait-reload env (empty party, gated HUD,
     * throttled palette, real compositor workspace, immediate-END dialog,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh
     * field buffer). */
    ev_install_safe_env();

    /* handler_01 fires cutscene EVENT 3; register its own zero-group script so
     * the real fd2_cutscene_event_trigger returns fast. */
    g_ev1_script_03[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[3] = g_ev1_script_03;
}

/* ----------------------------------------------------------------
 * The handler fires its fixed ch1 slot-1 sequence end-to-end. Its observable,
 * deterministic contract is: it invokes the appear-animation exactly once with
 * chapter id 4, and the whole real callee chain (camera pan, frame composite,
 * zero-group cutscene 3, facing reset, immediate-END dialog page 4) runs to
 * completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch1_event1_fires_appear_anim_for_slot4(void)
{
    ev1_install_safe_env();

    fd2_chapter_event_handler_01__ch1_dialog_with_state(0);

    /* the appear-explosion animation fired exactly once, for chapter/slot 4. */
    ASSERT_EQ(g_animate_party_addition_calls, 1);
    ASSERT_EQ(g_animate_party_addition_last_chapter, 4);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_02__ch1_dialog_with_state @ 0x3431D
 *
 * Structurally identical to handler_01 (same straight-line, no-branch callee
 * sequence; no RNG, no numeric computation, no CALL-return value used, no state
 * of its own). It differs only in the fixed arguments:
 *   pan_cursor_and_window(0,0x10); animate_party_addition_with_appear_effect(5);
 *   clear_keyboard_buffer; composite_battle_frame(1); cutscene_event_trigger(4);
 *   clear_all_chars_facing; display_dialog_scene(page 5, ...).
 *
 * In the binary the dialog call is reached by a JMP into handler_01's shared
 * tail (PUSH current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24;
 * POP EBX; RET); the emit reproduces that tail inline. The distinguishing
 * testable contract versus handler_01 is the slot/chapter id (5, not 4) passed
 * to the appear animation and the cutscene EVENT id (4, not 3). As with
 * handler_01 the appear animation is the not-yet-emitted heavy callee, so the
 * shared testglob recording stub stands in for it; every other callee is a real
 * emitted function and runs end-to-end against the same proven safe env (empty
 * active party, gated HUD, throttled palette cycle, a zero-group cutscene script
 * for event 4 so it composites once and returns, an immediate-END dialog program
 * for page 5, and an empty BIOS keyboard buffer).
 *
 * The pure blit/display side effects of this beat (camera pan, cutscene
 * compositing, dialog glyphs, frame blits) are deferred to Phase 9 integration.
 * ================================================================ */

/* zero-group cutscene script for event 4: n_groups byte = 0, so the real
 * fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev2_script_04[1] = { 0 };

static void ev2_install_safe_env(void)
{
    /* shared safe env (empty party, gated HUD, throttled palette, real
     * compositor workspace, immediate-END dialog, empty keyboard buffer). */
    ev1_install_safe_env();

    /* handler_02 fires cutscene EVENT 4 (handler_01 fires 3); register its own
     * zero-group script so the real fd2_cutscene_event_trigger returns fast. */
    g_ev2_script_04[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[4] = g_ev2_script_04;
}

/* ----------------------------------------------------------------
 * The handler fires its fixed ch1 slot-2 sequence end-to-end. Its observable,
 * deterministic contract is: it invokes the appear-animation exactly once with
 * chapter/slot id 5, and the whole real callee chain (camera pan, frame
 * composite, zero-group cutscene 4, facing reset, immediate-END dialog page 5)
 * runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch1_event2_fires_appear_anim_for_slot5(void)
{
    ev2_install_safe_env();

    fd2_chapter_event_handler_02__ch1_dialog_with_state(0);

    /* the appear-explosion animation fired exactly once, for chapter/slot 5. */
    ASSERT_EQ(g_animate_party_addition_calls, 1);
    ASSERT_EQ(g_animate_party_addition_last_chapter, 5);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_03__ch1_dialog_with_state @ 0x34377
 *
 * Like handler_00 this is a straight-line, no-branch ch1 turn-event beat: no
 * RNG, no numeric computation, no CALL-return value used. Unlike 01/02 it fires
 * NO appear animation; instead it does a single REAL portrait reload to race 6,
 * bracketed by setting data_fd2_chapter_init_phase_flag to 1 before the reload
 * and back to 0 after:
 *   pan_cursor_and_window(0xB,0xB);
 *   chapter_init_phase_flag = 1; load_chapter_portraits_and_dump_tmp(6);
 *   chapter_init_phase_flag = 0;
 *   composite_battle_frame(1); cutscene_event_trigger(6);
 *   clear_all_chars_facing; clear_keyboard_buffer;
 *   display_dialog_scene(page 6, ...).
 *
 * In the binary the dialog call is reached by a JMP into handler_01's shared
 * tail (PUSH current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24;
 * POP EBX; RET); the emit reproduces that tail inline. The portrait loader does
 * NOT read the init-phase flag (it only gates the battle-init tile scan
 * elsewhere), so the flag bracketing is harmless for the reload itself.
 *
 * Every callee here is a REAL emitted function. The single
 * fd2_load_chapter_portraits_and_dump_tmp(6) runs FOR REAL against the staged
 * real FDICON.B24 + FDFIELD.DAT (same ch25-style env handler_00 uses:
 * alloc_offset 0 -> empty per-record scan, current_chapter_id 4 -> valid
 * FDFIELD index 0xE), so it frees+nulls the field buffer and rewrites the full
 * 0x32A00-byte FD2.TMP. The handler's observable, deterministic contract is:
 *   - the init-phase flag is set during the reload and ends back at 0;
 *   - the real reload happened (field buffer nulled, FD2.TMP at full size);
 *   - cutscene event 6 fires (zero-group script -> composites once, returns);
 *   - the whole real callee chain runs to completion without faulting.
 *
 * The pure blit/display side effects of this beat (camera pan, cutscene
 * compositing, dialog glyphs, frame blits, portrait pixels) are deferred to
 * Phase 9 integration.
 * ================================================================ */

/* zero-group cutscene script for event 6: n_groups byte = 0, so the real
 * fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev3_script_06[1] = { 0 };

static void ev3_install_safe_env(void)
{
    /* shared ch25-style real-portrait-reload env (empty party, gated HUD,
     * throttled palette, real compositor workspace, immediate-END dialog,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh
     * field buffer). handler_03 reloads ONCE (handler_00 reloads twice). */
    ev_install_safe_env();

    /* handler_03 fires cutscene EVENT 6; register its own zero-group script so
     * the real fd2_cutscene_event_trigger returns fast. */
    g_ev3_script_06[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[6] = g_ev3_script_06;
}

/* ----------------------------------------------------------------
 * The handler fires its fixed ch1 slot-3 sequence end-to-end. Its observable,
 * deterministic contract is: the init-phase flag is set to 1 during the real
 * portrait reload and reset to 0 afterward, the real reload runs (field buffer
 * nulled, FD2.TMP rewritten to its full 0x32A00-byte size), and the whole real
 * callee chain (camera pan, frame composite, zero-group cutscene 6, facing
 * reset, keyboard flush, immediate-END dialog page 6) runs to completion
 * without faulting.
 * ---------------------------------------------------------------- */
static void test_ch1_event3_reloads_race6_brackets_initphase(void)
{
    ev3_install_safe_env();

    /* perturb the init-phase flag so the handler's reset to 0 is observable. */
    data_fd2_chapter_init_phase_flag = 0x55;

    remove("FD2.TMP");

    fd2_chapter_event_handler_03__ch1_dialog_with_state(0);

    /* the flag was set to 1 around the reload and reset to 0 at the end. */
    ASSERT_EQ(data_fd2_chapter_init_phase_flag, 0);

    /* the real portrait reload ran: field buffer freed+nulled, and FD2.TMP was
     * rewritten to its full 0x32A00-byte size. */
    ASSERT_EQ(chapter_portrait_load_buffer, 0);
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_13__unref_char_cond @ 0x34716
 *
 * Dispatch idx 0x13 of the per-event handler table at 0x51B91. No chapter
 * FDFIELD turn-event / tile-step hook references this slot (unreferenced —
 * possibly cut content / non-chapter dispatcher). It is a char-conditional beat:
 *   set_combat_aux_block_byte_d_low4_for_char_range(7, 0x24, 7);   // arm 30 chars
 *   display_dialog_scene(page 8, ...);                             // unconditional
 *   any_alive = 0;
 *   for (i = 7; i < 0x25; i++):
 *     if (check_char_is_dead(i) == 0): any_alive = 1;              // no early break
 *   if (any_alive):
 *     display_dialog_scene(page 0xB, ...);                         // conditional
 *
 * Two distinct testable risk cores: (1) the AI-flag arming over the wide band
 * 0x07..0x24 (30 chars), and (2) the BRANCH whose guard is the EAX return value
 * of fd2_check_char_is_dead — exactly the CALL-return-value control-flow case
 * (the Ghidra EAX-tracking-bug risk class), so BOTH paths are exercised.
 *
 * fd2_set_combat_aux_block_byte_d_low4_for_char_range is the REAL emitted callee
 * (@0x3419C): for each char i in the INCLUSIVE range it rewrites
 * combat_aux_block[0xD] = (old & 0xF0) | (7 & 0xFF), i.e. it sets the low nibble
 * (ai_class) to 7 while PRESERVING the high nibble. fd2_check_char_is_dead
 * (@0x3453E) is also REAL: it reads runtime_char[i].flags bit0 through
 * data_fd2_battle_runtime_char_array_ptr, so the alive scan is pinned by the
 * g_ev_rc[i].flags bits. Both fd2_display_dialog_scene calls run FOR REAL.
 *
 * The dialog program here is custom (not the shared immediate-END one): page 8
 * is a single TEXT glyph (idx 0x41) then END, and page 0xB is a single TEXT
 * glyph (idx 0x42) then END. The TEXT-glyph blit is the testglob recorder
 * (g_dlg_glyph_calls / g_dlg_glyph_last_idx), so each dialog call that reaches
 * its glyph is observable WITHOUT touching real VGA. That makes the branch
 * directly observable: the any-alive path blits BOTH glyphs (page 8 then page
 * 0xB, last idx 0x42, 2 calls); the all-dead path blits only page 8's glyph
 * (last idx 0x41, 1 call). On the glyph+END path the VM touches no portrait,
 * scroll, file load, or busy-wait, so the run is deterministic with the BIOS
 * keyboard buffer left empty (blink_flag stays set -> one blink per glyph).
 *
 * The 64-slot g_ev_rc fixture keeps the highest touched char (0x24) in-bounds.
 * The pure blit/display side effects (the real glyph render path, cutscene/pan
 * compositing — none of which this handler invokes) are deferred to Phase 9.
 * ================================================================ */

/* glyph-blit recorder from testglob.c (the dialog VM's TEXT-glyph blit is a
 * stub there, so glyph emission is observable without touching real VGA). */
extern int    g_dlg_glyph_calls;
extern uint32 g_dlg_glyph_last_idx;

/* custom dialog program: pages 0..0x10 each have their own header word.
 * page 8 -> one glyph (0x41) then END; page 0xB -> one glyph (0x42) then END;
 * every other page -> immediate END. Layout (int16 words):
 *   [0..0x10]   header words (byte offsets into this same array)
 *   [0x11]      shared END (-1)           -> byte offset 0x11*2 = 0x22
 *   [0x12]      page 8 glyph (0x41)       -> byte offset 0x12*2 = 0x24
 *   [0x13]      page 8 END (-1)
 *   [0x14]      page 0xB glyph (0x42)     -> byte offset 0x14*2 = 0x28
 *   [0x15]      page 0xB END (-1)                                          */
static int16 g_ev13_dlg[0x16];

static void ev13_install_safe_env(void)
{
    int i;

    /* runtime-char slots: the AI write + dead scan span chars 0x07..0x24, so an
     * oversized (64-slot) array keeps every access in-bounds. */
    memset(g_ev_rc, 0, sizeof(g_ev_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ev_rc;

    /* custom dialog program (distinct page 8 / page 0xB so the branch is
     * observable via the glyph recorder). */
    for (i = 0; i <= 0x10; i++) {
        g_ev13_dlg[i] = (int16)(0x11 * 2);   /* default: immediate END opcode */
    }
    g_ev13_dlg[8]    = (int16)(0x12 * 2);    /* page 8  -> glyph(0x41), END */
    g_ev13_dlg[0xB]  = (int16)(0x14 * 2);    /* page 0xB-> glyph(0x42), END */
    g_ev13_dlg[0x11] = -1;                    /* shared END */
    g_ev13_dlg[0x12] = 0x41;                  /* page 8 glyph */
    g_ev13_dlg[0x13] = -1;                    /* page 8 END */
    g_ev13_dlg[0x14] = 0x42;                  /* page 0xB glyph */
    g_ev13_dlg[0x15] = -1;                    /* page 0xB END */
    current_chapter_text = (uint32)g_ev13_dlg;

    /* no portrait open on entry, so each END path skips the close sequence. */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* empty BIOS keyboard buffer (head==tail) so the real keyboard poll the VM
     * runs after each glyph returns 0 and the run stays deterministic. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;

    /* reset the glyph recorder so the per-test counts are clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * ANY-ALIVE path: every char in 0x07..0x24 starts alive (flags bit0 clear), so
 * the alive scan sets any_alive and the conditional page-0xB dialog runs.
 * Observable, deterministic contract: the AI low nibble of combat_aux_block[0xD]
 * becomes 7 for exactly chars 0x07..0x24 (high nibble preserved; neighbours
 * 0x06/0x25 untouched), BOTH dialog pages are emitted (page 8 glyph 0x41 then
 * page 0xB glyph 0x42 -> 2 glyph calls, last idx 0x42), and the whole real
 * callee chain runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch_event13_any_alive_arms_band_and_shows_second_dialog(void)
{
    int i;

    ev13_install_safe_env();

    /* g_ev_rc was memset to 0, so every char's flags bit0 is clear -> all 30
     * chars in 0x07..0x24 are alive and the any_alive guard passes. */

    /* seed every char the AI range touches (plus the two bounding neighbours)
     * with a sentinel whose high nibble is non-zero and low nibble differs from
     * 7, so both the low-nibble write to 7 AND the high-nibble preservation are
     * observable. */
    for (i = 7; i <= 0x24; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0x93;
    }
    g_ev_rc[6].combat_aux_block[0xD]    = 0x55;   /* below the inclusive range */
    g_ev_rc[0x25].combat_aux_block[0xD] = 0x66;   /* above the inclusive range */

    fd2_chapter_event_handler_13__unref_char_cond(0);

    /* exactly chars 0x07..0x24 armed: low nibble -> 7, high nibble (0x90) kept. */
    for (i = 7; i <= 0x24; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0x97);
    }
    /* bounding neighbours just outside the inclusive range left untouched. */
    ASSERT_EQ(g_ev_rc[6].combat_aux_block[0xD], 0x55);
    ASSERT_EQ(g_ev_rc[0x25].combat_aux_block[0xD], 0x66);

    /* both dialog pages were shown (page 8 then conditional page 0xB). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x42);   /* page 0xB glyph */

    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * ALL-DEAD path: every char in 0x07..0x24 is dead (flags bit0 set), so the
 * alive scan never sets any_alive and the conditional page-0xB dialog is
 * SKIPPED. Observable, deterministic contract: the AI arming (which happens
 * BEFORE the branch) still applies to all 30 chars, only the unconditional
 * page-8 dialog is emitted (1 glyph call, last idx 0x41 — never 0x42), and the
 * whole beat runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch_event13_all_dead_skips_second_dialog(void)
{
    int i;

    ev13_install_safe_env();

    /* pin every char in 0x07..0x24 dead so fd2_check_char_is_dead returns 1 for
     * all of them and any_alive stays 0. Also seed the AI sentinel as above so
     * the pre-branch arming is still observable. */
    for (i = 7; i <= 0x24; i++) {
        g_ev_rc[i].flags |= CHARFLAG_DEAD;
        g_ev_rc[i].combat_aux_block[0xD] = 0x93;
    }
    g_ev_rc[6].combat_aux_block[0xD]    = 0x55;
    g_ev_rc[0x25].combat_aux_block[0xD] = 0x66;

    fd2_chapter_event_handler_13__unref_char_cond(0);

    /* the AI arming runs BEFORE the branch, so it still applied to all 30 chars
     * (low nibble -> 7, high nibble 0x90 kept) regardless of the dead scan. */
    for (i = 7; i <= 0x24; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0x97);
    }
    ASSERT_EQ(g_ev_rc[6].combat_aux_block[0xD], 0x55);
    ASSERT_EQ(g_ev_rc[0x25].combat_aux_block[0xD], 0x66);

    /* only the unconditional page-8 dialog ran: 1 glyph, and the page-0xB glyph
     * (0x42) was never emitted. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x41);   /* page 8 glyph only */

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_14__ch6_dialog @ 0x347B1
 *
 * Dispatch idx 0x14 of the per-event handler table at 0x51B91 (chapter 6
 * turn-event slot 0). The MINIMAL dialog-only beat: a single straight-line
 * call with no branch, no RNG, no numeric computation, and no CALL-return
 * value used — it just shows dialog page 1 and does nothing else (no portrait
 * reload, no camera pan, no state writes). In the binary it prepares its own 8
 * PUSHes (page=1 + the fixed dialog geometry) and JMPs into handler_09's shared
 * tail at 0x3452F.
 *
 * The one observable, deterministic contract is which PAGE it dispatches into
 * the real fd2_display_dialog_scene VM. As in the handler_13 tests, a custom
 * dialog program is installed where the targeted page resolves to a single TEXT
 * glyph then END; the glyph blit is the testglob recorder
 * (g_dlg_glyph_calls / g_dlg_glyph_last_idx), so the page selection is
 * observable WITHOUT touching real VGA. To make a wrong-page dispatch fail
 * loudly, EVERY page is given its own distinct glyph idx (page p -> glyph
 * 0x50+p): a correct page-1 dispatch must emit exactly one glyph with idx 0x51.
 * With no portrait open (active_portrait_blit_offset 0) the END opcode returns
 * at once — no portrait, scroll, file load, or page-break busy-wait — and an
 * empty BIOS keyboard buffer keeps the per-glyph poll deterministic.
 *
 * The pure blit/display side effects (the real glyph render path) are deferred
 * to Phase 9 integration.
 * ================================================================ */

/* custom dialog program: each page 0..0x10 resolves to its own single glyph
 * (idx 0x50+page) then END, so the page the handler selects is identifiable by
 * the recorded glyph idx. Layout (int16 words):
 *   [0..0x10]      header words: page p -> byte offset of its glyph word
 *   [0x11+2*p]     page p glyph (0x50+p)
 *   [0x12+2*p]     page p END (-1)                                          */
static int16 g_ev14_dlg[0x11 + 2 * 0x11];

static void ev14_install_safe_env(void)
{
    int p;

    /* per-page (glyph, END) pairs start right after the 0x11 header words. */
    for (p = 0; p <= 0x10; p++) {
        g_ev14_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev14_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev14_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev14_dlg;

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
 * The handler fires its one beat: show dialog page 1 via the real dialog VM.
 * Observable, deterministic contract: exactly one glyph is emitted and it is
 * page 1's glyph (idx 0x51) — proving the handler dispatches page 1 (not any
 * other page) — and the real dialog call runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch6_event14_shows_dialog_page1(void)
{
    ev14_install_safe_env();

    fd2_chapter_event_handler_14__ch6_dialog(0);

    /* exactly page 1 was shown: one glyph, idx 0x51 (= 0x50 + page 1). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x51);
}

/* ================================================================
 * fd2_chapter_event_handler_15__ch6_char_cond @ 0x347D9
 *
 * Dispatch idx 0x15 of the per-event handler table at 0x51B91 (chapter 6
 * turn-event slot 1). A char-conditional beat:
 *   if (check_char_is_dead(8) == 0):           // 索倫 still alive
 *     display_dialog_scene(page 2, ...);
 * The single branch is guarded by the EAX return value of fd2_check_char_is_dead
 * — exactly the CALL-return-value control-flow case (the Ghidra EAX-tracking-bug
 * risk class) — so BOTH paths are exercised. No RNG, no numeric computation.
 *
 * fd2_check_char_is_dead (@0x3453E) is the REAL emitted callee: it reads
 * runtime_char[8].flags bit0 through data_fd2_battle_runtime_char_array_ptr, so
 * the alive/dead decision is pinned by g_ev_rc[8].flags. fd2_display_dialog_scene
 * runs FOR REAL on the same per-page-distinct-glyph program handler_14 uses
 * (page p -> single TEXT glyph idx 0x50+p, then END), so a correct page-2
 * dispatch must emit exactly one glyph with idx 0x52 and any wrong page fails
 * loudly. The glyph blit is the testglob recorder (g_dlg_glyph_calls /
 * g_dlg_glyph_last_idx), making both the dispatch-vs-skip branch and the page
 * selection observable WITHOUT touching real VGA. With no portrait open
 * (active_portrait_blit_offset 0) the END opcode returns at once and an empty
 * BIOS keyboard buffer keeps the per-glyph poll deterministic.
 *
 * The 64-slot g_ev_rc fixture keeps char 8 in-bounds. The pure blit/display
 * side effects (the real glyph render path) are deferred to Phase 9 integration.
 * ================================================================ */

static void ev15_install_safe_env(void)
{
    /* runtime-char slots: the gate reads char 8, so the 64-slot array keeps it
     * in-bounds. memset clears flags bit0 -> char 8 starts alive. */
    memset(g_ev_rc, 0, sizeof(g_ev_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ev_rc;

    /* reuse handler_14's per-page-distinct-glyph dialog program (page p ->
     * glyph 0x50+p, END) so the dispatched page is identifiable, the empty
     * keyboard buffer + no-portrait setup, and a clean glyph recorder. */
    ev14_install_safe_env();
}

/* ----------------------------------------------------------------
 * ALIVE path: char 8 (索倫) starts alive (flags bit0 clear), so the gate passes
 * and the conditional dialog page 2 runs. Observable, deterministic contract:
 * exactly one glyph is emitted and it is page 2's glyph (idx 0x52) — proving the
 * handler dispatches page 2 (not any other page) — and the real call runs to
 * completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch6_event15_alive_shows_dialog_page2(void)
{
    ev15_install_safe_env();

    /* g_ev_rc was memset to 0, so char 8's flags bit0 is clear -> alive. */

    fd2_chapter_event_handler_15__ch6_char_cond(0);

    /* exactly page 2 was shown: one glyph, idx 0x52 (= 0x50 + page 2). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x52);

    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * DEAD path: char 8 (索倫) is pinned dead (flags bit0 set), so the gate fails
 * and the conditional dialog is SKIPPED. Observable, deterministic contract: no
 * glyph is emitted at all (the dialog VM is never entered) and the beat runs to
 * completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch6_event15_dead_skips_dialog(void)
{
    ev15_install_safe_env();

    /* pin char 8 dead so fd2_check_char_is_dead(8) returns 1 and the gate fails. */
    g_ev_rc[8].flags |= CHARFLAG_DEAD;

    fd2_chapter_event_handler_15__ch6_char_cond(0);

    /* dialog skipped entirely: no glyph emitted. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_16__ch6_char_cond @ 0x34819
 *
 * Dispatch idx 0x16 of the per-event handler table at 0x51B91 (chapter 6
 * turn-event slot 2). A char-conditional beat:
 *   if (check_char_is_dead(8) == 0):              // 索倫 still alive
 *     load_chapter_portraits_and_dump_tmp(1);     // portrait set 1 reload
 *     show_chapter_intro_text_dialog_mode_3();    // page-3 dialog helper
 * The single branch is guarded by the EAX return value of fd2_check_char_is_dead
 * — exactly the CALL-return-value control-flow case (the Ghidra EAX-tracking-bug
 * risk class) — so BOTH paths are exercised. No RNG, no numeric computation.
 *
 * fd2_check_char_is_dead (@0x3453E) is the REAL emitted gate: it reads
 * runtime_char[8].flags bit0 through data_fd2_battle_runtime_char_array_ptr, so
 * the alive/dead decision is pinned by g_ev_rc[8].flags.
 *
 * On the alive branch the handler does NOT inline a dialog call — in the binary
 * it tail-JMPs to the named helper fd2_show_chapter_intro_text_dialog_mode_3
 * @0x34906 (it is that helper's sole caller). That helper is now the REAL
 * emitted function (src/field/chevt1.c): it dispatches dialog page 3 through the
 * real fd2_display_dialog_scene VM. Reusing handler_14's per-page-distinct-glyph
 * program (page p -> single TEXT glyph idx 0x50+p, then END) over the portrait-
 * reload env, the alive gate must emit exactly one glyph with idx 0x53 (= page
 * 3) and the dead gate must emit none — proving the gate delegates to the real
 * page-3 helper iff 索倫 is alive. The portrait reload
 * fd2_load_chapter_portraits_and_dump_tmp(1) is the REAL emitted callee too: it
 * re-reads FDFIELD.DAT[chapter*3+2] and rewrites the 0x32A00-byte FD2.TMP swap
 * file, so the alive branch is additionally pinned by the real FD2.TMP rewrite
 * (and the dead branch by its absence).
 *
 * Driven on-host with the shared fieldfix "ch25-style real portrait reload" env
 * (64-slot g_ev_rc keeps char 8 in-bounds, alloc_offset 0 -> empty per-record
 * scan, current_chapter_id 4 -> valid FDFIELD index 0xE, staged real FDICON.B24
 * + FDFIELD.DAT), with current_chapter_text re-pointed at the per-page glyph
 * program so the real helper's page-3 dispatch is observable. The pure
 * blit/display side effects (the real glyph render path) are deferred to Phase 9
 * integration.
 * ================================================================ */

/* portrait-reload env (FD2.TMP rewrite) PLUS handler_14's per-page-distinct-
 * glyph dialog program, so the real page-3 helper's dispatch is observable via
 * the glyph recorder (alive -> 1 glyph idx 0x53; dead -> 0). */
static void ev16_install_safe_env(void)
{
    ev_install_safe_env();
    ev14_install_safe_env();
}

/* ----------------------------------------------------------------
 * ALIVE path: char 8 (索倫) starts alive (flags bit0 clear), so the gate passes:
 * the real portrait set 1 reload runs (rewriting FD2.TMP to its full 0x32A00
 * bytes) and the real page-3 dialog helper dispatches page 3 exactly once
 * (one glyph, idx 0x53).
 * ---------------------------------------------------------------- */
static void test_ch6_event16_alive_reloads_portraits_and_shows_dialog(void)
{
    ev16_install_safe_env();

    /* g_ev_rc was memset to 0, so char 8's flags bit0 is clear -> alive. */
    remove("FD2.TMP");

    fd2_chapter_event_handler_16__ch6_char_cond(0);

    /* real portrait reload ran: FD2.TMP rewritten to its full 0x32A00 bytes. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);
    /* the real page-3 helper dispatched exactly page 3: one glyph, idx 0x53. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x53);

    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * DEAD path: char 8 (索倫) is pinned dead (flags bit0 set), so the gate fails
 * and the whole beat is SKIPPED — neither the real portrait reload (no FD2.TMP
 * written) nor the real page-3 dialog helper runs (no glyph emitted).
 * ---------------------------------------------------------------- */
static void test_ch6_event16_dead_skips_beat(void)
{
    ev16_install_safe_env();

    /* pin char 8 dead so fd2_check_char_is_dead(8) returns 1 and the gate fails. */
    g_ev_rc[8].flags |= CHARFLAG_DEAD;
    remove("FD2.TMP");

    fd2_chapter_event_handler_16__ch6_char_cond(0);

    /* beat skipped: no portrait reload (FD2.TMP absent) and no dialog dispatch. */
    ASSERT_EQ(ev_fd2_tmp_size(), -1);
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_17__unref_turn_gated @ 0x34844
 *
 * Dispatch idx 0x17 of the per-event handler table at 0x51B91. No chapter
 * FDFIELD turn-event / tile-step hook references this slot (unreferenced —
 * possibly cut content / non-chapter dispatcher). It is a turn-counter-gated
 * beat:
 *   set_combat_aux_block_byte_d_low4_for_char_range(8, 0x1C, 0);  // arm 21 chars
 *   display_dialog_scene(page 4, ...);                            // unconditional
 *   if ((int)data_fd2_battle_turn_counter < 0x0F):                // signed (JGE)
 *     load_chapter_portraits_and_dump_tmp(2);                     // portrait set 2
 *     pan_cursor_and_window(5, 0x11);  cutscene_event_trigger(0x19);
 *     display_dialog_scene(page 5, ...);
 *     pan_cursor_and_window(5, 0x11);  cutscene_event_trigger(0x1A);
 *     mark_char_as_dead(0x21);                                    // kill char 0x21
 *     battle_anim_phase = 1;
 *
 * The testable risk core is the turn-counter GATE (a complex-control-flow /
 * state-transition branch), so BOTH paths are exercised. The guard is a SIGNED
 * compare in the binary (CMP [0x53BEF],0xF; JGE), so the boundary value 0x0F
 * itself must skip; the gate-fail test pins turn == 0x0F to nail that boundary.
 *
 * Every callee here is a REAL emitted function driven against the shared
 * fieldfix "ch25-style real portrait reload" env, plus zero-group cutscene
 * scripts for events 0x19 / 0x1A so the real fd2_cutscene_event_trigger
 * composites once and returns:
 *   - fd2_set_combat_aux_block_byte_d_low4_for_char_range (@0x3419C) writes
 *     combat_aux_block[0xD] = (old & 0xF0) | (0 & 0x0F) for the INCLUSIVE range
 *     0x08..0x1C, i.e. it clears the low nibble (ai_class) while preserving the
 *     high nibble — observable on g_ev_rc, and it runs BEFORE the gate so it
 *     applies on BOTH paths;
 *   - fd2_mark_char_as_dead (@0x32975) stores g_ev_rc[0x21].flags = 1 (a direct
 *     byte store of 1, not an OR) through data_fd2_battle_runtime_char_array_ptr;
 *   - fd2_load_chapter_portraits_and_dump_tmp(2) runs FOR REAL against the staged
 *     FDICON.B24 + FDFIELD.DAT and rewrites FD2.TMP to its full 0x32A00 bytes;
 *   - the two fd2_display_dialog_scene calls (pages 4 and 5) take the shared
 *     immediate-END program so each returns at once with no glyph blits;
 *   - fd2_pan_cursor_and_window / fd2_composite_battle_frame run against the
 *     staged camera + compositor workspace with the empty active party.
 * The final battle_anim_phase = 1 store (which the binary reaches via the JMP
 * into the shared __CHK epilogue tail at 0x35C18) is observable on the
 * gate-pass path; the gate-fail path leaves it at its perturbed sentinel.
 *
 * The 64-slot g_ev_rc fixture keeps the highest touched char (0x21) in-bounds.
 * The pure blit/display side effects (dialog glyphs, cutscene/pan compositing,
 * portrait pixels) are deferred to Phase 9 integration.
 * ================================================================ */

/* zero-group cutscene scripts for events 0x19 / 0x1A: n_groups byte = 0, so the
 * real fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev17_script_19[1] = { 0 };
static uint8 g_ev17_script_1a[1] = { 0 };

static void ev17_install_safe_env(void)
{
    /* shared ch25-style real-portrait-reload env (empty party, gated HUD,
     * throttled palette, real compositor workspace, immediate-END dialog,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh
     * field buffer, 64-slot g_ev_rc). */
    ev_install_safe_env();

    /* handler_17 fires cutscene EVENTS 0x19 and 0x1A; register their own
     * zero-group scripts so the real fd2_cutscene_event_trigger returns fast. */
    g_ev17_script_19[0] = 0;
    g_ev17_script_1a[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x19] = g_ev17_script_19;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x1A] = g_ev17_script_1a;
}

/* ----------------------------------------------------------------
 * GATE-PASS path: the battle turn counter is below 0x0F, so the full boss-death
 * cinematic runs. Observable, deterministic contract: the AI low nibble of
 * combat_aux_block[0xD] is cleared to 0 for exactly chars 0x08..0x1C (high
 * nibble preserved; bounding neighbours 0x07/0x1D untouched), char 0x21 is
 * killed (flags = 1), battle_anim_phase ends at 1, the real portrait reload
 * rewrote FD2.TMP to its full 0x32A00 bytes, and the whole real callee chain
 * runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch_event17_turn_below_gate_runs_cinematic(void)
{
    int i;

    ev17_install_safe_env();

    /* gate passes: turn counter below 0x0F. */
    data_fd2_battle_turn_counter = 5;

    /* seed the AI band (plus the two bounding neighbours) with a sentinel whose
     * high nibble is non-zero and low nibble differs from 0, so both the
     * low-nibble clear AND the high-nibble preservation are observable. */
    for (i = 8; i <= 0x1C; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0x9A;
    }
    g_ev_rc[7].combat_aux_block[0xD]    = 0x55;   /* below the inclusive range */
    g_ev_rc[0x1D].combat_aux_block[0xD] = 0x66;   /* above the inclusive range */

    /* char 0x21 starts alive; perturb anim_phase so its set-to-1 is observable. */
    g_ev_rc[0x21].flags = 0;
    data_fd2_battle_anim_phase = 0x77;

    remove("FD2.TMP");

    fd2_chapter_event_handler_17__unref_turn_gated(0);

    /* exactly chars 0x08..0x1C armed: low nibble -> 0, high nibble (0x90) kept. */
    for (i = 8; i <= 0x1C; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0x90);
    }
    /* bounding neighbours just outside the inclusive range left untouched. */
    ASSERT_EQ(g_ev_rc[7].combat_aux_block[0xD], 0x55);
    ASSERT_EQ(g_ev_rc[0x1D].combat_aux_block[0xD], 0x66);

    /* the gated cinematic ran: char 0x21 killed, anim_phase flipped to 1, and
     * the real portrait reload rewrote FD2.TMP to its full 0x32A00 bytes. */
    ASSERT_EQ(g_ev_rc[0x21].flags, 1);
    ASSERT_EQ(data_fd2_battle_anim_phase, 1);
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * GATE-FAIL path: the battle turn counter is at the boundary value 0x0F, so the
 * signed (JGE) gate fails and the whole cinematic block is SKIPPED. Observable,
 * deterministic contract: the AI arming (which happens BEFORE the gate) still
 * cleared the low nibble for all of chars 0x08..0x1C; but char 0x21 is NOT
 * killed (flags stay 0), battle_anim_phase keeps its perturbed sentinel (never
 * set to 1), and no portrait reload happened (FD2.TMP absent). This also pins
 * the signed-compare boundary: turn == 0x0F must take the skip path.
 * ---------------------------------------------------------------- */
static void test_ch_event17_turn_at_gate_skips_cinematic(void)
{
    int i;

    ev17_install_safe_env();

    /* gate fails at the boundary: 0x0F is NOT < 0x0F (signed JGE -> skip). */
    data_fd2_battle_turn_counter = 0xF;

    for (i = 8; i <= 0x1C; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0x9A;
    }
    g_ev_rc[7].combat_aux_block[0xD]    = 0x55;
    g_ev_rc[0x1D].combat_aux_block[0xD] = 0x66;

    /* char 0x21 starts alive; perturb anim_phase to a sentinel that the skip
     * path must leave untouched. */
    g_ev_rc[0x21].flags = 0;
    data_fd2_battle_anim_phase = 0x77;

    remove("FD2.TMP");

    fd2_chapter_event_handler_17__unref_turn_gated(0);

    /* the AI arming runs BEFORE the gate, so it still applied to all of
     * chars 0x08..0x1C regardless of the turn counter. */
    for (i = 8; i <= 0x1C; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0x90);
    }
    ASSERT_EQ(g_ev_rc[7].combat_aux_block[0xD], 0x55);
    ASSERT_EQ(g_ev_rc[0x1D].combat_aux_block[0xD], 0x66);

    /* the gated cinematic was skipped: char 0x21 NOT killed, anim_phase keeps
     * its sentinel, and no FD2.TMP was written. */
    ASSERT_EQ(g_ev_rc[0x21].flags, 0);
    ASSERT_EQ(data_fd2_battle_anim_phase, 0x77);
    ASSERT_EQ(ev_fd2_tmp_size(), -1);

    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

void run_field_chevt11_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt11\n");
    RUN_TEST(test_ch1_event0_recruits_hanuo_and_reloads_portraits);
    RUN_TEST(test_ch1_event1_fires_appear_anim_for_slot4);
    RUN_TEST(test_ch1_event2_fires_appear_anim_for_slot5);
    RUN_TEST(test_ch1_event3_reloads_race6_brackets_initphase);
    RUN_TEST(test_ch_event13_any_alive_arms_band_and_shows_second_dialog);
    RUN_TEST(test_ch_event13_all_dead_skips_second_dialog);
    RUN_TEST(test_ch6_event14_shows_dialog_page1);
    RUN_TEST(test_ch6_event15_alive_shows_dialog_page2);
    RUN_TEST(test_ch6_event15_dead_skips_dialog);
    RUN_TEST(test_ch6_event16_alive_reloads_portraits_and_shows_dialog);
    RUN_TEST(test_ch6_event16_dead_skips_beat);
    RUN_TEST(test_ch_event17_turn_below_gate_runs_cinematic);
    RUN_TEST(test_ch_event17_turn_at_gate_skips_cinematic);
    printf("\n");
}
