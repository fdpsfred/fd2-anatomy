/*
 * btltab.c -- battle-system read-only data tables (.object2)
 *
 * Constant scoring/weight values consumed by the enemy-turn battle AI.
 */

#include "types.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * data_fd2_battle_ai_enemy_spell_score_multiplier_15 @ 0x50144  (8 bytes, double)
 *
 * Priority-enemy spell-score multiplier (1.5). Read once by
 * fd2_score_spell_candidate: when scoring a basic damage spell (spell_id < 13)
 * against a target whose bChar_id == 0 (high-priority enemy), the per-target
 * score is loaded as an integer (FILD), multiplied by this double (FMUL qword),
 * then rounded back to int (FISTP). Read-only; no writers.
 * ---------------------------------------------------------------- */
const double data_fd2_battle_ai_enemy_spell_score_multiplier_15 = 1.5;

/* ----------------------------------------------------------------
 * data_fd2_battle_spell_ap_boost_factor_015 @ 0x50210  (8 bytes, double)
 *
 * AP-boost spell/item multiplier (0.15). Read once by
 * fd2_cast_ap_boost_spell: the buffed unit's wAP is loaded (FILD),
 * multiplied by this double (FMUL qword), and 1 is added, then the
 * result is truncated toward zero to yield the AP delta. Read-only;
 * no writers. IEEE754 0x3FC3333333333333.
 * ---------------------------------------------------------------- */
const double data_fd2_battle_spell_ap_boost_factor_015 = 0.15;

/* ----------------------------------------------------------------
 * data_fd2_battle_spell_dp_boost_factor_015 @ 0x50218  (8 bytes, double)
 *
 * DP-boost spell/item multiplier (0.15). Read once by
 * fd2_cast_dp_boost_spell: the buffed unit's wDP is loaded (FILD),
 * multiplied by this double (FMUL qword ptr [0x50218]), and 1 is added,
 * then the result is rounded to yield the DP delta. Read-only; no
 * writers (single READ xref from 0x22924). IEEE754 0x3FC3333333333333 --
 * the same 0.15 constant as the AP variant @ 0x50210 but a distinct
 * .rodata copy.
 * ---------------------------------------------------------------- */
const double data_fd2_battle_spell_dp_boost_factor_015 = 0.15;

/* ----------------------------------------------------------------
 * data_fd2_battle_tile_attr_mv_modifier_table @ 0x51A12  (24 bytes, int[6])
 *
 * Per-tile-attribute movement/AP percent modifier table. Indexed by the
 * tile attribute id (0..5) returned via fd2_read_tile_attribute_at_pos.
 * Each element is a SIGNED 32-bit percentage applied to a unit's AP
 * (and to movement cost) when it stands on that terrain, e.g.
 *   effective_AP = AP + (modifier * AP) / 100.
 *
 * Element type is signed int32: callers load it with a dword read at
 * [idx*4 + 0x51A12] and feed it to a signed IDIV (the negative entries
 * 0xFFFFFFFB = -5 must be interpreted as -5%). The sibling DEF/defense
 * table immediately follows at 0x51A2A (data_fd2_battle_tile_attr_def_
 * modifier_table), which bounds this table at exactly 6 elements.
 *
 * Read-only; no writers. Readers: fd2_execute_attack_damage_calculation,
 * fd2_ai_score_physical_attack, fd2_calculate_combat_hit_outcome,
 * fd2_render_terrain_info_hud_panel.
 *
 * Raw bytes @ 0x51A12 (LE):
 *   05 00 00 00  00 00 00 00  fb ff ff ff
 *   fb ff ff ff  fb ff ff ff  00 00 00 00
 * = { 5, 0, -5, -5, -5, 0 }
 * ---------------------------------------------------------------- */
/* Non-const: read-only in-game (no writers), but mutated by test fixtures; the
 * Watcom extern in globals.h must agree (const/non-const mismatch is E1129). */
const int32 data_fd2_battle_tile_attr_mv_modifier_table[6] = {
    5, 0, -5, -5, -5, 0
};

/* ----------------------------------------------------------------
 * data_fd2_battle_tile_attr_def_modifier_table @ 0x51A2A  (24 bytes, int[6])
 *
 * Per-tile-attribute defense percent modifier table. Sibling of the
 * movement/AP table at 0x51A12; both are int[6] and indexed by the
 * tile attribute id (0..5) returned via fd2_read_tile_attribute_at_pos.
 * Each element is a SIGNED 32-bit percentage applied to a unit's DP
 * (defense) when it stands on that terrain:
 *   effective_DP = DP + (modifier * DP) / 100.
 *
 * Element type is signed int32: callers load it with a dword read at
 * [idx*4 + 0x51A2A] (MOV EDX,dword ptr [EAX*4 + 0x51A2A]) and feed it
 * to a signed IDIV by 100 with SAR EDX,0x1F sign-extension, so the
 * negative entry 0xFFFFFFFB = -5 means a -5% defense penalty. The next
 * symbol (data_fd2_ui_click_debounce_skip_count @ 0x51A42) bounds this
 * table at exactly 6 elements / 24 bytes.
 *
 * Read-only; no writers. Readers: fd2_execute_attack_damage_calculation
 * (defender_DP terrain bonus), fd2_ai_score_physical_attack,
 * fd2_calculate_combat_hit_outcome, fd2_render_terrain_info_hud_panel
 * (DEF row via fd2_render_signed_modifier_with_icon).
 *
 * Raw bytes @ 0x51A2A (LE):
 *   00 00 00 00  00 00 00 00  0a 00 00 00
 *   0a 00 00 00  fb ff ff ff  00 00 00 00
 * = { 0, 0, 10, 10, -5, 0 }
 * ---------------------------------------------------------------- */
