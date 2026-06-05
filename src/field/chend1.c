/*
 * chend1.c — Chapter 1「初試身手」end handler
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_chapter_01_end @ 0x22EF6  (0 direct callers; dispatched via the
 *   chapter-end handler pointer table @ 0x51DE9)
 *
 * Chapter 1 end handler. Shows the chapter-end dialog page, persists the
 * party's runtime-character state back to the template store, then advances
 * the current-chapter id to 1 (the next chapter the engine will load).
 *
 * Paired init handler: fd2_chapter_01_init @ 0x3231B.
 * Post-action handler: fd2_check_battle_end_default_handler @ 0x205B4.
 * Walkthrough: assets/chapters/chapter_01.md.
 * ---------------------------------------------------------------- */
void fd2_chapter_01_end(void)
{
    fd2_display_dialog_scene(current_chapter_text, 9, 0xa0000, 0x140, 0xcd,
                             0x4c, 0x4a, 0x13, 1);
    fd2_save_runtime_char_to_template();
    data_fd2_chapter_current_chapter_id = 1;
}
