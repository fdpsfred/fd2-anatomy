/*
 * anitab.c -- animation read-only constants and tables (.object2)
 */

#include "types.h"
#include "globals.h"
#include "protos.h"   /* ANI-decoder chunk handler prototypes (dispatch table below) */

/* ----------------------------------------------------------------
 * data_fd2_animation_summon_radial_angle_step_12 @ 0x5022B  (8 bytes, double)
 *
 * Summon-spell variant-C 5-slot radial animation y-amplitude scale.
 * Sole reader fd2_tick_summon_anim_variant_c_5slot_radial (@ 0x26E39) consumes
 * it at 0x27033 via "FMUL double ptr [0x5022B]" while computing each slot's
 * y-coordinate:
 *   y[i] = (int)( angle_accumulator * sin(angle_rad) * 1.2 + 30.0 )
 * The FMUL on an 8-byte operand proves type double; the sibling y-offset
 * constant 30.0 lives at 0x50233 (FADD double ptr [0x50233] at 0x27039).
 *
 * Bytes 33 33 33 33 33 33 F3 3F (LE) = IEEE-754 double 1.2 exactly.
 * Read-only (single READ xref, no writers); compiler rodata literal.
 */
const double data_fd2_animation_summon_radial_angle_step_12 = 1.2;

/* ----------------------------------------------------------------
 * data_fd2_animation_summon_radial_radius_30 @ 0x50233  (8 bytes, double)
 *
 * Summon-spell variant-C 5-slot radial animation y-offset (vertical bias).
 * Sole reader fd2_tick_summon_anim_variant_c_5slot_radial (@ 0x26E39) consumes
 * it at 0x27039 via "FADD double ptr [0x50233]" -- the trailing "+ 30.0" of the
 * per-slot y-coordinate:
 *   y[i] = (int)( angle_accumulator * sin(angle_rad) * 1.2 + 30.0 )
 * The FADD on an 8-byte operand proves type double; the sibling y-amplitude
 * constant 1.2 lives at 0x5022B (FMUL double ptr [0x5022B] at 0x27033).
 *
 * Bytes 00 00 00 00 00 00 3E 40 (LE) = IEEE-754 double 30.0 exactly.
 * Read-only (single READ xref, no writers); compiler rodata literal.
 */
const double data_fd2_animation_summon_radial_radius_30 = 30.0;

/* ----------------------------------------------------------------
 * data_fd2_animation_spell_palette_flash_table @ 0x51AAD  (108 bytes, uint8[108])
 *
 * Per-spell VGA DAC index-0 flash colour table. Layout is 36 entries x 3
 * components stored as three contiguous 36-byte planes:
 *   R plane @ +0x00 (offset  0..35)
 *   G plane @ +0x24 (offset 36..71)
 *   B plane @ +0x48 (offset 72..107)
 * Indexed by spell_id (0x00..0x23 == 0..35).
 *
 * Both readers prove uint8 element / flat 3-plane layout via the same access
 * pattern (no struct field, no stride>1):
 *   fd2_play_spell_palette_flash_with_sfx @ 0x1D700
 *     outp(0x3C9, table[pattern_id]);          // R
 *     outp(0x3C9, table[pattern_id + 0x24]);   // G
 *     outp(0x3C9, table[pattern_id + 0x48]);   // B
 *   fd2_play_figani_animation_loop @ 0x2B855
 *     outp(0x3C9, table[spell_id]);            // R  (same +0/+0x24/+0x48 trio)
 * Each byte is fed to the 8-bit VGA palette DAC data port (0x3C9); values are
 * 6-bit (max 0x3F), confirming an unsigned byte per component.
 *
 * Read-only (both xrefs are DATA reads; no writers); compiler rodata table.
 */
