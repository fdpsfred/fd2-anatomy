/*
 * anicine.c — FD2 cinematic full-screen image display (chapter-ending flow).
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
#include <stdlib.h>
#include <conio.h>

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

/* ----------------------------------------------------------------
 * fd2_play_full_combat_cinematic @ 0x28A6C  (4 callers)
 *
 * FULL COMBAT CINEMATIC — the dramatic 1-on-1 attack/counter close-up
 * shown for major hits (boss attacks / climactic moments / scripted
 * event set pieces).
 *
 * Callers:
 *   fd2_execute_ai_physical_attack          @ 0x1548E  (AI physical hit, gate open)
 *   fd2_player_inline_action_menu_dispatch  @ 0x18D8C  (player attack)
 *   fd2_chapter_event_handler ch25 1st-time @ 0x353DA  (scripted event)
 *   fd2_play_game_ending_cinematic          @ 0x2BCE5  (ending epilogue)
 *
 * Setup: when not in scripted mode (data_..._scripted_cinematic_mode == 0)
 * free the three big in-game caches, then allocate a 64000-byte mode-13h
 * framebuffer scratch (dst) and a 0x1F400 composite work buffer.
 *
 * Pick the "spotlight" char (the player-side combatant) and the "terrain"
 * char (the other one): if the attacker is enemy-team (team==0) the
 * attacker is spotlight, else the defender is. For each, derive a terrain
 * background byte: normally the tile attribute under the char's grid pos,
 * but for immune (job 0x13, or archetype 4/5 with portrait != 0x1C)
 * classes whose under-foot tile is wrong, use the per-chapter override
 * byte data_fd2_chapter_combat_cinematic_mode_per_chapter[chapter] when
 * that byte is non-zero.
 *
 * In scripted mode the spotlight terrain is forced to the scripted-mode
 * value, and the name-banner index is forced to 3 for the climactic
 * portraits (attacker 0x1A/0x36 or defender 0x37).
 *
 * Resource loads: name-banner from TAI.DAT, attacker/defender FIGANI
 * silhouettes + animation streams from FIGANI.DAT, terrain backdrop(s)
 * from BG.DAT. The defender's animation FIGANI (att_anim_figani) carries
 * a split-screen flag in byte +1: when non-zero a 3-layer split
 * background (BG.DAT[0/1/2]) is built and the spotlight/split buffers
 * swapped for an enemy-team attacker.
 *
 * Sequence: fade to black, flash the attacker silhouette, check counter
 * eligibility (loads the defender's counter-blow FIGANI if eligible),
 * zoom the pose in, run the attacker hit cinematic; if it landed and the
 * defender can counter (and not scripted) run the swapped counter
 * cinematic; in scripted mode latch the flag to 1 and run the counter
 * cinematic for the guided sequence.
 *
 * Cleanup: free all scratch + loaded resources; when not scripted,
 * reallocate the large game-state buffer, reload the battle-scene
 * snapshot from FDSHAP.DAT, restore the portrait cache, settle 6 ticks,
 * fade to black, clear VGA, recomposite the battle frame, stop the FIGANI
 * SFX, free the SFX banks, fade in.
 *
 * void __cdecl (2 stack params). EBX/ESI/EDI/EBP are callee-saved; the
 * __CHK(0x64) stack-probe prologue is compiler-injected.
 * ---------------------------------------------------------------- */
