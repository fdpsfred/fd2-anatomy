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

/* FDOTHER.DAT intro-panel RLE resource id, selected by the chapter-intro
 * metadata category code. fd2_chapter_transition_menu copies all 3 bytes into a
 * stack scratch buffer (MOVSW + MOVSB) then indexes them by metadata[0] (stored
 * via base -2 in the source) to obtain the resource id passed to
 * fd2_load_dat_resource("FDOTHER.DAT", idx). Byte-width, unsigned. Read-only.
 * @ 0x526D7 */
const uint8 data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table[3] =
    { 0x0b, 0x3d, 0x3e };
