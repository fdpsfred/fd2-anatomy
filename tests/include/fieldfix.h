#ifndef FIELDFIX_H
#define FIELDFIX_H
/* Shared in-memory fixture for driving the REAL chapter turn-event handlers
 * (src/field/chevt1.c, dispatch table 0x51B91) from a test. Include AFTER the
 * common preamble (needs types/consts/globals + the testglob externs). The test
 * leaf for these handlers is split across chevt11.c (handlers 00..03) and
 * chevt12.c (handlers 04/06/09/0b/0c/0e), so the safe env both halves drive the
 * real callees through lives here.
 *
 * ev_install_safe_env() sets up the proven "ch25-style real portrait reload"
 * environment the handlers' real callees run against:
 *   - a 64-slot runtime-char array (oversized so every callee write is
 *     in-bounds), pointed at by data_fd2_battle_runtime_char_array_ptr;
 *   - an empty active party so the real composite/paint char loops iterate zero;
 *   - the real fd2_composite_battle_frame workspace (logical row 0 at ptr+0x8088),
 *     HUD gated off, the palette cycle throttled to its no-op early return;
 *   - an immediate-END dialog program (pages 0..0x10 each redirect to a single
 *     -1 END opcode) so the real fd2_display_dialog_scene returns at once with
 *     no glyph blits and never reaches the page-break busy-wait;
 *   - an empty BIOS keyboard buffer (head==tail) for fd2_clear_keyboard_buffer;
 *   - zero-group cutscene scripts for events 7/8 (handler_00) so the real
 *     fd2_cutscene_event_trigger composites once and returns;
 *   - a 64-slot menu roster (start empty) for the real recruit;
 *   - the ch25-style real portrait-reload knobs: alloc_offset 0 (empty per-record
 *     tile-event scan -> no fd2_init_runtime_char_for_battle), a fresh field
 *     buffer, and current_chapter_id 4 so the FDFIELD re-read index (4*3+2 = 0xE)
 *     is the same valid index the rsrc loader suite exercises.
 *
 * ev_restore_rc_ptr() puts data_fd2_battle_runtime_char_array_ptr back where the
 * other suites expect it; ev_fd2_tmp_size() reports the FD2.TMP swap-file size
 * (the full real reload rewrites it to 0x32A00 bytes). */

extern runtime_char g_test_rc_array[8];
extern void *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];

/* 64-slot runtime-char fixture (the handlers' callees touch active battle
 * slots; an oversized array keeps every write in-bounds). */
static runtime_char g_ev_rc[64];

/* compositor workspace span the real fd2_blit_rectangle reads:
 * (h-1)*stride + w = 191*0x1C8 + 0x138. */
#define EV_WS_SPAN (191u * 0x1C8u + 0x138u)
static uint8 g_ev_ws_buffer[EV_WS_SPAN];

/* 768-byte VGA palette backing for any real palette read. */
static uint8 g_ev_palette[768];

/* menu roster buffer for the real recruit (64 slots x 0x50 bytes). */
static uint8 g_ev_roster[64 * 0x50];

/* immediate-END dialog text program: a page-offset table (indices 0..0x10)
 * each pointing at a single -1 END opcode at the tail. */
static int16 g_ev_dlg[0x12];

/* zero-group cutscene scripts for events 7 and 8: n_groups byte = 0, so the
 * real fd2_cutscene_event_trigger just composites once and returns. */
static uint8 g_ev_script_07[1] = { 0 };
static uint8 g_ev_script_08[1] = { 0 };

static void ev_install_safe_env(void)
{
    int i;

    /* runtime-char slots the handler's callees touch. */
    memset(g_ev_rc, 0, sizeof(g_ev_rc));
    data_fd2_battle_runtime_char_array_ptr = g_ev_rc;

    /* empty active party: char-overlay/shadow + save loops no-op. */
    data_fd2_battle_party_member_count = 0;

    /* real compositor workspace backing. */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ev_ws_buffer - 0x8088;

    /* anim_phase=0 -> cursor-overlay switch falls through (no blits). */
    data_fd2_battle_anim_phase = 0;

    /* HUD panel gated off. */
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;

    /* palette-cycle to its no-op early-return path (no extra VGA writes). */
    data_fd2_animation_palette_cycle_last_tick =
        (uint16)data_fd2_input_idle_current_bios_tick_word;

    /* 768-byte palette so any real palette read stays in-bounds. */
    memset(g_ev_palette, 0, sizeof(g_ev_palette));
    data_fd2_vga_palette_data_ptr = (uint32)g_ev_palette;

    /* immediate-END dialog program (pages 0..0x10 -> single END opcode). */
    for (i = 0; i <= 0x10; i++) {
        g_ev_dlg[i] = (int16)(0x11 * 2);   /* byte offset of the END opcode */
    }
    g_ev_dlg[0x11] = -1;                    /* END */
    current_chapter_text = (uint32)g_ev_dlg;

    /* empty BIOS keyboard buffer (head==tail) for fd2_clear_keyboard_buffer. */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;

    /* zero-group cutscene scripts for the two events handler_00 fires. */
    g_ev_script_07[0] = 0;
    g_ev_script_08[0] = 0;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[7] = g_ev_script_07;
    data_fd2_chapter_cutscene_event_script_ptr_table_106[8] = g_ev_script_08;

    /* menu roster for the real recruit; start empty. */
    memset(g_ev_roster, 0, sizeof(g_ev_roster));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_ev_roster;
    data_fd2_shared_menu_party_member_count = 0;

    /* ch25-style real portrait reload: empty tile-event scan (alloc_offset 0
     * -> no per-record fd2_init_runtime_char_for_battle), fresh field buffer,
     * and a valid FDFIELD re-read index (chapter 4 -> 4*3+2 = 0xE). */
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    chapter_portrait_load_buffer = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_chapter_current_chapter_id = 4;
}

static void ev_restore_rc_ptr(void)
{
    /* leave the shared pointer where other suites expect it. */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}

static long ev_fd2_tmp_size(void)
{
    FILE *fp;
    long n;

    fp = fopen("FD2.TMP", "rb");
    if (fp == NULL) return -1;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fclose(fp);
    return n;
}

#endif
