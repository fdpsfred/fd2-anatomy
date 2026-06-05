/*
 * aniend.c — GAME CLEAR ending cutscene + post-game clear-record menu.
 *
 * The post-victory lifecycle screen: plays the multi-phase ending cinematic
 * (title splash, mid ANI, vertical credit scroll with scripted SFX/palette
 * swaps, red fade-out), reveals the clear-status panel, reads FD2.SAV to
 * decide whether the menu has 2 or 3 options, then runs the up/down/commit
 * menu loop and returns the chosen index.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <dos.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * fd2_play_ending_and_record_clear @ 0x1F894  (1 caller)
 *
 * Sole caller: fd2_main_menu_continue_dispatcher @ 0x25EBB (entered when the
 * player selects "Continue" on the main menu and the save shows the final
 * chapter cleared).
 *
 * Phases (see plate @ 0x1F894 for full per-address breakdown):
 *   1  Setup: copy ending_music_trigger_frames[15] to stack.
 *   2  Title splash: FDOTHER.DAT[0x4A] sprite + [0x4C] palette, fade in/out.
 *   3  Mid ANI: ANI sequence 3, palette FDOTHER.DAT[0x63].
 *   4  Build 234KB scroll panel from FDOTHER.DAT[0x45..0x49] (5 segments).
 *   5  Scroll credits down from row 0x217..0; scripted cinematic inserts +
 *      music_trigger_frames-driven SFX/palette swaps; early-skip on keypress.
 *   6  Red-tint fade to black.
 *   7  Clear-status panel: FDOTHER.DAT[7] sprite + [8] palette, zoom-in ANI.
 *   8  FD2.SAV read: decrypt + checksum; if ok menu_options=2, and if
 *      current_chapter_id (save[0x30C5]) != 0xFF then menu_options=3.
 *   9  Menu loop: INT 16h key read; up/down move cursor (wrap), Enter/Space/
 *      0xE0/0x52 commit.
 *  10  Commit highlight blink (4x).
 *  11  Cleanup; tail-jumps the shared epilogue → returns active_idx_var.
 *
 * Returns: final menu selection (0..menu_options-1).
 *
 * NOTE: the decompiler tracks the [0x53A65] global (data_fd2_vga_palette_data_ptr)
 * as a stale local ("frame_buf"); every such site is the palette-data global,
 * which is both passed as the load buffer and assigned the load result.
 * ---------------------------------------------------------------- */
