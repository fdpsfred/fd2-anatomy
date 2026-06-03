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
#include <stdlib.h>

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

/* fd2_load_chapter_battle_data captures (testglob.c) */
extern int    g_load_portrait_calls;
extern int    g_portraits_dump_calls;
extern uint32 g_portraits_dump_last_mode;
extern runtime_char g_test_rc_array[8];

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

/* ================================================================
 * fd2_load_chapter_battle_data fixture + tests
 *
 * The real function fopens FDICON.B24 and reads packed FDFIELD/FDSHAP
 * data through pointer globals. The testglob fakes of fd2_load_dat_resource
 * return any pre-seeded (non-NULL) pointer unchanged, so the fixture stages
 * every buffer the function reads and writes a real FDICON.B24 so fopen
 * succeeds (its content is unused — only the FILE* validity matters).
 * fd2_recalculate_combat_stats is the REAL linked routine; the zeroed roster
 * templates leave every inventory slot unequipped and every status buff off,
 * so it runs as a safe no-op stat copy.
 * ================================================================ */
static uint8 *g_cb_field;      /* chapter_portrait_load_buffer (pos table) */
static uint8 *g_cb_tileevent;  /* tile_event_data_table                    */
static uint8 *g_cb_tilemap;    /* battle_tile_map (width/height header)     */
static uint8 *g_cb_roster;     /* shared menu party roster templates       */

#define CB_ALLOC_OFFSET 1u

/* total_size = number of loop slots; alloc_offset feeds the field-pos table
 * base = alloc_offset*6 + 2; party_member_count gates active vs dead.
 * slot6_char_id sets roster[6].char_id (offset +8) for the chapter<0xd /
 * slot==6 special case. */
static void setup_cb_fixture(int chapter, int total_size,
                             int menu_party_count, int sclar_w, int scalar_h,
                             uint8 slot6_char_id)
{
    int i;
    FILE *fp;

    data_fd2_chapter_current_chapter_id = (uint32)chapter;

    /* tile_event: [0]=scene_id, [1]=cache_total_size, [2]=cache_alloc_offset */
    g_cb_tileevent = (uint8 *)malloc(16);
    memset(g_cb_tileevent, 0, 16);
    g_cb_tileevent[0] = 0x00;                    /* FDSHAP scene id */
    g_cb_tileevent[1] = (uint8)total_size;       /* cache_total_size */
    g_cb_tileevent[2] = (uint8)CB_ALLOC_OFFSET;  /* cache_alloc_offset */
    data_fd2_tile_event_data_table_ptr = (uint32)g_cb_tileevent;

    /* tile_map: 16-bit width @ +0, 16-bit height @ +2 */
    g_cb_tilemap = (uint8 *)malloc(16);
    memset(g_cb_tilemap, 0, 16);
    *(int16 *)(g_cb_tilemap + 0) = (int16)sclar_w;
    *(int16 *)(g_cb_tilemap + 2) = (int16)scalar_h;
    data_fd2_battle_tile_map_ptr = (uint32)g_cb_tilemap;

    /* field position table: base = alloc_offset*6 + 2, 6-byte stride,
     * pos_x at +0 and pos_y at +2 of each entry. Seed entry k = (0x10+k, 0x20+k). */
    g_cb_field = (uint8 *)malloc(CB_ALLOC_OFFSET * 6 + 2 + (total_size + 1) * 6);
    memset(g_cb_field, 0, CB_ALLOC_OFFSET * 6 + 2 + (total_size + 1) * 6);
    for (i = 0; i < total_size; i++) {
        uint8 *e = g_cb_field + CB_ALLOC_OFFSET * 6 + 2 + i * 6;
        e[0] = (uint8)(0x10 + i);   /* pos_x */
        e[2] = (uint8)(0x20 + i);   /* pos_y */
    }
    chapter_portrait_load_buffer = (uint32)g_cb_field;

    /* roster templates: 0x50-byte stride, zeroed (no equipped items / buffs).
     * portrait_id (+0x07) seeded so fd2_load_portrait_to_cache sees distinct ids. */
    g_cb_roster = (uint8 *)malloc((total_size + 1) * 0x50);
    memset(g_cb_roster, 0, (total_size + 1) * 0x50);
    for (i = 0; i <= total_size; i++) {
        g_cb_roster[i * 0x50 + 0x07] = (uint8)(0x40 + i);  /* portrait_id */
        g_cb_roster[i * 0x50 + 0x08] = (uint8)(0x80 + i);  /* char_id */
    }
    g_cb_roster[6 * 0x50 + 0x08] = slot6_char_id;          /* slot-6 special */
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_cb_roster;

    data_fd2_shared_menu_party_member_count = (uint32)menu_party_count;

    /* freed-if-nonzero then re-malloc'd; NULL/zero so no stale free */
    data_fd2_battle_runtime_char_array_ptr = NULL;
    portrait_sprite_cache = 0;

    fp = fopen("FDICON.B24", "wb");
    fclose(fp);

    g_load_portrait_calls = 0;
    g_portraits_dump_calls = 0;
    g_portraits_dump_last_mode = 0xFF;
}

