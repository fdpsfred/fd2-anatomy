/*
 * input.c — Keyboard polling, BIOS tick timing, keyboard buffer ops
 *
 * Accesses BIOS Data Area (BDA) directly via DOS/4G flat memory model.
 * 0x41A = keyboard buffer head, 0x41C = tail, 0x46C = tick counter.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <dos.h>
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_check_keyboard_buffer_nonempty @ 0x10620  (17 callers)
 *
 * Returns nonzero if BIOS keyboard buffer has pending input.
 * ---------------------------------------------------------------- */
int fd2_check_keyboard_buffer_nonempty(void)
{
    return BIOS_KBD_TAIL != BIOS_KBD_HEAD;
}

/* ----------------------------------------------------------------
 * fd2_clear_keyboard_buffer @ 0x4E031  (62 callers)
 *
 * Drains keyboard buffer by setting TAIL = HEAD.
 * ---------------------------------------------------------------- */
void fd2_clear_keyboard_buffer(void)
{
    BIOS_KBD_TAIL = BIOS_KBD_HEAD;
}

/* ----------------------------------------------------------------
 * fd2_read_bios_midnight_tick @ 0x4DFC0  (2 callers)
 *
 * Returns low 16 bits of BIOS tick counter (~18.2 Hz).
 * ---------------------------------------------------------------- */
uint16 fd2_read_bios_midnight_tick(void)
{
    return (uint16)BIOS_TICK_COUNT;
}

/* ----------------------------------------------------------------
 * fd2_wait_one_bios_tick @ 0x13460  (4 callers)
 *
 * Busy-spins until BIOS tick advances at least 1 step (~55 ms).
 *
 * The BIOS tick is read as a SIGN-EXTENDED 16-bit word (asm:
 * MOVSX EAX,word ptr [0x46C]) at both the spin-compare and the
 * cache-store, NOT as a full 32-bit dword. The sign-extended int32
 * is stored verbatim into the uint32 cache (MOV [0x53A0C],EAX), so
 * a low word of 0xFFFF caches as 0xFFFFFFFF.
 * ---------------------------------------------------------------- */
void fd2_wait_one_bios_tick(void)
{
    while ((uint32)(int32)(int16)BIOS_TICK_WORD
           == data_fd2_engine_wait_one_bios_tick_last_seen) {
    }
    data_fd2_engine_wait_one_bios_tick_last_seen =
        (uint32)(int32)(int16)BIOS_TICK_WORD;
}

/* ----------------------------------------------------------------
 * fd2_wait_n_bios_ticks @ 0x17AA9  (96 callers)
 *
 * Busy-waits until BIOS tick has advanced by at least n_ticks.
 *
 * Both cache-stores read the BIOS tick as a SIGN-EXTENDED 16-bit
 * word (asm: MOVSX EAX,word ptr [0x46C]; MOV [0x53A2C],EAX), NOT as
 * a full 32-bit dword — identical to the sibling fd2_wait_one_bios_tick.
 * The sign-extended int32 is stored verbatim, so a low word of 0xFFFF
 * caches as 0xFFFFFFFF.
 *
 * NOTE: Ghidra decompiler has a bug here — renders the subtraction
 * as "tick - tick" (= 0). Assembly confirms it reads
 * wait_n_bios_ticks_last_seen as the second operand.
 * ---------------------------------------------------------------- */
void fd2_wait_n_bios_ticks(uint32 n_ticks)
{
    int elapsed;

    data_fd2_engine_wait_n_bios_ticks_last_seen =
        (uint32)(int32)(int16)BIOS_TICK_WORD;
    do {
        elapsed = (int)(int16)
            ((uint16)BIOS_TICK_COUNT
           - (uint16)data_fd2_engine_wait_n_bios_ticks_last_seen);
        if (elapsed < 0) {
            elapsed = elapsed + 0x10000;
        }
    } while (elapsed < (int)n_ticks);
    data_fd2_engine_wait_n_bios_ticks_last_seen =
        (uint32)(int32)(int16)BIOS_TICK_WORD;
}

/* ----------------------------------------------------------------
 * fd2_wait_input_with_recruitment_repaint @ 0x32004  (1 caller)
 *
 * Wait for key with throttled recruitment screen repaint.
 * Extra remap: ASCII space (0x20) also maps to Enter (0x1C).
 * ---------------------------------------------------------------- */
