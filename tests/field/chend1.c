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

/* ================================================================
 * fd2_chapter_04_end @ 0x231BC
 *
 * The Chapter 4 end handler is the trivial 3-step shape: it shows chapter-end
 * dialog page 4 via the real fd2_display_dialog_scene, persists battle-runtime
 * char state via the real fd2_save_runtime_char_to_template, then advances the
 * chapter id. Unlike chapter 1 (which writes the id := 1 absolutely), chapter 4
 * INCREMENTS the id (+= 1, the binary's `INC [0x53c03]`).
 *
 * Both callees are the real linked functions, so the fixture stands up the same
 * in-memory env the chapter 1 suite uses: current_chapter_text points at a
 * minimal int16 program whose page-4 header word redirects to one glyph + END
 * (so the real dialog VM runs headless via the testglob.c glyph recorder), plus
 * a zeroed runtime-char array + zeroed roster with member_count = 1 so the real
 * save pass runs harmlessly.
 *
 * Asserted: the dialog VM ran against page 4 (glyph recorder pins the page
 * index / text base), and the id transition is an INCREMENT of the prior value
 * (not an absolute set). The dialog page's pixels are display side-effects
 * deferred to Phase 9.
 * ================================================================ */

static uint8 g_ce4_tmpl[8 * 0x50];
static int16 g_ce4_text[16];

static void ce4_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    /* dialog program: page 4's header word (prog[4]) is a byte offset that
     * redirects cur_op to prog[5] = one glyph (0x44), prog[6] = -1 END. */
    for (i = 0; i < 16; i++) {
        g_ce4_text[i] = 0;
    }
    g_ce4_text[4] = 10;       /* byte offset to prog[5] (page 4 start) */
    g_ce4_text[5] = 0x44;     /* one glyph */
    g_ce4_text[6] = -1;       /* END */
    current_chapter_text = (uint32)g_ce4_text;

    /* save-template safe env (save suite baseline). */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_ce4_tmpl, 0, sizeof(g_ce4_tmpl));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce4_tmpl;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;
}

static void ce4_fixture_teardown(void)
{
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
}

/* ----------------------------------------------------------------
 * End-to-end: the handler runs dialog page 4 then advances the chapter id by 1.
 * Seeded at the in-game value (3, chapter 4 follows chapter 3), it lands on 4.
 * The glyph recorder proves the real dialog VM ran on page 4 of
 * current_chapter_text (guards a wrong text base / page index).
 * ---------------------------------------------------------------- */
static void test_chapter_04_end_runs_dialog_page4_and_increments_id(void)
{
    int    glyph_calls;
    uint32 glyph_idx;
    uint32 chapter_id;

    ce4_fixture_reset();
    data_fd2_chapter_current_chapter_id = 3;   /* chapter 4 follows chapter 3 */

    fd2_chapter_04_end();

    glyph_calls = g_dlg_glyph_calls;
    glyph_idx   = g_dlg_glyph_last_idx;
    chapter_id  = data_fd2_chapter_current_chapter_id;
    ce4_fixture_teardown();

    /* page 4 redirected to a single glyph: the real VM blitted exactly it. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x44);

    /* state transition: id incremented 3 -> 4. */
    ASSERT_EQ((long)chapter_id, 4L);
}

/* ----------------------------------------------------------------
 * The id update is a relative INCREMENT, not an absolute set: seeded with a
 * distinctive unrelated value (7), the handler leaves 8 — proving it does not
 * hardcode the id to 4 (or to 1, as chapter 1 does).
 * ---------------------------------------------------------------- */
static void test_chapter_04_end_increments_not_absolute(void)
{
    uint32 chapter_id;

    ce4_fixture_reset();
    data_fd2_chapter_current_chapter_id = 7;   /* distinctive, unrelated to 4 */

    fd2_chapter_04_end();

    chapter_id = data_fd2_chapter_current_chapter_id;
    ce4_fixture_teardown();

    ASSERT_EQ((long)chapter_id, 8L);           /* 7 + 1, not a constant */
}

/* ================================================================
 * fd2_chapter_05_end @ 0x231F9
 *
 * The Chapter 5 end handler is UNCONDITIONAL (no branch): it copies three
 * 7-byte scene tables (recruit-scene X / Y / facing @ 0x520CF / 0x520D6 /
 * 0x520DD) into on-stack placement blocks and stages the recruit scene via
 * fd2_setup_chars_and_camera_for_intro (chars 0..6, plus an extra char 0x29
 * placed at (0xC,8) facing 0, camera origin (6,4)). It then shows recruit
 * dialog page 9 (real fd2_display_dialog_scene), recruits char #10 (僧侶瑪琳,
 * real fd2_init_runtime_char_from_base_growth), persists the party (real
 * fd2_save_runtime_char_to_template), then advances chapter_id by 1.
 *
 * Every callee is the real linked function EXCEPT two seams (same as the
 * chapter 03 suite): fd2_check_char_is_dead (testglob.c stub — only gates the
 * save's char-0 dead-skip here, harmless) and fd2_setup_chars_and_camera_for_intro
 * (testglob.c recording fake — the real one is an unemitted VGA scene stager
 * deferred to Phase 9). The fake now also snapshots the extra-char placement
 * args (idx / pos_x / pos_y / facing), which chapter 5 exercises with non-zero
 * values (chapter 3 left them 0), so this test pins the full 11-arg call shape.
 *
 * Fixtures mirror the chapter 03 suite: a minimal dialog program whose page 9
 * redirects to one glyph (0x99) + END, a zeroed runtime-char array + zeroed
 * roster (member_count seeded so the recruit append and chapter-id transition
 * are observable). On-screen pixels of the dialog/scene are deferred to Phase 9.
 *
 * Asserted: the scene staged once with the three tables copied verbatim, the
 * full scalar arg set (chars 0..6, extra char 0x29 at (0xC,8) facing 0, camera
 * (6,4)), recruit dialog page 9 ran (glyph 0x99), char #10 recruited (roster
 * count delta), and chapter_id := prev+1 (a relative increment, not absolute).
 * ================================================================ */

extern int32  g_setup_intro_extra_pos_x;
extern int32  g_setup_intro_extra_pos_y;
extern int32  g_setup_intro_extra_facing;

extern uint8 data_fd2_chapter_ch05_end_scene_char_pos_x_table[7];
extern uint8 data_fd2_chapter_ch05_end_scene_char_pos_y_table[7];
extern uint8 data_fd2_chapter_ch05_end_scene_char_facing_table[7];

static uint8  g_ce5_roster[8 * 0x50];
static int16  g_ce5_text[16];

static void ce5_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    /* dialog program: page 9's header word (prog[9]) is a byte offset that
     * redirects cur_op to prog[10] = one glyph (0x99), prog[11] = -1 END. */
    for (i = 0; i < 16; i++) {
        g_ce5_text[i] = 0;
    }
    g_ce5_text[9]  = 20;      /* byte offset to prog[10] (page 9 start) */
    g_ce5_text[10] = 0x99;    /* one glyph */
    g_ce5_text[11] = -1;      /* END */
    current_chapter_text = (uint32)g_ce5_text;

    /* save-template safe env (chapter 01 baseline): zeroed runtime chars +
     * zeroed roster, one scanned runtime char and one template entry. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_ce5_roster, 0, sizeof(g_ce5_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce5_roster;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;  /* recruit appends at slot 1 */

    /* scene-stager recording fake reset. */
    g_setup_intro_calls = 0;
    g_setup_intro_char_start = -1;
    g_setup_intro_char_end = -1;
    g_setup_intro_extra_char_idx = 0xFFFFFFFFuL;
    g_setup_intro_extra_pos_x = -1;
    g_setup_intro_extra_pos_y = -1;
    g_setup_intro_extra_facing = -1;
    g_setup_intro_camera_x = 0xFFFFFFFFuL;
    g_setup_intro_camera_y = 0xFFFFFFFFuL;
    for (i = 0; i < 7; i++) {
        g_setup_intro_px[i] = 0xFF;
        g_setup_intro_py[i] = 0xFF;
        g_setup_intro_facing[i] = 0xFF;
    }

    /* preset chapter id to a known value so the +1 transition is observable. */
    data_fd2_chapter_current_chapter_id = 5;
}

static void ce5_fixture_teardown(void)
{
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    g_check_char_is_dead_return = 0;
}

/* ----------------------------------------------------------------
 * End-to-end: the unconditional handler stages the recruit scene (setup fired
 * once with the three tables copied verbatim, chars 0..6, extra char 0x29 at
 * (0xC,8) facing 0, camera (6,4)), runs recruit dialog page 9 (its single glyph
 * 0x99), recruits char #10 (roster count 1 -> 2), and advances chapter_id 5 -> 6.
 * ---------------------------------------------------------------- */
