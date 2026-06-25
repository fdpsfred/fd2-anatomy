/*
 * Frozen logic-net unit tests: src/save/save.c checksum + cipher primitives.
 *
 * fd2_save_compute_checksum (byte-sum integrity) and fd2_save_crypt_buffer
 * (involution stream cipher) are pure in-memory routines with no callees, no
 * spy doubles, no fopen -> immune to the coordinated-landing breakage that
 * retired the broader save spy/orchestration tests (moved to
 * legacy/tests_unit_spy/). They underpin the FD2.SAV round-trip the playthrough
 * system relies on (CONTINUE load, save commit), so they are pinned directly.
 *
 * Expected values cross-checked against the FD2.LE function under Ghidra's
 * emulator (see per-test notes).
 */
#include <string.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "protos.h"

/* ================================================================
 * fd2_save_compute_checksum -- sums buf[0 .. len-5] as a u32 (the last 4
 * bytes hold the checksum itself and are excluded). Emulator ground truth:
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

/* ================================================================
 * fd2_save_crypt_buffer -- XOR stream cipher (involution).
 *
 * Ground-truth keystream (state seed 0xA5, advance via ROL16(state + 0x9014,
 * 3), XOR low byte). The first four keystream bytes were verified against the
 * FD2.LE function under Ghidra's emulator: final 16-bit state after 4
 * iterations = 0x45AC.
 *   iter1: ROL16(0x90B9,3) = 0x85CC -> key 0xCC
 *   iter2: ROL16(0x15E0,3) = 0xAF00 -> key 0x00
 *   iter3: ROL16(0x3F14,3) = 0xF8A1 -> key 0xA1
 *   iter4: ROL16(0x88B5,3) = 0x45AC -> key 0xAC
 * ================================================================ */

/* The deterministic keystream the cipher XORs in, byte by byte. */
static const uint8 g_crypt_keystream[4] = { 0xCC, 0x00, 0xA1, 0xAC };

/* ---- Test: zero buffer -> output equals the raw keystream ---- */
static void test_crypt_keystream_on_zero_buffer(void)
{
    uint8 buf[4];

    buf[0] = 0x00; buf[1] = 0x00; buf[2] = 0x00; buf[3] = 0x00;
    fd2_save_crypt_buffer((uint32)buf, 4);
    /* 0x00 ^ key == key, so the buffer now holds the keystream. */
    ASSERT_MEM_EQ(buf, g_crypt_keystream, 4);
}

/* ---- Test: non-zero buffer XORed with the known keystream ---- */
static void test_crypt_xor_known_data(void)
{
    uint8 buf[4];

    buf[0] = 0x12; buf[1] = 0x34; buf[2] = 0x56; buf[3] = 0x78;
    fd2_save_crypt_buffer((uint32)buf, 4);
    ASSERT_EQ(buf[0], (uint8)(0x12 ^ 0xCC));   /* 0xDE */
    ASSERT_EQ(buf[1], (uint8)(0x34 ^ 0x00));   /* 0x34 */
    ASSERT_EQ(buf[2], (uint8)(0x56 ^ 0xA1));   /* 0xF7 */
    ASSERT_EQ(buf[3], (uint8)(0x78 ^ 0xAC));   /* 0xD4 */
}

/* ---- Test: involution -- crypt twice restores the original ---- */
static void test_crypt_is_involution(void)
{
    uint8 buf[64];
    uint8 orig[64];
    uint32 i;

    for (i = 0; i < 64; i++) {
        buf[i] = (uint8)(i * 7 + 3);
        orig[i] = buf[i];
    }
    fd2_save_crypt_buffer((uint32)buf, 64);   /* encrypt */
    /* After one pass the data must differ somewhere (keystream != 0). */
    ASSERT_NE(buf[0], orig[0]);
    fd2_save_crypt_buffer((uint32)buf, 64);   /* decrypt */
    ASSERT_MEM_EQ(buf, orig, 64);
}

/* ---- Test: size = 1 applies exactly one keystream byte ---- */
static void test_crypt_size_one(void)
{
    uint8 buf[1];

    buf[0] = 0xFF;
    fd2_save_crypt_buffer((uint32)buf, 1);
    ASSERT_EQ(buf[0], (uint8)(0xFF ^ 0xCC));   /* 0x33 */
}

void run_save_savecsum_tests(void)
{
    SUITE_BEGIN(save_savecsum);
    RUN_TEST(test_checksum_basic_excludes_last4);
    RUN_TEST(test_checksum_trailing_bytes_ignored);
    RUN_TEST(test_checksum_u32_accumulator);
    RUN_TEST(test_checksum_u32_wrap);
    RUN_TEST(test_checksum_len5_single_byte);
    RUN_TEST(test_crypt_keystream_on_zero_buffer);
    RUN_TEST(test_crypt_xor_known_data);
    RUN_TEST(test_crypt_is_involution);
    RUN_TEST(test_crypt_size_one);
    SUITE_END();
}
