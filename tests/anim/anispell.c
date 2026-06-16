/*
 * unit tests for src/anim/anispell.c
 *
 * fd2_play_ani_file_animation_sequence @ 0x20421
 * fd2_animate_bg_zoom_transition_in    @ 0x29C90
 * fd2_animate_bg_zoom_transition_out   @ 0x29DED
 * fd2_cycle_sprite_anim_with_bg_frames @ 0x2A5D0
 * fd2_play_spell_cast_sequence         @ 0x2A6BD
 *
 * ===== fd2_play_ani_file_animation_sequence @ 0x20421 =====
 *
 * TEST DEFERRED TO PHASE 9 (INTEGRATION) — reason below.
 *
 * This function is a pure file-I/O + decode-to-VGA + display orchestrator:
 *   open ANI.DAT, seek the per-animation TOC entry (anim_idx*4 + 6), read the
 *   stream offset, read the 0xAD-byte header, then for header[0xA5] frames:
 *   read an 8-byte frame header, read the raw frame bytes into a backbuffer,
 *   and hand them to the shared ANI decoder which writes decoded pixels
 *   straight to the physical VGA frame buffer (0xA0000, mode 13h). A per-frame
 *   BIOS-tick delay and a skip-on-key poll bracket each frame.
 *
 * There is no host-harness unit seam for it:
 *   1. The body unconditionally calls the REAL fd2_ani_decoder_decode_frame_bytes
 *      (anidec.obj is linked into TEST.EXE) on frame 0, BEFORE the skip-on-key
 *      check, so the loop cannot be made to exit before the decoder runs. Every
 *      animation in the real ANI.DAT has >= 12 frames (anim 0 = 96, anim 1 = 51,
 *      ...), so any invocation reaches the decoder.
 *   2. The real decoder dispatches each chunk byte through the 10-entry table
 *      data_fd2_animation_ani_decoder_frame_dispatch_table[chunk_type]. Running
 *      it safely over real frame bytes requires the real chunk handlers (they
 *      consume their operands so only valid 0..9 chunk bytes are ever read);
 *      a no-op stub that does not advance the cursor turns raw frame bytes into
 *      out-of-range table indices (>= 10) -> OOB call. Installing the full real
 *      handler set instead performs a complete multi-frame decode that writes to
 *      0xA0000 — i.e. an end-to-end decode of the real cinematic, which is an
 *      integration scenario, not a unit test of this orchestrator.
 *   3. The remaining observable effects are display/timing side effects only
 *      (VGA pixels, BIOS-tick delay via the fd2_delay_ms spy, keyboard
 *      poll on the real BIOS buffer, and the intro chime via the
 *      fd2_play_sfx_with_handle spy). Per the project test policy, pure
 *      blit/display side-effect state defers to Phase 9 integration.
 *
 * The non-display logic that COULD carry risk — the TOC offset arithmetic
 * (anim_idx*4 + 6), the stream-offset seek, and the frame-count read at
 * header[0xA5] — was verified directly against the real ANI.DAT during emit
 * (prefix "LLLLLL"; u32 TOC from offset 6; per-stream 0xAD header; [0xA5] frame
 * count), and the emitted C mirrors the disassembly exactly. The Ghidra
 * EAX-tracking artifacts (the CONCAT22 frame-count packing fed only its low 16
 * bits to the loop, and the bool-return of the keyboard poll) were resolved by
 * reading the assembly. See src/emit_issues.json (00020421).
 *
 * The Phase 9 integration test will drive the real function with the real
 * handler table installed, against the staged real ANI.DAT, asserting the
 * decoder receives exactly header[0xA5] frames with the per-frame decoded-size
 * sequence from an independent ANI.DAT parse, the intro chime fires once on
 * frame 0 for anim_idx==1 and is stopped+freed on exit, and the skip-on-key
 * path breaks the loop early when the BIOS keyboard buffer is made non-empty.
 *
 * ===== fd2_animate_bg_zoom_transition_in @ 0x29C90 =====
 *
 * UNIT-TESTED: the non-display computation (the 3-layer BG cache cycling) is
 * driven against the REAL function below; only the pure VRAM pixel output is
 * left to Phase 9.
 *
 * The function's only non-display logic is the BG-layer index math:
 *   - phase 1 (frame_iter 9..0): bg_layer[frame_iter % 3]
 *   - phase 3 (frame_iter 9..0): bg_layer[(frame_iter + 2) % 3]
 * a signed-IDIV-by-3 modulo (matches C `%` for the non-negative frame range)
 * plus the per-phase blit dst/stride. fd2_rle_blit_sprite is a recording stub
 * (testglob.c) that logs every resolved BG-layer pointer + dst into
 * g_rle_blit_log_*, so the cycling sequence is directly observable. The blit
 * destination (0xA0000) is the VGA aperture — a harmless host-harness write,
 * the same convention used by the blitspr / status / aniwalk2 suites.
 *
 * The intervening fd2_flash_char_hit_sprite (phase 2) is REAL-linked and runs
 * the REAL fd2_render_mini_char_status_panel + fd2_display_dialog_scene; those
 * are stood up by the shared minipfix.h fixture (the same fixture that drives
 * fd2_flash_char_hit_sprite end-to-end in tests/battle/battle2.c), so the whole
 * function runs without touching real VGA / fopen. The pure pixel output of the
 * scroll blits is the only thing deferred to Phase 9. See emit_issues.json
 * (00029c90).
 *
 * ===== fd2_animate_bg_zoom_transition_out @ 0x29DED =====
 *
 * UNIT-TESTED the same way as the zoom-in counterpart: the REAL function is
 * driven and the non-display computation asserted; only the VRAM pixel output
 * is left to Phase 9.
 *
 * The non-display logic differs from zoom-in in three ways, all asserted here:
 *   - both scroll passes count FORWARD (phase 1 frame_iter 1..9, phase 3 1..10)
 *   - the index math is bg_layer[frame_iter % 3] (phase 1) and
 *     bg_layer[(frame_iter + 1) % 3] (phase 3)
 *   - phase 1 + the phase-2 defender repaint blit into workspace + 0x140, while
 *     phase 3 blits into bare workspace (the reverse band assignment of zoom-in)
 * plus the extra phase-2 banner + terrain backdrop rle blits (the zoom-in has
 * only the single silhouette blit). fd2_rle_blit_sprite is the same recording
 * stub; the intervening fd2_flash_char_hit_sprite is REAL-linked and stood up by
 * the shared minipfix.h fixture. See emit_issues.json (00029ded).
 *
 * ===== fd2_play_spell_cast_cinematic @ 0x2A2E8 =====
 *
 * TEST DEFERRED TO PHASE 9 (INTEGRATION) — reason below.
 *
 * This is the class-promotion cinematic: a real-file + decode-to-VGA + timing
 * orchestrator with no host-harness unit seam for its only non-display logic.
 *
 *   1. Real-file I/O with no recording seam. The two pieces of computation
 *      unique to this function are the FIGANI indices it derives —
 *      caster = rt_chars[caster_char_idx].portrait_id * 3 and target =
 *      spell_id * 3 — but each is passed straight into the REAL
 *      fd2_load_dat_resource (src/rsrc/rsrc.c, linked into TEST.EXE), which
 *      fopen()s the genuine FIGANI.DAT and seeks to that index. There is no
 *      recording stub in the seam to observe the index, and fd2_load_dat_resource
 *      is real-linked (shared by many suites) so it cannot be replaced for this
 *      one test. FIGANI.DAT / BG.DAT are real game files (forbidden to fake), and
 *      are not in build_test.py's staged GAME_FILES set, so a real load would not
 *      even resolve in the harness without changing the stage list.
 *   2. The phase-2 / phase-4 anim is the REAL-class end-to-end cinematic. Both
 *      animation phases call fd2_cycle_sprite_anim_with_bg_frames (a separate
 *      routing target @ 0x2A5D0, emitted in this file and unit-tested below). Its
 *      real body dereferences the real FIGANI sprite atlas (*(byte*)atlas,
 *      *(int*)(atlas+8+frame*4)) and spins on the REAL fd2_wait_n_bios_ticks
 *      16 + 24 = 40 times — i.e. ~2.2s of real BIOS-tick waiting plus a full real
 *      atlas traversal against the genuine FIGANI.DAT. Driving it inside this
 *      cinematic for real is an integration scenario, not a unit test of this
 *      orchestrator.
 *   3. The phase-1 BG cycler is timer-coupled at the instruction level: the
 *      cycler advance is reached past a JZ on the flags left by `ADD ESP,0xC`
 *      (always non-zero -> branch never taken -> the cycler advances every
 *      frame), with a dead `MOVSX EAX, word ptr [0x46C]` BIOS-tick read in front
 *      of it. The emitted C reproduces the actual (unconditional) behavior; there
 *      is nothing data-dependent here to assert beyond the cycling sequence,
 *      which is observed identically through the same rle-blit stub already
 *      exercised by the two zoom suites above.
 *   4. Everything else is pure display/timing side effects: malloc + memmove of
 *      the VGA aperture (0xA0000), fade-to-black / fade-in (no-op stubs),
 *      set_vga_palette_range[_with_add] (real, exercised by tests/gfx/palette.c),
 *      blit_rectangle / blit_indexed_sprite, fd2_delay_ms (spy). Per the
 *      project test policy, pure blit/display side-effect state defers to Phase 9
 *      integration.
 *
 * The non-display arithmetic that could carry risk was verified directly against
 * ground truth during emit: the runtime_char stride (0x50) and .portrait_id
 * offset (+0x07) from types.h, the FIGANI index *3 and the caller's
 * (char_idx, class_id) argument order from fd2_run_class_promotion_menu_main @
 * 0x31385, the BG.DAT / FIGANI.DAT filename constants at 0x52381 / 0x52388, and
 * the unconditional phase-1 cycler from the JZ-on-ADD-flags disassembly. The
 * emitted C mirrors the disassembly exactly. See src/emit_issues.json (0002a2e8).
 *
 * The Phase 9 integration test will stage the real BG.DAT + FIGANI.DAT, install
 * the real fd2_cycle_sprite_anim_with_bg_frames, and drive the real function,
 * asserting the two FIGANI loads request indices portrait_id*3 and class_id*3 and
 * that the cinematic restores the backed-up VGA frame on exit.
 *
 * ===== fd2_play_spell_cast_sequence @ 0x2A6BD =====
 *
 * TEST DEFERRED TO PHASE 9 (INTEGRATION) — reason below.
 *
 * This is the master "big spell cast" animation + damage orchestrator. Every one
 * of its exit paths invokes a real, heavy VGA-cinematic / real-file callee with
 * no host-harness unit seam:
 *
 *   1. Both top-level routing paths call a REAL cinematic worker:
 *      spell_id >= 0x20 -> fd2_execute_summon_spell_cast (src/spell/spellcin.c),
 *      spell_id == 0x18 || 0x1C..0x1F -> fd2_execute_special_attack_skill (same).
 *      Both are real-linked into TEST.EXE (shared by other code), so they cannot
 *      be replaced with a recording stub for this one test; and both themselves
 *      fopen the genuine FIGANI.DAT / BG.DAT via the real fd2_load_dat_resource
 *      and write to the 0xA0000 VGA aperture — i.e. invoking either is a full
 *      cinematic run, an integration scenario.
 *   2. The inline path (basic spells 0x00..0x17,0x19..0x1B) loads BG.DAT,
 *      TAI.DAT and FIGANI.DAT through the real fd2_load_dat_resource (src/rsrc),
 *      and then DEREFERENCES the returned buffers (the caster FIGANI's first
 *      int16/byte for the frame count, the per-target FIGANI streams, the
 *      tile-event byte). BG.DAT / TAI.DAT / FIGANI.DAT are real game files
 *      (forbidden to fake) and are NOT in build_test.py's staged GAME_FILES set
 *      (only FDICON.B24 / FDFIELD / FDSHAP / FDOTHER / FDTXT / FDMUS / DATO /
 *      FD2.SAV are staged), so a real load would not even resolve in the harness
 *      and the subsequent deref would fault. fd2_load_dat_resource is real-linked
 *      and shared, so it cannot be stubbed for this one test.
 *   3. The per-phase animation is driven by the 10-entry function-pointer table
 *      data_fd2_battle_spell_cast_cinematic_phase_handler_table @ 0x523B9, whose
 *      real handlers are themselves VGA cinematics; and the inner blits go to the
 *      real fd2_blit_rectangle (0xA0000) plus the real fd2_flash_char_hit_sprite.
 *   4. Everything else is display/timing side effects (malloc/free, the work +
 *      backbuffer blits, BIOS-tick waits, palette fades, the final scene-cache
 *      reload). Per the project test policy, pure blit/display side-effect state
 *      defers to Phase 9 integration.
 *
 * The non-display logic that DOES carry risk was verified directly against the
 * disassembly ground truth during emit, and the emitted C mirrors it exactly:
 *   - Top-level dispatch thresholds: >= 0x20 summon; == 0x18 or in 0x1C..0x1F
 *     special; otherwise inline (matches the CMP 0x20 / CMP 0x18 / CMP 0x1B
 *     ladder).
 *   - The portrait_load / shine-table-offset selection (spell 8 -> 0xB0/0x13;
 *     spell > 3 -> 0xB0/0xF; else 0x20/0x0B).
 *   - The HP-bar lerp: tgt->hp_current = starting_hp - (int16)((int)(starting_hp
 *     - final_hp) * hit_count / hp_lerp_hits[spell_id]) (signed IDIV; both HP
 *     snapshots sign-extended from int16; the damage is applied by, and the
 *     final HP written by, the real fd2_calc_magic_damage; the original HP is
 *     restored before the lerp). The miss test is is_miss = (damage == 0),
 *     resolved from the SETZ -> byte in the assembly (NOT a stale EAX read).
 *   - The per-target shake direction: shake_y_dir = 1 - fd2_advance_rng_state()
 *     % 3, consuming the RNG seed left in EAX (range [0,0xFFFF], zero-extended)
 *     by the CALL at 0x2af40 -- NOT a stale EAX read of flash_unit (the Ghidra
 *     EAX-tracking bug: fd2_advance_rng_state decompiles as void). It feeds only
 *     the cosmetic shake X-offset sign; the RNG primitive itself is unit-tested
 *     in tests/battle. See src/emit_issues.json (0002a6bd). Reachable only
 *     behind the same real-cinematic / real-file wall as the rest of this fn, so
 *     it defers with the function.
 *   - The 6 function-local const tables (shake X/Y offsets, HP-lerp hit counts,
 *     player/enemy caster sprite-id tables, intro SFX-bank table) and the latent
 *     spell_id >= table-length over-read documented in src/emit_issues.json.
 *   - The 10 indirect dispatch-table calls' reconstructed 5 args (the decompiler
 *     masks them as "()"): handler(caster_idx, team_caster_sprite, work_buffer,
 *     stride, phase_code) with phase_code the per-site immediate 0..8.
 * See src/emit_issues.json (0002a6bd).
 *
 * The Phase 9 integration test will stage the real BG.DAT / TAI.DAT / FIGANI.DAT,
 * install controllable phase handlers in the 0x523B9 table, drive the real
 * function for a basic attack spell (spell_id 0, target_count 1), and assert the
 * target's HP bar lerps from its pre-cast value to the fd2_calc_magic_damage
 * result over hp_lerp_hits[spell_id] steps, plus that the summon / special
 * routing branches hand off to the correct worker.
 *
 * ===== fd2_cycle_sprite_anim_with_bg_frames @ 0x2A5D0 =====
 *
 * UNIT-TESTED: the two non-display computations are driven against the REAL
 * function below; only the pure VRAM pixel output is left to Phase 9.
 *
 *   1. BG cycler — bg_variant_idx = (bg_variant_idx + 1) % 3, advanced once per
 *      frame before the BG blit, so the rle-blit log records the cycling pointer
 *      sequence 1,2,0,1,2,0,... (the three contiguous BG-layer globals indexed as
 *      uint32[3], each seeded with a distinct readable buffer).
 *   2. Frame-advance state machine — the higher-risk computation: per frame it
 *      reads hold_count = atlas[6 + atlas[8 + frame_idx*4]], increments a tick,
 *      and on tick == hold_count resets the tick (XOR-with-self -> 0) and advances
 *      frame_idx, wrapping to 0 when it reaches frame_count (= atlas[0]). This is
 *      driven over a hand-built in-memory atlas with frame_count=3 and per-frame
 *      hold counts {2,1,3}, exercising multi-tick holds, single-tick holds, tick
 *      accumulation + XOR-reset, the per-frame block-offset indexing, and the
 *      frame_idx wrap. The frame_idx blitted each iteration is recovered through a
 *      per-call frame log added to the fd2_blit_indexed_sprite stub (testglob.c).
 *
 * The atlas is a pure in-memory buffer (NOT a game file): the function only reads
 * atlas[0], the int32 per-frame block-offset table at atlas+8, and the hold byte
 * at atlas+6+offset, all of which are constructed here. fd2_blit_rectangle is the
 * REAL emitted function; it reads 0xC8*0x140 = 0x19000 bytes from workspace
 * (sized 0x1F400, in-bounds) and writes to the 0xA0000 VGA aperture (the harmless
 * host-harness write convention used by the zoom suites). fd2_wait_n_bios_ticks is
 * REAL and busy-waits one BIOS tick (~55ms) per frame against the physical tick
 * word at 0x46C, which advances under the host; iter_count is kept small (8) so
 * the test completes in well under a second. The pure scroll/atlas pixel output is
 * the only thing deferred to Phase 9.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "minipfix.h"
#include <stdio.h>

extern runtime_char g_test_rc_array[8];
extern int32  g_rle_blit_log_stride[64];
extern int    g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int    g_blit_indexed_sprite_last_x;
extern int    g_blit_indexed_sprite_last_y;
extern int    g_blit_indexed_log_on;
extern uint32 g_blit_indexed_log_frame[64];

/* distinguishable, non-zero sentinels for the 3 BG-layer slots. The function
 * only passes these opaque to the rle-blit stub (never dereferences them), so
 * any distinct values suffice to recover the cycling order. */
