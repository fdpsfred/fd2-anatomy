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

/* fd2_render_party_roster_grid recording stub (testglob.c): captures the last
 * (highlight_idx, surface_offset) and a call count. */
extern int    g_roster_grid_calls;
extern uint32 g_roster_grid_last_highlight;
extern uint32 g_roster_grid_last_surface;
/* scroll-page animation recording stubs (testglob.c). */
extern int g_scroll_up_in_shop_calls;
extern int g_scroll_down_in_shop_calls;

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


/* ================================================================
 * fd2_party_roster_single_select_loop @ 0x2E6B8 — party-roster member
 * select grid loop. Returns 1 on commit (Enter 0x1C / Space 0x39), -1 on
 * cancel (Esc 0x01). The four arrows move data_fd2_ui_menu_cursor_idx within
 * 0..count-1 (count = data_fd2_shared_menu_party_member_count): Right +1 /
 * Left -1 / Down +2 / Up -2, each with a bound guard (no move past the edge),
 * the cursor chime (SFX id 0), the 6-item viewport paging in steps of 2
 * (scroll up when cursor - scroll_offset > 5; scroll down when cursor <
 * scroll_offset), and a grid re-render to 0xA0000.
 *
 * The loop drives the REAL fd2_wait_input_with_chapter_dialog_blink(3) exactly
 * as the chapter-intro tests above: the BIOS keyboard ring is pre-filled and
 * the tick @ 0x46C held stable so the per-frame blink body is skipped. The
 * setup phase runs for real — 3 x malloc(64000), a 0xA0000 framebuffer
 * snapshot memmove, and the 6-frame real fd2_slide_panel_down_step
 * (src/anim/aniwalk.c) which composites into VGA (0xA0000) — all harmless in
 * the DOS test target. fd2_dialog_sprite_blit_normal and
 * fd2_render_party_roster_grid are the testglob recording stubs; the header
 * blit only requires atlas[+0x46] to be a readable dword, so ps_prep points
 * the atlas at a zeroed 256-byte buffer. The scroll-page animations are the
 * recording stubs g_scroll_up_in_shop_calls / g_scroll_down_in_shop_calls.
 *
 * Each test pins: the return code, the final cursor index, the final
 * scroll_offset, the cursor-chime count, the last re-render highlight arg, and
 * the per-direction scroll-animation counts. These cover every navigation
 * branch, both signed bound guards, and both viewport-page transitions —
 * the risk-bearing arithmetic of the function. (The setup-phase blit/slide
 * pixel effects are pure display side-effects, verified by three-source
 * review and deferred to Phase 9 integration.)
 * ================================================================ */

/* Prime the roster-select prerequisites: a zeroed sprite atlas (so the header
 * blit's atlas[+0x46] dword read is in-bounds), a stable BIOS tick (so the
 * wait-input per-frame blink body is skipped), reset cursor/scroll, and reset
 * every observed counter. */
static void ps_prep(uint32 member_count, uint32 start_cursor,
                    uint32 start_scroll)
{
    memset(g_ci_atlas, 0, sizeof(g_ci_atlas));
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_ci_atlas;
    data_fd2_shared_menu_party_member_count = member_count;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint32 *)0x46CuL = 0x00000100uL;   /* stable tick */
    g_play_sfx_with_handle_calls = 0;
    g_sfx_last_id = -1;
    g_roster_grid_calls = 0;
    g_roster_grid_last_highlight = 0xFFFFFFFFuL;
    g_roster_grid_last_surface = 0;
    g_scroll_up_in_shop_calls = 0;
    g_scroll_down_in_shop_calls = 0;
    /* the loop sets these two to 0 at setup, but seed them so a no-op setup
     * would be caught; the function overwrites both before the input loop. */
    data_fd2_ui_menu_cursor_idx = start_cursor;
    data_fd2_ui_menu_scroll_offset = start_scroll;
}

/* Fill the BIOS keyboard ring with n scancodes (high byte = INT 16h AH). */
static void ps_queue(const uint16 *scancodes, int n)
{
    int i;
    *(volatile uint16 *)0x41AuL = 0x1E;                 /* head */
    *(volatile uint16 *)0x41CuL = (uint16)(0x1E + n * 2); /* tail = head + 2n */
    for (i = 0; i < n; i++) {
        *(volatile uint16 *)(0x41EuL + (uint32)i * 2) =
            (uint16)(scancodes[i] << 8);
    }
}


