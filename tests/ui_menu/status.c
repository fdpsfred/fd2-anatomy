/*
 * unit tests for src/ui_menu/status.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>

#define USE_ITEM_ID 10

extern runtime_char g_test_rc_array[8];
extern int g_build_spell_list_return;
extern int g_ail_vol_calls;
extern int g_ail_last_vol;
extern int g_ail_last_ramp;
extern uint8 data_fd2_audio_bgm_last_set_track_id;
extern uint8 data_fd2_battle_summon_minor_anim_state5_frame_counter;
extern uint8 data_fd2_battle_summon_minor_anim_alternating_blit_toggle;
extern int g_ending_menu_return;
extern int g_slot_selector_return;
extern int g_chapter_transition_return;
extern int g_play_sfx_with_handle_calls;
extern int g_play_sfx_sample_from_bank_calls;
extern int g_blit_indexed_sprite_calls;
extern uint32 g_blit_indexed_sprite_last_frame;
extern int g_blit_indexed_sprite_last_x;
extern int g_blit_indexed_sprite_last_y;
extern int g_composite_call_count;
extern int g_attack_dispatch_return;
extern int g_attack_dispatch_calls;
extern int g_seek_optimal_return;
extern int g_advance_nearest_return;
extern int g_walk_return;
extern int g_score_physical_return;
extern int g_pass_turn_calls;
extern int g_execute_spell_calls;
extern int g_execute_physical_calls;
extern int g_pathfind_return;
extern int g_pathfind_walk_return;
extern int g_pathfind_write_dst;
extern int g_pathfind_dst_x;
extern int g_pathfind_dst_y;
extern int g_pathfind_seq_enable;
extern int g_pathfind_seq[4];
extern int g_pathfind_seq_idx;
extern int g_pathfind_seq_steps;
extern uint8 g_pathfind_step_bytes[8];
extern int g_pathfind_md0_dst_x;
extern int g_pathfind_md0_dst_y;
extern uint8 g_spell_list_buf[12];
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;
extern int g_composite_call_count;
/* fake for the remaining unemitted party-query callee of the real
 * fd2_render_party_status_overview_content (testglob.c). The team-alive
 * counter (fd2_count_active_chars_for_team_filter) is now real and reads
 * data_fd2_battle_party_member_count / g_test_rc_array. */
extern uint32 g_has_char_fake;
/* data_fd2_ui_slide_* workspace ptr globals are declared in globals.h */

/* Inject one keystroke into the BIOS keyboard buffer (BDA @ 0x400) so the real
 * fd2_wait_for_input_dialog_with_blink() exits its busy-wait on the first poll
 * and INT 16h fn 10h returns `scancode` in AH. head != tail makes the buffer
 * non-empty; the buffer-head word @ 0x41E carries scancode (high) / ASCII (low).
 * Mirrors tests/input/input.c (test_wait_dialog_blink_esc). */
static void kbd_inject_scancode(int scancode)
{
    *(volatile uint16 *)0x41AuL = 0x1E;                       /* head        */
    *(volatile uint16 *)0x41CuL = 0x20;                       /* tail=head+2 */
    *(volatile uint16 *)0x41EuL = (uint16)((scancode << 8) & 0xFF00); /* AH=scancode */
}



/* ---- Test: combat bubble pos ---- */

static void test_stat_preview_basic(void)
{
    int32 stats[4];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    fd2_compute_equipped_stats_with_item_preview(0, 0, (uint32)stats);
    ASSERT_EQ(stats[0], 0);
    ASSERT_EQ(stats[1], 0);
}


/*
 * Candidate = WEAPON (preview_cat 0x01 <= 0x14). Exercises the loop add-block
 * and the category-opposition branch (asm 0x2f052-0x2f08e), which the all-zero
 * smoke test above never enters.
 *   Base stats: AP=50 (aux+0x10), DP=20 (aux+0x12), DX=Stat4=30 (dx_block+1).
 *   Candidate item[1] (type 0x01): .ap10 .ht3 .dp2 .ev1
 *     -> AP=60 DP=22 DX=33 Stat4=31.
 *   slot0 = equipped ARMOR item[2] (type 0x15, OPPOSITE category) added:
 *     .ap0 .ht0 .dp8 .ev2 -> AP=60 DP=30 DX=33 Stat4=33.
 *   slot1 = equipped WEAPON item[3] (type 0x05, SAME category) MUST be excluded
 *     (preview replaces it): .ap100 .dp100 -> no effect, proving the exclusion.
 * fd2_get_item_effect_entry returns &item.type, so item+1/+3/+5/+7 read
 * .ap/.ht/.dp/.ev and route to AP/DX/DP/Stat4 (mirrors the sibling
 * test_recalc_combat_stats_equipped_item mapping).
 */
static void test_stat_preview_weapon_opposite_and_same_category(void)
{
    int32 stats[4];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 50;   /* base AP */
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x12) = 20;   /* base DP */
    *(int16 *)(g_test_rc_array[0].ai_target_and_dx_block + 1) = 30;/* base DX */
    data_fd2_battle_item_effect_table[1].type = 0x01;  /* candidate weapon */
    data_fd2_battle_item_effect_table[1].ap = 10;
    data_fd2_battle_item_effect_table[1].ht = 3;
    data_fd2_battle_item_effect_table[1].dp = 2;
    data_fd2_battle_item_effect_table[1].ev = 1;
    g_test_rc_array[0].inventory_slots[0] = 0x40;      /* slot0 equipped     */
    g_test_rc_array[0].inventory_slots[1] = 2;         /* armor, opposite    */
    data_fd2_battle_item_effect_table[2].type = 0x15;
    data_fd2_battle_item_effect_table[2].dp = 8;
    data_fd2_battle_item_effect_table[2].ev = 2;
    g_test_rc_array[0].inventory_slots[2] = 0x40;      /* slot1 equipped     */
    g_test_rc_array[0].inventory_slots[3] = 3;         /* weapon, same cat   */
    data_fd2_battle_item_effect_table[3].type = 0x05;
    data_fd2_battle_item_effect_table[3].ap = 100;     /* must be excluded   */
    data_fd2_battle_item_effect_table[3].dp = 100;
    fd2_compute_equipped_stats_with_item_preview(0, 1, (uint32)stats);
    ASSERT_EQ(stats[0], 60);   /* AP    = 50 + 10 + 0 */
    ASSERT_EQ(stats[1], 30);   /* DP    = 20 +  2 + 8 */
    ASSERT_EQ(stats[2], 33);   /* DX    = 30 +  3 + 0 */
    ASSERT_EQ(stats[3], 33);   /* Stat4 = 30 +  1 + 2 */
}


/*
 * Candidate = ARMOR (preview_cat 0x16 > 0x14): covers the SECOND branch path
 * (asm 0x2f061-0x2f06e). Distinct base values catch any field mis-routing.
 *   Base: AP=5 DP=7 DX=Stat4=11 (DX/Stat4 share dx_block+1).
 *   Candidate armor item[4] (type 0x16): .ap1 .ht2 .dp3 .ev4
 *     -> AP=6 DP=10 DX=13 Stat4=15.
 *   slot0 = equipped WEAPON item[5] (type 0x05, OPPOSITE) added:
 *     .ap40 .ht50 .dp60 .ev70 -> AP=46 DP=70 DX=63 Stat4=85.
 *   slot1 = equipped ARMOR item[6] (type 0x1F, SAME category) excluded.
 *   slot2 = WEAPON item[7] but NOT equipped (flag bit 0x40 clear) -> the
 *     equipped-flag gate (asm 0x2f04d TEST/JZ) skips it despite opposite cat.
 */
