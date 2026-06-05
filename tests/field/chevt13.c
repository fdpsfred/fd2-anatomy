/*
 * unit tests for src/field/chevt1.c (part 3: handler 18 +
 * fd2_show_chapter_intro_text_dialog_mode_3 + handlers 19, 1A, 1B, 1C, 1D, 1E)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1/2 (chevt11.c /
 * chevt12.c) cover handlers 00..17; this part covers handler 18, the named
 * helper fd2_show_chapter_intro_text_dialog_mode_3 @ 0x34906, handler 19, the
 * tile-step char-conditional handler 1A, the ch8 every-turn cinematic handler
 * 1B, the ch8 turn-15 AI-control handler 1C, the dialog+chain handler 1D, and
 * the unreferenced major-cinematic handler 1E.
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

/* testglob.c's __delay_thunk_375b2 stub records each idle-hold (call count +
 * last tick arg), so handler_1b's two 100ms holds are observable. */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;

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

/* ================================================================
 * fd2_chapter_event_handler_1a__ch7_char_cond @ 0x3499B
 *
 * Dispatch idx 0x1A of the per-event handler table at 0x51B91 (chapter 7
 * tile-step event slot 0). char-conditional, TILE-STEP variant: the dispatch
 * arg is the id of the char who stepped onto the trigger tile, and the beat is
 * gated on that char's team:
 *   if (runtime_char_array[stepping_char_id].team != 0):    // a non-enemy stepped
 *     set_combat_aux_block_byte_d_low4_for_char_range(9, 0x1B, 0); // disarm AI flag
 *     tile_event_consumed_flags[0x10] = 1;                  // consume the trigger
 *
 * The testable risk core is the TEAM GATE branch (a control-flow branch whose
 * guard is the stepping char's team byte) plus its two write effects, so all
 * three team senses are exercised: team 0 (enemy) takes the SKIP path, while
 * team 1 (npc) and team 2 (player) both take the FIRE path — pinning the guard
 * as "!= 0" (any non-enemy), not "== 2" or a signed >= test. There is no dialog,
 * no RNG, and no display side effect, so every effect is a directly observable
 * in-memory write.
 *
 * The range disarm is driven against the REAL emitted helper
 * fd2_set_combat_aux_block_byte_d_low4_for_char_range (it ORs (byte&0xF0)|0 into
 * combat_aux_block[0xD] for chars 0x09..0x1B inclusive, preserving the high
 * nibble); the consumed-flags block and the runtime-char array are local buffers
 * so both the gate read and every write are observable. The stepping char id is
 * chosen OUTSIDE the [9,0x1B] disarm range (char 5) so its team seed is
 * independent of the range write, and the disarm-range boundaries (chars 8 and
 * 0x1C just outside, chars 9 and 0x1B at the edges) are checked explicitly.
 * ================================================================ */

/* local runtime-char array for handler_1A: oversized so the disarm range
 * [9,0x1B] and the stepping char (5) are all in-bounds. */
static runtime_char g_ev1a_rc[64];

/* backing for the 0x20-byte tile-event consumed-flags block: byte [0x10] is the
 * slot this handler consumes when the gate passes. */
static uint8 g_ev1a_consumed_flags[0x20];

/* Seed every char's combat_aux_block[0xD] with 0xA5 (high nibble 0xA, low
 * nibble 5) so a disarm write is observable as low nibble -> 0 with the high
 * nibble preserved (0xA5 -> 0xA0), and an untouched char keeps 0xA5. */
static void ev1a_install_env(uint8 stepping_team)
{
    int i;

    memset(g_ev1a_rc, 0, sizeof(g_ev1a_rc));
    for (i = 0; i < 64; i++) {
        g_ev1a_rc[i].combat_aux_block[0xD] = 0xA5;
    }
    /* the stepping char (id 5) carries the team the gate reads. */
    g_ev1a_rc[5].team = stepping_team;
    data_fd2_battle_runtime_char_array_ptr = g_ev1a_rc;

    memset(g_ev1a_consumed_flags, 0, sizeof(g_ev1a_consumed_flags));
    data_fd2_field_map_tile_event_consumed_flags_ptr =
        (uint32)g_ev1a_consumed_flags;
}

