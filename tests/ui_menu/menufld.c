/*
 * unit tests for src/ui_menu/menufld.c
 *
 * fd2_handle_tile_event_interaction is a field-map treasure/gold/event-tile
 * interaction handler dominated by the real dialog VM, portrait paint, and the
 * blocking input wait fd2_wait_for_input_dialog_with_blink (real, input.c).
 *
 * Host-testable here (no blocking input reached):
 *   - the two early gates: attribute event-bit clear -> return; already-consumed
 *     tile -> return (both before any dialog / wait / state change).
 *
 * Deferred to Phase 9 integration: every non-gate path runs the real opening
 * Yes/No prompt fd2_text_dialog_typewriter_loop (emitted in src/dialog/dialog.c),
 * a busy-wait that blocks on INT 16h after fd2_handle_tile_event_interaction has
 * just cleared the BIOS keyboard buffer — the unit harness has no real keyboard
 * to release it, so the loop cannot return in-process. This is the same blocking
 * input the GOLD/ITEM paths already deferred via fd2_wait_for_input_dialog_with_
 * blink. The branch logic past the prompt (NO cancel, the YES gate cursor==0
 * compound, the EVENT post-action consequence dispatch, the gold/item paths) is
 * therefore exercised under the emulator where real input drives the prompt.
 *
 * Real-callee safety: the opening dialog runs the real fd2_display_dialog_scene
 * against a page table whose every used text id maps to an immediate-END program
 * (returns at once, no portrait/wait side effects). The tile read runs the real
 * fd2_read_tile_attribute_at_pos over an in-memory tile map + attribute buffer.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "menufix.h"

/* SFX seam (testglob.c); asserted zero on the early-return gate paths. */
extern int g_play_sfx_sample_from_bank_calls;

/* portrait buffer the real fd2_paint_portrait_to_dialog_area dereferences. */
extern uint8 *data_fd2_portrait_sprite_buffer;

/* ---- in-memory environment ---- */

/* Dialog text resource: page table (index 0..0x1af) where every entry points to
 * a single shared END (-1) opcode placed just past the table. */
#define MFLD_TEXT_WORDS 440
static int16 mfld_text[MFLD_TEXT_WORDS];

/* tile-map meta (cursor at 0,0 -> meta at +0); attribute flags buffer
 * (attr[sprite_idx*4]); 32 consumed flags; tile_event_data_table is addressed
 * as base + idx*3 with type at +0x53 and value uint16 at +0x54, so the backing
 * buffer must cover 0x53 + 32*3 + slack. */
static uint8 mfld_tile_map[64];
static uint8 mfld_tile_attr[64];
static uint8 mfld_consumed[32];
static uint8 mfld_event_data[0x53 + 32 * 3 + 8];

/* a 16-byte portrait buffer: [0..3] frame-0 offset = 0 so the real paint reads
 * sprite_addr = buf + 0 (then blits to the recording stub). */
static uint8 mfld_portrait[16];

/*
 * Build a host-safe environment and seed the cursor tile so the real
 * fd2_read_tile_attribute_at_pos yields terrain_class == tile_idx and attribute
 * byte == attr. Installs an event entry (type, value) at tile_idx, and resets
 * the consumed flag for tile_idx to `consumed`.
 */
