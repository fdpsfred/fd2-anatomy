/*
 * pathfnd.c — Movement range flood fill + pathfinding.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_pathfind_count_unique_directions @ 0x4E3CF
 *
 * Count direction-change transitions in the pathfind stack.
 * Returns transition_count * 4 as tiebreak weight.
 * ---------------------------------------------------------------- */
uint8 fd2_pathfind_count_unique_directions(void)
{
    uint8 remain;
    uint8 unique_count;
    uint8 prev_dir;
    uint8 *stack_ptr;

    stack_ptr = data_fd2_battle_pathfind_step_stack;
    unique_count = 0;
    prev_dir = 0xFF;
    remain = data_fd2_battle_pathfind_current_depth;
    do {
        if (stack_ptr[3] != prev_dir) {
            unique_count++;
            prev_dir = stack_ptr[3];
        }
        stack_ptr += 8;
        remain--;
    } while (remain != 0);
    return unique_count << 2;
}

/* ----------------------------------------------------------------
 * fd2_flood_fill_movement_range_recursive @ 0x4E0DC (2 callers: the
 * orchestrator fd2_init_movement_range_floodfill @ 0x4E040 and itself)
 *
 * Recursive 4-neighbour flood fill that paints the movement-range markers
 * for "where can this unit move?". Reached only via the orchestrator
 * (which seeds the origin tile) and self-recursion.
 *
 * In FD2.LE this routine uses the Watcom register-passing convention with a
 * hand-rolled recursion stack (EDI walks the workspace at 0x60079, pushing a
 * 7-byte {x, y, cost, btm_ptr} frame per level). That register/EDI machinery
 * is purely the original encoding of recursion; this emit replaces it with
 * native C recursion + parameters, which is Layer-2 equivalent (identical
 * marker writes, identical visit order, identical termination). The map_width
 * row stride that the binary keeps live in EBP for the whole fill is recomputed
 * here from the map_width global (constant during a fill).
 *
 *   x, y     : current tile coordinates (DL/DH in the binary).
 *   cost     : residual movement budget left at this tile (CL); higher means
 *              closer to the origin.
 *   btm_ptr  : pointer to this tile's marker byte in the battle tile map
 *              (EBX); the neighbour step reads attr at [-3..-2] and flags at
 *              [-1] relative to it.
 *
 * For each of the four directions, in the binary's order right/left/down/up:
 *   - bounds-check the neighbour (right/down use an unsigned (coord+1)<extent
 *     test; left/up just require coord!=0),
 *   - call fd2_flood_fill_neighbor_step(cost, neighbour_btm, &new_cost), which
 *     conditionally writes the neighbour's marker and returns non-zero (binary:
 *     carry clear) iff the tile improved and expansion should continue, with
 *     new_cost = cost - tile_cost (forced to 0 for an 0x80 "movement sink"),
 *   - recurse into the neighbour with the reduced new_cost when it returned
 *     non-zero.
 * The original `cost`/coords are unchanged across directions and across the
 * recursive call (the binary reloads them from its saved EDI frame; here the
 * locals simply persist), so every direction starts from the same residual.
 * ---------------------------------------------------------------- */
void fd2_flood_fill_movement_range_recursive(uint8 x, uint8 y, uint8 cost,
    uint8 *btm_ptr)
{
    uint32 stride;
    uint8 new_cost;

    /* Row stride in bytes = map_width tiles * 4 bytes/tile (EBP in the
     * binary, set once by the orchestrator). */
    stride = (uint32)data_fd2_battle_pathfind_map_width * 4;

    /* right: (x+1) < map_width (unsigned) */
    if ((uint8)(x + 1) < data_fd2_battle_pathfind_map_width) {
        new_cost = 0;
        if (fd2_flood_fill_neighbor_step(cost, btm_ptr + 4, &new_cost)) {
            fd2_flood_fill_movement_range_recursive((uint8)(x + 1), y,
                new_cost, btm_ptr + 4);
        }
    }

    /* left: x != 0 */
    if (x != 0) {
        new_cost = 0;
        if (fd2_flood_fill_neighbor_step(cost, btm_ptr - 4, &new_cost)) {
            fd2_flood_fill_movement_range_recursive((uint8)(x - 1), y,
                new_cost, btm_ptr - 4);
        }
    }

    /* down: (y+1) < map_height (unsigned) */
    if ((uint8)(y + 1) < data_fd2_battle_pathfind_map_height) {
        new_cost = 0;
        if (fd2_flood_fill_neighbor_step(cost, btm_ptr + stride, &new_cost)) {
            fd2_flood_fill_movement_range_recursive(x, (uint8)(y + 1),
                new_cost, btm_ptr + stride);
        }
    }

    /* up: y != 0 */
    if (y != 0) {
        new_cost = 0;
        if (fd2_flood_fill_neighbor_step(cost, btm_ptr - stride, &new_cost)) {
            fd2_flood_fill_movement_range_recursive(x, (uint8)(y - 1),
                new_cost, btm_ptr - stride);
        }
    }
}
