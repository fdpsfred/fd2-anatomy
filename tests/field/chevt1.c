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

void run_field_chevt1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt1\n");
    RUN_TEST(test_ch1_event0_recruits_hanuo_and_reloads_portraits);
    printf("\n");
}
