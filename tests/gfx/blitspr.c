/*
 * unit tests for src/gfx/blitspr.c
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/*
 * Per-row strided copy: copy a 3-row x 4-byte block out of a wider source
 * (src stride 8) into a wider destination (dst stride 6). Only the w_bytes
 * window of each row is touched; bytes outside the window must stay intact,
 * and the stride advance must select the right source/destination rows.
 */
static void test_blit_strided_window(void)
{
    static uint8 src[3 * 8];
    static uint8 dst[3 * 6];
    int r, c;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 8; c++) {
            src[r * 8 + c] = (uint8)(0x10 * (r + 1) + c);
        }
    }
    memset(dst, 0xEE, sizeof(dst));

    fd2_blit_rectangle((uint32)dst, 6, (uint32)src, 8, 4, 3);

    /* each dst row got src[row][0..3]; dst[row][4..5] untouched (0xEE) */
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            ASSERT_EQ((long)dst[r * 6 + c], (long)(uint8)(0x10 * (r + 1) + c));
        }
        ASSERT_EQ((long)dst[r * 6 + 4], 0xEE);
        ASSERT_EQ((long)dst[r * 6 + 5], 0xEE);
    }
}

/*
 * Each per-row copy is memmove, not memcpy: with src and dst overlapping
 * within a single row (dst = src + 1, forward overlap), memmove preserves the
 * original source bytes while a naive forward byte copy would smear buf[0]
 * across the whole window. Single row (height 1) isolates the per-row copy.
 */
static void test_blit_overlap_memmove(void)
{
    static uint8 buf[8];
    int i;

    for (i = 0; i < 8; i++) {
        buf[i] = (uint8)(i + 1);   /* 1,2,3,4,5,6,7,8 */
    }

    /* copy buf[0..5] -> buf[1..6], one row of 6 bytes, overlapping forward */
    fd2_blit_rectangle((uint32)(buf + 1), 6, (uint32)buf, 6, 6, 1);

    ASSERT_EQ((long)buf[0], 1);          /* source head untouched */
    /* buf[1..6] == original buf[0..5] == 1,2,3,4,5,6 (memmove semantics) */
    for (i = 1; i <= 6; i++) {
        ASSERT_EQ((long)buf[i], (long)i);
    }
    ASSERT_EQ((long)buf[7], 8);          /* tail untouched */
}

/*
 * Signed height compare (asm: CMP EBX,height / JL): height 0 and any negative
 * height copy zero rows, leaving dst pristine. Guards against an unsigned
 * compare that would treat a negative height as a huge loop count.
 */
static void test_blit_nonpositive_height(void)
{
    static uint8 src[4];
    static uint8 dst[4];

    memset(src, 0x55, sizeof(src));
    memset(dst, 0xCC, sizeof(dst));

    fd2_blit_rectangle((uint32)dst, 4, (uint32)src, 4, 4, 0);
    ASSERT_EQ((long)dst[0], 0xCC);

    fd2_blit_rectangle((uint32)dst, 4, (uint32)src, 4, 4, (uint32)-1);
    ASSERT_EQ((long)dst[0], 0xCC);
    ASSERT_EQ((long)dst[3], 0xCC);
}

/* capture vars from testglob.c stubs of the two callees */
extern uint32 g_blitsetup_dst, g_blitsetup_sprite, g_blitsetup_stride;
extern uint32 g_saveblk_out, g_saveblk_w, g_saveblk_h, g_saveblk_dst,
              g_saveblk_src, g_saveblk_stride;

/*
 * fd2_blit_indexed_sprite_with_alloc wires its 5 cdecl args into:
 *   - sprite_data = sprite_idx * dst_pitch + sheet_base
 *   - malloc(width*height + 8) with width/height read as signed 16-bit
 *     words from sprite_hdr[0] and sprite_hdr[2]
 *   - fd2_save_screen_block_to_buffer(save_buf, width, height, dst,
 *                                     sprite_data, dst_pitch)
 *   - fd2_blit_sprite_with_stride_setup(sprite_data + dst, sprite_hdr,
 *                                       dst_pitch)
 *   - returns the malloc'd save_buf
 * Drive it with distinct values and verify every captured argument plus
 * the returned pointer; the save buffer must be at least width*height+8.
 */
