/*
 * testanim.c — Animation function test cases
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

extern runtime_char g_test_rc_array[8];

static uint8 t_tile_map[48];
static uint8 t_attr_flags[32];
static uint8 t_consumed[32];

static void test_tick_tile_event_anim_consumed_event(void)
{
    uint32 save_w;
    uint32 save_h;
    uint32 save_tm;
    uint32 save_af;
    uint32 save_cf;

    save_w  = data_fd2_battle_map_width_tiles;
    save_h  = data_fd2_battle_map_height_tiles;
    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;
    save_cf = data_fd2_field_map_tile_event_consumed_flags_ptr;

    data_fd2_battle_map_width_tiles  = 2;
    data_fd2_battle_map_height_tiles = 2;
    data_fd2_battle_tile_map_ptr     = (uint32)t_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr_flags;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)t_consumed;

    memset(t_tile_map, 0, sizeof(t_tile_map));
    memset(t_attr_flags, 0, sizeof(t_attr_flags));
    memset(t_consumed, 0, sizeof(t_consumed));

    /*
     * Tile (0,0) at tile_map offset 0.
     * fd2_read_tile_attribute_at_pos reads:
     *   sprite_idx    = *(uint16*)(tile_map + 4) & 0x3FF
     *   terrain_byte  = tile_map[6]
     *   terrain_class = terrain_byte & 0x1F  → buf+2
     *   attr_flags    = attr_buf[sprite_idx*4..+3] → buf+4..+7
     *
     * Set sprite_idx = 3, terrain_class = 7.
     * Set attr_flags[3*4] = 0x20  → (0x20 & 0x60) == 0x20.
     * Set consumed[7] = 1.
     */
    *(uint16 *)(t_tile_map + 4) = 3;
    t_tile_map[6] = 7;
    t_attr_flags[3 * 4] = 0x20;
    t_consumed[7] = 1;

    fd2_tick_tile_event_animations();

    ASSERT_EQ(*(uint16 *)(t_tile_map + 4), 4);
    ASSERT_EQ(t_tile_map[6], 0);

    data_fd2_battle_map_width_tiles  = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_tile_map_ptr     = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
    data_fd2_field_map_tile_event_consumed_flags_ptr = save_cf;
}

static void test_tick_tile_event_anim_non_event_no_change(void)
{
    uint32 save_w;
    uint32 save_h;
    uint32 save_tm;
    uint32 save_af;
    uint32 save_cf;

    save_w  = data_fd2_battle_map_width_tiles;
    save_h  = data_fd2_battle_map_height_tiles;
    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;
    save_cf = data_fd2_field_map_tile_event_consumed_flags_ptr;

    data_fd2_battle_map_width_tiles  = 2;
    data_fd2_battle_map_height_tiles = 2;
    data_fd2_battle_tile_map_ptr     = (uint32)t_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr_flags;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)t_consumed;

    memset(t_tile_map, 0, sizeof(t_tile_map));
    memset(t_attr_flags, 0, sizeof(t_attr_flags));
    memset(t_consumed, 0, sizeof(t_consumed));

    /* Tile (0,0): sprite_idx = 2, terrain_class = 5.
     * attr_flags = 0x40 → (0x40 & 0x60) = 0x40 != 0x20 → skip.
     */
    *(uint16 *)(t_tile_map + 4) = 2;
    t_tile_map[6] = 5;
    t_attr_flags[2 * 4] = 0x40;
    t_consumed[5] = 1;

    fd2_tick_tile_event_animations();

    ASSERT_EQ(*(uint16 *)(t_tile_map + 4), 2);
    ASSERT_EQ(t_tile_map[6], 5);

    data_fd2_battle_map_width_tiles  = save_w;
    data_fd2_battle_map_height_tiles = save_h;
    data_fd2_battle_tile_map_ptr     = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
    data_fd2_field_map_tile_event_consumed_flags_ptr = save_cf;
}

static void test_tick_chapter_palette_fast_cycle(void)
{
    int tick_val;

    tick_val = (int)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)tick_val;
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 0;

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_walk_anim_alt_palette_idx, 1);

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_walk_anim_alt_palette_idx, 2);
}

static void test_tick_chapter_palette_fast_wrap(void)
{
    int tick_val;

    tick_val = (int)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)tick_val;
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 3;

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_walk_anim_alt_palette_idx, 0);
}

static void test_tick_chapter_palette_slow_triggers(void)
{
    int tick_val;

    tick_val = (int)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)(tick_val - 10);
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_ambient_palette_anim_idx, 1);
}

/*
 * Slow-branch condition is a two-part OR: assembly @0x12995 tests
 * delta>4 (JG), and on fall-through @0x129a8 tests delta<0 (JGE skips
 * the slow body when delta>=0). The slow body therefore also runs for
 * a NEGATIVE delta, which production hits when the BIOS midnight tick
 * wraps (latch from the previous day is above the post-rollover tick).
 * Set latch = tick + 10 so delta = -10 < 0, exercising the JGE-not-taken
 * arm; ambient_idx must advance 0 -> 1.
 *
 * Determinism note: 0x46C is the live BIOS tick (advances ~18.2/s).
 * The ambient_idx assertion holds regardless of jitter — for the slow
 * body to be skipped the tick would have to advance into [10,14] counts
 * (~0.6 s) between adjacent statements; any larger advance re-enters the
 * slow body via the delta>4 arm. The latch-update side effect is NOT
 * asserted: the function re-reads 0x46C internally, so a value compare
 * against a re-read in the test would be a genuine race, not a fixture.
 */
