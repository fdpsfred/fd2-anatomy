"""
mkroute.py — FD2 emit routing manager

src/routing.json is the single source of truth for:
  - function → target .c file routing
  - function → phase assignment
  - done / asm review status

JSON schema: { "<address>": { "name", "target", "phase", "done", "asm" } }

Commands:
  generate   — Create routing.json from emit_functions.json + routing rules
               (preserves done/asm/target from existing routing.json if present)
  resplit    — Re-apply the sub-file split to existing routing.json targets
               (idempotent; only `target` changes)
  status     — Print per-phase and per-target progress summary
  mark <addr> done|asm  — Set done=true or asm=true for a function
  pending [--phase N]   — List pending (not done) functions, optionally filtered by phase
  validate   — Check consistency (0 UNROUTED, 650 total)
"""

import re
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
EMIT_FUNCS = ROOT / "workspace" / "emit" / "emit_functions.json"
ROUTING_JSON = ROOT / "src" / "routing.json"
ROUTING_MD = ROOT / "src" / "routing.md"

PHASE_NAMES = {
    1: "Table / Input / Battle Core / Spell Handlers / Palette",
    2: "Battle AI / Animation Tick / Lifecycle",
    3: "Graphics Blit + Render + CRT",
    4: "Animation Play / Dialog / Spell Workers",
    5: "Menu / Save-Load / Misc Util",
    6: "Chapter Init / End / Post-Action",
    7: "Chapter Event Handlers",
}

DONE_INITIAL = {
    "fd2_get_item_effect_entry","fd2_get_spell_effect_entry","fd2_get_enemy_data_entry",
    "fd2_get_char_base_entry","fd2_get_char_growth_entry","fd2_get_chapter_intro_metadata_entry",
    "fd2_get_spell_learning_entry","fd2_get_class_promotion_data_entry",
    "fd2_get_attack_anim_pattern_for_weapon","fd2_get_job_allowed_items_table_entry",
    "fd2_get_movement_cost_table_for_job","fd2_get_cutscene_event_script",
    "fd2_advance_rng_state","fd2_deduct_caster_mp","fd2_apply_hp_heal_and_award_xp",
    "fd2_apply_heal_spell_to_target","fd2_apply_damage_and_award_xp","fd2_calc_magic_damage",
    "fd2_recompute_runtime_char_total_stats","fd2_recalculate_combat_stats",
    "fd2_check_can_counter_attack","fd2_check_can_default_attack_target",
    "fd2_execute_attack_damage_calculation",
    "fd2_spell_handler_id_0_via_targeted_blink","fd2_spell_handler_id_1_via_targeted_blink",
    "fd2_spell_handler_id_2_via_targeted_blink","fd2_spell_handler_id_3_via_targeted_blink",
    "fd2_spell_handler_id_4_via_full_screen_flash","fd2_spell_handler_id_5_via_full_screen_flash",
    "fd2_spell_handler_id_6_via_full_screen_flash","fd2_spell_handler_id_7_via_full_screen_flash",
    "fd2_spell_handler_id_8_via_targeted_blink",
    "fd2_cast_spell_0a_basic","fd2_cast_spell_0b_with_prefx","fd2_cast_spell_0c_with_prefx",
    "fd2_cast_spell_0d_variant_b","fd2_cast_spell_0e_variant_b","fd2_cast_spell_0f_variant_b",
    "fd2_cast_spell_10_variant_b","fd2_cast_spell_11_stage_a","fd2_cast_spell_12_stage_b",
    "fd2_cast_spell_13_stage_c","fd2_cast_spell_14_dispatch_aa8","fd2_cast_spell_15_dispatch_aa8",
    "fd2_cast_spell_16_dispatch_cda","fd2_spell_handler_id_26_via_status_d1b_effect_25",
    "fd2_spell_handler_id_27_via_status_d1b_effect_26","fd2_cast_spell_17_complex",
    "fd2_check_keyboard_buffer_nonempty","fd2_clear_keyboard_buffer","fd2_read_bios_midnight_tick",
    "fd2_wait_one_bios_tick","fd2_wait_n_bios_ticks",
    "fd2_cursor_move_up","fd2_cursor_move_down","fd2_cursor_move_right","fd2_cursor_move_left",
    "fd2_pan_cursor_to_tile_animated","fd2_pan_cursor_to_char","fd2_pan_cursor_and_window",
}


