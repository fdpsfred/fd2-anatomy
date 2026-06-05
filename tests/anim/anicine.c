/*
 * unit tests for src/anim/anicine.c
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "testharn.h"
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"
#include "realfile.h"
#include "minipfix.h"

/* runtime-char array backing + render/SFX spies (testglob.c) */
extern runtime_char g_test_rc_array[8];
extern int    g_play_sfx_with_handle_calls;
extern int    g_sfx_id_count;
extern int    g_sfx_id_log[64];

/* fd2_load_figani_sfx_bank stub (testglob.c): returns a real malloc'd handle
 * (freed by the function's cleanup) when g_figani_sfx_bank_nonnull is set, and
 * records the figani_buf passed to it. */
extern int    g_figani_sfx_bank_nonnull;
extern int    g_load_figani_sfx_bank_calls;
extern uint32 g_load_figani_sfx_bank_last_arg;

/* fd2_play_char_intro_zoom_anim stub (testglob.c): records call + args. */
extern int    g_zoom_anim_calls;
extern uint32 g_zoom_anim_last_char;
extern uint32 g_zoom_anim_last_mode;

/* fd2_rle_blit_sprite spy full per-call log (testglob.c). The BG backdrop blit
 * fd2_rle_blit_sprite(spotlight, 0, 0x32, dst, 0x140, -1) is the unique call
 * with dst_y == 0x32; the mini-panel painter (run earlier via the real
 * fd2_flash_char_hit_sprite) contributes the preceding calls. Scanning the log
 * for y==0x32 recovers the backdrop sprite ptr, whose first payload byte pins
 * which BG.DAT index the +6 tile-attr byte selected. */
extern int    g_rle_blit_calls;
extern int    g_rle_blit_log_on;
extern int32  g_rle_blit_log_y[64];
extern int32  g_rle_blit_log_x[64];
extern uint8  g_rle_blit_log_first_byte[64];

/* ----------------------------------------------------------------
 * fd2_play_figani_char_intro_animation @ 0x28784
 *
 * Real-file drive of the FIGANI per-pose loop. The function loads the
 * real FIGANI.DAT[portrait_id*3 + 1] animation stream (staged into the
 * test cwd by build_test.py) and walks its pose-offset table. The
 * highest-risk control flow is the per-pose conditional SFX dispatch:
 *
 *   for i in 0..figani_buf[0]:
 *     entry = figani_buf + *(int*)(figani_buf + 8 + i*4)
 *     if entry[+5] != 0: fd2_play_sfx_with_handle(bank, entry[+5], 1)
 *     ... blits ...     fd2_wait_n_bios_ticks(entry[+6])
 *
 * The test asserts the recorded fired-SFX id sequence against an
 * INDEPENDENT parse of the same real FIGANI.DAT bytes (no hardcoded
 * magic): each fire equals the non-zero entry[+5] in pose order, and the
 * cleanup stop-all appends one final -1. The forwarded SFX-bank handle
 * (from the fd2_load_figani_sfx_bank stub sentinel) and the zoom-anim
 * hand-off args are checked too.
 *
 * The display side-effects (real flash/blit/composite into 0xA0000) run
 * host-safely under DOS/4GW (0xA0000 is real VGA RAM) over in-memory
 * battle-scene buffers; only the loop's observable SFX/zoom plumbing is
 * asserted.
 * ---------------------------------------------------------------- */

#define CINE_MAP_W     0x20
#define CINE_WIN_OX    0x10u
#define CINE_WIN_OY    0x10u
/* A valid BG.DAT entry index (BG.DAT has 50 entries). The function selects the
 * BG index from tile_attr[+6] (= attr-flags byte 2 of sprite_idx 0), which the
 * fixture drives via g_cine_attr_buf[2]. */
#define CINE_BG_INDEX  5

