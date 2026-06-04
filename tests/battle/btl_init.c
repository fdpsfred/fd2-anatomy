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

#define USE_ITEM_ID 10

extern runtime_char g_test_rc_array[8];
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
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;


/* ---- Test: fd2_init_battle_state_for_chapter ---- */

/* fd2_init_battle_state_for_chapter calls the real (linked) chapter-battle-data
 * loader, which fopen+freads the staged real DAT archives via the real
 * fd2_load_dat_resource. Drive it end-to-end against real chapter 0 (total 4):
 * the asserted state (cursor/viewport zeroing, anim_phase, turn_counter) is
 * set by the function AFTER the load and is file-content-independent; the
 * load itself must run without faulting, which needs a valid in-process menu
 * roster (NOT a file) for the active-slot build and a 0x20 consumed-flags
 * buffer for the post-load memset. */
#define BI_ROSTER_SLOTS 8
static uint8  g_bi_roster[BI_ROSTER_SLOTS * 0x50];
static uint8 *g_bi_consumed;

static void setup_init_fixture(void)
{
    int i;

    data_fd2_chapter_current_chapter_id = 0;     /* real chapter 0 (total 4) */

    memset(g_bi_roster, 0, sizeof(g_bi_roster));
    for (i = 0; i < BI_ROSTER_SLOTS; i++) {
        g_bi_roster[i * 0x50 + 0x07] = (uint8)(0x40 + i);  /* portrait_id */
        g_bi_roster[i * 0x50 + 0x08] = (uint8)(0x80 + i);  /* char_id */
    }
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_bi_roster;
    data_fd2_shared_menu_party_member_count = BI_ROSTER_SLOTS;

    g_bi_consumed = (uint8 *)malloc(0x20);
    memset(g_bi_consumed, 0, 0x20);
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_bi_consumed;

    /* loader-returned pointer globals start NULL (loader's free(old_buf) no-op) */
    data_fd2_battle_runtime_char_array_ptr = NULL;
    portrait_sprite_cache = 0;
    current_chapter_text = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    battle_scene_snapshot = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
}

