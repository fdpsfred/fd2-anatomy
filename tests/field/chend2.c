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
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];
extern void *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];

/* __delay_thunk_375b2 call recorder (tests/testglob.c) — used by the ch29
 * suite to confirm both palette fade loops ran to completion. */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;

/* fd2_kill_runtime_chars_from_index_to_end recorder double (tests/testglob.c)
 * — used by the ch29 suite to confirm the kill call passes start index 0x14. */
extern int    g_ce_kill_from_calls;
extern uint32 g_ce_kill_from_last_idx;

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
extern int g_ce_find_have_item100;
extern int g_ce_find_calls;

/* zero-group cutscene scripts for events 0x3F / 0x40 (all-collected branch):
 * n_groups byte = 0, so the real fd2_cutscene_event_trigger just composites
 * once and returns. */
static uint8 g_ce21_script_3f[1] = { 0 };
static uint8 g_ce21_script_40[1] = { 0 };

/* zero-group cutscene scripts for ch22's events 0x41 / 0x42. */
static uint8 g_ce22_script_41[1] = { 0 };
static uint8 g_ce22_script_42[1] = { 0 };

/* zero-group cutscene scripts for ch23's events 0x47 / 0x48 / 0x49: n_groups
 * byte = 0, so the real fd2_cutscene_event_trigger just composites once and
 * returns. */
static uint8 g_ce23_script_47[1] = { 0 };
static uint8 g_ce23_script_48[1] = { 0 };
static uint8 g_ce23_script_49[1] = { 0 };

/* immediate-END dialog text program for ch23. The shared ce_install_safe_env
 * program only covers page offsets 0..0x10, but fd2_chapter_23_end's final
 * dialog uses page 0x11 (17); so a ch23-local program is installed whose
 * page-offset table (indices 0..0x11) all point at a single -1 END opcode at
 * the tail. Every fd2_display_dialog_scene call then returns at once with no
 * glyph blits and never reaches the page-break busy-wait. */
static int16 g_ce23_dlg[0x13];

/* Phase-1 predicates for fd2_chapter_23_end are now both driven through REAL
 * functions: the 天空之鑰 arm via fd2_any_char_has_item (g_ce_find_have_item100
 * makes char 0 hold item 100), and the 蜜蒂 arm via fd2_find_template_char_by_id,
 * which linear-scans the template roster (g_ce_roster, see ce_install_safe_env)
 * for char_id 0x12 at +0x08. ce23_seed_miti_in_roster() pre-places a 蜜蒂 entry
 * so the predicate's "present" arm is reached; leaving it out keeps the roster
 * free of 0x12 so the "absent" arm runs (the recruits write 0x16/0x13, not 0x12,
 * so they never spuriously satisfy the predicate). */
static void ce23_seed_miti_in_roster(void)
{
    uint8 *roster = (uint8 *)data_fd2_shared_menu_party_roster_buffer_ptr;
    /* place 蜜蒂 (char_id 0x12) at +0x08 of the first template slot and account
     * for it in the member count; later recruits append after this entry. */
    roster[data_fd2_shared_menu_party_member_count * 0x50 + 8] = 0x12;
    data_fd2_shared_menu_party_member_count += 1;
}

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

/* ----------------------------------------------------------------
 * Chapter 22 end handler — fd2_chapter_22_end @ 0x244B6.
 *
 * fd2_chapter_22_end is a straight-line, no-branch display handler (no RNG,
 * no numeric computation). Its testable risk core is the call sequence
 * running to completion and the tail-jump fall-through into the shared
 * fd2_chapter_14_end epilogue snippet @ 0x239AC — i.e. that the table-copy
 * loops do not fault and that the handler ends by saving the runtime char
 * templates and advancing current_chapter_id by exactly one.
 *
 * The handler is driven end-to-end on-host with the proven chend2 safe env:
 *   - fd2_setup_chars_and_camera_for_intro is a no-op double (its real char
 *     placement/camera/fade is display-only; deferred to Phase 9), so the
 *     two fd2_pan_cursor_and_window calls see the window origin we pre-seed
 *     here (0x10,0x10 then target 0x10,0xE) and the X-pan + first Y-pan are
 *     no-ops, leaving the second pan a deterministic 2-step scroll;
 *   - fd2_cast_screen_wide_spell_with_fade is a no-op double (150KB malloc +
 *     ~95-tick shockwave/palette-flash blocking display; deferred to Phase 9);
 *   - the three fd2_display_dialog_scene calls take the immediate-END program,
 *     the two fd2_cutscene_event_trigger calls take zero-group scripts, and
 *     the real fd2_play_palette_fade_to_black + the two direct
 *     memset(0xA0000,…) screen clears run against the staged palette/VGA.
 *
 * The full display path (real radial spell + camera placement + screen
 * fades) is pure blit/display orchestration deferred to Phase 9 integration.
 * ---------------------------------------------------------------- */
