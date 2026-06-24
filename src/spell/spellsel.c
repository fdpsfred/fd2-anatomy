/*
 * spellsel.c — Spell-selection support
 *
 * Functions:
 *   fd2_build_usable_spell_list @ 0x1c269 (8 call sites in 7 functions)
 *   fd2_grant_spell_to_char @ 0x1d79c (1 caller)
 *   fd2_draw_spell_selection_list @ 0x1ceed (3 callers)
 *   fd2_spell_select_input_loop @ 0x1d51d (1 caller)
 *   fd2_spell_selection_menu_main @ 0x1cff0 (1 caller)
 *   fd2_play_spell_palette_flash_with_sfx @ 0x1d6c8 (1 caller)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <string.h>
#include <conio.h>

/* ----------------------------------------------------------------
 * fd2_build_usable_spell_list(char_idx, out_buf) @ 0x1c269
 *
 * Enumerate every spell the runtime character has learned. Walks the
 * 5-byte spells_known_bitmap (40 slots, of which only ids 0x00..0x23
 * are real spells) and, for each set bit, emits spell_id = byte*8 + bit.
 *
 * out_buf == NULL -> only the count is returned (used to size a buffer
 * before allocating). Pure enumeration: no MP / usability check here;
 * those live in the callers.
 *
 * Cdecl, 2 stack params; returns int count. The binary's __CHK(0x18)
 * stack-probe prologue is compiler-injected and not part of the source;
 * the body's tail `JMP 0x22bbe` is Watcom's shared epilogue.
 * ---------------------------------------------------------------- */
int fd2_build_usable_spell_list(uint32 char_idx, uint32 out_buf)
{
    runtime_char *pCharArray;
    uint8 bitmap_byte;
    uint32 byte_iter;
    uint32 bit_iter;
    int spell_count;

    pCharArray = data_fd2_battle_runtime_char_array_ptr;
    spell_count = 0;

    for (byte_iter = 0; (int)byte_iter < 5; byte_iter++) {
        bitmap_byte = pCharArray[char_idx].spells_known_bitmap[byte_iter];
        for (bit_iter = 0; (int)bit_iter < 8; bit_iter++) {
            if (((bitmap_byte >> bit_iter) & 1) != 0) {
                if (out_buf != 0) {
                    *(uint8 *)(out_buf + spell_count) =
                        (uint8)(byte_iter * 8 + bit_iter);
                }
                spell_count++;
            }
        }
    }

    return spell_count;
}

/* Single-bit mask per bit index: data_fd2_battle_spell_bit_mask_lookup_table
 * @ 0x52024, eight bytes {1,2,4,8,0x10,0x20,0x40,0x80} = 1 << n. Only
 * fd2_grant_spell_to_char reads it (it copies the table to a local before the
 * indexed read, mirroring the machine code's MOVSD-to-stack). */
static const uint8 data_fd2_battle_spell_bit_mask_lookup_table[8] = {
    1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80
};

/* ----------------------------------------------------------------
 * fd2_grant_spell_to_char(char_idx, spell_id) @ 0x1d79c  (1 caller)
 *
 * Mark spell_id as learned for the runtime char at char_idx by setting its
 * bit in the 5-byte spells_known_bitmap (+0x1A, the same bitmap enumerated by
 * fd2_build_usable_spell_list):
 *
 *   spells_known_bitmap[spell_id / 8] |= 1 << (spell_id % 8)
 *
 * spell_id is 0x00..0x23 (36 spells; bits 36..39 unused). Called from
 * fd2_process_xp_and_level_up_for_char when a level-up grants a spell.
 *
 * The binary builds the OR mask by indexing a local copy of the 8-byte
 * bit-mask table with spell_id % 8; since the table is {1<<0 .. 1<<7} that is
 * exactly 1 << (spell_id % 8). The byte offset and bit index come from a signed
 * div/mod by 8 in the machine code, but spell_id is always a small non-negative
 * id, so plain /8 and %8 reproduce it.
 *
 * Cdecl, 2 stack params; void return. The binary's __CHK(0x20) stack-probe
 * prologue is compiler-injected and not part of the source; the body's tail
 * `JMP 0x114ff` is Watcom's shared epilogue.
 * ---------------------------------------------------------------- */