/* ---- setup resets cursor/scroll, then commit/cancel ---- */

/* Enter (0x1C) commits immediately. The setup must have reset cursor and
 * scroll to 0 (ps_prep seeded them non-zero), and rendered the initial grid
 * with highlight 0; no input-loop navigation, so no chime and no scroll. */
static void test_ps_enter_commits_after_setup(void)
{
    uint16 keys[1];
    int r;
    ps_prep(8, 5, 4);
    keys[0] = 0x1C;
    ps_queue(keys, 1);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);     /* setup reset */
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 0);  /* setup reset */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);          /* commit: no chime */
    ASSERT_TRUE(g_roster_grid_calls >= 1);               /* initial render */
    ASSERT_EQ((long)g_roster_grid_last_highlight, 0);    /* initial grid @ 0 */
}

/* Space (0x39) is the second commit scancode. */
static void test_ps_space_commits(void)
{
    uint16 keys[1];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x39;
    ps_queue(keys, 1);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
}

/* Esc (0x01) cancels: returns -1. */
static void test_ps_esc_cancels(void)
{
    uint16 keys[1];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x01;
    ps_queue(keys, 1);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, -1);
}


/* ---- Right (0x4D): cursor++, guard at count-1 ---- */

/* Right then commit: cursor 0 -> 1, chime, re-render highlight 1, no scroll
 * (1 - 0 = 1, not > 5). */
static void test_ps_right_increments(void)
{
    uint16 keys[2];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x4D; keys[1] = 0x1C;
    ps_queue(keys, 2);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ(g_sfx_last_id, 0);
    ASSERT_EQ((long)g_roster_grid_last_highlight, 1);    /* re-render @ 1 */
    ASSERT_EQ(g_scroll_up_in_shop_calls, 0);
}

/* Right at the last index is a no-op. The setup resets cursor to 0, so we
 * drive to the last index (7) with 7 Rights, then an 8th Right is guarded out
 * (count-1 == cursor). Final cursor 7; exactly 7 chimes (the guarded 8th fires
 * none); the boundary crossing 5->6 paged the viewport once. Then Esc exits. */
static void test_ps_right_blocked_at_last(void)
{
    uint16 keys[9];
    int r;
    int i;
    ps_prep(8, 0, 0);
    for (i = 0; i < 7; i++) {
        keys[i] = 0x4D;            /* 0->1->2->3->4->5->6->7 */
    }
    keys[7] = 0x4D;                /* 7: guarded (count-1 == cursor) */
    keys[8] = 0x01;
    ps_queue(keys, 9);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 7);  /* stuck at last */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 7);       /* 8th fired none */
    /* count 8, scroll steps when cursor-scroll>5: 6-0=6 -> scroll 2;
     * then 7-2=5 (not >5). So exactly one scroll-up page. */
    ASSERT_EQ(g_scroll_up_in_shop_calls, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 2);
}

/* count == 1: cursor starts 0 (= count-1), so Right is always guarded out
 * (no move, no chime). Verifies the count-1 == cursor edge directly. */
static void test_ps_right_guard_single_member(void)
{
    uint16 keys[2];
    int r;
    ps_prep(1, 0, 0);
    keys[0] = 0x4D; keys[1] = 0x01;
    ps_queue(keys, 2);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);  /* never moved */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);       /* guarded: no chime */
    ASSERT_EQ(g_scroll_up_in_shop_calls, 0);
}


/* ---- Left (0x4B): cursor--, guard at 0 ---- */

/* Left at cursor 0 (setup-reset) is guarded out: no move, no chime. */
static void test_ps_left_guard_at_zero(void)
{
    uint16 keys[2];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x4B; keys[1] = 0x01;
    ps_queue(keys, 2);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);  /* guarded */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_scroll_down_in_shop_calls, 0);
}

/* Right then Left returns to 0: 0->1 (chime), 1->0 (chime); two chimes, final
 * cursor 0, no scroll (scroll stays 0; 0 < 0 false). */
