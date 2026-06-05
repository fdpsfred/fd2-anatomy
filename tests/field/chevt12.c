/*
 * unit tests for src/field/chevt1.c (part 2 of 2: handlers 04/06/09/0b/0c/0e)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Part 1 (chevt11.c) covers
 * the four chapter-1 handlers (00..03); this part covers 04/06/09/0b/0c/0e. The
 * shared "ch25-style real portrait reload" safe env both parts drive the real
 * callees through lives in tests/include/fieldfix.h.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "fieldfix.h"

/* ================================================================
 * fd2_chapter_event_handler_04__unref_dialog_with_state @ 0x343E2
 *
 * Dispatch idx 0x04 of the per-event handler table at 0x51B91. No chapter
 * FDFIELD turn-event / tile-step hook references this slot (unreferenced —
 * possibly cut content). It is the simplest of the group: a straight-line,
 * no-branch beat with no RNG, no numeric computation, and no CALL-return value
 * used. Its ONLY deterministic, non-display state mutation — and thus its entire
 * testable risk core — is the team flip:
 *   - runtime_char_array[0xD].team = 1 (哈瓦特, char_id 0xD, flipped to ally);
 * followed by a single dialog page-7 display.
 *
 * The dialog call (fd2_display_dialog_scene, page 7) runs FOR REAL against the
 * immediate-END dialog program (current_chapter_text[7] -> a single -1 END
 * opcode): with no portrait open the VM reads END and returns at once, so it
 * performs zero glyph blits and never touches the compositor, palette, BIOS
 * tick, or the runtime-char sprite-load opcodes. Handler_04 calls NONE of the
 * heavy callees the other handlers use (no portrait reload, composite, pan,
 * cutscene, keyboard flush, or recruit), so the safe env here is just the
 * runtime-char array (for the team write + index bound) and the immediate-END
 * dialog program.
 *
 * The pure display side effect (the page-7 dialog render path when it is NOT
 * the immediate-END program) is deferred to Phase 9 integration.
 * ================================================================ */

/* immediate-END dialog program private to the handler_04 suite (pages 0..0x10
 * each point at a single -1 END opcode at the tail). */
static int16 g_ev4_dlg[0x12];

