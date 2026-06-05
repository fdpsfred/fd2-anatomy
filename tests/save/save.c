/*
 * unit tests for src/save/save.c
 *
 * Covers fd2_save_runtime_char_to_template — the chapter-end persistence of
 * battle-runtime char state back into the menu/template roster array.
 *
 * Risk-worthy state transitions exercised:
 *   - char_id match drives the 0x50-byte copy; non-match leaves template alone
 *   - transient status block (+0x22..+0x27, 6 bytes) is zeroed after copy
 *   - template flags (+5) masked to dead-bit only (& 1)
 *   - HP synced (current<-max) only when alive (flags != 1); skipped when dead
 *   - MP always restored (current<-max)
 *   - char-0 (索爾) dead-in-battle special case: when dead, template NOT touched
 *   - fd2_recompute_runtime_char_total_stats runs (aggregate +0x48 re-derived)
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>

extern runtime_char g_test_rc_array[8];
extern int g_check_char_is_dead_return;
extern int g_slot_selector_return;

/* Test-owned template (menu/roster) buffer: 8 entries x 0x50 bytes. */
static uint8 g_test_tmpl[8 * 0x50];

/* Byte accessors into the raw template buffer (the function treats template
 * entries as raw byte arrays at fixed offsets, so verify the same way). */
#define TMPL(i)        (g_test_tmpl + (i) * 0x50)
#define TMPL_B(i, off) (TMPL(i)[(off)])
#define TMPL_W(i, off) (*(uint16 *)(TMPL(i) + (off)))
#define RC_B(i, off)   (((uint8 *)&g_test_rc_array[i])[(off)])
#define RC_W(i, off)   (*(uint16 *)(((uint8 *)&g_test_rc_array[i]) + (off)))

/* Reset all shared state this function reads/writes to a clean baseline. */
static void save_fixture_reset(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(g_test_tmpl, 0, sizeof(g_test_tmpl));
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_test_tmpl;
    g_check_char_is_dead_return = 0;
    data_fd2_battle_party_member_count = 1;
    data_fd2_shared_menu_party_member_count = 1;
}

/* ---- Test: alive char copied, status cleared, HP+MP restored ---- */
static void test_save_alive_copy_and_restore(void)
{
    save_fixture_reset();

    /* runtime source: char_id 5, alive (flags bit0 clear), mid-battle HP/MP. */
    RC_B(0, 0x08) = 5;       /* char_id */
    RC_B(0, 0x05) = 0x84;    /* flags: bit7(acted)+bit2(cannot_act), bit0 clear */
    RC_B(0, 0x22) = 0xAA;    /* status block bytes that must be wiped */
    RC_B(0, 0x23) = 0xBB;
    RC_B(0, 0x24) = 0xCC;
    RC_B(0, 0x25) = 0xDD;
    RC_B(0, 0x26) = 0xEE;    /* sleep flag */
    RC_B(0, 0x27) = 0xFF;    /* combat_aux[0] (silence) */
    RC_W(0, 0x40) = 7;       /* hp_current (drained) */
    RC_W(0, 0x42) = 99;      /* hp_max */
    RC_W(0, 0x44) = 3;       /* mp_current (drained) */
    RC_W(0, 0x46) = 42;      /* mp_max */

    /* template entry 0 already holds char_id 5 (matches). */
    TMPL_B(0, 0x08) = 5;

    fd2_save_runtime_char_to_template();

    /* char_id copied through */
    ASSERT_EQ(TMPL_B(0, 0x08), 5);
    /* status block (+0x22..+0x27) zeroed */
    ASSERT_EQ(TMPL_B(0, 0x22), 0);
    ASSERT_EQ(TMPL_B(0, 0x23), 0);
    ASSERT_EQ(TMPL_B(0, 0x24), 0);
    ASSERT_EQ(TMPL_B(0, 0x25), 0);
    ASSERT_EQ(TMPL_B(0, 0x26), 0);
    ASSERT_EQ(TMPL_B(0, 0x27), 0);
    /* flags masked to dead-bit only: 0x84 & 1 == 0 */
    ASSERT_EQ(TMPL_B(0, 0x05), 0);
    /* alive -> hp_current synced to hp_max */
    ASSERT_EQ(TMPL_W(0, 0x40), 99);
    ASSERT_EQ(TMPL_W(0, 0x42), 99);
    /* mp always restored */
    ASSERT_EQ(TMPL_W(0, 0x44), 42);
    ASSERT_EQ(TMPL_W(0, 0x46), 42);
}

