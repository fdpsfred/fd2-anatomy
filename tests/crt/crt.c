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

/* ================================================================
 * crt_equivalent_lx_header_reader_36344 @ 0x36344
 *
 * Builds a minimal in-memory LX image and drives the header reader.
 * mode_byte bit0=1 makes every chunk read a memcpy from the supplied
 * base, so the whole function runs in memory with no file I/O. The
 * file-backed variant writes the same image to a SCRATCH temp file
 * (a generated file, NOT a staged game resource) and drives the real
 * open/lseek/read path with mode_byte bit0=0.
 *
 * Image layout (offsets into the buffer):
 *   +0x3C : uint32 e_lfanew  -> we place the LX header at LXHDR.
 *   LXHDR : "LX" magic (+0x00), then a 0xAC-byte header.
 *   LXHDR +0x40 : uint32 object_table_offset (relative to e_lfanew).
 *   LXHDR +0x44 : uint32 number_of_objects.
 *   obj recs    : 0x18 bytes each at (e_lfanew + object_table_offset);
 *                 +0x00 holds the object virtual size.
 * Expected return = number_of_objects*15 + Sum(virtual_size).
 * ================================================================ */

#define CRT_LX_BUF_SIZE   512
#define CRT_LX_HDR_OFF    0x80    /* where the LX header sits */
#define CRT_LX_OBJTBL_OFF 0xC0    /* object table, relative to e_lfanew */
#define CRT_LX_SCRATCH    "CRTLXHDR.TMP"

/* poke a little-endian uint32 into buf at byte offset off */
static void crt_put32(uint8 *buf, int off, uint32 v)
{
    buf[off + 0] = (uint8)(v & 0xFF);
    buf[off + 1] = (uint8)((v >> 8) & 0xFF);
    buf[off + 2] = (uint8)((v >> 16) & 0xFF);
    buf[off + 3] = (uint8)((v >> 24) & 0xFF);
}

/* Build an LX image into buf with n_objs object records whose virtual
 * sizes come from vsizes[]. Returns nothing; buf must be zeroed first. */
static void crt_build_lx_image(uint8 *buf, int n_objs, const uint32 *vsizes)
{
    int rec_base;
    int i;

    /* e_lfanew at MZ+0x3C points at the LX header */
    crt_put32(buf, 0x3C, CRT_LX_HDR_OFF);

    /* LX magic "LX" at the header start */
    buf[CRT_LX_HDR_OFF + 0] = 'L';
    buf[CRT_LX_HDR_OFF + 1] = 'X';

    /* object_table_offset (relative to e_lfanew) and number_of_objects */
    crt_put32(buf, CRT_LX_HDR_OFF + 0x40, CRT_LX_OBJTBL_OFF);
    crt_put32(buf, CRT_LX_HDR_OFF + 0x44, (uint32)n_objs);

    /* object records: 0x18 bytes each; virtual size at +0x00 */
    rec_base = CRT_LX_HDR_OFF + CRT_LX_OBJTBL_OFF;
    for (i = 0; i < n_objs; i++) {
        crt_put32(buf, rec_base + i * 0x18 + 0x00, vsizes[i]);
    }
}

/* in-memory (mode bit0=1) happy path: single object */
static void test_lx_header_inmem_single_object(void)
{
    static uint8 buf[CRT_LX_BUF_SIZE];
    static const uint32 vsizes[1] = { 0x1000 };
    int r;
    int i;

    for (i = 0; i < CRT_LX_BUF_SIZE; i++) {
        buf[i] = 0;
    }
    crt_build_lx_image(buf, 1, vsizes);

    /* mode bit0=1 -> base address path; arg0 == in-memory base */
    r = crt_equivalent_lx_header_reader_36344((char *)buf, 1);

    /* 1*15 + 0x1000 */
    ASSERT_EQ(r, 1 * 15 + 0x1000);
}

/* in-memory happy path: several objects -> verifies the object loop
 * accumulates every virtual size and chains the read offset by 0x18. */
static void test_lx_header_inmem_multi_object(void)
{
    static uint8 buf[CRT_LX_BUF_SIZE];
    static const uint32 vsizes[3] = { 0x100, 0x2000, 0x30 };
    int r;
    int i;

    for (i = 0; i < CRT_LX_BUF_SIZE; i++) {
        buf[i] = 0;
    }
    crt_build_lx_image(buf, 3, vsizes);

    r = crt_equivalent_lx_header_reader_36344((char *)buf, 1);

    /* 3*15 + (0x100 + 0x2000 + 0x30) */
    ASSERT_EQ(r, 3 * 15 + (0x100 + 0x2000 + 0x30));
}

/* in-memory: zero objects still validates magic and returns 0*15+0 == 0. */
static void test_lx_header_inmem_zero_objects(void)
{
    static uint8 buf[CRT_LX_BUF_SIZE];
    int r;
    int i;

    for (i = 0; i < CRT_LX_BUF_SIZE; i++) {
        buf[i] = 0;
    }
    crt_build_lx_image(buf, 0, (const uint32 *)0);

    r = crt_equivalent_lx_header_reader_36344((char *)buf, 1);

    ASSERT_EQ(r, 0);
}

