/*
 * unit tests for src/ui_menu/menucfg.c
 *
 * fd2_game_options_menu_loop is an infinite settings loop that exits only when
 * fd2_settings_menu_input_step returns -1 (cancel). The real input-step path is
 * driven through the real fd2_wait_input_with_dialog_repaint by staging real
 * BIOS-keyboard scancodes (tests/include/menufix.h):
 *   - mfix_load_select(cursor) stages [arrow(cursor), Space] so the loop runs
 *     one navigation step (cursor moves, input-step returns 0) then a commit
 *     (Space, input-step returns 1) -> exactly one toggle iteration, then the
 *     dispatch toggles the slot. The loop's next iteration would read more keys;
 *     to terminate, the dispatch-then-cancel tests stage a trailing Esc.
 *   - mfix_load_cancel() stages [Esc] for the plain cancel early-out (no toggle).
 *
 * Covered branches: cancel early-out, BGM toggle (cursor 0, including the
 * AIL_set_sequence_volume fade value / ramp), SFX toggle (else / cursor 1),
 * game-speed toggle (cursor 2), terrain-HUD toggle (cursor 3). The label
 * rebuild at the top of each iteration is purely text-token state feeding the
 * (real) dialog renderer, so it has no observable seam here.
 */

#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "menufix.h"

/* AIL_set_sequence_volume tracking (defined in testglob.c) */
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;

/* real-render seams hit by the now-real fd2_open_settings_dialog_with_slide */
extern uint32 g_blitsetup_dst, g_blitsetup_sprite, g_blitsetup_stride;
extern int    g_blitsetup_calls;

/* idle-loop buffer-flip seam (testglob.c repaint stub): when set, the second
 * menu-loop iteration's idle wait gets a key delivered so the staged Esc is
 * read and the loop exits. */
extern int g_repaint_flip_buffer_after;
extern int g_repaint_settings_calls;

/* Workspace span the real final fd2_blit_rectangle(ptr+0x8088, ..., h=0xC0)
 * reads, plus the 0x8088 header: (0xC0-1)*0x1C8 + 0x138 + 0x8088. */
#define CFG_WS_SPAN (191u * 0x1C8u + 0x138u + 0x8088u)
static uint8 cfg_ws_buffer[CFG_WS_SPAN];

/* Fake dialog-state handle: an offset table (dword per atlas entry) followed
 * by sprite-pixel space. fd2_open_settings_dialog_with_slide reads
 * handle + *(int32*)(handle + sprite_id*4). 256 entries cover the loop's max
 * sprite_id (menu_options up to 0x19, *3 = 0x4B). */
#define CFG_HANDLE_ENTRIES 256
static int32 cfg_dialog_handle[CFG_HANDLE_ENTRIES + 256];

/* Stand up a host-safe render environment so the real open-dialog runs:
 * empty party (find_char -> -1, overlay paints nothing), real workspace, a
 * dialog handle whose offset table is all-zero (sprite_addr == handle). */
static void cfg_setup_render_env(void)
{
    int i;
    for (i = 0; i < CFG_HANDLE_ENTRIES + 256; i++) {
        cfg_dialog_handle[i] = 0;
    }
    data_fd2_battle_party_member_count = 0;
    /* cursor (1,1): the now-real fd2_backup_dialog_area_to_buffer anchors its
     * snapshot at (cx-1)*0x18 + (cy-1)*0x2AC0 + 0x8088, so cursor 0 would
     * underflow. (1,1) -> anchor == 0x8088, region stays inside cfg_ws_buffer. */
    data_fd2_battle_cursor_screen_x = 1;
    data_fd2_battle_cursor_screen_y = 1;
    data_fd2_large_game_state_buffer_ptr = (uint32)cfg_ws_buffer;
    data_fd2_menu_dialog_state_handle = (uint32)cfg_dialog_handle;
    data_fd2_audio_fdother_sfx_bank_buf_ptr = 0;
    if (data_fd2_dialog_area_backup_buffer != (void *)0) {
        free(data_fd2_dialog_area_backup_buffer);
        data_fd2_dialog_area_backup_buffer = (void *)0;
    }
    g_blitsetup_calls = 0;
}

