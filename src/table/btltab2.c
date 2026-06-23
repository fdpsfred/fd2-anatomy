/*
 * btltab2.c -- battle summon-spell read-only data tables (.object2)
 *
 * Per-summon SFX-bank data for the summon-spell (召喚系) cinematic, indexed by
 * summon_idx = spell_id - 0x20. Other adjacent per-summon battle-animation
 * tables are emitted into this file as those symbols are individually processed.
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_aura_ring_8slot_row_multiplier_table @ 0x52440
 * (int32[8])
 *
 * Per-slot row-stride multiplier for the 8 aura-ring sprites orbiting the
 * caster during the summon-spell (召喚系) cinematic; companion of the x-offset
 * table @ 0x52420. Consumed read-only by fd2_render_summon_aura_sprite_ring
 * (@ 0x262EF): each slot's vertical screen position is row_multiplier[slot] *
 * framebuffer_row_stride added to the ring origin, so the signed values pull
 * slots above (negative) and below (positive) center. Whole-dword indexing at
 * a 4-byte stride with signed entries fixes the int32[8] type (matches the
 * Ghidra int[8]). Values (LE) = {-10,-24,-30,-24,-10,4,10,4}.
 * ---------------------------------------------------------------- */
const int32 data_fd2_battle_summon_aura_ring_8slot_row_multiplier_table[8] =
    { -10, -24, -30, -24, -10, 4, 10, 4 };

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_main_anim_12slot_y_offset_table @ 0x52460 (int32[12])
 *
 * Per-color vertical pixel offset added to each of the 12 sprites in the main
 * summon-spell (召喚系) animation. Read-only base offset used by
 * fd2_tick_summon_spell_main_animation_state (@ 0x26795): the slot's screen
 * position is origin_y + y_offset[color] (+0x14 for the enemy team) minus
 * v_offset[color] * row_stride. Whole-dword indexing at a 4-byte stride with
 * signed entries fixes the int32[12] type (matches the Ghidra int[12]).
 * Values (LE) = {30,0,70,40,130,70,-30,30,110,80,-10,30}.
 * ---------------------------------------------------------------- */
const int32 data_fd2_battle_summon_main_anim_12slot_y_offset_table[12] =
    { 30, 0, 70, 40, 130, 70, -30, 30, 110, 80, -10, 30 };

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_main_anim_12color_v_offset_table @ 0x52490 (uint8[12])
 *
 * Per-color row-stride multiplier (vertical lift) for the 12-sprite main
 * summon-spell (召喚系) animation; sibling of the y-offset table @ 0x52460 and
 * the sprite-offset table @ 0x5249C. Read-only in
 * fd2_tick_summon_spell_main_animation_state (@ 0x26795): screen y =
 * origin_y + y_offset[color] - v_offset[color] * row_stride; the value is a
 * small unsigned count of framebuffer rows, byte-loaded with stride 1
 * (uint8[12], matches the Ghidra byte[12]). Values = {0,10,0,20,5,20,0,10,0,18,0,10}.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_battle_summon_main_anim_12color_v_offset_table[12] =
    { 0, 10, 0, 20, 5, 20, 0, 10, 0, 18, 0, 10 };

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_main_anim_12color_sprite_offset_table @ 0x5249C
 * (uint8[12])
 *
 * Per-color base sprite-sheet index for the 12-sprite main summon-spell (召喚系)
 * animation; the rendered frame is sprite_offset[color] + frame_counter.
 * Read-only in fd2_tick_summon_spell_main_animation_state (@ 0x26795), which
 * also keys its SFX bucket on whether sprite_offset[color] is zero (0 -> the
 * sample-from-bank path on the done frame, non-zero -> the with-handle path on
 * frame 0). Byte-loaded with stride 1 (uint8[12], matches the Ghidra byte[12]).
 * Values = {0x16,0,0,0,0,0,0x0B,0,0x16,0,0,0}.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_battle_summon_main_anim_12color_sprite_offset_table[12] =
    { 0x16, 0, 0, 0, 0, 0, 0x0B, 0, 0x16, 0, 0, 0 };

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_a_10color_y_offset_table @ 0x524A8
 * (int32[10])
 *
 * Per-color vertical pixel offset for the variant-A summon-spell (召喚系)
 * animation's 10 color phases. Read-only base offset consumed by
 * fd2_tick_summon_anim_variant_a_6slot (@ 0x269D3); the slot's screen y is
 * origin_y + y_offset[color]. Whole-dword
 * indexing at a 4-byte stride fixes the int32[10] type (matches the Ghidra
 * int[10]). Values (LE) = {30,50,70,40,80,100,70,30,60,90}.
 * ---------------------------------------------------------------- */