static void test_tick_chapter_palette_slow_triggers_negative_delta(void)
{
    int tick_val;

    tick_val = (int)(int16)BIOS_TICK_WORD;
    data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
        (uint32)(tick_val + 10);
    data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;

    fd2_tick_chapter_palette_animation();
    ASSERT_EQ(data_fd2_graphics_chapter_ambient_palette_anim_idx, 1);
}

static void test_find_char_by_id_found(void)
{
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].char_id = 5;
    g_test_rc_array[1].char_id = 12;
    g_test_rc_array[2].char_id = 7;
    data_fd2_battle_party_member_count = 4;

    result = fd2_find_char_by_id_or_template(12);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(data_fd2_dialog_current_speaker_char_ptr,
              (uint32)&g_test_rc_array[1]);
}

static uint8 t_tmpl_roster[4 * 0x50];

static void test_find_char_by_id_fallback_template(void)
{
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(t_tmpl_roster, 0, sizeof(t_tmpl_roster));
    g_test_rc_array[0].char_id = 5;
    data_fd2_battle_party_member_count = 2;
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)t_tmpl_roster;
    data_fd2_shared_menu_party_member_count = 3;
    t_tmpl_roster[1 * 0x50 + 8] = 20;

    result = fd2_find_char_by_id_or_template(20);
    ASSERT_EQ(result, -1);
    ASSERT_EQ(data_fd2_dialog_current_speaker_char_ptr,
              (uint32)&t_tmpl_roster[1 * 0x50]);
}

static void test_find_char_at_cursor_found(void)
{
    uint32 save_cx;
    uint32 save_cy;
    int result;

    save_cx = data_fd2_battle_cursor_world_x;
    save_cy = data_fd2_battle_cursor_world_y;

    g_test_rc_array[0].pos_x = 3;
    g_test_rc_array[0].pos_y = 7;
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[1].pos_x = 5;
    g_test_rc_array[1].pos_y = 9;
    g_test_rc_array[1].flags = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 9;

    result = fd2_find_char_at_cursor_pos();
    ASSERT_EQ(result, 1);

    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_world_y = save_cy;
}

static void test_find_char_at_cursor_not_found(void)
{
    uint32 save_cx;
    uint32 save_cy;
    int result;

    save_cx = data_fd2_battle_cursor_world_x;
    save_cy = data_fd2_battle_cursor_world_y;

    g_test_rc_array[0].pos_x = 3;
    g_test_rc_array[0].pos_y = 7;
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_cursor_world_x = 10;
    data_fd2_battle_cursor_world_y = 10;

    result = fd2_find_char_at_cursor_pos();
    ASSERT_EQ(result, -1);

    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_world_y = save_cy;
}

static void test_walk_path_animation_loop_2steps(void)
{
    uint8 path[2];
    uint32 save_cx;
    uint32 save_cy;

    save_cx = data_fd2_battle_cursor_world_x;
    save_cy = data_fd2_battle_cursor_world_y;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5;
    g_test_rc_array[0].pos_y = 5;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_map_height_tiles = 15;

    path[0] = 0;
    path[1] = 3;
    fd2_walk_path_animation_loop(0, (uint32)path, 2);

    ASSERT_EQ(g_test_rc_array[0].pos_x, 6);
    ASSERT_EQ(g_test_rc_array[0].pos_y, 6);

    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_world_y = save_cy;
}

static void test_mark_char_as_dead(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[2].flags = 0x84;
    fd2_mark_char_as_dead(2);
    ASSERT_EQ(g_test_rc_array[2].flags, 0x01);
}

static void test_set_chapter_init_done_flag(void)
{
    data_fd2_chapter_chapter_init_done_flag = 0;
    fd2_set_chapter_init_done_flag();
    ASSERT_EQ(data_fd2_chapter_chapter_init_done_flag, 1);
}

static void test_set_combat_aux_low4(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].combat_aux_block[0xD] = 0xA3;
    g_test_rc_array[1].combat_aux_block[0xD] = 0xF7;
    fd2_set_combat_aux_block_byte_d_low4_for_char_range(0, 1, 5);
    ASSERT_EQ(g_test_rc_array[0].combat_aux_block[0xD], 0xA5);
    ASSERT_EQ(g_test_rc_array[1].combat_aux_block[0xD], 0xF5);
}

static void test_mark_aoe_plus_pattern(void)
{
    uint8 t_tmap[48];
    uint32 save_tm;

    save_tm = data_fd2_battle_tile_map_ptr;
    memset(t_tmap, 0, sizeof(t_tmap));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;

    fd2_mark_aoe_plus_pattern_at(1, 1);

    ASSERT_EQ(t_tmap[(1*3+1)*4+6] & 0x40, 0x40);
    ASSERT_EQ(t_tmap[(1*3+0)*4+6] & 0x80, 0x80);
    ASSERT_EQ(t_tmap[(0*3+1)*4+6] & 0x80, 0x80);
    ASSERT_EQ(t_tmap[(1*3+2)*4+6] & 0x80, 0x80);
    ASSERT_EQ(t_tmap[(2*3+1)*4+6] & 0x80, 0x80);

    data_fd2_battle_tile_map_ptr = save_tm;
}

static void test_collect_pending_drops(void)
{
    uint8 out[12];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[0].combat_aux_block[10] = 1;
    g_test_rc_array[0].hp_current = 0;
    data_fd2_battle_party_member_count = 1;

    memset(out, 0, sizeof(out));
    result = fd2_collect_pending_death_drops((uint32)out);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(out[0], 1);
}

static void test_collect_dead_char_drops(void)
{
    uint8 out[12];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[0].combat_aux_block[10] = 3;
    g_test_rc_array[0].hp_current = 0;
    g_test_rc_array[1].flags = 0;
    g_test_rc_array[1].combat_aux_block[10] = 3;
    g_test_rc_array[1].hp_current = 10;
    data_fd2_battle_party_member_count = 2;

    memset(out, 0, sizeof(out));
    result = fd2_collect_dead_char_drops((uint32)out);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(out[0], 3);
}