const uint8 data_fd2_animation_spell_palette_flash_table[108] = {
    /* R plane (spell 0x00..0x23) */
    0x3f,0x3f,0x3f,0x3f,0x2b,0x2b,0x2b,0x2b,0x3f,0x23,0x2e,0x2e,
    0x2e,0x3f,0x3f,0x3f,0x3f,0x32,0x32,0x32,0x3f,0x3f,0x23,0x1e,
    0x00,0x3f,0x0a,0x23,0x3f,0x3f,0x3f,0x3f,0x2b,0x3f,0x3f,0x2b,
    /* G plane (spell 0x00..0x23) */
    0x00,0x00,0x00,0x00,0x32,0x32,0x32,0x32,0x3f,0x10,0x28,0x28,
    0x28,0x3d,0x3d,0x28,0x28,0x32,0x32,0x32,0x28,0x28,0x00,0x2a,
    0x00,0x3d,0x1f,0x19,0x3f,0x3f,0x3f,0x3f,0x32,0x3f,0x00,0x32,
    /* B plane (spell 0x00..0x23) */
    0x00,0x00,0x00,0x00,0x3c,0x3c,0x3c,0x3c,0x3f,0x08,0x1e,0x1e,
    0x1e,0x2e,0x2e,0x1e,0x1e,0x32,0x32,0x32,0x1e,0x1e,0x00,0x23,
    0x00,0x2e,0x00,0x00,0x3f,0x3f,0x3f,0x3f,0x3c,0x3f,0x00,0x3c
};

/* ----------------------------------------------------------------
 * data_fd2_animation_status_overlay_flicker_color_template @ 0x51F15  (30 bytes, uint8[30])
 *
 * Per-status-effect silhouette colour template for the "status effect
 * applied" full-screen flicker animation. One palette index per status
 * kind; almost all kinds use 0xC0 (the flicker fill colour), with a few
 * status-specific overrides (0x92/0x48/0xD8 at indices 17..19, 0x23 at 22).
 *
 * Sole reader fd2_animate_status_effect_overlay_flicker (@ 0x1C2DA) copies
 * the table into a stack scratch and indexes it by status_kind:
 *   0x1C2EF  MOV ECX,7
 *   0x1C2F4  MOV EDI,ESP
 *   0x1C2F6  MOV ESI,0x51F15
 *   0x1C2FB  REP MOVSD          ; 7 dwords = 28 bytes
 *   0x1C2FD  MOVSW              ; +1 word  =  2 bytes  -> 30 bytes total
 *   ...
 *   0x1C34B  MOVZX EAX,byte ptr [ESP + EBP*1]   ; tmp_copy[status_kind]
 * The 30-byte REP MOVSD+MOVSW copy width fixes the extent; the MOVZX byte
 * load (stride 1) feeding fd2_tile_blit_24x24_solid_color's palette-index
 * argument fixes the element type as uint8. The DWORD copy granularity is a
 * memcpy optimisation, NOT a 4-byte element stride. The next distinct table
 * (data_fd2_animation_spell_sprite_offset_table) begins exactly at +30
 * (0x51F33), confirming the 30-byte boundary.
 *
 * Read-only (single READ xref, no writers); compiler rodata table.
 *
 * Type is uint8[30], not a 4-byte-element array: the byte-stride MOVZX
 * caller access and the 30-byte REP MOVSD+MOVSW copy extent are decisive.
 */
const uint8 data_fd2_animation_status_overlay_flicker_color_template[30] = {
    0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,
    0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x92,0x48,0xd8,
    0xc0,0xc0,0x23,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0
};

/* ----------------------------------------------------------------
 * data_fd2_animation_spell_sprite_offset_table @ 0x51F33  (33 bytes, uint8[33])
 *
 * Per-spell sprite-index base offset (animation frame base) for the
 * per-target spell-impact animation. One byte per spell_id; the value is
 * added to the running frame index to pick the source sprite within the
 * portrait sheet. First of three parallel 33-byte byte tables:
 *   data_fd2_animation_spell_sprite_offset_table @ 0x51F33 (sprite-base offset)
 *   data_fd2_animation_spell_frame_count_table   @ 0x51F54 (total frame count)
 *   data_fd2_animation_spell_sfx_frame_table     @ 0x51F75 (SFX trigger frame)
 *
 * Sole reader fd2_animate_spell_impact_per_target (@ 0x1C4CC) copies the
 * table into a stack scratch and indexes it by spell_id:
 *   0x1C4E4  MOV ECX,8
 *   0x1C4E9  LEA EDI,[ESP + 0x48]
 *   0x1C4ED  MOV ESI,0x51F33
 *   0x1C4F2  REP MOVSD          ; 8 dwords = 32 bytes
 *   0x1C4F4  MOVSB              ; +1 byte  = 33 bytes total
 *   ...
 *   0x1C67B  MOVZX EAX,byte ptr [ESP + EBP*1 + 0x48]   ; tmp[spell_id]
 *   0x1C680  ADD EAX,EDI                               ; + frame_idx
 *   0x1C688  MOV EAX,[EDX + EAX*4 + 0x6]               ; -> sprite frame entry
 * The 33-byte REP MOVSD+MOVSB copy width fixes the extent; the MOVZX byte
 * load (stride 1, zero-extended) fixes the element type as unsigned uint8.
 * The DWORD copy granularity is a memcpy optimisation, NOT a 4-byte stride.
 * The next table (data_fd2_animation_spell_frame_count_table) begins exactly
 * at +33 (0x51F54), confirming the 33-byte boundary.
 *
 * Read-only (single READ xref + the base-address DATA xref, no writers);
 * compiler rodata table.
 */
