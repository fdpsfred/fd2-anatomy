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
 * Owned global data (definition; extern in globals.h).
 *
 * data_fd2_battle_scripted_cinematic_mode_or_terrain_idx @ 0x540FF
 *   Dual-purpose scripted-cinematic mode flag / forced terrain-index
 *   latch for the full combat cinematic. Zero in the normal battle path.
 *   The writers store a non-zero value before the cinematic reads it,
 *   then it is latched to 1: fd2_play_full_combat_cinematic here,
 *   fd2_play_game_ending_cinematic in aniend.c (stores the per-duel
 *   scripted-outcome table value before each credit-roll cinematic), and
 *   the ch25 scripted event. When non-zero, scripted mode is ON and the
 *   same value is reused as the forced spotlight / split-bg terrain
 *   index here. Readers also use non-zero as a mute / scripted-outcome
 *   gate: fd2_play_sfx_with_handle and fd2_play_sfx_sample_from_bank in
 *   audio.c silence SFX (their comments name it tutorial_mode_flag), and
 *   fd2_execute_combat_hit_cinematic forces the hit-outcome rolls to 0.
 *   Accessed as a full 32-bit word at every site; zero-initialized
 *   (.bss).
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_scripted_cinematic_mode_or_terrain_idx;

/* ----------------------------------------------------------------
 * data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr @ 0x54103
 *   Heap pointer to the "B" split-screen background RLE buffer used
 *   by the full combat cinematic when the defender is counter-capable
 *   (separate idle FIGANI present). Owner/writer:
 *   fd2_play_full_combat_cinematic here -- zeroed first, then assigned
 *   the malloc-backed result of fd2_load_dat_resource(BG.DAT), swapped
 *   with the spotlight background when the attacker is enemy-team, blit
 *   to the framebuffer, and finally freed. Also read by
 *   fd2_execute_combat_hit_cinematic (passed to the bg zoom
 *   transitions). Accessed as a full 32-bit pointer at every site;
 *   zero-initialized at rest (.bss), populated only at runtime.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr;

/* ----------------------------------------------------------------
 * data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr @ 0x54107
 *   Heap pointer to the terrain-backdrop ("spotlight" background) RLE
 *   buffer for the combat/character cinematics. Writers/readers here:
 *   fd2_play_figani_char_intro_animation and fd2_play_full_combat_cinematic
 *   each zero it first, then store the malloc-backed result of
 *   fd2_load_dat_resource(BG.DAT) into it, RLE-blit it as the spotlight
 *   background, and free it on cleanup; fd2_play_full_combat_cinematic also
 *   swaps it with the "B" split background (0x54103) when the attacker is
 *   enemy-team. Also read by fd2_execute_combat_hit_cinematic (passed to the
 *   bg zoom transitions). Accessed as a full 32-bit pointer at every site;
 *   zero-initialized at rest (.bss), populated only at runtime.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr;

/* ----------------------------------------------------------------
 * data_fd2_battle_special_cinematic_bg_layers[3] @ 0x5410B/0F/13
 *   Three heap pointers to the parallax background sub-layers (layer 0 @
 *   0x5410B, 1 @ 0x5410F, 2 @ 0x54113) of the special-attack / class-promotion
 *   full-screen cinematics. The BG zoom/scroll readers index them as a
 *   uint32[3] (e.g. bg_layers[frame % 3]); a real array is used so C
 *   guarantees the ascending adjacency the indexing relies on. Separate
 *   tentative scalars do NOT guarantee it -- Watcom lays BSS/COMDEF objects in
 *   reverse definition order, which inverts slots [1]/[2] and feeds garbage
 *   pointers to the RLE blitter (the special-attack cinematic crash).
 *   Writers/readers: fd2_play_full_combat_cinematic here and
 *   fd2_execute_special_attack_skill (spellcin.c), plus
 *   fd2_play_class_promotion_cinematic and the two BG zoom transitions (anispell.c)
 *   -- each zeroes a slot, stores the malloc-backed result of
 *   fd2_load_dat_resource(BG.DAT, ..., n) into it, RLE-blits it as a parallax
 *   backdrop, and frees it on cleanup. Zero-initialized at rest (.bss),
 *   populated only at runtime.
 * ---------------------------------------------------------------- */
uint32 data_fd2_battle_special_cinematic_bg_layers[3];

