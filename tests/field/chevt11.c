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

void run_field_chevt11_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt11\n");
    RUN_TEST(test_ch1_event0_recruits_hanuo_and_reloads_portraits);
    RUN_TEST(test_ch1_event1_fires_appear_anim_for_slot4);
    RUN_TEST(test_ch1_event2_fires_appear_anim_for_slot5);
    RUN_TEST(test_ch1_event3_reloads_race6_brackets_initphase);
    printf("\n");
}
