/*
 * unit tests for src/field/chend1.c
 *
 * fd2_chapter_01_end is the Chapter 1 end handler: a 3-step orchestrator that
 *   (1) shows chapter-end dialog page 9 via the real fd2_display_dialog_scene,
 *   (2) persists battle-runtime char state via the real
 *       fd2_save_runtime_char_to_template, and
 *   (3) sets current_chapter_id = 1.
 *
 * Both callees are the real linked functions, so this test stands up the same
 * safe in-memory fixtures their own suites use:
 *   - dialog VM: current_chapter_text points at a minimal int16 program whose
 *     page-9 header word redirects to a single glyph + END, so the real VM runs
 *     to completion using the testglob.c glyph/blink recording stubs (no VGA),
 *     with the BIOS keyboard buffer empty and the portrait latch cleared.
 *   - save-template: a zeroed runtime-char array + zeroed roster template with
 *     the roster pointer set and member_count = 1 (the save suite's baseline),
 *     so the real persistence pass and its fd2_recompute_runtime_char_total_stats
 *     callee run harmlessly.
 *
 * Asserted: the dialog VM actually ran against page 9 of current_chapter_text
 * (glyph recorder), and the chapter-id state transition committed to 1. The
 * pixel output of the dialog page is pure display and is deferred to Phase 9.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];
extern int    g_check_char_is_dead_return;

/* dialog-VM glyph recorder (testglob.c). */
extern int    g_dlg_glyph_calls;
extern uint32 g_dlg_glyph_last_idx;

/* delay thunk recorder (testglob.c). */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;

/* roster template the real save-template pass copies into. */
static uint8 g_ce1_tmpl[8 * 0x50];

/* Minimal dialog program: page 9's header word (prog[9]) is a byte offset that
 * redirects cur_op to prog[10] = one glyph (0x41), prog[11] = -1 END. The VM
 * blits the single glyph then returns. */
static int16 g_ce1_text[16];

static void ce1_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    for (i = 0; i < 16; i++) {
        g_ce1_text[i] = 0;
    }
    g_ce1_text[9] = 20;       /* byte offset to prog[10] (page 9 start) */
    g_ce1_text[10] = 0x41;    /* one glyph */
    g_ce1_text[11] = -1;      /* END */
    current_chapter_text = (uint32)g_ce1_text;

    /* save-template safe env (save suite baseline). */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_ce1_tmpl, 0, sizeof(g_ce1_tmpl));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce1_tmpl;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;

    /* preset chapter id to a non-target value so the transition is observable. */
    data_fd2_chapter_current_chapter_id = 0;
}

/* ----------------------------------------------------------------
 * End-to-end: the handler runs the dialog page then advances chapter id to 1.
 * The glyph recorder proves the real dialog VM was invoked on page 9 of
 * current_chapter_text (guards against a wrong text base / page index), and the
 * chapter-id write is the handler's state-transition contract.
 * ---------------------------------------------------------------- */
static void test_chapter_01_end_runs_dialog_and_advances_id(void)
{
    ce1_fixture_reset();

    fd2_chapter_01_end();

    /* page 9 redirected to a single glyph: the real VM blitted exactly it. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x41);

    /* state transition: next chapter id committed. */
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 1L);
}

/* ----------------------------------------------------------------
 * The chapter-id write is unconditional: even if it already held a stale value,
 * the handler overwrites it with 1.
 * ---------------------------------------------------------------- */
static void test_chapter_01_end_overwrites_stale_id(void)
{
    ce1_fixture_reset();
    data_fd2_chapter_current_chapter_id = 7;   /* stale */

    fd2_chapter_01_end();

    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 1L);
}

