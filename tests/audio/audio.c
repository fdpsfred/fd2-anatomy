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
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx */

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
extern int g_chapter_transition_return;
extern int g_play_sfx_with_handle_calls;
extern uint32 g_sfx_last_arg_a;
extern uint32 g_sfx_last_handle;
extern int g_sfx_last_id;
extern int g_sfx_last_arg_c;
extern int g_sfx_id_count;
extern int g_sfx_id_log[64];
extern int g_ail_stop_sample_calls;
extern int g_ail_init_sample_calls;
extern int g_ail_set_sample_addr_calls;
extern int g_ail_start_sample_calls;
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


/* ---- Test: fd2_play_sfx_with_handle (direct) ---- */

/* Gate behaviour: with any of the three audio gates closed the real player must
 * return without touching the AIL layer (no stop, no init, no start). Exercise
 * each closed-gate combination and confirm zero AIL activity. */
static void test_play_sfx_gates_block(void)
{
    uint32 base;

    base = audiofix_make_bank(8);
    data_fd2_audio_sfx_sample_handle_0 = (void *)0x1234;

    /* driver flag off */
    data_fd2_audio_sfx_driver_available_flag = 0;
    data_fd2_audio_sfx_enabled_flag = 1;
    data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 0;
    g_ail_stop_sample_calls = 0;
    g_ail_start_sample_calls = 0;
    fd2_play_sfx_with_handle(base, 3, 1);
    ASSERT_EQ((long)g_ail_stop_sample_calls, 0);
    ASSERT_EQ((long)g_ail_start_sample_calls, 0);

    /* sample-system flag off */
    data_fd2_audio_sfx_driver_available_flag = 1;
    data_fd2_audio_sfx_enabled_flag = 0;
    g_ail_stop_sample_calls = 0;
    fd2_play_sfx_with_handle(base, 3, 1);
    ASSERT_EQ((long)g_ail_stop_sample_calls, 0);

    /* cinematic/terrain override on */
    data_fd2_audio_sfx_enabled_flag = 1;
    data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 7;
    g_ail_stop_sample_calls = 0;
    fd2_play_sfx_with_handle(base, 3, 1);
    ASSERT_EQ((long)g_ail_stop_sample_calls, 0);
}

/* Stop-only path: gates open, sfx_id == -1 stops the active sample and returns
 * before any address/init/start programming (init/set_addr/start stay at 0). */
static void test_play_sfx_stop_only(void)
{
    uint32 base;

    base = audiofix_make_bank(8);
    audiofix_enable_sfx();
    data_fd2_audio_sfx_sample_handle_0 = (void *)0x55;

    g_ail_stop_sample_calls = 0;
    g_ail_init_sample_calls = 0;
    g_ail_set_sample_addr_calls = 0;
    g_ail_start_sample_calls = 0;

    fd2_play_sfx_with_handle(base, -1, 1);

    ASSERT_EQ((long)g_ail_stop_sample_calls, 1);
    ASSERT_EQ((long)g_ail_init_sample_calls, 0);
    ASSERT_EQ((long)g_ail_set_sample_addr_calls, 0);
    ASSERT_EQ((long)g_ail_start_sample_calls, 0);
}

/* Normal play path: gates open, sfx_id != -1 -> stop, then init + program the
 * sample slot + set loop count + start, exactly once each. The fixture bank
 * encodes length(id)==id, so the captured length (g_sfx_last_id) recovers the
 * fired id, and the captured start equals bank_base + offset(id). Drive ids 0
 * and 5 and verify the full AIL call sequence, the recovered id, the address
 * arithmetic, and that loop_count is forwarded verbatim. */
