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

/* runtime-char array the roster grid indexes via scroll_offset + iter. Shared
 * by the mode-3 panel test and the fd2_render_party_roster_grid tests below. */
static runtime_char g_roster_chars[16];

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
 *   mode 3     -> the REAL fd2_render_party_roster_grid, whose portrait
 *                 bg-fill blits also land in g_blitpass_* / g_blitbgfill_calls
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
/* fd2_tile_blit_24x24_with_dialog_bg_fill spy (testglob.c, shared g_blitpass_*).
 * In mode 3 the REAL fd2_render_party_roster_grid (src/gfx/rndmenu.c) overlays
 * the roster, so its per-char portrait blits also land here; mode 1/2 never
 * invoke the grid, so g_blitbgfill_calls stays 0 there. */
extern int    g_blitbgfill_calls;

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

    ASSERT_EQ((long)g_blitbgfill_calls, 0);             /* mode 1 -> no roster grid */
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
 * mode 3: identical panels to mode 1, but first overlays the REAL party
 * roster grid with (cursor_idx, 0xA0000). With one party member the real
 * grid makes exactly one portrait bg-fill blit, and its per-char dst is
 * keyed off surface_offset 0xA0000 — proving the overlay ran with the
 * right surface. The two scroll panels still draw afterward.
 * ---------------------------------------------------------------- */
static void test_mode3_calls_roster_grid_then_panels(void)
{
    runtime_char *saved_char_ptr = data_fd2_battle_runtime_char_array_ptr;

    panels_setup();
    data_fd2_ui_menu_cursor_idx = 4;
    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_visible_item_count = 0;            /* 0+6 >= 0 -> right static */

    /* real roster-grid fixture: 1 visible char, names via immediate-END VM.
     * Uses the file-scope g_roster_chars (not a dangling stack array) and the
     * panels_setup portrait cache; the pointer is restored below. */
    memset(g_roster_chars, 0, sizeof(g_roster_chars));
    data_fd2_battle_runtime_char_array_ptr = g_roster_chars;
    data_fd2_shared_menu_party_member_count = 1;
    data_fd2_chapter_intro_dialog_subframe_anim_counter = 0;
    intro_text_all_end();

    fd2_render_chapter_intro_dialog_panels((uint32)g_panel_atlas, 3);

    /* roster grid ran for the single member: one portrait bg-fill blit whose
     * dst is the iter-0 slot computed off surface_offset 0xA0000. */
    ASSERT_EQ((long)g_blitbgfill_calls, 1);
    ASSERT_EQ((long)g_blitpass_dst[0],
              (long)((0x75u) * 0x140u + 0xA0000u + 0xeu));
    /* both panels still drawn */
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 2);

    data_fd2_battle_runtime_char_array_ptr = saved_char_ptr;
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

/* ================================================================
 * fd2_render_shop_item_grid @ 0x2DC55
 *
 * Renders up to 6 items (2-col x 3-row) with category icon, item name,
 * a primary stat (AP/DP/HP/MP) and a price. All blits are reached
 * through the real pipeline against fakes:
 *   fd2_blit_sheet_sprite_at_offset -> real -> fd2_blit_sprite_raw_with_header
 *     spy (g_blitraw_log_*): records every sheet-icon blit. With a fake
 *     offset table (table[i]=i) the sprite index is (logged_sprite - sheet).
 *   fd2_blit_indexed_sprite_at_xy / fd2_render_decimal_number_to_buffer ->
 *     real -> fd2_rle_blit_sprite spy (g_rle_blit_log_*): records the "—"
 *     placeholder sprite and every decimal digit glyph.
 *   fd2_display_dialog_scene: REAL against the immediate-END text program
 *     (intro_text_all_end) -> returns at once, no fopen / no glyph blits.
 *   fd2_get_item_effect_entry: REAL -> &item_effect_table[id].type, so the
 *     entry offsets the grid reads (type@+0, ap@+1, dp@+5, use_effect@+0xD,
 *     use_param@+0xE, price@+0x13) are seeded straight into the struct.
 *
 * Risk-bearing logic under test: the visible-count cap (6 / tail-clamp 5),
 * the kind-based stat dispatch (weapon/armor/HP/MP/placeholder), the
 * category-icon selection, the per-item column/row dst arithmetic, and the
 * (price*3)/4 sell-mode discount.
 * ================================================================ */

extern int    g_blitraw_log_on;
extern int    g_blitraw_count;
extern uint32 g_blitraw_log_dst[512];
extern uint32 g_blitraw_log_sprite[512];
extern int    g_rle_blit_calls;
extern int    g_rle_blit_log_on;
extern uint32 g_rle_blit_log_sprite[64];
extern uint32 g_rle_blit_log_dst[64];

/* anim sprite sheet (icons + digit glyphs): header(6) + 256 int32 entries,
 * table[i]=i so resolved sprite = sheet + sprite_idx. */
static int32 g_shop_anim_sheet[2 + 256];
/* menu-screen atlas (price coin icon) — distinct sheet from the anim sheet. */
static int32 g_shop_menu_atlas[2 + 256];
/* item id array passed to the grid (absolute ids; grid indexes via
 * scroll_offset + iter). */
static uint8 g_shop_ids[64];

static uint32 g_shop_anim_base;
static uint32 g_shop_menu_base;

static void shop_setup(void)
{
    uint8 *anim = (uint8 *)g_shop_anim_sheet;
    uint8 *menu = (uint8 *)g_shop_menu_atlas;
    int    i;

    for (i = 0; i < 256; i++) {
        *(int32 *)(anim + 6 + i * 4) = i;
        *(int32 *)(menu + 6 + i * 4) = i;
    }
    g_shop_anim_base = (uint32)anim;
    g_shop_menu_base = (uint32)menu;
    data_fd2_ui_anim_sprite_sheet_ptr = g_shop_anim_base;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = g_shop_menu_base;

    /* item names go through the real dialog VM; immediate-END = no output */
    intro_text_all_end();

    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    for (i = 0; i < 64; i++) {
        g_shop_ids[i] = 0;
    }

    data_fd2_ui_menu_scroll_offset = 0;

    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;
}

/* seed item_effect_table[id]: type, ap, dp, use_effect, use_param, price.
 * fd2_get_item_effect_entry returns &table[id].type, so the grid reads
 * type@p+0, ap@p+1, dp@p+5, use_effect@p+0xD, use_param@p+0xE, price@p+0x13. */
static void shop_seed_item(uint8 id, uint8 type, int16 ap, int16 dp,
                           uint8 use_effect, int16 use_param, uint16 price)
{
    item_effect *e = &data_fd2_battle_item_effect_table[id];
    e->type = type;
    e->ap = (uint16)ap;
    e->dp = (uint16)dp;
    e->use_effect = use_effect;
    e->use_param_lo = (uint8)((uint16)use_param & 0xff);
    e->use_param_hi = (uint8)(((uint16)use_param >> 8) & 0xff);
    e->price = price;
}

/* recover the sheet-icon sprite index of the g_blitraw_log entry at `idx`. */
static uint32 shop_sheet_sprite(int idx)
{
    return g_blitraw_log_sprite[idx] - g_shop_anim_base;
}

/* assert a `digits`-wide "%0.<digits>d" decimal of `value` rendered at `dst`
 * with sprite base `color`, starting at rle-log index `from`. */
static void shop_assert_decimal(int from, uint32 dst, uint32 value,
                                uint32 color, uint32 digits)
{
    char fmt[8];
    char s[20];
    int  i;

    fmt[0] = '%'; fmt[1] = '0'; fmt[2] = '.';
    fmt[3] = (char)('0' + digits);
    fmt[4] = 'd'; fmt[5] = '\0';
    sprintf(s, fmt, value);

    for (i = 0; i < (int)digits; i++) {
        ASSERT_EQ((long)(g_rle_blit_log_sprite[from + i] - g_shop_anim_base),
                  (long)(color + (uint32)(uint8)s[i] - 0x30));
        ASSERT_EQ((long)g_rle_blit_log_dst[from + i],
                  (long)(dst + (uint32)(i * 6)));
    }
}

/* ----------------------------------------------------------------
 * Visible-count cap: item_count <= 6 -> draw every item. Use 4
 * placeholder items (kind 0x20, use_effect not 5/0xB) so each item makes
 * exactly two sheet blits (category icon + coin icon); 4 items -> 8.
 * ---------------------------------------------------------------- */
static void test_cap_small_count_draws_all(void)
{
    int i;

    shop_setup();
    for (i = 0; i < 4; i++) {
        g_shop_ids[i] = (uint8)i;
        shop_seed_item((uint8)i, 0x20, 0, 0, 0x00, 0, 100);
    }

    fd2_render_shop_item_grid(4, g_shop_ids, 99, 0x1000, 0);

    ASSERT_EQ((long)g_blitraw_count, 8);            /* 4 items x 2 sheet blits */
}

/* item_count > 6 but the scroll window has >= 6 items below it
 * (item_count >= scroll_offset + 6) -> draw_count clamps to exactly 6. */
static void test_cap_large_count_draws_six(void)
{
    int i;

    shop_setup();
    data_fd2_ui_menu_scroll_offset = 0;             /* 10 >= 0+6 -> 6 */
    for (i = 0; i < 16; i++) {
        g_shop_ids[i] = (uint8)i;
        shop_seed_item((uint8)i, 0x20, 0, 0, 0x00, 0, 100);
    }

    fd2_render_shop_item_grid(10, g_shop_ids, 99, 0x1000, 0);

    ASSERT_EQ((long)g_blitraw_count, 12);           /* 6 items x 2 */
}

/* item_count > 6 AND item_count < scroll_offset + 6 -> tail-clamp to 5.
 * scroll_offset = 4, item_count = 8 -> 8 < 4+6=10 -> draw_count = 5. */
static void test_cap_tail_clamps_to_five(void)
{
    int i;

    shop_setup();
    data_fd2_ui_menu_scroll_offset = 4;
    for (i = 0; i < 32; i++) {
        g_shop_ids[i] = (uint8)i;
        shop_seed_item((uint8)i, 0x20, 0, 0, 0x00, 0, 100);
    }

    fd2_render_shop_item_grid(8, g_shop_ids, 99, 0x1000, 0);

    ASSERT_EQ((long)g_blitraw_count, 10);           /* 5 items x 2 */
}

/* ----------------------------------------------------------------
 * Weapon (kind < 0x15): category icon 0x3B, AP icon 0x40, stat = entry.ap
 * (3 digits). One item -> sheet blits: [0]=category 0x3B, [1]=AP icon 0x40,
 * [2]=coin 0x0F. rle: [0..2]=AP value 3 digits, [3..7]=price 5 digits.
 * Also verifies the iter=0 dst arithmetic for the category icon.
 * ---------------------------------------------------------------- */
static void test_weapon_stat_and_icons(void)
{
    uint32 surf = 0x1000;
    uint32 cat_dst;
    uint32 ap_dst;

    shop_setup();
    g_shop_ids[0] = 7;
    shop_seed_item(7, 0x05, 123, 999, 0x00, 0, 250);    /* weapon, ap=123 */

    fd2_render_shop_item_grid(1, g_shop_ids, 99, (int32)surf, 0);

    /* sheet icons: category 0x3B, AP 0x40, coin 0x0F (coin from menu atlas) */
    ASSERT_EQ((long)g_blitraw_count, 3);
    ASSERT_EQ((long)shop_sheet_sprite(0), 0x3B);        /* weapon category */
    ASSERT_EQ((long)shop_sheet_sprite(1), 0x40);        /* AP icon */
    /* coin uses the menu atlas, so recover against that base */
    ASSERT_EQ((long)(g_blitraw_log_sprite[2] - g_shop_menu_base), 0x0F);

    /* iter=0: col_x=10, row_off=0. category dst = (0+0x77)*0x140+surf+10 */
    cat_dst = (0x77u) * 0x140u + surf + 10u;
    ASSERT_EQ((long)g_blitraw_log_dst[0], (long)cat_dst);
    /* AP icon dst = (0+0x79)*0x140 + surf + 10 + 0x5F */
    ap_dst = (0x79u) * 0x140u + surf + 10u + 0x5Fu;
    ASSERT_EQ((long)g_blitraw_log_dst[1], (long)ap_dst);

    /* rle: AP value 123 (3 digits, base 0x2A) then price 250 (5 digits, 0x77) */
    shop_assert_decimal(0, (0x79u) * 0x140u + surf + 10u + 0x76u, 123, 0x2a, 3);
    shop_assert_decimal(3, surf + 10u + 0x68u + (0x83u) * 0x140u, 250, 0x77, 5);
    ASSERT_EQ((long)g_rle_blit_calls, 8);               /* 3 + 5 */
}

/* Armor (0x15 <= kind < 0x20): category icon 0x3C, DP icon 0x41,
 * stat = entry.dp. */
static void test_armor_stat_and_icons(void)
{
    shop_setup();
    g_shop_ids[0] = 3;
    shop_seed_item(3, 0x18, 111, 87, 0x00, 0, 400);     /* armor, dp=87 */

    fd2_render_shop_item_grid(1, g_shop_ids, 99, 0x2000, 0);

    ASSERT_EQ((long)g_blitraw_count, 3);
    ASSERT_EQ((long)shop_sheet_sprite(0), 0x3C);        /* armor category */
    ASSERT_EQ((long)shop_sheet_sprite(1), 0x41);        /* DP icon */
    /* DP value 87, 3 digits */
    shop_assert_decimal(0, (0x79u) * 0x140u + 0x2000u + 10u + 0x76u, 87, 0x2a, 3);
}