# ── Subcategory → Phase assignment (from mkwklist.py) ──

def _subcat(n):
    if n.startswith("crt_"):                                          return "crt_eq"
    if "_chapter_event_handler_" in n:                                return "ch_event"
    if re.match(r"fd2_chapter_\d", n):                                return "ch_init_end"
    if n.startswith("fd2_chapter_"):                                   return "ch_other"
    if re.match(r"fd2_(spell_handler_|cast_spell_)", n):              return "spell_hdlr"
    if re.match(r"fd2_(blit_|tile_blit_|rle_|dialog_sprite|decode_dialog)", n): return "gfx_blit"
    if re.match(r"fd2_(render_|paint_|composite_|fill_)", n):         return "gfx_render"
    if re.match(r"fd2_(set_vga|set_full_vga|update_palette|interpolate_|palette_fade|apply_palette|flash_)", n): return "gfx_palette"
    if re.match(r"fd2_(play_|animate_|cycle_|step_figani|scatter_|render_circle|render_filled|render_summon)", n): return "anim_play"
    if re.match(r"fd2_(ani_decoder|tick_|slide_)", n):                return "anim_tick"
    if re.match(r"fd2_(cursor_|pan_|check_keyboard|clear_keyboard|wait_|read_bios|wrapper_clear|read_tile|pause_)", n): return "input"
    if re.match(r"fd2_get_", n):                                      return "table_acc"
    if re.match(r"fd2_(ai_|enemy_turn|npc_turn|attack_action|score_|scan_|compute_aoe|execute_ai_|face_|check_char_status|check_all|check_battle|check_tile|init_battle|init_movement|flood_|pathfind_|walk_|mark_|collect_|tally_|find_tile|find_char)", n): return "battle_ai"
    if re.match(r"fd2_(advance_rng|deduct_|apply_|calc_|recalculate_|recompute_|check_can_|execute_attack|calculate_|compute_combat|compute_equipped)", n): return "battle_core"
    if re.match(r"fd2_(menu_|run_|open_|close_|field_|game_|player_|settings_|shop_|equip_|inventory_|item_|build_|promote_|party_|count_|recruitment|reorder_|pin_|require_)", n): return "menu_ui"
    if re.match(r"fd2_(display_|assemble_|show_|text_dialog|portrait_|cinematic_|wrap_|setup_chars|delay_400)", n): return "dialog_cin"
    if re.match(r"fd2_(save_|load_|backup_|restore_)", n):            return "save_load"
    if re.match(r"fd2_(dpmi_|noop_|main|set_bgm|play_sfx|debug_|set_word|set_chapter|set_combat|set_battle|set_runtime)", n): return "lifecycle"
    if re.match(r"fd2_(cast_|execute_offensive|execute_aoe|execute_status|execute_combat|execute_special|execute_summon|dispatch_variant|draw_spell|spell_selection|spell_select|grant_spell)", n):
        return "spell_worker"
    return "misc_util"

PHASE_MAP = {
    "table_acc": 1, "input": 1, "battle_core": 1, "spell_hdlr": 1, "gfx_palette": 1,
    "battle_ai": 2, "anim_tick": 2, "lifecycle": 2,
    "crt_eq": 3, "gfx_blit": 3, "gfx_render": 3,
    "anim_play": 4, "dialog_cin": 4, "spell_worker": 4,
    "misc_util": 5, "save_load": 5, "menu_ui": 5,
    "ch_init_end": 6, "ch_other": 6,
    "ch_event": 7,
}

def assign_phase(name):
    return PHASE_MAP.get(_subcat(name), 5)


# ── Routing target assignment ──

def parse_routing_md_targets(path):
    """Parse explicit per-row routing from routing.md relocated-helpers table."""
    targets = {}
    row_re = re.compile(
        r"\|\s*`([^`]+)`\s*\|"       # | `function_name` |
        r"\s*`([0-9a-fA-F]+)`\s*\|"  # | `address` |
        r"\s*([^\|]+?)\s*\|"         # | target |
    )
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = row_re.search(line)
            if m:
                addr = m.group(2).lower()
                target = m.group(3).strip()
                if "/" in target and ".c" in target:
                    targets[addr] = target
    return targets

