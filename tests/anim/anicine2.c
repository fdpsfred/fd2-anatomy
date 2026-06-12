/*
 * unit tests for src/anim/anicine.c (part 2)
 *
 * fd2_play_figani_animation_loop @ 0x2B659 — the master FIGANI animation
 * player. These tests pin the function's risk-bearing CONTROL FLOW (branch
 * gates that are not pure display side-effects):
 *
 *   - the per-pose special-skill SFX-hook gate (spell_id in {0x18,0x1C..0x1E}
 *     AND pose[+5] != 0);
 *   - the per-sub-frame composite ORDER + inclusion gate (keyed on the caster's
 *     team and spell_id < 10 || spell_id == 0x1C);
 *   - the target-FIGANI pose-cycle advance/wrap counters;
 *   - the spell-cast-frame remap_idx selection (spell_id -> 0x13 / 0x0F / 0x0B)
 *     observed numerically through the remap-table address handed to the
 *     palette-remap RLE blit.
 *
 * The composite is driven with synthetic FIGANI byte streams (no real DAT) so
 * the loop bounds and per-pose metadata are exact and host-independent; the
 * indexed-sprite blits route through the testglob spy (atlas/frame logged), and
 * the RLE palette-remap blit through its recording stub. The real callees that
 * the loop drives (fd2_blit_rectangle row-copies, fd2_deduct_caster_mp,
 * fd2_flash_char_hit_sprite, fd2_wait_n_bios_ticks) run for real over
 * in-memory buffers + the zeroed runtime-char / spell tables.
 *
 * The pure pixel/DAC display side-effects of the spell-cast block (the actual
 * remapped RLE pixels and the DAC index-0 flash sequence) are not host-
 * observable and are left to Phase 9 integration; only the branch that SELECTS
 * the remap table is asserted here.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "minipfix.h"

extern runtime_char g_test_rc_array[8];

/* SFX spy (testglob.c): per-call id log. */
extern int    g_play_sfx_with_handle_calls;
extern int    g_sfx_id_count;
extern int    g_sfx_id_log[64];

/* indexed-sprite spy ordered atlas/frame log (testglob.c). */
extern int    g_blit_indexed_sprite_calls;
extern int    g_blit_indexed_log_on;
extern int    g_blit_indexed_log_count;
extern uint32 g_blit_indexed_atlas_log[64];
extern uint32 g_blit_indexed_frame_log[64];

/* palette-remap RLE blit recording stub (testglob.c). */
extern int    g_rle_remap_calls;
extern int    g_rle_remap_log_count;
extern int32  g_rle_remap_log_palette[16];
extern int32  g_rle_remap_log_dstx[16];
extern int32  g_rle_remap_log_dsty[16];

extern int    g_delay375b2_calls;

/* Two real-blit work buffers sized as the game allocates them: the 128 KB
 * composite workspace and the 64 KB framebuffer scratch the loop row-copies
 * between (fd2_blit_rectangle reads 0xC8 rows of 0x140 bytes = 0xFA00).
 * g_fa_dstbuf is also the dst the spell-cast block hands to the real
 * fd2_flash_char_hit_sprite mini-panel painter (the binary forwards dst_buf,
 * arg6); 64000 bytes covers the panel at team-2 offset 0x05AB and the
 * team-0 offset 0xC080 (0xC080 + panel extent < 64000). */
static uint8 g_fa_workspace[0x1F400];
static uint8 g_fa_dstbuf[64000];

/* Caster FIGANI byte stream: holds the synthetic pose metadata the loop walks.
 * (The mini-panel write goes to dst_buf, NOT here — the loop forwards arg6.) */
static uint8 g_fa_caster_fig[0x10000];
static uint8 g_fa_target_fig[256];
/* Synthetic tile-anim base table for the remap_idx selection test. */
static uint8 g_fa_tileanim[256];

#define FA_WIN_OX 0x10u
#define FA_WIN_OY 0x10u

/* Build a FIGANI stream into `buf`: `npose` poses, pose i described by
 * meta[i] = {type, sfx, subframes}. Header byte +2 = pose count (loop bound);
 * byte +0 = pose count too (the target wrap reads [0]). The per-pose offset
 * table (int32) sits at +8; each pose entry is laid out sequentially after the
 * table with its [+4]/[+5]/[+6] metadata bytes. Returns nothing. */
static void fa_build_figani(uint8 *buf, int npose, const uint8 meta[][3])
{
    int   i;
    uint32 entry_base;
    uint32 off;

    memset(buf, 0, 8 + npose * 4 + npose * 16);
    buf[0] = (uint8)npose;       /* pose count (target wrap reads [0]) */
    buf[2] = (uint8)npose;       /* pose count (caster loop bound is [2]) */
    entry_base = (uint32)(8 + npose * 4);
    for (i = 0; i < npose; i++) {
        off = entry_base + (uint32)i * 16;
        memcpy(buf + 8 + i * 4, &off, 4);   /* relative offset to pose i */
        buf[off + 4] = meta[i][0];          /* type marker */
        buf[off + 5] = meta[i][1];          /* SFX hook id */
        buf[off + 6] = meta[i][2];          /* sub-frame count */
    }
}