int fd2_play_ending_and_record_clear(void)
{
    uint8 *sfx_bank;
    uint8 *scroll_segment_buf;   /* title sprite reused as 5-segment loader buf */
    uint8 *panel_buf;            /* 234KB assembled vertical-scroll image       */
    uint8 *ptr_00;               /* clear-status panel sprite sheet             */
    uint8 *save_buf;
    int music_trigger_frames[15];
    int menu_options;
    int exit_flag;
    int active_idx_var;
    int next_idx;
    int i;
    int iVar4;
    int uVar5;
    uint32 row_offset;
    uint32 checksum;
    uint32 anim_idx;
    uint32 ms_per_frame;
    uint32 palette_idx_arg;
    uint32 img_a;
    uint32 img_b;
    uint8 music_step;
    uint8 music_idx;
    int k;

    menu_options = 1;
    exit_flag = 0;
    music_step = 0xC;
    music_idx = 0;
    active_idx_var = 0;

    for (k = 0; k < 15; k++) {
        music_trigger_frames[k] = data_fd2_chapter_ending_music_trigger_frames[k];
    }

    /* Phase 2 — title splash */
    sfx_bank = (uint8 *)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x4D);
    memset((void *)0xA0000, 0, 64000);
    data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat,
        data_fd2_vga_palette_data_ptr, 0x4C);
    fd2_set_vga_palette_range(0, 0xFF, 0x40);
    scroll_segment_buf = (uint8 *)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x4A);
    fd2_rle_blit_sprite((uint32)scroll_segment_buf, 0, 0, 0xA0000, 0x140, 0xFFFFFFFF);
    fd2_play_palette_fade_in();
    fd2_wait_n_bios_ticks(1);
    fd2_wait_n_bios_ticks(0x1E);
    fd2_play_palette_fade_to_black();

    /* Phase 3 — mid ANI */
    data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat,
        data_fd2_vga_palette_data_ptr, 0x63);
    memset((void *)0xA0000, 0, 64000);
    fd2_set_vga_palette_range(0, 0xFF, 0);
    fd2_play_ani_file_animation_sequence(3, 0x5A, 1);
    fd2_play_palette_fade_to_black();

    /* Phase 4 — build scrollable panel */
    memset((void *)0xA0000, 0, 64000);
    data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat,
        data_fd2_vga_palette_data_ptr, 0x65);
    fd2_set_vga_palette_range(0, 0xFF, 0x40);
    panel_buf = (uint8 *)malloc(0x396C0);
    memset(panel_buf, 0, 0x396C0);
    for (iVar4 = 0; iVar4 < 5; iVar4++) {
        scroll_segment_buf = (uint8 *)fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            (uint32)scroll_segment_buf, iVar4 + 0x45);
        fd2_rle_blit_sprite((uint32)scroll_segment_buf, 0, iVar4 * 0x93,
                            (uint32)panel_buf, 0x140, 0xFFFFFFFF);
    }
    fd2_clear_keyboard_buffer();
    if (data_fd2_battle_runtime_char_array_ptr != (runtime_char *)0) {
        free(data_fd2_battle_runtime_char_array_ptr);
    }
    data_fd2_battle_runtime_char_array_ptr = (runtime_char *)malloc(0xA0);

    /* Phase 5 — scrolling-credit countdown */
    for (iVar4 = 0x217; iVar4 >= 0; iVar4--) {
        fd2_blit_rectangle(0xA0000, 0x140,
                           (uint32)panel_buf + (uint32)(iVar4 * 0x140),
                           0x140, 0x140, 0xC8);
        if (iVar4 == 0x217) {
            fd2_play_palette_fade_in();
        }
        row_offset = (uint32)(iVar4 * 0x140);
        if (iVar4 == 0x19) {
            palette_idx_arg = 0;
            ms_per_frame = 0xF;
            anim_idx = 0;
            fd2_load_and_fade_in_cinematic_image(anim_idx, ms_per_frame, palette_idx_arg);
            fd2_blit_rectangle(0xA0000, 0x140, (uint32)panel_buf + row_offset,
                               0x140, 0x140, 0xC8);
            data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                data_fd2_vga_palette_data_ptr, 0x65);
            fd2_play_palette_fade_in();
        } else {
            if (iVar4 == 0x14A) {
                fd2_play_palette_fade_to_black();
                fd2_load_and_fade_in_cinematic_image(4, 0x5A, 99);
                palette_idx_arg = 0;
                ms_per_frame = 0x32;
                anim_idx = 5;
                fd2_load_and_fade_in_cinematic_image(anim_idx, ms_per_frame, palette_idx_arg);
                fd2_blit_rectangle(0xA0000, 0x140, (uint32)panel_buf + row_offset,
                                   0x140, 0x140, 0xC8);
                data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
                    (uint32)data_fd2_string_resource_filename_fdother_dat,
                    data_fd2_vga_palette_data_ptr, 0x65);
                fd2_play_palette_fade_in();
            } else if (iVar4 == 0xD2) {
                fd2_play_palette_fade_to_black();
                fd2_load_and_fade_in_cinematic_image(6, 0x5A, 99);
                palette_idx_arg = 0;
                ms_per_frame = 0x32;
                anim_idx = 7;
                fd2_load_and_fade_in_cinematic_image(anim_idx, ms_per_frame, palette_idx_arg);
                fd2_blit_rectangle(0xA0000, 0x140, (uint32)panel_buf + row_offset,
                                   0x140, 0x140, 0xC8);
                data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
                    (uint32)data_fd2_string_resource_filename_fdother_dat,
                    data_fd2_vga_palette_data_ptr, 0x65);
                fd2_play_palette_fade_in();
            } else if (iVar4 == 0x6E) {
                fd2_play_palette_fade_to_black();
                palette_idx_arg = 99;
                ms_per_frame = 0x5A;
                anim_idx = 8;
                fd2_load_and_fade_in_cinematic_image(anim_idx, ms_per_frame, palette_idx_arg);
                fd2_blit_rectangle(0xA0000, 0x140, (uint32)panel_buf + row_offset,
                                   0x140, 0x140, 0xC8);
                data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
                    (uint32)data_fd2_string_resource_filename_fdother_dat,
                    data_fd2_vga_palette_data_ptr, 0x65);
                fd2_play_palette_fade_in();
            } else if (iVar4 == 0x1C2) {
                img_b = 99;
                img_a = 100;
                fd2_display_cinematic_image_with_fade(img_a, img_b,
                                                      (uint32)panel_buf, iVar4);
            } else if (iVar4 == 10) {
                img_b = 0x4C;
                img_a = 0x4B;
                fd2_display_cinematic_image_with_fade(img_a, img_b,
                                                      (uint32)panel_buf, iVar4);
            }
        }
        if (iVar4 == music_trigger_frames[music_idx]) {
            music_step = 0;
            fd2_play_sfx_with_handle((uint32)sfx_bank, 0, 1);
            data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                data_fd2_vga_palette_data_ptr, 0x66);
            fd2_set_vga_palette_range(0, 0xFF, 0);
            music_idx = music_idx + 1;
        }
        if (music_step == 0xB) {
            data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdother_dat,
                data_fd2_vga_palette_data_ptr, 0x65);
            fd2_set_vga_palette_range(0, 0xFF, 0);
        }
        music_step = music_step + 1;
        __delay_thunk_375b2(0x1E);
        if (iVar4 == 0) {
            __delay_thunk_375b2(1000);
        }
        if (fd2_check_keyboard_buffer_nonempty() != 0) {
            break;
        }
    }

    /* Phase 6 — red-tint fade to black */
    for (uVar5 = 0x28; uVar5 >= 0; uVar5--) {
        fd2_interpolate_palette_range_toward_color(0, 0xFF, (uint32)uVar5, 0x3F, 0, 0);
        __delay_thunk_375b2(8);
    }
    __delay_thunk_375b2(100);
    fd2_clear_keyboard_buffer();
    free(panel_buf);
    free(scroll_segment_buf);

    /* Phase 7 — clear-status panel reveal */
    ptr_00 = (uint8 *)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 7);
    data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat,
        data_fd2_vga_palette_data_ptr, 8);
    memset((void *)0xA0000, 0, 64000);
    fd2_set_vga_palette_range(0, 0xFF, 0);
    fd2_play_ani_file_animation_sequence(1, 0xF, 1);
    fd2_play_sfx_sample_from_bank((uint32)sfx_bank, 3, 1);
    fd2_set_vga_palette_range_with_add(0, 0xFF, 0x40);
    fd2_blit_indexed_sprite_at_xy(0xA0000, 0x140, (uint32)ptr_00, 0);
    for (uVar5 = 0; uVar5 < 0x29; uVar5++) {
        fd2_interpolate_palette_range_toward_color(0, 0xFF, (uint32)uVar5, 0x38, 0x3C, 0x3F);
        __delay_thunk_375b2(8);
    }
    fd2_clear_keyboard_buffer();

    /* Phase 8 — FD2.SAV read for completion check */
    save_buf = (uint8 *)fopen("FD2.SAV", "rb");
    if (save_buf != (uint8 *)0) {
        FILE *fp;
        fp = (FILE *)save_buf;
        save_buf = (uint8 *)malloc(0x59CB);
        fread(save_buf, 1, 0x59CB, fp);
        fclose(fp);
        fd2_save_crypt_buffer((uint32)save_buf, 0x59CB);
        checksum = fd2_save_compute_checksum((uint32)save_buf, 0x59CB);
        if (checksum == *(uint32 *)(save_buf + 0x59C7)) {
            menu_options = 2;
            if (save_buf[0x30C5] != 0xFF) {
                menu_options = 3;
            }
        }
        free(save_buf);
    }

    /* Phase 9 — 3/2-option menu loop.
       active_idx_var (the EBP state register) is mutated in place by each
       arrow branch; the commit branch sets exit_flag. The middle arg
       0xFFFFFFFF passed in Phase 10 is the "no highlight" sentinel. */
    fd2_render_chapter_status_panel_segments((uint32)ptr_00, 0, menu_options);
    while (exit_flag == 0) {
        fd2_render_chapter_status_panel_segments((uint32)ptr_00, active_idx_var, menu_options);
        data_fd2_input_key_input_mode = 0x10;
        int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                     (union REGS *)&data_fd2_input_last_key_pressed);
        next_idx = menu_options - 1;
        if (data_fd2_input_key_input_mode == 0x48) {
            fd2_play_sfx_with_handle((uint32)sfx_bank, 2, 1);
            if (active_idx_var != 0) {
                active_idx_var = active_idx_var - 1;
            } else {
                active_idx_var = next_idx;
            }
        } else if (data_fd2_input_key_input_mode == 0x50) {
            fd2_play_sfx_with_handle((uint32)sfx_bank, 2, 1);
            if (active_idx_var == next_idx) {
                active_idx_var = active_idx_var ^ next_idx;
            } else {
                active_idx_var = active_idx_var + 1;
            }
        } else if (data_fd2_input_last_key_pressed == '\r'
                   || data_fd2_input_last_key_pressed == ' '
                   || data_fd2_input_key_input_mode == 0xE0
                   || data_fd2_input_key_input_mode == 0x52) {
            fd2_play_sfx_with_handle((uint32)sfx_bank, 1, 1);
            exit_flag = 1;
        }
    }

    /* Phase 10 — commit highlight blink */
    for (i = 0; i < 4; i++) {
        fd2_render_chapter_status_panel_segments((uint32)ptr_00, 0xFFFFFFFF, menu_options);
        __delay_thunk_375b2(0x50);
        fd2_render_chapter_status_panel_segments((uint32)ptr_00, active_idx_var, menu_options);
        __delay_thunk_375b2(0x50);
    }

    /* Phase 11 — cleanup + tail */
    fd2_play_palette_fade_to_black();
    memset((void *)0xA0000, 0, 64000);
    free(ptr_00);
    fd2_play_sfx_with_handle((uint32)sfx_bank, 0xFFFFFFFF, 1);
    free(sfx_bank);
    return active_idx_var;
}