void fd2_grant_spell_to_char(uint32 char_idx, uint32 spell_id)
{
    uint8 bit_mask_table[8];
    runtime_char *pChar;
    uint32 byte_off;

    bit_mask_table[0] = data_fd2_battle_spell_bit_mask_lookup_table[0];
    bit_mask_table[1] = data_fd2_battle_spell_bit_mask_lookup_table[1];
    bit_mask_table[2] = data_fd2_battle_spell_bit_mask_lookup_table[2];
    bit_mask_table[3] = data_fd2_battle_spell_bit_mask_lookup_table[3];
    bit_mask_table[4] = data_fd2_battle_spell_bit_mask_lookup_table[4];
    bit_mask_table[5] = data_fd2_battle_spell_bit_mask_lookup_table[5];
    bit_mask_table[6] = data_fd2_battle_spell_bit_mask_lookup_table[6];
    bit_mask_table[7] = data_fd2_battle_spell_bit_mask_lookup_table[7];

    pChar = data_fd2_battle_runtime_char_array_ptr + char_idx;
    byte_off = (int)spell_id / 8;
    pChar->spells_known_bitmap[byte_off] =
        pChar->spells_known_bitmap[byte_off] |
        bit_mask_table[(int)spell_id % 8];
}

/* ----------------------------------------------------------------
 * fd2_draw_spell_selection_list(caster_idx, highlighted_idx, render_buf)
 *   @ 0x1ceed
 *
 * Draw the battle spell-picker as a 4-column list into render_buf.
 * fd2_build_usable_spell_list enumerates the caster's learned spell ids
 * into a local array, and each one is laid out into a grid cell:
 *
 *   row_pixel       = (spell_iter % 4) * 0x16          (4 spells per row)
 *   col_addr_offset = render_buf + (spell_iter / 4) * 100 + 0x12
 *   color           = (spell_iter == highlighted_idx) ? 0xC9 : 0xCD
 *                     (highlighted: yellow / normal: red)
 *
 * Per cell three things are drawn:
 *   - spell name text: page id = spell_id + 0x1B9 into _all_game_text,
 *     at (row_pixel + 0x67) * 320 + col_addr_offset, glyph border = color.
 *   - MP icon sprite 0x5C from the ui/anim sheet, at
 *     col_addr_offset + 0x32 + (row_pixel + 0x6C) * 320.
 *   - MP cost as a 2-digit number, read from the spell effect record
 *     (*(byte*)(pSpell + 5)), at col_addr_offset + 0x49 + (row_pixel +
 *     0x6C) * 320.
 *
 * "usable" is a conventional name: build_usable_spell_list lists only
 * learned spells; whether MP is sufficient (selectable vs greyed) is
 * checked live by fd2_spell_select_input_loop.
 *
 * The spell_iter / 4 and % 4 are signed divisions in the binary; spell_iter
 * is a non-negative loop counter so the (int) casts only pin the codegen.
 *
 * Cdecl, 3 stack params; void return. The binary's __CHK(0x5C) stack-probe
 * prologue is compiler-generated and omitted here.
 * ---------------------------------------------------------------- */
void fd2_draw_spell_selection_list(uint32 caster_idx, uint32 highlighted_idx,
                                   uint32 render_buf)
{
    uint8  spell_id_list[32];
    int    spell_count;
    int    spell_iter;
    uint32 row_pixel;
    uint32 color;
    uint32 col_addr_offset;
    uint32 row_y;
    uint8 *pSpell;

    spell_count = fd2_build_usable_spell_list(caster_idx, (uint32)spell_id_list);

    for (spell_iter = 0; spell_iter < spell_count; spell_iter = spell_iter + 1) {
        row_pixel = (spell_iter % 4) * 0x16;
        color = ((uint32)spell_iter == highlighted_idx) ? 0xc9 : 0xcd;
        col_addr_offset = render_buf + (spell_iter / 4) * 100 + 0x12;

        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr, spell_id_list[spell_iter] + 0x1b9,
            (row_pixel + 0x67) * 0x140 + col_addr_offset, 0x140, color,
            0x4c, 0, 0, 0);

        row_y = (row_pixel + 0x6c) * 0x140;
        fd2_blit_sheet_sprite_at_offset(col_addr_offset + 0x32 + row_y, 0x140,
                                        data_fd2_ui_anim_sprite_sheet_ptr,
                                        0x5c);

        pSpell = fd2_get_spell_effect_entry((int)spell_id_list[spell_iter]);
        fd2_render_decimal_number_to_buffer(col_addr_offset + 0x49 + row_y,
                                            0x140, (uint32)pSpell[5], 0x2a, 2);
    }
}

