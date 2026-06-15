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
 * Used for the poison-weapon green flash: sole caller
 * fd2_execute_attack_damage_calculation flashes the screen dark
 * green (1,0x20,0) twice when a poison hit lands.
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
    const uint8 *rgb_ptr;
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
 * fd2_palette_overbright_settle_step_loop @ 0x25052  (1 caller)
 *
 * Palette over-bright pulse-down. Walk intensity from start_intensity
 * down to 0 (inclusive), each step calling
 * fd2_set_vga_palette_range_with_add(0,0xFF,intensity) — which writes
 * min(base[i]+intensity,0x3F) to the full DAC range — then sleeping
 * step_delay_ms via CRT delay().
 *
 * This is an ADDITIVE over-bright effect, NOT a fade to black:
 * intensity>=0x3F saturates every channel to full white; intensity=0
 * writes the BASE (normal scene) palette back, so the screen settles
 * to base, never to black. A single call is a white-flash decaying to
 * base. (The chapter-27 blackout is done separately afterward by
 * memset(0xA0000,0,64000) + fd2_play_palette_fade_to_black.)
 * ---------------------------------------------------------------- */
void fd2_palette_overbright_settle_step_loop(uint32 start_intensity,
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

/* ----------------------------------------------------------------
 * fd2_play_palette_fade_in @ 0x1F525  (24 call-sites)
 *
 * VGA palette fade-IN from black to full brightness. Walks the
 * brightness_subtract amount from 0x40 down to 0 (inclusive),
 * each step writing the full DAC range via
 * fd2_set_vga_palette_range(0,0xFF,subtract) — which writes
 * max(0, base[i]-subtract) — then waiting 2 BIOS ticks.
 *
 *   subtract=0x40 → every channel clamped to 0 → screen BLACK
 *   subtract=0    → base palette written unchanged → FULL brightness
 *
 * So the loop proceeds BLACK -> FULL = fade-IN. Pairs with
 * fd2_play_palette_fade_to_black @ 0x1F882 (fade-OUT counterpart).
 * Called from every chapter-intro / cinematic / ending reveal hook.
 * ---------------------------------------------------------------- */
void fd2_play_palette_fade_in(void)
{
    int subtract;

    for (subtract = 0x40; subtract >= 0; subtract--) {
        fd2_set_vga_palette_range(0, 0xFF, (uint32)subtract);
        __delay_thunk_375b2(2);
    }
}

/* ----------------------------------------------------------------
 * fd2_play_palette_fade_to_black @ 0x1F882  (22 call-sites)
 *
 * VGA palette fade-OUT from full brightness to black. Walks the
 * brightness_subtract amount from 0 up to 0x3F (i.e. subtract < 0x40,
 * 0x40 iterations), each step writing the full DAC range via
 * fd2_set_vga_palette_range(0,0xFF,subtract) — which writes
 * max(0, base[i]-subtract) — then waiting 2 BIOS ticks.
 *
 *   subtract=0    → base palette written unchanged → FULL brightness
 *   subtract=0x3F → every channel clamped to 0 → screen BLACK
 *
 * So the loop proceeds FULL -> BLACK = fade-OUT. Pairs with
 * fd2_play_palette_fade_in @ 0x1F525 (fade-IN counterpart). In the
 * binary this entry is a shared-body wrapper: it XORs the counter to 0
 * then JMPs into the darken loop body (0x1F503-0x1F524) physically
 * living inside fd2_load_and_fade_in_cinematic_image @ 0x1F81E. Watcom
 * regenerates an equivalent standalone loop here.
 * Called from every "fade-out to black" chapter / cinematic hook.
 * ---------------------------------------------------------------- */
void fd2_play_palette_fade_to_black(void)
{
    int subtract;

    for (subtract = 0; subtract < 0x40; subtract++) {
        fd2_set_vga_palette_range(0, 0xFF, (uint32)subtract);
        __delay_thunk_375b2(2);
    }
}

/* ----------------------------------------------------------------
 * fd2_fill_palette_blink_pattern_6byte @ 0x33FC1  (0 direct callers)
 *
 * Writes a 6-byte incrementing palette-index sequence into a caller
 * buffer, anchored on input_index:
 *   base = input_index & 0xF8   (floor to a multiple of 8)
 *   bump = (input_index % 8 > 3) ? 2 : 0
 *   for i in 0..5: out[i] = (uint8)(base + bump + i)
 * The +2 bump shifts the anchor when the low 3 bits of input_index are
 * in the high half (4-7), so the two halves of an 8-step cycle map onto
 * palette pair A (offset 0) vs pair B (offset 2) — a double-buffered
 * blink swap (cursor blink / portrait highlight / status flash).
 *
 * input_index is a byte value (0..255). The binary divides by 8 with a
 * signed IDIV; for the documented 0..255 domain this equals the unsigned
 * remainder, so `int % 8` reproduces it exactly. base/bump/i are summed
 * and truncated in an 8-bit register, mirrored here by the uint8 store.
 *
 * Invoked indirectly (palette-effect dispatch via a function-pointer
 * table); no direct xref callers in the binary.
 *
 * Cdecl, 2 stack params; void return. The binary's __CHK(0xC) stack-probe
 * prologue is compiler-generated and omitted here. EBX is callee-saved.
 * ---------------------------------------------------------------- */
void fd2_fill_palette_blink_pattern_6byte(int input_index,
                                          uint32 output_buffer_addr)
{
    int bump;
    int i;

    bump = 0;
    if (input_index % 8 > 3) {
        bump = 2;
    }
    for (i = 0; i < 6; i++) {
        *(uint8 *)(output_buffer_addr + (uint32)i) =
            (uint8)((input_index & 0xF8) + bump + i);
    }
}
