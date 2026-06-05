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
