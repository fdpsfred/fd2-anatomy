/*
 * anispell.c — full-screen ANI.DAT cinematic / slideshow playback.
 *
 * fd2_play_ani_file_animation_sequence @ 0x20421 (4 callers)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * fd2_play_ani_file_animation_sequence @ 0x20421  (4 callers)
 *
 * Plays a multi-frame ANI.DAT animation onto the VGA frame buffer
 * (0xA0000, mode 13h) through the shared ANI decoder.
 *
 * Params:
 *   anim_idx        = animation index. Its TOC entry sits at file
 *                     offset anim_idx*4 + 6 in ANI.DAT.
 *   per_frame_delay = per-frame wait in BIOS ticks (~55ms each).
 *   skip_on_key_flag= if non-zero, the loop polls the keyboard buffer
 *                     and breaks early on any keystroke.
 *
 * ANI.DAT layout: a 6-byte prefix, then a u32 absolute-offset TOC from
 * file offset 6 (entry i at 6+i*4). Each animation stream begins with an
 * 0xAD-byte header whose [0xA5] word is the frame count; each frame is an
 * 8-byte header ([0]=raw byte count, [2]=decoded chunk count) followed by
 * the raw frame bytes.
 *
 * anim_idx == 1 is the intro animation: it also loads FDOTHER.DAT[0x4E] as
 * a chime SFX, fires it on frame 0, and stops + frees it on exit.
 *
 * The original's tail is a JMP into the shared __CHK epilogue at 0x19518
 * (ADD ESP / POP regs / RET); emitted here as a plain return. See
 * emit_issues.json.
 * ---------------------------------------------------------------- */
void fd2_play_ani_file_animation_sequence(uint32 anim_idx,
                                          uint32 per_frame_delay,
                                          uint32 skip_on_key_flag)
{
    uint32  sfx_buf;
    void   *scratch;
    void   *backbuf;
    void   *fp;
    int16   frame_count;
    int     frame_iter;
    uint8   frame_header[8];
    uint16  frame_data_size;
    uint16  frame_decoded_size;

    sfx_buf = 0;
    fd2_clear_keyboard_buffer();
    if (anim_idx == 1) {
        sfx_buf = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat, 0, 0x4e);
    }

    scratch = malloc(0x300);
    backbuf = malloc(64000);
    fd2_ani_decoder_set_target_buffer(64000, 0xa0000, (uint32)scratch);

    fp = fopen("ANI.DAT", "rb");
    fseek(fp, (long)(anim_idx * 4 + 6), SEEK_SET);
    fread(backbuf, 1, 8, fp);
    fseek(fp, (long)*(uint32 *)backbuf, SEEK_SET);
    fread(backbuf, 1, 0xad, fp);
    frame_count = *(int16 *)((uint8 *)backbuf + 0xa5);

    for (frame_iter = 0; frame_iter < (int)frame_count; frame_iter++) {
        fread(frame_header, 1, 8, fp);
        frame_data_size = *(uint16 *)frame_header;
        frame_decoded_size = *(uint16 *)(frame_header + 2);
        fread(backbuf, 1, (uint32)frame_data_size, fp);
        fd2_ani_decoder_decode_frame_bytes(frame_decoded_size, (uint32)backbuf);
        if (anim_idx == 1 && frame_iter == 0) {
            fd2_play_sfx_with_handle(sfx_buf, 0, 1);
        }
        __delay_thunk_375b2(per_frame_delay);
        if (skip_on_key_flag != 0) {
            if (fd2_check_keyboard_buffer_nonempty()) break;
        }
        fd2_clear_keyboard_buffer();
    }

    free(scratch);
    free(backbuf);
    if (sfx_buf != 0) {
        fd2_play_sfx_with_handle(sfx_buf, -1, 1);
        free((void *)sfx_buf);
    }
    fclose(fp);
}