/* ----------------------------------------------------------------
 * GATE-PASS path (team 2 = player). The non-enemy stepping char fires the beat:
 * chars 0x09..0x1B inclusive get combat_aux_block[0xD] low nibble cleared to 0
 * (high nibble preserved: 0xA5 -> 0xA0), chars just outside that range (8 and
 * 0x1C) stay 0xA5, and tile-event slot [0x10] is consumed (set to 1).
 * ---------------------------------------------------------------- */
static void test_ch7_event1a_player_steps_disarms_and_consumes(void)
{
    int i;

    ev1a_install_env(2);   /* TEAM_PLAYER */

    fd2_chapter_event_handler_1a__ch7_char_cond(5);

    /* chars 0x09..0x1B inclusive: low nibble cleared, high nibble preserved. */
    for (i = 0x09; i <= 0x1B; i++) {
        ASSERT_EQ(g_ev1a_rc[i].combat_aux_block[0xD], 0xA0);
    }
    /* boundaries just outside the range are untouched. */
    ASSERT_EQ(g_ev1a_rc[0x08].combat_aux_block[0xD], 0xA5);
    ASSERT_EQ(g_ev1a_rc[0x1C].combat_aux_block[0xD], 0xA5);

    /* the trigger slot [0x10] was consumed. */
    ASSERT_EQ(g_ev1a_consumed_flags[0x10], 1);

    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * GATE-PASS path (team 1 = npc). An npc is also a non-enemy, so it must take the
 * same FIRE path as the player — this pins the guard as "team != 0" rather than
 * "team == 2". Same observable effects: range disarm + slot [0x10] consumed.
 * ---------------------------------------------------------------- */
static void test_ch7_event1a_npc_steps_disarms_and_consumes(void)
{
    int i;

    ev1a_install_env(1);   /* TEAM_NPC */

    fd2_chapter_event_handler_1a__ch7_char_cond(5);

    for (i = 0x09; i <= 0x1B; i++) {
        ASSERT_EQ(g_ev1a_rc[i].combat_aux_block[0xD], 0xA0);
    }
    ASSERT_EQ(g_ev1a_consumed_flags[0x10], 1);

    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * GATE-FAIL path (team 0 = enemy). An enemy stepping onto the tile must take the
 * SKIP path: NO char in [9,0x1B] is disarmed (every seeded 0xA5 is left intact)
 * and slot [0x10] stays 0 (never consumed).
 * ---------------------------------------------------------------- */
static void test_ch7_event1a_enemy_steps_skips_beat(void)
{
    int i;

    ev1a_install_env(0);   /* TEAM_ENEMY */

    fd2_chapter_event_handler_1a__ch7_char_cond(5);

    /* whole disarm range untouched. */
    for (i = 0x09; i <= 0x1B; i++) {
        ASSERT_EQ(g_ev1a_rc[i].combat_aux_block[0xD], 0xA5);
    }
    /* trigger slot never consumed. */
    ASSERT_EQ(g_ev1a_consumed_flags[0x10], 0);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_1b__ch8_cinematic @ 0x349D9
 *
 * Dispatch idx 0x1B of the per-event handler table at 0x51B91 (chapter 8
 * every-turn cinematic, turn-event slots 0-5 = turns 2..7). A straight-line
 * cinematic beat with NO branch, no RNG, no numeric computation and no
 * CALL-return value used:
 *   pan_cursor_and_window(8, 2);
 *   __delay_thunk_375b2(100);                          // ~100ms hold
 *   load_chapter_portraits_and_dump_tmp(turn_counter); // reload portrait set
 *   __delay_thunk_375b2(100);                          // ~100ms hold
 *
 * Two observable, deterministic contracts are pinned:
 *   1. the portrait-set arg is wired to the battle turn counter
 *      (data_fd2_battle_turn_counter @ 0x53BEF) and the real loader runs
 *      end-to-end against the staged FDICON.B24 + FDFIELD.DAT, rewriting
 *      FD2.TMP to its full 0x32A00 bytes — proving the whole real callee
 *      chain (pan + portrait reload) runs to completion without faulting;
 *   2. both ~100ms holds fire: the testglob __delay_thunk_375b2 recorder
 *      stub sees exactly two calls, the last with ticks == 100 (0x64). This
 *      is what nails the binary's "PUSH 0x64; JMP 0x353D1" tail-jump (into
 *      the shared CALL __delay_thunk_375b2 / RET tail) as a real 100ms hold
 *      rather than fd2_delay_400ms_via_idle_thunk's own 0x190 PUSH.
 *
 * The env is the shared ch25-style real-portrait-reload fixture: alloc_offset
 * 0 (the per-record race scan iterates zero, so the turn-counter arg gates no
 * fd2_init_runtime_char_for_battle but is still consumed by the loader),
 * current_chapter_id 4 (FDFIELD re-read index 4*3+2 = 0xE is valid), an empty
 * active party, and the real compositor workspace for the camera pan. The pure
 * blit/display side effects (camera pan pixels, portrait pixels) are deferred
 * to Phase 9 integration.
 * ================================================================ */
static void test_ch8_event1b_runs_cinematic_with_turn_keyed_reload(void)
{
    ev_install_safe_env();

    /* reset the idle-hold recorder so the per-test count is clean. */
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;

    /* a concrete turn-counter value is handed to the portrait loader as its
     * target_race_id; with alloc_offset 0 the value gates nothing, but the
     * loader still re-reads FDFIELD and rewrites FD2.TMP. */
    data_fd2_battle_turn_counter = 4;

    remove("FD2.TMP");

    fd2_chapter_event_handler_1b__ch8_cinematic(0);

    /* the real portrait reload ran end-to-end: FD2.TMP rewritten to full size. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* exactly two ~100ms holds fired; the last hold carried ticks == 100. */
    ASSERT_EQ((long)g_delay375b2_calls, 2);
    ASSERT_EQ((long)g_delay375b2_last_ticks, (long)0x64);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_1c__ch8_ai_ctrl @ 0x34A0E
 *
 * Dispatch idx 0x1C of the per-event handler table at 0x51B91 (chapter 8
 * turn-event slot 6, turn 15 / phase 0); also tail-called from handler_1D
 * @ 0x34A3C. A straight-line AI-control beat with NO branch, no RNG and no
 * CALL-return value used: a single masked write per slot over the 18
 * runtime-char slots 0x0A..0x1B inclusive:
 *   for (i = 0; i < 0x12; i++)
 *       runtime_char[i + 10].combat_aux_block[0xD] &= 0x80;
 *
 * The testable risk core is the exact MASK and the exact slot RANGE. There is
 * no display side effect, so every effect is a directly observable in-memory
 * write. Seeding combat_aux_block[0xD] = 0xFF for every char makes the mask
 * fully discriminating: an in-range slot must become 0x80 (low 7 bits cleared,
 * bit 7 preserved) — distinguishing the &= 0x80 mask from &= 0xF0 (the low-4
 * variant fd2_set_combat_aux_block_byte_d_low4_for_char_range, which would
 * leave 0xF0), from &= 0x0F (would leave 0x0F) and from a no-op (0xFF). The
 * range boundaries are pinned explicitly: slots 0x09 (just below) and 0x1C
 * (just above) must stay 0xFF, and the bottom/top edges 0x0A and 0x1B must be
 * cleared to 0x80.
 * ================================================================ */

/* local runtime-char array for handler_1C: oversized so the cleared range
 * [0x0A,0x1B] and both just-outside boundary slots (0x09, 0x1C) are
 * in-bounds. */
static runtime_char g_ev1c_rc[64];

static void test_ch8_event1c_clears_low7_bits_for_slots_0a_1b(void)
{
    int i;

    /* seed every char's combat_aux_block[0xD] with 0xFF so a masked write is
     * observable as 0xFF -> 0x80 (bit 7 kept, low 7 cleared) and an untouched
     * char keeps 0xFF. */
    memset(g_ev1c_rc, 0, sizeof(g_ev1c_rc));
    for (i = 0; i < 64; i++) {
        g_ev1c_rc[i].combat_aux_block[0xD] = 0xFF;
    }
    data_fd2_battle_runtime_char_array_ptr = g_ev1c_rc;

    fd2_chapter_event_handler_1c__ch8_ai_ctrl(0);

    /* slots 0x0A..0x1B inclusive (18 chars): low 7 bits cleared, bit 7 kept. */
    for (i = 0x0A; i <= 0x1B; i++) {
        ASSERT_EQ(g_ev1c_rc[i].combat_aux_block[0xD], 0x80);
    }
    /* boundaries just outside the cleared range are untouched. */
    ASSERT_EQ(g_ev1c_rc[0x09].combat_aux_block[0xD], 0xFF);
    ASSERT_EQ(g_ev1c_rc[0x1C].combat_aux_block[0xD], 0xFF);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_1d__unref_dialog_with_state @ 0x34A3C
 *
 * Dispatch idx 0x1D of the per-event handler table at 0x51B91. No chapter
 * FDFIELD turn-event / tile-step hook references this slot (unreferenced —
 * possibly cut content / non-chapter dispatcher). A straight-line, no-branch
 * TWO-BEAT handler with no RNG and no CALL-return value used:
 *   display_dialog_scene(page 2, ...);
 *   fd2_chapter_event_handler_1c__ch8_ai_ctrl(event_arg);   // tail-chain
 *
 * In the binary it prepares its own 8 PUSHes (page=2 + the fixed dialog
 * geometry) and CALLs fd2_display_dialog_scene, then forwards its own incoming
 * arg (PUSH dword ptr [ESP+4]) into handler_1c and returns. The emit reproduces
 * both beats inline; the arg pass-through is kept for byte-faithful equivalence
 * (handler_1c ignores its parameter).
 *
 * Both beats are pinned, each against a REAL emitted callee:
 *   - the dialog beat runs FOR REAL on the per-page-distinct-glyph program
 *     (page p -> single TEXT glyph idx 0x50+p, then END), so a correct page-2
 *     dispatch must emit exactly one glyph with idx 0x52 and any wrong page
 *     fails loudly. The glyph blit is the testglob recorder (g_dlg_glyph_calls
 *     / g_dlg_glyph_last_idx), making the dispatched page observable WITHOUT
 *     touching real VGA;
 *   - the chained handler_1c beat clears bits 0-6 of combat_aux_block[0xD]
 *     (keeping bit 7) for the 18 runtime-char slots 0x0A..0x1B inclusive. With
 *     every char's byte seeded 0xFF, an in-range slot must become 0x80 and the
 *     just-outside boundary slots 0x09 / 0x1C must stay 0xFF — proving the tail
 *     chain into handler_1c actually ran with the exact mask and range.
 *
 * The pure blit/display side effects (dialog glyphs) are deferred to Phase 9
 * integration.
 * ================================================================ */

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), so the dispatched page is identifiable by the recorded glyph idx.
 * Layout (int16 words):
 *   [0..0x10]      header words: page p -> byte offset of its glyph word
 *   [0x11+2*p]     page p glyph (0x50+p)
 *   [0x12+2*p]     page p END (-1)                                          */
static int16 g_ev1d_dlg[0x11 + 2 * 0x11];

static void ev1d_install_safe_env(void)
{
    int p;
    int i;

    /* shared ch25-style env (64-slot g_ev_rc, empty party, gated HUD, throttled
     * palette, real compositor workspace, empty keyboard buffer). It installs an
     * immediate-END dialog program, which the per-page-distinct-glyph program
     * below then overrides. */
    ev_install_safe_env();

    /* per-page (glyph, END) pairs start right after the 0x11 header words, so
     * the page the handler selects is identifiable by the recorded glyph idx. */
    for (p = 0; p <= 0x10; p++) {
        g_ev1d_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev1d_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev1d_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev1d_dlg;

    /* no portrait open on entry, so the END path skips the close sequence and
     * returns immediately. */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* seed every char's combat_aux_block[0xD] with 0xFF so the chained
     * handler_1c masked write is observable as 0xFF -> 0x80 (bit 7 kept, low 7
     * cleared) and an untouched char keeps 0xFF. */
    for (i = 0; i < 64; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xFF;
    }

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
}

/* ----------------------------------------------------------------
 * The handler fires both beats: show dialog page 2 via the real dialog VM, then
 * tail-chain into handler_1c. Observable, deterministic contract: exactly one
 * glyph is emitted and it is page 2's glyph (idx 0x52) — proving the page-2
 * dispatch — AND the chained handler_1c masked slots 0x0A..0x1B to 0x80 while
 * leaving the just-outside boundaries 0x09 / 0x1C at 0xFF.
 * ---------------------------------------------------------------- */
static void test_ch_event1d_shows_dialog_page2_then_chains_handler_1c(void)
{
    int i;

    ev1d_install_safe_env();

    fd2_chapter_event_handler_1d__unref_dialog_with_state(0);

    /* beat 1: exactly page 2 was shown — one glyph, idx 0x52 (= 0x50 + page 2). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x52);

    /* beat 2: the tail chain into handler_1c ran — slots 0x0A..0x1B inclusive
     * (18 chars) had their low 7 bits cleared, bit 7 kept. */
    for (i = 0x0A; i <= 0x1B; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0x80);
    }
    /* boundaries just outside the cleared range are untouched. */
    ASSERT_EQ(g_ev_rc[0x09].combat_aux_block[0xD], 0xFF);
    ASSERT_EQ(g_ev_rc[0x1C].combat_aux_block[0xD], 0xFF);

    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_1e__unref_major_cinematic @ 0x34A7A
 *
 * Dispatch idx 0x1E of the per-event handler table at 0x51B91. No chapter
 * FDFIELD turn-event / tile-step hook references this slot (unreferenced —
 * possibly cut content / non-chapter dispatcher). A major-cinematic beat that
 * mixes pure in-memory state writes with two dialog dispatches and a portrait
 * reload:
 *   for (i = 0xC; i < 0x22; i++)                          // AI-clear loop
 *       runtime_char[i].combat_aux_block[0xD] = 0;
 *   tile_event_table[+3] = (uint8)(turn_counter + 1);     // schedule turn N+1
 *   tile_event_table[+6] = (uint8)(turn_counter + 2);     // schedule turn N+2
 *   runtime_char[0xB].flags = 0;  team = 1; portrait_id = 6; char_id = 6;
 *   runtime_char[0xB].combat_aux_block[0x0A] = 0xFF;
 *   runtime_char[0xB].combat_aux_block[0x0D] = 0x80;      // locked AI bit
 *   runtime_char[0xB].hp_current = 1;                     // spawn at 1 HP
 *   display_dialog_scene(page 2, ...);
 *   load_chapter_portraits_and_dump_tmp(1);
 *   display_dialog_scene(page 3, ...);
 *   pending_xp_credit = 0;
 *   tile_event_consumed_flags[0x10] = 2;                  // consume w/ value 2
 *
 * The testable risk core is the numeric / state logic: the AI-clear slot RANGE
 * (0x0C..0x21 inclusive — slot 0x0B is NOT cleared by the loop, it is the spawn
 * target), the two scheduled turn bytes (a BYTE read + byte increment of the
 * turn counter, so a high turn count wraps mod 256), the exact char-0x0B spawn
 * block (offsets + values), the pending-XP reset, and the consumed-flag written
 * with the DISTINCT value 2 (not the 1 the sibling handlers write). All of that
 * is pinned by test_ch_event1e_state in one shot:
 *   - the runtime-char array and the tile-event table are local buffers so the
 *     loop range, the spawn block and the two scheduled turn bytes are
 *     observable;
 *   - turn_counter is seeded 0xFE, so the byte truncation is fully
 *     discriminating: entry +3 must become 0xFF (0xFE+1) and entry +6 must wrap
 *     to 0x00 (0xFE+2 mod 256) — distinguishing the byte write from a wider one;
 *   - combat_aux_block[0xD] is seeded 0xA5 for every slot, so the loop clear
 *     shows as 0xA5 -> 0 across 0x0C..0x21, the just-below boundary slot 0x0A
 *     stays 0xA5, the just-above boundary slot 0x22 stays 0xA5, and the spawn
 *     slot 0x0B ends 0x80 (overwritten by the spawn, not the loop);
 *   - the two dialog calls + the portrait reload run FOR REAL against the shared
 *     ch25-style env: with the immediate-END dialog program installed by
 *     ev_install_safe_env they return at once (no glyphs), and the real reload
 *     (alloc_offset 0 -> no per-record char init, so it never disturbs the
 *     seeded char array) rewrites FD2.TMP to its full 0x32A00 bytes — proving
 *     the whole real callee chain runs to completion without faulting.
 *
 * The dialog PAGE dispatch (page 2 then page 3) is pinned separately by
 * test_ch_event1e_shows_dialog_pages_2_then_3 against the per-page-distinct-glyph
 * program: exactly two glyphs are emitted and the last is page 3's (idx 0x53).
 * The pure blit/display side effects (dialog glyphs, portrait pixels) are
 * deferred to Phase 9 integration.
 * ================================================================ */

/* tile-event table backing the handler writes its two scheduled turn bytes into
 * (entry +3 and +6). Oversized past the +6 write so both land in-bounds; the
 * real portrait reload never reads it (alloc_offset 0). */
static uint8 g_ev1e_tile_table[0x10];

/* backing for the 0x20-byte tile-event consumed-flags block: byte [0x10] is the
 * slot this handler consumes with the distinct value 2. */
static uint8 g_ev1e_consumed_flags[0x20];

/* per-page-distinct-glyph dialog program (page p -> single glyph idx 0x50+p,
 * then END), so the dispatched page is identifiable by the recorded glyph idx.
 * Layout (int16 words):
 *   [0..0x10]      header words: page p -> byte offset of its glyph word
 *   [0x11+2*p]     page p glyph (0x50+p)
 *   [0x12+2*p]     page p END (-1)                                          */
static int16 g_ev1e_dlg[0x11 + 2 * 0x11];

/* ----------------------------------------------------------------
 * State / numeric risk core. Drives the handler against the shared ch25-style
 * real-reload env (dialogs return immediately via the immediate-END program,
 * the real portrait reload rewrites FD2.TMP) and pins every in-memory write:
 * the AI-clear range 0x0C..0x21 with both boundaries, the two byte-truncated
 * scheduled turns (0xFE -> 0xFF / 0x00), the full char-0x0B spawn block, the
 * pending-XP reset, and the consumed-flag written with the distinct value 2.
 * ---------------------------------------------------------------- */
static void test_ch_event1e_state(void)
{
    int i;

    /* shared ch25-style real-reload env: empty party, real compositor
     * workspace, immediate-END dialog program, alloc_offset 0, chapter 4,
     * 64-slot g_ev_rc (memset to 0). */
    ev_install_safe_env();

    /* seed every char's combat_aux_block[0xD] with 0xA5 so the loop clear shows
     * as 0xA5 -> 0, the spawn slot ends 0x80, and untouched boundary slots keep
     * 0xA5. */
    for (i = 0; i < 64; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xA5;
    }

    /* point the tile-event table at a local buffer so the two scheduled turn
     * bytes (entry +3 / +6) are observable; seed it with a sentinel so the
     * writes are visible against a known background. */
    memset(g_ev1e_tile_table, 0x5A, sizeof(g_ev1e_tile_table));
    data_fd2_tile_event_data_table_ptr = (uint32)g_ev1e_tile_table;

    /* turn counter 0xFE so the byte increments wrap: +1 -> 0xFF, +2 -> 0x00. */
    data_fd2_battle_turn_counter = 0xFE;

    /* point the consumed-flags block at a local buffer; seed [0x10] with a
     * sentinel so the write of the distinct value 2 is observable. */
    memset(g_ev1e_consumed_flags, 0, sizeof(g_ev1e_consumed_flags));
    g_ev1e_consumed_flags[0x10] = 0x77;
    data_fd2_field_map_tile_event_consumed_flags_ptr =
        (uint32)g_ev1e_consumed_flags;

    /* perturb pending XP so its reset to 0 is observable. */
    data_fd2_battle_pending_xp_credit = 0x12345678;

    remove("FD2.TMP");

    fd2_chapter_event_handler_1e__unref_major_cinematic(0);

    /* AI-clear loop: slots 0x0C..0x21 inclusive cleared to 0. */
    for (i = 0x0C; i <= 0x21; i++) {
        ASSERT_EQ(g_ev_rc[i].combat_aux_block[0xD], 0);
    }
    /* boundary just below the loop (slot 0x0A) untouched; slot 0x0B is the spawn
     * target (checked below); boundary just above the loop (slot 0x22) untouched. */
    ASSERT_EQ(g_ev_rc[0x0A].combat_aux_block[0xD], 0xA5);
    ASSERT_EQ(g_ev_rc[0x22].combat_aux_block[0xD], 0xA5);

    /* two scheduled turn bytes, byte-truncated: +3 -> 0xFF, +6 -> 0x00. The
     * surrounding sentinel bytes are left intact (only +3 and +6 are written). */
    ASSERT_EQ(g_ev1e_tile_table[3], 0xFF);
    ASSERT_EQ(g_ev1e_tile_table[6], 0x00);
    ASSERT_EQ(g_ev1e_tile_table[2], 0x5A);
    ASSERT_EQ(g_ev1e_tile_table[4], 0x5A);
    ASSERT_EQ(g_ev1e_tile_table[5], 0x5A);
    ASSERT_EQ(g_ev1e_tile_table[7], 0x5A);

    /* char-0x0B spawn block. */
    ASSERT_EQ(g_ev_rc[0x0B].flags, 0);
    ASSERT_EQ(g_ev_rc[0x0B].team, 1);
    ASSERT_EQ(g_ev_rc[0x0B].portrait_id, 6);
    ASSERT_EQ(g_ev_rc[0x0B].char_id, 6);
    ASSERT_EQ(g_ev_rc[0x0B].combat_aux_block[0x0A], 0xFF);
    ASSERT_EQ(g_ev_rc[0x0B].combat_aux_block[0x0D], 0x80);
    ASSERT_EQ((long)g_ev_rc[0x0B].hp_current, 1);

    /* pending XP reset. */
    ASSERT_EQ((long)data_fd2_battle_pending_xp_credit, 0);

    /* consumed-flag slot [0x10] written with the DISTINCT value 2. */
    ASSERT_EQ(g_ev1e_consumed_flags[0x10], 2);

    /* the real portrait reload ran end-to-end: FD2.TMP rewritten to full size. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Dialog PAGE dispatch. Against the per-page-distinct-glyph program (page p ->
 * single glyph idx 0x50+p, then END), the handler's two dialog calls must emit
 * exactly two glyphs and the LAST must be page 3's (idx 0x53) — proving the
 * second dialog dispatches page 3. (The recorder keeps only the last idx, so the
 * first page is not independently provable here; the count of 2 confirms exactly
 * two dialogs fired.)
 * ---------------------------------------------------------------- */
static void test_ch_event1e_shows_dialog_pages_2_then_3(void)
{
    int p;

    /* shared ch25-style env (real reload, empty party); then override its
     * immediate-END dialog program with the per-page-distinct-glyph program. */
    ev_install_safe_env();

    for (p = 0; p <= 0x10; p++) {
        g_ev1e_dlg[p] = (int16)((0x11 + 2 * p) * 2);   /* byte offset of glyph */
        g_ev1e_dlg[0x11 + 2 * p] = (int16)(0x50 + p);  /* page p glyph idx */
        g_ev1e_dlg[0x12 + 2 * p] = -1;                  /* page p END */
    }
    current_chapter_text = (uint32)g_ev1e_dlg;

    /* no portrait open on entry, so each dialog END path returns at once. */
    data_fd2_dialog_active_portrait_blit_offset = 0;

    /* point the tile-event table + consumed-flags at local buffers so the
     * handler's state writes do not fault on a null pointer. */
    memset(g_ev1e_tile_table, 0, sizeof(g_ev1e_tile_table));
    data_fd2_tile_event_data_table_ptr = (uint32)g_ev1e_tile_table;
    memset(g_ev1e_consumed_flags, 0, sizeof(g_ev1e_consumed_flags));
    data_fd2_field_map_tile_event_consumed_flags_ptr =
        (uint32)g_ev1e_consumed_flags;

    /* reset the glyph recorder so the per-test count is clean. */
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    remove("FD2.TMP");

    fd2_chapter_event_handler_1e__unref_major_cinematic(0);

    /* exactly two dialogs fired; the last dispatched page 3 (idx 0x53). */
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x53);

    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ev_restore_rc_ptr();
}

/* ================================================================
 * fd2_chapter_event_handler_1f__ch9_reinforcement @ 0x34B5D
 *
 * Dispatch idx 0x1F of the per-event handler table at 0x51B91 (chapter 9
 * turn-event slots 0+1, the race_id 0+1 batch reinforcements). A straight-line
 * spawner cinematic with NO branch, no RNG, and no CALL-return value used:
 *   load_chapter_portraits_and_dump_tmp(tile_event_consumed_flags[0x10]); // load batch N
 *   tile_event_consumed_flags[0x10]++;                                    // advance counter
 *   pan_cursor_and_window(0,    0);    __delay_thunk_375b2(200);          // TL
 *   pan_cursor_and_window(0xC,  0);    __delay_thunk_375b2(200);          // TR
 *   pan_cursor_and_window(0xC,  0xB);  __delay_thunk_375b2(200);          // BR
 *   pan_cursor_and_window(0,    0xB);  __delay_thunk_375b2(200);          // BL
 *
 * Two observable, deterministic contracts are pinned:
 *   1. the batch counter is byte [0x10] of the tile-event consumed-flags block
 *      (pointed at by data_fd2_field_map_tile_event_consumed_flags_ptr, here
 *      backed by a local buffer so the read-then-advance is observable): the
 *      seeded value selects the batch loaded, and the handler advances it by
 *      exactly 1 afterward. Seeding a non-0 / non-1 value also pins that this
 *      handler is UNGATED (no first-time gate like the sibling 0x19) — a pure
 *      read-and-increment, not a conditional. The real loader runs end-to-end
 *      against the staged FDICON.B24 + FDFIELD.DAT, rewriting FD2.TMP to its
 *      full 0x32A00 bytes — proving the whole real callee chain (portrait
 *      reload + the four corner pans) runs to completion without faulting;
 *   2. all four 200ms holds fire: the testglob __delay_thunk_375b2 recorder
 *      stub sees exactly four calls, the last with ticks == 200 (0xC8). The
 *      final hold nails the binary's "PUSH 0xC8; JMP 0x353D1" tail-jump (into
 *      the shared CALL __delay_thunk_375b2 / RET tail) as a real 200ms hold.
 *
 * The env is the shared ch25-style real-portrait-reload fixture: alloc_offset
 * 0 (the per-record race scan iterates zero, so the seeded batch counter gates
 * no fd2_init_runtime_char_for_battle but is still consumed by the loader),
 * current_chapter_id 4 (FDFIELD re-read index 4*3+2 = 0xE is valid), an empty
 * active party, and the real compositor workspace for the four camera pans. The
 * pure blit/display side effects (the four corner pans' pixels, portrait pixels)
 * are deferred to Phase 9 integration.
 * ================================================================ */

/* backing for the 0x20-byte tile-event consumed-flags block: byte [0x10] is the
 * race_id batch counter this handler reads and advances. */
static uint8 g_ev1f_consumed_flags[0x20];

static void test_ch9_event1f_loads_batch_and_advances_counter(void)
{
    ev_install_safe_env();

    /* reset the idle-hold recorder so the per-test count is clean. */
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;

    /* point the consumed-flags global at the local block and seed the batch
     * counter to a non-0 / non-1 value: the handler must load that batch and
     * advance the counter unconditionally (no first-time gate). */
    memset(g_ev1f_consumed_flags, 0, sizeof(g_ev1f_consumed_flags));
    data_fd2_field_map_tile_event_consumed_flags_ptr =
        (uint32)g_ev1f_consumed_flags;
    g_ev1f_consumed_flags[0x10] = 5;

    remove("FD2.TMP");

    fd2_chapter_event_handler_1f__ch9_reinforcement(0);

    /* the real portrait reload ran end-to-end: FD2.TMP rewritten to full size. */
    ASSERT_EQ(ev_fd2_tmp_size(), 0x32A00);

    /* the batch counter advanced by exactly 1 (read 5 -> stored 6). */
    ASSERT_EQ(g_ev1f_consumed_flags[0x10], 6);

    /* all four corner holds fired; the last hold carried ticks == 200. */
    ASSERT_EQ((long)g_delay375b2_calls, 4);
    ASSERT_EQ((long)g_delay375b2_last_ticks, (long)0xC8);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
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
    RUN_TEST(test_ch7_event1a_player_steps_disarms_and_consumes);
    RUN_TEST(test_ch7_event1a_npc_steps_disarms_and_consumes);
    RUN_TEST(test_ch7_event1a_enemy_steps_skips_beat);
    RUN_TEST(test_ch8_event1b_runs_cinematic_with_turn_keyed_reload);
    RUN_TEST(test_ch8_event1c_clears_low7_bits_for_slots_0a_1b);
    RUN_TEST(test_ch_event1d_shows_dialog_page2_then_chains_handler_1c);
    RUN_TEST(test_ch_event1e_state);
    RUN_TEST(test_ch_event1e_shows_dialog_pages_2_then_3);
    RUN_TEST(test_ch9_event1f_loads_batch_and_advances_counter);
    printf("\n");
}
