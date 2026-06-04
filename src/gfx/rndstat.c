/*
 * rndstat.c — dialog portrait / status-area rendering helpers
 */

#include <string.h>
#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* ----------------------------------------------------------------
 * fd2_paint_portrait_to_dialog_area @ 0x16559 (30 callers)
 *
 * Blit one portrait sprite frame into the active dialog portrait slot,
 * choosing a normal vs horizontally-mirrored blit based on the active
 * portrait slot offset (data_fd2_dialog_active_portrait_blit_offset).
 *
 * The portrait sprite source is the active DATO.DAT entry stored in
 * data_fd2_portrait_sprite_buffer. The first ints at the buffer head
 * form a per-frame sprite-offset table (mouth-open/close cycle); the
 * chosen frame's sprite payload is at buffer + offset_table[frame].
 *
 * data_fd2_dialog_active_portrait_blit_offset:
 *   0x0728  left-side portrait slot   -> normal blit
 *   0x9017  right-side ally portrait  -> mirrored blit
 * The destination linear address is 0xA0000 + that offset.
 *
 * Cdecl, 1 stack param; void return.
 * ---------------------------------------------------------------- */
void fd2_paint_portrait_to_dialog_area(uint32 frame)
{
    uint32 sprite_addr;

    sprite_addr = *(int32 *)(data_fd2_portrait_sprite_buffer + frame * 4)
                  + (uint32)data_fd2_portrait_sprite_buffer;

    if (data_fd2_dialog_active_portrait_blit_offset != 0x9017) {
        fd2_dialog_sprite_blit_normal(
            data_fd2_dialog_active_portrait_blit_offset + 0xa0000,
            sprite_addr, 0x140);
        return;
    }
    fd2_dialog_sprite_blit_mirrored(0xa9017, sprite_addr, 0x140);
}

/* ----------------------------------------------------------------
 * fd2_render_horizontal_bar_segments @ 0x17d6f (1 caller)
 *
 * Render a 1-pixel-segmented horizontal HP/MP bar at dst_offset.
 * Each segment sprite is 1 pixel wide; the bar advances one pixel
 * per segment.
 *
 * filled_count == 0 -> fully empty bar:
 *   0x65 (101) empty-middle sprites (0x1D) at dst_offset+1 .. dst_offset+0x65,
 *   then empty right cap (0x1E) at dst_offset+0x66 (the offset the loop counter
 *   reaches; NOT dst_offset+filled_count, which would be +0 here).
 *
 * filled_count != 0 -> filled bar:
 *   left cap (sprite_base) at dst_offset,
 *   (filled_count-1) middle sprites (sprite_base+1) at dst_offset+1 ..,
 *   then right cap (sprite_base+2) at dst_offset+filled_count.
 *
 * Sprite base indices: 0x17 = HP bar (red), 0x1A = MP bar (blue).
 *
 * Cdecl, 4 stack params; void return. Mirrors the binary's shared-final-blit
 * control flow: the offset of the trailing cap (uVar1) is the value left in
 * EAX by the last LEA inside whichever branch ran.
 * ---------------------------------------------------------------- */
void fd2_render_horizontal_bar_segments(uint32 dst_offset, uint32 dst_pitch,
                                        uint32 filled_count, uint32 sprite_base)
{
    uint32 cap_offset;
    uint32 empty_iter;
    uint32 filled_iter;
    uint32 final_sprite;

    cap_offset = 0;
    empty_iter = 0;
    if (filled_count == 0) {
        for (;;) {
            empty_iter = empty_iter + 1;
            cap_offset = dst_offset + empty_iter;
            if ((int32)empty_iter > 0x65) {
                break;
            }
            fd2_blit_sheet_sprite_at_offset(cap_offset, dst_pitch,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x1d);
        }
        final_sprite = 0x1e;
    } else {
        fd2_blit_sheet_sprite_at_offset(dst_offset, dst_pitch,
                                        data_fd2_ui_anim_sprite_sheet_ptr,
                                        sprite_base);
        for (filled_iter = 1;
             cap_offset = dst_offset + filled_iter,
                 (int32)filled_iter < (int32)filled_count;
             filled_iter = filled_iter + 1) {
            fd2_blit_sheet_sprite_at_offset(cap_offset, dst_pitch,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            sprite_base + 1);
        }
        final_sprite = sprite_base + 2;
    }
    fd2_blit_sheet_sprite_at_offset(cap_offset, dst_pitch,
                                    data_fd2_ui_anim_sprite_sheet_ptr,
                                    final_sprite);
}

