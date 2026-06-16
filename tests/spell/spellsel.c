/*
 * unit tests for src/spell/spellsel.c
 *
 * fd2_build_usable_spell_list(char_idx, out_buf): enumerate the spell ids the
 * runtime char at char_idx has learned (5-byte spells_known_bitmap, +0x1A,
 * 40 slots), returning the count and -- when out_buf != NULL -- writing each
 * id = byte*8 + bit in ascending order.
 *
 * fd2_draw_spell_selection_list(caster_idx, highlighted_idx, render_buf):
 * draw the learned-spell list as a 4-column grid. Drives the REAL renderer
 * and observes its dispatch to:
 *   - fd2_blit_sheet_sprite_at_offset (REAL) -> fd2_blit_sprite_raw_with_header
 *     spy (g_blitraw_*): the MP-icon sprite 0x5C per cell. With a fake sheet
 *     (table[i]=i) the sprite index is (logged_sprite - sheet) and the logged
 *     dst is the cell's icon offset.
 *   - fd2_render_decimal_number_to_buffer (REAL) -> fd2_blit_indexed_sprite_at_xy
 *     -> fd2_rle_blit_sprite spy (g_rle_blit_log_*): the 2-digit MP cost read
 *     from the spell effect record (*(byte*)(pSpell+5)).
 *   - fd2_display_dialog_scene (REAL) for the spell-name label; neutralized by
 *     an immediate-END text program (no blit, no fopen), except the highlight
 *     test which renders one glyph to capture the border color it forwards.
 *
 * Spell records live in data_fd2_battle_spell_effect_table (in-memory table in
 * testglob.c); spells learned by the caster live in g_test_rc_array's
 * spells_known_bitmap.
 */

#include <string.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* --- recording seams provided by testglob.c (see tests/gfx/rndstat.c) --- */
/* fd2_blit_sheet_sprite_at_offset -> raw-blit spy log (MP icon) */
extern int    g_blitraw_log_on;
extern int    g_blitraw_count;
extern uint32 g_blitraw_log_dst[512];
extern uint32 g_blitraw_log_sprite[512];
/* fd2_render_decimal_number_to_buffer digit glyphs (rle-blit spy log) */
extern int    g_rle_blit_calls;
extern int    g_rle_blit_log_on;
extern uint32 g_rle_blit_log_sprite[64];
extern uint32 g_rle_blit_log_dst[64];
/* fd2_blit_glyph_2bpp_with_outline spy: captures the color (page_idx) arg */
extern int    g_dlg_glyph_calls;
extern uint32 g_dlg_glyph_last_p5;
/* fd2_play_sfx_with_handle recording fake: bumped once per SFX (UI move sound) */
extern int    g_play_sfx_with_handle_calls;
/* host runtime_char fixture backing data_fd2_battle_runtime_char_array_ptr */
extern runtime_char g_test_rc_array[8];

/* fake sprite sheet: 6-byte header then 256 int32 entries with table[i] = i,
 * so a blit's resolved sprite addr = sheet_base + sprite_index and we recover
 * the index as (logged_sprite - sheet_base). */
static int32 dssl_sheet[2 + 256];

static uint32 dssl_setup_sheet(void)
{
    uint8 *base = (uint8 *)dssl_sheet;
    int    i;

    for (i = 0; i < 256; i++) {
        *(int32 *)(base + 6 + i * 4) = i;
    }
    data_fd2_ui_anim_sprite_sheet_ptr = (uint32)base;
    return (uint32)base;
}

/* immediate-END text program: every page entry points at a word holding -1, so
 * fd2_display_dialog_scene returns at once (no glyph blit). The spell-name page
 * is spell_id + 0x1B9 (<= 0x23 + 0x1B9 = 0x1DC), well inside the table. */
#define DSSL_TEXT_WORDS 0x400
#define DSSL_END_OFF    0x700      /* byte offset of the END (-1) marker */
static uint16 dssl_text[DSSL_TEXT_WORDS];

static void dssl_setup_text_end(void)
{
    int i;

    for (i = 0; i < DSSL_TEXT_WORDS; i++) {
        dssl_text[i] = 0;
    }
    *(int16 *)((uint8 *)dssl_text + DSSL_END_OFF) = -1;
    for (i = 0; i < (DSSL_END_OFF / 2); i++) {
        dssl_text[i] = (uint16)DSSL_END_OFF;
    }
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)dssl_text;
}

/* one-glyph text program: each used page points at the pair [glyph, -1], so the
 * dialog VM renders exactly one TEXT glyph (capturing the forwarded border
 * color in g_dlg_glyph_last_p5) and then ends. */
static void dssl_setup_text_one_glyph(void)
{
    int i;

    for (i = 0; i < DSSL_TEXT_WORDS; i++) {
        dssl_text[i] = 0;
    }
    /* glyph stream parked at DSSL_END_OFF: [0x0010 (TEXT)] [0xFFFF (END)] */
    *(int16 *)((uint8 *)dssl_text + DSSL_END_OFF)     = 0x0010;
    *(int16 *)((uint8 *)dssl_text + DSSL_END_OFF + 2) = -1;
    for (i = 0; i < (DSSL_END_OFF / 2); i++) {
        dssl_text[i] = (uint16)DSSL_END_OFF;
    }
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)dssl_text;
}