static void test_ch22_end_runs_and_advances(void)
{
    uint32 chap0;

    ce_install_safe_env();

    /* setup is stubbed (does not set the window origin), so pre-seed it to
     * the first pan target; the second pan then scrolls y 0x10 -> 0xE. */
    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x10;
    data_fd2_battle_cursor_world_x = 0x10;
    data_fd2_battle_cursor_world_y = 0x10;

    /* zero-group cutscene scripts for the two events the handler fires. */
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x41] = g_ce22_script_41;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x42] = g_ce22_script_42;

    chap0 = data_fd2_chapter_current_chapter_id;

    fd2_chapter_22_end();

    /* tail-jump fall-through ran: chapter id advanced by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    /* the camera pan reached its scripted Y target (0xE). */
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 0xE);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Chapter 23 end handler — fd2_chapter_23_end @ 0x24754.
 *
 * The risk core is the Phase-1 three-way story-branch logic; the branch
 * predicates are the real fd2_any_char_has_item (driven via the find double's
 * g_ce_find_have_item100), the real fd2_find_template_char_by_id (driven via
 * the template roster: ce23_seed_miti_in_roster places 蜜蒂/0x12 for the
 * present arm, otherwise the roster has no 0x12 for the absent arm), and the
 * turn counter, and each branch's observable mutation is a recruit
 * (data_fd2_shared_menu_party_member_count via the real
 * fd2_init_runtime_char_from_base_growth) and/or a death-mark (the real
 * fd2_mark_char_as_dead writes runtime_char[0x11].flags = CHARFLAG_DEAD).
 *
 * Each case drives the whole handler end-to-end on-host with the proven chend2
 * safe env. Phase 2 then runs FOR REAL: the staged FDFIELD.DAT[0x45] /
 * FDSHAP.DAT[0x2E,0x2F] resource loads and the real
 * fd2_load_chapter_background_layers (FDOTHER.DAT[0xF] at chapter id 0x18) all
 * execute against the real game files build_test.py stages into the test cwd,
 * so the resource indices are validated as in-bounds. current_chapter_id is
 * seeded to 0x17 (the real chapter-23 value) so the background-layer reload
 * follows the exact in-game path; the only stubbed Phase-2 callees are the
 * display-only fd2_animate_screen_shake / fd2_play_rising_pre_cast_effect /
 * fd2_obfuscate_battle_tile_map. The pure blit/display side effects of Phase 2
 * (the actual frame rendered) are deferred to Phase 9 integration.
 * ---------------------------------------------------------------- */
static void ce23_setup(void)
{
    int i;

    ce_install_safe_env();

    /* immediate-END dialog program covering page 0x11 (the shared env stops at
     * page 0x10): offsets 0..0x11 all point at a single -1 END opcode. */
    for (i = 0; i <= 0x11; i++) {
        g_ce23_dlg[i] = (int16)(0x12 * 2);   /* byte offset of the END opcode */
    }
    g_ce23_dlg[0x12] = -1;                    /* END */
    current_chapter_text = (uint32)g_ce23_dlg;

    /* zero-group cutscene scripts for the events the handler can fire. */
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x47] = g_ce23_script_47;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x48] = g_ce23_script_48;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x49] = g_ce23_script_49;

    /* real chapter-23 value: the handler's +1 lands on 0x18, the in-game
     * single-sprite background-layer path (FDOTHER.DAT[0xF]). */
    data_fd2_chapter_current_chapter_id = 0x17;

    /* 蜜蒂 (slot 0x11) starts alive so a death-mark is observable as a change. */
    g_ce_rc[0x11].flags = 0;

    /* default: 天空之鑰 (item 100) not held; each test sets this explicitly to
     * drive the real fd2_any_char_has_item via the find double. */
    g_ce_find_have_item100 = 0;
}

/* ----------------------------------------------------------------
 * Branch combo 1 — 天空之鑰 held + 蜜蒂 present:
 *   join arm  -> recruit 卡里斯 (char 0x16);
 *   蜜蒂 arm  -> mark 蜜蒂 (slot 0x11) dead (no recruit).
 * The roster is pre-seeded with 蜜蒂 (count 1) so the real predicate's present
 * arm runs; the 卡里斯 recruit then appends, leaving count 2. Slot 0x11 dead;
 * chapter id advances 0x17 -> 0x18.
 * ---------------------------------------------------------------- */
