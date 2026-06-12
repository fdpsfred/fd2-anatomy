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

/* ================================================================
 * fd2_tile_blit_24x24_remap @ 0x4DCC6 — same RLE syntax as the tint
 * blitter above, but every written pixel is mapped through a 256-entry
 * palette_remap LUT, and the 0xC0 command is NOT a transparent skip:
 * it REMAPS the existing destination byte in place (out = remap[dst]).
 *
 * Because there is no transparent-skip command, filler rows after the
 * row under test use the in-place-remap command (CMD_INPLACE) over the
 * sentinel-filled destination; the LUT is built so remap[SENT] == SENT,
 * making that an observable no-op on untouched bytes.
 * ================================================================ */

#define CMD_INPLACE(n) ((uint8)(0xC0u | ((n) - 1)))  /* remap existing dst */

/* Destination for the remap-blit tests (same geometry as the tint one). */
static uint8 g_remap_dst[TINT_STRIDE * (TINT_H + 1)];

/* 256-entry palette translation table used by the remap blitter. */
static uint8 g_remap_lut[256];

/* Build a non-identity LUT (out = in + 0x10, 8-bit wrap) but pin
 * remap[SENT] = SENT so the in-place filler rows leave sentinel bytes
 * unchanged and "untouched" assertions remain meaningful. */
static void remap_lut_init(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        g_remap_lut[i] = (uint8)(i + 0x10);
    }
    g_remap_lut[TINT_SENT] = TINT_SENT;
}

static void remap_reset_dst(void)
{
    memset(g_remap_dst, TINT_SENT, sizeof(g_remap_dst));
}

/* Append (TINT_H - 1) in-place-remap filler rows (rows 1..23). Over the
 * sentinel destination with remap[SENT]==SENT these advance dst without
 * changing any byte's value. */
static void put_remap_filler_rows(uint8 **pp)
{
    int r;
    for (r = 1; r < TINT_H; r++) {
        *(*pp)++ = CMD_INPLACE(TINT_W);
    }
}

/* ----------------------------------------------------------------
 * RUN mode (0x00..0x3F): fill n pixels from a SINGLE remapped source
 * byte; only one source byte consumed.
 * ---------------------------------------------------------------- */
