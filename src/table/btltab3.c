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

/* ----------------------------------------------------------------
 * data_fd2_battle_class_promotion_data_table @ 0x615FE  (72 bytes, .object3)
 *
 * Class-promotion result table. 36 entries x 2 bytes, indexed by
 * (target_class_id - 0x20) where target_class_id is a promoted-class portrait
 * id in [0x20, 0x43]. Accessor fd2_get_class_promotion_data_entry @ 0x4E48D
 * returns &table[(class_id - 0x20)*2]; callers read only byte[0] and byte[1]
 * (8-bit MOVs), so element type is uint8 and stride is 2.
 *   byte[0] = post-promotion job_id (00h..1Ah job-id space)
 *   byte[1] = learned-spell id on promotion (0 = none)
 * Read-only const (no writers). Index bound 0x43 is set by
 * fd2_build_promotion_candidates_with_targets @ 0x31793, whose target class is
 * portrait_id+0x20 (default) / portrait_id+0x32 (alt key-item path) / 0x34
 * (Lord), with portrait_id in [0,0x12); max = 0x11+0x32 = 0x43. Table ends at
 * 0x61646 where data_fd2_battle_movement_cost_table begins. The last two
 * entries (class 0x42/0x43) are alt-path-only promotions absent from the
 * named-portrait guide list but proven reachable by the builder.
 *
 * Readers (all via the accessor): fd2_run_class_promotion_menu_main @ 0x31385,
 * fd2_execute_class_promotion_with_dialog @ 0x31602,
 * fd2_render_promote_candidates_grid @ 0x31019.
 */
const uint8 data_fd2_battle_class_promotion_data_table[72] = {
    /* idx  class  job   spell */
    0x09, 0x01,  /*  0  0x20  0x09 SwordMaster   1 */
    0x0A, 0x00,  /*  1  0x21  0x0A Paladin       - */
    0x09, 0x01,  /*  2  0x22  0x09 SwordMaster   1 */
    0x0A, 0x00,  /*  3  0x23  0x0A Paladin       - */
    0x0B, 0x01,  /*  4  0x24  0x0B HolyKnight    1 */
    0x0B, 0x01,  /*  5  0x25  0x0B HolyKnight    1 */
    0x0B, 0x01,  /*  6  0x26  0x0B HolyKnight    1 */
    0x0B, 0x01,  /*  7  0x27  0x0B HolyKnight    1 */
    0x0C, 0x01,  /*  8  0x28  0x0C Sniper        1 */
    0x0D, 0x01,  /*  9  0x29  0x0D ArchMage      1 */
    0x0E, 0x01,  /* 10  0x2A  0x0E Priest        1 */
    0x0E, 0x01,  /* 11  0x2B  0x0E Priest        1 */
    0x10, 0x00,  /* 12  0x2C  0x10 Gladiator     - */
    0x0C, 0x01,  /* 13  0x2D  0x0C Sniper        1 */
    0x0D, 0x01,  /* 14  0x2E  0x0D ArchMage      1 */
    0x10, 0x00,  /* 15  0x2F  0x10 Gladiator     - */
    0x0A, 0x00,  /* 16  0x30  0x0A Paladin       - */
    0x0F, 0x01,  /* 17  0x31  0x0F DragonKnightL 1 */
    0x11, 0x02,  /* 18  0x32  0x11 Hero          2 */
    0x12, 0x00,  /* 19  0x33  0x12 RuneWarrior   - */
    0x15, 0x02,  /* 20  0x34  0x15 Summoner      2 */
    0x12, 0x00,  /* 21  0x35  0x12 RuneWarrior   - */
    0x13, 0x02,  /* 22  0x36  0x13 DragonKnight  2 */
    0x13, 0x02,  /* 23  0x37  0x13 DragonKnight  2 */
    0x13, 0x02,  /* 24  0x38  0x13 DragonKnight  2 */
    0x13, 0x02,  /* 25  0x39  0x13 DragonKnight  2 */
    0x14, 0x01,  /* 26  0x3A  0x14 SharpShooter  1 */
    0x16, 0x01,  /* 27  0x3B  0x16 Saint         1 */
    0x16, 0x01,  /* 28  0x3C  0x16 Saint         1 */
    0x16, 0x01,  /* 29  0x3D  0x16 Saint         1 */
    0x18, 0x01,  /* 30  0x3E  0x18 WarSaint      1 */
    0x14, 0x01,  /* 31  0x3F  0x14 SharpShooter  1 */
    0x16, 0x01,  /* 32  0x40  0x16 Saint         1 */
    0x18, 0x01,  /* 33  0x41  0x18 WarSaint      1 */
    0x12, 0x00,  /* 34  0x42  0x12 RuneWarrior   -  (alt-path only) */
    0x17, 0x01   /* 35  0x43  0x17 Ninja         1  (alt-path only) */
};

