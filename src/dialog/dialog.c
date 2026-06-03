/*
 * dialog.c — FD2 dialog-box sprite/overlay helpers.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_cleanup_dialog_sprite_buffer @ 0x15E71 (7 callers)
 *
 * Restore the screen block saved inside saved_block back to dst at
 * the given stride, then free saved_block. Undoes a transient sprite
 * blit (pairs with fd2_blit_indexed_sprite_with_alloc /
 * fd2_alloc_and_blit_indexed_sprite_chunk): the pre-blit screen
 * content was snapshotted into saved_block's saved-pixels area, and
 * this restores it before releasing the temp buffer.
 * ---------------------------------------------------------------- */
void fd2_cleanup_dialog_sprite_buffer(uint32 saved_block, uint32 dst, uint32 stride)
{
    fd2_restore_screen_block_from_buffer(saved_block, dst, stride);
    free((void *)saved_block);
}

/* ----------------------------------------------------------------
 * fd2_display_dialog_scene @ 0x15F84 (40+ callers)
 *
 * FD2 dialog VM: interprets a compiled dialog bytecode stream
 * (int16 opcodes) starting at text_base[page_idx].  Negative values
 * are control codes; non-negative values are glyph indices rendered
 * with fd2_blit_glyph_2bpp_with_outline (advancing 0x10 px per glyph).
 *
 *   -1  (0xFFFF) END          flush open portrait, return render_pos
 *   -2  (0xFFFE) LINE ADVANCE  bump line_count + reflow, NO input wait
 *   -3  (0xFFFD) PAGE BREAK    bump line, paint portrait, wait for input
 *   -4           sub-dialog at last_action_sprite_id (recursive)
 *   -5           sub-dialog at drop_swap_text_id      (recursive)
 *   -6           literal number (sprintf %d then blit digits)
 *   -0x11/-0x12  load ENEMY sprite into portrait slot1/slot2
 *   -0x13/-0x14  load ALLY  sprite into portrait slot1/slot2
 *   else         TEXT glyph
 *
 * Args (9, __cdecl): text_base, page_idx, render_pos, render_pitch,
 *   glyph_p5, glyph_p6, glyph_p7, glyph_height, blink_flag.
 * Returns final render_pos (consumed by the -4/-5 recursive callers).
 *
 * Mirrors the vendor's register-liveness behaviour: pSpeaker (slot1)
 * is only reassigned on the enemy!=0x27 branch / ally(-0x13) path, so
 * the 0x27 case reuses its previous value, exactly as the binary does.
 * ---------------------------------------------------------------- */
