/*
 * unit tests for src/anim/anispell.c
 *
 * fd2_play_ani_file_animation_sequence @ 0x20421
 * fd2_animate_bg_zoom_transition_in    @ 0x29C90
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
 *      (VGA pixels, BIOS-tick delay via the __delay_thunk_375b2 spy, keyboard
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

void run_anim_anispell_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anispell\n");
    printf("  (fd2_play_ani_file_animation_sequence deferred to Phase 9 "
           "integration: decode-to-VGA orchestrator; see file header)\n");
    RUN_TEST(test_bg_zoom_transition_bg_cycling);
    printf("\n");
}
