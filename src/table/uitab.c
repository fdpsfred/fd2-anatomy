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
 * data_fd2_ui_field_status_save_load_quit_menu_template @ 0x51EF5  (16 bytes)
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
const int32 data_fd2_ui_field_status_save_load_quit_menu_template[4] = { 12, 13, 14, 15 };

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
 * 3 x int32 entries { 0x20, 0x50, 0x48 } indexed by runtime_char team id:
 *   [0] 0x20 -> team 0 (enemy)
 *   [1] 0x50 -> team 1 (ally / NPC)
 *   [2] 0x48 -> team 2 (player)
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
 * Both readers compute the same price:
 *     price = char.bLevel * cost_table[char.bJob_id - 1]
 *   fd2_render_promote_members_grid @ 0x30A47 -- the price listed per
 *     candidate in the revive grid; renders it as an orange 5-digit decimal
 *     beside a coin icon. Encodes the base directly: [job_id*2 + 0x5266B]
 *     with a separate DEC of job_id.
 *   fd2_run_revive_menu_main @ 0x30DC3 -- the price quoted in the "pay X
 *     gold?" confirmation and then deducted from party gold. Here the
 *     compiler folded the -1 into the base: [job_id*2 + 0x52669], and
 *     0x52669 == this table - 2 falls inside the preceding int16[6] table
 *     @ 0x5265F. That is a folded-base encoding artifact only; reading it
 *     as an index into that neighbour would make the charged price diverge
 *     from the listed one.
 * The element
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
 * runtime_char.bPortrait_id. Portrait ids 0..0x11 are the 18 early-game
 * recruitable characters in their basic (un-promoted) form, so each table
 * entry names the key item that unlocks that character's *alternate* (special)
 * promotion path. Both readers load a single byte and zero-extend it (MOVZX
 * EAX, byte ptr [idx + 0x526A7]) to use as an item id:
 *   - fd2_build_promotion_candidates_with_targets (@ 0x3180A) passes
 *       table[portrait_id] to fd2_find_inventory_slot_with_item; carrying that
 *       item switches the target class from default portrait_id+0x20 to the alt
 *       portrait_id+0x32.
 *   - fd2_run_class_promotion_menu_main (@ 0x31525) reads table[bPortrait_id]
 *       as the item to consume when the chosen class_id > 0x31 (an alt path).
 * Stride 1, unsigned byte, never written -> const uint8 flat table.
 * All values except 0xFF are real class-change ("轉職") item ids:
 *   0x58 聖者之戒 (priest/mage->saint), 0x59 勇者徽章 (Sol->hero),
 *   0x5B 領悟之書 (monk->martial-saint), 0x5C 心眼之書 (archer->sniper),
 *   0x5D 白金徽章 (soldier->magic-warrior), 0xCD 飛龍卵 (knight->dragon-knight).
 * Index meaning: [0]索爾 0x59, [1]哈諾/[3]哈瓦特 0x5D, [4]亞雷斯/[5]洛娜/[6]萊汀
 * 0xCD (the three knight chars), [8]希莉亞/[13]貝克威 0x5C, [9]悠妮/[10]瑪琳/
 * [11]索菲亞/[14]珊 0x58, [12]凱麗/[15]賽可邦勒 0x5B. 0xFF (idx 2,16,17) marks
 * characters with no table-driven alt item. Index 7 (蘭斯洛特) is the one entry
 * never read: the candidate loop skips portrait_id == 7. (悠妮/idx 9 has an
 * extra hard-coded path: carrying Sword 0x5A also unlocks class 0x34.)
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
 * reader fd2_animate_chapter_intro_dialog_wings (@ 0x2D669) copies all
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

