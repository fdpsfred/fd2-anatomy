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

/* per-basic-class required class-change key-item id (real FD2.LE values
 * @ 0x526A7), mirrored from testglob.c so the builder's expected key item per
 * portrait_id can be referenced in assertions. */
extern uint8  data_fd2_ui_per_basic_portrait_class_change_key_item_id_table[18];

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

/* ----------------------------------------------------------------
 * fd2_run_class_promotion_menu_main @ 0x31385 — "no one is ready" early-return.
 *
 * Uses an under-level single-member party so the REAL candidate builder reports
 * 0 eligible members and the menu takes its count==0 branch: load the town
 * speaker portrait, show the "no one
 * is ready" dialog (FDTXT 0x24F), wait one key, close, and return — never
 * reaching the picker, the item-consume branch, the BGM fanfare, or any
 * runtime_char mutation. This is the one class-promotion path bounded enough
 * for an in-process unit test (it drives the REAL fd2_load_chapter_portrait
 * against the staged real DATO.DAT, the REAL fd2_display_dialog_scene on the
 * minip immediate-END program, and the REAL fd2_wait_for_input_dialog_with_blink
 * released by a pre-armed nonempty BIOS keyboard buffer). It pins the
 * EAX-consuming count==0 branch — the builder's byte count is returned
 * zero-extended (MOVZX) and TEST EAX,EAX drives the if — and proves the early
 * return fires before any commit-side state changes.
 *
 * The commit path (item consume by class_id band, the promotion fanfare,
 * job_id/portrait_id writeback, FDICON.B24 portrait reload, and the stat-gain
 * dialog) sits behind two sequential blocking input loops — the candidate
 * picker then the yes/no typewriter — which a single in-process BIOS buffer
 * cannot feed, so its behavioral coverage is deferred to Phase 9 integration
 * under the emulator (the same deferral the sibling fd2_run_revive_menu_main
 * and the other input-loop menus already apply).
 * ---------------------------------------------------------------- */
static void test_promote_no_candidates_returns(void)
{
    uint32 saved_sprite_id;
    int reached;

    /* minip env: sprite sheet + immediate-END dialog program + blit spies. */
    minip_setup_env();
    data_fd2_battle_tile_map_ptr = 0;                   /* full-screen dialog layout */
    data_fd2_dialog_active_portrait_blit_offset = 0;
    data_fd2_portrait_sprite_buffer = 0;                /* loader frees prev iff != 0 */

    /* the REAL builder now decides the count: a single under-level member is
     * ineligible (level 0 < 0x14), so it reports 0 -> count==0 branch. No key
     * item matters here since the member is filtered out before any inventory
     * lookup, and the all-zero fixture below has no special inventory state. */

    /* single-member party on the shared (file-static) fixture so any
     * (unexpected) commit-path roster read stays in bounds AND no dangling
     * pointer is left behind for later suites; the count==0 branch must not
     * touch it. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].job_id = 0x55;                   /* sentinel: must survive */
    g_test_rc_array[0].portrait_id = 0x09;
    g_test_rc_array[0].status_flags_block[0] = 0x00;    /* level 0 < 0x14 -> skip */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_shared_menu_party_member_count = 1;

    data_fd2_dialog_last_action_sprite_id_param = 0xCAFE;
    saved_sprite_id = data_fd2_dialog_last_action_sprite_id_param;

    /* pre-arm the BIOS keyboard buffer NONEMPTY so the single
     * fd2_wait_for_input_dialog_with_blink(0) exits on its first poll. */
    *(volatile uint16 *)0x41AuL = 0x1E;                 /* head        */
    *(volatile uint16 *)0x41CuL = 0x20;                 /* tail = head+2 -> nonempty */
    *(volatile uint16 *)0x41EuL = 0x1C00;              /* Enter scancode in AH       */

    reached = 0;
    fd2_run_class_promotion_menu_main();
    reached = 1;

    /* control returned via the count==0 branch (no hang in the picker). */
    ASSERT_EQ(reached, 1);
    /* the commit path never ran: the sentinel char + dialog-substitution param
     * were left untouched. */
    ASSERT_EQ((long)g_test_rc_array[0].job_id, 0x55);
    ASSERT_EQ((long)g_test_rc_array[0].portrait_id, 0x09);
    ASSERT_EQ((long)data_fd2_dialog_last_action_sprite_id_param,
              (long)saved_sprite_id);

    /* free the portrait buffer the real loader may have allocated. */
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
}

