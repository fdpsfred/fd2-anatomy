/*
 * promote.c — Church-revive and class-promotion menu helpers
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_build_dead_chars_list_for_revive @ 0x309FF  (1 caller)
 *
 * Build a list of party members who are CURRENTLY DEAD (eligible
 * for the revive-at-church service). For each char idx in
 * 0..menu_party_member_count-1: if fd2_check_char_is_dead(idx)
 * returns 1 (runtime_char[idx].bFlags & 1), append idx to
 * out_list_buf. Returns the count of dead chars (in EAX).
 *
 * Sole caller: fd2_run_revive_menu_main @ 0x30DC3 (mid-game town
 * typeC option 2 = 復活).
 *
 * int __cdecl with the __CHK(0x14) stack-probe prologue (compiler-
 * injected, not part of the source). EBX is the loop counter / char
 * index (callee-saved); ESI holds out_list_buf; the trailing POP ESI
 * / POP EBX / RET is the shared epilogue. count is a single-byte
 * local (stored / read via byte ops) returned zero-extended.
 * ---------------------------------------------------------------- */
int fd2_build_dead_chars_list_for_revive(uint8 *out_list_buf)
{
    uint8 count;
    uint32 idx;

    count = 0;
    for (idx = 0; (int)idx < (int)data_fd2_shared_menu_party_member_count; idx++) {
        if (fd2_check_char_is_dead(idx) == 1) {
            out_list_buf[count] = (uint8)idx;
            count++;
        }
    }
    return (int)count;
}
