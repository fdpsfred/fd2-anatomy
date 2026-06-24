/*
 * promote.c — Church-revive and class-promotion menu helpers
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <stdio.h>
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
    data_fd2_ui_menu_candidate_array_ptr = candidate_idx_list;
    data_fd2_ui_menu_visible_item_count = candidate_count;

    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_composed_target_buf_ptr + 0x8c05,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr +
            *(int *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0x46),
        0x140);

    fd2_render_promote_members_grid(candidate_count,
        data_fd2_ui_slide_composed_target_buf_ptr,
        data_fd2_ui_menu_cursor_idx, candidate_idx_list);

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
                    data_fd2_ui_menu_cursor_idx, candidate_idx_list);
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
                    data_fd2_ui_menu_cursor_idx, candidate_idx_list);
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
 * revive picker, this one takes a THIRD param: the per-candidate
 * post-promotion target-class list (caller passes target_classes[]).
 * It renders via the 5-arg fd2_render_promote_candidates_grid, which
 * feeds each target class to fd2_get_class_promotion_data_entry to show
 * the post-promotion target job (the "current job -> target job" arrow)
 * per candidate.
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
    data_fd2_ui_menu_candidate_array_ptr = (uint8 *)char_list_ptr;
    data_fd2_ui_menu_visible_item_count = (uint32)char_count;

    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_composed_target_buf_ptr + 0x8c05,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr +
            *(int *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0x46),
        0x140);

    fd2_render_promote_candidates_grid((uint32)char_count,
        data_fd2_ui_slide_composed_target_buf_ptr,
        data_fd2_ui_menu_cursor_idx, (uint8 *)char_list_ptr,
        (uint8 *)price_aux_list_ptr);

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
                    data_fd2_ui_menu_cursor_idx, (uint8 *)char_list_ptr,
                    (uint8 *)price_aux_list_ptr);
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
                    data_fd2_ui_menu_cursor_idx, (uint8 *)char_list_ptr,
                    (uint8 *)price_aux_list_ptr);
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
 * fd2_execute_class_promotion_with_dialog @ 0x31602  (1 caller)
 *
 * Class-promotion finalization with stat-gain dialogs. char_idx =
 * index into the runtime_char array. Sole caller:
 * fd2_run_class_promotion_menu_main @ 0x31385 (after the candidate has
 * been picked, confirmed, the required item consumed, the promotion
 * fanfare/cinematic played, and job_id/portrait_id written back).
 *
 * Sequence:
 *   1. growth = fd2_get_char_growth_entry(rt_chars[idx].portrait_id) ->
 *      pointer to this (new) class's growth table row (5 (min,max) byte
 *      pairs: AP, DP, DX, HP, MP).
 *   2. Reload the speaker portrait and set the dialog substitution sprite
 *      id = rt_chars[idx].job_id + 0x96 ("becomes [class]" portrait).
 *   3. Show the "becomes [class]" dialog (FDTXT page 0x253) + paint the
 *      portrait + clear keyboard.
 *   4. Five fd2_roll_stat_gain_and_show_message rolls, one per stat. Each
 *      takes the stat's 16-bit raw slot, the matching (min,max) growth
 *      pair, the per-stat message page (0x1EA..0x1EE), and a 4-row dialog
 *      cursor that it returns advanced (so the messages stack down the
 *      box); the cursor is threaded call-to-call. The final return is the
 *      row index reused as the spell-dialog vertical offset base.
 *        +0x37 (combat_aux[0x10], AP raw)  growth+0  page 0x1EA
 *        +0x39 (combat_aux[0x12], DP raw)  growth+2  page 0x1EB
 *        +0x3E (ai_target_and_dx[1], DX raw) growth+4 page 0x1EC
 *        +0x42 (hp_max)                    growth+6  page 0x1ED
 *        +0x46 (mp_max)                    growth+8  page 0x1EE
 *   5. promo_entry = fd2_get_class_promotion_data_entry(rt_chars[idx].
 *      portrait_id). If promo_entry[1] != 0 (this class learns a spell on
 *      promotion): stash it as the dialog value, show the "learns [spell]"
 *      dialog (page 0x254) at row*0x17C0 + base, wait for a keypress, then
 *      append the spell id to combat_aux[0x14] (the known-spell list tail).
 *   6. fd2_recalculate_combat_stats(idx) (re-derive AP/DP/DX/EV from the
 *      new raw stats + equipment) then slide the dialog out.
 *   7. Reset to a fresh level-1 state for the new class: status_flags[0]
 *      (level) = 1, movement_order = 0 (XP-carry reset), and full-restore
 *      HP/MP (current = max).
 *   8. Clear the keyboard buffer.
 *
 * void __cdecl with the __CHK(0x34) stack-probe prologue (compiler-
 * injected, not part of the source). EDI caches the growth pointer, then
 * is reused for the threaded stat-roll cursor; ESI holds &rt_chars[idx];
 * the trailing POP EDI,ESI,EBX / RET is the function's own epilogue.
 *
 * EAX-tracking note (verified against the asm): the five
 * fd2_roll_stat_gain_and_show_message calls each PUSH their EAX result
 * straight into the next call's 4th arg (the threaded cursor), and the
 * last one is MOV EDI,EAX (reused as the spell-dialog row base), so all
 * five return values are genuinely consumed. fd2_get_class_promotion_
 * data_entry returns a pointer immediately dereferenced (MOVZX EAX,
 * byte ptr [EAX+1]); the TEST EAX,EAX is the real has-a-spell test.
 * ---------------------------------------------------------------- */
