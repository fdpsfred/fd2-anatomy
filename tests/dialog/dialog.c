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
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx */

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

    /* The typewriter step fires fd2_play_sfx_with_handle(fdother bank, 2, 1)
     * once per rendered glyph; g_dlg_blink_calls counts those via the relocated
     * AIL stop spy. Open the audio gates and stage a valid bank so the now-real
     * player reaches that spy. */
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);
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

/* ---- fd2_close_dialog_panels_then_slide_in_at (dialog teardown) ----
 * Phase 1 (always): reverse-order cleanup of the 5 layer buffers (slots
 * 4..1 then 0). Each cleanup forwards to the recording restore stub
 * (g_restore_block_*) and free()s the slot, so g_restore_block_calls
 * counts the cleanups and the malloc'd slots are released safely.
 *
 * Phase 2 (slot_offset != 0): an interpolation loop runs frames 0..N
 * where N = cursor_x + cursor_y (testglob defaults 5 + 5 = 10, i.e. 11
 * frames). Each frame calls the REAL fd2_blit_indexed_sprite_with_alloc
 * (→ real fd2_save_screen_block_to_buffer = g_saveblk_calls++, real
 * fd2_blit_sprite_with_stride_setup = g_blitsetup_dst capture) then one
 * cleanup. The blit resolves dst = sprite_idx*0x140 + sheet_base + dst,
 * with sheet_base = interp_y (arg4) and sprite_idx = interp_x (arg5) —
 * the close path's mirror of the open path's arg pairing — so the final
 * frame's g_blitsetup_dst pins the interpolation endpoint arithmetic. */
extern int    g_saveblk_calls;
extern uint32 g_blitsetup_dst;

/* Allocate the 5 frame-layer slots so cleanup's free() is valid, and
 * arm the recorders. Returns the array base passed as anim_handle. */
