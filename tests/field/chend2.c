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

/* ----------------------------------------------------------------
 * Chapter 21 end handler — fd2_chapter_21_end @ 0x240FA.
 *
 * Risk core: the deterministic 6-item collection decision and its branch.
 * The handler scans chars 0..15 for each of the six collectible item ids
 * 0xD1..0xD6 (黃金徽章 + 5 顆眼) via fd2_find_inventory_slot_with_item and
 * counts the holders into `collected`; only an exact total of 6 unlocks the
 * hidden-stage branch (consume the six items, award item 100 天空之鑰, fire
 * cutscene events 0x3F/0x40, play the FDOTHER.DAT sprite slideshow, final
 * page 10). Any other total takes the page-6 path. Both paths re-init
 * 希爾法 (0x18) and 羅蘭 (0x17) from base+growth, save the runtime
 * templates, and advance current_chapter_id.
 *
 * Three callees are not yet emitted and are stubbed in tests/testglob.c:
 *   - fd2_find_inventory_slot_with_item — a programmable double; its
 *     g_ce_find_have_d6 gate decides whether the 0xD6 holder exists, so the
 *     count lands on exactly 6 (all collected) or 5 (one missing). It maps
 *     item 0xD1->char 0 .. 0xD6->char 5, returning slot 0 for a hold.
 *   - fd2_setup_chars_and_camera_for_intro — no-op (its real placement/
 *     palette work is display-only; deferred to Phase 9).
 *   - fd2_play_chapter_intro_sprite_slideshow — no-op (it memmoves 64000
 *     bytes to/from the absolute VGA framebuffer 0xA0000 and loads
 *     FDOTHER.DAT; deferred to Phase 9). Stubbing it lets the all-collected
 *     branch run on-host so the item-100 award is observable.
 *
 * The all-collected branch is observed via the REAL
 * fd2_give_item_to_first_player_char(100), which only runs on that branch:
 * the first bTeam==2 char with an empty inventory slot receives item 100.
 * The remaining branch callees (remove-slot, cutscene-trigger, dialog,
 * recruit, save-template) are the already-emitted real functions.
 * ---------------------------------------------------------------- */

extern int g_ce_find_have_d6;
extern int g_ce_find_calls;

/* zero-group cutscene scripts for events 0x3F / 0x40 (all-collected branch):
 * n_groups byte = 0, so the real fd2_cutscene_event_trigger just composites
 * once and returns. */
static uint8 g_ce21_script_3f[1] = { 0 };
static uint8 g_ce21_script_40[1] = { 0 };

/* ----------------------------------------------------------------
 * Partial collection (5 holders): collected == 5 != 6, so the page-6 path
 * is taken — it must NOT award the hidden key (item 100). The full count
 * double-loop runs (6 items x 16 chars = 96 find calls), both recruits run
 * (希爾法 + 羅蘭), and the chapter id advances by one.
 * ---------------------------------------------------------------- */
static void test_ch21_end_partial_collection_page6_path(void)
{
    uint32 chap0;

    ce_install_safe_env();
    chap0 = data_fd2_chapter_current_chapter_id;

    /* a lone player char with an empty inventory: if the all-collected
     * branch were wrongly taken it would receive item 100 here. */
    data_fd2_battle_party_member_count = 1;
    g_ce_rc[0].team = 2;
    g_ce_rc[0].inventory_slots[0] = 0x80;   /* empty slot 0 */

    g_ce_find_have_d6 = 0;                   /* 0xD6 absent -> collected = 5 */
    g_ce_find_calls = 0;

    fd2_chapter_21_end();

    /* the full count double-loop ran: 6 item ids x 16 chars. */
    ASSERT_EQ(g_ce_find_calls, 6 * 16);

    /* page-6 path: the key was NOT awarded (slot 0 stayed empty). */
    ASSERT_EQ(g_ce_rc[0].inventory_slots[0], 0x80);

    /* both recruits ran (希爾法 + 羅蘭). */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 2);

    /* chapter id advanced by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Full collection (6 holders): collected == 6, so the hidden-stage branch
 * is taken — it consumes the six items and awards item 100 (天空之鑰) to
 * the first player char via the real fd2_give_item_to_first_player_char,
 * fires cutscene events 0x3F/0x40, and (after the deferred slideshow) shows
 * final page 10. Observed via the real key award; recruits + chapter
 * advance still run.
 * ---------------------------------------------------------------- */
static void test_ch21_end_full_collection_awards_key(void)
{
    uint32 chap0;

    ce_install_safe_env();
    chap0 = data_fd2_chapter_current_chapter_id;

    /* lone player char (索爾) with a fully-empty inventory so the real
     * fd2_add_item_to_inventory finds slot 0 for the key. */
    data_fd2_battle_party_member_count = 1;
    g_ce_rc[0].team = 2;
    g_ce_rc[0].inventory_slots[0]  = 0x80;
    g_ce_rc[0].inventory_slots[2]  = 0x80;
    g_ce_rc[0].inventory_slots[4]  = 0x80;
    g_ce_rc[0].inventory_slots[6]  = 0x80;
    g_ce_rc[0].inventory_slots[8]  = 0x80;
    g_ce_rc[0].inventory_slots[10] = 0x80;
    g_ce_rc[0].inventory_slots[12] = 0x80;
    g_ce_rc[0].inventory_slots[14] = 0x80;

    /* zero-group cutscene scripts for the two events fired on this branch. */
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x3F] = g_ce21_script_3f;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x40] = g_ce21_script_40;

    g_ce_find_have_d6 = 1;                   /* all six present -> collected = 6 */

    fd2_chapter_21_end();

    /* hidden-stage branch ran: char 0 received item 100 in a now-occupied
     * slot 0 (flag cleared, item id = 100). */
    ASSERT_EQ(g_ce_rc[0].inventory_slots[0], 0);
    ASSERT_EQ(g_ce_rc[0].inventory_slots[1], 100);

    /* both recruits ran and chapter advanced on this branch too. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 2);
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    ce_restore_rc_ptr();
}

void run_field_chend2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chend2\n");
    RUN_TEST(test_ch20_end_places_chars_and_advances);
    RUN_TEST(test_ch20_end_reaims_camera);
    RUN_TEST(test_ch21_end_partial_collection_page6_path);
    RUN_TEST(test_ch21_end_full_collection_awards_key);
    printf("\n");
}
