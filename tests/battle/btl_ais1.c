/*
 * unit tests for src/battle/btl_aisc.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

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
extern uint8 g_spell_list_buf[12];
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;

#include "battlfix.h"

static uint8 t_ai_attr_buf[8];


/* ---- Full scoring-path tests for fd2_ai_score_physical_attack @ 0x14237 ----
 *
 * Drive the real per-tile / per-target scoring loop end to end. All callees on
 * the path are the REAL functions: fd2_get_inventory_slot_item_id /
 * fd2_get_item_effect_entry / fd2_get_movement_cost_table_for_job (table.c),
 * fd2_check_char_status_immunity / fd2_read_tile_attribute_at_pos /
 * fd2_check_can_default_attack_target (battle.c), and
 * fd2_mark_char_occupant_tiles_for_team / fd2_collect_unmarked_tile_positions /
 * fd2_compute_aoe_targets (btl_ai.c), and the REAL
 * fd2_find_equipped_item_by_kind (the caster is equipped at slot 0 in
 * ti_setup_phys). Only fd2_init_movement_range_floodfill,
 * fd2_paint_threat_overlay_for_team and fd2_obfuscate_battle_tile_map are
 * stubs (no-ops that leave the tile map intact).
 *
 * Geometry (shared by ti_setup): 3x3 map, party_member_count=2.
 *   char 0 = caster, team 0 (TEAM_ENEMY), pos (0,0), non-immune.
 *   char 1 = target, team 2 (player), pos (1,1).
 * Only tile (1,1) is left unmarked (+7 != 0xFF); every other tile stays 0xFF
 * from reset_ai_stubs. The caster-occupant pass marks nothing (exclude_idx==0
 * skips the caster; char 1 is team 2 so the team_selector==0 pass ignores it),
 * so the candidate list is exactly [(1,1)]. ctx_flag==0 -> use_smaller_aoe=1 ->
 * fd2_compute_aoe_targets runs with team_filter==1, collecting char 1 (team!=0)
 * because its tile (1,1) is unmarked. aoe_radius (item range_min, +0xB) is 0 so
 * no radius re-marking; spell_range (item range_max, +0xC) is 1 (<0x10).
 * Both attacker and target are non-immune, so terrain modifiers are skipped and
 * effective_AP/DP == base AP/DP. With the single candidate equal to the target
 * tile (Manhattan distance 0) the counter-bonus default-attack check returns -1
 * (dist != 1), isolating raw_damage = AP - DP for tests 1-3.
 *
 * raw_damage paths (verified against disasm 0x14586/0x1458c kill at 0x1447b):
 *   raw <= 2            -> score_class 0
 *   raw  > 2            -> score_class 8
 *   raw  > target HP    -> raw *= 2, score_class 0x12  (kill-shot)
 * Best-slot update fires when score_class > best OR (== best && raw > tiebreak);
 * both start at 0. */
static void ti_setup_phys(uint32 *save_pmc, uint32 *save_w,
                          uint32 *save_h)
{
    *save_pmc = data_fd2_battle_party_member_count;
    *save_w = data_fd2_battle_map_width_tiles;
    *save_h = data_fd2_battle_map_height_tiles;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    reset_ai_stubs();                 /* tile map -> 0xFF, ptr wired */
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;

    g_test_rc_array[0].pos_x = 0;      /* caster */
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].inventory_slots[0] = 0x40;/* slot 0 equipped (REAL
                                                    find_equipped -> slot 0) */
    g_test_rc_array[0].inventory_slots[1] = 5;   /* equipped item id 5 */

    g_test_rc_array[1].pos_x = 1;      /* target on the single candidate */
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].team = 2;

    /* item 5: range_min(+0xB)=0 -> aoe_radius 0; range_max(+0xC)=1 -> range 1 */
    data_fd2_battle_item_effect_table[5].range_min = 0;
    data_fd2_battle_item_effect_table[5].range_max = 1;

    /* unmark tile (1,1) only: byte (y*w + x)*4 + 7 with w=3 */
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
}