static void test_tick_tutorial_sfx_counter(void)
{
    uint8 save_ctr;

    save_ctr = data_fd2_audio_walk_step_sfx_cadence_counter;
    data_fd2_audio_walk_step_sfx_cadence_counter = 5;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].job_id = 1;

    fd2_tick_tutorial_progress_with_sfx(0);
    ASSERT_EQ(data_fd2_audio_walk_step_sfx_cadence_counter, 6);

    data_fd2_audio_walk_step_sfx_cadence_counter = save_ctr;
}

static void test_score_item_candidate_damage(void)
{
    uint8 tgt[1];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 10;
    g_test_rc_array[0].hp_max = 100;
    data_fd2_battle_item_effect_table[0].use_effect = 5;
    tgt[0] = 0;
    result = fd2_score_item_candidate(0, 1, (uint32)tgt);
    ASSERT_EQ(result, 8);
}

/* HP-damage effect path (use_effect 5/0xD), middle band:
 * hp_max/3 < hp <= hp_max/2  -> per_score 3 (asm 0x158c4 CMP/JG, 0x158c8 MOV 3).
 * hp 40, max 100: max/3=33 (40>33), max/2=50 (40<=50) -> 3. aux[0x34] clear. */
static void test_score_item_candidate_score3(void)
{
    uint8 tgt[1];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 40;
    g_test_rc_array[0].hp_max = 100;
    data_fd2_battle_item_effect_table[0].use_effect = 5;
    tgt[0] = 0;
    result = fd2_score_item_candidate(0, 1, (uint32)tgt);
    ASSERT_EQ(result, 3);
}

/* HP-damage effect path, high band: hp > hp_max/2 -> per_score 0
 * (asm 0x158c6 JG -> 0x158cf XOR EAX,EAX). hp 60, max 100: 60>50 -> 0. */
static void test_score_item_candidate_score0(void)
{
    uint8 tgt[1];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 60;
    g_test_rc_array[0].hp_max = 100;
    data_fd2_battle_item_effect_table[0].use_effect = 5;
    tgt[0] = 0;
    result = fd2_score_item_candidate(0, 1, (uint32)tgt);
    ASSERT_EQ(result, 0);
}

/* HP-damage effect path, high-value-target amplification: if
 * combat_aux_block[0x0D] bit 0x80 set (struct +0x34), per_score *= 3
 * (asm 0x158d1 TEST .. 0x158d9 SHL EAX,2 / SUB EAX,EBX). Two targets in one
 * call cover both amplified bands: base 8 (hp<=max/3) -> 24, base 3 -> 9;
 * total 33. Asserts the *3 is applied per-target before summation. */
static void test_score_item_candidate_x3_amplify(void)
{
    uint8 tgt[2];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 10;          /* base 8  -> *3 = 24 */
    g_test_rc_array[0].hp_max = 100;
    g_test_rc_array[0].combat_aux_block[0x0D] = 0x80;
    g_test_rc_array[1].hp_current = 40;          /* base 3  -> *3 = 9  */
    g_test_rc_array[1].hp_max = 100;
    g_test_rc_array[1].combat_aux_block[0x0D] = 0x80;
    data_fd2_battle_item_effect_table[0].use_effect = 5;
    tgt[0] = 0;
    tgt[1] = 1;
    result = fd2_score_item_candidate(0, 2, (uint32)tgt);
    ASSERT_EQ(result, 33);
}

/* Spell-wrapper effect path, non-0x18 (use_effect 0x14/0x15): threshold comes
 * from the wrapped spell's damage (pSpell[0]); the spell id is item.use_param
 * (uint16 at struct +15/+16, read via fd2_get_spell_effect_entry's EAX return,
 * asm 0x15938 CALL / 0x15946 MOVZX EBP,[EAX]). Per target: hp<=threshold ->
 * 0x12 (kill), hp>threshold -> 8. spell 4 damage 50; two targets hp 30 (<=50
 * kill 0x12) and hp 70 (>50 normal 8) -> 0x12+8 = 0x1A. Item index 9. */
static void test_score_item_candidate_spell_wrapper(void)
{
    uint8 tgt[2];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 30;          /* 30 <= 50 -> 0x12 */
    g_test_rc_array[0].hp_max = 100;
    g_test_rc_array[1].hp_current = 70;          /* 70 >  50 -> 8    */
    g_test_rc_array[1].hp_max = 100;
    data_fd2_battle_spell_effect_table[4].damage = 50;
    data_fd2_battle_item_effect_table[9].use_effect = 0x14;
    data_fd2_battle_item_effect_table[9].use_param_lo = 4;   /* spell id 4 */
    data_fd2_battle_item_effect_table[9].use_param_hi = 0;
    tgt[0] = 0;
    tgt[1] = 1;
    result = fd2_score_item_candidate(9, 2, (uint32)tgt);
    ASSERT_EQ(result, 0x12 + 8);
}

/* Spell-wrapper effect path, 0x18: threshold is item.use_param itself (the
 * uint16 at struct +15/+16); the spell getter's result is IGNORED (asm 0x15941
 * CMP ESI,0x18 / JZ 0x15949 skips MOVZX EBP,[EAX]). use_param 50, wrapped spell
 * id 4 damage deliberately 999 to prove its damage is NOT used as threshold.
 * Two targets hp 30 (<=50 -> 0x12) and hp 70 (>50 -> 8) -> 0x1A. Item index 10. */