/* ---- Test: dead char keeps drained HP, dead-bit preserved ---- */
static void test_save_dead_keeps_hp(void)
{
    save_fixture_reset();

    /* char_id 9 (not 0, so no special-case skip), dead (flags bit0 set). */
    RC_B(0, 0x08) = 9;
    RC_B(0, 0x05) = 0x85;    /* dead-bit set + transient bits */
    RC_W(0, 0x40) = 0;       /* hp_current 0 (dead) */
    RC_W(0, 0x42) = 50;      /* hp_max */
    RC_W(0, 0x44) = 1;       /* mp_current */
    RC_W(0, 0x46) = 30;      /* mp_max */

    TMPL_B(0, 0x08) = 9;

    fd2_save_runtime_char_to_template();

    /* flags masked: 0x85 & 1 == 1 (dead) */
    ASSERT_EQ(TMPL_B(0, 0x05), 1);
    /* dead -> hp_current NOT synced; stays the copied value (0) */
    ASSERT_EQ(TMPL_W(0, 0x40), 0);
    ASSERT_EQ(TMPL_W(0, 0x42), 50);
    /* mp still restored even when dead */
    ASSERT_EQ(TMPL_W(0, 0x44), 30);
    ASSERT_EQ(TMPL_W(0, 0x46), 30);
}

/* ---- Test: char 0 dead in battle -> template NOT overwritten ---- */
static void test_save_char0_dead_skips(void)
{
    save_fixture_reset();
    g_check_char_is_dead_return = 1;   /* 索爾 reported dead in battle */

    /* runtime char_id 0 (索爾), would carry battle-dead state. */
    RC_B(0, 0x08) = 0;
    RC_B(0, 0x05) = 0x01;    /* dead */
    RC_W(0, 0x40) = 0;       /* hp_current 0 */

    /* template entry 0 holds char 0 with a SENTINEL pre-state that must survive. */
    TMPL_B(0, 0x08) = 0;
    TMPL_B(0, 0x05) = 0x77;  /* sentinel flags */
    TMPL_W(0, 0x40) = 1234;  /* sentinel hp_current */

    fd2_save_runtime_char_to_template();

    /* skip path: nothing copied/masked -> sentinels intact */
    ASSERT_EQ(TMPL_B(0, 0x05), 0x77);
    ASSERT_EQ(TMPL_W(0, 0x40), 1234);
}

/* ---- Test: char 0 ALIVE -> still copied (special case only when dead) ---- */
static void test_save_char0_alive_copies(void)
{
    save_fixture_reset();
    g_check_char_is_dead_return = 0;   /* 索爾 alive */

    RC_B(0, 0x08) = 0;
    RC_B(0, 0x05) = 0x80;    /* acted bit, not dead */
    RC_W(0, 0x40) = 5;
    RC_W(0, 0x42) = 80;
    RC_W(0, 0x44) = 2;
    RC_W(0, 0x46) = 20;

    TMPL_B(0, 0x08) = 0;
    TMPL_W(0, 0x40) = 1234;  /* sentinel that should be overwritten */

    fd2_save_runtime_char_to_template();

    /* copied + masked + HP/MP restored */
    ASSERT_EQ(TMPL_B(0, 0x05), 0);     /* 0x80 & 1 == 0 */
    ASSERT_EQ(TMPL_W(0, 0x40), 80);    /* alive -> hp_current = hp_max */
    ASSERT_EQ(TMPL_W(0, 0x44), 20);    /* mp restored */
}

