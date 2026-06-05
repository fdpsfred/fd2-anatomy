/*
 * unit tests for src/ui_menu/shop.c — part 2 (fd2_run_give_item_menu).
 *
 * Split from shop1.c (the shop input-loop / panel-open / buy / sell / equip
 * suites) to stay under the 1000-line-per-leaf cap.
 *
 * fd2_run_give_item_menu (@0x2F8EA) is the GIVE/TRADE-item top-level loop: pick a
 * SOURCE member, list their inventory in the shop panel, pick the item, pick a
 * TARGET member, then move the item from source to target (source stats
 * recomputed). Unlike the buy/sell siblings, EVERY iteration opens the
 * chapter-speaker portrait + a prompt dialog at the loop top BEFORE the first
 * roster select, so these tests drive the REAL portrait+dialog VM end-to-end:
 *
 *   - fd2_load_chapter_portrait (rsrc.c)   — real DATO.DAT load (staged by
 *     build_test.py) + dialog-frame draw + 6-frame slide; portrait_kind comes
 *     from data_fd2_chapter_intro_menu_speaker_portrait_id_table[cursor_state].
 *   - fd2_display_dialog_scene (dialog.c)  — the real dialog VM; driven against a
 *     synthetic all-game-text table (give_text below) where every page index the
 *     function uses (0x1FE/0x1FF/0x200 and the inventory-full table value 506/1)
 *     redirects to an immediate END (-1), so the VM returns with no glyph blit
 *     and no portrait flush (portrait_anim stays 0).
 *   - fd2_wait_for_input_dialog_with_blink(1) (input.c) — real busy-wait; a key
 *     pre-staged in the BIOS ring (menufix mfix_load_keys) makes it fall straight
 *     through INT 16h, consuming exactly one ring key per call. Its mode-1 entry
 *     blit resolves through the minipfix sprite sheet to a flat host-safe address.
 *   - fd2_party_roster_single_select_loop — the scripted testglob stub
 *     (g_single_select_ret/_cursor); each call returns the next scripted result
 *     and writes the matching cursor index. The give loop calls it TWICE per
 *     iteration (source then target), so the script drives both.
 *   - the transfer callees (fd2_count_usable_inventory_slots / fd2_get_inventory_
 *     slot_item_id / fd2_remove_inventory_slot_at / fd2_add_item_to_inventory /
 *     fd2_recalculate_combat_stats) are all REAL and pure in-memory ops over the
 *     repointed runtime_char array, so the give mechanic is asserted on real
 *     post-state.
 *
 * Ring-key budget per path (each fd2_wait_for_input_dialog_with_blink and each
 * accepted shop-input-loop iteration consumes exactly one INT 16h key; the dialog
 * VM with a bare-END stream and the scripted select stub consume none). Each of
 * these blocks ends with one wait: the loop top, the empty-inventory reject, the
 * give-to-whom prompt, and the target-full reject. Per test:
 *   - source-cancel:       1 (loop-top wait, then source Esc).
 *   - empty -> reloop:      3 (iter1 loop-top + iter1 empty-reject + iter2 loop-top).
 *   - item-select Esc:      3 (iter1 loop-top + shop Esc + iter2 loop-top).
 *   - full transfer:        4 (loop-top + shop commit + give-to-whom + iter2 loop-top).
 *   - target full:          5 (loop-top + shop commit + give-to-whom + full-reject
 *                              + iter2 loop-top).
 *
 * Risk coverage: the loop-top visible-count store + source-cancel early return;
 * the skip-empty inventory-build compaction + empty-list "nothing to give"
 * reject (with the char_id, not portrait_id, read into the sprite-id param); the
 * item-select Esc reloop; the full productive transfer (item removed from the
 * source slot, added unequipped to the target, source stats recomputed); and the
 * target-inventory-full reject. The portrait/dialog/slide side effects themselves
 * are shared infrastructure already pinned by tests/rsrc, tests/dialog and
 * tests/input and are not re-asserted here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "menufix.h"
#include "minipfix.h"

/* shop-panel title-blit + grid-render spies (testglob.c) */
extern int    g_dlg_blit_normal_calls;
extern int    g_shop_grid_render_calls;
extern uint32 g_shop_grid_last_count;
extern uint32 g_shop_grid_last_array;
extern uint32 g_shop_grid_last_cursor;
extern uint32 g_shop_grid_last_sell;
extern int    g_shop_grid_capture_list;
extern uint8  g_shop_grid_list[32];

