/*
 * promote.c — Church-revive and class-promotion menu helpers
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_build_dead_chars_list_for_revive @ 0x309FF  (1 caller)
 *
 * Build a list of party members who are CURRENTLY DEAD (eligible
 * for the revive-at-church service). For each char idx in
 * 0..menu_party_member_count-1: if fd2_check_char_is_dead(idx)
 * returns 1 (runtime_char[idx].bFlags & 1), append idx to
 * out_list_buf. Returns the count of dead chars (in EAX).
 *
 * Sole caller: fd2_run_revive_menu_main @ 0x30DC3 (mid-game town
 * typeC option 2 = 復活).
 *
 * int __cdecl with the __CHK(0x14) stack-probe prologue (compiler-
 * injected, not part of the source). EBX is the loop counter / char
 * index (callee-saved); ESI holds out_list_buf; the trailing POP ESI
 * / POP EBX / RET is the shared epilogue. count is a single-byte
 * local (stored / read via byte ops) returned zero-extended.
 * ---------------------------------------------------------------- */
int fd2_build_dead_chars_list_for_revive(uint8 *out_list_buf)
{
    uint8 count;
    uint32 idx;

    count = 0;
    for (idx = 0; (int)idx < (int)data_fd2_shared_menu_party_member_count; idx++) {
        if (fd2_check_char_is_dead(idx) == 1) {
            out_list_buf[count] = (uint8)idx;
            count++;
        }
    }
    return (int)count;
}

/* ----------------------------------------------------------------
 * fd2_promote_members_select_loop @ 0x30C22
 * (1 caller: fd2_run_revive_menu_main @ 0x30E90, the church-revive
 *  candidate picker)
 *
 * Church-revive candidate selection loop. Sets up the panel,
 * slides it down (6-frame reveal), runs the Up/Down navigation loop,
 * and returns the user's choice. NOT shared with class promotion:
 * the class-promotion menu fd2_run_class_promotion_menu_main uses a
 * separate 3-arg fd2_promote_member_select_loop (singular) instead.
 *
 * Returns:  1 = committed (Enter/Space),  -1 = cancelled (Esc).
 *
 * Setup: allocates three 64000-byte (mode 13h) workspaces (shared with
 * close fn fd2_close_intro_dialog_with_slide_out @ 0x2D31B), snapshots
 * VRAM into workspace_b, clones into workspace_c, stashes the candidate
 * list + count for re-render, blits the panel sprite, and renders the
 * grid. The viewport shows 3 items; navigation scrolls 1 step at a time.
 *
 * int __cdecl with the __CHK(0x24) stack-probe prologue (compiler-
 * injected, not part of the source). ESI is the result accumulator
 * (callee-saved), EBX the reveal-frame counter, EDI/EBP cache the two
 * params. The function tail-jumps to a shared MOV EAX,ESI / POP
 * EBP,EDI,ESI,EBX / RET epilogue (the trailing epilogue of
 * fd2_load_chapter_party_roster @ 0x2D3F8), i.e. plain `return result`.
 * The buffers are NOT freed here; cleanup is the caller's job via
 * fd2_close_intro_dialog_with_slide_out.
 *
 * fd2_wait_input_with_chapter_dialog_blink returns the scancode in the
 * full EAX; compared directly as int (asm uses CMP EAX,imm, no byte
 * truncation), so no CONCAT31 narrowing.
 * ---------------------------------------------------------------- */
int fd2_promote_members_select_loop(uint32 candidate_count, uint8 *candidate_idx_list)
{
    int result;
    int frame_iter;
    int scancode;

    result = 0;

    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);

    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr,
            (void *)0xa0000, 64000);
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);

    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_cursor_idx = 0;
    data_fd2_ui_menu_candidate_array_ptr = (uint32)candidate_idx_list;
    data_fd2_ui_menu_visible_item_count = candidate_count;

    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_composed_target_buf_ptr + 0x8c05,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr +
            *(int *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0x46),
        0x140);

    fd2_render_promote_members_grid(candidate_count,
        data_fd2_ui_slide_composed_target_buf_ptr,
        data_fd2_ui_menu_cursor_idx, (int)candidate_idx_list);

    for (frame_iter = 5; frame_iter >= 0; frame_iter--) {
        fd2_slide_panel_down_step((uint32)(frame_iter * 0xd + 0x70),
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr);
    }

    do {
        scancode = fd2_wait_input_with_chapter_dialog_blink(2);
        if (scancode == 0x48) {
            if (data_fd2_ui_menu_cursor_idx != 0) {
                data_fd2_ui_menu_cursor_idx--;
                if ((int)data_fd2_ui_menu_cursor_idx <
                        (int)data_fd2_ui_menu_scroll_offset) {
                    data_fd2_ui_menu_scroll_offset--;
                    fd2_animate_scroll_down_in_shop_dialog();
                }
                fd2_render_promote_members_grid(candidate_count, 0xa0000,
                    data_fd2_ui_menu_cursor_idx, (int)candidate_idx_list);
            }
        }
        else if (scancode == 0x50) {
            if ((int)data_fd2_ui_menu_cursor_idx < (int)(candidate_count - 1)) {
                data_fd2_ui_menu_cursor_idx++;
                if ((int)(data_fd2_ui_menu_cursor_idx -
                          data_fd2_ui_menu_scroll_offset) > 2) {
                    data_fd2_ui_menu_scroll_offset++;
                    fd2_animate_scroll_up_in_shop_dialog();
                }
                fd2_render_promote_members_grid(candidate_count, 0xa0000,
                    data_fd2_ui_menu_cursor_idx, (int)candidate_idx_list);
            }
        }
        else if (scancode == 0x1c || scancode == 0x39) {
            result = 1;
        }
        else if (scancode == 1) {
            result = -1;
        }
    } while (result == 0);

    return result;
}