static void ti_restore_phys(uint32 save_pmc, uint32 save_w,
                            uint32 save_h)
{
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
}


static void ti_setup_item(uint32 *save_pmc, uint32 *save_w, uint32 *save_h)
{
    *save_pmc = data_fd2_battle_party_member_count;
    *save_w = data_fd2_battle_map_width_tiles;
    *save_h = data_fd2_battle_map_height_tiles;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    reset_ai_stubs();                 /* tile map -> 0xFF, ptr wired */
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;

    g_test_rc_array[0].pos_x = 0;     /* caster */
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].team = 0;
    /* All slot flag bytes are clear (memset above), so the REAL
     * fd2_count_usable_inventory_slots returns 8. Slot 0 carries item id 7
     * (scored below); slots 1..7 carry item id 0, whose zeroed effect entry
     * has range_max(+0xD)==0 so the scorer skips them (continue) -> inert. */
    g_test_rc_array[0].inventory_slots[1] = 7;   /* slot 0 item id = 7 */

    data_fd2_battle_ai_best_item_target_x = 0xEE;
    data_fd2_battle_ai_best_item_target_y = 0xEE;
    data_fd2_battle_ai_best_item_slot = 0xEE;
}


static void ti_restore_item(uint32 save_pmc, uint32 save_w, uint32 save_h)
{
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
}


static void ts_setup_spell(uint32 *save_pmc, uint32 *save_w, uint32 *save_h,
                           int *save_ret)
{
    *save_pmc = data_fd2_battle_party_member_count;
    *save_w = data_fd2_battle_map_width_tiles;
    *save_h = data_fd2_battle_map_height_tiles;
    *save_ret = g_build_spell_list_return;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_spell_effect_table, 0,
           sizeof(data_fd2_battle_spell_effect_table));
    memset(g_spell_list_buf, 0, sizeof(g_spell_list_buf));
    reset_ai_stubs();                 /* tile map -> 0xFF, ptr wired */
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;

    g_test_rc_array[0].pos_x = 0;     /* caster */
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].mp_current = 50;
    g_test_rc_array[0].combat_aux_block[0] = 0;   /* +0x27 not silenced */

    data_fd2_battle_ai_best_spell_target_x = 0xEE;
    data_fd2_battle_ai_best_spell_target_y = 0xEE;
    data_fd2_battle_ai_best_spell_id = 0xEE;
}


static void ts_restore_spell(uint32 save_pmc, uint32 save_w, uint32 save_h,
                             int save_ret)
{
    data_fd2_battle_party_member_count = save_pmc;
    data_fd2_battle_map_width_tiles = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    g_build_spell_list_return = save_ret;
}


static void test_ai_score_phys_no_weapon(void)
{
    int result;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* caster (char 0) has no equipped slot -> REAL find_equipped returns
     * 0xFFFFFFFF -> early return 0, best score reset to 0. */
    data_fd2_battle_ai_best_physical_score = 99;
    result = fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ(result, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 0);
}


/* Normal hit: AP 20, DP 10 -> raw 10 (>2 -> class 8), HP 100 (no kill),
 * char_id != 0 (no flank), distance 0 (no counter). Best slots take the
 * single candidate (1,1) targeting char 1. */
static void test_ai_score_phys_normal_hit_score8(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    ti_setup_phys(&save_pmc, &save_w, &save_h);
    g_test_rc_array[0].ap = 20;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].char_id = 7;          /* non-leader: no flank */
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 1);
    ti_restore_phys(save_pmc, save_w, save_h);
}


/* Kill shot: AP 20, DP 10 -> raw 10 > HP 5 -> raw doubled, class 0x12. */
static void test_ai_score_phys_kill_shot_score12(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    ti_setup_phys(&save_pmc, &save_w, &save_h);
    g_test_rc_array[0].ap = 20;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].char_id = 7;
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 0x12);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 1);
    ti_restore_phys(save_pmc, save_w, save_h);
}