/* in-memory: bad LX magic -> strcmp mismatch -> return 0 (no aggregate). */
static void test_lx_header_inmem_bad_magic(void)
{
    static uint8 buf[CRT_LX_BUF_SIZE];
    static const uint32 vsizes[1] = { 0x9999 };
    int r;
    int i;

    for (i = 0; i < CRT_LX_BUF_SIZE; i++) {
        buf[i] = 0;
    }
    crt_build_lx_image(buf, 1, vsizes);
    /* corrupt the magic: "MZ" instead of "LX" */
    buf[CRT_LX_HDR_OFF + 0] = 'M';
    buf[CRT_LX_HDR_OFF + 1] = 'Z';

    r = crt_equivalent_lx_header_reader_36344((char *)buf, 1);

    ASSERT_EQ(r, 0);
}

/* file-backed (mode bit0=0): write the same image to a scratch file and
 * drive the real open/lseek/read path. The scratch file is generated
 * here (not a game resource). */
static void test_lx_header_file_multi_object(void)
{
    static uint8 buf[CRT_LX_BUF_SIZE];
    static const uint32 vsizes[2] = { 0x800, 0x40 };
    int fd;
    int r;
    int i;

    for (i = 0; i < CRT_LX_BUF_SIZE; i++) {
        buf[i] = 0;
    }
    crt_build_lx_image(buf, 2, vsizes);

    fd = open(CRT_LX_SCRATCH, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
              S_IREAD | S_IWRITE);
    ASSERT_TRUE(fd >= 0);
    write(fd, buf, CRT_LX_BUF_SIZE);
    close(fd);

    /* mode bit0=0 -> open(path,...) + lseek/read drive the reads */
    r = crt_equivalent_lx_header_reader_36344(CRT_LX_SCRATCH, 0);
    remove(CRT_LX_SCRATCH);

    /* 2*15 + (0x800 + 0x40) */
    ASSERT_EQ(r, 2 * 15 + (0x800 + 0x40));
}

/* file-backed: open() failure on a missing path -> return 0, no crash. */
static void test_lx_header_file_open_fail(void)
{
    int r;

    remove(CRT_LX_SCRATCH);   /* ensure it does not exist */
    r = crt_equivalent_lx_header_reader_36344(CRT_LX_SCRATCH, 0);
    ASSERT_EQ(r, 0);
}

/* ================================================================
 * crt_equivalent_lx_module_loader_3647b @ 0x3647b
 *
 * Drives the full loader entirely in memory (mode/flags bit0=1 makes
 * every chunk read a memcpy from the supplied base, so no file I/O is
 * needed and the loader's path argument doubles as the source-image
 * base address). A complete LX image is assembled in a SOURCE buffer;
 * the loader writes the loaded pages and applies relocations into a
 * separate DESTINATION buffer (the caller_buf), which is then checked.
 *
 * Image layout (offsets into the source buffer):
 *   +0x3C            : uint32 e_lfanew -> LX header at CRT_LXL_HDR_OFF.
 *   HDR+0x00         : "LX" magic.
 *   HDR+0x14         : uint32 number_of_fixup_pages (outer fixup loop).
 *   HDR+0x28         : uint32 page_size_max (fixup src_offset bound).
 *   HDR+0x2C         : uint8  page_offset_shift (we use 0).
 *   HDR+0x40         : uint32 object_table_offset       (rel to e_lfanew).
 *   HDR+0x44         : uint32 number_of_objects.
 *   HDR+0x48         : uint32 object_page_table_offset   (rel to e_lfanew).
 *   HDR+0x68         : uint32 fixup_page_table_offset    (rel to e_lfanew).
 *   HDR+0x6C         : uint32 fixup_record_table_offset  (rel to e_lfanew).
 *   HDR+0x80         : uint32 page_data_base (absolute file offset base).
 *   object record    : 0x18 bytes; +0x00 vsize, +0x08 flags, +0x10 #pages.
 *   page entry       : 0x08 bytes; +0x00 page index, +0x04 byte count.
 *   fixup page table : uint32 cumulative offsets into the fixup records.
 *   fixup record     : src_type(1) target_type(1) src_off(2)
 *                      target_obj(1) target_disp(2).
 *
 * page file offset = (page_index << shift) + page_data_base, so with
 * shift 0 the page data sits at page_data_base + page_index.
 * ================================================================ */

#define CRT_LXL_SRC_SIZE   1024
#define CRT_LXL_DST_SIZE   512
#define CRT_LXL_HDR_OFF    0x80
#define CRT_LXL_OBJTBL     0xC0   /* object table,  rel to e_lfanew */
#define CRT_LXL_PAGETBL    0x100  /* page table,    rel to e_lfanew */
#define CRT_LXL_FIXPAGE    0x140  /* fixup page tbl, rel to e_lfanew */
#define CRT_LXL_FIXREC     0x160  /* fixup records, rel to e_lfanew */
#define CRT_LXL_PAGEDATA   0x200  /* page data, absolute file offset */

/* allocator stub for the flags&4 path: hands back a fixed static buffer
 * and records the size it was asked for so the test can assert the
 * loader passed the header reader's total image size through. */
static uint8  crt_lxl_alloc_pool[CRT_LXL_DST_SIZE];
static uint32 crt_lxl_alloc_last_size;
static int    crt_lxl_alloc_return_null;

static void *crt_lxl_test_alloc(uint32 size)
{
    crt_lxl_alloc_last_size = size;
    if (crt_lxl_alloc_return_null) {
        return (void *)0;
    }
    return crt_lxl_alloc_pool;
}

/* Build a single-object, single-page LX image with one fixup record.
 * page_bytes bytes of payload (0x80,0x81,...) are placed at the page
 * data location; the fixup writes obj_base+target_disp at src_off. */