void fd2_play_full_combat_cinematic(uint32 a, uint32 d)
{
    void *dst;
    void *workbuf;
    runtime_char *p_attacker;
    runtime_char *p_defender;
    runtime_char *p_spotlight;
    runtime_char *p_terrain;
    uint32 attacker_portrait;
    uint32 defender_portrait;
    uint32 spotlight_terrain;
    uint32 defender_terrain;
    uint32 banner_term;
    uint32 banner_idx;
    uint32 split_screen_flag;
    uint32 banner_rle;
    uint32 def_silhouette;
    uint32 att_silhouette;
    uint32 att_anim_figani;
    uint32 def_anim_figani;
    uint8  tile_attr[8];

    def_anim_figani = 0;
    data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr = 0;
    p_attacker = &data_fd2_battle_runtime_char_array_ptr[a];
    p_defender = &data_fd2_battle_runtime_char_array_ptr[d];
    defender_portrait = p_defender->portrait_id;
    attacker_portrait = p_attacker->portrait_id;
    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0) {
        free((void *)portrait_sprite_cache);
        free((void *)data_fd2_large_game_state_buffer_ptr);
        free((void *)battle_scene_snapshot);
        battle_scene_snapshot = 0;
    }
    dst = malloc(64000);
    workbuf = malloc(0x1F400);
    memset(dst, 0, 64000);

    p_spotlight = p_defender;
    p_terrain = p_attacker;
    if (p_attacker->team == 0) {
        p_spotlight = p_attacker;
        p_terrain = p_defender;
    }

    /* spotlight terrain: keep per-chapter override only for immune classes
     * with a non-zero override byte and portrait != 0x1C, else read tile */
    spotlight_terrain =
        data_fd2_chapter_combat_cinematic_mode_per_chapter
            [data_fd2_chapter_current_chapter_id];
    if ((((p_spotlight->job_id != 0x13) && (p_spotlight->archetype_flag != 4)) &&
         (p_spotlight->archetype_flag != 5)) ||
        ((spotlight_terrain == 0) || (p_spotlight->portrait_id == 0x1C))) {
        fd2_read_tile_attribute_at_pos(p_spotlight->pos_x, p_spotlight->pos_y,
                                       (uint32)tile_attr);
        spotlight_terrain = tile_attr[6];
    }

    /* terrain char's backdrop byte: always read the tile first, then apply
     * the same immune-class override rule.
     *
     * The binary keeps TWO distinct values here (EAX vs the [ESP+8] slot):
     *   banner_term     (EAX)     -> non-scripted TAI.DAT name-banner index
     *   defender_terrain ([ESP+8]) -> BG.DAT split-bg index (split path below)
     * Both start equal to the per-chapter override byte and stay in lockstep
     * EXCEPT the immune + override==0 sub-case: there EAX (banner_term) is left
     * holding override(0) while [ESP+8] (defender_terrain) is reloaded to the
     * under-foot tile attribute. Tracking them separately preserves that split. */
    fd2_read_tile_attribute_at_pos(p_terrain->pos_x, p_terrain->pos_y,
                                   (uint32)tile_attr);
    banner_term =
        data_fd2_chapter_combat_cinematic_mode_per_chapter
            [data_fd2_chapter_current_chapter_id];
    defender_terrain = banner_term;
    if ((((p_terrain->job_id == 0x13) || (p_terrain->archetype_flag == 4)) ||
         (p_terrain->archetype_flag == 5)) &&
        (p_terrain->portrait_id != 0x1C)) {
        if (defender_terrain == 0) {
            defender_terrain = tile_attr[6];
            /* banner_term stays = override(0) here (EAX not reloaded) */
        }
    } else {
        banner_term = tile_attr[6];
        defender_terrain = tile_attr[6];
    }

    /* scripted mode forces the spotlight terrain and (for the climactic
     * portraits) the banner index */
    banner_idx = banner_term;
    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx != 0) {
        spotlight_terrain = data_fd2_battle_scripted_cinematic_mode_or_terrain_idx;
        banner_idx = data_fd2_battle_scripted_cinematic_mode_or_terrain_idx;
        if (((attacker_portrait == 0x1A) || (attacker_portrait == 0x36)) ||
            (defender_portrait == 0x37)) {
            banner_idx = 3;
        }
    }

    banner_rle = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_tai_dat, 0, banner_idx);
    def_silhouette = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
        defender_portrait * 3);
    att_silhouette = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
        attacker_portrait * 3);
    att_anim_figani = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
        attacker_portrait * 3 + 1);
    split_screen_flag = *(uint8 *)(att_anim_figani + 1);
    fd2_play_palette_fade_to_black();
    data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_bg_dat_52381,
            data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr,
            spotlight_terrain);
    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0) {
        fd2_flash_char_hit_sprite((uint32)dst, a);
    }
    if ((fd2_check_can_counter_attack(a, d) == 1) ||
        (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx != 0)) {
        def_anim_figani = fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_figani_dat_52388, 0,
            defender_portrait * 3 + 1);
    }

    if (split_screen_flag == 0) {
        fd2_rle_blit_sprite(
            data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr,
            0, 0x32, (uint32)dst, 0x140, 0xFFFFFFFF);
        if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0) {
            fd2_flash_char_hit_sprite((uint32)dst, d);
        }
    } else {
        uint32 swap_tmp;

        data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr = 0;
        data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = 0;
        data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = 0;
        data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = 0;
        data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_bg_dat_52381, 0,
                defender_terrain);
        data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_bg_dat_52381,
                data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr, 0);
        data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_bg_dat_52381,
                data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr, 1);
        data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_bg_dat_52381,
                data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr, 2);
        swap_tmp = data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr;
        if (p_attacker->team == 0) {
            data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr =
                data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr;
            data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr = swap_tmp;
        }
        fd2_rle_blit_sprite(
            data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr,
            0, 0x32, (uint32)dst, 0x140, 0xFFFFFFFF);
    }

    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0) {
        data_fd2_audio_figani_sfx_bank_buf_ptr =
            fd2_load_figani_sfx_bank(att_anim_figani);
        data_fd2_audio_figani_sfx_bank_defender_buf_ptr =
            fd2_load_figani_sfx_bank(def_anim_figani);
    }
    fd2_play_char_intro_zoom_anim(a, split_screen_flag, att_silhouette,
                                  def_silhouette, (uint32)workbuf, (uint32)dst,
                                  banner_rle);
    if (fd2_execute_combat_hit_cinematic(
            a, d, att_anim_figani, def_silhouette, (uint32)workbuf, (uint32)dst,
            banner_rle, data_fd2_audio_figani_sfx_bank_buf_ptr) != 0) {
        if ((fd2_check_can_counter_attack(a, d) == 1) &&
            (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0)) {
            fd2_execute_combat_hit_cinematic(
                d, a, def_anim_figani, att_silhouette, (uint32)workbuf,
                (uint32)dst, banner_rle,
                data_fd2_audio_figani_sfx_bank_defender_buf_ptr);
        }
    }
    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx != 0) {
        data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 1;
        fd2_execute_combat_hit_cinematic(
            d, a, def_anim_figani, att_silhouette, (uint32)workbuf,
            (uint32)dst, banner_rle,
            data_fd2_audio_figani_sfx_bank_defender_buf_ptr);
    }

    if ((split_screen_flag == 0) &&
        (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0)) {
        fd2_blit_rectangle((uint32)workbuf, 0x280, (uint32)dst, 0x140, 0x140, 0xC8);
        fd2_rle_blit_sprite(banner_rle, 0xA4, 0x9D, (uint32)workbuf, 0x280,
                            0xFFFFFFFF);
        fd2_blit_indexed_sprite(def_silhouette, 0, (uint32)workbuf, 0x280, -1);
        fd2_blit_indexed_sprite(att_silhouette, 0, (uint32)workbuf, 0x280, -1);
        fd2_blit_rectangle(0xA0000, 0x140, (uint32)workbuf, 0x280, 0x140, 0xC8);
    }

    free(dst);
    free(workbuf);
    free((void *)def_silhouette);
    free((void *)att_silhouette);
    free((void *)att_anim_figani);
    free((void *)data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr);
    free((void *)banner_rle);
    if (split_screen_flag != 0) {
        free((void *)data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr);
        free((void *)data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr);
        free((void *)data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr);
        def_anim_figani = data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr;
    }
    free((void *)def_anim_figani);

    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0) {
        data_fd2_large_game_state_buffer_ptr = (uint32)malloc(0x25680);
        battle_scene_snapshot =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
                battle_scene_snapshot,
                (uint32) * (uint8 *)data_fd2_tile_event_data_table_ptr * 2);
        fd2_restore_portrait_cache_from_tmp();
        fd2_wait_n_bios_ticks(6);
        fd2_play_palette_fade_to_black();
        memset((void *)0xA0000, 0, 64000);
        fd2_composite_battle_frame(1);
        fd2_play_sfx_with_handle(data_fd2_audio_figani_sfx_bank_buf_ptr, -1, 1);
        if (data_fd2_audio_figani_sfx_bank_buf_ptr != 0) {
            free((void *)data_fd2_audio_figani_sfx_bank_buf_ptr);
        }
        if (data_fd2_audio_figani_sfx_bank_defender_buf_ptr != 0) {
            free((void *)data_fd2_audio_figani_sfx_bank_defender_buf_ptr);
        }
        fd2_play_palette_fade_in();
    }
}

