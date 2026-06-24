/*
 * chtab.c -- chapter dispatch read-only function-pointer tables (.object2)
 *
 * Per-chapter handler jump tables indexed by chapter id (index 0 = chapter 1
 * .. index 29 = chapter 30). The pointed-to handler functions live in
 * src/field/chpost.c (post-action handlers); their prototypes are in protos.h.
 */

#include "types.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * data_fd2_chapter_post_action_handler_table @ 0x51B19  (30 entries, 4-byte ptrs)
 *
 * Per-chapter "post-action" handler, invoked once after each player unit
 * finishes acting. Indexed by current_chapter_id (index 0 = chapter 1).
 * Read-only const table in .object2.
 *
 * Caller (fd2_game_main_loop @ 0x1d89a and 3 sibling turn-phase callers):
 *     MOV  EAX,[0x53C03]                 ; current_chapter_id
 *     PUSH ESI                           ; arg = active char index (event_arg)
 *     CALL dword ptr [EAX*0x4 + 0x51B19] ; stride 4, call thru fn ptr
 *     ADD  ESP,0x4                       ; cdecl, single uint32 arg
 * => element type: void (*)(uint32), 30 entries, cdecl, indexed by chapter-1.
 *
 * Chapters with no special post-action point at the shared default
 * fd2_check_battle_end_default_handler. Chapters 22/27/28 share one handler.
 * ---------------------------------------------------------------- */
