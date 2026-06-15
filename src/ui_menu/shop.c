/*
 * shop.c — Shop / give-item screen: panel open animation + navigation loop.
 *
 * fd2_open_shop_dialog_panel @ 0x2E0BD (3 callers: fd2_run_buy_item_menu,
 *   fd2_run_sell_item_menu, fd2_run_give_item_menu)
 * fd2_shop_menu_input_loop @ 0x2DF6B (3 callers: fd2_run_buy_item_menu,
 *   fd2_run_sell_item_menu, fd2_run_give_item_menu)
 * fd2_pick_stat_compare_color @ 0x2EF8F (1 caller:
 *   fd2_render_party_roster_with_item_stat_preview)
 * fd2_run_buy_item_menu @ 0x2F0B0 (1 caller: fd2_run_chapter_intro_menu_main)
 * fd2_run_sell_item_menu @ 0x2F642 (1 caller: fd2_run_chapter_intro_menu_main)
 * fd2_run_equip_member_menu @ 0x2F883 (1 caller: fd2_run_chapter_intro_menu_main)
 * fd2_run_give_item_menu @ 0x2F8EA (1 caller: fd2_run_chapter_intro_menu_main)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_pick_stat_compare_color @ 0x2EF8F  (1 caller)
 *
 * Pick the digit-color sprite-base for the "with item equipped" stat
 * preview, comparing the char's current base stat against the previewed
 * stat (signed):
 *   current == preview  -> 0x1F  (red,    "no change")
 *   current  <  preview  -> 0x2A  (white,  new stat is LARGER -> player
 *                                  loses points when changing; counter-
 *                                  intuitive but matches the original)
 *   current  >  preview  -> 0x77  (orange, "would increase")
 *
 * Sole caller fd2_render_party_roster_with_item_stat_preview @ 0x2EBE0
 * invokes it once per stat (AP, DP, DX, Stat4) per visible char in the
 * buy-item compare overlay; the cdecl call sites PUSH preview then
 * current and ADD ESP,8 afterward, consuming the uint return in EAX.
 *
 * The binary's __CHK(4) stack-probe prologue is compiler-injected and not
 * part of the source, so it is omitted. The post-__CHK code reloads both
 * operands from the stack (no CALL-return value is used), so there is no
 * EAX-tracking hazard here.
 * ---------------------------------------------------------------- */
uint32 fd2_pick_stat_compare_color(int current_stat, int preview_stat)
{
    if (current_stat == preview_stat) {
        return 0x1f;
    }
    if (current_stat < preview_stat) {
        return 0x2a;
    }
    return 0x77;
}

/* ----------------------------------------------------------------
 * fd2_shop_menu_input_loop @ 0x2DF6B  (3 callers)
 *
 * Cursor navigation + row-paged scroll for the buy / sell / give-item
 * screens, laid out as a 2-column grid showing 6 items (3 rows) at a
 * time. Per iteration it reads one scancode through the real
 * fd2_wait_input_with_chapter_dialog_blink(1) (mode 1 = 2-panel layout)
 * and dispatches:
 *   Right (0x4D): if cursor != item_count-1, cursor += 1
 *   Left  (0x4B): if cursor != 0,            cursor -= 1
 *   Up    (0x48): if cursor > 1,             cursor -= 2  (row up)
 *   Down  (0x50): if cursor < item_count-2,  cursor += 2  (row down)
 *   Enter (0x1C) / Space (0x39): return 1   (commit)
 *   Esc   (0x01):                return -1  (cancel)
 * On any move it plays SFX 0 (cursor chime), pages the 6-item viewport
 * by 2 when the cursor leaves it (animating the scroll), and re-renders
 * the grid. Loops until commit or cancel.
 *
 * Globals: data_fd2_ui_menu_cursor_idx (0x53C57) absolute cursor;
 *          data_fd2_ui_menu_scroll_offset (0x5412F) top-row index of the
 *          6-item viewport (steps of 2);
 *          data_fd2_audio_fdother_sfx_bank_buf_ptr (0x53EEC) SFX bank.
 *
 * EAX-bug note: the scancode is the full int return of
 * fd2_wait_input_with_chapter_dialog_blink (disasm CMP EAX,0x4d et al.
 * compare the full 32-bit EAX), captured as an int here — Ghidra
 * narrows it to a byte via CONCAT31, which this avoids.
 * ---------------------------------------------------------------- */