/* scripted source/target single-select stub controls (testglob.c) */
extern int  g_single_select_ret[8];
extern int  g_single_select_cursor[8];
extern int  g_single_select_idx;
extern int  g_single_select_calls;

/* the default runtime-char array these tests temporarily repoint the shared
 * global at (testglob.c); the runner restores the global to it after the suite
 * so later suites that rely on the default wiring are not disturbed even if a
 * test bails early on an assertion failure. */
extern runtime_char g_test_rc_array[8];

/* A runtime_char array the give loop indexes for source/target. */
static runtime_char g_give_chars[4];

/* Sprite atlas for fd2_open_shop_dialog_panel's title blit (reads the relative
 * sprite offset at slot +0x46, then calls the blit spy). */
static uint8 g_give_atlas[0x100];

/* Synthetic all-game-text table: every page index 0..0x200 holds the byte offset
 * to a single END (-1) opcode at word 0x201, so fd2_display_dialog_scene returns
 * immediately for the give loop's pages (0x1FE/0x1FF/0x200) and for the
 * inventory-full table values (506 / 1), with no glyph blit. */
static uint16 g_give_text[0x202];

#define GIVE_END_WORD 0x201

/* Free the slide/portrait buffers the real fd2_load_chapter_portrait /
 * fd2_open_shop_dialog_panel leak each call (the close stub is a no-op), and zero
 * the globals so no dangling pointers survive into the next test. */
static void give_free_workspaces(void)
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
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
}

/* Common per-test setup: sprite sheet + immediate-END text + blit spies via
 * minip_setup_env (which also points data_fd2_all_game_text_ptr at its own small
 * table); then override the text pointer with give_text (large enough for the
 * give loop's high page indices) and reset the give-specific seams. */
static void give_reset(void)
{
    int i;

    minip_setup_env();                 /* sprite sheet + dialog blit spies */

    for (i = 0; i < GIVE_END_WORD; i++) {
        g_give_text[i] = (uint16)(GIVE_END_WORD * 2);
    }
    g_give_text[GIVE_END_WORD] = (uint16)-1;
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)g_give_text;

    /* cursor_state 1 -> portrait kind 0x80 (a real DATO.DAT resource the rsrc
     * suite already proves loads) and inventory-full text page 506 (in range). */
    data_fd2_chapter_intro_menu_cursor_state = 1;

    /* portrait buffer starts unallocated (loader frees prev iff nonzero). */
    data_fd2_portrait_sprite_buffer = 0;
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;

    /* sprite atlas for the panel title blit */
    *(uint32 *)(g_give_atlas + 0x46) = 0u;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)g_give_atlas;

    /* RNG seed feeds the blink-countdown draw inside the input wait. */
    data_fd2_shared_rng_seed = 0;
    /* tile map ptr only selects the blink-period constant (both host-safe). */
    data_fd2_battle_tile_map_ptr = 0;
    data_fd2_audio_fdother_sfx_bank_buf_ptr = 0x55AA;

    /* reset the scripted select stub */
    for (i = 0; i < 8; i++) {
        g_single_select_ret[i] = -1;
        g_single_select_cursor[i] = 0;
    }
    g_single_select_idx = 0;
    g_single_select_calls = 0;

    /* reset the panel/grid spies */
    g_dlg_blit_normal_calls = 0;
    g_shop_grid_render_calls = 0;
    g_shop_grid_last_count = 0;
    g_shop_grid_last_array = 0;
    g_shop_grid_last_cursor = 0xFFFFFFFFu;
    g_shop_grid_last_sell = 0xFFFFFFFFu;
    g_shop_grid_capture_list = 0;
    memset(g_shop_grid_list, 0xEE, sizeof(g_shop_grid_list));

    memset(g_give_chars, 0, sizeof(g_give_chars));
}

/* Occupy `n` of char `ci`'s 8 inventory slots (0..n-1) with item ids
 * base+0,base+1,... (flag = 0, occupied-unequipped) and mark the rest empty
 * (flag bit 0x80). Mirrors the gap-free inventory invariant the give loop relies
 * on (compacted-list index == raw slot). */
