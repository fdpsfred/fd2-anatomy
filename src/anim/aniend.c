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
 * Title-screen attract cinematic + main menu. Despite the "ending" art it
 * draws from, this is the top-level menu screen, not a post-clear-only path.
 *
 * Sole caller: fd2_main_menu_continue_dispatcher @ 0x25EBB, called
 * UNCONDITIONALLY at the top of the dispatcher (call site 0x25EC8) on every
 * return to the top level. Its EAX return selects the dispatcher branch:
 *   0 -> NEW GAME ; 1 -> CONTINUE (save-slot loader) ; 2 -> continue an
 *   already-completed game (full engine reload). The 3rd option only appears
 *   when FD2.SAV holds a finished save (Phase 8).
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
 *  11  Cleanup; tail-jumps the shared epilogue -> returns active_idx_var.
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

    /* Phase 2 -- title splash */
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

    /* Phase 3 -- mid ANI */
    data_fd2_vga_palette_data_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat,
        data_fd2_vga_palette_data_ptr, 0x63);
    memset((void *)0xA0000, 0, 64000);
    fd2_set_vga_palette_range(0, 0xFF, 0);
    fd2_play_ani_file_animation_sequence(3, 0x5A, 1);
    fd2_play_palette_fade_to_black();

    /* Phase 4 -- build scrollable panel */
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

    /* Phase 5 -- scrolling-credit countdown */
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
        fd2_delay_ms(0x1E);
        if (iVar4 == 0) {
            fd2_delay_ms(1000);
        }
        if (fd2_check_keyboard_buffer_nonempty() != 0) {
            break;
        }
    }

    /* Phase 6 -- red-tint fade to black */
    for (uVar5 = 0x28; uVar5 >= 0; uVar5--) {
        fd2_interpolate_palette_range_toward_color(0, 0xFF, (uint32)uVar5, 0x3F, 0, 0);
        fd2_delay_ms(8);
    }
    fd2_delay_ms(100);
    fd2_clear_keyboard_buffer();
    free(panel_buf);
    free(scroll_segment_buf);

    /* Phase 7 -- clear-status panel reveal */
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
        fd2_delay_ms(8);
    }
    fd2_clear_keyboard_buffer();

    /* Phase 8 -- FD2.SAV read for completion check */
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

    /* Phase 9 -- 3/2-option menu loop.
       active_idx_var (the EBP state register) is mutated in place by each
       arrow branch; the commit branch sets exit_flag. The middle arg
       0xFFFFFFFF passed in Phase 10 is the "no highlight" sentinel. */
    fd2_render_chapter_status_panel_segments((uint32)ptr_00, 0, menu_options);
    while (exit_flag == 0) {
        fd2_render_chapter_status_panel_segments((uint32)ptr_00, active_idx_var, menu_options);
#ifdef FD2_REPLAY
        fd2_replay_pump();   /* non-polling read: ensure a scripted key is ready */
#endif
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

    /* Phase 10 -- commit highlight blink */
    for (i = 0; i < 4; i++) {
        fd2_render_chapter_status_panel_segments((uint32)ptr_00, 0xFFFFFFFF, menu_options);
        fd2_delay_ms(0x50);
        fd2_render_chapter_status_panel_segments((uint32)ptr_00, active_idx_var, menu_options);
        fd2_delay_ms(0x50);
    }

    /* Phase 11 -- cleanup + tail */
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
 * Sole caller: main @ 0x25BF4 (entered when game_event_flag == 1, i.e. a
 * chapter was just cleared). Plays a short 2-frame "chapter cleared" fanfare
 * sprite sequence, then returns; main clears the event flag afterward.
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
    fd2_delay_ms(100);
    fd2_set_vga_palette_range_with_add(0, 0xFF, 0);
    fd2_delay_ms(500);

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

/* ----------------------------------------------------------------
 * fd2_play_game_ending_cinematic @ 0x2BCE5  (2 callers)
 *
 * Callers: fd2_chapter_27_end @ 0x250CC (BAD path — no sky key, 悠妮 alone
 *          game over, short ending) and fd2_chapter_30_end @ 0x25757 (GOOD
 *          path — final boss killed, full ending + 20-char credit roll).
 *
 * The whole branch structure pivots on current_chapter_id == 0x1A: the BAD
 * path takes the == 0x1A side of every dialog dispatch; the GOOD path takes
 * the else side and reaches the post-fd2_play_final_chapter_30_ending credit
 * roll (the BAD path's chapter_27_end never sets up the conditions to reach
 * here with a non-0x1A id... but the code itself runs the same tail either
 * way once invoked — faithfully reproduced).
 *
 * Phases (see plate @ 0x2BCE5 for the full per-address breakdown):
 *   - copy the three 20-byte per-duel credit-roll tables to the stack
 *   - malloc a 320KB scroll workspace + a 64000-byte VGA backup
 *   - title frame (FDOTHER.DAT[0x36] frame 0), fade in, mid ANI seq 2
 *   - highlight frame 9 + fade-down, dialog dispatch 1
 *   - 3x full palette fade, sprite cycling 0x0C..0x6C, dialog dispatch 2
 *   - 0x28-iteration horizontal-scroll duel intro (var_14/var_18 offsets)
 *   - dialog dispatch 3
 *   - 200-iteration scroll with a trailing palette fade-out (rows >0x87)
 *   - fd2_play_final_chapter_30_ending()
 *   - BGM transition image (FDOTHER.DAT[0x3C]), credits BGM
 *   - 20-char credit roll: per duel, derive runtime_char[0]/[1] team+portrait
 *     from the top/bottom tables (team = 2 if id < 0x4C else 0) and the
 *     scripted-cinematic mode from the scripted table, run a scripted duel,
 *     then show the credit sprite frame and fade
 *   - final ending image (FDOTHER.DAT[0x3B])
 *
 * Resource handles in the credit roll, exactly as the machine code uses them
 * (note these run opposite to the obvious name->index reading): res_3a =
 * FDOTHER.DAT[0x3A] is the sprite atlas passed to fd2_blit_indexed_sprite,
 * while res_39 = FDOTHER.DAT[0x39] is assigned to the VGA palette-data global
 * (and is the one freed after the loop); res_3a is intentionally leaked, as
 * in the original. The EBP-saved data_fd2_vga_palette_data_ptr is restored at
 * the top of every credit-roll iteration (and once after the loop).
 *
 * data_fd2_vga_palette_data_ptr is the 0x53A65 global; the decompiler tracks
 * it as a stale local in places, but every such site is this palette-data
 * global.
 * ---------------------------------------------------------------- */
void fd2_play_game_ending_cinematic(void)
{
    uint8 *workspace;          /* 320KB scroll/compose workspace               */
    void  *vga_backup;         /* 64000-byte snapshot of the title frame       */
    uint8 *sheet;              /* FDOTHER.DAT[0x36] character-endings atlas     */
    uint32 res_3a;             /* FDOTHER.DAT[0x3A] — credit-roll blit atlas    */
    uint32 res_39;             /* FDOTHER.DAT[0x39] — assigned to palette ptr   */
    uint32 res_3b;             /* FDOTHER.DAT[0x3B] — final ending image        */
    uint32 res_3c;             /* FDOTHER.DAT[0x3C] — BGM-transition image      */
    uint32 saved_palette_ptr;  /* EBP — saved data_fd2_vga_palette_data_ptr     */
    uint8  top_tbl[20];        /* top-half portrait ids per duel                */
    uint8  bottom_tbl[20];     /* bottom-half portrait ids per duel             */
    uint8  scripted_tbl[20];   /* scripted_cinematic mode per duel              */
    uint32 v;                  /* palette brightness sweep                      */
    uint32 brightness_sub;     /* 200-loop fade-out accumulator                 */
    uint32 sprite_idx;
    uint32 dialog_id;
    uint32 portrait_id;
    int32  off_down;           /* var_14 — descending sprite x offset           */
    int32  off_up;             /* var_18 — ascending sprite x offset            */
    int    i;
    int    k;

    /* copy the three 20-byte per-duel tables to the stack */
    for (k = 0; k < 20; k++) {
        top_tbl[k] = data_fd2_chapter_ending_credit_roll_top_portrait_id_table[k];
    }
    for (k = 0; k < 20; k++) {
        bottom_tbl[k] = data_fd2_chapter_ending_credit_roll_bottom_portrait_id_table[k];
    }
    for (k = 0; k < 20; k++) {
        scripted_tbl[k] = data_fd2_chapter_ending_credit_roll_scripted_outcome_table[k];
    }

    off_down = 0x122;
    off_up   = 0x50;

    workspace  = (uint8 *)malloc(0x1F400);
    vga_backup = malloc(64000);
    memset(vga_backup, 0, 64000);

    /* title frame, fade out/in, mid ANI */
    sheet = (uint8 *)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x36);
    fd2_blit_indexed_sprite((uint32)sheet, 0, (int)vga_backup, 0x140, -1);
    fd2_play_palette_fade_to_black();
    memmove((void *)0xA0000, vga_backup, 64000);
    fd2_play_palette_fade_in();
    fd2_delay_ms(1000);
    fd2_play_ani_file_animation_sequence(2, 100, 0);

    /* highlight frame 9 + fade-down */
    fd2_set_vga_palette_range_with_add(0, 0xFF, 0x3F);
    memmove((void *)0xA0000, vga_backup, 64000);
    fd2_blit_indexed_sprite((uint32)sheet, 9, 0xA0000, 0x140, -1);
    for (v = 0x3F; (int)v >= 0; v--) {
        fd2_set_vga_palette_range_with_add(0, 0xFF, v);
        fd2_delay_ms(4);
    }
    fd2_delay_ms(2000);

    /* dialog dispatch 1 */
    if (data_fd2_chapter_current_chapter_id == 0x1A) {
        portrait_id = 0x11;
        dialog_id   = 4;
    } else {
        fd2_show_portrait_dialog_with_input(0x25, 2);
        fd2_show_portrait_dialog_with_input(0x15, 3);
        fd2_show_portrait_dialog_with_input(0x1A, 4);
        fd2_show_portrait_dialog_with_input(0x69, 5);
        portrait_id = 6;
        dialog_id   = 0x20;
    }
    fd2_show_portrait_dialog_with_input(dialog_id, portrait_id);
    fd2_delay_ms(500);

    /* 3x full palette fade + 200ms hold */
    for (i = 0; i < 3; i++) {
        for (v = 0x3F; (int)v >= 0; v--) {
            fd2_set_vga_palette_range_with_add(0, 0xFF, v);
            fd2_delay_ms(4);
        }
        fd2_delay_ms(200);
    }

    /* sprite cycling 0x0C..0x6C */
    for (sprite_idx = 0xC; (int)sprite_idx < 0x6D; sprite_idx++) {
        fd2_blit_indexed_sprite((uint32)sheet, sprite_idx, 0xA0000, 0x140, -1);
        fd2_delay_ms(0x14);
    }
    memmove((void *)0xA0000, vga_backup, 64000);

    /* dialog dispatch 2 */
    if (data_fd2_chapter_current_chapter_id == 0x1A) {
        fd2_show_portrait_dialog_with_input(0x15, 0x12);
        fd2_show_portrait_dialog_with_input(0x18, 0x13);
        portrait_id = 0x14;
        dialog_id   = 0x1A;
    } else {
        portrait_id = 7;
        dialog_id   = 0x2D;
    }
    fd2_show_portrait_dialog_with_input(dialog_id, portrait_id);
    fd2_delay_ms(2000);

    /* 0x28-iteration horizontal-scroll duel intro */
    for (i = 0; i < 0x28; i++) {
        fd2_blit_rectangle((uint32)workspace + 0xA0, 0x280, (uint32)vga_backup,
                           0x140, 0x140, 0xC8);
        fd2_blit_indexed_sprite((uint32)sheet, i % 4 + 1,
                                off_down + (int)workspace, 0x280, -1);
        fd2_blit_indexed_sprite((uint32)sheet, i % 4 + 5,
                                off_up + (int)workspace, 0x280, -1);
        off_up = off_up + 2;
        if (i < 0x19) {
            off_down = off_down - 4;
        } else {
            off_down = off_down - 2;
        }
        fd2_delay_ms(0x14);
        fd2_blit_rectangle(0xA0000, 0x140, (uint32)workspace + 0xA0,
                           0x280, 0x140, 0xC8);
    }

    memmove(workspace, vga_backup, 64000);
    fd2_blit_indexed_sprite((uint32)sheet, 1, (int)workspace, 0x140, -1);
    fd2_blit_indexed_sprite((uint32)sheet, 5, (int)workspace, 0x140, -1);
    fd2_blit_rectangle(0xA0000, 0x140, (uint32)workspace, 0x140, 0x140, 0xC8);

    /* dialog dispatch 3 */
    if (data_fd2_chapter_current_chapter_id == 0x1A) {
        fd2_show_portrait_dialog_with_input(0x20, 0x15);
        fd2_show_portrait_dialog_with_input(0x24, 0x16);
        portrait_id = 0x17;
        dialog_id   = 0x20;
    } else {
        fd2_show_portrait_dialog_with_input(0x20, 8);
        portrait_id = 9;
        dialog_id   = 0x24;
    }
    fd2_show_portrait_dialog_with_input(dialog_id, portrait_id);

    /* 200-iteration scroll with trailing palette fade-out (rows > 0x87) */
    brightness_sub = 0;
    for (i = 0; i < 200; i++) {
        memmove(workspace, vga_backup, 64000);
        fd2_blit_indexed_sprite((uint32)sheet, i % 4 + 1, (int)workspace, 0x140, -1);
        fd2_blit_indexed_sprite((uint32)sheet, i % 4 + 5, (int)workspace, 0x140, -1);
        fd2_delay_ms(0x14);
        fd2_blit_rectangle(0xA0000, 0x140, (uint32)workspace, 0x140, 0x140, 0xC8);
        if (i > 0x87) {
            brightness_sub = brightness_sub + 1;
        }
        fd2_set_vga_palette_range(0, 0xFF, brightness_sub);
    }

    free(workspace);
    memset((void *)0xA0000, 0, 64000);

    fd2_play_final_chapter_30_ending();

    memset((void *)0xA0000, 0, 64000);
    fd2_set_bgm_track_with_fade(0xFFFFFFFF, 1);
    fd2_wait_n_bios_ticks(0x32);
    res_3c = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x3C);
    fd2_rle_blit_sprite(res_3c, 0, 0, 0xA0000, 0x140, 0xFFFFFFFF);
    fd2_play_palette_fade_in();
    fd2_set_bgm_track_with_fade(0x12, 0);
    fd2_wait_n_bios_ticks(0x50);
    fd2_play_palette_fade_to_black();
    memset((void *)0xA0000, 0, 64000);

    res_3a = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, res_3c, 0x3A);
    /* the 0x39 load's buf arg is the still-zero res_39 slot, not res_3a
       (matches the machine code's [ESP+0x40]==0 at this point) */
    res_39 = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x39);

    /* 20-char credit roll (good ending only reaches the post-final tail) */
    saved_palette_ptr = data_fd2_vga_palette_data_ptr;
    for (v = 0; (int)v < 0x14; v++) {
        data_fd2_vga_palette_data_ptr = saved_palette_ptr;

        if (top_tbl[v] < 0x4C) {
            data_fd2_battle_runtime_char_array_ptr->team = 2;
        } else {
            data_fd2_battle_runtime_char_array_ptr->team = 0;
        }
        data_fd2_battle_runtime_char_array_ptr->portrait_id = top_tbl[v];

        if (bottom_tbl[v] < 0x4C) {
            data_fd2_battle_runtime_char_array_ptr[1].team = 2;
        } else {
            data_fd2_battle_runtime_char_array_ptr[1].team = 0;
        }
        data_fd2_battle_runtime_char_array_ptr[1].portrait_id = bottom_tbl[v];

        data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = scripted_tbl[v];
        fd2_play_full_combat_cinematic(0, 1);

        data_fd2_vga_palette_data_ptr = res_39;
        fd2_set_vga_palette_range(0, 0xFF, 0);
        fd2_wait_n_bios_ticks(0x14);
        fd2_blit_indexed_sprite(res_3a, v, 0xA0000, 0x140, -1);
        fd2_wait_n_bios_ticks(0x4E);
        fd2_play_palette_fade_to_black();
        memset((void *)0xA0000, 0, 64000);
    }
    data_fd2_vga_palette_data_ptr = saved_palette_ptr;

    free((void *)res_39);
    fd2_wait_n_bios_ticks(0x32);
    res_3b = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, res_3a, 0x3B);
    fd2_rle_blit_sprite(res_3b, 0, 0, 0xA0000, 0x140, 0xFFFFFFFF);
    fd2_play_palette_fade_in();
    free((void *)res_3b);
}