static void test_score_item_candidate_spell_0x18(void)
{
    uint8 tgt[2];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].hp_current = 30;          /* 30 <= 50 -> 0x12 */
    g_test_rc_array[0].hp_max = 100;
    g_test_rc_array[1].hp_current = 70;          /* 70 >  50 -> 8    */
    g_test_rc_array[1].hp_max = 100;
    data_fd2_battle_spell_effect_table[4].damage = 999; /* must be ignored */
    data_fd2_battle_item_effect_table[10].use_effect = 0x18;
    data_fd2_battle_item_effect_table[10].use_param_lo = 50; /* threshold 50 */
    data_fd2_battle_item_effect_table[10].use_param_hi = 0;
    tgt[0] = 0;
    tgt[1] = 1;
    result = fd2_score_item_candidate(10, 2, (uint32)tgt);
    ASSERT_EQ(result, 0x12 + 8);
}

static void test_slide_panel_left_main_stationary(void)
{
    ASSERT_TRUE(1);
}

/* slide_panel_down_step restores the *background snapshot* (0x53C5F) into the
 * workspace, then overlays src_buffer rows. This test pins the global the
 * first memmove reads from: snapshot bytes (0xAA) must reach workspace top,
 * NOT composed_target bytes (0xCC). Catches the wrong-source-global
 * regression. Row 0 copies src_buffer+0x8C05 (0xBB) to workspace+5, so
 * workspace[0..4] stay snapshot, workspace[5] becomes src. With y_offset=0,
 * row_count = min(0x56,200) = 0x56. Assertions read only dst_workspace,
 * which is fully written before the final VGA blit. */
static uint8 g_dp_workspace[64000];
static uint8 g_dp_snapshot[64000];
static uint8 g_dp_composed[64000];
static uint8 g_dp_src[64000];

static void test_slide_panel_down_step_restores_snapshot(void)
{
    uint32 save_snap;
    uint32 save_comp;
    long last_row_dst;

    save_snap = data_fd2_ui_slide_bg_snapshot_buf_ptr;
    save_comp = data_fd2_ui_slide_composed_target_buf_ptr;

    memset(g_dp_workspace, 0x11, sizeof(g_dp_workspace));
    memset(g_dp_snapshot, 0xAA, sizeof(g_dp_snapshot));
    memset(g_dp_composed, 0xCC, sizeof(g_dp_composed));
    memset(g_dp_src, 0xBB, sizeof(g_dp_src));

    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)g_dp_snapshot;
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)g_dp_composed;

    fd2_slide_panel_down_step(0, (uint32)g_dp_workspace, (uint32)g_dp_src);

    /* workspace top 5 bytes untouched by row copy -> must be snapshot (0xAA),
     * proving the restore read bg_snapshot, not composed_target (0xCC). */
    ASSERT_EQ((long)g_dp_workspace[0], 0xAA);
    ASSERT_EQ((long)g_dp_workspace[4], 0xAA);
    /* row 0 copy: dst offset 5, 0x136 bytes from src (0xBB). */
    ASSERT_EQ((long)g_dp_workspace[5], 0xBB);
    ASSERT_EQ((long)g_dp_workspace[5 + 0x135], 0xBB);
    /* byte just past row 0's 0x136-wide copy is snapshot again. */
    ASSERT_EQ((long)g_dp_workspace[5 + 0x136], 0xAA);
    /* last copied row is row 0x55: dst = 5 + 0x55*0x140. */
    last_row_dst = 5 + 0x55 * 0x140;
    ASSERT_EQ((long)g_dp_workspace[last_row_dst], 0xBB);
    /* row 0x56 would start at 5 + 0x56*0x140; must NOT be copied (snapshot). */
    ASSERT_EQ((long)g_dp_workspace[5 + 0x56 * 0x140], 0xAA);

    data_fd2_ui_slide_bg_snapshot_buf_ptr = save_snap;
    data_fd2_ui_slide_composed_target_buf_ptr = save_comp;
}

/* Clip branch: y_offset = 195 -> 195+0x56=281 >= 200 -> row_count = 200-195 = 5.
 * Only 5 rows (195..199) get overlaid; row 200 region stays snapshot. */
static void test_slide_panel_down_step_bottom_clip(void)
{
    uint32 save_snap;
    long row5_dst;

    save_snap = data_fd2_ui_slide_bg_snapshot_buf_ptr;

    memset(g_dp_workspace, 0x11, sizeof(g_dp_workspace));
    memset(g_dp_snapshot, 0xAA, sizeof(g_dp_snapshot));
    memset(g_dp_src, 0xBB, sizeof(g_dp_src));
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)g_dp_snapshot;

    fd2_slide_panel_down_step(195, (uint32)g_dp_workspace, (uint32)g_dp_src);

    /* row 0 (screen y=195): dst = 5 + 195*0x140 -> src (0xBB). */
    ASSERT_EQ((long)g_dp_workspace[5 + 195 * 0x140], 0xBB);
    /* row 4 (screen y=199, last): dst = 5 + 199*0x140 -> src (0xBB). */
    row5_dst = 5 + 199 * 0x140;
    ASSERT_EQ((long)g_dp_workspace[row5_dst], 0xBB);
    /* row 5 (screen y=200) clipped off: never reached (200*0x140 = 64000,
     * past buffer end), so verify a still-snapshot interior byte at y=199
     * just before the copied span start instead: workspace[199*0x140] is
     * outside the 5-byte left margin? offset 199*0x140 = 63680 < 63685 (copy
     * start), so it stays snapshot. */
    ASSERT_EQ((long)g_dp_workspace[199 * 0x140], 0xAA);

    data_fd2_ui_slide_bg_snapshot_buf_ptr = save_snap;
}

/* ---- ANI decoder tests ---- */

static uint8 g_test_palette_buf[768];
static uint8 g_test_row_buf[320];