static void test_stat_preview_armor_branch_and_flag_gate(void)
{
    int32 stats[4];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 5;    /* base AP */
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x12) = 7;    /* base DP */
    *(int16 *)(g_test_rc_array[0].ai_target_and_dx_block + 1) = 11;/* base DX */
    data_fd2_battle_item_effect_table[4].type = 0x16;  /* candidate armor   */
    data_fd2_battle_item_effect_table[4].ap = 1;
    data_fd2_battle_item_effect_table[4].ht = 2;
    data_fd2_battle_item_effect_table[4].dp = 3;
    data_fd2_battle_item_effect_table[4].ev = 4;
    g_test_rc_array[0].inventory_slots[0] = 0x40;      /* slot0 equipped     */
    g_test_rc_array[0].inventory_slots[1] = 5;         /* weapon, opposite   */
    data_fd2_battle_item_effect_table[5].type = 0x05;
    data_fd2_battle_item_effect_table[5].ap = 40;
    data_fd2_battle_item_effect_table[5].ht = 50;
    data_fd2_battle_item_effect_table[5].dp = 60;
    data_fd2_battle_item_effect_table[5].ev = 70;
    g_test_rc_array[0].inventory_slots[2] = 0x40;      /* slot1 equipped     */
    g_test_rc_array[0].inventory_slots[3] = 6;         /* armor, same cat    */
    data_fd2_battle_item_effect_table[6].type = 0x1F;
    data_fd2_battle_item_effect_table[6].ap = 1000;    /* must be excluded   */
    data_fd2_battle_item_effect_table[6].dp = 2000;
    g_test_rc_array[0].inventory_slots[4] = 0x00;      /* slot2 NOT equipped */
    g_test_rc_array[0].inventory_slots[5] = 7;         /* weapon, opposite   */
    data_fd2_battle_item_effect_table[7].type = 0x03;
    data_fd2_battle_item_effect_table[7].ap = 999;     /* flag-gated out     */
    fd2_compute_equipped_stats_with_item_preview(0, 4, (uint32)stats);
    ASSERT_EQ(stats[0], 46);   /* AP    = 5  + 1 + 40 */
    ASSERT_EQ(stats[1], 70);   /* DP    = 7  + 3 + 60 */
    ASSERT_EQ(stats[2], 63);   /* DX    = 11 + 2 + 50 */
    ASSERT_EQ(stats[3], 85);   /* Stat4 = 11 + 4 + 70 */
}


/*
 * Signed-16-bit guard: the function reads item bonuses with MOVSX (asm
 * 0x2f070 MOVSX word). A negative bonus must DECREASE the stat. If the emit
 * ever regressed to a zero/unsigned read, AP would become 100 or 100+0xFFE2.
 *   Base AP=100; candidate weapon item[1] all-zero; slot0 equipped armor
 *   item[8] (opposite) with .ap = -30 (int16) -> AP = 70.
 */
static void test_stat_preview_signed_negative_bonus(void)
{
    int32 stats[4];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(data_fd2_battle_item_effect_table));
    *(int16 *)(g_test_rc_array[0].combat_aux_block + 0x10) = 100;  /* base AP */
    data_fd2_battle_item_effect_table[1].type = 0x01;  /* candidate weapon  */
    g_test_rc_array[0].inventory_slots[0] = 0x40;      /* slot0 equipped    */
    g_test_rc_array[0].inventory_slots[1] = 8;         /* armor, opposite   */
    data_fd2_battle_item_effect_table[8].type = 0x15;
    data_fd2_battle_item_effect_table[8].ap = (uint16)(-30);  /* int16 -30  */
    fd2_compute_equipped_stats_with_item_preview(0, 1, (uint32)stats);
    ASSERT_EQ(stats[0], 70);   /* AP = 100 + (-30), proves MOVSX sign-extend */
    ASSERT_EQ(stats[1], 0);    /* DP unchanged (no DP bonuses)               */
}


/*
 * fd2_close_status_screen_with_slide_out @ 0x196CB — end-to-end smoke.
 *
 * The function is a blit/free/recomposite teardown (no return value, no
 * branching logic beyond the fixed 1..5 slide loop), so the host-observable
 * proxy is g_composite_call_count: it must run the 5-frame slide loop, the
 * VRAM-restore memmove, the three free()s, and then composite exactly one
 * battle frame. We pre-allocate the three workspace buffers (the open
 * counterpart's job) so the real fd2_slide_panel_down_step memmoves stay in
 * bounds; the function itself free()s all three, so the test must NOT free
 * them again and resets the globals to 0 afterward to avoid dangling ptrs.
 *
 * Buffer math (proves the allocation size is sufficient): the largest
 * fd2_slide_panel_down_step write in this loop is at y_offset=0x7D
 * (frame 1: 1*0xD+0x70=0x7D) — row_count = 200-125 = 75, top write
 * 5 + 199*320 + 0x136 = 63995 < 64000; the largest read from src is
 * 0x8C05 + 0x55*0x140 + 0x136 = 63355 < 64000. All five frames stay
 * within the 64000-byte mode-13h workspaces.
 *
 * memmove((void*)0xA0000, ...) and the in-loop blit to 0xA0000 target the
 * VGA aperture; under DOS/4GW 0xA0000 is real VGA RAM so the writes are
 * harmless (same convention as tests/anim/aniwalk2.c and tests/gfx).
 */
static void test_close_status_screen_slide_out_runs_full_teardown(void)
{
    data_fd2_ui_slide_anim_accumulator_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_bg_snapshot_buf_ptr = (uint32)malloc(64000);
    data_fd2_ui_slide_composed_target_buf_ptr = (uint32)malloc(64000);
    ASSERT_TRUE(data_fd2_ui_slide_anim_accumulator_buf_ptr != 0);
    ASSERT_TRUE(data_fd2_ui_slide_bg_snapshot_buf_ptr != 0);
    ASSERT_TRUE(data_fd2_ui_slide_composed_target_buf_ptr != 0);

    /* fd2_composite_battle_frame's pipeline stages are stubbed; phase 0
     * makes the real fd2_paint_cursor_overlay_pattern a no-op. */
    data_fd2_battle_anim_phase = 0;
    g_composite_call_count = 0;

    fd2_close_status_screen_with_slide_out();

    /* Step 4 recomposites exactly one frame after the teardown. */
    ASSERT_EQ(g_composite_call_count, 1);

    /* The function already free()d all three; drop the dangling globals so
     * later tests in the suite never reuse a freed pointer. */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
}


