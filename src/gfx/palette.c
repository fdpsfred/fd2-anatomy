/*
 * palette.c — VGA DAC palette manipulation primitives
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <conio.h>
#include <dos.h>

/* ----------------------------------------------------------------
 * fd2_set_vga_palette_range @ 0x11D40
 *
 * Write palette entries [start..end] to VGA DAC, subtracting
 * brightness_subtract from each R/G/B component (clamped to 0).
 * Source data at data_fd2_vga_palette_data_ptr (768-byte palette).
 * ---------------------------------------------------------------- */
void fd2_set_vga_palette_range(uint32 start_idx, uint32 end_idx,
                                uint32 brightness_subtract)
{
    uint32 idx;
    int val;

    for (idx = start_idx; (int)idx <= (int)end_idx; idx++) {
        outp(0x3C8, idx);

        val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                              + idx * 3) - (int)brightness_subtract;
        if (val < 0) val = 0;
        outp(0x3C9, val);

        val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                              + idx * 3 + 1) - (int)brightness_subtract;
        if (val < 0) val = 0;
        outp(0x3C9, val);

        val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                              + idx * 3 + 2) - (int)brightness_subtract;
        if (val < 0) val = 0;
        outp(0x3C9, val);
    }
}

/* ----------------------------------------------------------------
 * fd2_set_full_vga_palette_to_color @ 0x203BD
 *
 * Set all 256 VGA palette entries to a single (R,G,B) color.
 * Used for full-screen flash effects (critical hit, lightning).
 * ---------------------------------------------------------------- */
void fd2_set_full_vga_palette_to_color(uint32 r, uint32 g, uint32 b)
{
    uint32 idx;

    for (idx = 0; (int)idx < 0x100; idx++) {
        outp(0x3C8, idx);
        outp(0x3C9, r);
        outp(0x3C9, g);
        outp(0x3C9, b);
    }
}

/* ----------------------------------------------------------------
 * fd2_set_vga_palette_range_with_add @ 0x11DF2
 *
 * Sister to fd2_set_vga_palette_range: ADDs brightness and caps
 * each channel at 0x3F (VGA 6-bit max). Used for fade-from-black.
 * ---------------------------------------------------------------- */
void fd2_set_vga_palette_range_with_add(uint32 start_idx, uint32 end_idx,
                                         uint32 brightness_add)
{
    uint32 idx;
    int val;

    for (idx = start_idx; (int)idx <= (int)end_idx; idx++) {
        outp(0x3C8, idx);

        val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                              + idx * 3) + (int)brightness_add;
        if (val > 0x3F) val = 0x3F;
        outp(0x3C9, val);

        val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                              + idx * 3 + 1) + (int)brightness_add;
        if (val > 0x3F) val = 0x3F;
        outp(0x3C9, val);

        val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                              + idx * 3 + 2) + (int)brightness_add;
        if (val > 0x3F) val = 0x3F;
        outp(0x3C9, val);
    }
}

/* ----------------------------------------------------------------
 * fd2_update_palette_cycle_anim @ 0x4DFCC
 *
 * Palette cycling for env effects (water/lava/fire). Cycles
 * palette entries 0xE0..0xEF via a 93-byte sliding-window table
 * (16 frames × 3-byte stride, each frame reads 48 sequential bytes).
 * Throttled to ~2 BIOS ticks (~110ms) between updates.
 * ---------------------------------------------------------------- */
void fd2_update_palette_cycle_anim(void)
{
    uint16 tick;
    uint32 offset;
    uint8 *rgb_ptr;
    int i;
    uint8 palette_idx;

    tick = fd2_read_bios_midnight_tick();
    if ((uint16)(tick
        - data_fd2_animation_palette_cycle_last_tick) < 2) {
        return;
    }

    data_fd2_animation_palette_cycle_frame_idx++;
    if (data_fd2_animation_palette_cycle_frame_idx == 0x10) {
        data_fd2_animation_palette_cycle_frame_idx = 0;
    }

    offset = (uint32)data_fd2_animation_palette_cycle_frame_idx * 3;
    rgb_ptr = data_fd2_animation_palette_cycle_rgb_table + offset;

    palette_idx = 0xE0;
    for (i = 0; i < 16; i++) {
        outp(0x3C8, palette_idx);
        outp(0x3C9, *rgb_ptr++);
        outp(0x3C9, *rgb_ptr++);
        outp(0x3C9, *rgb_ptr++);
        palette_idx++;
    }

    data_fd2_animation_palette_cycle_last_tick =
        fd2_read_bios_midnight_tick();
}