static uint32 close_anim_setup(void)
{
    int i;
    install_fake_sheet();
    for (i = 0; i < 5; i++) {
        data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[i] = malloc(32);
    }
    g_restore_block_calls = 0;
    g_saveblk_calls = 0;
    g_blitsetup_dst = 0;
    return (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs;
}

/*
 * slot_offset == 0: Phase 2 is skipped entirely. Exactly the 5 layer
 * buffers are torn down (one restore each), and no blit / save occurs.
 */
static void test_close_no_slide(void)
{
    uint32 base;

    base = close_anim_setup();
    fd2_close_dialog_panels_then_slide_in_at(base, 0);

    ASSERT_EQ((long)g_restore_block_calls, 5);
    ASSERT_EQ((long)g_saveblk_calls, 0);
}

/*
 * slot_offset != 0 with cursor (5,5): Phase 1 does 5 cleanups, then the
 * slide loop runs 11 frames (0..10), each doing one real blit (+1 save)
 * and one cleanup → 5 + 11 = 16 restores, 11 saves. The final frame
 * (f=10) interpolates to the endpoint:
 *   src_x_px = src_y_px = 5*0x18 = 120
 *   interp_y = 5 - ((5 - 124)*10)/10 = 5 - (-119) = 124
 *   interp_x = 5 - ((5 - 124)*10)/10 = 124          (slot_offset = 5)
 * and the real blit lands at
 *   dst = interp_x*0x140 + interp_y + 0xA0000 = 124*0x140 + 124 + 0xA0000
 */
static void test_close_slide_symmetric(void)
{
    uint32 base;
    uint32 saved_x;
    uint32 saved_y;

    saved_x = data_fd2_battle_cursor_screen_x;
    saved_y = data_fd2_battle_cursor_screen_y;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;

    base = close_anim_setup();
    fd2_close_dialog_panels_then_slide_in_at(base, 5);

    ASSERT_EQ((long)g_restore_block_calls, 16);
    ASSERT_EQ((long)g_saveblk_calls, 11);
    ASSERT_EQ((long)g_blitsetup_dst,
              (long)(124u * 0x140u + 124u + 0xA0000u));

    data_fd2_battle_cursor_screen_x = saved_x;
    data_fd2_battle_cursor_screen_y = saved_y;
}

/*
 * Asymmetric cursor isolates interp_y (from cursor_x) from interp_x
 * (from cursor_y), confirming the close path's mirrored arg pairing:
 * the blit's sheet_base (arg4) carries interp_y and sprite_idx (arg5)
 * carries interp_x. With cursor (x=5, y=3) and slot_offset != 0:
 *   src_x_px = 5*0x18 = 120,  src_y_px = 3*0x18 = 72
 *   total_frames = 5 + 3 = 8  → 9 frames (0..8); final frame f=8:
 *   interp_y = 5      - ((5      - (120+4))*8)/8 = 120+4 = 124
 *   interp_x = offset - ((offset - (72 +4))*8)/8 = 72 +4 = 76
 * (frame N always lands exactly on the source pixel, so both endpoints
 * are slot_offset-independent and differ only by the per-axis source.)
 * Real blit dst = interp_x*0x140 + interp_y + 0xA0000
 *               = 76*0x140 + 124 + 0xA0000.
 */
static void test_close_slide_asymmetric_axes(void)
{
    uint32 base;
    uint32 saved_x;
    uint32 saved_y;

    saved_x = data_fd2_battle_cursor_screen_x;
    saved_y = data_fd2_battle_cursor_screen_y;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 3;

    base = close_anim_setup();
    fd2_close_dialog_panels_then_slide_in_at(base, 0x200);

    ASSERT_EQ((long)g_restore_block_calls, 5 + 9);   /* 5 + 9 frames */
    ASSERT_EQ((long)g_saveblk_calls, 9);
    ASSERT_EQ((long)g_blitsetup_dst,
              (long)(76u * 0x140u + 124u + 0xA0000u));

    data_fd2_battle_cursor_screen_x = saved_x;
    data_fd2_battle_cursor_screen_y = saved_y;
}

/*
 * fd2_cinematic_scroll_text_up_for_special_scenes early-returns (no scroll,
 * no framebuffer touch) when data_fd2_dialog_active_portrait_blit_offset is
 * neither 0x728 nor 0x9017. This is the only path safe to drive in a unit
 * test: the active-portrait paths write to fixed absolute VGA framebuffer
 * addresses (0xA0B4F / 0xA951F) that are unmapped in the host process, so
 * the actual scroll behavior is deferred to Phase 9 integration. Calling
 * with a non-portrait global value must complete without dereferencing the
 * framebuffer (a crash here would mean the guard branch was emitted wrong).
 */
static void test_scroll_no_portrait_is_noop(void)
{
    uint32 saved;

    saved = data_fd2_dialog_active_portrait_blit_offset;

    data_fd2_dialog_active_portrait_blit_offset = 0;
    fd2_cinematic_scroll_text_up_for_special_scenes();

    data_fd2_dialog_active_portrait_blit_offset = 0x1234;
    fd2_cinematic_scroll_text_up_for_special_scenes();

    /* survived both calls without touching the framebuffer */
    ASSERT_TRUE(1);

    data_fd2_dialog_active_portrait_blit_offset = saved;
}

/*
 * fd2_backup_dialog_area_to_buffer snapshots a 0x48 x 0x48 pixel block out of
 * the working framebuffer (data_fd2_large_game_state_buffer_ptr + 0x8088),
 * anchored one tile up/left of the cursor, into a freshly malloc'd
 * data_fd2_dialog_area_backup_buffer.  Verifies (a) the source anchor formula
 * (cx-1)*0x18 + (cy-1)*0x2AC0 + 0x8088, (b) the working-buffer row stride
 * 0x1C8, (c) the dest row stride 0x48, and (d) that all 0x48 rows x 0x48 cols
 * are copied.  Source is filled so cell value == (offset_from_buffer_base) low
 * byte, letting us recompute every expected byte independently of the code.
 */
static void test_backup_snapshots_region(void)
{
    uint8 *work;
    uint8 *backup;
    uint32 work_size;
    uint32 anchor;     /* offset of row 0 col 0 within work buffer */
    uint32 i;
    int    row;
    int    col;
    int    ok;
    uint32 cx;
    uint32 cy;

    /* big enough that anchor + (0x47*0x1C8) + 0x48 stays in bounds */
    work_size = 0x20000;
    work = (uint8 *)malloc(work_size);
    ASSERT_TRUE(work != NULL);
    for (i = 0; i < work_size; i++) {
        work[i] = (uint8)(i & 0xFF);
    }

    cx = 2;
    cy = 3;
    data_fd2_large_game_state_buffer_ptr = (uint32)work;
    data_fd2_battle_cursor_screen_x = cx;
    data_fd2_battle_cursor_screen_y = cy;
    data_fd2_dialog_area_backup_buffer = (void *)0;  /* no prior buffer */

    anchor = 0x8088 + (cx - 1) * 0x18 + (cy - 1) * 0x2AC0;
    ASSERT_TRUE(anchor + 0x47 * 0x1C8 + 0x48 <= work_size);

    fd2_backup_dialog_area_to_buffer();

    backup = (uint8 *)data_fd2_dialog_area_backup_buffer;
    ASSERT_TRUE(backup != NULL);

    /* every cell: backup[row*0x48 + col] == work[anchor + row*0x1C8 + col] */
    ok = 1;
    for (row = 0; row < 0x48; row++) {
        for (col = 0; col < 0x48; col++) {
            uint8 got = backup[row * 0x48 + col];
            uint8 exp = work[anchor + (uint32)row * 0x1C8 + (uint32)col];
            if (got != exp) {
                ok = 0;
            }
        }
    }
    ASSERT_TRUE(ok);

    /* spot-check the two extreme corners against the raw formula */
    ASSERT_EQ((long)backup[0], (long)(uint8)(anchor & 0xFF));
    ASSERT_EQ((long)backup[0x47 * 0x48 + 0x47],
              (long)(uint8)((anchor + 0x47 * 0x1C8 + 0x47) & 0xFF));

    free(backup);
    data_fd2_dialog_area_backup_buffer = (void *)0;
    free(work);
    data_fd2_large_game_state_buffer_ptr = 0;
}

/*
 * When a previous backup buffer exists it must be freed before the new alloc
 * (the function owns and replaces the buffer each call).  We give it a real
 * malloc'd buffer so the internal free() is valid, then confirm the pointer
 * was replaced with a fresh allocation.
 */
static void test_backup_frees_prior_buffer(void)
{
    uint8 *work;
    void  *prior;

    work = (uint8 *)malloc(0x20000);
    ASSERT_TRUE(work != NULL);
    memset(work, 0x55, 0x20000);

    prior = malloc(0x1440);
    ASSERT_TRUE(prior != NULL);

    data_fd2_large_game_state_buffer_ptr = (uint32)work;
    data_fd2_battle_cursor_screen_x = 1;   /* cx-1 == 0 */
    data_fd2_battle_cursor_screen_y = 1;   /* cy-1 == 0 */
    data_fd2_dialog_area_backup_buffer = prior;

    fd2_backup_dialog_area_to_buffer();

    /* a new buffer was allocated (replacing prior, which was freed) */
    ASSERT_TRUE(data_fd2_dialog_area_backup_buffer != NULL);
    /* with cx=cy=1 the anchor is exactly base+0x8088, row 0 col 0 byte = 0x55 */
    ASSERT_EQ((long)((uint8 *)data_fd2_dialog_area_backup_buffer)[0], 0x55);

    free(data_fd2_dialog_area_backup_buffer);
    data_fd2_dialog_area_backup_buffer = (void *)0;
    free(work);
    data_fd2_large_game_state_buffer_ptr = 0;
}

/*
 * fd2_restore_dialog_area_from_buffer is the inverse of the backup: it copies
 * the 0x48 x 0x48 backup buffer (data_fd2_dialog_area_backup_buffer, dest-row
 * stride 0x48) back into the working framebuffer at anchor
 * 0x8088 + (cx-1)*0x18 + (cy-1)*0x2AC0 with working-buffer row stride 0x1C8.
 * The backup buffer is filled so cell[row*0x48+col] == low byte of its own
 * index, letting us recompute every expected framebuffer byte independently.
 * The working buffer is pre-cleared to a sentinel so we can also confirm the
 * function touches ONLY the 0x48x0x48 destination cells and nothing else.
 */
static void test_restore_writes_region(void)
{
    uint8 *work;
    uint8 *backup;
    uint32 work_size;
    uint32 anchor;
    uint32 i;
    int    row;
    int    col;
    int    ok;
    int    untouched_ok;
    uint32 cx;
    uint32 cy;

    work_size = 0x20000;
    work = (uint8 *)malloc(work_size);
    ASSERT_TRUE(work != NULL);
    memset(work, 0xEE, work_size);   /* sentinel for "untouched" */

    backup = (uint8 *)malloc(0x1440);  /* 0x48 * 0x48 */
    ASSERT_TRUE(backup != NULL);
    for (i = 0; i < 0x1440; i++) {
        backup[i] = (uint8)(i & 0xFF);
    }

    cx = 2;
    cy = 3;
    data_fd2_large_game_state_buffer_ptr = (uint32)work;
    data_fd2_battle_cursor_screen_x = cx;
    data_fd2_battle_cursor_screen_y = cy;
    data_fd2_dialog_area_backup_buffer = backup;

    anchor = 0x8088 + (cx - 1) * 0x18 + (cy - 1) * 0x2AC0;
    ASSERT_TRUE(anchor + 0x47 * 0x1C8 + 0x48 <= work_size);

    fd2_restore_dialog_area_from_buffer();

    /* every dest cell: work[anchor+row*0x1C8+col] == backup[row*0x48+col] */
    ok = 1;
    for (row = 0; row < 0x48; row++) {
        for (col = 0; col < 0x48; col++) {
            uint8 got = work[anchor + (uint32)row * 0x1C8 + (uint32)col];
            uint8 exp = backup[row * 0x48 + col];
            if (got != exp) {
                ok = 0;
            }
        }
    }
    ASSERT_TRUE(ok);

    /* the gap byte just past each copied row must remain the sentinel,
       proving the per-row copy length is exactly 0x48 (not the 0x1C8 stride) */
    untouched_ok = 1;
    for (row = 0; row < 0x47; row++) {
        if (work[anchor + (uint32)row * 0x1C8 + 0x48] != 0xEE) {
            untouched_ok = 0;
        }
    }
    ASSERT_TRUE(untouched_ok);

    /* spot-check extreme corners against the raw formula */
    ASSERT_EQ((long)work[anchor], (long)(uint8)0);
    ASSERT_EQ((long)work[anchor + 0x47 * 0x1C8 + 0x47],
              (long)(uint8)((0x47 * 0x48 + 0x47) & 0xFF));

    free(backup);
    data_fd2_dialog_area_backup_buffer = (void *)0;
    free(work);
    data_fd2_large_game_state_buffer_ptr = 0;
}

/* per-call corner-blit log + composite proxy (testglob.c) */
extern int    g_blitsetup_calls;
extern uint32 g_blitsetup_dst_log[32];
extern uint32 g_blitsetup_sprite_log[32];
extern int    g_composite_call_count;
extern uint32 g_tile_map_last_dst;
extern uint32 g_tile_map_last_stride;
extern uint32 g_tile_map_last_w;
extern uint32 g_tile_map_last_h;
extern uint32 g_tile_map_last_ox;
extern uint32 g_tile_map_last_oy;

/* Shared fixture for fd2_animate_dialog_page_advance_collapse: allocate the
 * game-state work buffer (gss), the composed-target work buffer (rwc) and the
 * menu-dialog-state handle, point the globals at them, and seed rwc + handle
 * with independently-recomputable data.  gss must cover the row-copy dst, the
 * yes_no box anchor (gss+0x1A59C) and fd2_blit_rectangle's gss+0x8088 read
 * span; rwc must cover both the per-frame and settle source spans; the handle
 * needs valid dword sprite offsets at selector*0xC for selectors 0x10/0x11.
 * Returns gss; out-params expose rwc/handle and the two seeded sprite offsets. */
#define PAC_H_OFF_L 0x1111u   /* handle dword stored at selector 0x10 -> 0xC0 */
#define PAC_H_OFF_R 0x2222u   /* handle dword stored at selector 0x11 -> 0xCC */
static uint32 g_pac_saved_party_count;
static void pac_setup(uint8 **out_gss, uint8 **out_rwc, uint8 **out_handle)
{
    uint8 *gss;
    uint8 *rwc;
    uint8 *handle;
    uint32 k;

    gss    = (uint8 *)malloc(0x24000);
    rwc    = (uint8 *)malloc(0x12000);
    handle = (uint8 *)malloc(0x200);
    ASSERT_TRUE(gss != NULL && rwc != NULL && handle != NULL);

    memset(gss, 0xAA, 0x24000);
    for (k = 0; k < 0x12000; k++) {
        rwc[k] = (uint8)((k * 7u + 3u) & 0xFF);   /* distinguishable source */
    }
    memset(handle, 0, 0x200);
    *(uint32 *)(handle + 0x10 * 0xC) = PAC_H_OFF_L;   /* selector 0x10 -> 0xC0 */
    *(uint32 *)(handle + 0x11 * 0xC) = PAC_H_OFF_R;   /* selector 0x11 -> 0xCC */

    data_fd2_large_game_state_buffer_ptr    = (uint32)gss;
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)rwc;
    data_fd2_menu_dialog_state_handle       = (uint32)handle;
    g_pac_saved_party_count                 = data_fd2_battle_party_member_count;
    data_fd2_battle_party_member_count      = 0;   /* empty party -> overlay no-op */

    g_blitsetup_calls    = 0;
    g_composite_call_count = 0;

    *out_gss = gss;
    *out_rwc = rwc;
    *out_handle = handle;
}

