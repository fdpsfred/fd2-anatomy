/*
 * misc.c — Miscellaneous utility functions
 */

#include "types.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>

/* ----------------------------------------------------------------
 * fd2_debug_print_ans_and_length @ 0x16F0B  (0 callers, dead code)
 *
 * Dev-time debug print: formats int as string, prints value and
 * string length, then waits for keypress. Left over from dev.
 * ---------------------------------------------------------------- */
void fd2_debug_print_ans_and_length(int value)
{
    char buf[20];
    sprintf(buf, "%d", value);
    printf(" Ans = %s,   Length = %d\n", buf, strlen(buf));
    getch();
}

/* ----------------------------------------------------------------
 * fd2_set_word_global_52758 @ 0x3615E
 *
 * Get-and-set helper for the AIL internal allocator slot
 * data_ail_alloc_fnptr (0x52758): stores new_val as the new allocator
 * function pointer and returns the prior pointer (so a caller can save
 * and restore it). The AIL internal load/alloc routines call through
 * this slot to allocate; game main installs malloc here at startup.
 * Dword (32-bit pointer) value, not a word. No callers in static xrefs
 * (the API entry; the slot itself is consumed by the AIL_internal_*
 * routines as a data pointer). Paired with the +4 slot helper @ 0x3616E
 * which swaps data_ail_free_fnptr.
 * ---------------------------------------------------------------- */
uint32 fd2_set_word_global_52758(uint32 new_val)
{
    uint32 old;
    old = data_ail_alloc_fnptr;
    data_ail_alloc_fnptr = new_val;
    return old;
}

/* ----------------------------------------------------------------
 * fd2_set_word_global_5275c @ 0x3616E
 *
 * Get-and-set helper for the AIL internal de-allocator slot
 * data_ail_free_fnptr (0x5275C): stores new_val as the new free
 * function pointer and returns the prior pointer (so a caller can save
 * and restore it). The AIL internal routines call through this slot to
 * release memory; default value is the CRT free. Dword (32-bit pointer)
 * value, not a word. No callers in static xrefs (the API entry; the slot
 * itself is consumed as a data pointer by AIL_internal_decommit_and_free
 * and the AIL load/install routines). Paired with the alloc-slot helper
 * @ 0x3615E which swaps data_ail_alloc_fnptr (the +4-below sibling).
 * ---------------------------------------------------------------- */
uint32 fd2_set_word_global_5275c(uint32 new_val)
{
    uint32 old;
    old = data_ail_free_fnptr;
    data_ail_free_fnptr = new_val;
    return old;
}

/* ----------------------------------------------------------------
 * fd2_any_char_has_item @ 0x24B14  (3 callers)
 *
 * Returns 1 if any character in runtime_char_array[0..15] holds the
 * given item_id, else -1. Scans chars 0..15, calling
 * fd2_find_inventory_slot_with_item(char_idx, item_id) on each; on the
 * first char whose search returns a slot (!= -1) it returns 1 at once,
 * otherwise -1 after all 16 chars miss. Used to detect plot-critical
 * items in party inventory (e.g. 天空之鑰 / item 100) for story branches:
 * fd2_chapter_23_end, fd2_chapter_27_end, fd2_chapter_27_init.
 * ---------------------------------------------------------------- */
int fd2_any_char_has_item(int item_id)
{
    int char_idx;

    for (char_idx = 0; char_idx < 0x10; char_idx++) {
        if (fd2_find_inventory_slot_with_item(char_idx, item_id) != -1) {
            return 1;
        }
    }
    return -1;
}