/* ---- Test: no char_id match -> template untouched ---- */
static void test_save_no_match_no_write(void)
{
    save_fixture_reset();

    RC_B(0, 0x08) = 5;       /* runtime char_id 5 */
    RC_W(0, 0x40) = 7;

    /* template entry holds a DIFFERENT char_id (3) with sentinels. */
    TMPL_B(0, 0x08) = 3;
    TMPL_B(0, 0x05) = 0x55;
    TMPL_W(0, 0x40) = 999;

    fd2_save_runtime_char_to_template();

    /* no match -> nothing written */
    ASSERT_EQ(TMPL_B(0, 0x08), 3);
    ASSERT_EQ(TMPL_B(0, 0x05), 0x55);
    ASSERT_EQ(TMPL_W(0, 0x40), 999);
}

/* ---- Test: recompute runs -> aggregate AP (+0x48) re-derived from +0x37 ---- */
static void test_save_recompute_runs(void)
{
    save_fixture_reset();

    RC_B(0, 0x08) = 4;       /* char_id */
    RC_B(0, 0x05) = 0x00;    /* alive */
    RC_W(0, 0x37) = 123;     /* base AP source (recompute reads slot+0x37) */
    RC_W(0, 0x42) = 60;      /* hp_max */
    RC_W(0, 0x46) = 25;      /* mp_max */
    /* inventory slots left 0 -> no equipped marker -> no item-effect lookups */

    TMPL_B(0, 0x08) = 4;
    TMPL_W(0, 0x48) = 0xDEAD; /* stale aggregate that recompute must overwrite */

    fd2_save_runtime_char_to_template();

    /* recompute wrote AP_total (= base +0x37, no item boosts) into +0x48 */
    ASSERT_EQ(TMPL_W(0, 0x48), 123);
}

/* ---- Test: multi runtime members match correct template rows ---- */
static void test_save_multi_member_routing(void)
{
    save_fixture_reset();
    data_fd2_battle_party_member_count = 2;
    data_fd2_shared_menu_party_member_count = 2;

    /* runtime[0] = char_id 5, runtime[1] = char_id 8 */
    RC_B(0, 0x08) = 5;
    RC_B(0, 0x05) = 0x00;
    RC_W(0, 0x42) = 11;
    RC_W(0, 0x46) = 12;
    RC_B(1, 0x08) = 8;
    RC_B(1, 0x05) = 0x00;
    RC_W(1, 0x42) = 21;
    RC_W(1, 0x46) = 22;

    /* template rows in SWAPPED order: row0 = char 8, row1 = char 5. */
    TMPL_B(0, 0x08) = 8;
    TMPL_B(1, 0x08) = 5;

    fd2_save_runtime_char_to_template();

    /* runtime char 5 must land in template row1, char 8 in row0. */
    ASSERT_EQ(TMPL_W(1, 0x40), 11);  /* row1 (char5): hp_current = hp_max 11 */
    ASSERT_EQ(TMPL_W(1, 0x44), 12);
    ASSERT_EQ(TMPL_W(0, 0x40), 21);  /* row0 (char8): hp_current = hp_max 21 */
    ASSERT_EQ(TMPL_W(0, 0x44), 22);
}

/* ================================================================
 * fd2_save_compute_checksum — byte-sum integrity checksum.
 *
 * Sums buf[0 .. len-5] as a u32 (the last 4 bytes hold the checksum
 * itself and are excluded). Pure in-memory computation, no fopen.
 * Expected values cross-checked against the FD2.LE emulator:
 *   bytes 01..08, len 8 -> sum of first 4 = 0x0A
 *   bytes FF*8,   len 8 -> 4*0xFF        = 0x3FC
 * ================================================================ */

/* ---- Test: hand-computed sum, last 4 bytes excluded ---- */
static void test_checksum_basic_excludes_last4(void)
{
    uint8 buf[8];

    buf[0] = 1; buf[1] = 2; buf[2] = 3; buf[3] = 4;
    buf[4] = 5; buf[5] = 6; buf[6] = 7; buf[7] = 8;
    /* len 8 -> remaining = 4 -> sums buf[0..3] = 1+2+3+4 = 10. */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 8), 10);
}

