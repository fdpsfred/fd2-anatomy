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
/* minipfix.h: immediate-END text table + sprite sheet + dialog/blit spies, so
 * the load path's real fd2_load_chapter_portrait / fd2_display_dialog_scene /
 * fd2_paint_portrait_to_dialog_area run without VGA, and the dialog returns at
 * the END opcode without consuming the injected keystroke. */
#include "minipfix.h"
/* savefix.h: stages the real fd2_save_slot_selector_ui's setup deps (sprite
 * atlas for the panel-header blit arg + all-END grid text) and queues the
 * scancode sequence the picker's int386(0x16) reads one-per-poll. */
#include "savefix.h"

extern runtime_char g_test_rc_array[8];

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

    /* runtime char_id 0 (索爾), dead in battle (flags bit0 set) -> the real
     * fd2_check_char_is_dead(0) returns 1, triggering the skip path. */
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

    /* runtime char_id 0 (索爾) alive (flags bit0 clear) -> is_dead(0)==0, so the
     * char-0 skip special case does NOT fire and the entry is copied. */
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

/* roster source the function memmoves into the slot (0xA00 bytes) */
static uint8 *g_scs_roster;
/* backup of the staged FD2.SAV so teardown can restore the original */
static uint8 *g_scs_sav_backup;

/* Down-navigate to slot `slot` then commit: the real picker resets the cursor
 * to 0, so reaching slot N takes N Down keys (0x50) then Enter (0x1C). Returns
 * the count written into `codes`. */
static int scs_commit_keys(int *codes, uint32 slot)
{
    int n = 0;
    uint32 k;
    for (k = 0; k < slot; k++) {
        codes[n++] = 0x50;            /* Down */
    }
    codes[n++] = 0x1C;                /* Enter = commit */
    return n;
}

