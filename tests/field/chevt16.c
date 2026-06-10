/*
 * unit tests for src/field/chevt1.c (part 6: handler 2D)
 *
 * The chapter turn-event handlers in src/field/chevt1.c are dispatched as
 * indices of the per-event handler table at 0x51B91. Parts 1..5 (chevt11.c /
 * chevt12.c / chevt13.c / chevt14.c / chevt15.c) cover the other handlers; this
 * part covers handler 2D (a chapter-19 AI-control beat).
 *
 * handler 2D — fd2_chapter_event_handler_2d__ch19_ai_ctrl @ 0x350B9
 *   chapter 19 AI-control beat, dispatch idx 0x2D of that table. Its body is a
 *   straight-line single call (no dialog, no RNG, no numeric computation, no
 *   CALL-return value used):
 *     - the per-event AI/dialog control flag (low nibble of combat_aux_block[0xD],
 *       struct offset 0x34) is set to 3 for the runtime-char range [0x10, 0x1F]
 *       inclusive (16 chars) via the real range setter
 *       fd2_set_combat_aux_block_byte_d_low4_for_char_range (which masks the byte
 *       to (old & 0xF0) | new_val, so only the low nibble moves and the high
 *       nibble survives).
 *
 * In the original binary this handler pre-pushes the flag (3) and end (0x1F)
 * args and JMPs (0x350C7 -> 0x34F37) into the class-3 shared tail hosted by
 * handler 12, which supplies the fixed start arg 0x10 — the same borrowed tail
 * handler 2B uses, so the effect is the 0x2B single-char write generalised to
 * the [0x10, 0x1F] range.
 *
 * The single callee is REAL. The writes span indices 0x10..0x1F, all inside the
 * shared g_ev_rc[0x48] fixture (max index 0x47), so no private oversized array is
 * needed and no dialog program / glyph recorder is involved. The distinguishing
 * feature is the 16-char [0x10, 0x1F] range, so the boundary guards (chars 0x0F
 * below and 0x20 above) are the load-bearing assertions.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "fieldfix.h"

/* ================================================================
 * handler 2D — fd2_chapter_event_handler_2d__ch19_ai_ctrl @ 0x350B9
 * ================================================================ */

/* Seed combat_aux_block[0xD] of every touched char (and the boundary guards) to
 * 0xA5 (high nibble 0xA must survive, low nibble 0x5 must move to 3), and the
 * in-slot neighbour bytes [0xC]/[0xE] to distinct sentinels so the low-nibble-
 * only write is independently witnessed. */
static void ev2d_seed(void)
{
    int i;

    for (i = 0x10; i <= 0x1F; i++) {
        g_ev_rc[i].combat_aux_block[0xD] = 0xA5;
        g_ev_rc[i].combat_aux_block[0xC] = 0xCC;   /* in-slot neighbour guard */
        g_ev_rc[i].combat_aux_block[0xE] = 0xEE;   /* in-slot neighbour guard */
    }

    /* chars just outside the range: must stay at their seeded sentinel. */
    g_ev_rc[0x0F].combat_aux_block[0xD] = 0xA5;    /* below range */
    g_ev_rc[0x20].combat_aux_block[0xD] = 0xA5;    /* above range */
}

/* ----------------------------------------------------------------
 * The handler fires its one beat: set the AI flag of chars 0x10..0x1F
 * ([0xD] low nibble 3). All deterministic and observable; checked in one pass.
 * ---------------------------------------------------------------- */
static void test_ch19_event2d_sets_ai_flag3_range_0x10_to_0x1f(void)
{
    /* ev_install_safe_env zeroes g_ev_rc and points the runtime-char pointer at
     * it; this handler only needs that pointer (it never touches the dialog VM). */
    ev_install_safe_env();
    ev2d_seed();

    fd2_chapter_event_handler_2d__ch19_ai_ctrl(0);

    /* (1) combat_aux_block[0xD] low nibble set to 3, high nibble preserved
     * (0xA5 -> 0xA3) across the whole range — boundaries + an interior char. */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xD], (long)0xA3);
    ASSERT_EQ((long)g_ev_rc[0x17].combat_aux_block[0xD], (long)0xA3);
    ASSERT_EQ((long)g_ev_rc[0x1F].combat_aux_block[0xD], (long)0xA3);

    /* (2) range boundaries are exact: the chars just below (0x0F) and above
     * (0x20) keep their seeded sentinel — load-bearing for [0x10, 0x1F]. */
    ASSERT_EQ((long)g_ev_rc[0x0F].combat_aux_block[0xD], (long)0xA5);
    ASSERT_EQ((long)g_ev_rc[0x20].combat_aux_block[0xD], (long)0xA5);

    /* (3) in-slot neighbour bytes survive: the low-nibble-only [0xD] write did
     * not touch [0xC] or [0xE]. */
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev_rc[0x10].combat_aux_block[0xE], (long)0xEE);
    ASSERT_EQ((long)g_ev_rc[0x1F].combat_aux_block[0xC], (long)0xCC);
    ASSERT_EQ((long)g_ev_rc[0x1F].combat_aux_block[0xE], (long)0xEE);

    ev_restore_rc_ptr();
}

void run_field_chevt16_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt16\n");
    RUN_TEST(test_ch19_event2d_sets_ai_flag3_range_0x10_to_0x1f);
    printf("\n");
}
