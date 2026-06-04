/*
 * unit tests for src/anim/anicombt.c  (part 3)
 *
 * Covers the turn-phase "ENEMY TURN" / "PLAYER TURN" banner animators:
 *   fd2_animate_phase_banner_slide_in  @ 0x1F1CC
 *   fd2_animate_phase_banner_slide_out @ 0x1F30A
 *
 * The body is almost entirely display side-effect (VGA backup/restore,
 * sprite blits, palette fade, BIOS-tick waits) and is exercised end-to-end
 * by the real fd2_run_full_turn_cycle integration test in
 * tests/battle/btl_turn.c. Here we pin the two deterministic control-flow
 * facts in isolation:
 *
 *   1. the slide-in renders exactly 7 banner frames (5-frame x_offset
 *      countdown 0x64..0 plus two settle frames at x=1 then x=0), and
 *   2. the palette fade-in loop runs exactly 16 times with scroll_offset
 *      advancing 1..16,
 *
 * and that the function completes without faulting — which proves the
 * EAX-bug fix: each of the two per-frame fd2_alloc_and_blit_indexed_sprite_chunk
 * calls returns its own malloc'd save buffer and each is freed once; a wrong
 * (collapsed/duplicated) free arg would crash on a bad pointer before the
 * assertions are reached.
 *
 * The fade loop calls the REAL fd2_alloc_and_blit_indexed_sprite_chunk, which
 * resolves sprite_addr = sheet + *(int32 *)(sheet + 6 + idx*4); a synthetic
 * sheet whose every offset-table entry points at a {int16 0, int16 0} header
 * makes those blits zero-size (malloc(8), no pixels) for indices 0x50/0x51/0x52.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* phase-banner callee recording (testglob.c): the not-yet-emitted
 * fd2_render_phase_banner_frame / fd2_scroll_buffer_block_with_wrap stubs. */
extern int    g_render_phase_banner_frame_calls;
extern uint32 g_render_phase_banner_frame_last_x;
extern int    g_scroll_buffer_calls;
extern uint32 g_scroll_buffer_last_wrap;

/* large_game_state_buffer destination for the slide-in's
 * memmove(lgs, snapshot, 64000) working-buffer restore (needs >= 64000 B). */
static uint8 g_banner_lgs[0x10000];

/* Synthetic sprite atlas read by the real fd2_alloc_and_blit_indexed_sprite_chunk
 * in the fade loop. Offset table at +6 (4 B/entry); every entry -> a {0,0}
 * header at +0x400 so width=height=0. Sized to cover indices 0..0x52. */
#define BANNER_SHEET_HDR_OFF 0x400
static uint8 g_banner_sheet[0x600];

static void t_install_banner_sheet(void)
{
    int idx;

    memset(g_banner_sheet, 0, sizeof(g_banner_sheet));
    for (idx = 0; idx < 0x53; idx++) {
        *(int32 *)(g_banner_sheet + 6 + idx * 4) = (int32)BANNER_SHEET_HDR_OFF;
    }
    /* header {0,0} at +0x400 already zeroed by the memset above. */
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_banner_sheet;
}