/* HP-boost consumable (kind==0x20, use_effect==5): category 0x3D, HP icon
 * 0x42, stat = use_param. */
static void test_hp_consumable_stat_and_icons(void)
{
    shop_setup();
    g_shop_ids[0] = 0x80;
    shop_seed_item(0x80, 0x20, 0, 0, 0x05, 50, 30);     /* HP boost, param=50 */

    fd2_render_shop_item_grid(1, g_shop_ids, 99, 0x3000, 0);

    ASSERT_EQ((long)g_blitraw_count, 3);
    ASSERT_EQ((long)shop_sheet_sprite(0), 0x3D);        /* other category */
    ASSERT_EQ((long)shop_sheet_sprite(1), 0x42);        /* HP icon */
    shop_assert_decimal(0, (0x79u) * 0x140u + 0x3000u + 10u + 0x76u, 50, 0x2a, 3);
}

/* MP-boost consumable (kind==0x20, use_effect==0xB): category 0x3D, MP icon
 * 0x43, stat = use_param. */
static void test_mp_consumable_stat_and_icons(void)
{
    shop_setup();
    g_shop_ids[0] = 0x90;
    shop_seed_item(0x90, 0x20, 0, 0, 0x0B, 25, 60);     /* MP boost, param=25 */

    fd2_render_shop_item_grid(1, g_shop_ids, 99, 0x4000, 0);

    ASSERT_EQ((long)g_blitraw_count, 3);
    ASSERT_EQ((long)shop_sheet_sprite(0), 0x3D);        /* other category */
    ASSERT_EQ((long)shop_sheet_sprite(1), 0x43);        /* MP icon */
    shop_assert_decimal(0, (0x79u) * 0x140u + 0x4000u + 10u + 0x76u, 25, 0x2a, 3);
}

/* Stat-less item (kind==0x20, use_effect neither 5 nor 0xB): category 0x3D,
 * no stat icon/decimal; a "—" placeholder sprite 0x29 is RLE-blit instead.
 * Sheet blits = category + coin = 2 (no stat icon). rle = placeholder(1) +
 * price(5) = 6, placeholder first. ---------------------------------------- */
static void test_placeholder_when_no_stat(void)
{
    uint32 surf = 0x5000;
    uint32 ph_dst;

    shop_setup();
    g_shop_ids[0] = 0x40;
    shop_seed_item(0x40, 0x20, 0, 0, 0x07, 0, 80);      /* use_effect 7 -> placeholder */

    fd2_render_shop_item_grid(1, g_shop_ids, 99, (int32)surf, 0);

    ASSERT_EQ((long)g_blitraw_count, 2);                /* category + coin only */
    ASSERT_EQ((long)shop_sheet_sprite(0), 0x3D);        /* other category */

    /* placeholder sprite 0x29 via the rle path, dst =
     * (0+0x7B)*0x140 + surf + 10 + 0x5F */
    ph_dst = (0x7Bu) * 0x140u + surf + 10u + 0x5Fu;
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - g_shop_anim_base), 0x29);
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)ph_dst);

    /* only the 5 price digits follow the placeholder (no 3-digit stat) */
    shop_assert_decimal(1, surf + 10u + 0x68u + (0x83u) * 0x140u, 80, 0x77, 5);
    ASSERT_EQ((long)g_rle_blit_calls, 6);               /* 1 placeholder + 5 price */
}

/* ----------------------------------------------------------------
 * Sell-mode discount: price -> price * 3 / 4. Full price when flag==0,
 * discounted when flag!=0. The 5 price digits encode the rendered value.
 * price=100 -> 75 (300/4); a non-multiple price=99 -> 74 (297/4 = 74.25).
 * ---------------------------------------------------------------- */
static void test_price_full_when_not_sell(void)
{
    shop_setup();
    g_shop_ids[0] = 1;
    shop_seed_item(1, 0x20, 0, 0, 0x07, 0, 12345);

    fd2_render_shop_item_grid(1, g_shop_ids, 99, 0x100, 0);

    /* placeholder at rle[0]; price digits at rle[1..5] = full 12345 */
    shop_assert_decimal(1, 0x100u + 10u + 0x68u + (0x83u) * 0x140u, 12345, 0x77, 5);
}

static void test_price_discounted_when_sell(void)
{
    shop_setup();
    g_shop_ids[0] = 1;
    shop_seed_item(1, 0x20, 0, 0, 0x07, 0, 100);

    fd2_render_shop_item_grid(1, g_shop_ids, 99, 0x100, 1);

    /* 100 * 3 / 4 = 75 */
    shop_assert_decimal(1, 0x100u + 10u + 0x68u + (0x83u) * 0x140u, 75, 0x77, 5);
}

static void test_price_discount_rounds_toward_zero(void)
{
    shop_setup();
    g_shop_ids[0] = 1;
    shop_seed_item(1, 0x20, 0, 0, 0x07, 0, 99);

    fd2_render_shop_item_grid(1, g_shop_ids, 99, 0x100, 1);

    /* 99 * 3 = 297; 297 >> 2 = 74 (toward zero) */
    shop_assert_decimal(1, 0x100u + 10u + 0x68u + (0x83u) * 0x140u, 74, 0x77, 5);
}

/* ----------------------------------------------------------------
 * Second column / second row dst arithmetic. iter=1 -> col_x =
 * (1%2)*0x94+10 = 0x9E, row_off = (1/2)*0x1A = 0 (still row 0). iter=2 ->
 * col_x = 10, row_off = 0x1A. Drive 3 placeholder items and check the
 * coin-icon dst for iter 1 and 2 to prove the column/row formulas.
 * ---------------------------------------------------------------- */
static void test_column_row_offsets(void)
{
    uint32 surf = 0x8000;
    uint32 coin_dst_i1;
    uint32 coin_dst_i2;

    shop_setup();
    g_shop_ids[0] = 0; g_shop_ids[1] = 0; g_shop_ids[2] = 0;
    shop_seed_item(0, 0x20, 0, 0, 0x07, 0, 1);          /* placeholder */

    fd2_render_shop_item_grid(3, g_shop_ids, 99, (int32)surf, 0);

    /* each placeholder item makes 2 sheet blits: [cat, coin]. coin index for
     * item n is 2*n + 1. */
    /* iter=1: col_x = 0x9E, row_off = 0 -> coin dst = (0+0x83)*0x140+surf+0x9E+0x5F */
    coin_dst_i1 = (0x83u) * 0x140u + surf + 0x9Eu + 0x5Fu;
    ASSERT_EQ((long)g_blitraw_log_dst[2 * 1 + 1], (long)coin_dst_i1);
    /* iter=2: col_x = 10, row_off = 0x1A -> coin dst = (0x1A+0x83)*0x140+surf+10+0x5F */
    coin_dst_i2 = (0x1Au + 0x83u) * 0x140u + surf + 10u + 0x5Fu;
    ASSERT_EQ((long)g_blitraw_log_dst[2 * 2 + 1], (long)coin_dst_i2);
}

/* ================================================================
 * fd2_render_party_roster_grid @ 0x2EA90
 *
 * 2-col x 3-row party-roster viewport (up to 6 chars). Per char:
 *   - a 24x24 portrait bg-fill blit reached through the real
 *     fd2_tile_blit_24x24_with_dialog_bg_fill spy (g_blitpass_* /
 *     g_blitbgfill_calls): verifies the portrait src (blink-frame
 *     atlas indexing) and the dst column/row arithmetic.
 *   - a class/job name via the REAL fd2_display_dialog_scene. The text
 *     program points exactly ONE page (= char_id + 1) at a single-glyph
 *     program and every other page at END, so a rendered glyph proves the
 *     page index, and g_dlg_glyph_last_pos / g_dlg_glyph_last_p5 capture
 *     the name dst arithmetic and the selection border glyph.
 *
 * Risk-bearing logic under test: the blink-frame remap (3->1), the
 * visible-count cap (6 / tail-clamp 5), the per-char char_idx/col/row
 * arithmetic, the portrait blink-atlas source index, the name page
 * (char_id+1) + dst, and the highlight border glyph (0xC9 vs 0xCD).
 * ================================================================ */

extern uint32 g_dlg_glyph_last_idx;  /* last rendered glyph index   (testglob.c) */
extern uint32 g_dlg_glyph_last_pos;  /* last glyph render position  (testglob.c) */
extern uint32 g_dlg_glyph_last_p5;   /* glyph colour/border param   (testglob.c) */

/* (g_roster_chars defined near the top; shared with the mode-3 panel test) */
/* dialog text program. Layout mirrors intro_text_all_end: the page-pointer
 * table occupies words 0..0x3BF; the opcode data lives ABOVE it so the
 * table-init loop never clobbers it. byte 0x780 (word 0x3C0) = shared END;
 * byte 0x782 (word 0x3C1) = a 1-glyph blob, byte 0x784 = its trailing END. */
static uint16 g_roster_text[0x400];

#define ROSTER_END_OFF     0x780           /* byte offset of the shared END  */
#define ROSTER_GLYPH_OFF   0x782           /* byte offset of the 1-glyph blob */

/* All pages -> END (no glyph). */
static void roster_text_all_end(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        g_roster_text[i] = 0;
    }
    *(int16 *)((uint8 *)g_roster_text + ROSTER_END_OFF) = -1;       /* shared END */
    for (i = 0; i < 0x3c0; i++) {
        g_roster_text[i] = (uint16)ROSTER_END_OFF;                 /* default END */
    }
    data_fd2_all_game_text_ptr = (uint32)g_roster_text;
}

/* Like roster_text_all_end but aim page `glyph_page` at a [glyph_val][END]
 * blob so exactly that page renders one glyph (proving the page index reached
 * the VM); render_pos and p5 of that glyph then capture the name arithmetic. */
static void roster_text_glyph_at(uint32 glyph_page, uint16 glyph_val)
{
    roster_text_all_end();
    *(uint16 *)((uint8 *)g_roster_text + ROSTER_GLYPH_OFF)     = glyph_val;
    *(int16  *)((uint8 *)g_roster_text + ROSTER_GLYPH_OFF + 2) = -1;
    g_roster_text[glyph_page] = (uint16)ROSTER_GLYPH_OFF;
}

/* runtime-char array pointer saved by roster_setup so roster_teardown can
 * restore the testglob default (g_test_rc_array) other suites depend on. */
static runtime_char *roster_saved_char_ptr;

/* common roster fixture: N members, scroll offset, blink counter, portrait
 * cache cleared; names default to all-END (override with roster_text_glyph_at). */
static void roster_setup(uint32 member_count, uint32 scroll, uint32 subframe)
{
    roster_saved_char_ptr = data_fd2_battle_runtime_char_array_ptr;

    memset(g_roster_chars, 0, sizeof(g_roster_chars));
    memset(g_portrait_cache, 0, sizeof(g_portrait_cache));

    data_fd2_battle_runtime_char_array_ptr = g_roster_chars;
    portrait_sprite_cache = (uint32)g_portrait_cache;
    data_fd2_shared_menu_party_member_count = member_count;
    data_fd2_ui_menu_scroll_offset = scroll;
    data_fd2_chapter_intro_dialog_subframe_anim_counter = subframe;

    roster_text_all_end();

    g_blitpass_calls = 0;
    g_blitbgfill_calls = 0;
    g_dlg_glyph_calls = 0;
}

/* Restore the runtime-char array pointer roster_setup repointed, so a following
 * suite (e.g. ui_menu/status.c, which relies on the g_test_rc_array wiring) is
 * not polluted by the local roster array. */
static void roster_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = roster_saved_char_ptr;
}

/* ----------------------------------------------------------------
 * Visible-count cap: member_count <= 6 -> draw every member (one
 * portrait bg-fill blit each).
 * ---------------------------------------------------------------- */
static void test_roster_cap_small_draws_all(void)
{
    roster_setup(4, 0, 0);

    fd2_render_party_roster_grid(99, 0x1000);

    ASSERT_EQ((long)g_blitbgfill_calls, 4);
    roster_teardown();
}

/* member_count > 6 with a full window below the scroll
 * (member_count >= scroll + 6) -> draw exactly 6. */
static void test_roster_cap_large_draws_six(void)
{
    roster_setup(10, 0, 0);                 /* 10 >= 0+6 -> 6 */

    fd2_render_party_roster_grid(99, 0x1000);

    ASSERT_EQ((long)g_blitbgfill_calls, 6);
    roster_teardown();
}

/* member_count > 6 AND member_count < scroll + 6 -> tail-clamp to 5.
 * scroll = 4, count = 8 -> 8 < 4+6=10 -> draw 5. */
static void test_roster_cap_tail_clamps_to_five(void)
{
    roster_setup(8, 4, 0);

    fd2_render_party_roster_grid(99, 0x1000);

    ASSERT_EQ((long)g_blitbgfill_calls, 5);
    roster_teardown();
}

/* ----------------------------------------------------------------
 * Portrait column/row dst arithmetic across both columns and a row
 * advance: iter 0..3 -> col_off = (iter%2)*0x84, row_off=(iter/2)*0x1A,
 * dst = (row_off+0x75)*0x140 + surf + 0xE + col_off.
 * ---------------------------------------------------------------- */