/* ----------------------------------------------------------------
 * fd2_render_status_screen_static_layout @ 0x17eef (3 callers)
 *
 * Render the static border + portrait + UI sprites of the character
 * status panel into overlay_buffer, then call the stat-detail painter.
 *
 * Sets the active portrait blit offset to the status-screen mode
 * (0xC88), loads the character's portrait sprite from DATO.DAT, draws
 * the layered dialog frame, blits the portrait and two UI sprites, then
 * fills HP/MP bars and numeric stats via fd2_render_full_char_stat_panel.
 *
 * data_fd2_dialog_active_portrait_blit_offset values:
 *   0x0000  inactive
 *   0x0728  left-side dialog portrait
 *   0x9017  right-side ally dialog portrait
 *   0x0C88  status screen portrait
 *
 * Cdecl, 2 stack params; void return.
 * ---------------------------------------------------------------- */
void fd2_render_status_screen_static_layout(uint32 char_idx, uint32 overlay_buffer)
{
    uint32 portrait_pixels;

    data_fd2_dialog_active_portrait_blit_offset = 0xc88;
    data_fd2_portrait_sprite_buffer =
        (uint8 *)fd2_load_dat_resource(
            0x51a70, (uint32)data_fd2_portrait_sprite_buffer,
            (uint32)data_fd2_battle_runtime_char_array_ptr[char_idx].portrait_id);

    portrait_pixels = (uint32)*data_fd2_portrait_sprite_buffer
                      + (uint32)data_fd2_portrait_sprite_buffer;

    fd2_assemble_dialog_frame_layered(overlay_buffer, 0x140, 5, 7, 5, 5);

    fd2_dialog_sprite_blit_normal(
        overlay_buffer + data_fd2_dialog_active_portrait_blit_offset,
        portrait_pixels, 0x140);
    fd2_dialog_sprite_blit_normal(
        overlay_buffer + 0x91c,
        data_fd2_ui_anim_sprite_sheet_ptr
            + *(int32 *)(data_fd2_ui_anim_sprite_sheet_ptr + 0x56),
        0x140);
    fd2_dialog_sprite_blit_normal(
        overlay_buffer + 0x7585,
        data_fd2_ui_anim_sprite_sheet_ptr
            + *(int32 *)(data_fd2_ui_anim_sprite_sheet_ptr + 0x5a),
        0x140);

    fd2_render_full_char_stat_panel(char_idx, overlay_buffer);
}

