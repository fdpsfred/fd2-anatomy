/*
 * unit tests for src/battle/btl_turn.c
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
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;
extern uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
extern int g_ending_menu_return;
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


static uint8 t_tmpl_roster[4 * 0x50];


/* Minimal dialog-text fixture for the status-tick display path.
 *
 * fd2_tick_status_effects_and_show_messages drives the real dialog VM
 * (fd2_display_dialog_scene) with page indices 0x1E1..0x1E7 whenever a status
 * message is shown. The VM does cur_op = base + (int16)base[page_idx], then
 * reads opcodes until it hits -1 (END). We give every referenced page slot an
 * offset pointing at a single END word, so the scene returns immediately with
 * no glyphs rendered (and thus no per-glyph blink/BIOS-tick pacing). Without a
 * real text pointer the VM would otherwise walk arbitrary low memory. */
#define T_DLG_PAGES   0x1E8                 /* covers indices 0..0x1E7 */
#define T_DLG_ENDWORD T_DLG_PAGES           /* word index of the END (-1) */
static int16 t_dlg_text[T_DLG_PAGES + 1];

static void t_install_dialog_text(void)
{
    int p;
    for (p = 0; p < T_DLG_PAGES; p++) {
        t_dlg_text[p] = (int16)(T_DLG_ENDWORD * 2);   /* byte offset to END */
    }
    t_dlg_text[T_DLG_ENDWORD] = -1;                   /* END opcode */
    data_fd2_all_game_text_ptr = (uint32)t_dlg_text;
}


/* Safe battle-scene fixture for status-tick tests.
 *
 * fd2_tick_status_effects_and_show_messages pans the cursor / closes the status
 * screen, and those real callees recomposite the battle frame, whose real
 * fd2_composite_all_chars_overlay -> fd2_paint_chars_shadow_overlay loops over
 * the live party calling the real fd2_blit_animated_tile_at_pos. That blitter is
 * real and (for cursor-overlay tiles) reaches the real
 * fd2_tile_blit_24x24_with_remap_table, which would read whatever RLE bytes the
 * sprite source points at. Without a deterministic tile map the leftover globals
 * from prior tests can make a tile resolve to a renderable+overlay blit over
 * garbage, hanging the real decoder.
 *
 * Point the tile-map at the middle of a large zeroed buffer (so even the
 * shadow overlay's negative-row probes read a 0 tile id) and give every tile a
 * zeroed attribute entry: attr bit 0x80 (renderable) is clear, so
 * fd2_blit_animated_tile_at_pos skips the blit entirely before computing a
 * sprite source or calling any blitter. This keeps the status-tick logic under
 * test while making the incidental repaint a deterministic no-op. */
#define T_SCENE_W        0x20
static uint8 t_scene_tilemap[0x4000];   /* (tile_y*W + tile_x)*4, mid-anchored */
static uint8 t_scene_attr[0x1000];      /* tile_id (<=0x3FF) * 4 -> <= 0xFFC    */
static uint8 t_scene_ws[0x2000];        /* dst workspace anchor (never written) */

static void t_install_safe_battle_scene(void)
{
    memset(t_scene_tilemap, 0, sizeof(t_scene_tilemap));
    memset(t_scene_attr, 0, sizeof(t_scene_attr));
    data_fd2_battle_tile_map_ptr =
        (uint32)(t_scene_tilemap + sizeof(t_scene_tilemap) / 2);
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_scene_attr;
    data_fd2_battle_map_width_tiles = T_SCENE_W;
    data_fd2_large_game_state_buffer_ptr = (uint32)t_scene_ws;
    data_fd2_graphics_bg_anim_flip_flag = 0;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
}


/* ---- Test: fd2_tick_status_effects_and_show_messages ---- */

static void test_status_tick_poison_damage(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    t_install_dialog_text();
    t_install_safe_battle_scene();
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
    ((uint8 *)&g_test_rc_array[0])[0x25] = 1;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 100;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 200;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)*(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40), 80);
    data_fd2_battle_party_member_count = 4;
}


static void test_status_tick_poison_clamp_zero(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    t_install_dialog_text();
    t_install_safe_battle_scene();
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
    ((uint8 *)&g_test_rc_array[0])[0x25] = 1;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 5;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 200;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)*(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40), 0);
    data_fd2_battle_party_member_count = 4;
}


static void test_status_tick_poison_skip_dead(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    t_install_dialog_text();
    t_install_safe_battle_scene();
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 1;
    ((uint8 *)&g_test_rc_array[0])[0x25] = 1;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40) = 100;
    *(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x42) = 200;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)*(uint16 *)((uint8 *)&g_test_rc_array[0] + 0x40), 100);
    data_fd2_battle_party_member_count = 4;
}


static void test_status_tick_timer_decrement(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    t_install_dialog_text();
    t_install_safe_battle_scene();
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
    /* alive char: hp>0 keeps the now-real fd2_play_death_animation_and_mark_dead
     * (run between the two passes) from marking this hp==0 char dead, which
     * would otherwise gate Pass 2's timer countdown (it skips dead chars). */
    g_test_rc_array[0].hp_current = 100;
    ((uint8 *)&g_test_rc_array[0])[0x22] = 3;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)((uint8 *)&g_test_rc_array[0])[0x22], 2);
    data_fd2_battle_party_member_count = 4;
}


