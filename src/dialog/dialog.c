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
