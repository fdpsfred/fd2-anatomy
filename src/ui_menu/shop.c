/*
 * shop.c — Shop / give-item screen: panel open animation + navigation loop.
 *
 * fd2_open_shop_dialog_panel @ 0x2E0BD (3 callers: fd2_run_buy_item_menu,
 *   fd2_run_sell_item_menu, fd2_run_give_item_menu)
 * fd2_shop_menu_input_loop @ 0x2DF6B (3 callers: fd2_run_buy_item_menu,
 *   fd2_run_sell_item_menu, fd2_run_give_item_menu)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <string.h>

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
int fd2_shop_menu_input_loop(uint32 param_1, uint32 param_2, uint32 param_3)
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
void fd2_open_shop_dialog_panel(uint32 item_count, uint32 item_id_array,
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
