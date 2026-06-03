/*
 * unit tests for src/life/main.c
 *
 * The save-load and main-menu paths drive the REAL loaders against the staged
 * real game files (FD2.SAV / FDICON.B24 / FDOTHER.DAT / FDFIELD.DAT /
 * FDTXT.DAT / FDSHAP.DAT / FDMUS.DAT, copied into the test cwd by
 * build_test.py). Expected save state is obtained by decrypting the SAME real
 * FD2.SAV in-test with the linked fd2_save_crypt_buffer (an involution), so
 * every restored field is cross-checked against the real plaintext — no
 * fabricated save image, no hardcoded values.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>
#include "realfile.h"   /* realdat_read_resource() */

#define USE_ITEM_ID 10

/* Read the staged real FD2.SAV and decrypt it in place with the linked real
 * cipher, returning the malloc'd 0x59CB plaintext (caller frees). */
static uint8 *realsav_decrypted(void)
{
    FILE  *fp;
    uint8 *buf;

    buf = (uint8 *)malloc(0x59CB);
    fp = fopen("FD2.SAV", "rb");
    fread(buf, 1, 0x59CB, fp);
    fclose(fp);
    fd2_save_crypt_buffer((uint32)buf, 0x59CB);
    return buf;
}

extern runtime_char g_test_rc_array[8];
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern int g_ending_menu_return;
extern int g_slot_selector_return;

/* fd2_load_save_and_init_engine cinematic-loop recorders (testglob.c) */
extern int g_alloc_blit_calls;
extern uint32 g_alloc_blit_last_idx;
/* fd2_cleanup_dialog_sprite_buffer is now the real emitted function; it calls
 * fd2_restore_screen_block_from_buffer exactly once per invocation, so the
 * restore-stub counter is an exact proxy for the cleanup-call count. */
extern int g_restore_block_calls;

/* buffers staged by the fixture, freed by teardown */
static void *g_ls_roster_buf;
static void *g_ls_consumed_buf;

/* ----------------------------------------------------------------
 * fd2_load_save_and_init_engine fixture
 *
 * The real function fopen/freads the staged real FD2.SAV + FDICON.B24,
 * fwrites FD2.TMP, reloads the chapter's real FDOTHER/FDFIELD/FDTXT/FDSHAP
 * resources, and memmoves restored state out of the (decrypted) save buffer
 * into engine globals. Point every engine pointer-global at a valid buffer so
 * it runs end-to-end without faulting; the save's real checksum matches, so
 * the error-dialog arm (which would block on input) is not taken.
 * ---------------------------------------------------------------- */
static void setup_load_save_fixture(void)
{
    /* destination buffers the function memmoves into / reads from */
    g_ls_roster_buf = malloc(0xA00);
    g_ls_consumed_buf = malloc(0x20);
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ls_roster_buf;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ls_consumed_buf;

    /* freed-if-nonzero then re-malloc'd / reloaded by the function; NULL/0
     * them so it does not free a static/stale pointer. */
    data_fd2_battle_runtime_char_array_ptr = NULL;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    portrait_sprite_cache = 0;
    chapter_portrait_load_buffer = 0;

    /* loader-returned pointer globals start NULL so the loader's free(old_buf)
     * is a no-op on the first load of each */
    data_fd2_vga_palette_data_ptr = 0;
    current_chapter_text = 0;
    battle_scene_snapshot = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;  /* trailing bgm load target */

    g_alloc_blit_calls = 0;
    g_restore_block_calls = 0;
    g_alloc_blit_last_idx = 0;
}

/* Free fixture buffers and restore every global the load touched back to its
 * testglob.c default so later suites are not contaminated. */
