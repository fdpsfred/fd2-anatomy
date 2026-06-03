/*
 * rsrc.c — chapter resource loading (background layers, battle data)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>

/* ----------------------------------------------------------------
 * fd2_load_chapter_background_layers @ 0x10652  (1 caller)
 *
 * Chapter-specific background layer load from FDOTHER.DAT.
 *
 * Frees + nulls both static_bg_buffer and animated_bg_buffer, then
 * selects one of three load shapes by current chapter id:
 *   - Single-sprite default (chapters 9/0x18/0x19 -> idx 0xF,
 *     0x1C/0x1D -> idx 0x37, all others -> idx 0x10): load sprite
 *     into static_bg_buffer, allocate a 320x200 work buffer.
 *   - 2-sprite widescreen (chapters 0x11/0x15/0x16/0x1B): allocate
 *     static_bg_buffer sized bg_width*bg_height, blit a top half and
 *     a bottom half (top at y=0, bottom at y=bg_height/2).
 *   - Text-scroll cinematic (chapter 0x17): allocate a 312x200
 *     buffer, blit sheet idx 0x2A, arm the text-scroll cinematic.
 * ---------------------------------------------------------------- */
void fd2_load_chapter_background_layers(void)
{
    uint32 bg_width;
    uint32 bg_height;
    uint32 fdother_idx;
    uint32 idx_base;
    uint32 top_sprite;
    uint32 bot_sprite;

    bg_width = 0x1ce;
    bg_height = 0xe2;
    fdother_idx = 0x10;

    if (data_fd2_graphics_static_bg_buffer_ptr != 0) {
        free((void *)data_fd2_graphics_static_bg_buffer_ptr);
    }
    data_fd2_graphics_static_bg_buffer_ptr = 0;

    if (data_fd2_graphics_animated_bg_buffer_ptr != 0) {
        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
    }
    data_fd2_graphics_animated_bg_buffer_ptr = 0;

    if (data_fd2_chapter_current_chapter_id == 9 ||
        data_fd2_chapter_current_chapter_id == 0x18 ||
        data_fd2_chapter_current_chapter_id == 0x19) {
        fdother_idx = 0xf;
    } else if (data_fd2_chapter_current_chapter_id == 0x11 ||
               data_fd2_chapter_current_chapter_id == 0x15 ||
               data_fd2_chapter_current_chapter_id == 0x16 ||
               data_fd2_chapter_current_chapter_id == 0x1b) {
        idx_base = 0x10;
        if (data_fd2_chapter_current_chapter_id == 0x15) {
            bg_width = 0x198;
            bg_height = 0x114;
            idx_base = 0x23;
        } else if (data_fd2_chapter_current_chapter_id == 0x16) {
            bg_width = 0x198;
            bg_height = 0x100;
            idx_base = 0x28;
        } else if (data_fd2_chapter_current_chapter_id == 0x1b) {
            bg_height = 0xf4;
            idx_base = 0x2e;
        }

        data_fd2_graphics_static_bg_buffer_ptr =
            (uint32)malloc(bg_width * bg_height);

        top_sprite = fd2_load_dat_resource(0x51a4d,
            data_fd2_graphics_animated_bg_buffer_ptr, idx_base);
        data_fd2_graphics_animated_bg_buffer_ptr = top_sprite;
        fd2_rle_blit_sprite(top_sprite, 0, 0,
            data_fd2_graphics_static_bg_buffer_ptr, bg_width, 0xffffffff);

        bot_sprite = fd2_load_dat_resource(0x51a4d,
            data_fd2_graphics_animated_bg_buffer_ptr, idx_base + 1);
        data_fd2_graphics_animated_bg_buffer_ptr = bot_sprite;
        fd2_rle_blit_sprite(bot_sprite, 0, (int32)bg_height / 2,
            data_fd2_graphics_static_bg_buffer_ptr, bg_width, 0xffffffff);

        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
        data_fd2_graphics_animated_bg_buffer_ptr = 0;
        return;
    } else if (data_fd2_chapter_current_chapter_id == 0x17) {
        data_fd2_graphics_static_bg_buffer_ptr = (uint32)malloc(0xea00);

        data_fd2_graphics_animated_bg_buffer_ptr = fd2_load_dat_resource(
            0x51a4d, data_fd2_graphics_animated_bg_buffer_ptr, 0x2a);
        fd2_rle_blit_sprite(data_fd2_graphics_animated_bg_buffer_ptr, 0, 0,
            data_fd2_graphics_static_bg_buffer_ptr, 0x138, 0xffffffff);

        free((void *)data_fd2_graphics_animated_bg_buffer_ptr);
        data_fd2_graphics_animated_bg_buffer_ptr = 0;
        fd2_scroll_text_screen_up_by_lines(0);
        return;
    } else if (data_fd2_chapter_current_chapter_id != 0x1c &&
               data_fd2_chapter_current_chapter_id != 0x1d) {
        data_fd2_graphics_animated_bg_buffer_ptr = 0;
        return;
    } else {
        fdother_idx = 0x37;
    }

    data_fd2_graphics_static_bg_buffer_ptr = fd2_load_dat_resource(
        0x51a4d, data_fd2_graphics_static_bg_buffer_ptr, fdother_idx);
    data_fd2_graphics_animated_bg_buffer_ptr = (uint32)malloc(64000);
}

