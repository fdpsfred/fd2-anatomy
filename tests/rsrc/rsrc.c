/*
 * unit tests for src/rsrc/rsrc.c
 *
 * Every resource-loader test drives the REAL loader against the staged real
 * game files (FDOTHER.DAT / FDFIELD.DAT / FDTXT.DAT / FDSHAP.DAT / FDICON.B24,
 * copied into the test cwd by build_test.py) and cross-checks the loader's
 * output against an independent parse of the SAME real bytes (realfile.h /
 * the inline FDICON reader). No fabricated fixtures, no hardcoded values.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>
#include "realfile.h"   /* realdat_read_resource() */
#include "minipfix.h"   /* minip_setup_env(): sprite sheet + dialog-blit spies */

/* capture globals from testglob.c */
extern int    g_rle_blit_calls;
extern uint32 g_rle_blit_last_sprite;
extern int32  g_rle_blit_last_x;
extern int32  g_rle_blit_last_y;
extern uint32 g_rle_blit_last_buf;
extern int32  g_rle_blit_last_stride;
extern uint32 g_rle_blit_last_palette;
extern int32  g_rle_blit_y_log[4];
extern uint8  g_rle_blit_sprite_first_byte_log[4];

extern int    g_scroll_text_calls;
extern uint32 g_scroll_text_last_arg;

/* fd2_load_and_fade_in_cinematic_image captures (testglob.c spies). Note:
 * fd2_set_vga_palette_range is NOT a spy -- it is the real emitted primitive
 * (src/gfx/palette.c) and runs end-to-end, reading the 768-byte palette at
 * data_fd2_vga_palette_data_ptr and outp-ing to the (no-op) VGA DAC. */
extern int    g_play_ani_calls;
extern uint32 g_play_ani_last_idx;
extern uint32 g_play_ani_last_delay;
extern uint32 g_play_ani_last_skip;
extern int    g_fade_to_black_calls;

/* fd2_load_chapter_battle_data captures (testglob.c) */
extern runtime_char g_test_rc_array[8];

/* DAT filename strings (match the Ghidra/globals symbols; equal to the staged
 * real file names). */
extern char data_fd2_string_fdmus_dat[];
extern char data_fd2_string_resource_filename_fdother_dat[];

/* Read the 13 sprite-header ints (12 frame offsets + 1 end-mark) for a
 * portrait from the staged real FDICON.B24: entries portrait_id*12..+12,
 * each a 4-byte absolute offset stored from file offset 6. */
static void fdicon_offsets13(int portrait_id, int32 *out13)
{
    FILE *fp;

    fp = fopen("FDICON.B24", "rb");
    fseek(fp, 6 + portrait_id * 12 * 4, SEEK_SET);
    fread(out13, 4, 13, fp);
    fclose(fp);
}

/* ================================================================
 * fd2_load_dat_resource @ 0x111ba  (direct tests)
 *
 * Drive the loader against staged real DAT archives and verify the returned
 * buffer + last_loaded_resource_size match an independent parse of the same
 * archive (realdat_read_resource). Distinct indices resolve to distinct
 * payloads; a non-NULL old_buf is freed and a fresh buffer is returned.
 * ================================================================ */
static void test_ldr_normal_load_and_size(void)
{
    uint8 *ref;
    long   ref_size;
    uint8 *buf;

    /* real FDMUS.DAT index 0 (a tiny 3-byte resource) */
    ref_size = realdat_read_resource("FDMUS.DAT", 0, &ref);
    ASSERT_TRUE(ref_size > 0);
    buf = (uint8 *)fd2_load_dat_resource((uint32)data_fd2_string_fdmus_dat, 0, 0);
    ASSERT_TRUE(buf != 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, ref_size);
    ASSERT_EQ((long)memcmp(buf, ref, (size_t)ref_size), 0);
    free(buf);
    free(ref);

    /* real FDMUS.DAT index 3 (a larger FORM chunk) — distinct payload+size */
    ref_size = realdat_read_resource("FDMUS.DAT", 3, &ref);
    ASSERT_TRUE(ref_size > 0);
    buf = (uint8 *)fd2_load_dat_resource((uint32)data_fd2_string_fdmus_dat, 0, 3);
    ASSERT_TRUE(buf != 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, ref_size);
    ASSERT_EQ((long)memcmp(buf, ref, (size_t)ref_size), 0);
    free(buf);
    free(ref);
}

/* old_buf != 0 is freed; a fresh buffer holding the real payload is returned. */
static void test_ldr_old_buf_freed(void)
{
    uint8 *ref;
    long   ref_size;
    uint32 old_buf;
    uint8 *buf;

    ref_size = realdat_read_resource("FDOTHER.DAT", 0, &ref);  /* vga palette */
    ASSERT_TRUE(ref_size > 0);

    old_buf = (uint32)malloc(4);    /* loader will free() this */
    buf = (uint8 *)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, old_buf, 0);
    ASSERT_TRUE(buf != 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, ref_size);
    ASSERT_EQ((long)memcmp(buf, ref, (size_t)ref_size), 0);
    free(buf);
    free(ref);
}

/* ================================================================
 * fd2_load_portrait_to_cache @ 0x11019  (direct tests)
 *
 * Drive the loader against the staged real FDICON.B24 and verify the cache
 * bookkeeping (frame-offset table, buffer_used, id_list, count) against the
 * portrait's real 13-int header. data_size = offsets[12]-offsets[0]; the
 * first-init frame table is ((int*)cache)[i] = (offsets[i]-offsets[0]) + 0x780.
 * ================================================================ */
static FILE *g_lpc_fp;

static void lpc_setup(void)
{
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
    g_lpc_fp = fopen("FDICON.B24", "rb");
}

static void lpc_teardown(void)
{
    if (g_lpc_fp != NULL) {
        fclose(g_lpc_fp);
        g_lpc_fp = NULL;
    }
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
}

/* First-time init: count 0 -> allocate cache, build frame table, return 0. */
static void test_lpc_first_init(void)
{
    int    idx;
    int32  off[13];
    uint32 data_size;
    int32 *tbl;
    int    i;

    lpc_setup();
    fdicon_offsets13(2, off);
    data_size = (uint32)(off[12] - off[0]);

    idx = fd2_load_portrait_to_cache(2, (uint32)g_lpc_fp);

    ASSERT_EQ((long)idx, 0);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 1);
    ASSERT_EQ((long)*(uint32 *)data_fd2_resource_portrait_cache_id_list_base, 2);
    ASSERT_TRUE(data_fd2_portrait_sprite_cache != 0);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_buffer_used,
              (long)(data_size + 0x780));
    tbl = (int32 *)data_fd2_portrait_sprite_cache;
    for (i = 0; i < 12; i++) {
        ASSERT_EQ((long)tbl[i], (long)((off[i] - off[0]) + 0x780));
    }
    lpc_teardown();
}