static void teardown_init_fixture(void)
{
    if (data_fd2_battle_runtime_char_array_ptr != NULL)
        free(data_fd2_battle_runtime_char_array_ptr);
    if (portrait_sprite_cache != 0)
        free((void *)portrait_sprite_cache);
    /* loader-returned buffers left live by the function */
    if (current_chapter_text != 0)
        free((void *)current_chapter_text);
    if (data_fd2_tile_event_data_table_ptr != 0)
        free((void *)data_fd2_tile_event_data_table_ptr);
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
    /* chapter_portrait_load_buffer freed+nulled by the function */

    free(g_bi_consumed);

    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_battle_tile_map_ptr = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
    portrait_sprite_cache = 0;
    current_chapter_text = 0;
    battle_scene_snapshot = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_graphics_static_bg_buffer_ptr = 0;
    data_fd2_graphics_animated_bg_buffer_ptr = 0;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_chapter_current_chapter_id = 1;

    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
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

    /* fresh portrait cache + the STAGED real FDICON.B24 for the real portrait
     * loader (the returned portrait_idx lands in sprite_state[0] and is not
     * asserted; only that the load runs without faulting). */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;
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
    remove("FD2.TMP");        /* generated swap file (not a staged game file) */
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


/* ---- Tests: fd2_init_runtime_char_from_base_growth ---- */

extern item_effect data_fd2_battle_item_effect_table[215];

static uint8 g_ircbg_roster[2 * RUNTIME_CHAR_SIZE + 4];

/* Populate slot 0 of the menu roster from base/growth tables; verify the
 * stat formulas (HP/MP use level-1, AP/DP/DX use level), the fixed slot
 * bytes, the inventory-mask encoding, the spell-bitmap memmove, and that
 * the equip-adjusted aggregates (+0x48..) land after the recompute pass
 * (with zero item boosts so they equal the base AP/DP/DX values). */
static void test_ircbg_player_stats(void)
{
    uint8 *base;
    uint8 *grow;
    uint8 *slot;
    uint8  prev_x;

    memset(g_ircbg_roster, 0xcd, sizeof(g_ircbg_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ircbg_roster;
    data_fd2_shared_menu_party_member_count = 0;

    /* seed slot +0..+4 with a sentinel; the function must leave them as-is */
    slot = g_ircbg_roster;
    slot[0] = 0xcd; slot[1] = 0xcd; slot[2] = 0xcd; slot[3] = 0xcd;
    slot[4] = 0xcd;
    prev_x = slot[0];

    base = (uint8 *)&data_fd2_battle_character_base_table[7];
    memset(base, 0, sizeof(character_base));
    base[0]  = 0xaa;                 /* +0x1F archetype */
    base[1]  = 0xbb;                 /* +0x20 job */
    base[2]  = 4;                    /* level */
    *(uint16 *)(base + 3)    = 40;   /* HP base */
    *(uint16 *)(base + 5)    = 12;   /* MP base */
    base[7]  = 0x55;                 /* magic resist */
    base[8]  = 0x11; base[9] = 0x22; /* spell bitmap source (+8..+0xB) */
    base[0xa] = 0x33; base[0xb] = 0x44;
    base[0xc] = 30;                  /* primary equip item id */
    base[0xd] = 31;                  /* secondary equip item id */
    base[0xe] = 0x70;                /* inv item 0 (present) */
    base[0xf] = 0xff;                /* inv item 1 (empty) */
    base[0x10] = 0x71;               /* inv item 2 (present) */
    base[0x11] = 0xff;               /* inv item 3 (empty) */
    *(uint16 *)(base + 0x12) = 6;    /* AP base */
    *(uint16 *)(base + 0x14) = 4;    /* DP base */
    *(uint16 *)(base + 0x16) = 8;    /* DX base */

    grow = (uint8 *)&data_fd2_battle_character_growth_table[7];
    memset(grow, 0, sizeof(character_growth));
    grow[0] = 2;   /* AP growth/level */
    grow[2] = 1;   /* DP growth/level */
    grow[4] = 3;   /* DX growth/level */
    grow[6] = 5;   /* HP growth/(level-1) */
    grow[8] = 2;   /* MP growth/(level-1) */

    /* zero-boost effect entries for the two equipped item ids */
    memset(&data_fd2_battle_item_effect_table[30], 0, sizeof(item_effect));
    memset(&data_fd2_battle_item_effect_table[31], 0, sizeof(item_effect));

    fd2_init_runtime_char_from_base_growth(7);

    /* +0..+4 untouched */
    ASSERT_EQ((long)slot[0], (long)prev_x);
    ASSERT_EQ((long)slot[4], 0xcd);
    /* fixed header bytes */
    ASSERT_EQ((long)slot[5], 0);
    ASSERT_EQ((long)slot[6], 2);
    ASSERT_EQ((long)slot[7], 7);
    ASSERT_EQ((long)slot[8], 7);
    ASSERT_EQ((long)slot[9], 0);
    /* equip slots */
    ASSERT_EQ((long)slot[0xa], 0x40);
    ASSERT_EQ((long)slot[0xb], 30);
    ASSERT_EQ((long)slot[0xc], 0x40);
    ASSERT_EQ((long)slot[0xd], 31);
    /* inventory loop: present -> mask 0, empty(0xff) -> mask 0x80 */
    ASSERT_EQ((long)slot[0xe], 0);    ASSERT_EQ((long)slot[0xf], 0x70);
    ASSERT_EQ((long)slot[0x10], 0x80); ASSERT_EQ((long)slot[0x11], 0xff);
    ASSERT_EQ((long)slot[0x12], 0);    ASSERT_EQ((long)slot[0x13], 0x71);
    ASSERT_EQ((long)slot[0x14], 0x80); ASSERT_EQ((long)slot[0x15], 0xff);
    /* extra empty slots */
    ASSERT_EQ((long)slot[0x16], 0x80);
    ASSERT_EQ((long)slot[0x18], 0x80);
    /* spell bitmap memmove from base+8 (4 bytes) */
    ASSERT_EQ((long)slot[0x1a], 0x11);
    ASSERT_EQ((long)slot[0x1b], 0x22);
    ASSERT_EQ((long)slot[0x1c], 0x33);
    ASSERT_EQ((long)slot[0x1d], 0x44);
    ASSERT_EQ((long)slot[0x1e], 0);
    /* identity bytes */
    ASSERT_EQ((long)slot[0x1f], 0xaa);
    ASSERT_EQ((long)slot[0x20], 0xbb);
    ASSERT_EQ((long)slot[0x21], 4);     /* level */
    /* status block zeroed */
    ASSERT_EQ((long)slot[0x22], 0);
    ASSERT_EQ((long)slot[0x27], 0);
    ASSERT_EQ((long)slot[0x31], 0xff);  /* pickup kind none */
    /* AP = base 6 + growth 2 * level 4 = 14 */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x37), 14);
    /* DP = base 4 + growth 1 * 4 = 8 */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x39), 8);
    ASSERT_EQ((long)slot[0x3b], 0x55);  /* magic resist */
    ASSERT_EQ((long)slot[0x3c], 0);     /* movement order */
    /* DX = base 8 + growth 3 * 4 = 20 */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x3e), 20);
    /* HP = base 40 + growth 5 * (level-1=3) = 55 */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x40), 55);
    ASSERT_EQ((long)*(uint16 *)(slot + 0x42), 55);
    /* MP = base 12 + growth 2 * 3 = 18 */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x44), 18);
    ASSERT_EQ((long)*(uint16 *)(slot + 0x46), 18);
    /* equip-adjusted aggregates (zero boosts) = base AP/DP/DX */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x48), 14);
    ASSERT_EQ((long)*(uint16 *)(slot + 0x4a), 8);
    ASSERT_EQ((long)*(uint16 *)(slot + 0x4c), 20);
    ASSERT_EQ((long)*(uint16 *)(slot + 0x4e), 20);
    /* member count incremented */
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count, 1);

    data_fd2_shared_menu_party_member_count = 0;
}

