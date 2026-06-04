/*
 * unit tests for src/field/chend2.c
 *
 * fd2_chapter_20_end is a heavy chapter-end cutscene transition. Its
 * deterministic, non-display state mutations are the testable risk core:
 *   - copy the four ch20 position tables onto runtime_char slots 0..15
 *     (scene1, facing=1) and slots 0x34..0x3C (scene2, facing=3);
 *   - re-aim camera/cursor (window origin + cursor world @ 0x1A,0x1F,
 *     cursor screen @ 0,0; anim_phase=0);
 *   - recruit 謝多 (char 0x19) unconditionally;
 *   - advance current_chapter_id.
 *
 * The function is driven for real end-to-end on the turn_counter >= 16 path,
 * which skips the int386 + FDICON.B24/FDFIELD.DAT portrait-load branch and
 * the 達可賽 (char 0x1C) recruit. To keep the real display/dialog/cutscene
 * callees host-safe and non-blocking the fixture, mirroring tests/field/
 * chtrans.c's proven safe-render env:
 *   - empties the party (member_count=0) so the real char-overlay / shadow /
 *     save-template loops iterate zero chars, and points runtime_char at a
 *     64-entry local array (the handler writes slots up to 0x3C);
 *   - backs the compositor workspace with a real buffer so the real
 *     fd2_blit_rectangle reads a valid source (VGA-side write is harmless in
 *     DOSBox), with the HUD panel gated off and anim_phase=0 so the cursor
 *     overlay takes its no-op path;
 *   - stages a 768-byte zero palette so the real fade-out/in's
 *     fd2_set_vga_palette_range reads in-bounds;
 *   - installs an immediate-END dialog text program (every page offset points
 *     at a -1 END opcode) so the real fd2_display_dialog_scene returns at once
 *     with no glyph blits and never reaches the page-break busy-wait;
 *   - installs a 0-group cutscene script for event 0x3B so the interpreter
 *     just composites once;
 *   - points the menu roster buffer at a real 64-slot buffer so the real
 *     recruit (fd2_init_runtime_char_from_base_growth) writes in-bounds.
 *
 * The full turn_counter < 16 path (real DOS-interrupt portrait load + the
 * 達可賽 recruit) is pure display/file orchestration with a DOS int386 that
 * cannot run on the host harness, so its behavioral coverage is deferred to
 * Phase 9 integration.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];
extern void *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];

/* 64-slot runtime-char fixture (handler writes slots 0..0x3C). */
static runtime_char g_ce_rc[64];

/* compositor workspace span the real fd2_blit_rectangle reads:
 * (h-1)*stride + w = 191*0x1C8 + 0x138. */
#define CE_WS_SPAN (191u * 0x1C8u + 0x138u)
static uint8 g_ce_ws_buffer[CE_WS_SPAN];

/* 768-byte VGA palette backing for the real palette-fade routines. */
static uint8 g_ce_palette[768];

/* menu roster buffer for the real recruit (64 slots x 0x50 bytes). */
static uint8 g_ce_roster[64 * 0x50];

/* immediate-END dialog text program: a page-offset table (indices 0..0x10)
 * each pointing at a single -1 END opcode at the tail. */
static int16 g_ce_dlg[0x12];

/* one-group-less cutscene script: n_groups = 0 -> interpreter just
 * composites once and returns. */
static uint8 g_ce_script[1] = { 0 };

