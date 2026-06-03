/*
 * btl_init.c — Battle setup: runtime-char init, battle-state init, clear / restore / convert
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_init_battle_state_for_chapter @ 0x205DA
 *
 * Initialize battle state for the current chapter: clear flags,
 * load battle data, zero viewport/cursor, first paint, fade in.
 * ---------------------------------------------------------------- */
void fd2_init_battle_state_for_chapter(void)
{
    data_fd2_battle_anim_phase = 0;
    data_fd2_chapter_event_or_battle_end_code = 0;
    fd2_load_chapter_battle_data(data_fd2_chapter_current_chapter_id);
    memset((void *)data_fd2_field_map_tile_event_consumed_flags_ptr,
           0, 0x20);
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_battle_cursor_world_x = 0;
    data_fd2_battle_cursor_world_y = 0;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;
    fd2_composite_battle_frame(1);
    data_fd2_battle_anim_phase = 1;
    fd2_play_palette_fade_in();
    data_fd2_battle_turn_counter = 1;
    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * fd2_init_runtime_char_for_battle @ 0x10C50  (1 caller)
 *
 * Initialize a NEW battle-runtime_char slot from chapter data
 * (FDFIELD per-char record + FDICON.B24 portrait load).
 *
 * char_field_idx selects the per-char field record; fdicon_fp is the
 * open FDICON.B24 handle passed to the portrait loader.
 *
 * 1. Desired spawn position from chapter_portrait_load_buffer
 *    + char_field_idx*6 : byte +2 = desired_x, byte +4 = desired_y.
 * 2. Repaint threat overlay (clear team 0/1 paint).
 * 3. If chapter_init_phase_flag == 0: scan the tile map for the
 *    nearest walkable+unoccupied tile (attr byte 6 bit 0x40 == 0),
 *    minimizing taxi distance from desired_x/y; else use desired_x/y.
 * 4. Read per-char record from tile_event_data_table at
 *    (char_field_idx*0x1A + 0x83), stride 0x1A.
 * 5. Compute HP/MP/AP/DP/DX from char base+growth (id < 0x44) or
 *    enemy data (id >= 0x44, scaled by level).
 * 6. Populate the runtime_char slot at party_member_count.
 * 7. fd2_recalculate_combat_stats() applies equipment deltas.
 * 8. party_member_count++.
 * ---------------------------------------------------------------- */
void fd2_init_runtime_char_for_battle(uint32 char_field_idx, uint32 fdicon_fp)
{
    uint8  *pSlot;
    uint8  *pField;
    uint8  *pRec;
    uint8  *pBase;
    uint8  *pGrowth;
    uint8  *pEnemy;
    uint8  *pTile;
    uint8   char_id;
    uint8   level;
    uint8   team;
    uint8   record_char_id;
    uint8   record_level;
    uint16  total_HP;
    uint16  total_MP;
    uint32  desired_x;
    uint32  desired_y;
    uint32  spawn_x;
    uint32  spawn_y;
    int     best_taxi;
    int     taxi;
    int     row;
    int     col;
    int     slot_iter;
    int     portrait_idx;

    pSlot = (uint8 *)data_fd2_battle_runtime_char_array_ptr
          + data_fd2_battle_party_member_count * RUNTIME_CHAR_SIZE;

    pField = (uint8 *)(chapter_portrait_load_buffer + char_field_idx * 6);
    desired_x = (uint32)pField[2];
    desired_y = (uint32)pField[4];

    fd2_paint_threat_overlay_for_team(0);
    fd2_paint_threat_overlay_for_team(1);

    spawn_x = desired_x;
    spawn_y = desired_y;
    if (data_fd2_chapter_init_phase_flag == 0) {
        best_taxi = 0xff;
        for (row = 0; row < (int)data_fd2_battle_map_height_tiles;
             row = row + 1) {
            for (col = 0; col < (int)data_fd2_battle_map_width_tiles;
                 col = col + 1) {
                pTile = (uint8 *)(data_fd2_battle_tile_map_ptr
                      + (uint32)((data_fd2_battle_map_width_tiles * row
                                  + col) * 4 + 6));
                if ((*pTile & 0x40) == 0) {
                    taxi = abs(col - (int)desired_x)
                         + abs(row - (int)desired_y);
                    if (taxi <= best_taxi) {
                        best_taxi = taxi;
                        spawn_x = (uint32)col;
                        spawn_y = (uint32)row;
                    }
                }
            }
        }
    }

    fd2_obfuscate_battle_tile_map(data_fd2_battle_tile_map_ptr);

    pRec = (uint8 *)(data_fd2_tile_event_data_table_ptr
         + char_field_idx * 0x1a + 0x83);
    team           = pRec[0];
    record_char_id = pRec[1];
    char_id        = record_char_id;
    record_level   = pRec[4];
    level          = record_level;

    if ((uint32)record_char_id < 0x44) {
        pBase   = fd2_get_char_base_entry((uint32)record_char_id);
        pGrowth = fd2_get_char_growth_entry((int)(uint32)record_char_id);
        total_HP = (uint16)(*(int16 *)(pBase + 3)
                 + (uint16)pGrowth[6] * (level - 1));
        total_MP = (uint16)(*(int16 *)(pBase + 5)
                 + (level - 1) * (uint16)pGrowth[8]);
        pSlot[0x1f] = pBase[0];
        pSlot[0x20] = pBase[1];
        *(uint16 *)(pSlot + 0x37) =
            (uint16)((uint16)pGrowth[0] * (uint16)level
                     + *(uint16 *)(pBase + 0x12));
        *(uint16 *)(pSlot + 0x39) =
            (uint16)((uint16)pGrowth[2] * (uint16)level
                     + *(uint16 *)(pBase + 0x14));
        pSlot[0x3b] = pBase[7];
        *(uint16 *)(pSlot + 0x3e) =
            (uint16)((uint16)pGrowth[4] * (uint16)level
                     + *(uint16 *)(pBase + 0x16));
    }
    else {
        pEnemy = fd2_get_enemy_data_entry((int)((uint32)record_char_id - 0x44));
        total_HP = (uint16)((uint16)level * *(int16 *)(pEnemy + 2));
        total_MP = (uint16)((uint16)level * (uint16)pEnemy[4]);
        pSlot[0x1f] = pEnemy[0];
        pSlot[0x20] = pEnemy[1];
        *(uint16 *)(pSlot + 0x37) = (uint16)((uint16)level * (uint16)pEnemy[5]);
        *(uint16 *)(pSlot + 0x39) = (uint16)((uint16)level * (uint16)pEnemy[6]);
        *(uint16 *)(pSlot + 0x3e) = (uint16)((uint16)level * (uint16)pEnemy[7]);
        pSlot[0x3b] = pEnemy[8];
    }

    pSlot[0] = (uint8)spawn_x;
    pSlot[1] = (uint8)spawn_y;
    portrait_idx = fd2_load_portrait_to_cache((uint32)char_id, fdicon_fp);
    pSlot[2] = (uint8)portrait_idx;
    pSlot[3] = 0;
    pSlot[4] = 0;
    pSlot[5] = 0;
    pSlot[6] = team;
    pSlot[7] = char_id;
    pSlot[8] = char_id;
    pSlot[9] = 0;

    if ((int8)pRec[5] == -1) {
        pSlot[0xa] = 0x40;
        pSlot[0xb] = pRec[6];
        pSlot[0xc] = 0x80;
    }
    else {
        pSlot[0xa] = 0x40;
        pSlot[0xb] = pRec[5];
        pSlot[0xc] = 0x40;
        pSlot[0xd] = pRec[6];
    }

    for (slot_iter = 0; slot_iter < 6; slot_iter = slot_iter + 1) {
        if ((int8)pRec[7 + slot_iter] == -1)
            pSlot[slot_iter * 2 + 0xe] = 0x80;
        else
            pSlot[slot_iter * 2 + 0xe] = 0;
        pSlot[slot_iter * 2 + 0xf] = pRec[7 + slot_iter];
    }

    memset(pSlot + 0x22, 0, 6);
    memmove(pSlot + 0x1a, pRec + 0xd, 4);
    pSlot[0x1e] = 0;
    pSlot[0x21] = record_level;
    pSlot[0x31] = pRec[0x16];
    *(uint16 *)(pSlot + 0x32) = *(uint16 *)(pRec + 0x17);
    pSlot[0x34] = pRec[0x11];
    pSlot[0x35] = pRec[0x12];
    pSlot[0x36] = pRec[0x13];
    pSlot[0x3d] = pRec[2];
    if (pSlot[6] == 2)
        pSlot[0x3c] = 0;
    else
        pSlot[0x3c] = 0xff;

    *(uint16 *)(pSlot + 0x40) = total_HP;
    *(uint16 *)(pSlot + 0x42) = total_HP;
    *(uint16 *)(pSlot + 0x44) = total_MP;
    *(uint16 *)(pSlot + 0x46) = total_MP;

    fd2_recalculate_combat_stats(data_fd2_battle_party_member_count);
    data_fd2_battle_party_member_count = data_fd2_battle_party_member_count + 1;
}

/* ----------------------------------------------------------------
 * fd2_set_chapter_init_done_flag @ 0x33FAF
 *
 * Set chapter_init_done_flag byte to 1.
 * ---------------------------------------------------------------- */
void fd2_set_chapter_init_done_flag(void)
{
    data_fd2_chapter_chapter_init_done_flag = 1;
}

/* ----------------------------------------------------------------
 * fd2_set_battle_anim_phase_to_1 @ 0x35C15
 *
 * Shared tail chunk: set battle_anim_phase = 1.
 * Originally a JMP target with stack cleanup; emitted as
 * standalone setter.
 * ---------------------------------------------------------------- */
void fd2_set_battle_anim_phase_to_1(void)
{
    data_fd2_battle_anim_phase = 1;
}