/* ----------------------------------------------------------------
 * fd2_load_portrait_to_cache @ 0x11019  (8 callers)
 *
 * Load a portrait's 12-sprite frames from FDICON.B24 into the global
 * 200KB cache. Idempotent on repeat calls (returns existing idx).
 *
 * Cache structure (200KB buffer at portrait_sprite_cache):
 *   [0 .. 0x77F]  sprite-offset lookup table: 40-portrait capacity
 *                 (40 x 12 sprites x 4-byte abs-offset)
 *   [0x780 ..]    packed sprite data, sequential per portrait
 *   portrait_cache_buffer_used (@0x539EC) = current data end-of-buffer
 *   portrait_cache_count       (@0x53BDF) = number of cached portraits
 *   portrait_cache_id_list[N]  (@0x53B17) = portrait_id of N-th cached
 *                                           entry (parallel to slots)
 *
 * Reads the 6720-byte FDICON.B24 sprite-header table (skipping the
 * 6-byte magic), extracts the 13 ints for this portrait (12 frame
 * offsets + 1 trailing end-mark), then either first-time-inits the
 * cache or appends at the tail. A cache hit (portrait_id already
 * present) short-circuits with no I/O.
 * ---------------------------------------------------------------- */
int fd2_load_portrait_to_cache(uint32 portrait_id, uint32 fp)
{
    int32  sprite_offsets[13];
    uint32 data_size;
    void  *hdr_buf;
    int    i;
    int    cache_idx;

    fseek((void *)fp, 6, SEEK_SET);
    hdr_buf = malloc(0x1a40);
    fread(hdr_buf, 1, 0x1a40, (void *)fp);
    for (i = 0; i < 0xd; i++) {
        sprite_offsets[i] =
            *(int32 *)((uint8 *)hdr_buf + (portrait_id * 0xc + i) * 4);
    }
    data_size = (uint32)(sprite_offsets[12] - sprite_offsets[0]);
    free(hdr_buf);

    if (data_fd2_resource_portrait_cache_count == 0) {
        *(uint32 *)data_fd2_resource_portrait_cache_id_list_base = portrait_id;
        portrait_sprite_cache = (uint32)malloc(0x32a00);
        fseek((void *)fp, sprite_offsets[0], SEEK_SET);
        fread((void *)(portrait_sprite_cache + 0x780), 1, data_size,
              (void *)fp);
        for (i = 0; i < 0xc; i++) {
            *(int32 *)(portrait_sprite_cache + i * 4) =
                (sprite_offsets[i] - sprite_offsets[0]) + 0x780;
        }
        data_fd2_resource_portrait_cache_buffer_used = data_size + 0x780;
        cache_idx = 0;
    } else {
        for (i = 0; i < (int)data_fd2_resource_portrait_cache_count; i++) {
            if (portrait_id ==
                *(uint32 *)(data_fd2_resource_portrait_cache_id_list_base
                            + i * 4)) {
                return i;
            }
        }
        *(uint32 *)(data_fd2_resource_portrait_cache_id_list_base + i * 4) =
            portrait_id;
        fseek((void *)fp, sprite_offsets[0], SEEK_SET);
        fread((void *)(portrait_sprite_cache
                       + data_fd2_resource_portrait_cache_buffer_used),
              1, data_size, (void *)fp);
        for (i = 0; i < 0xc; i++) {
            *(int32 *)(portrait_sprite_cache
                       + (data_fd2_resource_portrait_cache_count * 0xc + i) * 4)
                = (int32)(data_fd2_resource_portrait_cache_buffer_used
                          + (sprite_offsets[i] - sprite_offsets[0]));
        }
        data_fd2_resource_portrait_cache_buffer_used += data_size;
        cache_idx = (int)data_fd2_resource_portrait_cache_count;
    }

    data_fd2_resource_portrait_cache_count++;
    return cache_idx;
}