/* ----------------------------------------------------------------
 * data_fd2_audio_figani_sfx_bank_buf_ptr @ 0x54117
 *   Heap pointer to the SFX handle bank extracted from a FIGANI
 *   animation byte stream. The three cinematic players that drive
 *   FIGANI overlays each store this the same way:
 *   fd2_play_figani_char_intro_animation and
 *   fd2_play_full_combat_cinematic here, and
 *   fd2_execute_special_attack_skill (special-attack cinematic). Every
 *   writer assigns it the return of fd2_load_figani_sfx_bank(figani_buf)
 *   before any read, passes it by value to fd2_play_sfx_with_handle(bank,
 *   sfx_id, 1) on per-pose SFX hooks, then on cleanup calls
 *   fd2_play_sfx_with_handle(bank, -1, 1) to stop all SFX and frees it
 *   when non-NULL. Accessed as a full 32-bit pointer (MOV dword) at every
 *   site, never indexed locally; zero-initialized at rest (.bss),
 *   populated only at runtime.
 * ---------------------------------------------------------------- */
uint32 data_fd2_audio_figani_sfx_bank_buf_ptr;

/* ----------------------------------------------------------------
 * data_fd2_audio_figani_sfx_bank_defender_buf_ptr @ 0x5411B
 *   Defender-side counterpart of data_fd2_audio_figani_sfx_bank_buf_ptr
 *   (0x54117): the SFX handle bank loaded for the DEFENDER's FIGANI
 *   animation stream so the counter-attack pose can play its own sound
 *   effects. Sole owner/writer: fd2_play_full_combat_cinematic here. The
 *   assignment (return of fd2_load_figani_sfx_bank(def_anim_figani)) runs
 *   only on the non-scripted path; the value is then passed by value to
 *   fd2_execute_combat_hit_cinematic for the swapped-role counter
 *   cinematic on BOTH the normal-counter and the scripted forced-counter
 *   paths (in scripted mode it was never assigned, so it passes NULL,
 *   which the muted scripted-mode audio path ignores). On cleanup it is
 *   freed when non-NULL. Accessed as a full 32-bit pointer (MOV dword) at
 *   every site, never indexed; zero-initialized at rest (.bss), populated
 *   only at runtime.
 * ---------------------------------------------------------------- */
uint32 data_fd2_audio_figani_sfx_bank_defender_buf_ptr;

/* ----------------------------------------------------------------
 * fd2_display_cinematic_image_with_fade @ 0x1F73F  (1 caller)
 *
 * Two-stage cinematic image display with palette transitions, used by the
 * title-screen attract / credit-roll sequence.
 *
 * Stage 1 -- full-screen FDOTHER.DAT image:
 *   fade current screen to black, clear the 64000-byte framebuffer at
 *   0xA0000, load FDOTHER.DAT[palette_idx] palette into
 *   data_fd2_vga_palette_data_ptr, load FDOTHER.DAT[image1_idx] sprite,
 *   RLE-blit it full-screen (320 stride) to 0xA0000, fade in to reveal
 *   the blit, hold for 1 + 6 BIOS ticks (~385ms), then fade to black.
 *
 * Stage 2 -- reveal one screenful out of the caller's scroll panel:
 *   load FDOTHER.DAT[0x65] (final-clear notice) palette, then blit a full
 *   320x200 window from (src_buf + row_idx*320) with src stride 320 into
 *   0xA0000, and fade in to reveal it. The window is the caller's panel
 *   buffer scrolled to start row row_idx.
 *
 * Params: image1_idx = stage-1 image idx (FDOTHER.DAT entry),
 *   palette_idx = stage-1 palette idx,
 *   src_buf = stage-2 SOURCE BUFFER BASE pointer -- the caller's malloc'd
 *     scroll-panel buffer, NOT an x offset,
 *   row_idx = stage-2 source start row (multiplied by the 320 stride to
 *     index into src_buf, i.e. the panel scroll position).
 *
 * Sole caller: fd2_title_attract_and_main_menu @ 0x1FBAF (two scroll-loop
 *   sites: row 0x1C2 with image 0x64 / palette 99, and row 0x0A with image
 *   0x4B / palette 0x4C).
 * ---------------------------------------------------------------- */