/* ----------------------------------------------------------------
 * fd2_execute_class_promotion_with_dialog @ 0x31602 — promotion finalization.
 *
 * Drives the REAL function in-process against the minip immediate-END dialog
 * program + a zeroed runtime_char fixture, with the not-yet-emitted
 * fd2_roll_stat_gain_and_show_message faked (records the threaded 4-row cursor
 * + per-stat args, returns g_roll_stat_next_row). portrait_id is held in
 * [0x20,0x33] so it is simultaneously a valid DATO.DAT portrait index (136
 * entries 0..0x87, used by the REAL fd2_load_chapter_portrait), an in-bounds
 * character_growth[68] index (fd2_get_char_growth_entry), and an in-bounds
 * class_promotion_data_table[20*2] index ((id-0x20)*2; fd2_get_class_promotion_
 * data_entry). The REAL fd2_load_chapter_portrait / fd2_paint_portrait_to_
 * dialog_area / fd2_display_dialog_scene / fd2_recalculate_combat_stats /
 * fd2_close_intro_dialog_with_slide_out all run for real (slide buffers are
 * malloc'd by the loader and freed by the close fn within this one call).
 *
 * Pinned (the risk-bearing logic, not the blit side effects):
 *   - the dialog substitution sprite id = job_id + 0x96 (step 2);
 *   - the five stat rolls receive the correct stat pointers, growth-pair
 *     offsets (+0,+2,+4,+6,+8) and message pages (0x1EA..0x1EE), and the
 *     4-row cursor is threaded call-to-call (5th call's row == the value the
 *     fake returns), exercising the EAX-from-CALL chain;
 *   - the conditional spell append: when promo_entry[1] != 0 the spell id is
 *     stashed in last_action_value and added to combat_aux[0x14] (and the
 *     blink-wait is reached — BIOS buffer pre-armed); when 0 the append is
 *     skipped and combat_aux[0x14] is left untouched (the MOVZX/TEST branch);
 *   - the fresh-state reset (step 7): level(status_flags[0])=1,
 *     movement_order=0, hp_current=hp_max, mp_current=mp_max.
 * ---------------------------------------------------------------- */
extern int    g_roll_stat_calls;
extern short *g_roll_stat_last_stat_ptr;
extern uint8 *g_roll_stat_last_growth_ptr;
extern uint32 g_roll_stat_last_text_id;
extern int    g_roll_stat_last_row;
extern int    g_roll_stat_next_row;
extern int    g_roll_stat_arm_kbd_on_call;

/* This function shows dialog pages 0x253 / 0x254, which lie past the end of
 * minipfix's t_minip_text[0x200]. Use a larger immediate-END program so the
 * REAL fd2_display_dialog_scene resolves those pages to an END opcode (it
 * reads *(int16*)(base + page*2) as the byte offset to the page program, then
 * the first opcode there): every page entry in [0,0x300) points to the END
 * word parked at byte offset 0x600 (= index 0x300). */
static uint16 t_promote_text[0x400];

static void promote_text_setup(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        t_promote_text[i] = 0x600;          /* -> byte offset of the END word */
    }
    t_promote_text[0x300] = (uint16)-1;     /* END opcode */
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)t_promote_text;
}

/* Stand up the shared env for a real promotion-finalize run: minip dialog/
 * sprite env (sprite sheet + blit spies), a larger immediate-END text program
 * (pages 0x253/0x254 are out of minip's range), a single zeroed runtime_char
 * at slot 0, the slide-buffer globals cleared (the loader mallocs them), the
 * roll fake reset, and the BIOS keyboard buffer pre-armed nonempty. */
