/*
 * anispell.c — full-screen ANI.DAT cinematic / slideshow playback and
 * combat-cinematic background zoom transitions.
 *
 * fd2_play_ani_file_animation_sequence    @ 0x20421 (4 callers)
 * fd2_animate_bg_zoom_transition_in       @ 0x29C90 (2 callers)
 * fd2_animate_bg_zoom_transition_out      @ 0x29DED (1 caller)
 * fd2_play_spell_cast_cinematic           @ 0x2A2E8 (1 caller)
 * fd2_cycle_sprite_anim_with_bg_frames    @ 0x2A5D0 (1 caller)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
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

/* ----------------------------------------------------------------
 * fd2_animate_bg_zoom_transition_in @ 0x29C90  (2 callers)
 *
 * BG zoom-in scroll transition for a combat cinematic — the
 * "approaching attacker" visual. Iterates 10 frames down then 10 frames
 * up through the 3-layer parallax BG cache
 * (data_fd2_battle_special_cinematic_bg_layer_0/1/2 @ 0x5410B/0F/13)
 * loaded by the caller, painting the attacker silhouette into the work
 * buffer between the two scroll passes.
 *
 * Params (__cdecl):
 *   char_unit_id    runtime-char index of the attacker (forwarded to the
 *                   hit-flash overlay)
 *   char_sprite_idx atlas/sprite index blitted into the workspace after
 *                   the silhouette is composed
 *   clear_buf       64000-byte (mode-13h sized) scratch the silhouette is
 *                   rendered into before being blitted into workspace
 *   workspace       128K (0x1F400) work buffer holding the scrolled BG
 *   caster_figani   FIGANI sprite stream for the caster silhouette
 *
 * The three BG-layer pointers sit contiguously at 0x5410B/0F/13 and the
 * original indexes them as a uint32[3]; reproduced here by indexing
 * through the address of the first slot. See emit_issues.json.
 * ---------------------------------------------------------------- */
void fd2_animate_bg_zoom_transition_in(uint32 char_unit_id,
                                       uint32 char_sprite_idx,
                                       uint32 clear_buf,
                                       uint32 workspace,
                                       uint32 caster_figani)
{
    uint32 *bg_layer = &data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr;
    int frame_iter;

    /* Phase 1 — descending scroll (frame_iter = 9..0) */
    for (frame_iter = 9; frame_iter >= 0; frame_iter--) {
        fd2_rle_blit_sprite(bg_layer[frame_iter % 3], 0, 0x32,
                            workspace, 0x280, 0xffffffff);
        fd2_blit_rectangle(0xa0000, 0x140, frame_iter * 0x20 + workspace,
                           0x280, 0x140, 0xc8);
    }

    /* Phase 2 — reset buffers + paint attacker silhouette into workspace */
    memset((void *)workspace, 0, 0x1f400);
    memset((void *)clear_buf, 0, 64000);
    fd2_rle_blit_sprite(caster_figani, 0, 0x32, clear_buf, 0x140, 0xffffffff);
    fd2_flash_char_hit_sprite(clear_buf, char_unit_id);
    fd2_blit_rectangle(workspace, 0x280, clear_buf, 0x140, 0x140, 0xc8);
    fd2_blit_indexed_sprite(char_sprite_idx, 0, (int)workspace, 0x280, -1);

    /* Phase 3 — ascending scroll with rotated BG cycling (frame_iter = 9..0) */
    for (frame_iter = 9; frame_iter >= 0; frame_iter--) {
        fd2_rle_blit_sprite(bg_layer[(frame_iter + 2) % 3], 0, 0x32,
                            workspace + 0x140, 0x280, 0xffffffff);
        fd2_blit_rectangle(0xa0000, 0x140, frame_iter * 0x20 + workspace,
                           0x280, 0x140, 0xc8);
    }
}

