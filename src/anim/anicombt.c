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
 * Parameters (__cdecl, 4 args; param_1 only forwarded to the stack check):
 *   param_1            unused by the body
 *   status_kind        selects the silhouette colour tmp_copy[status_kind]
 *   target_count       number of entries in char_idx_array
 *   char_idx_array     byte array of runtime-char indices to overlay
 *
 * The colour table at 0x51F15 is copied into a 30-byte stack scratch
 * (7 dwords + 1 word in the original, matched here byte-for-byte) before
 * indexing by status_kind.
 * ---------------------------------------------------------------- */
void fd2_animate_status_effect_overlay_flicker(uint32 param_1, uint32 status_kind,
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

    (void)param_1;

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

        src_sprite = portrait_sprite_cache +
                     *(uint32 *)(portrait_sprite_cache + frame_idx * 4);
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
 * Parameters (__cdecl, 4 args; param_1 only forwarded to the stack check):
 *   param_1            unused by the body
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
void fd2_animate_spell_impact_per_target(uint32 param_1, uint32 spell_id,
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

    (void)param_1;

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
 * Parameters (__cdecl, 4 args; param_1 and spell_id are only consumed by
 * the stack check, not by the body):
 *   param_1            unused by the body
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
void fd2_animate_spell_full_screen_flash(uint32 param_1, uint32 spell_id,
                                         uint32 target_count,
                                         uint32 char_idx_array)
{
    uint8 *flash_buf;
    int iter;

    (void)param_1;
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
        __delay_thunk_375b2(0x5a);
        fd2_blit_rectangle(0xa0504, 0x140, (uint32)flash_buf + 0x8088,
                           0x1c8, 0x138, 0xc0);
        __delay_thunk_375b2(0x5a);
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
 * Parameters (__cdecl, 4 args; param_1 only forwarded to the stack check):
 *   param_1            unused by the body
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
void fd2_animate_spell_overlay_blink(uint32 param_1, uint32 spell_id,
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

    (void)param_1;

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

                src_sprite = portrait_sprite_cache +
                             *(uint32 *)(portrait_sprite_cache + frame_idx * 4);
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

        fd2_paint_chars_shadow_overlay();
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