static void scs_setup(uint32 chapter_with_nonzero_category, uint32 slot)
{
    FILE  *fp;
    int    i;

    (void)slot;

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

    /* stage the real picker's setup deps (sprite atlas + all-END grid text) */
    savefix_setup_env();
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

    /* reclaim the 3 workspaces the last picker call leaked */
    savefix_free_selector_workspaces();

    /* clear test-owned shared state so later suites are clean */
    g_scs_sav_backup = 0;
    g_scs_roster = 0;
    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_chapter_per_chapter_category_table[chapter] = 0;
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
    int    keys[8];
    int    n;

    scs_setup(0x16, 2);          /* chapter 0x16 has category 1; slot 2 */

    /* picker call 1: Down,Down,Enter -> commit slot 2; call 2: Esc -> cancel
     * (exits the save loop). All keys queued upfront; the real int386(0x16)
     * reads one per poll. */
    n = scs_commit_keys(keys, 2);
    keys[n++] = 0x01;            /* Esc = cancel */
    savefix_queue_scancodes(keys, n);

    fd2_save_current_state_to_slot(1);

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

    int    keys[8];
    int    n;

    scs_setup(0x17, 3);          /* chapter 0x17 category 1; slot 3 */

    /* snapshot the decrypted ORIGINAL so we can prove only slot 3 changed */
    orig = (uint8 *)malloc(SAV_SIZE);
    memcpy(orig, g_scs_sav_backup, SAV_SIZE);
    fd2_save_crypt_buffer((uint32)orig, SAV_SIZE);

    /* commit slot 3 (Down x3, Enter), then Esc to exit the loop */
    n = scs_commit_keys(keys, 3);
    keys[n++] = 0x01;
    savefix_queue_scancodes(keys, n);

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
    int    keys[1];

    scs_setup(0x18, 1);

    /* single Esc -> the picker returns -1 on its first poll, the do-while
     * exits, and no fopen "wb" write path is taken */
    keys[0] = 0x01;
    savefix_queue_scancodes(keys, 1);

    fd2_save_current_state_to_slot(1);

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

/* ================================================================
 * fd2_load_state_from_selected_slot — restore current-state globals
 * from a chosen FD2.SAV slot (inverse of fd2_save_current_state_to_slot).
 *
 * Host-testable without reaching the modal confirmation:
 *   - cancel arm (selector -1): body skipped, no state touched, no dialog.
 *   - empty-slot arm (slot[+0xA00]==0xFF): inner if skipped (no restore,
 *     no dialog) and the loop re-prompts, exercising the slot-base
 *     arithmetic + the re-prompt sentinel.
 *
 * The restore arm (category==0) unconditionally runs the real load-confirm
 * dialog + the real blocking fd2_wait_for_input_dialog_with_blink. That is
 * driven here exactly like the status-screen tests drive the same real wait:
 * a scancode is injected into the BIOS keyboard buffer so the wait returns on
 * its first poll, and minip_setup_env() points data_fd2_all_game_text_ptr at
 * an immediate-END text table so the real fd2_display_dialog_scene returns at
 * the END opcode WITHOUT consuming that keystroke. fd2_load_chapter_portrait
 * runs for real against the staged DATO.DAT (it neither clears the keyboard
 * buffer nor blocks). The slot's member-count byte is set to 0 so the
 * per-member portrait-cache rebuild loop body never runs (no FDICON parse
 * needed for the assertions); the rebuild's per-member routing is left to the
 * sibling fd2_load_save_and_init_engine coverage. Expected slot bytes are the
 * exact plaintext written into the real FD2.SAV (encrypted with the linked
 * real cipher) before the load; the original file is restored at teardown.
 * ================================================================ */

/* backup of the staged FD2.SAV so teardown restores the original */
static uint8 *g_lss_sav_backup;
/* the plaintext we wrote into the slot (for verbatim roster comparison) */
static uint8 *g_lss_plain;
/* dest roster buffer the loader memmoves the slot body into */
static uint8 *g_lss_roster;

/* Down-navigate to slot `slot` then commit (Down x slot, Enter). */
static int lss_commit_keys(int *codes, uint32 slot)
{
    int n = 0;
    uint32 k;
    for (k = 0; k < slot; k++) {
        codes[n++] = 0x50;            /* Down */
    }
    codes[n++] = 0x1C;                /* Enter = commit */
    return n;
}

/* free the portrait buffer fd2_load_chapter_portrait leaks (the 3 picker /
 * portrait workspaces are reclaimed via savefix_free_selector_workspaces). */
static void lss_free_portrait_workspaces(void)
{
    savefix_free_selector_workspaces();
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
}

/* Build a known plaintext save image with slot `slot` carrying the given
 * header fields + a recognisable roster body, encrypt it into the real
 * FD2.SAV, and arm the one-shot selector to commit that slot. */
static void lss_setup(uint32 chapter_story, uint32 member_count, uint32 slot)
{
    FILE  *fp;
    long   base;
    long   i;

    /* back up the staged real FD2.SAV verbatim */
    g_lss_sav_backup = (uint8 *)malloc(SAV_SIZE);
    fp = fopen("FD2.SAV", "rb");
    fread(g_lss_sav_backup, 1, SAV_SIZE, fp);
    fclose(fp);

    /* author a fresh plaintext image */
    g_lss_plain = (uint8 *)malloc(SAV_SIZE);
    for (i = 0; i < SAV_SIZE; i++) {
        g_lss_plain[i] = (uint8)(i & 0xFF);
    }
    base = SAV_SLOT0 + (long)slot * SAV_STRIDE;
    /* recognisable 0xA00-byte roster body */
    for (i = 0; i < 0xA00; i++) {
        g_lss_plain[base + i] = (uint8)(i * 5 + 0x23);
    }
    /* scalar header at +0xA00.. */
    g_lss_plain[base + 0xA00] = (uint8)chapter_story;        /* chapter id   */
    g_lss_plain[base + 0xA01] = (uint8)member_count;         /* member count */
    *(uint32 *)(g_lss_plain + base + 0xA02) = 0x0BADF00D;    /* gold (u32)   */
    g_lss_plain[base + 0xA06] = 0x5A;                        /* terrain hud  */
    g_lss_plain[base + 0xA07] = 0x3C;                        /* game speed   */
    g_lss_plain[base + 0xA08] = 1;                           /* bgm enabled  */
    g_lss_plain[base + 0xA09] = 0;                           /* sfx enabled  */
    /* valid checksum tail (the load path does not verify it, but keep the
     * on-disk image self-consistent) */
    *(uint32 *)(g_lss_plain + 0x59C7) =
        fd2_save_compute_checksum((uint32)g_lss_plain, SAV_SIZE);

    /* encrypt a copy into FD2.SAV; keep g_lss_plain as the plaintext oracle */
    {
        uint8 *enc = (uint8 *)malloc(SAV_SIZE);
        memcpy(enc, g_lss_plain, SAV_SIZE);
        fd2_save_crypt_buffer((uint32)enc, SAV_SIZE);
        fp = fopen("FD2.SAV", "wb");
        fwrite(enc, 1, SAV_SIZE, fp);
        fclose(fp);
        free(enc);
    }

    /* dest roster buffer the loader writes the slot body into */
    g_lss_roster = (uint8 *)malloc(0xA00);
    memset(g_lss_roster, 0, 0xA00);
    data_fd2_shared_menu_party_roster_buffer_ptr = (uint32)g_lss_roster;

    /* this chapter is a story chapter -> the load (restore) arm */
    data_fd2_chapter_per_chapter_category_table[chapter_story] = 0;

    /* default-slot speaker portrait id for fd2_load_chapter_portrait */
    data_fd2_chapter_intro_menu_speaker_portrait_id_table[0] = 0x40;

    (void)slot;

    /* immediate-END dialog text + sprite sheet + blit spies */
    minip_setup_env();
    data_fd2_portrait_sprite_buffer = 0;

    /* stage the real picker's setup deps (sprite atlas + all-END grid text) */
    savefix_setup_env();
}

static void lss_teardown(uint32 chapter_story)
{
    FILE *fp;

    /* restore the original staged FD2.SAV bytes */
    fp = fopen("FD2.SAV", "wb");
    fwrite(g_lss_sav_backup, 1, SAV_SIZE, fp);
    fclose(fp);
    free(g_lss_sav_backup);
    free(g_lss_plain);
    free(g_lss_roster);
    g_lss_sav_backup = 0;
    g_lss_plain = 0;
    g_lss_roster = 0;

    lss_free_portrait_workspaces();

    data_fd2_shared_menu_party_roster_buffer_ptr = 0;
    data_fd2_chapter_per_chapter_category_table[chapter_story] = 0;
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_chapter_current_chapter_id = 1;
    data_fd2_resource_portrait_cache_count = 0;
}

/* ---- Test: cancel arm -> no state touched ---- */
static void test_lss_cancel_restores_nothing(void)
{
    uint32 chap_before;
    uint32 gold_before;
    uint32 count_before;
    int    keys[1];

    lss_setup(5, 0, 1);

    /* single Esc -> the picker cancels on its first poll, loop exits */
    keys[0] = 0x01;
    savefix_queue_scancodes(keys, 1);

    chap_before  = data_fd2_chapter_current_chapter_id;
    gold_before  = data_fd2_shared_party_total_gold;
    count_before = data_fd2_shared_menu_party_member_count;

    fd2_load_state_from_selected_slot();

    /* nothing restored on cancel */
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, (long)chap_before);
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, (long)gold_before);
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count,
              (long)count_before);
    /* dest roster untouched (still zero) */
    ASSERT_EQ((long)g_lss_roster[0], 0);

    lss_teardown(5);
}