static void test_chapter_05_end_stages_scene_and_recruits(void)
{
    int    setup_calls;
    int    glyph_calls;
    uint32 glyph_idx;
    int32  char_start;
    int32  char_end;
    uint32 extra_idx;
    int32  extra_x;
    int32  extra_y;
    int32  extra_facing;
    uint32 cam_x;
    uint32 cam_y;
    uint32 recruit_count;
    uint32 chapter_id;
    int    tables_match;
    int    i;

    ce5_fixture_reset();

    fd2_chapter_05_end();

    /* snapshot observables, then restore globals, then assert. */
    setup_calls   = g_setup_intro_calls;
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    char_start    = g_setup_intro_char_start;
    char_end      = g_setup_intro_char_end;
    extra_idx     = g_setup_intro_extra_char_idx;
    extra_x       = g_setup_intro_extra_pos_x;
    extra_y       = g_setup_intro_extra_pos_y;
    extra_facing  = g_setup_intro_extra_facing;
    cam_x         = g_setup_intro_camera_x;
    cam_y         = g_setup_intro_camera_y;
    recruit_count = data_fd2_shared_menu_party_member_count;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    tables_match  = 1;
    for (i = 0; i < 7; i++) {
        if (g_setup_intro_px[i] != data_fd2_chapter_ch05_end_scene_char_pos_x_table[i] ||
            g_setup_intro_py[i] != data_fd2_chapter_ch05_end_scene_char_pos_y_table[i] ||
            g_setup_intro_facing[i] != data_fd2_chapter_ch05_end_scene_char_facing_table[i]) {
            tables_match = 0;
        }
    }
    ce5_fixture_teardown();

    /* scene staged once with the three tables copied verbatim into the blocks. */
    ASSERT_EQ((long)setup_calls, 1L);
    ASSERT_EQ((long)tables_match, 1L);
    ASSERT_EQ((long)char_start, 0L);
    ASSERT_EQ((long)char_end, 6L);

    /* extra char placed: idx 0x29 at (0xC,8) facing 0 (chapter 5's distinct
     * non-zero placement, vs chapter 3's all-zero extra args). */
    ASSERT_EQ((long)extra_idx, (long)0x29);
    ASSERT_EQ((long)extra_x, (long)0xc);
    ASSERT_EQ((long)extra_y, 8L);
    ASSERT_EQ((long)extra_facing, 0L);

    /* camera origin (6,4). */
    ASSERT_EQ((long)cam_x, 6L);
    ASSERT_EQ((long)cam_y, 4L);

    /* recruit dialog page 9 rendered exactly its glyph 0x99. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x99);

    /* char #10 recruited (roster grew) and chapter id advanced. */
    ASSERT_EQ((long)recruit_count, 2L);
    ASSERT_EQ((long)chapter_id, 6L);
}

/* ----------------------------------------------------------------
 * The chapter-id update is a relative INCREMENT, not an absolute set: seeded
 * with a distinctive unrelated value (7), the handler leaves 8 — proving it does
 * not hardcode the id. (The scene staging / recruit are unconditional and run
 * identically regardless of the seed.)
 * ---------------------------------------------------------------- */
static void test_chapter_05_end_increments_not_absolute(void)
{
    uint32 chapter_id;

    ce5_fixture_reset();
    data_fd2_chapter_current_chapter_id = 7;   /* distinctive, unrelated to 6 */

    fd2_chapter_05_end();

    chapter_id = data_fd2_chapter_current_chapter_id;
    ce5_fixture_teardown();

    ASSERT_EQ((long)chapter_id, 8L);           /* 7 + 1, not a constant */
}

/* ================================================================
 * fd2_chapter_06_end @ 0x23296
 *
 * The Chapter 6 end handler is a straight-line (no-branch) orchestrator:
 *   (1) recruits char #13 (弓兵貝克威) via the real
 *       fd2_init_runtime_char_from_base_growth (appends a roster slot from the
 *       static char base/growth tables),
 *   (2) refreshes the portrait cache for race 3 via the real
 *       fd2_load_chapter_portraits_and_dump_tmp (re-reads FDFIELD.DAT, race-scans
 *       the tile-event table, rewrites FD2.TMP from portrait_sprite_cache),
 *   (3) pans the view window to (5,0xE) via the real fd2_pan_cursor_and_window,
 *   (4) fires cutscene event 0x1B via the real fd2_cutscene_event_trigger,
 *   (5) shows chapter-end dialog page 6 via the real fd2_display_dialog_scene,
 *   (6) persists the party via the real fd2_save_runtime_char_to_template, then
 *   (7) advances chapter_id by 1 (the binary's INC [0x53c03]).
 *
 * EVERY callee is the real linked function (no fakes). The fixture mirrors the
 * chapter 02 suite's safe headless env for the same real callees:
 *   - dialog VM: current_chapter_text points at a minimal int16 program whose
 *     page-6 header word redirects to one glyph (0x66) + END, so the real VM
 *     runs headless via the testglob.c glyph recorder (BIOS kbd buffer empty,
 *     portrait latch cleared).
 *   - portrait dump: the staged real FDICON.B24 + FDFIELD.DAT are read for real
 *     and FD2.TMP is rewritten for real; alloc_offset == 0 makes the race-3 scan
 *     loop body never fire (no FDICON per-char parse), portrait_sprite_cache
 *     points at a 0x32A00 scratch buffer, and chapter_id is seeded to a value
 *     whose FDFIELD re-read index (idx 0xE @ chapter_id 4) is valid.
 *   - cutscene event 0x1B: its script-table slot points at an n_groups==0 script
 *     so the trigger is a no-op plus the trailing composite (no runtime-char
 *     indexing, so the default 8-slot g_test_rc_array suffices).
 *   - pan/compose (real): a bounded view-window pan over the tile-map compositor
 *     (recording stub).
 *   - recruit (real) + save (real): char #13 appends to the zeroed roster
 *     (harmless stats from zeroed base/growth) and runtime char 0 is persisted.
 *
 * Asserted: the dialog page that ran (glyph id 0x66 pins page 6 / text base),
 * char #13 recruited (roster count delta), the portrait dump completed for real
 * (FD2.TMP rewritten), and the chapter-id transition is a relative INCREMENT
 * (not an absolute set). On-screen pixels of the dialog/cutscene/pan are display
 * side-effects deferred to Phase 9.
 * ================================================================ */

static uint8  g_ce6_roster[8 * 0x50];
static int16  g_ce6_text[16];
static uint8  g_ce6_script[1];           /* n_groups == 0 */
static uint32 g_ce6_psc;                 /* portrait_sprite_cache scratch */

static void ce6_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    /* dialog program: page 6's header word (prog[6]) is a byte offset that
     * redirects cur_op to prog[7] = one glyph (0x66), prog[8] = -1 END. */
    for (i = 0; i < 16; i++) {
        g_ce6_text[i] = 0;
    }
    g_ce6_text[6] = 14;       /* byte offset to prog[7] (page 6 start) */
    g_ce6_text[7] = 0x66;     /* one glyph */
    g_ce6_text[8] = -1;       /* END */
    current_chapter_text = (uint32)g_ce6_text;

    /* recruit + save safe env (chapter 01 baseline): zeroed runtime chars +
     * zeroed roster, one scanned runtime char and one template entry. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_ce6_roster, 0, sizeof(g_ce6_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce6_roster;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;  /* recruit appends at slot 1 */

    /* portrait dump: skip the race scan (alloc_offset 0 -> no FDICON per-char
     * parse), give the FD2.TMP fwrite a real 0x32A00 source, re-read a valid
     * FDFIELD index (chapter 4 -> idx 0xE, proven by the rsrc suite). */
    if (g_ce6_psc == 0) {
        g_ce6_psc = (uint32)malloc(0x32a00);
    }
    portrait_sprite_cache = g_ce6_psc;
    chapter_portrait_load_buffer = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;

    /* cutscene event 0x1B -> empty (n_groups == 0) script. */
    g_ce6_script[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x1b] = g_ce6_script;
    data_fd2_chapter_cutscene_event_state = 0;    /* normal compose path */

    /* bounded view-window pan (origin near the (5,0xE) target). */
    data_fd2_battle_view_window_origin_x = 5;
    data_fd2_battle_view_window_origin_y = 0xe;

    /* seed chapter id: 4 makes the FDFIELD re-read index (4*3+2 = 0xE) valid;
     * the +1 transition is then observable as 4 -> 5. */
    data_fd2_chapter_current_chapter_id = 4;
}

static void ce6_fixture_teardown(void)
{
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_chapter_cutscene_event_state = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x1b] = 0;
    portrait_sprite_cache = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_current_chapter_id = 1;
    remove("FD2.TMP");
}