static void pac_teardown(uint8 *gss, uint8 *rwc, uint8 *handle)
{
    free(gss);
    free(rwc);
    free(handle);
    data_fd2_large_game_state_buffer_ptr    = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    data_fd2_menu_dialog_state_handle       = 0;
    data_fd2_battle_tile_map_ptr            = 0;
    data_fd2_battle_party_member_count      = g_pac_saved_party_count;
}

/*
 * Core arithmetic of fd2_animate_dialog_page_advance_collapse with the
 * battle-tile-map gate OFF (battle_tile_map <= 1 -> no scene prime).  Drives
 * the real function (corner sprites blit to recording stub; row-copy + settle
 * memmoves + fd2_blit_rectangle run for real, the latter two writing to the
 * DOS/4GW VGA aperture which is harmless) and verifies:
 *   (a) exactly 8 corner blits = 2 (left/right) x 4 frames;
 *   (b) per-blit dst = corner_offset + (gss + 0x1A59C) where the left offset
 *       steps -12,-8,-4,0 and the right offset steps +12,+8,+4,0 across frames
 *       (the binary's corner_state[i+2] animation, init -16/+16, +4/-4 each
 *       frame), and per-blit sprite = handle + handle[selector*0xC] with
 *       selector = template[0]=0x10 for the left and template[1]=0x11 for the
 *       right corner (the Watcom adjacent-locals pairing corner_state[i] vs
 *       corner_state[i+2] — the subtle part this test pins);
 *   (c) the per-frame row copy: gss[0x8089 + (r+0x6C)*0x1C8 + c] ==
 *       rwc[0x8C05 + r*0x140 + c] for sampled (r,c).
 */