/* common fixture: zero char 0, clear spell table, install fake sheet +
 * immediate-END text, arm the blit/rle logs. Returns the sheet base. */
static uint32 dssl_setup(void)
{
    uint32 sheet;

    data_fd2_battle_runtime_char_array_ptr = g_test_rc_array;
    memset(&g_test_rc_array[0], 0, sizeof(g_test_rc_array[0]));
    memset(data_fd2_battle_spell_effect_table, 0,
           sizeof(spell_effect) * 36);

    sheet = dssl_setup_sheet();
    dssl_setup_text_end();

    g_blitraw_count = 0;
    g_blitraw_log_on = 1;
    g_rle_blit_calls = 0;
    g_rle_blit_log_on = 1;
    g_dlg_glyph_calls = 0;
    return sheet;
}

/* mark spell `id` (0..39) learned for g_test_rc_array[0]. */
static void dssl_learn(int id)
{
    g_test_rc_array[0].spells_known_bitmap[id >> 3] |= (uint8)(1 << (id & 7));
}

/* expected MP-icon dst for a cell at list index `iter` over render_buf. */
static uint32 dssl_icon_dst(uint32 render_buf, int iter)
{
    uint32 row_pixel = (uint32)((iter % 4) * 0x16);
    uint32 col_off   = render_buf + (uint32)(iter / 4) * 100 + 0x12;
    return col_off + 0x32 + (row_pixel + 0x6c) * 0x140;
}

/* expected MP-cost number dst for a cell at list index `iter`. */
static uint32 dssl_num_dst(uint32 render_buf, int iter)
{
    uint32 row_pixel = (uint32)((iter % 4) * 0x16);
    uint32 col_off   = render_buf + (uint32)(iter / 4) * 100 + 0x12;
    return col_off + 0x49 + (row_pixel + 0x6c) * 0x140;
}

/* assert the 2-digit decimal `value` (00..99) renders as two glyphs at `dst`
 * starting at rle-log index `from`, sprite base 0x2A, 6px apart. The renderer
 * uses "%0.2d" so each digit glyph = base + digit_char - '0'. */
static void dssl_assert_2digit(int from, uint32 dst, uint32 value)
{
    uint32 d0 = (value / 10) % 10;
    uint32 d1 = value % 10;

    ASSERT_EQ((long)(g_rle_blit_log_sprite[from + 0] -
                     data_fd2_ui_anim_sprite_sheet_ptr),
              (long)(0x2a + d0));
    ASSERT_EQ((long)g_rle_blit_log_dst[from + 0], (long)dst);
    ASSERT_EQ((long)(g_rle_blit_log_sprite[from + 1] -
                     data_fd2_ui_anim_sprite_sheet_ptr),
              (long)(0x2a + d1));
    ASSERT_EQ((long)g_rle_blit_log_dst[from + 1], (long)(dst + 6));
}

/* Local runtime-char fixture; the global array pointer is repointed at it for
 * the duration of each test and restored afterwards (no cross-suite pollution
 * of the shared g_test_rc_array). */
static runtime_char bsl_chars[8];
static runtime_char *bsl_saved_ptr;

static void bsl_setup(void)
{
    bsl_saved_ptr = data_fd2_battle_runtime_char_array_ptr;
    memset(bsl_chars, 0, sizeof(bsl_chars));
    data_fd2_battle_runtime_char_array_ptr = bsl_chars;
}

static void bsl_teardown(void)
{
    data_fd2_battle_runtime_char_array_ptr = bsl_saved_ptr;
}

/* Empty bitmap -> count 0, and out_buf is not touched. */
static void test_bsl_empty(void)
{
    uint8 out[40];
    int n;

    bsl_setup();
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 0);
    ASSERT_EQ((int)out[0], 0xCC);            /* untouched */
    bsl_teardown();
}

/* Count-only mode (out_buf == NULL): returns the count without writing. Three
 * bits set in byte 0 -> count 3. */
static void test_bsl_count_only(void)
{
    int n;

    bsl_setup();
    bsl_chars[0].spells_known_bitmap[0] = 0x07;   /* bits 0,1,2 */
    n = fd2_build_usable_spell_list(0, 0);
    ASSERT_EQ(n, 3);
    bsl_teardown();
}

/* Write mode, byte 0: ids equal the bit index. bits 0,2,5 set ->
 * ids {0,2,5}, count 3, ascending order. */
static void test_bsl_byte0_ids(void)
{
    uint8 out[40];
    int n;

    bsl_setup();
    bsl_chars[0].spells_known_bitmap[0] = 0x25;   /* bits 0,2,5 (0x01|0x04|0x20) */
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 3);
    ASSERT_EQ((int)out[0], 0);
    ASSERT_EQ((int)out[1], 2);
    ASSERT_EQ((int)out[2], 5);
    ASSERT_EQ((int)out[3], 0xCC);            /* nothing written past count */
    bsl_teardown();
}

/* Bit-position math across bytes: id = byte*8 + bit. byte 1 bit 3 -> 11,
 * byte 3 bit 0 -> 24, byte 4 bit 7 -> 39 (the highest reachable slot). The
 * three set bytes also prove every one of the 5 bytes is scanned. */