/* ----------------------------------------------------------------
 * fd2_find_template_char_by_id @ 0x24BDE  (1 caller)
 *
 * Linear-scans the menu/template party roster (buffer ptr at
 * data_fd2_shared_menu_party_roster_buffer_ptr, 0x50-byte stride,
 * count at data_fd2_shared_menu_party_member_count) for an entry
 * whose char_id byte at offset +0x08 equals char_id. Returns 1 on
 * the first match, 0 if the scan exhausts.
 *
 * Byte-identical duplicate of fd2_check_party_has_char_id @ 0x33499
 * (Watcom emitted the same body into two translation units). This
 * copy resides in the battle/spell address range.
 *
 * Caller: fd2_chapter_23_end uses it as the "蜜蒂 (char_id 0x12) is
 * in the party" predicate for the Phase-1 conditional joins.
 *
 * Cdecl, 1 stack param; int return. The binary's __CHK(8) stack-probe
 * prologue is compiler-generated and omitted here. EBX is callee-saved.
 * ---------------------------------------------------------------- */
int fd2_find_template_char_by_id(uint32 char_id)
{
    int idx;

    for (idx = 0; (int32)data_fd2_shared_menu_party_member_count > idx; idx++) {
        if (*(uint8 *)(idx * 0x50 + 8 +
                       data_fd2_shared_menu_party_roster_buffer_ptr) == char_id) {
            return 1;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_check_party_has_char_id @ 0x33499  (7 callers)
 *
 * Linear-scans the menu/template party roster (buffer ptr at
 * data_fd2_shared_menu_party_roster_buffer_ptr, 0x50-byte stride,
 * count at data_fd2_shared_menu_party_member_count) for an entry
 * whose char_id byte at offset +0x08 equals char_id. Returns 1 on
 * the first match, 0 if the scan exhausts. char_id is the char_id
 * (init value, the value passed to fd2_init_runtime_char_from_base_growth
 * when the char joined), NOT a job_id/class.
 *
 * Byte-identical twin of fd2_find_template_char_by_id @ 0x24BDE
 * (Watcom emitted the same body into two translation units); this copy
 * is in the menu/chapter address range. Distinct from
 * fd2_require_char_id_in_active_party @ 0x31DBE, which iterates the live
 * runtime_char_array (active battle scope) and shows an error dialog on
 * a miss.
 *
 * Callers / use cases: chapter 15/17 init/end dialog branches
 * (凱麗 char_id 0xC, 蜜蒂 char_id 0x12), fd2_chapter_17_post_action
 * lose condition, fd2_run_recruitment_or_branch_screen required-char
 * gate, and fd2_render_party_status_overview_content (0x12 present check).
 *
 * Cdecl, 1 stack param; uint32 return. The binary's __CHK(8) stack-probe
 * prologue is compiler-generated and omitted here. EBX is callee-saved.
 * ---------------------------------------------------------------- */
uint32 fd2_check_party_has_char_id(uint32 char_id)
{
    int iter;

    for (iter = 0; (int32)data_fd2_shared_menu_party_member_count > iter;
         iter++) {
        if (*(uint8 *)(iter * 0x50 + 8 +
                       data_fd2_shared_menu_party_roster_buffer_ptr) == char_id) {
            return 1;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------
 * fd2_require_char_id_in_active_party @ 0x31DBE  (2 callers)
 *
 * Verifies a required char_id is in the active battle party
 * (runtime_char_array slots 1..max_chars). If missing, shows a
 * "you need [char-name]" portrait error dialog and returns 0; if
 * present, returns 1 with no dialog.
 *
 * max_chars = active-party iteration cap (caller passes 0x0F or
 * 0x13). req_char_id = required char_id (low byte). The scan checks
 * slots [1..max_chars]: slot 0 is the lord (always present, not
 * checked); slots 1..max_chars are the player-selected party from
 * the recruitment screen.
 *
 * On a miss it loads chapter portrait 0x4B, sets the dynamic per-char
 * portrait slot to (req_char_id & 0xFF) + 1, renders the "you need
 * [name]" dialog scene, raises the battle-tile-map input guard,
 * waits for an input dialog with blink, lowers the guard, then slides
 * the dialog out.
 *
 * Distinct from fd2_check_party_has_char_id @ 0x33499 (template/menu
 * scope, no side effect); this one checks the active battle scope and
 * emits an error dialog.
 *
 * Caller: fd2_run_recruitment_or_branch_screen @ 0x31CC2 / 0x31CFF
 * (required-class gate after commit).
 *
 * Cdecl, 2 stack params; char return. The binary's __CHK(0x30)
 * stack-probe prologue is compiler-generated and omitted here. EBX is
 * callee-saved.
 * ---------------------------------------------------------------- */
char fd2_require_char_id_in_active_party(uint32 max_chars, uint32 req_char_id)
{
    uint8 found;
    int iter;

    found = 0;
    for (iter = 0; iter < (int32)max_chars; iter++) {
        if (data_fd2_battle_runtime_char_array_ptr[iter + 1].char_id ==
            (uint8)req_char_id) {
            found = 1;
        }
    }
    if (found == 0) {
        fd2_dialog_open_speaker_portrait(0x4B);
        data_fd2_dialog_last_action_sprite_id_param = (req_char_id & 0xFF) + 1;
        fd2_display_dialog_scene(data_fd2_all_game_text_ptr, 0x291, 0xA951F,
                                 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
        fd2_paint_portrait_to_dialog_area(0);
        data_fd2_battle_tile_map_ptr = 1;
        fd2_wait_for_input_dialog_with_blink(0);
        data_fd2_battle_tile_map_ptr = 0;
        fd2_close_intro_dialog_with_slide_out();
    }
    return (char)found;
}

/* ----------------------------------------------------------------
 * fd2_count_selected_chars @ 0x320CE  (2 callers)
 *
 * Counts the non-zero bytes in the per-member selection_state array
 * sel_state over the iteration range [0, menu_party_member_count - 1)
 * — i.e. every recruitable party member except the final sentinel slot.
 * Returns the count of selected members.
 *
 * Used by the recruitment screen:
 *   - fd2_render_recruitment_select_screen @ 0x31E80 to display the
 *     "N selected" / "max-N remaining" counters.
 *   - fd2_run_recruitment_or_branch_screen @ 0x318AD to auto-commit when
 *     the count equals max_chars after a toggle (caller compares the
 *     returned EAX against its max-chars cap).
 *
 * Cdecl, 1 stack param (pointer to the selection_state byte array); int
 * return in EAX. The binary's __CHK(8) stack-probe prologue is
 * compiler-generated and omitted here. EBX (the running count) is
 * callee-saved and is the value loaded into EAX by the shared epilogue
 * the loop exits into.
 * ---------------------------------------------------------------- */
int fd2_count_selected_chars(uint32 sel_state)
{
    int count;
    int iter;

    count = 0;
    for (iter = 0;
         iter < (int32)data_fd2_shared_menu_party_member_count - 1;
         iter++) {
        if (*(char *)(sel_state + iter) != '\0') {
            count++;
        }
    }
    return count;
}

/* ----------------------------------------------------------------
 * fd2_reorder_party_by_selection @ 0x320FC  (1 caller)
 *
 * Reorders the menu/template party roster
 * (data_fd2_shared_menu_party_roster_buffer_ptr, 0x50-byte entries) by the
 * per-member selection_state array sel_state: every selected member
 * (sel_state[i] != 0) is packed to the front (roster slots 1..K) and every
 * unselected member drops to the back (slots K+1..N-1). Slot 0 (the lord)
 * is never moved.
 *
 * It snapshots the original roster into a 0xA00-byte temp buffer (0xA00/0x50
 * = 0x20 = 32 entries), then makes two passes over iter in
 * [0, menu_party_member_count - 1): pass 1 copies the snapshot entry
 * (iter+1) into the next output slot when selected; pass 2 copies it when
 * unselected. The shared out_slot index advances across both passes, so the
 * selected block is laid down first and the unselected block follows it. The
 * (iter+1) source index skips snapshot slot 0 (the lord) — only the
 * non-lord template chars are reordered. The temp buffer is freed before
 * return.
 *
 * Caller: fd2_run_recruitment_or_branch_screen @ 0x31a74, invoked when Enter
 * commits a complete selection (count == max_chars).
 *
 * Cdecl, 1 stack param (pointer to the selection_state byte array); void
 * return. The binary's __CHK(0x20) stack-probe prologue is compiler-
 * generated and omitted here, and the free()+register-restore epilogue is a
 * shared tail (PUSH dst; JMP 0x301E7) regenerated by the trailing free().
 * EBX/ESI/EDI are callee-saved.
 * ---------------------------------------------------------------- */
void fd2_reorder_party_by_selection(uint32 sel_state)
{
    void *snapshot;
    int iter;
    int out_slot;

    out_slot = 1;
    snapshot = malloc(0xA00);
    memmove(snapshot,
            (void *)data_fd2_shared_menu_party_roster_buffer_ptr, 0xA00);

    for (iter = 0;
         iter < (int32)data_fd2_shared_menu_party_member_count - 1;
         iter++) {
        if (*(char *)(sel_state + iter) != '\0') {
            memmove((void *)((uint32)out_slot * 0x50 +
                             data_fd2_shared_menu_party_roster_buffer_ptr),
                    (void *)((uint32)(iter + 1) * 0x50 + (uint32)snapshot),
                    0x50);
            out_slot++;
        }
    }

    for (iter = 0;
         iter < (int32)data_fd2_shared_menu_party_member_count - 1;
         iter++) {
        if (*(char *)(sel_state + iter) == '\0') {
            memmove((void *)((uint32)out_slot * 0x50 +
                             data_fd2_shared_menu_party_roster_buffer_ptr),
                    (void *)((uint32)(iter + 1) * 0x50 + (uint32)snapshot),
                    0x50);
            out_slot++;
        }
    }

    free(snapshot);
}

/* ----------------------------------------------------------------
 * fd2_pin_required_char_to_party_slot1 @ 0x321C8  (1 caller)
 *
 * Pins the character whose char_id == char_id into menu/template
 * roster slot 1 and shifts the remaining non-lord chars down to slots
 * 2..N-1. Slot 0 (the lord) is never touched.
 *
 * It scans the active battle roster
 * (data_fd2_battle_runtime_char_array_ptr) over slots
 * [1, menu_party_member_count) for the entry whose char_id byte
 * (+0x08) matches char_id, recording match_idx (the loop runs to the
 * end with no early-out, so the LAST match wins; char_ids are unique in
 * practice so 0 or 1 match exists). It snapshots the template roster
 * (data_fd2_shared_menu_party_roster_buffer_ptr, 0x50-byte entries)
 * into a 0xA00-byte temp (0xA00/0x50 = 0x20 = 32 entries), copies the
 * matched snapshot entry into roster slot 1, then packs every other
 * snapshot entry [1..N-1] (skipping match_idx) into slots 2.. in order.
 * The temp is freed.
 *
 * It then reloads the portrait sprite cache to match the new roster
 * order: free(data_fd2_portrait_sprite_cache); reopen FDICON.B24; reset
 * portrait_cache_count to 0; for each roster slot [0, member_count)
 * call fd2_load_portrait_to_cache(roster[slot].portrait_id (+0x07), fp);
 * fclose(fp).
 *
 * char_id = required char_id (low byte; this is a char_id, NOT a
 * class/job id). The runtime roster slot i and template roster slot i
 * reference the same char, so the search index maps directly to the
 * reorder index.
 *
 * Caller: fd2_run_recruitment_or_branch_screen @ 0x31D2A / 0x31D34
 * (post-confirm, after the required-char-id check passes).
 *
 * Cdecl, 1 stack param; void return. The binary's __CHK(0x20) stack-
 * probe prologue is compiler-generated and omitted here. EBX/ESI are
 * callee-saved.
 * ---------------------------------------------------------------- */
void fd2_pin_required_char_to_party_slot1(uint32 char_id)
{
    void *snapshot;
    int iter;
    void *fp;
    uint8 match_idx;
    uint8 out_slot;

    match_idx = 0;
    out_slot = 2;

    for (iter = 1; iter < (int32)data_fd2_shared_menu_party_member_count;
         iter++) {
        if (data_fd2_battle_runtime_char_array_ptr[iter].char_id ==
            (uint8)char_id) {
            match_idx = (uint8)iter;
        }
    }

    snapshot = malloc(0xA00);
    memmove(snapshot,
            (void *)data_fd2_shared_menu_party_roster_buffer_ptr, 0xA00);
    memmove((void *)(data_fd2_shared_menu_party_roster_buffer_ptr + 0x50),
            (void *)((uint32)match_idx * 0x50 + (uint32)snapshot), 0x50);

    for (iter = 1; iter < (int32)data_fd2_shared_menu_party_member_count;
         iter++) {
        if ((uint32)iter != match_idx) {
            memmove((void *)((uint32)out_slot * 0x50 +
                             data_fd2_shared_menu_party_roster_buffer_ptr),
                    (void *)((uint32)iter * 0x50 + (uint32)snapshot), 0x50);
            out_slot++;
        }
    }

    free(snapshot);

    free((void *)data_fd2_portrait_sprite_cache);
    fp = fopen("FDICON.B24", "rb");
    data_fd2_resource_portrait_cache_count = 0;
    for (iter = 0;
         iter < (int32)data_fd2_shared_menu_party_member_count; iter++) {
        fd2_load_portrait_to_cache(
            *(uint8 *)(data_fd2_shared_menu_party_roster_buffer_ptr +
                       iter * 0x50 + 7),
            (uint32)fp);
    }
    fclose(fp);
}

/* ----------------------------------------------------------------
 * fd2_delay_ms @ 0x375B2  (50+ callers, game-wide)
 *
 * Thin millisecond-delay wrapper: the binary's body is a single tail-call
 * (JMP) to the Watcom CRT __delay (0x3DCCD). Called directly from across the
 * game (field menus, dialog / battle / AI / spell animations, save-load, the
 * turn cycle) wherever a fixed pause is needed; e.g. fd2_delay_ms(0x50) ~ 80ms.
 *
 * The emit pipeline had carried this as a synthetic address-suffixed thunk name
 * (__delay_thunk_375b2; the binary has no symbol at 0x375B2); renamed to
 * fd2_delay_ms and restored here as a real function. The delay(ms) call re-emits
 * the equivalent CRT tail-call.
 * ---------------------------------------------------------------- */
void fd2_delay_ms(uint32 ms)
{
    delay(ms);
}

/* ----------------------------------------------------------------
 * fd2_delay_400ms_via_idle_thunk @ 0x353CC  (1 caller)
 *
 * Fixed 400ms delay wrapper: PUSH 0x190 (=400); CALL fd2_delay_ms;
 * ADD ESP,4; RET. No params, void return. The 400 argument is pushed
 * and cleaned up around the call (cdecl); fd2_delay_ms forwards it to
 * the Watcom CRT delay(ms), so 400 is the delay duration in milliseconds.
 *
 * Caller: fd2_cinematic_chapter_portrait_dump_with_white_flash @ 0x35822.
 * (The same 4-instruction body is also reached as the fall-through tail of
 * fd2_chapter_event_handler_36__ch24_cinematic @ 0x3535D, which the other
 * handlers reproduce as their own inline fd2_delay_ms calls.)
 * ---------------------------------------------------------------- */
void fd2_delay_400ms_via_idle_thunk(void)
{
    fd2_delay_ms(400);
}