void fd2_display_cinematic_image_with_fade(uint32 image1_idx, uint32 palette_idx,
                                           uint32 src_buf, int32 row_idx)
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
    stage2_src = (uint32)row_idx * 0x140 + src_buf;
    fd2_blit_rectangle(0xA0000, 0x140, stage2_src, 0x140, 0x140, 0xC8);
    fd2_play_palette_fade_in();
}

/* ----------------------------------------------------------------
 * fd2_play_figani_char_intro_animation @ 0x28784  (1 caller)
 *
 * Plays the FIGANI character intro animation (full-screen pose with
 * name banner) for the runtime char given by char_idx. The sole live
 * trigger is the long-range branch of fd2_execute_ai_item_use: it is
 * the caster's spotlight pose shown just before a long-range item/spell
 * strike (char_idx is the caster).
 *
 * Setup: free large_game_state_buffer + data_fd2_battle_scene_snapshot, allocate
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
 * large_game_state_buffer (0x25680) and reload data_fd2_battle_scene_snapshot
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
    free((void *)data_fd2_battle_scene_snapshot);
    data_fd2_battle_scene_snapshot = 0;
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
    data_fd2_battle_scene_snapshot =
        fd2_load_dat_resource(
            (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
            data_fd2_battle_scene_snapshot,
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
 * Pick the "spotlight" char (the enemy-side combatant) and the "terrain"
 * char (the other one): if the attacker is enemy-team (team==0) the
 * attacker is spotlight, else the defender is -- so the spotlight always
 * resolves to the enemy unit. For each, derive a terrain
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
void fd2_play_full_combat_cinematic(uint32 attacker_idx, uint32 defender_idx)
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
    p_attacker = &data_fd2_battle_runtime_char_array_ptr[attacker_idx];
    p_defender = &data_fd2_battle_runtime_char_array_ptr[defender_idx];
    defender_portrait = p_defender->portrait_id;
    attacker_portrait = p_attacker->portrait_id;
    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0) {
        free((void *)data_fd2_portrait_sprite_cache);
        free((void *)data_fd2_large_game_state_buffer_ptr);
        free((void *)data_fd2_battle_scene_snapshot);
        data_fd2_battle_scene_snapshot = 0;
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
        fd2_flash_char_hit_sprite((uint32)dst, attacker_idx);
    }
    if ((fd2_check_can_counter_attack(attacker_idx, defender_idx) == 1) ||
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
            fd2_flash_char_hit_sprite((uint32)dst, defender_idx);
        }
    } else {
        uint32 swap_tmp;

        data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr = 0;
        data_fd2_battle_special_cinematic_bg_layers[0] = 0;
        data_fd2_battle_special_cinematic_bg_layers[1] = 0;
        data_fd2_battle_special_cinematic_bg_layers[2] = 0;
        data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_bg_dat_52381, 0,
                defender_terrain);
        data_fd2_battle_special_cinematic_bg_layers[0] =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_bg_dat_52381,
                data_fd2_battle_special_cinematic_bg_layers[0], 0);
        data_fd2_battle_special_cinematic_bg_layers[1] =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_bg_dat_52381,
                data_fd2_battle_special_cinematic_bg_layers[1], 1);
        data_fd2_battle_special_cinematic_bg_layers[2] =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_bg_dat_52381,
                data_fd2_battle_special_cinematic_bg_layers[2], 2);
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
    fd2_play_char_intro_zoom_anim(attacker_idx, split_screen_flag, att_silhouette,
                                  def_silhouette, (uint32)workbuf, (uint32)dst,
                                  banner_rle);
    if (fd2_execute_combat_hit_cinematic(
            attacker_idx, defender_idx, att_anim_figani, def_silhouette,
            (uint32)workbuf, (uint32)dst, banner_rle,
            data_fd2_audio_figani_sfx_bank_buf_ptr) != 0) {
        if ((fd2_check_can_counter_attack(attacker_idx, defender_idx) == 1) &&
            (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0)) {
            fd2_execute_combat_hit_cinematic(
                defender_idx, attacker_idx, def_anim_figani, att_silhouette,
                (uint32)workbuf, (uint32)dst, banner_rle,
                data_fd2_audio_figani_sfx_bank_defender_buf_ptr);
        }
    }
    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx != 0) {
        data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 1;
        fd2_execute_combat_hit_cinematic(
            defender_idx, attacker_idx, def_anim_figani, att_silhouette,
            (uint32)workbuf, (uint32)dst, banner_rle,
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
        free((void *)data_fd2_battle_special_cinematic_bg_layers[0]);
        free((void *)data_fd2_battle_special_cinematic_bg_layers[1]);
        def_anim_figani = data_fd2_battle_special_cinematic_bg_layers[2];
    }
    free((void *)def_anim_figani);

    if (data_fd2_battle_scripted_cinematic_mode_or_terrain_idx == 0) {
        data_fd2_large_game_state_buffer_ptr = (uint32)malloc(0x25680);
        data_fd2_battle_scene_snapshot =
            fd2_load_dat_resource(
                (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
                data_fd2_battle_scene_snapshot,
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
 * from 0x30 down to 0; the loop counter runs 8..0, so max = 8*6 = 0x30).
 * Drives the "character sweeps onto the screen" intro before the per-hit
 * FIGANI frames play.
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
 * Positional args (consistent across all 6 callers):
 *   char_unit_id  unit index -> runtime_char.team selects top/bottom half
 *   mode_flag     0 = draw static char layer, nonzero = skip it
 *   char_sprite   sliding overlay sprite (blitted every frame at the offset)
 *   char_sprite2  static character sprite (blitted at the fixed origin)
 *   workspace     composite work buffer (0x280-stride slide base)
 *   bg_sprite     clear source + final-settle RLE destination (0x140 stride)
 *   weapon_sprite RLE backdrop sprite blitted at (0xA4, 0x9D) -- despite the
 *                 name this is NEVER a weapon; every caller passes a
 *                 TAI.DAT / FDSHAP.DAT name-banner / character-base sprite.
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
                        fd2_delay_ms(0x14);
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
                        fd2_delay_ms(0x14);
                        outp(0x3C8, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                        fd2_delay_ms(0x28);
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

/* ----------------------------------------------------------------
 * fd2_play_figani_animation_loop @ 0x2B659  (3 callers)
 *
 * Master FIGANI animation player — iterates a FIGANI byte stream pose by
 * pose, performing per-pose SFX hooks, MP deductions, character composite,
 * and palette FX. Heart of every special-attack + summon visual sequence.
 *
 * Callers: fd2_execute_special_attack_skill @ 0x276EC,
 *   fd2_execute_summon_spell_cast @ 0x27FC9,
 *   fd2_play_spell_cast_sequence @ 0x2A6BD.
 *
 * Arguments (mapped from the three call sites; the Ghidra decompiler's
 * formal names are scrambled, so names here follow the actual pushed args):
 *   caster_idx    runtime_char index of the casting unit (selects team)
 *   spell_id      spell / technique id (drives all the branch gates)
 *   caster_figani caster animation byte stream — the per-pose loop driver
 *   target_figani target pose animation byte stream (the wrapping pose)
 *   workspace     128 KB composite work buffer (0x140-stride blit target)
 *   dst_buf       64 KB framebuffer-sized scratch (restore source)
 *   bg_layer_a    background sprite stream for the palette-remap RLE blit
 *   bg_layer_b    second background sprite stream for the palette-remap blit
 *
 * FIGANI byte-stream layout (caster_figani):
 *   byte +2 = pose count; per-pose table at +8 (pose_idx*4 -> relative
 *   offset). Per-pose entry: +4 type marker (1 = spell-cast frame),
 *   +5 SFX hook idx (0 = none), +6 sub-frame count.
 *
 * remap_idx selects a palette-remap table from data_fd2_tile_anim_table_base:
 *   spell_id in {8, 0x20, 0x21}       -> 0x13
 *   spell_id > 3 or spell_id == 0x23  -> 0x0F
 *   otherwise                         -> 0x0B
 *
 * Per pose:
 *   - special-skill SFX hook: when spell_id is 0x18 or in 0x1C..0x1E and the
 *     pose carries an SFX hook (+5 != 0), play it from the figani SFX bank.
 *   - spell-cast frame (type +4 == 1): deduct caster MP, flash the caster
 *     hit sprite, and (when spell_id < 10 or > 31) arm a 6-subframe palette
 *     write, RLE-blit the two background layers with the remap table, and
 *     fire the summon cast SFX.
 *   - per sub-frame (+6 count): restore dst_buf into workspace, composite
 *     the caster/target poses (order keyed on caster team and spell_id),
 *     flush workspace to VGA, optionally paint palette index 0 from the
 *     per-spell RGB flash table for the armed countdown, advance the target
 *     FIGANI counters, and wait one BIOS tick.
 *
 * NOTE: every CALL in this body returns into a discarded value (the loop
 * uses memory loads, not call return values), so there is no EAX-tracking
 * hazard here.
 * ---------------------------------------------------------------- */