const int32 data_fd2_battle_summon_anim_variant_a_10color_y_offset_table[10] =
    { 30, 50, 70, 40, 80, 100, 70, 30, 60, 90 };

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_b_10color_y_offset_table @ 0x524D0
 * (int32[10])
 *
 * Per-color vertical pixel offset for the variant-B summon-spell (召喚系)
 * animation's 10 color phases; the variant-B counterpart of the variant-A
 * table @ 0x524A8 (the two hold identical data but are distinct symbols; this
 * one is read by fd2_tick_summon_anim_variant_b_6slot @ 0x26BFD, the variant-A
 * table by fd2_tick_summon_anim_variant_a_6slot @ 0x269D3). Read-only; the
 * slot's screen y is origin_y + y_offset[color]. Whole-dword indexing at a
 * 4-byte stride fixes the
 * int32[10] type (matches the Ghidra int[10]).
 * Values (LE) = {30,50,70,40,80,100,70,30,60,90}.
 * ---------------------------------------------------------------- */
const int32 data_fd2_battle_summon_anim_variant_b_10color_y_offset_table[10] =
    { 30, 50, 70, 40, 80, 100, 70, 30, 60, 90 };

/* ----------------------------------------------------------------
 * data_fd2_battle_special_attack_shake_x_offset_table @ 0x52549 (uint8[6])
 *
 * Per-sub-frame horizontal screen-shake displacement cache for the special-
 * attack cinematic, indexed by a decaying shake counter (5..0). Read-only base
 * table consumed by fd2_execute_special_attack_skill (@ 0x276EC); the same
 * {0,4,9,14,18,14} shape as the combat-hit x-shake table @ 0x5255F.
 * Byte-loaded with stride 1 (uint8[6], matches the Ghidra byte[6]).
 * Values = {0,4,9,14,18,14}.
 * ---------------------------------------------------------------- */
const uint8 data_fd2_battle_special_attack_shake_x_offset_table[6] =
    { 0, 4, 9, 14, 18, 14 };

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_spell_palette_r_table @ 0x5254F (4-byte dword)
 * data_fd2_battle_summon_spell_palette_g_table @ 0x52553 (4-byte dword)
 * data_fd2_battle_summon_spell_palette_b_table @ 0x52557 (4-byte dword)
 *
 * Per-summon VGA palette R/G/B channel bytes for the summon-spell (召喚系)
 * cinematic, the three tables immediately preceding the SFX-bank-index table @
 * 0x5255B. fd2_execute_summon_spell_cast (@ 0x27FC9) loads each whole 4-byte
 * table into a stack-local dword, then byte-indexes that local copy by
 * summon_idx = spell_id - 0x20 (0x20..0x23 -> 0..3) to pick the per-summon
 * channel value. Because the byte-index lives in the CONSUMER on its local
 * dword copy, each global is a scalar uint32 holding the 4 channel bytes
 * LE-packed (NOT a uint8[4]; an array would decay to a pointer at the whole-
 * dword copy). Read-only.
 *   R @ 0x5254F = {3F,33,35,35} -> 0x3535333F
 *   G @ 0x52553 = {3F,39,00,3A} -> 0x3A00393F
 *   B @ 0x52557 = {3F,3F,00,09} -> 0x09003F3F
 * ---------------------------------------------------------------- */