static void mfld_setup(uint8 tile_idx, uint8 attr, uint8 ev_type,
                       uint16 ev_value, uint8 consumed)
{
    int i;

    /* dialog text resource: all pages -> shared END opcode at word index 432. */
    for (i = 0; i < MFLD_TEXT_WORDS; i++) {
        mfld_text[i] = 0;
    }
    mfld_text[432] = -1;                 /* END opcode body */
    for (i = 0; i <= 0x1af; i++) {
        mfld_text[i] = (int16)(432 * 2); /* byte offset to the END opcode */
    }
    data_fd2_all_game_text_ptr = (uint32)mfld_text;

    /* tile map: sprite_idx=1 at meta+4, terrain=tile_idx at meta+6. */
    for (i = 0; i < 64; i++) {
        mfld_tile_map[i] = 0;
        mfld_tile_attr[i] = 0;
    }
    *(uint16 *)(mfld_tile_map + 4) = 1;          /* sprite_idx */
    mfld_tile_map[6] = (uint8)(tile_idx & 0x1f); /* terrain class (5-bit) */
    mfld_tile_attr[1 * 4 + 0] = attr;            /* attr_ptr[0] -> out_buf[+4] */
    data_fd2_battle_tile_map_ptr = (uint32)mfld_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)mfld_tile_attr;
    data_fd2_battle_map_width_tiles = 20;
    data_fd2_battle_cursor_world_x = 0;
    data_fd2_battle_cursor_world_y = 0;

    /* consumed flags. */
    for (i = 0; i < 32; i++) {
        mfld_consumed[i] = 0;
    }
    mfld_consumed[tile_idx] = consumed;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)mfld_consumed;

    /* tile event data table: entry at base + idx*3, type @ +0x53, value @ +0x54. */
    for (i = 0; i < (int)sizeof(mfld_event_data); i++) {
        mfld_event_data[i] = 0;
    }
    mfld_event_data[tile_idx * 3 + 0x53] = ev_type;
    *(uint16 *)(mfld_event_data + tile_idx * 3 + 0x54) = ev_value;
    data_fd2_tile_event_data_table_ptr = (uint32)mfld_event_data;

    /* runtime char array (portrait id read from [char_idx].portrait_id and
     * passed to the noop fd2_load_chapter_portrait; value is not asserted). */
    for (i = 0; i < 8; i++) {
        data_fd2_battle_runtime_char_array_ptr[i].portrait_id = 3;
    }

    /* portrait buffer for the real paint. */
    for (i = 0; i < 16; i++) {
        mfld_portrait[i] = 0;
    }
    data_fd2_portrait_sprite_buffer = mfld_portrait;
    data_fd2_dialog_active_portrait_blit_offset = 0x728; /* normal-blit slot */

    /* dialog state. */
    data_fd2_ui_menu_cursor_idx = 0;
    data_fd2_audio_fdother_sfx_bank_buf_ptr = 0;

    /* reset observable counters (the early-return gate paths must leave the SFX
     * seam untouched). */
    g_play_sfx_sample_from_bank_calls = 0;
}

/*
 * Gate 1: attribute byte with neither 0x20 nor 0x40 set -> (attr & 0x60)==0, the
 * handler returns immediately after the tile read. No dialog, no typewriter, no
 * gold/consumed mutation.
 */
static void test_gate_no_event_bit(void)
{
    mfld_setup(4, 0x00, 1 /*gold*/, 100, 0);
    data_fd2_shared_party_total_gold = 777;

    fd2_handle_tile_event_interaction(0);

    ASSERT_EQ(g_play_sfx_sample_from_bank_calls, 0);         /* never reached prompt */
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, 777);  /* untouched */
    ASSERT_EQ((int)mfld_consumed[4], 0);                     /* untouched */
}

/*
 * Gate 2: event bit set (0x20) but the tile is already consumed -> returns after
 * the tile read + consumed check, before any dialog.
 */
static void test_gate_already_consumed(void)
{
    mfld_setup(6, 0x20, 1 /*gold*/, 50, 1 /*consumed*/);
    data_fd2_shared_party_total_gold = 555;

    fd2_handle_tile_event_interaction(0);

    ASSERT_EQ(g_play_sfx_sample_from_bank_calls, 0);         /* never reached prompt */
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, 555);
    ASSERT_EQ((int)mfld_consumed[6], 1);                     /* still 1 */
}

/*
 * The NO-cancel branch, the YES gate (typewriter==1 && cursor==0) compound, the
 * EVENT post-action consequence dispatch (indexed by the event VALUE word, an
 * EAX/arg-tracking-sensitive indirect call) and the GOLD/ITEM paths all sit past
 * the real opening fd2_text_dialog_typewriter_loop prompt. The handler clears the
 * BIOS keyboard buffer immediately before that prompt, so in the host the prompt
 * busy-waits forever with no key to release it. These branches are therefore
 * deferred to Phase 9 integration (real input drives the prompt there); see the
 * file header. The prompt's own logic is unit-tested directly against a preloaded
 * keyboard buffer in tests/dialog/dialog.c (test_typewriter_*).
 */