int fd2_shop_menu_input_loop(uint32 param_1, uint8 *param_2, uint32 param_3)
{
    int scancode;
    uint32 delta;
    int result;

    result = 0;
    do {
        scancode = fd2_wait_input_with_chapter_dialog_blink(1);
        if (scancode == 0x4d) {
            if (param_1 - 1 != data_fd2_ui_menu_cursor_idx) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1;
LAB_dfb8:
                delta = data_fd2_ui_menu_cursor_idx
                      - data_fd2_ui_menu_scroll_offset;
                if (5 < (int)delta) {
                    data_fd2_ui_menu_scroll_offset =
                        data_fd2_ui_menu_scroll_offset + 2;
                    fd2_animate_scroll_up_in_shop_dialog();
                }
LAB_dfd4:
                fd2_render_shop_item_grid(param_1, param_2,
                    data_fd2_ui_menu_cursor_idx, 0xa0000, param_3 & 0xff);
            }
        }
        else if (scancode == 0x4b) {
            if (data_fd2_ui_menu_cursor_idx != 0) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 1;
LAB_e01e:
                if ((int)data_fd2_ui_menu_cursor_idx
                        < (int)data_fd2_ui_menu_scroll_offset) {
                    data_fd2_ui_menu_scroll_offset =
                        data_fd2_ui_menu_scroll_offset - 2;
                    fd2_animate_scroll_down_in_shop_dialog();
                }
                goto LAB_dfd4;
            }
        }
        else if (scancode == 0x48) {
            if (1 < (int)data_fd2_ui_menu_cursor_idx) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 2;
                goto LAB_e01e;
            }
        }
        else if (scancode == 0x50) {
            if ((int)data_fd2_ui_menu_cursor_idx < (int)(param_1 - 2)) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 2;
                goto LAB_dfb8;
            }
        }
        else if ((scancode == 0x1c) || (scancode == 0x39)) {
            result = 1;
        }
        else if (scancode == 1) {
            result = -1;
        }
        if (result != 0) {
            return result;
        }
    } while (1);
}

/* ----------------------------------------------------------------
 * fd2_open_shop_dialog_panel @ 0x2E0BD  (3 callers)
 *
 * Open the SHOP DIALOG panel with a 6-frame slide-down reveal. Open-side
 * counterpart of fd2_close_intro_dialog_with_slide_out @ 0x2D31B; shares the
 * three 64000-byte (320x200) workspace globals with it.
 *
 * Setup:
 *   1. Allocate three 64000-byte render workspaces:
 *        slide_anim_accumulator (0x53C5B) — per-frame slide scratch
 *        slide_bg_snapshot      (0x53C5F) — VGA backup (restored by close fn)
 *        slide_composed_target  (0x53C63) — full panel composite
 *   2. memmove 0xA0000 -> snapshot (capture the live framebuffer), then
 *      memmove snapshot -> composed_target (composite starts as the screen).
 *   3. Paint the shop-title sprite into composed_target at mode-13h offset
 *      0x8C05: fd2_dialog_sprite_blit_normal(composed_target + 0x8C05,
 *      atlas + *(atlas + 0x46), 0x140), atlas = sprite-atlas buffer (0x54147).
 *   4. Render the item grid into composed_target with the current cursor:
 *      fd2_render_shop_item_grid(item_count, item_id_array,
 *      cursor_idx (0x53C57), composed_target, sell_mode_flag & 0xFF).
 *   5. 6-frame slide-down loop (frame_iter 5->0):
 *        panel_y = frame_iter*0xD + 0x70  (0xB1,0xA4,0x97,0x8A,0x7D,0x70)
 *        fd2_slide_panel_down_step(panel_y, slide_anim_accumulator,
 *                                  slide_composed_target)
 *
 * The binary's __CHK(0x1C) stack-probe prologue is compiler-injected and not
 * part of the source, so it is omitted. sell_mode_flag is forwarded masked to
 * its low byte (the call site does MOVZX EAX, byte ptr [sell_mode_flag]).
 * ---------------------------------------------------------------- */
void fd2_open_shop_dialog_panel(uint32 item_count, uint8 *item_id_array,
                                uint32 sell_mode_flag)
{
    uint32 frame_iter;
    uint32 panel_y;

    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);

    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr,
            (void *)0xa0000, 64000);
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);

    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_composed_target_buf_ptr + 0x8c05,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr
            + *(uint32 *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0x46),
        0x140);

    fd2_render_shop_item_grid(item_count, item_id_array,
        data_fd2_ui_menu_cursor_idx,
        data_fd2_ui_slide_composed_target_buf_ptr, sell_mode_flag & 0xff);

    for (frame_iter = 5; -1 < (int)frame_iter; frame_iter--) {
        panel_y = frame_iter * 0xd + 0x70;
        fd2_slide_panel_down_step(panel_y,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr);
    }
}