static void test_page_advance_collapse_corners_and_copy(void)
{
    uint8 *gss;
    uint8 *rwc;
    uint8 *handle;
    uint32 box;
    int    f;
    int    rs[4];
    int    ls[4];
    int    ok;
    int    r;
    int    c;

    pac_setup(&gss, &rwc, &handle);
    box = (uint32)gss + 0x1A59C;

    data_fd2_battle_tile_map_ptr = 1;   /* gate OFF */

    fd2_animate_dialog_page_advance_collapse();

    /* gate OFF: scene-prime composite must not have run */
    ASSERT_EQ((long)g_composite_call_count, 0);

    /* (a) 2 corner blits per frame x 4 frames */
    ASSERT_EQ((long)g_blitsetup_calls, 8);

    /* (b) corner offsets per frame: left -12,-8,-4,0 ; right +12,+8,+4,0 */
    ls[0] = -12; ls[1] = -8; ls[2] = -4; ls[3] = 0;
    rs[0] =  12; rs[1] =  8; rs[2] =  4; rs[3] = 0;
    ok = 1;
    for (f = 0; f < 4; f++) {
        /* left corner = log[2f]: selector 0x10 */
        if (g_blitsetup_dst_log[2 * f] != (uint32)(box + (uint32)ls[f])) {
            ok = 0;
        }
        if (g_blitsetup_sprite_log[2 * f] != (uint32)handle + PAC_H_OFF_L) {
            ok = 0;
        }
        /* right corner = log[2f+1]: selector 0x11 */
        if (g_blitsetup_dst_log[2 * f + 1] != (uint32)(box + (uint32)rs[f])) {
            ok = 0;
        }
        if (g_blitsetup_sprite_log[2 * f + 1] != (uint32)handle + PAC_H_OFF_R) {
            ok = 0;
        }
    }
    ASSERT_TRUE(ok);

    /* (c) per-frame row copy landed rwc rows into the gss work buffer */
    ok = 1;
    for (r = 0; r <= 0x55; r += 0x55) {          /* first + last row */
        for (c = 0; c <= 0x135; c += 0x135) {    /* first + last col */
            uint8 got = gss[0x8089 + (uint32)(r + 0x6C) * 0x1C8 + (uint32)c];
            uint8 exp = rwc[0x8C05 + (uint32)r * 0x140 + (uint32)c];
            if (got != exp) {
                ok = 0;
            }
        }
    }
    ASSERT_TRUE(ok);

    pac_teardown(gss, rwc, handle);
}