/* ----------------------------------------------------------------
 * fd2_render_full_char_stat_panel @ 0x17fc0 (1 caller)
 *
 * Paint the full character stat detail (HP/MP bars + numeric stats +
 * name/archetype/job text labels + team/status icons) for
 * runtime_char[char_idx] into overlay_buffer (a 0x140-stride surface).
 *
 * Layout (offsets within the 0x140-stride surface):
 *
 *   Bars + HP/MP numbers:
 *     +0x2A06  HP bar (sprite base 0x17, proportional)
 *     +0x41C6  MP bar (sprite base 0x1A)
 *     +0x344B  HP current 3-digit (red glow if equal to HP max)
 *     +0x3465  HP max     3-digit (always red: current==max)
 *     +0x4ACB  MP current 3-digit (red glow if equal to MP max)
 *     +0x4AE5  MP max     3-digit (always red)
 *
 *   Status / movement numbers (white 0x2A, 2-digit):
 *     +0x29DD  status_flags_block[0]   (level)
 *     +0x379D  movement_order          (MV)
 *     +0x455D  combat_aux_block[0x14]  (magic resist)
 *
 *   Combat stat numbers (color = 0x77 red if the matching boost flag is
 *   set, else 0x2A white; 3-digit):
 *     +0x545D  ap           (red if status_flags_block[1])
 *     +0x635D  dp           (red if status_flags_block[2])
 *     +0x4535  ai_target_and_dx_block[1] as word (DX base) — always 0x2A
 *     +0x5435  dx_current   (red if status_flags_block[3])
 *     +0x6335  stat4_current (evade) — SAME color flag as dx_current
 *                            (the binary reuses the dx color in ESI)
 *
 *   Text labels (fd2_display_dialog_scene against data_fd2_all_game_text_ptr,
 *   render pitch 0x140, glyph_p5 0xCD, glyph_p6 0x4C, rest 0):
 *     +0x10A3  page = char_id + 1
 *     +0x1113  page = archetype_flag + 0x8C
 *     +0x113B  page = job_id + 0x96
 *
 *   Team / status icons (sheet = data_fd2_ui_anim_sprite_sheet_ptr):
 *     +0x25E5  team flag: sprite 0x36 when team == 0 (enemy), else 0x35
 *     +0x55C2 + i*0x23 (i=0..2): status-icon slot — sprite 0x37+i when the
 *              byte at struct offset 0x25+i is non-zero. Offsets 0x25/0x26/0x27
 *              are status_flags_block[4], status_sleep_flag and
 *              combat_aux_block[0]; the binary reads them as a flat
 *              status_flags_block[4..6] overrun (intentional vendor layout),
 *              so this code walks the raw bytes via a cursor to stay exact.
 *
 * Cdecl, 2 stack params; void return.
 * ---------------------------------------------------------------- */
void fd2_render_full_char_stat_panel(uint32 char_idx, uint32 overlay_buffer)
{
    runtime_char *rc;
    int32         hp_cur;
    int32         hp_max;
    int32         mp_cur;
    int32         mp_max;
    uint32        color;
    uint32        dx_color;
    uint8        *flag_cursor;
    int           i;

    rc = &data_fd2_battle_runtime_char_array_ptr[char_idx];

    hp_cur = (int32)(int16)rc->hp_current;
    hp_max = (int32)(int16)rc->hp_max;
    mp_cur = (int32)(int16)rc->mp_current;
    mp_max = (int32)(int16)rc->mp_max;

    /* HP / MP proportional bars */
    fd2_render_hp_or_mp_bar_proportional(overlay_buffer + 0x2a06, 0x140, 0x17,
                                         (uint32)hp_cur, (uint32)hp_max);
    fd2_render_hp_or_mp_bar_proportional(overlay_buffer + 0x41c6, 0x140, 0x1a,
                                         (uint32)mp_cur, (uint32)mp_max);

    /* HP / MP current+max numbers (red glow when current == max) */
    fd2_render_number_red_when_full(overlay_buffer + 0x344b, 0x140,
                                    (uint32)hp_cur, (uint32)hp_max, 3);
    fd2_render_number_red_when_full(overlay_buffer + 0x3465, 0x140,
                                    (uint32)hp_max, (uint32)hp_max, 3);
    fd2_render_number_red_when_full(overlay_buffer + 0x4acb, 0x140,
                                    (uint32)mp_cur, (uint32)mp_max, 3);
    fd2_render_number_red_when_full(overlay_buffer + 0x4ae5, 0x140,
                                    (uint32)mp_max, (uint32)mp_max, 3);

    /* level / movement / magic-resist (white, 2-digit) */
    fd2_render_decimal_number_to_buffer(overlay_buffer + 0x29dd, 0x140,
                                        rc->status_flags_block[0], 0x2a, 2);
    fd2_render_decimal_number_to_buffer(overlay_buffer + 0x379d, 0x140,
                                        rc->movement_order, 0x2a, 2);
    fd2_render_decimal_number_to_buffer(overlay_buffer + 0x455d, 0x140,
                                        rc->combat_aux_block[0x14], 0x2a, 2);

    /* AP (red if boosted) */
    color = (rc->status_flags_block[1] != 0) ? 0x77 : 0x2a;
    fd2_render_decimal_number_to_buffer(overlay_buffer + 0x545d, 0x140,
                                        (uint32)(int32)(int16)rc->ap, color, 3);

    /* DP (red if boosted) */
    color = (rc->status_flags_block[2] != 0) ? 0x77 : 0x2a;
    fd2_render_decimal_number_to_buffer(overlay_buffer + 0x635d, 0x140,
                                        (uint32)(int32)(int16)rc->dp, color, 3);

    /* DX base (always white) — word at ai_target_and_dx_block[1] */
    fd2_render_decimal_number_to_buffer(
        overlay_buffer + 0x4535, 0x140,
        (uint32)(int32)*(int16 *)(rc->ai_target_and_dx_block + 1), 0x2a, 3);

    /* DX current and evade share one color flag (status_flags_block[3]) */
    dx_color = (rc->status_flags_block[3] != 0) ? 0x77 : 0x2a;
    fd2_render_decimal_number_to_buffer(overlay_buffer + 0x5435, 0x140,
                                        (uint32)(int32)(int16)rc->dx_current,
                                        dx_color, 3);
    fd2_render_decimal_number_to_buffer(overlay_buffer + 0x6335, 0x140,
                                        (uint32)(int32)(int16)rc->stat4_current,
                                        dx_color, 3);

    /* text labels: name / archetype / job */
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                             (uint32)rc->char_id + 1, overlay_buffer + 0x10a3,
                             0x140, 0xcd, 0x4c, 0, 0, 0);
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                             (uint32)rc->archetype_flag + 0x8c,
                             overlay_buffer + 0x1113, 0x140, 0xcd, 0x4c,
                             0, 0, 0);
    fd2_display_dialog_scene(data_fd2_all_game_text_ptr,
                             (uint32)rc->job_id + 0x96, overlay_buffer + 0x113b,
                             0x140, 0xcd, 0x4c, 0, 0, 0);

    /* team flag icon: enemy (team 0) -> 0x36, player/npc -> 0x35 */
    fd2_blit_sheet_sprite_at_offset(overlay_buffer + 0x25e5, 0x140,
                                    data_fd2_ui_anim_sprite_sheet_ptr,
                                    (rc->team == 0) ? 0x36 : 0x35);

    /* up to three status-effect icons; raw-byte walk from struct offset 0x25 */
    flag_cursor = (uint8 *)rc + 0x25;
    for (i = 0; i < 3; i++) {
        if (flag_cursor[i] != 0) {
            fd2_blit_sheet_sprite_at_offset(
                overlay_buffer + 0x55c2 + i * 0x23, 0x140,
                data_fd2_ui_anim_sprite_sheet_ptr, (uint32)(i + 0x37));
        }
    }
}

