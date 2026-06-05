/*
 * anicine.c — FD2 cinematic full-screen image display (chapter-ending flow).
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
#include <stdlib.h>

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

/* ----------------------------------------------------------------
 * fd2_play_figani_char_intro_animation @ 0x28784  (1 caller)
 *
 * Plays the FIGANI character intro animation (full-screen pose with
 * name banner) for the runtime char given by char_idx. Used at chapter
 * intros / character introductions.
 *
 * Setup: free large_game_state_buffer + battle_scene_snapshot, allocate
 * a 64000-byte mode-13h framebuffer scratch (dst) and a 0x1F400 work
 * buffer (ptr), memset dst, read the tile attribute under the char's
 * grid position, then load four resources via fd2_load_dat_resource:
 *   TAI.DAT[3]                       -> ptr_00   (name-banner sprite)
 *   FIGANI.DAT[portrait_id*3]        -> ptr_01   (silhouette sprite)
 *   FIGANI.DAT[portrait_id*3 + 1]    -> figani_buf (animation stream)
 *   BG.DAT[tile_attr_byte]           -> spotlight bg buf (terrain backdrop)
 * and the FIGANI SFX bank.
 *
 * Reveal: fade to black, flash the silhouette on the backdrop, RLE-blit
 * the terrain, then zoom the pose in via fd2_play_char_intro_zoom_anim.
 *
 * Per-frame loop (figani_buf[0] frames): for each frame, read the
 * per-frame metadata entry at figani_buf + *(int*)(figani_buf+8+i*4);
 * if metadata[+5] != 0 trigger its SFX, restore the backdrop into the
 * work buffer, composite the frame, push the work buffer to VGA, then
 * hold for metadata[+6] BIOS ticks.
 *
 * Cleanup: free all scratch buffers + the loaded resources, reallocate
 * large_game_state_buffer (0x25680) and reload battle_scene_snapshot
 * from FDSHAP.DAT, settle 6 ticks, fade to black, clear VGA, recomposite
 * the battle frame, stop all FIGANI SFX, free the SFX bank, fade in.
 *
 * Sole caller: fd2_execute_ai_item_use @ 0x15055.
 * ---------------------------------------------------------------- */
void fd2_play_figani_char_intro_animation(uint32 char_idx)
{
    void *dst;
    void *ptr;
    uint32 ptr_00;
    uint32 ptr_01;
    uint32 figani_buf;
    runtime_char *rt_char;
    uint8 portrait_id;
    uint8 tile_attr[8];     /* fd2_read_tile_attribute_at_pos fills +0..+7;
                             * +6 (attr-flags byte 2) is the BG.DAT index */
    uint32 frame_iter;
    uint32 frame_entry;

    data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr = 0;
    rt_char = &data_fd2_battle_runtime_char_array_ptr[char_idx];
    portrait_id = rt_char->portrait_id;
    free((void *)data_fd2_large_game_state_buffer_ptr);
    free((void *)battle_scene_snapshot);
    battle_scene_snapshot = 0;
    dst = malloc(64000);
    ptr = malloc(0x1F400);
    memset(dst, 0, 64000);
    fd2_read_tile_attribute_at_pos(rt_char->pos_x, rt_char->pos_y,
                                   (uint32)tile_attr);
    ptr_00 = fd2_load_dat_resource(
                 (uint32)data_fd2_string_resource_filename_tai_dat, 0, 3);
    ptr_01 = fd2_load_dat_resource(
                 (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
                 (uint32)portrait_id * 3);
    figani_buf = fd2_load_dat_resource(
                 (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
                 (uint32)portrait_id * 3 + 1);
    fd2_play_palette_fade_to_black();
    data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_bg_dat_52381,
            data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr,
            tile_attr[6]);
    fd2_flash_char_hit_sprite((uint32)dst, char_idx);
    fd2_rle_blit_sprite(data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr,
                        0, 0x32, (uint32)dst, 0x140, 0xFFFFFFFF);
    data_fd2_audio_figani_sfx_bank_buf_ptr = fd2_load_figani_sfx_bank(figani_buf);
    fd2_play_char_intro_zoom_anim(char_idx, 1, ptr_01, ptr_01, (uint32)ptr,
                                  (uint32)dst, ptr_00);
    for (frame_iter = 0;
         (int32)frame_iter < (int32)(uint32) * (uint8 *)figani_buf;
         frame_iter = frame_iter + 1) {
        frame_entry =
            *(uint32 *)(figani_buf + 8 + frame_iter * 4) + figani_buf;
        if (*(uint8 *)(frame_entry + 5) != 0) {
            fd2_play_sfx_with_handle(data_fd2_audio_figani_sfx_bank_buf_ptr,
                                     *(uint8 *)(frame_entry + 5), 1);
        }
        fd2_blit_rectangle((uint32)ptr, 0x140, (uint32)dst, 0x140, 0x140, 0xC8);
        fd2_blit_indexed_sprite(figani_buf, frame_iter, (uint32)ptr, 0x140, -1);
        fd2_blit_rectangle(0xA0000, 0x140, (uint32)ptr, 0x140, 0x140, 0xC8);
        fd2_wait_n_bios_ticks(*(uint8 *)(frame_entry + 6));
    }
    free(dst);
    free(ptr);
    free((void *)ptr_01);
    free((void *)figani_buf);
    free((void *)data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr);
    free((void *)ptr_00);
    data_fd2_large_game_state_buffer_ptr = (uint32)malloc(0x25680);
    battle_scene_snapshot =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
            battle_scene_snapshot,
            (uint32) * (uint8 *)data_fd2_tile_event_data_table_ptr * 2);
    fd2_wait_n_bios_ticks(6);
    fd2_play_palette_fade_to_black();
    memset((void *)0xA0000, 0, 64000);
    fd2_composite_battle_frame(1);
    fd2_play_sfx_with_handle(data_fd2_audio_figani_sfx_bank_buf_ptr, -1, 1);
    if (data_fd2_audio_figani_sfx_bank_buf_ptr != 0) {
        free((void *)data_fd2_audio_figani_sfx_bank_buf_ptr);
    }
    fd2_play_palette_fade_in();
}
