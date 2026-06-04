/*
 * spellsel.c — Spell-selection support
 *
 * Functions:
 *   fd2_build_usable_spell_list @ 0x1c269 (8 call sites in 7 functions)
 *   fd2_draw_spell_selection_list @ 0x1ceed (3 callers)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

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
int fd2_build_usable_spell_list(uint32 ci, uint32 buf)
{
    runtime_char *pCharArray;
    uint8 bitmap_byte;
    uint32 byte_iter;
    uint32 bit_iter;
    int spell_count;

    pCharArray = data_fd2_battle_runtime_char_array_ptr;
    spell_count = 0;

    for (byte_iter = 0; (int)byte_iter < 5; byte_iter++) {
        bitmap_byte = pCharArray[ci].spells_known_bitmap[byte_iter];
        for (bit_iter = 0; (int)bit_iter < 8; bit_iter++) {
            if (((bitmap_byte >> bit_iter) & 1) != 0) {
                if (buf != 0) {
                    *(uint8 *)(buf + spell_count) =
                        (uint8)(byte_iter * 8 + bit_iter);
                }
                spell_count++;
            }
        }
    }

    return spell_count;
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