/* ----------------------------------------------------------------
 * End-to-end: the straight-line handler recruits char #13, refreshes the race-3
 * portrait set (real FDICON/FDFIELD read + FD2.TMP rewrite), pans the window,
 * fires the (empty) cutscene event, runs dialog page 6 (its single glyph 0x66),
 * persists the party, and advances chapter_id 4 -> 5. The glyph recorder pins
 * that the real dialog VM ran on page 6 of current_chapter_text; FD2.TMP's
 * presence proves the real portrait dump completed.
 * ---------------------------------------------------------------- */
static void test_chapter_06_end_recruits_loads_and_increments_id(void)
{
    int    glyph_calls;
    uint32 glyph_idx;
    uint32 recruit_count;
    uint32 chapter_id;
    FILE  *tmp_fp;
    int    tmp_present;

    ce6_fixture_reset();

    fd2_chapter_06_end();

    /* snapshot observables, then restore globals, then assert (so the teardown's
     * global-restore runs even if an assert early-returns). */
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    recruit_count = data_fd2_shared_menu_party_member_count;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    tmp_fp = fopen("FD2.TMP", "rb");
    tmp_present = (tmp_fp != NULL);
    if (tmp_fp != NULL) {
        fclose(tmp_fp);
    }
    ce6_fixture_teardown();

    /* dialog page 6 rendered exactly its glyph 0x66. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x66);

    /* char #13 recruited (roster grew 1 -> 2). */
    ASSERT_EQ((long)recruit_count, 2L);

    /* the real portrait dump rewrote FD2.TMP. */
    ASSERT_EQ((long)tmp_present, 1L);

    /* state transition: id incremented 4 -> 5 (relative, not absolute). */
    ASSERT_EQ((long)chapter_id, 5L);
}

/* ----------------------------------------------------------------
 * The chapter-id update is a relative INCREMENT, not an absolute set: seeded
 * with a distinctive unrelated value (7), the handler leaves 8 — proving it does
 * not hardcode the id. (The FDFIELD re-read index 7*3+2 = 0x17 is still a valid
 * FDFIELD entry, so the real portrait dump runs unchanged.)
 * ---------------------------------------------------------------- */
static void test_chapter_06_end_increments_not_absolute(void)
{
    uint32 chapter_id;

    ce6_fixture_reset();
    data_fd2_chapter_current_chapter_id = 7;   /* distinctive, unrelated to 5 */

    fd2_chapter_06_end();

    chapter_id = data_fd2_chapter_current_chapter_id;
    ce6_fixture_teardown();

    ASSERT_EQ((long)chapter_id, 8L);           /* 7 + 1, not a constant */
}

/* ================================================================
 * fd2_chapter_07_end @ 0x232E8
 *
 * The Chapter 7 end handler unconditionally copies three 9-byte scene tables
 * (recruit-scene X / Y / facing @ 0x520E4 / 0x520ED / 0x520F6) into on-stack
 * placement blocks and persists the party (real fd2_save_runtime_char_to_template),
 * then takes a DUAL-condition recruit:
 *   tile_event_consumed_flags[0x11] == 1  AND  fd2_check_char_is_dead(0x2B) == 0:
 *     stages the scene via fd2_setup_chars_and_camera_for_intro (chars 0..8, plus
 *     extra char 0x2B at (0xC,7) facing 2, camera origin (6,2)), shows recruit
 *     dialog page 4, recruits char #12 (武者凱麗, real
 *     fd2_init_runtime_char_from_base_growth).
 *   otherwise: shows the no-recruit dialog page 5 only — no scene, no recruit.
 * It then advances chapter_id by 1.
 *
 * The C `&&` short-circuits exactly as the binary does (CMP flag==1 / JNZ, then
 * the dead-check CALL only on the flag-true fall-through), so when the flag is
 * clear the dead-check is never invoked.
 *
 * Every callee is the real linked function EXCEPT two seams (same as the chapter
 * 03/05 suites): fd2_check_char_is_dead (testglob.c stub — now also records its
 * call count + last arg so this suite can pin the short-circuit and the 0x2B
 * query) and fd2_setup_chars_and_camera_for_intro (testglob.c recording fake —
 * the real one is an unemitted VGA scene stager deferred to Phase 9; its capture
 * loop is bounded by the inclusive char range, so it snapshots all 9 table
 * entries here). The real save pass is made a clean no-op (battle_party_member_count
 * == 0) so the dead-check call recorder reflects only the dual-condition's query.
 *
 * Fixtures mirror the chapter 03/05 suites: a minimal dialog program whose pages 4
 * and 5 each redirect to one distinct glyph (0x44 / 0x55) + END, a zeroed
 * runtime-char array + zeroed roster, and a 0x20-byte tile-event consumed-flags
 * buffer. On-screen pixels of the dialog/scene are deferred to Phase 9.
 *
 * Asserted per branch: which dialog page ran (glyph id), whether the scene staged
 * and (recruit) the exact 9 copied table entries + char range (0..8) + extra char
 * 0x2B at (0xC,7) facing 2 + camera (6,2), whether char #12 was recruited (roster
 * count delta), whether/with-what the dead-check was queried, and chapter_id := prev+1.
 * ================================================================ */

extern int    g_check_char_is_dead_calls;
extern uint32 g_check_char_is_dead_last_arg;

extern uint8 data_fd2_chapter_ch07_end_scene_char_pos_x_table[9];
extern uint8 data_fd2_chapter_ch07_end_scene_char_pos_y_table[9];
extern uint8 data_fd2_chapter_ch07_end_scene_char_facing_table[9];

static uint8  g_ce7_roster[8 * 0x50];
static int16  g_ce7_text[16];
static uint8  g_ce7_consumed[0x20];

static void ce7_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    /* dialog program: pages 4 and 5 each redirect to one distinct glyph + END. */
    for (i = 0; i < 16; i++) {
        g_ce7_text[i] = 0;
    }
    g_ce7_text[4]  = 16;      /* page 4 -> idx 8 */
    g_ce7_text[5]  = 20;      /* page 5 -> idx 10 */
    g_ce7_text[8]  = 0x44;    /* page 4 glyph */
    g_ce7_text[9]  = -1;      /* END */
    g_ce7_text[10] = 0x55;    /* page 5 glyph */
    g_ce7_text[11] = -1;      /* END */
    current_chapter_text = (uint32)g_ce7_text;

    /* save pass made a clean no-op (no runtime chars scanned) so the dead-check
     * recorder reflects only the dual-condition's query; the recruit append uses
     * the shared-menu count so roster growth is still observable. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_ce7_roster, 0, sizeof(g_ce7_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce7_roster;
    data_fd2_battle_party_member_count = 0;       /* save pass: no-op */
    data_fd2_shared_menu_party_member_count = 0;  /* recruit appends at slot 0 */

    /* tile-event consumed-flags buffer (index 0x11 is the gate). */
    memset(g_ce7_consumed, 0, sizeof(g_ce7_consumed));
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce7_consumed;

    /* dead-check stub recorder + return reset. */
    g_check_char_is_dead_return = 0;
    g_check_char_is_dead_calls = 0;
    g_check_char_is_dead_last_arg = 0xFFFFFFFFuL;

    /* scene-stager recording fake reset. */
    g_setup_intro_calls = 0;
    g_setup_intro_char_start = -1;
    g_setup_intro_char_end = -1;
    g_setup_intro_extra_char_idx = 0xFFFFFFFFuL;
    g_setup_intro_extra_pos_x = -1;
    g_setup_intro_extra_pos_y = -1;
    g_setup_intro_extra_facing = -1;
    g_setup_intro_camera_x = 0xFFFFFFFFuL;
    g_setup_intro_camera_y = 0xFFFFFFFFuL;
    for (i = 0; i < 9; i++) {
        g_setup_intro_px[i] = 0xFF;
        g_setup_intro_py[i] = 0xFF;
        g_setup_intro_facing[i] = 0xFF;
    }

    /* preset chapter id to a known value so the +1 transition is observable. */
    data_fd2_chapter_current_chapter_id = 7;
}

static void ce7_fixture_teardown(void)
{
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_chapter_current_chapter_id = 1;
    g_check_char_is_dead_return = 0;
}

/* ----------------------------------------------------------------
 * Both conditions met (flag[0x11]==1 AND char #0x2B alive): the handler stages
 * the recruit scene (setup fired once with the three 9-byte tables copied
 * verbatim, chars 0..8, extra char 0x2B at (0xC,7) facing 2, camera (6,2)), runs
 * recruit dialog page 4 (its single glyph 0x44), recruits char #12 (roster count
 * 0 -> 1), and advances chapter_id 7 -> 8. The dead-check was queried for char
 * 0x2B (the second conjunct).
 * ---------------------------------------------------------------- */
