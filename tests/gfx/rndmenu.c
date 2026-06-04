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

/* ================================================================
 * fd2_render_chapter_intro_dialog_panels @ 0x2D9FE
 *
 * Per-tick renderer for the chapter-intro dialog panels with three
 * layout modes plus a shared sub-frame cycler. All blits are pure
 * display side-effects reached through recording spies:
 *   mode 0     -> fd2_blit_sprite_with_stride_setup (g_blitsetup_* log)
 *   mode 1/2/3 -> fd2_dialog_sprite_blit_normal     (g_dlg_blit_*  log)
 *   mode 2     -> fd2_tile_blit_24x24_with_dialog_bg_fill (g_blitpass_*)
 *   mode 3     -> fd2_render_party_roster_grid      (g_roster_grid_*)
 * The risk-bearing logic is the sprite-index/address arithmetic, the
 * scroll/count branch selection, the icon-count cap, and the
 * anim-phase remap (0,1,2,3 -> 0,1,2,1).
 * ================================================================ */

/* fd2_blit_sprite_with_stride_setup spy (testglob.c) */
extern uint32 g_blitsetup_dst, g_blitsetup_sprite, g_blitsetup_stride;
extern int    g_blitsetup_calls;
/* fd2_dialog_sprite_blit_normal per-call log (testglob.c) */
extern uint32 g_dlg_blit_dst_log[16];
extern uint32 g_dlg_blit_sprite_log[16];
/* fd2_tile_blit_24x24_with_dialog_bg_fill spy (testglob.c, shared g_blitpass_*) */
extern int    g_blitbgfill_calls;
/* fd2_render_party_roster_grid spy (testglob.c) */
extern int    g_roster_grid_calls;
extern uint32 g_roster_grid_last_highlight;
extern uint32 g_roster_grid_last_surface;

/* sprite atlas: int32 offset table. Slot at +6 + i*4 gives an animated
 * sprite's payload offset; slot at +0x4A is the static "no scroll" sprite. */
static int32 g_panel_atlas[64];
/* portrait-id source table for the mode-2 roster row */
static uint8 g_candidate_arr[64];

static void panels_reset_spies(void)
{
    g_blitsetup_calls = 0;
    g_dlg_blit_normal_calls = 0;
    g_blitpass_calls = 0;
    g_blitbgfill_calls = 0;
    g_roster_grid_calls = 0;
}

/* common atlas/global fixture */
static void panels_setup(void)
{
    int i;

    for (i = 0; i < 64; i++) {
        g_panel_atlas[i] = 0;
        g_candidate_arr[i] = 0;
    }
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_panel_atlas;
    data_fd2_ui_menu_candidate_array_ptr = g_candidate_arr;
    portrait_sprite_cache = (uint32)g_portrait_cache;
    memset(g_portrait_cache, 0, sizeof(g_portrait_cache));

    data_fd2_chapter_intro_dialog_anim_frame_idx = 0;
    data_fd2_chapter_intro_dialog_subframe_anim_counter = 0;
    data_fd2_ui_menu_cursor_idx = 0;
    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_visible_item_count = 0;

    panels_reset_spies();
}

/* The function reads sprite offsets as *(int32 *)(atlas + 6 + idx*4): an
 * UNALIGNED int32 at byte offset 6 + idx*4. The static "no scroll" slot is
 * *(int32 *)(atlas + 0x4A). Seed both via byte-accurate pointer arithmetic so
 * the +6 misalignment matches the emitted code exactly. */
#define ATLAS_ANIM_SLOT(idx)   (*(int32 *)((uint8 *)g_panel_atlas + 6 + (idx) * 4))
#define ATLAS_STATIC_SLOT      (*(int32 *)((uint8 *)g_panel_atlas + 0x4A))

/* ----------------------------------------------------------------
 * Sub-frame cycler: on an ODD frame_idx the subframe counter advances.
 * Run with an out-of-range mode (99) so only the cycler executes.
 * ---------------------------------------------------------------- */
static void test_subframe_advances_on_odd_frame(void)
{
    panels_setup();
    data_fd2_chapter_intro_dialog_anim_frame_idx = 3;   /* odd */
    data_fd2_chapter_intro_dialog_subframe_anim_counter = 1;

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 99);

    ASSERT_EQ((long)data_fd2_chapter_intro_dialog_subframe_anim_counter, 2);
    /* no mode matched -> no blits */
    ASSERT_EQ((long)g_blitsetup_calls, 0);
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 0);
    ASSERT_EQ((long)g_blitpass_calls, 0);
}

/* ----------------------------------------------------------------
 * Even frame_idx leaves the subframe counter untouched.
 * ---------------------------------------------------------------- */