/* ----------------------------------------------------------------
 * fd2_animate_bg_zoom_transition_out @ 0x29DED  (1 caller)
 *
 * BG zoom-out scroll transition for a combat cinematic — the camera
 * pulls back from the attacker (player/ally side), the mirror of
 * fd2_animate_bg_zoom_transition_in. Runs an outward scroll, repaints the
 * defender into the work buffer, then a second outward scroll with a
 * rotated BG cycle, all over the 3-layer parallax BG cache
 * (data_fd2_battle_special_cinematic_bg_layer_0/1/2 @ 0x5410B/0F/13)
 * loaded by the caller.
 *
 * Called by fd2_execute_combat_hit_cinematic.
 *
 * Params (__cdecl):
 *   char_unit_id        runtime-char index of the unit (forwarded to the
 *                       hit-flash overlay)
 *   char_sprite_idx     atlas/sprite index blitted into the workspace after
 *                       the defender is composed
 *   terrain_bg          terrain backdrop RLE sprite stream
 *   clear_buf           64000-byte (mode-13h sized) scratch the defender is
 *                       composed into before being blitted into workspace
 *   workspace           128K (0x1F400) work buffer holding the scrolled BG;
 *                       blits target workspace + 0x140 in the lower row band
 *   name_banner_sprite  RLE sprite stream for the unit name banner
 *
 * The three BG-layer pointers sit contiguously at 0x5410B/0F/13 and the
 * original indexes them as a uint32[3]; reproduced here by indexing
 * through the address of the first slot. See emit_issues.json.
 *
 * Unlike the zoom-in counterpart, both scroll passes here count forward
 * (1..9 and 1..10) and the second pass blits into bare workspace while the
 * first pass and the defender repaint use workspace + 0x140.
 * ---------------------------------------------------------------- */
void fd2_animate_bg_zoom_transition_out(uint32 char_unit_id,
                                        uint32 char_sprite_idx,
                                        uint32 terrain_bg,
                                        uint32 clear_buf,
                                        uint32 workspace,
                                        uint32 name_banner_sprite)
{
    uint32 *bg_layer = &data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr;
    uint32 lower_band = workspace + 0x140;
    int frame_iter;

    /* Phase 1 — outward scroll, BG cycle (frame_iter = 1..9) */
    for (frame_iter = 1; frame_iter < 10; frame_iter++) {
        fd2_rle_blit_sprite(bg_layer[frame_iter % 3], 0, 0x32,
                            lower_band, 0x280, 0xffffffff);
        fd2_blit_rectangle(0xa0000, 0x140, frame_iter * 0x20 + workspace,
                           0x280, 0x140, 0xc8);
    }

    /* Phase 2 — reset buffers + paint banner, terrain and defender */
    memset((void *)workspace, 0, 0x1f400);
    memset((void *)clear_buf, 0, 64000);
    fd2_rle_blit_sprite(name_banner_sprite, 0, 0x32, clear_buf, 0x140, 0xffffffff);
    fd2_rle_blit_sprite(terrain_bg, 0xa4, 0x9d, clear_buf, 0x140, 0xffffffff);
    fd2_flash_char_hit_sprite(clear_buf, char_unit_id);
    fd2_blit_rectangle(lower_band, 0x280, clear_buf, 0x140, 0x140, 0xc8);
    fd2_blit_indexed_sprite(char_sprite_idx, 0, (int)lower_band, 0x280, -1);

    /* Phase 3 — outward scroll with rotated BG cycling (frame_iter = 1..10) */
    for (frame_iter = 1; frame_iter <= 10; frame_iter++) {
        fd2_rle_blit_sprite(bg_layer[(frame_iter + 1) % 3], 0, 0x32,
                            workspace, 0x280, 0xffffffff);
        fd2_blit_rectangle(0xa0000, 0x140, frame_iter * 0x20 + workspace,
                           0x280, 0x140, 0xc8);
    }
}