/* ----------------------------------------------------------------
 * data_fd2_ui_chapter_intro_dialog_corner_offset_table_b @ 0x526EA  (16 bytes)
 *
 * Twin of table_a above (immediately adjacent: 0x526DA + 0x10 = 0x526EA),
 * with the same four signed corner offsets { -39, -13, 13, 39 }. Used by the
 * chapter-intro dialog input-wait loop fd2_wait_input_with_chapter_dialog_blink
 * (@ 0x2D85F): it copies all four 32-bit words into a local corner_offs[16]
 * buffer with a count-4 REP MOVSD (MOV ECX,4; MOV ESI,0x526EA; REP MOVSD ->
 * 32-bit elements), then on each frame passes that buffer to
 * fd2_render_chapter_intro_dialog_panels. When mode==0 it also indexes the
 * local copy by corner i (0..3) with stride 4, loading each element as a signed
 * dword (MOV EAX,[ESP+i*4+8]) and adding the framebuffer base 0xAD430 to place
 * the four cursor-corner indicator sprites. Stride 4, signed int32, never
 * written -> const int32 flat table.
 */
const int32 data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[4] = { -39, -13, 13, 39 };

/* ----------------------------------------------------------------
 * data_fd2_ui_field_command_menu_state_template @ 0x53EF2  (16 bytes)
 *
 * Per-option enable/disable flag state for the in-field command menu,
 * paired with data_fd2_ui_field_command_menu_options_template @ 0x51E9F.
 * 4 x int32 entries, all 0 (= all four options Save/Load, End Turn,
 * Options, Suspend are always selectable). The sole reader
 * fd2_field_command_menu_loop (@ 0x16F55) copies all four 32-bit words into
 * a local menu_state[16] buffer with a count-4 REP MOVSD (MOV ECX,4;
 * MOV ESI,0x53EF2; REP MOVSD -> 32-bit elements), then hands that buffer to
 * fd2_open_settings_dialog_with_slide / fd2_settings_menu_input_step, which
 * mutate the per-option flags on the local copy during navigation. The
 * global source array is never written -> statically all zero. Element type
 * int32[4] matches the dword copy stride and the sibling *_menu_state_template
 * family (e.g. data_fd2_ui_save_load_menu_state_template @ 0x53F22).
 */
const int32 data_fd2_ui_field_command_menu_state_template[4] = { 0, 0, 0, 0 };

/* ----------------------------------------------------------------
 * data_fd2_ui_game_options_menu_state_template @ 0x53F02  (16 bytes)
 *
 * Per-option enable/disable flag state for the game-options (settings) menu,
 * paired with data_fd2_ui_game_options_menu_slots_template @ 0x51EAF.
 * 4 x int32 entries, all 0 (= all four options BGM / SE / Speed / Other are
 * always selectable). The sole reader fd2_game_options_menu_loop (@ 0x16FDD)
 * copies all four 32-bit words into a local menu_state[16] buffer with a
 * count-4 REP MOVSD (MOV ECX,4; MOV ESI,0x53F02; REP MOVSD -> 32-bit
 * elements), then hands that buffer to fd2_open_settings_dialog_with_slide /
 * fd2_settings_menu_input_step, which read the per-option flags on the local
 * copy (EBX[0/4/8/0xc]) to skip greyed-out options during navigation. The
 * global source array is never written -> statically all zero. Element type
 * int32[4] matches the dword copy stride and the sibling *_menu_state_template
 * family (e.g. data_fd2_ui_field_command_menu_state_template @ 0x53EF2).
 */
const int32 data_fd2_ui_game_options_menu_state_template[4] = { 0, 0, 0, 0 };