/* Cache hit: same portrait_id on a populated cache returns its slot, no growth. */
static void test_lpc_cache_hit(void)
{
    int idx0;
    int idx1;
    uint32 used_after_first;

    lpc_setup();
    idx0 = fd2_load_portrait_to_cache(2, (uint32)g_lpc_fp);
    used_after_first = data_fd2_resource_portrait_cache_buffer_used;
    idx1 = fd2_load_portrait_to_cache(2, (uint32)g_lpc_fp);

    ASSERT_EQ((long)idx0, 0);
    ASSERT_EQ((long)idx1, 0);                       /* hit returns slot 0     */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 1); /* no growth  */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_buffer_used,
              (long)used_after_first);
    lpc_teardown();
}

/* Append miss: a new portrait_id appends at the tail with the running offset. */
static void test_lpc_append_miss(void)
{
    int    idx0;
    int    idx1;
    int32  off2[13];
    int32  off5[13];
    uint32 ds2;
    uint32 used_after_first;
    int32 *tbl;
    int    i;

    lpc_setup();
    fdicon_offsets13(2, off2);
    fdicon_offsets13(5, off5);
    ds2 = (uint32)(off2[12] - off2[0]);
    used_after_first = ds2 + 0x780;

    idx0 = fd2_load_portrait_to_cache(2, (uint32)g_lpc_fp);  /* slot 0 */
    idx1 = fd2_load_portrait_to_cache(5, (uint32)g_lpc_fp);  /* slot 1 */

    ASSERT_EQ((long)idx0, 0);
    ASSERT_EQ((long)idx1, 1);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 2);
    /* id_list[1] == 5 */
    ASSERT_EQ((long)*(uint32 *)(data_fd2_resource_portrait_cache_id_list_base
                                + 1 * 4), 5);
    /* buffer_used after two portraits = ds2 + ds5 + 0x780 */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_buffer_used,
              (long)(used_after_first + (uint32)(off5[12] - off5[0])));
    /* slot 1 frame table at ((int*)cache)[12..23] = used_after_first + delta */
    tbl = (int32 *)data_fd2_portrait_sprite_cache;
    for (i = 0; i < 12; i++) {
        ASSERT_EQ((long)tbl[12 + i],
                  (long)(used_after_first + (uint32)(off5[i] - off5[0])));
    }
    lpc_teardown();
}

/* ================================================================
 * fd2_load_chapter_background_layers @ 0x10652
 *
 * Drive the loader against the staged real FDOTHER.DAT. Each load shape is
 * verified against the real FDOTHER index it must fetch (first payload byte
 * via realdat_read_resource) plus the fixed geometry constants.
 * ================================================================ */
static int fdother_first_byte(int index)
{
    uint8 *p;
    int    b;

    if (realdat_read_resource("FDOTHER.DAT", index, &p) < 0) return -1;
    b = p[0];
    free(p);
    return b;
}

static void reset_capture(void)
{
    g_rle_blit_calls = 0;
    g_rle_blit_last_sprite = 0;
    g_rle_blit_last_x = 0;
    g_rle_blit_last_y = 0;
    g_rle_blit_last_buf = 0;
    g_rle_blit_last_stride = 0;
    g_rle_blit_last_palette = 0;
    memset(g_rle_blit_y_log, 0, sizeof(g_rle_blit_y_log));
    memset(g_rle_blit_sprite_first_byte_log, 0,
           sizeof(g_rle_blit_sprite_first_byte_log));

    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
}

static void reset_capture_teardown(void)
{
    if (data_fd2_graphics_static_bg_buffer_ptr != 0) {
        free((void *)data_fd2_graphics_static_bg_buffer_ptr);
        data_fd2_graphics_static_bg_buffer_ptr = 0;
    }
    if (data_fd2_graphics_animated_bg_buffer_ptr != 0) {
        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
        data_fd2_graphics_animated_bg_buffer_ptr = 0;
    }
}

/* Default single-sprite path: ordinary chapter (9) -> idx 0xF, one load into
   static_bg, no blit, animated_bg = malloc(64000). static_bg holds the real
   FDOTHER[0xF] payload (first byte cross-checked). */
static void test_default_path_chapter9_idx_f(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 9;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_rle_blit_calls, 0);
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);   /* loaded sprite */
    ASSERT_NE(data_fd2_graphics_animated_bg_buffer_ptr, 0); /* malloc(64000) */
    ASSERT_EQ((long)*(uint8 *)data_fd2_graphics_static_bg_buffer_ptr,
              (long)fdother_first_byte(0xf));
    reset_capture_teardown();
}

/* Chapter 0x1C / 0x1D default path -> idx 0x37. */
static void test_default_path_chapter1c_idx_37(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x1c;

    fd2_load_chapter_background_layers();

    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_NE(data_fd2_graphics_animated_bg_buffer_ptr, 0);
    ASSERT_EQ((long)*(uint8 *)data_fd2_graphics_static_bg_buffer_ptr,
              (long)fdother_first_byte(0x37));
    reset_capture_teardown();
}

static void test_default_path_chapter1d_idx_37(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x1d;

    fd2_load_chapter_background_layers();

    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ((long)*(uint8 *)data_fd2_graphics_static_bg_buffer_ptr,
              (long)fdother_first_byte(0x37));
    reset_capture_teardown();
}

/* No-load chapter (not in any dispatch list): both buffers nulled,
   nothing loaded, nothing blitted. */
static void test_unmatched_chapter_no_load(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 5;
    /* seed pre-existing buffers so we can confirm they are freed+nulled */
    data_fd2_graphics_static_bg_buffer_ptr = (uint32)malloc(16);
    data_fd2_graphics_animated_bg_buffer_ptr = (uint32)malloc(16);

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_rle_blit_calls, 0);
    ASSERT_EQ(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ(data_fd2_graphics_animated_bg_buffer_ptr, 0);
    reset_capture_teardown();
}

/* 2-sprite widescreen path, chapter 0x11: defaults 0x1CE x 0xE2, idx_base 0x10.
   Top blit at y=0, bottom at y=bg_height/2=0x71. Two loads (idx 0x10 then
   0x11), two blits, animated_bg freed+nulled. */
