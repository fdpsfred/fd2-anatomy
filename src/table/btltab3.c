/*
 * btltab3.c -- battle summon-spell animation runtime state (.object2 BSS)
 *
 * Zero-initialized per-slot state arrays for the variant-A 6-slot summon-spell
 * animation state machine (fd2_tick_summon_anim_variant_a_6slot). All entries
 * start at zero and are filled at runtime during the INIT state (state_code==0)
 * before any TICK-state read; no static initializer in the original binary.
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array @ 0x53FFC  (24 bytes)
 *
 * Per-slot color index (one int32 per slot, 6 slots). Written by
 * fd2_tick_summon_anim_variant_a_6slot: INIT seeds slot i with i; the color
 * rotation path stores (rotation_counter % 10). Read in TICK frames as the
 * index into the local per-color vertical row-offset table. Accessed via
 * "[idx*4 + 0x53FFC]" DWORD moves (see disasm 0x26A4E / 0x26B0C / 0x26BB8),
 * confirming int32 elements with stride 4. Zero-bss (runtime-initialized).
 */
int32 data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[6];

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array @ 0x54014  (6 bytes)
 *
 * Per-slot vertical jitter byte (one uint8 per slot, 6 slots). Written by
 * fd2_tick_summon_anim_variant_a_6slot: both the INIT state and the per-slot
 * frame-8 wrap path store (rng % 2) * 7, i.e. 0 or 7. Read in TICK frames as
 * an additive offset to the sprite frame index passed to fd2_blit_indexed_sprite.
 * Accessed via byte moves "MOV byte ptr [idx + 0x54014],AL" (writes 0x26A71 /
 * 0x26BE4) and "MOVZX EDX, byte ptr [idx + 0x54014]" (read 0x26B1F), confirming
 * uint8 elements with stride 1, read zero-extended (unsigned). First access is a
 * write (INIT) -> zero-bss (runtime-initialized, all-zero static storage).
 */
uint8 data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[6];

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array @ 0x5401C  (24 bytes)
 *
 * Per-slot frame counter (one int32 per slot, 6 slots) for the variant-B 6-slot
 * summon-spell animation state machine (fd2_tick_summon_anim_variant_b_6slot).
 * Written by that function: INIT seeds slot i with -2*i (so 0,-2,-4,..,-10); the
 * TICK states (2/5/8) increment it, wrap it to 0 at frame 7, and gate blitting on
 * "0 <= counter < 6". Signed values arise (negative seed and signed "< 0" test),
 * so elements are signed int32. Accessed via DWORD moves "[idx*4 + 0x5401C]"
 * (writes 0x26C71 / 0x26D7B, read-modify-write 0x26D22, signed compares 0x26D29 /
 * 0x26D3F / 0x26DB9 / 0x26DC2 / 0x26E02), confirming int32 elements with stride 4
 * across a 6-iteration loop (CMP ...,6; JL/JGE). First access is a write (INIT)
 * -> zero-bss (runtime-initialized, all-zero static storage).
 */
int32 data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[6];

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array @ 0x54034  (24 bytes)
 *
 * Per-slot color index (one int32 per slot, 6 slots) for the variant-B 6-slot
 * summon-spell animation state machine (fd2_tick_summon_anim_variant_b_6slot).
 * Written by that function: INIT seeds slot i with i; the frame-7 color-rotation
 * path stores (rotation_counter % 10). Read in TICK frames as the index into the
 * local per-color vertical row-offset table (aiStack_3c[color]). Values stay in
 * 0..9. Accessed via DWORD moves "[idx*4 + 0x54034]": writes 0x26C78 (store i)
 * and 0x26D75 (store rotation index), read 0x26DD1 (used as a *4-scaled array
 * index), confirming int32 elements with stride 4 across a 6-iteration loop
 * (CMP ...,6; JL/JGE). First access is a write (INIT) -> zero-bss
 * (runtime-initialized, all-zero static storage).
 */
int32 data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[6];

