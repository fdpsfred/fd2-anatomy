/*
 * unit tests for src/battle/btl_init.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>
#include "rsrcfix.h"   /* write_fake_fdicon() */

#define USE_ITEM_ID 10

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


/* ---- Test: fd2_init_battle_state_for_chapter ---- */

/* fd2_init_battle_state_for_chapter calls the real (linked) chapter-battle-data
 * loader, which fopens FDICON.B24 and reads packed FDFIELD/FDSHAP buffers via
 * pointer globals (and exit()s if FDICON.B24 is missing). Stage a minimal
 * end-to-end fixture: zero-length party (total_size 0) so the per-slot build
 * loop is a no-op, plus valid tile-map / tile-event / consumed-flags buffers
 * and an on-disk FDICON.B24 so the loader runs to completion without faulting. */
static uint8 *g_bi_tileevent;
static uint8 *g_bi_tilemap;
static uint8 *g_bi_field;
static uint8 *g_bi_consumed;

static void setup_init_fixture(void)
{
    FILE *fp;

    data_fd2_chapter_current_chapter_id = 0x10;

    g_bi_tileevent = (uint8 *)malloc(16);
    memset(g_bi_tileevent, 0, 16);   /* [1]=total_size=0 -> empty loop */
    data_fd2_tile_event_data_table_ptr = (uint32)g_bi_tileevent;

    g_bi_tilemap = (uint8 *)malloc(16);
    memset(g_bi_tilemap, 0, 16);
    data_fd2_battle_tile_map_ptr = (uint32)g_bi_tilemap;

    g_bi_field = (uint8 *)malloc(64);
    memset(g_bi_field, 0, 64);
    chapter_portrait_load_buffer = (uint32)g_bi_field;

    g_bi_consumed = (uint8 *)malloc(0x20);
    memset(g_bi_consumed, 0, 0x20);
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_bi_consumed;

    data_fd2_battle_runtime_char_array_ptr = NULL;
    portrait_sprite_cache = 0;

    fp = fopen("FDICON.B24", "wb");
    fclose(fp);
}

static void teardown_init_fixture(void)
{
    if (data_fd2_battle_runtime_char_array_ptr != NULL)
        free(data_fd2_battle_runtime_char_array_ptr);
    if (portrait_sprite_cache != 0)
        free((void *)portrait_sprite_cache);
    free(g_bi_tileevent);
    free(g_bi_tilemap);
    /* g_bi_field already free()d by the loader (it frees + nulls
     * chapter_portrait_load_buffer, whose value is g_bi_field) */
    free(g_bi_consumed);

    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    portrait_sprite_cache = 0;
    current_chapter_text = 0;
    battle_scene_snapshot = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_chapter_current_chapter_id = 1;

    remove("FDICON.B24");
}

static void test_init_battle_state_zeros_cursor(void)
{
    setup_init_fixture();
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_view_window_origin_y = 3;
    data_fd2_chapter_event_or_battle_end_code = 99;
    fd2_init_battle_state_for_chapter();
    ASSERT_EQ((long)data_fd2_battle_cursor_world_x, 0);
    ASSERT_EQ((long)data_fd2_battle_cursor_world_y, 0);
    ASSERT_EQ((long)data_fd2_battle_cursor_screen_x, 0);
    ASSERT_EQ((long)data_fd2_battle_cursor_screen_y, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 0);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0);
    ASSERT_EQ((long)data_fd2_chapter_event_or_battle_end_code, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
    ASSERT_EQ((long)data_fd2_battle_turn_counter, 1);
    teardown_init_fixture();
}


static void test_set_chapter_init_done_flag(void)
{
    data_fd2_chapter_chapter_init_done_flag = 0;
    fd2_set_chapter_init_done_flag();
    ASSERT_EQ(data_fd2_chapter_chapter_init_done_flag, 1);
}


static void test_set_battle_anim_phase_to_1(void)
{
    data_fd2_battle_anim_phase = 0;
    fd2_set_battle_anim_phase_to_1();
    ASSERT_EQ(data_fd2_battle_anim_phase, 1);
}


/* ---- Tests: fd2_init_runtime_char_for_battle ---- */

extern enemy_data        data_fd2_battle_enemy_data_table[68];
extern character_base    data_fd2_battle_character_base_table[32];
extern character_growth  data_fd2_battle_character_growth_table[68];

