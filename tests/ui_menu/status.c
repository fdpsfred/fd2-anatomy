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
/* controllable fake for the not-yet-emitted fd2_inventory_grid_input_step
 * (testglob.c): drives the modal dispatcher's input loop with a programmed
 * return sequence. */
extern int g_grid_input_seq[8];
extern int g_grid_input_seq_len;
extern int g_grid_input_calls;
/* data_fd2_ui_slide_* workspace ptr globals are declared in globals.h */


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
 * fd2_inventory_selection_modal_dispatch @ 0x1B932
 *
 * The modal is a thin orchestrator around three pieces:
 *   1. fd2_open_status_screen_with_slide_in(char_idx)  — REAL display setup
 *      (allocates the three 64000-byte workspaces, loads the real DATO.DAT
 *      portrait, renders the static panel + inventory grid, runs a 12-frame
 *      slide-in). This is the Phase-9 display path and is exercised here only
 *      to the extent of "it runs end-to-end without faulting".
 *   2. a do { r = fd2_inventory_grid_input_step(...); } while (r == 0) loop.
 *   3. a 12-frame outro + VGA restore + three free()s, then
 *      return (r != -1) as a 0/1 boolean.
 *
 * The unique, testable logic is the loop termination and the boolean return
 * (an EAX-tracking-bug-prone "use the CALL's EAX result" point: the binary
 * does MOV ESI,EAX / ... / CMP ESI,-1 / SETNZ). We drive that deterministically
 * by faking only the not-yet-emitted fd2_inventory_grid_input_step (testglob.c)
 * with a programmed return sequence, and assert: (a) the dispatcher returns at
 * all (proving the loop's exit condition is wired to the input result — an
 * EAX regression would spin forever and trip the harness hang detector);
 * (b) the loop iterated exactly len(sequence) times; (c) the boolean return is
 * 1 for a confirmed slot (terminal != -1) and 0 for Esc cancel (terminal -1).
 *
 * Fixture (mirrors test_open_party_overview_runs_and_returns): a zeroed sprite
 * sheet whose offset table resolves every sprite to sheet+0, an immediate-END
 * text program so the panel's three real fd2_display_dialog_scene labels return
 * at once (no DATO text fopen, no input wait), and char_idx's portrait_id = 0
 * so the REAL fd2_load_dat_resource reads resource 0 of the staged real
 * DATO.DAT (always a valid index). The three workspaces are malloc'd inside
 * open and free()d inside the dispatcher, so the test must not pre-allocate or
 * re-free them; the globals are reset to 0 afterward to drop the dangling ptrs.
 * Writes to 0xA0000 hit the VGA aperture (harmless under DOS/4GW, same
 * convention as the sibling status/gfx tests).
 * ---------------------------------------------------------------- */
static uint8  g_modal_sheet[4096];
static uint16 g_modal_text[0x400];

static void modal_setup_render_fixture(uint32 char_idx)
{
    int i;

    /* runtime char: zero, valid portrait, all inventory slots empty (flag
     * bit 0x80) so the grid renderer draws nothing and never touches text. */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[char_idx].portrait_id = 0;     /* DATO resource 0 */
    for (i = 0; i < 8; i++) {
        g_test_rc_array[char_idx].inventory_slots[i * 2] = 0x80;
    }
    memset(data_fd2_battle_item_effect_table, 0,
           sizeof(item_effect) * 215);

    /* sprite sheet: zeroed offset table -> every sprite resolves to sheet+0. */
    memset(g_modal_sheet, 0, sizeof(g_modal_sheet));
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)g_modal_sheet;

    /* immediate-END text program: a -1 opcode parked high, every page word
     * pointing at it so the panel's three real dialog labels return at once. */
    for (i = 0; i < 0x400; i++) {
        g_modal_text[i] = 0x600;
    }
    *(int16 *)((uint8 *)g_modal_text + 0x600) = -1;
    data_fd2_all_game_text_ptr = (uint32)g_modal_text;

    /* loader free()s old_buf first; NULL it so that free is a no-op. */
    data_fd2_portrait_sprite_buffer = 0;
    /* sfx callee is a no-op recording fake; any non-zero handle is fine. */
    data_fd2_audio_fdother_sfx_bank_buf_ptr = (uint32)g_modal_sheet;

    /* open() malloc's all three workspaces; the dispatcher free()s them. */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
}

static void modal_teardown_render_fixture(void)
{
    /* the dispatcher already free()d all three workspaces; drop the dangling
     * globals so later suites never reuse a freed pointer. */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    /* the portrait buffer the real loader returned is leaked by the function
     * itself (no caller frees it); free it here to keep the test tidy. */
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
}

/* Confirm path: input step returns 0,0 (still in grid) then 3 (slot chosen);
 * the loop runs 3 times and the dispatcher returns 1 (3 != -1). */
static void test_inventory_modal_confirm_returns_true(void)
{
    int ret;

    modal_setup_render_fixture(0);
    g_grid_input_seq[0] = 0;
    g_grid_input_seq[1] = 0;
    g_grid_input_seq[2] = 3;        /* terminal: a confirmed slot index */
    g_grid_input_seq_len = 3;
    g_grid_input_calls = 0;

    ret = fd2_inventory_selection_modal_dispatch(0, 1);

    ASSERT_EQ((long)g_grid_input_calls, 3);   /* loop iterated 0,0,3 */
    ASSERT_EQ((long)ret, 1);                   /* 3 != -1 -> true */
    modal_teardown_render_fixture();
}

/* Cancel path: input step returns 0 (still in grid) then -1 (Esc); the loop
 * runs 2 times and the dispatcher returns 0 (-1 == -1). Also proves gate_flag
 * is forwarded unchanged (the fake ignores it, but the call must compile/run
 * with mode 0). */
static void test_inventory_modal_cancel_returns_false(void)
{
    int ret;

    modal_setup_render_fixture(0);
    g_grid_input_seq[0] = 0;
    g_grid_input_seq[1] = -1;       /* terminal: Esc cancel */
    g_grid_input_seq_len = 2;
    g_grid_input_calls = 0;

    ret = fd2_inventory_selection_modal_dispatch(0, 0);

    ASSERT_EQ((long)g_grid_input_calls, 2);   /* loop iterated 0,-1 */
    ASSERT_EQ((long)ret, 0);                   /* -1 == -1 -> false */
    modal_teardown_render_fixture();
}

/* Immediate confirm: the very first input poll returns a terminal slot (5),
 * so the loop body runs exactly once (do/while, not while) and returns true.
 * Pins the do-while semantics: the input step is always called at least once. */
static void test_inventory_modal_immediate_confirm_runs_once(void)
{
    int ret;

    modal_setup_render_fixture(1);
    g_grid_input_seq[0] = 5;        /* terminal on the first poll */
    g_grid_input_seq_len = 1;
    g_grid_input_calls = 0;

    ret = fd2_inventory_selection_modal_dispatch(1, 1);

    ASSERT_EQ((long)g_grid_input_calls, 1);   /* do-while: exactly one poll */
    ASSERT_EQ((long)ret, 1);                   /* 5 != -1 -> true */
    modal_teardown_render_fixture();
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
    RUN_TEST(test_inventory_modal_confirm_returns_true);
    RUN_TEST(test_inventory_modal_cancel_returns_false);
    RUN_TEST(test_inventory_modal_immediate_confirm_runs_once);
    printf("\n");
}
