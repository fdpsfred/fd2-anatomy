/*
 * unit tests for src/anim/aniui.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>

#define USE_ITEM_ID 10

extern runtime_char g_test_rc_array[8];
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;
extern uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
extern int g_ending_menu_return;
extern int g_chapter_transition_return;
extern int g_play_sfx_with_handle_calls;
extern int g_play_sfx_sample_from_bank_calls;
extern int g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int g_blit_indexed_sprite_last_x;
extern int g_blit_indexed_sprite_last_y;
extern int g_composite_call_count;
extern int g_attack_dispatch_return;
extern int g_attack_dispatch_calls;
extern int g_seek_optimal_return;
extern int g_advance_nearest_return;
extern int g_walk_return;
extern int g_score_physical_return;
extern int g_pass_turn_calls;
extern int g_execute_spell_calls;
extern int g_execute_physical_calls;
extern int g_pathfind_return;
extern int g_pathfind_walk_return;
extern int g_pathfind_write_dst;
extern int g_pathfind_dst_x;
extern int g_pathfind_dst_y;
extern int g_pathfind_seq_enable;
extern int g_pathfind_seq[4];
extern int g_pathfind_seq_idx;
extern int g_pathfind_seq_steps;
extern uint8 g_pathfind_step_bytes[8];
extern int g_pathfind_md0_dst_x;
extern int g_pathfind_md0_dst_y;
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;

/* screen-shake observability (testglob.c):
 *   - the per-frame delay thunk records call count + last tick value
 *   - the tile-map composite stub counts every composite pass (the function's
 *     own mode-9 snapshot + the one inside the real fd2_composite_battle_frame)
 *   - g_composite_call_count mirrors that count */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;
extern int    g_tile_map_calls;
extern int    g_composite_call_count;

/* party-addition appear-effect observability (testglob.c): the recording
 * fd2_blit_sprite_with_decoded_pixels stub counts every explosion-sprite
 * blit emitted by the per-frame new-char loop. */
extern int    g_blitdec_calls;

/* money-roller observability (testglob.c): the per-digit blit primitive records
 * its call count + last resolved args (slot dst, stride, sprite index). */
extern int    g_money_blit_calls;
extern uint32 g_money_blit_last_dst;
extern uint32 g_money_blit_last_stride;
extern uint32 g_money_blit_last_sprite;


/* Backing for data_fd2_large_game_state_buffer_ptr. The real fd2_blit_rectangle
 * (and the real fd2_composite_battle_frame finalizer) read the visible region
 * from workspace = base + 0x8088, up to row 0xC0 at stride 0x1C8 starting at
 * +0x8088 (+0x1C8 for odd iterations): max read offset = 0x8088 + 0xC0*0x1C8 +
 * 0x138 < 0x1E000. The VGA-side write to 0xA0504 lands in emulated video memory
 * and is harmless (write-only). */
#define SHAKE_LGS_SPAN 0x1E000u
static uint8 g_shake_lgs[SHAKE_LGS_SPAN];

/* Make fd2_composite_battle_frame(0) host-safe: HUD gated off (early-return),
 * cursor phase 0 (no overlay blit), empty party (no per-char paint), palette
 * cycle throttled to early-return. */