static void test_bsl_cross_byte_ids(void)
{
    uint8 out[40];
    int n;

    bsl_setup();
    bsl_chars[0].spells_known_bitmap[1] = 1 << 3;  /* id 1*8+3 = 11 */
    bsl_chars[0].spells_known_bitmap[3] = 1 << 0;  /* id 3*8+0 = 24 */
    bsl_chars[0].spells_known_bitmap[4] = 1 << 7;  /* id 4*8+7 = 39 */
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 3);
    ASSERT_EQ((int)out[0], 11);
    ASSERT_EQ((int)out[1], 24);
    ASSERT_EQ((int)out[2], 39);
    bsl_teardown();
}

/* Loop bound is exactly 5 bytes: a byte-5 (offset +0x1F = archetype_flag) all
 * ones must NOT be enumerated. Set every spells_known_bitmap bit (5 bytes =
 * 40 ids 0..39) and fully populate the adjacent archetype_flag byte; the count
 * stays 40 and the last id is 39. */
static void test_bsl_loop_bound_five_bytes(void)
{
    uint8 out[64];
    int n;
    int i;

    bsl_setup();
    for (i = 0; i < 5; i++) {
        bsl_chars[0].spells_known_bitmap[i] = 0xFF;
    }
    bsl_chars[0].archetype_flag = 0xFF;          /* +0x1F, must be ignored */
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 40);
    ASSERT_EQ((int)out[0], 0);
    ASSERT_EQ((int)out[39], 39);                 /* 4*8+7, last real slot */
    ASSERT_EQ((int)out[40], 0xCC);               /* no 41st id from byte 5 */
    bsl_teardown();
}

/* char_idx selects the right struct (stride 0x50): index 0 is empty, index 3
 * carries the spells, querying 3 returns its list and querying 0 returns 0. */
static void test_bsl_char_index_stride(void)
{
    uint8 out[40];
    int n;

    bsl_setup();
    bsl_chars[3].spells_known_bitmap[0] = 0x02;   /* bit 1 -> id 1 */
    bsl_chars[3].spells_known_bitmap[2] = 0x01;   /* bit 0 -> id 16 */

    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(3, (uint32)out);
    ASSERT_EQ(n, 2);
    ASSERT_EQ((int)out[0], 1);
    ASSERT_EQ((int)out[1], 16);

    n = fd2_build_usable_spell_list(0, 0);        /* index 0 still empty */
    ASSERT_EQ(n, 0);
    bsl_teardown();
}

/* ================================================================
 * fd2_draw_spell_selection_list @ 0x1ceed
 * ================================================================ */

/* no spells learned -> spell_count 0 -> the loop never runs: no MP-icon blit,
 * no MP-cost digits, no name-label glyph. Proves the loop bound. */
static void test_dssl_no_spells_draws_nothing(void)
{
    uint32 buf = 0x100000;

    dssl_setup();

    fd2_draw_spell_selection_list(0, (uint32)-1, buf);

    ASSERT_EQ((long)g_blitraw_count, 0);
    ASSERT_EQ((long)g_rle_blit_calls, 0);
    ASSERT_EQ((long)g_dlg_glyph_calls, 0);     /* immediate-END text: no glyph */
}

/* SKIP (Phase 3): the dssl_* tests below write now-const data_fd2_battle_spell_effect_table; restore + rewrite to drive real data */
#if 0
/* one learned spell at list index 0: the cell draws the MP icon (sprite 0x5C)
 * and the 2-digit MP cost (read from spell record +5). With iter 0 the cell is
 * at col_addr_offset = buf + 0x12, row_pixel 0. Validates the core offset math
 * and the pSpell -> *(pSpell+5) MP-cost read (the post-CALL EAX path). */
static void test_dssl_single_spell_cell0(void)
{
    uint32 buf = 0x100000;
    uint32 sheet;

    sheet = dssl_setup();
    dssl_learn(5);                                /* spell id 5 */
    data_fd2_battle_spell_effect_table[5].mp_cost = 0x11;  /* 17 */

    fd2_draw_spell_selection_list(0, (uint32)-1, buf);

    /* one MP-icon blit: sprite 0x5C at the cell icon offset */
    ASSERT_EQ((long)g_blitraw_count, 1);
    ASSERT_EQ((long)(g_blitraw_log_sprite[0] - sheet), 0x5c);
    ASSERT_EQ((long)g_blitraw_log_dst[0], (long)dssl_icon_dst(buf, 0));

    /* MP cost 17 -> two glyphs "17" at the number offset, base 0x2A */
    ASSERT_EQ((long)g_rle_blit_calls, 2);
    dssl_assert_2digit(0, dssl_num_dst(buf, 0), 17);
}

/* the spell name page is spell_id + 0x1B9: build a spell list of one id and
 * confirm the dialog VM is entered at page (id + 0x1B9). We point ONLY that
 * page at a one-glyph program (all others at END) and assert exactly one glyph
 * was rendered -- proving the page index the renderer used. */
