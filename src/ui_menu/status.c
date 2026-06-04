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
 * highlight) into workspace_c. Drives a 12-frame slide-in (frame 0xB down
 * to 0); a chime SFX fires at frame 0xB (open) and frame 5 (mid). Finally
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