/* ----------------------------------------------------------------
 * fd2_run_buy_item_menu @ 0x2F0B0  (1 caller: fd2_run_chapter_intro_menu_main)
 *
 * Top-level loop for the BUY branch of the chapter-intro shop menu. Each
 * iteration: open the item-grid panel, run the cursor loop, and on a commit
 * walk the buy flow (eligibility filter -> "buy for whom?" confirm ->
 * affordability -> recipient roster select -> inventory-full check -> add item
 * -> optional auto-equip -> transaction + money-drop animation). Esc on the
 * item grid exits.
 *
 *   shop_item_count     number of items offered this visit
 *   shop_item_id_array  byte array of item ids (indexed by the saved cursor)
 *
 * Per-shop-tier dialog text ids are pulled from five short[6] tables, indexed
 * by data_fd2_chapter_intro_menu_cursor_state (the shop variant, 0..5). Four of
 * them are snapshotted into local arrays up front (matching the binary's MOVSD
 * block copies); the inventory-full table is read straight from the global:
 *   buy_for     (0x526FA) -> "buy for whom?" confirmation
 *   no_money    (0x52706) -> "can't afford"
 *   no_equip    (0x52712) -> "no one can equip this"
 *   auto_equip  (0x5271E) -> "auto-equip the gear just bought?"
 *   inv_full    (0x5265F) -> "inventory is full"
 *
 * Item category: fd2_get_item_effect_entry(item_id)[+0] < 0x20 means gear (an
 * equip eligibility scan + stat-preview roster + auto-equip prompt apply); >=
 * 0x20 is a consumable (whole party eligible, plain roster select, no equip).
 * Price is the 16-bit field at item_entry[+0x13].
 *
 * The binary's __CHK(0x90) stack-probe prologue is compiler-injected and not
 * part of the source, so it is omitted (as in the sibling shop functions). The
 * decompiler's frame_pad[4004] / in_stack_00000000 are __CHK artifacts and not
 * real locals (the real frame is the 0x58 declared below).
 *
 * EAX-bug notes (each "CALL then use return" point checked against asm):
 *   - fd2_get_item_effect_entry returns the item-effect pointer (EBX), used for
 *     the category byte and the +0x13 price word.
 *   - fd2_shop_menu_input_loop / both roster select loops return the full int
 *     selection (-1 cancel / 1 commit), captured as int (no byte narrowing).
 *   - fd2_text_dialog_typewriter_loop returns the yes/no choice (-1 / else);
 *     the actual yes/no is then read from data_fd2_ui_menu_cursor_idx.
 *   - In the auto-equip branch the slot index is fd2_count_usable_inventory_
 *     slots(recipient) - 1 (the asm SUB EAX,ESI has ESI == 1 here, the commit
 *     result, which is provably 1 on this path).
 *
 * The gold-vs-price test is SIGNED (asm JGE), so both sides are cast to int.
 * ---------------------------------------------------------------- */
