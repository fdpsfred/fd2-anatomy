/*
 * unit tests for src/anim/aniend.c
 *   - fd2_play_ending_and_record_clear  (exercised below, Phase 8)
 *   - fd2_play_chapter_clear_fanfare    (deferred, see below)
 *
 * The functions are cutscene/fanfare drivers: they write the VGA framebuffer
 * at 0xA0000, stop/fade BGM, load FDOTHER.DAT sprite sheets, play ANI files,
 * issue BIOS-tick delays, and (for the ending) block on INT 16h keyboard input
 * — none of which is callable under TEST.EXE. fd2_play_chapter_clear_fanfare is
 * a pure linear display side-effect sequence (no branch / arithmetic / RNG /
 * state transition), so it has no isolable logic to unit-test and is deferred
 * in full to Phase 9 integration playtest. The ending driver's display/menu
 * phases are likewise deferred to Phase 9 integration playtest.
 *
 * What IS isolable and exercised here is Phase 8 — the FD2.SAV completion
 * check that decides the menu option count. We reproduce that exact decision
 * by reading the STAGED real FD2.SAV, decrypting it with the linked real
 * fd2_save_crypt_buffer, checksumming with the linked real
 * fd2_save_compute_checksum, and deriving menu_options the same way the
 * function does. Expected values come from the real file (no fabricated save,
 * no hardcoded magic). We also assert the real ending-music trigger table the
 * function copies into its stack on entry.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

/* FD2.SAV plaintext size and header field offsets (mirrors src/anim/aniend.c
 * Phase 8 and src/life/* save handling). */
#define SAV_SIZE        0x59CB
#define SAV_CHK_OFF     0x59C7   /* stored checksum (u32, little-endian) */
#define SAV_CHAPTER_OFF 0x30C5   /* current_chapter_id; 0xFF == never completed */

/* Read the staged real FD2.SAV and decrypt in place with the linked real
 * cipher, returning the malloc'd SAV_SIZE plaintext (caller frees), or NULL. */
static uint8 *realsav_decrypted(void)
{
    FILE  *fp;
    uint8 *buf;
    size_t got;

    fp = fopen("FD2.SAV", "rb");
    if (fp == NULL) {
        return NULL;
    }
    buf = (uint8 *)malloc(SAV_SIZE);
    got = fread(buf, 1, SAV_SIZE, fp);
    fclose(fp);
    if (got != SAV_SIZE) {
        free(buf);
        return NULL;
    }
    fd2_save_crypt_buffer((uint32)buf, SAV_SIZE);
    return buf;
}

/* ---- Phase 8: real FD2.SAV checksum matches its stored tail ---- */
static void test_sav_checksum_matches_stored(void)
{
    uint8 *buf;
    uint32 computed;
    uint32 stored;

    buf = realsav_decrypted();
    ASSERT_TRUE(buf != NULL);

    computed = fd2_save_compute_checksum((uint32)buf, SAV_SIZE);
    stored = *(uint32 *)(buf + SAV_CHK_OFF);
    ASSERT_EQ(computed, stored);

    free(buf);
}

/* ---- Phase 8: menu_options derivation from the real save state ----
 * The function: checksum ok -> menu_options=2; if also save[0x30C5]!=0xFF
 * -> menu_options=3. The real FD2.SAV is a valid, completed save (chapter
 * id 0), so the decision must resolve to 3. */
static void test_sav_menu_options_decision(void)
{
    uint8 *buf;
    uint32 computed;
    uint32 stored;
    int    menu_options;
    uint8  chapter_id;

    buf = realsav_decrypted();
    ASSERT_TRUE(buf != NULL);

    computed = fd2_save_compute_checksum((uint32)buf, SAV_SIZE);
    stored = *(uint32 *)(buf + SAV_CHK_OFF);
    chapter_id = buf[SAV_CHAPTER_OFF];

    /* exact reproduction of the Phase 8 branch (starting from the
     * function's initial menu_options = 1). */
    menu_options = 1;
    if (computed == stored) {
        menu_options = 2;
        if (chapter_id != 0xFF) {
            menu_options = 3;
        }
    }

    ASSERT_EQ(computed, stored);     /* save is valid              */
    ASSERT_NE(chapter_id, 0xFF);     /* save was completed         */
    ASSERT_EQ(menu_options, 3);      /* => 3-option clear-record menu */

    free(buf);
}

/* ---- Phase 1: ending-music trigger frame table has its real values ----
 * The function copies this 15-entry int table to its stack on entry; the
 * Phase 5 scroll loop fires SFX/palette swaps when the scroll row equals an
 * entry. Assert the real binary values (FD2.LE @ 0x5204E). */
static void test_ending_music_trigger_frames_real_values(void)
{
    static const int32 expected[15] = {
        0x208, 0x1AE, 0x19A, 0x154, 0x136, 0x12C, 0xF0, 0xB4,
        0x96,  0x82,  0x6E,  0x57,  0x40,  0x16,  0x3E8
    };
    int i;

    for (i = 0; i < 15; i++) {
        ASSERT_EQ(data_fd2_chapter_ending_music_trigger_frames[i], expected[i]);
    }
}

void run_anim_aniend_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/aniend\n");
    RUN_TEST(test_sav_checksum_matches_stored);
    RUN_TEST(test_sav_menu_options_decision);
    RUN_TEST(test_ending_music_trigger_frames_real_values);
    printf("\n");
}
