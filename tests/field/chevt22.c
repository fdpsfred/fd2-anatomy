/*
 * unit tests for src/field/chevt2.c (part 2)
 *
 * fd2_chapter_event_handler_38__ch25_dialog_with_state (dispatch idx 0x38 @
 * table 0x51B91) is a straight-line ch25 turn-6 establishing scene with no
 * computed logic and no turn gate. Its functionally-exact contract is the fixed
 * call sequence, in order:
 *     fd2_pan_cursor_and_window(6, 0x28)
 *     fd2_load_chapter_portraits_and_dump_tmp(1)
 *     fd2_cutscene_event_trigger(0x4A)
 *     fd2_display_dialog_scene(current_chapter_text, page 5, ...)
 *     fd2_clear_all_chars_facing()
 * In the binary the last call is a tail-JMP into fd2_clear_all_chars_facing
 * (a binary-size optimisation); the source is a plain call + return.
 *
 * These tests drive the REAL handler and its REAL callees. The deterministic,
 * host-observable contract pinned here is:
 *   (a) the camera pan lands the battle window origin exactly on (6, 0x28)
 *       (fd2_pan_cursor_and_window steps origin one tile per composite until it
 *       equals the target), and
 *   (b) the page-5 dialog VM actually executes its body (g_dlg_glyph_calls).
 * The cutscene-0x4A walk-animation interpreter and the facing reset are pure
 * display/state side effects; per the same deferral the sibling handlers apply
 * to display branches, their pixel/sprite output is deferred to Phase 9
 * integration. They are still driven for real here over host-safe inputs (a
 * 0-group cutscene script + an empty party) so the real handler executes its
 * whole body without any wild reads.
 *
 * Host-safety recipe mirrors the proven part-1 (chevt2) and chtrans suites:
 *   - empty party (member_count = 0): the cutscene per-char paint, the shadow
 *     overlay, and the facing-reset loop all iterate zero chars,
 *   - the portrait loader scan length is the in-process alloc_offset; 0 makes
 *     the scan a no-op (the loader still re-reads the real staged FDFIELD.DAT
 *     and rewrites FD2.TMP, exactly as the part-1 h36 test exercises),
 *   - a real workspace buffer + sprite atlas back the real compositor/blit so
 *     the pan + cutscene composites read valid memory (the tile-map blit itself
 *     is the testglob recorder g_composite_call_count),
 *   - cutscene_event_state = 0 skips the palette-fade-in tween (no VGA palette
 *     writes); the cutscene script for slot 0x4A is an in-memory 0-group script
 *     so the interpreter just composites a final clean frame,
 *   - the page-5 dialog runs the real dialog VM over an in-memory int16 program
 *     (NOT a game file); the glyph blitter is the testglob recorder, the empty
 *     BIOS keyboard buffer keeps blink set, and audiofix gates the per-glyph
 *     blink/typewriter path host-safely.
 */

#include <string.h>
#include <stdlib.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include <stdio.h>
#include "audiofix.h"   /* audiofix_make_bank / audiofix_enable_sfx */
#include "minipfix.h"   /* minip_setup_env: sprite sheet + dialog-blit spies */

extern runtime_char g_test_rc_array[8];

/* testglob recorders */
extern int g_composite_call_count;   /* tile-map blit (composite stage 1)   */
extern int g_dlg_glyph_calls;        /* dialog glyph blitter                 */

/* testglob opt-in seam: flip the BIOS keyboard buffer nonempty on the Nth
 * mirrored-portrait blit, releasing the blocking wait that follows a
 * fd2_clear_keyboard_buffer drain (handler_3a path). */
extern int g_dlg_blit_mirror_inject_after;
extern int g_dlg_blit_mirror_inject_scancode;

extern void *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];

/* ---- host-safe render workspace (mirrors chtrans ct_install_safe_render_env) */
#define CE22_WS_SPAN (191u * 0x1c8u + 0x138u)   /* (h-1)*sstride + w the blit reads */
static uint8 g_ce22_ws_buffer[CE22_WS_SPAN];
static uint8 g_ce22_sprite_atlas[6 + 64 * 4 + 4];

/* ---- in-memory cutscene script for slot 0x4A: a single n_groups=0 byte, so
 * the interpreter parses zero groups and only composites the final frame. ---- */