static void test_status_tick_timer_expires_recalc(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    t_install_dialog_text();
    t_install_safe_battle_scene();
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
    /* alive char (hp>0): see test_status_tick_timer_decrement — keeps the real
     * inter-pass death pass from marking this char dead and gating Pass 2. */
    g_test_rc_array[0].hp_current = 100;
    ((uint8 *)&g_test_rc_array[0])[0x22] = 1;
    data_fd2_battle_party_member_count = 1;
    fd2_tick_status_effects_and_show_messages(0);
    ASSERT_EQ((long)((uint8 *)&g_test_rc_array[0])[0x22], 0);
    data_fd2_battle_party_member_count = 4;
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


static void test_mark_char_as_dead(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[2].flags = 0x84;
    fd2_mark_char_as_dead(2);
    ASSERT_EQ(g_test_rc_array[2].flags, 0x01);
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


/* ---- Test: fd2_kill_runtime_chars_from_index_to_end @ 0x35BBA ----
 *
 * Zeroes hp_current for slots [start_char_idx, party_member_count) then
 * runs the (now real) fd2_play_death_animation_and_mark_dead tail call.
 *
 * To keep the death tail a silent no-render no-op, every killed char is
 * placed OFF-screen (pos_x past the view window): the real death function
 * then collects 0 on-screen dying chars and takes its silent branch, which
 * sets flags |= 1 (CHARFLAG_DEAD) on every hp_current==0 char and returns
 * without touching the VGA buffer. Asserting that silent mark also confirms
 * the tail call actually ran. */

static void t_kill_setup_offscreen_chars(int n, uint16 hp)
{
    int k;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* default view window so pos_x=100 is off-screen for every slot */
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_view_window_max_x = 13;
    data_fd2_battle_view_window_max_y = 8;
    for (k = 0; k < n; k++) {
        g_test_rc_array[k].pos_x = 100;   /* > origin_x + max_x => off-screen */
        g_test_rc_array[k].pos_y = 0;
        g_test_rc_array[k].flags = 0;
        g_test_rc_array[k].hp_current = hp;
    }
}

static void test_kill_from_index_zeros_tail_range(void)
{
    /* start_char_idx = 2 of 4 -> slots 2,3 killed, slots 0,1 preserved */
    t_kill_setup_offscreen_chars(4, 250);
    data_fd2_battle_party_member_count = 4;
    fd2_kill_runtime_chars_from_index_to_end(2);
    ASSERT_EQ((long)g_test_rc_array[0].hp_current, 250);
    ASSERT_EQ((long)g_test_rc_array[1].hp_current, 250);
    ASSERT_EQ((long)g_test_rc_array[2].hp_current, 0);
    ASSERT_EQ((long)g_test_rc_array[3].hp_current, 0);
    /* untouched (still alive) slots keep flags clear */
    ASSERT_EQ(g_test_rc_array[0].flags, 0);
    ASSERT_EQ(g_test_rc_array[1].flags, 0);
    /* killed slots: silent death path marked them dead (flags |= 1) */
    ASSERT_EQ(g_test_rc_array[2].flags, 1);
    ASSERT_EQ(g_test_rc_array[3].flags, 1);
}

static void test_kill_from_index_zero_kills_all(void)
{
    /* start_char_idx = 0 -> whole party killed and marked dead */
    t_kill_setup_offscreen_chars(3, 99);
    data_fd2_battle_party_member_count = 3;
    fd2_kill_runtime_chars_from_index_to_end(0);
    ASSERT_EQ((long)g_test_rc_array[0].hp_current, 0);
    ASSERT_EQ((long)g_test_rc_array[1].hp_current, 0);
    ASSERT_EQ((long)g_test_rc_array[2].hp_current, 0);
    ASSERT_EQ(g_test_rc_array[0].flags, 1);
    ASSERT_EQ(g_test_rc_array[1].flags, 1);
    ASSERT_EQ(g_test_rc_array[2].flags, 1);
    data_fd2_battle_party_member_count = 4;
}

static void test_kill_from_index_empty_range_noop(void)
{
    /* start_char_idx == party_member_count -> loop body never runs;
     * no char has hp_current==0, so the death tail marks nothing dead. */
    t_kill_setup_offscreen_chars(3, 77);
    data_fd2_battle_party_member_count = 3;
    fd2_kill_runtime_chars_from_index_to_end(3);
    ASSERT_EQ((long)g_test_rc_array[0].hp_current, 77);
    ASSERT_EQ((long)g_test_rc_array[1].hp_current, 77);
    ASSERT_EQ((long)g_test_rc_array[2].hp_current, 77);
    ASSERT_EQ(g_test_rc_array[0].flags, 0);
    ASSERT_EQ(g_test_rc_array[1].flags, 0);
    ASSERT_EQ(g_test_rc_array[2].flags, 0);
    data_fd2_battle_party_member_count = 4;
}


/* fd2_collect_pending_death_drops @ 0x1B6B7 — 3-condition AND filter
 * (flags&CHARFLAG_DEAD==0, combat_aux_block[10]!=0xFF, hp_current==0)
 * packing each kept 3-byte entry (combat_aux_block[10..12]) at
 * out + drop_count*3 (asm 0x1b70d MOV EAX,ESI; SHL 2; SUB ESI -> ESI*3).
 * Expectations derived statically from disasm; emulate blocked by __CHK LOCK. */
static void test_collect_pending_drops(void)
{
    uint8 out[12];
    int result;

    /* Happy path: single qualifying char -> 1 entry, type byte at out[0]. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[0].combat_aux_block[10] = 1;
    g_test_rc_array[0].hp_current = 0;
    data_fd2_battle_party_member_count = 1;

    memset(out, 0, sizeof(out));
    result = fd2_collect_pending_death_drops((uint32)out);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(out[0], 1);

    /* (a) Two qualifying chars at idx0/idx1 with distinct 3-byte entries.
     * Pins the *3 packing stride (entry1 must land at out[3], not out[1]/out[2])
     * and confirms the full 3-byte memmove of combat_aux_block[10..12]. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[0].hp_current = 0;
    g_test_rc_array[0].combat_aux_block[10] = 1;   /* type */
    g_test_rc_array[0].combat_aux_block[11] = 0x11;
    g_test_rc_array[0].combat_aux_block[12] = 0x22;
    g_test_rc_array[1].flags = 0;
    g_test_rc_array[1].hp_current = 0;
    g_test_rc_array[1].combat_aux_block[10] = 2;   /* type */
    g_test_rc_array[1].combat_aux_block[11] = 0x33;
    g_test_rc_array[1].combat_aux_block[12] = 0x44;
    data_fd2_battle_party_member_count = 2;

    memset(out, 0, sizeof(out));
    result = fd2_collect_pending_death_drops((uint32)out);
    ASSERT_EQ(result, 2);
    ASSERT_EQ(out[0], 1);
    ASSERT_EQ(out[1], 0x11);
    ASSERT_EQ(out[2], 0x22);
    ASSERT_EQ(out[3], 2);
    ASSERT_EQ(out[4], 0x33);
    ASSERT_EQ(out[5], 0x44);

    /* (b) combat_aux_block[10]==0xFF empty-entry skip. This is the sole
     * semantic distinguishing this fn from fd2_collect_dead_char_drops
     * (which keeps ==3). char0 empty + char1 qualifying -> only char1 kept,
     * compacted to out[0]. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[0].hp_current = 0;
    g_test_rc_array[0].combat_aux_block[10] = 0xFF;  /* empty -> skip */
    g_test_rc_array[1].flags = 0;
    g_test_rc_array[1].hp_current = 0;
    g_test_rc_array[1].combat_aux_block[10] = 7;
    data_fd2_battle_party_member_count = 2;

    memset(out, 0, sizeof(out));
    result = fd2_collect_pending_death_drops((uint32)out);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(out[0], 7);

    /* (c) flags & CHARFLAG_DEAD dead-skip (others passing). */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = CHARFLAG_DEAD;        /* dead -> skip */
    g_test_rc_array[0].hp_current = 0;
    g_test_rc_array[0].combat_aux_block[10] = 5;
    data_fd2_battle_party_member_count = 1;

    memset(out, 0xEE, sizeof(out));
    result = fd2_collect_pending_death_drops((uint32)out);
    ASSERT_EQ(result, 0);

    /* (d) hp_current != 0 alive-skip (others passing). asm uses MOVZX word
     * + TEST + JG, i.e. keep only when HP<=0 (HP is u16 -> ==0). */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].flags = 0;
    g_test_rc_array[0].hp_current = 1;               /* alive -> skip */
    g_test_rc_array[0].combat_aux_block[10] = 5;
    data_fd2_battle_party_member_count = 1;

    memset(out, 0xEE, sizeof(out));
    result = fd2_collect_pending_death_drops((uint32)out);
    ASSERT_EQ(result, 0);
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


/* flag=1 game-over path: protagonist (char[0]) dead. char[1] is a dead
 * enemy so the loop's "alive enemy -> flag=0" does not fire; the final
 * "if char[0] dead -> flag=1" sets the lose code. */
static void test_check_battle_end_gameover(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    g_test_rc_array[0].flags = 1;   /* protagonist dead */
    g_test_rc_array[1].team = 0;
    g_test_rc_array[1].flags = 1;   /* enemy dead (no flag=0 reset) */
    data_fd2_battle_party_member_count = 2;

    fd2_check_battle_end_condition();
    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
}


/* flag=1 precedence: even with an alive enemy (loop writes flag=0), a dead
 * protagonist makes the trailing check override the result back to 1. This
 * pins the loop-then-final ordering proven by the assembly. */
static void test_check_battle_end_gameover_overrides_continue(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].team = 2;
    g_test_rc_array[0].flags = 1;   /* protagonist dead */
    g_test_rc_array[1].team = 0;
    g_test_rc_array[1].flags = 0;   /* enemy alive -> loop sets flag=0 */
    data_fd2_battle_party_member_count = 2;

    fd2_check_battle_end_condition();
    ASSERT_EQ(data_fd2_chapter_event_or_battle_end_code, 1);
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


/* ---- fd2_run_full_turn_cycle ---- */

/* fd2_animate_phase_banner_slide_in / _out and fd2_render_phase_banner_frame are
 * now all emitted for real (anicombt.c / rndscene.c), so the full cycle runs the
 * real banners. Both banner animators are the ONLY callers of the recording
 * fd2_scroll_buffer_block_with_wrap stub (slide_in scrolls 16x, slide_out 17x),
 * so g_scroll_buffer_calls cleanly counts banner-animator activity end-to-end,
 * isolated from the Phase-F reveal loops (which never scroll). */
extern int g_scroll_buffer_calls;
extern int g_restore_block_calls;
extern uint8 data_fd2_audio_bgm_driver_available_flag;

/* Spy handlers + matching tile-event table for driving the REAL
 * fd2_fire_chapter_turn_events_for_phase through fd2_run_full_turn_cycle.
 * Each spy is installed into data_fd2_battle_ai_post_action_consequence_table
 * at a distinct event_id and records that it fired (the dispatcher always
 * passes arg 0, so phase distinction is encoded by which table entry's
 * phase byte matched). */
static int g_turncycle_spy_b_fired;   /* phase 1 (end-of-player-turn)   */
static int g_turncycle_spy_d_fired;   /* phase 0 (enemy-turn start)     */
static int g_turncycle_spy_f_fired;   /* phase 2 (new-player-turn)      */
static void turncycle_spy_b(uint32 a) { (void)a; g_turncycle_spy_b_fired++; }
static void turncycle_spy_d(uint32 a) { (void)a; g_turncycle_spy_d_fired++; }
static void turncycle_spy_f(uint32 a) { (void)a; g_turncycle_spy_f_fired++; }

/* 16-entry chapter turn-event table backing store (entries start at
 * byte +3, 3-byte stride: turn @+0, event_id @+1, phase @+2 within entry). */
static uint8 t_turn_event_table[64];

/* large_game_state_buffer surface read by the real fd2_blit_rectangle
 * (src = base + 0x8088, 0xC0 rows of 0x138 bytes at stride 0x1C8). Sized
 * to cover the highest read offset (0x8088 + 0xBF*0x1C8 + 0x138). */
static uint8 t_state_buf[0x20000];

/* Synthetic sprite atlas for the Phase-F reveal loops. The real
 * fd2_alloc_and_blit_indexed_sprite_chunk and fd2_render_decimal_number
 * resolve sprite_addr = sheet + *(int32 *)(sheet + 6 + idx*4); every
 * offset-table entry points at a {int16 0, int16 0} header so width and
 * height are 0 (malloc(8), zero-size blits via the faked leaf blitters).
 * Covers every index the reveal loops touch (0x2A..0x5D). */
#define T_SHEET_HDR_OFF 0x400
static uint8 t_sprite_sheet[0x600];

static void t_install_sprite_sheet(void)
{
    int idx;

    memset(t_sprite_sheet, 0, sizeof(t_sprite_sheet));
    /* offset table at +6, 4 bytes per entry; all -> the zero header. */
    for (idx = 0; idx < 0x60; idx++) {
        *(int32 *)(t_sprite_sheet + 6 + idx * 4) =
            (int32)T_SHEET_HDR_OFF;
    }
    /* header at +0x400 is already {0,0} from the memset. */
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)t_sprite_sheet;
}

/* Terminating sprite atlases for the REAL battle-frame compositor that the full
 * turn cycle drives (fd2_composite_battle_frame -> real cursor overlay / per-char
 * paint / shadow overlay, plus the direct fd2_paint_char_sprite_at_world_with_mode
 * calls). Those paths feed a sprite stream to the real RLE blitters
 * (fd2_tile_blit_24x24_passthrough and siblings); an unset/garbage atlas would
 * point them at a stream whose run-length decode never lands x_remain on 0,
 * looping forever.
 *
 * The cursor overlay resolves its sprite through a +6 offset table
 * (runtime_battle_state_ptr), the per-char painter through a +0 table
 * (portrait_sprite_cache); both are pointed at one shared, fully transparent
 * 24-row "SKIP 24 x 24" sprite that decodes cleanly and paints nothing. The
 * animated-tile / shadow overlay (which would resolve a +10 snapshot table) is
 * disarmed instead by zeroing the tile-map and tile-attribute buffers, so its
 * per-tile renderable-bit gate is clear and it never reads the snapshot. */
static uint8 t_cursor_atlas[0x200];     /* +6 offset table -> all-SKIP sprite   */
static uint8 t_portrait_atlas[0x600];   /* +0 offset table -> all-SKIP sprite   */
static uint8 t_blit_tile_map[0x400];    /* zeroed: tiles resolve non-renderable */
static uint8 t_blit_tile_attr[0x400];   /* zeroed: renderable bit 0x80 clear    */

static void t_fill_skip_sprite(uint8 *p)
{
    int i;
    for (i = 0; i < 24; i++) {
        p[i] = (uint8)(0xC0u | 23u);    /* one "SKIP 24" command per row */
    }
}

static void t_install_blit_atlases(void)
{
    int i;

    /* cursor atlas: 4-byte offset table at +6, entry idx -> the all-SKIP sprite
     * placed past the table; covers every cursor sprite index (0x00..0x12). */
    memset(t_cursor_atlas, 0, sizeof(t_cursor_atlas));
    for (i = 0; i < 0x40; i++) {
        *(int32 *)(t_cursor_atlas + 6 + i * 4) = (int32)0x100;
    }
    t_fill_skip_sprite(t_cursor_atlas + 0x100);
    data_fd2_runtime_battle_state_ptr = (uint32)t_cursor_atlas;

    /* portrait cache: 4-byte offset table at +0, entry idx -> the all-SKIP
     * sprite; covers facing*3 + cache_idx*0xC + frame for the test's char. */
    memset(t_portrait_atlas, 0, sizeof(t_portrait_atlas));
    for (i = 0; i < 0x80; i++) {
        *(int32 *)(t_portrait_atlas + i * 4) = (int32)0x400;
    }
    t_fill_skip_sprite(t_portrait_atlas + 0x400);
    portrait_sprite_cache = (uint32)t_portrait_atlas;

    /* zeroed tile-map + attr so the shadow/animated-tile overlay's renderable
     * gate is clear -> it never reaches the snapshot lookup or a blit. */
    memset(t_blit_tile_map, 0, sizeof(t_blit_tile_map));
    memset(t_blit_tile_attr, 0, sizeof(t_blit_tile_attr));
    data_fd2_battle_tile_map_ptr = (uint32)t_blit_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)t_blit_tile_attr;

    /* the real terrain-info HUD panel (also reached via fd2_composite_battle_frame)
     * would blit a terrain icon through the snapshot table; keep its enable gate
     * OFF so it early-returns and never reads the snapshot. */
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;
}