static uint8 g_cine_tile_map[CINE_MAP_W * CINE_MAP_W * 4];
static uint8 g_cine_attr_buf[256 * 4];
static uint8 g_cine_tile_event[4];
static uint8 g_cine_portrait_cache[256 * 4];

/* Build the safe full fixture and seed the runtime char so the function loads
 * FIGANI.DAT[portrait_id*3 + 1]. The tile map is zeroed -> sprite_idx 0, so the
 * read tile-attr record is attr_buf[0..3]; byte +6 of the 8-byte output (the
 * BG.DAT index) is attr_buf[2], which is set to CINE_BG_INDEX. */
static void setup_figani_intro(uint8 portrait_id, uint8 team)
{
    int i;
    uint32 *table;

    /* runtime char 0 = the intro target */
    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = CINE_WIN_OX + 1;
    g_test_rc_array[0].pos_y = CINE_WIN_OY + 1;
    g_test_rc_array[0].portrait_id = portrait_id;
    g_test_rc_array[0].team = team;

    /* tile map + attribute buffers (read by fd2_read_tile_attribute_at_pos):
     * tile map zeroed -> sprite_idx 0, terrain 0; attr_buf[2] drives the BG
     * index that lands at tile_attr[+6]. */
    memset(g_cine_tile_map, 0, sizeof(g_cine_tile_map));
    memset(g_cine_attr_buf, 0, sizeof(g_cine_attr_buf));
    memset(g_cine_tile_event, 0, sizeof(g_cine_tile_event));
    g_cine_attr_buf[2] = CINE_BG_INDEX;
    data_fd2_battle_tile_map_ptr = (uint32)g_cine_tile_map;
    data_fd2_battle_map_width_tiles = CINE_MAP_W;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_cine_attr_buf;
    data_fd2_tile_event_data_table_ptr = (uint32)g_cine_tile_event;

    /* The function free()s data_fd2_large_game_state_buffer_ptr at entry and
     * reallocates a fresh malloc(0x25680) in cleanup (which the closing
     * fd2_composite_battle_frame(1) then composites into at +0x8088). Set NULL
     * so the entry free() is a no-op (never free a static buffer). */
    data_fd2_large_game_state_buffer_ptr = 0;

    /* portrait cache (mini-panel / compositor per-char paint) */
    table = (uint32 *)g_cine_portrait_cache;
    for (i = 0; i < 256; i++) {
        table[i] = (uint32)i * 0x100u;
    }
    portrait_sprite_cache = (uint32)g_cine_portrait_cache;

    /* view window + finalizer gating (HUD off, play inactive, anim phase 0,
     * palette cycle throttled to no-op) */
    data_fd2_battle_view_window_origin_x = CINE_WIN_OX;
    data_fd2_battle_view_window_origin_y = CINE_WIN_OY;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;
    data_fd2_battle_anim_phase = 0;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;
    data_fd2_chapter_current_chapter_id = 1;

    /* entry frees of these globals must be no-ops (free(NULL)) */
    battle_scene_snapshot = 0;

    /* mini-panel painter fixture (sprite sheet + immediate-END text table) */
    minip_setup_env();

    /* SFX-bank stub returns a real malloc'd handle (freed by the cleanup) */
    g_figani_sfx_bank_nonnull = 1;
    g_load_figani_sfx_bank_calls = 0;
    g_load_figani_sfx_bank_last_arg = 0;

    g_zoom_anim_calls = 0;
    g_zoom_anim_last_char = 0;
    g_zoom_anim_last_mode = 0;

    g_play_sfx_with_handle_calls = 0;
    g_sfx_id_count = 0;
    memset(g_sfx_id_log, 0, sizeof(g_sfx_id_log));
}

/* Independent parse: derive the ordered list of non-zero pose SFX ids for the
 * real FIGANI.DAT[portrait_id*3 + 1] entry (same bytes the loader reads). The
 * loop bound is the LOW BYTE of the u16 at +0 (the function reads
 * *(uint8 *)figani_buf), pose i metadata is at figani_buf + u32[+8 + i*4],
 * and the SFX hook byte is metadata[+5]. Returns the fire count; fills out[].
 */