static void test_dssl_name_page_is_id_plus_0x1b9(void)
{
    uint32 buf = 0x100000;
    int    id  = 0x12;
    int    i;

    dssl_setup();
    dssl_learn(id);
    data_fd2_battle_spell_effect_table[id].mp_cost = 3;

    /* rebuild the text table: every page -> END except (id + 0x1B9) -> glyph */
    for (i = 0; i < DSSL_TEXT_WORDS; i++) {
        dssl_text[i] = 0;
    }
    *(int16 *)((uint8 *)dssl_text + DSSL_END_OFF)     = -1;      /* END pair... */
    *(int16 *)((uint8 *)dssl_text + DSSL_END_OFF + 4) = 0x0010;  /* glyph */
    *(int16 *)((uint8 *)dssl_text + DSSL_END_OFF + 6) = -1;      /* then END */
    for (i = 0; i < (DSSL_END_OFF / 2); i++) {
        dssl_text[i] = (uint16)DSSL_END_OFF;                     /* -> END */
    }
    dssl_text[id + 0x1b9] = (uint16)(DSSL_END_OFF + 4);          /* -> glyph */
    data_fd2_all_game_text_ptr = (uint32)(uint8 *)dssl_text;

    fd2_draw_spell_selection_list(0, (uint32)-1, buf);

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);       /* only the id+0x1B9 page drew */
}

/* highlight branch: the cell whose list index == highlighted_idx gets border
 * color 0xC9 (yellow); all others 0xCD (red). The color is the page_idx arg of
 * fd2_display_dialog_scene, forwarded to the glyph blitter as p5. Render one
 * glyph and read g_dlg_glyph_last_p5 for the highlighted vs non-highlighted
 * single-spell case. */
static void test_dssl_highlight_color_yellow(void)
{
    uint32 buf = 0x100000;

    dssl_setup();
    dssl_setup_text_one_glyph();
    dssl_learn(7);
    data_fd2_battle_spell_effect_table[7].mp_cost = 9;

    fd2_draw_spell_selection_list(0, 0, buf);    /* index 0 highlighted */

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xc9);  /* highlighted -> yellow */
}

static void test_dssl_highlight_color_red_when_not_selected(void)
{
    uint32 buf = 0x100000;

    dssl_setup();
    dssl_setup_text_one_glyph();
    dssl_learn(7);
    data_fd2_battle_spell_effect_table[7].mp_cost = 9;

    fd2_draw_spell_selection_list(0, 1, buf);    /* index 1 highlighted, none exists */

    ASSERT_EQ((long)g_dlg_glyph_calls, 1);
    ASSERT_EQ((long)g_dlg_glyph_last_p5, 0xcd);  /* not selected -> red */
}

/* grid layout: 5 learned spells fill list indices 0..4. The first 4 stack
 * vertically in page-column 0 (row_pixel 0,0x16,0x2C,0x42; col_addr_offset =
 * buf+0x12), and index 4 wraps to page-column 1 (col_addr_offset += 100,
 * row_pixel back to 0). Validates the (iter/4) page step and (iter%4) row step
 * via the MP-icon dst of every cell. */
static void test_dssl_grid_4col_wrap(void)
{
    uint32 buf = 0x100000;
    uint32 sheet;
    int    i;

    sheet = dssl_setup();
    /* learn ids 0,1,2,3,4 (build_usable_spell_list emits them ascending) */
    for (i = 0; i < 5; i++) {
        dssl_learn(i);
        data_fd2_battle_spell_effect_table[i].mp_cost = (uint8)(10 + i);
    }

    fd2_draw_spell_selection_list(0, (uint32)-1, buf);

    /* 5 cells -> 5 MP-icon blits (all sprite 0x5C) and 10 MP-cost glyphs */
    ASSERT_EQ((long)g_blitraw_count, 5);
    ASSERT_EQ((long)g_rle_blit_calls, 10);

    for (i = 0; i < 5; i++) {
        ASSERT_EQ((long)(g_blitraw_log_sprite[i] - sheet), 0x5c);
        ASSERT_EQ((long)g_blitraw_log_dst[i], (long)dssl_icon_dst(buf, i));
        dssl_assert_2digit(i * 2, dssl_num_dst(buf, i), 10 + i);
    }

    /* explicit cross-check of the wrap: index 3 and index 4 share neither row
     * nor page-column. index 3: page 0, row 3 (row_pixel 0x42); index 4: page
     * 1 (col offset +100), row 0 (row_pixel 0). */
    ASSERT_EQ((long)g_blitraw_log_dst[3],
              (long)(buf + 0x12 + 0x32 + ((uint32)(3 * 0x16) + 0x6c) * 0x140));
    ASSERT_EQ((long)g_blitraw_log_dst[4],
              (long)(buf + 100 + 0x12 + 0x32 + (0x6cu) * 0x140));
}

/* the MP cost is read from the spell record at +5 (mp_cost), per spell id --
 * not a fixed slot. Two spells with distinct costs render distinct digit runs
 * at their own cells. */
