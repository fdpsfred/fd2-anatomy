/* sfxdiag.c - Reproduce FD2's full SFX path to decide whether the problem is in
 * driver install or in actual PCM output. Replicates main's AIL init via the
 * INI path (AIL_install_MDI_INI / AIL_install_DIG_INI), then plays one FDOTHER
 * SFX sample exactly as fd2_play_sfx_with_handle does (entry+6 = offset,
 * entry+10 = end), and polls AIL_sample_status. The DOSBox conf wraps this in
 * `mixer wavstart/wavstop`, so the captured WAV shows whether real sound came
 * out (non-silent waveform) independent of any human listening.
 *
 * Each log line is open-append-CLOSE so a mid-run fault still leaves a trace.
 * Linked against the game's ailv3.lib + fd2common.lib, FD2 compile flags. */
#include "ailv3.h"
#include <stdio.h>
#include <stdlib.h>

extern char data_ail_diagnostic_message_scratch_buffer[];
#pragma aux data_ail_diagnostic_message_scratch_buffer "*";
extern void *data_ail_alloc_fnptr;
extern void *data_ail_free_fnptr;
#pragma aux data_ail_alloc_fnptr "*";
#pragma aux data_ail_free_fnptr "*";

static void logln(const char *s)
{
    FILE *f = fopen("SFXDIAG.LOG", "a");
    if (f) { fputs(s, f); fputc('\n', f); fclose(f); }
}

int main(void)
{
    char buf[220];
    HMDIDRIVER mdi;
    HDIGDRIVER dig;
    HSAMPLE hsfx;
    FILE *fp;
    unsigned char *bank;
    long banksize;
    unsigned long sfx_off, sfx_end;
    int st, i;
    FILE *t;

    t = fopen("SFXDIAG.LOG", "w");
    if (t) fclose(t);

    data_ail_alloc_fnptr = (void *)malloc;
    data_ail_free_fnptr = (void *)free;

    AIL_startup();
    logln("[1] AIL_startup ok");

    mdi = AIL_install_MDI_INI();
    sprintf(buf, "[2] install_MDI_INI = %p", (void *)mdi);
    logln(buf);

    dig = AIL_install_DIG_INI();
    sprintf(buf, "[3] install_DIG_INI = %p  err=%d  diag='%.80s'",
            (void *)dig, AIL_get_last_error_code(),
            data_ail_diagnostic_message_scratch_buffer);
    logln(buf);
    if (dig == 0) { logln("[X] DIG install failed -> stop"); AIL_shutdown(); return 1; }

    hsfx = AIL_allocate_sample_handle(dig);
    sprintf(buf, "[4] allocate_sample_handle = %p", (void *)hsfx);
    logln(buf);
    if (hsfx == 0) { logln("[X] sample handle failed"); AIL_shutdown(); return 2; }

    /* FDOTHER[0x1F] SFX sub-archive, pre-extracted to SFXBANK.DAT */
    fp = fopen("SFXBANK.DAT", "rb");
    if (fp == 0) { logln("[X] SFXBANK.DAT missing"); AIL_shutdown(); return 3; }
    fseek(fp, 0, SEEK_END);
    banksize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    bank = (unsigned char *)malloc((unsigned)banksize);
    fread(bank, 1, (unsigned)banksize, fp);
    fclose(fp);
    sprintf(buf, "[5] SFXBANK.DAT %ld bytes magic=%.6s", banksize, (char *)bank);
    logln(buf);

    /* SFX id 0, exactly as fd2_play_sfx_with_handle: entry=bank+0; off=[entry+6]; end=[entry+10] */
    sfx_off = *(unsigned long *)(bank + 6);
    sfx_end = *(unsigned long *)(bank + 10);
    sprintf(buf, "[6] sfx0 off=%lu end=%lu len=%lu", sfx_off, sfx_end, sfx_end - sfx_off);
    logln(buf);

    AIL_init_sample(hsfx);
    AIL_set_sample_address((int)hsfx, (unsigned)(bank + sfx_off),
                           (unsigned)(sfx_end - sfx_off));
    AIL_set_sample_loop_count(hsfx, 1);
    AIL_start_sample(hsfx);
    logln("[7] start_sample called");

    /* poll status for ~2s; status 4 = PLAYING, 2 = DONE. If the sample really
     * plays, status should be PLAYING then transition to DONE; a stuck or
     * never-PLAYING status hints at a silent DMA/IRQ failure. */
    for (i = 0; i < 8; i++) {
        st = AIL_sample_status(hsfx);
        sprintf(buf, "[8.%d] sample_status=%d", i, st);
        logln(buf);
        AIL_delay(250);
    }

    free(bank);
    AIL_shutdown();
    logln("[9] done");
    return 0;
}