static void test_roster_portrait_col_row_offsets(void)
{
    uint32 surf = 0x2000;

    roster_setup(4, 0, 0);

    fd2_render_party_roster_grid(99, surf);

    ASSERT_EQ((long)g_blitpass_calls, 4);
    /* iter0: col 0, row 0 */
    ASSERT_EQ((long)g_blitpass_dst[0],
              (long)((0x00u + 0x75u) * 0x140u + surf + 0xeu + 0x00u));
    /* iter1: col 0x84, row 0 */
    ASSERT_EQ((long)g_blitpass_dst[1],
              (long)((0x00u + 0x75u) * 0x140u + surf + 0xeu + 0x84u));
    /* iter2: col 0, row 0x1A */
    ASSERT_EQ((long)g_blitpass_dst[2],
              (long)((0x1au + 0x75u) * 0x140u + surf + 0xeu + 0x00u));
    /* iter3: col 0x84, row 0x1A */
    ASSERT_EQ((long)g_blitpass_dst[3],
              (long)((0x1au + 0x75u) * 0x140u + surf + 0xeu + 0x84u));
    ASSERT_EQ((long)g_blitpass_stride[0], 0x140);
    roster_teardown();
}

/* ----------------------------------------------------------------
 * Portrait source uses char_idx = scroll + iter to index the per-char
 * blink-atlas: src = cache + cache[char_idx*0x30 + blink*4]. With
 * scroll=2 the first drawn char_idx is 2. blink (subframe) = 1.
 * ---------------------------------------------------------------- */
static void test_roster_portrait_src_uses_scroll_and_blink(void)
{
    int32 *cache;

    roster_setup(2, 2, 1);                  /* scroll 2 -> char_idx 2,3; blink 1 */
    cache = (int32 *)g_portrait_cache;
    /* char_idx 2, blink 1 -> cache[2*0x30 + 1*4] */
    cache[(2 * 0x30 + 1 * 4) / 4] = 0x123;

    fd2_render_party_roster_grid(99, 0x1000);

    ASSERT_EQ((long)g_blitpass_calls, 2);
    ASSERT_EQ((long)g_blitpass_src[0],
              (long)((uint32)g_portrait_cache + 0x123u));
    roster_teardown();
}

/* ----------------------------------------------------------------
 * Blink-frame remap: subframe_counter 3 -> blink_frame 1 (source uses
 * cache[id*0x30 + 1*4], NOT id*0x30 + 3*4).
 * ---------------------------------------------------------------- */
static void test_roster_blink_frame_3_maps_to_1(void)
{
    int32 *cache;

    roster_setup(1, 0, 3);                  /* subframe 3 -> blink 1 */
    cache = (int32 *)g_portrait_cache;
    cache[(0 * 0x30 + 1 * 4) / 4] = 0xAA;   /* blink 1 (expected) */
    cache[(0 * 0x30 + 3 * 4) / 4] = 0xBB;   /* blink 3 (must NOT be used) */

    fd2_render_party_roster_grid(99, 0x1000);

    ASSERT_EQ((long)g_blitpass_calls, 1);
    ASSERT_EQ((long)g_blitpass_src[0],
              (long)((uint32)g_portrait_cache + 0xAAu));
    roster_teardown();
}

/* blink-frame passthrough: subframe 2 (not 3) used as-is. */
static void test_roster_blink_frame_passthrough(void)
{
    int32 *cache;

    roster_setup(1, 0, 2);                  /* subframe 2 -> blink 2 */
    cache = (int32 *)g_portrait_cache;
    cache[(0 * 0x30 + 2 * 4) / 4] = 0x5C;

    fd2_render_party_roster_grid(99, 0x1000);

    ASSERT_EQ((long)g_blitpass_src[0],
              (long)((uint32)g_portrait_cache + 0x5Cu));
    roster_teardown();
}

/* ----------------------------------------------------------------
 * Name dialog: page index = char.char_id + 1, dst =
 * (row_off+0x79)*0x140 + surf + 0x28 + col_off, and the highlighted
 * char's border glyph = 0xC9. Single member (iter0: col0,row0) with
 * char_id 0x0A so page = 0x0B carries the one-glyph blob.
 * ---------------------------------------------------------------- */
static void test_roster_name_page_dst_and_highlight_border(void)
{
    uint32 surf = 0x4000;

    roster_setup(1, 0, 0);
    g_roster_chars[0].char_id = 0x0A;       /* page = 0x0B */
    roster_text_glyph_at(0x0B, 0x37);       /* one glyph (idx 0x37) on page 0x0B */

    fd2_render_party_roster_grid(0, surf);  /* highlight_idx 0 == char_idx 0 */

    /* exactly one glyph rendered -> page index 0x0B (= char_id+1) reached VM */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x37);
    /* name dst arithmetic (iter0: col0,row0) */
    ASSERT_EQ((long)g_dlg_glyph_last_pos,
              (long)((0x00u + 0x79u) * 0x140u + surf + 0x28u + 0x00u));
    /* highlighted -> border 0xC9 */
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xC9);
    roster_teardown();
}

/* non-highlighted char -> border glyph 0xCD. Two members, highlight a
 * different slot; verify the iter-0 char's name carries 0xCD. */
static void test_roster_border_not_highlighted(void)
{
    roster_setup(2, 0, 0);
    g_roster_chars[0].char_id = 0x03;       /* page = 0x04 */
    g_roster_chars[1].char_id = 0x07;
    roster_text_glyph_at(0x04, 0x22);       /* only char_idx 0's page emits a glyph */

    fd2_render_party_roster_grid(1, 0x4000);/* highlight slot 1, not 0 */

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);  /* only char_idx 0 page has a glyph */
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xCD);
    roster_teardown();
}

/* ================================================================
 * fd2_render_party_roster_with_item_stat_preview @ 0x2EBE0
 *
 * Single-column class-filtered roster (up to 3 visible chars) with a
 * side-by-side current-vs-preview stat block (AP/DP/DX/Stat4). Per char:
 *   - a 24x24 portrait bg-fill blit (real fd2_tile_blit_24x24_with_dialog_bg_fill
 *     spy g_blitpass_* / g_blitbgfill_calls): pins the blink-frame portrait
 *     source and the row dst arithmetic.
 *   - a char name via the REAL fd2_display_dialog_scene against a one-glyph /
 *     all-END text program: pins the page index (char_id + 1) and the highlight
 *     border glyph (g_dlg_glyph_last_p5).
 *   - 8 stat-icon blits via the recording fd2_dialog_sprite_blit_normal stub
 *     (g_dlg_blit_dst_log / g_dlg_blit_sprite_log): pins each icon's atlas sprite
 *     offset (+0x4E/+0x52/+0x56/+0x5A current, +0x5E preview) and dst.
 *   - 8 decimal renders via the REAL fd2_render_decimal_number_to_buffer ->
 *     fd2_rle_blit_sprite spy (g_rle_blit_log_*) against a fake digit sheet
 *     (sheet[i]=i): pins the value, the compare colour (sprite base) and the dst.
 *   - the preview stats come from the REAL
 *     fd2_compute_equipped_stats_with_item_preview (reads the item-effect table
 *     via the real fd2_get_item_effect_entry); the compare colour per stat comes
 *     from the fd2_pick_stat_compare_color stub (faithful 3-branch logic +
 *     g_pick_color_* recording).
 *
 * Risk-bearing logic under test: the visible-count cap (min 3), the blink-frame
 * remap (3->1), the candidate_array[scroll+iter] char index, the highlight border
 * (0xC9 vs 0xCD), the four stat columns' current/preview field mapping and
 * compare pairing, and the dense row/offset dst arithmetic of every icon + digit.
 * ================================================================ */

extern int    g_dlg_blit_normal_calls;          /* (declared above too) */
extern int    g_pick_color_calls;
extern int32  g_pick_color_cur_log[16];
extern int32  g_pick_color_prev_log[16];

/* preview-renderer fixture */
static uint8        g_pv_cands[16];
static runtime_char g_pv_chars[16];
static uint8        g_pv_cache[512];
static int32        g_pv_anim[2 + 256];
static uint8        g_pv_atlas[256];
static uint16       g_pv_text[0x400];
static runtime_char *g_pv_saved_char_ptr;

#define PV_END_OFF    0x780
#define PV_GLYPH_OFF  0x782

/* stat-icon atlas dwords: distinct per slot so resolved sprite = atlas+atlas[off]
 * uniquely identifies which icon slot the renderer read. */
#define PV_ATLAS_AP   0x100
#define PV_ATLAS_DP   0x200
#define PV_ATLAS_DX   0x300
#define PV_ATLAS_ST4  0x400
#define PV_ATLAS_PREV 0x500

static void pv_text_all_end(void)
{
    int i;
    for (i = 0; i < 0x400; i++) {
        g_pv_text[i] = 0;
    }
    *(int16 *)((uint8 *)g_pv_text + PV_END_OFF) = -1;
    for (i = 0; i < 0x3c0; i++) {
        g_pv_text[i] = (uint16)PV_END_OFF;
    }
    data_fd2_all_game_text_ptr = (uint32)g_pv_text;
}

/* aim page `glyph_page` at a [glyph][END] blob (one rendered glyph). */
static void pv_text_glyph_at(uint32 glyph_page, uint16 glyph_val)
{
    pv_text_all_end();
    *(uint16 *)((uint8 *)g_pv_text + PV_GLYPH_OFF)     = glyph_val;
    *(int16  *)((uint8 *)g_pv_text + PV_GLYPH_OFF + 2) = -1;
    g_pv_text[glyph_page] = (uint16)PV_GLYPH_OFF;
}

/* member_count chars, scroll, blink (subframe). Candidate i -> char i. The anim
 * digit sheet and the menu atlas use table[i]=i so a resolved sprite = base+idx.
 * Stat-icon atlas dwords are seeded distinct. Item table zeroed. */
static void pv_setup(uint32 scroll, uint32 subframe)
{
    int i;
    uint8 *anim = (uint8 *)g_pv_anim;

    g_pv_saved_char_ptr = data_fd2_battle_runtime_char_array_ptr;

    memset(g_pv_chars, 0, sizeof(g_pv_chars));
    memset(g_pv_cache, 0, sizeof(g_pv_cache));
    memset(g_pv_anim, 0, sizeof(g_pv_anim));
    memset(g_pv_atlas, 0, sizeof(g_pv_atlas));
    for (i = 0; i < 256; i++) {
        *(int32 *)(anim + 6 + i * 4) = i;
    }
    for (i = 0; i < 16; i++) {
        g_pv_cands[i] = (uint8)i;
    }
    *(int32 *)(g_pv_atlas + 0x4e) = PV_ATLAS_AP;
    *(int32 *)(g_pv_atlas + 0x52) = PV_ATLAS_DP;
    *(int32 *)(g_pv_atlas + 0x56) = PV_ATLAS_DX;
    *(int32 *)(g_pv_atlas + 0x5a) = PV_ATLAS_ST4;
    *(int32 *)(g_pv_atlas + 0x5e) = PV_ATLAS_PREV;

    data_fd2_battle_runtime_char_array_ptr = g_pv_chars;
    portrait_sprite_cache = (uint32)g_pv_cache;
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_pv_anim;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_pv_atlas;
    data_fd2_ui_menu_scroll_offset = scroll;
    data_fd2_chapter_intro_dialog_subframe_anim_counter = subframe;
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    pv_text_all_end();

    g_blitpass_calls = 0;
    g_blitbgfill_calls = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_blit_normal_calls = 0;
    g_pick_color_calls = 0;
    g_rle_blit_log_on = 1;
    g_rle_blit_calls = 0;
}

static void pv_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_pv_saved_char_ptr;
    g_rle_blit_log_on = 0;
}

/* Seed item `item_id`'s effect entry: ap@+1, ht@+3, dp@+5, ev@+7 (the bonuses
 * the preview-compute adds), and type@+0 (category; 0 = weapon, keeps the
 * opposite-category equipped-item loop a no-op when slots are empty). */
static void pv_seed_item(uint32 item_id, int16 ap, int16 ht, int16 dp, int16 ev)
{
    data_fd2_battle_item_effect_table[item_id].type = 0;
    data_fd2_battle_item_effect_table[item_id].ap = (uint16)ap;
    data_fd2_battle_item_effect_table[item_id].ht = (uint16)ht;
    data_fd2_battle_item_effect_table[item_id].dp = (uint16)dp;
    data_fd2_battle_item_effect_table[item_id].ev = (uint16)ev;
}

/* ----------------------------------------------------------------
 * Visible-count cap: draw_count = min(candidate_count, 3). One portrait
 * bg-fill blit per drawn row.
 * ---------------------------------------------------------------- */
static void test_preview_cap_min_of_count_and_3(void)
{
    pv_setup(0, 0);

    g_blitbgfill_calls = 0;
    fd2_render_party_roster_with_item_stat_preview(2, (uint32)g_pv_cands, 0, 99, 0x1000);
    ASSERT_EQ((long)g_blitbgfill_calls, 2);

    g_blitbgfill_calls = 0;
    fd2_render_party_roster_with_item_stat_preview(5, (uint32)g_pv_cands, 0, 99, 0x1000);
    ASSERT_EQ((long)g_blitbgfill_calls, 3);   /* capped at 3 */

    g_blitbgfill_calls = 0;
    fd2_render_party_roster_with_item_stat_preview(0, (uint32)g_pv_cands, 0, 99, 0x1000);
    ASSERT_EQ((long)g_blitbgfill_calls, 0);   /* nothing to draw */
    pv_teardown();
}

/* ----------------------------------------------------------------
 * Portrait dst/src: char_idx = candidate_array[scroll + iter]; dst =
 * row_y*0x140 + surf + 0xE with row_y = iter*0x1A + 0x75; src = cache +
 * cache[char_idx*0x30 + blink*4]. scroll 0, blink 0, 2 rows.
 * ---------------------------------------------------------------- */
