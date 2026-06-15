/*
 * unit tests for src/anim/anisummn.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx */

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


/* ---- Test: fd2_tick_summon_spell_minor_animation_state ---- */

/* ---- Test: fd2_tick_summon_anim_variant_d_3slot ---- */

static void test_summon_d_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1], -3);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[3], -9);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_color_rotation_counter, 4);
}


static void test_summon_d_state3_hold(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x20);
}


static void test_summon_d_state6_terminate(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 0x10);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_terminate_flag, 1);
}


/* TICK (states 2/5/8) first toggles odd_even (mod 2). When the pre-call
 * toggle is 0 it becomes 1, so the whole even-frame update block is gated
 * OFF: slots still blit if visible, but frame counters do NOT advance, no
 * SFX, no done. State 2. */
static void test_summon_d_tick_toggle_skips_update(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 3; i++) {
        data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i] = 0;
        data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle, 1);
    for (i = 0; i < 3; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i], 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 3);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}


/* TICK with pre-call toggle 1 -> becomes 0 -> update block runs. Only the
 * 3 active slots iterate; each in-range (post-init staggered) slot advances
 * frame by 1, slot 0 frame 0 (in range) blits, slots 1/2 hidden (negative).
 * No slot hits frame 1 (no SFX) or post-inc 2 (no done). State 2. */
static void test_summon_d_tick_advance_no_done(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 1;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1] = -3;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2] = -6;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[0] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[1] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[2] = 0;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1], -2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2], -5);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}


/* done_flag (return 1) is set iff some slot's post-increment counter == 2.
 * slot 0 goes 1->2 (done); slots 1/2 go 0->1 (no done). Pre-call toggle 1
 * -> 0 so update runs. slot 0 frame 1 also fires the i==0 SFX bucket. */
static void test_summon_d_tick_done_at_frame2(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 1;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0] = 1;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[0] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[1] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[2] = 0;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0], 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2], 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 3);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}


/* Frame-7 color rotation (terminate_flag==0): on the slot whose post-inc
 * frame reaches 7, rotation_counter=(counter+1)%10, color_idx[i]=counter,
 * frame_counter[i]=0. slot 0 frame 6 (hidden, no blit) -> post-inc 7 ->
 * rotation. counter 3 -> 4. State 5 also TICKs. Pre-call toggle 1 -> 0. */
static void test_summon_d_tick_color_rotation(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_anim_variant_d_color_rotation_counter = 3;
    g_blit_indexed_sprite_calls = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0] = 6;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1] = -3;
    data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2] = -6;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[0] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[1] = 0;
    data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[2] = 0;
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_color_rotation_counter, 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[0], 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[1], -2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[2], -5);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
}


/* 2-bucket frame-1 SFX: all 3 slots at frame 1 -> slot 0 plays with_handle,
 * slot 1 plays sample_from_bank, slot 2 silent. All 3 in range so all blit;
 * each post-inc 1->2 sets done (return 1). Pre-call toggle 1 -> 0. State 8. */
static void test_summon_d_tick_sfx_bucket_split(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_d_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle = 1;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 3; i++) {
        data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i] = 1;
        data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_anim_variant_d_3slot(0, 0, 0, 0, 8);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 3);
    for (i = 0; i < 3; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[i], 2);
}


static void test_summon_a_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_shared_rng_seed = 42;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[2], -4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_color_rotation_counter, 6);
}


static void test_summon_a_state6_terminate(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 8);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_terminate_flag, 1);
}


/* state 3 is a pure constant return (disasm 0x26a9c CMP EAX,3 -> MOV EAX,0xc). */
static void test_summon_a_state3(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0xC);
}


/* TICK (state 2) blit-gate + frame-0 SFX + done_flag, rotation suppressed via
 * terminate_flag==1 so the frame-8 path is isolated out:
 *   slot0 frame 0  -> in [0,7) blit, frame==0 SFX (with_handle), ++ ->1
 *   slot1 frame 2  -> blit, ++ ->3 sets done_flag (disasm 0x26b64 CMP ...,3)
 *   slot2 frame 6  -> blit (6<7), ++ ->7
 *   slot3 frame -1 -> NOT in [0,7), no blit, ++ ->0
 *   slot4 frame -2 -> no blit, ++ ->-1
 *   slot5 frame 3  -> blit, ++ ->4
 * Variant-A TICK only ever calls fd2_play_sfx_with_handle (single CALL 0x25a96
 * at 0x26b51); it never calls fd2_play_sfx_sample_from_bank. */