/*
 * The battle-tile-map gate: when battle_tile_map > 1 the function primes the
 * underlying scene (palette tick + fd2_composite_battle_tile_map + char
 * overlay) before the fold-in frames; when <= 1 it skips straight to the
 * frames.  fd2_composite_battle_tile_map is the recording proxy (counts as the
 * scene prime), and the party count is 0 so the real char-overlay/shadow are
 * no-ops and the real palette tick only bumps its counters.  Verifies the gate
 * decision both ways and that the prime composites the gss work buffer at
 * gss+0x8088 with the binary's fixed tile-map params (stride 0x1C8, 0xD x 8,
 * origin from the battle-view window globals).
 */
static void test_page_advance_collapse_composite_gate(void)
{
    uint8 *gss;
    uint8 *rwc;
    uint8 *handle;

    /* gate OFF (== 1): no prime */
    pac_setup(&gss, &rwc, &handle);
    data_fd2_battle_tile_map_ptr = 1;
    fd2_animate_dialog_page_advance_collapse();
    ASSERT_EQ((long)g_composite_call_count, 0);
    pac_teardown(gss, rwc, handle);

    /* gate ON (> 1): exactly one scene prime with the fixed tile-map params */
    pac_setup(&gss, &rwc, &handle);
    data_fd2_battle_tile_map_ptr = 2;
    data_fd2_battle_view_window_origin_x = 0x37;
    data_fd2_battle_view_window_origin_y = 0x29;
    fd2_animate_dialog_page_advance_collapse();
    ASSERT_EQ((long)g_composite_call_count, 1);
    ASSERT_EQ((long)g_tile_map_last_dst, (long)((uint32)gss + 0x8088));
    ASSERT_EQ((long)g_tile_map_last_stride, (long)0x1C8);
    ASSERT_EQ((long)g_tile_map_last_w, (long)0xD);
    ASSERT_EQ((long)g_tile_map_last_h, (long)8);
    ASSERT_EQ((long)g_tile_map_last_ox, (long)0x37);
    ASSERT_EQ((long)g_tile_map_last_oy, (long)0x29);
    pac_teardown(gss, rwc, handle);
}

/* ---- fd2_text_dialog_typewriter_loop (Yes/No typewriter + input loop) ----
 *
 * The function runs a 4-frame intro (memmove copybacks + corner blits +
 * fd2_blit_rectangle, all against work buffers / the harmless VGA aperture)
 * and then a key-driven main loop. With the BIOS keyboard buffer pre-loaded
 * non-empty at entry, the inner throttle loop is skipped on every pass and
 * execution falls straight through to the real INT 16h (int386 AH=10h) read +
 * scancode dispatch — the same drive the input.c wait_* tests use. We can thus
 * exercise the return contract (1 = advance/Yes, -1 = cancel), the cursor
 * left/right side-effects, and the oscillator reset, all deterministically.
 *
 * The per-glyph typewriter body and the full-page copyback are pure display
 * side effects gated behind the throttle (a BIOS-tick-paced frame that only
 * fires after >=2 ticks of real wall time); driving them needs the buffer to
 * start empty and flip mid-loop, and they only write pixels / work buffers.
 * Those are deferred to Phase 9 integration. The EAX-bug-sensitive RNG seeding
 * at setup IS pinned here: with the loop skipped exactly one
 * fd2_advance_rng_state() runs, so the post-run seed proves the pace counter is
 * seeded from a fresh RNG return (not the clobbered __CHK / blit-return EAX
 * that Ghidra renders).
 *
 * Large work buffers (the setup copies the whole 200x320 page into the
 * game-state buffer at (row-4)*0x1C8 + 0x8084, reaching ~0x3D3DC, and reads the
 * 64000-byte VGA snapshot back out of the slide buffer), sized past those spans.
 */
