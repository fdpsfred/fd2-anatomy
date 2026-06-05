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
        (uint32)g_fa_workspace, g_fa_dstbuf,
        g_fa_caster_fig /* bg_layer_a (unused on type-0 poses) */,
        (uint32 *)g_fa_caster_fig /* bg_layer_b */);
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
    printf("\n");
    (void)_prev_fails;
}