static uint8 g_ce22_cutscene_script[1] = { 0x00 };

/* ---- in-memory page-5 dialog program (mirrors part-1 ce35_setup) ---- */
static int16 g_ce22_dlg_prog[12];

static void ce22_setup(int glyphs)
{
    int i;
    uint32 *atlas_tbl;

    /* --- portrait-loader env: alloc_offset 0 => scan is a no-op (still re-reads
     * the real staged FDFIELD.DAT[4*3+2] and rewrites FD2.TMP). --- */
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;            /* loaded fresh by the loader  */
    data_fd2_chapter_init_phase_flag = 1;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    data_fd2_chapter_current_chapter_id = 4;     /* re-read idx = 4*3+2 = 0xE   */
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_resource_portrait_cache_count = 0;
    data_fd2_resource_portrait_cache_buffer_used = 0;

    /* --- shared real-render env for pan + cutscene composites --- */
    data_fd2_battle_party_member_count = 0;      /* all per-char loops are no-ops */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce22_ws_buffer - 0x8088;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_battle_anim_phase = 1;
    atlas_tbl = (uint32 *)(g_ce22_sprite_atlas + 6);
    for (i = 0; i < 64; i++) {
        atlas_tbl[i] = (uint32)i;
    }
    data_fd2_runtime_battle_state_ptr = (uint32)g_ce22_sprite_atlas;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;
    data_fd2_chapter_cutscene_event_state = 0;   /* skip palette-fade-in tween   */

    /* --- cutscene slot 0x4A: 0-group in-memory script --- */
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x4A] =
        g_ce22_cutscene_script;

    /* --- page-5 dialog program: header word 5 -> body at byte 0x10 (int16 idx
     * 8); body is `glyphs` TEXT opcodes then END (-1). --- */
    memset(g_ce22_dlg_prog, 0, sizeof(g_ce22_dlg_prog));
    g_ce22_dlg_prog[5] = 0x10;
    for (i = 0; i < glyphs; i++) {
        g_ce22_dlg_prog[8 + i] = 0x41;           /* TEXT glyph */
    }
    g_ce22_dlg_prog[8 + glyphs] = -1;            /* END */
    current_chapter_text = (uint32)g_ce22_dlg_prog;

    /* --- deterministic dialog VM env: empty BIOS keyboard buffer + audio gated;
     * no active portrait so END skips the portrait-close path. --- */
    *(volatile uint16 *)0x41AuL = 0x20;
    *(volatile uint16 *)0x41CuL = 0x20;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    audiofix_enable_sfx();
    data_fd2_audio_fdother_sfx_bank_buf_ptr = audiofix_make_bank(0x1F);

    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;
}

static void ce22_teardown(void)
{
    if (portrait_sprite_cache != 0) {
        free((void *)portrait_sprite_cache);
        portrait_sprite_cache = 0;
    }
    data_fd2_chapter_cutscene_event_script_ptr_table_106[0x4A] = 0;
    data_fd2_tile_event_data_table_ptr = 0;
    data_fd2_resource_portrait_cache_alloc_offset = 0;
    chapter_portrait_load_buffer = 0;
    data_fd2_chapter_init_phase_flag = 0;
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    data_fd2_battle_party_member_count = 4;
    data_fd2_chapter_current_chapter_id = 1;
    current_chapter_text = 0;
    remove("FD2.TMP");                  /* generated swap file (not a game file) */
}

/* ----------------------------------------------------------------
 * Full handler contract: the camera pan lands the window origin exactly on the
 * literal target (6, 0x28), and the real page-5 dialog VM executes its body
 * (one glyph rendered). The window starts away from the target so the pan is
 * observable on both axes; g_composite_call_count > 0 proves the pan composited
 * frames. A 1-glyph page-5 program proves the dialog body (page 5, not some
 * other page) ran. The cutscene-0x4A interpreter and facing reset run for real
 * over the host-safe 0-group script + empty party.
 * ---------------------------------------------------------------- */