static void crt_lxl_build_image(uint8 *buf, uint32 vsize, uint32 page_bytes,
                                uint16 src_off, uint8 target_obj,
                                uint16 target_disp, uint8 obj_flags)
{
    int hdr = CRT_LXL_HDR_OFF;
    int rec;
    int i;

    crt_put32(buf, 0x3C, hdr);                         /* e_lfanew      */

    buf[hdr + 0] = 'L';
    buf[hdr + 1] = 'X';
    crt_put32(buf, hdr + 0x14, 1);                     /* 1 fixup page  */
    crt_put32(buf, hdr + 0x28, 0x1000);                /* page_size_max */
    buf[hdr + 0x2C] = 0;                               /* shift = 0     */
    crt_put32(buf, hdr + 0x40, CRT_LXL_OBJTBL);        /* obj table off */
    crt_put32(buf, hdr + 0x44, 1);                     /* 1 object      */
    crt_put32(buf, hdr + 0x48, CRT_LXL_PAGETBL);       /* page table off*/
    crt_put32(buf, hdr + 0x68, CRT_LXL_FIXPAGE);       /* fixup page off*/
    crt_put32(buf, hdr + 0x6C, CRT_LXL_FIXREC);        /* fixup rec off */
    crt_put32(buf, hdr + 0x80, CRT_LXL_PAGEDATA);      /* page data base*/

    /* object record at e_lfanew + object_table_offset */
    rec = hdr + CRT_LXL_OBJTBL;
    crt_put32(buf, rec + 0x00, vsize);                 /* virtual size  */
    buf[rec + 0x08] = obj_flags;                       /* flags         */
    crt_put32(buf, rec + 0x10, 1);                     /* 1 page        */

    /* page table entry at e_lfanew + object_page_table_offset */
    rec = hdr + CRT_LXL_PAGETBL;
    crt_put32(buf, rec + 0x00, 0);                     /* page index 0  */
    buf[rec + 0x04] = (uint8)(page_bytes & 0xFF);      /* byte count lo */
    buf[rec + 0x05] = (uint8)((page_bytes >> 8) & 0xFF);

    /* fixup page table: entry[0]=0, entry[1]=7 (one 7-byte record).
     * A record is src_type(1)+target_type(1)+src_off(2)+target_obj(1)
     * +target_disp(2) = 7 bytes; the cumulative offsets bound the
     * per-page record range [entry[i], entry[i+1]). */
    rec = hdr + CRT_LXL_FIXPAGE;
    crt_put32(buf, rec + 0x00, 0);
    crt_put32(buf, rec + 0x04, 7);

    /* one fixup record at e_lfanew + fixup_record_table_offset */
    rec = hdr + CRT_LXL_FIXREC;
    buf[rec + 0] = 0x07;                               /* src_type      */
    buf[rec + 1] = 0x00;                               /* target_type   */
    buf[rec + 2] = (uint8)(src_off & 0xFF);            /* src_off lo    */
    buf[rec + 3] = (uint8)((src_off >> 8) & 0xFF);     /* src_off hi    */
    buf[rec + 4] = target_obj;                         /* target object */
    buf[rec + 5] = (uint8)(target_disp & 0xFF);        /* disp lo       */
    buf[rec + 6] = (uint8)((target_disp >> 8) & 0xFF); /* disp hi       */

    /* page payload at page_data_base + page_index(0) */
    for (i = 0; i < (int)page_bytes; i++) {
        buf[CRT_LXL_PAGEDATA + i] = (uint8)(0x80 + i);
    }
}

/* full in-memory load + relocation: object base is the destination
 * buffer; one page of payload is copied, then one fixup patches a
 * 32-bit word = obj_base + target_disp at src_off. */
static void test_lxl_inmem_load_and_fixup(void)
{
    static uint8 src[CRT_LXL_SRC_SIZE];
    static uint8 dst[CRT_LXL_DST_SIZE];
    void  *r;
    uint32 expect;
    int    i;

    for (i = 0; i < CRT_LXL_SRC_SIZE; i++) src[i] = 0;
    for (i = 0; i < CRT_LXL_DST_SIZE; i++) dst[i] = 0xCC;

    /* vsize 0x40, 0x20 page bytes, fixup at src_off 0x10,
     * target object 1, disp 0x4. obj_flags 0 (no 16-byte skip). */
    crt_lxl_build_image(src, 0x40, 0x20, 0x10, 1, 0x4, 0x00);

    /* flags bit0=1 -> src doubles as in-memory base; bit2=0 -> use dst. */
    r = crt_equivalent_lx_module_loader_3647b((char *)src, 1, dst);

    /* returns the caller buffer */
    ASSERT_TRUE(r == (void *)dst);

    /* page payload copied to the object base (== dst start) */
    ASSERT_EQ(dst[0x00], 0x80);
    ASSERT_EQ(dst[0x1F], (uint8)(0x80 + 0x1F));

    /* relocation: *(uint32*)(page_base + src_off) = obj_base + disp.
     * obj_base == (uint32)dst, page_base == (uint32)dst (page 0). */
    expect = (uint32)dst + 0x4;
    ASSERT_EQ(*(uint32 *)(dst + 0x10), expect);
}

/* min(remaining_obj_size, page_byte_count): vsize smaller than the page
 * byte count clamps the copy to the remaining object size. */