static int figani_expected_sfx(uint8 portrait_id, int *out, int out_cap)
{
    uint8 *buf;
    long   size;
    int    n;
    int    pose_count;
    int    i;

    size = realdat_read_resource("FIGANI.DAT", (int)portrait_id * 3 + 1, &buf);
    if (size < 8 || buf == 0) {
        if (buf) free(buf);
        return -1;
    }
    pose_count = buf[0];          /* function reads *(uint8 *)figani_buf */
    n = 0;
    for (i = 0; i < pose_count; i++) {
        uint32 off;
        uint8  sfx;
        memcpy(&off, buf + 8 + i * 4, 4);
        if (off + 6 >= (uint32)size) {
            continue;
        }
        sfx = buf[off + 5];
        if (sfx != 0 && n < out_cap) {
            out[n++] = (int)sfx;
        }
    }
    free(buf);
    return n;
}

/* Drive the real function for one portrait and assert the recorded SFX id
 * stream == [<each non-zero pose hook in order>, -1(stop-all)]. */
static void run_intro_case(uint8 portrait_id, uint8 team)
{
    int    exp[64];
    int    exp_n;
    int    i;
    int    n;
    int    bd_count;
    int    bd_first_byte;
    uint8 *bg_payload;
    long   bg_size;

    exp_n = figani_expected_sfx(portrait_id, exp, 64);
    ASSERT_TRUE(exp_n >= 0);          /* FIGANI.DAT staged + entry parsable */

    /* Independent BG.DAT[CINE_BG_INDEX] first byte, to confirm the function
     * selected the BG index from tile_attr[+6] (not [0]). */
    bg_size = realdat_read_resource("BG.DAT", CINE_BG_INDEX, &bg_payload);
    ASSERT_TRUE(bg_size > 0 && bg_payload != 0);

    setup_figani_intro(portrait_id, team);
    fd2_play_figani_char_intro_animation(0);

    /* SFX bank was loaded from the figani stream and forwarded */
    ASSERT_EQ(g_load_figani_sfx_bank_calls, 1);

    /* zoom-anim hand-off: char idx 0, mode flag 1 (per the call site) */
    ASSERT_EQ(g_zoom_anim_calls, 1);
    ASSERT_EQ((long)g_zoom_anim_last_char, 0L);
    ASSERT_EQ((long)g_zoom_anim_last_mode, 1L);

    /* Find the unique BG backdrop blit in the rle log. The backdrop is the only
     * call passing literal (dst_x=0, dst_y=0x32); pose-sprite blits carry
     * width/height (x>=1) and the mini-panel uses other offsets. Its sprite is
     * BG.DAT[tile_attr[+6]]; the captured first payload byte must equal
     * BG.DAT[CINE_BG_INDEX][0] -> pins the +6 (not +0) tile-attr index path.
     * (First byte captured at blit time; the buffer is freed by cleanup.) */
    n = g_rle_blit_calls < 64 ? g_rle_blit_calls : 64;
    bd_count = 0;
    bd_first_byte = -1;
    for (i = 0; i < n; i++) {
        if (g_rle_blit_log_x[i] == 0 && g_rle_blit_log_y[i] == 0x32) {
            bd_count++;
            bd_first_byte = (int)g_rle_blit_log_first_byte[i];
        }
    }
    ASSERT_EQ(bd_count, 1);
    ASSERT_EQ(bd_first_byte, (int)bg_payload[0]);
    free(bg_payload);

    /* fired ids = per-pose hooks (in order) then the cleanup stop-all (-1) */
    ASSERT_EQ(g_sfx_id_count, exp_n + 1);
    for (i = 0; i < exp_n; i++) {
        ASSERT_EQ(g_sfx_id_log[i], exp[i]);
    }
    ASSERT_EQ(g_sfx_id_log[exp_n], -1);
}