static void test_dssl_mp_cost_per_spell(void)
{
    uint32 buf = 0x100000;

    dssl_setup();
    dssl_learn(2);
    dssl_learn(9);
    data_fd2_battle_spell_effect_table[2].mp_cost = 4;   /* id 2 -> "04" */
    data_fd2_battle_spell_effect_table[9].mp_cost = 25;  /* id 9 -> "25" */

    fd2_draw_spell_selection_list(0, (uint32)-1, buf);

    ASSERT_EQ((long)g_blitraw_count, 2);
    ASSERT_EQ((long)g_rle_blit_calls, 4);
    dssl_assert_2digit(0, dssl_num_dst(buf, 0), 4);     /* id 2 at index 0 */
    dssl_assert_2digit(2, dssl_num_dst(buf, 1), 25);    /* id 9 at index 1 */
}

/* caster_idx selects the runtime_char (stride 0x50): the spell list comes from
 * char[caster_idx], so a spell on char 3 is drawn only when caster_idx == 3. */
static void test_dssl_caster_idx_selects_char(void)
{
    uint32 buf = 0x100000;

    dssl_setup();
    g_test_rc_array[3].spells_known_bitmap[0] = 0x01;       /* id 0 on char 3 */
    data_fd2_battle_spell_effect_table[0].mp_cost = 8;

    fd2_draw_spell_selection_list(0, (uint32)-1, buf);      /* char 0 empty */
    ASSERT_EQ((long)g_blitraw_count, 0);

    g_blitraw_count = 0;
    g_rle_blit_calls = 0;
    fd2_draw_spell_selection_list(3, (uint32)-1, buf);      /* char 3 has it */
    ASSERT_EQ((long)g_blitraw_count, 1);
    dssl_assert_2digit(0, dssl_num_dst(buf, 0), 8);
}
#endif /* SKIP (Phase 3): dssl_* tests write now-const data_fd2_battle_spell_effect_table */

/* ================================================================
 * fd2_spell_selection_menu_main @ 0x1cff0
 * ================================================================
 *
 * Behavioral coverage deferred to Phase 9 integration. The function is a
 * heavy-UI battle modal: it allocates the three slide buffers, snapshots and
 * restores the VGA frame at 0xA0000, renders the status layout + spell list,
 * runs slide-in/out animations, and drives its own input loop
 * (fd2_spell_select_input_loop) followed by target-pick prompts
 * (fd2_compute_aoe_targets / fd2_wait_for_action_target_input). Every code path
 * past the menu-cancel early-out blocks on a real keyboard read with no async
 * key source in the host harness, so the spell-dispatch branch selection
 * (single / AoE / teleport) and the cast-vs-status-handler dispatch -- including
 * the per-spell status handler table call (caster_idx, n_targets, target_buf)
 * and the cast-sequence target count, both reconstructed from the disassembly
 * past the decompiler's dropped-argument CALLs -- cannot be exercised
 * in-process. The same non-isolability defers this modal's inline-dispatcher
 * Spell branch (tests/ui_menu/menu.c) and the sibling status-overview modal's
 * full input flow. Correctness here rests on the strict 3-source (plate / disasm
 * / decomp) review recorded in src/spell/spellsel.c.
 */

/* ================================================================
 * fd2_spell_select_input_loop @ 0x1d51d
 * ================================================================
 *
 * One frame of spell-picker input. Drives the REAL renderer
 * (fd2_draw_spell_selection_list, set up via dssl_setup) and the REAL key wait
 * (fd2_wait_for_input_dialog_with_blink), fed a single scancode pre-armed in
 * the BIOS keyboard buffer (sil_inject_scancode) which the wait reads on its
 * first poll -- this function never clears the buffer, so the pre-armed key
 * survives. The SFX callee fd2_play_sfx_with_handle is the recording fake
 * (g_play_sfx_with_handle_calls). Each case pins the cursor-navigation
 * arithmetic (the EAX-tracking-prone, high-value logic) and the return value
 * for one dispatch branch: Up/Down with wrap, Left/Right with their
 * row/spell_count bounds, the Enter/Space commit with the MP-cost gate
 * (*(byte*)(pSpell+5) <= caster.mp_current, a post-CALL EAX read), Esc cancel,
 * and the unhandled-key fall-to-0.
 *
 * spell_count = number of learned bits (dssl_learn ids 0..N-1 -> count N).
 * The cursor lives in data_fd2_ui_menu_cursor_idx; the spell records (mp_cost
 * @ +5) are data_fd2_battle_spell_effect_table, resolved by the REAL
 * fd2_get_spell_effect_entry. */

/* SKIP (Phase 3): sil_setup writes now-const data_fd2_battle_spell_effect_table; the whole sil_* block (helpers + tests) is skipped with it; restore + rewrite to drive real data */
#if 0
/* Pre-arm one scancode in the BIOS keyboard buffer (BDA @ 0x400) so the real
 * wait exits on its first poll with AH=scancode. Mirrors status.c's helper. */
static void sil_inject_scancode(int scancode)
{
    *(volatile uint16 *)0x41AuL = 0x1E;                          /* head        */
    *(volatile uint16 *)0x41CuL = 0x20;                          /* tail=head+2 */
    *(volatile uint16 *)0x41EuL = (uint16)((scancode << 8) & 0xFF00);
}

/* Stand up the renderer fixture, learn ids 0..n_spells-1 on char 0 (so
 * spell_count == n_spells), set the cursor and arm the scancode. */
