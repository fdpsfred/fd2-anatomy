/* strtab.c -- read-only string tables / message string constants for FD2.
 * Each symbol is a NUL-terminated ASCII C string stored as an explicit byte
 * list (byte-exact with FD2.LE); the decoded text appears in the comment. */
#include "types.h"
#include "globals.h"

/* 0x50004  " Out of Memory !!!\n" + NUL (20 bytes).
 * Message printed by fd2_load_save_and_init_engine on the save-buffer
 * malloc(0x59CB) failure path before exit(1) (BIOS int 10h text-mode reset
 * then printf). Read-only; consumed as a char* by printf. Distinct literal
 * copy from the sibling OOM strings at 0x50023/0x50037 in the same function. */
const char data_fd2_string_save_load_oom_msg_load_pbuf_50004[20] = {
    0x20, 0x4f, 0x75, 0x74, 0x20, 0x6f, 0x66, 0x20, 0x4d, 0x65,
    0x6d, 0x6f, 0x72, 0x79, 0x20, 0x21, 0x21, 0x21, 0x0a, 0x00
};

/* 0x50023  " Out of Memory !!!\n" + NUL (20 bytes).
 * Distinct literal copy used on the tile_event_data_table malloc(0x8a3)
 * failure path inside fd2_load_save_and_init_engine, printed (after a BIOS
 * int 10h text-mode reset) before exit(1). Read-only; consumed as a char* by
 * printf. Sibling OOM copies in the same function: 0x50004 (pBuf malloc),
 * 0x50037 (runtime_char_array malloc). */
const char data_fd2_string_save_load_oom_msg_tile_event_50023[20] = {
    0x20, 0x4f, 0x75, 0x74, 0x20, 0x6f, 0x66, 0x20, 0x4d, 0x65,
    0x6d, 0x6f, 0x72, 0x79, 0x20, 0x21, 0x21, 0x21, 0x0a, 0x00
};

/* 0x50037  " Out of Memory !!!\n" + NUL (20 bytes).
 * Distinct literal copy used on the runtime_char_array malloc(0x1e00)
 * failure path inside fd2_load_save_and_init_engine, printed before exit(1).
 * Read-only; consumed as a char* by printf. */
const char data_fd2_string_save_load_oom_msg_runtime_char_50037[20] = {
    0x20, 0x4f, 0x75, 0x74, 0x20, 0x6f, 0x66, 0x20, 0x4d, 0x65,
    0x6d, 0x6f, 0x72, 0x79, 0x20, 0x21, 0x21, 0x21, 0x0a, 0x00
};

/* 0x50064  " Out of Memory !!!\n" + NUL (20 bytes).
 * malloc-failure message printed by fd2_load_chapter_battle_data on the
 * runtime_char_array malloc(0x1e00) failure path (after an int386(0x10,...)
 * text-mode reset) before exit. Read-only; loaded as a char* and consumed
 * directly by printf. Distinct literal copy from the 0x50004/23/37 ones. */
const char data_fd2_string_field_map_oom_msg_chapter_runtime_50064[20] = {
    0x20, 0x4f, 0x75, 0x74, 0x20, 0x6f, 0x66, 0x20, 0x4d, 0x65,
    0x6d, 0x6f, 0x72, 0x79, 0x20, 0x21, 0x21, 0x21, 0x0a, 0x00
};

/* 0x50086  "\n\n File not found 'FDICON.B24!! \n\n" + NUL (35 bytes).
 * fopen("FDICON.B24","rb") failure message printed by
 * fd2_load_chapter_battle_data on the portrait-sheet open-fail path (after an
 * int386(0x10,...) text-mode reset) before exit. Read-only; loaded as a char*
 * and consumed directly by printf. */
const char data_fd2_string_field_map_fdicon_not_found_err_50086[35] = {
    0x0a, 0x0a, 0x20, 0x46, 0x69, 0x6c, 0x65, 0x20, 0x6e, 0x6f,
    0x74, 0x20, 0x66, 0x6f, 0x75, 0x6e, 0x64, 0x20, 0x27, 0x46,
    0x44, 0x49, 0x43, 0x4f, 0x4e, 0x2e, 0x42, 0x32, 0x34, 0x21,
    0x21, 0x20, 0x0a, 0x0a, 0x00
};