extern uint16 data_fd2_shared_rng_seed;

static uint8 *g_tw_gss;
static uint8 *g_tw_rwc;
static uint8 *g_tw_handle;
static uint32 g_tw_saved_party;

/* rol16((seed + 0x9014) & 0xFFFF, 3) — one fd2_advance_rng_state() step. */
static uint16 tw_rng_next(uint16 seed)
{
    uint32 v;
    v = (uint32)((seed + 0x9014u) & 0xFFFFu);
    v = ((v << 3) | (v >> 13)) & 0xFFFFu;
    return (uint16)v;
}

static void tw_setup(uint32 tile_map)
{
    g_tw_gss    = (uint8 *)malloc(0x42000);   /* covers (199-4)*0x1C8+0x8084 */
    g_tw_rwc    = (uint8 *)malloc(0x20000);   /* covers 199*0x140 + intro src */
    g_tw_handle = (uint8 *)malloc(0x400);
    ASSERT_TRUE(g_tw_gss != NULL && g_tw_rwc != NULL && g_tw_handle != NULL);
    memset(g_tw_gss, 0, 0x42000);
    memset(g_tw_rwc, 0, 0x20000);
    memset(g_tw_handle, 0, 0x400);

    data_fd2_large_game_state_buffer_ptr      = (uint32)g_tw_gss;
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)g_tw_rwc;
    data_fd2_menu_dialog_state_handle         = (uint32)g_tw_handle;
    g_tw_saved_party                          = data_fd2_battle_party_member_count;
    data_fd2_battle_party_member_count        = 0;   /* overlay no-op */
    data_fd2_battle_tile_map_ptr              = tile_map;

    g_blitsetup_calls      = 0;
    g_composite_call_count = 0;
    data_fd2_ui_menu_cursor_idx = 0x55;   /* poison: setup must clear to 0 */
}

static void tw_teardown(void)
{
    free(g_tw_gss);
    free(g_tw_rwc);
    free(g_tw_handle);
    data_fd2_large_game_state_buffer_ptr      = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    data_fd2_menu_dialog_state_handle         = 0;
    data_fd2_battle_tile_map_ptr              = 0;
    data_fd2_battle_party_member_count        = g_tw_saved_party;
}

/* Load one INT 16h scancode (high byte) into the BIOS keyboard ring and mark
 * the buffer non-empty so fd2_check_keyboard_buffer_nonempty() reads pending. */
static void tw_queue_key(uint16 scancode_high)
{
    *(volatile uint16 *)0x41AuL = 0x1E;            /* head */
    *(volatile uint16 *)0x41CuL = 0x20;            /* tail = head + 2 (1 key) */
    *(volatile uint16 *)0x41EuL = scancode_high << 8;
}

/* Each confirm scancode (Enter 0x1C, Space 0x39, extended 0xE0, numpad-0 0x52)
 * returns 1 and resets the blink oscillator to 0 on the way out. The loop is
 * skipped (buffer non-empty), so exactly one RNG step ran at setup: the post
 * seed pins the EAX-bug-corrected pace seeding. */
static void test_typewriter_confirm_returns_one(void)
{
    static const uint16 yes_scancodes[4] = { 0x1C, 0x39, 0xE0, 0x52 };
    int    k;
    int    r;
    uint16 seed0;

    for (k = 0; k < 4; k++) {
        tw_setup(0);                  /* full-screen dialog path */
        data_fd2_dialog_blink_phase_oscillator = 3;   /* nonzero -> must clear */
        seed0 = 0x1234;
        data_fd2_shared_rng_seed = seed0;
        tw_queue_key(yes_scancodes[k]);

        r = fd2_text_dialog_typewriter_loop();

        ASSERT_EQ((long)r, 1);
        ASSERT_EQ((long)data_fd2_dialog_blink_phase_oscillator, 0);
        ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);   /* setup cleared poison */
        ASSERT_EQ((long)data_fd2_shared_rng_seed, (long)tw_rng_next(seed0));
        tw_teardown();
    }
}

/* Esc (0x01) and numpad-. (0x53) cancel: return -1 and reset the oscillator. */
static void test_typewriter_cancel_returns_minus_one(void)
{
    static const uint16 no_scancodes[2] = { 0x01, 0x53 };
    int k;
    int r;

    for (k = 0; k < 2; k++) {
        tw_setup(2);                  /* in-battle small-dialog path */
        data_fd2_dialog_blink_phase_oscillator = 2;
        tw_queue_key(no_scancodes[k]);

        r = fd2_text_dialog_typewriter_loop();

        ASSERT_EQ((long)r, -1);
        ASSERT_EQ((long)data_fd2_dialog_blink_phase_oscillator, 0);
        tw_teardown();
    }
}

/* Left (0x4B) sets cursor=0 then loops; Right (0x4D) sets cursor=1 then loops.
 * We queue the navigation key followed by a confirm (Enter) so the loop reads
 * the nav key (sets the cursor, loops back), then the confirm consumes the
 * second buffered key and returns 1 — pinning both the cursor assignment and
 * the loop-back (non-return) behavior of the nav scancodes. */
