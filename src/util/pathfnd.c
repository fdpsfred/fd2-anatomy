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

/* ----------------------------------------------------------------
 * fd2_flood_fill_neighbor_step @ 0x4E16E (4 callers: all four directions of
 * fd2_flood_fill_movement_range_recursive @ 0x4E0DC)
 *
 * Inner step of the movement-range flood fill: visit one neighbour tile, look
 * up its movement cost via the two-level cost tables, and conditionally update
 * the tile's BTM marker when the new residual cost beats the existing marker.
 *
 * In FD2.LE this is a register-passing leaf with no stack frame: remaining cost
 * in CL, the tile's marker pointer in EBX, the secondary cost-table base live in
 * ESI (set once by the orchestrator and inherited through the whole recursion),
 * and it signals "improved, keep expanding" back to the caller via the carry
 * flag (CLC = improved / recurse, STC = skip) while writing the new residual
 * into the marker byte. This emit is Layer-2 equivalent: the carry result
 * becomes the int return value (non-zero == binary CLC) and the updated residual
 * is handed back through new_cost_out, which is how the caller obtains the value
 * (CL) it feeds into the recursive descent. The ESI cost-table base is read from
 * data_fd2_battle_pathfind_caller_context, the global the orchestrator writes ESI
 * into at entry (0x4E047), so the value is identical to the inherited register.
 *
 *   remaining_cost : residual movement budget at the source tile (CL).
 *   btm_attr_ptr   : pointer to the neighbour tile's marker byte (EBX); the raw
 *                    16-bit attribute word lives at [-3..-2] and the flags byte
 *                    at [-1] relative to it.
 *   new_cost_out   : receives the residual cost passed to the neighbour
 *                    (remaining_cost - tile_cost, forced to 0 for an 0x80 sink).
 *
 * Attribute -> cost: the low 10 bits of the attribute word index the primary
 * table (pointer at 0x60060) at [(attr<<2)+1] to get a secondary index, which
 * indexes the secondary cost table (ESI base) to get the tile's movement cost.
 * The marker is rewritten (and non-zero returned) only when the tile is
 * affordable (tile_cost <= remaining_cost), strictly improves on the current
 * marker (signed compare), and is not flagged impassable (flags & 0x40). An
 * 0x80 "movement sink" tile is marked reachable but with residual 0 so the
 * recursion stops expanding past it.
 * ---------------------------------------------------------------- */