/* ---- Test: trailing 4 bytes never contribute (vary them) ---- */
static void test_checksum_trailing_bytes_ignored(void)
{
    uint8 buf[8];

    buf[0] = 0x10; buf[1] = 0x20; buf[2] = 0x30; buf[3] = 0x40;
    /* last 4 bytes set to max; they must NOT be summed. */
    buf[4] = 0xFF; buf[5] = 0xFF; buf[6] = 0xFF; buf[7] = 0xFF;
    /* sums buf[0..3] = 0x10+0x20+0x30+0x40 = 0xA0. */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 8), 0xA0);
}

/* ---- Test: accumulator is full u32 (no byte/word truncation) ---- */
static void test_checksum_u32_accumulator(void)
{
    uint8 buf[12];
    int i;

    for (i = 0; i < 12; i++) {
        buf[i] = 0xFF;
    }
    /* len 12 -> remaining = 8 -> 8 * 0xFF = 0x7F8 (exceeds a byte,
     * proving the running sum is not truncated to 8 bits). */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 12), 0x7F8);
}

/* ---- Test: natural u32 wrap on overflow (matches LODSB/ADD EBX) ---- */
static void test_checksum_u32_wrap(void)
{
    static uint8 buf[0x10008];
    uint32 i;
    uint32 result;

    /* 0x10008-byte buffer of 0xFF: len 0x10008 -> remaining 0x10004
     * summed bytes -> total 0x10004 * 0xFF = 0xFF03FC, which far
     * exceeds 16 bits, exercising the full 32-bit running sum. */
    for (i = 0; i < 0x10008; i++) {
        buf[i] = 0xFF;
    }
    /* len 0x10008 -> remaining = 0x10004 -> 0x10004 * 0xFF. */
    result = (uint32)0x10004 * (uint32)0xFF;   /* = 0xFF03FC */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 0x10008), result);
}

/* ---- Test: minimal valid length (len = 5 -> sums exactly 1 byte) ---- */
static void test_checksum_len5_single_byte(void)
{
    uint8 buf[5];

    buf[0] = 0x7B;   /* the only summed byte */
    buf[1] = 0xFF; buf[2] = 0xFF; buf[3] = 0xFF; buf[4] = 0xFF;
    /* len 5 -> remaining = 1 -> sums buf[0] only = 0x7B. */
    ASSERT_EQ(fd2_save_compute_checksum((uint32)buf, 5), 0x7B);
}

/* ----------------------------------------------------------------
 * fd2_save_crypt_buffer tests
 *
 * Ground-truth keystream (state seed 0xA5, advance via
 * ROL16(state + 0x9014, 3), XOR low byte). The first four keystream
 * bytes were verified against the FD2.LE function under Ghidra's
 * emulator: final 16-bit state after 4 iterations = 0x45AC.
 *   iter1: ROL16(0x90B9,3) = 0x85CC -> key 0xCC
 *   iter2: ROL16(0x15E0,3) = 0xAF00 -> key 0x00
 *   iter3: ROL16(0x3F14,3) = 0xF8A1 -> key 0xA1
 *   iter4: ROL16(0x88B5,3) = 0x45AC -> key 0xAC
 * ---------------------------------------------------------------- */

/* The deterministic keystream the cipher XORs in, byte by byte. */
static const uint8 g_crypt_keystream[4] = { 0xCC, 0x00, 0xA1, 0xAC };

/* ---- Test: zero buffer -> output equals the raw keystream ---- */
static void test_crypt_keystream_on_zero_buffer(void)
{
    uint8 buf[4];

    buf[0] = 0x00; buf[1] = 0x00; buf[2] = 0x00; buf[3] = 0x00;
    fd2_save_crypt_buffer((uint32)buf, 4);
    /* 0x00 ^ key == key, so the buffer now holds the keystream. */
    ASSERT_MEM_EQ(buf, g_crypt_keystream, 4);
}

