/*
 * unit tests for src/anim/anidec.c
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


/* ---- ANI decoder tests ---- */

static uint8 g_test_palette_buf[768];

static uint8 g_test_row_buf[320];


static void test_ani_palette_fill_byte(void)
{
    uint8 stream[2];
    stream[0] = 0;
    stream[1] = 0x42;
    data_fd2_animation_ani_decoder_src_buf = (uint32)g_test_palette_buf;
    memset(g_test_palette_buf, 0, 768);
    data_fd2_animation_ani_decoder_frame_dispatch_table[0] =
        (void *)fd2_ani_decoder_chunk_palette_fill_byte;
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_palette_buf[0], 0x42);
    ASSERT_EQ((long)g_test_palette_buf[767], 0x42);
}


static void test_ani_row_copy_literal(void)
{
    uint8 stream[6];
    stream[0] = 1;
    stream[1] = 0xAA;
    stream[2] = 0xBB;
    stream[3] = 0xCC;
    stream[4] = 0xDD;
    stream[5] = 0xEE;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_target_width = 5;
    data_fd2_animation_ani_decoder_frame_dispatch_table[1] =
        (void *)fd2_ani_decoder_chunk_row_copy_literal;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[0], 0xAA);
    ASSERT_EQ((long)g_test_row_buf[4], 0xEE);
}


static void test_ani_sparse_set_byte(void)
{
    uint8 stream[10];
    stream[0] = 2;
    *(uint16 *)(stream + 1) = 2;
    *(uint16 *)(stream + 3) = 5;
    stream[5] = 0x77;
    *(uint16 *)(stream + 6) = 10;
    stream[8] = 0x88;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_frame_dispatch_table[2] =
        (void *)fd2_ani_decoder_chunk_sparse_set_byte;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[5], 0x77);
    ASSERT_EQ((long)g_test_row_buf[10], 0x88);
}


/*
 * test_ani_sparse_set_run_byte
 *
 * Drives fd2_ani_decoder_chunk_sparse_set_run_byte via decode_frame_bytes
 * (chunk-type byte 8 = real Ghidra dispatch slot @ 0x5278A, base 0x5276A +
 * index 8*4 = 0x20 per plate).  This is the high-risk run-fill handler:
 * multi-record loop, per-record offset positioning, and a counted run-fill
 * implemented in asm (00036c3d..00036c53) as
 *   LODSW offset -> EDI = dst_buf + offset
 *   LODSB run_count ; LODSB value (broadcast AL->AH)
 *   SHR ECX,1 + REP STOSW ; RCL ECX,1 + REP STOSB
 * i.e. memset(base + offset, value, run_count).  Emitted C uses memset,
 * which is byte-identical to the SHR/STOSW + RCL/STOSB sequence.
 *
 * Stream (handler reads from g_ani_cursor = stream+1):
 *   ushort N : count of run records
 *   N * { ushort offset, byte run_count, byte value }
 * Layout (N=2):
 *   rec0: offset=3,  run_count=4, value=0xAB -> base[3..6]   = 0xAB
 *   rec1: offset=20, run_count=5, value=0xCD -> base[20..24] = 0xCD
 *
 * Expected values are hand-derived from the stream format (pure memset
 * semantics, no emulate needed -- plate notes emulate memory injection is
 * non-functional for this routine in this Ghidra build) and asserted
 * against a real 320-byte row buffer.  Both even (run_count=4 -> STOSW
 * only) and odd (run_count=5 -> STOSW + trailing STOSB) run lengths are
 * exercised, and the bytes immediately before/after each run are checked
 * for no overshoot.
 */