static void ev4_install_safe_env(void)
{
    int i;

    /* runtime-char slots: index 0xD must be writable for the team flip, and an
     * oversized (64-slot) array keeps the write in-bounds. */
    memset(g_ev_rc, 0, sizeof(g_ev_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ev_rc;

    /* immediate-END dialog program (pages 0..0x10 -> single END opcode), so the
     * real fd2_display_dialog_scene(page 7) returns at once with no blits. */
    for (i = 0; i <= 0x10; i++) {
        g_ev4_dlg[i] = (int16)(0x11 * 2);   /* byte offset of the END opcode */
    }
    g_ev4_dlg[0x11] = -1;                    /* END */
    current_chapter_text = (uint32)g_ev4_dlg;

    /* no portrait open on entry, so the END path skips the close sequence. */
    data_fd2_dialog_active_portrait_blit_offset = 0;
}

/* ----------------------------------------------------------------
 * The handler flips 哈瓦特 (char_id 0xD) to the ally side and shows dialog
 * page 7. Its observable, deterministic contract is: runtime_char_array[0xD].team
 * becomes 1, and the whole beat (the immediate-END page-7 dialog) runs to
 * completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch_event4_flips_hawat_to_ally(void)
{
    ev4_install_safe_env();

    /* perturb char 0xD's team to a non-ally sentinel so the flip is observable,
     * and seed neighbours so we can confirm the write lands on index 0xD only. */
    g_ev_rc[0xD].team = 0x55;
    g_ev_rc[0xC].team = 0x33;
    g_ev_rc[0xE].team = 0x44;

    fd2_chapter_event_handler_04__unref_dialog_with_state(0);

    /* 哈瓦特 (char_id 0xD) flipped to ally (team 1). */
    ASSERT_EQ(g_ev_rc[0xD].team, 1);

    /* exactly that slot was touched: neighbours are unchanged. */
    ASSERT_EQ(g_ev_rc[0xC].team, 0x33);
    ASSERT_EQ(g_ev_rc[0xE].team, 0x44);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_06__ch2_reinforcement @ 0x34422
 *
 * Dispatch idx 0x06 of the per-event handler table at 0x51B91 — ch2
 * turn-3 reinforcement beat. Like the other group-1 handlers it is a
 * straight-line, no-branch dialog/cutscene sequence (no RNG, no
 * CALL-return value used) PLUS one trailing fixed-count loop. The loop
 * is its distinguishing, deterministic state contract and its testable
 * risk core:
 *   for (i = 5; i < 0xB; i++):
 *     runtime_char_array[i].combat_aux_block[0xE] = 0x1A   (AI behaviour)
 *     runtime_char_array[i].combat_aux_block[0xF] = 0x0F   (AI parameter)
 * i.e. exactly slots 5..0xA (six reinforcement enemies) are armed; slots
 * 4 and 0xB are left untouched (loop bound 5 <= i < 0xB).
 *
 * The dialog/cutscene prologue runs end-to-end against the same proven
 * env handler_03 uses. The single fd2_load_chapter_portraits_and_dump_tmp(3)
 * runs FOR REAL against the staged real FDICON.B24 + FDFIELD.DAT
 * (alloc_offset 0 -> empty per-record scan; current_chapter_id 4 ->
 * valid FDFIELD index 0xE), bracketed by data_fd2_chapter_init_phase_flag
 * 1->0; the loader does not read that flag, so the bracketing is harmless
 * for the reload itself and ends back at 0. fd2_pan_cursor_and_window(9,1),
 * the two __delay_thunk_375b2 busy-waits, the zero-group cutscene event
 * 0xD, and the immediate-END dialog page 4 all run for real and return
 * fast. The portrait-set argument (3 here vs 6 in handler_03) only selects
 * which portrait pixels load; the FDFIELD re-read index and the full
 * 0x32A00-byte FD2.TMP rewrite are identical.
 *
 * Observable, deterministic contract asserted: the six AI-byte pairs land
 * on exactly slots 5..0xA, the init-phase flag ends at 0, the real reload
 * happened (field buffer nulled, FD2.TMP at full size), and the whole real
 * callee chain runs to completion without faulting. The pure blit/display
 * side effects (camera pan, cutscene compositing, dialog glyphs, portrait
 * pixels) are deferred to Phase 9 integration.
 * ================================================================ */

/* zero-group cutscene script for event 0xD: n_groups byte = 0, so the real
 * fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev6_script_0d[1] = { 0 };

static void ev6_install_safe_env(void)
{
    /* shared ch25-style real-portrait-reload env (empty party, gated HUD,
     * throttled palette, real compositor workspace, immediate-END dialog,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh
     * field buffer). handler_06 reloads ONCE (portrait set 3). */
    ev_install_safe_env();

    /* handler_06 fires cutscene EVENT 0xD; register its own zero-group
     * script so the real fd2_cutscene_event_trigger returns fast. */
    g_ev6_script_0d[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0xD] = g_ev6_script_0d;
}

/* ----------------------------------------------------------------
 * The handler fires its fixed ch2 reinforcement sequence end-to-end, then
 * arms six reinforcement enemies. Its observable, deterministic contract
 * is: combat_aux_block[0xE]/[0xF] become (0x1A, 0x0F) for exactly slots
 * 5..0xA (slots 4 and 0xB untouched), the init-phase flag ends at 0, the
 * real portrait reload runs (field buffer nulled, FD2.TMP at full size),
 * and the whole real callee chain (pan, two delays, cutscene 0xD,
 * immediate-END dialog page 4) runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch2_event6_arms_reinforcement_enemies(void)
{
    int i;

    ev6_install_safe_env();

    /* perturb the init-phase flag so the handler's reset to 0 is observable. */
    data_fd2_chapter_init_phase_flag = 0x55;

    /* seed every aux-block byte the loop targets (plus the bounding
     * neighbours 4 and 0xB) with sentinels distinct from 0x1A/0x0F so both
     * the writes and the loop bounds are observable. */
    for (i = 4; i <= 0xB; i++) {
        g_ev_rc[i].combat_aux_block[0xE] = 0x77;
        g_ev_rc[i].combat_aux_block[0xF] = 0x88;
    }

    remove("FD2.TMP");

    fd2_chapter_event_handler_06__ch2_reinforcement(0);

    /* exactly slots 5..0xA armed with the AI behaviour pair (0x1A, 0x0F). */
    for (i = 5; i <= 0xA; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xE], 0x1A);
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xF], 0x0F);
    }

    /* bounding neighbours left untouched: slot 4 (below) and slot 0xB (above). */
    ASSERT_EQ(g_ev_rc[4].combat_aux_block[0xE], 0x77);
    ASSERT_EQ(g_ev_rc[4].combat_aux_block[0xF], 0x88);
    ASSERT_EQ(g_ev_rc[0xB].combat_aux_block[0xE], 0x77);
    ASSERT_EQ(g_ev_rc[0xB].combat_aux_block[0xF], 0x88);

    /* the init-phase flag was set to 1 around the reload and reset to 0. */
    ASSERT_EQ(data_fd2_chapter_init_phase_flag, 0);

    /* the real portrait reload ran: field buffer freed+nulled, and FD2.TMP
     * was rewritten to its full 0x32A00-byte size. */
    ASSERT_EQ(chapter_portrait_load_buffer, 0);
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_09__ch3_char_cond @ 0x344C2
 *
 * Dispatch idx 0x09 of the per-event handler table at 0x51B91 — ch3
 * turn-3 char-conditional beat. Unlike the group's straight-line handlers
 * this one has a single guarding BRANCH and therefore TWO paths that both
 * must be exercised:
 *   if (fd2_check_char_is_dead(6) == 0)   // 沃斯 (char 6) still alive
 *     load_chapter_portraits_and_dump_tmp(2);
 *     pan_cursor_and_window(3,0); delay(800);
 *     pan_cursor_and_window(3,0x11); delay(200);
 *     display_dialog_scene(page 4, ...);
 *   // else: skip the entire beat
 * No RNG, no numeric computation, no CALL-return value used other than the
 * fd2_check_char_is_dead(6) guard.
 *
 * fd2_check_char_is_dead is the real routine; it reads runtime_char[6].flags
 * bit0 through data_fd2_battle_runtime_char_array_ptr (pointed at g_ev_rc by
 * ev_install_safe_env). The guard is therefore pinned by g_ev_rc[6].flags:
 * bit0 clear -> alive -> body runs; CHARFLAG_DEAD set -> dead -> body skipped.
 * Both paths are asserted.
 *
 * ALIVE path: the single fd2_load_chapter_portraits_and_dump_tmp(2) runs FOR
 * REAL against the staged real FDICON.B24 + FDFIELD.DAT using the same proven
 * ch25-style env handler_03/06 use (alloc_offset 0 -> empty per-record scan;
 * current_chapter_id 4 -> valid FDFIELD index 0xE), so it frees+nulls the
 * field buffer and rewrites the full 0x32A00-byte FD2.TMP. The portrait-set
 * argument (2 here) only selects which portrait pixels load; the FDFIELD
 * re-read index and the FD2.TMP rewrite are identical to the other reloads.
 * The two __delay_thunk_375b2 busy-waits spin on the live BIOS tick, the two
 * fd2_pan_cursor_and_window calls run against the staged camera, and the
 * immediate-END dialog program (page 4 <= 0x10) makes fd2_display_dialog_scene
 * return at once with no glyph blits. Handler_09 fires NO cutscene event, so
 * no cutscene script needs registering.
 *
 * DEAD path: with the dead-check pinned to 1 the guard fails and NONE of the
 * body runs; the test proves it via a per-handler memory observable
 * (chapter_portrait_load_buffer): the env seeds it NULL and the real reload
 * would assign it a fresh buffer (and ultimately null it after freeing), so a
 * still-NULL buffer afterward proves fd2_load_chapter_portraits_and_dump_tmp
 * never executed. This is independent of the shared FD2.TMP swap file (a
 * global-cwd artifact five suites write, whose remove() DOSBox's local-drive
 * layer defers within a run, so its absence is not a reliable negative).
 *
 * The pure blit/display side effects of the alive beat (camera pan, dialog
 * glyphs, portrait pixels) are deferred to Phase 9 integration.
 * ================================================================ */

