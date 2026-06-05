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