/* Negligible damage: AP 11, DP 10 -> raw 1 (<=2 -> class 0), no kill. Score
 * class stays 0 but the (score==best && raw>tiebreak) branch still writes the
 * target slots once (raw 1 > initial tiebreak 0). Covers the class-0 path and
 * the tiebreak-only update. */
static void test_ai_score_phys_negligible_score0(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    ti_setup_phys(&save_pmc, &save_w, &save_h);
    g_test_rc_array[0].ap = 11;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].char_id = 7;
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 1);
    ti_restore_phys(save_pmc, save_w, save_h);
}


/* Counter-bonus + flank: two unmarked candidate tiles (1,0) and (1,1); target
 * char 1 (leader: char_id 0, awake) sits on (1,1). For candidate (1,0) the
 * target is Manhattan-distance 1 away, so fd2_check_can_default_attack_target
 * returns 1 (awake, adjacent, weapon range_min 0<=1) and adds effective_DP -
 * target_AP; then the leader flank multiplies raw by 3/2. Candidate (1,1) has
 * distance 0 (no counter) and a lower final raw, so it loses the score-8
 * tiebreak. Best slot must therefore be (1,0).
 *   AP 30, DP(target) 10 -> raw 20 (class 8, 20<=HP100 no kill)
 *   counter: + caster_DP(10) - target_AP(8) = +2 -> 22
 *   flank: 22*3/2 = 33
 * Asserts the counter+flank candidate (1,0) won. */
static void test_ai_score_phys_counter_and_flank(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    ti_setup_phys(&save_pmc, &save_w, &save_h);
    /* second candidate tile (1,0) */
    t_ai_tile_map[(0 * 3 + 1) * 4 + 7] = 0;
    g_test_rc_array[0].ap = 30;
    g_test_rc_array[0].dp = 10;        /* caster effective_DP */
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].ap = 8;         /* target_AP for counter term */
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].char_id = 0;    /* leader -> flank */
    g_test_rc_array[1].status_sleep_flag = 0;  /* awake -> counter eligible */
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 0);
    ti_restore_phys(save_pmc, save_w, save_h);
}


/* Terrain modifier (signed *AP/100 IDIV at disasm 0x143f1): make the CASTER
 * immune (job 0x13) so the attacker terrain bonus applies; the target stays
 * non-immune so its DP is unmodified. Base AP 12, DP 10 -> base raw 2 (class 0).
 * The candidate tile (1,1) resolves to tile_id T via fd2_read_tile_attribute_at_pos:
 * the tile cell's sprite_idx word (cell+4) is 0, so attr_ptr = attr_buffer+0 and
 * tile_buf[5] = attr_buffer[1] = T. mv_modifier[T] = 50 (%):
 *   effective_AP = 12 + (50 * 12)/100 = 12 + 6 = 18  ->  raw = 18 - 10 = 8 (class 8).
 * Asserting score == 8 (not 0) proves the terrain IDIV path executed with the
 * expected magnitude; a missing/incorrect terrain read would leave raw 2 -> 0.
 * Tile-map bytes for the (1,1) cell (k=4, stride 4): overlay byte base+7+k*4=+23
 * cleared so the tile is the single candidate; sprite_idx word at cell+4 = +20/+21
 * zeroed. The target (also at (1,1)) is non-immune so no target terrain read. */
