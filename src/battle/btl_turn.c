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
 * End-of-turn status effect ticker for one team. Called from
 * fd2_run_full_turn_cycle at three points: team==1 (end of player
 * turn), team==0 (enemy turn intro), team==2 (new player turn).
 * Only alive chars whose bTeam == team are processed.
 *
 * Pass 1 -- poison: if status byte at +0x25 is non-zero, deal
 *   damage = HP_max/10, clamp HP to >=0, stash damage in the dialog
 *   value placeholder (text 0x1E7 "poisoned for N HP"), pan/show/wait.
 * Between passes: fd2_play_death_animation_and_mark_dead() handles
 *   anyone the poison just killed, then the per-chapter post-action
 *   hook for the current chapter runs.
 * Pass 2 -- timer countdown: status bytes at +0x22..+0x27 (6 slots,
 *   poison's own slot +0x25 included). Each non-zero slot decrements;
 *   on reaching 0 show removal dialog (text 0x1E1+slot) and
 *   fd2_recalculate_combat_stats() to drop the expired modifier.
 *
 * Note: HP_max/10 is an unsigned widen of a uint16, so the division
 * is non-negative -- equivalent to the original signed IDIV.
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
            fd2_dialog_open_speaker_portrait((uint32)pChar[7]);
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
                        fd2_dialog_open_speaker_portrait(
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
 * Locate alive battle char whose char_id (runtime_char +8) ==
 * target_char_id; return its slot index, else -1.
 * Fallback: only when NO battle slot matched the id at all, scan
 * the menu party roster (stride 0x50) for a template ptr (used for
 * dialog portrait rendering). A dead battle match still caches its
 * runtime_char* and suppresses the fallback (returns -1).
 * Side-effect: always sets data_fd2_dialog_current_speaker_char_ptr
 * (NULL on no match, runtime_char* on battle id-hit, or roster
 * template* on the fallback path).
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
 * Set runtime_char[char_idx].flags bit 0x80 (CHARFLAG_ACTED), marking
 * the char as having taken its action this turn. The bit is consumed by
 * the sprite painter (renders the char dimmed) and by the turn/AI
 * dispatcher loops (skip an already-acted char).
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
 * After a character lands on tile (world_x, world_y) via a walk-step
 * or an action, check whether the tile fires a scripted post-action
 * consequence (e.g. a chapter reinforcement event).
 *
 * Reads the tile attribute (8 bytes). Skips animated event-tiles
 * (tile_buf[4] & 0x60) and tiles with no terrain class. Otherwise
 * indexes tile_event_data_table by (terrain_class - 1) and reads the
 * record's consequence index (+0x33) and event_type (+0x34). When the
 * index is valid (!= 0xFF) and event_type matches the expected one, it
 * latches ai_post_action_consequence_idx, which the next AI phase loop
 * iteration dispatches as a consequence handler.
 *
 * expected_event_type discriminates the trigger source:
 *   0 = walked into the tile (passed by every walk_step_*)
 *   1 = took an action at the tile (player/enemy action handlers).
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
 * For every runtime char i in the inclusive range [start_idx, end_idx],
 * write the low nibble of new_val into combat_aux_block[0xD] (= absolute
 * runtime_char offset 0x34, the per-char AI class / AI-dialog control
 * flag) while preserving its high 4 bits:
 *   combat_aux_block[0xD] = (combat_aux_block[0xD] & 0xF0) | (new_val & 0xF).
 *
 * The body does NOT mask new_val before the OR, so a caller passing a
 * value > 0xF would set high nibble bits too; every caller passes 0..0xF.
 * Used exclusively by chapter event handlers to arm/disarm the AI mode
 * (typically 0/3/7) of an NPC or enemy group for a story beat.
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
 * Default entry of the per-chapter post-action handler table
 * (data_fd2_chapter_post_action_handler_table @ 0x51B19). The 11
 * chapters with no bespoke win/lose rule (ch 1,3,4,5,6,7,8,9,11,14,24)
 * point their slot here; the other slots use chapter-specific handlers.
 *
 * Just runs the standard win/lose check via fd2_check_battle_end_condition.
 * event_arg is the shared dispatch argument (always 0 from the turn-cycle
 * callers); this default handler ignores it.
 *
 * In the original binary this is a distinct symbol that falls through
 * into fd2_check_battle_end_condition @ 0x205BE -- the only machine-code
 * difference is one extra Watcom __CHK(4) stack-frame probe, hence the
 * separate entry point modeled here as a thin wrapper.
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

/* ----------------------------------------------------------------
 * fd2_run_full_turn_cycle @ 0x1A30B (2 callers)
 *
 * END-OF-PLAYER-TURN -> NPC turn -> ENEMY turn -> NEW-PLAYER-TURN full
 * cycle. Triggered by fd2_check_all_player_acted_or_asleep and
 * fd2_field_command_menu_loop.
 *
 *   Phase A  party auto-heal (player team only): mark pass paints a
 *            heal indicator (mode 2, color 0xFD) for every qualifying
 *            char, plays a heal chime if any; the apply pass raises
 *            hp_current by hp_max/5 (clamped to hp_max) and marks each
 *            healed char acted.
 *   Phase B  fire_chapter_turn_events_for_phase(1) + status tick(1).
 *   Phase C  NPC turn (gate: game_event_flag == 0).
 *   Phase D  ENEMY intro banner (gate).
 *   Phase E  ENEMY turn (gate).
 *   Phase F  NEW PLAYER TURN: bump turn counter, banner, 9-step + 4-step
 *            "TURN N" reveal, fire phase-2 events, re-arm cursor (gate).
 *
 * Any inter-phase gate failing (game_event_flag != 0) tail-jumps to the
 * shared epilogue 0x10B46 (== plain return here).
 *
 * Heal qualifier (both passes): team==2, (flags & 0x81)==0,
 * status_flags_block[4]==0, status_paralysis_flag==0, hp_current != hp_max.
 *
 * NOTE (Ghidra EAX-tracking bug): in both reveal loops the decompiler
 * rendered fd2_cleanup_dialog_sprite_buffer's first arg as the sprite
 * index / destination buffer. The assembly (MOV EBX,EAX after each
 * fd2_alloc_and_blit_indexed_sprite_chunk, then PUSH EBX into the
 * cleanup call) shows the real first arg is the malloc'd save buffer
 * returned by fd2_alloc_and_blit_indexed_sprite_chunk. Encoded as such.
 * ---------------------------------------------------------------- */
void fd2_run_full_turn_cycle(void)
{
    int i;
    runtime_char *pc;
    uint32 hp_max;
    uint32 hp_after;
    int any_to_heal;
    int step;
    uint32 sprite_idx;
    uint32 save_buf;
    uint32 reveal_buf;

    any_to_heal = 0;
    fd2_wait_n_bios_ticks(1);

    /* Phase A pass 1: paint heal indicator for qualifying chars. */
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pc = &data_fd2_battle_runtime_char_array_ptr[i];
        hp_max = (uint32)pc->hp_max;
        if (pc->team == 2 &&
            (pc->flags & 0x81) == 0 &&
            pc->status_flags_block[4] == 0 &&
            pc->status_paralysis_flag == 0 &&
            (uint32)pc->hp_current != hp_max) {
            fd2_paint_char_sprite_at_world_with_mode(
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, (uint32)i, 2, 0xFD);
            any_to_heal = 1;
        }
    }
    fd2_blit_rectangle(0xA0504, 0x140,
                       data_fd2_large_game_state_buffer_ptr + 0x8088,
                       0x1C8, 0x138, 0xC0);
    if (any_to_heal) {
        fd2_play_sfx_with_handle(
            data_fd2_audio_fdother_sfx_bank_buf_ptr, 4, 1);
    }
    fd2_wait_n_bios_ticks(0);

    /* Phase A pass 2: apply hp_max/5 heal (clamped), mark acted. */
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pc = &data_fd2_battle_runtime_char_array_ptr[i];
        hp_max = (uint32)pc->hp_max;
        if (pc->team == 2 &&
            (pc->flags & 0x81) == 0 &&
            pc->status_flags_block[4] == 0 &&
            pc->status_paralysis_flag == 0 &&
            (uint32)pc->hp_current != hp_max) {
            hp_after = (uint32)pc->hp_current + hp_max / 5;
            if (hp_max < hp_after) {
                hp_after = hp_max;
            }
            pc->hp_current = (uint16)hp_after;
            fd2_paint_char_sprite_at_world_with_mode(
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, (uint32)i, 0, 0);
            fd2_mark_char_acted_this_turn((uint32)i);
        }
    }
    fd2_blit_rectangle(0xA0504, 0x140,
                       data_fd2_large_game_state_buffer_ptr + 0x8088,
                       0x1C8, 0x138, 0xC0);
    fd2_wait_n_bios_ticks(0);
    fd2_composite_battle_frame(0);

    /* Phase B: end-of-player-turn chapter events + status tick. */
    fd2_fire_chapter_turn_events_for_phase(1);
    fd2_tick_status_effects_and_show_messages(1);
    if (data_fd2_chapter_event_or_battle_end_code != 0) {
        return;
    }

    /* Phase C: NPC (team 1) turn. */
    fd2_maybe_load_speed_mode_sfx_bank();
    fd2_npc_turn_phase_team1();
    fd2_maybe_free_speed_mode_sfx_bank();
    if (data_fd2_chapter_event_or_battle_end_code != 0) {
        return;
    }

    /* Phase D: ENEMY intro banner. */
    if (data_fd2_audio_per_chapter_player_turn_bgm_track
            [data_fd2_chapter_current_chapter_id] !=
        data_fd2_audio_per_chapter_enemy_turn_bgm_track
            [data_fd2_chapter_current_chapter_id]) {
        fd2_set_bgm_track_with_fade(0xFFFFFFFF, 0);
    }
    fd2_animate_phase_banner_slide_in(0x52);
    fd2_delay_ms(0x14);
    fd2_animate_phase_banner_slide_out(0x52);
    fd2_clear_all_chars_acted_flag();
    fd2_composite_battle_frame(0);
    fd2_fire_chapter_turn_events_for_phase(0);
    fd2_tick_status_effects_and_show_messages(0);
    if (data_fd2_chapter_event_or_battle_end_code != 0) {
        return;
    }

    /* Phase E: ENEMY (team 0) turn. */
    fd2_set_bgm_track_with_fade(
        (uint32)data_fd2_audio_per_chapter_enemy_turn_bgm_track
            [data_fd2_chapter_current_chapter_id], 0);
    fd2_maybe_load_speed_mode_sfx_bank();
    fd2_enemy_turn_phase_team0();
    fd2_maybe_free_speed_mode_sfx_bank();
    if (data_fd2_chapter_event_or_battle_end_code != 0) {
        return;
    }

    /* Phase F: NEW PLAYER TURN. */
    data_fd2_battle_turn_counter = data_fd2_battle_turn_counter + 1;
    if (data_fd2_audio_per_chapter_player_turn_bgm_track
            [data_fd2_chapter_current_chapter_id] !=
        data_fd2_audio_per_chapter_enemy_turn_bgm_track
            [data_fd2_chapter_current_chapter_id]) {
        fd2_set_bgm_track_with_fade(0xFFFFFFFF, 0);
    }
    fd2_animate_phase_banner_slide_in(0x50);
    fd2_delay_ms(0x96);
    fd2_animate_phase_banner_slide_out(0x50);
    data_fd2_battle_anim_phase = 0;
    fd2_clear_all_chars_acted_flag();
    fd2_composite_battle_frame(0);
    fd2_set_bgm_track_with_fade(
        (uint32)data_fd2_audio_per_chapter_player_turn_bgm_track
            [data_fd2_chapter_current_chapter_id], 0);

    /* 9-step "TURN N" reveal into the 0xA0000 VGA aperture. The
     * cleanup at the head of each iteration releases the save buffer
     * allocated in the same pass (held in save_buf across the body). */
    for (step = 0; step < 9; step++) {
        sprite_idx = (uint32)(step + 0x53);
        save_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr, 0xA0000, 0x140,
            0x78, 0x54, sprite_idx);
        if (step > 6) {
            fd2_render_decimal_number_to_buffer(
                0xA726B, 0x140, data_fd2_battle_turn_counter, 0x2A, 3);
        }
        fd2_delay_ms(0x46);
        if (step == 8) {
            fd2_delay_ms(500);
        }
        fd2_cleanup_dialog_sprite_buffer(save_buf, 0xA0000, 0x140);
    }

    /* 4-step zoom reveal (step = 2,3,4 then jumps to 9) into the
     * large_game_state_buffer + 0x8088 surface (stride 0x1C8). */
    for (step = 2; step < 6; step++) {
        if (step == 5) {
            step = 9;
        }
        reveal_buf = data_fd2_large_game_state_buffer_ptr + 0x8088;
        save_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr, reveal_buf, 0x1C8,
            0x74, (uint32)(step * step + 0x54), 0x5B);
        fd2_render_decimal_number_to_buffer(
            data_fd2_large_game_state_buffer_ptr + 0x812F +
                (uint32)(step * step + 0x5A) * 0x1C8,
            0x1C8, data_fd2_battle_turn_counter, 0x2A, 3);
        fd2_blit_rectangle(0xA0504, 0x140, reveal_buf,
                           0x1C8, 0x138, 0xC0);
        fd2_wait_n_bios_ticks(1);
        fd2_cleanup_dialog_sprite_buffer(save_buf, reveal_buf, 0x1C8);
    }

    fd2_composite_battle_frame(0);
    fd2_delay_ms(200);
    data_fd2_battle_current_active_char_idx = 0;
    fd2_fire_chapter_turn_events_for_phase(2);
    fd2_tick_status_effects_and_show_messages(2);
    data_fd2_battle_anim_phase = 1;
    fd2_pan_cursor_to_char(0);
    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * fd2_fire_chapter_turn_events_for_phase @ 0x1A813 (1 caller)
 *
 * Fire the chapter turn-event scripts that match the current turn
 * counter and the requested phase.
 *
 * Scans the 16 chapter turn-event entries inside the table pointed to
 * by data_fd2_tile_event_data_table_ptr (0x53A55). Entries begin at
 * byte offset +3 with a 3-byte stride:
 *   entry[i].turn     @ base + 3 + i*3   (trigger turn)
 *   entry[i].event_id @ base + 4 + i*3   (handler-table index)
 *   entry[i].phase    @ base + 5 + i*3   (0/1/2)
 *
 * For each entry whose turn == data_fd2_battle_turn_counter and whose
 * phase == the phase argument, calls the chapter-event handler
 * data_fd2_battle_ai_post_action_consequence_table[event_id] with arg 0
 * (the same handler table shared with the tile "event" cells in
 * fd2_handle_tile_event_interaction).
 *
 * phase semantics: 0 = enemy-turn start, 1 = end-of-player-turn,
 * 2 = start-of-new-player-turn.
 * ---------------------------------------------------------------- */
