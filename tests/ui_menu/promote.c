/*
 * unit tests for src/ui_menu/promote.c
 *
 * fd2_build_dead_chars_list_for_revive(out) walks party slots
 * 0..menu_party_member_count-1, and for each slot whose
 * fd2_check_char_is_dead() returns 1 appends the slot index (as a
 * byte) to out, returning the count of dead chars.
 *
 * These tests drive the real per-char dead semantics by enabling
 * the testglob stub's array mode (g_check_char_is_dead_use_array),
 * which makes fd2_check_char_is_dead read
 * runtime_char[idx].flags & 1 from the shared g_test_rc_array
 * fixture — exactly what the real callee does.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* shared runtime_char fixture + the dead-check stub controls (testglob.c) */
extern runtime_char g_test_rc_array[8];
extern int g_check_char_is_dead_return;
extern int g_check_char_is_dead_use_array;

/* Seed the fixture: party of `n` members, those whose index is set in
 * dead_mask get flags bit0 (dead); all others cleared (alive). Other
 * flag bits are deliberately set on a couple of slots to prove the
 * dead test isolates bit0. */
static void seed_party(int n, unsigned dead_mask)
{
    int i;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (i = 0; i < n; i++) {
        if (dead_mask & (1u << i))
            g_test_rc_array[i].flags = 0x05;   /* bit0 dead + bit2 cannot_act */
        else
            g_test_rc_array[i].flags = 0x06;   /* bit2 set, bit0 clear -> alive */
    }
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_shared_menu_party_member_count = (uint32)n;
    g_check_char_is_dead_use_array = 1;
}

/* ---- Test: mixed pattern -> dead indices appended in order ---- */
static void test_mixed_dead_pattern(void)
{
    uint8 out[8];
    int count;

    memset(out, 0xAA, sizeof(out));      /* canary */
    seed_party(6, (1u << 1) | (1u << 3) | (1u << 4));  /* dead: 1,3,4 */

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 3);
    ASSERT_EQ(out[0], 1);
    ASSERT_EQ(out[1], 3);
    ASSERT_EQ(out[2], 4);
    ASSERT_EQ(out[3], 0xAA);             /* nothing written past count */
}

/* ---- Test: all alive -> count 0, buffer untouched ---- */
static void test_all_alive(void)
{
    uint8 out[8];
    int count;

    memset(out, 0xAA, sizeof(out));
    seed_party(5, 0u);                   /* none dead */

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 0);
    ASSERT_EQ(out[0], 0xAA);             /* no write at all */
}

/* ---- Test: all dead -> every index appended in order ---- */
static void test_all_dead(void)
{
    uint8 out[8];
    int count;

    seed_party(4, 0xFu);                 /* 0,1,2,3 all dead */

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 4);
    ASSERT_EQ(out[0], 0);
    ASSERT_EQ(out[1], 1);
    ASSERT_EQ(out[2], 2);
    ASSERT_EQ(out[3], 3);
}

/* ---- Test: empty party -> loop never runs, returns 0 ---- */
static void test_empty_party(void)
{
    uint8 out[8];
    int count;

    memset(out, 0xAA, sizeof(out));
    seed_party(0, 0u);                   /* member_count = 0 */

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 0);
    ASSERT_EQ(out[0], 0xAA);
}

/* ---- Test: bit0 isolation — flags with bit0 clear are NOT dead even
 *           when other bits are set; the trailing dead char is the only
 *           one appended. Proves the (& 1) mask + (== 1) compare chain. */
static void test_bit0_isolation(void)
{
    uint8 out[8];
    int count;

    memset(out, 0xAA, sizeof(out));
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0xFE;     /* every bit but bit0 -> alive */
    g_test_rc_array[1].flags = 0x80;     /* bit7 acted, bit0 clear -> alive */
    g_test_rc_array[2].flags = 0x01;     /* bit0 only -> dead */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_shared_menu_party_member_count = 3;
    g_check_char_is_dead_use_array = 1;

    count = fd2_build_dead_chars_list_for_revive(out);

    ASSERT_EQ(count, 1);
    ASSERT_EQ(out[0], 2);
    ASSERT_EQ(out[1], 0xAA);
}

void run_ui_menu_promote_tests(void)
{
    SUITE_BEGIN(ui_menu_promote);
    RUN_TEST(test_mixed_dead_pattern);
    RUN_TEST(test_all_alive);
    RUN_TEST(test_all_dead);
    RUN_TEST(test_empty_party);
    RUN_TEST(test_bit0_isolation);
    /* restore stub default so later suites keep historical behavior */
    g_check_char_is_dead_use_array = 0;
    g_check_char_is_dead_return = 0;
    SUITE_END();
}
