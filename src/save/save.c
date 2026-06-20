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
#include <dos.h>

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
 * fd2_obfuscate_battle_tile_map @ 0x4dbfc (15 callers)
 *
 * In-place per-tile field clamp/reset over a battle tile-map buffer.
 * __cdecl, one arg: tile_map = byte buffer base. void return.
 *
 * The buffer has a 4-byte header followed by (header[0] * header[2])
 * tile records of 4 bytes each:
 *   header[0] = map width, header[2] = map height -> tile_count =
 *               width * height (8-bit MUL, result fits in 16 bits).
 *   records start at tile_map + 4.
 * For each tile record the routine clamps three of its four bytes:
 *   rec[1] &= 0x03    (keep low 2 bits)
 *   rec[2] &= 0x1F    (keep low 5 bits — clears the 0x40 "occupied"
 *                      and 0x80 attribute bits read by callers)
 *   rec[3]  = 0xFF    (force to 0xFF)
 *   rec[0] is left untouched.
 * Header bytes 0/2 are read for the count but not modified.
 *
 * The Ghidra decompiler renders this as a 16-bit "rolling state"
 * LFSR with CONCAT11/&0x1FFF/&0x3FF/>>8 cascades, but that form is
 * algebraically identical to the three masks above (the disassembly
 * loads AL=0xFF once outside the loop and only ever stores it; each
 * mask byte is freshly loaded from memory, AND-ed, and stored back —
 * there is no carried state). The function's immediate constants are
 * exactly 0x03 / 0x04 / 0xFF / 0x1F, confirming the simple-mask form.
 *
 * NOTE: do-while means tile_count == 0 (degenerate 0-area map)
 * underflows the counter; callers always pass a loaded map with
 * valid non-zero dimensions.
 * ---------------------------------------------------------------- */