static void shake_setup(void)
{
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    g_tile_map_calls = 0;
    g_composite_call_count = 0;

    memset(g_shake_lgs, 0, sizeof(g_shake_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_shake_lgs;

    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x20;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;

    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;
    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_party_member_count = 0;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;
}


static void test_tick_tutorial_sfx_counter(void)
{
    uint8 save_ctr;

    save_ctr = data_fd2_audio_walk_step_sfx_cadence_counter;
    data_fd2_audio_walk_step_sfx_cadence_counter = 5;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].job_id = 1;

    fd2_tick_tutorial_progress_with_sfx(0);
    ASSERT_EQ(data_fd2_audio_walk_step_sfx_cadence_counter, 6);

    data_fd2_audio_walk_step_sfx_cadence_counter = save_ctr;
}


/*
 * fd2_animate_screen_shake — loop count + per-frame structure.
 *
 * The function snapshots the scene once (a mode-9 tile-map composite) + one
 * finalizer composite, then loops num_frames times, each frame blitting the
 * visible region to the mode13h primary with a 0x14-tick delay. The composite
 * count is frame-count-independent (always 2: the direct snapshot + the
 * finalizer's single tile-map pass); the delay count and per-frame blit count
 * equal num_frames.
 *
 * Verified here (host-safe observables): the loop iterates exactly num_frames
 * times (delay-thunk call count), the per-frame delay constant (0x14), and the
 * fixed two-composite preamble. The per-frame 1-row vertical-jitter source
 * alternation (i & 1) * 0x1C8 only manifests as a write to the mode13h primary
 * at the absolute VGA address 0xA0504 (the real fd2_blit_rectangle target);
 * VGA memory is write-only here (read-back is unreliable, no host buffer can
 * back the fixed 0xA0504), so the resulting pixel jitter is deferred to Phase 9
 * integration (screen-buffer comparison), consistent with the other VGA-output
 * side-effects (palette-DAC writes, the composite finalizer's own primary blit).
 */
static void test_screen_shake_loop_and_jitter(void)
{
    /* odd frame count */
    shake_setup();
    fd2_animate_screen_shake(3);
    ASSERT_EQ(g_delay375b2_calls, 3);
    ASSERT_EQ(g_delay375b2_last_ticks, 0x14u);
    ASSERT_EQ(g_tile_map_calls, 2);
    ASSERT_EQ(g_composite_call_count, 2);

    /* even frame count */
    shake_setup();
    fd2_animate_screen_shake(4);
    ASSERT_EQ(g_delay375b2_calls, 4);
    ASSERT_EQ(g_delay375b2_last_ticks, 0x14u);
    ASSERT_EQ(g_tile_map_calls, 2);
    ASSERT_EQ(g_composite_call_count, 2);

    /* single frame: one delay, one blit */
    shake_setup();
    fd2_animate_screen_shake(1);
    ASSERT_EQ(g_delay375b2_calls, 1);
    ASSERT_EQ(g_delay375b2_last_ticks, 0x14u);
    ASSERT_EQ(g_tile_map_calls, 2);

    /* zero frames: snapshot + finalizer composite still run, but no blit/delay.
     * Signed loop guard (int32)i < (int32)num_frames keeps num_frames==0 (and
     * any high-bit value) from looping. */
    shake_setup();
    fd2_animate_screen_shake(0);
    ASSERT_EQ(g_delay375b2_calls, 0);
    ASSERT_EQ(g_tile_map_calls, 2);
    ASSERT_EQ(g_composite_call_count, 2);
}


/*
 * fd2_animate_money_increment — gold mutation + rolling-digit structure.
 *
 * The function (a) snapshots the 8 current decimal digits, (b) commits the
 * delta to data_fd2_shared_party_total_gold immediately, (c) snapshots the 8
 * target digits, then rolls each mismatching digit forward through 9-frame
 * advance steps until current==target, re-diffing between steps so carries to
 * higher positions resolve on the next outer iteration. Each (digit, frame)
 * pair emits one fd2_blit_money_digit_sprite(slot, 0x140, cur_digit*9+phase).
 *
 * Host-verifiable observables (every assertion below is hand-traced):
 *   - the gold total is the load-bearing state mutation: gold += delta, applied
 *     up-front (the visual catches up) — asserted exactly for each case.
 *   - the blit count is fully determined by the start->target digit transition:
 *       0->1 : one mismatching position (pos7), one 9-frame step      = 9 blits
 *       0->2 : pos7 rolls two advance steps (re-diff drives 0->1->2)  = 18 blits
 *       5->10: pos7 rolls 6->7->8->9->0 across 5 steps (wrap 9->0 hit)
 *              + pos6 0->1 in the first step (2 digits *9)            = 54 blits
 *   - the last blit's resolved sprite index encodes cur_digit*9+phase with
 *     phase peaking at 9 on the final frame of the last advance step, and the
 *     slot dst = 0xA7A90 + pos*6 (pos7 = +0x2A), stride 0x140.
 * The 9x6 pixel copy itself is a display side-effect deferred to Phase 9.
 */
static void test_money_increment_roll_and_total(void)
{
    /* single-digit, single advance step: 00000000 -> 00000001 */
    data_fd2_shared_party_total_gold = 0;
    g_money_blit_calls = 0;
    g_delay375b2_calls = 0;
    fd2_animate_money_increment(1);
    ASSERT_EQ(data_fd2_shared_party_total_gold, 1u);
    ASSERT_EQ(g_money_blit_calls, 9);          /* 1 digit * 9 frames * 1 step */
    ASSERT_EQ(g_delay375b2_calls, 9);          /* one 10ms delay per frame */
    ASSERT_EQ(g_delay375b2_last_ticks, 10u);
    ASSERT_EQ(g_money_blit_last_stride, 0x140u);
    ASSERT_EQ(g_money_blit_last_dst, 0xA7A90u + 7u * 6u);
    ASSERT_EQ(g_money_blit_last_sprite, 0u * 9u + 9u);  /* cur=0, phase=9 */

    /* single-digit, two advance steps: 00000000 -> 00000002 (re-diff 0->1->2) */
    data_fd2_shared_party_total_gold = 0;
    g_money_blit_calls = 0;
    fd2_animate_money_increment(2);
    ASSERT_EQ(data_fd2_shared_party_total_gold, 2u);
    ASSERT_EQ(g_money_blit_calls, 18);
    ASSERT_EQ(g_money_blit_last_dst, 0xA7A90u + 7u * 6u);
    ASSERT_EQ(g_money_blit_last_sprite, 1u * 9u + 9u);  /* last step cur=1, phase=9 */

    /* carry/wrap path: 00000005 -> 00000010. pos7 rolls 6->7->8->9->0 (wrap
     * 9->0 on the 5th step); pos6 advances 0->1 only on the first step. */
    data_fd2_shared_party_total_gold = 5;
    g_money_blit_calls = 0;
    fd2_animate_money_increment(5);
    ASSERT_EQ(data_fd2_shared_party_total_gold, 10u);     /* 5 + 5 = 10 decimal */
    ASSERT_EQ(g_money_blit_calls, 54);          /* 2*9 (step1) + 9*4 (steps 2-5) */
    ASSERT_EQ(g_money_blit_last_dst, 0xA7A90u + 7u * 6u);
    ASSERT_EQ(g_money_blit_last_sprite, 9u * 9u + 9u);    /* wrap step cur=9, phase=9 */
}


/*
 * fd2_animate_money_decrement — gold mutation + descending rolling-digit
 * structure (inverse of money_increment).
 *
 * The function (a) snapshots the 8 current decimal digits, (b) commits the
 * subtraction to data_fd2_shared_party_total_gold immediately, (c) snapshots
 * the 8 target digits, then per outer iteration diffs the two. Unlike the
 * increment variant, each mismatching position is pre-decremented in the diff
 * loop itself (anim_state=9; cur_digit-=1 with 0xFF->9 borrow), and the inner
 * 9-frame roll renders sprite (anim_state + cur_digit*9 - 1) while anim_state
 * counts 9..1 down to 0. Re-diffing between steps resolves borrows to higher
 * positions. Each (digit, frame) pair emits one fd2_blit_money_digit_sprite.
 *
 * Host-verifiable observables (every assertion below is hand-traced):
 *   - the gold total is the load-bearing state mutation: gold -= delta, applied
 *     up-front (the visual catches down) — asserted exactly for each case.
 *   - the blit count is fully determined by the start->target digit transition:
 *       1->0 : pos7 mismatches, one 9-frame step (cur pre-dec 1->0)    = 9 blits
 *       2->0 : pos7 rolls two steps (re-diff drives 2->1->0)           = 18 blits
 *       10->5: step1 borrows pos7 0->9 + decrements pos6 1->0 (2 digits)
 *              then pos7 rolls 9->8->7->6->5 across 4 more steps        = 54 blits
 *   - the last blit's resolved sprite index encodes anim_state + cur_digit*9 - 1
 *     with anim_state bottoming at 1 on the final frame of the last advance step,
 *     and the slot dst = 0xA7A90 + pos*6 (pos7 = +0x2A), stride 0x140.
 * The 9x6 pixel copy itself is a display side-effect deferred to Phase 9.
 */
static void test_money_decrement_roll_and_total(void)
{
    /* single-digit, single advance step: 00000001 -> 00000000 */
    data_fd2_shared_party_total_gold = 1;
    g_money_blit_calls = 0;
    g_delay375b2_calls = 0;
    fd2_animate_money_decrement(1);
    ASSERT_EQ(data_fd2_shared_party_total_gold, 0u);
    ASSERT_EQ(g_money_blit_calls, 9);          /* 1 digit * 9 frames * 1 step */
    ASSERT_EQ(g_delay375b2_calls, 9);          /* one 10ms delay per frame */
    ASSERT_EQ(g_delay375b2_last_ticks, 10u);
    ASSERT_EQ(g_money_blit_last_stride, 0x140u);
    ASSERT_EQ(g_money_blit_last_dst, 0xA7A90u + 7u * 6u);
    /* last frame: anim_state=1, cur=0 -> 1 + 0*9 - 1 = 0 */
    ASSERT_EQ(g_money_blit_last_sprite, 0u);

    /* single-digit, two advance steps: 00000002 -> 00000000 (re-diff 2->1->0) */
    data_fd2_shared_party_total_gold = 2;
    g_money_blit_calls = 0;
    fd2_animate_money_decrement(2);
    ASSERT_EQ(data_fd2_shared_party_total_gold, 0u);
    ASSERT_EQ(g_money_blit_calls, 18);
    ASSERT_EQ(g_money_blit_last_dst, 0xA7A90u + 7u * 6u);
    /* last step cur pre-dec 1->0, last frame anim=1 -> 1 + 0*9 - 1 = 0 */
    ASSERT_EQ(g_money_blit_last_sprite, 0u);

    /* borrow path: 00000010 -> 00000005. step1: pos7 0->9 (0xFF borrow) and
     * pos6 1->0 both animate (2 digits); then pos7 rolls 9->8->7->6->5 over
     * 4 more steps (re-diff each). */
    data_fd2_shared_party_total_gold = 10;
    g_money_blit_calls = 0;
    fd2_animate_money_decrement(5);
    ASSERT_EQ(data_fd2_shared_party_total_gold, 5u);      /* 10 - 5 = 5 decimal */
    ASSERT_EQ(g_money_blit_calls, 54);          /* 2*9 (step1) + 9*4 (steps 2-5) */
    ASSERT_EQ(g_money_blit_last_dst, 0xA7A90u + 7u * 6u);
    /* last step cur=5, last frame anim=1 -> 1 + 5*9 - 1 = 45 */
    ASSERT_EQ(g_money_blit_last_sprite, 45u);
}


/*
 * fd2_animate_tutorial_dialog_intro_or_outro — per-frame "wing" slide math.
 *
 * The function runs 4 frames x 4 corners = 16 corner-sprite blits. Per blit
 * (frame f, corner c) it resolves, against the working composite buffer base
 * (data_fd2_large_game_state_buffer_ptr):
 *     dst    = base + corner_offs[c] / divisor + 0xD430
 *     sprite = atlas + *(int *)(atlas + 6 + (c*2+3)*4)
 *     stride = 0x140
 * where corner_offs = the signed table @ 0x526DA = {-39,-13,13,39} and the
 * divisor RAMP is the load-bearing branch:
 *     OPEN  (open_or_close != 0): divisor = f+1   -> 1,2,3,4 (wings grow)
 *     CLOSE (open_or_close == 0): divisor = 4-f   -> 4,3,2,1 (wings shrink)
 * The divide is SIGNED (x86 IDIV / Watcom int division: truncates toward 0),
 * so e.g. -39/2 == -19 and -13/4 == -3, NOT floor.
 *
 * Risk-driven coverage: the divisor branch (open vs close), the signed
 * truncating division over negative offsets, and the atlas-indexed sprite
 * source are pinned EXACTLY for all 16 blits via the recording blit stub's
 * per-call log (g_blitsetup_*_log). The host-side observables are computed
 * here by an independent reference (signed C division) so the assertion does
 * not merely echo the implementation.
 *
 * Display side-effects (the malloc'd framebuffer backup, the band fill, the
 * per-frame memmove commits to/from the VGA aperture 0xA0000, and the OPEN
 * vs CLOSE final framebuffer restore) are pixel output deferred to Phase 9
 * integration; 0xA0000 is real VGA RAM under DOS/4GW so the memmoves are
 * host-safe scratch. The working buffer is backed by a 64000+ byte host
 * buffer so the in-loop memmove(base, dst, 64000) stays in-bounds.
 */
extern int    g_blitsetup_calls;
extern uint32 g_blitsetup_stride;
extern uint32 g_blitsetup_dst_log[32];
extern uint32 g_blitsetup_sprite_log[32];

/* working composite buffer backing (>= 64000 for the per-frame memmove) */
static uint8 g_wing_lgs[64000];
/* atlas: corner c reads *(int*)(atlas + 6 + (c*2+3)*4); c=3 -> off 42..45,
 * so 64 bytes is in-bounds. Distinct per-corner offset values let the
 * sprite-source resolution be verified per corner. */
static uint8 g_wing_atlas[64];
static const int32 g_wing_corner_off[4] = { -39, -13, 13, 39 };
/* per-corner atlas dword offset values placed at 6 + (c*2+3)*4 */
static const int32 g_wing_atlas_val[4] = { 0x100, 0x200, 0x300, 0x400 };

static void wing_setup(void)
{
    int c;
    memset(g_wing_lgs, 0, sizeof(g_wing_lgs));
    memset(g_wing_atlas, 0, sizeof(g_wing_atlas));
    for (c = 0; c < 4; c++) {
        *(int32 *)(g_wing_atlas + 6 + (c * 2 + 3) * 4) = g_wing_atlas_val[c];
    }
    data_fd2_large_game_state_buffer_ptr = (uint32)g_wing_lgs;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_wing_atlas;
    g_blitsetup_calls = 0;
}

/* Verify all 16 blit (dst, sprite) pairs for one open_or_close value.
 * divisor_of(f) supplies the expected ramp for OPEN vs CLOSE. */
static void wing_check(uint32 open_or_close)
{
    uint32 base;
    uint32 atlas;
    int f, c, k;
    int32 divisor;
    uint32 exp_dst;
    uint32 exp_sprite;

    wing_setup();
    base = (uint32)g_wing_lgs;
    atlas = (uint32)g_wing_atlas;

    fd2_animate_tutorial_dialog_intro_or_outro(open_or_close);

    ASSERT_EQ(g_blitsetup_calls, 16);   /* 4 frames x 4 corners */

    k = 0;
    for (f = 0; f < 4; f++) {
        divisor = (open_or_close != 0) ? (f + 1) : (4 - f);
        for (c = 0; c < 4; c++) {
            exp_dst = base + (uint32)(g_wing_corner_off[c] / divisor) + 0xD430u;
            exp_sprite = atlas + (uint32)g_wing_atlas_val[c];
            ASSERT_EQ(g_blitsetup_dst_log[k], exp_dst);
            ASSERT_EQ(g_blitsetup_sprite_log[k], exp_sprite);
            k++;
        }
    }
    ASSERT_EQ(g_blitsetup_stride, 0x140u);
}

static void test_wing_slide_open_and_close(void)
{
    /* OPEN: divisor ramps 1,2,3,4 (wings grow). Spot anchors on the signed
     * truncating divide: frame0 div1 -> offsets verbatim {-39,-13,13,39};
     * frame1 div2 -> {-19,-6,6,19} (NOT floor: -39/2==-19, -13/2==-6). */
    wing_check(1);

    /* CLOSE: divisor ramps 4,3,2,1 (wings shrink). frame0 div4 ->
     * {-9,-3,3,9} (-39/4==-9, -13/4==-3); frame3 div1 -> verbatim. */
    wing_check(0);
}


/*
 * fd2_animate_scroll_up_in_shop_dialog — staged-shift cadence.
 *
 * The function shifts a 0x4A-row block of the shop-dialog area UP the mode13h
 * framebuffer in three 6-row steps (each followed by a 6-row palette-0x49 fill
 * and a 10ms pace), then a final 8-row shift + 8-row fill. It takes no args,
 * returns nothing, reads no state back; its only host-observable side-effect is
 * the pacing: exactly three __delay_thunk_375b2(10) calls (one per Phase-1
 * step), and none in the final Phase-2 shift. The page-stepping callers depend
 * on that fixed three-beat cadence, so it is pinned here.
 *
 * All memmove/memset traffic targets absolute aperture addresses 0xA8FCA..
 * ~0xAF466 (within mode13h VGA RAM 0xA0000..0xBFFFF, host-safe scratch under
 * DOS/4GW; the memmove also reads uninitialized aperture bytes, harmlessly
 * since nothing reads the result). The actual 0x11C-byte-per-row block scroll
 * is pixel output deferred to Phase 9 integration (no host buffer backs the
 * fixed VGA addresses for read-back), consistent with the other VGA-output
 * animations in this suite.
 */
static void test_shop_scroll_up_cadence(void)
{
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    fd2_animate_scroll_up_in_shop_dialog();
    ASSERT_EQ(g_delay375b2_calls, 3);        /* 3 Phase-1 steps, Phase-2 paces none */
    ASSERT_EQ(g_delay375b2_last_ticks, 10u); /* each step paces 10ms */
}


/*
 * fd2_animate_scroll_down_in_shop_dialog — staged-shift cadence (mirror of
 * scroll_up with reversed direction + backward row iteration).
 *
 * The function shifts the same 0x4A-row shop-dialog block DOWN the mode13h
 * framebuffer in three 6-row steps (each followed by a 6-row palette-0x49 fill
 * of the freed top rows and a 10ms pace), then a final 8-row shift + 8-row fill.
 * It takes no args, returns nothing, reads no state back. Direction is the only
 * structural difference from scroll_up: memmove copies src 0xA8FCA -> dst
 * 0xA974A (and 0xA99CA in Phase 2), and the row loop runs DESCENDING (0x49..0,
 * 0x47..0) so the overlapping copy proceeds high-address-first and never
 * clobbers a not-yet-moved source row.
 *
 * Host-observable side-effect = the pacing: exactly three __delay_thunk_375b2(10)
 * calls (one per Phase-1 step), none in Phase 2. The page-stepping callers depend
 * on that fixed three-beat cadence (identical to scroll_up), so it is pinned
 * here. The actual 0x11C-byte-per-row block scroll, the descending-iteration
 * overlap correctness, and the top-row fill are pixel output bound to the fixed
 * absolute aperture addresses 0xA8FCA.. (mode13h VGA RAM, host-safe scratch under
 * DOS/4GW); no host buffer can back those compile-time-literal addresses for
 * read-back, so that verification is deferred to Phase 9 integration, consistent
 * with the scroll_up counterpart and the other VGA-output animations here.
 */
static void test_shop_scroll_down_cadence(void)
{
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    fd2_animate_scroll_down_in_shop_dialog();
    ASSERT_EQ(g_delay375b2_calls, 3);        /* 3 Phase-1 steps, Phase-2 paces none */
    ASSERT_EQ(g_delay375b2_last_ticks, 10u); /* each step paces 10ms */
}


/*
 * fd2_animate_shop_transaction_feedback — per-state sprite-cycle dispatch +
 * state-4 palette-flash ramp.
 *
 * The function dispatches on data_fd2_chapter_intro_menu_cursor_state and runs
 * a fixed-length sprite cycle per state, with state 4 additionally driving a
 * cyan additive palette ramp. The load-bearing, host-verifiable logic is:
 *
 *   - the 5-way dispatch (states 1/3/4/5 each play a distinct cycle; state 2
 *     and any other value play nothing) — pinned by the blit count + the fixed
 *     framebuffer destination per state;
 *   - the cycle frame counts (5 / 1 / 9 / 7) and the sprite-index ramp
 *     (frame_idx = 0x17 + iter) — pinned by reading each blit's resolved sprite
 *     index out of the real fd2_blit_indexed_sprite_at_xy ->
 *     fd2_rle_blit_sprite spy log against a marker atlas (atlas[6+idx*4]=idx,
 *     so resolved sprite = atlas + idx);
 *   - the post-cycle portrait restore being taken ONLY by states 1/3/4 and
 *     skipped by state 5 / others — pinned by the dialog-blit-primitive spy
 *     call count (the real fd2_paint_portrait_to_dialog_area forwards to it);
 *   - the state-4 palette ramp iteration count: UP 0..0x3E step 2 (32 steps) +
 *     DOWN 0x3E..0 step 2 (32 steps) = 64, each step paced by
 *     __delay_thunk_375b2(4) — pinned by the delay-thunk count + tick value.
 *
 * The frame blits target fixed mode13h aperture addresses and the palette ramp
 * writes the VGA DAC (port 0x3C8/0x3C9 via the real
 * fd2_set_vga_palette_range_with_add); those are display side-effects whose
 * pixel/DAC output is deferred to Phase 9 integration. The per-frame
 * fd2_wait_n_bios_ticks() pacing runs for real against the live BIOS tick
 * counter (host-safe: the counter advances, so each wait terminates); the wait
 * durations are display pacing, also Phase-9 concerns, so they are not asserted.
 *
 * Atlas fixture: marker atlas with atlas[6 + idx*4] = idx for the indices the
 * cycles touch (max 0x1F -> 6+0x1F*4+4 = 134 bytes needed; 256 allocated).
 * Palette fixture: the state-4 ramp reads data_fd2_vga_palette_data_ptr +
 * idx*3 for idx 0..0xFF, so a 768-byte buffer keeps those reads in-bounds.
 */
extern int    g_rle_blit_calls;
extern int    g_rle_blit_log_on;
extern uint32 g_rle_blit_log_sprite[64];
extern uint32 g_rle_blit_log_dst[64];
extern int32  g_rle_blit_log_stride[64];
extern int    g_dlg_blit_normal_calls;
extern int    g_dlg_blit_mirrored_calls;

static uint8 g_shopfb_atlas[256];
static uint8 g_shopfb_pal[768];
static int32 g_shopfb_portrait[64];

static void shopfb_setup(uint32 state)
{
    int i;

    for (i = 0; i < 256; i++) {
        g_shopfb_atlas[i] = 0;
    }
    /* marker: resolved sub-sprite = atlas + atlas[6+idx*4] = atlas + idx */
    for (i = 0x17; i <= 0x1F; i++) {
        *(int32 *)(g_shopfb_atlas + 6 + i * 4) = i;
    }
    for (i = 0; i < 768; i++) {
        g_shopfb_pal[i] = 0x20;
    }
    for (i = 0; i < 64; i++) {
        g_shopfb_portrait[i] = 0;
    }

    data_fd2_chapter_intro_menu_cursor_state = state;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_shopfb_atlas;
    data_fd2_vga_palette_data_ptr = (uint32)g_shopfb_pal;
    data_fd2_portrait_sprite_buffer = (uint8 *)g_shopfb_portrait;
    data_fd2_dialog_active_portrait_blit_offset = 0x728;  /* normal-blit path */

    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;
    g_dlg_blit_normal_calls = 0;
    g_dlg_blit_mirrored_calls = 0;
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
}

/* Verify a contiguous sprite cycle: `count` blits, all to `dst`, stride 0x140,
 * sprite index ramping 0x17, 0x18, ... (recovered as resolved_sprite-atlas). */
static void shopfb_check_cycle(int count, uint32 dst)
{
    uint32 atlas;
    int k;

    atlas = (uint32)g_shopfb_atlas;
    ASSERT_EQ(g_rle_blit_calls, count);
    for (k = 0; k < count; k++) {
        ASSERT_EQ(g_rle_blit_log_dst[k], dst);
        ASSERT_EQ(g_rle_blit_log_stride[k], (int32)0x140);
        /* resolved sprite = atlas + idx, idx = 0x17 + k */
        ASSERT_EQ(g_rle_blit_log_sprite[k] - atlas, (uint32)(0x17 + k));
    }
}

static void test_shop_feedback_state1_cycle(void)
{
    /* state 1: 5-frame cycle at 0xA38E9, frames 0x17..0x1B, then restore. */
    shopfb_setup(1);
    fd2_animate_shop_transaction_feedback();
    shopfb_check_cycle(5, 0xA38E9u);
    ASSERT_EQ(g_dlg_blit_normal_calls, 1);   /* paint_portrait(0) restore */
    ASSERT_EQ(g_dlg_blit_mirrored_calls, 0);
}

static void test_shop_feedback_state3_splash(void)
{
    /* state 3: single splash sprite 0x17 at 0xA3154, then restore. */
    shopfb_setup(3);
    fd2_animate_shop_transaction_feedback();
    shopfb_check_cycle(1, 0xA3154u);
    ASSERT_EQ(g_dlg_blit_normal_calls, 1);   /* paint_portrait(0) restore */
}

static void test_shop_feedback_state4_cycle_and_flash(void)
{
    /* state 4: paint_portrait(3) + 9-frame cycle at 0xA2893 (0x17..0x1F) +
     * cyan palette ramp (UP 32 + DOWN 32 = 64 delay-paced steps) + restore. */
    shopfb_setup(4);
    fd2_animate_shop_transaction_feedback();
    shopfb_check_cycle(9, 0xA2893u);
    /* two portrait paints: mouth-closed (frame 3) before the cycle and the
     * frame-0 restore after the flash; both take the normal-blit path. */
    ASSERT_EQ(g_dlg_blit_normal_calls, 2);
    ASSERT_EQ(g_dlg_blit_mirrored_calls, 0);
    /* ramp UP 0..0x3E step 2 (32) + ramp DOWN 0x3E..0 step 2 (32) = 64 delays */
    ASSERT_EQ(g_delay375b2_calls, 64);
    ASSERT_EQ(g_delay375b2_last_ticks, 4u);
}

static void test_shop_feedback_state5_cycle(void)
{
    /* state 5: 7-frame cycle at 0xA2383 (0x17..0x1D); NO portrait restore. */
    shopfb_setup(5);
    fd2_animate_shop_transaction_feedback();
    shopfb_check_cycle(7, 0xA2383u);
    ASSERT_EQ(g_dlg_blit_normal_calls, 0);   /* state 5 skips paint_portrait(0) */
    ASSERT_EQ(g_dlg_blit_mirrored_calls, 0);
}

static void test_shop_feedback_state_other_noop(void)
{
    /* state 2 (and any non-{1,3,4,5}): no animation, no restore — only the
     * trailing fd2_clear_keyboard_buffer() runs (no blit, no delay). */
    shopfb_setup(2);
    fd2_animate_shop_transaction_feedback();
    ASSERT_EQ(g_rle_blit_calls, 0);
    ASSERT_EQ(g_dlg_blit_normal_calls, 0);
    ASSERT_EQ(g_dlg_blit_mirrored_calls, 0);
    ASSERT_EQ(g_delay375b2_calls, 0);
}


/*
 * fd2_animate_party_addition_with_appear_effect — 12-frame appearance-animation
 * control-flow skeleton (frame dispatch + SFX timing + composite branches).
 *
 * The function is a per-newly-joined-char "explosion appearance" animation
 * orchestrator. Its load-bearing, host-verifiable logic is the fixed 12-frame
 * loop and its per-frame dispatch:
 *   - frame 1 only: one fd2_play_sfx_with_handle (the appearance chime);
 *   - frame 6: redraw-from-backup + char repaint, NO tile-map composite;
 *   - frame 7 AND frame 8: each run exactly one fd2_composite_battle_tile_map
 *     (the only two tile-map composites in the whole run);
 *   - every frame: the per-new-char explosion-sprite blit loop runs over
 *     [old_count, party_count); with zero newly-added chars it emits zero
 *     fd2_blit_sprite_with_decoded_pixels calls.
 *
 * This case pins that skeleton deterministically by adding NO new chars: the
 * real fd2_load_chapter_portraits_and_dump_tmp is driven with
 * portrait_cache_alloc_offset == 0 (its race-scan loop body never runs, exactly
 * the test_pt_empty_table path in tests/rsrc/rsrc.c), so party_member_count
 * stays 0 and old_count == party_count. The two FDOTHER.DAT resource loads
 * (sfx idx 0x5F, explosion-sprite idx 9) run for real against the staged real
 * FDOTHER.DAT through the real fd2_load_dat_resource; the per-frame BIOS-tick
 * waits and keyboard-buffer clears run for real (host-safe: the tick counter
 * advances so each wait terminates). Observed via the recording spies:
 *   - g_play_sfx_with_handle_calls == 1   (frame-1 chime, fired once)
 *   - g_tile_map_calls            == 2    (frames 7 + 8)
 *   - g_blitdec_calls             == 0    (no new chars -> no explosion blits)
 * and the function returns (its three heap buffers are freed; no hang/leak).
 *
 * The per-new-char explosion-blit window-clip + dst-offset arithmetic (the
 * (pos_y-oy)*0x2AC0 + (pos_x-ox-1)*0x18 + 0x75D8 placement, and the frame-6/7
 * large_game_state_buffer pointer-shift composite trick) is pure VGA-output
 * positioning reachable only after the real fd2_init_runtime_char_for_battle
 * chain spawns in-window chars at FDFIELD-determined positions (which also
 * drags in the tile-map / tile-attr / portrait-cache redraw fixtures of the
 * frame-6/7/8 paint + shadow-overlay paths). That is deferred to Phase 9
 * integration (real chapter-1 party assembly), consistent with the other
 * VGA-output side-effects deferred across this suite; see src/emit_issues.json
 * (00032999). The frame blits land in the mode13h aperture (0xA0504) via the
 * real fd2_blit_rectangle; large_game_state_buffer is backed by a >= 0x25680
 * host buffer so the per-frame memmove(base, backup, 0x25680) stays in-bounds,
 * and portrait_sprite_cache is backed by a 0x32A00 buffer so the portrait
 * loader's FD2.TMP fwrite reads valid memory.
 */
static uint8 g_pa_lgs[0x26000];
static uint32 g_pa_saved_cache;
static uint32 g_pa_saved_chapter;
static uint32 g_pa_saved_alloc_off;
static uint32 g_pa_saved_count;

static void party_add_setup(void)
{
    memset(g_pa_lgs, 0, sizeof(g_pa_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_pa_lgs;

    /* portrait loader's FD2.TMP fwrite reads 0x32A00 bytes from this cache */
    g_pa_saved_cache = portrait_sprite_cache;
    portrait_sprite_cache = (uint32)malloc(0x32A00);

    g_pa_saved_chapter = data_fd2_chapter_current_chapter_id;
    g_pa_saved_alloc_off = data_fd2_resource_portrait_cache_alloc_offset;
    g_pa_saved_count = data_fd2_battle_party_member_count;

    /* chapter 1 -> FDFIELD re-read idx 1*3+2 = 5 (valid in the staged file) */
    data_fd2_chapter_current_chapter_id = 1;
    /* zero race-scan length -> portrait loader adds no chars (count stays 0) */
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;   /* loaded fresh by the loader */
    data_fd2_battle_party_member_count = 0;

    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x20;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;

    g_play_sfx_with_handle_calls = 0;
    g_tile_map_calls = 0;
    g_composite_call_count = 0;
    g_blitdec_calls = 0;
}

static void party_add_teardown(void)
{
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
    }
    portrait_sprite_cache = g_pa_saved_cache;
    data_fd2_chapter_current_chapter_id = g_pa_saved_chapter;
    data_fd2_resource_portrait_cache_alloc_offset = g_pa_saved_alloc_off;
    data_fd2_battle_party_member_count = g_pa_saved_count;
    chapter_portrait_load_buffer = 0;
    remove("FD2.TMP");   /* generated swap file (not a staged game file) */
}

static void test_party_add_frame_skeleton_and_sfx(void)
{
    party_add_setup();
    fd2_animate_party_addition_with_appear_effect(1);

    /* frame-1 appearance chime fires exactly once across the 12 frames */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    /* only frames 7 and 8 run a tile-map composite */
    ASSERT_EQ(g_tile_map_calls, 2);
    ASSERT_EQ(g_composite_call_count, 2);
    /* zero newly-added chars -> the per-frame explosion-blit loop is empty */
    ASSERT_EQ(g_blitdec_calls, 0);

    party_add_teardown();
}


/*
 * fd2_cinematic_warp_char_to_tile — argument-routing dispatcher.
 *
 * A 3-line cinematic wrapper with no internal branching or computation: it
 * pans the cursor to the target tile, then plays the warp-teleport char
 * animation onto that tile. The load-bearing, host-verifiable logic is the
 * argument routing through its two callees:
 *
 *   - fd2_pan_cursor_to_tile_animated(tile_x, tile_y) — the X/Y order must be
 *     correct. Pinned by pre-seating the cursor exactly at the target (with
 *     tile_x != tile_y) so the real pan's two while-loops match immediately and
 *     leave the cursor where it is. A swapped pan(tile_y, tile_x) would instead
 *     see cursor_world_x(=tile_x) != target_x(=tile_y) and step the cursor away,
 *     so an unchanged (tile_x, tile_y) cursor confirms the order.
 *   - fd2_animate_warp_teleport_char(char_id, tile_x, tile_y, tile_x, tile_y) —
 *     the defining "appear at target" semantics is that the source and
 *     destination tile coords are BOTH the target (src == dst). Pinned exactly
 *     via the recording spy in testglob.c, which captures all 5 received args.
 *
 * The pan callee is the real emitted routine (src/ui_menu/cursor.c); its initial
 * fd2_composite_battle_frame(0) is made host-safe by the same recipe the
 * screen-shake test uses (LGS buffer backed, view window set, HUD/play gated
 * off, empty party, palette cycle throttled, anim_phase==0 -> no cursor overlay
 * and no per-step BIOS wait). The teleport callee's actual malloc / FDOTHER.DAT
 * load / portal+collapse+expand blit sequence is pure VGA-output animation
 * deferred to Phase 9 integration; here it is the no-op recording spy.
 */
extern int    g_warp_teleport_calls;
extern uint32 g_warp_teleport_arg[5];

/* LGS backing for the real fd2_composite_battle_frame(0) reached by the pan's
 * opening composite; sized exactly like the screen-shake fixture. */
static uint8 g_cinwarp_lgs[SHAKE_LGS_SPAN];

/* Seat the cursor at (tx, ty) so the real pan matches immediately, and make the
 * opening composite host-safe. anim_phase==0 keeps the cursor overlay empty and
 * skips the per-step bios wait; large map dims + small screen coords keep the
 * cursor-move clip branches benign even if a loop were to step. */
static void cinwarp_setup(uint32 tx, uint32 ty)
{
    memset(g_cinwarp_lgs, 0, sizeof(g_cinwarp_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_cinwarp_lgs;

    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x20;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;

    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;
    data_fd2_battle_party_member_count = 0;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_map_width_tiles = 0x100;
    data_fd2_battle_map_height_tiles = 0x100;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_cursor_world_x = tx;   /* already at target -> pan no-op */
    data_fd2_battle_cursor_world_y = ty;

    g_warp_teleport_calls = 0;
    g_warp_teleport_arg[0] = 0;
    g_warp_teleport_arg[1] = 0;
    g_warp_teleport_arg[2] = 0;
    g_warp_teleport_arg[3] = 0;
    g_warp_teleport_arg[4] = 0;
}

static void cinwarp_check(uint32 char_id, uint32 tx, uint32 ty)
{
    cinwarp_setup(tx, ty);
    fd2_cinematic_warp_char_to_tile(char_id, tx, ty);

    /* pan got (tx, ty) in the right order: cursor (seated at target) unmoved */
    ASSERT_EQ(data_fd2_battle_cursor_world_x, tx);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, ty);

    /* warp-teleport got (char_id, tx, ty, tx, ty): src == dst == target tile */
    ASSERT_EQ(g_warp_teleport_calls, 1);
    ASSERT_EQ(g_warp_teleport_arg[0], char_id);
    ASSERT_EQ(g_warp_teleport_arg[1], tx);
    ASSERT_EQ(g_warp_teleport_arg[2], ty);
    ASSERT_EQ(g_warp_teleport_arg[3], tx);
    ASSERT_EQ(g_warp_teleport_arg[4], ty);
}

static void test_cinematic_warp_arg_routing(void)
{
    /* chapter-30 init call shape: char 5 -> tile (0x15, 5). tx != ty pins the
     * pan X/Y order; the spy pins the src==dst teleport duplication. */
    cinwarp_check(5, 0x15, 5);

    /* event-handler call shape: char 0x18 -> tile (0x16, 0x12). Distinct values
     * (and not hardcoded) re-confirm the routing on a second tuple. */
    cinwarp_check(0x18, 0x16, 0x12);
}


/*
 * fd2_animate_palette_flash_pulse_white — full pulse-white DAC fade cadence.
 *
 * No params, straight-line: fade the whole DAC up to white, hold at peak, fade
 * back down. The load-bearing, host-verifiable logic is the exact step cadence,
 * which pins both loop bounds and the mid-hold:
 *
 *   - fade UP   brightness 0..0x3F (64 steps, INC), each paced __delay_thunk_375b2(8)
 *   - hold      one __delay_thunk_375b2(400) at peak brightness
 *   - fade DOWN brightness 0x3E..0 (63 steps, DEC), each paced __delay_thunk_375b2(8)
 *
 * => exactly 64 + 1 + 63 = 128 delay calls; the final delay (fade-DOWN tail) is
 * 8ms, so g_delay375b2_last_ticks == 8. The 128 count is unique to these two
 * inclusive bounds (a 0..0x3E up-loop or a 0x3F..0 down-loop would not total
 * 128), so it pins the whole control flow.
 *
 * The actual DAC writes go through the real fd2_set_vga_palette_range_with_add
 * (port 0x3C8/0x3C9), reading data_fd2_vga_palette_data_ptr + idx*3 for
 * idx 0..0xFF; a 768-byte palette buffer keeps those reads in-bounds and the
 * port writes are host-safe under the emulated VGA. The emitted DAC pixel output
 * itself is a display side-effect deferred to Phase 9 integration, consistent
 * with the sibling fd2_animate_shop_transaction_feedback state-4 ramp test.
 */
static uint8 g_flash_pal[768];

static void test_palette_flash_pulse_white_cadence(void)
{
    int i;

    for (i = 0; i < 768; i++) {
        g_flash_pal[i] = 0x20;
    }
    data_fd2_vga_palette_data_ptr = (uint32)g_flash_pal;

    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;

    fd2_animate_palette_flash_pulse_white();

    /* UP 64 (0..0x3F) + hold 1 + DOWN 63 (0x3E..0) = 128 delay-paced steps */
    ASSERT_EQ(g_delay375b2_calls, 128);
    /* last paced delay is the fade-DOWN tail step (8ms), not the 400ms hold */
    ASSERT_EQ(g_delay375b2_last_ticks, 8u);
}


void run_anim_aniui_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/aniui\n");
    RUN_TEST(test_tick_tutorial_sfx_counter);
    RUN_TEST(test_screen_shake_loop_and_jitter);
    RUN_TEST(test_money_increment_roll_and_total);
    RUN_TEST(test_money_decrement_roll_and_total);
    RUN_TEST(test_wing_slide_open_and_close);
    RUN_TEST(test_shop_scroll_up_cadence);
    RUN_TEST(test_shop_scroll_down_cadence);
    RUN_TEST(test_shop_feedback_state1_cycle);
    RUN_TEST(test_shop_feedback_state3_splash);
    RUN_TEST(test_shop_feedback_state4_cycle_and_flash);
    RUN_TEST(test_shop_feedback_state5_cycle);
    RUN_TEST(test_shop_feedback_state_other_noop);
    RUN_TEST(test_party_add_frame_skeleton_and_sfx);
    RUN_TEST(test_cinematic_warp_arg_routing);
    RUN_TEST(test_palette_flash_pulse_white_cadence);
    printf("\n");
}