static void test_ai_score_phys_terrain_bonus_lifts_class(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    uint32 save_attr_ptr;
    ti_setup_phys(&save_pmc, &save_w, &save_h);
    save_attr_ptr = data_fd2_tile_attribute_flags_buffer_ptr;
    memset(t_ai_attr_buf, 0, sizeof(t_ai_attr_buf));
    t_ai_attr_buf[1] = 9;                 /* tile_id T = 9 */
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_ai_attr_buf;
    data_fd2_battle_tile_attr_mv_modifier_table[9] = 50;   /* +50% AP */
    data_fd2_battle_tile_attr_def_modifier_table[9] = 0;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 4] = 0;    /* sprite_idx low byte = 0 */
    t_ai_tile_map[(1 * 3 + 1) * 4 + 5] = 0;    /* sprite_idx high byte = 0 */

    g_test_rc_array[0].job_id = 0x13;     /* caster immune -> terrain applies */
    g_test_rc_array[0].ap = 12;
    g_test_rc_array[1].dp = 10;
    g_test_rc_array[1].hp_current = 100;
    g_test_rc_array[1].char_id = 7;
    data_fd2_battle_ai_best_physical_target_idx = 0xFF;
    fd2_ai_score_physical_attack(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_idx, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_physical_target_y, 1);
    data_fd2_tile_attribute_flags_buffer_ptr = save_attr_ptr;
    data_fd2_battle_tile_attr_mv_modifier_table[9] = 0;
    ti_restore_phys(save_pmc, save_w, save_h);
}


/* Short-range item (range_class 2 < 0x10), ctx_flag 0, target_side 0 ->
 * aoe_arg 1 -> team_filter 1 (team != 0). Single candidate tile (1,1) with a
 * team-2 target (char 1) on it -> n_targets 1. HP 5/100 -> score 8. Asserts the
 * best globals captured slot 0 at (1,1). */
static void test_ai_score_item_short_range_score8(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    ti_setup_item(&save_pmc, &save_w, &save_h);
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_item_effect_table[7].use_effect = 5;       /* +14 effect/gate */
    data_fd2_battle_item_effect_table[7].cast_range_flags = 2; /* +17 short range  */
    data_fd2_battle_item_effect_table[7].target_side = 0;      /* +18 -> aoe_arg 1  */
    data_fd2_battle_item_effect_table[7].area = 2;             /* +19 short aoe arg */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 1;
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].hp_max = 100;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;   /* candidate (1,1) */
    data_fd2_battle_ai_best_item_score = 99;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_slot, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 1);
    ti_restore_item(save_pmc, save_w, save_h);
}


/* Long-range item (range_class 0x12 -> line scan, step_count 2). Precompute
 * marks only the caster tile (0,0); candidate (0,2) stays unmarked. The line
 * scan from caster (0,0) toward (0,2) walks (0,1),(0,2) and collects the team-2
 * char on (0,2) (team_filter hardcoded 0 -> team != 0). HP 5/100 -> score 8.
 * Exercises the pItem[0x10]>=0x10 branch + EAX-return use from the line scan. */
static void test_ai_score_item_long_range_line(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    ti_setup_item(&save_pmc, &save_w, &save_h);
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_item_effect_table[7].use_effect = 5;          /* +14 */
    data_fd2_battle_item_effect_table[7].cast_range_flags = 0x12; /* +17 line, step 2 */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 0;
    g_test_rc_array[1].pos_y = 2;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].hp_max = 100;
    t_ai_tile_map[(2 * 3 + 0) * 4 + 7] = 0;   /* candidate (0,2) */
    data_fd2_battle_ai_best_item_score = 99;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_slot, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 2);
    ti_restore_item(save_pmc, save_w, save_h);
}


/* aoe_arg divergence on ctx_flag with target_side(+18)=2 (!=0):
 *   ctx_flag 0 -> aoe_arg = (pItem[0x11]==0)?1:0 = 0 -> team_filter 0 (team==0)
 *   ctx_flag 1 -> aoe_arg = pItem[0x11] = 2          -> team_filter 2 (team==1)
 * One team-0 char (char 2) sits on candidate (1,1). ctx 0 collects it (score 8);
 * ctx 1 (team_filter 2, no team-1 char) collects nothing -> no update -> score
 * stays 0 with target/slot untouched (sentinels). Short-range (range_class 1). */
