#ifndef SAVEFIX_H
#define SAVEFIX_H
/* savefix.h — drive the REAL fd2_save_slot_selector_ui (src/save/save.c) from
 * a caller's test (the save/load orchestrators in save/save.c and the CONTINUE
 * branch of fd2_main_menu_dispatcher in life/main.c).
 *
 * The real picker's setup phase, before its input loop, does:
 *   - malloc 3 x 64000-byte workspaces (a/b/c) -> NOT freed by the picker;
 *     the caller frees them via fd2_close_intro_dialog_with_slide_out (a no-op
 *     stub in tests), so each picker call LEAKS three 64000-byte buffers under
 *     the harness. savefix_free_selector_workspaces() reclaims the latest set.
 *   - memmove 64000 bytes from 0xA0000 (VGA; mapped+writable in the harness)
 *     into workspace b, then clone b->c.
 *   - fd2_dialog_sprite_blit_normal(c+0x8C05,
 *         atlas + *(uint32*)(atlas+0x46), 0x140) — blit is a recording stub,
 *     but atlas+0x46 is dereferenced first, so a valid atlas buffer is needed.
 *   - the REAL fd2_render_save_slot_grid(cursor, c, sav) — drives the REAL
 *     fd2_display_dialog_scene over data_fd2_all_game_text_ptr, so an all-END
 *     text program is needed (provided here).
 *   - fd2_play_sfx_with_handle (recording stub) + 6x fd2_slide_panel_down_step
 *     (real memmove over the workspaces).
 *
 * Its input loop (manual_mode==0) reads ONE scancode per poll via the real
 * int386(0x16) BIOS INT 16h fn 10h. Navigation keys (Up 0x48 / Down 0x50) keep
 * the loop running (they only move data_fd2_ui_menu_cursor_idx, 0..3 clamped);
 * Enter 0x1C / Space 0x39 return 1 (commit, leaving the cursor as the result);
 * Esc 0x01 returns -1 (cancel). So a single picker call may consume several
 * queued scancodes (the navigation keys) before the terminating commit/cancel.
 *
 * Include AFTER the common preamble (needs types + globals). The atlas/text
 * buffers are file-static here; call savefix_setup_env() in the fixture.
 */

/* sprite atlas: only +0x46 (a u32 sprite offset) is read before the blit stub.
 * 0x80 bytes is ample; the offset value is irrelevant (blit is a no-op stub). */
static uint8  t_savefix_atlas[0x80];
/* immediate-END dialog text program for the real grid renderer. */
static uint16 t_savefix_text[0x400];

#define T_SAVEFIX_END_OFF 0x780

/* Stage only the all-END dialog text program for the real grid renderer.
 * Use this when the caller already provides a real (malloc'd) sprite atlas it
 * frees itself — staging a static atlas here would make that free() corrupt. */
static void savefix_setup_text(void)
{
    int i;

    for (i = 0; i < 0x400; i++) {
        t_savefix_text[i] = 0;
    }
    *(int16 *)((uint8 *)t_savefix_text + T_SAVEFIX_END_OFF) = -1;
    for (i = 0; i < 0x3C0; i++) {
        t_savefix_text[i] = (uint16)T_SAVEFIX_END_OFF;
    }
    data_fd2_all_game_text_ptr = (uint32)t_savefix_text;
}

/* Stage both the sprite atlas (for the panel-header blit arg) and the grid
 * text. Use when the caller has no real atlas of its own (the picker is driven
 * directly, or via the save/load orchestrators that do not load FDOTHER). */
static void savefix_setup_env(void)
{
    int i;

    for (i = 0; i < (int)sizeof(t_savefix_atlas); i++) {
        t_savefix_atlas[i] = 0;
    }
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)t_savefix_atlas;

    savefix_setup_text();
}

/* Queue `n` scancodes (highest-significance byte = INT 16h AH) into the BIOS
 * keyboard buffer ring (BDA seg 0x40). head=0x1E, one word per key starting at
 * abs 0x41E, tail=head + n*2. The real int386(0x16) advances head by 2 each
 * read and the buffer reads non-empty until head==tail. n must be <= 15 (ring
 * holds 16 words; keep tail < 0x3E to avoid the 0x43E->0x41E wrap). */
static void savefix_queue_scancodes(const int *codes, int n)
{
    volatile uint16 *buf = (volatile uint16 *)0x41EuL;
    int i;

    *(volatile uint16 *)0x41AuL = 0x1E;                 /* head */
    for (i = 0; i < n; i++) {
        buf[i] = (uint16)((codes[i] << 8) & 0xFF00);    /* AH = scancode */
    }
    *(volatile uint16 *)0x41CuL = (uint16)(0x1E + n * 2); /* tail */
}

/* Free the three 64000-byte workspaces the latest picker call left allocated
 * (the picker never frees them; the caller's fd2_close_intro_dialog_with_slide_out
 * is a test stub). Idempotent: nulls the globals so a later call is a no-op. */
static void savefix_free_selector_workspaces(void)
{
    if (data_fd2_ui_slide_anim_accumulator_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_anim_accumulator_buf_ptr);
        data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_bg_snapshot_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr);
        data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    }
    if (data_fd2_ui_slide_composed_target_buf_ptr != 0) {
        free((void *)data_fd2_ui_slide_composed_target_buf_ptr);
        data_fd2_ui_slide_composed_target_buf_ptr = 0;
    }
}

#endif