static void test_subframe_unchanged_on_even_frame(void)
{
    panels_setup();
    data_fd2_chapter_intro_dialog_anim_frame_idx = 2;   /* even */
    data_fd2_chapter_intro_dialog_subframe_anim_counter = 1;

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 99);

    ASSERT_EQ((long)data_fd2_chapter_intro_dialog_subframe_anim_counter, 1);
}

/* ----------------------------------------------------------------
 * The wrap test (counter == 4 -> 0) is unconditional: it fires even on
 * an even frame where the counter was NOT just incremented.
 * ---------------------------------------------------------------- */
static void test_subframe_wraps_at_4_unconditionally(void)
{
    panels_setup();
    data_fd2_chapter_intro_dialog_anim_frame_idx = 2;   /* even -> no increment */
    data_fd2_chapter_intro_dialog_subframe_anim_counter = 4;

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 99);

    ASSERT_EQ((long)data_fd2_chapter_intro_dialog_subframe_anim_counter, 0);
}

/* odd frame increments 3 -> 4 then wraps to 0 in the same call */
static void test_subframe_increment_then_wrap(void)
{
    panels_setup();
    data_fd2_chapter_intro_dialog_anim_frame_idx = 1;   /* odd */
    data_fd2_chapter_intro_dialog_subframe_anim_counter = 3;

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 99);

    ASSERT_EQ((long)data_fd2_chapter_intro_dialog_subframe_anim_counter, 0);
}

/* ----------------------------------------------------------------
 * mode 0: single-corner cursor indicator. Verifies
 *   sprite slot idx = frame_idx/2 + cursor*2 + 3
 *   dst            = corner_anchors[cursor] + 0xAD430
 *   sprite addr    = atlas + atlas[+6 + idx*4]
 * ---------------------------------------------------------------- */
static void test_mode0_corner_sprite_arithmetic(void)
{
    uint32 corner[4];
    int    slot_idx;

    panels_setup();
    corner[0] = 0x1000;
    corner[1] = 0x2000;
    corner[2] = 0x3000;
    corner[3] = 0x4000;

    data_fd2_chapter_intro_dialog_anim_frame_idx = 4;   /* /2 = 2 */
    data_fd2_ui_menu_cursor_idx = 1;                    /* cursor*2 = 2 */
    /* idx = 2 + 2 + 3 = 7 */
    slot_idx = 7;
    ATLAS_ANIM_SLOT(slot_idx) = 0x55;

    fd2_render_chapter_intro_dialog_panels((uint32)corner, 0);

    ASSERT_EQ((long)g_blitsetup_calls, 1);
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 0);
    /* dst = corner[1] + 0xAD430 */
    ASSERT_EQ((long)g_blitsetup_dst, (long)(0x2000u + 0xAD430u));
    /* sprite = atlas + slot value 0x55 */
    ASSERT_EQ((long)g_blitsetup_sprite, (long)((uint32)g_panel_atlas + 0x55u));
    ASSERT_EQ((long)g_blitsetup_stride, 0x140);
}

/* ----------------------------------------------------------------
 * mode 1: two panels, scroll_offset == 0 -> left uses the static slot
 * (+0x4A); right uses animated slot frame_idx/2 + 0xD when more items
 * remain below (scroll+6 < count). Mode 1 does NOT call the roster grid.
 * ---------------------------------------------------------------- */
static void test_mode1_panels_scroll_zero(void)
{
    panels_setup();
    data_fd2_chapter_intro_dialog_anim_frame_idx = 4;   /* /2 = 2 */
    data_fd2_ui_menu_scroll_offset = 0;                 /* left -> static */
    data_fd2_ui_menu_visible_item_count = 10;           /* 0+6 < 10 -> right animated */

    ATLAS_STATIC_SLOT = 0x111;                          /* left static sprite */
    ATLAS_ANIM_SLOT(2 + 0xD) = 0x222;                   /* right: idx = 2+0xD = 0xF */

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 1);

    ASSERT_EQ((long)g_roster_grid_calls, 0);
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 2);
    /* left panel: dst 0xA972A, sprite = atlas + static slot */
    ASSERT_EQ((long)g_dlg_blit_dst_log[0], 0xA972A);
    ASSERT_EQ((long)g_dlg_blit_sprite_log[0],
              (long)((uint32)g_panel_atlas + 0x111u));
    /* right panel: dst 0xAE36A, sprite = atlas + animated slot */
    ASSERT_EQ((long)g_dlg_blit_dst_log[1], 0xAE36A);
    ASSERT_EQ((long)g_dlg_blit_sprite_log[1],
              (long)((uint32)g_panel_atlas + 0x222u));
}