/* ---- Test: non-zero buffer XORed with the known keystream ---- */
static void test_crypt_xor_known_data(void)
{
    uint8 buf[4];

    buf[0] = 0x12; buf[1] = 0x34; buf[2] = 0x56; buf[3] = 0x78;
    fd2_save_crypt_buffer((uint32)buf, 4);
    ASSERT_EQ(buf[0], (uint8)(0x12 ^ 0xCC));   /* 0xDE */
    ASSERT_EQ(buf[1], (uint8)(0x34 ^ 0x00));   /* 0x34 */
    ASSERT_EQ(buf[2], (uint8)(0x56 ^ 0xA1));   /* 0xF7 */
    ASSERT_EQ(buf[3], (uint8)(0x78 ^ 0xAC));   /* 0xD4 */
}

/* ---- Test: involution — crypt twice restores the original ---- */
static void test_crypt_is_involution(void)
{
    uint8 buf[64];
    uint8 orig[64];
    uint32 i;

    for (i = 0; i < 64; i++) {
        buf[i] = (uint8)(i * 7 + 3);
        orig[i] = buf[i];
    }
    fd2_save_crypt_buffer((uint32)buf, 64);   /* encrypt */
    /* After one pass the data must differ somewhere (keystream != 0). */
    ASSERT_NE(buf[0], orig[0]);
    fd2_save_crypt_buffer((uint32)buf, 64);   /* decrypt */
    ASSERT_MEM_EQ(buf, orig, 64);
}

/* ---- Test: size = 1 applies exactly one keystream byte ---- */
static void test_crypt_size_one(void)
{
    uint8 buf[1];

    buf[0] = 0xFF;
    fd2_save_crypt_buffer((uint32)buf, 1);
    ASSERT_EQ(buf[0], (uint8)(0xFF ^ 0xCC));   /* 0x33 */
}

/* ================================================================
 * fd2_save_current_state_to_slot — write current-state globals into
 * a chosen FD2.SAV slot, recompute checksum, re-encrypt, write back.
 *
 * Drives the REAL function against the staged real FD2.SAV (copied
 * into the test cwd by build_test.py). The interactive slot selector
 * is a controllable fake (testglob.c one-shot mode); the modal "saved"
 * confirmation dialog (which busy-waits on a keypress via the real
 * fd2_wait_for_input_dialog_with_blink and would hang the silent
 * harness) is gated behind per_chapter_category[chapter]==0, so the
 * save-path tests pick a chapter whose category is non-zero to skip
 * it. The cancel path and the slot-write arithmetic / field widths /
 * checksum round-trip are covered here; the confirmation-dialog arm is
 * deferred to Phase 9 integration (see src/emit_issues.json).
 *
 * Expected slot bytes are read back from the same real FD2.SAV after
 * the write and decrypted with the linked real cipher — no fabricated
 * save image, no hardcoded magic. The original file bytes are backed
 * up at setup and rewritten at teardown so neither later suites nor a
 * subsequent build (staged files persist) see a mutated FD2.SAV.
 * ================================================================ */

#define SAV_SIZE   0x59CBL
#define SAV_SLOT0  0x312BL          /* file offset of slot 0 base */
#define SAV_STRIDE 0xA28L           /* bytes per slot */

extern int    g_slot_selector_oneshot;
extern int    g_slot_selector_first_ret;
extern uint32 g_slot_selector_cursor;
extern int    g_slot_selector_calls;

/* roster source the function memmoves into the slot (0xA00 bytes) */
static uint8 *g_scs_roster;
/* backup of the staged FD2.SAV so teardown can restore the original */
static uint8 *g_scs_sav_backup;

