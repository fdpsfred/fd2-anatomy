#ifndef MINIPFIX_H
#define MINIPFIX_H
/* Shared fixture for driving the REAL fd2_render_mini_char_status_panel
 * (src/gfx/rndstat.c) from a caller's test. Include AFTER the common preamble
 * (needs types + the render-spy externs from testglob.c). Provides a sprite
 * sheet and an immediate-END text table so the real painter's background blit,
 * HP/MP bars, decimal numbers and name-label dialog scene all run without
 * touching VGA / fopen.
 *
 * Observation points (all in testglob.c):
 *   - g_dlg_blit_last_dst   : the background blit dst == the buf passed to the
 *                             panel (so a caller's computed screen offset is
 *                             recoverable as g_dlg_blit_last_dst).
 *   - g_rle_blit_log_*      : digit glyphs; the panel renders the sleep
 *                             indicator (status_flags_block[0]) first, so the
 *                             selected char's slot is recoverable from it.
 *
 * The sheet uses table[i] = i at byte offset 6, so the panel's background
 * sprite (sheet + *(int*)(sheet+0x5E)) resolves to sheet + table[22] =
 * sheet + 0x16 (0x5E = 6 + 22*4). */

extern int    g_blitraw_log_on;
extern int    g_blitraw_count;
extern int    g_rle_blit_calls;
extern int    g_rle_blit_log_on;
extern uint32 g_rle_blit_log_sprite[64];
extern uint32 g_rle_blit_log_dst[64];
extern int    g_dlg_blit_normal_calls;
extern int    g_dlg_blit_mirrored_calls;
extern uint32 g_dlg_blit_last_dst;
extern uint32 g_dlg_blit_last_sprite;
extern uint32 g_dlg_blit_last_stride;

/* sheet header (6 bytes) + 256-entry int32 offset table (table[i] = i) */
static int32  t_minip_sheet[2 + 256];
/* immediate-END text table: every page entry redirects to an END (-1) marker */
static uint16 t_minip_text[0x200];

#define T_MINIP_END_WORD 0x180

static void minip_setup_env(void)
{
    uint8 *base = (uint8 *)t_minip_sheet;
    int i;

    for (i = 0; i < 256; i++) {
        *(int32 *)(base + 6 + i * 4) = i;
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)base;

    for (i = 0; i < 0x200; i++) {
        t_minip_text[i] = 0;
    }
    t_minip_text[T_MINIP_END_WORD] = (uint16)-1;
    for (i = 0; i < T_MINIP_END_WORD; i++) {
        t_minip_text[i] = (uint16)(T_MINIP_END_WORD * 2);
    }
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)t_minip_text;

    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;
    g_dlg_blit_normal_calls = 0;
    g_dlg_blit_mirrored_calls = 0;
    g_dlg_blit_last_dst = 0;
    g_dlg_blit_last_sprite = 0;
    g_dlg_blit_last_stride = 0;
}

#endif