/* ----------------------------------------------------------------
 * fd2_play_chapter_clear_fanfare @ 0x22E5C  (1 caller)
 *
 * Sole caller: fd2_main @ 0x25BF4 (entered when game_event_flag == 1, i.e. a
 * chapter was just cleared). Plays a short 2-frame "chapter cleared" fanfare
 * sprite sequence, then returns; fd2_main clears the event flag afterward.
 *
 * Sequence:
 *   - stop BGM with fade
 *   - fade screen to black, load FDOTHER.DAT[0x4F] fanfare sprite sheet
 *   - clear framebuffer, blit frame 0, fade in, hold 9 ticks
 *   - blit frame 1, hold 36 ticks
 *   - free the sprite sheet
 *
 * The tail free() compiles (via the original) into a jump into the shared
 * free-wrapper epilogue; the plain call below is the functional equivalent.
 * ---------------------------------------------------------------- */
void fd2_play_chapter_clear_fanfare(void)
{
    uint32 fanfare_sprite;

    fd2_set_bgm_track_with_fade(0xFFFFFFFF, 1);
    fd2_wait_n_bios_ticks(1);
    fd2_play_palette_fade_to_black();
    fanfare_sprite = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x4F);
    memset((void *)0xA0000, 0, 64000);
    fd2_blit_indexed_sprite(fanfare_sprite, 0, 0xA0000, 0x140, -1);
    fd2_play_palette_fade_in();
    fd2_wait_n_bios_ticks(9);
    fd2_blit_indexed_sprite(fanfare_sprite, 1, 0xA0000, 0x140, -1);
    fd2_wait_n_bios_ticks(0x24);
    free((void *)fanfare_sprite);
}