/* ================================================================
 * fd2_chapter_02_end @ 0x22F37
 *
 * The Chapter 2 end handler's one branch is the 6-villager survival check
 * (runtime chars[5..0xA], flags bit0): all-alive shows dialog page 6 and
 * gifts 力量藥水 (item 0xC6) to the first player char, any-dead shows the
 * failure page 7 and gifts nothing. Everything after that is a fixed cutscene
 * tail (pan/portrait-load/3 cutscene events with pages 8/9/10 interleaved),
 * a recruit of char #8, a save-to-template pass, and chapter_id := 2.
 *
 * This drives the WHOLE real handler headless: every callee is the real linked
 * function. The fixtures mirror each callee's own suite so the orchestration
 * runs without VGA/input deps:
 *   - dialog VM (real): a minimal int16 program where the branch pages 6 and 7
 *     each redirect to one distinct glyph (0x66 / 0x77) + END, while pages 8/9/
 *     10 redirect to an immediate END (no glyph). With the BIOS keyboard buffer
 *     empty and the portrait latch cleared, the real VM renders exactly the one
 *     branch page; the glyph recorder then pins WHICH page (6 vs 7) ran via the
 *     single recorded glyph id, since no later page emits a glyph.
 *   - portrait dump (real fd2_load_chapter_portraits_and_dump_tmp): the staged
 *     real FDFIELD.DAT re-read + FD2.TMP (0x32A00) rewrite run for real; the
 *     race-scan length (alloc_offset) is 0 so no FDICON parse / per-char init
 *     fires, and portrait_sprite_cache points at a 0x32A00 scratch buffer.
 *   - cutscene events 0xE/0xF/0x10 (real fd2_cutscene_event_trigger): their
 *     script-table slots point at an n_groups==0 script, so each is a no-op
 *     plus the trailing composite.
 *   - pan/compose (real): bounded view-window pans over the tile-map compositor
 *     (recording stub); the runtime-char array is a local 16-slot fixture so the
 *     villager indices 5..0xA and the recruit's char-#8 roster append are valid.
 *   - recruit (real fd2_init_runtime_char_from_base_growth) + save (real
 *     fd2_save_runtime_char_to_template): append char #8 to the roster (zeroed
 *     base/growth tables -> harmless stats) and persist runtime char 0.
 *   - item gift (real fd2_give_item_to_first_player_char): runtime char 0 is a
 *     team==2 player with an empty backpack, so the gift lands in slot 0 and is
 *     directly observable.
 *
 * Asserted per branch: the dialog page that ran (glyph id), whether item 0xC6
 * was gifted, and the unconditional tail (recruit incremented the roster count,
 * the 5 inter-beat delays fired, battle_anim_phase ended 0, chapter_id := 2).
 * The on-screen pixels of every dialog/cutscene beat are display side-effects
 * deferred to Phase 9.
 * ================================================================ */

extern runtime_char *data_fd2_battle_runtime_char_array_ptr;

static runtime_char g_ce2_rc[16];
static uint8        g_ce2_roster[8 * 0x50];
static uint8        g_ce2_script[1];           /* n_groups == 0 */
static uint32       g_ce2_psc;                 /* portrait_sprite_cache scratch */

/* dialog program: header words prog[6..10] redirect each page to its opcode
 * stream. Pages 6/7 -> one distinct glyph + END; pages 8/9/10 -> immediate END.
 *   idx:  6   7   8   9  10  | 12 13 | 14 15 | 16
 *   off: 24  28  32  32  32  | g6 -1 | g7 -1 | -1   (byte offsets = idx*2) */
static int16        g_ce2_text[20];

static runtime_char *g_ce2_saved_rc_ptr;