/* ----------------------------------------------------------------
 * fd2_paint_status_panel_layer_left @ 0x182ad (2 callers)
 *
 * Copy the LEFT status panel (86 rows x 86 bytes) from src_buffer into
 * dst_workspace, applying horizontal-shift clipping so the panel can be
 * drawn part-way off the left screen edge during the slide animation.
 *
 * x_offset is the signed horizontal destination shift (in pixels).
 * When x_offset < 0 the panel is being pushed off the left edge: the
 * copy is narrowed (row_bytes shrinks by |x_offset|), the source is
 * advanced by |x_offset| (src_x_skip) so the visible part stays aligned,
 * and the destination x is clamped to 0.
 *
 * Per-row layout (stride 0x140 = 320 px/row):
 *   dst = dst_workspace + 0x8C0 (row 8 * 0x140 + 0xC0) + x_offset + row*0x140
 *   src = src_buffer    + 0x8C5 (+5 px vs dst x; sister _right uses its
 *                                own x) + src_x_skip + row*0x140
 *
 * Callers: fd2_open_char_status_screen, fd2_play_status_screen_outro_step
 * (the latter redraws the left panel each frame at the frame's x_offset).
 *
 * Cdecl, 3 stack params; void return. The binary's __CHK(0x20) stack-probe
 * prologue is compiler-generated and omitted here.
 * ---------------------------------------------------------------- */
void fd2_paint_status_panel_layer_left(uint32 x_offset, uint32 dst_workspace,
                                       uint32 src_buffer)
{
    uint32 row;
    uint32 src_x_skip;
    uint32 row_bytes;

    row_bytes = 0x56;
    src_x_skip = 0;
    if ((int32)x_offset < 0) {
        row_bytes = x_offset + 0x56;
        src_x_skip = -x_offset;
        x_offset = 0;
    }
    for (row = 0; (int32)row < 0x56; row = row + 1) {
        memmove((void *)(row * 0x140 + dst_workspace + 0x8c0 + x_offset),
                (void *)(src_buffer + 0x8c5 + src_x_skip + row * 0x140),
                row_bytes);
    }
}