/* Non-const for the same reason as the MV sibling above. */
const int32 data_fd2_battle_tile_attr_def_modifier_table[6] = {
    0, 0, 10, 10, -5, 0
};

/* ----------------------------------------------------------------
 * data_fd2_battle_view_window_max_x @ 0x51A87  (4 bytes, dword scalar)
 *
 * Battle view-window maximum x extent, in map tiles (value 13). This is a
 * single scalar, NOT a table: the dword at 0x51A87 is read directly with an
 * absolute-address load (e.g. fd2_blit_animated_tile_at_pos @ 0x12AE9:
 *   MOV EAX,[0x53AA9]            ; battle_view_window_origin_x
 *   ADD EAX,dword ptr [0x51A87] ; + this scalar
 *   CMP EAX,world_x / JL ...    ; signed bound check), never base+idx*stride.
 *
 * Two uses across 15 readers:
 *   1. Visible-tile cull: a tile/sprite at world (x,y) is drawn only when
 *      origin_x-1 <= x <= origin_x + view_window_max_x (and the y sibling
 *      bound). Comparisons are signed (JG/JL), and the value is always the
 *      small positive 13, so it acts as a non-negative coordinate extent.
 *   2. Tactical-overview camera (fd2_open_tactical_overview_zoom @ 0x2007B):
 *      src_cx_fp_base = origin_x*0xC00 + view_window_max_x*0x600  (16:6 fixed
 *      point), so it is consumed as a full 32-bit integer in arithmetic.
 *
 * Read-only; no writers (all 15 xrefs are READ). The adjacent dword at
 * 0x51A8B is the sibling y extent (data_fd2_battle_view_window_max_y = 8),
 * which bounds this scalar at exactly 4 bytes.
 *
 * Raw bytes @ 0x51A87 (LE): 0d 00 00 00  = 13.
 * ---------------------------------------------------------------- */
/* Non-const: read-only in-game, but tests set the window extent as a fixture. */
const uint32 data_fd2_battle_view_window_max_x = 13;

/* ----------------------------------------------------------------
 * data_fd2_battle_view_window_max_y @ 0x51A8B  (4 bytes, dword scalar)
 *
 * Battle view-window maximum y extent, in 24x24 map-tile units (value 8).
 * Sibling of data_fd2_battle_view_window_max_x @ 0x51A87, paired 1:1 across
 * all 15 readers. A single scalar, NOT a table: the dword at 0x51A8B is read
 * directly with an absolute-address load (e.g. fd2_blit_24x24_at_window_
 * relative_pos @ 0x12736:
 *   MOV EAX,[0x53AAD]            ; battle_view_window_origin_y
 *   ADD EAX,dword ptr [0x51A8B] ; + this scalar
 *   CMP EAX,world_y / JLE ...   ; signed bound check), never base+idx*stride.
 *
 * Two uses across the 15 readers:
 *   1. Visible-tile cull: a tile/sprite at world (x,y) is drawn only when
 *      origin_y <= y < origin_y + view_window_max_y (and the x sibling
 *      bound). Comparisons are signed (JL/JLE), and the value is the small
 *      positive 8, so it acts as a non-negative coordinate extent.
 *   2. Tactical-overview camera (fd2_open_tactical_overview_zoom @ 0x200A1):
 *      src_cy_fp_base = origin_y*0xC00 + view_window_max_y*0x600  (16:6 fixed
 *      point), so it is consumed as a full 32-bit integer in arithmetic.
 *
 * Read-only; no writers (all 15 xrefs are READ). The adjacent dword at
 * 0x51A8F (data_fd2_battle_ai_post_action_consequence_idx) bounds this
 * scalar at exactly 4 bytes.
 *
 * Raw bytes @ 0x51A8B (LE): 08 00 00 00  = 8.
 * ---------------------------------------------------------------- */
/* Non-const for the same reason as the X sibling above. */
const uint32 data_fd2_battle_view_window_max_y = 8;