/* ----------------------------------------------------------------
 * data_fd2_battle_movement_cost_table @ 0x61646  (580 bytes, .object3)
 *
 * Per-job movement-cost table: 29 rows x 20 bytes (uint8). Row r is the
 * cost vector for job r, indexed by tile-attribute class (0..19).
 * Accessor fd2_get_movement_cost_table_for_job @ 0x4E555 returns
 * &table[job_id*0x14]; job_id range is 0..0x1A (27 logical jobs, rows
 * 0..26). The physical symbol allocates 29 rows (580 bytes) ending
 * exactly at 0x6188A where data_fd2_battle_job_allowed_items_table
 * begins; rows 27..28 are reserved/extra slots past the accessor range.
 *
 * Element type and stride are proven by the consumer
 * fd2_flood_fill_neighbor_step @ 0x4E16E: "SUB CL, byte ptr [ESI+EAX*1]"
 * subtracts cost_table[tile_attr] (single-byte read, stride 1) from the
 * residual movement budget. Value meaning: 1 = normal step, 2..3 =
 * costlier terrain, 20 (0x14) = effectively impassable (drains budget).
 * Read-only const (no writers).
 *
 * Readers (the row pointer is forwarded into floodfill/pathfind):
 * fd2_player_action_menu_loop @ 0x18890, fd2_wait_for_action_target_input
 * @ 0x115B6, fd2_ai_score_physical_attack @ 0x14237,
 * fd2_ai_score_item_use @ 0x1567E, fd2_ai_score_offensive_spell @ 0x1598A,
 * fd2_ai_seek_optimal_position @ 0x14121, fd2_ai_walk_to_target_tile
 * @ 0x14B78, fd2_compute_aoe_targets @ 0x14818.
 */
const uint8 data_fd2_battle_movement_cost_table[580] = {
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  0 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  1 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  2 */
     1,20, 2, 3, 3,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  3 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  4 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  5 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  6 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  7 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  8 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job  9 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 10 */
     1,20, 2, 3, 3,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 11 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 12 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 13 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 14 */
     1, 1, 1, 1, 1,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 15 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 16 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 17 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 18 */
     1, 1, 1, 1, 1,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 19 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 20 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 21 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 22 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 23 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 24 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 25 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 26 */
     1,20, 1, 2, 2,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* job 27 */
    20,20,20,20, 1,20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  /* job 28 */
};

/*
 * data_fd2_battle_job_allowed_items_table @ 0x6188A  (203 bytes, .object3)
 *
 * Per-job equippable item-type table: 29 rows x 7 bytes (uint8). Row r holds
 * up to 6 allowed item-type IDs for job r (slot value 0xFF = unused), plus a
 * trailing constant 0x01 row marker at offset 6 (not consumed by the
 * accessor). Item-type IDs match the item-category byte at offset 0 of each
 * item_effect entry (e.g. 01=sword, 04=bow, 06=staff, 15=rod, 16=book...).
 *
 * Element type and 7-byte stride are proven by the accessor
 * fd2_get_job_allowed_items_table_entry @ 0x4E53E, which returns
 * &table[job_id*7], and its consumer fd2_check_job_can_equip_item @ 0x1C1C3:
 * it loads the row pointer, then scans offsets 0..5 with a single-byte read
 * "*(char *)(allowed_types + type_iter)" comparing each against the item's
 * category byte (type_iter 0..5, loop bound "5 < type_iter"). Single reader,
 * no writers -> read-only const.
 *
 * job_id range is 0..0x1A (27 logical jobs, rows 0..26). The physical symbol
 * allocates 29 rows (203 bytes) ending exactly at 0x61955 where
 * data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21 begins; rows 27..28
 * are reserved/extra slots past the accessor range.
 */