/* Phase A heal arithmetic + early-exit. game_event_flag is pre-set
 * nonzero so the cycle runs Phase A (heal) + Phase B (status tick),
 * then the first inter-phase gate returns before any NPC/enemy/new-turn
 * display. Pins the hp_max/5 heal, the clamp, the 5-condition qualifier,
 * the acted-flag mark, and the Phase-B-then-gate ordering. */
static void test_run_turn_cycle_phase_a_heal(void)
{
    uint32 save_lgs;
    uint32 save_te;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    t_install_dialog_text();
    save_lgs = data_fd2_large_game_state_buffer_ptr;
    data_fd2_large_game_state_buffer_ptr = (uint32)t_state_buf;

    /* Drive the REAL fire_chapter dispatcher: one entry matches the
     * current turn at phase 1 (Phase B) and one at phase 0 (Phase D).
     * The gate (game_event_flag=9) returns after Phase B, so only the
     * phase-1 spy must fire; the phase-0 spy proves Phase D never ran. */
    save_te = data_fd2_tile_event_data_table_ptr;
    memset(t_turn_event_table, 0, sizeof(t_turn_event_table));
    data_fd2_tile_event_data_table_ptr = (uint32)t_turn_event_table;
    data_fd2_battle_turn_counter = 3;
    /* entry[0] @ +3: turn=3, event_id=0x10, phase=1 */
    t_turn_event_table[3 + 0 * 3] = 3;
    t_turn_event_table[4 + 0 * 3] = 0x10;
    t_turn_event_table[5 + 0 * 3] = 1;
    /* entry[1] @ +6: turn=3, event_id=0x11, phase=0 */
    t_turn_event_table[3 + 1 * 3] = 3;
    t_turn_event_table[4 + 1 * 3] = 0x11;
    t_turn_event_table[5 + 1 * 3] = 0;
    data_fd2_battle_ai_post_action_consequence_table[0x10] = turncycle_spy_b;
    data_fd2_battle_ai_post_action_consequence_table[0x11] = turncycle_spy_d;
    g_turncycle_spy_b_fired = 0;
    g_turncycle_spy_d_fired = 0;

    /* [0] normal heal: 100/200 -> +40 = 140. */
    g_test_rc_array[0].team = 2;
    g_test_rc_array[0].hp_current = 100;
    g_test_rc_array[0].hp_max = 200;
    /* [1] clamp: 199/200 -> +40 = 239 -> clamped to 200. */
    g_test_rc_array[1].team = 2;
    g_test_rc_array[1].hp_current = 199;
    g_test_rc_array[1].hp_max = 200;
    /* [2] full HP (hp_current == hp_max) -> skipped. */
    g_test_rc_array[2].team = 2;
    g_test_rc_array[2].hp_current = 50;
    g_test_rc_array[2].hp_max = 50;
    /* [3] non-player team -> skipped. */
    g_test_rc_array[3].team = 0;
    g_test_rc_array[3].hp_current = 10;
    g_test_rc_array[3].hp_max = 100;
    /* [4] sleeping (status_sleep_flag) -> skipped. */
    g_test_rc_array[4].team = 2;
    g_test_rc_array[4].hp_current = 10;
    g_test_rc_array[4].hp_max = 100;
    g_test_rc_array[4].status_sleep_flag = 1;
    /* [5] poisoned (status_flags_block[4]) -> skipped. */
    g_test_rc_array[5].team = 2;
    g_test_rc_array[5].hp_current = 10;
    g_test_rc_array[5].hp_max = 100;
    g_test_rc_array[5].status_flags_block[4] = 1;
    /* [6] dead/acted (flags & 0x81) -> skipped. */
    g_test_rc_array[6].team = 2;
    g_test_rc_array[6].hp_current = 10;
    g_test_rc_array[6].hp_max = 100;
    g_test_rc_array[6].flags = 0x80;
    data_fd2_battle_party_member_count = 7;

    data_fd2_chapter_event_or_battle_end_code = 9;  /* gate -> early exit */
    g_scroll_buffer_calls = 0;

    fd2_run_full_turn_cycle();

    /* heal arithmetic */
    ASSERT_EQ((long)g_test_rc_array[0].hp_current, 140);
    ASSERT_EQ((long)g_test_rc_array[1].hp_current, 200);   /* clamped */
    ASSERT_EQ((long)g_test_rc_array[2].hp_current, 50);    /* skipped */
    ASSERT_EQ((long)g_test_rc_array[3].hp_current, 10);    /* skipped */
    ASSERT_EQ((long)g_test_rc_array[4].hp_current, 10);    /* skipped */
    ASSERT_EQ((long)g_test_rc_array[5].hp_current, 10);    /* skipped */
    ASSERT_EQ((long)g_test_rc_array[6].hp_current, 10);    /* skipped */
    /* healed chars get the acted flag (0x80); skipped chars do not. */
    ASSERT_EQ(g_test_rc_array[0].flags, 0x80);
    ASSERT_EQ(g_test_rc_array[1].flags, 0x80);
    ASSERT_EQ(g_test_rc_array[2].flags, 0x00);
    /* Phase B dispatched the phase-1 chapter event (real dispatcher routed
     * the turn=3/phase=1 entry to its handler); then the gate returned, so
     * the phase-0 (Phase D) entry never fired and no banner animated. */
    ASSERT_EQ(g_turncycle_spy_b_fired, 1);
    ASSERT_EQ(g_turncycle_spy_d_fired, 0);
    ASSERT_EQ(g_scroll_buffer_calls, 0);   /* no banner -> no scroll */

    data_fd2_battle_ai_post_action_consequence_table[0x10] = 0;
    data_fd2_battle_ai_post_action_consequence_table[0x11] = 0;
    data_fd2_tile_event_data_table_ptr = save_te;
    data_fd2_large_game_state_buffer_ptr = save_lgs;
    data_fd2_battle_party_member_count = 4;
}