/* ----------------------------------------------------------------
 * data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array @ 0x5404C  (6 bytes)
 *
 * Per-slot vertical jitter byte (one uint8 per slot, 6 slots) for the variant-B
 * 6-slot summon-spell animation state machine (fd2_tick_summon_anim_variant_b_6slot).
 * Written by that function: both the INIT state and the per-slot frame-7 wrap path
 * store (rng % 2) * 6, i.e. 0 or 6 (variant-A uses 0 or 7). Read in TICK frames as
 * an additive offset to the sprite frame index passed to fd2_blit_indexed_sprite.
 * Accessed via byte moves "MOV byte ptr [idx + 0x5404C],DL" (writes 0x26C97 /
 * 0x26D9D) and "MOVZX EDX, byte ptr [idx + 0x5404C]" (read 0x26DE4), confirming
 * uint8 elements with stride 1, read zero-extended (unsigned), across a
 * 6-iteration loop (CMP ...,6; JL/JGE). First access is a write (INIT) -> zero-bss
 * (runtime-initialized, all-zero static storage).
 */
uint8 data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[6];

/* ----------------------------------------------------------------
 * data_fd2_battle_pathfind_step_stack @ 0x60079  (256 bytes)
 *
 * Scratch recursion step-stack shared by the two battle pathfinding engines.
 * It is plain working memory: written before read on every search, never
 * initialized at load time (all-zero static storage is incidental) -> zero-bss.
 *
 * The region is overlaid with two different per-frame entry layouts depending
 * on which engine drives the recursion, so the honest C representation is a
 * flat byte buffer rather than a struct array:
 *
 *   - Path-search engine fd2_pathfind_recursive_with_direction (0x4E27C):
 *     8-byte frames. EDI is seeded with LEA EDI,[0x60079] in
 *     fd2_pathfind_check_destination_save_path (0x4E252) before the first
 *     recursive call. Each push writes "MOV word ptr [EDI],DX" (tile x,y at +0),
 *     "MOV word ptr [EDI+0x2],CX" (CH at +3 = direction code 0..3),
 *     "MOV dword ptr [EDI+0x4],EBX" (tile-map address at +4), then "ADD EDI,0x8".
 *     The retry path rewrites the previous frame's +3 direction via
 *     "MOV byte ptr [EDI-0x5],CH". Pop is "SUB DI,0x8".
 *
 *   - Flood-fill engine fd2_flood_fill_movement_range_recursive (0x4E0DC):
 *     7-byte frames. Each push writes "MOV word ptr [EDI],DX" (+0),
 *     "MOV byte ptr [EDI+0x2],CL" (+2), "MOV dword ptr [EDI+0x3],EBX" (+3),
 *     then "ADD EDI,0x7"; pop is "SUB DI,0x7".
 *
 * Readers fd2_pathfind_count_unique_directions (0x4E3DA) and
 * fd2_pathfind_check_destination_save_path (0x4E42B) walk the path-search
 * frames with "LEA ESI,[0x60079]" + "MOV AL,byte ptr [ESI+0x3]" + "ADD ESI,0x8",
 * looping data_fd2_battle_pathfind_current_depth (0x60077) times to collect the
 * per-frame direction bytes. 256 bytes bounds the recursion depth (>=32 frames
 * at the larger 8-byte stride). First access on every search is a write
 * -> zero-bss (runtime-initialized).
 */
uint8 data_fd2_battle_pathfind_step_stack[256];

