/*
 * replay.c -- deterministic input driver for the FD2 replay harness.
 *
 * Replay build only (-DFD2_REPLAY). Reads a text command script (SCRIPT.TXT,
 * staged into the EXE cwd by tools/fd2_play/run_play.py) and drives the
 * unmodified game input path by injecting scancodes into the BIOS keyboard
 * ring. Because a key is made available before each wait, the game's
 * "while (buffer empty) { idle animation }" loops run zero iterations -- so
 * the BIOS-tick-paced idle animation and its blink RNG draws are skipped,
 * making each run deterministic.
 *
 * Script commands (one per line; '#' = comment):
 *   SEED <hex16>          set data_fd2_shared_rng_seed
 *   KEY  <hexSc> [hexAsc] inject one keystroke (scancode, optional ascii)
 *   CAP                   dump a checkpoint (auto-numbered FBnn/STnn)
 *   END                   write DONE.TXT and exit
 * EOF behaves like END.
 *
 * Hooked from src/ at three gated call sites (all #ifdef FD2_REPLAY):
 *   life/main.c          fd2_replay_init() before the main loop + pinned warm-up
 *   input/input.c        fd2_replay_pump() in fd2_check_keyboard_buffer_nonempty
 *   anim/aniend.c,
 *   save/save.c          fd2_replay_pump() before the two non-polling reads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "playharn.h"

#define SCRIPT_NAME  "SCRIPT.TXT"
#define MAX_LINE     64
#define MAX_CAP      99

static FILE         *g_script = (FILE *)0;
static int           g_active = 0;
static int           g_cap_idx = 0;
static unsigned long g_hb_seq = 0;

/*
 * Inject one keystroke into the BIOS keyboard ring. The ring is 16 two-byte
 * slots at 0x40:0x1E..0x3D (flat 0x41E..0x43D), each (scancode<<8)|ascii;
 * head/tail (flat 0x41A/0x41C) are byte offsets from segment 0x40 (flat
 * 0x400). int386(0x16) reflects to the real-mode BIOS, which reads this same
 * ring -- the game's own fd2_clear_keyboard_buffer touches it via the flat
 * BDA the same way. Only called when the ring is empty, so no full check.
 */
static void kbd_inject(uint8 scancode, uint8 ascii)
{
    uint16 tail = BIOS_KBD_TAIL;
    *(volatile uint16 *)(0x400uL + tail) =
        (uint16)(((uint16)scancode << 8) | (uint16)ascii);
    tail = (uint16)(tail + 2);
    if (tail >= 0x3E) {
        tail = 0x1E;
    }
    BIOS_KBD_TAIL = tail;
}

void fd2_play_heartbeat(const char *tag)
{
    FILE *f = fopen("HB.TXT", "w");
    if (f != (FILE *)0) {
        fprintf(f, "%lu %s\n", ++g_hb_seq, tag);
        fclose(f);
    }
}

static void replay_finish(void)
{
    FILE *f;
    if (g_script != (FILE *)0) {
        fclose(g_script);
        g_script = (FILE *)0;
    }
    f = fopen("DONE.TXT", "w");
    if (f != (FILE *)0) {
        fprintf(f, "done\n");
        fclose(f);
    }
    exit(0);
}

void fd2_replay_init(void)
{
    g_script = fopen(SCRIPT_NAME, "r");
    g_active = (g_script != (FILE *)0);
    fd2_play_heartbeat("init");
}

void fd2_replay_pump(void)
{
    char line[MAX_LINE];
    char cmd[16];
    unsigned sc, asc;

    if (g_active == 0) {
        return;
    }
    /* A key is still pending: let the game consume it before injecting more. */
    if (BIOS_KBD_TAIL != BIOS_KBD_HEAD) {
        return;
    }

    for (;;) {
        if (fgets(line, MAX_LINE, g_script) == (char *)0) {
            replay_finish();           /* EOF == END */
            return;                    /* not reached */
        }
        cmd[0] = '\0';
        if (sscanf(line, "%15s", cmd) != 1) {
            continue;                  /* blank line */
        }
        if (cmd[0] == '#') {
            continue;                  /* comment */
        }
        if (strcmp(cmd, "END") == 0) {
            replay_finish();
            return;                    /* not reached */
        }
        if (strcmp(cmd, "SEED") == 0) {
            sc = 0;
            sscanf(line, "%15s %x", cmd, &sc);
            data_fd2_shared_rng_seed = (uint16)sc;
            continue;
        }
        if (strcmp(cmd, "CAP") == 0) {
            if (g_cap_idx <= MAX_CAP) {
                fd2_play_capture(g_cap_idx);
            }
            g_cap_idx++;
            continue;
        }
        if (strcmp(cmd, "KEY") == 0) {
            sc = 0;
            asc = 0;
            sscanf(line, "%15s %x %x", cmd, &sc, &asc);
            fd2_play_heartbeat(cmd);
            kbd_inject((uint8)sc, (uint8)asc);
            return;                    /* let the game read this key */
        }
        /* unknown command -> ignore */
    }
}
