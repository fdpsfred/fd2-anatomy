/*
 * unit tests for src/life/main.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>
#include "rsrcfix.h"   /* write_fake_fdicon(), write_fake_dat() */

#define USE_ITEM_ID 10

/* DAT filename strings (match the Ghidra/globals symbols) */
extern char data_fd2_string_resource_filename_fdtxt_dat[];
extern char data_fd2_string_resource_filename_fdother_dat[];
extern char data_fd2_string_resource_filename_fdfield_dat_51a59[];
extern char data_fd2_string_resource_filename_fdshap_dat_51a65[];
extern char data_fd2_string_fdmus_dat[];

/* Write a uniform packed DAT covering indices 0..n-1 with tiny zeroed
 * payloads (content irrelevant; only the index/offset table must resolve). */
static void write_stub_dat(const char *name, int n, int payload_sz)
{
    int   *sizes;
    const uint8 **ptrs;
    int    i;
    sizes = (int *)malloc((size_t)n * sizeof(int));
    ptrs  = (const uint8 **)malloc((size_t)n * sizeof(uint8 *));
    for (i = 0; i < n; i++) { sizes[i] = payload_sz; ptrs[i] = 0; }
    write_fake_dat(name, n, sizes, ptrs);
    free(sizes);
    free(ptrs);
}

/* Build the packed DAT files the real fd2_load_dat_resource (reached from
 * fd2_load_save_and_init_engine and fd2_load_chapter_background_layers) reads:
 *   FDOTHER.DAT idx 0 (vga palette) + bg single-sprite idx (<=0x10)
 *   FDFIELD.DAT idx chapter*3 (tile_map, 0-width/height) + chapter*3+2 (scratch)
 *   FDTXT.DAT   idx chapter+1
 *   FDSHAP.DAT  idx 0,1 (scene_id 0 from save buffer)
 * Payload content is irrelevant except FDFIELD[chapter*3] needs 0 width/height
 * (zeroed) so the linked fd2_tick_tile_event_animations stays a no-op. */
static void write_load_save_dats(int chapter_id)
{
    int base = chapter_id * 3;

    /* FDFIELD: indices 0..base+2, 16 B each so the tile_map header read at
     * +0/+2 yields width=height=0 (keeps fd2_tick_tile_event_animations a
     * no-op). */
    write_stub_dat(
        (const char *)data_fd2_string_resource_filename_fdfield_dat_51a59,
        base + 3, 16);
    /* FDTXT: indices 0..chapter+1 */
    write_stub_dat((const char *)data_fd2_string_resource_filename_fdtxt_dat,
                   chapter_id + 2, 4);
    /* FDOTHER: idx 0 (palette) + background_layers single-sprite (idx 0x10) */
    write_stub_dat((const char *)data_fd2_string_resource_filename_fdother_dat,
                   0x11, 16);
    /* FDSHAP: scene_id 0 -> indices 0,1 */
    write_stub_dat((const char *)data_fd2_string_resource_filename_fdshap_dat_51a65,
                   2, 4);
    /* FDMUS: the trailing bgm load uses track[chapter] (0 in tests); cover
     * a generous index range. */
    write_stub_dat((const char *)data_fd2_string_fdmus_dat, 0x21, 16);
}

extern runtime_char g_test_rc_array[8];
extern int g_build_spell_list_return;
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;
extern uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
extern int g_ending_menu_return;
extern int g_slot_selector_return;
extern int g_chapter_transition_return;
extern int g_play_sfx_with_handle_calls;
extern int g_play_sfx_sample_from_bank_calls;
extern int g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int g_blit_indexed_sprite_last_x;
extern int g_blit_indexed_sprite_last_y;
extern int    g_mini_panel_calls;
extern uint32 g_mini_panel_last_buf;
extern uint32 g_mini_panel_last_stride;
extern uint32 g_mini_panel_last_char;
extern int g_find_equipped_return;
extern int g_composite_call_count;
extern int g_attack_dispatch_return;
extern int g_attack_dispatch_calls;
extern int g_seek_optimal_return;
extern int g_advance_nearest_return;
extern int g_walk_return;
extern int g_score_physical_return;
extern int g_pass_turn_calls;
extern int g_execute_spell_calls;
extern int g_execute_physical_calls;
extern int g_pathfind_return;
extern int g_pathfind_walk_return;
extern int g_pathfind_write_dst;
extern int g_pathfind_dst_x;
extern int g_pathfind_dst_y;
extern int g_pathfind_seq_enable;
extern int g_pathfind_seq[4];
extern int g_pathfind_seq_idx;
extern int g_pathfind_seq_steps;
extern uint8 g_pathfind_step_bytes[8];
extern int g_pathfind_md0_dst_x;
extern int g_pathfind_md0_dst_y;
extern int g_count_usable_slots_return;
extern uint8 g_spell_list_buf[12];
extern int g_remove_inventory_calls;
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;

/* fd2_load_save_and_init_engine leaf-helper recorders (testglob.c) */
extern int g_alloc_blit_calls;
extern uint32 g_alloc_blit_last_idx;
extern int g_cleanup_sprite_calls;

