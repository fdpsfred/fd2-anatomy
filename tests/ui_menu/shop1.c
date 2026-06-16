/*
 * unit tests for src/ui_menu/shop.c
 *
 * fd2_shop_menu_input_loop (@0x2DF6B) is the cursor-navigation + row-paged
 * scroll loop for the buy / sell / give-item screens (2-column grid, 6-item
 * viewport). It reads one scancode per iteration through the REAL
 * fd2_wait_input_with_chapter_dialog_blink(1) (input.c), which pulls the
 * scancode from the BIOS keyboard ring via INT 16h fn 10h. These tests stage a
 * scancode sequence into that ring (menufix.h mfix_load_keys) so the real input
 * path drives the loop deterministically, then assert the resulting absolute
 * cursor (0x53C57), the viewport scroll offset (0x5412F), the return value, and
 * the SFX / grid-render / scroll-animation sequencing through the recording
 * stubs in testglob.c.
 *
 * Because the ring starts NONEMPTY (head != tail), fd2_wait_input_with_chapter_
 * dialog_blink's idle-repaint body runs zero times — only its one rng advance +
 * panel-render stub fire per key before INT 16h reads it — so the only SFX in
 * play is the cursor-move chime fired by the shop loop itself (one per accepted
 * move). mode=1 also skips the corner-sprite blit loop, so no atlas pointer is
 * dereferenced.
 *
 * Risk coverage: every scancode arm (Right/Left/Up/Down/Enter/Space/Esc), each
 * arm's boundary guard (no move at the grid edge -> no SFX/render), the signed
 * comparisons, and both viewport-paging seams reached two ways each (Right and
 * Down share the page-down block LAB_dfb8; Left and Up share the page-up block
 * LAB_e01e).
 */

#include <stdio.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "menufix.h"

/* recording seams from testglob.c (shop display callees) */
extern int    g_shop_grid_render_calls;
extern uint32 g_shop_grid_last_count;
extern uint32 g_shop_grid_last_array;
extern uint32 g_shop_grid_last_cursor;
extern uint32 g_shop_grid_last_dst;
extern uint32 g_shop_grid_last_sell;
/* Both shop-dialog scroll animations are the REAL emitted functions
 * (anim/aniui.c): fd2_animate_scroll_up_in_shop_dialog (page-DOWN) and
 * fd2_animate_scroll_down_in_shop_dialog (page-UP). Each shifts the shop-dialog
 * block along the VGA aperture (host-safe scratch under DOS/4GW) and paces with
 * three fd2_delay_ms(10) calls. A paging move is therefore observed via
 * the delay-thunk spy (g_delay375b2_calls == 3); the direction that ran is
 * pinned independently by data_fd2_ui_menu_scroll_offset (page-up lands lower,
 * page-down lands higher), so no per-animation call counter is needed. */
extern int    g_delay375b2_calls;
/* cursor-move chime counter (testglob.c fd2_play_sfx_with_handle spy) */
extern int    g_play_sfx_with_handle_calls;
/* title-sprite blit spy from testglob.c (fd2_dialog_sprite_blit_normal) */
extern int    g_dlg_blit_normal_calls;
extern uint32 g_dlg_blit_last_dst;
extern uint32 g_dlg_blit_last_sprite;
extern uint32 g_dlg_blit_last_stride;

/* A distinct item_id_array pointer value the loop forwards verbatim to the
 * grid renderer (never dereferenced by the loop itself). */
#define SHOP_ARR 0x12345678u

/* Reset every observable seam + the navigation state, stage `n` scancodes, and
 * point the SFX bank at a nonzero handle so the chime path is exercised. */
static void shop_setup(uint32 item_count, uint32 cursor, uint32 scroll,
                       const uint8 *keys, int n)
{
    data_fd2_ui_menu_cursor_idx = cursor;
    data_fd2_ui_menu_scroll_offset = scroll;
    data_fd2_audio_fdother_sfx_bank_buf_ptr = 0x55AA;
    /* keep the real input wait off its idle/blink/corner paths */
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
    data_fd2_shared_rng_seed = 0;

    g_shop_grid_render_calls = 0;
    g_shop_grid_last_count = 0;
    g_shop_grid_last_array = 0;
    g_shop_grid_last_cursor = 0xFFFFFFFFu;
    g_shop_grid_last_dst = 0;
    g_shop_grid_last_sell = 0xFFFFFFFFu;
    g_delay375b2_calls = 0;
    g_play_sfx_with_handle_calls = 0;

    mfix_load_keys(keys, n);
    (void)item_count;
}

/* ---- commit / cancel terminators ---- */

static void test_commit_enter_returns_1(void)
{
    uint8 keys[1];
    int r;
    keys[0] = MFIX_SC_ENTER;
    shop_setup(8, 3, 0, keys, 1);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    /* commit does not move the cursor, fire SFX, or render */
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 3);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_shop_grid_render_calls, 0);
}