static void test_chapter_07_end_flag_and_alive_recruits(void)
{
    int    setup_calls;
    int    glyph_calls;
    uint32 glyph_idx;
    int32  char_start;
    int32  char_end;
    uint32 extra_idx;
    int32  extra_x;
    int32  extra_y;
    int32  extra_facing;
    uint32 cam_x;
    uint32 cam_y;
    uint32 recruit_count;
    uint32 chapter_id;
    int    dead_calls;
    uint32 dead_arg;
    int    tables_match;
    int    i;

    ce7_fixture_reset();
    g_ce7_consumed[0x11] = 1;          /* tile event 0x11 triggered */
    g_check_char_is_dead_return = 0;   /* char 0x2B alive -> recruit branch */

    fd2_chapter_07_end();

    /* snapshot observables, then restore globals, then assert. */
    setup_calls   = g_setup_intro_calls;
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    char_start    = g_setup_intro_char_start;
    char_end      = g_setup_intro_char_end;
    extra_idx     = g_setup_intro_extra_char_idx;
    extra_x       = g_setup_intro_extra_pos_x;
    extra_y       = g_setup_intro_extra_pos_y;
    extra_facing  = g_setup_intro_extra_facing;
    cam_x         = g_setup_intro_camera_x;
    cam_y         = g_setup_intro_camera_y;
    recruit_count = data_fd2_shared_menu_party_member_count;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    dead_calls    = g_check_char_is_dead_calls;
    dead_arg      = g_check_char_is_dead_last_arg;
    tables_match  = 1;
    for (i = 0; i < 9; i++) {
        if (g_setup_intro_px[i] != data_fd2_chapter_ch07_end_scene_char_pos_x_table[i] ||
            g_setup_intro_py[i] != data_fd2_chapter_ch07_end_scene_char_pos_y_table[i] ||
            g_setup_intro_facing[i] != data_fd2_chapter_ch07_end_scene_char_facing_table[i]) {
            tables_match = 0;
        }
    }
    ce7_fixture_teardown();

    /* scene staged once with all 9 table entries copied verbatim into the blocks. */
    ASSERT_EQ((long)setup_calls, 1L);
    ASSERT_EQ((long)tables_match, 1L);
    ASSERT_EQ((long)char_start, 0L);
    ASSERT_EQ((long)char_end, 8L);

    /* extra char placed: idx 0x2B at (0xC,7) facing 2. */
    ASSERT_EQ((long)extra_idx, (long)0x2b);
    ASSERT_EQ((long)extra_x, (long)0xc);
    ASSERT_EQ((long)extra_y, 7L);
    ASSERT_EQ((long)extra_facing, 2L);

    /* camera origin (6,2). */
    ASSERT_EQ((long)cam_x, 6L);
    ASSERT_EQ((long)cam_y, 2L);

    /* recruit dialog page 4 rendered exactly its glyph 0x44. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x44);

    /* the second conjunct was evaluated: dead-check queried char 0x2B. */
    ASSERT_EQ((long)dead_calls, 1L);
    ASSERT_EQ((long)dead_arg, (long)0x2b);

    /* char #12 recruited (roster grew) and chapter id advanced. */
    ASSERT_EQ((long)recruit_count, 1L);
    ASSERT_EQ((long)chapter_id, 8L);
}

/* ----------------------------------------------------------------
 * Flag set but char #0x2B dead (flag[0x11]==1, dead-check != 0): the second
 * conjunct fails, so the handler skips the scene and the recruit, showing only
 * the no-recruit dialog page 5 (glyph 0x55). The dead-check WAS queried for char
 * 0x2B (the flag passed, so the conjunct was reached). Roster unchanged; chapter
 * id still advances 7 -> 8.
 * ---------------------------------------------------------------- */
static void test_chapter_07_end_flag_set_but_dead_no_recruit(void)
{
    int    setup_calls;
    int    glyph_calls;
    uint32 glyph_idx;
    uint32 recruit_count;
    uint32 chapter_id;
    int    dead_calls;
    uint32 dead_arg;

    ce7_fixture_reset();
    g_ce7_consumed[0x11] = 1;          /* tile event 0x11 triggered */
    g_check_char_is_dead_return = 1;   /* char 0x2B dead -> no-recruit branch */

    fd2_chapter_07_end();

    setup_calls   = g_setup_intro_calls;
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    recruit_count = data_fd2_shared_menu_party_member_count;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    dead_calls    = g_check_char_is_dead_calls;
    dead_arg      = g_check_char_is_dead_last_arg;
    ce7_fixture_teardown();

    /* no scene staged, no recruit. */
    ASSERT_EQ((long)setup_calls, 0L);
    ASSERT_EQ((long)recruit_count, 0L);

    /* the conjunct was reached (flag passed): dead-check queried char 0x2B. */
    ASSERT_EQ((long)dead_calls, 1L);
    ASSERT_EQ((long)dead_arg, (long)0x2b);

    /* no-recruit dialog page 5 rendered exactly its glyph 0x55. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x55);

    /* chapter id still advances. */
    ASSERT_EQ((long)chapter_id, 8L);
}

/* ----------------------------------------------------------------
 * Flag clear (flag[0x11]==0): the FIRST conjunct fails, so the && short-circuits
 * — the dead-check is never invoked — and the handler shows only the no-recruit
 * dialog page 5 (glyph 0x55) with no scene and no recruit. (Even with the dead-
 * check primed to "alive", the recruit must not fire, proving the flag gate.)
 * Chapter id still advances 7 -> 8.
 * ---------------------------------------------------------------- */
static void test_chapter_07_end_flag_clear_short_circuits(void)
{
    int    setup_calls;
    int    glyph_calls;
    uint32 glyph_idx;
    uint32 recruit_count;
    uint32 chapter_id;
    int    dead_calls;

    ce7_fixture_reset();
    /* g_ce7_consumed[0x11] left 0: first conjunct fails. */
    g_check_char_is_dead_return = 0;   /* primed alive — must be irrelevant. */

    fd2_chapter_07_end();

    setup_calls   = g_setup_intro_calls;
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    recruit_count = data_fd2_shared_menu_party_member_count;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    dead_calls    = g_check_char_is_dead_calls;
    ce7_fixture_teardown();

    /* no scene staged, no recruit. */
    ASSERT_EQ((long)setup_calls, 0L);
    ASSERT_EQ((long)recruit_count, 0L);

    /* short-circuit: the dead-check was never invoked. */
    ASSERT_EQ((long)dead_calls, 0L);

    /* no-recruit dialog page 5 rendered exactly its glyph 0x55. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x55);

    /* chapter id still advances. */
    ASSERT_EQ((long)chapter_id, 8L);
}

/* ================================================================
 * fd2_chapter_08_end @ 0x234BB
 *
 * The Chapter 8 end handler is UNCONDITIONAL (no branch): it copies two 10-byte
 * scene position tables (recruit-scene X / Y @ 0x520FF / 0x52109) into on-stack
 * placement blocks and stages the recruit scene via
 * fd2_setup_chars_and_camera_for_intro with an INLINE FIXED facing value 2
 * (< 4) — so unlike chapters 3/5/7 (which pass a facing-table address >= 4),
 * chapter 8 has no facing table and every placed char faces direction 2 (chars
 * 0..9, extra char 0x1C at (0xE,0x10) facing 0, camera origin (8,0xE)). It then
 * plays the post-battle cutscene — dialog page 3; battle_anim_phase = 0; event
 * 0x21; dialog page 4; cutscene_event_state = 1; battle_anim_phase = 0; event
 * 0x22; cutscene_event_state = 0 — fades to black (real fd2_set_vga_palette_range
 * then memset(0xA0000,0,64000)), recruits char #5 (騎士洛娜) via the real
 * fd2_init_runtime_char_from_base_growth, persists the party (real
 * fd2_save_runtime_char_to_template), then advances chapter_id by 1.
 *
 * Every callee is the real linked function EXCEPT two seams (same as the chapter
 * 03/05/07 suites): fd2_check_char_is_dead (testglob.c stub — only gates the
 * save's char-0 dead-skip here, and the composite char-paint loop, both no-ops
 * with party_member_count 0) and fd2_setup_chars_and_camera_for_intro (testglob.c
 * recording fake — the real one is an unemitted VGA scene stager deferred to
 * Phase 9). The fake now also records the raw facing argument
 * (g_setup_intro_facing_arg), which chapter 8 exercises with the fixed value 2.
 *
 * Fixtures mirror the chapter 06/07 suites' safe headless env: a minimal dialog
 * program whose pages 3 and 4 each redirect to one distinct glyph (0x33 / 0x44)
 * + END; party_member_count 0 so the save scan and the composite char-paint loop
 * are clean no-ops; the cutscene event slots 0x21/0x22 point at empty (n_groups
 * == 0) scripts; and a 768-byte VGA palette buffer so the real fade-to-black
 * palette write reads valid source bytes. On-screen pixels of the dialog /
 * cutscene / scene / fade are display side-effects deferred to Phase 9.
 *
 * Asserted: the scene staged once with the fixed facing arg 2 + the full scalar
 * arg set (chars 0..9, extra char 0x1C at (0xE,0x10) facing 0, camera (8,0xE))
 * + the X/Y tables copied verbatim; both dialog pages ran in order (3 then 4, via
 * the glyph recorder: 2 glyphs, last = page-4's 0x44); the cutscene_event_state
 * toggle ended 0 and battle_anim_phase ended 0; char #5 was recruited (roster
 * count delta); and chapter_id := prev+1 (a relative increment, not absolute).
 * ================================================================ */

