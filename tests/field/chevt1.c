/*
 * unit tests for src/field/chevt1.c
 *
 * fd2_chapter_event_handler_00__ch1_dialog_with_state @ 0x341DB is the
 * chapter-1 turn-event slot 0 (dispatch idx 0 of the per-event handler table
 * at 0x51B91). It is a straight-line, no-branch dialog/cutscene beat: no RNG,
 * no numeric computation, no CALL-return value used. Its testable risk core is
 * the deterministic, non-display state it mutates and that the whole call
 * sequence runs to completion without faulting:
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
 * The handler is driven end-to-end on-host with the proven chend2 safe env
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

extern runtime_char g_test_rc_array[8];
extern void *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];

/* fd2_check_char_is_dead is now the real routine in src/battle/battle.c; it
 * reads runtime_char[idx].flags bit0 through data_fd2_battle_runtime_char_array_ptr.
 * ev_install_safe_env points that pointer at g_ev_rc, so the handler_09 guard
 * for 沃斯 (char 6) is pinned by setting g_ev_rc[6].flags (CHARFLAG_DEAD). */

/* 64-slot runtime-char fixture (the handler's callees touch active battle
 * slots; an oversized array keeps every write in-bounds). */
static runtime_char g_ev_rc[64];

/* compositor workspace span the real fd2_blit_rectangle reads:
 * (h-1)*stride + w = 191*0x1C8 + 0x138. */
#define EV_WS_SPAN (191u * 0x1C8u + 0x138u)
static uint8 g_ev_ws_buffer[EV_WS_SPAN];

/* 768-byte VGA palette backing for any real palette read. */
static uint8 g_ev_palette[768];

/* menu roster buffer for the real recruit (64 slots x 0x50 bytes). */
static uint8 g_ev_roster[64 * 0x50];

/* immediate-END dialog text program: a page-offset table (indices 0..0x10)
 * each pointing at a single -1 END opcode at the tail. */
static int16 g_ev_dlg[0x12];

/* zero-group cutscene scripts for events 7 and 8: n_groups byte = 0, so the
 * real fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev_script_07[1] = { 0 };
static uint8 g_ev_script_08[1] = { 0 };

static void ev_install_safe_env(void)
{
    int i;

    /* runtime-char slots the handler's callees touch. */
    memset(g_ev_rc, 0, sizeof(g_ev_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ev_rc;

    /* empty active party: char-overlay/shadow + save loops no-op. */
    data_fd2_battle_party_member_count = 0;

    /* real compositor workspace backing. */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ev_ws_buffer - 0x8088;

    /* anim_phase=0 -> cursor-overlay switch falls through (no blits). */
    data_fd2_battle_anim_phase = 0;

    /* HUD panel gated off. */
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;

    /* palette-cycle to its no-op early-return path (no extra VGA writes). */
    data_fd2_animation_palette_cycle_last_tick =
        (uint16)data_fd2_input_idle_current_bios_tick_word;

    /* 768-byte palette so any real palette read stays in-bounds. */
    memset(g_ev_palette, 0, sizeof(g_ev_palette));
    data_fd2_vga_palette_data_ptr = (uint32)g_ev_palette;

    /* immediate-END dialog program (pages 0..0x10 -> single END opcode). */
    for (i = 0; i <= 0x10; i++) {
        g_ev_dlg[i] = (int16)(0x11 * 2);   /* byte offset of the END opcode */
    }
    g_ev_dlg[0x11] = -1;                    /* END */
    current_chapter_text = (uint32)g_ev_dlg;

    /* empty BIOS keyboard buffer (head==tail) for fd2_clear_keyboard_buffer. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;

    /* zero-group cutscene scripts for the two events the handler fires. */
    g_ev_script_07[0] = 0;
    g_ev_script_08[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[7] = g_ev_script_07;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[8] = g_ev_script_08;

    /* menu roster for the real recruit; start empty. */
    memset(g_ev_roster, 0, sizeof(g_ev_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ev_roster;
    data_fd2_shared_menu_party_member_count = 0;

    /* ch25-style real portrait reload: empty tile-event scan (alloc_offset 0
     * -> no per-record fd2_init_runtime_char_for_battle), fresh field buffer,
     * and a valid FDFIELD re-read index (chapter 4 -> 4*3+2 = 0xE). */
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    chapter_portrait_load_buffer = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_chapter_current_chapter_id = 4;
}

static void ev_restore_rc_ptr(void)
{
    /* leave the shared pointer where other suites expect it. */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}

static long ev_fd2_tmp_size(void)
{
    FILE *fp;
    long n;

    fp = fopen("FD2.TMP", "rb");
    if (fp == NULL) return -1;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fclose(fp);
    return n;
}

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
    int i;

    g_animate_party_addition_calls = 0;
    g_animate_party_addition_last_chapter = 0;

    /* runtime-char slots the real callees touch. */
    memset(g_ev_rc, 0, sizeof(g_ev_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ev_rc;

    /* empty active party: the real composite/paint char loops iterate zero. */
    data_fd2_battle_party_member_count = 0;

    /* real compositor workspace backing (handler_00 convention: the logical
     * row-0 the real fd2_composite_battle_frame reads is at ptr+0x8088). */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ev_ws_buffer - 0x8088;

    /* anim_phase=0 -> cursor-overlay switch falls through (no blits). */
    data_fd2_battle_anim_phase = 0;

    /* HUD panel gated off. */
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;

    /* palette-cycle to its no-op early-return path (no extra VGA writes). */
    data_fd2_animation_palette_cycle_last_tick =
        (uint16)data_fd2_input_idle_current_bios_tick_word;

    /* 768-byte palette so any real palette read stays in-bounds. */
    memset(g_ev_palette, 0, sizeof(g_ev_palette));
    data_fd2_vga_palette_data_ptr = (uint32)g_ev_palette;

    /* immediate-END dialog program (pages 0..0x10 -> single END opcode). */
    for (i = 0; i <= 0x10; i++) {
        g_ev_dlg[i] = (int16)(0x11 * 2);   /* byte offset of the END opcode */
    }
    g_ev_dlg[0x11] = -1;                    /* END */
    current_chapter_text = (uint32)g_ev_dlg;

    /* empty BIOS keyboard buffer (head==tail) for fd2_clear_keyboard_buffer. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;

    /* zero-group cutscene script for the one event (3) the handler fires. */
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

void run_field_chevt1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt1\n");
    RUN_TEST(test_ch1_event0_recruits_hanuo_and_reloads_portraits);
    RUN_TEST(test_ch1_event1_fires_appear_anim_for_slot4);
    RUN_TEST(test_ch1_event2_fires_appear_anim_for_slot5);
    RUN_TEST(test_ch1_event3_reloads_race6_brackets_initphase);
    RUN_TEST(test_ch_event4_flips_hawat_to_ally);
    RUN_TEST(test_ch2_event6_arms_reinforcement_enemies);
    RUN_TEST(test_ch3_event9_char6_alive_reloads_and_shows_dialog);
    RUN_TEST(test_ch3_event9_char6_dead_skips_beat);
    RUN_TEST(test_ch4_event0b_reloads_portraits_and_shows_dialog);
    RUN_TEST(test_ch_event0c_firsttime_arms_ai_flag7_and_consumes);
    RUN_TEST(test_ch_event0c_already_consumed_skips_beat);
    printf("\n");
}
