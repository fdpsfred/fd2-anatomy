/*
 * status.c — Status screen and item stat preview helpers
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_compute_equipped_stats_with_item_preview @ 0x2EFB7  (1 caller)
 *
 * Compute stats as if candidate item_id were equipped, replacing
 * the currently equipped item of the same category (weapon/armor).
 * Bonuses from the OPPOSITE category's equipped items are preserved.
 * Output: 4 int32 at stats_out_ptr: [AP, DP, DX, Stat4].
 * ---------------------------------------------------------------- */
void fd2_compute_equipped_stats_with_item_preview(uint32 char_idx,
                                                   uint32 item_id,
                                                   uint32 stats_out_ptr)
{
    runtime_char *rc;
    uint8 *item_entry;
    uint8 *cur_entry;
    uint32 preview_cat;
    int ap;
    int dp;
    int dx;
    int stat4;
    uint32 slot_iter;

    item_entry = fd2_get_item_effect_entry(item_id);
    preview_cat = (uint32)item_entry[0];

    rc = data_fd2_battle_runtime_char_array_ptr;
    ap = (int)*(int16 *)(rc[char_idx].combat_aux_block + 0x10)
       + (int)*(int16 *)(item_entry + 1);
    dp = (int)*(int16 *)(rc[char_idx].combat_aux_block + 0x12)
       + (int)*(int16 *)(item_entry + 5);
    dx = (int)*(int16 *)(rc[char_idx].ai_target_and_dx_block + 1)
       + (int)*(int16 *)(item_entry + 3);
    stat4 = (int)*(int16 *)(rc[char_idx].ai_target_and_dx_block + 1)
          + (int)*(int16 *)(item_entry + 7);

    for (slot_iter = 0; (int)slot_iter < 8; slot_iter++) {
        cur_entry = fd2_get_item_effect_entry(
            (uint32)rc[char_idx].inventory_slots[slot_iter * 2 + 1]);
        if ((rc[char_idx].inventory_slots[slot_iter * 2] & 0x40) != 0
            && ((preview_cat <= 0x14
                 && (uint32)cur_entry[0] > 0x14)
                || (preview_cat > 0x14
                    && (uint32)cur_entry[0] <= 0x14))) {
            ap += (int)*(int16 *)(cur_entry + 1);
            dp += (int)*(int16 *)(cur_entry + 5);
            dx += (int)*(int16 *)(cur_entry + 3);
            stat4 += (int)*(int16 *)(cur_entry + 7);
        }
    }

    *(int32 *)stats_out_ptr = (int32)ap;
    *(int32 *)(stats_out_ptr + 4) = (int32)dp;
    *(int32 *)(stats_out_ptr + 8) = (int32)dx;
    *(int32 *)(stats_out_ptr + 0xC) = (int32)stat4;
}

/* ----------------------------------------------------------------
 * fd2_check_job_can_equip_item @ 0x1C1C3  (2 callers)
 *
 * Job/equipment compatibility check. Returns 1 if char_idx's job
 * permits equipping item_id, else 0.
 *
 *   allowed_types = fd2_get_job_allowed_items_table_entry(
 *                       runtime_char[char_idx].job_id)   // 7-byte list
 *   item_category = fd2_get_item_effect_entry(item_id)[0]  // entry +0
 *   for i in 0..6: if item_category == allowed_types[i] return 1
 *   return 0
 *
 * Only the first 6 of the 7-byte allowed-items list are scanned
 * (loop bound is 6, matching asm CMP EAX,6 / JGE).
 *
 * Callers: fd2_equip_unequip_inventory_menu (modal confirm),
 * fd2_run_buy_item_menu (pre-purchase check).
 * int __cdecl with the __CHK(0x10) stack-probe prologue.
 * ---------------------------------------------------------------- */