/* 0x51A43  "FDTXT.DAT" + NUL (10 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the chapter/dialog text archive FDTXT.DAT. Readers: main (idx 0 ->
 * all-game-text), fd2_load_save_and_init_engine and fd2_load_chapter_battle_data
 * (idx chapter+1 -> current-chapter-text). Read-only; consumed as a char* path.
 * Immediately precedes the "FDOTHER.DAT" filename string at 0x51A4D. */
const char data_fd2_string_resource_filename_fdtxt_dat[10] = {
    0x46, 0x44, 0x54, 0x58, 0x54, 0x2e, 0x44, 0x41, 0x54, 0x00
};

/* 0x51A4D  "FDOTHER.DAT" + NUL (12 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the misc/global resource archive FDOTHER.DAT (palettes, SFX banks,
 * cinematic sprite sheets, ending art, etc.). Heavily referenced read-only
 * path: fd2_load_save_and_init_engine loads idx 0 -> vga_palette_data, and
 * dozens of animation / cinematic / chapter-intro / ending routines load other
 * indices. The address is taken (array decays) and consumed as a char* path;
 * never written. Immediately follows the "FDTXT.DAT" string at 0x51A43 and
 * precedes the "FDFIELD.DAT" string at 0x51A59. */
const char data_fd2_string_resource_filename_fdother_dat[12] = {
    0x46, 0x44, 0x4f, 0x54, 0x48, 0x45, 0x52, 0x2e, 0x44, 0x41,
    0x54, 0x00
};

/* 0x51A59  "FDFIELD.DAT" + NUL (12 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the per-chapter field/battle archive FDFIELD.DAT, indexed by chapter:
 * chapter*3 -> battle tile map, chapter*3+1 -> tile-event data table,
 * chapter*3+2 -> portrait load buffer. Readers: fd2_load_chapter_battle_data
 * loads all three indices; fd2_load_save_and_init_engine loads chapter*3 and
 * chapter*3+2 (it restores the tile-event table from FD2.SAV instead);
 * fd2_load_chapter_portraits_and_dump_tmp re-loads chapter*3+2; and
 * fd2_chapter_23_end loads the fixed index 0x45 for the chapter's second
 * battlefield. The address is taken (array decays) and consumed as a char*
 * path; never written. Immediately follows the "FDOTHER.DAT" string at
 * 0x51A4D. */
const char data_fd2_string_resource_filename_fdfield_dat_51a59[12] = {
    0x46, 0x44, 0x46, 0x49, 0x45, 0x4c, 0x44, 0x2e, 0x44, 0x41,
    0x54, 0x00
};

/* 0x51A65  "FDSHAP.DAT" + NUL (11 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the battle scene/shape archive FDSHAP.DAT. Readers:
 * fd2_load_save_and_init_engine and fd2_load_chapter_battle_data load
 * idx scene_id*2 -> battle_scene_snapshot and idx scene_id*2+1 ->
 * tile_attribute_flags_buffer, plus combat/spell/skill cinematic routines
 * (fd2_play_full_combat_cinematic, fd2_play_spell_cast_sequence,
 * fd2_execute_special_attack_skill, fd2_play_figani_char_intro_animation,
 * fd2_execute_summon_spell_cast) and fd2_chapter_23_end. The address is taken
 * (array decays) and consumed as a char* path; never written. Immediately
 * follows the "FDFIELD.DAT" string at 0x51A59 and precedes the "DATO.DAT"
 * string at 0x51A70. */
const char data_fd2_string_resource_filename_fdshap_dat_51a65[11] = {
    0x46, 0x44, 0x53, 0x48, 0x41, 0x50, 0x2e, 0x44, 0x41, 0x54,
    0x00
};

/* 0x51A70  "DATO.DAT" + NUL (9 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the portrait/character sprite archive DATO.DAT. Readers load a portrait
 * sprite by id into portrait_sprite_buffer: fd2_display_dialog_scene (dialog VM
 * LOAD ENEMY/ALLY SPRITE opcodes), fd2_play_final_chapter_30_ending, plus the
 * equip / status-screen member menus (fd2_run_equip_member_menu,
 * fd2_run_status_screen_member_menu, fd2_render_status_screen_static_layout) and
 * fd2_load_chapter_portrait. The address is taken (array decays) and consumed as
 * a char* path; never written. Immediately follows the "FDSHAP.DAT" string at
 * 0x51A65 and precedes the "FDMUS.DAT" string at 0x51A79. */