static void test_ani_sparse_set_run_byte(void)
{
    uint8 stream[11];

    stream[0] = 8;              /* dispatch index -> sparse_set_run_byte */
    *(uint16 *)(stream + 1) = 2;  /* N: 2 run records */
    /* rec0: offset=3, run_count=4 (even), value=0xAB */
    *(uint16 *)(stream + 3) = 3;
    stream[5] = 4;
    stream[6] = 0xAB;
    /* rec1: offset=20, run_count=5 (odd, trailing STOSB), value=0xCD */
    *(uint16 *)(stream + 7) = 20;
    stream[9] = 5;
    stream[10] = 0xCD;

    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_frame_dispatch_table[8] =
        (void *)fd2_ani_decoder_chunk_sparse_set_run_byte;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);

    /* rec0: 4 bytes of 0xAB at offset 3 */
    ASSERT_EQ((long)g_test_row_buf[2],  0x00);  /* before run: untouched */
    ASSERT_EQ((long)g_test_row_buf[3],  0xAB);  /* run start */
    ASSERT_EQ((long)g_test_row_buf[6],  0xAB);  /* run end (even, STOSW) */
    ASSERT_EQ((long)g_test_row_buf[7],  0x00);  /* after run: no overshoot */
    /* rec1: 5 bytes of 0xCD at offset 20 */
    ASSERT_EQ((long)g_test_row_buf[19], 0x00);  /* before run: untouched */
    ASSERT_EQ((long)g_test_row_buf[20], 0xCD);  /* run start */
    ASSERT_EQ((long)g_test_row_buf[24], 0xCD);  /* run end (odd, trailing STOSB) */
    ASSERT_EQ((long)g_test_row_buf[25], 0x00);  /* after run: no overshoot */
}


/*
 * test_ani_sparse_copy_literal
 *
 * Drives fd2_ani_decoder_chunk_sparse_copy_literal via decode_frame_bytes
 * (chunk-type byte 9 = real Ghidra dispatch slot @ 0x5278E, base 0x5276A +
 * index 9*4 = 0x24 per plate).  This is the row-buffer twin of
 * sparse_set_run_byte / palette_load_run_pairs: a multi-record loop with
 * per-record offset positioning and a counted literal copy implemented in
 * asm (00036c56..00036c7c) as
 *   LODSW N ; loop { LODSW offset -> EDI = dst_buf + offset ;
 *     LODSB count -> CL ; SHR ECX,1 + REP MOVSW ; RCL ECX,1 + REP MOVSB ;
 *     EDI = dst_buf ; DEC EDX JNZ }
 * i.e. memcpy(base + offset, cursor, count) per record, cursor advancing
 * across records.  Emitted C uses memcpy, byte-identical to the
 * SHR/REP MOVSW + RCL/REP MOVSB sequence (even half as words, trailing odd
 * byte as a single MOVSB).
 *
 * Stream format (handler reads from g_ani_cursor = stream+1):
 *   ushort N : count of copy records
 *   N * { ushort offset, byte count, byte[count] literal_data }
 * Layout (N=2):
 *   rec0: offset=3,  count=4 (even, REP MOVSW only)        -> base[3..6]
 *   rec1: offset=20, count=5 (odd, REP MOVSW + trail MOVSB)-> base[20..24]
 *
 * Expected values are hand-derived from the pure-memcpy format (same
 * justification as sibling sparse/run-pair tests; emulate_function memory
 * injection is non-functional for this routine in this Ghidra build).
 * Both even (count=4) and odd (count=5) lengths are exercised, the bytes
 * immediately before/after each copied region are checked for no overshoot,
 * and rec1 reading from the correct cursor position confirms the inter-record
 * cursor advance (2 + sum_i(3 + count_i)).
 */