/* Reset the runtime char + spies for an animation-loop drive. caster idx 0. */
static void fa_setup(uint8 caster_team, uint8 chapter)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = FA_WIN_OX + 1;
    g_test_rc_array[0].pos_y = FA_WIN_OY + 1;
    g_test_rc_array[0].team = caster_team;
    g_test_rc_array[0].mp_current = 0x100;   /* drained by real fd2_deduct_caster_mp */
    g_test_rc_array[0].hp_current = 100;
    g_test_rc_array[0].hp_max = 100;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_chapter_current_chapter_id = chapter;

    memset(g_fa_workspace, 0, sizeof(g_fa_workspace));
    memset(g_fa_dstbuf, 0, sizeof(g_fa_dstbuf));

    g_play_sfx_with_handle_calls = 0;
    g_sfx_id_count = 0;
    memset(g_sfx_id_log, 0, sizeof(g_sfx_id_log));
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_log_on = 1;
    g_blit_indexed_log_count = 0;
    memset(g_blit_indexed_atlas_log, 0, sizeof(g_blit_indexed_atlas_log));
    memset(g_blit_indexed_frame_log, 0, sizeof(g_blit_indexed_frame_log));
    g_rle_remap_calls = 0;
    g_rle_remap_log_count = 0;
    g_delay375b2_calls = 0;

    /* a real SFX-bank handle pointer (the spy ignores it) so the gate-on calls
     * forward a non-zero bank; value is opaque to the loop. */
    data_fd2_audio_figani_sfx_bank_buf_ptr = 0xBEEF;
    data_fd2_audio_summon_spell_sfx_bank_buf_ptr = 0xCAFE;
}

/* Drive the loop with the standard 2-pose target FIGANI (each pose 1 subframe,
 * so the target pose index advances by 1 every subframe and wraps at 2). */
static void fa_build_target_2pose(void)
{
    static const uint8 tmeta[2][3] = { {0, 0, 1}, {0, 0, 1} };
    fa_build_figani(g_fa_target_fig, 2, tmeta);
}

static void fa_run(uint32 spell_id)
{
    fd2_play_figani_animation_loop(0, spell_id,
        g_fa_caster_fig, g_fa_target_fig,
        (uint32)g_fa_workspace, (uint32)g_fa_dstbuf,
        (uint32)g_fa_caster_fig /* bg_layer_a (unused on type-0 poses) */,
        (uint32)g_fa_caster_fig /* bg_layer_b */);
}

/*
 * Special-skill SFX-hook gate FIRES: spell_id 0x18 (a gate member) with pose 0
 * carrying SFX hook 7 and pose 1 hook 0. Exactly one fire (id 7) from the
 * per-pose gate; pose 1's zero hook is skipped. Type-0 poses -> no spell-cast
 * block, so no summon SFX contaminates the log.
 */
static void test_figani_sfx_gate_fires(void)
{
    static const uint8 cmeta[2][3] = { {0, 7, 1}, {0, 0, 1} };

    fa_build_figani(g_fa_caster_fig, 2, cmeta);
    fa_build_target_2pose();
    fa_setup(2, 1);
    fa_run(0x18);

    ASSERT_EQ(g_sfx_id_count, 1);
    ASSERT_EQ(g_sfx_id_log[0], 7);
}

/*
 * Gate member 0x1C (in 0x1C..0x1E) also opens the SFX gate -> the hook fires.
 * Confirms the upper window arm of the (0x1B < id < 0x1F) condition, not just
 * the id == 0x18 literal.
 */
static void test_figani_sfx_gate_fires_0x1c(void)
{
    static const uint8 cmeta[2][3] = { {0, 9, 1}, {0, 0, 1} };

    fa_build_figani(g_fa_caster_fig, 2, cmeta);
    fa_build_target_2pose();
    fa_setup(2, 1);
    fa_run(0x1C);

    ASSERT_EQ(g_sfx_id_count, 1);
    ASSERT_EQ(g_sfx_id_log[0], 9);
}

/*
 * Gate CLOSED: spell_id 5 is neither 0x18 nor in 0x1C..0x1E, so the per-pose
 * SFX hook is suppressed for every pose despite pose 0's non-zero hook. With
 * type-0 poses there is no other SFX source -> zero fires.
 */
static void test_figani_sfx_gate_closed(void)
{
    static const uint8 cmeta[2][3] = { {0, 7, 1}, {0, 0, 1} };

    fa_build_figani(g_fa_caster_fig, 2, cmeta);
    fa_build_target_2pose();
    fa_setup(2, 1);
    fa_run(5);

    ASSERT_EQ(g_sfx_id_count, 0);
}

