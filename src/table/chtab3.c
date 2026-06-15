/* chtab3.c -- chapter intro/transition menu read-only data tables (.object2). */
#include "types.h"
#include "globals.h"

/* Per-chapter-state speaker portrait / DATO.DAT entry id for the chapter intro
 * menu. Indexed by chapter_transition_state (0..5); element passed to
 * fd2_load_chapter_portrait / fd2_load_dat_resource. Read-only. @ 0x52659 */
const uint8 data_fd2_chapter_intro_menu_speaker_portrait_id_table[6] =
    { 0x81, 0x80, 0x00, 0x82, 0x83, 0x84 };

/* Per-chapter category flag. Indexed by chapter id (0..29). 0 = story chapter
 * (intro panel + save/load menu), nonzero = battle chapter. Read by
 * fd2_chapter_transition_menu, fd2_save_current_state_to_slot,
 * fd2_load_state_from_selected_slot (CMP byte ptr [id + table], 0). Read-only.
 * @ 0x526B9 */
const uint8 data_fd2_chapter_per_chapter_category_table[30] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 0, 0, 1, 1, 1
};