static void ce2_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    for (i = 0; i < 20; i++) {
        g_ce2_text[i] = 0;
    }
    g_ce2_text[6]  = 24;      /* page 6 -> idx 12 */
    g_ce2_text[7]  = 28;      /* page 7 -> idx 14 */
    g_ce2_text[8]  = 32;      /* page 8 -> idx 16 (immediate END) */
    g_ce2_text[9]  = 32;      /* page 9 -> idx 16 */
    g_ce2_text[10] = 32;      /* page 10 -> idx 16 */
    g_ce2_text[12] = 0x66;    /* page 6 glyph */
    g_ce2_text[13] = -1;      /* END */
    g_ce2_text[14] = 0x77;    /* page 7 glyph */
    g_ce2_text[15] = -1;      /* END */
    g_ce2_text[16] = -1;      /* END (pages 8/9/10) */
    current_chapter_text = (uint32)g_ce2_text;

    /* runtime-char array: 16 local slots so chars[5..0xA] + char-#8 roster
     * append are valid. char 0 = team-2 player with empty backpack for the
     * item-gift observable; villagers 5..0xA default alive (flags == 0). */
    g_ce2_saved_rc_ptr = data_fd2_battle_runtime_char_array_ptr;
    memset(g_ce2_rc, 0, sizeof(g_ce2_rc));
    g_ce2_rc[0].team = 2;
    g_ce2_rc[0].char_id = 1;
    g_ce2_rc[0].pos_x = 5;
    g_ce2_rc[0].pos_y = 5;
    /* mark all 8 backpack slots empty (flag byte 0x80 = available), matching
     * the engine's empty-slot encoding so the real fd2_add_item_to_inventory
     * (which only fills a slot whose flag bit 0x80 is set) can place the gift. */
    for (i = 0; i < 8; i++) {
        g_ce2_rc[0].inventory_slots[i * 2] = 0x80;
        g_ce2_rc[0].inventory_slots[i * 2 + 1] = 0;
    }
    data_fd2_battle_runtime_char_array_ptr = g_ce2_rc;
    data_fd2_battle_party_member_count = 1;       /* gift + compose + save scan */
    g_check_char_is_dead_return = 0;              /* char 0 alive */

    /* roster/template buffer for the recruit append + save pass. */
    memset(g_ce2_roster, 0, sizeof(g_ce2_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce2_roster;
    data_fd2_shared_menu_party_member_count = 0;

    /* portrait dump: skip the race scan (alloc_offset 0 -> no FDICON parse),
     * give the FD2.TMP fwrite a real 0x32A00 source, re-read a valid FDFIELD
     * index (chapter 4 -> idx 0xE, proven by the rsrc suite). */
    if (g_ce2_psc == 0) {
        g_ce2_psc = (uint32)malloc(0x32a00);
    }
    portrait_sprite_cache = g_ce2_psc;
    chapter_portrait_load_buffer = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_chapter_current_chapter_id = 4;      /* FDFIELD re-read idx 0xE */

    /* cutscene events 0xE/0xF/0x10 -> empty (n_groups == 0) script. */
    g_ce2_script[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0xe] = g_ce2_script;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0xf] = g_ce2_script;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x10] = g_ce2_script;
    data_fd2_chapter_cutscene_event_state = 0;    /* normal compose path */

    /* bounded view-window pans (origin near the 0xE target). */
    data_fd2_battle_view_window_origin_x = 0xc;
    data_fd2_battle_view_window_origin_y = 0;

    /* delay recorder. */
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;

    data_fd2_battle_anim_phase = 0x55;            /* poisoned; handler zeroes it */
}

/* Restore every shared global this fixture mutated back to its testglob.c
 * default, so later suites (field/chtrans, gfx/rndscene, ...) that read these
 * globals are not perturbed. Called before the asserts in each test so it runs
 * even when an ASSERT_EQ early-returns on failure. */
static void ce2_fixture_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_ce2_saved_rc_ptr;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_chapter_cutscene_event_state = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0xe] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0xf] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x10] = 0;
    portrait_sprite_cache = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_current_chapter_id = 1;
    data_fd2_battle_anim_phase = 0;
    remove("FD2.TMP");
}

/* ----------------------------------------------------------------
 * All 6 villagers alive: the handler shows dialog page 6 (its single glyph
 * 0x66 is the only one rendered) and gifts item 0xC6 to player char 0. The
 * unconditional tail also runs: char #8 recruited (roster count 0 -> 1), the
 * 5 inter-beat delays fired (100,200,200,200,200), anim_phase ended 0, and the
 * chapter id advanced to 2.
 * ---------------------------------------------------------------- */
