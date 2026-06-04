/*
 * unit tests for src/audio/audio.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

#include <stdlib.h>
#include "realfile.h"   /* realdat_read_resource() */

#define USE_ITEM_ID 10

/* fd2_set_bgm_track_with_fade's track-change path calls the REAL
 * fd2_load_dat_resource against the STAGED real FDMUS.DAT[track_id]; the
 * loaded sequence buffer is fed to the linked fd2_dpmi_lock_size + AIL_*
 * stubs. The asserted outputs (AIL volume-call count / volume / ramp) are
 * pure control flow, independent of the sequence bytes, so no fixture is
 * needed — only the real archive must resolve the track index (it does:
 * real FDMUS.DAT covers indices 0..0x10+). */
static void free_bgm_buf(void)
{
    if (data_fd2_audio_bgm_sequence_data_buf_ptr != 0) {
        free((void *)data_fd2_audio_bgm_sequence_data_buf_ptr);
        data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    }
}

extern runtime_char g_test_rc_array[8];
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;
extern uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
extern int g_ending_menu_return;
extern int g_slot_selector_return;
extern int g_chapter_transition_return;
extern int g_play_sfx_with_handle_calls;
extern uint32 g_sfx_last_arg_a;
extern int g_sfx_last_id;
extern int g_sfx_last_arg_c;
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


/* ---- Test: fd2_set_bgm_track_with_fade ---- */

static void test_bgm_stop_with_fade(void)
{
    data_fd2_audio_bgm_last_set_track_id = 5;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(0xFFFFFFFF, 0);
    ASSERT_EQ((long)g_ail_vol_calls, 1);
    ASSERT_EQ((long)g_ail_last_vol, 0);
    ASSERT_EQ((long)g_ail_last_ramp, 4000);
}


static void test_bgm_same_track_noop(void)
{
    data_fd2_audio_bgm_last_set_track_id = 3;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(3, 1);
    ASSERT_EQ((long)g_ail_vol_calls, 0);
}


static void test_bgm_change_regular_track(void)
{
    data_fd2_audio_bgm_last_set_track_id = 0xFF;
    data_fd2_audio_bgm_enabled_flag = 1;
    data_fd2_audio_bgm_driver_available_flag = 1;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(5, 1);
    ASSERT_EQ((long)data_fd2_audio_bgm_last_set_track_id, 5);
    ASSERT_EQ((long)g_ail_vol_calls, 2);
    ASSERT_EQ((long)g_ail_last_vol, 0x7F);
    ASSERT_EQ((long)g_ail_last_ramp, 2000);
    free_bgm_buf();
}


static void test_bgm_disabled_zero_volume(void)
{
    data_fd2_audio_bgm_last_set_track_id = 0xFF;
    data_fd2_audio_bgm_enabled_flag = 0;
    data_fd2_audio_bgm_driver_available_flag = 1;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(5, 1);
    ASSERT_EQ((long)g_ail_last_vol, 0);
    ASSERT_EQ((long)g_ail_last_ramp, 0);
    data_fd2_audio_bgm_enabled_flag = 1;
    free_bgm_buf();
}


/* Special-cue branch: tracks 0x10/0x11 set volume instantly (vol 0x7F,
 * ramp 0) with a SINGLE AIL_set_sequence_volume call and NO 0-anchor,
 * unlike the regular 2-call/2000ms-ramp path. Distinct numeric output. */
static void test_bgm_special_cue_instant(void)
{
    data_fd2_audio_bgm_last_set_track_id = 0xFF;
    data_fd2_audio_bgm_enabled_flag = 1;
    data_fd2_audio_bgm_driver_available_flag = 1;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    g_ail_vol_calls = 0;
    fd2_set_bgm_track_with_fade(0x10, 1);
    ASSERT_EQ((long)g_ail_vol_calls, 1);
    ASSERT_EQ((long)g_ail_last_vol, 0x7F);
    ASSERT_EQ((long)g_ail_last_ramp, 0);
    free_bgm_buf();
}