static void test_ps_right_then_left(void)
{
    uint16 keys[3];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x4D; keys[1] = 0x4B; keys[2] = 0x1C;
    ps_queue(keys, 3);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 2);
    ASSERT_EQ((long)g_roster_grid_last_highlight, 0);
    ASSERT_EQ(g_scroll_down_in_shop_calls, 0);
}


/* ---- Down (0x50): cursor += 2, guard at count-2 ---- */

/* Down moves by 2: count 8, cursor 0 -> 2, chime, re-render @ 2, no scroll
 * (2 - 0 = 2, not > 5). */
static void test_ps_down_adds_two(void)
{
    uint16 keys[2];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x50; keys[1] = 0x1C;
    ps_queue(keys, 2);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_roster_grid_last_highlight, 2);
}

/* Down guard: count 8, cursor 6 (= count-2) -> Down blocked (6 < 8-2=6 is
 * false). Reach cursor 6 via three Downs (0->2->4->6), then a fourth Down is
 * guarded, then Esc. Three chimes total. */
static void test_ps_down_guard_at_count_minus_two(void)
{
    uint16 keys[5];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x50; keys[1] = 0x50; keys[2] = 0x50;  /* 0->2->4->6 */
    keys[3] = 0x50;                                   /* 6: guarded */
    keys[4] = 0x01;
    ps_queue(keys, 5);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 6);  /* stuck at count-2 */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 3);       /* only 3 moves fired */
}


/* ---- Up (0x48): cursor -= 2, guard at cursor > 1 ---- */

/* Up moves by 2: drive to cursor 4 (two Downs), then Up -> 2, then commit.
 * The Up's guard is (cursor > 1); 4 > 1 true. Three chimes. */
static void test_ps_up_subtracts_two(void)
{
    uint16 keys[4];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x50; keys[1] = 0x50;  /* 0->2->4 */
    keys[2] = 0x48;                   /* 4->2 */
    keys[3] = 0x1C;
    ps_queue(keys, 4);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 3);
    ASSERT_EQ((long)g_roster_grid_last_highlight, 2);
}

/* Up guard: cursor 1 -> Up blocked (1 > 1 false). Drive to cursor 1 via one
 * Right, then Up (guarded), then Esc. Only the Right fired a chime. */
static void test_ps_up_guard_at_one(void)
{
    uint16 keys[3];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x4D;   /* 0->1 */
    keys[1] = 0x48;   /* 1: guarded (1 > 1 false) */
    keys[2] = 0x01;
    ps_queue(keys, 3);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);  /* Up did not move */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);       /* only the Right */
}


/* ---- viewport paging (the scroll arithmetic) ---- */

/* Scroll-UP page: count 12. Two Downs from 0: 0->2 (2-0=2, no page), 2->4
 * (4-0=4, no page); a Right 4->5 (5-0=5, not >5, no page); a Right 5->6
 * (6-0=6 > 5 -> scroll += 2 = 2, animate up). Final cursor 6, scroll 2, one
 * scroll-up animation, zero scroll-down. */
static void test_ps_scroll_up_page_on_right(void)
{
    uint16 keys[5];
    int r;
    ps_prep(12, 0, 0);
    keys[0] = 0x50; keys[1] = 0x50;   /* 0->2->4 */
    keys[2] = 0x4D; keys[3] = 0x4D;   /* 4->5->6 (page at 6) */
    keys[4] = 0x1C;
    ps_queue(keys, 5);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 6);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 2);
    ASSERT_EQ(g_scroll_up_in_shop_calls, 1);
    ASSERT_EQ(g_scroll_down_in_shop_calls, 0);
    ASSERT_EQ((long)g_roster_grid_last_highlight, 6);
}

/* Scroll-UP via Down crossing the boundary: count 12. Downs 0->2->4->6: at 6,
 * 6-0=6 > 5 -> scroll=2, animate up. Continue 6->8: 8-2=6 > 5 -> scroll=4,
 * animate up again. Two scroll-up animations; final cursor 8, scroll 4. */
