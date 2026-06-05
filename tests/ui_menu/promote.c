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
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
/* immediate-END dialog text program + sprite sheet + blit spies, so the real
 * fd2_load_chapter_portrait / fd2_display_dialog_scene that the revive menu
 * drives return without hanging (minip_setup_env). */
#include "minipfix.h"

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

/* class-promotion candidate-grid spy (5-arg renderer, testglob.c) */
extern int    g_promote_cand_grid_calls;
extern uint32 g_promote_cand_grid_last_count;
extern uint32 g_promote_cand_grid_last_dst;
extern uint32 g_promote_cand_grid_last_cursor;
extern int    g_promote_cand_grid_last_list;
extern int    g_promote_cand_grid_last_aux;

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

/* ----------------------------------------------------------------
 * fd2_promote_member_select_loop @ 0x311DC — CLASS-PROMOTION input-loop
 * dispatch tests (singular loop; takes a 3rd target_classes arg and renders
 * via the 5-arg fd2_render_promote_candidates_grid).
 *
 * Same bounded shape as the revive picker above: full setup (3 workspace
 * mallocs, VRAM snapshot/clone, recording blit + 5-arg grid stub, 6 REAL
 * fd2_slide_panel_down_step frames) then one REAL
 * fd2_wait_input_with_chapter_dialog_blink(2) frame whose injected scancode
 * terminates the do/while on the first iteration. They pin the EAX-from-CALL
 * scancode dispatch (Enter/Space => 1, Esc => -1; the asm compares full EAX,
 * so the int compare here mirrors it) and prove setup zeroes cursor +
 * scroll_offset, stashes count/list, and renders the grid exactly once with
 * cursor 0, the char list, AND the new price/aux (target_classes) list
 * forwarded as the 5th arg. The Up/Down navigation arithmetic re-renders and
 * loops again (needs a 2nd key the in-process BIOS buffer cannot async-refill);
 * deferred to Phase 9 integration like the sibling picker, asserted absent via
 * the scroll-animator spies staying at 0. */
static uint8 g_promote_cand_list[4] = { 1, 4, 6, 8 };
static uint8 g_promote_target_list[4] = { 0x32, 0x33, 0x34, 0x35 };

static void promote_cand_loop_setup(void)
{
    promote_loop_setup();
    g_promote_cand_grid_calls = 0;
    g_promote_cand_grid_last_count = 0;
    g_promote_cand_grid_last_dst = 0;
    g_promote_cand_grid_last_cursor = 0;
    g_promote_cand_grid_last_list = 0;
    g_promote_cand_grid_last_aux = 0;
}

/* Enter (0x1C) on the first frame commits -> returns 1; setup invariants. */
static void test_cand_loop_enter_commits(void)
{
    int r;

    promote_cand_loop_setup();
    kbd_inject_scancode(0x1c);

    r = fd2_promote_member_select_loop(4, g_promote_cand_list,
                                       g_promote_target_list);

    ASSERT_EQ((long)r, 1);
    /* setup zeroed the poisoned cursor/scroll before the loop */
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_scroll_offset, 0);
    /* candidate list + count stashed for re-render */
    ASSERT_EQ((long)data_fd2_ui_menu_candidate_array_ptr,
              (long)(uint32)g_promote_cand_list);
    ASSERT_EQ((long)data_fd2_ui_menu_visible_item_count, 4);
    /* the 5-arg candidates grid rendered exactly once (setup), cursor 0,
     * with BOTH the char list and the target/aux list forwarded */
    ASSERT_EQ((long)g_promote_cand_grid_calls, 1);
    ASSERT_EQ((long)g_promote_cand_grid_last_count, 4);
    ASSERT_EQ((long)g_promote_cand_grid_last_cursor, 0);
    ASSERT_EQ((long)g_promote_cand_grid_last_list, (long)(int)g_promote_cand_list);
    ASSERT_EQ((long)g_promote_cand_grid_last_aux, (long)(int)g_promote_target_list);
    /* setup render targets the composed workspace_c, not the live aperture */
    ASSERT_EQ((long)g_promote_cand_grid_last_dst,
              (long)data_fd2_ui_slide_composed_target_buf_ptr);
    /* the revive members grid (4-arg) must NOT have fired */
    ASSERT_EQ((long)g_promote_grid_calls, 0);
    /* no navigation occurred -> neither scroll animator fired */
    ASSERT_EQ((long)g_promote_scroll_down_calls, 0);
    ASSERT_EQ((long)g_promote_scroll_up_calls, 0);
}

/* Space (0x39) is the second commit key -> also returns 1. */
static void test_cand_loop_space_commits(void)
{
    int r;

    promote_cand_loop_setup();
    kbd_inject_scancode(0x39);

    r = fd2_promote_member_select_loop(4, g_promote_cand_list,
                                       g_promote_target_list);

    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)g_promote_cand_grid_calls, 1);
}

