/*
 * unit tests for src/field/chtrans.c
 *
 * fd2_cutscene_event_trigger is a per-chapter walk-animation script
 * interpreter. The display side (composite/blit/paint) is driven through the
 * real fd2_composite_battle_frame pipeline; to stay host-safe and
 * deterministic these tests:
 *   - empty the party (member_count = 0) so the real per-char overlay paint
 *     iterates zero chars (no wild reads),
 *   - back the workspace with a real allocated buffer so the compositor's real
 *     fd2_blit_rectangle reads a valid source (VGA-side write is harmless),
 *   - set anim_phase = 1 + a sprite atlas so the single cursor-overlay blit has
 *     a valid source, and throttle the palette cycle to its no-op path,
 *   - keep cutscene_event_state = 0 so the palette-fade-in tween branch is
 *     skipped (no VGA palette writes).
 * The script is fed through the real fd2_get_cutscene_event_script by
 * installing an in-memory byte-script into the event-script pointer table.
 *
 * Asserted behavior is the deterministic interpreter core: pair parsing,
 * per-direction position commit (0=+y,1=-x,2=-y,3=+x), facing writes, and
 * walk-phase reset. The pixel output of the composite/blit/paint stages is
 * pure display and is deferred to Phase 9 integration.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];
extern void *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];

/* workspace span the real compositor blit reads: (h-1)*sstride + w. */
#define CT_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ct_ws_buffer[CT_WS_SPAN];

/* sprite atlas so the phase-1 cursor-overlay blit has a valid source. */
static uint8 g_ct_sprite_atlas[6 + 64 * 4 + 4];

static void ct_install_safe_render_env(void)
{
    int i;
    uint32 *table;

    /* empty party: real char-overlay + shadow loops iterate zero times. */
    data_fd2_battle_party_member_count = 0;

    /* real compositor workspace backing. */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ct_ws_buffer - 0x8088;

    /* full window + origin so cursor-overlay coords are in-window. */
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;

    /* single-blit cursor phase + valid atlas. */
    data_fd2_battle_anim_phase = 1;
    table = (uint32 *)(g_ct_sprite_atlas + 6);
    for (i = 0; i < 64; i++) {
        table[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ct_sprite_atlas;

    /* palette cycle to its early-return no-op path (no VGA port writes). */
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    /* skip the palette-fade-in tween branch entirely. */
    data_fd2_chapter_cutscene_event_state = 0;
}

static void ct_clear_chars(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
}

static void ct_run_script(uint8 *script)
{
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0] = script;
    fd2_cutscene_event_trigger(0);
}

/* ----------------------------------------------------------------
 * Normal walk mode (bit7 == 0): one walk repeat, four chars each moving in a
 * distinct direction. Verifies all four position-commit branches and that the
 * walk-phase (sprite_state[2]) is reset to 0 after the 6-frame animation.
 *   dir 0 => +y, dir 1 => -x, dir 2 => -y, dir 3 => +x
 * ---------------------------------------------------------------- */
static void test_normal_walk_commits_positions(void)
{
    /* n_groups=1; group: walk_count=1, step_count=4; pairs (char,dir). */
    static uint8 script[] = {
        1,
        1, 4,
        0, 0,   /* char 0 dir 0 (south, +y) */
        1, 1,   /* char 1 dir 1 (west,  -x) */
        2, 2,   /* char 2 dir 2 (north, -y) */
        3, 3    /* char 3 dir 3 (east,  +x) */
    };

    ct_install_safe_render_env();
    ct_clear_chars();
    g_test_rc_array[0].pos_x = 10; g_test_rc_array[0].pos_y = 10;
    g_test_rc_array[1].pos_x = 10; g_test_rc_array[1].pos_y = 10;
    g_test_rc_array[2].pos_x = 10; g_test_rc_array[2].pos_y = 10;
    g_test_rc_array[3].pos_x = 10; g_test_rc_array[3].pos_y = 10;

    ct_run_script(script);

    /* char 0: dir 0 => y+1, x unchanged */
    ASSERT_EQ(g_test_rc_array[0].pos_x, 10);
    ASSERT_EQ(g_test_rc_array[0].pos_y, 11);
    /* char 1: dir 1 => x-1, y unchanged */
    ASSERT_EQ(g_test_rc_array[1].pos_x, 9);
    ASSERT_EQ(g_test_rc_array[1].pos_y, 10);
    /* char 2: dir 2 => y-1, x unchanged */
    ASSERT_EQ(g_test_rc_array[2].pos_x, 10);
    ASSERT_EQ(g_test_rc_array[2].pos_y, 9);
    /* char 3: dir 3 => x+1, y unchanged */
    ASSERT_EQ(g_test_rc_array[3].pos_x, 11);
    ASSERT_EQ(g_test_rc_array[3].pos_y, 10);

    /* facing committed to last dir; walk-phase reset. */
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 0);
    ASSERT_EQ(g_test_rc_array[1].sprite_state[1], 1);
    ASSERT_EQ(g_test_rc_array[2].sprite_state[1], 2);
    ASSERT_EQ(g_test_rc_array[3].sprite_state[1], 3);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(g_test_rc_array[3].sprite_state[2], 0);
}

