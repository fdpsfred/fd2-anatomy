/*
 * unit tests for src/field/chevt2.c (part 7)
 *
 * fd2_chapter_event_handler_54__ch27_ai_ctrl @ 0x360C0
 *
 * ch27 AI-setup handler. Functionally-exact body is a single call:
 *     fd2_set_combat_aux_block_byte_d_low4_for_char_range(
 *         0x10, data_fd2_battle_party_member_count - 1, 0);
 * i.e. clear the low nibble (high nibble preserved) of
 * runtime_char.combat_aux_block[0xD] (absolute offset 0x34) to 0 for the
 * inclusive char range 0x10 .. (party_member_count - 1).
 *
 * Risk-bearing (numeric end-index computation + range state mutation):
 *   (a) the END index is DYNAMIC = party_member_count - 1, NOT a fixed literal:
 *       changing the party count changes exactly which chars are disarmed,
 *   (b) the START index is the fixed literal 0x10 (the borrowed shared tail's
 *       PUSH 0x10), so chars below 0x10 are never touched,
 *   (c) only the LOW nibble is written to 0; the HIGH nibble of byte 0x34 is
 *       preserved (AND 0xF0 ... OR 0 in the callee),
 *   (d) the inclusive boundaries are exact: char (party_member_count - 1) IS
 *       disarmed, char (party_member_count) just past the end is NOT,
 *   (e) the empty-range edge: party_member_count == 0 -> end = (uint)-1 =
 *       0xFFFFFFFF, which is below the start 0x10 under the callee's SIGNED
 *       inclusive comparison ((int)param_1 <= (int)param_2), so NO char is
 *       touched (this is the contrast that proves the end is unsigned-subtracted
 *       then signed-compared, not clamped),
 *   (f) the dispatch arg is ignored (passed nonzero).
 *
 * Drives the REAL handler and its REAL callee
 * (fd2_set_combat_aux_block_byte_d_low4_for_char_range) against a local
 * runtime_char array large enough to cover the highest index this suite uses
 * (the shared g_test_rc_array[8] is far too small for index 0x10+). No game
 * files, no display.
 */

#include <string.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];

#define CE54_NCHARS   0x40       /* covers every index this suite reads */
#define CE54_AI_OFF   0x34       /* combat_aux_block[0xD] absolute offset */

static runtime_char g_ce54_rc[CE54_NCHARS];

/* offset 0x34 of char `idx`, read as a raw byte */
static uint8 ce54_ai(int idx)
{
    return ((uint8 *)&g_ce54_rc[idx])[CE54_AI_OFF];
}

/* Seed offset 0x34 of every char with a non-zero HIGH nibble (0xA0) and a
 * non-zero LOW nibble (0x05) -> 0xA5, so an in-range write (low nibble -> 0,
 * high nibble preserved) yields 0xA0 while an untouched char keeps 0xA5. */
static void ce54_setup(uint32 party_count)
{
    int i;

    memset(g_ce54_rc, 0, sizeof(g_ce54_rc));
    for (i = 0; i < CE54_NCHARS; i++) {
        ((uint8 *)&g_ce54_rc[i])[CE54_AI_OFF] = 0xA5;
    }
    data_fd2_battle_runtime_char_array_ptr = g_ce54_rc;
    data_fd2_battle_party_member_count = party_count;
}

static void ce54_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
}

/* ----------------------------------------------------------------
 * The disarmed range is exactly 0x10 .. (party_member_count - 1) inclusive, the
 * low nibble cleared to 0 with the high nibble preserved. Seed party_count =
 * 0x20: chars 0x10..0x1F become 0xA0 (low nibble cleared), the char at the
 * inclusive end (0x1F) IS written, the char just past it (0x20) is NOT, and
 * char 0xF just below the fixed start is NOT. The dispatch arg is passed nonzero
 * to prove it is ignored.
 * ---------------------------------------------------------------- */