/* ---- Test: empty slot (chapter byte 0xFF) -> re-prompt, no restore ---- */
static void test_lss_empty_slot_reprompts(void)
{
    long   base;
    uint32 count_before;
    int    keys[4];
    int    n;

    lss_setup(5, 0, 1);

    /* picker call 1: Down,Enter -> commit slot 1 (empty: no restore/dialog,
     * re-prompt); call 2: Esc -> cancel and exit. */
    n = lss_commit_keys(keys, 1);
    keys[n++] = 0x01;
    savefix_queue_scancodes(keys, n);

    /* overwrite slot 1's chapter byte to 0xFF (empty) in the on-disk image */
    base = SAV_SLOT0 + 1 * SAV_STRIDE;
    g_lss_plain[base + 0xA00] = 0xFF;
    *(uint32 *)(g_lss_plain + 0x59C7) =
        fd2_save_compute_checksum((uint32)g_lss_plain, SAV_SIZE);
    {
        uint8 *enc = (uint8 *)malloc(SAV_SIZE);
        FILE  *fp;
        memcpy(enc, g_lss_plain, SAV_SIZE);
        fd2_save_crypt_buffer((uint32)enc, SAV_SIZE);
        fp = fopen("FD2.SAV", "wb");
        fwrite(enc, 1, SAV_SIZE, fp);
        fclose(fp);
        free(enc);
    }

    count_before = data_fd2_shared_menu_party_member_count;

    fd2_load_state_from_selected_slot();

    /* empty slot -> nothing restored, dest roster still zero */
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count,
              (long)count_before);
    ASSERT_EQ((long)g_lss_roster[0], 0);

    lss_teardown(5);
}