extern uint32 g_setup_intro_facing_arg;

extern uint8 data_fd2_chapter_ch08_end_scene_char_pos_x_table[10];
extern uint8 data_fd2_chapter_ch08_end_scene_char_pos_y_table[10];

static uint8  g_ce8_roster[8 * 0x50];
static int16  g_ce8_text[16];
static uint8  g_ce8_script[1];           /* n_groups == 0 */
static uint8  g_ce8_palette[768];        /* fade-to-black palette source */

static void ce8_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    /* dialog program: page 3 -> idx 8 (glyph 0x33 + END), page 4 -> idx 10
     * (glyph 0x44 + END). Distinct glyphs pin the page order (3 before 4). */
    for (i = 0; i < 16; i++) {
        g_ce8_text[i] = 0;
    }
    g_ce8_text[3]  = 16;      /* page 3 -> idx 8 */
    g_ce8_text[4]  = 20;      /* page 4 -> idx 10 */
    g_ce8_text[8]  = 0x33;    /* page 3 glyph */
    g_ce8_text[9]  = -1;      /* END */
    g_ce8_text[10] = 0x44;    /* page 4 glyph */
    g_ce8_text[11] = -1;      /* END */
    current_chapter_text = (uint32)g_ce8_text;

    /* save pass made a clean no-op (no runtime chars scanned), which also makes
     * the composite char-paint loop a no-op; the recruit append uses the
     * shared-menu count so roster growth is still observable. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_ce8_roster, 0, sizeof(g_ce8_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce8_roster;
    data_fd2_battle_party_member_count = 0;       /* save + composite: no-op */
    data_fd2_shared_menu_party_member_count = 0;  /* recruit appends at slot 0 */
    g_check_char_is_dead_return = 0;

    /* cutscene events 0x21/0x22 -> empty (n_groups == 0) script. */
    g_ce8_script[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x21] = g_ce8_script;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x22] = g_ce8_script;

    /* fade-to-black palette source (real fd2_set_vga_palette_range reads
     * palette[idx*3 + 0..2] for idx 0..0xFF = 768 bytes). */
    memset(g_ce8_palette, 0, sizeof(g_ce8_palette));
    data_fd2_vga_palette_data_ptr = (uint32)g_ce8_palette;

    /* bounded view-window origin for the composite passes. */
    data_fd2_battle_view_window_origin_x = 8;
    data_fd2_battle_view_window_origin_y = 0xe;

    /* scene-stager recording fake reset. */
    g_setup_intro_calls = 0;
    g_setup_intro_facing_arg = 0xFFFFFFFFuL;
    g_setup_intro_char_start = -1;
    g_setup_intro_char_end = -1;
    g_setup_intro_extra_char_idx = 0xFFFFFFFFuL;
    g_setup_intro_extra_pos_x = -1;
    g_setup_intro_extra_pos_y = -1;
    g_setup_intro_extra_facing = -1;
    g_setup_intro_camera_x = 0xFFFFFFFFuL;
    g_setup_intro_camera_y = 0xFFFFFFFFuL;
    for (i = 0; i < 9; i++) {
        g_setup_intro_px[i] = 0xFF;
        g_setup_intro_py[i] = 0xFF;
        g_setup_intro_facing[i] = 0xFF;
    }

    /* poison the toggled / reset state so the handler's writes are observable. */
    data_fd2_chapter_cutscene_event_state = 0x77;
    data_fd2_battle_anim_phase = 0x55;

    /* preset chapter id to a known value so the +1 transition is observable. */
    data_fd2_chapter_current_chapter_id = 8;
}

static void ce8_fixture_teardown(void)
{
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x21] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x22] = 0;
    data_fd2_chapter_cutscene_event_state = 0;
    data_fd2_vga_palette_data_ptr = 0;
    data_fd2_chapter_current_chapter_id = 1;
    data_fd2_battle_anim_phase = 0;
    g_check_char_is_dead_return = 0;
}

/* ----------------------------------------------------------------
 * End-to-end: the unconditional handler stages the recruit scene (setup fired
 * once with the fixed facing arg 2, the two 10-byte tables copied verbatim,
 * chars 0..9, extra char 0x1C at (0xE,0x10) facing 0, camera (8,0xE)), runs the
 * cutscene (dialog pages 3 then 4, glyphs 0x33 then 0x44; cutscene_event_state
 * toggled back to 0; battle_anim_phase reset to 0), fades to black, recruits
 * char #5 (roster count 0 -> 1), and advances chapter_id 8 -> 9.
 * ---------------------------------------------------------------- */
static void test_chapter_08_end_stages_scene_cutscene_and_recruits(void)
{
    int    setup_calls;
    uint32 facing_arg;
    int    glyph_calls;
    uint32 glyph_idx;
    int32  char_start;
    int32  char_end;
    uint32 extra_idx;
    int32  extra_x;
    int32  extra_y;
    int32  extra_facing;
    uint32 cam_x;
    uint32 cam_y;
    uint32 event_state;
    uint32 anim_phase;
    uint32 recruit_count;
    uint32 chapter_id;
    int    tables_match;
    int    i;

    ce8_fixture_reset();

    fd2_chapter_08_end();

    /* snapshot observables, then restore globals, then assert. */
    setup_calls   = g_setup_intro_calls;
    facing_arg    = g_setup_intro_facing_arg;
    glyph_calls   = g_dlg_glyph_calls;
    glyph_idx     = g_dlg_glyph_last_idx;
    char_start    = g_setup_intro_char_start;
    char_end      = g_setup_intro_char_end;
    extra_idx     = g_setup_intro_extra_char_idx;
    extra_x       = g_setup_intro_extra_pos_x;
    extra_y       = g_setup_intro_extra_pos_y;
    extra_facing  = g_setup_intro_extra_facing;
    cam_x         = g_setup_intro_camera_x;
    cam_y         = g_setup_intro_camera_y;
    event_state   = data_fd2_chapter_cutscene_event_state;
    anim_phase    = data_fd2_battle_anim_phase;
    recruit_count = data_fd2_shared_menu_party_member_count;
    chapter_id    = data_fd2_chapter_current_chapter_id;
    tables_match  = 1;
    for (i = 0; i < 9; i++) {
        if (g_setup_intro_px[i] != data_fd2_chapter_ch08_end_scene_char_pos_x_table[i] ||
            g_setup_intro_py[i] != data_fd2_chapter_ch08_end_scene_char_pos_y_table[i]) {
            tables_match = 0;
        }
    }
    ce8_fixture_teardown();

    /* scene staged once; facing is the inline fixed value 2 (NOT a table addr). */
    ASSERT_EQ((long)setup_calls, 1L);
    ASSERT_EQ((long)facing_arg, 2L);
    ASSERT_EQ((long)tables_match, 1L);
    ASSERT_EQ((long)char_start, 0L);
    ASSERT_EQ((long)char_end, 9L);

    /* extra char placed: idx 0x1C at (0xE,0x10) facing 0. */
    ASSERT_EQ((long)extra_idx, (long)0x1c);
    ASSERT_EQ((long)extra_x, (long)0xe);
    ASSERT_EQ((long)extra_y, (long)0x10);
    ASSERT_EQ((long)extra_facing, 0L);

    /* camera origin (8,0xE). */
    ASSERT_EQ((long)cam_x, 8L);
    ASSERT_EQ((long)cam_y, (long)0xe);

    /* both dialog pages ran in order: page 3 (0x33) then page 4 (0x44). */
    ASSERT_EQ((long)glyph_calls, 2);
    ASSERT_EQ((long)glyph_idx, (long)0x44);

    /* cutscene_event_state toggled 1 then back to 0; battle_anim_phase reset 0. */
    ASSERT_EQ((long)event_state, 0L);
    ASSERT_EQ((long)anim_phase, 0L);

    /* char #5 recruited (roster grew 0 -> 1) and chapter id advanced 8 -> 9. */
    ASSERT_EQ((long)recruit_count, 1L);
    ASSERT_EQ((long)chapter_id, 9L);
}