static void test_ai_score_item_ctx_flag_aoe_arg(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    /* ctx_flag 0 path: team_filter 0 collects the team-0 char -> score 8 */
    ti_setup_item(&save_pmc, &save_w, &save_h);
    data_fd2_battle_party_member_count = 3;
    data_fd2_battle_item_effect_table[7].use_effect = 5;
    data_fd2_battle_item_effect_table[7].cast_range_flags = 1;  /* short */
    data_fd2_battle_item_effect_table[7].target_side = 2;       /* +18 != 0 */
    data_fd2_battle_item_effect_table[7].area = 2;
    g_test_rc_array[2].team = 0;
    g_test_rc_array[2].pos_x = 1;
    g_test_rc_array[2].pos_y = 1;
    g_test_rc_array[2].hp_current = 5;
    g_test_rc_array[2].hp_max = 100;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_item_score = 0;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 1);
    ti_restore_item(save_pmc, save_w, save_h);

    /* ctx_flag 1 path: team_filter 2 finds no team-1 char -> no update */
    ti_setup_item(&save_pmc, &save_w, &save_h);
    data_fd2_battle_party_member_count = 3;
    data_fd2_battle_item_effect_table[7].use_effect = 5;
    data_fd2_battle_item_effect_table[7].cast_range_flags = 1;
    data_fd2_battle_item_effect_table[7].target_side = 2;
    data_fd2_battle_item_effect_table[7].area = 2;
    g_test_rc_array[2].team = 0;
    g_test_rc_array[2].pos_x = 1;
    g_test_rc_array[2].pos_y = 1;
    g_test_rc_array[2].hp_current = 5;
    g_test_rc_array[2].hp_max = 100;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_item_score = 0;
    fd2_ai_score_item_use(0, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 0xEE);  /* untouched */
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_slot, 0xEE);
    ti_restore_item(save_pmc, save_w, save_h);
}


/* Non-offensive item: use_effect(+14)=0 -> pItem[0xD]==0 -> slot skipped before
 * any tile work. No global update; score reset to 0 at entry, target/slot keep
 * their sentinels. A target is present on an unmarked tile to prove the skip is
 * the gate (not an empty candidate list). */
static void test_ai_score_item_non_offensive_skip(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    ti_setup_item(&save_pmc, &save_w, &save_h);
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_item_effect_table[7].use_effect = 0;        /* +14 -> skip */
    data_fd2_battle_item_effect_table[7].cast_range_flags = 2;
    data_fd2_battle_item_effect_table[7].target_side = 0;
    data_fd2_battle_item_effect_table[7].area = 2;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 1;
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    g_test_rc_array[1].hp_max = 100;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_item_score = 77;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_slot, 0xEE);
    ti_restore_item(save_pmc, save_w, save_h);
}


/* Best-candidate gating across two long-range candidates. Candidates collected
 * row-major: (2,0) (y=0) before (0,2) (y=2). The line scan is per-candidate
 * (caster->candidate), so each tile resolves a distinct target with its own
 * score. Scenario A: (2,0) target score 3, (0,2) target score 8 -> the higher
 * later score overwrites (best 8 at (0,2)). Scenario B swaps the HPs: (2,0)
 * score 8 first, (0,2) score 3 second -> the lower later score does NOT
 * overwrite (best 8 stays at (2,0)). char1 on (2,0), char2 on (0,2); both
 * team 2 so the team_filter-0 line scan collects each. score 8 = hp 5/100;
 * score 3 = hp 40/100 (40 > 100/3=33 and 40 <= 100/2=50). */
