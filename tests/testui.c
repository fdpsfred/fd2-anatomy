/*
 * testui.c — Unit tests for cursor, pan, input functions
 */

#include <stdio.h>
#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* globals + stubs in testglob.c */
extern runtime_char g_test_rc_array[8];
extern int g_composite_call_count;

/* ---- Tests: cursor ---- */

static void test_cursor_move_up_basic(void)
{
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_anim_phase = 0;
    fd2_cursor_move_up();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 4);
}

static void test_cursor_move_up_at_top(void)
{
    data_fd2_battle_cursor_world_y = 0;
    g_composite_call_count = 0;
    fd2_cursor_move_up();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 0);
    ASSERT_EQ(g_composite_call_count, 1);
}

/* Scroll branch: screen_y<2 && origin_y!=0 -> world_y-- AND origin_y--,
 * screen_y unchanged, then composite (JMP 0x11B90). */
static void test_cursor_move_up_scroll(void)
{
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 1;
    data_fd2_battle_view_window_origin_y = 3;
    g_composite_call_count = 0;
    fd2_cursor_move_up();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 4);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 2);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 1);
    ASSERT_EQ(g_composite_call_count, 1);
}

static void test_cursor_move_down_basic(void)
{
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 3;
    data_fd2_battle_anim_phase = 0;
    fd2_cursor_move_down();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 6);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 4);
}

/* Scroll branch: world_y!=height-1 && screen_y>=6 && origin_y!=height-8 ->
 * world_y++ AND origin_y++, screen_y unchanged, then composite (JMP 0x11BEF).
 * height=15 (default) so height-8=7 != origin_y(3); screen_y(10)>5.
 * Mirrors test_cursor_move_up_scroll for the down direction. */
static void test_cursor_move_down_scroll(void)
{
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 10;
    data_fd2_battle_view_window_origin_y = 3;
    data_fd2_battle_anim_phase = 1;
    g_composite_call_count = 0;
    fd2_cursor_move_down();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 6);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_y, 10);
    ASSERT_EQ(g_composite_call_count, 1);
}

/* Bottom-edge no-op: world_y == map_height_tiles-1 -> JZ 0x11BEF, no INC at
 * all, world_y unchanged, only composite refresh. height=15 -> world_y=14. */
static void test_cursor_move_down_at_bottom(void)
{
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_battle_cursor_world_y = 14;
    g_composite_call_count = 0;
    fd2_cursor_move_down();
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 14);
    ASSERT_EQ(g_composite_call_count, 1);
}

static void test_cursor_move_right_basic(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_anim_phase = 0;
    fd2_cursor_move_right();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 6);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 6);
}

/* Scroll branch: world_x!=width-1 && screen_x>0xa && origin_x!=width-0xd ->
 * world_x++ AND origin_x++, screen_x unchanged, then composite (JMP 0x11C37).
 * width=20 so width-0xd=7 != origin_x(3); screen_x(11)>0xa.
 * Mirrors test_cursor_move_down_scroll for the right (X) direction; note the
 * X viewport width is 0xd (13) vs Y's 8, so down's tests do not cover this. */
static void test_cursor_move_right_scroll(void)
{
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 11;
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_anim_phase = 1;
    g_composite_call_count = 0;
    fd2_cursor_move_right();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 6);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 11);
    ASSERT_EQ(g_composite_call_count, 1);
}

/* Right-edge no-op: world_x == map_width_tiles-1 -> JZ 0x11C10, no INC at all,
 * world_x unchanged, only composite refresh. width=20 -> world_x=19. */
static void test_cursor_move_right_at_right_edge(void)
{
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_cursor_world_x = 19;
    g_composite_call_count = 0;
    fd2_cursor_move_right();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 19);
    ASSERT_EQ(g_composite_call_count, 1);
}

static void test_cursor_move_left_basic(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_anim_phase = 0;
    fd2_cursor_move_left();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 4);
}

/* Scroll branch: world_x!=0 && screen_x<2 && origin_x!=0 -> world_x-- AND
 * origin_x--, screen_x unchanged, then composite (JMP 0x11CA1).
 * X-axis mirror of test_cursor_move_up_scroll. */