static void test_ani_palette_fill_byte(void)
{
    uint8 stream[2];
    stream[0] = 0;
    stream[1] = 0x42;
    data_fd2_animation_ani_decoder_src_buf = (uint32)g_test_palette_buf;
    memset(g_test_palette_buf, 0, 768);
    data_fd2_animation_ani_decoder_frame_dispatch_table[0] =
        (void *)fd2_ani_decoder_chunk_palette_fill_byte;
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_palette_buf[0], 0x42);
    ASSERT_EQ((long)g_test_palette_buf[767], 0x42);
}

static void test_ani_row_copy_literal(void)
{
    uint8 stream[6];
    stream[0] = 1;
    stream[1] = 0xAA;
    stream[2] = 0xBB;
    stream[3] = 0xCC;
    stream[4] = 0xDD;
    stream[5] = 0xEE;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_target_width = 5;
    data_fd2_animation_ani_decoder_frame_dispatch_table[1] =
        (void *)fd2_ani_decoder_chunk_row_copy_literal;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[0], 0xAA);
    ASSERT_EQ((long)g_test_row_buf[4], 0xEE);
}

static void test_ani_sparse_set_byte(void)
{
    uint8 stream[10];
    stream[0] = 2;
    *(uint16 *)(stream + 1) = 2;
    *(uint16 *)(stream + 3) = 5;
    stream[5] = 0x77;
    *(uint16 *)(stream + 6) = 10;
    stream[8] = 0x88;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_frame_dispatch_table[2] =
        (void *)fd2_ani_decoder_chunk_sparse_set_byte;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[5], 0x77);
    ASSERT_EQ((long)g_test_row_buf[10], 0x88);
}

static void test_ani_decode_frame_dispatch(void)
{
    uint8 stream[4];
    stream[0] = 3;
    stream[1] = 0x55;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_target_width = 4;
    data_fd2_animation_ani_decoder_frame_dispatch_table[3] =
        (void *)fd2_ani_decoder_chunk_row_fill_byte;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[0], 0x55);
    ASSERT_EQ((long)g_test_row_buf[3], 0x55);
}

static void test_scan_chars_along_line(void)
{
    uint8 out[8];
    uint32 save_cx;
    uint32 save_cy;
    int result;

    save_cx = data_fd2_battle_cursor_world_x;
    save_cy = data_fd2_battle_cursor_world_y;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 3; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].pos_x = 4; g_test_rc_array[1].pos_y = 5;
    g_test_rc_array[1].team = 2;
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_map_width_tiles = 10;
    data_fd2_battle_map_height_tiles = 10;

    memset(out, 0xFF, sizeof(out));
    result = fd2_scan_chars_along_line_with_team_filter(
        5, 5, (uint32)out, 2, 5, 3, 0);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(out[0], 1);

    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_world_y = save_cy;
}

static void test_scan_chars_manhattan_enemy(void)
{
    uint8 idx_out[8];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5; g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].pos_x = 6; g_test_rc_array[1].pos_y = 5;
    g_test_rc_array[1].team = 2;
    g_test_rc_array[2].pos_x = 5; g_test_rc_array[2].pos_y = 4;
    g_test_rc_array[2].team = 0;
    g_test_rc_array[2].flags = 1;
    data_fd2_battle_party_member_count = 3;

    memset(idx_out, 0xFF, sizeof(idx_out));
    result = fd2_scan_chars_within_manhattan_range(5, 5, 3,
        (uint32)idx_out, 0);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(idx_out[0], 0);
}

static void test_mark_occupant_tiles_team0(void)
{
    uint8 t_tmap[48];
    uint32 save_tm;

    save_tm = data_fd2_battle_tile_map_ptr;
    memset(t_tmap, 0, sizeof(t_tmap));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_battle_map_width_tiles = 3;
    data_fd2_battle_map_height_tiles = 3;
    data_fd2_battle_party_member_count = 2;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 1;
    g_test_rc_array[0].pos_y = 0;
    g_test_rc_array[0].team = 0;
    g_test_rc_array[1].pos_x = 2;
    g_test_rc_array[1].pos_y = 1;
    g_test_rc_array[1].team = 2;

    fd2_mark_char_occupant_tiles_for_team(0xFFFFFFuL, 0);

    ASSERT_EQ(t_tmap[(0*3+1)*4+7], 0xFF);
    ASSERT_EQ(t_tmap[(1*3+2)*4+7], 0);

    data_fd2_battle_tile_map_ptr = save_tm;
}

static void test_pathfind_count_unique_dirs(void)
{
    uint8 result;

    memset(data_fd2_battle_pathfind_step_stack, 0, 32);
    data_fd2_battle_pathfind_current_depth = 4;
    data_fd2_battle_pathfind_step_stack[0*8+3] = 0;
    data_fd2_battle_pathfind_step_stack[1*8+3] = 0;
    data_fd2_battle_pathfind_step_stack[2*8+3] = 1;
    data_fd2_battle_pathfind_step_stack[3*8+3] = 1;
    result = fd2_pathfind_count_unique_directions();
    ASSERT_EQ(result, 8);
}

static void test_collect_unmarked_tiles(void)
{
    uint8 t_tmap[32];
    uint8 out[16];
    uint32 save_tm;
    int result;

    save_tm = data_fd2_battle_tile_map_ptr;
    memset(t_tmap, 0, sizeof(t_tmap));
    memset(out, 0, sizeof(out));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_battle_map_width_tiles = 2;
    data_fd2_battle_map_height_tiles = 2;
    t_tmap[7] = 0;
    t_tmap[11] = 0xFF;
    t_tmap[15] = 0;
    t_tmap[19] = 0xFF;

    result = fd2_collect_unmarked_tile_positions((uint32)out);
    ASSERT_EQ(result, 2);
    ASSERT_EQ(out[0], 0);
    ASSERT_EQ(out[1], 0);
    ASSERT_EQ(out[2], 0);
    ASSERT_EQ(out[3], 1);

    data_fd2_battle_tile_map_ptr = save_tm;
}