static void give_seed_inventory(int ci, int n, uint8 base)
{
    runtime_char *rc;
    int s;

    rc = &g_give_chars[ci];
    for (s = 0; s < 8; s++) {
        if (s < n) {
            rc->inventory_slots[s * 2] = 0x00;
            rc->inventory_slots[s * 2 + 1] = (uint8)(base + s);
        } else {
            rc->inventory_slots[s * 2] = 0x80;
            rc->inventory_slots[s * 2 + 1] = 0x00;
        }
    }
}

/* ----------------------------------------------------------------
 * 1. Source-select cancel returns immediately.
 *
 * The loop top runs the real portrait+dialog(0x200)+wait+close, sets the
 * visible-item count from the party count, then the (only) source select cancels
 * (-1) so the function returns before building any inventory list or opening the
 * item panel. Pins the loop-top visible-count store, the one ring key consumed by
 * the loop-top wait, and the source-cancel early return.
 * ---------------------------------------------------------------- */
static void test_give_source_cancel_returns(void)
{
    runtime_char *saved_rc;
    uint8 keys[1];

    give_reset();
    saved_rc = data_fd2_battle_runtime_char_array_ptr;
    data_fd2_battle_runtime_char_array_ptr = g_give_chars;

    g_single_select_ret[0] = -1;       /* source select cancels */

    data_fd2_shared_menu_party_member_count = 6;
    data_fd2_ui_menu_visible_item_count = 0;   /* must be set at the loop top */

    keys[0] = MFIX_SC_ENTER;           /* consumed by the loop-top wait */
    mfix_load_keys(keys, 1);

    fd2_run_give_item_menu();

    ASSERT_EQ(g_single_select_calls, 1);
    ASSERT_EQ(data_fd2_ui_menu_visible_item_count, 6);
    /* no item panel opened (source cancelled before the build/open). The grid
     * render fires only inside the panel open / shop loop, so 0 == panel never
     * reached. (g_dlg_blit_normal is not a panel signal here: the loop-top
     * portrait's dialog-frame assembly also emits a normal blit.) */
    ASSERT_EQ(g_shop_grid_render_calls, 0);

    data_fd2_battle_runtime_char_array_ptr = saved_rc;
    give_free_workspaces();
}

/* ----------------------------------------------------------------
 * 2. Empty source inventory -> "nothing to give" reject, then reloop.
 *
 * The source select returns a char whose 8 slots are all empty, so the build
 * loop yields count 0 and the loop shows the page-0x1FF reject dialog (NOT the
 * item panel) and loops back to the source roster, which cancels the 2nd time.
 * Pins the skip-empty build producing an empty list, the empty-list branch, the
 * char_id (+0x08, not portrait_id +0x07) written to the sprite-id param, and the
 * reloop. Ring keys: iter1 loop-top wait (1) + the empty-branch "nothing to give"
 * wait (1) + iter2 loop-top wait (1) = 3.
 * ---------------------------------------------------------------- */
static void test_give_empty_inventory_reloops(void)
{
    runtime_char *saved_rc;
    uint8 keys[3];

    give_reset();
    saved_rc = data_fd2_battle_runtime_char_array_ptr;
    data_fd2_battle_runtime_char_array_ptr = g_give_chars;

    /* source = char 2; mark every slot empty (flag 0x80) so the build loop skips
     * all 8 (give_reset's memset leaves flag 0 = occupied, so this is required). */
    give_seed_inventory(2, 0, 0x00);
    g_give_chars[2].char_id = 0x1A;    /* sprite-id param must be char_id+1 */
    g_give_chars[2].portrait_id = 0x07;/* different from char_id to disambiguate */

    g_single_select_ret[0] = 1;        /* 1st source select commits char 2 */
    g_single_select_cursor[0] = 2;
    g_single_select_ret[1] = -1;       /* 2nd source select cancels -> return */

    data_fd2_shared_menu_party_member_count = 5;
    data_fd2_dialog_last_action_sprite_id_param = 0;

    keys[0] = MFIX_SC_ENTER;           /* iter1 loop-top wait */
    keys[1] = MFIX_SC_ENTER;           /* iter1 empty-branch "nothing to give" wait */
    keys[2] = MFIX_SC_ENTER;           /* iter2 loop-top wait */
    mfix_load_keys(keys, 3);

    fd2_run_give_item_menu();

    ASSERT_EQ(g_single_select_calls, 2);
    /* the reject path set the sprite-id param to source char_id + 1 (0x1B),
     * proving the +0x08 read (portrait_id 0x07 would have given 0x08). */
    ASSERT_EQ(data_fd2_dialog_last_action_sprite_id_param, 0x1Bu);
    /* the item panel was never opened (empty list -> reject -> reloop); the grid
     * render fires only inside the panel/shop loop, so 0 confirms it. */
    ASSERT_EQ(g_shop_grid_render_calls, 0);

    data_fd2_battle_runtime_char_array_ptr = saved_rc;
    give_free_workspaces();
}