static void promote_exec_setup(uint8 portrait_id, uint8 job_id)
{
    minip_setup_env();
    promote_text_setup();                             /* override text table  */
    data_fd2_battle_tile_map_ptr = 0;                 /* full-screen layout   */
    data_fd2_dialog_active_portrait_blit_offset = 0;
    data_fd2_portrait_sprite_buffer = 0;              /* loader frees prev iff !=0 */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].portrait_id = portrait_id;
    g_test_rc_array[0].job_id = job_id;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    g_roll_stat_calls = 0;
    g_roll_stat_last_stat_ptr = 0;
    g_roll_stat_last_growth_ptr = 0;
    g_roll_stat_last_text_id = 0;
    g_roll_stat_last_row = 0;
    g_roll_stat_next_row = 0;
    g_roll_stat_arm_kbd_on_call = 0;

    /* pre-arm the BIOS keyboard buffer NONEMPTY so the spell-branch
     * fd2_wait_for_input_dialog_with_blink(0) exits on its first poll. */
    *(volatile uint16 *)0x41AuL = 0x1E;               /* head                 */
    *(volatile uint16 *)0x41CuL = 0x20;               /* tail = head+2 -> nonempty */
    *(volatile uint16 *)0x41EuL = 0x1C00;             /* Enter scancode in AH */
}

static void promote_exec_teardown(void)
{
    g_roll_stat_arm_kbd_on_call = 0;     /* don't leak into other suites */
    /* the close fn freed the slide buffers; drop dangling globals and free
     * the portrait buffer the real loader allocated. */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
}

/* ---- spell-append branch TAKEN: class learns a spell on promotion ---- */
static void test_promote_exec_learns_spell(void)
{
    runtime_char *rc;

    /* portrait_id 0x30 -> promotion entry at table+(0x30-0x20)*2 = +0x20. */
    promote_exec_setup(0x30, 0x07);
    rc = &g_test_rc_array[0];

    /* promotion entry: [0]=new job id (unused here), [1]=learned spell id. */
    data_fd2_class_promotion_data_table[(0x30 - 0x20) * 2 + 0] = 0x09;
    data_fd2_class_promotion_data_table[(0x30 - 0x20) * 2 + 1] = 0x0B;  /* spell 11 */

    /* the roll fake returns this as the threaded cursor / spell-dialog row. */
    g_roll_stat_next_row = 2;
    /* the spell branch blocks on a blink-wait after the post-roll drain; the
     * fake re-arms the BIOS buffer (stands in for the player's keypress) so
     * the wait is satisfiable in-process. */
    g_roll_stat_arm_kbd_on_call = 1;

    /* seed HP/MP max distinct from a poisoned current; reset must copy max->cur. */
    rc->hp_max = 0x0140;
    rc->mp_max = 0x0037;
    rc->hp_current = 0xAAAA;          /* poisoned: reset must overwrite */
    rc->mp_current = 0xBBBB;          /* poisoned: reset must overwrite */
    rc->status_flags_block[0] = 0x55; /* poisoned level: reset must set 1 */
    rc->movement_order = 0xFF;        /* poisoned XP carry: reset must clear */
    rc->combat_aux_block[0x14] = 0x05;/* existing known-spell-list tail byte */

    fd2_execute_class_promotion_with_dialog(0);

    /* step 2: sprite id = job_id + 0x96. */
    ASSERT_EQ((long)data_fd2_dialog_last_action_sprite_id_param,
              (long)(0x07 + 0x96));

    /* step 4: five rolls, cursor threaded; last (5th) call's incoming row is
     * what the fake returned for the 4th call. */
    ASSERT_EQ((long)g_roll_stat_calls, 5);
    ASSERT_EQ((long)g_roll_stat_last_row, 2);            /* 5th call got fake's row */
    ASSERT_EQ((long)g_roll_stat_last_text_id, 0x1ee);    /* 5th page = MP_max */
    /* 5th roll targets &mp_max with growth+8. */
    ASSERT_EQ((long)(uint32)g_roll_stat_last_stat_ptr, (long)(uint32)&rc->mp_max);
    ASSERT_EQ((long)(uint32)g_roll_stat_last_growth_ptr,
              (long)(uint32)(fd2_get_char_growth_entry(0x30) + 8));

    /* step 5: spell learned -> value stashed + appended to combat_aux[0x14]. */
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, 0x0B);
    ASSERT_EQ((long)rc->combat_aux_block[0x14], (long)(0x05 + 0x0B));

    /* step 7: fresh level-1 state + full HP/MP restore. */
    ASSERT_EQ((long)rc->status_flags_block[0], 1);
    ASSERT_EQ((long)rc->movement_order, 0);
    ASSERT_EQ((long)rc->hp_current, 0x0140);
    ASSERT_EQ((long)rc->mp_current, 0x0037);

    promote_exec_teardown();
}