const uint8 data_fd2_battle_job_allowed_items_table[29 * 7] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x01,  /* job  0 */
    0x01,0x15,0x16,0xFF,0xFF,0xFF,0x01,  /* job  1 */
    0x04,0x15,0x16,0x17,0x18,0xFF,0x01,  /* job  2 */
    0x03,0x15,0x16,0x17,0xFF,0xFF,0x01,  /* job  3 */
    0x05,0x15,0x16,0xFF,0xFF,0xFF,0x01,  /* job  4 */
    0x06,0x15,0x19,0xFF,0xFF,0xFF,0x01,  /* job  5 */
    0x06,0x15,0x19,0xFF,0xFF,0xFF,0x01,  /* job  6 */
    0x02,0x15,0x16,0xFF,0xFF,0xFF,0x01,  /* job  7 */
    0x07,0x15,0x1A,0x0D,0xFF,0xFF,0x01,  /* job  8 */
    0x01,0x09,0x15,0x16,0xFF,0xFF,0x01,  /* job  9 */
    0x04,0x15,0x16,0x17,0x18,0xFF,0x01,  /* job 10 */
    0x03,0x0A,0x15,0x16,0x17,0x18,0x01,  /* job 11 */
    0x05,0x0C,0x15,0x16,0xFF,0xFF,0x01,  /* job 12 */
    0x06,0x15,0x19,0xFF,0xFF,0xFF,0x01,  /* job 13 */
    0x06,0x15,0x19,0xFF,0xFF,0xFF,0x01,  /* job 14 */
    0x01,0x0B,0x15,0x16,0xFF,0xFF,0x01,  /* job 15 */
    0x07,0x15,0x1A,0xFF,0xFF,0xFF,0x01,  /* job 16 */
    0x01,0x09,0x15,0x16,0x17,0xFF,0x01,  /* job 17 */
    0x04,0x15,0x16,0x17,0x18,0xFF,0x01,  /* job 18 */
    0x03,0x15,0x16,0x17,0xFF,0xFF,0x01,  /* job 19 */
    0x05,0x15,0x16,0x17,0xFF,0xFF,0x01,  /* job 20 */
    0x06,0x15,0x19,0xFF,0xFF,0xFF,0x01,  /* job 21 */
    0x06,0x15,0x19,0xFF,0xFF,0xFF,0x01,  /* job 22 */
    0x01,0x0B,0x15,0x16,0xFF,0xFF,0x01,  /* job 23 */
    0x07,0x15,0x1A,0xFF,0xFF,0xFF,0x01,  /* job 24 */
    0x08,0x1B,0xFF,0xFF,0xFF,0xFF,0x01,  /* job 25 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x01,  /* job 26 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x01,  /* job 27 */
    0x07,0x15,0x1A,0xFF,0xFF,0xFF,0x01   /* job 28 */
};