/* ----------------------------------------------------------------
 * 3. Item-select Esc cancels the shop grid -> reloop to the source roster.
 *
 * Source has items, so the loop resets cursor/scroll, opens the sell-mode item
 * panel and runs the real shop input loop; a staged Esc makes it return -1, so
 * the loop closes and reloops to the source roster (cancels the 2nd time) WITHOUT
 * the give-to-whom dialog or any transfer. The compacted list forwarded to the
 * panel (skip-empty + order preserved) is captured. Ring keys: iter1 loop-top
 * wait (1) + shop Esc (1) + iter2 loop-top wait (1) = 3.
 * ---------------------------------------------------------------- */
static void test_give_item_select_cancel_reloops(void)
{
    runtime_char *saved_rc;
    runtime_char *src;
    uint8 keys[3];

    give_reset();
    saved_rc = data_fd2_battle_runtime_char_array_ptr;
    data_fd2_battle_runtime_char_array_ptr = g_give_chars;

    /* source = char 0; occupy slots 0,1,3,5 (skip 2,4,6,7) so the compaction is
     * observable: list = [0x30,0x31,0x33,0x35]. */
    src = &g_give_chars[0];
    src->inventory_slots[0]  = 0x00; src->inventory_slots[1]  = 0x30;
    src->inventory_slots[2]  = 0x00; src->inventory_slots[3]  = 0x31;
    src->inventory_slots[4]  = 0x80; src->inventory_slots[5]  = 0x99;
    src->inventory_slots[6]  = 0x00; src->inventory_slots[7]  = 0x33;
    src->inventory_slots[8]  = 0x80; src->inventory_slots[9]  = 0x98;
    src->inventory_slots[10] = 0x00; src->inventory_slots[11] = 0x35;
    src->inventory_slots[12] = 0x80; src->inventory_slots[13] = 0x97;
    src->inventory_slots[14] = 0x80; src->inventory_slots[15] = 0x96;

    g_single_select_ret[0] = 1;        /* source commit char 0 */
    g_single_select_cursor[0] = 0;
    g_single_select_ret[1] = -1;       /* 2nd source select cancels -> return */

    data_fd2_shared_menu_party_member_count = 4;
    data_fd2_ui_menu_cursor_idx = 9;   /* seed non-zero so reset-to-0 is observable */
    data_fd2_ui_menu_scroll_offset = 6;
    g_shop_grid_capture_list = 1;      /* the array is the real on-stack list */

    keys[0] = MFIX_SC_ENTER;           /* iter1 loop-top wait */
    keys[1] = MFIX_SC_ESC;             /* shop grid cancel */
    keys[2] = MFIX_SC_ENTER;           /* iter2 loop-top wait */
    mfix_load_keys(keys, 3);

    fd2_run_give_item_menu();

    ASSERT_EQ(g_single_select_calls, 2);
    /* the panel opened once with the compacted list (4 items, empties dropped,
     * order preserved) and the cursor reset to 0 (sell_mode=1 layout). The grid
     * render fires exactly once: in the panel open. The shop loop took Esc as its
     * first key, so it moved nothing and did not re-render. (g_dlg_blit_normal is
     * not used as the panel signal: each loop-top portrait's dialog-frame
     * assembly also emits normal blits.) */
    ASSERT_EQ(g_shop_grid_render_calls, 1);
    ASSERT_EQ(g_shop_grid_last_count, 4);
    ASSERT_EQ(g_shop_grid_last_sell, 1);
    ASSERT_EQ(g_shop_grid_last_cursor, 0);
    ASSERT_EQ(g_shop_grid_list[0], 0x30);
    ASSERT_EQ(g_shop_grid_list[1], 0x31);
    ASSERT_EQ(g_shop_grid_list[2], 0x33);
    ASSERT_EQ(g_shop_grid_list[3], 0x35);
    ASSERT_EQ(data_fd2_ui_menu_scroll_offset, 0);

    g_shop_grid_capture_list = 0;
    data_fd2_battle_runtime_char_array_ptr = saved_rc;
    give_free_workspaces();
}