static void test_play_sfx_normal_play(void)
{
    uint32 base;

    base = audiofix_make_bank(8);
    audiofix_enable_sfx();
    data_fd2_audio_sfx_sample_handle_0 = (void *)0x77;

    /* id 5, loop 1 */
    g_ail_stop_sample_calls = 0;
    g_ail_init_sample_calls = 0;
    g_ail_set_sample_addr_calls = 0;
    g_ail_start_sample_calls = 0;
    g_sfx_last_id = 0;
    g_sfx_last_arg_a = 0;
    g_sfx_last_arg_c = 0;

    fd2_play_sfx_with_handle(base, 5, 1);

    ASSERT_EQ((long)g_ail_stop_sample_calls, 1);
    ASSERT_EQ((long)g_ail_init_sample_calls, 1);
    ASSERT_EQ((long)g_ail_set_sample_addr_calls, 1);
    ASSERT_EQ((long)g_ail_start_sample_calls, 1);
    ASSERT_EQ((long)g_sfx_last_id, 5);                 /* length(5) == 5    */
    /* start = base + offset(5); offset(5) = 5*4/2 = 10 (triangular tri(5)) */
    ASSERT_EQ((long)g_sfx_last_arg_a, (long)(base + 10u));
    ASSERT_EQ((long)g_sfx_last_arg_c, 1);             /* loop_count forwarded */

    /* id 0, loop 3: offset(0)==0 -> start==base, length 0 */
    g_sfx_last_id = 0xAB;
    g_sfx_last_arg_a = 0;
    g_sfx_last_arg_c = 0;
    fd2_play_sfx_with_handle(base, 0, 3);
    ASSERT_EQ((long)g_sfx_last_id, 0);
    ASSERT_EQ((long)g_sfx_last_arg_a, (long)base);
    ASSERT_EQ((long)g_sfx_last_arg_c, 3);
}


/* ---- Test: fd2_play_sfx_sample_from_bank (direct) ---- */

/* Gate behaviour: identical three-gate guard as fd2_play_sfx_with_handle. With
 * any gate closed the real player returns before touching the AIL layer. Each
 * closed-gate combination must yield zero AIL activity (no stop). */
static void test_play_sfx_from_bank_gates_block(void)
{
    uint32 base;

    base = audiofix_make_bank(8);
    data_fd2_audio_sfx_sample_handle_1 = (void *)0x5EE80000;

    /* driver flag off */
    data_fd2_audio_sfx_driver_available_flag = 0;
    data_fd2_audio_sfx_enabled_flag = 1;
    data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 0;
    g_ail_stop_sample_calls = 0;
    g_ail_start_sample_calls = 0;
    fd2_play_sfx_sample_from_bank(base, 3, 1);
    ASSERT_EQ((long)g_ail_stop_sample_calls, 0);
    ASSERT_EQ((long)g_ail_start_sample_calls, 0);

    /* sample-system flag off */
    data_fd2_audio_sfx_driver_available_flag = 1;
    data_fd2_audio_sfx_enabled_flag = 0;
    g_ail_stop_sample_calls = 0;
    fd2_play_sfx_sample_from_bank(base, 3, 1);
    ASSERT_EQ((long)g_ail_stop_sample_calls, 0);

    /* cinematic/terrain override on */
    data_fd2_audio_sfx_enabled_flag = 1;
    data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 7;
    g_ail_stop_sample_calls = 0;
    fd2_play_sfx_sample_from_bank(base, 3, 1);
    ASSERT_EQ((long)g_ail_stop_sample_calls, 0);
}

/* Stop-only path: gates open, sfx_id == 0xFFFFFFFF (-1) stops the active sample
 * on slot 1 and returns before any init/address/start programming. The stop is
 * routed to the slot-1 counter (g_play_sfx_sample_from_bank_calls) by the AIL
 * spy, confirming this player drives handle_1, not handle_0. */