#define BG_SENTINEL_0 0x11110000u
#define BG_SENTINEL_1 0x22220000u
#define BG_SENTINEL_2 0x33330000u

/*
 * Drives the REAL fd2_animate_bg_zoom_transition_in and asserts the only
 * non-display computation: the 3-layer BG cycling for both scroll passes, plus
 * each blit's dst buffer and stride.
 *
 * Expected rle-blit BG-layer indices (frame_iter 9..0):
 *   phase 1: frame_iter % 3       = 0,2,1,0,2,1,0,2,1,0
 *   phase 3: (frame_iter + 2) % 3 = 2,1,0,2,1,0,2,1,0,2
 *
 * Phase 1 occupies the FIRST 10 logged rle blits (nothing logs before it).
 * Between the passes, the real mini-panel painter (via fd2_flash_char_hit_sprite)
 * emits a bounded handful of glyph blits, so phase 3's 10 blits are the LAST 10
 * logged ones (nothing runs after phase 3) — read them from the tail of the log,
 * which is robust to the exact panel blit count.
 *
 * Buffer sizing (from the disassembly):
 *   - workspace must be >= 0x1F400 (the phase-2 memset clears 0x1F400, and the
 *     scroll blits read/write within it).
 *   - clear_buf must be >= 0x19000: the phase-2 blit_rectangle(workspace, 0x280,
 *     clear_buf, 0x140, 0x140, 0xC8) reads 0x140 bytes/row, advancing src by
 *     0x140/row for 0xC8 rows => a contiguous 0xC8*0x140 = 0x19000-byte read of
 *     clear_buf. (The game allocates clear_buf at 64000 and the real silhouette
 *     blit fills it; here the rle blit is a stub, so we just size the buffer to
 *     keep the real blit_rectangle read in-bounds.)
 */