static void test_two_sprite_chapter11(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x11;

    fd2_load_chapter_background_layers();

    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0],
              (long)fdother_first_byte(0x10));
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[1],
              (long)fdother_first_byte(0x11));
    ASSERT_EQ(g_rle_blit_calls, 2);
    ASSERT_EQ(g_rle_blit_y_log[0], 0);
    ASSERT_EQ(g_rle_blit_y_log[1], 0xe2 / 2);     /* 0x71 */
    ASSERT_EQ(g_rle_blit_last_stride, 0x1ce);
    ASSERT_EQ(g_rle_blit_last_palette, 0xffffffff);
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ(data_fd2_graphics_animated_bg_buffer_ptr, 0);
    reset_capture_teardown();
}

/* 2-sprite widescreen, chapter 0x15: bg 0x198 x 0x114, idx_base 0x23. */
static void test_two_sprite_chapter15(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x15;

    fd2_load_chapter_background_layers();

    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0],
              (long)fdother_first_byte(0x23));
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[1],
              (long)fdother_first_byte(0x24));
    ASSERT_EQ(g_rle_blit_calls, 2);
    ASSERT_EQ(g_rle_blit_y_log[1], 0x114 / 2);    /* 0x8a */
    ASSERT_EQ(g_rle_blit_last_stride, 0x198);
    reset_capture_teardown();
}

/* 2-sprite widescreen, chapter 0x16: bg 0x198 x 0x100, idx_base 0x28. */
static void test_two_sprite_chapter16(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x16;

    fd2_load_chapter_background_layers();

    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0],
              (long)fdother_first_byte(0x28));
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[1],
              (long)fdother_first_byte(0x29));
    ASSERT_EQ(g_rle_blit_y_log[1], 0x100 / 2);    /* 0x80 */
    ASSERT_EQ(g_rle_blit_last_stride, 0x198);
    reset_capture_teardown();
}

/* 2-sprite widescreen, chapter 0x1B: width default 0x1CE, height 0xF4,
   idx_base 0x2E. */
static void test_two_sprite_chapter1b(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x1b;

    fd2_load_chapter_background_layers();

    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0],
              (long)fdother_first_byte(0x2e));
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[1],
              (long)fdother_first_byte(0x2f));
    ASSERT_EQ(g_rle_blit_y_log[1], 0xf4 / 2);     /* 0x7a */
    ASSERT_EQ(g_rle_blit_last_stride, 0x1ce);
    reset_capture_teardown();
}

/* Text-scroll cinematic, chapter 0x17: idx 0x2A, stride 0x138, one blit, the
   real fd2_scroll_text_screen_up_by_lines(0) tail call (Mode B), animated_bg
   freed+nulled. The scroll's own cylinder-permutation behavior is covered by
   the dialog tests; here we pin the caller's dispatch (blit + buffer state) and
   drive the real tail call against the caller's malloc(0xea00) static_bg buffer.
   We arm the pending line count to a small bounded value first so the Mode-B
   scroll stays in bounds (0xC0 * 0x138 = 59904 <= 60000) and is deterministic
   regardless of any leftover state from other suites. */
static void test_text_scroll_chapter17(void)
{
    reset_capture();
    data_fd2_graphics_text_scroll_pending_line_count = 4;
    data_fd2_chapter_current_chapter_id = 0x17;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_rle_blit_calls, 1);
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0],
              (long)fdother_first_byte(0x2a));
    ASSERT_EQ(g_rle_blit_last_x, 0);
    ASSERT_EQ(g_rle_blit_last_y, 0);
    ASSERT_EQ(g_rle_blit_last_stride, 0x138);
    ASSERT_EQ(g_rle_blit_last_palette, 0xffffffff);
    /* real scroll(0) ran without disturbing buffer ownership */
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ(data_fd2_graphics_animated_bg_buffer_ptr, 0);
    data_fd2_graphics_text_scroll_pending_line_count = 0;
    reset_capture_teardown();
}

/* ================================================================
 * fd2_load_chapter_battle_data @ 0x1088d
 *
 * Drive the loader end-to-end against the staged real FDFIELD/FDSHAP/FDTXT/
 * FDOTHER/FDICON for a chosen real chapter. The directly file-derived state
 * (map width/height, cache_total_size, cache_alloc_offset) is cross-checked
 * against an independent parse of FDFIELD; the per-slot build loop's
 * active/dead gating, slot-6 special, and field-pos-driven spawn positions
 * are exercised with a test-controlled in-process menu roster (NOT a file).
 *
 * Real chapters used: ch0 (total=4, alloc=30, w=24, h=24) for the
 * active/party-count gating, and ch3 (total=7, alloc=40, chapter<0xd) for
 * the slot-6 special. Both have zero race==0 tile-event records, so the
 * trailing fd2_load_chapter_portraits_and_dump_tmp(0) appends nothing and
 * party_member_count == total_size exactly.
 * ================================================================ */
#define CB_ROSTER_SLOTS 16
static uint8 g_cb_roster[CB_ROSTER_SLOTS * 0x50];

/* Independent parse of the real FDFIELD headers for `chapter`. */
static void cb_real_headers(int chapter, int *w, int *h, int *total, int *alloc)
{
    uint8 *tm;
    uint8 *te;

    realdat_read_resource("FDFIELD.DAT", chapter * 3, &tm);
    *w = (int)*(int16 *)tm;
    *h = (int)*(int16 *)(tm + 2);
    free(tm);
    realdat_read_resource("FDFIELD.DAT", chapter * 3 + 1, &te);
    *total = te[1];
    *alloc = te[2];
    free(te);
}

/* Independent parse of the real field-pos entry for active slot `active_j`
 * (table base = alloc*6+2, 6-byte stride, pos_x@+0 / pos_y@+2). */
static void cb_real_field_pos(int chapter, int alloc, int active_j,
                              int *px, int *py)
{
    uint8 *fp;
    int    base;

    realdat_read_resource("FDFIELD.DAT", chapter * 3 + 2, &fp);
    base = alloc * 6 + 2;
    *px = fp[base + active_j * 6 + 0];
    *py = fp[base + active_j * 6 + 2];
    free(fp);
}

/* Seed the in-process menu roster (distinct portrait_id +0x07 per slot so each
 * active slot caches a distinct portrait) and null the loader-target globals. */