/* buffers staged by the fixture, freed by teardown */
static void *g_ls_roster_buf;
static void *g_ls_consumed_buf;
static void *g_ls_tilemap_buf;

/* ----------------------------------------------------------------
 * fd2_load_save_and_init_engine fixture
 *
 * The real function fopens/freads FD2.SAV + FDICON.B24 and fwrites
 * FD2.TMP, then memmoves restored state out of the save buffer into
 * engine globals. Write a deterministic FD2.SAV on disk and point all
 * engine pointer-globals at valid buffers so the real function runs
 * end-to-end without faulting. checksum_match selects whether the
 * real fd2_save_compute_checksum sum equals the value stored at
 * save tail +0x59C7.
 * ---------------------------------------------------------------- */
static void setup_load_save_fixture(int chapter_id, int party_count,
                                    int checksum_match)
{
    uint8 *sav;
    FILE *fp;

    /* destination buffers the function memmoves into / reads from */
    g_ls_roster_buf = malloc(0xA00);
    g_ls_consumed_buf = malloc(0x20);
    g_ls_tilemap_buf = 0;          /* tile_map now comes from FDFIELD load */
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ls_roster_buf;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ls_consumed_buf;

    /* these are freed-if-nonzero then re-malloc'd / reloaded by the function;
     * NULL/0 them so it does not free a static/stale pointer. tile_map is now
     * loaded from FDFIELD.DAT[chapter*3] (0-width/height) so the linked
     * fd2_tick_tile_event_animations stays a safe no-op. */
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

    /* real DAT archives for the resource reloads */
    write_load_save_dats(chapter_id);

    /* build the on-disk FD2.SAV (0x59CB bytes) */
    sav = (uint8 *)malloc(0x59CB);
    memset(sav, 0, 0x59CB);
    sav[0] = 0x00;                 /* tile_event[0] -> scene_id            */
    sav[1] = 0x11;                 /* tile_event[1] -> cache_total_size    */
    sav[2] = 0x22;                 /* tile_event[2] -> cache_alloc_offset  */
    /* saved runtime_char records start at 0x12A3 (0x50 stride). Seed a
     * distinct portrait_id (+0x07) per party member so the now-real
     * fd2_load_portrait_to_cache (called once per member in load_save)
     * registers each as a distinct cache entry: cache_count == party_count. */
    {
        int k;
        for (k = 0; k < party_count; k++) {
            sav[0x12A3 + k * 0x50 + 0x07] = (uint8)(0x40 + k);
        }
    }
    sav[0x30C3] = 7;               /* turn_counter                        */
    sav[0x30C4] = (uint8)party_count;
    sav[0x30C5] = (uint8)chapter_id;
    sav[0x30C6] = 0x12;            /* view_window_origin_x                */
    sav[0x30C7] = 0x34;            /* view_window_origin_y                */
    sav[0x30C8] = 0x56;            /* cursor_world_x                      */
    sav[0x30C9] = 0x78;            /* cursor_world_y                      */
    sav[0x30CA] = 0x9A;            /* cursor_screen_x                     */
    sav[0x30CB] = 0xBC;            /* cursor_screen_y                     */
    sav[0x30CC] = 3;               /* menu_party_member_count             */
    *(uint32 *)(sav + 0x30CD) = 0x4321;   /* party_total_gold            */
    sav[0x30D1] = 1;               /* game_speed_flag                     */
    sav[0x30D2] = 1;               /* terrain_hud_user_enabled            */
    sav[0x30D3] = 1;               /* bgm_enabled_flag                    */
    sav[0x30D4] = 0;               /* sfx_enabled_flag                    */
    /* Stored checksum at the tail. fd2_save_crypt_buffer is a no-op fake,
     * so the loader's real fd2_save_compute_checksum sums the raw on-disk
     * bytes [0..0x59C6]. For the match case store that real sum; for the
     * mismatch case store a value the real sum can never equal. */
    if (checksum_match) {
        *(uint32 *)(sav + 0x59C7) =
            fd2_save_compute_checksum((uint32)sav, 0x59CB);
    } else {
        /* real sum of 0x59C7 bytes <= 0x59C7 * 0xFF = 0x58C729; pick a
         * larger constant so it can never coincide with the true sum. */
        *(uint32 *)(sav + 0x59C7) = 0xFFFFFFFFu;
    }

    fp = fopen("FD2.SAV", "wb");
    fwrite(sav, 1, 0x59CB, fp);
    fclose(fp);
    free(sav);

    /* the real fd2_load_portrait_to_cache (reached through battle_data ->
     * the real fd2_init_runtime_char_for_battle) parses FDICON.B24 */
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
    write_fake_fdicon();

    g_alloc_blit_calls = 0;
    g_cleanup_sprite_calls = 0;
    g_alloc_blit_last_idx = 0;
}

/* Free fixture buffers and restore every global the load touched back to its
 * testglob.c default so later suites are not contaminated. */
