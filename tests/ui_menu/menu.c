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
#include "menufix.h"

/* fd2_field_command_menu_loop dispatch seams (defined in testglob.c). The
 * settings input-step is now the real emitted function driven by staging real
 * scancodes into the BIOS keyboard ring (menufix.h); only the save/load/quit
 * dispatch is still stubbed. */
extern int g_save_load_quit_dispatch_return;
extern int g_save_load_quit_dispatch_calls;

/* idle-loop buffer-flip seam (testglob.c repaint stub): exposes a pre-staged
 * Esc to the options submenu's idle wait after the field-command loop's close
 * cleared the buffer. */
extern int g_repaint_flip_buffer_after;
extern int g_repaint_settings_calls;

/* real-render seam: the now-real fd2_open_settings_dialog_with_slide blits 16
 * corner sprites per open (4 frames x 4 corners). */
extern int g_blitsetup_calls;

/* pathfind stub seam (testglob.c): the md==0 call path returns this value, used
 * to drive fd2_player_action_menu_loop's unreachable-destination branch. */
extern int g_pathfind_walk_return;

/* Host-safe render environment for the real open-dialog reached on every
 * field-command iteration: empty party (no real char paint), a real workspace
 * the final blit reads from, and a real dialog-state handle for the sprite
 * offset-table lookup. */
#define MNU_WS_SPAN (191u * 0x1C8u + 0x138u + 0x8088u)
static uint8 mnu_ws_buffer[MNU_WS_SPAN];
static int32 mnu_dialog_handle[512];

static void mnu_setup_render_env(void)
{
    int i;
    for (i = 0; i < 512; i++) {
        mnu_dialog_handle[i] = 0;
    }
    data_fd2_battle_party_member_count = 0;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;
    data_fd2_large_game_state_buffer_ptr = (uint32)mnu_ws_buffer;
    data_fd2_menu_dialog_state_handle = (uint32)mnu_dialog_handle;
    data_fd2_audio_fdother_sfx_bank_buf_ptr = 0;
    g_blitsetup_calls = 0;
}

/* Compile/link smoke: take the address of the emitted function and verify the
 * int-returning prototype is honored. Does not invoke it (real blocking input
 * read is unavailable in the unit harness). */
static void test_game_main_loop_symbol_linkable(void)
{
    int (*fp)(void);
    int (*pal)(uint32);

    fp = fd2_game_main_loop;
    ASSERT_TRUE(fp != 0);
    /* Its player-action callee is now the real emitted function. */
    pal = fd2_player_action_menu_loop;
    ASSERT_TRUE(pal != 0);
}

/* Reset render env + dispatch counters to a known baseline before each branch
 * test. The field-command loop's field-command menu_state template is all-zero
 * (every direction slot enabled), so an arrow always lands on its slot. */
static void fcm_reset(void)
{
    mnu_setup_render_env();
    g_save_load_quit_dispatch_calls = 0;
    g_repaint_settings_calls = 0;
    g_repaint_flip_buffer_after = 0;
    data_fd2_ui_menu_cursor_idx = 0;
}

/* Cancel: a single Esc -> input-step returns -1 -> close, composite, return 1
 * with no dispatch at all. Verifies the open/close lifecycle and the -1
 * early-out. (Field-command loop is single-pass; Esc on the first input-step
 * exits the do/while immediately.) */
static void test_field_command_menu_cancel(void)
{
    int r;

    fcm_reset();
    mfix_load_cancel();
    r = fd2_field_command_menu_loop();
    ASSERT_EQ(r, 1);
    /* one real open-dialog + one real close-dialog = 16 + 16 = 32 corner blits */
    ASSERT_EQ(g_blitsetup_calls, 32);
    ASSERT_EQ(g_save_load_quit_dispatch_calls, 0);
}

/* cursor == 0 (Save/Load/New Game): the function returns the dispatch result
 * verbatim. This is the EAX-passthrough return path (TAIL of the cursor-0
 * branch). Navigate Up (-> cursor 0) then Space (commit); confirm the dispatch
 * is called once and its result is propagated. */
static void test_field_command_menu_save_load_passthrough(void)
{
    int r;

    fcm_reset();
    mfix_load_select(0);            /* Up -> cursor 0, then Space commits */
    g_save_load_quit_dispatch_return = 42;
    r = fd2_field_command_menu_loop();
    ASSERT_EQ(r, 42);
    ASSERT_EQ(g_save_load_quit_dispatch_calls, 1);
    /* one real open + one real close before the cursor-0 dispatch = 32 blits */
    ASSERT_EQ(g_blitsetup_calls, 32);
}

/* cursor == 2 (Options): navigate Right (-> cursor 2) then Space (commit) in the
 * field-command loop, which then runs the real fd2_game_options_menu_loop
 * submenu. The field loop's close clears the keyboard buffer (tail:=head at the
 * ring slot holding the pre-staged Esc); the options submenu then idles and the
 * armed buffer-flip exposes that Esc so it cancels and returns. Field returns 0.
 *
 * Ring staged as [Right, Space, Esc]: field loop consumes Right+Space (no idle),
 * options submenu's first idle flip delivers Esc. */