# Explicit overrides for functions where name-based routing is wrong
EXPLICIT_TARGETS = {
    "fd2_wait_for_action_target_input": "input/input.c",
    "fd2_wait_for_input_with_idle": "input/input.c",
    "fd2_wait_for_input_v2": "input/input.c",
    "fd2_wait_for_input_dialog_with_blink": "input/input.c",
    "fd2_wait_input_with_dialog_repaint": "input/input.c",
    "fd2_wrapper_clear_keyboard_buffer": "input/input.c",
    "fd2_wait_input_with_status_panel_repaint": "input/input.c",
    "fd2_wait_ticks_or_keypress_with_palette": "input/input.c",
    "fd2_wait_input_with_chapter_dialog_blink": "input/input.c",
    "fd2_wait_input_with_recruitment_repaint": "input/input.c",
    "fd2_read_tile_attribute_at_pos": "battle/battle.c",
    "fd2_get_inventory_slot_item_id": "battle/battle.c",
    "fd2_apply_mp_heal_and_award_xp": "battle/battle.c",
    "fd2_compute_combat_bubble_screen_pos": "battle/battle.c",
    "fd2_calculate_combat_hit_outcome": "battle/battle.c",
    "fd2_flash_char_hit_sprite": "battle/battle.c",
    "fd2_find_equipped_item_by_kind": "battle/battle.c",
    "fd2_check_char_is_dead": "battle/battle.c",
    "fd2_set_runtime_char_evade": "battle/battle.c",
    "fd2_face_char_toward_target": "battle/battle.c",
    "fd2_check_char_status_immunity": "battle/battle.c",
    "fd2_compute_equipped_stats_with_item_preview": "ui_menu/status.c",
    "fd2_apply_use_effect_dispatch": "spell/spellwk.c",
    "fd2_apply_item_stat_modifier_with_anim": "spell/spellwk.c",
    "fd2_apply_attack_spell_damage": "spell/spellwk.c",
    "fd2_apply_status_effect_with_anim": "spell/spellwk.c",
    "fd2_game_main_loop": "ui_menu/menu.c",
    "fd2_load_save_and_init_engine": "life/main.c",
    "fd2_chapter_intro_menu_input_loop": "ui_menu/shop.c",
    "fd2_show_chapter_intro_text_dialog_mode_3": "field/chevt.c",
    "fd2_show_chapter_dialog_with_portrait_set_1": "field/chevt.c",
    "fd2_wrap_cinematic_chapter_portrait_dump_with_white_flash": "field/chevt.c",
    "fd2_cinematic_chapter_portrait_dump_with_white_flash": "field/chevt.c",
    "fd2_delay_400ms_via_idle_thunk": "util/misc.c",
    "fd2_obfuscate_battle_tile_map": "save/save.c",
    "fd2_resolve_terrain_for_aoe_targets": "battle/btl_ai.c",
    # AoE tile-marker primitive: name "set_tile_overlay" wrongly matched the
    # btl_turn rule, but its only caller is fd2_mark_aoe_plus_pattern_at (AI
    # targeting). Belongs with the AI targeting group (-> btl_aitg via subsplit).
    "fd2_set_tile_overlay_bit_80": "battle/btl_ai.c",
    "fd2_cutscene_event_trigger": "field/chtrans.c",
    "fd2_setup_chars_and_camera_for_intro": "field/chtrans.c",
    "fd2_chapter_transition_menu": "field/chtrans.c",
    "fd2_chapter_transition_with_intro": "field/chtrans.c",
}

