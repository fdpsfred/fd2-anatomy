/*
 * btl_turn.c — Battle turn cycle: turn loop, XP/level-up, drops, status tick, queries
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_tick_status_effects_and_show_messages @ 0x1A866
 *
 * End-of-turn status effect ticker. Two passes:
 *   Pass 1: poison damage (10% max HP) with dialog + death check
 *   Pass 2: timer-status countdown (slots 0..5) with removal dialog
 * ---------------------------------------------------------------- */
void fd2_tick_status_effects_and_show_messages(uint32 team)
{
    int i;
    uint8 *pChar;
    uint32 hp_current;
    uint32 damage;
    int hp_after;
    int timer_slot;
    uint8 timer_val;

    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        if (pChar[0x25] != 0 &&
            (uint32)pChar[6] == team &&
            (pChar[5] & 0x01) == 0) {
            hp_current = (uint32)*(uint16 *)(pChar + 0x40);
            damage = (uint32)*(uint16 *)(pChar + 0x42) / 10;
            data_fd2_dialog_last_action_value_param = damage;
            hp_after = (int)hp_current - (int)damage;
            if (hp_after < 0) hp_after = 0;
            *(uint16 *)(pChar + 0x40) = (uint16)hp_after;
            data_fd2_battle_anim_phase = 0;
            fd2_pan_cursor_to_char((uint32)i);
            data_fd2_battle_anim_phase = 1;
            fd2_load_chapter_portrait((uint32)pChar[7]);
            fd2_display_dialog_scene(
                data_fd2_all_game_text_ptr, 0x1E7,
                0xA9F23, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
            fd2_clear_keyboard_buffer();
            fd2_wait_ticks_or_keypress_with_palette(10);
            fd2_close_status_screen_with_slide_out();
        }
    }

    fd2_play_death_animation_and_mark_dead();
    data_fd2_chapter_post_action_handler_table
        [data_fd2_chapter_current_chapter_id](0);

    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        for (timer_slot = 0; timer_slot < 6; timer_slot++) {
            if ((uint32)pChar[6] == team &&
                (pChar[5] & 0x01) == 0) {
                timer_val = pChar[0x22 + timer_slot];
                if (timer_val != 0) {
                    pChar[0x22 + timer_slot] = timer_val - 1;
                    if (pChar[0x22 + timer_slot] == 0) {
                        data_fd2_battle_anim_phase = 0;
                        fd2_pan_cursor_to_char((uint32)i);
                        data_fd2_battle_anim_phase = 1;
                        fd2_load_chapter_portrait(
                            (uint32)pChar[7]);
                        fd2_display_dialog_scene(
                            data_fd2_all_game_text_ptr,
                            (uint32)timer_slot + 0x1E1,
                            0xA9F23, 0x140, 0xCD, 0x4C,
                            0x4A, 0x13, 1);
                        fd2_clear_keyboard_buffer();
                        fd2_wait_ticks_or_keypress_with_palette(
                            10);
                        fd2_close_status_screen_with_slide_out();
                        fd2_recalculate_combat_stats((uint32)i);
                    }
                }
            }
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_find_char_at_cursor_pos @ 0x12C0D
 *
 * Locate the alive char standing at (cursor_world_x, cursor_world_y).
 * Returns char_idx, or -1 if none found.
 * ---------------------------------------------------------------- */
int fd2_find_char_at_cursor_pos(void)
{
    int char_iter;
    uint8 *pChar;

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr;
    for (char_iter = 0;
         char_iter < (int)data_fd2_battle_party_member_count;
         char_iter++) {
        if (pChar[0] == data_fd2_battle_cursor_world_x &&
            pChar[1] == data_fd2_battle_cursor_world_y) {
            if (!fd2_check_char_is_dead(char_iter)) {
                return char_iter;
            }
        }
        pChar += RUNTIME_CHAR_SIZE;
    }
    return -1;
}

/* ----------------------------------------------------------------
 * fd2_find_char_by_id_or_template @ 0x12C60
 *
 * Locate alive battle char with char_id == target_char_id.
 * Fallback: if no battle match, scan menu party roster for
 * template ptr (for dialog portrait rendering).
 * Side-effect: sets data_fd2_dialog_current_speaker_char_ptr.
 * ---------------------------------------------------------------- */
int fd2_find_char_by_id_or_template(uint32 target_char_id)
{
    int char_iter;
    uint8 *pChar;
    int i;
    uint8 *tmpl;

    data_fd2_dialog_current_speaker_char_ptr = 0;
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr;
    for (char_iter = 0;
         char_iter < (int)data_fd2_battle_party_member_count;
         char_iter++) {
        if (pChar[8] == target_char_id) {
            data_fd2_dialog_current_speaker_char_ptr = (uint32)pChar;
            if (!fd2_check_char_is_dead(char_iter)) {
                return char_iter;
            }
        }
        pChar += RUNTIME_CHAR_SIZE;
    }
    if (data_fd2_dialog_current_speaker_char_ptr == 0) {
        tmpl = (uint8 *)data_fd2_shared_menu_party_roster_buffer_ptr;
        for (i = 0;
             i < (int)data_fd2_shared_menu_party_member_count;
             i++) {
            if (tmpl[8] == target_char_id) {
                data_fd2_dialog_current_speaker_char_ptr = (uint32)tmpl;
            }
            tmpl += RUNTIME_CHAR_SIZE;
        }
    }
    return -1;
}

/* ----------------------------------------------------------------
 * fd2_mark_char_acted_this_turn @ 0x13512
 *
 * Set runtime_char[char_idx].flags bit 0x80 (acted-this-turn).
 * ---------------------------------------------------------------- */
void fd2_mark_char_acted_this_turn(uint32 char_idx)
{
    uint8 *pChar;

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    pChar[5] |= CHARFLAG_ACTED;
}

/* ----------------------------------------------------------------
 * fd2_check_all_player_acted_or_asleep @ 0x13565
 *
 * Detect end-of-player-turn: if every player char is dead, acted,
 * or asleep, trigger full turn cycle (enemy/NPC phase).
 * ---------------------------------------------------------------- */
void fd2_check_all_player_acted_or_asleep(void)
{
    int char_iter;
    uint8 *pChar;
    int all_done;

    all_done = 1;
    for (char_iter = 0;
         char_iter < (int)data_fd2_battle_party_member_count;
         char_iter++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + char_iter * RUNTIME_CHAR_SIZE;
        if ((pChar[5] & 0x81) == 0 &&
            pChar[6] == TEAM_PLAYER &&
            pChar[0x26] == 0) {
            all_done = 0;
        }
    }
    if (all_done) {
        data_fd2_ui_play_active_flag = 0;
        data_fd2_battle_anim_phase = 0;
        fd2_run_full_turn_cycle();
        data_fd2_battle_anim_phase = 1;
        data_fd2_ui_play_active_flag = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_check_tile_event_post_action @ 0x13A44
 *
 * After a walk-step lands on (world_x, world_y), check if the
 * tile fires a scripted post-action consequence.
 * ---------------------------------------------------------------- */
void fd2_check_tile_event_post_action(uint32 world_x, uint32 world_y,
                                       uint32 expected_event_type)
{
    uint8 tile_buf[8];
    uint16 terrain_class;
    uint32 rec_addr;
    uint32 consequence_idx;
    uint32 event_type;

    fd2_read_tile_attribute_at_pos(world_x, world_y, (uint32)tile_buf);
    if ((tile_buf[4] & 0x60) == 0) {
        terrain_class = *(uint16 *)(tile_buf + 2);
        if (terrain_class != 0) {
            rec_addr = data_fd2_tile_event_data_table_ptr +
                       (terrain_class - 1) * 2;
            consequence_idx =
                (uint32)*(uint8 *)(rec_addr + 0x33);
            event_type =
                (uint32)*(uint8 *)(rec_addr + 0x34);
            if (consequence_idx != 0xFF &&
                event_type == expected_event_type) {
                data_fd2_battle_ai_post_action_consequence_idx =
                    consequence_idx;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_mark_char_as_dead @ 0x32975
 *
 * Set runtime_char[char_idx].flags = 1 (dead). Overwrites all bits.
 * ---------------------------------------------------------------- */
void fd2_mark_char_as_dead(uint32 char_idx)
{
    uint8 *pChar;

    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + char_idx * RUNTIME_CHAR_SIZE;
    pChar[5] = CHARFLAG_DEAD;
}

/* ----------------------------------------------------------------
 * fd2_set_combat_aux_block_byte_d_low4_for_char_range @ 0x3419C
 *
 * Write low 4 bits of new_val into combat_aux_block[0xD] for
 * chars in range [start_idx, end_idx] inclusive.
 * ---------------------------------------------------------------- */
void fd2_set_combat_aux_block_byte_d_low4_for_char_range(
    uint32 start_idx, uint32 end_idx, uint32 new_val)
{
    uint32 i;
    uint8 *pChar;

    for (i = start_idx; (int)i <= (int)end_idx; i++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        pChar[0x34] = (pChar[0x34] & 0xF0) | (uint8)new_val;
    }
}

/* ----------------------------------------------------------------
 * fd2_check_battle_end_condition @ 0x205BE
 *
 * Default win/lose check: flag=2 (victory) if all enemies dead,
 * flag=1 (game over) if protagonist dead, flag=0 (continue) otherwise.
 * ---------------------------------------------------------------- */
void fd2_check_battle_end_condition(void)
{
    int i;
    uint8 *pChar;

    data_fd2_chapter_event_or_battle_end_code = 2;
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        if (pChar[6] == TEAM_ENEMY &&
            (pChar[5] & CHARFLAG_DEAD) == 0) {
            data_fd2_chapter_event_or_battle_end_code = 0;
        }
    }
    pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr;
    if (pChar[5] & CHARFLAG_DEAD) {
        data_fd2_chapter_event_or_battle_end_code = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_check_battle_end_default_handler @ 0x205B4
 *
 * Default post_action_handler entry. Falls through to
 * fd2_check_battle_end_condition.
 * ---------------------------------------------------------------- */
void fd2_check_battle_end_default_handler(uint32 event_arg)
{
    fd2_check_battle_end_condition();
}

/* ----------------------------------------------------------------
 * fd2_collect_dead_char_drops @ 0x1B653
 *
 * Collect 3-byte drop entries from chars with HP==0, flags not
 * yet marked dead, combat_aux[10]==3. Returns count.
 * ---------------------------------------------------------------- */
int fd2_collect_dead_char_drops(uint32 out_buffer)
{
    int drop_count;
    int i;
    uint8 *pChar;

    drop_count = 0;
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        if ((pChar[5] & CHARFLAG_DEAD) == 0 &&
            pChar[0x31] == 3 &&
            *(uint16 *)(pChar + 0x40) == 0) {
            memmove((void *)(out_buffer + drop_count * 3),
                    pChar + 0x31, 3);
            drop_count++;
        }
    }
    return drop_count;
}

/* ----------------------------------------------------------------
 * fd2_collect_pending_death_drops @ 0x1B6B7
 *
 * Same as collect_dead_char_drops but accepts any type != 0xFF
 * (not just type==3).
 * ---------------------------------------------------------------- */
int fd2_collect_pending_death_drops(uint32 out_buffer)
{
    int drop_count;
    int i;
    uint8 *pChar;

    drop_count = 0;
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pChar = (uint8 *)data_fd2_battle_runtime_char_array_ptr
              + i * RUNTIME_CHAR_SIZE;
        if ((pChar[5] & CHARFLAG_DEAD) == 0 &&
            pChar[0x31] != 0xFF &&
            *(uint16 *)(pChar + 0x40) == 0) {
            memmove((void *)(out_buffer + drop_count * 3),
                    pChar + 0x31, 3);
            drop_count++;
        }
    }
    return drop_count;
}