void fd2_run_buy_item_menu(uint32 shop_item_count, uint8 *shop_item_id_array)
{
    int16   buy_for_table[6];
    int16   no_money_table[6];
    int16   no_equip_table[6];
    int16   auto_equip_table[6];
    uint8   eligible_chars[32];
    uint8  *item_entry;
    uint32  item_id;
    uint32  candidate_count;
    uint32  scan_iter;
    uint32  saved_visible_count;
    uint32  buy_char_idx;
    int     menu_choice;
    int     confirm_choice;
    int     auto_equip_choice;
    int     select_result;
    int     slot_count;
    uint8   recipient;

    memcpy(no_equip_table, data_fd2_dialog_shop_no_equip_dialog_text_id_table,
           sizeof(no_equip_table));
    memcpy(buy_for_table, data_fd2_dialog_shop_buy_for_dialog_text_id_table,
           sizeof(buy_for_table));
    memcpy(auto_equip_table,
           data_fd2_dialog_shop_auto_equip_dialog_text_id_table,
           sizeof(auto_equip_table));
    memcpy(no_money_table, data_fd2_dialog_shop_no_money_dialog_text_id_table,
           sizeof(no_money_table));

    for (;;) {
        data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_saved_cursor_idx;
        data_fd2_ui_menu_scroll_offset = data_fd2_ui_menu_saved_scroll_offset;
        fd2_open_shop_dialog_panel(shop_item_count, shop_item_id_array, 0);
        menu_choice = fd2_shop_menu_input_loop(shop_item_count,
                                               shop_item_id_array, 0);
        data_fd2_ui_menu_saved_cursor_idx = data_fd2_ui_menu_cursor_idx;
        data_fd2_ui_menu_saved_scroll_offset = data_fd2_ui_menu_scroll_offset;
        fd2_close_intro_dialog_with_slide_out();
        if (menu_choice == -1) {
            return;
        }

        item_id = *(uint8 *)(data_fd2_ui_menu_saved_cursor_idx
                             + shop_item_id_array);
        data_fd2_dialog_last_action_sprite_id_param = item_id + 0xb5;
        item_entry = fd2_get_item_effect_entry(item_id);
        data_fd2_dialog_last_action_value_param =
            *(uint16 *)(item_entry + 0x13);

        candidate_count = 0;
        for (scan_iter = 0;
             (int)scan_iter < (int)data_fd2_shared_menu_party_member_count;
             scan_iter++) {
            if (*item_entry < 0x20) {
                if (fd2_check_job_can_equip_item(scan_iter, item_id) != 1) {
                    continue;
                }
            }
            eligible_chars[candidate_count] = (uint8)scan_iter;
            candidate_count++;
        }

        if (*item_entry < 0x20 && candidate_count == 0) {
            fd2_load_chapter_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                    data_fd2_chapter_intro_menu_cursor_state]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                no_equip_table[data_fd2_chapter_intro_menu_cursor_state],
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(1);
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        saved_visible_count = data_fd2_ui_menu_visible_item_count;
        data_fd2_ui_menu_visible_item_count = candidate_count;
        data_fd2_ui_menu_candidate_array_ptr = eligible_chars;

        fd2_load_chapter_portrait(
            data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                data_fd2_chapter_intro_menu_cursor_state]);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
            buy_for_table[data_fd2_chapter_intro_menu_cursor_state],
            0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        confirm_choice = fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();
        if (confirm_choice == -1 || data_fd2_ui_menu_cursor_idx == 1) {
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        if ((int)data_fd2_shared_party_total_gold
                < (int)data_fd2_dialog_last_action_value_param) {
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                no_money_table[data_fd2_chapter_intro_menu_cursor_state],
                0xac44c, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(1);
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        fd2_close_intro_dialog_with_slide_out();

        if (*item_entry < 0x20) {
            select_result = fd2_party_roster_class_select_loop(candidate_count,
                (uint32)eligible_chars, item_id);
        } else {
            data_fd2_ui_menu_visible_item_count =
                data_fd2_shared_menu_party_member_count;
            select_result = fd2_party_roster_single_select_loop();
        }
        data_fd2_ui_menu_visible_item_count = saved_visible_count;
        fd2_close_intro_dialog_with_slide_out();
        if (select_result != 1) {
            continue;
        }

        recipient = eligible_chars[data_fd2_ui_menu_cursor_idx];
        if (fd2_count_usable_inventory_slots(recipient) == 8) {
            data_fd2_dialog_last_action_sprite_id_param =
                data_fd2_battle_runtime_char_array_ptr[recipient].portrait_id
                + 1;
            fd2_load_chapter_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                    data_fd2_chapter_intro_menu_cursor_state]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                data_fd2_dialog_shop_inventory_full_dialog_text_id_table[
                    data_fd2_chapter_intro_menu_cursor_state],
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(1);
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        fd2_add_item_to_inventory(recipient, item_id);
        if (*item_entry < 0x20) {
            fd2_load_chapter_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                    data_fd2_chapter_intro_menu_cursor_state]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                auto_equip_table[data_fd2_chapter_intro_menu_cursor_state],
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            buy_char_idx = recipient;
            auto_equip_choice = fd2_text_dialog_typewriter_loop();
            fd2_animate_dialog_page_advance_collapse();
            if (auto_equip_choice != -1 && data_fd2_ui_menu_cursor_idx == 0) {
                slot_count = fd2_count_usable_inventory_slots(buy_char_idx);
                fd2_equip_item_in_slot(buy_char_idx, slot_count - 1);
                fd2_recalculate_combat_stats(buy_char_idx);
            }
            fd2_close_intro_dialog_with_slide_out();
        }
        fd2_animate_shop_transaction_feedback();
        fd2_animate_money_decrement(data_fd2_dialog_last_action_value_param);
    }
}