/*
 * Composite order, caster team == 0 (enemy), spell_id < 10: each sub-frame
 * blits the TARGET pose first (gated on spell_id<10||==0x1C) then the CASTER
 * pose. Two poses x 1 sub-frame -> the ordered atlas log is
 * [target, caster, target, caster] with target frames following the pose-cycle
 * (0 then 1) and caster frames = the pose index (0 then 1).
 */
static void test_figani_blit_order_team0(void)
{
    static const uint8 cmeta[2][3] = { {0, 0, 1}, {0, 0, 1} };
    uint32 tfig = (uint32)g_fa_target_fig;
    uint32 cfig = (uint32)g_fa_caster_fig;

    fa_build_figani(g_fa_caster_fig, 2, cmeta);
    fa_build_target_2pose();
    fa_setup(0, 1);
    fa_run(5);

    ASSERT_EQ(g_blit_indexed_log_count, 4);
    /* sub-frame 0 (pose 0): target@0 then caster@0 */
    ASSERT_EQ((long)g_blit_indexed_atlas_log[0], (long)tfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[0], 0L);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[1], (long)cfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[1], 0L);
    /* sub-frame 1 (pose 1): target@1 then caster@1 */
    ASSERT_EQ((long)g_blit_indexed_atlas_log[2], (long)tfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[2], 1L);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[3], (long)cfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[3], 1L);
}

/*
 * Composite order, caster team != 0 (player), spell_id < 10: the order REVERSES
 * -- caster pose first, then the (gated) target pose. Same FIGANI as the team-0
 * case, so the only difference is the team branch.
 */
static void test_figani_blit_order_team_nonzero(void)
{
    static const uint8 cmeta[2][3] = { {0, 0, 1}, {0, 0, 1} };
    uint32 tfig = (uint32)g_fa_target_fig;
    uint32 cfig = (uint32)g_fa_caster_fig;

    fa_build_figani(g_fa_caster_fig, 2, cmeta);
    fa_build_target_2pose();
    fa_setup(2, 1);
    fa_run(5);

    ASSERT_EQ(g_blit_indexed_log_count, 4);
    /* sub-frame 0: caster@0 then target@0 */
    ASSERT_EQ((long)g_blit_indexed_atlas_log[0], (long)cfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[0], 0L);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[1], (long)tfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[1], 0L);
    /* sub-frame 1: caster@1 then target@1 */
    ASSERT_EQ((long)g_blit_indexed_atlas_log[2], (long)cfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[2], 1L);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[3], (long)tfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[3], 1L);
}

/*
 * spell_id gate skips the TARGET pose: spell_id 15 (>= 10, != 0x1C) with team
 * != 0 -> only the caster pose blits each sub-frame (the target-pose inclusion
 * gate is false). Two sub-frames -> exactly 2 indexed blits, both the caster
 * atlas at frames 0 then 1.
 */
static void test_figani_target_blit_gated_off(void)
{
    static const uint8 cmeta[2][3] = { {0, 0, 1}, {0, 0, 1} };
    uint32 tfig = (uint32)g_fa_target_fig;
    uint32 cfig = (uint32)g_fa_caster_fig;
    int    i;

    fa_build_figani(g_fa_caster_fig, 2, cmeta);
    fa_build_target_2pose();
    fa_setup(2, 1);
    fa_run(15);

    ASSERT_EQ(g_blit_indexed_log_count, 2);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[0], (long)cfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[0], 0L);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[1], (long)cfig);
    ASSERT_EQ((long)g_blit_indexed_frame_log[1], 1L);
    /* the target atlas never appears */
    for (i = 0; i < g_blit_indexed_log_count; i++) {
        ASSERT_TRUE((long)g_blit_indexed_atlas_log[i] != (long)tfig);
    }
}

/*
 * Target FIGANI pose-cycle wrap: a single caster pose with 5 sub-frames over a
 * 2-pose target (1 sub-frame each). The target frame index advances 0,1,0,1,0
 * across the five sub-frames (used at sub-frame START, before the advance), so
 * the recorded target-blit frame sequence is exactly that. team != 0, spell<10
 * so caster-then-target order; the target blit is index 1,3,5,7,9 in the log.
 */
static void test_figani_target_pose_cycle_wrap(void)
{
    static const uint8 cmeta[1][3] = { {0, 0, 5} };
    uint32 tfig = (uint32)g_fa_target_fig;
    int    expect[5];
    int    seen;
    int    i;

    fa_build_figani(g_fa_caster_fig, 1, cmeta);
    fa_build_target_2pose();
    fa_setup(2, 1);
    fa_run(5);

    expect[0] = 0; expect[1] = 1; expect[2] = 0; expect[3] = 1; expect[4] = 0;
    /* 5 sub-frames x (caster + target) = 10 blits */
    ASSERT_EQ(g_blit_indexed_log_count, 10);
    seen = 0;
    for (i = 0; i < g_blit_indexed_log_count; i++) {
        if (g_blit_indexed_atlas_log[i] == tfig) {
            ASSERT_EQ((long)g_blit_indexed_frame_log[i], (long)expect[seen]);
            seen++;
        }
    }
    ASSERT_EQ(seen, 5);
}