/* ----------------------------------------------------------------
 * fd2_run_revive_menu_main @ 0x30DC3  (1 caller)
 *
 * CHURCH REVIVE main menu loop. Picks a dead party member, charges a
 * per-level fee, then revives them (clears bFlags + restores HP_current
 * to HP_max). Sole caller: fd2_run_chapter_intro_menu_typeC @ 0x3072F
 * (option 2 = 復活 at the town chapter).
 *
 * Outer loop:
 *   - Build list of dead chars; if none, show "no one is dead" (FDTXT
 *     0x24C) and return.
 *   - Greeting "revive whom?" (0x24D), then run the candidate picker.
 *     Esc (-1) exits.
 *   - Compute price = char.bLevel * price_table[char.bJob_id + 5]
 *     (the per-job multiplier table that aliases the shop "inventory
 *     full" dialog-id table @ 0x5265F). Store dialog substitution
 *     params (sprite id = bChar_id + 1, value = price).
 *   - Confirm "pay X gold?" (0x24E). On yes (cursor 0) with enough
 *     gold: deduct, clear bFlags, full-restore HP, redraw money panel,
 *     play revive fanfare (BGM 0x11) then return to ambient (BGM 0x0B),
 *     loop again. Not enough gold -> show 0x1F8.
 *
 * void __cdecl with the __CHK(0x50) stack-probe prologue (compiler-
 * injected, not part of the source). EBX holds the dead-char count
 * (callee-saved), reused as the chosen runtime_char pointer once a
 * candidate is picked; ESI holds the typewriter result. The trailing
 * ADD ESP / POP ESI / POP EBX / RET is the function's own epilogue.
 *
 * The three return-value CALLs (build_dead_chars_list, the picker, and
 * the typewriter loop) each have an explicit MOV reg,EAX after them in
 * the assembly, so the return values are genuinely consumed (not the
 * Ghidra EAX-tracking artifact). count==0 branch passes that 0 count
 * straight through to the two dialog helpers.
 * ---------------------------------------------------------------- */
/* ----------------------------------------------------------------
 * fd2_promote_member_select_loop @ 0x311DC  (1 caller)
 *
 * CLASS-PROMOTION member-select grid loop (singular — distinct from
 * the church-revive picker fd2_promote_members_select_loop @ 0x30C22).
 * Allocates three 64000-byte (mode 13h) render workspaces, snapshots
 * VRAM 0xA0000 -> workspace_b -> workspace_c, blits the dialog frame
 * at workspace_c+0x8C05, renders the candidate grid (current job ->
 * target job arrow), then plays a 6-frame slide-down reveal. The input
 * loop moves the cursor Up(0x48)/Down(0x50) with 3-row auto-scroll,
 * commits on Enter(0x1C)/Space(0x39) -> return 1, cancels on
 * ESC(0x01) -> return -1.
 *
 * Sole caller: fd2_run_class_promotion_menu_main @ 0x31385. Unlike the
 * revive picker, this one takes a THIRD param (price_aux_list_ptr; the
 * caller passes target_classes[]) and renders via the 5-arg
 * fd2_render_promote_candidates_grid (which shows the post-promotion
 * target job per candidate).
 *
 * int __cdecl with the __CHK(0x28) stack-probe prologue (compiler-
 * injected, not part of the source). ESI is the result accumulator
 * (callee-saved), EBX the reveal-frame counter, EDI/EBP cache the
 * char_count / char_list_ptr params. The function tail-jumps to a
 * shared MOV EAX,ESI / POP EBP,EDI,ESI,EBX / RET epilogue (the
 * trailing epilogue at 0x2D3F8), i.e. plain `return result`. The
 * buffers are NOT freed here; cleanup is the caller's job via
 * fd2_close_intro_dialog_with_slide_out.
 *
 * fd2_wait_input_with_chapter_dialog_blink returns the scancode in the
 * full EAX; the asm compares it directly as int (CMP EAX,imm, no byte
 * truncation), so no CONCAT31 narrowing is modelled.
 * ---------------------------------------------------------------- */