static void test_summon_a_tick_blit_gate_sfx_done(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_a_terminate_flag = 1;
    for (i = 0; i < 6; i++)
        data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i] = 0;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0] = 0;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[1] = 2;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[2] = 6;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[3] = -1;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[4] = -2;
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[5] = 3;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 4);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[1], 3);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[2], 7);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[3], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[5], 4);
}


/* TICK color rotation at frame 8 (terminate_flag==0): counter=(counter+1)%10,
 * color_idx[i]=counter, frame_counter[i]=0, then RNG jitter=7*(rng%2)
 * (disasm 0x26bc8 CALL rng; 0x26bcf..0x26bde IDIV 2 -> 7*(rng%2); the emitter
 * corrected the decompiler EAX bug that used the loop index). seed=8192 ->
 * fd2_advance_rng_state ROL16(0x2000+0x9014,3)=0x80A5 (odd) -> jitter 7. slot0
 * is the only slot at frame 7 (->8); other slots stay at frame 4 (blit, ++ ->5,
 * never reach 3 or 8) so done stays 0. */
static void test_summon_a_tick_color_rotation(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_a_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_a_color_rotation_counter = 3;
    data_fd2_shared_rng_seed = 8192;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i] = 0;
        data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0] = 7;
    r = fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_color_rotation_counter, 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[0], 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[0], 7);
    ASSERT_EQ((long)r, 0);
}


/* TICK rotation mod-10 wrap + even-RNG jitter branch: counter 9 -> (9+1)%10==0,
 * and seed=0 -> fd2_advance_rng_state ROL16(0x9014,3)=0x80A4 (even) ->
 * jitter 7*(0)=0 (overwriting the preset 7, proving the RNG branch). Only slot0
 * reaches frame 8; other slots at frame -2 stay out of every gate. */
static void test_summon_a_tick_rotation_mod10_wrap(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_a_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_a_color_rotation_counter = 9;
    data_fd2_shared_rng_seed = 0;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[i] = -2;
        data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0] = 7;
    data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[0] = 7;
    fd2_tick_summon_anim_variant_a_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_color_rotation_counter, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[0], 0);
}


static void test_summon_8slot_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x1C);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[7], -14);
}


/* Helper-free shared setup note: in the unit-test build the three rodata
 * tables (visibility / y_offset / row_multiplier) are the zero-initialised
 * globals in testglob.c, so every 8slot test that depends on them sets the
 * entries it needs explicitly. */

/* state 5 done_flag return: loop increments each of slots 0..6, then sets
 * done=1 iff a post-increment counter == 9 (disasm 0x262cb INC; 0x262d2 CMP
 * ...,9). vis all 1 -> state-5 inverse gate (vis==0) never fires, isolating
 * the counter/return. slot 0 preset to 8 -> ++ -> 9 -> done; slot 1 at 0 ->
 * 1. Returns done_flag (read at 0x262e6 from [ESP+0x40]). */
/* SKIP (Phase 3): writes now-const data_fd2_battle_summon_spell_8slot_visibility_table; restore + rewrite to drive real const data */
#if 0
static void test_summon_8slot_state5_done_flag(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 1;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = 0;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 8;
    g_blit_indexed_sprite_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0], 9);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[1], 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
}
#endif


/* state 5 no-done: all slots 0..6 at 0 -> ++ -> 1, none reaches 9 -> return 0.
 * vis all 1 keeps the inverse gate closed (no blit). */
/* SKIP (Phase 3): writes now-const data_fd2_battle_summon_spell_8slot_visibility_table; restore + rewrite to drive real const data */
#if 0
static void test_summon_8slot_state5_no_done(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 1;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = 0;
    g_blit_indexed_sprite_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    for (i = 0; i < 7; i++)
        ASSERT_EQ(
            (long)data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i],
            1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
}
#endif


/* state 5 inverse blit gate vis[i]==0 (disasm 0x262a0 TEST/JNZ skips blit when
 * vis!=0): slot 0 vis==0 + counter in [0,0x10) -> blit; slot 1 vis==1 +
 * in-range -> no blit; remaining slots out of range. Pre-increment counter
 * gates the blit, so counter 2 (in range) blits then becomes 3. No slot hits
 * post-inc 9 -> return 0. Exactly 1 blit. */
/* SKIP (Phase 3): writes now-const data_fd2_battle_summon_spell_8slot_visibility_table; restore + rewrite to drive real const data */
#if 0
static void test_summon_8slot_state5_inverse_gate(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 1;
    data_fd2_battle_summon_spell_8slot_visibility_table[0] = 0;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 2;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[1] = 2;
    g_blit_indexed_sprite_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
}
#endif


