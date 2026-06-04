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

/* fd2_animate_attack_hit_sequence call counter (testglob.c stub): driven once
 * per landed hit by fd2_animate_combat_hit_with_hp_drain. */
extern int    g_attack_hit_seq_calls;

/* raw-blit recording log (testglob.c): the real fd2_render_combat_hp_bar_segments
 * forwards each segment blit's (dst, sprite_addr) here when g_blitraw_log_on. */
extern int    g_blitraw_log_on;
extern int    g_blitraw_count;
extern uint32 g_blitraw_log_dst[512];
extern uint32 g_blitraw_log_sprite[512];

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

/* ================================================================
 * fd2_show_miss_indicator tests (sibling producer for the projectile-paths
 * consumer; enqueues the 4-sprite "MISS" indicator)
 * ================================================================ */

/* the 4 miss-indicator sprite ids (testglob.c, real binary bytes @ 0x5204A) */
extern uint8 data_fd2_battle_miss_indicator_sprite_ids[4];

/*
 * An in-window target: the producer appends exactly four queue slots, one per
 * indicator sprite. Verifies the distinctive irregular x-offset row
 * ((char_iter==1) ? 8 : char_iter*5+2 -> 2, 8, 12, 17 -- note slot 1 lands on 8
 * instead of the 7 a plain *5+2 would give), the target-index fan-out across
 * all four slots, the sprite-id fan-out straight from the indicator table
 * (no thresholding, unlike the damage-number sibling), and the queue-count
 * advance by 4.
 */
static void test_miss_indicator_enqueue(void)
{
    uint32 base;

    setup_damagenum();
    base = data_fd2_battle_spell_aoe_count_and_fx_queue_idx;   /* 0 */

    g_test_rc_array[2].pos_x = 0x15;     /* inside the window */
    g_test_rc_array[2].pos_y = 0x24;

    fd2_show_miss_indicator(2);

    /* queue count advanced by 4 */
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, base + 4);

    /* irregular x-offset row: 0*5+2=2, slot1 forced to 8, 2*5+2=12, 3*5+2=17 */
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 0], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 1], 8u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 2], 12u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 3], 17u);

    /* every slot carries the target index */
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 0], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 1], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 2], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 3], 2u);

    /* sprite ids copied straight from the indicator table, one per slot */
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 0],
              data_fd2_battle_miss_indicator_sprite_ids[0]);
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 1],
              data_fd2_battle_miss_indicator_sprite_ids[1]);
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 2],
              data_fd2_battle_miss_indicator_sprite_ids[2]);
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 3],
              data_fd2_battle_miss_indicator_sprite_ids[3]);
}

/*
 * A non-zero starting queue index: every write must land at base+slot, not at
 * slot 0, and the count advances from the non-zero base. Confirms the base
 * offset threading for all three queue tables.
 */
static void test_miss_indicator_offset_base(void)
{
    uint32 base;

    setup_damagenum();
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 8;      /* non-zero base */
    base = 8;

    g_test_rc_array[0].pos_x = WIN_OX;   /* on the left window edge (in window) */
    g_test_rc_array[0].pos_y = WIN_OY;
    fd2_show_miss_indicator(0);

    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, base + 4);

    /* writes land at base..base+3 */
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 0], 2u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 1], 8u);
    ASSERT_EQ(data_fd2_battle_floating_damage_x_offset_queue[base + 3], 17u);
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 0],
              data_fd2_battle_miss_indicator_sprite_ids[0]);
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base + 3],
              data_fd2_battle_miss_indicator_sprite_ids[3]);
    ASSERT_EQ(data_fd2_battle_floating_damage_target_char_idx_queue[base + 0], 0u);

    /* slot just before the base was untouched (still the 0xEE sentinel) */
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[base - 1], 0xEEu);
}

/*
 * Out-of-window target: nothing is enqueued and the queue count is untouched.
 * Drives all four cull edges (one per call). The predicate is identical to the
 * fd2_show_damage_number sibling: x uses an inclusive reject at OX-1 and at
 * OX+MX (OX+MX itself is OUT); y uses an inclusive reject below OY-1 and above
 * OY+MY (no +1 on the y upper edge).
 */