static void test_ani_sparse_copy_literal(void)
{
    /* stream size = 1 (dispatch) + 2 (N) + rec0(3+4) + rec1(3+5) = 18 */
    uint8 stream[18];

    stream[0] = 9;                 /* dispatch index -> sparse_copy_literal */
    *(uint16 *)(stream + 1) = 2;   /* N: 2 copy records */
    /* rec0: offset=3, count=4 (even, REP MOVSW only) -> 0x11 0x22 0x33 0x44 */
    *(uint16 *)(stream + 3) = 3;
    stream[5] = 4;
    stream[6] = 0x11;
    stream[7] = 0x22;
    stream[8] = 0x33;
    stream[9] = 0x44;
    /* rec1: offset=20, count=5 (odd, REP MOVSW + trailing MOVSB) -> 0xAA..0xEE */
    *(uint16 *)(stream + 10) = 20;
    stream[12] = 5;
    stream[13] = 0xAA;
    stream[14] = 0xBB;
    stream[15] = 0xCC;
    stream[16] = 0xDD;
    stream[17] = 0xEE;

    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_frame_dispatch_table[9] =
        (void *)fd2_ani_decoder_chunk_sparse_copy_literal;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);

    /* rec0: 4 literal bytes at offset 3 (even count, REP MOVSW) */
    ASSERT_EQ((long)g_test_row_buf[2],  0x00);  /* before: untouched */
    ASSERT_EQ((long)g_test_row_buf[3],  0x11);
    ASSERT_EQ((long)g_test_row_buf[4],  0x22);
    ASSERT_EQ((long)g_test_row_buf[5],  0x33);
    ASSERT_EQ((long)g_test_row_buf[6],  0x44);  /* run end (even) */
    ASSERT_EQ((long)g_test_row_buf[7],  0x00);  /* after: no overshoot */
    /* rec1: 5 literal bytes at offset 20 (odd count, trailing MOVSB) */
    ASSERT_EQ((long)g_test_row_buf[19], 0x00);  /* before: untouched */
    ASSERT_EQ((long)g_test_row_buf[20], 0xAA);
    ASSERT_EQ((long)g_test_row_buf[21], 0xBB);
    ASSERT_EQ((long)g_test_row_buf[22], 0xCC);
    ASSERT_EQ((long)g_test_row_buf[23], 0xDD);
    ASSERT_EQ((long)g_test_row_buf[24], 0xEE);  /* run end (trailing MOVSB) */
    ASSERT_EQ((long)g_test_row_buf[25], 0x00);  /* after: no overshoot */
}


static void test_ani_decode_frame_dispatch(void)
{
    uint8 stream[4];
    stream[0] = 3;
    stream[1] = 0x55;
    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_target_width = 4;
    data_fd2_animation_ani_decoder_frame_dispatch_table[3] =
        (void *)fd2_ani_decoder_chunk_row_fill_byte;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);
    ASSERT_EQ((long)g_test_row_buf[0], 0x55);
    ASSERT_EQ((long)g_test_row_buf[3], 0x55);
}


static void test_ani_decoder_set_target_buffer(void)
{
    fd2_ani_decoder_set_target_buffer(320, 0x12345, 0xABCDE);
    ASSERT_EQ(data_fd2_animation_ani_decoder_target_width, 320);
    ASSERT_EQ(data_fd2_animation_ani_decoder_dst_buf, 0x12345);
    ASSERT_EQ(data_fd2_animation_ani_decoder_src_buf, 0xABCDE);
}