static void test_ch23_end_key_held_miti_present(void)
{
    ce23_setup();
    g_ce_find_have_item100 = 1;  /* 天空之鑰 held -> recruit 卡里斯 (0x16) */
    ce23_seed_miti_in_roster();  /* 蜜蒂 present -> mark 蜜蒂 dead */

    fd2_chapter_23_end();

    /* pre-seeded 蜜蒂 + one recruit (卡里斯) = 2; 蜜蒂 marked dead; chapter +1. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 2);
    ASSERT_EQ(g_ce_rc[0x11].flags, CHARFLAG_DEAD);
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, 0x18);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Branch combo 2 — 天空之鑰 NOT held + 蜜蒂 absent + within 15 turns:
 *   join arm  -> cutscene 0x47, NO recruit;
 *   蜜蒂 arm  -> turn_counter < 15 -> recruit 羅德曼 (char 0x13).
 * Exactly one recruit (from the turn arm only); slot 0x11 stays alive;
 * chapter id advances 0x17 -> 0x18.
 * ---------------------------------------------------------------- */
static void test_ch23_end_no_key_miti_absent_within_15_turns(void)
{
    ce23_setup();
    g_ce_find_have_item100 = 0;  /* 天空之鑰 not held -> cutscene 0x47 only */
    /* 蜜蒂 absent: roster stays free of 0x12 (no recruit on the no-key arm). */
    data_fd2_battle_turn_counter = 14;   /* < 15 -> recruit 羅德曼 (0x13) */

    fd2_chapter_23_end();

    /* exactly one recruit (羅德曼); 蜜蒂 slot left alive; chapter advanced. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 1);
    ASSERT_EQ(g_ce_rc[0x11].flags, 0);
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, 0x18);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Branch combo 3 — 天空之鑰 held + 蜜蒂 absent + NOT within 15 turns:
 *   join arm  -> recruit 卡里斯 (char 0x16);
 *   蜜蒂 arm  -> turn_counter >= 15 -> cutscene 0x48 + mark 蜜蒂 dead.
 * Exactly one recruit (卡里斯); slot 0x11 dead; chapter id advances.
 * ---------------------------------------------------------------- */
static void test_ch23_end_key_held_miti_absent_after_15_turns(void)
{
    ce23_setup();
    g_ce_find_have_item100 = 1;  /* 天空之鑰 held -> recruit 卡里斯 (0x16) */
    /* 蜜蒂 absent: the 卡里斯 recruit writes 0x16 (not 0x12) into the roster. */
    data_fd2_battle_turn_counter = 15;   /* >= 15 -> mark 蜜蒂 dead, no recruit */

    fd2_chapter_23_end();

    /* one recruit (卡里斯), 蜜蒂 marked dead, chapter advanced by one. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 1);
    ASSERT_EQ(g_ce_rc[0x11].flags, CHARFLAG_DEAD);
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, 0x18);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Chapter 24 end handler — fd2_chapter_24_end @ 0x24C1E.
 *
 * fd2_chapter_24_end is a straight-line, no-branch text-scroll cinematic (no
 * RNG, no numeric computation, no CALL-return value used). Its only meaningful
 * non-display game-state mutation is the epilogue, and the one control-flow
 * property worth pinning is that the two text-scroll phases run to completion
 * with the line counter carried from Phase 1 (lines 2..9) straight into Phase 2
 * (lines 10..14) and the brightness counter advancing 0..59 across all five
 * Phase-2 lines. The handler is driven end-to-end on-host with the proven
 * chend2 safe env:
 *   - the two fd2_display_dialog_scene calls take the immediate-END program so
 *     each returns at once with no glyph blits;
 *   - fd2_scroll_text_screen_up_by_lines is called with line in 2..14 (all
 *     non-zero), so it takes its Mode-A path (store the pending line count and
 *     return) — no malloc/memmove, no static_bg_buffer needed;
 *   - the real fd2_composite_battle_frame runs against the staged compositor
 *     workspace (HUD gated off, anim_phase=0 -> cursor overlay no-op, palette
 *     cycle throttled), and the real fd2_set_vga_palette_range reads the staged
 *     768-byte palette in-bounds (idx 0..255 -> palette[0..767]);
 *   - the empty party makes the tail fd2_save_runtime_char_to_template iterate
 *     zero chars, and the final memset blacks the (harmless) VGA framebuffer.
 *
 * fd2_wait_n_bios_ticks(1) is driven for real (it busy-waits on the live BIOS
 * tick at 0x46C, which advances ~18.2/s under DOSBox-X), so the full run spins
 * for the 300 frame waits; one end-to-end invocation covers the whole handler.
 * The pixel output of the composite/scroll/palette-fade stages is pure display
 * and is deferred to Phase 9 integration.
 * ---------------------------------------------------------------- */
static void test_ch24_end_runs_and_advances(void)
{
    uint32 chap0;

    ce_install_safe_env();
    chap0 = data_fd2_chapter_current_chapter_id;

    fd2_chapter_24_end();

    /* both text-scroll phases ran to completion and the epilogue advanced the
     * chapter id by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    /* anim_phase was reset to 0 at the start of each phase (and never set
     * back) — it ends at 0. */
    ASSERT_EQ(data_fd2_battle_anim_phase, 0);

    /* the handler adds no char: the save-template tail ran against the empty
     * party, leaving the menu roster count untouched. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 0);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Chapter 25 end handler — fd2_chapter_25_end @ 0x24DF2.
 *
 * fd2_chapter_25_end is a straight-line, no-branch dialog/cutscene handler (no
 * RNG, no numeric computation, no CALL-return value used). Its testable risk
 * core is (a) the unconditional real portrait reload + FD2.TMP swap-file
 * rewrite via fd2_load_chapter_portraits_and_dump_tmp(2); (b) the two-recruit
 * sequence with a save in between — 聖寇拉斯 (char 0x1A) is recruited and
 * saved, then 亞奇梅吉 (char 0x1D) is recruited AFTER the save through the
 * shared fd2_chapter_11_end tail snippet @ 0x237C8 (entered via PUSH 0x1D ;
 * JMP); and (c) that the shared tail's fall-through advances current_chapter_id
 * by exactly one.
 *
 * The handler is driven end-to-end on-host with the proven chend2 safe env
 * plus the rsrc portrait fixture:
 *   - fd2_load_chapter_portraits_and_dump_tmp runs FOR REAL against the staged
 *     real FDICON.B24 + FDFIELD.DAT (copied into the test cwd by build_test.py);
 *     alloc_offset is set to 0 so the per-record race scan iterates zero
 *     entries (no fd2_init_runtime_char_for_battle calls), and the real
 *     function re-reads FDFIELD[chapter*3+2], frees+nulls the field buffer, and
 *     rewrites the 0x32A00-byte FD2.TMP swap file;
 *   - current_chapter_id is seeded to 4 so the FDFIELD re-read index (4*3+2 =
 *     0xE) is the same valid index the rsrc loader suite exercises;
 *   - the two fd2_display_dialog_scene calls take the immediate-END program,
 *     the fd2_cutscene_event_trigger(0x4B) call takes a zero-group script, and
 *     fd2_pan_cursor_and_window runs against the staged camera state;
 *   - both fd2_init_runtime_char_from_base_growth recruits run FOR REAL,
 *     appending into the staged 64-slot menu roster, and the empty active party
 *     makes fd2_save_runtime_char_to_template iterate zero chars.
 *
 * The pure blit/display side effects (dialog glyphs, cutscene compositing,
 * camera pan pixels) are deferred to Phase 9 integration.
 * ---------------------------------------------------------------- */

/* zero-group cutscene script for ch25's event 0x4B: n_groups byte = 0, so the
 * real fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ce25_script_4b[1] = { 0 };

static long ce25_fd2_tmp_size(void)
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

static void test_ch25_end_real_portrait_reload_two_recruits_and_advance(void)
{
    uint32 chap0;

    ce_install_safe_env();

    /* zero-group cutscene script for the single event the handler fires. */
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x4B] = g_ce25_script_4b;

    /* portrait reload runs for real: empty tile-event scan (alloc_offset 0 ->
     * no per-record fd2_init_runtime_char_for_battle), fresh field buffer, and
     * a valid FDFIELD re-read index (chapter 4 -> 4*3+2 = 0xE). */
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    chapter_portrait_load_buffer = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_chapter_current_chapter_id = 4;
    chap0 = data_fd2_chapter_current_chapter_id;

    remove("FD2.TMP");

    fd2_chapter_25_end();

    /* shared-tail fall-through ran: chapter id advanced by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    /* both recruits ran (聖寇拉斯 0x1A before the save, 亞奇梅吉 0x1D after it). */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 2);

    /* the real portrait reload ran: field buffer freed+nulled, and the FD2.TMP
     * swap file was rewritten to its full 0x32A00-byte size. */
    ASSERT_EQ(chapter_portrait_load_buffer, 0);
    ASSERT_EQ(ce25_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Chapter 26 end handler — fd2_chapter_26_end @ 0x24E80.
 *
 * Two deterministic, non-display risk cores:
 *   (a) the 機器人渥德 force-positioning loop: every runtime_char in slots
 *       0x10..party_member_count whose portrait_id is 0x1F is moved to world
 *       tile (0x10, 6); slots below 0x10 and chars with a different portrait
 *       id are left untouched. The loop's start index (0x10) and the
 *       portrait-id guard are exercised by three planted chars below.
 *   (b) the dynamic dialog-page reads of tile_event_consumed_flags[0xC]: the
 *       first dialog uses page (flag + 5) and the third page (flag + 8). With
 *       the flag byte at 4 those land on pages 9 and 12, the largest pages the
 *       handler can request; pointing the flags buffer at a real 16-byte block
 *       exercises the [ptr+0xC] read in-bounds, and the immediate-END dialog
 *       program (page-offset table covering indices 0..0x10) resolves page 12
 *       to an instant END.
 *
 * The handler is driven end-to-end on-host with the proven chend2 safe env:
 *   - fd2_setup_chars_and_camera_for_intro is a no-op double (its real char
 *     placement/camera/fade is display-only; deferred to Phase 9), so the
 *     force-positioning loop above is the only thing that mutates the planted
 *     runtime_char slots;
 *   - the five fd2_display_dialog_scene calls take the immediate-END program,
 *     and the four fd2_cutscene_event_trigger calls take zero-group scripts
 *     (events 0x4D..0x50), so each returns at once with no glyph/cutscene
 *     blits;
 *   - the empty menu roster (member_count 0) makes the tail
 *     fd2_save_runtime_char_to_template iterate zero template entries, so the
 *     non-zero active party_member_count used to bound the placement loop is
 *     harmless there.
 *
 * No char is added by the handler — 機器人渥德 joins via an FDFIELD event;
 * this handler only positions it. The pure blit/display side effects (dialog
 * glyphs, cutscene compositing, camera placement) are deferred to Phase 9
 * integration.
 * ---------------------------------------------------------------- */

/* zero-group cutscene scripts for ch26's events 0x4D..0x50: n_groups byte = 0,
 * so the real fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ce26_script_4d[1] = { 0 };
static uint8 g_ce26_script_4e[1] = { 0 };
static uint8 g_ce26_script_4f[1] = { 0 };
static uint8 g_ce26_script_50[1] = { 0 };

/* real 16-byte tile-event flags block so the handler's [ptr+0xC] page reads
 * stay in-bounds; index 0xC carries the chosen-treasure-box value (0..4). */
static uint8 g_ce26_tile_flags[16];

static void test_ch26_end_positions_robot_and_advances(void)
{
    uint32 chap0;

    ce_install_safe_env();

    /* zero-group cutscene scripts for the four events the handler fires. */
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x4D] = g_ce26_script_4d;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x4E] = g_ce26_script_4e;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x4F] = g_ce26_script_4f;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x50] = g_ce26_script_50;

    /* real tile-event flags buffer; [0xC] = 4 -> the dynamic dialogs request
     * the largest pages (flag+5 = 9, flag+8 = 12), both covered by the
     * immediate-END program (indices 0..0x10). */
    memset(g_ce26_tile_flags, 0, sizeof(g_ce26_tile_flags));
    g_ce26_tile_flags[0xC] = 4;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce26_tile_flags;

    /* scan slots 0x10..0x13 (party_member_count 0x14). */
    data_fd2_battle_party_member_count = 0x14;

    /* slot 0x12: 機器人渥德 (portrait 0x1F) at a non-target position -> moved. */
    g_ce_rc[0x12].portrait_id = 0x1F;
    g_ce_rc[0x12].pos_x = 0x55;
    g_ce_rc[0x12].pos_y = 0x55;

    /* slot 0x13: a different portrait in range -> left untouched. */
    g_ce_rc[0x13].portrait_id = 0x20;
    g_ce_rc[0x13].pos_x = 0x33;
    g_ce_rc[0x13].pos_y = 0x33;

    /* slot 0x05: portrait 0x1F but below the loop's 0x10 start -> untouched. */
    g_ce_rc[0x05].portrait_id = 0x1F;
    g_ce_rc[0x05].pos_x = 0x44;
    g_ce_rc[0x05].pos_y = 0x44;

    chap0 = data_fd2_chapter_current_chapter_id;

    fd2_chapter_26_end();

    /* the in-range 0x1F char was force-positioned to (0x10, 6). */
    ASSERT_EQ(g_ce_rc[0x12].pos_x, 0x10);
    ASSERT_EQ(g_ce_rc[0x12].pos_y, 6);

    /* the in-range non-0x1F char kept its position. */
    ASSERT_EQ(g_ce_rc[0x13].pos_x, 0x33);
    ASSERT_EQ(g_ce_rc[0x13].pos_y, 0x33);

    /* the out-of-range 0x1F char (slot 5 < 0x10) was not touched. */
    ASSERT_EQ(g_ce_rc[0x05].pos_x, 0x44);
    ASSERT_EQ(g_ce_rc[0x05].pos_y, 0x44);

    /* epilogue ran: chapter id advanced by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    /* no char added by the handler. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 0);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Chapter 27 end handler — fd2_chapter_27_end @ 0x250CC. The FD2 GOOD/BAD
 * ending fork.
 *
 * The risk core is the GOOD/BAD fork on whether any party char holds 天空之鑰
 * (item 100), plus the common prologue's blanket reset of every active
 * runtime_char's flags byte (slots 0..15). The fork predicate is the REAL
 * fd2_any_char_has_item(100), driven through the find double's
 * g_ce_find_have_item100 (char 0 holds item 100), exactly as the ch23 suite
 * drives it.
 *
 * Only the GOOD path is driven end-to-end on-host: it advances
 * current_chapter_id, restores the (empty) template roster to full HP/MP, and
 * RETURNS (its tail shares fd2_render_party_status_overview_content's epilogue
 * @ 0x1B5EA) so play continues into chapter 28. The BAD path is intentionally
 * NOT a returnable code path — after the game-over cinematic it hard-locks in
 * an infinite loop (the "沒天空之鑰悠妮獨自回黃金城無法玩" game-over), so it
 * cannot be invoked from a host unit test; its warp/teleport + game-over
 * cinematic side effects are deferred to Phase 9 integration.
 *
 * The GOOD path is driven with the proven chend2 safe env. All its callees are
 * host-safe there: fd2_setup_chars_and_camera_for_intro is the no-op double;
 * the five fd2_display_dialog_scene calls (pages 8..12, all <= 0x10) take the
 * immediate-END program; the three fd2_cutscene_event_trigger calls
 * (events 0x52/0x53/0x54) take zero-group scripts; fd2_pan_cursor_and_window,
 * the five fd2_palette_overbright_settle_step_loop pulses, and the real
 * fd2_play_palette_fade_to_black all run against the staged camera/768-byte
 * palette; fd2_cast_screen_wide_spell_with_fade is the no-op double; the two
 * direct memset(0xA0000,…) clears hit the harmless VGA framebuffer; the empty
 * active party makes fd2_save_runtime_char_to_template iterate zero chars; and
 * the empty template roster makes fd2_restore_all_chars_full_hp_mp iterate
 * zero chars. The pure blit/display side effects of the GOOD-path cinematic
 * are deferred to Phase 9 integration.
 * ---------------------------------------------------------------- */

