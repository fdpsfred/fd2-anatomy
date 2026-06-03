/*
 * unit tests for src/ui_menu/menucfg.c
 *
 * fd2_game_options_menu_loop is an infinite settings loop that exits only when
 * fd2_settings_menu_input_step returns -1 (cancel). The test harness drives it
 * through the input-step / cursor seams in testglob.c:
 *   - g_settings_select_once = 1 makes the first input-step call return 1
 *     (selection of g_settings_cursor_idx) and the next call return -1 (cancel),
 *     so the loop runs exactly one toggle iteration and then exits.
 *   - g_settings_input_step_return = -1 (select_once = 0) exercises the plain
 *     cancel early-out with no toggle.
 *
 * Covered branches: cancel early-out, BGM toggle (cursor 0, including the
 * AIL_set_sequence_volume fade value / ramp), SFX toggle (else / cursor 1),
 * game-speed toggle (cursor 2), terrain-HUD toggle (cursor 3). The label
 * rebuild at the top of each iteration is purely text-token state feeding the
 * (stubbed) dialog renderer, so it has no observable seam here.
 */

#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* settings-loop seams (defined in testglob.c) */
extern int g_settings_input_step_return;
extern int g_settings_cursor_idx;
extern int g_settings_select_once;
extern int g_open_settings_dialog_calls;
extern int g_close_settings_dialog_calls;
extern int g_settings_input_step_calls;

/* AIL_set_sequence_volume tracking (defined in testglob.c) */
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;

/* Drive exactly one selection of `cursor`, then cancel. */
static void cfg_reset_select(int cursor)
{
    g_settings_cursor_idx = cursor;
    g_settings_select_once = 1;
    g_open_settings_dialog_calls = 0;
    g_close_settings_dialog_calls = 0;
    g_settings_input_step_calls = 0;
    g_ail_vol_calls = 0;
    g_ail_last_vol = 0;
    g_ail_last_ramp = 0;
    data_fd2_ui_menu_cursor_idx = 0;
}

/* Cancel immediately: no toggles, dialog opened and closed exactly once. */
static void test_options_cancel(void)
{
    uint8 bgm0, sfx0, spd0, hud0;

    g_settings_select_once = 0;
    g_settings_input_step_return = -1;
    g_open_settings_dialog_calls = 0;
    g_close_settings_dialog_calls = 0;
    g_ail_vol_calls = 0;

    bgm0 = data_fd2_audio_bgm_enabled_flag;
    sfx0 = data_fd2_audio_sfx_enabled_flag;
    spd0 = data_fd2_ui_game_speed_flag;
    hud0 = data_fd2_ui_terrain_hud_user_enabled;

    fd2_game_options_menu_loop();

    ASSERT_EQ(g_open_settings_dialog_calls, 1);
    ASSERT_EQ(g_close_settings_dialog_calls, 1);
    ASSERT_EQ(g_ail_vol_calls, 0);
    ASSERT_EQ(data_fd2_audio_bgm_enabled_flag, bgm0);
    ASSERT_EQ(data_fd2_audio_sfx_enabled_flag, sfx0);
    ASSERT_EQ(data_fd2_ui_game_speed_flag, spd0);
    ASSERT_EQ(data_fd2_ui_terrain_hud_user_enabled, hud0);
}

/* cursor 0, BGM currently OFF -> toggles ON, fades volume to 0x7F over 1000ms. */
static void test_options_bgm_enable(void)
{
    cfg_reset_select(0);
    data_fd2_audio_bgm_enabled_flag = 0;

    fd2_game_options_menu_loop();

    ASSERT_EQ(data_fd2_audio_bgm_enabled_flag, 1);
    ASSERT_EQ(g_ail_vol_calls, 1);
    ASSERT_EQ(g_ail_last_vol, 0x7f);
    ASSERT_EQ(g_ail_last_ramp, 1000);
}

/* cursor 0, BGM currently ON -> toggles OFF, fades volume to 0 over 1000ms. */
static void test_options_bgm_disable(void)
{
    cfg_reset_select(0);
    data_fd2_audio_bgm_enabled_flag = 1;

    fd2_game_options_menu_loop();

    ASSERT_EQ(data_fd2_audio_bgm_enabled_flag, 0);
    ASSERT_EQ(g_ail_vol_calls, 1);
    ASSERT_EQ(g_ail_last_vol, 0);
    ASSERT_EQ(g_ail_last_ramp, 1000);
}