/* ----------------------------------------------------------------
 * fd2_spell_select_input_loop(caster_idx) @ 0x1d51d  (1 caller)
 *
 * One frame of battle spell-picker selection input, called repeatedly by
 * fd2_spell_selection_menu_main until it returns non-zero. Redraw the list
 * with the cursor highlighted, get the learned-spell count, wait for a key,
 * then dispatch on the scancode:
 *
 *   0x48 Up:    cursor != 0 -> cursor--; else cursor = spell_count - 1 (wrap).
 *               SFX, return 0.
 *   0x50 Down:  cursor != spell_count-1 -> cursor++; else cursor = 0 (wrap).
 *               SFX, return 0.
 *   0x4B Left:  cursor >= 4 -> cursor -= 4, SFX, return 0; else no move.
 *   0x4D Right: cursor < spell_count-4 -> cursor += 4, SFX, return 0; else
 *               no move.
 *   0x1C/0x39 Enter/Space: rebuild the full id list; if the picked spell's
 *               MP cost (spell record +5) <= caster.mp_current return 1
 *               (commit at current_menu_cursor_idx), else stay (return 0).
 *   0x01 Esc:   return -1.
 *   other:      return 0.
 *
 * The cursor lives in the shared data_fd2_ui_menu_cursor_idx (0x53C57); the
 * caster's runtime_char is data_fd2_battle_runtime_char_array_ptr[caster_idx]
 * (stride 0x50, mp_current @ +0x44). The Left/Right "no move" branches and
 * Up/Down wrap both fall to the shared return 0. The post-CALL MP-cost read
 * (*(byte*)(pSpell+5)) and the SHL-by-3-then-mul layout follow the machine
 * code; spell_count-1 / spell_count-4 are precomputed (EBX/EDI/EBX-4 in the
 * binary) and re-expressed here as plain expressions.
 *
 * Cdecl, 1 stack param; returns int (1 commit / 0 stay / -1 cancel). The
 * binary's __CHK(0x28) stack-probe prologue is compiler-injected and not part
 * of the source; the body's tail `JMP 0x16f04` is Watcom's shared epilogue.
 * ---------------------------------------------------------------- */
int fd2_spell_select_input_loop(uint32 caster_idx)
{
    runtime_char *pCharArray;
    uint8  spell_id_list[12];
    uint32 spell_count;
    uint32 scancode;
    uint16 caster_MP;
    uint8 *pSpell;

    fd2_draw_spell_selection_list(caster_idx, data_fd2_ui_menu_cursor_idx,
                                  0xa0000);
    pCharArray = data_fd2_battle_runtime_char_array_ptr;
    spell_count = (uint32)fd2_build_usable_spell_list(caster_idx, 0);
    scancode = (uint32)(uint8)fd2_wait_for_input_dialog_with_blink(0);

    if (scancode == 0x48) {
        if (data_fd2_ui_menu_cursor_idx != 0) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 1;
            return 0;
        }
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
        data_fd2_ui_menu_cursor_idx = spell_count - 1;
    }
    else if (scancode == 0x50) {
        if (spell_count - 1 != data_fd2_ui_menu_cursor_idx) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1;
            return 0;
        }
        fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
        data_fd2_ui_menu_cursor_idx = 0;
    }
    else if (scancode == 0x4b) {
        if (3 < (int)data_fd2_ui_menu_cursor_idx) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 4;
            return 0;
        }
    }
    else if (scancode == 0x4d) {
        if ((int)data_fd2_ui_menu_cursor_idx < (int)(spell_count - 4)) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 4;
            return 0;
        }
    }
    else if (scancode == 0x1c || scancode == 0x39) {
        fd2_build_usable_spell_list(caster_idx, (uint32)spell_id_list);
        caster_MP = pCharArray[caster_idx].mp_current;
        pSpell = fd2_get_spell_effect_entry(
            (int)spell_id_list[data_fd2_ui_menu_cursor_idx]);
        if (pSpell[5] <= caster_MP) {
            return 1;
        }
    }
    else if (scancode == 1) {
        return -1;
    }

    return 0;
}

