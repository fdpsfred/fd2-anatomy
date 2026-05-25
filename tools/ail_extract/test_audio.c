/* test_audio.c - Verify rebuilt ailv3.lib + fd2common.lib can drive
 *                FD2-shipped MDI / DIG drivers and play FDMUS / FDOTHER assets.
 *
 * Build (DOSBox-X under Watcom 9.5a, 32-bit DOS/4G LE):
 *   wcc386 -bt=dos -mf -fpi87 -3r -zq -zp1 -d0 -i=. -i=%WATCOM%\H test_audio.c
 *   wlink @test_audio.lnk
 *
 * Run (host DOSBox-X silent mode):
 *   See tools/ail_extract/run_test.bat — mounts assets + redirects log/WAV.
 *
 * What it does:
 *   1. AIL_startup + hardcode IO_PARMS (driver name + IO/IRQ/DMA) per
 *      DOSBox-X SB16 + OPL3 emulation; bypasses AIL_API_read_INI because
 *      the .INI format for the FD2-shipped Miles build has not been audited
 *      yet (open issue).
 *   2. AIL_install_DIG_driver_file + _MDI_driver_file -> get handles
 *   3. Allocate sample + sequence handles
 *   4. Load FDMUS.DAT[0x12] XMI bytes -> AIL_init_sequence + start_sequence
 *   5. After 1s, load FDOTHER.DAT[0x1F] SFX sub-archive -> play sample idx 0
 *   6. Wait 3s, stop / release / shutdown
 *
 * AIL handle types currently exposed as `void *` (HSAMPLE / HDIGDRIVER /
 * etc typedefs pending Phase C audit per workspace/ail_extract/handoff.md).
 *
 * LLLLLL archive layout (FD2-internal container):
 *   [ 0.. 5] 6-byte magic "LLLLLL"
 *   [ 6.. 9] entry[0]_offset (uint32 LE) - data start of entry 0
 *   [10..13] entry[1]_offset            - data start of entry 1 (= end of 0)
 *   [ 6+N*4..9+N*4] entry[N]_offset
 *   data follows from entry[0]_offset onward.
 *
 * SFX entry layout inside FDOTHER[0x1F] (matches fd2_play_sfx_with_handle
 * @ 0x25a96 disasm):
 *   entry_ptr  = arc_base + sfx_id * 4   (LLLLLL offset slot)
 *   off        = *(uint32 *)(entry_ptr + 6)   (data start, after 6-byte magic)
 *   end        = *(uint32 *)(entry_ptr + 10)  (next entry's data start)
 *   sample_ptr = arc_base + off
 *   sample_len = end - off
 */
#include "ailv3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>

/* AIL allocator / free fn-ptr slots are normally initialised by FD2's
 * main bootstrap (`fd2_set_word_global_52758`), which test_audio.c skips.
 * We patch malloc / free in ourselves. */
extern void *data_ail_alloc_fnptr;
extern void *data_ail_free_fnptr;
#pragma aux data_ail_alloc_fnptr "*";
#pragma aux data_ail_free_fnptr "*";

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

/* ---- AIL config buffers (AIL_API_read_INI output layout, 264 bytes each:
 *      [0x000..0x07F] Device[128]   - device description
 *      [0x080..0x0FF] Driver[128]   - driver filename (e.g. "SB16.DIG")
 *      [0x100..0x101] short IO      - port
 *      [0x102..0x103] short IRQ
 *      [0x104..0x105] short DMA_8
 *      [0x106..0x107] short DMA_16
 *      AIL_install_*_driver_file takes driver-name ptr and io_parms ptr
 *      pointing to the IO/IRQ/DMA_8/DMA_16 short[4] starting at offset 0x100. */
static u8 mdi_cfg[264];
static u8 dig_cfg[264];

/* ---- Loaded asset buffers ---- */
static u8  *fdmus_xmi_buf;
static u32  fdmus_xmi_size;
static u8  *fdother_1f_buf;
static u32  fdother_1f_size;

/* ---- LLLLLL helpers ----------------------------------------------------- */

