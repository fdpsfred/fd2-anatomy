/* tau.c — FD2 game AIL usage scenario test
 *
 * S0: Baseline audio test (BGM + 5 SFX simultaneous, identical to Step 2 verified flow)
 * S1-S5: FD2 caller function patterns (Step 4)
 * S6: Remaining public API getter/setter coverage (Step 6)
 */
#include "ailv3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

typedef unsigned char  u8;
typedef unsigned int   u32;

extern char data_ail_diagnostic_message_scratch_buffer[];
#pragma aux data_ail_diagnostic_message_scratch_buffer "*";

static u8  *bgm_xmi_buf;  static u32 bgm_xmi_size;
static u8  *sfx_bank_buf; static u32 sfx_bank_size;

static int load_whole_file(const char *path, u8 **out_buf, u32 *out_size) {
    FILE *fp;  long  sz;  u8 *buf;
    fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END); sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return -2; }
    buf = (u8 *)malloc(sz);
    if (!buf) { fclose(fp); return -3; }
    if (fread(buf, 1, sz, fp) != (size_t)sz) { free(buf); fclose(fp); return -4; }
    fclose(fp);
    *out_buf = buf; *out_size = (u32)sz;
    return 0;
}

static void trace_line(const char *s) {
    FILE *tf = fopen("trace.log", "a");
    if (!tf) return;
    fprintf(tf, "%s\n", s);
    fclose(tf);
}

static void play_sfx_on_handle(void *handle, u8 *bank, int sfx_id, int loop) {
    u8 *entry;
    u32 off, end_off;
    AIL_stop_sample(handle);
    if (sfx_id == -1) return;
    entry = bank + sfx_id * 4;
    off = *(u32 *)(entry + 6);
    end_off = *(u32 *)(entry + 10);
    AIL_init_sample(handle);
    AIL_set_sample_address(handle, (u32)(bank + off), end_off - off);
    AIL_set_sample_loop_count(handle, loop);
    AIL_start_sample(handle);
}