void fd2_play_figani_animation_loop(uint32 caster_idx, uint32 spell_id,
                                    uint8 *caster_figani, uint8 *target_figani,
                                    uint32 workspace, uint32 dst_buf,
                                    uint32 bg_layer_a, uint32 bg_layer_b)
{
    uint32 remap_idx;
    uint32 pose_iter;
    uint32 subframe_iter;
    uint32 palette_write_countdown;
    uint32 pose_entry;
    uint32 remap_table;
    uint8  target_pose_idx;
    uint8  subframe_in_target;

    subframe_in_target = 0;
    target_pose_idx = 0;
    palette_write_countdown = 0;
    remap_idx = 0x0B;
    if ((spell_id == 8) || (spell_id == 0x20) || (spell_id == 0x21)) {
        remap_idx = 0x13;
    } else if (((int32)spell_id > 3) || (spell_id == 0x23)) {
        remap_idx = 0x0F;
    }

    for (pose_iter = 0;
         (int32)pose_iter < (int32)(uint32)caster_figani[2];
         pose_iter = pose_iter + 1) {
        pose_entry = (uint32)caster_figani
                     + *(int32 *)(caster_figani + pose_iter * 4 + 8);

        if (((spell_id == 0x18) ||
             (((int32)spell_id > 0x1B) && ((int32)spell_id < 0x1F))) &&
            (*(int8 *)(pose_entry + 5) != 0)) {
            fd2_play_sfx_with_handle(data_fd2_audio_figani_sfx_bank_buf_ptr,
                                     *(uint8 *)(pose_entry + 5), 1);
        }

        if (*(int8 *)(pose_entry + 4) == 1) {
            fd2_deduct_caster_mp(caster_idx, spell_id);
            fd2_flash_char_hit_sprite((uint32)dst_buf, caster_idx);
            if (((int32)spell_id < 10) || ((int32)spell_id > 31)) {
                palette_write_countdown = 6;
                remap_table = data_fd2_tile_anim_table_base
                    + *(int32 *)(data_fd2_tile_anim_table_base + remap_idx * 4
                                 + 6);
                fd2_rle_blit_with_palette_remap((uint16 *)bg_layer_a, 0, 0x32,
                                                (int32)dst_buf, 0x140,
                                                (int32)remap_table);
                fd2_rle_blit_with_palette_remap((uint16 *)bg_layer_b, 0xA4,
                                                0x9D, (int32)dst_buf, 0x140,
                                                (int32)remap_table);
                fd2_play_sfx_with_handle(
                    data_fd2_audio_summon_spell_sfx_bank_buf_ptr, 0, 1);
            }
        }

        for (subframe_iter = 0;
             (int32)subframe_iter < (int32)(uint32) * (uint8 *)(pose_entry + 6);
             subframe_iter = subframe_iter + 1) {
            fd2_blit_rectangle(workspace, 0x140, (uint32)dst_buf, 0x140, 0x140,
                               0xC8);
            if (data_fd2_battle_runtime_char_array_ptr[caster_idx].team == 0) {
                if (((int32)spell_id < 10) || (spell_id == 0x1C)) {
                    fd2_blit_indexed_sprite((uint32)target_figani,
                                            (uint32)target_pose_idx,
                                            (int)workspace, 0x140, -1);
                }
                fd2_blit_indexed_sprite((uint32)caster_figani, pose_iter,
                                        (int)workspace, 0x140, -1);
            } else {
                fd2_blit_indexed_sprite((uint32)caster_figani, pose_iter,
                                        (int)workspace, 0x140, -1);
                if (((int32)spell_id < 10) || (spell_id == 0x1C)) {
                    fd2_blit_indexed_sprite((uint32)target_figani,
                                            (uint32)target_pose_idx,
                                            (int)workspace, 0x140, -1);
                }
            }
            fd2_blit_rectangle(0xA0000, 0x140, workspace, 0x140, 0x140, 0xC8);

            if (palette_write_countdown != 0) {
                outp(0x3C8, 0);
                outp(0x3C9,
                     data_fd2_animation_spell_palette_flash_table[spell_id]);
                outp(0x3C9,
                     data_fd2_animation_spell_palette_flash_table[spell_id
                                                                  + 0x24]);
                outp(0x3C9,
                     data_fd2_animation_spell_palette_flash_table[spell_id
                                                                  + 0x48]);
                fd2_delay_ms(0x1E);
                outp(0x3C8, 0);
                outp(0x3C9, 0);
                outp(0x3C9, 0);
                outp(0x3C9, 0);
                palette_write_countdown = palette_write_countdown - 1;
            }

            subframe_in_target = subframe_in_target + 1;
            if ((uint32)subframe_in_target
                == *(uint8 *)((uint32)target_figani + 6
                              + *(int32 *)((uint32)target_figani
                                           + (uint32)target_pose_idx * 4 + 8))) {
                subframe_in_target = 0;
                target_pose_idx = target_pose_idx + 1;
                if (target_pose_idx == *(uint8 *)target_figani) {
                    target_pose_idx = 0;
                }
            }
            fd2_wait_n_bios_ticks(1);
        }
    }
}