static void test_bg_zoom_transition_bg_cycling(void)
{
    static const int phase1_idx[10] = {0, 2, 1, 0, 2, 1, 0, 2, 1, 0};
    static const int phase3_idx[10] = {2, 1, 0, 2, 1, 0, 2, 1, 0, 2};
    uint32 sentinel[3];
    uint8 *workspace;
    uint8 *clear_buf;
    int tail;
    int i;

    sentinel[0] = BG_SENTINEL_0;
    sentinel[1] = BG_SENTINEL_1;
    sentinel[2] = BG_SENTINEL_2;

    workspace = (uint8 *)malloc(0x1f400);
    clear_buf = (uint8 *)malloc(0x19000);
    ASSERT_TRUE(workspace != NULL);
    ASSERT_TRUE(clear_buf != NULL);

    /* the function indexes the three contiguous globals as uint32[3]. */
    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = BG_SENTINEL_0;
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = BG_SENTINEL_1;
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = BG_SENTINEL_2;

    /* stand up the real mini-panel painter (sprite sheet + immediate-END text)
     * so the phase-2 fd2_flash_char_hit_sprite runs without touching VGA/fopen;
     * also resets g_rle_blit_calls and enables g_rle_blit_log_on. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    minip_setup_env();

    /* char_unit_id 0 (-> mini-panel reads runtime_char[0], all-zero),
     * char_sprite_idx 7 (opaque to the blit_indexed stub),
     * caster_figani 0 (opaque to the rle stub). */
    fd2_animate_bg_zoom_transition_in(0, 7, (uint32)clear_buf,
                                      (uint32)workspace, 0);

    /* the log must not have wrapped (64-entry cap) or the tail read is wrong. */
    ASSERT_TRUE(g_rle_blit_calls < 64);
    /* phase1 (10) + phase2 caster blit (1) + panel glyphs + phase3 (10). */
    ASSERT_TRUE(g_rle_blit_calls >= 21);

    /* phase 2 ran (the blit_indexed stub recorded the call). */
    ASSERT_TRUE(g_blit_indexed_sprite_calls >= 1);

    /* --- phase 1: first 10 logged blits, dst = workspace, stride 0x280 --- */
    for (i = 0; i < 10; i++) {
        ASSERT_EQ((long)g_rle_blit_log_sprite[i], (long)sentinel[phase1_idx[i]]);
        ASSERT_EQ((long)g_rle_blit_log_dst[i], (long)(uint32)workspace);
        ASSERT_EQ((long)g_rle_blit_log_stride[i], (long)0x280);
    }

    /* --- phase 3: last 10 logged blits, dst = workspace+0x140, stride 0x280 --- */
    tail = g_rle_blit_calls - 10;
    for (i = 0; i < 10; i++) {
        ASSERT_EQ((long)g_rle_blit_log_sprite[tail + i],
                  (long)sentinel[phase3_idx[i]]);
        ASSERT_EQ((long)g_rle_blit_log_dst[tail + i],
                  (long)((uint32)workspace + 0x140));
        ASSERT_EQ((long)g_rle_blit_log_stride[tail + i], (long)0x280);
    }

    free(workspace);
    free(clear_buf);
    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = 0;
}