/* ----------------------------------------------------------------
 * fd2_run_sell_item_menu @ 0x2F642  (1 caller: fd2_run_chapter_intro_menu_main)
 *
 * Top-level loop for the SELL branch of the chapter-intro shop menu. The mirror
 * of fd2_run_buy_item_menu: it picks the seller char FIRST, builds that char's
 * inventory list, then runs the item grid; on a commit it sells the chosen item
 * at 75% of its base price (price * 3 / 4). Esc on the seller roster exits.
 *
 * Per iteration:
 *   1. data_fd2_ui_menu_visible_item_count = party_member_count, then
 *      fd2_party_roster_single_select_loop() picks the seller. -1 (Esc) returns.
 *   2. Build the seller's inventory id list: for slot 0..7, skip slots whose
 *      flag byte (inventory_slots[slot*2]) has bit 0x80 set (empty); otherwise
 *      append the item id (inventory_slots[slot*2+1]) to a local 8-byte list.
 *   3. If the list is empty, show the "nothing to sell" reject dialog (second
 *      table) and loop back to the seller roster.
 *   4. Otherwise reset cursor/scroll, open the sell-mode item panel (sell_mode=1
 *      shows 75% price), and run the shop input loop (sell_mode=1). -1 (Esc)
 *      loops back to the seller roster.
 *   5. On commit, read the chosen slot's item id via
 *      fd2_get_inventory_slot_item_id(seller, cursor), set the portrait param
 *      (item_id + 0xB5), and compute the sale price = item_entry[+0x13] * 3 / 4
 *      (signed >>2) into data_fd2_dialog_last_action_value_param.
 *   6. Show the "sell?" yes/no confirm dialog (first table); cancel / "no"
 *      (cursor == 1) loops back. On "yes": transaction feedback, a money-gain
 *      animation of the sale price, remove the slot, and recompute combat stats.
 *
 * Per-shop-tier dialog text ids come from two short[6] tables indexed by
 * data_fd2_chapter_intro_menu_cursor_state (the shop variant, 0..5), snapshotted
 * up front into local arrays (matching the binary's MOVSD block copies):
 *   sell_for   (0x5272A) -> "what would you sell?" confirm
 *   no_items   (0x52736) -> "you have nothing to sell" reject
 *
 * Price is the 16-bit field at item_entry[+0x13] (same field the buy sibling
 * reads); fd2_get_item_effect_entry returns the item-effect pointer.
 *
 * The binary's __CHK(0x54) stack-probe prologue is compiler-injected and not
 * part of the source, so it is omitted (as in the sibling shop functions). The
 * decompiler's frame_pad[4036] / in_stack_00000000 are __CHK artifacts and not
 * real locals.
 *
 * EAX-bug notes (each "CALL then use return" point checked against asm):
 *   - fd2_party_roster_single_select_loop returns the full int selection
 *     (-1 cancel), captured as int (no byte narrowing). The seller index is
 *     then read from data_fd2_ui_menu_cursor_idx.
 *   - fd2_get_inventory_slot_item_id returns a clean zero-extended byte (its
 *     tail is MOVZX EAX,[..]; RET), so item_id + 0xB5 and the +0x13 price word
 *     both use the byte value unambiguously.
 *   - fd2_shop_menu_input_loop returns the full int (-1 cancel / 1 commit).
 *   - fd2_text_dialog_typewriter_loop returns the yes/no choice (-1 / else);
 *     the actual yes/no is then read from data_fd2_ui_menu_cursor_idx (== 1 is
 *     "no").
 *
 * The sale price is a plain signed integer (price*3)>>2 (asm SHL/SUB then SAR),
 * which floors toward zero for the always-non-negative price*3.
 * ---------------------------------------------------------------- */
void fd2_run_sell_item_menu(void)
{
    int16   sell_for_table[6];
    int16   no_items_table[6];
    uint8   char_inventory[8];
    uint8  *item_entry;
    uint32  seller;
    uint32  slot_idx;
    uint8   item_id;
    int     select_result;
    uint32  inv_count;
    int     slot_iter;
    int     menu_result;
    int     confirm_choice;

    memcpy(sell_for_table, data_fd2_dialog_shop_sell_for_dialog_text_id_table,
           sizeof(sell_for_table));
    memcpy(no_items_table,
           data_fd2_dialog_shop_sell_nothing_to_sell_text_id_table,
           sizeof(no_items_table));

    for (;;) {
        data_fd2_ui_menu_visible_item_count =
            data_fd2_shared_menu_party_member_count;
        select_result = fd2_party_roster_single_select_loop();
        fd2_close_intro_dialog_with_slide_out();
        if (select_result == -1) {
            return;
        }
        seller = data_fd2_ui_menu_cursor_idx;

        inv_count = 0;
        for (slot_iter = 0; slot_iter < 8; slot_iter++) {
            if ((data_fd2_battle_runtime_char_array_ptr[seller]
                     .inventory_slots[slot_iter * 2] & 0x80) == 0) {
                char_inventory[inv_count] =
                    data_fd2_battle_runtime_char_array_ptr[seller]
                        .inventory_slots[slot_iter * 2 + 1];
                inv_count++;
            }
        }

        if (inv_count == 0) {
            data_fd2_dialog_last_action_sprite_id_param =
                data_fd2_battle_runtime_char_array_ptr[seller].portrait_id + 1;
            fd2_load_chapter_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                    data_fd2_chapter_intro_menu_cursor_state]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                no_items_table[data_fd2_chapter_intro_menu_cursor_state],
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(1);
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        data_fd2_ui_menu_cursor_idx = 0;
        data_fd2_ui_menu_scroll_offset = 0;
        fd2_open_shop_dialog_panel(inv_count, char_inventory, 1);
        data_fd2_ui_menu_visible_item_count = inv_count;
        menu_result = fd2_shop_menu_input_loop(inv_count,
                                               char_inventory, 1);
        if (menu_result == -1) {
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }
        slot_idx = data_fd2_ui_menu_cursor_idx;
        fd2_close_intro_dialog_with_slide_out();

        item_id = fd2_get_inventory_slot_item_id(seller,
                                                 data_fd2_ui_menu_cursor_idx);
        data_fd2_dialog_last_action_sprite_id_param = item_id + 0xb5;
        item_entry = fd2_get_item_effect_entry(item_id);
        data_fd2_dialog_last_action_value_param =
            (uint32)((int)((uint32)*(uint16 *)(item_entry + 0x13) * 3) >> 2);

        fd2_load_chapter_portrait(
            data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                data_fd2_chapter_intro_menu_cursor_state]);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
            sell_for_table[data_fd2_chapter_intro_menu_cursor_state],
            0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        confirm_choice = fd2_text_dialog_typewriter_loop();
        fd2_animate_dialog_page_advance_collapse();
        if (confirm_choice == -1 || data_fd2_ui_menu_cursor_idx == 1) {
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        fd2_close_intro_dialog_with_slide_out();
        fd2_animate_shop_transaction_feedback();
        fd2_animate_money_increment(data_fd2_dialog_last_action_value_param);
        fd2_remove_inventory_slot_at(seller, slot_idx);
        fd2_recalculate_combat_stats(seller);
    }
}