/* ----------------------------------------------------------------
 * data_fd2_battle_item_effect_table @ 0x602AC  (215 entries x 23 bytes = 4945)
 *
 * Read-only item stat/effect table (struct item_effect, 23-byte stride). One
 * entry per item id 0..0xD6 (215 items). Accessed exclusively through the
 * accessor fd2_get_item_effect_entry (0x4E56C), whose body is
 * "EAX = item_id * 0x17; return 0x602AD + EAX" (= &table[item_id].type, i.e.
 * the returned pointer P aims at field +1, the type byte). The 22 callers
 * (combat / AI / inventory / shop paths) index later fields by byte/word
 * offset off P, so P-relative +k maps to struct offset (k+1):
 *   P[+0x0D]=use_effect (offensive flag), P[+0x10]=cast_range_flags
 *   (range_class: <0x10 short / >=0x10 projectile), P[+0x11]=target_side
 *   (aoe_target_flag), P[+0x12]=area (aoe_shape) -- all read as *(byte *)
 *   (see fd2_execute_ai_item_use 0x15055, fd2_apply_use_effect_dispatch
 *   0x20C6F). u16 fields (ap/ht/dp/ev/price) are read with word moves. No
 *   writer exists; const data table.
 *
 * Field order (pack(1), little-endian) per struct item_effect in types.h:
 *   unknown_00, type, ap, ht, dp, ev, special_type, special_chance,
 *   range_min, range_max, use_effect, use_param_lo, use_param_hi,
 *   cast_range_flags, target_side, area, price, trailing_22.
 * Bytes are byte-exact from FD2.LE .object3 @ 0x602AC.
 */