int fd2_flood_fill_neighbor_step(uint8 remaining_cost, uint8 *btm_attr_ptr,
    uint8 *new_cost_out)
{
    uint16 attr_word;       /* AX  : raw 16-bit attribute word at [btm_attr_ptr-3] */
    uint8  cost_idx;        /* CH  : secondary-table index from the primary table */
    uint8  tile_cost;       /* cost_table[cost_idx] (ESI base) */
    uint8  new_cost;        /* CL  : remaining_cost - tile_cost */
    uint8  flags;           /* AL  : flags byte at [btm_attr_ptr-1] */

    /* low 10 bits of the attribute word, *4, select the primary-table entry;
     * its +1 byte is the index into the secondary cost table (ESI base). */
    attr_word = *(uint16 *)(btm_attr_ptr - 3);
    cost_idx = *(uint8 *)(data_fd2_battle_pathfind_tile_cost_table_ptr
        + (uint16)((attr_word & 0x3FF) << 2) + 1);
    tile_cost = *(uint8 *)(data_fd2_battle_pathfind_caller_context
        + (uint32)cost_idx);

    new_cost = (uint8)(remaining_cost - tile_cost);
    *new_cost_out = new_cost;

    flags = *(btm_attr_ptr - 1);
    if (tile_cost <= remaining_cost
        && (int8)*btm_attr_ptr < (int8)new_cost
        && (flags & 0x40) == 0) {
        if ((flags & 0x80) != 0) {
            new_cost = 0;
        }
        *btm_attr_ptr = new_cost;
        *new_cost_out = new_cost;
        return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_pathfind_recursive_with_direction @ 0x4E27C (2 callers: the orchestrator
 * fd2_pathfind_to_destination @ 0x4E1A6 and itself)
 *
 * Direction-tracked recursive pathfind expansion: the path-aware twin of
 * fd2_flood_fill_movement_range_recursive @ 0x4E0DC. It visits the four
 * neighbours of the current tile in the binary's order right/left/down/up,
 * tags each visit with a direction code, and recurses into any neighbour whose
 * cost step improved -- exactly like the flood fill -- but additionally records
 * the per-level direction so the orchestrator can reconstruct the chosen path.
 *
 * In FD2.LE this is a register-passing routine (DL/DH = x/y, CL = residual
 * cost, EBX = the tile's marker pointer, EBP = the map_width*4 row stride live
 * for the whole search, EDI = the recursion stack cursor). Unlike the flood
 * fill -- whose EDI stack was *purely* recursion plumbing that this project
 * replaced with native C recursion -- here the recursion stack is real,
 * consumed data: each 8-byte frame {x, y, cost, dir} written into
 * data_fd2_battle_pathfind_step_stack at the current depth carries the
 * direction byte that fd2_pathfind_neighbor_step_with_tiebreak reads back
 * (via fd2_pathfind_count_unique_directions for its tiebreak weight, and via
 * fd2_pathfind_check_destination_save_path to copy the direction sequence into
 * the output buffer when the destination is reached). So this emit keeps native
 * C recursion + parameters for the control flow, but still maintains the step
 * stack frame {x, y, cost, dir} at data_fd2_battle_pathfind_step_stack[depth*8]
 * for every level, since the direction bytes there are load-bearing. The depth
 * marker data_fd2_battle_pathfind_current_depth is incremented on entry and
 * decremented on exit, matching the binary's INC/DEC of 0x60077.
 *
 *   x, y     : current tile coordinates (DL/DH).
 *   cost     : residual movement budget left at this tile (CL).
 *   btm_ptr  : pointer to this tile's marker byte in the battle tile map (EBX);
 *              the neighbour step reads the attribute word at [-3..-2] and the
 *              flags byte at [-1] relative to it.
 *
 * Direction codes written per branch (pState[3] / frame[+3], the binary's CH):
 *   3 = right, 1 = left, 0 = down (y+1), 2 = up (y-1).
 *
 * For each direction the neighbour coordinates are handed to
 * fd2_pathfind_neighbor_step_with_tiebreak (which the helpers see as DL/DH);
 * it returns non-zero (binary: carry clear) iff the neighbour improved and the
 * search should descend, with new_cost = the reduced residual to recurse with
 * (CL after the step, forced to 0 for an 0x80 movement-sink tile). The
 * original x/y/cost/btm_ptr persist across directions and the recursive call
 * (the binary reloads them from its saved frame; here the locals simply
 * persist), so every direction starts from the same residual.
 * ---------------------------------------------------------------- */
void fd2_pathfind_recursive_with_direction(uint8 x, uint8 y, uint8 cost,
    uint8 *btm_ptr)
{
    uint32 stride;
    uint8 new_cost;
    uint8 *frame;

    /* Row stride in bytes = map_width tiles * 4 bytes/tile (EBP in the binary,
     * set once by the orchestrator). */
    stride = (uint32)data_fd2_battle_pathfind_map_width * 4;

    /* Push this level's frame {x, y, cost, dir} onto the recursion stack at the
     * current depth, then advance the depth marker (binary: write 8 bytes at
     * EDI then INC [0x60077]). The direction byte starts at 3 (right) and is
     * rewritten before each subsequent branch; the helpers read it back. */
    frame = &data_fd2_battle_pathfind_step_stack[
        (uint32)data_fd2_battle_pathfind_current_depth * 8];
    frame[0] = x;
    frame[1] = y;
    frame[2] = cost;
    frame[3] = 3;                  /* dir = right */
    *(uint32 *)(frame + 4) = (uint32)btm_ptr;
    data_fd2_battle_pathfind_current_depth++;

    /* right: dir=3 (already set), (x+1) < map_width (unsigned) */
    if ((uint8)(x + 1) < data_fd2_battle_pathfind_map_width) {
        new_cost = 0;
        if (fd2_pathfind_neighbor_step_with_tiebreak((uint8)(x + 1), y, cost,
                btm_ptr + 4, &new_cost)) {
            fd2_pathfind_recursive_with_direction((uint8)(x + 1), y,
                new_cost, btm_ptr + 4);
        }
    }

    /* left: dir=1, x != 0 */
    frame[3] = 1;
    if (x != 0) {
        new_cost = 0;
        if (fd2_pathfind_neighbor_step_with_tiebreak((uint8)(x - 1), y, cost,
                btm_ptr - 4, &new_cost)) {
            fd2_pathfind_recursive_with_direction((uint8)(x - 1), y,
                new_cost, btm_ptr - 4);
        }
    }

    /* down: dir=0, (y+1) < map_height (unsigned) */
    frame[3] = 0;
    if ((uint8)(y + 1) < data_fd2_battle_pathfind_map_height) {
        new_cost = 0;
        if (fd2_pathfind_neighbor_step_with_tiebreak(x, (uint8)(y + 1), cost,
                btm_ptr + stride, &new_cost)) {
            fd2_pathfind_recursive_with_direction(x, (uint8)(y + 1),
                new_cost, btm_ptr + stride);
        }
    }

    /* up: dir=2, y != 0 */
    frame[3] = 2;
    if (y != 0) {
        new_cost = 0;
        if (fd2_pathfind_neighbor_step_with_tiebreak(x, (uint8)(y - 1), cost,
                btm_ptr - stride, &new_cost)) {
            fd2_pathfind_recursive_with_direction(x, (uint8)(y - 1),
                new_cost, btm_ptr - stride);
        }
    }

    /* Pop this level's frame (binary: SUB DI,8 reload + DEC [0x60077]). */
    data_fd2_battle_pathfind_current_depth--;
}

/* ----------------------------------------------------------------
 * fd2_pathfind_neighbor_step_with_tiebreak @ 0x4E330 (1 caller: all four
 * directions of fd2_pathfind_recursive_with_direction @ 0x4E27C)
 *
 * Path-aware inner step: the direction-recording twin of
 * fd2_flood_fill_neighbor_step @ 0x4E16E. It visits one neighbour, looks up its
 * movement cost via the same two-level cost tables, and -- when the new residual
 * cost beats (or, in mode 1, ties-then-wins against) the existing marker --
 * records the branch direction into the tile's direction nibble and conditionally
 * commits the marker, signalling the caller to recurse.
 *
 * In FD2.LE this is a register-passing leaf with no stack frame: residual cost in
 * CL, the tile's marker pointer in EBX, the secondary cost-table base live in ESI
 * (inherited from the orchestrator through the whole recursion), and it signals
 * "improved, keep expanding" back to the caller via the carry flag (CLC = recurse,
 * STC = skip). The neighbour x/y ride in DL/DH and are not touched by this routine
 * itself -- they exist only so its two destination helpers
 * (fd2_pathfind_record_destination_xy / fd2_pathfind_check_destination_save_path)
 * can read them. This emit is Layer-2 equivalent: the carry result becomes the int
 * return (non-zero == binary CLC), the residual handed to the recursion (CL) is
 * returned through new_cost_out, the ESI base is read from
 * data_fd2_battle_pathfind_caller_context (the global the orchestrator writes ESI
 * into at entry), and x/y are passed explicitly so the helpers receive them.
 *
 *   x, y           : neighbour tile coordinates (DL/DH; pass-through to helpers).
 *   remaining_cost : residual movement budget at the source tile (CL).
 *   btm_attr_ptr   : pointer to the neighbour tile's marker byte (EBX); the 16-bit
 *                    attribute word lives at [-3..-2], the packed direction nibble
 *                    at [-2], and the flags byte at [-1] relative to it.
 *   new_cost_out   : on a non-zero return, receives the residual passed to the
 *                    neighbour (remaining_cost - tile_cost, forced to 0 for an 0x80
 *                    sink in modes 0/1).
 *
 * Mode (data_fd2_battle_pathfind_mode_flags @ 0x6017A, set by the orchestrator):
 *   0: standard           -- strictly-better commits; ties never win.
 *   1: standard + tiebreak -- on a tie, commit iff the new path's direction-change
 *                            weight (fd2_pathfind_count_unique_directions, a count
 *                            of direction transitions in the step stack, *4) beats
 *                            the weight already packed in the marker's [-2] byte.
 *   2: ignore-obstacles + dst-record -- skips the passability gate and records the
 *                            destination on every commit.
 *
 * On any commit the chosen direction code (count*4) is OR'd into the marker's [-2]
 * byte, preserving that byte's low 2 bits (the attribute's high 2 bits, which the
 * cost lookup still needs). In modes 0/1 the marker itself (and the recurse
 * signal) is then withheld for an 0x40 impassable tile -- but the direction byte
 * has already been written -- while an 0x80 sink tile is marked reachable with
 * residual 0 so the search stops expanding past it.
 *
 * NOTE: in every commit the value stored into the marker byte ([0]) is the residual
 * cost CL (== remaining_cost - tile_cost, or 0 for a sink), exactly like the flood
 * fill; the direction code is stored separately in [-2]. (The original plate
 * comment mislabelled the marker write as "direction"; corrected here.)
 * ---------------------------------------------------------------- */
int fd2_pathfind_neighbor_step_with_tiebreak(uint8 x, uint8 y, uint8 remaining_cost,
    uint8 *btm_attr_ptr, uint8 *new_cost_out)
{
    uint16 attr_word;       /* AX  : raw 16-bit attribute word at [btm_attr_ptr-3] */
    uint8  cost_idx;        /* CH  : secondary-table index from the primary table */
    uint8  tile_cost;       /* cost_table[cost_idx] (ESI base) */
    uint8  new_cost;        /* CL  : remaining_cost - tile_cost */
    uint8  dir_code;        /* AL  : direction weight from count_unique_directions */
    uint8  flags;           /* AL  : flags byte at [btm_attr_ptr-1] */

    /* low 10 bits of the attribute word, *4, select the primary-table entry; its
     * +1 byte is the index into the secondary cost table (ESI base). */
    attr_word = *(uint16 *)(btm_attr_ptr - 3);
    cost_idx = *(uint8 *)(data_fd2_battle_pathfind_tile_cost_table_ptr
        + (uint16)((attr_word & 0x3FF) << 2) + 1);
    tile_cost = *(uint8 *)(data_fd2_battle_pathfind_caller_context
        + (uint32)cost_idx);

    /* tile too costly (binary SUB CL,cost -> carry / JC): skip, do not recurse. */
    if (tile_cost > remaining_cost) {
        return 0;
    }
    new_cost = (uint8)(remaining_cost - tile_cost);

    /* improvement gate is SIGNED (CMP CL,[btm]; JL/JG). */
    if ((int8)new_cost < (int8)*btm_attr_ptr) {
        return 0;                                  /* existing strictly better */
    }
    if ((int8)new_cost > (int8)*btm_attr_ptr) {
        dir_code = fd2_pathfind_count_unique_directions();
    } else {
        /* tie: only mode 1 attempts the direction-weight tiebreak. */
        if (data_fd2_battle_pathfind_mode_flags != 1) {
            return 0;
        }
        dir_code = fd2_pathfind_count_unique_directions();
        /* commit only if the new weight strictly beats the one already packed
         * into [-2] (its low 2 attr bits masked off): JBE -> skip. */
        if (dir_code <= (uint8)(*(btm_attr_ptr - 2) & 0xFC)) {
            return 0;
        }
    }

    /* commit: pack the direction code into [-2], preserving its low 2 bits (the
     * attribute's high 2 bits). */
    *(btm_attr_ptr - 2) = (uint8)(dir_code | (*(btm_attr_ptr - 2) & 0x03));

    if (data_fd2_battle_pathfind_mode_flags == 2) {
        /* mode 2: write the residual marker, record the destination, recurse. */
        *btm_attr_ptr = new_cost;
        *new_cost_out = new_cost;
        fd2_pathfind_record_destination_xy(x, y, btm_attr_ptr);
        return 1;
    }

    /* modes 0/1: gate the marker write on passability. */
    flags = *(btm_attr_ptr - 1);
    if ((flags & 0x40) != 0) {
        return 0;                                  /* impassable: no marker, no recurse */
    }
    if ((flags & 0x80) != 0) {
        new_cost = 0;                              /* sink: reachable but no expand */
    }
    *btm_attr_ptr = new_cost;
    *new_cost_out = new_cost;
    fd2_pathfind_check_destination_save_path(x, y);
    return 1;
}

/* ----------------------------------------------------------------
 * fd2_pathfind_record_destination_xy @ 0x4E3B3 (1 caller: the mode-2 commit
 * branch of fd2_pathfind_neighbor_step_with_tiebreak @ 0x4E330)
 *
 * Mode-2 destination snapshot: when an in-flight step commits on a tile that is
 * flagged as a destination (the tile's flags byte at [-1] has bit 0x40 set),
 * records the neighbour's (x, y) into the path output buffer and marks the path
 * as "destination reached in one step" (best_path_length = 1). Reached only in
 * mode 2 (ignore-obstacles + dst-record), where the neighbour step skips the
 * passability gate, so a 0x40 tile is committed here rather than rejected.
 *
 * In FD2.LE this is a register-passing leaf with no stack frame: the neighbour
 * coordinates ride in DL/DH and the tile's marker pointer in EBX (it reads the
 * flags byte at [EBX-1]). This emit is Layer-2 equivalent: x/y are passed
 * explicitly (the caller already holds them) and btm_attr_ptr carries the EBX
 * marker pointer, so [btm_attr_ptr-1] is the same flags byte the binary tests.
 * The output buffer base is read from data_fd2_battle_pathfind_path_output_buffer_ptr
 * (the pointer the binary loads from 0x60073), and best_path_length is the byte
 * at 0x60078; both are the same globals the orchestrator set up for the search.
 *
 *   x, y         : neighbour tile coordinates (DL/DH in the binary) -> output.
 *   btm_attr_ptr : pointer to the neighbour tile's marker byte (EBX); the flags
 *                  byte tested here lives at [btm_attr_ptr-1].
 *
 * The binary loads the output-buffer pointer only inside the taken branch (after
 * the 0x40 test); this emit mirrors that by dereferencing the global inside the
 * if. void return.
 * ---------------------------------------------------------------- */
void fd2_pathfind_record_destination_xy(uint8 x, uint8 y, uint8 *btm_attr_ptr)
{
    uint8 *out_buf;

    if ((*(btm_attr_ptr - 1) & 0x40) != 0) {
        out_buf = (uint8 *)data_fd2_battle_pathfind_path_output_buffer_ptr;
        out_buf[0] = x;
        out_buf[1] = y;
        data_fd2_battle_pathfind_best_path_length = 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_pathfind_check_destination_save_path @ 0x4E401 (2 callers: the
 * orchestrator fd2_pathfind_to_destination @ 0x4E1A6 for the initial
 * origin-equals-destination check, and the mode-0/1 commit branch of
 * fd2_pathfind_neighbor_step_with_tiebreak @ 0x4E330 on every step)
 *
 * Path-completion check: when the current tile (x, y) is the destination and the
 * search reached it at a depth that is no worse than the best path found so far,
 * record this depth as the new best and snapshot the current direction sequence
 * (one direction byte per recursion level, taken from the step stack) into the
 * path output buffer.
 *
 * In FD2.LE this is a register-passing leaf with no stack frame: the current
 * tile coordinates ride in DL/DH and everything else is read from the pathfind
 * globals; it has no CALL instructions and returns void. This emit passes x/y
 * explicitly (DL/DH; the caller already holds them) and is otherwise a direct
 * Layer-2 transcription -- same global reads/writes, same guard order, same loop.
 *
 *   x, y : current tile coordinates (DL/DH in the binary).
 *
 * The depth-vs-best test is unsigned (binary CMP AH,[best]; JA), and
 * best_path_length is overwritten with the current depth as soon as that test
 * passes -- before the depth-nonzero guard -- so a zero-depth arrival still
 * lowers the best to 0 (binary stores AH into [0x60078], then OR AH,AH / JZ).
 * The copy loop walks the step stack with an 8-byte stride, taking each frame's
 * direction byte at [+3] (the binary's MOV AL,[ESI+3] / STOSB / ADD ESI,8), and
 * runs current_depth times into successive output-buffer bytes. void return.
 * ---------------------------------------------------------------- */
void fd2_pathfind_check_destination_save_path(uint8 x, uint8 y)
{
    uint8 *out_iter;
    uint8 *stack_iter;
    uint8  remain;

    if (x == data_fd2_battle_pathfind_dst_x
        && y == data_fd2_battle_pathfind_dst_y
        && data_fd2_battle_pathfind_current_depth
               <= data_fd2_battle_pathfind_best_path_length) {
        data_fd2_battle_pathfind_best_path_length =
            data_fd2_battle_pathfind_current_depth;
        if (data_fd2_battle_pathfind_current_depth != 0) {
            stack_iter = data_fd2_battle_pathfind_step_stack;
            out_iter = (uint8 *)data_fd2_battle_pathfind_path_output_buffer_ptr;
            remain = data_fd2_battle_pathfind_current_depth;
            do {
                *out_iter = stack_iter[3];
                stack_iter += 8;
                out_iter += 1;
                remain--;
            } while (remain != 0);
        }
    }
}

/* ----------------------------------------------------------------
 * Pathfind state globals (.object3, 0x60060+)
 *
 * Runtime-populated pathfinding state shared between the movement-range
 * flood-fill and the destination pathfinder. Each field is written by
 * fd2_init_movement_range_floodfill / fd2_pathfind_to_destination before
 * any reader runs, so the load-time value is zero (BSS).
 * ---------------------------------------------------------------- */

/*
 * tile-attribute -> movement-cost primary lookup table pointer @ 0x60060.
 * Holds a caller-supplied table base (passed as pCost_table). Readers
 * dereference it as (uint8 *): cost_idx = table[(attr & 0x3FF) * 4 + 1].
 * Zero at load; set at runtime by both pathfind orchestrators.
 */
uint32 data_fd2_battle_pathfind_tile_cost_table_ptr;

/*
 * battle tile map base pointer @ 0x60064.
 * Holds the caller-supplied battle tile map base (passed as pTile_map, a
 * byte * to the 4-byte-per-tile map). Both orchestrators write the full
 * 32-bit pointer (MOV [0x60064],EAX) at entry, then read it back as the
 * base for tile-address arithmetic (ADD EBX,dword ptr [0x60064]); map_width
 * and map_height are read from [ptr+0] and [ptr+2]. Zero at load; set at
 * runtime by both pathfind orchestrators.
 */
uint32 data_fd2_battle_pathfind_battle_tile_map_ptr;

/*
 * battle map width (tiles per row) @ 0x60068.
 * Single unsigned byte read from the tile-map header [pTile_map + 0] by
 * both orchestrators (MOV BL,byte ptr [EAX]; MOV byte ptr [0x60068],BL),
 * with map_height stored in the adjacent byte at 0x60069. Readers load it
 * 8-bit and zero-extend (MOV AL,[0x60068]; XOR AH,AH) before using it as
 * the unsigned row stride in tile-index math (map_width * src_y + src_x).
 * Zero at load; set at runtime by both pathfind orchestrators.
 */
uint8 data_fd2_battle_pathfind_map_width;

/*
 * battle map height (number of tile rows) @ 0x60069.
 * Single unsigned byte read from the tile-map header [pTile_map + 2] by both
 * orchestrators (MOV [0x60069] from the map header), stored in the byte
 * adjacent to map_width at 0x60068. Readers load it 8-bit and use it
 * unsigned as the down-direction bound in the flood-fill / pathfinder:
 * (uint8)(y + 1) < map_height. Zero at load; set at runtime by both
 * pathfind orchestrators.
 */
uint8 data_fd2_battle_pathfind_map_height;

/*
 * caller-supplied secondary cost-table base / caller context @ 0x6006A.
 * Both orchestrators write the full 32-bit value at entry as the FIRST store
 * of the pathfind setup (MOV ESI,[EBP+8]; MOV dword ptr [0x6006A],ESI at
 * 0x4E047 and 0x4E1AD). In the original binary the recursion leaves inherit
 * that value through the live ESI register, so Ghidra records only the two
 * writes and no direct memory reads; the leaf helpers
 * fd2_flood_fill_neighbor_step / fd2_pathfind_neighbor_step_with_tiebreak read
 * it back here as the secondary cost-table base:
 * tile_cost = *(uint8 *)(ctx + cost_idx). Stored 32-bit (used as an address);
 * zero at load, set at runtime by both pathfind orchestrators.
 */
uint32 data_fd2_battle_pathfind_caller_context;

/*
 * flood-fill / pathfind origin tile X (source column) @ 0x6006E.
 * Single unsigned byte. Both orchestrators write it at entry from the src_x
 * argument truncated to its low byte (MOV EAX,[EBP+0xC]; MOV [0x6006E],AL at
 * 0x4E050 and 0x4E1B6), in the contiguous seed block seed_x(0x6006E) /
 * seed_y(0x6006F) / max_steps(0x60070). Readers load it 8-bit and zero-extend
 * (XOR AH,AH; MOV AL,[0x6006E] at 0x4E0A7/0x4E22D; MOV DL,byte ptr [0x6006E]
 * at 0x4E0BA/0x4E240) before using it unsigned as the column term in the
 * origin tile-index math ((map_width * seed_y + seed_x) * 4 + 7). Scalar, not
 * an array (0x6006F is the separate seed_y field). Zero at load; set at
 * runtime by both pathfind orchestrators.
 */
uint8 data_fd2_battle_pathfind_floodfill_seed_x;

/*
 * flood-fill / pathfind origin tile Y (source row) @ 0x6006F.
 * Single unsigned byte. Both orchestrators write it at entry from the src_y
 * argument truncated to its low byte (MOV EAX,[EBP+0x10]; MOV [0x6006F],AL at
 * 0x4E058 and 0x4E1BE), in the contiguous seed block seed_x(0x6006E) /
 * seed_y(0x6006F) / max_steps(0x60070). Readers load it 8-bit and use it
 * unsigned as the row term in the origin tile-index math: it is the multiplier
 * in map_width * seed_y (MOV AH,byte ptr [0x6006F]; MUL AH at 0x4E09B/0x4E221)
 * and is also reloaded into DH (MOV DH,byte ptr [0x6006F] at 0x4E0C0/0x4E246)
 * as the starting y coordinate handed to the recursion. Scalar, not an array
 * (0x6006E is the separate seed_x field, 0x60070 the separate max_steps).
 * Zero at load; set at runtime by both pathfind orchestrators.
 */
uint8 data_fd2_battle_pathfind_floodfill_seed_y;

/*
 * flood-fill / pathfind step budget (max_steps / range_remaining) @ 0x60070.
 * Single unsigned byte, last field of the contiguous seed block seed_x(0x6006E)
 * / seed_y(0x6006F) / max_steps(0x60070). Both orchestrators write it at entry
 * from the step-budget argument truncated to its low byte (MOV EAX,[EBP+0x14];
 * MOV [0x60070],AL at 0x4E060 and 0x4E1C6). It is then read 8-bit (MOV CL,byte
 * ptr [0x60070] at 0x4E0C6 / 0x4E24C) and stored as the origin tile's initial
 * movement cost (tile_map[(map_width*seed_y+seed_x)*4+7] = max_steps), giving
 * the flood-fill / recursion a full budget to decrement during expansion.
 * Scalar, not an array (0x6006F is the separate seed_y field). Zero at load;
 * set at runtime by both pathfind orchestrators.
 */
uint8 data_fd2_battle_pathfind_floodfill_max_steps;

/*
 * pathfind destination tile X (target column) @ 0x60071.
 * Single unsigned byte, first field of the destination-coord pair dst_x(0x60071)
 * / dst_y(0x60072). Written only by the path-aware orchestrator
 * fd2_pathfind_to_destination at entry from the dst_x argument truncated to its
 * low byte (MOV EAX,[EBP+0x1C]; MOV [0x60071],AL at 0x4E1D6); the plain
 * flood-fill orchestrator never sets it. Read 8-bit by
 * fd2_pathfind_check_destination_save_path as the destination column compared
 * against the current search position (in_DL == data_fd2_battle_pathfind_dst_x
 * at 0x4E409) to decide arrival. Scalar, not an array (0x60072 is the separate
 * dst_y field). Zero at load; set at runtime by the path-aware orchestrator.
 */
uint8 data_fd2_battle_pathfind_dst_x;

/*
 * pathfind destination tile Y (target row) @ 0x60072.
 * Single unsigned byte, second field of the destination-coord pair dst_x(0x60071)
 * / dst_y(0x60072). Written only by the path-aware orchestrator
 * fd2_pathfind_to_destination at entry from the dst_y argument truncated to its
 * low byte (MOV EAX,[EBP+0x20]; MOV [0x60072],AL at 0x4E1DE); the plain
 * flood-fill orchestrator never sets it. Read 8-bit by
 * fd2_pathfind_check_destination_save_path as the destination row compared
 * against the current search position (in_DH == data_fd2_battle_pathfind_dst_y
 * at 0x4E409) to decide arrival. Scalar, not an array (0x60071 is the separate
 * dst_x field). Zero at load; set at runtime by the path-aware orchestrator.
 */
uint8 data_fd2_battle_pathfind_dst_y;

/*
 * pathfind path output buffer base pointer @ 0x60073.
 * Holds the caller-supplied output-buffer base where the search records the
 * resulting path (passed as the output-buffer argument). Written only by the
 * path-aware orchestrator fd2_pathfind_to_destination at entry, storing the
 * full 32-bit pointer (MOV EAX,[EBP+0x18]; MOV [0x60073],EAX at 0x4E1CE); the
 * plain flood-fill orchestrator never sets it. Read back by the path-recording
 * helpers as a (uint8 *) and written through byte-by-byte:
 * fd2_pathfind_record_destination_xy stores the landed (x, y) at [0]/[1], and
 * fd2_pathfind_check_destination_save_path copies the direction stack into it
 * (out_iter[0] = dir; out_iter += 1) for best_path_length steps. Single scalar
 * pointer, not a table. Zero at load; set at runtime by the path-aware
 * orchestrator.
 */
uint32 data_fd2_battle_pathfind_path_output_buffer_ptr;