/* ----------------------------------------------------------------
 * fd2_play_final_chapter_30_ending @ 0x2C405  (1 caller)
 *
 * Sole caller: fd2_play_game_ending_cinematic @ 0x2BCE5 (call site 0x2C18F,
 * invoked just before the 20-char credit roll).
 *
 * Chapter-30 final per-character portrait + epilogue monologue sequence:
 *   - load chapter-30 battle data (idx 0x1E); set the dialog portrait blit
 *     offset to 0xC88
 *   - 0x36B00-byte workspace: render the chapter-30 opening text (page 0x2C),
 *     then run a 500-iteration palette "breathing" sweep (brightness 0x28
 *     fades down to 0 over the first 200 frames, fades back up after frame 300)
 *   - reload a 0x1F400 workspace + two 64000-byte buffers; load the TAI.DAT[3]
 *     backdrop sprite + FDOTHER.DAT[0x38] RLE base image into bg_buf; start
 *     ending BGM track 4
 *   - reverse-iterate the menu party (most-recently-recruited first), with a
 *     0/1 display-slot swap so the two lead characters trade order:
 *       * load FIGANI.DAT pose data (figani_idx+1) and sprite sheet (figani_idx)
 *         where figani_idx = portrait_id * 3
 *       * play the char intro zoom, then a 20-tick figani pose hold, then walk
 *         the pose-step table (count = pose_data[2]; per-step delay =
 *         pose_data[pose_data[s*4+8] + 6])
 *       * snapshot bg_buf -> scratch, load the DATO.DAT portrait, frame the
 *         dialog panel; the last character (char_idx 0) gets a 0x1B8-tick
 *         monologue, all others 0xDC
 *       * per tick: render frame + RNG-driven mouth jitter + 5 dialog fragments
 *         (header / char name / separator / job name / epilogue passage); a key
 *         press skips to the next character; fade to black between characters
 *
 * EAX-tracking-bug correction: the decompiler renders the mouth-jitter reload
 * as `(byte)DATO_load_result & 0x1F`, but the machine code reloads it from the
 * RNG: `CALL fd2_advance_rng_state; AND AL,0x1F; ADD AL,0x28`. Reproduced as
 * `(fd2_advance_rng_state() & 0x1F) + 0x28` — the advance returns the new seed
 * in AX, which the compiler reuses in AL.
 *
 * Resources:
 *   chapter-30 battle data row (idx 0x1E)
 *   TAI.DAT[3]            — chapter-30 ending backdrop sprite
 *   FDOTHER.DAT[0x38]     — RLE base image
 *   FIGANI.DAT[portrait*3 (+1)] — per-char sprite sheet + pose data
 *   DATO.DAT[portrait_id] — large portrait sprite
 *   BGM track 4          — ending BGM
 *
 * The tail free(workspace) compiles (in the original) into a jump into the
 * shared free-wrapper epilogue; the plain call below is the equivalent.
 * ---------------------------------------------------------------- */
