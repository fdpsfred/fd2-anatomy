/*
 * save.c — FD2 chapter-end persistence of battle-runtime char state.
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

/* ----------------------------------------------------------------
 * fd2_save_current_state_to_slot @ 0x30012 (2 callers)
 *
 * Write the current game-state globals into a user-selected slot of
 * FD2.SAV, with checksum and crypto-buffer transform. __cdecl, one
 * arg: prompt_flag (1 = explicit save menu, 0 = battle-chapter auto
 * save) forwarded as the slot-selector's "save mode". void return.
 * Called by fd2_run_chapter_intro_menu_typeB(1) and
 * fd2_chapter_transition_menu(0).
 *
 * Flow:
 *   1. malloc(0x59CB); read FD2.SAV ("rb"): on fopen failure fill the
 *      buffer with 0xFF, else fread + decrypt + close.
 *   2. cursor_idx = 0, then loop: ask the slot selector (returns 1 on
 *      commit, -1 on cancel; the chosen 0..3 slot is left in
 *      cursor_idx). On cancel the body is skipped.
 *   3. slot_base = pBuf + cursor_idx * 0xA28 + 0x312B; the scalar
 *      header lives at slot_base + 0xA00.
 *   4. memmove the 0xA00-byte template/menu roster into the slot, then
 *      store the 8 scalar fields (chapter id / member count / gold u32
 *      / terrain-hud / game speed / bgm / sfx flags).
 *   5. fopen("wb"); recompute checksum into pBuf[0x59C7..0x59CA];
 *      re-encrypt; fwrite the whole buffer; close; decrypt again so the
 *      in-memory buffer stays plaintext for a possible next iteration.
 *   6. When per_chapter_category[current_chapter_id] == 0 (story
 *      chapter) show the "saved" confirmation dialog + blink wait.
 *   7. Always close the intro dialog; repeat the loop until the user
 *      cancels (-1); then free the buffer.
 *
 * NOTE: cursor_idx is reset to 0 once before the loop; the selector
 * leaves the user's chosen slot there, so each iteration writes the
 * slot the selector last navigated to.
 * ---------------------------------------------------------------- */
void fd2_save_current_state_to_slot(uint32 prompt_flag)
{
    uint8 *pBuf;
    void  *fp;
    uint32 slot_result;
    uint8 *slot_base;
    uint32 checksum;

    pBuf = (uint8 *)malloc(0x59cb);
    fp = fopen("FD2.SAV", "rb");
    if (fp == NULL) {
        memset(pBuf, 0xff, 0x59cb);
    } else {
        fread(pBuf, 1, 0x59cb, fp);
        fd2_save_crypt_buffer((uint32)pBuf, 0x59cb);
        fclose(fp);
    }

    data_fd2_ui_menu_cursor_idx = 0;
    do {
        slot_result = (uint32)fd2_save_slot_selector_ui((uint32)pBuf,
                                                        prompt_flag & 0xff);
        if (slot_result != 0xffffffff) {
            slot_base = pBuf + data_fd2_ui_menu_cursor_idx * 0xa28 + 0x312b;
            memmove((void *)slot_base,
                    (void *)data_fd2_shared_menu_party_roster_buffer_ptr,
                    0xa00);
            slot_base[0xa00] = (uint8)data_fd2_chapter_current_chapter_id;
            slot_base[0xa01] = (uint8)data_fd2_shared_menu_party_member_count;
            *(uint32 *)(slot_base + 0xa02) = data_fd2_shared_party_total_gold;
            slot_base[0xa06] = data_fd2_ui_terrain_hud_user_enabled;
            slot_base[0xa07] = data_fd2_ui_game_speed_flag;
            slot_base[0xa08] = data_fd2_audio_bgm_enabled_flag;
            slot_base[0xa09] = data_fd2_audio_sfx_enabled_flag;

            fp = fopen("FD2.SAV", "wb");
            checksum = fd2_save_compute_checksum((uint32)pBuf, 0x59cb);
            *(uint32 *)(pBuf + 0x59c7) = checksum;
            fd2_save_crypt_buffer((uint32)pBuf, 0x59cb);
            fwrite(pBuf, 1, 0x59cb, fp);
            fclose(fp);
            fd2_save_crypt_buffer((uint32)pBuf, 0x59cb);

            if (data_fd2_chapter_per_chapter_category_table
                    [data_fd2_chapter_current_chapter_id] == 0) {
                fd2_close_intro_dialog_with_slide_out();
                fd2_load_chapter_portrait(
                    (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[0]);
                fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x294,
                    0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
            }
        }
        fd2_close_intro_dialog_with_slide_out();
    } while (slot_result != 0xffffffff);

    free(pBuf);
}
