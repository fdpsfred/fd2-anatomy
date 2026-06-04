/*
 * unit tests for src/spell/spellsel.c
 *
 * fd2_build_usable_spell_list(char_idx, out_buf): enumerate the spell ids the
 * runtime char at char_idx has learned (5-byte spells_known_bitmap, +0x1A,
 * 40 slots), returning the count and -- when out_buf != NULL -- writing each
 * id = byte*8 + bit in ascending order.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* Local runtime-char fixture; the global array pointer is repointed at it for
 * the duration of each test and restored afterwards (no cross-suite pollution
 * of the shared g_test_rc_array). */
static runtime_char bsl_chars[8];
static runtime_char *bsl_saved_ptr;

static void bsl_setup(void)
{
    bsl_saved_ptr = data_fd2_battle_runtime_char_array_ptr;
    memset(bsl_chars, 0, sizeof(bsl_chars));
    data_fd2_battle_runtime_char_array_ptr = bsl_chars;
}

static void bsl_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = bsl_saved_ptr;
}

/* Empty bitmap -> count 0, and out_buf is not touched. */
static void test_bsl_empty(void)
{
    uint8 out[40];
    int n;

    bsl_setup();
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 0);
    ASSERT_EQ((int)out[0], 0xCC);            /* untouched */
    bsl_teardown();
}

/* Count-only mode (out_buf == NULL): returns the count without writing. Three
 * bits set in byte 0 -> count 3. */
static void test_bsl_count_only(void)
{
    int n;

    bsl_setup();
    bsl_chars[0].spells_known_bitmap[0] = 0x07;   /* bits 0,1,2 */
    n = fd2_build_usable_spell_list(0, 0);
    ASSERT_EQ(n, 3);
    bsl_teardown();
}

/* Write mode, byte 0: ids equal the bit index. bits 0,2,5 set ->
 * ids {0,2,5}, count 3, ascending order. */
static void test_bsl_byte0_ids(void)
{
    uint8 out[40];
    int n;

    bsl_setup();
    bsl_chars[0].spells_known_bitmap[0] = 0x25;   /* bits 0,2,5 (0x01|0x04|0x20) */
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 3);
    ASSERT_EQ((int)out[0], 0);
    ASSERT_EQ((int)out[1], 2);
    ASSERT_EQ((int)out[2], 5);
    ASSERT_EQ((int)out[3], 0xCC);            /* nothing written past count */
    bsl_teardown();
}

/* Bit-position math across bytes: id = byte*8 + bit. byte 1 bit 3 -> 11,
 * byte 3 bit 0 -> 24, byte 4 bit 7 -> 39 (the highest reachable slot). The
 * three set bytes also prove every one of the 5 bytes is scanned. */
static void test_bsl_cross_byte_ids(void)
{
    uint8 out[40];
    int n;

    bsl_setup();
    bsl_chars[0].spells_known_bitmap[1] = 1 << 3;  /* id 1*8+3 = 11 */
    bsl_chars[0].spells_known_bitmap[3] = 1 << 0;  /* id 3*8+0 = 24 */
    bsl_chars[0].spells_known_bitmap[4] = 1 << 7;  /* id 4*8+7 = 39 */
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 3);
    ASSERT_EQ((int)out[0], 11);
    ASSERT_EQ((int)out[1], 24);
    ASSERT_EQ((int)out[2], 39);
    bsl_teardown();
}

/* Loop bound is exactly 5 bytes: a byte-5 (offset +0x1F = archetype_flag) all
 * ones must NOT be enumerated. Set every spells_known_bitmap bit (5 bytes =
 * 40 ids 0..39) and fully populate the adjacent archetype_flag byte; the count
 * stays 40 and the last id is 39. */
static void test_bsl_loop_bound_five_bytes(void)
{
    uint8 out[64];
    int n;
    int i;

    bsl_setup();
    for (i = 0; i < 5; i++) {
        bsl_chars[0].spells_known_bitmap[i] = 0xFF;
    }
    bsl_chars[0].archetype_flag = 0xFF;          /* +0x1F, must be ignored */
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 40);
    ASSERT_EQ((int)out[0], 0);
    ASSERT_EQ((int)out[39], 39);                 /* 4*8+7, last real slot */
    ASSERT_EQ((int)out[40], 0xCC);               /* no 41st id from byte 5 */
    bsl_teardown();
}

/* char_idx selects the right struct (stride 0x50): index 0 is empty, index 3
 * carries the spells, querying 3 returns its list and querying 0 returns 0. */
static void test_bsl_char_index_stride(void)
{
    uint8 out[40];
    int n;

    bsl_setup();
    bsl_chars[3].spells_known_bitmap[0] = 0x02;   /* bit 1 -> id 1 */
    bsl_chars[3].spells_known_bitmap[2] = 0x01;   /* bit 0 -> id 16 */

    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(3, (uint32)out);
    ASSERT_EQ(n, 2);
    ASSERT_EQ((int)out[0], 1);
    ASSERT_EQ((int)out[1], 16);

    n = fd2_build_usable_spell_list(0, 0);        /* index 0 still empty */
    ASSERT_EQ(n, 0);
    bsl_teardown();
}

void run_spell_spellsel_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: spell/spellsel\n");
    RUN_TEST(test_bsl_empty);
    RUN_TEST(test_bsl_count_only);
    RUN_TEST(test_bsl_byte0_ids);
    RUN_TEST(test_bsl_cross_byte_ids);
    RUN_TEST(test_bsl_loop_bound_five_bytes);
    RUN_TEST(test_bsl_char_index_stride);
    printf("\n");
}
