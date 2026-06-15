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