static void sil_setup(int n_spells, int cursor, int scancode)
{
    int i;

    dssl_setup();
    for (i = 0; i < n_spells; i++) {
        dssl_learn(i);
        data_fd2_battle_spell_effect_table[i].mp_cost = (uint8)(1 + i);
    }
    data_fd2_ui_menu_cursor_idx = (uint32)cursor;
    g_play_sfx_with_handle_calls = 0;
    sil_inject_scancode(scancode);
}

/* Up, no wrap: cursor 3 -> 2, SFX fires, returns 0. */
static void test_sil_up_decrement(void)
{
    int r;
    sil_setup(8, 3, 0x48);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
}

/* Up, wrap: cursor 0, spell_count 5 -> spell_count-1 = 4, SFX, returns 0. */
static void test_sil_up_wrap_to_last(void)
{
    int r;
    sil_setup(5, 0, 0x48);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 4);   /* 5 - 1 */
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
}

/* Down, no wrap: cursor 2, spell_count 8 (last=7) -> 3, SFX, returns 0. */
static void test_sil_down_increment(void)
{
    int r;
    sil_setup(8, 2, 0x50);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 3);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
}

/* Down, wrap: cursor at last (spell_count-1 = 4 with spell_count 5) -> 0, SFX,
 * returns 0. Pins the wrap condition (cursor == spell_count - 1). */
static void test_sil_down_wrap_to_zero(void)
{
    int r;
    sil_setup(5, 4, 0x50);                /* cursor == spell_count - 1 */
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 0);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
}

/* Left, valid: cursor 5 (>= 4) -> 1 (cursor - 4), SFX, returns 0. */
static void test_sil_left_valid(void)
{
    int r;
    sil_setup(8, 5, 0x4b);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 1);   /* 5 - 4 */
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
}

/* Left, invalid: cursor 2 (< 4) -> unchanged, NO SFX, returns 0 (top row). */
static void test_sil_left_invalid_top_row(void)
{
    int r;
    sil_setup(8, 2, 0x4b);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);   /* no change */
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);  /* no move -> no SFX */
}

/* Right, valid: cursor 1, spell_count 8 (1 < 8-4 = 4) -> 5, SFX, returns 0. */
static void test_sil_right_valid(void)
{
    int r;
    sil_setup(8, 1, 0x4d);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 5);   /* 1 + 4 */
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 1);
}

/* Right, invalid: cursor 5, spell_count 8 (5 >= 8-4 = 4) -> unchanged, NO SFX,
 * returns 0. Pins the bound cursor < spell_count - 4. */
static void test_sil_right_invalid_bottom(void)
{
    int r;
    sil_setup(8, 5, 0x4d);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 5);   /* no change */
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* Enter (0x1C), MP sufficient: caster mp_current 20 >= picked spell mp_cost.
 * Returns 1 (commit) and leaves the cursor where it is. Exercises the
 * post-CALL EAX MP-cost read (*(byte*)(pSpell+5)). spell ids 0..3 learned,
 * cursor 2 -> spell_id_list[2] == id 2, mp_cost set to 5. */
static void test_sil_enter_commit_mp_ok(void)
{
    int r;
    sil_setup(4, 2, 0x1c);
    data_fd2_battle_spell_effect_table[2].mp_cost = 5;
    g_test_rc_array[0].mp_current = 20;
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)r, 1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);   /* cursor unchanged */
}

/* Enter, MP boundary equal: mp_current == mp_cost -> still commits (the gate is
 * mp_cost <= caster_MP). cost 9, MP 9 -> return 1. */
static void test_sil_enter_commit_mp_equal(void)
{
    int r;
    sil_setup(4, 1, 0x1c);
    data_fd2_battle_spell_effect_table[1].mp_cost = 9;
    g_test_rc_array[0].mp_current = 9;
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)r, 1);
}

/* Enter, MP insufficient: mp_current 3 < mp_cost 8 -> NOT selectable, return 0
 * (stay in loop), cursor unchanged. */
static void test_sil_enter_blocked_mp_low(void)
{
    int r;
    sil_setup(4, 2, 0x1c);
    data_fd2_battle_spell_effect_table[2].mp_cost = 8;
    g_test_rc_array[0].mp_current = 3;
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
}

/* Space (0x39) behaves exactly like Enter: MP-ok commit returns 1. */
static void test_sil_space_commits_like_enter(void)
{
    int r;
    sil_setup(4, 0, 0x39);
    data_fd2_battle_spell_effect_table[0].mp_cost = 2;
    g_test_rc_array[0].mp_current = 50;
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)r, 1);
}

/* Esc (0x01): returns -1, cursor untouched, no SFX. */
static void test_sil_esc_cancels(void)
{
    int r;
    sil_setup(4, 2, 0x01);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)r, -1);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* Unhandled key (e.g. 0x10 'Q'): no move, no SFX, returns 0. */
static void test_sil_unhandled_key_returns_zero(void)
{
    int r;
    sil_setup(4, 2, 0x10);
    r = fd2_spell_select_input_loop(0);
    ASSERT_EQ((long)r, 0);
    ASSERT_EQ((long)data_fd2_ui_menu_cursor_idx, 2);
    ASSERT_EQ((long)g_play_sfx_with_handle_calls, 0);
}

