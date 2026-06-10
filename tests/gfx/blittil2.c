/*
 * unit tests for src/gfx/blittile.c (part 3)
 *
 * Covers fd2_tile_blit_24x24_dimmed_grayscale @ 0x4DE56, the hand-written
 * RLE blit that remaps every painted pixel into the fixed 8-step grayscale
 * band: out = (src_pixel & 7) + 0x18. It is the off=0 / base=0x18 special
 * case of fd2_tile_blit_24x24_with_tint_offset, so it shares the same
 * 4-mode RLE command syntax.
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
#define GCMD_RUN(n)     ((uint8)(0x00u | ((n) - 1)))   /* fill n from 1 src byte */
#define GCMD_STRIDE(n)  ((uint8)(0x40u | ((n) - 1)))   /* n pixels, every other  */
#define GCMD_LIT(n)     ((uint8)(0x80u | ((n) - 1)))   /* copy n, 1 src byte each */
#define GCMD_SKIP(n)    ((uint8)(0xC0u | ((n) - 1)))   /* advance n (transparent) */

#define GRAY_W      24          /* sprite is 24x24 */
#define GRAY_H      24
#define GRAY_STRIDE 0x40u       /* generous dst row stride for stride tests */
#define GRAY_SENT   0xEEu       /* sentinel for untouched dst bytes         */

/* Destination big enough for 24 rows at the widest stride used here. */
static uint8 g_gray_dst[GRAY_STRIDE * (GRAY_H + 1)];

/* The reference pixel transform, computed in 8-bit exactly as the asm does:
 * AND AL,7 ; ADD AL,0x18. Base 0x18 and the &7 mask are hardcoded. */
static uint8 gray_of(uint8 src)
{
    return (uint8)((src & 7u) + 0x18u);
}

static void gray_reset_dst(void)
{
    memset(g_gray_dst, GRAY_SENT, sizeof(g_gray_dst));
}

/* Append a full transparent (SKIP-24) row to the stream at *pp. */
static void put_gskip_row(uint8 **pp)
{
    *(*pp)++ = GCMD_SKIP(GRAY_W);
}

/* Append 23 filler SKIP rows (rows 1..23) after the row under test. */
static void put_gfiller_rows(uint8 **pp)
{
    int r;
    for (r = 1; r < GRAY_H; r++) {
        put_gskip_row(pp);
    }
}

/* ----------------------------------------------------------------
 * LITERAL mode (0x80..0xBF): copy n grayscale pixels, one source byte
 * each, into consecutive dst bytes. Verify the per-pixel transform and
 * that exactly n bytes were written; the rest of row 0 (SKIP) and the
 * filler rows stay untouched.
 * ---------------------------------------------------------------- */
static void test_gray_literal_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_LIT(5);            /* 5 literal pixels */
    *p++ = 0x00u;
    *p++ = 0x01u;
    *p++ = 0x09u;                  /* 0x09 & 7 == 1 */
    *p++ = 0x07u;
    *p++ = 0xFFu;                  /* 0xFF & 7 == 7 -> 0x1F */
    *p++ = GCMD_SKIP(19);         /* finish the 24-column row */
    put_gfiller_rows(&p);

    gray_reset_dst();
    fd2_tile_blit_24x24_dimmed_grayscale((uint32)stream, (uint32)g_gray_dst,
                                         GRAY_W);

    ASSERT_EQ(g_gray_dst[0], gray_of(0x00u));   /* 0x18 */
    ASSERT_EQ(g_gray_dst[1], gray_of(0x01u));   /* 0x19 */
    ASSERT_EQ(g_gray_dst[2], gray_of(0x09u));   /* 0x19 */
    ASSERT_EQ(g_gray_dst[3], gray_of(0x07u));   /* 0x1F */
    ASSERT_EQ(g_gray_dst[4], gray_of(0xFFu));   /* 0x1F */
    /* columns 5..23 were SKIP, and every filler row (24..) untouched */
    for (i = 5; i < GRAY_W * GRAY_H; i++) {
        ASSERT_EQ(g_gray_dst[i], GRAY_SENT);
    }
}

/* ----------------------------------------------------------------
 * RUN mode (0x00..0x3F): fill n grayscale pixels from a SINGLE source
 * byte at consecutive dst positions; only one source byte consumed.
 * ---------------------------------------------------------------- */
static void test_gray_run_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = GCMD_RUN(6);            /* fill 6 from one src byte */
    *p++ = 0x0Au;                  /* 0x0A & 7 == 2 -> 0x1A */
    *p++ = GCMD_SKIP(18);         /* 6 + 18 == 24 */
    put_gfiller_rows(&p);

    gray_reset_dst();
    fd2_tile_blit_24x24_dimmed_grayscale((uint32)stream, (uint32)g_gray_dst,
                                         GRAY_W);

    e = gray_of(0x0Au);
    ASSERT_EQ(e, 0x1Au);
    for (i = 0; i < 6; i++) {
        ASSERT_EQ(g_gray_dst[i], e);
    }
    for (i = 6; i < GRAY_W * GRAY_H; i++) {
        ASSERT_EQ(g_gray_dst[i], GRAY_SENT);
    }
}