/* ENEMY-TURN banner (id 0x52): 7 slide frames, 16 fade frames. */
static void test_banner_slide_in_frame_and_fade_counts(void)
{
    uint32 save_lgs;
    uint32 save_sheet;

    save_lgs = data_fd2_large_game_state_buffer_ptr;
    save_sheet = data_fd2_ui_anim_sprite_sheet_ptr;

    memset(g_banner_lgs, 0, sizeof(g_banner_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_banner_lgs;
    t_install_banner_sheet();

    g_render_phase_banner_frame_calls = 0;
    g_render_phase_banner_frame_last_x = 0xFFFFFFFFu;
    g_scroll_buffer_calls = 0;
    g_scroll_buffer_last_wrap = 0;

    fd2_animate_phase_banner_slide_in(0x52);

    /* slide-in: frame_iter 4,3,2,1,0 (5) + settle x=1 + settle x=0 = 7. */
    ASSERT_EQ(g_render_phase_banner_frame_calls, 7);
    /* final frame renders at x_offset 0 (the settle position). */
    ASSERT_EQ((long)g_render_phase_banner_frame_last_x, 0);
    /* fade loop runs 16 times, scroll_offset 1..16 (last == 16). */
    ASSERT_EQ(g_scroll_buffer_calls, 16);
    ASSERT_EQ((long)g_scroll_buffer_last_wrap, 16);

    data_fd2_large_game_state_buffer_ptr = save_lgs;
    data_fd2_ui_anim_sprite_sheet_ptr = save_sheet;
}

/* PLAYER-TURN banner (id 0x50): identical structure, different sprite id —
 * confirms the frame/fade counts are independent of banner_sprite_id and the
 * blit of index 0x50 from the synthetic sheet is equally crash-free. */
static void test_banner_slide_in_player_id(void)
{
    uint32 save_lgs;
    uint32 save_sheet;

    save_lgs = data_fd2_large_game_state_buffer_ptr;
    save_sheet = data_fd2_ui_anim_sprite_sheet_ptr;

    memset(g_banner_lgs, 0, sizeof(g_banner_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_banner_lgs;
    t_install_banner_sheet();

    g_render_phase_banner_frame_calls = 0;
    g_scroll_buffer_calls = 0;

    fd2_animate_phase_banner_slide_in(0x50);

    ASSERT_EQ(g_render_phase_banner_frame_calls, 7);
    ASSERT_EQ(g_scroll_buffer_calls, 16);

    data_fd2_large_game_state_buffer_ptr = save_lgs;
    data_fd2_ui_anim_sprite_sheet_ptr = save_sheet;
}

/* ================================================================
 * fd2_animate_phase_banner_slide_out @ 0x1F30A — the turn-phase banner
 * slide-OUT animator (mirror of slide-in, called once a banner finishes
 * displaying). Same display-side-effect body; we pin the deterministic
 * control flow in isolation:
 *
 *   Phase 1 (palette fade-OUT): scrolls + double-blits + fades 17 times,
 *     fade_iter counting 0x10..0 and scroll_offset counting 0x11..1 (so the
 *     last scroll wrap_param is 1, vs slide-in's ascending 1..16).
 *   Phase 2: one composite_battle_tile_map + composite_all_chars_overlay.
 *   Phase 3 (slide-out): renders 5 banner frames, x_offset ASCENDING
 *     0,0x19,0x32,0x4B,0x64 (last == 0x64), vs slide-in's 7-frame descend.
 *
 * Completing without faulting proves the EAX-bug fix: each per-frame
 * fd2_alloc_and_blit_indexed_sprite_chunk returns its own malloc'd save
 * buffer freed once (a collapsed/duplicated free arg — e.g. freeing the
 * still-in-use scratch frame buffer mid-loop — would corrupt the heap and
 * crash on the next iteration's scroll/blit before the assertions run).
 *
 * party_member_count is forced to 0 so the real composite_all_chars_overlay
 * iterates zero chars (no portrait cache / runtime-char fixture needed); the
 * unconditional composite_battle_tile_map stage still fires once.
 * ================================================================ */
extern int g_composite_call_count;

/* ENEMY-TURN banner (id 0x52): 17 fade frames, 5 slide-out frames. */
static void test_banner_slide_out_frame_and_fade_counts(void)
{
    uint32 save_lgs;
    uint32 save_sheet;
    uint32 save_party;

    save_lgs = data_fd2_large_game_state_buffer_ptr;
    save_sheet = data_fd2_ui_anim_sprite_sheet_ptr;
    save_party = data_fd2_battle_party_member_count;

    memset(g_banner_lgs, 0, sizeof(g_banner_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_banner_lgs;
    t_install_banner_sheet();
    data_fd2_battle_party_member_count = 0;

    g_render_phase_banner_frame_calls = 0;
    g_render_phase_banner_frame_last_x = 0xFFFFFFFFu;
    g_scroll_buffer_calls = 0;
    g_scroll_buffer_last_wrap = 0;
    g_composite_call_count = 0;

    fd2_animate_phase_banner_slide_out(0x52);

    /* fade loop runs 17 times (fade_iter 0x10..0 inclusive), scroll_offset
     * counts 0x11 down to 1 (last passed wrap_param == 1). */
    ASSERT_EQ(g_scroll_buffer_calls, 17);
    ASSERT_EQ((long)g_scroll_buffer_last_wrap, 1);
    /* Phase 2 recomposites the battle scene exactly once. */
    ASSERT_EQ(g_composite_call_count, 1);
    /* Phase 3 slide-out renders 5 frames, x_offset 0,0x19,0x32,0x4B,0x64. */
    ASSERT_EQ(g_render_phase_banner_frame_calls, 5);
    ASSERT_EQ((long)g_render_phase_banner_frame_last_x, 0x64);

    data_fd2_large_game_state_buffer_ptr = save_lgs;
    data_fd2_ui_anim_sprite_sheet_ptr = save_sheet;
    data_fd2_battle_party_member_count = save_party;
}

/* PLAYER-TURN banner (id 0x50): identical structure, different sprite id —
 * confirms the fade/slide counts are independent of banner_sprite_id and the
 * blit of index 0x50 from the synthetic sheet is equally crash-free. */
static void test_banner_slide_out_player_id(void)
{
    uint32 save_lgs;
    uint32 save_sheet;
    uint32 save_party;

    save_lgs = data_fd2_large_game_state_buffer_ptr;
    save_sheet = data_fd2_ui_anim_sprite_sheet_ptr;
    save_party = data_fd2_battle_party_member_count;

    memset(g_banner_lgs, 0, sizeof(g_banner_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_banner_lgs;
    t_install_banner_sheet();
    data_fd2_battle_party_member_count = 0;

    g_render_phase_banner_frame_calls = 0;
    g_scroll_buffer_calls = 0;

    fd2_animate_phase_banner_slide_out(0x50);

    ASSERT_EQ(g_scroll_buffer_calls, 17);
    ASSERT_EQ(g_render_phase_banner_frame_calls, 5);

    data_fd2_large_game_state_buffer_ptr = save_lgs;
    data_fd2_ui_anim_sprite_sheet_ptr = save_sheet;
    data_fd2_battle_party_member_count = save_party;
}

void run_anim_anicombt3_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anicombt (3)\n");
    RUN_TEST(test_banner_slide_in_frame_and_fade_counts);
    RUN_TEST(test_banner_slide_in_player_id);
    RUN_TEST(test_banner_slide_out_frame_and_fade_counts);
    RUN_TEST(test_banner_slide_out_player_id);
    printf("\n");
}