const item_effect data_fd2_battle_item_effect_table[215] = {
    { 11,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,20,95,0,0,0,0,1,1,0,0,0,0,5,0,250,5 },
    { 0,1,40,90,0,0,0,0,1,1,0,0,0,0,5,0,800,5 },
    { 0,1,65,85,0,0,0,0,1,1,0,0,0,0,5,0,1500,5 },
    { 0,1,90,85,0,0,2,10,1,1,0,0,0,0,5,0,2400,5 },
    { 0,1,110,90,0,0,0,0,1,1,0,0,0,0,5,0,4200,5 },
    { 0,1,135,90,0,0,0,0,1,1,0,0,0,0,5,0,7800,5 },
    { 0,1,150,120,0,0,4,30,1,1,0,0,0,0,5,0,9600,5 },
    { 0,1,240,100,0,0,0,0,1,1,0,0,0,0,5,0,12000,5 },
    { 0,1,280,100,0,0,0,0,1,1,0,0,0,0,5,0,18000,5 },
    { 0,1,320,120,0,0,0,0,1,1,0,0,0,0,5,0,25000,5 },
    { 0,1,400,120,30,0,4,30,1,1,20,2,0,3,0,1,2,0 },
    { 0,1,8,100,0,0,0,0,1,1,0,0,0,0,5,0,30,5 },
    { 0,1,20,90,0,0,0,0,1,1,0,0,0,0,5,0,200,5 },
    { 0,1,50,95,0,0,2,30,1,1,0,0,0,0,5,0,1200,5 },
    { 0,1,80,120,0,0,0,0,1,1,0,0,0,0,5,0,2000,5 },
    { 0,1,120,120,0,0,0,0,1,1,0,0,0,0,5,0,5000,5 },
    { 0,1,150,100,30,0,0,0,1,1,0,0,0,0,5,0,8000,5 },
    { 0,1,165,90,0,0,4,5,1,1,0,0,0,0,5,0,8800,5 },
    { 0,1,200,120,0,0,0,0,1,1,0,0,0,0,5,0,14800,5 },
    { 0,3,20,90,0,0,0,0,1,2,0,0,0,0,5,0,80,5 },
    { 0,3,28,90,0,0,0,0,1,2,0,0,0,0,5,0,400,5 },
    { 0,3,50,85,0,0,0,0,1,2,0,0,0,0,5,0,1200,5 },
    { 0,3,85,90,0,0,0,0,1,2,0,0,0,0,5,0,1800,5 },
    { 0,3,120,85,0,0,0,0,1,2,0,0,0,0,5,0,4400,5 },
    { 0,3,150,100,0,0,0,0,1,2,0,0,0,0,5,0,6000,5 },
    { 0,3,190,90,0,0,0,0,1,2,0,0,0,0,5,0,8800,5 },
    { 0,3,240,100,0,0,0,0,1,2,0,0,0,0,5,0,11200,5 },
    { 0,3,280,110,0,0,0,0,1,2,0,0,0,0,5,0,16000,5 },
    { 0,3,400,120,20,0,0,0,1,2,21,6,0,3,0,2,2,0 },
    { 0,3,150,100,0,0,4,20,1,2,0,0,0,0,5,0,24000,5 },
    { 0,3,320,100,0,0,0,0,1,2,0,0,0,0,5,0,24000,5 },
    { 0,4,20,95,0,0,0,0,1,1,0,0,0,0,5,0,300,5 },
    { 0,4,35,90,0,0,0,0,1,2,0,0,0,0,5,0,800,5 },
    { 0,4,60,90,0,0,0,0,1,1,0,0,0,0,5,0,800,5 },
    { 0,4,90,85,0,0,0,0,1,1,0,0,0,0,5,0,2000,5 },
    { 0,4,120,85,0,0,0,0,1,1,0,0,0,0,5,0,3200,5 },
    { 0,4,145,85,0,0,0,0,1,1,0,0,0,0,5,0,5600,5 },
    { 0,4,180,100,0,0,0,0,1,1,21,1,0,2,0,1,12500,0 },
    { 0,4,225,100,0,0,4,10,1,1,0,0,0,0,5,0,12800,5 },
    { 0,4,300,100,0,0,0,0,1,1,13,100,0,2,1,1,15500,1 },
    { 0,4,340,100,0,0,0,0,1,1,0,0,0,0,5,0,22000,5 },
    { 0,4,380,100,0,0,0,0,1,1,0,0,0,0,5,0,28000,5 },
    { 0,4,400,80,0,0,4,80,1,1,0,0,0,0,5,0,2,5 },
    { 0,5,20,95,0,0,0,0,2,3,0,0,0,0,5,0,100,5 },
    { 0,5,40,90,0,0,0,0,2,4,0,0,0,0,5,0,500,5 },
    { 0,5,75,85,0,0,2,10,2,4,0,0,0,0,5,0,1800,5 },
    { 0,5,110,80,0,0,0,0,2,5,0,0,0,0,5,0,4800,5 },
    { 0,5,130,100,0,0,0,0,2,4,0,0,0,0,5,0,7200,5 },
    { 0,5,155,120,0,0,4,10,2,5,0,0,0,0,5,0,12000,5 },
    { 0,5,250,100,0,0,0,0,2,5,0,0,0,0,5,0,20000,5 },
    { 0,5,400,120,40,0,0,0,2,6,21,7,0,2,0,1,2,0 },
    { 0,6,8,85,0,0,0,0,1,1,0,0,0,0,5,0,20,5 },
    { 0,6,25,80,0,0,0,0,1,1,0,0,0,0,5,0,180,5 },
    { 0,6,60,80,0,0,0,0,1,1,0,0,0,0,5,0,640,5 },
    { 0,6,80,80,0,0,0,0,1,2,0,0,0,0,5,0,1000,5 },
    { 0,6,120,90,0,0,0,0,1,1,20,0,0,2,0,0,5600,0 },
    { 0,6,150,90,0,0,0,0,1,1,22,0,0,3,0,0,8800,0 },
    { 0,6,180,90,0,0,0,0,1,1,13,44,1,2,1,0,10000,1 },
    { 0,6,220,120,0,0,2,30,1,1,0,0,0,0,5,0,12800,5 },
    { 0,6,280,120,0,0,0,0,1,1,20,2,0,2,0,0,22400,0 },
    { 0,6,400,140,50,10,0,0,1,1,13,244,1,3,1,2,2,1 },
    { 0,7,20,100,0,0,0,0,1,1,0,0,0,0,5,0,200,5 },
    { 0,7,50,90,0,0,0,0,1,1,0,0,0,0,5,0,850,5 },
    { 0,7,80,95,0,0,0,0,1,1,0,0,0,0,5,0,1200,5 },
    { 0,7,100,90,0,0,2,20,1,1,0,0,0,0,5,0,3600,5 },
    { 0,7,175,95,0,0,2,20,1,1,0,0,0,0,5,0,6000,5 },
    { 0,7,150,110,0,0,0,0,1,1,0,0,0,0,5,0,7800,5 },
    { 0,7,200,120,0,0,0,0,1,1,0,0,0,0,5,0,11200,5 },
    { 0,7,240,100,0,0,4,20,1,1,0,0,0,0,5,0,13200,5 },
    { 0,7,280,110,0,0,0,0,1,1,0,0,0,0,5,0,17500,5 },
    { 0,7,400,140,30,0,3,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,8,15,90,0,0,0,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,8,100,85,0,0,0,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,8,240,80,0,0,0,0,1,2,0,0,0,0,5,0,2,5 },
    { 0,8,360,100,0,0,0,0,1,2,0,0,0,0,5,0,2,5 },
    { 0,9,200,150,0,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,10,200,150,0,0,0,0,2,6,0,0,0,0,5,0,0,5 },
    { 0,11,200,150,0,0,0,0,1,2,0,0,0,0,5,0,0,5 },
    { 0,12,450,80,0,0,0,0,0,0,24,3,0,31,0,4,0,0 },
    { 0,13,115,100,0,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,14,200,110,0,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,15,300,70,0,0,0,0,1,2,0,0,0,0,5,0,0,5 },
    { 0,16,240,130,0,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,17,200,90,0,0,0,0,1,3,0,0,0,0,5,0,0,5 },
    { 0,18,200,120,0,0,0,0,1,2,0,0,0,0,5,0,0,5 },
    { 0,19,250,120,0,0,0,0,1,2,0,0,0,0,5,0,0,5 },
    { 0,20,360,150,0,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,34,0,0,0,0,0,0,0,0,0,0,0,0,5,0,5000,5 },
    { 0,34,0,0,0,0,0,0,0,0,0,0,0,0,5,0,5000,5 },
    { 0,34,0,0,0,0,0,0,0,0,0,0,0,0,5,0,5000,5 },
    { 0,34,0,0,0,0,0,0,0,0,0,0,0,0,5,0,5000,5 },
    { 0,34,0,0,0,0,0,0,0,0,0,0,0,0,5,0,5000,5 },
    { 0,34,0,0,0,0,0,0,1,1,0,0,0,0,5,0,5000,5 },
    { 0,32,0,0,0,0,0,0,0,0,17,20,0,0,1,0,10000,1 },
    { 0,32,0,0,0,0,0,0,0,0,18,20,0,0,1,0,10000,1 },
    { 0,32,0,0,0,0,0,0,0,0,19,1,0,0,1,0,20000,1 },
    { 0,5,180,90,0,0,0,0,2,4,0,0,0,0,5,0,8800,5 },
    { 0,5,210,90,0,0,0,0,2,5,0,0,0,0,5,0,13500,5 },
    { 0,1,200,95,0,0,0,0,1,2,21,6,0,3,0,1,50,0 },
    { 0,34,0,0,0,0,0,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,6,200,100,0,0,0,0,1,1,23,1,0,1,1,0,60000,1 },
    { 0,32,0,0,0,0,0,0,0,0,5,244,1,4,1,2,1000,1 },
    { 0,1,350,170,0,0,0,0,1,3,0,0,0,0,5,0,2,5 },
    { 0,5,310,120,0,0,0,0,2,5,0,0,0,0,5,0,32000,5 },
    { 0,7,330,100,0,0,0,0,1,1,0,0,0,0,5,0,25800,5 },
    { 0,1,350,170,0,0,0,0,1,3,0,0,0,0,5,0,50,5 },
    { 0,1,400,103,0,0,0,0,1,3,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,21,0,0,300,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,24,0,0,200,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,1,10,95,0,0,0,0,1,1,0,0,0,0,5,0,70,5 },
    { 0,21,0,0,40,5,0,0,0,0,0,0,0,0,5,0,50,5 },
    { 0,21,0,0,2,0,0,0,0,0,0,0,0,0,5,0,50,5 },
    { 0,21,0,0,10,0,0,0,0,0,0,0,0,0,5,0,500,5 },
    { 0,21,0,0,80,10,0,0,0,0,0,0,0,0,5,0,10000,5 },
    { 0,21,0,0,250,10,0,0,0,0,0,0,0,0,5,0,0,5 },
    { 0,22,0,0,8,0,0,0,1,1,0,0,0,0,5,0,300,5 },
    { 0,22,0,0,15,0,0,0,1,1,0,0,0,0,5,0,800,5 },
    { 0,22,0,0,36,5,0,0,1,1,0,0,0,0,5,0,3200,5 },
    { 0,22,0,0,48,5,0,0,1,1,0,0,0,0,5,0,4000,5 },
    { 0,22,0,0,64,5,0,0,1,1,0,0,0,0,5,0,7800,5 },
    { 0,22,0,0,80,5,0,0,1,1,0,0,0,0,5,0,9600,5 },
    { 0,22,0,0,96,5,0,0,1,1,0,0,0,0,5,0,12000,5 },
    { 0,22,0,0,112,5,0,0,1,1,0,0,0,0,5,0,14400,5 },
    { 0,22,0,0,140,5,0,0,1,1,0,0,0,0,5,0,20000,5 },
    { 0,22,0,0,156,5,0,0,1,1,0,0,0,0,5,0,25600,5 },
    { 0,22,0,0,178,0,0,0,1,1,0,0,0,0,5,0,30000,5 },
    { 0,22,0,0,200,10,0,0,1,1,0,0,0,0,5,0,42000,5 },
    { 0,23,0,0,24,0,0,0,1,1,0,0,0,0,5,0,1000,5 },
    { 0,23,0,0,40,0,0,0,1,1,0,0,0,0,5,0,3600,5 },
    { 0,23,0,0,56,0,0,0,1,1,0,0,0,0,5,0,5800,5 },
    { 0,23,0,0,72,0,0,0,1,1,0,0,0,0,5,0,8400,5 },
    { 0,23,0,0,88,0,0,0,1,1,0,0,0,0,5,0,10800,5 },
    { 0,23,0,0,100,5,0,0,1,1,0,0,0,0,5,0,13200,5 },
    { 0,23,0,0,120,0,0,0,1,1,0,0,0,0,5,0,15600,5 },
    { 0,23,0,0,144,0,0,0,1,1,0,0,0,0,5,0,22000,5 },
    { 0,23,0,0,172,0,0,0,1,1,0,0,0,0,5,0,28000,5 },
    { 0,23,0,0,196,0,0,0,1,1,0,0,0,0,5,0,36000,5 },
    { 0,23,0,0,220,0,0,0,1,1,0,0,0,0,5,0,45000,5 },
    { 0,23,0,0,240,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,24,0,0,120,0,0,0,1,1,0,0,0,0,5,0,12000,5 },
    { 0,24,0,0,136,0,0,0,1,1,0,0,0,0,5,0,15000,5 },
    { 0,24,0,0,160,0,0,0,1,1,0,0,0,0,5,0,20000,5 },
    { 0,24,0,0,192,0,0,0,1,1,0,0,0,0,5,0,24000,5 },
    { 0,24,0,0,200,0,0,0,1,1,0,0,0,0,5,0,30000,5 },
    { 0,24,0,0,230,5,0,0,1,1,0,0,0,0,5,0,42000,5 },
    { 0,24,0,0,260,0,0,0,1,1,0,0,0,0,5,0,50000,5 },
    { 0,24,0,0,300,20,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,25,0,0,5,0,0,0,1,1,0,0,0,0,5,0,200,5 },
    { 0,25,0,0,12,0,0,0,1,1,0,0,0,0,5,0,750,5 },
    { 0,25,0,0,32,0,0,0,1,1,0,0,0,0,5,0,1800,5 },
    { 0,25,0,0,60,5,0,0,1,1,0,0,0,0,5,0,6400,5 },
    { 0,25,0,0,92,10,0,0,1,1,0,0,0,0,5,0,11200,5 },
    { 0,25,0,0,128,10,0,0,1,1,0,0,0,0,5,0,16400,5 },
    { 0,25,0,0,150,10,0,0,1,1,0,0,0,0,5,0,18000,5 },
    { 0,25,0,0,210,15,0,0,1,1,0,0,0,0,5,0,32000,5 },
    { 0,26,0,0,10,5,0,0,1,1,0,0,0,0,5,0,400,5 },
    { 0,26,0,0,28,0,0,0,1,1,0,0,0,0,5,0,1200,5 },
    { 0,26,0,0,55,0,0,0,1,1,0,0,0,0,5,0,5600,5 },
    { 0,26,0,0,90,0,0,0,1,1,0,0,0,0,5,0,11000,5 },
    { 0,26,0,0,130,0,0,0,1,1,0,0,0,0,5,0,18000,5 },
    { 0,26,0,0,200,10,0,0,1,1,0,0,0,0,5,0,34000,5 },
    { 0,27,0,0,8,0,0,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,27,0,0,80,10,0,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,27,0,0,150,0,0,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,27,0,0,180,0,0,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,28,0,0,80,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,28,0,0,160,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,29,0,0,260,0,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,30,0,0,200,10,0,0,1,1,0,0,0,0,5,0,0,5 },
    { 0,31,0,0,300,10,0,0,0,0,0,0,0,0,5,0,0,5 },
    { 0,21,0,0,60,5,0,0,1,1,0,0,0,0,5,0,50,5 },
    { 0,26,0,0,170,5,0,0,0,0,0,0,0,0,5,0,20000,5 },
    { 0,25,0,0,185,5,0,0,0,0,0,0,0,0,5,0,20000,5 },
    { 0,25,0,0,210,5,0,0,1,1,0,0,0,0,5,0,24000,5 },
    { 0,27,0,0,280,0,0,0,1,1,0,0,0,0,5,0,2,5 },
    { 0,32,0,0,0,0,0,0,0,0,5,40,0,1,1,0,10,1 },
    { 0,32,0,0,0,0,0,0,0,0,5,120,0,1,1,0,80,1 },
    { 0,32,0,0,0,0,0,0,0,0,5,44,1,1,1,0,300,1 },
    { 0,32,0,0,0,0,0,0,0,0,5,231,3,1,1,0,800,1 },
    { 0,32,0,0,0,0,0,0,0,0,6,0,0,1,1,0,20,1 },
    { 0,32,0,0,0,0,0,0,0,0,7,0,0,1,1,0,20,1 },
    { 0,32,0,0,0,0,0,0,0,0,8,9,0,0,1,0,10000,1 },
    { 0,32,0,0,0,0,0,0,0,0,9,9,0,0,1,0,10000,1 },
    { 0,32,0,0,0,0,0,0,0,0,10,7,0,0,1,0,10000,1 },
    { 0,33,0,0,0,0,0,0,0,0,0,0,0,0,5,0,1000,5 },
    { 0,33,0,0,0,0,0,0,0,0,0,0,0,0,5,0,5000,5 },
    { 0,33,0,0,0,0,0,0,0,0,0,0,0,0,5,0,20000,5 },
    { 0,33,0,0,0,0,0,0,0,0,0,0,0,0,5,0,50000,5 },
    { 0,34,0,0,0,0,0,0,0,0,0,0,0,0,5,0,100,5 },
    { 0,32,0,0,0,0,0,0,0,0,11,80,0,1,1,0,1000,1 },
    { 0,32,0,0,0,0,0,0,0,0,11,200,0,1,1,0,5000,1 },
    { 0,34,0,0,0,0,0,0,0,0,0,0,0,0,5,0,100,5 },
    { 0,34,0,0,0,0,0,0,0,0,0,0,0,0,5,0,2,5 },
    { 0,34,0,0,0,0,0,0,0,0,12,0,0,4,1,1,2,1 },
    { 0,34,0,0,0,0,0,0,0,0,13,200,0,4,1,1,2,1 },
    { 0,34,0,0,0,0,0,0,0,0,14,0,0,4,0,1,2,0 },
    { 0,34,0,0,0,0,0,0,0,0,15,0,0,4,1,1,2,1 },
    { 0,34,0,0,0,0,0,0,0,0,16,0,0,4,1,1,2,1 },
};
