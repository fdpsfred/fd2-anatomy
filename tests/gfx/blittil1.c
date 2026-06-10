/*
 * unit tests for src/gfx/blittile.c (part 2)
 *
 * Covers fd2_tile_blit_24x24_with_tint_offset @ 0x4DC34, the hand-written
 * RLE blit with palette-band tinting.
 *
 * The decoder ALWAYS processes 24 rows of 24 columns, so every test stream
 * must be a complete 24-row program: each row's commands must consume
 * exactly 24 column-counts (a stride-2 pixel consumes two). The "filler"
 * rows used after the row under test are a single SKIP-24 command, which
 * advances the destination by a full transparent row without writing.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* RLE command byte builders (low 6 bits + 1 == run length). */
#define CMD_RUN(n)     ((uint8)(0x00u | ((n) - 1)))   /* fill n from 1 src byte */
#define CMD_STRIDE(n)  ((uint8)(0x40u | ((n) - 1)))   /* n pixels, every other  */
#define CMD_LIT(n)     ((uint8)(0x80u | ((n) - 1)))   /* copy n, 1 src byte each */
#define CMD_SKIP(n)    ((uint8)(0xC0u | ((n) - 1)))   /* advance n (transparent) */

#define TINT_W      24          /* sprite is 24x24 */
#define TINT_H      24
#define TINT_STRIDE 0x40u       /* generous dst row stride for stride tests */
#define TINT_SENT   0xEEu       /* sentinel for untouched dst bytes         */

/* Destination big enough for 24 rows at the widest stride used here. */
static uint8 g_tint_dst[TINT_STRIDE * (TINT_H + 1)];

/* The reference pixel transform, computed in 8-bit exactly as the asm does:
 * ADD AL,team_offset ; AND AL,7 ; ADD AL,color_base. */
static uint8 tint_of(uint8 src, uint8 off, uint8 base)
{
    return (uint8)((((uint8)(src + off)) & 7u) + base);
}

static void tint_reset_dst(void)
{
    memset(g_tint_dst, TINT_SENT, sizeof(g_tint_dst));
}

/* Append a full transparent (SKIP-24) row to the stream at *pp. */
static void put_skip_row(uint8 **pp)
{
    *(*pp)++ = CMD_SKIP(TINT_W);
}

/* Append 23 filler SKIP rows (rows 1..23) after the row under test. */
static void put_filler_rows(uint8 **pp)
{
    int r;
    for (r = 1; r < TINT_H; r++) {
        put_skip_row(pp);
    }
}

/* ----------------------------------------------------------------
 * LITERAL mode (0x80..0xBF): copy n tinted pixels, one source byte
 * each, into consecutive dst bytes. Verify the per-pixel transform
 * and that exactly n bytes were written; the rest of row 0 (SKIP)
 * and the filler rows stay untouched.
 * ---------------------------------------------------------------- */
