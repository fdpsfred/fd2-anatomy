/*
 * unit tests for src/anim/anicombt.c -- part 2
 *
 * The floating-damage FX queue pair: the producer (fd2_show_damage_number)
 * and its consumer (fd2_animate_spell_projectile_paths). Split out of part 1
 * when the leaf passed 1000 lines.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* runtime-char array backing (testglob.c) */
extern runtime_char g_test_rc_array[8];

/* Battle back-buffer backing. The real per-frame restore memmoves 0x25680 bytes
 * out of data_fd2_large_game_state_buffer_ptr and the real fd2_blit_rectangle
 * reads from +0x8088, so the backing must span the whole 0x25680 snapshot. */
#define LGS_SPAN 0x26000u
static uint8 g_lgs[LGS_SPAN];

#define WIN_OX  0x10u
#define WIN_OY  0x20u
#define WIN_MX  0x0Du   /* x cull keep: OX-1 < x < OX+MX        = [0x10, 0x1C] */
#define WIN_MY  0x08u   /* y cull keep (producer): OY-1 <= y <= OY+MY = [0x1F, 0x28]
                           (the projectile consumer below does not cull) */

/* ================================================================
 * fd2_animate_spell_projectile_paths tests
 * ================================================================ */

/* decoded-pixel sprite blit recording (testglob.c): last-call src/dst/stride and
 * a call counter, plus the opt-in per-call log. */
extern uint32 g_blitdec_dst, g_blitdec_sprite, g_blitdec_stride;
extern int    g_blitdec_calls;
extern int    g_blitdec_log_on;
extern int    g_blitdec_log_count;
extern uint32 g_blitdec_log_dst[16];
extern uint32 g_blitdec_log_sprite[16];

/* delay thunk recording (testglob.c) */
extern int    g_delay375b2_calls;
extern uint32 g_delay375b2_last_ticks;

/* floating-damage FX queue tables (testglob.c, BSS) */
extern uint8  data_fd2_battle_floating_damage_sprite_id_queue[200];
extern uint8  data_fd2_battle_floating_damage_x_offset_queue[200];
extern uint8  data_fd2_battle_floating_damage_target_char_idx_queue[200];
/* projectile y-offset table (testglob.c, real binary bytes) */
extern uint8  data_fd2_animation_spell_projectile_y_offset_table[28];

/* Effect sprite sheet for the projectile blit: a dword table at +6 indexed by
 * sprite_id; entry[i] == i*0x10 so the resolved sprite addr (sheet + table[6 +
 * sprite_id*4]) uniquely identifies the sprite_id. */
static uint8 g_proj_sheet[6 + 256 * 4];

static void setup_projectile(void)
{
    int i;
    uint32 *tbl;

    g_blitdec_calls = 0;
    g_blitdec_dst = 0;
    g_blitdec_sprite = 0;
    g_blitdec_stride = 0;
    g_blitdec_log_on = 0;
    g_blitdec_log_count = 0;
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;

    /* the real per-frame restore memmoves 0x25680 out of the snapshot back into
     * this buffer, and the real fd2_blit_rectangle reads from +0x8088 */
    memset(g_lgs, 0, sizeof(g_lgs));
    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgs;

    tbl = (uint32 *)(g_proj_sheet + 6);
    for (i = 0; i < 256; i++) {
        tbl[i] = (uint32)i * 0x10u;
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_proj_sheet;

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_floating_damage_sprite_id_queue, 0, 200);
    memset(data_fd2_battle_floating_damage_x_offset_queue, 0, 200);
    memset(data_fd2_battle_floating_damage_target_char_idx_queue, 0, 200);
}

/*
 * Gate: a zero FX queue count takes the immediate-return path before any
 * malloc / snapshot / frame loop runs, so no blit and no delay happen.
 */
static void test_projectile_zero_queue_gate(void)
{
    setup_projectile();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;

    /* seed a stale slot that must NOT be drawn (count gates it out) */
    data_fd2_battle_floating_damage_sprite_id_queue[0] = 5;
    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;

    fd2_animate_spell_projectile_paths();

    ASSERT_EQ(g_blitdec_calls, 0);
    ASSERT_EQ(g_delay375b2_calls, 0);
}