/*
 * Portrait 0x01 (FIGANI idx 4): exactly one pose fires SFX (id 5 at pose 2);
 * the other 6 poses have hook byte 0 and must be skipped. Exercises both the
 * fire and the skip arm of the per-pose conditional.
 */
static void test_intro_single_sfx_fire(void)
{
    run_intro_case(0x01, 2);
}

/*
 * Portrait 0x00 (FIGANI idx 1, 11 poses): a single fire (id 3) at a later
 * pose index. Independent entry + different team (enemy) -> the flash router
 * takes the bTeam==0 branch; still host-safe.
 */
static void test_intro_other_portrait(void)
{
    run_intro_case(0x00, 0);
}

/*
 * Portrait 0x20 (FIGANI idx 97, 13 poses): multiple fires in one stream
 * (ids 1,1,1,...,5) -> verifies the ordered multi-fire sequence, not just a
 * single trigger.
 */
static void test_intro_multi_sfx_sequence(void)
{
    run_intro_case(0x20, 2);
}

/* ----------------------------------------------------------------
 * fd2_play_full_combat_cinematic @ 0x28A6C  (scripted-mode path)
 *
 * The full attack/counter cinematic. These tests drive the SCRIPTED-mode
 * path (data_..._scripted_cinematic_mode != 0), which isolates the novel
 * value/branch logic — name-banner index forcing and the attacker/counter
 * dispatch order — from the non-scripted display-cache plumbing (the entry
 * cache frees, FD2.TMP portrait-cache restore, and final composite are all
 * gated off in scripted mode and deferred to Phase 9 integration).
 *
 * In scripted mode the function: skips the mini-panel flash + SFX-bank
 * load + cleanup, forces the spotlight terrain to the scripted value,
 * forces the banner index to 3 for the climactic portraits (attacker
 * 0x1A/0x36 or defender 0x37) else uses the scripted value, then calls
 * fd2_execute_combat_hit_cinematic twice — first (attacker, defender),
 * then (defender, attacker) for the guided counter — and latches the
 * scripted flag to 1.
 *
 * The not-yet-emitted fd2_execute_combat_hit_cinematic is spied
 * (testglob.c): the spy records each call's attacker/defender order and
 * the banner sprite's first payload byte, which is cross-checked against
 * an INDEPENDENT realdat read of TAI.DAT[expected index] — pinning which
 * TAI entry the forcing logic selected, with no hardcoded magic.
 * ---------------------------------------------------------------- */

/* fd2_execute_combat_hit_cinematic spy (testglob.c) */
extern int    g_exec_hit_calls;
extern uint32 g_exec_hit_att[8];
extern uint32 g_exec_hit_def[8];
extern int    g_exec_hit_banner_first[8];
extern uint32 g_exec_hit_sfx[8];
extern int    g_exec_hit_return;

/* A valid BG.DAT index used as the scripted spotlight-terrain value. */
#define CINE_SCRIPT_BG  5

/* Seed two adjacent runtime chars (attacker = idx 0, defender = idx 1) with
 * the given portraits and a non-zero scripted-cinematic mode. job_id and
 * archetype are 0 (not an immune class) so the terrain path reads the tile;
 * the tile-map fixture makes that read well-defined. */
