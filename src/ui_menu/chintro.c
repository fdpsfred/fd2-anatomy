/*
 * chintro.c — Chapter-intro menu input loops
 */

#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_chapter_intro_menu_input_loop @ 0x2D7BD  (3 callers:
 *   fd2_run_chapter_intro_menu_main, fd2_run_chapter_intro_menu_typeB,
 *   fd2_run_chapter_intro_menu_typeC)
 *
 * Chapter-intro 4-way menu input loop. Blocks until commit / cancel:
 *   returns  1 = user committed (Enter / Space)
 *   returns -1 = user cancelled (Esc)
 *
 * Per iteration it polls fd2_wait_input_with_chapter_dialog_blink(0)
 * (which blinks the dialog cursor while waiting) and dispatches on the
 * returned scancode:
 *   0x4B (Left)        play cursor chime; cursor--; wrap < 0 -> 3
 *   0x4D (Right)       play cursor chime; cursor++; wrap > 3 -> 0
 *   0x1C/0x39 (Ent/Sp) commit  (result = 1)
 *   0x01 (Esc)         cancel  (result = -1)
 *   else               keep looping
 * The cursor is data_fd2_ui_menu_cursor_idx (0..3) and the cursor chime
 * is SFX id 0 in the FDOTHER UI bank. Loops while result == 0.
 *
 * int __cdecl with the __CHK(0x14) stack-probe prologue. The cursor
 * Left/Right wrap tests are signed (JGE / JLE), so the unsigned global is
 * cast to int for the bound checks, matching the disassembly.
 * ---------------------------------------------------------------- */
int fd2_chapter_intro_menu_input_loop(void)
{
    int result;
    int scancode;

    result = 0;
    do {
        scancode = fd2_wait_input_with_chapter_dialog_blink(0);
        if (scancode == 0x4b) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 1;
            if ((int)data_fd2_ui_menu_cursor_idx < 0) {
                data_fd2_ui_menu_cursor_idx = 3;
            }
        } else if (scancode == 0x4d) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1;
            if ((int)data_fd2_ui_menu_cursor_idx > 3) {
                data_fd2_ui_menu_cursor_idx = 0;
            }
        } else if (scancode == 0x1c || scancode == 0x39) {
            result = 1;
        } else if (scancode == 0x01) {
            result = -1;
        }
    } while (result == 0);

    return result;
}

/* ----------------------------------------------------------------
 * fd2_run_chapter_intro_menu_main @ 0x2E341  (1 caller:
 *   fd2_chapter_transition_with_intro @ 0x2D093)
 *
 * Generic CHAPTER INTRO menu — the BG-image + dialog selection screen
 * between chapters (buy / sell / equip / give item, plus continue-to-next
 * chapter). The default chapter-intro menu, used for chapter_transition_state
 * 3, 5, and "others" (state 0 -> typeB, state 4 -> typeC, state 2 -> no intro
 * menu). Returns 1 if chapter_transition_state was 0 (start-game flag), else 0.
 *
 * Setup: pick the FDOTHER BG-image idx by state (3->0x1D, 5->0x3F, else 0x0C),
 * load + fade it in, paint the speaker portrait, render the money panel
 * (live at 0xA76C5 and into the slide-snapshot shadow buffer), then show the
 * greeting dialog (idx 0x1F5 for chapter 1, else 0x1B8).
 *
 * Main loop: restore the saved cursor, play the open animation, run the 4-way
 * input loop, save the cursor, re-read the chapter roster into a local 12-byte
 * buffer, play the close animation; on commit dispatch by cursor (0=buy,
 * 1=sell, 2=equip, else=give) and re-show the greeting (idx 0x1F7 for chapter
 * 1, else 0x1B8). Loops while the input returned commit (1); cancel (-1) exits.
 *
 * Exit: blit the BG image, fade to black, then an 11-frame pose-out animation
 * (iVar5 = 10..0) that nearest-neighbour-scales pose_bitmap via
 * fd2_blit_scaled_chapter_pose, commits each frame from the large game-state
 * buffer, and ramps brightness; finally free the BG atlas and return the
 * start-game flag. (pose_bitmap is owned/freed by the caller.)
 *
 * uint32 __cdecl, 1 stack arg (pose_bitmap). Tail-jumps into the shared
 * stack-cleanup epilogue at 0x2D9F6 (frame layout identical to
 * fd2_wait_input_with_chapter_dialog_blink); emitted here as a plain return
 * (Watcom regenerates the epilogue).
 *
 * Note: the chapter-meta byte read at entry (chapter category) is captured
 * before the menu loop because the pose tables are indexed by
 * chapter_category*6 + cursor_state in the exit animation. The shadow money
 * panel is rendered into render_workspace_b + 0x76C5 / + 0x7BD0. Both decimal
 * renders use x=0x1F, 8 digits.
 * ---------------------------------------------------------------- */