/* Full cycle (game_event_flag stays 0): exercises Phases B-F including
 * both Phase-F reveal loops. Drives the REAL malloc/free pairing of
 * fd2_alloc_and_blit_indexed_sprite_chunk -> fd2_cleanup_dialog_sprite_buffer
 * (a wrong cleanup arg would free a non-heap pointer and crash), and the
 * 4-step loop's step=2,3,4,then-9 control flow. Confirms the turn counter
 * bumps once, both banners animate (in/out x2), and phase-0/2 events fire. */
static void test_run_turn_cycle_full_reveal(void)
{
    uint32 save_lgs;
    uint32 save_te;
    uint32 save_rbs;
    uint32 save_pcache;
    uint32 save_map;
    uint32 save_attr;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    t_install_dialog_text();
    t_install_sprite_sheet();
    /* this is the only turn-cycle test that runs the full reveal, so it is the
     * only one that reaches the real fd2_composite_battle_frame; give the real
     * compositor terminating sprite atlases (and a non-renderable tile-map) so its
     * RLE blitters do not run away on a garbage stream. */
    save_rbs    = data_fd2_runtime_battle_state_ptr;
    save_pcache = portrait_sprite_cache;
    save_map    = data_fd2_battle_tile_map_ptr;
    save_attr   = data_fd2_tile_attribute_flags_buffer_ptr;
    t_install_blit_atlases();
    save_lgs = data_fd2_large_game_state_buffer_ptr;
    data_fd2_large_game_state_buffer_ptr = (uint32)t_state_buf;

    /* Drive the REAL fire_chapter dispatcher at all three phases. Phase B
     * (phase 1) and Phase D (phase 0) fire while turn_counter == 7; Phase F
     * bumps it to 8 before firing phase 2, so that entry uses turn=8. */
    save_te = data_fd2_tile_event_data_table_ptr;
    memset(t_turn_event_table, 0, sizeof(t_turn_event_table));
    data_fd2_tile_event_data_table_ptr = (uint32)t_turn_event_table;
    /* entry[0]: turn=7, event_id=0x10, phase=1 (Phase B) */
    t_turn_event_table[3 + 0 * 3] = 7;
    t_turn_event_table[4 + 0 * 3] = 0x10;
    t_turn_event_table[5 + 0 * 3] = 1;
    /* entry[1]: turn=7, event_id=0x11, phase=0 (Phase D) */
    t_turn_event_table[3 + 1 * 3] = 7;
    t_turn_event_table[4 + 1 * 3] = 0x11;
    t_turn_event_table[5 + 1 * 3] = 0;
    /* entry[2]: turn=8, event_id=0x12, phase=2 (Phase F, post-bump) */
    t_turn_event_table[3 + 2 * 3] = 8;
    t_turn_event_table[4 + 2 * 3] = 0x12;
    t_turn_event_table[5 + 2 * 3] = 2;
    data_fd2_battle_ai_post_action_consequence_table[0x10] = turncycle_spy_b;
    data_fd2_battle_ai_post_action_consequence_table[0x11] = turncycle_spy_d;
    data_fd2_battle_ai_post_action_consequence_table[0x12] = turncycle_spy_f;
    g_turncycle_spy_b_fired = 0;
    g_turncycle_spy_d_fired = 0;
    g_turncycle_spy_f_fired = 0;

    /* one alive player char, already at full HP so Phase A heals nobody
     * (keeps the focus on Phases B-F). */
    g_test_rc_array[0].team = 2;
    g_test_rc_array[0].hp_current = 50;
    g_test_rc_array[0].hp_max = 50;
    data_fd2_battle_party_member_count = 1;

    /* Both per-chapter BGM tables are 0 at chapter 1 (default), so the
     * player!=enemy fade-out branches are skipped. Pre-set the real
     * fd2_set_bgm_track_with_fade's "last track" cache to 0 so its Phase
     * E / F calls with track_id=0 early-return (0==0) instead of loading
     * the real FDMUS.DAT. This keeps the cycle deterministic without
     * faking the real BGM setter. */
    data_fd2_audio_per_chapter_player_turn_bgm_track[1] = 0;
    data_fd2_audio_per_chapter_enemy_turn_bgm_track[1] = 0;
    data_fd2_audio_bgm_last_set_track_id = 0;
    data_fd2_chapter_current_chapter_id = 1;
    data_fd2_chapter_event_or_battle_end_code = 0;   /* no early exit */
    data_fd2_battle_turn_counter = 7;
    data_fd2_battle_current_active_char_idx = 99;
    data_fd2_battle_anim_phase = 5;
    g_scroll_buffer_calls = 0;
    g_restore_block_calls = 0;

    fd2_run_full_turn_cycle();

    /* turn counter bumped exactly once (Phase F). */
    ASSERT_EQ((long)data_fd2_battle_turn_counter, 8);
    /* Phase D banner (0x52) + Phase F banner (0x50). Each phase shows a
     * banner then slides it out: one real slide_in (scrolls 16x) + one real
     * slide_out (scrolls 17x) = 33 scrolls per phase. Two phases => 66 scroll
     * calls. fd2_scroll_buffer_block_with_wrap is called ONLY by the two banner
     * animators, so this confirms both banners ran for both phases (slide_in
     * AND slide_out fired exactly twice each; a missing slide_out would land at
     * 32, a missing whole banner far lower). The per-frame banner renderer's own
     * call sequence is pinned directly in tests/gfx/rndscene.c. */
    ASSERT_EQ(g_scroll_buffer_calls, 66);
    /* Real dispatcher fired the matching chapter event at each phase:
     * phase 1 (B) + phase 0 (D) while turn==7, phase 2 (F) at turn==8. */
    ASSERT_EQ(g_turncycle_spy_b_fired, 1);
    ASSERT_EQ(g_turncycle_spy_d_fired, 1);
    ASSERT_EQ(g_turncycle_spy_f_fired, 1);
    /* Phase F tail re-arms the active-char index and anim phase. */
    ASSERT_EQ((long)data_fd2_battle_current_active_char_idx, 0);
    ASSERT_EQ((long)data_fd2_battle_anim_phase, 1);
    /* Every save buffer allocated by the real cleanup-driving callees was
     * freed (the real fd2_cleanup_dialog_sprite_buffer forwards to the restore
     * stub once per call, so this counts total cleanups). Sources, all
     * deterministic: the two banners' per-frame renderer now cleans up 2x per
     * frame -> 24 frame renders (Phase D 7+5, Phase F 7+5) x 2 = 48; plus the
     * Phase-F reveal loops 9 (loop1) + 4 (loop2) = 13. Total 61. (If the EAX-fix
     * were wrong anywhere, free() of a bad pointer would crash before here.) */
    ASSERT_EQ(g_restore_block_calls, 61);

    data_fd2_battle_ai_post_action_consequence_table[0x10] = 0;
    data_fd2_battle_ai_post_action_consequence_table[0x11] = 0;
    data_fd2_battle_ai_post_action_consequence_table[0x12] = 0;
    data_fd2_tile_event_data_table_ptr = save_te;
    data_fd2_large_game_state_buffer_ptr = save_lgs;
    data_fd2_runtime_battle_state_ptr = save_rbs;
    portrait_sprite_cache = save_pcache;
    data_fd2_battle_tile_map_ptr = save_map;
    data_fd2_tile_attribute_flags_buffer_ptr = save_attr;
    data_fd2_battle_party_member_count = 4;
}


/* ---- fd2_fire_chapter_turn_events_for_phase (standalone) ----
 *
 * Dispatcher scans 16 entries (offset +3, 3-byte stride) of the table at
 * data_fd2_tile_event_data_table_ptr; for each entry whose turn byte (+3)
 * == data_fd2_battle_turn_counter and phase byte (+5) == the phase arg, it
 * calls data_fd2_battle_ai_post_action_consequence_table[event_id (+4)](0). */

static int g_fc_a_fired, g_fc_b_fired, g_fc_c_fired;
static uint32 g_fc_a_arg;
static void fc_spy_a(uint32 a) { g_fc_a_fired++; g_fc_a_arg = a; }
static void fc_spy_b(uint32 a) { (void)a; g_fc_b_fired++; }
static void fc_spy_c(uint32 a) { (void)a; g_fc_c_fired++; }

/* Reset spies + a zeroed 16-entry table pointed at by the global, and
 * install the three spies at fixed event_ids. Caller seeds entries and
 * the turn counter, then invokes the dispatcher. */
