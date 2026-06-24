/*
 * anicombt.c — battle combat-hit / spell-effect overlay animations.
 *
 * Visual feedback played when an attack lands, a spell resolves, or a
 * status effect is inflicted/cured. These routines snapshot the battle
 * back-buffer, draw an overlay, then flicker between the snapshot and the
 * modified frame before restoring the snapshot.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * fd2_animate_status_effect_overlay_flicker @ 0x1C2DA (11 callers)
 *
 * "Status effect applied" full-screen flicker animation. Draws a 24x24
 * status sprite over every targeted char that is inside the battle view
 * window, then flickers the modified battle frame against an unmodified
 * snapshot 5 times, finally leaving the snapshot on screen.
 *
 * Parameters (__cdecl, 4 args; caster_idx only forwarded to the stack check):
 *   caster_idx         unused by the body
 *   status_kind        selects the silhouette colour tmp_copy[status_kind]
 *   target_count       number of entries in char_idx_array
 *   char_idx_array     byte array of runtime-char indices to overlay
 *
 * The colour table at 0x51F15 is copied into a 30-byte stack scratch
 * (7 dwords + 1 word in the original, matched here byte-for-byte) before
 * indexing by status_kind.
 * ---------------------------------------------------------------- */
void fd2_animate_status_effect_overlay_flicker(uint32 caster_idx, uint32 status_kind,
                                               uint32 target_count,
                                               uint32 char_idx_array)
{
    uint8 *backup_buf;
    runtime_char *rt_char;
    int iVar3;
    uint32 pos_x;
    uint32 pos_y;
    uint32 frame_off;
    uint32 frame_idx;
    uint32 src_sprite;
    uint32 dst_addr;
    uint8 tmp_copy[32];

    (void)caster_idx;

    /* snapshot the status-effect colour template (7 dwords + 1 word = 30B) */
    memcpy(tmp_copy, data_fd2_animation_status_overlay_flicker_color_template, 30);

    fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr, 1, 1);

    backup_buf = (uint8 *)malloc(0x25680);
    memmove(backup_buf, (void *)data_fd2_large_game_state_buffer_ptr, 0x25680);

    /* draw the per-char status sprite overlay onto the live back-buffer */
    for (iVar3 = 0; iVar3 < (int)target_count; iVar3++) {
        rt_char = (runtime_char *)((uint32)data_fd2_battle_runtime_char_array_ptr +
                                   ((uint8 *)char_idx_array)[iVar3] * 0x50);
        pos_x = rt_char->pos_x;
        pos_y = rt_char->pos_y;

        if (((int)pos_x < (int)(data_fd2_battle_view_window_origin_x - 1)) ||
            ((int)pos_x > (int)(data_fd2_battle_view_window_origin_x +
                                data_fd2_battle_view_window_max_x)) ||
            ((int)pos_y < (int)(data_fd2_battle_view_window_origin_y - 1)) ||
            ((int)pos_y > (int)(data_fd2_battle_view_window_origin_y +
                                data_fd2_battle_view_window_max_y + 1))) {
            continue;
        }

        frame_off = (uint32)rt_char->sprite_state[0] * 0xc;
        if (data_fd2_graphics_chapter_ambient_palette_anim_idx == 3) {
            frame_idx = frame_off + 2;
        } else {
            frame_idx = frame_off + data_fd2_graphics_chapter_ambient_palette_anim_idx;
        }

        src_sprite = data_fd2_portrait_sprite_cache +
                     *(uint32 *)(data_fd2_portrait_sprite_cache + frame_idx * 4);
        dst_addr = data_fd2_large_game_state_buffer_ptr +
                   (pos_y - data_fd2_battle_view_window_origin_y) * 0x2ac0 +
                   (pos_x - data_fd2_battle_view_window_origin_x) * 0x18 + 0x75d8;

        fd2_tile_blit_24x24_solid_color(src_sprite, dst_addr, 0x1c8,
                                        tmp_copy[status_kind]);
    }

    /* flicker the modified frame against the snapshot 5 times */
    for (iVar3 = 0; iVar3 < 5; iVar3++) {
        fd2_blit_rectangle(0xa0504, 0x140, (uint32)backup_buf + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_wait_n_bios_ticks(1);
        fd2_blit_rectangle(0xa0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_wait_n_bios_ticks(1);
    }

    /* leave the unmodified snapshot on screen, then release it */
    fd2_blit_rectangle(0xa0504, 0x140, (uint32)backup_buf + 0x8088,
                       0x1c8, 0x138, 0xc0);
    free(backup_buf);
}

/* ----------------------------------------------------------------
 * fd2_animate_spell_impact_per_target @ 0x1C4CC (15 callers)
 *
 * Per-spell impact animation drawn over each target tile, with per-spell
 * SFX hook frames. The most-used spell visual effect. One of three
 * spell-impact play paths (paired with full-screen flash for high-tier
 * spells).
 *
 * Parameters (__cdecl, 4 args; caster_idx only forwarded to the stack check):
 *   caster_idx  unused by the body (callers pass caster_idx)
 *   spell_id           index 0..35 into the three per-spell byte tables
 *   target_count       number of entries in char_idx_array
 *   char_idx_array     byte array of runtime-char indices to overlay
 *
 * Three parallel per-spell byte tables (each copied into a 33-byte stack
 * scratch via 8-dword REP MOVSD + tail MOVSB, matched byte-for-byte):
 *   sprite_off_tbl   @ 0x51F33 — sprite-index offset (animation frame base)
 *   frame_count_tbl  @ 0x51F54 — total animation frame count
 *   sfx_frame_tbl    @ 0x51F75 — primary SFX id (0 = no SFX hook on frame 0)
 *
 * Pipeline:
 *   1. malloc a 0x25680 preserve buffer, snapshot the live back-buffer into
 *      it (aborts via printf + exit(1) if malloc fails).
 *   2. For each frame in 0..frame_count-1:
 *        a. restore the clean baseline back-buffer from the preserve buffer.
 *        b. sprite = portrait_sheet + portrait_sheet[6 + (sprite_off+frame)*4]
 *        c. for each target in window: blit the frame sprite at its tile.
 *        d. flush composite to mode13h primary.
 *        e. SFX hook: frame 0 fires sfx_frame_tbl[spell_id] (if non-zero);
 *           a per-spell dispatch chain fires extra SFX on specific frames.
 *        f. one BIOS-tick frame-timing pulse.
 *   3. free the preserve buffer; finalize with a composite.
 * ---------------------------------------------------------------- */
void fd2_animate_spell_impact_per_target(uint32 caster_idx, uint32 spell_id,
                                         uint32 target_count,
                                         uint32 char_idx_array)
{
    uint8 *backup_buf;
    runtime_char *rt_char;
    int frame_idx;
    int tgt_iter;
    uint32 frame_sprite_addr;
    uint32 pos_x;
    uint32 pos_y;
    uint8 sprite_off_tbl[33];
    uint8 sfx_frame_tbl[33];
    uint8 frame_count_tbl[33];

    (void)caster_idx;

    /* snapshot the three per-spell tables (8 dwords + 1 byte = 33B each) */
    memcpy(sprite_off_tbl, data_fd2_animation_spell_sprite_offset_table, 33);
    memcpy(frame_count_tbl, data_fd2_animation_spell_frame_count_table, 33);
    memcpy(sfx_frame_tbl, data_fd2_animation_spell_sfx_frame_table, 33);

    fd2_composite_battle_frame(0);

    backup_buf = (uint8 *)malloc(0x25680);
    if (backup_buf == 0) {
        printf("Out of memory at Get_EasyMagic \n");
        exit(1);
    }
    memmove(backup_buf, (void *)data_fd2_large_game_state_buffer_ptr, 0x25680);

    for (frame_idx = 0; frame_idx < (int)frame_count_tbl[spell_id]; frame_idx++) {
        frame_sprite_addr =
            data_fd2_resource_portrait_sheet_ptr +
            *(uint32 *)(data_fd2_resource_portrait_sheet_ptr + 6 +
                        ((uint32)sprite_off_tbl[spell_id] + frame_idx) * 4);

        /* restore the clean baseline back-buffer for this frame */
        memmove((void *)data_fd2_large_game_state_buffer_ptr, backup_buf, 0x25680);

        for (tgt_iter = 0; tgt_iter < (int)target_count; tgt_iter++) {
            rt_char = (runtime_char *)((uint32)data_fd2_battle_runtime_char_array_ptr +
                                       ((uint8 *)char_idx_array)[tgt_iter] * 0x50);
            pos_x = rt_char->pos_x;
            pos_y = rt_char->pos_y;

            if (((int)pos_x >= (int)(data_fd2_battle_view_window_origin_x - 1)) &&
                ((int)pos_x <= (int)(data_fd2_battle_view_window_origin_x +
                                     data_fd2_battle_view_window_max_x)) &&
                ((int)pos_y >= (int)(data_fd2_battle_view_window_origin_y - 1)) &&
                ((int)pos_y <= (int)(data_fd2_battle_view_window_origin_y +
                                     data_fd2_battle_view_window_max_y + 1))) {
                fd2_blit_sprite_with_decoded_pixels(
                    data_fd2_large_game_state_buffer_ptr +
                        (pos_y - data_fd2_battle_view_window_origin_y) * 0x2ac0 +
                        (pos_x - data_fd2_battle_view_window_origin_x) * 0x18 + 0x75d8,
                    frame_sprite_addr, 0x1c8);
            }
        }

        fd2_blit_rectangle(0xa0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1c8, 0x138, 0xc0);

        /* SFX hook: frame-0 table fire, plus per-spell special-case frames */
        if (frame_idx == 0 && sfx_frame_tbl[spell_id] != 0) {
            fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr,
                                     (int)sfx_frame_tbl[spell_id], 1);
        } else if (spell_id == 0x16 && frame_idx == 7) {
            fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr, 3, 1);
        } else if (spell_id == 0x19) {
            if (frame_idx == 3 || frame_idx == 6) {
                fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr, 5, 1);
            }
        } else if (spell_id == 0x12 && frame_idx == 4) {
            fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr, 7, 1);
        } else if (spell_id == 0x13) {
            if (frame_idx == 3 || frame_idx == 6) {
                fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr, 8, 1);
            }
        } else if (spell_id == 8) {
            if (frame_idx == 3 || frame_idx == 6) {
                fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr, 10, 1);
            }
        } else if (spell_id == 9) {
            if (frame_idx == 0xf || frame_idx == 0x13) {
                fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr, 0xf, 1);
            }
        }

        fd2_wait_n_bios_ticks(1);
    }

    free(backup_buf);
    fd2_composite_battle_frame(0);
}