static void test_ai_score_item_best_candidate_gating(void)
{
    uint32 save_pmc;
    uint32 save_w;
    uint32 save_h;
    /* Scenario A: higher (later) overwrites */
    ti_setup_item(&save_pmc, &save_w, &save_h);
    data_fd2_battle_party_member_count = 3;
    data_fd2_battle_item_effect_table[7].use_effect = 5;
    data_fd2_battle_item_effect_table[7].cast_range_flags = 0x12;  /* line, step 2 */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 2; g_test_rc_array[1].pos_y = 0;   /* on (2,0) */
    g_test_rc_array[1].hp_current = 40; g_test_rc_array[1].hp_max = 100;  /* score 3 */
    g_test_rc_array[2].team = 2;
    g_test_rc_array[2].pos_x = 0; g_test_rc_array[2].pos_y = 2;   /* on (0,2) */
    g_test_rc_array[2].hp_current = 5; g_test_rc_array[2].hp_max = 100;   /* score 8 */
    t_ai_tile_map[(0 * 3 + 2) * 4 + 7] = 0;   /* candidate (2,0) */
    t_ai_tile_map[(2 * 3 + 0) * 4 + 7] = 0;   /* candidate (0,2) */
    data_fd2_battle_ai_best_item_score = 0;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 2);
    ti_restore_item(save_pmc, save_w, save_h);

    /* Scenario B: lower (later) does NOT overwrite */
    ti_setup_item(&save_pmc, &save_w, &save_h);
    data_fd2_battle_party_member_count = 3;
    data_fd2_battle_item_effect_table[7].use_effect = 5;
    data_fd2_battle_item_effect_table[7].cast_range_flags = 0x12;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].pos_x = 2; g_test_rc_array[1].pos_y = 0;   /* on (2,0) */
    g_test_rc_array[1].hp_current = 5; g_test_rc_array[1].hp_max = 100;   /* score 8 */
    g_test_rc_array[2].team = 2;
    g_test_rc_array[2].pos_x = 0; g_test_rc_array[2].pos_y = 2;   /* on (0,2) */
    g_test_rc_array[2].hp_current = 40; g_test_rc_array[2].hp_max = 100;  /* score 3 */
    t_ai_tile_map[(0 * 3 + 2) * 4 + 7] = 0;
    t_ai_tile_map[(2 * 3 + 0) * 4 + 7] = 0;
    data_fd2_battle_ai_best_item_score = 0;
    fd2_ai_score_item_use(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_score, 8);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_x, 2);
    ASSERT_EQ((long)data_fd2_battle_ai_best_item_target_y, 0);
    ti_restore_item(save_pmc, save_w, save_h);
}


/* Gate: no castable spell (count 0) -> early return at entry. ai_best_spell_score
 * is cleared to 0 first thing; target/id sentinels stay untouched. A live target
 * sits on an unmarked tile to prove the gate (not an empty candidate list) is what
 * stops the scan. */
static void test_ai_spell_gate_no_castable(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 0;                 /* gate fail */
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].area = 2;
    g_spell_list_buf[0] = 0;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 99;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0xEE);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}


/* Gate: caster combat_aux_block[0] (+0x27, silence) != 0 -> early return even with
 * a castable spell and a valid target present. */
static void test_ai_spell_gate_silenced(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 5;
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 0;
    g_test_rc_array[0].combat_aux_block[0] = 1;    /* +0x27 silenced */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 77;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0xEE);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}


/* MP gate: the single castable spell costs more MP (+5 mp_cost) than the caster's
 * mp_current -> spell skipped before any tile work -> no update, score stays 0. A
 * killable target is present to prove the MP gate is the stopper. */
static void test_ai_spell_mp_gate_skips(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 99;   /* > caster mp 50 */
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 0;
    g_test_rc_array[0].mp_current = 50;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0xEE);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}


/* Happy path: one castable damage spell (id 0, damage 50, mp_cost 5, area 2,
 * target_side 0 -> ctx_flag 0 gives aoe_arg 1 -> team_filter 1 collects team!=0).
 * One team-2 target (char 1) on candidate tile (1,1), hp 5 < damage 50, char_id 9
 * (!=0 -> no x1.5) -> per_score 0x18 = 24. The expected score is cross-checked
 * against the REAL fd2_score_spell_candidate with the same (spell_id, target) so
 * no value is assumed. Asserts the best globals captured (1,1) and spell id 0. */
static void test_ai_spell_happy_path_capture(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    uint8 xcheck_buf[1];
    int expected;
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;                        /* spell id 0 */
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 5;
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 0;
    g_test_rc_array[0].mp_current = 50;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;         /* candidate + target tile (1,1) */
    data_fd2_battle_ai_best_spell_score = 0;

    /* ground truth from the real scorer for spell 0 against target char 1 */
    xcheck_buf[0] = 1;
    expected = fd2_score_spell_candidate(0, 1, (uint32)xcheck_buf);

    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, (long)expected);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0x18);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_y, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}