/* ----------------------------------------------------------------
 * data_fd2_battle_ai_post_action_consequence_table @ 0x51B91
 *     (360 bytes = 90 x 32-bit function pointers)
 *
 * Chapter-event handler dispatch table. Each slot is the entry address of
 * one fd2_chapter_event_handler_NN__* function (NN = the slot's 2-digit hex
 * index; all 90 names carry their own index, so the table is self-checking).
 * Indexed by an 8-bit event id and the selected handler is tail-called as a
 * single-argument cdecl function: the dispatch site loads the index, PUSHes one
 * arg (the active char_idx), CALLs through the table, and cleans the arg with
 * ADD ESP,4. Hence the element type is void (*)(uint32).
 *
 * The symbol is named for its primary battle-system role: the enemy-AI/turn
 * loops (fd2_enemy_turn_phase_team0/1, fd2_run_full_turn_cycle, the player
 * action menus) latch an index into the sibling scalar
 * data_fd2_battle_ai_post_action_consequence_idx @ 0x51A8F, then on the next
 * loop iteration -- if it is != 0xFF -- dispatch table[idx](char_idx) as the
 * post-action consequence (counter / death / status proc after a battle move),
 * resetting the latch to 0xFF. The same table is reused directly (no latch) by
 * the three FDFIELD/drop event paths that load the table base with a literal
 * [idx*4 + 0x51B91], the form Ghidra reports as the only three xrefs:
 *   fd2_handle_tile_event_interaction   @ 0x19511  (field-map event tile,
 *                                                   3-byte entry type "other")
 *   fd2_fire_chapter_turn_events_for_phase @ 0x1A85A (turn-gated chapter
 *                                                   scripts, entry .event_id)
 *   fd2_process_battle_drop_entries     @ 0x1AC1A  (kill-drop entry type 2,
 *                                                   "battle event")
 *
 * Read-only at runtime: no writers (all xrefs are reads/calls). The matching
 * extern in globals.h declares it non-const (void (*[90])(uint32)); kept
 * non-const here to match that contract (Layer-2 places the table via the
 * linker, no byte-exact layout needed). Targets span 0x341DB..0x360FE, the
 * contiguous block of chapter-event handlers. Several "unref_*" slots are
 * not reachable by any in-game event id but remain present in the original
 * table image and are emitted verbatim to preserve the 90-entry shape.
 *
 * Sibling dispatch tables in the same .object2 cluster:
 *   data_fd2_chapter_post_action_handler_table @ 0x51B19 (30 entries)
 *   data_fd2_battle_spell_handler_table        @ 0x51D01 (28 entries)
 * ---------------------------------------------------------------- */
void (*data_fd2_battle_ai_post_action_consequence_table[90])(uint32) = {
    fd2_chapter_event_handler_00__ch1_dialog_with_state,
    fd2_chapter_event_handler_01__ch1_dialog_with_state,
    fd2_chapter_event_handler_02__ch1_dialog_with_state,
    fd2_chapter_event_handler_03__ch1_dialog_with_state,
    fd2_chapter_event_handler_04__unref_dialog_with_state,
    fd2_chapter_event_handler_05__ch13_thunk,
    fd2_chapter_event_handler_06__ch2_reinforcement,
    fd2_chapter_event_handler_07__ch13_dialog_with_state,
    fd2_chapter_event_handler_08__ch13_first_time,
    fd2_chapter_event_handler_09__ch3_char_cond,
    fd2_chapter_event_handler_0a__ch14_first_time,
    fd2_chapter_event_handler_0b__ch4_dialog,
    fd2_chapter_event_handler_0c__unref_first_time,
    fd2_chapter_event_handler_0d__ch15_dialog_with_state,
    fd2_chapter_event_handler_0e__ch5_dialog_with_state,
    fd2_chapter_event_handler_0f__ch5_dialog_with_state,
    fd2_chapter_event_handler_10__ch5_dialog,
    fd2_chapter_event_handler_11__ch5_dialog_with_state,
    fd2_chapter_event_handler_12__ch15_dialog_with_state,
    fd2_chapter_event_handler_13__unref_char_cond,
    fd2_chapter_event_handler_14__ch6_dialog,
    fd2_chapter_event_handler_15__ch6_char_cond,
    fd2_chapter_event_handler_16__ch6_char_cond,
    fd2_chapter_event_handler_17__unref_turn_gated,
    fd2_chapter_event_handler_18__unref_dialog,
    fd2_chapter_event_handler_19__ch7_first_time,
    fd2_chapter_event_handler_1a__ch7_char_cond,
    fd2_chapter_event_handler_1b__ch8_cinematic,
    fd2_chapter_event_handler_1c__ch8_ai_ctrl,
    fd2_chapter_event_handler_1d__unref_dialog_with_state,
    fd2_chapter_event_handler_1e__unref_major_cinematic,
    fd2_chapter_event_handler_1f__ch9_reinforcement,
    fd2_chapter_event_handler_20__ch10_dialog,
    fd2_chapter_event_handler_21__ch10_dialog_with_state,
    fd2_chapter_event_handler_22__unref_dialog,
    fd2_chapter_event_handler_23__ch12_cinematic,
    fd2_chapter_event_handler_24__ch12_ai_ctrl,
    fd2_chapter_event_handler_25__unref_major_cinematic,
    fd2_chapter_event_handler_26__ch15_dialog,
    fd2_chapter_event_handler_27__unref_drop,
    fd2_chapter_event_handler_28__ch17_dialog_with_state,
    fd2_chapter_event_handler_29__unref_drop,
    fd2_chapter_event_handler_2a__ch18_dialog,
    fd2_chapter_event_handler_2b__ch18_ai_ctrl,
    fd2_chapter_event_handler_2c__ch19_ai_ctrl,
    fd2_chapter_event_handler_2d__ch19_ai_ctrl,
    fd2_chapter_event_handler_2e__ch19_reinforcement,
    fd2_chapter_event_handler_2f__ch21_turn_gated,
    fd2_chapter_event_handler_30__ch21_ai_ctrl,
    fd2_chapter_event_handler_31__ch22_turn_gated,
    fd2_chapter_event_handler_32__ch22_reinforcement,
    fd2_chapter_event_handler_33__unref_drop,
    fd2_chapter_event_handler_34__ch23_ai_ctrl,
    fd2_chapter_event_handler_35__unref_dialog_with_state,
    fd2_chapter_event_handler_36__ch24_cinematic,
    fd2_chapter_event_handler_37__ch25_first_time,
    fd2_chapter_event_handler_38__ch25_dialog_with_state,
    fd2_chapter_event_handler_39__ch26_cinematic,
    fd2_chapter_event_handler_3a__unref_pickup,
    fd2_chapter_event_handler_3b__ch26_ai_ctrl,
    fd2_chapter_event_handler_3c__ch26_ai_ctrl,
    fd2_chapter_event_handler_3d__ch26_pickup,
    fd2_chapter_event_handler_3e__ch27_dyn_turn_event,
    fd2_chapter_event_handler_3f__ch27_ai_ctrl,
    fd2_chapter_event_handler_40__unref_dyn_turn_event,
    fd2_chapter_event_handler_41__shared_dyn_turn_event,
    fd2_chapter_event_handler_42__ch28_dialog_with_state,
    fd2_chapter_event_handler_43__unref_dyn_turn_event,
    fd2_chapter_event_handler_44__ch28_dialog_with_state,
    fd2_chapter_event_handler_45__ch28_dyn_turn_event,
    fd2_chapter_event_handler_46__ch28_dialog_with_state,
    fd2_chapter_event_handler_47__unref_dyn_turn_event,
    fd2_chapter_event_handler_48__unref_ai_ctrl,
    fd2_chapter_event_handler_49__unref_sentinel,
    fd2_chapter_event_handler_4a__ch29_dyn_turn_event,
    fd2_chapter_event_handler_4b__ch29_major_cinematic,
    fd2_chapter_event_handler_4c__ch29_major_cinematic,
    fd2_chapter_event_handler_4d__unref_sentinel,
    fd2_chapter_event_handler_4e__unref_sentinel,
    fd2_chapter_event_handler_4f__ch29_dyn_turn_event,
    fd2_chapter_event_handler_50__ch30_ai_ctrl,
    fd2_chapter_event_handler_51__unref_dyn_turn_event,
    fd2_chapter_event_handler_52__ch30_major_cinematic,
    fd2_chapter_event_handler_53__unref_dialog_with_state,
    fd2_chapter_event_handler_54__ch27_ai_ctrl,
    fd2_chapter_event_handler_55__unref_sentinel,
    fd2_chapter_event_handler_56__unref_sentinel,
    fd2_chapter_event_handler_57__unref_sentinel,
    fd2_chapter_event_handler_58__unref_sentinel,
    fd2_chapter_event_handler_59__unref_sentinel
};