static void test_commit_space_returns_1(void)
{
    uint8 keys[1];
    int r;
    keys[0] = MFIX_SC_SPACE;
    shop_setup(8, 3, 0, keys, 1);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 3);
}

static void test_cancel_esc_returns_minus_1(void)
{
    uint8 keys[1];
    int r;
    keys[0] = MFIX_SC_ESC;
    shop_setup(8, 3, 0, keys, 1);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, -1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 3);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
}

/* an unhandled scancode is ignored (result stays 0, loop continues): a stray
 * key followed by Enter must still commit with no move. */
static void test_unhandled_key_ignored_then_commit(void)
{
    uint8 keys[2];
    int r;
    keys[0] = 0x10;            /* not a navigation/commit/cancel code */
    keys[1] = MFIX_SC_ENTER;
    shop_setup(8, 2, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ(g_shop_grid_render_calls, 0);
}

/* ---- Right (0x4D): cursor += 1 ---- */

static void test_right_moves_and_renders(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_RIGHT;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(8, 2, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 1);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 3);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);   /* 3-0=3, not >5: no page */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    /* grid render forwarded (item_count, array, cursor, 0xA0000, sell&0xff) */
    ASSERT_EQ(g_shop_grid_last_count, 8);
    ASSERT_EQ(g_shop_grid_last_array, SHOP_ARR);
    ASSERT_EQ(g_shop_grid_last_cursor, 3);
    ASSERT_EQ(g_shop_grid_last_dst, 0xa0000);
    ASSERT_EQ(g_shop_grid_last_sell, 1);
    ASSERT_EQ(g_delay375b2_calls, 0);   /* no page -> scroll-up anim not run */
}

/* Right at the last item (cursor == item_count-1): guard blocks the move, so no
 * SFX and no render — the loop just waits for the next key. */
static void test_right_at_last_item_no_move(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_RIGHT;
    keys[1] = MFIX_SC_ESC;
    shop_setup(8, 7, 2, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, -1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 7);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_shop_grid_render_calls, 0);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 2);
}

/* sell_mode_flag is forwarded masked to its low byte (PUSH of MOVZX AL). */
static void test_sell_mode_masked_low_byte(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_RIGHT;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(8, 0, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0x1FF);   /* low byte = 0xFF */
    ASSERT_EQ(r, 1);
    ASSERT_EQ(g_shop_grid_last_sell, 0xFF);
}

/* ---- Left (0x4B): cursor -= 1 ---- */

static void test_left_moves(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_LEFT;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(8, 3, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);    /* 2 !< 0: no page */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_cursor, 2);
    ASSERT_EQ(g_delay375b2_calls, 0);                /* no page: no anim pace */
}

/* Left at cursor 0: guard blocks the move. */
static void test_left_at_zero_no_move(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_LEFT;
    keys[1] = MFIX_SC_ESC;
    shop_setup(8, 0, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, -1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_shop_grid_render_calls, 0);
}

/* ---- Down (0x50): cursor += 2 (row down) ---- */

static void test_down_moves_two(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_DOWN;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(8, 1, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 3);       /* 1 + 2 */
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);    /* 3-0=3, not >5 */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_cursor, 3);
}

/* Down boundary: guard is cursor < item_count-2. With count=8, cursor=6 is NOT
 * < 6, so no move. */
static void test_down_at_last_row_no_move(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_DOWN;
    keys[1] = MFIX_SC_ESC;
    shop_setup(8, 6, 2, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, -1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 6);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_shop_grid_render_calls, 0);
}

/* ---- Up (0x48): cursor -= 2 (row up) ---- */

static void test_up_moves_two(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_UP;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(8, 4, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 2);       /* 4 - 2 */
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);    /* 2 !< 0: no page */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_cursor, 2);
}

/* Up boundary: guard is 1 < cursor. cursor=1 is NOT > 1, so no move. */
static void test_up_at_top_row_no_move(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_UP;
    keys[1] = MFIX_SC_ESC;
    shop_setup(8, 1, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, -1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 1);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 0);
    ASSERT_EQ(g_shop_grid_render_calls, 0);
}

/* ---- viewport paging: page DOWN (scroll += 2) via Right ---- */

/* Right from cursor 5 (scroll 0) -> cursor 6; delta = 6-0 = 6 > 5, so the
 * viewport pages down by 2 and the scroll-up animation fires. */
static void test_right_pages_viewport_down(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_RIGHT;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(12, 5, 0, keys, 2);
    r = fd2_shop_menu_input_loop(12, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 6);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 2);    /* page-DOWN branch */
    /* the page-down scroll-up anim paced 3 delays (10ms each) */
    ASSERT_EQ(g_delay375b2_calls, 3);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_cursor, 6);
}

/* Down also reaches the shared page-down block (LAB_dfb8). cursor 4 (scroll 0)
 * -> 6; delta 6 > 5 -> scroll 2 + scroll-up anim. */
