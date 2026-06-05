/*
 * unit tests for src/ui_menu/promote.c
 *
 * fd2_build_dead_chars_list_for_revive(out) walks party slots
 * 0..menu_party_member_count-1, and for each slot whose
 * fd2_check_char_is_dead() returns 1 appends the slot index (as a
 * byte) to out, returning the count of dead chars.
 *
 * These tests drive the real per-char dead semantics by enabling
 * the testglob stub's array mode (g_check_char_is_dead_use_array),
 * which makes fd2_check_char_is_dead read
 * runtime_char[idx].flags & 1 from the shared g_test_rc_array
 * fixture — exactly what the real callee does.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* shared runtime_char fixture + the dead-check stub controls (testglob.c) */
extern runtime_char g_test_rc_array[8];
extern int g_check_char_is_dead_return;
extern int g_check_char_is_dead_use_array;

/* promote/revive candidate-picker callee spies (testglob.c) */
extern int    g_promote_grid_calls;
extern uint32 g_promote_grid_last_count;
extern uint32 g_promote_grid_last_dst;
extern uint32 g_promote_grid_last_cursor;
extern int    g_promote_grid_last_list;
extern int    g_promote_scroll_down_calls;
extern int    g_promote_scroll_up_calls;

/* Inject one keystroke into the BIOS keyboard buffer (BDA @ 0x400) so the real
 * fd2_wait_input_with_chapter_dialog_blink() exits its busy-wait on the first
 * poll and INT 16h fn 10h returns `scancode` in AH. head != tail makes the
 * buffer non-empty; the head word @ 0x41E carries scancode (high)/ASCII (low).
 * Mirrors tests/input/input.c and tests/ui_menu/status.c. */
static void kbd_inject_scancode(int scancode)
{
    *(volatile uint16 *)0x41AuL = 0x1E;                       /* head        */
    *(volatile uint16 *)0x41CuL = 0x20;                       /* tail=head+2 */
    *(volatile uint16 *)0x41EuL = (uint16)((scancode << 8) & 0xFF00);
}

/* Stand up the minimum fixture the select loop's setup + one real input frame
 * need: a zeroed 256-byte sprite atlas (the setup blit derefs *(int*)(atlas+0x46)
 * and the real fd2_wait_input_with_chapter_dialog_blink reads it; both stay
 * in-bounds at 0), rng seed reset for determinism, cursor/scroll cleared, and
 * the callee spies zeroed. The blit + grid renderer are recording no-op stubs;
 * the 6 panel-slide steps and the input wait run for real against malloc'd
 * workspaces and the VGA aperture (harmless under DOS/4GW). */
static uint8 g_promote_atlas[256];

static void promote_loop_setup(void)
{
    memset(g_promote_atlas, 0, sizeof(g_promote_atlas));
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_promote_atlas;
    data_fd2_shared_rng_seed = 0;
    data_fd2_ui_menu_cursor_idx = 0xDEAD;        /* poisoned: loop must zero it */
    data_fd2_ui_menu_scroll_offset = 0xBEEF;     /* poisoned: loop must zero it */
    g_promote_grid_calls = 0;
    g_promote_grid_last_count = 0;
    g_promote_grid_last_dst = 0;
    g_promote_grid_last_cursor = 0;
    g_promote_grid_last_list = 0;
    g_promote_scroll_down_calls = 0;
    g_promote_scroll_up_calls = 0;
}

/* Seed the fixture: party of `n` members, those whose index is set in
 * dead_mask get flags bit0 (dead); all others cleared (alive). Other
 * flag bits are deliberately set on a couple of slots to prove the
 * dead test isolates bit0. */
static void seed_party(int n, unsigned dead_mask)
{
    int i;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (i = 0; i < n; i++) {
        if (dead_mask & (1u << i))
            g_test_rc_array[i].flags = 0x05;   /* bit0 dead + bit2 cannot_act */
        else
            g_test_rc_array[i].flags = 0x06;   /* bit2 set, bit0 clear -> alive */
    }
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_shared_menu_party_member_count = (uint32)n;
    g_check_char_is_dead_use_array = 1;
}

/* ---- Test: mixed pattern -> dead indices appended in order ---- */
static void test_mixed_dead_pattern(void)
{
    uint8 out[8];
    int count;

    memset(out, 0xAA, sizeof(out));      /* canary */
    seed_party(6, (1u << 1) | (1u << 3) | (1u << 4));  /* dead: 1,3,4 */

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 3);
    ASSERT_EQ(out[0], 1);
    ASSERT_EQ(out[1], 3);
    ASSERT_EQ(out[2], 4);
    ASSERT_EQ(out[3], 0xAA);             /* nothing written past count */
}

/* ---- Test: all alive -> count 0, buffer untouched ---- */
static void test_all_alive(void)
{
    uint8 out[8];
    int count;

    memset(out, 0xAA, sizeof(out));
    seed_party(5, 0u);                   /* none dead */

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 0);
    ASSERT_EQ(out[0], 0xAA);             /* no write at all */
}

/* ---- Test: all dead -> every index appended in order ---- */
static void test_all_dead(void)
{
    uint8 out[8];
    int count;

    seed_party(4, 0xFu);                 /* 0,1,2,3 all dead */

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 4);
    ASSERT_EQ(out[0], 0);
    ASSERT_EQ(out[1], 1);
    ASSERT_EQ(out[2], 2);
    ASSERT_EQ(out[3], 3);
}

