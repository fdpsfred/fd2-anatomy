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