/* ----------------------------------------------------------------
 * data_fd2_ui_player_action_menu_state_template @ 0x53F12  (16 bytes)
 *
 * Per-slot enable/disable flag state for the player's inline action submenu
 * (Attack / Spell / Item / Wait), paired with
 * data_fd2_ui_inline_action_menu_template @ 0x51ED5. 4 x int32 entries, all 0.
 * The sole reader fd2_player_action_menu_loop (@ 0x18890) copies all four
 * 32-bit words into a local menu_state[16] buffer with a count-4 REP MOVSD
 * (MOV ECX,4; MOV ESI,0x53F12; REP MOVSD -> 32-bit elements) at the start of
 * the player's turn UI. It then mutates only that local copy -- when the unit
 * actually moved before acting it sets menu_state[4] = 1 (a full dword write,
 * MOV dword ptr [ESP+4],1, i.e. ((int*)menu_state)[1] = 1) to grey out the
 * Spell slot, and passes the local copy to
 * fd2_player_inline_action_menu_dispatch, which declares it as int* and indexes
 * it as a 4-int gating array (pSlot_disable_arr[0..3], non-zero = disabled).
 * The global source array is never written -> statically all zero. Element type
 * int32[4] is confirmed by both the dword copy stride and the consumer's int*
 * indexing, and matches the sibling *_menu_state_template family (e.g.
 * data_fd2_ui_game_options_menu_state_template @ 0x53F02).
 */
const int32 data_fd2_ui_player_action_menu_state_template[4] = { 0, 0, 0, 0 };

/* ----------------------------------------------------------------
 * data_fd2_ui_save_load_menu_state_template @ 0x53F22  (16 bytes)
 *
 * Per-option enable/disable flag state for the field status/save/load/quit
 * submenu, paired with data_fd2_ui_field_status_save_load_quit_menu_template @ 0x51EF5.
 * 4 x int32 entries, all 0 at rest. The sole reader
 * fd2_field_menu_status_save_load_quit_dispatch (@ 0x19DF7) copies all four
 * 32-bit words into a local menu_state[16] buffer with a count-4 REP MOVSD
 * (MOV ECX,4; MOV ESI,0x53F22; REP MOVSD -> 32-bit elements), then mutates
 * only that local copy: when no save file exists it sets menu_state[8] = 1
 * (((int*)menu_state)[2] = 1, greys out the Load option), and when a party
 * member is alive-but-acted it sets menu_state[4] = 1 (((int*)menu_state)[1]
 * = 1, greys out the Save option). The mutated copy is handed to
 * fd2_open_settings_dialog_with_slide / fd2_settings_menu_input_step. The
 * global source array is never written -> statically all zero. Element type
 * int32[4] matches the dword copy stride and the sibling *_menu_state_template
 * family (e.g. data_fd2_ui_player_action_menu_state_template @ 0x53F12).
 */
const int32 data_fd2_ui_save_load_menu_state_template[4] = { 0, 0, 0, 0 };

/* ----------------------------------------------------------------
 * data_fd2_ui_item_command_menu_state_template @ 0x53F32  (16 bytes)
 *
 * Per-option enable/disable flag state for the battle item command submenu
 * (Use / Give / Sort-Equip / Drop), paired with
 * data_fd2_ui_item_command_menu_template @ 0x51F05. 4 x int32 entries, all 0
 * at rest (= all four options selectable). The sole reader
 * fd2_item_command_menu_dispatch (@ 0x1BC0A) copies all four 32-bit words into
 * a local menu_state[16] buffer with a count-4 REP MOVSD (MOV ECX,4;
 * MOV ESI,0x53F32; REP MOVSD -> 32-bit elements), then mutates only that local
 * copy: when fd2_compute_aoe_targets finds no adjacent ally tile in range it
 * sets menu_state[4] = 1 (((int*)menu_state)[1] = 1, greys out the Give
 * option). The mutated copy is handed to fd2_count_active_menu_items_until_zero
 * / fd2_open_settings_dialog_with_slide / fd2_settings_menu_input_step. The
 * global source array is never written -> statically all zero. Element type
 * int32[4] is confirmed by both the dword copy stride and the consumer's
 * element-wise int32 indexing (menu_state[0..3] = template[0..3]), and matches
 * the sibling *_menu_state_template family (e.g.
 * data_fd2_ui_save_load_menu_state_template @ 0x53F22).
 */
const int32 data_fd2_ui_item_command_menu_state_template[4] = { 0, 0, 0, 0 };