static void test_cursor_move_left_scroll(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 1;
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_anim_phase = 1;
    g_composite_call_count = 0;
    fd2_cursor_move_left();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 4);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 2);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 1);
    ASSERT_EQ(g_composite_call_count, 1);
}

/* Left-edge no-op: world_x == 0 -> JZ 0x11CA1, no DEC at all, world_x
 * unchanged, only composite refresh. Mirrors the up/right edge tests. */
static void test_cursor_move_left_at_left_edge(void)
{
    data_fd2_battle_cursor_world_x = 0;
    g_composite_call_count = 0;
    fd2_cursor_move_left();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 0);
    ASSERT_EQ(g_composite_call_count, 1);
}

/* Inner-step with animation: world_x!=0 && screen_x>=2 -> world_x-- AND
 * screen_x--; anim_phase!=0 so the early-return (JZ 0x11CAB) is NOT taken and
 * composite still runs. Complements test_cursor_move_left_basic, which sets
 * anim_phase==0 to take the early-return (no composite). */
static void test_cursor_move_left_inner_step_anim(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_anim_phase = 1;
    g_composite_call_count = 0;
    fd2_cursor_move_left();
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 4);
    ASSERT_EQ(data_fd2_battle_cursor_screen_x, 4);
    ASSERT_EQ(g_composite_call_count, 1);
}

/* ---- Tests: pan ---- */

static void test_pan_to_char(void)
{
    g_test_rc_array[2].pos_x = 10;
    g_test_rc_array[2].pos_y = 8;
    data_fd2_battle_cursor_world_x = 10;
    data_fd2_battle_cursor_world_y = 8;
    data_fd2_battle_anim_phase = 1;
    fd2_pan_cursor_to_char(2);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 10);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 8);
}

/* ---- Tests: pan_cursor_to_tile_animated ---- */

static void test_pan_to_tile_same_pos(void)
{
    data_fd2_battle_cursor_world_x = 7;
    data_fd2_battle_cursor_world_y = 4;
    data_fd2_battle_anim_phase = 1;
    fd2_pan_cursor_to_tile_animated(7, 4);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 7);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 4);
}

static void test_pan_to_tile_moves_x(void)
{
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_anim_phase = 0;
    fd2_pan_cursor_to_tile_animated(8, 5);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 8);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 5);
}

/* ---- Tests: pan_cursor_and_window ---- */

static void test_pan_and_window_same_pos(void)
{
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_view_window_origin_y = 2;
    fd2_pan_cursor_and_window(3, 2);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 2);
}

/* X-loop increment path: target_ox(3) > origin_x(0) -> JGE 0x13614 taken each
 * step, INC cursor_world_x AND INC origin_x in lockstep (asm 0x13614/0x1361a),
 * 3 steps until origin_x==target_ox. Y-loop skipped (origin_y==target_oy==4 ->
 * JZ 0x13181). Per-step composite pinned: g_composite_call_count==3. Deltas
 * hand-derived from the +1-per-step DEC/INC asm (emulate blocked by __CHK LOCK
 * pcodeop). cursor_world_x advances +1 each step: 5 -> 8. */
static void test_pan_and_window_inc_x_lockstep(void)
{
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 4;
    data_fd2_battle_cursor_world_x = 5;
    g_composite_call_count = 0;
    fd2_pan_cursor_and_window(3, 4);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 8);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 4);
    ASSERT_EQ(g_composite_call_count, 3);
}

/* Y-loop decrement path: target_oy(2) < origin_y(5) -> JGE 0x1364d NOT taken,
 * DEC cursor_world_y AND DEC origin_y in lockstep (asm 0x1363f/0x13645), 3
 * steps until origin_y==target_oy. X-loop skipped (origin_x==target_ox==3 ->
 * JZ 0x13631). Per-step composite pinned: g_composite_call_count==3. cursor_
 * world_y retreats -1 each step: 9 -> 6. Complements the inc-X test to cover
 * the opposite signed branch and the Y axis. */