/* ----------------------------------------------------------------
 * STRIDE-2 RUN (0x40..0x7F): write n grayscale pixels from a single
 * source byte, spaced every other dst byte (dst += 2 per pixel). Each
 * pixel consumes TWO column-counts, so a stride run of n covers 2n
 * columns. INC EDI precedes each STOSB, so pixels land at the ODD
 * offsets 1,3,5,7 and the even offsets stay untouched.
 * ---------------------------------------------------------------- */
static void test_gray_stride2_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e;

    *p++ = GCMD_STRIDE(4);         /* 4 pixels -> 8 column-counts */
    *p++ = 0x05u;                  /* 0x05 & 7 == 5 -> 0x1D */
    *p++ = GCMD_SKIP(16);         /* 8 + 16 == 24 */
    put_gfiller_rows(&p);

    gray_reset_dst();
    fd2_tile_blit_24x24_dimmed_grayscale((uint32)stream, (uint32)g_gray_dst,
                                         GRAY_W);

    e = gray_of(0x05u);
    ASSERT_EQ(e, 0x1Du);
    ASSERT_EQ(g_gray_dst[0], GRAY_SENT);
    ASSERT_EQ(g_gray_dst[1], e);
    ASSERT_EQ(g_gray_dst[2], GRAY_SENT);
    ASSERT_EQ(g_gray_dst[3], e);
    ASSERT_EQ(g_gray_dst[4], GRAY_SENT);
    ASSERT_EQ(g_gray_dst[5], e);
    ASSERT_EQ(g_gray_dst[6], GRAY_SENT);
    ASSERT_EQ(g_gray_dst[7], e);
    /* dst is now at offset 8; SKIP 16 -> offset 24, nothing else written. */
    for (i = 8; i < GRAY_W * GRAY_H; i++) {
        ASSERT_EQ(g_gray_dst[i], GRAY_SENT);
    }
}

/* ----------------------------------------------------------------
 * SKIP (0xC0..0xFF): advance dst by n with no writes (transparent),
 * consuming no source bytes. Confirm dst lands correctly for a
 * following command on the same row.
 * ---------------------------------------------------------------- */
static void test_gray_skip_then_literal(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_SKIP(10);         /* leave cols 0..9 transparent */
    *p++ = GCMD_LIT(1);           /* one literal pixel at col 10 */
    *p++ = 0x13u;                 /* 0x13 & 7 == 3 -> 0x1B */
    *p++ = GCMD_SKIP(13);        /* 10 + 1 + 13 == 24 */
    put_gfiller_rows(&p);

    gray_reset_dst();
    fd2_tile_blit_24x24_dimmed_grayscale((uint32)stream, (uint32)g_gray_dst,
                                         GRAY_W);

    for (i = 0; i < 10; i++) {
        ASSERT_EQ(g_gray_dst[i], GRAY_SENT);
    }
    ASSERT_EQ(g_gray_dst[10], gray_of(0x13u));
    for (i = 11; i < GRAY_W * GRAY_H; i++) {
        ASSERT_EQ(g_gray_dst[i], GRAY_SENT);
    }
}

/* ----------------------------------------------------------------
 * Pixel transform: the band is always 0x18..0x1F regardless of the
 * source byte's high bits, because only the low 3 bits survive the
 * mask. Verify the full 8-entry mapping plus a couple of high-bit
 * sources that fold back into the same band.
 * ---------------------------------------------------------------- */
static void test_gray_band_mapping(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_LIT(8);
    *p++ = 0x00u;                 /* -> 0x18 */
    *p++ = 0x01u;                 /* -> 0x19 */
    *p++ = 0x02u;                 /* -> 0x1A */
    *p++ = 0x03u;                 /* -> 0x1B */
    *p++ = 0x04u;                 /* -> 0x1C */
    *p++ = 0x05u;                 /* -> 0x1D */
    *p++ = 0x06u;                 /* -> 0x1E */
    *p++ = 0x07u;                 /* -> 0x1F */
    *p++ = GCMD_LIT(2);
    *p++ = 0xF8u;                 /* 0xF8 & 7 == 0 -> 0x18 */
    *p++ = 0x9Du;                 /* 0x9D & 7 == 5 -> 0x1D */
    *p++ = GCMD_SKIP(14);        /* 8 + 2 + 14 == 24 */
    put_gfiller_rows(&p);

    gray_reset_dst();
    fd2_tile_blit_24x24_dimmed_grayscale((uint32)stream, (uint32)g_gray_dst,
                                         GRAY_W);

    for (i = 0; i < 8; i++) {
        ASSERT_EQ(g_gray_dst[i], (uint8)(0x18u + i));
    }
    ASSERT_EQ(g_gray_dst[8], 0x18u);
    ASSERT_EQ(g_gray_dst[9], 0x1Du);
    for (i = 10; i < GRAY_W * GRAY_H; i++) {
        ASSERT_EQ(g_gray_dst[i], GRAY_SENT);
    }
}