/* ---- Test: restore arm -> 8 scalar fields + roster + chapter meta ---- */
static void test_lss_restores_slot_state(void)
{
    long base;
    int  keys[4];
    int  n;

    lss_setup(5, 0, 2);          /* story chapter 5, member_count 0, slot 2 */

    /* picker call 1: Down,Down,Enter -> commit slot 2; the restore arm then
     * runs the real confirm dialog whose blink wait reads ONE more scancode
     * (Esc). A successful restore forces slot_result = -1, so the loop exits
     * after this single picker call (no re-prompt). */
    n = lss_commit_keys(keys, 2);
    keys[n++] = 0x01;            /* consumed by the confirm-dialog blink wait */
    savefix_queue_scancodes(keys, n);

    fd2_load_state_from_selected_slot();

    base = SAV_SLOT0 + 2 * SAV_STRIDE;

    /* 8 scalar fields restored from the slot header (exact offsets/widths) */
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 5);
    ASSERT_EQ((long)data_fd2_shared_menu_party_member_count, 0);
    ASSERT_EQ((long)data_fd2_shared_party_total_gold, (long)0x0BADF00D);
    ASSERT_EQ((long)data_fd2_ui_terrain_hud_user_enabled, 0x5A);
    ASSERT_EQ((long)data_fd2_ui_game_speed_flag, 0x3C);
    ASSERT_EQ((long)data_fd2_audio_bgm_enabled_flag, 1);
    ASSERT_EQ((long)data_fd2_audio_sfx_enabled_flag, 0);

    /* 0xA00-byte roster restored verbatim out of the slot body */
    ASSERT_MEM_EQ(g_lss_roster, g_lss_plain + base, 0xA00);

    /* chapter-intro metadata pointer refreshed for the now-active chapter */
    ASSERT_EQ((long)data_fd2_chapter_intro_active_metadata_entry_ptr,
              (long)(uint32)fd2_get_chapter_intro_metadata_entry(5));

    /* member_count 0 -> portrait rebuild loop body skipped (count stays 0) */
    ASSERT_EQ((long)data_fd2_resource_portrait_cache_count, 0);

    lss_teardown(5);
}