static void test_pan_and_window_dec_y_lockstep(void)
{
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_view_window_origin_y = 5;
    data_fd2_battle_cursor_world_y = 9;
    g_composite_call_count = 0;
    fd2_pan_cursor_and_window(3, 2);
    ASSERT_EQ(data_fd2_battle_view_window_origin_y, 2);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 6);
    ASSERT_EQ(data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ(g_composite_call_count, 3);
}

/* ---- Tests: keyboard / BIOS ---- */

static void test_read_bios_tick(void)
{
    uint16 t;
    t = fd2_read_bios_midnight_tick();
    ASSERT_TRUE(1);
}

static void test_kbd_buffer_empty(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    r = fd2_check_keyboard_buffer_nonempty();
    ASSERT_EQ(r, 0);
}

static void test_kbd_buffer_nonempty(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x22;
    r = fd2_check_keyboard_buffer_nonempty();
    ASSERT_NE(r, 0);
}

static void test_clear_kbd_buffer(void)
{
    *(volatile uint16 *)0x41AuL = 0x30;
    *(volatile uint16 *)0x41CuL = 0x40;
    fd2_clear_keyboard_buffer();
    ASSERT_EQ(*(volatile uint16 *)0x41CuL, 0x30);
}

/* ---- Tests: wait_one_bios_tick ---- */

static void test_wait_one_bios_tick_smoke(void)
{
    /* Deterministic: tick low word = 0x0100, cache preset to a
     * different value so the spin exits on the first compare. */
    *(volatile uint32 *)0x46CuL = 0x00000100uL;
    data_fd2_engine_wait_one_bios_tick_last_seen = 0x00000099uL;
    fd2_wait_one_bios_tick();
    ASSERT_EQ(data_fd2_engine_wait_one_bios_tick_last_seen,
              0x00000100uL);
}

/* Pins the SIGN-EXTENDED 16-bit read (asm MOVSX EAX,word ptr [0x46C]).
 * Tick dword 0x0001FFFF -> low word 0xFFFF -> sx16 -> 0xFFFFFFFF cached.
 * A full-32-bit read (the prior bug) would cache 0x0001FFFF instead.
 * Cache preset to 0 (!= 0xFFFFFFFF) so the spin exits on the first
 * compare; the assertion then discriminates the two widths. */
static void test_wait_one_bios_tick_sign_extend(void)
{
    *(volatile uint32 *)0x46CuL = 0x0001FFFFuL;
    data_fd2_engine_wait_one_bios_tick_last_seen = 0;
    fd2_wait_one_bios_tick();
    ASSERT_EQ(data_fd2_engine_wait_one_bios_tick_last_seen,
              0xFFFFFFFFuL);
}

/* Positive low word (high bit clear): 0x00007FFF sign-extends to
 * itself, and the upper tick word (0x0001) must be discarded.
 * Distinguishes the 16-bit read from a full-32-bit read once more,
 * and confirms the spin-compare matches on a re-read of the same
 * (sign-extended) value: cache preset equal -> would spin -> so we
 * preset NOT equal to guarantee exit, then verify the store width. */
static void test_wait_one_bios_tick_positive_word(void)
{
    *(volatile uint32 *)0x46CuL = 0x00017FFFuL;
    data_fd2_engine_wait_one_bios_tick_last_seen = 0;
    fd2_wait_one_bios_tick();
    ASSERT_EQ(data_fd2_engine_wait_one_bios_tick_last_seen,
              0x00007FFFuL);
}

/* ---- Tests: wait_n_bios_ticks ---- */

/* Pins the SIGN-EXTENDED 16-bit store width (asm MOVSX EAX,word ptr
 * [0x46C]; MOV [0x53A2C],EAX), identical to the wait_one_bios_tick
 * idiom. n_ticks=0 makes the spin exit on the first compare
 * (elapsed = sx16(tick)-sx16(tick) = 0, and 0 < 0 is false), so the
 * assertion isolates the store width. Tick dword 0x0001FFFF -> low
 * word 0xFFFF -> sx16 -> 0xFFFFFFFF; a full-32-bit store would leave
 * 0x0001FFFF instead. */
static void test_wait_n_bios_ticks_sign_extend(void)
{
    *(volatile uint32 *)0x46CuL = 0x0001FFFFuL;
    data_fd2_engine_wait_n_bios_ticks_last_seen = 0;
    fd2_wait_n_bios_ticks(0);
    ASSERT_EQ(data_fd2_engine_wait_n_bios_ticks_last_seen,
              0xFFFFFFFFuL);
}