int fd2_wait_input_with_recruitment_repaint(uint32 panel_buf,
                                              uint32 max_chars,
                                              uint32 sel_state,
                                              uint32 cursor_idx)
{
    while (fd2_check_keyboard_buffer_nonempty() == 0) {
        if ((int)(int16)BIOS_TICK_WORD
            != (int)data_fd2_ui_recruitment_screen_repaint_tick_latch) {
            data_fd2_ui_recruitment_screen_repaint_tick_latch =
                (uint32)(int16)BIOS_TICK_WORD;
            fd2_render_recruitment_select_screen(
                panel_buf, max_chars, sel_state, cursor_idx);
            memmove((void *)0xA0000,
                    (void *)data_fd2_ui_slide_composed_target_buf_ptr,
                    64000);
        }
    }
    data_fd2_input_key_input_mode = 0x10;
    int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                 (union REGS *)&data_fd2_input_last_key_pressed);
    if (data_fd2_input_key_input_mode == 0xE0
        || data_fd2_input_key_input_mode == 0x52
        || data_fd2_input_last_key_pressed == 0x20) {
        data_fd2_input_key_input_mode = 0x1C;
    }
    if (data_fd2_input_key_input_mode == 0x53) {
        data_fd2_input_key_input_mode = 0x01;
    }
    return (int)(uint32)data_fd2_input_key_input_mode;
}

/* ----------------------------------------------------------------
 * fd2_wait_input_with_chapter_dialog_blink @ 0x2D85F
 *
 * Wait for key while cycling chapter-intro dialog panels (4 frames)
 * and portrait blink. If mode==0, also blits corner indicator sprites.
 * Returns scancode after INT 16h + remap.
 * ---------------------------------------------------------------- */
int fd2_wait_input_with_chapter_dialog_blink(uint32 mode)
{
    uint32 corner_offs[4];
    uint32 saved_tick;
    int blink_countdown;
    uint8 blinking_flag;
    uint32 rng_val;
    uint32 i;
    int elapsed;
    int cur_tick;
    uint32 sprite_idx;

    blinking_flag = 0;
    corner_offs[0] = data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[0];
    corner_offs[1] = data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[1];
    corner_offs[2] = data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[2];
    corner_offs[3] = data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[3];

    saved_tick = (uint32)(uint16)BIOS_TICK_WORD;
    rng_val = fd2_advance_rng_state();
    blink_countdown = (int)(rng_val % 0x32) + 8;

    data_fd2_chapter_intro_dialog_anim_frame_idx = 2;
    fd2_render_chapter_intro_dialog_panels((uint32)corner_offs, mode);

    if (mode == 0) {
        for (i = 0; i < 4; i++) {
            sprite_idx = i * 2 + 3;
            if (i == data_fd2_ui_menu_cursor_idx) {
                sprite_idx++;
            }
            fd2_blit_sprite_with_stride_setup(
                corner_offs[i] + 0xAD430,
                data_fd2_ui_menu_screen_sprite_atlas_buf_ptr
                    + *(uint32 *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr
                                  + 6 + sprite_idx * 4),
                0x140);
        }
    }

    do {
        cur_tick = (int)(int16)(uint16)BIOS_TICK_WORD;
        elapsed = cur_tick - (int)(int16)(uint16)saved_tick;
        if (elapsed >= 2 || cur_tick < (int)(int16)(uint16)saved_tick) {
            data_fd2_chapter_intro_dialog_anim_frame_idx++;
            if (data_fd2_chapter_intro_dialog_anim_frame_idx == 4) {
                data_fd2_chapter_intro_dialog_anim_frame_idx = 0;
            }
            fd2_render_chapter_intro_dialog_panels(
                (uint32)corner_offs, mode);

            if (blinking_flag != 0) {
                fd2_paint_portrait_to_dialog_area(0);
                rng_val = fd2_advance_rng_state();
                blink_countdown = (int)(rng_val % 0x1E) + 2;
                blinking_flag = 0;
            } else {
                int old_cd;
                old_cd = blink_countdown;
                blink_countdown--;
                if (old_cd == 0) {
                    fd2_paint_portrait_to_dialog_area(3);
                    blinking_flag = 1;
                }
            }
            saved_tick = (uint32)(uint16)BIOS_TICK_WORD;
        }
    } while (fd2_check_keyboard_buffer_nonempty() == 0);

    data_fd2_input_key_input_mode = 0x10;
    int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                 (union REGS *)&data_fd2_input_last_key_pressed);
    if (data_fd2_input_key_input_mode == 0xE0
        || data_fd2_input_key_input_mode == 0x52) {
        data_fd2_input_key_input_mode = 0x1C;
    }
    if (data_fd2_input_key_input_mode == 0x53) {
        data_fd2_input_key_input_mode = 0x01;
    }
    return (int)(uint32)data_fd2_input_key_input_mode;
}