static void setup_scripted_cinematic(uint8 att_portrait, uint8 def_portrait,
                                     uint32 scripted_mode)
{
    int i;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = CINE_WIN_OX + 1;
    g_test_rc_array[0].pos_y = CINE_WIN_OY + 1;
    g_test_rc_array[0].portrait_id = att_portrait;
    g_test_rc_array[0].team = 2;                /* player attacker */
    g_test_rc_array[1].pos_x = CINE_WIN_OX + 2; /* adjacent to attacker */
    g_test_rc_array[1].pos_y = CINE_WIN_OY + 1;
    g_test_rc_array[1].portrait_id = def_portrait;
    g_test_rc_array[1].team = 0;                /* enemy defender */

    memset(g_cine_tile_map, 0, sizeof(g_cine_tile_map));
    memset(g_cine_attr_buf, 0, sizeof(g_cine_attr_buf));
    memset(g_cine_tile_event, 0, sizeof(g_cine_tile_event));
    data_fd2_battle_tile_map_ptr = (uint32)g_cine_tile_map;
    data_fd2_battle_map_width_tiles = CINE_MAP_W;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_cine_attr_buf;
    data_fd2_tile_event_data_table_ptr = (uint32)g_cine_tile_event;
    data_fd2_chapter_current_chapter_id = 1;

    /* scripted mode: skips entry cache frees + cleanup, so these globals are
     * never freed/restored; the SFX banks are not loaded (stay as set). */
    data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = scripted_mode;
    data_fd2_audio_figani_sfx_bank_buf_ptr = 0;
    data_fd2_audio_figani_sfx_bank_defender_buf_ptr = 0;

    g_zoom_anim_calls = 0;
    g_zoom_anim_last_char = 0;
    g_zoom_anim_last_mode = 0;

    g_exec_hit_calls = 0;
    g_exec_hit_return = 1;
    for (i = 0; i < 8; i++) {
        g_exec_hit_att[i] = 0;
        g_exec_hit_def[i] = 0;
        g_exec_hit_banner_first[i] = 0;
        g_exec_hit_sfx[i] = 0;
    }
}

/* Drive the scripted cinematic and assert the two-call dispatch order +
 * the banner index the forcing logic selected (via TAI.DAT[exp_banner]). */
static void run_scripted_case(uint8 att_portrait, uint8 def_portrait,
                              uint32 scripted_mode, int exp_banner_idx)
{
    uint8 *tai;
    long   tai_size;
    int    exp_first;
    uint32 exp_flag;
    uint8 *fig;
    long   fig_size;

    /* independent TAI.DAT[exp_banner_idx] first byte */
    tai_size = realdat_read_resource("TAI.DAT", exp_banner_idx, &tai);
    ASSERT_TRUE(tai_size > 0 && tai != 0);
    exp_first = (int)tai[0];
    free(tai);

    /* independent split-screen flag = attacker anim-FIGANI[+1] (the byte the
     * function reads to choose single vs split background) */
    fig_size = realdat_read_resource("FIGANI.DAT",
                                     (int)att_portrait * 3 + 1, &fig);
    ASSERT_TRUE(fig_size > 1 && fig != 0);
    exp_flag = (uint32)fig[1];
    free(fig);

    setup_scripted_cinematic(att_portrait, def_portrait, scripted_mode);
    fd2_play_full_combat_cinematic(0, 1);

    /* scripted mode dispatches the hit cinematic exactly twice: the attacker
     * blow (0->1) then the guided counter (1->0) */
    ASSERT_EQ(g_exec_hit_calls, 2);
    ASSERT_EQ((long)g_exec_hit_att[0], 0L);
    ASSERT_EQ((long)g_exec_hit_def[0], 1L);
    ASSERT_EQ((long)g_exec_hit_att[1], 1L);
    ASSERT_EQ((long)g_exec_hit_def[1], 0L);

    /* both calls receive the same banner sprite; its first payload byte pins
     * the forced/selected TAI index */
    ASSERT_EQ(g_exec_hit_banner_first[0], exp_first);
    ASSERT_EQ(g_exec_hit_banner_first[1], exp_first);

    /* scripted mode does not load the SFX banks: both forwarded handles are 0 */
    ASSERT_EQ((long)g_exec_hit_sfx[0], 0L);
    ASSERT_EQ((long)g_exec_hit_sfx[1], 0L);

    /* zoom-anim handed off once: char idx 0 (attacker), mode = split flag */
    ASSERT_EQ(g_zoom_anim_calls, 1);
    ASSERT_EQ((long)g_zoom_anim_last_char, 0L);
    ASSERT_EQ((long)g_zoom_anim_last_mode, (long)exp_flag);

    /* the scripted flag is latched to 1 by the guided-counter block */
    ASSERT_EQ((long)data_fd2_battle_scripted_cinematic_mode_or_terrain_idx, 1L);
}

