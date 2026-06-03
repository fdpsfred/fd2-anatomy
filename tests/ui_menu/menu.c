/*
 * unit tests for src/ui_menu/menu.c
 *
 * fd2_game_main_loop is a per-frame keyboard-scancode dispatcher whose very
 * first action is fd2_wait_for_input_with_idle() (real implementation in
 * src/input/input.c, blocking hardware keyboard read). Every branch is gated
 * on that scancode, and the function is otherwise dominated by display/blit
 * and menu side-effects. There is no injection seam for the scancode without
 * distorting the emitted code, so end-to-end behavioral coverage of the
 * dispatch branches (including the int-return EAX-tracking correction on the
 * field-command path) is deferred to Phase 9 integration testing under the
 * emulator, where the real input path can drive it.
 *
 * The link-smoke test below confirms the new translation unit compiles, links
 * against its dispatch-target stubs, and that the function symbol is callable
 * with the int return type recovered from the disassembly.
 *
 * fd2_field_command_menu_loop IS covered here for the dispatch branches that do
 * not pull in heavy real graphics callees: the cancel early-out (input -1 ->
 * return 1), the cursor-0 Save/Load path (returns the dispatch result verbatim
 * — the EAX-passthrough return), and the cursor-2 Options path (return 0). The
 * cursor-1 End-Turn and cursor-3 Suspend branches drive the real
 * fd2_display_dialog_scene / turn-cycle graphics path; their behavioral
 * coverage is deferred to Phase 9 integration under the emulator.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern int g_player_action_menu_loop_return;

/* fd2_field_command_menu_loop dispatch seams (defined in testglob.c) */
extern int g_settings_input_step_return;
extern int g_settings_cursor_idx;
extern int g_open_settings_dialog_calls;
extern int g_close_settings_dialog_calls;
extern int g_settings_input_step_calls;
extern int g_save_load_quit_dispatch_return;
extern int g_save_load_quit_dispatch_calls;
extern int g_settings_select_once;

/* Compile/link smoke: take the address of the emitted function and verify the
 * int-returning prototype is honored. Does not invoke it (real blocking input
 * read is unavailable in the unit harness). */
static void test_game_main_loop_symbol_linkable(void)
{
    int (*fp)(void);

    fp = fd2_game_main_loop;
    ASSERT_TRUE(fp != 0);
    /* Stub default keeps the would-be player-action loop one-shot if driven. */
    ASSERT_EQ(g_player_action_menu_loop_return, 1);
}

/* Reset the dispatch seams to a known baseline before each branch test. */
static void fcm_reset(int cursor, int input_return)
{
    g_settings_cursor_idx = cursor;
    g_settings_input_step_return = input_return;
    g_open_settings_dialog_calls = 0;
    g_close_settings_dialog_calls = 0;
    g_settings_input_step_calls = 0;
    g_save_load_quit_dispatch_calls = 0;
    g_settings_select_once = 0;
    data_fd2_ui_menu_cursor_idx = 0;
}

/* Cancel: settings input returns -1 -> close, composite, return 1 with no
 * dispatch at all. Verifies the open/close lifecycle and the -1 early-out. */
static void test_field_command_menu_cancel(void)
{
    int r;

    fcm_reset(0, -1);
    r = fd2_field_command_menu_loop();
    ASSERT_EQ(r, 1);
    ASSERT_EQ(g_open_settings_dialog_calls, 1);
    ASSERT_EQ(g_close_settings_dialog_calls, 1);
    ASSERT_EQ(g_save_load_quit_dispatch_calls, 0);
}

/* cursor == 0 (Save/Load/New Game): the function returns the dispatch result
 * verbatim. This is the EAX-passthrough return path (TAIL of the cursor-0
 * branch). Confirm the dispatch is called once and its result is propagated. */
static void test_field_command_menu_save_load_passthrough(void)
{
    int r;

    fcm_reset(0, 1);                 /* input non-zero (chose), cursor 0 */
    g_save_load_quit_dispatch_return = 42;
    r = fd2_field_command_menu_loop();
    ASSERT_EQ(r, 42);
    ASSERT_EQ(g_save_load_quit_dispatch_calls, 1);
    ASSERT_EQ(g_close_settings_dialog_calls, 1);
}

/* cursor == 2 (Options): runs the real fd2_game_options_menu_loop submenu then
 * returns 0. Drive the input-step seam in select-once mode: the field-command
 * loop's first input-step returns 1 (selecting cursor 2 -> Options), then the
 * nested options loop's next input-step returns -1 (cancel) so it exits. */
static void test_field_command_menu_options(void)
{
    int r;

    fcm_reset(2, 1);                 /* cursor 2 */
    g_settings_select_once = 1;      /* select once, then cancel the submenu */
    r = fd2_field_command_menu_loop();
    ASSERT_EQ(r, 0);
    ASSERT_EQ(g_save_load_quit_dispatch_calls, 0);
    /* one open/close for the field-command dialog, one for the options dialog */
    ASSERT_EQ(g_open_settings_dialog_calls, 2);
    ASSERT_EQ(g_close_settings_dialog_calls, 2);
}

void run_ui_menu_menu_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/menu\n");
    RUN_TEST(test_game_main_loop_symbol_linkable);
    RUN_TEST(test_field_command_menu_cancel);
    RUN_TEST(test_field_command_menu_save_load_passthrough);
    RUN_TEST(test_field_command_menu_options);
    printf("\n");
}