/* ----------------------------------------------------------------
 * fd2_wait_input_with_status_panel_repaint @ 0x18B84  (1 caller)
 *
 * Wait until keyboard input available while repainting battle
 * field + mini-status panel for char_idx. Does NOT read the key.
 * Panel side chosen by cursor_screen_x (<7 → left, else right).
 * ---------------------------------------------------------------- */
void fd2_wait_input_with_status_panel_repaint(uint32 char_idx)
{
    uint32 panel_off;
    int last_tick;

    panel_off = (data_fd2_battle_cursor_screen_x < 7)
              ? 0x984 : 0x8ED;

    while (fd2_check_keyboard_buffer_nonempty() == 0) {
        fd2_update_palette_cycle_anim();
        if ((int)(int16)BIOS_TICK_WORD != last_tick) {
            fd2_update_palette_cycle_anim();
            fd2_tick_chapter_palette_animation();
            fd2_composite_battle_tile_map(
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, 0xD, 8,
                data_fd2_battle_view_window_origin_x,
                data_fd2_battle_view_window_origin_y);
            fd2_paint_cursor_overlay_pattern();
            fd2_composite_all_chars_overlay();
            fd2_render_mini_char_status_panel(
                data_fd2_large_game_state_buffer_ptr + 0x8088
                    + panel_off,
                0x1C8, char_idx);
            fd2_render_terrain_info_hud_panel(
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8);
            fd2_blit_rectangle(0xA0504, 0x140,
                data_fd2_large_game_state_buffer_ptr + 0x8088,
                0x1C8, 0x138, 0xC0);
            last_tick = (int)(int16)BIOS_TICK_WORD;
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_wait_input_with_dialog_repaint @ 0x17898
 *
 * Wait for key while repainting dialog background + borders.
 * Includes blink oscillator (0/1 toggle every >3 ticks) and
 * full tile-map + chars + HUD composite each frame.
 * ---------------------------------------------------------------- */
int fd2_wait_input_with_dialog_repaint(uint32 menu_state,
                                        uint32 pSlot_disable_arr)
{
    while (fd2_check_keyboard_buffer_nonempty() == 0) {
        int tick_diff;
        fd2_update_palette_cycle_anim();
        tick_diff = (int)(int16)(uint16)BIOS_TICK_WORD
                  - (int)data_fd2_dialog_blink_phase_oscillator_tick_latch;
        if (tick_diff > 3 || tick_diff < 0) {
            data_fd2_dialog_blink_phase_oscillator++;
            if (data_fd2_dialog_blink_phase_oscillator == 2) {
                data_fd2_dialog_blink_phase_oscillator = 0;
            }
            data_fd2_dialog_blink_phase_oscillator_tick_latch =
                (uint32)(int16)(uint16)BIOS_TICK_WORD;
        }
        fd2_tick_chapter_palette_animation();
        fd2_composite_battle_tile_map(
            data_fd2_large_game_state_buffer_ptr + 0x8088,
            0x1C8, 0xD, 8,
            data_fd2_battle_view_window_origin_x,
            data_fd2_battle_view_window_origin_y);
        fd2_composite_all_chars_overlay();
        fd2_render_terrain_info_hud_panel(
            data_fd2_large_game_state_buffer_ptr + 0x8088, 0x1C8);
        fd2_repaint_settings_dialog_borders(
            menu_state, pSlot_disable_arr);
        fd2_blit_rectangle(0xA0504, 0x140,
            data_fd2_large_game_state_buffer_ptr + 0x8088,
            0x1C8, 0x138, 0xC0);
    }
    data_fd2_input_key_input_mode = 0x10;
    int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                 (union REGS *)&data_fd2_input_last_key_pressed);
    if (data_fd2_input_key_input_mode == 0xE0
        || data_fd2_input_key_input_mode == 0x52) {
        data_fd2_input_key_input_mode = 0x1C;
    }
    if (data_fd2_input_key_input_mode == 0x53) {
        data_fd2_input_key_input_mode = 0x01;
    }
    return (int)(uint32)data_fd2_input_key_input_mode;
}

/* ----------------------------------------------------------------
 * fd2_wait_for_input_dialog_with_blink @ 0x16C57
 *
 * Wait for key while animating portrait blink (random 2..31 tick
 * interval) and optional ▼ arrow (toggles sprite 0x12/0x13 every
 * 3 ticks). Ends with INT 16h + scancode remap.
 * ---------------------------------------------------------------- */
int fd2_wait_for_input_dialog_with_blink(uint32 animated_cursor_mode)
{
    uint8 blink_state;
    uint32 arrow_sprite_idx;
    uint32 arrow_anim_div;
    uint32 blink_period;
    uint32 pos_offset;
    uint32 saved_tick;
    int blink_countdown;
    uint32 rng_val;
    int old_cd;

    blink_state = 0;
    arrow_sprite_idx = 0x12;
    arrow_anim_div = 0;

    blink_period = 0x47A0;
    if (data_fd2_battle_tile_map_ptr == 0) {
        blink_period = 0x4770;
    }

    saved_tick = (uint32)(uint16)BIOS_TICK_WORD;
    rng_val = fd2_advance_rng_state();
    blink_countdown = (int)(rng_val % 30) + 2;

    if (data_fd2_dialog_active_portrait_blit_offset == 0x728) {
        pos_offset = 0xA0B4F;
    } else {
        pos_offset = 0xA951F;
    }

    if (animated_cursor_mode == 1) {
        fd2_blit_sheet_sprite_at_offset(
            pos_offset + blink_period + 0x640, 0x140,
            data_fd2_ui_anim_sprite_sheet_ptr, 0x12);
    }

    while (fd2_check_keyboard_buffer_nonempty() == 0) {
        fd2_update_palette_cycle_anim();
        if ((int)(int16)(uint16)BIOS_TICK_WORD
            - (int)(int16)(uint16)saved_tick >= 2) {

            if (animated_cursor_mode == 1) {
                arrow_anim_div++;
                if (arrow_anim_div == 3) {
                    arrow_anim_div = 0;
                    arrow_sprite_idx++;
                    if (arrow_sprite_idx == 0x14) {
                        arrow_sprite_idx = 0x12;
                    }
                    fd2_blit_sheet_sprite_at_offset(
                        pos_offset + blink_period + 0x640,
                        0x140,
                        data_fd2_ui_anim_sprite_sheet_ptr,
                        arrow_sprite_idx);
                }
            }

            if (blink_state != 0) {
                fd2_paint_portrait_to_dialog_area(0);
                rng_val = fd2_advance_rng_state();
                blink_countdown = (int)(rng_val % 30) + 2;
                blink_state = 0;
            } else {
                old_cd = blink_countdown;
                blink_countdown--;
                if (old_cd == 0) {
                    fd2_paint_portrait_to_dialog_area(3);
                    blink_state = 1;
                }
            }
            saved_tick = (uint32)(uint16)BIOS_TICK_WORD;
        }
    }

    if (animated_cursor_mode == 1) {
        fd2_blit_sheet_sprite_at_offset(
            pos_offset + blink_period, 0x140,
            data_fd2_ui_anim_sprite_sheet_ptr, 0x0D);
    }

    data_fd2_input_key_input_mode = 0x10;
    int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                 (union REGS *)&data_fd2_input_last_key_pressed);
    if (data_fd2_input_key_input_mode == 0xE0
        || data_fd2_input_key_input_mode == 0x52) {
        data_fd2_input_key_input_mode = 0x1C;
    }
    if (data_fd2_input_key_input_mode == 0x53) {
        data_fd2_input_key_input_mode = 0x01;
    }
    return (int)(uint32)data_fd2_input_key_input_mode;
}

/* ----------------------------------------------------------------
 * fd2_wait_ticks_or_keypress_with_palette @ 0x1E5C0  (3 callers)
 *
 * Wait up to max_ticks BIOS ticks or until a key is pressed.
 * Keeps palette cycle animation running during the wait.
 * Clears keyboard buffer on exit.
 * ---------------------------------------------------------------- */
void fd2_wait_ticks_or_keypress_with_palette(uint32 max_ticks)
{
    int start_tick;
    uint8 keypress;
    int current_tick;

    start_tick = (int)(int16)BIOS_TICK_WORD;
    do {
        fd2_update_palette_cycle_anim();
        keypress = (uint8)fd2_check_keyboard_buffer_nonempty();
        current_tick = (int)(int16)BIOS_TICK_WORD;
        if (current_tick - start_tick >= (int)max_ticks
            || current_tick < start_tick) {
            keypress = 1;
        }
    } while (keypress == 0);
    fd2_clear_keyboard_buffer();
}

/* ----------------------------------------------------------------
 * fd2_wait_for_input_v2 @ 0x12DAC
 *
 * Block until keyboard input, with palette-cycle anim + battle-frame
 * redraw per BIOS tick. Same INT 16h + scancode remap as
 * fd2_wait_for_input_with_idle.
 *
 * Tick state: the binary keeps last_tick in the ESI register, which it
 * PUSH/POP-saves, so no state is propagated back to the caller. ESI is
 * NOT initialized inside this routine — on entry last_tick is whatever
 * the caller left in ESI (the sole caller, fd2_wait_for_action_target_input,
 * holds target_iter there: an arbitrary option index 0..n_options-1).
 * Unlike fd2_wait_for_input_with_idle (which persists tick state across
 * calls via globals 0x539F0/0x539F2), v2 tracks the redraw tick per-call.
 * The seed only affects whether the FIRST idle iteration redraws before
 * the first BIOS-tick change; both the binary's junk seed and our 0 almost
 * always differ from the free-running tick, so iteration 1 redraws either
 * way. See the declaration note below for the equivalence divergence.
 * ---------------------------------------------------------------- */
int fd2_wait_for_input_v2(void)
{
    /*
     * The binary seeds last_tick from the incoming ESI register (= caller
     * target_iter); we use 0. This is unportable to express exactly (reading
     * a caller's register), and 0 is the correct portable rendering. It only
     * changes the iteration-1 redraw decision: fd2_composite_battle_frame(0)
     * advances chapter/cycle palette-anim counters (display state only), and
     * the divergence is bounded to at most one redraw on the FIRST loop pass
     * before the first BIOS-tick change — it converges immediately and never
     * affects the return value, game state, RNG, or save data.
     */
    int last_tick;

    last_tick = 0;
    while (fd2_check_keyboard_buffer_nonempty() == 0) {
        fd2_update_palette_cycle_anim();
        if (last_tick != (int)(int16)BIOS_TICK_WORD) {
            fd2_composite_battle_frame(0);
            last_tick = (int)(int16)BIOS_TICK_WORD;
        }
    }
    data_fd2_input_key_input_mode = 0x10;
    int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                 (union REGS *)&data_fd2_input_last_key_pressed);
    if (data_fd2_input_key_input_mode == 0xe0
        || data_fd2_input_key_input_mode == 0x52) {
        data_fd2_input_key_input_mode = 0x1c;
    }
    if (data_fd2_input_key_input_mode == 0x53) {
        data_fd2_input_key_input_mode = 0x01;
    }
    return (int)(uint32)data_fd2_input_key_input_mode;
}