static void test_indexed_sprite_alloc_wiring(void)
{
    static int16 hdr[2];
    uint32 sheet[64];      /* backing for sprite_data address arithmetic */
    void *ret;
    uint32 expect_sprite_data;
    uint32 dst, dst_pitch, sprite_idx;

    hdr[0] = 6;            /* width  */
    hdr[1] = 5;            /* height */

    dst = 0xA0000;
    dst_pitch = 0x140;
    sprite_idx = 3;
    /* sheet_base is just a base value combined arithmetically */
    expect_sprite_data = sprite_idx * dst_pitch + (uint32)sheet;

    ret = fd2_blit_indexed_sprite_with_alloc((uint32)hdr, dst, dst_pitch,
                                             (uint32)sheet, sprite_idx);

    ASSERT_TRUE(ret != NULL);

    /* save-under snapshot call */
    ASSERT_EQ((long)g_saveblk_out, (long)(uint32)ret);
    ASSERT_EQ((long)g_saveblk_w, 6);
    ASSERT_EQ((long)g_saveblk_h, 5);
    ASSERT_EQ((long)g_saveblk_dst, (long)dst);
    ASSERT_EQ((long)g_saveblk_src, (long)expect_sprite_data);
    ASSERT_EQ((long)g_saveblk_stride, (long)dst_pitch);

    /* sprite blit call: dst arg = sprite_data + dst */
    ASSERT_EQ((long)g_blitsetup_dst, (long)(expect_sprite_data + dst));
    ASSERT_EQ((long)g_blitsetup_sprite, (long)(uint32)hdr);
    ASSERT_EQ((long)g_blitsetup_stride, (long)dst_pitch);

    free(ret);
}

/*
 * width/height are MOVSX'd from 16-bit words: a header word of 0xFFFF
 * means -1, so width*height is a signed product. Confirm the signed
 * read by using a header whose raw word would be a huge value if read
 * unsigned but is a small signed product here (-1 * -1 == 1, so the
 * malloc size is 1 + 8 = 9, not a multi-GB request that would fail).
 */
static void test_indexed_sprite_signed_dims(void)
{
    static int16 hdr[2];
    uint32 sheet[8];
    void *ret;

    hdr[0] = -1;          /* 0xFFFF read as signed short */
    hdr[1] = -1;

    ret = fd2_blit_indexed_sprite_with_alloc((uint32)hdr, 0, 0x10,
                                             (uint32)sheet, 0);

    /* (-1)*(-1) + 8 = 9 bytes; alloc must succeed (proves signed read) */
    ASSERT_TRUE(ret != NULL);
    ASSERT_EQ((long)g_saveblk_w, -1);
    ASSERT_EQ((long)g_saveblk_h, -1);

    free(ret);
}

/* capture vars for fd2_blit_sprite_with_decoded_pixels stub (testglob.c) */
extern uint32 g_blitdec_dst, g_blitdec_sprite, g_blitdec_stride;

/*
 * fd2_alloc_and_blit_indexed_sprite_chunk resolves a sprite header through
 * the atlas offset table, computes the paint offset, snapshots the
 * destination block, and paints the sprite. Verify all of:
 *   - sprite_hdr = sheet_base + *(int32*)(sheet_base + 6 + sprite_idx*4)
 *     (the 4-byte-per-entry offset table indirection, picking the right
 *      entry by sprite_idx)
 *   - width/height read as signed 16-bit words from sprite_hdr[0]/[2]
 *   - dst_off = row_idx * surface_pitch + col_offset
 *   - fd2_save_screen_block_to_buffer(save_buf, w, h, dst, dst_off, pitch)
 *   - fd2_blit_sprite_with_decoded_pixels(dst + dst_off, sprite_hdr, pitch)
 *   - malloc'd save_buf (>= w*h+8) returned in EAX
 */
