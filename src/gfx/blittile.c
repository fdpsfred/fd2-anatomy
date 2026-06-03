/*
 * blittile.c — 24x24 tile/sprite blit helpers (battle render workspace)
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_blit_24x24_at_window_relative_pos @ 0x126F7 (1 caller)
 *
 * Paint a 24x24 sprite at world coords (world_x, world_y) into the
 * battle render workspace, only if the tile is visible inside the
 * battle window. Out-of-window calls are silent no-ops, so callers
 * (the cursor-overlay range patterns) can pass x+-k / y+-k offsets
 * that fall outside the window without guarding.
 *
 * Window test:
 *   origin_x <= world_x < origin_x + max_x
 *   origin_y <= world_y < origin_y + max_y
 *
 * Sprite source: cursor/UI sprite atlas at
 *   data_fd2_runtime_battle_state_ptr; the per-index absolute byte
 *   offset lives in an offset table at +6 (index*4), so
 *   sprite_src = base + *(int*)(base + 6 + sprite_idx*4).
 *
 * Destination: battle render workspace at
 *   data_fd2_large_game_state_buffer_ptr + 0x8088, with row stride
 *   0x2AC0 (= 24 rows * 0x1C8 pitch) and column stride 0x18 (24
 *   bytes per tile). Blit pitch passed to the passthrough blitter
 *   is 0x1C8.
 * ---------------------------------------------------------------- */
void fd2_blit_24x24_at_window_relative_pos(uint32 world_x, uint32 world_y,
                                           uint32 sprite_idx)
{
    uint32 sprite_src;
    uint32 dst;

    if ((int)data_fd2_battle_view_window_origin_x <= (int)world_x &&
        (int)world_x < (int)(data_fd2_battle_view_window_origin_x +
                             data_fd2_battle_view_window_max_x) &&
        (int)data_fd2_battle_view_window_origin_y <= (int)world_y &&
        (int)world_y < (int)(data_fd2_battle_view_window_origin_y +
                             data_fd2_battle_view_window_max_y)) {

        sprite_src = data_fd2_runtime_battle_state_ptr +
            *(int *)(data_fd2_runtime_battle_state_ptr + 6 + sprite_idx * 4);

        dst = data_fd2_large_game_state_buffer_ptr +
            (world_y - data_fd2_battle_view_window_origin_y) * 0x2AC0 +
            (world_x - data_fd2_battle_view_window_origin_x) * 0x18 +
            0x8088;

        fd2_tile_blit_24x24_passthrough(sprite_src, dst, 0x1C8);
    }
}