/* ----------------------------------------------------------------
 * data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21 @ 0x61955  (84 bytes)
 * data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b @ 0x619A9  (84 bytes)
 *
 * Per-weapon-type attack-hit animation scripts. 21-entry pointer table indexed
 * by weapon attack-pattern id (0..20); each entry points into the immediately
 * following 84-byte script pool. Read-only: the sole table reader is
 * fd2_get_attack_anim_pattern_for_weapon @ 0x4E536 (returns
 * table[weapon_type], asm "MOV EAX,[EBX*4 + 0x61955]"); the pool reader is
 * fd2_animate_attack_hit_sequence @ 0x1E98C, which dereferences the returned
 * pointer as a byte stream:
 *     step_count = p[0];
 *     for step 0..step_count: sprite_id = p[step*2+1]; sfx_id = p[step*2+2];
 *     (sfx_id 0xFF means "no sound").
 * No writers anywhere -> const. Each pool record is
 * { step_count, (sprite_id, sfx_id) * step_count } so its length is
 * 1 + 2*step_count bytes; records are packed and shared across multiple weapon
 * types (several table slots point at the same record).
 *
 * Pool record start offsets (raw table dword targets -> offset into pool):
 *   0x619A9 -> +0   0x619B6 -> +13  0x619C3 -> +26
 *   0x619D0 -> +39  0x619E1 -> +56  0x619F2 -> +73
 *
 * Emitted as a named data-ptr table: the pool is its own symbol and the table
 * entries reference it via &pool[offset] (Layer-2 equivalent; linker places
 * both, original byte layout has table directly preceding pool).
 */
const uint8 data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[84] = {
    /* +0  record A (step_count 6) */
    0x06, 0x00,0xFF, 0x01,0x00, 0x02,0xFF, 0x03,0xFF, 0x04,0xFF, 0x05,0xFF,
    /* +13 record B (step_count 6) */
    0x06, 0x06,0x03, 0x07,0x03, 0x08,0x03, 0x09,0xFF, 0x0A,0xFF, 0x0B,0xFF,
    /* +26 record C (step_count 6) */
    0x06, 0x0C,0xFF, 0x0D,0x01, 0x0E,0xFF, 0x0F,0xFF, 0x10,0xFF, 0x11,0xFF,
    /* +39 record D (step_count 8) */
    0x08, 0x12,0x02, 0x13,0x02, 0x14,0x02, 0x15,0xFF, 0x16,0xFF, 0x17,0xFF, 0x18,0xFF, 0x19,0xFF,
    /* +56 record E (step_count 8) */
    0x08, 0x1F,0x05, 0x20,0x05, 0x21,0x05, 0x22,0xFF, 0x23,0xFF, 0x24,0xFF, 0x25,0xFF, 0x26,0xFF,
    /* +73 record F (step_count 5) */
    0x05, 0x1A,0xFF, 0x1B,0x03, 0x1C,0xFF, 0x1D,0xFF, 0x1E,0xFF
};

const uint8 *data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21[21] = {
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[0],   /* idx  0 -> 0x619A9 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[0],   /* idx  1 -> 0x619A9 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[0],   /* idx  2 -> 0x619A9 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[13],  /* idx  3 -> 0x619B6 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[26],  /* idx  4 -> 0x619C3 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[39],  /* idx  5 -> 0x619D0 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[56],  /* idx  6 -> 0x619E1 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[73],  /* idx  7 -> 0x619F2 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[56],  /* idx  8 -> 0x619E1 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[0],   /* idx  9 -> 0x619A9 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[13],  /* idx 10 -> 0x619B6 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[39],  /* idx 11 -> 0x619D0 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[0],   /* idx 12 -> 0x619A9 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[56],  /* idx 13 -> 0x619E1 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[73],  /* idx 14 -> 0x619F2 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[56],  /* idx 15 -> 0x619E1 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[56],  /* idx 16 -> 0x619E1 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[13],  /* idx 17 -> 0x619B6 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[0],   /* idx 18 -> 0x619A9 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[56],  /* idx 19 -> 0x619E1 */
    &data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[56]   /* idx 20 -> 0x619E1 */
};

