/*
 * misc.c — Miscellaneous utility functions
 */

#include "types.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <string.h>
#include <conio.h>

/* ----------------------------------------------------------------
 * fd2_debug_print_ans_and_length @ 0x16F0B  (0 callers, dead code)
 *
 * Dev-time debug print: formats int as string, prints value and
 * string length, then waits for keypress. Left over from dev.
 * ---------------------------------------------------------------- */
void fd2_debug_print_ans_and_length(int value)
{
    char buf[20];
    sprintf(buf, "%d", value);
    printf(" Ans = %s,   Length = %d\n", buf, strlen(buf));
    getch();
}

/* ----------------------------------------------------------------
 * fd2_set_word_global_52758 @ 0x3615E
 *
 * Swap AIL alloc function pointer. Returns old value.
 * ---------------------------------------------------------------- */
uint32 fd2_set_word_global_52758(uint32 new_val)
{
    uint32 old;
    old = data_ail_alloc_fnptr;
    data_ail_alloc_fnptr = new_val;
    return old;
}

/* ----------------------------------------------------------------
 * fd2_set_word_global_5275c @ 0x3616E
 *
 * Swap AIL free function pointer. Returns old value.
 * ---------------------------------------------------------------- */
uint32 fd2_set_word_global_5275c(uint32 new_val)
{
    uint32 old;
    old = data_ail_free_fnptr;
    data_ail_free_fnptr = new_val;
    return old;
}

/* ----------------------------------------------------------------
 * fd2_any_char_has_item @ 0x24B14  (3 callers)
 *
 * Returns 1 if any character in runtime_char_array[0..15] holds the
 * given item_id, else -1. Scans chars 0..15, calling
 * fd2_find_inventory_slot_with_item(char_idx, item_id) on each; on the
 * first char whose search returns a slot (!= -1) it returns 1 at once,
 * otherwise -1 after all 16 chars miss. Used to detect plot-critical
 * items in party inventory (e.g. 天空之鑰 / item 100) for story branches:
 * fd2_chapter_23_end, fd2_chapter_27_end, fd2_chapter_27_init.
 * ---------------------------------------------------------------- */
int fd2_any_char_has_item(int item_id)
{
    int char_idx;

    for (char_idx = 0; char_idx < 0x10; char_idx++) {
        if (fd2_find_inventory_slot_with_item(char_idx, item_id) != -1) {
            return 1;
        }
    }
    return -1;
}

/* ----------------------------------------------------------------
 * fd2_find_template_char_by_id @ 0x24BDE  (1 caller)
 *
 * Linear-scans the menu/template party roster (buffer ptr at
 * data_fd2_shared_menu_party_roster_buffer_ptr, 0x50-byte stride,
 * count at data_fd2_shared_menu_party_member_count) for an entry
 * whose char_id byte at offset +0x08 equals char_id. Returns 1 on
 * the first match, 0 if the scan exhausts.
 *
 * Byte-identical duplicate of fd2_check_party_has_char_id @ 0x33499
 * (Watcom emitted the same body into two translation units). This
 * copy resides in the battle/spell address range.
 *
 * Caller: fd2_chapter_23_end uses it as the "蜜蒂 (char_id 0x12) is
 * in the party" predicate for the Phase-1 conditional joins.
 *
 * Cdecl, 1 stack param; int return. The binary's __CHK(8) stack-probe
 * prologue is compiler-generated and omitted here. EBX is callee-saved.
 * ---------------------------------------------------------------- */
int fd2_find_template_char_by_id(uint32 char_id)
{
    int idx;

    for (idx = 0; (int32)data_fd2_shared_menu_party_member_count > idx; idx++) {
        if (*(uint8 *)(idx * 0x50 + 8 +
                       data_fd2_shared_menu_party_roster_buffer_ptr) == char_id) {
            return 1;
        }
    }
    return 0;
}