static void test_chapter_02_end_villagers_alive_page6_and_gift(void)
{
    int    glyph_calls;
    uint32 glyph_idx;
    uint8  slot0_flag;
    uint8  slot0_item;
    uint32 recruit_count;
    int    delay_calls;
    uint32 delay_ticks;
    uint32 anim_phase;
    uint32 chapter_id;

    ce2_fixture_reset();
    /* villagers 5..0xA already alive (flags == 0). */

    fd2_chapter_02_end();

    /* capture every observable, then restore globals, then assert (so the
     * teardown's global-restore runs even if an assert early-returns). */
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    slot0_flag    = g_ce2_rc[0].inventory_slots[0];
    slot0_item    = g_ce2_rc[0].inventory_slots[1];
    recruit_count = data_fd2_shared_menu_party_member_count;
    delay_calls   = g_delay375b2_calls;
    delay_ticks   = g_delay375b2_last_ticks;
    anim_phase    = data_fd2_battle_anim_phase;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    ce2_fixture_teardown();

    /* branch: page 6 rendered exactly one glyph (0x66); pages 8/9/10 emit none. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x66);

    /* gift landed: fd2_add_item_to_inventory clears the slot's empty-flag (0x80
     * -> 0) and writes the item id into the paired byte. */
    ASSERT_EQ((long)slot0_flag, 0L);
    ASSERT_EQ((long)slot0_item, (long)0xc6);

    /* unconditional tail. */
    ASSERT_EQ((long)recruit_count, 1L);            /* char #8 recruited */
    ASSERT_EQ((long)delay_calls, 5L);              /* 100,200,200,200,200 */
    ASSERT_EQ((long)delay_ticks, 200L);
    ASSERT_EQ((long)anim_phase, 0L);
    ASSERT_EQ((long)chapter_id, 2L);
}

/* ----------------------------------------------------------------
 * A villager dead (flags bit0 set on one of chars[5..0xA]): the handler shows
 * the failure page 7 (single glyph 0x77) and gifts NOTHING — player char 0's
 * backpack stays empty. The unconditional tail still runs identically (recruit,
 * delays, anim_phase 0, chapter id 2), proving the gift is the only behavioural
 * difference the branch makes.
 * ---------------------------------------------------------------- */
static void test_chapter_02_end_villager_dead_page7_no_gift(void)
{
    int    glyph_calls;
    uint32 glyph_idx;
    uint8  slot0_flag;
    uint8  slot0_item;
    uint32 recruit_count;
    int    delay_calls;
    uint32 anim_phase;
    uint32 chapter_id;

    ce2_fixture_reset();
    g_ce2_rc[8].flags |= 1;   /* villager (index 8, within 5..0xA) is dead */

    fd2_chapter_02_end();

    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    slot0_flag    = g_ce2_rc[0].inventory_slots[0];
    slot0_item    = g_ce2_rc[0].inventory_slots[1];
    recruit_count = data_fd2_shared_menu_party_member_count;
    delay_calls   = g_delay375b2_calls;
    anim_phase    = data_fd2_battle_anim_phase;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    ce2_fixture_teardown();

    /* branch: page 7 rendered exactly one glyph (0x77). */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x77);

    /* no gift: char 0's first backpack slot stays empty (flag 0x80 untouched). */
    ASSERT_EQ((long)slot0_flag, (long)0x80);
    ASSERT_EQ((long)slot0_item, 0L);

    /* unconditional tail identical to the all-alive path. */
    ASSERT_EQ((long)recruit_count, 1L);
    ASSERT_EQ((long)delay_calls, 5L);
    ASSERT_EQ((long)anim_phase, 0L);
    ASSERT_EQ((long)chapter_id, 2L);
}

