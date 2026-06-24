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
 *   id 0xD-0x10:  fd2_execute_variant_b_heal_cast (heal/buff variant)
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
 * dispatch via fd2_execute_variant_b_heal_cast (heal-style worker that applies
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
    fd2_execute_variant_b_heal_cast(caster, 0xd, n_tgt, (int)tgt_arr);
}

/* spell_id 0xE: status SFX (id 0xB) + slide pre-effect (2, 4) (stronger than
 * 0xD's {1, 2}), then the variant-B heal dispatch. @ 0x21B99 */
void fd2_cast_spell_0e_variant_b(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0xb, 1);
    fd2_play_variant_b_slide_pre_effect(2, 4);
    fd2_execute_variant_b_heal_cast(caster, 0xe, n_tgt, (int)tgt_arr);
}

/* spell_id 0xF: status SFX (id 0xB) + slide pre-effect (8, 4), then the
 * variant-B heal dispatch. @ 0x2211C */
void fd2_cast_spell_0f_variant_b(
    uint32 caster, uint32 n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0xb, 1);
    fd2_play_variant_b_slide_pre_effect(8, 4);
    fd2_execute_variant_b_heal_cast(caster, 0xf, n_tgt, (int)tgt_arr);
}

/* spell_id 0x10: status SFX (id 0xB) + slide pre-effect (6, 6), then the
 * variant-B heal dispatch. @ 0x22153. The dispatch table @ 0x51D01 maps this
 * handler to TWO slots -- entry 0x10 (primary) and entry 0x18 (duplicate, the
 * 淒煌斬 special-attack slot, which is normally driven by
 * fd2_execute_special_attack_skill, so 0x18 here is a fallback/placeholder). */
void fd2_cast_spell_10_variant_b(
    uint32 caster, uint32 n_tgt, uint8 *tgt_arr)
{
    fd2_play_sfx_with_handle(
        data_fd2_audio_status_effect_sfx_handle_ptr, 0xb, 1);
    fd2_play_variant_b_slide_pre_effect(6, 6);
    fd2_execute_variant_b_heal_cast(caster, 0x10, n_tgt, (int)tgt_arr);
}

/* === Stat boost wrappers (id 0x11-0x13) === */

/* spell_id 0x11 (AP boost): reset the AoE target counter, deduct caster MP using
 * cost-table index 0x12 (not 0x11 -- intentional cost-table mapping, matching the
 * 0x12 stage_b wrapper), then delegate to the AP-boost worker. @ 0x226EA */
void fd2_cast_spell_11_ap_boost(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster, 0x12);
    fd2_cast_ap_boost_spell(caster, n_tgt, tgt_arr);
}

/* spell_id 0x12 (DP/defense boost, 魔鎧術): reset the AoE target counter, deduct
 * caster MP using cost-table index 0x12, then delegate to the DP-boost worker.
 * @ 0x2282F. Mirrors the 0x11 AP-boost wrapper; the DP-boost worker is shared
 * with the item-use path and the 破壞神 summon spell. */
void fd2_cast_spell_12_dp_boost(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster, 0x12);
    fd2_cast_dp_boost_spell(caster, n_tgt, (uint32)tgt_arr);
}

/* spell_id 0x13 (speed boost, 風行術): reset the AoE target counter, deduct
 * caster MP using cost-table index 0x13, then delegate to the speed-boost
 * worker. @ 0x22960. Sibling of the 0x11 AP-boost / 0x12 DP-boost wrappers --
 * these are three distinct buff spells, not three stages of one. Unlike the
 * 0x11/0x12 wrappers (which both charge MP via index 0x12), this one charges
 * via its own spell id 0x13. */
void fd2_cast_spell_13_speed_boost(
    uint32 caster, uint32 n_tgt, uint8 *tgt_arr)
{
    data_fd2_battle_spell_aoe_count_and_fx_queue_idx = 0;
    fd2_deduct_caster_mp(caster, 0x13);
    fd2_cast_speed_boost_spell(caster, n_tgt, (uint32)tgt_arr);
}