static void test_tint_literal_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = CMD_LIT(5);             /* 5 literal pixels */
    *p++ = 0x00;
    *p++ = 0x01;
    *p++ = 0x09;                   /* 0x09 & 7 == 1 */
    *p++ = 0x07;
    *p++ = 0xFFu;                  /* 0xFF & 7 == 7 */
    *p++ = CMD_SKIP(19);          /* finish the 24-column row */
    put_filler_rows(&p);

    tint_reset_dst();
    fd2_tile_blit_24x24_with_tint_offset((uint32)stream, (uint32)g_tint_dst,
                                         TINT_W, 0u, 0u);

    ASSERT_EQ(g_tint_dst[0], tint_of(0x00u, 0u, 0u));
    ASSERT_EQ(g_tint_dst[1], tint_of(0x01u, 0u, 0u));
    ASSERT_EQ(g_tint_dst[2], tint_of(0x09u, 0u, 0u));
    ASSERT_EQ(g_tint_dst[3], tint_of(0x07u, 0u, 0u));
    ASSERT_EQ(g_tint_dst[4], tint_of(0xFFu, 0u, 0u));
    /* columns 5..23 were SKIP, and every filler row (24..575) untouched */
    for (i = 5; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_tint_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * RUN mode (0x00..0x3F): fill n tinted pixels from a SINGLE source
 * byte at consecutive dst positions; only one source byte consumed.
 * ---------------------------------------------------------------- */
static void test_tint_run_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = CMD_RUN(6);             /* fill 6 from one src byte */
    *p++ = 0x0Au;                  /* 0x0A & 7 == 2 */
    *p++ = CMD_SKIP(18);          /* 6 + 18 == 24 */
    put_filler_rows(&p);

    tint_reset_dst();
    fd2_tile_blit_24x24_with_tint_offset((uint32)stream, (uint32)g_tint_dst,
                                         TINT_W, 0u, 0u);

    e = tint_of(0x0Au, 0u, 0u);
    for (i = 0; i < 6; i++) {
        ASSERT_EQ(g_tint_dst[i], e);
    }
    for (i = 6; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_tint_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * STRIDE-2 RUN (0x40..0x7F): write n tinted pixels from a single
 * source byte, spaced every other dst byte (dst += 2 per pixel).
 * Each pixel consumes TWO column-counts, so a stride run of n covers
 * 2n columns. Verify the painted/gap interleave.
 * ---------------------------------------------------------------- */
static void test_tint_stride2_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = CMD_STRIDE(4);          /* 4 pixels -> 8 column-counts */
    *p++ = 0x05u;                  /* 0x05 & 7 == 5 */
    *p++ = CMD_SKIP(16);          /* 8 + 16 == 24 */
    put_filler_rows(&p);

    tint_reset_dst();
    fd2_tile_blit_24x24_with_tint_offset((uint32)stream, (uint32)g_tint_dst,
                                         TINT_W, 0u, 0u);

    e = tint_of(0x05u, 0u, 0u);
    /* INC EDI precedes each STOSB: pixels land at offsets 1,3,5,7. */
    ASSERT_EQ(g_tint_dst[0], TINT_SENT);
    ASSERT_EQ(g_tint_dst[1], e);
    ASSERT_EQ(g_tint_dst[2], TINT_SENT);
    ASSERT_EQ(g_tint_dst[3], e);
    ASSERT_EQ(g_tint_dst[4], TINT_SENT);
    ASSERT_EQ(g_tint_dst[5], e);
    ASSERT_EQ(g_tint_dst[6], TINT_SENT);
    ASSERT_EQ(g_tint_dst[7], e);
    /* dst is now at offset 8; SKIP 16 -> offset 24, nothing else written. */
    for (i = 8; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_tint_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * SKIP (0xC0..0xFF): advance dst by n with no writes (transparent),
 * consuming no source bytes. Confirm dst lands correctly for a
 * following command on the same row.
 * ---------------------------------------------------------------- */
static void test_tint_skip_then_literal(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = CMD_SKIP(10);          /* leave cols 0..9 transparent */
    *p++ = CMD_LIT(1);            /* one literal pixel at col 10 */
    *p++ = 0x13u;                 /* 0x13 & 7 == 3 */
    *p++ = CMD_SKIP(13);         /* 10 + 1 + 13 == 24 */
    put_filler_rows(&p);

    tint_reset_dst();
    fd2_tile_blit_24x24_with_tint_offset((uint32)stream, (uint32)g_tint_dst,
                                         TINT_W, 0u, 0u);

    for (i = 0; i < 10; i++) {
        ASSERT_EQ(g_tint_dst[i], TINT_SENT);
    }
    ASSERT_EQ(g_tint_dst[10], tint_of(0x13u, 0u, 0u));
    for (i = 11; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_tint_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * Pixel transform with non-zero team_offset and color_base, plus the
 * 8-bit wrap of (src + team_offset) before masking. With off=5:
 *   src 0x03 -> (3+5)&7 == 0
 *   src 0x00 -> (0+5)&7 == 5
 *   src 0xFF -> (0x104)&7 == 4
 *   src 0x06 -> (6+5)&7 == 3
 * base shifts the whole band up.
 * ---------------------------------------------------------------- */
static void test_tint_offset_and_base_wrap(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    uint8 off = 5u;
    uint8 base = 0x30u;

    *p++ = CMD_LIT(4);
    *p++ = 0x03u;
    *p++ = 0x00u;
    *p++ = 0xFFu;
    *p++ = 0x06u;
    *p++ = CMD_SKIP(20);
    put_filler_rows(&p);

    tint_reset_dst();
    fd2_tile_blit_24x24_with_tint_offset((uint32)stream, (uint32)g_tint_dst,
                                         TINT_W, base, off);

    ASSERT_EQ(g_tint_dst[0], (uint8)(0u + base));
    ASSERT_EQ(g_tint_dst[1], (uint8)(5u + base));
    ASSERT_EQ(g_tint_dst[2], (uint8)(4u + base));
    ASSERT_EQ(g_tint_dst[3], (uint8)(3u + base));
    /* cross-check against the shared reference transform */
    ASSERT_EQ(g_tint_dst[0], tint_of(0x03u, off, base));
    ASSERT_EQ(g_tint_dst[2], tint_of(0xFFu, off, base));
}

/* ----------------------------------------------------------------
 * Full 24x24 sprite, one LITERAL-24 command per row, stride 24 so the
 * destination is a contiguous 576-byte image. Source byte for row r,
 * col c is (r*24 + c). Exercises all 24 rows and the row-wrap reset,
 * and confirms the byte just past the image is untouched.
 * ---------------------------------------------------------------- */
static void test_tint_full_24x24_row_wrap(void)
{
    uint8 stream[TINT_H * (1 + TINT_W)];
    uint8 *p = stream;
    int r, c;
    uint8 sv;

    for (r = 0; r < TINT_H; r++) {
        *p++ = CMD_LIT(TINT_W);
        for (c = 0; c < TINT_W; c++) {
            *p++ = (uint8)(r * TINT_W + c);
        }
    }

    tint_reset_dst();
    fd2_tile_blit_24x24_with_tint_offset((uint32)stream, (uint32)g_tint_dst,
                                         TINT_W, 0u, 0u);

    for (r = 0; r < TINT_H; r++) {
        for (c = 0; c < TINT_W; c++) {
            sv = (uint8)(r * TINT_W + c);
            ASSERT_EQ(g_tint_dst[r * TINT_W + c], tint_of(sv, 0u, 0u));
        }
    }
    ASSERT_EQ(g_tint_dst[TINT_W * TINT_H], TINT_SENT);
}

/* ----------------------------------------------------------------
 * Row stride > 24: the decoder must jump dst by (stride - 0x18) at
 * each row boundary, leaving the inter-row gap untouched. Row 0 is a
 * RUN-24, rows 1..23 are SKIP-24 (which still advance dst by a full
 * stride per row). Confirm row 0 content, the gap after it, and that
 * the next row would begin exactly at offset == stride.
 * ---------------------------------------------------------------- */
static void test_tint_row_stride_advance(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e0;

    *p++ = CMD_RUN(TINT_W);        /* row 0: RUN 24 from src 0x02 */
    *p++ = 0x02u;
    put_filler_rows(&p);           /* rows 1..23: SKIP 24 each */

    tint_reset_dst();
    fd2_tile_blit_24x24_with_tint_offset((uint32)stream, (uint32)g_tint_dst,
                                         TINT_STRIDE, 0u, 0u);

    e0 = tint_of(0x02u, 0u, 0u);
    for (i = 0; i < TINT_W; i++) {
        ASSERT_EQ(g_tint_dst[i], e0);
    }
    /* gap between row 0 columns and the next row start (24..stride-1) */
    for (i = TINT_W; i < (int)TINT_STRIDE; i++) {
        ASSERT_EQ(g_tint_dst[i], TINT_SENT);
    }
    /* every following row is SKIP-only, so the whole rest stays sentinel,
     * including the byte at offset == stride where row 1 begins */
    for (i = (int)TINT_STRIDE; i < (int)(TINT_STRIDE * TINT_H); i++) {
        ASSERT_EQ(g_tint_dst[i], TINT_SENT);
    }
}

void run_gfx_blittile1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/blittile (part 2 - tint RLE blit)\n");
    RUN_TEST(test_tint_literal_mode);
    RUN_TEST(test_tint_run_mode);
    RUN_TEST(test_tint_stride2_mode);
    RUN_TEST(test_tint_skip_then_literal);
    RUN_TEST(test_tint_offset_and_base_wrap);
    RUN_TEST(test_tint_full_24x24_row_wrap);
    RUN_TEST(test_tint_row_stride_advance);
    printf("\n");
}