static void test_h54_disarms_dynamic_range_low_nibble_only(void)
{
    int i;

    ce54_setup(0x20);

    fd2_chapter_event_handler_54__ch27_ai_ctrl(0x77);

    /* every char in 0x10 .. 0x1F had its low nibble cleared, high nibble kept */
    for (i = 0x10; i <= 0x1F; i++) {
        ASSERT_EQ((long)ce54_ai(i), 0xA0);
    }
    /* inclusive end is exactly party_member_count - 1 = 0x1F (covered above) and
     * the char one past the end (0x20 = party_member_count) is untouched */
    ASSERT_EQ((long)ce54_ai(0x20), 0xA5);
    /* the fixed start is 0x10: the char just below it (0xF) is untouched */
    ASSERT_EQ((long)ce54_ai(0x0F), 0xA5);

    ce54_teardown();
}

/* ----------------------------------------------------------------
 * The range END is DYNAMIC (party_member_count - 1), not a fixed literal: a
 * different party count moves the end. Seed party_count = 0x14 (smaller): now
 * only 0x10..0x13 are disarmed, and 0x14 (the new just-past-end) plus everything
 * above stays 0xA5. Run side by side with the 0x20 case above (different end
 * index) this pins the end to party_member_count - 1.
 * ---------------------------------------------------------------- */
static void test_h54_end_index_tracks_party_count(void)
{
    int i;

    ce54_setup(0x14);

    fd2_chapter_event_handler_54__ch27_ai_ctrl(0);

    /* disarmed range shrank to 0x10 .. 0x13 */
    for (i = 0x10; i <= 0x13; i++) {
        ASSERT_EQ((long)ce54_ai(i), 0xA0);
    }
    /* char 0x14 (= party_member_count) and beyond are untouched */
    ASSERT_EQ((long)ce54_ai(0x14), 0xA5);
    ASSERT_EQ((long)ce54_ai(0x1F), 0xA5);
    ASSERT_EQ((long)ce54_ai(0x20), 0xA5);

    ce54_teardown();
}

/* ----------------------------------------------------------------
 * Single-char range edge: party_count = 0x11 -> end = 0x10 = start, so EXACTLY
 * one char (0x10) is disarmed and char 0x11 (= party_member_count) is not. This
 * pins the inclusive lower boundary (start itself is written) and proves the end
 * computation handles the start == end case.
 * ---------------------------------------------------------------- */
static void test_h54_single_char_range_when_count_is_start_plus_1(void)
{
    ce54_setup(0x11);

    fd2_chapter_event_handler_54__ch27_ai_ctrl(0x33);

    /* exactly char 0x10 disarmed */
    ASSERT_EQ((long)ce54_ai(0x10), 0xA0);
    /* char 0x11 (= party_member_count) untouched */
    ASSERT_EQ((long)ce54_ai(0x11), 0xA5);
    /* char 0xF below the start untouched */
    ASSERT_EQ((long)ce54_ai(0x0F), 0xA5);

    ce54_teardown();
}

/* ----------------------------------------------------------------
 * Empty-range edge: party_count = 0 -> end = (uint)0 - 1 = 0xFFFFFFFF. Under the
 * callee's SIGNED inclusive comparison ((int)0x10 <= (int)0xFFFFFFFF -> 16 <= -1
 * is FALSE) the loop body never runs, so NO char is touched. This is the
 * defining contrast: a clamped or unsigned-compared end would instead disarm a
 * huge range. Every seeded byte must remain 0xA5.
 * ---------------------------------------------------------------- */
