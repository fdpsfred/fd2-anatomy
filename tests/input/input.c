/*
 * unit tests for src/input/input.c
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>

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
extern int    g_mini_panel_calls;
extern uint32 g_mini_panel_last_buf;
extern uint32 g_mini_panel_last_stride;
extern uint32 g_mini_panel_last_char;
extern int g_find_equipped_return;
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
extern int g_count_usable_slots_return;
extern uint8 g_spell_list_buf[12];
extern int g_remove_inventory_calls;
extern int g_cast_status_cure_calls;
extern int g_cast_status_via_d1b_calls;
extern int g_repaint_settings_calls;
extern int g_repaint_flip_buffer_after;


/* ---- Tests: keyboard / BIOS ---- */

static void test_read_bios_tick(void)
{
    uint16 t;
    t = fd2_read_bios_midnight_tick();
    ASSERT_TRUE(1);
}


static void test_kbd_buffer_empty(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    r = fd2_check_keyboard_buffer_nonempty();
    ASSERT_EQ(r, 0);
}


static void test_kbd_buffer_nonempty(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x22;
    r = fd2_check_keyboard_buffer_nonempty();
    ASSERT_NE(r, 0);
}


static void test_clear_kbd_buffer(void)
{
    *(volatile uint16 *)0x41AuL = 0x30;
    *(volatile uint16 *)0x41CuL = 0x40;
    fd2_clear_keyboard_buffer();
    ASSERT_EQ(*(volatile uint16 *)0x41CuL, 0x30);
}


/* ---- Tests: wait_one_bios_tick ---- */

static void test_wait_one_bios_tick_smoke(void)
{
    /* Deterministic: tick low word = 0x0100, cache preset to a
     * different value so the spin exits on the first compare. */
    *(volatile uint32 *)0x46CuL = 0x00000100uL;
    data_fd2_engine_wait_one_bios_tick_last_seen = 0x00000099uL;
    fd2_wait_one_bios_tick();
    ASSERT_EQ(data_fd2_engine_wait_one_bios_tick_last_seen,
              0x00000100uL);
}


/* Pins the SIGN-EXTENDED 16-bit read (asm MOVSX EAX,word ptr [0x46C]).
 * Tick dword 0x0001FFFF -> low word 0xFFFF -> sx16 -> 0xFFFFFFFF cached.
 * A full-32-bit read (the prior bug) would cache 0x0001FFFF instead.
 * Cache preset to 0 (!= 0xFFFFFFFF) so the spin exits on the first
 * compare; the assertion then discriminates the two widths. */
static void test_wait_one_bios_tick_sign_extend(void)
{
    *(volatile uint32 *)0x46CuL = 0x0001FFFFuL;
    data_fd2_engine_wait_one_bios_tick_last_seen = 0;
    fd2_wait_one_bios_tick();
    ASSERT_EQ(data_fd2_engine_wait_one_bios_tick_last_seen,
              0xFFFFFFFFuL);
}


/* Positive low word (high bit clear): 0x00007FFF sign-extends to
 * itself, and the upper tick word (0x0001) must be discarded.
 * Distinguishes the 16-bit read from a full-32-bit read once more,
 * and confirms the spin-compare matches on a re-read of the same
 * (sign-extended) value: cache preset equal -> would spin -> so we
 * preset NOT equal to guarantee exit, then verify the store width. */
static void test_wait_one_bios_tick_positive_word(void)
{
    *(volatile uint32 *)0x46CuL = 0x00017FFFuL;
    data_fd2_engine_wait_one_bios_tick_last_seen = 0;
    fd2_wait_one_bios_tick();
    ASSERT_EQ(data_fd2_engine_wait_one_bios_tick_last_seen,
              0x00007FFFuL);
}


/* ---- Tests: wait_n_bios_ticks ---- */

/* Pins the SIGN-EXTENDED 16-bit store width (asm MOVSX EAX,word ptr
 * [0x46C]; MOV [0x53A2C],EAX), identical to the wait_one_bios_tick
 * idiom. n_ticks=0 makes the spin exit on the first compare
 * (elapsed = sx16(tick)-sx16(tick) = 0, and 0 < 0 is false), so the
 * assertion isolates the store width. Tick dword 0x0001FFFF -> low
 * word 0xFFFF -> sx16 -> 0xFFFFFFFF; a full-32-bit store would leave
 * 0x0001FFFF instead. */
