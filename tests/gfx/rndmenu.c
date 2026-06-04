/*
 * unit tests for src/gfx/rndmenu.c
 *
 * fd2_render_chapter_intro_overlay @ 0x2CF71 composes the chapter-intro
 * screen (panel + title text + portrait icon) into the large game-state
 * working buffer, then commits to the VGA primary. The pure-display blits
 * are reached through recording spies; the only risk-bearing logic is the
 * portrait frame-index remap (3 -> 1) and the portrait dst/src address
 * arithmetic (per-chapter pose tables indexed by category*6 + cursor_state).
 *
 * Pipeline wiring used here:
 *   - memmove(lgsb, snapshot, 0x25680): both point at real >=0x25680 buffers.
 *   - fd2_dialog_sprite_blit_normal: recording spy (g_dlg_blit_*).
 *   - fd2_display_dialog_scene: REAL (dialog VM) against an immediate-END
 *     text program -> returns at once, no fopen / no glyph blits.
 *   - fd2_tile_blit_24x24_passthrough: recording spy (g_blitpass_*[0]).
 *   - fd2_blit_rectangle: REAL; reads from lgsb+0x8088, writes to VGA 0xA0504
 *     (harmless under DOS/4GW).
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* panel-blit recording spy (testglob.c) */
extern int    g_dlg_blit_normal_calls;
extern uint32 g_dlg_blit_last_dst;
extern uint32 g_dlg_blit_last_sprite;
extern uint32 g_dlg_blit_last_stride;
/* portrait-blit recording spy (testglob.c) */
extern int    g_blitpass_calls;
extern uint32 g_blitpass_src[64];
extern uint32 g_blitpass_dst[64];
extern uint32 g_blitpass_stride[64];
/* dialog-VM glyph spy (testglob.c); the chapter-intro metadata table is
 * already declared in globals.h and defined in testglob.c. */
extern int    g_dlg_glyph_calls;

/* working surface + snapshot source for the memmove + final blit_rectangle.
 * 0x25680 spans the memmove and (for the commit) lgsb+0x8088 + 191*0x1c8 + 0x138
 * = 120312 < 0x25680, so a single 0x25680 buffer covers every read. */
#define LGSB_SPAN 0x25680u
static uint8 g_lgsb_buf[LGSB_SPAN];
static uint8 g_snapshot_buf[LGSB_SPAN];

/* portrait sprite cache: head holds a per-frame int32 offset table; the chosen
 * frame's payload is at cache_base + offset_table[frame]. */
static uint8 g_portrait_cache[256];

/* immediate-END text program: every page word points at an END (-1) opcode so
 * the real fd2_display_dialog_scene returns without fopen / glyph output. */
static uint16 g_intro_text[0x400];

static void intro_text_all_end(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        g_intro_text[i] = 0;
    }
    *(int16 *)((uint8 *)g_intro_text + 0x780) = -1;   /* END marker */
    for (i = 0; i < 0x3c0; i++) {
        g_intro_text[i] = (uint16)0x780;              /* byte offset of END */
    }
    data_fd2_all_game_text_ptr = (uint32)g_intro_text;
}

/* Common fixture: known chapter, pose tables, portrait cache, surfaces, text. */
static void intro_setup(uint8 category, uint32 cursor_state)
{
    int chapter_id = 5;                                /* (5-1)*31 = 124 in-bounds */

    memset(g_lgsb_buf, 0, LGSB_SPAN);
    memset(g_snapshot_buf, 0xAB, LGSB_SPAN);
    memset(g_portrait_cache, 0, sizeof(g_portrait_cache));
    memset(data_fd2_chapter_intro_portrait_pose_y_row_table, 0, 18);
    memset(data_fd2_chapter_intro_portrait_pose_x_column_table, 0, 18);

    data_fd2_large_game_state_buffer_ptr = (uint32)g_lgsb_buf;
    battle_scene_snapshot                = (uint32)g_snapshot_buf;
    portrait_sprite_cache                = (uint32)g_portrait_cache;
    data_fd2_chapter_intro_menu_overlay_buf_ptr = 0x12345678;

    data_fd2_chapter_current_chapter_id  = (uint32)chapter_id;
    data_fd2_chapter_intro_metadata_table[(chapter_id - 1) * 0x1F] = category;
    data_fd2_chapter_intro_menu_cursor_state = cursor_state;

    intro_text_all_end();

    g_dlg_blit_normal_calls = 0;
    g_blitpass_calls = 0;
    g_dlg_glyph_calls = 0;
}

/* ----------------------------------------------------------------
 * Full composition with a non-special frame index (no 3->1 remap).
 * Verifies: panel blit args, title-dialog ran (immediate END), portrait
 * src/dst arithmetic, blit strides, and the working-surface memmove.
 * ---------------------------------------------------------------- */