/*
 * test_ani_palette_load_rle
 *
 * Drives fd2_ani_decoder_chunk_palette_load_rle via decode_frame_bytes
 * (chunk-type byte 2 = real Ghidra dispatch slot @ 0x52772, base 0x5276A
 * + index 2*4).  Exercises every decode path of the handler:
 *   - literal bytes (top two bits != 11)            -> 0x01, 0x02
 *   - even RLE run (REP STOSW only, no trailing)    -> 0xFE -> 62x 0x33
 *   - odd  RLE run (REP STOSW + trailing REP STOSB) -> 0xFF -> 63x 0x44
 *   - run boundaries that sum to exactly 0x300      -> 10x (0xFF -> 0x55)
 *   - terminating odd run hitting pos == 768 exactly-> 0xCB -> 11x 0x66
 *
 * 0xC0|N is a run of length N; the following byte is the value V.  Token
 * stream consumed by the handler (g_ani_cursor starts at stream+1):
 *   01 02 FE 33 FF 44 (FF 55)x10 CB 66
 * Output layout (sums to 2+62+63+630+11 = 768):
 *   [0..1]=01,02  [2..63]=33  [64..126]=44  [127..756]=55  [757..767]=66
 *
 * Expected values are hand-derived from the RLE format (verified against
 * the function's assembly: AND AL,0x3F run length, SHR/REP STOSW + RCL/REP
 * STOSB byte-broadcast write).  emulate_function memory injection is
 * non-functional for this routine in this Ghidra build (the src_buf
 * pointer at 0x52766 and the ESI-pointed stream are silently ignored, so
 * EDI base stays 0 and ESI consumes static-image zero bytes instead of an
 * injected stream); ground truth therefore comes from the format, while
 * the decode logic itself executes through the compiled C against a real
 * 768-byte buffer here.
 */
static void test_ani_palette_load_rle(void)
{
    uint8 stream[29];
    int i;

    stream[0]  = 2;     /* dispatch index -> palette_load_rle */
    stream[1]  = 0x01;  /* literal */
    stream[2]  = 0x02;  /* literal */
    stream[3]  = 0xC0 | 62; /* 0xFE: even run, length 62 */
    stream[4]  = 0x33;
    stream[5]  = 0xC0 | 63; /* 0xFF: odd run, length 63 */
    stream[6]  = 0x44;
    for (i = 0; i < 10; i++) {
        stream[7 + i * 2] = 0xC0 | 63; /* 0xFF: run, length 63 */
        stream[8 + i * 2] = 0x55;
    }
    stream[27] = 0xC0 | 11; /* 0xCB: terminating odd run, length 11 */
    stream[28] = 0x66;

    data_fd2_animation_ani_decoder_src_buf = (uint32)g_test_palette_buf;
    data_fd2_animation_ani_decoder_frame_dispatch_table[2] =
        (void *)fd2_ani_decoder_chunk_palette_load_rle;
    memset(g_test_palette_buf, 0, 768);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);

    ASSERT_EQ((long)g_test_palette_buf[0], 0x01);   /* first literal */
    ASSERT_EQ((long)g_test_palette_buf[1], 0x02);   /* second literal */
    ASSERT_EQ((long)g_test_palette_buf[2], 0x33);   /* even run start */
    ASSERT_EQ((long)g_test_palette_buf[63], 0x33);  /* even run end (boundary) */
    ASSERT_EQ((long)g_test_palette_buf[64], 0x44);  /* odd run start */
    ASSERT_EQ((long)g_test_palette_buf[126], 0x44); /* odd run last byte (trailing STOSB) */
    ASSERT_EQ((long)g_test_palette_buf[127], 0x55); /* fill run start */
    ASSERT_EQ((long)g_test_palette_buf[756], 0x55); /* fill run last byte */
    ASSERT_EQ((long)g_test_palette_buf[757], 0x66); /* final run start */
    ASSERT_EQ((long)g_test_palette_buf[767], 0x66); /* exact termination at 768 */
}


/*
 * test_ani_palette_load_run_pairs
 *
 * Drives fd2_ani_decoder_chunk_palette_load_run_pairs via decode_frame_bytes
 * (chunk-type byte 3 = real Ghidra dispatch slot @ 0x52776, base 0x5276A +
 * index 3*4 = 0xC).  Exercises the numeric / control-flow risk paths of the
 * handler:
 *   - non-zero start: dst = base + start*3 offset (AX 16-bit start*3)
 *   - even byte_count: REP MOVSW only path  (n_colors=2 -> 6 bytes)
 *   - odd  byte_count: REP MOVSW + trailing REP MOVSB (n_colors=1 -> 3 bytes)
 *   - multi-segment (count=2): base reset + cursor advance between segments
 *
 * Stream format (handler reads from g_ani_cursor = stream+1):
 *   byte: count = number of segments
 *   per segment: start, n_colors, then n_colors*3 raw RGB bytes
 * Layout (count=2):
 *   seg0: start=2, n_colors=2 -> 6 bytes at base + 2*3 = base[6..11]
 *   seg1: start=10,n_colors=1 -> 3 bytes at base + 10*3 = base[30..32]
 * Expected values are hand-derived from the format (pure memcpy semantics,
 * no emulate needed) and asserted against a real 768-byte palette buffer.
 */