static void test_remap_run_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = CMD_RUN(6);
    *p++ = 0x0Au;
    *p++ = CMD_INPLACE(18);        /* 6 + 18 == 24, sentinel stays sentinel */
    put_remap_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_remap((uint32)stream, (uint32)g_remap_dst,
                              TINT_W, (uint32)g_remap_lut);

    e = g_remap_lut[0x0Au];
    for (i = 0; i < 6; i++) {
        ASSERT_EQ(g_remap_dst[i], e);
    }
    for (i = 6; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * LITERAL mode (0x80..0xBF): copy n remapped pixels, one source byte
 * each, into consecutive dst bytes.
 * ---------------------------------------------------------------- */
static void test_remap_literal_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = CMD_LIT(5);
    *p++ = 0x00u;
    *p++ = 0x01u;
    *p++ = 0x42u;
    *p++ = 0x7Fu;
    *p++ = 0xABu;
    *p++ = CMD_INPLACE(19);        /* finish the 24-column row */
    put_remap_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_remap((uint32)stream, (uint32)g_remap_dst,
                              TINT_W, (uint32)g_remap_lut);

    ASSERT_EQ(g_remap_dst[0], g_remap_lut[0x00u]);
    ASSERT_EQ(g_remap_dst[1], g_remap_lut[0x01u]);
    ASSERT_EQ(g_remap_dst[2], g_remap_lut[0x42u]);
    ASSERT_EQ(g_remap_dst[3], g_remap_lut[0x7Fu]);
    ASSERT_EQ(g_remap_dst[4], g_remap_lut[0xABu]);
    for (i = 5; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * STRIDE-2 RUN (0x40..0x7F): write n remapped pixels from a single
 * source byte at every other dst byte. The asm is INC EDI; STOSB, so
 * pixels land at odd offsets 1,3,5,7 (dst += 2 per pixel, NOT +3), and
 * each pixel consumes TWO column-counts. This is the load-bearing test
 * for the +2 stride (the decompiler renders this branch as +3).
 * ---------------------------------------------------------------- */
static void test_remap_stride2_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = CMD_STRIDE(4);          /* 4 pixels -> 8 column-counts */
    *p++ = 0x05u;
    *p++ = CMD_INPLACE(16);        /* 8 + 16 == 24 */
    put_remap_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_remap((uint32)stream, (uint32)g_remap_dst,
                              TINT_W, (uint32)g_remap_lut);

    e = g_remap_lut[0x05u];
    ASSERT_EQ(g_remap_dst[0], TINT_SENT);
    ASSERT_EQ(g_remap_dst[1], e);
    ASSERT_EQ(g_remap_dst[2], TINT_SENT);
    ASSERT_EQ(g_remap_dst[3], e);
    ASSERT_EQ(g_remap_dst[4], TINT_SENT);
    ASSERT_EQ(g_remap_dst[5], e);
    ASSERT_EQ(g_remap_dst[6], TINT_SENT);
    ASSERT_EQ(g_remap_dst[7], e);
    /* dst is now at offset 8; the rest of the image stays sentinel */
    for (i = 8; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * IN-PLACE REMAP (0xC0..0xFF): for n bytes at dst, write
 * remap[existing_dst_byte] with NO source bytes consumed. Seed the dst
 * with distinct values, remap them in place, then prove the very next
 * stream byte is read as a source byte by a following RUN command
 * (i.e. the in-place command consumed zero source bytes).
 * ---------------------------------------------------------------- */
static void test_remap_inplace_remap_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 seed[5];
    uint8 run_pixel;

    seed[0] = 0x00u;
    seed[1] = 0x10u;
    seed[2] = 0x55u;
    seed[3] = 0x80u;
    seed[4] = 0xFEu;

    *p++ = CMD_INPLACE(5);         /* remap dst[0..4] in place */
    *p++ = CMD_RUN(19);            /* fill dst[5..23] from one src byte */
    *p++ = 0x2Au;                  /* <-- the RUN's source byte */
    put_remap_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    for (i = 0; i < 5; i++) {
        g_remap_dst[i] = seed[i];  /* pre-seed the in-place region */
    }

    fd2_tile_blit_24x24_remap((uint32)stream, (uint32)g_remap_dst,
                              TINT_W, (uint32)g_remap_lut);

    /* dst[0..4] became remap[seed]; this also proves the LUT, not the
     * source stream, drove these bytes. */
    for (i = 0; i < 5; i++) {
        ASSERT_EQ(g_remap_dst[i], g_remap_lut[seed[i]]);
    }
    /* RUN read stream byte 0x2A (in-place consumed no source byte). */
    run_pixel = g_remap_lut[0x2Au];
    for (i = 5; i < TINT_W; i++) {
        ASSERT_EQ(g_remap_dst[i], run_pixel);
    }
    for (i = TINT_W; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * Confirm the LUT is genuinely applied (output == table[src], not the
 * raw source value) across a few source bytes, with a non-identity
 * table whose mapping differs from the identity for each one.
 * ---------------------------------------------------------------- */
static void test_remap_lut_is_applied(void)
{
    uint8 stream[64];
    uint8 *p = stream;

    *p++ = CMD_LIT(4);
    *p++ = 0x01u;                  /* remap -> 0x11 (!= 0x01) */
    *p++ = 0x20u;                  /* remap -> 0x30 */
    *p++ = 0xF0u;                  /* remap -> 0x00 (8-bit wrap) */
    *p++ = 0x7Eu;                  /* remap -> 0x8E */
    *p++ = CMD_INPLACE(20);
    put_remap_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_remap((uint32)stream, (uint32)g_remap_dst,
                              TINT_W, (uint32)g_remap_lut);

    ASSERT_EQ(g_remap_dst[0], 0x11u);
    ASSERT_EQ(g_remap_dst[1], 0x30u);
    ASSERT_EQ(g_remap_dst[2], 0x00u);
    ASSERT_EQ(g_remap_dst[3], 0x8Eu);
    /* none of the outputs equals the raw source byte */
    ASSERT_EQ(g_remap_dst[0] != 0x01u, 1);
    ASSERT_EQ(g_remap_dst[2] != 0xF0u, 1);
}

/* ----------------------------------------------------------------
 * Full 24x24 sprite, one LITERAL-24 command per row, stride 24 so the
 * destination is a contiguous 576-byte image. Source byte for row r,
 * col c is (r*24 + c). Exercises all 24 rows and the row-wrap reset.
 * ---------------------------------------------------------------- */
static void test_remap_full_24x24_row_wrap(void)
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

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_remap((uint32)stream, (uint32)g_remap_dst,
                              TINT_W, (uint32)g_remap_lut);

    for (r = 0; r < TINT_H; r++) {
        for (c = 0; c < TINT_W; c++) {
            sv = (uint8)(r * TINT_W + c);
            ASSERT_EQ(g_remap_dst[r * TINT_W + c], g_remap_lut[sv]);
        }
    }
    ASSERT_EQ(g_remap_dst[TINT_W * TINT_H], TINT_SENT);
}

/* ----------------------------------------------------------------
 * Row stride > 24: the decoder jumps dst by (stride - 0x18) at each row
 * boundary, leaving the inter-row gap untouched. Row 0 is a RUN-24;
 * rows 1..23 are in-place-remap (no value change over sentinel). Confirm
 * row 0 content, the gap after it, and that row 1 begins at stride.
 * ---------------------------------------------------------------- */
static void test_remap_row_stride_advance(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e0;

    *p++ = CMD_RUN(TINT_W);        /* row 0: RUN 24 from src 0x02 */
    *p++ = 0x02u;
    put_remap_filler_rows(&p);     /* rows 1..23: in-place over sentinel */

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_remap((uint32)stream, (uint32)g_remap_dst,
                              TINT_STRIDE, (uint32)g_remap_lut);

    e0 = g_remap_lut[0x02u];
    for (i = 0; i < TINT_W; i++) {
        ASSERT_EQ(g_remap_dst[i], e0);
    }
    for (i = TINT_W; i < (int)TINT_STRIDE; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
    for (i = (int)TINT_STRIDE; i < (int)(TINT_STRIDE * TINT_H); i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ================================================================
 * fd2_tile_blit_24x24_with_remap_table @ 0x4DD52 — same RLE syntax and
 * LUT remap as fd2_tile_blit_24x24_remap above, but the 0xC0 command is
 * a TRUE transparent skip (advance dst, write nothing, consume no source
 * byte) instead of an in-place remap. So these tests reuse g_remap_dst /
 * g_remap_lut but use plain SKIP-24 (CMD_SKIP) filler rows.
 * ================================================================ */

/* Append (TINT_H - 1) transparent SKIP-24 filler rows (rows 1..23). */
static void put_remap_skip_filler_rows(uint8 **pp)
{
    int r;
    for (r = 1; r < TINT_H; r++) {
        *(*pp)++ = CMD_SKIP(TINT_W);
    }
}

/* ----------------------------------------------------------------
 * RUN mode (0x00..0x3F): fill n pixels from a SINGLE remapped source
 * byte; only one source byte consumed.
 * ---------------------------------------------------------------- */
static void test_remaptab_run_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = CMD_RUN(6);
    *p++ = 0x0Au;
    *p++ = CMD_SKIP(18);           /* 6 + 18 == 24 */
    put_remap_skip_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_with_remap_table((uint32)stream, (uint32)g_remap_dst,
                                         TINT_W, (uint32)g_remap_lut);

    e = g_remap_lut[0x0Au];
    for (i = 0; i < 6; i++) {
        ASSERT_EQ(g_remap_dst[i], e);
    }
    for (i = 6; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * LITERAL mode (0x80..0xBF): copy n remapped pixels, one source byte
 * each, into consecutive dst bytes.
 * ---------------------------------------------------------------- */
static void test_remaptab_literal_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = CMD_LIT(5);
    *p++ = 0x00u;
    *p++ = 0x01u;
    *p++ = 0x42u;
    *p++ = 0x7Fu;
    *p++ = 0xABu;
    *p++ = CMD_SKIP(19);           /* finish the 24-column row */
    put_remap_skip_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_with_remap_table((uint32)stream, (uint32)g_remap_dst,
                                         TINT_W, (uint32)g_remap_lut);

    ASSERT_EQ(g_remap_dst[0], g_remap_lut[0x00u]);
    ASSERT_EQ(g_remap_dst[1], g_remap_lut[0x01u]);
    ASSERT_EQ(g_remap_dst[2], g_remap_lut[0x42u]);
    ASSERT_EQ(g_remap_dst[3], g_remap_lut[0x7Fu]);
    ASSERT_EQ(g_remap_dst[4], g_remap_lut[0xABu]);
    for (i = 5; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * STRIDE-2 RUN (0x40..0x7F): write n remapped pixels from a single
 * source byte at every other dst byte. The asm is INC EDI; STOSB, so
 * pixels land at odd offsets 1,3,5,7 (dst += 2 per pixel, NOT +3), and
 * each pixel consumes TWO column-counts. Load-bearing test for the +2
 * stride (the decompiler renders this branch as +3).
 * ---------------------------------------------------------------- */
static void test_remaptab_stride2_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = CMD_STRIDE(4);          /* 4 pixels -> 8 column-counts */
    *p++ = 0x05u;
    *p++ = CMD_SKIP(16);           /* 8 + 16 == 24 */
    put_remap_skip_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_with_remap_table((uint32)stream, (uint32)g_remap_dst,
                                         TINT_W, (uint32)g_remap_lut);

    e = g_remap_lut[0x05u];
    ASSERT_EQ(g_remap_dst[0], TINT_SENT);
    ASSERT_EQ(g_remap_dst[1], e);
    ASSERT_EQ(g_remap_dst[2], TINT_SENT);
    ASSERT_EQ(g_remap_dst[3], e);
    ASSERT_EQ(g_remap_dst[4], TINT_SENT);
    ASSERT_EQ(g_remap_dst[5], e);
    ASSERT_EQ(g_remap_dst[6], TINT_SENT);
    ASSERT_EQ(g_remap_dst[7], e);
    /* dst is now at offset 8; the rest of the image stays sentinel */
    for (i = 8; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * TRANSPARENT SKIP (0xC0..0xFF): for n bytes at dst, advance the cursor
 * WITHOUT writing and WITHOUT consuming any source byte. This is the one
 * behavioural difference from fd2_tile_blit_24x24_remap (which would
 * remap the existing dst bytes). Pre-seed the skipped region with
 * distinct values and prove they survive untouched, then prove the byte
 * right after the SKIP command is read as a source byte by a following
 * RUN (i.e. SKIP consumed zero source bytes).
 * ---------------------------------------------------------------- */
static void test_remaptab_transparent_skip(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 seed[5];
    uint8 run_pixel;

    seed[0] = 0x00u;
    seed[1] = 0x10u;
    seed[2] = 0x55u;
    seed[3] = 0x80u;
    seed[4] = 0xFEu;

    *p++ = CMD_SKIP(5);            /* skip dst[0..4]: leave them as seeded */
    *p++ = CMD_RUN(19);            /* fill dst[5..23] from one src byte */
    *p++ = 0x2Au;                  /* <-- the RUN's source byte */
    put_remap_skip_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    for (i = 0; i < 5; i++) {
        g_remap_dst[i] = seed[i];  /* pre-seed the skipped region */
    }

    fd2_tile_blit_24x24_with_remap_table((uint32)stream, (uint32)g_remap_dst,
                                         TINT_W, (uint32)g_remap_lut);

    /* dst[0..4] are UNCHANGED: true transparent skip, not in-place remap. */
    for (i = 0; i < 5; i++) {
        ASSERT_EQ(g_remap_dst[i], seed[i]);
    }
    /* RUN read stream byte 0x2A (SKIP consumed no source byte). */
    run_pixel = g_remap_lut[0x2Au];
    for (i = 5; i < TINT_W; i++) {
        ASSERT_EQ(g_remap_dst[i], run_pixel);
    }
    for (i = TINT_W; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * Confirm the LUT is genuinely applied (output == table[src], not the
 * raw source value) across a few source bytes whose mapping differs from
 * the identity.
 * ---------------------------------------------------------------- */
static void test_remaptab_lut_is_applied(void)
{
    uint8 stream[64];
    uint8 *p = stream;

    *p++ = CMD_LIT(4);
    *p++ = 0x01u;                  /* remap -> 0x11 (!= 0x01) */
    *p++ = 0x20u;                  /* remap -> 0x30 */
    *p++ = 0xF0u;                  /* remap -> 0x00 (8-bit wrap) */
    *p++ = 0x7Eu;                  /* remap -> 0x8E */
    *p++ = CMD_SKIP(20);
    put_remap_skip_filler_rows(&p);

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_with_remap_table((uint32)stream, (uint32)g_remap_dst,
                                         TINT_W, (uint32)g_remap_lut);

    ASSERT_EQ(g_remap_dst[0], 0x11u);
    ASSERT_EQ(g_remap_dst[1], 0x30u);
    ASSERT_EQ(g_remap_dst[2], 0x00u);
    ASSERT_EQ(g_remap_dst[3], 0x8Eu);
    ASSERT_EQ(g_remap_dst[0] != 0x01u, 1);
    ASSERT_EQ(g_remap_dst[2] != 0xF0u, 1);
}

/* ----------------------------------------------------------------
 * Full 24x24 sprite, one LITERAL-24 command per row, stride 24 so the
 * destination is a contiguous 576-byte image. Source byte for row r,
 * col c is (r*24 + c). Exercises all 24 rows and the row-wrap reset.
 * ---------------------------------------------------------------- */
static void test_remaptab_full_24x24_row_wrap(void)
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

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_with_remap_table((uint32)stream, (uint32)g_remap_dst,
                                         TINT_W, (uint32)g_remap_lut);

    for (r = 0; r < TINT_H; r++) {
        for (c = 0; c < TINT_W; c++) {
            sv = (uint8)(r * TINT_W + c);
            ASSERT_EQ(g_remap_dst[r * TINT_W + c], g_remap_lut[sv]);
        }
    }
    ASSERT_EQ(g_remap_dst[TINT_W * TINT_H], TINT_SENT);
}

/* ----------------------------------------------------------------
 * Row stride > 24: the decoder jumps dst by (stride - 0x18) at each row
 * boundary, leaving the inter-row gap untouched. Row 0 is a RUN-24; rows
 * 1..23 are transparent SKIP-24. Confirm row 0 content, the gap after it,
 * and that row 1 would begin at stride (all sentinel here).
 * ---------------------------------------------------------------- */
static void test_remaptab_row_stride_advance(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e0;

    *p++ = CMD_RUN(TINT_W);        /* row 0: RUN 24 from src 0x02 */
    *p++ = 0x02u;
    put_remap_skip_filler_rows(&p);/* rows 1..23: transparent skip */

    remap_lut_init();
    remap_reset_dst();
    fd2_tile_blit_24x24_with_remap_table((uint32)stream, (uint32)g_remap_dst,
                                         TINT_STRIDE, (uint32)g_remap_lut);

    e0 = g_remap_lut[0x02u];
    for (i = 0; i < TINT_W; i++) {
        ASSERT_EQ(g_remap_dst[i], e0);
    }
    for (i = TINT_W; i < (int)TINT_STRIDE; i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
    for (i = (int)TINT_STRIDE; i < (int)(TINT_STRIDE * TINT_H); i++) {
        ASSERT_EQ(g_remap_dst[i], TINT_SENT);
    }
}

/* ================================================================
 * fd2_tile_blit_24x24_solid_color @ 0x4DDD7 — same 4-mode RLE syntax as
 * the blitters above, but every painted pixel is forced to a single
 * SILHOUETTE COLOR (the RLE source byte values are read to advance the
 * stream but their values are discarded). The third parameter is PACKED:
 * the row advance is (value - 0x18) and the fill colour is (uint8)value.
 * The 0xC0 command is a TRUE transparent skip (no write, no source).
 * Reuses the CMD_* builders and TINT_* constants above.
 * ================================================================ */

/* Destination for the solid-colour blit tests (same geometry). */
static uint8 g_solid_dst[TINT_STRIDE * (TINT_H + 1)];

/* Wider destination for the packed-colour test, which uses the real VGA
 * stride 0x140: 24 SKIP rows advance dst by a full stride each, so the
 * buffer must span all 24 rows even though only row 0 is written. */
static uint8 g_solid_dst_wide[0x140 * TINT_H];

static void solid_reset_dst(void)
{
    memset(g_solid_dst, TINT_SENT, sizeof(g_solid_dst));
}

/* Append (TINT_H - 1) transparent SKIP-24 filler rows (rows 1..23). */
static void put_solid_skip_filler_rows(uint8 **pp)
{
    int r;
    for (r = 1; r < TINT_H; r++) {
        *(*pp)++ = CMD_SKIP(TINT_W);
    }
}

/* ----------------------------------------------------------------
 * RUN mode (0x00..0x3F): consume ONE (ignored) source byte, paint n
 * COLOUR pixels contiguously. Load-bearing: the painted value is the
 * packed colour (low byte of param_3 == 0x18 here), NOT the source byte.
 * ---------------------------------------------------------------- */
static void test_solid_run_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = CMD_RUN(6);
    *p++ = 0x0Au;                  /* source byte: read but discarded */
    *p++ = CMD_SKIP(18);          /* 6 + 18 == 24 */
    put_solid_skip_filler_rows(&p);

    solid_reset_dst();
    /* stride 0x18 -> colour 0x18, row advance 0 (contiguous) */
    fd2_tile_blit_24x24_solid_color((uint32)stream, (uint32)g_solid_dst,
                                    TINT_W, 0u);

    for (i = 0; i < 6; i++) {
        ASSERT_EQ(g_solid_dst[i], (uint8)TINT_W);   /* colour, not 0x0A */
    }
    /* the source byte value never reached the destination */
    ASSERT_EQ(g_solid_dst[0] != 0x0Au, 1);
    for (i = 6; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_solid_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * LITERAL mode (0x80..0xBF): consume n source bytes (one per pixel) but
 * paint the COLOUR each time. Proves both that n source bytes are
 * consumed (stream advances so the trailing SKIP lands right) and that
 * their values are ignored (all painted pixels equal the colour).
 * ---------------------------------------------------------------- */
static void test_solid_literal_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = CMD_LIT(5);
    *p++ = 0x00u;
    *p++ = 0x11u;
    *p++ = 0x22u;
    *p++ = 0x33u;
    *p++ = 0x44u;                  /* 5 source bytes, all discarded */
    *p++ = CMD_SKIP(19);          /* 5 + 19 == 24 */
    put_solid_skip_filler_rows(&p);

    solid_reset_dst();
    fd2_tile_blit_24x24_solid_color((uint32)stream, (uint32)g_solid_dst,
                                    TINT_W, 0u);

    for (i = 0; i < 5; i++) {
        ASSERT_EQ(g_solid_dst[i], (uint8)TINT_W);   /* all == colour */
    }
    for (i = 5; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_solid_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * STRIDE-2 RUN (0x40..0x7F): consume ONE (ignored) source byte, paint n
 * COLOUR pixels at every other dst byte. The asm is INC EDI; STOSB, so
 * pixels land at odd offsets 1,3,5,7 (dst += 2 per pixel, NOT +3), and
 * each pixel consumes TWO column-counts. Load-bearing test for the +2
 * stride (the decompiler renders this branch as +3).
 * ---------------------------------------------------------------- */
static void test_solid_stride2_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = CMD_STRIDE(4);          /* 4 pixels -> 8 column-counts */
    *p++ = 0x05u;                  /* source byte: discarded */
    *p++ = CMD_SKIP(16);          /* 8 + 16 == 24 */
    put_solid_skip_filler_rows(&p);

    solid_reset_dst();
    fd2_tile_blit_24x24_solid_color((uint32)stream, (uint32)g_solid_dst,
                                    TINT_W, 0u);

    e = (uint8)TINT_W;             /* colour == low byte of stride 0x18 */
    ASSERT_EQ(g_solid_dst[0], TINT_SENT);
    ASSERT_EQ(g_solid_dst[1], e);
    ASSERT_EQ(g_solid_dst[2], TINT_SENT);
    ASSERT_EQ(g_solid_dst[3], e);
    ASSERT_EQ(g_solid_dst[4], TINT_SENT);
    ASSERT_EQ(g_solid_dst[5], e);
    ASSERT_EQ(g_solid_dst[6], TINT_SENT);
    ASSERT_EQ(g_solid_dst[7], e);
    /* dst is now at offset 8; the rest of the image stays sentinel */
    for (i = 8; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_solid_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * TRANSPARENT SKIP (0xC0..0xFF): advance dst by n with no write and no
 * source byte consumed. Pre-seed the skipped region with distinct values
 * and prove they survive, then prove the byte right after the SKIP is
 * read as a source byte by a following RUN (SKIP consumed zero source).
 * ---------------------------------------------------------------- */
static void test_solid_transparent_skip(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 seed[5];

    seed[0] = 0x00u;
    seed[1] = 0x10u;
    seed[2] = 0x55u;
    seed[3] = 0x80u;
    seed[4] = 0xFEu;

    *p++ = CMD_SKIP(5);           /* skip dst[0..4]: leave them seeded */
    *p++ = CMD_RUN(19);           /* fill dst[5..23] with colour */
    *p++ = 0x2Au;                 /* the RUN's (ignored) source byte */
    put_solid_skip_filler_rows(&p);

    solid_reset_dst();
    for (i = 0; i < 5; i++) {
        g_solid_dst[i] = seed[i];
    }

    fd2_tile_blit_24x24_solid_color((uint32)stream, (uint32)g_solid_dst,
                                    TINT_W, 0u);

    /* dst[0..4] unchanged: true transparent skip */
    for (i = 0; i < 5; i++) {
        ASSERT_EQ(g_solid_dst[i], seed[i]);
    }
    /* RUN painted colour over dst[5..23]; SKIP consumed no source byte so
     * the RUN read its source from the byte right after CMD_RUN(19). */
    for (i = 5; i < TINT_W; i++) {
        ASSERT_EQ(g_solid_dst[i], (uint8)TINT_W);
    }
    for (i = TINT_W; i < TINT_W * TINT_H; i++) {
        ASSERT_EQ(g_solid_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * Packed stride+colour: the fill colour is the LOW BYTE of param_3 while
 * the value as a whole is the row stride. Pass 0x140 (typical VGA stride)
 * and confirm every painted pixel is 0x40 (= 0x140 & 0xFF, the white-
 * silhouette band) — distinct from both the source bytes and SENT.
 * ---------------------------------------------------------------- */
static void test_solid_packed_color_from_low_byte(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = CMD_LIT(4);
    *p++ = 0x01u;
    *p++ = 0x02u;
    *p++ = 0x03u;
    *p++ = 0x04u;
    *p++ = CMD_SKIP(20);          /* 4 + 20 == 24 */
    put_solid_skip_filler_rows(&p);

    memset(g_solid_dst_wide, TINT_SENT, sizeof(g_solid_dst_wide));
    /* stride 0x140 -> colour 0x40, row advance 0x128 (real VGA layout) */
    fd2_tile_blit_24x24_solid_color((uint32)stream, (uint32)g_solid_dst_wide,
                                    0x140u, 0u);

    for (i = 0; i < 4; i++) {
        ASSERT_EQ(g_solid_dst_wide[i], 0x40u);   /* 0x140 & 0xFF */
    }
    /* not the source bytes */
    ASSERT_EQ(g_solid_dst_wide[0] != 0x01u, 1);
    ASSERT_EQ(g_solid_dst_wide[3] != 0x04u, 1);
    /* rows 1..23 are SKIP-only, so the entire rest of the surface stays
     * sentinel (the row advance never causes a stray write). */
    for (i = 4; i < (int)sizeof(g_solid_dst_wide); i++) {
        ASSERT_EQ(g_solid_dst_wide[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * Row stride > 24: the decoder jumps dst by (stride - 0x18) at each row
 * boundary, leaving the inter-row gap untouched. With stride 0x40 the
 * colour is 0x40 and the advance is 0x28. Row 0 is a RUN-24; rows 1..23
 * are transparent SKIP-24. Confirm row 0 colour, the gap after it, and
 * that row 1 would begin at stride (all sentinel here).
 * ---------------------------------------------------------------- */
static void test_solid_row_stride_advance(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = CMD_RUN(TINT_W);        /* row 0: RUN 24 (colour fill) */
    *p++ = 0x02u;                  /* ignored source byte */
    put_solid_skip_filler_rows(&p);/* rows 1..23: transparent skip */

    solid_reset_dst();
    /* stride 0x40 -> colour 0x40, row advance 0x28 */
    fd2_tile_blit_24x24_solid_color((uint32)stream, (uint32)g_solid_dst,
                                    TINT_STRIDE, 0u);

    for (i = 0; i < TINT_W; i++) {
        ASSERT_EQ(g_solid_dst[i], (uint8)TINT_STRIDE);   /* 0x40 */
    }
    for (i = TINT_W; i < (int)TINT_STRIDE; i++) {
        ASSERT_EQ(g_solid_dst[i], TINT_SENT);
    }
    for (i = (int)TINT_STRIDE; i < (int)(TINT_STRIDE * TINT_H); i++) {
        ASSERT_EQ(g_solid_dst[i], TINT_SENT);
    }
}

/* ----------------------------------------------------------------
 * Full 24x24 sprite, one LITERAL-24 command per row, stride 24 so the
 * destination is a contiguous 576-byte image and the colour is 0x18.
 * Every source byte differs (r*24 + c) yet every output pixel must be
 * the single colour. Exercises all 24 rows and the row-wrap reset.
 * ---------------------------------------------------------------- */
static void test_solid_full_24x24_row_wrap(void)
{
    uint8 stream[TINT_H * (1 + TINT_W)];
    uint8 *p = stream;
    int r, c;

    for (r = 0; r < TINT_H; r++) {
        *p++ = CMD_LIT(TINT_W);
        for (c = 0; c < TINT_W; c++) {
            *p++ = (uint8)(r * TINT_W + c);
        }
    }

    solid_reset_dst();
    fd2_tile_blit_24x24_solid_color((uint32)stream, (uint32)g_solid_dst,
                                    TINT_W, 0u);

    for (r = 0; r < TINT_H; r++) {
        for (c = 0; c < TINT_W; c++) {
            ASSERT_EQ(g_solid_dst[r * TINT_W + c], (uint8)TINT_W);
        }
    }
    ASSERT_EQ(g_solid_dst[TINT_W * TINT_H], TINT_SENT);
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
    RUN_TEST(test_remap_run_mode);
    RUN_TEST(test_remap_literal_mode);
    RUN_TEST(test_remap_stride2_mode);
    RUN_TEST(test_remap_inplace_remap_mode);
    RUN_TEST(test_remap_lut_is_applied);
    RUN_TEST(test_remap_full_24x24_row_wrap);
    RUN_TEST(test_remap_row_stride_advance);
    RUN_TEST(test_remaptab_run_mode);
    RUN_TEST(test_remaptab_literal_mode);
    RUN_TEST(test_remaptab_stride2_mode);
    RUN_TEST(test_remaptab_transparent_skip);
    RUN_TEST(test_remaptab_lut_is_applied);
    RUN_TEST(test_remaptab_full_24x24_row_wrap);
    RUN_TEST(test_remaptab_row_stride_advance);
    RUN_TEST(test_solid_run_mode);
    RUN_TEST(test_solid_literal_mode);
    RUN_TEST(test_solid_stride2_mode);
    RUN_TEST(test_solid_transparent_skip);
    RUN_TEST(test_solid_packed_color_from_low_byte);
    RUN_TEST(test_solid_row_stride_advance);
    RUN_TEST(test_solid_full_24x24_row_wrap);
    printf("\n");
}
