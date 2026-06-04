/*
 * anicine.c — FD2 cinematic full-screen image display (chapter-ending flow).
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_display_cinematic_image_with_fade @ 0x1F73F  (1 caller)
 *
 * Two-stage cinematic image display with palette transitions.
 *
 * Stage 1 — full-screen image:
 *   fade current screen to black, clear the 64000-byte framebuffer at
 *   0xA0000, load FDOTHER.DAT[palette_idx] palette into
 *   data_fd2_vga_palette_data_ptr, load FDOTHER.DAT[image1_idx] sprite,
 *   RLE-blit it full-screen (320 stride) to 0xA0000, fade in to reveal
 *   the blit, hold for 1 + 6 BIOS ticks (~385ms), then fade to black.
 *
 * Stage 2 — flash card:
 *   load cinematic 0x65 (final-clear notice) palette, blit a 320x200
 *   region from offset (row_idx*320 + src_x_off) within the loaded
 *   source buffer to 0xA0000, then fade in to reveal it.
 *
 * Params: image1_idx = stage-1 image idx (FDOTHER.DAT entry),
 *   palette_idx = stage-1 palette idx, src_x_off = stage-2 source x
 *   offset, row_idx = stage-2 source y offset (row, multiplied by the
 *   320 stride).
 *
 * Sole caller: fd2_play_ending_and_record_clear @ 0x1FBAF.
 * ---------------------------------------------------------------- */
void fd2_display_cinematic_image_with_fade(uint32 image1_idx, uint32 palette_idx,
                                           uint32 src_x_off, int32 row_idx)
{
    uint32 image_data;
    uint32 stage2_src;

    fd2_play_palette_fade_to_black();
    memset((void *)0xA0000, 0, 64000);
    data_fd2_vga_palette_data_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_vga_palette_data_ptr, palette_idx);
    image_data =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            0, image1_idx);
    fd2_rle_blit_sprite(image_data, 0, 0, 0xA0000, 0x140, 0xFFFFFFFF);
    fd2_play_palette_fade_in();
    fd2_wait_n_bios_ticks(1);
    fd2_wait_n_bios_ticks(6);
    fd2_play_palette_fade_to_black();
    data_fd2_vga_palette_data_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat,
            data_fd2_vga_palette_data_ptr, 0x65);
    stage2_src = (uint32)row_idx * 0x140 + src_x_off;
    fd2_blit_rectangle(0xA0000, 0x140, stage2_src, 0x140, 0x140, 0xC8);
    fd2_play_palette_fade_in();
}
