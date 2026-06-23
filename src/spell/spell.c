/*
 * spell.c — Spell handler dispatch functions
 *
 * 24 handlers registered in spell dispatch table @ 0x51D01.
 * All are thin wrappers that delegate to worker functions with
 * hardcoded spell_id / effect_id parameters.
 *
 * Pattern families:
 *   id 0-3,8:     fd2_execute_offensive_targeted_spell (blink overlay)
 *   id 4-7:       fd2_execute_offensive_full_screen_flash_spell
 *   id 0xA-0xC:   fd2_cast_earthquake_spell_with_screen_shake
 *   id 0xD-0x10:  fd2_dispatch_variant_b_cast (heal/buff variant)
 *   id 0x11-0x13: stat boost wrappers (AP/DP/speed)
 *   id 0x14-0x15: fd2_apply_status_effect_with_anim
 *   id 0x16,0x1A-0x1B: fd2_cast_status_spell_via_d1b
 *   id 0x17:      complex teleport spell
 */

#include "types.h"
#include "consts.h"
#include "globals.h"
#include "protos.h"

/* === Targeted blink family (id 0-3, 8) === */

void fd2_spell_handler_id_0_via_targeted_blink(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_targeted_spell(caster, 0, n_tgt, (int)tgt_arr);
}

void fd2_spell_handler_id_1_via_targeted_blink(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_targeted_spell(caster, 1, n_tgt, (int)tgt_arr);
}

void fd2_spell_handler_id_2_via_targeted_blink(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_targeted_spell(caster, 2, n_tgt, (int)tgt_arr);
}

void fd2_spell_handler_id_3_via_targeted_blink(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_targeted_spell(caster, 3, n_tgt, (int)tgt_arr);
}

void fd2_spell_handler_id_8_via_targeted_blink(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_targeted_spell(caster, 8, n_tgt, (int)tgt_arr);
}

/* === Full screen flash family (id 4-7) === */

void fd2_spell_handler_id_4_via_full_screen_flash(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_full_screen_flash_spell(
        caster, 4, n_tgt, (int)tgt_arr);
}

void fd2_spell_handler_id_5_via_full_screen_flash(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_full_screen_flash_spell(
        caster, 5, n_tgt, (int)tgt_arr);
}

void fd2_spell_handler_id_6_via_full_screen_flash(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_full_screen_flash_spell(
        caster, 6, n_tgt, (int)tgt_arr);
}

void fd2_spell_handler_id_7_via_full_screen_flash(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_execute_offensive_full_screen_flash_spell(
        caster, 7, n_tgt, (int)tgt_arr);
}

/* === Earthquake family (id 0xA-0xC) === */

void fd2_cast_spell_0a_basic(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_cast_earthquake_spell_with_screen_shake(
        caster, 0xa, n_tgt, tgt_arr);
}

/* spell_id 0xB: like 0xA but adds a pre-effect before the earthquake worker --
 * generic cast SFX (id 2) + rising pre-cast effect (initial=0xF, step=0xA). @ 0x2185F */
void fd2_cast_spell_0b_with_prefx(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 2, 1);
    fd2_play_rising_pre_cast_effect(caster, 0xf, 10);
    fd2_cast_earthquake_spell_with_screen_shake(
        caster, 0xb, n_tgt, tgt_arr);
}

/* spell_id 0xC: like 0xB but adds a pre-effect before the earthquake worker --
 * generic cast SFX (id 2) + rising pre-cast effect (initial=0x1E, step=0x10,
 * faster/higher than 0xB's {0xF, 0xA}). @ 0x21A9E */
void fd2_cast_spell_0c_with_prefx(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 2, 1);
    fd2_play_rising_pre_cast_effect(caster, 0x1e, 0x10);
    fd2_cast_earthquake_spell_with_screen_shake(
        caster, 0xc, n_tgt, tgt_arr);
}

/* === Variant-B heal/buff family (id 0xD-0x10) ===
 * All four play status-effect SFX (id 0xB) + a slide pre-cast effect, then
 * dispatch via fd2_dispatch_variant_b_cast (heal-style worker that applies
 * fd2_apply_heal_spell_to_target, not damage calc). They differ only by their
 * slide pre-effect params and spell_id literal. */

/* spell_id 0xD (heal/cure): status SFX (id 0xB) + slide pre-effect (1, 2),
 * then the variant-B heal dispatch. @ 0x21AD9 */
void fd2_cast_spell_0d_variant_b(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0xb, 1);
    fd2_play_variant_b_slide_pre_effect(1, 2);
    fd2_dispatch_variant_b_cast(caster, 0xd, n_tgt, (int)tgt_arr);
}

/* spell_id 0xE: status SFX (id 0xB) + slide pre-effect (2, 4) (stronger than
 * 0xD's {1, 2}), then the variant-B heal dispatch. @ 0x21B99 */
void fd2_cast_spell_0e_variant_b(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0xb, 1);
    fd2_play_variant_b_slide_pre_effect(2, 4);
    fd2_dispatch_variant_b_cast(caster, 0xe, n_tgt, (int)tgt_arr);
}

/* spell_id 0xF: status SFX (id 0xB) + slide pre-effect (8, 4), then the
 * variant-B heal dispatch. @ 0x2211C */
void fd2_cast_spell_0f_variant_b(
    uint32 caster, uint32 n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0xb, 1);
    fd2_play_variant_b_slide_pre_effect(8, 4);
    fd2_dispatch_variant_b_cast(caster, 0xf, n_tgt, (int)tgt_arr);
}

void fd2_cast_spell_10_variant_b(
    uint32 caster, uint32 n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0xb, 1);
    fd2_play_variant_b_slide_pre_effect(6, 6);
    fd2_dispatch_variant_b_cast(caster, 0x10, n_tgt, (int)tgt_arr);
}

/* === Stat boost wrappers (id 0x11-0x13) === */

void fd2_cast_spell_11_stage_a(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster, 0x12);
    fd2_cast_ap_boost_spell(caster, n_tgt, tgt_arr);
}

void fd2_cast_spell_12_stage_b(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster, 0x12);
    fd2_cast_dp_boost_spell(caster, n_tgt, (uint32)tgt_arr);
}

void fd2_cast_spell_13_stage_c(
    uint32 caster, uint32 n_tgt, uint8 *tgt_arr)
{
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster, 0x13);
    fd2_cast_speed_boost_spell(caster, n_tgt, (uint32)tgt_arr);
}

/* === Status effect family (id 0x14-0x15) === */

void fd2_cast_spell_14_dispatch_aa8(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_apply_status_effect_with_anim(
        caster, 0x14, n_tgt, (int)tgt_arr, 0x25);
}

void fd2_cast_spell_15_dispatch_aa8(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_apply_status_effect_with_anim(
        caster, 0x15, n_tgt, (int)tgt_arr, 0x26);
}

/* === Status d1b family (id 0x16, 0x1A, 0x1B) === */

void fd2_cast_spell_16_dispatch_cda(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_cast_status_spell_via_d1b(
        caster, 0x16, n_tgt, (int)tgt_arr, 0x27);
}

void fd2_spell_handler_id_26_via_status_d1b_effect_25(
    int caster, int n_tgt, int tgt_arr)
{
    fd2_cast_status_spell_via_d1b(
        caster, 0x1a, n_tgt, tgt_arr, 0x25);
}

void fd2_spell_handler_id_27_via_status_d1b_effect_26(
    int caster, int n_tgt, int tgt_arr)
{
    fd2_cast_status_spell_via_d1b(
        caster, 0x1b, n_tgt, tgt_arr, 0x26);
}