/* ----------------------------------------------------------------
 * fd2_wait_for_input_with_idle @ 0x11AA8
 *
 * Main input loop with idle animation. Busy-polls keyboard; while
 * no key pending, advances palette cycle and redraws battle frame
 * at ~18.2 Hz. On key, reads via INT 16h and remaps extended keys.
 * ---------------------------------------------------------------- */
int fd2_wait_for_input_with_idle(void)
{
    int16 current_tick;

    while (fd2_check_keyboard_buffer_nonempty() == 0) {
        fd2_update_palette_cycle_anim();
        data_fd2_input_idle_current_bios_tick_word = BIOS_TICK_WORD;
        current_tick = (int16)BIOS_TICK_WORD;
        if (current_tick
            != (int16)data_fd2_input_idle_last_rendered_tick_word) {
            fd2_composite_battle_frame(0);
            data_fd2_input_idle_last_rendered_tick_word = BIOS_TICK_WORD;
        }
    }
    data_fd2_input_key_input_mode = 0x10;
    int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                 (union REGS *)&data_fd2_input_last_key_pressed);
    if (data_fd2_input_key_input_mode == 0xe0
        || data_fd2_input_key_input_mode == 0x52) {
        data_fd2_input_key_input_mode = 0x1c;
    }
    if (data_fd2_input_key_input_mode == 0x53) {
        data_fd2_input_key_input_mode = 0x01;
    }
    return (int)(uint32)data_fd2_input_key_input_mode;
}