static void test_compose_args_frame_nonremap(void)
{
    uint8  category = 2;
    uint32 cursor   = 1;
    uint32 table_off;
    uint32 expect_dst;
    uint32 expect_src;

    intro_setup(category, cursor);

    /* category*6 + cursor = 13; seed pose tables at that index */
    table_off = (uint32)category * 6 + cursor;        /* = 13 */
    data_fd2_chapter_intro_portrait_pose_x_column_table[table_off] = 0x0A; /* col */
    data_fd2_chapter_intro_portrait_pose_y_row_table[table_off]    = 0x14; /* row */

    /* frame index 2 (not 3) -> used as-is; offset table[2] = 0x40 */
    data_fd2_chapter_intro_dialog_anim_frame_idx = 2;
    *(int32 *)(g_portrait_cache + 2 * 4) = 0x40;

    fd2_render_chapter_intro_overlay();

    /* working surface restored from snapshot (memmove copied the 0xAB fill) */
    ASSERT_EQ((long)g_lgsb_buf[0], 0xAB);
    ASSERT_EQ((long)g_lgsb_buf[LGSB_SPAN - 1], 0xAB);

    /* panel blit: dst = lgsb+0x1A20C, sprite = overlay buf ptr, stride 0x1C8 */
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 1);
    ASSERT_EQ((long)g_dlg_blit_last_dst, (long)((uint32)g_lgsb_buf + 0x1a20c));
    ASSERT_EQ((long)g_dlg_blit_last_sprite, 0x12345678);
    ASSERT_EQ((long)g_dlg_blit_last_stride, 0x1c8);

    /* title dialog ran against immediate-END -> no glyphs emitted */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    /* portrait blit recorded once */
    ASSERT_EQ((long)g_blitpass_calls, 1);
    ASSERT_EQ((long)g_blitpass_stride[0], 0x1c8);

    /* dst = lgsb + col*0x1C8 + row + 0x8088 */
    expect_dst = (uint32)g_lgsb_buf + 0x0Au * 0x1c8u + 0x14u + 0x8088u;
    ASSERT_EQ((long)g_blitpass_dst[0], (long)expect_dst);

    /* src = *(int32*)(cache + frame*4) + cache = cache + 0x40 */
    expect_src = (uint32)g_portrait_cache + 0x40u;
    ASSERT_EQ((long)g_blitpass_src[0], (long)expect_src);
}

/* ----------------------------------------------------------------
 * Frame index 3 is remapped to 1: the portrait src must use offset
 * table[1], NOT table[3].
 * ---------------------------------------------------------------- */
static void test_frame_index_3_remaps_to_1(void)
{
    uint8  category = 0;
    uint32 cursor   = 0;
    uint32 expect_src;

    intro_setup(category, cursor);

    /* distinct offsets at [1] and [3] so a wrong index is detectable */
    *(int32 *)(g_portrait_cache + 1 * 4) = 0x11;
    *(int32 *)(g_portrait_cache + 3 * 4) = 0x99;
    data_fd2_chapter_intro_dialog_anim_frame_idx = 3;

    fd2_render_chapter_intro_overlay();

    ASSERT_EQ((long)g_blitpass_calls, 1);
    /* remap 3 -> 1 means offset table[1] = 0x11 is used */
    expect_src = (uint32)g_portrait_cache + 0x11u;
    ASSERT_EQ((long)g_blitpass_src[0], (long)expect_src);
}

/* ----------------------------------------------------------------
 * Frame index 1 (the remap target value) is itself used unchanged: the
 * CMP is against 3 only, so 1 stays 1. Guards against an over-broad remap.
 * ---------------------------------------------------------------- */
static void test_frame_index_1_unchanged(void)
{
    uint8  category = 0;
    uint32 cursor   = 0;
    uint32 expect_src;

    intro_setup(category, cursor);

    *(int32 *)(g_portrait_cache + 1 * 4) = 0x22;
    data_fd2_chapter_intro_dialog_anim_frame_idx = 1;

    fd2_render_chapter_intro_overlay();

    ASSERT_EQ((long)g_blitpass_calls, 1);
    expect_src = (uint32)g_portrait_cache + 0x22u;
    ASSERT_EQ((long)g_blitpass_src[0], (long)expect_src);
}

/* ----------------------------------------------------------------
 * table_off = chapter_meta_byte*6 + cursor_state drives BOTH pose-table
 * reads. Pick a category/cursor that lands on a different index and seed
 * only that index to prove the index formula.
 * ---------------------------------------------------------------- */
static void test_pose_table_index_formula(void)
{
    uint8  category = 1;
    uint32 cursor   = 3;
    uint32 table_off;
    uint32 expect_dst;

    intro_setup(category, cursor);

    table_off = (uint32)category * 6 + cursor;         /* = 9 */
    data_fd2_chapter_intro_portrait_pose_x_column_table[table_off] = 0x07;
    data_fd2_chapter_intro_portrait_pose_y_row_table[table_off]    = 0x03;

    data_fd2_chapter_intro_dialog_anim_frame_idx = 0;
    *(int32 *)(g_portrait_cache + 0 * 4) = 0;

    fd2_render_chapter_intro_overlay();

    ASSERT_EQ((long)g_blitpass_calls, 1);
    /* dst uses index 9 (= 1*6 + 3); index 13 etc. are still zero */
    expect_dst = (uint32)g_lgsb_buf + 0x07u * 0x1c8u + 0x03u + 0x8088u;
    ASSERT_EQ((long)g_blitpass_dst[0], (long)expect_dst);
}

void run_gfx_rndmenu_tests(void)
{
    SUITE_BEGIN(gfx_rndmenu);
    RUN_TEST(test_compose_args_frame_nonremap);
    RUN_TEST(test_frame_index_3_remaps_to_1);
    RUN_TEST(test_frame_index_1_unchanged);
    RUN_TEST(test_pose_table_index_formula);
    SUITE_END();
}