/* ================================================================
 * fd2_field_menu_status_save_load_quit_dispatch @ 0x19DF7 coverage
 *
 * The setup phase is host-safe and deterministic and is driven end to end here:
 *   - the two 4-int template copies (options {12,13,14,15} / state {0,0,0,0}),
 *   - the real fopen("FD2.SAV","rb") probe (FD2.SAV is staged into the test cwd
 *     by build_test.py, so the file IS present -> the Load option is NOT grayed,
 *     menu_state[2] stays 0; the probe malloc/fread/free path runs),
 *   - the party scan that grays the Save option (menu_state[1] = 1) when a unit
 *     has acted (flags & 0x80) but is still alive (flags & 1 == 0),
 *   - the real fd2_open_settings_dialog_with_slide, the real input-step loop
 *     fed a single Esc through the BIOS keyboard ring (so it returns -1 at once),
 *     the real close, and the input_result == -1 -> return 0 early-out.
 *
 * The per-corner sprite index the open-dialog computes is
 *   sprite_id[c] = menu_options[c] * 3 + menu_state[c] * 2,
 * and the recording blit stub (g_blitsetup_sprite_log) captures
 *   sprite_addr = handle + handle[sprite_id].
 * With the dialog-state handle's offset table set to the identity (handle[i]=i),
 * sprite_addr - handle == sprite_id, so each corner's gating decision is read
 * back directly from the recorded blits. Frame 0's four corner blits are the
 * first four recorded.
 *
 * The Status (cursor 0), Save (1), Load (2) and Quit (3) dispatch arms all run
 * past the real fd2_text_dialog_typewriter_loop YES/NO prompt (a BIOS-keyboard
 * busy-wait) or the heavy save-file / engine-reload UI; their behavioral
 * coverage is deferred to Phase 9 integration under the emulator. (The actual
 * FD2.SAV snapshot assembly is never written by these unit tests — only the
 * read-only Esc-cancel path runs — so the staged real save file is preserved.)
 * ================================================================ */

/* corner-blit recorder (testglob.c). */
extern uint32 g_blitsetup_sprite_log[32];
extern int    g_blitsetup_calls;

/* idle-loop buffer-flip seam (testglob.c): disarmed here so the staged Esc is
 * read on the first input-step without the buffer being flipped underneath us. */
extern int g_repaint_flip_buffer_after;
extern int g_repaint_settings_calls;

/* Host-safe render env shared by the dispatch's real open/close dialog. */
#define SLQ_WS_SPAN (191u * 0x1C8u + 0x138u + 0x8088u)
static uint8  slq_ws_buffer[SLQ_WS_SPAN];
static int32  slq_dialog_handle[64];      /* identity offset table; max id 47 */
static runtime_char slq_chars[4];

/* Saved shared-global snapshot so each test restores cross-suite state on exit
 * (the runtime-char-array pointer in particular defaults to g_test_rc_array,
 * which the following ui_menu/status suite relies on). */
static runtime_char *slq_saved_char_ptr;
static uint32 slq_saved_party_count;

/* Build the host-safe environment: identity dialog-offset table (so a recorded
 * corner blit's sprite address minus the handle equals the sprite id), an empty
 * party by default, the workspace + cursor cell the open-dialog reads, and a
 * single Esc staged so the settings input loop cancels immediately. Snapshots
 * the shared globals it overwrites so slq_teardown() can restore them. */