static void test_down_pages_viewport_down(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_DOWN;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(12, 4, 0, keys, 2);
    r = fd2_shop_menu_input_loop(12, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 6);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 2);
    ASSERT_EQ(g_delay375b2_calls, 3);   /* page-down -> real scroll-up anim */
    ASSERT_EQ(g_shop_grid_render_calls, 1);
}

/* Right just inside the viewport must NOT page: cursor 4 (scroll 0) -> 5;
 * delta 5 is not > 5. */
static void test_right_no_page_at_viewport_edge(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_RIGHT;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(12, 4, 0, keys, 2);
    r = fd2_shop_menu_input_loop(12, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 5);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);
    ASSERT_EQ(g_delay375b2_calls, 0);   /* delta 5 not >5: no page, no anim */
}

/* ---- viewport paging: page UP (scroll -= 2) via Left ---- */

/* Left from cursor 2 (scroll 2) -> cursor 1; 1 < 2, so the viewport pages up by
 * 2 and the scroll-down animation fires. */
static void test_left_pages_viewport_up(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_LEFT;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(12, 2, 2, keys, 2);
    r = fd2_shop_menu_input_loop(12, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 1);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);    /* page-UP branch */
    /* page-up ran the REAL scroll-down anim (3 paced 10ms delays) */
    ASSERT_EQ(g_delay375b2_calls, 3);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_cursor, 1);
}

/* Up also reaches the shared page-up block (LAB_e01e). cursor 3 (scroll 2) ->
 * 1; 1 < 2 -> scroll 0 + scroll-down anim. */
static void test_up_pages_viewport_up(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_UP;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(12, 3, 2, keys, 2);
    r = fd2_shop_menu_input_loop(12, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 1);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);    /* page-UP branch */
    ASSERT_EQ(g_delay375b2_calls, 3);   /* page-up -> real scroll-down anim */
    ASSERT_EQ(g_shop_grid_render_calls, 1);
}

/* Left staying within the current page must NOT page: cursor 3 (scroll 2) -> 2;
 * 2 is not < 2. */
static void test_left_no_page_within_viewport(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_LEFT;
    keys[1] = MFIX_SC_ENTER;
    shop_setup(12, 3, 2, keys, 2);
    r = fd2_shop_menu_input_loop(12, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 2);
    ASSERT_EQ(g_delay375b2_calls, 0);                /* no page: no anim pace */
}

/* ---- multi-key navigation: several accepted moves before commit ---- */

static void test_multi_move_then_commit(void)
{
    uint8 keys[4];
    int r;
    keys[0] = MFIX_SC_DOWN;    /* 0 -> 2 */
    keys[1] = MFIX_SC_DOWN;    /* 2 -> 4 */
    keys[2] = MFIX_SC_RIGHT;   /* 4 -> 5 */
    keys[3] = MFIX_SC_ENTER;   /* commit */
    shop_setup(12, 0, 0, keys, 4);
    r = fd2_shop_menu_input_loop(12, (uint8 *)SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 5);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);    /* 5-0=5, never >5 */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 3);       /* one chime per move */
    ASSERT_EQ(g_shop_grid_render_calls, 3);           /* one render per move */
    ASSERT_EQ(g_shop_grid_last_cursor, 5);
}

/* ================================================================
 * fd2_open_shop_dialog_panel @ 0x2E0BD — panel open with slide-down.
 *
 * Drives the REAL open end-to-end. The three 64000-byte workspaces are
 * malloc'd for real; the framebuffer-snapshot memmove(snapshot, 0xA0000) and
 * the per-frame fd2_slide_panel_down_step memmove(0xA0000, ...) target the VGA
 * aperture, which under DOS/4GW is real RAM so the access is harmless (same
 * convention as the close-side test_close_status_screen_slide_out and
 * tests/rsrc fd2_load_chapter_portrait). The title-sprite blit and the item-
 * grid render are the recording testglob spies, so the composited writes the
 * test asserts on never touch real video state.
 *
 * The function leaks the three workspaces (the close counterpart frees them in
 * game), so the test free()s all three and zeroes the globals afterward.
 *
 * Slide-loop bounds (proves the 64000-byte buffers suffice for all 6 frames):
 * the worst case is the final frame y=0x70 -> row_count clamps to 0x56 (86)
 * rows; the top dst write is workspace_a + 0x70*0x140 + 5 + 85*0x140 + 0x135
 * = workspace_a + 63354 < 64000 and the top src read is workspace_c + 0x8C05 +
 * 85*0x140 + 0x135 = workspace_c + 63354 < 64000. Every frame y>=0x70 yields
 * the same or fewer rows, so all stay in bounds.
 *
 * Risk coverage: the three malloc->global stores, the title-sprite atlas-offset
 * address math (atlas + *(atlas+0x46)) and its fixed dst (composed+0x8C05) /
 * stride 0x140, the grid-render argument forwarding (count, array, cursor read
 * from 0x53C57, dst = composed_target, sell_mode low byte), and the 6-frame
 * slide loop running to completion in-bounds.
 * ================================================================ */