/* Drive exactly one selection of `cursor`, then cancel: stage [arrow, Space,
 * Esc] in the BIOS keyboard ring and arm the idle buffer-flip so iteration 2's
 * idle delivers the staged Esc (see mfix_load_select_then_cancel). */
static void cfg_reset_select(int cursor)
{
    cfg_setup_render_env();
    g_ail_vol_calls = 0;
    g_ail_last_vol = 0;
    g_ail_last_ramp = 0;
    data_fd2_ui_menu_cursor_idx = 0;
    g_repaint_settings_calls = 0;
    g_repaint_flip_buffer_after = 1;
    mfix_load_select_then_cancel(cursor);
}

/* Cancel immediately: no toggles, dialog opened (real render) and closed once. */
static void test_options_cancel(void)
{
    uint8 bgm0, sfx0, spd0, hud0;

    cfg_setup_render_env();
    g_repaint_flip_buffer_after = 0;     /* Esc present immediately; no idle */
    mfix_load_cancel();
    g_ail_vol_calls = 0;

    bgm0 = data_fd2_audio_bgm_enabled_flag;
    sfx0 = data_fd2_audio_sfx_enabled_flag;
    spd0 = data_fd2_ui_game_speed_flag;
    hud0 = data_fd2_ui_terrain_hud_user_enabled;

    fd2_game_options_menu_loop();

    /* one real open-dialog + one real close-dialog, each 4 frames x 4 corners
       = 16 corner blits, so 32 total for the single cancel iteration. */
    ASSERT_EQ(g_blitsetup_calls, 32);
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

/* fd2_open_settings_dialog_with_slide: drive the real render once and verify
 * the deterministic core — the per-corner sprite atlas index
 * (menu_options[c]*3 + menu_state[c]*2), the handle offset-table lookup
 * (sprite_addr = handle + table[index]), and the final corner offset after
 * 4 frames of outward spread. The capture stub records the LAST blit, which is
 * corner 3 on frame 3:
 *   corner_offsets[3] = 0x390 + 4*0x8E8 = 0x2730
 *   dst              = (ptr + 0x8088) + 0x2730
 *   sprite_id        = menu_options[3]*3 + menu_state[3]*2
 *   sprite_addr      = handle + table[sprite_id]
 * Other observable invariants: 16 corner blits, one backup, four restores,
 * stride always 0x1C8. */
static void test_open_dialog_last_blit(void)
{
    int32 menu_options[4];
    int32 menu_state[4];
    int sprite_id;
    uint32 expect_dst;
    uint32 expect_sprite;

    cfg_setup_render_env();

    menu_options[0] = 1; menu_options[1] = 2;
    menu_options[2] = 3; menu_options[3] = 4;
    menu_state[0] = 0;   menu_state[1] = 0;
    menu_state[2] = 0;   menu_state[3] = 5;

    /* corner 3: sprite_id = 4*3 + 5*2 = 22; seed table[22] with a known offset */
    sprite_id = menu_options[3] * 3 + menu_state[3] * 2;
    ASSERT_EQ(sprite_id, 22);
    cfg_dialog_handle[sprite_id] = 0x100;

    fd2_open_settings_dialog_with_slide(menu_options, menu_state);

    /* panel_anchor = base + 0x8088 + cx*0x18 + cy*0x2AC0; cursor (1,1) here. */
    expect_dst = (uint32)cfg_ws_buffer + 0x8088u + 0x18u + 0x2AC0u + 0x2730u;
    expect_sprite = (uint32)cfg_dialog_handle + 0x100u;

    ASSERT_EQ(g_blitsetup_calls, 16);
    /* the real fd2_backup_dialog_area_to_buffer ran once -> backup buffer set,
       and the now-real fd2_restore_dialog_area_from_buffer ran (4x during the
       slide) reading from that same buffer without faulting */
    ASSERT_TRUE(data_fd2_dialog_area_backup_buffer != NULL);
    ASSERT_EQ((long)g_blitsetup_dst, (long)expect_dst);
    ASSERT_EQ((long)g_blitsetup_sprite, (long)expect_sprite);
    ASSERT_EQ(g_blitsetup_stride, 0x1C8u);
}

/* fd2_close_settings_dialog_with_slide: drive the real close render once and
 * verify the deterministic core — same per-corner sprite atlas index and
 * handle offset-table lookup as open, but the 4 corner offsets start at their
 * outer extents and converge INWARD each frame (opposite sense to open). The
 * capture stub records the LAST blit = corner 3 on frame 3:
 *   corner_offsets[3] = 0x2AC0 - 4*0x8E8 = 0x720
 *   dst              = (ptr + 0x8088) + cx*0x18 + cy*0x2AC0 + 0x720
 *   sprite_id        = menu_options[3]*3 + menu_state[3]*2
 *   sprite_addr      = handle + table[sprite_id]
 * Invariants: 16 corner blits, stride always 0x1C8. The close also restores
 * the dialog area from backup (5x: once per frame + one final) and clears the
 * keyboard buffer, both real callees, exercised here without faulting. Since
 * the real fd2_restore_dialog_area_from_buffer copies FROM the backup buffer
 * unconditionally, we first run the real fd2_backup_dialog_area_to_buffer to
 * stand up a valid "save under" snapshot, exactly as the live open->close flow
 * would. */
static void test_close_dialog_last_blit(void)
{
    int32 menu_options[4];
    int32 menu_state[4];
    int sprite_id;
    uint32 expect_dst;
    uint32 expect_sprite;

    cfg_setup_render_env();
    fd2_backup_dialog_area_to_buffer();

    menu_options[0] = 1; menu_options[1] = 2;
    menu_options[2] = 3; menu_options[3] = 4;
    menu_state[0] = 0;   menu_state[1] = 0;
    menu_state[2] = 0;   menu_state[3] = 5;

    /* corner 3: sprite_id = 4*3 + 5*2 = 22; seed table[22] with a known offset */
    sprite_id = menu_options[3] * 3 + menu_state[3] * 2;
    ASSERT_EQ(sprite_id, 22);
    cfg_dialog_handle[sprite_id] = 0x100;

    fd2_close_settings_dialog_with_slide(menu_options, menu_state);

    /* panel_anchor = base + 0x8088 + cx*0x18 + cy*0x2AC0; cursor (1,1) here.
       corner_offsets[3] converges inward: 0x2AC0 - 4*0x8E8 = 0x720. */
    expect_dst = (uint32)cfg_ws_buffer + 0x8088u + 0x18u + 0x2AC0u + 0x720u;
    expect_sprite = (uint32)cfg_dialog_handle + 0x100u;

    ASSERT_EQ(g_blitsetup_calls, 16);
    ASSERT_EQ((long)g_blitsetup_dst, (long)expect_dst);
    ASSERT_EQ((long)g_blitsetup_sprite, (long)expect_sprite);
    ASSERT_EQ(g_blitsetup_stride, 0x1C8u);
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

/* ----------------------------------------------------------------
 * fd2_settings_menu_input_step @ 0x177FC direct tests.
 *
 * One input-step reads a single scancode through the real
 * fd2_wait_input_with_dialog_repaint. Stage one scancode in the BIOS keyboard
 * ring (buffer nonempty so no idle/render is hit) and verify the return value
 * and cursor side effect. all_enabled is the int[4] slot-disable array with
 * every slot enabled (0). ---------------------------------------------------- */

/* All slots enabled; cursor pre-poisoned so a write is observable. */
static void cfg_input_setup(int32 *menu_state, uint8 scancode)
{
    menu_state[0] = 0;
    menu_state[1] = 0;
    menu_state[2] = 0;
    menu_state[3] = 0;
    data_fd2_ui_menu_cursor_idx = 99;
    {
        uint8 k[1];
        k[0] = scancode;
        mfix_load_keys(k, 1);
    }
}

/* Esc -> cancel (-1), cursor untouched. */
static void test_input_esc_cancel(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_ESC);
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, -1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 99u);
}

