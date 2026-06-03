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

/* fd2_load_chapter_battle_data captures (testglob.c) */
extern runtime_char g_test_rc_array[8];

#include "rsrcfix.h"   /* write_fake_fdicon(), write_fake_dat() */

/* FDOTHER.DAT named filename string (matches Ghidra/globals symbol) */
extern char data_fd2_string_resource_filename_fdother_dat[];

/* Build an FDOTHER.DAT covering indices 0..N-1; payload[idx][0] = idx so a
 * test can confirm which index the real loader fetched (single-sprite path
 * derefs static_bg directly; multi-sprite paths read the blit first-byte log).
 * Each payload is BG_PAYLOAD_SZ bytes (blit is a stub, so content beyond the
 * marker byte is irrelevant). */
#define BG_DAT_INDICES   0x38
#define BG_PAYLOAD_SZ    16
static uint8 g_bg_payloads[BG_DAT_INDICES][BG_PAYLOAD_SZ];
static const uint8 *g_bg_payload_ptrs[BG_DAT_INDICES];
static int          g_bg_sizes[BG_DAT_INDICES];

static void write_fdother_bg_dat(void)
{
    int i;
    for (i = 0; i < BG_DAT_INDICES; i++) {
        memset(g_bg_payloads[i], 0, BG_PAYLOAD_SZ);
        g_bg_payloads[i][0] = (uint8)i;          /* index marker */
        g_bg_payload_ptrs[i] = g_bg_payloads[i];
        g_bg_sizes[i] = BG_PAYLOAD_SZ;
    }
    write_fake_dat((const char *)data_fd2_string_resource_filename_fdother_dat,
                   BG_DAT_INDICES, g_bg_sizes, g_bg_payload_ptrs);
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

    g_scroll_text_calls = 0;
    g_scroll_text_last_arg = 0;

    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;

    write_fdother_bg_dat();
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
    remove((const char *)data_fd2_string_resource_filename_fdother_dat);
}

/* Default single-sprite path: ordinary chapter (e.g. 9) -> idx 0xF,
   one load into static_bg, no blit, animated_bg = malloc(64000). */
static void test_default_path_chapter9_idx_f(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 9;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_rle_blit_calls, 0);
    ASSERT_EQ(g_scroll_text_calls, 0);
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);   /* loaded sprite */
    ASSERT_NE(data_fd2_graphics_animated_bg_buffer_ptr, 0); /* malloc(64000) */
    /* static_bg holds the loaded FDOTHER index 0xF (payload marker byte) */
    ASSERT_EQ((long)*(uint8 *)data_fd2_graphics_static_bg_buffer_ptr, 0xf);
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
    ASSERT_EQ((long)*(uint8 *)data_fd2_graphics_static_bg_buffer_ptr, 0x37);
    reset_capture_teardown();
}

static void test_default_path_chapter1d_idx_37(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x1d;

    fd2_load_chapter_background_layers();

    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ((long)*(uint8 *)data_fd2_graphics_static_bg_buffer_ptr, 0x37);
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

    /* unmatched chapter: nothing loaded/blitted, both buffers freed+nulled */
    ASSERT_EQ(g_rle_blit_calls, 0);
    ASSERT_EQ(g_scroll_text_calls, 0);
    ASSERT_EQ(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ(data_fd2_graphics_animated_bg_buffer_ptr, 0);
    reset_capture_teardown();
}

/* 2-sprite widescreen path, chapter 0x11: defaults 0x1CE x 0xE2,
   idx_base 0x10. Top blit at y=0, bottom at y=bg_height/2=0x71.
   Two loads (idx 0x10 then 0x11), two blits, animated_bg freed+nulled. */
static void test_two_sprite_chapter11(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x11;

    fd2_load_chapter_background_layers();

    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0], 0x10);
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[1], 0x11);
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

    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0], 0x23);
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[1], 0x24);
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

    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0], 0x28);
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[1], 0x29);
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

    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0], 0x2e);
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[1], 0x2f);
    ASSERT_EQ(g_rle_blit_y_log[1], 0xf4 / 2);     /* 0x7a */
    ASSERT_EQ(g_rle_blit_last_stride, 0x1ce);
    reset_capture_teardown();
}