/* FIGANI pose-loop sub-frame counter @ 0x540FC. Runtime state for
 * fd2_step_figani_pose_animation: current sub-frame within the active pose.
 * Zero-initialized; the stepper writes it (reset path stores 0) before any
 * read. Accessed only as a byte and read zero-extended -> unsigned 8-bit. */
uint8 data_fd2_graphics_figani_pose_anim_subframe_idx;

/* FIGANI pose-loop pose counter @ 0x540FD. Runtime state for
 * fd2_step_figani_pose_animation: current pose index (0..figani[+0]-1),
 * used to index the per-pose metadata table at figani+8+idx*4. Zero-
 * initialized; the stepper writes it (INC and reset-store 0) and reads it
 * only as a byte, zero-extended -> unsigned 8-bit. */
uint8 data_fd2_graphics_figani_pose_anim_pose_idx;

/* ----------------------------------------------------------------
 * fd2_step_figani_pose_animation @ 0x2B9A1  (3 callers)
 *
 * Per-frame state stepper for a free-running FIGANI pose-loop. Maintains
 * two module-global byte counters that walk forward through pose x
 * sub-frame, blitting the current pose into dst_buf and auto-wrapping back
 * to the start of the loop once the last pose finishes.
 *
 * Globals:
 *   data_fd2_graphics_figani_pose_anim_pose_idx     [0x540FD] current pose index
 *   data_fd2_graphics_figani_pose_anim_subframe_idx [0x540FC] current sub-frame within pose
 *
 * FIGANI stream layout used here:
 *   byte +0                       = pose count (the wrap bound)
 *   int32 +8 + pose_idx*4         = byte offset (relative to the stream) to pose's metadata block
 *   pose_block +6                 = that pose's sub-frame count
 *
 * Algorithm:
 *   palette_op != 0:
 *     blit the current pose, then advance the sub-frame counter.
 *     while still inside the pose (sub_frame < pose.sub_count) -> return.
 *     otherwise advance the pose counter; if still inside the loop
 *     (pose_idx < pose_count) reset the sub-frame counter and return.
 *   palette_op == 0  OR  the pose counter ran past the last pose:
 *     reset both counters to 0 (rewind to the start of the loop). palette_op
 *     == 0 is an explicit "rewind" call used between cinematic phases.
 *
 * palette_op is forwarded straight to fd2_blit_indexed_sprite as its blit
 * mode (0xFFFFFFFF = passthrough, > 0xFF = translucent, <= 0xFF = silhouette).
 *
 * Callers: fd2_execute_special_attack_skill @ 0x276EC,
 *   fd2_play_final_chapter_30_ending @ 0x2C405,
 *   fd2_play_spell_cast_sequence @ 0x2A6BD.
 * System = graphics (FIGANI pose-loop state machine; pose x sub-frame walk
 * with auto-reset).
 * ---------------------------------------------------------------- */