/* opaque, never-dereferenced sentinels for the phase-2 banner + terrain blits.
 * Both blits land at log index >= 9 (after phase 1's 9 blits), i.e. outside the
 * stub's first-4-calls deref window, so any distinct non-zero values suffice. */
#define BANNER_SENTINEL  0x44440000u
#define TERRAIN_SENTINEL 0x55550000u

/*
 * Drives the REAL fd2_animate_bg_zoom_transition_out and asserts its only
 * non-display computation: the forward 3-layer BG cycling for both scroll
 * passes, the per-phase blit dst band (workspace+0x140 vs bare workspace), and
 * the phase-2 banner + terrain backdrop blits.
 *
 * Expected rle-blit BG-layer indices:
 *   phase 1 (frame_iter 1..9):  frame_iter % 3        = 1,2,0,1,2,0,1,2,0
 *   phase 3 (frame_iter 1..10): (frame_iter + 1) % 3  = 2,0,1,2,0,1,2,0,1,2
 *
 * Log layout (the recording stub never wraps below the 64 cap here):
 *   [0..8]   phase 1, dst = workspace + 0x140, stride 0x280
 *   [9]      banner blit  (name_banner_sprite, dst = clear_buf, stride 0x140)
 *   [10]     terrain blit (terrain_bg,         dst = clear_buf, stride 0x140)
 *   [11..]   real mini-panel glyph blits (bounded count)
 *   tail 10  phase 3, dst = workspace, stride 0x280
 *
 * Buffer sizing (same reasoning as the zoom-in test): workspace >= 0x1F400
 * (phase-2 memset clears 0x1F400) and clear_buf >= 0x19000 (the phase-2
 * blit_rectangle reads 0xC8 rows * 0x140 bytes from clear_buf).
 */
