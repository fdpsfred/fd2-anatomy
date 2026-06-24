/*
 * btl_init.c — Battle setup: runtime-char init, battle-state init, clear / restore / convert
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
 * 1. Desired spawn position from data_fd2_chapter_portrait_load_buffer
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

    pField = (uint8 *)(data_fd2_chapter_portrait_load_buffer + char_field_idx * 6);
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

    fd2_battle_reset_tile_transient_state(data_fd2_battle_tile_map_ptr);

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
 * fd2_init_runtime_char_from_base_growth @ 0x112A5  (20 callers)
 *
 * Populate a new MENU-party (= template_chars) slot from the static
 * char_base + char_growth tables. Called when a character joins
 * permanently (chapter-end recruit) or is force-revived. Operates on
 * the menu_party_roster_buffer / menu_party_member_count — NOT the
 * battle runtime_char_array.
 *
 * Stat formulas:
 *   HP/MP    = base + growth * (level - 1)   (level 1 char => base value)
 *   AP/DP/DX = base + growth * level
 *
 * Slot bytes +0..+4 (pos/sprite_state) are left untouched here; the
 * caller seeds those. The new slot is at
 *   roster_buffer + menu_party_member_count * RUNTIME_CHAR_SIZE.
 * After populating, equip-adjusted aggregates are recomputed and the
 * member count is incremented.
 * ---------------------------------------------------------------- */
void fd2_init_runtime_char_from_base_growth(uint32 char_id)
{
    uint8  *pSlot;
    uint8  *pBase;
    uint8  *pGrowth;
    uint8   level;
    uint16  total_HP;
    uint16  total_MP;
    uint16  base_AP;
    uint16  base_DP;
    uint16  base_DX;
    int     i;

    pSlot = (uint8 *)data_fd2_shared_menu_party_roster_buffer_ptr
          + data_fd2_shared_menu_party_member_count * RUNTIME_CHAR_SIZE;

    pBase   = fd2_get_char_base_entry(char_id);
    pGrowth = fd2_get_char_growth_entry((int)char_id);

    level = pBase[2];
    total_HP = (uint16)((uint16)pGrowth[6] * (level - 1)
             + *(int16 *)(pBase + 3));
    total_MP = (uint16)(*(int16 *)(pBase + 5)
             + (level - 1) * (uint16)pGrowth[8]);
    base_AP = *(uint16 *)(pBase + 0x12);
    base_DP = *(uint16 *)(pBase + 0x14);
    base_DX = *(uint16 *)(pBase + 0x16);

    pSlot[5] = 0;
    pSlot[6] = 2;
    pSlot[7] = (uint8)char_id;
    pSlot[8] = (uint8)char_id;
    pSlot[9] = 0;
    pSlot[0xa] = 0x40;
    pSlot[0xb] = pBase[0xc];
    pSlot[0xc] = 0x40;
    pSlot[0xd] = pBase[0xd];

    for (i = 0; i < 4; i = i + 1) {
        if ((int8)pBase[0xe + i] == -1)
            pSlot[i * 2 + 0xe] = 0x80;
        else
            pSlot[i * 2 + 0xe] = 0;
        pSlot[i * 2 + 0xf] = pBase[0xe + i];
    }

    pSlot[0x16] = 0x80;
    pSlot[0x18] = 0x80;
    memmove(pSlot + 0x1a, pBase + 8, 4);
    pSlot[0x1e] = 0;
    pSlot[0x1f] = pBase[0];
    pSlot[0x20] = pBase[1];
    pSlot[0x21] = level;
    memset(pSlot + 0x22, 0, 6);
    pSlot[0x31] = 0xff;
    *(uint16 *)(pSlot + 0x37) =
        (uint16)(base_AP + (uint16)pGrowth[0] * (uint16)level);
    *(uint16 *)(pSlot + 0x39) =
        (uint16)(base_DP + (uint16)pGrowth[2] * (uint16)level);
    pSlot[0x3b] = pBase[7];
    pSlot[0x3c] = 0;
    *(uint16 *)(pSlot + 0x3e) =
        (uint16)(base_DX + (uint16)pGrowth[4] * (uint16)level);
    *(uint16 *)(pSlot + 0x40) = total_HP;
    *(uint16 *)(pSlot + 0x42) = total_HP;
    *(uint16 *)(pSlot + 0x44) = total_MP;
    *(uint16 *)(pSlot + 0x46) = total_MP;

    fd2_recompute_runtime_char_total_stats(data_fd2_shared_menu_party_member_count);
    data_fd2_shared_menu_party_member_count =
        data_fd2_shared_menu_party_member_count + 1;
}