static void test_alloc_blit_chunk_wiring(void)
{
    static uint8 sheet[256];
    uint32 sheet_base;
    uint32 hdr0_off, hdr1_off;
    uint32 dst, surface_pitch, col_offset, row_idx;
    uint32 expect_dst_off, expect_sprite_hdr;
    uint32 ret;

    sheet_base = (uint32)sheet;

    /* two sprite headers placed in the sheet; offset table at +6 holds
       their byte offsets relative to sheet_base (4 bytes per entry). */
    hdr0_off = 0x40;
    hdr1_off = 0x60;
    *(int32 *)(sheet + 6 + 0 * 4) = (int32)hdr0_off;
    *(int32 *)(sheet + 6 + 1 * 4) = (int32)hdr1_off;

    /* header for entry 1: width=6, height=5 */
    *(int16 *)(sheet + hdr1_off + 0) = 6;
    *(int16 *)(sheet + hdr1_off + 2) = 5;

    dst = 0xA0000;
    surface_pitch = 0x140;
    col_offset = 0x78;
    row_idx = 0x54;

    expect_sprite_hdr = sheet_base + hdr1_off;
    expect_dst_off = row_idx * surface_pitch + col_offset;

    ret = fd2_alloc_and_blit_indexed_sprite_chunk(sheet_base, dst,
                                                  surface_pitch, col_offset,
                                                  row_idx, 1);

    ASSERT_TRUE(ret != 0);

    /* save-under snapshot wiring */
    ASSERT_EQ((long)g_saveblk_out, (long)ret);
    ASSERT_EQ((long)g_saveblk_w, 6);
    ASSERT_EQ((long)g_saveblk_h, 5);
    ASSERT_EQ((long)g_saveblk_dst, (long)dst);
    ASSERT_EQ((long)g_saveblk_src, (long)expect_dst_off);
    ASSERT_EQ((long)g_saveblk_stride, (long)surface_pitch);

    /* sprite paint wiring: dst arg = dst + dst_off, sprite = resolved hdr */
    ASSERT_EQ((long)g_blitdec_dst, (long)(dst + expect_dst_off));
    ASSERT_EQ((long)g_blitdec_sprite, (long)expect_sprite_hdr);
    ASSERT_EQ((long)g_blitdec_stride, (long)surface_pitch);

    free((void *)ret);
}

/*
 * width/height are MOVSX'd from 16-bit words: a header of (-1,-1) makes
 * width*height = 1, so malloc(1+8)=9 succeeds (an unsigned read would
 * request ~4GB and fail). Confirms the signed product used for the
 * scratch buffer size.
 */
static void test_alloc_blit_chunk_signed_dims(void)
{
    static uint8 sheet[128];
    uint32 sheet_base;
    uint32 ret;

    sheet_base = (uint32)sheet;
    *(int32 *)(sheet + 6 + 0 * 4) = 0x30;
    *(int16 *)(sheet + 0x30 + 0) = -1;
    *(int16 *)(sheet + 0x30 + 2) = -1;

    ret = fd2_alloc_and_blit_indexed_sprite_chunk(sheet_base, 0, 0x10, 0, 0, 0);

    ASSERT_TRUE(ret != 0);
    ASSERT_EQ((long)g_saveblk_w, -1);
    ASSERT_EQ((long)g_saveblk_h, -1);

    free((void *)ret);
}

/* capture vars for fd2_blit_sprite_raw_with_header stub (testglob.c) */
extern uint32 g_blitraw_dst, g_blitraw_sprite, g_blitraw_stride;

/*
 * fd2_blit_sheet_sprite_at_offset resolves a sprite through the atlas
 * offset table and forwards a raw blit. Verify:
 *   - sprite_addr = sheet + *(int32*)(sheet + 6 + sprite_idx*4)
 *     (4-byte-per-entry table; picking the right entry by sprite_idx)
 *   - fd2_blit_sprite_raw_with_header(dst, sprite_addr, dst_pitch)
 *     forwards dst and dst_pitch unchanged and the resolved sprite addr.
 * Drive with a multi-entry table and assert the captured args.
 */
static void test_sheet_sprite_offset_wiring(void)
{
    static uint8 sheet[256];
    uint32 sheet_base;
    uint32 hdr0_off, hdr1_off, hdr2_off;
    uint32 dst, dst_pitch;
    uint32 expect_sprite_addr;

    sheet_base = (uint32)sheet;

    /* three offset-table entries (4 bytes each) starting at sheet+6 */
    hdr0_off = 0x40;
    hdr1_off = 0x60;
    hdr2_off = 0x80;
    *(int32 *)(sheet + 6 + 0 * 4) = (int32)hdr0_off;
    *(int32 *)(sheet + 6 + 1 * 4) = (int32)hdr1_off;
    *(int32 *)(sheet + 6 + 2 * 4) = (int32)hdr2_off;

    dst = 0xA0000;
    dst_pitch = 0x140;

    /* pick entry 2: sprite_addr must resolve to sheet_base + hdr2_off */
    expect_sprite_addr = sheet_base + hdr2_off;

    fd2_blit_sheet_sprite_at_offset(dst, dst_pitch, sheet_base, 2);

    ASSERT_EQ((long)g_blitraw_dst, (long)dst);
    ASSERT_EQ((long)g_blitraw_sprite, (long)expect_sprite_addr);
    ASSERT_EQ((long)g_blitraw_stride, (long)dst_pitch);

    /* pick entry 0: distinct entry selects a distinct sprite_addr */
    fd2_blit_sheet_sprite_at_offset(dst, dst_pitch, sheet_base, 0);
    ASSERT_EQ((long)g_blitraw_sprite, (long)(sheet_base + hdr0_off));
}