/* ----------------------------------------------------------------
 * Full 24x24 sprite, one LITERAL-24 command per row, stride 24 so the
 * destination is a contiguous 576-byte image. Source byte for row r,
 * col c is (r*24 + c). Exercises all 24 rows and the row-wrap reset,
 * and confirms the byte just past the image is untouched.
 * ---------------------------------------------------------------- */
static void test_gray_full_24x24_row_wrap(void)
{
    uint8 stream[GRAY_H * (1 + GRAY_W)];
    uint8 *p = stream;
    int r, c;
    uint8 sv;

    for (r = 0; r < GRAY_H; r++) {
        *p++ = GCMD_LIT(GRAY_W);
        for (c = 0; c < GRAY_W; c++) {
            *p++ = (uint8)(r * GRAY_W + c);
        }
    }

    gray_reset_dst();
    fd2_tile_blit_24x24_dimmed_grayscale((uint32)stream, (uint32)g_gray_dst,
                                         GRAY_W);

    for (r = 0; r < GRAY_H; r++) {
        for (c = 0; c < GRAY_W; c++) {
            sv = (uint8)(r * GRAY_W + c);
            ASSERT_EQ(g_gray_dst[r * GRAY_W + c], gray_of(sv));
        }
    }
    ASSERT_EQ(g_gray_dst[GRAY_W * GRAY_H], GRAY_SENT);
}

/* ----------------------------------------------------------------
 * Row stride > 24: the decoder must jump dst by (stride - 0x18) at each
 * row boundary, leaving the inter-row gap untouched. Row 0 is a RUN-24,
 * rows 1..23 are SKIP-24 (which still advance dst by a full stride per
 * row). Confirm row 0 content, the gap after it, and that the next row
 * would begin exactly at offset == stride.
 * ---------------------------------------------------------------- */
static void test_gray_row_stride_advance(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;
    uint8 e0;

    *p++ = GCMD_RUN(GRAY_W);       /* row 0: RUN 24 from src 0x02 */
    *p++ = 0x02u;                  /* -> 0x1A */
    put_gfiller_rows(&p);          /* rows 1..23: SKIP 24 each */

    gray_reset_dst();
    fd2_tile_blit_24x24_dimmed_grayscale((uint32)stream, (uint32)g_gray_dst,
                                         GRAY_STRIDE);

    e0 = gray_of(0x02u);
    for (i = 0; i < GRAY_W; i++) {
        ASSERT_EQ(g_gray_dst[i], e0);
    }
    /* gap between row 0 columns and the next row start (24..stride-1) */
    for (i = GRAY_W; i < (int)GRAY_STRIDE; i++) {
        ASSERT_EQ(g_gray_dst[i], GRAY_SENT);
    }
    /* every following row is SKIP-only, so the whole rest stays sentinel,
     * including the byte at offset == stride where row 1 begins */
    for (i = (int)GRAY_STRIDE; i < (int)(GRAY_STRIDE * GRAY_H); i++) {
        ASSERT_EQ(g_gray_dst[i], GRAY_SENT);
    }
}

/* ================================================================
 * fd2_tile_blit_24x24_passthrough @ 0x4DEDA
 *
 * The base member of the family: no palette transform, the source byte
 * is written to the destination as-is. Same 4-mode RLE command syntax
 * as the grayscale sibling above, so these tests mirror those but with
 * the identity pixel mapping. The decoder always processes 24 rows of
 * 24 columns, so each stream is a complete 24-row program; filler rows
 * after the row under test are a single SKIP-24 command.
 * ================================================================ */

#define PASS_W      24          /* sprite is 24x24 */
#define PASS_H      24
#define PASS_STRIDE 0x40u       /* generous dst row stride for stride tests */
#define PASS_SENT   0xEEu       /* sentinel for untouched dst bytes         */

/* Destination big enough for 24 rows at the widest stride used here. */
static uint8 g_pass_dst[PASS_STRIDE * (PASS_H + 1)];

static void pass_reset_dst(void)
{
    memset(g_pass_dst, PASS_SENT, sizeof(g_pass_dst));
}

/* Append 23 filler SKIP rows (rows 1..23) after the row under test. */
static void put_pass_filler_rows(uint8 **pp)
{
    int r;
    for (r = 1; r < PASS_H; r++) {
        *(*pp)++ = GCMD_SKIP(PASS_W);
    }
}