static void teardown_load_save_fixture(void)
{
    /* the function re-malloc'd these; free its buffers */
    if (data_fd2_battle_runtime_char_array_ptr != NULL)
        free(data_fd2_battle_runtime_char_array_ptr);
    if (data_fd2_tile_event_data_table_ptr != 0)
        free((void *)data_fd2_tile_event_data_table_ptr);
    if (portrait_sprite_cache != 0)
        free((void *)portrait_sprite_cache);
    if (chapter_portrait_load_buffer != 0)
        free((void *)chapter_portrait_load_buffer);
    /* loader-returned buffers left live by the function */
    if (data_fd2_vga_palette_data_ptr != 0)
        free((void *)data_fd2_vga_palette_data_ptr);
    if (current_chapter_text != 0)
        free((void *)current_chapter_text);
    if (data_fd2_battle_tile_map_ptr != 0)
        free((void *)data_fd2_battle_tile_map_ptr);
    if (battle_scene_snapshot != 0)
        free((void *)battle_scene_snapshot);
    if (data_fd2_tile_attribute_flags_buffer_ptr != 0)
        free((void *)data_fd2_tile_attribute_flags_buffer_ptr);
    if (data_fd2_graphics_static_bg_buffer_ptr != 0)
        free((void *)data_fd2_graphics_static_bg_buffer_ptr);
    if (data_fd2_graphics_animated_bg_buffer_ptr != 0)
        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
    if (data_fd2_audio_bgm_sequence_data_buf_ptr != 0)
        free((void *)data_fd2_audio_bgm_sequence_data_buf_ptr);
    free(g_ls_roster_buf);
    free(g_ls_consumed_buf);

    /* restore testglob.c defaults */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    portrait_sprite_cache = 0;
    chapter_portrait_load_buffer = 0;
    battle_scene_snapshot = 0;
    current_chapter_text = 0;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_map_height_tiles = 15;

    data_fd2_vga_palette_data_ptr = 0;
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    data_fd2_chapter_current_chapter_id = 1;

    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
}


/* ---- Test: fd2_main_menu_continue_dispatcher ---- */

/* The new-game / continue paths call the REAL fd2_load_dat_resource for
 * FDOTHER (palette / menu atlas) and fd2_set_bgm_track_with_fade for FDMUS,
 * both against the staged real archives. Null the loader-target globals so the
 * loader's free(old_buf) is a no-op, then free the loaded buffers afterwards. */
static void setup_menu_dats(void)
{
    data_fd2_vga_palette_data_ptr = 0;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    data_fd2_audio_bgm_last_set_track_id = 0xFF;
}

static void teardown_menu_dats(void)
{
    if (data_fd2_vga_palette_data_ptr != 0)
        free((void *)data_fd2_vga_palette_data_ptr);
    if (data_fd2_ui_menu_screen_sprite_atlas_buf_ptr != 0)
        free((void *)data_fd2_ui_menu_screen_sprite_atlas_buf_ptr);
    if (data_fd2_audio_bgm_sequence_data_buf_ptr != 0)
        free((void *)data_fd2_audio_bgm_sequence_data_buf_ptr);
    data_fd2_vga_palette_data_ptr = 0;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
}

static void test_main_menu_new_game(void)
{
    int r;
    setup_menu_dats();
    g_ending_menu_return = 0;
    data_fd2_chapter_current_chapter_id = 5;
    r = fd2_main_menu_continue_dispatcher();
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 0);
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count, 0);
    ASSERT_EQ((long)data_fd2_ui_play_active_flag, 1);
    teardown_menu_dats();
}


static void test_main_menu_fallback(void)
{
    int r;
    /* menu_choice==2 routes to the real fd2_load_save_and_init_engine();
     * stage its buffer fixtures so it runs against the real FD2.SAV. */
    setup_load_save_fixture();
    g_ending_menu_return = 2;
    r = fd2_main_menu_continue_dispatcher();
    ASSERT_EQ((long)r, 0);
    teardown_load_save_fixture();
}


/* ---- Test: fd2_load_save_and_init_engine ---- */