/* aoe_arg divergence on ctx_flag with target_side(+6)=2 (!=0):
 *   ctx_flag 0 -> aoe_arg = (pSpell[6]==0)?1:0 = 0 -> team_filter 0 (team==0)
 *   ctx_flag 1 -> aoe_arg = pSpell[6] = 2          -> team_filter 2 (team==1)
 * A team-0 char (char 1) sits on candidate (1,1). ctx 0 collects it (kill shot,
 * char_id 9 -> score 0x18); ctx 1 (team_filter 2, no team-1 char) collects
 * nothing -> no update -> score stays 0, target/id keep sentinels. Same map/target
 * for both runs, only ctx_flag differs. */
static void test_ai_spell_ctx_flag_aoe_arg(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    /* ctx_flag 0: team_filter 0 collects the team-0 char -> score 0x18 */
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 5;
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 2;   /* +6 != 0 */
    g_test_rc_array[1].team = 0;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0x18);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_y, 1);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);

    /* ctx_flag 1: team_filter 2 finds no team-1 char -> no update */
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 1;
    g_spell_list_buf[0] = 0;
    data_fd2_battle_spell_effect_table[0].damage = 50;
    data_fd2_battle_spell_effect_table[0].mp_cost = 5;
    data_fd2_battle_spell_effect_table[0].area = 2;
    data_fd2_battle_spell_effect_table[0].target_side = 2;
    g_test_rc_array[1].team = 0;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 1);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_target_x, 0xEE);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 0xEE);
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}


/* base_dmg tiebreak (asm 0x15aec-0x15af3, untested by any other test): when a
 * later candidate produces a score EQUAL to the current best, it overwrites only
 * if its spell 'damage' (base_dmg) is strictly greater than the best so far.
 * Both spells (ids 2 and 3) are damage spells; the single target (char 1, hp 5,
 * char_id 9) is a kill shot for any damage > 5, so BOTH score 0x18 (equal). One
 * candidate tile so target coords are fixed; spell-list order decides which sets
 * best_dmg first.
 *   Scenario A: list [2(dmg50), 3(dmg60)] -> spell 3 score==best, 60>50 -> wins.
 *   Scenario B: list [2(dmg60), 3(dmg50)] -> spell 3 score==best, 50<=60 -> JLE,
 *               best stays spell 2. */
static void test_ai_spell_base_dmg_tiebreak(void)
{
    uint32 save_pmc, save_w, save_h;
    int save_ret;
    /* Scenario A: later higher base_dmg wins the equal-score tiebreak */
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 2;
    g_spell_list_buf[0] = 2;                        /* evaluated first */
    g_spell_list_buf[1] = 3;                        /* evaluated second */
    data_fd2_battle_spell_effect_table[2].damage = 50;
    data_fd2_battle_spell_effect_table[2].mp_cost = 5;
    data_fd2_battle_spell_effect_table[2].area = 2;
    data_fd2_battle_spell_effect_table[2].target_side = 0;
    data_fd2_battle_spell_effect_table[3].damage = 60;
    data_fd2_battle_spell_effect_table[3].mp_cost = 5;
    data_fd2_battle_spell_effect_table[3].area = 2;
    data_fd2_battle_spell_effect_table[3].target_side = 0;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0x18);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 3);   /* higher base_dmg */
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);

    /* Scenario B: later lower base_dmg does NOT overwrite (JLE skip) */
    ts_setup_spell(&save_pmc, &save_w, &save_h, &save_ret);
    data_fd2_battle_party_member_count = 2;
    g_build_spell_list_return = 2;
    g_spell_list_buf[0] = 2;
    g_spell_list_buf[1] = 3;
    data_fd2_battle_spell_effect_table[2].damage = 60;
    data_fd2_battle_spell_effect_table[2].mp_cost = 5;
    data_fd2_battle_spell_effect_table[2].area = 2;
    data_fd2_battle_spell_effect_table[2].target_side = 0;
    data_fd2_battle_spell_effect_table[3].damage = 50;
    data_fd2_battle_spell_effect_table[3].mp_cost = 5;
    data_fd2_battle_spell_effect_table[3].area = 2;
    data_fd2_battle_spell_effect_table[3].target_side = 0;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].char_id = 9;
    g_test_rc_array[1].pos_x = 1; g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].hp_current = 5;
    t_ai_tile_map[(1 * 3 + 1) * 4 + 7] = 0;
    data_fd2_battle_ai_best_spell_score = 0;
    fd2_ai_score_offensive_spell(0, 0);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_score, 0x18);
    ASSERT_EQ((long)data_fd2_battle_ai_best_spell_id, 2);   /* first kept */
    ts_restore_spell(save_pmc, save_w, save_h, save_ret);
}