/* ----------------------------------------------------------------
 * ALIVE path: 沃斯 (char 6) alive -> the beat runs end-to-end. Observable,
 * deterministic contract: the real portrait reload happened (field buffer
 * freed+nulled, FD2.TMP rewritten to its full 0x32A00-byte size) and the whole
 * real callee chain (reload, two pans, two delays, immediate-END dialog page 4)
 * runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch3_event9_char6_alive_reloads_and_shows_dialog(void)
{
    /* shared ch25-style real-portrait-reload env (empty party, gated HUD,
     * throttled palette, real compositor workspace, immediate-END dialog,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh
     * field buffer). handler_09 reloads ONCE (portrait set 2). */
    ev_install_safe_env();

    /* 沃斯 (char 6) alive: ev_install_safe_env memset g_ev_rc, so g_ev_rc[6].flags
     * bit0 is clear -> fd2_check_char_is_dead(6)==0 and the guard passes. */

    /* seed the camera at the origin so the two pans (to (3,0) then (3,0x11))
     * are bounded and the final window origin is the observable. */
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;

    remove("FD2.TMP");

    fd2_chapter_event_handler_09__ch3_char_cond(0);

    /* the body ran: the camera panned to the final target (3, 0x11). */
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 0x11);

    /* and the real portrait reload ran: field buffer freed+nulled, and FD2.TMP
     * was rewritten to its full 0x32A00-byte size. */
    ASSERT_EQ(chapter_portrait_load_buffer, 0);
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * DEAD path: 沃斯 (char 6) already dead -> the entire beat is skipped.
 * Observable, deterministic contract: with the dead-check pinned to 1 the guard
 * fails, so the body's camera pans never run and the window origin keeps the
 * sentinel it was seeded with (the alive path would have moved it to (3,0x11)).
 * ---------------------------------------------------------------- */