static void test_h38_pan_to_6_28_then_page5_dialog(void)
{
    ce22_setup(1);

    /* start the window away from (6, 0x28) on both axes so the pan is visible */
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;

    fd2_chapter_event_handler_38__ch25_dialog_with_state(0);

    /* pan landed the window origin on the literal target (6, 0x28) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 6);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x28);
    /* the camera pan composited frames on the way to the target */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* the page-5 dialog body executed (rendered the single glyph) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    /* the portrait loader ran its no-op scan and nulled its load buffer */
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);

    ce22_teardown();
}

/* ----------------------------------------------------------------
 * The handler ignores its dispatch arg: it is the table's 1-arg cdecl ABI
 * placeholder and the body reads only literals. Driving the handler with a
 * non-zero arg (0x55) must produce the identical observable effect — origin on
 * (6, 0x28) and the page-5 dialog body running — proving the arg does not leak
 * into any call. The dialog body is empty (immediate END) here so the pan +
 * dialog-entry are the sole effects under test.
 * ---------------------------------------------------------------- */
static void test_h38_ignores_dispatch_arg(void)
{
    ce22_setup(0);                      /* 0 glyphs: immediate END */

    data_fd2_battle_view_window_origin_x = 0x10;
    data_fd2_battle_view_window_origin_y = 0x05;

    fd2_chapter_event_handler_38__ch25_dialog_with_state(0x55);

    /* identical fixed target regardless of the arg */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 6);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0x28);
    /* dialog was entered but rendered nothing (immediate END) */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce22_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_39__ch26_cinematic @ 0x354DD
 *
 * ch26 establishing-shot stub (dispatch idx 0x39 @ table 0x51B91). Body:
 *   fd2_load_chapter_portraits_and_dump_tmp(data_fd2_battle_turn_counter)
 *       -- the RAW counter, same as the ch24 handler_36, NOT the signed /2 used
 *          by the ch21/ch22 handlers
 *   fd2_pan_cursor_and_window(9, 0)
 *   __delay_thunk_375b2(400)
 * No turn gate, no dialog. In the binary the pan + 400ms hold + RET is a
 * Class-3 shared tail borrowed from handler_36 @ 0x353C4; these tests pin THIS
 * handler's functionally-exact contract: the RAW-counter portrait index and the
 * single pan target (9, 0). Reuses ce22_setup's real render/portrait env above
 * (the portrait loader re-reads the staged real FDFIELD.DAT and rewrites
 * FD2.TMP); __delay_thunk_375b2 is REAL and paces 400ms against the
 * host-advancing BIOS tick word.
 * ================================================================ */

/* ----------------------------------------------------------------
 * Portrait index is the RAW counter (NO /2) — guards against copying the
 * ch21/ch22 handlers' signed /2. The tile-event table holds a decoy record
 * race=2 (the value a wrong /2 port — 5/2 == 2 — would select) and a target
 * record race=5; with turn_counter = 5 only the race=5 record matches, so the
 * real fd2_init_runtime_char_for_battle runs exactly once (party_member_count
 * 0 -> 1). ce22_setup primes the real loader env; we then install a non-zero
 * tile-event scan (ce22_setup leaves alloc_offset 0) with the decoy records.
 * ---------------------------------------------------------------- */
static uint8 *g_ce22_h39_tileevent;

static void test_h39_portrait_index_is_raw_counter(void)
{
    static const uint8 races[6] = { 0, 0, 0x02, 0, 0, 0x05 }; /* decoy@2, target@5 */
    int i;

    ce22_setup(0);

    /* install a 6-record tile-event scan (stride 0x1A, race byte at +0x98) over
     * the no-op env ce22_setup left in place. */
    g_ce22_h39_tileevent =
        (uint8 *)malloc((size_t)0x98 + (size_t)6 * 0x1a + 0x20);
    memset(g_ce22_h39_tileevent, 0, (size_t)0x98 + (size_t)6 * 0x1a + 0x20);
    for (i = 0; i < 6; i++) {
        g_ce22_h39_tileevent[i * 0x1a + 0x98] = races[i];
    }
    data_fd2_tile_event_data_table_ptr = (uint32)g_ce22_h39_tileevent;
    data_fd2_resource_portrait_cache_alloc_offset = 6;
    data_fd2_battle_party_member_count = 0;

    data_fd2_battle_turn_counter = 5;             /* raw 5 (NOT 5/2 == 2) */

    fd2_chapter_event_handler_39__ch26_cinematic(0);

    /* exactly the race==5 record matched -> one char inited; the race==2 decoy
     * (the only other non-zero record) would have matched a wrong /2 port. */
    ASSERT_EQ((long)data_fd2_battle_party_member_count, 1);
    ASSERT_EQ((long)chapter_portrait_load_buffer, 0);  /* loader freed+nulled */

    free(g_ce22_h39_tileevent);
    g_ce22_h39_tileevent = 0;
    ce22_teardown();
}

