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
extern int    g_shop_scroll_up_calls;
extern int    g_shop_scroll_down_calls;
/* cursor-move chime counter (testglob.c fd2_play_sfx_with_handle spy) */
extern int    g_play_sfx_with_handle_calls;

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
    g_shop_scroll_up_calls = 0;
    g_shop_scroll_down_calls = 0;
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 3);
}

static void test_cancel_esc_returns_minus_1(void)
{
    uint8 keys[1];
    int r;
    keys[0] = MFIX_SC_ESC;
    shop_setup(8, 3, 0, keys, 1);
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 1);
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
    ASSERT_EQ(g_shop_scroll_up_calls, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0x1FF);   /* low byte = 0xFF */
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);    /* 2 !< 0: no page */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_cursor, 2);
    ASSERT_EQ(g_shop_scroll_down_calls, 0);
}

/* Left at cursor 0: guard blocks the move. */
static void test_left_at_zero_no_move(void)
{
    uint8 keys[2];
    int r;
    keys[0] = MFIX_SC_LEFT;
    keys[1] = MFIX_SC_ESC;
    shop_setup(8, 0, 0, keys, 2);
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(8, SHOP_ARR, 0);
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
    r = fd2_shop_menu_input_loop(12, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 6);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 2);
    ASSERT_EQ(g_shop_scroll_up_calls, 1);
    ASSERT_EQ(g_shop_scroll_down_calls, 0);
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
    r = fd2_shop_menu_input_loop(12, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 6);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 2);
    ASSERT_EQ(g_shop_scroll_up_calls, 1);
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
    r = fd2_shop_menu_input_loop(12, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 5);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);
    ASSERT_EQ(g_shop_scroll_up_calls, 0);
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
    r = fd2_shop_menu_input_loop(12, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 1);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);
    ASSERT_EQ(g_shop_scroll_down_calls, 1);
    ASSERT_EQ(g_shop_scroll_up_calls, 0);
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
    r = fd2_shop_menu_input_loop(12, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 1);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);
    ASSERT_EQ(g_shop_scroll_down_calls, 1);
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
    r = fd2_shop_menu_input_loop(12, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 2);
    ASSERT_EQ(g_shop_scroll_down_calls, 0);
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
    r = fd2_shop_menu_input_loop(12, SHOP_ARR, 0);
    ASSERT_EQ(r, 1);
    ASSERT_EQ(data_fd2_ui_menu_cursor_idx, 5);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);    /* 5-0=5, never >5 */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 3);       /* one chime per move */
    ASSERT_EQ(g_shop_grid_render_calls, 3);           /* one render per move */
    ASSERT_EQ(g_shop_grid_last_cursor, 5);
}

void run_ui_menu_shop_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/shop\n");
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
}