/* Positive low word (high bit clear): 0x00007FFF sign-extends to
 * itself, and the upper tick word (0x0001) must be discarded. Once
 * more distinguishes the 16-bit store from a full-32-bit store.
 * n_ticks=0 exits immediately (no spin). */
static void test_wait_n_bios_ticks_positive_word(void)
{
    *(volatile uint32 *)0x46CuL = 0x00017FFFuL;
    data_fd2_engine_wait_n_bios_ticks_last_seen = 0;
    fd2_wait_n_bios_ticks(0);
    ASSERT_EQ(data_fd2_engine_wait_n_bios_ticks_last_seen,
              0x00007FFFuL);
}

/* ---- Tests: update_palette_cycle_anim ---- */

static void test_update_palette_cycle_anim_no_update(void)
{
    data_fd2_animation_palette_cycle_last_tick =
        (uint16)BIOS_TICK_WORD;
    data_fd2_animation_palette_cycle_frame_idx = 5;
    fd2_update_palette_cycle_anim();
    ASSERT_EQ(data_fd2_animation_palette_cycle_frame_idx, 5);
}

/* ---- Tests: get_inventory_slot_item_id ---- */

static void test_get_inventory_slot_item_id(void)
{
    uint8 r;
    g_test_rc_array[1].inventory_slots[2] = 0x40;
    g_test_rc_array[1].inventory_slots[3] = 0x2A;
    r = fd2_get_inventory_slot_item_id(1, 1);
    ASSERT_EQ(r, 0x2A);
}

/* ---- Tests: read_tile_attribute ---- */

/* sprite_idx==0 baseline: pins the +0 attr lookup and the terrain 0x1F mask
 * (meta byte 0xA3 -> 0x03). The sprite word is 0 here so the 0x3FF mask is not
 * exercised; that is covered by test_read_tile_attribute_sprite_mask below. */
static void test_read_tile_attribute(void)
{
    uint8 fake_map[16];
    uint8 fake_attr[4];
    uint8 out[8];

    memset(fake_map, 0, sizeof(fake_map));
    fake_map[4] = 0x00; fake_map[5] = 0x00;
    fake_map[6] = 0xA3;
    data_fd2_battle_tile_map_ptr = (uint32)fake_map;
    data_fd2_battle_map_width_tiles = 1;
    fake_attr[0] = 0xAA; fake_attr[1] = 0xBB;
    fake_attr[2] = 0xCC; fake_attr[3] = 0xDD;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)fake_attr;
    fd2_read_tile_attribute_at_pos(0, 0, (uint32)out);
    ASSERT_EQ(*(uint16 *)out, 0);
    ASSERT_EQ(*(uint16 *)(out + 2), 3);
    ASSERT_EQ(out[4], 0xAA);
    ASSERT_EQ(out[7], 0xDD);
}

/* 10-bit sprite mask: asm `MOV BX,[EAX]; AND BH,0x3` (0x12e67/0x12e6a) masks the
 * sprite word to 0x3FF before both the out[+0] store and the attr-table index.
 * Sprite word 0xFC07 -> 0xFC07 & 0x3FF == 0x0007 (high 6 bits dropped). A
 * mistranscribed mask (0x1FF / 0xFFF / missing) would change out[+0] and the
 * looked-up attr bytes, so both are asserted. Index 7 -> attr base + (int16)7*4
 * == +28, so fake_attr is sized to 32 with distinct bytes at 28..31; the
 * (int16) sign-extension equals zero-extension here since masked idx <= 0x3FF.
 * Terrain meta 0x5C & 0x1F == 0x1C also keeps the 5-bit mask exercised. */