/* ---- Test: restore arm frees a non-null prior portrait sprite cache ---- */
static void test_lss_restore_frees_prior_portrait_cache(void)
{
    int keys[4];
    int n;

    lss_setup(5, 0, 2);
    /* a non-null prior cache pointer must be freed by the restore arm */
    portrait_sprite_cache = (uint32)malloc(64);

    /* commit slot 2 (Down,Down,Enter) + Esc for the confirm-dialog blink wait */
    n = lss_commit_keys(keys, 2);
    keys[n++] = 0x01;
    savefix_queue_scancodes(keys, n);

    fd2_load_state_from_selected_slot();

    /* restore happened (chapter id came through) */
    ASSERT_EQ((long)data_fd2_chapter_current_chapter_id, 5);
    /* the free path ran (no crash / no double-free under the harness). The
     * pointer value itself is not nulled by the function, so teardown must not
     * free it again. */
    portrait_sprite_cache = 0;

    lss_teardown(5);
}

/* ================================================================
 * fd2_save_slot_selector_ui — the 4-slot picker itself, driven directly.
 *
 * Risk-bearing control flow exercised here (manual_mode==0, the direct
 * int386(0x16) BIOS path): commit (Enter/Space -> 1), cancel (Esc -> -1),
 * Up/Down cursor navigation with the 0..3 clamp, and the scancode remap
 * (0xE0/0x52 -> Enter, 0x53 -> Esc). The picker leaves the chosen slot in
 * data_fd2_ui_menu_cursor_idx; it does NOT reset the cursor (its callers do),
 * so each test seeds the cursor directly. A single picker call consumes every
 * queued navigation key until a terminating commit/cancel is read.
 *
 * The setup phase (3x malloc + VGA snapshot + grid render) runs for real;
 * savefix_setup_env() stages the sprite atlas + all-END grid text. A zeroed
 * in-memory SAV image is passed as the decrypted buffer (the grid render reads
 * per-slot chapter bytes but renders nothing under the all-END program).
 * ================================================================ */

/* grid render reads per-slot chapter bytes up to slot 3 (0x312B + 3*0xA28 +
 * 0xA00 = 0x59A3); 0x5A00 covers it. */
#define SEL_SAV_SIZE 0x5A00u
static uint8 g_sel_sav[SEL_SAV_SIZE];

static void sel_setup(uint32 seed_cursor)
{
    memset(g_sel_sav, 0, sizeof(g_sel_sav));
    savefix_setup_env();
    data_fd2_ui_menu_cursor_idx = seed_cursor;
}

static void sel_teardown(void)
{
    savefix_free_selector_workspaces();
    data_fd2_ui_menu_cursor_idx = 0;
}

/* ---- commit: Enter returns 1, cursor preserved as the result ---- */
static void test_sel_enter_commits(void)
{
    int keys[1];
    int r;

    sel_setup(2);
    keys[0] = 0x1C;                       /* Enter */
    savefix_queue_scancodes(keys, 1);

    r = fd2_save_slot_selector_ui((uint32)g_sel_sav, 0);

    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    sel_teardown();
}

/* ---- commit: Space also returns 1 ---- */
static void test_sel_space_commits(void)
{
    int keys[1];
    int r;

    sel_setup(1);
    keys[0] = 0x39;                       /* Space */
    savefix_queue_scancodes(keys, 1);

    r = fd2_save_slot_selector_ui((uint32)g_sel_sav, 0);

    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);
    sel_teardown();
}

/* ---- cancel: Esc returns -1 ---- */
static void test_sel_esc_cancels(void)
{
    int keys[1];
    int r;

    sel_setup(0);
    keys[0] = 0x01;                       /* Esc */
    savefix_queue_scancodes(keys, 1);

    r = fd2_save_slot_selector_ui((uint32)g_sel_sav, 0);

    ASSERT_EQ((long)r, -1);
    sel_teardown();
}