/* ----------------------------------------------------------------
 * fd2_spell_selection_menu_main(caster_idx) @ 0x1cff0  (1 caller)
 *
 * Battle in-combat spell-selection modal (main entry). Opened by
 * fd2_player_inline_action_menu_dispatch when the player picks "Spell".
 *
 * Setup (same three-buffer layout as the status overview screen):
 *   alloc accumulator/snapshot/composed buffers (64000 each), snapshot
 *   the VGA frame at 0xA0000 into snapshot, copy into composed, render
 *   the static status layout + the spell list (highlight = -1) into it,
 *   then a 12-frame slide-in (frame 11..0) via the outro-step driver.
 *
 * Input loop: repeat fd2_spell_select_input_loop(caster_idx) until it
 * returns non-zero (1 = commit at current_menu_cursor_idx, -1 = Esc).
 *
 * Teardown: 12-frame slide-out (frame 0..0xB), restore the VGA snapshot
 * to 0xA0000, free the three buffers. On Esc (loop == -1) return -1.
 *
 * Spell dispatch (loop != -1):
 *   pSpell        = fd2_get_spell_effect_entry(spell_id_list[cursor])
 *   battle phase  = pSpell[4] + 2  (animation freeze period)
 *   aoe_kind      = pSpell[3]      (0 = single, !=0 = AoE)
 *   spell_id      = spell_id_list[cursor]
 *
 * Target-selection branch:
 *   - aoe_kind != 0 && spell_id == 0x17 (teleport-class): two-stage
 *     target pick (source range, then a free destination via wait mode
 *     6); on commit, record the teleport destination world coords and
 *     pan the cursor.
 *   - aoe_kind == 0 || spell_id == 0x17 (single target): pick within the
 *     spell range (wait mode 5 when no targets in range, else 4).
 *   - else (AoE, aoe_kind != 0 && spell_id != 0x17): pick the AoE anchor
 *     (wait mode = pSpell[6]); spell 0x1E uses a line scan to resolve the
 *     final hit list, all others a second AoE compute pass.
 *
 * On cancel (result == -1): restore the cursor, return 0.
 * On hit: composite the frame, then either
 *   - spell_id < 9 || == 0x18 || > 0x1B: fd2_play_spell_cast_sequence
 *     (ordinary attack / heal / full-party spells), or
 *   - 9 <= spell_id <= 0x1B && != 0x18 (status/special spells): load the
 *     status SFX, play the palette flash, invoke the per-spell status
 *     handler in data_fd2_battle_spell_handler_table[spell_id]
 *     (called with caster_idx, hit count, target buffer), free the SFX.
 * Then collect/animate/process death drops and return 1.
 *
 * Returns: 1 = cast performed, 0 = target cancelled, -1 = menu cancelled.
 *
 * Cdecl, 1 stack param. The binary's __CHK(0x108) stack-probe prologue is
 * compiler-injected and not part of the source.
 *
 * NOTE: the three "dropped-argument" CALLs flagged by the decompiler are
 * reconstructed from the disassembly here -- the per-spell status handler
 * table call passes (caster_idx, n_targets, target_buf), and the cast /
 * drops calls take the live target/drop counts -- matching the machine code.
 * ---------------------------------------------------------------- */
