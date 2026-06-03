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


/* ---- Test: fd2_tick_status_effects_and_show_messages ---- */

static void test_status_tick_poison_damage(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    t_install_dialog_text();
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
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
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
    g_test_rc_array[0].team = 0;
    g_test_rc_array[0].flags = 0;
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
    RUN_TEST(test_check_battle_end_victory);
    RUN_TEST(test_check_battle_end_continues);
    RUN_TEST(test_check_battle_end_gameover);
    RUN_TEST(test_check_battle_end_gameover_overrides_continue);
    RUN_TEST(test_collect_dead_char_drops);
    RUN_TEST(test_collect_pending_drops);
    printf("\n");
}