/* ----------------------------------------------------------------
 * fd2_run_equip_member_menu @ 0x2F883  (1 caller:
 *   fd2_run_chapter_intro_menu_main, option idx 2 = 裝備)
 *
 * Top-level loop for the EQUIP branch of the chapter-intro shop menu. Each
 * iteration: present the full party roster, let the player pick a member, run
 * that member's 8-slot equip/unequip inventory submenu, then reload the chapter
 * speaker portrait. Esc on the roster exits the loop.
 *
 * Per iteration:
 *   1. data_fd2_ui_menu_visible_item_count = party_member_count, then
 *      fd2_party_roster_single_select_loop() picks the member; the selection is
 *      kept (signed -1 means Esc).
 *   2. fd2_close_intro_dialog_with_slide_out() tears down the roster panel.
 *   3. Snapshot data_fd2_dialog_active_portrait_blit_offset (the equip submenu
 *      clobbers it) — done after the Esc check, since the snapshot is unused on
 *      the exit path (matching the binary's MOV-after-JZ ordering).
 *   4. On Esc (selection == -1) return.
 *   5. fd2_equip_unequip_inventory_menu(data_fd2_ui_menu_cursor_idx) runs the
 *      per-member equip UI for the member left in the cursor global.
 *   6. Restore data_fd2_dialog_active_portrait_blit_offset.
 *   7. Reload the chapter speaker portrait sprite from DATO.DAT (path string
 *      0x51A70, the 80x80 portrait archive) into data_fd2_portrait_sprite_buffer,
 *      indexed by
 *      data_fd2_chapter_intro_menu_speaker_portrait_id_table[
 *      data_fd2_chapter_intro_menu_cursor_state].
 *
 * The binary's __CHK(0x14) stack-probe prologue is compiler-injected and not
 * part of the source, so it is omitted (as in the sibling shop functions). The
 * decompiler's in_stack_* are __CHK artifacts and not real locals.
 *
 * EAX-bug notes (each "CALL then use return" point checked against asm):
 *   - fd2_party_roster_single_select_loop returns the full int selection
 *     (asm MOV EBX,EAX then CMP EBX,-1), captured as int (no byte narrowing).
 *     The chosen member is then read from data_fd2_ui_menu_cursor_idx.
 *   - fd2_load_dat_resource returns the freshly loaded sprite buffer pointer
 *     (asm MOV [0x53A85],EAX), stored straight into the portrait buffer global.
 * ---------------------------------------------------------------- */
void fd2_run_equip_member_menu(void)
{
    int    select_result;
    uint32 saved_blit_offset;

    for (;;) {
        data_fd2_ui_menu_visible_item_count =
            data_fd2_shared_menu_party_member_count;
        select_result = fd2_party_roster_single_select_loop();
        fd2_close_intro_dialog_with_slide_out();
        saved_blit_offset = data_fd2_dialog_active_portrait_blit_offset;
        if (select_result == -1) {
            return;
        }

        fd2_equip_unequip_inventory_menu(data_fd2_ui_menu_cursor_idx);
        data_fd2_dialog_active_portrait_blit_offset = saved_blit_offset;

        data_fd2_portrait_sprite_buffer =
            (uint8 *)fd2_load_dat_resource(0x51a70,
                (uint32)data_fd2_portrait_sprite_buffer,
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                    data_fd2_chapter_intro_menu_cursor_state]);
    }
}