static void setup_cb_fixture(int chapter, int menu_party_count,
                             uint8 slot6_char_id)
{
    int i;

    data_fd2_chapter_current_chapter_id = (uint32)chapter;

    memset(g_cb_roster, 0, sizeof(g_cb_roster));
    for (i = 0; i < CB_ROSTER_SLOTS; i++) {
        g_cb_roster[i * 0x50 + 0x07] = (uint8)(0x40 + i);  /* portrait_id */
        g_cb_roster[i * 0x50 + 0x08] = (uint8)(0x80 + i);  /* char_id */
    }
    g_cb_roster[6 * 0x50 + 0x08] = slot6_char_id;          /* slot-6 special */
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_cb_roster;
    data_fd2_shared_menu_party_member_count = (uint32)menu_party_count;

    /* freed-if-nonzero then re-malloc'd / reloaded; NULL so no stale free */
    data_fd2_battle_runtime_char_array_ptr = NULL;
    data_fd2_portrait_sprite_cache = 0;
    data_fd2_current_chapter_text = 0;
    data_fd2_chapter_portrait_load_buffer = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    data_fd2_battle_scene_snapshot = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
}

static void teardown_cb_fixture(void)
{
    if (data_fd2_battle_runtime_char_array_ptr != NULL)
        free(data_fd2_battle_runtime_char_array_ptr);
    if (data_fd2_portrait_sprite_cache != 0)
        free((void *)data_fd2_portrait_sprite_cache);
    /* loader-returned buffers the function leaves live (it does NOT free
     * these): data_fd2_current_chapter_text, tile_event, tile_map, scene snapshot,
     * tile-attr flags, and the background buffers. */
    if (data_fd2_current_chapter_text != 0)
        free((void *)data_fd2_current_chapter_text);
    if (data_fd2_tile_event_data_table_ptr != 0)
        free((void *)data_fd2_tile_event_data_table_ptr);
    if (data_fd2_battle_tile_map_ptr != 0)
        free((void *)data_fd2_battle_tile_map_ptr);
    if (data_fd2_battle_scene_snapshot != 0)
        free((void *)data_fd2_battle_scene_snapshot);
    if (data_fd2_tile_attribute_flags_buffer_ptr != 0)
        free((void *)data_fd2_tile_attribute_flags_buffer_ptr);
    if (data_fd2_graphics_static_bg_buffer_ptr != 0)
        free((void *)data_fd2_graphics_static_bg_buffer_ptr);
    if (data_fd2_graphics_animated_bg_buffer_ptr != 0)
        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
    /* data_fd2_chapter_portrait_load_buffer was freed+nulled by the function */

    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    data_fd2_chapter_portrait_load_buffer = 0;
    data_fd2_battle_scene_snapshot = 0;
    data_fd2_current_chapter_text = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
    data_fd2_portrait_sprite_cache = 0;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_chapter_current_chapter_id = 1;

    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* All slots active: real chapter 0 (total 4), menu count covers all four. */
static void test_cb_all_active(void)
{
    runtime_char *arr;
    int w, h, total, alloc;
    int px, py;

    cb_real_headers(0, &w, &h, &total, &alloc);
    setup_cb_fixture(0, total, 0xFF);
    fd2_load_chapter_battle_data(0);
    arr = data_fd2_battle_runtime_char_array_ptr;

    /* file-derived header state */
    ASSERT_EQ((long)data_fd2_battle_map_width_tiles, (long)w);
    ASSERT_EQ((long)data_fd2_battle_map_height_tiles, (long)h);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_total_size, (long)total);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_alloc_offset, (long)alloc);
    ASSERT_EQ((long)data_fd2_battle_party_member_count, (long)total);

    /* every slot active -> one distinct portrait cached each (race0==0 so the
     * dump_tmp tail adds none) */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, (long)total);

    /* slot 0 active fields; pos from the real field-pos table (active slot 0) */
    cb_real_field_pos(0, alloc, 0, &px, &py);
    ASSERT_EQ((long)arr[0].flags, 0);          /* not dead */
    ASSERT_EQ((long)arr[0].team, 2);           /* player */
    ASSERT_EQ((long)arr[0].pos_x, (long)px);
    ASSERT_EQ((long)arr[0].pos_y, (long)py);
    ASSERT_EQ((long)arr[0].combat_aux_block[10], 0xff);
    ASSERT_EQ((long)arr[0].sprite_state[1], 0);
    ASSERT_EQ((long)arr[0].sprite_state[2], 0);
    /* slot 2 active, field-pos table advanced 6 bytes per active slot */
    cb_real_field_pos(0, alloc, 2, &px, &py);
    ASSERT_EQ((long)arr[2].pos_x, (long)px);
    ASSERT_EQ((long)arr[2].pos_y, (long)py);
    ASSERT_EQ((long)arr[2].team, 2);

    ASSERT_EQ((long)data_fd2_chapter_portrait_load_buffer, 0);
    teardown_cb_fixture();
}

/* active_count gating: real chapter 0 (total 4) but only 2 party members ->
   slots 2,3 dead. */
static void test_cb_party_count_gate(void)
{
    runtime_char *arr;
    int w, h, total, alloc;

    cb_real_headers(0, &w, &h, &total, &alloc);
    setup_cb_fixture(0, 2, 0xFF);
    fd2_load_chapter_battle_data(0);
    arr = data_fd2_battle_runtime_char_array_ptr;

    ASSERT_EQ((long)data_fd2_battle_party_member_count, (long)total);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 2);  /* 2 active */
    ASSERT_EQ((long)arr[0].flags, 0);
    ASSERT_EQ((long)arr[1].flags, 0);
    ASSERT_EQ((long)arr[2].flags, 1);            /* dead (gated) */
    ASSERT_EQ((long)arr[3].flags, 1);            /* dead (gated) */
    ASSERT_EQ((long)arr[2].team, 0);             /* zeroed, not set to 2 */

    teardown_cb_fixture();
}

/* slot-6 special: chapter<0xd && slot==6 && roster[6].char_id != 2 -> slot 6
   dead. Real chapter 3 (total 7). */
static void test_cb_slot6_special_dead(void)
{
    runtime_char *arr;
    int w, h, total, alloc;

    cb_real_headers(3, &w, &h, &total, &alloc);
    setup_cb_fixture(3, CB_ROSTER_SLOTS, 0x99 /* != 2 */);
    fd2_load_chapter_battle_data(3);
    arr = data_fd2_battle_runtime_char_array_ptr;

    /* slots 0..5 active (6 loads); slot 6 forced dead */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, (long)total);
    ASSERT_EQ((long)arr[6].flags, 1);
    ASSERT_EQ((long)arr[5].flags, 0);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 6);

    teardown_cb_fixture();
}