/* ---- Test: empty party -> loop never runs, returns 0 ---- */
static void test_empty_party(void)
{
    uint8 out[8];
    int count;

    memset(out, 0xAA, sizeof(out));
    seed_party(0, 0u);                   /* member_count = 0 */

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 0);
    ASSERT_EQ(out[0], 0xAA);
}

/* ---- Test: bit0 isolation — flags with bit0 clear are NOT dead even
 *           when other bits are set; the trailing dead char is the only
 *           one appended. Proves the (& 1) mask + (== 1) compare chain. */
static void test_bit0_isolation(void)
{
    uint8 out[8];
    int count;

    memset(out, 0xAA, sizeof(out));
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0xFE;     /* every bit but bit0 -> alive */
    g_test_rc_array[1].flags = 0x80;     /* bit7 acted, bit0 clear -> alive */
    g_test_rc_array[2].flags = 0x01;     /* bit0 only -> dead */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_shared_menu_party_member_count = 3;
    g_check_char_is_dead_use_array = 1;

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 1);
    ASSERT_EQ(out[0], 2);
    ASSERT_EQ(out[1], 0xAA);
}

/* ----------------------------------------------------------------
 * fd2_promote_members_select_loop @ 0x30C22 — input-loop dispatch tests.
 *
 * Each case runs the FULL setup (3 workspace mallocs, VRAM snapshot/clone,
 * recording blit + grid stubs, 6 REAL fd2_slide_panel_down_step frames) then a
 * single REAL fd2_wait_input_with_chapter_dialog_blink(2) frame whose injected
 * scancode terminates the do/while on the first iteration. They pin the
 * scancode -> return-value dispatch (the EAX-from-CALL mapping that is the
 * EAX-tracking-prone risk here: Enter/Space => 1, Esc => -1) and prove the
 * setup zeroes cursor + scroll_offset and renders the grid exactly once before
 * the loop. The Up/Down navigation arithmetic (cursor clamp + 3-item viewport
 * scroll transitions) re-renders and loops again, so it needs a second key the
 * host BIOS buffer cannot async-refill mid-loop; that path is deferred to
 * Phase 9 integration (the same deferral the codebase applies to every
 * input-loop-released menu). The non-execution of those branches here is
 * asserted via the scroll-animator spies staying at 0. */
static uint8 g_promote_list[4] = { 2, 5, 7, 9 };

/* Enter (0x1C) on the first frame commits -> returns 1. */
static void test_select_loop_enter_commits(void)
{
    int r;

    promote_loop_setup();
    kbd_inject_scancode(0x1c);

    r = fd2_promote_members_select_loop(4, g_promote_list);

    ASSERT_EQ((long)r, 1);
    /* setup zeroed the poisoned cursor/scroll before the loop */
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 0);
    /* candidate list + count stashed for re-render */
    ASSERT_EQ((long)data_fd2_ui_menu_candidate_array_ptr, (long)(uint32)g_promote_list);
    ASSERT_EQ((long)data_fd2_ui_menu_visible_item_count, 4);
    /* grid rendered exactly once (setup), with cursor 0 and the real list */
    ASSERT_EQ((long)g_promote_grid_calls, 1);
    ASSERT_EQ((long)g_promote_grid_last_count, 4);
    ASSERT_EQ((long)g_promote_grid_last_cursor, 0);
    ASSERT_EQ((long)g_promote_grid_last_list, (long)(int)g_promote_list);
    /* no navigation occurred -> neither scroll animator fired */
    ASSERT_EQ((long)g_promote_scroll_down_calls, 0);
    ASSERT_EQ((long)g_promote_scroll_up_calls, 0);
}

/* Space (0x39) is the second commit key -> also returns 1. */
static void test_select_loop_space_commits(void)
{
    int r;

    promote_loop_setup();
    kbd_inject_scancode(0x39);

    r = fd2_promote_members_select_loop(4, g_promote_list);

    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)g_promote_grid_calls, 1);
}

/* Esc (0x01) cancels -> returns -1. */
static void test_select_loop_esc_cancels(void)
{
    int r;

    promote_loop_setup();
    kbd_inject_scancode(0x01);

    r = fd2_promote_members_select_loop(4, g_promote_list);

    ASSERT_EQ((long)r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
    ASSERT_EQ((long)g_promote_grid_calls, 1);
    ASSERT_EQ((long)g_promote_scroll_down_calls, 0);
    ASSERT_EQ((long)g_promote_scroll_up_calls, 0);
}

void run_ui_menu_promote_tests(void)
{
    SUITE_BEGIN(ui_menu_promote);
    RUN_TEST(test_mixed_dead_pattern);
    RUN_TEST(test_all_alive);
    RUN_TEST(test_all_dead);
    RUN_TEST(test_empty_party);
    RUN_TEST(test_bit0_isolation);
    RUN_TEST(test_select_loop_enter_commits);
    RUN_TEST(test_select_loop_space_commits);
    RUN_TEST(test_select_loop_esc_cancels);
    /* restore stub default so later suites keep historical behavior */
    g_check_char_is_dead_use_array = 0;
    g_check_char_is_dead_return = 0;
    SUITE_END();
}