/* ----- spell-cast-frame remap_idx selection ----- */

/* Place a distinct sentinel int32 at table[remap_idx*4 + 6] for each of the
 * three remap_idx values, so one synthetic base table serves all three cases.
 * The loop computes remap_table = base + table[remap_idx*4 + 6]. */
#define FA_OFF_0B  (0x0B * 4 + 6)
#define FA_OFF_0F  (0x0F * 4 + 6)
#define FA_OFF_13  (0x13 * 4 + 6)
#define FA_VAL_0B  0x0000B000L
#define FA_VAL_0F  0x0000F000L
#define FA_VAL_13  0x00013000L

static void fa_build_tileanim(void)
{
    int32 v;

    memset(g_fa_tileanim, 0, sizeof(g_fa_tileanim));
    v = (int32)FA_VAL_0B; memcpy(g_fa_tileanim + FA_OFF_0B, &v, 4);
    v = (int32)FA_VAL_0F; memcpy(g_fa_tileanim + FA_OFF_0F, &v, 4);
    v = (int32)FA_VAL_13; memcpy(g_fa_tileanim + FA_OFF_13, &v, 4);
    data_fd2_tile_anim_table_base = (uint32)g_fa_tileanim;
}

/* Drive ONE spell-cast (type==1) pose with the given spell_id (must be < 10 so
 * the palette/remap block fires) and assert the remap table passed to BOTH
 * palette-remap RLE blits equals base + expected_off. The first blit carries
 * dst_x=0,dst_y=0x32 (bg_layer_a); the second dst_x=0xA4,dst_y=0x9D (bg_layer_b)
 * -- both share the one computed remap_table. */
static void run_remap_case(uint32 spell_id, int32 expected_val)
{
    static const uint8 cmeta[1][3] = { {1, 0, 1} };  /* type 1, no hook, 1 subframe */
    uint32 expected_remap;

    fa_build_figani(g_fa_caster_fig, 1, cmeta);
    fa_build_target_2pose();
    fa_build_tileanim();
    /* mini-panel fixture for the real fd2_flash_char_hit_sprite. The binary
     * forwards dst_buf (arg6) as the painter's dst, so with caster team != 0
     * the panel lands at dst_buf + 0x05AB; g_fa_dstbuf (64000 B) absorbs it. */
    minip_setup_env();
    fa_setup(2, 1);
    fa_run(spell_id);

    /* the loop adds the int32 STORED at table[remap_idx*4+6] (= expected_val) to
     * the base, so remap_table == base + expected_val. */
    expected_remap = data_fd2_tile_anim_table_base + (uint32)expected_val;

    /* two remap blits issued for the single spell-cast pose */
    ASSERT_EQ(g_rle_remap_calls, 2);
    /* blit 0 -> bg_layer_a at (0, 0x32); blit 1 -> bg_layer_b at (0xA4, 0x9D) */
    ASSERT_EQ((long)g_rle_remap_log_dstx[0], 0L);
    ASSERT_EQ((long)g_rle_remap_log_dsty[0], 0x32L);
    ASSERT_EQ((long)g_rle_remap_log_dstx[1], 0xA4L);
    ASSERT_EQ((long)g_rle_remap_log_dsty[1], 0x9DL);
    /* both carry the remap table selected by remap_idx */
    ASSERT_EQ((long)(uint32)g_rle_remap_log_palette[0], (long)expected_remap);
    ASSERT_EQ((long)(uint32)g_rle_remap_log_palette[1], (long)expected_remap);

    /* palette_write_countdown armed to 6 -> the single sub-frame paints DAC
     * index 0 and runs the 0x1E-tick delay once (the countdown decrements). */
    ASSERT_EQ(g_delay375b2_calls, 1);

    /* Pin the spell-cast-frame fd2_flash_char_hit_sprite buffer routing: the
     * binary passes dst_buf (arg6), NOT caster_figani (arg3). The real painter
     * blits its background to buf = dst_buf + screen_off first, and the dialog
     * blit spy records that dst. Caster team 2 (non-zero), chapter 1, idx 0 ->
     * screen_off 0x05AB, so the panel must land inside g_fa_dstbuf, proving the
     * routing rather than masking it behind a large caster_figani buffer. */
    ASSERT_EQ((long)g_dlg_blit_last_dst, (long)((uint32)g_fa_dstbuf + 0x05ABu));
}

/*
 * remap_idx = 0x13 for spell_id 8 (the {8,0x20,0x21} arm). spell_id 8 < 10 so
 * the palette/remap block fires; the table index taken is 0x13.
 */
