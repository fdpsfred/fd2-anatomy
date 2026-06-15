/* chtab3.c -- chapter intro/transition menu read-only data tables (.object2). */
#include "types.h"
#include "globals.h"

/* Per-chapter-state speaker portrait / DATO.DAT entry id for the chapter intro
 * menu. Indexed by chapter_transition_state (0..5); element passed to
 * fd2_load_chapter_portrait / fd2_load_dat_resource. Read-only. @ 0x52659 */
const uint8 data_fd2_chapter_intro_menu_speaker_portrait_id_table[6] =
    { 0x81, 0x80, 0x00, 0x82, 0x83, 0x84 };
