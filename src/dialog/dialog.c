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
#include <dos.h>

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
 *   render_pitch also drives the per-line reflow (render_base +
 *   render_pitch * glyph_height * line_count).  glyph_p5/p6/p7 are
 *   passed straight through to fd2_blit_glyph_2bpp_with_outline as its
 *   fill_color / outline_color / bg_color palette indices.
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
                (uint8 *)fd2_load_dat_resource(
                                      (uint32)data_fd2_string_resource_filename_dato_dat_51a70,
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
                (uint8 *)fd2_load_dat_resource(
                                      (uint32)data_fd2_string_resource_filename_dato_dat_51a70,
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
                (uint8 *)fd2_load_dat_resource(
                                      (uint32)data_fd2_string_resource_filename_dato_dat_51a70,
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
                (uint8 *)fd2_load_dat_resource(
                                      (uint32)data_fd2_string_resource_filename_dato_dat_51a70,
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
                fd2_delay_ms(10);
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
    fd2_delay_ms(10);

    fd2_save_screen_block_to_buffer(
        (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[1],
        0x136, 0x56, 0xa0000, width, 0x140);
    fd2_assemble_dialog_frame_layered(0xa0000, 0x140, 5, flip, 8, 3);
    fd2_delay_ms(10);

    fd2_save_screen_block_to_buffer(
        (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[2],
        0x136, 0x56, 0xa0000, width, 0x140);
    fd2_assemble_dialog_frame_layered(0xa0000, 0x140, 5, flip, 0xc, 4);
    fd2_delay_ms(10);

    fd2_save_screen_block_to_buffer(
        (uint32)data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[3],
        0x136, 0x56, 0xa0000, width, 0x140);
    fd2_assemble_dialog_frame_layered(0xa0000, 0x140, 5, flip, 0x10, 5);
    fd2_delay_ms(10);

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
 * Tear down the open dialog's 5 layered frame buffers, then
 * optionally retract the panel back toward the battle cursor.
 *
 * anim_handle is the restore handle returned by
 * fd2_play_dialog_open_animation: a pointer to the 5-entry layer
 * save-buffer array. slot_offset is the portrait slot offset the
 * caller (fd2_display_dialog_scene) tracked for the open box (0 / 2 /
 * 0x70); it doubles as the panel's resting screen-Y column for the
 * retract slide.
 *
 * Phase 1: reverse-order cleanup of layers 4..1 (10ms pause between
 * each), then a final cleanup of layer 0.
 *
 * Phase 2 (only when slot_offset != 0): slide the dialog sprite from
 * its resting position (screen X=5, Y=slot_offset) back to the battle
 * cursor pixel (cursor_x*0x18+4, cursor_y*0x18+4) over
 * (cursor_x + cursor_y) frames, reusing layer slot 0 as a transient
 * save buffer for each frame.
 *
 * Exact directional inverse of the open animation's slide-in
 * (fd2_play_dialog_open_animation): that runs cursor -> (5,
 * slot_offset); this runs (5, slot_offset) -> cursor, and closes the
 * frame layers in reverse-Z order over the same step count.
 *
 * Note: the per-frame blit passes the local '5 - ...' interpolant as
 * sheet_base (arg4, screen X) and the local 'slot_offset - ...'
 * interpolant as sprite_idx (arg5, screen Y) -- the mirror of the
 * open animation's argument pairing.
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
        fd2_delay_ms(10);
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
                fd2_delay_ms(10);
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

/* ----------------------------------------------------------------
 * fd2_restore_dialog_area_from_buffer @ 0x17643 (2 callers)
 *
 * Inverse of fd2_backup_dialog_area_to_buffer. Copies the saved
 * 0x48 x 0x48 pixel patch from data_fd2_dialog_area_backup_buffer
 * back into the working framebuffer at the cursor corner, restoring
 * the background that was hidden by an overlaid settings / status
 * dialog.
 *
 * Destination anchor: large_game_state_buffer + 0x8088, offset by
 * (cursor_screen_x - 1) tile-columns (0x18 px each) and
 * (cursor_screen_y - 1) tile-rows (0x2AC0 bytes each).
 * Working-buffer row stride is 0x1C8.
 *
 * Callers: fd2_open_settings_dialog_with_slide,
 *          fd2_close_settings_dialog_with_slide.
 * ---------------------------------------------------------------- */
void fd2_restore_dialog_area_from_buffer(void)
{
    uint32 dst_row_ptr;
    uint32 row;

    dst_row_ptr = (data_fd2_battle_cursor_screen_x - 1) * 0x18 +
                  data_fd2_large_game_state_buffer_ptr + 0x8088 +
                  (data_fd2_battle_cursor_screen_y - 1) * 0x2AC0;

    for (row = 0; (int32)row < 0x48; row++) {
        memmove((void *)dst_row_ptr,
                (void *)(row * 0x48 + (uint32)data_fd2_dialog_area_backup_buffer),
                0x48);
        dst_row_ptr += 0x1C8;
    }
}

/* ----------------------------------------------------------------
 * fd2_animate_dialog_page_advance_collapse @ 0x197E5 (17 callers)
 *
 * Dialog "page-advance / fold-in" transition animation. Played after a
 * Yes/No prompt confirm: the current dialog box's two corner sprites
 * fold inward toward the center while the underlying scene (battle map
 * or next page) is repainted.
 *
 * corner_state[] is the binary's 16-byte (4-dword) on-stack scratch:
 *   corner_state[0] = template[0] (0x10)  sprite-index selector, corner 0
 *   corner_state[1] = template[1] (0x11)  sprite-index selector, corner 1
 *   corner_state[2] = left-corner offset  init -16 (0xFFFFFFF0), +16 over 4 frames
 *   corner_state[3] = right-corner offset init +16 (0x10),       -16 over 4 frames
 * The per-corner blit loop reads selector = corner_state[i] and
 * offset = corner_state[i+2]; this mirrors the vendor's adjacent-locals
 * layout (template dwords sit directly before the two animated offsets).
 *
 * If battle_tile_map > 1 the battle base scene is primed first (palette
 * tick + tile-map composite + char overlay). Each of 4 frames advances
 * both corner offsets by 4 toward center, copies 0x56 rows of the
 * composed work buffer into the game-state work buffer, blits the two
 * corner sprites at their current offsets, then flushes the dialog band
 * to the framebuffer. A final settle pass copies the bottom 0x56 rows of
 * the composed work buffer straight to the framebuffer.
 *
 * void __cdecl with the __CHK(0x3c) stack-probe prologue; the body's
 * trailing JMP 0x17E03 is the shared Watcom epilogue of
 * fd2_render_horizontal_bar_segments (== return).
 * ---------------------------------------------------------------- */
void fd2_animate_dialog_page_advance_collapse(void)
{
    uint32 corner_state[4];
    uint32 yes_no_box_addr;
    int    frame;
    int    i;
    uint32 row;
    uint32 selector;
    uint32 sprite_addr;

    corner_state[0] = (uint32)data_fd2_dialog_advance_collapse_template[0];
    corner_state[1] = (uint32)data_fd2_dialog_advance_collapse_template[1];
    yes_no_box_addr = data_fd2_large_game_state_buffer_ptr + 0x1A59C;
    corner_state[2] = 0xFFFFFFF0;
    corner_state[3] = 0x10;

    if (1 < data_fd2_battle_tile_map_ptr) {
        fd2_tick_chapter_palette_animation();
        fd2_composite_battle_tile_map(
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8, 0xD, 8,
            data_fd2_battle_view_window_origin_x,
            data_fd2_battle_view_window_origin_y);
        fd2_composite_all_chars_overlay();
    }

    for (frame = 0; frame < 4; frame++) {
        corner_state[2] += 4;
        corner_state[3] -= 4;

        for (row = 0; (int32)row < 0x56; row++) {
            memmove((void *)((row + 0x6C) * 0x1C8
                             + data_fd2_large_game_state_buffer_ptr + 0x8089),
                    (void *)(row * 0x140
                             + data_fd2_ui_slide_composed_target_buf_ptr
                             + 0x8C05),
                    0x136);
        }

        for (i = 0; i < 2; i++) {
            selector    = corner_state[i];
            sprite_addr = *(uint32 *)(data_fd2_menu_dialog_state_handle
                                      + selector * 0xC)
                          + data_fd2_menu_dialog_state_handle;
            fd2_blit_sprite_with_stride_setup(
                corner_state[i + 2] + yes_no_box_addr, sprite_addr, 0x1C8);
        }

        fd2_blit_rectangle(0xA0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1C8, 0x138, 0xC0);
    }

    for (row = 0; (int32)row < 0x56; row++) {
        memmove((void *)(row * 0x140 + 0xA8C05),
                (void *)(data_fd2_ui_slide_composed_target_buf_ptr + 5
                         + (row + 0x70) * 0x140),
                0x136);
    }
}

/* ----------------------------------------------------------------
 * fd2_text_dialog_typewriter_loop @ 0x19953 (17 callers)
 *
 * Yes/No prompt: typewriter character animation + input loop, the
 * confirm-time companion of fd2_animate_dialog_page_advance_collapse.
 *
 * Setup: snapshot VRAM (0xA0000) into the slide-composed work buffer,
 * copy the full 200x320 page back into the game-state work buffer
 * (row -4..199, stride 0x1C8), prime the battle base scene when
 * battle_tile_map > 1, then play a 4-frame intro that folds the two
 * Yes/No corner sprites OUTWARD (left corner -4/frame, right +4/frame).
 *
 * Main loop (runs until a key event):
 *   - throttle on the BIOS tick word @ 0x46C: a frame only advances
 *     when (cur - latch) >= 2 ticks (or the tick wrapped negative).
 *   - oscillator (0x53C13) cycles 0..3 each accepted frame.
 *   - re-composite the battle base scene when battle_tile_map > 1.
 *   - typewriter: char_phase==1 paints the current glyph
 *     (blit_normal when battle_tile_map==0, else blit_mirrored) and
 *     reseeds the inter-glyph pace = rng()%0x1E + 10; otherwise the
 *     pace counter counts down and, on reaching 0, starts the next
 *     glyph (offset *(buf+0xC)) and re-arms char_phase.
 *   - copy the work buffer back to the framebuffer (full page when
 *     battle_tile_map<2, else the small in-battle dialog band).
 *   - draw the two Yes/No boxes (selector = corner_state[i]*3, plus
 *     oscillator/2 on the currently selected box), then flush via
 *     fd2_blit_rectangle.
 *
 * Input dispatch (INT 16h via int386, scancode read from
 * key_input_mode @ 0x53A8E):
 *   0xE0 / 0x52 / 0x1C / 0x39  -> return 1  (Enter/Space/extended -> Yes/advance)
 *   0x01 / 0x53                -> return -1 (Esc/Numpad-. -> cancel)
 *   0x4B (Left)  -> cursor = 0 (Yes)
 *   0x4D (Right) -> cursor = 1 (No)
 *   else         -> keep looping
 * The oscillator is reset to 0 on every exit path.
 *
 * EAX-bug note: Ghidra renders both pace seeds off a clobbered
 * register (__CHK's result for the init seed, the blit return for the
 * per-glyph seed). The assembly seeds each from a *fresh*
 * fd2_advance_rng_state() return (CALL 0x4E893 then IDIV 0x1E),
 * reproduced faithfully here. corner_state[] mirrors the vendor's
 * adjacent-locals layout: [0..1] = template[2..3] sprite selectors,
 * [2..3] = the two animated corner offsets.
 * ---------------------------------------------------------------- */
int fd2_text_dialog_typewriter_loop(void)
{
    int32  corner_state[4];
    uint32 yes_no_box_addr;
    int    frame;
    int    i;
    int    row;
    int    pace_counter;
    int    selector;
    uint8  char_phase;
    int    tick_diff;
    uint8 *glyph_src;
    uint32 dst;

    corner_state[0] = data_fd2_dialog_advance_collapse_template[2];
    corner_state[1] = data_fd2_dialog_advance_collapse_template[3];
    char_phase      = 0;
    data_fd2_ui_menu_cursor_idx = 0;
    pace_counter    = (int)fd2_advance_rng_state() % 0x1E + 2;
    yes_no_box_addr = data_fd2_large_game_state_buffer_ptr + 0x1A59C;
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)0xA0000, 64000);
    corner_state[2] = 0;
    corner_state[3] = 0;

    for (row = 0; row < 200; row++) {
        memmove((void *)((uint32)(row - 4) * 0x1C8
                         + data_fd2_large_game_state_buffer_ptr + 0x8084),
                (void *)((uint32)row * 0x140
                         + (uint32)data_fd2_ui_slide_composed_target_buf_ptr),
                0x140);
    }

    if (1 < data_fd2_battle_tile_map_ptr) {
        fd2_tick_chapter_palette_animation();
        fd2_composite_battle_tile_map(
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8, 0xD, 8,
            data_fd2_battle_view_window_origin_x,
            data_fd2_battle_view_window_origin_y);
        fd2_composite_all_chars_overlay();
    }

    for (frame = 0; frame < 4; frame++) {
        corner_state[2] -= 4;
        corner_state[3] += 4;

        for (i = 0; i < 0x56; i++) {
            memmove((void *)(data_fd2_large_game_state_buffer_ptr + 0x8089
                             + (uint32)(i + 0x6C) * 0x1C8),
                    (void *)((uint32)data_fd2_ui_slide_composed_target_buf_ptr
                             + (uint32)i * 0x140 + 0x8C05),
                    0x136);
        }

        for (i = 0; i < 2; i++) {
            fd2_blit_sprite_with_stride_setup(
                (uint32)corner_state[i + 2] + yes_no_box_addr,
                *(uint32 *)(data_fd2_menu_dialog_state_handle
                            + (uint32)corner_state[i] * 0xC)
                    + data_fd2_menu_dialog_state_handle,
                0x1C8);
        }

        fd2_blit_rectangle(0xA0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1C8, 0x138, 0xC0);
    }

    for (;;) {
        while (fd2_check_keyboard_buffer_nonempty() == 0) {
            fd2_update_palette_cycle_anim();

            tick_diff = (int)(int16)BIOS_TICK_WORD
                      - (int)data_fd2_dialog_blink_phase_oscillator_tick_latch;
            if (tick_diff < 2 && tick_diff >= 0) {
                continue;
            }

            data_fd2_dialog_blink_phase_oscillator++;
            if (data_fd2_dialog_blink_phase_oscillator == 4) {
                data_fd2_dialog_blink_phase_oscillator = 0;
            }
            data_fd2_dialog_blink_phase_oscillator_tick_latch =
                (uint32)(int16)BIOS_TICK_WORD;

            if (1 < data_fd2_battle_tile_map_ptr) {
                fd2_tick_chapter_palette_animation();
                fd2_composite_battle_tile_map(
                    data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8,
                    0xD, 8, data_fd2_battle_view_window_origin_x,
                    data_fd2_battle_view_window_origin_y);
                fd2_composite_all_chars_overlay();
            }

            if (char_phase != 0) {
                glyph_src = data_fd2_portrait_sprite_buffer
                          + *data_fd2_portrait_sprite_buffer;
                dst = (uint32)data_fd2_ui_slide_composed_target_buf_ptr
                    + data_fd2_dialog_active_portrait_blit_offset;
                if (data_fd2_battle_tile_map_ptr == 0) {
                    fd2_dialog_sprite_blit_normal(dst, (uint32)glyph_src, 0x140);
                } else {
                    fd2_dialog_sprite_blit_mirrored(dst, (uint32)glyph_src,
                                                    0x140);
                }
                pace_counter = (int)fd2_advance_rng_state() % 0x1E + 10;
                char_phase = 0;
            } else {
                if (pace_counter == 0) {
                    glyph_src = data_fd2_portrait_sprite_buffer
                              + *(int *)(data_fd2_portrait_sprite_buffer + 0xC);
                    dst = (uint32)data_fd2_ui_slide_composed_target_buf_ptr
                        + data_fd2_dialog_active_portrait_blit_offset;
                    if (data_fd2_battle_tile_map_ptr == 0) {
                        fd2_dialog_sprite_blit_normal(dst, (uint32)glyph_src,
                                                      0x140);
                    } else {
                        fd2_dialog_sprite_blit_mirrored(dst, (uint32)glyph_src,
                                                        0x140);
                    }
                    char_phase = 1;
                }
                pace_counter--;
            }

            if (data_fd2_battle_tile_map_ptr < 2) {
                for (i = 0; i < 200; i++) {
                    memmove((void *)(data_fd2_large_game_state_buffer_ptr
                                     + 0x8084 + (uint32)(i - 4) * 0x1C8),
                            (void *)((uint32)data_fd2_ui_slide_composed_target_buf_ptr
                                     + (uint32)i * 0x140),
                            0x140);
                }
            } else {
                for (i = 0; i < 0x56; i++) {
                    memmove((void *)(data_fd2_large_game_state_buffer_ptr
                                     + 0x8089 + (uint32)(i + 0x6C) * 0x1C8),
                            (void *)((uint32)data_fd2_ui_slide_composed_target_buf_ptr
                                     + (uint32)i * 0x140 + 0x8C05),
                            0x136);
                }
            }

            for (i = 0; i < 2; i++) {
                selector = corner_state[i] * 3;
                if ((uint32)i == data_fd2_ui_menu_cursor_idx) {
                    selector += (int)(data_fd2_dialog_blink_phase_oscillator / 2);
                }
                fd2_blit_sprite_with_stride_setup(
                    (uint32)corner_state[i + 2] + yes_no_box_addr,
                    *(uint32 *)(data_fd2_menu_dialog_state_handle
                                + (uint32)selector * 4)
                        + data_fd2_menu_dialog_state_handle,
                    0x1C8);
            }

            fd2_blit_rectangle(0xA0504, 0x140,
                               data_fd2_large_game_state_buffer_ptr + 0x8088,
                               0x1C8, 0x138, 0xC0);
        }

        data_fd2_input_key_input_mode = 0x10;
        int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                     (union REGS *)&data_fd2_input_last_key_pressed);
        if (data_fd2_input_key_input_mode == 0xE0
            || data_fd2_input_key_input_mode == 0x52
            || data_fd2_input_key_input_mode == 0x1C
            || data_fd2_input_key_input_mode == 0x39) {
            break;
        }
        if (data_fd2_input_key_input_mode == 0x01
            || data_fd2_input_key_input_mode == 0x53) {
            data_fd2_dialog_blink_phase_oscillator = 0;
            return -1;
        }
        if (data_fd2_input_key_input_mode == 0x4B) {
            data_fd2_ui_menu_cursor_idx = 0;
        } else if (data_fd2_input_key_input_mode == 0x4D) {
            data_fd2_ui_menu_cursor_idx = 1;
        }
    }

    data_fd2_dialog_blink_phase_oscillator = 0;
    return 1;
}

/* ----------------------------------------------------------------
 * fd2_scroll_text_screen_up_by_lines @ 0x24D22 (3 callers)
 *
 * Dual-mode scroll-up helper over the static background buffer
 * (data_fd2_graphics_static_bg_buffer_ptr, 0xC0 lines x 0x138
 * bytes/line).
 *
 * Mode A (lines != 0): store low byte of lines into the pending
 * line-count state and return; this pre-sets how many rows the next
 * Mode-B call will scroll.
 *
 * Mode B (lines == 0): read N = pending line count and scroll the
 * whole 0xC0-line buffer up by N lines, with the bottom N lines
 * wrapping to the top (cylinder scroll):
 *   1. malloc(N * 0x138) scratch buffer.
 *   2. copy the bottom N rows (rows [0xC0-N .. 0xBF]) into scratch.
 *   3. for i = 0xBF-N down to 0: move row[i] down to row[i+N]
 *      (content visually moves up).
 *   4. paste the saved bottom N rows at the top.
 *   5. free scratch.
 *
 * Cdecl, 1 stack param; void return. The binary's __CHK(0x18)
 * stack-probe prologue is compiler-injected, not emitted here.
 * ---------------------------------------------------------------- */
void fd2_scroll_text_screen_up_by_lines(uint32 lines)
{
    void *scratch;
    int32 i;

    if (lines != 0) {
        data_fd2_graphics_text_scroll_pending_line_count = (uint8)lines;
        return;
    }

    scratch = malloc((uint32)data_fd2_graphics_text_scroll_pending_line_count * 0x138);
    memmove(scratch,
            (void *)((0xC0 - (uint32)data_fd2_graphics_text_scroll_pending_line_count) * 0x138 +
                     data_fd2_graphics_static_bg_buffer_ptr),
            (uint32)data_fd2_graphics_text_scroll_pending_line_count * 0x138);

    for (i = 0xBF - (int32)(uint32)data_fd2_graphics_text_scroll_pending_line_count; i >= 0; i--) {
        void *src = (void *)(i * 0x138 + data_fd2_graphics_static_bg_buffer_ptr);
        memmove((void *)((uint32)src +
                         (uint32)data_fd2_graphics_text_scroll_pending_line_count * 0x138),
                src, 0x138);
    }

    memmove((void *)data_fd2_graphics_static_bg_buffer_ptr, scratch,
            (uint32)data_fd2_graphics_text_scroll_pending_line_count * 0x138);
    free(scratch);
}

/* ----------------------------------------------------------------
 * fd2_close_intro_dialog_with_slide_out @ 0x2D31B (19 callers)
 *
 * Close a chapter-intro / menu dialog panel with a 5-frame slide-down
 * animation, restore the framebuffer from a backup snapshot, then free
 * the three dialog workspace buffers. Same algorithmic shape as
 * fd2_close_status_screen_with_slide_out @ 0x196CB, except this variant
 * does NOT recomposite a battle frame afterward (chapter-transition flow
 * rather than the in-battle status flow).
 *
 * Pipeline:
 *   1. 5-frame slide-down (frame_iter 1..5): each frame drives
 *      fd2_slide_panel_down_step(frame_iter*0xD + 0x70, accumulator, target)
 *      (panel_y = 0x7D, 0x8A, 0x97, 0xA4, 0xB1).
 *   2. memmove(0xA0000, bg_snapshot, 64000) — restore the screen snapshot
 *      to mode-13h VRAM.
 *   3. free the three 64000-byte workspaces (a / b / c).
 *
 * Globals (allocated by the open counterpart, freed here):
 *   render_workspace_a @ 0x53C5B — per-frame animation accumulator
 *   render_workspace_b @ 0x53C5F — underlying framebuffer snapshot
 *   render_workspace_c @ 0x53C63 — composed dialog-panel target image
 *
 * void __cdecl with the compiler-injected __CHK(0x14) stack-probe prologue
 * (not part of the source). EBX is the loop counter (callee-saved); the
 * trailing POP EBX + RET is the shared epilogue.
 * ---------------------------------------------------------------- */
void fd2_close_intro_dialog_with_slide_out(void)
{
    uint32 frame_iter;

    for (frame_iter = 1; (int)frame_iter < 6; frame_iter++) {
        fd2_slide_panel_down_step(frame_iter * 0xd + 0x70,
            data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr);
    }

    memmove((void *)0xa0000,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
    free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    free((void *)data_fd2_ui_slide_composed_target_buf_ptr);
}

/* ----------------------------------------------------------------
 * fd2_show_portrait_dialog_with_input @ 0x2C39B (1 caller)
 *
 * Display a portrait + dialog scene and block on user input. Used by
 * fd2_play_game_ending_cinematic (sole caller) for the per-character
 * ending epilogue dialogs.
 *
 * Sequence (fixed, no branches):
 *   fd2_clear_keyboard_buffer()
 *   fd2_load_chapter_portrait(portrait_id)        // loads from DATO.DAT
 *   fd2_clear_keyboard_buffer()
 *   fd2_display_dialog_scene(data_fd2_current_chapter_text, text_idx,
 *                            0xA9514, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1)
 *   fd2_paint_portrait_to_dialog_area(0)
 *   fd2_wait_for_input_dialog_with_blink(0)        // blocking cursor blink
 *   fd2_close_intro_dialog_with_slide_out()
 *   fd2_clear_keyboard_buffer()
 *
 * data_fd2_current_chapter_text (0x53A79) is the active FDTXT dialog source block.
 * The display-scene return value is discarded. Cdecl, 2 stack params;
 * void return. The binary's __CHK(0x2c) stack-probe prologue is
 * compiler-injected, not emitted here.
 * ---------------------------------------------------------------- */
void fd2_show_portrait_dialog_with_input(uint32 portrait_id, uint32 text_idx)
{
    fd2_clear_keyboard_buffer();
    fd2_load_chapter_portrait(portrait_id);
    fd2_clear_keyboard_buffer();
    fd2_display_dialog_scene(data_fd2_current_chapter_text, text_idx, 0xa9514, 0x140,
                             0xcd, 0x4c, 0x4a, 0x13, 1);
    fd2_paint_portrait_to_dialog_area(0);
    fd2_wait_for_input_dialog_with_blink(0);
    fd2_close_intro_dialog_with_slide_out();
    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * data_fd2_graphics_text_scroll_pending_line_count @ 0x51A10
 *
 * Pending line-count state for fd2_scroll_text_screen_up_by_lines
 * (background-buffer cylinder scroll). Mode A stores the low byte of
 * the row count here (MOV [0x51A10],AL); Mode B reads it back
 * unsigned (MOVZX, byte ptr) to drive the scroll. Single writable
 * unsigned byte; static initial value 0x01.
 * ---------------------------------------------------------------- */
uint8 data_fd2_graphics_text_scroll_pending_line_count = 1;

/* ----------------------------------------------------------------
 * data_fd2_dialog_portrait_blink_frame_idx @ 0x53A10
 *
 * Internal frame counter for the talking-portrait mouth animation in
 * fd2_portrait_blink_animation_step. Advanced once every 2 calls
 * (gated by the subtick divider at 0x53A14); cycles 0 -> 1 -> 2 -> 3
 * -> 0, with frame 3 collapsed to 1 when painted (visible sequence
 * 0/1/2/1). Accessed exclusively as dword ptr (INC / CMP ...,0x4 /
 * MOV ...,0 / MOV EAX,[...]) -> unsigned 32-bit. Pure runtime state:
 * relies on zero-initialization at startup (the 0..3 cycle is correct
 * only from initial 0); no static initializer.
 * ---------------------------------------------------------------- */
uint32 data_fd2_dialog_portrait_blink_frame_idx;

/* ----------------------------------------------------------------
 * data_fd2_dialog_portrait_blink_subtick_counter @ 0x53A14
 *
 * 2-call subtick divider gating the talking-portrait mouth animation
 * in fd2_portrait_blink_animation_step. Incremented every call; when
 * it reaches 2 the frame counter at 0x53A10 advances and it resets to
 * 0 (cycles 0/1 across calls, so the visible frame updates once per 2
 * calls). Accessed exclusively as dword ptr (INC [...] / CMP ...,0x2 /
 * MOV ...,0) -> unsigned 32-bit. Pure runtime state: relies on
 * zero-initialization at startup (first call INCs from 0); no static
 * initializer.
 * ---------------------------------------------------------------- */
uint32 data_fd2_dialog_portrait_blink_subtick_counter;

/* ----------------------------------------------------------------
 * data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs @ 0x53A18
 *
 * 5-entry array of save-buffer pointers (void *[5], 20 bytes total),
 * one per assemble stage of the layered dialog frame. Populated at
 * runtime by fd2_play_dialog_open_animation: the open loop fills each
 * slot via malloc(0x682C) (asm: MOV [ESI*4 + 0x53A18], EAX, stride 4,
 * ESI = 0..4), then every slot is read back as a 32-bit pointer arg to
 * fd2_save_screen_block_to_buffer (slots 0x53A18 / 0x53A1C / 0x53A20 /
 * 0x53A24 / 0x53A28) to snapshot the band of screen each stage covers.
 * fd2_close_dialog_panels_then_slide_in_at later frees the buffers in
 * reverse-Z order to restore the screen when the dialog closes; the
 * array head address is returned to the caller as the restore handle.
 * Pure runtime state: every slot is written (malloc / blit result)
 * before it is read, so it relies on zero-initialization at startup;
 * no static initializer.
 * ---------------------------------------------------------------- */
void *data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[5];

/* ----------------------------------------------------------------
 * data_fd2_dialog_area_backup_buffer @ 0x53A71
 *
 * Single heap pointer (void *) for the dialog "save under" snapshot.
 * fd2_backup_dialog_area_to_buffer frees any prior buffer, then sets
 * this to malloc(0x1440) (a 0x48 x 0x48 = 5184-byte pixel patch) and
 * fills it row by row from the working framebuffer at the cursor
 * corner; fd2_restore_dialog_area_from_buffer reads it back as the
 * memmove source to repaint the hidden background when the dialog
 * closes. Accessed exclusively as dword ptr (CMP [...] ,0x0 / PUSH
 * dword ptr [...] to free / MOV [...] ,EAX from malloc / MOV EDX,
 * dword ptr [...] base) -> a 4-byte pointer. Pure runtime state:
 * first use is a write (the malloc store after a NULL check), so it
 * relies on zero-initialization at startup; no static initializer.
 * ---------------------------------------------------------------- */
void *data_fd2_dialog_area_backup_buffer;

/* ----------------------------------------------------------------
 * data_fd2_portrait_sprite_buffer @ 0x53A85
 *
 * Single heap pointer (uint8 *) to the currently loaded portrait
 * sprite blob (a DATO.DAT resource). The dialog/status-screen open
 * paths reload it with the running pattern
 *   data_fd2_portrait_sprite_buffer =
 *       fd2_load_dat_resource("DATO.DAT",
 *                             (uint32)data_fd2_portrait_sprite_buffer,
 *                             portrait_id);
 * i.e. the prior pointer is handed back to the loader as the reusable
 * buffer and the fresh pointer is stored. Consumers then dereference
 * it as a byte buffer whose first byte is the header-size offset:
 * sprite_pixels = data_fd2_portrait_sprite_buffer +
 *                 *data_fd2_portrait_sprite_buffer (skip header).
 * Accessed exclusively as dword ptr (asm: MOV [0x53A85],EAX from the
 * loader return; PUSH dword ptr [0x53A85] back into the loader; MOVZX
 * EBX,byte ptr [EAX] to read the header offset) -> a 4-byte pointer.
 * Writers: fd2_display_dialog_scene, fd2_load_chapter_portrait,
 * fd2_render_status_screen_static_layout, fd2_run_equip_member_menu,
 * fd2_run_status_screen_member_menu, fd2_play_final_chapter_30_ending.
 * Pure runtime state: first use is the loader-return write, so it
 * relies on zero-initialization at startup; no static initializer.
 * ---------------------------------------------------------------- */
uint8 *data_fd2_portrait_sprite_buffer;

/* ----------------------------------------------------------------
 * data_fd2_dialog_active_portrait_blit_offset @ 0x53C67
 *
 * Active dialog/portrait blit offset: a mode-13h (320x200) linear
 * pixel offset added to a framebuffer base (0xA0000 VRAM or a 64000-
 * byte composed-overlay workspace) to position the current portrait /
 * dialog panel. Also doubles as the active-dialog-mode selector that
 * readers test to pick a rendering path.
 *
 * Writers set it to one of a small set of slot offsets per scene:
 *   0x728  in-field NPC dialog slot      0x9017 status/wide slot
 *   0xC88  status-screen portrait slot   0x10BB/0x6AB/0xF63/0x576/
 *   0xE3C  five chapter-intro special-positioned portraits
 * and reset it to 0 on the dialog-close path (the "no active dialog"
 * sentinel that readers compare against, e.g. != 0x9017 / == 0x728).
 *
 * Accessed exclusively as dword ptr (asm: MOV dword ptr [0x53C67],imm
 * to set a slot; CMP dword ptr [0x53C67],0x728 / 0x9017 to branch;
 * ADD EAX,dword ptr [0x53C67] to bias the blit base) -> unsigned
 * 32-bit. The largest value 0x9017 (36887) exceeds a signed 16-bit
 * range, confirming a >=32-bit unsigned cell; tests round-trip a full
 * 0xABCD1234 through it. Pure runtime state: every scene writes a slot
 * (or 0) before the readers run, so it relies on zero-initialization
 * at startup; no static initializer.
 * ---------------------------------------------------------------- */
uint32 data_fd2_dialog_active_portrait_blit_offset;