/* state 4 SFX trigger: fd2_play_sfx_with_handle fires when a slot counter == 3
 * (disasm 0x2620b CMP ...,3; 0x2621f CALL). vis all 0 keeps the state-4 gate
 * (vis==1) closed so no blit confounds the count. slot 0 == 3 -> exactly one
 * SFX; state 4 always returns 0. */
/* SKIP (Phase 3): writes now-const data_fd2_battle_summon_spell_8slot_visibility_table; restore + rewrite to drive real const data */
#if 0
static void test_summon_8slot_state4_sfx_trigger(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 3;
    g_play_sfx_with_handle_calls = 0;
    g_blit_indexed_sprite_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 4);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
}
#endif


/* state 4 SFX negative: no slot 0..6 counter == 3 -> no SFX. */
/* SKIP (Phase 3): writes now-const data_fd2_battle_summon_spell_8slot_visibility_table; restore + rewrite to drive real const data */
#if 0
static void test_summon_8slot_state4_sfx_no_trigger(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = 4;
    g_play_sfx_with_handle_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 4);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}
#endif


/* state 4 visibility gate vis[i]==1 (disasm 0x2623e MOVZX; 0x26243 CMP ...,1):
 * slot 0 vis==1 + counter in range -> blit; slot 1 vis==0 + in range -> no
 * blit; rest out of range. Counters kept != 3 so no SFX confounds the count.
 * Exactly 1 blit. */
/* SKIP (Phase 3): writes now-const data_fd2_battle_summon_spell_8slot_visibility_table; restore + rewrite to drive real const data */
#if 0
static void test_summon_8slot_state4_visibility_gate(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++)
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
    data_fd2_battle_summon_spell_8slot_visibility_table[0] = 1;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 2;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[1] = 2;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}
#endif


/* state 4 blit-y arithmetic, player team (no enemy adjust): single visible
 * in-range slot 0. The display-y expression row_mul[0]*row_stride + origin_y +
 * y_off[0] (disasm 0x26257 IMUL EBP; 0x2625a ADD) is the 3rd blit argument, so
 * it lands in the stub's x slot (g_blit_indexed_sprite_last_x); the 4th
 * argument row_stride lands in last_y. With row_mul[0]=-10, row_stride=10,
 * origin_y=100, y_off[0]=40 -> -10*10 + 100 + 40 = 40. frame = counter = 5
 * (!=3 so no SFX). */
/* SKIP (Phase 3): writes now-const data_fd2_battle_summon_spell_8slot_row_multiplier_table, data_fd2_battle_summon_spell_8slot_visibility_table, data_fd2_battle_summon_spell_8slot_y_offset_table; restore + rewrite to drive real const data */
#if 0
static void test_summon_8slot_state4_blit_y_arithmetic(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    for (i = 0; i < 7; i++) {
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
        data_fd2_battle_summon_spell_8slot_y_offset_table[i] = 0;
        data_fd2_battle_summon_spell_8slot_row_multiplier_table[i] = 0;
    }
    data_fd2_battle_summon_spell_8slot_visibility_table[0] = 1;
    data_fd2_battle_summon_spell_8slot_y_offset_table[0] = 40;
    data_fd2_battle_summon_spell_8slot_row_multiplier_table[0] = -10;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 5;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_x = 0;
    g_blit_indexed_sprite_last_y = 0;
    g_blit_indexed_sprite_last_frame = 0;
    g_play_sfx_with_handle_calls = 0;
    fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 100, 10, 4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 5);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 40);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_y, 10);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}
#endif


/* state 4 blit-y enemy-team offset: team==0 adds 0x94 (148) to every y_off
 * before the blit math (disasm 0x261b3 TEST/JZ then 0x261bb ADD ...,0x94).
 * Same inputs as the arithmetic test but team=0 -> y_off[0] 40+148=188 ->
 * -10*10 + 100 + 188 = 188, landing in last_x (3rd arg). frame unchanged (5). */
/* SKIP (Phase 3): writes now-const data_fd2_battle_summon_spell_8slot_row_multiplier_table, data_fd2_battle_summon_spell_8slot_visibility_table, data_fd2_battle_summon_spell_8slot_y_offset_table; restore + rewrite to drive real const data */
#if 0
static void test_summon_8slot_state4_blit_y_enemy_team_offset(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    for (i = 0; i < 7; i++) {
        data_fd2_battle_summon_spell_8slot_visibility_table[i] = 0;
        data_fd2_battle_summon_spell_8slot_y_offset_table[i] = 0;
        data_fd2_battle_summon_spell_8slot_row_multiplier_table[i] = 0;
    }
    data_fd2_battle_summon_spell_8slot_visibility_table[0] = 1;
    data_fd2_battle_summon_spell_8slot_y_offset_table[0] = 40;
    data_fd2_battle_summon_spell_8slot_row_multiplier_table[0] = -10;
    for (i = 0; i < 8; i++)
        data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[i] = -100;
    data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[0] = 5;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_x = 0;
    g_blit_indexed_sprite_last_y = 0;
    g_blit_indexed_sprite_last_frame = 0;
    fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 100, 10, 4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 5);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 188);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_y, 10);
}
#endif