/* A small backing buffer for the sprite atlas. Slot +0x46 holds the relative
 * sprite offset the open fn adds to the atlas base to form the blit source. */
static uint8 g_shop_atlas[0x100];

/* item_id_array pointer the open fn forwards verbatim to the grid renderer
 * (never dereferenced). */
#define OPEN_ARR 0x0BADF00Du

static void open_reset_observed(void)
{
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;

    g_dlg_blit_normal_calls = 0;
    g_dlg_blit_last_dst = 0;
    g_dlg_blit_last_sprite = 0;
    g_dlg_blit_last_stride = 0;

    g_shop_grid_render_calls = 0;
    g_shop_grid_last_count = 0;
    g_shop_grid_last_array = 0;
    g_shop_grid_last_cursor = 0xFFFFFFFFu;
    g_shop_grid_last_dst = 0;
    g_shop_grid_last_sell = 0xFFFFFFFFu;
}

/* Free the three leaked workspaces and zero the globals (no dangling ptrs). */
static void open_free_workspaces(void)
{
    if (data_fd2_ui_slide_anim_accumulator_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
        data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_bg_snapshot_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
        data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_composed_target_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_composed_target_buf_ptr);
        data_fd2_ui_slide_composed_target_buf_ptr = 0;
    }
}

static void test_open_shop_dialog_composites_and_slides(void)
{
    uint32 composed;
    uint32 atlas_base;
    uint32 sprite_off;

    open_reset_observed();

    /* sprite atlas: relative offset 0x1234 stored at slot +0x46 */
    sprite_off = 0x1234u;
    *(uint32 *)(g_shop_atlas + 0x46) = sprite_off;
    atlas_base = (uint32)g_shop_atlas;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = atlas_base;

    data_fd2_ui_menu_cursor_idx = 4;

    fd2_open_shop_dialog_panel(8, (uint8 *)OPEN_ARR, 0);

    /* three workspaces allocated, distinct, non-zero */
    ASSERT_TRUE(data_fd2_ui_slide_anim_accumulator_buf_ptr != 0);
    ASSERT_TRUE(data_fd2_ui_slide_bg_snapshot_buf_ptr != 0);
    ASSERT_TRUE(data_fd2_ui_slide_composed_target_buf_ptr != 0);
    ASSERT_NE(data_fd2_ui_slide_anim_accumulator_buf_ptr,
              data_fd2_ui_slide_bg_snapshot_buf_ptr);
    ASSERT_NE(data_fd2_ui_slide_bg_snapshot_buf_ptr,
              data_fd2_ui_slide_composed_target_buf_ptr);

    composed = data_fd2_ui_slide_composed_target_buf_ptr;

    /* title-sprite blit fired once: dst = composed+0x8C05, stride 0x140,
     * sprite = atlas_base + *(atlas_base+0x46) */
    ASSERT_EQ(g_dlg_blit_normal_calls, 1);
    ASSERT_EQ(g_dlg_blit_last_dst, composed + 0x8c05);
    ASSERT_EQ(g_dlg_blit_last_stride, 0x140);
    ASSERT_EQ(g_dlg_blit_last_sprite, atlas_base + sprite_off);

    /* item grid rendered once into composed_target with forwarded args */
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_count, 8);
    ASSERT_EQ(g_shop_grid_last_array, OPEN_ARR);
    ASSERT_EQ(g_shop_grid_last_cursor, 4);          /* read from 0x53C57 */
    ASSERT_EQ(g_shop_grid_last_dst, composed);       /* composed_target, not VGA */
    ASSERT_EQ(g_shop_grid_last_sell, 0);

    open_free_workspaces();
}

/* sell_mode_flag is forwarded masked to its low byte (MOVZX AL at the call
 * site); the cursor highlight is read live from 0x53C57. */
static void test_open_shop_dialog_sell_mode_masked(void)
{
    open_reset_observed();

    *(uint32 *)(g_shop_atlas + 0x46) = 0u;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_shop_atlas;
    data_fd2_ui_menu_cursor_idx = 9;

    fd2_open_shop_dialog_panel(20, (uint8 *)OPEN_ARR, 0x1FFu);   /* low byte = 0xFF */

    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_count, 20);
    ASSERT_EQ(g_shop_grid_last_cursor, 9);
    ASSERT_EQ(g_shop_grid_last_sell, 0xFF);

    open_free_workspaces();
}

/* ================================================================
 * fd2_pick_stat_compare_color @ 0x2EF8F — 3-branch signed comparator.
 *
 * Pure function: maps (current_stat, preview_stat) -> digit-color sprite
 * base. Branch map (signed):
 *   current == preview -> 0x1F
 *   current  <  preview -> 0x2A
 *   current  >  preview -> 0x77
 *
 * Risk coverage: each of the three branches returns its exact constant;
 * the comparison is SIGNED (disasm JGE + int operands), so the negative
 * cases (current=-1 vs preview=1, current=1 vs preview=-1, and an equal
 * pair at a negative value) pin the signedness — an accidental unsigned
 * compare would flip those two ordered cases.
 * ================================================================ */