/* Text-scroll cinematic, chapter 0x17: idx 0x2A, stride 0x138, one blit,
   scroll armed with arg 0, animated_bg freed+nulled. */
static void test_text_scroll_chapter17(void)
{
    reset_capture();
    data_fd2_chapter_current_chapter_id = 0x17;

    fd2_load_chapter_background_layers();

    ASSERT_EQ(g_rle_blit_calls, 1);
    ASSERT_EQ((long)g_rle_blit_sprite_first_byte_log[0], 0x2a);
    ASSERT_EQ(g_rle_blit_last_x, 0);
    ASSERT_EQ(g_rle_blit_last_y, 0);
    ASSERT_EQ(g_rle_blit_last_stride, 0x138);
    ASSERT_EQ(g_rle_blit_last_palette, 0xffffffff);
    ASSERT_EQ(g_scroll_text_calls, 1);
    ASSERT_EQ(g_scroll_text_last_arg, 0);
    ASSERT_NE(data_fd2_graphics_static_bg_buffer_ptr, 0);
    ASSERT_EQ(data_fd2_graphics_animated_bg_buffer_ptr, 0);
    reset_capture_teardown();
}

/* ================================================================
 * fd2_load_chapter_battle_data fixture + tests
 *
 * The real fd2_load_dat_resource (now linked) fopen+freads the named DAT
 * files, so the fixture writes real on-disk archives whose indices deliver
 * the exact bytes the function consumes:
 *   FDFIELD.DAT[ch*3]   -> tile_map  (16-bit width@+0, height@+2)
 *   FDFIELD.DAT[ch*3+1] -> tile_event ([0]=scene_id,[1]=total_size,
 *                                       [2]=alloc_offset, race bytes @+0x98)
 *   FDFIELD.DAT[ch*3+2] -> field-pos table (pos_x@+0, pos_y@+2, stride 6)
 *   FDTXT.DAT[ch+1]     -> current_chapter_text (loaded, not read here)
 *   FDSHAP.DAT[scene*2], [scene*2+1] -> battle_scene_snapshot / tile attr
 *   FDOTHER.DAT[...]    -> background_layers default-path single sprite
 * The shared menu roster stays a direct in-process global (not DAT-loaded).
 * fd2_recalculate_combat_stats / fd2_load_portrait_to_cache are the REAL
 * linked routines; FDICON.B24 is staged for the portrait loader.
 * ================================================================ */
static uint8 *g_cb_field;      /* FDFIELD[ch*3+2] payload (pos table)      */
static uint8 *g_cb_tileevent;  /* FDFIELD[ch*3+1] payload (tile_event)     */
static uint8 *g_cb_tilemap;    /* FDFIELD[ch*3]   payload (width/height)    */
static uint8 *g_cb_roster;     /* shared menu party roster templates       */

extern char data_fd2_string_resource_filename_fdtxt_dat[];
extern char data_fd2_string_resource_filename_fdfield_dat_51a59[];
extern char data_fd2_string_resource_filename_fdshap_dat_51a65[];

#define CB_ALLOC_OFFSET 1u

/* Build the three FDFIELD resources (tile_map / tile_event / field-pos) for
 * the given chapter so indices ch*3, ch*3+1, ch*3+2 all resolve. The DAT
 * also needs to cover index 0..ch*3+2; lower indices get tiny stub payloads. */
static int          g_cb_fld_sizes[0x60 * 3 + 4];
static const uint8 *g_cb_fld_ptrs[0x60 * 3 + 4];
static uint8        g_cb_fld_stub[4];