const uint8 data_fd2_animation_spell_sprite_offset_table[33] = {
    0x31,0x31,0x31,0x31,0x40,0x40,0x40,0x40,0x4c,0x57,0x31,
    0x31,0x31,0x39,0x39,0x39,0x39,0xb7,0x7e,0x93,0xcc,0xd9,
    0xaa,0x31,0x31,0xbf,0x8a,0x9e,0x31,0x31,0x00,0x00,0x40
};

/* ----------------------------------------------------------------
 * data_fd2_animation_spell_frame_count_table @ 0x51F54  (33 bytes, uint8[33])
 *
 * Per-spell total animation frame count for the per-target spell-impact
 * animation. One byte per spell_id; the value is the outer frame-loop bound
 * (how many frames the impact effect plays). Second of three parallel 33-byte
 * byte tables:
 *   data_fd2_animation_spell_sprite_offset_table @ 0x51F33 (sprite-base offset)
 *   data_fd2_animation_spell_frame_count_table   @ 0x51F54 (total frame count)
 *   data_fd2_animation_spell_sfx_frame_table     @ 0x51F75 (SFX trigger frame)
 *
 * Sole reader fd2_animate_spell_impact_per_target (@ 0x1C4CC) copies the
 * table into a stack scratch and indexes it by spell_id:
 *   0x1C4F5  MOV ECX,8
 *   0x1C4FA  MOV EDI,ESP
 *   0x1C4FC  MOV ESI,0x51F54
 *   0x1C501  REP MOVSD          ; 8 dwords = 32 bytes
 *   0x1C503  MOVSB              ; +1 byte  = 33 bytes total
 *   ...
 *   0x1C66F  MOVZX EAX,byte ptr [ESP + EBP*1]   ; tmp[spell_id] = frame_count
 *   0x1C673  CMP EDI,EAX                        ; loop while frame_idx < count
 * The 33-byte REP MOVSD+MOVSB copy width fixes the extent; the MOVZX byte
 * load (stride 1, zero-extended) fixes the element type as unsigned uint8.
 * The DWORD copy granularity is a memcpy optimisation, NOT a 4-byte stride.
 * The next table (data_fd2_animation_spell_sfx_frame_table) begins exactly
 * at +33 (0x51F75), confirming the 33-byte boundary.
 *
 * Read-only (single READ xref, no writers); compiler rodata table.
 */
const uint8 data_fd2_animation_spell_frame_count_table[33] = {
    8,8,8,8,10,10,10,10,11,27,8,8,8,7,7,7,7,8,12,11,13,
    13,13,8,8,13,9,12,8,8,0,0,10
};