static void slq_setup(void)
{
    int i;

    slq_saved_char_ptr = data_fd2_battle_runtime_char_array_ptr;
    slq_saved_party_count = data_fd2_battle_party_member_count;

    for (i = 0; i < 64; i++) {
        slq_dialog_handle[i] = i;          /* identity: handle[id] = id */
    }
    for (i = 0; i < (int)sizeof(slq_chars); i++) {
        ((uint8 *)slq_chars)[i] = 0;
    }

    data_fd2_battle_runtime_char_array_ptr = slq_chars;
    data_fd2_battle_party_member_count = 0;
    data_fd2_battle_cursor_screen_x = 0;
    data_fd2_battle_cursor_screen_y = 0;
    data_fd2_battle_cursor_world_x = 0;
    data_fd2_battle_cursor_world_y = 0;
    data_fd2_battle_view_window_origin_x = 0;
    data_fd2_battle_view_window_origin_y = 0;
    data_fd2_large_game_state_buffer_ptr = (uint32)slq_ws_buffer;
    data_fd2_menu_dialog_state_handle = (uint32)slq_dialog_handle;
    data_fd2_audio_fdother_sfx_bank_buf_ptr = 0;
    data_fd2_ui_menu_cursor_idx = 0;

    g_blitsetup_calls = 0;
    g_repaint_flip_buffer_after = 0;       /* do not flip the ring underneath us */
    g_repaint_settings_calls = 0;
    mfix_load_cancel();                    /* single Esc -> input-step returns -1 */
}

/* Restore the shared globals slq_setup() captured, so the following suite sees
 * the pre-test environment (no cross-suite pollution of the char-array ptr). */
static void slq_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = slq_saved_char_ptr;
    data_fd2_battle_party_member_count = slq_saved_party_count;
}

/* Cancel with an empty party + FD2.SAV present: the dispatch copies both
 * templates, probes the (existing) save file so Load stays enabled, opens the
 * settings dialog, reads Esc, closes, and returns 0. Frame-0 corner sprite ids
 * read back as the ungated defaults {36,39,42,45} (= options{12,13,14,15}*3),
 * proving the FD2.SAV-present probe left menu_state[2] at 0 and the empty-party
 * scan left menu_state[1] at 0. */
static void test_save_load_quit_cancel_no_gating(void)
{
    int r;

    slq_setup();
    r = fd2_field_menu_status_save_load_quit_dispatch();

    ASSERT_EQ(r, 0);
    /* one real open (16 corner blits) + one real close (16) = 32. */
    ASSERT_EQ(g_blitsetup_calls, 32);
    /* frame-0 corners 0..3 = first four recorded blits; identity handle. */
    ASSERT_EQ((int)(g_blitsetup_sprite_log[0] - data_fd2_menu_dialog_state_handle), 36); /* 12*3 */
    ASSERT_EQ((int)(g_blitsetup_sprite_log[1] - data_fd2_menu_dialog_state_handle), 39); /* 13*3 */
    ASSERT_EQ((int)(g_blitsetup_sprite_log[2] - data_fd2_menu_dialog_state_handle), 42); /* 14*3 */
    ASSERT_EQ((int)(g_blitsetup_sprite_log[3] - data_fd2_menu_dialog_state_handle), 45); /* 15*3 */
    slq_teardown();
}

/* Save-gating scan: one party member that has acted (flags & 0x80) yet is alive
 * (flags & 1 == 0) makes the scan set menu_state[1] = 1, so corner 1's sprite id
 * becomes 13*3 + 1*2 = 41 (vs the ungated 39). The member is placed away from the
 * cursor so fd2_find_char_at_cursor_pos returns -1 (no char restamp). A single
 * Esc cancels; the dispatch returns 0. Corners 0/2/3 stay ungated. */
static void test_save_load_quit_save_gated_scan(void)
{
    int r;

    slq_setup();
    data_fd2_battle_party_member_count = 1;
    slq_chars[0].pos_x = 5;                 /* not at cursor (0,0) */
    slq_chars[0].pos_y = 5;
    slq_chars[0].flags = 0x80;              /* acted, alive -> Save grayed */

    r = fd2_field_menu_status_save_load_quit_dispatch();

    ASSERT_EQ(r, 0);
    ASSERT_EQ(g_blitsetup_calls, 32);
    ASSERT_EQ((int)(g_blitsetup_sprite_log[0] - data_fd2_menu_dialog_state_handle), 36); /* Status ungated */
    ASSERT_EQ((int)(g_blitsetup_sprite_log[1] - data_fd2_menu_dialog_state_handle), 41); /* Save grayed: 13*3+2 */
    ASSERT_EQ((int)(g_blitsetup_sprite_log[2] - data_fd2_menu_dialog_state_handle), 42); /* Load ungated */
    ASSERT_EQ((int)(g_blitsetup_sprite_log[3] - data_fd2_menu_dialog_state_handle), 45); /* Quit ungated */

    slq_teardown();
}

