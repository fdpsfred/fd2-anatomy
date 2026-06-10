#ifndef BATTLFIX_H
#define BATTLFIX_H
/* shared test fixtures for the battle domain; include AFTER the
   common preamble (needs the test externs + types). */


/* 4-byte tile-map header + 20x15 4-byte tile records. The +4 header room lets
 * the real fd2_obfuscate_battle_tile_map (count = header[0]*header[2], records
 * start at base+4) iterate the full 300-tile map without running past the
 * buffer. (Before this function was emitted it was a no-op stub, so the header
 * was never read and the buffer carried no header allowance.) */
static uint8 t_ai_tile_map[4 + 20 * 15 * 4];

/* testglob.c: snapshot the painted tile map so the floodfill stub repaints the
 * test's intended +7 reachability after the real obfuscate resets it. Tests
 * call bf_capture_tilemap() once after painting +7 and before invoking the
 * AI function under test. g_bf_tilemap_snapshot_bytes==0 => stub inert. */
extern void   bf_capture_tilemap(void);
extern uint32 g_bf_tilemap_snapshot_bytes;


static void reset_ai_stubs(void)
{
    g_composite_call_count = 0;
    g_attack_dispatch_return = 0;
    g_attack_dispatch_calls = 0;
    g_seek_optimal_return = 0;
    g_advance_nearest_return = 0;
    g_walk_return = 0;
    g_score_physical_return = 0;
    g_pass_turn_calls = 0;
    g_execute_spell_calls = 0;
    g_execute_physical_calls = 0;
    g_pathfind_return = 0;
    g_pathfind_walk_return = 0;
    g_pathfind_write_dst = 0;
    g_pathfind_dst_x = 0;
    g_pathfind_dst_y = 0;
    g_pathfind_seq_enable = 0;
    g_pathfind_seq[0] = 0; g_pathfind_seq[1] = 0;
    g_pathfind_seq[2] = 0; g_pathfind_seq[3] = 0;
    g_pathfind_seq_idx = 0;
    g_pathfind_seq_steps = 0;
    memset(g_pathfind_step_bytes, 0, sizeof(g_pathfind_step_bytes));
    g_pathfind_md0_dst_x = -1;
    g_pathfind_md0_dst_y = -1;
    memset(t_ai_tile_map, 0xFF, sizeof(t_ai_tile_map));
    /* Valid header dims so the real fd2_obfuscate_battle_tile_map iterates a
     * bounded count (20*15 = 300 records) instead of header[0]*header[2] =
     * 0xFF*0xFF = 65025, which would overrun the buffer by ~260 KB. */
    t_ai_tile_map[0] = 20;   /* map width  (header byte 0) */
    t_ai_tile_map[2] = 15;   /* map height (header byte 2) */
    data_fd2_battle_tile_map_ptr = (uint32)t_ai_tile_map;
    g_bf_tilemap_snapshot_bytes = 0;   /* disarm floodfill repaint by default */
}

#endif