static void teardown_load_save_fixture(void)
{
    /* the function re-malloc'd these two; free its buffers */
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
    free(g_ls_tilemap_buf);

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

    remove("FD2.SAV");
    remove("FDICON.B24");
    remove("FD2.TMP");
    remove((const char *)data_fd2_string_resource_filename_fdtxt_dat);
    remove((const char *)data_fd2_string_resource_filename_fdother_dat);
    remove((const char *)data_fd2_string_resource_filename_fdfield_dat_51a59);
    remove((const char *)data_fd2_string_resource_filename_fdshap_dat_51a65);
    remove((const char *)data_fd2_string_fdmus_dat);
}


/* ---- Test: fd2_score_spell_candidate ---- */

/* ---- Test: fd2_main_menu_continue_dispatcher ---- */

/* The new-game / continue paths call the REAL fd2_load_dat_resource for
 * FDOTHER (palette/menu atlas) and fd2_set_bgm_track_with_fade for FDMUS.
 * Stage both archives + null the loader-target globals so the loader's
 * free(old_buf) is a no-op, then free the loaded buffers afterwards. */
static void setup_menu_dats(void)
{
    data_fd2_vga_palette_data_ptr = 0;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
    data_fd2_audio_bgm_sequence_data_buf_ptr = 0;
    data_fd2_audio_bgm_last_set_track_id = 0xFF;
    write_stub_dat((const char *)data_fd2_string_resource_filename_fdother_dat,
                   0x11, 16);
    write_stub_dat((const char *)data_fd2_string_fdmus_dat, 0x21, 16);
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
    remove((const char *)data_fd2_string_resource_filename_fdother_dat);
    remove((const char *)data_fd2_string_fdmus_dat);
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
     * stage its file + buffer fixtures so it runs without faulting. */
    setup_load_save_fixture(3, 2, 1);
    g_ending_menu_return = 2;
    r = fd2_main_menu_continue_dispatcher();
    ASSERT_EQ((long)r, 0);
    teardown_load_save_fixture();
}


/* ---- Test: fd2_load_save_and_init_engine ---- */

static void test_load_save_restores_scalar_state(void)
{
    setup_load_save_fixture(3, 2, 1);
    data_fd2_battle_anim_phase = 0xFF;
    data_fd2_battle_current_active_char_idx = 0xFF;

    fd2_load_save_and_init_engine();

    /* scalar engine state restored from the save header tail */
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 3);
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 2);
    ASSERT_EQ((long)data_fd2_battle_turn_counter, 7);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0x12);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x34);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_x, 0x56);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_y, 0x78);
    ASSERT_EQ((long)data_fd2_battle_cursor_screen_x, 0x9A);
    ASSERT_EQ((long)data_fd2_battle_cursor_screen_y, 0xBC);
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count, 3);
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, 0x4321);
    ASSERT_EQ((long)data_fd2_ui_game_speed_flag, 1);
    ASSERT_EQ((long)data_fd2_ui_terrain_hud_user_enabled, 1);
    ASSERT_EQ((long)data_fd2_audio_bgm_enabled_flag, 1);
    ASSERT_EQ((long)data_fd2_audio_sfx_enabled_flag, 0);

    /* derived state from tile-map + tile-event header */
    ASSERT_EQ((long)data_fd2_battle_map_width_tiles, 0);
    ASSERT_EQ((long)data_fd2_battle_map_height_tiles, 0);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_total_size, 0x11);
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_alloc_offset, 0x22);

    /* end-of-routine engine flags */
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
    ASSERT_EQ((long)data_fd2_battle_current_active_char_idx, 0);

    /* portrait cache rebuilt once per (distinct) party member */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 2);
    teardown_load_save_fixture();
}


static void test_load_save_cinematic_loop_counts(void)
{
    setup_load_save_fixture(1, 1, 1);

    fd2_load_save_and_init_engine();

    /* intro loop = 9 frames; zoom loop = i 2,3,4 then 5->9 = 4 frames;
     * total 13 alloc/blit + matching cleanup calls (validates the
     * i==5 -> i=9 skip in the zoom loop) */
    ASSERT_EQ((long)g_alloc_blit_calls, 13);
    ASSERT_EQ((long)g_cleanup_sprite_calls, 13);
    teardown_load_save_fixture();
}

/* NOTE: the checksum-mismatch arm (fd2_save_compute_checksum result !=
 * stored tail) is intentionally NOT unit-tested. It calls the real,
 * linked fd2_wait_for_input_dialog_with_blink(), which busy-waits for a
 * keypress and therefore hangs forever in the silent automated harness.
 * That arm only differs by an error-dialog (display + blocking input)
 * and then falls through to the same restore path covered above; its
 * coverage is deferred to Phase 9 integration. See src/emit_issues.json. */


static void test_main_menu_continue_quit(void)
{
    int r;
    setup_menu_dats();
    g_ending_menu_return = 1;
    g_slot_selector_return = -1;
    r = fd2_main_menu_continue_dispatcher();
    ASSERT_EQ((long)r, -1);
    /* the menu-atlas FDOTHER[0xD] buffer is freed + nulled by the function;
     * teardown frees the palette + any bgm buffer + removes the archives. */
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