/*
 * The offset-table entry is read as a signed 32-bit value and added to
 * sheet: a negative table entry yields sprite_addr below sheet_base.
 * Confirms the signed *(int32*) read (not unsigned).
 */
static void test_sheet_sprite_negative_offset(void)
{
    static uint8 sheet[256];
    uint32 sheet_base;

    sheet_base = (uint32)sheet;
    *(int32 *)(sheet + 6 + 0 * 4) = (int32)-0x10;

    fd2_blit_sheet_sprite_at_offset(0xB0000, 0x100, sheet_base, 0);

    ASSERT_EQ((long)g_blitraw_sprite, (long)(sheet_base - 0x10));
    ASSERT_EQ((long)g_blitraw_dst, (long)0xB0000);
    ASSERT_EQ((long)g_blitraw_stride, (long)0x100);
}

/* capture vars for fd2_rle_blit_sprite stub (testglob.c) */
extern uint32 g_rle_blit_last_sprite, g_rle_blit_last_buf, g_rle_blit_last_palette;
extern int32 g_rle_blit_last_x, g_rle_blit_last_y, g_rle_blit_last_stride;

/*
 * fd2_blit_indexed_sprite_at_xy resolves a sub-sprite through the atlas
 * offset table (same sheet+6+idx*4 table as fd2_blit_sheet_sprite_at_offset)
 * and forwards an RLE blit at (0, 0) with transparent passthrough. Verify:
 *   - sprite_addr = sheet + *(int32*)(sheet + 6 + sprite_idx*4)
 *   - fd2_rle_blit_sprite(sprite_addr, 0, 0, dst, dst_pitch, 0xFFFFFFFF):
 *     rle_stream=sprite_addr, dst_x=dst_y=0, dst_buf=dst,
 *     stride=dst_pitch, palette_op=-1 (transparent passthrough).
 */
static void test_indexed_xy_offset_and_arg_routing(void)
{
    static uint8 sheet[256];
    uint32 sheet_base;
    uint32 dst, dst_pitch;

    sheet_base = (uint32)sheet;

    /* three offset-table entries (4 bytes each) starting at sheet+6 */
    *(int32 *)(sheet + 6 + 0 * 4) = (int32)0x40;
    *(int32 *)(sheet + 6 + 1 * 4) = (int32)0x60;
    *(int32 *)(sheet + 6 + 2 * 4) = (int32)0x80;

    dst = 0xA0000;
    dst_pitch = 0x140;

    /* pick entry 2: sprite_addr must resolve to sheet_base + 0x80 */
    fd2_blit_indexed_sprite_at_xy(dst, dst_pitch, sheet_base, 2);
    ASSERT_EQ((long)g_rle_blit_last_sprite, (long)(sheet_base + 0x80));
    ASSERT_EQ((long)g_rle_blit_last_x, (long)0);
    ASSERT_EQ((long)g_rle_blit_last_y, (long)0);
    ASSERT_EQ((long)g_rle_blit_last_buf, (long)dst);
    ASSERT_EQ((long)g_rle_blit_last_stride, (long)dst_pitch);
    ASSERT_EQ((long)g_rle_blit_last_palette, (long)0xFFFFFFFF);

    /* pick entry 0: distinct entry selects a distinct sprite_addr */
    fd2_blit_indexed_sprite_at_xy(dst, dst_pitch, sheet_base, 0);
    ASSERT_EQ((long)g_rle_blit_last_sprite, (long)(sheet_base + 0x40));
}

/*
 * The offset-table entry is read as a signed 32-bit value and added to
 * sheet: a negative table entry yields sprite_addr below sheet_base.
 * Confirms the signed *(int32*) read (not unsigned).
 */
