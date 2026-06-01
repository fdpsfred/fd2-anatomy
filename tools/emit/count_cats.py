import json, re
from pathlib import Path

fns = json.loads((Path(__file__).resolve().parents[2] / "workspace" / "emit" / "emit_functions.json").read_text())
cats = {}
for f in fns:
    n = f["name"]
    if n.startswith("crt_"):
        cat = "crt_eq"
    elif "_chapter_event_handler_" in n:
        cat = "ch_event"
    elif re.match(r"fd2_chapter_\d", n):
        cat = "ch_init_end"
    elif n.startswith("fd2_chapter_"):
        cat = "ch_other"
    elif re.match(r"fd2_(spell_handler_|cast_spell_)", n):
        cat = "spell_hdlr"
    elif re.match(r"fd2_(blit_|tile_blit_|rle_|dialog_sprite|decode_dialog)", n):
        cat = "gfx_blit"
    elif re.match(r"fd2_(render_|paint_|composite_|fill_)", n):
        cat = "gfx_render"
    elif re.match(r"fd2_(set_vga|set_full_vga|update_palette|interpolate_|palette_fade|apply_palette|flash_)", n):
        cat = "gfx_palette"
    elif re.match(r"fd2_(play_|animate_|cycle_|step_figani|scatter_|render_circle|render_filled|render_summon)", n):
        cat = "anim_play"
    elif re.match(r"fd2_(ani_decoder|tick_|slide_)", n):
        cat = "anim_tick"
    elif re.match(r"fd2_(cursor_|pan_|check_keyboard|clear_keyboard|wait_|read_bios|wrapper_clear|read_tile|pause_)", n):
        cat = "input"
    elif re.match(r"fd2_get_", n):
        cat = "table_acc"
    elif re.match(r"fd2_(ai_|enemy_turn|npc_turn|attack_action|score_|scan_|compute_aoe|execute_ai_|face_|check_char_status|check_all|check_battle|check_tile|init_battle|init_movement|flood_|pathfind_|walk_|mark_|collect_|tally_|find_tile|find_char)", n):
        cat = "battle_ai"
    elif re.match(r"fd2_(advance_rng|deduct_|apply_|calc_|recalculate_|recompute_|check_can_|execute_attack|calculate_|compute_combat|compute_equipped)", n):
        cat = "battle_core"
    elif re.match(r"fd2_(menu_|run_|open_|close_|field_|game_|player_|settings_|shop_|equip_|inventory_|item_|build_|promote_|party_|count_|recruitment|reorder_|pin_|require_)", n):
        cat = "menu_ui"
    elif re.match(r"fd2_(display_|assemble_|show_|text_dialog|portrait_|cinematic_|wrap_|setup_chars|delay_400)", n):
        cat = "dialog_cin"
    elif re.match(r"fd2_(save_|load_|backup_|restore_)", n):
        cat = "save_load"
    elif re.match(r"fd2_(dpmi_|noop_|main|set_bgm|play_sfx|debug_|set_word|set_chapter|set_combat|set_battle|set_runtime)", n):
        cat = "lifecycle"
    else:
        cat = "OTHER"
    cats.setdefault(cat, []).append(n)

for c in sorted(cats, key=lambda x: -len(cats[x])):
    print(f"{len(cats[c]):3d}  {c}")
    if c == "OTHER":
        for nm in cats[c]:
            print(f"       {nm}")
print(f"{sum(len(v) for v in cats.values()):3d}  TOTAL")