static void test_bg_zoom_transition_out_bg_cycling(void)
{
    static const int phase1_idx[9]  = {1, 2, 0, 1, 2, 0, 1, 2, 0};
    static const int phase3_idx[10] = {2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
    uint32 sentinel[3];
    uint8 *workspace;
    uint8 *clear_buf;
    int tail;
    int i;

    sentinel[0] = BG_SENTINEL_0;
    sentinel[1] = BG_SENTINEL_1;
    sentinel[2] = BG_SENTINEL_2;

    workspace = (uint8 *)malloc(0x1f400);
    clear_buf = (uint8 *)malloc(0x19000);
    ASSERT_TRUE(workspace != NULL);
    ASSERT_TRUE(clear_buf != NULL);

    /* the function indexes the three contiguous globals as uint32[3]. */
    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = BG_SENTINEL_0;
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = BG_SENTINEL_1;
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = BG_SENTINEL_2;

    /* stand up the real mini-panel painter so the phase-2
     * fd2_flash_char_hit_sprite runs without touching VGA/fopen; also resets
     * g_rle_blit_calls and enables g_rle_blit_log_on. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    minip_setup_env();

    /* char_unit_id 0 (-> mini-panel reads runtime_char[0], all-zero),
     * char_sprite_idx 7 (opaque to the blit_indexed stub),
     * terrain_bg / name_banner_sprite opaque to the rle stub. */
    fd2_animate_bg_zoom_transition_out(0, 7, TERRAIN_SENTINEL,
                                       (uint32)clear_buf, (uint32)workspace,
                                       BANNER_SENTINEL);

    /* the log must not have wrapped (64-entry cap) or the tail read is wrong. */
    ASSERT_TRUE(g_rle_blit_calls < 64);
    /* phase1 (9) + banner (1) + terrain (1) + panel glyphs + phase3 (10). */
    ASSERT_TRUE(g_rle_blit_calls >= 21);

    /* phase 2 defender repaint ran. The call is
     * fd2_blit_indexed_sprite(char_sprite_idx, 0, workspace+0x140, 0x280, -1):
     * char_sprite_idx is the atlas (arg1, discarded by the stub), the frame_idx
     * (arg2) is the literal 0, and the x-arg is the lower band workspace+0x140.
     * The stub records frame_idx/x/y, so assert those (the atlas is not
     * observable through this stub). */
    ASSERT_TRUE(g_blit_indexed_sprite_calls >= 1);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_frame, (long)0);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x,
              (long)((uint32)workspace + 0x140));
    ASSERT_EQ((long)g_blit_indexed_sprite_last_y, (long)0x280);

    /* --- phase 1: first 9 logged blits, dst = workspace+0x140, stride 0x280 --- */
    for (i = 0; i < 9; i++) {
        ASSERT_EQ((long)g_rle_blit_log_sprite[i], (long)sentinel[phase1_idx[i]]);
        ASSERT_EQ((long)g_rle_blit_log_dst[i],
                  (long)((uint32)workspace + 0x140));
        ASSERT_EQ((long)g_rle_blit_log_stride[i], (long)0x280);
    }

    /* --- phase 2: banner then terrain backdrop, both into clear_buf @ 0x140 --- */
    ASSERT_EQ((long)g_rle_blit_log_sprite[9], (long)BANNER_SENTINEL);
    ASSERT_EQ((long)g_rle_blit_log_dst[9], (long)(uint32)clear_buf);
    ASSERT_EQ((long)g_rle_blit_log_stride[9], (long)0x140);
    ASSERT_EQ((long)g_rle_blit_log_sprite[10], (long)TERRAIN_SENTINEL);
    ASSERT_EQ((long)g_rle_blit_log_dst[10], (long)(uint32)clear_buf);
    ASSERT_EQ((long)g_rle_blit_log_stride[10], (long)0x140);

    /* --- phase 3: last 10 logged blits, dst = bare workspace, stride 0x280 --- */
    tail = g_rle_blit_calls - 10;
    /* phase 3 must start strictly after the banner+terrain pair. */
    ASSERT_TRUE(tail >= 11);
    for (i = 0; i < 10; i++) {
        ASSERT_EQ((long)g_rle_blit_log_sprite[tail + i],
                  (long)sentinel[phase3_idx[i]]);
        ASSERT_EQ((long)g_rle_blit_log_dst[tail + i], (long)(uint32)workspace);
        ASSERT_EQ((long)g_rle_blit_log_stride[tail + i], (long)0x280);
    }

    free(workspace);
    free(clear_buf);
    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = 0;
}