/* ----------------------------------------------------------------
 * data_fd2_animation_spell_sfx_frame_table @ 0x51F75  (33 bytes, uint8[33])
 *
 * Per-spell primary SFX trigger value for the per-target spell-impact
 * animation. One byte per spell_id; the value is the sound-effect id played
 * on frame 0 (0 = no SFX hook on frame 0). Third of three parallel 33-byte
 * byte tables:
 *   data_fd2_animation_spell_sprite_offset_table @ 0x51F33 (sprite-base offset)
 *   data_fd2_animation_spell_frame_count_table   @ 0x51F54 (total frame count)
 *   data_fd2_animation_spell_sfx_frame_table     @ 0x51F75 (SFX trigger value)
 *
 * Sole reader fd2_animate_spell_impact_per_target (@ 0x1C4CC) copies the
 * table into a stack scratch and indexes it by spell_id:
 *   0x1C504  MOV ECX,8
 *   0x1C509  LEA EDI,[ESP + 0x24]
 *   0x1C50D  MOV ESI,0x51F75
 *   0x1C512  REP MOVSD          ; 8 dwords = 32 bytes
 *   0x1C514  MOVSB              ; +1 byte  = 33 bytes total
 *   ...
 *   0x1C647  CMP byte ptr [ESP + EBP*1 + 0x24],0x0   ; if tmp[spell_id] != 0
 *   0x1C650  MOVZX EAX,byte ptr [ESP + EBP*1 + 0x28] ; sfx_id = tmp[spell_id]
 *   0x1C655  PUSH EAX                                ; -> fd2_play_sfx_with_handle
 * The 33-byte REP MOVSD+MOVSB copy width fixes the extent; the byte-wide CMP
 * (the !=0 hook test) plus the MOVZX byte load (stride 1, zero-extended)
 * feeding fd2_play_sfx_with_handle's sfx-id argument fix the element type as
 * unsigned uint8. The DWORD copy granularity is a memcpy optimisation, NOT a
 * 4-byte element stride. This is the last of the three sibling tables; the
 * next distinct table (data_fd2_animation_spell_overlay_blink_mask_table)
 * begins at 0x52006.
 *
 * (Additional per-frame SFX cues for specific spells are emitted by an inline
 * dispatch chain in the reader, not from this table.)
 *
 * Read-only (single READ xref + the base-address DATA xref, no writers);
 * compiler rodata table.
 */
const uint8 data_fd2_animation_spell_sfx_frame_table[33] = {
    6,6,6,6,9,9,9,9,10,14,0,0,0,12,12,12,12,6,7,8,4,4,
    3,0,0,5,3,2,0,0,0,0,9
};

/* ----------------------------------------------------------------
 * data_fd2_animation_spell_overlay_blink_mask_table @ 0x52006  (30 bytes)
 *
 * Per-spell tint-mask byte table for the 10-frame "spell hit mark fade"
 * overlay animation. Sole reader fd2_animate_spell_overlay_blink (@ 0x1CD17)
 * copies the whole table into a 32-byte stack scratch, then indexes it by
 * spell_id to pick the per-spell tint anchor for each fading 24x24 blit:
 *   0x1CD2C  MOV ECX,7
 *   0x1CD31  MOV EDI,ESP
 *   0x1CD33  MOV ESI,0x52006
 *   0x1CD38  REP MOVSD          ; 7 dwords = 28 bytes
 *   0x1CD3A  MOVSW              ; +1 word  = 30 bytes total
 *   ...
 *   0x1CDE9  MOVZX EAX,byte ptr [ESP + EBP*1 + 0x4]  ; tmp[spell_id], EBP=spell_id
 *   0x1CDEE  PUSH EAX                                ; -> tile_blit tint-mask arg
 * The 30-byte REP MOVSD+MOVSW copy width fixes the extent; the byte-wide
 * MOVZX load (stride 1, zero-extended) feeding the tint-mask argument of
 * fd2_tile_blit_24x24_with_tint_offset fixes the element type as unsigned
 * uint8. The DWORD copy granularity is a memcpy optimisation, NOT a 4-byte
 * element stride. The extent ends exactly where the next named table,
 * data_fd2_battle_spell_bit_mask_lookup_table (the {1,2,4,...,0x80} byte
 * masks consumed by fd2_grant_spell_to_char), begins at 0x52024.
 *
 * (Ghidra's applied datatype byte[28] is 2 bytes short of the real 30-byte
 * copy extent; corrected to byte[30] in the program database.)
 *
 * Read-only (single READ xref + the base-address DATA xref, no writers);
 * compiler rodata table.
 */
const uint8 data_fd2_animation_spell_overlay_blink_mask_table[30] = {
    0x20,0x20,0x20,0x20,0x08,0x08,0x08,0x08,
    0xc8,0x08,0x08,0x08,0x08,0x08,0x10,0x10,
    0x10,0x10,0x08,0x08,0x10,0x10,0x10,0x08,
    0x08,0x10,0x10,0x10,0x08,0x08
};