int main(int argc, char **argv) {
    void *hmdi, *hdig, *hseq, *hsfx0, *hsfx1;
    FILE *tf;
    int   i;
    char  buf[80];
    (void)argc; (void)argv;

    tf = fopen("trace.log", "w"); if (!tf) return 99;
    fclose(tf);
    trace_line("[1] open trace");

    /* ================================================================
     * S0: Baseline audio (Step 2 verified flow — BGM + 5 SFX @ 1.5s)
     * This is byte-identical to the original tau.c that passed 10/10.
     * ================================================================ */
    trace_line("[S0] baseline audio test");

    AIL_startup();
    trace_line("[S0] AIL_startup OK");

    hmdi = (void *)AIL_install_MDI_INI();
    if (!hmdi) { trace_line("[S0_FAIL] MDI"); return 3; }
    trace_line("[S0] install_MDI_INI OK");

    hdig = (void *)AIL_install_DIG_INI();
    if (!hdig) { trace_line("[S0_FAIL] DIG"); return 4; }
    trace_line("[S0] install_DIG_INI OK");

    hseq = AIL_allocate_sequence_handle(hmdi);
    hsfx0 = AIL_allocate_sample_handle(hdig);
    hsfx1 = AIL_allocate_sample_handle(hdig);
    if (!hseq || !hsfx0) { trace_line("[S0_FAIL] handles"); return 5; }

    if (load_whole_file("BGM12.XMI", &bgm_xmi_buf, &bgm_xmi_size) != 0 ||
        load_whole_file("SFX_BANK.DAT", &sfx_bank_buf, &sfx_bank_size) != 0) {
        trace_line("[S0_FAIL] load files"); return 6;
    }
    trace_line("[S0] loaded BGM + SFX bank");

    /* BGM start — instant volume (original Step 2 pattern) */
    if (AIL_init_sequence(hseq, bgm_xmi_buf, 0) == 0) {
        trace_line("[S0_FAIL] init_sequence"); return 7;
    }
    AIL_set_sequence_volume(hseq, 127, 0);
    AIL_set_sequence_loop_count(hseq, 99);
    AIL_start_sequence(hseq);
    trace_line("[S0] BGM start; play 2s");
    AIL_delay(120);

    /* 5 SFX — original pattern (no stop before init, direct API calls) */
    for (i = 0; i < 5; i++) {
        int sfx_ids[5] = { 3, 1, 4, 2, 0 };
        int sfx_id = sfx_ids[i];
        u8 *entry = sfx_bank_buf + sfx_id * 4;
        u32 off = *(u32 *)(entry + 6);
        u32 end = *(u32 *)(entry + 10);
        sprintf(buf, "[S0] SFX[%d] off=0x%lx size=%lu", sfx_id,
                (unsigned long)off, (unsigned long)(end - off));
        trace_line(buf);
        AIL_init_sample(hsfx0);
        AIL_set_sample_address(hsfx0, (u32)(sfx_bank_buf + off), end - off);
        AIL_set_sample_loop_count(hsfx0, 1);
        AIL_start_sample(hsfx0);
        AIL_delay(90);
    }

    /* Stop BGM + cleanup for S0 */
    AIL_stop_sample(hsfx0);
    AIL_stop_sequence(hseq);
    trace_line("[S0_END] baseline audio OK");

    /* ================================================================
     * S4: fd2_set_bgm_track_with_fade — BGM with 2s fade-in
     * ================================================================ */
    trace_line("[S4] fd2_set_bgm_track_with_fade pattern");
    AIL_init_sequence(hseq, bgm_xmi_buf, 0);
    AIL_start_sequence(hseq);
    AIL_set_sequence_volume(hseq, 0, 0);
    AIL_set_sequence_volume(hseq, 127, 2000);
    AIL_set_sequence_loop_count(hseq, 99);
    trace_line("[S4] BGM started with 2s fade-in; play 2s");
    AIL_delay(120);
    trace_line("[S4_END]");

    /* ================================================================
     * S2: fd2_play_sfx_with_handle — SFX on handle 0 (FD2 pattern)
     * ================================================================ */
    trace_line("[S2] fd2_play_sfx_with_handle pattern (handle 0)");
    for (i = 0; i < 5; i++) {
        int sfx_ids[5] = { 3, 1, 4, 2, 0 };
        sprintf(buf, "[S2] SFX[%d] on h0", sfx_ids[i]);
        trace_line(buf);
        play_sfx_on_handle(hsfx0, sfx_bank_buf, sfx_ids[i], 1);
        AIL_delay(90);
    }
    play_sfx_on_handle(hsfx0, sfx_bank_buf, -1, 0);
    trace_line("[S2_END]");

    /* ================================================================
     * S3: fd2_play_sfx_sample_from_bank — SFX on handle 1
     * ================================================================ */
    if (hsfx1) {
        trace_line("[S3] fd2_play_sfx_sample_from_bank pattern (handle 1)");
        for (i = 0; i < 3; i++) {
            int sfx_ids[3] = { 4, 2, 0 };
            sprintf(buf, "[S3] SFX[%d] on h1", sfx_ids[i]);
            trace_line(buf);
            play_sfx_on_handle(hsfx1, sfx_bank_buf, sfx_ids[i], 1);
            AIL_delay(60);
        }
        play_sfx_on_handle(hsfx1, sfx_bank_buf, -1, 0);
        trace_line("[S3_END]");
    } else {
        trace_line("[S3_SKIP] no second sample handle");
    }

    /* ================================================================
     * S5: fd2_game_options_menu_loop — volume toggle
     * ================================================================ */
    trace_line("[S5] fd2_game_options_menu_loop volume toggle");
    AIL_set_sequence_volume(hseq, 0, 1000);
    trace_line("[S5] BGM muted (1s fade)");
    AIL_delay(90);
    AIL_set_sequence_volume(hseq, 127, 1000);
    trace_line("[S5] BGM restored (1s fade)");
    AIL_delay(90);
    trace_line("[S5_END]");

    /* ================================================================
     * S4b: fd2_set_bgm_track_with_fade(-1) — fade-out stop
     * ================================================================ */
    trace_line("[S4b] BGM fade-out stop (track_id=-1 pattern)");
    AIL_set_sequence_volume(hseq, 0, 4000);
    AIL_delay(60);
    AIL_stop_sequence(hseq);
    trace_line("[S4b_END]");

    /* ================================================================
     * S6: Remaining public API linkage + getter/setter test
     * ================================================================ */
    trace_line("[S6] remaining API coverage test");
    {
        int val;
        val = AIL_sample_status(hsfx0);
        sprintf(buf, "[S6] sample_status=%d", val);
        trace_line(buf);

        AIL_set_sample_type(hsfx0, 0, 0);
        AIL_set_sample_playback_rate(hsfx0, 11025);
        AIL_set_sample_volume(hsfx0, 100);
        AIL_set_sample_pan(hsfx0, 64);
        trace_line("[S6] sample set type/rate/vol/pan OK");

        val = AIL_sample_playback_rate(hsfx0);
        sprintf(buf, "[S6] sample_rate=%d", val);
        trace_line(buf);

        val = AIL_sample_volume(hsfx0);
        sprintf(buf, "[S6] sample_vol=%d", val);
        trace_line(buf);

        val = AIL_sample_pan(hsfx0);
        sprintf(buf, "[S6] sample_pan=%d", val);
        trace_line(buf);

        val = AIL_sample_loop_count(hsfx0);
        sprintf(buf, "[S6] sample_loop=%d", val);
        trace_line(buf);

        play_sfx_on_handle(hsfx0, sfx_bank_buf, 0, 1);
        AIL_delay(15);
        AIL_stop_sample(hsfx0);
        AIL_resume_sample(hsfx0);
        AIL_delay(15);
        AIL_end_sample(hsfx0);
        trace_line("[S6] stop/resume/end_sample OK");

        val = AIL_sequence_status(hseq);
        sprintf(buf, "[S6] seq_status=%d", val);
        trace_line(buf);

        AIL_init_sequence(hseq, bgm_xmi_buf, 0);
        AIL_start_sequence(hseq);
        AIL_set_sequence_volume(hseq, 80, 0);

        val = AIL_sequence_volume(hseq);
        sprintf(buf, "[S6] seq_vol=%d", val);
        trace_line(buf);

        val = AIL_sequence_tempo(hseq);
        sprintf(buf, "[S6] seq_tempo=%d", val);
        trace_line(buf);

        val = AIL_sequence_loop_count(hseq);
        sprintf(buf, "[S6] seq_loop=%d", val);
        trace_line(buf);

        {
            int beat, measure;
            AIL_sequence_position(hseq, &beat, &measure);
            sprintf(buf, "[S6] seq_pos beat=%d meas=%d", beat, measure);
            trace_line(buf);
        }

        AIL_set_sequence_tempo(hseq, 120);
        AIL_delay(30);
        AIL_stop_sequence(hseq);
        AIL_resume_sequence(hseq);
        AIL_delay(30);
        AIL_end_sequence(hseq);
        trace_line("[S6] set_tempo/stop/resume/end_seq OK");

        val = AIL_get_last_error_code();
        sprintf(buf, "[S6] last_error=%d", val);
        trace_line(buf);

        val = AIL_active_sample_count(hdig);
        sprintf(buf, "[S6] active_samples=%d", val);
        trace_line(buf);

        val = AIL_active_sequence_count(hmdi);
        sprintf(buf, "[S6] active_seqs=%d", val);
        trace_line(buf);

        val = AIL_interrupt_divisor();
        sprintf(buf, "[S6] int_divisor=%d", val);
        trace_line(buf);

        AIL_set_preference(12, 1);
        trace_line("[S6] set_preference OK");
    }
    trace_line("[S6_END]");

    /* ================================================================
     * Shutdown: fd2_main style — plain AIL_shutdown, NO uninstall
     * ================================================================ */
    trace_line("[SHUTDOWN] fd2_main-style plain AIL_shutdown");
    AIL_shutdown();
    trace_line("[SHUTDOWN_END] AIL_shutdown OK");

    trace_line("[FINAL]");
    free(bgm_xmi_buf); free(sfx_bank_buf);
    _exit(0);
    return 0;
}