/* ----------------------------------------------------------------
 * The chapter-id update is a relative INCREMENT, not an absolute set: seeded
 * with a distinctive unrelated value (3), the handler leaves 4 — proving it does
 * not hardcode the id. (The scene staging / cutscene / recruit are unconditional
 * and run identically regardless of the seed.)
 * ---------------------------------------------------------------- */
static void test_chapter_08_end_increments_not_absolute(void)
{
    uint32 chapter_id;

    ce8_fixture_reset();
    data_fd2_chapter_current_chapter_id = 3;   /* distinctive, unrelated to 9 */

    fd2_chapter_08_end();

    chapter_id = data_fd2_chapter_current_chapter_id;
    ce8_fixture_teardown();

    ASSERT_EQ((long)chapter_id, 4L);           /* 3 + 1, not a constant */
}

/* ================================================================
 * fd2_chapter_09_end @ 0x235BC
 *
 * The Chapter 9 end handler is a straight-line (no-branch) orchestrator that
 * differs from the chapter 06 shape only by a leading REVIVE step:
 *   (1) revives runtime char #11 by clearing all of its status flags
 *       (runtime_char[0xB].flags = 0 — the binary's
 *       MOV byte ptr [runtime_char_array_ptr + 0xB*0x50 + 0x5], 0); the char was
 *       previously asleep/disabled, so this re-enables it (no new recruit),
 *   (2) pans the view window to (6,1) via the real fd2_pan_cursor_and_window,
 *   (3) refreshes the portrait cache for race 4 via the real
 *       fd2_load_chapter_portraits_and_dump_tmp (re-reads FDFIELD.DAT, race-scans
 *       the tile-event table, rewrites FD2.TMP from portrait_sprite_cache),
 *   (4) fires cutscene event 0x24 via the real fd2_cutscene_event_trigger,
 *   (5) shows chapter-end dialog page 4 via the real fd2_display_dialog_scene
 *       (the shared chapter-04 tail @ 0x231C6),
 *   (6) persists the party via the real fd2_save_runtime_char_to_template, then
 *   (7) advances chapter_id by 1 (the binary's INC [0x53c03]).
 *
 * EVERY callee is the real linked function (no fakes). The fixture mirrors the
 * chapter 06 suite's safe headless env, plus the chapter 02 suite's runtime-char
 * array swap: data_fd2_battle_runtime_char_array_ptr is repointed at a local
 * 16-slot array so index 0xB is in bounds, and char #11 is seeded with poisoned
 * flags 0x05 (dead|cannot_act) so the revive (-> 0) is observable. Unlike chapter
 * 06 there is no recruit, so the roster count is not asserted; the real save pass
 * still runs over runtime char 0.
 *
 * Asserted: char #11 revived (flags 0x05 -> 0), the dialog page that ran (glyph
 * id 0x66 pins page 4 / text base), the real portrait dump completed (FD2.TMP
 * rewritten), and the chapter-id transition is a relative INCREMENT (not an
 * absolute set). On-screen pixels of the dialog/cutscene/pan are display
 * side-effects deferred to Phase 9.
 * ================================================================ */

static runtime_char g_ce9_rc[16];
static uint8        g_ce9_roster[8 * 0x50];
static uint8        g_ce9_script[1];           /* n_groups == 0 */
static uint32       g_ce9_psc;                 /* portrait_sprite_cache scratch */
static int16        g_ce9_text[16];
static runtime_char *g_ce9_saved_rc_ptr;

static void ce9_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    /* dialog program: page 4's header word (prog[4]) is a byte offset that
     * redirects cur_op to prog[5] = one glyph (0x66), prog[6] = -1 END. */
    for (i = 0; i < 16; i++) {
        g_ce9_text[i] = 0;
    }
    g_ce9_text[4] = 10;       /* byte offset to prog[5] (page 4 start) */
    g_ce9_text[5] = 0x66;     /* one glyph */
    g_ce9_text[6] = -1;       /* END */
    current_chapter_text = (uint32)g_ce9_text;

    /* runtime-char array: 16 local slots so char #11 (revive) is in bounds.
     * Seed char #11 with poisoned flags so the revive (flags -> 0) is visible;
     * char 0 stays zeroed (alive) for the real save scan. */
    g_ce9_saved_rc_ptr = data_fd2_battle_runtime_char_array_ptr;
    memset(g_ce9_rc, 0, sizeof(g_ce9_rc));
    g_ce9_rc[0xb].flags = 0x05;          /* dead|cannot_act -> handler clears */
    data_fd2_battle_runtime_char_array_ptr = g_ce9_rc;

    /* save safe env (chapter 01 baseline): zeroed roster, one scanned runtime
     * char and one template entry. */
    memset(g_ce9_roster, 0, sizeof(g_ce9_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce9_roster;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;

    /* portrait dump: skip the race scan (alloc_offset 0 -> no FDICON per-char
     * parse), give the FD2.TMP fwrite a real 0x32A00 source, re-read a valid
     * FDFIELD index (chapter 4 -> idx 0xE, proven by the rsrc suite). */
    if (g_ce9_psc == 0) {
        g_ce9_psc = (uint32)malloc(0x32a00);
    }
    portrait_sprite_cache = g_ce9_psc;
    chapter_portrait_load_buffer = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;

    /* cutscene event 0x24 -> empty (n_groups == 0) script. */
    g_ce9_script[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x24] = g_ce9_script;
    data_fd2_chapter_cutscene_event_state = 0;    /* normal compose path */

    /* bounded view-window pan (origin near the (6,1) target). */
    data_fd2_battle_view_window_origin_x = 6;
    data_fd2_battle_view_window_origin_y = 1;

    /* seed chapter id: 4 makes the FDFIELD re-read index (4*3+2 = 0xE) valid;
     * the +1 transition is then observable as 4 -> 5. */
    data_fd2_chapter_current_chapter_id = 4;
}

static void ce9_fixture_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_ce9_saved_rc_ptr;
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_chapter_cutscene_event_state = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x24] = 0;
    portrait_sprite_cache = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_current_chapter_id = 1;
    remove("FD2.TMP");
}

/* ----------------------------------------------------------------
 * End-to-end: the handler revives char #11 (flags 0x05 -> 0), pans the window,
 * refreshes the race-4 portrait set (real FDICON/FDFIELD read + FD2.TMP rewrite),
 * fires the (empty) cutscene event, runs dialog page 4 (its single glyph 0x66),
 * persists the party, and advances chapter_id 4 -> 5. The glyph recorder pins
 * that the real dialog VM ran on page 4 of current_chapter_text; FD2.TMP's
 * presence proves the real portrait dump completed.
 * ---------------------------------------------------------------- */
static void test_chapter_09_end_revives_char11_and_increments_id(void)
{
    int    glyph_calls;
    uint32 glyph_idx;
    uint8  char11_flags;
    uint32 chapter_id;
    FILE  *tmp_fp;
    int    tmp_present;

    ce9_fixture_reset();

    fd2_chapter_09_end();

    /* snapshot observables, then restore globals, then assert (so the teardown's
     * global-restore runs even if an assert early-returns). */
    char11_flags = g_ce9_rc[0xb].flags;
    glyph_calls  = g_dlg_glyph_calls;
    glyph_idx    = g_dlg_glyph_last_idx;
    chapter_id   = data_fd2_chapter_current_chapter_id;
    tmp_fp = fopen("FD2.TMP", "rb");
    tmp_present = (tmp_fp != NULL);
    if (tmp_fp != NULL) {
        fclose(tmp_fp);
    }
    ce9_fixture_teardown();

    /* char #11 revived: every status flag cleared. */
    ASSERT_EQ((long)char11_flags, 0L);

    /* dialog page 4 rendered exactly its glyph 0x66. */
    ASSERT_EQ((long)glyph_calls, 1);
    ASSERT_EQ((long)glyph_idx, (long)0x66);

    /* the real portrait dump rewrote FD2.TMP. */
    ASSERT_EQ((long)tmp_present, 1L);

    /* state transition: id incremented 4 -> 5 (relative, not absolute). */
    ASSERT_EQ((long)chapter_id, 5L);
}

/* ----------------------------------------------------------------
 * The chapter-id update is a relative INCREMENT, not an absolute set: seeded
 * with a distinctive unrelated value (7), the handler leaves 8 — proving it does
 * not hardcode the id. (The FDFIELD re-read index 7*3+2 = 0x17 is still a valid
 * FDFIELD entry, so the real portrait dump runs unchanged.)
 * ---------------------------------------------------------------- */
