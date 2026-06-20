#ifndef PLAYHARN_H
#define PLAYHARN_H

/*
 * playharn.h -- shared decls for the FD2 replay/capture harness.
 *
 * Guest-side code compiled ONLY into the replay build (FD2RP.EXE) via
 * tools/fd2_play/build_replay.py with -DFD2_REPLAY. NEVER linked into the
 * production FD2.EXE. ASCII-only; all emitted host files use DOS 8.3 names.
 *
 * The src/ hooks (fd2_replay_init / fd2_replay_pump) are declared in
 * src/include/protos.h under #ifdef FD2_REPLAY; this header carries the
 * harness-internal decls shared between replay.c and capture.c.
 */

/* Dump one checkpoint to the current dir (= tests/OUT, the EXE cwd):
 *   FBnn.BIN  raw VGA mode-13h framebuffer (0xA0000, 64000 bytes)
 *   STnn.BIN  16 x int32 key-global header + party_count x 0x50 runtime_char
 * idx is the capture sequence number (00..99). */
void fd2_play_capture(int idx);

/* Rewrite HB.TXT (fopen/fprintf/fclose) so the host poller can detect a hang
 * and name the last scripted step, mirroring the unit harness heartbeat. */
void fd2_play_heartbeat(const char *tag);

#endif /* PLAYHARN_H */
