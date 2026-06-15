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

/* ----------------------------------------------------------------
 * data_fd2_ui_item_command_menu_template @ 0x51F05  (16 bytes)
 *
 * Item command submenu option list: 4 x int32 entries { 8, 9, 10, 11 }
 * (Use / Give / Sort-Equip / Drop). The sole reader
 * fd2_item_command_menu_dispatch copies all four 32-bit words into a local
 * menu_options[16] buffer with a count-4 REP MOVSD (MOV ECX,4; MOV ESI,0x51F05;
 * REP MOVSD -> 32-bit elements, int* / stride-4 loop), then passes that buffer
 * to fd2_open_settings_dialog_with_slide. The current_menu_cursor_idx (0..3)
 * selected from this menu dispatches the Use/Give/Sort/Drop branches.
 * Read-only; the source array is never written -- only the local copy is
 * consumed (paired with data_fd2_ui_item_command_menu_state_template @ 0x53F32,
 * which holds the per-option enable/disable flags mutated on the stack, e.g.
 * Give is greyed out when no adjacent ally tile is in range).
 */
const int32 data_fd2_ui_item_command_menu_template[4] = { 8, 9, 10, 11 };

/* ----------------------------------------------------------------
 * data_fd2_ui_tactical_overview_team_colors_table @ 0x5208A  (12 bytes)
 *
 * Tactical-overview (zoom-out battlefield map) per-team palette color bases:
 * 3 x int32 entries { 0x20, 0x50, 0x48 } for player / enemy / neutral team.
 * The sole reader fd2_open_tactical_overview_zoom copies all three 32-bit
 * words into a local team_color_table_a[3] stack buffer with a count-3 MOVSD
 * (MOV ESI,0x5208A; 3 x MOVSD -> 32-bit elements), then indexes that buffer by
 * each char's team id (rt_char[6], values 0..2) with stride 4 as a full dword:
 * team_color_base + anim_phase (0..7) yields the palette index passed to
 * fd2_fill_screen_rect_with_byte to draw the char's 1-byte team-tinted square.
 * Read-only; the source array is never written -- only the local copy is read.
 */
const int32 data_fd2_ui_tactical_overview_team_colors_table[3] = { 0x20, 0x50, 0x48 };

/* ----------------------------------------------------------------
 * data_fd2_ui_per_job_revive_or_promote_cost_table @ 0x5266B  (60 bytes)
 *
 * Per-job cost multiplier used to price the revive/promote candidate grid:
 * 30 x int16 (signed short), indexed by (job_id - 1) where job_id is 1..0x1A.
 * The sole reader fd2_render_promote_members_grid (@ 0x30A47) computes the
 * displayed price as:
 *     price = char.bLevel * cost_table[char.bJob_id - 1]
 * then renders it as an orange 5-digit decimal beside a coin icon. The element
 * access uses a (signed short) load sign-extended to int -- a stride-2 / 16-bit
 * read (the older Ghidra plate showed BYTE_ARRAY[(job_id - 1) * 2], i.e. byte
 * stride 2). All stored values are positive (max 3000), so the sign extension
 * never alters the result. 26 meaningful job entries (indices 0..25) cover
 * job_id 1..0x1A; the final 4 entries are 100 filler. Read-only; never written.
 */
const int16 data_fd2_ui_per_job_revive_or_promote_cost_table[30] = {
     100,  150,  100,  100,  100,  100,  100,  100,
    1200, 1600, 1000, 1000, 1200, 1400, 1200, 1600,
     100, 1800, 1200, 1000, 3000, 1000, 1000, 1400,
     350,  100,  100,  100,  100,  100
};

/* ----------------------------------------------------------------
 * data_fd2_ui_per_basic_portrait_class_change_key_item_id_table @ 0x526A7
 *   (18 bytes, = 0x5266B + 0x3C, immediately after the cost table above)
 *
 * Per-basic-class required class-change key-item id, indexed directly by
 * runtime_char.bPortrait_id (basic classes 0..0x11 -> 18 entries). Both
 * readers load a single byte and zero-extend it (MOVZX EAX, byte ptr
 * [idx + 0x526A7]) to use as an item id:
 *   - fd2_build_promotion_candidates_with_targets (@ 0x3180A) passes
 *       table[portrait_id] to fd2_find_inventory_slot_with_item to test
 *       whether the member carries the item that unlocks the alt promotion.
 *   - fd2_run_class_promotion_menu_main (@ 0x31525) reads
 *       table[bPortrait_id] as the item to consume when class_id > 0x31.
 * Stride 1, unsigned byte, never written -> const uint8 flat table.
 * 0xFF marks classes with no table-driven item; the 0xCD bytes at indices
 * 4..7 are unused filler (portrait_id 7 is skipped by the candidate loop).
 */
const uint8 data_fd2_ui_per_basic_portrait_class_change_key_item_id_table[18] = {
    0x59, 0x5D, 0xFF, 0x5D, 0xCD, 0xCD, 0xCD, 0xCD, 0x5C,
    0x58, 0x58, 0x58, 0x5B, 0x5C, 0x58, 0x5B, 0xFF, 0xFF
};

/* ----------------------------------------------------------------
 * data_fd2_ui_chapter_intro_dialog_corner_offset_table_a @ 0x526DA  (16 bytes)
 *
 * Speech-bubble "wing" corner offsets for the chapter-intro dialog panel
 * open/close animation: 4 x int32 (signed) { -39, -13, 13, 39 }. The sole
 * reader fd2_animate_tutorial_dialog_intro_or_outro (@ 0x2D669) copies all
 * four 32-bit words into a local corner_offs[16] buffer with a count-4 REP
 * MOVSD (MOV ECX,4; MOV ESI,0x526DA; REP MOVSD -> 32-bit elements), then per
 * animation frame computes each wing's blit destination as
 *     base + corner_offs[i] / divisor + 0xD430
 * where the element is loaded as a signed dword and divided via IDIV (SAR
 * EDX,0x1F sign-extend then IDIV). The symmetric -39/-13/13/39 pairs place
 * the four wings around the panel center; divisor ramps 1..4 (open) or 4..1
 * (close) to slide the wings in/out. Stride 4, signed int32, never written.
 */
const int32 data_fd2_ui_chapter_intro_dialog_corner_offset_table_a[4] = { -39, -13, 13, 39 };
