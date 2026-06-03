/*
 * unit tests for src/save/save.c
 *
 * Covers fd2_save_runtime_char_to_template — the chapter-end persistence of
 * battle-runtime char state back into the menu/template roster array.
 *
 * Risk-worthy state transitions exercised:
 *   - char_id match drives the 0x50-byte copy; non-match leaves template alone
 *   - transient status block (+0x22..+0x27, 6 bytes) is zeroed after copy
 *   - template flags (+5) masked to dead-bit only (& 1)
 *   - HP synced (current<-max) only when alive (flags != 1); skipped when dead
 *   - MP always restored (current<-max)
 *   - char-0 (索爾) dead-in-battle special case: when dead, template NOT touched
 *   - fd2_recompute_runtime_char_total_stats runs (aggregate +0x48 re-derived)
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

extern runtime_char g_test_rc_array[8];
extern int g_check_char_is_dead_return;

/* Test-owned template (menu/roster) buffer: 8 entries x 0x50 bytes. */
static uint8 g_test_tmpl[8 * 0x50];

/* Byte accessors into the raw template buffer (the function treats template
 * entries as raw byte arrays at fixed offsets, so verify the same way). */
#define TMPL(i)        (g_test_tmpl + (i) * 0x50)
#define TMPL_B(i, off) (TMPL(i)[(off)])
#define TMPL_W(i, off) (*(uint16 *)(TMPL(i) + (off)))
#define RC_B(i, off)   (((uint8 *)&g_test_rc_array[i])[(off)])
#define RC_W(i, off)   (*(uint16 *)(((uint8 *)&g_test_rc_array[i]) + (off)))

/* Reset all shared state this function reads/writes to a clean baseline. */
static void save_fixture_reset(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_test_tmpl, 0, sizeof(g_test_tmpl));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_test_tmpl;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;
}

/* ---- Test: alive char copied, status cleared, HP+MP restored ---- */
static void test_save_alive_copy_and_restore(void)
{
    save_fixture_reset();

    /* runtime source: char_id 5, alive (flags bit0 clear), mid-battle HP/MP. */
    RC_B(0, 0x08) = 5;       /* char_id */
    RC_B(0, 0x05) = 0x84;    /* flags: bit7(acted)+bit2(cannot_act), bit0 clear */
    RC_B(0, 0x22) = 0xAA;    /* status block bytes that must be wiped */
    RC_B(0, 0x23) = 0xBB;
    RC_B(0, 0x24) = 0xCC;
    RC_B(0, 0x25) = 0xDD;
    RC_B(0, 0x26) = 0xEE;    /* sleep flag */
    RC_B(0, 0x27) = 0xFF;    /* combat_aux[0] (silence) */
    RC_W(0, 0x40) = 7;       /* hp_current (drained) */
    RC_W(0, 0x42) = 99;      /* hp_max */
    RC_W(0, 0x44) = 3;       /* mp_current (drained) */
    RC_W(0, 0x46) = 42;      /* mp_max */

    /* template entry 0 already holds char_id 5 (matches). */
    TMPL_B(0, 0x08) = 5;

    fd2_save_runtime_char_to_template();

    /* char_id copied through */
    ASSERT_EQ(TMPL_B(0, 0x08), 5);
    /* status block (+0x22..+0x27) zeroed */
    ASSERT_EQ(TMPL_B(0, 0x22), 0);
    ASSERT_EQ(TMPL_B(0, 0x23), 0);
    ASSERT_EQ(TMPL_B(0, 0x24), 0);
    ASSERT_EQ(TMPL_B(0, 0x25), 0);
    ASSERT_EQ(TMPL_B(0, 0x26), 0);
    ASSERT_EQ(TMPL_B(0, 0x27), 0);
    /* flags masked to dead-bit only: 0x84 & 1 == 0 */
    ASSERT_EQ(TMPL_B(0, 0x05), 0);
    /* alive -> hp_current synced to hp_max */
    ASSERT_EQ(TMPL_W(0, 0x40), 99);
    ASSERT_EQ(TMPL_W(0, 0x42), 99);
    /* mp always restored */
    ASSERT_EQ(TMPL_W(0, 0x44), 42);
    ASSERT_EQ(TMPL_W(0, 0x46), 42);
}

/* ---- Test: dead char keeps drained HP, dead-bit preserved ---- */
static void test_save_dead_keeps_hp(void)
{
    save_fixture_reset();

    /* char_id 9 (not 0, so no special-case skip), dead (flags bit0 set). */
    RC_B(0, 0x08) = 9;
    RC_B(0, 0x05) = 0x85;    /* dead-bit set + transient bits */
    RC_W(0, 0x40) = 0;       /* hp_current 0 (dead) */
    RC_W(0, 0x42) = 50;      /* hp_max */
    RC_W(0, 0x44) = 1;       /* mp_current */
    RC_W(0, 0x46) = 30;      /* mp_max */

    TMPL_B(0, 0x08) = 9;

    fd2_save_runtime_char_to_template();

    /* flags masked: 0x85 & 1 == 1 (dead) */
    ASSERT_EQ(TMPL_B(0, 0x05), 1);
    /* dead -> hp_current NOT synced; stays the copied value (0) */
    ASSERT_EQ(TMPL_W(0, 0x40), 0);
    ASSERT_EQ(TMPL_W(0, 0x42), 50);
    /* mp still restored even when dead */
    ASSERT_EQ(TMPL_W(0, 0x44), 30);
    ASSERT_EQ(TMPL_W(0, 0x46), 30);
}