static void test_preview_portrait_dst_src(void)
{
    uint32 surf = 0x2000;
    int32 *cache;

    pv_setup(0, 0);
    cache = (int32 *)g_pv_cache;
    cache[(0 * 0x30 + 0 * 4) / 4] = 0x111;   /* char 0, blink 0 */
    cache[(1 * 0x30 + 0 * 4) / 4] = 0x222;   /* char 1, blink 0 */

    fd2_render_party_roster_with_item_stat_preview(2, (uint32)g_pv_cands, 0, 99, surf);

    ASSERT_EQ((long)g_blitpass_calls, 2);
    /* row 0: row_y = 0x75 */
    ASSERT_EQ((long)g_blitpass_dst[0], (long)(0x75u * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_pv_cache + 0x111u));
    /* row 1: row_y = 0x1A + 0x75 = 0x8F */
    ASSERT_EQ((long)g_blitpass_dst[1], (long)(0x8fu * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_src[1], (long)((uint32)g_pv_cache + 0x222u));
    ASSERT_EQ((long)g_blitpass_stride[0], 0x140);
    pv_teardown();
}

/* char_idx uses scroll + iter: scroll 2 -> first drawn char is cands[2]. */
static void test_preview_char_idx_uses_scroll(void)
{
    int32 *cache;

    pv_setup(2, 0);                          /* scroll 2 */
    cache = (int32 *)g_pv_cache;
    cache[(2 * 0x30 + 0 * 4) / 4] = 0x3C;    /* char 2, blink 0 */

    fd2_render_party_roster_with_item_stat_preview(1, (uint32)g_pv_cands, 0, 99, 0x1000);

    ASSERT_EQ((long)g_blitpass_calls, 1);
    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_pv_cache + 0x3Cu));
    pv_teardown();
}

/* ----------------------------------------------------------------
 * Blink-frame remap: subframe 3 -> blink 1 (src uses cache[id*0x30 + 1*4]).
 * ---------------------------------------------------------------- */
static void test_preview_blink_frame_3_maps_to_1(void)
{
    int32 *cache;

    pv_setup(0, 3);                          /* subframe 3 -> blink 1 */
    cache = (int32 *)g_pv_cache;
    cache[(0 * 0x30 + 1 * 4) / 4] = 0xAA;    /* blink 1 (expected) */
    cache[(0 * 0x30 + 3 * 4) / 4] = 0xBB;    /* blink 3 (must NOT be used) */

    fd2_render_party_roster_with_item_stat_preview(1, (uint32)g_pv_cands, 0, 99, 0x1000);

    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_pv_cache + 0xAAu));
    pv_teardown();
}

/* blink passthrough: subframe 2 (not 3) used as-is. */
static void test_preview_blink_frame_passthrough(void)
{
    int32 *cache;

    pv_setup(0, 2);
    cache = (int32 *)g_pv_cache;
    cache[(0 * 0x30 + 2 * 4) / 4] = 0x5C;

    fd2_render_party_roster_with_item_stat_preview(1, (uint32)g_pv_cands, 0, 99, 0x1000);

    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_pv_cache + 0x5Cu));
    pv_teardown();
}

/* ----------------------------------------------------------------
 * Name page = char.char_id + 1; name dst = (row_y+4)*0x140 + surf + 0x28;
 * highlighted row (scroll+iter == highlight_idx) gets border 0xC9. One char.
 * ---------------------------------------------------------------- */
static void test_preview_name_page_dst_and_highlight(void)
{
    uint32 surf = 0x4000;
    uint32 row_y = 0x75;                     /* iter 0 */

    pv_setup(0, 0);
    g_pv_chars[0].char_id = 0x0A;            /* page = 0x0B */
    pv_text_glyph_at(0x0B, 0x37);

    fd2_render_party_roster_with_item_stat_preview(1, (uint32)g_pv_cands, 0, 0, surf);

    ASSERT_EQ(g_dlg_glyph_calls, 1);                       /* page 0x0B reached VM */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x37);
    ASSERT_EQ((long)g_dlg_glyph_last_pos,
              (long)((row_y + 4) * 0x140u + surf + 0x28u));
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xC9);            /* highlighted */
    pv_teardown();
}

/* non-highlighted row -> border 0xCD. Two chars, highlight slot 1 (not 0);
 * mark char 0's page so the emitted glyph carries row 0's border. */
static void test_preview_border_not_highlighted(void)
{
    pv_setup(0, 0);
    g_pv_chars[0].char_id = 0x03;            /* page = 0x04 */
    g_pv_chars[1].char_id = 0x07;
    pv_text_glyph_at(0x04, 0x22);            /* only row 0's page emits a glyph */

    fd2_render_party_roster_with_item_stat_preview(2, (uint32)g_pv_cands, 0, 1, 0x4000);

    ASSERT_EQ(g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xCD);            /* row 0 not highlighted */
    pv_teardown();
}

/* ----------------------------------------------------------------
 * Stat-icon blits: 8 per char via the recording fd2_dialog_sprite_blit_normal
 * stub. Pins each icon's atlas sprite (current +0x4E/+0x52/+0x56/+0x5A, preview
 * +0x5E) and its dst. One char, iter 0 (row_y = 0x75), surf 0x6000.
 *
 * dst row bases: b3=surf+(row_y+3)*0x140, b4=+(row_y+4), b12=+(row_y+0xC),
 * b13=+(row_y+0xD).
 * ---------------------------------------------------------------- */
static void test_preview_stat_icon_sprites_and_dsts(void)
{
    uint32 surf = 0x6000;
    uint32 row_y = 0x75;
    uint32 atlas = (uint32)g_pv_atlas;
    uint32 b3, b4, b12, b13;

    pv_setup(0, 0);

    fd2_render_party_roster_with_item_stat_preview(1, (uint32)g_pv_cands, 0, 99, surf);

    b3  = surf + (row_y + 3) * 0x140u;
    b4  = surf + (row_y + 4) * 0x140u;
    b12 = surf + (row_y + 0xc) * 0x140u;
    b13 = surf + (row_y + 0xd) * 0x140u;

    ASSERT_EQ((long)g_dlg_blit_normal_calls, 8);

    /* [0] AP current icon, [1] AP preview icon */
    ASSERT_EQ((long)g_dlg_blit_dst_log[0], (long)(b3 + 0x7au));
    ASSERT_EQ((long)g_dlg_blit_sprite_log[0], (long)(atlas + PV_ATLAS_AP));
    ASSERT_EQ((long)g_dlg_blit_dst_log[1], (long)(b4 + 0x9du));
    ASSERT_EQ((long)g_dlg_blit_sprite_log[1], (long)(atlas + PV_ATLAS_PREV));
    /* [2] DP current icon, [3] DP preview icon */
    ASSERT_EQ((long)g_dlg_blit_dst_log[2], (long)(b12 + 0x7au));
    ASSERT_EQ((long)g_dlg_blit_sprite_log[2], (long)(atlas + PV_ATLAS_DP));
    ASSERT_EQ((long)g_dlg_blit_dst_log[3], (long)(b13 + 0x9du));
    ASSERT_EQ((long)g_dlg_blit_sprite_log[3], (long)(atlas + PV_ATLAS_PREV));
    /* [4] DX current icon, [5] DX preview icon */
    ASSERT_EQ((long)g_dlg_blit_dst_log[4], (long)(b3 + 0xc4u));
    ASSERT_EQ((long)g_dlg_blit_sprite_log[4], (long)(atlas + PV_ATLAS_DX));
    ASSERT_EQ((long)g_dlg_blit_dst_log[5], (long)(b4 + 0xeau));
    ASSERT_EQ((long)g_dlg_blit_sprite_log[5], (long)(atlas + PV_ATLAS_PREV));
    /* [6] Stat4 current icon, [7] Stat4 preview icon */
    ASSERT_EQ((long)g_dlg_blit_dst_log[6], (long)(b12 + 0xc4u));
    ASSERT_EQ((long)g_dlg_blit_sprite_log[6], (long)(atlas + PV_ATLAS_ST4));
    ASSERT_EQ((long)g_dlg_blit_dst_log[7], (long)(b13 + 0xeau));
    ASSERT_EQ((long)g_dlg_blit_sprite_log[7], (long)(atlas + PV_ATLAS_PREV));
    pv_teardown();
}

/* ----------------------------------------------------------------
 * Compare-colour pairing: per char, fd2_pick_stat_compare_color is called once
 * per stat with (current, preview). current = the runtime-char AP/DP/DX/Stat4
 * fields; preview = current-base(0 here) + the candidate item's bonus. Seed the
 * item bonuses and the char's current fields so each stat's pair is distinct.
 * ---------------------------------------------------------------- */
static void test_preview_compare_color_pairs(void)
{
    pv_setup(0, 0);
    g_pv_chars[0].ap = 10;
    g_pv_chars[0].dp = 20;
    g_pv_chars[0].dx_current = 30;
    g_pv_chars[0].stat4_current = 40;
    /* item id 3: ap+5, ht(->dx)+6, dp+7, ev(->stat4)+8; char base stats 0 ->
     * preview = item bonus */
    pv_seed_item(3, 5, 6, 7, 8);

    fd2_render_party_roster_with_item_stat_preview(1, (uint32)g_pv_cands, 3, 99, 0x1000);

    ASSERT_EQ((long)g_pick_color_calls, 4);    /* AP, DP, DX, Stat4 */
    /* AP: current 10 vs preview 5 */
    ASSERT_EQ((long)g_pick_color_cur_log[0], 10);
    ASSERT_EQ((long)g_pick_color_prev_log[0], 5);
    /* DP: current 20 vs preview 7 */
    ASSERT_EQ((long)g_pick_color_cur_log[1], 20);
    ASSERT_EQ((long)g_pick_color_prev_log[1], 7);
    /* DX: current 30 vs preview 6 (item ht -> dx) */
    ASSERT_EQ((long)g_pick_color_cur_log[2], 30);
    ASSERT_EQ((long)g_pick_color_prev_log[2], 6);
    /* Stat4: current 40 vs preview 8 (item ev -> stat4) */
    ASSERT_EQ((long)g_pick_color_cur_log[3], 40);
    ASSERT_EQ((long)g_pick_color_prev_log[3], 8);
    pv_teardown();
}

/* ----------------------------------------------------------------
 * Decimal values + colours + dsts: per stat the SAME colour is used for both
 * the current and preview digits. With the fake sheet (sheet[i]=i) a digit
 * glyph resolves to sheet + (colour + digit). One char; seed AP so current ==
 * preview (colour 0x1F), DP so current < preview (0x2A), DX so current >
 * preview (0x77). Verify the first stat-value blit dst of each value and the
 * 3-digit glyph offsets of the AP pair (which pins value, colour, and path).
 *
 * Decimal-blit order (3 glyphs per value): AP cur, AP prev, DP cur, DP prev,
 * DX cur, DX prev, Stat4 cur, Stat4 prev.
 * ---------------------------------------------------------------- */
static void test_preview_decimal_values_colors_dsts(void)
{
    uint32 surf = 0x8000;
    uint32 row_y = 0x75;
    uint32 b3, b12, sheet;

    pv_setup(0, 0);
    g_pv_chars[0].ap = 10;             /* AP current 10 */
    g_pv_chars[0].dp = 20;             /* DP current 20 */
    g_pv_chars[0].dx_current = 30;     /* DX current 30 */
    g_pv_chars[0].stat4_current = 0;   /* Stat4 current 0 */
    /* item id 1 (char base stats all 0 -> preview == item bonus):
     *   ap +10  -> preview 10 == cur 10  -> 0x1F
     *   dp +25  -> preview 25 >  cur 20  -> 0x2A
     *   ht +5   -> dx preview 5 < cur 30 -> 0x77
     *   ev +0   -> stat4 preview 0 == cur 0 -> 0x1F
     * pv_seed_item args order: (item_id, ap, ht, dp, ev) */
    pv_seed_item(1, 10, 5, 25, 0);

    fd2_render_party_roster_with_item_stat_preview(1, (uint32)g_pv_cands, 1, 99, surf);

    b3    = surf + (row_y + 3) * 0x140u;
    b12   = surf + (row_y + 0xc) * 0x140u;
    sheet = (uint32)g_pv_anim;

    /* 8 values x 3 digits = 24 digit blits */
    ASSERT_EQ((long)g_rle_blit_calls, 24);

    /* AP current value 10 -> "010", colour 0x1F: idx 0x1F+0,0x1F+1,0x1F+0.
     * dst = b3+0x89 (then +6, +12). */
    ASSERT_EQ((long)g_rle_blit_log_dst[0], (long)(b3 + 0x89u));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[0] - sheet), 0x1f + 0);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[1] - sheet), 0x1f + 1);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[2] - sheet), 0x1f + 0);
    /* AP preview value 10 -> "010", same colour 0x1F; dst = b3+0xA5. */
    ASSERT_EQ((long)g_rle_blit_log_dst[3], (long)(b3 + 0xa5u));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[3] - sheet), 0x1f + 0);

    /* DP current value 20 -> "020", colour 0x2A; dst = b12+0x89. */
    ASSERT_EQ((long)g_rle_blit_log_dst[6], (long)(b12 + 0x89u));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[6] - sheet), 0x2a + 0);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[7] - sheet), 0x2a + 2);
    /* DP preview value 25 -> "025", same colour 0x2A; dst = b12+0xA5. */
    ASSERT_EQ((long)g_rle_blit_log_dst[9], (long)(b12 + 0xa5u));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[11] - sheet), 0x2a + 5);

    /* DX current value 30 -> "030", colour 0x77; dst = b3+0xD6. */
    ASSERT_EQ((long)g_rle_blit_log_dst[12], (long)(b3 + 0xd6u));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[13] - sheet), 0x77 + 3);
    /* DX preview value 5 -> "005", same colour 0x77; dst = b3+0xF2. */
    ASSERT_EQ((long)g_rle_blit_log_dst[15], (long)(b3 + 0xf2u));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[17] - sheet), 0x77 + 5);

    /* Stat4 current value 0 -> "000", colour 0x1F; dst = b12+0xD6. */
    ASSERT_EQ((long)g_rle_blit_log_dst[18], (long)(b12 + 0xd6u));
    ASSERT_EQ((long)(g_rle_blit_log_sprite[18] - sheet), 0x1f + 0);
    /* Stat4 preview value 0 -> "000"; dst = b12+0xF2. */
    ASSERT_EQ((long)g_rle_blit_log_dst[21], (long)(b12 + 0xf2u));
    pv_teardown();
}