/* Space -> commit (1), cursor untouched. */
static void test_input_space_commit(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_SPACE);
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 99u);
}

/* Enter (0x1C) -> commit (1) as well. */
static void test_input_enter_commit(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_ENTER);
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 1);
}

/* Up -> cursor 0, return 0 (slot enabled). */
static void test_input_up_sets_cursor0(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_UP);
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 0u);
}

/* Left -> cursor 1. */
static void test_input_left_sets_cursor1(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_LEFT);
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 1u);
}

/* Right -> cursor 2. */
static void test_input_right_sets_cursor2(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_RIGHT);
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 2u);
}

/* Down -> cursor 3. */
static void test_input_down_sets_cursor3(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_DOWN);
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 3u);
}

/* Down with slot 3 DISABLED (menu_state[3] != 0): cursor NOT changed, return 0.
 * Confirms each arrow consults its own direction-indexed disable entry
 * (Down -> [3]) and that a disabled slot is a no-op. */
static void test_input_down_disabled_noop(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_DOWN);
    st[3] = 1;                       /* Down slot disabled */
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 99u); /* unchanged */
}

/* Right with slot 2 disabled: no-op. Pins the [2] index for Right. */
static void test_input_right_disabled_noop(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, MFIX_SC_RIGHT);
    st[2] = 7;
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 99u);
}