static void test_wait_n_bios_ticks_sign_extend(void)
{
    *(volatile uint32 *)0x46CuL = 0x0001FFFFuL;
    data_fd2_engine_wait_n_bios_ticks_last_seen = 0;
    fd2_wait_n_bios_ticks(0);
    ASSERT_EQ(data_fd2_engine_wait_n_bios_ticks_last_seen,
              0xFFFFFFFFuL);
}


/* Positive low word (high bit clear): 0x00007FFF sign-extends to
 * itself, and the upper tick word (0x0001) must be discarded. Once
 * more distinguishes the 16-bit store from a full-32-bit store.
 * n_ticks=0 exits immediately (no spin). */
static void test_wait_n_bios_ticks_positive_word(void)
{
    *(volatile uint32 *)0x46CuL = 0x00017FFFuL;
    data_fd2_engine_wait_n_bios_ticks_last_seen = 0;
    fd2_wait_n_bios_ticks(0);
    ASSERT_EQ(data_fd2_engine_wait_n_bios_ticks_last_seen,
              0x00007FFFuL);
}


/* ---- Tests: wait_for_input_dialog_with_blink ---- */

static void test_wait_dialog_blink_esc(void)
{
    int r;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x011B;
    r = fd2_wait_for_input_dialog_with_blink(0);
    ASSERT_EQ(r, 0x01);
}


/* Scancode remap path (loop skipped: head!=tail so the buffer reads nonempty
 * and execution falls straight through to INT 16h + remap). The INT 16h scancode
 * is the HIGH byte of the word at the buffer head 0x41E (AH from INT 16h AH=10h),
 * exactly as the sibling fd2_wait_for_input_with_idle tests above. asm 0x179a4-
 * 0x179c3: 0xE0->0x1C, 0x52->0x1C, 0x53->0x01, else passthrough. */
static void test_wait_dialog_repaint_remap_52(void)
{
    int r;
    g_repaint_flip_buffer_after = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5200;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    ASSERT_EQ(r, 0x1c);
}


static void test_wait_dialog_repaint_remap_e0(void)
{
    int r;
    g_repaint_flip_buffer_after = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0xE000;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    ASSERT_EQ(r, 0x1c);
}


static void test_wait_dialog_repaint_remap_53(void)
{
    int r;
    g_repaint_flip_buffer_after = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5300;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    ASSERT_EQ(r, 0x01);
}


static void test_wait_dialog_repaint_passthrough(void)
{
    int r;
    g_repaint_flip_buffer_after = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    ASSERT_EQ(r, 0x39);
}


/* Blink oscillator state-transition (asm 0x178bf-0x178f8): runs the idle loop
 * BODY exactly once via the repaint-stub buffer flip, then the seeded passthrough
 * scancode exits. Buffer starts EMPTY (head==tail==0x1E) so the loop is entered;
 * the stub flips tail->head+2 on its first call so the next top-of-loop check
 * exits. Oscillator trigger is forced deterministically: tick_latch=0 and BIOS
 * tick low word 0x4000 give diff=0x4000 (>3, high bit clear) regardless of the
 * free-running timer ISR (wrap into 0..3 is impossible in one tick from 0x4000),
 * so the 0/1 oscillator advances (+1, wrap at 2). 0->1 here (no wrap). */
static void test_wait_dialog_repaint_oscillator_0_to_1(void)
{
    int r;
    g_repaint_settings_calls = 0;
    g_repaint_flip_buffer_after = 1;
    data_fd2_dialog_blink_phase_oscillator = 0;
    data_fd2_dialog_blink_phase_oscillator_tick_latch = 0;
    *(volatile uint32 *)0x46CuL = 0x00004000uL;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x1E;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    g_repaint_flip_buffer_after = 0;
    ASSERT_EQ(g_repaint_settings_calls, 1);
    ASSERT_EQ(data_fd2_dialog_blink_phase_oscillator, 1);
    ASSERT_EQ(r, 0x39);
}


/* Same single-body drive as above but oscillator preset to 1 so the +1 hits 2
 * and wraps back to 0 (asm 0x178dd CMP ==2 -> 0x178e6 store 0). Pins the modulo-2
 * wrap that the 0->1 case does not exercise. */
static void test_wait_dialog_repaint_oscillator_1_to_0(void)
{
    int r;
    g_repaint_settings_calls = 0;
    g_repaint_flip_buffer_after = 1;
    data_fd2_dialog_blink_phase_oscillator = 1;
    data_fd2_dialog_blink_phase_oscillator_tick_latch = 0;
    *(volatile uint32 *)0x46CuL = 0x00004000uL;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x1E;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_input_with_dialog_repaint(0, 0);
    g_repaint_flip_buffer_after = 0;
    ASSERT_EQ(g_repaint_settings_calls, 1);
    ASSERT_EQ(data_fd2_dialog_blink_phase_oscillator, 0);
    ASSERT_EQ(r, 0x39);
}