/* ----------------------------------------------------------------
 * fd2_animate_spell_full_screen_flash @ 0x1CAC7 (2 callers)
 *
 * Two-buffer full-screen strobe flash. The "global glow" lead-in effect
 * for high-tier offensive spells, played before the per-target impact
 * animation. Renders two full-screen composites (a white-flash variant
 * and a colour-flash variant) into two separate buffers, then alternates
 * blitting them to the primary surface four times to produce a strobe.
 *
 * Parameters (__cdecl, 4 args; caster_idx and spell_id are only consumed by
 * the stack check, not by the body):
 *   caster_idx         unused by the body
 *   spell_id           unused by the body (the flash variant is fixed)
 *   target_count       number of entries in char_idx_array
 *   char_idx_array     byte array of runtime-char indices forwarded to the
 *                      spell-effect overlay compositor
 *
 * Pipeline:
 *   1. composite variant 0x4A into the live back-buffer (white flash).
 *   2. malloc a 0x25680 secondary buffer; composite variant 0x4B into it
 *      (colour flash).
 *   3. four times: blit A (+0x8088) -> primary, delay 0x5A; blit B
 *      (+0x8088) -> primary, delay 0x5A. (8 blits total, ~720ms strobe.)
 *   4. restore the battle frame; free the secondary buffer.
 *
 * The original tail-jumps into a shared epilogue (the secondary-buffer
 * pointer pushed for free() is discarded by that epilogue's ADD ESP,4);
 * this is reproduced here as a plain free() at function end.
 * ---------------------------------------------------------------- */