/* Load outer LLLLLL entry `idx` from .DAT file into freshly malloc'd buffer. */
static int load_dat_entry(const char *path, int idx, u8 **out_buf, u32 *out_size) {
    FILE *fp;
    u8   magic[6];
    u32  off0, off1;
    u8  *buf;

    fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "load_dat_entry: fopen %s failed\n", path); return -1; }
    if (fread(magic, 1, 6, fp) != 6 || memcmp(magic, "LLLLLL", 6) != 0) {
        fprintf(stderr, "load_dat_entry: %s missing LLLLLL magic\n", path);
        fclose(fp); return -2;
    }
    if (fseek(fp, 6 + idx * 4, SEEK_SET) != 0 ||
        fread(&off0, 4, 1, fp) != 1 ||
        fread(&off1, 4, 1, fp) != 1) {
        fprintf(stderr, "load_dat_entry: %s offset slot read failed\n", path);
        fclose(fp); return -3;
    }
    if (off1 <= off0) {
        fprintf(stderr, "load_dat_entry: %s idx %d empty/invalid (off0=%lu off1=%lu)\n",
                path, idx, (unsigned long)off0, (unsigned long)off1);
        fclose(fp); return -4;
    }
    *out_size = off1 - off0;
    buf = (u8 *)malloc(*out_size);
    if (!buf) { fclose(fp); return -5; }
    if (fseek(fp, off0, SEEK_SET) != 0 ||
        fread(buf, 1, *out_size, fp) != *out_size) {
        free(buf); fclose(fp); return -6;
    }
    fclose(fp);
    *out_buf = buf;
    return 0;
}

/* ---- Main --------------------------------------------------------------- */

