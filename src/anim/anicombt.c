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