static void test_miss_indicator_cull_all_edges(void)
{
    setup_damagenum();

    /* left: pos_x == OX-1 -> rejected (x <= OX-1) */
    g_test_rc_array[0].pos_x = (uint8)(WIN_OX - 1);
    g_test_rc_array[0].pos_y = WIN_OY;
    fd2_show_miss_indicator(0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0u);

    /* right: pos_x == OX+MX -> rejected (x >= OX+MX) */
    g_test_rc_array[0].pos_x = (uint8)(WIN_OX + WIN_MX);
    g_test_rc_array[0].pos_y = WIN_OY;
    fd2_show_miss_indicator(0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0u);

    /* top: pos_y == OY-2 -> rejected (y < OY-1) */
    g_test_rc_array[0].pos_x = WIN_OX;
    g_test_rc_array[0].pos_y = (uint8)(WIN_OY - 2);
    fd2_show_miss_indicator(0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0u);

    /* bottom: pos_y == OY+MY+1 -> rejected (y > OY+MY) */
    g_test_rc_array[0].pos_x = WIN_OX;
    g_test_rc_array[0].pos_y = (uint8)(WIN_OY + WIN_MY + 1);
    fd2_show_miss_indicator(0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 0u);

    /* the queue slots were never written (still the 0xEE sentinel) */
    ASSERT_EQ(data_fd2_battle_floating_damage_sprite_id_queue[0], 0xEEu);

    /* an in-window target on the inclusive accept extremes IS enqueued:
     * pos_x = OX+MX-1 (last in-window column) and pos_y = OY+MY (last in-window
     * row, since there is no +1 on the y upper edge) */
    g_test_rc_array[0].pos_x = (uint8)(WIN_OX + WIN_MX - 1);  /* 0x1C, in window */
    g_test_rc_array[0].pos_y = (uint8)(WIN_OY + WIN_MY);      /* 0x28, accepted */
    fd2_show_miss_indicator(0);
    ASSERT_EQ(data_fd2_battle_spell_aoe_count_and_fx_queue_idx, 4u);
}

/* ================================================================
 * fd2_animate_combat_hit_with_hp_drain tests
 *
 * This is the melee-hit orchestrator: it resolves the attacker's weapon,
 * decides the hit budget (1, or 2 for a double-strike / a 3% RNG proc),
 * then per hit drives the REAL fd2_execute_attack_damage_calculation, the
 * stubbed fd2_animate_attack_hit_sequence (call-counted), and the REAL
 * fd2_render_combat_hp_bar_segments bar drain (observed through the raw-blit
 * log + the delay-thunk counter). The damage calc, item table and RNG seed
 * are wired exactly like the battle.c eatk_* tests.
 * ================================================================ */

/* UI/anim sheet so the real fd2_render_combat_hp_bar_segments ->
 * fd2_blit_sheet_sprite_at_offset can resolve sprite 0x17..0x1e without
 * faulting. Offset-table entry i == i (sprite index recoverable), but only
 * the dst of the first segment blit is needed here. */
static uint8 g_hpdrain_ui_sheet[6 + 0x20 * 4];