static void test_indexed_xy_negative_offset(void)
{
    static uint8 sheet[256];
    uint32 sheet_base;

    sheet_base = (uint32)sheet;
    *(int32 *)(sheet + 6 + 0 * 4) = (int32)-0x10;

    fd2_blit_indexed_sprite_at_xy(0xB0000, 0x100, sheet_base, 0);

    ASSERT_EQ((long)g_rle_blit_last_sprite, (long)(sheet_base - 0x10));
    ASSERT_EQ((long)g_rle_blit_last_buf, (long)0xB0000);
    ASSERT_EQ((long)g_rle_blit_last_stride, (long)0x100);
    ASSERT_EQ((long)g_rle_blit_last_palette, (long)0xFFFFFFFF);
}

/*
 * fd2_fill_screen_rect_with_byte paints its (size-1)x(size-1) marker square
 * with memset DIRECTLY into the hard-coded mode13h VGA framebuffer at linear
 * 0xA0000 (row_ptr = 0xA0000 + y*320 + x; (size-1) rows x (size-1) bytes,
 * stride 320; signed loop bound so size<=1 paints nothing).
 *
 * Behavioral read-back verification is DEFERRED to Phase 9 integration: the
 * destination is the literal 0xA0000 hard-coded in the binary (not a redirect-
 * able parameter), so the fill cannot be aimed at observable RAM without
 * altering the emitted code. Under the text-mode test harness, 0xA0000 maps to
 * inactive VGA planar hardware (the active text buffer is 0xB8000), so reads
 * back 0xFF regardless of what was written and cannot witness the fill — this
 * is the pure display side-effect category the test policy defers to Phase 9.
 * Equivalence rests on the three-source match: the arithmetic and the size-1
 * geometry are identical in form to the read-back-verified fd2_blit_rectangle
 * family above (only the count is memset width vs memmove width).
 */

/*
 * fd2_blit_money_digit_sprite resolves a 9-row x 6-byte digit sprite out of
 * the chapter-intro sprite atlas and copies it row-by-row into dst at a given
 * row stride. The source address is:
 *   src = atlas + *(int32*)(atlas + 0xE) + 4 + sprite_idx * 6
 * The atlas base is the global data_fd2_ui_menu_screen_sprite_atlas_buf_ptr,
 * which here points at an in-memory buffer (no file I/O: the function only
 * dereferences the pointer value).
 *
 * This test verifies the full pipeline: the +0xE offset-table indirection,
 * the +4 section-header skip, the sprite_idx*6 source selection, all nine
 * 6-byte rows copied, the source row stride of 6, and the destination row
 * stride applied between rows (with bytes outside the 6-wide window left
 * intact so an over-copy would be caught).
 */
static void test_money_digit_full_blit(void)
{
    static uint8 atlas[512];
    static uint8 dst[9 * 10];
    uint32 saved_atlas;
    int32 section_off;
    uint32 src_base;
    int row, col;
    uint32 sprite_idx;
    uint32 dst_stride;

    /* offset-table entry at atlas+0xE points to the digit-sprite section */
    section_off = 0x80;
    *(int32 *)(atlas + 0xE) = section_off;

    /* sprite rows begin at section + 4 (header skip) + sprite_idx*6 */
    sprite_idx = 3;
    src_base = (uint32)atlas + (uint32)section_off + 4 + sprite_idx * 6;

    /* lay 9 rows of 6 distinct bytes at the resolved source */
    for (row = 0; row < 9; row++) {
        for (col = 0; col < 6; col++) {
            *(uint8 *)(src_base + row * 6 + col) =
                (uint8)(0x10 * (row + 1) + col);
        }
    }

    dst_stride = 10;                 /* wider than the 6-byte copy window */
    memset(dst, 0xEE, sizeof(dst));

    saved_atlas = data_fd2_ui_menu_screen_sprite_atlas_buf_ptr;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)atlas;

    fd2_blit_money_digit_sprite((uint32)dst, dst_stride, sprite_idx);

    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = saved_atlas;

    /* each of the 9 dst rows got its 6 source bytes; dst[row][6..9] untouched */
    for (row = 0; row < 9; row++) {
        for (col = 0; col < 6; col++) {
            ASSERT_EQ((long)dst[row * dst_stride + col],
                      (long)(uint8)(0x10 * (row + 1) + col));
        }
        ASSERT_EQ((long)dst[row * dst_stride + 6], 0xEE);
        ASSERT_EQ((long)dst[row * dst_stride + 9], 0xEE);
    }
}