/* ----------------------------------------------------------------
 * fd2_tick_chapter_palette_animation @ 0x1297D
 *
 * Per-frame palette animation tick for chapter visuals.
 * Slow cycle: every 4 BIOS ticks, advance ambient palette idx 0..3.
 * Fast cycle: every call, advance walk anim alt palette idx 0..3.
 * ---------------------------------------------------------------- */
void fd2_tick_chapter_palette_animation(void)
{
    int delta;

    delta = (int)(int16)BIOS_TICK_WORD -
            (int)data_fd2_graphics_chapter_ambient_palette_anim_tick_latch;
    if (delta > 4 || delta < 0) {
        data_fd2_graphics_chapter_ambient_palette_anim_idx++;
        if (data_fd2_graphics_chapter_ambient_palette_anim_idx == 4) {
            data_fd2_graphics_chapter_ambient_palette_anim_idx = 0;
        }
        data_fd2_graphics_chapter_ambient_palette_anim_tick_latch =
            (uint32)(int)(int16)BIOS_TICK_WORD;
    }
    data_fd2_graphics_chapter_walk_anim_alt_palette_idx++;
    if (data_fd2_graphics_chapter_walk_anim_alt_palette_idx == 4) {
        data_fd2_graphics_chapter_walk_anim_alt_palette_idx = 0;
    }
}

/* ----------------------------------------------------------------
 * fd2_apply_palette_remap_run @ 0x4DB9C
 *
 * In-place byte remap: buf[i] = remap_table[buf[i]] for byte_count
 * bytes. Uses LODSB/STOSB/LOOP. byte_count must be >= 1 (do-while).
 * ---------------------------------------------------------------- */
void fd2_apply_palette_remap_run(uint32 remap_table,
                                  uint32 byte_count, uint8 *buf)
{
    do {
        *buf = *(uint8 *)(remap_table + (uint32)*buf);
        buf++;
        byte_count--;
    } while (byte_count != 0);
}

/* ----------------------------------------------------------------
 * fd2_interpolate_palette_range_toward_color @ 0x286BD  (2 callers)
 *
 * Linearly interpolate palette range [start, end) between original
 * palette and target (R,G,B). blend=0x28 → original, blend=0 → flat.
 * Formula: out = target + (palette - target) * blend / 0x28
 * ---------------------------------------------------------------- */
void fd2_interpolate_palette_range_toward_color(
    uint32 start_idx, uint32 end_idx, uint32 blend,
    uint32 target_r, uint32 target_g, uint32 target_b)
{
    uint32 idx;
    uint32 rgb_off;
    int pal_val;
    int tgt;

    for (idx = start_idx; (int)idx < (int)end_idx; idx++) {
        outp(0x3C8, idx);
        rgb_off = idx * 3;

        pal_val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                                  + rgb_off);
        tgt = (int)(uint8)target_r;
        outp(0x3C9, (pal_val - tgt) * (int)blend / 0x28 + tgt);

        pal_val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                                  + rgb_off + 1);
        tgt = (int)(uint8)target_g;
        outp(0x3C9, (pal_val - tgt) * (int)blend / 0x28 + tgt);

        pal_val = (int)*(uint8 *)(data_fd2_vga_palette_data_ptr
                                  + rgb_off + 2);
        tgt = (int)(uint8)target_b;
        outp(0x3C9, (pal_val - tgt) * (int)blend / 0x28 + tgt);
    }
}

/* ----------------------------------------------------------------
 * fd2_palette_fade_to_black_step_loop @ 0x25052  (1 caller)
 *
 * Walk intensity from start_intensity down to 0 (inclusive), each
 * step calling fd2_set_vga_palette_range_with_add(0,0xFF,intensity)
 * then sleeping step_delay_ms via CRT delay().
 * ---------------------------------------------------------------- */
void fd2_palette_fade_to_black_step_loop(uint32 start_intensity,
                                          uint32 step_delay_ms)
{
    int intensity;

    for (intensity = (int)start_intensity; intensity >= 0;
         intensity--) {
        fd2_set_vga_palette_range_with_add(0, 0xFF,
                                            (uint32)intensity);
        delay(step_delay_ms);
    }
}