void fd2_fire_chapter_turn_events_for_phase(uint32 phase)
{
    int i;
    uint8 *entry;

    for (i = 0; i < 0x10; i++) {
        entry = (uint8 *)data_fd2_tile_event_data_table_ptr + i * 3;
        if ((uint32)entry[3] == data_fd2_battle_turn_counter &&
            (uint32)entry[5] == phase) {
            data_fd2_battle_ai_post_action_consequence_table[entry[4]](0);
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_process_battle_drop_entries @ 0x1AA1D (9 callers)
 *
 * Battle kill-drop processing. Called after every kill by the
 * attack/spell/use paths to apply the 3-byte drop entries that
 * fd2_collect_dead_char_drops / fd2_collect_pending_death_drops
 * accumulated, to the recipient (typically the killer).
 *
 * Each entry: byte type + ushort value.
 *   type 0 ITEM:  gate team==2; last_action_sprite_id = value+0xB5,
 *                 dialog 0x1B0 "got X", add_item_to_inventory; on
 *                 -1 (bag full) run the discard/swap flow.
 *   type 1 GOLD:  gate team==2; dialog 0x1B3; party_total_gold += value.
 *   type 2 EVENT: delay 200 then call chapter event handler
 *                 table[value](recipient_idx). (no team gate)
 *   type 3 DIALOG: display scripted FDTXT dialog page `value`.
 *   other:        skip entry.
 *
 * The team!=2 gate on type 0/1 returns immediately (no remaining
 * entries processed); each handled entry falls through to the loop
 * increment. count==0 / non-player-team / loop-done all reach the
 * shared epilogue at 0x22BBE (== plain return here).
 *
 * NOTE (Ghidra EAX/arg-tracking bug): the decompiler rendered the
 * type-2 handler-table call as 0-arg. The assembly (PUSH EDI before
 * CALL [EAX*4+0x51B91], ADD ESP,4) shows it passes recipient_idx.
 * Encoded as such.
 * ---------------------------------------------------------------- */
void fd2_process_battle_drop_entries(uint32 recipient_idx,
                                     uint32 entry_count,
                                     uint32 entry_array_ptr)
{
    runtime_char *pCharArray;
    uint32 entry_iter;
    uint8 *pEntry;
    uint8 entry_type;
    uint32 entry_value;
    int add_result;
    int typewriter_result;
    uint32 swap_dialog_text;

    pCharArray = data_fd2_battle_runtime_char_array_ptr;
    if (entry_count == 0) {
        return;
    }

    for (entry_iter = 0; (int)entry_iter < (int)entry_count;
         entry_iter++) {
        fd2_clear_keyboard_buffer();
        pEntry = (uint8 *)entry_array_ptr + entry_iter * 3;
        entry_value = (uint32)*(uint16 *)(pEntry + 1);
        entry_type = pEntry[0];

        if (entry_type == 0) {
            if (pCharArray[recipient_idx].team != 2) {
                return;
            }
            data_fd2_dialog_last_action_text_id_param =
                entry_value + 0xB5;
            fd2_dialog_open_speaker_portrait(
                (uint32)pCharArray[recipient_idx].portrait_id);
            fd2_display_dialog_scene(
                data_fd2_all_game_text_ptr, 0x1B0, 0xA9F23,
                0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
            add_result = fd2_add_item_to_inventory(
                recipient_idx, entry_value);
            if (add_result == -1) {
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
                fd2_close_status_screen_with_slide_out();
                fd2_delay_ms(100);
                fd2_dialog_open_speaker_portrait(
                    (uint32)pCharArray[recipient_idx].portrait_id);
                fd2_display_dialog_scene(
                    data_fd2_all_game_text_ptr, 0x1B1, 0xA9F23,
                    0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
                fd2_paint_portrait_to_dialog_area(0);
                typewriter_result = fd2_text_dialog_typewriter_loop();
                fd2_animate_dialog_page_advance_collapse();
                if (typewriter_result == 1 &&
                    data_fd2_ui_menu_cursor_idx == 0) {
                    fd2_close_status_screen_with_slide_out();
                    if (fd2_inventory_selection_modal_dispatch(
                            recipient_idx, 0) != 0) {
                        fd2_get_inventory_slot_item_id(
                            recipient_idx,
                            data_fd2_ui_menu_cursor_idx);
                        fd2_remove_inventory_slot_at(
                            recipient_idx,
                            data_fd2_ui_menu_cursor_idx);
                        fd2_add_item_to_inventory(
                            recipient_idx, entry_value);
                        continue;
                    }
                    fd2_delay_ms(100);
                    fd2_dialog_open_speaker_portrait(
                        (uint32)pCharArray[recipient_idx].portrait_id);
                    swap_dialog_text = 0xA9F23;
                } else {
                    swap_dialog_text = 0xAB6E3;
                }
                fd2_display_dialog_scene(
                    data_fd2_all_game_text_ptr, 0x1B2,
                    swap_dialog_text, 0x140, 0xCD, 0x4C,
                    0x4A, 0x13, 1);
                fd2_delay_ms(200);
            } else {
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
            }
            fd2_close_status_screen_with_slide_out();
        } else if (entry_type == 1) {
            if (pCharArray[recipient_idx].team != 2) {
                return;
            }
            fd2_dialog_open_speaker_portrait(
                (uint32)pCharArray[recipient_idx].portrait_id);
            data_fd2_dialog_last_action_value_param = entry_value;
            fd2_display_dialog_scene(
                data_fd2_all_game_text_ptr, 0x1B3, 0xA9F23,
                0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(0);
            fd2_close_status_screen_with_slide_out();
            data_fd2_shared_party_total_gold +=
                data_fd2_dialog_last_action_value_param;
        } else if (entry_type == 2) {
            fd2_delay_ms(200);
            data_fd2_battle_ai_post_action_consequence_table
                [entry_value](recipient_idx);
        } else if (entry_type == 3) {
            fd2_display_dialog_scene(
                data_fd2_current_chapter_text_ptr, entry_value, 0xA0000,
                0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_count_active_chars_for_team_filter @ 0x1B5F1 (1 caller)
 *
 * Count "actively usable" chars on the given team: matching team,
 * portrait_id != 0x79 (exclude hidden/quest portrait),
 * archetype_flag != 10 (exclude boss / special unit), and alive.
 * Used by fd2_render_party_status_overview_content to show each
 * team's surviving headcount (team 0/1/2).
 * ---------------------------------------------------------------- */
int fd2_count_active_chars_for_team_filter(uint32 team)
{
    int count;
    int i;
    runtime_char *pc;

    count = 0;
    for (i = 0; i < (int)data_fd2_battle_party_member_count; i++) {
        pc = &data_fd2_battle_runtime_char_array_ptr[i];
        if (pc->team == team &&
            pc->portrait_id != 0x79 &&
            pc->archetype_flag != 10) {
            if (!fd2_check_char_is_dead((uint32)i)) {
                count++;
            }
        }
    }
    return count;
}

/* ----------------------------------------------------------------
 * fd2_roll_stat_gain_and_show_message @ 0x1E529 (2 callers)
 *
 * Level-up / promotion single-stat gain roll + on-screen message.
 * Callers: fd2_process_xp_and_level_up_for_char (x5 stat slots),
 * fd2_execute_class_promotion_with_dialog (x5 promotion bonuses).
 *
 * growth_pair is a 2-byte char_growth_entry stat field: byte[0] = min
 * gain, byte[1] = max gain + 1. So the rolled gain spans [min, max]:
 *   min_gain   = growth_pair[0]
 *   range      = growth_pair[1] - growth_pair[0]   (= max+1 - min)
 *   rand_extra = (range != 0) ? fd2_advance_rng_state() % range : 0
 *   gain (data_fd2_dialog_last_action_value_param) = min_gain + rand_extra
 *
 * Only when gain != 0 is the message shown and the gain applied:
 *   - row_idx 3 is the 4th (bottom) row; scroll up one and use row 2.
 *   - render the FDTXT page dialog_text_id at row_idx * 0x17C0 + 0xA951F.
 *   - *(int16 *)stat_ptr += gain (low 16 bits).
 *   - row_idx++ (advance to the next message row).
 * Returns the next row index (unchanged when gain == 0).
 *
 * NOTE (Ghidra EAX-tracking bug): the decompiler dropped the
 * fd2_advance_rng_state() return value and rendered the modulo dividend
 * as growth_pair. The assembly (CALL 0x4E893 then MOV EDX,EAX; SAR;
 * IDIV ESI) shows the dividend is the RNG result. Encoded as such.
 * ---------------------------------------------------------------- */
int fd2_roll_stat_gain_and_show_message(short *stat_ptr, uint8 *growth_pair,
                                        uint32 dialog_text_id, int row_idx)
{
    int min_gain;
    int range;
    int rand_extra;
    int gain;

    min_gain = (int)growth_pair[0];
    range = (int)growth_pair[1] - min_gain;
    rand_extra = 0;
    if (range != 0) {
        rand_extra = (int)fd2_advance_rng_state() % range;
    }
    gain = min_gain + rand_extra;
    data_fd2_dialog_last_action_value_param = (uint32)gain;

    if (gain != 0) {
        if (row_idx == 3) {
            row_idx = 2;
            fd2_scroll_portrait_dialog_text_up_one_line();
        }
        fd2_clear_keyboard_buffer();
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr, dialog_text_id,
            (uint32)row_idx * 0x17C0 + 0xA951F, 0x140, 0xCD,
            0x4C, 0x4A, 0x13, 1);
        *(int16 *)stat_ptr = (int16)(*(int16 *)stat_ptr +
            (int16)data_fd2_dialog_last_action_value_param);
        row_idx++;
    }
    return row_idx;
}

/* ----------------------------------------------------------------
 * fd2_process_xp_and_level_up_for_char @ 0x1E292 (2 callers)
 *
 * Apply accumulated XP, run level-up animation and spell learning for
 * one unit. Callers: fd2_game_main_loop, fd2_execute_ai_physical_attack.
 *
 * Gate (any one skips): pending_xp_credit == 0, flags bit0 (dead), or
 * already at level cap (portrait 0x1E/0x1F hero -> 99; others -> 0x28).
 *
 * remaining_xp = pending_xp_credit + carry-over exp_carry.
 * Per level-up: level++, roll 5 stat slots, learn spells whose required
 * level matches, recalc stats, subtract 100. A per-call cap forces an
 * early exit at level 30 (normal) or 99 (hero), discarding leftover XP.
 * On exit exp_carry keeps the (possibly zeroed) remainder and
 * pending_xp_credit is cleared.
 * ---------------------------------------------------------------- */
void fd2_process_xp_and_level_up_for_char(uint32 char_idx)
{
    runtime_char *pCharArray;
    uint8 *pGrowth;
    uint8 *pSpellLearn;
    uint8 portrait_id;
    int remaining_xp;
    int row;
    uint32 spell_pair_iter;
    uint32 spell_id;
    uint8 at_level_cap;

    pCharArray = data_fd2_battle_runtime_char_array_ptr;
    row = 2;
    portrait_id = pCharArray[char_idx].portrait_id;

    if (data_fd2_battle_pending_xp_credit == 0 ||
        (pCharArray[char_idx].flags & 1) != 0) {
        return;
    }

    if (portrait_id == 0x1E || portrait_id == 0x1F) {
        at_level_cap = (pCharArray[char_idx].status_flags_block[0] == 99);
    } else {
        at_level_cap = (pCharArray[char_idx].status_flags_block[0] == 0x28);
    }
    if (at_level_cap) {
        return;
    }

    pGrowth = fd2_get_char_growth_entry((int)pCharArray[char_idx].portrait_id);
    remaining_xp = (int)(data_fd2_battle_pending_xp_credit
                       + (uint32)pCharArray[char_idx].exp_carry);
    data_fd2_dialog_last_action_value_param = data_fd2_battle_pending_xp_credit;
    fd2_clear_keyboard_buffer();
    fd2_dialog_open_speaker_portrait((uint32)pCharArray[char_idx].portrait_id);
    fd2_display_dialog_scene(
        data_fd2_all_game_text_ptr, 0x1E8, 0xA951F,
        0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);

    while (remaining_xp > 99) {
        fd2_clear_keyboard_buffer();
        pCharArray[char_idx].status_flags_block[0] =
            (uint8)(pCharArray[char_idx].status_flags_block[0] + 1);
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr, 0x1E9, 0xAACDF,
            0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        row = fd2_roll_stat_gain_and_show_message(
            (short *)(pCharArray[char_idx].combat_aux_block + 0x10), pGrowth, 0x1EA, row);
        row = fd2_roll_stat_gain_and_show_message(
            (short *)(pCharArray[char_idx].combat_aux_block + 0x12), pGrowth + 2, 0x1EB, row);
        row = fd2_roll_stat_gain_and_show_message(
            (short *)(pCharArray[char_idx].ai_target_and_dx_block + 1), pGrowth + 4, 0x1EC, row);
        row = fd2_roll_stat_gain_and_show_message(
            (short *)&pCharArray[char_idx].hp_max, pGrowth + 6, 0x1ED, row);
        row = fd2_roll_stat_gain_and_show_message(
            (short *)&pCharArray[char_idx].mp_max, pGrowth + 8, 0x1EE, row);

        if (pGrowth[10] != 0xFF) {
            pSpellLearn = fd2_get_spell_learning_entry((int)pGrowth[10]);
            for (spell_pair_iter = 0; (int)spell_pair_iter < 6;
                 spell_pair_iter++) {
                if ((uint32)pCharArray[char_idx].status_flags_block[0] ==
                    pSpellLearn[spell_pair_iter * 2]) {
                    spell_id = pSpellLearn[spell_pair_iter * 2 + 1];
                    data_fd2_dialog_last_action_text_id_param =
                        spell_id + 0x1B9;
                    fd2_grant_spell_to_char(char_idx, spell_id);
                    fd2_display_dialog_scene(
                        data_fd2_all_game_text_ptr, 0x24B,
                        (uint32)row * 0x17C0 + 0xA951F,
                        0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
                }
            }
        }

        fd2_recalculate_combat_stats(char_idx);
        remaining_xp = remaining_xp - 100;
        if (((portrait_id == 0x1E || portrait_id == 0x1F) &&
             pCharArray[char_idx].status_flags_block[0] == 99) ||
            pCharArray[char_idx].status_flags_block[0] == 0x1E) {
            remaining_xp = 0;
        }
    }

    fd2_wait_ticks_or_keypress_with_palette(0xB);
    fd2_close_status_screen_with_slide_out();
    pCharArray[char_idx].exp_carry = (uint8)remaining_xp;
    data_fd2_battle_pending_xp_credit = 0;
}

/* ----------------------------------------------------------------
 * fd2_kill_runtime_chars_from_index_to_end @ 0x35BBA (4 callers)
 *
 * Sets hp_current = 0 for every runtime_char_array entry from
 * start_char_idx (inclusive) to party_member_count-1, then plays the
 * death animation once.
 *
 * Usage: game-over / story-event mass kill of trailing party slots
 * (e.g. wiping enemy reinforcement squads at chapter transitions).
 * Callers: fd2_chapter_29_end @ 0x2548C,
 *   fd2_chapter_event_handler_35__unref_dialog_with_state @ 0x35321,
 *   fd2_chapter_event_handler_40__unref_dyn_turn_event @ 0x358EA,
 *   fd2_chapter_event_handler_47__unref_dyn_turn_event @ 0x35B6B.
 * ---------------------------------------------------------------- */
void fd2_kill_runtime_chars_from_index_to_end(uint32 start_char_idx)
{
    uint32 i;

    for (i = start_char_idx;
         (int)i < (int)data_fd2_battle_party_member_count; i++) {
        data_fd2_battle_runtime_char_array_ptr[i].hp_current = 0;
    }
    fd2_play_death_animation_and_mark_dead();
}

/* ----------------------------------------------------------------
 * data_fd2_dialog_last_action_value_param @ 0x53AE1 (.object2, 4 bytes)
 *
 * Transient 32-bit staging value passed to the dialog VM. Written just
 * before a dialog that shows a number (gold amount, poison damage, XP
 * gained, stat-gain delta, shop price, promote cost, save-slot index),
 * then read by fd2_display_dialog_scene -6 LITERAL-NUMBER opcode via
 * sprintf("%d", ...) and by gold/price arithmetic. Always written
 * before first read on every path, so the binary stores it zero-init.
 *
 * Home owner: btl_turn.c. Also written from gfx/rndmenu.c,
 * ui_menu/menufld.c, ui_menu/promote.c, ui_menu/shop.c (multi-writer).
 * ---------------------------------------------------------------- */
uint32 data_fd2_dialog_last_action_value_param = 0;

/* ----------------------------------------------------------------
 * data_fd2_dialog_current_speaker_char_ptr @ 0x53C1B (.object2, 4 bytes)
 *
 * Cached pointer to the dialog speaker's character record, used to fetch
 * the portrait/name when the dialog VM loads an ally sprite. Written by
 * fd2_find_char_by_id_or_template: cleared to NULL at entry, then set to
 * either a runtime_char* (alive/dead battle slot whose bChar_id matched)
 * or a menu-roster template* (battle miss, found in the menu party).
 * Read by fd2_display_dialog_scene in the -0x11/-0x12 sprite-load opcodes
 * (only when the opcode's char_id != 0x27), right after it calls
 * fd2_find_char_by_id_or_template: dereferenced as runtime_char* to read
 * ->bPortrait_id (+7) and ->bPos_x/y (+0/+1) for the speaker portrait.
 * Always cleared before first use each call, so the binary stores it
 * zero-init (NULL). Home owner: btl_turn.c (alongside the writer).
 * ---------------------------------------------------------------- */
uint32 data_fd2_dialog_current_speaker_char_ptr = 0;
