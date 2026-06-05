/*
 * unit tests for src/field/chevt2.c (part 3)
 *
 * fd2_cinematic_chapter_portrait_dump_with_white_flash @ 0x35822 — the chapter
 * portrait cinematic with a white-flash transition. Its functionally-exact body
 * is the fixed call sequence:
 *     fd2_pan_cursor_and_window(target_tile_x, target_tile_y)
 *     fd2_load_chapter_portraits_and_dump_tmp(chapter_id & 0xFF)
 *     __delay_thunk_375b2(300)
 *     fd2_set_vga_palette_range_with_add(0, 0xFF, 0xFF)    -- pure-white flash
 *     __delay_thunk_375b2(200)
 *     fd2_set_vga_palette_range_with_add(0, 0xFF, 0)       -- restore
 *     fd2_composite_battle_frame(0)
 *     fd2_delay_400ms_via_idle_thunk()                     -- JMP tail-call
 *
 * The risk-bearing (non-display) contract pinned here:
 *   (a) the two stack args (target_tile_x, target_tile_y) reach the real pan in
 *       the right order -> the battle window origin lands exactly on the target,
 *   (b) chapter_id is truncated to its LOW BYTE (binary MOVZX EAX, byte ptr) and
 *       forwarded to the real portrait loader -> a tile-event record whose race
 *       byte equals (chapter_id & 0xFF) inits exactly one runtime_char, and
 *   (c) the white-flash delay sequence is exactly 300, 200, 400 in order (the
 *       last via the fd2_delay_400ms_via_idle_thunk tail-call).
 *
 * These drive the REAL function and its REAL callees over the staged real game
 * files (the portrait loader re-reads FDFIELD.DAT and rewrites the FD2.TMP swap
 * file). Host-safety recipe mirrors the proven chevt2 part-1/part-2 suites:
 *   - the portrait-loader tile-event scan length is the in-process global
 *     data_fd2_resource_portrait_cache_alloc_offset over an in-process
 *     tile-event table (race byte at record+0x98, stride 0x1A); chapter 4 so the
 *     loader re-reads the real FDFIELD.DAT[4*3+2],
 *   - a host-safe render workspace + sprite atlas back the real pan composites
 *     and the final composite (the tile-map blit is the testglob recorder
 *     g_composite_call_count; the per-char overlay iterates only the chars the
 *     loader inited),
 *   - a full 768-byte palette buffer backs the two real
 *     fd2_set_vga_palette_range_with_add white-flash writes (256 DAC entries),
 *   - __delay_thunk_375b2 is the testglob recorder; the opt-in g_delay375b2_log
 *     captures the exact 300/200/400 tick sequence, and the
 *     fd2_delay_400ms_via_idle_thunk testglob stub forwards 400 into it.
 * The palette-port writes and composited pixels are pure display side effects
 * owned by the palette / rndscene suites; they execute for real here only as a
 * byproduct and are not asserted (deferred to Phase 9 integration).
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

extern runtime_char g_test_rc_array[8];

/* testglob recorders */
extern int    g_composite_call_count;
extern int    g_delay375b2_log_on;
extern int    g_delay375b2_log_count;
extern uint32 g_delay375b2_log[16];

/* ---- portrait-loader fixture (mirrors chevt2 ce_setup_portrait_env): a
 * tile-event table of `count` records (stride 0x1A) whose race bytes (+0x98) are
 * race_of[k]; alloc_offset = count drives the scan length. Chapter 4 -> the real
 * loader re-reads real FDFIELD.DAT[4*3+2 = 0xE]. */
static uint8 *g_ce23_tileevent;

/* ---- host-safe render workspace + sprite atlas + 768-byte palette for the real
 * pan composites, the two white-flash palette writes, and the final composite. */
#define CE23_WS_SPAN (191u * 0x1c8u + 0x138u)
static uint8 g_ce23_ws[CE23_WS_SPAN];
static uint8 g_ce23_atlas[6 + 64 * 4 + 4];
static uint8 g_ce23_palette[256 * 3];

/* Stand up the full real-cinematic env. `count`/`races` drive the portrait-id
 * observation; the window origin starts at (start_ox, start_oy) so the pan target
 * is observable on the final origin. */