static void test_lxl_inmem_clamp_to_vsize(void)
{
    static uint8 src[CRT_LXL_SRC_SIZE];
    static uint8 dst[CRT_LXL_DST_SIZE];
    void *r;
    int   i;

    for (i = 0; i < CRT_LXL_SRC_SIZE; i++) src[i] = 0;
    for (i = 0; i < CRT_LXL_DST_SIZE; i++) dst[i] = 0xCC;

    /* vsize 0x0C but page byte count 0x20 -> copy clamps to 0x0C.
     * Keep the fixup at src_off 4 so it patches dst[4..7] (clear of the
     * 0x0B payload byte we assert below). The loader first
     * memset(dst,0,total_size) where total_size = 1*15 + 0x0C = 0x1B,
     * so untouched-but-zeroed bytes read back 0, while bytes past
     * total_size keep the 0xCC fill. */
    crt_lxl_build_image(src, 0x0C, 0x20, 0x04, 1, 0x0, 0x00);

    r = crt_equivalent_lx_module_loader_3647b((char *)src, 1, dst);
    ASSERT_TRUE(r == (void *)dst);

    /* last clamped payload byte present (0x0B); had the copy used the
     * page byte count 0x20 instead, dst[0x0C] would be 0x8C, but the
     * clamp means it is only the memset 0. */
    ASSERT_EQ(dst[0x0B], (uint8)(0x80 + 0x0B));
    ASSERT_EQ(dst[0x0C], 0x00);
    /* memset spanned exactly total_size (0x1B); past that the fill survives */
    ASSERT_EQ(dst[0x1B], 0xCC);
}

/* bad LX magic -> loader returns 0 without touching the destination. */
static void test_lxl_inmem_bad_magic(void)
{
    static uint8 src[CRT_LXL_SRC_SIZE];
    static uint8 dst[CRT_LXL_DST_SIZE];
    void *r;
    int   i;

    for (i = 0; i < CRT_LXL_SRC_SIZE; i++) src[i] = 0;
    for (i = 0; i < CRT_LXL_DST_SIZE; i++) dst[i] = 0xCC;
    crt_lxl_build_image(src, 0x40, 0x20, 0x10, 1, 0x4, 0x00);
    src[CRT_LXL_HDR_OFF + 0] = 'M';   /* corrupt magic */
    src[CRT_LXL_HDR_OFF + 1] = 'Z';

    r = crt_equivalent_lx_module_loader_3647b((char *)src, 1, dst);
    ASSERT_TRUE(r == (void *)0);
}

/* invalid fixup record (src_type low 3 bits zero) -> abort, return 0. */
static void test_lxl_inmem_bad_fixup(void)
{
    static uint8 src[CRT_LXL_SRC_SIZE];
    static uint8 dst[CRT_LXL_DST_SIZE];
    void *r;
    int   i;

    for (i = 0; i < CRT_LXL_SRC_SIZE; i++) src[i] = 0;
    for (i = 0; i < CRT_LXL_DST_SIZE; i++) dst[i] = 0xCC;
    crt_lxl_build_image(src, 0x40, 0x20, 0x10, 1, 0x4, 0x00);
    /* src_type = 0 -> (src_type & 7)==0 -> invalid */
    src[CRT_LXL_HDR_OFF + CRT_LXL_FIXREC + 0] = 0x00;

    r = crt_equivalent_lx_module_loader_3647b((char *)src, 1, dst);
    ASSERT_TRUE(r == (void *)0);
}

/* flags&4 alloc path: the loader allocates via data_ail_alloc_fnptr,
 * which must be called with the header reader's total image size, and
 * the returned pool becomes both the object base and the return value. */
static void test_lxl_alloc_path(void)
{
    static uint8 src[CRT_LXL_SRC_SIZE];
    uint32 saved_fnptr;
    void  *r;
    uint32 expect_size;
    int    i;

    for (i = 0; i < CRT_LXL_SRC_SIZE; i++) src[i] = 0;
    for (i = 0; i < CRT_LXL_DST_SIZE; i++) crt_lxl_alloc_pool[i] = 0xCC;
    crt_lxl_build_image(src, 0x40, 0x20, 0x10, 1, 0x4, 0x00);

    saved_fnptr = data_ail_alloc_fnptr;
    data_ail_alloc_fnptr = (uint32)crt_lxl_test_alloc;
    crt_lxl_alloc_last_size = 0;
    crt_lxl_alloc_return_null = 0;

    /* flags bit0=1 (in-memory base) | bit2=1 (alloc). caller_buf ignored. */
    r = crt_equivalent_lx_module_loader_3647b((char *)src, 1 | 4, (void *)0);
    data_ail_alloc_fnptr = saved_fnptr;

    /* allocator returned the pool, which is the loader's return value */
    ASSERT_TRUE(r == (void *)crt_lxl_alloc_pool);

    /* header reader formula: num_obj*15 + Sum(vsize) = 1*15 + 0x40 */
    expect_size = 1 * 15 + 0x40;
    ASSERT_EQ(crt_lxl_alloc_last_size, expect_size);

    /* page payload landed in the allocated pool */
    ASSERT_EQ(crt_lxl_alloc_pool[0x00], 0x80);
}

/* flags&4 alloc path with allocator failure -> loader returns 0. */
static void test_lxl_alloc_fail(void)
{
    static uint8 src[CRT_LXL_SRC_SIZE];
    uint32 saved_fnptr;
    void  *r;
    int    i;

    for (i = 0; i < CRT_LXL_SRC_SIZE; i++) src[i] = 0;
    crt_lxl_build_image(src, 0x40, 0x20, 0x10, 1, 0x4, 0x00);

    saved_fnptr = data_ail_alloc_fnptr;
    data_ail_alloc_fnptr = (uint32)crt_lxl_test_alloc;
    crt_lxl_alloc_return_null = 1;

    r = crt_equivalent_lx_module_loader_3647b((char *)src, 1 | 4, (void *)0);

    data_ail_alloc_fnptr = saved_fnptr;
    crt_lxl_alloc_return_null = 0;

    ASSERT_TRUE(r == (void *)0);
}