/* ----------------------------------------------------------------
 * fd2_paint_status_panel_layer_right @ 0x18312 (2 callers)
 *
 * Copy the RIGHT status panel (86 rows x 223 bytes) from src_buffer
 * into dst_workspace, applying VERTICAL-shift clipping so the panel
 * can be drawn part-way off the top screen edge during the slide
 * animation. (Sister of fd2_paint_status_panel_layer_left, which
 * shifts HORIZONTALLY.)
 *
 * y_offset is the signed vertical destination shift (in rows). When
 * y_offset < 0 the panel is being pushed off the top edge: the copy
 * is shortened (row_count shrinks by |y_offset|), the source row index
 * is advanced by |y_offset| (src_y_skip) so the visible part stays
 * aligned, and the destination row base is clamped to 0.
 *
 * Per-row layout (stride 0x140 = 320 px/row):
 *   dst = dst_workspace + 0x5C  + (y_offset   + row) * 0x140
 *   src = src_buffer    + 0x91C + (src_y_skip + row) * 0x140
 * Each row copies 0xDF (223) bytes. Src offset 0x91C = row 7 * 0x140 +
 * 0x5C: the right panel lives 7 rows below the source buffer head.
 *
 * Callers: fd2_open_char_status_screen, fd2_play_status_screen_outro_step
 * (the latter redraws the right panel each frame at the frame's y_offset).
 *
 * Cdecl, 3 stack params; void return. The binary's __CHK(0x20) stack-probe
 * prologue is compiler-generated and omitted here.
 * ---------------------------------------------------------------- */
void fd2_paint_status_panel_layer_right(uint32 y_offset, uint32 dst_workspace,
                                        uint32 src_buffer)
{
    uint32 row;
    uint32 src_y_skip;
    uint32 row_count;

    row_count = 0x56;
    src_y_skip = 0;
    if ((int32)y_offset < 0) {
        row_count = y_offset + 0x56;
        src_y_skip = -y_offset;
        y_offset = 0;
    }
    for (row = 0; (int32)row < (int32)row_count; row = row + 1) {
        memmove((void *)(y_offset * 0x140 + dst_workspace + 0x5c + row * 0x140),
                (void *)(src_y_skip * 0x140 + src_buffer + 0x91c + row * 0x140),
                0xdf);
    }
}

/* ----------------------------------------------------------------
 * fd2_render_inventory_item_grid @ 0x184c0 (3 callers)
 *
 * Render the 8-slot inventory grid for runtime_char[char_idx] into
 * dst_buf, packing non-empty slots left-to-right / top-to-bottom into a
 * 4-column x 2-row grid (cell 0x96 wide x 0x16 tall). The slot whose raw
 * slot index equals highlight_slot gets a highlighted name border
 * (highlight_slot == -1 -> none highlighted).
 *
 * Each runtime_char inventory slot is 2 bytes: [0]=bSlot_flag [1]=bItem_id
 * (inventory_slots[2*slot] / [2*slot+1]). bSlot_flag bit7 = empty slot
 * (skipped), bit6 = item equipped (background sprite +3 variant).
 *
 * Per drawn slot (active_slot_count counts only drawn slots):
 *   col   = active_slot_count / 4   row = active_slot_count % 4
 *   col_x = col * 0x96 + 0x2A       row_y = row * 0x16
 *   item  = fd2_get_item_effect_entry(bItem_id)   (23-byte effect record)
 *
 * Background icon sprite by item[0] (type):
 *   < 0x15 -> 0x3B (weapon)   < 0x20 -> 0x3C (armor/shield)
 *   else   -> 0x3D (other);   + 3 if equipped (slot_flag & 0x40)
 *   blit at dst_buf + col_x - 0x1D + (row_y + 0x65) * 0x140
 *
 * Name label: page = bItem_id + 0xB5 into _all_game_text, at
 *   dst_buf + col_x + (row_y + 0x67) * 0x140, border 0xC9 if highlighted
 *   else 0xCD, glyph params (0x4C,0,0,0).
 *
 * Value label sprite + number (number at dst_buf + col_x + 0x5D +
 * (row_y + 0x6B)*0x140, label sprite at +0x44 of the same row):
 *   item[0] < 0x15            -> sprite 0x40, value = *(int16*)(item+1)
 *   item[0] < 0x20            -> sprite 0x41, value = *(int16*)(item+5)
 *   item[0]==0x20 & item[0xD]==5  -> sprite 0x42, value = *(int16*)(item+0xE)
 *   item[0]==0x20 & item[0xD]==0xB-> sprite 0x43, value = *(int16*)(item+0xE)
 *   else -> placeholder dot sprite 0x29 at the value-label position, no
 *           number, and this slot is NOT counted (active_slot_count
 *           unchanged) so the next item reuses the same grid cell.
 *
 * The function reads item record bytes by raw offset (item[0], item[0xD],
 * item+1/+5/+0xE), matching the binary exactly.
 *
 * Cdecl, 3 stack params; void return. The binary's __CHK(0x58) stack-probe
 * prologue is compiler-generated and omitted here.
 * ---------------------------------------------------------------- */