/* ---- spell-append branch SKIPPED: class learns no spell (entry[1]==0) ---- */
static void test_promote_exec_no_spell(void)
{
    runtime_char *rc;

    /* portrait_id 0x20 -> promotion entry at table+0; force [1]=0. */
    promote_exec_setup(0x20, 0x11);
    rc = &g_test_rc_array[0];

    data_fd2_class_promotion_data_table[(0x20 - 0x20) * 2 + 0] = 0x21;
    data_fd2_class_promotion_data_table[(0x20 - 0x20) * 2 + 1] = 0x00;  /* no spell */

    g_roll_stat_next_row = 1;

    rc->hp_max = 0x00C8;
    rc->mp_max = 0x0010;
    rc->hp_current = 0x0001;          /* poisoned */
    rc->mp_current = 0x0002;          /* poisoned */
    rc->status_flags_block[0] = 0x33; /* poisoned level */
    rc->movement_order = 0xFF;        /* poisoned XP carry */
    rc->combat_aux_block[0x14] = 0x07;/* must stay untouched (branch skipped) */

    /* sentinel the dialog value so we can prove the skipped branch never wrote it. */
    data_fd2_dialog_last_action_value_param = 0xDEAD;

    fd2_execute_class_promotion_with_dialog(0);

    /* step 2 still runs. */
    ASSERT_EQ((long)data_fd2_dialog_last_action_sprite_id_param,
              (long)(0x11 + 0x96));
    /* all five rolls still run regardless of the spell branch. */
    ASSERT_EQ((long)g_roll_stat_calls, 5);

    /* step 5 SKIPPED: combat_aux[0x14] untouched, dialog value sentinel intact. */
    ASSERT_EQ((long)rc->combat_aux_block[0x14], 0x07);
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, (long)0xDEAD);

    /* step 7 still applies. */
    ASSERT_EQ((long)rc->status_flags_block[0], 1);
    ASSERT_EQ((long)rc->movement_order, 0);
    ASSERT_EQ((long)rc->hp_current, 0x00C8);
    ASSERT_EQ((long)rc->mp_current, 0x0010);

    promote_exec_teardown();
}

/* ----------------------------------------------------------------
 * fd2_build_promotion_candidates_with_targets @ 0x31793 — REAL emit tests.
 *
 * Stands up the shared g_test_rc_array fixture directly (no blocking I/O) and
 * drives the real eligibility scan + parallel out_chars/out_targets packing.
 * The find-item callee is the REAL fd2_find_inventory_slot_with_item: by
 * default promo_cand_setup empties every inventory slot so no char owns any
 * item (-> the default +0x20 target), and promo_cand_grant_item() stamps an
 * item into a char's real inventory to fire the key-item / Sword branches.
 *
 * Eligibility (all three must hold to record a candidate):
 *   level (status_flags_block[0]) >= 0x14, portrait_id < 0x12, portrait_id != 7.
 * Target class: portrait_id+0x20 default; portrait_id+0x32 if the char owns the
 * per-class key item (data..key_item_id_table[portrait_id]); 0x34 if
 * portrait_id==9 and the char owns Sword(0x5A) (this last write wins).
 * ---------------------------------------------------------------- */

/* zero the fixture, set party size, and mark every inventory slot of every
 * char EMPTY (flag bit 0x80) so the real fd2_find_inventory_slot_with_item
 * reports "not owned" by default (usable count 0 -> returns -1). Tests grant
 * an item via promo_cand_grant_item to fire the key-item / Sword branches. */