/* ----------------------------------------------------------------
 * fd2_play_spell_cast_cinematic @ 0x2A2E8  (1 caller)
 *
 * The CLASS PROMOTION cinematic. Sole caller: fd2_run_class_promotion_menu_main
 * @ 0x31385, which invokes it as fd2_play_spell_cast_cinematic(char_idx,
 * class_id). The "spell_cast" / "spell_id" naming is a misnomer kept stable
 * across xrefs; the second arg is the target class id, used purely to index
 * FIGANI.DAT for a "becomes-this-class" silhouette — the fn does not care about
 * spell semantics.
 *
 * Loads three parallax BGs from BG.DAT[0..2] into the 3-layer cache
 * (data_fd2_battle_special_cinematic_bg_layer_0/1/2 @ 0x5410B/0F/13), loads the
 * caster silhouette FIGANI.DAT[caster.portrait_id*3] and the target-class
 * silhouette FIGANI.DAT[class_id*3], backs up the current VGA frame, then plays:
 *   Phase 1  fade out + 9-frame (iter 8..0) zoom-in slide of the caster figure,
 *            brightness ramping from 48 down to 0 (set_vga_palette_range iter*6);
 *   Phase 2  fd2_cycle_sprite_anim_with_bg_frames(caster, work, 0x10) — 16 anim
 *            frames over the cycling parallax BG;
 *   Phase 3  20-step additive palette flash (the "shine" burst), 10ms/step;
 *   Phase 4  swap to the target-class silhouette at the slide-in cap (0x14*10),
 *            restore full brightness, then anim it (0x18 = 24 frames);
 *   Phase 5  fade to black, restore the pre-cinematic VGA frame, fade in, free.
 *
 * The three BG-layer pointers sit contiguously at 0x5410B/0F/13; the original
 * indexes them as a uint32[3], reproduced here by indexing through the address
 * of the first slot (same idiom as the bg_zoom_transition siblings above).
 *
 * Phase-1 BG cycler note: in the disassembly the per-frame cycler
 * (bg_idx = (bg_idx+1) % 3) is reached via a JZ that tests the flags left by the
 * preceding `ADD ESP,0xC` (stack cleanup), which is never zero — so the branch is
 * never taken and the cycler advances every frame, exactly like the unconditional
 * cycler in fd2_cycle_sprite_anim_with_bg_frames. The dead `MOV EAX,0x46C; MOVSX`
 * BIOS-tick read that precedes the JZ has no effect (result discarded); it is not
 * reproduced. See src/emit_issues.json (0002a2e8).
 *
 * caster_char_idx indexes data_fd2_battle_runtime_char_array_ptr (stride 0x50);
 * .portrait_id is at +0x07. cdecl, void return.
 * ---------------------------------------------------------------- */
void fd2_play_spell_cast_cinematic(uint32 caster_char_idx, uint32 spell_id)
{
    uint32 *bg_layer = &data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr;
    void   *work;
    void   *caster_figani;
    void   *target_figani;
    void   *vga_backup;
    int     bg_idx;
    int     zoom_iter;
    int     flash_iter;

    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = 0;

    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381, 0, 0);
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381,
        data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr, 1);
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381,
        data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr, 2);

    work = malloc(0x1f400);
    caster_figani = (void *)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
        (uint32)data_fd2_battle_runtime_char_array_ptr[caster_char_idx].portrait_id * 3);
    target_figani = (void *)fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
        spell_id * 3);
    vga_backup = malloc(64000);
    memmove(vga_backup, (void *)0xa0000, 64000);

    fd2_play_palette_fade_to_black();
    fd2_wait_n_bios_ticks(1);

    /* Phase 1 — fade out, 9-frame zoom-in slide of the caster figure. */
    bg_idx = 0;
    for (zoom_iter = 8; zoom_iter >= 0; zoom_iter--) {
        memset(work, 0, 0x1f400);
        bg_idx = (bg_idx + 1) % 3;
        fd2_rle_blit_sprite(bg_layer[bg_idx], 0, 0x32, (uint32)work, 0x280,
                            0xffffffff);
        fd2_blit_indexed_sprite((uint32)caster_figani, 0,
                                zoom_iter * 10 + (int)work, 0x280, -1);
        fd2_blit_rectangle(0xa0000, 0x140, (uint32)work, 0x280, 0x140, 0xc8);
        fd2_set_vga_palette_range(0, 0xff, zoom_iter * 6);
    }

    /* Phase 2 — caster animation over cycling parallax BG (16 frames). */
    fd2_cycle_sprite_anim_with_bg_frames((uint32)caster_figani, (uint32)work, 0x10);

    /* Phase 3 — additive palette flash burst (20 steps, 10ms each). */
    for (flash_iter = 0; flash_iter < 0x14; flash_iter++) {
        fd2_set_vga_palette_range_with_add(0, 0xff, flash_iter * 3);
        __delay_thunk_375b2(10);
    }

    /* Phase 4 — swap to the target-class silhouette at the slide-in cap. */
    memset(work, 0, 0x1f400);
    fd2_rle_blit_sprite(data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr, 0,
                        0x32, (uint32)work, 0x280, 0xffffffff);
    fd2_blit_indexed_sprite((uint32)target_figani, 0,
                            flash_iter * 10 + (int)work, 0x280, -1);
    fd2_blit_rectangle(0xa0000, 0x140, (uint32)work, 0x280, 0x140, 0xc8);
    fd2_set_vga_palette_range(0, 0xff, 0);
    fd2_cycle_sprite_anim_with_bg_frames((uint32)target_figani, (uint32)work, 0x18);

    /* Phase 5 — fade to black, restore the pre-cinematic frame, fade in, free. */
    fd2_play_palette_fade_to_black();
    memmove((void *)0xa0000, vga_backup, 64000);
    fd2_play_palette_fade_in();

    free(vga_backup);
    free((void *)data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr);
    free((void *)data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr);
    free((void *)data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr);
    free(caster_figani);
    free(target_figani);
    free(work);
}