/* ----------------------------------------------------------------
 * data_fd2_animation_earthquake_screen_shake_params_table @ 0x52096  (36 bytes, int32[9])
 *
 * Earthquake (地震) spell full-screen-shake parameter table. 9 signed 32-bit
 * values laid out column-major as three parallel 3-entry sets, one entry per
 * slow-shake frame (3 frames):
 *   X-offset[0..2] -> entries [0],[1],[2]  = { 128,   0, -128 }
 *   Y-offset[0..2] -> entries [3],[4],[5]  = { 128,   0,  128 }
 *   scale  [0..2]  -> entries [6],[7],[8]  = { 131, 128,  125 }
 *
 * Sole reader fd2_cast_earthquake_spell_with_screen_shake (@ 0x21548) copies
 * the whole table onto the stack as nine contiguous dwords, then consumes it
 * per slow-shake frame i (0..2):
 *   0002155d  MOV ESI,0x52096 ; MOVSD x3   ; entries [0..2] -> X-offset slots
 *   00021569  MOV ESI,0x520a2 ; MOVSD x3   ; entries [3..5] -> Y-offset slots
 *   00021575  MOV ESI,0x520ae ; MOVSD x3   ; entries [6..8] -> scale slots
 *   ...
 *   0002167e  PUSH dword ptr [ESP+ESI*4+0x14]   ; scale[i]   -> blit arg3
 *   000216a4  ADD EAX,dword ptr [ESP+ESI*4+0x30] ; +X-offset[i] -> blit arg1
 *   000216cb  ADD EAX,dword ptr [ESP+ESI*4+0x28] ; +Y-offset[i] -> blit arg2
 * Every access is a 4-byte (dword) load/move; the values are passed as the
 * x/y screen-shake displacements and the magnification arg of
 * fd2_blit_scaled_tile_map_view. The presence of -128 (entry[2]) proves the
 * element type is SIGNED 32-bit int. The next labeled symbol begins at
 * 0x520ba, confirming the 36-byte (9 x 4) extent.
 *
 * (Layer-2 consumer src/spell/spellcin.c indexes the global directly as
 * shake_params[i] / [i+3] / [i+6] rather than copying to stack -- equivalent.)
 *
 * Read-only (single READ xref + the base-address DATA xref, no writers);
 * compiler rodata table.
 */
const int32 data_fd2_animation_earthquake_screen_shake_params_table[9] = {
    128,   0, -128,   /* X-offset per slow-shake frame */
    128,   0,  128,   /* Y-offset per slow-shake frame */
    131, 128,  125    /* scale    per slow-shake frame */
};

/* ----------------------------------------------------------------
 * data_fd2_animation_spell_projectile_y_offset_table @ 0x5202C  (25 bytes, uint8[25])
 *
 * AoE spell projectile / spark vertical-rise offset sequence, used by
 * fd2_animate_spell_projectile_paths (@ 0x1DF58) for the 22-frame multi-target
 * flight animation (lightning / missile-rain effects).
 *
 * Setup (@ 0x1DF69-0x1DF77) copies the table into a stack scratch via
 *   MOV ECX,6 ; MOV ESI,0x5202C ; REP MOVSD ; MOVSB
 * i.e. exactly 6 dwords + 1 byte = 25 bytes (the sole READ + base DATA xrefs).
 * Each element is consumed as an unsigned byte at 0x1E09A:
 *   MOVZX EAX, byte ptr [ESP + ((fx_iter%4) + frame)] ; SUB EAX,3 ; IMUL ...,0x1C8
 * The MOVZX byte load proves element type uint8; the index range is
 * frame(0..21) + (fx_iter%4)(0..3) = 0..24, exactly the 25 entries. The next
 * labeled symbol begins at 0x52045 (read by fd2_show_damage_number), so the
 * table is 25 bytes -- not 28; the local stack buffer is 28 but only 25 are
 * filled.
 *
 * Read-only (single READ xref + the base-address DATA xref, no writers);
 * compiler rodata table.
 */
const uint8 data_fd2_animation_spell_projectile_y_offset_table[25] = {
    0x0f,0x0f,0x0f,0x0f,0x07,0x03,0x01,0x00,
    0x00,0x01,0x03,0x07,0x0f,0x0f,0x0b,0x09,
    0x08,0x08,0x09,0x0b,0x0f,0x0f,0x0f,0x0f,
    0x0f
};