/* ----------------------------------------------------------------
 * The single pan lands the window origin exactly on the borrowed-tail target
 * (9, 0). The window starts away from the target on both axes so the move is
 * observable, and g_composite_call_count > 0 proves the pan composited frames.
 * No dialog runs in this handler. A non-zero dispatch arg (0x55) is passed to
 * confirm the arg does not leak into the pan target (the body reads only
 * literals + the turn counter). alloc_offset 0 keeps the portrait load a
 * host-safe no-op.
 * ---------------------------------------------------------------- */
static void test_h39_pan_to_9_0_ignores_arg(void)
{
    ce22_setup(0);                       /* alloc_offset 0: portrait scan no-op */
    data_fd2_battle_turn_counter = 2;    /* real ch26 trigger turn */

    /* start the window away from the target on both axes */
    data_fd2_battle_view_window_origin_x = 0x40;
    data_fd2_battle_view_window_origin_y = 0x40;
    g_composite_call_count = 0;
    g_dlg_glyph_calls = 0;

    fd2_chapter_event_handler_39__ch26_cinematic(0x55);

    /* pan landed the window origin on the literal target (9, 0) */
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_x, 9);
    ASSERT_EQ((long)data_fd2_battle_view_window_origin_y, 0);
    /* the camera pan composited frames on the way to the target */
    ASSERT_TRUE(g_composite_call_count > 0);
    /* no dialog in this handler */
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);

    ce22_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_3a__unref_pickup @ 0x354FE
 *
 * Tile-pickup handler (dispatch idx 0x3A @ table 0x51B91). Body:
 *   - copy the inline 5-byte item-id table { 0x1D,0x2B,0x33,0x3D,0x47 } to a local
 *   - fd2_clear_keyboard_buffer()
 *   - fd2_load_chapter_portrait(runtime_char[ci].portrait_id)
 *   - if (fd2_count_usable_inventory_slots(ci) == 8):   // inventory full
 *       page-0x1E0 "inventory full" dialog -> paint -> wait -> close
 *     else:                                             // has space
 *       read cursor tile attr; tile_attr = terrain-class low byte
 *       last_action_sprite_id = item_id_table[tile_attr] + 0xB5
 *       page-0x1A6 "you got [item]" dialog -> paint -> wait
 *       fd2_add_item_to_inventory(ci, item_id_table[tile_attr])
 *       close; consumed_flags[0..4] = 1; tick tile-event anims
 *
 * The risk-bearing computed contract pinned here is: the inventory-full vs
 * has-space BRANCH, the table-lookup + sprite-id formula (+0xB5), the item grant,
 * and the 5-slot consume lockout. The REAL handler is driven end-to-end over the
 * proven host-safe render env (minipfix sprite sheet + dialog-blit spies; the
 * portrait loader re-reads the real staged DATO.DAT; the page dialogs run the
 * REAL dialog VM over an in-memory immediate-END text table whose END path skips
 * the portrait/close work because no speaker portrait was armed; the blocking
 * fd2_wait_for_input_dialog_with_blink(0) returns at once because the BIOS
 * keyboard buffer is pre-seeded non-empty; the slide-out close + composite + the
 * memmove to/from 0xA0000 are harmless under DOS/4GW, same convention as the
 * status.c / rsrc.c lcp suites). The display side effects (frame draw, portrait
 * blit, slide animation) are owned by the dialog/rsrc/status/input suites; here
 * they only execute for real as a byproduct.
 * ================================================================ */

/* in-memory dialog text table covering both page indices the handler uses
 * (0x1A6 and 0x1E0); each redirects to an immediate END (-1) so the dialog VM
 * returns without page-break wait or speaker portrait work. */
static int16 g_ce3a_text[0x200];