static void test_chapter_09_end_increments_not_absolute(void)
{
    uint32 chapter_id;

    ce9_fixture_reset();
    data_fd2_chapter_current_chapter_id = 7;   /* distinctive, unrelated to 5 */

    fd2_chapter_09_end();

    chapter_id = data_fd2_chapter_current_chapter_id;
    ce9_fixture_teardown();

    ASSERT_EQ((long)chapter_id, 8L);           /* 7 + 1, not a constant */
}

/* ================================================================
 * fd2_chapter_10_end @ 0x235F9
 *
 * The Chapter 10「洞窟中的激戰」end handler is a straight-line (no-branch)
 * cutscene orchestrator that restores and repositions the party after the cave
 * rescue:
 *   (1) copies the two 11-byte scene position tables (X / Y @ 0x52113 / 0x5211E)
 *       onto on-stack placement blocks,
 *   (2) fades the screen to black (real fd2_play_palette_fade_to_black) and
 *       clears every char's acted flag (real fd2_clear_all_chars_acted_flag),
 *   (3) places the 11 party units (chars 0..0xA) at those tiles, each with
 *       sprite_state[1] (facing) = 2 (the handler's own loop, INDEPENDENT of
 *       party_member_count),
 *   (4) revives/repositions the rescued NPCs that started the battle asleep or
 *       disabled: char 0x32 -> (15,35) sleep flag cleared; char 0x33 -> (14,35)
 *       sleep flag cleared; char 0x34 -> (16,35) flags cleared; char 5 flags
 *       cleared,
 *   (5) resets battle_anim_phase, sets the view-window origin and cursor-world
 *       to (9,34) and cursor-screen to (0,0),
 *   (6) composites one battle frame (real fd2_composite_battle_frame(1)), fades
 *       the palette back in (real fd2_play_palette_fade_in), delays 200 ticks,
 *   (7) shows dialog page 4 (real fd2_display_dialog_scene), resets
 *       battle_anim_phase, fires cutscene event 0x25 (real
 *       fd2_cutscene_event_trigger), shows dialog page 5,
 *   (8) persists the party (real fd2_save_runtime_char_to_template), recruits
 *       char 11 (索菲亞) then char 6 (萊汀) (real
 *       fd2_init_runtime_char_from_base_growth), then
 *   (9) advances chapter_id by 1.
 *
 * EVERY callee is the real linked function. The only recording seams (shared
 * with the compositor / dialog suites) are the testglob.c stubs already used
 * everywhere: the dialog-VM glyph blit, the fd2_composite_battle_tile_map proxy
 * (the first stage of every composite frame, a no-op counter), the __delay_thunk
 * recorder, and fd2_check_char_is_dead. The fixture mirrors the gfx/rndscene.c
 * composite-frame env (workspace at large_game_state_buffer + 0x8088, full
 * window, a sprite atlas for the cursor overlay) plus the chapter 08 dialog /
 * palette env, and swaps data_fd2_battle_runtime_char_array_ptr to a 64-slot
 * local array so indices 0x32/0x33/0x34 (and the 0..0xA placement loop) are in
 * bounds. party_member_count is 0 so the real fade/clear/save/composite-char
 * loops are clean no-ops, isolating the handler's own placement + revive writes.
 *
 * Asserted: chars 0..0xA placed from the X/Y tables with facing 2; the four
 * rescued NPCs revived/repositioned (poisoned sleep flags / flags / positions
 * overwritten to the handler's literals); both dialog pages ran in order (4 then
 * 5, via the glyph recorder); the view-window/cursor globals set to (9,34) and
 * (0,0) and battle_anim_phase ended 0; and chapter_id := prev+1 (a relative
 * increment, not absolute). On-screen pixels of the fade / composite / dialog /
 * cutscene are display side-effects deferred to Phase 9.
 * ================================================================ */

/* composite-frame recording seams (testglob.c). */
extern int    g_tile_map_calls;
extern int    g_composite_call_count;

/* CHEND10_WS_SPAN: the real fd2_blit_rectangle (composite finalizer) memmoves
 * the visible 312x192 region from workspace (src == large_game_state_buffer +
 * 0x8088) to VGA 0xA0504, reading (h-1)*sstride + w = 191*0x1c8 + 0x138 bytes
 * from the workspace. Back the workspace with exactly that span and offset the
 * base pointer back by 0x8088 (the gfx/rndscene.c compositor-test trick) so the
 * +0x8088 read window lands at the buffer start. */
#define CHEND10_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ce10_ws[CHEND10_WS_SPAN];

/* cursor-overlay sprite atlas: fd2_paint_cursor_overlay_pattern resolves cursor
 * sprites through data_fd2_runtime_battle_state_ptr (a sheet with a dword table
 * at +6 indexed by sprite_idx). An identity table keeps every resolved sprite
 * address valid so the overlay's blit reaches the recording passthrough stub. */
static uint8 g_ce10_atlas[6 + 64 * 4 + 4];

static uint8        g_ce10_palette[768];       /* fade source (768 = 0x100*3) */
static uint8        g_ce10_roster[8 * 0x50];   /* save-template target */
static int16        g_ce10_text[16];           /* dialog program (pages 4,5) */
static uint8        g_ce10_script[1];          /* cutscene 0x25: n_groups == 0 */
static runtime_char g_ce10_rc[64];             /* indices 0..0x34 in bounds */
static runtime_char *g_ce10_saved_rc_ptr;

