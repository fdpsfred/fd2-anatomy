#ifndef BATTLFIX_H
#define BATTLFIX_H
/* shared test fixtures for the battle domain; include AFTER the
   common preamble (needs the test externs + types). */


static uint8 t_ai_tile_map[20 * 15 * 4];


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
    data_fd2_battle_tile_map_ptr = (uint32)t_ai_tile_map;
}

#endif