/* Unmapped scancode (e.g. 0x10 'Q') -> return 0, cursor untouched. */
static void test_input_unmapped_noop(void)
{
    int32 st[4];
    int r;
    cfg_input_setup(st, 0x10);
    r = fd2_settings_menu_input_step(st, st);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 99u);
}

/* ----------------------------------------------------------------
 * fd2_repaint_settings_dialog_borders @ 0x179D5 direct tests.
 *
 * Drive the real per-frame border repaint once with a controlled cursor cell,
 * menu arrays, selected-corner index and blink phase; the capture stub records
 * the LAST corner blit (corner 3). Verifies the fixed 4-corner offsets, the
 * panel anchor, the per-corner sprite atlas index (menu_options[c]*3 +
 * menu_state[c]*2) and the handle offset-table lookup. find_char returns -1
 * (empty party) so no char restamp runs. ---------------------------------- */

/* Non-selected corner 3: sprite_id = 4*3 + 5*2 = 22, no blink add (cursor_idx 1).
 * corner_offsets[3] = 0x2AC0; cursor (1,1) -> anchor + 0x18 + 0x2AC0. */
static void test_repaint_borders_last_corner(void)
{
    int32 menu_options[4];
    int32 menu_state[4];
    uint32 expect_dst;
    uint32 expect_sprite;

    cfg_setup_render_env();
    menu_options[0] = 1; menu_options[1] = 2;
    menu_options[2] = 3; menu_options[3] = 4;
    menu_state[0] = 0;   menu_state[1] = 0;
    menu_state[2] = 0;   menu_state[3] = 5;
    data_fd2_ui_menu_cursor_idx = 1;          /* selected corner != 3 */
    data_fd2_dialog_blink_phase_oscillator = 1; /* must NOT affect corner 3 */
    cfg_dialog_handle[22] = 0x100;

    fd2_repaint_settings_dialog_borders((uint32)menu_options,
                                        (uint32)menu_state);

    expect_dst = (uint32)cfg_ws_buffer + 0x8088u + 0x18u + 0x2AC0u + 0x2AC0u;
    expect_sprite = (uint32)cfg_dialog_handle + 0x100u;

    ASSERT_EQ(g_blitsetup_calls, 4);
    ASSERT_EQ((long)g_blitsetup_dst, (long)expect_dst);
    ASSERT_EQ((long)g_blitsetup_sprite, (long)expect_sprite);
    ASSERT_EQ(g_blitsetup_stride, 0x1C8u);
}