/* ----------------------------------------------------------------
 * fd2_play_char_intro_zoom_anim @ 0x29164  (6 callers)
 *
 * 9-frame zoom-in / fade-in introduction animation for a character
 * displayed in a special-attack cinematic backdrop. Each frame combines a
 * 10-px-per-frame slide with a palette-darkening fade (intensity steps of 6
 * from 0x36 down to 0). Drives the "character sweeps onto the screen" intro
 * before the per-hit FIGANI frames play.
 *
 * Branch on data_fd2_battle_runtime_char_array_ptr[char_unit_id].team
 * (0 = enemy, 1 = NPC ally, 2 = player):
 *
 *   team != 0 (ally / player) -> TOP-HALF display, slide-in from the right:
 *     for frame in 8..0 descending:
 *       clear workspace from bg_sprite, (mode_flag==0) lay the static char
 *       sprite (char_sprite2), RLE-blit the background (weapon_sprite) and the
 *       overlay (char_sprite) at workspace + frame*10, push to VGA, ramp the
 *       palette by frame*6.
 *     final settle: RLE-blit the background into bg_sprite at stride 0x140.
 *
 *   team == 0 (enemy) -> BOTTOM-HALF display (workspace + 0x140 origin),
 *   slide direction reversed:
 *     (mode_flag==0) settle the background into bg_sprite first.
 *     for iter in 8..0 descending:
 *       clear the bottom-half region from bg_sprite, RLE-blit the overlay
 *       (char_sprite) at base - iter*10, (mode_flag==0) lay the static char
 *       sprite (char_sprite2) at base, push to VGA, ramp the palette by iter*6.
 *
 * mode_flag == 0 enables the static character-sprite layer; nonzero skips it
 * (used when the caller pre-composited the character into the backdrop, e.g.
 * for split-screen 1-on-1 cinematics).
 *
 * Positional args mirror the two callers in this file:
 *   char_unit_id  unit index -> runtime_char.team selects top/bottom half
 *   mode_flag     0 = draw static char layer, nonzero = skip it
 *   char_sprite   sliding overlay sprite (blitted every frame at the offset)
 *   char_sprite2  static character sprite (blitted at the fixed origin)
 *   workspace     composite work buffer (0x280-stride slide base)
 *   bg_sprite     clear source + final-settle RLE destination (0x140 stride)
 *   weapon_sprite RLE background sprite stream
 *
 * Globals touched: data_fd2_battle_runtime_char_array_ptr [0x53A45] (read team).
 *
 * Callers: fd2_execute_special_attack_skill, fd2_execute_summon_spell_cast,
 *   fd2_play_figani_char_intro_animation, fd2_play_final_chapter_30_ending,
 *   fd2_play_full_combat_cinematic, fd2_play_spell_cast_sequence.
 * System = battle (cinematic-intro zoom + palette-fade reveal).
 * ---------------------------------------------------------------- */