/* tile fixture for fd2_read_tile_attribute_at_pos / fd2_tick_tile_event_animations
 * (1x1 map). tile meta is 4 bytes; the read uses [+4..+5]=sprite_idx and
 * [+6]=terrain_byte. attr-flags buffer is indexed by sprite_idx*4. consume-flags
 * buffer is indexed by [0..4] (handler) and by terrain-class (tick). */
static uint8 g_ce3a_tile_map[64];
static uint8 g_ce3a_attr_flags[64];
static uint8 g_ce3a_consume[0x100];

/* render workspace backing for fd2_composite_battle_frame(0) inside the slide-out
 * close (the tile-map composite stage is the testglob recorder; the workspace is
 * never dereferenced, but back it for safety). */
static uint8 g_ce3a_ws[0x10000];

#define CE3A_SPRITE_SENTINEL 0x5A5A5A5AuL   /* poison for last_action_sprite_id */

/* Stand up the shared host-safe env; `terrain_class` is the tile terrain-class
 * byte the cursor-tile read will yield (used by the pickup-branch tests). */
static void ce3a_setup(uint8 terrain_class)
{
    int i;

    minip_setup_env();                 /* sprite sheet + dialog-blit spies      */

    /* immediate-END program for both handler page indices */
    for (i = 0; i < 0x200; i++) {
        g_ce3a_text[i] = 0;
    }
    g_ce3a_text[0x1A6] = (int16)(0x1FE * 2);   /* page 0x1A6 -> END word */
    g_ce3a_text[0x1E0] = (int16)(0x1FE * 2);   /* page 0x1E0 -> END word */
    g_ce3a_text[0x1FE] = -1;                    /* END */
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)g_ce3a_text;

    /* runtime_char array: the loader reads char[ci].portrait_id; the inventory
     * helpers read/write char[ci].inventory_slots[]. */
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));

    /* tile fixture: cursor at (0,0); 1x1 map. sprite_idx 0 -> attr_ptr = flags+0.
     * terrain_byte at meta[+6] -> terrain class (low 5 bits). attr flags byte 0
     * so tick's (flags & 0x60)==0x20 branch is false (no anim bump). */
    memset(g_ce3a_tile_map, 0, sizeof(g_ce3a_tile_map));
    memset(g_ce3a_attr_flags, 0, sizeof(g_ce3a_attr_flags));
    memset(g_ce3a_consume, 0, sizeof(g_ce3a_consume));
    g_ce3a_tile_map[4] = 0;            /* sprite_idx low  */
    g_ce3a_tile_map[5] = 0;            /* sprite_idx high */
    g_ce3a_tile_map[6] = terrain_class;
    data_fd2_battle_cursor_world_x = 0;
    data_fd2_battle_cursor_world_y = 0;
    data_fd2_battle_map_width_tiles = 1;
    data_fd2_battle_map_height_tiles = 1;
    data_fd2_battle_tile_map_ptr = (uint32)g_ce3a_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_ce3a_attr_flags;
    data_fd2_field_map_tile_event_consumed_flags_ptr = (uint32)g_ce3a_consume;

    /* slide-out close env: composite workspace + phase 0 (cursor overlay no-op),
     * empty party (per-char overlays no-op). The portrait loader allocates the
     * three slide workspaces and the close frees them, so null the globals first
     * (loader assigns fresh; the prev-portrait-buffer free path needs null too). */
    data_fd2_large_game_state_buffer_ptr = (uint32)g_ce3a_ws - 0x8088;
    data_fd2_battle_anim_phase = 0;
    data_fd2_battle_party_member_count = 0;
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
    }
    data_fd2_portrait_sprite_buffer = 0;

    /* The handler's FIRST call is fd2_clear_keyboard_buffer (tail := head), which
     * empties the buffer, so a pre-seed here would be wiped. Instead arm the
     * mirrored-blit seam: fd2_load_chapter_portrait draws the portrait via the
     * mirrored blit (call #1) AFTER the drain and BEFORE the blocking
     * fd2_wait_for_input_dialog_with_blink(0); the spy then re-fills the buffer
     * nonempty so the wait returns at once. */
    *(volatile uint16 *)0x41AuL = 0x1E;
    *(volatile uint16 *)0x41CuL = 0x1E;     /* start EMPTY (head == tail) */
    g_dlg_blit_mirror_inject_after = 1;     /* flip on the 1st mirrored blit */
    g_dlg_blit_mirror_inject_scancode = 0x01;   /* Esc scancode */

    data_fd2_dialog_last_action_sprite_id_param = CE3A_SPRITE_SENTINEL;
}