int fd2_check_job_can_equip_item(uint32 char_idx, uint32 item_id)
{
    uint8 *allowed_types;
    uint8 item_category;
    uint32 type_iter;

    allowed_types = fd2_get_job_allowed_items_table_entry(
        (int)data_fd2_battle_runtime_char_array_ptr[char_idx].job_id);
    item_category = fd2_get_item_effect_entry(item_id)[0];

    for (type_iter = 0; (int)type_iter < 6; type_iter++) {
        if (item_category == allowed_types[type_iter]) {
            return 1;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_count_usable_inventory_slots @ 0x1B8A6  (9 callers)
 *
 * Count how many of the 8 inventory slots in runtime_char[ci] are
 * currently active: a slot is active when its flag byte
 * (inventory_slots[slot*2]) has bit 0x80 clear. Returns the count.
 *
 * Used by AI (fd2_ai_score_item_use slot-iteration cap), the menu/UI
 * item-slot gating (item/buy/give/equip dispatch), the chapter-08/3a
 * pickup events, and fd2_find_inventory_slot_with_item.
 *
 * int __cdecl with the __CHK(8) stack-probe prologue (compiler-injected,
 * not part of the source). EBX is the accumulator (callee-saved); the
 * trailing POP EBX + RET is the shared epilogue.
 * ---------------------------------------------------------------- */
int fd2_count_usable_inventory_slots(uint32 ci)
{
    runtime_char *rc;
    uint32 slot_iter;
    int active_count;

    rc = data_fd2_battle_runtime_char_array_ptr;
    active_count = 0;
    for (slot_iter = 0; (int)slot_iter < 8; slot_iter++) {
        if ((rc[ci].inventory_slots[slot_iter * 2] & 0x80) == 0) {
            active_count++;
        }
    }
    return active_count;
}

/* ----------------------------------------------------------------
 * fd2_add_item_to_inventory @ 0x1BB8C  (9 callers)
 *
 * Add item_id to the first empty inventory slot of runtime_char[char_idx].
 * Each slot is 2 bytes (inventory_slots[i*2]=flag, [i*2+1]=item_id); there
 * are 8 slots (indices 0..7). A slot is empty when its flag byte has bit
 * 0x80 set. The first empty slot found is marked occupied-but-unequipped
 * (flag = 0), its item_id byte is set to (uint8)item_id, and 1 is returned.
 * If all 8 slots are full, -1 is returned.
 *
 * Slot flag bits: 0x80 = empty, 0x40 = equipped (mutually exclusive with
 * 0x80), 0 = occupied but not equipped.
 *
 * Callers: tile-event pickup, battle drop, item-command consume/replace,
 * AI enemy-turn pickup, shop buy, give item, chapter event handlers.
 *
 * int __cdecl with the __CHK(8) stack-probe prologue (compiler-injected,
 * not part of the source). EBX is the runtime_char base pointer (callee-
 * saved); the trailing POP EBX + RET is the shared epilogue. Only the low
 * byte of item_id is stored.
 * ---------------------------------------------------------------- */
int fd2_add_item_to_inventory(uint32 char_idx, uint32 item_id)
{
    runtime_char *rc;
    uint32 slot_iter;

    rc = data_fd2_battle_runtime_char_array_ptr;
    for (slot_iter = 0; (int)slot_iter < 8; slot_iter++) {
        if ((rc[char_idx].inventory_slots[slot_iter * 2] & 0x80) != 0) {
            rc[char_idx].inventory_slots[slot_iter * 2] = 0;
            rc[char_idx].inventory_slots[slot_iter * 2 + 1] = (uint8)item_id;
            return 1;
        }
    }
    return -1;
}

/* ----------------------------------------------------------------
 * fd2_open_status_screen_with_slide_in @ 0x17E0B  (3 callers)
 *
 * Open the character status panel with a 12-frame slide-in animation.
 *
 * Allocates three 64000-byte (320x200 mode 13h) workspaces:
 *   render_workspace_a — per-frame interpolated animation accumulator
 *   render_workspace_b — pristine snapshot of the current screen (backdrop)
 *   render_workspace_c — fully-rendered status-panel target image
 *
 * Backs up VRAM (0xA0000) into workspace_b, copies that into workspace_c,
 * then renders the static status layout + inventory grid (item_id -1 = no
 * highlight) into workspace_c. Drives a 12-frame slide-in by calling the
 * shared fd2_play_status_screen_outro_step with the frame index running
 * backwards (0xB down to 0), i.e. the slide-out step reversed produces the
 * entrance; a chime SFX fires at frame 0xB (open) and frame 5 (mid). Finally
 * drains the keyboard buffer.
 *
 * Counterpart: fd2_close_status_screen_with_slide_out.
 *
 * void __cdecl with the __CHK(0x18) stack-probe prologue (EBX is the loop
 * counter). The trailing fd2_clear_keyboard_buffer() + POP EBX + RET form
 * the shared epilogue.
 * ---------------------------------------------------------------- */
void fd2_open_status_screen_with_slide_in(uint32 char_idx)
{
    int frame_iter;

    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);

    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr,
            (void *)0xa0000, 64000);
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);

    fd2_render_status_screen_static_layout(char_idx,
        data_fd2_ui_slide_composed_target_buf_ptr);
    fd2_render_inventory_item_grid(char_idx, -1,
        data_fd2_ui_slide_composed_target_buf_ptr);

    for (frame_iter = 0xb; frame_iter >= 0; frame_iter--) {
        if (frame_iter == 0xb || frame_iter == 5) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                5, 1);
        }
        fd2_play_status_screen_outro_step((uint32)frame_iter,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr,
            (int)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    }

    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * fd2_open_char_status_screen @ 0x17AED  (2 callers)
 *
 * Display character status screen modal. Reached on Space/Enter for the
 * player's own character and on F2/F4 for any character (both via
 * fd2_game_main_loop), and from fd2_run_status_screen_member_menu.
 *
 * Pipeline:
 *   1. fd2_open_status_screen_with_slide_in(char_idx) — populate stat
 *      display (HP/MP/AP/DP/EXP/Level) with slide-in animation.
 *   2. fd2_wait_for_input_dialog_with_blink(0) — wait for ACK.
 *   3. spell_count = fd2_build_usable_spell_list(char_idx, NULL).
 *
 *   If the character has usable spells, play a 7-frame slide-in of the
 *   spell-list panel, render it read-only, then a 7-frame slide-out, with
 *   open/ready SFX bracketing.
 *
 *   The outro (always runs, frames 0..11) slides the status screen away.
 *   SFX fires on frame 0 and frame 7.
 *
 *   Cleanup restores the underlying screen snapshot to VRAM and frees the
 *   three workspace buffers.
 *
 * Globals:
 *   render_workspace_a @ 0x53C5B — working composite (64000B, mode 13h)
 *   render_workspace_b @ 0x53C5F — underlying screen snapshot
 *   render_workspace_c @ 0x53C63 — UI overlay buffer
 *   fdother_resource_buffer @ 0x53EEC — SFX bank
 *   ui_anim_sprite_sheet @ 0x53A81 — sprite sheet base for status panel
 *
 * void __cdecl with the __CHK(0x18) stack-probe prologue. The final
 * free(render_workspace_c) is emitted by Watcom as a tail call (JMP free).
 * ---------------------------------------------------------------- */