void fd2_step_figani_pose_animation(uint32 figani_data, uint32 palette_op,
                                    uint32 dst_buf, uint32 dst_stride)
{
    if (palette_op != 0) {
        fd2_blit_indexed_sprite(figani_data,
                                data_fd2_graphics_figani_pose_anim_pose_idx,
                                dst_buf, (int)dst_stride, palette_op);
        data_fd2_graphics_figani_pose_anim_subframe_idx =
            data_fd2_graphics_figani_pose_anim_subframe_idx + 1;
        if (data_fd2_graphics_figani_pose_anim_subframe_idx <
            *(uint8 *)(figani_data + 6 +
                       *(int32 *)(figani_data + 8 +
                                  (uint32)data_fd2_graphics_figani_pose_anim_pose_idx * 4))) {
            return;
        }
        data_fd2_graphics_figani_pose_anim_pose_idx =
            data_fd2_graphics_figani_pose_anim_pose_idx + 1;
        if (data_fd2_graphics_figani_pose_anim_pose_idx < *(uint8 *)figani_data) {
            data_fd2_graphics_figani_pose_anim_subframe_idx = 0;
            return;
        }
    }
    data_fd2_graphics_figani_pose_anim_pose_idx = 0;
    data_fd2_graphics_figani_pose_anim_subframe_idx = 0;
}