void fd2_obfuscate_battle_tile_map(uint32 tile_map)
{
    uint8 *pTile;
    uint32 tile_count;

    pTile = (uint8 *)tile_map;
    tile_count = (uint16)((uint16)pTile[0] * (uint16)pTile[2]);
    pTile = pTile + 4;
    do {
        pTile[3] = 0xff;
        pTile[2] = (uint8)(pTile[2] & 0x1f);
        pTile[1] = (uint8)(pTile[1] & 0x03);
        tile_count = tile_count - 1;
        pTile = pTile + 4;
    } while (tile_count != 0);
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

/* ----------------------------------------------------------------
 * fd2_load_state_from_selected_slot @ 0x301f4 (1 caller)
 *
 * Restore game state from a user-selected FD2.SAV slot — inverse of
 * fd2_save_current_state_to_slot @ 0x30012. __cdecl, void return.
 * Called by fd2_run_chapter_intro_menu_typeB (option 2 = ロード).
 *
 * Flow:
 *   1. malloc(0x59CB); read FD2.SAV ("rb"): on fopen failure fill the
 *      buffer with 0xFF, else fread + decrypt + close.
 *   2. cursor_idx = 0, then loop: ask the slot selector (mode 1; the
 *      chosen 0..3 slot is left in cursor_idx; returns -1 on cancel).
 *      Always close the intro dialog after the selector. On cancel
 *      (-1) the body is skipped and the loop exits.
 *   3. slot_base = pBuf + cursor_idx * 0xA28 + 0x312B; the chapter
 *      header byte lives at slot_base + 0xA00.
 *   4. If the slot is empty (slot_base[0xA00] == 0xFF) silently
 *      re-prompt. Else if that chapter's category is non-zero (not a
 *      story chapter) show the error dialog (text 0x1DF) and re-prompt.
 *   5. On a loadable slot (category == 0): memmove the 0xA00-byte
 *      template/menu roster out of the slot, restore the 8 scalar
 *      fields (chapter id / member count / gold u32 / terrain-hud /
 *      game speed / bgm / sfx flags), free the old portrait sprite
 *      cache, reopen FDICON.B24 and rebuild the per-member portrait
 *      cache (roster[i].bPortrait_id at +7), refresh the active
 *      chapter intro metadata entry, set slot_result = -1 to end the
 *      loop, then show the "loaded" confirmation dialog (text 0x1DE)
 *      + blink wait.
 *   6. free the buffer.
 *
 * NOTE: cursor_idx is reset to 0 once before the loop; the selector
 * leaves the user's chosen slot there. slot_result doubles as the
 * loop sentinel — it is forced to -1 only after a successful restore,
 * so empty/non-story selections fall through and re-prompt.
 * ---------------------------------------------------------------- */
void fd2_load_state_from_selected_slot(void)
{
    uint8 *pBuf;
    void  *fp;
    uint32 slot_result;
    uint8 *slot_base;
    int32  i;
    uint32 text_id;

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
        slot_result = (uint32)fd2_save_slot_selector_ui((uint32)pBuf, 1);
        fd2_close_intro_dialog_with_slide_out();
        if (slot_result != 0xffffffff) {
            slot_base = pBuf + data_fd2_ui_menu_cursor_idx * 0xa28 + 0x312b;
            if (slot_base[0xa00] != 0xff) {
                if (data_fd2_chapter_per_chapter_category_table
                        [slot_base[0xa00]] == 0) {
                    memmove((void *)data_fd2_shared_menu_party_roster_buffer_ptr,
                            (void *)slot_base, 0xa00);
                    data_fd2_chapter_current_chapter_id = slot_base[0xa00];
                    data_fd2_shared_menu_party_member_count = slot_base[0xa01];
                    data_fd2_shared_party_total_gold =
                        *(uint32 *)(slot_base + 0xa02);
                    data_fd2_ui_terrain_hud_user_enabled = slot_base[0xa06];
                    data_fd2_ui_game_speed_flag = slot_base[0xa07];
                    data_fd2_audio_bgm_enabled_flag = slot_base[0xa08];
                    data_fd2_audio_sfx_enabled_flag = slot_base[0xa09];

                    if (data_fd2_portrait_sprite_cache != 0) {
                        free((void *)data_fd2_portrait_sprite_cache);
                    }
                    fp = fopen("FDICON.B24", "rb");
                    data_fd2_resource_portrait_cache_count = 0;
                    for (i = 0;
                         i < (int32)data_fd2_shared_menu_party_member_count;
                         i++) {
                        fd2_load_portrait_to_cache(
                            (uint32)*(uint8 *)(
                                data_fd2_shared_menu_party_roster_buffer_ptr +
                                i * 0x50 + 7),
                            (uint32)fp);
                    }
                    fclose(fp);
                    data_fd2_chapter_intro_active_metadata_entry_ptr =
                        (uint32)fd2_get_chapter_intro_metadata_entry(
                            data_fd2_chapter_current_chapter_id);
                    slot_result = 0xffffffff;
                    fd2_load_chapter_portrait(
                        (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[0]);
                    text_id = 0x1de;
                } else {
                    fd2_load_chapter_portrait(
                        (uint32)data_fd2_chapter_intro_menu_speaker_portrait_id_table[0]);
                    text_id = 0x1df;
                }
                fd2_display_dialog_scene(data_fd2_all_game_text_ptr, text_id,
                    0xa94cc, 0x140, 0xcd, 0x4c, 0x4a, 0x13, 1);
                fd2_paint_portrait_to_dialog_area(0);
                fd2_wait_for_input_dialog_with_blink(0);
                fd2_close_intro_dialog_with_slide_out();
            }
        }
    } while (slot_result != 0xffffffff);

    free(pBuf);
}

/* ----------------------------------------------------------------
 * fd2_save_slot_selector_ui @ 0x30550 (3 callers)
 *
 * 4-slot save-file picker UI. __cdecl, two args:
 *   sav_decrypted_buf = plaintext FD2.SAV image (forwarded to the
 *                       grid renderer for per-slot summaries),
 *   manual_mode       = 0 -> read keys via direct BIOS int 16h;
 *                       nonzero -> poll-and-blink input helper.
 * Returns 1 on commit (chosen slot left in data_fd2_ui_menu_cursor_idx,
 * 0..3) and -1 on cancel.
 *
 * Allocates 3 x 64000-byte workspace buffers, snapshots the live VGA
 * framebuffer (0xA0000) into b, clones it to c, blits the panel-header
 * sprite into c, paints the initial slot grid into c, then reveals the
 * panel with a 6-frame slide-down. After setup it runs an Up/Down
 * navigation loop until the user commits or cancels.
 *
 * The workspace buffers a/b/c are NOT freed here — the caller frees
 * them via fd2_close_intro_dialog_with_slide_out (same 3-buffer state).
 *
 * Callers: fd2_main_menu_continue_dispatcher @ 0x25EBB,
 *          fd2_save_current_state_to_slot    @ 0x30012,
 *          fd2_load_state_from_selected_slot @ 0x301F4.
 *
 * NOTE: the binary tail-JMPs into a shared epilogue at 0x2D3F8 that
 * does MOV EAX,ESI / POP EBP,EDI,ESI,EBX / RET; ESI holds the result,
 * so this is functionally identical to "return result;".
 * ---------------------------------------------------------------- */
int fd2_save_slot_selector_ui(uint32 sav_decrypted_buf, uint32 manual_mode)
{
    uint32 scancode;
    int32  intro_iter;
    int    result;

    result = 0;
    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);
    memmove((void *)data_fd2_ui_slide_bg_snapshot_buf_ptr,
            (void *)0xa0000, 64000);
    memmove((void *)data_fd2_ui_slide_composed_target_buf_ptr,
            (void *)data_fd2_ui_slide_bg_snapshot_buf_ptr, 64000);
    fd2_dialog_sprite_blit_normal(
        data_fd2_ui_slide_composed_target_buf_ptr + 0x8c05,
        data_fd2_ui_menu_screen_sprite_atlas_buf_ptr +
            *(uint32 *)(data_fd2_ui_menu_screen_sprite_atlas_buf_ptr + 0x46),
        0x140);
    fd2_render_save_slot_grid(data_fd2_ui_menu_cursor_idx,
                              data_fd2_ui_slide_composed_target_buf_ptr,
                              (uint8 *)sav_decrypted_buf);
    fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr, 5, 1);
    for (intro_iter = 5; intro_iter >= 0; intro_iter--) {
        fd2_slide_panel_down_step((uint32)(intro_iter * 0xd + 0x70),
                                  data_fd2_ui_slide_anim_accumulator_buf_ptr,
                                  data_fd2_ui_slide_composed_target_buf_ptr);
    }

    do {
        if (manual_mode == 0) {
#ifdef FD2_REPLAY
            fd2_replay_pump();   /* non-polling read: ensure a scripted key is ready */
#endif
            data_fd2_input_key_input_mode = 0x10;
            int386(0x16, (union REGS *)&data_fd2_input_last_key_pressed,
                         (union REGS *)&data_fd2_input_last_key_pressed);
            if (data_fd2_input_key_input_mode == 0xe0
                || data_fd2_input_key_input_mode == 0x52) {
                data_fd2_input_key_input_mode = 0x1c;
            }
            if (data_fd2_input_key_input_mode == 0x53) {
                data_fd2_input_key_input_mode = 1;
            }
            scancode = (uint32)data_fd2_input_key_input_mode;
        } else {
            scancode = (uint32)(uint8)fd2_wait_for_input_dialog_with_blink(0);
        }

        if (scancode == 0x50 && data_fd2_ui_menu_cursor_idx != 3) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     7, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx + 1;
            fd2_render_save_slot_grid(data_fd2_ui_menu_cursor_idx, 0xa0000,
                                      (uint8 *)sav_decrypted_buf);
        } else if (scancode == 0x48 && data_fd2_ui_menu_cursor_idx != 0) {
            fd2_play_sfx_with_handle(data_fd2_audio_fdother_sfx_bank_buf_ptr,
                                     7, 1);
            data_fd2_ui_menu_cursor_idx = data_fd2_ui_menu_cursor_idx - 1;
            fd2_render_save_slot_grid(data_fd2_ui_menu_cursor_idx, 0xa0000,
                                      (uint8 *)sav_decrypted_buf);
        } else if (scancode == 0x1c || scancode == 0x39) {
            result = 1;
        } else if (scancode == 1) {
            result = -1;
        }

        if (result != 0) {
            return result;
        }
    } while (1);
}