static void promo_cand_setup(int member_count)
{
    int c;
    int s;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_shared_menu_party_member_count = (uint32)member_count;
    for (c = 0; c < 8; c++) {
        for (s = 0; s < 8; s++) {
            g_test_rc_array[c].inventory_slots[s * 2] = 0x80;   /* empty */
        }
    }
}

/* an eligible member: level 0x14, given portrait_id (caller picks < 0x12,
 * != 7 for "kept"). */
static void promo_cand_set_member(int idx, uint8 portrait_id, uint8 level)
{
    g_test_rc_array[idx].portrait_id = portrait_id;
    g_test_rc_array[idx].status_flags_block[0] = level;
}

/* grant char idx a real inventory copy of item_id by making its first empty
 * slot usable (flag clear) and stamping the item id there. Since promo_cand_setup
 * empties all slots and grants fill consecutively from slot 0, after k grants
 * slots 0..k-1 are usable, so fd2_count_usable_inventory_slots(idx)==k and
 * fd2_find_inventory_slot_with_item scans exactly the granted slots [0,k). */
static void promo_cand_grant_item(int idx, uint8 item_id)
{
    int s;
    for (s = 0; s < 8; s++) {
        if ((g_test_rc_array[idx].inventory_slots[s * 2] & 0x80) != 0) {
            g_test_rc_array[idx].inventory_slots[s * 2] = 0x00;
            g_test_rc_array[idx].inventory_slots[s * 2 + 1] = item_id;
            return;
        }
    }
}

/* ---- every eligibility filter + default +0x20 target, with packing ---- */
static void test_build_cand_filters_and_default_target(void)
{
    uint8 out_chars[32];
    uint8 out_targets[32];
    uint8 count;

    promo_cand_setup(6);
    /* idx0: eligible, basic class 3 -> kept, target 0x23.            */
    promo_cand_set_member(0, 0x03, 0x14);
    /* idx1: level 0x13 (< 0x14) -> skipped.                          */
    promo_cand_set_member(1, 0x04, 0x13);
    /* idx2: portrait_id 0x12 (>= 0x12) -> skipped.                   */
    promo_cand_set_member(2, 0x12, 0x20);
    /* idx3: portrait_id 7 (reserved lord) -> skipped.                */
    promo_cand_set_member(3, 0x07, 0x20);
    /* idx4: eligible, class 0x11 (highest basic), high level -> kept */
    promo_cand_set_member(4, 0x11, 0x40);
    /* idx5: eligible, class 0 -> kept, target 0x20.                  */
    promo_cand_set_member(5, 0x00, 0x14);

    memset(out_chars, 0xAA, sizeof(out_chars));
    memset(out_targets, 0xAA, sizeof(out_targets));

    count = fd2_build_promotion_candidates_with_targets(out_chars, out_targets);

    /* three kept (idx 0,4,5), packed contiguously skipping the filtered ones. */
    ASSERT_EQ((long)count, 3);
    ASSERT_EQ((long)out_chars[0], 0);
    ASSERT_EQ((long)out_chars[1], 4);
    ASSERT_EQ((long)out_chars[2], 5);
    /* default target = portrait_id + 0x20 (no char owns its key item). */
    ASSERT_EQ((long)out_targets[0], 0x03 + 0x20);
    ASSERT_EQ((long)out_targets[1], 0x11 + 0x20);
    ASSERT_EQ((long)out_targets[2], 0x00 + 0x20);
    /* the slot past the last candidate was never written. */
    ASSERT_EQ((long)out_chars[3], 0xAA);
    ASSERT_EQ((long)out_targets[3], 0xAA);
}

