/*
 * unit tests for src/dialog/dialog.c
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* recording stub for fd2_restore_screen_block_from_buffer (testglob.c) */
extern int    g_restore_block_calls;
extern uint32 g_restore_block_last_buf;
extern uint32 g_restore_block_last_dst;
extern uint32 g_restore_block_last_stride;

/*
 * fd2_cleanup_dialog_sprite_buffer forwards (saved_block, dst, stride) to
 * fd2_restore_screen_block_from_buffer in that exact order, then frees
 * saved_block. Assert the three args land in the right slots (guards against
 * a swapped argument order) and that the restore happens exactly once.
 */
static void test_cleanup_forwards_args(void)
{
    void *blk;

    blk = malloc(64);
    ASSERT_TRUE(blk != NULL);

    g_restore_block_calls = 0;
    g_restore_block_last_buf = 0;
    g_restore_block_last_dst = 0;
    g_restore_block_last_stride = 0;

    fd2_cleanup_dialog_sprite_buffer((uint32)blk, 0xA0000u, 320u);

    ASSERT_EQ((long)g_restore_block_calls, 1);
    ASSERT_EQ((long)g_restore_block_last_buf, (long)(uint32)blk);
    ASSERT_EQ((long)g_restore_block_last_dst, (long)0xA0000u);
    ASSERT_EQ((long)g_restore_block_last_stride, (long)320u);
}

/*
 * The buffer passed as saved_block must be returned to the heap. malloc a
 * block, run cleanup (which frees it), then malloc the same size again: a
 * correctly-freed block is reused by the allocator, so we get the identical
 * address back. (If cleanup leaked the block, the second malloc would have to
 * carve fresh storage and return a different address.)
 */
static void test_cleanup_frees_buffer(void)
{
    void *blk;
    void *again;

    blk = malloc(128);
    ASSERT_TRUE(blk != NULL);

    fd2_cleanup_dialog_sprite_buffer((uint32)blk, 0xA0000u, 320u);

    again = malloc(128);
    ASSERT_EQ((long)(uint32)again, (long)(uint32)blk);
    free(again);
}

/* ---- fd2_display_dialog_scene (dialog VM) ----
 * recording state from testglob.c's glyph/blink stubs. */
extern int    g_dlg_glyph_calls;
extern uint32 g_dlg_glyph_last_idx;
extern uint32 g_dlg_glyph_last_pos;
extern int    g_dlg_blink_calls;

/* Reset VM-visible state for a deterministic run: empty BIOS keyboard buffer
 * (head==tail) so the real fd2_check_keyboard_buffer_nonempty() polls 0, and
 * clear the glyph/blink recorders + the active-portrait latch. */
static void dlg_reset(void)
{
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;
    g_dlg_glyph_last_pos = 0;
    g_dlg_blink_calls = 0;
    data_fd2_dialog_active_portrait_blit_offset = 0;
}

/*
 * TEXT glyphs advance render_pos by 0x10 each and END (-1) returns the final
 * render_pos. Stream header word [0]=2 redirects cur_op to the opcode body
 * (text_base + 2). Three glyphs then END: each glyph is blitted at the running
 * position and the returned value is start + 3*0x10. With an empty keyboard
 * buffer blink_flag stays set, so the blink step runs once per glyph.
 */
static void test_text_glyph_advance_and_end(void)
{
    int16  prog[8];
    uint32 ret;

    dlg_reset();
    prog[0] = 2;          /* offset to first opcode */
    prog[1] = 0x41;       /* glyph */
    prog[2] = 0x42;       /* glyph */
    prog[3] = 0x43;       /* glyph */
    prog[4] = -1;         /* END */

    ret = fd2_display_dialog_scene((uint32)prog, 0, 0x1000u, 320u,
                                   0xcd, 0x4c, 0x4a, 0x13, 1);

    ASSERT_EQ((long)g_dlg_glyph_calls, 3);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x43);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)(0x1000u + 0x20u)); /* 3rd glyph pos */
    ASSERT_EQ((long)ret, (long)(0x1000u + 0x30u));
    ASSERT_EQ((long)g_dlg_blink_calls, 3);
}