static void test_figani_remap_idx_0x13(void)
{
    run_remap_case(8, FA_VAL_13);
}

/*
 * remap_idx = 0x0F for spell_id 5 (the (int)spell_id > 3 arm). 3 < 5 < 8 and
 * 5 < 10, so the 0x0F table index is selected and the block fires.
 */
static void test_figani_remap_idx_0x0f(void)
{
    run_remap_case(5, FA_VAL_0F);
}

/*
 * remap_idx = 0x0B (default) for spell_id 2: not in {8,0x20,0x21}, not > 3, not
 * 0x23 -> the default 0x0B index. 2 < 10 so the block still fires.
 */
static void test_figani_remap_idx_default_0x0b(void)
{
    run_remap_case(2, FA_VAL_0B);
}

/* ===== fd2_step_figani_pose_animation @ 0x2B9A1 =====
 *
 * The per-frame pose-loop stepper. These tests pin its risk-bearing piece:
 * the (pose_idx, subframe_idx) state machine and its three exits
 * (within-pose advance / pose advance with subframe reset / wrap-to-start
 * after the last pose), plus the palette_op == 0 explicit rewind path.
 *
 * A synthetic FIGANI stream gives exact per-pose sub-frame counts (the
 * function reads the pose count at byte [0] and each pose's sub-frame count
 * at pose_block[+6]); fa_build_figani already lays both out. The indexed
 * sprite blit routes through the testglob spy, whose last_frame / last_x /
 * last_y record the pose index, dst_buf and dst_stride the stepper forwards.
 * The actual painted pixels are a pure display side-effect (Phase 9). */

extern uint8 data_fd2_graphics_figani_pose_anim_subframe_idx;
extern uint8 data_fd2_graphics_figani_pose_anim_pose_idx;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int    g_blit_indexed_sprite_last_x;
extern int    g_blit_indexed_sprite_last_y;

/* Caster stream for the stepper: pose 0 has 2 sub-frames, pose 1 has 1. */
static void fs_build_2pose_2then1(void)
{
    static const uint8 meta[2][3] = { {0, 0, 2}, {0, 0, 1} };
    fa_build_figani(g_fa_caster_fig, 2, meta);
}

/* Zero the two stepper globals + the blit spy before a fresh walk. */
static void fs_reset(void)
{
    data_fd2_graphics_figani_pose_anim_pose_idx = 0;
    data_fd2_graphics_figani_pose_anim_subframe_idx = 0;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_log_on = 0;
    g_blit_indexed_sprite_last_frame = 0xFFFF;
    g_blit_indexed_sprite_last_x = 0;
    g_blit_indexed_sprite_last_y = 0;
}

/*
 * Walks the full 2-pose / (2,1)-subframe loop a step at a time and pins the
 * (pose_idx, subframe_idx) pair plus the blit's pose-index argument after
 * each call, covering every exit:
 *   start (0,0)
 *   step1 -> within pose 0:      blit pose 0, (0,1)
 *   step2 -> pose 0 done:        blit pose 0, advance to pose 1, (1,0)
 *   step3 -> last pose done:     blit pose 1, wrap to start, (0,0)
 * The blit's frame arg is the pose index sampled BEFORE any advance, so the
 * recorded sequence is 0, 0, 1.
 */
static void test_step_figani_pose_walk(void)
{
    uint32 fig;

    fs_build_2pose_2then1();
    fs_reset();
    fig = (uint32)g_fa_caster_fig;

    /* step 1: blit pose 0, sub-frame 0 -> 1, still inside pose 0 */
    fd2_step_figani_pose_animation(fig, 1, 0xA0000, 0x140);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 0L);   /* pose idx pre-advance */
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, (long)0xA0000);  /* dst_buf */
    ASSERT_EQ((long)g_blit_indexed_sprite_last_y, 0x140L);         /* dst_stride */
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_pose_idx, 0);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_subframe_idx, 1);

    /* step 2: blit pose 0, sub-frame 1 -> 2 == sub_count -> advance to pose 1 */
    fd2_step_figani_pose_animation(fig, 1, 0xA0000, 0x140);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 2);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 0L);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_pose_idx, 1);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_subframe_idx, 0);

    /* step 3: blit pose 1, sub-frame 0 -> 1 == sub_count -> pose 2 == count -> wrap */
    fd2_step_figani_pose_animation(fig, 1, 0xA0000, 0x140);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 3);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 1L);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_pose_idx, 0);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_subframe_idx, 0);
}

/*
 * palette_op == 0 is the explicit rewind: from a mid-loop state (pose 1,
 * sub-frame 0) a zero call must reset BOTH counters to 0 and issue NO blit.
 */