/* ---- key-item branch: owning key_item[portrait_id] -> target +0x32 ---- */
static void test_build_cand_key_item_branch(void)
{
    uint8 out_chars[32];
    uint8 out_targets[32];
    uint8 count;
    uint8 key_for_8;

    promo_cand_setup(2);
    /* idx0: portrait_id 8 owns its key item -> +0x32; idx1: class 3, no item. */
    promo_cand_set_member(0, 0x08, 0x14);
    promo_cand_set_member(1, 0x03, 0x14);

    key_for_8 = data_fd2_ui_per_basic_portrait_class_change_key_item_id_table[0x08];
    promo_cand_grant_item(0, key_for_8);          /* char 0 holds class-8 key */

    count = fd2_build_promotion_candidates_with_targets(out_chars, out_targets);

    ASSERT_EQ((long)count, 2);
    ASSERT_EQ((long)out_chars[0], 0);
    ASSERT_EQ((long)out_chars[1], 1);
    /* idx0 took the alt path (+0x32) because it owns its class-8 key item;
     * idx1 owns nothing so it stayed on the default (+0x20). The per-char
     * key-item probe is thus pinned by the divergent targets. */
    ASSERT_EQ((long)out_targets[0], 0x08 + 0x32);
    ASSERT_EQ((long)out_targets[1], 0x03 + 0x20);
}

/* ---- Lord direct: portrait_id 9 + Sword(0x5A) -> target 0x34 (wins) ---- */
static void test_build_cand_lord_sword_branch(void)
{
    uint8 out_chars[32];
    uint8 out_targets[32];
    uint8 count;
    uint8 key_for_9;

    promo_cand_setup(1);
    promo_cand_set_member(0, 0x09, 0x14);   /* Lord candidate, eligible */

    /* grant BOTH the class-9 key item AND the Sword so the +0x32 write happens
     * first and the 0x34 Sword write then overrides it (the asm order). */
    key_for_9 = data_fd2_ui_per_basic_portrait_class_change_key_item_id_table[0x09];
    promo_cand_grant_item(0, key_for_9);
    promo_cand_grant_item(0, 0x5A);

    count = fd2_build_promotion_candidates_with_targets(out_chars, out_targets);

    ASSERT_EQ((long)count, 1);
    ASSERT_EQ((long)out_chars[0], 0);
    ASSERT_EQ((long)out_targets[0], 0x34);   /* Lord-direct overrides +0x32 */
}

/* ---- portrait_id 9 WITHOUT Sword stays on the key-item / default path ---- */
static void test_build_cand_lord_no_sword(void)
{
    uint8 out_chars[32];
    uint8 out_targets[32];
    uint8 count;

    promo_cand_setup(1);
    promo_cand_set_member(0, 0x09, 0x14);
    /* own nothing: no key item, no Sword -> plain default target. */

    count = fd2_build_promotion_candidates_with_targets(out_chars, out_targets);

    ASSERT_EQ((long)count, 1);
    ASSERT_EQ((long)out_chars[0], 0);
    ASSERT_EQ((long)out_targets[0], 0x09 + 0x20);   /* 0x29, not 0x34 */
}

/* ---- empty party -> count 0, no writes, no find-item probes ---- */
static void test_build_cand_empty_party(void)
{
    uint8 out_chars[32];
    uint8 out_targets[32];
    uint8 count;

    promo_cand_setup(0);
    memset(out_chars, 0xAA, sizeof(out_chars));
    memset(out_targets, 0xAA, sizeof(out_targets));

    count = fd2_build_promotion_candidates_with_targets(out_chars, out_targets);

    ASSERT_EQ((long)count, 0);
    /* no eligible char -> the builder never reached an inventory probe and
     * wrote nothing into either output buffer. */
    ASSERT_EQ((long)out_chars[0], 0xAA);
    ASSERT_EQ((long)out_targets[0], 0xAA);
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
    RUN_TEST(test_promote_no_candidates_returns);
    RUN_TEST(test_promote_exec_learns_spell);
    RUN_TEST(test_promote_exec_no_spell);
    RUN_TEST(test_build_cand_filters_and_default_target);
    RUN_TEST(test_build_cand_key_item_branch);
    RUN_TEST(test_build_cand_lord_sword_branch);
    RUN_TEST(test_build_cand_lord_no_sword);
    RUN_TEST(test_build_cand_empty_party);
    /* restore stub default so later suites keep historical behavior */
    g_check_char_is_dead_use_array = 0;
    g_check_char_is_dead_return = 0;
    SUITE_END();
}