static runtime_char g_irc_slots[8];
static uint8 g_irc_field[64];
static uint8 g_irc_tilemap[64];
static uint8 g_irc_tileevent[256];
/* the real fd2_init_runtime_char_for_battle calls the now-real
 * fd2_load_portrait_to_cache(char_id, fdicon_fp), which fseek/freads the
 * passed FILE*. Stage a valid FDICON.B24 and pass this handle. */
static FILE *g_irc_fp;

/* Stage field buffer (desired_x/y) and the per-char tile-event record at
 * char_field_idx, plus point all backing globals at local buffers. Uses
 * chapter_init_phase_flag=1 so the spawn position is taken verbatim from
 * the field buffer (no tile-map search). */
static void irc_setup(uint32 field_idx, uint8 desired_x, uint8 desired_y)
{
    memset(g_irc_slots, 0, sizeof(g_irc_slots));
    memset(g_irc_field, 0, sizeof(g_irc_field));
    memset(g_irc_tilemap, 0, sizeof(g_irc_tilemap));
    memset(g_irc_tileevent, 0, sizeof(g_irc_tileevent));

    g_irc_field[field_idx * 6 + 2] = desired_x;
    g_irc_field[field_idx * 6 + 4] = desired_y;

    data_fd2_battle_runtime_char_array_ptr = g_irc_slots;
    data_fd2_battle_party_member_count = 0;
    chapter_portrait_load_buffer = (uint32)g_irc_field;
    data_fd2_battle_tile_map_ptr = (uint32)g_irc_tilemap;
    data_fd2_tile_event_data_table_ptr = (uint32)g_irc_tileevent;
    data_fd2_chapter_init_phase_flag = 1;

    /* fresh portrait cache + valid FDICON for the real portrait loader */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
    write_fake_fdicon();
    g_irc_fp = fopen("FDICON.B24", "rb");
}

static void irc_teardown(void)
{
    if (g_irc_fp != NULL) {
        fclose(g_irc_fp);
        g_irc_fp = NULL;
    }
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
    remove("FDICON.B24");
    remove("FD2.TMP");
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    chapter_portrait_load_buffer = 0;
    data_fd2_battle_tile_map_ptr = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_chapter_init_phase_flag = 0;
}