/* ----------------------------------------------------------------
 * mode 1 with scroll_offset != 0 (left becomes animated slot
 * frame_idx/2 + 0xB) and no items below (scroll+6 >= count, right
 * falls back to the static slot).
 * ---------------------------------------------------------------- */
static void test_mode1_panels_scrolled_and_tail(void)
{
    panels_setup();
    data_fd2_chapter_intro_dialog_anim_frame_idx = 6;   /* /2 = 3 */
    data_fd2_ui_menu_scroll_offset = 5;                 /* left -> animated */
    data_fd2_ui_menu_visible_item_count = 8;            /* 5+6=11 >= 8 -> right static */

    ATLAS_ANIM_SLOT(3 + 0xB) = 0x333;                   /* left: idx = 3+0xB = 0xE */
    ATLAS_STATIC_SLOT = 0x444;                          /* right static */

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 1);

    ASSERT_EQ((long)g_dlg_blit_normal_calls, 2);
    ASSERT_EQ((long)g_dlg_blit_sprite_log[0],
              (long)((uint32)g_panel_atlas + 0x333u));   /* left animated */
    ASSERT_EQ((long)g_dlg_blit_sprite_log[1],
              (long)((uint32)g_panel_atlas + 0x444u));   /* right static */
}

/* ----------------------------------------------------------------
 * mode 3: identical panels to mode 1, but first overlays the party
 * roster grid with (cursor_idx, 0xA0000).
 * ---------------------------------------------------------------- */
static void test_mode3_calls_roster_grid_then_panels(void)
{
    panels_setup();
    data_fd2_ui_menu_cursor_idx = 4;
    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_visible_item_count = 0;            /* 0+6 >= 0 -> right static */

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 3);

    ASSERT_EQ((long)g_roster_grid_calls, 1);
    ASSERT_EQ((long)g_roster_grid_last_highlight, 4);
    ASSERT_EQ((long)g_roster_grid_last_surface, 0xA0000);
    /* both panels still drawn */
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 2);
}

/* ----------------------------------------------------------------
 * mode 2 right panel uses the scroll+3 cap (NOT +6). With count=7 and
 * scroll=0: 0+3 < 7 -> right animated; the +6 variant (0+6 < 7 also
 * true) would pick the same branch, so choose count so +3 and +6 differ:
 * count=5, scroll=0 -> 0+3<5 (animated) but 0+6<5 false. Proves +3.
 * ---------------------------------------------------------------- */
static void test_mode2_right_panel_uses_plus3_cap(void)
{
    panels_setup();
    data_fd2_chapter_intro_dialog_anim_frame_idx = 2;   /* /2 = 1 */
    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_visible_item_count = 5;            /* 0+3<5 true, 0+6<5 false */

    ATLAS_STATIC_SLOT = 0x10;                           /* left static (scroll==0) */
    ATLAS_ANIM_SLOT(1 + 0xD) = 0x6AB;                   /* right animated: idx=1+0xD=0xE */

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 2);

    ASSERT_EQ((long)g_dlg_blit_normal_calls, 2);
    /* right panel chose the animated slot -> proves +3 cap (not +6) */
    ASSERT_EQ((long)g_dlg_blit_dst_log[1], 0xAE36A);
    ASSERT_EQ((long)g_dlg_blit_sprite_log[1],
              (long)((uint32)g_panel_atlas + 0x6ABu));
}

/* ----------------------------------------------------------------
 * mode 2 icon loop: icon_count = min(3, count); each icon blits
 *   src = cache + cache[portrait_id*0x30 + anim_phase*4]
 *   dst = 0xA000E + (0x75 + i*0x1A)*0x140
 * portrait_id is candidate_array[scroll + i].
 * ---------------------------------------------------------------- */