static void test_step_figani_rewind_on_zero(void)
{
    uint32 fig;

    fs_build_2pose_2then1();
    fs_reset();
    fig = (uint32)g_fa_caster_fig;

    /* advance two steps into the loop (lands at pose 1, sub-frame 0) */
    fd2_step_figani_pose_animation(fig, 1, 0xA0000, 0x140);
    fd2_step_figani_pose_animation(fig, 1, 0xA0000, 0x140);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_pose_idx, 1);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 2);

    /* rewind: no blit, both counters back to 0 */
    fd2_step_figani_pose_animation(fig, 0, 0xA0000, 0x140);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 2);   /* unchanged: no blit on rewind */
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_pose_idx, 0);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_subframe_idx, 0);
}

/*
 * Single-pose, single-sub-frame stream: every non-zero call blits pose 0
 * then wraps straight back to (0,0) (sub-frame hits the count and the pose
 * counter immediately passes the 1-pose bound). Confirms the wrap path fires
 * on the very first pose when it is also the last.
 */
static void test_step_figani_single_pose_wraps_each_call(void)
{
    static const uint8 meta[1][3] = { {0, 0, 1} };
    uint32 fig;

    fa_build_figani(g_fa_caster_fig, 1, meta);
    fs_reset();
    fig = (uint32)g_fa_caster_fig;

    fd2_step_figani_pose_animation(fig, 1, 0xA0000, 0x140);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 0L);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_pose_idx, 0);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_subframe_idx, 0);

    fd2_step_figani_pose_animation(fig, 1, 0xA0000, 0x140);
    ASSERT_EQ(g_blit_indexed_sprite_calls, 2);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_pose_idx, 0);
    ASSERT_EQ((int)data_fd2_graphics_figani_pose_anim_subframe_idx, 0);
}

/* ===== fd2_animate_spell_hit_cinematic @ 0x2BA22 =====
 *
 * The spell-HIT cinematic sub-loop: Phase 1 (frame 1..4) slides the caster
 * sprite in, Phase 2 (frame 4..0) slides the hit-effect sprite back out. These
 * tests pin its risk-bearing CONTROL FLOW:
 *   - the per-frame slide x = frame*0x23*team_dir_sign + workspace_ptr, where
 *     team_dir_sign is +1 for team-0 (enemy) casters and -1 otherwise — the
 *     single team-branch that flips the whole zoom direction;
 *   - the Phase 1 (caster atlas, 4 frames) vs Phase 2 (hit-effect atlas, 5
 *     frames) sprite-source + iteration split, plus the once-per-frame spell
 *     top-half pose blit (9 total);
 *   - the per-element palette-flash dispatch: handler[spell_type_idx] fired
 *     twice per frame with phase codes 4 then 5 (18 calls over 9 frames), its
 *     ignored int return notwithstanding;
 *   - the electric/lightning flicker branch (spell_type_idx 3 or 7): the
 *     background composite dst alternates workspace_ptr and workspace_ptr-0x280,
 *     observed through the REAL fd2_blit_rectangle landing a seeded sentinel.
 *
 * The indexed-sprite blits route through the testglob spy (atlas/frame/x logged
 * per call); the palette-flash dispatch routes through the testglob spell-phase
 * handler spy (phase-code sequence logged). The background + VGA-push
 * fd2_blit_rectangle and the per-frame fd2_wait_n_bios_ticks(1) run for real
 * over in-memory buffers; the actual composited pixels are a display side-effect
 * left to Phase 9, except the flicker dst which is asserted via the sentinel. */

extern int    g_blit_indexed_x_log[64];

extern int    g_spell_phase_handler_calls;
extern int    g_spell_phase_handler_log_count;
extern int    g_spell_phase_handler_phase_log[64];

/* Workspace large enough for the real fd2_blit_rectangle composite: the bg
 * composite writes 0xC8 rows x 0x280 stride from flicker_dst (>= workspace_ptr -
 * 0x280 = base + 0x4740), reaching base + 0x4740 + 0xC8*0x280 = base + 0x24240. */
static uint8 g_hc_workspace[0x30000];
/* bg source: real blit reads 0xC8 rows x 0x140 = 0x10F00 bytes. Seed [0]. */
static uint8 g_hc_bg[0x11000];
/* distinct atlas buffers so the spy log tells spell-pose / caster / effect
 * apart. spell atlas[0] = pose_count (the function reads *(uint8*)atlas). */
static uint8 g_hc_spell_atlas[16];
static uint8 g_hc_caster_atlas[16];
static uint8 g_hc_effect_atlas[16];

#define HC_POSE_COUNT 5