static void ce23_setup(int count, const uint8 *races,
                       uint32 start_ox, uint32 start_oy)
{
    int i;
    uint32 *atlas_tbl;

    /* --- portrait loader env --- */
    g_ce23_tileevent =
        (uint8 *)malloc((size_t)0x98 + (size_t)count * 0x1a + 0x20);
    memset(g_ce23_tileevent, 0, (size_t)0x98 + (size_t)count * 0x1a + 0x20);
    for (i = 0; i < count; i++) {
        g_ce23_tileevent[i * 0x1a + 0x98] = races[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce23_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = (uint32)count;
    chapter_portrait_load_buffer = 0;            /* loaded fresh by the loader  */
    data_fd2_chapter_init_phase_flag = 1;        /* spawn = field value verbatim */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_party_member_count = 0;
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE    */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    /* --- render env for pan composites + final composite --- */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce23_ws - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_view_window_origin_x = start_ox;
    data_fd2_battle_view_window_origin_y = start_oy;
    data_fd2_battle_anim_phase = 1;
    atlas_tbl = (uint32 *)(g_ce23_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce23_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    /* --- 768-byte palette for the two fd2_set_vga_palette_range_with_add calls -- */
    for (i = 0; i < 256 * 3; i++) {
        g_ce23_palette[i] = 0x20;
    }
    data_fd2_vga_palette_data_ptr = (uint32)g_ce23_palette;

    /* --- delay-tick log + composite counter --- */
    g_delay375b2_log_on = 1;
    g_delay375b2_log_count = 0;
    g_composite_call_count = 0;
}

static void ce23_teardown(void)
{
    free(g_ce23_tileevent);
    g_ce23_tileevent = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    g_delay375b2_log_on = 0;
    g_delay375b2_log_count = 0;
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}

/* ----------------------------------------------------------------
 * Full sequence: drive the cinematic with (x=9, y=7, chapter_id=4). The two
 * stack args land the window origin exactly on (9, 7) (window starts away on both
 * axes so the pan is visible on each); the white-flash delay sequence is exactly
 * 300, 200, 400 (the last via the fd2_delay_400ms_via_idle_thunk tail-call); the
 * portrait loader inits the sole race-4 record (chapter_id 4 forwarded); and the
 * cinematic composited frames (pan steps + the final composite).
 * ---------------------------------------------------------------- */
static void test_white_flash_full_sequence(void)
{
    static const uint8 races[1] = { 4 };       /* race == chapter_id 4 */

    ce23_setup(1, races, 0x40, 0x40);

    fd2_cinematic_chapter_portrait_dump_with_white_flash(9, 7, 4);

    /* (a) pan landed the window origin on the literal target (9, 7) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 9);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 7);
    /* (c) the white-flash delay sequence is exactly 300, 200, 400 */
    ASSERT_EQ((long)g_delay375b2_log_count, 3);
    ASSERT_EQ((long)g_delay375b2_log[0], 300);
    ASSERT_EQ((long)g_delay375b2_log[1], 200);
    ASSERT_EQ((long)g_delay375b2_log[2], 400);
    /* (b) chapter_id 4 forwarded to the loader -> the race-4 record inited */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* the cinematic composited frames (pan steps + final composite) */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* the loader freed + nulled its scratch buffer */
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);

    ce23_teardown();
}

/* ----------------------------------------------------------------
 * chapter_id is forwarded as its LOW BYTE only (binary MOVZX EAX, byte ptr
 * [ESP+0xc]). Driving with chapter_id = 0x105 must behave identically to 0x05:
 * the loader's race-scan compares the full 32-bit (chapter_id & 0xFF) against
 * each record's byte race, so only the race-5 record matches. A race-1 decoy
 * (the low nibble of 0x105 if a wrong byte were taken) must NOT match. count == 1
 * proves the &0xFF truncation: WITHOUT the mask the loader would compare 0x105 to
 * the byte races (max 0xFF) and match nothing (count 0).
 * ---------------------------------------------------------------- */
static void test_white_flash_chapter_id_low_byte_only(void)
{
    static const uint8 races[2] = { 5, 1 };    /* target 5, decoy 1 */

    ce23_setup(2, races, 0x10, 0x10);

    fd2_cinematic_chapter_portrait_dump_with_white_flash(3, 4, 0x105);

    /* (0x105 & 0xFF) == 5 matched the race-5 record; the decoy (1) did not, and
     * an unmasked 0x105 would have matched nothing */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    /* the args still landed the pan and ran one delay triple */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 3);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 4);
    ASSERT_EQ((long)g_delay375b2_log_count, 3);

    ce23_teardown();
}

/* ----------------------------------------------------------------
 * The pan target is the two forwarded args, not a hardcoded constant: a second,
 * distinct target (2, 0xB) (handler_34's first-call coords) lands the window
 * origin on (2, 0xB). Combined with the (9, 7) case above this proves both the
 * x and y args are forwarded (not fixed). A race that does not match any id keeps
 * the loader scan a no-op so the focus stays on the pan.
 * ---------------------------------------------------------------- */
static void test_white_flash_pan_target_is_args_not_constant(void)
{
    static const uint8 races[1] = { 0x7F };    /* never equals chapter_id 0 */

    ce23_setup(1, races, 0x20, 0x20);

    fd2_cinematic_chapter_portrait_dump_with_white_flash(2, 0xB, 0);

    /* origin landed on the distinct literal target (2, 0xB) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 2);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0xB);
    /* chapter_id 0 matched no record (race 0x7F) -> loader scan was a no-op */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 0);
    /* still one white-flash delay triple */
    ASSERT_EQ((long)g_delay375b2_log_count, 3);

    ce23_teardown();
}

void run_field_chevt23_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 3)\n");
    RUN_TEST(test_white_flash_full_sequence);
    RUN_TEST(test_white_flash_chapter_id_low_byte_only);
    RUN_TEST(test_white_flash_pan_target_is_args_not_constant);
    printf("\n");
}
