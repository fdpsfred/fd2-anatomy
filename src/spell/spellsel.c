/*
 * spellsel.c — Spell-selection support
 *
 * Functions:
 *   fd2_build_usable_spell_list @ 0x1c269 (8 call sites in 7 functions)
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