static void test_ps_scroll_up_twice_on_down(void)
{
    uint16 keys[5];
    int r;
    ps_prep(12, 0, 0);
    keys[0] = 0x50; keys[1] = 0x50;   /* 0->2->4 */
    keys[2] = 0x50;                    /* 4->6: scroll 0->2 */
    keys[3] = 0x50;                    /* 6->8: scroll 2->4 */
    keys[4] = 0x1C;
    ps_queue(keys, 5);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 8);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 4);
    ASSERT_EQ(g_scroll_up_in_shop_calls, 2);
    ASSERT_EQ(g_scroll_down_in_shop_calls, 0);
}

/* Scroll-DOWN page: count 12. Build cursor 8 / scroll 4 (four Downs as above),
 * then Left 8->7 (7 < 4 false, no page), Left 7->6 (6 < 4 false), ..., Left
 * 4->3 (3 < 4 -> scroll -= 2 = 2, animate down). Final cursor 3, scroll 2,
 * one scroll-down, two scroll-ups (from the build-up). */
static void test_ps_scroll_down_page_on_left(void)
{
    uint16 keys[10];
    int r;
    ps_prep(12, 0, 0);
    keys[0] = 0x50; keys[1] = 0x50; keys[2] = 0x50; keys[3] = 0x50; /* ->8, scroll 4 */
    keys[4] = 0x4B; keys[5] = 0x4B; keys[6] = 0x4B; keys[7] = 0x4B; /* 8->7->6->5->4 */
    keys[8] = 0x4B;   /* 4->3: 3 < 4 -> page down, scroll 4->2 */
    keys[9] = 0x1C;
    ps_queue(keys, 10);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 3);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 2);
    ASSERT_EQ(g_scroll_up_in_shop_calls, 2);    /* from the 4 Downs */
    ASSERT_EQ(g_scroll_down_in_shop_calls, 1);  /* the boundary-crossing Left */
    ASSERT_EQ((long)g_roster_grid_last_highlight, 3);
}

/* Scroll-DOWN via Up crossing the boundary: count 12. Build cursor 8 / scroll
 * 4, then Up 8->6 (6 < 4 false), Up 6->4 (4 < 4 false), Up 4->2 (2 < 4 ->
 * scroll -= 2 = 2, animate down). Final cursor 2, scroll 2, one scroll-down. */
static void test_ps_scroll_down_on_up(void)
{
    uint16 keys[8];
    int r;
    ps_prep(12, 0, 0);
    keys[0] = 0x50; keys[1] = 0x50; keys[2] = 0x50; keys[3] = 0x50; /* ->8, scroll 4 */
    keys[4] = 0x48; keys[5] = 0x48;   /* 8->6->4 */
    keys[6] = 0x48;                    /* 4->2: 2 < 4 -> page down, scroll 4->2 */
    keys[7] = 0x1C;
    ps_queue(keys, 8);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 2);
    ASSERT_EQ(g_scroll_down_in_shop_calls, 1);
}

/* Unmapped scancode falls through every branch (result stays 0) and the loop
 * continues; the queued Enter then commits. Cursor untouched, no chime. */
static void test_ps_unmapped_loops_then_commit(void)
{
    uint16 keys[2];
    int r;
    ps_prep(8, 0, 0);
    keys[0] = 0x10; keys[1] = 0x1C;
    ps_queue(keys, 2);
    r = fd2_party_roster_single_select_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);  /* unmapped: no change */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
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
    RUN_TEST(test_ps_enter_commits_after_setup);
    RUN_TEST(test_ps_space_commits);
    RUN_TEST(test_ps_esc_cancels);
    RUN_TEST(test_ps_right_increments);
    RUN_TEST(test_ps_right_blocked_at_last);
    RUN_TEST(test_ps_right_guard_single_member);
    RUN_TEST(test_ps_left_guard_at_zero);
    RUN_TEST(test_ps_right_then_left);
    RUN_TEST(test_ps_down_adds_two);
    RUN_TEST(test_ps_down_guard_at_count_minus_two);
    RUN_TEST(test_ps_up_subtracts_two);
    RUN_TEST(test_ps_up_guard_at_one);
    RUN_TEST(test_ps_scroll_up_page_on_right);
    RUN_TEST(test_ps_scroll_up_twice_on_down);
    RUN_TEST(test_ps_scroll_down_page_on_left);
    RUN_TEST(test_ps_scroll_down_on_up);
    RUN_TEST(test_ps_unmapped_loops_then_commit);
    printf("\n");
}