int fd2_spell_selection_menu_main(uint32 caster_idx)
{
    uint8 *pSpell;
    uint32 aoe_kind;
    uint32 spell_id;
    uint32 aoe_target_count;
    uint32 n_targets;
    uint32 drops_count;
    int    result_or_aoe;
    int    loop_result;
    int    wait_mode;
    uint32 frame;
    uint32 saved_cursor_x;
    uint32 saved_cursor_y;
    uint8  target_buf[100];
    uint8  drops_buf[100];
    uint8  spell_id_list[12];

    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);

    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr,
            (void *)0xa0000, 64000);
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    fd2_render_status_screen_static_layout(
        caster_idx, data_fd2_ui_slide_composed_target_buf_ptr);
    fd2_draw_spell_selection_list(
        caster_idx, 0xffffffff, data_fd2_ui_slide_composed_target_buf_ptr);

    for (frame = 0xb; (int)frame >= 0; frame = frame - 1) {
        fd2_render_status_screen_slide_frame(
            frame, data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr,
            (int)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    }

    data_fd2_ui_menu_cursor_idx = 0;
    do {
        loop_result = fd2_spell_select_input_loop(caster_idx);
    } while (loop_result == 0);

    for (frame = 0; (int)frame < 0xc; frame = frame + 1) {
        fd2_render_status_screen_slide_frame(
            frame, data_fd2_ui_slide_anim_accumulator_buf_ptr,
            data_fd2_ui_slide_composed_target_buf_ptr,
            (int)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    }
    memmove((void *)0xa0000,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
    free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
    free((void *)data_fd2_ui_slide_composed_target_buf_ptr);

    if (loop_result == -1) {
        return -1;
    }

    fd2_build_usable_spell_list(caster_idx, (uint32)spell_id_list);
    pSpell = fd2_get_spell_effect_entry(
        (int)spell_id_list[data_fd2_ui_menu_cursor_idx]);
    data_fd2_battle_anim_phase = (uint32)pSpell[4] + 2;
    aoe_kind = (uint32)pSpell[3];
    spell_id = (uint32)spell_id_list[data_fd2_ui_menu_cursor_idx];

    if (aoe_kind != 0 && spell_id == 0x17) {
        /* teleport-class: source range pick, then free destination pick */
        aoe_target_count = fd2_compute_aoe_targets(
            data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y,
            (uint32)target_buf, aoe_kind, 1, (uint32)pSpell[6]);
        result_or_aoe = fd2_wait_for_action_target_input(
            (int)pSpell[6], aoe_target_count, target_buf);
        fd2_battle_reset_tile_transient_state(data_fd2_battle_tile_map_ptr);

        n_targets = fd2_compute_aoe_targets(
            data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y,
            (uint32)target_buf, (uint32)pSpell[4], 0, (uint32)pSpell[6]);
        fd2_battle_reset_tile_transient_state(data_fd2_battle_tile_map_ptr);

        if (result_or_aoe != -1) {
            result_or_aoe = fd2_wait_for_action_target_input(
                6, (uint32)target_buf[0], (uint8 *)0);
        }
        if (result_or_aoe != -1) {
            data_fd2_battle_teleport_dest_world_x =
                data_fd2_battle_cursor_world_x;
            data_fd2_battle_teleport_dest_world_y =
                data_fd2_battle_cursor_world_y;
            data_fd2_battle_anim_phase = 0;
            fd2_pan_cursor_to_char(caster_idx);
            data_fd2_battle_anim_phase = 1;
        }
    }
    else if (aoe_kind == 0 || spell_id == 0x17) {
        /* single target */
        data_fd2_battle_anim_phase = 1;
        n_targets = fd2_compute_aoe_targets(
            data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y,
            (uint32)target_buf, (uint32)pSpell[4], 0, 0);
        wait_mode = 4;
        if (n_targets == 0) {
            wait_mode = 5;
        }
        result_or_aoe = fd2_wait_for_action_target_input(
            wait_mode, 0, target_buf);
    }
    else {
        /* AoE (aoe_kind != 0 && spell_id != 0x17) */
        aoe_target_count = fd2_compute_aoe_targets(
            data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y,
            (uint32)target_buf, aoe_kind, 0, (uint32)pSpell[6]);
        saved_cursor_x = data_fd2_battle_cursor_world_x;
        saved_cursor_y = data_fd2_battle_cursor_world_y;
        result_or_aoe = fd2_wait_for_action_target_input(
            (int)pSpell[6], aoe_target_count, target_buf);
        fd2_battle_reset_tile_transient_state(data_fd2_battle_tile_map_ptr);

        if (spell_id == 0x1e) {
            n_targets = fd2_scan_chars_along_line_with_team_filter(
                data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y,
                (uint32)target_buf, saved_cursor_x, saved_cursor_y,
                (uint32)(pSpell[3] - 0x10), 1);
        }
        else {
            n_targets = fd2_compute_aoe_targets(
                data_fd2_battle_cursor_world_x, data_fd2_battle_cursor_world_y,
                (uint32)target_buf, (uint32)pSpell[4], 0, (uint32)pSpell[6]);
        }
    }

    fd2_battle_reset_tile_transient_state(data_fd2_battle_tile_map_ptr);

    if (result_or_aoe == -1) {
        data_fd2_battle_anim_phase = 0;
        fd2_pan_cursor_to_char(caster_idx);
        data_fd2_battle_anim_phase = 1;
        return 0;
    }

    data_fd2_battle_anim_phase = 0;
    fd2_composite_battle_frame(0);
    spell_id = (uint32)spell_id_list[data_fd2_ui_menu_cursor_idx];
    if (spell_id < 9 || spell_id == 0x18 || spell_id > 0x1b) {
        fd2_play_spell_cast_sequence(
            caster_idx, spell_id, n_targets, (uint32)target_buf);
    }
    else {
        fd2_load_status_effect_sfx();
        fd2_play_spell_palette_flash_with_sfx((int)spell_id);
        data_fd2_battle_spell_handler_table[spell_id](
            caster_idx, n_targets, target_buf);
        fd2_stop_and_free_status_effect_sfx();
    }
    drops_count = fd2_collect_pending_death_drops((uint32)drops_buf);
    fd2_play_death_animation_and_mark_dead();
    fd2_process_battle_drop_entries(caster_idx, drops_count, (uint32)drops_buf);
    data_fd2_battle_anim_phase = 1;
    return 1;
}

/* ----------------------------------------------------------------
 * fd2_play_spell_palette_flash_with_sfx(spell_id) @ 0x1d6c8 (1 caller)
 *
 * Status-class spell cast effect: play the SFX that
 * fd2_load_status_effect_sfx loaded, then flash VGA DAC palette
 * index 0 four times in the spell's signature colour.
 *
 * Each of the 4 iterations: set DAC write index to 0 (port 0x3C8),
 * write the spell's R/G/B from data_fd2_animation_spell_palette_flash_table
 * (port 0x3C9 ×3), wait one BIOS tick, then reset DAC index 0 to
 * RGB(0,0,0) and wait one more tick.
 *
 * The flash table is a 108-byte (36 entry × 3) RGB table laid out as
 * three contiguous 36-byte planes: R at +0, G at +0x24, B at +0x48,
 * indexed by spell_id (0x00..0x23).
 *
 * Cdecl, 1 stack param; returns void. The binary's __CHK(0x18)
 * stack-probe prologue is compiler-injected and not part of the source.
 * ---------------------------------------------------------------- */
void fd2_play_spell_palette_flash_with_sfx(int spell_id)
{
    uint32 beep_iter;

    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0, 1);
    for (beep_iter = 0; (int)beep_iter < 4; beep_iter++) {
        outp(0x3C8, 0);
        outp(0x3C9,
             data_fd2_animation_spell_palette_flash_table[spell_id]);
        outp(0x3C9,
             data_fd2_animation_spell_palette_flash_table[spell_id + 0x24]);
        outp(0x3C9,
             data_fd2_animation_spell_palette_flash_table[spell_id + 0x48]);
        fd2_wait_n_bios_ticks(1);
        outp(0x3C8, 0);
        outp(0x3C9, 0);
        outp(0x3C9, 0);
        outp(0x3C9, 0);
        fd2_wait_n_bios_ticks(1);
    }
}