static void test_ani_palette_load_run_pairs(void)
{
    uint8 stream[15];

    stream[0]  = 3;     /* dispatch index -> palette_load_run_pairs */
    stream[1]  = 2;     /* count: 2 segments */
    /* segment 0: start=2, n_colors=2 (even, 6 bytes -> REP MOVSW only) */
    stream[2]  = 2;
    stream[3]  = 2;
    stream[4]  = 0x11;
    stream[5]  = 0x22;
    stream[6]  = 0x33;
    stream[7]  = 0x44;
    stream[8]  = 0x55;
    stream[9]  = 0x66;
    /* segment 1: start=10, n_colors=1 (odd, 3 bytes -> MOVSW + trailing MOVSB) */
    stream[10] = 10;
    stream[11] = 1;
    stream[12] = 0x77;
    stream[13] = 0x88;
    stream[14] = 0x99;

    data_fd2_animation_ani_decoder_src_buf = (uint32)g_test_palette_buf;
    data_fd2_animation_ani_decoder_frame_dispatch_table[3] =
        (void *)fd2_ani_decoder_chunk_palette_load_run_pairs;
    memset(g_test_palette_buf, 0, 768);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);

    /* seg0: 6 RGB bytes copied to base + 2*3 = offset 6 */
    ASSERT_EQ((long)g_test_palette_buf[6],  0x11);
    ASSERT_EQ((long)g_test_palette_buf[7],  0x22);
    ASSERT_EQ((long)g_test_palette_buf[8],  0x33);
    ASSERT_EQ((long)g_test_palette_buf[9],  0x44);
    ASSERT_EQ((long)g_test_palette_buf[10], 0x55);
    ASSERT_EQ((long)g_test_palette_buf[11], 0x66);
    /* seg1: 3 RGB bytes copied to base + 10*3 = offset 30 (odd trailing MOVSB) */
    ASSERT_EQ((long)g_test_palette_buf[30], 0x77);
    ASSERT_EQ((long)g_test_palette_buf[31], 0x88);
    ASSERT_EQ((long)g_test_palette_buf[32], 0x99);
    /* untouched regions stay zero (no overshoot past segment lengths) */
    ASSERT_EQ((long)g_test_palette_buf[0],  0x00);
    ASSERT_EQ((long)g_test_palette_buf[5],  0x00);
    ASSERT_EQ((long)g_test_palette_buf[12], 0x00);
    ASSERT_EQ((long)g_test_palette_buf[29], 0x00);
    ASSERT_EQ((long)g_test_palette_buf[33], 0x00);
}


