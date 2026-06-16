/*
 * testglob.c — Fake global definitions for all test suites
 */
#include "types.h"
#include "globals.h"
#include "blitprob.h"
#include <stdlib.h>
#include <stdio.h>

/* Per-test heartbeat for build_test.py's hang detector. Each test writes its
 * name (with a monotonically increasing seq so the content always changes) to
 * E:\OUT\HB.TXT via fopen/fprintf/FCLOSE — the close is what forces DOSBox to
 * commit the write to the host file, so the host-side poller sees it live
 * (an in-program fflush alone does NOT propagate under DOSBox local-drive
 * caching). If a test hangs, HB.TXT freezes on its name -> the poller can both
 * detect the stall and report exactly which test hung. */
void test_heartbeat(const char *name)
{
    static unsigned long hb_seq = 0;
    FILE *f = fopen("E:\\OUT\\HB.TXT", "w");
    if (f) {
        fprintf(f, "%lu %s\n", ++hb_seq, name);
        fclose(f);
    }
}

runtime_char  g_test_rc_array[8];
uint32 data_ail_alloc_fnptr = 0;
uint32 data_ail_free_fnptr = 0;
void fd2_execute_offensive_targeted_spell(int a, int b, int c, int d) { }
void fd2_execute_offensive_full_screen_flash_spell(int a, int b, int c, int d) { }
/* fd2_dispatch_variant_b_cast: now emitted for real in src/spell/spellcin.c;
 * its former no-op stub here was removed. */
void fd2_cast_ap_boost_spell(int a, int b, uint8 *c) { }
void fd2_cast_dp_boost_spell(int a, int b, uint32 c) { }
void fd2_cast_speed_boost_spell(uint32 a, uint32 b, uint32 c) { }
/* fd2_cast_earthquake_spell_with_screen_shake / fd2_cast_screen_wide_spell_with_fade /
 * fd2_cinematic_chapter_portrait_dump_with_white_flash / fd2_dispatch_variant_b_cast:
 * no-op stubs retained here; the real bodies (where emitted) supersede them at link. */
void fd2_cast_earthquake_spell_with_screen_shake(int a, int b, int c, uint8 *d) { }
void fd2_cast_screen_wide_spell_with_fade(uint32 a, uint32 b, uint32 c, int d) { }
void fd2_cinematic_chapter_portrait_dump_with_white_flash(uint32 a, uint32 b, uint32 c) { }
void fd2_dispatch_variant_b_cast(int a, int b, int c, int d) { }
/* crt_equivalent_matherr_default_return_zero_4d8ea @ 0x4d8ea is now emitted as
 * the real "return 0" primitive in src/crt/crt.c; its earlier link-only stub
 * and the g_matherr_return_zero_entered counter have been removed. The thunk
 * tests now observe the thunk's return value (0, produced by the real
 * primitive) directly. */
int g_play_sfx_with_handle_calls = 0;
int g_dlg_blink_calls = 0;
/* Per-invocation counter for the real fd2_play_sfx_sample_from_bank
 * (src/audio/audio.c). Defined here ahead of the AIL_stop_sample spy that
 * increments it; see the relocation note near the removed stub below for the
 * handle-based routing rationale. */
int g_play_sfx_sample_from_bank_calls = 0;
/* SFX-id capture log: fd2_animate_spell_impact_per_target's per-spell SFX
 * dispatch chain is the highest-risk control flow in that function, so its
 * test pins the exact sequence of fired SFX ids (arg b) per frame. Additive:
 * existing tests only read g_play_sfx_with_handle_calls. */
int    g_sfx_last_id = 0;
int    g_sfx_id_count = 0;
int    g_sfx_id_log[64];
/* Additive arg-capture for fd2_play_and_free_status_effect_sfx's test: it must
 * forward the bank handle (arg a) and the trailing flag (arg c). Existing tests
 * only read g_sfx_last_id / g_play_sfx_with_handle_calls, so adding these is
 * non-breaking. */
uint32 g_sfx_last_arg_a = 0;
int    g_sfx_last_arg_c = 0;
/* Additive: captures the AIL sample handle (slot) the real player programmed, so
 * fd2_play_sfx_sample_from_bank's direct test can assert it drives slot 1
 * (data_fd2_audio_sfx_sample_handle_1) rather than slot 0. Non-breaking: existing
 * tests don't read it. */
uint32 g_sfx_last_handle = 0;
/* fd2_play_sfx_with_handle is now a real emitted function (src/audio/audio.c);
 * its former counting stub here was removed. The SFX spy seam relocates one
 * level down into the AIL_* sample stubs the real player drives, mirroring the
 * fd2_blit_rectangle -> fd2_composite_battle_tile_map relocation done earlier.
 *
 * The real player runs its three audio gates, then unconditionally calls
 * AIL_stop_sample once per invocation (both the stop-only sfx_id==-1 path and
 * the normal play path) -> that is the host-observable per-call counter
 * (g_play_sfx_with_handle_calls / g_dlg_blink_calls). On the normal path it
 * then calls AIL_set_sample_address(handle, base+off(id), end(id)-off(id)):
 * caller tests stage a sfx bank (audiofix_make_bank) whose per-id offsets are
 * laid out so that length == sfx_id and start == base + off(id). The spy
 * therefore records the recovered sfx id in g_sfx_last_id / g_sfx_id_log (the
 * captured length) and the resolved sample start in g_sfx_last_arg_a. The
 * loop_count argument is recovered from AIL_set_sample_loop_count
 * (g_sfx_last_arg_c). See audiofix.h for the exact (triangular) offset layout.
 *
 * Gate note: a caller test that wants the SFX to actually fire must enable the
 * two driver flags + clear the cinematic flag; audiofix_enable_sfx() does this. */
int g_ail_stop_sample_calls = 0;
int g_ail_init_sample_calls = 0;
int g_ail_set_sample_addr_calls = 0;
int g_ail_start_sample_calls = 0;
void AIL_stop_sample(uint32 sample)
{
    g_ail_stop_sample_calls++;
    /* Route the per-invocation counter to the right player by the handle the
     * real function passed: slot 1 (handle_1) is fd2_play_sfx_sample_from_bank;
     * everything else (slot 0 / handle_0) is fd2_play_sfx_with_handle, which the
     * dialog typewriter and most callers drive. */
    if (sample == data_fd2_audio_sfx_sample_handle_1) {
        g_play_sfx_sample_from_bank_calls++;
    } else {
        g_play_sfx_with_handle_calls++;
        g_dlg_blink_calls++;
    }
}
void AIL_init_sample(uint32 sample) { (void)sample; g_ail_init_sample_calls++; }
void AIL_set_sample_address(uint32 sample, uint32 start, uint32 len)
{
    g_ail_set_sample_addr_calls++;
    g_sfx_last_handle = sample;   /* slot/handle programmed     */
    g_sfx_last_arg_a = start;     /* bank base (entry off==0)  */
    g_sfx_last_id = (int)len;     /* sfx id    (entry end==id) */
    if (g_sfx_id_count < 64) {
        g_sfx_id_log[g_sfx_id_count] = (int)len;
    }
    g_sfx_id_count++;
}
void AIL_set_sample_loop_count(uint32 sample, int count)
{
    (void)sample;
    g_sfx_last_arg_c = count;
}
void AIL_start_sample(uint32 sample) { (void)sample; g_ail_start_sample_calls++; }
/* Chapter 10 end scene char placement tables (data segment @ 0x52113 /
 * 0x5211E). Real binary bytes until the data segment is emitted;
 * fd2_chapter_10_end copies each 11-byte table into an on-stack placement block
 * and places chars 0..0xA at those tiles. X/Y are battle-tile coords; chapter
 * 10 has no facing table (the handler writes the inline fixed facing value 2
 * into sprite_state[1] for every placed char). */
/* data_fd2_chapter_ch10_end_scene_char_pos_x_table now has its real const
 * definition in src/table/chtab.c (emitted), so no stand-in here. */
/* data_fd2_chapter_ch10_end_scene_char_pos_y_table now has its real const
 * definition in src/table/chtab.c (emitted), so no stand-in here. */
/* Chapter 12 end scene char placement tables (data segment @ 0x52129 /
 * 0x52137 / 0x52145). Real binary bytes until the data segment is emitted;
 * fd2_chapter_12_end copies each 14-byte table into an on-stack placement block
 * and places chars 0..0xD. X/Y are battle-tile coords, facing is sprite
 * direction (0..3). */
/* fd2_play_rising_pre_cast_effect and fd2_play_variant_b_slide_pre_effect are
 * now real emitted functions (src/spell/spellcin.c); their former stubs were
 * removed. Both are pure VGA/VRAM cinematic workers whose behavior is deferred
 * to Phase 9 (see tests/spell/spellcin.c header). Their callees
 * fd2_render_circle_anim_row and fd2_render_filled_circle_band_anim (graphics,
 * not yet emitted) are stubbed here so spellcin.obj links. */
void fd2_render_circle_anim_row(int cx, int cy, int r, int scale_num,
                                int start_row, int end_row,
                                uint8 *palette_remap_src) {
    (void)cx; (void)cy; (void)r; (void)scale_num;
    (void)start_row; (void)end_row; (void)palette_remap_src;
}
void fd2_render_filled_circle_band_anim(uint32 param_1, uint32 param_2,
                                        uint32 param_3, int cx, int cy,
                                        int radius) {
    (void)param_1; (void)param_2; (void)param_3;
    (void)cx; (void)cy; (void)radius;
}
void fd2_play_rising_pre_cast_effect(int a, int b, int c) { }
void fd2_play_variant_b_slide_pre_effect(int a, int b) { }
/* The four warp sub-animations (fd2_animate_warp_teleport_char,
 * fd2_animate_warp_portal_open_at, fd2_animate_warp_out_collapse,
 * fd2_animate_warp_in_expand) now live in src/spell/spellcin.c; their former
 * stubs were removed. */
/* fd2_restore_portrait_cache_from_tmp: slated for src/rsrc/rsrc.c (not yet
 * emitted). The non-scripted cleanup path of fd2_play_full_combat_cinematic
 * reaches it, but the anicine.c unit tests exercise only the scripted path
 * (which skips cleanup), so a noop stub suffices here. */
void fd2_restore_portrait_cache_from_tmp(void) { }
/* fd2_animate_warp_teleport_char recording spy: captures call count + all 5
 * received args so callers (fd2_cinematic_warp_char_to_tile) can pin their
 * argument routing, in particular the src==dst tile duplication. Behaviorally
 * still a no-op (the real teleport animation is a Phase-9 display concern). */
int    g_warp_teleport_calls = 0;
uint32 g_warp_teleport_arg[5] = {0,0,0,0,0};
void fd2_animate_warp_teleport_char(uint32 a, uint32 b, uint32 c, uint32 d, uint32 e)
{
    g_warp_teleport_calls++;
    g_warp_teleport_arg[0] = a;
    g_warp_teleport_arg[1] = b;
    g_warp_teleport_arg[2] = c;
    g_warp_teleport_arg[3] = d;
    g_warp_teleport_arg[4] = e;
}
/* fd2_check_char_is_dead is emitted for real in src/battle/battle.c (reads
 * runtime_char[idx].flags bit0). This stub is a link-time fallback for the test
 * build and merges both branches' recording families so either suite's fixtures
 * work if the stub is ever the live definition:
 *
 *  - g_check_char_is_dead_use_array opts in to faithful per-slot flags (reads
 *    runtime_char[c].flags from data_fd2_battle_runtime_char_array_ptr);
 *  - g_check_char_is_dead_use_by_idx opts in to a per-index dead bitmap
 *    (g_check_char_is_dead_by_idx[c & 0xFF]) for chapter-end count gates;
 *  - g_check_char_is_dead_calls / g_check_char_is_dead_last_arg record the
 *    invocation count and most recent char index for caller-routing tests;
 *  - the default returns the uniform g_check_char_is_dead_return. */
int    g_check_char_is_dead_return = 0;
int    g_check_char_is_dead_use_array = 0;
int    g_check_char_is_dead_calls = 0;
uint32 g_check_char_is_dead_last_arg = 0xFFFFFFFFuL;
int    g_check_char_is_dead_use_by_idx = 0;
uint8  g_check_char_is_dead_by_idx[256] = {0};
int fd2_check_char_is_dead(uint32 c)
{
    g_check_char_is_dead_calls++;
    g_check_char_is_dead_last_arg = c;
    if (g_check_char_is_dead_use_by_idx) {
        return (int)g_check_char_is_dead_by_idx[c & 0xFF];
    }
    if (g_check_char_is_dead_use_array) {
        return data_fd2_battle_runtime_char_array_ptr[c].flags & 1;
    }
    return g_check_char_is_dead_return;
}
/* fd2_scan_chars_within_manhattan_range: now in btl_ai.c */
/* fd2_composite_battle_frame (rndscene.c) pipeline-callee stubs with arg
 * capture, so the compositor test can assert workspace address, pixel
 * constants, and call ordering forwarded to each stage. */
int    g_tile_map_calls = 0;
uint32 g_tile_map_last_dst = 0;
uint32 g_tile_map_last_stride = 0;
uint32 g_tile_map_last_w = 0;
uint32 g_tile_map_last_h = 0;
uint32 g_tile_map_last_ox = 0;
uint32 g_tile_map_last_oy = 0;
int    g_composite_call_count = 0;
/* opt-in ordered dst log (default off; used by the full-screen-flash test to
 * confirm the two real spell-effect composites route into the back-buffer then
 * a distinct malloc'd buffer, followed by the finalizer composite). */
int    g_tile_map_log_on = 0;
int    g_tile_map_log_count = 0;
uint32 g_tile_map_log_dst[16];
/* Test-controllable loop-break seam for the idle loops (the real
 * fd2_wait_input_with_dialog_repaint and the menu loops that idle through it).
 * The tile-map composite runs exactly once at the top of every idle-loop body
 * (and once per fd2_composite_battle_frame pass), and is the only such callee
 * still backed by a recording stub, so it is the single harness-driveable seam
 * for running an idle loop BODY exactly once: when g_repaint_flip_buffer_after
 * != 0, the g_repaint_settings_calls counter reaching that threshold flips the
 * BIOS keyboard buffer from empty->nonempty (tail 0x41C := head 0x41A + 2) so
 * the next loop-top fd2_check_keyboard_buffer_nonempty() returns nonzero and
 * the loop exits. Default 0 keeps the historical no-op behavior for all other
 * tests. (The former seam lived in the fd2_render_terrain_info_hud_panel stub;
 * that function is now a real emitted routine in src/gfx/rndstat.c.) */
int g_repaint_settings_calls = 0;
int g_repaint_flip_buffer_after = 0;
void fd2_composite_battle_tile_map(uint32 d, uint32 s, uint32 w, uint32 h, uint32 ox, uint32 oy) {
    /* The tile-map blit is the first stage of every fd2_composite_battle_frame
     * pass and runs exactly once per composite (unconditional, both skip-cycle
     * paths). It is the host-observable proxy that counts composite frames for
     * caller tests (cursor.c, spelleff.c, btl_ai.c, ...) that only care "a frame
     * composited". fd2_blit_rectangle is now a real emitted function
     * (src/gfx/blitspr.c) and no longer available as that proxy. */
    g_composite_call_count++;
    g_tile_map_calls++;
    g_tile_map_last_dst = d; g_tile_map_last_stride = s;
    g_tile_map_last_w = w; g_tile_map_last_h = h;
    g_tile_map_last_ox = ox; g_tile_map_last_oy = oy;
    if (g_tile_map_log_on && g_tile_map_log_count < 16) {
        g_tile_map_log_dst[g_tile_map_log_count] = d;
        g_tile_map_log_count++;
    }

    g_repaint_settings_calls++;
    if (g_repaint_flip_buffer_after != 0 &&
        g_repaint_settings_calls >= g_repaint_flip_buffer_after) {
        *(volatile uint16 *)0x41CuL =
            (uint16)(*(volatile uint16 *)0x41AuL + 2);
    }
}
/* fd2_paint_cursor_overlay_pattern, fd2_composite_all_chars_overlay,
 * fd2_paint_char_sprite_at_world_pos, fd2_paint_chars_shadow_overlay and
 * fd2_blit_animated_tile_at_pos are now real emitted functions
 * (src/gfx/rndscene.c / src/gfx/blittile.c); their former no-op/recording
 * stubs here were removed. The real fd2_composite_all_chars_overlay loops over
 * alive party slots calling the real fd2_paint_char_sprite_at_world_pos and
 * finishes with one unconditional real fd2_paint_chars_shadow_overlay. */