def _base_target(name, phase):
    """Assign base routing target (pre subsplit) for a function."""
    if name in EXPLICIT_TARGETS:
        return EXPLICIT_TARGETS[name]

    # Phase 6 chapter handler patterns
    if phase == 6:
        if "_init" in name: return "field/chinit.c"
        if "_end" in name: return "field/chend.c"
        if "_post_action" in name: return "field/chpost.c"
    # Phase 7 event handlers
    if phase == 7:
        return "field/chevt.c"

    # Name-based routing
    sc = _subcat(name)
    NAME_TO_TARGET = {
        "table_acc": "table/table.c",
        "input": "input/input.c",
        "battle_core": "battle/battle.c",
        "battle_ai": "battle/btl_ai.c",
        "spell_hdlr": "spell/spell.c",
        "spell_worker": "spell/spellwk.c",
        "gfx_blit": "gfx/blit.c",
        "gfx_render": "gfx/render.c",
        "gfx_palette": "gfx/palette.c",
        "anim_play": "anim/anim.c",
        "anim_tick": "anim/anim.c",
        "menu_ui": "ui_menu/menu.c",
        "dialog_cin": "dialog/dialog.c",
        "save_load": "save/save.c",
        "crt_eq": "crt/crt.c",
        "ch_event": "field/chevt.c",
        "ch_init_end": "field/chinit.c",
        "ch_other": "field/chtrans.c",
    }
    if sc in NAME_TO_TARGET:
        return NAME_TO_TARGET[sc]

    # Lifecycle subcategory → split by specific prefixes
    if name.startswith("fd2_main"): return "life/main.c"
    if name.startswith("fd2_set_bgm") or name.startswith("fd2_play_sfx") or name.startswith("fd2_load_figani_sfx") or name.startswith("fd2_load_status_effect_sfx") or name.startswith("fd2_play_and_free"): return "audio/audio.c"
    if name.startswith("fd2_dpmi_"): return "util/dpmi.c"
    if name.startswith("fd2_noop_"): return "util/noop.c"
    if name.startswith("fd2_debug_") or name.startswith("fd2_set_word_"): return "util/misc.c"
    if name.startswith("fd2_set_chapter_") or name.startswith("fd2_set_combat_") or name.startswith("fd2_set_battle_") or name.startswith("fd2_set_runtime"): return "battle/btl_turn.c"
    if name.startswith("fd2_init_runtime") or name.startswith("fd2_init_battle"): return "battle/btl_turn.c"
    if name.startswith("fd2_load_") or name.startswith("fd2_restore_portrait"): return "rsrc/rsrc.c"

    # Broad menu_ui patterns
    if any(name.startswith(f"fd2_{p}") for p in [
        "field_", "game_", "player_", "settings_", "count_active_menu",
        "handle_tile_event", "maybe_load_speed", "maybe_free_speed",
        "open_tactical", "repaint_settings",
    ]): return "ui_menu/menu.c"
    if any(name.startswith(f"fd2_{p}") for p in [
        "open_char_status", "open_status", "close_status", "open_party_status",
        "equip_", "inventory_", "item_command", "add_item", "remove_inventory",
        "count_usable", "give_item", "check_job_can", "find_inventory",
        "run_status_screen",
    ]): return "ui_menu/status.c"
    if any(name.startswith(f"fd2_{p}") for p in [
        "shop_", "run_buy", "run_sell", "run_equip_member", "run_give_item",
        "run_chapter_intro", "party_roster", "promote_", "run_revive",
        "run_class_promotion", "execute_class_promotion", "build_dead",
        "build_promotion", "run_recruitment", "pick_stat",
    ]): return "ui_menu/shop.c"

    # Battle turn lifecycle
    if any(name.startswith(f"fd2_{p}") for p in [
        "clear_all_chars", "convert_battle",
        "run_full_turn", "fire_chapter_turn", "process_battle_drop",
        "process_xp", "roll_stat_gain", "restore_all_chars",
        "kill_runtime", "count_active_chars", "find_template",
        "mark_char_acted", "mark_char_as_dead", "check_all_player",
        "check_tile_event", "check_battle_end", "collect_dead",
        "collect_pending", "tick_status_effects", "find_char",
    ]): return "battle/btl_turn.c"

    # Pathfinding
    if name.startswith("fd2_pathfind_") or name.startswith("fd2_flood_fill") or name.startswith("fd2_init_movement"): return "util/pathfnd.c"

    # Walk animation
    if name.startswith("fd2_walk_"): return "anim/anim.c"

    # Party utility
    if any(name.startswith(f"fd2_{p}") for p in [
        "any_char_has", "check_party_has", "require_char_id",
        "count_selected", "reorder_party", "pin_required",
    ]): return "util/misc.c"

    # Save
    if name.startswith("fd2_save_"): return "save/save.c"

    # Cursor
    if name.startswith("fd2_cursor_") or name.startswith("fd2_pan_"): return "ui_menu/cursor.c"

    # Screen block
    if any(name.startswith(f"fd2_{p}") for p in [
        "save_screen_block", "restore_screen_block", "save_block_loop",
        "restore_block_loop", "scroll_buffer_block", "alloc_and_blit",
    ]): return "gfx/blit.c"

    return "UNROUTED"