/* ================================================================
 * fd2_render_save_slot_grid @ 0x30437
 *
 * 4-slot save-file grid. Per slot it renders a "Slot N" header (FDTXT
 * page 0x225) plus either a chapter intro-icon (page chapter_id+0x202)
 * and chapter title (page chapter_id+0x226), or a single "EMPTY"
 * sentinel sprite (page 0x202) when the slot's chapter byte is 0xFF.
 * The selected slot draws in the highlight border glyph (0xC9 vs 0xCD).
 *
 * The decrypted FD2.SAV buffer here is a pure in-memory byte array
 * (the function receives an already-decrypted pointer from its caller
 * and does no fopen). The risk-bearing logic — per-slot loop, border
 * glyph selection, the slot-base address arithmetic (0x312B + iter*0xA28,
 * chapter byte at +0xA00), the row dst arithmetic
 * ((iter*0x13+0x77)*0x140), the empty-vs-chapter branch and its page /
 * position selection, and the per-slot display_slot_number — is pinned
 * by driving the REAL fd2_display_dialog_scene against a text program
 * where exactly one page is aimed at a single-glyph (or literal-number)
 * blob, so a rendered glyph proves the page index reached the VM while
 * g_dlg_glyph_last_pos / g_dlg_glyph_last_p5 capture the dst arithmetic
 * and the border glyph.
 * ================================================================ */

/* SAV header = 0x312B, 4 slots x 0xA28; chapter byte at slot+0xA00.
 * Last read = 0x312B + 3*0xA28 + 0xA00 = 0x5A23, so 0x5B00 covers it. */
#define SAV_HDR_SIZE   0x312Bu
#define SAV_SLOT_SIZE  0x0A28u
#define SAV_CHAP_OFF   0x0A00u
#define SAV_BUF_SIZE   0x5B00u
static uint8 g_sav_buf[SAV_BUF_SIZE];

/* dialog text program for the save-slot grid (mirrors roster_text_*):
 * page-pointer table at words 0..0x3BF, opcode blobs above it. */
static uint16 g_sav_text[0x400];

#define SAV_END_OFF    0x780               /* byte offset of the shared END   */
#define SAV_GLYPH_OFF  0x782               /* byte offset of the 1-glyph blob  */
#define SAV_NUM_OFF    0x788               /* byte offset of the literal-num op */

/* All pages -> END (no glyph). */
static void sav_text_all_end(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        g_sav_text[i] = 0;
    }
    *(int16 *)((uint8 *)g_sav_text + SAV_END_OFF) = -1;        /* shared END */
    for (i = 0; i < 0x3c0; i++) {
        g_sav_text[i] = (uint16)SAV_END_OFF;                  /* default END */
    }
    data_fd2_all_game_text_ptr = (uint32)g_sav_text;
}

/* Aim page `glyph_page` at a [glyph_val][END] blob so exactly that page
 * renders one glyph; its render_pos and p5 capture the call arithmetic. */
static void sav_text_glyph_at(uint32 glyph_page, uint16 glyph_val)
{
    sav_text_all_end();
    *(uint16 *)((uint8 *)g_sav_text + SAV_GLYPH_OFF)     = glyph_val;
    *(int16  *)((uint8 *)g_sav_text + SAV_GLYPH_OFF + 2) = -1;
    g_sav_text[glyph_page] = (uint16)SAV_GLYPH_OFF;
}

/* Aim page `num_page` at a [literal-number(-6)][END] blob so that page
 * renders the digits of data_fd2_dialog_last_action_value_param. */
static void sav_text_number_at(uint32 num_page)
{
    sav_text_all_end();
    *(int16 *)((uint8 *)g_sav_text + SAV_NUM_OFF)     = -6;
    *(int16 *)((uint8 *)g_sav_text + SAV_NUM_OFF + 2) = -1;
    g_sav_text[num_page] = (uint16)SAV_NUM_OFF;
}

/* Set slot `slot`'s stored chapter byte. */
static void sav_set_chapter(uint32 slot, uint8 chapter)
{
    g_sav_buf[SAV_HDR_SIZE + slot * SAV_SLOT_SIZE + SAV_CHAP_OFF] = chapter;
}

/* Common fixture: zeroed SAV buffer, all-END text, glyph counters reset. */
static void sav_setup(void)
{
    memset(g_sav_buf, 0, SAV_BUF_SIZE);
    sav_text_all_end();
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
    g_dlg_glyph_last_pos = 0;
    g_dlg_glyph_last_p5 = 0;
}

/* Expected row base for a slot: surface_offset + (slot*0x13 + 0x77)*0x140. */
static uint32 sav_row_off(uint32 slot, uint32 surf)
{
    return surf + (slot * 0x13u + 0x77u) * 0x140u;
}

/* ----------------------------------------------------------------
 * "Slot N" header: page 0x225 at row_off + 0xA, highlighted slot draws
 * the 0xC9 border glyph. Isolate slot 0 by making it the only slot whose
 * header reaches a glyph is impossible (all 4 use page 0x225), so instead
 * make all 4 chapters EMPTY (0xFF) and aim page 0x225 at the glyph: every
 * slot emits a header glyph; the LAST captured (slot 3) is asserted, and
 * highlight slot 3 -> 0xC9.
 * ---------------------------------------------------------------- */
static void test_sav_header_highlighted_last_slot(void)
{
    uint32 surf = 0x2000;

    sav_setup();
    sav_set_chapter(0, 0xFF);
    sav_set_chapter(1, 0xFF);
    sav_set_chapter(2, 0xFF);
    sav_set_chapter(3, 0xFF);
    sav_text_glyph_at(0x225, 0x41);        /* header page -> one glyph */

    fd2_render_save_slot_grid(3, surf, g_sav_buf);   /* highlight slot 3 */

    /* 4 headers reached the VM (one glyph each) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 4);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x41);
    /* last header = slot 3: pos = row3 + 0xA */
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(sav_row_off(3, surf) + 0xau));
    /* slot 3 highlighted -> 0xC9 */
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xC9);
}

/* Non-highlighted header -> 0xCD. Highlight slot 0; the last header
 * rendered (slot 3) is NOT highlighted. */
static void test_sav_header_not_highlighted(void)
{
    sav_setup();
    sav_set_chapter(0, 0xFF);
    sav_set_chapter(1, 0xFF);
    sav_set_chapter(2, 0xFF);
    sav_set_chapter(3, 0xFF);
    sav_text_glyph_at(0x225, 0x41);

    fd2_render_save_slot_grid(0, 0x2000, g_sav_buf);  /* highlight slot 0 */

    ASSERT_EQ((long)g_dlg_glyph_calls, 4);
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xCD);       /* slot 3 not highlighted */
}

/* ----------------------------------------------------------------
 * display_slot_number @ [0x53AE1] = slot_iter + 1, consumed by the
 * dialog VM literal-number opcode. Aim the header page 0x225 at a
 * literal-number blob; each slot emits one digit (slots 1..4 are all
 * single-digit), and the last (slot 3) renders "4" -> glyph idx 4.
 * ---------------------------------------------------------------- */
static void test_sav_display_slot_number_per_slot(void)
{
    sav_setup();
    sav_set_chapter(0, 0xFF);
    sav_set_chapter(1, 0xFF);
    sav_set_chapter(2, 0xFF);
    sav_set_chapter(3, 0xFF);
    sav_text_number_at(0x225);

    fd2_render_save_slot_grid(0, 0x2000, g_sav_buf);

    /* 4 slots, each a single-digit slot number -> 4 glyph calls */
    ASSERT_EQ((long)g_dlg_glyph_calls, 4);
    /* last slot (iter 3) -> display_slot_number 4 -> digit '4' -> idx 4 */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 4);
    /* the global is left at the final iteration's value */
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, 4);
}

/* ----------------------------------------------------------------
 * Empty slot (chapter byte 0xFF): renders the "EMPTY" sprite (page
 * 0x202) at row_off + 0x58 via the shared tail call, and does NOT render
 * a chapter intro-icon. Isolate slot 2: only slot 2 empty, others use a
 * distinct non-zero chapter so page 0x202 is unique to the empty slot.
 * ---------------------------------------------------------------- */
static void test_sav_empty_slot_renders_empty_sprite(void)
{
    uint32 surf = 0x3000;

    sav_setup();
    sav_set_chapter(0, 0x10);
    sav_set_chapter(1, 0x11);
    sav_set_chapter(2, 0xFF);              /* empty */
    sav_set_chapter(3, 0x12);
    sav_text_glyph_at(0x202, 0x55);        /* EMPTY sprite page */

    fd2_render_save_slot_grid(0, surf, g_sav_buf);

    /* exactly one EMPTY sprite rendered (only slot 2) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x55);
    /* EMPTY sprite dst = row2 + 0x58 */
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(sav_row_off(2, surf) + 0x58u));
}

/* The empty branch must NOT render a chapter intro-icon. With chapter
 * 0xFF the icon page (0xFF would be 0x301, but that page is never used);
 * verify by aiming the glyph at the chapter-title page chapter+0x226 and
 * the icon page chapter+0x202 -> neither fires for an empty slot. Use a
 * single empty slot and confirm no glyph when only those pages carry it. */
static void test_sav_empty_slot_skips_icon_and_title(void)
{
    sav_setup();
    sav_set_chapter(0, 0xFF);
    sav_set_chapter(1, 0xFF);
    sav_set_chapter(2, 0xFF);
    sav_set_chapter(3, 0xFF);
    /* aim glyph at a chapter icon/title page that a non-empty slot would
     * use (e.g. chapter 0x10 -> icon 0x212); empties never reach it. */
    sav_text_glyph_at(0x212, 0x66);

    fd2_render_save_slot_grid(0, 0x2000, g_sav_buf);

    ASSERT_EQ((long)g_dlg_glyph_calls, 0);   /* no icon/title for empties */
}

/* ----------------------------------------------------------------
 * Chapter slot intro-icon: page = chapter_id + 0x202 at row_off + 0x28.
 * Give slot 1 a unique chapter so its icon page is reached by no other
 * slot. ---------------------------------------------------------------- */
static void test_sav_chapter_icon_page_and_pos(void)
{
    uint32 surf = 0x4000;
    uint8  chap = 0x05;

    sav_setup();
    sav_set_chapter(0, 0xFF);
    sav_set_chapter(1, chap);              /* chapter slot */
    sav_set_chapter(2, 0xFF);
    sav_set_chapter(3, 0xFF);
    sav_text_glyph_at((uint32)chap + 0x202, 0x77);   /* intro-icon page */

    fd2_render_save_slot_grid(0, surf, g_sav_buf);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x77);
    /* icon dst = row1 + 0x28 */
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(sav_row_off(1, surf) + 0x28u));
    /* slot 1 not highlighted -> 0xCD */
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xCD);
}

/* Chapter slot title: page = chapter_id + 0x226 at row_off + 0x82,
 * highlighted slot -> 0xC9. Isolate slot 0 with a unique chapter. */
static void test_sav_chapter_title_page_pos_highlight(void)
{
    uint32 surf = 0x4000;
    uint8  chap = 0x07;

    sav_setup();
    sav_set_chapter(0, chap);              /* chapter slot, highlighted */
    sav_set_chapter(1, 0xFF);
    sav_set_chapter(2, 0xFF);
    sav_set_chapter(3, 0xFF);
    sav_text_glyph_at((uint32)chap + 0x226, 0x88);   /* chapter-title page */

    fd2_render_save_slot_grid(0, surf, g_sav_buf);    /* highlight slot 0 */

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x88);
    /* title dst = row0 + 0x82 */
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(sav_row_off(0, surf) + 0x82u));
    /* slot 0 highlighted -> 0xC9 */
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xC9);
}

/* ----------------------------------------------------------------
 * Slot-base address + chapter-byte offset: the chapter byte must be read
 * from 0x312B + slot*0xA28 + 0xA00. Put a unique chapter only at slot 3's
 * exact offset (all others 0xFF) and aim the glyph at slot 3's title
 * page -> the glyph fires only if the byte was read from the right place.
 * ---------------------------------------------------------------- */
