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


/* Blit position arithmetic (high-risk numeric path), non-enemy team (no
 * +0x14). The vertical blit position is the 3rd fd2_blit_indexed_sprite
 * argument (captured as last_x by the stub; the stub's "y" param receives
 * row_stride):
 *   pos = origin_y + y_offsets[color] - v_offsets[color]*row_stride
 *   sprite_id = spr_offsets[color] + frame
 * color 3: y_off=50, v_off=2, spr_off=7. origin_y=100, row_stride=10,
 * frame=4 (in range, !=0/3/0xB so no SFX/done/rotation) -> only slot 0
 * blits. Expected sprite_id = 7+4 = 11; pos = 100 + 50 - 2*10 = 130.
 * Toggle 0 -> 1 so the update block is OFF (frame stays 4), isolating
 * the blit math. State 2. */
static void test_summon_main_tick_blit_y_arithmetic(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_y_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12color_v_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12color_sprite_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_y_offset_table[3] = 50;
    data_fd2_battle_summon_main_anim_12color_v_offset_table[3] = 2;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[3] = 7;
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 4;
    data_fd2_battle_summon_main_anim_12slot_color_idx_array[0] = 3;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_frame = 0;
    g_blit_indexed_sprite_last_x = 0;
    fd2_tick_summon_spell_main_animation_state(0, 0, 100, 10, 2);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 11);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 130);
    /* toggle was 0 -> became 1: update gated off, frame unchanged */
    ASSERT_EQ((long)data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0], 4);
}


/* Team (enemy) Y-adjust: bTeam==0 adds 0x14 to every y_offset before the
 * blit math. Same inputs as the arithmetic test but team=0 ->
 * pos = origin_y + (y_offsets[color]+0x14) - v_offsets[color]*row_stride
 *     = 100 + (50+20) - 2*10 = 150. sprite_id unchanged (11). State 2,
 * toggle 0 -> 1 isolates blit. */
static void test_summon_main_tick_blit_y_enemy_team_offset(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 0;
    data_fd2_battle_summon_main_anim_odd_even_frame_toggle = 0;
    for (i = 0; i < 12; i++) {
        data_fd2_battle_summon_main_anim_12slot_y_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12color_v_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12color_sprite_offset_table[i] = 0;
        data_fd2_battle_summon_main_anim_12slot_frame_counter_array[i] = -4;
        data_fd2_battle_summon_main_anim_12slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_main_anim_12slot_y_offset_table[3] = 50;
    data_fd2_battle_summon_main_anim_12color_v_offset_table[3] = 2;
    data_fd2_battle_summon_main_anim_12color_sprite_offset_table[3] = 7;
    data_fd2_battle_summon_main_anim_12slot_frame_counter_array[0] = 4;
    data_fd2_battle_summon_main_anim_12slot_color_idx_array[0] = 3;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_frame = 0;
    g_blit_indexed_sprite_last_x = 0;
    fd2_tick_summon_spell_main_animation_state(0, 0, 100, 10, 2);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 11);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 150);
}


static void test_summon_generic_reset(void)
{
    int r;
    data_fd2_battle_summon_spell_anim_phase_byte = 0xFF;
    r = fd2_tick_summon_spell_animation_state(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 0x1D);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0);
}


static void test_summon_generic_state3(void)
{
    int r;
    r = fd2_tick_summon_spell_animation_state(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0xC);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0x10);
}


static void test_summon_generic_state5_advance(void)
{
    int r;
    data_fd2_battle_summon_spell_anim_phase_byte = 0x10;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_spell_animation_state(0, 0, 100, 320, 5);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0x11);
    ASSERT_EQ((long)r, 1);
}


/* State 6: SFX chime + persistent phase write 0xA + distinct return 0xA.
 * asm 0x265af-0x265cd: PUSH 3/CALL sfx; MOV [phase],0xa; MOV EAX,0xa; return.
 * Same risk class (persistent mutation + distinct frame-budget value) as
 * the tested state 3. Inputs other than state_code do not affect this path. */
static void test_summon_generic_state6(void)
{
    int r;
    data_fd2_battle_summon_spell_anim_phase_byte = 0xFF;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_play_sfx_with_handle_calls = 0;
    r = fd2_tick_summon_spell_animation_state(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 0xA);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0xA);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
}