static void test_field_command_menu_options(void)
{
    int r;

    fcm_reset();
    g_repaint_flip_buffer_after = 1;
    /* [Right, Space] present; Esc waits at the post-clear head for the submenu. */
    {
        uint8 keys[3];
        keys[0] = MFIX_SC_RIGHT;   /* -> cursor 2 in field loop */
        keys[1] = MFIX_SC_SPACE;   /* commit field selection */
        keys[2] = MFIX_SC_ESC;     /* cancels the options submenu after idle flip */
        *(volatile uint16 *)0x41AuL = 0x1E;
        *(volatile uint16 *)0x41EuL = (uint16)((uint16)keys[0] << 8);
        *(volatile uint16 *)0x420uL = (uint16)((uint16)keys[1] << 8);
        *(volatile uint16 *)0x422uL = (uint16)((uint16)keys[2] << 8);
        *(volatile uint16 *)0x41CuL = 0x22; /* tail: Right+Space present */
    }
    r = fd2_field_command_menu_loop();
    ASSERT_EQ(r, 0);
    ASSERT_EQ(g_save_load_quit_dispatch_calls, 0);
    /* one open/close for the field-command dialog, one open/close for the
     * options dialog; each real open and each real close = 16 corner blits
     * -> 4 * 16 = 64. Plus the options submenu idles exactly once (the armed
     * buffer-flip exposes the staged Esc), and that single idle-loop body runs
     * the real fd2_repaint_settings_dialog_borders = 4 more corner blits. 68. */
    ASSERT_EQ(g_blitsetup_calls, 68);
}

/* ----------------------------------------------------------------
 * fd2_player_action_menu_loop coverage
 *
 * This is a heavy UI orchestrator: most of its body runs the real
 * movement-range paint, status-panel repaint, target-input loop, walk
 * animation, and (after a destination is chosen) the inline action
 * submenu — all display / blocking-input side effects with no clean
 * isolation seam for the deep paths (full behavioral coverage of the
 * action-commit and result-code tail is deferred to Phase 9).
 *
 * Two deterministic, host-safe early-exit paths ARE driven here:
 *   1. Cancel at target-input (Esc): exercises the setup block (template
 *      copy, result_code := 0, consequence_idx := 0xFF) and the
 *      cancel early-out (pan back, return 1).
 *   2. Destination chosen but pathfind reports it unreachable (0xFF):
 *      exercises the commit, the EAX-tracking-sensitive pathfind-return
 *      comparison (full-register == 0xFF), the anim_phase 0->1 toggle,
 *      and the "return 1" exit — without entering the inline submenu.
 *
 * Both are made tractable by: an empty party (party_member_count == 0)
 * so the overlay/occupant/target-input party loops are inert; a staged
 * BIOS-ring scancode (the real fd2_wait_for_input_v2 reads it via INT
 * 16h); the cursor left on the actor tile so the animated pan is a no-op
 * composite; and the floodfill/obfuscate/pathfind stubs in testglob.c.
 * ---------------------------------------------------------------- */

#define PAML_NCHARS 4
static runtime_char paml_chars[PAML_NCHARS];
static uint8 paml_tile_map[64 * 4];

/* Saved shared-global snapshot so each test restores cross-suite state on
 * exit (the runtime-char-array pointer in particular defaults to
 * g_test_rc_array, which later suites rely on). */
static runtime_char *paml_saved_char_ptr;
static uint32 paml_saved_party_count;
static uint32 paml_saved_cursor_x;
static uint32 paml_saved_cursor_y;
static uint32 paml_saved_anim_phase;
static uint32 paml_saved_result_code;
static uint32 paml_saved_consequence_idx;
static uint32 paml_saved_tile_map_ptr;
static uint32 paml_saved_map_width;

/* Build a single-actor runtime context at index 0 and point the global
 * char-array pointer at it. range = combat_aux_block[0x14] feeds the
 * malloc size; keep it small. Snapshots every shared global it mutates so
 * paml_teardown() can restore them. Caller stages input. */
