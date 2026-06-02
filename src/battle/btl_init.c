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