void fd2_play_char_intro_zoom_anim(uint32 char_unit_id, uint32 mode_flag,
                                   uint32 char_sprite, uint32 char_sprite2,
                                   uint32 workspace, uint32 bg_sprite,
                                   uint32 weapon_sprite)
{
    int    frame;
    uint32 blit_dst;
    uint32 base;

    if (data_fd2_battle_runtime_char_array_ptr[char_unit_id].team != 0) {
        for (frame = 8; frame >= 0; frame--) {
            fd2_blit_rectangle(workspace, 0x280, bg_sprite, 0x140, 0x140, 0xC8);
            if (mode_flag == 0) {
                fd2_blit_indexed_sprite(char_sprite2, 0, (int)workspace, 0x280,
                                        -1);
            }
            blit_dst = workspace + (uint32)frame * 10;
            fd2_rle_blit_sprite(weapon_sprite, 0xA4, 0x9D, blit_dst, 0x280,
                                0xFFFFFFFF);
            fd2_blit_indexed_sprite(char_sprite, 0, (int)blit_dst, 0x280, -1);
            fd2_blit_rectangle(0xA0000, 0x140, workspace, 0x280, 0x140, 0xC8);
            fd2_set_vga_palette_range(0, 0xFF, (uint32)frame * 6);
        }
        fd2_rle_blit_sprite(weapon_sprite, 0xA4, 0x9D, bg_sprite, 0x140,
                            0xFFFFFFFF);
        return;
    }

    if (mode_flag == 0) {
        fd2_rle_blit_sprite(weapon_sprite, 0xA4, 0x9D, bg_sprite, 0x140,
                            0xFFFFFFFF);
    }
    for (frame = 8; frame >= 0; frame--) {
        base = workspace + 0x140;
        fd2_blit_rectangle(base, 0x280, bg_sprite, 0x140, 0x140, 0xC8);
        fd2_blit_indexed_sprite(char_sprite, 0, (int)(base - (uint32)frame * 10),
                                0x280, -1);
        if (mode_flag == 0) {
            fd2_blit_indexed_sprite(char_sprite2, 0, (int)base, 0x280, -1);
        }
        fd2_blit_rectangle(0xA0000, 0x140, workspace + 0x140, 0x280, 0x140,
                           0xC8);
        fd2_set_vga_palette_range(0, 0xFF, (uint32)frame * 6);
    }
}