/*
 * fd2_open_party_status_overview_screen @ 0x1B1E7 — end-to-end smoke.
 *
 * The army-status overview is a pure blit/display orchestrator: it allocates
 * two 64000-byte mode-13h workspaces, composites the battle scene + party
 * roster, runs a 12-frame slide-in, spins in a wait loop until a key is
 * pressed (redrawing on each BIOS-tick change), runs a 12-frame slide-out,
 * restores the screen, and frees both buffers. It has no numeric/RNG/state
 * logic of its own, so the host-observable proxies are: (1) the function
 * RETURNS (never hangs) — proving the wait-loop exit condition is wired to
 * the keyboard-poll return value (an EAX-tracking regression would loop
 * forever and trip the harness hang detector); (2) the static overview
 * content renderer ran at least once during setup.
 *
 * Deterministic termination: the wait loop is `do { kbd = poll(); ...redraw
 * if tick changed... } while (kbd == 0);`. We pre-fill the BIOS keyboard
 * buffer NONEMPTY (tail = head + 2) before the call, so the very first
 * iteration's poll returns nonzero and the loop exits after exactly one
 * pass — no dependence on the uninitialized last_tick gate, no hang risk.
 *
 * Fixture: the real slide-panel / composite / blit_rectangle / palette
 * callees are linked for real, so we give large_game_state_buffer a 128 KB
 * backing (the setup fd2_blit_rectangle reads large_game_state_buffer+0x8088
 * for 192 rows of stride 456 -> top read ~120.7 KB) and set the party
 * member count to 0 so fd2_composite_all_chars_overlay / shadow overlay
 * become no-ops (no per-char sprite fixture needed). The two 64000-byte
 * workspaces are malloc'd internally and freed internally; their blits stay
 * in bounds.
 *
 * fd2_render_party_status_overview_content is now the REAL body (src/gfx/
 * rndstat.c), so it is exercised end-to-end here. To keep that safe we stand
 * up the minimal fixtures it dereferences: a zeroed sprite-sheet whose offset
 * table resolves every sprite to (sheet + 0) so the real
 * fd2_blit_indexed_sprite_at_xy -> fd2_rle_blit_sprite spy reads a valid byte,
 * and an immediate-END text program (every page word -> a -1 opcode) so the
 * two real fd2_display_dialog_scene calls return at once (no DAT fopen, no
 * input wait). Its two unemitted party-query callees are faked in testglob.c
 * (team counts 0, has-char 0); chapter id is parked off the Mitti special
 * case. The content renderer's largest write at stride 456 lands within the
 * 128 KB backing (gold field ~+0x1B3xx of the +0x7964 base). Writes to
 * 0xA0000 hit the VGA aperture (harmless under DOS/4GW, same convention as the
 * sibling status-screen and gfx tests).
 */
static uint8  g_overview_sheet[1024];
static uint16 g_overview_text[0x400];

static void test_open_party_overview_runs_and_returns(void)
{
    uint32 saved_lgsb;
    uint32 saved_count;
    int    saved_flip;
    uint32 saved_sheet;
    uint32 saved_text;
    uint32 saved_chapter;
    uint32 saved_turn;
    uint32 saved_gold;
    int    reached;
    int    i;

    saved_lgsb = data_fd2_large_game_state_buffer_ptr;
    saved_count = data_fd2_battle_party_member_count;
    saved_flip = g_repaint_flip_buffer_after;
    saved_sheet = data_fd2_ui_anim_sprite_sheet_ptr;
    saved_text = data_fd2_all_game_text_ptr;
    saved_chapter = data_fd2_chapter_current_chapter_id;
    saved_turn = data_fd2_battle_turn_counter;
    saved_gold = data_fd2_shared_party_total_gold;

    /* 128 KB backing so the real setup blit's +0x8088 stride-456 reads stay
     * in bounds; party count 0 makes the real overlay compositors no-ops. */
    data_fd2_large_game_state_buffer_ptr = (uint32)malloc(0x20000);
    ASSERT_TRUE(data_fd2_large_game_state_buffer_ptr != 0);
    data_fd2_battle_party_member_count = 0;
    g_repaint_flip_buffer_after = 0;     /* not using the seam; kbd starts set */

    /* sprite sheet: zeroed offset table -> every sprite resolves to sheet+0. */
    memset(g_overview_sheet, 0, sizeof(g_overview_sheet));
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_overview_sheet;
    /* immediate-END text program: park a -1 opcode high, point every page word
     * at it so both real dialog calls return without touching DATO.DAT. */
    for (i = 0; i < 0x400; i++) {
        g_overview_text[i] = 0x600;
    }
    *(int16 *)((uint8 *)g_overview_text + 0x600) = -1;
    data_fd2_all_game_text_ptr = (uint32)g_overview_text;
    /* small numbers (normal decimal path) and chapter off the Mitti branch. */
    data_fd2_chapter_current_chapter_id = 3;
    data_fd2_battle_turn_counter = 12;
    data_fd2_shared_party_total_gold = 5000;
    /* party_member_count is 0 (set above), so the real
     * fd2_count_active_chars_for_team_filter returns 0 for every team. */
    g_has_char_fake = 0;

    /* Pre-arm the BIOS keyboard buffer as NONEMPTY so the wait loop exits on
     * its first poll (tail != head). Deterministic single-iteration exit. */
    *(volatile uint16 *)0x41AuL = 0x1E;          /* head */
    *(volatile uint16 *)0x41CuL = 0x20;          /* tail = head + 2 -> nonempty */

    reached = 0;
    fd2_open_party_status_overview_screen();
    reached = 1;

    /* Reaching here proves the wait loop terminated (no hang) and the real
     * setup-pass overview render completed without faulting. */
    ASSERT_EQ((long)reached, 1);

    free((void *)data_fd2_large_game_state_buffer_ptr);
    data_fd2_large_game_state_buffer_ptr = saved_lgsb;
    data_fd2_battle_party_member_count = saved_count;
    g_repaint_flip_buffer_after = saved_flip;
    data_fd2_ui_anim_sprite_sheet_ptr = saved_sheet;
    data_fd2_all_game_text_ptr = saved_text;
    data_fd2_chapter_current_chapter_id = saved_chapter;
    data_fd2_battle_turn_counter = saved_turn;
    data_fd2_shared_party_total_gold = saved_gold;
}


/* ----------------------------------------------------------------
 * fd2_count_usable_inventory_slots @ 0x1B8A6
 *
 * Counts inventory slots whose flag byte (inventory_slots[slot*2]) has bit
 * 0x80 CLEAR, over the 8 slots of runtime_char[ci]. These tests pin the
 * load-bearing details: the 0x80-clear polarity, the 2-byte slot stride
 * (flag at the even byte; the odd item-id byte must be ignored), the 8-slot
 * bound, and correct indexing of ci into the 0x50-stride runtime_char array
 * (g_test_rc_array, wired to data_fd2_battle_runtime_char_array_ptr).
 * ---------------------------------------------------------------- */

/* All flag bytes clear -> all 8 slots active. */
static void test_count_usable_all_clear(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    ASSERT_EQ(fd2_count_usable_inventory_slots(0), 8);
}

/* All flag bytes bit-0x80 set -> 0 active (the Item-gate boundary). */
static void test_count_usable_all_set(void)
{
    int s;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (s = 0; s < 8; s++) {
        g_test_rc_array[0].inventory_slots[s * 2] = 0x80;
    }
    ASSERT_EQ(fd2_count_usable_inventory_slots(0), 0);
}