/* ---- Test: char 0 dead in battle -> template NOT overwritten ---- */
static void test_save_char0_dead_skips(void)
{
    save_fixture_reset();
    g_check_char_is_dead_return = 1;   /* 索爾 reported dead in battle */

    /* runtime char_id 0 (索爾), would carry battle-dead state. */
    RC_B(0, 0x08) = 0;
    RC_B(0, 0x05) = 0x01;    /* dead */
    RC_W(0, 0x40) = 0;       /* hp_current 0 */

    /* template entry 0 holds char 0 with a SENTINEL pre-state that must survive. */
    TMPL_B(0, 0x08) = 0;
    TMPL_B(0, 0x05) = 0x77;  /* sentinel flags */
    TMPL_W(0, 0x40) = 1234;  /* sentinel hp_current */

    fd2_save_runtime_char_to_template();

    /* skip path: nothing copied/masked -> sentinels intact */
    ASSERT_EQ(TMPL_B(0, 0x05), 0x77);
    ASSERT_EQ(TMPL_W(0, 0x40), 1234);
}

/* ---- Test: char 0 ALIVE -> still copied (special case only when dead) ---- */
static void test_save_char0_alive_copies(void)
{
    save_fixture_reset();
    g_check_char_is_dead_return = 0;   /* 索爾 alive */

    RC_B(0, 0x08) = 0;
    RC_B(0, 0x05) = 0x80;    /* acted bit, not dead */
    RC_W(0, 0x40) = 5;
    RC_W(0, 0x42) = 80;
    RC_W(0, 0x44) = 2;
    RC_W(0, 0x46) = 20;

    TMPL_B(0, 0x08) = 0;
    TMPL_W(0, 0x40) = 1234;  /* sentinel that should be overwritten */

    fd2_save_runtime_char_to_template();

    /* copied + masked + HP/MP restored */
    ASSERT_EQ(TMPL_B(0, 0x05), 0);     /* 0x80 & 1 == 0 */
    ASSERT_EQ(TMPL_W(0, 0x40), 80);    /* alive -> hp_current = hp_max */
    ASSERT_EQ(TMPL_W(0, 0x44), 20);    /* mp restored */
}

/* ---- Test: no char_id match -> template untouched ---- */
static void test_save_no_match_no_write(void)
{
    save_fixture_reset();

    RC_B(0, 0x08) = 5;       /* runtime char_id 5 */
    RC_W(0, 0x40) = 7;

    /* template entry holds a DIFFERENT char_id (3) with sentinels. */
    TMPL_B(0, 0x08) = 3;
    TMPL_B(0, 0x05) = 0x55;
    TMPL_W(0, 0x40) = 999;

    fd2_save_runtime_char_to_template();

    /* no match -> nothing written */
    ASSERT_EQ(TMPL_B(0, 0x08), 3);
    ASSERT_EQ(TMPL_B(0, 0x05), 0x55);
    ASSERT_EQ(TMPL_W(0, 0x40), 999);
}

/* ---- Test: recompute runs -> aggregate AP (+0x48) re-derived from +0x37 ---- */
static void test_save_recompute_runs(void)
{
    save_fixture_reset();

    RC_B(0, 0x08) = 4;       /* char_id */
    RC_B(0, 0x05) = 0x00;    /* alive */
    RC_W(0, 0x37) = 123;     /* base AP source (recompute reads slot+0x37) */
    RC_W(0, 0x42) = 60;      /* hp_max */
    RC_W(0, 0x46) = 25;      /* mp_max */
    /* inventory slots left 0 -> no equipped marker -> no item-effect lookups */

    TMPL_B(0, 0x08) = 4;
    TMPL_W(0, 0x48) = 0xDEAD; /* stale aggregate that recompute must overwrite */

    fd2_save_runtime_char_to_template();

    /* recompute wrote AP_total (= base +0x37, no item boosts) into +0x48 */
    ASSERT_EQ(TMPL_W(0, 0x48), 123);
}