/* caster_idx selects the runtime_char (stride 0x50) for the MP check: put a
 * cheap spell on char 3 and a high MP on char 3, query caster_idx 3 -> commit.
 * Validates pCharArray[caster_idx].mp_current and the per-char spell list. */
static void test_sil_caster_idx_mp_from_right_char(void)
{
    int r;
    dssl_setup();                                /* char 0 empty */
    memset(&g_test_rc_array[3], 0, sizeof(g_test_rc_array[3]));
    g_test_rc_array[3].spells_known_bitmap[0] = 0x01;  /* id 0 on char 3 */
    data_fd2_battle_spell_effect_table[0].mp_cost = 4;
    g_test_rc_array[3].mp_current = 30;
    data_fd2_ui_menu_cursor_idx = 0;
    g_play_sfx_with_handle_calls = 0;
    sil_inject_scancode(0x1c);
    r = fd2_spell_select_input_loop(3);
    ASSERT_EQ((long)r, 1);                        /* char 3 MP covers cost */
}
#endif /* SKIP (Phase 3): sil_* block writes now-const data_fd2_battle_spell_effect_table */

/* ---- Tests: fd2_play_spell_palette_flash_with_sfx (0x1D6C8) ----
 *
 * The function's only host-observable seam is the single SFX trigger
 * (fd2_play_sfx_with_handle, counted by g_play_sfx_with_handle_calls); the
 * R/G/B writes go to the VGA DAC via outp(), which -- like every other
 * outp-only palette routine in this harness (see tests/gfx/palette.c) -- is a
 * hardware side effect that cannot be captured in-process, so those are
 * smoke-level. The table-index math (+0, +0x24, +0x48 into the 108-byte
 * data_fd2_animation_spell_palette_flash_table) is pinned by exercising the
 * spell-id domain endpoints, which read the table's lowest and highest in-
 * bounds bytes.
 *
 * fd2_wait_n_bios_ticks(1) is called 8x per invocation; it spins on the live
 * BIOS tick at 0x46C (advances ~18.2/s under the harness), so each call
 * returns after ~1 real tick -- deterministic termination, not frozen. */
static void test_psf_sfx_fires_once(void)
{
    g_play_sfx_with_handle_calls = 0;
    fd2_play_spell_palette_flash_with_sfx(9);
    /* SFX is triggered once before the 4-iteration flash loop, NOT per
     * iteration: exactly one call regardless of loop count. */
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
}

static void test_psf_id_domain_endpoints(void)
{
    /* pid=0 reads table[0]/[0x24]/[0x48]; pid=0x23 (max spell id) reads
     * table[0x23]/[0x47]/[0x6B] -- index 0x6B == 107 is the last byte of the
     * 108-byte table. Driving both endpoints proves the +0x24/+0x48 plane
     * offsets stay in-bounds across the whole spell-id range. Each call must
     * fire the SFX once and return cleanly. */
    g_play_sfx_with_handle_calls = 0;
    fd2_play_spell_palette_flash_with_sfx(0);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);

    g_play_sfx_with_handle_calls = 0;
    fd2_play_spell_palette_flash_with_sfx(0x23);
    ASSERT_EQ(g_play_sfx_with_handle_calls, 1);
}

/* ================================================================
 * fd2_grant_spell_to_char @ 0x1d79c
 * ================================================================
 *
 * Set spell_id's bit in char[char_idx].spells_known_bitmap (+0x1A):
 *   spells_known_bitmap[spell_id / 8] |= 1 << (spell_id % 8)
 * Pure in-memory bit manipulation -- pinned directly. Reuses the bsl_* runtime-
 * char fixture (local bsl_chars repointed for the test). Cross-validates layout
 * against the sibling enumerator fd2_build_usable_spell_list. */

/* grant one spell into byte 0 -> only that bit set, count is 1. id 5 lands in
 * byte 0 bit 5 (mask 0x20). */
static void test_grant_byte0_bit(void)
{
    bsl_setup();
    fd2_grant_spell_to_char(0, 5);
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[0], 0x20);  /* 1 << 5 */
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[1], 0);
    bsl_teardown();
}

/* byte-offset / bit math across the 5 bytes: id = byte*8 + bit, so
 * 11 -> byte 1 bit 3, 24 -> byte 3 bit 0, 0x23 (35, max spell id) -> byte 4
 * bit 3. Each lands in its own byte with the right single-bit mask. */
static void test_grant_byte_offset_math(void)
{
    bsl_setup();
    fd2_grant_spell_to_char(0, 11);     /* 11/8=1, 11%8=3 -> byte 1, 1<<3 */
    fd2_grant_spell_to_char(0, 24);     /* 24/8=3, 24%8=0 -> byte 3, 1<<0 */
    fd2_grant_spell_to_char(0, 0x23);   /* 35/8=4, 35%8=3 -> byte 4, 1<<3 */
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[0], 0);
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[1], 1 << 3);
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[2], 0);
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[3], 1 << 0);
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[4], 1 << 3);
    bsl_teardown();
}

/* OR semantics: a second grant into the same byte preserves existing bits. ids
 * 1 and 6 share byte 0 -> the byte holds both bits (0x02 | 0x40 = 0x42), not
 * just the last. */