static void test_sav_slot_base_and_chapter_offset(void)
{
    uint32 surf = 0x1000;
    uint8  chap = 0x0A;

    sav_setup();
    sav_set_chapter(0, 0xFF);
    sav_set_chapter(1, 0xFF);
    sav_set_chapter(2, 0xFF);
    sav_set_chapter(3, chap);              /* only slot 3 non-empty */
    sav_text_glyph_at((uint32)chap + 0x226, 0x99);   /* slot 3 title page */

    fd2_render_save_slot_grid(0, surf, g_sav_buf);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);            /* read from slot 3's +0xA00 */
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(sav_row_off(3, surf) + 0x82u));
}

/* ----------------------------------------------------------------
 * Row arithmetic across slots: isolate each slot via a unique chapter
 * title page and verify its row base. Covers the (slot*0x13+0x77)*0x140
 * stride for slots 0..3 individually.
 * ---------------------------------------------------------------- */
static void test_sav_row_offset_per_slot(void)
{
    uint32 surf = 0x6000;
    uint32 slot;
    uint8  chap;

    for (slot = 0; slot < 4; slot++) {
        chap = (uint8)(0x20 + slot);       /* distinct chapter per slot */
        sav_setup();
        sav_set_chapter(0, 0xFF);
        sav_set_chapter(1, 0xFF);
        sav_set_chapter(2, 0xFF);
        sav_set_chapter(3, 0xFF);
        sav_set_chapter(slot, chap);
        sav_text_glyph_at((uint32)chap + 0x226, 0x30);  /* this slot's title */

        fd2_render_save_slot_grid(99, surf, g_sav_buf);

        ASSERT_EQ((long)g_dlg_glyph_calls, 1);
        ASSERT_EQ((long)g_dlg_glyph_last_pos,
                  (long)(sav_row_off(slot, surf) + 0x82u));
    }
}

/* ----------------------------------------------------------------
 * Chapter slot renders BOTH icon and title (two dialog draws), while an
 * empty slot renders ONE (the EMPTY sprite). One chapter slot + one empty
 * slot, aiming the glyph at pages shared by the loop is not isolable, so
 * verify the call counts by routing every relevant page to a glyph and
 * counting: a chapter slot contributes icon+title+header = but header
 * page 0x225 is shared. Instead count only the non-header chapter pages.
 * Here: slot 0 chapter 0x0C -> icon 0x20E + title 0x232 both glyph;
 * slot 1 empty -> sprite 0x202 glyph. Total = 3 distinct glyph pages.
 * ---------------------------------------------------------------- */
static void test_sav_chapter_two_draws_empty_one_draw(void)
{
    uint8 chap = 0x0C;

    sav_setup();
    sav_set_chapter(0, chap);              /* chapter -> icon + title */
    sav_set_chapter(1, 0xFF);              /* empty   -> EMPTY sprite  */
    sav_set_chapter(2, 0xFF);
    sav_set_chapter(3, 0xFF);
    /* route icon, title, and EMPTY pages each to a one-glyph blob */
    sav_text_all_end();
    *(uint16 *)((uint8 *)g_sav_text + SAV_GLYPH_OFF)     = 0x10;
    *(int16  *)((uint8 *)g_sav_text + SAV_GLYPH_OFF + 2) = -1;
    g_sav_text[(uint32)chap + 0x202] = (uint16)SAV_GLYPH_OFF;  /* icon  */
    g_sav_text[(uint32)chap + 0x226] = (uint16)SAV_GLYPH_OFF;  /* title */
    g_sav_text[0x202]                = (uint16)SAV_GLYPH_OFF;  /* EMPTY */
    g_dlg_glyph_calls = 0;

    fd2_render_save_slot_grid(0, 0x2000, g_sav_buf);

    /* slot0: icon + title (2); slots 1..3 empty: EMPTY sprite x3 (3) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 5);
}

/* ================================================================
 * fd2_render_promote_members_grid @ 0x30A47
 *
 * Single-column promote/revive candidate grid (up to 3 visible chars).
 * Per char it draws a 24x24 portrait, three FDTXT labels (char name,
 * archetype, job), a coin icon and a 5-digit per-job price. The harness
 * reaches every side-effect through the real pipeline against fakes:
 *   - portrait: real fd2_tile_blit_24x24_with_dialog_bg_fill (g_blitpass_* /
 *     g_blitbgfill_calls) -> pins blink-frame source + row dst arithmetic.
 *   - three labels: the REAL fd2_display_dialog_scene against a text program
 *     where exactly ONE page is aimed at a one-glyph blob, so a rendered glyph
 *     proves that page index reached the VM and g_dlg_glyph_last_pos / _p5
 *     capture the dst arithmetic and the border glyph.
 *   - coin icon: real fd2_blit_sheet_sprite_at_offset -> fd2_blit_sprite_raw_
 *     with_header spy (g_blitraw_log_*); with sheet[i]=i the sprite index is
 *     (logged_sprite - sheet_base).
 *   - price: real fd2_render_decimal_number_to_buffer -> fd2_rle_blit_sprite
 *     spy (g_rle_blit_log_*); the 5 digit glyphs encode the rendered value
 *     (verified via shop_assert_decimal).
 *   - the candidate index list is a plain in-memory byte array passed by the
 *     caller; the runtime-char array is the file-scope g_roster_chars.
 *
 * Risk-bearing logic under test: the visible-count cap (min 3), the blink-frame
 * remap (3->1), char_idx = candidate_idx_list[scroll+iter], the single-column
 * portrait dst/src, the highlight border (0xC9 vs 0xCD), each label's page index
 * (char_id+1 / archetype+0x8C / job+0x96) and dst, the coin icon sprite + dst,
 * and above all the price = level * cost_table[job_id-1] computation (signed
 * int16 table indexed by job_id-1).
 * ================================================================ */

/* candidate index list the grid dereferences via scroll_offset + iter. */
static uint8 g_promo_cands[64];
static runtime_char *g_promo_saved_char_ptr;

/* common promote-grid fixture: N candidates mapped 1:1 to g_roster_chars, scroll
 * offset, blink (subframe) counter, portrait cache + atlas/anim sheets cleared,
 * names default to all-END (override with roster_text_glyph_at). The coin/price
 * spies (g_blitraw_* / g_rle_blit_*) are armed; the cost table is zeroed (each
 * test seeds the entries it exercises). */
static void promo_setup(uint32 scroll, uint32 subframe)
{
    uint8 *anim = (uint8 *)g_shop_anim_sheet;
    uint8 *menu = (uint8 *)g_shop_menu_atlas;
    int    i;

    g_promo_saved_char_ptr = data_fd2_battle_runtime_char_array_ptr;

    memset(g_roster_chars, 0, sizeof(g_roster_chars));
    memset(g_portrait_cache, 0, sizeof(g_portrait_cache));
    for (i = 0; i < 256; i++) {
        *(int32 *)(anim + 6 + i * 4) = i;
        *(int32 *)(menu + 6 + i * 4) = i;
    }
    for (i = 0; i < 64; i++) {
        g_promo_cands[i] = (uint8)i;       /* candidate k -> char k by default */
    }
    for (i = 0; i < 32; i++) {
        data_fd2_ui_per_job_revive_or_promote_cost_table[i] = 0;
    }

    g_shop_anim_base = (uint32)anim;
    g_shop_menu_base = (uint32)menu;
    data_fd2_battle_runtime_char_array_ptr = g_roster_chars;
    portrait_sprite_cache = (uint32)g_portrait_cache;
    /* coin icon is blit from the MENU atlas (param [0x54147]); the price digit
     * glyphs come from the anim sheet ([0x53A81]) inside
     * fd2_render_decimal_number_to_buffer. Both use table[i]=i so a resolved
     * sprite = base + index. */
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = g_shop_menu_base;
    data_fd2_ui_anim_sprite_sheet_ptr = g_shop_anim_base;
    data_fd2_ui_menu_scroll_offset = scroll;
    data_fd2_chapter_intro_dialog_subframe_anim_counter = subframe;

    roster_text_all_end();

    g_blitpass_calls = 0;
    g_blitbgfill_calls = 0;
    g_dlg_glyph_calls = 0;
    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;
}

static void promo_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_promo_saved_char_ptr;
    g_blitraw_log_on = 0;
    g_rle_blit_log_on = 0;
}

/* ----------------------------------------------------------------
 * Visible-count cap: draw_count = min(candidate_count, 3). One portrait
 * bg-fill blit per drawn row.
 * ---------------------------------------------------------------- */
static void test_promo_cap_min_of_count_and_3(void)
{
    promo_setup(0, 0);

    g_blitbgfill_calls = 0;
    fd2_render_promote_members_grid(2, 0x1000, 99, g_promo_cands);
    ASSERT_EQ((long)g_blitbgfill_calls, 2);

    g_blitbgfill_calls = 0;
    fd2_render_promote_members_grid(5, 0x1000, 99, g_promo_cands);
    ASSERT_EQ((long)g_blitbgfill_calls, 3);     /* capped at 3 */

    g_blitbgfill_calls = 0;
    fd2_render_promote_members_grid(0, 0x1000, 99, g_promo_cands);
    ASSERT_EQ((long)g_blitbgfill_calls, 0);     /* nothing to draw */
    promo_teardown();
}

/* ----------------------------------------------------------------
 * Portrait dst (single column) + src. char_idx = cands[scroll+iter];
 * dst = (row_off+0x75)*0x140 + surf + 0xE with row_off = iter*0x1A; src =
 * cache + cache[char_idx*0x30 + blink*4]. scroll 0, blink 0, 2 rows.
 * ---------------------------------------------------------------- */
static void test_promo_portrait_dst_src(void)
{
    uint32 surf = 0x2000;
    int32 *cache;

    promo_setup(0, 0);
    cache = (int32 *)g_portrait_cache;
    cache[(0 * 0x30 + 0 * 4) / 4] = 0x111;      /* char 0, blink 0 */
    cache[(1 * 0x30 + 0 * 4) / 4] = 0x222;      /* char 1, blink 0 */

    fd2_render_promote_members_grid(2, surf, 99, g_promo_cands);

    ASSERT_EQ((long)g_blitpass_calls, 2);
    /* row 0: row_off 0 */
    ASSERT_EQ((long)g_blitpass_dst[0], (long)((0x00u + 0x75u) * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0x111u));
    /* row 1: row_off 0x1A */
    ASSERT_EQ((long)g_blitpass_dst[1], (long)((0x1au + 0x75u) * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_src[1], (long)((uint32)g_portrait_cache + 0x222u));
    ASSERT_EQ((long)g_blitpass_stride[0], 0x140);
    promo_teardown();
}

/* char_idx comes from candidate_idx_list[scroll + iter], NOT scroll+iter
 * directly: scroll 1, and cands[1] -> char 7, so the first drawn portrait
 * indexes char 7's cache row. ---------------------------------------------- */
static void test_promo_char_idx_from_candidate_list(void)
{
    int32 *cache;

    promo_setup(1, 0);                          /* scroll 1 */
    g_promo_cands[1] = 7;                        /* cands[scroll+0] = 7 */
    cache = (int32 *)g_portrait_cache;
    cache[(7 * 0x30 + 0 * 4) / 4] = 0x3C0;       /* char 7, blink 0 */

    fd2_render_promote_members_grid(1, 0x1000, 99, g_promo_cands);

    ASSERT_EQ((long)g_blitpass_calls, 1);
    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0x3C0u));
    promo_teardown();
}

/* ----------------------------------------------------------------
 * Blink-frame remap: subframe 3 -> blink 1 (src uses cache[id*0x30 + 1*4]).
 * ---------------------------------------------------------------- */
static void test_promo_blink_frame_3_maps_to_1(void)
{
    int32 *cache;

    promo_setup(0, 3);                          /* subframe 3 -> blink 1 */
    cache = (int32 *)g_portrait_cache;
    cache[(0 * 0x30 + 1 * 4) / 4] = 0xAA;        /* blink 1 (expected) */
    cache[(0 * 0x30 + 3 * 4) / 4] = 0xBB;        /* blink 3 (must NOT be used) */

    fd2_render_promote_members_grid(1, 0x1000, 99, g_promo_cands);

    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0xAAu));
    promo_teardown();
}

/* blink passthrough: subframe 2 (not 3) used as-is. */
static void test_promo_blink_frame_passthrough(void)
{
    int32 *cache;

    promo_setup(0, 2);
    cache = (int32 *)g_portrait_cache;
    cache[(0 * 0x30 + 2 * 4) / 4] = 0x5C;

    fd2_render_promote_members_grid(1, 0x1000, 99, g_promo_cands);

    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0x5Cu));
    promo_teardown();
}

/* ----------------------------------------------------------------
 * Char-name label: page = char.char_id + 1, dst = text_col + 0x28 where
 * text_col = surf + (row_off+0x79)*0x140; highlighted row (scroll+iter ==
 * highlight_idx) -> border 0xC9. One char, iter 0.
 * ---------------------------------------------------------------- */
static void test_promo_name_page_dst_and_highlight(void)
{
    uint32 surf = 0x4000;
    uint32 text_col = surf + (0x00u + 0x79u) * 0x140u;   /* iter 0 */

    promo_setup(0, 0);
    g_roster_chars[0].char_id = 0x0A;           /* name page = 0x0B */
    roster_text_glyph_at(0x0B, 0x37);           /* only the name page emits a glyph */

    fd2_render_promote_members_grid(1, surf, 0, g_promo_cands);  /* highlight idx 0 */

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);                /* page 0x0B reached VM */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x37);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(text_col + 0x28u));
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xC9);           /* highlighted */
    promo_teardown();
}

/* non-highlighted row -> border 0xCD. Two chars, highlight slot 1 (not 0);
 * mark char 0's name page so the emitted glyph carries row 0's border. */