/* ---- Tests: wait_ticks_or_keypress ---- */

static void test_wait_ticks_or_keypress_timeout(void)
{
    fd2_wait_ticks_or_keypress_with_palette(0);
    ASSERT_TRUE(1);
}


/* ---- Tests: wait_for_input_v2 ---- */

static void test_wait_input_v2_basic(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_for_input_v2();
    ASSERT_EQ(r, 0x39);
}


/* ---- Tests: wait_for_input_with_idle ---- */

static void test_wait_input_idle_arrow(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x4800;
    r = fd2_wait_for_input_with_idle();
    ASSERT_EQ(r, 0x48);
}


static void test_wait_input_idle_remap_52(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5200;
    r = fd2_wait_for_input_with_idle();
    ASSERT_EQ(r, 0x1c);
}


static void test_wait_input_idle_remap_53(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5300;
    r = fd2_wait_for_input_with_idle();
    ASSERT_EQ(r, 0x01);
}


/* ---- Tests: wait_for_action_target_input ---- */

static void test_wait_action_target_esc(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x011B;
    r = fd2_wait_for_action_target_input(4, 0, 0);
    ASSERT_EQ(r, -1);
}


static void test_wait_action_target_mode4_commit(void)
{
    int r;
    uint8 fake_tile_map[64];
    memset(fake_tile_map, 0, sizeof(fake_tile_map));
    fake_tile_map[7] = 0x00;
    data_fd2_battle_tile_map_ptr = (uint32)fake_tile_map;
    data_fd2_battle_cursor_world_x = 0;
    data_fd2_battle_cursor_world_y = 0;
    data_fd2_battle_map_width_tiles = 4;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x1C0D;
    r = fd2_wait_for_action_target_input(4, 0, 0);
    ASSERT_EQ(r, 1);
}


static void test_wait_action_target_mode5_no_commit(void)
{
    int r;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x22;
    *(volatile uint16 *)0x41EuL = 0x1C0D;
    *(volatile uint16 *)0x420uL = 0x011B;
    r = fd2_wait_for_action_target_input(5, 0, 0);
    ASSERT_EQ(r, -1);
}


/* ---- Tests: wait_input_with_chapter_dialog_blink @ 0x2D85F ---- */

/* Scancode-remap + return-value path. The buffer starts NONEMPTY (head 0x41A
 * != tail 0x41C) so the do-while runs its body once and exits at the bottom
 * check WITHOUT the per-frame tick gate having to fire (the gate compares the
 * freshly-latched saved_tick against the current BIOS tick: diff is 0/1 on the
 * first pass, so the blink/render side effects are skipped and only the
 * INT 16h + remap tail is exercised). The INT 16h fn 10h scancode is the HIGH
 * byte of the word at the buffer head 0x41E (AH from INT 16h), exactly as the
 * sibling wait_for_input_with_idle / wait_input_with_dialog_repaint tests.
 * asm 0x2d9c2-0x2d9ef: 0xE0->0x1C, 0x52->0x1C, 0x53->0x01, else passthrough.
 * NOTE this remap set DIFFERS from fd2_wait_input_with_recruitment_repaint
 * (which additionally maps ASCII space 0x20 -> 0x1C); the four cases below pin
 * this function's set independently. rng_seed is reset for determinism since
 * setup unconditionally advances it once (asm 0x2d88f).
 *
 * mode==0 takes the corner-sprite blit loop (asm 0x2d8ce-0x2d8ff), which
 * dereferences the atlas pointer: MOV EAX,[0x54147]; ADD EAX,[EAX+EDX*4+6]
 * (sprite_idx 3..9 for the 4 corners). A 256-byte zeroed atlas buffer keeps
 * that read in-bounds and deterministic; the looked-up offset (0) is added to
 * the base and handed to the no-op blit stub. */
static void test_wait_chapter_blink_remap_e0(void)
{
    static uint8 fake_atlas[256];
    int r;
    memset(fake_atlas, 0, sizeof(fake_atlas));
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)fake_atlas;
    data_fd2_ui_menu_cursor_idx = 0;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0xE000;
    r = fd2_wait_input_with_chapter_dialog_blink(0);
    ASSERT_EQ(r, 0x1c);
}


