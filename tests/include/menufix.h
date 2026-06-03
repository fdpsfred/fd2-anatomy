/*
 * menufix.h — in-memory fixture helpers for the settings/options menu tests.
 *
 * The real fd2_settings_menu_input_step drives one keystroke through the real
 * fd2_wait_input_with_dialog_repaint, which reads the scancode from the BIOS
 * keyboard buffer (INT 16h AH=10h, HIGH byte = scancode). These helpers stage
 * one or more scancodes into the BIOS keyboard ring so a menu loop can be
 * driven deterministically:
 *   - mfix_load_keys() writes a sequence of scancodes into the 16-word ring at
 *     0x41E with head=0x41A and tail set just past the last key, so each
 *     fd2_wait_input_with_dialog_repaint call consumes the next scancode (and
 *     fd2_check_keyboard_buffer_nonempty sees head!=tail, skipping the idle
 *     loop). The scancode goes in the HIGH byte of each ring word.
 *
 * Direction scancodes (settings menu cross-shape):
 *   Up=0x48 -> cursor 0, Left=0x4B -> cursor 1, Right=0x4D -> cursor 2,
 *   Down=0x50 -> cursor 3. Space=0x39 / Enter=0x1C commit, Esc=0x01 cancels.
 */
#ifndef MENUFIX_H
#define MENUFIX_H

#include "types.h"

#define MFIX_SC_ESC    0x01
#define MFIX_SC_ENTER  0x1C
#define MFIX_SC_SPACE  0x39
#define MFIX_SC_UP     0x48
#define MFIX_SC_LEFT   0x4B
#define MFIX_SC_RIGHT  0x4D
#define MFIX_SC_DOWN   0x50

/* Map a settings cursor slot (0..3) to its arrow scancode. */
static uint8 mfix_dir_scancode(int cursor)
{
    switch (cursor) {
    case 0:  return MFIX_SC_UP;
    case 1:  return MFIX_SC_LEFT;
    case 2:  return MFIX_SC_RIGHT;
    default: return MFIX_SC_DOWN; /* 3 */
    }
}

/* Stage `count` scancodes into the BIOS keyboard ring starting at head 0x41E.
 * Each entry occupies one 16-bit ring word with the scancode in the high byte
 * (ASCII low byte 0). head := 0x41E (start), tail := 0x41E + count*2. */
static void mfix_load_keys(const uint8 *scancodes, int count)
{
    int i;
    *(volatile uint16 *)0x41AuL = 0x1E;                 /* head */
    for (i = 0; i < count; i++) {
        *(volatile uint16 *)(0x41EuL + (uint32)i * 2u) =
            (uint16)((uint16)scancodes[i] << 8);
    }
    *(volatile uint16 *)0x41CuL = (uint16)(0x1E + count * 2); /* tail */
}

/* Drive one navigation-to-`cursor` then commit (Space): the loop's input-step
 * returns 0 on the arrow (cursor moves), then 1 on Space (selection). */
static void mfix_load_select(int cursor)
{
    uint8 keys[2];
    keys[0] = mfix_dir_scancode(cursor);
    keys[1] = MFIX_SC_SPACE;
    mfix_load_keys(keys, 2);
}

/* For an infinite menu loop (fd2_game_options_menu_loop): navigate to `cursor`,
 * commit (Space) so the dispatch toggles that slot, then Esc so the loop's next
 * iteration cancels and the loop exits. Exactly one toggle iteration runs.
 *
 * Layout models real play across two iterations. Iteration 1 reads arrow then
 * Space (buffer nonempty, no idle); its close-dialog clears the buffer
 * (tail:=head, now at the ring slot holding Esc). Iteration 2 finds the buffer
 * empty and idles; the caller must set g_repaint_flip_buffer_after = 1 so the
 * repaint stub flips the buffer nonempty (tail:=head+2) on the first idle pass,
 * exposing the pre-staged Esc at the current head — exactly as a key arriving
 * during idle would. INT 16h then reads Esc -> input-step returns -1 -> exit.
 *
 * Ring: [arrow, Space, Esc] at 0x41E/0x420/0x422; head=0x41E, tail=0x422
 * (only arrow+Space initially "present"; Esc waits at the post-clear head). */
static void mfix_load_select_then_cancel(int cursor)
{
    *(volatile uint16 *)0x41AuL = 0x1E;                         /* head */
    *(volatile uint16 *)0x41EuL = (uint16)((uint16)mfix_dir_scancode(cursor) << 8);
    *(volatile uint16 *)0x420uL = (uint16)((uint16)MFIX_SC_SPACE << 8);
    *(volatile uint16 *)0x422uL = (uint16)((uint16)MFIX_SC_ESC << 8);
    *(volatile uint16 *)0x41CuL = 0x22;                         /* tail: arrow+Space */
}

/* Drive a single Esc (cancel): input-step returns -1 immediately. */
static void mfix_load_cancel(void)
{
    uint8 keys[1];
    keys[0] = MFIX_SC_ESC;
    mfix_load_keys(keys, 1);
}

#endif /* MENUFIX_H */