/* ----------------------------------------------------------------
 * fd2_play_chapter_intro_sprite_slideshow @ 0x24336  (1 caller)
 *
 * Sole caller: fd2_chapter_21_end @ 0x240FA (call site 0x242C9), reached only
 * after the chapter-21 hidden-stage 6-item collection unlock. Plays a 101-frame
 * (0x65) sprite slideshow off FDOTHER.DAT[0x22], with a mid-show white flash.
 *
 * Sequence:
 *   - pan cursor/window to (0xE, 8)
 *   - malloc a 64000-byte workspace, snapshot the live VGA framebuffer into it
 *   - load FDOTHER.DAT[0x22] sprite sheet, blit base frame 0 into the workspace
 *   - Phase 1 (frames 1..0x44, with palette cycling each frame):
 *       copy workspace -> large_game_state_buffer, blit frame, copy to 0xA0000,
 *       advance palette cycle, hold 3 ticks, drain keyboard
 *   - play ANI sequence 0; flash white (palette +0x3F, hold 100), restore
 *     (palette +0, hold 500)
 *   - Phase 2 (frames 0x45..0x64, no palette cycling): same blit/copy, hold 3
 *     ticks, drain keyboard
 *   - free workspace + sprite sheet, then composite battle frame 0
 *
 * The large_game_state_buffer global holds a scratch address used as the blit
 * scratch (read/written as a raw far buffer, like 0xA0000). The tail call to
 * fd2_composite_battle_frame_zero compiles (in the original) into a JMP that
 * replaces the epilogue; the plain call below is the functional equivalent.
 * ---------------------------------------------------------------- */