/* ----------------------------------------------------------------
 * data_fd2_battle_spell_handler_table @ 0x51D01  (112 bytes = 28 x 32-bit fn ptr)
 *
 * Special-spell dispatch table, indexed by spell_id (0x00..0x1B). Each slot is
 * the entry address of one spell-worker function; the selected handler is
 * called as a cdecl 3-arg function (caster_unit_id, n_targets, target_id_array)
 * with the arguments cleaned by ADD ESP,0xC. The dispatch site loads the spell
 * id, scales by 4 and calls through the table:
 *   fd2_execute_ai_offensive_spell @ 0x1541F:
 *     MOV  EAX,[0x53C2F]                 ; active_spell_id
 *     CALL dword ptr [EAX*0x4 + 0x51D01] ; table[spell_id](caster,n_tgt,tgt_buf)
 *   fd2_spell_selection_menu_main  @ 0x1D479: same form, indexed by the
 *     player-selected spell id (spell_id_list[cursor]).
 * Hence stride is 4 and the element type is void (*)(uint32,uint32,uint8 *).
 *
 * The two readers only reach this table on the status/special branch (spell id
 * >= 9 and not the plain cast-sequence ids); the basic damage/heal spells take
 * fd2_play_spell_cast_sequence instead. Several handlers' Ghidra prototypes
 * differ cosmetically (e.g. fd2_cast_spell_17_complex's 3rd param shows as
 * uint32, fd2_spell_handler_id_26/27's as int), but the binary calling
 * convention is uniform: 3 dword args pushed as (caster_unit_id, n_targets,
 * target_id_array_ptr). Each initializer is therefore cast to the table's
 * element type to match the proven ABI.
 *
 * Note the table is not byte-injective: entry 0x10 and entry 0x18 both point at
 * fd2_cast_spell_10_variant_b @ 0x22153 (spell 0x18 reuses the 0x10 worker).
 *
 * Read-only at runtime: no writers (both xrefs are CALLs through the table).
 * Kept non-const to match the void (*[28])(...) extern in globals.h (Layer-2
 * places the table via the linker; no byte-exact layout needed). Sibling
 * dispatch tables in the same .object2 cluster:
 *   data_fd2_chapter_post_action_handler_table  @ 0x51B19 (30 entries)
 *   data_fd2_battle_ai_post_action_consequence_table @ 0x51B91 (90 entries)
 *
 * Memory image @ 0x51D01 (28 LE dwords, each an entry-point address):
 *   00021206 0002134B 00021364 0002137D 00021396 00021449 00021462 0002147B
 *   00021494 000214AD 00021527 0002185F 00021A9E 00021AD9 00021B99 0002211C
 *   00022153 000226EA 0002282F 00022960 00022A85 00022BC6 00022BE1 0002218A
 *   00022153 00022C04 00022CBF 00022E41
 * ---------------------------------------------------------------- */
