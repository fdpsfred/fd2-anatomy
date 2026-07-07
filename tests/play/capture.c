/*
 * capture.c -- checkpoint state dump for the FD2 replay harness.
 *
 * Replay build only (-DFD2_REPLAY). Writes the surfaces the host comparator
 * reads: the raw VGA framebuffer, the 256-entry DAC palette (so the host can
 * render a viewable PNG), and a fixed-layout snapshot of the key game globals
 * plus the active runtime_char array. All writes are fopen/fwrite/fclose so
 * DOSBox-X flushes them to the host on close.
 */

#include <stdio.h>
#include <conio.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "playharn.h"

/* VGA mode-13h visible framebuffer: 320x200 = 64000 bytes at 0xA0000.
 * Holds palette INDICES (not RGB), so the dump is immune to palette-cycle
 * timing -- only the drawn tiles/sprites affect it. */
#define FB_ADDR  0xA0000uL
#define FB_SIZE  64000u

/* STnn.BIN header: 16 little-endian int32 key globals, then the runtime_char
 * array. Host (tools/fd2_play/compare.py) parses by this fixed order. */
#define ST_HDR_WORDS   16
#define ST_MAX_CHARS   64       /* runtime array is sized per chapter; clamp */

void fd2_play_capture(int idx)
{
    char name[16];
    FILE *f;
    long hdr[ST_HDR_WORDS];
    uint8 pal[768];
    uint32 n;
    int k;

    sprintf(name, "FB%02d.BIN", idx);
    f = fopen(name, "wb");
    if (f != NULL) {
        fwrite((void *)FB_ADDR, 1, FB_SIZE, f);
        fclose(f);
    }

    /* DAC palette: 256 entries x (R,G,B), 6-bit each. Read via VGA ports
     * (3C7h = read index, 3C9h = data) -- works in DOS/4GW flat PM without a
     * real-mode buffer. Host scales 6->8 bit when rendering. */
    sprintf(name, "PAL%02d.BIN", idx);
    f = fopen(name, "wb");
    if (f != NULL) {
        outp(0x3C7, 0);
        for (k = 0; k < 768; k++) {
            pal[k] = (uint8)inp(0x3C9);
        }
        fwrite(pal, 1, 768, f);
        fclose(f);
    }

    hdr[0]  = (long)data_fd2_chapter_current_chapter_id;
    hdr[1]  = (long)data_fd2_chapter_event_or_battle_end_code;
    hdr[2]  = (long)data_fd2_battle_turn_counter;
    hdr[3]  = (long)data_fd2_battle_party_member_count;
    hdr[4]  = (long)data_fd2_shared_party_total_gold;
    hdr[5]  = (long)data_fd2_battle_cursor_world_x;
    hdr[6]  = (long)data_fd2_battle_cursor_world_y;
    hdr[7]  = (long)data_fd2_battle_cursor_screen_x;
    hdr[8]  = (long)data_fd2_battle_cursor_screen_y;
    hdr[9]  = (long)data_fd2_battle_view_window_origin_x;
    hdr[10] = (long)data_fd2_battle_view_window_origin_y;
    hdr[11] = (long)data_fd2_shared_rng_seed;
    hdr[12] = (long)data_fd2_battle_ai_best_physical_score;
    hdr[13] = (long)data_fd2_battle_ai_best_spell_score;
    hdr[14] = (long)data_fd2_battle_ai_best_item_score;
    hdr[15] = 0;                 /* reserved */

    sprintf(name, "ST%02d.BIN", idx);
    f = fopen(name, "wb");
    if (f != NULL) {
        fwrite(hdr, sizeof(long), ST_HDR_WORDS, f);
        n = data_fd2_battle_party_member_count;
        if (n > ST_MAX_CHARS) {
            n = ST_MAX_CHARS;
        }
        if (data_fd2_battle_runtime_char_array_ptr != (runtime_char *)0 && n > 0) {
            fwrite(data_fd2_battle_runtime_char_array_ptr, 0x50, n, f);
        }
        fclose(f);
    }

    fd2_probe_summary();   /* flush cumulative remap-run stats (probe.c) */
}