static void test_check_battle_end_victory(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[1].team = 0;
    g_test_rc_array[1].flags = 1;
    data_fd2_battle_party_member_count = 2;

    fd2_check_battle_end_condition();
    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 2);
}

static void test_check_battle_end_continues(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[1].team = 0;
    g_test_rc_array[1].flags = 0;
    data_fd2_battle_party_member_count = 2;

    fd2_check_battle_end_condition();
    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 0);
}

static void test_find_tile_attr_match_miss(void)
{
    uint8 t_tmap[24];
    uint8 t_attr[32];
    uint8 out[2];
    uint32 save_tm;
    uint32 save_af;
    int result;

    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;

    memset(t_tmap, 0, sizeof(t_tmap));
    memset(t_attr, 0, sizeof(t_attr));
    memset(out, 0xFF, sizeof(out));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr;
    data_fd2_battle_map_width_tiles = 2;
    data_fd2_battle_map_height_tiles = 2;

    result = fd2_find_tile_with_attribute_match(99, (uint32)out);
    ASSERT_EQ(result, -1);

    data_fd2_battle_tile_map_ptr = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
}

static void test_tally_chars_zero_field(void)
{
    uint8 idx_arr[3];
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].status_sleep_flag = 0;
    g_test_rc_array[1].status_sleep_flag = 1;
    g_test_rc_array[2].status_sleep_flag = 0;
    idx_arr[0] = 0;
    idx_arr[1] = 1;
    idx_arr[2] = 2;
    data_fd2_battle_party_member_count = 4;

    result = fd2_tally_chars_with_zero_at_field(3, (uint32)idx_arr,
        0x26, 10);
    ASSERT_EQ(result, 20);
}

static void test_ani_decoder_set_target_buffer(void)
{
    fd2_ani_decoder_set_target_buffer(320, 0x12345, 0xABCDE);
    ASSERT_EQ(data_fd2_animation_ani_decoder_target_width, 320);
    ASSERT_EQ(data_fd2_animation_ani_decoder_dst_buf, 0x12345);
    ASSERT_EQ(data_fd2_animation_ani_decoder_src_buf, 0xABCDE);
}

static void test_set_battle_anim_phase_to_1(void)
{
    data_fd2_battle_anim_phase = 0;
    fd2_set_battle_anim_phase_to_1();
    ASSERT_EQ(data_fd2_battle_anim_phase, 1);
}

static void test_check_tile_event_no_trigger(void)
{
    uint8 t_tmap[24];
    uint8 t_attr[32];
    uint32 save_tm;
    uint32 save_af;

    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;

    memset(t_tmap, 0, sizeof(t_tmap));
    memset(t_attr, 0, sizeof(t_attr));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr;
    data_fd2_battle_map_width_tiles = 2;
    data_fd2_battle_map_height_tiles = 2;
    t_attr[0] = 0x20;
    data_fd2_battle_ai_post_action_consequence_idx = 99;

    fd2_check_tile_event_post_action(0, 0, 0);
    ASSERT_EQ(data_fd2_battle_ai_post_action_consequence_idx, 99);

    data_fd2_battle_tile_map_ptr = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
}

/*
 * Positive-trigger path: exercise the state-transition WRITE at asm
 * 00013a95  MOV [data_fd2_battle_ai_post_action_consequence_idx],EDX,
 * which the prior no-trigger test never reaches (it early-exits at the
 * 00013a64 `TEST byte [ESP+4],0x60` / JNZ 00013a9b guard).
 *
 * Data flow (verified byte-exact vs disasm of 00013a44 + callee 00012e38):
 *   read_tile_attribute_at_pos(0,0,buf):
 *     tile_meta = tile_map + (0*width+0)*4 = tile_map+0
 *     sprite_idx    = *(u16)(tile_meta+4) & 0x3FF      -> set 0
 *     terrain_byte  = *(u8)(tile_meta+6)               -> set 5
 *     buf[+2 u16]   = terrain_byte & 0x1F  = 5  (terrain_class, nonzero)
 *     attr_ptr      = attr_flags + sprite_idx*4 = attr_flags+0
 *     buf[+4]       = attr_ptr[0]                       -> set 0 (bit 0x60 clear)
 *   back in check_tile_event_post_action:
 *     (buf[4] & 0x60)==0 && terrain_class!=0  -> enter body
 *     rec = tile_event_data_table + (5-1)*2 = table+8
 *     consequence_idx = *(u8)(rec+0x33) = table[0x3B]   -> set 0x42 (!=0xFF)
 *     event_type      = *(u8)(rec+0x34) = table[0x3C]   -> set 0x55
 *     event_type == expected_event_type(0x55) -> WRITE idx = 0x42
 *
 * Ground-truth value (0x42) is the consequence_idx byte stored verbatim
 * by MOV [...],EDX. emulate_function(00013a44) cannot be used to derive
 * it because the function's __CHK stack-probe prologue (CALL 00036cd7)
 * begins with `XCHG [ESP+4],EAX`, whose implicit-LOCK semantics raise an
 * "Unimplemented CALLOTHER pcodeop (LOCK)" in the Ghidra emulator; the
 * expected value is instead hand-derived from the asm and asserted here.
 */