static uint32 g_fc_save_te;
static void fc_setup(void)
{
    g_fc_a_fired = g_fc_b_fired = g_fc_c_fired = 0;
    g_fc_a_arg = 0xFFFFFFFF;
    g_fc_save_te = data_fd2_tile_event_data_table_ptr;
    memset(t_turn_event_table, 0, sizeof(t_turn_event_table));
    data_fd2_tile_event_data_table_ptr = (uint32)t_turn_event_table;
    data_fd2_battle_ai_post_action_consequence_table[0x20] = fc_spy_a;
    data_fd2_battle_ai_post_action_consequence_table[0x21] = fc_spy_b;
    data_fd2_battle_ai_post_action_consequence_table[0x22] = fc_spy_c;
}

static void fc_teardown(void)
{
    data_fd2_battle_ai_post_action_consequence_table[0x20] = 0;
    data_fd2_battle_ai_post_action_consequence_table[0x21] = 0;
    data_fd2_battle_ai_post_action_consequence_table[0x22] = 0;
    data_fd2_tile_event_data_table_ptr = g_fc_save_te;
}

/* Write entry[idx]: turn @ +3, event_id @ +4, phase @ +5 (3-byte stride). */
static void fc_set_entry(int idx, uint8 turn, uint8 event_id, uint8 phase)
{
    t_turn_event_table[3 + idx * 3] = turn;
    t_turn_event_table[4 + idx * 3] = event_id;
    t_turn_event_table[5 + idx * 3] = phase;
}

/* A matching entry fires its handler exactly once, with arg 0. */
static void test_fire_chapter_match_fires(void)
{
    fc_setup();
    data_fd2_battle_turn_counter = 5;
    fc_set_entry(0, 5, 0x20, 2);    /* turn=5, handler 0x20, phase=2 */

    fd2_fire_chapter_turn_events_for_phase(2);

    ASSERT_EQ(g_fc_a_fired, 1);
    ASSERT_EQ((long)g_fc_a_arg, 0);   /* dispatcher passes 0 */
    fc_teardown();
}

/* Matching turn but wrong phase -> no fire. */
static void test_fire_chapter_phase_mismatch_skips(void)
{
    fc_setup();
    data_fd2_battle_turn_counter = 5;
    fc_set_entry(0, 5, 0x20, 1);    /* phase=1 */

    fd2_fire_chapter_turn_events_for_phase(2);   /* arg phase=2 */

    ASSERT_EQ(g_fc_a_fired, 0);
    fc_teardown();
}

/* Matching phase but wrong turn -> no fire. */
static void test_fire_chapter_turn_mismatch_skips(void)
{
    fc_setup();
    data_fd2_battle_turn_counter = 5;
    fc_set_entry(0, 6, 0x20, 2);    /* turn=6 != counter 5 */

    fd2_fire_chapter_turn_events_for_phase(2);

    ASSERT_EQ(g_fc_a_fired, 0);
    fc_teardown();
}

/* event_id (+4) selects which handler slot is called; only the matching
 * entry's handler runs. Verifies the +4 byte routes the call and that a
 * non-matching entry (different phase) is left alone. */
static void test_fire_chapter_event_id_routing(void)
{
    fc_setup();
    data_fd2_battle_turn_counter = 9;
    fc_set_entry(0, 9, 0x21, 0);    /* matches phase 0 -> handler 0x21 */
    fc_set_entry(1, 9, 0x20, 1);    /* phase 1, must NOT fire on arg 0 */

    fd2_fire_chapter_turn_events_for_phase(0);

    ASSERT_EQ(g_fc_b_fired, 1);     /* 0x21 fired */
    ASSERT_EQ(g_fc_a_fired, 0);     /* 0x20 skipped (phase mismatch) */
    fc_teardown();
}

/* The scan covers all 16 entries: a match in the last slot (index 15)
 * fires, while an entry in slot 16 (just past the scanned range) with the
 * same match criteria must NOT fire -> proves the bound is exactly 16. */
static void test_fire_chapter_scans_exactly_16(void)
{
    fc_setup();
    data_fd2_battle_turn_counter = 4;
    fc_set_entry(15, 4, 0x20, 2);   /* last scanned slot -> fires */
    fc_set_entry(16, 4, 0x21, 2);   /* out of range -> must not fire */

    fd2_fire_chapter_turn_events_for_phase(2);

    ASSERT_EQ(g_fc_a_fired, 1);     /* slot 15 fired */
    ASSERT_EQ(g_fc_b_fired, 0);     /* slot 16 not scanned */
    fc_teardown();
}

/* Multiple entries match the same turn+phase: every match fires. */
static void test_fire_chapter_multiple_matches(void)
{
    fc_setup();
    data_fd2_battle_turn_counter = 3;
    fc_set_entry(0, 3, 0x20, 1);
    fc_set_entry(5, 3, 0x22, 1);    /* same turn+phase, different handler */

    fd2_fire_chapter_turn_events_for_phase(1);

    ASSERT_EQ(g_fc_a_fired, 1);
    ASSERT_EQ(g_fc_c_fired, 1);
    fc_teardown();
}


/* ---- fd2_process_battle_drop_entries @ 0x1AA1D ----
 *
 * The item (type 0) and gold (type 1) reward paths drive the real dialog
 * VM + the real blocking fd2_wait_for_input_dialog_with_blink + portrait
 * blits; their pure display orchestration is deferred to Phase 9
 * integration (the same deferral testglob.c already applies to the
 * gold/item wait paths). These cases cover the deterministic, display-free
 * logic: the entry_count gate, the type 0/1 team!=2 early return, the
 * type-2 chapter-event dispatch (including the EAX/arg-tracking-bug fix
 * where the handler is invoked with recipient_idx, not 0), the unknown-type
 * skip, and loop iteration across multiple entries.
 *
 * Reuses the fc_* chapter-event spy table (installed at slots 0x20/0x21/
 * 0x22) since type-2 entries dispatch through the same
 * data_fd2_battle_ai_post_action_consequence_table. A type-2 entry's
 * value field selects the handler slot. */

/* Write drop entry[idx]: type @ +0, value (ushort) @ +1. */
static void drop_set_entry(uint8 *buf, int idx, uint8 type, uint16 value)
{
    buf[idx * 3 + 0] = type;
    *(uint16 *)(buf + idx * 3 + 1) = value;
}

/* entry_count == 0 -> immediate return, nothing dispatched even though the
 * buffer holds a would-fire type-2 entry. */
static void test_drop_count_zero_returns(void)
{
    uint8 drops[6];

    fc_setup();
    drop_set_entry(drops, 0, 2, 0x20);
    fd2_process_battle_drop_entries(0, 0, (uint32)drops);
    ASSERT_EQ(g_fc_a_fired, 0);
    fc_teardown();
}

/* type 2 (BATTLE EVENT): after delay, calls the handler-table slot named by
 * the entry value, passing recipient_idx. Verifies the arg is recipient_idx
 * (3 here), NOT 0 -- the Ghidra decompiler dropped the PUSH EDI argument. */
static void test_drop_type2_event_passes_recipient(void)
{
    uint8 drops[6];

    fc_setup();
    drop_set_entry(drops, 0, 2, 0x20);   /* value 0x20 -> fc_spy_a */
    fd2_process_battle_drop_entries(3, 1, (uint32)drops);
    ASSERT_EQ(g_fc_a_fired, 1);
    ASSERT_EQ((long)g_fc_a_arg, 3);      /* recipient_idx, not 0 */
    fc_teardown();
}

/* type 0 (ITEM) with recipient team != 2 -> immediate return before
 * fd2_add_item_to_inventory is ever reached. With the real add-item now linked,
 * the proxy is the recipient's inventory: slot 0 is pre-marked empty (flag
 * 0x80, sentinel id); a reached add-item would stamp flag=0 + item id=5, so the
 * slot staying empty proves the early return fired. */
static void test_drop_type0_nonplayer_team_returns(void)
{
    uint8 drops[6];

    fc_setup();
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[1].team = 0;          /* enemy team -> gate fails */
    g_test_rc_array[1].inventory_slots[0] = 0x80;   /* slot 0 empty       */
    g_test_rc_array[1].inventory_slots[1] = 0xEE;   /* sentinel item id   */
    data_fd2_battle_party_member_count = 4;
    drop_set_entry(drops, 0, 0, 5);
    fd2_process_battle_drop_entries(1, 1, (uint32)drops);
    /* never added: slot 0 still empty, sentinel id intact */
    ASSERT_EQ(g_test_rc_array[1].inventory_slots[0], 0x80);
    ASSERT_EQ(g_test_rc_array[1].inventory_slots[1], 0xEE);
    fc_teardown();
}

/* type 1 (GOLD) with recipient team != 2 -> immediate return; party gold
 * is left untouched. */
static void test_drop_type1_nonplayer_team_returns(void)
{
    uint8 drops[6];
    uint32 gold_before;

    fc_setup();
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[2].team = 1;          /* npc team -> gate fails */
    data_fd2_battle_party_member_count = 4;
    data_fd2_shared_party_total_gold = 777;
    gold_before = data_fd2_shared_party_total_gold;
    drop_set_entry(drops, 0, 1, 250);
    fd2_process_battle_drop_entries(2, 1, (uint32)drops);
    ASSERT_EQ(data_fd2_shared_party_total_gold, gold_before);
    fc_teardown();
}

