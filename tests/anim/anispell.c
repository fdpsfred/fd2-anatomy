/*
 * unit tests for src/anim/anispell.c
 *
 * fd2_play_ani_file_animation_sequence @ 0x20421
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
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

void run_anim_anispell_tests(void)
{
    printf("Suite: anim/anispell\n");
    printf("  (deferred to Phase 9 integration: decode-to-VGA orchestrator; "
           "see file header)\n\n");
}