void fd2_play_final_chapter_30_ending(void)
{
    void   *workspace;       /* 0x36B00 then 0x1F400 compose/scroll workspace  */
    void   *bg_buf;          /* 64000-byte backdrop (RLE base image)           */
    void   *scratch;         /* 64000-byte per-character dialog backdrop copy  */
    uint32  bg_sprite;       /* TAI.DAT[3] backdrop sprite sheet               */
    uint8  *pose_data;       /* FIGANI.DAT[figani_idx+1] pose-step table        */
    uint32  sprite_sheet;    /* FIGANI.DAT[figani_idx] sprite sheet            */
    uint32  rle_stream;      /* FDOTHER.DAT[0x38] RLE base image (freed after)  */
    runtime_char *party;     /* cached runtime_char array base in the loop      */
    uint32  brightness;
    uint32  i;
    int     char_idx;
    uint8   swap_idx;
    uint8   portrait_id;
    uint32  figani_idx;
    int     t;
    int     s;
    int     step_offset;
    int     tick_count;
    int     frame_offset;
    uint32  rng_val;
    uint8   jitter_counter;

    workspace = (void *)0;   /* pose_data / sprite_sheet self-ref buf seeds     */
    pose_data = (uint8 *)0;
    sprite_sheet = 0;
    jitter_counter = 0;

    fd2_load_chapter_battle_data(0x1E);
    data_fd2_dialog_active_portrait_blit_offset = 0xC88;

    workspace = malloc(0x36B00);
    memset(workspace, 0, 0x36B00);
    fd2_display_dialog_scene(data_fd2_current_chapter_text, 0x2C,
                             (uint32)workspace + 0x12C30, 0x140, 0xCD, 0x4C,
                             0, 0x19, 0);

    /* 500-iter palette breathing: fade-down to 0 over frames 0..199,
       fade-up after frame 300 */
    brightness = 0x28;
    for (i = 0; (int)i < 500; i++) {
        fd2_set_vga_palette_range(0, 0xFF, brightness);
        fd2_blit_rectangle(0xA0000, 0x140,
                           (uint32)workspace + i * 0x140, 0x140, 0x140, 0xC8);
        if ((int)i < 200 && (uint8)brightness != 0 && (int)i % 5 == 0) {
            brightness = (uint8)(brightness - 1);
        }
        if ((int)i > 300 && (int)i % 5 == 0) {
            brightness = (uint8)(brightness + 1);
        }
        fd2_wait_n_bios_ticks(1);
    }
    free(workspace);

    workspace = malloc(0x1F400);
    bg_buf  = malloc(64000);
    scratch = malloc(64000);

    bg_sprite = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_tai_dat, 0, 3);
    rle_stream = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x38);
    fd2_rle_blit_sprite(rle_stream, 0, 0, (uint32)bg_buf, 0x140, 0xFFFFFFFF);
    free((void *)rle_stream);
    fd2_set_bgm_track_with_fade(4, 0);

    /* reverse-iterate menu party (most-recently-recruited first) */
    for (char_idx = (int)data_fd2_shared_menu_party_member_count - 1;
         char_idx >= 0; char_idx--) {
        party = data_fd2_battle_runtime_char_array_ptr;

        /* 0/1 swap so the two lead slots trade display order */
        if (char_idx == 0) {
            swap_idx = 1;
        } else if (char_idx == 1) {
            swap_idx = 0;
        } else {
            swap_idx = (uint8)char_idx;
        }

        portrait_id = data_fd2_battle_runtime_char_array_ptr[swap_idx].portrait_id;
        figani_idx = (uint32)portrait_id * 3;

        pose_data = (uint8 *)fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_figani_dat_52388,
            (uint32)pose_data, figani_idx + 1);
        sprite_sheet = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_figani_dat_52388,
            sprite_sheet, figani_idx);

        fd2_play_char_intro_zoom_anim((uint32)char_idx, 1, sprite_sheet, 0,
                                      (uint32)workspace, (uint32)bg_buf,
                                      bg_sprite);
        fd2_step_figani_pose_animation(sprite_sheet, 0, (uint32)workspace, 0x140);

        /* 20-tick figani pose hold */
        for (t = 0; t < 0x14; t++) {
            fd2_blit_rectangle((uint32)workspace, 0x140, (uint32)bg_buf,
                               0x140, 0x140, 0xC8);
            fd2_step_figani_pose_animation(sprite_sheet, 0xFFFFFFFF,
                                           (uint32)workspace, 0x140);
            fd2_blit_rectangle(0xA0000, 0x140, (uint32)workspace,
                               0x140, 0x140, 0xC8);
            fd2_wait_n_bios_ticks(1);
        }

        /* pose-step loop driven by the pose_data header */
        for (s = 0; s < (int)pose_data[2]; s++) {
            step_offset = *(int32 *)(pose_data + s * 4 + 8);
            fd2_blit_rectangle((uint32)workspace, 0x140, (uint32)bg_buf,
                               0x140, 0x140, 0xC8);
            fd2_blit_indexed_sprite((uint32)pose_data, (uint32)s,
                                    (uint32)workspace, 0x140, -1);
            fd2_blit_rectangle(0xA0000, 0x140, (uint32)workspace,
                               0x140, 0x140, 0xC8);
            fd2_wait_n_bios_ticks((uint32)pose_data[step_offset + 6]);
        }

        memmove(scratch, bg_buf, 64000);
        data_fd2_portrait_sprite_buffer = (uint8 *)fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_dato_dat_51a70,
            (uint32)data_fd2_portrait_sprite_buffer, (uint32)portrait_id);
        fd2_assemble_dialog_frame_layered((uint32)scratch, 0x140, 5, 7, 5, 5);

        /* last character (char_idx 0) gets the long monologue */
        if (char_idx == 0) {
            tick_count = 0x1B8;
        } else {
            tick_count = 0xDC;
        }

        for (t = 0; t < tick_count; t++) {
            fd2_blit_rectangle((uint32)workspace, 0x140, (uint32)scratch,
                               0x140, 0x140, 0xC8);

            /* RNG-driven mouth jitter (EAX-bug corrected: jitter from RNG) */
            if (jitter_counter == 0) {
                rng_val = fd2_advance_rng_state();
                jitter_counter = (uint8)((rng_val & 0x1F) + 0x28);
            } else {
                jitter_counter = jitter_counter - 1;
            }
            if (jitter_counter < 2) {
                frame_offset = 0xC;
            } else {
                frame_offset = 0;
            }
            fd2_dialog_sprite_blit_normal(
                data_fd2_dialog_active_portrait_blit_offset + (uint32)workspace,
                (uint32)data_fd2_portrait_sprite_buffer
                    + *(int32 *)(data_fd2_portrait_sprite_buffer + frame_offset),
                0x140);
            fd2_step_figani_pose_animation(sprite_sheet, 0xFFFFFFFF,
                                           (uint32)workspace, 0x140);

            /* 5-fragment dialog assembly */
            fd2_display_dialog_scene(data_fd2_current_chapter_text, 10,
                                     (uint32)workspace + 0x16E9, 0x140, 0xCD,
                                     0x4C, 0, 0, 0);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                                     (uint32)(party[swap_idx].char_id + 1),
                                     (uint32)workspace + 0x171B, 0x140, 0xCD,
                                     0x4C, 0, 0, 0);
            fd2_display_dialog_scene(data_fd2_current_chapter_text, 0xB,
                                     (uint32)workspace + 0x2FE9, 0x140, 0xCD,
                                     0x4C, 0, 0, 0);
            fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                                     (uint32)(party[swap_idx].job_id + 0x96),
                                     (uint32)workspace + 0x301B, 0x140, 0xCD,
                                     0x4C, 0, 0, 0);
            if (t < 0xDC) {
                i = (uint8)(party[swap_idx].char_id + 0xC);
            } else {
                i = 0x2D;
            }
            fd2_display_dialog_scene(data_fd2_current_chapter_text, i,
                                     (uint32)workspace + 0x7D08, 0x140, 0xCD,
                                     0x4C, 0, 0x14, 0);

            fd2_blit_rectangle(0xA0000, 0x140, (uint32)workspace,
                               0x140, 0x140, 0xC8);
            fd2_wait_n_bios_ticks(1);

            /* key press -> skip to next character */
            if (fd2_check_keyboard_buffer_nonempty() != 0) {
                char_idx = 1;
                fd2_clear_keyboard_buffer();
            }
        }
        fd2_play_palette_fade_to_black();
    }

    free(bg_buf);
    free(scratch);
    free((void *)pose_data);
    free((void *)bg_sprite);
    free(workspace);
}