/* An unknown type (5) is skipped and the loop continues to the next entry
 * (a type-2 event), which fires -- proving the default branch falls through
 * to the loop increment rather than returning. */
static void test_drop_unknown_type_skipped_loop_continues(void)
{
    uint8 drops[9];

    fc_setup();
    drop_set_entry(drops, 0, 5, 0x1234);  /* unknown -> skip */
    drop_set_entry(drops, 1, 2, 0x22);    /* value 0x22 -> fc_spy_c */
    fd2_process_battle_drop_entries(0, 2, (uint32)drops);
    ASSERT_EQ(g_fc_c_fired, 1);
    fc_teardown();
}

/* Two type-2 entries both dispatch: the loop iterates every entry, each
 * selecting its own handler slot by value. */
static void test_drop_type2_loop_dispatches_all(void)
{
    uint8 drops[9];

    fc_setup();
    drop_set_entry(drops, 0, 2, 0x20);    /* fc_spy_a */
    drop_set_entry(drops, 1, 2, 0x22);    /* fc_spy_c */
    fd2_process_battle_drop_entries(0, 2, (uint32)drops);
    ASSERT_EQ(g_fc_a_fired, 1);
    ASSERT_EQ(g_fc_c_fired, 1);
    fc_teardown();
}


/* ---- fd2_count_active_chars_for_team_filter @ 0x1B5F1 ----
 *
 * Counts chars on `team` that pass a 4-condition AND filter: team match,
 * portrait_id != 0x79 (hidden/quest portrait), archetype_flag != 10
 * (boss/special), and !fd2_check_char_is_dead(i) (alive). The real
 * fd2_check_char_is_dead reads runtime_char[i].flags bit0, so the alive
 * condition is pinned by leaving .flags == 0 (memset) for alive chars and
 * setting .flags |= CHARFLAG_DEAD for dead ones. */

/* All four AND conditions, with one disqualifier per filter present so each
 * exclusion is individually proven. All chars alive (.flags == 0 after memset).
 *   [0] team=1, portrait=1, arch=0   -> COUNTS
 *   [1] team=0 (wrong team)          -> excluded
 *   [2] team=1, portrait=0x79        -> excluded (hidden portrait)
 *   [3] team=1, arch=10              -> excluded (boss)
 *   [4] team=1, portrait=5, arch=2   -> COUNTS
 * Expected count for team 1 = 2. */
static void test_count_active_basic_filters(void)
{
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    g_test_rc_array[0].team = 1;
    g_test_rc_array[0].portrait_id = 1;
    g_test_rc_array[0].archetype_flag = 0;

    g_test_rc_array[1].team = 0;            /* wrong team -> excluded */
    g_test_rc_array[1].portrait_id = 1;
    g_test_rc_array[1].archetype_flag = 0;

    g_test_rc_array[2].team = 1;
    g_test_rc_array[2].portrait_id = 0x79;  /* hidden portrait -> excluded */
    g_test_rc_array[2].archetype_flag = 0;

    g_test_rc_array[3].team = 1;
    g_test_rc_array[3].portrait_id = 1;
    g_test_rc_array[3].archetype_flag = 10; /* boss -> excluded */

    g_test_rc_array[4].team = 1;
    g_test_rc_array[4].portrait_id = 5;
    g_test_rc_array[4].archetype_flag = 2;

    data_fd2_battle_party_member_count = 5;

    result = fd2_count_active_chars_for_team_filter(1);
    ASSERT_EQ(result, 2);

    data_fd2_battle_party_member_count = 4;
}

/* Same roster scanned per-team: the function is called with team 0/1/2 by
 * fd2_render_party_status_overview_content, so confirm the team filter
 * partitions the count independently.
 *   team 0: [1],[3] -> 2     team 1: [0],[4] -> 2     team 2: [2] -> 1 */
static void test_count_active_per_team(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;

    g_test_rc_array[0].team = 1;            /* counts for team 1 */
    g_test_rc_array[1].team = 0;            /* counts for team 0 */
    g_test_rc_array[2].team = 2;            /* counts for team 2 */
    g_test_rc_array[3].team = 0;            /* counts for team 0 */
    g_test_rc_array[4].team = 1;            /* counts for team 1 */
    data_fd2_battle_party_member_count = 5;

    ASSERT_EQ(fd2_count_active_chars_for_team_filter(0), 2);
    ASSERT_EQ(fd2_count_active_chars_for_team_filter(1), 2);
    ASSERT_EQ(fd2_count_active_chars_for_team_filter(2), 1);

    data_fd2_battle_party_member_count = 4;
}

/* Dead-check AND-condition: chars that pass team/portrait/archetype are
 * still excluded when fd2_check_char_is_dead returns nonzero. Setting the
 * dead bit (CHARFLAG_DEAD) on all three otherwise-qualifying chars drops the
 * count to 0 -- proving the alive condition gates the increment. */
static void test_count_active_dead_excluded(void)
{
    int result;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    g_test_rc_array[0].team = 1;
    g_test_rc_array[1].team = 1;
    g_test_rc_array[2].team = 1;
    data_fd2_battle_party_member_count = 3;

    /* all alive (.flags == 0) -> all 3 count */
    result = fd2_count_active_chars_for_team_filter(1);
    ASSERT_EQ(result, 3);

    /* all dead -> none count */
    g_test_rc_array[0].flags |= CHARFLAG_DEAD;
    g_test_rc_array[1].flags |= CHARFLAG_DEAD;
    g_test_rc_array[2].flags |= CHARFLAG_DEAD;
    result = fd2_count_active_chars_for_team_filter(1);
    ASSERT_EQ(result, 0);

    data_fd2_battle_party_member_count = 4;
}

/* Empty roster (party_member_count == 0): the loop body never runs and the
 * count is 0, pinning the JGE loop-exit at entry. */
static void test_count_active_empty_roster(void)
{
    int result;

    data_fd2_battle_party_member_count = 0;
    result = fd2_count_active_chars_for_team_filter(1);
    ASSERT_EQ(result, 0);
    data_fd2_battle_party_member_count = 4;
}


/* ---- Tests: fd2_process_xp_and_level_up_for_char ----
 *
 * The handler drives the real dialog VM / portrait loader / status-screen
 * teardown for its on-screen feedback, and the real char_growth / spell_learning
 * table accessors, fd2_roll_stat_gain_and_show_message (the real stat roll, now
 * emitted in this file) and fd2_recalculate_combat_stats for its math. Its one
 * remaining not-yet-emitted callee (fd2_grant_spell_to_char) is a recording fake
 * in testglob.c. The dialog VM is neutralised with an END-only page table
 * (extended to cover page 0x24B used by the spell-learn message). What is pinned
 * here is the handler's own logic: the three skip gates, the XP carry-in/carry-out
 * arithmetic, the level-up count (observed via the per-slot stat gains the real
 * roll applies), the per-call level cap (30 normal / 99 hero), and the spell-learn
 * trigger. The 5-slot roll's stat-write sequence is checked here against the
 * fixed RNG-free gains seeded by xp_setup; the roll primitive's own RNG / row /
 * dialog paths are pinned by the test_roll_stat_* cases below. */

extern int    g_grant_spell_calls;
extern uint32 g_grant_spell_last_char;
extern uint32 g_grant_spell_last_spell;
extern uint32 data_fd2_dialog_last_action_sprite_id_param;
extern character_growth data_fd2_battle_character_growth_table[68];
extern uint8 data_fd2_spell_learning_table[20 * 12];

/* END-only dialog page table covering every page index the handler emits
 * (0x1E8/0x1E9/0x1EA-0x1EE and 0x24B). */
#define T_XP_PAGES   0x24C
static int16 t_xp_dlg_text[T_XP_PAGES + 1];

static void xp_setup(uint8 portrait_id, uint8 spell_learn_idx)
{
    int p;
    character_growth *g;

    for (p = 0; p < T_XP_PAGES; p++) {
        t_xp_dlg_text[p] = (int16)(T_XP_PAGES * 2);
    }
    t_xp_dlg_text[T_XP_PAGES] = -1;
    data_fd2_all_game_text_ptr = (uint32)t_xp_dlg_text;
    data_fd2_dialog_active_portrait_blit_offset = 0;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].portrait_id = portrait_id;
    g_test_rc_array[0].flags = 0;

    /* Known growth row for this portrait. Each (min,max) pair is min==max so the
     * real fd2_roll_stat_gain_and_show_message rolls a fixed, RNG-free gain equal
     * to that min (range==0 -> no fd2_advance_rng_state). Distinct per-stat gains
     * (AP 1, DP 2, DX 3, HP 4, MP 5) let the per-slot stat writes be pinned.
     * spell-learning index per the test. */
    g = &data_fd2_battle_character_growth_table[portrait_id];
    memset(g, 0, sizeof(*g));
    g->ap_min = 1; g->ap_max = 1;
    g->dp_min = 2; g->dp_max = 2;
    g->dx_min = 3; g->dx_max = 3;
    g->hp_min = 4; g->hp_max = 4;
    g->mp_min = 5; g->mp_max = 5;
    g->spell_learning_idx = spell_learn_idx;

    g_grant_spell_calls = 0;
    g_grant_spell_last_char = 0xFFFFFFFFu;
    g_grant_spell_last_spell = 0xFFFFFFFFu;
    data_fd2_dialog_last_action_sprite_id_param = 0;
    data_fd2_battle_party_member_count = 4;
}