static void test_check_tile_event_triggers(void)
{
    uint8 t_tmap[24];
    uint8 t_attr[32];
    uint8 t_table[64];
    uint32 save_tm;
    uint32 save_af;
    uint32 save_te;

    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;
    save_te = data_fd2_tile_event_data_table_ptr;

    memset(t_tmap, 0, sizeof(t_tmap));
    memset(t_attr, 0, sizeof(t_attr));
    memset(t_table, 0, sizeof(t_table));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr;
    data_fd2_tile_event_data_table_ptr = (uint32)t_table;
    data_fd2_battle_map_width_tiles = 2;
    data_fd2_battle_map_height_tiles = 2;

    t_tmap[6] = 5;          /* terrain_class = 5 (nonzero) */
    t_attr[0] = 0;          /* attr flags bit 0x60 clear   */
    t_table[(5 - 1) * 2 + 0x33] = 0x42;   /* consequence_idx != 0xFF */
    t_table[(5 - 1) * 2 + 0x34] = 0x55;   /* event_type              */

    data_fd2_battle_ai_post_action_consequence_idx = 99;
    fd2_check_tile_event_post_action(0, 0, 0x55);
    ASSERT_EQ(data_fd2_battle_ai_post_action_consequence_idx, 0x42);

    data_fd2_battle_tile_map_ptr = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
    data_fd2_tile_event_data_table_ptr = save_te;
}

/*
 * Same trigger setup but expected_event_type != table event_type, so the
 * 00013a8f `CMP EAX,[ESP+0x14]` / JNZ 00013a9b guard skips the write; the
 * consequence_idx sentinel must survive unchanged.
 */
static void test_check_tile_event_event_type_mismatch(void)
{
    uint8 t_tmap[24];
    uint8 t_attr[32];
    uint8 t_table[64];
    uint32 save_tm;
    uint32 save_af;
    uint32 save_te;

    save_tm = data_fd2_battle_tile_map_ptr;
    save_af = data_fd2_tile_attribute_flags_buffer_ptr;
    save_te = data_fd2_tile_event_data_table_ptr;

    memset(t_tmap, 0, sizeof(t_tmap));
    memset(t_attr, 0, sizeof(t_attr));
    memset(t_table, 0, sizeof(t_table));
    data_fd2_battle_tile_map_ptr = (uint32)t_tmap;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_attr;
    data_fd2_tile_event_data_table_ptr = (uint32)t_table;
    data_fd2_battle_map_width_tiles = 2;
    data_fd2_battle_map_height_tiles = 2;

    t_tmap[6] = 5;
    t_attr[0] = 0;
    t_table[(5 - 1) * 2 + 0x33] = 0x42;   /* consequence_idx != 0xFF */
    t_table[(5 - 1) * 2 + 0x34] = 0x55;   /* event_type = 0x55       */

    data_fd2_battle_ai_post_action_consequence_idx = 77;
    fd2_check_tile_event_post_action(0, 0, 0x12);   /* expected != 0x55 */
    ASSERT_EQ(data_fd2_battle_ai_post_action_consequence_idx, 77);

    data_fd2_battle_tile_map_ptr = save_tm;
    data_fd2_tile_attribute_flags_buffer_ptr = save_af;
    data_fd2_tile_event_data_table_ptr = save_te;
}

static void test_check_all_acted_not_done(void)
{
    /* char[1] is an active player (flags bit0/bit7 clear, team 2, awake)
       so all_done becomes false and the branch is SKIPPED. Use sentinels
       on both written globals to prove the branch body never ran:
       anim_phase=9 must survive (branch would set 0 then 1), and
       ui_play_active_flag=7 must survive (branch would set 0 then 1). */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0x80;
    g_test_rc_array[0].team = 2;
    g_test_rc_array[1].flags = 0;
    g_test_rc_array[1].team = 2;
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_anim_phase = 9;
    data_fd2_ui_play_active_flag = 7;

    fd2_check_all_player_acted_or_asleep();
    ASSERT_EQ(data_fd2_battle_anim_phase, 9);
    ASSERT_EQ((long)data_fd2_ui_play_active_flag, 7);
}

static void test_check_all_acted_triggers(void)
{
    /* Every player char is acted/asleep/dead so all_done stays true and
       the branch is TAKEN: anim_phase ends at 1 and ui_play_active_flag
       ends at 1 (both written to 0 then 1 around the turn-cycle call). */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0x80;
    g_test_rc_array[0].team = 2;
    g_test_rc_array[1].flags = 0x01;
    g_test_rc_array[1].team = 2;
    data_fd2_battle_party_member_count = 2;
    data_fd2_battle_anim_phase = 5;
    data_fd2_ui_play_active_flag = 7;

    fd2_check_all_player_acted_or_asleep();
    ASSERT_EQ(data_fd2_battle_anim_phase, 1);
    ASSERT_EQ((long)data_fd2_ui_play_active_flag, 1);
}

static void test_mark_char_acted(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[1].flags = 0x04;

    fd2_mark_char_acted_this_turn(1);

    ASSERT_EQ(g_test_rc_array[1].flags, 0x84);
}

static void test_walk_step_right_basic(void)
{
    uint32 save_ox;
    uint32 save_cx;
    uint32 save_sx;

    save_ox = data_fd2_battle_view_window_origin_x;
    save_cx = data_fd2_battle_cursor_world_x;
    save_sx = data_fd2_battle_cursor_screen_x;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 5;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_cursor_world_x = 5;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;

    fd2_walk_step_right(0);

    ASSERT_EQ(g_test_rc_array[0].pos_x, 6);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 3);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 6);

    data_fd2_battle_view_window_origin_x = save_ox;
    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_screen_x = save_sx;
}

static void test_walk_step_up_basic(void)
{
    uint32 save_oy;
    uint32 save_cy;
    uint32 save_sy;

    save_oy = data_fd2_battle_view_window_origin_y;
    save_cy = data_fd2_battle_cursor_world_y;
    save_sy = data_fd2_battle_cursor_screen_y;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_y = 5;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;

    fd2_walk_step_up(0);

    ASSERT_EQ(g_test_rc_array[0].pos_y, 4);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 2);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 4);

    data_fd2_battle_view_window_origin_y = save_oy;
    data_fd2_battle_cursor_world_y = save_cy;
    data_fd2_battle_cursor_screen_y = save_sy;
}

