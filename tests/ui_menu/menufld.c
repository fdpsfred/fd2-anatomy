/*
 * unit tests for src/ui_menu/menufld.c
 *
 * fd2_handle_tile_event_interaction is a field-map treasure/gold/event-tile
 * interaction handler dominated by the real dialog VM, portrait paint, and the
 * blocking input wait fd2_wait_for_input_dialog_with_blink (real, input.c).
 *
 * Host-testable here (no blocking input reached):
 *   - the two early gates: attribute event-bit clear -> return; already-consumed
 *     tile -> return (both before any dialog / wait / state change);
 *   - the NO branch (typewriter result != YES): cancel dialog + close, no gold
 *     or consumed-flag mutation;
 *   - the EVENT branch (event type != 0,1): SFX + post-action consequence table
 *     dispatch, indexed by the event VALUE word and called with char_idx (an
 *     EAX/arg-tracking-sensitive indirect call), with no blocking wait.
 *
 * Deferred to Phase 9 integration (the GOLD and ITEM paths both reach the real
 * blocking fd2_wait_for_input_dialog_with_blink before any observable state
 * change, and the unit harness has no real keyboard to release it): the gold
 * accumulation, the item add / inventory-full swap UI, and the consumed-flag
 * set that follow the wait. Those are exercised under the emulator where the
 * real input path can drive them.
 *
 * Real-callee safety: the opening/branch dialogs run the real
 * fd2_display_dialog_scene against a page table whose every used text id maps to
 * an immediate-END program (returns at once, no portrait/wait side effects); the
 * real fd2_paint_portrait_to_dialog_area reads a small valid portrait buffer and
 * blits to the recording stub. The tile read runs the real
 * fd2_read_tile_attribute_at_pos over an in-memory tile map + attribute buffer.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* typewriter / SFX / inventory seams (testglob.c). */
extern int g_typewriter_loop_return;
extern int g_typewriter_loop_calls;
extern int g_anim_dialog_page_advance_calls;
extern int g_play_sfx_sample_from_bank_calls;
extern int g_add_item_calls;
extern int g_inventory_modal_calls;

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

/* recording post-action consequence handler. */
static int    mfld_post_calls;
static uint32 mfld_post_last_arg;
static void mfld_post_handler(uint32 char_idx)
{
    mfld_post_calls++;
    mfld_post_last_arg = char_idx;
}

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

    /* reset observable counters. */
    g_typewriter_loop_calls = 0;
    g_anim_dialog_page_advance_calls = 0;
    g_play_sfx_sample_from_bank_calls = 0;
    g_add_item_calls = 0;
    g_inventory_modal_calls = 0;
    mfld_post_calls = 0;
    mfld_post_last_arg = 0;
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

    ASSERT_EQ(g_typewriter_loop_calls, 0);
    ASSERT_EQ(g_play_sfx_sample_from_bank_calls, 0);
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

    ASSERT_EQ(g_typewriter_loop_calls, 0);
    ASSERT_EQ(g_play_sfx_sample_from_bank_calls, 0);
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, 555);
    ASSERT_EQ((int)mfld_consumed[6], 1);                     /* still 1 */
}

/*
 * NO branch: event bit set, not consumed, but the opening typewriter prompt
 * returns "No" (0). The handler runs the cancel dialog + close and returns with
 * no SFX, no gold add, no consumed-flag set. The opening typewriter +
 * page-advance still ran exactly once.
 */
static void test_no_branch_cancel(void)
{
    mfld_setup(7, 0x60, 1 /*gold*/, 200, 0);
    data_fd2_shared_party_total_gold = 1000;
    g_typewriter_loop_return = 0;        /* "No" */

    fd2_handle_tile_event_interaction(0);

    ASSERT_EQ(g_typewriter_loop_calls, 1);
    ASSERT_EQ(g_anim_dialog_page_advance_calls, 1);
    ASSERT_EQ(g_play_sfx_sample_from_bank_calls, 0);   /* SFX only in YES */
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, 1000);  /* no add */
    ASSERT_EQ((int)mfld_consumed[7], 0);               /* not consumed */
}

/*
 * YES gate is the compound (typewriter==1 && cursor==0): with the typewriter
 * returning YES but the menu cursor forced non-zero, the handler must route to
 * the NO/cancel branch — no SFX, no event dispatch.
 */
static void test_yes_gate_requires_cursor_zero(void)
{
    mfld_setup(8, 0x20, 2 /*event*/, 5, 0);
    g_typewriter_loop_return = 1;        /* "Yes" ... */
    data_fd2_battle_ai_post_action_consequence_table[5] = mfld_post_handler;
    data_fd2_ui_menu_cursor_idx = 2;     /* ... but cursor != 0 -> NO branch */

    fd2_handle_tile_event_interaction(0);

    ASSERT_EQ(g_play_sfx_sample_from_bank_calls, 0); /* YES path not entered */
    ASSERT_EQ(mfld_post_calls, 0);                   /* no event dispatch */

    data_fd2_battle_ai_post_action_consequence_table[5] = (void *)0;
}

/*
 * EVENT branch: YES (typewriter==1, cursor==0), event type 2 (neither ITEM(0)
 * nor GOLD(1)). The handler plays the pickup SFX, then dispatches the
 * post-action consequence table indexed by the event VALUE word, passing
 * char_idx. No blocking wait, no gold/consumed mutation on this path.
 */
static void test_event_post_action_dispatch(void)
{
    mfld_setup(9, 0x20, 2 /*event*/, 5 /*table index*/, 0);
    g_typewriter_loop_return = 1;        /* YES */
    data_fd2_battle_ai_post_action_consequence_table[5] = mfld_post_handler;
    data_fd2_shared_party_total_gold = 1234;

    fd2_handle_tile_event_interaction(3 /*char_idx*/);

    ASSERT_EQ(g_play_sfx_sample_from_bank_calls, 1);       /* YES SFX played */
    ASSERT_EQ(mfld_post_calls, 1);                         /* table[value] called */
    ASSERT_EQ((long)mfld_post_last_arg, 3);               /* with char_idx */
    ASSERT_EQ(g_add_item_calls, 0);                        /* not the ITEM path */
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, 1234); /* not the GOLD path */
    ASSERT_EQ((int)mfld_consumed[9], 0);                   /* EVENT leaves it */

    data_fd2_battle_ai_post_action_consequence_table[5] = (void *)0;
}

/*
 * EVENT dispatch index is the VALUE word, not the event type: a different value
 * selects a different table slot. Confirms the indirect-call index source.
 */
static void test_event_dispatch_index_is_value(void)
{
    mfld_setup(10, 0x60, 7 /*event type, arbitrary >1*/, 11 /*value=index*/, 0);
    g_typewriter_loop_return = 1;
    data_fd2_battle_ai_post_action_consequence_table[11] = mfld_post_handler;

    fd2_handle_tile_event_interaction(2);

    ASSERT_EQ(mfld_post_calls, 1);
    ASSERT_EQ((long)mfld_post_last_arg, 2);

    data_fd2_battle_ai_post_action_consequence_table[11] = (void *)0;
}

void run_ui_menu_menufld_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/menufld\n");
    RUN_TEST(test_gate_no_event_bit);
    RUN_TEST(test_gate_already_consumed);
    RUN_TEST(test_no_branch_cancel);
    RUN_TEST(test_yes_gate_requires_cursor_zero);
    RUN_TEST(test_event_post_action_dispatch);
    RUN_TEST(test_event_dispatch_index_is_value);
}