static void test_stat_color_equal_returns_red(void)
{
    ASSERT_EQ(fd2_pick_stat_compare_color(50, 50), 0x1fu);
    ASSERT_EQ(fd2_pick_stat_compare_color(0, 0), 0x1fu);
    ASSERT_EQ(fd2_pick_stat_compare_color(-7, -7), 0x1fu);   /* equal, negative */
}

static void test_stat_color_current_less_returns_white(void)
{
    ASSERT_EQ(fd2_pick_stat_compare_color(10, 25), 0x2au);
    ASSERT_EQ(fd2_pick_stat_compare_color(0, 1), 0x2au);
    /* signed: -1 < 1 -> 0x2A (unsigned 0xFFFFFFFF would be > 1 -> 0x77) */
    ASSERT_EQ(fd2_pick_stat_compare_color(-1, 1), 0x2au);
}

static void test_stat_color_current_greater_returns_orange(void)
{
    ASSERT_EQ(fd2_pick_stat_compare_color(25, 10), 0x77u);
    ASSERT_EQ(fd2_pick_stat_compare_color(1, 0), 0x77u);
    /* signed: 1 > -1 -> 0x77 */
    ASSERT_EQ(fd2_pick_stat_compare_color(1, -1), 0x77u);
}

/* ================================================================
 * fd2_run_buy_item_menu @ 0x2F0B0 — buy-branch top-level loop.
 *
 * Drives the REAL outer loop through its CANCEL exit: each iteration the loop
 * restores the cursor/scroll from their saved copies, opens the item panel
 * (real fd2_open_shop_dialog_panel: 3 x 64000-byte mallocs + title-blit/grid
 * spies + slide steps into the host-safe VGA aperture), runs the real shop
 * input loop, saves the cursor/scroll back, then closes. A staged Esc makes the
 * input loop return -1 on the first key, so the loop returns immediately —
 * before any item-table / party-state / recipient-select code runs.
 *
 * fd2_close_intro_dialog_with_slide_out is a no-op stub here (it frees the 3
 * workspaces in game), so the test frees them itself after the call (same as
 * the open-panel test).
 *
 * Risk coverage: the loop-top cursor/scroll RESTORE-from-saved (cursor and
 * scroll are seeded different from their saved copies, so a swapped
 * save/restore direction would land the wrong value), the matching SAVE-back
 * after the input loop, the sell_mode=0 buy-branch flag forwarded to the panel
 * open + grid render, and the sel==-1 early return (the function returns rather
 * than looping). The deeper buy mechanics (eligibility filter, confirm,
 * affordability, recipient select, inventory check, add/auto-equip, money anim)
 * are dominated by blocking input + VGA blits + not-yet-emitted callees and are
 * deferred to Phase 9 integration.
 * ================================================================ */

/* sprite-atlas backing buffer for fd2_open_shop_dialog_panel's title blit
 * (it reads the relative sprite offset at slot +0x46, then calls the spy). */
static uint8 g_buy_atlas[0x100];

#define BUY_ARR 0xCAFEF00Du

static void buy_free_workspaces(void)
{
    if (data_fd2_ui_slide_anim_accumulator_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
        data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_bg_snapshot_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
        data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_composed_target_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_composed_target_buf_ptr);
        data_fd2_ui_slide_composed_target_buf_ptr = 0;
    }
}

static void test_buy_menu_cancel_returns_and_persists_cursor(void)
{
    uint8 keys[1];

    /* observed seams + slide-buffer globals */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    g_dlg_blit_normal_calls = 0;
    g_shop_grid_render_calls = 0;
    g_shop_grid_last_sell = 0xFFFFFFFFu;
    g_shop_grid_last_count = 0;
    g_shop_grid_last_array = 0;
    g_shop_grid_last_cursor = 0xFFFFFFFFu;

    /* keep the real input wait off its idle/blink/corner paths */
    data_fd2_shared_rng_seed = 0;
    *(uint32 *)(g_buy_atlas + 0x46) = 0u;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_buy_atlas;
    data_fd2_audio_fdother_sfx_bank_buf_ptr = 0x55AA;

    /* saved cursor/scroll differ from the live ones: the loop must overwrite
     * the live cursor/scroll FROM the saved copies at the top, then (Esc moves
     * nothing) save the same values back. */
    data_fd2_ui_menu_saved_cursor_idx = 5;
    data_fd2_ui_menu_saved_scroll_offset = 4;
    data_fd2_ui_menu_cursor_idx = 1;
    data_fd2_ui_menu_scroll_offset = 0;

    keys[0] = MFIX_SC_ESC;
    mfix_load_keys(keys, 1);

    fd2_run_buy_item_menu(8, (uint8 *)BUY_ARR);

    /* loop restored cursor/scroll from saved (5/4), Esc moved nothing, then
     * saved them back -> all four equal the saved seeds. */
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 5);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 4);
    ASSERT_EQ(data_fd2_ui_menu_saved_cursor_idx, 5);
    ASSERT_EQ(data_fd2_ui_menu_saved_scroll_offset, 4);

    /* the panel opened exactly once (one iteration), buy-branch sell_mode=0,
     * with the restored cursor (5) and forwarded count/array. */
    ASSERT_EQ(g_dlg_blit_normal_calls, 1);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_sell, 0);
    ASSERT_EQ(g_shop_grid_last_count, 8);
    ASSERT_EQ(g_shop_grid_last_array, BUY_ARR);
    ASSERT_EQ(g_shop_grid_last_cursor, 5);

    buy_free_workspaces();
}