static void hc_setup(uint8 caster_team)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = caster_team;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    memset(g_hc_workspace, 0, sizeof(g_hc_workspace));
    memset(g_hc_bg, 0, sizeof(g_hc_bg));
    g_hc_bg[0] = 0xAB;                 /* flicker-landing sentinel */
    memset(g_hc_spell_atlas, 0, sizeof(g_hc_spell_atlas));
    g_hc_spell_atlas[0] = HC_POSE_COUNT;

    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_log_on = 1;
    g_blit_indexed_log_count = 0;
    memset(g_blit_indexed_atlas_log, 0, sizeof(g_blit_indexed_atlas_log));
    memset(g_blit_indexed_frame_log, 0, sizeof(g_blit_indexed_frame_log));
    memset(g_blit_indexed_x_log, 0, sizeof(g_blit_indexed_x_log));

    g_spell_phase_handler_calls = 0;
    g_spell_phase_handler_log_count = 0;
    memset(g_spell_phase_handler_phase_log, 0, sizeof(g_spell_phase_handler_phase_log));
}

static void hc_run(int spell_type_idx)
{
    fd2_animate_spell_hit_cinematic(
        0 /* attacker_idx */, 0xD15Au /* dispatch_sprite_atlas (opaque) */,
        (uint32)g_hc_spell_atlas, (int)(uint32)g_hc_caster_atlas,
        (uint32)g_hc_workspace, (uint32)g_hc_bg,
        (int)(uint32)g_hc_effect_atlas, spell_type_idx);
}

/*
 * Blit inventory + phase split: a non-flicker spell (type 5) issues 9 spell-pose
 * blits (one per frame), 4 caster-atlas blits (Phase 1, frames 1..4) and 5
 * effect-atlas blits (Phase 2, frames 4..0) = 18 indexed blits, interleaved
 * [spell-pose, slide] per frame. The dispatch fires 18 times (2/frame) with the
 * phase-code sequence 4,5 repeated.
 */
static void test_hit_cinematic_blit_inventory(void)
{
    uint32 spell = (uint32)g_hc_spell_atlas;
    uint32 caster = (uint32)g_hc_caster_atlas;
    uint32 effect = (uint32)g_hc_effect_atlas;
    int    i;
    int    spell_pose, caster_hit, effect_hit;

    hc_setup(0);
    hc_run(5);

    ASSERT_EQ(g_blit_indexed_log_count, 18);
    spell_pose = 0; caster_hit = 0; effect_hit = 0;
    for (i = 0; i < g_blit_indexed_log_count; i++) {
        if (g_blit_indexed_atlas_log[i] == spell) {
            spell_pose++;
            /* spell pose blits pose_count-1 every frame */
            ASSERT_EQ((long)g_blit_indexed_frame_log[i], (long)(HC_POSE_COUNT - 1));
        } else if (g_blit_indexed_atlas_log[i] == caster) {
            caster_hit++;
            ASSERT_EQ((long)g_blit_indexed_frame_log[i], 0L);
        } else if (g_blit_indexed_atlas_log[i] == effect) {
            effect_hit++;
            ASSERT_EQ((long)g_blit_indexed_frame_log[i], 0L);
        }
    }
    ASSERT_EQ(spell_pose, 9);
    ASSERT_EQ(caster_hit, 4);
    ASSERT_EQ(effect_hit, 5);

    /* every frame: [spell-pose, slide] order -> even idx = spell, odd = slide */
    for (i = 0; i < g_blit_indexed_log_count; i += 2) {
        ASSERT_EQ((long)g_blit_indexed_atlas_log[i], (long)spell);
    }
    /* Phase 1 slide sprites are the caster (idx 1,3,5,7); Phase 2 the effect. */
    ASSERT_EQ((long)g_blit_indexed_atlas_log[1], (long)caster);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[7], (long)caster);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[9], (long)effect);
    ASSERT_EQ((long)g_blit_indexed_atlas_log[17], (long)effect);

    /* dispatch: 2 per frame x 9 frames, phase codes 4,5 repeated */
    ASSERT_EQ(g_spell_phase_handler_calls, 18);
    ASSERT_EQ(g_spell_phase_handler_log_count, 18);
    for (i = 0; i < 18; i += 2) {
        ASSERT_EQ(g_spell_phase_handler_phase_log[i], 4);
        ASSERT_EQ(g_spell_phase_handler_phase_log[i + 1], 5);
    }
}

/*
 * team 0 (enemy) -> team_dir_sign = +1: the slide sprite moves to
 * frame*0x23 + workspace_ptr. Phase 1 caster x at frames 1..4 = workspace_ptr +
 * {0x23,0x46,0x69,0x8C}; Phase 2 effect x at frames 4,3,2,1,0 = workspace_ptr +
 * {0x8C,0x69,0x46,0x23,0}. The slide blits are the odd-indexed log entries.
 */
