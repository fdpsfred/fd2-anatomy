/*
 * chintro.c — Chapter-intro menu input loops
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_chapter_intro_menu_input_loop @ 0x2D7BD  (3 callers:
 *   fd2_run_chapter_intro_menu_main, fd2_run_chapter_intro_menu_typeB,
 *   fd2_run_chapter_intro_menu_typeC)
 *
 * Chapter-intro 4-way menu input loop. Blocks until commit / cancel:
 *   returns  1 = user committed (Enter / Space)
 *   returns -1 = user cancelled (Esc)
 *
 * Per iteration it polls fd2_wait_input_with_chapter_dialog_blink(0)
 * (which blinks the dialog cursor while waiting) and dispatches on the
 * returned scancode:
 *   0x4B (Left)        play cursor chime; cursor--; wrap < 0 -> 3
 *   0x4D (Right)       play cursor chime; cursor++; wrap > 3 -> 0
 *   0x1C/0x39 (Ent/Sp) commit  (result = 1)
 *   0x01 (Esc)         cancel  (result = -1)
 *   else               keep looping
 * The cursor is data_fd2_ui_menu_cursor_idx (0..3) and the cursor chime
 * is SFX id 0 in the FDOTHER UI bank. Loops while result == 0.
 *
 * int __cdecl with the __CHK(0x14) stack-probe prologue. The cursor
 * Left/Right wrap tests are signed (JGE / JLE), so the unsigned global is
 * cast to int for the bound checks, matching the disassembly.
 * ---------------------------------------------------------------- */
int fd2_chapter_intro_menu_input_loop(void)
{
    int result;
    int scancode;

    result = 0;
    do {
        scancode = fd2_wait_input_with_chapter_dialog_blink(0);
        if (scancode == 0x4b) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 1;
            if ((int)data_fd2_ui_menu_cursor_idx < 0) {
                data_fd2_ui_menu_cursor_idx = 3;
            }
        } else if (scancode == 0x4d) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     0, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1;
            if ((int)data_fd2_ui_menu_cursor_idx > 3) {
                data_fd2_ui_menu_cursor_idx = 0;
            }
        } else if (scancode == 0x1c || scancode == 0x39) {
            result = 1;
        } else if (scancode == 0x01) {
            result = -1;
        }
    } while (result == 0);

    return result;
}