void fd2_animate_spell_full_screen_flash(uint32 caster_idx, uint32 spell_id,
                                         uint32 target_count,
                                         uint32 char_idx_array)
{
    uint8 *flash_buf;
    int iter;

    (void)caster_idx;
    (void)spell_id;

    /* variant-A (white flash) composite into the live back-buffer */
    fd2_composite_chars_with_spell_effect_overlay(
        data_fd2_large_game_state_buffer_ptr, target_count, char_idx_array, 0x4a);

    /* variant-B (colour flash) composite into a fresh secondary buffer */
    flash_buf = (uint8 *)malloc(0x25680);
    fd2_composite_chars_with_spell_effect_overlay(
        (uint32)flash_buf, target_count, char_idx_array, 0x4b);

    /* strobe: alternate A/B four times (8 blits, 8 delays) */
    for (iter = 0; iter < 4; iter++) {
        fd2_blit_rectangle(0xa0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_delay_ms(0x5a);
        fd2_blit_rectangle(0xa0504, 0x140, (uint32)flash_buf + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_delay_ms(0x5a);
    }

    fd2_composite_battle_frame(0);
    free(flash_buf);
}

/* ----------------------------------------------------------------
 * fd2_animate_spell_overlay_blink @ 0x1CD17 (3 callers)
 *
 * 10-frame "spell-hit mark fade-out" overlay animation. Triggered by the
 * caster's spell-hit animation chain (fd2_apply_use_effect_dispatch,
 * fd2_execute_offensive_targeted_spell[_variant_b]). Each frame restores a
 * clean snapshot of the battle back-buffer, redraws a tinted 24x24 mark over
 * every targeted char inside the battle view window, flushes the composite to
 * the mode13h primary, and waits one BIOS tick; the per-frame tint team-offset
 * steps 7..0 across the loop so the mark fades out.
 *
 * Parameters (__cdecl, 4 args; caster_idx only forwarded to the stack check):
 *   caster_idx         unused by the body (callers pass the caster char index)
 *   spell_id           index into the per-spell tint-mask byte table
 *   target_count       number of entries in char_idx_array
 *   char_idx_array     byte array of runtime-char indices to overlay
 *
 * The per-spell tint-mask byte table at 0x52006 is copied into a 32-byte stack
 * scratch (7 dwords + 1 word = 30 bytes, matched byte-for-byte) before indexing
 * by spell_id; the indexed byte becomes the blit colour_base anchor.
 *
 * The original tail-jumps into fd2_animate_status_effect_overlay_flicker's
 * shared epilogue (snapshot pointer pushed for free() then ADD ESP,4);
 * reproduced here as a plain free() at function end.
 * ---------------------------------------------------------------- */
void fd2_animate_spell_overlay_blink(uint32 caster_idx, uint32 spell_id,
                                     uint32 target_count,
                                     uint32 char_idx_array)
{
    uint8 *backup_buf;
    runtime_char *rt_char;
    int iVar3;
    int tgt_iter;
    uint32 pos_x;
    uint32 pos_y;
    uint32 frame_off;
    uint32 frame_idx;
    uint32 src_sprite;
    uint32 dst_addr;
    uint8 mask_tbl[32];

    (void)caster_idx;

    /* snapshot the per-spell tint-mask byte table (7 dwords + 1 word = 30B) */
    memcpy(mask_tbl, data_fd2_animation_spell_overlay_blink_mask_table, 30);

    backup_buf = (uint8 *)malloc(0x25680);
    memmove(backup_buf, (void *)data_fd2_large_game_state_buffer_ptr, 0x25680);

    for (iVar3 = 0; iVar3 < 10; iVar3++) {
        /* restore the clean baseline back-buffer for this frame */
        memmove((void *)data_fd2_large_game_state_buffer_ptr, backup_buf, 0x25680);

        for (tgt_iter = 0; tgt_iter < (int)target_count; tgt_iter++) {
            rt_char = (runtime_char *)((uint32)data_fd2_battle_runtime_char_array_ptr +
                                       ((uint8 *)char_idx_array)[tgt_iter] * 0x50);
            pos_x = rt_char->pos_x;
            pos_y = rt_char->pos_y;

            if (((int)pos_x >= (int)(data_fd2_battle_view_window_origin_x - 1)) &&
                ((int)pos_x <= (int)(data_fd2_battle_view_window_origin_x +
                                     data_fd2_battle_view_window_max_x)) &&
                ((int)pos_y >= (int)(data_fd2_battle_view_window_origin_y - 1)) &&
                ((int)pos_y <= (int)(data_fd2_battle_view_window_origin_y +
                                     data_fd2_battle_view_window_max_y + 1))) {
                frame_off = (uint32)rt_char->sprite_state[0] * 0xc;
                if (data_fd2_graphics_chapter_ambient_palette_anim_idx == 3) {
                    frame_idx = frame_off + 2;
                } else {
                    frame_idx = frame_off + data_fd2_graphics_chapter_ambient_palette_anim_idx;
                }

                src_sprite = data_fd2_portrait_sprite_cache +
                             *(uint32 *)(data_fd2_portrait_sprite_cache + frame_idx * 4);
                dst_addr = data_fd2_large_game_state_buffer_ptr +
                           (pos_y - data_fd2_battle_view_window_origin_y) * 0x2ac0 +
                           (pos_x - data_fd2_battle_view_window_origin_x) * 0x18 + 0x75d8;

                fd2_tile_blit_24x24_with_tint_offset(src_sprite, dst_addr, 0x1c8,
                                                     mask_tbl[spell_id],
                                                     7 - (iVar3 % 8));
            }
        }

        fd2_blit_rectangle(0xa0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_wait_n_bios_ticks(1);
    }

    /* leave the clean snapshot composite on screen, then release it */
    fd2_blit_rectangle(0xa0504, 0x140, (uint32)backup_buf + 0x8088,
                       0x1c8, 0x138, 0xc0);
    free(backup_buf);
}

/* ----------------------------------------------------------------
 * fd2_play_death_animation_and_mark_dead @ 0x1DB65 (8 callers)
 *
 * Scan all party slots for dying chars (hp_current == 0, flags bit0 not yet
 * set), play their death flicker + decay animation, then mark them
 * permanently dead.
 *
 * No parameters (__cdecl, void); the 0xA8 pushed before the stack-check is
 * this routine's own frame size.
 *
 * Phase 1 — collect on-screen dying chars:
 *   For each char 0..party_member_count-1: if flags bit0 == 0 AND
 *   hp_current == 0 AND (pos_x, pos_y) inside the battle view window, cache
 *   its screen-pixel pointer into the stack array dying_screen_pos[]. The
 *   per-char pointer is
 *     lgs + ((pos_y-1)-origin_y)*0x2AC0 + ((pos_x-1)-origin_x)*0x18 + 0x75D8
 *   (note the -1 on both axes, unlike the per-spell overlays).
 *
 *   If nothing was collected: set flags |= 1 on every hp_current==0 char and
 *   return (silent off-screen death, no animation).
 *
 * Phase 2 — flicker animation (13 frames):
 *   Per frame f in 0..12: composite the battle tile map into ws+0x8088, paint
 *   every non-dead char (an hp_current==0 char gets sprite_state[1] = f % 4 as
 *   a 4-frame blink key first), paint the shadow overlay, flush to the mode13h
 *   primary, wait one BIOS tick. After the loop: set flags |= 1 on every
 *   hp_current==0 char (PERMANENTLY DEAD).
 *
 * Phase 3 — decay/explosion (12 frames split 0..5 + 6..11):
 *   malloc a 0x25680 scratch buffer, composite a clean tile map into it, then
 *   composite_all_chars_overlay against it as the working buffer (swapping the
 *   global back-buffer pointer and restoring it). Fire the death SFX. Frames
 *   0..5 blit the per-frame death sprite (sheet[6 + (f+0x44)*4]) at each cached
 *   screen pos with no per-frame restore; frames 6..11 first memmove the clean
 *   scratch back into the live buffer so only the death sprite remains, then
 *   blit. free(scratch); fd2_composite_battle_frame(0) for the final cleanup.
 * ---------------------------------------------------------------- */
void fd2_play_death_animation_and_mark_dead(void)
{
    void *scratch_buf;
    void *saved_lgs;
    runtime_char *rt_char;
    int char_iter;
    int frame_iter;
    int tgt_iter;
    uint32 n_dying;
    uint32 frame_sprite_addr;
    uint32 pos_x;
    uint32 pos_y;
    uint32 dying_screen_pos[30];

    /* Phase 1 — collect on-screen dying chars */
    n_dying = 0;
    for (char_iter = 0; char_iter < (int)data_fd2_battle_party_member_count;
         char_iter++) {
        rt_char = &data_fd2_battle_runtime_char_array_ptr[char_iter];
        pos_x = rt_char->pos_x;
        pos_y = rt_char->pos_y;

        if (((rt_char->flags & 1) == 0) && (rt_char->hp_current == 0) &&
            ((int)(data_fd2_battle_view_window_origin_x - 1) <= (int)pos_x) &&
            ((int)pos_x <= (int)(data_fd2_battle_view_window_origin_x +
                                 data_fd2_battle_view_window_max_x)) &&
            ((int)(data_fd2_battle_view_window_origin_y - 1) <= (int)pos_y) &&
            ((int)pos_y <= (int)(data_fd2_battle_view_window_origin_y +
                                 data_fd2_battle_view_window_max_y + 1))) {
            dying_screen_pos[n_dying] =
                data_fd2_large_game_state_buffer_ptr +
                ((pos_y - 1) - data_fd2_battle_view_window_origin_y) * 0x2ac0 +
                ((pos_x - 1) - data_fd2_battle_view_window_origin_x) * 0x18 +
                0x75d8;
            n_dying++;
        }
    }

    if (n_dying == 0) {
        /* silent off-screen death: permanently mark every dying char */
        for (char_iter = 0; char_iter < (int)data_fd2_battle_party_member_count;
             char_iter++) {
            if (data_fd2_battle_runtime_char_array_ptr[char_iter].hp_current == 0) {
                data_fd2_battle_runtime_char_array_ptr[char_iter].flags = 1;
            }
        }
        return;
    }

    /* Phase 2 — 13-frame flicker */
    for (frame_iter = 0; frame_iter < 0xd; frame_iter++) {
        fd2_composite_battle_tile_map(
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0xd, 8,
            data_fd2_battle_view_window_origin_x,
            data_fd2_battle_view_window_origin_y);

        for (char_iter = 0; char_iter < (int)data_fd2_battle_party_member_count;
             char_iter++) {
            if ((data_fd2_battle_runtime_char_array_ptr[char_iter].flags & 1) == 0) {
                if (data_fd2_battle_runtime_char_array_ptr[char_iter].hp_current == 0) {
                    data_fd2_battle_runtime_char_array_ptr[char_iter].sprite_state[1] =
                        (uint8)(frame_iter % 4);
                }
                fd2_paint_char_sprite_at_world_pos((uint32)char_iter);
            }
        }

        fd2_redraw_terrain_tiles_under_chars();
        fd2_blit_rectangle(0xa0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_wait_n_bios_ticks(1);
    }

    /* permanently mark every dying char dead after the flicker */
    for (char_iter = 0; char_iter < (int)data_fd2_battle_party_member_count;
         char_iter++) {
        if (data_fd2_battle_runtime_char_array_ptr[char_iter].hp_current == 0) {
            data_fd2_battle_runtime_char_array_ptr[char_iter].flags = 1;
        }
    }

    /* Phase 3 — decay/explosion. Build a clean scratch composite, overlay all
     * chars onto it (via a temporary back-buffer swap), then strobe the death
     * sprite over the cached positions. */
    scratch_buf = malloc(0x25680);
    fd2_composite_battle_tile_map(
        (uint32)scratch_buf + 0x8088, 0x1c8, 0xd, 8,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);
    saved_lgs = (void *)data_fd2_large_game_state_buffer_ptr;
    data_fd2_large_game_state_buffer_ptr = (uint32)scratch_buf;
    fd2_composite_all_chars_overlay();
    data_fd2_large_game_state_buffer_ptr = (uint32)saved_lgs;

    fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 3, 1);

    /* frames 0..5: no per-frame restore */
    for (frame_iter = 0; frame_iter < 6; frame_iter++) {
        for (tgt_iter = 0; tgt_iter < (int)n_dying; tgt_iter++) {
            frame_sprite_addr =
                data_fd2_ui_anim_sprite_sheet_ptr +
                *(int *)(data_fd2_ui_anim_sprite_sheet_ptr +
                         (frame_iter + 0x44) * 4 + 6);
            fd2_blit_sprite_with_decoded_pixels(dying_screen_pos[tgt_iter],
                                                frame_sprite_addr, 0x1c8);
        }
        fd2_blit_rectangle(0xa0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_wait_n_bios_ticks(1);
    }

    /* frames 6..11: restore the clean scratch each frame, then blit */
    for (frame_iter = 6; frame_iter < 0xc; frame_iter++) {
        memmove((void *)data_fd2_large_game_state_buffer_ptr, scratch_buf, 0x25680);
        for (tgt_iter = 0; tgt_iter < (int)n_dying; tgt_iter++) {
            fd2_blit_sprite_with_decoded_pixels(
                dying_screen_pos[tgt_iter],
                data_fd2_ui_anim_sprite_sheet_ptr +
                    *(int *)(data_fd2_ui_anim_sprite_sheet_ptr +
                             (frame_iter + 0x44) * 4 + 6),
                0x1c8);
        }
        fd2_blit_rectangle(0xa0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_wait_n_bios_ticks(1);
    }

    free(scratch_buf);
    fd2_composite_battle_frame(0);
}

/* ----------------------------------------------------------------
 * fd2_animate_spell_projectile_paths @ 0x1DF58 (8 callers)
 *
 * Multi-target spell projectile / damage-number 22-frame flight-path
 * animation. Runs the floating-damage FX queue (built up by
 * fd2_show_damage_number / fd2_show_miss_indicator): each queued entry is a
 * sprite that rises over its target tile along a fixed 4-frame x 6-row y
 * offset pattern. Used by lightning / missile-rain style AoE spells and by
 * the damage-number rise after every hit.
 *
 * No parameters (__cdecl, void); the 0x4C pushed before the stack-check is
 * this routine's own frame size.
 *
 * Gate: runs only when the FX queue count (spell_aoe_count_and_fx_queue_idx)
 * is non-zero; otherwise it tail-jumps straight to the shared epilogue
 * (reproduced here as an immediate return).
 *
 * Setup:
 *   y_offset_table = first 25 bytes of the 28-byte projectile y-offset table
 *   (6 dwords + 1 byte REP MOVSD/MOVSB, matched byte-for-byte). Indexed by
 *   (fx_iter % 4 + frame) so each queue slot's row pattern is phase-shifted.
 *   snapshot_buf = malloc(0x25680); snapshot the live back-buffer into it.
 *
 * 22-frame loop (frame = 0..0x15):
 *   restore the clean back-buffer from the snapshot, then for each queued FX:
 *     sprite_id = floating_damage_sprite_id_queue[fx_iter]
 *     if sprite_id == 0: skip (blank digit / stopped effect)
 *     sprite_addr = sprite_sheet + sprite_sheet[6 + sprite_id*4]
 *     target = runtime_char_array[floating_damage_target_char_idx_queue[fx_iter]]
 *     dst = lgs + 0x8088
 *         + (target.pos_y - origin_y) * 0x2AC0
 *         + (target.pos_x - origin_x) * 0x18
 *         + floating_damage_x_offset_queue[fx_iter]
 *         + (y_offset_table[fx_iter % 4 + frame] - 3) * 0x1C8
 *     fd2_blit_sprite_with_decoded_pixels(dst, sprite_addr, 0x1C8)
 *   flush composite to the mode13h primary; fd2_delay_ms(2) (~2ms pacing).
 *
 * End: free the snapshot, fd2_delay_ms(500) (~500ms settle), then tail-jump
 * to the shared epilogue (reproduced as the function return).
 * ---------------------------------------------------------------- */
void fd2_animate_spell_projectile_paths(void)
{
    void *snapshot_buf;
    runtime_char *target;
    int frame;
    int fx_iter;
    uint32 sprite_addr;
    uint32 dst_addr;
    uint8 y_offset_table[28];

    /* snapshot the projectile y-offset table (6 dwords + 1 byte = 25B) */
    memcpy(y_offset_table, data_fd2_animation_spell_projectile_y_offset_table, 25);

    if (data_fd2_battle_spell_aoe_count_and_fx_queue_idx == 0) {
        return;
    }

    snapshot_buf = malloc(0x25680);
    memmove(snapshot_buf, (void *)data_fd2_large_game_state_buffer_ptr, 0x25680);

    for (frame = 0; frame < 0x16; frame++) {
        /* restore the clean baseline back-buffer for this frame */
        memmove((void *)data_fd2_large_game_state_buffer_ptr, snapshot_buf, 0x25680);

        for (fx_iter = 0;
             fx_iter < (int)data_fd2_battle_spell_aoe_count_and_fx_queue_idx;
             fx_iter++) {
            if (data_fd2_battle_floating_damage_sprite_id_queue[fx_iter] == 0) {
                continue;
            }

            sprite_addr =
                data_fd2_ui_anim_sprite_sheet_ptr +
                *(uint32 *)(data_fd2_ui_anim_sprite_sheet_ptr + 6 +
                            (uint32)data_fd2_battle_floating_damage_sprite_id_queue[fx_iter] * 4);

            target = &data_fd2_battle_runtime_char_array_ptr
                          [data_fd2_battle_floating_damage_target_char_idx_queue[fx_iter]];

            dst_addr = data_fd2_large_game_state_buffer_ptr + 0x8088 +
                       ((uint32)target->pos_y - data_fd2_battle_view_window_origin_y) * 0x2ac0 +
                       ((uint32)target->pos_x - data_fd2_battle_view_window_origin_x) * 0x18 +
                       (uint32)data_fd2_battle_floating_damage_x_offset_queue[fx_iter] +
                       (y_offset_table[fx_iter % 4 + frame] - 3) * 0x1c8;

            fd2_blit_sprite_with_decoded_pixels(dst_addr, sprite_addr, 0x1c8);
        }

        fd2_blit_rectangle(0xa0504, 0x140,
                           data_fd2_large_game_state_buffer_ptr + 0x8088,
                           0x1c8, 0x138, 0xc0);
        fd2_delay_ms(2);
    }

    free(snapshot_buf);
    fd2_delay_ms(500);
}

/* ----------------------------------------------------------------
 * fd2_show_damage_number @ 0x1E0DB (15 callers)
 *
 * Enqueue a 4-digit floating-damage number over a target's tile. Producer for
 * the FX queue consumed by fd2_animate_spell_projectile_paths: appends four
 * queue slots (one per decimal place: thousands/hundreds/tens/units), so a hit
 * shows the damage rising over the target's head. Called by every
 * apply_*_spell / apply_use_effect / apply_attack_spell_damage / cast_*_spell
 * path after damage/heal resolution.
 *
 * Parameters (__cdecl, 3 args):
 *   amount         the number to display (a damage or heal value)
 *   marker_char    sprite-id base for the digits: '^' (0x5E, red attack/magic
 *                  damage), 'i' (0x69, green heal/positive), '_' suppress
 *   target_idx     runtime-char index whose tile the number floats over
 *
 * Viewport cull: if the target is outside the battle view window, nothing is
 * enqueued and the queue count is left unchanged. Note the cull predicate is
 * NOT identical to the sibling spell overlays: the x test uses origin_x-1 as an
 * exclusive lower / origin_x+max_x as an exclusive upper bound; the y test uses
 * origin_y-1 as an inclusive lower / origin_y+max_y as an inclusive upper bound
 * (no +1 on the y upper edge).
 *
 * For each of the four digit positions (digit_iter 0..3, magnitude_threshold
 * stepping 3..0):
 *   - x_offset_queue[base+digit_iter]      = digit_iter*5 + 2  (5px/digit row)
 *   - target_char_idx_queue[base+digit_iter] = target_idx
 *   - re-format the whole number with "%d" and take its length; a digit slot is
 *     shown only once the number is long enough to reach that place
 *     (strlen > magnitude_threshold). When shown:
 *         sprite_id_queue[base+digit_iter] = marker_char + numstr[digit_pos]-'0'
 *     and digit_pos advances to the next formatted character; otherwise the
 *     slot is blanked (sprite_id 0) so the consumer skips it.
 * Finally the queue count (spell_aoe_count_and_fx_queue_idx) advances by 4.
 *
 * The 8-byte work buffer is primed from the "    \0" template @ 0x52045 (only 5
 * bytes are copied; the trailing bytes are never read). The original tail-jumps
 * into the shared epilogue (fd2_noop_stub_b43); reproduced here as the return.
 * ---------------------------------------------------------------- */
void fd2_show_damage_number(uint32 amount, uint32 marker_char, uint32 target_idx)
{
    runtime_char *target;
    char num_string[8];
    uint32 digit_iter;
    uint32 digit_pos;
    uint32 magnitude_threshold;
    uint32 len;
    int target_x;
    int target_y;

    /* prime the 8-byte work buffer with the "    \0" template (5 bytes) */
    memcpy(num_string, data_fd2_battle_damage_number_format_buffer, 5);
    magnitude_threshold = 3;
    digit_pos = 0;

    target = &data_fd2_battle_runtime_char_array_ptr[target_idx];
    target_x = target->pos_x;
    target_y = target->pos_y;

    /* viewport cull (see header: asymmetric x<= / y< lower-edge tests) */
    if ((target_x <= (int)data_fd2_battle_view_window_origin_x - 1) ||
        (target_x >= (int)(data_fd2_battle_view_window_origin_x +
                           data_fd2_battle_view_window_max_x)) ||
        (target_y < (int)data_fd2_battle_view_window_origin_y - 1) ||
        (target_y > (int)(data_fd2_battle_view_window_origin_y +
                          data_fd2_battle_view_window_max_y))) {
        return;
    }

    for (digit_iter = 0; (int)digit_iter < 4; digit_iter++) {
        sprintf(num_string, "%d", amount);

        data_fd2_battle_floating_damage_x_offset_queue
            [data_fd2_battle_spell_aoe_count_and_fx_queue_idx + digit_iter] =
                (uint8)(digit_iter * 5 + 2);
        data_fd2_battle_floating_damage_target_char_idx_queue
            [data_fd2_battle_spell_aoe_count_and_fx_queue_idx + digit_iter] =
                (uint8)target_idx;

        len = strlen(num_string);
        if (magnitude_threshold < len) {
            data_fd2_battle_floating_damage_sprite_id_queue
                [data_fd2_battle_spell_aoe_count_and_fx_queue_idx + digit_iter] =
                    (uint8)(marker_char + num_string[digit_pos] - '0');
            digit_pos++;
        } else {
            data_fd2_battle_floating_damage_sprite_id_queue
                [data_fd2_battle_spell_aoe_count_and_fx_queue_idx + digit_iter] = 0;
        }
        magnitude_threshold--;
    }

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx += 4;
}

/* ----------------------------------------------------------------
 * fd2_show_miss_indicator @ 0x1E1DC (13 callers)
 *
 * Enqueue a 4-slot "MISS" floating indicator over a target's tile. Sibling
 * producer to fd2_show_damage_number for the FX queue consumed by
 * fd2_animate_spell_projectile_paths: when an attack misses or a target
 * dodges/resists, four queue slots are appended so a fixed 4-sprite "MISS"
 * sequence rises over the target's head. Called by every
 * apply_use_effect / apply_attack_spell / cast_*_spell / execute_offensive_*
 * path after a hit roll fails.
 *
 * Parameters (__cdecl, 1 arg):
 *   target_idx   runtime-char index whose tile the indicator floats over
 *
 * Viewport cull (identical predicate to fd2_show_damage_number): the x test
 * uses origin_x-1 as an exclusive lower / origin_x+max_x as an exclusive
 * upper bound; the y test uses origin_y-1 as an inclusive lower /
 * origin_y+max_y as an inclusive upper bound. If the target is outside the
 * battle view window, nothing is enqueued and the queue count is unchanged.
 *
 * For each of the four sprite slots (char_iter 0..3):
 *   - x_offset_queue[base+char_iter]       = (char_iter==1) ? 8 : char_iter*5+2
 *     (5px-per-row layout, with the second sprite nudged to row 8 instead of 7)
 *   - target_char_idx_queue[base+char_iter] = target_idx
 *   - sprite_id_queue[base+char_iter]       = miss_indicator_sprite_ids[char_iter]
 * Finally the queue count (spell_aoe_count_and_fx_queue_idx) advances by 4.
 *
 * The 4 sprite ids are primed from the live table @ 0x5204A (loaded as one
 * dword into a 4-byte work buffer, then read back per slot). The original
 * tail-jumps into the shared epilogue; reproduced here as the return.
 * ---------------------------------------------------------------- */
void fd2_show_miss_indicator(uint32 target_idx)
{
    runtime_char *target;
    uint8 sprite_ids[4];
    uint32 char_iter;
    int target_x;
    int target_y;

    /* prime the 4-byte work buffer with the miss-indicator sprite ids */
    memcpy(sprite_ids, data_fd2_battle_miss_indicator_sprite_ids, 4);

    target = &data_fd2_battle_runtime_char_array_ptr[target_idx];
    target_x = target->pos_x;
    target_y = target->pos_y;

    /* viewport cull (see header: asymmetric x<= / y< lower-edge tests) */
    if ((target_x <= (int)data_fd2_battle_view_window_origin_x - 1) ||
        (target_x >= (int)(data_fd2_battle_view_window_origin_x +
                           data_fd2_battle_view_window_max_x)) ||
        (target_y < (int)data_fd2_battle_view_window_origin_y - 1) ||
        (target_y > (int)(data_fd2_battle_view_window_origin_y +
                          data_fd2_battle_view_window_max_y))) {
        return;
    }

    for (char_iter = 0; (int)char_iter < 4; char_iter++) {
        if (char_iter == 1) {
            data_fd2_battle_floating_damage_x_offset_queue
                [data_fd2_battle_spell_aoe_count_and_fx_queue_idx + char_iter] = 8;
        } else {
            data_fd2_battle_floating_damage_x_offset_queue
                [data_fd2_battle_spell_aoe_count_and_fx_queue_idx + char_iter] =
                    (uint8)(char_iter * 5 + 2);
        }

        data_fd2_battle_floating_damage_target_char_idx_queue
            [data_fd2_battle_spell_aoe_count_and_fx_queue_idx + char_iter] =
                (uint8)target_idx;
        data_fd2_battle_floating_damage_sprite_id_queue
            [data_fd2_battle_spell_aoe_count_and_fx_queue_idx + char_iter] =
                sprite_ids[char_iter];
    }

    data_fd2_battle_spell_aoe_count_and_fx_queue_idx += 4;
}

/* ----------------------------------------------------------------
 * fd2_animate_combat_hit_with_hp_drain @ 0x1E856 (2 callers)
 *
 * Resolve and play one melee hit, then smoothly drain the defender's HP
 * bar from its pre-hit length down to the post-hit length. The outer loop
 * repeats while the defender is still alive and a hit budget remains, so a
 * double-strike weapon (or the random extra-hit proc) lands twice.
 *
 * Hit budget (hits_remaining):
 *   - the attacker's equipped weapon (slot 0) is looked up and its effect
 *     entry fetched; weapon_entry[+9] is the special-type byte.
 *   - special-type == 3 (the "double" weapon class) -> budget 2.
 *   - one RNG draw is then taken UNCONDITIONALLY (it always advances the
 *     shared seed); if (draw % 100 < 3) the budget is raised to 2 as a flat
 *     3% extra-hit proc.
 *
 * EAX-bug note: Ghidra's decompiler re-uses weapon_entry[+9] as the value
 * fed into `% 100 < 3`, because it loses track that the CALL to
 * fd2_advance_rng_state clobbers EAX. The assembly (0x1E8C3 MOV EDX,EAX
 * right after the CALL at 0x1E8BE) divides the RNG RETURN value, not the
 * weapon class. The faithful condition is therefore
 *   fd2_advance_rng_state() % 100 < 3
 * and the threshold is the literal 3 (0x1E8CF CMP EDX,0x3), not the weapon
 * byte.
 *
 * Per hit:
 *   - bar_numerator = defender.hp_current(pre-hit) * 0x46  (0x46 = 70, the
 *     full bar width 0x45 plus one so integer truncation still yields a full
 *     bar at full HP).
 *   - fd2_execute_attack_damage_calculation applies the damage and returns
 *     the surviving HP.
 *   - fd2_animate_attack_hit_sequence plays the weapon sprite/SFX.
 *   - the bar shrinks one pixel at a time from the pre-hit length
 *     (bar_numerator / hp_max) down to the post-hit floor
 *     ((surviving_HP * 0x45) / hp_max + 1), ~8 BIOS ticks per frame.
 *
 * Destination = (panel_xy[1] + 6) * 320 + panel_xy[0] + 0xA0007, i.e. inside
 * the 0xA0000 combat overlay surface. panel_xy is a 2-int (x, y) from the
 * caller.
 *
 * Returns 0 when the defender dies, otherwise the surviving HP.
 *
 * 2 callers: fd2_execute_ai_physical_attack (hit + counter-attack).
 * ---------------------------------------------------------------- */
int fd2_animate_combat_hit_with_hp_drain(uint32 attacker_idx, uint32 defender_idx,
                                         uint32 panel_xy_ptr)
{
    runtime_char *defender;
    uint8 *weapon_entry;
    uint32 weapon_slot;
    uint8 weapon_id;
    uint32 weapon_class;
    uint32 hp_max;
    uint32 bar_numerator;
    uint32 bar_pixels;
    int hits_remaining;
    int surviving_HP;

    defender = &data_fd2_battle_runtime_char_array_ptr[defender_idx];
    hits_remaining = 1;

    weapon_slot = fd2_find_equipped_item_by_kind(attacker_idx, 0);
    weapon_id = fd2_get_inventory_slot_item_id(attacker_idx, weapon_slot);
    weapon_entry = fd2_get_item_effect_entry(weapon_id);
    weapon_class = weapon_entry[9];
    if (weapon_class == 3) {
        hits_remaining = 2;
    }
    /* unconditional RNG advance; the proc threshold is the literal 3, fed by
     * the RNG return value (see EAX-bug note above) */
    if ((int)fd2_advance_rng_state() % 100 < 3) {
        hits_remaining = 2;
    }

    surviving_HP = 0;
    do {
        if (hits_remaining == 0) {
            return surviving_HP;
        }
        hp_max = defender->hp_max;
        bar_numerator = (uint32)defender->hp_current * 0x46;
        surviving_HP = fd2_execute_attack_damage_calculation((int)attacker_idx,
                                                             (int)defender_idx);
        fd2_animate_attack_hit_sequence(attacker_idx, defender_idx);
        for (bar_pixels = (uint32)((int)bar_numerator / (int)hp_max);
             (int)(surviving_HP * 0x45) / (int)hp_max + 1 <= (int)bar_pixels;
             bar_pixels--) {
            fd2_render_combat_hp_bar_segments(
                (*(int32 *)(panel_xy_ptr + 4) + 6) * 0x140 +
                    *(int32 *)panel_xy_ptr + 0xa0007,
                0x140, bar_pixels);
            fd2_delay_ms(8);
        }
        hits_remaining--;
    } while (surviving_HP != 0);
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_animate_attack_hit_sequence @ 0x1E98C (1 caller)
 *
 * Weapon-attack hit animation: per-step sprite + SFX sequence played at
 * the defender's on-screen position.
 *
 *   weapon_slot = fd2_find_equipped_item_by_kind(attacker_idx, 0)
 *   weapon_id   = fd2_get_inventory_slot_item_id(attacker_idx, weapon_slot)
 *   pWeapon     = fd2_get_item_effect_entry(weapon_id)
 *   pattern     = fd2_get_attack_anim_pattern_for_weapon(pWeapon[0])
 *   step_count  = pattern[0]
 *
 * Each weapon has its own attack pattern (sword swing / bow shot / spell
 * book cast, ...); the pattern table is supplied by
 * fd2_get_attack_anim_pattern_for_weapon. For each step:
 *   sprite_id = pattern[step*2 + 1]
 *   sfx_id    = pattern[step*2 + 2]   (0xFF = silent)
 *   if sfx_id != 0xFF:
 *       if last-hit/miss flag != 0 (a miss): sfx_id := 4 (forced whoosh)
 *       fd2_play_sfx_with_handle(walk_overlay_ptr, sfx_id, 1)
 *   pose switch (only on a hit; a miss leaves the attacker still):
 *       step 0 && !miss: paint(0xA0504, 0x140, defender_idx, mode=2, 0xFD) (attack pose)
 *       step 1 && !miss: paint(0xA0504, 0x140, defender_idx, mode=0, 0)    (back to idle)
 *   draw the hit sprite at the defender's tile:
 *       saved = fd2_alloc_and_blit_indexed_sprite_chunk(
 *                   portrait_sheet, 0xA0000, 0x140,
 *                   (pos_x - origin_x)*0x18 + 4, (pos_y - origin_y)*0x18, sprite_id)
 *       fd2_delay_ms(0x50)   (~80 ms)
 *       fd2_cleanup_dialog_sprite_buffer(saved, 0xA0000, 0x140)
 *
 * The paint target index is the defender: arg2 is loaded into EBP at 0x1E99D
 * and pushed as the paint target at 0x1EA4E (both pose paths converge there),
 * per both the disassembly and the decompiler.
 *
 * EAX-bug note: the cleanup call receives the SAVE-BLOCK HANDLE returned by
 * fd2_alloc_and_blit_indexed_sprite_chunk (asm 0x1EA86 MOV ESI,EAX captures
 * the return value, passed to cleanup at 0x1EA9F PUSH ESI), NOT the sprite
 * id. Ghidra's decompiler lost the EAX value across the CALL and incorrectly
 * reused the sprite id (sprite_buf_arg) as the cleanup argument; the faithful
 * argument is the malloc'd save buffer that cleanup restores then frees.
 *
 * 1 caller: fd2_animate_combat_hit_with_hp_drain.
 * ---------------------------------------------------------------- */
void fd2_animate_attack_hit_sequence(uint32 attacker_idx, uint32 defender_idx)
{
    runtime_char *defender;
    uint8 *weapon_entry;
    uint8 *pattern;
    uint32 weapon_slot;
    uint8 weapon_id;
    uint8 step_count;
    uint32 step_iter;
    uint32 sfx_byte;
    uint32 sprite_id;
    uint32 dst_x;
    uint32 dst_y;
    uint32 saved_block;

    weapon_slot = fd2_find_equipped_item_by_kind(attacker_idx, 0);
    weapon_id = fd2_get_inventory_slot_item_id(attacker_idx, weapon_slot);
    weapon_entry = fd2_get_item_effect_entry(weapon_id);
    pattern = fd2_get_attack_anim_pattern_for_weapon(weapon_entry[0]);
    step_count = pattern[0];

    defender = &data_fd2_battle_runtime_char_array_ptr[defender_idx];
    dst_x = ((uint32)defender->pos_x - data_fd2_battle_view_window_origin_x) * 0x18 + 4;
    dst_y = ((uint32)defender->pos_y - data_fd2_battle_view_window_origin_y) * 0x18;

    for (step_iter = 0; (int)step_iter < (int)(uint32)step_count; step_iter++) {
        sfx_byte = pattern[step_iter * 2 + 2];
        if (sfx_byte != 0xff) {
            if (data_fd2_battle_last_hit_or_miss_flag != 0) {
                sfx_byte = 4;
            }
            fd2_play_sfx_with_handle(data_fd2_battle_fast_mode_walk_overlay_ptr,
                                     (int)sfx_byte, 1);
        }

        if (step_iter == 0 && data_fd2_battle_last_hit_or_miss_flag == 0) {
            fd2_paint_char_sprite_at_world_with_mode(0xa0504, 0x140, defender_idx, 2, 0xfd);
        } else if (step_iter == 1 && data_fd2_battle_last_hit_or_miss_flag == 0) {
            fd2_paint_char_sprite_at_world_with_mode(0xa0504, 0x140, defender_idx, 0, 0);
        }

        sprite_id = pattern[step_iter * 2 + 1];
        saved_block = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_resource_portrait_sheet_ptr, 0xa0000, 0x140,
            dst_x, dst_y, sprite_id);
        fd2_delay_ms(0x50);
        fd2_cleanup_dialog_sprite_buffer(saved_block, 0xa0000, 0x140);
    }
}

/* ----------------------------------------------------------------
 * fd2_animate_combat_speech_bubbles @ 0x1EB05 (1 caller)
 *
 * Pre-attack "vs" speech-bubble fade-in, a 10-frame animation played at
 * the start of an AI physical-attack sequence.
 *
 * Step 1: compute the attacker's bubble screen position into
 *         combat_speech_bubble_pos_pairs[0..1] (x, y) from defender_idx.
 * Step 2: if the defender can counter-attack, compute the counter bubble
 *         position into [2..3] from attacker_idx; otherwise store -1 in [2]
 *         as the "no counter" sentinel.
 *
 * Frames 0..9 use portrait sprite ids 0x27..0x30 (frame + 0x27). Each frame
 * allocates+blits the attacker bubble (and the counter bubble when present),
 * waits ~25ms, and for frames 0..8 restores the previous frame's snapshot
 * before drawing the next. After the loop the bubble buffers are freed.
 *
 * Returns the address of combat_speech_bubble_pos_pairs (as a uint32); the
 * caller reads [0..1] via this pointer and [2..3] via pointer + 8.
 *
 * combat_speech_bubble_pos_pairs (0x53A30, 4 dwords = 16 bytes):
 *   [0] 0x53A30 attacker bubble x   [1] 0x53A34 attacker bubble y
 *   [2] 0x53A38 counter  bubble x   [3] 0x53A3C counter  bubble y
 *                        (-1 in [2] means no counter)
 *
 * Caller: fd2_execute_ai_physical_attack (sole caller).
 * ---------------------------------------------------------------- */
uint32 fd2_animate_combat_speech_bubbles(uint32 attacker_idx, uint32 defender_idx)
{
    uint32 can_counter;
    uint32 attacker_buf;
    uint32 counter_buf;
    uint32 frame_iter;
    uint32 sprite_id;

    counter_buf = 0;

    fd2_compute_combat_bubble_screen_pos(
        (uint32)data_fd2_battle_combat_speech_bubble_pos_pairs, defender_idx);

    can_counter = (uint32)fd2_check_can_counter_attack(attacker_idx, defender_idx);
    if (can_counter == 1) {
        fd2_compute_combat_bubble_screen_pos(
            (uint32)&data_fd2_battle_combat_speech_bubble_pos_pairs[2], attacker_idx);
    } else {
        data_fd2_battle_combat_speech_bubble_pos_pairs[2] = 0xffffffff;
    }

    for (frame_iter = 0; (int)frame_iter < 10; frame_iter++) {
        sprite_id = frame_iter + 0x27;

        attacker_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_resource_portrait_sheet_ptr, 0xa0000, 0x140,
            data_fd2_battle_combat_speech_bubble_pos_pairs[0],
            data_fd2_battle_combat_speech_bubble_pos_pairs[1], sprite_id);

        if (data_fd2_battle_combat_speech_bubble_pos_pairs[2] != 0xffffffff) {
            counter_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
                data_fd2_resource_portrait_sheet_ptr, 0xa0000, 0x140,
                data_fd2_battle_combat_speech_bubble_pos_pairs[2],
                data_fd2_battle_combat_speech_bubble_pos_pairs[3], sprite_id);
        }

        fd2_delay_ms(0x19);

        if ((int)frame_iter < 9) {
            fd2_cleanup_dialog_sprite_buffer(attacker_buf, 0xa0000, 0x140);
            if (data_fd2_battle_combat_speech_bubble_pos_pairs[2] != 0xffffffff) {
                fd2_cleanup_dialog_sprite_buffer(counter_buf, 0xa0000, 0x140);
            }
        }
    }

    free((void *)attacker_buf);
    if (data_fd2_battle_combat_speech_bubble_pos_pairs[2] != 0xffffffff) {
        free((void *)counter_buf);
    }

    return (uint32)data_fd2_battle_combat_speech_bubble_pos_pairs;
}

/* ----------------------------------------------------------------
 * fd2_animate_phase_banner_slide_in @ 0x1F1CC (1 caller)
 *
 * "ENEMY TURN" / "PLAYER TURN" turn-phase banner slide-in animation.
 * Called by fd2_run_full_turn_cycle at the start of Phase D (ENEMY,
 * banner_sprite_id 0x52) and Phase F (PLAYER, banner_sprite_id 0x50).
 * Paired with fd2_animate_phase_banner_slide_out.
 *
 *   snapshot = malloc(64000); memmove(snapshot, 0xA0000, 64000)  // backup VGA
 *
 * Phase 1 (slide-in, 7 frames): the two banner halves slide from the
 *   screen edges toward centre. x_offset = 0x64,0x4B,0x32,0x19,0 (the
 *   frame_iter*0x19 countdown), then a 1 and a 0 settle frame.
 *
 *   memmove(large_game_state_buffer, snapshot, 64000)  // restore working buf
 *   memset(snapshot, 0, 64000)                          // reuse as black frame
 *
 * Phase 2 (palette fade-in, 16 frames): for brightness 0..15 the source
 *   block is scrolled (scroll_offset 1..16) into the snapshot, the main
 *   banner sprite (banner_sprite_id) and the corner frame sprite (0x51)
 *   are blitted, the VGA palette is faded up, and the frame is pushed to
 *   0xA0000 with a one-tick wait.
 *
 * NOTE (Ghidra EAX-tracking bug): the decompiler collapsed the two
 * fd2_alloc_and_blit_indexed_sprite_chunk return values and the trailing
 * memmove return into a single reused temporary. The assembly shows each
 * blit returns its own freshly malloc'd save buffer, each freed
 * immediately after its blit (PUSH EAX; CALL free); the memmove return is
 * discarded. Encoded as two separate save-buffer frees per frame.
 * ---------------------------------------------------------------- */
void fd2_animate_phase_banner_slide_in(uint32 banner_sprite_id)
{
    uint8 *snapshot_buf;
    uint32 blit_buf;
    uint32 frame_iter;
    uint32 scroll_offset;
    uint32 fade_iter;

    scroll_offset = 1;
    snapshot_buf = (uint8 *)malloc(64000);
    memmove(snapshot_buf, (void *)0xa0000, 64000);

    for (frame_iter = 4; -1 < (int32)frame_iter; frame_iter--) {
        fd2_render_phase_banner_frame(frame_iter * 0x19, banner_sprite_id);
    }
    fd2_render_phase_banner_frame(1, banner_sprite_id);
    fd2_render_phase_banner_frame(0, banner_sprite_id);

    memmove((void *)data_fd2_large_game_state_buffer_ptr, snapshot_buf, 64000);
    memset(snapshot_buf, 0, 64000);

    for (fade_iter = 0; (int32)fade_iter < 0x10; fade_iter++) {
        fd2_scroll_buffer_block_with_wrap(scroll_offset, snapshot_buf,
                                          (void *)data_fd2_large_game_state_buffer_ptr);
        blit_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr, (uint32)snapshot_buf, 0x140,
            0x59, 0x56, banner_sprite_id);
        free((void *)blit_buf);
        blit_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr, (uint32)snapshot_buf, 0x140,
            0xa9, 0x56, 0x51);
        free((void *)blit_buf);
        fd2_set_vga_palette_range(0x10, 0xff, fade_iter);
        memmove((void *)0xa0000, snapshot_buf, 64000);
        fd2_wait_n_bios_ticks(1);
        scroll_offset++;
    }

    free(snapshot_buf);
    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * fd2_animate_phase_banner_slide_out @ 0x1F30A (1 caller)
 *
 * "ENEMY TURN" / "PLAYER TURN" turn-phase banner slide-out animation.
 * Called by fd2_run_full_turn_cycle once a phase banner has finished
 * displaying (Phase D / Phase F). Paired with
 * fd2_animate_phase_banner_slide_in.
 *
 *   scratch = malloc(64000); memset(scratch, 0, 64000)   // black frame
 *
 * Phase 1 (palette fade-out, 17 frames, fade_iter 0x10..0 counting down):
 *   the source block is scrolled (scroll_offset 0x11 counting down) into
 *   the black scratch frame, the main banner sprite (banner_sprite_id) and
 *   the corner frame sprite (0x51) are blitted, the VGA palette is faded
 *   down (brightness 16..0), and the frame is pushed to 0xA0000 with a
 *   one-tick wait.
 *
 * Phase 2 (restore the battle scene): recomposite the battle tile map and
 *   overlay all chars back onto the working buffer.
 *
 * Phase 3 (5-frame slide-out): the two banner halves slide from centre out
 *   to the screen edges. x_offset = iVar1*0x19 = 0,0x19,0x32,0x4B,0x64.
 *
 * NOTE (Ghidra EAX-tracking bug): the decompiler collapsed the two
 * fd2_alloc_and_blit_indexed_sprite_chunk return values and the trailing
 * memmove return into a single reused temporary, and even hoisted the frees
 * into the wrong place. The assembly shows each blit returns its own freshly
 * malloc'd save buffer, each freed immediately after its blit (PUSH EAX;
 * CALL free); the memmove return is discarded; the scratch frame buffer is
 * freed once, after the fade loop. Encoded here to match the assembly.
 * ---------------------------------------------------------------- */
void fd2_animate_phase_banner_slide_out(uint32 banner_sprite_id)
{
    uint8 *scratch_buf;
    uint32 blit_buf;
    int32 frame_iter;
    uint32 scroll_offset;
    uint32 fade_iter;

    scroll_offset = 0x11;
    scratch_buf = (uint8 *)malloc(64000);
    memset(scratch_buf, 0, 64000);

    for (fade_iter = 0x10; -1 < (int32)fade_iter; fade_iter--) {
        fd2_scroll_buffer_block_with_wrap(scroll_offset, scratch_buf,
                                          (void *)data_fd2_large_game_state_buffer_ptr);
        blit_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr, (uint32)scratch_buf, 0x140,
            0x59, 0x56, banner_sprite_id);
        free((void *)blit_buf);
        blit_buf = fd2_alloc_and_blit_indexed_sprite_chunk(
            data_fd2_ui_anim_sprite_sheet_ptr, (uint32)scratch_buf, 0x140,
            0xa9, 0x56, 0x51);
        free((void *)blit_buf);
        fd2_set_vga_palette_range(0x10, 0xff, fade_iter);
        memmove((void *)0xa0000, scratch_buf, 64000);
        fd2_wait_n_bios_ticks(1);
        scroll_offset--;
    }

    free(scratch_buf);

    fd2_composite_battle_tile_map(
        data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1c8, 0xd, 8,
        data_fd2_battle_view_window_origin_x,
        data_fd2_battle_view_window_origin_y);
    fd2_composite_all_chars_overlay();

    for (frame_iter = 0; frame_iter < 5; frame_iter++) {
        fd2_render_phase_banner_frame(frame_iter * 0x19, banner_sprite_id);
    }
}