static void test_read_tile_attribute_sprite_mask(void)
{
    uint8 fake_map[16];
    uint8 fake_attr[32];
    uint8 out[8];

    memset(fake_map, 0, sizeof(fake_map));
    memset(fake_attr, 0, sizeof(fake_attr));
    *(uint16 *)(fake_map + 4) = 0xFC07;
    fake_map[6] = 0x5C;
    data_fd2_battle_tile_map_ptr = (uint32)fake_map;
    data_fd2_battle_map_width_tiles = 1;
    fake_attr[28] = 0x11; fake_attr[29] = 0x22;
    fake_attr[30] = 0x33; fake_attr[31] = 0x44;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)fake_attr;
    fd2_read_tile_attribute_at_pos(0, 0, (uint32)out);
    ASSERT_EQ(*(uint16 *)out, 0x0007);
    ASSERT_EQ(*(uint16 *)(out + 2), 0x1C);
    ASSERT_EQ(out[4], 0x11);
    ASSERT_EQ(out[5], 0x22);
    ASSERT_EQ(out[6], 0x33);
    ASSERT_EQ(out[7], 0x44);
}

/* ---- Tests: palette range ---- */

static void test_set_vga_palette_range_basic(void)
{
    uint8 fake_pal[6];
    fake_pal[0] = 0x3F; fake_pal[1] = 0x20; fake_pal[2] = 0x10;
    fake_pal[3] = 0x05; fake_pal[4] = 0x00; fake_pal[5] = 0x3F;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;
    fd2_set_vga_palette_range(0, 1, 0x10);
    ASSERT_TRUE(1);
}

static void test_palette_remap_run(void)
{
    uint8 table[256];
    uint8 data[4];
    int i;
    for (i = 0; i < 256; i++) table[i] = (uint8)(255 - i);
    data[0] = 0; data[1] = 1; data[2] = 2; data[3] = 3;
    fd2_apply_palette_remap_run((uint32)table, 4, data);
    ASSERT_EQ(data[0], 255);
    ASSERT_EQ(data[1], 254);
    ASSERT_EQ(data[3], 252);
}

static void test_interpolate_palette(void)
{
    uint8 fake_pal[3];
    fake_pal[0] = 0x28; fake_pal[1] = 0x14; fake_pal[2] = 0x00;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;
    fd2_interpolate_palette_range_toward_color(0, 1, 0x28, 0, 0, 0);
    ASSERT_TRUE(1);
}

static void test_palette_fade_to_black(void)
{
    uint8 fake_pal[3];
    fake_pal[0] = 0x3F; fake_pal[1] = 0x3F; fake_pal[2] = 0x3F;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;
    fd2_palette_fade_to_black_step_loop(0, 0);
    ASSERT_TRUE(1);
}

static void test_set_vga_palette_range_with_add(void)
{
    uint8 fake_pal[6];
    fake_pal[0] = 0x30; fake_pal[1] = 0x3F; fake_pal[2] = 0x10;
    fake_pal[3] = 0x20; fake_pal[4] = 0x3E; fake_pal[5] = 0x00;
    data_fd2_vga_palette_data_ptr = (uint32)fake_pal;
    fd2_set_vga_palette_range_with_add(0, 1, 0x10);
    ASSERT_TRUE(1);
}

/* ---- Tests: wait_for_input_dialog_with_blink ---- */

static void test_wait_dialog_blink_esc(void)
{
    int r;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x011B;
    r = fd2_wait_for_input_dialog_with_blink(0);
    ASSERT_EQ(r, 0x01);
}

/* ---- Tests: wait_input_with_dialog_repaint ---- */

/* Loop-break control for driving the idle loop body exactly once; see the stub
 * fd2_repaint_settings_dialog_borders in testglob.c. */
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;

/* Scancode remap path (loop skipped: head!=tail so the buffer reads nonempty
 * and execution falls straight through to INT 16h + remap). The INT 16h scancode
 * is the HIGH byte of the word at the buffer head 0x41E (AH from INT 16h AH=10h),
 * exactly as the sibling fd2_wait_for_input_with_idle tests above. asm 0x179a4-
 * 0x179c3: 0xE0->0x1C, 0x52->0x1C, 0x53->0x01, else passthrough. */
static void test_wait_dialog_repaint_remap_52(void)
{
    int r;
    g_repaint_flip_buffer_after = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5200;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    ASSERT_EQ(r, 0x1c);
}