/* ----------------------------------------------------------------
 * data_fd2_animation_summon_variant_c_radial_5slot_offsets @ 0x524F8
 *   (20 bytes, int32[5])
 *
 * Summon-spell variant-C 5-slot radial animation per-slot x-offset table.
 * Sole reader fd2_tick_summon_anim_variant_c_5slot_radial (@ 0x26E39) copies
 * the whole table into a local stack buffer at 0x26E5D-0x26E62:
 *   MOV ECX,0x5 ; MOV ESI,0x524F8 ; REP MOVSD  -> 5 dwords (20 bytes)
 * The REP MOVSD with ECX=5 proves element type int32 and count 5 (4-byte
 * stride). The values are signed offsets: for the enemy team (bTeam==0) the
 * first three are negated in place
 *   (0x26EA1 MOV EDX,[ESP+EBX*4] ; 0x26EA4 NEG EDX ; 0x26EA6 store back),
 * then later consumed as a signed x-coordinate addend in the blit call
 * (0x27187 ADD EDX,[ESP+ESI*4+8]). The NEG and signed use confirm int32.
 * Indexed by a per-slot frame value (scratch sprite-id) in state 4/5.
 *
 * Bytes 0A 00 00 00 08 00 00 00 03 00 00 00 00 00 00 00 00 00 00 00 (LE)
 *   = {10, 8, 3, 0, 0}. The sibling byte-width copy of the same values lives
 * at 0x5250C (data_fd2_animation_summon_variant_c_radial_5slot_byte_offsets,
 * uint8[5]) and is the start of the next labeled symbol, so this table is
 * exactly 20 bytes.
 * Read-only (single READ xref + the base-address DATA xref, no writers);
 * compiler rodata table.
 */
const int32 data_fd2_animation_summon_variant_c_radial_5slot_offsets[5] = {
    10, 8, 3, 0, 0
};

/* ----------------------------------------------------------------
 * data_fd2_animation_summon_variant_c_radial_5slot_byte_offsets @ 0x5250C
 *   (5 bytes, uint8[5])
 *
 * Summon-spell variant-C 5-slot radial animation per-slot byte y-offset table;
 * the byte-width companion to the int32 x-offset table at 0x524F8 above.
 * Sole reader fd2_tick_summon_anim_variant_c_5slot_radial (@ 0x26E39) copies
 * all 5 bytes into a local stack buffer at 0x26E68-0x26E6E:
 *   MOV ESI,0x5250C ; MOVSD ; MOVSB  -> 4+1 = 5 bytes
 * The trailing MOVSB (not a second MOVSD) proves the table is exactly 5 bytes
 * and byte-granular. Each element is later read unsigned, 1-byte stride, at
 * 0x2716D in the state-4/5 blit path:
 *   MOVZX ESI,byte ptr [0x54095]            ; acc = angle_accumulator (index)
 *   MOVZX EDX,byte ptr [ESP + ESI*1 + 0x2c] ; byte_offs[acc], *1 stride, zero-ext
 * and added as a y-offset into the blit coordinate. MOVZX + *1 stride proves
 * element type uint8. (For the enemy team the first three entries are zeroed in
 * place at 0x26EA9 MOV byte ptr [ESP+EBX*1+0x24],0x0.)
 *
 * Bytes 0A 08 03 00 00 = {10, 8, 3, 0, 0} (same logical values as the int32
 * sibling, stored byte-wide here).
 * Read-only (single READ xref + the base-address DATA xref, both the MOVSD/MOVSB
 * copy source; no writers); compiler rodata table.
 */
const uint8 data_fd2_animation_summon_variant_c_radial_5slot_byte_offsets[5] = {
    10, 8, 3, 0, 0
};

/* ----------------------------------------------------------------
 * data_fd2_animation_summon_variant_d_3slot_color_row_offsets @ 0x52511
 *   (40 bytes, int32[10])
 *
 * Summon-spell variant-D 3-active-slot animation per-color row y-offset table.
 * Sole reader fd2_tick_summon_anim_variant_d_3slot (@ 0x272B8, dispatch entry
 * #7 of the 10-entry summon-tick table) copies all 10 dwords into a local
 * stack buffer at 0x272D2:
 *   MOV ECX,0xA ; MOV ESI,0x52511 ; MOVSD.REP  -> 10 * 4 = 40 bytes
 * REP-MOVSD with ECX=10 proves the table is exactly 10 elements, 4-byte
 * stride. Each element is a signed y-offset: index #1 holds 0xFFFFFFF6 (-10),
 * and the decompiler types the source pointer as int*. The local copy is read
 * with a 4-byte stride and added into a blit y-coordinate:
 *   ADD ECX,dword ptr [ESP + EDX*4 + 0x8]   ; origin_y + local_offsets[color]
 * then passed as the y argument to fd2_blit_indexed_sprite. The signed -10
 * entry and the additive use confirm element type int32 (signed). For the
 * enemy team (runtime_char[caster].bTeam == 0) every entry is shifted +0x82 in
 * the local copy (0x272FA ADD dword ptr [ESP+EBX*4],0x82); the table itself is
 * never written (only the stack copy is mutated).
 *
 * Bytes (LE) 1E 000000 / F6 FFFFFF / 46 000000 / 14 000000 / 64 000000 /
 *            82 000000 / 28 000000 / 50 000000 / 6E 000000 / 3C 000000
 *   = {30, -10, 70, 20, 100, 130, 40, 80, 110, 60}.
 * Read-only (single READ xref + the base-address DATA xref, both the MOVSD
 * copy source; no writers); compiler rodata table.
 */
