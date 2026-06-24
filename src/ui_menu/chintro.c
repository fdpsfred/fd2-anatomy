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
 * greeting dialog (idx 0x1F5 when chapter_transition_state == 1, else 0x1B8).
 *
 * Main loop: restore the saved cursor, play the open animation, run the 4-way
 * input loop, save the cursor, re-read the chapter roster into a local 12-byte
 * buffer, play the close animation; on commit dispatch by cursor (0=buy,
 * 1=sell, 2=equip, else=give) and re-show the greeting (idx 0x1F7 when
 * chapter_transition_state == 1, else 0x1B8). Loops while the input returned
 * commit (1); cancel (-1) exits.
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
    fd2_blit_indexed_sprite_rle(0xa0000, 0x140, atlas, 0);
    fd2_play_palette_fade_in();
    fd2_delay_ms(200);
    fd2_dialog_open_speaker_portrait(
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
            fd2_dialog_open_speaker_portrait(
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

    fd2_blit_indexed_sprite_rle(0xa0000, 0x140,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0);
    fd2_delay_ms(200);
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

/* ----------------------------------------------------------------
 * fd2_run_chapter_intro_menu_typeB @ 0x2FC85  (1 caller:
 *   fd2_chapter_transition_with_intro @ 0x2D093)
 *
 * Chapter-intro menu TYPE B — the between-chapters menu used for non-shop
 * (battle-only) chapters, i.e. when chapter_transition_state == 0. Same
 * orchestrator shape as fd2_run_chapter_intro_menu_main but with a fixed
 * BG-image idx and a different 4-way dispatch (Status / Save / Load /
 * Begin-battle). Returns 1 when the player confirms starting the battle
 * (cursor 3 -> "begin battle?" -> yes), else 0.
 *
 * The caller passes snapshot_buf = the previously-allocated 64000-byte
 * backdrop snapshot used by the fade-to-black / scaled-pose outro animation
 * (owned/freed by the caller).
 *
 * Setup: read the chapter-meta byte (chapter category) at entry — captured
 * before the menu loop because the pose tables are indexed by
 * chapter_category*6 + cursor_state in the exit animation; load the fixed
 * FDOTHER BG image (idx 0x0D), fade it in, paint the speaker portrait
 * (portrait_id_table[0]), then show the greeting dialog (idx 0x249).
 *
 * Main loop: restore the saved cursor, play the open animation, run the 4-way
 * input loop, save the cursor, play the outro animation, then unconditionally
 * close the dialog with the slide-out (fd2_close_intro_dialog_with_slide_out,
 * every iteration); on commit dispatch by cursor:
 *   0  fd2_run_status_screen_member_menu()      // member status screen
 *   1  fd2_save_current_state_to_slot(1)         // save slot UI
 *   2  fd2_load_state_from_selected_slot()       // load slot UI (may not return)
 *   3  "begin battle?" confirmation: show portrait[0] + dialog 0x19F, run the
 *      typewriter loop + page-advance collapse; if the typewriter returned a
 *      non-cancel (!= -1) AND the confirm cursor landed on 0 (yes), show the
 *      "battle starting" dialog 0x1A0, delay, slide-out and return 1.
 * After a non-battle action it re-shows the greeting (idx 0x24A). Loops while
 * the input returned commit (1); cancel (-1) exits.
 *
 * Exit: blit the BG image, fade to black, then an 11-frame pose-out animation
 * (iVar5 = 10..0) that nearest-neighbour-scales snapshot_buf via
 * fd2_blit_scaled_chapter_pose, commits each frame from the large game-state
 * buffer, and ramps brightness; finally free the BG atlas and return 0.
 *
 * uint32 __cdecl, 1 stack arg (snapshot_buf), with the __CHK(0x3C) stack-probe
 * prologue (compiler-injected, not part of the source). The chapter-meta byte
 * is read once via EAX-after-CALL (verified against the assembly:
 * CALL fd2_get_chapter_intro_metadata_entry; MOV AL,[EAX]). The pose-out
 * arithmetic is identical to fd2_run_chapter_intro_menu_main: src_cx from the
 * y-row table (-0x96, +0x5000), src_cy from the x-column table (-100, +0x3200),
 * both scaled by iVar5/10*0x80; the pose-table index is cursor_state +
 * chapter_meta_byte*6. The dispatch / cursor-wrap bound tests live in the
 * shared fd2_chapter_intro_menu_input_loop (signed). The "begin battle"
 * sub-prompt reuses data_fd2_ui_menu_cursor_idx as its yes/no cursor (0 = yes).
 * ---------------------------------------------------------------- */
uint32 fd2_run_chapter_intro_menu_typeB(uint32 snapshot_buf)
{
    uint8 *chapter_meta;
    uint8 chapter_meta_byte;
    uint32 atlas;
    uint32 stored_cursor;
    int sel;
    int typewriter_ret;
    int iVar5;
    int table_off;

    chapter_meta = fd2_get_chapter_intro_metadata_entry(
                       data_fd2_chapter_current_chapter_id);
    chapter_meta_byte = *chapter_meta;

    atlas = fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0x0d);
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = atlas;
    fd2_blit_indexed_sprite_rle(0xa0000, 0x140, atlas, 0);
    fd2_play_palette_fade_in();
    fd2_delay_ms(200);
    fd2_dialog_open_speaker_portrait(
        data_fd2_chapter_intro_menu_speaker_portrait_id_table[0]);
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x249, 0xa94cc, 0x140,
        0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);

    stored_cursor = 0;
    do {
        data_fd2_ui_menu_cursor_idx = stored_cursor;
        fd2_animate_tutorial_dialog_intro_or_outro(0);
        sel = fd2_chapter_intro_menu_input_loop();
        if ((sel & 0xff) == 1) {
            stored_cursor = data_fd2_ui_menu_cursor_idx;
        }
        fd2_animate_tutorial_dialog_intro_or_outro(1);
        fd2_close_intro_dialog_with_slide_out();
        if ((sel & 0xff) == 1) {
            if (data_fd2_ui_menu_cursor_idx == 0) {
                fd2_run_status_screen_member_menu();
            } else if (data_fd2_ui_menu_cursor_idx == 1) {
                fd2_save_current_state_to_slot(1);
            } else if (data_fd2_ui_menu_cursor_idx == 2) {
                fd2_load_state_from_selected_slot();
            } else {
                fd2_dialog_open_speaker_portrait(
                    data_fd2_chapter_intro_menu_speaker_portrait_id_table[0]);
                fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x19f,
                    0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
                fd2_paint_portrait_to_dialog_area(0);
                typewriter_ret = fd2_text_dialog_typewriter_loop();
                fd2_animate_dialog_page_advance_collapse();
                if (typewriter_ret != -1 && data_fd2_ui_menu_cursor_idx == 0) {
                    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x1a0,
                        0xaac8c, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
                    fd2_delay_ms(200);
                    fd2_close_intro_dialog_with_slide_out();
                    return 1;
                }
                fd2_close_intro_dialog_with_slide_out();
            }
            fd2_dialog_open_speaker_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[0]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x24a, 0xa94cc,
                0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
        }
    } while ((sel & 0xff) == 1);

    fd2_blit_indexed_sprite_rle(0xa0000, 0x140,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0);
    fd2_delay_ms(200);
    fd2_play_palette_fade_to_black();
    for (iVar5 = 10; iVar5 >= 0; iVar5--) {
        table_off = (int)data_fd2_chapter_intro_menu_cursor_state +
                    (int)chapter_meta_byte * 6;
        fd2_blit_scaled_chapter_pose(
            (uint32)((((int)data_fd2_chapter_intro_portrait_pose_y_row_table[
                          table_off] - 0x96) * iVar5 / 10) * 0x80 + 0x5000),
            (uint32)((((int)data_fd2_chapter_intro_portrait_pose_x_column_table[
                          table_off] - 100) * iVar5 / 10) * 0x80 + 0x3200),
            snapshot_buf, iVar5 * -9 + 0x80);
        memmove((void *)0xa0000,
                (void *)data_fd2_large_game_state_buffer_ptr, 64000);
        fd2_set_vga_palette_range(0, 0xff, (uint32)(iVar5 << 2));
    }

    free((void *)data_fd2_ui_menu_screen_sprite_atlas_buf_ptr);
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_run_chapter_intro_menu_typeC @ 0x3072F  (1 caller:
 *   fd2_chapter_transition_with_intro @ 0x2D093)
 *
 * Chapter-intro menu TYPE C — the "town" chapter services menu, used when
 * chapter_transition_state == 4 (mid-game checkpoint towns). Same orchestrator
 * shape as fd2_run_chapter_intro_menu_main / typeB but with a fixed BG-image
 * idx (FDOTHER 0x0E) and a 4-way dispatch of pure services:
 *   0  fd2_run_status_screen_member_menu()    // 状態 member status screen
 *   1  fd2_run_give_item_menu()               // 贈   give item between members
 *   2  fd2_run_revive_menu_main()             // 復活 church revive (clear bFlags,
 *                                                     HP_current = HP_max, per-fee)
 *   3  fd2_run_class_promotion_menu_main()    // 轉職 class change (per-class fee,
 *                                                     level >= 0x14 + key item)
 * There is no "begin battle" branch — typeC is a pure-services menu and always
 * returns 0 (exits only via cancel back to the chapter-transition flow).
 *
 * The caller passes pose_bitmap = the previously-allocated 64000-byte backdrop
 * snapshot used by the fade-to-black / scaled-pose outro animation (owned/freed
 * by the caller).
 *
 * Setup: read the chapter-meta byte (chapter category) at entry — captured
 * before the menu loop because the pose tables are indexed by
 * chapter_category*6 + cursor_state in the exit animation; load the fixed
 * FDOTHER BG image (idx 0x0E), fade it in, paint the speaker portrait
 * (portrait_id_table[4]); then render the money panel both live (panel sprite
 * at atlas[+0x0A], blit to 0xA76C5 + decimal at 0xA7BD0) and into the
 * slide-snapshot shadow buffer (render_workspace_b + 0x76C5 / + 0x7BD0), then
 * show the greeting dialog (idx 0x249). Both decimal renders use x=0x1F, 8
 * digits.
 *
 * Main loop: restore the saved cursor, play the open animation, run the shared
 * 4-way input loop, save the cursor on commit, play the outro animation, then
 * unconditionally close the dialog with the slide-out
 * (fd2_close_intro_dialog_with_slide_out, every iteration); on commit dispatch
 * by cursor (0=status, 1=give, 2=revive, 3=promote) and re-show the greeting
 * (idx 0x24A). Loops while the input returned commit (1); cancel (-1) exits.
 *
 * Exit: blit the BG image, fade to black, then an 11-frame pose-out animation
 * (iVar5 = 10..0) that nearest-neighbour-scales pose_bitmap via
 * fd2_blit_scaled_chapter_pose, commits each frame from the large game-state
 * buffer, and ramps brightness; finally free the BG atlas and return 0.
 *
 * uint32 __cdecl, 1 stack arg (pose_bitmap), with the __CHK(0x3C) stack-probe
 * prologue (compiler-injected, not part of the source). The chapter-meta byte
 * is read once via EAX-after-CALL (verified against the assembly:
 * CALL fd2_get_chapter_intro_metadata_entry; MOV AL,[EAX]). The dispatch's
 * cursor==1 test is emitted from CMP EAX,[cursor] where EAX holds the committed
 * result constant 1 (verified against the assembly; identical to _main's sell
 * branch). The pose-out arithmetic is byte-for-byte identical to
 * fd2_run_chapter_intro_menu_main: src_cx from the y-row table (-0x96, +0x5000),
 * src_cy from the x-column table (-100, +0x3200), both scaled by iVar5/10*0x80;
 * the pose-table index is cursor_state + chapter_meta_byte*6. The cursor-wrap /
 * commit / cancel decision logic lives in the shared
 * fd2_chapter_intro_menu_input_loop (signed bound tests). Tail-jumps into the
 * shared stack-cleanup epilogue inside fd2_run_chapter_intro_menu_main (at
 * 0x2FF84: free atlas; atlas = NULL; return 0); emitted here as the equivalent
 * explicit free + return 0 (Watcom regenerates the epilogue).
 * ---------------------------------------------------------------- */