static void test_typewriter_cursor_left_then_confirm(void)
{
    int r;

    tw_setup(0);
    /* two keys: 0x4B (Left) at head, 0x1C (Enter) next; tail = head + 4 */
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x22;
    *(volatile uint16 *)0x41EuL = 0x4B00;
    *(volatile uint16 *)0x420uL = 0x1C00;
    data_fd2_ui_menu_cursor_idx = 1;     /* preset 1 so Left -> 0 is observable */

    r = fd2_text_dialog_typewriter_loop();

    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);   /* Left committed cursor=0 */
    tw_teardown();
}

static void test_typewriter_cursor_right_then_confirm(void)
{
    int r;

    tw_setup(0);
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x22;
    *(volatile uint16 *)0x41EuL = 0x4D00;   /* Right */
    *(volatile uint16 *)0x420uL = 0x1C00;   /* Enter */
    data_fd2_ui_menu_cursor_idx = 0;     /* preset 0 so Right -> 1 is observable */

    r = fd2_text_dialog_typewriter_loop();

    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);   /* Right committed cursor=1 */
    tw_teardown();
}

/* The 4-frame intro runs unconditionally before the main loop: it folds the two
 * Yes/No corner sprites outward, emitting exactly 2 corner blits per frame = 8.
 * With battle_tile_map==0 (gate off) the scene-prime composite must not run. */
static void test_typewriter_intro_emits_eight_corner_blits(void)
{
    tw_setup(0);
    tw_queue_key(0x1C);                /* Enter -> exit right after intro */

    fd2_text_dialog_typewriter_loop();

    ASSERT_EQ((long)g_blitsetup_calls, 8);     /* 2 corners x 4 intro frames */
    ASSERT_EQ((long)g_composite_call_count, 0);/* gate off: no scene prime */
    tw_teardown();
}

/* battle_tile_map>1 primes the battle base scene once during setup (palette tick
 * + fd2_composite_battle_tile_map + char overlay) before the intro frames. The
 * tile-map composite proxy counts that single prime; the intro itself adds no
 * composite. Confirms the in-battle gate and the fixed tile-map params. */
static void test_typewriter_battle_gate_primes_scene(void)
{
    tw_setup(2);
    data_fd2_battle_view_window_origin_x = 0x21;
    data_fd2_battle_view_window_origin_y = 0x33;
    tw_queue_key(0x1C);

    fd2_text_dialog_typewriter_loop();

    ASSERT_EQ((long)g_composite_call_count, 1);   /* exactly one scene prime */
    ASSERT_EQ((long)g_tile_map_last_dst, (long)((uint32)g_tw_gss + 0x8088));
    ASSERT_EQ((long)g_tile_map_last_stride, (long)0x1C8);
    ASSERT_EQ((long)g_tile_map_last_w, (long)0xD);
    ASSERT_EQ((long)g_tile_map_last_h, (long)8);
    ASSERT_EQ((long)g_tile_map_last_ox, (long)0x21);
    ASSERT_EQ((long)g_tile_map_last_oy, (long)0x33);
    tw_teardown();
}

/* ---- fd2_scroll_text_screen_up_by_lines (0x24D22) ---------------- */
#define SCROLL_ROWS   0xC0
#define SCROLL_STRIDE 0x138

/* unique 3-byte fingerprint for a given source row index */
static uint8 scroll_tag0(int idx)  { return (uint8)idx; }
static uint8 scroll_tag1(int idx)  { return (uint8)(idx * 7 + 3); }
static uint8 scroll_tag2(int idx)  { return (uint8)(idx ^ 0xAA); }

static void scroll_fill_rows(uint8 *buf)
{
    int idx;
    for (idx = 0; idx < SCROLL_ROWS; idx++) {
        uint8 *row = buf + (uint32)idx * SCROLL_STRIDE;
        row[0]                  = scroll_tag0(idx);
        row[5]                  = scroll_tag1(idx);
        row[SCROLL_STRIDE - 1]  = scroll_tag2(idx);
    }
}

/* assert that destination row `dst` holds the full 0x138-byte fingerprint
 * of source row `src` (all three witness bytes), i.e. the whole row moved. */
static int scroll_row_is(uint8 *buf, int dst, int src)
{
    uint8 *row = buf + (uint32)dst * SCROLL_STRIDE;
    return row[0] == scroll_tag0(src)
        && row[5] == scroll_tag1(src)
        && row[SCROLL_STRIDE - 1] == scroll_tag2(src);
}

/*
 * Mode A: lines != 0 stores the low byte of `lines` into the pending
 * line-count state and returns without touching the buffer. We also pass a
 * value > 0xFF (0x105) to confirm only the low byte (0x05) is kept -- the
 * binary does MOV AL,[ESP+0xc] / MOV [0x51A10],AL (byte store).
 */