/* State 5 wrap path: phase 0x11 -> INC -> 0x12 -> reset to 0x10, return 0.
 * Complements state5_advance (0x10->0x11). asm 0x26701 INC; 0x2670e CMP 0x11
 * (miss); 0x2672c CMP 0x12 (hit) -> 0x26731 MOV [phase],0x10; return EDI=0.
 * team=2 -> is_enemy=0, so only the single unconditional blit fires; the
 * 0x11 SFX branch is skipped (phase is 0x12 after INC). */
static void test_summon_generic_state5_wrap(void)
{
    int r;
    data_fd2_battle_summon_spell_anim_phase_byte = 0x11;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    r = fd2_tick_summon_spell_animation_state(0, 0, 100, 320, 5);
    ASSERT_EQ((long)data_fd2_battle_summon_spell_anim_phase_byte, 0x10);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}


/* fd2_tick_sprite_animation_step @ 0x2673f — the per-frame pacing primitive
 * used by the summon state machine (call sites 0x2660e/0x2667b are gated on
 * state codes 1/2/7/8, which no summon-state test exercises, so this needs a
 * direct test). asm-verified (0002677e-00026790):
 *   blit(atlas, *p_frame_idx, x, y, -1)              // pre-advance frame index
 *   frame_off = *(uint32*)(atlas + *p_frame_idx*4 + 8)
 *   hold_time = *(uint8*)(atlas + frame_off + 6)      // MOVZX -> unsigned byte
 *   INC byte[p_tick]                                  // (*p_tick)++
 *   if (*p_tick == hold_time) { *p_tick = 0; INC byte[p_frame_idx]; }
 * Atlas built in a local buffer: table uint32 at +8 (idx 0) -> frame_off=16,
 * so hold_time lives at +22 (no overlap with the +8..+11 table bytes). */
static void test_tick_sprite_animation_step_hold_not_reached(void)
{
    uint8 atlas[64];
    uint8 frame_idx;
    uint8 tick;

    memset(atlas, 0, sizeof(atlas));
    *(uint32 *)(atlas + 8) = 16;     /* frame_off for idx 0 */
    atlas[16 + 6] = 3;               /* hold_time = 3 */
    frame_idx = 0;
    tick = 0;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_frame = 0;
    g_blit_indexed_sprite_last_x = 0;
    g_blit_indexed_sprite_last_y = 0;
    fd2_tick_sprite_animation_step(&frame_idx, &tick, 50, 70, (uint32)atlas);
    /* tick 0 -> 1, != hold 3: no advance; blit used pre-advance idx 0 */
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, 50);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_y, 70);
    ASSERT_EQ((long)tick, 1);
    ASSERT_EQ((long)frame_idx, 0);
}


/* Hold reached: tick 2 -> 3 == hold_time 3 -> tick resets to 0 and frame_idx
 * advances 0 -> 1. The blit still renders the PRE-advance index (0), proving
 * the render precedes the frame-advance. */
static void test_tick_sprite_animation_step_hold_reached(void)
{
    uint8 atlas[64];
    uint8 frame_idx;
    uint8 tick;

    memset(atlas, 0, sizeof(atlas));
    *(uint32 *)(atlas + 8) = 16;     /* frame_off for idx 0 */
    atlas[16 + 6] = 3;               /* hold_time = 3 */
    frame_idx = 0;
    tick = 2;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_sprite_last_frame = 0xFF;
    fd2_tick_sprite_animation_step(&frame_idx, &tick, 50, 70, (uint32)atlas);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, 0);
    ASSERT_EQ((long)tick, 0);
    ASSERT_EQ((long)frame_idx, 1);
}


static void test_summon_b_init(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_shared_rng_seed = 100;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[3], -6);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_color_rotation_counter, 6);
}


static void test_summon_b_state3(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0xC);
}


static void test_summon_b_state6_terminate(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 8);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_terminate_flag, 1);
}


/* TICK (state 2): every in-range slot advances frame_counter by 1.
 * Frames all 3 -> all become 4: in visible range so all 6 blit, none
 * reaches done(2) or rotation(7), no slot at frame 0 so no SFX. */
static void test_summon_b_tick_increment_no_done(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 3;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    for (i = 0; i < 6; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i], 4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 6);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}


/* done_flag (return 1) is set iff some slot's post-increment counter == 2.
 * slot 2 goes 1->2 (done); other slots 4->5 (no done). State 5 also TICKs. */
static void test_summon_b_tick_done_at_frame2(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[2] = 1;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[2], 2);
}


/* Blit visibility guard: only slots with pre-increment frame in [0,6) blit.
 * frames [-1,0,5,6,3,-3] -> in range: slots 1,2,4 (3 blits). State 8 TICKs. */