# ── Sub-file split (keep each .c <= ~1000 emitted lines) ──
# Oversized base targets are split by cohesive sub-feature into 8.3 sub-files.
# chevt/chend split by handler-index / chapter-number; the rest by name pattern.
# Mirrors workspace/file_split/split_design.md (the reviewed design).

def _any(name, subs):
    return any(s in name for s in subs)

def _subsplit(name, target):
    if target == "anim/anim.c":
        if _any(name, ["walk_", "slide_panel", "play_status_screen_outro", "tick_tile_event"]): return "anim/aniwalk.c"
        if "tick_summon" in name or "tick_sprite_animation_step" in name: return "anim/anisummn.c"
        if _any(name, ["full_combat_cinematic", "execute_combat_hit_cinematic", "char_intro_zoom",
                       "figani_char_intro", "figani_animation_loop", "step_figani_pose",
                       "animate_spell_hit_cinematic", "display_cinematic_image"]): return "anim/anicine.c"
        if _any(name, ["spell_cast_cinematic", "spell_cast_sequence", "cycle_sprite_anim_with_bg",
                       "bg_zoom_transition", "play_ani_file_animation_sequence"]): return "anim/anispell.c"
        if _any(name, ["ending", "final_chapter_30", "chapter_clear_fanfare",
                       "chapter_intro_sprite_slideshow"]): return "anim/aniend.c"
        if _any(name, ["money", "tutorial", "scroll_up_in_shop", "scroll_down_in_shop",
                       "shop_transaction", "party_addition", "warp_char_to_tile",
                       "palette_flash_pulse", "screen_shake"]): return "anim/aniui.c"
        return "anim/anicombt.c"
    if target == "gfx/render.c":
        if _any(name, ["status_screen_static", "full_char_stat_panel", "status_panel_layer",
                       "inventory_item_grid", "number_red_when_full", "hp_or_mp_bar_proportional",
                       "decimal_number_to_buffer", "mini_char_status_panel", "terrain_info_hud",
                       "signed_modifier_with_icon", "party_status_overview", "chapter_status_panel",
                       "horizontal_bar_segments", "paint_portrait_to_dialog"]): return "gfx/rndstat.c"
        if _any(name, ["chapter_intro_overlay", "chapter_intro_dialog_panels", "shop_item_grid",
                       "party_roster_grid", "party_roster_with_item_stat", "save_slot_grid",
                       "promote_members_grid", "promote_candidates_grid",
                       "recruitment_select_screen", "battle_scene_with_portrait_grid"]): return "gfx/rndmenu.c"
        return "gfx/rndscene.c"
    if target == "spell/spellwk.c":
        if _any(name, ["build_usable_spell_list", "draw_spell_selection_list",
                       "spell_selection_menu_main", "spell_select_input_loop",
                       "play_spell_palette_flash", "grant_spell_to_char"]): return "spell/spellsel.c"
        if _any(name, ["earthquake", "rising_pre_cast", "dispatch_variant_b_cast",
                       "execute_aoe_spell_with_caster_portrait", "scatter_sprite", "variant_b_slide",
                       "animate_warp_", "screen_wide_spell_with_fade", "execute_special_attack_skill",
                       "execute_summon_spell_cast"]): return "spell/spellcin.c"
        return "spell/spelleff.c"
    if target == "battle/btl_ai.c":
        if _any(name, ["advance_to_nearest", "pass_turn_with_heal", "seek_optimal_position",
                       "walk_to_target_tile", "mark_aoe_plus_pattern", "mark_char_occupant",
                       "scan_chars_within_manhattan", "compute_aoe_targets", "scan_chars_along_line",
                       "collect_unmarked_tile", "tally_chars_with_zero", "find_tile_with_attribute",
                       "resolve_terrain_for_aoe", "set_tile_overlay"]): return "battle/btl_aitg.c"
        if "score" in name: return "battle/btl_aisc.c"
        return "battle/btl_ai.c"
    if target == "ui_menu/shop.c":
        if _any(name, ["chapter_intro_menu_input_loop", "run_chapter_intro_menu",
                       "party_roster_single_select", "party_roster_class_select"]): return "ui_menu/chintro.c"
        if _any(name, ["build_dead_chars", "promote_member", "run_revive", "run_class_promotion",
                       "execute_class_promotion", "build_promotion_candidates",
                       "run_recruitment_or_branch"]): return "ui_menu/promote.c"
        return "ui_menu/shop.c"
    if target == "gfx/blit.c":
        if _any(name, ["tile_blit_24x24", "blit_24x24_at_window", "blit_animated_tile",
                       "blit_24x24_tile_to_battle_grid", "blit_scaled_chapter_pose",
                       "blit_scaled_tile_map_view"]): return "gfx/blittile.c"
        return "gfx/blitspr.c"
    if target == "ui_menu/menu.c":
        if _any(name, ["options_menu_loop", "count_active_menu_items", "settings_dialog",
                       "settings_menu_input", "settings_dialog_borders", "speed_mode_overlay"]): return "ui_menu/menucfg.c"
        if _any(name, ["handle_tile_event_interaction", "field_menu_status_save_load_quit",
                       "open_tactical_overview"]): return "ui_menu/menufld.c"
        return "ui_menu/menu.c"
    if target == "battle/btl_turn.c":
        if _any(name, ["init_runtime_char", "init_battle_state", "convert_battle_tiles",
                       "restore_all_chars", "clear_all_chars", "set_chapter_init_done",
                       "set_battle_anim_phase"]): return "battle/btl_init.c"
        return "battle/btl_turn.c"
    if target == "field/chevt.c":
        m = re.search(r"event_handler_([0-9a-fA-F]{2})", name)
        if m:
            return "field/chevt2.c" if int(m.group(1), 16) >= 0x2f else "field/chevt1.c"
        # named helpers (no handler index): place with their consumer half
        if _any(name, ["cinematic_chapter_portrait_dump", "wrap_cinematic"]): return "field/chevt2.c"
        return "field/chevt1.c"
    if target == "field/chend.c":
        m = re.search(r"chapter_(\d\d)_end", name)
        if m:
            return "field/chend2.c" if int(m.group(1)) >= 20 else "field/chend1.c"
        return "field/chend1.c"
    return target