/* fd2_tile_blit_24x24_passthrough is now emitted for real in src/gfx/blittile.c;
 * its former recording stub (the g_blitpass_calls counter and the g_blitpass_src
 * / g_blitpass_dst / g_blitpass_stride arrays) here was removed. Caller tests
 * that previously checked the recorded (src, dst, stride) now drive the real
 * blitter against one-pixel RLE probe sprites over a real back-buffer and observe
 * the painted bytes (same migration applied earlier to the dimmed / remap / solid
 * siblings), which proves the caller's src / dst / stride arithmetic and branch
 * selection without a stub. The probe-sprite helpers below (bp_*) build those
 * sprites and read the painted output back. */

/* ---- passthrough-blit probe support -------------------------------------
 * Each "probe" sprite is a 24x24 RLE stream that paints exactly one identifying
 * byte at tile-local (row 0, col 0) and is transparent everywhere else, so when
 * a caller forwards it to the real fd2_tile_blit_24x24_passthrough the painted
 * destination byte directly reveals (a) WHERE the blit landed (the byte offset)
 * and (b) WHICH source the caller resolved (the byte value, since each atlas
 * slot's probe paints a slot-specific value). bp_probe2 additionally paints a
 * second copy at (row 1, col 0); because the passthrough row reset advances by
 * stride-0x18, that second byte lands exactly `stride` past the first, so a test
 * can confirm the forwarded row stride end-to-end. Probe values are 1-based
 * (slot k -> k+1) so the painted byte is always distinct from the 0 background. */

/* Write a one-pixel probe (paints `value` at row0/col0, transparent elsewhere)
 * into a 24x24 RLE sprite slot. */
void bp_probe1(uint8 *slot, uint8 value)
{
    int i;
    slot[0] = (uint8)0x80u;          /* LITERAL run length 1 */
    slot[1] = value;                 /* the one painted byte  */
    slot[2] = (uint8)(0xC0u | 22u);  /* SKIP 23 -> finish row 0 (1 + 23 == 24) */
    for (i = 1; i < 24; i++) {
        slot[2 + i] = (uint8)(0xC0u | 23u);  /* rows 1..23: SKIP 24 */
    }
}

/* Write a two-pixel probe: `value` at row0/col0 AND row1/col0, transparent
 * elsewhere. The second byte lands exactly `stride` past the first. */
void bp_probe2(uint8 *slot, uint8 value)
{
    int i;
    slot[0] = (uint8)0x80u;          /* row 0: LITERAL 1 */
    slot[1] = value;
    slot[2] = (uint8)(0xC0u | 22u);  /* row 0: SKIP 23 */
    slot[3] = (uint8)0x80u;          /* row 1: LITERAL 1 */
    slot[4] = value;
    slot[5] = (uint8)(0xC0u | 22u);  /* row 1: SKIP 23 */
    for (i = 2; i < 24; i++) {
        slot[4 + i] = (uint8)(0xC0u | 23u);  /* rows 2..23: SKIP 24 */
    }
}
/* p4-cascade recorders re-instated for link. integ migrated the passthrough /
 * solid-colour blitters (src/gfx/blittile.c) and the party has-char query
 * (src/util/misc.c) to real bodies and dropped these recording globals; p4's
 * anicomb1 / anicomb2 / rndscene / chend1 suites still reference them, so the
 * globals are retained here. They are unfilled at runtime (the real functions
 * win at link); the dependent suites are re-pointed at the real outputs in the
 * systematic-fix phase. */
int    g_blitpass_calls = 0;
uint32 g_blitpass_src[64];
uint32 g_blitpass_dst[64];
uint32 g_blitpass_stride[64];
int    g_blitsolid_calls = 0;
uint32 g_blitsolid_color[64];
int    g_has_char_calls = 0;
uint32 g_has_char_fake = 0;
uint32 g_has_char_last_arg = 0;
/* g_scaledmap_* recorders for the tactical-overview zoom suite. The real
 * fd2_blit_scaled_tile_map_view (src/gfx/blittile.c) supersedes the former
 * recording stub at link, so these globals are retained only so the zoom suite
 * links; the suite seeds/asserts the real fixed-point camera + scale math. */
int    g_scaledmap_calls = 0;
uint32 g_scaledmap_cx[16];
uint32 g_scaledmap_cy[16];
uint32 g_scaledmap_scale[16];
uint32 g_scaledmap_table0, g_scaledmap_table1, g_scaledmap_table2;
uint32 g_scaledmap_table40, g_scaledmap_table41;
/* g_blitdim_calls recorder for the dimmed-blitter dispatch tests. The real
 * fd2_tile_blit_24x24_dimmed_grayscale (src/gfx/blittile.c) supersedes the
 * former recording stub at link; retained so dependent suites link. */
int    g_blitdim_calls = 0;

/* Absolute byte offset (from atlas base) of slot i's sprite data, for an atlas
 * laid out by bp_build_atlas1. The sprite data region begins right AFTER the
 * n-entry offset table so no slot (in particular slot 0) overlaps the table. A
 * caller that overwrites a specific slot's probe must use this same formula. */
uint32 bp_atlas_slot_off(uint32 table_off, int n, uint32 entry_span, int i)
{
    return table_off + (uint32)n * 4u + (uint32)i * entry_span;
}

/* Build an atlas whose offset table (entry i at base+table_off+i*4) points slot
 * i at bp_atlas_slot_off(...), and plant a one-pixel probe (value i+1) at each
 * slot. After this, a caller that resolves src = base + table[idx] reads slot
 * idx's probe, so the painted byte value identifies idx (value-1). The sprite
 * data starts after the offset table (NOT at base+0), so slot 0 does not clobber
 * the table; requires entry_span >= 26 so adjacent probe programs do not overlap.
 * The backing buffer must span bp_atlas_slot_off(table_off, n, entry_span, n-1)
 * + 26 bytes. */
void bp_build_atlas1(uint8 *base, uint32 table_off, uint32 entry_span, int n)
{
    int i;
    uint32 slot_off;
    for (i = 0; i < n; i++) {
        slot_off = bp_atlas_slot_off(table_off, n, entry_span, i);
        *(uint32 *)(base + table_off + (uint32)i * 4u) = slot_off;
        bp_probe1(base + slot_off, (uint8)(i + 1));
    }
}

/* Count bytes equal to `value` in buf[0..size) and report the first offset
 * (0xFFFFFFFF if none). Used to confirm a probe painted exactly where expected. */
int bp_count_value(const uint8 *buf, uint32 size, uint8 value, uint32 *first_off)
{
    uint32 i;
    int n = 0;
    *first_off = 0xFFFFFFFFu;
    for (i = 0; i < size; i++) {
        if (buf[i] == value) {
            if (n == 0) {
                *first_off = i;
            }
            n++;
        }
    }
    return n;
}
/* fd2_render_party_roster_grid: now emitted for real in src/gfx/rndmenu.c and
 * linked. fd2_render_chapter_intro_dialog_panels (mode 3) overlays it; the panel
 * test now drives the real grid (its bg-fill portrait blits land in g_blitpass_*
 * and the per-char name dialog runs against the immediate-END text program). */
/* fd2_render_party_roster_with_item_stat_preview: now emitted for real in
 * src/gfx/rndmenu.c and linked. Its sole caller fd2_party_roster_class_select_loop
 * (ui_menu/chintro.c, the cs_* tests) now drives the real renderer against a
 * fixture (runtime-char array + portrait cache + zeroed atlas + all-END dialog
 * text + zeroed item-effect table), exactly like the ps_* tests drive the real
 * party-roster grid: each per-char name dialog runs against the immediate-END
 * program, so marking the highlighted char's name page proves which cursor the
 * final re-render highlighted (g_dlg_glyph_last_p5 == 0xC9). Its own behaviour is
 * covered by tests/gfx/rndmenu.c. */
/* fd2_pick_stat_compare_color (the 3-branch stat-compare colour picker; real body
 * not yet emitted — routes to ui_menu/shop.c). The stub reproduces the exact
 * comparator (==current -> 0x1F, current<preview -> 0x2A, current>preview -> 0x77)
 * so callers see faithful digit colours, and logs the per-call (current, preview)
 * pair + a count so the rndmenu stat-preview test can pin which value pair each of
 * the four stat columns was coloured against. */
int    g_pick_color_calls = 0;
int32  g_pick_color_last_current = 0;
int32  g_pick_color_last_preview = 0;
int32  g_pick_color_cur_log[16];
int32  g_pick_color_prev_log[16];
uint32 fd2_pick_stat_compare_color(int32 current_stat, int32 preview_stat) {
    if (g_pick_color_calls < 16) {
        g_pick_color_cur_log[g_pick_color_calls] = current_stat;
        g_pick_color_prev_log[g_pick_color_calls] = preview_stat;
    }
    g_pick_color_calls++;
    g_pick_color_last_current = current_stat;
    g_pick_color_last_preview = preview_stat;
    if (current_stat == preview_stat) {
        return 0x1f;
    }
    if (current_stat < preview_stat) {
        return 0x2a;
    }
    return 0x77;
}
/* fd2_count_selected_chars is now a real emitted function (src/util/misc.c).
 * Its former counting stub (which logged g_count_selected_calls) was removed;
 * the recruitment render test now exercises the real counter through the
 * renderer's "remaining = max_chars - count" number output. */

/* Count all nonzero (painted) bytes in buf[0..size). With one-pixel probes this
 * equals the number of passthrough blits that actually painted. */
int bp_count_painted(const uint8 *buf, uint32 size)
{
    uint32 i;
    int n = 0;
    for (i = 0; i < size; i++) {
        if (buf[i] != 0) {
            n++;
        }
    }
    return n;
}

/* ---- shared compositor host-safety fixture (see blitprob.h) ----------------
 * Turn-gated / cinematic chapter-event handlers pan the camera, which drives the
 * REAL fd2_composite_battle_frame; its per-char painter, shadow overlay, cursor
 * overlay and terrain HUD all feed a sprite stream to the REAL RLE blitters
 * (fd2_tile_blit_24x24_passthrough + siblings). An unset / real-but-non-24x24-RLE
 * sprite source makes the row decoder spin forever (the row loop only exits when
 * the remaining-column counter hits exactly 0). Make every such blit a
 * deterministic no-op without touching the emit C. */
#define TG_PCACHE_SPAN  0x32a00u           /* size the loader's FD2.TMP fwrite reads */
static uint8  tg_pcache[TG_PCACHE_SPAN];   /* +0 offset table -> all-SKIP sprite     */
static uint8  tg_cursor_atlas[0x200];      /* +6 offset table -> all-SKIP sprite     */
static uint8  tg_safe_tile_map[0x400];     /* zeroed: tiles resolve non-renderable   */
static uint8  tg_safe_tile_attr[0x400];    /* zeroed: renderable bit 0x80 clear      */
static uint32 tg_saved_pcache;
static uint32 tg_saved_rbs;
static uint32 tg_saved_tile_map;
static uint32 tg_saved_tile_attr;
static uint8  tg_saved_hud_enabled;
static uint32 tg_saved_play_active;

static void tg_fill_skip_sprite(uint8 *p)
{
    int i;
    for (i = 0; i < 24; i++) {
        p[i] = (uint8)(0xC0u | 23u);       /* one "SKIP 24" command per row */
    }
}

void tg_install_compositor_safe_atlases(void)
{
    uint32 i;

    tg_saved_pcache      = data_fd2_portrait_sprite_cache;
    tg_saved_rbs         = data_fd2_runtime_battle_state_ptr;
    tg_saved_tile_map    = data_fd2_battle_tile_map_ptr;
    tg_saved_tile_attr   = data_fd2_tile_attribute_flags_buffer_ptr;
    tg_saved_hud_enabled = data_fd2_ui_terrain_hud_user_enabled;
    tg_saved_play_active = data_fd2_ui_play_active_flag;

    /* per-char painter: a static safe portrait cache (NOT heap, so callers whose
     * teardown does NOT free data_fd2_portrait_sprite_cache, e.g. the spelleff impact
     * fixture, leak nothing). The +0 offset table all points at one transparent
     * "SKIP 24 x 24" sprite at +0x780. Pre-seed the portrait cache id-list (id 0)
     * + count 1 so a matching tile-event record's fd2_load_portrait_to_cache takes
     * the cache-HIT early return, leaving this safe atlas intact instead of
     * re-loading real FDICON. (The chapter-event loader fixtures zero every
     * tile-event byte but the race tag, so the matched record's char_id is 0.)
     * tg_restore_compositor_safe_atlases puts the saved pointer back, so a caller
     * teardown that DOES free(data_fd2_portrait_sprite_cache) frees its own pointer, never
     * this static buffer. */
    for (i = 0; i < TG_PCACHE_SPAN; i++) {
        tg_pcache[i] = 0;
    }
    for (i = 0; i < 0x80u; i++) {           /* covers facing*3 + cache_idx*0xC + pal */
        *(int32 *)(tg_pcache + i * 4u) = (int32)0x780;
    }
    tg_fill_skip_sprite(tg_pcache + 0x780);
    data_fd2_portrait_sprite_cache = (uint32)tg_pcache;
    *(uint32 *)data_fd2_resource_portrait_cache_id_list_base = 0;  /* char_id 0    */
    data_fd2_resource_portrait_cache_count = 1;                    /* -> cache hit */
    data_fd2_resource_portrait_cache_buffer_used = 0x780;

    /* cursor overlay: 4-byte offset table at +6 -> the all-SKIP sprite. */
    for (i = 0; i < sizeof(tg_cursor_atlas); i++) {
        tg_cursor_atlas[i] = 0;
    }
    for (i = 0; i < 0x40u; i++) {
        *(int32 *)(tg_cursor_atlas + 6 + i * 4u) = (int32)0x100;
    }
    tg_fill_skip_sprite(tg_cursor_atlas + 0x100);
    data_fd2_runtime_battle_state_ptr = (uint32)tg_cursor_atlas;

    /* shadow / animated-tile overlay: non-renderable tiles. */
    for (i = 0; i < sizeof(tg_safe_tile_map); i++) {
        tg_safe_tile_map[i] = 0;
    }
    for (i = 0; i < sizeof(tg_safe_tile_attr); i++) {
        tg_safe_tile_attr[i] = 0;
    }
    data_fd2_battle_tile_map_ptr = (uint32)tg_safe_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)tg_safe_tile_attr;

    /* terrain HUD panel: stays off so it early-returns before any blit. */
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;
}

void tg_restore_compositor_safe_atlases(void)
{
    data_fd2_portrait_sprite_cache                    = tg_saved_pcache;
    data_fd2_runtime_battle_state_ptr        = tg_saved_rbs;
    data_fd2_battle_tile_map_ptr             = tg_saved_tile_map;
    data_fd2_tile_attribute_flags_buffer_ptr = tg_saved_tile_attr;
    data_fd2_ui_terrain_hud_user_enabled     = tg_saved_hud_enabled;
    data_fd2_ui_play_active_flag             = tg_saved_play_active;
}
/* fd2_tile_blit_24x24_dimmed_grayscale is now emitted for real in
 * src/gfx/blittile.c; its former recording stub (and the g_blitdim_calls
 * counter) here were removed. The acted-char caller test
 * (test_paint_acted_dimmed in tests/gfx/rndscene.c) instead drives the real
 * dimmed blitter against a single-pixel RLE sprite planted only at the
 * expected sprite-source slot (transparent SKIP elsewhere) over a real
 * back-buffer, and observes the single painted grayscale byte
 * ((src & 7) + 0x18), which proves the caller's acted-flag branch selection
 * plus its src / dst arithmetic. */
/* fd2_tile_blit_24x24_with_remap_table is now emitted for real in
 * src/gfx/blittile.c; its former recording stub here was removed. The sole
 * caller test (fd2_blit_animated_tile_at_pos in tests/gfx/blittile.c) instead
 * drives the real blitter against a one-pixel RLE sprite at the window-origin
 * cell (in-bounds dst) and observes the painted LUT-remapped byte, which proves
 * the caller's src / dst / remap_table arithmetic and remap-branch selection. */
/* fd2_tile_blit_24x24_solid_color is now emitted for real in
 * src/gfx/blittile.c; its former recording stub here was removed. The
 * caller test (fd2_animate_status_effect_overlay_flicker in
 * tests/anim/anicombt.c) instead drives the real blitter against a
 * one-pixel RLE sprite placed only at the expected sprite-source offset
 * (transparent SKIP bytes elsewhere) and observes the single painted
 * silhouette byte, which proves the caller's src / dst arithmetic and
 * window-cull predicate. The painted colour is the LOW BYTE of the
 * stride argument (param_3 & 0xFF); param_4 is read by the caller but
 * IGNORED by the blitter (verified against the 0x4DDD7 disassembly). */