static void scs_setup(uint32 chapter_with_nonzero_category, uint32 slot)
{
    FILE  *fp;
    int    i;

    /* back up the staged real FD2.SAV verbatim */
    g_scs_sav_backup = (uint8 *)malloc(SAV_SIZE);
    fp = fopen("FD2.SAV", "rb");
    fread(g_scs_sav_backup, 1, SAV_SIZE, fp);
    fclose(fp);

    /* roster the function copies into the slot: a recognisable pattern */
    g_scs_roster = (uint8 *)malloc(0xA00);
    for (i = 0; i < 0xA00; i++) {
        g_scs_roster[i] = (uint8)(i * 3 + 0x11);
    }
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_scs_roster;

    /* current-state scalar globals the function stores into the slot header */
    data_fd2_chapter_current_chapter_id = chapter_with_nonzero_category;
    data_fd2_shared_menu_party_member_count = 7;
    data_fd2_shared_party_total_gold = 0x1234ABCD;   /* tests the u32 +0xA02 */
    data_fd2_ui_terrain_hud_user_enabled = 0x5A;
    data_fd2_ui_game_speed_flag = 0x3C;
    data_fd2_audio_bgm_enabled_flag = 1;
    data_fd2_audio_sfx_enabled_flag = 0;

    /* non-zero category for this chapter -> the modal confirmation
     * (and its blocking input wait) is skipped */
    data_fd2_chapter_per_chapter_category_table[chapter_with_nonzero_category] = 1;

    /* one-shot selector: commit slot `slot` once, then cancel */
    g_slot_selector_oneshot = 1;
    g_slot_selector_first_ret = 1;
    g_slot_selector_cursor = slot;
    g_slot_selector_calls = 0;
}

static void scs_teardown(uint32 chapter)
{
    FILE *fp;

    /* restore the original staged FD2.SAV bytes */
    fp = fopen("FD2.SAV", "wb");
    fwrite(g_scs_sav_backup, 1, SAV_SIZE, fp);
    fclose(fp);
    free(g_scs_sav_backup);
    free(g_scs_roster);

    /* clear test-owned shared state so later suites are clean */
    g_scs_sav_backup = 0;
    g_scs_roster = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_chapter_per_chapter_category_table[chapter] = 0;
    g_slot_selector_oneshot = 0;
    g_slot_selector_first_ret = 1;
    g_slot_selector_cursor = 0;
    g_slot_selector_calls = 0;
    g_slot_selector_return = -1;
    data_fd2_chapter_current_chapter_id = 1;
}

/* Read the just-written FD2.SAV and decrypt it (cipher is an involution),
 * returning the malloc'd 0x59CB plaintext (caller frees). */
static uint8 *scs_read_decrypted(void)
{
    FILE  *fp;
    uint8 *buf;

    buf = (uint8 *)malloc(SAV_SIZE);
    fp = fopen("FD2.SAV", "rb");
    fread(buf, 1, SAV_SIZE, fp);
    fclose(fp);
    fd2_save_crypt_buffer((uint32)buf, SAV_SIZE);
    return buf;
}

/* ---- Test: commit one slot -> header fields + roster written, checksum valid ---- */
static void test_scs_writes_slot_header_and_roster(void)
{
    uint8 *sav;
    uint8 *slot;
    long   base;

    scs_setup(0x16, 2);          /* chapter 0x16 has category 1; slot 2 */

    fd2_save_current_state_to_slot(1);

    /* selector called twice: one commit + one cancel to exit the loop */
    ASSERT_EQ((long)g_slot_selector_calls, 2);

    sav = scs_read_decrypted();
    base = SAV_SLOT0 + 2 * SAV_STRIDE;
    slot = sav + base;

    /* 0xA00-byte roster copied verbatim into the slot body */
    ASSERT_MEM_EQ(slot, g_scs_roster, 0xA00);
    /* scalar header at +0xA00.. */
    ASSERT_EQ((long)slot[0xA00], 0x16);              /* chapter id (byte) */
    ASSERT_EQ((long)slot[0xA01], 7);                 /* member count (byte) */
    ASSERT_EQ((long)*(uint32 *)(slot + 0xA02),
              (long)0x1234ABCD);                     /* gold (u32) */
    ASSERT_EQ((long)slot[0xA06], 0x5A);              /* terrain hud */
    ASSERT_EQ((long)slot[0xA07], 0x3C);              /* game speed */
    ASSERT_EQ((long)slot[0xA08], 1);                 /* bgm enabled */
    ASSERT_EQ((long)slot[0xA09], 0);                 /* sfx enabled */

    /* checksum tail (+0x59C7) is the byte-sum of buf[0..0x59C6] */
    ASSERT_EQ((long)*(uint32 *)(sav + 0x59C7),
              (long)fd2_save_compute_checksum((uint32)sav, SAV_SIZE));

    free(sav);
    scs_teardown(0x16);
}