/* ----------------------------------------------------------------
 * 4. Full productive transfer: item moves source -> target.
 *
 * Source (char 0) has items; the shop loop commits the cursor-0 entry (slot 0);
 * the target (char 1) has room (< 8 items); the transfer reads the source slot,
 * removes it from the source (later slots shift up, slot 7 emptied), adds it
 * unequipped to the target's first free slot, and recomputes the SOURCE's combat
 * stats. After one productive iteration the source select cancels.
 *
 * Asserts the REAL post-state: the source slot-0 item is gone (the old slot-1
 * item shifted into slot 0, slot count dropped by one) and the target gained the
 * given item id in its first previously-free slot, unequipped (flag 0). Ring
 * keys: loop-top wait (1) + shop commit (1) + give-to-whom wait (1) = 3, then a
 * 4th loop-top wait for the cancelling 2nd iteration.
 * ---------------------------------------------------------------- */
static void test_give_full_flow_transfers_item(void)
{
    runtime_char *saved_rc;
    uint8 keys[4];

    give_reset();
    saved_rc = data_fd2_battle_runtime_char_array_ptr;
    data_fd2_battle_runtime_char_array_ptr = g_give_chars;

    /* source char 0: 3 items in slots 0,1,2 (ids 0x40,0x41,0x42), unequipped. */
    give_seed_inventory(0, 3, 0x40);
    /* target char 1: 2 items in slots 0,1 (ids 0x50,0x51); slots 2..7 free. */
    give_seed_inventory(1, 2, 0x50);

    /* 1st source select -> char 0; target select -> char 1; 2nd source cancel. */
    g_single_select_ret[0] = 1;  g_single_select_cursor[0] = 0;  /* source */
    g_single_select_ret[1] = 1;  g_single_select_cursor[1] = 1;  /* target */
    g_single_select_ret[2] = -1;                                 /* exit */

    data_fd2_shared_menu_party_member_count = 4;

    keys[0] = MFIX_SC_ENTER;   /* loop-top wait (iter1) */
    keys[1] = MFIX_SC_ENTER;   /* shop commit at cursor 0 -> slot 0 */
    keys[2] = MFIX_SC_ENTER;   /* give-to-whom wait */
    keys[3] = MFIX_SC_ENTER;   /* loop-top wait (iter2, then source cancel) */
    mfix_load_keys(keys, 4);

    fd2_run_give_item_menu();

    /* both productive selects + the cancelling source select consumed */
    ASSERT_EQ(g_single_select_calls, 3);

    /* SOURCE (char 0): the slot-0 item (0x40) was removed; remove shifts the
     * remaining items up, so slot 0 now holds the old slot-1 item (0x41), slot 1
     * holds the old slot-2 item (0x42), and slot 2 became empty. Net: 2 items. */
    ASSERT_EQ(fd2_count_usable_inventory_slots(0), 2);
    ASSERT_EQ(g_give_chars[0].inventory_slots[0], 0x00);   /* slot0 flag occupied */
    ASSERT_EQ(g_give_chars[0].inventory_slots[1], 0x41);   /* slot0 id shifted up */
    ASSERT_EQ(g_give_chars[0].inventory_slots[2], 0x00);
    ASSERT_EQ(g_give_chars[0].inventory_slots[3], 0x42);
    ASSERT_EQ(g_give_chars[0].inventory_slots[4] & 0x80, 0x80); /* slot2 now empty */

    /* TARGET (char 1): gained the given item (0x40) in its first free slot (2),
     * unequipped (flag 0); slot count rose from 2 to 3. */
    ASSERT_EQ(fd2_count_usable_inventory_slots(1), 3);
    ASSERT_EQ(g_give_chars[1].inventory_slots[4], 0x00);   /* slot2 flag occupied */
    ASSERT_EQ(g_give_chars[1].inventory_slots[5], 0x40);   /* given item id */

    data_fd2_battle_runtime_char_array_ptr = saved_rc;
    give_free_workspaces();
}