uint32 fd2_run_chapter_intro_menu_typeC(uint32 pose_bitmap)
{
    uint8 *chapter_meta;
    uint8 chapter_meta_byte;
    uint32 atlas;
    uint8 *money_panel_sprite;
    uint32 stored_cursor;
    int sel;
    int iVar5;
    int table_off;

    chapter_meta = fd2_get_chapter_intro_metadata_entry(
                       data_fd2_chapter_current_chapter_id);
    chapter_meta_byte = *chapter_meta;

    atlas = fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0x0e);
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = atlas;
    fd2_blit_indexed_sprite_rle(0xa0000, 0x140, atlas, 0);
    fd2_play_palette_fade_in();
    fd2_delay_ms(200);
    fd2_dialog_open_speaker_portrait(
        data_fd2_chapter_intro_menu_speaker_portrait_id_table[4]);

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

    fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x249, 0xa94cc, 0x140,
        0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);

    stored_cursor = 0;
    do {
        data_fd2_ui_menu_cursor_idx = stored_cursor;
        fd2_animate_tutorial_dialog_intro_or_outro(0);
        sel = fd2_chapter_intro_menu_input_loop();
        if ((sel & 0xff) == 1) {
            stored_cursor = data_fd2_ui_menu_cursor_idx;
        }
        fd2_animate_tutorial_dialog_intro_or_outro(1);
        fd2_close_intro_dialog_with_slide_out();
        if ((sel & 0xff) == 1) {
            if (data_fd2_ui_menu_cursor_idx == 0) {
                fd2_run_status_screen_member_menu();
            } else if (data_fd2_ui_menu_cursor_idx == 1) {
                fd2_run_give_item_menu();
            } else if (data_fd2_ui_menu_cursor_idx == 2) {
                fd2_run_revive_menu_main();
            } else if (data_fd2_ui_menu_cursor_idx == 3) {
                fd2_run_class_promotion_menu_main();
            }
            fd2_dialog_open_speaker_portrait(
                data_fd2_chapter_intro_menu_speaker_portrait_id_table[4]);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x24a, 0xa94cc,
                0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
            fd2_paint_portrait_to_dialog_area(0);
        }
    } while ((sel & 0xff) == 1);

    fd2_blit_indexed_sprite_rle(0xa0000, 0x140,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr, 0);
    fd2_delay_ms(200);
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
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_party_roster_single_select_loop @ 0x2E6B8  (5 callers:
 *   fd2_run_buy_item_menu, fd2_run_sell_item_menu, fd2_run_equip_member_menu,
 *   fd2_run_give_item_menu, fd2_run_status_screen_member_menu)
 *
 * Party-roster member selection menu loop. Opens a 2-column x 3-row party
 * grid panel (allocates the 3 shared workspace buffers, slides the panel in,
 * runs the input loop) and returns the selection:
 *   returns  1 = user committed (Enter 0x1C / Space 0x39)
 *   returns -1 = user cancelled (Esc 0x01)
 * Used to pick a character for the buy / sell / equip / give / status screens.
 *
 * Setup: allocate render_workspace_a/b/c (3 x 64000); snapshot the live VGA
 * framebuffer (0xA0000) into render_workspace_b, clone it into render_workspace_c;
 * reset scroll-offset and cursor to 0; blit the panel header sprite (atlas entry
 * at atlas[+0x46]) into render_workspace_c + 0x8C05; render the initial grid; then
 * a 6-frame slide-down (frame 5..0, panel y = 0x70 + frame*0xD).
 *
 * Input loop: poll fd2_wait_input_with_chapter_dialog_blink(3) (mode 3 = roster
 * grid + cursor) and dispatch on the scancode. The four arrows move the cursor
 * within 0..count-1 (count = menu_party_member_count), play the cursor chime
 * (SFX id 0 from the FDOTHER bank), page the 6-item viewport in steps of 2
 * (scroll up when cursor - scroll_offset > 5, scroll down when cursor <
 * scroll_offset, each with the matching scroll animation), and re-render the
 * grid live to 0xA0000. Right/Down share the cursor+/page-up tail (LAB at the
 * "cursor - scroll > 5" test); Left/Up share the cursor-/page-down tail (LAB at
 * the "cursor < scroll" test). Enter/Space commit (1); Esc cancels (-1). Loops
 * while result == 0.
 *
 * int __cdecl, void params, ESI = result (callee-saved). The __CHK(0x18)
 * stack-probe prologue is compiler-injected and not part of the source. The
 * wait-input function returns a zero-extended byte scancode in EAX, so the
 * full-width scancode compares match the disassembly. The bound tests are
 * signed (JGE/JLE/JL), so the unsigned cursor/count/scroll globals are cast to
 * int. Buffer cleanup is performed by the caller via
 * fd2_close_intro_dialog_with_slide_out @ 0x2D31B (this fn opens; the companion
 * closes -- same 3-buffer state shared).
 * ---------------------------------------------------------------- */