/* Player class (char_id < 0x44): stats from base + growth tables. */
static void test_irc_player_class_stats(void)
{
    uint8 *rec;
    uint8 *base;
    uint8 *grow;
    runtime_char *rc;

    irc_setup(0, 7, 9);

    /* per-char record at tile_event + 0x83 + idx*0x1A (idx 0 -> +0x83) */
    rec = g_irc_tileevent + 0x83;
    rec[0] = 2;       /* team = player */
    rec[1] = 5;       /* char_id (< 0x44) */
    rec[2] = 0x11;    /* ai_target_id */
    rec[4] = 3;       /* level */
    rec[5] = 0x21;    /* weapon slot (!= 0xFF -> dual encoding) */
    rec[6] = 0x22;    /* second equip */
    rec[7] = 0x30;    /* inventory item 0 (!= 0xFF) */
    rec[8] = 0xff;    /* inventory item 1 (empty) */

    /* character_base[5]: bytes via raw access */
    base = (uint8 *)&data_fd2_battle_character_base_table[5];
    memset(base, 0, sizeof(character_base));
    base[0] = 0xaa;          /* +0x1F archetype */
    base[1] = 0xbb;          /* +0x20 job */
    base[3] = 40;            /* HP base (word) */
    base[5] = 12;            /* MP base (word) */
    base[7] = 0x55;          /* magic resist */
    *(uint16 *)(base + 0x12) = 6;   /* AP base */
    *(uint16 *)(base + 0x14) = 4;   /* DP base */
    *(uint16 *)(base + 0x16) = 8;   /* DX base */

    grow = (uint8 *)&data_fd2_battle_character_growth_table[5];
    memset(grow, 0, sizeof(character_growth));
    grow[0] = 2;   /* AP growth/level */
    grow[2] = 1;   /* DP growth/level */
    grow[4] = 3;   /* DX growth/level */
    grow[6] = 5;   /* HP growth/(level-1) */
    grow[8] = 2;   /* MP growth/(level-1) */

    fd2_init_runtime_char_for_battle(0, (uint32)g_irc_fp);

    rc = &g_irc_slots[0];
    /* spawn taken verbatim (phase flag = 1) */
    ASSERT_EQ((long)rc->pos_x, 7);
    ASSERT_EQ((long)rc->pos_y, 9);
    ASSERT_EQ((long)rc->team, 2);
    ASSERT_EQ((long)rc->portrait_id, 5);   /* +7 char_id */
    ASSERT_EQ((long)rc->char_id, 5);       /* +8 char_id copy */
    ASSERT_EQ((long)rc->archetype_flag, 0xaa);
    ASSERT_EQ((long)rc->job_id, 0xbb);
    ASSERT_EQ((long)rc->status_flags_block[0], 3); /* +0x21 level */
    /* movement_order: team==2 -> 0 */
    ASSERT_EQ((long)rc->movement_order, 0);
    ASSERT_EQ((long)rc->ai_target_and_dx_block[0], 0x11); /* +0x3D */
    /* +0x3B magic resist = base[7] */
    ASSERT_EQ((long)*((uint8 *)rc + 0x3b), 0x55);
    /* HP = base[3] + growth[6]*(level-1) = 40 + 5*2 = 50 */
    ASSERT_EQ((long)rc->hp_current, 50);
    ASSERT_EQ((long)rc->hp_max, 50);
    /* MP = base[5] + growth[8]*(level-1) = 12 + 2*2 = 16 */
    ASSERT_EQ((long)rc->mp_current, 16);
    ASSERT_EQ((long)rc->mp_max, 16);
    /* AP(+0x37) = growth[0]*level + base[0x12] = 2*3 + 6 = 12 */
    ASSERT_EQ((long)*(uint16 *)((uint8 *)rc + 0x37), 12);
    /* DP(+0x39) = growth[2]*level + base[0x14] = 1*3 + 4 = 7 */
    ASSERT_EQ((long)*(uint16 *)((uint8 *)rc + 0x39), 7);
    /* DX(+0x3E) = growth[4]*level + base[0x16] = 3*3 + 8 = 17 */
    ASSERT_EQ((long)*(uint16 *)((uint8 *)rc + 0x3e), 17);
    /* dual-equip encoding: slot5 != 0xFF */
    ASSERT_EQ((long)rc->inventory_slots[0], 0x40);  /* +0xA */
    ASSERT_EQ((long)rc->inventory_slots[1], 0x21);  /* +0xB = rec[5] */
    ASSERT_EQ((long)rc->inventory_slots[2], 0x40);  /* +0xC */
    ASSERT_EQ((long)rc->inventory_slots[3], 0x22);  /* +0xD = rec[6] */
    /* inventory slot 0 present, slot 1 empty */
    ASSERT_EQ((long)rc->inventory_slots[4], 0);     /* +0xE flag present */
    ASSERT_EQ((long)rc->inventory_slots[5], 0x30);  /* +0xF item id */
    ASSERT_EQ((long)rc->inventory_slots[6], 0x80);  /* +0x10 flag empty */
    ASSERT_EQ((long)rc->inventory_slots[7], 0xff);  /* +0x11 item id */
    /* party count incremented */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);

    irc_teardown();
}

/* Enemy class (char_id >= 0x44): stats from enemy table scaled by level,
 * second-slot-only equip encoding (rec[5] == 0xFF). */