/* ----------------------------------------------------------------
 * fd2_cycle_sprite_anim_with_bg_frames @ 0x2A5D0  (1 caller)
 *
 * Generic sprite-animation player loop with a 3-variant cycling parallax
 * background. Sole caller: fd2_play_spell_cast_cinematic, which invokes it for
 * both the caster phase (iter_count = 0x10) and the target-class phase
 * (iter_count = 0x18). Renders iter_count frames at 1 BIOS tick per frame.
 *
 * Params (__cdecl):
 *   sprite_atlas   indexed-sprite atlas (FIGANI stream) blitted each frame
 *   workspace      128K (0x1F400) work buffer; cleared and recomposited per frame
 *   iter_count     number of frames to render
 *
 * Per iteration:
 *   - memset(workspace, 0, 0x1F400)
 *   - bg_variant_idx = (bg_variant_idx + 1) % 3       (advances every frame)
 *   - rle-blit the cycling BG layer at y=0x32 into workspace
 *   - blit the current atlas frame into workspace
 *   - push workspace to the VGA aperture (0xA0000)
 *   - per-frame hold: hold_count = atlas[6 + atlas[8 + frame_idx*4]]; tick++;
 *     when tick == hold_count, reset tick to 0 and advance frame_idx, wrapping
 *     to 0 when it reaches frame_count (= atlas[0]).
 *   - wait 1 BIOS tick
 *
 * The tick / frame_idx resets use the original's XOR-with-self idiom
 * (tick ^= hold_count when equal -> 0; frame_idx ^= frame_count at wrap -> 0);
 * preserved verbatim. The three BG-layer pointers sit contiguously at
 * 0x5410B/0F/13 and the original indexes them as a uint32[3]; reproduced here by
 * indexing through the address of the first slot (same idiom as the
 * bg_zoom_transition / spell_cast_cinematic siblings above).
 * ---------------------------------------------------------------- */
void fd2_cycle_sprite_anim_with_bg_frames(uint32 sprite_atlas, uint32 workspace,
                                          uint32 iter_count)
{
    uint32 *bg_layer = &data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr;
    uint32  frame_idx;
    uint32  bg_variant_idx;
    uint32  tick;
    uint32  iter;
    uint32  hold_count;

    frame_idx = 0;
    bg_variant_idx = 0;
    tick = 0;
    for (iter = 0; (int)iter < (int)iter_count; iter++) {
        memset((void *)workspace, 0, 0x1f400);
        bg_variant_idx = (int)(bg_variant_idx + 1) % 3;
        fd2_rle_blit_sprite(bg_layer[bg_variant_idx], 0, 0x32, workspace, 0x280,
                            0xffffffff);
        fd2_blit_indexed_sprite(sprite_atlas, frame_idx, (int)workspace, 0x280, -1);
        fd2_blit_rectangle(0xa0000, 0x140, workspace, 0x280, 0x140, 0xc8);
        hold_count = (uint32)*(uint8 *)
            (*(int *)(sprite_atlas + 8 + frame_idx * 4) + 6 + sprite_atlas);
        tick = tick + 1;
        if (tick == hold_count) {
            tick = tick ^ hold_count;
            frame_idx = frame_idx + 1;
            if (frame_idx == *(uint8 *)sprite_atlas) {
                frame_idx = frame_idx ^ *(uint8 *)sprite_atlas;
            }
        }
        fd2_wait_n_bios_ticks(1);
    }
}