/* ----------------------------------------------------------------
 * fd2_wait_for_action_target_input @ 0x115B6
 *
 * Interactive cursor input loop for selecting a target tile/char.
 * Returns  1 = target committed,  -1 = ESC cancelled.
 *
 * Mode 4: commit if tile passable (skip chars-in-range check).
 * Mode 5: ENTER/SPACE always loops (never commits via this path).
 * Mode 6: commit only if NO living ally at cursor AND terrain
 *          passable; if a living ally IS found, keep looping.
 * Other:  commit if tile passable AND manhattan-range has targets.
 * ---------------------------------------------------------------- */
int fd2_wait_for_action_target_input(int mode, uint32 n_options,
                                      uint8 *pTarget_array)
{
    uint32 target_iter;
    uint32 self_idx_in_mode6;
    uint32 anim_phase_ref;
    uint8 found_target_flag;
    uint8 tile_buf[8];
    uint8 current_char_idx;
    int scancode;
    uint8 job;
    uint8 *move_cost;
    uint32 char_iter;
    int hit_count;

    target_iter = 0;
    if (mode == 6) {
        self_idx_in_mode6 = n_options;
        n_options = 0;
    }

    anim_phase_ref = data_fd2_battle_anim_phase;
    if ((int)anim_phase_ref > 1) {
        anim_phase_ref = anim_phase_ref - 1;
    }

    if (n_options == 0) goto wait_input;
    current_char_idx = pTarget_array[0];

check_and_pan:
    if (data_fd2_battle_runtime_char_array_ptr[
            (uint32)current_char_idx].portrait_id != 0x79) {
        fd2_pan_cursor_to_char((uint32)current_char_idx);
    }

wait_input:
    for (;;) {
        scancode = fd2_wait_for_input_v2();
        if (scancode == 0x01) {
            return -1;
        }
        if (scancode == 0x39 || scancode == 0x1c) {
            if (mode == 6) {
                found_target_flag = 0;
                for (char_iter = 0;
                     (int)char_iter
                         < (int)data_fd2_battle_party_member_count;
                     char_iter++) {
                    if (char_iter != self_idx_in_mode6
                        && (uint32)data_fd2_battle_runtime_char_array_ptr
                               [char_iter].pos_x
                               == data_fd2_battle_cursor_world_x
                        && (uint32)data_fd2_battle_runtime_char_array_ptr
                               [char_iter].pos_y
                               == data_fd2_battle_cursor_world_y
                        && fd2_check_char_is_dead(char_iter) == 0) {
                        found_target_flag = 1;
                    }
                }
                if (found_target_flag != 0) {
                    continue;
                }
                job = data_fd2_battle_runtime_char_array_ptr[
                          self_idx_in_mode6].job_id;
                if (data_fd2_battle_runtime_char_array_ptr[
                        self_idx_in_mode6].portrait_id == 0x1c) {
                    job = 1;
                }
                if (fd2_check_char_status_immunity(
                        self_idx_in_mode6) != 0) {
                    job = 0x13;
                }
                move_cost = fd2_get_movement_cost_table_for_job(
                                (uint32)job);
                fd2_read_tile_attribute_at_pos(
                    data_fd2_battle_cursor_world_x,
                    data_fd2_battle_cursor_world_y,
                    (uint32)tile_buf);
                if (move_cost[tile_buf[5]] == 0x14) {
                    continue;
                }
                return 1;
            }
            if (mode != 5
                && *(uint8 *)(data_fd2_battle_tile_map_ptr
                       + (data_fd2_battle_cursor_world_y
                          * data_fd2_battle_map_width_tiles
                          + data_fd2_battle_cursor_world_x) * 4
                       + 7) != 0xff) {
                if (mode != 4) {
                    hit_count =
                        fd2_scan_chars_within_manhattan_range(
                            data_fd2_battle_cursor_world_x,
                            data_fd2_battle_cursor_world_y,
                            anim_phase_ref, 0, mode);
                    if (hit_count == 0) {
                        continue;
                    }
                }
                return 1;
            }
            continue;
        }
        if ((scancode == 0x2c || scancode == 0x4c)
            && n_options != 0) {
            target_iter = target_iter + 1;
            if (target_iter == n_options) {
                target_iter ^= n_options;
            }
            current_char_idx = pTarget_array[target_iter];
            goto check_and_pan;
        }
        if (scancode == 0x48) {
            fd2_cursor_move_up();
        } else if (scancode == 0x50) {
            fd2_cursor_move_down();
        } else if (scancode == 0x4b) {
            fd2_cursor_move_left();
        } else if (scancode == 0x4d) {
            fd2_cursor_move_right();
        } else {
            continue;
        }
        fd2_play_sfx_with_handle(
            data_fd2_audio_fdother_sfx_bank_buf_ptr, 0, 1);
    }
}