static void test_irc_enemy_class_stats(void)
{
    uint8 *rec;
    uint8 *en;
    runtime_char *rc;

    irc_setup(0, 4, 5);

    rec = g_irc_tileevent + 0x83;
    rec[0] = 0;        /* team = enemy */
    rec[1] = 0x44 + 2; /* char_id >= 0x44 -> enemy index 2 */
    rec[4] = 4;        /* level */
    rec[5] = 0xff;     /* second-slot-only encoding */
    rec[6] = 0x77;     /* the single equip */
    rec[7] = 0xff;     /* inventory empty */

    en = (uint8 *)&data_fd2_battle_enemy_data_table[2];
    memset(en, 0, sizeof(enemy_data));
    en[0] = 0xcc;                 /* +0x1F archetype */
    en[1] = 0xdd;                 /* +0x20 job */
    *(uint16 *)(en + 2) = 30;     /* HP word */
    en[4] = 5;                    /* MP byte */
    en[5] = 3;                    /* AP byte */
    en[6] = 2;                    /* DP byte */
    en[7] = 6;                    /* DX byte */
    en[8] = 0x99;                 /* magic resist */

    fd2_init_runtime_char_for_battle(0, (uint32)g_irc_fp);

    rc = &g_irc_slots[0];
    ASSERT_EQ((long)rc->pos_x, 4);
    ASSERT_EQ((long)rc->pos_y, 5);
    ASSERT_EQ((long)rc->team, 0);
    ASSERT_EQ((long)rc->char_id, 0x46);
    ASSERT_EQ((long)rc->archetype_flag, 0xcc);
    ASSERT_EQ((long)rc->job_id, 0xdd);
    /* movement_order: team != 2 -> 0xFF */
    ASSERT_EQ((long)rc->movement_order, 0xff);
    /* +0x3B magic resist = enemy[8] */
    ASSERT_EQ((long)*((uint8 *)rc + 0x3b), 0x99);
    /* HP = enemy_word[2] * level = 30*4 = 120 */
    ASSERT_EQ((long)rc->hp_current, 120);
    ASSERT_EQ((long)rc->hp_max, 120);
    /* MP = enemy[4] * level = 5*4 = 20 */
    ASSERT_EQ((long)rc->mp_current, 20);
    ASSERT_EQ((long)rc->mp_max, 20);
    /* AP(+0x37) = enemy[5]*level = 3*4 = 12 */
    ASSERT_EQ((long)*(uint16 *)((uint8 *)rc + 0x37), 12);
    /* DP(+0x39) = enemy[6]*level = 2*4 = 8 */
    ASSERT_EQ((long)*(uint16 *)((uint8 *)rc + 0x39), 8);
    /* DX(+0x3E) = enemy[7]*level = 6*4 = 24 */
    ASSERT_EQ((long)*(uint16 *)((uint8 *)rc + 0x3e), 24);
    /* second-slot-only encoding: rec[5]==0xFF */
    ASSERT_EQ((long)rc->inventory_slots[0], 0x40);  /* +0xA */
    ASSERT_EQ((long)rc->inventory_slots[1], 0x77);  /* +0xB = rec[6] */
    ASSERT_EQ((long)rc->inventory_slots[2], 0x80);  /* +0xC */

    irc_teardown();
}

/* Phase flag == 0: nearest-walkable-tile search picks the closest tile
 * whose attr byte (+6 of the 4-byte cell) has bit 0x40 clear. */
static void test_irc_tile_search_nearest(void)
{
    uint8 *rec;
    runtime_char *rc;
    int x, y;
    uint32 cell;

    irc_setup(0, 3, 2);

    /* small 4x4 map; mark every cell occupied (bit 0x40 set) except (1,1) */
    data_fd2_battle_map_width_tiles = 4;
    data_fd2_battle_map_height_tiles = 4;
    for (y = 0; y < 4; y = y + 1) {
        for (x = 0; x < 4; x = x + 1) {
            cell = (uint32)((4 * y + x) * 4 + 6);
            g_irc_tilemap[cell] = 0x40;
        }
    }
    g_irc_tilemap[(4 * 1 + 1) * 4 + 6] = 0; /* only (x=1,y=1) walkable */
    data_fd2_chapter_init_phase_flag = 0;

    rec = g_irc_tileevent + 0x83;
    rec[1] = 0x44;     /* enemy idx 0 (keep stat path simple) */
    rec[4] = 1;        /* level */
    rec[5] = 0xff;
    rec[7] = 0xff;

    fd2_init_runtime_char_for_battle(0, (uint32)g_irc_fp);

    rc = &g_irc_slots[0];
    ASSERT_EQ((long)rc->pos_x, 1);
    ASSERT_EQ((long)rc->pos_y, 1);

    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_map_height_tiles = 15;
    irc_teardown();
}


void run_battle_btl_init_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: battle/btl_init\n");
    RUN_TEST(test_init_battle_state_zeros_cursor);
    RUN_TEST(test_set_chapter_init_done_flag);
    RUN_TEST(test_set_battle_anim_phase_to_1);
    RUN_TEST(test_irc_player_class_stats);
    RUN_TEST(test_irc_enemy_class_stats);
    RUN_TEST(test_irc_tile_search_nearest);
    printf("\n");
}