/* g_blittint_* recorders for the spell-overlay-blink tint-blit tests. The real
 * fd2_tile_blit_24x24_with_tint_offset (src/gfx/blittile.c) supersedes the
 * former recording stub at link; these globals are retained so the dependent
 * suites link (they seed/assert the real per-char dst/sprite/colour math). */
int    g_blittint_calls = 0;
uint32 g_blittint_color_base[64];
uint32 g_blittint_team_offset[64];
/* fd2_render_terrain_info_hud_panel is now a real emitted function
 * (src/gfx/rndstat.c). Its former recording/loop-break stub here was removed;
 * the idle-loop break seam (g_repaint_settings_calls / g_repaint_flip_buffer_after)
 * moved up into the fd2_composite_battle_tile_map stub, which also runs once per
 * idle-loop body. */
void fd2_render_recruitment_party_screen(void) { }
/* Shop / roster menu shared state (BSS) — real FD2.LE globals.
 *   visible_item_count @ 0x5413F  rows the renderer paints
 *   candidate_array_ptr @ 0x54143  -> the equip-eligible char-id byte array
 *   saved_cursor / saved_scroll @ 0x5414B / 0x5414F  persist across re-opens */
/* not-yet-emitted buy-flow callees (real fns in src later; stubbed for the
 * link). The buy-menu cancel test never reaches these — Esc on the item grid
 * returns before the eligibility scan / recipient select. */
int  fd2_party_roster_class_select_loop(uint32 candidate_count,
                                        uint32 candidate_array_ptr,
                                        uint32 item_id)
{
    (void)candidate_count; (void)candidate_array_ptr; (void)item_id;
    return -1;
}
/* Scripted seller/recipient single-select loop. Each call consumes the next
 * entry of g_single_select_ret[] as its return value and writes the matching
 * g_single_select_cursor[] entry into data_fd2_ui_menu_cursor_idx (the real
 * function leaves the chosen index there). The arrays default to {-1,...}
 * (immediate cancel), which is exactly the prior unconditional `return -1`
 * behaviour relied on by the buy-menu cancel test (that test never reaches this
 * call). The sell-menu tests script a seller index then a cancel. */
int  g_single_select_ret[8]    = { -1, -1, -1, -1, -1, -1, -1, -1 };
int  g_single_select_cursor[8] = {  0,  0,  0,  0,  0,  0,  0,  0 };
int  g_single_select_idx       = 0;
int  g_single_select_calls     = 0;
int  fd2_party_roster_single_select_loop(void)
{
    int i;
    int r;
    i = g_single_select_idx;
    if (i > 7) {
        i = 7;
    }
    r = g_single_select_ret[i];
    data_fd2_ui_menu_cursor_idx = (uint32)g_single_select_cursor[i];
    g_single_select_idx++;
    g_single_select_calls++;
    return r;
}
/* fd2_animate_shop_transaction_feedback: now emitted in src/anim/aniui.c and
 * linked for real. Its caller tests (tests/anim/aniui.c) drive the real
 * per-state sprite cycle + state-4 palette flash through the real
 * fd2_blit_indexed_sprite_at_xy -> fd2_rle_blit_sprite spy and the real
 * fd2_paint_portrait_to_dialog_area -> dialog-blit spy. The former empty stub
 * here was removed (it shadowed the real function and warned at link). */
/* Recording stubs for fd2_shop_menu_input_loop's not-yet-emitted display
 * callees: the grid renderer (gfx/rndmenu.c) and the page-up scroll animation
 * (anim/aniui.c fd2_animate_scroll_down_in_shop_dialog). The shop input loop
 * re-renders the 2-column item grid after every cursor move and animates the
 * viewport when it pages; recording the last forwarded (item_count, cursor,
 * dst, sell_mode) and the call counts lets the shop.c navigation test pin the
 * cursor / scroll-paging arithmetic and the SFX/render sequencing without
 * touching VGA. (The page-DOWN scroll animation
 * fd2_animate_scroll_up_in_shop_dialog is already emitted and host-safe, so the
 * loop calls the real function — see below.) */
int    g_shop_grid_render_calls = 0;
uint32 g_shop_grid_last_count = 0;
uint32 g_shop_grid_last_array = 0;
uint32 g_shop_grid_last_cursor = 0;
uint32 g_shop_grid_last_dst = 0;
uint32 g_shop_grid_last_sell = 0;
/* Opt-in capture of the forwarded item-id list contents. Default OFF so the
 * existing shop/buy/open tests (which pass a synthetic non-dereferenceable
 * pointer for item_id_array) are unaffected. The sell-menu test sets
 * g_shop_grid_capture_list=1 because there the array is the real on-stack
 * inventory list built by fd2_run_sell_item_menu, live during this call. */
int    g_shop_grid_capture_list = 0;
uint8  g_shop_grid_list[32];
void fd2_render_shop_item_grid(uint32 item_count, uint32 item_id_array,
                               uint32 cursor, uint32 dst_buf,
                               uint32 sell_mode_flag)
{
    uint32 i;
    uint32 n;
    g_shop_grid_render_calls++;
    g_shop_grid_last_count = item_count;
    g_shop_grid_last_array = item_id_array;
    g_shop_grid_last_cursor = cursor;
    g_shop_grid_last_dst = dst_buf;
    g_shop_grid_last_sell = sell_mode_flag;
    if (g_shop_grid_capture_list && item_id_array != 0) {
        n = item_count;
        if (n > sizeof(g_shop_grid_list)) {
            n = sizeof(g_shop_grid_list);
        }
        for (i = 0; i < n; i++) {
            g_shop_grid_list[i] = ((const uint8 *)item_id_array)[i];
        }
    }
}
/* Both shop-dialog scroll animations are now REAL emitted functions in
 * src/anim/aniui.c: fd2_animate_scroll_up_in_shop_dialog (page-DOWN) and
 * fd2_animate_scroll_down_in_shop_dialog (page-UP). Each only writes the
 * mode13h aperture + paces with three __delay_thunk_375b2(10) calls, so both
 * are host-safe to call directly. The shop navigation test observes that an
 * animation paced via g_delay375b2_calls == 3, with the branch direction
 * pinned independently by data_fd2_ui_menu_scroll_offset. */
/* chapter-intro menu globals + heavy-callee stubs for fd2_run_chapter_intro_menu_main
 * (src/ui_menu/chintro.c). That orchestrator is itself deferred to Phase 9 (no
 * in-process seam: real-file loaders + VGA port I/O + four nested interactive
 * sub-menus — see the deferral note in tests/ui_menu/chintro.c), so these only
 * satisfy the link and are never invoked by a test. */
/* per-chapter dispatch category (real data @ 0x526B9, emit'd by the data
 * pipeline; zero-filled fake here, sized past chapter_id 30 (0x1E) for the
 * indexed read in fd2_save_current_state_to_slot / fd2_chapter_transition_menu /
 * fd2_load_state_from_selected_slot). Tests set the one index they exercise. */
void fd2_animate_tutorial_dialog_intro_or_outro(uint32 closing) { (void)closing; }
int  fd2_load_chapter_party_roster(uint8 *out_buf) { (void)out_buf; return 0; }
void fd2_run_buy_item_menu(uint32 n, uint8 *a) { (void)n; (void)a; }
void fd2_run_sell_item_menu(void) { }
void fd2_run_equip_member_menu(void) { }
void fd2_run_give_item_menu(void) { }
/* heavy-callee stubs for fd2_run_chapter_intro_menu_typeB (the non-shop
 * between-chapters orchestrator, also Phase 9 deferred). status/load open their
 * own real-file UI; stubbed to satisfy the link, never invoked.
 * fd2_save_current_state_to_slot and fd2_load_state_from_selected_slot are now
 * emitted for real in src/save/save.c and driven by the test_scs_* /
 * test_lss_* cases in tests/save/save.c against the real FD2.SAV. */
void fd2_run_status_screen_member_menu(void) { }
/* heavy-callee stubs for fd2_run_chapter_intro_menu_typeC (the town-services
 * orchestrator, Phase 9 deferred). Revive (church revive) and class-promotion
 * each open their own real-file dialog UI; stubbed to satisfy the link, never
 * invoked. fd2_run_give_item_menu / fd2_run_status_screen_member_menu (typeC's
 * other two dispatch targets) are already stubbed above. */
void fd2_run_revive_menu_main(void) { }
void fd2_run_class_promotion_menu_main(void) { }
void fd2_blit_scaled_chapter_pose(uint32 cx, uint32 cy, uint32 bmp, int32 s)
{ (void)cx; (void)cy; (void)bmp; (void)s; }
void fd2_render_chapter_dialog_borders(void) { }
/* fd2_render_chapter_intro_dialog_panels now has a real body in
 * src/gfx/rndmenu.c (driven by the mode-0/1/2/3 tests in tests/gfx/rndmenu.c);
 * its former no-op stub was removed. */
/* capture wiring for fd2_blit_indexed_sprite_with_alloc tests; also drives the
 * real fd2_open_settings_dialog_with_slide corner-sprite blit. Records the last
 * (dst, sprite, stride) and counts total calls so the dialog-open / settings
 * loop tests can observe that the render ran. */
uint32 g_blitsetup_dst, g_blitsetup_sprite, g_blitsetup_stride;
int    g_blitsetup_calls = 0;
/* per-call log (page-advance collapse test verifies all 8 corner blits) */
uint32 g_blitsetup_dst_log[32];
uint32 g_blitsetup_sprite_log[32];
void fd2_blit_sprite_with_stride_setup(uint32 d, uint32 s, uint32 st)
{
    if (g_blitsetup_calls < 32) {
        g_blitsetup_dst_log[g_blitsetup_calls] = d;
        g_blitsetup_sprite_log[g_blitsetup_calls] = s;
    }
    g_blitsetup_dst = d;
    g_blitsetup_sprite = s;
    g_blitsetup_stride = st;
    g_blitsetup_calls++;
}
/* fd2_restore_dialog_area_from_buffer now has a real body in
 * src/dialog/dialog.c (inverse of fd2_backup_dialog_area_to_buffer). */
uint32 g_saveblk_out, g_saveblk_w, g_saveblk_h, g_saveblk_dst,
       g_saveblk_src, g_saveblk_stride;
int    g_saveblk_calls = 0;
void fd2_save_screen_block_to_buffer(uint32 out_buf, uint32 width, uint32 height,
                                     uint32 dst, uint32 src_ptr, uint32 stride)
{
    g_saveblk_out = out_buf;
    g_saveblk_w = width;
    g_saveblk_h = height;
    g_saveblk_dst = dst;
    g_saveblk_src = src_ptr;
    g_saveblk_stride = stride;
    g_saveblk_calls++;
}
/* fd2_assemble_dialog_frame_layered is now emitted for real in
 * src/dialog/dialog.c. Its callers' tests (fd2_play_dialog_open_animation,
 * and the dedicated frame-layout test) drive the real function and observe
 * its blit calls through the fd2_blit_sprite_raw_with_header log below. */
/* capture wiring for fd2_alloc_and_blit_indexed_sprite_chunk tests */
uint32 g_blitdec_dst, g_blitdec_sprite, g_blitdec_stride;
int    g_blitdec_calls = 0;
/* opt-in full per-call log (default off; used by the full-screen-flash test to
 * capture both real spell-effect overlay invocations' resolved fx-sprite addr
 * and dst, since the last-call vars above keep only the final call). */
int    g_blitdec_log_on = 0;
int    g_blitdec_log_count = 0;
uint32 g_blitdec_log_dst[16];
uint32 g_blitdec_log_sprite[16];
void fd2_blit_sprite_with_decoded_pixels(uint32 d, uint32 s, uint32 st)
{
    g_blitdec_dst = d;
    g_blitdec_sprite = s;
    g_blitdec_stride = st;
    if (g_blitdec_log_on && g_blitdec_log_count < 16) {
        g_blitdec_log_dst[g_blitdec_log_count] = d;
        g_blitdec_log_sprite[g_blitdec_log_count] = s;
        g_blitdec_log_count++;
    }
    g_blitdec_calls++;
}
/* capture wiring for fd2_blit_sheet_sprite_at_offset tests */
uint32 g_blitraw_dst, g_blitraw_sprite, g_blitraw_stride;
/* full call log (used by dialog frame-layout tests): records every raw blit */
int    g_blitraw_log_on = 0;
int    g_blitraw_count = 0;
uint32 g_blitraw_log_dst[512];
uint32 g_blitraw_log_sprite[512];
uint32 fd2_blit_sprite_raw_with_header(uint32 d, uint32 s, uint32 st)
{
    g_blitraw_dst = d;
    g_blitraw_sprite = s;
    g_blitraw_stride = st;
    if (g_blitraw_log_on && g_blitraw_count < 512) {
        g_blitraw_log_dst[g_blitraw_count] = d;
        g_blitraw_log_sprite[g_blitraw_count] = s;
        g_blitraw_count++;
    }
    return 0;
}
/* fd2_render_recruitment_select_screen is now a real emitted function
 * (src/gfx/rndmenu.c); its former no-op stub here was removed. The
 * recruitment render test (tests/gfx/rndmenu.c) drives the real renderer. */
/* fd2_animate_spell_impact_per_target is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
/* fd2_animate_status_effect_overlay_flicker is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
/* fd2_animate_spell_full_screen_flash is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
/* fd2_animate_spell_overlay_blink is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
/* fd2_show_damage_number is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
/* fd2_show_miss_indicator is now a real emitted function
 * (src/anim/anicombt.c); its former no-op stub here was removed. */
void fd2_show_status_effect_overlay(uint32 t, uint32 s) { }
/* fd2_animate_spell_projectile_paths is emitted for real in src/anim/anicombt.c
 * (p4); the no-op stub below is the test-build fallback (superseded at link).
 * fd2_composite_then_animate_projectiles: shared spell-finale + epilogue helper
 * @ 0x21190, routed to gfx/rndscene.c. Stub here so callers link. */
void fd2_animate_spell_projectile_paths(void) { }
void fd2_composite_then_animate_projectiles(void) { }
/* fd2_remove_inventory_slot_at: now emitted for real in src/ui_menu/status.c.
 * Its old spy global g_remove_inventory_calls is gone; spell/spelleff.c now
 * observes the real slot-consume by checking slot[7].flag == 0x80. */
/* fd2_load_status_effect_sfx + fd2_play_and_free_status_effect_sfx: now emitted
 * for real in src/audio/audio.c. test_load_status_effect_sfx_real drives the
 * loader against staged real FDOTHER.DAT; test_play_and_free_status_effect_sfx
 * drives the player+free (observed via the g_sfx_* capture spy). */
/* fd2_collect_pending_death_drops: now in btl_turn.c */
/* fd2_display_dialog_scene: now emitted in src/dialog/dialog.c */
/* fd2_load_chapter_portrait: now emitted for real in src/rsrc/rsrc.c and
 * linked for real; its blit-offset branch + DATO.DAT load are driven by the
 * test_lcp_* cases in tests/rsrc/rsrc.c (observed via the g_dlg_blit_mirrored
 * capture spy + an independent DATO.DAT parse). */
/* fd2_close_status_screen_with_slide_out: now emitted in src/ui_menu/status.c
 * and linked for real; the test_close_status_screen_slide_out_runs_full_
 * teardown case in tests/ui_menu/status.c drives the real function (observed
 * via g_composite_call_count for the trailing recomposite). */
/* fd2_load_chapter_battle_data: now in rsrc/rsrc.c */
/* fd2_load_chapter_portraits_and_dump_tmp: now in rsrc/rsrc.c */
/* fd2_init_runtime_char_for_battle is now emitted in src/battle/btl_init.c
 * and linked for real; its caller test in tests/rsrc/rsrc.c drives the real
 * function and observes data_fd2_battle_party_member_count. */
/* fd2_play_palette_fade_to_black: now emitted in src/gfx/palette.c and linked
 * for real; its life-suite callers (fd2_load_save_and_init_engine,
 * fd2_main_menu_continue_dispatcher) drive the real fade, so their fixtures
 * stage a valid 768-byte palette buffer before the call. The former empty
 * neutralizing stub was removed. */
int g_ending_menu_return = 0;
int fd2_play_ending_and_record_clear(void) { return g_ending_menu_return; }
/* fd2_save_slot_selector_ui is now emitted for real in src/save/save.c and
 * linked. Its callers (fd2_save_current_state_to_slot,
 * fd2_load_state_from_selected_slot in save/save.c, and
 * fd2_main_menu_continue_dispatcher in life/main.c) drive the real picker:
 * each iteration reads ONE scancode via the real int386(0x16) BIOS read, so
 * tests pre-arm the BIOS keyboard buffer (Enter 0x1C = commit the slot left in
 * data_fd2_ui_menu_cursor_idx, Esc 0x01 = cancel). The setup phase blits a
 * panel sprite (the recording fd2_dialog_sprite_blit_normal stub below) and
 * paints the grid via the real fd2_render_save_slot_grid, so those callers'
 * fixtures stage a small sprite-atlas buffer and an all-END dialog text
 * program. The former g_slot_selector_* control fake was removed. */