/*
 * Scripted + attacker portrait 0x1A (a climactic-portrait trigger): the
 * banner index is FORCED to 3 regardless of the scripted-mode value 5.
 * TAI[3] (first byte) differs from TAI[5], so the assertion distinguishes
 * the forced path from the not-forced path. (FIGANI[0x1A*3+1][+1] == 0, so
 * the single-background arm is taken.)
 */
static void test_scripted_banner_forced(void)
{
    run_scripted_case(0x1A, 0x02, CINE_SCRIPT_BG, 3);
}

/*
 * Scripted + neither trigger portrait (attacker 0x01, defender 0x02, none of
 * 0x1A/0x36/0x37): the banner index is the scripted-mode value itself
 * (CINE_SCRIPT_BG), exercising the not-forced arm of the same branch.
 */
static void test_scripted_banner_not_forced(void)
{
    run_scripted_case(0x01, 0x02, CINE_SCRIPT_BG, CINE_SCRIPT_BG);
}

/*
 * Scripted dispatch is independent of the attacker-blow return value: even
 * when fd2_execute_combat_hit_cinematic reports "miss" (return 0), the
 * guided-counter block (gated only by the scripted flag, not the return)
 * still fires the second call. Confirms the scripted counter is NOT gated by
 * the hit-landed result the way the non-scripted counter is.
 */
static void test_scripted_counter_ignores_hit_result(void)
{
    uint8 *tai;
    long   tai_size;
    int    exp_first;

    tai_size = realdat_read_resource("TAI.DAT", 3, &tai);
    ASSERT_TRUE(tai_size > 0 && tai != 0);
    exp_first = (int)tai[0];
    free(tai);

    setup_scripted_cinematic(0x1A, 0x02, CINE_SCRIPT_BG);
    g_exec_hit_return = 0;               /* attacker blow "misses" */
    fd2_play_full_combat_cinematic(0, 1);

    ASSERT_EQ(g_exec_hit_calls, 2);      /* counter still dispatched */
    ASSERT_EQ((long)g_exec_hit_att[1], 1L);
    ASSERT_EQ((long)g_exec_hit_def[1], 0L);
    ASSERT_EQ(g_exec_hit_banner_first[1], exp_first);
}

/* ----------------------------------------------------------------
 * fd2_play_full_combat_cinematic @ 0x28A6C  (non-scripted terrain-override path)
 *
 * Regression for the immune-class + chapter-override==0 sub-case, where the
 * binary keeps TWO distinct terrain values (EAX vs the [ESP+8] slot):
 *
 *   banner_term     (EAX, drives the TAI.DAT name-banner index)
 *   defender_terrain ([ESP+8], drives the BG.DAT split-bg index)
 *
 * Both start equal to the per-chapter override byte. For an immune terrain
 * char (job 0x13 / archetype 4|5, portrait != 0x1C) with override==0, only
 * [ESP+8] is reloaded to the under-foot tile attribute while EAX stays = the
 * override (0). At the non-scripted exit the value PUSHed as the TAI.DAT index
 * is EAX, so the name banner is TAI.DAT[override] = TAI.DAT[0], NOT
 * TAI.DAT[tile_attr[6]]. (Verified at 0x28bff JNZ / 0x28c01..0x28c0a — EAX is
 * never reloaded in that arm — and 0x28c41 PUSH EAX.)
 *
 * The non-scripted path loads that banner unconditionally and forwards it to
 * fd2_execute_combat_hit_cinematic; the spy captures the banner sprite's first
 * payload byte, cross-checked against an INDEPENDENT realdat read of
 * TAI.DAT[0] (no hardcoded magic). The tile_attr[+6] fixture byte points at a
 * different TAI entry whose first byte differs from TAI.DAT[0]'s, so the
 * assertion fails if the banner index ever collapses back to defender_terrain.
 *
 * Chapter 24 is chosen because data_fd2_chapter_combat_cinematic_mode_per_chapter
 * [24] == 0 (the real FD2.LE override table). The full non-scripted cleanup
 * (cache frees + reload + final composite) runs host-safely over the same
 * in-memory battle-scene fixture the FIGANI-intro test uses.
 * ---------------------------------------------------------------- */

