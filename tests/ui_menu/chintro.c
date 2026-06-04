/*
 * unit tests for src/ui_menu/chintro.c
 *
 * fd2_chapter_intro_menu_input_loop @ 0x2D7BD — chapter-intro 4-way menu
 * input loop. Returns 1 on commit (Enter 0x1C / Space 0x39), -1 on cancel
 * (Esc 0x01); Left (0x4B) / Right (0x4D) move data_fd2_ui_menu_cursor_idx
 * with signed wrap-around over 0..3 and loop back (no return).
 *
 * The loop drives the REAL fd2_wait_input_with_chapter_dialog_blink(0)
 * (src/input/input.c), which reads scancodes via INT 16h fn 0x10 from the
 * BIOS keyboard ring (head 0x41A / tail 0x41C / words at 0x41E..). We
 * pre-fill the ring with a scancode sequence so each loop iteration
 * consumes one key (same mechanism as tests/dialog/dialog.c's typewriter
 * loop tests). With the buffer non-empty at entry and the BIOS tick @ 0x46C
 * held stable, the wait function's per-frame blink/render body is skipped
 * (tick delta 0), so fd2_paint_portrait_to_dialog_area is never reached and
 * only the corner-sprite blit (mode==0) runs against a zeroed fake atlas.
 * fd2_play_sfx_with_handle is the testglob stub: g_play_sfx_with_handle_calls
 * counts the cursor-chime, g_sfx_last_id captures its id (always 0 here).
 *
 * fd2_run_chapter_intro_menu_main @ 0x2E341 is intentionally NOT unit-tested
 * here: behavioral coverage is DEFERRED to Phase 9 integration. It is a pure
 * display/input orchestrator whose only computation (the start-game return
 * flag and the inline pose-out scale arithmetic) is reachable only by running
 * the whole function, which (a) fopens real game files FDOTHER.DAT / DATO.DAT
 * via fd2_load_dat_resource + fd2_load_chapter_portrait + fd2_display_dialog_scene,
 * (b) drives the 4-way input loop and dispatches into the four heavy
 * interactive sub-menus (buy/sell/equip/give), and (c) performs VGA DAC port
 * I/O (fd2_set_vga_palette_range -> outp) plus writes to physical VGA memory
 * at 0xA0000 in the 11-frame exit animation. Those hardware/file/input
 * dependencies make a meaningful assertion only possible at the scripted
 * gameplay level. Correctness was established by three-source review (plate /
 * disassembly / decompiler), including verification of every CALL-then-EAX use
 * against the assembly and every branch constant (FDOTHER idx, greeting ids
 * 0x1F5/0x1F7/0x1B8, decimal x=0x1F, dispatch order, return flag).
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern int g_play_sfx_with_handle_calls;
extern int g_sfx_last_id;

static uint8 g_ci_atlas[256];

/* Prime the wait-input prerequisites: a zeroed corner-sprite atlas (so the
 * mode==0 blit loop reads in-bounds), a stable BIOS tick (so the per-frame
 * blink body is skipped), and a reset SFX call counter. */
static void ci_prep(void)
{
    memset(g_ci_atlas, 0, sizeof(g_ci_atlas));
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_ci_atlas;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint32 *)0x46CuL = 0x00000100uL;   /* stable tick */
    g_play_sfx_with_handle_calls = 0;
    g_sfx_last_id = -1;
}

/* Queue one scancode (high byte = INT 16h AH) into the BIOS keyboard ring. */
static void ci_queue1(uint16 sc)
{
    *(volatile uint16 *)0x41AuL = 0x1E;             /* head */
    *(volatile uint16 *)0x41CuL = 0x20;             /* tail = head + 2 (1 key) */
    *(volatile uint16 *)0x41EuL = (uint16)(sc << 8);
}

/* Queue two scancodes; the loop consumes [sc0] then [sc1] across iterations. */
static void ci_queue2(uint16 sc0, uint16 sc1)
{
    *(volatile uint16 *)0x41AuL = 0x1E;             /* head */
    *(volatile uint16 *)0x41CuL = 0x22;             /* tail = head + 4 (2 keys) */
    *(volatile uint16 *)0x41EuL = (uint16)(sc0 << 8);
    *(volatile uint16 *)0x420uL = (uint16)(sc1 << 8);
}