/* default state: any state_code other than 3/4/5 returns 0 with no SFX/blit
 * (fall-through epilogue, EAX=0). */
static void test_summon_8slot_default_state(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    r = fd2_tick_summon_spell_setup_pre_animation_8slot(0, 0, 0, 0, 7);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}


static void test_summon_main_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[11], -22);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_color_rotation_counter, 12);
}


/* state 3: pure 40-tick hold, returns 0x28, no side effects. */
static void test_summon_main_state3_hold(void)
{
    int r;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x28);
}


/* state 6: sets terminate_flag=1 (stops color rotation), returns 0x14. */
static void test_summon_main_state6_terminate(void)
{
    int r;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 0x14);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_terminate_flag, 1);
}


/* default branch: any other state_code returns 0. */
static void test_summon_main_default_state(void)
{
    int r;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 7);
    ASSERT_EQ((long)r, 0);
}


/* TICK (states 2/5/8) first toggles odd_even (mod 2). When the pre-call
 * toggle is 0 it becomes 1, gating the whole even-frame update block OFF:
 * in-range slots still blit, but frame counters do NOT advance, no SFX,
 * no done. All 12 slots frame 5 (in range [0,0xB)). State 2. */
static void test_summon_main_tick_toggle_skips_update(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = 5;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_odd_even_frame_toggle, 1);
    for (i = 0; i < 12; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i], 5);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 12);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}


/* TICK with pre-call toggle 1 -> becomes 0 -> update block runs. Staggered
 * init frames (slot 0 at 0, others negative): only slot 0 is in range, so
 * 1 blit; every slot's frame advances by 1; no slot reaches post-inc 3
 * (no done) or post-inc 0xB (no rotation). color 0 spr_offset is left 0 so
 * slot 0 frame 0 fires no with_handle. State 2. */
static void test_summon_main_tick_advance_no_done(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -2 * i;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_odd_even_frame_toggle, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[1], -1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[11], -21);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}


/* done-at-frame-3: a slot whose post-increment counter reaches 3 sets
 * return 1. spr_offset[color]==0 for that slot, so frame-3 also fires
 * sample_from_bank. slot 0 frame 2 -> post-inc 3 (done + sample); other
 * slots negative (no effect). Pre-call toggle 1 -> 0. State 5 also TICKs. */
static void test_summon_main_tick_done_at_frame3(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 2;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 3);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}


/* SFX bucket split driven by spr_offsets[color]:
 *  - frame 0 with spr_offsets[color]!=0  -> with_handle (sfx 2)
 *  - post-inc frame 3 with spr_offsets[color]==0 -> sample_from_bank (sfx 1)
 * Assembly note: done_flag is set whenever post-inc==3 REGARDLESS of
 * spr_offsets; the sample SFX is the spr==0-only extra. Here slot 0 (color
 * 0, spr=0x16) is at frame 0 -> with_handle, and slot 1 (color 1, spr=0)
 * is at frame 2 -> post-inc 3 -> sample + done. Pre-call toggle 1 -> 0.
 * State 8. */
static void test_summon_main_tick_sfx_bucket_split(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0x16;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[1] = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++)
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -8;
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 0;
    data_fd2_battle_summon_main_anim_12slot_color_idx_array[0] = 0;
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[1] = 2;
    data_fd2_battle_summon_main_anim_12slot_color_idx_array[1] = 1;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 8);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    /* both slots in range -> 2 blits */
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[1], 3);
}


/* done at post-inc 3 fires even when spr_offsets[color]!=0 (no sample SFX):
 * slot 0 color 0 spr=0x16 at frame 2 -> post-inc 3 -> done=1 but NO
 * sample_from_bank. Isolates the done/sample decoupling. Toggle 1 -> 0. */
static void test_summon_main_tick_done_without_sample_when_spr_nonzero(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0x16;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 2;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 3);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
    /* frame went 2->3 so it was never 0 at entry: no with_handle either */
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}


/* Color rotation at post-inc frame 0xB (terminate==0): rotation_counter =
 * (counter+1)%12, color_idx[i]=counter, frame_counter[i]=0. slot 0 frame
 * 0xA (in range, blits) -> post-inc 0xB -> rotation. counter 5 -> 6.
 * Other slots negative. Pre-call toggle 1 -> 0. State 2. */