/* ================================================================
 * crt_equivalent_exit_chain_stub_36de3 @ 0x36de3
 *
 * atexit default no-op handler: a 1-byte RET. The CRT seeds the three
 * atexit chain slots with this stub, and exit/_exit invoke an unused
 * slot via CALL [slot] expecting an immediate clean return. The only
 * observable contract is therefore: (1) calling it directly is a no-op
 * that returns to the caller, and (2) calling it THROUGH a function
 * pointer (its real invocation form) likewise returns cleanly with the
 * surrounding state intact. There is no return value to check.
 * ================================================================ */

/* direct call returns cleanly: sentinels bracketing a local are intact
 * after the call, and execution proceeds past it (a broken RET / stack
 * imbalance would corrupt the frame or fail to return). */
static void test_exit_stub_direct_call_is_noop(void)
{
    volatile int guard_lo = 0x11223344;
    volatile int marker   = 0;
    volatile int guard_hi = 0x55667788;

    crt_equivalent_exit_chain_stub_36de3();
    marker = 1;   /* reached only if the stub returned */

    ASSERT_EQ(marker, 1);
    ASSERT_EQ(guard_lo, 0x11223344);
    ASSERT_EQ(guard_hi, 0x55667788);
}

/* call THROUGH a function pointer, exactly as exit/_exit invoke the
 * atexit slots (CALL [slot]); the indirect call must also return
 * cleanly. Invoke it repeatedly to mirror the 3-slot chain walk. */
static void test_exit_stub_indirect_call_chain(void)
{
    void (*chain_slot)(void);
    int  i;
    int  completed;

    chain_slot = crt_equivalent_exit_chain_stub_36de3;
    ASSERT_TRUE(chain_slot != (void (*)(void))0);

    completed = 0;
    for (i = 0; i < 3; i++) {     /* slots 0x527d8 / 0x527dc / 0x527e0 */
        chain_slot();
        completed++;
    }
    ASSERT_EQ(completed, 3);
}

/* ================================================================
 * crt_equivalent_get_eflags_thunk @ 0x37f86
 *
 * Watcom `_disable` primitive: PUSHFD; POP EAX; CLI -> returns the prior
 * EFLAGS in EAX and disables interrupts (clears IF). The two observable
 * contracts are (1) the return value is a genuine live EFLAGS image, and
 * (2) the CLI side effect actually clears IF, while the call itself
 * remains stack-balanced (it is reached by a near CALL and must RET to
 * the caller without disturbing the frame).
 *
 * Test-local helpers (read-only / restore) keep the suite safe: a missed
 * timer tick during the few instructions IF is cleared is harmless, and
 * interrupts are re-enabled immediately after each observation so later
 * suites run normally.
 *
 * x86 EFLAGS architectural invariants used as deterministic, environment-
 * independent oracles: bit 1 is reserved and always reads 1; bits 3 and 5
 * are reserved and always read 0. A real PUSHFD image therefore satisfies
 * (eflags & 0x2A) == 0x02, which 0 / uninitialized garbage would not.
 * IF is bit 9 (0x200).
 * ================================================================ */

/* read-only EFLAGS snapshot: PUSHFD; POP EAX (no CLI -> unprivileged and
 * side-effect-free). Used to observe IF before/after the thunk. */
extern unsigned long test_read_eflags(void);
#pragma aux test_read_eflags = \
    0x9c    /* pushfd  */ \
    0x58    /* pop eax */ \
    value [eax] modify exact [eax];

/* re-enable interrupts after the thunk's CLI so the timer tick resumes. */
extern void test_enable_interrupts(void);
#pragma aux test_enable_interrupts = \
    0xfb    /* sti */ \
    modify exact [];

/* (1) the thunk returns a genuine live EFLAGS image (not 0 / garbage):
 * the architectural reserved-bit pattern must hold. */
static void test_eflags_thunk_returns_live_eflags(void)
{
    unsigned long r;

    r = crt_equivalent_get_eflags_thunk();
    test_enable_interrupts();     /* restore IF that the thunk's CLI cleared */

    /* bit 1 always set, bits 3 and 5 always clear in a real EFLAGS image */
    ASSERT_EQ((int)(r & 0x2A), 0x02);
}

/* (2) the value the thunk returns is captured BEFORE its own CLI runs, so
 * its IF bit reflects the prior interrupt state, and the CLI side effect
 * actually clears IF afterward (observed via a fresh read). With the timer
 * ISR live on entry, IF is set going in. */
static void test_eflags_thunk_disables_interrupts(void)
{
    unsigned long before;
    unsigned long captured;
    unsigned long after;

    before   = test_read_eflags();              /* IF state on entry        */
    captured = crt_equivalent_get_eflags_thunk();/* returns prior EFLAGS,CLI */
    after    = test_read_eflags();              /* IF after the CLI         */
    test_enable_interrupts();                   /* restore for later suites */

    /* the returned image is the pre-CLI snapshot: its IF matches `before` */
    ASSERT_EQ((int)(captured & 0x200), (int)(before & 0x200));
    /* interrupts were enabled on entry (timer ISR running) ... */
    ASSERT_EQ((int)(before & 0x200), 0x200);
    /* ... and the thunk's CLI cleared IF */
    ASSERT_EQ((int)(after & 0x200), 0x00);
}