static void test_hit_cinematic_slide_team0_positive(void)
{
    int wp = (int)(uint32)g_hc_workspace + 0x49C0;
    int p1_frame[4];
    int p2_frame[5];
    int i;

    hc_setup(0);
    hc_run(5);

    p1_frame[0] = 1; p1_frame[1] = 2; p1_frame[2] = 3; p1_frame[3] = 4;
    for (i = 0; i < 4; i++) {
        /* slide blit for Phase-1 frame (i+1) sits at log idx 2*i+1 */
        ASSERT_EQ((long)g_blit_indexed_x_log[2 * i + 1],
                  (long)(p1_frame[i] * 0x23 + wp));
    }
    p2_frame[0] = 4; p2_frame[1] = 3; p2_frame[2] = 2; p2_frame[3] = 1; p2_frame[4] = 0;
    for (i = 0; i < 5; i++) {
        /* Phase-2 slide blit sits at log idx 8 + 2*i + 1 */
        ASSERT_EQ((long)g_blit_indexed_x_log[8 + 2 * i + 1],
                  (long)(p2_frame[i] * 0x23 + wp));
    }
}

/*
 * team != 0 (player) -> team_dir_sign = -1: the slide direction REVERSES, so the
 * sprite moves to workspace_ptr - frame*0x23. Same FIGANI/buffers as the team-0
 * case; only the team byte differs. Spot-check Phase 1 frame 1 and 4, Phase 2
 * frame 4 and 0.
 */
static void test_hit_cinematic_slide_team_nonzero_negative(void)
{
    int wp = (int)(uint32)g_hc_workspace + 0x49C0;

    hc_setup(2);
    hc_run(5);

    /* Phase 1 frame 1 (idx 1) and frame 4 (idx 7) */
    ASSERT_EQ((long)g_blit_indexed_x_log[1], (long)(wp - 1 * 0x23));
    ASSERT_EQ((long)g_blit_indexed_x_log[7], (long)(wp - 4 * 0x23));
    /* Phase 2 frame 4 (idx 9) and frame 0 (idx 17) */
    ASSERT_EQ((long)g_blit_indexed_x_log[9], (long)(wp - 4 * 0x23));
    ASSERT_EQ((long)g_blit_indexed_x_log[17], (long)(wp - 0 * 0x23));
}

/*
 * Electric/lightning flicker branch (spell_type_idx 3): the background composite
 * dst drops to workspace_ptr-0x280 on odd flicker_toggle frames. The composite
 * copies bg[0]=0xAB to its dst's first byte, so an odd frame stamps 0xAB at
 * workspace_ptr-0x280[0]. A non-flicker spell (type 5) ALWAYS composites at
 * workspace_ptr and never writes the -0x280 row, so that byte stays 0 — it is
 * the flicker discriminator. (The composite's stride 0x280 spans the whole
 * frame, so a -0x280 write's row 1 also lands on workspace_ptr with bg's zero
 * rows; the discriminator is the -0x280 row-0 byte, which only the flicker path
 * can set.) Dispatch index 3 is a valid handler slot. */
static void test_hit_cinematic_flicker_type3(void)
{
    uint32 wp;

    hc_setup(3);
    wp = (uint32)g_hc_workspace + 0x49C0;
    hc_run(3);
    /* flicker dropped a composite to the -0x280 row -> sentinel landed there */
    ASSERT_EQ((int)*(uint8 *)(wp - 0x280), 0xAB);
}

static void test_hit_cinematic_no_flicker_type5(void)
{
    uint32 wp;

    hc_setup(0);
    wp = (uint32)g_hc_workspace + 0x49C0;
    hc_run(5);
    /* non-flicker spell never composites at workspace_ptr-0x280 ... */
    ASSERT_EQ((int)*(uint8 *)(wp - 0x280), 0x00);
    /* ... it always composites at workspace_ptr, stamping the sentinel there */
    ASSERT_EQ((int)*(uint8 *)wp, 0xAB);
}

void run_anim_anicine2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anicine (2)\n");
    RUN_TEST(test_figani_sfx_gate_fires);
    RUN_TEST(test_figani_sfx_gate_fires_0x1c);
    RUN_TEST(test_figani_sfx_gate_closed);
    RUN_TEST(test_figani_blit_order_team0);
    RUN_TEST(test_figani_blit_order_team_nonzero);
    RUN_TEST(test_figani_target_blit_gated_off);
    RUN_TEST(test_figani_target_pose_cycle_wrap);
    RUN_TEST(test_figani_remap_idx_0x13);
    RUN_TEST(test_figani_remap_idx_0x0f);
    RUN_TEST(test_figani_remap_idx_default_0x0b);
    RUN_TEST(test_step_figani_pose_walk);
    RUN_TEST(test_step_figani_rewind_on_zero);
    RUN_TEST(test_step_figani_single_pose_wraps_each_call);
    RUN_TEST(test_hit_cinematic_blit_inventory);
    RUN_TEST(test_hit_cinematic_slide_team0_positive);
    RUN_TEST(test_hit_cinematic_slide_team_nonzero_negative);
    RUN_TEST(test_hit_cinematic_flicker_type3);
    RUN_TEST(test_hit_cinematic_no_flicker_type5);
    printf("\n");
    (void)_prev_fails;
}