/*
 * Full 22-frame flight with one active FX slot. Verifies:
 *  - one blit per frame -> 22 blits total (sprite_id != 0 every frame);
 *  - the sprite-source arithmetic sheet + sheet[6 + sprite_id*4] (frame-
 *    invariant);
 *  - the per-frame dst arithmetic, including the distinctive y-offset table
 *    progression y_offset_table[fx%4 + frame] across the first 16 frames
 *    (the descending-then-rising rise pattern), the -3 bias, and the
 *    0x2AC0 / 0x18 / 0x1C8 strides plus the +x_offset and +0x8088 base;
 *  - the per-frame delay(2) cadence plus the closing delay(500).
 */
static void test_projectile_full_flight_arithmetic(void)
{
    uint32 sprite_id;
    uint32 x_off;
    uint32 px;
    uint32 py;
    uint32 base;
    uint32 exp_sprite;
    int f;

    setup_projectile();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 1;

    sprite_id = 0x5e;          /* '^' damage-number marker base */
    x_off = 7;
    px = 0x15;
    py = 0x24;

    data_fd2_battle_floating_damage_sprite_id_queue[0] = (uint8)sprite_id;
    data_fd2_battle_floating_damage_x_offset_queue[0] = (uint8)x_off;
    data_fd2_battle_floating_damage_target_char_idx_queue[0] = 0;
    g_test_rc_array[0].pos_x = (uint8)px;
    g_test_rc_array[0].pos_y = (uint8)py;

    g_blitdec_log_on = 1;
    fd2_animate_spell_projectile_paths();

    /* one blit per frame, all 22 frames (slot active every frame) */
    ASSERT_EQ(g_blitdec_calls, 22);
    /* the opt-in log caps at 16 -> first 16 frames captured */
    ASSERT_EQ(g_blitdec_log_count, 16);

    /* frame-invariant sprite source: sheet + table[6 + sprite_id*4],
     * table[k] == k*0x10 with k = sprite_id */
    exp_sprite = (uint32)g_proj_sheet + sprite_id * 0x10u;

    /* dst base shared by every frame (everything except the y-offset term) */
    base = (uint32)g_lgs + 0x8088u
         + (py - WIN_OY) * 0x2ac0u
         + (px - WIN_OX) * 0x18u
         + x_off;

    for (f = 0; f < 16; f++) {
        /* fx_iter == 0 so the table index is just the frame number */
        int yval = (int)data_fd2_animation_spell_projectile_y_offset_table[f];
        uint32 exp_dst = base + (uint32)((yval - 3) * 0x1c8);
        ASSERT_EQ(g_blitdec_log_dst[f], exp_dst);
        ASSERT_EQ(g_blitdec_log_sprite[f], exp_sprite);
    }

    /* the last recorded (frame 21) blit confirms the loop ran to completion:
     * index = 0%4 + 21 = 21 -> y_offset_table[21] = 0x0F */
    {
        int yval21 = (int)data_fd2_animation_spell_projectile_y_offset_table[21];
        uint32 exp_dst21 = base + (uint32)((yval21 - 3) * 0x1c8);
        ASSERT_EQ(g_blitdec_dst, exp_dst21);
        ASSERT_EQ(g_blitdec_sprite, exp_sprite);
        ASSERT_EQ(g_blitdec_stride, 0x1c8u);
    }

    /* 22 per-frame delays of 2 ticks + one closing 500-tick settle */
    ASSERT_EQ(g_delay375b2_calls, 23);
    ASSERT_EQ(g_delay375b2_last_ticks, 500u);
}

/*
 * A queued slot whose sprite_id is 0 (a blank damage digit) is skipped every
 * frame, so it contributes no blits, while an active slot beside it still
 * draws. Confirms the per-slot sprite_id==0 continue and that the frame loop
 * still runs its full 22 passes (and closing delay) around the skip.
 */
