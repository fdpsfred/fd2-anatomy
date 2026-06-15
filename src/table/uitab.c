/*
 * uitab.c -- UI menu read-only data tables (.object2)
 *
 * Static templates for the in-field command menu and related UI dialogs.
 * Each template is copied (not referenced) into a local stack buffer at
 * menu-open time, then handed to the settings-dialog renderer.
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * data_fd2_ui_field_command_menu_options_template @ 0x51E9F  (16 bytes)
 *
 * Field command menu option list: 4 x int32 entries { 7, 5, 6, 4 }.
 * fd2_field_command_menu_loop copies all four 32-bit words into a local
 * menu_options[16] buffer with an int* / stride-4 loop, then passes that
 * buffer to fd2_open_settings_dialog_with_slide to render the modal
 * (Save/Load, End Turn, Options, Suspend). Read-only; the source array is
 * never written.
 */
const int32 data_fd2_ui_field_command_menu_options_template[4] = { 7, 5, 6, 4 };

/* ----------------------------------------------------------------
 * data_fd2_ui_game_options_menu_slots_template @ 0x51EAF  (16 bytes)
 *
 * Game options menu base text-token IDs: 4 x int32 entries
 * { 0x12, 0x14, 0x16, 0x18 } (BGM / SE / Speed / Other).
 * fd2_game_options_menu_loop copies all four 32-bit words into a local
 * menu_slot_0..3 buffer with an int* / stride-4 loop (REP MOVSD, count 4),
 * then adds 0 or 1 to each slot per the current toggle state to pick the
 * ON/OFF (or Slow/Fast) label variant. Read-only; the source array is never
 * written -- only the local copies are mutated.
 */
const int32 data_fd2_ui_game_options_menu_slots_template[4] = { 0x12, 0x14, 0x16, 0x18 };

/* ----------------------------------------------------------------
 * data_fd2_ui_inline_action_menu_template @ 0x51ED5  (16 bytes)
 *
 * Inline action submenu option list: 4 x int32 entries { 0, 1, 2, 3 }
 * (Attack / Spell / Item / Wait), opened after the player picks a
 * destination tile. fd2_player_inline_action_menu_dispatch copies all four
 * 32-bit words into a local menu_template[16] buffer with an int* / stride-4
 * loop (REP MOVSD, count 4), then passes that buffer to the settings-dialog
 * open / input / close routines. Read-only; the source array is never
 * written -- only the local copy is consumed.
 */
const int32 data_fd2_ui_inline_action_menu_template[4] = { 0, 1, 2, 3 };

/* ----------------------------------------------------------------
 * data_fd2_ui_save_load_newgame_menu_template @ 0x51EF5  (16 bytes)
 *
 * Field status/save/load/quit submenu option list: 4 x int32 entries
 * { 12, 13, 14, 15 } (Status / Save / Load / Quit). The sole reader
 * fd2_field_menu_status_save_load_quit_dispatch copies all four 32-bit words
 * into a local menu_options[16] buffer with a count-4 REP MOVSD (32-bit
 * elements), then passes that buffer to fd2_open_settings_dialog_with_slide.
 * Read-only; the source array is never written -- only the local copy is
 * consumed (paired with data_fd2_ui_save_load_menu_state_template @ 0x53F22,
 * which holds the per-option enable/disable flags mutated on the stack).
 */
const int32 data_fd2_ui_save_load_newgame_menu_template[4] = { 12, 13, 14, 15 };