void fd2_render_inventory_item_grid(uint32 char_idx, int highlight_slot,
                                    uint32 dst_buf)
{
    runtime_char *rc;
    uint8        *slot;
    uint8         slot_flag;
    uint8        *item;
    uint8         item_type;
    uint32        item_id;
    uint32        active_slot_count;
    uint32        slot_iter;
    uint32        col_x;
    uint32        row_y;
    uint32        bg_sprite;
    uint32        border_sprite;
    uint32        value_blit_addr;
    uint32        label_blit_addr;
    int32         value;

    rc = &data_fd2_battle_runtime_char_array_ptr[char_idx];
    active_slot_count = 0;

    for (slot_iter = 0; (int32)slot_iter < 8; slot_iter = slot_iter + 1) {
        slot = &rc->inventory_slots[slot_iter * 2];
        slot_flag = slot[0];
        if ((slot_flag & 0x80) != 0) {
            continue;
        }

        col_x = (active_slot_count / 4) * 0x96 + 0x2a;
        row_y = (active_slot_count & 3) * 0x16;

        item_id = slot[1];
        item = fd2_get_item_effect_entry((int)item_id);
        item_type = item[0];

        /* background icon sprite by item type, +3 if equipped */
        if (item_type < 0x15) {
            bg_sprite = 0x3b;
        } else if (item_type < 0x20) {
            bg_sprite = 0x3c;
        } else {
            bg_sprite = 0x3d;
        }
        if ((slot_flag & 0x40) != 0) {
            bg_sprite = bg_sprite + 3;
        }
        fd2_blit_sheet_sprite_at_offset(
            dst_buf + col_x - 0x1d + (row_y + 0x65) * 0x140, 0x140,
            data_fd2_ui_anim_sprite_sheet_ptr, bg_sprite);

        /* item name label, highlighted border if this is the selected slot */
        border_sprite = ((int)slot_iter == highlight_slot) ? 0xc9 : 0xcd;
        fd2_display_dialog_scene(
            data_fd2_all_game_text_ptr, item_id + 0xb5,
            dst_buf + col_x + (row_y + 0x67) * 0x140, 0x140, border_sprite,
            0x4c, 0, 0, 0);

        value_blit_addr = dst_buf + col_x + 0x5d + (row_y + 0x6b) * 0x140;
        label_blit_addr = dst_buf + col_x + 0x44 + (row_y + 0x6b) * 0x140;

        if (item_type < 0x15) {
            fd2_blit_sheet_sprite_at_offset(label_blit_addr, 0x140,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x40);
            value = *(int16 *)(item + 1);
            fd2_render_decimal_number_to_buffer(value_blit_addr, 0x140,
                                                (uint32)value, 0x2a, 3);
        } else if (item_type < 0x20) {
            fd2_blit_sheet_sprite_at_offset(label_blit_addr, 0x140,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x41);
            value = *(int16 *)(item + 5);
            fd2_render_decimal_number_to_buffer(value_blit_addr, 0x140,
                                                (uint32)value, 0x2a, 3);
        } else if (item_type == 0x20 && item[0xd] == 0x05) {
            fd2_blit_sheet_sprite_at_offset(label_blit_addr, 0x140,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x42);
            value = *(int16 *)(item + 0xe);
            fd2_render_decimal_number_to_buffer(value_blit_addr, 0x140,
                                                (uint32)value, 0x2a, 3);
        } else if (item_type == 0x20 && item[0xd] == 0x0b) {
            fd2_blit_sheet_sprite_at_offset(label_blit_addr, 0x140,
                                            data_fd2_ui_anim_sprite_sheet_ptr,
                                            0x43);
            value = *(int16 *)(item + 0xe);
            fd2_render_decimal_number_to_buffer(value_blit_addr, 0x140,
                                                (uint32)value, 0x2a, 3);
        } else {
            /* unrecognized type: placeholder dot sprite, no number; this
             * slot still counts toward active_slot_count (the binary's
             * INC active_slot_count is reached on this path too). */
            fd2_blit_indexed_sprite_at_xy(label_blit_addr, 0x140,
                                          data_fd2_ui_anim_sprite_sheet_ptr,
                                          0x29);
        }

        active_slot_count = active_slot_count + 1;
    }
}