static void hpdrain_install_ui_sheet(void)
{
    int i;
    memset(g_hpdrain_ui_sheet, 0, sizeof(g_hpdrain_ui_sheet));
    for (i = 0; i <= 0x1e; i++) {
        *(int32 *)(g_hpdrain_ui_sheet + 6 + i * 4) = i;
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_hpdrain_ui_sheet;
}

/* Common reset mirroring battle.c eatk_reset: attacker (char 0) holds an
 * equipped weapon in slot 0 -> the real find_equipped/get_item chain returns
 * item id 0 -> weapon_entry = item_effect_table[0], which each test tunes. */
static void hpdrain_reset(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    memset(data_fd2_battle_enemy_data_table, 0,
           sizeof(data_fd2_battle_enemy_data_table));
    memset(data_fd2_battle_job_crit_rate_table, 0,
           sizeof(data_fd2_battle_job_crit_rate_table));
    g_test_rc_array[0].inventory_slots[0] = 0x40;   /* equipped flag */
    g_test_rc_array[0].inventory_slots[1] = 0;      /* weapon item id 0 */
    data_fd2_battle_pending_xp_credit = 0;
    data_fd2_battle_last_hit_or_miss_flag = 1;
    g_attack_hit_seq_calls = 0;
    g_delay375b2_calls = 0;
    g_delay375b2_last_ticks = 0;
    g_blitraw_log_on = 0;
    g_blitraw_count = 0;
    hpdrain_install_ui_sheet();
}

/* Death-on-first-hit. Defender at FULL HP (100/100) takes a lethal hit
 * (AP 200, DP 0 -> base damage (200*9)/10 = 180 >= 100), so surviving_HP == 0
 * and the outer do/while exits after one pass regardless of the hit budget.
 *
 * Pins the bar-drain math + the destination formula deterministically:
 *   - pre-hit bar length = hp_current(100) * 0x46 / hp_max(100) = 70 px
 *   - post-hit floor     = surviving(0) * 0x45 / hp_max + 1     = 1 px
 *   - the inner loop renders bar_pixels = 70,69,...,1  -> 70 frames, each
 *     followed by __delay_thunk_375b2(8)
 *   - dst = (panel_y(4)+6)*0x140 + panel_x(8) + 0xA0007 = 0xA0C8F, surfaced as
 *     the dst of the first segment blit (the 0x17 left cap).
 * Also confirms the defender HP was clamped to 0 and the return value is 0. */
static void test_hpdrain_death_full_bar(void)
{
    int panel_xy[2];
    int result;

    hpdrain_reset();
    /* attacker (0): hit, no crit, lethal */
    g_test_rc_array[0].team = 1;          /* skip XP block */
    g_test_rc_array[0].job_id = 0x13;     /* immune -> skip terrain */
    g_test_rc_array[0].ap = 200;
    g_test_rc_array[0].dx_current = 100;
    /* defender (1): full HP, dx_diff = 100 -> always HIT */
    g_test_rc_array[1].job_id = 0x13;
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].dp = 0;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].hp_max = 100;
    /* item 0: special_type(+10 = entry[+9]) = 0 -> no double-strike weapon */
    data_fd2_battle_item_effect_table[0].special_type = 0;
    data_fd2_shared_rng_seed = 0;         /* draw0 %100 = 32 -> no RNG proc */

    panel_xy[0] = 8;
    panel_xy[1] = 4;
    g_blitraw_log_on = 1;
    result = fd2_animate_combat_hit_with_hp_drain(0, 1, (uint32)panel_xy);
    g_blitraw_log_on = 0;

    ASSERT_EQ(result, 0);                              /* defender died */
    ASSERT_EQ(g_test_rc_array[1].hp_current, 0);       /* HP clamped */
    ASSERT_EQ(g_attack_hit_seq_calls, 1);              /* one hit played */
    ASSERT_EQ(g_delay375b2_calls, 70);                 /* drain 70 -> 1 */
    ASSERT_EQ(g_delay375b2_last_ticks, 8u);
    ASSERT_TRUE(g_blitraw_count > 0);
    /* first segment blit (0x17 left cap) sits at the computed dst */
    ASSERT_EQ(g_blitraw_log_dst[0], 0xA0C8Fu);
}

/* Death-on-first-hit, HALF bar -> pins that the pre-hit bar length uses the
 * defender's hp_current/hp_max read BEFORE the damage calc (not the cleared
 * post-hit value). hp 50/100 -> 50*0x46/100 = 35 px; surviving 0 -> floor 1;
 * drain 35,34,...,1 -> 35 frames. A bug reading hp AFTER the hit (0/100)
 * would give 0 frames. */
static void test_hpdrain_death_half_bar(void)
{
    int panel_xy[2];
    int result;

    hpdrain_reset();
    g_test_rc_array[0].team = 1;
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].ap = 200;
    g_test_rc_array[0].dx_current = 100;
    g_test_rc_array[1].job_id = 0x13;
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].dp = 0;
    g_test_rc_array[1].hp_current = 50;    /* half */
    g_test_rc_array[1].hp_max = 100;
    data_fd2_battle_item_effect_table[0].special_type = 0;
    data_fd2_shared_rng_seed = 0;

    panel_xy[0] = 0;
    panel_xy[1] = 0;
    result = fd2_animate_combat_hit_with_hp_drain(0, 1, (uint32)panel_xy);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(g_delay375b2_calls, 35);     /* 50*70/100 = 35 down to 1 */
}

/* RNG-proc double-strike (the EAX-bug fix). Every hit is a guaranteed MISS
 * (dx_diff = 0 -> draw %100 < 0 is never true), so the defender survives and
 * the do/while runs the FULL hit budget; fd2_animate_attack_hit_sequence is
 * called once per budgeted hit, counted by g_attack_hit_seq_calls.
 *
 * special_type(entry[+9]) = 50, so the buggy decompiled form
 * (weapon_class %100 < 3 -> 50 < 3 == false) would NEVER upgrade. The
 * faithful form divides the fd2_advance_rng_state() RETURN value: seed 21 ->
 * draw 0x814C (33100), %100 = 0 < 3 -> budget raised to 2. Observing exactly
 * 2 hits proves the proc is driven by the RNG draw, not the weapon byte. */