/* Mixed pattern: slots 1,3,5 inactive (0x80 set) -> 5 active. Also proves the
 * test reads only the FLAG byte (even offset): the odd item-id bytes are all
 * 0xFF (bit 0x80 set) yet must not be counted, and other flag-byte bits set
 * alongside non-0x80 values must not flip the result. */
static void test_count_usable_mixed_and_stride(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (i = 0; i < 8; i++) {
        g_test_rc_array[0].inventory_slots[i * 2 + 1] = 0xFF;   /* item id byte */
    }
    g_test_rc_array[0].inventory_slots[0] = 0x40;   /* slot 0 active (bit7 clear) */
    g_test_rc_array[0].inventory_slots[2] = 0x80;   /* slot 1 inactive */
    g_test_rc_array[0].inventory_slots[6] = 0xC0;   /* slot 3 inactive (bit7 set) */
    g_test_rc_array[0].inventory_slots[10] = 0x80;  /* slot 5 inactive */
    /* slots 0,2,4,6,7 active -> 5 */
    ASSERT_EQ(fd2_count_usable_inventory_slots(0), 5);
}

/* ci indexing: index 3 must read runtime_char[3] (offset 3*0x50), independent
 * of the neighbours. char 0 all-set (would be 0), char 3 has 2 active slots. */
static void test_count_usable_ci_indexing(void)
{
    int s;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (s = 0; s < 8; s++) {
        g_test_rc_array[0].inventory_slots[s * 2] = 0x80;   /* char 0: none */
        g_test_rc_array[3].inventory_slots[s * 2] = 0x80;   /* char 3: start none */
    }
    g_test_rc_array[3].inventory_slots[2 * 2] = 0x00;   /* char 3 slot 2 active */
    g_test_rc_array[3].inventory_slots[6 * 2] = 0x00;   /* char 3 slot 6 active */
    ASSERT_EQ(fd2_count_usable_inventory_slots(3), 2);
    ASSERT_EQ(fd2_count_usable_inventory_slots(0), 0);
}

/* ----------------------------------------------------------------
 * fd2_remove_inventory_slot_at @ 0x1B8E7
 *
 * Removes inventory slot `slot` of runtime_char[char_idx] by shifting
 * slots[slot+1 .. 7] down over slots[slot .. 6] ((7-slot)*2 bytes via
 * memmove), then stamping slot[7].flag (inventory_slots[14]) = 0x80 vacant.
 * Each slot is 2 bytes: [i*2]=flag, [i*2+1]=item_id. These tests pin the
 * shift direction, the (7-slot)*2 byte count (incl. the slot==7 zero-copy
 * boundary), the flag/item_id 2-byte pairing, the always-vacate of slot 7,
 * and char_idx indexing into the 0x50-stride array.
 * ---------------------------------------------------------------- */

/* Fill char `ci`'s 8 slots with a recognizable pattern:
 * slot i -> flag=0x10+i, item_id=0x20+i. */
static void seed_inventory(int ci)
{
    int i;
    for (i = 0; i < 8; i++) {
        g_test_rc_array[ci].inventory_slots[i * 2]     = (uint8)(0x10 + i);
        g_test_rc_array[ci].inventory_slots[i * 2 + 1] = (uint8)(0x20 + i);
    }
}

/* Remove a MIDDLE slot (3): slots 0..2 stay, slots 4..7 shift into 3..6,
 * slot 7 becomes vacant. Verifies both flag and item_id bytes shift as a
 * pair, and that the removed slot's old contents are overwritten. */
static void test_remove_slot_middle_shifts_and_vacates(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    seed_inventory(0);

    fd2_remove_inventory_slot_at(0, 3);

    /* slots 0..2 untouched */
    for (i = 0; i < 3; i++) {
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[i * 2],     0x10 + i);
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[i * 2 + 1], 0x20 + i);
    }
    /* slots 3..6 now hold what was in 4..7 (shifted down by one) */
    for (i = 3; i < 7; i++) {
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[i * 2],     0x10 + (i + 1));
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[i * 2 + 1], 0x20 + (i + 1));
    }
    /* slot 7 vacated: flag=0x80; item_id left as old slot 7's id (only the
     * flag byte at +0x18 is written by the binary). */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x80);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[15], 0x27);
}

/* Remove slot 0: full-length shift (count=(7-0)*2=14). Every slot 1..7
 * moves down into 0..6; slot 7 vacated. Catches an off-by-one in the count. */
static void test_remove_slot_zero_full_shift(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    seed_inventory(0);

    fd2_remove_inventory_slot_at(0, 0);

    for (i = 0; i < 7; i++) {
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[i * 2],     0x10 + (i + 1));
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[i * 2 + 1], 0x20 + (i + 1));
    }
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x80);   /* slot 7 vacant */
}

/* Remove slot 7 (boundary): count=(7-7)*2=0, memmove copies nothing, so
 * slots 0..6 are byte-for-byte unchanged; only slot 7's flag is stamped
 * 0x80. A wrong count formula (e.g. (8-slot)*2 or signed underflow) would
 * corrupt slot 6 or read past the array; this pins it. */
static void test_remove_slot_seven_only_vacates(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    seed_inventory(0);

    fd2_remove_inventory_slot_at(0, 7);

    for (i = 0; i < 7; i++) {
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[i * 2],     0x10 + i);
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[i * 2 + 1], 0x20 + i);
    }
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x80);   /* flag stamped */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[15], 0x27);   /* item_id kept */
}

/* char_idx indexing: operating on char 2 must not disturb its neighbours
 * (chars 1 and 3), proving the 0x50-stride base offset. */
static void test_remove_slot_char_index_isolation(void)
{
    int i;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    seed_inventory(1);
    seed_inventory(2);
    seed_inventory(3);

    fd2_remove_inventory_slot_at(2, 0);

    /* char 2 shifted (slot 0 removed) */
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[0],  0x11);   /* was slot1 flag */
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[1],  0x21);   /* was slot1 id   */
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[14], 0x80);   /* slot7 vacant   */
    /* neighbours char 1 and char 3 fully intact */
    for (i = 0; i < 8; i++) {
        ASSERT_EQ(g_test_rc_array[1].inventory_slots[i * 2],     0x10 + i);
        ASSERT_EQ(g_test_rc_array[1].inventory_slots[i * 2 + 1], 0x20 + i);
        ASSERT_EQ(g_test_rc_array[3].inventory_slots[i * 2],     0x10 + i);
        ASSERT_EQ(g_test_rc_array[3].inventory_slots[i * 2 + 1], 0x20 + i);
    }
}

/* ----------------------------------------------------------------
 * fd2_add_item_to_inventory @ 0x1BB8C
 *
 * Adds item_id into the FIRST empty inventory slot of runtime_char[char_idx]
 * (8 slots, 2 bytes each: [i*2]=flag, [i*2+1]=item_id). A slot is empty when
 * its flag byte has bit 0x80 SET. The slot found is stamped flag=0 (occupied,
 * not equipped) and item_id byte = (uint8)item_id; returns 1. If all 8 slots
 * are full, returns -1 and writes nothing. These tests pin: the first-empty
 * selection (lowest index wins), the 0x80-SET empty polarity (opposite of the
 * count-usable 0x80-clear), flag stamped to 0 (not 0x40), item_id stored as
 * the LOW byte only, the all-full -1 with no mutation, and char_idx indexing
 * into the 0x50-stride array (g_test_rc_array, wired to the runtime-char ptr).
 * ---------------------------------------------------------------- */