static void test_h54_empty_range_when_party_count_zero(void)
{
    int i;

    ce54_setup(0);

    fd2_chapter_event_handler_54__ch27_ai_ctrl(0x55);

    /* nothing was disarmed: the whole array keeps its 0xA5 seed */
    for (i = 0; i < CE54_NCHARS; i++) {
        ASSERT_EQ((long)ce54_ai(i), 0xA5);
    }

    ce54_teardown();
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_55__unref_sentinel @ 0x360D8
 *
 * Pure no-op sentinel: the binary body is PUSH 4; CALL __CHK; RET — a stack
 * probe then an immediate return, with NO state mutation at all. Unlike the
 * consumed-flag sentinels (handler_49 / 4d / 4e) which each write one
 * consumed_flags byte, this handler writes nothing.
 *
 * The risk here is purely the "does nothing" contract plus cdecl stack
 * discipline (bare RET, caller cleans the arg). This test drives the REAL
 * handler with a fully seeded consumed_flags region and a sentinel
 * battle_anim_phase, then asserts that EVERY byte of both is byte-identical
 * afterwards — i.e. the handler had no observable side effect — and that it
 * returns cleanly (a wrong RET width / arg read would corrupt the stack and
 * crash here, not silently pass). The dispatch arg is passed nonzero to prove
 * it is ignored.
 * ---------------------------------------------------------------- */
#define CE55_FLAGS_LEN  0x20      /* covers every flag slot any sibling uses */

static uint8 g_ce55_flags[CE55_FLAGS_LEN];

static void test_h55_pure_noop_mutates_nothing(void)
{
    uint8 flags_before[CE55_FLAGS_LEN];
    uint32 saved_flags_ptr;
    uint32 saved_anim_phase;
    int i;

    /* seed the consumed-flag region with a recognisable non-zero pattern */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        g_ce55_flags[i] = (uint8)(0x80 | i);
    }
    memcpy(flags_before, g_ce55_flags, sizeof(flags_before));

    saved_flags_ptr = data_fd2_field_map_tile_event_consumed_flags_ptr;
    saved_anim_phase = data_fd2_battle_anim_phase;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce55_flags;
    data_fd2_battle_anim_phase = 0x5A5A5A5A;

    fd2_chapter_event_handler_55__unref_sentinel(0x77);

    /* no consumed-flag byte was touched (contrast: handler_49/4d/4e write one) */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        ASSERT_EQ((long)g_ce55_flags[i], (long)flags_before[i]);
    }
    /* battle_anim_phase is likewise untouched */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, (long)0x5A5A5A5A);

    data_fd2_field_map_tile_event_consumed_flags_ptr = saved_flags_ptr;
    data_fd2_battle_anim_phase = saved_anim_phase;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_56__unref_sentinel @ 0x360E3
 *
 * Pure no-op sentinel, functionally identical to handler_55. The binary body is
 * a 7-byte stub: PUSH 4; JMP 0x360DD — it borrows handler_55's CALL __CHK; RET
 * shared tail (it is one of the four consumers 56/57/58/59 of that tail). The net
 * effect is a stack probe then an immediate return, with NO state mutation at all
 * (not even a consumed_flags byte, unlike handler_49 / 4d / 4e).
 *
 * Same risk profile as handler_55: the "does nothing" contract plus cdecl stack
 * discipline through the BORROWED tail (the bare RET lives in handler_55; a wrong
 * frame size pushed before the JMP, or an arg read, would corrupt the stack on
 * return and crash here rather than silently pass). This test drives the REAL
 * handler with a fully seeded consumed_flags region and a sentinel
 * battle_anim_phase, then asserts every byte of both is byte-identical afterwards
 * and that it returns cleanly. The dispatch arg is passed nonzero to prove it is
 * ignored. A distinct seed pattern from the handler_55 test keeps this case
 * self-contained.
 * ---------------------------------------------------------------- */
