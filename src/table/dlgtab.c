/*
 * dlgtab.c -- text-dialog read-only data tables (.object2)
 *
 * Constant templates consumed by the dialog page-advance / Yes-No prompt
 * animation primitives. Read-only; no writers.
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * data_fd2_dialog_advance_collapse_template @ 0x51EE5  (16 bytes, int32[4])
 *
 * Left-corner sprite-coordinate base values for the dialog box "fold in"
 * transition. Accessed as two int32 pairs:
 *   [0],[1] read by fd2_animate_dialog_page_advance_collapse
 *           (local base + corner data, copied via two MOVSD from 0x51EE5)
 *   [2],[3] read by fd2_text_dialog_typewriter_loop
 *           (same layout, copied via two MOVSD from 0x51EED)
 * Each element is used as a signed int in sprite-blit address arithmetic.
 * Read-only constant table.
 */
const int32 data_fd2_dialog_advance_collapse_template[4] = { 0x10, 0x11, 0x10, 0x11 };

/* ----------------------------------------------------------------
 * data_fd2_dialog_shop_inventory_full_dialog_text_id_table @ 0x5265F (12 bytes, int16[6])
 *
 * Per-shop-tier FDTXT dialog page text-id for the "inventory is full" reject
 * message in the chapter-intro shop menu. Indexed by the shop-tier radio
 * state data_fd2_chapter_intro_menu_cursor_state (0..5).
 *
 * Readers (both index [cursor_state] and pass the int16 as the FDTXT page id
 * to fd2_display_dialog_scene):
 *   fd2_run_buy_item_menu  @ 0x2F3C1  (inventory-full on buy)
 *   fd2_run_give_item_menu @ 0x2FB4E  (inventory-full on give)
 * Sibling tier tables (buy_for/no_money/no_equip/auto_equip @ 0x526FA..) are
 * accessed *(short *)(base + cursor_state*2), confirming int16 stride.
 * Read-only constant table; no writers.
 *
 * NOTE: the adjacent symbol data_fd2_ui_per_job_revive_or_promote_cost_table
 * @ 0x5266B (read [job_id-1] by fd2_render_promote_members_grid) is a SEPARATE
 * symbol with its own xref, NOT an overflow alias of this one.
 */
const int16 data_fd2_dialog_shop_inventory_full_dialog_text_id_table[6] = {
    0x0001, 0x01FA, 0x0001, 0x01FA, 0x01FA, 0x01FA
};

/* ----------------------------------------------------------------
 * data_fd2_dialog_shop_buy_for_dialog_text_id_table @ 0x526FA (12 bytes, int16[6])
 *
 * Per-shop-tier FDTXT dialog page text-id for the "buy for whom?" confirmation
 * prompt in the chapter-intro shop menu. Indexed by the shop-tier radio state
 * data_fd2_chapter_intro_menu_cursor_state (0..5).
 *
 * Sole reader fd2_run_buy_item_menu @ 0x2F0CE block-copies the 12 bytes into a
 * stack-local buffer (3x MOVSD from 0x526FA), then reads it back as a signed
 * 16-bit word: MOVSX EAX, word ptr [ESP + cursor_state*2 + ...] @ 0x2F184, and
 * passes the value as the FDTXT page id to fd2_display_dialog_scene. The *2
 * stride + MOVSX word confirms int16 (signed) elements; index range 0..5 gives
 * the 6-element dimension.
 * Read-only constant table; no writers.
 */
const int16 data_fd2_dialog_shop_buy_for_dialog_text_id_table[6] = {
    0x0001, 0x01F6, 0x0001, 0x01B7, 0x0001, 0x01B7
};

/* ----------------------------------------------------------------
 * data_fd2_dialog_shop_no_money_dialog_text_id_table @ 0x52706 (12 bytes, int16[6])
 *
 * Per-shop-tier FDTXT dialog page text-id for the "can't afford" reject prompt
 * shown when party gold is below the item price in the chapter-intro shop menu.
 * Indexed by the shop-tier radio state data_fd2_chapter_intro_menu_cursor_state
 * (0..5).
 *
 * Sole reader fd2_run_buy_item_menu @ 0x2F0CE block-copies the 12 bytes into a
 * stack-local buffer (MOVSD from 0x52706), then -- only on the affordability
 * failure branch (party_total_gold < price) -- reads it back as a signed 16-bit
 * word: MOVSX EAX, word ptr [ESP + cursor_state*2 + ...] and passes the value as
 * the FDTXT page id to fd2_display_dialog_scene. The *2 stride + MOVSX word
 * confirms int16 (signed) elements; index range 0..5 gives the 6-element
 * dimension. (Buy-only: the give-item flow has no price check, so this table has
 * a single reader.)
 * Read-only constant table; no writers.
 */
const int16 data_fd2_dialog_shop_no_money_dialog_text_id_table[6] = {
    0x0001, 0x01F8, 0x0001, 0x01B6, 0x0001, 0x01B6
};