uint32 fd2_display_dialog_scene(uint32 text_base, uint32 page_idx,
                                uint32 render_pos, uint32 render_pitch,
                                uint32 glyph_p5, uint32 glyph_p6,
                                uint32 glyph_p7, uint32 glyph_height,
                                uint32 blink_flag)
{
    uint32        render_base;
    uint32        cur_op;
    uint32        next_op;
    int           opcode;
    uint32        line_count;
    uint32        portrait_anim;
    uint32        portrait_flip;
    uint32        sub_idx;
    uint32        portrait_target;
    int           found;
    runtime_char *pSpeaker;
    runtime_char *pAlly;
    runtime_char *speaker_global;
    uint8        *sprite;
    char          digit_buf[12];
    int           digit_len;
    int           i;

    line_count    = 0;
    portrait_anim = 0;
    portrait_flip = 0;
    render_base   = render_pos;
    pSpeaker      = (runtime_char *)0;
    cur_op = text_base + (uint32)*(int16 *)(text_base + page_idx * 2);

    for (;;) {
        opcode = (int)*(int16 *)cur_op;

        if (opcode == -1) {                         /* END */
            if (portrait_anim != 0) {
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
                fd2_close_dialog_panels_then_slide_in_at(portrait_anim,
                                                         portrait_flip);
                data_fd2_dialog_active_portrait_blit_offset = 0;
            }
            return render_pos;
        }

        if (opcode == -2) {                         /* LINE ADVANCE (silent) */
            if (((data_fd2_dialog_active_portrait_blit_offset == 0x728) ||
                 (data_fd2_dialog_active_portrait_blit_offset == 0x9017)) &&
                (line_count == 3)) {
                fd2_cinematic_scroll_text_up_for_special_scenes();
                line_count = 2;
            }
            line_count += 1;
            render_pos = render_base + render_pitch * glyph_height * line_count;
            cur_op += 2;
            continue;
        }

        if (opcode == -3) {                         /* PAGE BREAK (wait + clear) */
            if (((data_fd2_dialog_active_portrait_blit_offset == 0x728) ||
                 (data_fd2_dialog_active_portrait_blit_offset == 0x9017)) &&
                (line_count == 3)) {
                fd2_cinematic_scroll_text_up_for_special_scenes();
                line_count = 2;
            }
            line_count += 1;
            render_pos = render_base + render_pitch * glyph_height * line_count;
            cur_op += 2;
            if ((data_fd2_dialog_active_portrait_blit_offset == 0x728) ||
                (data_fd2_dialog_active_portrait_blit_offset == 0x9017)) {
                fd2_paint_portrait_to_dialog_area(0);
            }
            fd2_wait_for_input_dialog_with_blink(1);
            blink_flag = 1;
            continue;
        }

        next_op = cur_op + 2;

        if ((opcode == -4) || (opcode == -5)) {     /* recursive sub-dialog */
            sub_idx = (opcode == -4)
                          ? data_fd2_dialog_last_action_sprite_id_param
                          : data_fd2_dialog_drop_swap_text_id_param;
            render_pos = fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                                                  sub_idx, render_pos,
                                                  render_pitch, 0xcd, 0x4c,
                                                  0x4a, 0x13, 1);
            cur_op = next_op;
            continue;
        }

        if (opcode == -6) {                         /* literal number */
            sprintf(digit_buf, "%d", data_fd2_dialog_last_action_value_param);
            digit_len = (int)(strlen(digit_buf) & 0xff);
            for (i = 0; i < digit_len; i++) {
                fd2_blit_glyph_2bpp_with_outline(data_fd2_chinese_font_sheet,
                                                 (uint32)((uint8)digit_buf[i] - 0x30),
                                                 render_pos, render_pitch,
                                                 glyph_p5, glyph_p6,
                                                 (uint16)glyph_p7);
                if (fd2_check_keyboard_buffer_nonempty() != 0) {
                    blink_flag = 0;
                }
                if (blink_flag != 0) {
                    fd2_portrait_blink_animation_step();
                }
                render_pos += 0x10;
            }
            cur_op += 2;
            continue;
        }

        if (opcode == -0x11) {                       /* ENEMY sprite slot1 */
            if (portrait_anim != 0) {
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
                fd2_close_dialog_panels_then_slide_in_at(portrait_anim,
                                                         portrait_flip);
            }
            data_fd2_dialog_active_portrait_blit_offset = 0x728;
            portrait_target = (uint32)*(uint16 *)(cur_op + 2);
            found = fd2_find_char_by_id_or_template(portrait_target);
            portrait_flip = (found == -1) ? 0 : 2;
            if (portrait_target != 0x27) {
                speaker_global = (runtime_char *)data_fd2_dialog_current_speaker_char_ptr;
                portrait_target = (uint32)speaker_global->portrait_id;
                pSpeaker = speaker_global;
            }
            data_fd2_portrait_sprite_buffer =
                (uint8 *)fd2_load_dat_resource(0x51a70,
                                      (uint32)data_fd2_portrait_sprite_buffer,
                                      portrait_target);
            portrait_anim = fd2_play_dialog_open_animation(
                                (uint32)pSpeaker->pos_x,
                                (uint32)pSpeaker->pos_y, portrait_flip);
            sprite = (uint8 *)data_fd2_portrait_sprite_buffer;
            sprite = sprite + *sprite;
            fd2_dialog_sprite_blit_normal(
                data_fd2_dialog_active_portrait_blit_offset + 0xa0000,
                (uint32)sprite, 0x140);
            render_base = 0xa0b4f;
            render_pos  = 0xa0b4f;
            line_count  = 0;
            blink_flag  = 1;
            cur_op += 4;
            continue;
        }

        if (opcode == -0x12) {                       /* ENEMY sprite slot2 */
            if (portrait_anim != 0) {
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
                fd2_close_dialog_panels_then_slide_in_at(portrait_anim,
                                                         portrait_flip);
            }
            data_fd2_dialog_active_portrait_blit_offset = 0x9017;
            portrait_target = (uint32)*(uint16 *)(cur_op + 2);
            found = fd2_find_char_by_id_or_template(portrait_target);
            portrait_flip = (found == -1) ? 0 : 0x70;
            pAlly = (runtime_char *)data_fd2_dialog_current_speaker_char_ptr;
            data_fd2_portrait_sprite_buffer =
                (uint8 *)fd2_load_dat_resource(0x51a70,
                                      (uint32)data_fd2_portrait_sprite_buffer,
                                      (uint32)pAlly->portrait_id);
            portrait_anim = fd2_play_dialog_open_animation(
                                (uint32)pAlly->pos_x,
                                (uint32)pAlly->pos_y, portrait_flip);
            sprite = (uint8 *)data_fd2_portrait_sprite_buffer;
            sprite = sprite + *sprite;
            fd2_dialog_sprite_blit_mirrored(
                data_fd2_dialog_active_portrait_blit_offset + 0xa0000,
                (uint32)sprite, 0x140);
            render_base = 0xa951f;
            render_pos  = 0xa951f;
            line_count  = 0;
            blink_flag  = 1;
            cur_op += 4;
            continue;
        }

        if (opcode == -0x13) {                       /* ALLY sprite slot1 */
            if (portrait_anim != 0) {
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
                fd2_close_dialog_panels_then_slide_in_at(portrait_anim,
                                                         portrait_flip);
            }
            data_fd2_dialog_active_portrait_blit_offset = 0x728;
            pSpeaker = data_fd2_battle_runtime_char_array_ptr
                       + *(uint16 *)(cur_op + 2);
            portrait_flip   = 2;
            portrait_target = (uint32)pSpeaker->portrait_id;
            data_fd2_portrait_sprite_buffer =
                (uint8 *)fd2_load_dat_resource(0x51a70,
                                      (uint32)data_fd2_portrait_sprite_buffer,
                                      portrait_target);
            portrait_anim = fd2_play_dialog_open_animation(
                                (uint32)pSpeaker->pos_x,
                                (uint32)pSpeaker->pos_y, portrait_flip);
            sprite = (uint8 *)data_fd2_portrait_sprite_buffer;
            sprite = sprite + *sprite;
            fd2_dialog_sprite_blit_normal(
                data_fd2_dialog_active_portrait_blit_offset + 0xa0000,
                (uint32)sprite, 0x140);
            render_base = 0xa0b4f;
            render_pos  = 0xa0b4f;
            line_count  = 0;
            blink_flag  = 1;
            cur_op += 4;
            continue;
        }

        if (opcode == -0x14) {                       /* ALLY sprite slot2 */
            if (portrait_anim != 0) {
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
                fd2_close_dialog_panels_then_slide_in_at(portrait_anim,
                                                         portrait_flip);
            }
            data_fd2_dialog_active_portrait_blit_offset = 0x9017;
            pAlly = data_fd2_battle_runtime_char_array_ptr
                    + *(uint16 *)(cur_op + 2);
            portrait_flip = 0x70;
            data_fd2_portrait_sprite_buffer =
                (uint8 *)fd2_load_dat_resource(0x51a70,
                                      (uint32)data_fd2_portrait_sprite_buffer,
                                      (uint32)pAlly->portrait_id);
            portrait_anim = fd2_play_dialog_open_animation(
                                (uint32)pAlly->pos_x,
                                (uint32)pAlly->pos_y, portrait_flip);
            sprite = (uint8 *)data_fd2_portrait_sprite_buffer;
            sprite = sprite + *sprite;
            fd2_dialog_sprite_blit_mirrored(
                data_fd2_dialog_active_portrait_blit_offset + 0xa0000,
                (uint32)sprite, 0x140);
            render_base = 0xa951f;
            render_pos  = 0xa951f;
            line_count  = 0;
            blink_flag  = 1;
            cur_op += 4;
            continue;
        }

        /* TEXT glyph (default) */
        fd2_blit_glyph_2bpp_with_outline(data_fd2_chinese_font_sheet,
                                         (uint32)opcode, render_pos,
                                         render_pitch, glyph_p5, glyph_p6,
                                         (uint16)glyph_p7);
        render_pos += 0x10;
        if (fd2_check_keyboard_buffer_nonempty() != 0) {
            blink_flag = 0;
        }
        cur_op = next_op;
        if (blink_flag != 0) {
            fd2_portrait_blink_animation_step();
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_portrait_blink_animation_step @ 0x164E8 (1 caller)
 *
 * One step of the talking-portrait mouth animation, called once per
 * rendered glyph from fd2_display_dialog_scene's per-glyph loop while
 * blink_flag is set.
 *
 * A 2-call subtick divider gates the visible-frame update: on every
 * 2nd call the internal frame counter advances 0->1->2->3->0, and the
 * value 3 is collapsed to 1 so the painted frames cycle 0,1,2,1.
 * Each call also fires the typewriter "click" SFX and paces one BIOS
 * tick to set the text-type speed.
 * ---------------------------------------------------------------- */
void fd2_portrait_blink_animation_step(void)
{
    uint32 paint_idx;

    data_fd2_dialog_portrait_blink_subtick_counter =
        data_fd2_dialog_portrait_blink_subtick_counter + 1;
    if (data_fd2_dialog_portrait_blink_subtick_counter == 2) {
        data_fd2_dialog_portrait_blink_frame_idx =
            data_fd2_dialog_portrait_blink_frame_idx + 1;
        if (data_fd2_dialog_portrait_blink_frame_idx == 4) {
            data_fd2_dialog_portrait_blink_frame_idx = 0;
        }
        paint_idx = data_fd2_dialog_portrait_blink_frame_idx;
        if (data_fd2_dialog_portrait_blink_frame_idx == 3) {
            paint_idx = 1;
        }
        fd2_paint_portrait_to_dialog_area(paint_idx);
        data_fd2_dialog_portrait_blink_subtick_counter = 0;
    }
    fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 2, 1);
    fd2_wait_n_bios_ticks(1);
}

/* ----------------------------------------------------------------
 * fd2_play_dialog_open_animation @ 0x165AC (1 caller)
 *
 * Animate a dialog box opening, then assemble its 5-layer frame.
 *
 * If dst_origin (flip) == 0, default it from the active portrait
 * mode (0x728 enemy -> 2 / 0x9017 ally -> 0x70). Otherwise pan the
 * battle cursor to (pos_x, pos_y) and linear-interpolate a sprite
 * blit from the cursor pixel toward dst_origin over (cursor_x +
 * cursor_y) frames.
 *
 * Always: allocate 5 ~27KB screen-save buffers (0x53A18..0x53A28)
 * and run the 5-stage frame assembly (top edge / top inner / middle
 * / lower inner / bottom edge), saving the underlying screen band
 * before each stage so the dialog can later be closed cleanly.
 *
 * Returns the head of the 5-buffer save array (= 0x53A18), used by
 * the caller / fd2_close_dialog_panels_then_slide_in_at to restore
 * the screen when the dialog closes.
 * ---------------------------------------------------------------- */
uint32 fd2_play_dialog_open_animation(uint32 pos_x, uint32 pos_y, uint32 flip)
{
    uint32 cursor_x_pixel;
    uint32 cursor_y_pixel;
    int    total_steps;
    int    step;
    int    i;
    int    interp_x;
    int    interp_y;
    uint32 sprite_addr;
    uint8 *sheet;
    uint32 width;

    if (flip == 0) {
        if (data_fd2_dialog_active_portrait_blit_offset == 0x728) {
            flip = 2;
        } else if (data_fd2_dialog_active_portrait_blit_offset == 0x9017) {
            flip = 0x70;
        }
    } else {
        data_fd2_battle_anim_phase = 0;
        fd2_pan_cursor_to_tile_animated((int)pos_x, (int)pos_y);
        data_fd2_battle_anim_phase = 1;
        cursor_x_pixel = data_fd2_battle_cursor_screen_x * 0x18 + 4;
        cursor_y_pixel = data_fd2_battle_cursor_screen_y * 0x18 + 4;
        total_steps = (int)(data_fd2_battle_cursor_screen_x +
                            data_fd2_battle_cursor_screen_y);
        if (total_steps != 0) {
            for (step = 0; step <= total_steps; step++) {
                interp_y = (int)cursor_y_pixel -
                           ((int)(cursor_y_pixel - flip) * step) / total_steps;
                interp_x = (int)cursor_x_pixel -
                           ((int)(cursor_x_pixel - 5) * step) / total_steps;
                sheet = (uint8 *)data_fd2_ui_anim_sprite_sheet_ptr;
                sprite_addr = data_fd2_ui_anim_sprite_sheet_ptr +
                              (uint32)(*(int16 *)(sheet + 6));
                data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[0] =
                    fd2_blit_indexed_sprite_with_alloc(sprite_addr, 0xa0000,
                                                       0x140, (uint32)interp_x,
                                                       (uint32)interp_y);
                __delay_thunk_375b2(10);
                fd2_clear_keyboard_buffer();
                fd2_cleanup_dialog_sprite_buffer(
                    (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[0],
                    0xa0000, 0x140);
            }
        }
    }

    for (i = 0; i < 5; i++) {
        data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[i] = malloc(0x682c);
    }

    width = flip * 0x140 + 5;

    fd2_save_screen_block_to_buffer(
        (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[0],
        0x136, 0x56, 0xa0000, width, 0x140);
    fd2_assemble_dialog_frame_layered(0xa0000, 0x140, 5, flip, 4, 2);
    __delay_thunk_375b2(10);

    fd2_save_screen_block_to_buffer(
        (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[1],
        0x136, 0x56, 0xa0000, width, 0x140);
    fd2_assemble_dialog_frame_layered(0xa0000, 0x140, 5, flip, 8, 3);
    __delay_thunk_375b2(10);

    fd2_save_screen_block_to_buffer(
        (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[2],
        0x136, 0x56, 0xa0000, width, 0x140);
    fd2_assemble_dialog_frame_layered(0xa0000, 0x140, 5, flip, 0xc, 4);
    __delay_thunk_375b2(10);

    fd2_save_screen_block_to_buffer(
        (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[3],
        0x136, 0x56, 0xa0000, width, 0x140);
    fd2_assemble_dialog_frame_layered(0xa0000, 0x140, 5, flip, 0x10, 5);
    __delay_thunk_375b2(10);

    fd2_save_screen_block_to_buffer(
        (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[4],
        0x136, 0x56, 0xa0000, width, 0x140);
    fd2_assemble_dialog_frame_layered(0xa0000, 0x140, 5, flip, 0x13, 5);

    fd2_clear_keyboard_buffer();
    return (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs;
}

/* ----------------------------------------------------------------
 * fd2_close_dialog_panels_then_slide_in_at @ 0x16B43 (1 caller)
 *
 * Tear down the open dialog's 5 layered frame buffers, optionally
 * followed by a slide-out animation toward (slide_to_y_pixel, 5).
 *
 * Phase 1: reverse-order cleanup of layers 4..1 (10ms pause between
 * each), then a final cleanup of layer 0.
 *
 * Phase 2 (only when slide_to_y_pixel != 0): interpolate a sprite
 * blit from the battle-cursor pixel back toward (slide_to_y_pixel, 5)
 * over (cursor_x + cursor_y) frames, reusing layer slot 0 as a
 * transient save buffer for each frame.
 *
 * Symmetric inverse of fd2_play_dialog_open_animation: closes in
 * reverse-Z order with the slide following the same step count.
 *
 * Note: matching the binary, the per-frame blit passes the
 * '5 - ...' interpolant as sheet_base (arg4) and the
 * 'slide_to_y_pixel - ...' interpolant as sprite_idx (arg5) — the
 * mirror image of the open animation's argument pairing.
 * ---------------------------------------------------------------- */
void fd2_close_dialog_panels_then_slide_in_at(uint32 anim_handle,
                                              uint32 slot_offset)
{
    uint32 *layer_ptr_array;
    int     slot;
    int     src_x_px;
    int     src_y_px;
    int     total_frames;
    int     frame;
    int     interp_y;
    int     interp_x;
    uint32  sprite_addr;
    uint8  *sheet;

    layer_ptr_array = (uint32 *)anim_handle;

    for (slot = 4; slot > 0; slot--) {
        fd2_cleanup_dialog_sprite_buffer(layer_ptr_array[slot], 0xa0000, 0x140);
        __delay_thunk_375b2(10);
    }
    fd2_cleanup_dialog_sprite_buffer(layer_ptr_array[0], 0xa0000, 0x140);

    if (slot_offset != 0) {
        src_x_px     = (int)(data_fd2_battle_cursor_screen_x * 0x18);
        src_y_px     = (int)(data_fd2_battle_cursor_screen_y * 0x18);
        total_frames = (int)(data_fd2_battle_cursor_screen_x +
                             data_fd2_battle_cursor_screen_y);
        if (total_frames != 0) {
            for (frame = 0; frame <= total_frames; frame++) {
                interp_y = 5 - ((5 - (src_x_px + 4)) * frame) / total_frames;
                interp_x = (int)slot_offset -
                           (((int)slot_offset - (src_y_px + 4)) * frame)
                               / total_frames;
                sheet = (uint8 *)data_fd2_ui_anim_sprite_sheet_ptr;
                sprite_addr = data_fd2_ui_anim_sprite_sheet_ptr +
                              (uint32)(*(int16 *)(sheet + 6));
                layer_ptr_array[0] =
                    (uint32)fd2_blit_indexed_sprite_with_alloc(
                                sprite_addr, 0xa0000, 0x140,
                                (uint32)interp_y, (uint32)interp_x);
                __delay_thunk_375b2(10);
                fd2_cleanup_dialog_sprite_buffer(layer_ptr_array[0],
                                                 0xa0000, 0x140);
            }
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_assemble_dialog_frame_layered @ 0x168B6 (4 callers)
 *
 * Compose a resizable dialog box from 17 sprite tiles in
 * _ui_and_anim_sprite_sheet, using a 4-corner / 4-secondary-corner /
 * 5-edge-stretcher / 1-inner-fill pattern.
 *
 *   topleft = dst + col_offset + row_offset * pitch
 *   row stride = pitch * 0x10 pixels per tile row
 *   col stride = 0x10 pixels per tile col
 *
 * Sprite-idx -> role:
 *   1 / 2   outer top corners (left / right)
 *   3 / 4   outer bottom corners (left / right)
 *   5       inner top-left corner
 *   6       inner top-right corner
 *   7 / 8   inner bottom corners (left / right)
 *   9       top inner edge (stretchable, repeated)
 *   A       left edge (stretchable, repeated)
 *   B       right edge (stretchable)
 *   C       bottom inner edge (stretchable)
 *   D       center background fill (repeated over entire interior)
 *   E / F   mid-row edges (left / right)
 *   10 / 11 bottom-row left/right secondary corners
 *
 * Used by fd2_play_dialog_open_animation (5-stage frame assembly),
 * fd2_load_chapter_portrait, fd2_play_final_chapter_30_ending and
 * fd2_render_status_screen_static_layout to produce dialog boxes of
 * arbitrary (n_cols, n_rows) tile dimensions.
 * ---------------------------------------------------------------- */
void fd2_assemble_dialog_frame_layered(uint32 dst, uint32 pitch,
                                       uint32 col_offset, int row_offset,
                                       int n_cols, int n_rows)
{
    uint32 n_cols_minus_2;
    uint32 row_pixels_16;
    uint32 pitch3;
    uint32 topleft;
    uint32 inner_top;
    uint32 inner_top_right;
    uint32 bottom_row_ofs;
    uint32 row_full;
    uint32 p;
    uint32 inner_origin;
    uint32 bottom_full;
    int    col;
    int    row;
    int    c;
    int    r;

    n_cols_minus_2 = (uint32)(n_cols - 2);
    row_pixels_16  = pitch * 0x10;
    pitch3         = pitch * 3;
    topleft        = col_offset + ((uint32)row_offset * pitch + dst);

    fd2_blit_sheet_sprite_at_offset(topleft, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 1);

    p = topleft + 3;
    fd2_blit_sheet_sprite_at_offset(p + (uint32)n_cols * 0x10, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 2);

    inner_top   = pitch3 + topleft;
    bottom_full = row_pixels_16 * (uint32)n_rows;
    fd2_blit_sheet_sprite_at_offset(inner_top + bottom_full, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 3);
    fd2_blit_sheet_sprite_at_offset((topleft + 3) + (uint32)n_cols * 0x10
                                        + pitch3 + bottom_full,
                                    pitch, data_fd2_ui_anim_sprite_sheet_ptr, 4);

    fd2_blit_sheet_sprite_at_offset(p, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 5);

    inner_top_right = n_cols_minus_2 * 0x10 + topleft + 0x13;
    fd2_blit_sheet_sprite_at_offset(inner_top_right, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 6);

    fd2_blit_sheet_sprite_at_offset(p + pitch3 + bottom_full, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 7);
    fd2_blit_sheet_sprite_at_offset(inner_top_right + pitch3 + bottom_full,
                                    pitch, data_fd2_ui_anim_sprite_sheet_ptr, 8);

    fd2_blit_sheet_sprite_at_offset(inner_top, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 0xe);

    inner_top_right = inner_top + 0x23 + n_cols_minus_2 * 0x10;
    fd2_blit_sheet_sprite_at_offset(inner_top_right, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 0xf);

    bottom_row_ofs = row_pixels_16 * (uint32)(n_rows - 1);
    fd2_blit_sheet_sprite_at_offset(inner_top + bottom_row_ofs, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 0x10);
    fd2_blit_sheet_sprite_at_offset(inner_top_right + bottom_row_ofs, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 0x11);

    if (0 < (int)n_cols_minus_2) {
        for (col = 0; col < (int)n_cols_minus_2; col++) {
            p = topleft + 0x13 + (uint32)col * 0x10;
            fd2_blit_sheet_sprite_at_offset(p, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 9);
            fd2_blit_sheet_sprite_at_offset(p + row_pixels_16 * (uint32)n_rows
                                                + pitch3,
                                    pitch, data_fd2_ui_anim_sprite_sheet_ptr,
                                    0xc);
        }
    }

    if (0 < n_rows - 2) {
        row = 0;
        while (row < n_rows - 2) {
            row++;
            row_full = row_pixels_16 * (uint32)row + pitch3 + topleft;
            fd2_blit_sheet_sprite_at_offset(row_full, pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr, 0xa);
            fd2_blit_sheet_sprite_at_offset((uint32)n_cols * 0x10 + row_full + 3,
                                    pitch, data_fd2_ui_anim_sprite_sheet_ptr,
                                    0xb);
        }
    }

    for (r = 0; r < n_rows; r++) {
        for (c = 0; c < n_cols; c++) {
            inner_origin = pitch3 + topleft + (uint32)c * 0x10 + 3;
            fd2_blit_sheet_sprite_at_offset(row_pixels_16 * (uint32)r
                                                + inner_origin,
                                    pitch, data_fd2_ui_anim_sprite_sheet_ptr,
                                    0xd);
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_backup_dialog_area_to_buffer @ 0x175A9 (1 caller)
 *
 * Snapshot a 0x48 x 0x48 pixel region of the working framebuffer to
 * data_fd2_dialog_area_backup_buffer for the "save under" restore-on-
 * close mechanism. Frees any previous backup, allocs a fresh
 * 0x1440 (= 0x48*0x48 = 5184) byte buffer, then copies 0x48 rows of
 * 0x48 bytes each.
 *
 * Source anchor: large_game_state_buffer + 0x8088, offset by
 * (cursor_screen_x - 1) tile-columns (0x18 px each) and
 * (cursor_screen_y - 1) tile-rows (0x2AC0 bytes each) — i.e. top-left
 * corner one tile up/left of the cursor (1-tile margin around it).
 * Working-buffer row stride is 0x1C8.
 *
 * Used by fd2_open_settings_dialog_with_slide before drawing a dialog;
 * fd2_restore_dialog_area_from_buffer puts the saved pixels back.
 * ---------------------------------------------------------------- */
void fd2_backup_dialog_area_to_buffer(void)
{
    uint32 src_row_ptr;
    uint32 row;

    if (data_fd2_dialog_area_backup_buffer != (void *)0) {
        free(data_fd2_dialog_area_backup_buffer);
    }
    data_fd2_dialog_area_backup_buffer = malloc(0x1440);

    src_row_ptr = (data_fd2_battle_cursor_screen_x - 1) * 0x18 +
                  data_fd2_large_game_state_buffer_ptr + 0x8088 +
                  (data_fd2_battle_cursor_screen_y - 1) * 0x2AC0;

    for (row = 0; (int32)row < 0x48; row++) {
        memmove((void *)(row * 0x48 + (uint32)data_fd2_dialog_area_backup_buffer),
                (void *)src_row_ptr, 0x48);
        src_row_ptr += 0x1C8;
    }
}

/* ----------------------------------------------------------------
 * fd2_cinematic_scroll_text_up_for_special_scenes @ 0x16E24 (2 callers)
 *
 * Scroll the dialog text area upward by 19 pixel rows (5*3 + 4) when
 * an active portrait is on screen
 * (data_fd2_dialog_active_portrait_blit_offset in {0x728, 0x9017}).
 * No portrait -> no scroll.
 *
 * fb_offset selects the left (0xA0B4F) or right (0xA951F) portrait
 * text area. Five outer passes each shift up by 3 rows (memmove 0xD0
 * bytes per row, 0x48 rows, src 3 rows below dst at stride 0x140) and
 * fill the bottom row with text-bg pixel 0x4A; a final pass shifts up
 * by 4 rows and clears the bottom row again.
 * ---------------------------------------------------------------- */
void fd2_cinematic_scroll_text_up_for_special_scenes(void)
{
    uint32 row_iter;
    int32  iVar1;
    uint32 fb_offset;
    uint32 outer_iter;

    if ((data_fd2_dialog_active_portrait_blit_offset == 0x728) ||
        (data_fd2_dialog_active_portrait_blit_offset == 0x9017)) {
        if (data_fd2_dialog_active_portrait_blit_offset == 0x728) {
            fb_offset = 0xa0b4f;
        }
        else {
            fb_offset = 0xa951f;
        }
        for (outer_iter = 0; (int32)outer_iter < 5; outer_iter = outer_iter + 1) {
            for (row_iter = 0; (int32)row_iter < 0x48; row_iter = row_iter + 1) {
                memmove((void *)(row_iter * 0x140 + fb_offset - 1),
                        (void *)((row_iter + 3) * 0x140 + fb_offset - 1), 0xd0);
            }
            memset((void *)(fb_offset + 0x5a00), 0x4a, 0xd0);
        }
        for (iVar1 = 0; iVar1 < 0x48; iVar1 = iVar1 + 1) {
            memmove((void *)((uint32)iVar1 * 0x140 + fb_offset - 1),
                    (void *)((uint32)(iVar1 + 4) * 0x140 + fb_offset - 1), 0xd0);
        }
        memset((void *)(fb_offset + 0x5a00), 0x4a, 0xd0);
    }
}
