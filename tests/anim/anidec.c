/*
 * unit tests for src/anim/anidec.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

#define USE_ITEM_ID 10

extern runtime_char g_test_rc_array[8];
extern int g_build_spell_list_return;
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
extern int g_play_sfx_sample_from_bank_calls;
extern int g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int g_blit_indexed_sprite_last_x;
extern int g_blit_indexed_sprite_last_y;
extern int    g_mini_panel_calls;
extern uint32 g_mini_panel_last_buf;
extern uint32 g_mini_panel_last_stride;
extern uint32 g_mini_panel_last_char;
extern int g_find_equipped_return;
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
extern int g_count_usable_slots_return;
extern uint8 g_spell_list_buf[12];
extern int g_remove_inventory_calls;
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;


/* ---- ANI decoder tests ---- */

static uint8 g_test_palette_buf[768];

static uint8 g_test_row_buf[320];


static void test_ani_palette_fill_byte(void)
{
    uint8 stream[2];
    stream[0] = 0;
    stream[1] = 0x42;
    data_fd2_animation_ani_decoder_src_buf = (uint32)g_test_palette_buf;
    memset(g_test_palette_buf, 0, 768);
    data_fd2_animation_ani_decoder_frame_dispatch_table[0] =
        (void *)fd2_ani_decoder_chunk_palette_fill_byte;
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_palette_buf[0], 0x42);
    ASSERT_EQ((long)g_test_palette_buf[767], 0x42);
}


static void test_ani_row_copy_literal(void)
{
    uint8 stream[6];
    stream[0] = 1;
    stream[1] = 0xAA;
    stream[2] = 0xBB;
    stream[3] = 0xCC;
    stream[4] = 0xDD;
    stream[5] = 0xEE;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_target_width = 5;
    data_fd2_animation_ani_decoder_frame_dispatch_table[1] =
        (void *)fd2_ani_decoder_chunk_row_copy_literal;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[0], 0xAA);
    ASSERT_EQ((long)g_test_row_buf[4], 0xEE);
}


static void test_ani_sparse_set_byte(void)
{
    uint8 stream[10];
    stream[0] = 2;
    *(uint16 *)(stream + 1) = 2;
    *(uint16 *)(stream + 3) = 5;
    stream[5] = 0x77;
    *(uint16 *)(stream + 6) = 10;
    stream[8] = 0x88;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_frame_dispatch_table[2] =
        (void *)fd2_ani_decoder_chunk_sparse_set_byte;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[5], 0x77);
    ASSERT_EQ((long)g_test_row_buf[10], 0x88);
}


static void test_ani_decode_frame_dispatch(void)
{
    uint8 stream[4];
    stream[0] = 3;
    stream[1] = 0x55;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_target_width = 4;
    data_fd2_animation_ani_decoder_frame_dispatch_table[3] =
        (void *)fd2_ani_decoder_chunk_row_fill_byte;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[0], 0x55);
    ASSERT_EQ((long)g_test_row_buf[3], 0x55);
}


static void test_ani_decoder_set_target_buffer(void)
{
    fd2_ani_decoder_set_target_buffer(320, 0x12345, 0xABCDE);
    ASSERT_EQ(data_fd2_animation_ani_decoder_target_width, 320);
    ASSERT_EQ(data_fd2_animation_ani_decoder_dst_buf, 0x12345);
    ASSERT_EQ(data_fd2_animation_ani_decoder_src_buf, 0xABCDE);
}


void run_anim_anidec_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anidec\n");
    RUN_TEST(test_ani_decoder_set_target_buffer);
    RUN_TEST(test_ani_palette_fill_byte);
    RUN_TEST(test_ani_row_copy_literal);
    RUN_TEST(test_ani_sparse_set_byte);
    RUN_TEST(test_ani_decode_frame_dispatch);
    printf("\n");
}