/* fd2_close_intro_dialog_with_slide_out: now emitted in src/dialog/dialog.c
 * and linked for real; its caller test (tests/life/main.c
 * test_main_menu_continue_quit) pre-allocates the three slide workspace
 * buffers so the real teardown's memmoves stay in bounds, and its own caller
 * test lives in tests/dialog/dialog.c. */
int g_chapter_transition_return = 0;
int fd2_chapter_transition_menu(void) { return g_chapter_transition_return; }
/* fd2_animate_scroll_up_in_shop_dialog / _down_in_shop_dialog (the shop/roster
 * dialog scroll-page animations @ 0x2E19B / 0x2E26C; real bodies not yet
 * emitted). Both are pure VGA-framebuffer memmove/memset paced loops over the
 * absolute shop-grid region (0xA8FCA..), so a recording stub suffices: the
 * roster-select loop (src/ui_menu/chintro.c) calls one of them on each
 * viewport-page transition, and tests assert the per-direction call count.
 * Remove these doubles when the real functions are emitted. */
int g_scroll_up_in_shop_calls = 0;
int g_scroll_down_in_shop_calls = 0;

/* fd2_build_promotion_candidates_with_targets @ 0x31793: REAL in
 * src/ui_menu/promote.c. It drives the real per-char eligibility scan and the
 * real fd2_find_inventory_slot_with_item (also real, src/ui_menu/status.c) for
 * its key-item / Sword target-class branches. Tests grant a char an item by
 * populating g_test_rc_array's real inventory slots, so no find-item fake is
 * needed here. The class-promotion menu's count==0 early-return path is driven
 * by an ineligible (under-level) party on g_test_rc_array. */

/* fd2_roll_stat_gain_and_show_message @ 0x1E529: not yet emitted (its real
 * emit lands in battle/btl_turn.c); controllable fake so its two callers
 * (fd2_execute_class_promotion_with_dialog @ 0x31602 and
 * fd2_process_xp_and_level_up_for_char) link and can be driven without the
 * real RNG roll + blocking stat-gain dialog. The fake records the threaded
 * 4-row cursor it was last handed and returns g_roll_stat_next_row so the
 * caller's row-threading and final-row-as-spell-offset wiring stay testable;
 * it does NOT mutate *stat_ptr (the real roll's stat add is exercised in the
 * real function's own emit test, not via this caller).
 *
 * g_roll_stat_arm_kbd_on_call: when set, each invocation pre-arms the BIOS
 * keyboard buffer NONEMPTY. The class-promotion caller drains the buffer
 * (fd2_clear_keyboard_buffer) BEFORE these rolls and then, in its learned-
 * spell branch, blocks on fd2_wait_for_input_dialog_with_blink(0) AFTER them;
 * an in-process test cannot inject the awaited keypress between the drain and
 * that wait, so the fake stands in for the player's keypress (the real roll's
 * own clear/redisplay does not run here). The intervening real
 * fd2_display_dialog_scene does not drain the buffer (proven by the sibling
 * no-candidates test), so the armed state survives to the blink-wait. */
int    g_roll_stat_calls = 0;
short *g_roll_stat_last_stat_ptr = 0;
uint8 *g_roll_stat_last_growth_ptr = 0;
uint32 g_roll_stat_last_text_id = 0;
int    g_roll_stat_last_row = 0;
int    g_roll_stat_next_row = 0;
int    g_roll_stat_arm_kbd_on_call = 0;
int fd2_roll_stat_gain_and_show_message(short *stat_ptr, uint8 *growth_pair_ptr,
                                        uint32 dialog_text_id, int row_idx)
{
    g_roll_stat_calls++;
    g_roll_stat_last_stat_ptr = stat_ptr;
    g_roll_stat_last_growth_ptr = growth_pair_ptr;
    g_roll_stat_last_text_id = dialog_text_id;
    g_roll_stat_last_row = row_idx;
    if (g_roll_stat_arm_kbd_on_call) {
        *(volatile uint16 *)0x41AuL = 0x1E;          /* head                 */
        *(volatile uint16 *)0x41CuL = 0x20;          /* tail = head+2 -> nonempty */
        *(volatile uint16 *)0x41EuL = 0x1C00;        /* Enter scancode in AH */
    }
    return g_roll_stat_next_row;
}
/* g_fade_to_black_calls counting recorder for the rsrc cinematic-load tests
 * (tests/rsrc/rsrc.c). The real fd2_play_palette_fade_to_black (src/gfx/palette.c)
 * supersedes the counting stub at link; the recorder is retained so those tests
 * link. */
int g_fade_to_black_calls = 0;
int g_slot_selector_return = -1;
int fd2_save_slot_selector_ui(uint32 b, uint32 m) { (void)b; (void)m; return g_slot_selector_return; }
void fd2_close_intro_dialog_with_slide_out(void) { }
/* fd2_chapter_transition_menu: now emitted in src/field/chtrans.c and linked
 * for real. (g_chapter_transition_return is gone; the orphan extern decls in
 * the per-suite boilerplate blocks are unused and harmless.) */

/* fd2_chapter_transition_with_intro: now emitted in src/field/chtrans.c and
 * linked for real. Its not-yet-emitted callees (the three intro menus, the
 * scaled-pose blit, and the two pose target tables) are stubbed/faked below so
 * TEST.EXE links; the menu commit results default nonzero. */
int g_chapter_intro_menu_return = 1;
int fd2_run_chapter_intro_menu_typeB(uint32 pose_bitmap)
{
    (void)pose_bitmap;
    return g_chapter_intro_menu_return;
}
int fd2_run_chapter_intro_menu_typeC(uint32 pose_bitmap)
{
    (void)pose_bitmap;
    return g_chapter_intro_menu_return;
}
int fd2_run_chapter_intro_menu_main(uint32 pose_bitmap)
{
    (void)pose_bitmap;
    return g_chapter_intro_menu_return;
}
void fd2_render_chapter_intro_overlay(void) { }
int g_run_recruitment_return = 1;
int fd2_run_recruitment_or_branch_screen(void)
{
    return g_run_recruitment_return;
}
void fd2_save_current_state_to_slot(int slot) { (void)slot; }

/* ---- fd2_load_save_and_init_engine leaf helper fakes ----
 * (the real fd2_load_save_and_init_engine now lives in src/life/main.c) */
/* fd2_save_compute_checksum: now emitted in src/save/save.c (the loader
 * checksum test now stores a real computed checksum in the FD2.SAV tail). */
/* fd2_save_crypt_buffer: now emitted in src/save/save.c (the FD2.SAV fixture
 * encrypts its image so the loader's real decrypt recovers the plaintext). */
/* fd2_load_chapter_background_layers: now in rsrc/rsrc.c */
/* fd2_load_portrait_to_cache: now emitted in src/rsrc/rsrc.c */
/* fd2_alloc_and_blit_indexed_sprite_chunk: now emitted in src/gfx/blitspr.c.
 * It calls fd2_save_screen_block_to_buffer exactly once per invocation, so the
 * save-block call counter (g_saveblk_calls) is an exact proxy for the
 * alloc/blit-chunk call count in any test that drives it in isolation. */
/* fd2_render_decimal_number_to_buffer: now emitted in src/gfx/rndstat.c and
 * linked for real. Its caller tests (panel / inventory / redfull in
 * tests/gfx/rndstat.c) drive the real renderer end-to-end through the real
 * fd2_blit_indexed_sprite_at_xy -> fd2_rle_blit_sprite spy, recovering each
 * rendered digit/overflow glyph from the g_rle_blit_log_* per-call log against
 * a fake sheet (table[i]=i). No argument-recording spy remains. */
/* fd2_render_hp_or_mp_bar_proportional: now emitted in src/gfx/rndstat.c. The
 * panel tests drive the real function, which computes the proportional segment
 * count and forwards to the real fd2_render_horizontal_bar_segments ->
 * fd2_blit_sheet_sprite_at_offset pipeline, so the bars are observed end-to-end
 * through the g_blitraw_* sprite log (no bar-specific spy needed). */
/* fd2_render_number_red_when_full: now emitted in src/gfx/rndstat.c. It is a
 * thin wrapper that forwards into the real fd2_render_decimal_number_to_buffer
 * with a red/white color chosen by current==max; the panel/redfull tests
 * observe its rendered digit glyphs through the g_rle_blit_log_* log. */
int    g_delay375b2_calls = 0;
uint32 g_delay375b2_last_ticks = 0;
/* Opt-in ordered tick log (default off; additive — existing tests only read the
 * count + last_ticks above). The white-flash cinematic test turns this on to pin
 * its exact 300 / 200 / 400 delay sequence (the third value arrives via the
 * fd2_delay_400ms_via_idle_thunk tail-call below, which forwards 400 here). */
int    g_delay375b2_log_on = 0;
int    g_delay375b2_log_count = 0;
uint32 g_delay375b2_log[16];
void __delay_thunk_375b2(uint32 ticks)
{
    g_delay375b2_calls++;
    g_delay375b2_last_ticks = ticks;
    if (g_delay375b2_log_on && g_delay375b2_log_count < 16) {
        g_delay375b2_log[g_delay375b2_log_count] = ticks;
        g_delay375b2_log_count++;
    }
}
/* fd2_delay_400ms_via_idle_thunk @ 0x353CC: a separately-routed real function
 * (target src/util/misc.c, not yet emitted) whose entire body is
 * __delay_thunk_375b2(400). The white-flash cinematic tail-calls it (JMP 0x353CC)
 * for its final 400ms hold. Stub forwards to the real delay thunk so callers link
 * and the 400 is observed in the delay log; remove when misc.c emits the real
 * body. */
void fd2_delay_400ms_via_idle_thunk(void) { __delay_thunk_375b2(400); }

/* fd2_animate_palette_flash_pulse_white @ 0x35E5A: a separately-routed real
 * function (target src/anim/aniui.c, not yet emitted) whose body is the
 * ~1.4s pulse-white palette flash (a 64-step fade-up, 400ms peak hold, 63-step
 * fade-down, all driven through fd2_set_vga_palette_range_with_add +
 * __delay_thunk_375b2). The ch29 endgame handler_4c fires it six times, so a
 * real run would churn ~750 palette writes and ~750 delay-log entries with no
 * value to that handler's risk-bearing logic (its branch, the 8-bit
 * party_member_count-3 / turn-counter stores, and the flash/dialog sequencing).
 * The recording stub counts invocations so the caller can pin the exact flash
 * count without that churn; remove when aniui.c emits the real body. */
int g_palette_flash_pulse_white_calls = 0;
void fd2_animate_palette_flash_pulse_white(void)
{
    g_palette_flash_pulse_white_calls++;
}

/* fd2_cinematic_chapter_portrait_dump_with_white_flash is now a real emitted
 * function (src/field/chevt2.c); its former (x, y, id) recording stub here was
 * removed. The handler_34 + wrap-thunk suites that used to spy on this stub now
 * drive the real cinematic over a host-safe render/portrait/palette env and
 * observe its forwarded args one level down: the masked portrait id via the real
 * fd2_load_chapter_portraits_and_dump_tmp race-scan (party member count), the
 * pan target via the real fd2_pan_cursor_and_window window origin, and the
 * per-call delay sequence via the g_delay375b2_log above. */

/* Recording stub for the still-unemitted callee
 * fd2_kill_runtime_chars_from_index_to_end (routing target battle/btl_turn.c,
 * not yet emitted). The real function zeroes hp_current for runtime_char slots
 * [start_char_idx .. party_member_count) and then plays the death animation;
 * that loop is the callee's own behavior and is covered when 0x35BBA is emitted
 * into btl_turn.c. Recording each call's start index + call count lets
 * fd2_chapter_event_handler_35's test pin THIS handler's contract: a single
 * kill call with start index 0x12, issued AFTER the page-5 dialog. The stub
 * will be replaced by the real body when its routing target is emitted. */
int    g_kill_from_calls = 0;
uint32 g_kill_from_index[4];

/* Recording stub for the still-unemitted callee fd2_cinematic_warp_char_to_tile
 * (routing target anim/aniui.c, not yet emitted). The real helper pans the camera
 * to a tile then runs the full warp-teleport char animation
 * (fd2_animate_warp_teleport_char: FDOTHER.DAT SFX load, a 150KB snapshot, portal
 * open/collapse/expand frames, and a row-by-row "pop-in" copy to the mode-13h
 * framebuffer). That entire chain is pure display owned by the spellcin warp suite
 * and deferred to Phase 9; running it just to reach a caller's downstream logic
 * would churn real file I/O + framebuffer writes with no value to the caller's
 * risk-bearing routing. Recording each call's (char_id, tile_x, tile_y) lets
 * fd2_chapter_event_handler_52's test pin THIS handler's contract: the exact warp
 * char-index arithmetic (boss = 0x18-stage; pair = 2*stage+0x19 / +0x1A) and the
 * literal tile targets. The stub will be replaced by the real body when aniui.c
 * emits 0x33F78. */
int    g_warp_char_calls = 0;
uint32 g_warp_char_id[4];
uint32 g_warp_tile_x[4];
uint32 g_warp_tile_y[4];
void fd2_cinematic_warp_char_to_tile(uint32 char_id, uint32 tile_x, uint32 tile_y)
{
    if (g_warp_char_calls < 4) {
        g_warp_char_id[g_warp_char_calls] = char_id;
        g_warp_tile_x[g_warp_char_calls] = tile_x;
        g_warp_tile_y[g_warp_char_calls] = tile_y;
    }
    g_warp_char_calls++;
}

/* Link-time stub for the still-unemitted callee of the orphan/unreachable
 * fd2_execute_aoe_spell_with_caster_portrait_radial_scatter (src/spell/spellcin.c).
 * That AoE cinematic has no caller and no test drives it (its per-frame work is
 * unconditional VRAM blits to the hardcoded mode-13h literal 0xA0504, deferred to
 * Phase 9 like the rest of spellcin.c). This stub only needs to resolve the
 * symbol; its real body is owned by its routing target
 * (fd2_blit_palette_remap_with_sprite_mask -> gfx/blitspr.c) and will replace
 * this when emitted. (The sibling scatter callee
 * fd2_scatter_sprite_around_origin_with_random_offset is now the real emitted
 * function in src/spell/spellcin.c; its former stub here was removed.) */
void fd2_blit_palette_remap_with_sprite_mask(
    uint8 *dst, uint16 *sprite_mask, uint32 stride, uint32 remap_table) {
    (void)dst; (void)sprite_mask; (void)stride; (void)remap_table;
}

/* fd2_animate_party_addition_with_appear_effect (@0x32999) is not yet emitted.
 * It is a heavy 12-frame "new char appearance" explosion animation (real
 * FDOTHER.DAT reads, 0x25680-byte back-buffer snapshots, per-new-char sprite
 * blits, and a portrait reload that rewrites FD2.TMP). A recording stub here
 * lets callers that merely fire it (the ch1 turn-event handlers) be tested for
 * their own contract — that they invoke it once with the right chapter id —
 * without dragging the full animation pipeline into the unit test. Its own
 * display/state effects are covered when that function is emitted. */
int    g_animate_party_addition_calls = 0;
uint32 g_animate_party_addition_last_chapter = 0;
void fd2_animate_party_addition_with_appear_effect(uint32 chapter_id) {
    g_animate_party_addition_calls++;
    g_animate_party_addition_last_chapter = chapter_id;
}

/* fd2_show_chapter_intro_text_dialog_mode_3 is now a real emitted function
 * (src/field/chevt1.c); its former recording stub here was removed. The real
 * class-3 "shared tail" helper dispatches dialog page 3 through the real
 * fd2_display_dialog_scene VM, so its sole caller fd2_chapter_event_handler_16
 * is now tested against the real page-3 dispatch (the glyph recorder) instead
 * of the stub counter; see tests/field/chevt11.c (handler_16) and
 * tests/field/chevt13.c (the helper itself). */

/* fd2_blit_money_digit_sprite: the per-digit slot-machine blit primitive, not
 * yet emitted (target gfx/blitspr.c). Recording stub: the money-roller animations
 * (fd2_animate_money_increment / _decrement) drive it once per (digit, frame);
 * the tests observe the rolling structure through the call count + last args
 * (resolved screen slot, stride, and sprite index = cur_digit*9 + anim_phase).
 * The actual sprite pixel copy is a display side-effect deferred to Phase 9. */