static void test_hpdrain_rng_double_strike(void)
{
    int panel_xy[2];
    int result;

    hpdrain_reset();
    g_test_rc_array[0].team = 1;
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].ap = 0;
    g_test_rc_array[0].dx_current = 0;     /* dx_diff = 0 -> always MISS */
    g_test_rc_array[1].job_id = 0x13;
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].hp_max = 100;
    /* special_type 50: buggy `weapon_class %100 < 3` would be false */
    data_fd2_battle_item_effect_table[0].special_type = 50;
    data_fd2_shared_rng_seed = 21;         /* draw0 = 33100, %100 = 0 < 3 */

    panel_xy[0] = 0;
    panel_xy[1] = 0;
    result = fd2_animate_combat_hit_with_hp_drain(0, 1, (uint32)panel_xy);

    ASSERT_EQ(g_attack_hit_seq_calls, 2);  /* RNG proc -> two hits */
    ASSERT_EQ(result, 100);                /* all misses -> defender survives */
    ASSERT_EQ(g_test_rc_array[1].hp_current, 100);
}

/* Control for the proc test: NO double-strike weapon and NO RNG proc.
 * special_type = 50 (buggy form would upgrade), but seed 0 -> draw0 %100 = 32
 * (>= 3) so the faithful form leaves the budget at 1. All misses -> defender
 * survives -> the do/while stops after one budgeted hit. Exactly 1 hit proves
 * the upgrade did NOT fire off the weapon byte (which is 50). */
static void test_hpdrain_single_hit_no_proc(void)
{
    int panel_xy[2];
    int result;

    hpdrain_reset();
    g_test_rc_array[0].team = 1;
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].ap = 0;
    g_test_rc_array[0].dx_current = 0;     /* always MISS */
    g_test_rc_array[1].job_id = 0x13;
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].hp_max = 100;
    data_fd2_battle_item_effect_table[0].special_type = 50;
    data_fd2_shared_rng_seed = 0;          /* draw0 %100 = 32 -> no proc */

    panel_xy[0] = 0;
    panel_xy[1] = 0;
    result = fd2_animate_combat_hit_with_hp_drain(0, 1, (uint32)panel_xy);

    ASSERT_EQ(g_attack_hit_seq_calls, 1);  /* single hit */
    ASSERT_EQ(result, 100);                /* survived */
}

/* Double-strike WEAPON class (special_type == 3) takes the budget to 2 with
 * NO RNG help: seed 0 -> draw0 %100 = 32 (no proc). All misses -> two hits.
 * Pins the `weapon_entry[+9] == 3` branch. */
static void test_hpdrain_weapon_double_strike(void)
{
    int panel_xy[2];
    int result;

    hpdrain_reset();
    g_test_rc_array[0].team = 1;
    g_test_rc_array[0].job_id = 0x13;
    g_test_rc_array[0].ap = 0;
    g_test_rc_array[0].dx_current = 0;
    g_test_rc_array[1].job_id = 0x13;
    g_test_rc_array[1].stat4_current = 0;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].hp_max = 100;
    data_fd2_battle_item_effect_table[0].special_type = 3;  /* double weapon */
    data_fd2_shared_rng_seed = 0;          /* no RNG proc */

    panel_xy[0] = 0;
    panel_xy[1] = 0;
    result = fd2_animate_combat_hit_with_hp_drain(0, 1, (uint32)panel_xy);

    ASSERT_EQ(g_attack_hit_seq_calls, 2);  /* weapon class -> two hits */
    ASSERT_EQ(result, 100);
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
    RUN_TEST(test_miss_indicator_enqueue);
    RUN_TEST(test_miss_indicator_offset_base);
    RUN_TEST(test_miss_indicator_cull_all_edges);
    RUN_TEST(test_hpdrain_death_full_bar);
    RUN_TEST(test_hpdrain_death_half_bar);
    RUN_TEST(test_hpdrain_rng_double_strike);
    RUN_TEST(test_hpdrain_single_hit_no_proc);
    RUN_TEST(test_hpdrain_weapon_double_strike);
    printf("\n");
}