/* All 8 slots empty (flag 0x80) -> item lands in slot 0; flag 0, id stored. */
static void test_add_item_first_empty_is_slot0(void)
{
    int s;
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (s = 0; s < 8; s++) {
        g_test_rc_array[0].inventory_slots[s * 2]     = 0x80;   /* empty */
        g_test_rc_array[0].inventory_slots[s * 2 + 1] = 0xAA;   /* sentinel id */
    }
    r = fd2_add_item_to_inventory(0, 0x42);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[0], 0x00);     /* occupied  */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[1], 0x42);     /* item id   */
    /* slots 1..7 untouched (still empty, sentinel id intact) */
    for (s = 1; s < 8; s++) {
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[s * 2],     0x80);
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[s * 2 + 1], 0xAA);
    }
}

/* Slots 0..2 occupied (flag 0, not 0x80), slot 3 first empty -> item lands in
 * slot 3; the occupied slots ahead of it are NOT overwritten. Pins the
 * first-empty scan starting from index 0 and skipping occupied slots. Also
 * confirms an equipped slot (flag 0x40, bit 0x80 clear) counts as occupied
 * and is skipped, not treated as empty. */
static void test_add_item_skips_occupied_to_first_empty(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].inventory_slots[0] = 0x00;   /* slot0 occupied        */
    g_test_rc_array[0].inventory_slots[1] = 0x11;
    g_test_rc_array[0].inventory_slots[2] = 0x40;   /* slot1 equipped (occ.) */
    g_test_rc_array[0].inventory_slots[3] = 0x12;
    g_test_rc_array[0].inventory_slots[4] = 0x00;   /* slot2 occupied        */
    g_test_rc_array[0].inventory_slots[5] = 0x13;
    g_test_rc_array[0].inventory_slots[6] = 0x80;   /* slot3 EMPTY (target)  */
    g_test_rc_array[0].inventory_slots[7] = 0x99;
    g_test_rc_array[0].inventory_slots[8] = 0x80;   /* slot4 also empty      */
    g_test_rc_array[0].inventory_slots[9] = 0x99;
    r = fd2_add_item_to_inventory(0, 0x55);
    ASSERT_EQ((long)r, 1);
    /* slot3 filled */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[6], 0x00);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[7], 0x55);
    /* slots 0..2 untouched */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[0], 0x00);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[1], 0x11);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[2], 0x40);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[3], 0x12);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[4], 0x00);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[5], 0x13);
    /* slot4 (later empty) left alone */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[8], 0x80);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[9], 0x99);
}

/* Only slot 7 empty -> item lands in slot 7 (last-slot boundary), returns 1. */
static void test_add_item_only_last_slot_empty(void)
{
    int s;
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (s = 0; s < 7; s++) {
        g_test_rc_array[0].inventory_slots[s * 2] = 0x00;   /* occupied */
    }
    g_test_rc_array[0].inventory_slots[7 * 2] = 0x80;       /* slot7 empty */
    r = fd2_add_item_to_inventory(0, 0x77);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[14], 0x00);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[15], 0x77);
}

/* All 8 slots full (no 0x80 anywhere) -> returns -1 and mutates nothing.
 * Mix of occupied (0) and equipped (0x40) flags, none empty. */
static void test_add_item_all_full_returns_minus1(void)
{
    int s;
    int r;
    uint8 before[16];
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (s = 0; s < 8; s++) {
        g_test_rc_array[0].inventory_slots[s * 2]     = (s & 1) ? 0x40 : 0x00;
        g_test_rc_array[0].inventory_slots[s * 2 + 1] = (uint8)(0x30 + s);
    }
    memcpy(before, g_test_rc_array[0].inventory_slots, 16);
    r = fd2_add_item_to_inventory(0, 0x42);
    ASSERT_EQ((long)r, -1);
    /* inventory byte-for-byte unchanged */
    ASSERT_EQ(memcmp(before, g_test_rc_array[0].inventory_slots, 16), 0);
}

/* item_id stored as LOW byte only: pass 0x1234, the slot id byte must be 0x34
 * (MOV DL,[ESP+0xc]; MOV [EAX+1],DL — only DL is written). */
static void test_add_item_stores_low_byte_only(void)
{
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].inventory_slots[0] = 0x80;          /* slot0 empty */
    r = fd2_add_item_to_inventory(0, 0x1234);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[1], 0x34);
}

/* char_idx indexing: adding to char 4 must hit runtime_char[4] (offset 4*0x50)
 * and leave its neighbours (chars 3 and 5) untouched. Char 4 slot 0 empty. */
static void test_add_item_char_index_isolation(void)
{
    int s;
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* chars 3 and 5: all slots empty with a sentinel id, must stay intact */
    for (s = 0; s < 8; s++) {
        g_test_rc_array[3].inventory_slots[s * 2]     = 0x80;
        g_test_rc_array[3].inventory_slots[s * 2 + 1] = 0xC3;
        g_test_rc_array[5].inventory_slots[s * 2]     = 0x80;
        g_test_rc_array[5].inventory_slots[s * 2 + 1] = 0xC5;
        g_test_rc_array[4].inventory_slots[s * 2]     = 0x80;
    }
    r = fd2_add_item_to_inventory(4, 0x66);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ(g_test_rc_array[4].inventory_slots[0], 0x00);
    ASSERT_EQ(g_test_rc_array[4].inventory_slots[1], 0x66);
    /* neighbours untouched */
    for (s = 0; s < 8; s++) {
        ASSERT_EQ(g_test_rc_array[3].inventory_slots[s * 2],     0x80);
        ASSERT_EQ(g_test_rc_array[3].inventory_slots[s * 2 + 1], 0xC3);
        ASSERT_EQ(g_test_rc_array[5].inventory_slots[s * 2],     0x80);
        ASSERT_EQ(g_test_rc_array[5].inventory_slots[s * 2 + 1], 0xC5);
    }
}