int    g_money_blit_calls = 0;
uint32 g_money_blit_last_dst = 0;
uint32 g_money_blit_last_stride = 0;
uint32 g_money_blit_last_sprite = 0;
void fd2_blit_money_digit_sprite(uint32 dst_buf, uint32 dst_stride, uint32 sprite_idx)
{
    g_money_blit_calls++;
    g_money_blit_last_dst = dst_buf;
    g_money_blit_last_stride = dst_stride;
    g_money_blit_last_sprite = sprite_idx;
}

/* fd2_setup_chars_and_camera_for_intro recording fake. The real function (not yet
 * emitted; assigned to src/field/chtrans.c) is a chapter intro scene stager:
 * palette fade-out, place chars in the [char_start..char_end] range using the
 * three byte-array tables, reset the camera, composite + fade-in. That is all VGA
 * display side-effect deferred to Phase 9, so here we only record the call and
 * snapshot the three placement-block tables + scalar args, letting a caller test
 * (field/chend1's chapter 03/05/07/08 end) verify the orchestration: which branch
 * invoked it, the X/Y/facing tables copied into the on-stack blocks, the char
 * index range, the facing argument, and the camera origin.
 *
 * The real function consumes exactly (char_end - char_start + 1) table entries
 * (it indexes the byte arrays by the inclusive [char_start..char_end] loop var),
 * so the snapshot loop is bounded the same way: chapter 03/05 pass 7-entry blocks
 * (char_end == 6), chapter 07 passes 9-entry blocks (char_end == 8), and chapter
 * 08 passes 10-entry blocks (char_end == 9, only entries 0..9 captured into the
 * size-9-indexed buffers via the count clamp). Bounding by the live range avoids
 * reading past a caller's on-stack block. The facing argument is a byte-array
 * table address (>= 4) for chapters 3/5/7/12/14 and an inline fixed facing value
 * (< 4) for chapter 8; g_setup_intro_facing_arg records the raw value. Buffers
 * are sized 16 to hold the widest caller's live range (chapter 14 places chars
 * 0..0xF = 16 entries); narrower callers (chapters 3/5/7/8/12) fill only their
 * leading entries. */
int    g_setup_intro_calls = 0;
uint8  g_setup_intro_px[16];
uint8  g_setup_intro_py[16];
uint8  g_setup_intro_facing[16];
uint32 g_setup_intro_facing_arg = 0xFFFFFFFFuL;
int32  g_setup_intro_char_start = -1;
int32  g_setup_intro_char_end = -1;
uint32 g_setup_intro_extra_char_idx = 0xFFFFFFFFuL;
int32  g_setup_intro_extra_pos_x = -1;
int32  g_setup_intro_extra_pos_y = -1;
int32  g_setup_intro_extra_facing = -1;
uint32 g_setup_intro_camera_x = 0xFFFFFFFFuL;
uint32 g_setup_intro_camera_y = 0xFFFFFFFFuL;
void fd2_setup_chars_and_camera_for_intro(uint32 px_table, uint32 py_table,
                                          uint32 facing_table_or_fixed,
                                          int32 char_start, int32 char_end,
                                          uint32 extra_char_idx, int32 extra_pos_x,
                                          int32 extra_pos_y, int32 extra_facing,
                                          uint32 camera_origin_x,
                                          uint32 camera_origin_y)
{
    int i;
    int count;
    g_setup_intro_calls++;
    g_setup_intro_facing_arg = facing_table_or_fixed;
    count = char_end - char_start + 1;
    if (count < 0) {
        count = 0;
    }
    if (count > 16) {
        count = 16;
    }
    for (i = 0; i < count; i++) {
        g_setup_intro_px[i]     = ((uint8 *)px_table)[char_start + i];
        g_setup_intro_py[i]     = ((uint8 *)py_table)[char_start + i];
        /* The real function treats facing_table_or_fixed < 4 as an inline fixed
         * facing applied to every placed char (chapter 8); >= 4 is a byte-array
         * table address indexed per char (chapters 3/5/7). Model both so the
         * fixed case does not dereference a non-pointer. */
        if (facing_table_or_fixed < 4) {
            g_setup_intro_facing[i] = (uint8)facing_table_or_fixed;
        } else {
            g_setup_intro_facing[i] = ((uint8 *)facing_table_or_fixed)[char_start + i];
        }
    }
    g_setup_intro_char_start = char_start;
    g_setup_intro_char_end = char_end;
    g_setup_intro_extra_char_idx = extra_char_idx;
    g_setup_intro_extra_pos_x = extra_pos_x;
    g_setup_intro_extra_pos_y = extra_pos_y;
    g_setup_intro_extra_facing = extra_facing;
    g_setup_intro_camera_x = camera_origin_x;
    g_setup_intro_camera_y = camera_origin_y;
    /* Camera re-aim (load-bearing STATE for the integ cinematic suites): point the
     * view window + cursor at the camera origin so the pan helpers that follow
     * (fd2_pan_cursor_and_window) terminate instead of looping ~2^31 times from a
     * prior suite's stale origin. */
    data_fd2_battle_view_window_origin_x = camera_origin_x;
    data_fd2_battle_view_window_origin_y = camera_origin_y;
    data_fd2_battle_cursor_world_x = camera_origin_x;
    data_fd2_battle_cursor_world_y = camera_origin_y;
}

/* fd2_composite_chars_with_spell_effect_overlay is now a real emitted function
 * (src/gfx/rndscene.c); its former recording stub here was removed. The real
 * overlay first composites a tile map (observable via the
 * fd2_composite_battle_tile_map dst log) and then, per targeted char, draws the
 * effect sprite through the recording fd2_blit_sprite_with_decoded_pixels stub
 * (whose opt-in log captures the resolved fx-sprite addr per call). Its own
 * behavior is covered by tests/gfx/rndscene.c; the caller
 * fd2_animate_spell_full_screen_flash observes those two seams. */
int g_ail_vol_calls = 0;
int g_ail_last_vol = 0;
int g_ail_last_ramp = 0;
void AIL_set_sequence_volume(uint32 s, int t, int r) { g_ail_vol_calls++; g_ail_last_vol = t; g_ail_last_ramp = r; (void)s; }
void AIL_stop_sequence(uint32 s) { (void)s; }
int  AIL_init_sequence(uint32 s, uint32 d, int i) { (void)s; (void)d; (void)i; return 0; }
void AIL_start_sequence(uint32 s) { (void)s; }
void AIL_set_sequence_loop_count(uint32 s, uint32 c) { (void)s; (void)c; }
/* AIL sound-system lifecycle stubs. Referenced only by fd2_main (whose own
 * behavioral test is deferred to Phase 9 integration -- it issues INT 10h via
 * the real linked int386, which has no deterministic seam in the DOS/4GW
 * harness; see src/emit_issues.json @00025bf4). These exist purely to satisfy
 * the link; returning NULL handles keeps fd2_main's "driver installed?" arms
 * un-taken if it were ever driven. */
void  AIL_startup(void) {}
void  AIL_shutdown(void) {}
int   AIL_install_MDI_INI(void) { return 0; }
int   AIL_install_DIG_INI(void) { return 0; }
void *AIL_allocate_sequence_handle(void *mdi_driver) { (void)mdi_driver; return (void *)0; }
void *AIL_allocate_sample_handle(void *dig_driver) { (void)dig_driver; return (void *)0; }
/* fd2_play_chapter_clear_fanfare: chapter-clear jingle, not yet emitted;
 * referenced only by the (Phase 9-deferred) fd2_main loop. */
void fd2_play_chapter_clear_fanfare(void) {}
/* fd2_load_dat_resource: now emitted in src/rsrc/rsrc.c. Its caller tests
 * drive the real loader against the staged real DAT files (copied into the
 * test cwd by build_test.py) and cross-check its output against an independent
 * parse via tests/include/realfile.h. */
int    g_rle_blit_calls = 0;
uint32 g_rle_blit_last_sprite = 0;
int32  g_rle_blit_last_x = 0;
int32  g_rle_blit_last_y = 0;
uint32 g_rle_blit_last_buf = 0;
int32  g_rle_blit_last_stride = 0;
uint32 g_rle_blit_last_palette = 0;
int32  g_rle_blit_y_log[4];
uint8  g_rle_blit_sprite_first_byte_log[4];
/* opt-in full per-call log (default off; used by the decimal-number digit
 * renderer test to recover each digit's resolved sprite addr + dst, and by the
 * HUD tests to pin the backdrop blit (now no longer the LAST rle blit, since the
 * real signed-modifier renderer appends sign-icon + digit blits after it)). */
int    g_rle_blit_log_on = 0;
uint32 g_rle_blit_log_sprite[64];
uint32 g_rle_blit_log_dst[64];
int32  g_rle_blit_log_stride[64];
uint32 g_rle_blit_log_palette[64];
int32  g_rle_blit_log_y[64];
int32  g_rle_blit_log_x[64];
uint8  g_rle_blit_log_first_byte[64];
void fd2_rle_blit_sprite(uint32 rle_stream, int32 dst_x, int32 dst_y,
                         uint32 dst_buf, int32 stride, uint32 palette_op) {
    g_rle_blit_last_sprite = rle_stream;
    g_rle_blit_last_x = dst_x;
    g_rle_blit_last_y = dst_y;
    g_rle_blit_last_buf = dst_buf;
    g_rle_blit_last_stride = stride;
    g_rle_blit_last_palette = palette_op;
    if (g_rle_blit_calls < 4) {
        g_rle_blit_y_log[g_rle_blit_calls] = dst_y;
        /* first payload byte of the loaded sprite, used by the rsrc tests to
         * verify which DAT index the real loader fetched (each fixture seeds
         * payload[idx][0] = idx). */
        g_rle_blit_sprite_first_byte_log[g_rle_blit_calls] =
            (rle_stream != 0) ? *(uint8 *)rle_stream : 0;
    }
    if (g_rle_blit_log_on && g_rle_blit_calls < 64) {
        g_rle_blit_log_sprite[g_rle_blit_calls] = rle_stream;
        g_rle_blit_log_dst[g_rle_blit_calls] = dst_buf;
        g_rle_blit_log_stride[g_rle_blit_calls] = stride;
        g_rle_blit_log_palette[g_rle_blit_calls] = palette_op;
        g_rle_blit_log_y[g_rle_blit_calls] = dst_y;
        g_rle_blit_log_x[g_rle_blit_calls] = dst_x;
        /* capture the first payload byte NOW (the sprite buffer may be freed
         * by the caller's cleanup before the test inspects it). */
        g_rle_blit_log_first_byte[g_rle_blit_calls] =
            (rle_stream != 0) ? *(uint8 *)rle_stream : 0;
    }
    g_rle_blit_calls++;
}
/* fd2_render_signed_modifier_with_icon: now emitted in src/gfx/rndstat.c. The
 * rndstat HUD tests drive the real function (through its only caller
 * fd2_render_terrain_info_hud_panel), observing the sign-icon blit and the
 * 2-digit magnitude glyphs end-to-end via the g_rle_blit_log_* per-call log
 * against the fake sheet (table[i]=i). The former (dst,stride,modifier)
 * recording stub was removed. */
/* fd2_scroll_text_screen_up_by_lines: now emitted in src/dialog/dialog.c. Its
 * own dual-mode + cylinder-scroll behavior is covered by the dialog tests; the
 * rsrc caller test (chapter 0x17) drives the real tail call against a bounded
 * static_bg buffer. The former (calls, last_arg) recording stub was removed. */
void fd2_play_palette_fade_in(void) { }
void fd2_play_death_animation_and_mark_dead(void) { }
/* fd2_animate_party_addition_with_appear_effect: now emitted for real in
 * src/anim/aniui.c and linked. Its 12-frame appearance-animation skeleton
 * (frame-1 SFX, frame-7/8 tile-map composites, per-new-char explosion blit)
 * is driven by the test_party_add_* cases in tests/anim/aniui.c through the
 * real fd2_load_dat_resource (staged FDOTHER.DAT) + the recording
 * fd2_composite_battle_tile_map / fd2_blit_sprite_with_decoded_pixels spies.
 * The former no-op stub here was removed (it shadowed the real symbol). */
/* p4 recorders: the real fd2_scroll_text_screen_up_by_lines (src/dialog/dialog.c)
 * and fd2_play_ani_file_animation_sequence (driven by the rsrc cinematic-load
 * path) supersede their recording stubs at link; these globals are retained so
 * the rsrc/dialog suites link (they seed/assert the real arg pass-through). */
int    g_scroll_text_calls = 0;
uint32 g_scroll_text_last_arg = 0;
int    g_play_ani_calls = 0;
uint32 g_play_ani_last_idx = 0;
uint32 g_play_ani_last_delay = 0;
uint32 g_play_ani_last_skip = 0;

/* Turn-cycle display/dispatch callees driven by fd2_run_full_turn_cycle.
 * fd2_fire_chapter_turn_events_for_phase is now emitted for real in
 * src/battle/btl_turn.c; its former counting stub was removed and the
 * caller tests drive the real dispatcher via an in-memory tile-event
 * table + spy handlers (see tests/battle/btl_turn.c).
 * fd2_maybe_load_speed_mode_overlay / fd2_maybe_free_speed_mode_overlay:
 * now emitted in src/ui_menu/menucfg.c. */
/* fd2_animate_phase_banner_slide_in / fd2_animate_phase_banner_slide_out:
 * both now emitted for real in src/anim/anicombt.c; their former counting
 * stubs here were removed. fd2_render_phase_banner_frame is now also emitted
 * for real (src/gfx/rndscene.c), so its former counting stub
 * (g_render_phase_banner_frame_calls / _last_x) was removed. The real per-frame
 * renderer drives two real fd2_alloc_and_blit_indexed_sprite_chunk +
 * fd2_blit_rectangle + fd2_wait_n_bios_ticks + two real
 * fd2_cleanup_dialog_sprite_buffer calls; each frame render therefore makes
 * exactly two fd2_restore_screen_block_from_buffer calls (g_restore_block_calls
 * counts only frame-render cleanups — the fade loops free their save buffers
 * directly, not via cleanup), which the banner slide tests use as the exact
 * per-frame counter. The frame renderer's own x_offset->col_offset arithmetic
 * and full call sequence are pinned by a dedicated test in tests/gfx/rndscene.c.
 * The turn-cycle test (battle/btl_turn.c) counts frame renders the same way. */
/* Vertical-scroll block copy: callee of the now-real banner slide_in /
 * slide_out, not yet emitted. Records call count (and the last wrap_param) so
 * the banner tests can pin their fade loops: slide_in scrolls 16x with
 * scroll_offset advancing 1..16; slide_out scrolls 17x with scroll_offset
 * counting 0x11..1. */
int g_scroll_buffer_calls = 0;
uint32 g_scroll_buffer_last_wrap = 0;
void fd2_scroll_buffer_block_with_wrap(uint32 wrap_param, void *dst_buf,
                                       void *src_buf) {
    g_scroll_buffer_calls++;
    g_scroll_buffer_last_wrap = wrap_param;
    (void)dst_buf;
    (void)src_buf;
}
/* fd2_process_battle_drop_entries: now emitted for real in
 * src/battle/btl_turn.c; its former noop stub here was removed. The
 * battle/btl_turn.c drop tests drive the real function (control-flow gates +
 * type-2 chapter-event dispatch); the type-0/1 display sequences are deferred
 * to Phase 9 integration. */
/* fd2_cast_group_hp_heal_spell: now emitted for real in
 * src/spell/spelleff.c; its former no-op stub here was removed. The
 * spell/spelleff.c group-heal tests drive the real function (per-target
 * heal loop over the real fd2_apply_hp_heal_and_award_xp + real impact/
 * flicker/composite callees). */
/* fd2_cast_status_cure_spell: now emitted for real in src/spell/spelleff.c
 * and linked; its former call-counting stub here was removed. The
 * spell/spelleff.c status-cure tests drive the real function (per-target
 * status-byte check + clear, real fd2_apply_hp_heal_and_award_xp heal, real
 * impact/flicker/composite callees). g_cast_status_cure_calls is retained as
 * a defined global because many test files still carry its extern in their
 * boilerplate decl block (none increment it now). */
int g_cast_status_cure_calls = 0;
/* fd2_cast_status_inflict_spell @ 0x22D1B: now emitted for real in
 * src/spell/spelleff.c and linked; its former call-recording stub here was
 * removed. The spellef1.c inflict tests drive the real function (per-target
 * affliction roll + status-byte timer write, real fd2_apply_damage_and_award_
 * xp, real impact/flicker/composite callees), and the d1b-wrapper test below
 * verifies verbatim arg forwarding through the real worker's observable
 * effects (MP deduct on the forwarded caster/spell + affliction landing on the
 * forwarded target at the forwarded sprite_id). */
