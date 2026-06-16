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
 * Every offset-table slot points at one shared safe sprite (a valid, bounded,
 * paint-nothing 4x1) so the panel's background blit (sheet + *(int*)(sheet+0x5E))
 * and its digit glyphs (sheet + table[base+digit]) all decode cleanly under the
 * REAL blitters instead of spinning. The spy-era g_dlg_blit_* / g_rle_blit_log_*
 * recorders below are no longer filled (the real blitters paint instead of
 * recording); suites that asserted on them are being rewritten to read back real
 * pixels / drive real outputs (Phase 3). */

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

/* sheet header (6 bytes) + 256-entry int32 offset table + a payload sprite tail.
 * Every table slot points at the one payload sprite (see minip_setup_env): the
 * REAL blitters decode it, so it must be a valid, bounded, paint-nothing sprite
 * rather than the old table[i]=i self-reference (whose decoded header had rows=0
 * and spun the real fd2_dialog_sprite_blit_normal / fd2_rle_blit_sprite). */
static int32  t_minip_sheet[2 + 256 + 8];
#define MINIP_SAFE_SPRITE_OFF ((2 + 256) * 4)   /* byte offset of the payload sprite */
/* immediate-END text table: every page entry redirects to an END (-1) marker */
static uint16 t_minip_text[0x200];

#define T_MINIP_END_WORD 0x180

static void minip_setup_env(void)
{
    uint8 *base = (uint8 *)t_minip_sheet;
    int i;

    /* one shared safe sprite: width 4, rows 1. fd2_rle_blit_sprite reads the SKIP
     * command (0xC0|3) -> advances 4 transparent cols, paints nothing;
     * fd2_dialog_sprite_blit_normal reads width 4 / rows 1 -> writes 4 bounded
     * pixels. Both terminate (width>0, rows>0) instead of spinning. */
    base[MINIP_SAFE_SPRITE_OFF + 0] = 4;              /* width lo */
    base[MINIP_SAFE_SPRITE_OFF + 1] = 0;              /* width hi */
    base[MINIP_SAFE_SPRITE_OFF + 2] = 1;              /* rows  lo */
    base[MINIP_SAFE_SPRITE_OFF + 3] = 0;              /* rows  hi */
    base[MINIP_SAFE_SPRITE_OFF + 4] = 0xC0 | (4 - 1); /* SKIP 4 cols (rle path) */
    for (i = 0; i < 256; i++) {
        *(int32 *)(base + 6 + i * 4) = MINIP_SAFE_SPRITE_OFF;
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