const int32 data_fd2_animation_summon_variant_d_3slot_color_row_offsets[10] = {
    30, -10, 70, 20, 100, 130, 40, 80, 110, 60
};

/* ----------------------------------------------------------------
 * data_fd2_animation_summon_variant_e_16slot_sprite_base_table @ 0x52539
 *   (16 bytes, uint8[16])
 *
 * Summon-spell variant-E 16-slot animation per-slot sprite-id base table.
 * Sole reader fd2_tick_summon_anim_variant_e_16slot (@ 0x274B0, dispatch
 * entry #8 of the 10-entry summon-tick table) copies all 16 bytes into a
 * local stack buffer at the function prologue (0x274D1):
 *   MOV ECX,0x4 ; MOV ESI,0x52539 ; MOVSD.REP  -> 4 * 4 = 16 bytes
 * REP-MOVSD with ECX=4 proves the table is exactly 16 bytes. Each entry is
 * read back from the local copy with a 1-byte, zero-extended stride:
 *   MOVZX EDX,byte ptr [ESP + EBX*0x1 + 0xc]   ; base = table[slot]
 * and added to the slot's frame counter to form a sprite id:
 *   sprite_id = frame_counter[slot] + table[slot]
 * passed to fd2_blit_indexed_sprite. The MOVZX byte load with EBX*1 stride
 * confirms element type uint8 (unsigned, 1-byte). The table itself is never
 * written (single READ xref at 0x274D1 plus its base-address DATA xref at
 * 0x274CC; no writers); compiler rodata table.
 *
 * Bytes (LE): 00 08 18 10 08 00 18 10 00 08 18 10 08 00 18 10
 *   = {0, 8, 24, 16, 8, 0, 24, 16, 0, 8, 24, 16, 8, 0, 24, 16}.
 */
const uint8 data_fd2_animation_summon_variant_e_16slot_sprite_base_table[16] = {
    0x00, 0x08, 0x18, 0x10, 0x08, 0x00, 0x18, 0x10,
    0x00, 0x08, 0x18, 0x10, 0x08, 0x00, 0x18, 0x10
};

