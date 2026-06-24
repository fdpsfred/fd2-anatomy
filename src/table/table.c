/*
 * table.c — .object3 data table accessor functions
 *
 * 13 leaf functions that return pointers into the game's read-only
 * data tables in .object3. All are __cdecl, 1 stack arg, no side effects.
 *
 * Addresses: 0x4E48D..0x4E7F8 (.object1 tail cluster)
 */

#include "types.h"
#include "globals.h"

/* ----------------------------------------------------------------
 * fd2_get_item_effect_entry @ 0x4E56C  (23 callers)
 *
 * Returns pointer to item_effect_table[item_id].type (skips +0 prefix byte).
 * Assembly: EAX = item_id * 0x17 + 0x602AD
 * ---------------------------------------------------------------- */
uint8 *fd2_get_item_effect_entry(int item_id)
{
    return (uint8 *)&data_fd2_battle_item_effect_table[item_id].type;
}

/* ----------------------------------------------------------------
 * fd2_get_spell_effect_entry @ 0x4E516  (11 callers)
 *
 * Returns pointer to spell_effect_table[spell_id].
 * Assembly: EAX = spell_id * 7 + 0x619FD
 * ---------------------------------------------------------------- */
uint8 *fd2_get_spell_effect_entry(int spell_id)
{
    return (uint8 *)&data_fd2_battle_spell_effect_table[spell_id];
}

/* ----------------------------------------------------------------
 * fd2_get_enemy_data_entry @ 0x4E4FF  (4 callers)
 *
 * Returns pointer to the 10-byte enemy_data_table[idx] entry. idx is the
 * enemy-relative index = (portrait/class id - 0x44), range 0..0x43 (68 entries).
 * Entry fields: RA, CL, HP(uint16 @+2), MP, AP, DP, DX, MV(=magic_resist for
 * enemies), EX(exp reward @+9). On battle spawn HP/MP/AP/DP/DX = field * level.
 * Assembly: EAX = idx * 0xA + 0x61AF9
 * ---------------------------------------------------------------- */
uint8 *fd2_get_enemy_data_entry(int idx)
{
    return (uint8 *)&data_fd2_battle_enemy_data_table[idx];
}

/* ----------------------------------------------------------------
 * fd2_get_char_base_entry @ 0x4E4E8  (2 callers)
 *
 * Returns pointer to character_base_table[idx], the per-character starting-
 * attribute table. idx = char_id (0..0x1F; 32 entries, one per playable
 * character). The 24-byte entry holds base RA/CL/LV/HP/MP/MV, initial spell
 * bitmap + equipment/inventory item ids, and base AP/DP/DX. Callers combine
 * it with char_growth (scaled by level) to populate a runtime char slot.
 * Assembly: EAX = idx * 0x18 + 0x61DA1
 * ---------------------------------------------------------------- */
uint8 *fd2_get_char_base_entry(uint32 idx)
{
    return (uint8 *)&data_fd2_battle_character_base_table[idx];
}

/* ----------------------------------------------------------------
 * fd2_get_char_growth_entry @ 0x4E4D1  (4 callers)
 *
 * Returns pointer to character_growth_table[idx] (idx = character/portrait
 * id). The 11-byte entry holds per-stat growth deltas (AP/DP/DX/HP/MP, each
 * a min/max pair) plus a spell-learning index at +0x0A. Callers scale these
 * by level to derive runtime stats.
 * Assembly: EAX = idx * 0xB + 0x620A1
 * ---------------------------------------------------------------- */
uint8 *fd2_get_char_growth_entry(int idx)
{
    return (uint8 *)&data_fd2_battle_character_growth_table[idx];
}

/* ----------------------------------------------------------------
 * fd2_get_chapter_intro_metadata_entry @ 0x4E4B9  (7 callers)
 *
 * Returns pointer to chapter_intro_metadata for 1-based chapter_id.
 * Assembly: EAX = (chapter_id - 1) * 0x1F + 0x6238D
 * ---------------------------------------------------------------- */