/* (3) stack-balanced clean return, both direct and THROUGH a function
 * pointer (the original near-CALL / address-taken invocation form). Stack
 * guard sentinels bracketing a local must survive; a wrong cc (e.g. RET N)
 * or a non-callable in-line splice would corrupt the frame or fail to
 * return. */
static void test_eflags_thunk_clean_return(void)
{
    volatile int   guard_lo = 0x0BADF00D;
    volatile int   marker   = 0;
    volatile int   guard_hi = 0x0C0FFEE0;
    unsigned long (*fp)(void);
    unsigned long  r1;
    unsigned long  r2;

    r1 = crt_equivalent_get_eflags_thunk();   /* direct near call */
    test_enable_interrupts();
    marker = 1;

    fp = crt_equivalent_get_eflags_thunk;     /* address-taken -> out-of-line */
    ASSERT_TRUE(fp != (unsigned long (*)(void))0);
    r2 = fp();                                 /* indirect call */
    test_enable_interrupts();

    ASSERT_EQ(marker, 1);
    ASSERT_EQ(guard_lo, 0x0BADF00D);
    ASSERT_EQ(guard_hi, 0x0C0FFEE0);
    /* both invocation forms returned a real EFLAGS image */
    ASSERT_EQ((int)(r1 & 0x2A), 0x02);
    ASSERT_EQ((int)(r2 & 0x2A), 0x02);
}

/* ================================================================
 * crt_equivalent_fpe_default_handler_3d26e @ 0x3d26e
 *
 * SIGFPE / FPU-exception default no-op handler: a 1-byte RET. The CRT
 * seeds the FPE dispatch slot @ 0x5283c with this stub, and the exception
 * deliverers (__FPE_exception_ / __int7) invoke the slot via CALL [slot]
 * with the FPE code as the argument, expecting an immediate clean return.
 * The observable contract is therefore: (1) a direct __cdecl call passing
 * an fpe_code is a no-op that returns to the caller with the surrounding
 * stack intact, and (2) a call THROUGH a function pointer (its real
 * invocation form) likewise returns cleanly. There is no return value.
 * ================================================================ */

/* direct __cdecl call passing an fpe code returns cleanly: sentinels
 * bracketing a local are intact afterward and execution proceeds past the
 * call. A broken RET or a stub that wrongly cleaned the stack (RET 4)
 * would imbalance the cdecl frame and corrupt these guards. */
static void test_fpe_handler_direct_call_is_noop(void)
{
    volatile int guard_lo = 0x12345678;
    volatile int marker   = 0;
    volatile int guard_hi = 0x76543210;

    crt_equivalent_fpe_default_handler_3d26e(8);   /* 8 == typical SIGFPE */
    marker = 1;   /* reached only if the stub returned */

    ASSERT_EQ(marker, 1);
    ASSERT_EQ(guard_lo, 0x12345678);
    ASSERT_EQ(guard_hi, 0x76543210);
}

/* the argument is ignored: any fpe_code value behaves identically (the
 * body never reads it). Drive several distinct codes through the cdecl
 * call and confirm each returns and leaves the frame undisturbed. */
static void test_fpe_handler_ignores_code(void)
{
    static const int codes[4] = { 0, 8, -1, 0x7FFFFFFF };
    volatile int guard = 0x0FACADE0;
    int i;
    int completed;

    completed = 0;
    for (i = 0; i < 4; i++) {
        crt_equivalent_fpe_default_handler_3d26e(codes[i]);
        completed++;
    }

    ASSERT_EQ(completed, 4);
    ASSERT_EQ(guard, 0x0FACADE0);
}

/* call THROUGH a function pointer, exactly as __FPE_exception_ / __int7
 * invoke the dispatch slot (CALL [0x5283c]); the indirect call must also
 * return cleanly. The slot signature is void(int), so call it with a code
 * argument through that pointer type. */
static void test_fpe_handler_indirect_call(void)
{
    void (*handler_slot)(int);
    int  i;
    int  completed;

    handler_slot = crt_equivalent_fpe_default_handler_3d26e;
    ASSERT_TRUE(handler_slot != (void (*)(int))0);

    completed = 0;
    for (i = 0; i < 3; i++) {
        handler_slot(i);
        completed++;
    }
    ASSERT_EQ(completed, 3);
}

/* ================================================================
 * crt_equivalent_matherr_default_thunk_4d340 @ 0x4d340
 *
 * Default value of the user-matherr-handler slot @ 0x539A8: a 5-byte JMP to
 * the "return 0" primitive crt_equivalent_matherr_default_return_zero_4d8ea
 * @ 0x4d8ea. _matherr reads the slot and CALLs it as __cdecl
 * int(struct exception *exc) (PUSH exc; CALL [slot]; ADD ESP,4); a 0 result
 * means "not handled". The observable contracts are: (1) the thunk's JMP
 * actually reaches the target, which yields the return value 0; (2) the
 * exception-struct argument is ignored (the thunk JMPs straight through
 * without reading it); (3) the call is stack-balanced under cdecl (the
 * caller reclaims the pushed arg), both as a direct call and THROUGH a
 * function pointer — the address-taken slot-CALL form _matherr uses.
 *
 * The JMP target is the real primitive crt_equivalent_matherr_default_return_
 * zero_4d8ea @ 0x4d8ea (src/crt/crt.c), whose sole effect is returning 0; the
 * thunk's JMP reaching it is observable as the call yielding 0.
 * ================================================================ */