/* ----------------------------------------------------------------
 * Data definitions
 * ---------------------------------------------------------------- */

/* data_fd2_input_idle_current_bios_tick_word @ 0x539F0  (zero-bss)
 *
 * Latest BIOS midnight-tick counter (0:046C, 18.2 Hz word) snapshot,
 * captured each idle iteration of fd2_wait_for_input_with_idle. Compared
 * against the last-rendered tick to drive the 18.2 Hz cursor-blink redraw.
 * Zero-initialized in BSS; first touched by a runtime write. */
uint16 data_fd2_input_idle_current_bios_tick_word;

/* data_fd2_input_idle_last_rendered_tick_word @ 0x539F2  (zero-bss)
 *
 * Last BIOS midnight-tick value (0:046C, 18.2 Hz word) for which the
 * cursor-blink frame was composited. Each idle iteration of
 * fd2_wait_for_input_with_idle compares the freshly snapshotted tick
 * against this; when they differ it re-composites the blink frame and
 * stores the new tick here, yielding the 18.2 Hz cursor blink. Read as
 * a sign-extended 16-bit word (asm: MOVSX EAX,word ptr [0x539F2]).
 * Zero-initialized in BSS; first touched by a runtime write. */
uint16 data_fd2_input_idle_last_rendered_tick_word;