/* ----------------------------------------------------------------
 * data_fd2_animation_ani_decoder_frame_dispatch_table @ 0x5276A
 *   (40 bytes, 10 x 4-byte function pointers)
 *
 * ANI.DAT frame-byte-stream opcode dispatch table. The decoder loop
 * fd2_ani_decoder_decode_frame_bytes (@ 0x36C9E) reads one opcode byte per
 * iteration and calls the handler at table[opcode]:
 *   00036cb2  LODSB ESI                       ; AL = opcode byte
 *   00036cb3  SUB AH,AH
 *   00036cb5  SHL AX,0x2                       ; *4  (4-byte pointer stride)
 *   00036cb9  MOVZX EBX,AX
 *   00036cbc  CALL dword ptr [EBX + 0x5276a]   ; dispatch -> handler
 * The "index * 4 then CALL [base+index*4]" addressing proves a flat array of
 * 4-byte function pointers, indexed by the opcode byte. Every handler is a
 * zero-argument void routine that operates on shared globals (the ANI cursor
 * and the target-width/dst/src triple set by fd2_ani_decoder_set_target_buffer
 * @ 0x36C7D); none takes a parameter, so the element type is void (*)(void).
 * The Layer-2 consumer src/anim/anidec.c indexes this table by the chunk-type
 * byte and casts each slot to (void (*)(void)) -- equivalent.
 *
 * Extent: Ghidra types the symbol pointer[10] (len 40). The 10th pointer ends
 * at 0x52792, immediately followed by 4 zero bytes and the "Stack Overflow!"
 * literal -- not an 11th entry -- so the table is exactly 10 entries. (The
 * caller-function plate's "256-entry" wording refers to the conceptual
 * opcode-byte index space, not the allocated symbol size.)
 *
 * Entries, in address order (resolved from the 4-byte LE pointers):
 *   [0] 0x36AE7 fd2_ani_decoder_chunk_palette_fill_byte
 *   [1] 0x36B01 fd2_ani_decoder_chunk_palette_load_literal
 *   [2] 0x36B0F fd2_ani_decoder_chunk_palette_load_rle
 *   [3] 0x36B51 fd2_ani_decoder_chunk_palette_load_run_pairs
 *   [4] 0x36B8A fd2_ani_decoder_chunk_row_fill_byte
 *   [5] 0x36BB2 fd2_ani_decoder_chunk_row_copy_literal
 *   [6] 0x36BCE fd2_ani_decoder_chunk_row_decode_rle
 *   [7] 0x36C13 fd2_ani_decoder_chunk_sparse_set_byte
 *   [8] 0x36C2C fd2_ani_decoder_chunk_sparse_set_run_byte
 *   [9] 0x36C56 fd2_ani_decoder_chunk_sparse_copy_literal
 *
 * Read-only (single base-address DATA xref from the decoder loop; no writers);
 * compiler rodata function-pointer table.
 */
void (*data_fd2_animation_ani_decoder_frame_dispatch_table[10])(void) = {
    fd2_ani_decoder_chunk_palette_fill_byte,
    fd2_ani_decoder_chunk_palette_load_literal,
    fd2_ani_decoder_chunk_palette_load_rle,
    fd2_ani_decoder_chunk_palette_load_run_pairs,
    fd2_ani_decoder_chunk_row_fill_byte,
    fd2_ani_decoder_chunk_row_copy_literal,
    fd2_ani_decoder_chunk_row_decode_rle,
    fd2_ani_decoder_chunk_sparse_set_byte,
    fd2_ani_decoder_chunk_sparse_set_run_byte,
    fd2_ani_decoder_chunk_sparse_copy_literal
};

/* ----------------------------------------------------------------
 * data_fd2_animation_palette_cycle_rgb_table @ 0x60003  (.object3, 93 bytes)
 *
 * VGA palette-cycle RGB animation source for environment effects (water/lava/
 * fire). Read-only. Sole accessor fd2_update_palette_cycle_anim (@ 0x4DFCC):
 *   0004e001: LEA ESI,[0x60003]          ; base = this table
 *   0004dffd/dfff: MOV AH,3 / MUL AH      ; frame_idx (0..15) * 3
 *   0004e007: ADD ESI,EAX                 ; ESI = base + frame_idx*3
 *   0004e009: MOV ECX,0x10                ; 16 colors
 *   0004e019..e01e: LODSB / OUT 0x3C9     ; emit 3 consecutive bytes R,G,B per color
 * So the table is a flat uint8 stream consumed by LODSB (8-bit, unsigned VGA
 * 6-bit components). It is a sliding 48-byte window: frame f reads bytes
 * [f*3 .. f*3+47]. Max start offset 15*3=45, +48-1 => last byte 92 => exactly
 * 93 bytes (matches the symbol extent; the prior "768 bytes" plate note was a
 * mis-derivation of a non-overlapping 16x16x3 layout and is wrong).
 * Laid out below as the 31 RGB triples (byte-exact to memory @ 0x60003).
 */
const uint8 data_fd2_animation_palette_cycle_rgb_table[93] = {
    14, 21, 38,   13, 20, 37,   13, 20, 37,   13, 20, 37,
    12, 19, 36,   12, 19, 36,   11, 18, 35,   11, 18, 35,
    11, 18, 35,   11, 18, 35,   12, 19, 36,   12, 19, 36,
    13, 20, 37,   14, 21, 38,   14, 21, 38,   14, 21, 38,
    14, 21, 38,   13, 20, 37,   13, 20, 37,   13, 20, 37,
    12, 19, 36,   12, 19, 36,   11, 18, 35,   11, 18, 35,
    11, 18, 35,   11, 18, 35,   12, 19, 36,   12, 19, 36,
    13, 20, 37,   14, 21, 38,   14, 21, 38
};