const char data_fd2_string_resource_filename_dato_dat_51a70[9] = {
    0x44, 0x41, 0x54, 0x4f, 0x2e, 0x44, 0x41, 0x54, 0x00
};

/* 0x51A79  "FDMUS.DAT" + NUL (10 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the BGM/music sequence archive FDMUS.DAT. Sole reader is
 * fd2_set_bgm_track_with_fade, which loads FDMUS.DAT[track_id] into the BGM
 * sequence data buffer (then DPMI-locks it and hands it to AIL_init_sequence).
 * The address is taken (PUSH 0x51A79) and consumed as a char* path via
 * fopen(...,"rb"); never written. Immediately follows the "DATO.DAT" string at
 * 0x51A70. */
const char data_fd2_string_resource_filename_fdmus_dat[10] = {
    0x46, 0x44, 0x4d, 0x55, 0x53, 0x2e, 0x44, 0x41, 0x54, 0x00
};

/* 0x51EBF  "%0.5d" + NUL (6 bytes).
 * printf-style zero-padded decimal format template. Sole reader is
 * fd2_render_decimal_number_to_buffer, which copies all 6 bytes to a local
 * stack buffer and overwrites byte[3] ('5') with ('0' + digit_count) to form
 * "%0.Nd", then sprintf()s the value with it before blitting the digit glyphs.
 * Read-only; the symbol itself is never written (the mutation is on the local
 * copy), consumed byte-wise as a char[]. */
const char data_fd2_string_ui_render_decimal_format_template[6] = {
    0x25, 0x30, 0x2e, 0x35, 0x64, 0x00
};

/* 0x52381  "BG.DAT" + NUL (7 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the parallax-background archive BG.DAT. The combat/spell/skill cinematic
 * routines (fd2_play_full_combat_cinematic, fd2_play_spell_cast_cinematic,
 * fd2_play_spell_cast_sequence, fd2_execute_special_attack_skill,
 * fd2_play_figani_char_intro_animation, fd2_execute_summon_spell_cast) load
 * indices 0/1/2 into special_attack_bg_layer_0/1/2 as the three parallax BG
 * planes. The address is taken (PUSH 0x52381 / array decays) and consumed as a
 * char* path; never written. Immediately precedes the "FIGANI.DAT" string at
 * 0x52388. */
const char data_fd2_string_resource_filename_bg_dat_52381[7] = {
    0x42, 0x47, 0x2e, 0x44, 0x41, 0x54, 0x00
};

/* 0x52388  "FIGANI.DAT" + NUL (11 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the character figure-animation archive FIGANI.DAT. The combat/spell/
 * skill cinematic routines (fd2_play_full_combat_cinematic,
 * fd2_play_spell_cast_cinematic, fd2_play_spell_cast_sequence,
 * fd2_execute_special_attack_skill, fd2_play_figani_char_intro_animation,
 * fd2_execute_summon_spell_cast, fd2_play_final_chapter_30_ending) load a pair
 * of indices per character: portrait_id*3 -> silhouette/skeleton sprite and
 * portrait_id*3+1 -> animation byte stream (poses + per-pose SFX/hold-tick
 * metadata). The address is taken (PUSH 0x52388 / array decays) and consumed as
 * a char* path; never written. Immediately follows the "BG.DAT" string at
 * 0x52381. */
const char data_fd2_string_resource_filename_figani_dat_52388[11] = {
    0x46, 0x49, 0x47, 0x41, 0x4e, 0x49, 0x2e, 0x44, 0x41, 0x54,
    0x00
};

/* 0x52393  "TAI.DAT" + NUL (8 bytes).
 * Resource filename passed as the first argument to fd2_load_dat_resource to
 * open the caster character-base sprite archive TAI.DAT. The combat/spell/
 * skill cinematic routines (fd2_play_full_combat_cinematic,
 * fd2_play_spell_cast_sequence, fd2_execute_special_attack_skill,
 * fd2_play_figani_char_intro_animation, fd2_execute_summon_spell_cast,
 * fd2_play_final_chapter_30_ending) load the caster base sprite (indexed by the
 * tile attribute byte at the caster position) into the cast-pose resource
 * buffer. The address is taken (PUSH 0x52393 / array decays) and consumed as a
 * char* path; never written. Immediately follows the "FIGANI.DAT" string at
 * 0x52388. */
const char data_fd2_string_resource_filename_tai_dat[8] = {
    0x54, 0x41, 0x49, 0x2e, 0x44, 0x41, 0x54, 0x00
};