const uint32 data_fd2_battle_summon_spell_palette_r_table = 0x3535333f;
const uint32 data_fd2_battle_summon_spell_palette_g_table = 0x3a00393f;
const uint32 data_fd2_battle_summon_spell_palette_b_table = 0x09003f3f;

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_spell_sfx_bank_index_table @ 0x5255B (4-byte dword)
 *
 * Per-summon FDOTHER.DAT entry index of the SFX bank loaded for the
 * summon-spell (召喚系) cinematic, sitting immediately after the three
 * per-summon RGB palette tables (R @ 0x5254F / G @ 0x52553 / B @ 0x52557).
 * fd2_execute_summon_spell_cast (@ 0x27FC9) loads the whole 4 bytes in one move
 * (MOV EAX,[0x5255B] / MOV [ESP+0x14],EAX) into a stack-local dword, then
 * byte-indexes that local copy by summon_idx = spell_id - 0x20 (0x20..0x23 ->
 * 0..3) to pick the per-summon bank index, which it passes to
 * fd2_load_dat_resource(0x51A4D="FDOTHER.DAT", 0, index) and stores in
 * data_fd2_audio_summon_spell_sfx_bank_buf_ptr @ 0x5411F:
 *   MOV EAX, [ESP+0x50]                            ; spell_id (0x20..0x23)
 *   MOVZX EAX, byte ptr [ESP + EAX*1 - 0xC]        ; bank = local_copy[summon_idx]
 *   fd2_load_dat_resource(0x51A4D, 0, bank)        ; -> summon SFX bank buffer
 * This is the last of the four adjacent per-summon tables, so its extent is
 * exactly 4 bytes: 5B,5C,5D,5E (little-endian = 0x5E5D5C5B). The byte-index
 * lives in the CONSUMER on its local dword copy, so the global itself is a
 * scalar uint32 holding the 4 bytes LE-packed (NOT a uint8[4]; an array would
 * decay to a pointer at the consumer's whole-dword copy and store the address
 * instead of the bytes). Read-only (only ever copied out, never written; single
 * xref is the READ at 0x27FF5). Each byte is an FDOTHER.DAT resource index, not
 * a VGA channel, so values are not bounded by 0x3F.
 * ---------------------------------------------------------------- */
const uint32 data_fd2_battle_summon_spell_sfx_bank_index_table = 0x5e5d5c5b;

/* ----------------------------------------------------------------
 * data_fd2_battle_combat_hit_shake_x_offset_table @ 0x5255F (int32[6])
 *
 * Per-subframe horizontal screen-shake displacement (in pixels) applied to the
 * defender sprite during the combat-hit FIGANI cinematic. Consumed only by
 * fd2_execute_combat_hit_cinematic (@ 0x2939D), which copies the whole table
 * into a stack-local int[6] in one shot:
 *   MOV  ECX, 0x6
 *   MOV  ESI, 0x5255F
 *   REP  MOVSD                                  ; 6 dwords -> local x-shake[6]
 * The REP MOVSD (move-string-DWORD) proves a 4-byte element stride, 6 elements.
 * A shake counter ESI is latched to 5 on each landed hit and decays toward 0;
 * each subframe the local copy is indexed by that counter as a full dword and
 * added/subtracted to the defender sprite's framebuffer pointer:
 *   ADD  EAX, dword ptr [ESP + ESI*4 + 0x8]     ; player-attacker branch (+x)
 *   SUB  EAX, dword ptr [ESP + ESI*4 + 0x8]     ; enemy-attacker branch  (-x)
 * The SUB path treats the value as a signed displacement, so the element type is
 * the signed int32 the consumer's local int[6] expects (matches Ghidra int[6]).
 * Read-only: the only xrefs are the DATA address-load at 0x293CF and the READ at
 * 0x293D4; never written. The adjacent int32[6] at 0x52577 is the companion
 * vertical table (data_fd2_battle_combat_hit_shake_y_offset_table), a separate
 * symbol. Values (LE) = {0,4,9,14,18,14}.
 * ---------------------------------------------------------------- */
const int32 data_fd2_battle_combat_hit_shake_x_offset_table[6] =
    { 0, 4, 9, 14, 18, 14 };