void fd2_execute_class_promotion_with_dialog(uint32 char_idx)
{
    uint8 *growth;
    int row;
    uint8 *promo_entry;
    runtime_char *rt_chars;

    rt_chars = data_fd2_battle_runtime_char_array_ptr;
    growth = fd2_get_char_growth_entry((int)rt_chars[char_idx].portrait_id);
    fd2_clear_keyboard_buffer();
    fd2_load_chapter_portrait((uint32)rt_chars[char_idx].portrait_id);
    data_fd2_dialog_last_action_sprite_id_param =
        (uint32)rt_chars[char_idx].job_id + 0x96;
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x253, 0xa951f,
        0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);
    fd2_clear_keyboard_buffer();

    row = fd2_roll_stat_gain_and_show_message(
        (short *)(rt_chars[char_idx].combat_aux_block + 0x10),
        growth, 0x1ea, 1);
    row = fd2_roll_stat_gain_and_show_message(
        (short *)(rt_chars[char_idx].combat_aux_block + 0x12),
        growth + 2, 0x1eb, row);
    row = fd2_roll_stat_gain_and_show_message(
        (short *)(rt_chars[char_idx].ai_target_and_dx_block + 1),
        growth + 4, 0x1ec, row);
    row = fd2_roll_stat_gain_and_show_message(
        (short *)&rt_chars[char_idx].hp_max, growth + 6, 0x1ed, row);
    row = fd2_roll_stat_gain_and_show_message(
        (short *)&rt_chars[char_idx].mp_max, growth + 8, 0x1ee, row);

    promo_entry = fd2_get_class_promotion_data_entry(
        (int)rt_chars[char_idx].portrait_id);
    if (promo_entry[1] != 0) {
        data_fd2_dialog_last_action_value_param = (uint32)promo_entry[1];
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x254,
            (uint32)(row * 0x17c0 + 0xa951f), 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_wait_for_input_dialog_with_blink(0);
        rt_chars[char_idx].combat_aux_block[0x14] = (uint8)
            (rt_chars[char_idx].combat_aux_block[0x14] +
             (int8)data_fd2_dialog_last_action_value_param);
    }

    fd2_recalculate_combat_stats(char_idx);
    fd2_close_intro_dialog_with_slide_out();
    rt_chars[char_idx].status_flags_block[0] = 1;
    rt_chars[char_idx].movement_order = 0;
    rt_chars[char_idx].hp_current = rt_chars[char_idx].hp_max;
    rt_chars[char_idx].mp_current = rt_chars[char_idx].mp_max;
    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * fd2_build_promotion_candidates_with_targets @ 0x31793  (1 caller)
 *
 * Build the promotion-eligible candidate list with target classes.
 * out_chars  = char-idx output buffer (max 32 entries),
 * out_targets = target-class output buffer (max 32 entries).
 * Returns the count (byte, zero-extended into EAX by the epilogue).
 *
 * Iterates char_idx over [0, party_member_count):
 *   - reads runtime_char[char_idx] (stride 0x50): portrait_id at +0x07,
 *     level at +0x21 (status_flags_block[0]).
 *   - skip unless level >= 0x14 (20) AND portrait_id < 0x12 (basic
 *     classes 0..0x11 only) AND portrait_id != 7 (蘭斯洛特/Lancelot, a
 *     fixed class with no promotion path -- handled elsewhere).
 *   - else record: out_chars[count] = char_idx;
 *     out_targets[count] = portrait_id + 0x20 (default tier-1 upgrade,
 *     e.g. base class 9 悠妮/Yuni -> 0x29 大法師/Archmage).
 *   - if fd2_find_inventory_slot_with_item(char_idx, key_item) != -1
 *     (unit holds the matching class-change item, where key_item =
 *     data_fd2_ui_per_basic_portrait_class_change_key_item_id_table
 *     [portrait_id]): out_targets[count] = portrait_id + 0x32 (alt
 *     advanced-tier branch, e.g. portrait 9 + item 0x58 聖者之戒
 *     -> 0x3B 聖者/Saint).
 *   - special: if portrait_id == 9 (悠妮/Yuni) AND the unit holds item
 *     0x5A (精靈契印, Yuni's summoner class-change item):
 *     out_targets[count] = 0x34 (悠妮（召喚師）/Yuni Summoner branch).
 *   - count++.
 *
 * Sole caller: fd2_run_class_promotion_menu_main @ 0x31385.
 *
 * uint8 __cdecl with the __CHK(0x24) stack-probe prologue (compiler-
 * injected, not part of the source). EBP caches out_chars, EDI caches
 * out_targets, EBX holds portrait_id, ESI the &rt_chars[idx] pointer
 * (then reused for &out_targets[count]); count is a single-byte local.
 *
 * EAX-tracking notes (verified against the asm): both
 * fd2_find_inventory_slot_with_item calls are immediately followed by
 * ADD ESP,8 / CMP EAX,-1, so EAX is the genuine slot return value.
 * ---------------------------------------------------------------- */
uint8 fd2_build_promotion_candidates_with_targets(uint8 *out_chars,
                                                  uint8 *out_targets)
{
    uint8 count;
    uint32 char_idx;
    uint8 portrait_id;
    runtime_char *rt_chars;

    rt_chars = data_fd2_battle_runtime_char_array_ptr;
    count = 0;
    for (char_idx = 0; (int)char_idx < (int)data_fd2_shared_menu_party_member_count;
         char_idx++) {
        portrait_id = rt_chars[char_idx].portrait_id;
        if (rt_chars[char_idx].status_flags_block[0] >= 0x14 &&
            (uint32)portrait_id < 0x12 && portrait_id != 7) {
            out_chars[count] = (uint8)char_idx;
            out_targets[count] = portrait_id + 0x20;
            if (fd2_find_inventory_slot_with_item(char_idx,
                    (uint32)data_fd2_ui_per_basic_portrait_class_change_key_item_id_table
                            [portrait_id]) != 0xffffffff) {
                out_targets[count] = portrait_id + 0x32;
            }
            if (portrait_id == 9) {
                if (fd2_find_inventory_slot_with_item(char_idx, 0x5a) != -1) {
                    out_targets[count] = 0x34;
                }
            }
            count++;
        }
    }
    return count;
}

/* ----------------------------------------------------------------
 * fd2_run_class_promotion_menu_main @ 0x31385  (1 caller)
 *
 * CLASS PROMOTION main menu (church / promotion service). Sole caller:
 * fd2_run_chapter_intro_menu_typeC @ 0x3072F (town promotion option).
 *
 * Outer loop:
 *   1. fd2_build_promotion_candidates_with_targets builds the parallel
 *      candidate_chars[] / target_classes[] lists (eligibility checked
 *      inside that helper: level>=20, basic class only, key-item check).
 *   2. If count==0: load chapter type-C portrait, show "no one is ready"
 *      dialog (page 0x24F), wait input, slide out, return.
 *   3. Show "promote whom?" prompt (page 0x250), wait input(blink=1),
 *      slide out, clear keyboard buffer.
 *   4. fd2_promote_member_select_loop -> 1=commit / -1=cancel; cancel
 *      returns from the menu.
 *   5. Reload chapter portrait, fetch candidate_chars[cursor_idx] and
 *      target_classes[cursor_idx], set last_action_sprite_id =
 *      char.portrait_id+1, show "ready to become [class]?" dialog
 *      (page 0x252) via fd2_text_dialog_typewriter_loop. Loop back to
 *      the top if the typewriter returns -1 (cancel) or cursor_idx!=0
 *      (NO selected; yes=row 0, no=row 1).
 *   6. Consume the required item: class_id==0x34 (劍士/Lord direct path)
 *      consumes Sword(0x5A); class_id>0x31 consumes the key item read
 *      from data_fd2_ui_per_basic_portrait_class_change_key_item_id_table
 *      [char.portrait_id]; class_id<=0x31 (the 0x20..0x31 tier-1 upgrade)
 *      consumes nothing.
 *   7. fd2_set_bgm_track_with_fade(0x10,1) promotion fanfare,
 *      fd2_play_spell_cast_cinematic(char_idx,class_id),
 *      fd2_set_bgm_track_with_fade(0xB,0).
 *   8. rt_chars[idx].job_id = *fd2_get_class_promotion_data_entry(class_id);
 *      rt_chars[idx].portrait_id = target_classes[cursor_idx]. Free the
 *      portrait sprite cache if set, reopen FDICON.B24, reset the portrait
 *      cache count, reload every party portrait
 *      (fd2_load_portrait_to_cache(roster[7 + i*0x50])), fclose.
 *   9. fd2_execute_class_promotion_with_dialog(char_idx) (stat gains +
 *      spell unlock + HP/MP restore), clear keyboard, loop back to step 1.
 *
 * void __cdecl with the __CHK(0x7c) stack-probe prologue (compiler-
 * injected, not part of the source). The frame holds two parallel
 * 32-entry byte arrays (target_classes[], candidate_chars[]) plus the
 * stashed class_id. EDI caches char_idx, ESI the &rt_chars[idx] pointer
 * (and is also reused as the portrait reload counter); the trailing
 * ADD ESP / POP EBP,EDI,ESI,EBX / RET is the function's own epilogue.
 *
 * EAX-tracking notes (verified against the asm):
 *   - fd2_build_promotion_candidates_with_targets returns its byte count
 *     zero-extended (epilogue MOVZX EAX,[count]); TEST EAX,EAX is a true
 *     count==0 test and the value is forwarded as a plain count.
 *   - fd2_promote_member_select_loop (MOV ESI,EAX) and
 *     fd2_text_dialog_typewriter_loop (MOV EBX,EAX) genuinely consume EAX.
 *   - fd2_find_inventory_slot_with_item's result is passed straight into
 *     fd2_remove_inventory_slot_at, and fd2_get_class_promotion_data_entry
 *     returns a pointer that is immediately dereferenced (MOV AL,[EAX]).
 * ---------------------------------------------------------------- */
void fd2_run_class_promotion_menu_main(void)
{
    uint8 candidate_count;
    int sel;
    int typewriter_ret;
    uint32 char_idx;
    uint32 class_id;
    uint32 item_id;
    int do_consume;
    uint32 slot;
    uint8 *promo_entry;
    void *fp;
    int i;
    runtime_char *rt_chars;
    uint8 target_classes[32];
    uint8 candidate_chars[32];

    do {
        do {
            candidate_count = fd2_build_promotion_candidates_with_targets(
                candidate_chars, target_classes);
            if (candidate_count == 0) {
                fd2_load_chapter_portrait(
                    (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[4]);
                fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x24f,
                    0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
                fd2_close_intro_dialog_with_slide_out();
                return;
            }

            fd2_load_chapter_portrait(
                (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[4]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x250,
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(1);
            fd2_close_intro_dialog_with_slide_out();
            fd2_clear_keyboard_buffer();

            sel = fd2_promote_member_select_loop((int)candidate_count,
                candidate_chars, target_classes);
            fd2_close_intro_dialog_with_slide_out();
            if (sel == -1) {
                return;
            }

            fd2_load_chapter_portrait(
                (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[4]);
            rt_chars = data_fd2_battle_runtime_char_array_ptr;
            char_idx = (uint32)candidate_chars[data_fd2_ui_menu_cursor_idx];
            class_id = (uint32)target_classes[data_fd2_ui_menu_cursor_idx];
            data_fd2_dialog_last_action_sprite_id_param =
                (uint32)rt_chars[char_idx].portrait_id + 1;

            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x252,
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_clear_keyboard_buffer();
            typewriter_ret = fd2_text_dialog_typewriter_loop();
            fd2_animate_dialog_page_advance_collapse();
            fd2_close_intro_dialog_with_slide_out();
        } while (typewriter_ret == -1 || data_fd2_ui_menu_cursor_idx != 0);

        do_consume = 0;
        item_id = 0;
        if (class_id == 0x34) {
            item_id = 0x5a;
            do_consume = 1;
        }
        else if (class_id > 0x31) {
            item_id = (uint32)data_fd2_ui_per_basic_portrait_class_change_key_item_id_table
                [rt_chars[char_idx].portrait_id];
            do_consume = 1;
        }
        if (do_consume) {
            slot = (uint32)fd2_find_inventory_slot_with_item(char_idx, item_id);
            fd2_remove_inventory_slot_at(char_idx, slot);
        }

        fd2_set_bgm_track_with_fade(0x10, 1);
        fd2_play_spell_cast_cinematic(char_idx, class_id);
        fd2_set_bgm_track_with_fade(0xb, 0);

        promo_entry = fd2_get_class_promotion_data_entry((int)class_id);
        rt_chars[char_idx].job_id = *promo_entry;
        rt_chars[char_idx].portrait_id = (uint8)class_id;

        if (data_fd2_portrait_sprite_cache != 0) {
            free((void *)data_fd2_portrait_sprite_cache);
        }
        fp = fopen("FDICON.B24", "rb");
        data_fd2_resource_portrait_cache_count = 0;
        for (i = 0; i < (int)data_fd2_shared_menu_party_member_count; i++) {
            fd2_load_portrait_to_cache(
                (uint32)*(uint8 *)(data_fd2_shared_menu_party_roster_buffer_ptr
                                   + 7 + i * 0x50),
                (uint32)fp);
        }
        fclose(fp);

        fd2_execute_class_promotion_with_dialog(char_idx);
        fd2_clear_keyboard_buffer();
    } while (1);
}

/* ----------------------------------------------------------------
 * fd2_run_recruitment_or_branch_screen @ 0x318AD  (2 callers)
 *
 * RECRUITMENT SCREEN / CHAPTER-BRANCH party-select dispatch, shown at
 * chapter transitions where the player picks who joins the next battle
 * (10x3 selectable grid). No params; reads current_chapter_id; returns
 * 1 when a full party was committed and all required-char gates passed,
 * else 0 (ESC / declined / failed gate). Party order escapes via
 * runtime_char_array (reorder) + the menu roster globals.
 *
 * Both callers (fd2_chapter_transition_menu @ 0x2CCD6 and
 * fd2_chapter_transition_with_intro @ 0x2D16B) consume the return value
 * with TEST EAX,EAX / JZ <loop>: a 0 result re-runs the screen (you
 * cannot ESC out of a mandatory recruitment), a 1 proceeds. The ESC
 * path sets the internal result_flag to -1 inside the input loop, but
 * the post-commit gate (LAB at 0x31CD4: CMP result_flag,1 / JNZ ->
 * XOR EDI,EDI) collapses every non-1 value to 0 before the final
 * MOV EAX,EDI / RET, so the only observable returns are 0 and 1.
 *
 * Flow:
 *   - max_chars = current_chapter_id > 0x1A ? 0x13 : 0x0F.
 *   - memset(selection_state[32], 0, 0x1E) (32-byte local cap; only the
 *     first 30 entries are cleared/used).
 *   - malloc 4x 64000-byte mode-13h buffers: render_workspace_a (0x53C5B),
 *     render_workspace_b (0x53C5F), panel_buf (local), render_workspace_c
 *     (0x53C63). Snapshot VGA 0xA0000 -> b -> panel_buf (twin: b restores,
 *     panel_buf is the foreground panel).
 *   - Blit the recruitment panel layers into panel_buf: atlas-offset +0x56
 *     (header banner) -> +0x91C, atlas-offset +0x5A (grid frame) -> +0x7585
 *     (both via fd2_dialog_sprite_blit_normal), sprite_id 0x89 -> +0x8C5
 *     (fd2_blit_indexed_sprite_rle). Initial render.
 *   - Intro slide-in: iter 0xB..0 (12 frames) via
 *     fd2_play_status_screen_outro_step; SFX 5 at iter==0xB and iter==5.
 *   - Input loop (do/while result_flag == 0):
 *       fd2_wait_input_with_recruitment_repaint -> scancode (full EAX,
 *       returned MOVZX byte; the asm compares it as int via EBX so the
 *       byte/CONCAT31 narrowing in the raw decompiler is an artifact).
 *       The scancode tests are independent ifs (matching the asm's chained
 *       CMP EBX), NOT a switch: 0x1C Enter intentionally rewrites scancode
 *       to 0x4D so the right-arrow auto-advance also fires.
 *         0x01 ESC   : result_flag = -1.
 *         0x1C Enter : SFX 7 (fd2_play_sfx_sample_from_bank),
 *                      selection_state[cursor] ^= 1, scancode := 0x4D; if
 *                      fd2_count_selected_chars == max_chars then
 *                      result_flag = 1 + fd2_reorder_party_by_selection.
 *         0x4B Left  : SFX 0, cursor--; if underflow (==0xffffffff) ->
 *                      menu_party_member_count - 2.
 *         0x4D Right : SFX 0, cursor++; if == menu_party_member_count - 1
 *                      -> cursor ^= (count - 1) (i.e. 0).
 *         0x48 Up && cursor > 9   : SFX 0, cursor -= 10.
 *         0x50 Down && cursor < count - 11 : SFX 0, cursor += 10.
 *       Re-render then memmove panel_buf(via render_workspace_c) -> 0xA0000.
 *   - Outro slide-out: iter 0..0xB (12 frames); SFX 6 at iter==0 and
 *     iter==7. Restore VGA via render_workspace_b -> 0xA0000. Free all 4.
 *   - Required-char gate (only if result_flag == 1): free portrait cache,
 *     fopen FDICON.B24, reset portrait_cache_count, reload every party
 *     portrait, fclose. Then by current_chapter_id resolve a required
 *     char_id and call fd2_require_char_id_in_active_party(max_chars, id),
 *     storing its 0/1 result into result_flag:
 *         0x10 && fd2_check_party_has_char_id(0x12) -> id 0x12
 *         0x11 / 0x13 / >=0x1A                       -> id 9
 *         0x12                                       -> id 0x10
 *         0x14                                       -> id 0x15
 *         0x15 / 0x16                                -> id 0x18
 *         0x19 && fd2_require_char_id_in_active_party(max_chars,9) -> id 0x1D
 *         (other chapters: no gate, result_flag stays 1)
 *       If the gate fails, result_flag becomes 0 and the function returns 0.
 *   - Pin + confirm (result_flag == 1 only): re-resolve the same chapter
 *     table (chapter 0x19 additionally pins char 9 first), call
 *     fd2_pin_required_char_to_party_slot1(id). Load portrait 0x4B, show
 *     the "ready to fight?" dialog (FDTXT 0x292) + paint portrait, set the
 *     battle_tile_map guard = 1, clear keyboard, run the typewriter loop,
 *     guard = 0, page-advance collapse, slide the dialog out. Final return:
 *     1 only if the typewriter was not cancelled (!= -1) AND the YES row
 *     (cursor_idx == 0) was chosen; otherwise 0.
 *
 * int __cdecl with the __CHK(0x64) stack-probe prologue (compiler-
 * injected, not part of the source). EBP caches max_chars, ESI the cursor
 * index, EDI the result_flag (also the return value), EBX the slide-frame
 * counter; the trailing ADD ESP / POP EBP,EDI,ESI,EBX / RET is the
 * function's own epilogue.
 *
 * GHIDRA SIGNATURE FIX: the decompiler modelled this as `void` with the
 * return value dropped, but both callers TEST EAX after the call, so it is
 * int-returning. Corrected the Ghidra prototype + plate accordingly.
 *
 * EAX-tracking notes (verified against the asm):
 *   - fd2_wait_input_with_recruitment_repaint -> MOV EBX,EAX, scancode is
 *     the genuine full-word (zero-extended byte) return.
 *   - fd2_count_selected_chars -> CMP EAX,EBP (count vs max_chars).
 *   - fd2_check_party_has_char_id -> TEST EAX,EAX (real has-char test).
 *   - fd2_require_char_id_in_active_party -> MOV EDI,EAX (0/1 into result_flag).
 *   - fd2_text_dialog_typewriter_loop -> MOV EBX,EAX (typewriter_ret).
 * ---------------------------------------------------------------- */
int fd2_run_recruitment_or_branch_screen(void)
{
    uint8 selection_state[32];
    void *panel_buf;
    int max_chars;
    int cursor_idx;
    int result_flag;
    int iter;
    int scancode;
    int count;
    int found;
    int typewriter_ret;
    int i;
    uint32 required_id;
    void *fp;

    max_chars = 0xf;
    result_flag = 0;
    cursor_idx = 0;
    memset(selection_state, 0, 0x1e);
    if ((int)data_fd2_chapter_current_chapter_id > 0x1a) {
        max_chars = 0x13;
    }

    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    panel_buf = malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);

    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr,
            (void *)0xa0000, 64000);
    memmove(panel_buf, (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);

    fd2_dialog_sprite_blit_normal(
        (uint32)panel_buf + 0x91c,
        data_fd2_ui_anim_sprite_sheet_ptr +
            *(int *)(data_fd2_ui_anim_sprite_sheet_ptr + 0x56),
        0x140);
    fd2_dialog_sprite_blit_normal(
        (uint32)panel_buf + 0x7585,
        data_fd2_ui_anim_sprite_sheet_ptr +
            *(int *)(data_fd2_ui_anim_sprite_sheet_ptr + 0x5a),
        0x140);
    fd2_blit_indexed_sprite_rle((uint32)panel_buf + 0x8c5, 0x140,
        data_fd2_ui_anim_sprite_sheet_ptr, 0x89);

    fd2_render_recruitment_select_screen((uint32)panel_buf, (uint32)max_chars,
        (uint32)selection_state, 0);

    for (iter = 0xb; iter >= 0; iter--) {
        if (iter == 0xb || iter == 5) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                5, 1);
        }
        fd2_play_status_screen_outro_step((uint32)iter,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr,
            (int)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    }

    fd2_clear_keyboard_buffer();
    do {
        scancode = fd2_wait_input_with_recruitment_repaint((uint32)panel_buf,
            (uint32)max_chars, (uint32)selection_state, (uint32)cursor_idx);
        if (scancode == 1) {
            result_flag = -1;
        }
        if (scancode == 0x1c) {
            fd2_play_sfx_sample_from_bank(
                data_fd2_audio_fdother_sfx_bank_buf_ptr, 7, 1);
            selection_state[cursor_idx] = selection_state[cursor_idx] ^ 1;
            scancode = 0x4d;
            count = fd2_count_selected_chars((uint32)selection_state);
            if (count == max_chars) {
                result_flag = 1;
                fd2_reorder_party_by_selection((uint32)selection_state);
            }
        }
        if (scancode == 0x4b) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                0, 1);
            cursor_idx--;
            if (cursor_idx == -1) {
                cursor_idx = (int)data_fd2_shared_menu_party_member_count - 2;
            }
        }
        if (scancode == 0x4d) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                0, 1);
            cursor_idx++;
            if (cursor_idx ==
                    (int)(data_fd2_shared_menu_party_member_count - 1)) {
                cursor_idx = cursor_idx ^
                    (int)(data_fd2_shared_menu_party_member_count - 1);
            }
        }
        if (scancode == 0x48 && cursor_idx > 9) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                0, 1);
            cursor_idx -= 10;
        }
        if (scancode == 0x50 &&
            cursor_idx < (int)data_fd2_shared_menu_party_member_count - 0xb) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                0, 1);
            cursor_idx += 10;
        }
        fd2_render_recruitment_select_screen((uint32)panel_buf,
            (uint32)max_chars, (uint32)selection_state, (uint32)cursor_idx);
        memmove((void *)0xa0000,
            (void *)data_fd2_ui_slide_composed_target_buf_ptr, 64000);
    } while (result_flag == 0);

    for (iter = 0; iter < 0xc; iter++) {
        if (iter == 0 || iter == 7) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                6, 1);
        }
        fd2_play_status_screen_outro_step((uint32)iter,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr,
            (int)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    }
    memmove((void *)0xa0000,
        (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
    free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    free(panel_buf);
    free((void *)data_fd2_ui_slide_composed_target_buf_ptr);

    if (result_flag == 1) {
        free((void *)data_fd2_portrait_sprite_cache);
        fp = fopen("FDICON.B24", "rb");
        data_fd2_resource_portrait_cache_count = 0;
        for (i = 0; i < (int)data_fd2_shared_menu_party_member_count; i++) {
            fd2_load_portrait_to_cache(
                (uint32)*(uint8 *)(data_fd2_shared_menu_party_roster_buffer_ptr
                                   + 7 + i * 0x50),
                (uint32)fp);
        }
        fclose(fp);

        required_id = 0;
        if (data_fd2_chapter_current_chapter_id == 0x10 &&
            fd2_check_party_has_char_id(0x12) != 0) {
            required_id = 0x12;
            found = fd2_require_char_id_in_active_party((uint32)max_chars,
                required_id);
            result_flag = found;
        }
        else if (data_fd2_chapter_current_chapter_id == 0x11 ||
                 data_fd2_chapter_current_chapter_id == 0x13 ||
                 data_fd2_chapter_current_chapter_id > 0x19) {
            required_id = 9;
            found = fd2_require_char_id_in_active_party((uint32)max_chars,
                required_id);
            result_flag = found;
        }
        else if (data_fd2_chapter_current_chapter_id == 0x12) {
            required_id = 0x10;
            found = fd2_require_char_id_in_active_party((uint32)max_chars,
                required_id);
            result_flag = found;
        }
        else if (data_fd2_chapter_current_chapter_id == 0x14) {
            required_id = 0x15;
            found = fd2_require_char_id_in_active_party((uint32)max_chars,
                required_id);
            result_flag = found;
        }
        else if (data_fd2_chapter_current_chapter_id == 0x15 ||
                 data_fd2_chapter_current_chapter_id == 0x16) {
            required_id = 0x18;
            found = fd2_require_char_id_in_active_party((uint32)max_chars,
                required_id);
            result_flag = found;
        }
        else if (data_fd2_chapter_current_chapter_id == 0x19) {
            found = fd2_require_char_id_in_active_party((uint32)max_chars, 9);
            result_flag = 0;
            if (found != 0) {
                required_id = 0x1d;
                found = fd2_require_char_id_in_active_party((uint32)max_chars,
                    required_id);
                result_flag = found;
            }
        }
    }

    /* LAB_00031CD4: every non-1 result_flag (0 declined, -1 ESC, or a failed
     * required-char gate) is collapsed to 0 here (asm XOR EDI,EDI before the
     * shared MOV EAX,EDI / RET). Only result_flag == 1 continues to pin+confirm. */
    if (result_flag != 1) {
        return 0;
    }

    if (data_fd2_chapter_current_chapter_id == 0x11 ||
        data_fd2_chapter_current_chapter_id == 0x13 ||
        data_fd2_chapter_current_chapter_id > 0x19) {
        required_id = 9;
    }
    else if (data_fd2_chapter_current_chapter_id == 0x14) {
        required_id = 0x15;
    }
    else if (data_fd2_chapter_current_chapter_id == 0x15 ||
             data_fd2_chapter_current_chapter_id == 0x16) {
        required_id = 0x18;
    }
    else {
        if (data_fd2_chapter_current_chapter_id != 0x19) {
            goto after_pin;
        }
        fd2_pin_required_char_to_party_slot1(9);
        required_id = 0x1d;
    }
    fd2_pin_required_char_to_party_slot1(required_id);

after_pin:
    fd2_load_chapter_portrait(0x4b);
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x292, 0xa951f,
        0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);
    data_fd2_battle_tile_map_ptr = 1;
    fd2_clear_keyboard_buffer();
    typewriter_ret = fd2_text_dialog_typewriter_loop();
    data_fd2_battle_tile_map_ptr = 0;
    fd2_animate_dialog_page_advance_collapse();
    fd2_close_intro_dialog_with_slide_out();
    if (typewriter_ret == -1) {
        return 0;
    }
    if (data_fd2_ui_menu_cursor_idx != 0) {
        return 0;
    }
    return 1;
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

/* ----------------------------------------------------------------
 * UI slide-in/out animation render-workspace pointer (BSS, zero at rest).
 *
 * data_fd2_ui_slide_anim_accumulator_buf_ptr @ 0x53C5B (.object2)
 *   Per-frame interpolated 320x200 (mode 13h) image buffer used by the
 *   menu slide-in/slide-out transitions (status / portrait / shop /
 *   promote / save / spell-select / chapter-intro). Assigned at runtime
 *   via (uint32)malloc(64000) by each opener and free()d on close; never
 *   statically initialized, so it lives in BSS and rests at 0.
 * ---------------------------------------------------------------- */
uint32 data_fd2_ui_slide_anim_accumulator_buf_ptr;

/* data_fd2_ui_slide_bg_snapshot_buf_ptr @ 0x53C5F (.object2)
 *   Pristine 320x200 background snapshot captured from VGA (0xA0000) at
 *   the start of each slide transition and used to restore the backdrop
 *   between frames. Same lifecycle as the accumulator above: assigned via
 *   (uint32)malloc(64000) by each opener, free()d on close, never
 *   statically initialized -> lives in BSS, rests at 0. */
uint32 data_fd2_ui_slide_bg_snapshot_buf_ptr;

/* data_fd2_ui_slide_composed_target_buf_ptr @ 0x53C63 (.object2)
 *   Fully composed 320x200 target image for the slide transition: the
 *   background snapshot is copied in, then the status panel / inventory
 *   grid / portrait / shop / promote content is rendered on top, giving
 *   the final frame the animation slides toward. Same lifecycle as the
 *   two pointers above: assigned via (uint32)malloc(64000) by each opener,
 *   free()d on close, never statically initialized -> lives in BSS, rests
 *   at 0. */
uint32 data_fd2_ui_slide_composed_target_buf_ptr;

/* data_fd2_ui_menu_candidate_array_ptr @ 0x54143 (.object2)
 *   Universal scrollable-menu candidate pointer: points at the byte array
 *   of valid candidate ids currently shown in the active menu/grid. Paired
 *   with the visible-row count @ 0x5413F. Each menu flow assigns it to a
 *   local byte[] up front (writers: fd2_run_buy_item_menu sets it to the
 *   equip-eligible char-id list; fd2_promote_members_select_loop and
 *   fd2_promote_member_select_loop set it to the promotable-member /
 *   candidate-class list); the chapter-intro panel renderer reads it back
 *   as candidate_array_ptr[scroll_offset + i] (a byte index into the
 *   portrait/sprite cache). Asm: store at 0x2F24C is MOV [0x54143],EAX with
 *   EAX = ESP (address of a stack-local byte[32]) -> a single 4-byte
 *   pointer slot; reads are byte-wide. Never statically initialized -> lives
 *   in BSS, rests at 0. (Ghidra previously mis-labeled this as
 *   chapter_intro_face_table_ptr after its first-observed chapter-face use.) */
uint8 *data_fd2_ui_menu_candidate_array_ptr;
