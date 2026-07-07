/*
 * probe.c -- warp/band/remap argument probe for the FD2 replay harness.
 *
 * Replay build only (-DFD2_REPLAY). Appends one line per instrumented call
 * to PROBE.TXT in the EXE cwd. Purpose: verify on a correct CPU (DOSBox-X)
 * that every value flowing through the warp -> band -> circle ->
 * palette-remap chain stays inside its legal range during the ch30 opening
 * summon scene (the reported 86Box-macOS crash trigger), i.e. that the
 * rebuild passes only legal arguments where the crash decode requires an
 * impossible one (src_y % 24 != 15, remap count <= 0).
 *
 * Line formats (all decimal):
 *   WT id=.. tx=.. ty=.. cx=.. cy=..   warp_teleport entry + cursor globals
 *   WO x=.. y=.. m=..                  out_collapse entry, m = src_y % 24
 *   BA c=.. b=.. r=.. t=.. e=..        band_anim entry (5 int args)
 *   CI c=.. y=.. r=.. s=.. a=.. e=..   circle_anim_row entry
 *   RV n=.. cnt=..                     remap-run INVARIANT VIOLATION
 *                                      (count < 1 or count > 0x138)
 *   RS n=.. min=.. max=.. bad=..       cumulative remap stats (at capture)
 *
 * The remap-run hook does no file I/O on the good path (counters only);
 * fd2_play_capture calls fd2_probe_summary to flush the RS line.
 *
 * Hook sites (all #ifdef FD2_REPLAY, decls in src/include/protos.h):
 *   spell/spellcin.c  fd2_animate_warp_teleport_char / _out_collapse
 *   gfx/rndscene.c    fd2_render_filled_circle_band_anim / _circle_anim_row
 *   gfx/palette.c     fd2_apply_palette_remap_run
 */

#include <stdio.h>
#include "types.h"
#include "protos.h"
#include "playharn.h"

#define PROBE_NAME  "PROBE.TXT"

static unsigned long g_remap_calls = 0;
static long          g_remap_min = 0x7FFFFFFFL;
static long          g_remap_max = -0x7FFFFFFFL;
static unsigned long g_remap_bad = 0;

static FILE *probe_open(void)
{
    return fopen(PROBE_NAME, "a");
}

void fd2_probe_warp(long char_id, long tile_x, long tile_y,
                    long cursor_x, long cursor_y)
{
    FILE *f = probe_open();
    if (f != (FILE *)0) {
        fprintf(f, "WT id=%ld tx=%ld ty=%ld cx=%ld cy=%ld\n",
                char_id, tile_x, tile_y, cursor_x, cursor_y);
        fclose(f);
    }
}

void fd2_probe_warpout(long src_x, long src_y)
{
    FILE *f = probe_open();
    if (f != (FILE *)0) {
        fprintf(f, "WO x=%ld y=%ld m=%ld\n",
                src_x, src_y, src_y % 24);
        fclose(f);
    }
}

void fd2_probe_band(long col, long bottom, long radius, long top, long end)
{
    FILE *f = probe_open();
    if (f != (FILE *)0) {
        fprintf(f, "BA c=%ld b=%ld r=%ld t=%ld e=%ld\n",
                col, bottom, radius, top, end);
        fclose(f);
    }
}

void fd2_probe_circle(long cx, long cy, long r, long scale,
                      long start_row, long end_row)
{
    FILE *f = probe_open();
    if (f != (FILE *)0) {
        fprintf(f, "CI c=%ld y=%ld r=%ld s=%ld a=%ld e=%ld\n",
                cx, cy, r, scale, start_row, end_row);
        fclose(f);
    }
}

void fd2_probe_remap(long count)
{
    FILE *f;
    g_remap_calls++;
    if (count < g_remap_min) {
        g_remap_min = count;
    }
    if (count > g_remap_max) {
        g_remap_max = count;
    }
    if (count < 1 || count > 0x138) {
        g_remap_bad++;
        f = probe_open();
        if (f != (FILE *)0) {
            fprintf(f, "RV n=%lu cnt=%ld\n", g_remap_calls, count);
            fclose(f);
        }
    }
}

void fd2_probe_summary(void)
{
    FILE *f = probe_open();
    if (f != (FILE *)0) {
        fprintf(f, "RS n=%lu min=%ld max=%ld bad=%lu\n",
                g_remap_calls, g_remap_min, g_remap_max, g_remap_bad);
        fclose(f);
    }
}