/*
 * If the keyboard buffer is non-empty at entry, the real keyboard poll returns
 * nonzero after the first glyph, clearing blink_flag so the blink step is NOT
 * called for the remaining glyphs. Glyph blitting and render_pos advance are
 * unaffected.
 */
static void test_keypress_suppresses_blink(void)
{
    int16  prog[8];
    uint32 ret;

    dlg_reset();
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x22;   /* head != tail -> input pending */
    prog[0] = 2;
    prog[1] = 0x41;
    prog[2] = 0x42;
    prog[3] = -1;

    ret = fd2_display_dialog_scene((uint32)prog, 0, 0x1000u, 320u,
                                   0xcd, 0x4c, 0x4a, 0x13, 1);

    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)ret, (long)(0x1000u + 0x20u));
    ASSERT_EQ((long)g_dlg_blink_calls, 0);  /* poll cleared blink_flag immediately */
}

/*
 * LINE ADVANCE (-2) is the silent newline: it bumps line_count and recomputes
 * render_pos = render_base + render_pitch*glyph_height*line_count, with no wait
 * for input (unlike PAGE BREAK -3). render_base is the (unmutated) initial
 * render_pos arg. With pitch=320, height=0x13, one glyph then one -2 then one
 * glyph: first glyph at start; after -2 the row jumps to start + 320*0x13*1; the
 * trailing glyph blits there and the return adds 0x10. (-3 is NOT used here
 * because it invokes the real busy-wait fd2_wait_for_input_dialog_with_blink.)
 */
static void test_line_advance_arithmetic(void)
{
    int16  prog[8];
    uint32 ret;
    uint32 row1;

    dlg_reset();
    prog[0] = 2;
    prog[1] = 0x41;       /* glyph at start */
    prog[2] = -2;         /* LINE ADVANCE */
    prog[3] = 0x42;       /* glyph at new row */
    prog[4] = -1;

    ret = fd2_display_dialog_scene((uint32)prog, 0, 0x1000u, 320u,
                                   0xcd, 0x4c, 0x4a, 0x13, 1);

    row1 = 0x1000u + 320u * 0x13u * 1u;
    ASSERT_EQ((long)g_dlg_glyph_calls, 2);
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)row1);      /* 2nd glyph at new row */
    ASSERT_EQ((long)ret, (long)(row1 + 0x10u));
}

/*
 * Two consecutive LINE ADVANCE ops increment line_count to 2, so the second row
 * is render_base + pitch*height*2. Confirms line_count accumulates across -2 ops.
 */
static void test_line_advance_count_accumulates(void)
{
    int16  prog[8];
    uint32 ret;
    uint32 row2;

    dlg_reset();
    prog[0] = 2;
    prog[1] = -2;
    prog[2] = -2;
    prog[3] = 0x41;
    prog[4] = -1;

    ret = fd2_display_dialog_scene((uint32)prog, 0, 0x2000u, 320u,
                                   0xcd, 0x4c, 0x4a, 0x13, 1);

    row2 = 0x2000u + 320u * 0x13u * 2u;
    ASSERT_EQ((long)g_dlg_glyph_last_pos, (long)row2);
    ASSERT_EQ((long)ret, (long)(row2 + 0x10u));
}

/*
 * LITERAL NUMBER (-6): sprintf("%d", last_action_value) then blit each digit,
 * advancing render_pos by 0x10 per digit. The operand comes from the global
 * data_fd2_dialog_last_action_value_param, not the opcode stream; the -6 opcode
 * itself consumes just one word. Value 425 -> 3 digits, blitted as ('4'-0x30)=4
 * etc., total advance 3*0x10. Checks digit count, last digit index, return.
 */
static void test_literal_number_digits(void)
{
    int16  prog[8];
    uint32 ret;

    dlg_reset();
    data_fd2_dialog_last_action_value_param = 425;
    prog[0] = 2;
    prog[1] = -6;         /* literal number */
    prog[2] = -1;

    ret = fd2_display_dialog_scene((uint32)prog, 0, 0x1000u, 320u,
                                   0xcd, 0x4c, 0x4a, 0x13, 1);

    ASSERT_EQ((long)g_dlg_glyph_calls, 3);          /* "425" -> 3 digits */
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)5); /* '5' - 0x30 */
    ASSERT_EQ((long)ret, (long)(0x1000u + 0x30u));
}