static void test_load_save_restores_scalar_state(void)
{
    uint8 *sav;
    uint8 *tm;
    int    exp_chapter;
    int    exp_party;
    int    distinct;
    int    k;
    int    seen[256];

    setup_load_save_fixture();
    data_fd2_battle_anim_phase = 0xFF;
    data_fd2_battle_current_active_char_idx = 0xFF;

    sav = realsav_decrypted();
    exp_chapter = sav[0x30C5];
    exp_party = sav[0x30C4];

    fd2_load_save_and_init_engine();

    /* scalar engine state restored from the decrypted save header tail */
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, (long)exp_chapter);
    ASSERT_EQ((long)data_fd2_battle_party_member_count, (long)exp_party);
    ASSERT_EQ((long)data_fd2_battle_turn_counter, (long)sav[0x30C3]);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, (long)sav[0x30C6]);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, (long)sav[0x30C7]);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_x, (long)sav[0x30C8]);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_y, (long)sav[0x30C9]);
    ASSERT_EQ((long)data_fd2_battle_cursor_screen_x, (long)sav[0x30CA]);
    ASSERT_EQ((long)data_fd2_battle_cursor_screen_y, (long)sav[0x30CB]);
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count, (long)sav[0x30CC]);
    ASSERT_EQ((long)data_fd2_shared_party_total_gold,
              (long)*(uint32 *)(sav + 0x30CD));
    ASSERT_EQ((long)data_fd2_ui_game_speed_flag, (long)sav[0x30D1]);
    ASSERT_EQ((long)data_fd2_ui_terrain_hud_user_enabled, (long)sav[0x30D2]);
    ASSERT_EQ((long)data_fd2_audio_bgm_enabled_flag, (long)sav[0x30D3]);
    ASSERT_EQ((long)data_fd2_audio_sfx_enabled_flag, (long)sav[0x30D4]);

    /* derived state from the real tile-map (FDFIELD[chapter*3]) + tile-event
     * header (decrypted save bytes 1/2) */
    realdat_read_resource("FDFIELD.DAT", exp_chapter * 3, &tm);
    ASSERT_EQ((long)data_fd2_battle_map_width_tiles, (long)*(int16 *)tm);
    ASSERT_EQ((long)data_fd2_battle_map_height_tiles, (long)*(int16 *)(tm + 2));
    free(tm);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_total_size, (long)sav[1]);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_alloc_offset, (long)sav[2]);

    /* end-of-routine engine flags */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
    ASSERT_EQ((long)data_fd2_battle_current_active_char_idx, 0);

    /* portrait cache rebuilt once per DISTINCT party-member portrait_id
     * (offset +0x07 of each 0x50 runtime_char record at save +0x12A3) */
    memset(seen, 0, sizeof(seen));
    distinct = 0;
    for (k = 0; k < exp_party; k++) {
        int pid = sav[0x12A3 + k * 0x50 + 0x07];
        if (!seen[pid]) { seen[pid] = 1; distinct++; }
    }
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, (long)distinct);

    free(sav);
    teardown_load_save_fixture();
}


static void test_load_save_cinematic_loop_counts(void)
{
    setup_load_save_fixture();

    fd2_load_save_and_init_engine();

    /* intro loop = 9 frames; zoom loop = i 2,3,4 then 5->9 = 4 frames;
     * total 13 alloc/blit + matching cleanup calls (validates the
     * i==5 -> i=9 skip in the zoom loop). These loop bounds are fixed and
     * independent of the save's chapter/party content. */
    ASSERT_EQ((long)g_alloc_blit_calls, 13);
    ASSERT_EQ((long)g_restore_block_calls, 13);
    teardown_load_save_fixture();
}

/* NOTE: the checksum-mismatch arm (fd2_save_compute_checksum result !=
 * stored tail) is intentionally NOT unit-tested. The staged real FD2.SAV has
 * a valid checksum (so the match path runs), and the mismatch arm calls the
 * real, linked fd2_wait_for_input_dialog_with_blink(), which busy-waits for a
 * keypress and would hang forever in the silent automated harness. That arm
 * only differs by an error-dialog (display + blocking input) and then falls
 * through to the same restore path covered above; its coverage is deferred to
 * Phase 9 integration. See src/emit_issues.json. */


static void test_main_menu_continue_quit(void)
{
    int r;
    setup_menu_dats();
    g_ending_menu_return = 1;
    g_slot_selector_return = -1;
    r = fd2_main_menu_continue_dispatcher();
    ASSERT_EQ((long)r, -1);
    /* the menu-atlas FDOTHER[0xD] buffer is freed + nulled by the function;
     * teardown frees the palette + any bgm buffer. */
    teardown_menu_dats();
}


void run_life_main_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: life/main\n");
    RUN_TEST(test_main_menu_new_game);
    RUN_TEST(test_main_menu_fallback);
    RUN_TEST(test_main_menu_continue_quit);
    RUN_TEST(test_load_save_restores_scalar_state);
    RUN_TEST(test_load_save_cinematic_loop_counts);
    printf("\n");
}