static void write_cb_fdfield(int chapter)
{
    int    n;
    int    base;
    int    i;

    base = chapter * 3;
    n = base + 3;                 /* indices 0..base+2 inclusive */
    memset(g_cb_fld_stub, 0, sizeof(g_cb_fld_stub));
    for (i = 0; i < n; i++) {
        g_cb_fld_sizes[i] = (int)sizeof(g_cb_fld_stub);
        g_cb_fld_ptrs[i]  = g_cb_fld_stub;
    }
    /* tile_map @ base, tile_event @ base+1, field-pos @ base+2 */
    g_cb_fld_sizes[base]     = 16;
    g_cb_fld_ptrs[base]      = g_cb_tilemap;
    g_cb_fld_sizes[base + 1] = (int)(0x98 + CB_ALLOC_OFFSET * 0x1a + 0x20);
    g_cb_fld_ptrs[base + 1]  = g_cb_tileevent;
    g_cb_fld_sizes[base + 2] = (int)(CB_ALLOC_OFFSET * 6 + 2 +
                                     ((g_cb_tileevent[1]) + 1) * 6);
    g_cb_fld_ptrs[base + 2]  = g_cb_field;
    write_fake_dat((const char *)data_fd2_string_resource_filename_fdfield_dat_51a59,
                   n, g_cb_fld_sizes, g_cb_fld_ptrs);
}

/* total_size = number of loop slots; alloc_offset feeds the field-pos table
 * base = alloc_offset*6 + 2; party_member_count gates active vs dead.
 * slot6_char_id sets roster[6].char_id (offset +8) for the chapter<0xd /
 * slot==6 special case. */
static void setup_cb_fixture(int chapter, int total_size,
                             int menu_party_count, int sclar_w, int scalar_h,
                             uint8 slot6_char_id)
{
    int i;
    int scene_stub_sizes[2];
    const uint8 *scene_stub_ptrs[2];
    int txt_n;
    int *txt_sizes;
    const uint8 **txt_ptrs;

    data_fd2_chapter_current_chapter_id = (uint32)chapter;

    /* tile_event payload: [0]=scene_id, [1]=cache_total_size,
     * [2]=cache_alloc_offset. Race bytes (+0x98 stride 0x1A) left at 0xEE
     * sentinel so the tail dump_tmp(target_race_id 0) matches none. */
    g_cb_tileevent = (uint8 *)malloc(0x98 + CB_ALLOC_OFFSET * 0x1a + 0x20);
    memset(g_cb_tileevent, 0xEE, 0x98 + CB_ALLOC_OFFSET * 0x1a + 0x20);
    g_cb_tileevent[0] = 0x00;                    /* FDSHAP scene id */
    g_cb_tileevent[1] = (uint8)total_size;       /* cache_total_size */
    g_cb_tileevent[2] = (uint8)CB_ALLOC_OFFSET;  /* cache_alloc_offset */

    /* tile_map payload: 16-bit width @ +0, 16-bit height @ +2 */
    g_cb_tilemap = (uint8 *)malloc(16);
    memset(g_cb_tilemap, 0, 16);
    *(int16 *)(g_cb_tilemap + 0) = (int16)sclar_w;
    *(int16 *)(g_cb_tilemap + 2) = (int16)scalar_h;

    /* field-pos payload: base = alloc_offset*6 + 2, 6-byte stride,
     * pos_x @ +0 / pos_y @ +2. Seed entry k = (0x10+k, 0x20+k). */
    g_cb_field = (uint8 *)malloc(CB_ALLOC_OFFSET * 6 + 2 + (total_size + 1) * 6);
    memset(g_cb_field, 0, CB_ALLOC_OFFSET * 6 + 2 + (total_size + 1) * 6);
    for (i = 0; i < total_size; i++) {
        uint8 *e = g_cb_field + CB_ALLOC_OFFSET * 6 + 2 + i * 6;
        e[0] = (uint8)(0x10 + i);   /* pos_x */
        e[2] = (uint8)(0x20 + i);   /* pos_y */
    }

    /* write the three FDFIELD resources as one archive */
    write_cb_fdfield(chapter);

    /* FDTXT.DAT: index ch+1 loaded (not read), tiny payloads for 0..ch+1 */
    txt_n = chapter + 2;
    txt_sizes = (int *)malloc((size_t)txt_n * sizeof(int));
    txt_ptrs  = (const uint8 **)malloc((size_t)txt_n * sizeof(uint8 *));
    for (i = 0; i < txt_n; i++) { txt_sizes[i] = 4; txt_ptrs[i] = 0; }
    write_fake_dat((const char *)data_fd2_string_resource_filename_fdtxt_dat,
                   txt_n, txt_sizes, txt_ptrs);
    free(txt_sizes);
    free(txt_ptrs);

    /* FDSHAP.DAT: scene_id 0 -> indices 0 (snapshot) and 1 (attr flags) */
    scene_stub_sizes[0] = 4; scene_stub_sizes[1] = 4;
    scene_stub_ptrs[0] = 0;  scene_stub_ptrs[1] = 0;
    write_fake_dat((const char *)data_fd2_string_resource_filename_fdshap_dat_51a65,
                   2, scene_stub_sizes, scene_stub_ptrs);

    /* FDOTHER.DAT: background_layers default path loads one sprite (idx<=0x10
     * for chapters not in the special lists); reuse the full bg archive. */
    write_fdother_bg_dat();

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

    /* freed-if-nonzero then re-malloc'd; NULL/zero so no stale free. The
     * loader-returned buffers for these pointer globals start NULL so the
     * loader's free(old_buf) is a no-op on the first load. */
    data_fd2_battle_runtime_char_array_ptr = NULL;
    portrait_sprite_cache = 0;
    current_chapter_text = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    battle_scene_snapshot = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;

    /* the real fd2_load_portrait_to_cache (now linked) parses FDICON.B24 */
    write_fake_fdicon();
}