/* (1) the thunk's JMP reaches the target and the call yields 0 ("not
 * handled"): control tail-transfers through the JMP into the real "return 0"
 * primitive, whose 0 becomes the thunk's result. */
static void test_matherr_thunk_returns_zero_via_jmp(void)
{
    int dummy_exc;     /* stand-in exception struct; never read by the thunk */
    int r;

    r = crt_equivalent_matherr_default_thunk_4d340(&dummy_exc);

    ASSERT_EQ(r, 0);
}

/* (2) the exception-struct argument is ignored: any pointer value (including
 * NULL) behaves identically, returning 0 and reaching the target, because
 * the thunk JMPs straight through without dereferencing it. */
static void test_matherr_thunk_ignores_exc_arg(void)
{
    int  marker;
    void *args[3];
    int  i;
    int  completed;

    marker  = 0;
    args[0] = (void *)0;          /* NULL */
    args[1] = &marker;            /* valid pointer */
    args[2] = (void *)0xDEADBEEF; /* bogus, must not be dereferenced */

    completed = 0;
    for (i = 0; i < 3; i++) {
        ASSERT_EQ(crt_equivalent_matherr_default_thunk_4d340(args[i]), 0);
        completed++;
    }

    ASSERT_EQ(completed, 3);
    ASSERT_EQ(marker, 0);   /* the pointed-to slot was never touched */
}

/* (3) stack-balanced clean return, both direct and THROUGH a function
 * pointer (the slot-CALL form _matherr uses: CALL [0x539A8] with exc pushed,
 * caller cleans up). Guard sentinels bracketing a local must survive and the
 * cdecl frame must stay balanced; a wrong cc (e.g. RET 4 swallowing the arg)
 * or a CALL-instead-of-JMP frame shift would corrupt these guards. The
 * target stub must have run once per call. */
static void test_matherr_thunk_clean_return_direct_and_indirect(void)
{
    volatile int guard_lo = 0x0BADF00D;
    volatile int marker   = 0;
    volatile int guard_hi = 0x0C0FFEE0;
    int dummy_exc;
    int (*fp)(void *);
    int r1;
    int r2;

    r1 = crt_equivalent_matherr_default_thunk_4d340(&dummy_exc); /* direct */
    marker = 1;

    fp = crt_equivalent_matherr_default_thunk_4d340; /* address-taken -> PUBDEF */
    ASSERT_TRUE(fp != (int (*)(void *))0);
    r2 = fp(&dummy_exc);                              /* indirect (slot form) */

    ASSERT_EQ(marker, 1);
    ASSERT_EQ(guard_lo, 0x0BADF00D);
    ASSERT_EQ(guard_hi, 0x0C0FFEE0);
    ASSERT_EQ(r1, 0);  /* both forms tail-JMP into the real "return 0" -> 0 */
    ASSERT_EQ(r2, 0);
}

/* ================================================================
 * crt_equivalent_matherr_default_return_zero_4d8ea @ 0x4d8ea
 *
 * The "return 0" primitive the matherr thunk JMPs to (slot [0x539A8]'s
 * default handler body). Body: XOR EAX,EAX; RET — returns 0 for any input
 * and ignores its exception-struct pointer. Contracts: (1) returns 0 for
 * every exc argument, including the NULL and slot-CALL forms _matherr uses;
 * (2) __cdecl stack-balanced both direct and through a function pointer (the
 * address-taken slot form), with bracketing guard sentinels intact.
 *
 * Not in protos.h (it is a JMP target, reached only via the matherr thunk),
 * so it is declared locally here to call it directly.
 * ================================================================ */
extern int crt_equivalent_matherr_default_return_zero_4d8ea(void *exc);

/* (1) returns 0 regardless of the exception-struct pointer (NULL, a valid
 * pointer, and a bogus non-NULL one that must never be dereferenced). */
static void test_matherr_primitive_returns_zero_any_arg(void)
{
    int  marker;
    void *args[3];
    int  i;

    marker  = 0;
    args[0] = (void *)0;          /* NULL */
    args[1] = &marker;            /* valid pointer */
    args[2] = (void *)0xDEADBEEF; /* bogus, must not be dereferenced */

    for (i = 0; i < 3; i++) {
        ASSERT_EQ(crt_equivalent_matherr_default_return_zero_4d8ea(args[i]), 0);
    }
    ASSERT_EQ(marker, 0);   /* the pointed-to slot was never touched */
}

/* (2) __cdecl clean return, both direct and through a function pointer (the
 * address-taken slot form: CALL [0x539A8] with exc pushed, caller cleans up).
 * Guard sentinels bracketing a local must survive a balanced cdecl frame; a
 * wrong cc (e.g. RET 4 swallowing the arg) would corrupt them. */
static void test_matherr_primitive_clean_return_direct_and_indirect(void)
{
    volatile int guard_lo = 0x0BADF00D;
    volatile int marker   = 0;
    volatile int guard_hi = 0x0C0FFEE0;
    int dummy_exc;
    int (*fp)(void *);
    int r1;
    int r2;

    r1 = crt_equivalent_matherr_default_return_zero_4d8ea(&dummy_exc); /* direct */
    marker = 1;

    fp = crt_equivalent_matherr_default_return_zero_4d8ea; /* address-taken */
    ASSERT_TRUE(fp != (int (*)(void *))0);
    r2 = fp(&dummy_exc);                                   /* indirect (slot form) */

    ASSERT_EQ(marker, 1);
    ASSERT_EQ(guard_lo, 0x0BADF00D);
    ASSERT_EQ(guard_hi, 0x0C0FFEE0);
    ASSERT_EQ(r1, 0);
    ASSERT_EQ(r2, 0);
}

