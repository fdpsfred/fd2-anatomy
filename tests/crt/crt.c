/*
 * unit tests for src/crt/crt.c
 *
 * crt_equivalent_lx_chunk_read_36107 @ 0x36107 — mode-flag chunk reader.
 *   mode bit0=1 : memcpy(dest, file_handle+offset, length)   (in-memory)
 *   mode bit0=0 : lseek(file_handle, offset, SEEK_SET)
 *                 + read(file_handle, dest, length)          (file-backed)
 *   returns offset+length.
 *
 * The file-backed path is exercised against a SCRATCH temp file created
 * in the test cwd with the real Watcom low-level open/write/close (this is
 * a generated scratch file, NOT a staged game resource), so the real
 * lseek+read primitives the function calls are driven end-to-end and the
 * read-back bytes are checked against the pattern written.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>   /* S_IREAD, S_IWRITE */

#define CRT_SCRATCH_FILE "CRTCHUNK.TMP"

/* ---- in-memory (memcpy) path ---- */

/* mode bit0 set: copy length bytes from (file_handle+offset) into dest.
 * Here file_handle is reused as the base address of an in-memory source. */
static void test_memcpy_path_basic(void)
{
    static uint8 src[16];
    static uint8 dst[16];
    int i;
    int r;

    for (i = 0; i < 16; i++) {
        src[i] = (uint8)(0xA0 + i);
        dst[i] = 0;
    }
    /* offset 0, length 8, mode bit0=1 -> memcpy(dst, src+0, 8) */
    r = crt_equivalent_lx_chunk_read_36107((int)src, 0, 1, dst, 8);

    ASSERT_EQ(dst[0], 0xA0);
    ASSERT_EQ(dst[7], 0xA7);
    ASSERT_EQ(dst[8], 0);          /* not over-copied */
    ASSERT_EQ(r, 0 + 8);           /* offset + length */
}

/* memcpy path with a non-zero offset: the source address is base+offset,
 * proving the offset is folded into the src pointer (and the return). */
static void test_memcpy_path_offset(void)
{
    static uint8 src[16];
    static uint8 dst[16];
    int i;
    int r;

    for (i = 0; i < 16; i++) {
        src[i] = (uint8)(i);
        dst[i] = 0xFF;
    }
    /* offset 4, length 5, mode bit0=1 -> memcpy(dst, src+4, 5) */
    r = crt_equivalent_lx_chunk_read_36107((int)src, 4, 1, dst, 5);

    ASSERT_EQ(dst[0], 4);
    ASSERT_EQ(dst[1], 5);
    ASSERT_EQ(dst[4], 8);
    ASSERT_EQ(dst[5], 0xFF);       /* untouched past length */
    ASSERT_EQ(r, 4 + 5);           /* offset + length */
}

/* mode odd values other than 1 still take the in-memory branch (bit0 test). */
static void test_mode_bit0_only(void)
{
    static uint8 src[8];
    static uint8 dst[8];
    int i;
    int r;

    for (i = 0; i < 8; i++) {
        src[i] = (uint8)(0x10 + i);
        dst[i] = 0;
    }
    /* mode 0xFF has bit0 set -> memcpy path */
    r = crt_equivalent_lx_chunk_read_36107((int)src, 0, 0xFF, dst, 4);

    ASSERT_EQ(dst[0], 0x10);
    ASSERT_EQ(dst[3], 0x13);
    ASSERT_EQ(dst[4], 0);
    ASSERT_EQ(r, 4);
}

/* ---- file-backed (lseek + read) path ---- */

/* Create the scratch file with a known byte pattern using the real
 * low-level write path, returning a read-only descriptor positioned by
 * the function under test. */
static int crt_make_scratch(int n)
{
    int fd;
    uint8 buf[64];
    int i;

    for (i = 0; i < n && i < (int)sizeof(buf); i++) {
        buf[i] = (uint8)(0x80 + i);
    }
    fd = open(CRT_SCRATCH_FILE, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
              S_IREAD | S_IWRITE);
    if (fd < 0) {
        return -1;
    }
    write(fd, buf, (unsigned)n);
    close(fd);
    return open(CRT_SCRATCH_FILE, O_RDONLY | O_BINARY);
}

/* mode bit0 clear: lseek to offset then read length bytes. Offset 0. */
static void test_file_read_at_zero(void)
{
    static uint8 dst[64];
    int fd;
    int r;
    int i;

    for (i = 0; i < 64; i++) {
        dst[i] = 0;
    }
    fd = crt_make_scratch(32);
    ASSERT_TRUE(fd >= 0);

    /* offset 0, length 16, mode bit0=0 -> read first 16 bytes */
    r = crt_equivalent_lx_chunk_read_36107(fd, 0, 0, dst, 16);
    close(fd);
    remove(CRT_SCRATCH_FILE);

    ASSERT_EQ(dst[0], 0x80);
    ASSERT_EQ(dst[15], 0x8F);
    ASSERT_EQ(dst[16], 0);         /* read stopped at length */
    ASSERT_EQ(r, 0 + 16);          /* offset + length */
}

/* file-backed read FROM A NON-ZERO OFFSET: proves the lseek(SEEK_SET)
 * actually repositions the file before read (a missing/broken seek would
 * return the bytes at offset 0 instead). */
static void test_file_read_at_offset(void)
{
    static uint8 dst[64];
    int fd;
    int r;
    int i;

    for (i = 0; i < 64; i++) {
        dst[i] = 0;
    }
    fd = crt_make_scratch(32);
    ASSERT_TRUE(fd >= 0);

    /* offset 8, length 8, mode bit0=0 -> bytes [8..15] = 0x88..0x8F */
    r = crt_equivalent_lx_chunk_read_36107(fd, 8, 0, dst, 8);
    close(fd);
    remove(CRT_SCRATCH_FILE);

    ASSERT_EQ(dst[0], 0x88);       /* lseek landed on offset 8 */
    ASSERT_EQ(dst[7], 0x8F);
    ASSERT_EQ(dst[8], 0);
    ASSERT_EQ(r, 8 + 8);           /* offset + length */
}

/* Even mode values (bit0 clear) take the file path; pin the bit0 test on
 * the file branch the same way test_mode_bit0_only pins the memcpy branch. */
static void test_file_even_mode(void)
{
    static uint8 dst[64];
    int fd;
    int r;
    int i;

    for (i = 0; i < 64; i++) {
        dst[i] = 0;
    }
    fd = crt_make_scratch(32);
    ASSERT_TRUE(fd >= 0);

    /* mode 2 has bit0 clear -> file path; offset 4, length 4 */
    r = crt_equivalent_lx_chunk_read_36107(fd, 4, 2, dst, 4);
    close(fd);
    remove(CRT_SCRATCH_FILE);

    ASSERT_EQ(dst[0], 0x84);
    ASSERT_EQ(dst[3], 0x87);
    ASSERT_EQ(dst[4], 0);
    ASSERT_EQ(r, 4 + 4);
}

void run_crt_crt_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: crt/crt\n");
    RUN_TEST(test_memcpy_path_basic);
    RUN_TEST(test_memcpy_path_offset);
    RUN_TEST(test_mode_bit0_only);
    RUN_TEST(test_file_read_at_zero);
    RUN_TEST(test_file_read_at_offset);
    RUN_TEST(test_file_even_mode);
    printf("\n");
}