void (*data_fd2_battle_spell_handler_table[28])(uint32, uint32, uint8 *) = {
    /* 0x00 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_0_via_targeted_blink,
    /* 0x01 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_1_via_targeted_blink,
    /* 0x02 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_2_via_targeted_blink,
    /* 0x03 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_3_via_targeted_blink,
    /* 0x04 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_4_via_full_screen_flash,
    /* 0x05 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_5_via_full_screen_flash,
    /* 0x06 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_6_via_full_screen_flash,
    /* 0x07 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_7_via_full_screen_flash,
    /* 0x08 */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_8_via_targeted_blink,
    /* 0x09 */ (void (*)(uint32, uint32, uint8 *)) fd2_execute_offensive_single_target_spell_id_9,
    /* 0x0a */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_0a_basic,
    /* 0x0b */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_0b_with_prefx,
    /* 0x0c */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_0c_with_prefx,
    /* 0x0d */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_0d_variant_b,
    /* 0x0e */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_0e_variant_b,
    /* 0x0f */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_0f_variant_b,
    /* 0x10 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_10_variant_b,
    /* 0x11 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_11_stage_a,
    /* 0x12 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_12_stage_b,
    /* 0x13 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_13_stage_c,
    /* 0x14 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_14_dispatch_aa8,
    /* 0x15 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_15_dispatch_aa8,
    /* 0x16 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_16_dispatch_cda,
    /* 0x17 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_17_complex,
    /* 0x18 */ (void (*)(uint32, uint32, uint8 *)) fd2_cast_spell_10_variant_b,
    /* 0x19 */ (void (*)(uint32, uint32, uint8 *)) fd2_execute_status_clear_holy_word_spell_id_25,
    /* 0x1a */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_26_via_status_d1b_effect_25,
    /* 0x1b */ (void (*)(uint32, uint32, uint8 *)) fd2_spell_handler_id_27_via_status_d1b_effect_26
};

/* ----------------------------------------------------------------
 * data_fd2_battle_job_magic_resist_table @ 0x51F96  (112 bytes, uint32[28])
 *
 * Per-job magic-damage scale factor (read-only). Indexed by job_id, which is
 * 1-based, so the accessor uses (job_id - 1). The value is the fraction of
 * spell power the defender's job takes, in tenths: magic damage applied to a
 * defender is (s32)(spell_base_power * resist_value) / 10. A value of 10 means
 * full damage (no resistance); lower values resist more, so the in-game magic
 * resistance is (10 - resist_value) * 10 percent (e.g. job 0x05 法師 -> 7 ->
 * 30% resist; job 0x0D 大法師 -> 5 -> 50% resist; job 0x1A -> 4 -> 60% resist).
 *
 * Sole consumer fd2_calc_magic_damage @ 0x1C75E:
 *     MOV ECX,0x1C ; MOV EDI,ESP ; MOV ESI,0x51F96 ; REP MOVSD
 *         -> bulk-copies 28 dwords (112 bytes) into a stack scratch buffer
 *     IMUL EDX,[ESP + ESI*4 - 4]   ; ESI = bJob_id
 *         -> stride 4, element = uint32, index = bJob_id - 1
 *
 * Extent is 28 dwords: the REP MOVSD count is 28 and the data region runs
 * [0x51F96, 0x52006) (112 bytes); the next table (consumed by
 * fd2_animate_spell_overlay_blink) begins at 0x52006. Entries 0..25 map to the
 * 26 defined non-dragon jobs (job_id 0x01..0x1A -> index 0..25); job 0x00 (the
 * dragon) has no entry and is never looked up. Indices 26..27 are trailing
 * dwords the bulk copy also pulls in (not addressed by any defined job_id).
 * Values are small positive scale factors (4..10). No writers.
 * ---------------------------------------------------------------- */
const uint32 data_fd2_battle_job_magic_resist_table[28] = {
    /* job 0x01 */ 10, /* job 0x02 */ 10, /* job 0x03 */ 10, /* job 0x04 */ 10,
    /* job 0x05 */  7, /* job 0x06 */  7, /* job 0x07 */ 10, /* job 0x08 */ 10,
    /* job 0x09 */ 10, /* job 0x0a */ 10, /* job 0x0b */  9, /* job 0x0c */ 10,
    /* job 0x0d */  5, /* job 0x0e */  5, /* job 0x0f */  8, /* job 0x10 */ 10,
    /* job 0x11 */  6, /* job 0x12 */  8, /* job 0x13 */ 10, /* job 0x14 */  9,
    /* job 0x15 */  5, /* job 0x16 */  5, /* job 0x17 */ 10, /* job 0x18 */  8,
    /* job 0x19 */  8, /* job 0x1a */  4, /* idx 26   */ 10, /* idx 27   */  7
};