/*
 * page_idx selects which header word redirects to the opcode body, so two
 * different starting pages can share one buffer. Header[1] points at a 1-glyph
 * page; entering with page_idx=1 renders exactly that page.
 */
static void test_page_idx_selects_start(void)
{
    int16  prog[12];
    uint32 ret;

    dlg_reset();
    /* header words 0 and 1; page 1 body starts at byte offset 8 (word index 4) */
    prog[0] = 4;          /* page 0 -> word index 2 */
    prog[1] = 8;          /* page 1 -> word index 4 */
    prog[2] = 0x41;       /* page 0 body: 2 glyphs */
    prog[3] = 0x42;
    prog[4] = 0x55;       /* page 1 body: 1 glyph */
    prog[5] = -1;

    ret = fd2_display_dialog_scene((uint32)prog, 1, 0x1000u, 320u,
                                   0xcd, 0x4c, 0x4a, 0x13, 1);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x55);
    ASSERT_EQ((long)ret, (long)(0x1000u + 0x10u));
}

/*
 * fd2_portrait_blink_animation_step state machine. A 2-call subtick divider
 * gates the visible frame update; the internal frame counter advances
 * 0->1->2->3->0 on every 2nd call, with 3 collapsed to 1 when painted, so the
 * painted frames cycle 1,2,1,0 over 8 calls (one paint per 2 calls). Each call
 * fires the typewriter SFX exactly once (tracked by g_dlg_blink_calls).
 */
extern uint32 data_fd2_dialog_portrait_blink_frame_idx;
extern uint32 data_fd2_dialog_portrait_blink_subtick_counter;
/* spies for the real fd2_paint_portrait_to_dialog_area's blit primitive:
 * with active offset 0 (!= 0x9017) it takes the normal-blit path, and the
 * sprite payload address it forwards is buffer + offset_table[frame]. We
 * set offset_table[i] = i so the captured sprite offset == the frame index. */
extern int    g_dlg_blit_normal_calls;
extern uint32 g_dlg_blit_last_sprite;

static int32 g_blink_portrait_buf[16];

static uint32 blink_painted_frame(void)
{
    return g_dlg_blit_last_sprite - (uint32)(uint8 *)g_blink_portrait_buf;
}

static void test_blink_frame_cycle(void)
{
    int i;

    dlg_reset();
    for (i = 0; i < 16; i++) {
        g_blink_portrait_buf[i] = i;   /* offset_table[frame] == frame */
    }
    data_fd2_portrait_sprite_buffer = (uint8 *)g_blink_portrait_buf;
    data_fd2_dialog_portrait_blink_frame_idx = 0;
    data_fd2_dialog_portrait_blink_subtick_counter = 0;
    g_dlg_blit_normal_calls = 0;
    g_dlg_blink_calls = 0;

    /* Call 1: subtick 0->1, no paint */
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_subtick_counter, 1);
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 0);

    /* Call 2: subtick hits 2 -> frame 0->1, paint(1), subtick reset */
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_subtick_counter, 0);
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_frame_idx, 1);
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 1);
    ASSERT_EQ((long)blink_painted_frame(), 1);

    /* Calls 3-4: frame 1->2, paint(2) */
    fd2_portrait_blink_animation_step();
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_frame_idx, 2);
    ASSERT_EQ((long)blink_painted_frame(), 2);

    /* Calls 5-6: frame 2->3, painted value collapses 3 -> 1 */
    fd2_portrait_blink_animation_step();
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_frame_idx, 3);
    ASSERT_EQ((long)blink_painted_frame(), 1);

    /* Calls 7-8: frame 3->4 wraps to 0, paint(0) */
    fd2_portrait_blink_animation_step();
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_frame_idx, 0);
    ASSERT_EQ((long)blink_painted_frame(), 0);

    /* 8 calls -> 4 paints, and SFX fires once per call */
    ASSERT_EQ((long)g_dlg_blit_normal_calls, 4);
    ASSERT_EQ((long)g_dlg_blink_calls, 8);
}