/*
 * The section offset at atlas+0xE is read as a signed 32-bit value: a
 * negative entry resolves the digit-sprite section BELOW the atlas base.
 * Confirms the *(int32*) signed read (an unsigned read would land ~4GB away
 * and fault). The atlas pointer is aimed mid-buffer so a negative offset
 * still lands in valid memory.
 */
static void test_money_digit_signed_section_offset(void)
{
    static uint8 backing[512];
    static uint8 dst[9 * 6];
    uint32 atlas_base;
    uint32 saved_atlas;
    int32 section_off;
    uint32 src_base;
    int row, col;

    /* place the atlas pointer mid-buffer so atlas + (negative) stays in range */
    atlas_base = (uint32)backing + 0x100;
    section_off = -0x40;                          /* negative -> below base */
    *(int32 *)(atlas_base + 0xE) = section_off;

    /* sprite_idx 0: src = atlas + section_off + 4 */
    src_base = atlas_base + (uint32)section_off + 4;
    for (row = 0; row < 9; row++) {
        for (col = 0; col < 6; col++) {
            *(uint8 *)(src_base + row * 6 + col) = (uint8)(0xA0 + row * 6 + col);
        }
    }

    memset(dst, 0x00, sizeof(dst));

    saved_atlas = data_fd2_ui_menu_screen_sprite_atlas_buf_ptr;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = atlas_base;

    fd2_blit_money_digit_sprite((uint32)dst, 6, 0);

    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = saved_atlas;

    for (row = 0; row < 9; row++) {
        for (col = 0; col < 6; col++) {
            ASSERT_EQ((long)dst[row * 6 + col], (long)(uint8)(0xA0 + row * 6 + col));
        }
    }
}

/*
 * sprite_idx selects the source by sprite_idx*6: two adjacent indices pick
 * source windows exactly 6 bytes apart. Drive the same atlas with sprite_idx
 * 0 and sprite_idx 1 and confirm the second blit reads the bytes one 6-byte
 * sprite-row further along than the first.
 */
static void test_money_digit_sprite_idx_stride(void)
{
    static uint8 atlas[512];
    /* the blit always writes 9 rows; size each dst to hold the full 9x6 run */
    static uint8 dst0[9 * 6];
    static uint8 dst1[9 * 6];
    uint32 saved_atlas;
    uint32 section_base;
    int i;

    *(int32 *)(atlas + 0xE) = 0x20;
    /* section payload begins at atlas + 0x20 + 4; fill a ramp so each
       6-byte window is distinguishable. sprite_idx 1 reads up through
       section[6 + 8*6 .. +5] = section[54..59], so the ramp must span >= 60. */
    section_base = (uint32)atlas + 0x20 + 4;
    for (i = 0; i < 128; i++) {
        *(uint8 *)(section_base + i) = (uint8)(0x01 + i);
    }

    saved_atlas = data_fd2_ui_menu_screen_sprite_atlas_buf_ptr;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)atlas;

    /* stride 6 packs the 9 rows contiguously; only row 0 is asserted below */
    fd2_blit_money_digit_sprite((uint32)dst0, 6, 0);
    fd2_blit_money_digit_sprite((uint32)dst1, 6, 1);

    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = saved_atlas;

    /* sprite_idx 0 row 0 = section[0..5]; sprite_idx 1 row 0 = section[6..11] */
    for (i = 0; i < 6; i++) {
        ASSERT_EQ((long)dst0[i], (long)(uint8)(0x01 + i));
        ASSERT_EQ((long)dst1[i], (long)(uint8)(0x01 + 6 + i));
    }
}

void run_gfx_blitspr_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: gfx/blitspr\n");
    RUN_TEST(test_blit_strided_window);
    RUN_TEST(test_blit_overlap_memmove);
    RUN_TEST(test_blit_nonpositive_height);
    RUN_TEST(test_indexed_sprite_alloc_wiring);
    RUN_TEST(test_indexed_sprite_signed_dims);
    RUN_TEST(test_alloc_blit_chunk_wiring);
    RUN_TEST(test_alloc_blit_chunk_signed_dims);
    RUN_TEST(test_sheet_sprite_offset_wiring);
    RUN_TEST(test_sheet_sprite_negative_offset);
    RUN_TEST(test_indexed_xy_offset_and_arg_routing);
    RUN_TEST(test_indexed_xy_negative_offset);
    RUN_TEST(test_money_digit_full_blit);
    RUN_TEST(test_money_digit_signed_section_offset);
    RUN_TEST(test_money_digit_sprite_idx_stride);
    printf("\n");
}