static void test_mode2_icon_loop_arithmetic(void)
{
    int32 *cache;
    uint32 expect_dst0, expect_dst1;
    uint32 expect_src0, expect_src1;

    panels_setup();
    data_fd2_chapter_intro_dialog_anim_frame_idx = 0;
    data_fd2_chapter_intro_dialog_subframe_anim_counter = 2; /* anim_phase = 2 */
    data_fd2_ui_menu_scroll_offset = 1;
    data_fd2_ui_menu_visible_item_count = 2;            /* min(3,2) = 2 icons */
    /* right panel: 1+3=4 >= 2 -> static; left scroll!=0 -> animated. Seed both
     * to keep the panel blits well-defined (not under test here). */
    ATLAS_ANIM_SLOT(0 + 0xB) = 0;
    ATLAS_STATIC_SLOT = 0;

    /* portrait ids at candidate[scroll+0]=candidate[1], candidate[scroll+1]=candidate[2] */
    g_candidate_arr[1] = 0x02;
    g_candidate_arr[2] = 0x05;

    cache = (int32 *)g_portrait_cache;
    /* icon0: portrait_id 2 -> cache[2*0x30 + 2*4] = cache[(0x60+8)/4 = 0x1A] */
    cache[(0x02 * 0x30 + 2 * 4) / 4] = 0x700;
    /* icon1: portrait_id 5 -> cache[5*0x30 + 2*4] */
    cache[(0x05 * 0x30 + 2 * 4) / 4] = 0x800;

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 2);

    /* 2 panel blits + 2 icon blits */
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 2);
    ASSERT_EQ((long)g_blitbgfill_calls, 2);
    ASSERT_EQ((long)g_blitpass_calls, 2);

    expect_dst0 = (0x75u + 0u * 0x1Au) * 0x140u + 0xA000Eu;
    expect_dst1 = (0x75u + 1u * 0x1Au) * 0x140u + 0xA000Eu;
    ASSERT_EQ((long)g_blitpass_dst[0], (long)expect_dst0);
    ASSERT_EQ((long)g_blitpass_dst[1], (long)expect_dst1);

    expect_src0 = (uint32)g_portrait_cache + 0x700u;
    expect_src1 = (uint32)g_portrait_cache + 0x800u;
    ASSERT_EQ((long)g_blitpass_src[0], (long)expect_src0);
    ASSERT_EQ((long)g_blitpass_src[1], (long)expect_src1);
    ASSERT_EQ((long)g_blitpass_stride[0], 0x140);
}

/* ----------------------------------------------------------------
 * mode 2 anim-phase remap: subframe_counter 3 -> phase 1 (so the icon
 * src uses cache[id*0x30 + 1*4], not id*0x30 + 3*4).
 * ---------------------------------------------------------------- */
static void test_mode2_anim_phase_3_maps_to_1(void)
{
    int32 *cache;

    panels_setup();
    data_fd2_chapter_intro_dialog_subframe_anim_counter = 3;  /* -> phase 1 */
    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_visible_item_count = 1;            /* 1 icon */
    g_candidate_arr[0] = 0x01;                          /* portrait_id 1 */

    cache = (int32 *)g_portrait_cache;
    /* distinct values at phase 1 and phase 3 slots so a wrong phase is caught */
    cache[(0x01 * 0x30 + 1 * 4) / 4] = 0xAA;            /* phase 1 (expected) */
    cache[(0x01 * 0x30 + 3 * 4) / 4] = 0xBB;            /* phase 3 (must NOT be used) */

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 2);

    ASSERT_EQ((long)g_blitbgfill_calls, 1);
    /* uses phase-1 slot value 0xAA */
    ASSERT_EQ((long)g_blitpass_src[0],
              (long)((uint32)g_portrait_cache + 0xAAu));
}

/* ----------------------------------------------------------------
 * mode 2 icon-count cap: count > 3 clamps to exactly 3 icons.
 * ---------------------------------------------------------------- */
static void test_mode2_icon_count_caps_at_3(void)
{
    panels_setup();
    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_visible_item_count = 9;            /* min(3,9) = 3 */

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 2);

    ASSERT_EQ((long)g_blitbgfill_calls, 3);
}

void run_gfx_rndmenu_tests(void)
{
    SUITE_BEGIN(gfx_rndmenu);
    RUN_TEST(test_compose_args_frame_nonremap);
    RUN_TEST(test_frame_index_3_remaps_to_1);
    RUN_TEST(test_frame_index_1_unchanged);
    RUN_TEST(test_pose_table_index_formula);
    RUN_TEST(test_subframe_advances_on_odd_frame);
    RUN_TEST(test_subframe_unchanged_on_even_frame);
    RUN_TEST(test_subframe_wraps_at_4_unconditionally);
    RUN_TEST(test_subframe_increment_then_wrap);
    RUN_TEST(test_mode0_corner_sprite_arithmetic);
    RUN_TEST(test_mode1_panels_scroll_zero);
    RUN_TEST(test_mode1_panels_scrolled_and_tail);
    RUN_TEST(test_mode3_calls_roster_grid_then_panels);
    RUN_TEST(test_mode2_right_panel_uses_plus3_cap);
    RUN_TEST(test_mode2_icon_loop_arithmetic);
    RUN_TEST(test_mode2_anim_phase_3_maps_to_1);
    RUN_TEST(test_mode2_icon_count_caps_at_3);
    SUITE_END();
}
