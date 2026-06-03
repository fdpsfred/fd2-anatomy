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
extern int    g_paint_portrait_calls;
extern uint32 g_paint_portrait_last_frame;

static void test_blink_frame_cycle(void)
{
    dlg_reset();
    data_fd2_dialog_portrait_blink_frame_idx = 0;
    data_fd2_dialog_portrait_blink_subtick_counter = 0;
    g_paint_portrait_calls = 0;
    g_paint_portrait_last_frame = 0xffffffffu;
    g_dlg_blink_calls = 0;

    /* Call 1: subtick 0->1, no paint */
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_subtick_counter, 1);
    ASSERT_EQ((long)g_paint_portrait_calls, 0);

    /* Call 2: subtick hits 2 -> frame 0->1, paint(1), subtick reset */
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_subtick_counter, 0);
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_frame_idx, 1);
    ASSERT_EQ((long)g_paint_portrait_calls, 1);
    ASSERT_EQ((long)g_paint_portrait_last_frame, 1);

    /* Calls 3-4: frame 1->2, paint(2) */
    fd2_portrait_blink_animation_step();
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_frame_idx, 2);
    ASSERT_EQ((long)g_paint_portrait_last_frame, 2);

    /* Calls 5-6: frame 2->3, painted value collapses 3 -> 1 */
    fd2_portrait_blink_animation_step();
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_frame_idx, 3);
    ASSERT_EQ((long)g_paint_portrait_last_frame, 1);

    /* Calls 7-8: frame 3->4 wraps to 0, paint(0) */
    fd2_portrait_blink_animation_step();
    fd2_portrait_blink_animation_step();
    ASSERT_EQ((long)data_fd2_dialog_portrait_blink_frame_idx, 0);
    ASSERT_EQ((long)g_paint_portrait_last_frame, 0);

    /* 8 calls -> 4 paints, and SFX fires once per call */
    ASSERT_EQ((long)g_paint_portrait_calls, 4);
    ASSERT_EQ((long)g_dlg_blink_calls, 8);
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
    printf("\n");
}