/* ----------------------------------------------------------------
 * fd2_run_give_item_menu @ 0x2F8EA  (1 caller: fd2_run_chapter_intro_menu_main,
 *   option idx 3 = 贈)
 *
 * GIVE / TRADE an item between two party members. Each iteration runs a six-step
 * flow with TWO roster selects (source then target):
 *   1. Open the chapter-speaker portrait + the "give: pick a source" prompt
 *      dialog (text page 0x200), then pick the SOURCE member via
 *      fd2_party_roster_single_select_loop(). Esc (-1) exits the loop.
 *   2. Build the source's inventory id list: for slot 0..7, skip slots whose
 *      flag byte (inventory_slots[slot*2]) has bit 0x80 set (empty); otherwise
 *      append the item id (inventory_slots[slot*2+1]) to a local 8-byte list.
 *   3. If the list is empty, show the "nothing to give" reject dialog (fixed
 *      page 0x1FF) and loop back to the source roster.
 *   4. Otherwise reset cursor/scroll, open the sell-mode item panel (sell_mode=1
 *      is layout-only here) and run the shop input loop (sell_mode=1). Esc (-1)
 *      loops back to the source roster.
 *   5. Show the "give to whom?" prompt dialog (fixed page 0x1FE; an informational
 *      portrait+dialog, NOT a yes/no confirm), then pick the TARGET member.
 *      Esc (-1) loops back to the source roster.
 *   6. If the target already holds 8 items, show the "target inventory full"
 *      reject dialog (data_fd2_dialog_shop_inventory_full_dialog_text_id_table[
 *      data_fd2_chapter_intro_menu_cursor_state]) and loop back. Otherwise read
 *      the chosen item from the source slot, remove it from the source, add it
 *      (unequipped) to the target, and recompute the SOURCE's combat stats (the
 *      target is not recomputed because the item is added unequipped).
 *
 * Slot/list-index identity: the shop cursor indexes the compacted display list,
 * but the transfer passes that cursor straight to fd2_get_inventory_slot_item_id
 * / fd2_remove_inventory_slot_at as a raw slot index. This is correct because
 * inventories are kept gap-free (remove shifts later slots up, add fills the
 * first free slot), so the compacted-list index always equals the raw slot.
 *
 * The two reject dialogs read the member's char_id (runtime struct +0x08), not
 * the portrait_id (+0x07) that the sibling sell menu uses, into
 * data_fd2_dialog_last_action_sprite_id_param (matching the binary's MOVZX +8).
 *
 * The binary's __CHK(0x3C) stack-probe prologue is compiler-injected and not
 * part of the source, so it is omitted (as in the sibling shop functions). The
 * decompiler's in_stack_* are __CHK artifacts and not real locals; the real
 * frame is the 8-byte inventory list below.
 *
 * EAX-bug notes (each "CALL then use return" point checked against asm):
 *   - both fd2_party_roster_single_select_loop calls return the full int
 *     selection (asm MOV EBX,EAX then CMP EBX,-1), captured as int (no byte
 *     narrowing). The source / target index is then read from
 *     data_fd2_ui_menu_cursor_idx.
 *   - fd2_shop_menu_input_loop returns the full int (-1 cancel / 1 commit); the
 *     chosen slot is read from data_fd2_ui_menu_cursor_idx after it returns.
 *   - fd2_get_inventory_slot_item_id returns a clean zero-extended byte (tail is
 *     MOVZX EAX,[..]; RET); its value is saved into give_item_id BEFORE the
 *     intervening fd2_remove_inventory_slot_at call (asm MOV EBX,EAX) and then
 *     forwarded to fd2_add_item_to_inventory, so no EAX clobber hazard.
 * ---------------------------------------------------------------- */