int fd2_promote_member_select_loop(int char_count, void *char_list_ptr,
                                   void *price_aux_list_ptr)
{
    int result;
    int frame_iter;
    int scancode;

    result = 0;

    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);

    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr,
            (void *)0xa0000, 64000);
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);

    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_cursor_idx = 0;
    data_fd2_ui_menu_candidate_array_ptr = (uint32)char_list_ptr;
    data_fd2_ui_menu_visible_item_count = (uint32)char_count;

    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_composed_target_buf_ptr + 0x8c05,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr +
            *(int *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0x46),
        0x140);

    fd2_render_promote_candidates_grid((uint32)char_count,
        data_fd2_ui_slide_composed_target_buf_ptr,
        data_fd2_ui_menu_cursor_idx, (int)char_list_ptr,
        (int)price_aux_list_ptr);

    for (frame_iter = 5; frame_iter >= 0; frame_iter--) {
        fd2_slide_panel_down_step((uint32)(frame_iter * 0xd + 0x70),
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr);
    }

    do {
        scancode = fd2_wait_input_with_chapter_dialog_blink(2);
        if (scancode == 0x48) {
            if (data_fd2_ui_menu_cursor_idx != 0) {
                data_fd2_ui_menu_cursor_idx--;
                if ((int)data_fd2_ui_menu_cursor_idx <
                        (int)data_fd2_ui_menu_scroll_offset) {
                    data_fd2_ui_menu_scroll_offset--;
                    fd2_animate_scroll_down_in_shop_dialog();
                }
                fd2_render_promote_candidates_grid((uint32)char_count, 0xa0000,
                    data_fd2_ui_menu_cursor_idx, (int)char_list_ptr,
                    (int)price_aux_list_ptr);
            }
        }
        else if (scancode == 0x50) {
            if ((int)data_fd2_ui_menu_cursor_idx < char_count - 1) {
                data_fd2_ui_menu_cursor_idx++;
                if ((int)(data_fd2_ui_menu_cursor_idx -
                          data_fd2_ui_menu_scroll_offset) > 2) {
                    data_fd2_ui_menu_scroll_offset++;
                    fd2_animate_scroll_up_in_shop_dialog();
                }
                fd2_render_promote_candidates_grid((uint32)char_count, 0xa0000,
                    data_fd2_ui_menu_cursor_idx, (int)char_list_ptr,
                    (int)price_aux_list_ptr);
            }
        }
        else if (scancode == 0x1c || scancode == 0x39) {
            result = 1;
        }
        else if (scancode == 1) {
            result = -1;
        }
    } while (result == 0);

    return result;
}

void fd2_run_revive_menu_main(void)
{
    int dead_count;
    int sel;
    int typewriter_ret;
    uint8 chosen_idx;
    runtime_char *rc;
    uint8 candidate_chars[32];

    do {
        dead_count = fd2_build_dead_chars_list_for_revive(candidate_chars);
        if (dead_count == 0) {
            fd2_load_chapter_portrait(
                (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[4]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x24c,
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(0);
            fd2_close_intro_dialog_with_slide_out();
            return;
        }

        fd2_load_chapter_portrait(
            (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[4]);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x24d,
            0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        fd2_wait_for_input_dialog_with_blink(1);
        fd2_close_intro_dialog_with_slide_out();

        sel = fd2_promote_members_select_loop((uint32)dead_count,
                                              candidate_chars);
        fd2_close_intro_dialog_with_slide_out();
        if (sel == -1) {
            return;
        }

        fd2_load_chapter_portrait(
            (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[4]);
        rc = data_fd2_battle_runtime_char_array_ptr;
        chosen_idx = candidate_chars[data_fd2_ui_menu_cursor_idx];
        data_fd2_dialog_last_action_sprite_id_param =
            (uint32)rc[chosen_idx].char_id + 1;
        data_fd2_dialog_last_action_value_param =
            (uint32)rc[chosen_idx].status_flags_block[0] *
            (int32)data_fd2_dialog_shop_inventory_full_dialog_text_id_table
                [rc[chosen_idx].job_id + 5];

        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x24e,
            0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        typewriter_ret = fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();

        if (typewriter_ret != -1 && data_fd2_ui_menu_cursor_idx == 0) {
            if ((int32)data_fd2_dialog_last_action_value_param <=
                    (int32)data_fd2_shared_party_total_gold) {
                fd2_animate_money_decrement(
                    data_fd2_dialog_last_action_value_param);
                rc[chosen_idx].flags = 0;
                rc[chosen_idx].hp_current = rc[chosen_idx].hp_max;
                fd2_dialog_sprite_blit_normal(
                    data_fd2_ui_slide_bg_snapshot_buf_ptr + 0x76c5,
                    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr +
                        *(int32 *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr
                                   + 10),
                    0x140);
                fd2_render_decimal_number_to_buffer(
                    data_fd2_ui_slide_bg_snapshot_buf_ptr + 0x7bd0, 0x140,
                    data_fd2_shared_party_total_gold, 0x1f, 8);
                fd2_close_intro_dialog_with_slide_out();
                fd2_set_bgm_track_with_fade(0x11, 1);
                fd2_animate_shop_transaction_feedback();
                fd2_set_bgm_track_with_fade(0xb, 1);
                continue;
            }
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1f8,
                0xac44c, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(1);
        }
        fd2_close_intro_dialog_with_slide_out();
    } while (1);
}