static void ce3a_teardown(void)
{
    /* the handler's slide-out close already free()d the three slide workspaces;
     * drop the dangling globals. free + null the loaded portrait buffer. */
    data_fd2_ui_slide_anim_accumulator_buf_ptr = 0;
    data_fd2_ui_slide_bg_snapshot_buf_ptr = 0;
    data_fd2_ui_slide_composed_target_buf_ptr = 0;
    if (data_fd2_portrait_sprite_buffer != 0) {
        free((void *)data_fd2_portrait_sprite_buffer);
        data_fd2_portrait_sprite_buffer = 0;
    }
    data_fd2_battle_tile_map_ptr = 0;
    data_fd2_tile_attribute_flags_buffer_ptr = 0;
    data_fd2_field_map_tile_event_consumed_flags_ptr = 0;
    data_fd2_all_game_text_ptr = 0;
    data_fd2_battle_party_member_count = 4;
    data_fd2_dialog_active_portrait_blit_offset = 0;
    g_dlg_blit_mirror_inject_after = 0;
    g_dlg_blit_mirror_inject_scancode = 0;
}

/* ----------------------------------------------------------------
 * Inventory-FULL branch: all 8 of char 2's inventory slots are occupied
 * (slot_flag bit 0x80 clear) so fd2_count_usable_inventory_slots == 8 and the
 * handler takes the "inventory full" path. Proof the full path was taken (and the
 * pickup path was NOT): (a) last_action_sprite_id is never written (stays at the
 * poison sentinel — it is only assigned in the pickup branch), (b) no item was
 * granted (the slots stay exactly as seeded — a reached add-item would clear a
 * 0x80 flag, but there is none to clear here anyway, so we instead assert the
 * seeded occupied item ids are intact), and (c) the 5 tile-event consume flags
 * stay 0 (the lockout loop runs only on the pickup path).
 * ---------------------------------------------------------------- */
static void test_h3a_inventory_full_branch(void)
{
    int i;

    ce3a_setup(3);

    g_test_rc_array[2].portrait_id = 0x40;     /* default 0x9017 portrait slot */
    /* fill all 8 inventory slots occupied (flag 0, item id sentinel) */
    for (i = 0; i < 8; i++) {
        g_test_rc_array[2].inventory_slots[i * 2 + 0] = 0x00;   /* occupied */
        g_test_rc_array[2].inventory_slots[i * 2 + 1] = (uint8)(0xA0 + i);
    }

    fd2_chapter_event_handler_3a__unref_pickup(2);

    /* (a) the pickup-only sprite-id write did not happen */
    ASSERT_EQ((long)data_fd2_dialog_last_action_sprite_id_param,
              (long)CE3A_SPRITE_SENTINEL);
    /* (b) inventory untouched: every slot still occupied with its sentinel id */
    for (i = 0; i < 8; i++) {
        ASSERT_EQ(g_test_rc_array[2].inventory_slots[i * 2 + 0], 0x00);
        ASSERT_EQ(g_test_rc_array[2].inventory_slots[i * 2 + 1],
                  (uint8)(0xA0 + i));
    }
    /* (c) no tile-event slot was consumed */
    for (i = 0; i < 5; i++) {
        ASSERT_EQ(g_ce3a_consume[i], 0x00);
    }

    ce3a_teardown();
}

/* ----------------------------------------------------------------
 * Pickup branch: char 2 has a free inventory slot so the handler reads the
 * cursor tile, looks the item up in the 5-byte table by terrain class, publishes
 * the item sprite id, grants the item and locks out all 5 tile-event slots. With
 * terrain class 3 the table selects item id 0x3D, so:
 *   - last_action_sprite_id == 0x3D + 0xB5 == 0xF2  (table lookup + sprite formula)
 *   - the first empty slot becomes occupied (flag 0) holding item id 0x3D (grant)
 *   - consumed_flags[0..4] are all 1                 (broad lockout)
 * Slot 0 is seeded empty (0x80) and the other 7 occupied so the grant lands in a
 * deterministic slot 0; a pre-seeded sentinel id there is overwritten by 0x3D.
 * ---------------------------------------------------------------- */
