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
        fd2_delay_ms(per_frame_delay);
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
 * fd2_animate_bg_zoom_transition_in @ 0x29C90  (2 callers, 3 call sites)
 *
 * BG zoom-in scroll transition for a combat cinematic -- the camera pushes
 * in toward the unit. Iterates 10 frames down then 10 frames up through the
 * 3-layer parallax BG cache
 * (data_fd2_battle_special_cinematic_bg_layers @ 0x5410B/0F/13)
 * loaded by the caller, compositing the relevant combat unit (the attacker
 * or the defender, depending on phase) into the work buffer between the two
 * scroll passes.
 *
 * Callers: fd2_execute_combat_hit_cinematic (charge-in and second-strike
 * branches, on the player/ally-attacker side) and
 * fd2_execute_special_attack_skill (per-target loop, non-0x1C path).
 *
 * Params (__cdecl):
 *   char_unit_id    runtime-char index of the unit (forwarded to the
 *                   hit-flash overlay)
 *   char_sprite_idx FIGANI/sprite-sheet stream pointer for the unit pose,
 *                   blitted (frame 0) into the workspace. NOTE: misnomer --
 *                   this is a sheet pointer, not an index (it is passed as
 *                   fd2_blit_indexed_sprite's sheet_ptr with idx 0).
 *   clear_buf       64000-byte (mode-13h sized) scratch the unit pose is
 *                   composed into before being blitted into workspace
 *   workspace       128K (0x1F400) work buffer holding the scrolled BG
 *   caster_figani   terrain/BG backdrop RLE stream rendered into clear_buf
 *                   under the unit. NOTE: misnomer -- this is the terrain
 *                   backdrop (cf. the terrain_bg param of the zoom-out
 *                   counterpart), not a caster FIGANI.
 *
 * The three BG-layer pointers are a real uint32[3] array
 * (data_fd2_battle_special_cinematic_bg_layers), indexed directly so C
 * guarantees the ascending adjacency the frame cycling relies on.
 * ---------------------------------------------------------------- */
void fd2_animate_bg_zoom_transition_in(uint32 char_unit_id,
                                       uint32 char_sprite_idx,
                                       uint32 clear_buf,
                                       uint32 workspace,
                                       uint32 caster_figani)
{
    int frame_iter;

    /* Phase 1 — descending scroll (frame_iter = 9..0) */
    for (frame_iter = 9; frame_iter >= 0; frame_iter--) {
        fd2_rle_blit_sprite(data_fd2_battle_special_cinematic_bg_layers[frame_iter % 3], 0, 0x32,
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
        fd2_rle_blit_sprite(data_fd2_battle_special_cinematic_bg_layers[(frame_iter + 2) % 3], 0, 0x32,
                            workspace + 0x140, 0x280, 0xffffffff);
        fd2_blit_rectangle(0xa0000, 0x140, frame_iter * 0x20 + workspace,
                           0x280, 0x140, 0xc8);
    }
}

/* ----------------------------------------------------------------
 * fd2_animate_bg_zoom_transition_out @ 0x29DED  (1 caller, 2 call sites)
 *
 * BG zoom-out scroll transition for a combat cinematic -- the camera
 * pulls back from the unit, the mirror of fd2_animate_bg_zoom_transition_in.
 * Runs an outward scroll, repaints the unit (banner + terrain backdrop +
 * pose) into the lower row band of the work buffer, then a second outward
 * scroll with a rotated BG cycle, all over the 3-layer parallax BG cache
 * (data_fd2_battle_special_cinematic_bg_layers @ 0x5410B/0F/13) loaded by
 * the caller.
 *
 * Called by fd2_execute_combat_hit_cinematic (enemy-team-attacker branches:
 * the charge-in section and the second-strike loop tail; the player/ally
 * branch at each site uses fd2_animate_bg_zoom_transition_in instead).
 *
 * Params (__cdecl). Three current names are misnomers; the true semantics
 * (proven via the caller chain fd2_play_full_combat_cinematic and the blit
 * coordinates) are noted, and corrected names are slated for Stage 2 rename:
 *   char_unit_id        runtime-char index of the unit (forwarded to the
 *                       hit-flash overlay)
 *   char_sprite_idx     FIGANI/sprite-sheet stream POINTER for the unit pose,
 *                       blitted (frame 0) into the lower band. NOTE: misnomer
 *                       -- a sheet pointer, not an index (passed as
 *                       fd2_blit_indexed_sprite's sheet_ptr with idx 0).
 *   terrain_bg          NAME-BANNER RLE sprite, painted at (0xA4,0x9D) into
 *                       clear_buf. NOTE: misnomer -- this is the TAI.DAT name
 *                       banner the caller loads, NOT the terrain backdrop
 *                       (see name_banner_sprite below; the two are swapped).
 *   clear_buf           64000-byte (mode-13h sized) scratch the banner,
 *                       backdrop and unit are composed into before being
 *                       blitted into the workspace lower band
 *   workspace           128K (0x1F400) work buffer holding the scrolled BG;
 *                       the unit repaint targets workspace + 0x140 (lower
 *                       row band)
 *   name_banner_sprite  TERRAIN/BG backdrop RLE sprite, painted at (0,0x32)
 *                       into clear_buf. NOTE: misnomer -- this is the BG.DAT
 *                       spotlight terrain backdrop the caller loads, NOT the
 *                       name banner (cf. terrain_bg above; the two are
 *                       swapped). Mirrors the caster_figani backdrop of the
 *                       zoom-in counterpart.
 *
 * The three BG-layer pointers are a real uint32[3] array
 * (data_fd2_battle_special_cinematic_bg_layers), indexed directly so C
 * guarantees the ascending adjacency the frame cycling relies on.
 *
 * Unlike the zoom-in counterpart, both scroll passes here count forward
 * (1..9 and 1..10) and the second pass blits into bare workspace while the
 * first pass and the unit repaint use workspace + 0x140.
 * ---------------------------------------------------------------- */
void fd2_animate_bg_zoom_transition_out(uint32 char_unit_id,
                                        uint32 char_sprite_idx,
                                        uint32 terrain_bg,
                                        uint32 clear_buf,
                                        uint32 workspace,
                                        uint32 name_banner_sprite)
{
    uint32 lower_band = workspace + 0x140;
    int frame_iter;

    /* Phase 1 — outward scroll, BG cycle (frame_iter = 1..9) */
    for (frame_iter = 1; frame_iter < 10; frame_iter++) {
        fd2_rle_blit_sprite(data_fd2_battle_special_cinematic_bg_layers[frame_iter % 3], 0, 0x32,
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
        fd2_rle_blit_sprite(data_fd2_battle_special_cinematic_bg_layers[(frame_iter + 1) % 3], 0, 0x32,
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
 * (data_fd2_battle_special_cinematic_bg_layers @ 0x5410B/0F/13), loads the
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
 * The three BG-layer pointers are a real uint32[3] array
 * (data_fd2_battle_special_cinematic_bg_layers), indexed directly so C
 * guarantees the ascending adjacency the BG cycling relies on.
 *
 * Phase-1 BG cycler note: in the disassembly the per-frame cycler
 * (bg_idx = (bg_idx+1) % 3) is reached via a JZ that tests the flags left by the
 * preceding `ADD ESP,0xC` (stack cleanup), which is never zero — so the branch is
 * never taken and the cycler advances every frame, exactly like the unconditional
 * cycler in fd2_cycle_sprite_anim_with_bg_frames. The dead `MOV EAX,0x46C; MOVSX`
 * BIOS-tick read that precedes the JZ has no effect (result discarded); it is not
 * reproduced. See tools/code_emit/data/emit_issues.json (0002a2e8).
 *
 * caster_char_idx indexes data_fd2_battle_runtime_char_array_ptr (stride 0x50);
 * .portrait_id is at +0x07. cdecl, void return.
 * ---------------------------------------------------------------- */
void fd2_play_spell_cast_cinematic(uint32 caster_char_idx, uint32 spell_id)
{
    void   *work;
    void   *caster_figani;
    void   *target_figani;
    void   *vga_backup;
    int     bg_idx;
    int     zoom_iter;
    int     flash_iter;

    data_fd2_battle_special_cinematic_bg_layers[0] = 0;
    data_fd2_battle_special_cinematic_bg_layers[1] = 0;
    data_fd2_battle_special_cinematic_bg_layers[2] = 0;

    data_fd2_battle_special_cinematic_bg_layers[0] = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381, 0, 0);
    data_fd2_battle_special_cinematic_bg_layers[1] = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381,
        data_fd2_battle_special_cinematic_bg_layers[1], 1);
    data_fd2_battle_special_cinematic_bg_layers[2] = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381,
        data_fd2_battle_special_cinematic_bg_layers[2], 2);

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
        fd2_rle_blit_sprite(data_fd2_battle_special_cinematic_bg_layers[bg_idx], 0, 0x32, (uint32)work, 0x280,
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
        fd2_delay_ms(10);
    }

    /* Phase 4 — swap to the target-class silhouette at the slide-in cap. */
    memset(work, 0, 0x1f400);
    fd2_rle_blit_sprite(data_fd2_battle_special_cinematic_bg_layers[0], 0,
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
    free((void *)data_fd2_battle_special_cinematic_bg_layers[0]);
    free((void *)data_fd2_battle_special_cinematic_bg_layers[1]);
    free((void *)data_fd2_battle_special_cinematic_bg_layers[2]);
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
 * preserved verbatim. The three BG-layer pointers are a real uint32[3] array
 * (data_fd2_battle_special_cinematic_bg_layers), indexed directly so C
 * guarantees the ascending adjacency the per-frame cycling relies on.
 * ---------------------------------------------------------------- */
void fd2_cycle_sprite_anim_with_bg_frames(uint32 sprite_atlas, uint32 workspace,
                                          uint32 iter_count)
{
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
        fd2_rle_blit_sprite(data_fd2_battle_special_cinematic_bg_layers[bg_variant_idx], 0, 0x32, workspace, 0x280,
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

/* ----------------------------------------------------------------
 * fd2_play_spell_cast_sequence @ 0x2A6BD  (2 callers)
 *
 * The master "big spell cast" animation + damage-application orchestrator.
 * Callers: fd2_execute_ai_offensive_spell @ 0x15311 (enemy AI offensive
 * spell) and fd2_spell_selection_menu_main @ 0x1CFF0 (player menu cast).
 *
 * Top-level dispatch on spell_id:
 *   spell_id >= 0x20                  -> fd2_execute_summon_spell_cast
 *   spell_id == 0x18 or in 0x1C..0x1F -> fd2_execute_special_attack_skill
 *   otherwise (0x00..0x17,0x19..0x1B) -> inline main sequence below.
 *
 * The inline sequence frees the scratch caches, loads the battle backdrop
 * (BG.DAT / TAI.DAT), the caster + per-target FIGANI streams and the
 * spell-specific FDOTHER sprite, allocates a 64000-byte backbuffer and a
 * 0x2A300 frame-scratch, then runs the cast animation in phases driven by a
 * per-spell function-pointer "cinematic phase handler" table @ 0x523B9. Each
 * handler call returns the frame count for that phase and is invoked with
 * (caster_idx, team_caster_sprite, work_buffer, stride, phase_code). The
 * HP bar of each target is lerped from its pre-cast value down to the value
 * computed by fd2_calc_magic_damage (which itself applies the damage); the
 * original HP is restored before the lerp and the final HP is written by
 * calc_magic_damage. Finally all resources are freed and the battle scene
 * caches are reloaded.
 *
 * Notes on the emit:
 *  - The dispatch table @ 0x523B9 is called via an FF /4 indirect jump
 *    pushing 5 args + ADD ESP,0x14 with the result in EAX (a frame count);
 *    Ghidra's decompiler masks the args (shows "()") because it lacks the
 *    fn-ptr signature. The 5 args are reconstructed from the disassembly:
 *    phase_code is the per-call-site immediate (0..8), the work-buffer
 *    pointer matches the buffer used by the adjacent blit, and stride is
 *    0x140 for the full-frame phases / 0x280 for the wide composite phases.
 *  - The 6 small const tables (shake X/Y offsets, HP-lerp hit counts, the
 *    player/enemy caster sprite-id tables and the intro SFX-bank table) are
 *    function-local const arrays in the original (the disassembly copies
 *    each from .rodata onto the stack with REP MOVSD before use). They are
 *    indexed by spell_id; for spell_id >= the table length this is a latent
 *    out-of-bounds read in the original that reads adjacent stack locals
 *    (it only feeds the cosmetic HP-bar lerp speed / sprite selection — the
 *    applied damage is unaffected). Reproduced verbatim. See emit_issues.json.
 *  - target FIGANI streams are held in a 30-dword stack buffer; index [0] is
 *    the decompiler's "pTarget_first_figani", indices [0..target_count-1] are
 *    the per-target streams (the original's off-by-one pointer arithmetic
 *    target_figani_arr + i*4 - 4 resolves to figani_buf[i]).
 *  - calc_magic_damage returns the damage amount; 0 means a miss (no HP
 *    change, "miss" frame shown). EAX after that CALL is the damage value
 *    (verified against the disassembly SETZ -> is_miss byte).
 *
 * cdecl, void return.
 * ---------------------------------------------------------------- */
void fd2_play_spell_cast_sequence(uint32 caster_idx, uint32 spell_id,
                                  uint32 target_count, uint32 target_ids_arg)
{
    /* function-local const tables (the original copies each from .rodata onto
     * the stack with REP MOVSD before use; emitted as initialized const locals
     * so Watcom reproduces the same rodata->stack copy). */
    const int32 shake_x_offsets[4]      = { 6, 4, 2, 0 };
    const int32 shake_y_row_offsets[4]  = { -3, -2, -1, 0 };
    const uint8 hp_lerp_hits[10]        = { 7, 8, 6, 13, 6, 6, 5, 5, 16, 20 };
    const uint8 player_team_sprite_id[9]  = { 18, 19, 26, 39, 22, 24, 32, 37, 28 };
    const uint8 enemy_team_sprite_id[10]  = { 20, 21, 27, 43, 23, 25, 33, 38, 30, 44 };
    const uint8 intro_sfx_bank[10]      = { 82, 82, 83, 84, 85, 86, 87, 88, 89, 90 };

    uint8  *target_ids = (uint8 *)target_ids_arg;
    uint32  figani_buf[30];
    uint8   tile_attr_buf[8];

    runtime_char *caster;
    runtime_char *tgt;
    uint32  bg_idx;
    uint32  tai_idx;
    uint32  cinematic_mode;
    uint32  team_caster_sprite;
    uint32  bg_resource;
    uint32  tai_resource;
    uint32  caster_figani_a;
    uint32  caster_figani_b;
    void   *backbuf;
    void   *work;
    uint32  caster_anim_base;
    uint32  caster_last_frame;

    uint32  palette_op_base;   /* portrait_load_idx_x100, init 0x20 */
    uint32  shine_table_offset;     /* portrait_load_idx_offset, init 0x0B */
    uint32  shake_step_counter;     /* palette_y_loop_counter, init 8 */
    uint32  shake_idx;              /* palette_idx, init 3 */
    int32   shake_y_dir;            /* palette_y_dir_sign, init -1 */
    uint32  swap_toggle;            /* init 0 */

    int     i;
    int     n_frames;
    uint32  target_iter;
    int     anim_iter;
    uint32  hit_count;
    int     damage;
    int16   starting_hp;
    int16   final_hp;
    uint8   is_miss;
    uint32  swap_buf;
    uint32  ret_phase;
    uint32  flash_unit;

    shake_step_counter = 8;
    shake_idx = 3;
    shake_y_dir = -1;
    palette_op_base = 0x20;
    shine_table_offset = 0xb;
    swap_toggle = 0;

    if ((int)spell_id >= 0x20) {
        fd2_execute_summon_spell_cast(caster_idx, spell_id, target_count,
                                      (int)target_ids_arg);
        return;
    }

    if ((spell_id == 0x18) || (0x1b < (int)spell_id)) {
        fd2_execute_special_attack_skill(caster_idx, spell_id, (int)target_count,
                                         target_ids);
        return;
    }

    if (spell_id == 8) {
        palette_op_base = 0xb0;
        shine_table_offset = 0x13;
    } else if (3 < (int)spell_id) {
        palette_op_base = 0xb0;
        shine_table_offset = 0xf;
    }

    free((void *)data_fd2_portrait_sprite_cache);
    free((void *)data_fd2_large_game_state_buffer_ptr);
    free((void *)data_fd2_battle_scene_snapshot);
    data_fd2_battle_scene_snapshot = 0;

    for (i = 0; i < 0x1e; i++) {
        figani_buf[i] = 0;
    }

    caster = &data_fd2_battle_runtime_char_array_ptr[caster_idx];
    fd2_read_tile_attribute_at_pos(caster->pos_x, caster->pos_y,
                                   (uint32)tile_attr_buf);

    cinematic_mode = (uint32)data_fd2_chapter_combat_cinematic_mode_per_chapter
                         [data_fd2_chapter_current_chapter_id];
    if ((fd2_check_char_status_immunity(caster_idx) == 0) || (cinematic_mode == 0)) {
        cinematic_mode = (uint32)tile_attr_buf[6];
    }

    {
        uint32 resolved_terrain;
        resolved_terrain =
            (uint32)fd2_resolve_terrain_for_aoe_targets((int)target_count,
                                                        (uint8 *)target_ids_arg);
        tai_idx = cinematic_mode;
        bg_idx = resolved_terrain;
        if (caster->team == 0) {
            tai_idx = resolved_terrain;
            bg_idx = cinematic_mode;
        }
    }

    bg_resource = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_bg_dat_52381, 0, bg_idx);
    tai_resource = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_tai_dat, 0, tai_idx);

    backbuf = malloc(64000);
    work = malloc(0x2a300);
    memset(backbuf, 0, 64000);
    fd2_flash_char_hit_sprite((uint32)backbuf, caster_idx);
    fd2_flash_char_hit_sprite((uint32)backbuf, (uint32)target_ids[0]);
    fd2_rle_blit_sprite(bg_resource, 0, 0x32, (uint32)backbuf, 0x140, 0xffffffff);

    caster = data_fd2_battle_runtime_char_array_ptr;
    caster_anim_base =
        (uint32)data_fd2_battle_runtime_char_array_ptr[caster_idx].portrait_id * 3;
    caster_figani_a = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0, caster_anim_base);
    caster_figani_b = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0, caster_anim_base + 2);

    data_fd2_audio_summon_spell_sfx_bank_buf_ptr = 0;
    data_fd2_audio_summon_spell_sfx_bank_buf_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdother_dat, 0,
        (uint32)intro_sfx_bank[spell_id]);

    if (*(int16 *)caster_figani_b == 0) {
        caster_figani_b = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_figani_dat_52388,
            caster_figani_b, caster_anim_base + 1);
    }

    fd2_play_palette_fade_to_black();

    for (i = 0; i < (int)target_count; i++) {
        figani_buf[i] = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_figani_dat_52388, figani_buf[i],
            (uint32)data_fd2_battle_runtime_char_array_ptr[target_ids[i]].portrait_id * 3);
    }

    if (caster[caster_idx].team == 0) {
        team_caster_sprite = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat, 0,
            (uint32)enemy_team_sprite_id[spell_id]);
    } else {
        team_caster_sprite = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdother_dat, 0,
            (uint32)player_team_sprite_id[spell_id]);
    }

    fd2_play_char_intro_zoom_anim(caster_idx, 0, caster_figani_a, figani_buf[0],
                                  (uint32)work, (uint32)backbuf, tai_resource);
    fd2_play_figani_animation_loop(caster_idx, spell_id, (uint8 *)caster_figani_b,
                                   (uint8 *)figani_buf[0], (uint32)work, (uint32)backbuf,
                                   bg_resource, tai_resource);

    /* pre-cast slide-in (spell_id == 9 only) */
    if (spell_id == 9) {
        for (i = 0; i < 0xb; i++) {
            fd2_blit_rectangle((uint32)work + 0x140, 0x280, (uint32)backbuf,
                               0x140, 0x140, 0xc8);
            if (i != 10) {
                fd2_blit_indexed_sprite(caster_figani_a, 0,
                                        (int)((uint32)work + 0x140) + i * -10, 0x280, -1);
            }
            fd2_blit_indexed_sprite(figani_buf[0], 0, (int)((uint32)work + 0x140), 0x280, -1);
            fd2_blit_rectangle(0xa0000, 0x140, (uint32)work + 0x140, 0x280, 0x140, 0xc8);
        }
        fd2_delay_ms(500);
    }

    caster_last_frame = *(uint8 *)caster_figani_b - 1;

    /* pre-cast animation loop (phase 0 returns the frame count) */
    n_frames = data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
                   caster_idx, team_caster_sprite, (uint32)work, 0x140, 0);
    fd2_step_figani_pose_animation(figani_buf[0], 0, (uint32)work, 0x280);
    for (i = 0; i < n_frames; i++) {
        fd2_blit_rectangle((uint32)work + 0x4ba0, 0x280, (uint32)backbuf,
                           0x140, 0x140, 0xc8);
        data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
            caster_idx, team_caster_sprite, (uint32)work + 0x4ba0, 0x280, 1);
        if (spell_id != 9) {
            fd2_blit_indexed_sprite(caster_figani_b, caster_last_frame,
                                    (int)((uint32)work + 0x4ba0), 0x280, -1);
        }
        fd2_step_figani_pose_animation(figani_buf[0], 0xffffffff,
                                       (uint32)work + 0x4ba0, 0x280);
        data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
            caster_idx, team_caster_sprite, (uint32)work + 0x4ba0, 0x280, 2);
        fd2_blit_rectangle(0xa0000, 0x140, (uint32)work + 0x4ba0, 0x280, 0x140, 0xc8);
        fd2_wait_n_bios_ticks(1);
    }

    /* main cast loop, per target */
    for (target_iter = 0; (int)target_iter < (int)target_count; target_iter++) {
        n_frames = data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
                       caster_idx, team_caster_sprite, (uint32)work, 0x140, 3);
        fd2_step_figani_pose_animation(figani_buf[0], 0, (uint32)work, 0x280);

        tgt = &data_fd2_battle_runtime_char_array_ptr[target_ids[target_iter]];
        starting_hp = (int16)tgt->hp_current;
        damage = fd2_calc_magic_damage((uint32)target_ids[target_iter], spell_id);
        final_hp = (int16)tgt->hp_current;
        tgt->hp_current = (uint16)starting_hp;
        is_miss = (uint8)(damage == 0);
        hit_count = 1;

        for (anim_iter = 0; anim_iter < n_frames; anim_iter++) {
            if ((spell_id == 7) || (spell_id == 3) || (spell_id == 9)) {
                swap_toggle = swap_toggle ^ 1;
                swap_buf = (uint32)work + 0x4ba0 - swap_toggle * 0x280;
            } else {
                swap_buf = (uint32)work + 0x4ba0;
            }
            fd2_blit_rectangle(swap_buf, 0x280, (uint32)backbuf, 0x140, 0x140, 0xc8);
            data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
                caster_idx, team_caster_sprite, (uint32)work + 0x4ba0, 0x280, 4);
            if (spell_id != 9) {
                fd2_blit_indexed_sprite(caster_figani_b, caster_last_frame,
                                        (int)((uint32)work + 0x4ba0), 0x280, -1);
            }
            if (is_miss) {
                fd2_step_figani_pose_animation(figani_buf[target_iter], 0xffffffff,
                                               (uint32)work + 0x4ba0, 0x280);
                data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
                    caster_idx, team_caster_sprite, (uint32)work + 0x4ba0, 0x280, 5);
            } else {
                fd2_step_figani_pose_animation(
                    figani_buf[target_iter],
                    shake_step_counter * 0x100 + palette_op_base,
                    shake_y_row_offsets[shake_idx] * 0x280 +
                        shake_x_offsets[shake_idx] * shake_y_dir +
                        (uint32)work + 0x4ba0,
                    0x280);
                shake_step_counter = shake_step_counter - 1;
                if (shake_step_counter == 1) {
                    shake_step_counter = 8;
                }
                if (shake_idx != 3) {
                    shake_idx = shake_idx + 1;
                }
                ret_phase = data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
                                caster_idx, team_caster_sprite, (uint32)work + 0x4ba0, 0x280, 5);
                if (ret_phase == 1) {
                    flash_unit = 1;
                    if ((int)hit_count <= (int)(uint32)hp_lerp_hits[spell_id]) {
                        tgt->hp_current = (uint16)
                            (starting_hp -
                             (int16)(((int)(starting_hp - final_hp) * (int)hit_count) /
                                     (int)(uint32)hp_lerp_hits[spell_id]));
                        flash_unit = (uint32)target_ids[target_iter];
                        fd2_flash_char_hit_sprite((uint32)backbuf, flash_unit);
                        hit_count = hit_count + 1;
                    }
                    shake_idx = 0;
                    /* shake_y_dir is computed from the freshly-advanced RNG
                     * seed left in EAX by fd2_advance_rng_state (range
                     * [0,0xFFFF]), NOT from flash_unit. The decompiler
                     * mis-attributes the IDIV operand to flash_unit because
                     * fd2_advance_rng_state is prototyped void; the
                     * disassembly (0x2af40-0x2af58) divides the CALL's EAX
                     * return by 3. Ghidra EAX-tracking bug, corrected here. */
                    shake_y_dir = 1 - (int)fd2_advance_rng_state() % 3;
                }
            }
            fd2_blit_rectangle(0xa0000, 0x140, (uint32)work + 0x4ba0, 0x280, 0x140, 0xc8);
            fd2_wait_n_bios_ticks(1);
        }

        if (target_count - 1 != target_iter) {
            fd2_animate_spell_hit_cinematic(caster_idx, team_caster_sprite, caster_figani_b,
                                            figani_buf[target_iter], (uint32)work,
                                            (uint32)backbuf, figani_buf[target_iter + 1],
                                            spell_id);
            fd2_flash_char_hit_sprite((uint32)backbuf,
                                      (uint32)target_ids[target_iter + 1]);
        }
    }

    /* post-cast animation loop (phase 6 returns the frame count) */
    n_frames = data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
                   caster_idx, team_caster_sprite, (uint32)work, 0x140, 6);
    for (i = 0; i < n_frames; i++) {
        fd2_blit_rectangle((uint32)work + 0x4ba0, 0x280, (uint32)backbuf,
                           0x140, 0x140, 0xc8);
        data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
            caster_idx, team_caster_sprite, (uint32)work + 0x4ba0, 0x280, 7);
        if (spell_id != 9) {
            fd2_blit_indexed_sprite(caster_figani_b, caster_last_frame,
                                    (int)((uint32)work + 0x4ba0), 0x280, -1);
        }
        fd2_step_figani_pose_animation(figani_buf[target_count - 1], 0xffffffff,
                                       (uint32)work + 0x4ba0, 0x280);
        data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_id](
            caster_idx, team_caster_sprite, (uint32)work + 0x4ba0, 0x280, 8);
        fd2_blit_rectangle(0xa0000, 0x140, (uint32)work + 0x4ba0, 0x280, 0x140, 0xc8);
        fd2_wait_n_bios_ticks(1);
    }

    /* post-cast slide-out (spell_id == 9 only) */
    if (spell_id == 9) {
        for (i = 7; -1 < i; i--) {
            uint32 slide_buf = (uint32)work + 0x140;
            fd2_blit_rectangle(slide_buf, 0x280, (uint32)backbuf, 0x140, 0x140, 0xc8);
            fd2_blit_indexed_sprite(caster_figani_a, 0, (int)slide_buf + i * -10, 0x280, -1);
            fd2_blit_indexed_sprite(figani_buf[0], 0, (int)slide_buf, 0x280, -1);
            fd2_blit_rectangle(0xa0000, 0x140, slide_buf, 0x280, 0x140, 0xc8);
        }
    }

    /* final shake / shine lerp (4 frames) */
    for (i = 1; i < 4; i++) {
        int32 shine_remap;
        fd2_blit_rectangle((uint32)work, 0x140, (uint32)backbuf, 0x140, 0x140, 0xc8);
        shine_remap = *(int32 *)(data_fd2_tile_anim_table_base + 6 +
                                 (shine_table_offset + i) * 4) +
                      (int32)data_fd2_tile_anim_table_base;
        fd2_rle_blit_with_palette_remap((uint16 *)bg_resource, 0, 0x32, (uint32)backbuf, 0x140, shine_remap);
        fd2_rle_blit_with_palette_remap((uint16 *)tai_resource, 0xa4, 0x9d, (uint32)backbuf, 0x140, shine_remap);
        fd2_blit_indexed_sprite(caster_figani_b, 0, (int)(uint32)work, 0x140, -1);
        fd2_step_figani_pose_animation(figani_buf[target_count - 1], 0xffffffff,
                                       (uint32)work, 0x140);
        fd2_blit_rectangle(0xa0000, 0x140, (uint32)work, 0x140, 0x140, 0xc8);
        fd2_wait_n_bios_ticks(1);
    }

    fd2_blit_rectangle((uint32)work, 0x140, (uint32)backbuf, 0x140, 0x140, 0xc8);
    fd2_rle_blit_sprite(bg_resource, 0, 0x32, (uint32)backbuf, 0x140, 0xffffffff);
    fd2_rle_blit_sprite(tai_resource, 0xa4, 0x9d, (uint32)backbuf, 0x140, 0xffffffff);
    fd2_blit_indexed_sprite(caster_figani_b, 0, (int)(uint32)work, 0x140, -1);
    fd2_step_figani_pose_animation(figani_buf[target_count - 1], 0xffffffff,
                                   (uint32)work, 0x140);
    fd2_blit_rectangle(0xa0000, 0x140, (uint32)work, 0x140, 0x140, 0xc8);

    fd2_play_sfx_with_handle(data_fd2_audio_summon_spell_sfx_bank_buf_ptr, -1, 1);
    free((void *)data_fd2_audio_summon_spell_sfx_bank_buf_ptr);
    free((void *)team_caster_sprite);
    for (i = 0; i < (int)target_count; i++) {
        free((void *)figani_buf[i]);
    }
    free(backbuf);
    free(work);
    free((void *)caster_figani_a);
    free((void *)caster_figani_b);
    free((void *)bg_resource);
    free((void *)tai_resource);

    data_fd2_large_game_state_buffer_ptr = (uint32)malloc(0x25680);
    data_fd2_battle_scene_snapshot = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65, data_fd2_battle_scene_snapshot,
        (uint32)*(uint8 *)data_fd2_tile_event_data_table_ptr * 2);
    fd2_restore_portrait_cache_from_tmp();
    fd2_wait_n_bios_ticks(8);
    fd2_play_palette_fade_to_black();
    memset((void *)0xa0000, 0, 64000);
    fd2_composite_battle_frame(1);
    fd2_play_palette_fade_in();
}

/* --------------------------------------------------------------------------
 * Module data
 * ------------------------------------------------------------------------ */

/*
 * Summon-spell SFX bank buffer pointer (FDOTHER.DAT bank, malloc-backed).
 * Mutable runtime handle: set to 0 then assigned the loaded bank buffer in
 * fd2_execute_summon_spell_cast / fd2_play_spell_cast_sequence, read by the
 * summon animation tick handlers, freed at teardown. Zero-initialized (.bss).
 */
uint32 data_fd2_audio_summon_spell_sfx_bank_buf_ptr;