void fd2_run_give_item_menu(void)
{
    uint8  inv_list[8];
    uint32 source;
    uint32 selected_slot;
    uint32 inv_count;
    int    slot_iter;
    int    src_select;
    int    item_select;
    int    tgt_select;
    uint8  give_item_id;

    for (;;) {
        fd2_load_chapter_portrait(
            data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                data_fd2_chapter_intro_menu_cursor_state]);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x200,
            0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        fd2_wait_for_input_dialog_with_blink(1);
        fd2_close_intro_dialog_with_slide_out();

        data_fd2_ui_menu_visible_item_count =
            data_fd2_shared_menu_party_member_count;
        src_select = fd2_party_roster_single_select_loop();
        fd2_close_intro_dialog_with_slide_out();
        source = data_fd2_ui_menu_cursor_idx;
        if (src_select == -1) {
            return;
        }

        inv_count = 0;
        for (slot_iter = 0; slot_iter < 8; slot_iter++) {
            if ((data_fd2_battle_runtime_char_array_ptr[source]
                     .inventory_slots[slot_iter * 2] & 0x80) == 0) {
                inv_list[inv_count] =
                    data_fd2_battle_runtime_char_array_ptr[source]
                        .inventory_slots[slot_iter * 2 + 1];
                inv_count++;
            }
        }

        if (inv_count == 0) {
            data_fd2_dialog_last_action_sprite_id_param =
                data_fd2_battle_runtime_char_array_ptr[
                    data_fd2_ui_menu_cursor_idx].char_id + 1;
            fd2_load_chapter_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                    data_fd2_chapter_intro_menu_cursor_state]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1ff,
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(1);
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        data_fd2_ui_menu_cursor_idx = 0;
        data_fd2_ui_menu_scroll_offset = 0;
        fd2_open_shop_dialog_panel(inv_count, inv_list, 1);
        data_fd2_ui_menu_visible_item_count = inv_count;
        item_select = fd2_shop_menu_input_loop(inv_count, inv_list, 1);
        selected_slot = data_fd2_ui_menu_cursor_idx;
        if (item_select == -1) {
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        fd2_close_intro_dialog_with_slide_out();
        fd2_load_chapter_portrait(
            data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                data_fd2_chapter_intro_menu_cursor_state]);
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1fe,
            0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        fd2_wait_for_input_dialog_with_blink(1);
        fd2_close_intro_dialog_with_slide_out();
        tgt_select = fd2_party_roster_single_select_loop();
        fd2_close_intro_dialog_with_slide_out();
        if (tgt_select == -1) {
            continue;
        }

        if (fd2_count_usable_inventory_slots(data_fd2_ui_menu_cursor_idx) == 8) {
            data_fd2_dialog_last_action_sprite_id_param =
                data_fd2_battle_runtime_char_array_ptr[
                    data_fd2_ui_menu_cursor_idx].char_id + 1;
            fd2_load_chapter_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                    data_fd2_chapter_intro_menu_cursor_state]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                data_fd2_dialog_shop_inventory_full_dialog_text_id_table[
                    data_fd2_chapter_intro_menu_cursor_state],
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
            fd2_wait_for_input_dialog_with_blink(1);
            fd2_close_intro_dialog_with_slide_out();
            continue;
        }

        give_item_id = fd2_get_inventory_slot_item_id(source, selected_slot);
        fd2_remove_inventory_slot_at(source, selected_slot);
        fd2_add_item_to_inventory(data_fd2_ui_menu_cursor_idx, give_item_id);
        fd2_recalculate_combat_stats(source);
    }
}

/* ----------------------------------------------------------------
 * data_fd2_dialog_last_action_sprite_id_param @ 0x53AD9  (.object2, uint32)
 *
 * Transient dialog-VM parameter: the FDTXT page/text index used by the
 * dialog interpreter (fd2_display_dialog_scene @ 0x16067) when it hits the
 * -4 RECURSIVE opcode (it loads this and re-enters itself with it as the
 * page_idx). Writers across the shop / give / promote / revive / battle-drop
 * / tile-event paths set it (typically item_id+0xB5, portrait_id+1, or
 * char_id+1) immediately before invoking the dialog scene, so first access
 * on every code path is a write -- the binary image is statically zero, so
 * this is a zero-initialized (BSS) scalar, not a constant table.
 * ---------------------------------------------------------------- */
uint32 data_fd2_dialog_last_action_sprite_id_param;

/* ----------------------------------------------------------------
 * data_fd2_ui_menu_scroll_offset @ 0x5412F  (.object2, uint32)
 *
 * Shared menu-scroll state: the top-row index of the 6-item visible
 * viewport in every chapter-intro roster/shop/promote grid (scrolls in
 * steps of 2 for the 2-wide buy/sell/give/roster grids, steps of 1 for
 * the 1-wide promote lists). Every menu loop that uses it writes 0 to it
 * during setup before the first read (fd2_shop_menu_input_loop,
 * fd2_party_roster_single_select_loop, fd2_party_roster_class_select_loop,
 * fd2_promote_member(s)_select_loop, the chapter-intro dialog loops);
 * fd2_run_buy_item_menu reloads it from data_fd2_ui_menu_saved_scroll_offset
 * and writes it back to persist across panel re-opens. Comparisons against
 * the cursor index are signed (the readers cast to int32 at the use site:
 * MOV/SUB DWORD + JL/JGE in the binary), but the stored value is always a
 * small non-negative viewport index, so it is declared uint32 to match the
 * sibling menu-state globals (cursor_idx / visible_item_count /
 * saved_cursor_idx / saved_scroll_offset). The binary image is statically
 * zero and first access on every path is a write, so this is a
 * zero-initialized (BSS) scalar, not a constant.
 * ---------------------------------------------------------------- */
uint32 data_fd2_ui_menu_scroll_offset;
