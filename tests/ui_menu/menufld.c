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

void run_ui_menu_menufld_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/menufld\n");
    RUN_TEST(test_gate_no_event_bit);
    RUN_TEST(test_gate_already_consumed);
}