static void teardown_cb_fixture(void)
{
    if (data_fd2_battle_runtime_char_array_ptr != NULL)
        free(data_fd2_battle_runtime_char_array_ptr);
    if (portrait_sprite_cache != 0)
        free((void *)portrait_sprite_cache);
    /* loader-returned buffers the function leaves live (it does NOT free
     * these): current_chapter_text, tile_event, tile_map, scene snapshot,
     * tile-attr flags, and the background buffers. */
    if (current_chapter_text != 0)
        free((void *)current_chapter_text);
    if (data_fd2_tile_event_data_table_ptr != 0)
        free((void *)data_fd2_tile_event_data_table_ptr);
    if (data_fd2_battle_tile_map_ptr != 0)
        free((void *)data_fd2_battle_tile_map_ptr);
    if (battle_scene_snapshot != 0)
        free((void *)battle_scene_snapshot);
    if (data_fd2_tile_attribute_flags_buffer_ptr != 0)
        free((void *)data_fd2_tile_attribute_flags_buffer_ptr);
    if (data_fd2_graphics_static_bg_buffer_ptr != 0)
        free((void *)data_fd2_graphics_static_bg_buffer_ptr);
    if (data_fd2_graphics_animated_bg_buffer_ptr != 0)
        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
    /* chapter_portrait_load_buffer was freed+nulled by the function */

    /* the payload source buffers (separate from the loaded copies) */
    free(g_cb_tileevent);
    free(g_cb_tilemap);
    free(g_cb_field);
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
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
    portrait_sprite_cache = 0;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_chapter_current_chapter_id = 1;

    remove((const char *)data_fd2_string_resource_filename_fdtxt_dat);
    remove((const char *)data_fd2_string_resource_filename_fdfield_dat_51a59);
    remove((const char *)data_fd2_string_resource_filename_fdshap_dat_51a65);
    remove((const char *)data_fd2_string_resource_filename_fdother_dat);
    remove("FDICON.B24");
    remove("FD2.TMP");
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

    /* every slot active: one distinct portrait cached each. The tail
     * dump_tmp(0) runs for real; its tile-event race bytes are the 0xEE
     * sentinel so it matches no entry (target_race_id 0) and adds no extra
     * init_rtchar / portrait. Each active slot's portrait_id is distinct
     * (0x40+i) so the real loader's cache_count ends == active slot count. */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 3);

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

    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 2); /* 2 active */
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
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 7);

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
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 8); /* 8 active */

    teardown_cb_fixture();
}