static void test_projectile_zero_sprite_id_skip(void)
{
    setup_projectile();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 3;

    /* slot 0: blank (sprite_id 0) -> skipped */
    data_fd2_battle_floating_damage_sprite_id_queue[0] = 0;
    data_fd2_battle_floating_damage_target_char_idx_queue[0] = 0;
    /* slot 1: active */
    data_fd2_battle_floating_damage_sprite_id_queue[1] = 0x60;
    data_fd2_battle_floating_damage_x_offset_queue[1] = 2;
    data_fd2_battle_floating_damage_target_char_idx_queue[1] = 0;
    /* slot 2: blank (sprite_id 0) -> skipped */
    data_fd2_battle_floating_damage_sprite_id_queue[2] = 0;
    data_fd2_battle_floating_damage_target_char_idx_queue[2] = 0;

    g_test_rc_array[0].pos_x = 0x15;
    g_test_rc_array[0].pos_y = 0x24;

    fd2_animate_spell_projectile_paths();

    /* only slot 1 draws: exactly one blit per frame -> 22 over 22 frames */
    ASSERT_EQ(g_blitdec_calls, 22);
    /* last blit resolved from slot 1's sprite_id (0x60): sheet + 0x60*0x10 */
    ASSERT_EQ(g_blitdec_sprite, (uint32)g_proj_sheet + 0x60u * 0x10u);
    /* the loop still completed: closing 500-tick delay fired */
    ASSERT_EQ(g_delay375b2_last_ticks, 500u);
    ASSERT_EQ(g_delay375b2_calls, 23);
}

/* ================================================================
 * fd2_show_damage_number tests (producer for the projectile-paths consumer)
 * ================================================================ */

/* The FX queue tables (sprite_id/x_offset/target_char_idx) and the queue count
 * data_fd2_battle_spell_aoe_count_and_fx_queue_idx are declared in globals.h and
 * backed by testglob.c; the projectile-paths suite above already externs the
 * three queue arrays. */

static void setup_damagenum(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_floating_damage_sprite_id_queue, 0xEE, 200);
    memset(data_fd2_battle_floating_damage_x_offset_queue, 0xEE, 200);
    memset(data_fd2_battle_floating_damage_target_char_idx_queue, 0xEE, 200);

    data_fd2_battle_view_window_origin_x = WIN_OX;
    data_fd2_battle_view_window_origin_y = WIN_OY;
    data_fd2_battle_view_window_max_x = WIN_MX;
    data_fd2_battle_view_window_max_y = WIN_MY;

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
}

/*
 * A 3-digit number (123) over an in-window target with marker '^' (0x5E).
 * The producer appends exactly four queue slots (one per place):
 *   - thousands place: number too short (strlen 3 !> threshold 3) -> blanked (0)
 *   - hundreds/tens/units: shown, sprite_id = 0x5E + digit - '0'.
 * Verifies the per-slot x-offset (digit_iter*5+2), the target index fan-out,
 * the magnitude-threshold blanking of the leading place, the digit->sprite_id
 * mapping for the shown places, and the queue-count advance by 4.
 */
static void test_damagenum_three_digit_attack(void)
{
    uint32 base;

    setup_damagenum();
    base = data_fd2_battle_spell_aoe_count_and_fx_queue_idx;   /* 0 */

    g_test_rc_array[2].pos_x = 0x15;     /* inside the window */
    g_test_rc_array[2].pos_y = 0x24;

    fd2_show_damage_number(123, '^', 2);

    /* queue count advanced by 4 */
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, base + 4);

    /* x-offset row: digit_iter*5 + 2 -> 2, 7, 12, 17 */
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 0], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 1], 7u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 2], 12u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 3], 17u);

    /* every slot carries the target index */
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 0], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 1], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 2], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 3], 2u);

    /* slot 0 = thousands place: strlen("123")=3, threshold 3 -> 3 !> 3 -> blank */
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 0], 0u);
    /* slots 1..3 = hundreds/tens/units: '1','2','3' -> 0x5E + {1,2,3} */
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 1],
              (uint8)('^' + 1));
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 2],
              (uint8)('^' + 2));
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 3],
              (uint8)('^' + 3));
}

/*
 * A full 4-digit number (7204) shows every place (strlen 4 > threshold 3,2,1,0
 * for all four), with marker 'i' (0x69, heal/green). Confirms the digit_pos
 * walk advances across all four formatted characters and the zero digit maps to
 * marker+0. Also exercises a non-zero starting queue index so the base offset
 * threading is checked.
 */