/* zero-group cutscene scripts for ch27's events 0x52 / 0x53 / 0x54: n_groups
 * byte = 0, so the real fd2_cutscene_event_trigger just composites once and
 * returns. */
static uint8 g_ce27_script_52[1] = { 0 };
static uint8 g_ce27_script_53[1] = { 0 };
static uint8 g_ce27_script_54[1] = { 0 };

static void test_ch27_end_good_path_resets_flags_and_advances(void)
{
    int i;
    uint32 chap0;

    ce_install_safe_env();

    /* zero-group cutscene scripts for the three events the GOOD path fires. */
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x52] = g_ce27_script_52;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x53] = g_ce27_script_53;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x54] = g_ce27_script_54;

    /* GOOD path: 天空之鑰 (item 100) held -> fd2_any_char_has_item != -1. */
    g_ce_find_have_item100 = 1;

    /* dirty every active runtime_char's flags byte so the prologue's blanket
     * reset of slots 0..15 is observable; slot 0x10 (just past the reset
     * range) is dirtied too and must survive untouched. */
    for (i = 0; i < 16; i++) {
        g_ce_rc[i].flags = 0xFF;
    }
    g_ce_rc[0x10].flags = 0xAA;

    chap0 = data_fd2_chapter_current_chapter_id;

    fd2_chapter_27_end();

    /* prologue reset every active slot's flags byte to 0. */
    for (i = 0; i < 16; i++) {
        ASSERT_EQ(g_ce_rc[i].flags, 0);
    }
    /* slot 0x10 is outside the 16-slot reset range -> left untouched. */
    ASSERT_EQ(g_ce_rc[0x10].flags, 0xAA);

    /* GOOD path ran to its return: chapter id advanced by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    /* GOOD path adds no char (it persists the existing roster, not a recruit). */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 0);

    g_ce_find_have_item100 = 0;
    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Chapter 28 end handler — fd2_chapter_28_end @ 0x25464. The simplest end
 * handler: a single dialog scene (page 7) then the shared fd2_chapter_04_end
 * epilogue tail (save runtime char templates + advance current_chapter_id).
 *
 * No RNG, no numeric computation, no CALL-return value used, no char added.
 * The testable risk core is that the tail-jump fall-through into
 * fd2_chapter_04_end's epilogue runs to completion — the handler ends by
 * saving the templates and advancing current_chapter_id by exactly one. It is
 * driven end-to-end on-host with the proven chend2 safe env: the single
 * fd2_display_dialog_scene call (page 7, <= 0x10) takes the immediate-END
 * program so it returns at once with no glyph blits, and the empty active
 * party makes fd2_save_runtime_char_to_template iterate zero chars. The pure
 * blit/display side effects of the dialog scene are deferred to Phase 9
 * integration.
 * ---------------------------------------------------------------- */