static void test_h3a_pickup_grants_item_and_locks_slots(void)
{
    int i;

    ce3a_setup(3);                              /* terrain class 3 -> item 0x3D */

    g_test_rc_array[2].portrait_id = 0x40;      /* default portrait slot */
    g_test_rc_array[2].inventory_slots[0] = 0x80;   /* slot 0 EMPTY        */
    g_test_rc_array[2].inventory_slots[1] = 0xEE;   /* sentinel item id    */
    for (i = 1; i < 8; i++) {
        g_test_rc_array[2].inventory_slots[i * 2 + 0] = 0x00;   /* occupied */
        g_test_rc_array[2].inventory_slots[i * 2 + 1] = (uint8)(0xB0 + i);
    }

    fd2_chapter_event_handler_3a__unref_pickup(2);

    /* table lookup (item_id_table[3] == 0x3D) + sprite formula (+0xB5) */
    ASSERT_EQ((long)data_fd2_dialog_last_action_sprite_id_param,
              (long)(0x3D + 0xB5));
    /* item granted into the empty slot 0: flag cleared to occupied, id == 0x3D */
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[0], 0x00);
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[1], 0x3D);
    /* all 5 tile-event slots locked out */
    for (i = 0; i < 5; i++) {
        ASSERT_EQ(g_ce3a_consume[i], 0x01);
    }
    /* the other occupied slots were left alone (grant hit slot 0 only) */
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[2], 0x00);
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[3], (uint8)0xB1);

    ce3a_teardown();
}

/* ----------------------------------------------------------------
 * Table-index coverage: a different terrain class selects a different item,
 * proving the index is the tile's terrain-class byte (not a constant). Terrain
 * class 0 -> item id 0x1D -> sprite 0x1D + 0xB5 == 0xD2, granted as id 0x1D.
 * ---------------------------------------------------------------- */
static void test_h3a_pickup_table_index_class0(void)
{
    ce3a_setup(0);                              /* terrain class 0 -> item 0x1D */

    g_test_rc_array[2].portrait_id = 0x40;
    g_test_rc_array[2].inventory_slots[0] = 0x80;   /* slot 0 empty */
    g_test_rc_array[2].inventory_slots[1] = 0xEE;

    fd2_chapter_event_handler_3a__unref_pickup(2);

    ASSERT_EQ((long)data_fd2_dialog_last_action_sprite_id_param,
              (long)(0x1D + 0xB5));
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[0], 0x00);
    ASSERT_EQ(g_test_rc_array[2].inventory_slots[1], 0x1D);

    ce3a_teardown();
}

/* ================================================================
 * fd2_chapter_event_handler_3b__ch26_ai_ctrl @ 0x35641
 *
 * Char-conditional in-memory computation: only when the stepping char's team
 * (runtime_char +0x06) is non-zero (npc/player; an enemy stepper with team==0
 * is skipped) does it disarm AI control flag 0 (the low nibble of
 * runtime_char.combat_aux_block[0xD], absolute offset 0x34) for the inclusive
 * char range 0x27..0x2C.
 *
 * Drives the REAL handler and its REAL callee
 * (fd2_set_combat_aux_block_byte_d_low4_for_char_range) against a local
 * runtime_char array big enough for index 0x2C (the shared g_test_rc_array[8]
 * is too small). No game files, no display.
 * ================================================================ */

#define CE3B_NCHARS      0x30       /* must cover the highest index 0x2C */
#define CE3B_AI_OFF      0x34       /* combat_aux_block[0xD] absolute offset */

static runtime_char g_ce3b_rc[CE3B_NCHARS];

/* offset 0x34 of char `idx`, read as raw byte */
static uint8 ce3b_ai(int idx)
{
    return ((uint8 *)&g_ce3b_rc[idx])[CE3B_AI_OFF];
}

/* Seed offset 0x34 of every char with a non-zero HIGH nibble (0xA0) and a
 * non-zero LOW nibble (0x05) so we can prove: (a) in-range chars get their low
 * nibble cleared to 0 with the high nibble preserved -> 0xA0, and (b)
 * out-of-range chars keep 0xA5 untouched. `stepper_team` seeds the team byte
 * of the stepping char (index 0). */