/* ----------------------------------------------------------------
 * fd2_execute_combat_hit_cinematic @ 0x2939D  (1 caller)
 *
 * COMBAT HIT EXECUTION inside the FIGANI cinematic. Drives the per-frame
 * composite of the attacker's strike, the target reaction, progressive
 * damage application, SFX, and the crit/poison palette flash. Called twice
 * per encounter by fd2_play_full_combat_cinematic @ 0x28A6C (attacker phase,
 * then counter-attack phase).
 *
 * Per-encounter hit-count gate:
 *   hit_count defaults to 1. fd2_calculate_combat_hit_outcome fills the local
 *   outcome block (miss/crit/poison/reserved/double-hit/damage). In scripted
 *   mode (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx != 0) the
 *   outcome is forced to all-zero. A 3% RNG roll, or the double-hit flag on
 *   the first pass, raises hit_count to 2.
 *
 * Charge-in sub-animation (figani[+1] != 0): plays the attacker's approach
 * frames, then the background zoom transition. Branch on attacker.team:
 *   team != 0 (player/ally attacker) -> fd2_animate_bg_zoom_transition_in
 *                                       (top-half composite, workspace+0x140)
 *   team == 0 (enemy attacker)       -> fd2_animate_bg_zoom_transition_out
 *                                       (bottom-half composite, workspace)
 *
 * Per-frame inner loop: restore from framebuffer, composite attacker frame
 * and defender pose (the defender pose is shaken by the x/y shake tables,
 * indexed by a shake counter that resets to 5 on each landed hit and decays),
 * push to VGA. frame[+4]==1 marks a hit: HP is reduced progressively as
 * defender_HP_initial - hit_index*damage/total_hits (clamped >= 0). frame[+5]
 * is an SFX hook; frame[+7]&1 selects the slash-layer draw order. On the final
 * hit frame, poison flashes the DAC magenta and crit flashes it white.
 *
 * After all subframes for a strike, hit_count is decremented; while it stays
 * positive the attacker recoil + opposite-direction zoom replays for the
 * second strike. Returns the defender's final hp_current, used by the caller
 * to decide whether the defender died this cinematic. In scripted mode, once
 * the latched flag is 1 and the last hit landed, returns 1 early.
 *
 * Globals: data_fd2_battle_runtime_char_array_ptr [0x53A45] (reads
 * attacker.team, writes defender.hp_current); data_fd2_battle_scripted_
 * cinematic_mode_or_terrain_idx [0x540FF]; the combat-cinematic background
 * buffers; data_fd2_battle_combat_hit_shake_x/y_offset_table [0x5255F/0x52577].
 * DAC ports 0x3C8/0x3C9 drive the crit/poison palette flash.
 *
 * int __cdecl, 8 stack params. EBX/ESI/EDI/EBP callee-saved; __CHK(0xB8)
 * stack-probe prologue is compiler-injected.
 *
 * NOTE: the 3% bonus-hit roll uses fd2_advance_rng_state()'s return value
 * (Ghidra's decompiler mis-attributes it to defender_idx*0x50 via its EAX
 * tracking bug; the disassembly does MOV EDX,EAX right after the CALL).
 * ---------------------------------------------------------------- */