/* cursor 1 (else branch): SFX flag flips 0 -> 1 and 1 -> 0, no AIL call. */
static void test_options_sfx_toggle(void)
{
    cfg_reset_select(1);
    data_fd2_audio_sfx_enabled_flag = 0;
    fd2_game_options_menu_loop();
    ASSERT_EQ(data_fd2_audio_sfx_enabled_flag, 1);
    ASSERT_EQ(g_ail_vol_calls, 0);

    cfg_reset_select(1);
    data_fd2_audio_sfx_enabled_flag = 1;
    fd2_game_options_menu_loop();
    ASSERT_EQ(data_fd2_audio_sfx_enabled_flag, 0);
}

/* cursor 2: game-speed flag flips via XOR 1. */
static void test_options_speed_toggle(void)
{
    cfg_reset_select(2);
    data_fd2_ui_game_speed_flag = 0;
    fd2_game_options_menu_loop();
    ASSERT_EQ(data_fd2_ui_game_speed_flag, 1);
    ASSERT_EQ(g_ail_vol_calls, 0);

    cfg_reset_select(2);
    data_fd2_ui_game_speed_flag = 1;
    fd2_game_options_menu_loop();
    ASSERT_EQ(data_fd2_ui_game_speed_flag, 0);
}

/* cursor 3: terrain-HUD user-enabled flag flips via XOR 1. */
static void test_options_terrain_hud_toggle(void)
{
    cfg_reset_select(3);
    data_fd2_ui_terrain_hud_user_enabled = 0;
    fd2_game_options_menu_loop();
    ASSERT_EQ(data_fd2_ui_terrain_hud_user_enabled, 1);

    cfg_reset_select(3);
    data_fd2_ui_terrain_hud_user_enabled = 1;
    fd2_game_options_menu_loop();
    ASSERT_EQ(data_fd2_ui_terrain_hud_user_enabled, 0);
}

/* fd2_count_active_menu_items_until_zero: count leading non-zero entries (max 4)
 * of a 4-slot int menu definition into data_fd2_ui_menu_cursor_idx. */

/* First slot zero -> count 0. */
static void test_count_first_zero(void)
{
    int32 def[4];
    def[0] = 0;
    def[1] = 7;
    def[2] = 7;
    def[3] = 7;
    data_fd2_ui_menu_cursor_idx = 99;
    fd2_count_active_menu_items_until_zero(def);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 0);
}

/* Stops at the first zero in the middle. */
static void test_count_partial(void)
{
    int32 def[4];
    def[0] = 5;
    def[1] = 6;
    def[2] = 0;
    def[3] = 9;
    data_fd2_ui_menu_cursor_idx = 99;
    fd2_count_active_menu_items_until_zero(def);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 2);
}

/* All four non-zero -> count caps at 4 (loop bound), never reads def[4]. */
static void test_count_all_four(void)
{
    int32 def[5];
    def[0] = 1;
    def[1] = 2;
    def[2] = 3;
    def[3] = 4;
    def[4] = 0xDEAD; /* must be ignored by the < 4 bound */
    data_fd2_ui_menu_cursor_idx = 0;
    fd2_count_active_menu_items_until_zero(def);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 4);
}

/* Exactly one active entry. */
static void test_count_one(void)
{
    int32 def[4];
    def[0] = -1; /* non-zero negative still counts */
    def[1] = 0;
    def[2] = 0;
    def[3] = 0;
    data_fd2_ui_menu_cursor_idx = 99;
    fd2_count_active_menu_items_until_zero(def);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 1);
}

void run_ui_menu_menucfg_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/menucfg\n");
    RUN_TEST(test_options_cancel);
    RUN_TEST(test_options_bgm_enable);
    RUN_TEST(test_options_bgm_disable);
    RUN_TEST(test_options_sfx_toggle);
    RUN_TEST(test_options_speed_toggle);
    RUN_TEST(test_options_terrain_hud_toggle);
    RUN_TEST(test_count_first_zero);
    RUN_TEST(test_count_partial);
    RUN_TEST(test_count_all_four);
    RUN_TEST(test_count_one);
    printf("\n");
}