/* ================================================================
 * fd2_chapter_03_end @ 0x230F2
 *
 * The Chapter 3 end handler unconditionally copies three 7-byte scene tables
 * (recruit-scene X / Y / facing @ 0x520BA / 0x520C1 / 0x520C8) into on-stack
 * placement blocks and persists the party (real fd2_save_runtime_char_to_template),
 * then branches on fd2_check_char_is_dead(6):
 *   alive (== 0): stages the scene via fd2_setup_chars_and_camera_for_intro
 *     (chars 0..6, camera origin (2,0)), shows recruit dialog page 7, then
 *     recruits char #2 (劍士鐵諾, real fd2_init_runtime_char_from_base_growth).
 *   dead  (!= 0): shows the no-recruit dialog page 6 only — no scene, no recruit.
 * It then advances chapter_id by 1.
 *
 * Every callee is the real linked function EXCEPT two seams:
 *   - fd2_check_char_is_dead -> testglob.c stub (g_check_char_is_dead_return),
 *     selecting the branch (this also gates the save's char-0 dead-skip, which is
 *     harmless here).
 *   - fd2_setup_chars_and_camera_for_intro -> testglob.c recording fake (the real
 *     one is an unemitted VGA scene stager deferred to Phase 9); it snapshots the
 *     three placement tables + char range + camera origin so this test verifies
 *     the table copy and the exact scalar args the handler passed.
 *
 * Fixtures mirror the chapter 01/02 suites: a minimal dialog program whose
 * branch pages 6 and 7 each redirect to one distinct glyph (0x66 / 0x77) + END,
 * a zeroed runtime-char array + zeroed roster (member_count seeded so the recruit
 * append and chapter-id transition are observable). On-screen pixels of the
 * dialog/scene are display side-effects deferred to Phase 9.
 *
 * Asserted per branch: which dialog page ran (glyph id), whether the scene stager
 * fired and (alive) the exact copied tables + char range (0..6) + camera (2,0),
 * whether char #2 was recruited (roster count delta), and chapter_id := prev+1.
 * ================================================================ */

extern int    g_setup_intro_calls;
extern uint8  g_setup_intro_px[7];
extern uint8  g_setup_intro_py[7];
extern uint8  g_setup_intro_facing[7];
extern int32  g_setup_intro_char_start;
extern int32  g_setup_intro_char_end;
extern uint32 g_setup_intro_extra_char_idx;
extern uint32 g_setup_intro_camera_x;
extern uint32 g_setup_intro_camera_y;

extern uint8 data_fd2_chapter_ch03_end_scene_char_pos_x_table[7];
extern uint8 data_fd2_chapter_ch03_end_scene_char_pos_y_table[7];
extern uint8 data_fd2_chapter_ch03_end_scene_char_facing_table[7];

static uint8  g_ce3_roster[8 * 0x50];
static int16  g_ce3_text[20];

static void ce3_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    /* dialog program: pages 6 and 7 each redirect to one distinct glyph + END. */
    for (i = 0; i < 20; i++) {
        g_ce3_text[i] = 0;
    }
    g_ce3_text[6]  = 24;      /* page 6 -> idx 12 */
    g_ce3_text[7]  = 28;      /* page 7 -> idx 14 */
    g_ce3_text[12] = 0x66;    /* page 6 glyph */
    g_ce3_text[13] = -1;      /* END */
    g_ce3_text[14] = 0x77;    /* page 7 glyph */
    g_ce3_text[15] = -1;      /* END */
    current_chapter_text = (uint32)g_ce3_text;

    /* save-template safe env (chapter 01 baseline): zeroed runtime chars +
     * zeroed roster, one scanned runtime char and one template entry. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_ce3_roster, 0, sizeof(g_ce3_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce3_roster;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;  /* recruit appends at slot 1 */

    /* scene-stager recording fake reset. */
    g_setup_intro_calls = 0;
    g_setup_intro_char_start = -1;
    g_setup_intro_char_end = -1;
    g_setup_intro_extra_char_idx = 0xFFFFFFFFuL;
    g_setup_intro_camera_x = 0xFFFFFFFFuL;
    g_setup_intro_camera_y = 0xFFFFFFFFuL;
    for (i = 0; i < 7; i++) {
        g_setup_intro_px[i] = 0xFF;
        g_setup_intro_py[i] = 0xFF;
        g_setup_intro_facing[i] = 0xFF;
    }

    /* preset chapter id to a known value so the +1 transition is observable. */
    data_fd2_chapter_current_chapter_id = 3;
}

static void ce3_fixture_teardown(void)
{
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    g_check_char_is_dead_return = 0;
}

/* ----------------------------------------------------------------
 * Chapter survivor char #6 alive (dead-check 0): the handler stages the recruit
 * scene (setup fired once with the three tables copied verbatim, chars 0..6,
 * camera (2,0)), runs recruit dialog page 7 (its single glyph 0x77), recruits
 * char #2 (roster count 1 -> 2), and advances chapter_id 3 -> 4.
 * ---------------------------------------------------------------- */