int fd2_execute_combat_hit_cinematic(uint32 attacker_idx, uint32 defender_idx,
    uint32 attacker_figani, uint32 defender_figani, uint32 workspace,
    uint32 framebuffer, uint32 name_banner_sprite, uint32 sfx_bank)
{
    runtime_char *rt_chars;
    int32  i;
    int32  shake_x_table[6];
    int32  shake_y_table[6];
    /* fd2_calculate_combat_hit_outcome writes 6 consecutive dwords through the
     * passed pointer (one contiguous stack block in the binary). Kept as a
     * struct so the [0..5] layout the callee fills stays contiguous. */
    struct {
        uint32 miss_flag;       /* +0  1 = miss (whiff only) */
        uint32 crit_flag;       /* +1  1 = critical (white DAC flash) */
        uint32 poison_applied;  /* +2  1 = poison (magenta DAC flash) */
        uint32 unused;          /* +3  reserved */
        uint32 double_hit_flag; /* +4  1 = play two strikes back-to-back */
        uint32 damage_value;    /* +5  HP to drain across the hit frames */
    } oc;
    uint32 hit_count;
    uint32 hits_consumed;
    uint32 defender_HP_initial;
    uint32 defender_HP_after;
    uint32 total_hit_frames;
    uint32 hit_count_so_far;
    uint32 frame_iter;
    uint32 subframe_iter;
    int32  shake_idx;
    uint32 sprite_palette_color;
    uint32 defender_y_offset;
    uint32 dst;
    uint32 frame_entry;
    uint8  defender_figani_iter;
    uint8  subframe_step;
    int    double_hit_consumed;

    rt_chars = data_fd2_battle_runtime_char_array_ptr;
    subframe_step = 0;
    defender_figani_iter = 0;
    /* The binary leaves these two stack slots ([ESP+0x50] / [ESP+0x58])
     * uninitialized on its degenerate paths (a zero-hit-frame attacker_figani,
     * or scripted mode where the HP-initial load is skipped). Real FIGANI data
     * always has hit frames, so on every reachable real-data path both are
     * assigned in the frame loop before being read; the 0 init only removes the
     * compiler's may-be-uninitialized warning and is behavior-identical there.
     * In the scripted edge it yields a benign 0 (the scripted cinematic's
     * defender HP is cosmetic; real combat resolution lives elsewhere) instead
     * of UB stack garbage. */
    defender_HP_initial = 0;
    defender_HP_after = 0;
    for (i = 0; i < 6; i++) {
        shake_x_table[i] = data_fd2_battle_combat_hit_shake_x_offset_table[i];
    }
    for (i = 0; i < 6; i++) {
        shake_y_table[i] = data_fd2_battle_combat_hit_shake_y_offset_table[i];
    }
    shake_idx = 0;
    sprite_palette_color = 0xFFFFFFFF;
    hit_count = 1;
    total_hit_frames = 0;
    double_hit_consumed = 0;

    for (i = 0; i < (int32)(uint32) * (uint8 *)attacker_figani; i++) {
        if (*(int8 *)(*(int32 *)(attacker_figani + 8 + i * 4)
                      + 4 + attacker_figani) != 0) {
            total_hit_frames = total_hit_frames + 1;
        }
    }
    if (total_hit_frames == 0) {
        total_hit_frames = 1;
    }

    /* 3% bonus-hit roll keys off the RNG return value (see header note). */
    if (fd2_advance_rng_state() % 100 < 3) {
        hit_count = 2;
    }

    do {
        hits_consumed = hit_count - 1;
        if (hit_count == 0) {
            return (int)defender_HP_after;
        }
        hit_count_so_far = 0;
        if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0) {
            defender_HP_initial = (uint32)rt_chars[defender_idx].hp_current;
            fd2_calculate_combat_hit_outcome(attacker_idx, defender_idx,
                                             (uint32 *)&oc);
            if ((!double_hit_consumed) && (oc.double_hit_flag != 0)) {
                double_hit_consumed = 1;
                hits_consumed = hit_count;
            }
        }
        hit_count = hits_consumed;
        frame_iter = 0;
        if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx != 0) {
            oc.miss_flag = 0;
            oc.crit_flag = 0;
            oc.poison_applied = 0;
            oc.unused = 0;
            oc.double_hit_flag = 0;
            oc.damage_value = 0;
        }

        if (*(int8 *)(attacker_figani + 1) != 0) {
            memset((void *)workspace, 0, 0x1F400);
            if (rt_chars[attacker_idx].team == 0) {
                for (; (int32)frame_iter
                       < (int32)(uint32) * (uint8 *)(attacker_figani + 2);
                     frame_iter = frame_iter + 1) {
                    frame_entry =
                        *(int32 *)(attacker_figani + 8 + frame_iter * 4)
                        + attacker_figani;
                    if (*(int8 *)(frame_entry + 5) != 0) {
                        fd2_play_sfx_with_handle(
                            sfx_bank, *(uint8 *)(frame_entry + 5), 1);
                    }
                    fd2_blit_rectangle(workspace, 0x280, framebuffer, 0x140,
                                       0x140, 0xC8);
                    fd2_blit_indexed_sprite(attacker_figani, frame_iter,
                                            (int)workspace, 0x280, -1);
                    fd2_blit_rectangle(0xA0000, 0x140, workspace, 0x280, 0x140,
                                       0xC8);
                    fd2_wait_n_bios_ticks(*(uint8 *)(frame_entry + 6));
                }
                fd2_animate_bg_zoom_transition_out(
                    defender_idx, defender_figani, name_banner_sprite,
                    framebuffer, workspace,
                    data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr);
            } else {
                for (; (int32)frame_iter
                       < (int32)(uint32) * (uint8 *)(attacker_figani + 2);
                     frame_iter = frame_iter + 1) {
                    frame_entry =
                        *(int32 *)(attacker_figani + 8 + frame_iter * 4)
                        + attacker_figani;
                    if (*(int8 *)(frame_entry + 5) != 0) {
                        fd2_play_sfx_with_handle(
                            sfx_bank, *(uint8 *)(frame_entry + 5), 1);
                    }
                    dst = workspace + 0x140;
                    fd2_blit_rectangle(dst, 0x280, framebuffer, 0x140, 0x140,
                                       0xC8);
                    fd2_blit_indexed_sprite(attacker_figani, frame_iter,
                                            (int)dst, 0x280, -1);
                    fd2_blit_rectangle(0xA0000, 0x140, dst, 0x280, 0x140, 0xC8);
                    fd2_wait_n_bios_ticks(*(uint8 *)(frame_entry + 6));
                }
                fd2_animate_bg_zoom_transition_in(
                    defender_idx, defender_figani, framebuffer, workspace,
                    data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr);
            }
        }

        for (; (int32)frame_iter < (int32)(uint32) * (uint8 *)attacker_figani;
             frame_iter = frame_iter + 1) {
            frame_entry = *(int32 *)(attacker_figani + 8 + frame_iter * 4)
                          + attacker_figani;
            if (*(int8 *)(frame_entry + 4) == 0) {
                if (*(int8 *)(frame_entry + 5) != 0) {
                    fd2_play_sfx_with_handle(
                        sfx_bank, *(uint8 *)(frame_entry + 5), 1);
                }
            } else {
                hit_count_so_far = hit_count_so_far + 1;
                defender_HP_after =
                    defender_HP_initial
                    - (int32)(hit_count_so_far * oc.damage_value)
                          / (int32)total_hit_frames;
                if ((int32)defender_HP_after < 0) {
                    defender_HP_after = 0;
                }
                rt_chars[defender_idx].hp_current = (uint16)defender_HP_after;
                if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx
                    == 0) {
                    fd2_flash_char_hit_sprite(framebuffer, defender_idx);
                }
                if (oc.miss_flag == 0) {
                    shake_idx = 5;
                    sprite_palette_color = 0x21;
                    if (*(int8 *)(frame_entry + 5) != 0) {
                        fd2_play_sfx_with_handle(
                            sfx_bank, *(uint8 *)(frame_entry + 5), 1);
                    }
                } else if (*(int8 *)(frame_entry + 5) != 0) {
                    fd2_play_sfx_with_handle(sfx_bank, 0, 1);
                }
            }

            for (subframe_iter = 0;
                 (int32)subframe_iter
                     < (int32)(uint32) * (uint8 *)(frame_entry + 6);
                 subframe_iter = subframe_iter + 1) {
                defender_y_offset = (uint32)shake_y_table[shake_idx];
                if (*(int8 *)(attacker_figani + 1) != 0) {
                    defender_y_offset = 0;
                }
                dst = workspace + 0x3EA8;
                fd2_blit_rectangle(dst, 400, framebuffer, 0x140, 0x140, 0xC8);
                if (rt_chars[attacker_idx].team == 0) {
                    if ((*(uint8 *)(frame_entry + 7) & 1) == 0) {
                        fd2_blit_indexed_sprite(attacker_figani, frame_iter,
                                                (int)dst, 400, -1);
                    }
                    fd2_blit_indexed_sprite(
                        defender_figani, defender_figani_iter,
                        (int)(workspace + 0x3EA8 + (uint32)shake_x_table[shake_idx]
                              + defender_y_offset * 400),
                        400, sprite_palette_color);
                    if ((*(uint8 *)(frame_entry + 7) & 1) != 0) {
                        fd2_blit_indexed_sprite(attacker_figani, frame_iter,
                                                (int)(workspace + 0x3EA8), 400,
                                                -1);
                    }
                } else {
                    if ((*(uint8 *)(frame_entry + 7) & 1) != 0) {
                        fd2_blit_indexed_sprite(attacker_figani, frame_iter,
                                                (int)dst, 400, -1);
                    }
                    fd2_blit_indexed_sprite(
                        defender_figani, defender_figani_iter,
                        (int)((workspace + 0x3EA8
                               - (uint32)shake_x_table[shake_idx])
                              - defender_y_offset * 400),
                        400, sprite_palette_color);
                    if ((*(uint8 *)(frame_entry + 7) & 1) == 0) {
                        fd2_blit_indexed_sprite(attacker_figani, frame_iter,
                                                (int)(workspace + 0x3EA8), 400,
                                                -1);
                    }
                }
                fd2_blit_rectangle(0xA0000, 0x140, workspace + 0x3EA8, 0x190,
                                   0x140, 0xC8);
                if ((*(int8 *)(frame_entry + 4) == 1)
                    && (hit_count_so_far == total_hit_frames)) {
                    if (oc.poison_applied != 0) {
                        outp(0x3C8, 0);
                        outp(0x3C9, 1);
                        outp(0x3C9, 0x20);
                        outp(0x3C9, 0);
                        __delay_thunk_375b2(0x14);
                        outp(0x3C8, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                    }
                    if (oc.crit_flag != 0) {
                        outp(0x3C8, 0);
                        outp(0x3C9, 0x3F);
                        outp(0x3C9, 0x3F);
                        outp(0x3C9, 0x3F);
                        __delay_thunk_375b2(0x14);
                        outp(0x3C8, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                        __delay_thunk_375b2(0x28);
                    }
                }
                subframe_step = subframe_step + 1;
                if (subframe_step
                    == *(uint8 *)(defender_figani
                                  + *(int32 *)((uint32)defender_figani_iter * 4
                                               + defender_figani + 8)
                                  + 6)) {
                    subframe_step = 0;
                    defender_figani_iter = defender_figani_iter + 1;
                    if (defender_figani_iter == *(uint8 *)defender_figani) {
                        defender_figani_iter = 0;
                    }
                }
                if (shake_idx != 0) {
                    shake_idx = shake_idx - 1;
                }
                sprite_palette_color = 0xFFFFFFFF;
                fd2_wait_n_bios_ticks(1);
            }

            if ((data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 1)
                && (hit_count_so_far == total_hit_frames)) {
                return 1;
            }
        }

        if (defender_HP_after == 0) {
            hit_count = 0;
        }
        if ((hit_count != 0) && (*(int8 *)(attacker_figani + 1) == 1)) {
            memset((void *)workspace, 0, 0x1F400);
            if (rt_chars[attacker_idx].team == 0) {
                fd2_blit_rectangle(workspace + 0x140, 0x280, framebuffer, 0x140,
                                   0x140, 0xC8);
                fd2_blit_indexed_sprite(defender_figani, 0,
                                        (int)(workspace + 0x140), 0x280, -1);
                fd2_animate_bg_zoom_transition_in(
                    attacker_idx, attacker_figani, framebuffer, workspace,
                    data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr);
            } else {
                fd2_blit_rectangle(workspace, 0x280, framebuffer, 0x140, 0x140,
                                   0xC8);
                fd2_blit_indexed_sprite(defender_figani, 0, (int)workspace,
                                        0x280, -1);
                fd2_animate_bg_zoom_transition_out(
                    attacker_idx, attacker_figani, name_banner_sprite,
                    framebuffer, workspace,
                    data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr);
            }
        }
    } while (1);
}