static void test_promo_border_not_highlighted(void)
{
    promo_setup(0, 0);
    g_roster_chars[0].char_id = 0x03;           /* name page = 0x04 */
    g_roster_chars[1].char_id = 0x07;
    roster_text_glyph_at(0x04, 0x22);           /* only row 0's name emits a glyph */

    fd2_render_promote_members_grid(2, 0x4000, 1, g_promo_cands);  /* highlight slot 1 */

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xCD);           /* row 0 not highlighted */
    promo_teardown();
}

/* ----------------------------------------------------------------
 * Archetype label: page = char.archetype_flag + 0x8C, dst = text_col + 0x82.
 * Aim only that page at the glyph. ---------------------------------------- */
static void test_promo_archetype_page_and_dst(void)
{
    uint32 surf = 0x4000;
    uint32 text_col = surf + (0x00u + 0x79u) * 0x140u;

    promo_setup(0, 0);
    g_roster_chars[0].char_id = 0x00;           /* name page 1 (no glyph there) */
    g_roster_chars[0].archetype_flag = 0x05;    /* archetype page = 0x05 + 0x8C = 0x91 */
    roster_text_glyph_at(0x91, 0x44);

    fd2_render_promote_members_grid(1, surf, 99, g_promo_cands);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);                /* archetype page reached VM */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x44);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(text_col + 0x82u));
    promo_teardown();
}

/* ----------------------------------------------------------------
 * Job label: page = char.job_id + 0x96, dst = text_col + 0xAF. Aim only the
 * job page at the glyph (job_id 4 -> page 0x9A). Guards against confusing the
 * job label with the price (the price reads job_id-1 from the cost table).
 * ---------------------------------------------------------------- */
static void test_promo_job_page_and_dst(void)
{
    uint32 surf = 0x4000;
    uint32 text_col = surf + (0x00u + 0x79u) * 0x140u;

    promo_setup(0, 0);
    g_roster_chars[0].job_id = 0x04;            /* job page = 0x04 + 0x96 = 0x9A */
    roster_text_glyph_at(0x9A, 0x55);

    fd2_render_promote_members_grid(1, surf, 99, g_promo_cands);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);                /* job page reached VM */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x55);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(text_col + 0xafu));
    promo_teardown();
}

/* ----------------------------------------------------------------
 * Coin icon: sprite 0x0F from the MENU atlas, dst = price_y + 0xDC where
 * price_y = surf + (row_off+0x7D)*0x140. job_id 1 keeps the price well-defined
 * (cost[0] is seeded). One char, iter 0.
 * ---------------------------------------------------------------- */
static void test_promo_coin_icon_sprite_and_dst(void)
{
    uint32 surf = 0x3000;
    uint32 price_y = surf + (0x00u + 0x7du) * 0x140u;

    promo_setup(0, 0);
    g_roster_chars[0].job_id = 1;
    g_roster_chars[0].status_flags_block[0] = 1;          /* level 1 */
    data_fd2_ui_per_job_revive_or_promote_cost_table[0] = 100;

    fd2_render_promote_members_grid(1, surf, 99, g_promo_cands);

    /* exactly one sheet blit: the coin icon (labels go through the dialog VM, not
     * the sheet path; the price digits go through the rle path). */
    ASSERT_EQ((long)g_blitraw_count, 1);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - g_shop_menu_base), 0x0F);
    ASSERT_EQ((long)g_blitraw_log_dst[0], (long)(price_y + 0xDCu));
    promo_teardown();
}

/* ----------------------------------------------------------------
 * Price = level * cost_table[job_id - 1], 5 orange (0x77) digits at
 * price_y + 0xE4. The cost table is indexed by job_id-1 (not job_id): seed a
 * decoy at index job_id and the real value at index job_id-1, and prove the
 * rendered value uses the job_id-1 entry. level 7, job_id 5, cost[4] = 300 ->
 * price 2100; cost[5] (decoy) = 9999 must NOT be used.
 * ---------------------------------------------------------------- */
static void test_promo_price_level_times_cost_indexed_by_job_minus_1(void)
{
    uint32 surf = 0x1000;
    uint32 price_y = surf + (0x00u + 0x7du) * 0x140u;

    promo_setup(0, 0);
    g_roster_chars[0].job_id = 5;
    g_roster_chars[0].status_flags_block[0] = 7;          /* level 7 */
    data_fd2_ui_per_job_revive_or_promote_cost_table[4] = 300;   /* job_id-1 = 4 */
    data_fd2_ui_per_job_revive_or_promote_cost_table[5] = 9999;  /* decoy at job_id */

    fd2_render_promote_members_grid(1, surf, 99, g_promo_cands);

    /* 5 price digits via the rle path; value = 7 * 300 = 2100 -> "02100". */
    ASSERT_EQ((long)g_rle_blit_calls, 5);
    shop_assert_decimal(0, price_y + 0xE4u, 2100, 0x77, 5);
    promo_teardown();
}

/* Price with a different level/cost to pin the multiply (not a fixed value):
 * level 12, job_id 2, cost[1] = 150 -> 1800. ----------------------------- */
static void test_promo_price_multiply(void)
{
    uint32 surf = 0x1000;
    uint32 price_y = surf + (0x00u + 0x7du) * 0x140u;

    promo_setup(0, 0);
    g_roster_chars[0].job_id = 2;
    g_roster_chars[0].status_flags_block[0] = 12;         /* level 12 */
    data_fd2_ui_per_job_revive_or_promote_cost_table[1] = 150;   /* job_id-1 = 1 */

    fd2_render_promote_members_grid(1, surf, 99, g_promo_cands);

    shop_assert_decimal(0, price_y + 0xE4u, 1800, 0x77, 5);   /* 12 * 150 */
    promo_teardown();
}

/* ----------------------------------------------------------------
 * Row arithmetic across iters: with 3 candidates the coin-icon dst for iter
 * 0/1/2 pins the row_off = iter*0x1A single-column stride. job_id 1, level 1
 * (cost[0]=1) so each row makes exactly one coin blit (index = iter).
 * ---------------------------------------------------------------- */
static void test_promo_row_offset_per_iter(void)
{
    uint32 surf = 0x6000;
    int    i;

    promo_setup(0, 0);
    for (i = 0; i < 3; i++) {
        g_roster_chars[i].job_id = 1;
        g_roster_chars[i].status_flags_block[0] = 1;
    }
    data_fd2_ui_per_job_revive_or_promote_cost_table[0] = 1;

    fd2_render_promote_members_grid(3, surf, 99, g_promo_cands);

    ASSERT_EQ((long)g_blitraw_count, 3);                  /* one coin per row */
    /* coin dst for iter k = surf + (k*0x1A + 0x7D)*0x140 + 0xDC */
    ASSERT_EQ((long)g_blitraw_log_dst[0],
              (long)(surf + (0x00u + 0x7du) * 0x140u + 0xDCu));
    ASSERT_EQ((long)g_blitraw_log_dst[1],
              (long)(surf + (0x1au + 0x7du) * 0x140u + 0xDCu));
    ASSERT_EQ((long)g_blitraw_log_dst[2],
              (long)(surf + (0x34u + 0x7du) * 0x140u + 0xDCu));
    promo_teardown();
}

/* ================================================================
 * fd2_render_promote_candidates_grid @ 0x31019
 *
 * Class-promotion candidate grid (up to 3 visible chars, single column).
 * Per char it draws a 24x24 portrait, then FOUR FDTXT labels: char name,
 * current job, a "-> 轉職" promotion icon (page 0x251), and the target
 * post-promotion job. Unlike fd2_render_promote_members_grid it takes a 5th
 * argument — a parallel promotion-target class list — and looks the target
 * job up through the REAL fd2_get_class_promotion_data_entry (entry[0] =
 * post-promotion job_id). It draws no coin/price.
 *
 * The harness reaches every side-effect through the real pipeline:
 *   - portrait: real fd2_tile_blit_24x24_with_dialog_bg_fill (g_blitpass_* /
 *     g_blitbgfill_calls) -> pins blink-frame source + row dst arithmetic.
 *   - all four labels: the REAL fd2_display_dialog_scene against a text program
 *     where exactly ONE page is aimed at a one-glyph blob, so a rendered glyph
 *     proves that page index reached the VM and g_dlg_glyph_last_pos / _p5
 *     capture the dst arithmetic and the border glyph.
 *   - target job: the REAL fd2_get_class_promotion_data_entry against the
 *     file-scope data_fd2_class_promotion_data_table (seeded per test), so the
 *     target-list indexing AND the entry[0] dereference are both exercised.
 *
 * Risk-bearing logic under test: the visible-count cap (min 3), the blink-frame
 * remap (3->1), char_idx = candidate_idx_list[scroll+iter], the single-column
 * portrait dst/src, the highlight border (0xC9 vs 0xCD), each label's page index
 * (char_id+1 / job+0x96 / 0x251 / target_job+0x96) and dst, and above all the
 * target-job lookup: promotion_target_list[scroll+iter] -> table entry[0] (the
 * 5th-arg list is read with the SAME scroll+iter index as the candidate list
 * but is an INDEPENDENT array, and only the first byte of the 2-byte entry is
 * used for the page index).
 * ================================================================ */

/* parallel promotion-target class list (5th arg); class ids 0x20..0x33 keep the
 * real fd2_get_class_promotion_data_entry lookup inside the 20-entry table. */
static uint8 g_cand_targets[64];
static runtime_char *g_cand_saved_char_ptr;

/* class-promotion candidate-grid fixture: N candidates mapped 1:1 to
 * g_roster_chars, scroll offset, blink (subframe) counter, portrait cache
 * cleared, names default to all-END (override with roster_text_glyph_at). The
 * candidate index list defaults to identity; the promotion-target list defaults
 * to class 0x20; the class-promotion data table is zeroed (each test seeds the
 * entries it exercises). */
static void cand_setup(uint32 scroll, uint32 subframe)
{
    int i;

    g_cand_saved_char_ptr = data_fd2_battle_runtime_char_array_ptr;

    memset(g_roster_chars, 0, sizeof(g_roster_chars));
    memset(g_portrait_cache, 0, sizeof(g_portrait_cache));
    for (i = 0; i < 64; i++) {
        g_promo_cands[i]  = (uint8)i;          /* candidate k -> char k */
        g_cand_targets[i] = 0x20;              /* default target class 0x20 */
    }
    for (i = 0; i < 20 * 2; i++) {
        data_fd2_class_promotion_data_table[i] = 0;
    }

    data_fd2_battle_runtime_char_array_ptr = g_roster_chars;
    portrait_sprite_cache = (uint32)g_portrait_cache;
    data_fd2_ui_menu_scroll_offset = scroll;
    data_fd2_chapter_intro_dialog_subframe_anim_counter = subframe;

    roster_text_all_end();

    g_blitpass_calls = 0;
    g_blitbgfill_calls = 0;
    g_dlg_glyph_calls = 0;
}

static void cand_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_cand_saved_char_ptr;
}

/* ----------------------------------------------------------------
 * Visible-count cap: draw_count = min(candidate_count, 3). One portrait
 * bg-fill blit per drawn row.
 * ---------------------------------------------------------------- */
static void test_cand_cap_min_of_count_and_3(void)
{
    cand_setup(0, 0);

    g_blitbgfill_calls = 0;
    fd2_render_promote_candidates_grid(2, 0x1000, 99, g_promo_cands,
                                       g_cand_targets);
    ASSERT_EQ((long)g_blitbgfill_calls, 2);

    g_blitbgfill_calls = 0;
    fd2_render_promote_candidates_grid(5, 0x1000, 99, g_promo_cands,
                                       g_cand_targets);
    ASSERT_EQ((long)g_blitbgfill_calls, 3);     /* capped at 3 */

    g_blitbgfill_calls = 0;
    fd2_render_promote_candidates_grid(0, 0x1000, 99, g_promo_cands,
                                       g_cand_targets);
    ASSERT_EQ((long)g_blitbgfill_calls, 0);     /* nothing to draw */
    cand_teardown();
}

/* ----------------------------------------------------------------
 * Portrait dst (single column) + src. char_idx = cands[scroll+iter];
 * dst = (row_off+0x75)*0x140 + surf + 0xE with row_off = iter*0x1A; src =
 * cache + cache[char_idx*0x30 + blink*4]. scroll 0, blink 0, 2 rows.
 * ---------------------------------------------------------------- */
static void test_cand_portrait_dst_src(void)
{
    uint32 surf = 0x2000;
    int32 *cache;

    cand_setup(0, 0);
    cache = (int32 *)g_portrait_cache;
    cache[(0 * 0x30 + 0 * 4) / 4] = 0x111;      /* char 0, blink 0 */
    cache[(1 * 0x30 + 0 * 4) / 4] = 0x222;      /* char 1, blink 0 */

    fd2_render_promote_candidates_grid(2, surf, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_blitpass_calls, 2);
    /* row 0: row_off 0 */
    ASSERT_EQ((long)g_blitpass_dst[0], (long)((0x00u + 0x75u) * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0x111u));
    /* row 1: row_off 0x1A */
    ASSERT_EQ((long)g_blitpass_dst[1], (long)((0x1au + 0x75u) * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_src[1], (long)((uint32)g_portrait_cache + 0x222u));
    ASSERT_EQ((long)g_blitpass_stride[0], 0x140);
    cand_teardown();
}

/* char_idx comes from candidate_idx_list[scroll + iter], NOT scroll+iter
 * directly: scroll 1, and cands[1] -> char 7, so the first drawn portrait
 * indexes char 7's cache row. ---------------------------------------------- */
static void test_cand_char_idx_from_candidate_list(void)
{
    int32 *cache;

    cand_setup(1, 0);                           /* scroll 1 */
    g_promo_cands[1] = 7;                        /* cands[scroll+0] = 7 */
    cache = (int32 *)g_portrait_cache;
    cache[(7 * 0x30 + 0 * 4) / 4] = 0x3C0;       /* char 7, blink 0 */

    fd2_render_promote_candidates_grid(1, 0x1000, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_blitpass_calls, 1);
    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0x3C0u));
    cand_teardown();
}