/* ================================================================
 * crt_equivalent_get_eflags @ 0x3ed58
 *
 * The 4-byte `_disable` primitive the thunk @ 0x37f86 JMPs into:
 * PUSHFD; POP EAX; CLI; RET. Same two observable contracts as the thunk
 * (it has the identical body): (1) the return value is a genuine live
 * EFLAGS image, and (2) the CLI side effect clears IF while the call
 * stays stack-balanced. Reuses the test_read_eflags / test_enable_
 * interrupts helpers defined above for the thunk suite (read-only / restore
 * so a missed timer tick while IF is clear is harmless). The reserved-bit
 * oracle (eflags & 0x2A) == 0x02 distinguishes a real EFLAGS value from
 * 0 / garbage; IF is bit 9 (0x200).
 * ================================================================ */

/* (1) returns a genuine live EFLAGS image (not 0 / garbage). */
static void test_get_eflags_returns_live_eflags(void)
{
    unsigned long r;

    r = crt_equivalent_get_eflags();
    test_enable_interrupts();     /* restore IF that the primitive's CLI cleared */

    ASSERT_EQ((int)(r & 0x2A), 0x02);
}

/* (2) the returned value is captured BEFORE the primitive's own CLI (its IF
 * reflects the prior state), and the CLI actually clears IF afterward. */
static void test_get_eflags_disables_interrupts(void)
{
    unsigned long before;
    unsigned long captured;
    unsigned long after;

    before   = test_read_eflags();          /* IF state on entry            */
    captured = crt_equivalent_get_eflags(); /* returns prior EFLAGS, then CLI */
    after    = test_read_eflags();          /* IF after the CLI             */
    test_enable_interrupts();               /* restore for later suites     */

    /* the returned image is the pre-CLI snapshot: its IF matches `before` */
    ASSERT_EQ((int)(captured & 0x200), (int)(before & 0x200));
    /* interrupts were enabled on entry (timer ISR running) ... */
    ASSERT_EQ((int)(before & 0x200), 0x200);
    /* ... and the primitive's CLI cleared IF */
    ASSERT_EQ((int)(after & 0x200), 0x00);
}

/* (3) stack-balanced clean return, both direct and THROUGH a function
 * pointer (the address-taken / out-of-line invocation form the thunk's JMP
 * target must support). Guard sentinels bracketing a local must survive. */
static void test_get_eflags_clean_return(void)
{
    volatile int   guard_lo = 0x0BADF00D;
    volatile int   marker   = 0;
    volatile int   guard_hi = 0x0C0FFEE0;
    unsigned long (*fp)(void);
    unsigned long  r1;
    unsigned long  r2;

    r1 = crt_equivalent_get_eflags();         /* direct near call */
    test_enable_interrupts();
    marker = 1;

    fp = crt_equivalent_get_eflags;           /* address-taken -> out-of-line */
    ASSERT_TRUE(fp != (unsigned long (*)(void))0);
    r2 = fp();                                 /* indirect call */
    test_enable_interrupts();

    ASSERT_EQ(marker, 1);
    ASSERT_EQ(guard_lo, 0x0BADF00D);
    ASSERT_EQ(guard_hi, 0x0C0FFEE0);
    ASSERT_EQ((int)(r1 & 0x2A), 0x02);
    ASSERT_EQ((int)(r2 & 0x2A), 0x02);
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
    RUN_TEST(test_lx_header_inmem_single_object);
    RUN_TEST(test_lx_header_inmem_multi_object);
    RUN_TEST(test_lx_header_inmem_zero_objects);
    RUN_TEST(test_lx_header_inmem_bad_magic);
    RUN_TEST(test_lx_header_file_multi_object);
    RUN_TEST(test_lx_header_file_open_fail);
    RUN_TEST(test_lxl_inmem_load_and_fixup);
    RUN_TEST(test_lxl_inmem_clamp_to_vsize);
    RUN_TEST(test_lxl_inmem_bad_magic);
    RUN_TEST(test_lxl_inmem_bad_fixup);
    RUN_TEST(test_lxl_alloc_path);
    RUN_TEST(test_lxl_alloc_fail);
    RUN_TEST(test_exit_stub_direct_call_is_noop);
    RUN_TEST(test_exit_stub_indirect_call_chain);
    RUN_TEST(test_eflags_thunk_returns_live_eflags);
    RUN_TEST(test_eflags_thunk_disables_interrupts);
    RUN_TEST(test_eflags_thunk_clean_return);
    RUN_TEST(test_get_eflags_returns_live_eflags);
    RUN_TEST(test_get_eflags_disables_interrupts);
    RUN_TEST(test_get_eflags_clean_return);
    RUN_TEST(test_fpe_handler_direct_call_is_noop);
    RUN_TEST(test_fpe_handler_ignores_code);
    RUN_TEST(test_fpe_handler_indirect_call);
    RUN_TEST(test_matherr_thunk_returns_zero_via_jmp);
    RUN_TEST(test_matherr_thunk_ignores_exc_arg);
    RUN_TEST(test_matherr_thunk_clean_return_direct_and_indirect);
    RUN_TEST(test_matherr_primitive_returns_zero_any_arg);
    RUN_TEST(test_matherr_primitive_clean_return_direct_and_indirect);
    printf("\n");
}