/* ----------------------------------------------------------------
 * data_fd2_dialog_shop_no_equip_dialog_text_id_table @ 0x52712 (12 bytes, int16[6])
 *
 * Per-shop-tier FDTXT dialog page text-id for the "no one can equip this"
 * reject prompt shown when a gear item has zero equip-capable recipients in
 * the chapter-intro shop menu. Indexed by the shop-tier radio state
 * data_fd2_chapter_intro_menu_cursor_state (0..5).
 *
 * Sole reader fd2_run_buy_item_menu @ 0x2F0E6 block-copies the 12 bytes into a
 * stack-local buffer (3x MOVSD from 0x52712), then -- only on the gear /
 * no-eligible-candidate branch -- reads it back as a signed 16-bit word:
 * MOVSX EAX, word ptr [ESP + cursor_state*2 + ...] @ 0x2F184, and passes the
 * value as the FDTXT page id to fd2_display_dialog_scene. The *2 stride +
 * MOVSX word confirms int16 (signed) elements; index range 0..5 gives the
 * 6-element dimension.
 * Read-only constant table; no writers.
 */
const int16 data_fd2_dialog_shop_no_equip_dialog_text_id_table[6] = {
    0x0001, 0x01F9, 0x0001, 0x01B5, 0x0001, 0x01B5
};

/* ----------------------------------------------------------------
 * data_fd2_dialog_shop_auto_equip_dialog_text_id_table @ 0x5271E (12 bytes, int16[6])
 *
 * Per-shop-tier FDTXT dialog page text-id for the "auto-equip the gear just
 * bought?" yes/no prompt shown after a gear purchase is added to a recipient's
 * inventory in the chapter-intro shop menu. Indexed by the shop-tier radio state
 * data_fd2_chapter_intro_menu_cursor_state (0..5).
 *
 * Sole reader fd2_run_buy_item_menu @ 0x2F0ED block-copies the 12 bytes into a
 * stack-local buffer (3x MOVSD from 0x5271E), then -- only on the gear /
 * post-add-to-inventory branch -- reads it back as a signed 16-bit word:
 * MOVSX EAX, word ptr [ESP + cursor_state*2 + 0x54] @ 0x2F441, and passes the
 * value as the FDTXT page id to fd2_display_dialog_scene. The *2 stride + MOVSX
 * word confirms int16 (signed) elements; index range 0..5 gives the 6-element
 * dimension. (Buy-only: only gear purchases reach the auto-equip prompt, so this
 * table has a single reader.)
 * Read-only constant table; no writers.
 */
const int16 data_fd2_dialog_shop_auto_equip_dialog_text_id_table[6] = {
    0x0001, 0x01FB, 0x0001, 0x01FB, 0x0001, 0x01FB
};

/* ----------------------------------------------------------------
 * data_fd2_dialog_shop_sell_for_dialog_text_id_table @ 0x5272A (12 bytes, int16[6])
 *
 * Per-shop-tier FDTXT dialog page text-id for the "sell this for <price>?"
 * yes/no confirmation prompt in the chapter-intro shop SELL flow. Indexed by
 * the shop-tier radio state data_fd2_chapter_intro_menu_cursor_state (0..5).
 *
 * Sole reader fd2_run_sell_item_menu @ 0x2F654 block-copies the 12 bytes into a
 * stack-local buffer (3x MOVSD from 0x5272A), then on the sale-confirm branch
 * reads it back as a signed 16-bit word: MOVSX EAX, word ptr [ESP +
 * cursor_state*2 + 0x1c] @ 0x2F80C, and passes the value as the FDTXT page id to
 * fd2_display_dialog_scene. The *2 stride + MOVSX word confirms int16 (signed)
 * elements; index range 0..5 gives the 6-element dimension. (Sell-only: the
 * confirm prompt is reached only from the sell flow, so this table has a single
 * reader.)
 * Read-only constant table; no writers.
 */
const int16 data_fd2_dialog_shop_sell_for_dialog_text_id_table[6] = {
    0x01FC, 0x01FC, 0x01FC, 0x0293, 0x01FC, 0x01FC
};

/* ----------------------------------------------------------------
 * data_fd2_dialog_shop_sell_nothing_to_sell_text_id_table @ 0x52736 (12 bytes, int16[6])
 *
 * Per-shop-tier FDTXT dialog page text-id for the "you have nothing to sell"
 * reject prompt shown when the selected character's inventory is empty in the
 * chapter-intro shop SELL flow. Indexed by the shop-tier radio state
 * data_fd2_chapter_intro_menu_cursor_state (0..5).
 *
 * Sole reader fd2_run_sell_item_menu @ 0x2F660 block-copies the 12 bytes into a
 * stack-local buffer (3x MOVSD from 0x52736), then -- only on the empty-inventory
 * branch -- reads it back as a signed 16-bit word: MOVSX EAX, word ptr [ESP +
 * cursor_state*2 + 0x28] @ 0x2F6DE, and passes the value as the FDTXT page id to
 * fd2_display_dialog_scene. The *2 stride + MOVSX word confirms int16 (signed)
 * elements; index range 0..5 gives the 6-element dimension. (Sell-only: the
 * nothing-to-sell prompt is reached only from the sell flow, so this table has a
 * single reader.)
 * All six entries are 0x01FD (the same FDTXT page across every shop tier).
 * Read-only constant table; no writers.
 */
const int16 data_fd2_dialog_shop_sell_nothing_to_sell_text_id_table[6] = {
    0x01FD, 0x01FD, 0x01FD, 0x01FD, 0x01FD, 0x01FD
};