/* ----------------------------------------------------------------
 * data_fd2_battle_damage_number_format_buffer @ 0x52045  (5 bytes, byte[5])
 *
 * Pre-fill template "    \0" (4 spaces + NUL) for the floating damage-number
 * work buffer. NOT a printf format string.
 *
 * Sole consumer fd2_show_damage_number @ 0x1E0DB:
 *     MOV ESI,0x52045 ; MOVSD ; MOVSB
 *         -> copies exactly 5 bytes (4 spaces + NUL) into an 8-byte stack
 *            scratch buffer, then sprintf("%d") overwrites it digit by digit.
 * The C consumer (fd2_show_damage_number) mirrors this with memcpy(.., 5).
 *
 * Extent is 5 bytes, not 8: only MOVSD (4) + MOVSB (1) are read. Ghidra
 * declares byte[8], but bytes 5..7 in that view are the first three bytes of
 * the physically adjacent data_fd2_battle_miss_indicator_sprite_ids @ 0x5204A,
 * which is a separate symbol and is never read through this buffer.
 * Read-only; no writers.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_battle_damage_number_format_buffer[5] = {
    0x20, 0x20, 0x20, 0x20, 0x00
};

/* ----------------------------------------------------------------
 * data_fd2_battle_miss_indicator_sprite_ids @ 0x5204A  (4 bytes, byte[4])
 *
 * The 4 text-glyph sprite ids for the "MISS"/"閃避" floating indicator,
 * one per the 4 indicator frames enqueued above the dodging target.
 *
 * Sole consumer fd2_show_miss_indicator @ 0x1E1DC:
 *     MOV EAX,[0x5204A] ; MOV [ESP],EAX        ; load all 4 bytes as one dword
 *     ... loop char_iter 0..3:
 *       MOV BL,byte ptr [ESP + char_iter]      ; index the dword copy per byte
 *       MOV [.. sprite_id_queue ..],BL         ; enqueue one sprite id per frame
 * The dword is loaded once into a stack scratch, then read byte-by-byte; the
 * destination sprite_id queue is a byte array, so each element is a uint8.
 *
 * Physically adjacent to data_fd2_battle_damage_number_format_buffer @ 0x52045
 * (which ends at 0x5204A) but a separate symbol with its own sole reader.
 * Read-only; no writers.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_battle_miss_indicator_sprite_ids[4] = {
    0x74, 0x75, 0x76, 0x76
};

/* ----------------------------------------------------------------
 * data_fd2_battle_job_crit_rate_table @ 0x5239B  (27 bytes, uint8[27])
 *
 * Per-job base critical-hit rate, in percent (read-only). Indexed by job_id,
 * which is 1-based, so the accessor uses (job_id - 1); element 0 is job 0x01.
 * Job 0x00 (the dragon) has no crit entry and is never looked up.
 *
 * Consumers fd2_execute_attack_damage_calculation @ 0x1EE08 and
 * fd2_calculate_combat_hit_outcome @ 0x2A0E1:
 *     MOV EAX,[job_id_minus_1] ; MOVZX EAX,byte ptr [EAX + 0x5239B]
 *         -> stride 1, element = uint8, zero-extended (unsigned), index =
 *            bJob_id - 1. The value seeds total_crit_pct for the crit roll.
 *
 * Extent is 27 bytes: entries 0..25 map to jobs 0x01..0x1A (the 26 defined
 * jobs); byte 26 is a trailing 0 pad. The table sits immediately after the
 * "TAI.DAT" string (ends at 0x5239B) and before zero padding at 0x523B2.
 * No writers.
 * ---------------------------------------------------------------- */
/* Non-const: read-only in-game, but seeded by test fixtures. */
const uint8 data_fd2_battle_job_crit_rate_table[27] = {
    /* job 0x01 */  5, /* job 0x02 */  3, /* job 0x03 */  3, /* job 0x04 */  5,
    /* job 0x05 */  3, /* job 0x06 */  3, /* job 0x07 */  0, /* job 0x08 */ 18,
    /* job 0x09 */  5, /* job 0x0a */  3, /* job 0x0b */  3, /* job 0x0c */ 12,
    /* job 0x0d */  3, /* job 0x0e */  3, /* job 0x0f */ 12, /* job 0x10 */ 10,
    /* job 0x11 */  6, /* job 0x12 */  3, /* job 0x13 */  3, /* job 0x14 */  7,
    /* job 0x15 */  3, /* job 0x16 */  3, /* job 0x17 */ 30, /* job 0x18 */ 18,
    /* job 0x19 */  0, /* job 0x1a */  0, /* idx 26   */  0
};