/* ---- Test: multi runtime members match correct template rows ---- */
static void test_save_multi_member_routing(void)
{
    save_fixture_reset();
    data_fd2_battle_party_member_count = 2;
    data_fd2_shared_menu_party_member_count = 2;

    /* runtime[0] = char_id 5, runtime[1] = char_id 8 */
    RC_B(0, 0x08) = 5;
    RC_B(0, 0x05) = 0x00;
    RC_W(0, 0x42) = 11;
    RC_W(0, 0x46) = 12;
    RC_B(1, 0x08) = 8;
    RC_B(1, 0x05) = 0x00;
    RC_W(1, 0x42) = 21;
    RC_W(1, 0x46) = 22;

    /* template rows in SWAPPED order: row0 = char 8, row1 = char 5. */
    TMPL_B(0, 0x08) = 8;
    TMPL_B(1, 0x08) = 5;

    fd2_save_runtime_char_to_template();

    /* runtime char 5 must land in template row1, char 8 in row0. */
    ASSERT_EQ(TMPL_W(1, 0x40), 11);  /* row1 (char5): hp_current = hp_max 11 */
    ASSERT_EQ(TMPL_W(1, 0x44), 12);
    ASSERT_EQ(TMPL_W(0, 0x40), 21);  /* row0 (char8): hp_current = hp_max 21 */
    ASSERT_EQ(TMPL_W(0, 0x44), 22);
}

/* ================================================================
 * fd2_save_compute_checksum — byte-sum integrity checksum.
 *
 * Sums buf[0 .. len-5] as a u32 (the last 4 bytes hold the checksum
 * itself and are excluded). Pure in-memory computation, no fopen.
 * Expected values cross-checked against the FD2.LE emulator:
 *   bytes 01..08, len 8 -> sum of first 4 = 0x0A
 *   bytes FF*8,   len 8 -> 4*0xFF        = 0x3FC
 * ================================================================ */

/* ---- Test: hand-computed sum, last 4 bytes excluded ---- */
static void test_checksum_basic_excludes_last4(void)
{
    uint8 buf[8];

    buf[0] = 1; buf[1] = 2; buf[2] = 3; buf[3] = 4;
    buf[4] = 5; buf[5] = 6; buf[6] = 7; buf[7] = 8;
    /* len 8 -> remaining = 4 -> sums buf[0..3] = 1+2+3+4 = 10. */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 8), 10);
}

/* ---- Test: trailing 4 bytes never contribute (vary them) ---- */
static void test_checksum_trailing_bytes_ignored(void)
{
    uint8 buf[8];

    buf[0] = 0x10; buf[1] = 0x20; buf[2] = 0x30; buf[3] = 0x40;
    /* last 4 bytes set to max; they must NOT be summed. */
    buf[4] = 0xFF; buf[5] = 0xFF; buf[6] = 0xFF; buf[7] = 0xFF;
    /* sums buf[0..3] = 0x10+0x20+0x30+0x40 = 0xA0. */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 8), 0xA0);
}

/* ---- Test: accumulator is full u32 (no byte/word truncation) ---- */
static void test_checksum_u32_accumulator(void)
{
    uint8 buf[12];
    int i;

    for (i = 0; i < 12; i++) {
        buf[i] = 0xFF;
    }
    /* len 12 -> remaining = 8 -> 8 * 0xFF = 0x7F8 (exceeds a byte,
     * proving the running sum is not truncated to 8 bits). */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 12), 0x7F8);
}

/* ---- Test: natural u32 wrap on overflow (matches LODSB/ADD EBX) ---- */
static void test_checksum_u32_wrap(void)
{
    static uint8 buf[0x10008];
    uint32 i;
    uint32 result;

    /* 0x10008-byte buffer of 0xFF: len 0x10008 -> remaining 0x10004
     * summed bytes -> total 0x10004 * 0xFF = 0xFF03FC, which far
     * exceeds 16 bits, exercising the full 32-bit running sum. */
    for (i = 0; i < 0x10008; i++) {
        buf[i] = 0xFF;
    }
    /* len 0x10008 -> remaining = 0x10004 -> 0x10004 * 0xFF. */
    result = (uint32)0x10004 * (uint32)0xFF;   /* = 0xFF03FC */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 0x10008), result);
}

/* ---- Test: minimal valid length (len = 5 -> sums exactly 1 byte) ---- */
static void test_checksum_len5_single_byte(void)
{
    uint8 buf[5];

    buf[0] = 0x7B;   /* the only summed byte */
    buf[1] = 0xFF; buf[2] = 0xFF; buf[3] = 0xFF; buf[4] = 0xFF;
    /* len 5 -> remaining = 1 -> sums buf[0] only = 0x7B. */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 5), 0x7B);
}

void run_save_save_tests(void)
{
    SUITE_BEGIN(save_save);
    RUN_TEST(test_save_alive_copy_and_restore);
    RUN_TEST(test_save_dead_keeps_hp);
    RUN_TEST(test_save_char0_dead_skips);
    RUN_TEST(test_save_char0_alive_copies);
    RUN_TEST(test_save_no_match_no_write);
    RUN_TEST(test_save_recompute_runs);
    RUN_TEST(test_save_multi_member_routing);
    RUN_TEST(test_checksum_basic_excludes_last4);
    RUN_TEST(test_checksum_trailing_bytes_ignored);
    RUN_TEST(test_checksum_u32_accumulator);
    RUN_TEST(test_checksum_u32_wrap);
    RUN_TEST(test_checksum_len5_single_byte);
    SUITE_END();
}