static void test_play_sfx_from_bank_stop_only(void)
{
    uint32 base;

    base = audiofix_make_bank(8);
    audiofix_enable_sfx();
    data_fd2_audio_sfx_sample_handle_1 = (void *)0x5EE80000;

    g_ail_stop_sample_calls = 0;
    g_ail_init_sample_calls = 0;
    g_ail_set_sample_addr_calls = 0;
    g_ail_start_sample_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    g_play_sfx_with_handle_calls = 0;

    fd2_play_sfx_sample_from_bank(base, 0xFFFFFFFF, 1);

    ASSERT_EQ((long)g_ail_stop_sample_calls, 1);
    ASSERT_EQ((long)g_ail_init_sample_calls, 0);
    ASSERT_EQ((long)g_ail_set_sample_addr_calls, 0);
    ASSERT_EQ((long)g_ail_start_sample_calls, 0);
    /* the one stop routed to the slot-1 (this function's) counter */
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* Normal play path: gates open, sfx_id != -1 -> stop, init, program the sample
 * slot, set loop count, start (once each). The fixture bank encodes
 * length(id)==id and start==base+offset(id); drive ids 5 and 0 and verify the
 * full AIL sequence, the recovered id, the address arithmetic, that loop_count
 * forwards verbatim, and that the programmed handle is slot 1 (handle_1) — the
 * sole behavioural difference from fd2_play_sfx_with_handle. */
static void test_play_sfx_from_bank_normal_play(void)
{
    uint32 base;

    base = audiofix_make_bank(8);
    audiofix_enable_sfx();
    /* distinct sentinels so the captured handle proves slot 1, not slot 0 */
    data_fd2_audio_sfx_sample_handle_0 = (void *)0x5EE40000;
    data_fd2_audio_sfx_sample_handle_1 = (void *)0x5EE80000;

    /* id 5, loop 1 */
    g_ail_stop_sample_calls = 0;
    g_ail_init_sample_calls = 0;
    g_ail_set_sample_addr_calls = 0;
    g_ail_start_sample_calls = 0;
    g_sfx_last_id = 0;
    g_sfx_last_arg_a = 0;
    g_sfx_last_arg_c = 0;
    g_sfx_last_handle = 0;

    fd2_play_sfx_sample_from_bank(base, 5, 1);

    ASSERT_EQ((long)g_ail_stop_sample_calls, 1);
    ASSERT_EQ((long)g_ail_init_sample_calls, 1);
    ASSERT_EQ((long)g_ail_set_sample_addr_calls, 1);
    ASSERT_EQ((long)g_ail_start_sample_calls, 1);
    ASSERT_EQ((long)g_sfx_last_id, 5);                 /* length(5) == 5    */
    /* start = base + offset(5); offset(5) = tri(5) = 10 */
    ASSERT_EQ((long)g_sfx_last_arg_a, (long)(base + 10u));
    ASSERT_EQ((long)g_sfx_last_arg_c, 1);             /* loop_count forwarded */
    /* programmed sample slot is handle_1 (slot 1), the distinguishing behaviour */
    ASSERT_EQ((long)g_sfx_last_handle, (long)0x5EE80000);

    /* id 0, loop 3: offset(0)==0 -> start==base, length 0 */
    g_sfx_last_id = 0xAB;
    g_sfx_last_arg_a = 0;
    g_sfx_last_arg_c = 0;
    fd2_play_sfx_sample_from_bank(base, 0, 3);
    ASSERT_EQ((long)g_sfx_last_id, 0);
    ASSERT_EQ((long)g_sfx_last_arg_a, (long)base);
    ASSERT_EQ((long)g_sfx_last_arg_c, 3);
}


/* ---- Test: fd2_play_and_free_status_effect_sfx ---- */

/* The function's whole semantic is: play the loaded SFX bank in kill-all mode
 * (fd2_play_sfx_with_handle(handle, -1, 1)) then free the bank buffer. With the
 * gates open, the -1 (kill-all) call reaches the real player's stop-only path,
 * which stops the active sample exactly once and issues no init/start. Drive it
 * with a real malloc'd buffer in the handle slot and confirm via the AIL stop
 * spy that one stop fired and no sample was programmed. The real free(handle)
 * inside the function releases the buffer, so the test must NOT free it again
 * (would double-free); it only nulls the global afterward. */
static void test_play_and_free_status_effect_sfx(void)
{
    uint8 *buf;

    buf = (uint8 *)malloc(64);
    ASSERT_TRUE(buf != 0);

    audiofix_enable_sfx();
    data_fd2_audio_sfx_sample_handle_0 = (void *)0x99;
    data_fd2_audio_status_effect_sfx_handle_ptr = (uint32)buf;
    g_ail_stop_sample_calls = 0;
    g_ail_init_sample_calls = 0;
    g_ail_start_sample_calls = 0;

    fd2_play_and_free_status_effect_sfx();

    ASSERT_EQ((long)g_ail_stop_sample_calls, 1);   /* kill-all stop fired */
    ASSERT_EQ((long)g_ail_init_sample_calls, 0);   /* stop-only: no program */
    ASSERT_EQ((long)g_ail_start_sample_calls, 0);

    /* buf was freed by the function under test; just clear the dangling global */
    data_fd2_audio_status_effect_sfx_handle_ptr = 0;
}


/* ---- Test: fd2_load_figani_sfx_bank ---- */

extern const uint8 data_fd2_audio_figani_sfx_bank_fdother_index_lut[6];

/* Core translation path: figani_data[+4] is a 1-based id into the 6-byte
 * FDOTHER index LUT; the function loads FDOTHER.DAT entry lut[id-1] via the
 * REAL fd2_load_dat_resource and returns its buffer. For each id 1..6 build an
 * in-memory FIGANI header (only [+4] is read — NOT a game file), drive the
 * function, and cross-check the returned buffer's payload + the size side
 * effect against an INDEPENDENT parse (realfile.h) of the SAME real FDOTHER
 * entry lut[id-1]. Proves: the 1-based LUT lookup, the exact LUT bytes, the
 * right filename/index forwarded, and the EAX return-value capture. */
static void test_load_figani_sfx_bank_translate_real(void)
{
    static const uint8 lut_truth[6] =
        {0x30, 0x31, 0x32, 0x33, 0x34, 0x35};   /* binary @0x525D6 */
    uint8   figani[8];
    uint8  *ref;
    long    ref_size;
    uint32  got;
    int     id;

    /* anchor the LUT bytes to the binary's ground truth (independent of the
     * loader cross-check below) so neither side can drift together silently */
    ASSERT_EQ((long)memcmp(
        data_fd2_audio_figani_sfx_bank_fdother_index_lut,
        lut_truth, 6), 0);

    for (id = 1; id <= 6; ++id) {
        int idx = (int)data_fd2_audio_figani_sfx_bank_fdother_index_lut[id - 1];

        ref_size = realdat_read_resource("FDOTHER.DAT", idx, &ref);
        ASSERT_TRUE(ref_size > 0);

        memset(figani, 0, sizeof(figani));
        figani[4] = (uint8)id;
        data_fd2_resource_last_loaded_resource_size = 0;

        got = fd2_load_figani_sfx_bank((uint32)figani);

        ASSERT_TRUE(got != 0);
        ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, ref_size);
        ASSERT_EQ((long)memcmp((void *)got, ref, (size_t)ref_size), 0);

        free((void *)got);
        free(ref);
    }
}