static void test_grant_or_preserves_existing(void)
{
    bsl_setup();
    fd2_grant_spell_to_char(0, 1);
    fd2_grant_spell_to_char(0, 6);
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[0], 0x42);  /* (1<<1)|(1<<6) */
    bsl_teardown();
}

/* a pre-existing unrelated bit is not disturbed by a grant into the same byte:
 * seed bit 0, grant id 4 -> byte holds 0x01 | 0x10 = 0x11. */
static void test_grant_keeps_seeded_bit(void)
{
    bsl_setup();
    bsl_chars[0].spells_known_bitmap[0] = 0x01;   /* bit 0 already learned */
    fd2_grant_spell_to_char(0, 4);
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[0], 0x11);  /* 0x01|0x10 */
    bsl_teardown();
}

/* idempotent: granting the same spell twice leaves exactly one bit set. */
static void test_grant_idempotent(void)
{
    bsl_setup();
    fd2_grant_spell_to_char(0, 9);
    fd2_grant_spell_to_char(0, 9);     /* 9/8=1, 9%8=1 -> byte 1 bit 1 */
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[1], 1 << 1);
    bsl_teardown();
}

/* char_idx selects the struct (stride 0x50): grant on char 3 only sets char 3's
 * bitmap; char 0 stays empty. */
static void test_grant_char_index_stride(void)
{
    bsl_setup();
    fd2_grant_spell_to_char(3, 7);     /* byte 0 bit 7 on char 3 */
    ASSERT_EQ((int)bsl_chars[3].spells_known_bitmap[0], 0x80);  /* 1 << 7 */
    ASSERT_EQ((int)bsl_chars[0].spells_known_bitmap[0], 0);     /* char 0 empty */
    bsl_teardown();
}

/* cross-validate the bit layout against the enumerator: grant a set of ids,
 * then fd2_build_usable_spell_list must report exactly those ids in ascending
 * order. Proves grant's byte/bit packing matches the reader's. */
static void test_grant_roundtrips_through_enumerator(void)
{
    uint8 out[40];
    int   n;

    bsl_setup();
    fd2_grant_spell_to_char(0, 0);
    fd2_grant_spell_to_char(0, 13);
    fd2_grant_spell_to_char(0, 0x23);
    memset(out, 0xCC, sizeof(out));
    n = fd2_build_usable_spell_list(0, (uint32)out);
    ASSERT_EQ(n, 3);
    ASSERT_EQ((int)out[0], 0);
    ASSERT_EQ((int)out[1], 13);
    ASSERT_EQ((int)out[2], 0x23);
    bsl_teardown();
}

void run_spell_spellsel_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: spell/spellsel\n");
    RUN_TEST(test_bsl_empty);
    RUN_TEST(test_bsl_count_only);
    RUN_TEST(test_bsl_byte0_ids);
    RUN_TEST(test_bsl_cross_byte_ids);
    RUN_TEST(test_bsl_loop_bound_five_bytes);
    RUN_TEST(test_bsl_char_index_stride);
    RUN_TEST(test_dssl_no_spells_draws_nothing);
#if 0 /* SKIP (Phase 3): dssl_* + sil_* tests write now-const data_fd2_battle_spell_effect_table */
    RUN_TEST(test_dssl_single_spell_cell0);
    RUN_TEST(test_dssl_name_page_is_id_plus_0x1b9);
    RUN_TEST(test_dssl_highlight_color_yellow);
    RUN_TEST(test_dssl_highlight_color_red_when_not_selected);
    RUN_TEST(test_dssl_grid_4col_wrap);
    RUN_TEST(test_dssl_mp_cost_per_spell);
    RUN_TEST(test_dssl_caster_idx_selects_char);
    RUN_TEST(test_sil_up_decrement);
    RUN_TEST(test_sil_up_wrap_to_last);
    RUN_TEST(test_sil_down_increment);
    RUN_TEST(test_sil_down_wrap_to_zero);
    RUN_TEST(test_sil_left_valid);
    RUN_TEST(test_sil_left_invalid_top_row);
    RUN_TEST(test_sil_right_valid);
    RUN_TEST(test_sil_right_invalid_bottom);
    RUN_TEST(test_sil_enter_commit_mp_ok);
    RUN_TEST(test_sil_enter_commit_mp_equal);
    RUN_TEST(test_sil_enter_blocked_mp_low);
    RUN_TEST(test_sil_space_commits_like_enter);
    RUN_TEST(test_sil_esc_cancels);
    RUN_TEST(test_sil_unhandled_key_returns_zero);
    RUN_TEST(test_sil_caster_idx_mp_from_right_char);
#endif
    RUN_TEST(test_psf_sfx_fires_once);
    RUN_TEST(test_psf_id_domain_endpoints);
    RUN_TEST(test_grant_byte0_bit);
    RUN_TEST(test_grant_byte_offset_math);
    RUN_TEST(test_grant_or_preserves_existing);
    RUN_TEST(test_grant_keeps_seeded_bit);
    RUN_TEST(test_grant_idempotent);
    RUN_TEST(test_grant_char_index_stride);
    RUN_TEST(test_grant_roundtrips_through_enumerator);
    printf("\n");
}