static void test_xp_gate_no_pending(void)
{
    xp_setup(5, 0xFF);
    g_test_rc_array[0].status_flags_block[0] = 1;
    g_test_rc_array[0].movement_order = 7;
    data_fd2_battle_pending_xp_credit = 0;
    fd2_process_xp_and_level_up_for_char(0);
    /* gate skips: no level-up, no stat roll applied (hp_max stays 0) */
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 0);
    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 1);
    ASSERT_EQ((long)g_test_rc_array[0].movement_order, 7);
}

static void test_xp_gate_dead(void)
{
    xp_setup(5, 0xFF);
    g_test_rc_array[0].flags = 1;
    g_test_rc_array[0].status_flags_block[0] = 1;
    data_fd2_battle_pending_xp_credit = 200;
    fd2_process_xp_and_level_up_for_char(0);
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 0);
    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 1);
    /* pending is NOT cleared when the gate skips */
    ASSERT_EQ((long)data_fd2_battle_pending_xp_credit, 200);
}

static void test_xp_gate_normal_level_cap(void)
{
    xp_setup(5, 0xFF);
    g_test_rc_array[0].status_flags_block[0] = 0x28;  /* normal cap */
    data_fd2_battle_pending_xp_credit = 200;
    fd2_process_xp_and_level_up_for_char(0);
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 0);
    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 0x28);
    ASSERT_EQ((long)data_fd2_battle_pending_xp_credit, 200);
}

static void test_xp_gate_hero_level_cap(void)
{
    xp_setup(0x1E, 0xFF);                              /* hero portrait */
    g_test_rc_array[0].status_flags_block[0] = 99;     /* hero cap */
    data_fd2_battle_pending_xp_credit = 200;
    fd2_process_xp_and_level_up_for_char(0);
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 0);
    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 99);
}

static void test_xp_single_levelup_and_roll_sequence(void)
{
    uint8 *base;

    xp_setup(5, 0xFF);
    g_test_rc_array[0].status_flags_block[0] = 1;
    g_test_rc_array[0].movement_order = 0;
    data_fd2_battle_pending_xp_credit = 150;
    fd2_process_xp_and_level_up_for_char(0);

    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 2);   /* +1 level */
    ASSERT_EQ((long)g_test_rc_array[0].movement_order, 50);         /* 150-100 carry */
    ASSERT_EQ((long)data_fd2_battle_pending_xp_credit, 0);          /* cleared */

    /* The 5 stat slots the handler rolls, each += its fixed gain (AP 1, DP 2,
     * DX 3, HP 4, MP 5 from xp_setup), proving the slot ptr / growth-pair
     * routing the handler hands to fd2_roll_stat_gain_and_show_message. */
    base = (uint8 *)&g_test_rc_array[0];
    ASSERT_EQ((long)*(int16 *)(base + 0x37), 1);   /* combat_aux_block[0x10] AP */
    ASSERT_EQ((long)*(int16 *)(base + 0x39), 2);   /* combat_aux_block[0x12] DP */
    ASSERT_EQ((long)*(int16 *)(base + 0x3E), 3);   /* ai_target_and_dx_block[1] DX */
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 4);
    ASSERT_EQ((long)g_test_rc_array[0].mp_max, 5);
    /* last roll's gain (MP slot, 5) stays latched in the dialog value param */
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, 5);
}

static void test_xp_multi_levelup(void)
{
    uint8 *base;

    xp_setup(5, 0xFF);
    g_test_rc_array[0].status_flags_block[0] = 1;
    g_test_rc_array[0].movement_order = 0;
    data_fd2_battle_pending_xp_credit = 250;
    fd2_process_xp_and_level_up_for_char(0);

    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 3);   /* +2 levels */
    ASSERT_EQ((long)g_test_rc_array[0].movement_order, 50);        /* 250-200 */
    ASSERT_EQ((long)data_fd2_battle_pending_xp_credit, 0);
    /* each of the 5 slots rolled once per level -> twice the single-level gain */
    base = (uint8 *)&g_test_rc_array[0];
    ASSERT_EQ((long)*(int16 *)(base + 0x37), 2);   /* AP 1*2 */
    ASSERT_EQ((long)*(int16 *)(base + 0x39), 4);   /* DP 2*2 */
    ASSERT_EQ((long)*(int16 *)(base + 0x3E), 6);   /* DX 3*2 */
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 8);
    ASSERT_EQ((long)g_test_rc_array[0].mp_max, 10);
}

static void test_xp_carryover_movement_order(void)
{
    xp_setup(5, 0xFF);
    g_test_rc_array[0].status_flags_block[0] = 1;
    g_test_rc_array[0].movement_order = 60;            /* prior carry-over */
    data_fd2_battle_pending_xp_credit = 50;            /* 50 + 60 = 110 */
    fd2_process_xp_and_level_up_for_char(0);

    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 2);   /* one level */
    /* one level-up: each slot rolled once -> single-level gains */
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 4);
    ASSERT_EQ((long)g_test_rc_array[0].mp_max, 5);
    ASSERT_EQ((long)g_test_rc_array[0].movement_order, 10);        /* 110-100 */
    ASSERT_EQ((long)data_fd2_battle_pending_xp_credit, 0);
}

static void test_xp_percall_cap_level30(void)
{
    xp_setup(5, 0xFF);
    g_test_rc_array[0].status_flags_block[0] = 29;
    g_test_rc_array[0].movement_order = 0;
    data_fd2_battle_pending_xp_credit = 500;           /* enough for many levels */
    fd2_process_xp_and_level_up_for_char(0);

    /* per-call cap: reaching level 30 forces remaining_xp to 0 -> single level */
    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 0x1E);
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 4);                /* single level-up roll */
    ASSERT_EQ((long)g_test_rc_array[0].movement_order, 0);        /* leftover discarded */
    ASSERT_EQ((long)data_fd2_battle_pending_xp_credit, 0);
}

static void test_xp_hero_cap_level99(void)
{
    xp_setup(0x1E, 0xFF);
    g_test_rc_array[0].status_flags_block[0] = 98;
    g_test_rc_array[0].movement_order = 0;
    data_fd2_battle_pending_xp_credit = 500;
    fd2_process_xp_and_level_up_for_char(0);

    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 99);
    ASSERT_EQ((long)g_test_rc_array[0].hp_max, 4);               /* single level-up roll */
    ASSERT_EQ((long)g_test_rc_array[0].movement_order, 0);
    ASSERT_EQ((long)data_fd2_battle_pending_xp_credit, 0);
}

static void test_xp_spell_learn_on_match(void)
{
    uint8 *learn;

    xp_setup(5, 3);                                    /* growth[5].spell_learning_idx = 3 */
    learn = &data_fd2_spell_learning_table[3 * 12];
    memset(learn, 0xEE, 12);                           /* no pair matches by default */
    learn[0] = 2;                                      /* pair0 req_level = 2 (the new level) */
    learn[1] = 7;                                      /* pair0 spell_id = 7 */
    g_test_rc_array[0].status_flags_block[0] = 1;
    data_fd2_battle_pending_xp_credit = 150;           /* one level: 1 -> 2 */
    fd2_process_xp_and_level_up_for_char(0);

    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 2);
    ASSERT_EQ((long)g_grant_spell_calls, 1);
    ASSERT_EQ((long)g_grant_spell_last_char, 0);
    ASSERT_EQ((long)g_grant_spell_last_spell, 7);
    /* spell name text id = spell_id + 0x1B9 */
    ASSERT_EQ((long)data_fd2_dialog_last_action_sprite_id_param, (long)(7 + 0x1B9));
}

static void test_xp_spell_learn_no_match(void)
{
    uint8 *learn;

    xp_setup(5, 3);
    learn = &data_fd2_spell_learning_table[3 * 12];
    memset(learn, 0xEE, 12);                           /* no req_level equals level 2 */
    g_test_rc_array[0].status_flags_block[0] = 1;
    data_fd2_battle_pending_xp_credit = 150;
    fd2_process_xp_and_level_up_for_char(0);

    ASSERT_EQ((long)g_test_rc_array[0].status_flags_block[0], 2);
    ASSERT_EQ((long)g_grant_spell_calls, 0);           /* nothing learned */
}


/* ---- Tests: fd2_roll_stat_gain_and_show_message (the stat-roll primitive) ----
 *
 * Drives the roll/apply/return logic directly. The display side (dialog VM,
 * cinematic scroll) is neutralised: an END-only page table makes the real
 * fd2_display_dialog_scene a no-op (portrait_anim never set), and with
 * data_fd2_dialog_active_portrait_blit_offset == 0 the real cinematic scroll
 * returns without touching VGA. What is pinned: the (min,max) growth decode,
 * the RNG-vs-fixed gain (incl. the EAX-bug path that must use the RNG return,
 * not the growth pointer), the gain==0 skip (no stat write, row unchanged),
 * the *(int16*)stat_ptr += gain apply, and the row==3 collapse + advance. */