static void test_wait_dialog_repaint_remap_e0(void)
{
    int r;
    g_repaint_flip_buffer_after = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0xE000;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    ASSERT_EQ(r, 0x1c);
}

static void test_wait_dialog_repaint_remap_53(void)
{
    int r;
    g_repaint_flip_buffer_after = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5300;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    ASSERT_EQ(r, 0x01);
}

static void test_wait_dialog_repaint_passthrough(void)
{
    int r;
    g_repaint_flip_buffer_after = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    ASSERT_EQ(r, 0x39);
}

/* Blink oscillator state-transition (asm 0x178bf-0x178f8): runs the idle loop
 * BODY exactly once via the repaint-stub buffer flip, then the seeded passthrough
 * scancode exits. Buffer starts EMPTY (head==tail==0x1E) so the loop is entered;
 * the stub flips tail->head+2 on its first call so the next top-of-loop check
 * exits. Oscillator trigger is forced deterministically: tick_latch=0 and BIOS
 * tick low word 0x4000 give diff=0x4000 (>3, high bit clear) regardless of the
 * free-running timer ISR (wrap into 0..3 is impossible in one tick from 0x4000),
 * so the 0/1 oscillator advances (+1, wrap at 2). 0->1 here (no wrap). */
static void test_wait_dialog_repaint_oscillator_0_to_1(void)
{
    int r;
    g_repaint_settings_calls = 0;
    g_repaint_flip_buffer_after = 1;
    data_fd2_dialog_blink_phase_oscillator = 0;
    data_fd2_dialog_blink_phase_oscillator_tick_latch = 0;
    *(volatile uint32 *)0x46CuL = 0x00004000uL;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x1E;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    g_repaint_flip_buffer_after = 0;
    ASSERT_EQ(g_repaint_settings_calls, 1);
    ASSERT_EQ(data_fd2_dialog_blink_phase_oscillator, 1);
    ASSERT_EQ(r, 0x39);
}

/* Same single-body drive as above but oscillator preset to 1 so the +1 hits 2
 * and wraps back to 0 (asm 0x178dd CMP ==2 -> 0x178e6 store 0). Pins the modulo-2
 * wrap that the 0->1 case does not exercise. */
static void test_wait_dialog_repaint_oscillator_1_to_0(void)
{
    int r;
    g_repaint_settings_calls = 0;
    g_repaint_flip_buffer_after = 1;
    data_fd2_dialog_blink_phase_oscillator = 1;
    data_fd2_dialog_blink_phase_oscillator_tick_latch = 0;
    *(volatile uint32 *)0x46CuL = 0x00004000uL;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x1E;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    g_repaint_flip_buffer_after = 0;
    ASSERT_EQ(g_repaint_settings_calls, 1);
    ASSERT_EQ(data_fd2_dialog_blink_phase_oscillator, 0);
    ASSERT_EQ(r, 0x39);
}

/* ---- Tests: wait_ticks_or_keypress ---- */

static void test_wait_ticks_or_keypress_timeout(void)
{
    fd2_wait_ticks_or_keypress_with_palette(0);
    ASSERT_TRUE(1);
}

/* ---- Tests: wait_for_input_v2 ---- */

static void test_wait_input_v2_basic(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_for_input_v2();
    ASSERT_EQ(r, 0x39);
}

/* ---- Tests: wait_for_input_with_idle ---- */

static void test_wait_input_idle_arrow(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x4800;
    r = fd2_wait_for_input_with_idle();
    ASSERT_EQ(r, 0x48);
}

static void test_wait_input_idle_remap_52(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5200;
    r = fd2_wait_for_input_with_idle();
    ASSERT_EQ(r, 0x1c);
}

static void test_wait_input_idle_remap_53(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5300;
    r = fd2_wait_for_input_with_idle();
    ASSERT_EQ(r, 0x01);
}

/* ---- Tests: wait_for_action_target_input ---- */

static void test_wait_action_target_esc(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x011B;
    r = fd2_wait_for_action_target_input(4, 0, 0);
    ASSERT_EQ(r, -1);
}