/* ---- Down navigates 0->3 and clamps at 3 ---- */
static void test_sel_down_clamps_at_3(void)
{
    int keys[6];
    int r;

    sel_setup(0);
    /* 4 Downs from slot 0: 0->1->2->3->(3 clamp), then Enter */
    keys[0] = 0x50; keys[1] = 0x50; keys[2] = 0x50; keys[3] = 0x50;
    keys[4] = 0x1C;
    savefix_queue_scancodes(keys, 5);

    r = fd2_save_slot_selector_ui((uint32)g_sel_sav, 0);

    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 3);
    sel_teardown();
}

/* ---- Up navigates 3->0 and clamps at 0 ---- */
static void test_sel_up_clamps_at_0(void)
{
    int keys[6];
    int r;

    sel_setup(3);
    /* 4 Ups from slot 3: 3->2->1->0->(0 clamp), then Enter */
    keys[0] = 0x48; keys[1] = 0x48; keys[2] = 0x48; keys[3] = 0x48;
    keys[4] = 0x1C;
    savefix_queue_scancodes(keys, 5);

    r = fd2_save_slot_selector_ui((uint32)g_sel_sav, 0);

    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
    sel_teardown();
}

/* ---- remap: extended 0xE0 prefix -> Enter (commit) ---- */
static void test_sel_remap_e0_to_enter(void)
{
    int keys[1];
    int r;

    sel_setup(1);
    keys[0] = 0xE0;
    savefix_queue_scancodes(keys, 1);

    r = fd2_save_slot_selector_ui((uint32)g_sel_sav, 0);

    ASSERT_EQ((long)r, 1);
    sel_teardown();
}

/* ---- remap: numpad 0x52 -> Enter (commit) ---- */
static void test_sel_remap_52_to_enter(void)
{
    int keys[1];
    int r;

    sel_setup(1);
    keys[0] = 0x52;
    savefix_queue_scancodes(keys, 1);

    r = fd2_save_slot_selector_ui((uint32)g_sel_sav, 0);

    ASSERT_EQ((long)r, 1);
    sel_teardown();
}

/* ---- remap: numpad 0x53 -> Esc (cancel) ---- */
static void test_sel_remap_53_to_esc(void)
{
    int keys[1];
    int r;

    sel_setup(1);
    keys[0] = 0x53;
    savefix_queue_scancodes(keys, 1);

    r = fd2_save_slot_selector_ui((uint32)g_sel_sav, 0);

    ASSERT_EQ((long)r, -1);
    sel_teardown();
}

/* ----------------------------------------------------------------
 * fd2_obfuscate_battle_tile_map @ 0x4dbfc
 *
 * Per-tile field clamp/reset. Buffer = 4-byte header + (hdr[0]*hdr[2])
 * 4-byte tile records; per record: rec[1]&=0x03, rec[2]&=0x1F,
 * rec[3]=0xFF, rec[0] untouched. Header bytes read-only.
 * Pure in-memory; no globals / files involved.
 * ---------------------------------------------------------------- */

/* ---- Test: each tile record's three bytes masked, byte0 preserved ---- */
static void test_obf_masks_each_record(void)
{
    uint8 buf[4 + 2 * 4];
    uint8 *rec;
    int i;

    /* header: width=2, height=1 -> tile_count = 2 */
    memset(buf, 0xff, sizeof(buf));
    buf[0] = 2;
    buf[1] = 0xAA; /* header padding byte, must stay untouched */
    buf[2] = 1;
    buf[3] = 0xBB; /* header padding byte, must stay untouched */

    fd2_obfuscate_battle_tile_map((uint32)buf);

    /* header preserved */
    ASSERT_EQ((long)buf[0], 2);
    ASSERT_EQ((long)buf[1], 0xAA);
    ASSERT_EQ((long)buf[2], 1);
    ASSERT_EQ((long)buf[3], 0xBB);

    /* both records (start at buf+4): 0xFF -> rec[1]=0x03, rec[2]=0x1F,
       rec[3]=0xFF, rec[0] untouched (still 0xFF). */
    for (i = 0; i < 2; i++) {
        rec = buf + 4 + i * 4;
        ASSERT_EQ((long)rec[0], 0xFF);
        ASSERT_EQ((long)rec[1], 0x03);
        ASSERT_EQ((long)rec[2], 0x1F);
        ASSERT_EQ((long)rec[3], 0xFF);
    }
}

