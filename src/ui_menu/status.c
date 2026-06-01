/*
 * status.c — Status screen and item stat preview helpers
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_compute_equipped_stats_with_item_preview @ 0x2EFB7  (1 caller)
 *
 * Compute stats as if candidate item_id were equipped, replacing
 * the currently equipped item of the same category (weapon/armor).
 * Bonuses from the OPPOSITE category's equipped items are preserved.
 * Output: 4 int32 at stats_out_ptr: [AP, DP, DX, Stat4].
 * ---------------------------------------------------------------- */
void fd2_compute_equipped_stats_with_item_preview(uint32 char_idx,
                                                   uint32 item_id,
                                                   uint32 stats_out_ptr)
{
    runtime_char *rc;
    uint8 *item_entry;
    uint8 *cur_entry;
    uint32 preview_cat;
    int ap;
    int dp;
    int dx;
    int stat4;
    uint32 slot_iter;

    item_entry = fd2_get_item_effect_entry(item_id);
    preview_cat = (uint32)item_entry[0];

    rc = data_fd2_battle_runtime_char_array_ptr;
    ap = (int)*(int16 *)(rc[char_idx].combat_aux_block + 0x10)
       + (int)*(int16 *)(item_entry + 1);
    dp = (int)*(int16 *)(rc[char_idx].combat_aux_block + 0x12)
       + (int)*(int16 *)(item_entry + 5);
    dx = (int)*(int16 *)(rc[char_idx].ai_target_and_dx_block + 1)
       + (int)*(int16 *)(item_entry + 3);
    stat4 = (int)*(int16 *)(rc[char_idx].ai_target_and_dx_block + 1)
          + (int)*(int16 *)(item_entry + 7);

    for (slot_iter = 0; (int)slot_iter < 8; slot_iter++) {
        cur_entry = fd2_get_item_effect_entry(
            (uint32)rc[char_idx].inventory_slots[slot_iter * 2 + 1]);
        if ((rc[char_idx].inventory_slots[slot_iter * 2] & 0x40) != 0
            && ((preview_cat <= 0x14
                 && (uint32)cur_entry[0] > 0x14)
                || (preview_cat > 0x14
                    && (uint32)cur_entry[0] <= 0x14))) {
            ap += (int)*(int16 *)(cur_entry + 1);
            dp += (int)*(int16 *)(cur_entry + 5);
            dx += (int)*(int16 *)(cur_entry + 3);
            stat4 += (int)*(int16 *)(cur_entry + 7);
        }
    }

    *(int32 *)stats_out_ptr = (int32)ap;
    *(int32 *)(stats_out_ptr + 4) = (int32)dp;
    *(int32 *)(stats_out_ptr + 8) = (int32)dx;
    *(int32 *)(stats_out_ptr + 0xC) = (int32)stat4;
}
