#ifndef BLITPROB_H
#define BLITPROB_H
/* Shared probe-sprite support for testing callers of the real
 * fd2_tile_blit_24x24_passthrough (src/gfx/blittile.c). Include AFTER the common
 * preamble (needs types.h for uint8/uint32).
 *
 * The passthrough blitter has no recording stub: it paints pixels for real. To
 * verify a caller's resolved (src, dst, stride) the tests install "probe"
 * sprites and read the painted destination bytes back:
 *
 *   - A probe is a 24x24 RLE sprite that paints one identifying byte at
 *     tile-local (row 0, col 0) and is transparent (SKIP) everywhere else. When
 *     the caller forwards it to the real blitter the painted byte's OFFSET in the
 *     destination buffer reveals where the blit landed (= the caller's dst), and
 *     the painted byte's VALUE reveals which atlas slot the caller resolved
 *     (each slot's probe paints a slot-specific value).
 *   - bp_probe2 additionally paints the same value at (row 1, col 0). Because the
 *     passthrough row reset advances dst by stride-0x18, that second byte lands
 *     exactly `stride` past the first, so a test can confirm the forwarded row
 *     stride end-to-end (used where stride is the property under test).
 *   - bp_build_atlas1 lays out an offset table plus a one-pixel probe (value i+1)
 *     at each slot, so a caller computing src = base + table[idx] reads slot idx's
 *     probe -> painted value == idx+1. The sprite-data region starts AFTER the
 *     offset table (slot offsets come from bp_atlas_slot_off), so slot 0 never
 *     clobbers the table; a caller that overwrites one slot's probe must use
 *     bp_atlas_slot_off for the same address.
 *
 * Probe values are 1-based (slot k -> k+1) so a painted byte is always distinct
 * from the 0 background a test memsets the buffer to.
 *
 * Defined in tests/testglob.c (shared across all test translation units). */

void bp_probe1(uint8 *slot, uint8 value);
void bp_probe2(uint8 *slot, uint8 value);
uint32 bp_atlas_slot_off(uint32 table_off, int n, uint32 entry_span, int i);
void bp_build_atlas1(uint8 *base, uint32 table_off, uint32 entry_span, int n);
int  bp_count_value(const uint8 *buf, uint32 size, uint8 value, uint32 *first_off);
int  bp_count_painted(const uint8 *buf, uint32 size);

/* ---- compositor host-safety fixture (shared) -----------------------------
 * Make every blit reached by the REAL fd2_composite_battle_frame a deterministic
 * no-op, WITHOUT touching the emit C. Needed by the chapter-event tests whose
 * turn-gated / cinematic handlers pan the camera (fd2_pan_cursor_and_window drives
 * the real compositor per pan step): the compositor's per-char painter, shadow
 * overlay, cursor overlay and terrain HUD all feed a sprite stream to the real
 * fd2_tile_blit_24x24_passthrough (and siblings), and an unset / real-but-non-
 * 24x24-RLE sprite source makes the RLE row decoder spin forever (its row loop
 * only exits when x_remain hits exactly 0).
 *
 * tg_install_compositor_safe_atlases (call at the END of a test's setup, after any
 * fixture that itself sets data_fd2_portrait_sprite_cache / runtime_battle_state_ptr):
 *   - per-char painter: a heap data_fd2_portrait_sprite_cache (0x32A00 — the size the
 *     loader's final FD2.TMP fwrite reads) whose +0 offset table all points at one
 *     transparent "SKIP 24 x 24" sprite, plus a pre-seeded portrait cache id-list
 *     (id 0) + count 1 so a matching tile-event record's fd2_load_portrait_to_cache
 *     takes the cache-HIT early return (leaving this safe atlas intact) instead of
 *     re-loading real FDICON. (The chapter-event loader fixtures zero every
 *     tile-event byte but the race tag, so the matched record's char_id is 0.)
 *   - cursor overlay: all-SKIP atlas at runtime_battle_state_ptr (+6 table).
 *   - shadow / animated-tile overlay: zeroed tile-map + attr so the per-tile
 *     renderable bit (0x80) stays clear and it never reads the snapshot atlas.
 *   - terrain HUD panel: enable + play-active gates off so it early-returns.
 * The portrait cache is heap so the caller's existing teardown free() stays valid.
 * tg_restore_compositor_safe_atlases restores the non-cache globals it changed;
 * call it before the test's teardown (which frees data_fd2_portrait_sprite_cache). */
void tg_install_compositor_safe_atlases(void);
void tg_restore_compositor_safe_atlases(void);

#endif