static void ce_install_safe_env(void)
{
    int i;

    /* runtime-char slots written by the handler. */
    memset(g_ce_rc, 0, sizeof(g_ce_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ce_rc;

    /* empty active party: char-overlay/shadow + save-template loops no-op. */
    data_fd2_battle_party_member_count = 0;

    /* real compositor workspace backing. */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce_ws_buffer - 0x8088;

    /* anim_phase=0 -> cursor-overlay switch falls through (no blits). */
    data_fd2_battle_anim_phase = 0;

    /* HUD panel gated off (both flags default 0, set explicitly for clarity). */
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;

    /* palette-cycle to its no-op early-return path (no extra VGA writes). */
    data_fd2_animation_palette_cycle_last_tick =
        (uint16)data_fd2_input_idle_current_bios_tick_word;

    /* 768-byte palette so the real fade reads stay in-bounds. */
    memset(g_ce_palette, 0, sizeof(g_ce_palette));
    data_fd2_vga_palette_data_ptr = (uint32)g_ce_palette;

    /* immediate-END dialog program. */
    for (i = 0; i <= 0x10; i++) {
        g_ce_dlg[i] = (int16)(0x11 * 2);   /* byte offset of the END opcode */
    }
    g_ce_dlg[0x11] = -1;                    /* END */
    current_chapter_text = (uint32)g_ce_dlg;

    /* empty BIOS keyboard buffer (head==tail). */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;

    /* 0-group cutscene script for event 0x3B. */
    g_ce_script[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x3B] = g_ce_script;

    /* menu roster for the real recruit; start with an empty roster. */
    memset(g_ce_roster, 0, sizeof(g_ce_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce_roster;
    data_fd2_shared_menu_party_member_count = 0;
}

static void ce_restore_rc_ptr(void)
{
    /* leave the shared pointer where other suites expect it. */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}

/* ----------------------------------------------------------------
 * turn_counter >= 16: the handler places both position groups, advances the
 * chapter id, recruits exactly one char (謝多 0x19), and skips the 達可賽
 * (0x1C) recruit.
 * ---------------------------------------------------------------- */
static void test_ch20_end_places_chars_and_advances(void)
{
    int i;
    uint32 chap0;

    ce_install_safe_env();
    data_fd2_battle_turn_counter = 16;          /* >= 0x10 -> short path */
    chap0 = data_fd2_chapter_current_chapter_id;

    fd2_chapter_20_end();

    /* scene1: slots 0..15 placed at the scene1 tables, facing south-east=1. */
    for (i = 0; i < 16; i++) {
        ASSERT_EQ(g_ce_rc[i].pos_x,
                  data_fd2_chapter_ch20_end_scene1_char_pos_x_table[i]);
        ASSERT_EQ(g_ce_rc[i].pos_y,
                  data_fd2_chapter_ch20_end_scene1_char_pos_y_table[i]);
        ASSERT_EQ(g_ce_rc[i].sprite_state[1], 1);
    }

    /* scene2: slots 0x34..0x3C placed at the scene2 tables, facing=3. */
    for (i = 0; i < 9; i++) {
        ASSERT_EQ(g_ce_rc[i + 0x34].pos_x,
                  data_fd2_chapter_ch20_end_scene2_char_pos_x_table[i]);
        ASSERT_EQ(g_ce_rc[i + 0x34].pos_y,
                  data_fd2_chapter_ch20_end_scene2_char_pos_y_table[i]);
        ASSERT_EQ(g_ce_rc[i + 0x34].sprite_state[1], 3);
    }

    /* chapter id advanced by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    /* exactly one recruit on the short path (謝多 only, not 達可賽). */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 1);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Camera/cursor re-aim: window origin + cursor world land at (0x1A,0x1F),
 * cursor screen at (0,0), anim_phase reset to 0.
 * ---------------------------------------------------------------- */
static void test_ch20_end_reaims_camera(void)
{
    ce_install_safe_env();
    data_fd2_battle_turn_counter = 16;

    /* perturb to non-target values first. */
    data_fd2_battle_view_window_origin_x = 0x77;
    data_fd2_battle_view_window_origin_y = 0x77;
    data_fd2_battle_cursor_world_x = 0x77;
    data_fd2_battle_cursor_world_y = 0x77;
    data_fd2_battle_cursor_screen_x = 0x77;
    data_fd2_battle_cursor_screen_y = 0x77;

    fd2_chapter_20_end();

    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 0x1A);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 0x1F);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 0x1A);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 0x1F);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 0);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 0);
    ASSERT_EQ(data_fd2_battle_anim_phase, 0);

    ce_restore_rc_ptr();
}

void run_field_chend2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chend2\n");
    RUN_TEST(test_ch20_end_places_chars_and_advances);
    RUN_TEST(test_ch20_end_reaims_camera);
    printf("\n");
}