/* ----------------------------------------------------------------
 * data_fd2_battle_combat_hit_shake_y_offset_table @ 0x52577 (int32[6])
 *
 * Per-subframe vertical screen-shake displacement (in pixels) applied to the
 * defender sprite during the combat-hit FIGANI cinematic; companion of the
 * horizontal table at 0x5255F just above. Consumed only by
 * fd2_execute_combat_hit_cinematic (@ 0x2939D), which copies the whole table
 * into a stack-local int[6] in one shot, immediately after the x-table copy:
 *   MOV  ECX, 0x6
 *   MOV  ESI, 0x52577
 *   REP  MOVSD                                  ; 6 dwords -> local y-shake[6]
 * The REP MOVSD (move-string-DWORD) proves a 4-byte element stride, 6 elements.
 * Each subframe the same shake counter (latched to 5 on a landed hit, decaying
 * toward 0) indexes the local copy as a full dword; the value is scaled by the
 * framebuffer row stride (0x190 = 400) and added/subtracted to the defender
 * sprite pointer to nudge it up or down:
 *   MOV   EAX, dword ptr [ESP + ESI*4 + 0x30]   ; y_offset = y_shake[counter]
 *   IMUL  EDX, [ESP + 0x80], 0x190              ; y_offset * 400 (row stride)
 *   ADD/SUB ...                                 ; player(+) vs enemy(-) branch
 * The signed +/-400 scaling makes the element a signed int32, matching the
 * consumer's local int[6] (and the Ghidra int[6] type). Read-only: the only
 * xrefs are the DATA address-load at 0x293DF and the READ at 0x293E4; never
 * written. Values (LE) = {0,2,4,6,8,10}.
 * ---------------------------------------------------------------- */
const int32 data_fd2_battle_combat_hit_shake_y_offset_table[6] =
    { 0, 2, 4, 6, 8, 10 };