static void paml_setup(uint8 job_id, uint8 portrait_id, uint8 archetype,
                       uint8 range_remaining)
{
    int i;

    paml_saved_char_ptr = data_fd2_battle_runtime_char_array_ptr;
    paml_saved_party_count = data_fd2_battle_party_member_count;
    paml_saved_cursor_x = data_fd2_battle_cursor_world_x;
    paml_saved_cursor_y = data_fd2_battle_cursor_world_y;
    paml_saved_anim_phase = data_fd2_battle_anim_phase;
    paml_saved_result_code = data_fd2_battle_player_action_result_code;
    paml_saved_consequence_idx = data_fd2_battle_ai_post_action_consequence_idx;
    paml_saved_tile_map_ptr = data_fd2_battle_tile_map_ptr;
    paml_saved_map_width = data_fd2_battle_map_width_tiles;

    mnu_setup_render_env();             /* empty party + workspace + dialog */
    for (i = 0; i < (int)sizeof(paml_chars); i++) {
        ((uint8 *)paml_chars)[i] = 0;
    }
    paml_chars[0].pos_x = 3;
    paml_chars[0].pos_y = 2;
    paml_chars[0].team = 2;             /* player */
    paml_chars[0].job_id = job_id;
    paml_chars[0].portrait_id = portrait_id;
    paml_chars[0].archetype_flag = archetype;
    paml_chars[0].combat_aux_block[0x14] = range_remaining;

    data_fd2_battle_runtime_char_array_ptr = paml_chars;
    data_fd2_battle_party_member_count = 0;   /* party loops inert */
    data_fd2_battle_cursor_world_x = paml_chars[0].pos_x;
    data_fd2_battle_cursor_world_y = paml_chars[0].pos_y;
    data_fd2_battle_anim_phase = 1;

    /* Sentinels to prove the setup block overwrites them. */
    data_fd2_battle_player_action_result_code = 0x1234;
    data_fd2_battle_ai_post_action_consequence_idx = 0x1234;
}

/* Restore every shared global paml_setup() captured, so a following suite
 * sees the pre-test environment (no cross-suite pollution). */
static void paml_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = paml_saved_char_ptr;
    data_fd2_battle_party_member_count = paml_saved_party_count;
    data_fd2_battle_cursor_world_x = paml_saved_cursor_x;
    data_fd2_battle_cursor_world_y = paml_saved_cursor_y;
    data_fd2_battle_anim_phase = paml_saved_anim_phase;
    data_fd2_battle_player_action_result_code = paml_saved_result_code;
    data_fd2_battle_ai_post_action_consequence_idx = paml_saved_consequence_idx;
    data_fd2_battle_tile_map_ptr = paml_saved_tile_map_ptr;
    data_fd2_battle_map_width_tiles = paml_saved_map_width;
}

/* Path 1: Esc at the target-input -> cancel early-out returns 1. Also
 * asserts the setup block reset result_code (->0) and consequence_idx
 * (->0xFF). job_id 5 / portrait 0 / archetype 0 -> no class override
 * (status-immunity false, portrait != 0x1C). */
static void test_player_action_menu_cancel(void)
{
    int r;

    paml_setup(5, 0, 0, 5);
    mfix_load_cancel();                  /* single Esc in the BIOS ring */
    r = fd2_player_action_menu_loop(0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ((int)data_fd2_battle_player_action_result_code, 0);
    ASSERT_EQ((int)data_fd2_battle_ai_post_action_consequence_idx, 0xff);
    paml_teardown();
}

/* Path 2: commit the destination (Space on a passable tile), then the
 * pathfind stub reports the tile unreachable (0xFF) -> the function
 * returns 1 without entering the inline submenu. Verifies the
 * full-register 0xFF comparison and that anim_phase ends restored to 1
 * (the 0 -> pan -> 1 toggle around the pan ran to completion). */
static void test_player_action_menu_unreachable(void)
{
    int r;
    int saved_walk_return;

    paml_setup(5, 0, 0, 5);

    /* Passable tile at the cursor: tile_map[(y*width + x)*4 + 7] != 0xFF.
     * cursor (3,2), width 20 -> need a buffer covering that index; clear
     * a local map and point the global at it with a small width. */
    {
        int i;
        for (i = 0; i < (int)sizeof(paml_tile_map); i++) {
            paml_tile_map[i] = 0;        /* all passable (+7 byte == 0) */
        }
    }
    data_fd2_battle_tile_map_ptr = (uint32)paml_tile_map;
    data_fd2_battle_map_width_tiles = 4;
    paml_chars[0].pos_x = 0;
    paml_chars[0].pos_y = 0;
    data_fd2_battle_cursor_world_x = 0;
    data_fd2_battle_cursor_world_y = 0;

    saved_walk_return = g_pathfind_walk_return;
    g_pathfind_walk_return = 0xff;       /* md==0 path returns this */

    mfix_load_keys((const uint8 *)"\x39", 1);  /* Space -> commit (mode 4) */
    r = fd2_player_action_menu_loop(0);

    g_pathfind_walk_return = saved_walk_return;

    ASSERT_EQ(r, 1);
    ASSERT_EQ((int)data_fd2_battle_anim_phase, 1);
    paml_teardown();
}

void run_ui_menu_menu_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/menu\n");
    RUN_TEST(test_game_main_loop_symbol_linkable);
    RUN_TEST(test_field_command_menu_cancel);
    RUN_TEST(test_field_command_menu_save_load_passthrough);
    RUN_TEST(test_field_command_menu_options);
    RUN_TEST(test_player_action_menu_cancel);
    RUN_TEST(test_player_action_menu_unreachable);
    printf("\n");
}