static void test_scroll_mode_a_stores_low_byte(void)
{
    uint8 *buf;

    buf = (uint8 *)malloc(SCROLL_ROWS * SCROLL_STRIDE);
    ASSERT_TRUE(buf != NULL);
    scroll_fill_rows(buf);
    data_fd2_graphics_static_bg_buffer_ptr = (uint32)buf;

    data_fd2_graphics_text_scroll_pending_line_count = 0;
    fd2_scroll_text_screen_up_by_lines(7);
    ASSERT_EQ((long)data_fd2_graphics_text_scroll_pending_line_count, 7);

    /* low-byte truncation: 0x105 -> 0x05 */
    fd2_scroll_text_screen_up_by_lines(0x105);
    ASSERT_EQ((long)data_fd2_graphics_text_scroll_pending_line_count, 0x05);

    /* buffer must be untouched in Mode A (row 0 still tagged for row 0) */
    ASSERT_TRUE(scroll_row_is(buf, 0, 0));
    ASSERT_TRUE(scroll_row_is(buf, SCROLL_ROWS - 1, SCROLL_ROWS - 1));

    free(buf);
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_text_scroll_pending_line_count = 0;
}

/*
 * Mode B cylinder scroll, N=1. Derived from the disassembly:
 *   dst row[p] for p in [0 .. N-1]   == old row[(0xC0-N) + p]   (bottom wraps to top)
 *   dst row[p] for p in [N .. 0xBF]  == old row[p - N]          (shift down by N)
 * So with N=1: row[0]==old row[0xBF]; row[p]==old row[p-1] for p in 1..0xBF.
 * Verifying the full 0x138-byte fingerprint guards against partial-row moves.
 */
static void test_scroll_mode_b_cylinder_n1(void)
{
    uint8 *buf;
    int p;
    int ok;

    buf = (uint8 *)malloc(SCROLL_ROWS * SCROLL_STRIDE);
    ASSERT_TRUE(buf != NULL);
    scroll_fill_rows(buf);
    data_fd2_graphics_static_bg_buffer_ptr = (uint32)buf;
    data_fd2_graphics_text_scroll_pending_line_count = 1;

    fd2_scroll_text_screen_up_by_lines(0);

    ok = scroll_row_is(buf, 0, SCROLL_ROWS - 1);   /* row 0 <- old last row */
    for (p = 1; p < SCROLL_ROWS; p++) {
        if (!scroll_row_is(buf, p, p - 1)) {
            ok = 0;
        }
    }
    ASSERT_TRUE(ok);

    free(buf);
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_text_scroll_pending_line_count = 0;
}

/*
 * Mode B cylinder scroll with N=3 (multi-row wrap, exercises the malloc
 * scratch + the i = 0xBF-N..0 loop boundary).
 *   row[0]==old 0xBD, row[1]==old 0xBE, row[2]==old 0xBF   (bottom 3 wrap to top)
 *   row[p]==old (p-3) for p in [3 .. 0xBF]                 (shift down by 3)
 */
static void test_scroll_mode_b_cylinder_n3(void)
{
    uint8 *buf;
    int p;
    int ok;
    int N;

    N = 3;
    buf = (uint8 *)malloc(SCROLL_ROWS * SCROLL_STRIDE);
    ASSERT_TRUE(buf != NULL);
    scroll_fill_rows(buf);
    data_fd2_graphics_static_bg_buffer_ptr = (uint32)buf;
    data_fd2_graphics_text_scroll_pending_line_count = (uint8)N;

    fd2_scroll_text_screen_up_by_lines(0);

    ok = 1;
    /* bottom N rows wrapped to the top */
    for (p = 0; p < N; p++) {
        if (!scroll_row_is(buf, p, (SCROLL_ROWS - N) + p)) {
            ok = 0;
        }
    }
    /* everything else shifted down by N */
    for (p = N; p < SCROLL_ROWS; p++) {
        if (!scroll_row_is(buf, p, p - N)) {
            ok = 0;
        }
    }
    ASSERT_TRUE(ok);

    free(buf);
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_text_scroll_pending_line_count = 0;
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
    RUN_TEST(test_close_no_slide);
    RUN_TEST(test_close_slide_symmetric);
    RUN_TEST(test_close_slide_asymmetric_axes);
    RUN_TEST(test_scroll_no_portrait_is_noop);
    RUN_TEST(test_backup_snapshots_region);
    RUN_TEST(test_backup_frees_prior_buffer);
    RUN_TEST(test_restore_writes_region);
    RUN_TEST(test_page_advance_collapse_corners_and_copy);
    RUN_TEST(test_page_advance_collapse_composite_gate);
    RUN_TEST(test_typewriter_confirm_returns_one);
    RUN_TEST(test_typewriter_cancel_returns_minus_one);
    RUN_TEST(test_typewriter_cursor_left_then_confirm);
    RUN_TEST(test_typewriter_cursor_right_then_confirm);
    RUN_TEST(test_typewriter_intro_emits_eight_corner_blits);
    RUN_TEST(test_typewriter_battle_gate_primes_scene);
    RUN_TEST(test_scroll_mode_a_stores_low_byte);
    RUN_TEST(test_scroll_mode_b_cylinder_n1);
    RUN_TEST(test_scroll_mode_b_cylinder_n3);
    audiofix_disable_sfx();   /* restore safe gate state for later suites */
    printf("\n");
}