static void test_summon_b_tick_blit_visibility_guard(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    g_blit_indexed_sprite_calls = 0;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0] = -1;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[1] = 0;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[2] = 5;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[3] = 6;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[4] = 3;
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[5] = -3;
    fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 8);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 3);
}


/* 2-bucket frame-0 SFX: all frames 0 -> slot 0 plays with_handle, slot 3
 * plays sample_from_bank, slots 1,2,4,5 silent. All 6 blit (0 in range). */
static void test_summon_b_tick_sfx_bucket_split(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 0;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 6);
    ASSERT_EQ((long)r, 0);
    for (i = 0; i < 6; i++)
        ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i], 1);
}


/* Color rotation at frame 7 (terminate_flag==0): counter=(counter+1)%10,
 * color_idx[i]=counter, frame_counter[i]=0, jitter[i]=(rng%2)*6.
 * seed=8192 -> first RNG output 0x80a5 (odd) -> jitter 6. */
static void test_summon_b_tick_color_rotation(void)
{
    int r;
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_b_color_rotation_counter = 3;
    data_fd2_shared_rng_seed = 8192;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
        data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0] = 6;
    r = fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_color_rotation_counter, 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[0], 4);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[0], 6);
    ASSERT_EQ((long)r, 0);
}


/* terminate_flag==1 blocks the frame-7 rotation entirely: frame stays 7,
 * counter and color_idx unchanged. */
static void test_summon_b_tick_rotation_blocked_by_terminate(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 1;
    data_fd2_battle_summon_anim_variant_b_color_rotation_counter = 3;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0] = 6;
    fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0], 7);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_color_rotation_counter, 3);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[0], 0);
}


/* Rotation counter wraps mod 10: counter 9 -> (9+1)%10 == 0. */
static void test_summon_b_tick_rotation_mod10_wrap(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    data_fd2_battle_summon_anim_variant_b_terminate_flag = 0;
    data_fd2_battle_summon_anim_variant_b_color_rotation_counter = 9;
    data_fd2_shared_rng_seed = 8192;
    for (i = 0; i < 6; i++) {
        data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[i] = 4;
        data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[i] = 0;
    }
    data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[0] = 6;
    fd2_tick_summon_anim_variant_b_6slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_color_rotation_counter, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[0], 0);
}


static void test_summon_e_init(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 3);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[0], 0);
    ASSERT_EQ((long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[15], -30);
}


static void test_summon_e_state3(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x22);
}


/* TICK (state 2): every slot advances frame_counter by 1; done_flag (return 1)
 * is set iff some slot's post-increment counter == 4. All 16 frames 3 -> all
 * become 4: all in visible range [0,8) so all 16 blit, every slot reaches
 * done(4), and frame 3 triggers no SFX (not 0, not 4). */
static void test_summon_e_tick_done_and_counter(void)
{
    int r;
    int i;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 16; i++)
        data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i] = 3;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 1);
    for (i = 0; i < 16; i++)
        ASSERT_EQ(
            (long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i],
            4);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 16);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}


/* frame==0 SFX branch: all 16 frames 0 -> each plays with_handle, none plays
 * sample_from_bank; 0 is in [0,8) so all 16 blit; post-increment 1 != 4 so no
 * done. State 5 also TICKs (same path as state 2). */
static void test_summon_e_tick_sfx_frame0(void)
{
    int r;
    int i;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 16; i++)
        data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i] = 0;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 16);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 16);
    for (i = 0; i < 16; i++)
        ASSERT_EQ(
            (long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i],
            1);
}


/* frame==4 SFX branch: all 16 frames 4 -> each plays sample_from_bank, none
 * plays with_handle; 4 is in [0,8) so all 16 blit; post-increment 5 != 4 so no
 * done. */
static void test_summon_e_tick_sfx_frame4(void)
{
    int r;
    int i;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 16; i++)
        data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i] = 4;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 16);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 16);
    for (i = 0; i < 16; i++)
        ASSERT_EQ(
            (long)data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i],
            5);
}


/* Blit visibility gate boundaries: blit fires iff pre-increment frame in [0,8).
 * slot0=-1 (below range, no blit), slot1=7 (top in-range, blit),
 * slot2=8 (above range boundary, no blit), all others -1 (no blit) -> 1 blit.
 * No frame is 0 or 4, so no SFX fires (isolates the blit gate). */