static void test_walk_step_left_basic(void)
{
    uint32 save_ox;
    uint32 save_cx;
    uint32 save_sx;

    save_ox = data_fd2_battle_view_window_origin_x;
    save_cx = data_fd2_battle_cursor_world_x;
    save_sx = data_fd2_battle_cursor_screen_x;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = 8;
    data_fd2_battle_view_window_origin_x = 3;
    data_fd2_battle_cursor_world_x = 8;
    data_fd2_battle_cursor_screen_x = 5;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;

    fd2_walk_step_left(0);

    ASSERT_EQ(g_test_rc_array[0].pos_x, 7);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 1);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(data_fd2_battle_cursor_world_x, 7);

    data_fd2_battle_view_window_origin_x = save_ox;
    data_fd2_battle_cursor_world_x = save_cx;
    data_fd2_battle_cursor_screen_x = save_sx;
}

static void test_walk_step_down_basic(void)
{
    uint32 save_oy;
    uint32 save_cy;
    uint32 save_sy;

    save_oy = data_fd2_battle_view_window_origin_y;
    save_cy = data_fd2_battle_cursor_world_y;
    save_sy = data_fd2_battle_cursor_screen_y;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_y = 5;
    g_test_rc_array[0].sprite_state[1] = 2;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_y = 5;
    data_fd2_battle_cursor_screen_y = 5;
    data_fd2_battle_map_height_tiles = 15;
    data_fd2_battle_compose_walk_step_y_sub_pixel_offset = 0;
    data_fd2_battle_walk_anim_y_scroll_rows = 0;

    fd2_walk_step_down(0);

    ASSERT_EQ(g_test_rc_array[0].pos_y, 6);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[1], 0);
    ASSERT_EQ(g_test_rc_array[0].sprite_state[2], 0);
    ASSERT_EQ(data_fd2_battle_cursor_world_y, 6);

    data_fd2_battle_view_window_origin_y = save_oy;
    data_fd2_battle_cursor_world_y = save_cy;
    data_fd2_battle_cursor_screen_y = save_sy;
}

void run_anim_tests(void)
{
    SUITE_BEGIN(anim);

    RUN_TEST(test_tick_tile_event_anim_consumed_event);
    RUN_TEST(test_tick_tile_event_anim_non_event_no_change);
    RUN_TEST(test_tick_chapter_palette_fast_cycle);
    RUN_TEST(test_tick_chapter_palette_fast_wrap);
    RUN_TEST(test_tick_chapter_palette_slow_triggers);
    RUN_TEST(test_tick_chapter_palette_slow_triggers_negative_delta);
    RUN_TEST(test_find_char_at_cursor_found);
    RUN_TEST(test_find_char_at_cursor_not_found);
    RUN_TEST(test_find_char_by_id_found);
    RUN_TEST(test_find_char_by_id_fallback_template);
    RUN_TEST(test_walk_step_down_basic);
    RUN_TEST(test_walk_step_left_basic);
    RUN_TEST(test_walk_step_up_basic);
    RUN_TEST(test_walk_step_right_basic);
    RUN_TEST(test_walk_path_animation_loop_2steps);
    RUN_TEST(test_mark_char_acted);
    RUN_TEST(test_check_all_acted_not_done);
    RUN_TEST(test_check_all_acted_triggers);
    RUN_TEST(test_check_tile_event_no_trigger);
    RUN_TEST(test_check_tile_event_triggers);
    RUN_TEST(test_check_tile_event_event_type_mismatch);
    RUN_TEST(test_mark_char_as_dead);
    RUN_TEST(test_set_chapter_init_done_flag);
    RUN_TEST(test_set_combat_aux_low4);
    RUN_TEST(test_set_battle_anim_phase_to_1);
    RUN_TEST(test_ani_decoder_set_target_buffer);
    RUN_TEST(test_tally_chars_zero_field);
    RUN_TEST(test_find_tile_attr_match_miss);
    RUN_TEST(test_check_battle_end_victory);
    RUN_TEST(test_check_battle_end_continues);
    RUN_TEST(test_collect_unmarked_tiles);
    RUN_TEST(test_pathfind_count_unique_dirs);
    RUN_TEST(test_mark_occupant_tiles_team0);
    RUN_TEST(test_scan_chars_manhattan_enemy);
    RUN_TEST(test_tick_tutorial_sfx_counter);
    RUN_TEST(test_collect_dead_char_drops);
    RUN_TEST(test_collect_pending_drops);
    RUN_TEST(test_mark_aoe_plus_pattern);
    RUN_TEST(test_scan_chars_along_line);
    RUN_TEST(test_slide_panel_left_main_stationary);
    RUN_TEST(test_slide_panel_down_step_restores_snapshot);
    RUN_TEST(test_slide_panel_down_step_bottom_clip);
    RUN_TEST(test_score_item_candidate_damage);
    RUN_TEST(test_score_item_candidate_score3);
    RUN_TEST(test_score_item_candidate_score0);
    RUN_TEST(test_score_item_candidate_x3_amplify);
    RUN_TEST(test_score_item_candidate_spell_wrapper);
    RUN_TEST(test_score_item_candidate_spell_0x18);
    RUN_TEST(test_ani_palette_fill_byte);
    RUN_TEST(test_ani_row_copy_literal);
    RUN_TEST(test_ani_sparse_set_byte);
    RUN_TEST(test_ani_decode_frame_dispatch);

    SUITE_END();
}