static void test_wait_chapter_blink_remap_52(void)
{
    static uint8 fake_atlas[256];
    int r;
    memset(fake_atlas, 0, sizeof(fake_atlas));
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)fake_atlas;
    data_fd2_ui_menu_cursor_idx = 0;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5200;
    r = fd2_wait_input_with_chapter_dialog_blink(0);
    ASSERT_EQ(r, 0x1c);
}


static void test_wait_chapter_blink_remap_53(void)
{
    static uint8 fake_atlas[256];
    int r;
    memset(fake_atlas, 0, sizeof(fake_atlas));
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)fake_atlas;
    data_fd2_ui_menu_cursor_idx = 0;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5300;
    r = fd2_wait_input_with_chapter_dialog_blink(0);
    ASSERT_EQ(r, 0x01);
}


/* Passthrough: a real menu-nav scancode (0x39 = SPACE) is NOT in the remap set
 * and must be returned verbatim. Pins that the function does NOT add the
 * recruitment-sibling's 0x20->0x1C remap nor any other rewrite. */
static void test_wait_chapter_blink_passthrough(void)
{
    static uint8 fake_atlas[256];
    int r;
    memset(fake_atlas, 0, sizeof(fake_atlas));
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = (uint32)fake_atlas;
    data_fd2_ui_menu_cursor_idx = 0;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x3920;
    r = fd2_wait_input_with_chapter_dialog_blink(0);
    ASSERT_EQ(r, 0x39);
}


/* mode != 0 split: asm 0x2d8c3 CMP [ESP+0x2c],0x0 / JNZ 0x2d901 skips the
 * corner-sprite blit loop entirely, so the atlas pointer is NEVER dereferenced.
 * Deliberately leave data_fd2_ui_menu_screen_sprite_atlas_buf_ptr at 0 to prove
 * the loop is skipped (a non-zero mode that still entered the loop would read
 * *(uint32*)(0+6+idx*4)); the remap tail must still produce the same result. */
static void test_wait_chapter_blink_mode1_remap_53(void)
{
    int r;
    data_fd2_ui_menu_screen_sprite_atlas_buf_ptr = 0;
    data_fd2_shared_rng_seed = 0;
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x20;
    *(volatile uint16 *)0x41EuL = 0x5300;
    r = fd2_wait_input_with_chapter_dialog_blink(1);
    ASSERT_EQ(r, 0x01);
}


void run_input_input_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: input/input\n");
    RUN_TEST(test_read_bios_tick);
    RUN_TEST(test_kbd_buffer_empty);
    RUN_TEST(test_kbd_buffer_nonempty);
    RUN_TEST(test_clear_kbd_buffer);
    RUN_TEST(test_wait_one_bios_tick_smoke);
    RUN_TEST(test_wait_one_bios_tick_sign_extend);
    RUN_TEST(test_wait_one_bios_tick_positive_word);
    RUN_TEST(test_wait_n_bios_ticks_sign_extend);
    RUN_TEST(test_wait_n_bios_ticks_positive_word);
    RUN_TEST(test_wait_dialog_blink_esc);
    RUN_TEST(test_wait_dialog_repaint_remap_52);
    RUN_TEST(test_wait_dialog_repaint_remap_e0);
    RUN_TEST(test_wait_dialog_repaint_remap_53);
    RUN_TEST(test_wait_dialog_repaint_passthrough);
    RUN_TEST(test_wait_dialog_repaint_oscillator_0_to_1);
    RUN_TEST(test_wait_dialog_repaint_oscillator_1_to_0);
    RUN_TEST(test_wait_ticks_or_keypress_timeout);
    RUN_TEST(test_wait_input_v2_basic);
    RUN_TEST(test_wait_input_idle_arrow);
    RUN_TEST(test_wait_input_idle_remap_52);
    RUN_TEST(test_wait_input_idle_remap_53);
    RUN_TEST(test_wait_action_target_esc);
    RUN_TEST(test_wait_action_target_mode4_commit);
    RUN_TEST(test_wait_action_target_mode5_no_commit);
    RUN_TEST(test_wait_chapter_blink_remap_e0);
    RUN_TEST(test_wait_chapter_blink_remap_52);
    RUN_TEST(test_wait_chapter_blink_remap_53);
    RUN_TEST(test_wait_chapter_blink_passthrough);
    RUN_TEST(test_wait_chapter_blink_mode1_remap_53);
    printf("\n");
}