static void test_ch3_event9_char6_dead_skips_beat(void)
{
    ev_install_safe_env();

    /* 沃斯 (char 6) dead: set the dead bit on g_ev_rc[6] so the guard fails. */
    g_ev_rc[6].flags |= CHARFLAG_DEAD;

    /* seed the camera at a sentinel distinct from the body's final pan target
     * (3, 0x11); if the body runs it would overwrite this. */
    data_fd2_battle_view_window_origin_x = 0x42;
    data_fd2_battle_view_window_origin_y = 0x37;

    fd2_chapter_event_handler_09__ch3_char_cond(0);

    /* body skipped: no pan ran, so the camera origin still holds the sentinel. */
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 0x42);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 0x37);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_0b__ch4_dialog @ 0x34565
 *
 * Dispatch idx 0x0B of the per-event handler table at 0x51B91 — ch4
 * turn-4 dialog-only beat, and the simplest handler in the group: a
 * straight-line, no-branch sequence with no RNG, no numeric computation,
 * and no CALL-return value used. It does just two things:
 *   load_chapter_portraits_and_dump_tmp(2);
 *   display_dialog_scene(page 2, ...);
 *
 * In the binary the handler prepares its own 8 PUSHes (page=2 plus the
 * fixed dialog geometry) and JMPs into handler_09's shared tail at 0x3452F
 * (PUSH current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24;
 * RET); the emit reproduces that tail inline. It has NO state of its own
 * and NO branch, so its entire testable risk core is that the real portrait
 * reload happens and the whole beat runs to completion without faulting.
 *
 * Both callees are REAL emitted functions and run end-to-end against the
 * same proven ch25-style env handler_09's alive path uses: the single
 * fd2_load_chapter_portraits_and_dump_tmp(2) runs FOR REAL against the
 * staged real FDICON.B24 + FDFIELD.DAT (alloc_offset 0 -> empty per-record
 * scan; current_chapter_id 4 -> valid FDFIELD index 0xE), so it frees+nulls
 * the field buffer and rewrites the full 0x32A00-byte FD2.TMP; the portrait
 * set argument (2) only selects which portrait pixels load. The immediate-END
 * dialog program (page 2 <= 0x10) makes fd2_display_dialog_scene return at
 * once with no glyph blits. Handler_0b fires NO cutscene, pan, delay, or
 * recruit, so no extra env is needed.
 *
 * The pure blit/display side effects (dialog glyphs, portrait pixels) are
 * deferred to Phase 9 integration.
 * ================================================================ */