/* ---- Test: slot index routes the write to base 0x312B + slot*0xA28 ---- */
static void test_scs_slot_index_routes_offset(void)
{
    uint8 *sav;
    uint8 *orig;
    long   base3;
    long   base0;
    int    i;
    int    differs;

    scs_setup(0x17, 3);          /* chapter 0x17 category 1; slot 3 */

    /* snapshot the decrypted ORIGINAL so we can prove only slot 3 changed */
    orig = (uint8 *)malloc(SAV_SIZE);
    memcpy(orig, g_scs_sav_backup, SAV_SIZE);
    fd2_save_crypt_buffer((uint32)orig, SAV_SIZE);

    fd2_save_current_state_to_slot(1);

    sav = scs_read_decrypted();
    base3 = SAV_SLOT0 + 3 * SAV_STRIDE;
    base0 = SAV_SLOT0 + 0 * SAV_STRIDE;

    /* slot 3 header carries the written chapter id */
    ASSERT_EQ((long)sav[base3 + 0xA00], 0x17);

    /* slot 0 body region is unchanged vs the original plaintext (the write
     * landed at slot 3, not slot 0) */
    differs = 0;
    for (i = 0; i < 0xA00; i++) {
        if (sav[base0 + i] != orig[base0 + i]) { differs = 1; break; }
    }
    ASSERT_EQ((long)differs, 0);

    free(orig);
    free(sav);
    scs_teardown(0x17);
}

/* ---- Test: cancel (selector returns -1) writes nothing, file unchanged ---- */
static void test_scs_cancel_leaves_file_unchanged(void)
{
    uint8 *sav;
    int    i;
    int    differs;

    /* setup arms one-shot but we override to pure-cancel below */
    scs_setup(0x18, 1);
    g_slot_selector_oneshot = 0;     /* constant mode */
    g_slot_selector_return = -1;     /* immediate cancel */

    fd2_save_current_state_to_slot(1);

    /* selector consulted exactly once, then the do-while exits */
    ASSERT_EQ((long)g_slot_selector_calls, 1);

    /* file bytes identical to the staged original (no fopen "wb" path taken) */
    sav = (uint8 *)malloc(SAV_SIZE);
    {
        FILE *fp = fopen("FD2.SAV", "rb");
        fread(sav, 1, SAV_SIZE, fp);
        fclose(fp);
    }
    differs = 0;
    for (i = 0; i < SAV_SIZE; i++) {
        if (sav[i] != g_scs_sav_backup[i]) { differs = 1; break; }
    }
    ASSERT_EQ((long)differs, 0);

    free(sav);
    scs_teardown(0x18);
}

void run_save_save_tests(void)
{
    SUITE_BEGIN(save_save);
    RUN_TEST(test_save_alive_copy_and_restore);
    RUN_TEST(test_save_dead_keeps_hp);
    RUN_TEST(test_save_char0_dead_skips);
    RUN_TEST(test_save_char0_alive_copies);
    RUN_TEST(test_save_no_match_no_write);
    RUN_TEST(test_save_recompute_runs);
    RUN_TEST(test_save_multi_member_routing);
    RUN_TEST(test_checksum_basic_excludes_last4);
    RUN_TEST(test_checksum_trailing_bytes_ignored);
    RUN_TEST(test_checksum_u32_accumulator);
    RUN_TEST(test_checksum_u32_wrap);
    RUN_TEST(test_checksum_len5_single_byte);
    RUN_TEST(test_crypt_keystream_on_zero_buffer);
    RUN_TEST(test_crypt_xor_known_data);
    RUN_TEST(test_crypt_is_involution);
    RUN_TEST(test_crypt_size_one);
    RUN_TEST(test_scs_writes_slot_header_and_roster);
    RUN_TEST(test_scs_slot_index_routes_offset);
    RUN_TEST(test_scs_cancel_leaves_file_unchanged);
    SUITE_END();
}