/* ----------------------------------------------------------------
 * fd2_inventory_selection_modal_dispatch @ 0x1B932 — input-loop tests deferred.
 *
 * The modal is do { r = fd2_inventory_grid_input_step(...); } while (r == 0),
 * preceded by fd2_open_status_screen_with_slide_in(char_idx) and followed by a
 * 12-frame outro; it returns (r != -1) as a 0/1 boolean. Now that the real
 * fd2_inventory_grid_input_step is emitted (below) the loop is end-to-end real:
 * the grid-input step calls the real fd2_wait_for_input_dialog_with_blink, which
 * busy-waits on the BIOS keyboard buffer. fd2_open_status_screen_with_slide_in
 * ends by calling fd2_clear_keyboard_buffer() (BIOS_KBD_TAIL = BIOS_KBD_HEAD),
 * so any scancode pre-armed before the call is wiped before the first poll — and
 * the host harness has no async key source to refill the buffer mid-loop.
 *
 * The modal's loop-termination / boolean-return logic therefore needs real
 * keyboard input to release the wait and is deferred to Phase 9 integration
 * (the same deferral the codebase applies to every input-loop-released path).
 * The previous tests here drove a fake fd2_inventory_grid_input_step (now
 * removed); the real grid-input step's full dispatch — including the return
 * values 0 / 1 / -1 that the modal's loop and SETNZ tail consume — is covered
 * directly and deterministically by the test_grid_input_* cases below (which
 * call it directly and inject the scancode the wait reads, with no intervening
 * buffer clear).
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * fd2_inventory_grid_input_step @ 0x1B9DE
 *
 * One frame of inventory-grid selection input: redraw the grid, count active
 * slots, wait for a key (real fd2_wait_for_input_dialog_with_blink, driven by
 * BIOS-buffer scancode injection — the grid-input step does NOT clear the
 * buffer, so a pre-armed scancode survives to the wait's INT 16h read), and
 * dispatch. These tests pin the cursor navigation arithmetic (the high-value,
 * EAX-tracking-prone logic) for every branch: Up/Down with wrap, Left/Right with
 * their row/active_count bounds, the Enter/Space commit with the gate_flag
 * item-usability deref, Esc cancel, and the unhandled-key loop-again.
 * data_fd2_ui_menu_cursor_idx @ 0x53C57 holds the cursor; the SFX callee is a
 * no-op recording fake (testglob.c).
 *
 * Fixture: g_grid_sheet (zeroed offset table -> every sprite resolves to
 * sheet+0) and g_grid_text (immediate-END page program) keep the REAL grid
 * renderer in-bounds for active slots; the item-effect table is zeroed and read
 * by the real fd2_get_item_effect_entry. Writes to 0xA0000 hit the VGA aperture
 * (harmless under DOS/4GW, same convention as the sibling status/gfx tests).
 * ---------------------------------------------------------------- */
static uint8  g_grid_sheet[4096];
static uint16 g_grid_text[0x400];

/* Stand up the render fixture and make exactly n_active of char 0's 8 inventory
 * slots active (flag bit 0x80 clear, item_id 0); the rest are empty (0x80 set).
 * Leaves data_fd2_ui_menu_cursor_idx untouched (each test sets it). */
static void grid_setup_active(int n_active)
{
    int i;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (i = 0; i < 8; i++) {
        g_test_rc_array[0].inventory_slots[i * 2]     = (i < n_active)
                                                            ? 0x00 : 0x80;
        g_test_rc_array[0].inventory_slots[i * 2 + 1] = 0;   /* item id 0 */
    }
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(item_effect) * 215);

    memset(g_grid_sheet, 0, sizeof(g_grid_sheet));
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_grid_sheet;

    for (i = 0; i < 0x400; i++) {
        g_grid_text[i] = 0x600;
    }
    *(int16 *)((uint8 *)g_grid_text + 0x600) = -1;
    data_fd2_all_game_text_ptr = (uint32)g_grid_text;

    data_fd2_audio_fdother_sfx_bank_buf_ptr = (uint32)g_grid_sheet;
}

/* Up, no wrap: cursor 3 -> 2 (decrement), returns 0. */
static void test_grid_input_up_decrement(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 3;
    kbd_inject_scancode(0x48);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ((long)r, 0);
}

/* Up, wrap: cursor 0 with active_count 5 -> active_count-1 = 4, returns 0.
 * Pins the wrap target = active_count - 1. */
static void test_grid_input_up_wrap_to_last(void)
{
    int r;
    grid_setup_active(5);
    data_fd2_ui_menu_cursor_idx = 0;
    kbd_inject_scancode(0x48);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 4);   /* 5 - 1 */
    ASSERT_EQ((long)r, 0);
}

/* Down, no wrap: cursor 2 (active_count 8, last=7) -> 3, returns 0. */
static void test_grid_input_down_increment(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 2;
    kbd_inject_scancode(0x50);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 3);
    ASSERT_EQ((long)r, 0);
}

/* Down, wrap: cursor at last (active_count-1 = 4 with active_count 5) -> 0,
 * returns 0. Pins the wrap condition (cursor == active_count - 1). */
static void test_grid_input_down_wrap_to_zero(void)
{
    int r;
    grid_setup_active(5);
    data_fd2_ui_menu_cursor_idx = 4;       /* == active_count - 1 */
    kbd_inject_scancode(0x50);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
    ASSERT_EQ((long)r, 0);
}

/* Left, valid: cursor 5 (>= 4) -> 1 (cursor - 4), returns 0. Jump up a row. */
static void test_grid_input_left_valid(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 5;
    kbd_inject_scancode(0x4b);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);   /* 5 - 4 */
    ASSERT_EQ((long)r, 0);
}

/* Left, invalid: cursor 2 (< 4) -> unchanged, returns 0 (top row, no move). */
static void test_grid_input_left_invalid_top_row(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 2;
    kbd_inject_scancode(0x4b);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);   /* no change */
    ASSERT_EQ((long)r, 0);
}

/* Right, valid: cursor 1 (< 4 and < active_count-4 = 4) -> 5, returns 0. */
static void test_grid_input_right_valid(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 1;
    kbd_inject_scancode(0x4d);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 5);   /* 1 + 4 */
    ASSERT_EQ((long)r, 0);
}

/* Right, invalid (bottom row): cursor 4 (not < 4) -> unchanged, returns 0. */
static void test_grid_input_right_invalid_bottom_row(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 4;
    kbd_inject_scancode(0x4d);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 4);   /* no change */
    ASSERT_EQ((long)r, 0);
}

/* Right, invalid (no slot below): cursor 1 but active_count 5 (active_count-4=1,
 * cursor not < 1) -> unchanged, returns 0. Pins the active_count-4 bound that
 * stops the cursor moving onto an empty cell below. */
static void test_grid_input_right_invalid_no_slot_below(void)
{
    int r;
    grid_setup_active(5);
    data_fd2_ui_menu_cursor_idx = 1;       /* not < (5 - 4) = 1 */
    kbd_inject_scancode(0x4d);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);   /* no change */
    ASSERT_EQ((long)r, 0);
}

/* Enter, gate_flag 0: commit immediately (no item-usability check), returns 1.
 * Cursor unchanged. */
static void test_grid_input_enter_gate0_commits(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 3;
    kbd_inject_scancode(0x1c);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 3);   /* unchanged */
}

/* Space (0x39), gate_flag 0: same commit path as Enter, returns 1. */
static void test_grid_input_space_gate0_commits(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 0;
    kbd_inject_scancode(0x39);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)r, 1);
}

/* Enter, gate_flag 1, item IS usable: the selected slot's item has use_effect
 * != 0, so the gate passes and the step commits (returns 1).
 * fd2_get_item_effect_entry returns &entry.type (table+1), so the byte read at
 * +0xD is the .use_effect field (item_effect offset +0xE). item_id at slot 0 is
 * 7; set entry 7's use_effect nonzero. */
static void test_grid_input_enter_gate1_usable_commits(void)
{
    int r;
    grid_setup_active(8);
    g_test_rc_array[0].inventory_slots[1] = 7;          /* slot 0 item id = 7 */
    data_fd2_battle_item_effect_table[7].use_effect = 0x05;   /* usable */
    data_fd2_ui_menu_cursor_idx = 0;
    kbd_inject_scancode(0x1c);
    r = fd2_inventory_grid_input_step(0, 1);
    ASSERT_EQ((long)r, 1);
}

