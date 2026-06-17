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
#include <dos.h>

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
    int st;
    FILE *t;

    t = fopen("SFXDIAG.LOG", "w");
    if (t) fclose(t);

    /* [D] delay-calibration probe: CLIB3S delay() vs BIOS tick @0x46C.
     * __delay_init (XI-chain ctor) calibrates data_crt_delay_calibration_counter
     * at CRT startup; this checks the resulting delay(ms) actually waits ms.
     * fd2's fade_in (0x41 x delay(2)) + delay(200) gate the shop-entry footstep
     * window, so a short delay() shrinks that window and cuts the footstep. */
    {
        unsigned long bt0, bt1;
        bt0 = *(volatile unsigned long *)0x46cUL;
        delay(1000);
        bt1 = *(volatile unsigned long *)0x46cUL;
        sprintf(buf, "[D0] delay(1000)=%lu BIOS ticks (1s expects ~18)",
                bt1 - bt0);
        logln(buf);
        bt0 = *(volatile unsigned long *)0x46cUL;
        delay(200);
        bt1 = *(volatile unsigned long *)0x46cUL;
        sprintf(buf, "[D1] delay(200)=%lu BIOS ticks (0.2s expects ~4)",
                bt1 - bt0);
        logln(buf);
    }

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

    /* Play a sequence of the longer FDOTHER[0x1F] SFX (each 0.2-0.6 s) so a
     * human listener can clearly tell sound-vs-silence. Status alone is
     * unreliable: it reaches DONE within one 250 ms poll regardless of loop
     * count (220x tested) or whether BLASTER IRQ matches the card -- so the
     * audible playout below is what actually answers "does PCM come out".
     * Each entry resolved exactly as fd2_play_sfx_with_handle:
     * entry = bank + id*4; off = [entry+6]; end = [entry+10]. */
    {
        static const int play_ids[8] = {1, 3, 4, 5, 8, 10, 11, 12};
        int n;
        for (n = 0; n < 8; n++) {
            int id;
            unsigned char *entry;
            id = play_ids[n];
            entry = bank + id * 4;
            sfx_off = *(unsigned long *)(entry + 6);
            sfx_end = *(unsigned long *)(entry + 10);
            AIL_init_sample(hsfx);
            AIL_set_sample_address(hsfx, (unsigned)(bank + sfx_off),
                                   (unsigned)(sfx_end - sfx_off));
            AIL_set_sample_loop_count(hsfx, 1);
            AIL_start_sample(hsfx);
            st = AIL_sample_status(hsfx);
            sprintf(buf, "[8.%d] sfx%d len=%lu status=%d",
                    n, id, sfx_end - sfx_off, st);
            logln(buf);
            AIL_delay(45);  /* ~0.75 s: cover playout + a short gap */
        }
    }

    free(bank);
    AIL_shutdown();
    logln("[9] done");
    return 0;
}