/* ----------------------------------------------------------------
 * data_fd2_battle_spell_effect_table @ 0x619FD  (36 entries x 7 bytes = 252)
 *
 * Read-only spell stat/effect table (struct spell_effect, 7-byte stride). One
 * entry per spell id 0..0x23 (36 spells). Accessed exclusively through the
 * accessor fd2_get_spell_effect_entry (0x4E516), whose body is
 * "EAX = spell_id * 7; return 0x619FD + EAX" (= &table[spell_id]). The 10
 * caller functions (combat / AI / cast / MP / draw paths) read fields off the
 * returned pointer at their struct byte offsets:
 *   +0 damage  read as *(short *) (signed 16-bit) -- fd2_calc_magic_damage
 *              (0x1C79E), fd2_apply_heal_spell_to_target (0x1C8FC)
 *   +2 hit_rate read as *(byte *)  -- fd2_calc_magic_damage chance_pct
 *   +5 mp_cost  read as *(byte *)  -- fd2_deduct_caster_mp (0x1CA98),
 *              fd2_draw_spell_selection_list (0x1CF7F)
 * No writer exists; const data table.
 *
 * Field order (pack(1), little-endian) per struct spell_effect in types.h:
 *   damage(u16), hit_rate, cast_range_flags, area, mp_cost, target_side.
 * Bytes are byte-exact from FD2.LE .object3 @ 0x619FD.
 */
const spell_effect data_fd2_battle_spell_effect_table[36] = {
    { 50, 90, 5, 0, 2, 0 },  /* 0x00 */
    { 120, 90, 5, 0, 6, 0 },  /* 0x01 */
    { 250, 90, 5, 1, 20, 0 },  /* 0x02 */
    { 500, 85, 5, 1, 42, 0 },  /* 0x03 */
    { 40, 85, 4, 1, 4, 0 },  /* 0x04 */
    { 100, 80, 4, 1, 15, 0 },  /* 0x05 */
    { 220, 80, 4, 2, 30, 0 },  /* 0x06 */
    { 450, 80, 4, 2, 60, 0 },  /* 0x07 */
    { 440, 100, 8, 0, 24, 0 },  /* 0x08 */
    { 999, 50, 3, 0, 30, 0 },  /* 0x09 */
    { 80, 95, 0, 5, 18, 0 },  /* 0x0A */
    { 160, 90, 0, 7, 45, 0 },  /* 0x0B */
    { 340, 90, 0, 9, 80, 0 },  /* 0x0C */
    { 70, 0, 4, 0, 3, 1 },  /* 0x0D */
    { 140, 0, 4, 1, 10, 1 },  /* 0x0E */
    { 260, 0, 5, 2, 20, 1 },  /* 0x0F */
    { 500, 0, 5, 3, 40, 1 },  /* 0x10 */
    { 0, 0, 4, 2, 5, 1 },  /* 0x11 */
    { 0, 0, 4, 2, 5, 1 },  /* 0x12 */
    { 0, 0, 4, 2, 8, 1 },  /* 0x13 */
    { 0, 0, 4, 2, 5, 1 },  /* 0x14 */
    { 0, 0, 4, 2, 5, 1 },  /* 0x15 */
    { 0, 0, 4, 2, 8, 0 },  /* 0x16 */
    { 0, 0, 3, 0, 20, 3 },  /* 0x17 */
    { 0, 0, 5, 1, 22, 0 },  /* 0x18 */
    { 0, 0, 3, 1, 24, 1 },  /* 0x19 */
    { 10, 50, 4, 2, 8, 0 },  /* 0x1A */
    { 10, 50, 4, 2, 10, 0 },  /* 0x1B */
    { 0, 0, 1, 0, 22, 0 },  /* 0x1C */
    { 0, 0, 0, 2, 26, 0 },  /* 0x1D */
    { 0, 0, 20, 0, 24, 0 },  /* 0x1E */
    { 0, 0, 0, 2, 26, 0 },  /* 0x1F */
    { 800, 90, 5, 3, 76, 0 },  /* 0x20 */
    { 0, 0, 5, 3, 52, 1 },  /* 0x21 */
    { 0, 0, 5, 3, 28, 1 },  /* 0x22 */
    { 0, 0, 4, 2, 36, 0 },  /* 0x23 */
};