/* ----------------------------------------------------------------
 * data_fd2_battle_spell_cast_cinematic_phase_handler_table @ 0x523B9
 *     (40 bytes = 10 x 32-bit function pointers)
 *
 * Summon-spell tick / cinematic-phase dispatch table. NOT a flat byte/const
 * table (the worklist's "const, needs_bytes" classification is wrong): each
 * 4-byte slot is the entry address of one fd2_tick_summon_* (or render)
 * function, and the slot is invoked through the pointer. Confirmed by Ghidra xrefs-from
 * (slot 0 -> fd2_tick_summon_spell_setup_pre_animation_8slot, etc.) and by the
 * call site disassembly.
 *
 * Indexed by spell_id and called as a 5-arg cdecl returning an int frame count.
 * Dispatch site fd2_play_spell_cast_sequence @ 0x2AC25:
 *     MOV  EAX,EBP                          ; EBP = spell_id
 *     PUSH 0x0                              ; arg5 phase/state code
 *     PUSH 0x140                            ; arg4 row stride
 *     PUSH ESI                              ; arg3 dst work buffer
 *     PUSH [ESP+0x120]                      ; arg2 caster sprite handle
 *     PUSH [ESP+0x154]                      ; arg1 caster unit id
 *     CALL dword ptr [EAX*0x4 + 0x523B9]    ; table[spell_id](...)
 *     ADD  ESP,0x14                         ; cdecl cleanup of 5 dword args
 *     MOV  [ESP+0xF8],EAX                   ; EAX = returned frame count
 * Hence stride is 4 and the element type is int (*)(uint32,uint32,uint32,
 * uint32,uint32). Every entry returns the per-phase frame count that drives
 * the surrounding cinematic blit loop.
 *
 * Readers: fd2_play_spell_cast_sequence @ 0x2A6BD (10 call sites, one per
 * cinematic phase) and fd2_animate_spell_hit_cinematic @ 0x2BA22 (4 sites).
 * Read-only at runtime: no writers (all xrefs are CALLs through the table).
 *
 * The constituent handlers' Ghidra prototypes differ cosmetically (slot 1
 * fd2_render_summon_aura_sprite_ring shows (int,int,int,int,char); the other
 * nine show (uint32 x5)) but the binary calling convention is uniform -- five
 * dword args pushed, int returned. Each initializer is cast to the table's
 * element type to match that proven ABI, exactly as the sibling
 * data_fd2_battle_spell_handler_table @ 0x51D01 does.
 *
 * Memory image @ 0x523B9 (10 LE dwords, each an entry-point address):
 *   00026152 000262EF 00026528 00026795 000269D3
 *   00026BFD 00026E39 000272B8 000274B0 000275D6
 * ---------------------------------------------------------------- */
int (*data_fd2_battle_spell_cast_cinematic_phase_handler_table[10])(
        uint32, uint32, uint32, uint32, uint32) = {
    /* 0 0x26152 */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_spell_setup_pre_animation_8slot,
    /* 1 0x262EF */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_render_summon_aura_sprite_ring,
    /* 2 0x26528 */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_spell_animation_state,
    /* 3 0x26795 */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_spell_main_animation_state,
    /* 4 0x269D3 */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_anim_variant_a_6slot,
    /* 5 0x26BFD */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_anim_variant_b_6slot,
    /* 6 0x26E39 */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_anim_variant_c_5slot_radial,
    /* 7 0x272B8 */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_anim_variant_d_3slot,
    /* 8 0x274B0 */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_anim_variant_e_16slot,
    /* 9 0x275D6 */ (int (*)(uint32, uint32, uint32, uint32, uint32)) fd2_tick_summon_spell_minor_animation_state
};

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_spell_8slot_visibility_table @ 0x523E1  (7 bytes, uint8[7])
 *
 * Per-slot visibility/enable mask for the summon-spell stage-0 orbit setup,
 * read-only. Consumed by fd2_tick_summon_spell_setup_pre_animation_8slot
 * @ 0x26152, which copies all 7 bytes into a stack buffer (LEA EDI,[ESP+0x38];
 * MOV ESI,0x523E1; MOVSD/MOVSW/MOVSB) and then reads them back per slot:
 *     MOVZX EDX,byte ptr [ESP + EBX*0x1 + 0x38]   ; stride 1, unsigned byte,
 *                                                   index = slot (0..6)
 *   - state 4 (BLIT-IF-VISIBLE): slot drawn when mask == 1 (CMP EDX,1)
 *   - state 5 (BLIT-IF-HIDDEN):  slot drawn when mask == 0 (TEST EDX,EDX)
 * So mask == 1 selects the "front" group revealed in the visible pass and
 * hidden in the inverse pass; mask == 0 is the opposite. Element type is uint8
 * (MOVZX = zero-extended). No writers.
 *
 * Memory image @ 0x523E1: 00 00 01 00 01 00 00  (slots 2 and 4 are the
 * visible-pass group). Sits immediately before the 7-entry u32 y-offset table
 * at 0x523E8.
 * ---------------------------------------------------------------- */
/* Non-const: read-only in-game, but mutated by anisumm1 test fixtures. */
const uint8 data_fd2_battle_summon_spell_8slot_visibility_table[7] = {
    /* slot 0 */ 0x00, /* slot 1 */ 0x00, /* slot 2 */ 0x01, /* slot 3 */ 0x00,
    /* slot 4 */ 0x01, /* slot 5 */ 0x00, /* slot 6 */ 0x00
};

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_spell_8slot_y_offset_table @ 0x523E8  (28 bytes, uint32[7])
 *
 * Per-slot baseline vertical (Y) pixel offset for the summon-spell stage-0
 * orbit setup, read-only. Consumed by
 * fd2_tick_summon_spell_setup_pre_animation_8slot @ 0x26152, which copies all
 * 7 dwords into a stack buffer:
 *     MOV ECX,0x7; LEA EDI,[ESP+0x1c]; MOV ESI,0x523E8; REP MOVSD
 *   (REP MOVSD x7 = 7 elements * 4 bytes = 28 bytes; element width = 4)
 * then, for the enemy team (runtime_char[caster].bTeam == 0), biases every
 * slot down by 0x94 px with a dword-stride accumulate:
 *     ADD dword ptr [ESP+EBX*0x4+0x1c],0x94   ; EBX = slot (0..6), stride 4
 * and finally feeds each element into the per-slot blit Y coordinate as
 *     y = row_multiplier[slot]*row_stride + origin_y + y_offset[slot].
 * Element type is uint32 (32-bit copy/accumulate via MOVSD/ADD dword); the
 * values are small positive offsets added into a signed int Y. No writers.
 *
 * Memory image @ 0x523E8 (little-endian dwords):
 *   28 00 00 00  46 00 00 00  78 00 00 00  50 00 00 00
 *   32 00 00 00  64 00 00 00  46 00 00 00
 *   = { 0x28, 0x46, 0x78, 0x50, 0x32, 0x64, 0x46 }
 *   = {   40,   70,  120,   80,   50,  100,   70 }
 * Sits immediately between the 7-byte visibility mask at 0x523E1 and the
 * 7-entry i32 row-multiplier table at 0x52404.
 * ---------------------------------------------------------------- */