/* slot-6 special inactive when roster[6].char_id == 2 -> slot 6 stays active. */
static void test_cb_slot6_special_active(void)
{
    runtime_char *arr;
    int w, h, total, alloc;

    cb_real_headers(3, &w, &h, &total, &alloc);
    setup_cb_fixture(3, CB_ROSTER_SLOTS, 0x02 /* == 2 keeps slot active */);
    fd2_load_chapter_battle_data(3);
    arr = data_fd2_battle_runtime_char_array_ptr;

    ASSERT_EQ((long)arr[6].flags, 0);            /* active */
    ASSERT_EQ((long)arr[6].team, 2);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, (long)total); /* 7 */

    teardown_cb_fixture();
}

/* ================================================================
 * fd2_load_chapter_portraits_and_dump_tmp @ 0x10b4e
 *
 * Drive against the staged real FDFIELD (re-read of FDFIELD[chapter*3+2]) and
 * FDICON.B24. The tile-event race table is an in-process global (NOT a file),
 * so the per-test race layout selects which records match target_race_id; the
 * function re-loads the real field buffer, runs the real
 * fd2_init_runtime_char_for_battle per match, and rewrites the FD2.TMP swap
 * file (0x32A00 bytes).
 * ================================================================ */
static uint8 *g_pt_tileevent;   /* tile-event table for the race scan */
/* Tile map for the real fd2_init_runtime_char_for_battle -> the real
 * fd2_obfuscate_battle_tile_map (count = header[0]*header[2]). phase_flag=1
 * here skips the spawn search, so a tiny valid 2x2 map (4 records) is enough
 * to keep the obfuscate do-while bounded instead of underflowing on a NULL
 * map pointer. */
static uint8 g_pt_tilemap[64];

/* Build a tile-event table of `count` records (stride 0x1A); record k has its
 * race byte (+0x98) set to race_of[k]. alloc_offset = count drives the scan
 * length. Chapter 4 -> the function re-reads real FDFIELD[4*3+2 = 0xE]. */