/* === Status effect family (id 0x14-0x15) === */

/* spell_id 0x14 (解毒術, cure-poison): dispatch-table entry @ 0x22A85
 * (table[0x14] @ 0x51D51). Forwards to the shared status-cure worker with
 * effect animation/sprite id 0x25. Shares its {push caster + call + cleanup}
 * tail with the 0x15 sibling: that sibling jumps into this body at 0x22A9B
 * after pushing its own sprite id 0x26 / spell id 0x15. */
void fd2_cast_spell_14_dispatch(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_apply_status_effect_with_anim(
        caster, 0x14, n_tgt, (int)tgt_arr, 0x25);
}

/* spell_id 0x15 (祛麻術, cure-paralysis): dispatch-table entry @ 0x22BC6
 * (table[0x15] @ 0x51D55). Mirrors the 0x14 sibling, forwarding to the same
 * shared status-cure worker but with effect animation/sprite id 0x26 (vs 0x14's
 * 0x25). In the original binary this entry tail-jumps into the 0x14 body to
 * reuse its {push caster + call worker + cleanup} tail. */
void fd2_cast_spell_15_dispatch(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_apply_status_effect_with_anim(
        caster, 0x15, n_tgt, (int)tgt_arr, 0x26);
}

/* === Status d1b family (id 0x16, 0x1A, 0x1B) === */

/* spell_id 0x16 (封咒術, seal): dispatch-table entry @ 0x22BE1
 * (table[0x16] @ 0x51D59). Forwards to the shared status-inflict worker
 * fd2_cast_status_spell_via_d1b with effect/sprite id 0x27. In the original
 * binary this body also hosts the {push caster + call worker + cleanup} shared
 * tail (@ 0x22BF7) that the 0x1A and 0x1B siblings jump into after pushing
 * their own spell/effect ids. Distinct from the 0x14/0x15 status-cure family,
 * which uses a different worker (0x22AA8). */
void fd2_cast_spell_16_dispatch(
    int caster, int n_tgt, uint8 *tgt_arr)
{
    fd2_cast_status_spell_via_d1b(
        caster, 0x16, n_tgt, (int)tgt_arr, 0x27);
}

/* spell_id 0x1A (毒擊術, poison-strike): dispatch-table entry @ 0x22CBF
 * (table[0x1A] @ 0x51D69). Same family as the 0x16 entry above -- forwards to
 * the shared status-inflict worker fd2_cast_status_spell_via_d1b, here with
 * effect/sprite id 0x25. In the original binary this entry tail-jumps into the
 * 0x16 body (@ 0x22BF7) to reuse its {push caster + call worker + cleanup}
 * tail. Sibling: the 0x1B entry below (spell 0x1B with effect 0x26). */
void fd2_cast_spell_1a_dispatch(
    int caster, int n_tgt, int tgt_arr)
{
    fd2_cast_status_spell_via_d1b(
        caster, 0x1a, n_tgt, tgt_arr, 0x25);
}

/* spell_id 0x1B (麻痹術, paralysis): dispatch-table entry @ 0x22E41
 * (table[0x1B] @ 0x51D6D). Same family as the 0x16/0x1A entries above --
 * forwards to the shared status-inflict worker fd2_cast_status_spell_via_d1b,
 * here with effect/sprite id 0x26. In the original binary this entry
 * tail-jumps into the 0x16 body (@ 0x22BF7) to reuse its {push caster + call
 * worker + cleanup} tail. Sibling of the 0x1A entry above (spell 0x1A with
 * effect 0x25). */
void fd2_cast_spell_1b_dispatch(
    int caster, int n_tgt, int tgt_arr)
{
    fd2_cast_status_spell_via_d1b(
        caster, 0x1b, n_tgt, tgt_arr, 0x26);
}