/* fd2_cast_status_spell_via_d1b: now emitted for real in src/spell/spelleff.c
 * and linked; its former call-counting stub here was removed. The spellef1.c
 * d1b-wrapper test drives the real function (real fd2_deduct_caster_mp MP
 * deduction + AoE-index reset + verbatim forward into the REAL inflict worker).
 * g_cast_status_via_d1b_calls is retained as a defined global because other
 * spell test files still carry its extern in their boilerplate decl block (none
 * increment it now). */
int g_cast_status_via_d1b_calls = 0;
/* fd2_render_mini_char_status_panel @ 0x18c6d: now emitted for real in
 * src/gfx/rndstat.c and linked. Its callers' tests (fd2_flash_char_hit_sprite
 * in tests/battle/battle2.c) drive the real painter via the shared mini-panel
 * fixture (tests/include/minipfix.h) and observe the forwarded buf/char through
 * the real background blit + sleep-indicator digit, so no stub/spy is kept. */
/* fd2_tick_tutorial_progress_with_sfx: now in anim.c */
/* fd2_run_full_turn_cycle: now emitted in src/battle/btl_turn.c */
/* fd2_enemy_turn_action_dispatcher: now in btl_ai.c */
/* fd2_ai_score_offensive_spell: now in btl_ai.c */
/* fd2_build_usable_spell_list: now real in src/spell/spellsel.c. Tests drive it
 * via the queried unit's spells_known_bitmap (+0x1A) so the real enumerator
 * produces the desired (count, ascending ids). */
/* fd2_score_spell_candidate: now in btl_ai.c */
/* fd2_ai_score_item_use: now in btl_ai.c */
/* fd2_count_usable_inventory_slots: now REAL in src/ui_menu/status.c */
/* fd2_spell_selection_menu_main is now emitted for real in src/spell/spellsel.c
 * and linked; its former counting stub (and the g_inline_spell_menu_return /
 * _calls / _pending seams that drove it) were removed. The inline-action
 * dispatcher's Spell-branch behavioral coverage (case-1 commit XP scaling /
 * cancel return 0) is a heavy-UI input-loop path -- the real spell modal runs
 * its own input loop (fd2_spell_select_input_loop) and target-pick prompts on a
 * keyboard read with no async key source in the host harness -- so it is
 * deferred to Phase 9 integration, the same deferral applied to the Item branch.
 * fd2_spell_select_input_loop (0x1D51D), the modal's per-frame input handler, is
 * now emitted for real in src/spell/spellsel.c and linked; its former noop stub
 * (which returned -1 to terminate the do/while) was removed.
 *
 * fd2_play_spell_palette_flash_with_sfx (0x1D6C8) -- VGA DAC palette flash + SFX
 * for status-class spells -- is now emitted for real in src/spell/spellsel.c and
 * linked; its former noop stub was removed. Its flash colour source,
 * data_fd2_animation_spell_palette_flash_table, is a const data table in the
 * binary; the harness defines it below (seeded with the real binary bytes). */
/* fd2_equip_unequip_inventory_menu is now emitted for real in
 * src/ui_menu/status.c and linked; its former no-op stub was removed. The
 * Sort/Equip modal is a heavy inventory-equip UI loop with no in-process input
 * seam (its end-to-end behavior is deferred to Phase 9 — see the deferral note
 * in tests/ui_menu/status.c), so no test drives it directly; the real body now
 * satisfies the link. Its equip-decision callee fd2_check_job_can_equip_item
 * (0x1C1C3) is now emitted for real in src/ui_menu/status.c; its former no-op
 * stub was removed. */
/* fd2_item_command_menu_dispatch is now emitted for real in
 * src/ui_menu/status.c and linked; its former counting stub (and the
 * g_inline_item_menu_return / g_inline_item_menu_calls seams that drove it)
 * were removed. The inline-action dispatcher's Item-branch behavioral coverage
 * (case-2 commit / cancel) is a heavy-UI input-loop path — the real item
 * command menu opens its own settings dialog and blocks on a keyboard read
 * whose buffer the caller's close already cleared — so it is deferred to
 * Phase 9 integration, the same deferral the file applies to the other
 * non-isolable heavy-UI submenus. */
/* fd2_handle_tile_event_interaction is now emitted for real in
 * src/ui_menu/menufld.c and linked; its former counting stub was removed. The
 * inline-action dispatcher's Wait-branch tests now drive the real handler, which
 * gate-returns on a non-event cursor tile (see tests/ui_menu/menu.c iam_setup's
 * zeroed tile-map / attr buffers). Its own behavioral coverage lives in
 * tests/ui_menu/menufld.c. */
/* 10-entry summon-spell tick dispatch table (@ 0x523B9). Real entries return an
 * int frame count and perform per-element palette flash / sprite tick. The
 * fd2_animate_spell_hit_cinematic test drives the cinematic and needs this
 * table populated (the binary calls through it twice per frame). All slots are
 * wired to a single spy that records the phase_code (arg5) sequence and the
 * call count so the test can pin the per-phase dispatch order; the returned
 * frame count is ignored by the cinematic, so the spy returns a fixed value. */
int    g_spell_phase_handler_calls = 0;
int    g_spell_phase_handler_log_count = 0;
int    g_spell_phase_handler_phase_log[64] = {0};
uint32 g_spell_phase_handler_arg2_log[64] = {0};
uint32 g_spell_phase_handler_dst_log[64] = {0};
int g_blit_indexed_sprite_calls = 0;
uint32 g_blit_indexed_sprite_last_frame = 0;
int g_blit_indexed_sprite_last_x = 0;
int g_blit_indexed_sprite_last_y = 0;
/* opt-in per-call frame-index log (default off; used by the cycle-sprite-anim
 * test to recover the full frame-advance sequence produced by the atlas-driven
 * hold/wrap state machine in fd2_cycle_sprite_anim_with_bg_frames). */
int g_blit_indexed_log_on = 0;
uint32 g_blit_indexed_log_frame[64];
/* Per-call atlas/frame log (off by default). The FIGANI animation-loop test
 * pins the team/spell-id-dependent composite ORDER by reading the atlas (caster
 * vs target FIGANI stream pointer) and frame index of each indexed-sprite blit
 * in sequence. */
int    g_blit_indexed_log_count = 0;
uint32 g_blit_indexed_atlas_log[64] = {0};
uint32 g_blit_indexed_frame_log[64] = {0};
/* per-call dst x/y log (parallel to atlas/frame; additive — existing tests read
 * only atlas/frame). The spell-hit-cinematic test reads x to pin the per-frame
 * slide position (frame*0x23*team_dir_sign + workspace_ptr). */
int    g_blit_indexed_x_log[64] = {0};
int    g_blit_indexed_y_log[64] = {0};
/* additive per-call frame-index log (capacity 128); lets the chapter-intro
 * slideshow test (tests/anim/aniend.c) witness the exact 101-frame ordering and
 * the phase-1 -> phase-2 shared-index continuation. Existing consumers only read
 * the _calls / _last_* scalars and are unaffected. */
uint32 g_blit_indexed_sprite_frame_log[128];
int    g_blit_indexed_sprite_frame_log_n = 0;
void fd2_blit_indexed_sprite(uint32 a, uint32 f, int x, int y, int m) {
    if (g_blit_indexed_log_on && g_blit_indexed_sprite_calls < 64) {
        g_blit_indexed_log_frame[g_blit_indexed_sprite_calls] = f;
    }
    g_blit_indexed_sprite_calls++;
    g_blit_indexed_sprite_last_frame = f;
    g_blit_indexed_sprite_last_x = x;
    g_blit_indexed_sprite_last_y = y;
    if (g_blit_indexed_log_on && g_blit_indexed_log_count < 64) {
        g_blit_indexed_atlas_log[g_blit_indexed_log_count] = a;
        g_blit_indexed_frame_log[g_blit_indexed_log_count] = f;
        g_blit_indexed_x_log[g_blit_indexed_log_count] = x;
        g_blit_indexed_y_log[g_blit_indexed_log_count] = y;
        g_blit_indexed_log_count++;
    }
    if (g_blit_indexed_sprite_frame_log_n < 128) {
        g_blit_indexed_sprite_frame_log[g_blit_indexed_sprite_frame_log_n] = f;
        g_blit_indexed_sprite_frame_log_n++;
    }
    (void)m;
}
/* fd2_rle_blit_with_palette_remap @ 0x4E583 is a display-only RLE sprite
 * decoder with a 256-entry palette LUT; it is not emitted yet (only the FIGANI
 * animation loop references it). Recording spy: the loop's spell-cast-frame
 * block computes a remap_table address from data_fd2_tile_anim_table_base +
 * remap_idx, then passes it as palette_remap. Capturing palette_remap (and the
 * dst_x/dst_y/stream that pin which of the two layer blits) lets the test
 * assert the remap_idx selection numerically without touching real VGA RAM. */
int    g_rle_remap_calls = 0;
int    g_rle_remap_log_count = 0;
int32  g_rle_remap_log_palette[16] = {0};
int32  g_rle_remap_log_dstx[16] = {0};
int32  g_rle_remap_log_dsty[16] = {0};
uint32 g_rle_remap_log_stream[16] = {0};
void fd2_rle_blit_with_palette_remap(uint16 *rle_stream, int32 dst_x, int32 dst_y,
                                     int32 dst_buf, int32 stride, int32 palette_remap) {
    g_rle_remap_calls++;
    if (g_rle_remap_log_count < 16) {
        g_rle_remap_log_palette[g_rle_remap_log_count] = palette_remap;
        g_rle_remap_log_dstx[g_rle_remap_log_count] = dst_x;
        g_rle_remap_log_dsty[g_rle_remap_log_count] = dst_y;
        g_rle_remap_log_stream[g_rle_remap_log_count] = (uint32)rle_stream;
        g_rle_remap_log_count++;
    }
    (void)dst_buf; (void)stride;
}
void  *data_fd2_animation_ani_decoder_frame_dispatch_table[10] = {0};
/* fd2_composite_battle_frame is now a real emitted function (src/gfx/rndscene.c).
 * g_composite_call_count (defined above with the pipeline stubs) remains the
 * observable that existing caller tests (cursor.c, spelleff.c, btl_ai.c, ...)
 * use to count "a composite frame ran"; the real compositor calls
 * fd2_composite_battle_tile_map exactly once per frame, so the tile-map stub
 * bumps it. */

/* --- AI dispatcher stubs + tracking --- */
int g_attack_dispatch_return = 0;
int g_attack_dispatch_calls = 0;
int g_seek_optimal_return = 0;
int g_advance_nearest_return = 0;
int g_walk_return = 0;
int g_score_physical_return = 0;
int g_pass_turn_calls = 0;
int g_execute_spell_calls = 0;
int g_execute_physical_calls = 0;
/* fd2_attack_action_dispatch: now in btl_ai.c */
/* fd2_ai_seek_optimal_position: now in btl_ai.c */
/* fd2_ai_advance_to_nearest_team_target: now in btl_ai.c */
/* fd2_ai_pass_turn_with_heal: now in btl_ai.c */
/* fd2_ai_walk_to_target_tile: now in btl_ai.c */
/* fd2_ai_score_physical_attack: now in btl_ai.c */
/* fd2_execute_ai_offensive_spell: now in btl_ai.c */
/* fd2_play_spell_cast_sequence: now emitted for real in src/anim/anispell.c
 * (its former no-op linker stub was removed). */
/* fd2_execute_ai_physical_attack: now in btl_ai.c */
uint32 fd2_animate_combat_speech_bubbles(uint32 ci, uint32 ti) { return 0; }
void fd2_render_combatant_hp_bar_proportional(uint32 d, uint32 s, uint32 ci, uint32 st) { }
int fd2_animate_combat_hit_with_hp_drain(uint32 a, uint32 d, uint32 st) { return 0; }
void fd2_render_combat_combatant_panels(uint32 st, uint32 a, uint32 d) { }
/* fd2_play_full_combat_cinematic: now emitted for real in src/anim/anicine.c.
 * Its callee fd2_execute_combat_hit_cinematic is also emitted for real now
 * (src/anim/anicine.c); the former spy here was removed (it would duplicate
 * the real symbol at link time). The anicine.c caller tests drive the real
 * caller + real callee and observe the dispatch order / forwarded name-banner
 * through the recording fd2_animate_bg_zoom_transition_in/out stubs below
 * (the callee forwards char_idx + name_banner into them on its charge-in
 * path), and the SFX-bank handle through the fd2_play_sfx_with_handle log. */

/* Recording stubs for the FIGANI cinematic background zoom transitions (real
 * bodies @ 0x29C90 / 0x29DED not yet emitted; display-only). The real
 * fd2_execute_combat_hit_cinematic forwards the focus char_idx (and, for the
 * _out variant, the name-banner sprite) into these on its charge-in path, so
 * recording their args is the host-observable seam for the caller tests'
 * dispatch-order + banner-forcing assertions. */
int    g_zoom_in_calls = 0;
int    g_zoom_out_calls = 0;
uint32 g_zoom_in_char[8] = {0};
uint32 g_zoom_out_char[8] = {0};
int    g_zoom_out_banner_first[8] = {0};
void fd2_animate_bg_zoom_transition_in(uint32 char_idx, uint32 figani,
    uint32 framebuffer, uint32 workspace, uint32 bg_buf)
{
    if (g_zoom_in_calls < 8) {
        g_zoom_in_char[g_zoom_in_calls] = char_idx;
    }
    g_zoom_in_calls++;
    (void)figani; (void)framebuffer; (void)workspace; (void)bg_buf;
}
void fd2_animate_bg_zoom_transition_out(uint32 char_idx, uint32 figani,
    uint32 name_banner, uint32 framebuffer, uint32 workspace, uint32 bg_buf)
{
    if (g_zoom_out_calls < 8) {
        g_zoom_out_char[g_zoom_out_calls] = char_idx;
        g_zoom_out_banner_first[g_zoom_out_calls] =
            name_banner ? (int)*(uint8 *)name_banner : -1;
    }
    g_zoom_out_calls++;
    (void)figani; (void)framebuffer; (void)workspace; (void)bg_buf;
}
/* data_fd2_battle_combat_hit_shake_y_offset_table: now homed in
 * src/table/btltab2.c (real .object2 const @ 0x52577, companion of the
 * horizontal table @ 0x5255F also there); its leftover fake def here was
 * removed. */
void fd2_process_xp_and_level_up_for_char(uint32 ci) { }
/* fd2_execute_ai_item_use: now in btl_ai.c */
/* fd2_play_figani_char_intro_animation: now emitted for real in
 * src/anim/anicine.c; its former noop stub here was removed. The
 * anicine.c FIGANI-intro test drives the real function against staged real
 * FIGANI.DAT and asserts the per-pose SFX-dispatch sequence; the two
 * not-yet-emitted callees it reaches are stubbed just below. */
/* g_figani_sfx_bank_nonnull: when set, return a real malloc'd handle (the
 * caller's cleanup free()s it, so it must be a genuine heap pointer, never a
 * fake sentinel); when 0, return 0 (the no-bank path). */
int    g_figani_sfx_bank_nonnull = 0;
int    g_load_figani_sfx_bank_calls = 0;
uint32 g_load_figani_sfx_bank_last_arg = 0;
uint32 g_load_figani_sfx_bank_last_ret = 0;
uint32 fd2_load_figani_sfx_bank(uint32 figani_data)
{
    uint32 h;

    g_load_figani_sfx_bank_calls++;
    g_load_figani_sfx_bank_last_arg = figani_data;
    h = g_figani_sfx_bank_nonnull ? (uint32)malloc(16) : 0u;
    g_load_figani_sfx_bank_last_ret = h;
    return h;
}
/* fd2_play_char_intro_zoom_anim is now a real emitted function
 * (src/anim/anicine.c); its former spy stub here is retired. Sibling tests
 * (figani intro / combat cinematic) now exercise it for real. */
/* fd2_apply_use_effect_dispatch: already in spellwk.c */
/* fd2_add_item_to_inventory is now emitted for real in src/ui_menu/status.c
 * (and covered there by the test_add_item_* cases). The battle-drop suite that
 * once used the g_add_item_* spy now asserts on the real inventory state. */
/* fd2_inventory_selection_modal_dispatch and fd2_inventory_grid_input_step are
 * both now emitted for real in src/ui_menu/status.c. The grid-input step is
 * covered directly by the test_grid_input_* cases in tests/ui_menu/status.c,
 * which inject scancodes through the BIOS keyboard buffer and exercise the real
 * fd2_wait_for_input_dialog_with_blink. The modal dispatcher's input-loop tests
 * are deferred to Phase 9 integration: its fd2_open_status_screen_with_slide_in
 * clears the keyboard buffer before the loop, so the real wait can only be
 * released by async keyboard input (see tests/ui_menu/status.c). Both functions'
 * previous recording / sequence fakes were removed. */