/* ---- fd2_play_dialog_open_animation (dialog-open / 5-stage frame) ----
 * These drive the flip==0 path, which skips the cursor pan + the
 * sprite-interpolation loop and exercises the deterministic body:
 * default-origin selection from the portrait mode, the 5 frame-layer
 * malloc()s, the width = flip*0x140+5 arithmetic, and the fixed
 * 5-stage assemble table. fd2_assemble_dialog_frame_layered is now real
 * (src/dialog/dialog.c); each stage is observed through the raw-blit log
 * (g_blitraw_*) below. fd2_save_screen_block_to_buffer is stubbed in
 * testglob.c (g_saveblk_*); the save-buffer array global lives there too. */
extern void  *data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[5];
extern int    g_saveblk_calls;
extern uint32 g_saveblk_out, g_saveblk_w, g_saveblk_h, g_saveblk_dst,
              g_saveblk_src, g_saveblk_stride;

/* raw-blit recording log (testglob.c): every fd2_blit_sprite_raw_with_header
 * call appends (dst, sprite_addr) when g_blitraw_log_on is set. */
extern int    g_blitraw_log_on;
extern int    g_blitraw_count;
extern uint32 g_blitraw_log_dst[512];
extern uint32 g_blitraw_log_sprite[512];

/* A fake sprite atlas so the real fd2_blit_sheet_sprite_at_offset can resolve
 * a sprite without touching real game data. Its 4-byte offset-table entry for
 * index i stores the value i, so the resolved sprite address minus the sheet
 * base equals the sprite index, letting tests read the index back from the
 * blit log. Layout: header (6 bytes) then int32 table[idx]. */
static uint8 g_fake_sheet[256];