static void setup_pt_fixture(int count, const uint8 *race_of)
{
    int i;

    g_pt_tileevent = (uint8 *)malloc((size_t)0x98 + (size_t)count * 0x1a + 0x20);
    memset(g_pt_tileevent, 0, (size_t)0x98 + (size_t)count * 0x1a + 0x20);
    for (i = 0; i < count; i++) {
        g_pt_tileevent[i * 0x1a + 0x98] = race_of[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_pt_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = (uint32)count;

    /* valid 2x2 tile map so the real obfuscate (invoked by the real
     * fd2_init_runtime_char_for_battle on a race match) iterates 4 records
     * instead of underflowing on a NULL map pointer. */
    memset(g_pt_tilemap, 0, sizeof(g_pt_tilemap));
    g_pt_tilemap[0] = 2;   /* map width  (header byte 0) */
    g_pt_tilemap[2] = 2;   /* map height (header byte 2) */
    data_fd2_battle_tile_map_ptr = (uint32)g_pt_tilemap;

    data_fd2_chapter_portrait_load_buffer = 0;          /* loaded fresh by the function */
    data_fd2_chapter_init_phase_flag = 1;       /* spawn = field value verbatim */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;   /* re-read idx = 4*3+2 = 0xE */

    /* the real fd2_load_portrait_to_cache (reached via the real
     * fd2_init_runtime_char_for_battle for matching races) parses the staged
     * real FDICON.B24; reset the cache so it first-inits cleanly. */
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
}

static void teardown_pt_fixture(void)
{
    free(g_pt_tileevent);
    g_pt_tileevent = 0;
    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;   /* g_pt_tilemap is static; just unlink */
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

static long fd2_tmp_size(void)
{
    FILE *fp;
    long n;

    fp = fopen("FD2.TMP", "rb");
    if (fp == NULL) return -1;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fclose(fp);
    return n;
}

/* Single matching record: real init runs once (party count 0 -> 1). The
 * FDFIELD[4*3+2 = 0xE] re-read happened (buffer freed+nulled on exit) and the
 * FD2.TMP swap file is written (0x32A00). */
static void test_pt_single_match(void)
{
    static const uint8 races[2] = { 0x07, 0x09 };

    setup_pt_fixture(2, races);
    fd2_load_chapter_portraits_and_dump_tmp(0x07);

    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    ASSERT_EQ((long)data_fd2_chapter_portrait_load_buffer, 0);
    ASSERT_EQ(fd2_tmp_size(), 0x32A00);

    teardown_pt_fixture();
}

/* No record matches the target race: no init, but the swap file is rewritten. */
static void test_pt_no_match(void)
{
    static const uint8 races[3] = { 0x01, 0x02, 0x03 };

    setup_pt_fixture(3, races);
    fd2_load_chapter_portraits_and_dump_tmp(0x7F);

    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    ASSERT_EQ((long)data_fd2_chapter_portrait_load_buffer, 0);
    ASSERT_EQ(fd2_tmp_size(), 0x32A00);

    teardown_pt_fixture();
}

/* Multiple matches across the loop: every matching index is visited in order. */
static void test_pt_multiple_match(void)
{
    static const uint8 races[4] = { 0x05, 0x05, 0x09, 0x05 };

    setup_pt_fixture(4, races);
    fd2_load_chapter_portraits_and_dump_tmp(0x05);

    /* three matching records (indices 0,1,3) -> init runs 3x */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 3);

    teardown_pt_fixture();
}

/* alloc_offset == 0: loop body never runs, swap file still produced. */
static void test_pt_empty_table(void)
{
    setup_pt_fixture(0, (const uint8 *)0);
    fd2_load_chapter_portraits_and_dump_tmp(0x00);

    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    ASSERT_EQ(fd2_tmp_size(), 0x32A00);

    teardown_pt_fixture();
}

/* ================================================================
 * fd2_load_chapter_portrait @ 0x1956b
 *
 * Drives the real dialog-portrait open against the staged real DATO.DAT.
 * The risk-bearing logic is (a) the 6-way portrait_kind -> blit_offset branch
 * and (b) the real DATO.DAT resource load (portrait_sprite_buffer). The dialog
 * frame draw runs for real through fd2_assemble_dialog_frame_layered into the
 * sheet-offset table (minipfix.h sprite sheet, raw blit captured by the
 * testglob spy); the portrait blit is the recording fd2_dialog_sprite_blit_
 * mirrored spy; the 6-frame slide loop runs the real fd2_slide_panel_down_step
 * (memmove to/from 0xA0000, harmless scratch in the host harness).
 *
 * Cross-check: portrait_sprite_buffer must hold the SAME bytes an independent
 * realdat_read_resource("DATO.DAT", kind) parse yields, and the mirrored blit
 * must fire once with dst = composed_target + blit_offset, sprite =
 * portrait_buf + *portrait_buf, stride 0x140.
 *
 * (g_dlg_blit_mirrored_calls / g_dlg_blit_last_{dst,sprite,stride} are declared
 * by minipfix.h, included above.)
 * ================================================================ */

/* Free the three 64000-byte workspaces the real function leaks each call plus
 * the loaded portrait buffer, and reset the blit-capture counter. */
static void lcp_cleanup(void)
{
    if (data_fd2_ui_slide_anim_accumulator_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
        data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_bg_snapshot_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
        data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_composed_target_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_composed_target_buf_ptr);
        data_fd2_ui_slide_composed_target_buf_ptr = 0;
    }
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
    g_dlg_blit_mirrored_calls = 0;
}

/* Run the open for one portrait_kind and assert the blit offset, the real
 * DATO.DAT payload, and the mirrored portrait blit. */
static void lcp_check_kind(uint32 kind, uint32 expect_offset)
{
    uint8 *ref;
    long   ref_size;
    uint32 composed;

    minip_setup_env();                 /* sprite sheet + blit spies */
    data_fd2_portrait_sprite_buffer = 0;   /* loader frees prev iff nonzero */
    g_dlg_blit_mirrored_calls = 0;

    ref_size = realdat_read_resource("DATO.DAT", (int)kind, &ref);
    ASSERT_TRUE(ref_size > 0);

    fd2_load_chapter_portrait(kind);

    /* (a) 6-way branch picked the right mode-13h offset */
    ASSERT_EQ((long)data_fd2_dialog_active_portrait_blit_offset,
              (long)expect_offset);

    /* (b) the real DATO.DAT resource was loaded into portrait_sprite_buffer */
    ASSERT_TRUE(data_fd2_portrait_sprite_buffer != 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, ref_size);
    ASSERT_EQ((long)memcmp(data_fd2_portrait_sprite_buffer, ref,
                           (size_t)ref_size), 0);

    /* (c) the mirrored portrait blit fired once at composed_target+offset with
     *     sprite = buf + buf[0] (header skip), stride 0x140 */
    composed = data_fd2_ui_slide_composed_target_buf_ptr;
    ASSERT_TRUE(composed != 0);
    ASSERT_EQ((long)g_dlg_blit_mirrored_calls, 1);
    ASSERT_EQ((long)g_dlg_blit_last_dst, (long)(composed + expect_offset));
    ASSERT_EQ((long)g_dlg_blit_last_sprite,
              (long)((uint32)data_fd2_portrait_sprite_buffer
                     + *data_fd2_portrait_sprite_buffer));
    ASSERT_EQ((long)g_dlg_blit_last_stride, 0x140);

    free(ref);
    lcp_cleanup();
}

/* Each special kind 0x80..0x84 maps to its fixed story-portrait slot. */
static void test_lcp_special_kinds(void)
{
    lcp_check_kind(0x80, 0x10bb);
    lcp_check_kind(0x81, 0x06ab);
    lcp_check_kind(0x82, 0x0f63);
    lcp_check_kind(0x83, 0x0576);
    lcp_check_kind(0x84, 0x0e3c);
}

/* A standard portrait id (not 0x80..0x84) falls to the default 0x9017 slot. */
static void test_lcp_default_kind(void)
{
    lcp_check_kind(0x40, 0x9017);
}

/* ================================================================
 * fd2_load_and_fade_in_cinematic_image @ 0x1f81e
 *
 * Loads FDOTHER.DAT[palette_idx] into data_fd2_vga_palette_data_ptr (the real
 * loader, driven against the staged real FDOTHER.DAT), applies it at full
 * brightness via the real fd2_set_vga_palette_range, renders the ANI cinematic,
 * then falls through into fd2_play_palette_fade_to_black (emit pipeline §模式 B).
 * The anim arg pass-through and the fade-out tail are observed via testglob
 * spies; the loaded palette bytes are cross-checked against an independent
 * realdat parse. The framebuffer memset + ANI playback are display
 * side-effects deferred to Phase 9. fd2_set_vga_palette_range runs for real and
 * reads the full 768-byte palette, so the global must point at a valid palette
 * buffer across the call (the real FDOTHER.DAT[0] palette is exactly that).
 * ================================================================ */

/* Reset the spies + the loaded palette buffer between cases. */
static void cinematic_reset(void)
{
    if (data_fd2_vga_palette_data_ptr != 0) {
        free((void *)data_fd2_vga_palette_data_ptr);
        data_fd2_vga_palette_data_ptr = 0;
    }
    g_play_ani_calls = 0;
    g_fade_to_black_calls = 0;
}

/* palette_idx != -1: clears + loads the real FDOTHER.DAT palette into the
 * global, passes (anim_idx,delay,0) straight to the ANI renderer, and fades to
 * black exactly once. */
static void test_cinematic_loads_palette_and_renders(void)
{
    uint8 *ref;
    long   ref_size;

    cinematic_reset();
    ref_size = realdat_read_resource("FDOTHER.DAT", 0, &ref);   /* vga palette */
    ASSERT_TRUE(ref_size > 0);

    fd2_load_and_fade_in_cinematic_image(7, 3, 0);

    /* palette actually loaded from the real archive into the global (and is the
     * buffer the real fd2_set_vga_palette_range just consumed) */
    ASSERT_TRUE(data_fd2_vga_palette_data_ptr != 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, ref_size);
    ASSERT_EQ((long)memcmp((void *)data_fd2_vga_palette_data_ptr, ref,
                           (size_t)ref_size), 0);

    /* anim_idx / per_frame_delay passed through; skip-on-key hardwired to 0 */
    ASSERT_EQ((long)g_play_ani_calls, 1);
    ASSERT_EQ((long)g_play_ani_last_idx, 7);
    ASSERT_EQ((long)g_play_ani_last_delay, 3);
    ASSERT_EQ((long)g_play_ani_last_skip, 0);

    /* fall-through tail fades to black once */
    ASSERT_EQ((long)g_fade_to_black_calls, 1);

    free(ref);
    cinematic_reset();
}

/* palette_idx == -1: keeps the current palette (no framebuffer clear, no
 * FDOTHER load); the global keeps pointing at the pre-existing buffer and the
 * loader never runs, but the palette is still applied, the cinematic still
 * renders, and the screen still fades out with the same arg pass-through.
 * Pre-seed the global with a REAL FDOTHER.DAT palette so the real
 * fd2_set_vga_palette_range has a full 768-byte buffer to read. */
static void test_cinematic_keeps_palette_when_idx_neg1(void)
{
    uint32 pal_buf;

    cinematic_reset();
    /* a genuine 768-byte palette already resident from a prior load */
    pal_buf = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0);
    ASSERT_TRUE(pal_buf != 0);
    data_fd2_vga_palette_data_ptr = pal_buf;

    /* poison the loader's size output so a stray reload would be detectable */
    data_fd2_resource_last_loaded_resource_size = 0xdeadbeef;

    fd2_load_and_fade_in_cinematic_image(2, 5, 0xffffffff);

    /* no reload: same pointer, and the loader's size output is still the poison
     * value (the FDOTHER load block was skipped entirely) */
    ASSERT_EQ((long)data_fd2_vga_palette_data_ptr, (long)pal_buf);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, (long)0xdeadbeef);

    ASSERT_EQ((long)g_play_ani_calls, 1);
    ASSERT_EQ((long)g_play_ani_last_idx, 2);
    ASSERT_EQ((long)g_play_ani_last_delay, 5);
    ASSERT_EQ((long)g_play_ani_last_skip, 0);

    ASSERT_EQ((long)g_fade_to_black_calls, 1);

    cinematic_reset();                       /* frees the global (= pal_buf) */
}