/* NULL figani_data short-circuits before any load: returns 0 and does not
 * touch the loader (size global stays at its pre-poison sentinel). */
static void test_load_figani_sfx_bank_null(void)
{
    uint32 got;

    data_fd2_resource_last_loaded_resource_size = 0x1234;
    got = fd2_load_figani_sfx_bank(0);
    ASSERT_EQ((long)got, 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, 0x1234);
}

/* figani_data[+4] == 0 means "no SFX bank": returns 0 without loading (size
 * global untouched), even though figani_data itself is non-NULL. */
static void test_load_figani_sfx_bank_no_ref(void)
{
    uint8  figani[8];
    uint32 got;

    memset(figani, 0, sizeof(figani));      /* [+4] == 0 */
    data_fd2_resource_last_loaded_resource_size = 0x5678;
    got = fd2_load_figani_sfx_bank((uint32)figani);
    ASSERT_EQ((long)got, 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, 0x5678);
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
    RUN_TEST(test_play_sfx_gates_block);
    RUN_TEST(test_play_sfx_stop_only);
    RUN_TEST(test_play_sfx_normal_play);
    RUN_TEST(test_play_sfx_from_bank_gates_block);
    RUN_TEST(test_play_sfx_from_bank_stop_only);
    RUN_TEST(test_play_sfx_from_bank_normal_play);
    RUN_TEST(test_play_and_free_status_effect_sfx);
    RUN_TEST(test_load_figani_sfx_bank_translate_real);
    RUN_TEST(test_load_figani_sfx_bank_null);
    RUN_TEST(test_load_figani_sfx_bank_no_ref);
    audiofix_disable_sfx();   /* restore safe gate state for later suites */
    printf("\n");
}