/* ----------------------------------------------------------------
 * fd2_animate_spell_hit_cinematic @ 0x2BA22  (1 caller)
 *
 * Spell-HIT cinematic sub-loop: a 9-frame zoom that brackets one target's
 * hit moment inside the basic-spell cast sequence. Phase 1 (frame 1..4)
 * slides the CASTER sprite in; Phase 2 (frame 4..0) slides the HIT-EFFECT
 * sprite back out, reversing the motion. Each frame composites the static
 * background, fires the per-element palette-flash handler before and after
 * the sprite blits, pushes the workspace to the mode13h primary at 0xA0000,
 * and waits one BIOS tick.
 *
 * Setup:
 *   workspace_ptr = base_workspace_offset + 0x49C0   // composite scratch base
 *   team_dir_sign = runtime_char[attacker_idx].team == 0 ? +1 : -1
 *       // enemy casters (team 0) slide in from the right (+); player from left
 *   pose_count    = *(uint8 *)spell_sprite_atlas     // pose total of the spell anim
 *   flicker_toggle = 0   // shared across BOTH phases (Phase 2 keeps Phase 1's value)
 *
 * Per-frame body (identical in both phases except the slide sprite + direction):
 *   1. background composite into flicker_dst:
 *        electric/lightning spells (spell_type_idx 3 or 7): flicker_toggle ^= 1,
 *          flicker_dst = workspace_ptr - flicker_toggle*0x280 (1-row vertical jitter);
 *        else flicker_dst = workspace_ptr.
 *        fd2_blit_rectangle(flicker_dst, 0x280, bg_workbuf, 0x140, 0x140, 0xC8)
 *   2. dispatch[spell_type_idx](attacker_idx, dispatch_sprite_atlas,
 *                               workspace_ptr, 0x280, 4)   // pre-blit palette flash
 *   3. blit spell top-half pose:
 *        fd2_blit_indexed_sprite(spell_sprite_atlas, pose_count-1, workspace_ptr, 0x280, -1)
 *   4. blit the sliding sprite at frame*0x23 px/frame:
 *        Phase 1: caster_sprite_atlas; Phase 2: hit_effect_sprite
 *        fd2_blit_indexed_sprite(slide_sprite, 0,
 *                                frame*0x23*team_dir_sign + workspace_ptr, 0x280, -1)
 *   5. dispatch[spell_type_idx](attacker_idx, dispatch_sprite_atlas,
 *                               workspace_ptr, 0x280, 5)   // post-blit palette flash
 *   6. fd2_blit_rectangle(0xA0000, 0x140, workspace_ptr, 0x280, 0x140, 0xC8)  // push to VGA
 *   7. fd2_wait_n_bios_ticks(1)
 *
 * The dispatch is data_fd2_battle_spell_cast_cinematic_phase_handler_table
 * (the 10-entry summon-spell tick table @ 0x523B9), indexed by spell_type_idx;
 * each entry takes (sprite_handle, sprite_atlas, dst, stride, phase_code) and
 * returns an int frame count which is IGNORED here (the cinematic frame count
 * is fixed at 4+5). The Ghidra decompiler drops the five pushed arguments and
 * the return for these indirect calls; the assembly (PUSH x5 ; ADD ESP,0x14)
 * is authoritative.
 *
 * Globals: data_fd2_battle_runtime_char_array_ptr (team read);
 *   data_fd2_battle_spell_cast_cinematic_phase_handler_table (palette flash dispatch).
 *
 * Sole caller: fd2_play_spell_cast_sequence @ 0x2A6BD (basic-spell path, between
 * consecutive targets of an AoE cast).
 * System = battle (spell-hit cinematic 9-frame zoom; per-element palette flash).
 * ---------------------------------------------------------------- */