uint32 fd2_run_chapter_intro_menu_main(uint32 pose_bitmap)
{
    uint8 *chapter_meta;
    uint8 chapter_meta_byte;
    uint32 fdother_idx;
    uint32 atlas;
    uint8 *money_panel_sprite;
    uint32 greeting_idx;
    uint32 stored_cursor;
    uint32 return_code;
    int sel;
    int roster_count;
    int iVar5;
    int table_off;
    uint8 party_roster[12];

    stored_cursor = 0;
    return_code = 0;

    chapter_meta = fd2_get_chapter_intro_metadata_entry(
                       data_fd2_chapter_current_chapter_id);
    chapter_meta_byte = *chapter_meta;

    if (data_fd2_chapter_intro_menu_cursor_state == 3) {
        fdother_idx = 0x1d;
    } else if (data_fd2_chapter_intro_menu_cursor_state == 5) {
        fdother_idx = 0x3f;
    } else {
        fdother_idx = 0x0c;
    }

    atlas = fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, fdother_idx);
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = atlas;
    fd2_blit_indexed_sprite_at_xy(0xa0000, 0x140, atlas, 0);
    fd2_play_palette_fade_in();
    __delay_thunk_375b2(200);
    fd2_load_chapter_portrait(
        data_fd2_chapter_intro_menu_speaker_portrait_id_table[
            data_fd2_chapter_intro_menu_cursor_state]);

    money_panel_sprite = (uint8 *)(*(int32 *)(atlas + 10) + atlas);
    fd2_dialog_sprite_blit_normal(0xa76c5, (uint32)money_panel_sprite, 0x140);
    fd2_render_decimal_number_to_buffer(0xa7bd0, 0x140,
        data_fd2_shared_party_total_gold, 0x1f, 8);
    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_bg_snapshot_buf_ptr + 0x76c5,
        (uint32)money_panel_sprite, 0x140);
    fd2_render_decimal_number_to_buffer(
        data_fd2_ui_slide_bg_snapshot_buf_ptr + 0x7bd0, 0x140,
        data_fd2_shared_party_total_gold, 0x1f, 8);

    if (data_fd2_chapter_intro_menu_cursor_state == 1) {
        greeting_idx = 0x1f5;
    } else {
        greeting_idx = 0x1b8;
    }
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, greeting_idx,
        0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);

    data_fd2_ui_menu_saved_cursor_idx = 0;
    data_fd2_ui_menu_saved_scroll_offset = 0;
    do {
        data_fd2_ui_menu_cursor_idx = stored_cursor;
        fd2_animate_tutorial_dialog_intro_or_outro(0);
        sel = fd2_chapter_intro_menu_input_loop();
        if ((sel & 0xff) == 1) {
            stored_cursor = data_fd2_ui_menu_cursor_idx;
        }
        fd2_animate_tutorial_dialog_intro_or_outro(1);
        roster_count = fd2_load_chapter_party_roster(party_roster);
        data_fd2_ui_menu_visible_item_count = (uint32)(roster_count & 0xff);
        fd2_close_intro_dialog_with_slide_out();
        if ((sel & 0xff) == 1) {
            if (data_fd2_ui_menu_cursor_idx == 0) {
                fd2_run_buy_item_menu((uint32)(roster_count & 0xff),
                                      party_roster);
            } else if (data_fd2_ui_menu_cursor_idx == 1) {
                fd2_run_sell_item_menu();
            } else if (data_fd2_ui_menu_cursor_idx == 2) {
                fd2_run_equip_member_menu();
            } else {
                fd2_run_give_item_menu();
            }
            fd2_load_chapter_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[
                    data_fd2_chapter_intro_menu_cursor_state]);
            if (data_fd2_chapter_intro_menu_cursor_state == 1) {
                greeting_idx = 0x1f7;
            } else {
                greeting_idx = 0x1b8;
            }
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, greeting_idx,
                0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
        }
    } while ((sel & 0xff) == 1);

    fd2_blit_indexed_sprite_at_xy(0xa0000, 0x140,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0);
    __delay_thunk_375b2(200);
    fd2_play_palette_fade_to_black();
    for (iVar5 = 10; iVar5 >= 0; iVar5--) {
        table_off = (int)data_fd2_chapter_intro_menu_cursor_state +
                    (int)chapter_meta_byte * 6;
        fd2_blit_scaled_chapter_pose(
            (uint32)((((int)data_fd2_chapter_intro_portrait_pose_y_row_table[
                          table_off] - 0x96) * iVar5 / 10) * 0x80 + 0x5000),
            (uint32)((((int)data_fd2_chapter_intro_portrait_pose_x_column_table[
                          table_off] - 100) * iVar5 / 10) * 0x80 + 0x3200),
            pose_bitmap, iVar5 * -9 + 0x80);
        memmove((void *)0xa0000,
                (void *)data_fd2_large_game_state_buffer_ptr, 64000);
        fd2_set_vga_palette_range(0, 0xff, (uint32)(iVar5 << 2));
    }

    free((void *)data_fd2_ui_menu_screen_sprite_atlas_buf_ptr);
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
    return_code = (uint32)(data_fd2_chapter_intro_menu_cursor_state == 0);
    return return_code;
}