static void test_damagenum_four_digit_heal_offset_base(void)
{
    uint32 base;

    setup_damagenum();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 8;      /* non-zero base */
    base = 8;

    g_test_rc_array[0].pos_x = WIN_OX;   /* on the left window edge (in window) */
    g_test_rc_array[0].pos_y = WIN_OY;
    fd2_show_damage_number(7204, 'i', 0);

    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, base + 4);

    /* all four places shown: '7','2','0','4' -> 0x69 + {7,2,0,4} */
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 0],
              (uint8)('i' + 7));
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 1],
              (uint8)('i' + 2));
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 2],
              (uint8)('i' + 0));
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 3],
              (uint8)('i' + 4));

    /* x-offsets still land at base+slot, not at slot 0 */
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 0], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 3], 17u);
}

/*
 * A single-digit number (5) shows only the units place; the three leading
 * places are blanked (strlen 1 !> thresholds 3,2,1) and only the last
 * (threshold 0, 1 > 0) is shown. Verifies the threshold cascade and that
 * digit_pos stays at 0 until the one shown place consumes numstr[0]. The target
 * sits on the two inclusive accept extremes that differ from the overlays:
 * pos_x = OX+MX-1 (last in-window column, since OX+MX is OUT) and pos_y = OY+MY
 * (last in-window row, since this producer has no +1 on the y upper edge).
 */
static void test_damagenum_single_digit_blanks_leading(void)
{
    uint32 base;

    setup_damagenum();
    base = 0;

    g_test_rc_array[1].pos_x = (uint8)(WIN_OX + WIN_MX - 1);  /* 0x1C, in window */
    g_test_rc_array[1].pos_y = (uint8)(WIN_OY + WIN_MY);      /* 0x28, accepted */
    fd2_show_damage_number(5, '^', 1);

    /* on the inclusive accept extremes -> ACCEPTED; queue count advanced by 4 */
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, base + 4);

    /* first three places blank, units shows '5' */
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 0], 0u);
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 1], 0u);
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 2], 0u);
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 3],
              (uint8)('^' + 5));
}

/*
 * Out-of-window target: nothing is enqueued and the queue count is untouched.
 * Drives all four cull edges (one per call), including the boundary that the
 * sibling overlays treat differently:
 *   - x <= OX-1            (left, inclusive reject)
 *   - x >= OX+MX           (right, inclusive reject -- note: OX+MX itself is OUT)
 *   - y <  OY-1            (top)
 *   - y >  OY+MY           (bottom -- note: NO +1 here, unlike the overlays)
 */
static void test_damagenum_cull_all_edges(void)
{
    setup_damagenum();

    /* left: pos_x == OX-1 -> rejected (x <= OX-1) */
    g_test_rc_array[0].pos_x = (uint8)(WIN_OX - 1);
    g_test_rc_array[0].pos_y = WIN_OY;
    fd2_show_damage_number(99, '^', 0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0u);

    /* right: pos_x == OX+MX -> rejected (x >= OX+MX) */
    g_test_rc_array[0].pos_x = (uint8)(WIN_OX + WIN_MX);
    g_test_rc_array[0].pos_y = WIN_OY;
    fd2_show_damage_number(99, '^', 0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0u);

    /* top: pos_y == OY-2 -> rejected (y < OY-1) */
    g_test_rc_array[0].pos_x = WIN_OX;
    g_test_rc_array[0].pos_y = (uint8)(WIN_OY - 2);
    fd2_show_damage_number(99, '^', 0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0u);

    /* bottom: pos_y == OY+MY+1 -> rejected (y > OY+MY; the overlays would keep
       this row, but this producer's cull has no +1 on the y upper edge) */
    g_test_rc_array[0].pos_x = WIN_OX;
    g_test_rc_array[0].pos_y = (uint8)(WIN_OY + WIN_MY + 1);
    fd2_show_damage_number(99, '^', 0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0u);

    /* the queue slots were never written (still the 0xEE sentinel) */
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[0], 0xEEu);
}

void run_anim_anicombt2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anicombt (2)\n");
    RUN_TEST(test_projectile_zero_queue_gate);
    RUN_TEST(test_projectile_full_flight_arithmetic);
    RUN_TEST(test_projectile_zero_sprite_id_skip);
    RUN_TEST(test_damagenum_three_digit_attack);
    RUN_TEST(test_damagenum_four_digit_heal_offset_base);
    RUN_TEST(test_damagenum_single_digit_blanks_leading);
    RUN_TEST(test_damagenum_cull_all_edges);
    printf("\n");
}