static void test_summon_main_tick_color_rotation(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_color_rotation_counter = 5;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    g_blit_indexed_sprite_calls = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 0xA;
    r = fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_color_rotation_counter, 6);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_color_idx_array[0], 6);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
}


/* terminate_flag==1 blocks the frame-0xB rotation entirely: frame stays
 * 0xB, rotation_counter and color_idx unchanged. slot 0 frame 0xA ->
 * post-inc 0xB but rotation skipped. Toggle 1 -> 0. */
static void test_summon_main_tick_rotation_blocked_by_terminate(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 1;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_color_rotation_counter = 5;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 0xA;
    fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 0xB);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_color_rotation_counter, 5);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_color_idx_array[0], 0);
}


/* Rotation counter wraps mod 12: counter 11 -> (11+1)%12 == 0. The signed
 * IDIV-by-12 path computes the modulus; assert the wrap lands on 0. slot 0
 * frame 0xA -> post-inc 0xB -> rotation. Toggle 1 -> 0. */
static void test_summon_main_tick_rotation_mod12_wrap(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_terminate_flag = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 1;
    data_fd2_battle_summon_main_anim_color_rotation_counter = 11;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[0] = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 0xA;
    fd2_tick_summon_spell_main_animation_state(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_color_rotation_counter, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_color_idx_array[0], 0);
}


void run_anim_anisummn1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anisummn (1/2)\n");
    /* The summon tick variants fire fd2_play_sfx_with_handle(summon bank, id, 1)
     * (id <= 3); the now-real player needs the audio gates open and a valid bank
     * so the AIL stop spy bumps g_play_sfx_with_handle_calls per fired SFX. The
     * gates and bank ptr are not reset by any test below, so set them once here. */
    audiofix_enable_sfx();
    data_fd2_audio_summon_spell_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);
    RUN_TEST(test_summon_d_init);
    RUN_TEST(test_summon_d_state3_hold);
    RUN_TEST(test_summon_d_state6_terminate);
    RUN_TEST(test_summon_d_tick_toggle_skips_update);
    RUN_TEST(test_summon_d_tick_advance_no_done);
    RUN_TEST(test_summon_d_tick_done_at_frame2);
    RUN_TEST(test_summon_d_tick_color_rotation);
    RUN_TEST(test_summon_d_tick_sfx_bucket_split);
    RUN_TEST(test_summon_8slot_init);
#if 0
    RUN_TEST(test_summon_8slot_state5_done_flag);
#endif
#if 0
    RUN_TEST(test_summon_8slot_state5_no_done);
#endif
#if 0
    RUN_TEST(test_summon_8slot_state5_inverse_gate);
#endif
#if 0
    RUN_TEST(test_summon_8slot_state4_sfx_trigger);
#endif
#if 0
    RUN_TEST(test_summon_8slot_state4_sfx_no_trigger);
#endif
#if 0
    RUN_TEST(test_summon_8slot_state4_visibility_gate);
#endif
#if 0
    RUN_TEST(test_summon_8slot_state4_blit_y_arithmetic);
#endif
#if 0
    RUN_TEST(test_summon_8slot_state4_blit_y_enemy_team_offset);
#endif
    RUN_TEST(test_summon_8slot_default_state);
    RUN_TEST(test_summon_main_init);
    RUN_TEST(test_summon_main_state3_hold);
    RUN_TEST(test_summon_main_state6_terminate);
    RUN_TEST(test_summon_main_default_state);
    RUN_TEST(test_summon_main_tick_toggle_skips_update);
    RUN_TEST(test_summon_main_tick_advance_no_done);
    RUN_TEST(test_summon_main_tick_done_at_frame3);
    RUN_TEST(test_summon_main_tick_sfx_bucket_split);
    RUN_TEST(test_summon_main_tick_done_without_sample_when_spr_nonzero);
    RUN_TEST(test_summon_main_tick_color_rotation);
    RUN_TEST(test_summon_main_tick_rotation_blocked_by_terminate);
    RUN_TEST(test_summon_main_tick_rotation_mod12_wrap);
    RUN_TEST(test_summon_a_init);
    RUN_TEST(test_summon_a_state6_terminate);
    RUN_TEST(test_summon_a_state3);
    RUN_TEST(test_summon_a_tick_blit_gate_sfx_done);
    RUN_TEST(test_summon_a_tick_color_rotation);
    RUN_TEST(test_summon_a_tick_rotation_mod10_wrap);
    audiofix_disable_sfx();   /* restore safe gate state for later suites */
    printf("\n");
}
