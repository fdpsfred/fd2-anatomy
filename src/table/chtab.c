/*
 * chtab.c -- chapter dispatch read-only function-pointer tables (.object2)
 *
 * Per-chapter handler jump tables indexed by chapter id (index 0 = chapter 1
 * .. index 29 = chapter 30). The pointed-to handler functions live in
 * src/field/chpost.c (post-action handlers); their prototypes are in protos.h.
 */

#include "types.h"
#include "globals.h"

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
 * Per-chapter init handler, invoked once when a chapter starts (NEW GAME and
 * after a save slot is loaded). Indexed by current_chapter_id (index 0 =
 * chapter 1). Read-only const table in .object2.
 *
 * Caller (fd2_main_menu_continue_dispatcher @ 0x25EBB):
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