void fd2_animate_spell_hit_cinematic(uint32 attacker_idx, uint32 dispatch_sprite_atlas,
                                     uint32 spell_sprite_atlas, int caster_sprite_atlas,
                                     uint32 base_workspace_offset, uint32 bg_workbuf,
                                     int hit_effect_sprite, int spell_type_idx)
{
    uint32 workspace_ptr;
    int    team_dir_sign;
    uint8  flicker_toggle;
    uint8  pose_count;
    uint32 flicker_dst;
    int    frame;

    workspace_ptr = base_workspace_offset + 0x49C0;
    team_dir_sign = -1;
    flicker_toggle = 0;
    pose_count = *(uint8 *)spell_sprite_atlas;
    if (data_fd2_battle_runtime_char_array_ptr[attacker_idx].team == 0) {
        team_dir_sign = 1;
    }

    /* Phase 1 — zoom-IN, caster sprite sliding in (frame 1..4). */
    for (frame = 1; frame < 5; frame++) {
        if (spell_type_idx == 7 || spell_type_idx == 3) {
            flicker_toggle = (uint8)(flicker_toggle ^ 1);
            flicker_dst = workspace_ptr - (uint32)flicker_toggle * 0x280;
        } else {
            flicker_dst = workspace_ptr;
        }
        fd2_blit_rectangle(flicker_dst, 0x280, (uint32)bg_workbuf, 0x140, 0x140, 0xC8);
        data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_type_idx](
            attacker_idx, dispatch_sprite_atlas, workspace_ptr, 0x280, 4);
        fd2_blit_indexed_sprite(spell_sprite_atlas, (uint32)(pose_count - 1),
                                (int)workspace_ptr, 0x280, -1);
        fd2_blit_indexed_sprite((uint32)caster_sprite_atlas, 0,
                                frame * 0x23 * team_dir_sign + (int)workspace_ptr,
                                0x280, -1);
        data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_type_idx](
            attacker_idx, dispatch_sprite_atlas, workspace_ptr, 0x280, 5);
        fd2_blit_rectangle(0xA0000, 0x140, workspace_ptr, 0x280, 0x140, 0xC8);
        fd2_wait_n_bios_ticks(1);
    }

    /* Phase 2 — zoom-OUT, hit-effect sprite sliding out (frame 4..0). */
    for (frame = 4; -1 < frame; frame--) {
        if (spell_type_idx == 7 || spell_type_idx == 3) {
            flicker_toggle = (uint8)(flicker_toggle ^ 1);
            flicker_dst = workspace_ptr - (uint32)flicker_toggle * 0x280;
        } else {
            flicker_dst = workspace_ptr;
        }
        fd2_blit_rectangle(flicker_dst, 0x280, (uint32)bg_workbuf, 0x140, 0x140, 0xC8);
        data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_type_idx](
            attacker_idx, dispatch_sprite_atlas, workspace_ptr, 0x280, 4);
        fd2_blit_indexed_sprite(spell_sprite_atlas, (uint32)(pose_count - 1),
                                (int)workspace_ptr, 0x280, -1);
        fd2_blit_indexed_sprite((uint32)hit_effect_sprite, 0,
                                frame * 0x23 * team_dir_sign + (int)workspace_ptr,
                                0x280, -1);
        data_fd2_battle_spell_cast_cinematic_phase_handler_table[spell_type_idx](
            attacker_idx, dispatch_sprite_atlas, workspace_ptr, 0x280, 5);
        fd2_blit_rectangle(0xA0000, 0x140, workspace_ptr, 0x280, 0x140, 0xC8);
        fd2_wait_n_bios_ticks(1);
    }
}