/* ----------------------------------------------------------------
 * fd2_restore_all_chars_full_hp_mp @ 0x25089  (2 callers)
 *
 * Walk the menu/template char roster (menu_party_roster_buffer_ptr,
 * stride RUNTIME_CHAR_SIZE; count = menu_party_member_count) and reset
 * every slot to a fully-healed state:
 *   flags (+0x05) := 0          // clear status bits
 *   hp_current (+0x40) := hp_max (+0x42)
 *   mp_current (+0x44) := mp_max (+0x46)
 *
 * The loop counter is a byte (BL in the binary), so it implicitly caps
 * at 0xFF entries — safe given menu_party_member_count is always a small
 * party-roster value.
 *
 * Callers (chapter-end cinematic full-restore points):
 *   fd2_chapter_27_end — BAD-path revive sequence.
 *   fd2_chapter_30_end — final-boss-death "double restore" before the
 *                        epilogue battle-data load.
 *
 * void __cdecl with the __CHK(8) stack-probe prologue (compiler-injected,
 * not written here). EBX is the loop counter; POP EBX + RET is the
 * shared epilogue.
 * ---------------------------------------------------------------- */
void fd2_restore_all_chars_full_hp_mp(void)
{
    runtime_char *roster;
    uint8         char_iter;

    roster = (runtime_char *)data_fd2_shared_menu_party_roster_buffer_ptr;
    for (char_iter = 0;
         (int)(uint32)char_iter < (int)data_fd2_shared_menu_party_member_count;
         char_iter = char_iter + 1) {
        roster[char_iter].flags = 0;
        roster[char_iter].hp_current = roster[char_iter].hp_max;
        roster[char_iter].mp_current = roster[char_iter].mp_max;
    }
}

/* ----------------------------------------------------------------
 * fd2_clear_all_chars_facing @ 0x134E4  (23 callers)
 *
 * Reset facing direction (= 0 / south) for every party member, then
 * pause 20 ms for the visual transition. Called at the end of walk
 * sequences and chapter intros to restore default facing.
 *
 * Walks the battle runtime_char array (stride RUNTIME_CHAR_SIZE) for
 * party_member_count entries, zeroing sprite_state[1] (= facing,
 * byte +3) in each slot.
 * ---------------------------------------------------------------- */
void fd2_clear_all_chars_facing(void)
{
    uint8 *pSlot;
    int    char_idx;

    pSlot = (uint8 *)data_fd2_battle_runtime_char_array_ptr;
    for (char_idx = 0;
         char_idx < (int)data_fd2_battle_party_member_count;
         char_idx = char_idx + 1) {
        pSlot[3] = 0;
        pSlot = pSlot + RUNTIME_CHAR_SIZE;
    }
    fd2_delay_ms(0x14);
}

/* ----------------------------------------------------------------
 * fd2_clear_all_chars_acted_flag @ 0x13536  (4 callers)
 *
 * Clear the acted-this-turn flag (bit 0x80) on flags (+0x05) for
 * every battle party member, leaving the other flag bits intact.
 * Called at the start of each new turn cycle to give all chars
 * another action.
 *
 * Walks the battle runtime_char array (stride RUNTIME_CHAR_SIZE) for
 * party_member_count entries, masking flags with 0x7F.
 * ---------------------------------------------------------------- */
void fd2_clear_all_chars_acted_flag(void)
{
    int char_iter;

    for (char_iter = 0;
         char_iter < (int)data_fd2_battle_party_member_count;
         char_iter = char_iter + 1) {
        data_fd2_battle_runtime_char_array_ptr[char_iter].flags =
            (uint8)(data_fd2_battle_runtime_char_array_ptr[char_iter].flags
                    & 0x7f);
    }
}

/* ----------------------------------------------------------------
 * fd2_set_chapter_init_done_flag @ 0x33FAF
 *
 * Unconditionally set chapter_init_done_flag (byte @ 0x53A44) to 1.
 *
 * Sole caller fd2_game_main_loop runs this in a
 *   while (chapter_init_done_flag == 0) fd2_set_chapter_init_done_flag();
 * spin on the action/confirm (Space/Enter) key path. Because the flag is
 * BSS-cleared at load and this setter always writes 1, the loop body runs
 * exactly once -- on the first confirm press while the click-debounce skip
 * count is zero -- latching the flag 0 -> 1; later confirm presses skip it.
 * The flag has exactly one reader (that loop condition) and one writer
 * (this function).
 * ---------------------------------------------------------------- */