/* data_fd2_engine_wait_one_bios_tick_last_seen @ 0x53A0C  (zero-bss)
 *
 * Private 1-tick frame-pacer state for fd2_wait_one_bios_tick: caches the
 * last-observed BIOS midnight-tick (0:046C, 18.2 Hz). The function spins
 * while the live tick equals this cached value, then stores the new tick
 * here so the next call waits for the following tick (~55 ms step). The
 * tick is read sign-extended to 32 bits (asm: MOVSX EAX,word ptr [0x46C])
 * and the full 32-bit EAX is compared/stored as a dword (CMP EAX,dword
 * ptr [0x53A0C] / MOV [0x53A0C],EAX), so a low word of 0xFFFF caches as
 * 0xFFFFFFFF. Only read/written by fd2_wait_one_bios_tick.
 * Zero-initialized in BSS; first touched by a runtime write. */
uint32 data_fd2_engine_wait_one_bios_tick_last_seen;

/* data_fd2_engine_wait_n_bios_ticks_last_seen @ 0x53A2C  (zero-bss)
 *
 * Private N-tick frame-pacer state for fd2_wait_n_bios_ticks: caches the
 * last-observed BIOS midnight-tick (0:046C, 18.2 Hz). On entry the function
 * snapshots the current tick here, spins until the live tick has advanced by
 * at least n_ticks, then re-stores the new tick so the next call counts from
 * the latest reference (~55 ms per tick). The tick is read sign-extended to
 * 32 bits (asm: MOVSX EAX,word ptr [0x46C]) and the full 32-bit EAX is
 * stored / read back as a dword (MOV [0x53A2C],EAX / SUB EAX,dword ptr
 * [0x53A2C]), so a low word of 0xFFFF caches as 0xFFFFFFFF. Sibling of
 * data_fd2_engine_wait_one_bios_tick_last_seen with identical semantics.
 * Only read/written by fd2_wait_n_bios_ticks.
 * Zero-initialized in BSS; first touched by a runtime write. */