/* ----------------------------------------------------------------
 * The handler fires its fixed ch4 dialog-only beat end-to-end. Its
 * observable, deterministic contract is: the real portrait reload happened
 * (field buffer freed+nulled, FD2.TMP rewritten to its full 0x32A00-byte
 * size) and the whole beat (real reload + immediate-END dialog page 2) runs
 * to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch4_event0b_reloads_portraits_and_shows_dialog(void)
{
    /* shared ch25-style real-portrait-reload env (empty party, gated HUD,
     * throttled palette, real compositor workspace, immediate-END dialog,
     * empty keyboard buffer, alloc_offset 0, current_chapter_id 4, fresh
     * field buffer). handler_0b reloads ONCE (portrait set 2). */
    ev_install_safe_env();

    remove("FD2.TMP");

    fd2_chapter_event_handler_0b__ch4_dialog(0);

    /* the real portrait reload ran: field buffer freed+nulled, and FD2.TMP
     * was rewritten to its full 0x32A00-byte size. */
    ASSERT_EQ(chapter_portrait_load_buffer, 0);
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_0c__unref_first_time @ 0x34594
 *
 * Dispatch idx 0x0C of the per-event handler table at 0x51B91. No chapter
 * FDFIELD turn-event / tile-step hook references this slot (unreferenced —
 * possibly cut content). Unlike the group's straight-line handlers it is
 * FIRST-TIME GATED, so it has a single guarding BRANCH and TWO paths that
 * both must be exercised:
 *   if (tile_event_consumed_flags[0x10] == 0)        // first time only
 *     set_combat_aux_block_byte_d_low4_for_char_range(0x18, 0x1B, 7);
 *     display_dialog_scene(page 3, ...);
 *     tile_event_consumed_flags[0x10] = 1;           // consume the flag
 *   // else: skip the entire beat
 * No RNG, no numeric computation, no CALL-return value used.
 *
 * The AI-flag arming is its distinguishing, deterministic state contract
 * and its testable risk core. fd2_set_combat_aux_block_byte_d_low4_for_char_range
 * is the REAL emitted callee (@0x3419C): for each char i in [0x18, 0x1B] it
 * rewrites combat_aux_block[0xD] = (old & 0xF0) | (7 & 0xFF), i.e. it sets the
 * low nibble (ai_class) to 7 while PRESERVING the high nibble. Exactly the
 * four enemies 0x18..0x1B are armed; chars 0x17 (below) and 0x1C (above) are
 * left untouched (inclusive loop 0x18 <= i <= 0x1B). The gate flag is byte
 * [0x10] of the 0x20-byte tile-event consumed-flags block pointed at by
 * data_fd2_field_map_tile_event_consumed_flags_ptr; the env aims that pointer
 * at a private 0x20-byte buffer so both the read and the write stay in-bounds.
 *
 * The dialog call (fd2_display_dialog_scene, page 3) runs FOR REAL against the
 * immediate-END dialog program (current_chapter_text[3] -> a single -1 END
 * opcode): with no portrait open the VM reads END and returns at once, so it
 * performs zero glyph blits and never touches the compositor, palette, BIOS
 * tick, or the runtime-char sprite-load opcodes. Handler_0c calls NONE of the
 * heavy callees the dialog/cutscene handlers use (no portrait reload, composite,
 * pan, cutscene, keyboard flush, or recruit), so the safe env here is just the
 * runtime-char array (for the AI writes + index bound), the consumed-flags
 * block, and the immediate-END dialog program.
 *
 * The pure display side effect (the page-3 dialog render path when it is NOT
 * the immediate-END program) is deferred to Phase 9 integration.
 * ================================================================ */