/* ----------------------------------------------------------------
 * fd2_load_chapter_battle_data @ 0x1088d  (3 callers)
 *
 * CRITICAL chapter init: loads all per-chapter battle data and builds
 * runtime_char_array for the upcoming battle. Callers:
 * fd2_init_battle_state_for_chapter, fd2_chapter_30_end,
 * fd2_play_final_chapter_30_ending.
 *
 * Pipeline:
 *   1. fd2_load_chapter_background_layers()
 *   2. FDTXT.DAT[chapter+1] -> current_chapter_text
 *   3. FDFIELD.DAT[chapter*3 + 2/1/0] -> chapter_portrait_load_buffer /
 *      tile_event_data_table / battle_tile_map
 *   4. map width/height = tile_map[0]/[2] (16-bit)
 *   5. FDSHAP.DAT[tile_event[0]*2 (+1)] -> battle_scene_snapshot /
 *      tile_attribute_flags_buffer
 *   6. fd2_obfuscate_battle_tile_map(battle_tile_map)
 *   7. cache_total_size=tile_event[1], cache_alloc_offset=tile_event[2],
 *      party_member_count=cache_total_size
 *   8. free portrait_sprite_cache + old runtime_char_array
 *   9. runtime_char_array = malloc(0x1E00)  (96 chars x 0x50)
 *  10. fopen("FDICON.B24","rb")
 *  11. per slot (0..cache_total_size): build active player units from the
 *      shared menu party roster template, or zero+mark dead for empty slots
 *  12. fclose; free chapter_portrait_load_buffer
 *  13. fd2_load_chapter_portraits_and_dump_tmp(0)
 *
 * malloc/fopen failure -> INT 10h text-mode reset + printf + exit.
 *
 * The normal-path tail falls through into the shared epilogue fragment
 * fd2_noop_stub_b43 @ 0x10b43 (ADD ESP / POP regs / RET); emitted here as
 * a plain `return` and regenerated by the compiler. See emit_issues.json.
 * ---------------------------------------------------------------- */
void fd2_load_chapter_battle_data(uint32 chapter_id)
{
    uint32 active_count;
    uint32 fdfield_x3;
    uint8  scene_id;
    runtime_char *dst;
    void  *fp;
    uint8 *field_pos;
    uint8 *template_ptr;
    uint32 char_iter;

    active_count = 0;
    fd2_load_chapter_background_layers();

    current_chapter_text = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdtxt_dat,
        current_chapter_text, chapter_id + 1);

    fdfield_x3 = chapter_id * 3;
    chapter_portrait_load_buffer = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdfield_dat_51a59,
        chapter_portrait_load_buffer, fdfield_x3 + 2);
    data_fd2_tile_event_data_table_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdfield_dat_51a59,
        data_fd2_tile_event_data_table_ptr, fdfield_x3 + 1);
    data_fd2_battle_tile_map_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdfield_dat_51a59,
        data_fd2_battle_tile_map_ptr, fdfield_x3);

    data_fd2_battle_map_width_tiles =
        (int)*(int16 *)data_fd2_battle_tile_map_ptr;
    data_fd2_battle_map_height_tiles =
        (int)*(int16 *)(data_fd2_battle_tile_map_ptr + 2);

    scene_id = *(uint8 *)data_fd2_tile_event_data_table_ptr;
    battle_scene_snapshot = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
        battle_scene_snapshot, (uint32)scene_id * 2);
    data_fd2_tile_attribute_flags_buffer_ptr = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdshap_dat_51a65,
        data_fd2_tile_attribute_flags_buffer_ptr, (uint32)scene_id * 2 + 1);

    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);

    data_fd2_resource_portrait_cache_total_size =
        (uint32)((uint8 *)data_fd2_tile_event_data_table_ptr)[1];
    data_fd2_resource_portrait_cache_alloc_offset =
        (uint32)((uint8 *)data_fd2_tile_event_data_table_ptr)[2];
    data_fd2_battle_party_member_count =
        data_fd2_resource_portrait_cache_total_size;

    if (portrait_sprite_cache != 0)
        free((void *)portrait_sprite_cache);
    if (data_fd2_battle_runtime_char_array_ptr != NULL)
        free(data_fd2_battle_runtime_char_array_ptr);

    dst = (runtime_char *)malloc(0x1E00);
    data_fd2_battle_runtime_char_array_ptr = dst;
    if (dst == NULL) {
        *(uint16 *)&data_fd2_input_last_key_pressed = 3;
        int386(0x10, (union REGS *)&data_fd2_input_last_key_pressed,
                     (union REGS *)&data_fd2_input_last_key_pressed);
        printf(data_fd2_string_field_map_oom_msg_chapter_runtime_50064);
        exit(1);
    }

    data_fd2_resource_portrait_cache_count = 0;
    fp = fopen("FDICON.B24", "rb");
    if (fp == NULL) {
        *(uint16 *)&data_fd2_input_last_key_pressed = 3;
        int386(0x10, (union REGS *)&data_fd2_input_last_key_pressed,
                     (union REGS *)&data_fd2_input_last_key_pressed);
        printf(data_fd2_string_field_map_fdicon_not_found_err_50086);
        exit(1);
    }

    field_pos = (uint8 *)(chapter_portrait_load_buffer
        + data_fd2_resource_portrait_cache_alloc_offset * 6 + 2);
    template_ptr = (uint8 *)data_fd2_shared_menu_party_roster_buffer_ptr;

    for (char_iter = 0;
         (int)char_iter < (int)data_fd2_resource_portrait_cache_total_size;
         char_iter++) {
        if (((int)chapter_id < 0xd && char_iter == 6
             && template_ptr[8] != 2)
            || (int)data_fd2_shared_menu_party_member_count
                   <= (int)active_count) {
            memset(dst, 0, 0x50);
            dst->flags = 1;
        } else {
            int slot;

            memmove(dst, template_ptr, 0x50);
            dst->pos_x = field_pos[0];
            dst->pos_y = field_pos[2];
            field_pos += 6;
            slot = fd2_load_portrait_to_cache((uint32)dst->portrait_id,
                                              (uint32)fp);
            dst->sprite_state[0] = (uint8)slot;
            dst->sprite_state[1] = 0;
            dst->sprite_state[2] = 0;
            dst->team = 2;
            dst->combat_aux_block[10] = 0xff;
            memset(dst->status_flags_block + 1, 0, 6);
            fd2_recalculate_combat_stats(char_iter);
            template_ptr += 0x50;
            active_count++;
        }
        dst++;
    }

    fclose(fp);
    free((void *)chapter_portrait_load_buffer);
    chapter_portrait_load_buffer = 0;
    fd2_load_chapter_portraits_and_dump_tmp(0);
}