static void ce10_fixture_reset(void)
{
    int i;
    uint32 *atlas_table;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    g_tile_map_calls = 0;
    g_composite_call_count = 0;

    /* dialog program: page 4 -> idx 8 (glyph 0x44 + END), page 5 -> idx 10
     * (glyph 0x55 + END). Distinct glyphs pin the page order (4 before 5). */
    for (i = 0; i < 16; i++) {
        g_ce10_text[i] = 0;
    }
    g_ce10_text[4]  = 16;     /* page 4 -> idx 8 */
    g_ce10_text[5]  = 20;     /* page 5 -> idx 10 */
    g_ce10_text[8]  = 0x44;   /* page 4 glyph */
    g_ce10_text[9]  = -1;     /* END */
    g_ce10_text[10] = 0x55;   /* page 5 glyph */
    g_ce10_text[11] = -1;     /* END */
    current_chapter_text = (uint32)g_ce10_text;

    /* runtime-char array: 64 local slots so the 0..0xA placement loop and the
     * 0x32/0x33/0x34/5 revives are in bounds. Poison the revive targets so the
     * handler's writes (sleep flag/flags -> 0, position -> literals) are visible,
     * and poison chars 0..0xA so the table-driven placement is observable. */
    g_ce10_saved_rc_ptr = data_fd2_battle_runtime_char_array_ptr;
    memset(g_ce10_rc, 0, sizeof(g_ce10_rc));
    for (i = 0; i < 0xb; i++) {
        g_ce10_rc[i].pos_x = 0xEE;
        g_ce10_rc[i].pos_y = 0xEE;
        g_ce10_rc[i].sprite_state[1] = 0xEE;
    }
    g_ce10_rc[0x32].pos_x = 0xEE; g_ce10_rc[0x32].pos_y = 0xEE;
    g_ce10_rc[0x32].status_sleep_flag = 100;   /* asleep -> handler clears */
    g_ce10_rc[0x33].pos_x = 0xEE; g_ce10_rc[0x33].pos_y = 0xEE;
    g_ce10_rc[0x33].status_sleep_flag = 100;   /* asleep -> handler clears */
    g_ce10_rc[0x34].pos_x = 0xEE; g_ce10_rc[0x34].pos_y = 0xEE;
    g_ce10_rc[0x34].flags = 0x05;              /* dead|cannot_act -> cleared */
    g_ce10_rc[5].flags = 0x05;                 /* dead|cannot_act -> cleared */
    data_fd2_battle_runtime_char_array_ptr = g_ce10_rc;

    /* save + clear-acted + composite-char loops are all gated by
     * party_member_count: 0 makes them clean no-ops (the handler's OWN placement
     * loop runs regardless). Recruits append at shared-menu slot 0. */
    memset(g_ce10_roster, 0, sizeof(g_ce10_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce10_roster;
    data_fd2_battle_party_member_count = 0;
    data_fd2_shared_menu_party_member_count = 0;
    g_check_char_is_dead_return = 0;

    /* cutscene event 0x25 -> empty (n_groups == 0) script; the interpreter still
     * runs one real fd2_composite_battle_frame(1) at its tail. */
    g_ce10_script[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x25] = g_ce10_script;
    data_fd2_chapter_cutscene_event_state = 0;   /* normal compose path */

    /* fade-to-black / fade-in palette source (real fd2_set_vga_palette_range
     * reads palette[idx*3 + 0..2] for idx 0..0xFF = 768 bytes). */
    memset(g_ce10_palette, 0, sizeof(g_ce10_palette));
    data_fd2_vga_palette_data_ptr = (uint32)g_ce10_palette;

    /* composite-frame env (gfx/rndscene.c compositor-test fixture): workspace at
     * large_game_state_buffer + 0x8088, a full window so the cursor overlay's
     * coords are in-window, and an identity sprite atlas for cursor resolution. */
    memset(g_ce10_ws, 0, sizeof(g_ce10_ws));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce10_ws - 0x8088u;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    atlas_table = (uint32 *)(g_ce10_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_table[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce10_atlas;

    /* poison the view/cursor globals so the handler's writes (9,34)/(0,0) and
     * the battle_anim_phase reset are observable. */
    data_fd2_battle_anim_phase = 0x55;
    data_fd2_battle_view_window_origin_x = 0xAA;
    data_fd2_battle_view_window_origin_y = 0xAA;
    data_fd2_battle_cursor_world_x = 0xAA;
    data_fd2_battle_cursor_world_y = 0xAA;
    data_fd2_battle_cursor_screen_x = 0xAA;
    data_fd2_battle_cursor_screen_y = 0xAA;

    /* preset chapter id so the +1 transition is observable. */
    data_fd2_chapter_current_chapter_id = 10;
}

static void ce10_fixture_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_ce10_saved_rc_ptr;
    current_chapter_text = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x25] = 0;
    data_fd2_chapter_cutscene_event_state = 0;
    data_fd2_vga_palette_data_ptr = 0;
    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;
    data_fd2_chapter_current_chapter_id = 1;
    g_check_char_is_dead_return = 0;
}

/* ----------------------------------------------------------------
 * End-to-end: the handler places chars 0..0xA from the scene tables (each
 * facing 2), revives/repositions the four rescued NPCs (chars 0x32/0x33 sleep
 * flag -> 0 at (15,35)/(14,35); char 0x34 flags -> 0 at (16,35); char 5 flags ->
 * 0), runs dialog pages 4 then 5 (glyphs 0x44 then 0x55), sets the view-window
 * and cursor to (9,34) and cursor-screen to (0,0) with battle_anim_phase 0, and
 * advances chapter_id 10 -> 11. The glyph recorder pins that the real dialog VM
 * ran on pages 4 and 5 of current_chapter_text in order.
 * ---------------------------------------------------------------- */
static void test_chapter_10_end_places_party_revives_npcs_and_increments(void)
{
    int    placement_ok;
    uint8  c32_x, c32_y, c32_sleep;
    uint8  c33_x, c33_y, c33_sleep;
    uint8  c34_x, c34_y, c34_flags;
    uint8  c5_flags;
    int    glyph_calls;
    uint32 glyph_idx;
    uint32 win_ox, win_oy, cur_wx, cur_wy, cur_sx, cur_sy;
    uint32 anim_phase;
    uint32 chapter_id;
    int    i;

    ce10_fixture_reset();

    fd2_chapter_10_end();

    /* snapshot observables, then restore globals, then assert. */
    placement_ok = 1;
    for (i = 0; i < 0xb; i++) {
        if (g_ce10_rc[i].pos_x != data_fd2_chapter_ch10_end_scene_char_pos_x_table[i] ||
            g_ce10_rc[i].pos_y != data_fd2_chapter_ch10_end_scene_char_pos_y_table[i] ||
            g_ce10_rc[i].sprite_state[1] != 2) {
            placement_ok = 0;
        }
    }
    c32_x = g_ce10_rc[0x32].pos_x; c32_y = g_ce10_rc[0x32].pos_y;
    c32_sleep = g_ce10_rc[0x32].status_sleep_flag;
    c33_x = g_ce10_rc[0x33].pos_x; c33_y = g_ce10_rc[0x33].pos_y;
    c33_sleep = g_ce10_rc[0x33].status_sleep_flag;
    c34_x = g_ce10_rc[0x34].pos_x; c34_y = g_ce10_rc[0x34].pos_y;
    c34_flags = g_ce10_rc[0x34].flags;
    c5_flags = g_ce10_rc[5].flags;
    glyph_calls = g_dlg_glyph_calls;
    glyph_idx   = g_dlg_glyph_last_idx;
    win_ox = data_fd2_battle_view_window_origin_x;
    win_oy = data_fd2_battle_view_window_origin_y;
    cur_wx = data_fd2_battle_cursor_world_x;
    cur_wy = data_fd2_battle_cursor_world_y;
    cur_sx = data_fd2_battle_cursor_screen_x;
    cur_sy = data_fd2_battle_cursor_screen_y;
    anim_phase = data_fd2_battle_anim_phase;
    chapter_id = data_fd2_chapter_current_chapter_id;
    ce10_fixture_teardown();

    /* chars 0..0xA placed from the X/Y tables, each facing 2. */
    ASSERT_EQ((long)placement_ok, 1L);

    /* rescued NPCs revived/repositioned to the handler's literals. */
    ASSERT_EQ((long)c32_x, (long)0xf);
    ASSERT_EQ((long)c32_y, (long)0x23);
    ASSERT_EQ((long)c32_sleep, 0L);
    ASSERT_EQ((long)c33_x, (long)0xe);
    ASSERT_EQ((long)c33_y, (long)0x23);
    ASSERT_EQ((long)c33_sleep, 0L);
    ASSERT_EQ((long)c34_x, (long)0x10);
    ASSERT_EQ((long)c34_y, (long)0x23);
    ASSERT_EQ((long)c34_flags, 0L);
    ASSERT_EQ((long)c5_flags, 0L);

    /* both dialog pages ran in order: page 4 (0x44) then page 5 (0x55). */
    ASSERT_EQ((long)glyph_calls, 2);
    ASSERT_EQ((long)glyph_idx, (long)0x55);

    /* view-window origin + cursor-world (9,34); cursor-screen (0,0); anim 0. */
    ASSERT_EQ((long)win_ox, 9L);
    ASSERT_EQ((long)win_oy, (long)0x22);
    ASSERT_EQ((long)cur_wx, 9L);
    ASSERT_EQ((long)cur_wy, (long)0x22);
    ASSERT_EQ((long)cur_sx, 0L);
    ASSERT_EQ((long)cur_sy, 0L);
    ASSERT_EQ((long)anim_phase, 0L);

    /* state transition: id incremented 10 -> 11 (relative, not absolute). */
    ASSERT_EQ((long)chapter_id, 11L);
}

/* ----------------------------------------------------------------
 * The chapter-id update is a relative INCREMENT, not an absolute set: seeded
 * with a distinctive unrelated value (7), the handler leaves 8 — proving it does
 * not hardcode the id. (The placement / revives / cutscene are unconditional and
 * run identically regardless of the seed.)
 * ---------------------------------------------------------------- */
static void test_chapter_10_end_increments_not_absolute(void)
{
    uint32 chapter_id;

    ce10_fixture_reset();
    data_fd2_chapter_current_chapter_id = 7;   /* distinctive, unrelated to 11 */

    fd2_chapter_10_end();

    chapter_id = data_fd2_chapter_current_chapter_id;
    ce10_fixture_teardown();

    ASSERT_EQ((long)chapter_id, 8L);           /* 7 + 1, not a constant */
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
    RUN_TEST(test_chapter_04_end_runs_dialog_page4_and_increments_id);
    RUN_TEST(test_chapter_04_end_increments_not_absolute);
    RUN_TEST(test_chapter_05_end_stages_scene_and_recruits);
    RUN_TEST(test_chapter_05_end_increments_not_absolute);
    RUN_TEST(test_chapter_06_end_recruits_loads_and_increments_id);
    RUN_TEST(test_chapter_06_end_increments_not_absolute);
    RUN_TEST(test_chapter_07_end_flag_and_alive_recruits);
    RUN_TEST(test_chapter_07_end_flag_set_but_dead_no_recruit);
    RUN_TEST(test_chapter_07_end_flag_clear_short_circuits);
    RUN_TEST(test_chapter_08_end_stages_scene_cutscene_and_recruits);
    RUN_TEST(test_chapter_08_end_increments_not_absolute);
    RUN_TEST(test_chapter_09_end_revives_char11_and_increments_id);
    RUN_TEST(test_chapter_09_end_increments_not_absolute);
    RUN_TEST(test_chapter_10_end_places_party_revives_npcs_and_increments);
    RUN_TEST(test_chapter_10_end_increments_not_absolute);
    printf("\n");
}