static void install_fake_sheet(void)
{
    int i;
    memset(g_fake_sheet, 0, sizeof(g_fake_sheet));
    for (i = 0; i <= 0x11; i++) {
        *(int32 *)(g_fake_sheet + 6 + i * 4) = i;
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_fake_sheet;
}

/* sprite index of the k-th logged blit = sprite_addr - sheet_base */
static uint32 logged_sprite_idx(int k)
{
    return g_blitraw_log_sprite[k] - data_fd2_ui_anim_sprite_sheet_ptr;
}

/* Reset the open-animation recorders and clear the save-buffer array so
 * leaked addresses from a prior run can't masquerade as fresh mallocs. */
static void open_anim_reset(uint32 portrait_mode)
{
    int i;
    g_saveblk_calls = 0;
    install_fake_sheet();
    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    for (i = 0; i < 5; i++) {
        data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[i] = NULL;
    }
    data_fd2_dialog_active_portrait_blit_offset = portrait_mode;
}

/* blits emitted by one fd2_assemble_dialog_frame_layered(n_cols,n_rows) call:
 *   12 fixed corners/mid-edges
 *   + 2*(n_cols-2)  top/bottom inner edges
 *   + 2*(n_rows-2)  left/right edges
 *   + n_cols*n_rows interior fill                                            */
static int assemble_blit_count(int n_cols, int n_rows)
{
    return 12 + 2 * (n_cols - 2) + 2 * (n_rows - 2) + n_cols * n_rows;
}

/* The 5-stage table fd2_play_dialog_open_animation feeds to the (now real)
 * frame assembler. Each stage k uses (n_cols,n_rows) from the table below and
 * the same (dst=0xA0000, pitch=0x140, col_offset=5, row_offset=flip), so its
 * first blit (sprite 1) lands at topleft = 0xA0000 + 5 + flip*0x140 and it
 * emits assemble_blit_count(n_cols,n_rows) blits. Verifies: exactly 5 stage
 * starts (sprite index 1) at the expected topleft (flip/row_offset
 * propagation), the exact total blit count (a strict fingerprint of the
 * (n_cols,n_rows) table), and the 5 save bands with their fixed geometry. */
static void assert_five_stage_frame(uint32 expect_origin, uint32 expect_width)
{
    static const int tbl_cols[5] = { 4, 8, 0xc, 0x10, 0x13 };
    static const int tbl_rows[5] = { 2, 3, 4,   5,    5    };
    uint32 expect_topleft;
    int    expect_total;
    int    starts;
    int    k;

    ASSERT_EQ((long)g_saveblk_calls, 5);

    expect_topleft = 0xA0000u + 5u + expect_origin * 0x140u;
    expect_total = 0;
    for (k = 0; k < 5; k++) {
        expect_total += assemble_blit_count(tbl_cols[k], tbl_rows[k]);
    }
    ASSERT_EQ((long)g_blitraw_count, (long)expect_total);

    /* count stage starts (sprite index 1) and confirm each lands at topleft */
    starts = 0;
    for (k = 0; k < g_blitraw_count; k++) {
        if (logged_sprite_idx(k) == 1) {
            ASSERT_EQ((long)g_blitraw_log_dst[k], (long)expect_topleft);
            starts++;
        }
    }
    ASSERT_EQ((long)starts, 5);

    /* last save call's captured geometry (all 5 use the same constants) */
    ASSERT_EQ((long)g_saveblk_w, (long)0x136);
    ASSERT_EQ((long)g_saveblk_h, (long)0x56);
    ASSERT_EQ((long)g_saveblk_dst, (long)0xA0000u);
    ASSERT_EQ((long)g_saveblk_stride, (long)0x140);
    ASSERT_EQ((long)g_saveblk_src, (long)expect_width);
}

/* The 5 frame-layer buffers are freshly malloc'd into the global array and
 * the return value is the array head (= &array[0]). */
static void assert_buffers_allocated(uint32 ret)
{
    int i;
    g_blitraw_log_on = 0;
    ASSERT_EQ((long)ret,
              (long)(uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs);
    for (i = 0; i < 5; i++) {
        ASSERT_TRUE(data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[i] != NULL);
    }
    for (i = 0; i < 5; i++) {
        free(data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[i]);
        data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[i] = NULL;
    }
}

/*
 * flip==0 with enemy portrait mode (0x728): dst_origin defaults to 2, so
 * width = 2*0x140 + 5 = 0x285. The 5-stage table runs with origin 2.
 */
static void test_open_anim_default_origin_enemy(void)
{
    uint32 ret;

    open_anim_reset(0x728);
    ret = fd2_play_dialog_open_animation(0, 0, 0);
    assert_five_stage_frame(2u, 2u * 0x140u + 5u);
    assert_buffers_allocated(ret);
}

/*
 * flip==0 with ally portrait mode (0x9017): dst_origin defaults to 0x70, so
 * width = 0x70*0x140 + 5 = 0xCC05. Confirms the second (else-if) mapping and
 * that the large origin propagates through every stage + the width math.
 */
static void test_open_anim_default_origin_ally(void)
{
    uint32 ret;

    open_anim_reset(0x9017);
    ret = fd2_play_dialog_open_animation(0, 0, 0);
    assert_five_stage_frame(0x70u, 0x70u * 0x140u + 5u);
    assert_buffers_allocated(ret);
}

/*
 * flip==0 with neither portrait mode: dst_origin stays 0 (the else-if must
 * not fall through to a default), so width = 0*0x140 + 5 = 5 and every stage
 * runs with origin 0.
 */
static void test_open_anim_default_origin_none(void)
{
    uint32 ret;

    open_anim_reset(0x1234);   /* matches neither 0x728 nor 0x9017 */
    ret = fd2_play_dialog_open_animation(0, 0, 0);
    assert_five_stage_frame(0u, 5u);
    assert_buffers_allocated(ret);
}

/*
 * fd2_assemble_dialog_frame_layered direct layout test.
 *
 * Drives the real function with a small, hand-computable geometry
 * (dst=0, pitch=0x10, col_offset=5, row_offset=0, n_cols=4, n_rows=2) and
 * asserts the exact ordered sequence of (sprite_index, dst) blits. This pins
 * down every address-computation term: topleft, the 4 outer + 4 inner +
 * 2 mid + 2 secondary-bottom corners, the top/bottom stretch edge loop, the
 * (here empty) left/right edge loop, and the interior fill double loop.
 *
 *   topleft       = 0 + 5 + 0*0x10            = 0x05
 *   pitch3        = 0x10*3                     = 0x30
 *   row_pixels16  = 0x10*0x10                  = 0x100
 *   bottom_full   = 0x100 * n_rows(2)          = 0x200
 *   bottom_row    = 0x100 * (n_rows-1)         = 0x100
 *   inner_top     = 0x30 + 0x05               = 0x35
 */
static void test_frame_layout_exact(void)
{
    static const uint32 exp_idx[24] = {
        1, 2, 3, 4, 5, 6, 7, 8, 0xe, 0xf, 0x10, 0x11,
        9, 0xc, 9, 0xc,
        0xd, 0xd, 0xd, 0xd, 0xd, 0xd, 0xd, 0xd
    };
    static const uint32 exp_dst[24] = {
        0x05, 0x48, 0x235, 0x278, 0x08, 0x38, 0x238, 0x268,
        0x35, 0x78, 0x135, 0x178,
        0x18, 0x248, 0x28, 0x258,
        0x38, 0x48, 0x58, 0x68, 0x138, 0x148, 0x158, 0x168
    };
    int k;

    install_fake_sheet();
    g_blitraw_count = 0;
    g_blitraw_log_on = 1;

    fd2_assemble_dialog_frame_layered(0u, 0x10u, 5u, 0, 4, 2);

    g_blitraw_log_on = 0;

    ASSERT_EQ((long)g_blitraw_count, 24);
    for (k = 0; k < 24; k++) {
        ASSERT_EQ((long)logged_sprite_idx(k), (long)exp_idx[k]);
        ASSERT_EQ((long)g_blitraw_log_dst[k], (long)exp_dst[k]);
    }
}

/*
 * Left/right edge loop coverage: with n_rows>=4 the row loop (1..n_rows-2)
 * runs, emitting sprite A (left) + sprite B (right) per interior row. Uses
 * dst=0, pitch=0x10, col_offset=0, row_offset=0, n_cols=3, n_rows=4 so the
 * loop runs for row=1,2. For row r: row_full = 0x100*r + 0x30 + topleft(0);
 *   A @ row_full ; B @ row_full + n_cols*0x10 + 3 = row_full + 0x33.
 */
static void test_frame_left_right_edges(void)
{
    int    k;
    int    a_seen;
    int    b_seen;
    uint32 row1;
    uint32 row2;

    install_fake_sheet();
    g_blitraw_count = 0;
    g_blitraw_log_on = 1;

    fd2_assemble_dialog_frame_layered(0u, 0x10u, 0u, 0, 3, 4);

    g_blitraw_log_on = 0;

    row1 = 0x100u * 1u + 0x30u;     /* 0x130 */
    row2 = 0x100u * 2u + 0x30u;     /* 0x230 */

    a_seen = 0;
    b_seen = 0;
    for (k = 0; k < g_blitraw_count; k++) {
        if (logged_sprite_idx(k) == 0xa) {
            ASSERT_TRUE(g_blitraw_log_dst[k] == row1 ||
                        g_blitraw_log_dst[k] == row2);
            a_seen++;
        }
        if (logged_sprite_idx(k) == 0xb) {
            ASSERT_TRUE(g_blitraw_log_dst[k] == row1 + 0x33u ||
                        g_blitraw_log_dst[k] == row2 + 0x33u);
            b_seen++;
        }
    }
    ASSERT_EQ((long)a_seen, 2);
    ASSERT_EQ((long)b_seen, 2);
}

void run_dialog_dialog_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: dialog/dialog\n");
    RUN_TEST(test_cleanup_forwards_args);
    RUN_TEST(test_cleanup_frees_buffer);
    RUN_TEST(test_text_glyph_advance_and_end);
    RUN_TEST(test_keypress_suppresses_blink);
    RUN_TEST(test_line_advance_arithmetic);
    RUN_TEST(test_line_advance_count_accumulates);
    RUN_TEST(test_literal_number_digits);
    RUN_TEST(test_page_idx_selects_start);
    RUN_TEST(test_blink_frame_cycle);
    RUN_TEST(test_open_anim_default_origin_enemy);
    RUN_TEST(test_open_anim_default_origin_ally);
    RUN_TEST(test_open_anim_default_origin_none);
    RUN_TEST(test_frame_layout_exact);
    RUN_TEST(test_frame_left_right_edges);
    printf("\n");
}