/* Enter, gate_flag 1, item NOT usable: use_effect == 0, so the gate fails and
 * the step returns 0 (re-prompt). Pins the gate's CALL -> [EAX+0xD] deref. */
static void test_grid_input_enter_gate1_unusable_reprompts(void)
{
    int r;
    grid_setup_active(8);
    g_test_rc_array[0].inventory_slots[1] = 9;          /* slot 0 item id = 9 */
    data_fd2_battle_item_effect_table[9].use_effect = 0;     /* NOT usable */
    data_fd2_ui_menu_cursor_idx = 0;
    kbd_inject_scancode(0x1c);
    r = fd2_inventory_grid_input_step(0, 1);
    ASSERT_EQ((long)r, 0);
}

/* Esc (0x01): cancel, returns -1. */
static void test_grid_input_esc_cancels(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 2;
    kbd_inject_scancode(0x01);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)r, -1);
}

/* Unhandled key (0x10): no branch matches, returns 0 (loop again), cursor
 * unchanged. */
static void test_grid_input_other_key_loops(void)
{
    int r;
    grid_setup_active(8);
    data_fd2_ui_menu_cursor_idx = 6;
    kbd_inject_scancode(0x10);
    r = fd2_inventory_grid_input_step(0, 0);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 6);   /* unchanged */
}

/* ----------------------------------------------------------------
 * fd2_item_command_menu_dispatch @ 0x1BBDC — no-items early-out.
 *
 * The 4-way item command popup (Use/Give/Sort/Drop) begins by copying its two
 * 4-int templates into locals and then gates on
 * fd2_count_usable_inventory_slots(char_idx): when the character has zero
 * usable inventory slots it returns -1 immediately, BEFORE opening the
 * settings dialog or entering any keyboard input loop. That makes this path
 * the only deterministic, host-isolable branch of the function: it exercises
 * the template-copy prologue and the real count-usable gate with no async
 * keyboard dependency.
 *
 * The four dispatch branches (Use / Give / Sort / Drop) and the menu-cancel
 * early-out all run through fd2_open_settings_dialog_with_slide + the real
 * settings-menu input loop (and, beyond it, modal item-selection and
 * action-target input loops) which busy-wait on the BIOS keyboard buffer with
 * no in-process key source; their behavioral coverage is deferred to Phase 9
 * integration under the emulator (the same deferral applied to the sibling
 * fd2_inventory_selection_modal_dispatch input loop above).
 *
 * Wiring: g_test_rc_array[0]'s eight inventory slots all carry flag bit 0x80
 * (vacant), so the real fd2_count_usable_inventory_slots returns 0 and the
 * dispatcher returns -1 without any display/input side effect.
 * ---------------------------------------------------------------- */
static void test_item_command_no_items_returns_minus1(void)
{
    int s;
    int r;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    for (s = 0; s < 8; s++) {
        g_test_rc_array[0].inventory_slots[s * 2] = 0x80;   /* all vacant */
    }
    r = fd2_item_command_menu_dispatch(0);
    ASSERT_EQ((long)r, -1);
}

/* ----------------------------------------------------------------
 * fd2_equip_unequip_inventory_menu @ 0x1BFFE — input-loop deferred to Phase 9.
 *
 * The EQUIP/UNEQUIP modal is structurally identical to the sibling
 * fd2_inventory_selection_modal_dispatch above: it opens the status screen
 * (fd2_open_status_screen_with_slide_in, whose tail calls
 * fd2_clear_keyboard_buffer) and then runs
 * do { r = fd2_inventory_grid_input_step(char_idx, 0); } while (r == 0)
 * inside an outer while(1). The ONLY exits from the outer loop are
 * break-on-Esc (r == -1) and break-on-no-usable-slots; reaching either
 * first requires the inner do-while to release, which requires the real
 * fd2_inventory_grid_input_step -> fd2_wait_for_input_dialog_with_blink to
 * read a scancode AFTER the open-screen buffer clear. The host harness has
 * no async key source to refill the BIOS keyboard buffer mid-loop, so no
 * path through this function terminates in-process — it cannot be driven to
 * completion as a unit test, the same hard blocker (and same deferral)
 * documented for fd2_inventory_selection_modal_dispatch.
 *
 * Coverage of its constituent decisions IS deterministic and already in
 * place: the grid-input return values 0 / 1 / -1 that the loop and the
 * input == -1 break consume are pinned by the test_grid_input_* cases; the
 * no-usable-slots break gate (fd2_count_usable_inventory_slots == 0) is
 * pinned by the test_count_usable_* cases; and the equip-decision callees
 * it invokes on commit (fd2_check_job_can_equip_item, fd2_equip_item_in_slot)
 * are pure inventory primitives verified directly on their own emit turns.
 * Only the end-to-end modal loop + the equip-branch display refresh remain
 * for Phase 9 integration under the emulator.
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * fd2_equip_item_in_slot @ 0x1C142
 *
 * Equips slot `slot_idx`, auto-unequipping any same-category item already
 * equipped (flag bit 0x40). Category split is item_id 0x80 (both < 0x80 =
 * physical, both >= 0x80 = magical). The target item_id is fetched via the
 * real fd2_get_inventory_slot_item_id, which reads
 * g_test_rc_array[ci].inventory_slots[slot*2 + 1]. These tests pin: the
 * same-category unequip (physical & magical), cross-category preservation,
 * the 0x7F/0x80 boundary, the equipped-flag (0x40) gate, and char_idx
 * (0x50-stride) isolation.
 * ---------------------------------------------------------------- */

/* Equip a physical item while another physical item is already equipped:
 * the old physical slot must be unequipped and the target marked equipped. */
static void test_equip_physical_unequips_other_physical(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* slot 0: equipped physical weapon (id 0x05) */
    g_test_rc_array[0].inventory_slots[0] = 0x40;
    g_test_rc_array[0].inventory_slots[1] = 0x05;
    /* slot 2: the target slot, holds another physical item (id 0x10) */
    g_test_rc_array[0].inventory_slots[4] = 0x00;
    g_test_rc_array[0].inventory_slots[5] = 0x10;

    fd2_equip_item_in_slot(0, 2);

    ASSERT_EQ(g_test_rc_array[0].inventory_slots[0], 0x00);  /* old unequipped */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[4], 0x40);  /* target equipped */
}

/* Equip a physical item while a magical item (id >= 0x80) is equipped:
 * the cross-category magical slot must stay equipped. */
static void test_equip_physical_keeps_equipped_magical(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* slot 0: equipped magical spellbook (id 0x90) */
    g_test_rc_array[0].inventory_slots[0] = 0x40;
    g_test_rc_array[0].inventory_slots[1] = 0x90;
    /* slot 1: target physical item (id 0x20) */
    g_test_rc_array[0].inventory_slots[2] = 0x00;
    g_test_rc_array[0].inventory_slots[3] = 0x20;

    fd2_equip_item_in_slot(0, 1);

    ASSERT_EQ(g_test_rc_array[0].inventory_slots[0], 0x40);  /* magical kept */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[2], 0x40);  /* target equipped */
}