/* fd2_play_sfx_sample_from_bank is now a real emitted function
 * (src/audio/audio.c); its former counting stub here was removed (mirrors the
 * fd2_play_sfx_with_handle relocation above). It is structurally identical to
 * fd2_play_sfx_with_handle except it drives sample slot 1
 * (data_fd2_audio_sfx_sample_handle_1) instead of slot 0. Both real players run
 * their gates then unconditionally call AIL_stop_sample(<their handle>) once per
 * invocation, so the AIL_stop_sample spy discriminates the two by the handle
 * value it receives: handle_1 -> g_play_sfx_sample_from_bank_calls, otherwise
 * (handle_0) -> g_play_sfx_with_handle_calls / g_dlg_blink_calls. Callers that
 * count both (e.g. summon variant b/d/e ticks) leave the handle globals at their
 * distinct testglob defaults, so the seam routes each caller's calls to the
 * right counter. (Counter defined near the top of this file, ahead of the AIL
 * spy that increments it.) */
void fd2_paint_char_sprite_at_world_with_mode(uint32 w, uint32 s, uint32 c, uint32 m, uint32 co) { }
/* g_pathfind_* recorders -- retained (unfilled) so the caller suites still
 * compile/link. The spy that used to fill them is now emitted for real in
 * src/util/pathfnd.c (coordinated landing, open_issues #33); those suites'
 * assertions are rewritten to drive the real pathfind in Phase 3. */
int g_pathfind_return = 0;
int g_pathfind_walk_return = 0;
int g_pathfind_write_dst = 0;
int g_pathfind_dst_x = 0;
int g_pathfind_dst_y = 0;
int g_pathfind_seq_enable = 0;
int g_pathfind_seq[4] = { 0, 0, 0, 0 };
int g_pathfind_seq_idx = 0;
int g_pathfind_seq_steps = 0;
uint8 g_pathfind_step_bytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int g_pathfind_md0_dst_x = -1;
int g_pathfind_md0_dst_y = -1;
/* fd2_pathfind_to_destination @ 0x4E1A6: real in src/util/pathfnd.c; spy removed. */
/* fd2_obfuscate_battle_tile_map: now in save/save.c */

/* --- battle-AI tile-map reachability snapshot (retained for Phase 3) ---
 * fd2_init_movement_range_floodfill is now emitted for real in
 * src/util/pathfnd.c (coordinated landing, open_issues #33); its former repaint
 * stub is removed below. bf_capture_tilemap() and the snapshot globals are kept
 * because the battle-AI suites (btl_ais1/btl_aitg) still reference them; with
 * the real floodfill now recomputing the +7 reachability layer, those suites'
 * reachability assertions are rewritten to drive the real floodfill in Phase 3. */
uint8  g_bf_tilemap_snapshot[4 + 20 * 15 * 4];
uint32 g_bf_tilemap_snapshot_bytes = 0;
uint32 g_bf_tilemap_snapshot_ptr = 0;   /* map the snapshot was taken from */

void bf_capture_tilemap(void)
{
    uint32 n;

    n = sizeof(g_bf_tilemap_snapshot);
    if (data_fd2_battle_tile_map_ptr != 0) {
        memcpy(g_bf_tilemap_snapshot, (void *)data_fd2_battle_tile_map_ptr, n);
        g_bf_tilemap_snapshot_bytes = n;
        g_bf_tilemap_snapshot_ptr = data_fd2_battle_tile_map_ptr;
    } else {
        g_bf_tilemap_snapshot_bytes = 0;
        g_bf_tilemap_snapshot_ptr = 0;
    }
}

/* fd2_init_movement_range_floodfill @ 0x4E040: real in src/util/pathfnd.c; spy removed. */

/* fd2_flood_fill_neighbor_step @ 0x4E16E: now emitted for real in
 * src/util/pathfnd.c (coordinated landing per open_issues #33). Its former
 * faithful test stub and the g_ffns_* call recorders previously here are
 * removed; the floodfill caller tests in tests/util/pathfnd.c now drive the
 * real helper and assert the resulting marker grid directly. */

/* fd2_pathfind_check_destination_save_path @ 0x4E401: now emitted for real in
 * src/util/pathfnd.c (its own routing entry). Its former faithful test stub here
 * was removed; the neighbour-step tests in tests/util/pathfnd.c drive the real
 * helper through the real step, and dedicated direct tests assert its
 * destination-snapshot path output. (Its sibling fd2_pathfind_record_destination_xy
 * @ 0x4E3B3 is likewise real in src/util/pathfnd.c.) */
/* fd2_compute_aoe_targets: now in btl_ai.c */
/* fd2_pan_cursor_to_char: already in cursor.c */

/* --- menu.c dispatch-target stubs + tracking --- */
/* fd2_field_command_menu_loop is now emitted for real in ui_menu/menu.c.
 * Its menu-subsystem callees are stubbed below so its tests can drive each
 * dispatch branch by setting the input/cursor/dialog-result seams. */

/* fd2_settings_menu_input_step is now a real emitted function
 * (src/ui_menu/menucfg.c); its former stub and the g_settings_* seam variables
 * were removed. Tests that drive a menu loop (fd2_game_options_menu_loop,
 * fd2_field_command_menu_loop) now stage real scancodes into the BIOS keyboard
 * ring via tests/include/menufix.h, so the real input-step path (which reads
 * the key through the real fd2_wait_input_with_dialog_repaint) runs end to end.
 * fd2_open_settings_dialog_with_slide / fd2_close_settings_dialog_with_slide
 * are likewise real; tests observe g_blitsetup_calls (16 corner blits each) to
 * confirm the dialog opened/closed. */
/* fd2_field_menu_status_save_load_quit_dispatch is now emitted for real in
 * src/ui_menu/menufld.c; its former one-shot stub and the
 * g_save_load_quit_dispatch_* seam variables were removed. The
 * fd2_field_command_menu_loop cursor-0 path now drives the real dispatch end to
 * end: a staged BIOS-keyboard Esc cancels its settings sub-menu (the real
 * input-step path), so it returns 0 and the loop propagates that verbatim
 * (the EAX-passthrough). FD2.SAV is staged into the test cwd by build_test.py,
 * so the dispatch's real fopen("FD2.SAV","rb") probe reads it. */
/* fd2_open_party_status_overview_screen (the cursor-0 Status arm of the
 * save/load/quit dispatch) is now emitted for real in src/ui_menu/status.c
 * (with a host smoke test in tests/ui_menu/status.c); its former no-op
 * recording stub here was removed. The menufld.c dispatch tests cover only
 * the read-only Esc-cancel and menu-state gating in the setup phase and never
 * reach the cursor-0 Status path, so the real link-in is inert for them. */
/* fd2_text_dialog_typewriter_loop is now emitted for real in src/dialog/dialog.c
 * (driven by the test_typewriter_* cases in tests/dialog/dialog.c, which preload
 * the BIOS keyboard buffer so its INT 16h dispatch returns at once). Its former
 * recording stub (g_typewriter_loop_return / _calls) was removed. Caller paths
 * that reach it through the real blocking busy-wait (menufld.c's non-gate paths)
 * are deferred to Phase 9 integration, where real keyboard input releases the
 * loop — the same deferral the file already applies to its
 * fd2_wait_for_input_dialog_with_blink gold/item paths. */
/* fd2_animate_dialog_page_advance_collapse is now emitted for real in
 * src/dialog/dialog.c; its former recording stub here was removed. Caller
 * tests (menufld.c) drive the real function with the battle-tile-map gate ON
 * (so its scene-prime runs one fd2_composite_battle_tile_map) and observe its
 * effect through the recording g_composite_call_count proxy. */
/* fd2_game_options_menu_loop is now emitted for real in ui_menu/menucfg.c. */
/* fd2_player_action_menu_loop is now emitted for real in ui_menu/menu.c; its
 * former one-shot stub and the g_player_action_menu_loop_* seam variables were
 * removed. The fd2_game_main_loop player-action path now drives the real
 * function (behavioral coverage deferred to Phase 9 integration). */
/* fd2_player_inline_action_menu_dispatch is now emitted for real in
 * ui_menu/menu.c; its former one-shot stub and the g_inline_dispatch_* seam
 * variables were removed. Its spell/item submenu callees are stubbed above
 * (g_inline_spell_menu_* / g_inline_item_menu_*); its tile-event callee is now
 * the real fd2_handle_tile_event_interaction (src/ui_menu/menufld.c). */
/* fd2_open_char_status_screen: now emitted in src/ui_menu/status.c and linked
 * for real (was a recording stub here). It is pure VGA/sfx orchestration and is
 * never reached by a host test — fd2_game_main_loop (its sole in-tree caller)
 * is not exercised — so its behavioral coverage is deferred to Phase 9. */
/* fd2_open_tactical_overview_zoom: now emitted in src/ui_menu/menufld.c and
 * linked for real (was a recording stub here). The display-loop poll has no
 * harness-releasable exit, so its behavioral coverage is deferred to Phase 9
 * (see the test file header); its not-yet-emitted callee
 * fd2_blit_scaled_tile_map_view is the recording stub above. */
/* Recording stub for fd2_restore_screen_block_from_buffer (the screen-block
 * restore blitter, not yet emitted). fd2_cleanup_dialog_sprite_buffer must
 * forward its (saved_block, dst, stride) args to this in order, then free
 * saved_block. The stub captures the args so the cleanup test can assert the
 * forwarding without touching real VGA memory. */
int    g_restore_block_calls = 0;
uint32 g_restore_block_last_buf = 0;
uint32 g_restore_block_last_dst = 0;
uint32 g_restore_block_last_stride = 0;
void fd2_restore_screen_block_from_buffer(uint32 saved_block, uint32 dst, uint32 stride) {
    g_restore_block_calls++;
    g_restore_block_last_buf = saved_block;
    g_restore_block_last_dst = dst;
    g_restore_block_last_stride = stride;
}

/* ---- fd2_display_dialog_scene (dialog VM) support ----
 * Globals it reads/writes (not yet defined elsewhere) and display-side-effect
 * callees stubbed to noop, with a recording stub for the glyph blitter and a
 * call counter for the blink-animation step so the VM's render-position
 * arithmetic and opcode dispatch can be asserted without touching VGA / sfx.
 * fd2_check_keyboard_buffer_nonempty and fd2_wait_for_input_dialog_with_blink
 * are the REAL linked functions; the dialog-VM tests use only TEXT / -3 / -6 /
 * -1 opcodes so the busy-wait (page-break) and portrait/file-load paths are
 * never reached. With the BIOS keyboard buffer left empty (head==tail), the
 * real keyboard poll returns 0 so blink_flag stays set and the blink stub runs. */

int    g_dlg_glyph_calls = 0;
uint32 g_dlg_glyph_last_idx = 0;
uint32 g_dlg_glyph_last_pos = 0;
uint32 g_dlg_glyph_last_p5 = 0;   /* glyph colour/border param (p5) */

void fd2_blit_glyph_2bpp_with_outline(uint32 font_sheet, uint32 glyph_idx,
                                      uint32 render_pos, uint32 render_pitch,
                                      uint32 p5, uint32 p6, uint16 p7) {
    (void)font_sheet; (void)render_pitch; (void)p6; (void)p7;
    g_dlg_glyph_calls++;
    g_dlg_glyph_last_idx = glyph_idx;
    g_dlg_glyph_last_pos = render_pos;
    g_dlg_glyph_last_p5 = p5;
    g_dlg_glyph_last_p5  = p5;
}
/* fd2_play_dialog_open_animation: now emitted in src/dialog/dialog.c and
 * linked for real; its 5-stage frame assembly is driven by the
 * test_open_anim_* cases in tests/dialog/dialog.c.
 * fd2_cinematic_scroll_text_up_for_special_scenes: now emitted in
 * src/dialog/dialog.c and linked for real (was a no-op stub here). */
int    g_dlg_blit_normal_calls = 0;
int    g_dlg_blit_mirrored_calls = 0;
uint32 g_dlg_blit_last_dst = 0;
uint32 g_dlg_blit_last_sprite = 0;
uint32 g_dlg_blit_last_stride = 0;
/* per-call log (chapter-intro panel tests verify both the left and right
 * panel blits within a single render call) */
uint32 g_dlg_blit_dst_log[16];
uint32 g_dlg_blit_sprite_log[16];
/* Opt-in input seam for callers that drain the BIOS keyboard buffer
 * (fd2_clear_keyboard_buffer) and THEN block on the real
 * fd2_wait_for_input_dialog_with_blink, but draw a portrait via the mirrored
 * blit in between (e.g. the fd2_load_chapter_portrait -> dialog -> wait path of
 * the tile-pickup chapter event handler). When g_dlg_blit_mirror_inject_after
 * != 0, the spy flips the BIOS keyboard buffer nonempty with the injected
 * scancode on its g_dlg_blit_mirror_inject_after-th call, so the next
 * fd2_check_keyboard_buffer_nonempty() inside the busy-wait returns nonzero and
 * the real INT 16h read returns at once. Default 0 keeps the historical
 * record-only behaviour for every other test. */
int    g_dlg_blit_mirror_inject_after = 0;     /* 0 = disabled */
int    g_dlg_blit_mirror_inject_scancode = 0;
void fd2_dialog_sprite_blit_normal(uint32 dst, uint32 sprite, uint32 stride) {
    if (g_dlg_blit_normal_calls < 16) {
        g_dlg_blit_dst_log[g_dlg_blit_normal_calls] = dst;
        g_dlg_blit_sprite_log[g_dlg_blit_normal_calls] = sprite;
    }
    g_dlg_blit_normal_calls++;
    g_dlg_blit_last_dst = dst;
    g_dlg_blit_last_sprite = sprite;
    g_dlg_blit_last_stride = stride;
}
void fd2_dialog_sprite_blit_mirrored(uint32 dst, uint32 sprite, uint32 stride) {
    g_dlg_blit_mirrored_calls++;
    g_dlg_blit_last_dst = dst;
    g_dlg_blit_last_sprite = sprite;
    g_dlg_blit_last_stride = stride;
    if (g_dlg_blit_mirror_inject_after != 0
        && g_dlg_blit_mirrored_calls == g_dlg_blit_mirror_inject_after) {
        *(volatile uint16 *)0x41AuL = 0x1E;                 /* head        */
        *(volatile uint16 *)0x41CuL = 0x20;                 /* tail=head+2 */
        *(volatile uint16 *)0x41EuL =
            (uint16)((g_dlg_blit_mirror_inject_scancode << 8) & 0xFF00);
    }
}
/* Promote/revive candidate-picker callees (not yet emitted in src) — recording
 * no-op spies driving tests/ui_menu/promote.c fd2_promote_members_select_loop.
 * The grid renderer records its arg snapshot; the two scroll animators just
 * count (they only fire on Up/Down navigation, deferred to Phase 9). */
int    g_promote_grid_calls = 0;
uint32 g_promote_grid_last_count = 0;
uint32 g_promote_grid_last_dst = 0;
uint32 g_promote_grid_last_cursor = 0;
int    g_promote_grid_last_list = 0;
int    g_promote_scroll_down_calls = 0;
int    g_promote_scroll_up_calls = 0;
void fd2_render_promote_members_grid(uint32 candidate_count, uint32 dst_buffer,
                                     uint32 cursor_idx, int candidate_idx_list) {
    g_promote_grid_calls++;
    g_promote_grid_last_count = candidate_count;
    g_promote_grid_last_dst = dst_buffer;
    g_promote_grid_last_cursor = cursor_idx;
    g_promote_grid_last_list = candidate_idx_list;
}
/* CLASS-PROMOTION candidate-grid renderer (5-arg, distinct from the revive
 * members grid above) — recording no-op spy driving the singular select loop
 * fd2_promote_member_select_loop. Captures the extra price/aux (target_classes)
 * list arg too. Not yet emitted in src. */
int    g_promote_cand_grid_calls = 0;
uint32 g_promote_cand_grid_last_count = 0;
uint32 g_promote_cand_grid_last_dst = 0;
uint32 g_promote_cand_grid_last_cursor = 0;
int    g_promote_cand_grid_last_list = 0;
int    g_promote_cand_grid_last_aux = 0;
void fd2_render_promote_candidates_grid(uint32 char_count, uint32 dst_surface,
                                        uint32 cursor_idx, int char_list_ptr,
                                        int price_aux_list_ptr) {
    g_promote_cand_grid_calls++;
    g_promote_cand_grid_last_count = char_count;
    g_promote_cand_grid_last_dst = dst_surface;
    g_promote_cand_grid_last_cursor = cursor_idx;
    g_promote_cand_grid_last_list = char_list_ptr;
    g_promote_cand_grid_last_aux = price_aux_list_ptr;
}
void fd2_animate_scroll_down_in_shop_dialog(void) { g_promote_scroll_down_calls++; g_scroll_down_in_shop_calls++; }
void fd2_animate_scroll_up_in_shop_dialog(void)   { g_promote_scroll_up_calls++; g_scroll_up_in_shop_calls++; }
/* Shop money/transaction feedback animations (not yet emitted in src) —
 * recording no-op spies. The church-revive + buy/sell menus call these on the
 * commit path; that path is deferred to Phase 9 integration, so these only
 * count + capture the last decrement amount for future use. */