/* Level-1 boundary: HP/MP get NO growth (level-1 == 0), but AP/DP/DX
 * still get one growth tick (level factor == 1). Also exercises slot
 * index 1 (member_count == 1 -> second 0x50 stride). */
static void test_ircbg_level1_boundary(void)
{
    uint8 *base;
    uint8 *grow;
    uint8 *slot;

    memset(g_ircbg_roster, 0, sizeof(g_ircbg_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ircbg_roster;
    data_fd2_shared_menu_party_member_count = 1;

    base = (uint8 *)&data_fd2_battle_character_base_table[8];
    memset(base, 0, sizeof(character_base));
    base[2] = 1;                     /* level 1 */
    *(uint16 *)(base + 3)    = 25;   /* HP base */
    *(uint16 *)(base + 5)    = 7;    /* MP base */
    base[0xc] = 0xff;                /* primary equip empty id (mask still 0x40) */
    base[0xd] = 0xff;
    base[0xe] = 0xff; base[0xf] = 0xff;
    base[0x10] = 0xff; base[0x11] = 0xff;
    *(uint16 *)(base + 0x12) = 3;    /* AP base */
    *(uint16 *)(base + 0x14) = 2;    /* DP base */
    *(uint16 *)(base + 0x16) = 5;    /* DX base */

    grow = (uint8 *)&data_fd2_battle_character_growth_table[8];
    memset(grow, 0, sizeof(character_growth));
    grow[0] = 9;   /* AP growth */
    grow[2] = 4;   /* DP growth */
    grow[4] = 6;   /* DX growth */
    grow[6] = 100; /* HP growth (must NOT apply at level 1) */
    grow[8] = 50;  /* MP growth (must NOT apply at level 1) */

    /* equip ids 0xFF -> item_effect[255] is out of the 215 array, but the
     * mask bytes are 0x40 so recompute WILL look them up; give id 0xFF a
     * benign zero entry by clamping: use a real low id instead. */
    base[0xc] = 5; base[0xd] = 5;
    memset(&data_fd2_battle_item_effect_table[5], 0, sizeof(item_effect));

    fd2_init_runtime_char_from_base_growth(8);

    slot = g_ircbg_roster + 1 * RUNTIME_CHAR_SIZE;
    /* HP/MP: level-1 == 0, no growth */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x40), 25);
    ASSERT_EQ((long)*(uint16 *)(slot + 0x44), 7);
    /* AP/DP/DX: level factor 1 -> base + 1 growth tick */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x37), 12); /* 3 + 9 */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x39), 6);  /* 2 + 4 */
    ASSERT_EQ((long)*(uint16 *)(slot + 0x3e), 11); /* 5 + 6 */
    ASSERT_EQ((long)slot[0x21], 1);   /* level */
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count, 2);

    data_fd2_shared_menu_party_member_count = 0;
}


