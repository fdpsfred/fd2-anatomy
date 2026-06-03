/*
 * unit tests for src/rsrc/rsrc.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* capture globals from testglob.c */
extern int    g_load_dat_calls;
extern uint32 g_load_dat_last_fname;
extern uint32 g_load_dat_last_old_buf;
extern uint32 g_load_dat_last_idx;
extern uint32 g_load_dat_idx_log[8];

extern int    g_rle_blit_calls;
extern uint32 g_rle_blit_last_sprite;
extern int32  g_rle_blit_last_x;
extern int32  g_rle_blit_last_y;
extern uint32 g_rle_blit_last_buf;
extern int32  g_rle_blit_last_stride;
extern uint32 g_rle_blit_last_palette;
extern int32  g_rle_blit_y_log[4];

extern int    g_scroll_text_calls;
extern uint32 g_scroll_text_last_arg;

/* FDOTHER.DAT string address used by the function under test */
#define FDOTHER_DAT_ADDR 0x51a4d

static void reset_capture(void)
{
    g_load_dat_calls = 0;
    g_load_dat_last_fname = 0;
    g_load_dat_last_old_buf = 0;
    g_load_dat_last_idx = 0;
    memset(g_load_dat_idx_log, 0, sizeof(g_load_dat_idx_log));

    g_rle_blit_calls = 0;
    g_rle_blit_last_sprite = 0;
    g_rle_blit_last_x = 0;
    g_rle_blit_last_y = 0;
    g_rle_blit_last_buf = 0;
    g_rle_blit_last_stride = 0;
    g_rle_blit_last_palette = 0;
    memset(g_rle_blit_y_log, 0, sizeof(g_rle_blit_y_log));

    g_scroll_text_calls = 0;
    g_scroll_text_last_arg = 0;

    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
}

/* Default single-sprite path: ordinary chapter (e.g. 9) -> idx 0xF,
   one load into static_bg, no blit, animated_bg = malloc(64000). */
static void test_default_path_chapter9_idx_f(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 9;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_load_dat_calls, 1);
    ASSERT_EQ(g_load_dat_last_fname, FDOTHER_DAT_ADDR);
    ASSERT_EQ(g_load_dat_last_idx, 0xf);
    ASSERT_EQ(g_rle_blit_calls, 0);
    ASSERT_EQ(g_scroll_text_calls, 0);
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);   /* loaded sprite */
    ASSERT_NE(data_fd2_graphics_animated_bg_buffer_ptr, 0); /* malloc(64000) */
}

/* Chapter 0x1C / 0x1D default path -> idx 0x37. */
static void test_default_path_chapter1c_idx_37(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x1c;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_load_dat_calls, 1);
    ASSERT_EQ(g_load_dat_last_idx, 0x37);
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_NE(data_fd2_graphics_animated_bg_buffer_ptr, 0);
}

static void test_default_path_chapter1d_idx_37(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x1d;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_load_dat_calls, 1);
    ASSERT_EQ(g_load_dat_last_idx, 0x37);
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

    ASSERT_EQ(g_load_dat_calls, 0);
    ASSERT_EQ(g_rle_blit_calls, 0);
    ASSERT_EQ(g_scroll_text_calls, 0);
    ASSERT_EQ(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ(data_fd2_graphics_animated_bg_buffer_ptr, 0);
}

/* 2-sprite widescreen path, chapter 0x11: defaults 0x1CE x 0xE2,
   idx_base 0x10. Top blit at y=0, bottom at y=bg_height/2=0x71.
   Two loads (idx 0x10 then 0x11), two blits, animated_bg freed+nulled. */
static void test_two_sprite_chapter11(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x11;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_load_dat_calls, 2);
    ASSERT_EQ(g_load_dat_idx_log[0], 0x10);
    ASSERT_EQ(g_load_dat_idx_log[1], 0x11);
    ASSERT_EQ(g_rle_blit_calls, 2);
    ASSERT_EQ(g_rle_blit_y_log[0], 0);
    ASSERT_EQ(g_rle_blit_y_log[1], 0xe2 / 2);     /* 0x71 */
    ASSERT_EQ(g_rle_blit_last_stride, 0x1ce);
    ASSERT_EQ(g_rle_blit_last_palette, 0xffffffff);
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ(data_fd2_graphics_animated_bg_buffer_ptr, 0);
}

/* 2-sprite widescreen, chapter 0x15: bg 0x198 x 0x114, idx_base 0x23. */
static void test_two_sprite_chapter15(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x15;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_load_dat_calls, 2);
    ASSERT_EQ(g_load_dat_idx_log[0], 0x23);
    ASSERT_EQ(g_load_dat_idx_log[1], 0x24);
    ASSERT_EQ(g_rle_blit_y_log[1], 0x114 / 2);    /* 0x8a */
    ASSERT_EQ(g_rle_blit_last_stride, 0x198);
}

/* 2-sprite widescreen, chapter 0x16: bg 0x198 x 0x100, idx_base 0x28. */
static void test_two_sprite_chapter16(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x16;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_load_dat_idx_log[0], 0x28);
    ASSERT_EQ(g_load_dat_idx_log[1], 0x29);
    ASSERT_EQ(g_rle_blit_y_log[1], 0x100 / 2);    /* 0x80 */
    ASSERT_EQ(g_rle_blit_last_stride, 0x198);
}

/* 2-sprite widescreen, chapter 0x1B: width default 0x1CE, height 0xF4,
   idx_base 0x2E. */
static void test_two_sprite_chapter1b(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x1b;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_load_dat_idx_log[0], 0x2e);
    ASSERT_EQ(g_load_dat_idx_log[1], 0x2f);
    ASSERT_EQ(g_rle_blit_y_log[1], 0xf4 / 2);     /* 0x7a */
    ASSERT_EQ(g_rle_blit_last_stride, 0x1ce);
}

/* Text-scroll cinematic, chapter 0x17: idx 0x2A, stride 0x138, one blit,
   scroll armed with arg 0, animated_bg freed+nulled. */
static void test_text_scroll_chapter17(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x17;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_load_dat_calls, 1);
    ASSERT_EQ(g_load_dat_last_idx, 0x2a);
    ASSERT_EQ(g_rle_blit_calls, 1);
    ASSERT_EQ(g_rle_blit_last_x, 0);
    ASSERT_EQ(g_rle_blit_last_y, 0);
    ASSERT_EQ(g_rle_blit_last_stride, 0x138);
    ASSERT_EQ(g_rle_blit_last_palette, 0xffffffff);
    ASSERT_EQ(g_scroll_text_calls, 1);
    ASSERT_EQ(g_scroll_text_last_arg, 0);
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ(data_fd2_graphics_animated_bg_buffer_ptr, 0);
}

void run_rsrc_rsrc_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: rsrc/rsrc\n");
    RUN_TEST(test_default_path_chapter9_idx_f);
    RUN_TEST(test_default_path_chapter1c_idx_37);
    RUN_TEST(test_default_path_chapter1d_idx_37);
    RUN_TEST(test_unmatched_chapter_no_load);
    RUN_TEST(test_two_sprite_chapter11);
    RUN_TEST(test_two_sprite_chapter15);
    RUN_TEST(test_two_sprite_chapter16);
    RUN_TEST(test_two_sprite_chapter1b);
    RUN_TEST(test_text_scroll_chapter17);
    printf("\n");
}