/* ----------------------------------------------------------------
 * 5. Target inventory full -> reject, no transfer.
 *
 * Same productive flow as #4 up to the target select, but the target (char 1)
 * already holds 8 items, so fd2_count_usable_inventory_slots == 8 takes the
 * reject branch: it sets the sprite-id param to the TARGET's char_id + 1, shows
 * the inventory-full dialog, and loops back WITHOUT removing from the source or
 * adding to the target. The source select cancels the 2nd time. Pins the full
 * guard, the target char_id read, and that no transfer occurs. Ring keys: iter1
 * loop-top wait (1) + shop commit (1) + give-to-whom wait (1) + the full-branch
 * reject wait (1) + iter2 loop-top wait (1) = 5.
 * ---------------------------------------------------------------- */
static void test_give_target_full_rejects(void)
{
    runtime_char *saved_rc;
    uint8 keys[5];

    give_reset();
    saved_rc = data_fd2_battle_runtime_char_array_ptr;
    data_fd2_battle_runtime_char_array_ptr = g_give_chars;

    /* source char 0: 3 items (slots 0,1,2). target char 1: all 8 slots full. */
    give_seed_inventory(0, 3, 0x40);
    give_seed_inventory(1, 8, 0x50);
    g_give_chars[1].char_id = 0x09;     /* sprite-id param must be char_id + 1 */
    g_give_chars[1].portrait_id = 0x20; /* distinct from char_id */

    g_single_select_ret[0] = 1;  g_single_select_cursor[0] = 0;  /* source */
    g_single_select_ret[1] = 1;  g_single_select_cursor[1] = 1;  /* target (full) */
    g_single_select_ret[2] = -1;                                 /* exit */

    data_fd2_shared_menu_party_member_count = 4;
    data_fd2_dialog_last_action_sprite_id_param = 0;

    keys[0] = MFIX_SC_ENTER;   /* loop-top wait (iter1) */
    keys[1] = MFIX_SC_ENTER;   /* shop commit at cursor 0 */
    keys[2] = MFIX_SC_ENTER;   /* give-to-whom wait */
    keys[3] = MFIX_SC_ENTER;   /* full-branch reject wait */
    keys[4] = MFIX_SC_ENTER;   /* loop-top wait (iter2, then source cancel) */
    mfix_load_keys(keys, 5);

    fd2_run_give_item_menu();

    ASSERT_EQ(g_single_select_calls, 3);
    /* reject set the sprite-id param to TARGET char_id + 1 (0x0A) via the +0x08
     * read (portrait_id 0x20 would have given 0x21). */
    ASSERT_EQ(data_fd2_dialog_last_action_sprite_id_param, 0x0Au);
    /* no transfer: source still has its 3 items, target still full at 8. */
    ASSERT_EQ(fd2_count_usable_inventory_slots(0), 3);
    ASSERT_EQ(g_give_chars[0].inventory_slots[0], 0x00);
    ASSERT_EQ(g_give_chars[0].inventory_slots[1], 0x40);   /* slot 0 unchanged */
    ASSERT_EQ(fd2_count_usable_inventory_slots(1), 8);

    data_fd2_battle_runtime_char_array_ptr = saved_rc;
    give_free_workspaces();
}

void run_ui_menu_shop2_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/shop2\n");
    RUN_TEST(test_give_source_cancel_returns);
    RUN_TEST(test_give_empty_inventory_reloops);
    RUN_TEST(test_give_item_select_cancel_reloops);
    RUN_TEST(test_give_full_flow_transfers_item);
    RUN_TEST(test_give_target_full_rejects);
    /* restore the shared runtime-char global to its default even if a test above
     * bailed early (ASSERT returns before its own restore), so the downstream
     * status suite sees the default g_test_rc_array wiring. */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}