/* A dead member (flags & 1 set) or a not-yet-acted member (flags & 0x80 clear)
 * must NOT gray the Save option: the scan's compound condition requires acted AND
 * alive. Two members, neither qualifying, leave menu_state[1] at 0 -> corner 1
 * stays the ungated 39. */
static void test_save_load_quit_save_not_gated_when_dead_or_unacted(void)
{
    int r;

    slq_setup();
    data_fd2_battle_party_member_count = 2;
    slq_chars[0].pos_x = 5; slq_chars[0].pos_y = 5;
    slq_chars[0].flags = 0x81;              /* acted but DEAD (bit0 set) -> not gated */
    slq_chars[1].pos_x = 6; slq_chars[1].pos_y = 6;
    slq_chars[1].flags = 0x00;              /* alive but has NOT acted -> not gated */

    r = fd2_field_menu_status_save_load_quit_dispatch();

    ASSERT_EQ(r, 0);
    ASSERT_EQ((int)(g_blitsetup_sprite_log[1] - data_fd2_menu_dialog_state_handle), 39); /* Save ungated */

    slq_teardown();
}

/* ================================================================
 * fd2_open_tactical_overview_zoom @ 0x2000A — DEFERRED to Phase 9
 *
 * The whole callable surface sits behind a clear-then-poll-until-keypress
 * display loop with no harness-releasable exit, so the function cannot be
 * driven to completion in-process:
 *   fd2_clear_keyboard_buffer();                  // TAIL := HEAD (ring empty)
 *   while (fd2_check_keyboard_buffer_nonempty() == 0) { ...fill squares... }
 * Because the buffer is force-cleared immediately before the loop, a key
 * pre-staged into the BIOS ring (the seam the sibling
 * fd2_wait_input_with_recruitment_repaint tests use — those have no leading
 * clear) is wiped before the first check, so the loop is entered and spins
 * forever waiting on a keypress the text-mode harness cannot deliver. The
 * loop body's only callees are the REAL fd2_check_keyboard_buffer_nonempty
 * and fd2_fill_screen_rect_with_byte (gfx/blitspr.c) — neither is a stub, so
 * the sanctioned in-loop "flip the ring nonempty" seam
 * (g_repaint_flip_buffer_after, hosted in the fd2_composite_battle_tile_map
 * stub) is not reachable from this loop, and adding a seam to a real emitted
 * routine is disallowed. The intro 7-frame zoom runs before the loop but the
 * function never returns, so even it cannot be observed via a normal call.
 *
 * Static three-source verification stands in: the fixed-point camera
 * interpolation (delta*ratio/8 + base) was confirmed equal to the binary's
 * SHL/SBB/SAR round-toward-zero idiom, and because every fixed-point input is
 * pre-scaled by 0x600 (a multiple of 8) the division is always exact (the
 * rounding direction is never exercised by valid inputs); the scale series
 * (scroll_origin*frame/7 + 0x80), the zoom_level/scroll_origin height branch
 * (map_height <= 0x28), the tile_data_table fill (table[iy*0x40+ix] =
 * snapshot + cache_idx*0x240 + 6), and the anim_phase 0..7 bounce were all
 * checked line-by-line against the disassembly. Behavioral coverage (the
 * map/square blits, the bounce, the framebuffer memmove to 0xA0000) is
 * deferred to Phase 9 integration, where a real keypress releases the loop —
 * the same blocking-input deferral the field-tile handler above uses. The
 * (not-yet-emitted) fd2_blit_scaled_tile_map_view callee is satisfied for the
 * link by the recording stub in testglob.c.
 * ================================================================ */

void run_ui_menu_menufld_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/menufld\n");
    RUN_TEST(test_gate_no_event_bit);
    RUN_TEST(test_gate_already_consumed);
    RUN_TEST(test_save_load_quit_cancel_no_gating);
    RUN_TEST(test_save_load_quit_save_gated_scan);
    RUN_TEST(test_save_load_quit_save_not_gated_when_dead_or_unacted);
}