/* ----------------------------------------------------------------
 * fd2_load_chapter_portraits_and_dump_tmp @ 0x10b4e  (~52 callers)
 *
 * Portrait loader + FD2.TMP swap-file writer. Used by chapter init/end
 * paths and many chapter event handlers.
 *
 * Flow:
 *   1. fopen("FDICON.B24", "rb"); if NULL -> INT 10h text-mode reset +
 *      printf("File 'FDICON.B24' error !!\n") + exit.
 *   2. Re-read FDFIELD.DAT[current_chapter_id*3 + 2] into
 *      chapter_portrait_load_buffer (the buffer was freed by
 *      fd2_load_chapter_battle_data after its own load).
 *   3. For each entry in the tile-event table (count =
 *      portrait_cache_alloc_offset, stride 0x1A, race byte at +0x98):
 *      if race == target_race_id, call fd2_init_runtime_char_for_battle
 *      to populate a runtime_char + load its portrait from FDICON.B24.
 *   4. fclose(FDICON); free + null chapter_portrait_load_buffer.
 *   5. fopen("FD2.TMP", "wb"); fwrite(portrait_sprite_cache, 1, 0x32A00);
 *      fclose. FD2.TMP is the cross-chapter sprite swap file, refreshed
 *      (truncated + rewritten) after each portrait load.
 *
 * The disassembled error path tail-jumps into _main's shared
 * printf("%s") + exit(1) stub at 0x10056; emitted inline here to match
 * the fd2_load_chapter_battle_data idiom.
 * ---------------------------------------------------------------- */
void fd2_load_chapter_portraits_and_dump_tmp(uint32 target_race_id)
{
    void  *fp;
    uint32 iter;
    uint32 char_race;

    fp = fopen("FDICON.B24", "rb");
    if (fp == NULL) {
        *(uint16 *)&data_fd2_input_last_key_pressed = 3;
        int386(0x10, (union REGS *)&data_fd2_input_last_key_pressed,
                     (union REGS *)&data_fd2_input_last_key_pressed);
        printf("File 'FDICON.B24' error !!\n");
        exit(1);
    }

    chapter_portrait_load_buffer = fd2_load_dat_resource(
        (uint32)data_fd2_string_resource_filename_fdfield_dat_51a59,
        chapter_portrait_load_buffer,
        data_fd2_chapter_current_chapter_id * 3 + 2);

    for (iter = 0;
         (int)iter < (int)data_fd2_resource_portrait_cache_alloc_offset;
         iter++) {
        char_race = (uint32)*(uint8 *)(data_fd2_tile_event_data_table_ptr
                                       + iter * 0x1a + 0x98);
        if (char_race == target_race_id) {
            fd2_init_runtime_char_for_battle(iter, (uint32)fp);
        }
    }

    fclose(fp);
    free((void *)chapter_portrait_load_buffer);
    chapter_portrait_load_buffer = 0;

    fp = fopen("FD2.TMP", "wb");
    fwrite((void *)portrait_sprite_cache, 1, 0x32a00, fp);
    fclose(fp);
}