int fd2_party_roster_single_select_loop(void)
{
    int result;
    int scancode;
    uint32 frame_iter;

    result = 0;
    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);
    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, (void *)0xa0000,
            64000);
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_cursor_idx = 0;
    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_composed_target_buf_ptr + 0x8c05,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr +
            *(int32 *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0x46),
        0x140);
    fd2_render_party_roster_grid(data_fd2_ui_menu_cursor_idx,
                                 data_fd2_ui_slide_composed_target_buf_ptr);
    for (frame_iter = 5; (int)frame_iter >= 0; frame_iter--) {
        fd2_slide_panel_down_step(frame_iter * 0xd + 0x70,
                                  data_fd2_ui_slide_anim_accumulator_buf_ptr,
                                  data_fd2_ui_slide_composed_target_buf_ptr);
    }

    do {
        scancode = fd2_wait_input_with_chapter_dialog_blink(3);
        if (scancode == 0x4d) {
            if (data_fd2_shared_menu_party_member_count - 1 !=
                data_fd2_ui_menu_cursor_idx) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1;
LAB_page_up_check:
                if ((int)(data_fd2_ui_menu_cursor_idx -
                          data_fd2_ui_menu_scroll_offset) > 5) {
                    data_fd2_ui_menu_scroll_offset =
                        data_fd2_ui_menu_scroll_offset + 2;
                    fd2_animate_scroll_up_in_shop_dialog();
                }
LAB_rerender:
                fd2_render_party_roster_grid(data_fd2_ui_menu_cursor_idx,
                                             0xa0000);
            }
        } else if (scancode == 0x4b) {
            if (data_fd2_ui_menu_cursor_idx != 0) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 1;
LAB_page_down_check:
                if ((int)data_fd2_ui_menu_cursor_idx <
                    (int)data_fd2_ui_menu_scroll_offset) {
                    data_fd2_ui_menu_scroll_offset =
                        data_fd2_ui_menu_scroll_offset - 2;
                    fd2_animate_scroll_down_in_shop_dialog();
                }
                goto LAB_rerender;
            }
        } else if (scancode == 0x48) {
            if ((int)data_fd2_ui_menu_cursor_idx > 1) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 2;
                goto LAB_page_down_check;
            }
        } else if (scancode == 0x50) {
            if ((int)data_fd2_ui_menu_cursor_idx <
                (int)data_fd2_shared_menu_party_member_count - 2) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 2;
                goto LAB_page_up_check;
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
 * fd2_party_roster_class_select_loop @ 0x2E8CF  (1 caller:
 *   fd2_run_buy_item_menu @ 0x2F0B0)
 *
 * Class-filtered party-roster select loop with a stat preview. Opens a
 * single-column panel that shows only the chars who can equip a given item,
 * each row previewing the stats they would have if they equipped it, and
 * returns the selection:
 *   returns  1 = user committed (Enter 0x1C / Space 0x39)
 *   returns -1 = user cancelled (Esc 0x01)
 * Called from the buy-item menu's "give the bought item to someone" sub-prompt.
 *
 * Three stack args: candidate_count (the number of equip-capable chars),
 * candidate_array_ptr (pointer to the candidate char-id byte array — the
 * caller's eligible_chars[]), and item_id (the shop item id; the renderer's
 * preview path resolves it to an item-effect entry). All three are forwarded
 * positionally to the renderer fd2_render_party_roster_with_item_stat_preview
 * each draw.
 *
 * Setup: allocate render_workspace_a/b/c (3 x 64000); snapshot the live VGA
 * framebuffer (0xA0000) into render_workspace_b, clone it into
 * render_workspace_c; reset scroll-offset and cursor to 0; blit the panel
 * header sprite (atlas entry at atlas[+0x46]) into render_workspace_c + 0x8C05;
 * render the initial roster into render_workspace_c; then a 6-frame slide-down
 * (frame 5..0, panel y = 0x70 + frame*0xD).
 *
 * Input loop: poll fd2_wait_input_with_chapter_dialog_blink(2) (mode 2 =
 * stat-preview roster) and dispatch on the scancode. Only Up/Down move (this is
 * a single-column 1-step nav, not the 2-column paged grid of
 * fd2_party_roster_single_select_loop):
 *   0x50 (Down)  if cursor != candidate_count-1: chime; cursor++;
 *                if cursor - scroll_offset > 2: scroll_offset++, scroll up anim;
 *                re-render live to 0xA0000
 *   0x48 (Up)    if cursor != 0: chime; cursor--;
 *                if cursor < scroll_offset: scroll_offset--, scroll down anim;
 *                re-render
 *   0x1C/0x39    commit  (result = 1)
 *   0x01 (Esc)   cancel  (result = -1)
 * The cursor chime is SFX id 0 from the FDOTHER bank. The 3-item viewport
 * scrolls in steps of 1 (finer than the grid's step-of-2 paging). Loops while
 * result == 0.
 *
 * int __cdecl, 3 stack args, ESI = result (callee-saved). The __CHK(0x28)
 * stack-probe prologue is compiler-injected and not part of the source. The
 * wait-input function returns a zero-extended byte scancode in EAX, so the
 * full-width scancode compares match the disassembly. The scroll bound tests
 * are signed (JL/JGE), so the unsigned cursor/scroll globals are cast to int.
 * Buffer cleanup is performed by the caller via
 * fd2_close_intro_dialog_with_slide_out @ 0x2D31B (this fn opens; the companion
 * closes — same 3-buffer state shared). Tail-jumps into the shared stack-cleanup
 * epilogue at 0x2D3F8 (MOV EAX,ESI; pop regs; ret); emitted here as a plain
 * return (Watcom regenerates the epilogue).
 * ---------------------------------------------------------------- */
int fd2_party_roster_class_select_loop(uint32 candidate_count,
                                       uint32 candidate_array_ptr, uint32 item_id)
{
    int result;
    int scancode;
    uint32 frame_iter;

    result = 0;
    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);
    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, (void *)0xa0000,
            64000);
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    data_fd2_ui_menu_scroll_offset = 0;
    data_fd2_ui_menu_cursor_idx = 0;
    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_composed_target_buf_ptr + 0x8c05,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr +
            *(int32 *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0x46),
        0x140);
    fd2_render_party_roster_with_item_stat_preview(
        candidate_count, candidate_array_ptr, item_id,
        data_fd2_ui_menu_cursor_idx,
        data_fd2_ui_slide_composed_target_buf_ptr);
    for (frame_iter = 5; (int)frame_iter >= 0; frame_iter--) {
        fd2_slide_panel_down_step(frame_iter * 0xd + 0x70,
                                  data_fd2_ui_slide_anim_accumulator_buf_ptr,
                                  data_fd2_ui_slide_composed_target_buf_ptr);
    }

    do {
        scancode = fd2_wait_input_with_chapter_dialog_blink(2);
        if (scancode == 0x50) {
            if (candidate_count - 1 != data_fd2_ui_menu_cursor_idx) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1;
                if ((int)(data_fd2_ui_menu_cursor_idx -
                          data_fd2_ui_menu_scroll_offset) > 2) {
                    data_fd2_ui_menu_scroll_offset =
                        data_fd2_ui_menu_scroll_offset + 1;
                    fd2_animate_scroll_up_in_shop_dialog();
                }
LAB_rerender:
                fd2_render_party_roster_with_item_stat_preview(
                    candidate_count, candidate_array_ptr, item_id,
                    data_fd2_ui_menu_cursor_idx, 0xa0000);
            }
        } else if (scancode == 0x48) {
            if (data_fd2_ui_menu_cursor_idx != 0) {
                fd2_play_sfx_with_handle(
                    data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
                data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 1;
                if ((int)data_fd2_ui_menu_cursor_idx <
                    (int)data_fd2_ui_menu_scroll_offset) {
                    data_fd2_ui_menu_scroll_offset =
                        data_fd2_ui_menu_scroll_offset - 1;
                    fd2_animate_scroll_down_in_shop_dialog();
                }
                goto LAB_rerender;
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
 * Global state owned by this translation unit.
 *
 * data_fd2_ui_menu_cursor_idx @ 0x53C57 (uint32, 4 bytes, zero-init)
 *   Shared current-selection cursor index for the active modal menu
 *   (field command menu, options, settings, shop/inventory/spell/promote
 *   pickers, chapter-intro menu, etc). Runtime state: every menu-open
 *   site stores 0 here first, then key handlers bump it (e.g.
 *   fd2_settings_menu_input_step writes 0/1/2/3 for Home/Up/Left/Down,
 *   accessed as dword), and dispatch code reads it back. Never preset in
 *   the image; the linker places it in BSS (load-time zeroed).
 * ---------------------------------------------------------------- */
uint32 data_fd2_ui_menu_cursor_idx;

/* ----------------------------------------------------------------
 * data_fd2_ui_menu_screen_sprite_atlas_buf_ptr @ 0x54147
 *   (uint32 holding a heap pointer, 4 bytes, zero-init)
 *
 *   Pointer to the chapter-intro / menu sprite atlas buffer loaded on
 *   demand from FDOTHER.DAT. The CONTINUE / chapter-intro /
 *   chapter-transition flows assign it the malloc'd buffer returned by
 *   fd2_load_dat_resource(FDOTHER, ..., entry), then later free() it and
 *   store 0 back. The atlas entry index depends on the screen: the CONTINUE
 *   dispatcher and chapter-transition menu use 0x0D; the chapter-intro menus
 *   reuse this same pointer with their own entries (main: 0x0C/0x1D/0x3F by
 *   cursor state, typeB: 0x0D, typeC: 0x0E). Write/free sites
 *   (e.g. fd2_main_menu_dispatcher @ 0x25F5D /
 *   0x260CF, fd2_chapter_transition_menu @ 0x2CCAF,
 *   fd2_run_chapter_intro_menu_main @ 0x2E3A7 / 0x2E694,
 *   fd2_run_chapter_intro_menu_typeB @ 0x2FCC1 / 0x2FF92,
 *   fd2_run_chapter_intro_menu_typeC @ 0x3076B).
 *
 *   Many render helpers read it as a base pointer and parse the atlas
 *   header (e.g. fd2_blit_money_digit_sprite @ 0x2D636:
 *   MOV EBX,[0x54147]; ADD EBX,[EBX+0x0E]; ADD EBX,4 -- sprite-record
 *   offset table at +0x0E). Stored as uint32 (the project convention for
 *   runtime heap-pointer globals, cast to void* at the malloc/free sites);
 *   never preset in the image, so the linker places it in BSS
 *   (load-time zeroed).
 * ---------------------------------------------------------------- */
uint32 data_fd2_ui_menu_screen_sprite_atlas_buf_ptr;

/* ----------------------------------------------------------------
 * data_fd2_ui_menu_saved_cursor_idx @ 0x5414B (uint32, 4 bytes, zero-init)
 *   Persisted shop-menu cursor index, the saved companion to the live
 *   data_fd2_ui_menu_cursor_idx (0x53C57). fd2_run_buy_item_menu copies
 *   it into the live cursor before each panel re-open and snapshots the
 *   live cursor back into it after the input loop, so the highlight
 *   survives across the open/select/close dialog round trips
 *   (0x2F1B0: MOV EAX,[0x5414B]; MOV [0x53C57],EAX  and the reverse at
 *   0x2F1E9). The saved index is then used to fetch the chosen item id
 *   from the shop item-id array, e.g. 0x2F206:
 *   MOV EAX,[0x5414B]; MOVZX EBX,byte ptr [EAX+EBP*1] -- accessed as a
 *   full dword and treated as an unsigned offset into the byte array.
 *   fd2_run_chapter_intro_menu_main resets it on menu entry
 *   (0x2E4A1: MOV dword ptr [0x5414B],0x0). Never preset in the image,
 *   so the linker places it in BSS (load-time zeroed).
 * ---------------------------------------------------------------- */
uint32 data_fd2_ui_menu_saved_cursor_idx;

/* ----------------------------------------------------------------
 * data_fd2_ui_menu_saved_scroll_offset @ 0x5414F (uint32, 4 bytes, zero-init)
 *   Persisted shop-menu scroll offset, the saved companion to the live
 *   data_fd2_ui_menu_scroll_offset (0x5412F). It pairs with the saved
 *   cursor index above: fd2_run_buy_item_menu copies it into the live
 *   scroll offset before each panel re-open and snapshots the live
 *   offset back into it after the input loop, so the visible scroll
 *   window survives across the open/select/close dialog round trips
 *   (0x2F1BA: MOV EAX,[0x5414F]; MOV [0x5412F],EAX  and the reverse at
 *   0x2F1F3: MOV EAX,[0x5412F]; MOV [0x5414F],EAX -- accessed as a full
 *   dword in every reference, never a byte/word slice).
 *   fd2_run_chapter_intro_menu_main resets it on menu entry alongside the
 *   saved cursor (0x2E4AB: MOV dword ptr [0x5414F],0x0). Never preset in
 *   the image, so the linker places it in BSS (load-time zeroed).
 * ---------------------------------------------------------------- */
uint32 data_fd2_ui_menu_saved_scroll_offset;