static void teardown_cb_fixture(void)
{
    if (data_fd2_battle_runtime_char_array_ptr != NULL)
        free(data_fd2_battle_runtime_char_array_ptr);
    if (portrait_sprite_cache != 0)
        free((void *)portrait_sprite_cache);
    /* g_cb_field was already free()d + nulled by the function
     * (it frees chapter_portrait_load_buffer, whose value is g_cb_field) */
    free(g_cb_tileevent);
    free(g_cb_tilemap);
    free(g_cb_roster);

    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    chapter_portrait_load_buffer = 0;
    battle_scene_snapshot = 0;
    current_chapter_text = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    portrait_sprite_cache = 0;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_chapter_current_chapter_id = 1;

    remove("FDICON.B24");
}

/* All slots active: chapter>=0xd (no slot-6 special), party count covers all. */
static void test_cb_all_active(void)
{
    runtime_char *arr;

    setup_cb_fixture(0x10, 3, 3, 0x14, 0x0A, 0xFF);
    fd2_load_chapter_battle_data(0x10);
    arr = data_fd2_battle_runtime_char_array_ptr;

    /* derived header state */
    ASSERT_EQ((long)data_fd2_battle_map_width_tiles, 0x14);
    ASSERT_EQ((long)data_fd2_battle_map_height_tiles, 0x0A);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_total_size, 3);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_alloc_offset, CB_ALLOC_OFFSET);
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 3);

    /* every slot active: one portrait load each, dump finalised once */
    ASSERT_EQ((long)g_load_portrait_calls, 3);
    ASSERT_EQ((long)g_portraits_dump_calls, 1);
    ASSERT_EQ((long)g_portraits_dump_last_mode, 0);

    /* slot 0 active fields */
    ASSERT_EQ((long)arr[0].flags, 0);          /* not dead */
    ASSERT_EQ((long)arr[0].team, 2);           /* player */
    ASSERT_EQ((long)arr[0].pos_x, 0x10);
    ASSERT_EQ((long)arr[0].pos_y, 0x20);
    ASSERT_EQ((long)arr[0].combat_aux_block[10], 0xff);
    ASSERT_EQ((long)arr[0].sprite_state[1], 0);
    ASSERT_EQ((long)arr[0].sprite_state[2], 0);
    /* slot 2 active, field-pos table advanced 6 bytes per active slot */
    ASSERT_EQ((long)arr[2].pos_x, 0x12);
    ASSERT_EQ((long)arr[2].pos_y, 0x22);
    ASSERT_EQ((long)arr[2].team, 2);

    /* chapter_portrait_load_buffer freed + nulled */
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);

    teardown_cb_fixture();
}

/* active_count gating: total_size 4 but only 2 party members -> slots 2,3 dead. */
static void test_cb_party_count_gate(void)
{
    runtime_char *arr;

    setup_cb_fixture(0x10, 4, 2, 0x14, 0x0A, 0xFF);
    fd2_load_chapter_battle_data(0x10);
    arr = data_fd2_battle_runtime_char_array_ptr;

    ASSERT_EQ((long)g_load_portrait_calls, 2);   /* only 2 active */
    ASSERT_EQ((long)arr[0].flags, 0);
    ASSERT_EQ((long)arr[1].flags, 0);
    ASSERT_EQ((long)arr[2].flags, 1);            /* dead (gated) */
    ASSERT_EQ((long)arr[3].flags, 1);            /* dead (gated) */
    ASSERT_EQ((long)arr[2].team, 0);             /* zeroed, not set to 2 */

    teardown_cb_fixture();
}

/* slot-6 special: chapter<0xd && slot==6 && roster[6].char_id != 2 -> slot 6 dead. */
static void test_cb_slot6_special_dead(void)
{
    runtime_char *arr;

    setup_cb_fixture(5, 8, 8, 0x14, 0x0A, 0x99 /* != 2 */);
    fd2_load_chapter_battle_data(5);
    arr = data_fd2_battle_runtime_char_array_ptr;

    /* slots 0..5 + 7 active (7 loads); slot 6 forced dead */
    ASSERT_EQ((long)arr[6].flags, 1);
    ASSERT_EQ((long)arr[5].flags, 0);
    ASSERT_EQ((long)arr[7].flags, 0);
    ASSERT_EQ((long)g_load_portrait_calls, 7);

    teardown_cb_fixture();
}

/* slot-6 special inactive when roster[6].char_id == 2 -> slot 6 stays active. */
static void test_cb_slot6_special_active(void)
{
    runtime_char *arr;

    setup_cb_fixture(5, 8, 8, 0x14, 0x0A, 0x02 /* == 2 keeps slot active */);
    fd2_load_chapter_battle_data(5);
    arr = data_fd2_battle_runtime_char_array_ptr;

    ASSERT_EQ((long)arr[6].flags, 0);            /* active */
    ASSERT_EQ((long)arr[6].team, 2);
    ASSERT_EQ((long)g_load_portrait_calls, 8);   /* all 8 active */

    teardown_cb_fixture();
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
    RUN_TEST(test_cb_all_active);
    RUN_TEST(test_cb_party_count_gate);
    RUN_TEST(test_cb_slot6_special_dead);
    RUN_TEST(test_cb_slot6_special_active);
    printf("\n");
}