int main(int argc, char **argv) {
    void *hmdi, *hdig;
    void *hseq, *hsfx;
    char *mdi_drv_name, *dig_drv_name;
    void *mdi_io_parms, *dig_io_parms;
    u8   *sfx_entry;
    u32   sfx_offset, sfx_end;
    int   sfx_id = 0;

    FILE *tf;
    (void)argc; (void)argv;
    tf = fopen("trace.log", "w");
    if (!tf) return 99;

    /* Patch AIL allocator slots before startup (FD2 main normally does
     * this via fd2_set_word_global_52758). */
    fprintf(tf, "[P0] &alloc_fnptr=%p before=%p &free_fnptr=%p before=%p\n",
            &data_ail_alloc_fnptr, data_ail_alloc_fnptr,
            &data_ail_free_fnptr, data_ail_free_fnptr); fflush(tf);
    data_ail_alloc_fnptr = (void *)malloc;
    data_ail_free_fnptr  = (void *)free;
    fprintf(tf, "[P1] alloc=%p free=%p\n", data_ail_alloc_fnptr, data_ail_free_fnptr); fflush(tf);

    /* AIL_DEBUG intentionally NOT set: the long AIL_startup path installs a
     * 100 Hz log-timer ISR via DPMI INT 21h ax=2508h, which faults under
     * DOSBox-X's DPMI host. The short path (no AIL_DEBUG env) still runs
     * AIL_internal_init_runtime_defaults, sufficient for driver install +
     * playback. We trace progress to our own trace.log. */
    fprintf(tf, "[A] before AIL_startup\n"); fflush(tf);

    /* 1. AIL startup. */
    AIL_startup();
    fprintf(tf, "[B] after AIL_startup\n"); fflush(tf);

    /* 2. Hardcode IO_PARMS for DOSBox-X SB16 + OPL3 emulation
     *    (matches default DOSBox-X [sblaster] sbbase=220 irq=5 dma=1 hdma=5
     *    and [midi] mpu401=intelligent at 0x388 OPL3 port). */
    memset(mdi_cfg, 0, sizeof(mdi_cfg));
    strcpy((char *)(mdi_cfg + 0x80), "PCSPKR.MDI");
    *(u16 *)(mdi_cfg + 0x100) = (u16)-1; /* PC speaker, no IO port */
    *(u16 *)(mdi_cfg + 0x102) = (u16)-1;
    *(u16 *)(mdi_cfg + 0x104) = (u16)-1;
    *(u16 *)(mdi_cfg + 0x106) = (u16)-1;

    memset(dig_cfg, 0, sizeof(dig_cfg));
    strcpy((char *)(dig_cfg + 0x80), "SB16.DIG");
    *(u16 *)(dig_cfg + 0x100) = 0x220;
    *(u16 *)(dig_cfg + 0x102) = 5;
    *(u16 *)(dig_cfg + 0x104) = 1;
    *(u16 *)(dig_cfg + 0x106) = 5;

    mdi_drv_name = (char *)(mdi_cfg + 0x80);
    mdi_io_parms = mdi_cfg + 0x100;
    dig_drv_name = (char *)(dig_cfg + 0x80);
    dig_io_parms = dig_cfg + 0x100;

    /* 3. Install drivers via file. */
    fprintf(tf, "[C] before install_MDI_driver_file\n"); fflush(tf);
    hmdi = AIL_install_MDI_driver_file(mdi_drv_name, mdi_io_parms);
    {
        /* The diagnostic-message scratch buffer carries the error string when
         * install fails silently (e.g. "Driver file not found"). */
        extern char data_ail_diagnostic_message_scratch_buffer[];
        fprintf(tf, "[D] hmdi=%p err=%d diag='%.80s'\n", hmdi,
                AIL_get_last_error_code(),
                data_ail_diagnostic_message_scratch_buffer);
        fflush(tf);
    }
    if (!hmdi) { fclose(tf); AIL_shutdown(); return 3; }
    fprintf(tf, "[E] before install_DIG_driver_file\n"); fflush(tf);
    hdig = AIL_install_DIG_driver_file(dig_drv_name, dig_io_parms);
    fprintf(tf, "[F] hdig=%p\n", hdig); fflush(tf);
    if (!hdig) { fclose(tf); AIL_shutdown(); return 4; }

    /* 4. Allocate handles. */
    fprintf(tf, "[G] before allocate handles\n"); fflush(tf);
    hseq = AIL_allocate_sequence_handle(hmdi);
    hsfx = AIL_allocate_sample_handle(hdig);
    fprintf(tf, "[H] hseq=%p hsfx=%p\n", hseq, hsfx); fflush(tf);
    if (!hseq || !hsfx) { fclose(tf); AIL_shutdown(); return 5; }

    /* 5. Load FDMUS[0x12] (main menu BGM XMI, ~9.8 KB). */
    if (load_dat_entry("FDMUS.DAT", 0x12, &fdmus_xmi_buf, &fdmus_xmi_size) != 0) {
        AIL_shutdown(); return 6;
    }
    fprintf(stderr, "FDMUS[0x12]: %lu bytes\n", (unsigned long)fdmus_xmi_size);

    /* 6. Init + start the MIDI sequence. */
    if (AIL_init_sequence(hseq, fdmus_xmi_buf, 0) == 0) {
        fprintf(stderr, "AIL_init_sequence failed\n");
        AIL_shutdown(); return 7;
    }
    AIL_set_sequence_volume(hseq, 127, 0);
    AIL_start_sequence(hseq);

    /* 7. Give the MIDI 1 second of solo playback. */
    AIL_delay(1000);

    /* 8. Load FDOTHER[0x1F] (UI / SFX sample sub-archive, ~31 KB). */
    if (load_dat_entry("FDOTHER.DAT", 0x1F, &fdother_1f_buf, &fdother_1f_size) != 0) {
        AIL_stop_sequence(hseq);
        AIL_shutdown(); return 8;
    }
    fprintf(stderr, "FDOTHER[0x1F]: %lu bytes\n", (unsigned long)fdother_1f_size);

    /* 9. Pick SFX entry idx 0 (per fd2_play_sfx_with_handle layout). */
    sfx_entry  = fdother_1f_buf + sfx_id * 4;
    sfx_offset = *(u32 *)(sfx_entry + 6);
    sfx_end    = *(u32 *)(sfx_entry + 10);
    if (sfx_end <= sfx_offset || sfx_end > fdother_1f_size) {
        fprintf(stderr, "SFX %d offset/end invalid (%lu..%lu of %lu)\n",
                sfx_id, (unsigned long)sfx_offset,
                (unsigned long)sfx_end, (unsigned long)fdother_1f_size);
    } else {
        AIL_init_sample(hsfx);
        AIL_set_sample_address((int)hsfx,
                               (u32)(fdother_1f_buf + sfx_offset),
                               sfx_end - sfx_offset);
        AIL_set_sample_loop_count(hsfx, 1);
        AIL_start_sample(hsfx);
    }

    /* 10. Let both run together for ~3 seconds. */
    AIL_delay(3000);

    /* 11. Cleanup. */
    fprintf(tf, "[Z] before cleanup\n"); fflush(tf);
    AIL_stop_sample(hsfx);
    AIL_stop_sequence(hseq);
    AIL_release_sample_handle(hsfx);
    AIL_release_sequence_handle(hseq);
    AIL_uninstall_DIG_driver(hdig);
    AIL_uninstall_MDI_driver(hmdi);
    AIL_shutdown();
    fprintf(tf, "[FINAL] done\n"); fflush(tf);
    fclose(tf);

    free(fdmus_xmi_buf);
    free(fdother_1f_buf);
    return 0;
}