/*
 * Drives the REAL fd2_cycle_sprite_anim_with_bg_frames and asserts its two
 * non-display computations over an in-memory atlas:
 *   - the per-frame BG cycler bg_variant_idx = (bg_variant_idx + 1) % 3
 *   - the atlas-driven frame-advance state machine (hold accumulate + wrap)
 *
 * Atlas layout (a pure in-memory buffer; not a game file):
 *   atlas[0]            = 3              frame_count
 *   atlas[8 + i*4]      = block_off[i]   int32 per-frame block offset
 *   atlas[6 + block_off] = hold_count    byte, per frame
 * Block offsets {0x20,0x24,0x28} place the three hold bytes at atlas[0x26/2A/2E]
 * with hold counts {2,1,3}.
 *
 * Hand-derived expected sequences for iter_count = 8 (frame_idx blitted is the
 * value BEFORE that frame's advance; the cycler advances before its blit):
 *   iter:        0  1  2  3  4  5  6  7
 *   bg index:    1  2  0  1  2  0  1  2      (% 3, every frame)
 *   frame_idx:   0  0  1  2  2  2  0  0      (f0 hold 2, f1 hold 1, f2 hold 3,
 *                                            wrap 3->0 at iter 5)
 */
static void test_cycle_sprite_anim_frame_advance(void)
{
    static const int bg_seq[8]    = {1, 2, 0, 1, 2, 0, 1, 2};
    static const uint32 frame_seq[8] = {0, 0, 1, 2, 2, 2, 0, 0};
    uint8  *atlas;
    uint8  *workspace;
    uint8  *bg_buf[3];
    uint32  bg_ptr[3];
    int     i;

    /* build the in-memory atlas */
    atlas = (uint8 *)malloc(0x40);
    ASSERT_TRUE(atlas != NULL);
    memset(atlas, 0, 0x40);
    atlas[0] = 3;                                   /* frame_count */
    *(int32 *)(atlas + 8)  = 0x20;                  /* block_off[0] */
    *(int32 *)(atlas + 12) = 0x24;                  /* block_off[1] */
    *(int32 *)(atlas + 16) = 0x28;                  /* block_off[2] */
    atlas[6 + 0x20] = 2;                            /* frame 0 hold */
    atlas[6 + 0x24] = 1;                            /* frame 1 hold */
    atlas[6 + 0x28] = 3;                            /* frame 2 hold */

    workspace = (uint8 *)malloc(0x1f400);
    ASSERT_TRUE(workspace != NULL);

    /* Seed the three contiguous BG-layer globals with distinct readable buffers.
     * The function only passes them opaque to the rle stub, but the stub reads
     * the first byte of the first 4 sprite pointers, so they must be readable. */
    for (i = 0; i < 3; i++) {
        bg_buf[i] = (uint8 *)malloc(16);
        ASSERT_TRUE(bg_buf[i] != NULL);
        bg_buf[i][0] = (uint8)(0x10 + i);
        bg_ptr[i] = (uint32)bg_buf[i];
    }
    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = bg_ptr[0];
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = bg_ptr[1];
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = bg_ptr[2];

    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;
    g_blit_indexed_sprite_calls = 0;
    g_blit_indexed_log_on = 1;

    fd2_cycle_sprite_anim_with_bg_frames((uint32)atlas, (uint32)workspace, 8);

    g_rle_blit_log_on = 0;
    g_blit_indexed_log_on = 0;

    /* exactly one rle blit and one indexed blit per iteration */
    ASSERT_EQ((long)g_rle_blit_calls, (long)8);
    ASSERT_EQ((long)g_blit_indexed_sprite_calls, (long)8);

    /* BG cycler sequence: each blit's resolved sprite ptr = the cycled layer,
     * and dst = workspace, stride 0x280. */
    for (i = 0; i < 8; i++) {
        ASSERT_EQ((long)g_rle_blit_log_sprite[i], (long)bg_ptr[bg_seq[i]]);
        ASSERT_EQ((long)g_rle_blit_log_dst[i], (long)(uint32)workspace);
        ASSERT_EQ((long)g_rle_blit_log_stride[i], (long)0x280);
    }

    /* frame-advance state machine: the frame_idx blitted each iteration. */
    for (i = 0; i < 8; i++) {
        ASSERT_EQ((long)g_blit_indexed_log_frame[i], (long)frame_seq[i]);
    }

    /* the indexed-sprite blit always targets workspace at y = 0x280 */
    ASSERT_EQ((long)g_blit_indexed_sprite_last_x, (long)(uint32)workspace);
    ASSERT_EQ((long)g_blit_indexed_sprite_last_y, (long)0x280);

    free(atlas);
    free(workspace);
    for (i = 0; i < 3; i++) {
        free(bg_buf[i]);
    }
    data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr = 0;
    data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr = 0;
}

void run_anim_anispell_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anispell\n");
    printf("  (fd2_play_ani_file_animation_sequence deferred to Phase 9 "
           "integration: decode-to-VGA orchestrator; see file header)\n");
    printf("  (fd2_play_spell_cast_cinematic deferred to Phase 9 integration: "
           "real-file + decode-to-VGA + timing orchestrator; see file header)\n");
    printf("  (fd2_play_spell_cast_sequence deferred to Phase 9 integration: "
           "all paths invoke real cinematic/real-file callees; see file header)\n");
    RUN_TEST(test_bg_zoom_transition_bg_cycling);
    RUN_TEST(test_bg_zoom_transition_out_bg_cycling);
    RUN_TEST(test_cycle_sprite_anim_frame_advance);
    printf("\n");
}
