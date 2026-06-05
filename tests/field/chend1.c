/*
 * unit tests for src/field/chend1.c
 *
 * fd2_chapter_01_end is the Chapter 1 end handler: a 3-step orchestrator that
 *   (1) shows chapter-end dialog page 9 via the real fd2_display_dialog_scene,
 *   (2) persists battle-runtime char state via the real
 *       fd2_save_runtime_char_to_template, and
 *   (3) sets current_chapter_id = 1.
 *
 * Both callees are the real linked functions, so this test stands up the same
 * safe in-memory fixtures their own suites use:
 *   - dialog VM: current_chapter_text points at a minimal int16 program whose
 *     page-9 header word redirects to a single glyph + END, so the real VM runs
 *     to completion using the testglob.c glyph/blink recording stubs (no VGA),
 *     with the BIOS keyboard buffer empty and the portrait latch cleared.
 *   - save-template: a zeroed runtime-char array + zeroed roster template with
 *     the roster pointer set and member_count = 1 (the save suite's baseline),
 *     so the real persistence pass and its fd2_recompute_runtime_char_total_stats
 *     callee run harmlessly.
 *
 * Asserted: the dialog VM actually ran against page 9 of current_chapter_text
 * (glyph recorder), and the chapter-id state transition committed to 1. The
 * pixel output of the dialog page is pure display and is deferred to Phase 9.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];
extern int    g_check_char_is_dead_return;

/* dialog-VM glyph recorder (testglob.c). */
extern int    g_dlg_glyph_calls;
extern uint32 g_dlg_glyph_last_idx;

/* roster template the real save-template pass copies into. */
static uint8 g_ce1_tmpl[8 * 0x50];

/* Minimal dialog program: page 9's header word (prog[9]) is a byte offset that
 * redirects cur_op to prog[10] = one glyph (0x41), prog[11] = -1 END. The VM
 * blits the single glyph then returns. */
static int16 g_ce1_text[16];

static void ce1_fixture_reset(void)
{
    int i;

    /* dialog VM safe env. */
    *(volatile uint16 *)0x41AuL = 0x20;   /* BIOS kbd buffer head == tail */
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_glyph_calls = 0;
    g_dlg_glyph_last_idx = 0;

    for (i = 0; i < 16; i++) {
        g_ce1_text[i] = 0;
    }
    g_ce1_text[9] = 20;       /* byte offset to prog[10] (page 9 start) */
    g_ce1_text[10] = 0x41;    /* one glyph */
    g_ce1_text[11] = -1;      /* END */
    current_chapter_text = (uint32)g_ce1_text;

    /* save-template safe env (save suite baseline). */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_ce1_tmpl, 0, sizeof(g_ce1_tmpl));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ce1_tmpl;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;

    /* preset chapter id to a non-target value so the transition is observable. */
    data_fd2_chapter_current_chapter_id = 0;
}

/* ----------------------------------------------------------------
 * End-to-end: the handler runs the dialog page then advances chapter id to 1.
 * The glyph recorder proves the real dialog VM was invoked on page 9 of
 * current_chapter_text (guards against a wrong text base / page index), and the
 * chapter-id write is the handler's state-transition contract.
 * ---------------------------------------------------------------- */
static void test_chapter_01_end_runs_dialog_and_advances_id(void)
{
    ce1_fixture_reset();

    fd2_chapter_01_end();

    /* page 9 redirected to a single glyph: the real VM blitted exactly it. */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_idx, (long)0x41);

    /* state transition: next chapter id committed. */
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 1L);
}

/* ----------------------------------------------------------------
 * The chapter-id write is unconditional: even if it already held a stale value,
 * the handler overwrites it with 1.
 * ---------------------------------------------------------------- */
static void test_chapter_01_end_overwrites_stale_id(void)
{
    ce1_fixture_reset();
    data_fd2_chapter_current_chapter_id = 7;   /* stale */

    fd2_chapter_01_end();

    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 1L);
}

void run_field_chend1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chend1\n");
    RUN_TEST(test_chapter_01_end_runs_dialog_and_advances_id);
    RUN_TEST(test_chapter_01_end_overwrites_stale_id);
    printf("\n");
}