static void test_chapter_03_end_survivor_alive_recruits(void)
{
    int    setup_calls;
    int    glyph_calls;
    uint32 glyph_idx;
    int32  char_start;
    int32  char_end;
    uint32 extra_idx;
    uint32 cam_x;
    uint32 cam_y;
    uint32 recruit_count;
    uint32 chapter_id;
    int    tables_match;
    int    i;

    ce3_fixture_reset();
    g_check_char_is_dead_return = 0;   /* survivor #6 alive -> recruit branch */

    fd2_chapter_03_end();

    /* snapshot observables, then restore globals, then assert. */
    setup_calls   = g_setup_intro_calls;
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    char_start    = g_setup_intro_char_start;
    char_end      = g_setup_intro_char_end;
    extra_idx     = g_setup_intro_extra_char_idx;
    cam_x         = g_setup_intro_camera_x;
    cam_y         = g_setup_intro_camera_y;
    recruit_count = data_fd2_shared_menu_party_member_count;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    tables_match  = 1;
    for (i = 0; i < 7; i++) {
        if (g_setup_intro_px[i] != data_fd2_chapter_ch03_end_scene_char_pos_x_table[i] ||
            g_setup_intro_py[i] != data_fd2_chapter_ch03_end_scene_char_pos_y_table[i] ||
            g_setup_intro_facing[i] != data_fd2_chapter_ch03_end_scene_char_facing_table[i]) {
            tables_match = 0;
        }
    }
    ce3_fixture_teardown();

    /* scene staged once with the three tables copied verbatim into the blocks. */
    ASSERT_EQ((long)setup_calls, 1L);
    ASSERT_EQ((long)tables_match, 1L);
    ASSERT_EQ((long)char_start, 0L);
    ASSERT_EQ((long)char_end, 6L);
    ASSERT_EQ((long)extra_idx, 0L);    /* no additional char placed */
    ASSERT_EQ((long)cam_x, 2L);
    ASSERT_EQ((long)cam_y, 0L);        /* origin Y = dead-check result (0) */

    /* recruit dialog page 7 rendered exactly its glyph 0x77. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x77);

    /* char #2 recruited (roster grew) and chapter id advanced. */
    ASSERT_EQ((long)recruit_count, 2L);
    ASSERT_EQ((long)chapter_id, 4L);
}

/* ----------------------------------------------------------------
 * Chapter survivor char #6 dead (dead-check != 0): the handler skips the scene
 * and the recruit entirely, showing only the no-recruit dialog page 6 (glyph
 * 0x66). The roster count is unchanged, but chapter_id still advances 3 -> 4.
 * The table copy still ran (always), but no consumer fired, so the setup fake
 * stays untouched — proving the branch gates both the scene and the recruit.
 * ---------------------------------------------------------------- */
static void test_chapter_03_end_survivor_dead_no_recruit(void)
{
    int    setup_calls;
    int    glyph_calls;
    uint32 glyph_idx;
    uint32 recruit_count;
    uint32 chapter_id;

    ce3_fixture_reset();
    g_check_char_is_dead_return = 1;   /* survivor #6 dead -> no-recruit branch */

    fd2_chapter_03_end();

    setup_calls   = g_setup_intro_calls;
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    recruit_count = data_fd2_shared_menu_party_member_count;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    ce3_fixture_teardown();

    /* no scene staged, no recruit. */
    ASSERT_EQ((long)setup_calls, 0L);
    ASSERT_EQ((long)recruit_count, 1L);

    /* no-recruit dialog page 6 rendered exactly its glyph 0x66. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x66);

    /* chapter id still advances. */
    ASSERT_EQ((long)chapter_id, 4L);
}

void run_field_chend1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chend1\n");
    RUN_TEST(test_chapter_01_end_runs_dialog_and_advances_id);
    RUN_TEST(test_chapter_01_end_overwrites_stale_id);
    RUN_TEST(test_chapter_02_end_villagers_alive_page6_and_gift);
    RUN_TEST(test_chapter_02_end_villager_dead_page7_no_gift);
    RUN_TEST(test_chapter_03_end_survivor_alive_recruits);
    RUN_TEST(test_chapter_03_end_survivor_dead_no_recruit);
    printf("\n");
}