/* immediate-END dialog program private to the handler_0c suite (pages 0..0x10
 * each point at a single -1 END opcode at the tail). */
static int16 g_ev0c_dlg[0x12];

/* 0x20-byte tile-event consumed-flags block: the handler reads/writes byte
 * [0x10] of this block via data_fd2_field_map_tile_event_consumed_flags_ptr. */
static uint8 g_ev0c_consumed[0x20];

static uint32 g_ev0c_saved_consumed_ptr;

static void ev0c_install_safe_env(void)
{
    int i;

    /* runtime-char slots: the AI write targets chars 0x18..0x1B, so an
     * oversized (64-slot) array keeps every write in-bounds. */
    memset(g_ev_rc, 0, sizeof(g_ev_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ev_rc;

    /* private consumed-flags block; start every flag clear. */
    memset(g_ev0c_consumed, 0, sizeof(g_ev0c_consumed));
    g_ev0c_saved_consumed_ptr = data_fd2_field_map_tile_event_consumed_flags_ptr;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ev0c_consumed;

    /* immediate-END dialog program (pages 0..0x10 -> single END opcode), so the
     * real fd2_display_dialog_scene(page 3) returns at once with no blits. */
    for (i = 0; i <= 0x10; i++) {
        g_ev0c_dlg[i] = (int16)(0x11 * 2);   /* byte offset of the END opcode */
    }
    g_ev0c_dlg[0x11] = -1;                    /* END */
    current_chapter_text = (uint32)g_ev0c_dlg;

    /* no portrait open on entry, so the END path skips the close sequence. */
    data_fd2_dialog_active_portrait_blit_offset = 0;
}

static void ev0c_restore_env(void)
{
    data_fd2_field_map_tile_event_consumed_flags_ptr = g_ev0c_saved_consumed_ptr;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * FIRST-TIME path: gate flag [0x10] clear -> the beat runs. Observable,
 * deterministic contract: the low nibble (ai_class) of combat_aux_block[0xD]
 * becomes 7 for exactly chars 0x18..0x1B with the high nibble preserved, the
 * bounding neighbours 0x17 and 0x1C are untouched, the gate flag [0x10] is
 * consumed (set to 1), and the immediate-END page-3 dialog runs to completion
 * without faulting.
 * ---------------------------------------------------------------- */
static void test_ch_event0c_firsttime_arms_ai_flag7_and_consumes(void)
{
    int i;

    ev0c_install_safe_env();

    /* seed the four target chars' combat_aux_block[0xD] with a sentinel whose
     * high nibble is non-zero and low nibble differs from 7, so both the
     * low-nibble write to 7 AND the high-nibble preservation are observable. */
    for (i = 0x18; i <= 0x1B; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xA3;
    }
    /* bounding neighbours just outside the inclusive range. */
    g_ev_rc[0x17].combat_aux_block[0xD] = 0x55;
    g_ev_rc[0x1C].combat_aux_block[0xD] = 0x66;

    fd2_chapter_event_handler_0c__unref_first_time(0);

    /* exactly chars 0x18..0x1B armed: low nibble -> 7, high nibble (0xA0) kept. */
    for (i = 0x18; i <= 0x1B; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0xA7);
    }

    /* bounding neighbours left untouched. */
    ASSERT_EQ(g_ev_rc[0x17].combat_aux_block[0xD], 0x55);
    ASSERT_EQ(g_ev_rc[0x1C].combat_aux_block[0xD], 0x66);

    /* the gate flag was consumed (set to 1). */
    ASSERT_EQ(g_ev0c_consumed[0x10], 1);

    ev0c_restore_env();
}

/* ----------------------------------------------------------------
 * ALREADY-CONSUMED path: gate flag [0x10] already 1 -> the entire beat is
 * skipped. Observable, deterministic contract: the AI bytes for chars
 * 0x18..0x1B keep the sentinel they were seeded with (the first-time path
 * would have set their low nibble to 7), and the gate flag stays 1.
 * ---------------------------------------------------------------- */
static void test_ch_event0c_already_consumed_skips_beat(void)
{
    int i;

    ev0c_install_safe_env();

    /* pin the gate flag to 1 so the first-time guard fails. */
    g_ev0c_consumed[0x10] = 1;

    /* seed the target chars with a sentinel distinct from any (old&0xF0)|7
     * result, so a write would be detectable. */
    for (i = 0x18; i <= 0x1B; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0x22;
    }

    fd2_chapter_event_handler_0c__unref_first_time(0);

    /* body skipped: no AI write ran, so each target keeps its sentinel. */
    for (i = 0x18; i <= 0x1B; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0x22);
    }

    /* the gate flag is unchanged (still consumed). */
    ASSERT_EQ(g_ev0c_consumed[0x10], 1);

    ev0c_restore_env();
}

/* ================================================================
 * fd2_chapter_event_handler_0e__ch5_dialog_with_state @ 0x345EA
 *
 * Dispatch idx 0x0E of the per-event handler table at 0x51B91 — ch5 turn-3
 * dialog-with-state beat. A straight-line, no-branch sequence: no RNG, no
 * numeric computation, and no CALL-return value used. It does three things:
 *   set_combat_aux_block_byte_d_low4_for_char_range(0x25, 0x28, 0);
 *   set_combat_aux_block_byte_d_low4_for_char_range(0x0D, 0x18, 0);
 *   display_dialog_scene(page 3, ...);
 *
 * In the binary the handler prepares its own 8 PUSHes (page=3 plus the fixed
 * dialog geometry) and JMPs into handler_09's shared tail at 0x3452F (PUSH
 * current_chapter_text; CALL fd2_display_dialog_scene; ADD ESP,0x24; RET); the
 * emit reproduces that tail inline.
 *
 * The two AI-flag disarms are its distinguishing, deterministic state contract
 * and its testable risk core. fd2_set_combat_aux_block_byte_d_low4_for_char_range
 * is the REAL emitted callee (@0x3419C): for each char i in an INCLUSIVE range
 * it rewrites combat_aux_block[0xD] = (old & 0xF0) | (0 & 0xFF), i.e. it clears
 * the low nibble (ai_class -> 0) while PRESERVING the high nibble. The two calls
 * cover exactly chars 0x25..0x28 (4 chars) and chars 0x0D..0x18 (12 chars); the
 * char just outside each range (0x24/0x29 below/above the first, 0x0C/0x19
 * below/above the second) is left untouched.
 *
 * The dialog call (fd2_display_dialog_scene, page 3) runs FOR REAL against the
 * immediate-END dialog program (current_chapter_text[3] -> a single -1 END
 * opcode): with no portrait open the VM reads END and returns at once, so it
 * performs zero glyph blits and never touches the compositor, palette, BIOS
 * tick, or the runtime-char sprite-load opcodes. Handler_0e calls NONE of the
 * heavy callees the dialog/cutscene handlers use (no portrait reload, composite,
 * pan, cutscene, keyboard flush, or recruit), so the safe env here is just the
 * runtime-char array (for the AI writes + index bound; the 64-slot fixture
 * keeps the highest target 0x28 in-bounds) and the immediate-END dialog program.
 *
 * The pure display side effect (the page-3 dialog render path when it is NOT
 * the immediate-END program) is deferred to Phase 9 integration.
 * ================================================================ */

/* immediate-END dialog program private to the handler_0e suite (pages 0..0x10
 * each point at a single -1 END opcode at the tail). */
static int16 g_ev0e_dlg[0x12];

static void ev0e_install_safe_env(void)
{
    int i;

    /* runtime-char slots: the AI writes target chars 0x0D..0x18 and 0x25..0x28,
     * so an oversized (64-slot) array keeps the highest write (0x28) in-bounds. */
    memset(g_ev_rc, 0, sizeof(g_ev_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ev_rc;

    /* immediate-END dialog program (pages 0..0x10 -> single END opcode), so the
     * real fd2_display_dialog_scene(page 3) returns at once with no blits. */
    for (i = 0; i <= 0x10; i++) {
        g_ev0e_dlg[i] = (int16)(0x11 * 2);   /* byte offset of the END opcode */
    }
    g_ev0e_dlg[0x11] = -1;                    /* END */
    current_chapter_text = (uint32)g_ev0e_dlg;

    /* no portrait open on entry, so the END path skips the close sequence. */
    data_fd2_dialog_active_portrait_blit_offset = 0;
}

/* ----------------------------------------------------------------
 * The handler disarms the AI/dialog control flag across two char ranges and
 * shows dialog page 3. Its observable, deterministic contract is: the low
 * nibble (ai_class) of combat_aux_block[0xD] becomes 0 for exactly chars
 * 0x25..0x28 and 0x0D..0x18 with each high nibble preserved, the bounding
 * neighbours just outside both ranges (0x24/0x29 and 0x0C/0x19) are untouched,
 * and the immediate-END page-3 dialog runs to completion without faulting.
 * ---------------------------------------------------------------- */
static void test_ch5_event0e_disarms_two_ranges_and_shows_dialog(void)
{
    int i;

    ev0e_install_safe_env();

    /* seed every char the two ranges touch (plus the four bounding neighbours)
     * with a sentinel whose high nibble is non-zero and low nibble differs from
     * 0, so both the low-nibble clear AND the high-nibble preservation are
     * observable. */
    for (i = 0x0D; i <= 0x18; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xB5;
    }
    for (i = 0x25; i <= 0x28; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xC9;
    }
    /* bounding neighbours just outside each inclusive range. */
    g_ev_rc[0x0C].combat_aux_block[0xD] = 0x41;
    g_ev_rc[0x19].combat_aux_block[0xD] = 0x42;
    g_ev_rc[0x24].combat_aux_block[0xD] = 0x43;
    g_ev_rc[0x29].combat_aux_block[0xD] = 0x44;

    fd2_chapter_event_handler_0e__ch5_dialog_with_state(0);

    /* range 1 (chars 0x0D..0x18): low nibble cleared to 0, high nibble (0xB0)
     * preserved. */
    for (i = 0x0D; i <= 0x18; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0xB0);
    }
    /* range 2 (chars 0x25..0x28): low nibble cleared to 0, high nibble (0xC0)
     * preserved. */
    for (i = 0x25; i <= 0x28; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0xC0);
    }

    /* bounding neighbours just outside both ranges left untouched. */
    ASSERT_EQ(g_ev_rc[0x0C].combat_aux_block[0xD], 0x41);
    ASSERT_EQ(g_ev_rc[0x19].combat_aux_block[0xD], 0x42);
    ASSERT_EQ(g_ev_rc[0x24].combat_aux_block[0xD], 0x43);
    ASSERT_EQ(g_ev_rc[0x29].combat_aux_block[0xD], 0x44);

    ev_restore_rc_ptr();
}

void run_field_chevt12_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt12\n");
    RUN_TEST(test_ch_event4_flips_hawat_to_ally);
    RUN_TEST(test_ch2_event6_arms_reinforcement_enemies);
    RUN_TEST(test_ch3_event9_char6_alive_reloads_and_shows_dialog);
    RUN_TEST(test_ch3_event9_char6_dead_skips_beat);
    RUN_TEST(test_ch4_event0b_reloads_portraits_and_shows_dialog);
    RUN_TEST(test_ch_event0c_firsttime_arms_ai_flag7_and_consumes);
    RUN_TEST(test_ch_event0c_already_consumed_skips_beat);
    RUN_TEST(test_ch5_event0e_disarms_two_ranges_and_shows_dialog);
    printf("\n");
}