static void test_wait_action_target_mode4_commit(void)
{
    int r;
    uint8 fake_tile_map[64];
    memset(fake_tile_map, 0, sizeof(fake_tile_map));
    fake_tile_map[7] = 0x00;
    data_fd2_battle_tile_map_ptr = (uint32)fake_tile_map;
    data_fd2_battle_cursor_world_x = 0;
    data_fd2_battle_cursor_world_y = 0;
    data_fd2_battle_map_width_tiles = 4;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x1C0D;
    r = fd2_wait_for_action_target_input(4, 0, 0);
    ASSERT_EQ(r, 1);
}

static void test_wait_action_target_mode5_no_commit(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x22;
    *(volatile uint16 *)0x41EuL = 0x1C0D;
    *(volatile uint16 *)0x420uL = 0x011B;
    r = fd2_wait_for_action_target_input(5, 0, 0);
    ASSERT_EQ(r, -1);
}

void run_ui_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_cursor\n");
    RUN_TEST(test_cursor_move_up_basic);
    RUN_TEST(test_cursor_move_up_at_top);
    RUN_TEST(test_cursor_move_up_scroll);
    RUN_TEST(test_cursor_move_down_basic);
    RUN_TEST(test_cursor_move_down_scroll);
    RUN_TEST(test_cursor_move_down_at_bottom);
    RUN_TEST(test_cursor_move_right_basic);
    RUN_TEST(test_cursor_move_right_scroll);
    RUN_TEST(test_cursor_move_right_at_right_edge);
    RUN_TEST(test_cursor_move_left_basic);
    RUN_TEST(test_cursor_move_left_scroll);
    RUN_TEST(test_cursor_move_left_at_left_edge);
    RUN_TEST(test_cursor_move_left_inner_step_anim);
    RUN_TEST(test_pan_to_char);
    RUN_TEST(test_pan_to_tile_same_pos);
    RUN_TEST(test_pan_to_tile_moves_x);
    RUN_TEST(test_pan_and_window_same_pos);
    RUN_TEST(test_pan_and_window_inc_x_lockstep);
    RUN_TEST(test_pan_and_window_dec_y_lockstep);
    RUN_TEST(test_read_bios_tick);
    RUN_TEST(test_kbd_buffer_empty);
    RUN_TEST(test_kbd_buffer_nonempty);
    RUN_TEST(test_clear_kbd_buffer);
    RUN_TEST(test_wait_one_bios_tick_smoke);
    RUN_TEST(test_wait_one_bios_tick_sign_extend);
    RUN_TEST(test_wait_one_bios_tick_positive_word);
    RUN_TEST(test_wait_n_bios_ticks_sign_extend);
    RUN_TEST(test_wait_n_bios_ticks_positive_word);
    RUN_TEST(test_update_palette_cycle_anim_no_update);
    RUN_TEST(test_get_inventory_slot_item_id);
    RUN_TEST(test_read_tile_attribute);
    RUN_TEST(test_read_tile_attribute_sprite_mask);
    RUN_TEST(test_set_vga_palette_range_basic);
    RUN_TEST(test_set_vga_palette_range_with_add);
    RUN_TEST(test_palette_fade_to_black);
    RUN_TEST(test_palette_remap_run);
    RUN_TEST(test_interpolate_palette);
    RUN_TEST(test_wait_dialog_blink_esc);
    RUN_TEST(test_wait_dialog_repaint_remap_52);
    RUN_TEST(test_wait_dialog_repaint_remap_e0);
    RUN_TEST(test_wait_dialog_repaint_remap_53);
    RUN_TEST(test_wait_dialog_repaint_passthrough);
    RUN_TEST(test_wait_dialog_repaint_oscillator_0_to_1);
    RUN_TEST(test_wait_dialog_repaint_oscillator_1_to_0);
    RUN_TEST(test_wait_ticks_or_keypress_timeout);
    RUN_TEST(test_wait_input_v2_basic);
    RUN_TEST(test_wait_input_idle_arrow);
    RUN_TEST(test_wait_input_idle_remap_52);
    RUN_TEST(test_wait_input_idle_remap_53);
    RUN_TEST(test_wait_action_target_esc);
    RUN_TEST(test_wait_action_target_mode4_commit);
    RUN_TEST(test_wait_action_target_mode5_no_commit);
    printf("\n");
}