/* ---- Test: fd2_score_spell_candidate ---- */

static void test_spell_score_damage_kill_shot(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].char_id = 5;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 30;
    data_fd2_battle_spell_effect_table[2].damage = 50;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(2, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 0x18);
}


static void test_spell_score_damage_non_kill(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[1].char_id = 5;
    *(uint16 *)((uint8 *)&g_test_rc_array[1] + 0x40) = 200;
    data_fd2_battle_spell_effect_table[3].damage = 50;
    tgt_buf[0] = 1;
    score = fd2_score_spell_candidate(3, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 8);
}


static void test_spell_score_damage_priority_enemy(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].char_id = 0;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 200;
    data_fd2_battle_spell_effect_table[1].damage = 50;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(1, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 12);
}


static void test_spell_score_heal_critical(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 10;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 100;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0xd, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 8);
}


static void test_spell_score_heal_moderate(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 40;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 100;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0xd, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 3);
}


static void test_spell_score_heal_full(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 90;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 100;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0xd, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 0);
}


static void test_spell_score_heal_boost_doubles(void)
{
    uint8 tgt_buf[2];
    int score;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 10;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 100;
    ((uint8 *)&g_test_rc_array[0])[0x34] = 0x01;
    tgt_buf[0] = 0;
    score = fd2_score_spell_candidate(0xd, 1, (uint32)tgt_buf);
    ASSERT_EQ((long)score, 16);
}


void run_battle_btl_aisc1_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: battle/btl_aisc (1/2)\n");
    RUN_TEST(test_ai_score_phys_no_weapon);
    RUN_TEST(test_ai_score_phys_normal_hit_score8);
    RUN_TEST(test_ai_score_phys_kill_shot_score12);
    RUN_TEST(test_ai_score_phys_negligible_score0);
    RUN_TEST(test_ai_score_phys_counter_and_flank);
    RUN_TEST(test_ai_score_phys_terrain_bonus_lifts_class);
    RUN_TEST(test_ai_score_item_short_range_score8);
    RUN_TEST(test_ai_score_item_long_range_line);
    RUN_TEST(test_ai_score_item_ctx_flag_aoe_arg);
    RUN_TEST(test_ai_score_item_non_offensive_skip);
    RUN_TEST(test_ai_score_item_best_candidate_gating);
    RUN_TEST(test_ai_spell_gate_no_castable);
    RUN_TEST(test_ai_spell_gate_silenced);
    RUN_TEST(test_ai_spell_mp_gate_skips);
    RUN_TEST(test_ai_spell_happy_path_capture);
    RUN_TEST(test_ai_spell_ctx_flag_aoe_arg);
    RUN_TEST(test_ai_spell_base_dmg_tiebreak);
    RUN_TEST(test_spell_score_damage_kill_shot);
    RUN_TEST(test_spell_score_damage_non_kill);
    RUN_TEST(test_spell_score_damage_priority_enemy);
    RUN_TEST(test_spell_score_heal_critical);
    RUN_TEST(test_spell_score_heal_moderate);
    RUN_TEST(test_spell_score_heal_full);
    RUN_TEST(test_spell_score_heal_boost_doubles);
    printf("\n");
}