/* ================================================================
 * fd2_load_chapter_portraits_and_dump_tmp @ 0x10b4e
 * ================================================================ */

static uint8 *g_pt_tileevent;   /* tile-event table for the race scan */

/* Build a tile-event table of `count` records (stride 0x1A); record k has its
 * race byte (+0x98) set to race_of[k]. Sets alloc_offset = count. Also primes
 * portrait_sprite_cache so the tail fwrite has a valid buffer, and creates an
 * empty FDICON.B24 so the rb fopen succeeds. */
static void setup_pt_fixture(int count, const uint8 *race_of)
{
    int i;
    FILE *fp;
    size_t sz;

    sz = (size_t)0x98 + (size_t)count * 0x1a + 0x20;
    g_pt_tileevent = (uint8 *)malloc(sz);
    memset(g_pt_tileevent, 0, sz);
    for (i = 0; i < count; i++) {
        g_pt_tileevent[i * 0x1a + 0x98] = race_of[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_pt_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = (uint32)count;

    /* fd2_load_chapter_portraits_and_dump_tmp re-loads FDFIELD[ch*3+2] (ch=4
     * -> idx 0xE) into chapter_portrait_load_buffer via the REAL loader. The
     * real fd2_init_runtime_char_for_battle then reads the spawn field buffer
     * at that buffer + idx*6 and the per-char record at tile_event +
     * idx*0x1A + 0x83 (default zeros -> player class 0). Build an FDFIELD.DAT
     * whose index 0xE delivers a zeroed field buffer big enough for `count`
     * 6-byte spawn entries. Run the slot build in phase 1 so spawn = desired
     * position (no tile-map search). */
    {
        int    fld_n;
        int   *fld_sizes;
        const uint8 **fld_ptrs;
        int    j;
        int    field_idx = 4 * 3 + 2;     /* 0xE */
        fld_n = field_idx + 1;
        fld_sizes = (int *)malloc((size_t)fld_n * sizeof(int));
        fld_ptrs  = (const uint8 **)malloc((size_t)fld_n * sizeof(uint8 *));
        for (j = 0; j < fld_n; j++) { fld_sizes[j] = 4; fld_ptrs[j] = 0; }
        fld_sizes[field_idx] = count * 6 + 16;   /* zeroed (ptr NULL) */
        write_fake_dat(
            (const char *)data_fd2_string_resource_filename_fdfield_dat_51a59,
            fld_n, fld_sizes, fld_ptrs);
        free(fld_sizes);
        free(fld_ptrs);
    }
    chapter_portrait_load_buffer = 0;          /* loaded fresh by the function */
    data_fd2_chapter_init_phase_flag = 1;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;   /* re-read idx = 4*3+2 = 0xE */

    /* the real fd2_load_portrait_to_cache (now linked, reached via the real
     * fd2_init_runtime_char_for_battle for matching races) allocates the cache
     * on its first call (cache_count==0) and parses FDICON.B24. Reset the cache
     * state so it first-inits cleanly; teardown frees the buffer it mallocs. */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    write_fake_fdicon();
    (void)fp;
}

static void teardown_pt_fixture(void)
{
    free(g_pt_tileevent);
    g_pt_tileevent = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    /* chapter_portrait_load_buffer was freed+nulled by the function under
     * test; leave it at 0. */
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    remove((const char *)data_fd2_string_resource_filename_fdfield_dat_51a59);
    remove("FDICON.B24");
    remove("FD2.TMP");
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

/* Single matching record: init_rtchar called once for the matching index,
 * with the fopen handle as fp. FDFIELD re-read happens (idx = chapter*3+2).
 * chapter_portrait_load_buffer freed+nulled; FD2.TMP written (0x32A00). */
static void test_pt_single_match(void)
{
    static const uint8 races[2] = { 0x07, 0x09 };

    setup_pt_fixture(2, races);
    fd2_load_chapter_portraits_and_dump_tmp(0x07);

    /* one matching record -> real init runs once (party count 0 -> 1). The
     * FDFIELD[4*3+2 = 0xE] re-read happened (the function loaded a fresh field
     * buffer and freed+nulled it on exit). */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);

    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);
    ASSERT_EQ(fd2_tmp_size(), 0x32A00);

    teardown_pt_fixture();
}

/* No record matches the target race: no init_rtchar, but the swap file is
 * still rewritten. */
static void test_pt_no_match(void)
{
    static const uint8 races[3] = { 0x01, 0x02, 0x03 };

    setup_pt_fixture(3, races);
    fd2_load_chapter_portraits_and_dump_tmp(0x7F);

    /* no matching record -> real init never runs (party count stays 0) */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);
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

/* alloc_offset == 0: loop body never runs, no re-read scan match, swap file
 * still produced. */
static void test_pt_empty_table(void)
{
    setup_pt_fixture(0, (const uint8 *)0);
    fd2_load_chapter_portraits_and_dump_tmp(0x00);

    /* empty table -> loop body never runs, real init never called */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    ASSERT_EQ(fd2_tmp_size(), 0x32A00);

    teardown_pt_fixture();
}

/* ================================================================
 * fd2_load_portrait_to_cache @ 0x11019  (direct tests)
 *
 * write_fake_fdicon() lays each header entry e at offset
 * FDICON_DATA_BASE + e*4 (e = portrait*12 + frame), so every portrait's
 * data_size = offsets[12]-offsets[0] = 12*4 = 48 bytes, and the first-init
 * frame table resolves to ((int*)cache)[i] = i*4 + 0x780. These exact
 * numbers let the tests verify the offset arithmetic, the three control-flow
 * paths (first-init / cache-hit / append-miss), and the running buffer/count
 * bookkeeping. ================================================================ */
static FILE *g_lpc_fp;

static void lpc_setup(void)
{
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
    write_fake_fdicon();
    g_lpc_fp = fopen("FDICON.B24", "rb");
}

static void lpc_teardown(void)
{
    if (g_lpc_fp != NULL) {
        fclose(g_lpc_fp);
        g_lpc_fp = NULL;
    }
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
    remove("FDICON.B24");
}

/* First-time init: count 0 -> allocate cache, build frame table, return 0. */
static void test_lpc_first_init(void)
{
    int   idx;
    int32 *tbl;
    int    i;

    lpc_setup();
    idx = fd2_load_portrait_to_cache(2, (uint32)g_lpc_fp);

    ASSERT_EQ((long)idx, 0);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 1);
    ASSERT_EQ((long)*(uint32 *)data_fd2_resource_portrait_cache_id_list_base, 2);
    ASSERT_TRUE(portrait_sprite_cache != 0);
    /* data_size = 48, buffer_used = 48 + 0x780 */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_buffer_used, 48 + 0x780);
    /* frame table: ((int*)cache)[i] = i*4 + 0x780 */
    tbl = (int32 *)portrait_sprite_cache;
    for (i = 0; i < 12; i++) {
        ASSERT_EQ((long)tbl[i], i * 4 + 0x780);
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
    int32 *tbl;
    int    i;

    lpc_setup();
    idx0 = fd2_load_portrait_to_cache(2, (uint32)g_lpc_fp);  /* slot 0 */
    idx1 = fd2_load_portrait_to_cache(5, (uint32)g_lpc_fp);  /* slot 1 */

    ASSERT_EQ((long)idx0, 0);
    ASSERT_EQ((long)idx1, 1);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 2);
    /* id_list[1] == 5 */
    ASSERT_EQ((long)*(uint32 *)(data_fd2_resource_portrait_cache_id_list_base
                                + 1 * 4), 5);
    /* buffer_used after two 48-byte portraits = 2*48 + 0x780 */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_buffer_used,
              2 * 48 + 0x780);
    /* slot 1 frame table at ((int*)cache)[12..23] = (48 + 0x780) + i*4 */
    tbl = (int32 *)portrait_sprite_cache;
    for (i = 0; i < 12; i++) {
        ASSERT_EQ((long)tbl[12 + i], (48 + 0x780) + i * 4);
    }
    lpc_teardown();
}