/* ---- commit / cancel return values ---- */

/* Enter (0x1C) commits immediately: returns 1, no cursor change, no SFX. */
static void test_chintro_enter_commits(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 2;
    ci_queue1(0x1C);
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);  /* unchanged */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);       /* commit plays no chime */
}

/* Space (0x39) is the second commit scancode: returns 1. */
static void test_chintro_space_commits(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 0;
    ci_queue1(0x39);
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, 1);
}

/* Esc (0x01) cancels: returns -1. */
static void test_chintro_esc_cancels(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 1;
    ci_queue1(0x01);
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);  /* unchanged */
}


/* ---- Left (0x4B): cursor-- with signed wrap < 0 -> 3 ---- */

/* No-wrap: cursor 2 + Left -> 1, plays the chime (id 0), then loops; the
 * queued Enter commits. Pins both the decrement and the loop-back. */
static void test_chintro_left_no_wrap(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 2;
    ci_queue2(0x4B, 0x1C);
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ(g_sfx_last_id, 0);                       /* cursor-move chime */
}

/* Wrap: cursor 0 + Left -> -1 -> 3 (the signed JGE branch; an unsigned
 * compare would leave it at 0xFFFFFFFF). */
static void test_chintro_left_wraps_to_3(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 0;
    ci_queue2(0x4B, 0x1C);
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 3);
}


/* ---- Right (0x4D): cursor++ with signed wrap > 3 -> 0 ---- */

/* No-wrap: cursor 1 + Right -> 2, chime, then loops; queued Enter commits. */
static void test_chintro_right_no_wrap(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 1;
    ci_queue2(0x4D, 0x1C);
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ(g_sfx_last_id, 0);
}

/* Wrap: cursor 3 + Right -> 4 -> 0. */
static void test_chintro_right_wraps_to_0(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 3;
    ci_queue2(0x4D, 0x1C);
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
}


/* ---- loop-back behavior ---- */

/* An unmapped scancode (0x10) falls through every branch, leaving result==0,
 * so the loop continues; the queued Enter then commits. Cursor untouched and
 * no chime fired for the unmapped key. */
static void test_chintro_unmapped_loops_then_commit(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 2;
    ci_queue2(0x10, 0x1C);
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);  /* unmapped: no change */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);       /* unmapped: no chime */
}

/* Two consecutive Lefts then Esc: 1 -> 0 -> wrap 3, two chimes, then cancel.
 * Exercises multiple loop iterations with state carried across them. */
static void test_chintro_two_lefts_then_cancel(void)
{
    int r;
    ci_prep();
    data_fd2_ui_menu_cursor_idx = 1;
    /* three keys: Left, Left, Esc; tail = head + 6 */
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x24;
    *(volatile uint16 *)0x41EuL = 0x4B00;   /* Left: 1 -> 0 */
    *(volatile uint16 *)0x420uL = 0x4B00;   /* Left: 0 -> wrap 3 */
    *(volatile uint16 *)0x422uL = 0x0100;   /* Esc */
    r = fd2_chapter_intro_menu_input_loop();
    ASSERT_EQ(r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 3);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 2);       /* one chime per Left */
}


void run_ui_menu_chintro_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/chintro\n");
    RUN_TEST(test_chintro_enter_commits);
    RUN_TEST(test_chintro_space_commits);
    RUN_TEST(test_chintro_esc_cancels);
    RUN_TEST(test_chintro_left_no_wrap);
    RUN_TEST(test_chintro_left_wraps_to_3);
    RUN_TEST(test_chintro_right_no_wrap);
    RUN_TEST(test_chintro_right_wraps_to_0);
    RUN_TEST(test_chintro_unmapped_loops_then_commit);
    RUN_TEST(test_chintro_two_lefts_then_cancel);
    printf("\n");
}