void fd2_open_char_status_screen(uint32 char_idx)
{
    uint32 sprite_addr;
    uint32 intro_iter;
    int outro_back_iter;
    uint32 outro_iter;

    fd2_open_status_screen_with_slide_in(char_idx);
    fd2_wait_for_input_dialog_with_blink(0);

    if (fd2_build_usable_spell_list(char_idx, 0) != 0) {
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 6, 1);

        for (intro_iter = 0; (int)intro_iter < 7; intro_iter++) {
            memmove((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr,
                    (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
            fd2_paint_status_panel_layer_left(5,
                data_fd2_ui_slide_anim_accumulator_buf_ptr,
                data_fd2_ui_slide_composed_target_buf_ptr);
            fd2_paint_status_panel_layer_right(7,
                data_fd2_ui_slide_anim_accumulator_buf_ptr,
                data_fd2_ui_slide_composed_target_buf_ptr);
            fd2_slide_panel_up_partial_step(intro_iter * 0x10 + 0x5E,
                data_fd2_ui_slide_anim_accumulator_buf_ptr,
                data_fd2_ui_slide_composed_target_buf_ptr);
            memmove((void *)0xa0000,
                    (void *)data_fd2_ui_slide_anim_accumulator_buf_ptr, 64000);
        }

        memmove((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr,
                (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
        fd2_paint_status_panel_layer_left(5,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr);
        fd2_paint_status_panel_layer_right(7,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr);
        memmove((void *)0xa0000,
                (void *)data_fd2_ui_slide_anim_accumulator_buf_ptr, 64000);

        sprite_addr = data_fd2_ui_anim_sprite_sheet_ptr +
            (uint32)*(int32 *)(data_fd2_ui_anim_sprite_sheet_ptr + 0x5A);
        fd2_dialog_sprite_blit_normal(
            data_fd2_ui_slide_composed_target_buf_ptr + 0x7585,
            sprite_addr, 0x140);
        fd2_draw_spell_selection_list(char_idx, 0xffffffff,
            data_fd2_ui_slide_composed_target_buf_ptr);
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 5, 1);

        for (outro_back_iter = 6; outro_back_iter >= 0; outro_back_iter--) {
            memmove((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr,
                    (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
            fd2_paint_status_panel_layer_left(5,
                data_fd2_ui_slide_anim_accumulator_buf_ptr,
                data_fd2_ui_slide_composed_target_buf_ptr);
            fd2_paint_status_panel_layer_right(7,
                data_fd2_ui_slide_anim_accumulator_buf_ptr,
                data_fd2_ui_slide_composed_target_buf_ptr);
            fd2_slide_panel_up_partial_step(outro_back_iter * 0x10 + 0x5E,
                data_fd2_ui_slide_anim_accumulator_buf_ptr,
                data_fd2_ui_slide_composed_target_buf_ptr);
            memmove((void *)0xa0000,
                    (void *)data_fd2_ui_slide_anim_accumulator_buf_ptr, 64000);
        }

        fd2_wait_for_input_dialog_with_blink(0);
    }

    for (outro_iter = 0; (int)outro_iter < 0xc; outro_iter++) {
        if (outro_iter == 0 || outro_iter == 7) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                6, 1);
        }
        fd2_play_status_screen_outro_step(outro_iter,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr,
            (int)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    }

    memmove((void *)0xa0000,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
    free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    free((void *)data_fd2_ui_slide_composed_target_buf_ptr);
}

/* ----------------------------------------------------------------
 * fd2_close_status_screen_with_slide_out @ 0x196CB  (10 callers)
 *
 * Close the dialog/status panel opened by the status/portrait routines;
 * counterpart of fd2_open_status_screen_with_slide_in.
 *
 * Pipeline:
 *   1. 5-frame slide-down (frame_iter 1..5): each frame drives
 *      fd2_slide_panel_down_step(frame_iter*0xD + 0x70, accumulator, target)
 *      which slides the panel rows out and blits to mode-13h VRAM.
 *   2. memmove(0xA0000, bg_snapshot, 64000) — restore the underlying
 *      screen snapshot to VRAM.
 *   3. free the three 64000-byte workspaces (a / b / c).
 *   4. fd2_composite_battle_frame(0) — recomposite the battle/field scene.
 *
 * Globals (allocated by the open counterpart, freed here):
 *   render_workspace_a @ 0x53C5B — per-frame animation accumulator
 *   render_workspace_b @ 0x53C5F — underlying screen snapshot
 *   render_workspace_c @ 0x53C63 — composed status-panel target image
 *
 * void __cdecl with the __CHK(0x14) stack-probe prologue (compiler-injected,
 * not part of the source). EBX is the loop counter (callee-saved); the
 * trailing POP EBX + RET is the shared epilogue.
 * ---------------------------------------------------------------- */
void fd2_close_status_screen_with_slide_out(void)
{
    uint32 frame_iter;

    for (frame_iter = 1; (int)frame_iter < 6; frame_iter++) {
        fd2_slide_panel_down_step(frame_iter * 0xd + 0x70,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr);
    }

    memmove((void *)0xa0000,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
    free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    free((void *)data_fd2_ui_slide_composed_target_buf_ptr);

    fd2_composite_battle_frame(0);
}

/* ----------------------------------------------------------------
 * fd2_open_party_status_overview_screen @ 0x1B1E7  (1 caller)
 *
 * "Army status" full-screen overview (army menu). Invoked by
 * fd2_field_menu_status_save_load_quit_dispatch when the player picks
 * STATUS (cursor==0). Shows current chapter / turn / gold / per-team
 * alive counts / character roster.
 *
 * Setup: allocate two 64000-byte (320x200 mode-13h) workspaces —
 * snapshot_buf (zeroed) and panel_buf. Recomposite the battle tile map
 * + character overlay into large_game_state_buffer+0x8088, blit that
 * 312x192 region into snapshot_buf+0x504 (snapshot = the 0xA0000 mirror
 * minus the mode-13h header), then render the static overview content
 * (chapter/turn/gold/roster) into panel_buf at stride 320.
 *
 * Intro: 12-frame slide-in (frame 0..11). Each frame restores the
 * backdrop (memmove snapshot_buf -> large_game_state_buffer), advances
 * the right/top/bottom-main/bottom-small panel slide steps, then pushes
 * the composite up to VGA (memmove -> 0xA0000).
 *
 * Wait loop: spin until the keyboard buffer becomes non-empty. While
 * waiting, if the BIOS tick word @ 0x46C differs from last_tick, tick
 * the chapter palette + palette cycle animation, recomposite the battle
 * scene, re-render the overview content into large_game_state_buffer
 * +0x7964 (stride 456), and blit it to 0xA0504.
 *
 * Outro: 12-frame slide-out (frame 11..0), mirror of the intro but
 * using the LEFT main-panel slide step.
 *
 * Cleanup: restore the battle screen (memmove snapshot_buf -> 0xA0000),
 * free both workspaces, drain the keyboard buffer, then return.
 *
 * Faithful detail (binary artifact): last_tick is read by the wait-loop
 * gate but NEVER written — in the binary it is the caller's leftover EBP
 * (uninitialized). The redraw therefore fires whenever the live tick
 * differs from that fixed entry-time value, i.e. essentially every
 * iteration. This is reproduced exactly: last_tick is left uninitialized
 * and never assigned.
 *
 * void __cdecl, no params. EBX/ESI/EDI are callee-saved (the __CHK(0x2c)
 * stack-probe prologue is compiler-injected and omitted under -s). The
 * binary's final JMP 0x10C49 is this function's own epilogue (ADD ESP,4 /
 * POP EDI/ESI/EBX / RET); the compiler tail-merged it with two siblings
 * (fd2_convert_battle_tiles_to_24px, fd2_equip_unequip_inventory_menu) so
 * it physically lives at 0x10C49, but it is NOT a callee — a plain return
 * regenerates the identical epilogue.
 * ---------------------------------------------------------------- */
void fd2_open_party_status_overview_screen(void)
{
    uint32 snapshot_buf;
    uint32 panel_buf;
    int frame_iter;
    int frame_idx;
    int kbd_pending;
    int32 last_tick;    /* read by the wait-loop gate, never written
                         * (binary: uninitialized caller EBP) */

    snapshot_buf = (uint32)malloc(64000);
    panel_buf = (uint32)malloc(64000);
    memset((void *)snapshot_buf, 0, 64000);

    fd2_composite_battle_tile_map(
        data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0xd, 8,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);
    fd2_composite_all_chars_overlay();
    fd2_blit_rectangle(snapshot_buf + 0x504, 0x140,
        data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0x138, 0xc0);
    fd2_render_party_status_overview_content(panel_buf, 0x140);

    for (frame_iter = 0; frame_iter < 0xc; frame_iter++) {
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
                (void *)snapshot_buf, 64000);
        fd2_slide_panel_step_right_main(panel_buf, (uint32)frame_iter);
        fd2_slide_panel_step_top_small(panel_buf, (uint32)frame_iter);
        fd2_slide_panel_step_bottom_main(panel_buf, (uint32)frame_iter);
        fd2_slide_panel_step_bottom_small(panel_buf, (uint32)frame_iter);
        memmove((void *)0xa0000,
                (void *)data_fd2_large_game_state_buffer_ptr, 64000);
    }

    do {
        kbd_pending = fd2_check_keyboard_buffer_nonempty();
        if ((int32)(int16)BIOS_TICK_WORD != last_tick) {
            fd2_tick_chapter_palette_animation();
            fd2_update_palette_cycle_anim();
            fd2_composite_battle_tile_map(
                data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0xd, 8,
                data_fd2_battle_view_window_origin_x,
                data_fd2_battle_view_window_origin_y);
            fd2_composite_all_chars_overlay();
            fd2_render_party_status_overview_content(
                data_fd2_large_game_state_buffer_ptr + 0x7964, 0x1c8);
            fd2_blit_rectangle(0xa0504, 0x140,
                data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8,
                0x138, 0xc0);
        }
    } while (kbd_pending == 0);

    fd2_clear_keyboard_buffer();

    for (frame_idx = 0xb; frame_idx >= 0; frame_idx--) {
        memmove((void *)data_fd2_large_game_state_buffer_ptr,
                (void *)snapshot_buf, 64000);
        fd2_slide_panel_step_left_main(panel_buf, (uint32)frame_idx);
        fd2_slide_panel_step_top_small(panel_buf, (uint32)frame_idx);
        fd2_slide_panel_step_bottom_main(panel_buf, (uint32)frame_idx);
        fd2_slide_panel_step_bottom_small(panel_buf, (uint32)frame_idx);
        memmove((void *)0xa0000,
                (void *)data_fd2_large_game_state_buffer_ptr, 64000);
    }

    memmove((void *)0xa0000, (void *)snapshot_buf, 64000);
    free((void *)snapshot_buf);
    free((void *)panel_buf);
    fd2_clear_keyboard_buffer();
    /* binary tail-JMP 0x10C49 == this function's own shared epilogue
     * (ADD ESP,4 / POP EDI/ESI/EBX / RET); a plain return regenerates it. */
}

/* ----------------------------------------------------------------
 * fd2_remove_inventory_slot_at @ 0x1B8E7  (10 callers)
 *
 * Remove the item in inventory slot `slot` of runtime_char[char_idx] and
 * close the gap by shifting every following slot down by one. Each slot is
 * 2 bytes (inventory_slots[i*2]=flag, [i*2+1]=item_id); flag bit 0x80 marks
 * a vacant slot, bit 0x40 marks equipped. There are 8 slots (indices 0..7).
 *
 * The shift is a single memmove of (7 - slot) slots = (7 - slot) * 2 bytes,
 * copying slots[slot+1 .. 7] over slots[slot .. 6]. Slot 7 is then always
 * stamped vacant (flag = 0x80), so it becomes the freed "new empty" slot
 * regardless of which slot was removed. (slot == 7 copies 0 bytes and only
 * re-stamps slot 7 vacant.)
 *
 * Callers: item drop / give / sell, the "backpack full" pickup swap, and
 * chapter-script events.
 *
 * void __cdecl with the __CHK(0x14) stack-probe prologue (compiler-injected,
 * not part of the source). EBX is the saved base pointer (callee-saved); the
 * trailing POP EBX + RET is the shared epilogue.
 * ---------------------------------------------------------------- */
void fd2_remove_inventory_slot_at(uint32 char_idx, uint32 slot)
{
    runtime_char *rc;

    rc = data_fd2_battle_runtime_char_array_ptr;
    memmove(rc[char_idx].inventory_slots + slot * 2,
            rc[char_idx].inventory_slots + slot * 2 + 2,
            (7 - slot) * 2);
    rc[char_idx].inventory_slots[14] = 0x80;
}

/* ----------------------------------------------------------------
 * fd2_inventory_selection_modal_dispatch @ 0x1B932  (3 callers)
 *
 * Open the character inventory-selection modal: show char_idx's status
 * screen + inventory grid (slide-in), then loop processing directional /
 * confirm input until a slot is chosen or Esc is pressed, then play the
 * 12-frame close-screen outro, restore the saved VGA snapshot to 0xA0000,
 * free the three slide workspace buffers, and return whether the user
 * confirmed (chosen slot index left in data_fd2_ui_menu_cursor_idx).
 *
 * gate_flag is forwarded to the grid input step:
 *   1 = only usable items selectable (item command "use"/"equip")
 *   0 = any slot selectable (swap / give / sort)
 *
 * fd2_inventory_grid_input_step returns 0 while still in the grid, a non-
 * -1 value once a slot is confirmed, and -1 on Esc cancel; the modal
 * returns (result != -1) as a 0/1 boolean.
 *
 * Callers: tile-event swap, battle drop swap, item command menu.
 *
 * int __cdecl with the __CHK(0x20) stack-probe prologue (compiler-injected,
 * not part of the source). EBX/ESI/EDI are callee-saved.
 * ---------------------------------------------------------------- */
int fd2_inventory_selection_modal_dispatch(uint32 char_idx, uint32 gate_flag)
{
    uint32 input_result;
    uint32 outro_iter;

    fd2_open_status_screen_with_slide_in(char_idx);
    data_fd2_ui_menu_cursor_idx = 0;
    do {
        input_result = (uint32)fd2_inventory_grid_input_step(char_idx, gate_flag);
    } while (input_result == 0);

    for (outro_iter = 0; (int)outro_iter < 0xc; outro_iter++) {
        fd2_play_status_screen_outro_step(
            outro_iter,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr,
            (int)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    }

    memmove((void *)0xa0000,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
    free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    free((void *)data_fd2_ui_slide_composed_target_buf_ptr);

    return input_result != 0xffffffff;
}

/* ----------------------------------------------------------------
 * fd2_item_command_menu_dispatch @ 0x1BBDC  (1 caller:
 *                                  fd2_player_inline_action_menu_dispatch)
 *
 * 4-way item command popup (Use / Give / Sort-Equip / Drop) opened when
 * the player picks "Item" in the inline action submenu during their turn.
 *
 * Setup: copy the 4-int options template @ 0x51F05 ({8,9,10,11}) into
 * menu_options and the 4-int state template @ 0x53F32 ({0,0,0,0}) into
 * menu_state. If the character has no usable inventory slots, return -1
 * (do not open the menu). Pre-disable "Give" (menu_state[1]=1) when there
 * is no adjacent ally tile (fd2_compute_aoe_targets cursor, range 1, kind 3
 * == 0). Render the menu and loop settings-menu input until non-zero; close
 * it, recomposite, and snapshot the cursor world position (target_x/y).
 *
 * On menu cancel (input_result == -1): return -1.
 *
 * Dispatch on data_fd2_ui_menu_cursor_idx:
 *   0 = USE (loops until commit or modal cancel):
 *       Pick a usable inventory slot (modal gate=1). On commit, look up the
 *       item entry; battle_anim_phase = item[0x12]+2; compute the AoE target
 *       set (range=item[0x10], is_spell = item[0xD]==0x17, kind=item[0x15])
 *       and wait for a target (mode=item[0x15]). battle_anim_phase=1; compute
 *       the apply AoE (range=item[0x12], kind=item[0x15]) into final_aoe.
 *       If the item is a spellbook (item[0xD]==0x17): require the caster to
 *       be job_id 0x18 (Magician) with mp_max >= 0x14, else target=-1; on
 *       success wait again (mode 6), then stash the cursor as the teleport
 *       destination and pan to the caster (anim_phase 0->1). If target!=-1:
 *       apply the use effect, mark the caster acted, return 1. Otherwise pan
 *       back (anim_phase 0->1) and re-prompt. Modal cancel returns 0.
 *   1 = GIVE: pick a slot (gate=0). Compute adjacent-ally AoE into a 100-byte
 *       malloc buffer, wait (mode 3) for the recipient, find the char under
 *       the cursor, pan back to the origin tile, free the buffer. If a target
 *       was chosen, add this slot's item to the recipient; if the recipient
 *       is full (-1) open their inventory modal to pick a slot to swap (remove
 *       recipient slot + add my item + remove my slot + add the swapped item),
 *       else just remove my slot. Set player_action_result_code = 1.
 *       Always recalc combat stats, return 0.
 *   2 = SORT/EQUIP: fd2_equip_unequip_inventory_menu, return 0.
 *   3 = DROP: pick a slot (gate=0); if confirmed remove it. Recalc, return 0.
 *
 * Returns: 1 = action committed (turn used), 0 = re-prompt the outer inline
 * menu, -1 = no items (menu not opened) or outer menu cancelled.
 *
 * int __cdecl (Ghidra types the return undefined4; the sole caller consumes
 * it as int -1/0/1) with the __CHK(0x94) stack-probe prologue (compiler-
 * injected, omitted under -s). EBX/ESI/EDI/EBP are callee-saved; EBP holds
 * char_idx, ESI/EDI are scratch. EAX-bug notes: every CALL whose EAX is
 * reused below is a genuine return value verified against the disassembly —
 * the modal commit flag (MOV EDI,EAX), the item entry pointer (MOV ESI,EAX),
 * fd2_compute_aoe_targets' count passed straight into the target-input call,
 * the target result (MOV EDI,EAX), the give recipient index (MOV ESI,EAX),
 * the add-item result (CMP EAX,-1), and the swapped item id (MOV EBX,EAX).
 * ---------------------------------------------------------------- */
int fd2_item_command_menu_dispatch(uint32 char_idx)
{
    int32 menu_options[4];
    int32 menu_state[4];
    uint8 target_buf[52];
    runtime_char *rc;
    uint8 *item_entry;
    uint8 item_id;
    uint8 swapped_item_id;
    uint32 saved_my_slot;
    int32 target_x;
    int32 target_y;
    uint32 final_aoe;
    uint32 give_buf;
    int target_char_idx;
    int input_result;
    int modal_committed;
    int target_result;
    int give_target_result;
    int add_result;

    menu_options[0] = data_fd2_ui_item_command_menu_template[0];
    menu_options[1] = data_fd2_ui_item_command_menu_template[1];
    menu_options[2] = data_fd2_ui_item_command_menu_template[2];
    menu_options[3] = data_fd2_ui_item_command_menu_template[3];

    menu_state[0] = data_fd2_ui_item_command_menu_state_template[0];
    menu_state[1] = data_fd2_ui_item_command_menu_state_template[1];
    menu_state[2] = data_fd2_ui_item_command_menu_state_template[2];
    menu_state[3] = data_fd2_ui_item_command_menu_state_template[3];

    if (fd2_count_usable_inventory_slots(char_idx) == 0) {
        return -1;
    }

    if (fd2_compute_aoe_targets(data_fd2_battle_cursor_world_x,
            data_fd2_battle_cursor_world_y, 0, 1, 1, 3) == 0) {
        menu_state[1] = 1;
    }
    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);

    fd2_count_active_menu_items_until_zero(menu_state);
    fd2_open_settings_dialog_with_slide(menu_options, menu_state);
    do {
        input_result = fd2_settings_menu_input_step(menu_options, menu_state);
    } while (input_result == 0);
    fd2_close_settings_dialog_with_slide(menu_options, menu_state);
    fd2_composite_battle_frame(0);

    target_y = (int32)data_fd2_battle_cursor_world_y;
    target_x = (int32)data_fd2_battle_cursor_world_x;
    if (input_result == -1) {
        return -1;
    }

    if (data_fd2_ui_menu_cursor_idx == 0) {
        do {
            modal_committed =
                fd2_inventory_selection_modal_dispatch(char_idx, 1);
            if (modal_committed == 0) {
                return 0;
            }

            item_id = fd2_get_inventory_slot_item_id(char_idx,
                data_fd2_ui_menu_cursor_idx);
            item_entry = fd2_get_item_effect_entry((int)item_id);
            data_fd2_battle_anim_phase = (uint32)item_entry[0x12] + 2;
            target_result = fd2_wait_for_action_target_input(
                (int)item_entry[0x15],
                (uint32)fd2_compute_aoe_targets(
                    data_fd2_battle_cursor_world_x,
                    data_fd2_battle_cursor_world_y, (uint32)target_buf,
                    (uint32)item_entry[0x10],
                    (uint32)(item_entry[0xd] == 0x17),
                    (uint32)item_entry[0x15]),
                target_buf);
            fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);

            data_fd2_battle_anim_phase = 1;
            final_aoe = (uint32)fd2_compute_aoe_targets(
                data_fd2_battle_cursor_world_x,
                data_fd2_battle_cursor_world_y, (uint32)target_buf,
                (uint32)item_entry[0x12], 0, (uint32)item_entry[0x15]);
            fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);

            if (item_entry[0xd] == 0x17) {
                rc = data_fd2_battle_runtime_char_array_ptr;
                if (rc[char_idx].char_id != 0x18
                    || rc[char_idx].mp_max < 0x14) {
                    target_result = -1;
                }
                if (target_result != -1) {
                    target_result = fd2_wait_for_action_target_input(
                        6, (uint32)target_buf[0], (uint8 *)0);
                }
                if (target_result != -1) {
                    data_fd2_battle_teleport_dest_world_x =
                        data_fd2_battle_cursor_world_x;
                    data_fd2_battle_teleport_dest_world_y =
                        data_fd2_battle_cursor_world_y;
                    data_fd2_battle_anim_phase = 0;
                    fd2_pan_cursor_to_char(char_idx);
                    data_fd2_battle_anim_phase = 1;
                }
            }

            if (target_result != -1) {
                fd2_apply_use_effect_dispatch(char_idx,
                    data_fd2_ui_menu_cursor_idx, final_aoe,
                    (uint32)target_buf);
                fd2_mark_char_acted_this_turn(char_idx);
                return 1;
            }

            data_fd2_battle_anim_phase = 0;
            fd2_pan_cursor_to_char(char_idx);
            data_fd2_battle_anim_phase = 1;
        } while (1);
    }

    if (data_fd2_ui_menu_cursor_idx != 1) {
        if (data_fd2_ui_menu_cursor_idx == 2) {
            fd2_equip_unequip_inventory_menu(char_idx);
            return 0;
        }
        if (fd2_inventory_selection_modal_dispatch(char_idx, 0) != 0) {
            fd2_remove_inventory_slot_at(char_idx,
                data_fd2_ui_menu_cursor_idx);
        }
        fd2_recalculate_combat_stats(char_idx);
        return 0;
    }

    if (fd2_inventory_selection_modal_dispatch(char_idx, 0) != 0) {
        give_buf = (uint32)malloc(100);
        give_target_result = fd2_wait_for_action_target_input(3,
            (uint32)fd2_compute_aoe_targets(data_fd2_battle_cursor_world_x,
                data_fd2_battle_cursor_world_y, give_buf, 1, 1, 3),
            (uint8 *)give_buf);
        target_char_idx = fd2_find_char_at_cursor_pos();
        fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);
        fd2_pan_cursor_to_tile_animated(target_x, target_y);
        free((void *)give_buf);

        if (give_target_result != -1) {
            item_id = fd2_get_inventory_slot_item_id(char_idx,
                data_fd2_ui_menu_cursor_idx);
            add_result = fd2_add_item_to_inventory(target_char_idx,
                (uint32)item_id);
            if (add_result == -1) {
                saved_my_slot = data_fd2_ui_menu_cursor_idx;
                if (fd2_inventory_selection_modal_dispatch(target_char_idx, 0)
                        == 0) {
                    goto recalc_and_exit;
                }
                swapped_item_id = fd2_get_inventory_slot_item_id(
                    target_char_idx, data_fd2_ui_menu_cursor_idx);
                fd2_remove_inventory_slot_at(target_char_idx,
                    data_fd2_ui_menu_cursor_idx);
                fd2_add_item_to_inventory(target_char_idx, (uint32)item_id);
                fd2_remove_inventory_slot_at(char_idx, saved_my_slot);
                fd2_add_item_to_inventory(char_idx, (uint32)swapped_item_id);
            }
            else {
                fd2_remove_inventory_slot_at(char_idx,
                    data_fd2_ui_menu_cursor_idx);
            }
            data_fd2_battle_player_action_result_code = 1;
        }
    }
recalc_and_exit:
    fd2_recalculate_combat_stats(char_idx);
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_inventory_grid_input_step @ 0x1B9DE  (2 callers)
 *
 * One frame of inventory-grid selection input. Redraws char_idx's 8-slot
 * inventory grid to VRAM (highlighting data_fd2_ui_menu_cursor_idx), counts
 * the active (non-vacant) slots, waits for a keystroke, and dispatches it:
 *
 *   Up    (0x48): cursor != 0 -> cursor--, return 0
 *                 cursor == 0 -> cursor = active_count-1 (wrap), return 0
 *   Down  (0x50): cursor != active_count-1 -> cursor++, return 0
 *                 cursor == last           -> cursor = 0 (wrap), return 0
 *   Left  (0x4B): cursor >= 4 -> cursor -= 4, return 0; else no-op return 0
 *   Right (0x4D): cursor < 4 && cursor < active_count-4 -> cursor += 4,
 *                 return 0; else no-op return 0
 *   Enter/Space (0x1C / 0x39):
 *                 gate_flag == 0 -> return 1 (commit)
 *                 gate_flag != 0 -> commit only if the selected item has a
 *                 use-effect (fd2_get_item_effect_entry(item_id)[0xD] != 0),
 *                 otherwise return 0 (re-prompt)
 *   Esc   (0x01): return -1 (cancel)
 *   other:        return 0 (loop again)
 *
 * Each accepted directional move plays SFX 0. gate_flag selects whether any
 * slot is choosable (0: swap/give/sort) or only usable items (1: use/equip).
 * The chosen slot index is left in data_fd2_ui_menu_cursor_idx; the caller
 * (fd2_inventory_selection_modal_dispatch) loops while this returns 0.
 *
 * int __cdecl with the __CHK(0x1C) stack-probe prologue (compiler-injected,
 * not part of the source). EBX/ESI/EDI are callee-saved.
 * ---------------------------------------------------------------- */
int fd2_inventory_grid_input_step(uint32 char_idx, uint32 gate_flag)
{
    runtime_char *rc;
    uint32 active_count;
    uint32 slot_iter;
    int scancode;
    uint8 *item_entry;

    active_count = 0;
    fd2_render_inventory_item_grid(char_idx, (int)data_fd2_ui_menu_cursor_idx,
                                   0xa0000);

    rc = data_fd2_battle_runtime_char_array_ptr;
    for (slot_iter = 0; (int)slot_iter < 8; slot_iter++) {
        if ((rc[char_idx].inventory_slots[slot_iter * 2] & 0x80) == 0) {
            active_count++;
        }
    }

    scancode = fd2_wait_for_input_dialog_with_blink(0);

    if (scancode == 0x48) {
        if (data_fd2_ui_menu_cursor_idx != 0) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 1;
            return 0;
        }
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
        data_fd2_ui_menu_cursor_idx = active_count - 1;
    } else if (scancode == 0x50) {
        if (active_count - 1 != data_fd2_ui_menu_cursor_idx) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1;
            return 0;
        }
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
        data_fd2_ui_menu_cursor_idx = 0;
    } else if (scancode == 0x4b) {
        if (3 < data_fd2_ui_menu_cursor_idx) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 4;
            return 0;
        }
    } else if (scancode == 0x4d) {
        if (data_fd2_ui_menu_cursor_idx < 4
            && (int)data_fd2_ui_menu_cursor_idx < (int)(active_count - 4)) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 4;
            return 0;
        }
    } else {
        if (scancode == 0x1c || scancode == 0x39) {
            if (gate_flag != 0) {
                item_entry = fd2_get_item_effect_entry(
                    (int)rc[char_idx].inventory_slots[
                        data_fd2_ui_menu_cursor_idx * 2 + 1]);
                if (item_entry[0xd] == 0) {
                    return 0;
                }
            }
            return 1;
        }
        if (scancode == 1) {
            return -1;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_equip_unequip_inventory_menu @ 0x1BFFE  (2 callers:
 *   fd2_item_command_menu_dispatch option 2 SORT/EQUIP,
 *   fd2_run_equip_member_menu)
 *
 * EQUIP / UNEQUIP interactive modal for char_idx's inventory.
 *
 * Slide the status screen + inventory grid in, reset the grid cursor,
 * then loop:
 *   - Spin fd2_inventory_grid_input_step (gate=0, any slot selectable)
 *     until it returns non-zero (a slot was confirmed, or Esc).
 *   - Fetch the item id at the current cursor slot (always, even when
 *     about to exit — faithful to the binary ordering).
 *   - Esc (input == -1)  -> exit the modal.
 *   - No usable slots left -> exit the modal.
 *   - If the character's job can equip the item: equip it in that slot
 *     (the primitive auto-unequips the conflicting same-category slot),
 *     recalc combat stats, then refresh the static status layout +
 *     inventory grid (cursor -1 = no highlight) via the
 *     large_game_state_buffer VGA double-buffer. Otherwise re-prompt
 *     without doing anything.
 *
 * On exit: play the 12-frame status-screen outro (frames 0..0xB),
 * restore the saved VGA snapshot to 0xA0000, and free the three slide
 * workspace buffers. The cleanup tail is identical to
 * fd2_inventory_selection_modal_dispatch's.
 *
 * void __cdecl with the __CHK(0x20) stack-probe prologue (compiler-
 * injected, omitted under -s). EBX/ESI/EDI are callee-saved; EBX holds
 * char_idx, ESI the loop input result, EDI the fetched item id. The
 * binary's final JMP 0x10C49 is this function's own shared epilogue
 * (ADD ESP,4 / POP EDI/ESI/EBX / RET), tail-merged with two siblings
 * (fd2_convert_battle_tiles_to_24px, fd2_open_party_status_overview_screen);
 * it is NOT a callee — a plain return regenerates the identical epilogue.
 *
 * EAX-bug notes: every CALL whose EAX is reused is a genuine return —
 * the input result (MOV ESI,EAX), the item id (MOV EDI,EAX, fully
 * MOVZX-zero-extended by the callee), the usable-slot count (TEST EAX),
 * and the can-equip flag (TEST EAX).
 * ---------------------------------------------------------------- */
void fd2_equip_unequip_inventory_menu(uint32 char_idx)
{
    uint32 input_result;
    uint32 item_id;
    uint32 outro_iter;

    fd2_open_status_screen_with_slide_in(char_idx);
    data_fd2_ui_menu_cursor_idx = 0;

    while (1) {
        do {
            input_result =
                (uint32)fd2_inventory_grid_input_step(char_idx, 0);
        } while (input_result == 0);

        item_id = fd2_get_inventory_slot_item_id(char_idx,
            data_fd2_ui_menu_cursor_idx);

        if (input_result == 0xffffffff) {
            break;
        }
        if (fd2_count_usable_inventory_slots(char_idx) == 0) {
            break;
        }
        if (fd2_check_job_can_equip_item(char_idx, item_id) != 0) {
            fd2_equip_item_in_slot(char_idx, data_fd2_ui_menu_cursor_idx);
            fd2_recalculate_combat_stats(char_idx);
            memmove((void *)data_fd2_large_game_state_buffer_ptr,
                    (void *)0xa0000, 64000);
            fd2_render_status_screen_static_layout(char_idx,
                data_fd2_large_game_state_buffer_ptr);
            fd2_render_inventory_item_grid(char_idx, -1,
                data_fd2_large_game_state_buffer_ptr);
            memmove((void *)0xa0000,
                    (void *)data_fd2_large_game_state_buffer_ptr, 64000);
        }
    }

    for (outro_iter = 0; (int)outro_iter < 0xc; outro_iter++) {
        fd2_play_status_screen_outro_step(
            outro_iter,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr,
            (int)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    }

    memmove((void *)0xa0000,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
    free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    free((void *)data_fd2_ui_slide_composed_target_buf_ptr);
    /* binary tail-JMP 0x10C49 == this function's own shared epilogue
     * (ADD ESP,4 / POP EDI/ESI/EBX / RET); a plain return regenerates it. */
}

/* ----------------------------------------------------------------
 * fd2_equip_item_in_slot @ 0x1C142  (2 callers)
 *
 * Mark inventory slot `slot_idx` of runtime_char[char_idx] as equipped,
 * first auto-unequipping any same-category item already equipped. This
 * enforces the "one weapon + one spellbook" rule (at most one equipped
 * item per category).
 *
 * Each slot is 2 bytes (inventory_slots[i*2]=flag, [i*2+1]=item_id);
 * there are 8 slots (indices 0..7). Flag bit 0x40 = equipped.
 *
 * The category split point is item_id 0x80 (see assets/items.md):
 *   0x00..0x7F = weapons/armor/items (physical),
 *   0x80..0xD6 = spellbooks/spells (magical).
 * Two items are "same category" iff both < 0x80 or both >= 0x80.
 *
 * Algorithm:
 *   item_id = fd2_get_inventory_slot_item_id(char_idx, slot_idx)
 *   for each of the 8 slots: if it is equipped (flag & 0x40) and holds a
 *     same-category item, clear its flag (unequip).
 *   then mark slot_idx as equipped (flag = 0x40).
 *
 * Callers: fd2_equip_unequip_inventory_menu, fd2_run_buy_item_menu.
 *
 * void __cdecl with the __CHK(0x14) stack-probe prologue (compiler-
 * injected, omitted under -s). EBX holds the chosen item_id (callee-
 * saved); ESI is the runtime_char base pointer; the trailing POP ESI /
 * POP EBX / RET is the shared epilogue.
 * ---------------------------------------------------------------- */
void fd2_equip_item_in_slot(uint32 char_idx, uint32 slot_idx)
{
    runtime_char *rc;
    uint8 item_id;
    uint32 scan_iter;

    rc = data_fd2_battle_runtime_char_array_ptr;
    item_id = fd2_get_inventory_slot_item_id(char_idx, slot_idx);

    for (scan_iter = 0; (int)scan_iter < 8; scan_iter++) {
        if ((rc[char_idx].inventory_slots[scan_iter * 2] & 0x40) != 0
            && ((item_id < 0x80
                 && rc[char_idx].inventory_slots[scan_iter * 2 + 1] < 0x80)
                || (item_id >= 0x80
                    && rc[char_idx].inventory_slots[scan_iter * 2 + 1] >= 0x80))) {
            rc[char_idx].inventory_slots[scan_iter * 2] = 0;
        }
    }

    rc[char_idx].inventory_slots[slot_idx * 2] = 0x40;
}

/* ----------------------------------------------------------------
 * fd2_give_item_to_first_player_char @ 0x1C220  (2 callers)
 *
 * Give item_id to the first player-side character (team == 2) whose
 * inventory still has a free slot. Scans runtime_char[0 ..
 * party_member_count-1] in order; for each player character it tries
 * fd2_add_item_to_inventory(char_idx, item_id), and stops as soon as
 * one succeeds (return value != -1). If every player character's
 * backpack is full, the function returns silently (item lost).
 *
 * team encoding: 0 = enemy, 1 = neutral NPC, 2 = player.
 * fd2_add_item_to_inventory returns -1 = backpack full, 1 = added.
 *
 * Callers: fd2_chapter_02_end (story gift), fd2_chapter_21_end
 * (Sky Key hidden-stage key).
 *
 * void __cdecl with the __CHK(0x14) stack-probe prologue (compiler-
 * injected, not part of the source). EBX is the loop counter / char
 * index (callee-saved), ESI holds item_id; the trailing POP ESI /
 * POP EBX / RET is the shared epilogue. The CMP EAX,-1 after the
 * fd2_add_item_to_inventory CALL is a genuine return-value test.
 * ---------------------------------------------------------------- */
void fd2_give_item_to_first_player_char(uint32 item_id)
{
    runtime_char *rc;
    uint32 char_idx;

    rc = data_fd2_battle_runtime_char_array_ptr;
    for (char_idx = 0;
         (int)char_idx < (int)data_fd2_battle_party_member_count;
         char_idx++) {
        if (rc[char_idx].team == 2
            && fd2_add_item_to_inventory(char_idx, item_id) != -1) {
            return;
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_run_status_screen_member_menu @ 0x2FFA5  (2 callers)
 *
 * Status-screen viewer loop for party members. Each iteration picks a
 * member via the standard party-roster select, opens that member's
 * status screen, then reloads the DATO.DAT portrait sheet (which the
 * status screen overwrote) before looping again. Loops until Esc.
 *
 * Called from the chapter-intro menus (option 0 = 状態).
 *
 * void __cdecl with the __CHK(0x1c) stack-probe prologue (compiler-
 * injected, not part of the source). EBX and ESI both hold the
 * roster-select return value; EBX gates the in-body break and ESI the
 * loop-back test — both compare against -1, so the trailing
 * "while (sel != -1)" is the same value already broken on, i.e. an
 * infinite loop with an Esc break. EDI saves dialog_portrait_mode
 * across the status submenu; it is restored only on the non-Esc path.
 * The CALL fd2_load_dat_resource return value is stored back into
 * data_fd2_portrait_sprite_buffer (genuine return use).
 * ---------------------------------------------------------------- */
void fd2_run_status_screen_member_menu(void)
{
    int sel;
    uint32 saved_portrait_mode;

    data_fd2_ui_menu_visible_item_count = data_fd2_shared_menu_party_member_count;
    for (;;) {
        sel = fd2_party_roster_single_select_loop();
        fd2_close_intro_dialog_with_slide_out();
        saved_portrait_mode = data_fd2_dialog_active_portrait_blit_offset;
        if (sel == -1) {
            break;
        }
        fd2_open_char_status_screen(data_fd2_ui_menu_cursor_idx);
        data_fd2_dialog_active_portrait_blit_offset = saved_portrait_mode;
        data_fd2_portrait_sprite_buffer = (uint8 *)fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_dato_dat_51a70,
            (uint32)data_fd2_portrait_sprite_buffer,
            (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[0]);
    }
}

/* ----------------------------------------------------------------
 * fd2_find_inventory_slot_with_item @ 0x31860  (5 callers)
 *
 * Search runtime_char[char_idx]'s inventory for a slot holding item_id.
 * Iterates over the per-char usable slot count returned by
 * fd2_count_usable_inventory_slots(char_idx); for each slot it reads the
 * slot's item id via fd2_get_inventory_slot_item_id(char_idx, slot) and
 * returns the first slot index whose item id equals item_id. Returns -1
 * when the usable count is 0 or no slot matches.
 *
 * Used to detect whether a char carries a specific key item (promotion
 * key, plot item, the Sword that triggers the Lord-class path).
 *
 * Callers: fd2_run_class_promotion_menu_main,
 * fd2_build_promotion_candidates_with_targets, fd2_any_char_has_item,
 * fd2_chapter_21_end, fd2_chapter_event_handler_3d__ch26_pickup.
 *
 * int __cdecl with the __CHK(0x1c) stack-probe prologue (compiler-
 * injected, not part of the source). fd2_get_inventory_slot_item_id
 * returns a zero-extended byte (MOVZX), so the full-EAX compare in the
 * asm is exactly a byte == item_id test.
 * ---------------------------------------------------------------- */
int fd2_find_inventory_slot_with_item(uint32 char_idx, uint32 item_id)
{
    int slot_count;
    uint32 slot_iter;

    slot_count = fd2_count_usable_inventory_slots(char_idx);
    if (slot_count != 0) {
        for (slot_iter = 0; (int)slot_iter < slot_count; slot_iter++) {
            if (fd2_get_inventory_slot_item_id(char_idx, slot_iter) == item_id) {
                return (int)slot_iter;
            }
        }
    }
    return -1;
}

/* ---- battle teleport-spell scratch state ----
 * Runtime scratch: written (= cursor_world_x/y) in
 * fd2_item_command_menu_dispatch (USE-spellbook branch) and
 * fd2_spell_selection_menu_main before fd2_cast_spell_17_complex reads it,
 * so it is zero-bss despite a stale nonzero image byte. */
uint32 data_fd2_battle_teleport_dest_world_x;
uint32 data_fd2_battle_teleport_dest_world_y;