void fd2_play_chapter_intro_sprite_slideshow(void)
{
    void *workspace;
    uint32 sheet;
    uint32 sprite_idx;

    fd2_pan_cursor_and_window(0xE, 8);
    workspace = malloc(64000);
    memmove(workspace, (void *)0xA0000, 64000);
    sheet = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x22);
    fd2_blit_indexed_sprite(sheet, 0, (int)workspace, 0x140, -1);

    /* Phase 1 — frames 1..0x44 with palette cycling */
    for (sprite_idx = 1; (int)sprite_idx < 0x45; sprite_idx++) {
        memmove((void *)data_fd2_large_game_state_buffer_ptr, workspace, 64000);
        fd2_blit_indexed_sprite(sheet, sprite_idx,
                                (int)data_fd2_large_game_state_buffer_ptr, 0x140, -1);
        memmove((void *)0xA0000, (void *)data_fd2_large_game_state_buffer_ptr, 64000);
        fd2_update_palette_cycle_anim();
        fd2_wait_n_bios_ticks(3);
        fd2_clear_keyboard_buffer();
    }

    fd2_play_ani_file_animation_sequence(0, 0xF, 0);
    fd2_set_vga_palette_range_with_add(0, 0xFF, 0x3F);
    __delay_thunk_375b2(100);
    fd2_set_vga_palette_range_with_add(0, 0xFF, 0);
    __delay_thunk_375b2(500);

    /* Phase 2 — frames 0x45..0x64 without palette cycling */
    for (; (int)sprite_idx < 0x65; sprite_idx++) {
        memmove((void *)data_fd2_large_game_state_buffer_ptr, workspace, 64000);
        fd2_blit_indexed_sprite(sheet, sprite_idx,
                                (int)data_fd2_large_game_state_buffer_ptr, 0x140, -1);
        memmove((void *)0xA0000, (void *)data_fd2_large_game_state_buffer_ptr, 64000);
        fd2_wait_n_bios_ticks(3);
        fd2_clear_keyboard_buffer();
    }

    free(workspace);
    free((void *)sheet);
    fd2_composite_battle_frame_zero();
}