/* ================================================================
 * fd2_run_sell_item_menu @ 0x2F642 — sell-branch top-level loop.
 *
 * Drives the REAL outer loop. The seller roster select
 * (fd2_party_roster_single_select_loop) is the scripted testglob stub
 * (g_single_select_ret / _cursor): it returns a seller index then a cancel so
 * the loop runs exactly one productive iteration and then exits.
 *
 * test_sell_menu_cancel_returns_immediately: the first seller select cancels
 * (-1). The loop must set data_fd2_ui_menu_visible_item_count from the party
 * count at the top, then return without opening the item panel — pinning the
 * loop-top visible-count store and the sel==-1 early return.
 *
 * test_sell_menu_builds_inventory_list_skips_empty: drives the host-safe part of
 * a productive iteration. The seller's 8 inventory slots are seeded with a mix
 * of empty (flag bit 0x80 set) and occupied slots; the loop must build the
 * compacted item-id list skipping the empty slots, reset cursor/scroll to 0,
 * open the REAL sell-mode panel (sell_mode=1: 3x64000 mallocs + title/grid spies
 * + slide steps into the host-safe VGA aperture), then run the REAL shop input
 * loop, which a staged Esc cancels (-1) so the iteration loops back to the
 * seller roster (scripted to cancel the 2nd time). The forwarded count and the
 * captured list contents (g_shop_grid_capture_list) prove the skip-empty build
 * and ordering; the open panel's grid render pins sell_mode=1 and the cursor
 * reset to 0.
 *
 * The deeper sell mechanics past the item-grid commit (the chosen-item lookup,
 * the 75%-resale price = item_entry[+0x13]*3/4, the "sell?" confirm typewriter,
 * the transaction + money-gain animation, the slot removal + stat recompute) sit
 * behind the blocking confirm dialog + real VGA blits + the item-effect/runtime
 * tables, exactly as the sibling fd2_run_buy_item_menu deferred its post-confirm
 * flow; they are deferred to Phase 9 integration. The "nothing to sell" reject
 * dialog (empty inventory) likewise drives the real dialog VM + portrait path
 * and is deferred with it.
 * ================================================================ */

/* scripted seller-select stub controls (testglob.c) */
extern int  g_single_select_ret[8];
extern int  g_single_select_cursor[8];
extern int  g_single_select_idx;
extern int  g_single_select_calls;
/* opt-in capture of the forwarded item-id list (testglob.c grid-render spy) */
extern int   g_shop_grid_capture_list;
extern uint8 g_shop_grid_list[32];

/* sprite-atlas backing buffer for fd2_open_shop_dialog_panel's title blit. */
static uint8 g_sell_atlas[0x100];
/* a small runtime_char array so the inventory-build loop can index the seller. */
static runtime_char g_sell_chars[4];

static void sell_free_workspaces(void)
{
    if (data_fd2_ui_slide_anim_accumulator_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
        data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_bg_snapshot_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
        data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_composed_target_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_composed_target_buf_ptr);
        data_fd2_ui_slide_composed_target_buf_ptr = 0;
    }
}

/* reset the seller-select script + the slide-buffer globals + grid spies. */
static void sell_reset(void)
{
    int i;
    for (i = 0; i < 8; i++) {
        g_single_select_ret[i] = -1;
        g_single_select_cursor[i] = 0;
    }
    g_single_select_idx = 0;
    g_single_select_calls = 0;

    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;

    g_dlg_blit_normal_calls = 0;
    g_shop_grid_render_calls = 0;
    g_shop_grid_last_count = 0;
    g_shop_grid_last_array = 0;
    g_shop_grid_last_cursor = 0xFFFFFFFFu;
    g_shop_grid_last_dst = 0;
    g_shop_grid_last_sell = 0xFFFFFFFFu;
    g_shop_grid_capture_list = 0;
    memset(g_shop_grid_list, 0xEE, sizeof(g_shop_grid_list));

    /* keep the real input wait off its idle/blink/corner paths */
    data_fd2_shared_rng_seed = 0;
    *(uint32 *)(g_sell_atlas + 0x46) = 0u;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_sell_atlas;
    data_fd2_audio_fdother_sfx_bank_buf_ptr = 0x55AA;
}