/* Esc (0x01) cancels -> returns -1. */
static void test_cand_loop_esc_cancels(void)
{
    int r;

    promote_cand_loop_setup();
    kbd_inject_scancode(0x01);

    r = fd2_promote_member_select_loop(4, g_promote_cand_list,
                                       g_promote_target_list);

    ASSERT_EQ((long)r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
    ASSERT_EQ((long)g_promote_cand_grid_calls, 1);
    ASSERT_EQ((long)g_promote_scroll_down_calls, 0);
    ASSERT_EQ((long)g_promote_scroll_up_calls, 0);
}

/* ----------------------------------------------------------------
 * fd2_run_revive_menu_main @ 0x30DC3 — "nobody is dead" early-return path.
 *
 * Seeds an all-alive party so the real fd2_build_dead_chars_list_for_revive
 * returns 0; the menu then takes its count==0 branch: load the town speaker
 * portrait, show the "no one is dead" dialog (FDTXT 0x24C), wait one key,
 * close, and return — never touching gold, the picker, or the price globals.
 *
 * This is the one revive-loop path that is bounded enough for an in-process
 * unit test: it drives the REAL fd2_load_chapter_portrait (against the staged
 * real DATO.DAT, portrait kind = speaker_table[4] = 0x83), the REAL
 * fd2_display_dialog_scene (returns at once on the minip immediate-END text
 * program), and the REAL fd2_wait_for_input_dialog_with_blink(0) (released by
 * a pre-armed nonempty BIOS keyboard buffer). It pins the EAX-consuming
 * count==0 branch (build_dead_chars_list's return drives the if) and proves
 * the early return fires before any gold mutation.
 *
 * The commit path (price = level * price_table[job_id+5], the signed
 * affordability compare, and the bFlags=0 / HP-restore payoff) sits behind
 * three sequential blocking input loops — the candidate picker, then the
 * yes/no typewriter — plus the large-game-state / menu-dialog-handle / portrait
 * buffers those loops composite into. A single in-process BIOS buffer cannot
 * feed that many sequential waits, so its behavioral coverage is deferred to
 * Phase 9 integration under the emulator, the same deferral the sibling
 * input-loop menus (status member menu, inventory modal) already apply.
 * ---------------------------------------------------------------- */
static void test_revive_no_dead_chars_returns(void)
{
    uint32 saved_gold;
    uint32 saved_sprite_id;
    uint32 saved_value;
    int reached;

    /* minip env: sprite sheet + immediate-END dialog program + blit spies. */
    minip_setup_env();
    /* battle tile map NULL -> dialog helpers take their full-screen (menu)
     * layout, not the in-battle small-box path. */
    data_fd2_battle_tile_map_ptr = 0;
    data_fd2_dialog_active_portrait_blit_offset = 0;     /* != 0x728 menu pos */
    data_fd2_portrait_sprite_buffer = 0;                 /* loader frees prev iff != 0 */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;

    /* all-alive party -> dead-char list comes back empty (count 0). */
    seed_party(5, 0u);

    /* sentinels: the count==0 branch must NOT write any of these. */
    data_fd2_shared_party_total_gold = 4242;
    saved_gold = data_fd2_shared_party_total_gold;
    data_fd2_dialog_last_action_sprite_id_param = 0xCAFE;
    saved_sprite_id = data_fd2_dialog_last_action_sprite_id_param;
    data_fd2_dialog_last_action_value_param = 0xBEEF;
    saved_value = data_fd2_dialog_last_action_value_param;

    /* pre-arm the BIOS keyboard buffer NONEMPTY so the single
     * fd2_wait_for_input_dialog_with_blink(0) exits on its first poll. */
    *(volatile uint16 *)0x41AuL = 0x1E;          /* head        */
    *(volatile uint16 *)0x41CuL = 0x20;          /* tail = head + 2 -> nonempty */
    *(volatile uint16 *)0x41EuL = 0x1C00;        /* Enter scancode in AH       */

    reached = 0;
    fd2_run_revive_menu_main();
    reached = 1;

    /* control returned via the count==0 branch (no hang in the picker). */
    ASSERT_EQ(reached, 1);
    /* none of the money / dialog-substitution globals were touched. */
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, (long)saved_gold);
    ASSERT_EQ((long)data_fd2_dialog_last_action_sprite_id_param,
              (long)saved_sprite_id);
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, (long)saved_value);

    /* the real close fn freed all three workspaces; drop the dangling globals
     * and free the leaked portrait buffer so later suites stay clean. */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
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
    RUN_TEST(test_cand_loop_enter_commits);
    RUN_TEST(test_cand_loop_space_commits);
    RUN_TEST(test_cand_loop_esc_cancels);
    RUN_TEST(test_revive_no_dead_chars_returns);
    /* restore stub default so later suites keep historical behavior */
    g_check_char_is_dead_use_array = 0;
    g_check_char_is_dead_return = 0;
    SUITE_END();
}