/* ----------------------------------------------------------------
 * LITERAL mode (0x80..0xBF): copy n source bytes verbatim, one per dst
 * byte. Verify the bytes land unchanged and that exactly n are written.
 * ---------------------------------------------------------------- */
static void test_pass_literal_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_LIT(5);
    *p++ = 0x00u;
    *p++ = 0x42u;
    *p++ = 0x9Du;
    *p++ = 0x07u;
    *p++ = 0xFFu;
    *p++ = GCMD_SKIP(19);         /* 5 + 19 == 24 */
    put_pass_filler_rows(&p);

    pass_reset_dst();
    fd2_tile_blit_24x24_passthrough((uint32)stream, (uint32)g_pass_dst, PASS_W);

    ASSERT_EQ(g_pass_dst[0], 0x00u);
    ASSERT_EQ(g_pass_dst[1], 0x42u);
    ASSERT_EQ(g_pass_dst[2], 0x9Du);
    ASSERT_EQ(g_pass_dst[3], 0x07u);
    ASSERT_EQ(g_pass_dst[4], 0xFFu);
    for (i = 5; i < PASS_W * PASS_H; i++) {
        ASSERT_EQ(g_pass_dst[i], PASS_SENT);
    }
}

/* ----------------------------------------------------------------
 * RUN mode (0x00..0x3F): fill n dst bytes from a SINGLE source byte at
 * consecutive positions; only one source byte consumed.
 * ---------------------------------------------------------------- */
static void test_pass_run_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_RUN(6);
    *p++ = 0xA5u;                  /* written as-is, no transform */
    *p++ = GCMD_SKIP(18);         /* 6 + 18 == 24 */
    put_pass_filler_rows(&p);

    pass_reset_dst();
    fd2_tile_blit_24x24_passthrough((uint32)stream, (uint32)g_pass_dst, PASS_W);

    for (i = 0; i < 6; i++) {
        ASSERT_EQ(g_pass_dst[i], 0xA5u);
    }
    for (i = 6; i < PASS_W * PASS_H; i++) {
        ASSERT_EQ(g_pass_dst[i], PASS_SENT);
    }
}

/* ----------------------------------------------------------------
 * STRIDE-2 RUN (0x40..0x7F): write n pixels from a single source byte,
 * spaced every other dst byte (dst += 2 per pixel). The asm is
 * INC EDI; STOSB, so each pixel lands at the ODD offset 1,3,5,7 and the
 * even offsets stay untouched; the run of n covers 2n columns. This is
 * the EAX-bug-prone mode the Ghidra decompiler mis-rendered as +3, so
 * the explicit odd-offset assertions are the load-bearing check.
 * ---------------------------------------------------------------- */
static void test_pass_stride2_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_STRIDE(4);         /* 4 pixels -> 8 column-counts */
    *p++ = 0x77u;
    *p++ = GCMD_SKIP(16);         /* 8 + 16 == 24 */
    put_pass_filler_rows(&p);

    pass_reset_dst();
    fd2_tile_blit_24x24_passthrough((uint32)stream, (uint32)g_pass_dst, PASS_W);

    ASSERT_EQ(g_pass_dst[0], PASS_SENT);
    ASSERT_EQ(g_pass_dst[1], 0x77u);
    ASSERT_EQ(g_pass_dst[2], PASS_SENT);
    ASSERT_EQ(g_pass_dst[3], 0x77u);
    ASSERT_EQ(g_pass_dst[4], PASS_SENT);
    ASSERT_EQ(g_pass_dst[5], 0x77u);
    ASSERT_EQ(g_pass_dst[6], PASS_SENT);
    ASSERT_EQ(g_pass_dst[7], 0x77u);
    /* dst is now at offset 8; SKIP 16 -> offset 24, nothing else written. */
    for (i = 8; i < PASS_W * PASS_H; i++) {
        ASSERT_EQ(g_pass_dst[i], PASS_SENT);
    }
}

/* ----------------------------------------------------------------
 * SKIP (0xC0..0xFF): advance dst by n with no writes (transparent),
 * consuming no source bytes. Confirm dst lands correctly for a
 * following command on the same row.
 * ---------------------------------------------------------------- */
static void test_pass_skip_then_literal(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_SKIP(10);         /* leave cols 0..9 transparent */
    *p++ = GCMD_LIT(1);           /* one literal pixel at col 10 */
    *p++ = 0x3Cu;
    *p++ = GCMD_SKIP(13);        /* 10 + 1 + 13 == 24 */
    put_pass_filler_rows(&p);

    pass_reset_dst();
    fd2_tile_blit_24x24_passthrough((uint32)stream, (uint32)g_pass_dst, PASS_W);

    for (i = 0; i < 10; i++) {
        ASSERT_EQ(g_pass_dst[i], PASS_SENT);
    }
    ASSERT_EQ(g_pass_dst[10], 0x3Cu);
    for (i = 11; i < PASS_W * PASS_H; i++) {
        ASSERT_EQ(g_pass_dst[i], PASS_SENT);
    }
}