static void ce3b_setup(uint8 stepper_team)
{
    int i;

    memset(g_ce3b_rc, 0, sizeof(g_ce3b_rc));
    for (i = 0; i < CE3B_NCHARS; i++) {
        ((uint8 *)&g_ce3b_rc[i])[CE3B_AI_OFF] = 0xA5;
    }
    g_ce3b_rc[0].team = stepper_team;
    data_fd2_battle_runtime_char_array_ptr = g_ce3b_rc;
}

static void ce3b_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
}

/* ----------------------------------------------------------------
 * Non-enemy stepper (team != 0) disarms the range: every char in 0x27..0x2C
 * gets its low nibble cleared to 0 (high nibble 0xA preserved -> 0xA0).
 * ---------------------------------------------------------------- */
static void test_h3b_nonzero_team_disarms_range(void)
{
    int i;

    ce3b_setup(1);                              /* team 1 (npc) -> fires */

    fd2_chapter_event_handler_3b__ch26_ai_ctrl(0);

    for (i = 0x27; i <= 0x2C; i++) {
        ASSERT_EQ((long)ce3b_ai(i), 0xA0);
    }

    ce3b_teardown();
}

/* ----------------------------------------------------------------
 * Player stepper (team == 2) also fires (the condition is team != 0, not a
 * specific team): the range is disarmed identically.
 * ---------------------------------------------------------------- */
static void test_h3b_player_team_also_fires(void)
{
    int i;

    ce3b_setup(2);                              /* team 2 (player) -> fires */

    fd2_chapter_event_handler_3b__ch26_ai_ctrl(0);

    for (i = 0x27; i <= 0x2C; i++) {
        ASSERT_EQ((long)ce3b_ai(i), 0xA0);
    }

    ce3b_teardown();
}

/* ----------------------------------------------------------------
 * Enemy stepper (team == 0) is skipped: the conditional branch is NOT taken,
 * so the whole range stays at its seeded 0xA5 (no call to the AI-range writer).
 * ---------------------------------------------------------------- */
static void test_h3b_enemy_team_skips(void)
{
    int i;

    ce3b_setup(0);                              /* team 0 (enemy) -> skipped */

    fd2_chapter_event_handler_3b__ch26_ai_ctrl(0);

    for (i = 0x27; i <= 0x2C; i++) {
        ASSERT_EQ((long)ce3b_ai(i), 0xA5);
    }

    ce3b_teardown();
}

/* ----------------------------------------------------------------
 * Range boundaries are exact and inclusive: with a firing (non-enemy) stepper,
 * the chars just outside the range (0x26, 0x2D) stay 0xA5. Guards against
 * off-by-one bounds.
 * ---------------------------------------------------------------- */
static void test_h3b_range_boundaries_exact(void)
{
    ce3b_setup(1);

    fd2_chapter_event_handler_3b__ch26_ai_ctrl(0);

    ASSERT_EQ((long)ce3b_ai(0x26), 0xA5);       /* just before */
    ASSERT_EQ((long)ce3b_ai(0x2D), 0xA5);       /* just after  */
    ASSERT_EQ((long)ce3b_ai(0x27), 0xA0);       /* inclusive start fired */
    ASSERT_EQ((long)ce3b_ai(0x2C), 0xA0);       /* inclusive end fired   */

    ce3b_teardown();
}

void run_field_chevt22_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: field/chevt2 (part 2)\n");
    RUN_TEST(test_h38_pan_to_6_28_then_page5_dialog);
    RUN_TEST(test_h38_ignores_dispatch_arg);
    RUN_TEST(test_h39_portrait_index_is_raw_counter);
    RUN_TEST(test_h39_pan_to_9_0_ignores_arg);
    RUN_TEST(test_h3a_inventory_full_branch);
    RUN_TEST(test_h3a_pickup_grants_item_and_locks_slots);
    RUN_TEST(test_h3a_pickup_table_index_class0);
    RUN_TEST(test_h3b_nonzero_team_disarms_range);
    RUN_TEST(test_h3b_player_team_also_fires);
    RUN_TEST(test_h3b_enemy_team_skips);
    RUN_TEST(test_h3b_range_boundaries_exact);
    printf("\n");
}