/* END-only page table covering the few text ids these tests pass. */
#define T_ROLL_PAGES   0x200
static int16 t_roll_dlg_text[T_ROLL_PAGES + 1];

static void roll_setup(void)
{
    int p;
    for (p = 0; p < T_ROLL_PAGES; p++) {
        t_roll_dlg_text[p] = (int16)(T_ROLL_PAGES * 2);   /* -> the END slot */
    }
    t_roll_dlg_text[T_ROLL_PAGES] = -1;                   /* END opcode */
    data_fd2_all_game_text_ptr = (uint32)t_roll_dlg_text;
    data_fd2_dialog_active_portrait_blit_offset = 0;      /* scroll = no-op */
}

static void test_roll_stat_zero_gain_skips(void)
{
    uint8 growth[2];
    int16 stat;
    int next_row;

    roll_setup();
    growth[0] = 0; growth[1] = 0;                         /* min==max==0 -> gain 0 */
    stat = 7;
    data_fd2_dialog_last_action_value_param = 0xFFFF;
    next_row = fd2_roll_stat_gain_and_show_message(
        (short *)&stat, growth, 0x1EA, 2);
    ASSERT_EQ((long)next_row, 2);                         /* row unchanged */
    ASSERT_EQ((long)stat, 7);                             /* no stat write */
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, 0);
}

static void test_roll_stat_fixed_gain_applies(void)
{
    uint8 growth[2];
    int16 stat;
    int next_row;

    roll_setup();
    growth[0] = 6; growth[1] = 6;                         /* range 0 -> fixed gain 6 */
    stat = 10;
    next_row = fd2_roll_stat_gain_and_show_message(
        (short *)&stat, growth, 0x1EB, 2);
    ASSERT_EQ((long)next_row, 3);                         /* advanced one row */
    ASSERT_EQ((long)stat, 16);                            /* 10 + 6 */
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, 6);
}

static void test_roll_stat_rng_path_modulo(void)
{
    uint8 growth[2];
    int16 stat;
    int next_row;

    /* seed 0 -> first fd2_advance_rng_state() returns 0x80A4 (32932).
     * range 5 -> rand_extra = 32932 % 5 = 2; gain = min(10) + 2 = 12.
     * This pins the EAX-bug fix: the dividend is the RNG return, not growth. */
    roll_setup();
    data_fd2_shared_rng_seed = 0;
    growth[0] = 10; growth[1] = 15;                       /* range 5 */
    stat = 100;
    next_row = fd2_roll_stat_gain_and_show_message(
        (short *)&stat, growth, 0x1EC, 2);
    ASSERT_EQ((long)next_row, 3);
    ASSERT_EQ((long)stat, 112);                           /* 100 + 12 */
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, 12);
    ASSERT_EQ((long)data_fd2_shared_rng_seed, 0x80A4);    /* RNG advanced once */
}

static void test_roll_stat_negative_range_signed_modulo(void)
{
    uint8 growth[2];
    int16 stat;
    int next_row;

    /* max < min -> range = 2 - 5 = -3 (signed IDIV). seed 0 -> rng 32932.
     * 32932 % -3 == 1 (C truncates toward zero); gain = 5 + 1 = 6. */
    roll_setup();
    data_fd2_shared_rng_seed = 0;
    growth[0] = 5; growth[1] = 2;
    stat = 0;
    next_row = fd2_roll_stat_gain_and_show_message(
        (short *)&stat, growth, 0x1ED, 2);
    ASSERT_EQ((long)next_row, 3);
    ASSERT_EQ((long)stat, 6);
    ASSERT_EQ((long)data_fd2_dialog_last_action_value_param, 6);
}

static void test_roll_stat_row3_collapses_and_advances(void)
{
    uint8 growth[2];
    int16 stat;
    int next_row;

    /* row_idx 3 with a non-zero gain: collapse to row 2 (scroll is a no-op
     * with offset 0), render, then advance -> returns 3 (not 4). */
    roll_setup();
    growth[0] = 4; growth[1] = 4;                         /* fixed gain 4 */
    stat = 1;
    next_row = fd2_roll_stat_gain_and_show_message(
        (short *)&stat, growth, 0x1EE, 3);
    ASSERT_EQ((long)next_row, 3);                         /* row 3 -> 2 -> ++ = 3 */
    ASSERT_EQ((long)stat, 5);                             /* 1 + 4 */
}

static void test_roll_stat_zero_gain_row3_no_collapse(void)
{
    uint8 growth[2];
    int16 stat;
    int next_row;

    /* gain 0 short-circuits before the row==3 branch: row returned verbatim. */
    roll_setup();
    growth[0] = 0; growth[1] = 0;
    stat = 9;
    next_row = fd2_roll_stat_gain_and_show_message(
        (short *)&stat, growth, 0x1EA, 3);
    ASSERT_EQ((long)next_row, 3);                         /* unchanged, no collapse */
    ASSERT_EQ((long)stat, 9);
}


void run_battle_btl_turn_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: battle/btl_turn\n");
    RUN_TEST(test_status_tick_poison_damage);
    RUN_TEST(test_status_tick_poison_clamp_zero);
    RUN_TEST(test_status_tick_poison_skip_dead);
    RUN_TEST(test_status_tick_timer_decrement);
    RUN_TEST(test_status_tick_timer_expires_recalc);
    RUN_TEST(test_find_char_at_cursor_found);
    RUN_TEST(test_find_char_at_cursor_not_found);
    RUN_TEST(test_find_char_by_id_found);
    RUN_TEST(test_find_char_by_id_fallback_template);
    RUN_TEST(test_mark_char_acted);
    RUN_TEST(test_check_all_acted_not_done);
    RUN_TEST(test_check_all_acted_triggers);
    RUN_TEST(test_check_tile_event_no_trigger);
    RUN_TEST(test_check_tile_event_triggers);
    RUN_TEST(test_check_tile_event_event_type_mismatch);
    RUN_TEST(test_mark_char_as_dead);
    RUN_TEST(test_set_combat_aux_low4);
    RUN_TEST(test_kill_from_index_zeros_tail_range);
    RUN_TEST(test_kill_from_index_zero_kills_all);
    RUN_TEST(test_kill_from_index_empty_range_noop);
    RUN_TEST(test_check_battle_end_victory);
    RUN_TEST(test_check_battle_end_continues);
    RUN_TEST(test_check_battle_end_gameover);
    RUN_TEST(test_check_battle_end_gameover_overrides_continue);
    RUN_TEST(test_collect_dead_char_drops);
    RUN_TEST(test_collect_pending_drops);
    RUN_TEST(test_run_turn_cycle_phase_a_heal);
    RUN_TEST(test_run_turn_cycle_full_reveal);
    RUN_TEST(test_fire_chapter_match_fires);
    RUN_TEST(test_fire_chapter_phase_mismatch_skips);
    RUN_TEST(test_fire_chapter_turn_mismatch_skips);
    RUN_TEST(test_fire_chapter_event_id_routing);
    RUN_TEST(test_fire_chapter_scans_exactly_16);
    RUN_TEST(test_fire_chapter_multiple_matches);
    RUN_TEST(test_drop_count_zero_returns);
    RUN_TEST(test_drop_type2_event_passes_recipient);
    RUN_TEST(test_drop_type0_nonplayer_team_returns);
    RUN_TEST(test_drop_type1_nonplayer_team_returns);
    RUN_TEST(test_drop_unknown_type_skipped_loop_continues);
    RUN_TEST(test_drop_type2_loop_dispatches_all);
    RUN_TEST(test_count_active_basic_filters);
    RUN_TEST(test_count_active_per_team);
    RUN_TEST(test_count_active_dead_excluded);
    RUN_TEST(test_count_active_empty_roster);
    RUN_TEST(test_xp_gate_no_pending);
    RUN_TEST(test_xp_gate_dead);
    RUN_TEST(test_xp_gate_normal_level_cap);
    RUN_TEST(test_xp_gate_hero_level_cap);
    RUN_TEST(test_xp_single_levelup_and_roll_sequence);
    RUN_TEST(test_xp_multi_levelup);
    RUN_TEST(test_xp_carryover_movement_order);
    RUN_TEST(test_xp_percall_cap_level30);
    RUN_TEST(test_xp_hero_cap_level99);
    RUN_TEST(test_xp_spell_learn_on_match);
    RUN_TEST(test_xp_spell_learn_no_match);
    RUN_TEST(test_roll_stat_zero_gain_skips);
    RUN_TEST(test_roll_stat_fixed_gain_applies);
    RUN_TEST(test_roll_stat_rng_path_modulo);
    RUN_TEST(test_roll_stat_negative_range_signed_modulo);
    RUN_TEST(test_roll_stat_row3_collapses_and_advances);
    RUN_TEST(test_roll_stat_zero_gain_row3_no_collapse);
    printf("\n");
}