/* ----------------------------------------------------------------
 * Mixed-mode row: all four modes in a single 24-column row, to confirm
 * the dst cursor stays in sync as modes alternate. Column-count budget:
 *   RUN 3 (3) + STRIDE 2 (2*2=4) + LITERAL 2 (2) + SKIP 15 (15) == 24.
 * After RUN 3 dst is at offset 3; STRIDE 2 does INC then store twice, so
 * it writes at offsets 4 and 6, leaving 3 and 5 untouched; LITERAL 2
 * then writes offsets 7 and 8.
 * ---------------------------------------------------------------- */
static void test_pass_mixed_modes_row(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_RUN(3);    *p++ = 0x11u;           /* cols 0,1,2 = 0x11 */
    *p++ = GCMD_STRIDE(2); *p++ = 0x22u;           /* writes at off 4,6 */
    *p++ = GCMD_LIT(2);    *p++ = 0x33u; *p++ = 0x44u; /* off 7,8 */
    *p++ = GCMD_SKIP(15);                          /* 3+4+2+15 == 24 */
    put_pass_filler_rows(&p);

    pass_reset_dst();
    fd2_tile_blit_24x24_passthrough((uint32)stream, (uint32)g_pass_dst, PASS_W);

    ASSERT_EQ(g_pass_dst[0], 0x11u);
    ASSERT_EQ(g_pass_dst[1], 0x11u);
    ASSERT_EQ(g_pass_dst[2], 0x11u);
    ASSERT_EQ(g_pass_dst[3], PASS_SENT);   /* stride-2 starts with INC */
    ASSERT_EQ(g_pass_dst[4], 0x22u);
    ASSERT_EQ(g_pass_dst[5], PASS_SENT);
    ASSERT_EQ(g_pass_dst[6], 0x22u);
    ASSERT_EQ(g_pass_dst[7], 0x33u);
    ASSERT_EQ(g_pass_dst[8], 0x44u);
    for (i = 9; i < PASS_W * PASS_H; i++) {
        ASSERT_EQ(g_pass_dst[i], PASS_SENT);
    }
}

/* ----------------------------------------------------------------
 * Full 24x24 sprite, one LITERAL-24 command per row, stride 24 so the
 * destination is a contiguous 576-byte image. Source byte for row r,
 * col c is (r*24 + c). Exercises all 24 rows and the row-wrap reset,
 * and confirms the byte just past the image is untouched. Passthrough
 * means dst must equal the source bytes exactly.
 * ---------------------------------------------------------------- */
static void test_pass_full_24x24_row_wrap(void)
{
    uint8 stream[PASS_H * (1 + PASS_W)];
    uint8 *p = stream;
    int r, c;

    for (r = 0; r < PASS_H; r++) {
        *p++ = GCMD_LIT(PASS_W);
        for (c = 0; c < PASS_W; c++) {
            *p++ = (uint8)(r * PASS_W + c);
        }
    }

    pass_reset_dst();
    fd2_tile_blit_24x24_passthrough((uint32)stream, (uint32)g_pass_dst, PASS_W);

    for (r = 0; r < PASS_H; r++) {
        for (c = 0; c < PASS_W; c++) {
            ASSERT_EQ(g_pass_dst[r * PASS_W + c], (uint8)(r * PASS_W + c));
        }
    }
    ASSERT_EQ(g_pass_dst[PASS_W * PASS_H], PASS_SENT);
}

/* ----------------------------------------------------------------
 * Row stride > 24: the decoder must jump dst by (stride - 0x18) at each
 * row boundary, leaving the inter-row gap untouched. Row 0 is a RUN-24,
 * rows 1..23 are SKIP-24. Confirm row 0 content, the gap after it, and
 * that nothing past the row is written.
 * ---------------------------------------------------------------- */
static void test_pass_row_stride_advance(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_RUN(PASS_W);       /* row 0: RUN 24 from src 0x5A */
    *p++ = 0x5Au;
    put_pass_filler_rows(&p);      /* rows 1..23: SKIP 24 each */

    pass_reset_dst();
    fd2_tile_blit_24x24_passthrough((uint32)stream, (uint32)g_pass_dst,
                                    PASS_STRIDE);

    for (i = 0; i < PASS_W; i++) {
        ASSERT_EQ(g_pass_dst[i], 0x5Au);
    }
    for (i = PASS_W; i < (int)PASS_STRIDE; i++) {
        ASSERT_EQ(g_pass_dst[i], PASS_SENT);
    }
    for (i = (int)PASS_STRIDE; i < (int)(PASS_STRIDE * PASS_H); i++) {
        ASSERT_EQ(g_pass_dst[i], PASS_SENT);
    }
}