static void test_h56_pure_noop_mutates_nothing(void)
{
    uint8 flags_before[CE55_FLAGS_LEN];
    uint32 saved_flags_ptr;
    uint32 saved_anim_phase;
    int i;

    /* seed with a different recognisable non-zero pattern than the h55 test */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        g_ce55_flags[i] = (uint8)(0xC0 ^ i);
    }
    memcpy(flags_before, g_ce55_flags, sizeof(flags_before));

    saved_flags_ptr = data_fd2_field_map_tile_event_consumed_flags_ptr;
    saved_anim_phase = data_fd2_battle_anim_phase;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce55_flags;
    data_fd2_battle_anim_phase = 0xA3A3A3A3;

    fd2_chapter_event_handler_56__unref_sentinel(0x99);

    /* no consumed-flag byte was touched */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        ASSERT_EQ((long)g_ce55_flags[i], (long)flags_before[i]);
    }
    /* battle_anim_phase is likewise untouched */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, (long)0xA3A3A3A3);

    data_fd2_field_map_tile_event_consumed_flags_ptr = saved_flags_ptr;
    data_fd2_battle_anim_phase = saved_anim_phase;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_57__unref_sentinel @ 0x360EA
 *
 * Pure no-op sentinel, functionally identical to handler_55 / handler_56. The
 * binary body is a 7-byte stub: PUSH 4; JMP 0x360DD — it borrows handler_55's
 * CALL __CHK; RET shared tail (it is one of the four consumers 56/57/58/59 of
 * that tail). The net effect is a stack probe then an immediate return, with NO
 * state mutation at all (not even a consumed_flags byte, unlike handler_49 / 4d /
 * 4e).
 *
 * Same risk profile as handler_55 / 56: the "does nothing" contract plus cdecl
 * stack discipline through the BORROWED tail (the bare RET lives in handler_55; a
 * wrong frame size pushed before the JMP, or an arg read, would corrupt the stack
 * on return and crash here rather than silently pass). This test drives the REAL
 * handler with a fully seeded consumed_flags region and a sentinel
 * battle_anim_phase, then asserts every byte of both is byte-identical afterwards
 * and that it returns cleanly. The dispatch arg is passed nonzero to prove it is
 * ignored. A distinct seed pattern from the handler_55 / 56 tests keeps this case
 * self-contained.
 * ---------------------------------------------------------------- */
static void test_h57_pure_noop_mutates_nothing(void)
{
    uint8 flags_before[CE55_FLAGS_LEN];
    uint32 saved_flags_ptr;
    uint32 saved_anim_phase;
    int i;

    /* seed with yet another recognisable non-zero pattern, distinct from h55/h56 */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        g_ce55_flags[i] = (uint8)(0x33 + i);
    }
    memcpy(flags_before, g_ce55_flags, sizeof(flags_before));

    saved_flags_ptr = data_fd2_field_map_tile_event_consumed_flags_ptr;
    saved_anim_phase = data_fd2_battle_anim_phase;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce55_flags;
    data_fd2_battle_anim_phase = 0x6C6C6C6C;

    fd2_chapter_event_handler_57__unref_sentinel(0xAB);

    /* no consumed-flag byte was touched */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        ASSERT_EQ((long)g_ce55_flags[i], (long)flags_before[i]);
    }
    /* battle_anim_phase is likewise untouched */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, (long)0x6C6C6C6C);

    data_fd2_field_map_tile_event_consumed_flags_ptr = saved_flags_ptr;
    data_fd2_battle_anim_phase = saved_anim_phase;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_58__unref_sentinel @ 0x360F1
 *
 * Pure no-op sentinel, functionally identical to handler_55 / 56 / 57. The
 * binary body is a 7-byte stub: PUSH 4; JMP 0x360DD — it borrows handler_55's
 * CALL __CHK; RET shared tail (it is one of the four consumers 56/57/58/59 of
 * that tail). The net effect is a stack probe then an immediate return, with NO
 * state mutation at all (not even a consumed_flags byte, unlike handler_49 / 4d /
 * 4e).
 *
 * Same risk profile as handler_55 / 56 / 57: the "does nothing" contract plus
 * cdecl stack discipline through the BORROWED tail (the bare RET lives in
 * handler_55; a wrong frame size pushed before the JMP, or an arg read, would
 * corrupt the stack on return and crash here rather than silently pass). This
 * test drives the REAL handler with a fully seeded consumed_flags region and a
 * sentinel battle_anim_phase, then asserts every byte of both is byte-identical
 * afterwards and that it returns cleanly. The dispatch arg is passed nonzero to
 * prove it is ignored. A distinct seed pattern from the handler_55 / 56 / 57
 * tests keeps this case self-contained.
 * ---------------------------------------------------------------- */