int    g_money_decrement_calls = 0;
uint32 g_money_decrement_last_amount = 0;
int    g_shop_txn_feedback_calls = 0;
void fd2_animate_money_decrement(uint32 amount) {
    g_money_decrement_calls++;
    g_money_decrement_last_amount = amount;
}
void fd2_animate_shop_transaction_feedback(void) { g_shop_txn_feedback_calls++; }
/* fd2_render_full_char_stat_panel @ 0x17fc0: now emitted for real in
 * src/gfx/rndstat.c and driven by tests/gfx/rndstat.c (the numeric/bar
 * render primitives it dispatches to are the recording spies defined
 * above; the icon/team blits reach the real fd2_blit_sheet_sprite_at_offset
 * -> g_blitraw log; the three text-label fd2_display_dialog_scene calls run
 * for real against a minimal immediate-END text program). */
/* fd2_close_dialog_panels_then_slide_in_at: now emitted in
 * src/dialog/dialog.c and linked for real; its teardown + slide-out
 * interpolation is driven by the test_close_* cases in
 * tests/dialog/dialog.c (observed via the restore/save/blit-setup stubs). */
/* fd2_show_portrait_dialog_with_input @ 0x2C39B: now emitted in
 * src/dialog/dialog.c and linked for real (satisfies the link from its sole
 * caller fd2_play_game_ending_cinematic). It is a straight-line orchestration
 * wrapper (cyclomatic complexity 1: no branches/computation/RNG) that runs
 * clear_kbd -> load_chapter_portrait(portrait_id) -> clear_kbd ->
 * display_dialog_scene(data_fd2_current_chapter_text, text_idx, ...) -> paint(0) ->
 * wait_for_input_dialog_with_blink(0) -> close_intro_dialog -> clear_kbd.
 * No direct test drives it: it clears the BIOS keyboard buffer immediately
 * before the unconditional blocking fd2_wait_for_input_dialog_with_blink(0),
 * whose loop can only be released by async keyboard input the silent harness
 * cannot deliver (the same self-cleared-buffer + blocking-wait deferral this
 * file applies to the status-screen/settings modal dispatchers above). Its
 * argument forwarding and per-callee behavior are covered by the callees' own
 * suites (rsrc/rsrc.c test_lcp_* for load_chapter_portrait; dialog/dialog.c for
 * the dialog VM; input/input.c for the blink-wait), so end-to-end behavioral
 * coverage is deferred to Phase 9 integration. */

/* ---- fd2_open_char_status_screen / fd2_open_status_screen_with_slide_in
 * (status.c) support ----
 * The status-screen modals are pure VGA/sfx orchestration: every callee below
 * only blits/animates, and the functions themselves memmove to/from physical
 * VRAM (0xA0000). They are therefore deferred to Phase 9 integration and are
 * not driven by a host unit test; these noop stubs only satisfy the linker for
 * the not-yet-emitted display callees they reference. */
/* fd2_render_status_screen_static_layout: now emitted for real in
 * src/gfx/rndstat.c; deferred to Phase 9 integration for its dedicated test
 * (see src/emit_issues.json @00017eef). */
/* fd2_render_inventory_item_grid: now emitted for real in src/gfx/rndstat.c
 * (with host unit tests in tests/gfx/rndstat.c); stub removed. */
/* fd2_paint_status_panel_layer_left / _right: both now emitted for real in
 * src/gfx/rndstat.c (with host unit tests); stubs removed. */
/* fd2_play_status_screen_outro_step: now emitted for real in src/anim/aniwalk.c
 * (with host unit tests in tests/anim/aniwalk2.c driving the real panel
 * painters over in-memory buffers); stub removed. */
/* fd2_draw_spell_selection_list: now emitted for real in src/spell/spellsel.c
 * (with host unit tests in tests/spell/spellsel.c driving the real MP-icon /
 * decimal / name-label renderers over in-memory fixtures); stub removed. */

/* fd2_render_party_status_overview_content: now emitted for real in
 * src/gfx/rndstat.c (with host unit tests in tests/gfx/rndstat.c driving the
 * real sprite-sheet / decimal / dialog renderers over in-memory fixtures);
 * recording stub removed. The army-overview orchestrator test in
 * tests/ui_menu/status.c now stands up a minimal sprite-sheet + immediate-END
 * text fixture so the real content renderer runs safely. */

/* fd2_count_active_chars_for_team_filter: now emitted for real in
 * src/battle/btl_turn.c (it scans g_test_rc_array via
 * data_fd2_battle_runtime_char_array_ptr and the fd2_check_char_is_dead stub);
 * recording fake removed. The content-renderer tests in tests/gfx/rndstat.c
 * now seed g_test_rc_array with a known per-team alive distribution so the
 * real counter feeds the per-team decimal renders. */

/* fd2_check_party_has_char_id is now emitted for real in src/util/misc.c
 * (it scans the menu/template roster via
 * data_fd2_shared_menu_party_roster_buffer_ptr /
 * data_fd2_shared_menu_party_member_count). The recording fake and its
 * g_has_char_* globals were removed; the content-renderer tests in
 * tests/gfx/rndstat.c now seed that roster so the real query drives the
 * Mitti subtitle branch. */

/* ---- fd2_chapter_21_end (field/chend2.c) not-yet-emitted callees ----
 * fd2_chapter_21_end calls three functions that have not been emitted yet:
 *
 *   fd2_find_inventory_slot_with_item (0x31860 -> ui_menu/status.c) — the
 *     handler scans chars 0..15 for each collectible item id 0xD1..0xD6 and
 *     counts the holders to decide the 6-item hidden-stage unlock. This is a
 *     programmable double for the chend2 tests: it maps item 0xD1->char 0 ..
 *     0xD6->char 5 (returning slot 0 for a hold), with g_ce_find_have_d6
 *     gating whether the 0xD6 holder exists so a test can land the count on
 *     exactly 6 or 5. g_ce_find_calls records the call total. It also gates
 *     item 100 (天空之鑰) via g_ce_find_have_item100 (char 0 holds it) so the
 *     now-real fd2_any_char_has_item's held/not-held arms can be driven from
 *     the chend2 ch23 tests. (The real function's class-promotion callers are
 *     likewise unemitted, so nothing else depends on its true behavior yet;
 *     remove this double when the real function is emitted.)
 *
 *   fd2_setup_chars_and_camera_for_intro (0x233C6 -> field/chtrans.c) — places
 *     the cast, re-aims the camera, then fades the screen. The cast placement +
 *     fade are display (Phase-9-deferred no-op), but the camera re-aim is
 *     load-bearing state the later pans depend on, so the double still does that.
 *
 *   fd2_play_chapter_intro_sprite_slideshow (0x24336 -> anim/aniend.c) — the
 *     hidden-stage cinematic; it memmoves 64000 bytes to/from the absolute VGA
 *     framebuffer 0xA0000 and loads FDOTHER.DAT, so a no-op here. Deferred to
 *     Phase 9 integration; stubbing it lets the all-collected branch run
 *     on-host so the real item-100 award is observable. */
int g_ce_find_have_d6 = 0;
int g_ce_find_have_item100 = 0;
int g_ce_find_calls = 0;
int fd2_find_inventory_slot_with_item(int char_idx, int item_id) {
    g_ce_find_calls++;
    if (item_id >= 0xD1 && item_id <= 0xD6 && char_idx == (item_id - 0xD1)) {
        if (item_id == 0xD6 && !g_ce_find_have_d6) {
            return -1;
        }
        return 0;
    }
    if (item_id == 100 && char_idx == 0 && g_ce_find_have_item100) {
        return 0;
    }
    return -1;
}

void fd2_play_chapter_intro_sprite_slideshow(void) { }

/* ---- fd2_chapter_22_end (field/chend2.c) not-yet-emitted callee ----
 * fd2_cast_screen_wide_spell_with_fade (0x24618 -> anim, pending) — the
 * FD2-unique white-fade ending's screen-wide radial spell visual. It
 * malloc's a 150KB backdrop snapshot, runs a 9-frame growing-shockwave
 * blit loop and a 0x40-step palette flash, and blocks for ~95 ticks of
 * BIOS-tick delay; it also needs the sprite atlas (tile_anim_table_base)
 * and the status-effect SFX bank staged. Pure display, so a no-op here;
 * deferred to Phase 9 integration. Stubbing it lets fd2_chapter_22_end run
 * end-to-end on-host so the tail-jump save-template + chapter-advance is
 * observable. Remove this double when the real function is emitted. */

/* ---- fd2_chapter_23_end (field/chend2.c) not-yet-emitted callee ----
 * Both Phase-1 story predicates are now REAL: fd2_any_char_has_item (天空之鑰)
 * in src/util/misc.c, driven through the find double above via
 * g_ce_find_have_item100; and fd2_find_template_char_by_id (蜜蒂-roster) in
 * src/util/misc.c, driven directly through the template roster by the chend2
 * ch23 tests. Only the display-only screen shake remains doubled.
 *
 *   fd2_animate_screen_shake (0x24B4D -> graphics, pending) — a 1-row vertical
 *     blit-jitter loop over the snapshot buffer; pure display, no-op here.
 *     Deferred to Phase 9 integration.
 *
 * Remove this double when the real function is emitted. */
void fd2_animate_screen_shake(uint32 frame_count) {
    (void)frame_count;
}

/* ---- fd2_chapter_27_end (field/chend2.c) not-yet-emitted callee ----
 * fd2_play_game_ending_cinematic (0x2BCE5 -> anim/aniend.c, pending) — the
 * full game-over cinematic played on chapter 27's BAD ending (no 天空之鑰):
 * it loads FDOTHER.DAT assets, runs combat/ANI cinematics, portrait dialogs,
 * 64000-byte VGA framebuffer blits and palette fades, and ends by calling the
 * final chapter-30 ending. It is reached only on the bad path, immediately
 * before fd2_chapter_27_end's intentional infinite-loop hard-lock, so it is
 * never invoked by the on-host GOOD-path test; a no-op double here resolves
 * the link. Deferred to Phase 9 integration. Remove when the real function is
 * emitted. */
void fd2_play_game_ending_cinematic(void) {
}

/* ---- fd2_chapter_29_end (field/chend2.c) not-yet-emitted callees ----
 * fd2_kill_runtime_chars_from_index_to_end (0x35BBA -> field, pending) — the
 * real function wipes hp_current=0 for every runtime_char from start_char_idx
 * to party_member_count-1, then plays a death animation. Its own HP-wipe
 * behavior is owned by that function's future emit + test; here a recorder
 * double captures the start index so chapter 29's "kill from slot 0x14" call
 * is observable without faking the wipe (which would risk diverging from the
 * real emit). g_ce_kill_from_calls / g_ce_kill_from_last_idx record it.
 *
 * fd2_animate_palette_flash_pulse_white (0x35E5A -> graphics, pending) — a
 * 64-step additive over-bright white palette pulse with 4ms steps and a 400ms
 * hold; pure display, no-op here. Deferred to Phase 9 integration.
 *
 * Remove these doubles when the real functions are emitted. */
int    g_ce_kill_from_calls = 0;
uint32 g_ce_kill_from_last_idx = 0;
void fd2_kill_runtime_chars_from_index_to_end(uint32 start_char_idx) {
    g_ce_kill_from_calls++;
    g_ce_kill_from_last_idx = start_char_idx;
    if (g_kill_from_calls < 4) {
        g_kill_from_index[g_kill_from_calls] = start_char_idx;
    }
    g_kill_from_calls++;
}

/* ---- fd2_execute_special_attack_skill (src/spell/spellcin.c @ 0x276EC) callee
 * stubs. That worker is a monolithic VGA/VRAM cinematic deferred to Phase 9
 * integration (no unit test drives it), so these are plain no-op linker stubs.
 * Each is replaced when its real definition is emitted. Signatures match
 * src/include/protos.h. */
uint8 fd2_resolve_terrain_for_aoe_targets(int n_chars, uint32 target_byte_array)
    { return 0; }
/* fd2_load_figani_sfx_bank: now emitted for real in src/audio/audio.c
 * (its former no-op linker stub was removed). */
void fd2_step_figani_pose_animation(uint32 figani_data, uint32 palette_op,
    uint32 dst_buf, uint32 dst_stride) { }
/* fd2_animate_bg_zoom_transition_in: now emitted for real in
 * src/anim/anispell.c (its former no-op linker stub was removed). */
void fd2_play_char_intro_zoom_anim(uint32 caster_idx, uint32 mode_flag,
    uint32 caster_figani_a, uint32 target_figani0, uint32 workbuf2,
    uint32 workbuf1, uint32 tai_resource) { }
void fd2_play_figani_animation_loop(uint32 caster_idx, uint32 spell_id,
    uint32 caster_figani_b, uint32 target_figani0, uint32 workbuf2,
    uint32 workbuf1, uint32 bg_layer_saved, uint32 tai_resource) { }
/* fd2_play_spell_cast_sequence (src/anim/anispell.c @ 0x2A6BD) callee stubs.
 * That orchestrator is a real-file + VGA cinematic deferred to Phase 9 (no unit
 * test drives it), so these are plain no-op linker stubs. Replaced when their
 * real definitions are emitted. Signatures match src/include/protos.h. */
void fd2_animate_spell_hit_cinematic(uint32 caster_idx, uint32 caster_sprite,
    uint32 caster_figani_b, uint32 target_figani_cur, uint32 work_buf,
    uint32 backbuf, uint32 target_figani_next, uint32 spell_id) { }
/* Fake for the remaining unemitted party-wide item-query callee of the
 * chapter-init handlers (fd2_any_char_has_item -> src/util/misc.c; consumed by
 * fd2_chapter_27_init's Sky-Key bonus-page gate). Returns 1 if any char carries
 * the item, else -1; tests set the return value directly. Default -1 (item
 * absent). */
int    g_any_has_item_fake = -1;
uint32 g_any_has_item_last_arg = 0;
int    g_any_has_item_calls = 0;
int fd2_any_char_has_item(uint32 item_id) {
    g_any_has_item_calls++;
    g_any_has_item_last_arg = item_id;
    return g_any_has_item_fake;
}

/* Recording fake for a not-yet-emitted callee of
 * fd2_process_xp_and_level_up_for_char (src/battle/btl_turn.c).
 * (fd2_roll_stat_gain_and_show_message is now emitted in btl_turn.c and runs
 * for real in the level-up tests.)
 *
 * fd2_grant_spell_to_char (-> spell/spellsel.c, own turn) really writes the
 * spells-known bitmap; the fake logs (char_idx, spell_id) so the spell-learn
 * branch can be pinned without the real bitmap write. */
int    g_grant_spell_calls = 0;
uint32 g_grant_spell_last_char = 0;
uint32 g_grant_spell_last_spell = 0;
void fd2_grant_spell_to_char(uint32 char_idx, uint32 spell_id)
{
    g_grant_spell_calls++;
    g_grant_spell_last_char = char_idx;
    g_grant_spell_last_spell = spell_id;
}

/* Recording fakes for the two not-yet-emitted callees of
 * fd2_play_ending_and_record_clear (src/anim/aniend.c):
 *   fd2_display_cinematic_image_with_fade  -> anim/anicine.c (future)
 *   fd2_render_chapter_status_panel_segments -> gfx/rndstat.c (future)
 * The ending driver itself is a Phase-9 integration target (writes VGA at
 * 0xA0000, blocks on INT 16h), so these fakes only satisfy the linker; the
 * aniend unit tests exercise the isolable Phase-8 save decision directly. */
int    g_display_cinematic_calls = 0;
void fd2_display_cinematic_image_with_fade(uint32 stage1_img_idx, uint32 stage1_palette_idx,
                                           uint32 stage2_src_x, int stage2_src_row)
{
    g_display_cinematic_calls++;
    (void)stage1_img_idx; (void)stage1_palette_idx;
    (void)stage2_src_x; (void)stage2_src_row;
}
int    g_render_status_panel_calls = 0;
void fd2_render_chapter_status_panel_segments(uint32 panel_sheet, uint32 active_idx,
                                              uint32 menu_options)
{
    g_render_status_panel_calls++;
    (void)panel_sheet; (void)active_idx; (void)menu_options;
}