/*
 * test_ani_row_decode_rle
 *
 * Drives fd2_ani_decoder_chunk_row_decode_rle via decode_frame_bytes
 * (chunk-type byte 6 = real Ghidra dispatch slot @ 0x52782, base 0x5276A
 * + index 6*4 = 0x18 per plate).  This is the row-buffer twin of
 * palette_load_rle: identical RLE byte format and the same
 * AND AL,0x3F / SHR ECX,1 + REP STOSW / RCL ECX,1 + REP STOSB
 * byte-broadcast write, but bounded by target_width output bytes into
 * dst_buf instead of 768 bytes into src_buf.
 *
 * Exercises every decode path of the handler against target_width = 30:
 *   - literal bytes (top two bits != 11)               -> 0x01, 0x02
 *   - even RLE run (REP STOSW only, no trailing STOSB)  -> 0xC8 -> 8x 0x33
 *   - odd  RLE run (REP STOSW + trailing REP STOSB)     -> 0xC7 -> 7x 0x44
 *   - terminating odd run hitting pos == width exactly  -> 0xCD -> 13x 0x55
 *
 * 0xC0|N is a run of length N; the following byte is the value V to
 * broadcast.  Token stream consumed (g_ani_cursor starts at stream+1):
 *   01 02 C8 33 C7 44 CD 55
 * Output layout (sums to 2+8+7+13 = 30 = target_width):
 *   [0..1]=01,02  [2..9]=33  [10..16]=44  [17..29]=55
 *
 * Expected values are hand-derived from the RLE format, identical to the
 * already-validated palette_load_rle derivation (every byte of a run is the
 * broadcast value V; SHR/REP STOSW pairs cover the even half and RCL/REP
 * STOSB writes the trailing odd byte).  The decode logic executes through
 * the compiled C against a real 320-byte row buffer here.
 */
static void test_ani_row_decode_rle(void)
{
    uint8 stream[9];

    stream[0] = 6;          /* dispatch index -> row_decode_rle */
    stream[1] = 0x01;       /* literal */
    stream[2] = 0x02;       /* literal */
    stream[3] = 0xC0 | 8;   /* 0xC8: even run, length 8 */
    stream[4] = 0x33;
    stream[5] = 0xC0 | 7;   /* 0xC7: odd run, length 7 (trailing STOSB) */
    stream[6] = 0x44;
    stream[7] = 0xC0 | 13;  /* 0xCD: terminating odd run, length 13 */
    stream[8] = 0x55;

    data_fd2_animation_ani_decoder_dst_buf = (uint32)g_test_row_buf;
    data_fd2_animation_ani_decoder_target_width = 30;
    data_fd2_animation_ani_decoder_frame_dispatch_table[6] =
        (void *)fd2_ani_decoder_chunk_row_decode_rle;
    memset(g_test_row_buf, 0, 320);
    fd2_ani_decoder_decode_frame_bytes(1, (uint32)stream);

    ASSERT_EQ((long)g_test_row_buf[0],  0x01);  /* first literal */
    ASSERT_EQ((long)g_test_row_buf[1],  0x02);  /* second literal */
    ASSERT_EQ((long)g_test_row_buf[2],  0x33);  /* even run start */
    ASSERT_EQ((long)g_test_row_buf[9],  0x33);  /* even run end (boundary) */
    ASSERT_EQ((long)g_test_row_buf[10], 0x44);  /* odd run start */
    ASSERT_EQ((long)g_test_row_buf[16], 0x44);  /* odd run last byte (trailing STOSB) */
    ASSERT_EQ((long)g_test_row_buf[17], 0x55);  /* final run start */
    ASSERT_EQ((long)g_test_row_buf[29], 0x55);  /* exact termination at width */
    /* one byte past the decoded row stays untouched (no overshoot) */
    ASSERT_EQ((long)g_test_row_buf[30], 0x00);
}


void run_anim_anidec_tests(void)
{
    int _prev_fails = g_test_fail_count;
    printf("Suite: anim/anidec\n");
    RUN_TEST(test_ani_decoder_set_target_buffer);
    RUN_TEST(test_ani_palette_fill_byte);
    RUN_TEST(test_ani_palette_load_rle);
    RUN_TEST(test_ani_row_decode_rle);
    RUN_TEST(test_ani_palette_load_run_pairs);
    RUN_TEST(test_ani_row_copy_literal);
    RUN_TEST(test_ani_sparse_set_byte);
    RUN_TEST(test_ani_sparse_set_run_byte);
    RUN_TEST(test_ani_sparse_copy_literal);
    RUN_TEST(test_ani_decode_frame_dispatch);
    printf("\n");
}