/* Non-const for the same reason as the visibility sibling above. */
const uint32 data_fd2_battle_summon_spell_8slot_y_offset_table[7] = {
    /* slot 0 */ 0x28, /* slot 1 */ 0x46, /* slot 2 */ 0x78, /* slot 3 */ 0x50,
    /* slot 4 */ 0x32, /* slot 5 */ 0x64, /* slot 6 */ 0x46
};

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_spell_8slot_row_multiplier_table @ 0x52404
 *     (28 bytes, int32[7])
 *
 * Per-slot row-stride multiplier for the summon-spell stage-0 orbit setup,
 * read-only. Sibling of the visibility mask @ 0x523E1 and the y-offset table
 * @ 0x523E8; together they drive the per-slot blit Y coordinate:
 *     y = row_multiplier[slot] * row_stride + origin_y + y_offset[slot].
 *
 * Consumed by fd2_tick_summon_spell_setup_pre_animation_8slot @ 0x26152,
 * which bulk-copies all 7 dwords into a stack buffer:
 *     MOV ECX,0x7; MOV EDI,ESP; MOV ESI,0x52404; REP MOVSD
 *   (REP MOVSD x7 = 7 elements * 4 bytes = 28 bytes; element width = 4)
 * then feeds each element into the blit Y math with a SIGNED multiply:
 *     MOV EDX,[ESP + slot*4 + 0x8]   ; row_multiplier[slot] (dword)
 *     IMUL EDX,EBP                    ; * row_stride  (IMUL = signed)
 *     ADD  EDX,ECX                    ; + origin_y + y_offset[slot]
 *
 * Element type is signed int32: the values are loaded as 32-bit dwords and
 * IMUL'd (signed) by row_stride; the negative entries (0xFFFFFFF6 = -10,
 * 0xFFFFFFEC = -20, 0xFFFFFFF1 = -15, 0xFFFFFFFB = -5) shift those slots
 * upward by N*row_stride pixels, while the 0 entries leave the slot on the
 * baseline. No writers (single READ/DATA xref pair from the copy at 0x26193).
 *
 * Memory image @ 0x52404 (little-endian dwords):
 *   00 00 00 00  f6 ff ff ff  ec ff ff ff  00 00 00 00
 *   f1 ff ff ff  fb ff ff ff  00 00 00 00
 *   = { 0, -10, -20, 0, -15, -5, 0 }
 * ---------------------------------------------------------------- */
/* Non-const for the same reason as the visibility sibling above. */
const int32 data_fd2_battle_summon_spell_8slot_row_multiplier_table[7] = {
    /* slot 0 */ 0, /* slot 1 */ -10, /* slot 2 */ -20, /* slot 3 */ 0,
    /* slot 4 */ -15, /* slot 5 */ -5, /* slot 6 */ 0
};

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_aura_ring_8slot_x_offset_table @ 0x52420  (32 bytes, 8x int32)
 *
 * Per-slot horizontal x-offset for the 8-directional summon-spell aura
 * sprite ring. Read-only; single READ/DATA xref pair from the prologue
 * copy in fd2_render_summon_aura_sprite_ring @ 0x262EF:
 *
 *     MOV  ECX,0x8                     ; 8 dwords
 *     MOV  ESI,0x52420                 ; source = this table
 *     MOVSD.REP ES:EDI,ESI             ; copy 32 bytes to a local int[8]
 *
 * The renderer copies the table to a stack-local int[8], then for the
 * enemy team adds 0x94 to each slot (signed dword ADD), and forms each
 * sprite position as  row_multiplier[slot]*row_stride + x_offset[slot]
 * + 0x50 + origin_y.  Element type is signed int32: entries are loaded
 * as 32-bit dwords and added signed; the negative entries
 * (0xFFFFFFC5 = -59, 0xFFFFFFD9 = -39) shift those slots left.
 * No writers (renderer mutates only its local copy, never this table).
 *
 * Memory image @ 0x52420 (little-endian dwords):
 *   c5 ff ff ff  d9 ff ff ff  00 00 00 00  27 00 00 00
 *   37 00 00 00  27 00 00 00  00 00 00 00  d9 ff ff ff
 *   = { -59, -39, 0, 39, 55, 39, 0, -39 }
 * ---------------------------------------------------------------- */
const int32 data_fd2_battle_summon_aura_ring_8slot_x_offset_table[8] = {
    /* slot 0 */ -59, /* slot 1 */ -39, /* slot 2 */   0, /* slot 3 */ 39,
    /* slot 4 */  55, /* slot 5 */  39, /* slot 6 */   0, /* slot 7 */ -39
};