/* ----------------------------------------------------------------
 * data_fd2_battle_combat_speech_bubble_pos_pairs @ 0x53A30 (int32[4])
 *
 * Runtime scratch state for the combat-intro "vs" speech-bubble cinematic:
 * two (x,y) on-screen pixel-position pairs, [0..1] = attacker bubble,
 * [2..3] = counter-attacker bubble. Zero-initialized (.bss); every element is
 * written before it is ever read, so the static contents are don't-care (memory
 * reads as 0x00 x16).
 *
 * Sole accessor fd2_animate_combat_speech_bubbles (@ 0x1EB05) fills it then
 * consumes it:
 *   fd2_compute_combat_bubble_screen_pos(&pairs[0], defender_idx);   // 0x53A30
 *   if (can_counter)
 *       fd2_compute_combat_bubble_screen_pos(&pairs[2], attacker_idx); // 0x53A38
 *   else
 *       pairs[2] = -1;                              // MOV dword[0x53A38],0xFFFFFFFF
 * then per animation frame blits at (pairs[0],pairs[1]) and, when pairs[2] != -1,
 * at (pairs[2],pairs[3]). Every access is a whole dword (PUSH dword ptr / MOV
 * dword ptr), indexed flat at strides of 4 (0x53A30/34/38/3C), proving a 4-byte
 * element stride and exactly 4 elements (16 bytes).
 *
 * Element type is signed int32: the writer fd2_compute_combat_bubble_screen_pos
 * (@ 0x1EC2A) does signed coordinate arithmetic on the pair it is handed
 * (int iVar1 = y + -0x12; if (iVar1 < 0) ...; -1 < x + -0x56), and the [2] = -1
 * "no counter" sentinel is tested as signed (CMP dword[0x53A38],-0x1). The pair
 * pointer the writer receives is either 0x53A30 (attacker) or 0x53A38 (counter),
 * confirming the two-pair layout.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_combat_speech_bubble_pos_pairs[4];

/* ----------------------------------------------------------------
 * data_fd2_battle_floating_damage_sprite_id_queue @ 0x53C6C (uint8[200])
 *
 * Runtime scratch queue holding the per-glyph sprite id for the floating
 * damage-number / MISS overlay enqueued above a target's head during a battle
 * spell/attack. First of three contiguous, equal-length (200-byte) parallel
 * queues: sprite_id @ 0x53C6C, x_offset @ 0x53D34 (= 0x53C6C+200), and
 * target_char_idx @ 0x53DFC (= 0x53D34+200); each is a separate symbol. The
 * exact 200-byte extent is fixed by the 0xC8 gap to the next queue. Zero-
 * initialized (.bss): every slot is written before it is read, so the static
 * contents are don't-care (memory reads as 0x00 x200).
 *
 * Producers append 4 entries per call at the running queue index
 * data_fd2_battle_spell_aoe_count_and_fx_queue_idx (advanced by 4 each call):
 *   fd2_show_damage_number (@ 0x1E1C7): per digit, slot = marker_char + digit
 *     value - '0' when the place is significant, else 0 (blank place).
 *   fd2_show_miss_indicator (@ 0x1E258): copies a 4-byte "MISS" sprite run:
 *       MOV byte ptr [EDX + EAX*1 + 0x53C6C], BL   ; 8-bit store, stride 1
 * Consumer fd2_animate_spell_projectile_paths (@ 0x1E012) walks slots
 * 0..count-1, skipping 0 (= empty/finished), and uses the value (zero-extended)
 * as a sprite-sheet entry index:
 *       MOVZX EDX, byte ptr [ESI + 0x53C6C]        ; unsigned byte load, stride 1
 *       TEST  EDX, EDX  / JZ ...                    ; 0 -> skip
 *       MOV   ECX, dword ptr [EDX + EAX*4 + 0x6]    ; sprite_id used as *4 index
 * Both the 8-bit MOV stores and the MOVZX (zero-extending) load prove an
 * unsigned 1-byte element with stride 1, so the element type is uint8.
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_floating_damage_sprite_id_queue[200];

/* ----------------------------------------------------------------
 * data_fd2_battle_floating_damage_x_offset_queue @ 0x53D34 (uint8[200])
 *
 * Runtime scratch queue holding the per-glyph horizontal pixel x-offset for the
 * floating damage-number / MISS overlay enqueued above a target's head during a
 * battle spell/attack. Second of three contiguous, equal-length (200-byte)
 * parallel queues: sprite_id @ 0x53C6C, x_offset @ 0x53D34 (= 0x53C6C+200), and
 * target_char_idx @ 0x53DFC (= 0x53D34+200); each is a separate symbol. The
 * exact 200-byte extent is fixed by the 0xC8 gap to the next queue at 0x53DFC.
 * Zero-initialized (.bss): every slot is written before it is read, so the
 * static contents are don't-care (memory reads as 0x00 x200).
 *
 * Producers append 4 entries per call at the running queue index
 * data_fd2_battle_spell_aoe_count_and_fx_queue_idx (advanced by 4 each call):
 *   fd2_show_damage_number (@ 0x1E0DB): per digit, slot = digit_iter * 5 + 2
 *     (5 px per digit, horizontal layout).
 *   fd2_show_miss_indicator (@ 0x1E1E6): per glyph, slot = 8 for the middle
 *     glyph (char_iter == 1), else char_iter * 5 + 2.
 * Consumer fd2_animate_spell_projectile_paths (@ 0x1E060) walks slots
 * 0..count-1 and adds the value (zero-extended) into the framebuffer
 * destination pointer as a raw horizontal pixel offset:
 *       MOVZX EAX, byte ptr [ESI + 0x53D34]        ; unsigned byte load, stride 1
 *       ADD   EBX, EAX                              ; += x-offset
 * The 8-bit MOV stores in the producers and the MOVZX (zero-extending) load in
 * the consumer prove an unsigned 1-byte element with stride 1, so the element
 * type is uint8. The three queues share the same index, so the element width
 * matches the sibling sprite_id / target_char_idx queues (all uint8).
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_floating_damage_x_offset_queue[200];

/* ----------------------------------------------------------------
 * data_fd2_battle_floating_damage_target_char_idx_queue @ 0x53DFC (uint8[200])
 *
 * Runtime scratch queue holding the per-glyph target runtime_char index for the
 * floating damage-number / MISS overlay enqueued above a target's head during a
 * battle spell/attack. Third (last) of three contiguous, equal-length (200-byte)
 * parallel queues: sprite_id @ 0x53C6C, x_offset @ 0x53D34 (= 0x53C6C+200), and
 * target_char_idx @ 0x53DFC (= 0x53D34+200); each is a separate symbol. The
 * exact 200-byte extent is fixed by the 0xC8 gap to the next symbol
 * data_fd2_battle_spell_aoe_count_and_fx_queue_idx @ 0x53EC4. Zero-initialized
 * (.bss): every slot is written before it is read, so the static contents are
 * don't-care (memory reads as 0x00 x200).
 *
 * Producers append 4 entries per call at the running queue index
 * data_fd2_battle_spell_aoe_count_and_fx_queue_idx (advanced by 4 each call),
 * writing the target character index of the overlay being enqueued:
 *   fd2_show_damage_number (@ 0x1E19E): slot = (byte)target_char_idx (param_3):
 *       MOV byte ptr [EBX + EAX*1 + 0x53DFC], DL   ; 8-bit store, stride 1
 *   fd2_show_miss_indicator (@ 0x1E24E): slot = (byte)target_char_idx (param_1):
 *       MOV byte ptr [EDX + EAX*1 + 0x53DFC], CL   ; 8-bit store, stride 1
 * Consumer fd2_animate_spell_projectile_paths (@ 0x1E012) walks slots
 * 0..count-1 and uses the value as an index into the runtime char array to fetch
 * the target's board position when placing the glyph in the framebuffer:
 *       runtime_char_array[target_char_idx_queue[fx_iter]].bPos_x / .bPos_y
 * The 8-bit MOV stores in the producers and the byte-index use in the consumer
 * prove an unsigned 1-byte element with stride 1, so the element type is uint8.
 * The three queues share the same index, so the element width matches the
 * sibling sprite_id / x_offset queues (all uint8).
 * ---------------------------------------------------------------- */