/* ================================================================
 * fd2_tile_blit_24x24_with_dialog_bg_fill @ 0x4DF4C
 *
 * Identical to the passthrough blitter above in RUN / STRIDE-2 RUN /
 * LITERAL, but the 0xC0..0xFF command FILLS the run with the constant
 * dialog-background colour 0x49 (no source bytes consumed) instead of
 * leaving the destination untouched. These tests mirror the passthrough
 * ones, with the load-bearing difference being that the 0xC0..0xFF run
 * is now an active write of 0x49. The decoder always processes 24 rows
 * of 24 columns, so each stream is a complete 24-row program; filler
 * rows after the row under test are a single 0xC0..0xFF command, which
 * for THIS routine paints a whole row of 0x49 (the test destinations are
 * sized for that and the assertions account for it).
 * ================================================================ */

#define DLG_W       24          /* sprite is 24x24 */
#define DLG_H       24
#define DLG_STRIDE  0x40u       /* generous dst row stride for stride tests */
#define DLG_SENT    0xEEu       /* sentinel for untouched dst bytes         */
#define DLG_BG      0x49u       /* the constant dialog-background fill colour */
#define DLG_FILL(n) ((uint8)(0xC0u | ((n) - 1)))  /* fill n with 0x49 */

/* Destination big enough for 24 rows at the widest stride used here. */
static uint8 g_dlg_dst[DLG_STRIDE * (DLG_H + 1)];

static void dlg_reset_dst(void)
{
    memset(g_dlg_dst, DLG_SENT, sizeof(g_dlg_dst));
}

/* Append rows 1..23 as full-width FILL rows. For this routine a
 * 0xC0..0xFF command paints 0x49, so each filler row writes 24 bytes of
 * DLG_BG starting at that row's stride offset. Used when the test only
 * cares about row 0 and treats the rest as "known background". */
static void put_dlg_filler_rows_fill(uint8 **pp)
{
    int r;
    for (r = 1; r < DLG_H; r++) {
        *(*pp)++ = DLG_FILL(DLG_W);
    }
}

/* ----------------------------------------------------------------
 * DIALOG-BG FILL mode (0xC0..0xFF): the defining difference from the
 * passthrough sibling. A 0xC0..0xFF run writes the constant 0x49 for its
 * length, consuming NO source bytes, then a following same-row command
 * resumes at the advanced dst. Verify the fill bytes are exactly 0x49
 * and that a literal after it lands at the right column.
 * ---------------------------------------------------------------- */
static void test_dlg_fill_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = DLG_FILL(10);          /* cols 0..9 filled with 0x49 */
    *p++ = GCMD_LIT(1);           /* one literal pixel at col 10 */
    *p++ = 0x3Cu;
    *p++ = DLG_FILL(13);          /* cols 11..23 filled with 0x49 */
    put_dlg_filler_rows_fill(&p);

    dlg_reset_dst();
    fd2_tile_blit_24x24_with_dialog_bg_fill((uint32)stream, (uint32)g_dlg_dst,
                                            DLG_W);

    for (i = 0; i < 10; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);     /* fill wrote 0x49, not skip */
    }
    ASSERT_EQ(g_dlg_dst[10], 0x3Cu);         /* literal pixel verbatim */
    for (i = 11; i < DLG_W; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);     /* trailing fill = 0x49 */
    }
    /* rows 1..23 are full FILL rows of 0x49, contiguous at stride 24 */
    for (i = DLG_W; i < DLG_W * DLG_H; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);
    }
    /* byte just past the 24x24 image stays untouched */
    ASSERT_EQ(g_dlg_dst[DLG_W * DLG_H], DLG_SENT);
}

/* ----------------------------------------------------------------
 * LITERAL mode (0x80..0xBF): copy n source bytes verbatim, one per dst
 * byte (unchanged from passthrough). The remainder of row 0 is filled
 * with 0x49 (not skipped), so columns 5..23 must read back as 0x49.
 * ---------------------------------------------------------------- */
static void test_dlg_literal_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_LIT(5);
    *p++ = 0x00u;
    *p++ = 0x42u;
    *p++ = 0x9Du;
    *p++ = 0x07u;
    *p++ = 0xFFu;
    *p++ = DLG_FILL(19);          /* 5 + 19 == 24, trailing cols -> 0x49 */
    put_dlg_filler_rows_fill(&p);

    dlg_reset_dst();
    fd2_tile_blit_24x24_with_dialog_bg_fill((uint32)stream, (uint32)g_dlg_dst,
                                            DLG_W);

    ASSERT_EQ(g_dlg_dst[0], 0x00u);
    ASSERT_EQ(g_dlg_dst[1], 0x42u);
    ASSERT_EQ(g_dlg_dst[2], 0x9Du);
    ASSERT_EQ(g_dlg_dst[3], 0x07u);
    ASSERT_EQ(g_dlg_dst[4], 0xFFu);
    for (i = 5; i < DLG_W * DLG_H; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);
    }
    ASSERT_EQ(g_dlg_dst[DLG_W * DLG_H], DLG_SENT);
}