/* ----------------------------------------------------------------
 * fd2_render_number_red_when_full @ 0x1875d (3 callers)
 *
 * Thin wrapper around fd2_render_decimal_number_to_buffer that picks the
 * digit color from the equality of current and max: when current == max
 * the value is drawn with the red "full" glow (color 0x1F), otherwise it
 * is drawn white (color 0x2A). The chosen color is forwarded as the color
 * (sprite-base) argument; current is passed through as the value to draw.
 *
 * Used by the HP/MP digit renderers to flag a "full" stat (current value
 * has reached its maximum) with the red glow.
 *
 * Cdecl, 5 stack params; void return. The binary's __CHK(0x18) stack-probe
 * prologue is compiler-generated and omitted here.
 * ---------------------------------------------------------------- */
void fd2_render_number_red_when_full(uint32 dst_off, uint32 pitch,
                                     uint32 current, uint32 max, uint32 digits)
{
    uint32 color;

    color = (current == max) ? 0x1f : 0x2a;
    fd2_render_decimal_number_to_buffer(dst_off, pitch, current, color, digits);
}

/* ----------------------------------------------------------------
 * fd2_render_hp_or_mp_bar_proportional @ 0x18795 (2 callers)
 *
 * Compute a proportional fill-segment count from (current / max) and
 * dispatch to fd2_render_horizontal_bar_segments to paint the bar.
 *
 *   max == 0      -> return without drawing (div-by-zero guard; an absent
 *                    stat draws no bar at all).
 *   current == 0  -> segments = 0 (fully empty bar).
 *   else          -> segments = (current * 0x65) / max + 1.
 *
 * 0x65 (= 101) is the maximum fillable segment count (reached when
 * current == max). The +1 guarantees a minimum 1-segment sliver for any
 * non-zero current, so "1 HP left" still shows a visible bar. The binary
 * does a SIGNED multiply+divide (IMUL / SAR EDX,0x1F / IDIV), so the
 * arithmetic is performed with signed 32-bit operands here to match.
 *
 * Callers: fd2_render_full_char_stat_panel (HP sprite_base 0x17, MP 0x1A)
 * and fd2_render_mini_char_status_panel.
 *
 * Cdecl, 5 stack params; void return. The binary's __CHK(0x14) stack-probe
 * prologue is compiler-generated and omitted here.
 * ---------------------------------------------------------------- */
void fd2_render_hp_or_mp_bar_proportional(uint32 dst_off, uint32 pitch,
                                          uint32 sprite_base, uint32 current,
                                          uint32 max)
{
    uint32 segments;

    if (max == 0) {
        return;
    }
    if (current == 0) {
        segments = 0;
    } else {
        segments = (uint32)(((int32)current * 0x65) / (int32)max) + 1;
    }
    fd2_render_horizontal_bar_segments(dst_off, pitch, segments, sprite_base);
}