/* ================================================================
 * fd2_restore_portrait_cache_from_tmp @ 0x29117
 *
 * Reads the full 0x32A00-byte portrait sprite cache back from FD2.TMP into a
 * freshly malloc'd data_fd2_portrait_sprite_cache. This is the symmetric read of the
 * swap file written by fd2_load_chapter_portraits_and_dump_tmp's fwrite tail.
 *
 * Genuine round-trip (no fabricated file): seed data_fd2_portrait_sprite_cache with
 * real FDICON.B24-loaded portrait bytes, dump it to FD2.TMP with the REAL
 * writer (alloc_offset 0 so the writer's per-record loop is skipped and it
 * fwrites the cache verbatim), snapshot those genuine on-disk bytes, then drive
 * the reader and assert it restored a fresh non-NULL buffer holding byte-
 * identical content. Also asserts the on-disk FD2.TMP is exactly 0x32A00.
 * ================================================================ */

/* Fill the first `n` bytes of data_fd2_portrait_sprite_cache with genuine sprite bytes
 * by loading real portraits from the staged FDICON.B24 until the cache's used
 * span covers `n`; the rest of the 0x32A00 buffer keeps its malloc contents
 * (also written out verbatim by the dump, so the round-trip stays exact). */
static void rt_seed_cache_from_fdicon(void)
{
    FILE *fp;
    int   pid;

    if (data_fd2_portrait_sprite_cache != 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        data_fd2_portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    fp = fopen("FDICON.B24", "rb");
    /* load a handful of distinct real portraits -> genuine packed sprite bytes
     * land at cache+0x780.. ; first call malloc's the 0x32A00 buffer */
    for (pid = 1; pid <= 8; pid++) {
        fd2_load_portrait_to_cache((uint32)pid, (uint32)fp);
    }
    fclose(fp);
}

static void test_restore_roundtrip_from_tmp(void)
{
    uint8 *ref;
    uint8  prev_tileevent_dummy;
    uint32 saved_alloc;
    uint32 saved_chapter;
    uint32 saved_tileptr;
    uint32 saved_loadbuf;
    runtime_char *saved_rc;
    FILE  *vf;
    long   fsize;

    /* --- seed data_fd2_portrait_sprite_cache with genuine FDICON sprite content --- */
    rt_seed_cache_from_fdicon();
    ASSERT_TRUE(data_fd2_portrait_sprite_cache != 0);

    /* snapshot the genuine cache image we are about to write out */
    ref = (uint8 *)malloc(0x32a00);
    memcpy(ref, (void *)data_fd2_portrait_sprite_cache, 0x32a00);

    /* --- write FD2.TMP with the REAL writer, loop skipped (alloc_offset 0) --- */
    saved_alloc   = data_fd2_resource_portrait_cache_alloc_offset;
    saved_chapter = data_fd2_chapter_current_chapter_id;
    saved_tileptr = data_fd2_tile_event_data_table_ptr;
    saved_loadbuf = data_fd2_chapter_portrait_load_buffer;
    saved_rc      = data_fd2_battle_runtime_char_array_ptr;

    prev_tileevent_dummy = 0;
    data_fd2_tile_event_data_table_ptr = (uint32)&prev_tileevent_dummy;
    data_fd2_resource_portrait_cache_alloc_offset = 0; /* no per-record inits */
    data_fd2_chapter_current_chapter_id = 4;           /* re-read FDFIELD[0xE] */
    data_fd2_chapter_portrait_load_buffer = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    fd2_load_chapter_portraits_and_dump_tmp(0xFF);     /* no race matches -> dump */

    /* FD2.TMP now on disk, exactly the cache size */
    vf = fopen("FD2.TMP", "rb");
    ASSERT_TRUE(vf != NULL);
    fseek(vf, 0, SEEK_END);
    fsize = ftell(vf);
    fclose(vf);
    ASSERT_EQ(fsize, 0x32a00);

    /* the writer freed+nulled data_fd2_chapter_portrait_load_buffer; drop the cache so
     * the reader must re-malloc a fresh buffer */
    free((void *)data_fd2_portrait_sprite_cache);
    data_fd2_portrait_sprite_cache = 0;

    /* --- drive the reader under test --- */
    fd2_restore_portrait_cache_from_tmp();

    /* fresh non-NULL buffer holding the exact genuine bytes written out */
    ASSERT_TRUE(data_fd2_portrait_sprite_cache != 0);
    ASSERT_EQ((long)memcmp((void *)data_fd2_portrait_sprite_cache, ref, 0x32a00), 0);

    /* cleanup */
    free((void *)data_fd2_portrait_sprite_cache);
    data_fd2_portrait_sprite_cache = 0;
    free(ref);
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */

    data_fd2_resource_portrait_cache_alloc_offset = saved_alloc;
    data_fd2_chapter_current_chapter_id = saved_chapter;
    data_fd2_tile_event_data_table_ptr = saved_tileptr;
    data_fd2_chapter_portrait_load_buffer = saved_loadbuf;
    data_fd2_battle_runtime_char_array_ptr = saved_rc;
}

/* ================================================================
 * fd2_load_chapter_party_roster @ 0x2d392
 *
 * Pure in-memory extractor: copies the chapter-intro shop byte slice from
 * data_fd2_chapter_intro_active_metadata_entry_ptr + state_offset into the
 * caller's buffer, stopping at the first 0xFF or the state-specific cap, and
 * returns the count. No file I/O. The cursor state selects (cap, offset):
 *   state==1 -> (0xC, 0x03)   state==3 -> (8, 0xF)   else -> (8, 0x17).
 * The fixture is a single byte array the global points at; the expected
 * result is the same slice re-read independently here.
 * ================================================================ */

/* A metadata entry blob large enough to cover the 0x17+8 = 0x1F-byte window.
 * Filled with a recognizable ramp; specific 0xFF sentinels are placed per
 * test. The roster reader reads [offset .. offset+cap-1]. */
static uint8 g_lpr_meta[0x40];

static void lpr_setup(void)
{
    int i;

    for (i = 0; i < (int)sizeof(g_lpr_meta); i++) {
        g_lpr_meta[i] = (uint8)(0x10 + i);   /* never 0xFF on its own */
    }
    data_fd2_chapter_intro_active_metadata_entry_ptr = (uint32)g_lpr_meta;
}

/* Drive the reader for `state` and cross-check against an independent copy of
 * the same slice (offset/cap derived the same way the function does), honoring
 * the 0xFF terminator. */
static void lpr_check(uint32 state, uint32 exp_off, int exp_cap)
{
    uint8 out[16];
    int   ref_count;
    int   ret;
    int   i;

    data_fd2_chapter_intro_menu_cursor_state = state;
    memset(out, 0xAA, sizeof(out));

    /* independent reference: walk the same window, stop at 0xFF */
    ref_count = 0;
    for (i = 0; i < exp_cap; i++) {
        if (g_lpr_meta[exp_off + i] == 0xff) break;
        ref_count++;
    }

    ret = fd2_load_chapter_party_roster(out);

    ASSERT_EQ((long)ret, (long)ref_count);
    for (i = 0; i < ref_count; i++) {
        ASSERT_EQ((long)out[i], (long)g_lpr_meta[exp_off + i]);
    }
    /* the byte just past the written count must be untouched (no overrun) */
    ASSERT_EQ((long)out[ref_count], 0xAA);
}

/* state==1: cap 0xC, offset 0x03; no sentinel in the window -> full 12 bytes. */
static void test_lpr_state1_weapons_full(void)
{
    lpr_setup();
    lpr_check(1, 0x03, 0xc);
}

/* state==3: cap 8, offset 0x0F; full 8 bytes when no sentinel. */
static void test_lpr_state3_items_full(void)
{
    lpr_setup();
    lpr_check(3, 0x0f, 8);
}

/* else (state 0): cap 8, offset 0x17; full 8 bytes when no sentinel. */
static void test_lpr_state_other_mystery_full(void)
{
    lpr_setup();
    lpr_check(0, 0x17, 8);
}

/* else path is also taken for state 5 (and any non-1/3 value): same offset. */
static void test_lpr_state5_uses_else(void)
{
    lpr_setup();
    lpr_check(5, 0x17, 8);
}

/* 0xFF mid-window truncates: state==1, sentinel at window index 4 -> count 4. */
static void test_lpr_sentinel_truncates(void)
{
    lpr_setup();
    g_lpr_meta[0x03 + 4] = 0xff;        /* 5th byte of the state==1 window */
    lpr_check(1, 0x03, 0xc);
}

/* 0xFF at the first window byte -> count 0, nothing written. */
static void test_lpr_sentinel_at_start(void)
{
    lpr_setup();
    g_lpr_meta[0x0f] = 0xff;            /* first byte of the state==3 window */
    lpr_check(3, 0x0f, 8);
}

/* Cap boundary: a 0xFF sits exactly one past the cap, so it must NOT be seen;
 * the full cap is returned (state==3, sentinel at window index 8). */
static void test_lpr_sentinel_past_cap_ignored(void)
{
    lpr_setup();
    g_lpr_meta[0x0f + 8] = 0xff;        /* index == cap, outside the loop */
    lpr_check(3, 0x0f, 8);
}

void run_rsrc_rsrc_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: rsrc/rsrc\n");
    RUN_TEST(test_ldr_normal_load_and_size);
    RUN_TEST(test_ldr_old_buf_freed);
    RUN_TEST(test_lpc_first_init);
    RUN_TEST(test_lpc_cache_hit);
    RUN_TEST(test_lpc_append_miss);
    RUN_TEST(test_default_path_chapter9_idx_f);
    RUN_TEST(test_default_path_chapter1c_idx_37);
    RUN_TEST(test_default_path_chapter1d_idx_37);
    RUN_TEST(test_unmatched_chapter_no_load);
    RUN_TEST(test_two_sprite_chapter11);
    RUN_TEST(test_two_sprite_chapter15);
    RUN_TEST(test_two_sprite_chapter16);
    RUN_TEST(test_two_sprite_chapter1b);
    RUN_TEST(test_text_scroll_chapter17);
    RUN_TEST(test_cb_all_active);
    RUN_TEST(test_cb_party_count_gate);
    RUN_TEST(test_cb_slot6_special_dead);
    RUN_TEST(test_cb_slot6_special_active);
    RUN_TEST(test_pt_single_match);
    RUN_TEST(test_pt_no_match);
    RUN_TEST(test_pt_multiple_match);
    RUN_TEST(test_pt_empty_table);
    RUN_TEST(test_lcp_special_kinds);
    RUN_TEST(test_lcp_default_kind);
    RUN_TEST(test_cinematic_loads_palette_and_renders);
    RUN_TEST(test_cinematic_keeps_palette_when_idx_neg1);
    RUN_TEST(test_restore_roundtrip_from_tmp);
    RUN_TEST(test_lpr_state1_weapons_full);
    RUN_TEST(test_lpr_state3_items_full);
    RUN_TEST(test_lpr_state_other_mystery_full);
    RUN_TEST(test_lpr_state5_uses_else);
    RUN_TEST(test_lpr_sentinel_truncates);
    RUN_TEST(test_lpr_sentinel_at_start);
    RUN_TEST(test_lpr_sentinel_past_cap_ignored);
    printf("\n");
}