/* ----------------------------------------------------------------
 * Normal walk with walk_count=2 advances the same char twice in the same
 * direction (-x), confirming the outer walk-repeat loop and cumulative commit.
 * ---------------------------------------------------------------- */
static void test_normal_walk_repeat_twice(void)
{
    static uint8 script[] = {
        1,
        2, 1,   /* walk_count=2, step_count=1 */
        5, 1    /* char 5 dir 1 (west, -x) */
    };

    ct_install_safe_render_env();
    ct_clear_chars();
    g_test_rc_array[5].pos_x = 20; g_test_rc_array[5].pos_y = 7;

    ct_run_script(script);

    ASSERT_EQ(g_test_rc_array[5].pos_x, 18);   /* -1 twice */
    ASSERT_EQ(g_test_rc_array[5].pos_y, 7);
    ASSERT_EQ(g_test_rc_array[5].sprite_state[1], 1);
    ASSERT_EQ(g_test_rc_array[5].sprite_state[2], 0);
}

/* ----------------------------------------------------------------
 * Camera-only / facing-only mode (bit7 == 1, low7 != 0): sets sprite_state[1]
 * for listed chars and composites low7 times, but never changes position.
 * ---------------------------------------------------------------- */
static void test_camera_only_sets_facing_no_move(void)
{
    /* walk_count_or_flags = 0x82 => bit7 set, low7 = 2. step_count=2. */
    static uint8 script[] = {
        1,
        0x82, 2,
        2, 3,   /* char 2 dir 3 */
        4, 2    /* char 4 dir 2 */
    };

    ct_install_safe_render_env();
    ct_clear_chars();
    g_test_rc_array[2].pos_x = 6; g_test_rc_array[2].pos_y = 6;
    g_test_rc_array[4].pos_x = 8; g_test_rc_array[4].pos_y = 9;

    ct_run_script(script);

    /* facing updated, positions unchanged. */
    ASSERT_EQ(g_test_rc_array[2].sprite_state[1], 3);
    ASSERT_EQ(g_test_rc_array[4].sprite_state[1], 2);
    ASSERT_EQ(g_test_rc_array[2].pos_x, 6);
    ASSERT_EQ(g_test_rc_array[2].pos_y, 6);
    ASSERT_EQ(g_test_rc_array[4].pos_x, 8);
    ASSERT_EQ(g_test_rc_array[4].pos_y, 9);
}

/* ----------------------------------------------------------------
 * Multi-group sequence: a camera-only facing group followed by a normal-walk
 * group. Confirms the group loop advances the script pointer correctly across
 * the variable-length pair list of group 1 to reach group 2.
 * ---------------------------------------------------------------- */
static void test_multi_group_sequence(void)
{
    static uint8 script[] = {
        2,
        /* group 0: camera-only, low7=1, step_count=1: face char 1 east (3) */
        0x81, 1,
        1, 3,
        /* group 1: normal walk_count=1, step_count=1: char 1 walks east (+x) */
        1, 1,
        1, 3
    };

    ct_install_safe_render_env();
    ct_clear_chars();
    g_test_rc_array[1].pos_x = 4; g_test_rc_array[1].pos_y = 4;

    ct_run_script(script);

    /* group 0 set facing to 3; group 1 then moved +x once and reset phase. */
    ASSERT_EQ(g_test_rc_array[1].sprite_state[1], 3);
    ASSERT_EQ(g_test_rc_array[1].pos_x, 5);
    ASSERT_EQ(g_test_rc_array[1].pos_y, 4);
    ASSERT_EQ(g_test_rc_array[1].sprite_state[2], 0);
}

void run_field_chtrans_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chtrans\n");
    RUN_TEST(test_normal_walk_commits_positions);
    RUN_TEST(test_normal_walk_repeat_twice);
    RUN_TEST(test_camera_only_sets_facing_no_move);
    RUN_TEST(test_multi_group_sequence);
    printf("\n");
}