/* ----------------------------------------------------------------
 * Blink-frame remap: subframe 3 -> blink 1 (src uses cache[id*0x30 + 1*4]).
 * ---------------------------------------------------------------- */
static void test_cand_blink_frame_3_maps_to_1(void)
{
    int32 *cache;

    cand_setup(0, 3);                           /* subframe 3 -> blink 1 */
    cache = (int32 *)g_portrait_cache;
    cache[(0 * 0x30 + 1 * 4) / 4] = 0xAA;        /* blink 1 (expected) */
    cache[(0 * 0x30 + 3 * 4) / 4] = 0xBB;        /* blink 3 (must NOT be used) */

    fd2_render_promote_candidates_grid(1, 0x1000, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0xAAu));
    cand_teardown();
}

/* blink passthrough: subframe 2 (not 3) used as-is. */
static void test_cand_blink_frame_passthrough(void)
{
    int32 *cache;

    cand_setup(0, 2);
    cache = (int32 *)g_portrait_cache;
    cache[(0 * 0x30 + 2 * 4) / 4] = 0x5C;

    fd2_render_promote_candidates_grid(1, 0x1000, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0x5Cu));
    cand_teardown();
}

/* ----------------------------------------------------------------
 * Char-name label: page = char.char_id + 1, dst = text_col + 0x28 where
 * text_col = surf + (row_off+0x79)*0x140; highlighted row (scroll+iter ==
 * highlight_idx) -> border 0xC9. One char, iter 0.
 * ---------------------------------------------------------------- */
static void test_cand_name_page_dst_and_highlight(void)
{
    uint32 surf = 0x4000;
    uint32 text_col = surf + (0x00u + 0x79u) * 0x140u;   /* iter 0 */

    cand_setup(0, 0);
    g_roster_chars[0].char_id = 0x0A;           /* name page = 0x0B */
    roster_text_glyph_at(0x0B, 0x37);           /* only the name page emits a glyph */

    fd2_render_promote_candidates_grid(1, surf, 0, g_promo_cands,
                                       g_cand_targets);  /* highlight idx 0 */

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);                /* page 0x0B reached VM */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x37);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(text_col + 0x28u));
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xC9);           /* highlighted */
    cand_teardown();
}

/* non-highlighted row -> border 0xCD. Two chars, highlight slot 1 (not 0);
 * mark char 0's name page so the emitted glyph carries row 0's border. */
static void test_cand_border_not_highlighted(void)
{
    cand_setup(0, 0);
    g_roster_chars[0].char_id = 0x03;           /* name page = 0x04 */
    g_roster_chars[1].char_id = 0x07;
    roster_text_glyph_at(0x04, 0x22);           /* only row 0's name emits a glyph */

    fd2_render_promote_candidates_grid(2, 0x4000, 1, g_promo_cands,
                                       g_cand_targets);  /* highlight slot 1 */

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xCD);           /* row 0 not highlighted */
    cand_teardown();
}

/* ----------------------------------------------------------------
 * Current-job label: page = char.job_id + 0x96, dst = text_col + 0x82. Aim
 * only the current-job page at the glyph (job_id 4 -> page 0x9A). The target
 * class defaults to 0x20 whose entry[0] is 0, so its label page would be 0x96
 * (no glyph), keeping this assertion specific to the CURRENT job.
 * ---------------------------------------------------------------- */
static void test_cand_current_job_page_and_dst(void)
{
    uint32 surf = 0x4000;
    uint32 text_col = surf + (0x00u + 0x79u) * 0x140u;

    cand_setup(0, 0);
    g_roster_chars[0].job_id = 0x04;            /* current-job page = 0x9A */
    roster_text_glyph_at(0x9A, 0x55);

    fd2_render_promote_candidates_grid(1, surf, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);                /* current-job page reached VM */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x55);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(text_col + 0x82u));
    cand_teardown();
}

/* ----------------------------------------------------------------
 * Promotion icon label: fixed page 0x251, dst = text_col + 0xAF. Aim that page
 * at the glyph; no char field feeds it (it is a constant "-> 轉職" sprite id).
 * ---------------------------------------------------------------- */
static void test_cand_promotion_icon_page_and_dst(void)
{
    uint32 surf = 0x4000;
    uint32 text_col = surf + (0x00u + 0x79u) * 0x140u;

    cand_setup(0, 0);
    roster_text_glyph_at(0x251, 0x66);          /* the fixed promotion-icon page */

    fd2_render_promote_candidates_grid(1, surf, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);                /* page 0x251 reached VM */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x66);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(text_col + 0xafu));
    cand_teardown();
}

/* ----------------------------------------------------------------
 * Target-job label: page = promo_entry[0] + 0x96, dst = text_col + 0xEF, where
 * promo_entry = fd2_get_class_promotion_data_entry(targets[scroll+iter]). Seed
 * the real promotion table so target class 0x25 -> entry[0] = 0x07 -> page
 * 0x9D, and aim only that page at the glyph. A decoy in the entry's SECOND byte
 * proves only the first byte feeds the page.
 * ---------------------------------------------------------------- */
static void test_cand_target_job_page_dst_via_real_table(void)
{
    uint32 surf = 0x4000;
    uint32 text_col = surf + (0x00u + 0x79u) * 0x140u;

    cand_setup(0, 0);
    g_cand_targets[0] = 0x25;                    /* target class 0x25 */
    /* class 0x25 -> table index (0x25-0x20)*2 = 10 */
    data_fd2_class_promotion_data_table[10] = 0x07;   /* entry[0] -> page 0x9D */
    data_fd2_class_promotion_data_table[11] = 0x40;   /* entry[1] decoy (spell id) */
    roster_text_glyph_at(0x9D, 0x71);            /* only the target-job page glyphs */

    fd2_render_promote_candidates_grid(1, surf, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);                /* target-job page reached VM */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x71);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(text_col + 0xefu));
    cand_teardown();
}

/* The promotion-target list is read with scroll+iter (same index as the
 * candidate list) but is an INDEPENDENT array: scroll 1, targets[1] -> class
 * 0x28 whose entry[0] feeds the target-job page. cands[1] != targets[1] so a
 * mix-up would resolve the wrong class. -------------------------------------- */
static void test_cand_target_list_indexed_by_scroll(void)
{
    uint32 surf = 0x1000;
    uint32 text_col = surf + (0x00u + 0x79u) * 0x140u;

    cand_setup(1, 0);                            /* scroll 1 */
    g_promo_cands[1]  = 7;                        /* candidate (char) index, unrelated */
    g_cand_targets[1] = 0x28;                     /* target class for slot scroll+0 */
    /* class 0x28 -> table index (0x28-0x20)*2 = 16 */
    data_fd2_class_promotion_data_table[16] = 0x12;   /* entry[0] -> page 0xA8 */
    roster_text_glyph_at(0xA8, 0x4D);

    fd2_render_promote_candidates_grid(1, surf, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);                /* targets[1] resolved class 0x28 */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, 0x4D);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(text_col + 0xefu));
    cand_teardown();
}

/* ----------------------------------------------------------------
 * Row arithmetic across iters: with 3 candidates the portrait dst for iter
 * 0/1/2 pins the row_off = iter*0x1A single-column stride (one bg-fill blit per
 * row). Each row's portrait src also confirms char_idx = iter (identity list).
 * ---------------------------------------------------------------- */
static void test_cand_row_offset_per_iter(void)
{
    uint32 surf = 0x6000;
    int32 *cache;
    int    i;

    cand_setup(0, 0);
    cache = (int32 *)g_portrait_cache;
    for (i = 0; i < 3; i++) {
        cache[(i * 0x30 + 0 * 4) / 4] = 0x10 + i;     /* distinct src per char */
    }

    fd2_render_promote_candidates_grid(3, surf, 99, g_promo_cands,
                                       g_cand_targets);

    ASSERT_EQ((long)g_blitpass_calls, 3);                 /* one portrait per row */
    ASSERT_EQ((long)g_blitpass_dst[0],
              (long)((0x00u + 0x75u) * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_dst[1],
              (long)((0x1au + 0x75u) * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_dst[2],
              (long)((0x34u + 0x75u) * 0x140u + surf + 0xeu));
    ASSERT_EQ((long)g_blitpass_src[0], (long)((uint32)g_portrait_cache + 0x10u));
    ASSERT_EQ((long)g_blitpass_src[1], (long)((uint32)g_portrait_cache + 0x11u));
    ASSERT_EQ((long)g_blitpass_src[2], (long)((uint32)g_portrait_cache + 0x12u));
    cand_teardown();
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
    RUN_TEST(test_cap_small_count_draws_all);
    RUN_TEST(test_cap_large_count_draws_six);
    RUN_TEST(test_cap_tail_clamps_to_five);
    RUN_TEST(test_weapon_stat_and_icons);
    RUN_TEST(test_armor_stat_and_icons);
    RUN_TEST(test_hp_consumable_stat_and_icons);
    RUN_TEST(test_mp_consumable_stat_and_icons);
    RUN_TEST(test_placeholder_when_no_stat);
    RUN_TEST(test_price_full_when_not_sell);
    RUN_TEST(test_price_discounted_when_sell);
    RUN_TEST(test_price_discount_rounds_toward_zero);
    RUN_TEST(test_column_row_offsets);
    RUN_TEST(test_roster_cap_small_draws_all);
    RUN_TEST(test_roster_cap_large_draws_six);
    RUN_TEST(test_roster_cap_tail_clamps_to_five);
    RUN_TEST(test_roster_portrait_col_row_offsets);
    RUN_TEST(test_roster_portrait_src_uses_scroll_and_blink);
    RUN_TEST(test_roster_blink_frame_3_maps_to_1);
    RUN_TEST(test_roster_blink_frame_passthrough);
    RUN_TEST(test_roster_name_page_dst_and_highlight_border);
    RUN_TEST(test_roster_border_not_highlighted);
    RUN_TEST(test_preview_cap_min_of_count_and_3);
    RUN_TEST(test_preview_portrait_dst_src);
    RUN_TEST(test_preview_char_idx_uses_scroll);
    RUN_TEST(test_preview_blink_frame_3_maps_to_1);
    RUN_TEST(test_preview_blink_frame_passthrough);
    RUN_TEST(test_preview_name_page_dst_and_highlight);
    RUN_TEST(test_preview_border_not_highlighted);
    RUN_TEST(test_preview_stat_icon_sprites_and_dsts);
    RUN_TEST(test_preview_compare_color_pairs);
    RUN_TEST(test_preview_decimal_values_colors_dsts);
    RUN_TEST(test_sav_header_highlighted_last_slot);
    RUN_TEST(test_sav_header_not_highlighted);
    RUN_TEST(test_sav_display_slot_number_per_slot);
    RUN_TEST(test_sav_empty_slot_renders_empty_sprite);
    RUN_TEST(test_sav_empty_slot_skips_icon_and_title);
    RUN_TEST(test_sav_chapter_icon_page_and_pos);
    RUN_TEST(test_sav_chapter_title_page_pos_highlight);
    RUN_TEST(test_sav_slot_base_and_chapter_offset);
    RUN_TEST(test_sav_row_offset_per_slot);
    RUN_TEST(test_sav_chapter_two_draws_empty_one_draw);
    RUN_TEST(test_promo_cap_min_of_count_and_3);
    RUN_TEST(test_promo_portrait_dst_src);
    RUN_TEST(test_promo_char_idx_from_candidate_list);
    RUN_TEST(test_promo_blink_frame_3_maps_to_1);
    RUN_TEST(test_promo_blink_frame_passthrough);
    RUN_TEST(test_promo_name_page_dst_and_highlight);
    RUN_TEST(test_promo_border_not_highlighted);
    RUN_TEST(test_promo_archetype_page_and_dst);
    RUN_TEST(test_promo_job_page_and_dst);
    RUN_TEST(test_promo_coin_icon_sprite_and_dst);
    RUN_TEST(test_promo_price_level_times_cost_indexed_by_job_minus_1);
    RUN_TEST(test_promo_price_multiply);
    RUN_TEST(test_promo_row_offset_per_iter);
    RUN_TEST(test_cand_cap_min_of_count_and_3);
    RUN_TEST(test_cand_portrait_dst_src);
    RUN_TEST(test_cand_char_idx_from_candidate_list);
    RUN_TEST(test_cand_blink_frame_3_maps_to_1);
    RUN_TEST(test_cand_blink_frame_passthrough);
    RUN_TEST(test_cand_name_page_dst_and_highlight);
    RUN_TEST(test_cand_border_not_highlighted);
    RUN_TEST(test_cand_current_job_page_and_dst);
    RUN_TEST(test_cand_promotion_icon_page_and_dst);
    RUN_TEST(test_cand_target_job_page_dst_via_real_table);
    RUN_TEST(test_cand_target_list_indexed_by_scroll);
    RUN_TEST(test_cand_row_offset_per_iter);
    SUITE_END();
}