/* ----------------------------------------------------------------
 * data_fd2_battle_enemy_data_table @ 0x61AF9  (680 bytes = 68 x 10)
 *
 * Read-only enemy stat table; 68 entries of struct enemy_data (10 bytes:
 * race_id, class_id, hp(uint16), mp, ap, dp, dx, mv, exp_reward).
 * Indexed by (enemy_class_id - 0x44) via fd2_get_enemy_data_entry @ 0x4E4FF
 * (base + idx*0xA). Callers read +2 as 16-bit hp (MOVZX word) and +4..+9 as
 * bytes; on battle spawn enemy HP/MP/AP/DP/DX = field * level, and +9
 * (exp_reward) grants XP on kill. Readers: fd2_init_runtime_char_for_battle,
 * fd2_execute_attack_damage_calculation, fd2_apply_damage_and_award_xp.
 * Bytes are byte-exact from FD2.LE .object3 @ 0x61AF9.
 */
const enemy_data data_fd2_battle_enemy_data_table[68] = {
    { 1, 2, 18, 0, 5, 2, 1, 4, 30 },  /* 0x00 */
    { 1, 3, 20, 0, 8, 2, 1, 7, 30 },  /* 0x01 */
    { 1, 2, 16, 0, 5, 2, 1, 4, 30 },  /* 0x02 */
    { 3, 8, 20, 0, 10, 6, 2, 5, 30 },  /* 0x03 */
    { 2, 12, 46, 0, 12, 7, 4, 5, 30 },  /* 0x04 */
    { 4, 15, 24, 0, 16, 11, 5, 7, 30 },  /* 0x05 */
    { 1, 2, 12, 0, 5, 2, 1, 4, 30 },  /* 0x06 */
    { 1, 2, 12, 0, 5, 2, 1, 4, 30 },  /* 0x07 */
    { 1, 2, 14, 0, 5, 4, 1, 4, 30 },  /* 0x08 */
    { 1, 2, 18, 0, 8, 4, 2, 4, 45 },  /* 0x09 */
    { 1, 2, 23, 0, 10, 5, 2, 3, 70 },  /* 0x0A */
    { 1, 2, 30, 0, 16, 7, 4, 4, 120 },  /* 0x0B */
    { 1, 2, 54, 0, 24, 9, 5, 4, 120 },  /* 0x0C */
    { 1, 2, 60, 0, 25, 15, 6, 4, 120 },  /* 0x0D */
    { 1, 3, 15, 0, 8, 1, 1, 7, 40 },  /* 0x0E */
    { 1, 3, 20, 0, 8, 2, 2, 8, 75 },  /* 0x0F */
    { 1, 11, 44, 4, 26, 12, 4, 8, 180 },  /* 0x10 */
    { 1, 3, 48, 0, 22, 6, 5, 8, 160 },  /* 0x11 */
    { 1, 19, 31, 0, 16, 7, 3, 9, 80 },  /* 0x12 */
    { 1, 4, 12, 0, 4, 2, 2, 4, 40 },  /* 0x13 */
    { 1, 4, 36, 0, 17, 8, 6, 4, 128 },  /* 0x14 */
    { 1, 12, 30, 0, 20, 10, 6, 5, 130 },  /* 0x15 */
    { 1, 5, 10, 5, 2, 1, 2, 3, 50 },  /* 0x16 */
    { 1, 5, 39, 20, 12, 5, 4, 3, 130 },  /* 0x17 */
    { 1, 13, 29, 24, 14, 6, 4, 4, 200 },  /* 0x18 */
    { 1, 6, 12, 4, 5, 2, 1, 3, 50 },  /* 0x19 */
    { 1, 6, 41, 17, 17, 7, 5, 3, 120 },  /* 0x1A */
    { 1, 14, 33, 20, 18, 8, 4, 4, 160 },  /* 0x1B */
    { 1, 7, 14, 0, 7, 1, 1, 4, 21 },  /* 0x1C */
    { 1, 7, 24, 0, 8, 3, 2, 4, 33 },  /* 0x1D */
    { 1, 7, 70, 0, 34, 26, 12, 5, 200 },  /* 0x1E */
    { 1, 1, 36, 0, 20, 12, 4, 6, 110 },  /* 0x1F */
    { 1, 8, 45, 0, 22, 12, 5, 5, 150 },  /* 0x20 */
    { 1, 16, 46, 0, 24, 12, 6, 5, 160 },  /* 0x21 */
    { 8, 2, 16, 0, 8, 2, 1, 5, 40 },  /* 0x22 */
    { 8, 2, 22, 0, 10, 3, 2, 5, 60 },  /* 0x23 */
    { 10, 26, 80, 80, 20, 10, 4, 0, 250 },  /* 0x24 */
    { 10, 26, 80, 80, 20, 10, 4, 0, 250 },  /* 0x25 */
    { 4, 2, 25, 0, 13, 8, 3, 6, 100 },  /* 0x26 */
    { 4, 5, 20, 30, 11, 6, 3, 6, 120 },  /* 0x27 */
    { 5, 26, 38, 4, 28, 16, 6, 5, 170 },  /* 0x28 */
    { 5, 26, 40, 5, 30, 18, 6, 6, 180 },  /* 0x29 */
    { 5, 26, 40, 6, 33, 20, 6, 6, 200 },  /* 0x2A */
    { 6, 25, 40, 0, 20, 16, 3, 4, 80 },  /* 0x2B */
    { 6, 25, 45, 0, 22, 12, 4, 7, 120 },  /* 0x2C */
    { 6, 25, 100, 0, 0, 20, 0, 0, 180 },  /* 0x2D */
    { 6, 25, 36, 0, 18, 14, 5, 4, 150 },  /* 0x2E */
    { 6, 25, 34, 0, 15, 16, 3, 4, 150 },  /* 0x2F */
    { 6, 25, 100, 0, 20, 13, 3, 5, 140 },  /* 0x30 */
    { 1, 7, 30, 0, 5, 3, 2, 4, 99 },  /* 0x31 */
    { 1, 3, 12, 0, 8, 3, 1, 6, 80 },  /* 0x32 */
    { 1, 10, 120, 0, 20, 12, 5, 4, 120 },  /* 0x33 */
    { 1, 10, 90, 0, 20, 10, 4, 5, 255 },  /* 0x34 */
    { 9, 28, 42, 0, 31, 28, 5, 4, 100 },  /* 0x35 */
    { 7, 26, 150, 14, 22, 10, 3, 2, 250 },  /* 0x36 */
    { 7, 26, 160, 20, 20, 16, 4, 3, 250 },  /* 0x37 */
    { 7, 26, 145, 28, 23, 10, 4, 5, 250 },  /* 0x38 */
    { 7, 26, 140, 25, 24, 9, 4, 4, 250 },  /* 0x39 */
    { 9, 26, 300, 200, 15, 8, 3, 5, 255 },  /* 0x3A */
    { 10, 26, 120, 80, 20, 10, 4, 0, 250 },  /* 0x3B */
    { 9, 26, 500, 20, 20, 10, 5, 5, 255 },  /* 0x3C */
    { 9, 26, 500, 20, 20, 10, 5, 5, 255 },  /* 0x3D */
    { 9, 26, 500, 20, 20, 10, 5, 5, 255 },  /* 0x3E */
    { 9, 26, 500, 20, 20, 10, 5, 5, 255 },  /* 0x3F */
    { 1, 22, 12, 0, 1, 3, 0, 4, 1 },  /* 0x40 */
    { 1, 27, 12, 0, 1, 3, 1, 4, 1 },  /* 0x41 */
    { 1, 27, 14, 0, 1, 2, 1, 4, 1 },  /* 0x42 */
    { 1, 27, 33, 0, 1, 2, 2, 4, 1 },  /* 0x43 */
};