uint8 data_fd2_battle_floating_damage_target_char_idx_queue[200];

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array @ 0x53FE4
 * (int32[6])
 *
 * Runtime per-slot frame counter for the variant-A 6-slot summon-spell
 * (召喚系) animation state machine. Zero-initialized (.bss): the static
 * contents are don't-care (memory reads as 0x00 x24). It is populated by the
 * state machine's INIT branch before any tick frame reads it, so the zero
 * fill is never observed at runtime.
 *
 * Sole accessor fd2_tick_summon_anim_variant_a_6slot (@ 0x269D3), the +0x10
 * (index 4) entry of the 10-element summon-spell tick dispatch table at
 * 0x523B9. On state_code == 0 (INIT) it staggers the start of each slot:
 *   for i in 0..5:  frame[i] = -2 * i        ; 0,-2,-4,-6,-8,-10
 * and on the tick states {2,5,8} it advances/wraps each slot:
 *   if (0 <= frame[i] && frame[i] < 7) blit ...
 *   frame[i] += 1; if (frame[i] == 8 && !terminate) frame[i] = 0; ...
 *
 * Element type is signed int32 with a 4-byte stride and exactly 6 elements,
 * proven by the consumer's disassembly:
 *   MOV  dword ptr [EAX*4 + 0x53FE4], EDX     ; INIT store, EDX = -2*i (signed)
 *   CMP  dword ptr [EAX   + 0x53FE4], 0x0 / JL ; signed lower-bound test (>= 0)
 *   CMP  dword ptr [EAX   + 0x53FE4], 0x7 / JGE; signed upper-bound test (< 7)
 *   INC  dword ptr [EAX*4 + 0x53FE4]          ; advance one frame
 * with the loop bound CMP ...,0x6 / JL fixing 6 slots. Every access is a whole
 * dword at stride 4, and the JL/JGE signed branches plus the negative INIT
 * values make the element a signed int32 (matches Ghidra int[6]). The adjacent
 * int32[6] at 0x53FFC (color_idx) and byte[6] at 0x54014 (jitter) are separate
 * sibling symbols of the same state machine.
 * ---------------------------------------------------------------- */
int32 data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[6];