/* tile_attr[+6] value the buggy (collapsed) banner index would select. It must
 * resolve to a TAI.DAT entry whose first byte differs from TAI.DAT[0]'s. */
#define CINE_OVR_TILE_BG  5

/* Seed attacker (idx 0) as the immune TERRAIN char and defender (idx 1) as the
 * spotlight, non-scripted, on a chapter whose override byte is 0. Attacker
 * team != 0 -> p_terrain = attacker; job_id 0x13 + portrait != 0x1C makes the
 * immune branch fire; the zeroed inventory makes fd2_check_can_counter_attack
 * return -1 (no counter FIGANI / single hit). */
static void setup_override_terrain(uint8 chapter_idx)
{
    int i;
    uint32 *table;

    memset(g_test_rc_array, 0, sizeof(g_test_rc_array));
    g_test_rc_array[0].pos_x = CINE_WIN_OX + 1;
    g_test_rc_array[0].pos_y = CINE_WIN_OY + 1;
    g_test_rc_array[0].portrait_id = 0x01;      /* != 0x1C, FIGANI split flag 0 */
    g_test_rc_array[0].team = 2;                /* player -> terrain = attacker */
    g_test_rc_array[0].job_id = 0x13;           /* immune class */
    g_test_rc_array[1].pos_x = CINE_WIN_OX + 2; /* adjacent to attacker */
    g_test_rc_array[1].pos_y = CINE_WIN_OY + 1;
    g_test_rc_array[1].portrait_id = 0x02;
    g_test_rc_array[1].team = 0;                /* enemy spotlight */

    /* zeroed tile map -> sprite_idx 0; attr_buf[2] lands at tile_attr[+6]. Set
     * it to the non-override BG so banner_term(=override 0) and defender_terrain
     * (=tile_attr[6]) provably diverge. */
    memset(g_cine_tile_map, 0, sizeof(g_cine_tile_map));
    memset(g_cine_attr_buf, 0, sizeof(g_cine_attr_buf));
    memset(g_cine_tile_event, 0, sizeof(g_cine_tile_event));
    g_cine_attr_buf[2] = CINE_OVR_TILE_BG;
    data_fd2_battle_tile_map_ptr = (uint32)g_cine_tile_map;
    data_fd2_battle_map_width_tiles = CINE_MAP_W;
    data_fd2_tile_attribute_flags_buffer_ptr = (uint32)g_cine_attr_buf;
    data_fd2_tile_event_data_table_ptr = (uint32)g_cine_tile_event;
    data_fd2_chapter_current_chapter_id = chapter_idx;

    /* non-scripted: the entry cache frees + final reload/composite all run, so
     * NULL the freed globals (free(NULL) no-op; the loader-reload mallocs run)
     * and stand up the portrait cache + mini-panel + view-window fixtures. */
    data_fd2_battle_scripted_cinematic_mode_or_terrain_idx = 0;
    data_fd2_large_game_state_buffer_ptr = 0;
    battle_scene_snapshot = 0;
    portrait_sprite_cache = 0;

    table = (uint32 *)g_cine_portrait_cache;
    for (i = 0; i < 256; i++) {
        table[i] = (uint32)i * 0x100u;
    }
    portrait_sprite_cache = (uint32)g_cine_portrait_cache;

    data_fd2_battle_view_window_origin_x = CINE_WIN_OX;
    data_fd2_battle_view_window_origin_y = CINE_WIN_OY;
    data_fd2_battle_view_window_max_x = 0x100;
    data_fd2_battle_view_window_max_y = 0x100;
    data_fd2_ui_terrain_hud_user_enabled = 0;
    data_fd2_ui_play_active_flag = 0;
    data_fd2_battle_anim_phase = 0;
    data_fd2_animation_palette_cycle_last_tick = (uint16)BIOS_TICK_WORD;

    minip_setup_env();

    /* SFX banks: stub returns a real malloc'd handle (freed by cleanup). */
    g_figani_sfx_bank_nonnull = 1;
    g_load_figani_sfx_bank_calls = 0;
    data_fd2_audio_figani_sfx_bank_buf_ptr = 0;
    data_fd2_audio_figani_sfx_bank_defender_buf_ptr = 0;

    g_zoom_anim_calls = 0;
    g_zoom_anim_last_char = 0;
    g_zoom_anim_last_mode = 0;

    g_exec_hit_calls = 0;
    g_exec_hit_return = 1;
    for (i = 0; i < 8; i++) {
        g_exec_hit_att[i] = 0;
        g_exec_hit_def[i] = 0;
        g_exec_hit_banner_first[i] = 0;
        g_exec_hit_sfx[i] = 0;
    }
}