uint32 data_fd2_engine_wait_n_bios_ticks_last_seen;

/* data_fd2_input_key_input_mode @ 0x53A8E  (zero-bss)
 *
 * Last-key scancode / input mode byte. Every input-wait routine first writes
 * 0x10 here (cursor-mode preset), then calls
 *   int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
 *                (union REGS *)&data_fd2_input_last_key_pressed);
 * INT 16h "read key" returns AH=scancode / AL=ASCII in AX; this byte aliases
 * the AH field of that REGS union (it sits at &data_fd2_input_last_key_pressed
 * + 1), so the INT 16h call fills it with the received scancode. The routine
 * then remaps special scancodes (0xE0 / 'R' 0x52 -> 0x1C Enter; 'S' 0x53 ->
 * 0x01 Esc) and returns this byte. Accessed only as a single byte (asm:
 * MOV byte ptr [0x53A8E],imm8 / MOVZX EAX,byte ptr [0x53A8E]).
 *
 * Layout dependency: this byte must be placed at
 * data_fd2_input_last_key_pressed + 1 for the INT 16h AH result to land here
 * (vendor union-REGS overlap); data_fd2_input_last_key_pressed (home
 * life/main.c) owns the REGS-union base.
 * Zero-initialized in BSS; first touched by a runtime write. */
uint8 data_fd2_input_key_input_mode;

/* data_fd2_dialog_blink_phase_oscillator @ 0x53C13  (zero-bss)
 *
 * Dialog cursor/border blink-phase counter. A small free-running phase index
 * advanced off the BIOS midnight tick (0:046C) and read by the dialog repaint
 * code to alternate the selected corner/box sprite frame, producing the
 * highlight-blink animation. Accessed only as a 32-bit dword at every site
 * (asm: INC dword ptr [0x53C13] / CMP dword ptr [0x53C13],imm /
 * MOV dword ptr [0x53C13],0).
 *
 * Two writers with different wrap moduli share this one counter:
 *   - fd2_wait_input_with_dialog_repaint (settings/options dialog): increments
 *     once per >3-tick step and wraps 0<->1 (CMP ...,2), so the selected
 *     border corner toggles between sprite frame A and A+1.
 *   - fd2_text_dialog_typewriter_loop (text / Yes-No prompt): increments once
 *     per >=2-tick step and wraps 0..3 (CMP ...,4); the Yes/No highlight uses
 *     value/2 as its 0/1 frame offset, and the value is reset to 0 on both
 *     exit paths (Esc -> return -1, confirm -> return 1).
 * Read-only consumer fd2_repaint_settings_dialog_borders adds this value to a
 * sprite index for the currently-selected corner.
 * Zero-initialized in BSS; first touched by a runtime read-modify-write. */
uint32 data_fd2_dialog_blink_phase_oscillator;

/* data_fd2_dialog_blink_phase_oscillator_tick_latch @ 0x53C17  (zero-bss)
 *
 * Tick reference for the dialog-blink oscillator's step divider. Latches the
 * BIOS midnight tick counter (0:046C) at the moment the oscillator above last
 * advanced; the repaint loops gate the next advance on
 * (signed) (BIOS_tick - this_latch) crossing their step threshold.
 * Accessed only as a 32-bit dword at every site (asm:
 * SUB EAX,dword ptr [0x53C17] / MOV [0x53C17],EAX with the tick sign-extended
 * via CWDE/MOVSX), and the difference is compared with signed jumps (JG/JGE),
 * so the divider re-arms correctly across the day rollover.
 *
 * Both writers of the partner counter (0x53C13) share this latch:
 *   - fd2_wait_input_with_dialog_repaint: re-latches once the diff exceeds 3.
 *   - fd2_text_dialog_typewriter_loop:    re-latches once the diff reaches 2.
 * Zero-initialized in BSS; the first loop entry reads 0, which forces an
 * immediate advance + latch of the current tick. */
uint32 data_fd2_dialog_blink_phase_oscillator_tick_latch;