void fd2_set_chapter_init_done_flag(void)
{
    data_fd2_chapter_chapter_init_done_flag = 1;
}

/* ----------------------------------------------------------------
 * fd2_set_battle_anim_phase_to_1 @ 0x35C15
 *
 * Shared tail chunk: set battle_anim_phase = 1.
 *
 * In the original this is a tail entered via JMP from several chapter
 * cinematic handlers; it opened with ADD ESP,0xC to drop the 3 args
 * the caller had pushed (for the preceding 3-arg warp helper) before
 * deciding to flip the flag, then MOV [battle_anim_phase],1 and RET.
 * Here it is emitted as a plain void setter -- the arg cleanup is now
 * handled by the normal calling convention, so only the flag write
 * remains. Sole emitted caller: fd2_chapter_event_handler_52
 * (ch30 cinematic), which calls it on both return branches.
 * ---------------------------------------------------------------- */
void fd2_set_battle_anim_phase_to_1(void)
{
    data_fd2_battle_anim_phase = 1;
}

/* ----------------------------------------------------------------
 * fd2_convert_battle_tiles_to_24px @ 0x1399C  (2 callers)
 *
 * Convert data_fd2_battle_scene_snapshot's encoded tile data into a packed
 * 24x24 8bpp tile bank, returning the freshly allocated buffer.
 *
 * Layout of data_fd2_battle_scene_snapshot consumed here:
 *   +4  : uint16 tile_count
 *   +6  : int32[tile_count] offset table (each entry is a byte offset
 *         from snapshot base to that tile's RLE stream)
 *
 * Output bank layout (6-byte header + tile_count * 0x240 tile bytes):
 *   +0  : uint16 sprite width  = 24
 *   +2  : uint16 sprite height = 24
 *   +4  : uint16 tile_count
 *   +6  : tile_count packed tiles, 0x240 (= 24*24) bytes each
 *
 * Each tile is decoded by fd2_tile_blit_24x24_passthrough from its
 * RLE source into its 24x24 cell. On malloc failure the original
 * pushes the message string and tail-JMPs into the shared
 * printf(msg)+exit(1) error stub at 0x10056 (emitted inline here as
 * printf(...)+exit(1)). The "rease" in the message is a vendor typo.
 *
 * Callers (need the unpacked tile bank for cinematic effects):
 *   fd2_cast_earthquake_spell_with_screen_shake @ 0x21548
 *   fd2_open_tactical_overview_zoom @ 0x2000A
 * ---------------------------------------------------------------- */
void *fd2_convert_battle_tiles_to_24px(void)
{
    uint16  tile_count;
    uint8  *bank;
    int     i;

    tile_count = *(uint16 *)(data_fd2_battle_scene_snapshot + 4);
    bank = (uint8 *)malloc(tile_count * 0x240 + 6);
    if (bank == (uint8 *)0) {
        printf("Out of memory at rease shape !!!\n");
        exit(1);
    }

    *(uint16 *)(bank + 0) = 0x18;
    *(uint16 *)(bank + 2) = 0x18;
    *(uint16 *)(bank + 4) = tile_count;
    memset(bank + 6, 0, tile_count * 0x240);

    for (i = 0; i < (int)tile_count; i = i + 1) {
        fd2_tile_blit_24x24_passthrough(
            (uint32)(*(int32 *)(data_fd2_battle_scene_snapshot + 6 + i * 4)
                     + data_fd2_battle_scene_snapshot),
            (uint32)(bank + i * 0x240 + 6),
            0x18);
    }

    return bank;
}
/* ----------------------------------------------------------------
 * data_fd2_chapter_chapter_init_done_flag @ 0x53A44  (.object2, zero-bss)
 *
 * Single-byte runtime state flag. Cleared (0) at program load; set to 1
 * by fd2_set_chapter_init_done_flag (this file, MOV byte [0x53A44],1).
 * Read byte-wide by fd2_game_main_loop (compares == 0) to gate the
 * transition from chapter-init into active gameplay. Mutable (game-side
 * writer exists) -> not const; zero initial value -> tentative definition.
 * ---------------------------------------------------------------- */
uint8  data_fd2_chapter_chapter_init_done_flag;