static void test_ch28_end_runs_and_advances(void)
{
    uint32 chap0;

    ce_install_safe_env();
    chap0 = data_fd2_chapter_current_chapter_id;

    fd2_chapter_28_end();

    /* epilogue tail-jump ran: chapter id advanced by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    /* the handler adds no char: the save-template tail ran against the empty
     * party, leaving the menu roster count untouched. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 0);

    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Chapter 29 end handler — fd2_chapter_29_end @ 0x2548C. A straight-line
 * dramatic ending cutscene (no branch, no char added).
 *
 * The testable risk core is the handler's own deterministic state mutations,
 * which do not depend on the display-only callees:
 *   - it wipes the trailing party slots via
 *     fd2_kill_runtime_chars_from_index_to_end(0x14) — verified through the
 *     recorder double (start index 0x14, called exactly once);
 *   - it transmutes runtime_char[0x14] into its 變身 form by overwriting BOTH
 *     portrait_id (+0x07) and char_id (+0x08) with 0x7E;
 *   - it resets battle_anim_phase to 0 (last set before dialog page 15, with
 *     the palette loops not touching it, so it ends at 0);
 *   - it advances current_chapter_id by exactly one;
 *   - the two palette fade loops run to completion: a 64-step fade-out
 *     (v=0..0x3F) and a 63-step fade-in (v=0x3E..0), each step doing one
 *     __delay_thunk_375b2(4). The total __delay_thunk_375b2 call count
 *     (2+2+6 earthquake/flash holds + 64 fade-out + 1 black hold + 63 fade-in
 *     = 138) and a final ticks value of 4 confirm both signed-comparison
 *     loops iterated the right number of times in the right direction.
 *
 * It is driven end-to-end on-host with the proven chend2 safe env plus the
 * ch25-style real portrait reload: the six fd2_display_dialog_scene calls
 * (pages 10..15, all <= 0x10) take the immediate-END program; the real
 * fd2_load_chapter_portraits_and_dump_tmp(9) reads the staged FDICON.B24 +
 * FDFIELD.DAT with alloc_offset 0 (so the per-record race scan iterates zero
 * entries) and rewrites the 0x32A00-byte FD2.TMP swap file;
 * fd2_pan_cursor_and_window / fd2_pan_cursor_to_tile_animated run against the
 * staged camera; the warp/screen-shake/palette-flash callees are host-safe
 * doubles; the real fd2_set_vga_palette_range_with_add fade steps run against
 * the staged 768-byte palette; the direct memset(0xA0000,…) clear hits the
 * harmless VGA framebuffer; and the empty active party makes
 * fd2_save_runtime_char_to_template iterate zero chars. The pure blit/display
 * side effects (dialog glyphs, screen shake, palette pulses, warp animation)
 * are deferred to Phase 9 integration.
 * ---------------------------------------------------------------- */
static void test_ch29_end_transmutes_slot14_and_advances(void)
{
    uint32 chap0;

    ce_install_safe_env();

    /* ch25-style real portrait reload: empty tile-event scan (alloc_offset 0
     * -> no per-record fd2_init_runtime_char_for_battle), fresh field buffer,
     * and a valid FDFIELD re-read index (chapter 4 -> 4*3+2 = 0xE, the same
     * index the rsrc loader suite exercises). */
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    chapter_portrait_load_buffer = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_chapter_current_chapter_id = 4;
    chap0 = data_fd2_chapter_current_chapter_id;

    /* seed slot 0x14 with non-target ids so the 0x7E transmute is observable. */
    g_ce_rc[0x14].portrait_id = 0x11;
    g_ce_rc[0x14].char_id = 0x22;

    g_ce_kill_from_calls = 0;
    g_ce_kill_from_last_idx = 0;
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;

    remove("FD2.TMP");

    fd2_chapter_29_end();

    /* trailing-slot kill issued exactly once with start index 0x14. */
    ASSERT_EQ(g_ce_kill_from_calls, 1);
    ASSERT_EQ(g_ce_kill_from_last_idx, 0x14);

    /* slot 0x14 transmuted: BOTH portrait_id and char_id overwritten to 0x7E. */
    ASSERT_EQ(g_ce_rc[0x14].portrait_id, 0x7E);
    ASSERT_EQ(g_ce_rc[0x14].char_id, 0x7E);

    /* anim_phase left at 0 (last reset before page 15). */
    ASSERT_EQ(data_fd2_battle_anim_phase, 0);

    /* both palette fade loops ran to completion: 2+2+6 hold delays + 64
     * fade-out steps + 1 black hold + 63 fade-in steps = 138 delay calls, and
     * the final delay was a fade-in 4ms step. */
    ASSERT_EQ(g_delay375b2_calls, 138);
    ASSERT_EQ(g_delay375b2_last_ticks, 4);

    /* chapter id advanced by exactly one. */
    ASSERT_EQ(data_fd2_chapter_current_chapter_id, chap0 + 1);

    /* no char added by the handler. */
    ASSERT_EQ(data_fd2_shared_menu_party_member_count, 0);

    /* the real portrait reload ran: field buffer freed+nulled, and the FD2.TMP
     * swap file was rewritten to its full 0x32A00-byte size. */
    ASSERT_EQ(chapter_portrait_load_buffer, 0);
    ASSERT_EQ(ce25_fd2_tmp_size(), 0x32A00);

    /* leave the FD2.TMP swap file out of the shared cwd for later suites. */
    remove("FD2.TMP");
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    ce_restore_rc_ptr();
}

/* ----------------------------------------------------------------
 * Chapter 30 end handler — fd2_chapter_30_end @ 0x25757. The FD2 GOOD
 * ENDING. The handler is a straight-line cutscene that terminates in an
 * intentional infinite loop (CALL fd2_play_game_ending_cinematic; JMP self),
 * so it can never return and cannot be driven end-to-end on the host.
 *
 * Its only genuinely new, deterministic, non-display artifact is the trio of
 * 20-byte end-scene tables it materializes and copies onto the stack. The
 * FD2.LE prologue copies each as five dwords (MOVSD x5) and hands their
 * addresses to fd2_setup_chars_and_camera_for_intro, which indexes them
 * BYTE-wise by char slot (0..0x13). The 60 transcribed bytes are the testable
 * risk core, so this test asserts each const table byte-for-byte against the
 * FD2.LE ground truth (pos_x @ 0x52327, pos_y @ 0x5233B, facing @ 0x5234F),
 * including the facing table's single anomaly (slot 1 = 0x00, the rest 0x02).
 *
 * The placement itself is performed by fd2_setup_chars_and_camera_for_intro
 * (0x233C6, not yet emitted — a no-op double in tests/testglob.c), so the
 * char-slot writes are not observable on-host yet; that function's own emit +
 * test owns its placement behavior. Likewise everything after it in the real
 * handler (dialog pages 9/10/0/1, cutscene events 0x58/0x59, cursor pans, the
 * screen-wide death spell, the HP/MP restores, the chapter_id advance to 31,
 * the chapter-31 map load, the palette fade-in + composite-hold loops, and the
 * staff-roll cinematic) is display/orchestration side-effect tail the handler
 * runs straight into its infinite loop; its behavioral coverage is deferred to
 * Phase 9 integration. This test touches no shared global, so it needs no
 * safe-env fixture or teardown.
 * ---------------------------------------------------------------- */
static void test_ch30_end_scene_tables_match_ground_truth(void)
{
    /* FD2.LE ground truth: pos_x @ 0x52327, pos_y @ 0x5233B, facing @ 0x5234F
     * (slots 0..0x13, one byte each). */
    static const uint8 want_x[20] = {
        0x16, 0x16, 0x14, 0x15, 0x17, 0x18, 0x14, 0x15, 0x17, 0x18,
        0x14, 0x15, 0x16, 0x17, 0x18, 0x14, 0x15, 0x16, 0x17, 0x18
    };
    static const uint8 want_y[20] = {
        0x17, 0x13, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
        0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19, 0x19
    };
    static const uint8 want_f[20] = {
        0x02, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
    };
    int i;

    for (i = 0; i < 20; i++) {
        ASSERT_EQ(data_fd2_chapter_ch30_end_scene_char_pos_x_table[i], want_x[i]);
        ASSERT_EQ(data_fd2_chapter_ch30_end_scene_char_pos_y_table[i], want_y[i]);
        ASSERT_EQ(data_fd2_chapter_ch30_end_scene_char_facing_table[i], want_f[i]);
    }

    /* the facing table's lone non-0x02 entry is slot 1 (= 0x00). */
    ASSERT_EQ(data_fd2_chapter_ch30_end_scene_char_facing_table[1], 0x00);
}

void run_field_chend2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chend2\n");
    RUN_TEST(test_ch20_end_places_chars_and_advances);
    RUN_TEST(test_ch20_end_reaims_camera);
    RUN_TEST(test_ch21_end_partial_collection_page6_path);
    RUN_TEST(test_ch21_end_full_collection_awards_key);
    RUN_TEST(test_ch22_end_runs_and_advances);
    RUN_TEST(test_ch23_end_key_held_miti_present);
    RUN_TEST(test_ch23_end_no_key_miti_absent_within_15_turns);
    RUN_TEST(test_ch23_end_key_held_miti_absent_after_15_turns);
    RUN_TEST(test_ch24_end_runs_and_advances);
    RUN_TEST(test_ch25_end_real_portrait_reload_two_recruits_and_advance);
    RUN_TEST(test_ch26_end_positions_robot_and_advances);
    RUN_TEST(test_ch27_end_good_path_resets_flags_and_advances);
    RUN_TEST(test_ch28_end_runs_and_advances);
    RUN_TEST(test_ch29_end_transmutes_slot14_and_advances);
    RUN_TEST(test_ch30_end_scene_tables_match_ground_truth);
    printf("\n");
}