def assign_target(name, phase):
    """Routing target = base target refined by sub-file split."""
    return _subsplit(name, _base_target(name, phase))


# ── Commands ──

def cmd_generate():
    """Create routing.json from emit_functions.json + routing rules."""
    functions = json.loads(EMIT_FUNCS.read_text(encoding="utf-8"))

    # Load existing routing.json to preserve done/asm/target edits
    existing = {}
    if ROUTING_JSON.exists():
        existing = json.loads(ROUTING_JSON.read_text(encoding="utf-8"))

    routing = {}
    unrouted = []

    for fn in functions:
        addr = fn["address"].lower()
        name = fn["name"]
        phase = assign_phase(name)

        # Preserve existing state if available
        if addr in existing:
            prev = existing[addr]
            target = prev.get("target", assign_target(name, phase))
            done = prev.get("done", name in DONE_INITIAL)
            asm = prev.get("asm", name in DONE_INITIAL)
        else:
            target = assign_target(name, phase)
            done = name in DONE_INITIAL
            asm = done

        if target == "UNROUTED":
            unrouted.append(f"  {addr}  {name}")

        routing[addr] = {
            "name": name,
            "target": target,
            "phase": phase,
            "done": done,
            "asm": asm,
        }

    if unrouted:
        print(f"WARNING: {len(unrouted)} UNROUTED:")
        for u in unrouted:
            print(u)

    ROUTING_JSON.write_text(
        json.dumps(routing, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(f"Wrote {len(routing)} entries to {ROUTING_JSON}")
    _print_target_counts(routing)


def cmd_resplit():
    """Re-apply the sub-file split to existing routing.json targets (idempotent).
    Only the `target` field changes; key order and all other fields preserved."""
    raw = ROUTING_JSON.read_text(encoding="utf-8")
    routing = json.loads(raw)
    changed = []
    for addr, v in routing.items():
        new_t = _subsplit(v["name"], v["target"])
        if new_t != v["target"]:
            changed.append((addr, v["name"], v["target"], new_t))
            v["target"] = new_t
    out = json.dumps(routing, ensure_ascii=False, indent=2)
    if raw.endswith("\n"):
        out += "\n"
    ROUTING_JSON.write_text(out, encoding="utf-8")
    print(f"resplit: {len(changed)} entries re-targeted, {len(routing)} total")
    for addr, nm, old, new in changed:
        print(f"  {addr} {old:24s} -> {new:24s} {nm}")
    print()
    _print_target_counts(routing)


def cmd_status():
    """Print progress summary."""
    routing = json.loads(ROUTING_JSON.read_text(encoding="utf-8"))

    print("=== Per-phase progress ===\n")
    print(f"{'Phase':7s} {'Done':>5s} {'Left':>5s} {'Total':>5s}  Description")
    print("-" * 70)
    grand_total = grand_done = 0
    for p in range(1, 8):
        fns = [v for v in routing.values() if v["phase"] == p]
        done = sum(1 for v in fns if v["done"])
        left = len(fns) - done
        grand_total += len(fns)
        grand_done += done
        print(f"P{p:5d} {done:5d} {left:5d} {len(fns):5d}  {PHASE_NAMES[p]}")
    print("-" * 70)
    print(f"{'Total':7s} {grand_done:5d} {grand_total-grand_done:5d} {grand_total:5d}")

    print("\n=== Per-target counts ===\n")
    _print_target_counts(routing)


def cmd_mark(addr, flag):
    """Set done=true or asm=true for a function."""
    routing = json.loads(ROUTING_JSON.read_text(encoding="utf-8"))
    addr = addr.lower()
    if addr.startswith("0x"):
        addr = addr[2:]
    addr = addr.zfill(8)
    if addr not in routing:
        print(f"ERROR: address {addr} not found")
        sys.exit(1)
    entry = routing[addr]
    if flag == "done":
        entry["done"] = True
        entry["asm"] = True
    elif flag == "asm":
        entry["asm"] = True
    else:
        print(f"ERROR: unknown flag '{flag}', use 'done' or 'asm'")
        sys.exit(1)
    ROUTING_JSON.write_text(
        json.dumps(routing, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(f"Marked {entry['name']} @ {addr} → {flag}=true")


def cmd_pending(phase_filter=None):
    """List pending functions."""
    routing = json.loads(ROUTING_JSON.read_text(encoding="utf-8"))
    pending = [
        (v["phase"], k, v["name"], v["target"])
        for k, v in routing.items()
        if not v["done"] and (phase_filter is None or v["phase"] == phase_filter)
    ]
    pending.sort()
    for p, addr, name, target in pending:
        print(f"P{p} {addr} {target:25s} {name}")
    print(f"\n{len(pending)} pending")


def cmd_validate():
    """Check consistency."""
    routing = json.loads(ROUTING_JSON.read_text(encoding="utf-8"))
    issues = []
    if len(routing) != 650:
        issues.append(f"Expected 650 entries, got {len(routing)}")
    unrouted = [k for k, v in routing.items() if v["target"] == "UNROUTED"]
    if unrouted:
        issues.append(f"{len(unrouted)} UNROUTED entries")
    if issues:
        for i in issues:
            print(f"FAIL: {i}")
        sys.exit(1)
    else:
        print(f"OK: {len(routing)} entries, 0 UNROUTED")
        _print_target_counts(routing)


def _print_target_counts(routing):
    by_target = {}
    for v in routing.values():
        t = v["target"]
        by_target[t] = by_target.get(t, 0) + 1
    for t in sorted(by_target.keys()):
        print(f"  {t:30s} {by_target[t]:4d}")
    print(f"  {'TOTAL':30s} {sum(by_target.values()):4d}")


def main():
    if len(sys.argv) < 2:
        print("Usage: mkroute.py <generate|status|mark|pending|validate>")
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "generate":
        cmd_generate()
    elif cmd == "resplit":
        cmd_resplit()
    elif cmd == "status":
        cmd_status()
    elif cmd == "mark":
        if len(sys.argv) != 4:
            print("Usage: mkroute.py mark <address> done|asm")
            sys.exit(1)
        cmd_mark(sys.argv[2], sys.argv[3])
    elif cmd == "pending":
        phase = None
        if len(sys.argv) >= 4 and sys.argv[2] == "--phase":
            phase = int(sys.argv[3])
        cmd_pending(phase)
    elif cmd == "validate":
        cmd_validate()
    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)


if __name__ == "__main__":
    main()