static void test_summon_e_tick_blit_visibility_guard(void)
{
    int i;
    g_blit_indexed_sprite_calls = 0;
    g_play_sfx_with_handle_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    for (i = 0; i < 16; i++)
        data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[i] = -1;
    data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[1] = 7;
    data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[2] = 8;
    fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 2);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, 1);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ((long)g_play_sfx_sample_from_bank_calls, 0);
}


/* state 6: variant-E has no terminate logic, returns 2 with no side effects. */
static void test_summon_e_state6(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 6);
    ASSERT_EQ((long)r, 2);
}


/* default branch: any other state_code returns 0. */
static void test_summon_e_default_state(void)
{
    int r;
    r = fd2_tick_summon_anim_variant_e_16slot(0, 0, 0, 0, 7);
    ASSERT_EQ((long)r, 0);
}


static void test_summon_minor_init_state(void)
{
    int r;
    r = fd2_tick_summon_spell_minor_animation_state(0, 0, 0, 0, 0);
    ASSERT_EQ((long)r, 0x14);
    ASSERT_EQ((long)data_fd2_battle_summon_minor_anim_alternating_blit_toggle, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_minor_anim_state5_frame_counter, 1);
}


static void test_summon_minor_state3_hold(void)
{
    int r;
    r = fd2_tick_summon_spell_minor_animation_state(0, 0, 0, 0, 3);
    ASSERT_EQ((long)r, 0x3C);
}


static void test_summon_minor_state5_ramp(void)
{
    int r;
    data_fd2_battle_summon_minor_anim_state5_frame_counter = 0x11;
    r = fd2_tick_summon_spell_minor_animation_state(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_battle_summon_minor_anim_state5_frame_counter, 0x12);
}


static void test_summon_minor_state5_done(void)
{
    int r;
    data_fd2_battle_summon_minor_anim_state5_frame_counter = 0x2B;
    r = fd2_tick_summon_spell_minor_animation_state(0, 0, 0, 0, 5);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_battle_summon_minor_anim_state5_frame_counter, 0x2C);
}


void run_anim_anisummn2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anisummn (2/2)\n");
    /* Summon tick variants fire fd2_play_sfx_with_handle(summon bank, id, 1);
     * the now-real player needs the audio gates open and a valid bank so the AIL
     * stop spy bumps g_play_sfx_with_handle_calls per fired SFX. No test below
     * resets the gates or bank ptr, so set them once here. */
    audiofix_enable_sfx();
    data_fd2_audio_summon_spell_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);
    RUN_TEST(test_summon_main_tick_blit_y_arithmetic);
    RUN_TEST(test_summon_main_tick_blit_y_enemy_team_offset);
    RUN_TEST(test_summon_generic_reset);
    RUN_TEST(test_summon_generic_state3);
    RUN_TEST(test_summon_generic_state5_advance);
    RUN_TEST(test_summon_generic_state6);
    RUN_TEST(test_summon_generic_state5_wrap);
    RUN_TEST(test_tick_sprite_animation_step_hold_not_reached);
    RUN_TEST(test_tick_sprite_animation_step_hold_reached);
    RUN_TEST(test_summon_b_init);
    RUN_TEST(test_summon_b_state3);
    RUN_TEST(test_summon_b_state6_terminate);
    RUN_TEST(test_summon_b_tick_increment_no_done);
    RUN_TEST(test_summon_b_tick_done_at_frame2);
    RUN_TEST(test_summon_b_tick_blit_visibility_guard);
    RUN_TEST(test_summon_b_tick_sfx_bucket_split);
    RUN_TEST(test_summon_b_tick_color_rotation);
    RUN_TEST(test_summon_b_tick_rotation_blocked_by_terminate);
    RUN_TEST(test_summon_b_tick_rotation_mod10_wrap);
    RUN_TEST(test_summon_e_init);
    RUN_TEST(test_summon_e_state3);
    RUN_TEST(test_summon_e_tick_done_and_counter);
    RUN_TEST(test_summon_e_tick_sfx_frame0);
    RUN_TEST(test_summon_e_tick_sfx_frame4);
    RUN_TEST(test_summon_e_tick_blit_visibility_guard);
    RUN_TEST(test_summon_e_state6);
    RUN_TEST(test_summon_e_default_state);
    RUN_TEST(test_summon_minor_init_state);
    RUN_TEST(test_summon_minor_state3_hold);
    RUN_TEST(test_summon_minor_state5_ramp);
    RUN_TEST(test_summon_minor_state5_done);
    audiofix_disable_sfx();   /* restore safe gate state for later suites */
    printf("\n");
}
