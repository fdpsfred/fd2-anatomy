/*
 * save.c — FD2 chapter-end persistence of battle-runtime char state.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_save_runtime_char_to_template @ 0x11506 (24 callers)
 *
 * Persist battle-runtime char state back into the menu/template
 * roster array at chapter end. For each runtime party member,
 * find the template entry with a matching char_id and copy the
 * whole 0x50-byte struct back, clearing transient status, dropping
 * non-dead flags, restoring HP (if alive) and MP, then re-deriving
 * aggregate stats.
 *
 * SPECIAL CASE: never overwrite the template entry for char 0
 * (索爾) if he died in battle — chapter end revives him or branches
 * to game over, so his battle-dead state must not propagate.
 *
 * Called by every chapter_NN_end handler as part of the standard
 * 3-call tail (save_template + init_growth_char + chapter_id++).
 * ---------------------------------------------------------------- */
void fd2_save_runtime_char_to_template(void)
{
    uint32 rt_idx;
    uint32 tmpl_idx;
    uint8 *rt_char;
    uint8 *tmpl;

    for (rt_idx = 0; (int32)rt_idx < (int32)data_fd2_battle_party_member_count;
         rt_idx++) {
        rt_char = (uint8 *)(data_fd2_battle_runtime_char_array_ptr + rt_idx);

        for (tmpl_idx = 0;
             (int32)tmpl_idx < (int32)data_fd2_shared_menu_party_member_count;
             tmpl_idx++) {
            tmpl = (uint8 *)(data_fd2_shared_menu_party_roster_buffer_ptr +
                             tmpl_idx * 0x50);

            if (rt_char[8] == tmpl[8]) {
                if (tmpl[8] == 0) {
                    if (fd2_check_char_is_dead(rt_idx) != 0) {
                        continue;
                    }
                }
                memmove((void *)tmpl, (void *)rt_char, 0x50);
                memset((void *)(tmpl + 0x22), 0, 6);
                tmpl[5] = (uint8)(tmpl[5] & 1);
                if (tmpl[5] != 1) {
                    *(uint16 *)(tmpl + 0x40) = *(uint16 *)(tmpl + 0x42);
                }
                *(uint16 *)(tmpl + 0x44) = *(uint16 *)(tmpl + 0x46);
                fd2_recompute_runtime_char_total_stats(tmpl_idx);
            }
        }
    }
}
