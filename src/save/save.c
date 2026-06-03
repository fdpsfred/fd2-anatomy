/*
 * save.c — FD2 chapter-end persistence of battle-runtime char state.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>

/* ----------------------------------------------------------------
 * fd2_save_runtime_char_to_template @ 0x11506 (24 callers)
 *
 * Persist battle-runtime char state back into the menu/template
 * roster array at chapter end. For each runtime party member,
 * find the template entry with a matching char_id and copy the
 * whole 0x50-byte struct back, clearing transient status, dropping
 * non-dead flags, restoring HP (if alive) and MP, then re-deriving
 * aggregate stats.
 *
 * SPECIAL CASE: never overwrite the template entry for char 0
 * (索爾) if he died in battle — chapter end revives him or branches
 * to game over, so his battle-dead state must not propagate.
 *
 * Called by every chapter_NN_end handler as part of the standard
 * 3-call tail (save_template + init_growth_char + chapter_id++).
 * ---------------------------------------------------------------- */
void fd2_save_runtime_char_to_template(void)
{
    uint32 rt_idx;
    uint32 tmpl_idx;
    uint8 *rt_char;
    uint8 *tmpl;

    for (rt_idx = 0; (int32)rt_idx < (int32)data_fd2_battle_party_member_count;
         rt_idx++) {
        rt_char = (uint8 *)(data_fd2_battle_runtime_char_array_ptr + rt_idx);

        for (tmpl_idx = 0;
             (int32)tmpl_idx < (int32)data_fd2_shared_menu_party_member_count;
             tmpl_idx++) {
            tmpl = (uint8 *)(data_fd2_shared_menu_party_roster_buffer_ptr +
                             tmpl_idx * 0x50);

            if (rt_char[8] == tmpl[8]) {
                if (tmpl[8] == 0) {
                    if (fd2_check_char_is_dead(rt_idx) != 0) {
                        continue;
                    }
                }
                memmove((void *)tmpl, (void *)rt_char, 0x50);
                memset((void *)(tmpl + 0x22), 0, 6);
                tmpl[5] = (uint8)(tmpl[5] & 1);
                if (tmpl[5] != 1) {
                    *(uint16 *)(tmpl + 0x40) = *(uint16 *)(tmpl + 0x42);
                }
                *(uint16 *)(tmpl + 0x44) = *(uint16 *)(tmpl + 0x46);
                fd2_recompute_runtime_char_total_stats(tmpl_idx);
            }
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_save_compute_checksum @ 0x4dbb9 (4 callers)
 *
 * Sums all bytes in buf[0..len-5] as a u32 (excludes last 4 bytes
 * which hold the checksum itself). __cdecl: buf = byte buffer,
 * len = total length including the 4 trailing checksum bytes.
 * Returns the natural u32-wrap byte sum (no overflow handling).
 *
 * Body mirrors the LODSB/LOOP form: remaining = len - 4, then a
 * do-while that adds *pBuf and decrements remaining. do-while means
 * len <= 4 underflows; callers must pass len > 4 (FD2.SAV uses
 * len = 0x59CB, checksum field at +0x59C7).
 * ---------------------------------------------------------------- */
uint32 fd2_save_compute_checksum(uint32 buf, uint32 size)
{
    uint8 *pBuf;
    uint32 remaining;
    uint32 checksum;

    pBuf = (uint8 *)buf;
    remaining = size - 4;
    checksum = 0;
    do {
        checksum = checksum + *pBuf;
        remaining = remaining - 1;
        pBuf = pBuf + 1;
    } while (remaining != 0);
    return checksum;
}

/* ----------------------------------------------------------------
 * fd2_save_crypt_buffer @ 0x4dbd8 (6 callers)
 *
 * In-place XOR cipher for FD2.SAV: applied identically on write
 * (encrypt) and read (decrypt) — an involution. __cdecl:
 * buf = byte buffer, size = length (must be > 0). void return.
 *
 * scramble_state starts at 0xA5; each iteration advances it via
 * ROL16(state + 0x9014, 3) (16-bit add then rotate-left by 3 — the
 * Watcom shift+OR idiom on a 16-bit word) and XORs the low byte of
 * the new state into the current buffer byte. Because XOR is
 * self-inverse and the keystream is deterministic from the constant
 * seed, the same routine encrypts (write path) and decrypts (read
 * path).
 *
 * NOTE: do-while means size == 0 underflows; callers must pass
 * size >= 1.
 * ---------------------------------------------------------------- */
void fd2_save_crypt_buffer(uint32 buf, uint32 size)
{
    uint16 scramble_state;
    uint8 *pBuf;

    scramble_state = 0xa5;
    pBuf = (uint8 *)buf;
    do {
        scramble_state = (uint16)(((scramble_state + 0x9014) << 3) |
                                  ((uint16)(scramble_state + 0x9014) >> 0xd));
        *pBuf = (uint8)(*pBuf ^ (uint8)scramble_state);
        size = size - 1;
        pBuf = pBuf + 1;
    } while (size != 0);
}