static void test_sell_menu_cancel_returns_immediately(void)
{
    sell_reset();

    /* first (and only) seller select cancels */
    g_single_select_ret[0] = -1;

    data_fd2_shared_menu_party_member_count = 6;
    data_fd2_ui_menu_visible_item_count = 0;   /* must be overwritten at top */

    fd2_run_sell_item_menu();

    /* loop-top store ran, then sel==-1 returned before opening the panel */
    ASSERT_EQ(g_single_select_calls, 1);
    ASSERT_EQ(data_fd2_ui_menu_visible_item_count, 6);
    ASSERT_EQ(g_shop_grid_render_calls, 0);
    ASSERT_EQ(g_dlg_blit_normal_calls, 0);

    sell_free_workspaces();
}

static void test_sell_menu_builds_inventory_list_skips_empty(void)
{
    uint8 keys[1];
    runtime_char *seller;
    runtime_char *saved_rc;

    sell_reset();

    /* this test repoints the shared runtime-char array global; save it so the
     * later suites (status.c etc.) that rely on its default wiring are not
     * disturbed (same save/restore discipline as tests/battle/battle2.c). */
    saved_rc = data_fd2_battle_runtime_char_array_ptr;

    /* seller = index 2; 1st select returns it, 2nd select cancels the loop. */
    g_single_select_ret[0] = 1;       /* any non -1 commit code */
    g_single_select_cursor[0] = 2;    /* seller index written to cursor_idx */
    g_single_select_ret[1] = -1;      /* exit after the productive iteration */

    /* seed seller 2's 8 inventory slots (each = [flag, item_id]): occupy 0,1,4,6
     * (item ids 0x10,0x11,0x14,0x16) and mark 2,3,5,7 empty (flag bit 0x80). */
    seller = &g_sell_chars[2];
    memset(g_sell_chars, 0, sizeof(g_sell_chars));
    seller->inventory_slots[0]  = 0x00; seller->inventory_slots[1]  = 0x10;
    seller->inventory_slots[2]  = 0x00; seller->inventory_slots[3]  = 0x11;
    seller->inventory_slots[4]  = 0x80; seller->inventory_slots[5]  = 0x99;
    seller->inventory_slots[6]  = 0x80; seller->inventory_slots[7]  = 0x98;
    seller->inventory_slots[8]  = 0x00; seller->inventory_slots[9]  = 0x14;
    seller->inventory_slots[10] = 0x80; seller->inventory_slots[11] = 0x97;
    seller->inventory_slots[12] = 0x00; seller->inventory_slots[13] = 0x16;
    seller->inventory_slots[14] = 0x80; seller->inventory_slots[15] = 0x96;
    data_fd2_battle_runtime_char_array_ptr = g_sell_chars;

    /* distinct from the built inv_count (4) so the final visible-count store
     * (set from the party count at the top of the looped-back 2nd iteration)
     * is unambiguously the party count, proving the iteration looped back. */
    data_fd2_shared_menu_party_member_count = 5;
    /* seed cursor/scroll non-zero so the loop's reset-to-0 is observable */
    data_fd2_ui_menu_cursor_idx = 7;
    data_fd2_ui_menu_scroll_offset = 6;

    g_shop_grid_capture_list = 1;     /* the array is the real on-stack list */

    keys[0] = MFIX_SC_ESC;            /* cancel the shop item grid */
    mfix_load_keys(keys, 1);

    fd2_run_sell_item_menu();

    /* both scripted selects consumed (seller, then exit) */
    ASSERT_EQ(g_single_select_calls, 2);

    /* the panel opened once with the compacted list: 4 occupied slots, the
     * empties (0x99/0x98/0x97/0x96) dropped, original order preserved. */
    ASSERT_EQ(g_dlg_blit_normal_calls, 1);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_count, 4);
    ASSERT_EQ(g_shop_grid_last_sell, 1);          /* sell_mode=1 */
    ASSERT_EQ(g_shop_grid_last_cursor, 0);        /* cursor reset to 0 at open */
    ASSERT_EQ(g_shop_grid_list[0], 0x10);
    ASSERT_EQ(g_shop_grid_list[1], 0x11);
    ASSERT_EQ(g_shop_grid_list[2], 0x14);
    ASSERT_EQ(g_shop_grid_list[3], 0x16);

    /* the open also reset the scroll offset to 0 */
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);
    /* after the shop-grid Esc the iteration looped back to the seller roster:
     * the loop-top visible-count store ran again from the party count (5). */
    ASSERT_EQ(data_fd2_ui_menu_visible_item_count, 5);

    g_shop_grid_capture_list = 0;
    data_fd2_battle_runtime_char_array_ptr = saved_rc;
    sell_free_workspaces();
}