/* Selected corner 3 with blink phase 1: sprite_id = 4*3 + 5*2 + 1 = 23.
 * Pins the c == data_fd2_ui_menu_cursor_idx blink add (the highlight-frame
 * alternation): the selected corner's atlas index gains the 0/1 oscillator. */
static void test_repaint_borders_blink_selected(void)
{
    int32 menu_options[4];
    int32 menu_state[4];
    uint32 expect_sprite;

    cfg_setup_render_env();
    menu_options[0] = 1; menu_options[1] = 2;
    menu_options[2] = 3; menu_options[3] = 4;
    menu_state[0] = 0;   menu_state[1] = 0;
    menu_state[2] = 0;   menu_state[3] = 5;
    data_fd2_ui_menu_cursor_idx = 3;          /* corner 3 IS selected */
    data_fd2_dialog_blink_phase_oscillator = 1;
    cfg_dialog_handle[22] = 0x100;            /* without blink */
    cfg_dialog_handle[23] = 0x200;            /* with blink (+1) */

    fd2_repaint_settings_dialog_borders((uint32)menu_options,
                                        (uint32)menu_state);

    expect_sprite = (uint32)cfg_dialog_handle + 0x200u;

    ASSERT_EQ(g_blitsetup_calls, 4);
    ASSERT_EQ((long)g_blitsetup_sprite, (long)expect_sprite);
}

/* Selected corner 3, blink phase 0: the +0 add leaves sprite_id at 22 (the same
 * as the non-selected case), confirming the oscillator value (not just presence
 * of the branch) is what shifts the atlas frame. */
static void test_repaint_borders_blink_phase0(void)
{
    int32 menu_options[4];
    int32 menu_state[4];
    uint32 expect_sprite;

    cfg_setup_render_env();
    menu_options[0] = 1; menu_options[1] = 2;
    menu_options[2] = 3; menu_options[3] = 4;
    menu_state[0] = 0;   menu_state[1] = 0;
    menu_state[2] = 0;   menu_state[3] = 5;
    data_fd2_ui_menu_cursor_idx = 3;
    data_fd2_dialog_blink_phase_oscillator = 0;
    cfg_dialog_handle[22] = 0x100;
    cfg_dialog_handle[23] = 0x200;

    fd2_repaint_settings_dialog_borders((uint32)menu_options,
                                        (uint32)menu_state);

    expect_sprite = (uint32)cfg_dialog_handle + 0x100u;

    ASSERT_EQ(g_blitsetup_calls, 4);
    ASSERT_EQ((long)g_blitsetup_sprite, (long)expect_sprite);
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
    RUN_TEST(test_open_dialog_last_blit);
    RUN_TEST(test_close_dialog_last_blit);
    RUN_TEST(test_count_first_zero);
    RUN_TEST(test_count_partial);
    RUN_TEST(test_count_all_four);
    RUN_TEST(test_count_one);
    RUN_TEST(test_input_esc_cancel);
    RUN_TEST(test_input_space_commit);
    RUN_TEST(test_input_enter_commit);
    RUN_TEST(test_input_up_sets_cursor0);
    RUN_TEST(test_input_left_sets_cursor1);
    RUN_TEST(test_input_right_sets_cursor2);
    RUN_TEST(test_input_down_sets_cursor3);
    RUN_TEST(test_input_down_disabled_noop);
    RUN_TEST(test_input_right_disabled_noop);
    RUN_TEST(test_input_unmapped_noop);
    RUN_TEST(test_repaint_borders_last_corner);
    RUN_TEST(test_repaint_borders_blink_selected);
    RUN_TEST(test_repaint_borders_blink_phase0);
    printf("\n");
}