/* ----------------------------------------------------------------
 * RUN mode (0x00..0x3F): fill n dst bytes from a SINGLE source byte
 * (unchanged from passthrough); only one source byte consumed. The rest
 * of the row is 0x49 fill.
 * ---------------------------------------------------------------- */
static void test_dlg_run_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_RUN(6);
    *p++ = 0xA5u;                  /* written as-is, no transform */
    *p++ = DLG_FILL(18);          /* 6 + 18 == 24 */
    put_dlg_filler_rows_fill(&p);

    dlg_reset_dst();
    fd2_tile_blit_24x24_with_dialog_bg_fill((uint32)stream, (uint32)g_dlg_dst,
                                            DLG_W);

    for (i = 0; i < 6; i++) {
        ASSERT_EQ(g_dlg_dst[i], 0xA5u);
    }
    for (i = 6; i < DLG_W * DLG_H; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);
    }
    ASSERT_EQ(g_dlg_dst[DLG_W * DLG_H], DLG_SENT);
}

/* ----------------------------------------------------------------
 * STRIDE-2 RUN (0x40..0x7F): write n pixels from a single source byte,
 * spaced every other dst byte (dst += 2 per pixel). Same EAX-bug-prone
 * mode as the passthrough sibling: INC EDI; STOSB places pixels at the
 * ODD offsets 1,3,5,7 while the even offsets are SKIPPED (advanced over,
 * NOT filled). The even offsets therefore stay at the sentinel value —
 * confirming the stride-2 advance does not write the gap bytes. After
 * the 8 column-counts a DLG_FILL(16) paints the remaining 16 columns.
 * ---------------------------------------------------------------- */
static void test_dlg_stride2_mode(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_STRIDE(4);         /* 4 pixels -> 8 column-counts */
    *p++ = 0x77u;
    *p++ = DLG_FILL(16);          /* 8 + 16 == 24 */
    put_dlg_filler_rows_fill(&p);

    dlg_reset_dst();
    fd2_tile_blit_24x24_with_dialog_bg_fill((uint32)stream, (uint32)g_dlg_dst,
                                            DLG_W);

    /* even offsets 0,2,4,6 are advanced over without writing -> sentinel;
     * odd offsets 1,3,5,7 carry the source byte. */
    ASSERT_EQ(g_dlg_dst[0], DLG_SENT);
    ASSERT_EQ(g_dlg_dst[1], 0x77u);
    ASSERT_EQ(g_dlg_dst[2], DLG_SENT);
    ASSERT_EQ(g_dlg_dst[3], 0x77u);
    ASSERT_EQ(g_dlg_dst[4], DLG_SENT);
    ASSERT_EQ(g_dlg_dst[5], 0x77u);
    ASSERT_EQ(g_dlg_dst[6], DLG_SENT);
    ASSERT_EQ(g_dlg_dst[7], 0x77u);
    /* cols 8..23 were FILL -> 0x49 */
    for (i = 8; i < DLG_W; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);
    }
    for (i = DLG_W; i < DLG_W * DLG_H; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);
    }
    ASSERT_EQ(g_dlg_dst[DLG_W * DLG_H], DLG_SENT);
}

/* ----------------------------------------------------------------
 * Mixed-mode row: all four modes in a single 24-column row, to confirm
 * the dst cursor stays in sync as modes alternate, and that the trailing
 * FILL actively writes 0x49 over the columns the passthrough sibling
 * would have skipped. Column-count budget:
 *   RUN 3 (3) + STRIDE 2 (2*2=4) + LITERAL 2 (2) + FILL 15 (15) == 24.
 * After RUN 3 dst is at offset 3; STRIDE 2 does INC then store twice, so
 * it writes at offsets 4 and 6, leaving 3 and 5 untouched; LITERAL 2
 * then writes offsets 7 and 8; FILL 15 paints 0x49 over offsets 9..23.
 * The even gaps 3 and 5 inside the stride run stay sentinel.
 * ---------------------------------------------------------------- */