/* Equip a magical item while another magical is equipped AND a physical is
 * equipped: only the same-category magical is unequipped; physical kept. */
static void test_equip_magical_unequips_magical_keeps_physical(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* slot 0: equipped magical (id 0x85) -> must be unequipped */
    g_test_rc_array[0].inventory_slots[0] = 0x40;
    g_test_rc_array[0].inventory_slots[1] = 0x85;
    /* slot 1: equipped physical (id 0x03) -> must be kept */
    g_test_rc_array[0].inventory_slots[2] = 0x40;
    g_test_rc_array[0].inventory_slots[3] = 0x03;
    /* slot 3: target magical item (id 0xC0) */
    g_test_rc_array[0].inventory_slots[6] = 0x00;
    g_test_rc_array[0].inventory_slots[7] = 0xC0;

    fd2_equip_item_in_slot(0, 3);

    ASSERT_EQ(g_test_rc_array[0].inventory_slots[0], 0x00);  /* magical removed */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[2], 0x40);  /* physical kept   */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[6], 0x40);  /* target equipped */
}

/* Category boundary: id 0x7F is physical, id 0x80 is magical. Equipping a
 * 0x7F item must NOT unequip an equipped 0x80 item (different categories). */
static void test_equip_category_boundary_7f_vs_80(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* slot 0: equipped item id exactly 0x80 (magical) */
    g_test_rc_array[0].inventory_slots[0] = 0x40;
    g_test_rc_array[0].inventory_slots[1] = 0x80;
    /* slot 1: target item id exactly 0x7F (physical) */
    g_test_rc_array[0].inventory_slots[2] = 0x00;
    g_test_rc_array[0].inventory_slots[3] = 0x7F;

    fd2_equip_item_in_slot(0, 1);

    ASSERT_EQ(g_test_rc_array[0].inventory_slots[0], 0x40);  /* 0x80 kept */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[2], 0x40);  /* target equipped */
}

/* The unequip scan only touches slots with the 0x40 flag set. An occupied-
 * but-unequipped same-category slot (flag 0x00) must be left untouched. */
static void test_equip_ignores_unequipped_same_category(void)
{
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* slot 0: same-category physical but NOT equipped (flag 0x00) */
    g_test_rc_array[0].inventory_slots[0] = 0x00;
    g_test_rc_array[0].inventory_slots[1] = 0x05;
    /* slot 2: target physical item */
    g_test_rc_array[0].inventory_slots[4] = 0x00;
    g_test_rc_array[0].inventory_slots[5] = 0x10;

    fd2_equip_item_in_slot(0, 2);

    ASSERT_EQ(g_test_rc_array[0].inventory_slots[0], 0x00);  /* unchanged */
    ASSERT_EQ(g_test_rc_array[0].inventory_slots[4], 0x40);  /* target equipped */
}

/* char_idx indexing: operating on char 4 must hit runtime_char[4] (offset
 * 4*0x50) and leave other characters' slots completely untouched. */
static void test_equip_char_index_isolation(void)
{
    int s;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    /* char 3: an equipped physical that must NOT be disturbed */
    g_test_rc_array[3].inventory_slots[0] = 0x40;
    g_test_rc_array[3].inventory_slots[1] = 0x05;
    /* char 4: an equipped physical (slot 0) + target physical (slot 1) */
    g_test_rc_array[4].inventory_slots[0] = 0x40;
    g_test_rc_array[4].inventory_slots[1] = 0x07;
    g_test_rc_array[4].inventory_slots[2] = 0x00;
    g_test_rc_array[4].inventory_slots[3] = 0x11;

    fd2_equip_item_in_slot(4, 1);

    /* char 4: old physical unequipped, target equipped */
    ASSERT_EQ(g_test_rc_array[4].inventory_slots[0], 0x00);
    ASSERT_EQ(g_test_rc_array[4].inventory_slots[2], 0x40);
    /* char 3 untouched */
    ASSERT_EQ(g_test_rc_array[3].inventory_slots[0], 0x40);
    ASSERT_EQ(g_test_rc_array[3].inventory_slots[1], 0x05);
    /* every other char still all-zero */
    for (s = 0; s < 16; s++) {
        ASSERT_EQ(g_test_rc_array[0].inventory_slots[s], 0x00);
        ASSERT_EQ(g_test_rc_array[5].inventory_slots[s], 0x00);
    }
}

void run_ui_menu_status_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: ui_menu/status\n");
    RUN_TEST(test_stat_preview_basic);
    RUN_TEST(test_stat_preview_weapon_opposite_and_same_category);
    RUN_TEST(test_stat_preview_armor_branch_and_flag_gate);
    RUN_TEST(test_stat_preview_signed_negative_bonus);
    RUN_TEST(test_close_status_screen_slide_out_runs_full_teardown);
    RUN_TEST(test_open_party_overview_runs_and_returns);
    RUN_TEST(test_count_usable_all_clear);
    RUN_TEST(test_count_usable_all_set);
    RUN_TEST(test_count_usable_mixed_and_stride);
    RUN_TEST(test_count_usable_ci_indexing);
    RUN_TEST(test_remove_slot_middle_shifts_and_vacates);
    RUN_TEST(test_remove_slot_zero_full_shift);
    RUN_TEST(test_remove_slot_seven_only_vacates);
    RUN_TEST(test_remove_slot_char_index_isolation);
    RUN_TEST(test_add_item_first_empty_is_slot0);
    RUN_TEST(test_add_item_skips_occupied_to_first_empty);
    RUN_TEST(test_add_item_only_last_slot_empty);
    RUN_TEST(test_add_item_all_full_returns_minus1);
    RUN_TEST(test_add_item_stores_low_byte_only);
    RUN_TEST(test_add_item_char_index_isolation);
    RUN_TEST(test_grid_input_up_decrement);
    RUN_TEST(test_grid_input_up_wrap_to_last);
    RUN_TEST(test_grid_input_down_increment);
    RUN_TEST(test_grid_input_down_wrap_to_zero);
    RUN_TEST(test_grid_input_left_valid);
    RUN_TEST(test_grid_input_left_invalid_top_row);
    RUN_TEST(test_grid_input_right_valid);
    RUN_TEST(test_grid_input_right_invalid_bottom_row);
    RUN_TEST(test_grid_input_right_invalid_no_slot_below);
    RUN_TEST(test_grid_input_enter_gate0_commits);
    RUN_TEST(test_grid_input_space_gate0_commits);
    RUN_TEST(test_grid_input_enter_gate1_usable_commits);
    RUN_TEST(test_grid_input_enter_gate1_unusable_reprompts);
    RUN_TEST(test_grid_input_esc_cancels);
    RUN_TEST(test_grid_input_other_key_loops);
    RUN_TEST(test_item_command_no_items_returns_minus1);
    RUN_TEST(test_equip_physical_unequips_other_physical);
    RUN_TEST(test_equip_physical_keeps_equipped_magical);
    RUN_TEST(test_equip_magical_unequips_magical_keeps_physical);
    RUN_TEST(test_equip_category_boundary_7f_vs_80);
    RUN_TEST(test_equip_ignores_unequipped_same_category);
    RUN_TEST(test_equip_char_index_isolation);
    printf("\n");
}