/*
 * Non-scripted, immune terrain char, chapter 24 (override byte 0): the
 * name-banner index must be the override (0) -> TAI.DAT[0], NOT the under-foot
 * tile attribute (CINE_OVR_TILE_BG). Asserts the banner sprite forwarded to
 * fd2_execute_combat_hit_cinematic is TAI.DAT[0] via an independent read, and
 * confirms TAI.DAT[0] and TAI.DAT[CINE_OVR_TILE_BG] actually differ so the
 * check discriminates the fixed value flow from the collapsed one.
 */
static void test_nonscripted_immune_override_zero_banner(void)
{
    uint8 *tai0;
    uint8 *tai_tile;
    long   sz0;
    long   sz_tile;
    int    exp_first0;
    int    tile_first;

    /* independent TAI.DAT[0] (the override index) first byte */
    sz0 = realdat_read_resource("TAI.DAT", 0, &tai0);
    ASSERT_TRUE(sz0 > 0 && tai0 != 0);
    exp_first0 = (int)tai0[0];
    free(tai0);

    /* independent TAI.DAT[tile] first byte -> must differ from TAI.DAT[0]'s, or
     * the assertion below could not distinguish the bug from the fix */
    sz_tile = realdat_read_resource("TAI.DAT", CINE_OVR_TILE_BG, &tai_tile);
    ASSERT_TRUE(sz_tile > 0 && tai_tile != 0);
    tile_first = (int)tai_tile[0];
    free(tai_tile);
    ASSERT_TRUE(exp_first0 != tile_first);

    setup_override_terrain(24);
    fd2_play_full_combat_cinematic(0, 1);

    /* non-scripted single attacker blow (no counter: zeroed defender weapon) */
    ASSERT_EQ(g_exec_hit_calls, 1);
    ASSERT_EQ((long)g_exec_hit_att[0], 0L);
    ASSERT_EQ((long)g_exec_hit_def[0], 1L);

    /* the forwarded banner sprite is TAI.DAT[override=0], NOT TAI.DAT[tile] */
    ASSERT_EQ(g_exec_hit_banner_first[0], exp_first0);
}

void run_anim_anicine_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anicine\n");
    RUN_TEST(test_intro_single_sfx_fire);
    RUN_TEST(test_intro_other_portrait);
    RUN_TEST(test_intro_multi_sfx_sequence);
    RUN_TEST(test_scripted_banner_forced);
    RUN_TEST(test_scripted_banner_not_forced);
    RUN_TEST(test_scripted_counter_ignores_hit_result);
    RUN_TEST(test_nonscripted_immune_override_zero_banner);
    printf("\n");
    (void)_prev_fails;
}