void (*const data_fd2_chapter_post_action_handler_table[30])(uint32) = {
    fd2_check_battle_end_default_handler,        /* [0]  ch1  default */
    fd2_chapter_02_post_action,                  /* [1]  ch2  */
    fd2_check_battle_end_default_handler,        /* [2]  ch3  default */
    fd2_check_battle_end_default_handler,        /* [3]  ch4  default */
    fd2_check_battle_end_default_handler,        /* [4]  ch5  default */
    fd2_check_battle_end_default_handler,        /* [5]  ch6  default */
    fd2_check_battle_end_default_handler,        /* [6]  ch7  default */
    fd2_check_battle_end_default_handler,        /* [7]  ch8  default */
    fd2_check_battle_end_default_handler,        /* [8]  ch9  default */
    fd2_chapter_10_post_action,                  /* [9]  ch10 */
    fd2_check_battle_end_default_handler,        /* [10] ch11 default */
    fd2_chapter_12_post_action,                  /* [11] ch12 */
    fd2_chapter_13_post_action,                  /* [12] ch13 */
    fd2_check_battle_end_default_handler,        /* [13] ch14 default */
    fd2_chapter_15_post_action,                  /* [14] ch15 */
    fd2_chapter_16_post_action,                  /* [15] ch16 */
    fd2_chapter_17_post_action,                  /* [16] ch17 */
    fd2_chapter_18_post_action,                  /* [17] ch18 */
    fd2_chapter_19_post_action,                  /* [18] ch19 */
    fd2_chapter_20_post_action,                  /* [19] ch20 */
    fd2_chapter_21_post_action,                  /* [20] ch21 */
    fd2_chapter_22_27_28_post_action_shared,     /* [21] ch22 shared */
    fd2_chapter_23_post_action,                  /* [22] ch23 */
    fd2_check_battle_end_default_handler,        /* [23] ch24 default */
    fd2_chapter_25_post_action,                  /* [24] ch25 */
    fd2_chapter_26_post_action,                  /* [25] ch26 */
    fd2_chapter_22_27_28_post_action_shared,     /* [26] ch27 shared */
    fd2_chapter_22_27_28_post_action_shared,     /* [27] ch28 shared */
    fd2_chapter_29_post_action,                  /* [28] ch29 */
    fd2_chapter_30_post_action                   /* [29] ch30 */
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_init_handler_table @ 0x51D71  (30 entries, 4-byte ptrs)
 *
 * Per-chapter init handler, invoked once whenever a chapter starts: on NEW
 * GAME, after a save slot is loaded (CONTINUE), and on each in-game chapter
 * switch. Indexed by current_chapter_id (index 0 = chapter 1). Read-only
 * const table in .object2.
 *
 * Two readers, both using the same call pattern (no writers):
 *   fd2_main_menu_dispatcher @ 0x25F10 (NEW GAME) and @ 0x260F5
 *     (CONTINUE, after committing a save slot + chapter intro)
 *   main @ 0x25BF4, chapter-switch branch (game_event_flag == 2): runs the
 *     chapter-end handler, then this init handler for the next chapter
 *     MOV  EAX,[0x53C03]                 ; current_chapter_id
 *     CALL dword ptr [EAX*0x4 + 0x51D71] ; stride 4, call thru fn ptr, no args
 * => element type: void (*)(void), 30 entries, cdecl, indexed by chapter-1.
 *
 * Chapters 19/20/21 share one init handler (fd2_chapter_19_20_21_init_shared).
 * ---------------------------------------------------------------- */
void (*const data_fd2_chapter_init_handler_table[30])(void) = {
    fd2_chapter_01_init,                 /* [0]  ch1  */
    fd2_chapter_02_init,                 /* [1]  ch2  */
    fd2_chapter_03_init,                 /* [2]  ch3  */
    fd2_chapter_04_init,                 /* [3]  ch4  */
    fd2_chapter_05_init,                 /* [4]  ch5  */
    fd2_chapter_06_init,                 /* [5]  ch6  */
    fd2_chapter_07_init,                 /* [6]  ch7  */
    fd2_chapter_08_init,                 /* [7]  ch8  */
    fd2_chapter_09_init,                 /* [8]  ch9  */
    fd2_chapter_10_init,                 /* [9]  ch10 */
    fd2_chapter_11_init,                 /* [10] ch11 */
    fd2_chapter_12_init,                 /* [11] ch12 */
    fd2_chapter_13_init,                 /* [12] ch13 */
    fd2_chapter_14_init,                 /* [13] ch14 */
    fd2_chapter_15_init,                 /* [14] ch15 */
    fd2_chapter_16_init,                 /* [15] ch16 */
    fd2_chapter_17_init,                 /* [16] ch17 */
    fd2_chapter_18_init,                 /* [17] ch18 */
    fd2_chapter_19_20_21_init_shared,    /* [18] ch19 shared */
    fd2_chapter_19_20_21_init_shared,    /* [19] ch20 shared */
    fd2_chapter_19_20_21_init_shared,    /* [20] ch21 shared */
    fd2_chapter_22_init,                 /* [21] ch22 */
    fd2_chapter_23_init,                 /* [22] ch23 */
    fd2_chapter_24_init,                 /* [23] ch24 */
    fd2_chapter_25_init,                 /* [24] ch25 */
    fd2_chapter_26_init,                 /* [25] ch26 */
    fd2_chapter_27_init,                 /* [26] ch27 */
    fd2_chapter_28_init,                 /* [27] ch28 */
    fd2_chapter_29_init,                 /* [28] ch29 */
    fd2_chapter_30_init                  /* [29] ch30 */
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_end_handler_table @ 0x51DE9  (30 entries, 4-byte ptrs)
 *
 * Per-chapter end handler, invoked once when a chapter is being switched out
 * (game_event_flag == 2: chapter-switch branch of the main loop). Indexed by
 * current_chapter_id (index 0 = chapter 1 .. index 29 = chapter 30).
 * Read-only const table in .object2. The pointed-to handlers live in
 * src/field/chend1.c / chend2.c; prototypes are in protos.h.
 *
 * Caller (main @ 0x25BF4, chapter-switch branch @ 0x25E23):
 *     MOV  EAX,[0x53C03]                 ; current_chapter_id
 *     CALL dword ptr [EAX*0x4 + 0x51DE9] ; stride 4, call thru fn ptr, no args
 *     ; (cdecl, no stack cleanup -> zero-arg, void return)
 * => element type: void (*)(void), 30 entries, cdecl, indexed by chapter-1.
 *
 * Unlike the post-action / init tables, every chapter has its own distinct end
 * handler (no shared / default slots); entries are strictly sequential
 * fd2_chapter_01_end .. fd2_chapter_30_end.
 * ---------------------------------------------------------------- */
void (*const data_fd2_chapter_end_handler_table[30])(void) = {
    fd2_chapter_01_end,                  /* [0]  ch1  */
    fd2_chapter_02_end,                  /* [1]  ch2  */
    fd2_chapter_03_end,                  /* [2]  ch3  */
    fd2_chapter_04_end,                  /* [3]  ch4  */
    fd2_chapter_05_end,                  /* [4]  ch5  */
    fd2_chapter_06_end,                  /* [5]  ch6  */
    fd2_chapter_07_end,                  /* [6]  ch7  */
    fd2_chapter_08_end,                  /* [7]  ch8  */
    fd2_chapter_09_end,                  /* [8]  ch9  */
    fd2_chapter_10_end,                  /* [9]  ch10 */
    fd2_chapter_11_end,                  /* [10] ch11 */
    fd2_chapter_12_end,                  /* [11] ch12 */
    fd2_chapter_13_end,                  /* [12] ch13 */
    fd2_chapter_14_end,                  /* [13] ch14 */
    fd2_chapter_15_end,                  /* [14] ch15 */
    fd2_chapter_16_end,                  /* [15] ch16 */
    fd2_chapter_17_end,                  /* [16] ch17 */
    fd2_chapter_18_end,                  /* [17] ch18 */
    fd2_chapter_19_end,                  /* [18] ch19 */
    fd2_chapter_20_end,                  /* [19] ch20 */
    fd2_chapter_21_end,                  /* [20] ch21 */
    fd2_chapter_22_end,                  /* [21] ch22 */
    fd2_chapter_23_end,                  /* [22] ch23 */
    fd2_chapter_24_end,                  /* [23] ch24 */
    fd2_chapter_25_end,                  /* [24] ch25 */
    fd2_chapter_26_end,                  /* [25] ch26 */
    fd2_chapter_27_end,                  /* [26] ch27 */
    fd2_chapter_28_end,                  /* [27] ch28 */
    fd2_chapter_29_end,                  /* [28] ch29 */
    fd2_chapter_30_end                   /* [29] ch30 */
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ending_music_trigger_frames @ 0x5204E  (15 entries, 4-byte int)
 *
 * Scripted scroll-row thresholds that drive the GAME-CLEAR credit-roll SFX /
 * palette swaps. During the countdown loop the credit panel scrolls from row
 * 0x217 down to 0; whenever the current row equals the next unconsumed entry
 * of this table, the ending SFX is fired and the palette is swapped, then the
 * music index advances to the next entry. Read-only const table in .object2.
 *
 * Caller (fd2_title_attract_and_main_menu @ 0x1F894):
 *     int *piVar3 = data_fd2_chapter_ending_music_trigger_frames;
 *     for (n = 15; n != 0; n--) { *(int *)dst = *piVar3; piVar3++; dst += 4; }
 *     ...
 *     if (iVar4 == *(int *)(stack_copy + music_idx * 4)) { ... music_idx++; }
 * => element type: signed int (32-bit), stride 4, 15 entries; values are
 *    descending scroll-row indices (520,430,410,...,22) plus a trailing 1000
 *    sentinel, compared against the signed scroll countdown iVar4.
 *
 * No writers: the table is only copied (read) onto the caller's stack.
 * ---------------------------------------------------------------- */
const int32 data_fd2_chapter_ending_music_trigger_frames[15] = {
    0x208, 0x1AE, 0x19A, 0x154, 0x136, 0x12C, 0xF0, 0xB4,
    0x96,  0x82,  0x6E,  0x57,  0x40,  0x16,  0x3E8
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch03_end_scene_char_pos_x_table @ 0x520BA  (7 bytes)
 *
 * Chapter 3 end-scene character placement: per-character X tile coordinate
 * for the 7 characters staged in the chapter-3 recruit cutscene. First of
 * three parallel 7-byte tables (X @ 0x520BA, Y @ 0x520C1, facing @ 0x520C8).
 * Read-only const table in .object2.
 *
 * Caller (fd2_chapter_03_end @ 0x230F2):
 *     MOV  ESI,0x520BA
 *     LEA  EDI,[ESP+0x10]
 *     MOVSD ; MOVSW ; MOVSB        ; copy 7 raw bytes onto stack scene block
 * => the table is block-copied (4+2+1 = 7 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (recruit_block_a._0_1_ .. [6]). No struct stride, no wider
 *    element access.
 * => element type: uint8, 7 entries, read-only (single READ xref, no writer).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch03_end_scene_char_pos_x_table[7] = {
    8, 7, 9, 6, 10, 8, 8
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch03_end_scene_char_pos_y_table @ 0x520C1  (7 bytes)
 *
 * Chapter 3 end-scene character placement: per-character Y tile coordinate
 * for the 7 characters staged in the chapter-3 recruit cutscene. Second of
 * three parallel 7-byte tables (X @ 0x520BA, Y @ 0x520C1, facing @ 0x520C8).
 * Read-only const table in .object2.
 *
 * Caller (fd2_chapter_03_end @ 0x230F2):
 *     MOV  ESI,0x520C1
 *     LEA  EDI,[ESP+0x8]
 *     MOVSD ; MOVSW ; MOVSB        ; copy 7 raw bytes onto stack scene block
 * => the table is block-copied (4+2+1 = 7 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (recruit_block_b._0_1_ .. [6]). No struct stride, no wider
 *    element access.
 * => element type: uint8, 7 entries, read-only (single READ xref, no writer).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch03_end_scene_char_pos_y_table[7] = {
    3, 3, 3, 2, 2, 4, 1
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch03_end_scene_char_facing_table @ 0x520C8  (7 bytes)
 *
 * Chapter 3 end-scene character placement: per-character sprite facing
 * direction (0..3) for the 7 characters staged in the chapter-3 recruit
 * cutscene. Third of three parallel 7-byte tables (X @ 0x520BA,
 * Y @ 0x520C1, facing @ 0x520C8). Read-only const table in .object2.
 *
 * Caller (fd2_chapter_03_end @ 0x230F2):
 *     MOV  ESI,0x520C8
 *     MOV  EDI,ESP
 *     MOVSD ; MOVSW ; MOVSB        ; copy 7 raw bytes onto stack scene block
 * => the table is block-copied (4+2+1 = 7 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (recruit_block_c._0_1_ .. [6]). No struct stride, no wider
 *    element access.
 * => element type: uint8, 7 entries, read-only (single READ xref, no writer).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch03_end_scene_char_facing_table[7] = {
    2, 2, 2, 3, 1, 2, 0
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch05_end_scene_char_pos_x_table @ 0x520CF  (7 bytes)
 *
 * Chapter 5 end-scene character placement: per-character battle-tile X
 * coordinate for the 7 characters staged in the chapter-5 recruit cutscene
 * (recruit char #10). First of three parallel 7-byte tables (X @ 0x520CF,
 * Y @ 0x520D6, facing @ 0x520DD). Read-only const table in .object2.
 *
 * Caller (fd2_chapter_05_end @ 0x231F9):
 *     MOV  ESI,0x520CF
 *     LEA  EDI,[ESP+0x10]
 *     MOVSD ; MOVSW ; MOVSB        ; copy 7 raw bytes onto stack scene block
 * => the table is block-copied (4+2+1 = 7 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (var_10._0_1_ .. [6]). No struct stride, no wider element
 *    access, no sign extension.
 * => element type: uint8, 7 entries, read-only (single READ xref, no writer).
 *    The block is passed as the X-position argument to
 *    fd2_setup_chars_and_camera_for_intro @ 0x233C6.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch05_end_scene_char_pos_x_table[7] = {
    12, 11, 13, 10, 10, 14, 14
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch05_end_scene_char_pos_y_table @ 0x520D6  (7 bytes)
 *
 * Chapter 5 end-scene character placement: per-character battle-tile Y
 * coordinate for the 7 characters staged in the chapter-5 recruit cutscene
 * (recruit char #10). Second of three parallel 7-byte tables (X @ 0x520CF,
 * Y @ 0x520D6, facing @ 0x520DD). Read-only const table in .object2.
 *
 * Caller (fd2_chapter_05_end @ 0x231F9):
 *     MOV  ESI,0x520D6
 *     LEA  EDI,[ESP+0x8]
 *     MOVSD ; MOVSW ; MOVSB        ; copy 7 raw bytes onto stack scene block
 * => the table is block-copied (4+2+1 = 7 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (var_18._0_1_ .. [6]). No struct stride, no wider element
 *    access, no sign extension.
 * => element type: uint8, 7 entries, read-only (single READ xref, no writer).
 *    The block is passed as the Y-position argument to
 *    fd2_setup_chars_and_camera_for_intro @ 0x233C6.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch05_end_scene_char_pos_y_table[7] = {
    11, 11, 11, 9, 10, 9, 10
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch05_end_scene_char_facing_table @ 0x520DD  (7 bytes)
 *
 * Chapter 5 end-scene character placement: per-character sprite facing
 * direction for the 7 characters staged in the chapter-5 recruit cutscene
 * (recruit char #10). Third of three parallel 7-byte tables (X @ 0x520CF,
 * Y @ 0x520D6, facing @ 0x520DD). Read-only const table in .object2.
 *
 * Caller (fd2_chapter_05_end @ 0x231F9):
 *     MOV  ESI,0x520DD
 *     LEA  EDI,[ESP+0x0]
 *     MOVSD ; MOVSW ; MOVSB        ; copy 7 raw bytes onto stack scene block
 * => the table is block-copied (4+2+1 = 7 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (var_20._0_1_ .. [6]). No struct stride, no wider element
 *    access, no sign extension. Values are small direction codes (1..3).
 * => element type: uint8, 7 entries, read-only (single READ xref, no writer).
 *    The block is passed as the facing-direction argument to
 *    fd2_setup_chars_and_camera_for_intro @ 0x233C6.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch05_end_scene_char_facing_table[7] = {
    2, 2, 2, 3, 3, 1, 1
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch07_end_scene_char_pos_x_table @ 0x520E4  (9 bytes)
 *
 * Chapter 7 end-scene character placement: per-character battle-tile X
 * coordinate for the 9 characters staged in the chapter-7 recruit cutscene
 * (recruit char #12, 武者凱麗). First of three parallel 9-byte tables
 * (X @ 0x520E4, Y @ 0x520ED, facing @ 0x520F6). Read-only const table in
 * .object2.
 *
 * Caller (fd2_chapter_07_end @ 0x232E8):
 *     MOV  ESI,0x520E4
 *     LEA  EDI,[ESP+0x18]
 *     MOVSD ; MOVSD ; MOVSB        ; copy 9 raw bytes onto stack scene block
 * => the table is block-copied (4+4+1 = 9 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (var_14._0_1_ .. bStack_c). No struct stride, no wider element
 *    access, no sign extension. Values are small tile coords (9..15).
 * => element type: uint8, 9 entries, read-only (single READ xref, no writer).
 *    The block is passed as the X-position argument to
 *    fd2_setup_chars_and_camera_for_intro @ 0x233C6.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch07_end_scene_char_pos_x_table[9] = {
    12, 11, 13, 10, 14, 10, 14, 9, 15
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch07_end_scene_char_pos_y_table @ 0x520ED  (9 bytes)
 *
 * Chapter 7 end-scene character placement: per-character battle-tile Y
 * coordinate for the 9 characters staged in the chapter-7 recruit cutscene
 * (recruit char #12, 武者凱麗). Second of three parallel 9-byte tables
 * (X @ 0x520E4, Y @ 0x520ED, facing @ 0x520F6). Read-only const table in
 * .object2.
 *
 * Caller (fd2_chapter_07_end @ 0x232E8):
 *     MOV  ESI,0x520ED
 *     LEA  EDI,[ESP+0xC]
 *     MOVSD ; MOVSD ; MOVSB        ; copy 9 raw bytes onto stack scene block
 * => the table is block-copied (4+4+1 = 9 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (var_20._0_1_ .. bStack_18). No struct stride, no wider element
 *    access, no sign extension. Values are small tile coords (4..7).
 * => element type: uint8, 9 entries, read-only (single READ xref, no writer).
 *    The block is passed as the Y-position argument to
 *    fd2_setup_chars_and_camera_for_intro @ 0x233C6.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch07_end_scene_char_pos_y_table[9] = {
    4, 4, 4, 5, 5, 6, 6, 7, 7
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch07_end_scene_char_facing_table @ 0x520F6  (9 bytes)
 *
 * Chapter 7 end-scene character placement: per-character sprite facing
 * direction for the 9 characters staged in the chapter-7 recruit cutscene
 * (recruit char #12, 武者凱麗). Third of three parallel 9-byte tables
 * (X @ 0x520E4, Y @ 0x520ED, facing @ 0x520F6). Read-only const table in
 * .object2.
 *
 * Caller (fd2_chapter_07_end @ 0x232E8):
 *     MOV  ESI,0x520F6
 *     MOV  EDI,ESP
 *     MOVSD ; MOVSD ; MOVSB        ; copy 9 raw bytes onto stack scene block
 * => the table is block-copied (4+4+1 = 9 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (var_2c._0_1_ .. bStack_24). No struct stride, no wider element
 *    access, no sign extension. Values are sprite direction codes (0..3).
 * => element type: uint8, 9 entries, read-only (single READ xref, no writer).
 *    The block is passed as the facing argument to
 *    fd2_setup_chars_and_camera_for_intro @ 0x233C6.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch07_end_scene_char_facing_table[9] = {
    0, 0, 0, 3, 1, 3, 1, 3, 1
};


/* ----------------------------------------------------------------
 * data_fd2_chapter_ch08_end_scene_char_pos_x_table @ 0x520FF  (10 bytes)
 *
 * Chapter 8 end-scene character placement: per-character battle-tile X
 * coordinate for the 10 characters staged in the chapter-8 recruit cutscene
 * (recruit char #5, 騎士洛娜). First of two parallel 10-byte tables
 * (X @ 0x520FF, Y @ 0x52109). Sprite facing is an inline fixed value (2)
 * in the caller, so there is no facing table. Read-only const table in
 * .object2.
 *
 * Caller (fd2_chapter_08_end @ 0x234BB):
 *     MOV  ESI,0x520FF
 *     LEA  EDI,[ESP+0xC]
 *     MOVSD ; MOVSD ; MOVSW       ; copy 10 raw bytes onto stack scene block
 * => the table is block-copied (4+4+2 = 10 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (var_14._0_1_ .. bStack_c[1]). No struct stride, no wider
 *    element access, no sign extension. Values are small tile coords (11..17).
 * => element type: uint8, 10 entries, read-only (single READ xref, no writer).
 *    The block is passed as the X-position argument to
 *    fd2_setup_chars_and_camera_for_intro @ 0x233C6 (facing arg = inline 2).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch08_end_scene_char_pos_x_table[10] = {
    14, 13, 15, 12, 13, 14, 16, 11, 15, 17
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch08_end_scene_char_pos_y_table @ 0x52109  (10 bytes)
 *
 * Chapter 8 end-scene character placement: per-character battle-tile Y
 * coordinate for the 10 characters staged in the chapter-8 recruit cutscene
 * (recruit char #5, 騎士洛娜). Second of two parallel 10-byte tables
 * (X @ 0x520FF, Y @ 0x52109). Sprite facing is an inline fixed value (2)
 * in the caller, so there is no facing table. Read-only const table in
 * .object2.
 *
 * Caller (fd2_chapter_08_end @ 0x234BB):
 *     MOV  ESI,0x52109
 *     MOV  EDI,ESP
 *     MOVSD ; MOVSD ; MOVSW       ; copy 10 raw bytes onto stack scene block
 * => the table is block-copied (4+4+2 = 10 bytes) as a flat byte source into
 *    an on-stack character-placement block; each entry is consumed one byte
 *    at a time (local_20[0] .. abStack_18[1]). No struct stride, no wider
 *    element access, no sign extension. Values are small tile coords (18..20).
 * => element type: uint8, 10 entries, read-only (single READ xref, no writer).
 *    The block is passed as the Y-position argument to
 *    fd2_setup_chars_and_camera_for_intro @ 0x233C6 (facing arg = inline 2).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch08_end_scene_char_pos_y_table[10] = {
    20, 20, 20, 19, 19, 18, 19, 18, 19, 18
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch10_end_scene_char_pos_x_table @ 0x52113  (11 entries, uint8)
 *
 * Chapter 10 end-scene character placement: per-character battle-tile X
 * coordinate for the 11 characters staged at the end of chapter 10
 * (洞窟中的激戰). First of two parallel 11-byte tables (X @ 0x52113,
 * Y @ 0x5211E). Sprite facing is an inline fixed value (2) written into
 * pSprite_state[1] for every placed char in the caller, so there is no
 * facing table. Read-only const table in .object2.
 *
 * Caller (fd2_chapter_10_end @ 0x235F9):
 *     local_24[0] = data_fd2_chapter_ch10_end_scene_char_pos_x_table[0];
 *     ... (indices 0..10 copied one byte at a time onto an on-stack block)
 *     local_24[10] = data_fd2_chapter_ch10_end_scene_char_pos_x_table[10];
 *     for (i = 0; i < 0xb; i++)
 *         runtime_char[i].bPos_x = local_24[i];   // single-byte field
 * => each entry is consumed one byte at a time and stored into the byte
 *    field runtime_char.bPos_x. No struct stride, no wider element access,
 *    no sign extension. Values are small tile coords (13..17).
 * => element type: uint8, 11 entries, read-only (single READ xref, no writer).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch10_end_scene_char_pos_x_table[11] = {
    14, 15, 16, 13, 14, 15, 16, 17, 14, 15, 16
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch10_end_scene_char_pos_y_table @ 0x5211E  (11 entries, uint8)
 *
 * Chapter 10 end-scene character placement: per-character battle-tile Y
 * coordinate for the 11 characters staged at the end of chapter 10
 * (洞窟中的激戰). Second of two parallel 11-byte tables (X @ 0x52113,
 * Y @ 0x5211E). Read-only const table in .object2.
 *
 * Caller (fd2_chapter_10_end @ 0x235F9):
 *     var_18._0_1_ = data_fd2_chapter_ch10_end_scene_char_pos_y_table[0];
 *     ... (indices 0..10 copied one byte at a time onto an on-stack block)
 *     bStack_e     = data_fd2_chapter_ch10_end_scene_char_pos_y_table[10];
 *     for (i = 0; i < 0xb; i++)
 *         runtime_char[i].bPos_y = ((byte *)&var_18)[i];   // single-byte field
 * asm @ 0x2361E: MOV ESI,0x5211E; MOVSD/MOVSD/MOVSW/MOVSB copies 11 bytes;
 *     MOV BL,byte ptr [ESP+EAX+0xc]; MOV byte ptr [EDX+1],BL
 *     (EDX+1 = runtime_char.bPos_y, a single byte field).
 * => each entry is consumed one byte at a time and stored into the byte
 *    field runtime_char.bPos_y. No struct stride, no wider element access,
 *    no sign extension. Values are small tile coords (38..40).
 * => element type: uint8, 11 entries, read-only (single READ xref, no writer).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch10_end_scene_char_pos_y_table[11] = {
    38, 39, 38, 38, 39, 38, 39, 39, 40, 40, 40
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch12_end_scene_char_pos_x_table @ 0x52129  (14 entries, uint8)
 *
 * Chapter 12 end-scene character placement: per-character battle-tile X
 * coordinate for the 14 characters staged at the end of chapter 12
 * (北山道). First of three parallel 14-byte tables (X @ 0x52129,
 * Y @ 0x52137, facing @ 0x52145). Read-only const table in .object2.
 *
 * Caller (fd2_chapter_12_end @ 0x237D5):
 *     var_18._0_1_ = data_fd2_chapter_ch12_end_scene_char_pos_x_table[0];
 *     ... (indices 0..0xD copied one byte at a time onto an on-stack block)
 *     abStack_c[1] = data_fd2_chapter_ch12_end_scene_char_pos_x_table[0xd];
 *     fd2_setup_chars_and_camera_for_intro(&var_18, &var_28, facing, ...);
 * asm @ 0x237E8: MOV ESI,0x52129; MOVSD/MOVSD/MOVSD/MOVSW copies 14 bytes
 *     onto [ESP+0x20]; the block is then passed to the placement routine.
 * => each entry is consumed one byte at a time as a battle-tile X coord.
 *    No struct stride, no wider element access, no sign extension.
 *    Values are small tile coords (8..12).
 * => element type: uint8, 14 entries, read-only (single READ xref, no writer).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch12_end_scene_char_pos_x_table[14] = {
    10, 11, 9, 12, 8, 10, 11, 9, 12, 8, 8, 12, 8, 12
};