uint8 *fd2_get_chapter_intro_metadata_entry(int chapter_id)
{
    return (uint8 *)(data_fd2_chapter_intro_metadata_table
         + (chapter_id - 1) * 0x1F);
}

/* ----------------------------------------------------------------
 * fd2_get_spell_learning_entry @ 0x4E4A2  (1 caller)
 *
 * Returns pointer to spell_learning_table[idx].
 * Assembly: EAX = idx * 0xC + 0x626B3
 * ---------------------------------------------------------------- */
uint8 *fd2_get_spell_learning_entry(int idx)
{
    return (uint8 *)(data_fd2_battle_spell_learning_table + idx * 0xC);
}

/* ----------------------------------------------------------------
 * fd2_get_class_promotion_data_entry @ 0x4E48D  (3 callers)
 *
 * Returns pointer to the 2-byte promotion entry for class_id >= 0x20.
 * Entry layout: byte[0] = post-promotion job_id, byte[1] = learned-spell
 * id (0 = none). Callers read byte[0] (new job) and byte[1] (spell unlock).
 * Assembly: EAX = (class_id - 0x20) << 1 + 0x615FE
 * ---------------------------------------------------------------- */
uint8 *fd2_get_class_promotion_data_entry(int class_id)
{
    return (uint8 *)(data_fd2_battle_class_promotion_data_table
         + (class_id - 0x20) * 2);
}

/* ----------------------------------------------------------------
 * fd2_get_attack_anim_pattern_for_weapon @ 0x4E52D  (1 caller)
 *
 * Dereferences pointer table: returns the pointer stored at
 * weapon_attack_anim_pattern_ptr_table[weapon_type].
 * Assembly: EBX = weapon_type << 2; EAX = [EBX + 0x61955]
 * ---------------------------------------------------------------- */
uint8 *fd2_get_attack_anim_pattern_for_weapon(int weapon_type)
{
    return (uint8 *)data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21[weapon_type];
}

/* ----------------------------------------------------------------
 * fd2_get_job_allowed_items_table_entry @ 0x4E53E  (1 caller)
 *
 * Returns pointer to 7-byte allowed-items list for job_id.
 * Assembly: EAX = job_id * 7 + 0x6188A
 * ---------------------------------------------------------------- */
uint8 *fd2_get_job_allowed_items_table_entry(int job_id)
{
    return (uint8 *)(data_fd2_battle_job_allowed_items_table + job_id * 7);
}

/* ----------------------------------------------------------------
 * fd2_get_movement_cost_table_for_job @ 0x4E555  (8 callers)
 *
 * Returns pointer to 20-byte per-tile-type movement cost array for job_id.
 * Assembly: EAX = job_id * 0x14 + 0x61646
 * ---------------------------------------------------------------- */
uint8 *fd2_get_movement_cost_table_for_job(int job_id)
{
    return (uint8 *)(data_fd2_battle_movement_cost_table + job_id * 0x14);
}

/* ----------------------------------------------------------------
 * fd2_get_cutscene_event_script @ 0x4E7F8  (1 caller)
 *
 * Dereferences pointer table: returns the pointer stored at
 * cutscene_event_script_ptr_table[event_id].
 * Assembly: EAX = [event_id * 4 + 0x627D8]
 * ---------------------------------------------------------------- */
uint8 *fd2_get_cutscene_event_script(int event_id)
{
    return (uint8 *)data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id];
}

/* ----------------------------------------------------------------
 * fd2_get_orphan_table_60181_entry @ 0x4DB84  (0 callers -- orphan)
 *
 * Returns pointer to idx-th 3-byte entry of the table at 0x60181.
 * Assembly: EAX = idx * 3 + 0x60181
 * Orphan accessor in .object3; reachable only via indirect call.
 * ---------------------------------------------------------------- */
uint8 *fd2_get_orphan_table_60181_entry(int idx)
{
    return (uint8 *)(data_fd2_orphan_table_60181 + idx * 3);
}