/* ================================================================
 * fd2_load_dat_resource @ 0x111ba  (direct tests)
 *
 * Builds a known DAT via write_fake_dat() (each index k has a distinct
 * payload) and verifies: the returned buffer holds the index's payload,
 * last_loaded_resource_size = end-start for that index, distinct indices
 * resolve to distinct payloads, and a non-NULL old_buf is freed (a fresh
 * buffer is returned, not the old pointer).
 * ================================================================ */
#define LDR_DAT_NAME "TLOADDAT.DAT"

static void test_ldr_normal_load_and_size(void)
{
    /* index 0 -> 5 bytes {0xA0..0xA4}, index 1 -> 3 bytes {0xB0..0xB2},
     * index 2 -> 7 bytes {0xC0..0xC6} */
    static const uint8 p0[5] = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4 };
    static const uint8 p1[3] = { 0xB0, 0xB1, 0xB2 };
    static const uint8 p2[7] = { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6 };
    int sizes[3];
    const uint8 *ptrs[3];
    uint8 *buf;
    int i;

    sizes[0] = 5; sizes[1] = 3; sizes[2] = 7;
    ptrs[0] = p0; ptrs[1] = p1; ptrs[2] = p2;
    write_fake_dat(LDR_DAT_NAME, 3, sizes, ptrs);

    /* index 1: size = end-start = 3; content == p1 */
    buf = (uint8 *)fd2_load_dat_resource((uint32)LDR_DAT_NAME, 0, 1);
    ASSERT_TRUE(buf != 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, 3);
    for (i = 0; i < 3; i++) {
        ASSERT_EQ((long)buf[i], (long)p1[i]);
    }
    free(buf);

    /* index 2: size 7, content == p2 */
    buf = (uint8 *)fd2_load_dat_resource((uint32)LDR_DAT_NAME, 0, 2);
    ASSERT_TRUE(buf != 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, 7);
    for (i = 0; i < 7; i++) {
        ASSERT_EQ((long)buf[i], (long)p2[i]);
    }
    free(buf);

    remove(LDR_DAT_NAME);
}

/* old_buf != 0 is freed; a fresh buffer (not the old pointer) is returned. */
static void test_ldr_old_buf_freed(void)
{
    static const uint8 p0[4] = { 0x11, 0x22, 0x33, 0x44 };
    int sizes[1];
    const uint8 *ptrs[1];
    uint32 old_buf;
    uint8 *buf;
    int i;

    sizes[0] = 4;
    ptrs[0] = p0;
    write_fake_dat(LDR_DAT_NAME, 1, sizes, ptrs);

    old_buf = (uint32)malloc(4);    /* loader will free() this */
    buf = (uint8 *)fd2_load_dat_resource((uint32)LDR_DAT_NAME, old_buf, 0);

    ASSERT_TRUE(buf != 0);
    ASSERT_EQ((long)data_fd2_resource_last_loaded_resource_size, 4);
    for (i = 0; i < 4; i++) {
        ASSERT_EQ((long)buf[i], (long)p0[i]);
    }
    free(buf);
    remove(LDR_DAT_NAME);
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
    printf("\n");
}
