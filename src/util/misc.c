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