/* ---- Test: rec[2] clears the 0x40 occupied + 0x80 bits, keeps low5 ---- */
static void test_obf_clears_occupied_bit(void)
{
    uint8 buf[4 + 1 * 4];
    uint8 *rec;

    /* width=1, height=1 -> tile_count = 1 */
    memset(buf, 0, sizeof(buf));
    buf[0] = 1;
    buf[2] = 1;
    rec = buf + 4;
    rec[0] = 0x12;        /* base terrain index, must survive */
    rec[1] = 0xFE;        /* -> &0x03 = 0x02 */
    rec[2] = 0x40 | 0x80 | 0x0A; /* occupied+attr+low; -> 0x0A */
    rec[3] = 0x00;        /* -> forced 0xFF */

    fd2_obfuscate_battle_tile_map((uint32)buf);

    ASSERT_EQ((long)rec[0], 0x12);
    ASSERT_EQ((long)rec[1], 0x02);
    ASSERT_EQ((long)rec[2], 0x0A);          /* 0x40 and 0x80 cleared */
    ASSERT_EQ((long)(rec[2] & 0x40), 0);    /* caller's occupancy test now 0 */
    ASSERT_EQ((long)rec[3], 0xFF);
}

/* ---- Test: tile_count = width*height bounds the loop; header & the
 *           record just past the count are not touched ---- */
static void test_obf_count_boundary_and_header(void)
{
    uint8 buf[4 + 5 * 4]; /* room for 4 processed records + 1 sentinel */
    uint8 *sentinel;
    int i;

    /* width=2, height=2 -> tile_count = 4 records processed */
    memset(buf, 0xff, sizeof(buf));
    buf[0] = 2;
    buf[2] = 2;

    /* sentinel is the 5th record (index 4) — must remain all 0xFF */
    sentinel = buf + 4 + 4 * 4;

    fd2_obfuscate_battle_tile_map((uint32)buf);

    /* all 4 processed records got masked */
    for (i = 0; i < 4; i++) {
        ASSERT_EQ((long)buf[4 + i * 4 + 1], 0x03);
        ASSERT_EQ((long)buf[4 + i * 4 + 2], 0x1F);
        ASSERT_EQ((long)buf[4 + i * 4 + 3], 0xFF);
    }

    /* record beyond tile_count is untouched */
    ASSERT_EQ((long)sentinel[0], 0xFF);
    ASSERT_EQ((long)sentinel[1], 0xFF);
    ASSERT_EQ((long)sentinel[2], 0xFF);
    ASSERT_EQ((long)sentinel[3], 0xFF);
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
    RUN_TEST(test_lss_cancel_restores_nothing);
    RUN_TEST(test_lss_empty_slot_reprompts);
    RUN_TEST(test_lss_restores_slot_state);
    RUN_TEST(test_lss_restore_frees_prior_portrait_cache);
    RUN_TEST(test_sel_enter_commits);
    RUN_TEST(test_sel_space_commits);
    RUN_TEST(test_sel_esc_cancels);
    RUN_TEST(test_sel_down_clamps_at_3);
    RUN_TEST(test_sel_up_clamps_at_0);
    RUN_TEST(test_sel_remap_e0_to_enter);
    RUN_TEST(test_sel_remap_52_to_enter);
    RUN_TEST(test_sel_remap_53_to_esc);
    RUN_TEST(test_obf_masks_each_record);
    RUN_TEST(test_obf_clears_occupied_bit);
    RUN_TEST(test_obf_count_boundary_and_header);
    SUITE_END();
}