static void test_h58_pure_noop_mutates_nothing(void)
{
    uint8 flags_before[CE55_FLAGS_LEN];
    uint32 saved_flags_ptr;
    uint32 saved_anim_phase;
    int i;

    /* seed with a recognisable non-zero pattern distinct from h55/h56/h57 */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        g_ce55_flags[i] = (uint8)(0x5A - i);
    }
    memcpy(flags_before, g_ce55_flags, sizeof(flags_before));

    saved_flags_ptr = data_fd2_field_map_tile_event_consumed_flags_ptr;
    saved_anim_phase = data_fd2_battle_anim_phase;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce55_flags;
    data_fd2_battle_anim_phase = 0x1E1E1E1E;

    fd2_chapter_event_handler_58__unref_sentinel(0xCD);

    /* no consumed-flag byte was touched */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        ASSERT_EQ((long)g_ce55_flags[i], (long)flags_before[i]);
    }
    /* battle_anim_phase is likewise untouched */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, (long)0x1E1E1E1E);

    data_fd2_field_map_tile_event_consumed_flags_ptr = saved_flags_ptr;
    data_fd2_battle_anim_phase = saved_anim_phase;
}

/* ----------------------------------------------------------------
 * fd2_chapter_event_handler_59__unref_sentinel @ 0x360F8
 *
 * Pure no-op sentinel, functionally identical to handler_55 / 56 / 57 / 58. The
 * binary body is a 7-byte stub: PUSH 4; JMP 0x360DD — it borrows handler_55's
 * CALL __CHK; RET shared tail (it is the LAST of the four consumers 56/57/58/59
 * of that tail). The net effect is a stack probe then an immediate return, with
 * NO state mutation at all (not even a consumed_flags byte, unlike handler_49 /
 * 4d / 4e).
 *
 * Same risk profile as handler_55 / 56 / 57 / 58: the "does nothing" contract
 * plus cdecl stack discipline through the BORROWED tail (the bare RET lives in
 * handler_55; a wrong frame size pushed before the JMP, or an arg read, would
 * corrupt the stack on return and crash here rather than silently pass). This
 * test drives the REAL handler with a fully seeded consumed_flags region and a
 * sentinel battle_anim_phase, then asserts every byte of both is byte-identical
 * afterwards and that it returns cleanly. The dispatch arg is passed nonzero to
 * prove it is ignored. A distinct seed pattern from the handler_55 / 56 / 57 / 58
 * tests keeps this case self-contained.
 * ---------------------------------------------------------------- */
static void test_h59_pure_noop_mutates_nothing(void)
{
    uint8 flags_before[CE55_FLAGS_LEN];
    uint32 saved_flags_ptr;
    uint32 saved_anim_phase;
    int i;

    /* seed with a recognisable non-zero pattern distinct from h55/h56/h57/h58 */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        g_ce55_flags[i] = (uint8)(0x91 + i * 3);
    }
    memcpy(flags_before, g_ce55_flags, sizeof(flags_before));

    saved_flags_ptr = data_fd2_field_map_tile_event_consumed_flags_ptr;
    saved_anim_phase = data_fd2_battle_anim_phase;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce55_flags;
    data_fd2_battle_anim_phase = 0x7B7B7B7B;

    fd2_chapter_event_handler_59__unref_sentinel(0xEF);

    /* no consumed-flag byte was touched */
    for (i = 0; i < CE55_FLAGS_LEN; i++) {
        ASSERT_EQ((long)g_ce55_flags[i], (long)flags_before[i]);
    }
    /* battle_anim_phase is likewise untouched */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, (long)0x7B7B7B7B);

    data_fd2_field_map_tile_event_consumed_flags_ptr = saved_flags_ptr;
    data_fd2_battle_anim_phase = saved_anim_phase;
}

void run_field_chevt27_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 7)\n");
    RUN_TEST(test_h54_disarms_dynamic_range_low_nibble_only);
    RUN_TEST(test_h54_end_index_tracks_party_count);
    RUN_TEST(test_h54_single_char_range_when_count_is_start_plus_1);
    RUN_TEST(test_h54_empty_range_when_party_count_zero);
    RUN_TEST(test_h55_pure_noop_mutates_nothing);
    RUN_TEST(test_h56_pure_noop_mutates_nothing);
    RUN_TEST(test_h57_pure_noop_mutates_nothing);
    RUN_TEST(test_h58_pure_noop_mutates_nothing);
    RUN_TEST(test_h59_pure_noop_mutates_nothing);
    printf("\n");
}
