/*
 * chtab2.c -- chapter end-scene character placement tables (.object2)
 *
 * Read-only lookup tables consumed by per-chapter end handlers. Each handler
 * copies a flat byte table onto an on-stack placement block (MOVSD x N) and
 * passes it to fd2_setup_chars_and_camera_for_intro. X/Y entries are
 * battle-tile coordinates, facing entries are sprite directions (0..3).
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch12_end_scene_char_pos_y_table @ 0x52137  (14 bytes)
 *
 * Per-character Y tile coordinate for the chapter 12 (Bei Shan Dao) end
 * scene. fd2_chapter_12_end copies all 14 bytes onto the stack and passes
 * the block as the pos_y argument to fd2_setup_chars_and_camera_for_intro,
 * placing chars 0..0xD. Read-only; accessed as a flat uint8[14] (the caller
 * does a MOVSD x3 + MOVSW byte copy, no element scaling).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch12_end_scene_char_pos_y_table[14] = {
    4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 3, 3, 2, 2
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch12_end_scene_char_facing_table @ 0x52145  (14 bytes)
 *
 * Per-character sprite facing direction for the chapter 12 (Bei Shan Dao)
 * end scene. fd2_chapter_12_end copies all 14 bytes onto the stack and
 * passes the block as the facing argument to
 * fd2_setup_chars_and_camera_for_intro, placing chars 0..0xD. Read-only;
 * accessed as a flat uint8[14] (the caller does a MOVSD x3 + MOVSW byte
 * copy, no element scaling). Values are sprite directions (1..3).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch12_end_scene_char_facing_table[14] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 3, 1
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch14_end_scene_char_pos_x_table @ 0x52153  (16 bytes)
 *
 * Per-character X tile coordinate for the chapter 14 (Ping Yuan battle) end
 * scene. fd2_chapter_14_end copies all 16 bytes onto the stack (MOVSD x4)
 * and passes the block as the pos_x argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char and stores it into runtime_char.bPos_x for chars 0..0xF.
 * Read-only; accessed as a flat uint8[16] (one byte per char, stride 1).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch14_end_scene_char_pos_x_table[16] = {
    18, 17, 19, 18, 17, 19, 16, 20, 16, 15, 15, 16, 20, 21, 21, 20
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch14_end_scene_char_pos_y_table @ 0x52163  (16 bytes)
 *
 * Per-character Y tile coordinate for the chapter 14 (Ping Yuan battle) end
 * scene. fd2_chapter_14_end copies all 16 bytes onto the stack (MOVSD x4)
 * and passes the block as the pos_y argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char and stores it into runtime_char.bPos_y for chars 0..0xF.
 * Read-only; accessed as a flat uint8[16] (one byte per char, stride 1).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch14_end_scene_char_pos_y_table[16] = {
    15, 15, 15, 16, 16, 16, 15, 15, 12, 13, 14, 14, 12, 13, 14, 14
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch14_end_scene_char_facing_table @ 0x52173  (16 bytes)
 *
 * Per-character sprite facing direction for the chapter 14 (Ping Yuan battle)
 * end scene. fd2_chapter_14_end copies all 16 bytes onto the stack (MOVSD x4)
 * and passes the block as the facing argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char and stores it into runtime_char.pSprite_state[1] for chars
 * 0..0xF. Read-only; accessed as a flat uint8[16] (one byte per char, stride
 * 1). Values are sprite directions (1..3).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch14_end_scene_char_facing_table[16] = {
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 1, 1, 1, 1
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch16_end_scene_char_pos_x_table @ 0x52183  (16 bytes)
 *
 * Per-character X tile coordinate for the chapter 16 (Ice Plain battle) end
 * scene. fd2_chapter_16_end copies all 16 bytes onto the stack (MOVSD x4)
 * and passes the block as the pos_x argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char and stores it into runtime_char.bPos_x for chars 0..0xF.
 * Read-only; accessed as a flat uint8[16] (one byte per char, stride 1). The
 * adjacent pos_y_table follows at 0x52193, bounding this table at 16 bytes.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch16_end_scene_char_pos_x_table[16] = {
    28, 27, 28, 29, 30, 25, 26, 27, 26, 29, 30, 31, 25, 26, 30, 31
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch16_end_scene_char_pos_y_table @ 0x52193  (16 bytes)
 *
 * Per-character Y tile coordinate for the chapter 16 (Ice Plain battle) end
 * scene. fd2_chapter_16_end copies all 16 bytes onto the stack (MOVSD x4)
 * and passes the block as the pos_y argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char (prVar1->bPos_y = *(byte *)(char_idx + pos_y_base)) and
 * stores it into runtime_char.bPos_y for chars 0..0xF. Read-only; accessed
 * as a flat uint8[16] (one byte per char, stride 1). The next symbol (chapter
 * 17 pos_x_table) follows at 0x521A3, bounding this table at 16 bytes.
 * Chapter 16 has no facing table (the handler passes the inline fixed facing
 * value 0).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch16_end_scene_char_pos_y_table[16] = {
    28, 27, 27, 27, 27, 28, 28, 28, 27, 28, 28, 28, 29, 29, 29, 29
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch17_end_scene_char_pos_x_table @ 0x521A3  (16 bytes)
 *
 * Per-character X tile coordinate for the chapter 17 (Blade of Blood and Ice)
 * end scene. fd2_chapter_17_end copies all 16 bytes onto the stack (MOVSD x4)
 * and passes the block as the pos_x argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char (prVar1->bPos_x = *(byte *)(char_idx + pos_x_base)) and
 * stores it into runtime_char.bPos_x for chars 0..0xF. Read-only; accessed
 * as a flat uint8[16] (one byte per char, stride 1). The adjacent pos_y_table
 * follows at 0x521B3 (MOVSD source in the handler), bounding this table at 16
 * bytes.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch17_end_scene_char_pos_x_table[16] = {
    23, 22, 23, 24, 21, 22, 23, 24, 25, 20, 21, 22, 23, 24, 25, 26
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch17_end_scene_char_pos_y_table @ 0x521B3  (16 bytes)
 *
 * Per-character Y tile coordinate for the chapter 17 (Blade of Blood and Ice)
 * end scene. fd2_chapter_17_end copies all 16 bytes onto the stack (MOVSD x4)
 * and passes the block as the pos_y argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char (prVar1->bPos_y = *(byte *)(char_idx + pos_y_base)) and
 * stores it into runtime_char.bPos_y for chars 0..0xF. Read-only; accessed
 * as a flat uint8[16] (one byte per char, stride 1). Immediately follows the
 * pos_x_table at 0x521A3 (the two adjacent MOVSD sources in the handler).
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch17_end_scene_char_pos_y_table[16] = {
    18, 19, 19, 19, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch18_end_scene_char_pos_x_table @ 0x521C3  (17 bytes)
 *
 * Per-character X tile coordinate for the chapter 18 (Distant Shore) end
 * scene. fd2_chapter_18_end copies all 17 bytes onto the stack (MOVSD x4 +
 * MOVSB at 0x23CF2/0x23CF4) and passes the block as the pos_x argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char and stores it into runtime_char.bPos_x. Read-only;
 * accessed as a flat uint8[17] (one byte per char, stride 1). The adjacent
 * pos_y_table follows at 0x521D4 (next MOVSD source in the handler),
 * bounding this table at 17 bytes.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch18_end_scene_char_pos_x_table[17] = {
    22, 22, 21, 21, 21, 21, 20, 20, 20, 20, 22, 23, 24, 22, 23, 24, 25
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch18_end_scene_char_pos_y_table @ 0x521D4  (17 bytes)
 *
 * Per-character Y tile coordinate for the chapter 18 (Distant Shore) end
 * scene. fd2_chapter_18_end copies all 17 bytes onto the stack (MOVSD x4 +
 * MOVSB at 0x23D03/0x23D05) and passes the block as the pos_y argument to
 * fd2_setup_chars_and_camera_for_intro, which indexes it [char_idx] reading
 * one byte per char (prVar1->bPos_y = *(byte *)(char_idx + pos_y_base)) and
 * stores it into runtime_char.bPos_y. Read-only; accessed as a flat uint8[17]
 * (one byte per char, stride 1). Immediately follows the pos_x_table at
 * 0x521C3 (the two adjacent MOVSD sources in the handler); the facing_table
 * follows at 0x521E5, bounding this table at 17 bytes.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch18_end_scene_char_pos_y_table[17] = {
    7, 8, 6, 7, 8, 9, 6, 7, 8, 9, 5, 5, 5, 10, 10, 10, 7
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ch18_end_scene_char_facing_table @ 0x521E5  (17 bytes)
 *
 * Per-character sprite facing/direction for the chapter 18 (Distant Shore)
 * end scene. fd2_chapter_18_end copies all 17 bytes onto the stack (MOVSD x4
 * + MOVSB at 0x23D12/0x23D14) and passes the block as the param_3 facing
 * argument to fd2_setup_chars_and_camera_for_intro. Since the block address
 * is >= 4, that helper indexes it [char_idx] reading one byte per char
 * (bVar2 = *(byte *)(char_idx + facing_base)) and stores it into
 * runtime_char.pSprite_state[1]. Read-only; accessed as a flat uint8[17]
 * (one byte per char, stride 1; values are direction codes 0..3).
 * Immediately follows the pos_y_table at 0x521D4 (the last MOVSD source in
 * the handler), bounding this table at 17 bytes.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ch18_end_scene_char_facing_table[17] = {
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 2, 2, 2, 1
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_combat_cinematic_mode_per_chapter @ 0x52363  (30 bytes)
 *
 * Per-chapter terrain/backdrop override byte for the battle combat-cinematic
 * and AoE-spell-cast backdrop selection, indexed by current_chapter_id
 * (data_fd2_chapter_current_chapter_id @ 0x53C03). Read-only; accessed as a
 * flat uint8[30] (one byte per chapter, stride 1, unsigned): the readers all
 * do `MOVZX reg, byte ptr [current_chapter_id + 0x52363]`.
 *
 * Used by 4 reader sites (no writers):
 *   fd2_play_full_combat_cinematic    @ 0x28B67 / 0x28BCF -- override the
 *       under-foot terrain tile for immune (flying/lifted) classes when this
 *       chapter entry is non-zero.
 *   fd2_resolve_terrain_for_aoe_targets @ 0x2B5FB -- fallback terrain attribute
 *       used when all AoE targets are flying/lifted-immune classes.
 *   fd2_play_spell_cast_sequence      @ 0x2A8BE -- per-chapter BG.DAT backdrop
 *       index for the inline spell-cast cinematic.
 * Value 3 for chapters whose default override applies; 0 for chapters that
 * fall back to the live under-foot tile attribute instead.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_combat_cinematic_mode_per_chapter[30] = {
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 3, 0, 0
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ending_credit_roll_top_portrait_id_table @ 0x525DC  (20 bytes)
 *
 * Top-half portrait id for each of the 20 staff-roll duels played during the
 * game-clear ending cinematic. fd2_play_game_ending_cinematic copies all 20
 * bytes onto its stack with a MOVSD x5 block copy, then in the credit-roll
 * loop (char_idx 0..0x13) reads one byte per iteration:
 *   MOVZX ECX, byte ptr [stack + char_idx]    -- unsigned byte, stride 1
 *   CMP   ECX, 0x4C                            -- < 0x4C -> bTeam = 2, else 0
 *   MOV   runtime_char[0].bPortrait_id, CL     -- portrait id for the top fighter
 * Read-only; accessed as a flat uint8[20] (no element scaling). Values are
 * portrait ids; entries >= 0x4C place the top fighter on the enemy side.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ending_credit_roll_top_portrait_id_table[20] = {
    0x33, 0x6E, 0x13, 0x69, 0x36, 0x75, 0x1E, 0x7B, 0x27, 0x7F,
    0x40, 0x51, 0x34, 0x7D, 0x1A, 0x73, 0x29, 0x5B, 0x1F, 0x7E
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ending_credit_roll_bottom_portrait_id_table @ 0x525F0  (20 bytes)
 *
 * Bottom-half portrait id for each of the 20 staff-roll duels played during the
 * game-clear ending cinematic. fd2_play_game_ending_cinematic copies all 20
 * bytes onto its stack with a MOVSD x5 block copy, then in the credit-roll
 * loop (char_idx 0..0x13) reads one byte per iteration:
 *   MOVZX reg, byte ptr [stack + char_idx]     -- unsigned byte, stride 1
 *   CMP   reg, 0x4C                             -- < 0x4C -> bTeam = 2, else 0
 *   MOV   runtime_char[1].bPortrait_id, reg     -- portrait id for the bottom fighter
 * Read-only; accessed as a flat uint8[20] (no element scaling). Values are
 * portrait ids; entries >= 0x4C place the bottom fighter on the enemy side.
 * The adjacent scripted_outcome_table follows at 0x52604, bounding this table
 * at 20 bytes.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ending_credit_roll_bottom_portrait_id_table[20] = {
    0x67, 0x14, 0x53, 0x1C, 0x7C, 0x26, 0x5D, 0x22, 0x70, 0x2C,
    0x56, 0x35, 0x50, 0x37, 0x78, 0x24, 0x6A, 0x3C, 0x7A, 0x32
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_ending_credit_roll_scripted_outcome_table @ 0x52604  (20 bytes)
 *
 * Scripted-cinematic mode/terrain index for each of the 20 staff-roll duels
 * played during the game-clear ending cinematic. fd2_play_game_ending_cinematic
 * copies all 20 bytes onto its stack with a MOVSD x5 block copy, then in the
 * credit-roll loop (char_idx 0..0x13) reads one byte per iteration:
 *   MOVZX ECX, byte ptr [stack + char_idx]            -- unsigned byte, stride 1
 *   MOV   data_fd2_battle_scripted_cinematic_mode_or_terrain_idx, ECX  (@0x540FF)
 *   CALL  fd2_play_full_combat_cinematic(0, 1)        -- runs the pre-scripted duel
 * Read-only; accessed as a flat uint8[20] (no element scaling). Each byte selects
 * the scripted mode/terrain the duel uses; the block copy as 5 dwords is a speed
 * optimization, the access is per-byte across all 20 entries.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_ending_credit_roll_scripted_outcome_table[20] = {
    0x04, 0x03, 0x33, 0x0E, 0x19, 0x12, 0x28, 0x35, 0x16, 0x18,
    0x1C, 0x11, 0x1E, 0x1F, 0x32, 0x21, 0x22, 0x34, 0x24, 0x2F
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_intro_portrait_pose_y_row_table @ 0x52635  (18 bytes)
 *
 * Per-(chapter-category, transition-state) target Y pixel coordinate for the
 * chapter-intro portrait pose zoom-in/zoom-out animation. Indexed flat as
 *   pose_y_row_table[chapter_meta_byte * 6 + chapter_transition_state]
 * where chapter_meta_byte (0..2) is the first byte of the chapter-intro
 * metadata entry and chapter_transition_state (0..5) is the intro variant,
 * giving a logical [3][6] grid laid out as a flat uint8[18] (stride 6).
 *
 * Read-only; accessed as a flat uint8 (one MOVZX/byte load per lookup, no
 * element scaling). Used by 3 reader sites (no writers):
 *   fd2_render_chapter_intro_overlay     @ 0x2D031 -- adds the byte as a Y
 *       row offset (+ pose_y[off]) into the row-major working buffer when
 *       blitting the static portrait icon.
 *   fd2_chapter_transition_with_intro    @ 0x2D208 -- (pose_y[off] - 0x96) is
 *       the per-frame Y delta for the 10-frame zoom-in pose animation.
 *   fd2_run_chapter_intro_menu_main      @ 0x2E628 -- (pose_y[off] - 0x96) is
 *       the per-frame Y delta for the 11-frame zoom-out pose animation.
 * Values are screen-Y pixel coordinates (e.g. 0x9A=154, 0xDE=222), unsigned
 * (entries exceed signed-byte range). The adjacent pose_x_column_table
 * follows at 0x52647, bounding this table at 18 bytes.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_intro_portrait_pose_y_row_table[18] = {
    29,  41,  59, 154, 182,  10,
    90,  33,  53, 148, 222, 196,
    59,  10,  59, 130, 242, 136
};

/* ----------------------------------------------------------------
 * data_fd2_chapter_intro_portrait_pose_x_column_table @ 0x52647  (18 bytes)
 *
 * Companion to the pose_y_row_table above: the X/column member of the
 * chapter-intro portrait pose animation coordinate pair. Indexed flat with the
 * same key as the Y table:
 *   pose_x_column_table[chapter_meta_byte * 6 + chapter_transition_state]
 * where chapter_meta_byte (0..2) is the first byte of the chapter-intro
 * metadata entry and chapter_transition_state (0..5) is the intro variant,
 * giving a logical [3][6] grid laid out as a flat uint8[18] (stride 6).
 *
 * Read-only; accessed as a flat uint8 (one MOVZX/byte load per lookup). Used
 * by 3 reader sites (no writers):
 *   fd2_render_chapter_intro_overlay     @ 0x2D046 -- the byte is multiplied by
 *       the 0x1C8 buffer stride (IMUL EAX,EAX,0x1C8) and added into the
 *       row-major working buffer when blitting the static portrait icon.
 *   fd2_chapter_transition_with_intro    @ 0x2D1EB -- (pose_x[off] - 0x64) is
 *       the per-frame delta for the 10-frame zoom-in pose animation.
 *   fd2_run_chapter_intro_menu_main      @ 0x2E60B -- (pose_x[off] - 0x64) is
 *       the per-frame delta for the 11-frame zoom-out pose animation.
 * Values are unsigned screen pixel coordinates (e.g. 0xA3=163, 0x96=150),
 * exceeding signed-byte range. Bounded at 18 bytes by the next table at
 * 0x52659.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_chapter_intro_portrait_pose_x_column_table[18] = {
     46, 109, 163, 139,  65,  10,
     30, 105, 163, 139,  85,   8,
     26, 144, 163, 150,  31,  20
};