static void test_dlg_mixed_modes_row(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i;

    *p++ = GCMD_RUN(3);    *p++ = 0x11u;           /* cols 0,1,2 = 0x11 */
    *p++ = GCMD_STRIDE(2); *p++ = 0x22u;           /* writes at off 4,6 */
    *p++ = GCMD_LIT(2);    *p++ = 0x33u; *p++ = 0x44u; /* off 7,8 */
    *p++ = DLG_FILL(15);                           /* 3+4+2+15 == 24 */
    put_dlg_filler_rows_fill(&p);

    dlg_reset_dst();
    fd2_tile_blit_24x24_with_dialog_bg_fill((uint32)stream, (uint32)g_dlg_dst,
                                            DLG_W);

    ASSERT_EQ(g_dlg_dst[0], 0x11u);
    ASSERT_EQ(g_dlg_dst[1], 0x11u);
    ASSERT_EQ(g_dlg_dst[2], 0x11u);
    ASSERT_EQ(g_dlg_dst[3], DLG_SENT);   /* stride-2 starts with INC */
    ASSERT_EQ(g_dlg_dst[4], 0x22u);
    ASSERT_EQ(g_dlg_dst[5], DLG_SENT);
    ASSERT_EQ(g_dlg_dst[6], 0x22u);
    ASSERT_EQ(g_dlg_dst[7], 0x33u);
    ASSERT_EQ(g_dlg_dst[8], 0x44u);
    for (i = 9; i < DLG_W; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);   /* trailing FILL = 0x49 */
    }
    for (i = DLG_W; i < DLG_W * DLG_H; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_BG);
    }
    ASSERT_EQ(g_dlg_dst[DLG_W * DLG_H], DLG_SENT);
}

/* ----------------------------------------------------------------
 * Row stride > 24: the decoder must jump dst by (stride - 0x18) at each
 * row boundary, leaving the inter-row gap untouched. Row 0 is a RUN-24
 * (writes the source byte across all 24 cols); rows 1..23 are FILL-24
 * (paint 0x49 across all 24 cols). Confirm row 0 content, that the gap
 * between row 0's columns and the next row start stays sentinel, and
 * that row 1 begins exactly at offset == stride with 0x49.
 * ---------------------------------------------------------------- */
static void test_dlg_row_stride_advance(void)
{
    uint8 stream[64];
    uint8 *p = stream;
    int i, r;

    *p++ = GCMD_RUN(DLG_W);        /* row 0: RUN 24 from src 0x5A */
    *p++ = 0x5Au;
    put_dlg_filler_rows_fill(&p);  /* rows 1..23: FILL 24 each -> 0x49 */

    dlg_reset_dst();
    fd2_tile_blit_24x24_with_dialog_bg_fill((uint32)stream, (uint32)g_dlg_dst,
                                            DLG_STRIDE);

    /* row 0 columns 0..23 carry the run byte */
    for (i = 0; i < DLG_W; i++) {
        ASSERT_EQ(g_dlg_dst[i], 0x5Au);
    }
    /* gap between row 0 columns and the next row start (24..stride-1) */
    for (i = DLG_W; i < (int)DLG_STRIDE; i++) {
        ASSERT_EQ(g_dlg_dst[i], DLG_SENT);
    }
    /* rows 1..23: each starts at r*stride and fills 24 cols with 0x49,
     * with the (stride-24) tail of each row left as sentinel. */
    for (r = 1; r < DLG_H; r++) {
        for (i = 0; i < DLG_W; i++) {
            ASSERT_EQ(g_dlg_dst[r * (int)DLG_STRIDE + i], DLG_BG);
        }
        for (i = DLG_W; i < (int)DLG_STRIDE; i++) {
            ASSERT_EQ(g_dlg_dst[r * (int)DLG_STRIDE + i], DLG_SENT);
        }
    }
}

void run_gfx_blittile2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/blittile (part 3 - dimmed grayscale RLE blit)\n");
    RUN_TEST(test_gray_literal_mode);
    RUN_TEST(test_gray_run_mode);
    RUN_TEST(test_gray_stride2_mode);
    RUN_TEST(test_gray_skip_then_literal);
    RUN_TEST(test_gray_band_mapping);
    RUN_TEST(test_gray_full_24x24_row_wrap);
    RUN_TEST(test_gray_row_stride_advance);

    printf("Suite: gfx/blittile (passthrough RLE blit)\n");
    RUN_TEST(test_pass_literal_mode);
    RUN_TEST(test_pass_run_mode);
    RUN_TEST(test_pass_stride2_mode);
    RUN_TEST(test_pass_skip_then_literal);
    RUN_TEST(test_pass_mixed_modes_row);
    RUN_TEST(test_pass_full_24x24_row_wrap);
    RUN_TEST(test_pass_row_stride_advance);

    printf("Suite: gfx/blittile (dialog-bg fill RLE blit)\n");
    RUN_TEST(test_dlg_fill_mode);
    RUN_TEST(test_dlg_literal_mode);
    RUN_TEST(test_dlg_run_mode);
    RUN_TEST(test_dlg_stride2_mode);
    RUN_TEST(test_dlg_mixed_modes_row);
    RUN_TEST(test_dlg_row_stride_advance);
    printf("\n");
}