/* ================================================================
 * fd2_run_equip_member_menu @ 0x2F883 — equip-branch top-level loop.
 *
 * Drives the REAL outer loop through its CANCEL exit. The member roster select
 * (fd2_party_roster_single_select_loop) is the scripted testglob stub
 * (g_single_select_ret / _cursor); fd2_close_intro_dialog_with_slide_out is the
 * testglob no-op stub.
 *
 * test_equip_menu_cancel_returns_immediately: the (only) member select cancels
 * (-1). The loop must (1) store data_fd2_ui_menu_visible_item_count from the
 * party count at the top, (2) call the roster select exactly once, (3) snapshot
 * data_fd2_dialog_active_portrait_blit_offset, then (4) return on sel==-1 —
 * before the per-member equip submenu or the portrait reload run. Pins the
 * loop-top visible-count store and the sel==-1 early return, and confirms the
 * Esc path leaves data_fd2_dialog_active_portrait_blit_offset untouched (its
 * restore lives only on the productive path, which is skipped on Esc).
 *
 * The productive iteration cannot be unit-driven: on a non-Esc selection the
 * loop immediately enters fd2_equip_unequip_inventory_menu @ 0x1BFFE, the heavy
 * EQUIP/UNEQUIP modal that busy-waits on the BIOS keyboard buffer with no
 * in-process key source (documented as un-unit-testable in tests/ui_menu/
 * status.c). The blit-offset save/restore round-trip and the post-submenu
 * portrait reload (real fd2_load_dat_resource against staged DATO.DAT) sit
 * behind that blocking modal and are deferred to Phase 9 integration under the
 * emulator — the same deferral the sibling buy/sell loops apply to their
 * post-commit flows.
 * ================================================================ */

static void test_equip_menu_cancel_returns_immediately(void)
{
    int i;

    /* reset the scripted member-select stub */
    for (i = 0; i < 8; i++) {
        g_single_select_ret[i] = -1;
        g_single_select_cursor[i] = 0;
    }
    g_single_select_idx = 0;
    g_single_select_calls = 0;

    /* first (and only) member select cancels */
    g_single_select_ret[0] = -1;

    data_fd2_shared_menu_party_member_count = 7;
    data_fd2_ui_menu_visible_item_count = 0;   /* must be overwritten at top */
    /* sentinel the Esc path must NOT touch (the restore is productive-only) */
    data_fd2_dialog_active_portrait_blit_offset = 0xABCD1234u;

    fd2_run_equip_member_menu();

    /* loop-top store ran, select called once, then sel==-1 returned before the
     * equip submenu or any portrait reload. */
    ASSERT_EQ(g_single_select_calls, 1);
    ASSERT_EQ(data_fd2_ui_menu_visible_item_count, 7);
    /* the Esc path leaves the blit offset as seeded (restore path skipped) */
    ASSERT_EQ(data_fd2_dialog_active_portrait_blit_offset, 0xABCD1234u);
}

void run_ui_menu_shop1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/shop1\n");
    RUN_TEST(test_commit_enter_returns_1);
    RUN_TEST(test_commit_space_returns_1);
    RUN_TEST(test_cancel_esc_returns_minus_1);
    RUN_TEST(test_unhandled_key_ignored_then_commit);
    RUN_TEST(test_right_moves_and_renders);
    RUN_TEST(test_right_at_last_item_no_move);
    RUN_TEST(test_sell_mode_masked_low_byte);
    RUN_TEST(test_left_moves);
    RUN_TEST(test_left_at_zero_no_move);
    RUN_TEST(test_down_moves_two);
    RUN_TEST(test_down_at_last_row_no_move);
    RUN_TEST(test_up_moves_two);
    RUN_TEST(test_up_at_top_row_no_move);
    RUN_TEST(test_right_pages_viewport_down);
    RUN_TEST(test_down_pages_viewport_down);
    RUN_TEST(test_right_no_page_at_viewport_edge);
    RUN_TEST(test_left_pages_viewport_up);
    RUN_TEST(test_up_pages_viewport_up);
    RUN_TEST(test_left_no_page_within_viewport);
    RUN_TEST(test_multi_move_then_commit);
    RUN_TEST(test_open_shop_dialog_composites_and_slides);
    RUN_TEST(test_open_shop_dialog_sell_mode_masked);
    RUN_TEST(test_stat_color_equal_returns_red);
    RUN_TEST(test_stat_color_current_less_returns_white);
    RUN_TEST(test_stat_color_current_greater_returns_orange);
    RUN_TEST(test_buy_menu_cancel_returns_and_persists_cursor);
    RUN_TEST(test_sell_menu_cancel_returns_immediately);
    RUN_TEST(test_sell_menu_builds_inventory_list_skips_empty);
    RUN_TEST(test_equip_menu_cancel_returns_immediately);
}