/* ---- Test: fd2_clear_all_chars_facing ----
 *
 * Loop zeros sprite_state[1] (byte +3) for exactly party_member_count
 * slots (stride RUNTIME_CHAR_SIZE), then calls the (stubbed) delay.
 * Verifies: cleared slots get +3 == 0 while their other bytes survive;
 * slots at/after the count are untouched; count==0 clears nothing.
 */
static void test_clear_all_chars_facing_partial(void)
{
    int i;

    memset(g_test_rc_array, 0xAA, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 3;

    fd2_clear_all_chars_facing();

    /* slots 0..2: facing byte (+3) cleared, neighbours preserved */
    for (i = 0; i < 3; i = i + 1) {
        ASSERT_EQ((long)g_test_rc_array[i].sprite_state[1], 0);
        ASSERT_EQ((long)g_test_rc_array[i].sprite_state[0], 0xAA);
        ASSERT_EQ((long)g_test_rc_array[i].sprite_state[2], 0xAA);
        ASSERT_EQ((long)((uint8 *)&g_test_rc_array[i])[2], 0xAA);
        ASSERT_EQ((long)((uint8 *)&g_test_rc_array[i])[4], 0xAA);
    }
    /* slots 3..7: completely untouched, including facing byte */
    for (i = 3; i < 8; i = i + 1) {
        ASSERT_EQ((long)g_test_rc_array[i].sprite_state[1], 0xAA);
    }

    data_fd2_battle_party_member_count = 0;
}

static void test_clear_all_chars_facing_zero_count(void)
{
    int i;

    memset(g_test_rc_array, 0xAA, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 0;

    fd2_clear_all_chars_facing();

    for (i = 0; i < 8; i = i + 1) {
        ASSERT_EQ((long)g_test_rc_array[i].sprite_state[1], 0xAA);
    }

    data_fd2_battle_party_member_count = 0;
}


/* ---- Test: fd2_clear_all_chars_acted_flag ----
 *
 * Loop masks flags (byte +5) with 0x7F for exactly party_member_count
 * slots (stride RUNTIME_CHAR_SIZE).
 * Verifies: cleared slots keep all bits except 0x80 (so 0xAA -> 0x2A);
 * neighbouring bytes survive; slots at/after the count are untouched;
 * count==0 clears nothing.
 */
static void test_clear_all_chars_acted_partial(void)
{
    int i;

    memset(g_test_rc_array, 0xAA, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 3;

    fd2_clear_all_chars_acted_flag();

    /* slots 0..2: flags (+5) acted bit cleared (0xAA -> 0x2A),
       neighbours preserved */
    for (i = 0; i < 3; i = i + 1) {
        ASSERT_EQ((long)g_test_rc_array[i].flags, 0x2A);
        ASSERT_EQ((long)((uint8 *)&g_test_rc_array[i])[4], 0xAA);
        ASSERT_EQ((long)g_test_rc_array[i].team, 0xAA);
    }
    /* slots 3..7: completely untouched, including flags byte */
    for (i = 3; i < 8; i = i + 1) {
        ASSERT_EQ((long)g_test_rc_array[i].flags, 0xAA);
    }

    data_fd2_battle_party_member_count = 0;
}

static void test_clear_all_chars_acted_zero_count(void)
{
    int i;

    memset(g_test_rc_array, 0xAA, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 0;

    fd2_clear_all_chars_acted_flag();

    for (i = 0; i < 8; i = i + 1) {
        ASSERT_EQ((long)g_test_rc_array[i].flags, 0xAA);
    }

    data_fd2_battle_party_member_count = 0;
}

/* recording stub for fd2_tile_blit_24x24_passthrough (defined in testglob.c) */
extern int    g_blitpass_calls;
extern uint32 g_blitpass_src[64];
extern uint32 g_blitpass_dst[64];
extern uint32 g_blitpass_stride[64];

/* ---- fd2_convert_battle_tiles_to_24px @ 0x1399C ----
 * Build an in-memory battle_scene_snapshot (uint16 tile_count @ +4,
 * int32 offset table @ +6), invoke the converter, and verify the
 * allocated bank header, the zeroed tile region, and every recorded
 * blit's (src, dst, stride) arithmetic. Covers the offset-table index
 * math and the i*0x240+6 dst computation (EAX-bug-prone arithmetic). */
static void test_convert_battle_tiles_to_24px(void)
{
    uint8  *snap;
    uint8  *bank;
    uint16  tile_count;
    int32   offsets[3];
    int     i;
    int     j;

    tile_count = 3;
    offsets[0] = 0x100;   /* arbitrary byte offsets into the snapshot */
    offsets[1] = 0x040;
    offsets[2] = 0x200;

    /* snapshot needs >= 6 + 3*4 header bytes plus room past the
       largest offset; allocate generously */
    snap = (uint8 *)malloc(0x400);
    ASSERT_TRUE(snap != NULL);
    memset(snap, 0, 0x400);
    *(uint16 *)(snap + 4) = tile_count;
    for (i = 0; i < 3; i = i + 1)
        *(int32 *)(snap + 6 + i * 4) = offsets[i];

    battle_scene_snapshot = (uint32)snap;
    g_blitpass_calls = 0;

    bank = (uint8 *)fd2_convert_battle_tiles_to_24px();
    ASSERT_TRUE(bank != NULL);

    /* 6-byte header */
    ASSERT_EQ((long)*(uint16 *)(bank + 0), 0x18);
    ASSERT_EQ((long)*(uint16 *)(bank + 2), 0x18);
    ASSERT_EQ((long)*(uint16 *)(bank + 4), (long)tile_count);

    /* tile region memset to 0 */
    for (j = 0; j < (int)(tile_count * 0x240); j = j + 1)
        ASSERT_EQ((long)bank[6 + j], 0);

    /* one blit per tile, in order, with correct src/dst/stride */
    ASSERT_EQ((long)g_blitpass_calls, (long)tile_count);
    for (i = 0; i < (int)tile_count; i = i + 1) {
        ASSERT_EQ((long)g_blitpass_src[i],
                  (long)((uint32)snap + offsets[i]));
        ASSERT_EQ((long)g_blitpass_dst[i],
                  (long)((uint32)bank + i * 0x240 + 6));
        ASSERT_EQ((long)g_blitpass_stride[i], 0x18);
    }

    free(bank);
    free(snap);
    battle_scene_snapshot = 0;
    g_blitpass_calls = 0;
}


/* ---- Tests: fd2_restore_all_chars_full_hp_mp @ 0x25089 ----
 *
 * Walks the menu/template roster (menu_party_roster_buffer_ptr, stride
 * RUNTIME_CHAR_SIZE) for menu_party_member_count slots and resets each to
 * "fully healed": flags(+0x05)=0, hp_current(+0x40):=hp_max(+0x42),
 * mp_current(+0x44):=mp_max(+0x46).
 * Verifies: restored slots get cleared flags and current:=max for HP/MP;
 * the *max* fields and neighbouring bytes survive; slots at/after the count
 * are completely untouched; count==0 changes nothing.
 */
static runtime_char g_rfh_roster[8];

static void test_restore_full_hp_mp_partial(void)
{
    int i;

    memset(g_rfh_roster, 0xAA, sizeof(g_rfh_roster));
    /* give each slot a distinct, deliberately-not-full HP/MP and dirty flags */
    for (i = 0; i < 8; i = i + 1) {
        g_rfh_roster[i].flags      = 0xAA;
        g_rfh_roster[i].hp_current = (uint16)(1 + i);
        g_rfh_roster[i].hp_max     = (uint16)(100 + i);
        g_rfh_roster[i].mp_current = (uint16)(2 + i);
        g_rfh_roster[i].mp_max     = (uint16)(50 + i);
    }
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_rfh_roster;
    data_fd2_shared_menu_party_member_count = 3;

    fd2_restore_all_chars_full_hp_mp();

    /* slots 0..2: flags cleared, current := max for HP and MP */
    for (i = 0; i < 3; i = i + 1) {
        ASSERT_EQ((long)g_rfh_roster[i].flags, 0);
        ASSERT_EQ((long)g_rfh_roster[i].hp_current, (long)(100 + i));
        ASSERT_EQ((long)g_rfh_roster[i].hp_max,     (long)(100 + i));
        ASSERT_EQ((long)g_rfh_roster[i].mp_current, (long)(50 + i));
        ASSERT_EQ((long)g_rfh_roster[i].mp_max,     (long)(50 + i));
        /* neighbouring bytes the loop must not touch */
        ASSERT_EQ((long)g_rfh_roster[i].team, 0xAA);            /* +0x06 */
        ASSERT_EQ((long)g_rfh_roster[i].sprite_state[2], 0xAA); /* +0x04 */
        ASSERT_EQ((long)g_rfh_roster[i].ap, 0xAAAA);            /* +0x48 */
    }
    /* slots 3..7: completely untouched */
    for (i = 3; i < 8; i = i + 1) {
        ASSERT_EQ((long)g_rfh_roster[i].flags, 0xAA);
        ASSERT_EQ((long)g_rfh_roster[i].hp_current, (long)(1 + i));
        ASSERT_EQ((long)g_rfh_roster[i].mp_current, (long)(2 + i));
    }

    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
}

static void test_restore_full_hp_mp_zero_count(void)
{
    int i;

    memset(g_rfh_roster, 0xAA, sizeof(g_rfh_roster));
    for (i = 0; i < 8; i = i + 1) {
        g_rfh_roster[i].flags      = 0xAA;
        g_rfh_roster[i].hp_current = (uint16)(1 + i);
        g_rfh_roster[i].hp_max     = (uint16)(100 + i);
    }
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_rfh_roster;
    data_fd2_shared_menu_party_member_count = 0;

    fd2_restore_all_chars_full_hp_mp();

    for (i = 0; i < 8; i = i + 1) {
        ASSERT_EQ((long)g_rfh_roster[i].flags, 0xAA);
        ASSERT_EQ((long)g_rfh_roster[i].hp_current, (long)(1 + i));
    }

    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_shared_menu_party_member_count = 0;
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
    RUN_TEST(test_ircbg_player_stats);
    RUN_TEST(test_ircbg_level1_boundary);
    RUN_TEST(test_clear_all_chars_facing_partial);
    RUN_TEST(test_clear_all_chars_facing_zero_count);
    RUN_TEST(test_clear_all_chars_acted_partial);
    RUN_TEST(test_clear_all_chars_acted_zero_count);
    RUN_TEST(test_restore_full_hp_mp_partial);
    RUN_TEST(test_restore_full_hp_mp_zero_count);
    RUN_TEST(test_convert_battle_tiles_to_24px);
    printf("\n");
}