/* ---- Test: fd2_load_status_effect_sfx ---- */

/* Drives the REAL loader against the staged real FDOTHER.DAT entry 0x50: the
 * function clears the handle, then captures fd2_load_dat_resource's return
 * (the EAX value) into data_fd2_audio_status_effect_sfx_handle_ptr and the
 * loader sets last_loaded_resource_size as a side effect. Cross-check the
 * captured handle's payload + the size global against an independent parse of
 * the SAME real bytes (realfile.h) — proves the right filename, the right
 * entry index 0x50, and the return-value capture. No hardcoded values. */
static void test_load_status_effect_sfx_real(void)
{
    uint8 *ref;
    long   ref_size;

    ref_size = realdat_read_resource("FDOTHER.DAT", 0x50, &ref);
    ASSERT_TRUE(ref_size > 0);

    /* pre-poison so a no-op or a stale value can't pass */
    data_fd2_audio_status_effect_sfx_handle_ptr = 0xDEADBEEF;
    data_fd2_resource_last_loaded_resource_size = 0;

    fd2_load_status_effect_sfx();

    /* handle = loader's returned buffer (non-NULL, not the poison) */
    ASSERT_TRUE(data_fd2_audio_status_effect_sfx_handle_ptr != 0);
    ASSERT_TRUE(data_fd2_audio_status_effect_sfx_handle_ptr != 0xDEADBEEF);
    /* size side effect == independent parse of entry 0x50 */
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, ref_size);
    /* loaded payload byte-matches the real entry-0x50 bytes */
    ASSERT_EQ((long)memcmp(
        (void *)data_fd2_audio_status_effect_sfx_handle_ptr,
        ref, (size_t)ref_size), 0);

    free((void *)data_fd2_audio_status_effect_sfx_handle_ptr);
    data_fd2_audio_status_effect_sfx_handle_ptr = 0;
    free(ref);
}


/* ---- Test: fd2_play_and_free_status_effect_sfx ---- */

/* The function's whole semantic is: play the loaded SFX bank in kill-all mode
 * (fd2_play_sfx_with_handle(handle, -1, 1)) then free the bank buffer. Drive it
 * with a real malloc'd buffer in the handle slot and confirm, via the g_sfx_*
 * capture spy, that arg1 == that buffer, arg2 == -1 (the kill-all sentinel, the
 * defining behaviour), and arg3 == 1. The real free(handle) inside the function
 * releases the buffer, so the test must NOT free it again (would double-free);
 * it only nulls the global afterward. */
static void test_play_and_free_status_effect_sfx(void)
{
    uint8 *buf;

    buf = (uint8 *)malloc(64);
    ASSERT_TRUE(buf != 0);

    data_fd2_audio_status_effect_sfx_handle_ptr = (uint32)buf;
    g_play_sfx_with_handle_calls = 0;
    g_sfx_last_arg_a = 0;
    g_sfx_last_id = 0;
    g_sfx_last_arg_c = 0;

    fd2_play_and_free_status_effect_sfx();

    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_sfx_last_arg_a, (long)(uint32)buf);
    ASSERT_EQ((long)g_sfx_last_id, -1);
    ASSERT_EQ((long)g_sfx_last_arg_c, 1);

    /* buf was freed by the function under test; just clear the dangling global */
    data_fd2_audio_status_effect_sfx_handle_ptr = 0;
}


void run_audio_audio_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: audio/audio\n");
    RUN_TEST(test_bgm_stop_with_fade);
    RUN_TEST(test_bgm_same_track_noop);
    RUN_TEST(test_bgm_change_regular_track);
    RUN_TEST(test_bgm_disabled_zero_volume);
    RUN_TEST(test_bgm_special_cue_instant);
    RUN_TEST(test_load_status_effect_sfx_real);
    RUN_TEST(test_play_and_free_status_effect_sfx);
    printf("\n");
}
