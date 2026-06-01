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
